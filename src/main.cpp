#include <Arduino.h>
#include "sense.h"
#include "screen.h"
#include "battery.h"
#include "buttons.h"
#include "utils.h"
#include "wifi_manager.h"
#include "web_portal.h"
#include <esp_task_wdt.h>

WiFiManager wifiManager;
WebPortal webPortal;

void handleCaptivePortal();

void setup()
{
  Serial.begin(115200);

  if (checkBootHold(BTN1, 1000UL))
  {
    Serial.println("[BTN1] Held at boot");
    // TODO: Screen display "Configuration Mode"
    LittleFS.begin(true);
    readConfig();
    wifiManager.startAP(); // start the AP, since you held down btn1
    webPortal.begin(); // start the captive portal
    handleCaptivePortal(); // infinite loop that resets ESP once config saved
  }
  if (checkBootHold(BTN2, 1000UL))
  {
    Serial.println("[BTN2] Held at boot");
    // TODO: Screen display "Matter Commissioning Mode"
  }

  LittleFS.begin(true);
  readConfig();

  setupButtons();
  initializeScreen();
  if (!initializeSensors())
  {
    Serial.println("[SENSE] Initialization failed!");
    while (true)
    ; // halt execution
  }

  String ssid = json["ssid"].isNull() ? "Not set" : json["ssid"].as<String>();
  String pass = json["pass"].isNull() ? "Not set" : json["pass"].as<String>();
  Serial.printf("[WIFI] SSID: %s\n", ssid.c_str());
  Serial.printf("[WIFI] PASS: %s\n", pass.c_str());

  updateDHT();
  screenTest();

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

void loop()
{

}

void handleCaptivePortal()
{
  while(true){
    // Handle web portal
    webPortal.handleClient();

    // Check for restart after config save
    if (webPortal.shouldRestart())
    {
        Serial.println("[MAIN] Restarting...");
        delay(1000);
        ESP.restart();
    }
    delay(50);
  }
}