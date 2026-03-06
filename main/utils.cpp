#include "utils.h"
#include <esp_http_client.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_crt_bundle.h>

bool autoConfigureTimezone() {
  // Query the ESP-IDF network interface directly instead of WiFi.status().
  // Matter manages WiFi through ESP-IDF, so Arduino's WiFi class never
  // registers event handlers and WiFi.status() always returns WL_DISCONNECTED.
  constexpr unsigned long WIFI_TIMEOUT_MS = 20000;
  unsigned long deadline = millis() + WIFI_TIMEOUT_MS;

  auto hasIP = []() -> bool {
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) return false;
    esp_netif_ip_info_t ip_info;
    return (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0);
  };

  while (!hasIP() && millis() < deadline)
    delay(500);

  if (!hasIP()) {
    Serial.println("[Time] WiFi did not connect in time.");
    return false;
  }

  char buffer[32] = {0};

  esp_http_client_config_t config = {};
  config.url = "https://ipapi.co/utc_offset/";
  config.timeout_ms = 5000;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.user_agent = "ESP32";

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) {
    Serial.println("[Time] Failed to init HTTP client");
    return false;
  }

  esp_err_t err = esp_http_client_open(client, 0);
  if (err != ESP_OK) {
    Serial.printf("[Time] HTTP open failed: %s\n", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return false;
  }

  int content_length = esp_http_client_fetch_headers(client);
  int status = esp_http_client_get_status_code(client);
  if (content_length < 0 || status != 200) {
    Serial.printf("[Time] HTTP error, status: %d\n", status);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return false;
  }

  int bytes_read = esp_http_client_read(client, buffer, sizeof(buffer) - 1);
  esp_http_client_close(client);
  esp_http_client_cleanup(client);

  if (bytes_read < 5) {
    Serial.println("[Time] Failed to read HTTP response");
    return false;
  }

  // Response is plain text: "+HHMM" or "-HHMM" (e.g. "-0600")
  String offset_str = String(buffer).substring(0, 5);
  int sign = (offset_str[0] == '-') ? -1 : 1;
  int hours = offset_str.substring(1, 3).toInt();
  int minutes = offset_str.substring(3, 5).toInt();
  int total_offset = sign * (hours * 3600 + minutes * 60);

  Serial.printf("[Time] Auto-detected UTC offset: %s (%d seconds)\n", offset_str.c_str(), total_offset);
  configTime(total_offset, 0, "pool.ntp.org");
  return true;
}