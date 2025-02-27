/**@file
 *
 * This file contains function headers for patterns.cpp
 * along with the Strip_Buffer struct definition.
 *
**/

#ifndef PATTERNS_H
#define PATTERNS_H

#include "nanolux_types.h"
#include "storage.h"

/// @brief Holds persistent data for currently-running patterns.
///
/// This structure contains both persistent variables that can
/// be reused on a per-subpattern basis.
///
/// It also contains the LED buffer for that subpattern. The main
/// advantage of defining it here is that each subpattern can have
/// an independent pattern buffer separate from the main ones
/// in main.ino.
///
/// When a subpattern is modified in a way that requires a reset
/// (changing the subpattern name, changing LED length), the
/// existing pattern history should be replaced with the default
/// structure.
typedef struct{

  // Pattern Buffer for the particular history being used.
  CRGB leds[MAX_LEDS] = {0};

  // History Variables
  int frame = 0;                 // for spring mass 
  double amplitude = 0;          //for spring mass 2
  int tempHue = 0;
  int vol_pos = 0;
  int pix_pos = 0;
  uint8_t genre_smoothing[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  int genre_pose = 0;
  double max1[20] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  double max2[20] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  double max3[20] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  double max4[20] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  double max5[20] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  int maxIter = 0;
  double velocity = 0;
  double acceleration = 0;
  double smoothing_value[10] = {0,0,0,0,0,0,0,0,0,0};
  int location = 70;
  double velocities[5] = {0,0,0,0,0};
  double accelerations[5] = {0,0,0,0,0};
  int locations[5] = {70,60,50,40,30};
  double vRealSums[5] = {0,0,0,0,0};
} Strip_Buffer;

extern Pattern_Data params;

void nextPattern();

void clearLEDSegment(Strip_Buffer * buf, int len);

void setColorHSV(CRGB* leds, byte h, byte s, byte v, int len);

// void getFhue(uint8_t min_hue, uint8_t max_hue);

void getFhue(uint8_t min_hue, uint8_t max_hue);

void getVbrightness();

void blank(Strip_Buffer * buf, int len, Pattern_Data* params);

void confetti(Strip_Buffer * buf, int len, Pattern_Data* params);

void pix_freq(Strip_Buffer * buf, int len, Pattern_Data* params);

void eq(Strip_Buffer * buf, int len, Pattern_Data* params);

void tug_of_war(Strip_Buffer * buf, int len, Pattern_Data* params);

void saturated(Strip_Buffer * buf, int len, Pattern_Data* params);

void random_raindrop(Strip_Buffer * buf, int len, Pattern_Data* params);

void hue_trail(Strip_Buffer* buf, int len, Pattern_Data* params);

void groovy(Strip_Buffer* buf, int len, Pattern_Data* params);

void talking(Strip_Buffer *buf, int len, Pattern_Data *params);

void glitch(Strip_Buffer * buf, int len, Pattern_Data * params);

void bands(Strip_Buffer * buf, int len, Pattern_Data * params);

void Fire2012(Strip_Buffer * buf, int len, Pattern_Data* params);

void bar_fill(Strip_Buffer * buf, int len, Pattern_Data* params);

void vowels_raindrop(Strip_Buffer * buf, int len, Pattern_Data* params);

void blendIn(Strip_Buffer * buf, int len, Pattern_Data* params);

void bleedThrough(Strip_Buffer * buf, int len, Pattern_Data* params);

void showcasePercussion(Strip_Buffer * buf, int len, Pattern_Data* params);

void showcaseCentroid(Strip_Buffer * buf, int len, Pattern_Data* params);

void showcaseBread(Strip_Buffer * buf, int len, Pattern_Data* params);

void showcaseSalientFreqs(Strip_Buffer * buf, int len, Pattern_Data* params);

#endif
