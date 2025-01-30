/** @file
 *
 * This file contains functions that interface with the
 * phyiscal on-board hardware, such as encoders or buttons.
 *
 * This file also contains helper functions that don't make
 * sense to define elsewhere.
 *
**/
#include "cmath"
#include <FastLED.h>
#include <Arduino.h>
#include "arduinoFFT.h"
#include "patterns.h"
#include "nanolux_types.h"
#include "nanolux_util.h"
#include "storage.h"
#include <AiEsp32RotaryEncoder.h>


/// The current config of the device, defined in main.ino.
/// Used to check if serial printing is allowed.
extern Config_Data config;

/// Checks if the device button is pressed, defined in
/// main.ino.
extern bool button_pressed;

extern int NUM_PATTERNS;

/// Stores the inital time the button was pressed. Used
/// to determine if the button has been held long enough
/// to reset the device.
unsigned long start_millis = NULL;

/// Holds the start time of the current program loop.
long loop_start_time = 0;

/// Holds the expected end time of the current program loop.
long loop_end_time = 0;

/// Defines the rotary encoder object for the 2.0 boards
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(
  ROTARY_ENCODER_A_PIN,
  ROTARY_ENCODER_B_PIN,
  ROTARY_ENCODER_BUTTON_PIN,
  ROTARY_ENCODER_VCC_PIN,
  ROTARY_ENCODER_STEPS
);

/************************************************
 *
 * HELPERS:
 * Helper functions that could be used in many
 * places inside the program.
 *
*************************************************/

/// @brief Prints a character array message if allowed to.
/// @param msg  The message to print out to serial.
void nanolux_serial_print(char * msg){
  if(config.debug_mode == 1){
    Serial.println(msg);
  }
}

/// @brief Bounds a byte between an upper and a lower value.
/// @param val    The pointer to the value to modify.
/// @param lower  The lower value the value can be.
/// @param upper  The upper value the value can be.
void bound_byte(uint8_t * val, int lower, int upper){
  if(*val > upper){
    *val = upper;
  }else if(*val < lower){
    *val = lower;
  }
}

/// @brief Remaps a value in one range to another range.
///
/// @param x  The value to remap.
/// @param oMin The old range minimum.
/// @param oMax The old range maximum.
/// @param nMin The new range minimum.
/// @param nMax The new range maximum.
///
/// @return The value mapped to it's new range.
int remap( double x,double oMin,double oMax,double nMin,double nMax ){
  // range check
  if (oMin == oMax){
    return 0;
  }
  if (nMin == nMax){
    return 0;
  }

  // check reversed input range
  double reverseInput = false;
  double oldMin = min( oMin, oMax );
  double oldMax = max( oMin, oMax );
  if (oldMin != oMin){
    reverseInput = true;
  }

  // check reversed output range
  double reverseOutput = false;
  double newMin = min( nMin, nMax );
  double newMax = max( nMin, nMax );
  if (newMin != nMin){
    reverseOutput = true;
  }

  double portion = abs(x-oldMin)*(newMax-newMin)/(oldMax-oldMin);
  if (reverseInput){
    portion = abs(oldMax-x)*(newMax-newMin)/(oldMax-oldMin);
  }

  double result = portion + newMin;
  if (reverseOutput){
    result = newMax - portion;
  }

  return (int)result;
}

/// @brief Returns the index of the largest array element.
///
/// @param arr  The array to iterate through.
/// @param n    The number of elements to check.
///
/// @return   The index of the largest element in arr.
int largest(double arr[], int n){
  double max = arr[0];

  // Traverse array elements from second and
  // compare every element with current max 
  for (int i = 1; i < n; i++)
    if (arr[i] > max){
      max = arr[i];
    }

  return max;
}

/************************************************
 *
 * HARDWARE:
 * Code for interfacing with the button.
 *
*************************************************/

/// @brief Resets the button state to false.
///
/// If the button state is currently true, print to serial
/// if debug is enabled in the device config.
void reset_button_state(){
  button_pressed = false;
}

/// @brief A debounced interrupt to set the button state.
///
/// If the device button is pressed, wait 5 milliseconds.
/// If the button pin still reads a press, set button_pressed
/// to True.
void IRAM_ATTR buttonISR(){
  // let debounce settle 5ms, do not exceed 15ms
  delayMicroseconds(5000);
  // if still low trigger press
  if(digitalRead(BUTTON_PIN) == LOW){
    button_pressed = true;
  }
}

/************************************************
 *
 * HARDWARE:
 * Code for controlling device resets and infinte
 * loops.
 *
*************************************************/

/// @brief Causes the LED to turn on/off forever.
///
/// This function is an infinite loop that never terminates.
/// It clears the LED strip, then causes the built-in LED
/// to turn on and off with 1 second states.
void led_on_forever() {

  // Clear the LED strip before moving into the forever blink code.
  FastLED.clear();
  FastLED.show();

  // Source: Blink example
  while (true) {
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)
    delay(1000);                      // wait for a second
    digitalWrite(LED_BUILTIN, HIGH);  // turn the LED off by making the voltage LOW
    delay(1000);
  }
}

/// @brief If the hardware button is held for RESET_TIME, clear
/// the list of saves and call led_on_forever().
///
/// The inital button press sets start_millis to the current device
/// time. If the delta between the current time and start_millis is
/// equal to 1.5 seconds, turn on the LED.
///
/// If the delta is more than RESET_TIME, reset the device and call
/// led_on_forever(). This is an infinite loop, which requires a
/// device reset to clear. This is intended behavior. 
///
/// This function requires constant monitoring of the button, so
/// direct hardware calls are used here.
void process_reset_button(int button_value) {

  if (button_value) {

    if (start_millis == NULL)
      start_millis = millis();

    (millis() - start_millis > 1500)
      ? digitalWrite(LED_BUILTIN, HIGH)
      : digitalWrite(LED_BUILTIN, LOW);

    // If the current time is ever more than the system time,
    // this means an overflow has occured. This automatically
    // resets start_millis.
    if (start_millis > millis()) {
      start_millis = NULL;
      return;
    }

    if (millis() - start_millis > RESET_TIME) {
      clear_all();
      led_on_forever();
    }
  } else {
    digitalWrite(LED_BUILTIN, LOW);
    start_millis = NULL;
  }
}

/************************************************
 *
 * TIMING:
 * Code for managing the time of device program
 * loops.
 *
*************************************************/

/// @brief Logs the current program loop start time
/// and the expected loop end time.
///
/// @param ms The user-specified loop time.
void begin_loop_timer(long ms){
  loop_start_time = millis();
  loop_end_time = loop_start_time + ms;
}

/// @brief Returns the amount of time since expected loop termination.
///
/// Returns 0 if the expected end time has not been reached, or a
/// -1 if an error has occured.
///
/// If the loop end time has been reached, return the delta between
/// the expected time plus 1.
///
/// 1 is added to the output time to distinguish between positive
/// and negative 0. This probably should be refactored in the future.
long timer_overrun(){
  // Check for timer overflow.
  // This shouldn't happen as millis() shouldn't overflow
  // for 50 days.
  if(loop_start_time > millis()) return -1;

  // Return 0 if the current time is under the loop end time,
  // else return the difference between the current time and
  // the expected loop end time.
  return (millis() < loop_end_time) ? 0 : millis() - loop_end_time + 1;
}

/// @brief Processes the interrupt for the rotary encoder on 2.0 hardware
void IRAM_ATTR readEncoderISR(){
    rotaryEncoder.readEncoder_ISR();
}

/// @brief Performs initial setup for the rotary encoder
void setup_rotary_encoder(){
    rotaryEncoder.begin();
    rotaryEncoder.setup(readEncoderISR);
    rotaryEncoder.setBoundaries(0, NUM_PATTERNS - 1, true); //minValue, maxValue, circleValues true|false (when max go to min and vice versa)
    rotaryEncoder.disableAcceleration();
}

/// @brief Calculates the pattern index the rotary encoder currently
/// corresponds to.
/// @returns The pattern index the encoder is set to.
int calculate_pattern_index(){
    return static_cast<int>(floor(rotaryEncoder.readEncoder())) % NUM_PATTERNS;
}

/// @brief Returns the current state of the rotary encoder button.
/// @returns True if pressed, false if not.
bool isEncoderButtonPressed(){
    return rotaryEncoder.isEncoderButtonClicked();
}

/// @brief Sets the rotary encoder's current position to a new value
void update_encoder_value(long newValue){
    rotaryEncoder.setEncoderValue(newValue);
}