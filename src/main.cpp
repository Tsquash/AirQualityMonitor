#include <Arduino.h>
#include "sense.h" // rtc and sensors (I2C handling)
#include "screen.h" // eink
#include "battery.h" // battery monitoring
#include "buttons.h" // button handling
#include "utils.h" // file management helpers
#include "wifi_manager.h" // connecting to wifi 
#include "web_portal.h" // captive portal 
#include "time_manager.h" // all things NTP 

WiFiManager wifiManager;
WebPortal webPortal;
TimeManager timeManager;

void fallbackAP();
void handleCaptivePortal();

void setup()
{
  Serial.begin(115200);

  LittleFS.begin(true);
  readConfig();
  setupButtons();
  initializeScreen();

  if (checkBootHold(BTN1, 1000UL))
  {
    Serial.println("[BTN1] Held at boot");
    fallbackAP();
  }
  if (checkBootHold(BTN2, 1000UL))
  {
    Serial.println("[BTN2] Held at boot");
    screenPrint("Entering Matter Commissioning Mode");
  }
 
  if (!initializeSensors()) Serial.println("[SENSE] Initialization failed!");
  // no buttons were held, do I know the time?
  if(rtcLostPower()){
    // yes, do i have wifi creds?
    if(json["ssid"].isNull() || json["pass"].isNull() || 
        json["ssid"].as<String>() == "" || json["pass"].as<String>() == ""){
      // no ->  start AP mode for config 
      Serial.println("[RTC] No stored credentials to set RTC after power loss");
      fallbackAP();
    }
    else{
      // yes -> connect to wifi, get time from NTP to set RTC, then normal operation
      if(!wifiManager.connectToWiFi()){
        fallbackAP();
      }
      // wifi has connected, get time from NTP
      if(!timeManager.begin()){
        Serial.println("[NTP] TimeManager begin failed");
      }
      uint32_t hour = timeManager.getHour();
      uint32_t minute = timeManager.getMinute();
      uint32_t second = timeManager.getSecond();
      uint32_t weekday = timeManager.getWeekday();
      uint32_t day = timeManager.getDay();
      uint32_t month = timeManager.getMonth();
      uint32_t year = timeManager.getYear();
      setRTCTime(hour, minute, second);
      setRTCdate(day, weekday, month, year);
    }
  }
  // RTC either hasnt lost power or has now been set, continue normal operation


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

void fallbackAP()
{
  Serial.println("[MAIN] Starting fallback AP...");
  wifiManager.startAP(); // start the AP, since you held down btn1
  webPortal.begin(); // start the captive portal
  displayAP(wifiManager.mac);
  handleCaptivePortal(); // infinite loop that resets ESP once config saved
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