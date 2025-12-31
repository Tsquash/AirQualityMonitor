#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// Global configuration document
extern JsonDocument json;

// MAC address utilities
String macToStr(const uint8_t* mac);
String macLastThreeSegments(const uint8_t* mac);

// Configuration file management
bool readConfig();
bool saveConfig();

#endif // UTILS_H
