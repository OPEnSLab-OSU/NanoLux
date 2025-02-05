#ifndef NANOLUX_UTIL_H
#define NANOLUX_UTIL_H

#include <AiEsp32RotaryEncoder.h>

// Uncomment this macro if using the new hardware
// with the rotary encoder.

#define VERSION_2_HARDWARE

void IRAM_ATTR buttonISR();
void reset_button_state();
int remap( double x,double oMin,double oMax,double nMin,double nMax );
int largest(double arr[], int n);
void IRAM_ATTR button_down();
void IRAM_ATTR button_up();
void begin_loop_timer(long ms);
long timer_overrun();
void bound_byte(uint8_t * val, int lower, int upper);
void process_reset_button(int button_value);
void nanolux_serial_print(char * msg);
void IRAM_ATTR readEncoderISR();
void setup_rotary_encoder();
int calculate_pattern_index();
bool isEncoderButtonPressed();
void update_encoder_value(long newValue);

#endif
