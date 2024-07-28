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