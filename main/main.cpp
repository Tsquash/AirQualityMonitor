#include <Arduino.h>
#include "sense.h"                     // rtc and sensors (I2C handling)
#include "screen.h"                    // eink
#include "buttons.h"                   // button handling
#include "utils.h"                     // file management helpers
//#include "wifi_manager.h"              // connecting to wifi
//#include "web_portal.h"                // captive portal
// #include "time_manager.h"              // all things NTP
#include <Matter.h>
#include "matter_air_quality_sensor.h" // Include the new air quality sensor header

// WiFiManager wifiManager;
// WebPortal webPortal;
// TimeManager timeManager;
// List of Matter Endpoints for this Node
std::shared_ptr<MatterAirQualitySensor> matterAirQualitySensor; // Declare the air quality sensor

// void fallbackAP();
// void handleCaptivePortal();

bool matterCommissioned = false; // this will probably need to be extern?
unsigned long lastRtcSyncAttemptMs = 0;
constexpr unsigned long RTC_SYNC_INTERVAL_MS = 60UL * 60UL * 1000UL; // 1 hour

void setup()
{
  Serial.begin(115200);

  setupButtons();

  // Configure Matter node and start
  // Matter manages WiFi entirely through ESP-IDF's Device Layer.
  // Do NOT call Arduino WiFi functions (WiFi.begin, WiFi.disconnect, etc.)
  // as they bypass Matter's event handlers and leave the Arduino WiFi class
  // in a desynced state where WiFi.status() never reflects reality.
  node::config_t node_config;
  node_t *root_node = node::create(&node_config, NULL, NULL);
  root_node ? Serial.println("[Matter] Root Node created successfully.") : Serial.println("[Matter] Failed to create Root Node!");
  auto sensorData = std::make_shared<AirQualitySensor>();
  matterAirQualitySensor = MatterAirQualitySensor::CreateEndpoint(root_node, sensorData);
  esp_matter::start(nullptr);
  // Matter.begin();

  bool commissionedAtBoot = Matter.isDeviceCommissioned();
  if (commissionedAtBoot && checkBootHold(BTN2, 1000UL))
  {
    Serial.println("[BTN2] Held at boot on commissioned device — decommissioning");
    esp_matter::factory_reset();
    ESP.restart();
  }

  initializeScreen();
  initializeQueues();
  initializeSensors();

  if(commissionedAtBoot)
  {
    Serial.println("[Matter] Node is already commissioned.");
    matterCommissioned = true;
    drawStartup();
    setRTCFromNTP();
  }
  else
  {
    Serial.println("[Matter] Node is not commissioned yet.");
    drawCommission();
    delay(15000);

    if (rtcLostPower())
    {
      Serial.println("[Main] RTC lost power — waiting for Matter commissioning...");
      while (!Matter.isDeviceCommissioned())
        delay(1000);

      Serial.println("[Matter] Commissioned — setting RTC from NTP.");
      matterCommissioned = true;
      setRTCFromNTP();
    }
    else
    {
      Serial.println("[Main] RTC has valid time — continuing without Matter.");
      // matterCommissioned remains false
    }
  }

  // Normal operation
  setRTCAlarms();
  updateDHT();
  updateCO2(true);
  if (matterCommissioned)
    matterAirQualitySensor->UpdateMeasurements();
  delay(100);
  drawPage1();
}

void loop()
{
  // handle late commissioning (after startup 15s timeout)
  if (!matterCommissioned && Matter.isDeviceCommissioned())
  {
    Serial.println("[Matter] Late commissioning detected — enabling Matter updates.");
    matterCommissioned = true;
    setRTCFromNTP();
    if (matterAirQualitySensor)
      matterAirQualitySensor->UpdateMeasurements();
  }

  // maintain the 1Hz Gas Index
  processGasSensors();

  // Periodically mirror ESP system time (SNTP-maintained) to external RTC.
  // Non-blocking: if time is not valid yet, skip and retry next interval.
  unsigned long nowMs = millis();
  if (matterCommissioned && (nowMs - lastRtcSyncAttemptMs >= RTC_SYNC_INTERVAL_MS))
  {
    lastRtcSyncAttemptMs = nowMs;
    if (!setRTCFromSystemTimeIfValid())
    {
      Serial.println("[RTC] Periodic sync skipped (system time not valid yet).");
    }
  }

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
      if (matterCommissioned)
      {
        matterAirQualitySensor->UpdateMeasurements();
      }
      vocQueue.push(getVOCIndex());
      co2Queue.push(getCO2());
      noxQueue.push(getNOxIndex());
    }
  }
}