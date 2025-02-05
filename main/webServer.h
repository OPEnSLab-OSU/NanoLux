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

/*
 * WIFI management data.
 */
constexpr int MAX_WIFI_CONNECT_WAIT = 100;
constexpr int MAX_NETWORKS = 15;
constexpr int END_OF_DATA = 9999;
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
const char* SETTINGS_FILE = "/settings.json";
static StaticJsonDocument<384> settings;
const char* EMPTY_SETTING = "#_None_#";
volatile bool dirty_settings = false;



/*
 * Web Server related.
 */
constexpr int HTTP_OK = 200;
constexpr int HTTP_ACCEPTED = 202;
constexpr int HTTP_BAD_REQUEST = 400;
constexpr int HTTP_UNAUTHORIZED = 401;
constexpr int HTTP_METHOD_NOT_ALLOWED = 405;
constexpr int HTTP_UNPROCESSABLE = 422;
constexpr int HTTP_INTERNAL_ERROR = 500;
constexpr int HTTP_UNAVAILABLE = 503;

const char* CONTENT_JSON = "application/json";
const char* CONTENT_TEXT = "text/plain";

static String http_response;
const char* URL_FILE = "/assets/url.json";
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

/*
 * File system
 */
inline void initialize_file_system() {

  #ifdef SD_LOADER
    DEBUG_PRINTF("Initializing SD FS...");
    SPI.begin(SCK, MISO, MOSI, CS);
    if (!SD.begin(CS))
      DEBUG_PRINTF("Card Mount Failed");
    DEBUG_PRINTF("Card mount successful.");
  #else
    DEBUG_PRINTF("Initializing FS...");
    if (LittleFS.begin()) {
      DEBUG_PRINTF("done.\n");
    } else {
      DEBUG_PRINTF("fail.\n");
    }
  #endif 
}

/*
 * Settings Management
 */
inline void save_settings() {
  if (!settings.containsKey("wifi")) {
    settings.createNestedObject("wifi");
  }
  settings["hostname"] = hostname;
  settings["wifi"]["ssid"] = current_wifi.SSID;
  settings["wifi"]["key"] = current_wifi.Key;

  #ifdef SD_LOADER
    File saved_settings = SD.open(SETTINGS_FILE, "w");
  #else
    File saved_settings = LittleFS.open(SETTINGS_FILE, "w");
  #endif
  
  if (saved_settings) {
    serializeJson(settings, saved_settings);
    DEBUG_PRINTF("Saving settings:\n");
    serializeJsonPretty(settings, Serial);
    DEBUG_PRINTF("\n");
  } else {
    DEBUG_PRINTF("Unable to save settings file.\n");
  }
}


inline void load_settings() {
  DEBUG_PRINTF("Checking if settings are available.\n");

  #ifdef SD_LOADER
    File saved_settings = SD.open(SETTINGS_FILE, "r");
  #else
    File saved_settings = LittleFS.open(SETTINGS_FILE, "r");
  #endif
  
  if (saved_settings) {
    const DeserializationError error = deserializeJson(settings, saved_settings);
    if (!error) {
      DEBUG_PRINTF("Settings loaded:\n");
      serializeJsonPretty(settings, Serial);
      DEBUG_PRINTF("\n");

      hostname = settings["hostname"].as<String>();
      current_wifi.SSID = settings["wifi"]["ssid"].as<String>();
      current_wifi.Key = settings["wifi"]["key"].as<String>();

      return;
    }

    DEBUG_PRINTF("Error loading saved file: %s\n", error.c_str());
  }

  DEBUG_PRINTF("Unable to load settings. Will use defaults (re)create file.\n");

  current_wifi.SSID = EMPTY_SETTING;
  current_wifi.Key = EMPTY_SETTING;
  hostname = DEFAULT_HOSTNAME;
  save_settings();
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


/*
 * Wifi API handling
 */
inline void serve_wifi_list(AsyncWebServerRequest* request) {
  if (join_in_progress) {
    request->send(HTTP_UNAVAILABLE);
    return;
  }

  scan_ssids();

  StaticJsonDocument<1024> json_list;
  int wifi_number = 0;
  while (wifi_number < MAX_NETWORKS && available_networks[wifi_number].RSSI != END_OF_DATA) {
    JsonObject wifi = json_list.createNestedObject();
    wifi["ssid"] = available_networks[wifi_number].SSID;
    wifi["rssi"] = available_networks[wifi_number].RSSI;
    wifi["lock"] = available_networks[wifi_number].EncryptionType != WIFI_AUTH_OPEN;
    wifi_number++;
  }

  // If no results, return an empty array.
  String wifi_list;
  serializeJson(json_list, wifi_list);
  if (wifi_list == "null") {
    wifi_list = "[]";
  }

  DEBUG_PRINTF("Sending networks:\n%s\\n", wifi_list.c_str());
  request->send(HTTP_OK, CONTENT_JSON, wifi_list);
}


inline void handle_wifi_put_request(AsyncWebServerRequest* request, JsonVariant& json) {
  if (request->method() == HTTP_PUT) {
    const JsonObject& payload = json.as<JsonObject>();

    int status = HTTP_OK;

    bool joined = false;
    if (payload["ssid"] == nullptr) {
      DEBUG_PRINTF("/api/wifi: Forgetting current network.\n");
      WiFi.disconnect();
      delay(100);

      current_wifi.SSID = EMPTY_SETTING;
      current_wifi.Key = EMPTY_SETTING;
      save_settings();

      joined = true;
    } else {
      DEBUG_PRINTF("/api/wifi: Joining network.\n");
      joined = join_wifi(payload["ssid"], payload["key"]);
    }

    int response_status;
    String message;
    if (joined) {
      response_status = HTTP_ACCEPTED;
      message = "Operation completed.";
    } else {
      response_status = HTTP_INTERNAL_ERROR;
      message = "Unable to monitor join operation: could not start timer or get mutex.";
      DEBUG_PRINTF("%s\n", message.c_str());
    }
    request->send(response_status, CONTENT_JSON, build_response(joined, message.c_str(), nullptr));
  } else {
    request->send(HTTP_METHOD_NOT_ALLOWED);
  }
}


inline void handle_wifi_get_request(AsyncWebServerRequest* request) {
  const String wifi = current_wifi.SSID == EMPTY_SETTING ? String("null") : String("\"") + String(current_wifi.SSID) + String("\"");
  const bool connected = current_wifi.SSID == EMPTY_SETTING ? false : (WiFi.status() == WL_CONNECTED);
  const String response = String("{ \"ssid\": ") + String(wifi) + String(", \"connected\": ") + (connected ? "true" : "false") + String(" }");

  DEBUG_PRINTF("Sending current wifi: %s\n", response.c_str());
  request->send(HTTP_OK, CONTENT_JSON, response);
}

inline void handle_wifi_status_request(AsyncWebServerRequest* request) {
  const String wifi = current_wifi.SSID == EMPTY_SETTING ? String("null") : String("\"") + String(current_wifi.SSID) + String("\"");

  String status;
  if (xSemaphoreTake(join_status_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (join_in_progress) {
      status = "pending";
    } else if (join_succeeded) {
      status = "success";
    } else {
      status = "fail";
    }

    xSemaphoreGive(join_status_mutex);
  }

  const String response = String("{ \"ssid\": ") + String(wifi) + String(", \"status\": \"") + status + String("\" }");

  DEBUG_PRINTF("Sending wifi status: %s\n", response.c_str());
  request->send(HTTP_OK, CONTENT_JSON, response);
}


/*
 * Hostname API handling
 */
inline void handle_hostname_put_request(AsyncWebServerRequest* request, JsonVariant& json) {
  if (server_unavailable) {
    request->send(HTTP_UNAVAILABLE);
    return;
  }

  if (request->method() == HTTP_PUT) {
    const JsonObject& payload = json.as<JsonObject>();

    int status = HTTP_OK;

    hostname = payload["hostname"].as<String>();
    save_settings();
    DEBUG_PRINTF("Hostname %s saved.\n", hostname.c_str());
    request->send(HTTP_OK,
                  CONTENT_TEXT,
                  build_response(true, "New hostname saved.", nullptr));
  }
}

inline void handle_hostname_get_request(AsyncWebServerRequest* request) {
  if (server_unavailable) {
    request->send(HTTP_UNAVAILABLE);
    return;
  }

  const String response = String("{ \"hostname\": ") + "\"" + String(hostname) + String("\" }");

  DEBUG_PRINTF("Sending current hostname: %s\n", response.c_str());
  request->send(HTTP_OK, CONTENT_JSON, response);
}


/*
 * Health ping.
 */
inline void handle_health_check(AsyncWebServerRequest* request) {
  if (server_unavailable) {
    request->send(HTTP_UNAVAILABLE);
    return;
  }

  DEBUG_PRINTF("Pong.\n");
  request->send(HTTP_OK);
}


/*
 * Unknown path (404) handler
 */
inline void handle_unknown_url(AsyncWebServerRequest* request) {
  // If browser sends preflight to check for CORS we tell them
  // it's okay. NOTE: Google is stubborn about it. You will need
  // to disable strict CORS checking using the --disable-web-security
  // option when starting it.
  if (request->method() == HTTP_OPTIONS) {
    request->send(200);
    return;
  }

  // Otherwise, we got an unknown request. Print info about it
  // that may be useful for debugging.
  String method;
  switch (request->method()) {
    case HTTP_GET:
      method = "GET";
      break;
    case HTTP_POST:
      method = "POST";
      break;
    case HTTP_PUT:
      method = "PUT";
      break;
    default:
      method = "UNKNOWN";
  }
  DEBUG_PRINTF("Not Found: %s -> http://%s%s\n", method.c_str(), request->host().c_str(), request->url().c_str());

  if (request->contentLength()) {
    DEBUG_PRINTF("_CONTENT_TYPE: %s\n", request->contentType().c_str());
    DEBUG_PRINTF("_CONTENT_LENGTH: %u\n", request->contentLength());
  }

  const int headers = request->headers();
  for (int i = 0; i < headers; i++) {
    const AsyncWebHeader* header = request->getHeader(i);
    DEBUG_PRINTF("_HEADER[%s]: %s\n", header->name().c_str(), header->value().c_str());
  }

  request->send(404);
}


inline void save_url(const String& url) {
  // This is a static file that lives in the "assets" of the web app.
  // When it starts, it will check this file to know which URL to use to talk
  // back to the server. The reason we need this is that we don't know which
  // route is available (AP or STA) until runtime.
  #ifdef SD_LOADER
    File saved_url = SD.open(URL_FILE, "w");
  #else
    File saved_url = LittleFS.open(URL_FILE, "w");
  #endif
  
  if (saved_url) {
    StaticJsonDocument<192> data;

    data["url"] = url;
    Serial.println(url);

    serializeJson(data, saved_url);
    DEBUG_PRINTF("%s saved as Web App URL.\n", url.c_str());
  } else {
    DEBUG_PRINTF("Unable to save Web App URL, will default to http://192.168.4.1.\n");
  }
}


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
  ALWAYS_PRINTF("Backend availbale at: %s", api_url.c_str());
}


inline void initialize_web_server(const APIGetHook api_get_hooks[], const int get_hook_count, APIPutHook api_put_hooks[], const int put_hook_count, const char* password) {
  // Mutex to make join status globals thread safe.
  // The timer below runs on a different context than the web server,
  // so we need to properly marshall access between contexts.
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

  setup_networking(password);

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


  // Register the Web App
  DEBUG_PRINTF("Registering Web App files.\n");

  #ifdef SD_LOADER
    webServer.serveStatic("/", SD, "/").setDefaultFile("index.html");
  #else
    webServer.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  #endif

  
  webServer.onNotFound(handle_unknown_url);

  // "Disable" CORS.
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");
  esp_wifi_set_ps(WIFI_PS_NONE);
  webServer.begin();
  MDNS.addService("http", "tcp", 80);
}
