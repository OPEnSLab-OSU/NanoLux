/**@file
 *
 * This file contains all defined pattern handlers.
 *
 * The pattern refactor is still in-progress, so this file
 * currently has little in-line documentation.
 *
**/

#include <FastLED.h>
#include <Arduino.h>
#include "patterns.h"
#include "nanolux_types.h"
#include "nanolux_util.h"
#include "storage.h"
#include "core_analysis.h"
#include "ext_analysis.h"
#include "palettes.h"

extern unsigned long microseconds;
extern double vReal[SAMPLES];      // Sampling buffers
extern double vImag[SAMPLES];
extern double vRealHist[SAMPLES];  // for delta freq
extern double delt[SAMPLES];
extern bool button_pressed;
extern SimplePatternList gPatterns;
extern int NUM_PATTERNS;
extern SimplePatternList gPatterns_layer;
extern double peak;                       //  peak frequency
extern uint8_t fHue;                      // hue value based on peak frequency
extern double volume;                     //  NOOOOTEEEE:  static?? 
extern uint8_t vbrightness;
extern double maxDelt;                    // Frequency with the biggest change in amp.
extern int advanced_size;
CRGBPalette16 gPal = GMT_hot_gp; //store all palettes in array
bool gReverseDirection = false;

extern Config_Data config; // Currently loaded config
extern Pattern_Data params;

extern uint8_t manual_pattern_idx;
extern bool manual_control_enabled;
extern double fbs[5]; 

/// Global formant array, used for accessing.
extern double formants[3];

// get frequency hue
void getFhue(uint8_t min_hue, uint8_t max_hue){
    fHue = remap(
    log(peak) / log(2),
    log(MIN_FREQUENCY) / log(2),
    log(MAX_FREQUENCY) / log(2),
    min_hue, max_hue);
    // disable min hue and max hue
    // 10, 240);
}

/// get vol brightness
void getVbrightness(){
    vbrightness = remap(
    volume,
    MIN_VOLUME,
    MAX_VOLUME,
    0,
    MAX_BRIGHTNESS);
}


void nextPattern() {
  // add one to the current pattern number, and wrap around at the end
  if(manual_control_enabled){
    manual_pattern_idx = (manual_pattern_idx + 1) % NUM_PATTERNS;
  }else{
    manual_control_enabled = true;
  }
}

void clearLEDSegment(Strip_Buffer * buf, int len){
  for(int i = 0; i < len; i++)
    buf->leds[i] = CRGB(0,0,0);
}

void blank(Strip_Buffer * buf, int len, Pattern_Data* params){
  clearLEDSegment(buf, len);
}

void setColorHSV(CRGB* leds, byte h, byte s, byte v, int len) {
  // create a new HSV color
  CHSV color = CHSV(h, s, v);
  // use FastLED to set the color of all LEDs in the strip to the same color
  fill_solid(leds, len, color);
}


/// @brief Based on a sufficient volume, a pixel will float to some position on the light strip 
///        and fall down (vol_show adds another threshold)
/// @param buf Pointer to the Strip_Buffer structure, holds LED buffer and history variables.
/// @param len The length of LEDs to process
/// @param params Pointer to Pattern_Data structure containing configuration options.
void pix_freq(Strip_Buffer * buf, int len, Pattern_Data* params) {
    //switch(params->config){
      //case 0:
      //default:
    //getFhue();
    fadeToBlackBy(buf->leds, len, 50);
    if (volume > 200) {
      buf->pix_pos = map(peak, MIN_FREQUENCY, MAX_FREQUENCY, 0, len-1);
      buf->tempHue = fHue;
    }
    else {
      buf->pix_pos--;
      buf->tempHue--;
      buf->vol_pos--;
    }
    if (VOL_SHOW) {
      if (volume > 100) {
        buf->vol_pos = map(volume, MIN_VOLUME, MAX_VOLUME, 0, len-1);
        buf->tempHue = fHue;
      } else {
        buf->vol_pos--;
      }

      buf->leds[buf->vol_pos] = buf->vol_pos < len ? CRGB(255, 255, 255):CRGB(0, 0, 0);
    }
    buf->leds[buf->pix_pos] = buf->pix_pos < len ? CHSV(buf->tempHue, 255, 255):CRGB(0, 0, 0);
}

/// @brief Confetti effect using frequency and brightness.
///        Colored speckles that blink and fade smoothly are scattered across the strip.
/// @param buf Pointer to the Strip_Buffer structure, holds LED buffer and history variables.
/// @param len The length of LEDs to process
/// @param params Pointer to Pattern_Data structure containing configuration options.
void confetti(Strip_Buffer * buf, int len, Pattern_Data* params){
  // colored speckles based on frequency that blink in and fade smoothly
  fadeToBlackBy(buf->leds, len, 20);
  int pos = random16(len);
  switch(params->config){
      case 0:
      default:
      buf->leds[pos] += CHSV( fHue + random8(10), 255, vbrightness);
      buf->leds[pos] += CHSV( fHue + random8(10), 255, vbrightness);
  }
}

/// @brief  Outputs a steady moving stream of lights where each pixel correlates to a previous fHue value.
///          Visually tracks pitch over time, with brightness determined by volume.
///           blur configuration adds blur
/// @param buf Pointer to the Strip_Buffer structure, holds LED buffer and history variables.
/// @param len The length of LEDs to process
/// @param params Pointer to Pattern_Data structure containing configuration options.
void hue_trail(Strip_Buffer* buf, int len, Pattern_Data* params) {
    switch (params->config) {
        case 0: // freq_hue_trail (also default case)
        default: // Default case set to execute the freq_hue_trail pattern
            buf->leds[0] = CHSV(fHue, 255, vbrightness);
            buf->leds[1] = CHSV(fHue, 255, vbrightness);
            for (int i = len - 1; i > 1; i -= 2) {
                buf->leds[i] = buf->leds[i - 2];
                buf->leds[i - 1] = buf->leds[i - 2];
            }
            break;

        case 1: // blur
            {
            buf->leds[0] = CHSV(fHue, 255, vbrightness);
            buf->leds[1] = CHSV(fHue, 255, vbrightness);
            for (int i = len - 1; i > 1; i -= 2) {
                buf->leds[i] = buf->leds[i - 2];
                buf->leds[i - 1] = buf->leds[i - 2];
            }
            blur1d(buf->leds, len, 20);
            break;
            }
        case 2: //sin_hue
        {
        //Create sin beat
        uint16_t sinBeat0  = beatsin16(12, 0, len-1, 0, 0);
        
        //Given the sinBeat and fHue, color the LEDS and fade
        buf->leds[sinBeat0]  = CHSV(fHue, 255, MAX_BRIGHTNESS);
        fadeToBlackBy(buf->leds, len, 5);
        break;
      }

  }
}

/// @brief  Fills the light strip with a nice ambient mess of colors that shift slowly over time. 
///         This function is similar to grovvy noise except the scale and hue_shift values are quiote different.
///         Hue Octave Config remaps the volume to the range of hues present on strip.
///         Hue Shift Config remaps volume as octaves and hhue_shift parameters in fill_noise16()
///         Noise Compression config rremaps volume as hue_x parameter
/// @param buf Pointer to the Strip_Buffer structure, holds LED buffer and history variables.
/// @param len The length of LEDs to process
/// @param params Pointer to Pattern_Data structure containing configuration options.
void saturated(Strip_Buffer* buf, int len, Pattern_Data* params){
  //Set params for fill_noise16()
  uint8_t octaves = 1;
  uint16_t x = 0;
  int scale = 300;
  uint8_t hue_octaves = 1;
  uint16_t hue_x = 100;
  int hue_scale = 20; 
  uint16_t ntime = millis() / 3;
  uint8_t hue_shift =  50;
   switch (params->config) {
        case 0: // Default, no additional values changed
            fill_noise16 (buf->leds, len, octaves, x, scale, hue_octaves, hue_x, hue_scale, ntime, hue_shift);
          break;
        case 1: { // Hue octaves 
            hue_octaves = remap(volume, MIN_VOLUME, MAX_VOLUME, 1, 10);
            fill_noise16 (buf->leds, len, octaves, x, scale, hue_octaves, hue_x, hue_scale, ntime, hue_shift);
            }
            break;
            
        case 2: {// Hue shift 
            octaves = remap(volume, MIN_VOLUME, MAX_VOLUME, 50, 100);
            hue_shift = remap(volume, MIN_VOLUME, MAX_VOLUME, 50, 100);
            scale = 230;
            hue_x = 150;
            fill_noise16 (buf->leds, len, octaves, x, scale, hue_octaves, hue_x, hue_scale, ntime, hue_shift);            }
            break;
        case 3:{ // Compression
            hue_x = remap(volume, MIN_VOLUME, MAX_VOLUME, 1, 8);
            ntime = millis() / 4;
            fill_noise16 (buf->leds, len, octaves, x, scale, hue_octaves, hue_x, hue_scale, ntime, hue_shift);
            }
            break;
  //Add blur
  blur1d(buf->leds, len, 80);
  }
}

/// @brief   A cool fluctuating pattern that changes color in waves of greens, yellows, purples and blue. 
///       This function is similar to saturated_noise but the values of scale and hue_shift are 100 and 5 respectively. 
///       This is a moving pattern but it does not change based on and volume or frequency changes. Uses fill_noise16() and blur.
///       Hue Shift Change configuration remaps volume variable as hue_shift.
/// @param buf Pointer to the Strip_Buffer structure, holds LED buffer and history variables.
/// @param len The length of LEDs to process
/// @param params Pointer to Pattern_Data structure containing configuration options.
void groovy(Strip_Buffer* buf, int len, Pattern_Data* params) {
  uint8_t octaves = 1;
  uint16_t x = 0;
  int scale = 100;
  uint8_t hue_octaves = 1;
  uint16_t hue_x = 1;
  int hue_scale = 50;
  uint16_t ntime = millis() / 3;
  uint8_t hue_shift =  5;
    
    
    switch (params->config) {
        case 0: // groovy_noise (also default case)
        default:
            {
                fill_noise16(buf->leds, len, octaves, x, scale, hue_octaves, hue_x, hue_scale, ntime, hue_shift);
            }
            break;

        case 1: // Hue Shift Change
            {
                int shiftFromVolume = remap(volume, MIN_VOLUME, MAX_VOLUME, 5, 220);
                int xFromVolume = remap(volume, MIN_VOLUME, MAX_VOLUME, 1, 2);
                x = 0;
                hue_octaves = 1;
                hue_x = xFromVolume;
                hue_scale = 20;
                hue_shift =  shiftFromVolume;

                fill_noise16(buf->leds, len, octaves, x, scale, hue_octaves, hue_x, hue_scale, ntime, hue_shift);
            }
            break;
    }
    blur1d(buf->leds, len, 80);
}

/// @brief   Generates three clusters of lights, one in the middle, and two symmetric ones that travel out from the center and return. 
///         The distance the two outer LEDS from the center is determined by the detected volume.
///         Formant configuration takes the clusters to be red,green,blue.
///         Moving configuration, the three clusters move up and down according to sine wave motion.
/// @param buf Pointer to the Strip_Buffer structure, holds LED buffer and history variables.
/// @param len The length of LEDs to process
/// @param params Pointer to Pattern_Data structure containing configuration options.
void talking(Strip_Buffer *buf, int len, Pattern_Data *params) {
  // Common variables
  int offsetFromVolume;
  int midpoint = len / 2;

  switch (params->config) {
    case 1: { // Formants
      double f0Hue = remap(formants[0], MIN_FREQUENCY, MAX_FREQUENCY, 0, 255);
      double f1Hue = remap(formants[1], MIN_FREQUENCY, MAX_FREQUENCY, 0, 255);
      double f0 = remap(formants[0], MIN_FREQUENCY, MAX_FREQUENCY, 0, 255);
      double f1 = remap(formants[1], MIN_FREQUENCY, MAX_FREQUENCY, 0, 255);
      double f2 = remap(formants[2], MIN_FREQUENCY, MAX_FREQUENCY, 0, 255);

      offsetFromVolume = remap(volume, MIN_VOLUME, MAX_VOLUME, 1, 30);
      buf->leds[len / 2] = CRGB(f0, f1, f2);
      buf->leds[len / 2 - offsetFromVolume] = CHSV(f0Hue, 255, MAX_BRIGHTNESS);
      buf->leds[len / 2 + offsetFromVolume] = CHSV(f0Hue, 255, MAX_BRIGHTNESS);
      break;
    } 

    case 2: { // Moving
      offsetFromVolume = remap(volume, MIN_VOLUME, MAX_VOLUME, 0, 12500);

      uint16_t sinBeat0 = beatsin16(5, 2, len - 3, 0, 250);
      uint16_t sinBeat1 = beatsin16(5, 2, len - 3, 0, 0 - offsetFromVolume);
      uint16_t sinBeat2 = beatsin16(5, 2, len - 3, 0, 750 + offsetFromVolume);

      buf->leds[sinBeat0] = CHSV(fHue + 100, 255, MAX_BRIGHTNESS);
      buf->leds[sinBeat1] = CHSV(fHue, 255, MAX_BRIGHTNESS);
      buf->leds[sinBeat2] = CHSV(fHue, 255, MAX_BRIGHTNESS);
      break;
    } 

    default: // Talking Hue
    case 0:
      offsetFromVolume = remap(volume, MIN_VOLUME, MAX_VOLUME, 1, len/2);
      buf->leds[midpoint] = CHSV(fHue / 2, 255, MAX_BRIGHTNESS);
      buf->leds[midpoint - offsetFromVolume] = CHSV(fHue, 255, MAX_BRIGHTNESS);
      buf->leds[midpoint + offsetFromVolume] = CHSV(fHue, 255, MAX_BRIGHTNESS);
      break;
  }

  // Common effects for all modes
  blur1d(buf->leds, len, 80);
  // Adjust fade value based on the pattern
  int fadeValue = (params->config == 0 || params->config == 2) ? 150 : (params->config == 1) ? 200 : 100;
  fadeToBlackBy(buf->leds, len, fadeValue);
}

/// @brief  Creates two light clusters that move according to sine wave motion, but their speed is affected by the volume. 
///         One pulls its color from fHue, and the other pulls its color from the formant values.
///         Talk configuration combines glitch with talking_moving().
///         Sections configuration creates 4 seperate sine wave clusters.
/// @param buf Pointer to the Strip_Buffer structure, holds LED buffer and history variables.
/// @param len The length of LEDs to process
/// @param params Pointer to Pattern_Data structure containing configuration options.
void glitch(Strip_Buffer * buf, int len, Pattern_Data * params ) {
    int offsetFromVolume, speedFromVolume;
    uint16_t sinBeat[4]; 
    double f0Hue;
    
    speedFromVolume = remap(volume, MIN_VOLUME, MAX_VOLUME, 5, params->config == 0 ? 25 : 20); 
    switch (params->config) {
        case 0:
            sinBeat[0] = beatsin16(speedFromVolume, 0, len-1, 0, 0);
            sinBeat[1] = beatsin16(speedFromVolume, 0, len-1, 0, 32767);

            f0Hue = remap(formants[0], MIN_FREQUENCY, MAX_FREQUENCY, 0, 255);

            buf->leds[sinBeat[0]]  = CHSV(fHue, 255, MAX_BRIGHTNESS);
            buf->leds[sinBeat[1]]  = CHSV(f0Hue, 255, MAX_BRIGHTNESS); //can use fHue instead of formants

            blur1d(buf->leds, len, 80);
            fadeToBlackBy(buf->leds, len, 40);

            break;
        case 1: // glitch_talk
          {
            offsetFromVolume = remap(volume, MIN_VOLUME, MAX_VOLUME, 0, 20000);

            //Create 3 sin beats with the speed and offset(first and last parameters) changing based off variables above
            uint16_t sinBeat0  = beatsin16(speedFromVolume, 3, len-4, 0, 250);
            uint16_t sinBeat1  = beatsin16(speedFromVolume, 3, len-4, 0, 0 - offsetFromVolume);
            uint16_t sinBeat2  = beatsin16(speedFromVolume, 3, len-4, 0, 750 + offsetFromVolume);

            //Given the sinBeats and fHue, color the LEDS  
            buf->leds[sinBeat0]  = CHSV(fHue*2, 255, MAX_BRIGHTNESS);
            buf->leds[sinBeat1]  = CHSV(fHue, 255, MAX_BRIGHTNESS);
            buf->leds[sinBeat2]  = CHSV(fHue, 255, MAX_BRIGHTNESS);

            blur1d(buf->leds, len, 80);
            fadeToBlackBy(buf->leds, len, 100); 

            break;
          }
        case 2: // glitch_sections
          {
            offsetFromVolume = remap(volume, MIN_VOLUME, MAX_VOLUME, 0, 10000);

            //Create 4 sin beats with the offset(last parameter) changing based off offsetFromVolume
            uint16_t sinBeat0  = beatsin16(6, 0, len-1, 0, 0     - offsetFromVolume);
            uint16_t sinBeat1  = beatsin16(6, 0, len-1, 0, 16384 - offsetFromVolume);
            uint16_t sinBeat2  = beatsin16(6, 0, len-1, 0, 32767 - offsetFromVolume);
            uint16_t sinBeat3  = beatsin16(6, 0, len-1, 0, 49151 - offsetFromVolume);

            //Given the sinBeats and fHue, color the LEDS
            buf->leds[sinBeat0]  = CHSV(fHue, 255, MAX_BRIGHTNESS);
            buf->leds[sinBeat1]  = CHSV(fHue, 255, MAX_BRIGHTNESS);
            buf->leds[sinBeat2]  = CHSV(fHue, 255, MAX_BRIGHTNESS);
            buf->leds[sinBeat3]  = CHSV(fHue, 255, MAX_BRIGHTNESS);
          
            //Add blur and fade 
            blur1d(buf->leds, len, 80);
            fadeToBlackBy(buf->leds, len, 60);
            break;
          }
    }
}

/// @brief  Basic band config : Uses the band_split_bounce() function to generate a five band split, and maps that split to the light strip. The strip is broken into five chunks of different colors, 
///         where the volume of each band determines how much of each section of the LED strip is lit.
///         Advanced bands config : he strip is broken into five chunks of different colors, where the volume of each band determines how much of each section is lit, and that portion will diminish over time if a certain volume threshold is not met
///         Fomant bands config: emonstrates the formant feature of the audio analysis code. Each of the three formant values correspond to a third of the entire LED strip, where the individual formant values determine the hue of each third.
/// @param buf Pointer to the Strip_Buffer structure, holds LED buffer and history variables.
/// @param len The length of LEDs to process
/// @param params Pointer to Pattern_Data structure containing configuration options.
void bands(Strip_Buffer* buf, int len, Pattern_Data* params) {
    //double *fiveSamples = band_sample_bounce();
    
    update_five_band_split(len); // Maybe use above if you want, but its generally agreed this one looks better
    
    double avg1 = 0;
    double avg2 = 0;
    double avg3 = 0;
    double avg4 = 0;
    double avg5 = 0;

    double vol1 = fbs[0];
    double vol2 = fbs[1];
    double vol3 = fbs[2];
    double vol4 = fbs[3];
    double vol5 = fbs[4];


      switch (params->config) {
        case 0 : 
        {
            fadeToBlackBy(buf->leds, len, 85);
            // Fill each chunk of the light strip with the volume of each band
            for (int i = 0; i < vol1; i++) {
              buf->leds[i] = CRGB(255,0,0);
            }
            for (int i = len/5; i < len/5+vol2; i++) {
              buf->leds[i] = CRGB(255,255,0);
            }
            for (int i = 2*len/5; i < 2*len/5+vol3; i++) {
              buf->leds[i] = CRGB(0,255,0);
            }
            for (int i = 3*len/5; i < 3*len/5+vol4; i++) {
              buf->leds[i] = CRGB(0,255,255);
            }
            for (int i = 4*len/5; i < 4*len/5+vol5; i++) {
              buf->leds[i] = CRGB(0,0,255);
            }
            break;
        }
          case 1:{
          // Calculate the volume of each band
          for (int i = 0; i < advanced_size; i++) {
            avg1 += buf->max1[i];    
          }
          avg1 /= advanced_size;
          for (int i = 0; i < advanced_size; i++) {
            avg2 += buf->max2[i];
          }
          avg2 /= advanced_size;
          for (int i = 0; i < advanced_size; i++) {
            avg3 += buf->max3[i];
          }
          avg3 /= advanced_size;
          for (int i = 0; i < advanced_size; i++) {
            avg4 += buf->max4[i];
          }
          avg4 /= advanced_size;
          for (int i = 0; i < advanced_size; i++) {
            avg5 += buf->max5[i];
          }
          avg5 /= advanced_size;

          double *fiveSamples = band_split_bounce(len);

          vol1 = fbs[0];
          vol2 = fbs[1];
          vol3 = fbs[2];
          vol4 = fbs[3];
          vol5 = fbs[4]; 

          // Get the Five Sample Split ^

          if(config.debug_mode == 1){
            Serial.print("ADVANCED::\tAVG1:\t");
            Serial.print(avg1);
            Serial.print("\tAVG2:\t");
            Serial.print(avg2);
            Serial.print("\tAVG3:\t");
            Serial.print(avg3);
            Serial.print("\tAVG4:\t");
            Serial.print(avg4);
            Serial.print("\tAVG5:\t");
            Serial.print(avg5);
          }

          // If there exists a new volume that is bigger than the falling pixel, reassign it to the top, otherwise make it fall for each band
          if (vol1 <= avg1) {
            buf->max1[buf->maxIter] = 0;
          }
          else {
              for (int i = 0; i < 5; i++) {
                buf->max1[i] = vol1;
              }
          }
            
          if (vol2 <= avg2) {
            buf->max2[buf->maxIter] = 0;
          }
          else {
              for (int i = 0; i < 5; i++) {
                buf->max2[i] = vol2;
              }
          }

          if (vol3 <= avg3) {
            buf->max3[buf->maxIter] = 0;
          }
          else {
              for (int i = 0; i < 5; i++) {
                buf->max3[i] = vol3;
              }
          }

          if (vol4 <= avg4) {
            buf->max4[buf->maxIter] = 0;
          }
          else {
              for (int i = 0; i < 5; i++) {
                buf->max4[i] = vol4;
              }
          }

          if (vol5 <= avg5) {
            buf->max5[buf->maxIter] = 0;
          }
          else {
              for (int i = 0; i < 5; i++) {
                buf->max5[i] = vol5;
              }
          }

          // Get this smoothed array to loop to beginning again once it is at teh end of the falling pixel smoothing
          if (buf->maxIter == advanced_size-1) {
            buf->maxIter = 0;
          } else {
            buf->maxIter++;
          }

          // Fill the respective chunks of the light strip with the color based on above^
          for (int i = 0; i < vol1-1; i++) {
            buf->leds[i] = CRGB(255,0,0);
          }
          for (int i = len/5; i < len/5+vol2-1; i++) {
            buf->leds[i] = CRGB(255,255,0);
          }
          for (int i = 2*len/5; i < 2*len/5+vol3-1; i++) {
            buf->leds[i] = CRGB(0,255,0);
          }
          for (int i = 3*len/5; i < 3*len/5+vol4-1; i++) {
            buf->leds[i] = CRGB(0,255,255);
          }
          for (int i = 4*len/5; i < 4*len/5+vol5-1; i++) {
            buf->leds[i] = CRGB(0,0,255);
          }
          
          // Assign the values for the pixel to goto
          buf->leds[(int) avg1+ (int) vol1] = CRGB(255,255,255);
          buf->leds[(int) len/5 + (int) avg2+ (int) vol2] = CRGB(255,255,255);
          buf->leds[(int) 2*len/5+(int) avg3+ (int) vol3] = CRGB(255,255,255);
          buf->leds[(int) 3*len/5+(int) avg4+ (int) vol4] = CRGB(255,255,255);
          buf->leds[(int) 4*len/5+(int) avg5+ (int) vol5] = CRGB(255,255,255);
          fadeToBlackBy(buf->leds, len, 90);

          delete [] fiveSamples;
          break;
        }
        case 2 :
        {
            // Grab the formants
            double *temp_formants = density_formant();
            double f0Hue = remap(temp_formants[0], MIN_FREQUENCY, MAX_FREQUENCY, 0, 255);
            double f1Hue = remap(temp_formants[1], MIN_FREQUENCY, MAX_FREQUENCY, 0, 255);
            double f2Hue = remap(temp_formants[2], MIN_FREQUENCY, MAX_FREQUENCY, 0, 255);

            if(config.debug_mode == 1){
              Serial.print("\t f0Hue: ");
              Serial.print(temp_formants[0]);
              Serial.print("\t f1Hue: ");
              Serial.print(temp_formants[1]);
              Serial.print("\t f2Hue: ");
              Serial.print(temp_formants[2]);
            }

            // Fill 1/3 with each formant
            for (int i = 0; i < len; i++) {
              if (i < len/3) {
                buf->leds[i] = CHSV(f0Hue, 255, 255);
              }
              else if (len/3 <= i && i < 2*len/3) {
                buf->leds[i] = CHSV(f1Hue, 255, 255);
              } 
              else { 
                buf->leds[i] = CHSV(f2Hue, 255, 255);
              }
            }

            // Smooth out the result
            for (int i = 0; i < 5; i++) {
              blur1d(buf->leds, len, 50);
            }
            delete[] temp_formants;
            break;
          }
      }
}

/// @brief Short and sweet function. Each pixel corresponds to a value from vReal, 
///         where the volume at each pitch determines the brightness of each pixel. Hue is locked in to a rainbow.
/// @param buf Pointer to the Strip_Buffer structure, holds LED buffer and history variables.
/// @param len The length of LEDs to process
/// @param params Pointer to Pattern_Data structure containing configuration options.
void eq(Strip_Buffer * buf, int len, Pattern_Data* params) {
  
  for (int i = 0; i < len; i++) {
    int brit = map(vReal[i], MIN_FREQUENCY, MAX_FREQUENCY, 0, 255); // The brightness is based on HOW MUCH of the frequency exists
    int hue = map(i, 0, len, 0, 255); // The fue is based on position on the light strip, ergo, what frequency it is at
    if (vReal[i] > 200) { // An extra gate because the frequency array is really messy without it
      buf->leds[i] = CHSV(hue, 255, brit);
    }
  }
}

/// @brief A random spot is chosen along the length and does a ripple based on frequency
/// @param buf Pointer to the Strip_Buffer structure, holds LED buffer and history variables.
/// @param len The length of LEDs to process
/// @param params Pointer to Pattern_Data structure containing configuration options.
void random_raindrop(Strip_Buffer * buf, int len, Pattern_Data* params){
    int startIdx = random(len);

    buf->leds[startIdx] = CHSV(fHue, 255, vbrightness);
    
    for(int i = len-1; i > 0; i--) {
      if (i != startIdx) {
        buf->leds[i] = buf->leds[i-1];
      }
    }

    buf->leds[0] = CRGB::Black;
}

/// @brief Strip is split into two sides, red and blue showing push and pull motion 
///         based on either frequency or volume
/// @param buf Pointer to the Strip_Buffer structure, holds LED buffer and history variables.
/// @param len The length of LEDs to process
/// @param params Pointer to Pattern_Data structure containing configuration options.
void tug_of_war(Strip_Buffer * buf, int len, Pattern_Data* params) {
    int splitPosition;
    //use this function with smoothing for better results
    // red is on the left, blue is on the right
    switch(params->config) {
      case 0: // frequency
        {
          
        double *formants = density_formant();
        double f0 = formants[0];
        delete[] formants;
        splitPosition = remap(f0, MIN_FREQUENCY, MAX_FREQUENCY, 0, len);

        // red is on the left, blue is on the right
        for (int i = 0; i < len; i++) {
            if (i < splitPosition) {
                buf->leds[i] = CHSV(params->minhue, 255, 255);
            } else {
                buf->leds[i] = CHSV(params->maxhue, 255, 255);
            }
        }
    
        }
      case 1: // volume
        {
        splitPosition = remap(volume, MIN_VOLUME, MAX_VOLUME, 0, len);
        for (int i = 0; i < len; i++) {
            if (i < splitPosition) {
                buf->leds[i] = CHSV(params->minhue, 255, 255);
            } else {
                buf->leds[i] = CHSV(params->maxhue, 255, 255);
            }
        }
        }
    }
}



/// @brief Fire2012 pattern utilizing heating and cooling
/// @param buf Pointer to the Strip_Buffer structure, holds LED buffer and history variables.
/// @param len The length of LEDs to process
/// @param params Pointer to Pattern_Data structure containing configuration options.
void Fire2012(Strip_Buffer * buf, int len, Pattern_Data* params){
  
  int sparkVolume = remap(volume, MIN_VOLUME, MAX_VOLUME, 10,200);
  //int coolingVolume = remap(volume, MIN_VOLUME, MAX_VOLUME, 60, 40);
  //Serial.println(sparkVolume);
  
  
  // Array of temperature readings at each simulation cell
  static byte heat[MAX_LEDS];

// Step 1.  Cool down every cell a little
  for( int i = 0; i < config.length; i++) {
    heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / config.length) + 2));
  }

  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for( int k= config.length - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
  }
  
  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if( random8() < sparkVolume ) {
    int y = random8(7);
    heat[y] = qadd8( heat[y], random8(160,255) );
  }

  //Step 3.5. Calcualate Brightness from low frequencies
  int l = (sizeof(vReal)/sizeof(vReal[0])) / 7;
  double smol_arr[l];
  memcpy(smol_arr, vReal, l-1);
    
  // Step 4.  Map from heat cells to LED colors
  for( int j = 0; j < config.length; j++) {
    // Scale the heat value from 0-255 down to 0-240
    // for best results with color palettes.
  
    byte colorindex = scale8( heat[j], 240);
    //byte colorindex = scale8( heat[j], smol_arr);
    CRGB color = ColorFromPalette( gPal, colorindex);
    int pixelnumber;
    if( gReverseDirection ) {
      pixelnumber = (config.length-1) - j;
    } else {
      pixelnumber = j;
    }
    buf->leds[pixelnumber] = color;
  }
}

void vowels_raindrop(Strip_Buffer * buf, int len, Pattern_Data* params){
    int startIdx = random(len);
    VowelSounds result = vowel_detection();
    switch (result) {
      case aVowel:
        buf->leds[startIdx] = CRGB::Blue;
        break;
      case eVowel:
        buf->leds[startIdx] = CRGB::Green;
        break;
      case iVowel:
        buf->leds[startIdx] = CRGB::Red;
        break;
      case oVowel:
        buf->leds[startIdx] = CRGB::Orange;
        break;
      case uVowel:
        buf->leds[startIdx] = CRGB::Yellow;
        break;
      default: // no vowel is detected
        buf->leds[startIdx] = CRGB::Black;
        break;
    }

    buf->leds[startIdx] = CHSV(fHue, 255, vbrightness);
    
    for(int i = len-1; i > 0; i--) {
      if (i != startIdx) {
        buf->leds[i] = buf->leds[i-1];
      }
    }

    buf->leds[0] = CRGB::Black;
  
}

#define VOLUME 0
#define FREQUENCY 1
/// @brief Displays a pattern that occupies "lower" pixels at lower values,
/// and "higher" pixels at higher values.
/// @param buf Pointer to the Strip_Buffer structure, holds LED buffer and history variables.
/// @param len The length of LEDs to process
/// @param params Pointer to Pattern_Data structure containing configuration options.
void bar_fill(Strip_Buffer * buf, int len, Pattern_Data* params){

  uint8_t max_height = 0;

  switch(params->config) {

    case VOLUME: default: {
      max_height = remap(volume, MIN_VOLUME * 4, MAX_VOLUME/2, 0, len-1);
      break;
    }

    case FREQUENCY: {
      max_height = map(peak, MIN_FREQUENCY * 4, MAX_FREQUENCY/2, 0, len-1);
      break;
    }
  }

  uint8_t hue_step = (params->maxhue - params->minhue) / (len - 1);

  // Apply the color to the strip.
  for(uint8_t i = 0; i < max_height; i++){
    buf->leds[i] = CHSV(
      (params->minhue + hue_step * (i - 1)) % 255,
      255,
      255
    );
  }

  // Black out the rest of the strip.
  for(uint8_t i = max_height; i < len; i++){
    buf->leds[i] = CHSV(0, 0, 0);
  }
  
}

void blendIn(Strip_Buffer * buf, int len, Pattern_Data* params){
    CHSV backColor;
    CHSV corrColor;
    switch(params->config){
            case 0:
            default:
                backColor = CHSV(0, 0, 0);
                corrColor = CHSV(0, 0, 255);
    }
    for(int i = 0; i < len; i++){
        int blending = remap(vReal[i], MIN_FREQUENCY, MAX_FREQUENCY, 0, 256);
        buf->leds[i] = blend(backColor, corrColor, blending);
    }
}

void bleedThrough(Strip_Buffer * buf, int len, Pattern_Data* params){
    CHSV backColor;
    CHSV corrColor;
    fadeToBlackBy(buf->leds, len, 20);
    switch(params->config){
        case 0:
        default:
            backColor = CHSV(0, 0, 0);
            corrColor = CHSV(0, 0, 255);
    }
    int blending = remap(peak, MIN_FREQUENCY, MAX_FREQUENCY, 0, 256);
    buf->vol_pos += blending;
    if (buf->vol_pos > 256){
        buf->vol_pos = 256;
    }
    fill_solid(buf->leds, len, blend(backColor, corrColor, buf->vol_pos))
    if (buf->vol_pos == 256){
        CHSV tempColor = backColor;
        backColor = corrColor;
        corrColor = tempColor;
        buf->vol_pos = 0;
    }
}
/*
void bleedThroughArray(Strip_Buffer * buf, int len, Pattern_Data* params){
    CHSV backColor;
    CHSV corrColor;
    fadeToBlackBy(buf->leds, len, 50);
    switch(params->config){
        case 0:
        default:
            backColor = CHSV(0, 0, 0);
            corrColor = CHSV(255, 255, 255);
    }
    for(int i = 0; i < len; i++){
        int blending = map(vReal[i], MIN_FREQUENCY, MAX_FREQUENCY, 0, 256);
        buf->tempLeds[i] += blending;
        buf->leds[i] = blend(backColor, corrColor, buf->tempLeds[i]);
        if (buf->tempLeds[i] >= 256){
            CHSV tempColor = backColor;
            backColor = corrColor;
            corrColor = tempColor;
            buf->tempLeds[i] = 0;
        }
    }
}

void splash(Strip_Buffer * buf, int len, Pattern_Data* params){
    buf->leds[0] = CHSV(0, 0, 0);
    buf->leds[len] = CHSV(0, 0, 0);
    CHSV temp = buf->leds[0];
    for(int i = 1; i < len - 1; i++){
        CHSV holding = temp;
        temp = buf->leds[i];
        buf->leds[i] = blend(holding, buf->leds[i + 1], .5);
    }
}

void propagateSplatter(Strip_Buffer * buf, int len, Pattern_Data* params){
  static float chancePropagate[len];
  for(int i = 0; i < len; i++){
    //might change to use salient freqs
    if(vReal[i] == peak){
      chancePropagate[i] = 5;
    }
  }
  for(int i = 1; i < len - 1; i++){
      if(chancePropagate[i] >= random(5)){
        buf->leds[i] = CHSV(fhue, 255, vbrightness);
        if (i - 1 >= 0){
          chancePropagate[i - 1] = chancePropagate[i] - 1;
        }
        if (i + 1 <= len){
          chancePropagate[i + 1] = chancePropagate[i] - 1;
        }
      }
  }
}
*/
void showcasePercussion(Strip_Buffer * buf, int len, Pattern_Data* params){
  static bool perc = false;
  //percussionPresent is a hypothetical external bool that detects if percussion is present
  if (percussionPresent != perc){
    if (percussionPresent){
      buf->leds[len] = CHSV(fhue, 255, vbrightness);
    }
    perc = percussionPresent;
  }
  for (int i = 0; i < len - 1; i++){
    buf->leds[i] = buf->leds[i + 1];
  }
}

void showcaseCentroid(Strip_Buffer * buf, int len, Pattern_Data* params){
  //centroid is a hypothetical external float that showcases the frequency of the center of mass, characterizing brightness
  for(int i = 0; i < len; i++){
    int brit = map(vReal[i], MIN_FREQUENCY, MAX_FREQUENCY, 0, 255); // The brightness is based on HOW MUCH of the frequency exists
    if(vReal[i] >= centroid){
      buf->leds[i] = blend(buf->leds[i], CHSV(194, 79, brit));
    }
    else{
      buf->leds[i] = blend(buf->leds[i], CHSV(83, 38, brit));
    }
  }
}

/*
void showcaseNoisiness(Strip_Buffer * buf, int len, Pattern_Data* params){
  //noisiness is a hypothetical external float from 1 to 0, representing the randomness of energy in the frequency.
  //maybe use noisiness to set a blurring effect? to represent the erratic nature. Or maybe the speed for like a sin beat scenario
}
*/

void showcaseBread(Strip_Buffer * buf, int len, Pattern_Data* params){
  //breadSlicer is a hypothetical external array of floats, where each value corresponds to the sum of amps in that sector
  int slices;
  switch(param->config){
    case 0:
    slices = 1;
    break;
    case 1:
    slices = 3;
    break;
    case 2:
    slices = 5;
    break;
  }
  int bands[slices];
  for (int i = 0; i < slices; i++){
    bands[i] = MAX_FREQUENCY / slices * i;
  }
  //setBreadSlicer is a hypothetical helper function that lets us configure the breadSlicer, basically calling its setBands function from the patterns
  setBreadSlicer(bands, slices);
  float* amps = slicerOutput();
  for(int i = 0; i < slices - 1; i++){
    amps[i] = amps[i] / (MAX_FREQUENCY / slices); //set to average of that area
    int brit = remap(amps[i], MIN_FREQUENCY, MAX_FREQUENCY, 0, 255); //set brightness to average amp
    int startingPos = len / (slices - 1) * i; //set position that this slice occupies
    int sliceHue = 255 / (slices - 1); //set hue based on each slice
    for(int j = 0; j < len / (slices - 1); j++){
      buf->leds[startingPos + j] = CHSV(sliceHue, 70, brit);
    }
  }
}

void showcaseSalientFreqs(Strip_Buffer * buf, int len, Pattern_Data* params){
  //salientFreqs is a hypothetical external array of ints that represent indexes with the greatest change in amplitude. By default it gives 3 points
  static int splatter[len];
  fadeToBlackBy(buf->leds, len, 50);
  for(int i = 0; i < len; i++){
    splatter[i] = -1;
  }
  for(int i = 0; i < 3; i++){
    int startPos = salFreqs[i];
    for(int i = 0; i < 4; i++){
      if (startPos + i <= len){
        splatter[startPos + i] = i;
      }
      if(startPos - i >= 0){
        splatter[startPos - i] = i;
      }
    }
  }
  for(int i = 0; i < len; i++){
    if (splatter[i] == 0){
      buf->leds[i] = CHSV(fhue, 255, vbrightness);
      splatter[i] = -1;
    }
    elif(splatter[i] > 0){
      splatter[i] -= 1;
    }
  }
}