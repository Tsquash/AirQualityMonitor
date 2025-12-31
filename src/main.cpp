#include <Arduino.h>
#include "sense.h"
#include "screen.h"
#include "battery.h"
#include "buttons.h"

void setup() {
  setupButtons(); // must be first thing you do, checks for boot hold for 2 seconds 

  Serial.begin(115200);
  delay(5000);
  if(btn1Boot()){
    Serial.println("Button 1 held at boot!");
  }
  else Serial.println("Btn1 not held at boot.");
  if(btn2Boot()){
    Serial.println("Button 2 held at boot!");
  }
  else Serial.println("Btn2 not held at boot.");
  //initializeScreen(); 
  if(!initializeSensors()){
    Serial.println("Sensor initialization failed!");
    while(true); // halt execution
  }

  //screenTest();
  
  /* SGP41 TYPICAL SEQUENCE
  if (startSGP41Conditioning()) {
    delay(10000); 
  } else {
      Serial.println("Skipping conditioning (Sensor missing?)");
  }
  updateDHT(); // intial DHT read for the SGP41 comp
  uint16_t voc, nox;
  if (readSGP41Raw(voc, nox)) {
    Serial.printf("VOC: %d, NOx: %d\n", voc, nox);
  } else {
    Serial.println("SGP41 Read Failed");
  }
  // turn the heater off since we read
  if(!turnOffSGP41()){
    Serial.println("Failed to turn off SGP41");
  } 
  */
}

void loop() {
  
}
