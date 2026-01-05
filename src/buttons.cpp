#include <Arduino.h>
#include "buttons.h"

void setupButtons(){
    pinMode(BTN1, INPUT_PULLUP);
    pinMode(BTN2, INPUT_PULLUP);

    // TODO: initialize interrupts
}

bool checkBootHold(int pin, unsigned long holdTime) {
    unsigned long startPress = 0;
    unsigned long windowStart = millis();

    // Check for 3 seconds total window
    while (millis() - windowStart < 3000) { 
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