#include <Arduino.h>
#include "sense.h"                     // rtc and sensors (I2C handling)
#include "screen.h"                    // eink
#include "pm_link.h"
#include "buttons.h"                   // button handling
#include <Matter.h>
#include <nvs_flash.h>
#include "matter_air_quality_sensor.h"

std::shared_ptr<MatterAirQualitySensor> matterAirQualitySensor;
bool matterCommissioned = false;
unsigned long lastRtcSyncAttemptMs = 0;
constexpr unsigned long RTC_SYNC_INTERVAL_MS = 60UL * 60UL * 1000UL; // 1 hour

[[noreturn]] void factoryResetAllNvs()
{
  Serial.println("[RESET] Erasing the complete NVS partition, including Matter and PM pairing data");

  esp_err_t err = nvs_flash_deinit();
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_INITIALIZED)
    Serial.printf("[RESET] NVS deinitialization warning: %s\n", esp_err_to_name(err));

  err = nvs_flash_erase();
  if (err != ESP_OK)
  {
    Serial.printf("[RESET] Complete NVS erase failed: %s\n", esp_err_to_name(err));
    while (true) delay(1000);
  }

  Serial.println("[RESET] NVS erased; Matter is decommissioned. Restarting.");
  delay(100);
  ESP.restart();
  while (true) delay(1000);
}

void setup()
{
  Serial.begin(115200);

  setupButtons();
  bool resetAtBoot = digitalRead(BTN2) == LOW && checkBootHold(BTN2, 1000UL);
  bool pairAtBoot = !resetAtBoot && digitalRead(BTN1) == LOW && checkBootHold(BTN1, 1000UL);
  pageChangeRequested = false;

  if (resetAtBoot)
    factoryResetAllNvs();

  initializeScreen();

  // Configure Matter node and start
  // Matter manages WiFi entirely through ESP-IDF's Device Layer.
  // Do NOT call Arduino WiFi functions (WiFi.begin, WiFi.disconnect, etc.)
  // as they bypass Matter's event handlers and leave the Arduino WiFi class
  // in a desynced state where WiFi.status() never reflects reality.
  node::config_t node_config;
  node_t *root_node = node::create(&node_config, NULL, NULL);
  if (!root_node)
  {
    Serial.println("[Matter] Root node creation failed.");
    drawCenteredMessage("Matter endpoint creation failed.");
    while (true) delay(1000);
  }
  Serial.println("[Matter] Root node created successfully.");

  auto sensorData = std::make_shared<AirQualitySensor>();
  matterAirQualitySensor = MatterAirQualitySensor::CreateEndpoint(root_node, sensorData);
  if (!matterAirQualitySensor)
  {
    Serial.println("[Matter] Endpoint creation failed.");
    drawCenteredMessage("Matter endpoint creation failed.");
    while (true) delay(1000);
  }

  esp_err_t matterStartResult = esp_matter::start(nullptr);
  if (matterStartResult != ESP_OK)
  {
    Serial.printf("[Matter] Startup failed: %s\n", esp_err_to_name(matterStartResult));
    drawCenteredMessage("Matter startup failed.");
    while (true) delay(1000);
  }

  bool commissionedAtBoot = Matter.isDeviceCommissioned();
  bool pairPM = commissionedAtBoot && pairAtBoot;
  pageChangeRequested = false;
  if (commissionedAtBoot) {
    initializePMLink(pairPM);
    if (pairPM) {
      drawPairingStatus(PairingDisplayStatus::Pairing);
      uint32_t started = millis();
      while (!pmLinkPaired() && millis() - started < 120000) {
        processPMLink(false); // BLE only; sensors/RTC are initialized below.
        delay(10);
      }
      finishPMPairing();
      drawPairingStatus(pmLinkPaired() ? PairingDisplayStatus::Paired
                                       : PairingDisplayStatus::TimedOut);
      delay(4000);
      pageChangeRequested = false;
    }
  }
  if (!initializeSensors())
    Serial.println("[Sensors] Initialization failed; restart the device after checking sensor power and wiring.");

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
      initializePMLink(false);
      setRTCFromNTP();
    }
    else
    {
      Serial.println("[Main] RTC has valid time — continuing without Matter.");
      // matterCommissioned remains false
    }
  }

  // Normal operation
  if (!setRTCAlarms())
    Serial.println("[RTC] Alarm initialization failed; scheduled measurements will not run.");
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
    initializePMLink(false);
    setRTCFromNTP();
    if (matterAirQualitySensor)
      matterAirQualitySensor->UpdateMeasurements();
  }

  processPMLink();

  // maintain the 1Hz Gas Index
  processGasSensors();

  // Periodically mirror ESP system time (SNTP-maintained) to external RTC.
  // Non-blocking: if time is not valid yet, skip and retry next interval.
  unsigned long nowMs = millis();
  if (matterCommissioned && (nowMs - lastRtcSyncAttemptMs >= RTC_SYNC_INTERVAL_MS))
  {
    lastRtcSyncAttemptMs = nowMs;
    // Re-query the current UTC offset so DST changes apply without a reboot.
    setRTCFromNTP();
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
      finishPMMinute();
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
