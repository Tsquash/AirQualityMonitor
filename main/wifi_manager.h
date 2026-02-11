#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "utils.h"
#include "esp_wifi.h"
#include "esp_netif.h"

#ifndef AP_IP
#define AP_IP "192.168.0.1"
#endif

class WiFiManager {
public:
    uint8_t* mac;
    WiFiManager();
    bool begin();
    bool connectToWiFi();
    bool startAP();
    bool isConnected();
    String getLocalIP();
    void disconnect();
    
private:
    bool setupMDNS();
};

#endif // WIFI_MANAGER_H
