#include <atomic>
#pragma once

#include <ESPmDNS.h>
#include "BluetoothSerial.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <WebHandlerImpl.h>
#include <WebResponseImpl.h>
#include "esp_wifi.h"

typedef StaticJsonDocument<384> NanoluxJson;


//#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#define DEBUG_PRINTF(...)

#define ALWAYS_PRINTF(...) Serial.printf(__VA_ARGS__)

// Uncomment to use the old LittleFS web app loader.
#define SD_LOADER

#ifdef SD_LOADER
  #include "FS.h"
  #include "SD.h"
  #include "SPI.h"
  #define SCK 5
  #define MISO 19
  #define MOSI 18
  #define CS 21
#else
  #include <LittleFS.h>
#endif

/**
 * WEB SERVER CONSTANTS
**/
#define HTTP_OK                 200
#define HTTP_ACCEPTED           202
#define HTTP_BAD_REQUEST        400
#define HTTP_UNAUTHORIZED       401
#define HTTP_METHOD_NOT_ALLOWED 405
#define HTTP_UNPROCESSABLE      422
#define HTTP_INTERNAL_ERROR     500
#define HTTP_UNAVAILABLE        503

#define CONTENT_JSON "application/json"
#define CONTENT_TEXT "text/plain"
#define URL_FILE "/assets/url.json"
#define SETTINGS_FILE "/settings.json"
const char* EMPTY_SETTING = "#_None_#";

#define MAX_WIFI_CONNECT_WAIT 100
#define MAX_NETWORKS          15
#define END_OF_DATA           9999

/*
 * WIFI management data.
 */

typedef struct {
  String SSID;
  int32_t RSSI;
  uint8_t EncryptionType;
} WiFiNetwork;
WiFiNetwork available_networks[MAX_NETWORKS];

typedef struct {
  String SSID;
  String Key;
} CurrentWifi;
static CurrentWifi current_wifi;
static CurrentWifi candidate_wifi;
extern Config_Data config;  // Currently loaded config

/*
 * Artifacts used to manage the async WiFi join process.
 */
struct WlStatusToString {
  wl_status_t status;
  const char* description;
};

const WlStatusToString wl_status_to_string[] = {
  { WL_NO_SHIELD, "WiFi shield not present" },
  { WL_IDLE_STATUS, "WiFi is in idle state" },
  { WL_NO_SSID_AVAIL, "Configured SSID cannot be found" },
  { WL_SCAN_COMPLETED, "Scan completed" },
  { WL_CONNECTED, "Connected to network" },
  { WL_CONNECT_FAILED, "Connection failed" },
  { WL_CONNECTION_LOST, "Connection lost" },
  { WL_DISCONNECTED, "Disconnected from network" }
};

const char* status_description;

TimerHandle_t join_timer;
SemaphoreHandle_t join_status_mutex;

bool join_in_progress = false;
bool join_succeeded = false;


/*
 * Networking params
 */
const char* ap_ssid = "AUDIOLUX";
const char* ap_password = "12345678";
const char* DEFAULT_HOSTNAME = "audiolux";
static String hostname;


/*
 * Settings
 */
static StaticJsonDocument<384> settings;
volatile bool dirty_settings = false;
static String http_response;
volatile bool server_unavailable = false;

/*
 * Web Server API handlers
 */
typedef struct {
  String path;
  ArRequestHandlerFunction handler;
} APIGetHook;

typedef struct {
  String path;
  ArJsonRequestHandlerFunction request_handler;
} APIPutHook;


AsyncWebServer webServer(80);

/// @brief Opens a file on the currently-running filesystem.
///
/// Wrapper for an #ifdef macro for filesystem selection (SD vs LittleFS).
inline File open_file(const char * path, const char * mode){
  #ifdef SD_LOADER
    return SD.open(path, mode);
  #else
    return LittleFS.open(path, mode);
  #endif
}

/// @brief Saves a JSON to a given file path.
///
/// @param path The file location to save to.
/// @param json The JSON object to save.
inline bool save_json_to_file(const char * path, NanoluxJson json){
  File f = open_file(path, "w");
  if (f){
    serializeJson(f, json);
    return true;
  } 
  return false;
}

/// @brief Initalizes the file system used on the ESP32.
///
/// This function will start the filesystem on the SD card if the
/// flag SD_LOADER is defined.
inline void initialize_file_system() {
  #ifdef SD_LOADER
    DEBUG_PRINTF("Initializing SD FS...");
    SPI.begin(SCK, MISO, MOSI, CS);
    if (!SD.begin(CS))
      ALWAYS_PRINTF("Card Mount Failed");
  #else
    DEBUG_PRINTF("Initializing FS...");
    if (LittleFS.begin()) {
      DEBUG_PRINTF("done.\n");
    } else {
      DEBUG_PRINTF("fail.\n");
    }
  #endif 
}

/// @brief Saves the settings JSON file from memory onto the filesytem.
///
/// Saves the current hostname, SSID, and WiFi key to the storage JSON, then
/// writes to the storage file.
inline void save_settings() {

  if (!settings.containsKey("wifi")) settings.createNestedObject("wifi");

  settings["hostname"] = hostname;
  settings["wifi"]["ssid"] = current_wifi.SSID;
  settings["wifi"]["key"] = current_wifi.Key;

  if(save_json_to_file(SETTINGS_FILE, settings)){
    DEBUG_PRINTF("WiFi settings saved:\n");
    serializeJsonPretty(settings, Serial);
  }else{
    DEBUG_PRINTF("Unable to save settings file.");
  }

  DEBUG_PRINTF("\n");
}

inline bool load_json_from_file(const char * path, NanoluxJson target){

  File f = open_file(path, "r");

  if(f) return deserializeJson(target, f) ? false : true;

  return false;

}

/// @brief Loads the settings JSON from the filesystem into memory.
///
/// If unable to load settings, this function will load a "default"
/// settings file and save it to storage.
inline void load_settings() {

  DEBUG_PRINTF("Loading saved WiFi settings.\n");

  if(load_json_from_file(SETTINGS_FILE, settings)){

    DEBUG_PRINTF("Settings loaded:\n");
    serializeJsonPretty(settings, Serial);
    DEBUG_PRINTF("\n");

    hostname = settings["hostname"].as<String>();
    current_wifi.SSID = settings["wifi"]["ssid"].as<String>();
    current_wifi.Key = settings["wifi"]["key"].as<String>();

    return;
  }

  DEBUG_PRINTF("Unable to load settings. Saving empty file.\n");
  current_wifi.SSID = EMPTY_SETTING;
  current_wifi.Key = EMPTY_SETTING;
  hostname = DEFAULT_HOSTNAME;
  save_settings();
}



/// @brief Saves a given URL to the "url" file, which is where
/// the user connects to the device at in a browser.
///
/// @param url The URL to be saved.
inline void save_url(const String& url) {

  NanoluxJson data;
  data["url"] = url;

  if(save_json_to_file(URL_FILE, data)){
    DEBUG_PRINTF("%s saved as Web App URL.\n", url.c_str());
  }else{
    DEBUG_PRINTF("Unable to save Web App URL, will default to http://192.168.4.1.\n");
  }

}

inline const String& build_response(const bool success, const char* message, const char* details) {
  http_response = String("{\"success\": ") + (success ? "true" : "false");
  if (message != nullptr) {
    http_response += String(", \"message\": \"") + message + String("\"");
  }
  if (details != nullptr) {
    http_response += String(", \"details\": \"") + details + String("\"");
  }
  http_response += "}";

  return http_response;
}


/*
 * Network configuration
 */
inline void scan_ssids() {
  static int long_scan_count = 0;
  const int MAX_SCAN_ITERATIONS = 2;

  // Flow: check if there was a scan happening, and get its results.
  // If there was no scan start one and be done. If the previous scan
  // failed, start a new one and move on. If the previous scan succeeded
  // then stash the results and start a new one. The main consequence here
  // is that the very first time the scan is run, the result will be
  // and empty array. It is up to the client to handle that.

  // Start with the assumption we have an empty scan.
  available_networks[0].RSSI = END_OF_DATA;
  const int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true);
    long_scan_count = 0;
  } else if (n == WIFI_SCAN_RUNNING) {
    long_scan_count++;

    if (long_scan_count >= MAX_SCAN_ITERATIONS) {
      // This scan has run for a while. Cancel it and start a new one.
      WiFi.scanDelete();
      WiFi.scanNetworks(true);
      long_scan_count = 0;
    }
  } else if (n == 0) {
    long_scan_count = 0;
  } else if (n > 0) {
    const int network_count = min(n, MAX_NETWORKS);
    for (int i = 0; i < network_count; ++i) {
      available_networks[i].SSID = String(WiFi.SSID(i));
      available_networks[i].RSSI = WiFi.RSSI(i);
      available_networks[i].EncryptionType = WiFi.encryptionType(i);
    }

    if (network_count < MAX_NETWORKS) {
      available_networks[network_count].RSSI = END_OF_DATA;
    }

    WiFi.scanDelete();
    if (WiFi.scanComplete() == WIFI_SCAN_FAILED) {
      WiFi.scanNetworks(true);
    }
    long_scan_count = 0;
  }
}


inline bool initialize_wifi_connection() {
  server_unavailable = true;

  // Stop any pending WiFi scans.
  WiFi.scanDelete();

  // Drop the current connection, if any.
  WiFi.disconnect();
  delay(100);

  WiFi.begin(current_wifi.SSID.c_str(), current_wifi.Key.c_str());
  int wait_count = 0;
  while (WiFi.status() != WL_CONNECTED && wait_count < MAX_WIFI_CONNECT_WAIT) {
    delay(500);
    ++wait_count;
  }
  server_unavailable = false;

  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  WiFi.disconnect();
  delay(100);
  return false;
}


inline void initialize_mdns(bool use_user_hostname) {
  bool success = MDNS.begin(
    (use_user_hostname ? hostname : DEFAULT_HOSTNAME).c_str());

  // The assumption is that we are connected. Setup mDNS
  if (success) {
    ALWAYS_PRINTF("mDNS connected. The AudioLux can be reached at %s.local\n", hostname.c_str());
  } else {
    ALWAYS_PRINTF("Unable to setup mDNS\n");
  }
}


inline const char* get_status_description(const wl_status_t status) {
  for (int i = 0; i < sizeof(wl_status_to_string) / sizeof(WlStatusToString); i++) {
    if (wl_status_to_string[i].status == status) {
      status_description = wl_status_to_string[i].description;
      break;
    }
  }

  return status_description;
}


inline void on_join_timer(TimerHandle_t timer) {
  DEBUG_PRINTF("Timer: Checking WiFi Join Status.\n");
  if (xSemaphoreTake(join_status_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (join_in_progress) {
      wl_status_t status = WiFi.status();
      if (status == WL_CONNECTED) {
        join_in_progress = false;
        join_succeeded = true;
        xTimerStop(timer, 0);
        ALWAYS_PRINTF("Timer: WiFi join succeeded.\n");

        // Queue the settings for saving. Can't do it here
        // because FreeRTOS croaks with a stack overflow.
        // Possibly writing to flash is resource-heavy.
        current_wifi.SSID = candidate_wifi.SSID;
        current_wifi.Key = candidate_wifi.Key;
        dirty_settings = true;

        initialize_mdns(true);
      } else if (status != WL_IDLE_STATUS && status != WL_CONNECT_FAILED && status != WL_NO_SHIELD) {
        join_in_progress = false;
        join_succeeded = false;
        xTimerStop(timer, 0);
        DEBUG_PRINTF("Timer: WiFi join failed. Reason: %s.\n", get_status_description(status));
      }
    }

    xSemaphoreGive(join_status_mutex);
  }
}

/*
 * Wifi management
 */
inline bool join_wifi(const char* ssid, const char* key) {
  DEBUG_PRINTF("Trying to join network %s ...\n", ssid);

  // Reset any radio activity.
  WiFi.scanDelete();
  WiFi.disconnect();
  delay(100);
  WiFi.setHostname(hostname.c_str());

  candidate_wifi.SSID = ssid;
  candidate_wifi.Key = key;

  // Start the connection process.
  WiFi.begin(ssid, key);
  if (xSemaphoreTake(join_status_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    join_in_progress = true;
    join_succeeded = false;

    if (xTimerStart(join_timer, 0) != pdPASS) {
      DEBUG_PRINTF("Unable to star timer. Join unsupervised.\n");
      return false;
    }

    xSemaphoreGive(join_status_mutex);
    return true;
  }

  DEBUG_PRINTF("Unable to get mutex. Join unsupervised.\n");
  return false;
}

/// Include for the Web API
#include "web_api.h"

inline void setup_networking(const char* password) {
  initialize_file_system();

  // Load saved settings. If we have an SSID, try to join the network.
  load_settings();

  // Prevent he radio from going to sleep.
  WiFi.setSleep(false);

  // Local WiFi connection depends on whether it has been configured
  // by the user.
  bool wifi_okay = false;
  if (current_wifi.SSID != nullptr && current_wifi.SSID != EMPTY_SETTING) {
    DEBUG_PRINTF("Attempting to connect to saved WiFi: %s\n", current_wifi.SSID.c_str());
    wifi_okay = initialize_wifi_connection();
    if (wifi_okay) {
      ALWAYS_PRINTF("WiFi IP: %s\n", WiFi.localIP().toString().c_str());
    }
  } else {
    ALWAYS_PRINTF("****\n");
    ALWAYS_PRINTF("No wifi saved. AudioLux available via Access Point:\n");
    ALWAYS_PRINTF("SSID: %s Password: %s\n", ap_ssid, password);
    ALWAYS_PRINTF("****\n");
  }

  // AP mode is always active.
  WiFi.mode(WIFI_MODE_APSTA);
  if (password[0] == '\0') {
    WiFi.softAP("AudioluxUnsecured");
    ALWAYS_PRINTF("WIFI IS UNSECURED!!!\n");
    initialize_mdns(false);
  } else {
    WiFi.softAP(hostname, password);
    initialize_mdns(true);
  }

  delay(1000);
  const IPAddress ap_ip = WiFi.softAPIP();

  // Set up the URL that the Web App needs to talk to.
  // We prefer user's network if available.
  String api_url = "http://";
  if (wifi_okay) {
    api_url += hostname;
    api_url += ".local";
  } else {
    api_url += ap_ip.toString();
  }
  save_url(api_url);
  ALWAYS_PRINTF("Backend available at: %s", api_url.c_str());
}

inline void register_api(const APIGetHook api_get_hooks[], const int get_hook_count, APIPutHook api_put_hooks[], const int put_hook_count){
  // Register the main process API handlers.
  DEBUG_PRINTF("Registering main APIs.\n");
  for (int i = 0; i < get_hook_count; i++) {
    DEBUG_PRINTF("%s\n", api_get_hooks[i].path.c_str());
    webServer.on(api_get_hooks[i].path.c_str(), HTTP_GET, api_get_hooks[i].handler);
  }
  for (int i = 0; i < put_hook_count; i++) {
    DEBUG_PRINTF("%s\n", api_put_hooks[i].path.c_str());
    webServer.addHandler(new AsyncCallbackJsonWebHandler(api_put_hooks[i].path.c_str(), api_put_hooks[i].request_handler));
  }

  // Now add internal APi endpoints (wifi and health)
  webServer.on("/api/wifis", HTTP_GET, serve_wifi_list);
  webServer.on("/api/wifi", HTTP_GET, handle_wifi_get_request);
  webServer.on("/api/wifi_status", HTTP_GET, handle_wifi_status_request);
  webServer.on("/api/hostname", HTTP_GET, handle_hostname_get_request);
  webServer.on("/api/health", HTTP_GET, handle_health_check);

  webServer.addHandler(new AsyncCallbackJsonWebHandler("/api/wifi", handle_wifi_put_request));
  webServer.addHandler(new AsyncCallbackJsonWebHandler("/api/hostname", handle_hostname_put_request));

  webServer.onNotFound(handle_unknown_url);
}

/// @brief Creates both a mutex and timer monitor to observe and
/// accept incoming WiFi joins.
///
/// The timer runs on a different context than the web server,
/// so we need to properly marshall access between contexts.
inline void create_wifi_join_timer(){
  
  join_status_mutex = xSemaphoreCreateMutex();
  if (join_status_mutex == nullptr) {
    // If we get here we are in serious trouble, and there is nothing the
    // code can do. We just die unceremoniously.
    DEBUG_PRINTF("WebServer: failed to create mutex. Process halted.\n");
    for (;;) {
      delay(1000);
    }
  }

  // Software timer to monitor async WiFi joins.
  join_timer = xTimerCreate(
    "WiFiJoinTimer",
    pdMS_TO_TICKS(200),
    pdTRUE,   // Auto re-trigger.
    nullptr,  // Timer ID pointer, not used.
    on_join_timer);
}


inline void initialize_web_server(const APIGetHook api_get_hooks[], const int get_hook_count, APIPutHook api_put_hooks[], const int put_hook_count, const char* password) {

  create_wifi_join_timer();

  setup_networking(password);

  register_api(api_get_hooks, get_hook_count, api_put_hooks, put_hook_count);

  #ifdef SD_LOADER
    webServer.serveStatic("/", SD, "/").setDefaultFile("index.html");
  #else
    webServer.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  #endif

  // Setup access control headers.
  // The settings here are liberal to allow the MDNS connection to function properly
  // with the API
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

  esp_wifi_set_ps(WIFI_PS_NONE);

  webServer.begin();
  MDNS.addService("http", "tcp", 80);
}
