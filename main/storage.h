/**@file
 *
 * This file contains function headers for storage.cpp
 * along with data structures for user data stored in
 * NVS.
 *
**/

#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>

/******************************************************************
*
* STRIP AND PATTERN CONFIGURATIONS
*
* In the NanoLux codebase, the term "pattern" refers to the
* individual light pattern running on part of the LED strip.
* Therefore, in the case of strip splitting, up to PATTERN_LIMIT
* could run side-by-side. Each pattern has other data besides
* the pattern index, which includes a brightness value and a
* smoothing value.
*
* The term "strip config" refers to everything currently running on 
* the strip. Each strip config contains patterns within
* them, and have extra data that pertains to the entire LED strip
* or audio analysis module that cannot run multiple times. Of note
* is the number of patterns that pattern is currently running.
*
******************************************************************/

/// The number of saved strip configs the ESP32 is able to address.
#define NUM_SAVES  3

/// The number of patterns that can run at maximum.
#define PATTERN_LIMIT 4

typedef struct{

  uint8_t idx = 0; /// The selected pattern name to run.
  uint8_t brightness = 255; /// The pattern's brightness.
  uint8_t smoothing = 0; /// How smoothed pattern light changes are.
  uint8_t minhue = 0;
  uint8_t maxhue = 255;
  uint8_t config = 0; // diffrent configs
  uint8_t postprocessing_mode = 0; // The current mode for postprocessing
  
} Pattern_Data;
  

/// A structure holding strip configuration data.
typedef struct{

  uint8_t alpha = 0; /// How transparent the top pattern is in Z-layering.
  uint8_t noise_thresh = 0; /// The minimum noise floor to consider as audio.
  uint8_t mode = 0; /// The currently-running pattern mode (splitting vs layering).
  uint8_t pattern_count = 1; /// The number of patterns this config has.
  Pattern_Data pattern[PATTERN_LIMIT]; /// Data for all patterns loaded.

} Strip_Data;

/// A structure holding system configuration data.
typedef struct{

  uint8_t length = 60; /// The length of the LED strip.
  uint8_t loop_ms = 40; /// The number of milliseconds one program loop takes.
  uint8_t debug_mode = 0; /// The currently selected debug output mode.
  bool init = true; /// If the loaded config data is valid.
  char pass[16] = ""; // The current device password

} Config_Data;


void load_slot(int slot);
void set_slot(int slot);
void save_to_nvs();
void clear_all();
void load_from_nvs();
void save_config_to_nvs();
void verify_saves();


#endif