#include <Arduino.h>
//#include "sense.h"
#include "screen.h"

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  initializeScreen();
  /*
  if(!initializeSensors()){
    Serial.println("Sensor initialization failed!");
    while(true); // halt execution
  }
  */
  /*
  if (startSGP41Conditioning()) {
    delay(10000); 
  } else {
      Serial.println("Skipping conditioning (Sensor missing?)");
  }
  */ 
  //updateDHT(); // intial DHT read for the SGP41 comp
  /* uint16_t voc, nox;
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
  Serial.println("Screen test");
  screenTest();
  Serial.println("Screen test over");
  while(true);
}

void loop() {
  // put your main code here, to run repeatedly:
  /*
  updateDHT();
  Serial.print("temperature: ");
  Serial.println(getTemp(),1);
  Serial.print("humidity: ");
  Serial.println(getHumidity(),1);
  delay(5000);
  */
}
