#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// Global configuration document
extern DynamicJsonDocument json;

// MAC address utilities
String macToStr(const uint8_t* mac);
String macLastThreeSegments(const uint8_t* mac);

// Configuration file management
bool readConfig();
bool saveConfig();

// A fixed-size circular queue for graph data
class DataQueue {
  public:
    static const int MAX_SIZE = 60;
    int16_t buffer[MAX_SIZE];
    int count = 0;
    int head = 0; // Index of the oldest element

    void push(int16_t val) {
      if (count < MAX_SIZE) {
        // If not full, fill next available slot
        buffer[(head + count) % MAX_SIZE] = val;
        count++;
      } else {
        // If full, overwrite head (oldest) and move head forward
        buffer[head] = val; 
        head = (head + 1) % MAX_SIZE;
      }
    }
    
    // Helper to get element at logical index i (0 is oldest, count-1 is newest)
    int16_t get(int i) {
        if(i >= count) return 0;
        return buffer[(head + i) % MAX_SIZE];
    }
    
    int size() {
        return count;
    }
};

#endif // UTILS_H
