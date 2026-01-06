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
 
  if (!initializeSensors()) Serial.println("[SENSE] Initialization failed!");
  // TODO: Screen print this error
  // no buttons were held, do I know the time, or do I need to set it because config was changed?
  
  if(rtcLostPower() || (!json["just_restarted"].isNull() && json["just_restarted"].as<int>() == 1)){ 
    Serial.println("[MAIN] either detected restart after captive portal save or RTC lost power, attempt to update RTC");

    json["just_restarted"] = 0;
    saveConfig();

    // yes, do i have wifi creds?
    if(json["ssid"].isNull() || json["pass"].isNull() || 
        json["ssid"].as<String>() == "" || json["pass"].as<String>() == ""){
      // no ->  start AP mode for config 
      Serial.println("[RTC] No stored credentials to set RTC after power loss");
      // Screen print that there are no wifi credentials, delay for a few seconds, fallback ap
      fallbackAP();
    }
    else{
      // yes -> connect to wifi, get time from NTP to set RTC, then normal operation
      if(!wifiManager.connectToWiFi()){ 
        // TOOD: screen print could not connect to wifi, try again or reset ssid/pass in config. 
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
      uint32_t date = timeManager.getDay();
      uint32_t month = timeManager.getMonth();
      uint32_t year = timeManager.getYear();
      setRTCTime(hour, minute, second);
      setRTCdate(weekday, date, month, year);
    }
  }
  // RTC either hasnt lost power or has now been set, continue normal operation
  // perform initial sensor reads
  updateDHT();
  // perform initial screen drawing
  drawPage2();
  // do nothing until there is a interrupt by the RTC
  // isr for the T-10 seconds should start heating the SGP41 (look at example), at the minute it should read all sensors and update time
  // could potentially do partial update for the mintues and such, and after x partials do a full? or do a full at the hour or something? test

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
  // TODO: add back displayAP(wifiManager.mac);
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