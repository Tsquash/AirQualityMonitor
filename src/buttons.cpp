#include <Arduino.h>
#include "buttons.h"

InterruptDrivenButton btn1(BTN1);
InterruptDrivenButton btn2(BTN2);

DEFINE_IDB_ISR(btn1) 
DEFINE_IDB_ISR(btn2)

void setupButtons(){
    pinMode(BTN1, INPUT_PULLUP);
    pinMode(BTN2, INPUT_PULLUP);

    btn1.setup(IDB_ISR(btn1)); 
    btn2.setup(IDB_ISR(btn2)); 
}

// Standalone replacement - No Library needed
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