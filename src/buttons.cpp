#include <Arduino.h>
#include "buttons.h"
#include "screen.h"

volatile bool pageChangeRequested = false;
volatile unsigned long lastPressTime = 0;
const unsigned long DEBOUNCE_DELAY = 750;
extern int refreshCounter;

void IRAM_ATTR pageChangeISR() {
    unsigned long currentTime = millis();
    if (currentTime - lastPressTime > DEBOUNCE_DELAY) {
        pageChangeRequested = true;
        lastPressTime = currentTime;
        // Force full refresh on page change to sync buffers
        refreshCounter = 9; // next refresh will be 10th (full refresh)
    }
}

void setupButtons(){
    pinMode(BTN1, INPUT_PULLUP);
    pinMode(BTN2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BTN1), pageChangeISR, FALLING);
}

bool checkBootHold(int pin, unsigned long holdTime) {
    unsigned long startPress = 0;
    unsigned long windowStart = millis();

    // Check for 3 seconds total window
    while (millis() - windowStart < 2000) { 
        if (digitalRead(pin) == LOW) { // Button Pressed
            if (startPress == 0) startPress = millis();
            // If held long enough, return TRUE immediately
            if (millis() - startPress > holdTime) return true;
        } else {
            startPress = 0; // Reset if they let go
        }
        delay(10); // debounce / cpu relief
    }
    return false;
}