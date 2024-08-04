/** @file
 *
 * This file contains defined macros and constants
 * that are used in many places in the Nanolux codebase.
 *
**/

#ifndef NANOLUX_TYPES_H
#define NANOLUX_TYPES_H

// Pattern list function pointers
typedef void (*SimplePatternList[])();

// Array size macro
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

// ADC2 (pins A0, A1) is not available when the WiFi radio is active,
// so we use ADC1 (pins A2, A3). Additionally, rev 1.2 of the board
// must be blue-wired to use pins A2, A3.
// Rev 1.2: https://github.com/OPEnSLab-OSU/Nanolux/commit/caa2e2b44d8ce75352e0c0d2d5ace363cbcfa450
//
// When using a board with a version below 1.2, or an unmodified
// v1.2 board, set this pin to A0.
#define ANALOG_PIN          A3

// Vowel recognition
enum VowelSounds {
  aVowel = 1,
  eVowel = 2,
  iVowel = 3,
  oVowel = 4,
  uVowel = 5,
  noVowel = 6
};

// Rotary Encoder Constants
#define ROTARY_ENCODER_A_PIN 23
#define ROTARY_ENCODER_B_PIN 22
#define ROTARY_ENCODER_BUTTON_PIN -1
#define ROTARY_ENCODER_VCC_PIN -1
#define ROTARY_ENCODER_STEPS 4

// FastLED
#define MAX_LEDS    200
#define DATA_PIN    15      // No hardware SPI pins defined for the ESP32 yet.
#define CLK_PIN     14      // Use bitbanged output.
#define LED_TYPE    SK9822  // Define LED protocol.
#define COLOR_ORDER BGR     // Define color color order.
#define MAX_BRIGHTNESS     255
#define FRAMES_PER_SECOND  120

// arduinoFFT
#define SAMPLES             128     // Must be a power of 2  // 128 - 1024
#define SAMPLING_FREQUENCY  10000   // Hz, must be less than 10000 due to ADC
#define NOISE_GATE_THRESH   20
#define MAX_NOISE_GATE_THRESH   100

// Mode Constants
#define STRIP_SPLITTING 0
#define Z_LAYERING      1

// Button Input
#define BUTTON_PIN 33

// MAX - MIN | Freq Volume
#define MAX_FREQUENCY       4000.0
#define MIN_FREQUENCY       50.0
#define MAX_VOLUME          3000.0
#define MIN_VOLUME          100.0

// The time the button must be pressed to reset the ESP32 is 10 seconds.
#define RESET_TIME 10000

/*******************
 * PATTERN CONSTANTS
 *******************/

/******************************************************************
  * COOLING: How much does the air cool as it rises?
  * Less cooling = taller flames.  More cooling = shorter flames.
  * Default 55, suggested range 20-100 
*******************************************************************/
#define COOLING  100

/*****************************************************************************
  * SPARKING: What chance (out of 255) is there that a new spark will be lit?
  * Higher chance = more roaring fire.  Lower chance = more flickery fire.
  * Default 120, suggested range 50-200.
******************************************************************************/
#define SPARKING 72

// Used in Pix_Hue_Freq
#define VOL_SHOW true

#endif
