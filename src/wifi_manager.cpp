#include "wifi_manager.h"

WiFiManager::WiFiManager() {
}

bool WiFiManager::begin() {
    Serial.println("[WIFI] Initializing WiFi...");
    
    // Check if we have WiFi credentials
    if (json["ssid"].isNull() || json["pass"].isNull() || 
        json["ssid"].as<String>() == "" || json["pass"].as<String>() == "") {
        Serial.println("[WIFI] No WiFi credentials. Starting AP mode.");
        return startAP();
    }
    
    // Try to connect to WiFi
    if (connectToWiFi()) {
        return true;
    }
    
    // Fallback to AP mode
    Serial.println("[WIFI] Failed to connect. Starting AP mode.");
    return startAP();
}

bool WiFiManager::connectToWiFi() {
    String ssid = json["ssid"].as<String>();
    String pass = json["pass"].as<String>();
    
    Serial.print("[WIFI] Connecting to: ");
    Serial.println(ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.print("[WIFI] Connected! IP address: ");
        Serial.println(WiFi.localIP());
        
        setupMDNS();
        return true;
    }
    
    Serial.println();
    Serial.println("[WIFI] Connection failed");
    return false;
}

bool WiFiManager::startAP() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    String apName = "AirQuality-" + macLastThreeSegments(mac);
    
    Serial.print("[WIFI] Starting AP: ");
    Serial.println(apName);
    
    WiFi.mode(WIFI_AP);
    
    IPAddress apIP;
    apIP.fromString(AP_IP);
    IPAddress gateway = apIP;
    IPAddress subnet(255, 255, 255, 0);
    
    WiFi.softAPConfig(apIP, gateway, subnet);
    WiFi.softAP(apName.c_str());
    
    Serial.print("[WIFI] AP IP address: ");
    Serial.println(WiFi.softAPIP());
    
    setupMDNS();
    return true;
}

bool WiFiManager::setupMDNS() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    String hostname = "AirQuality-" + macLastThreeSegments(mac);
    hostname.toLowerCase();
    
    if (MDNS.begin(hostname.c_str())) {
        Serial.print("[MDNS] Responder started: ");
        Serial.println(hostname);
        MDNS.addService("http", "tcp", 80);
        return true;
    }
    
    Serial.println("[MDNS] Failed to start responder");
    return false;
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::getLocalIP() {
    if (WiFi.getMode() == WIFI_AP) {
        return WiFi.softAPIP().toString();
    }
    return WiFi.localIP().toString();
}

void WiFiManager::disconnect() {
    WiFi.disconnect();
}
