#include <Arduino.h>
#include "battery.h"

#define BATTERY_PIN 0 // Analog pin A0

int getBatteryPercentage() {
    // 1. Read the calibrated voltage at the pin in millivolts
    uint32_t raw_mv = analogReadMilliVolts(BATTERY_PIN); 

    // 2. Convert to battery voltage (Pin Voltage * Divider Ratio)
    // Divider Ratio = (R1 + R2) / R2 = (150 + 100) / 100 = 2.5
    float battery_voltage = (raw_mv * 2.5) / 1000.0; // Divide by 1000 to get Volts

    // Debugging: Print this to Serial to see what is actually happening!
    Serial.print("Pin mV: "); Serial.print(raw_mv);
    Serial.print(" | Bat V: "); Serial.println(battery_voltage);

    if (battery_voltage >= 4.20) return 100;
    else if (battery_voltage >= 4.05) return 90;
    else if (battery_voltage >= 3.95) return 80;
    else if (battery_voltage >= 3.85) return 65;
    else if (battery_voltage >= 3.80) return 50;
    else if (battery_voltage >= 3.75) return 40;
    else if (battery_voltage >= 3.70) return 30;
    else if (battery_voltage >= 3.60) return 20;
    else if (battery_voltage >= 3.50) return 10;
    else return 0;
}