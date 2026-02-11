#include <Arduino.h>
#include "sense.h"        // rtc and sensors (I2C handling)
#include "screen.h"       // eink
#include "buttons.h"      // button handling
#include "utils.h"        // file management helpers
#include "wifi_manager.h" // connecting to wifi
#include "web_portal.h"   // captive portal
#include "time_manager.h" // all things NTP
#undef IPADDR_NONE
#undef INADDR_NONE
#include <Matter.h>       // matter over thread / wifi
#include "matter_air_quality_sensor.h" // Include the new air quality sensor header

WiFiManager wifiManager;
WebPortal webPortal;
TimeManager timeManager;
// List of Matter Endpoints for this Node
std::shared_ptr<MatterAirQualitySensor> matterAirQualitySensor; // Declare the air quality sensor

void fallbackAP();
void handleCaptivePortal();

#define MATTER true

void setup()
{
  Serial.begin(115200);
  LittleFS.begin(true);
  readConfig();
  setupButtons();
  initializeScreen();
  drawStartup();
  initializeQueues();
  if (checkBootHold(BTN2, 1000UL))
  {
    Serial.println("[BTN2] Held at boot");
    fallbackAP();
  }
  if (!initializeSensors())
    Serial.println("[SENSE] Initialization failed!");
  // no buttons were held, do I know the time, or do I need to set it because config was changed?

  if (rtcLostPower() || (!json["just_restarted"].isNull() && json["just_restarted"].as<int>() == 1))
  {
    Serial.println("[MAIN] either detected restart after captive portal save or RTC lost power, attempt to update RTC");

    json["just_restarted"] = 0;
    saveConfig();

    // yes, do i have wifi creds?
    if (json["ssid"].isNull() || json["pass"].isNull() ||
        json["ssid"].as<String>() == "" || json["pass"].as<String>() == "")
    {
      // no ->  start AP mode for config
      Serial.println("[RTC] No stored credentials to set RTC after power loss");
      fallbackAP();
    }
    else
    {
      // yes -> connect to wifi, get time from NTP to set RTC, then normal operation
      if (!wifiManager.connectToWiFi())
      {
        fallbackAP();
      }
      // wifi has connected, get time from NTP
      if (!timeManager.begin())
      {
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
      setRTCdate(weekday, day, month, year);
      wifiManager.disconnect(); // for matter
    }
  }

  if (MATTER)
  {
    node::config_t node_config;

    node_t *root_node = node::create(&node_config, 
                                     NULL, // Attribute callback (can be NULL for now)
                                     NULL  // Identification callback
                                     );

    if (root_node) {
        Serial.println("[Matter] Root Node created successfully.");
    } else {
        Serial.println("[Matter] Still failed to create Root Node!");
    }
    auto sensorData = std::make_shared<AirQualitySensor>();
    matterAirQualitySensor = MatterAirQualitySensor::CreateEndpoint(root_node, sensorData);

    esp_matter::start(nullptr);
    // This may be a restart of a already commissioned Matter accessory
    if (!Matter.isDeviceCommissioned())
    {
      Serial.println("Matter Node is not commissioned yet.");
      Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
      Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
      // The device is now advertising in the background. 
      // Do NOT wait here. Let the loop() run.
    }
    else
    {
      Serial.println("Matter Node is already commissioned.");
    }
  }
  setRTCAlarms();
  updateDHT();
  updateCO2(true);
  if (matterAirQualitySensor) {
      matterAirQualitySensor->UpdateMeasurements();
  }
  delay(100);
  drawPage1();
}

void loop()
{
  // Maintain the 1Hz Gas Index
  processGasSensors();

  if (pageChangeRequested)
  {
    pageChangeRequested = false;
    changePage();
  }

  if (rtc_interrupt_occured)
  {
    rtc_interrupt_occured = false; // Reset flag immediately
    clearRTCInt();                 // Determine which alarm fired by setting 'minute_interrupt'
    if (minute_interrupt)
    {
      Serial.println("[MAIN] Minute Interrupt");
      page1 ? drawPage1() : drawPage2();
    }
    else
    {
      Serial.println("[MAIN] T-10s Interrupt");
      updateDHT();
      updateCO2();
      if (matterAirQualitySensor) {
      matterAirQualitySensor->UpdateMeasurements();
      }
      vocQueue.push(getVOCIndex());
      co2Queue.push(getCO2());
      noxQueue.push(getNOxIndex());
    }
  }
}

void fallbackAP()
{
  Serial.println("[MAIN] Starting fallback AP...");
  wifiManager.startAP(); // start the AP, since you held down btn1
  webPortal.begin();     // start the captive portal
  displayAP(wifiManager.mac);
  handleCaptivePortal(); // infinite loop that resets ESP once config saved
}

void handleCaptivePortal()
{
  while (true)
  {
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