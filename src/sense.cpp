#include <Arduino.h>
#include "sense.h"
// #include "screen.h" // TODO: add back? do i want to error on screen
#include "utils.h"
#include <Wire.h>
#include "DHT20.h"
#include <SensirionI2CSgp41.h>
#include <SensirionI2cScd4x.h>
#include <VOCGasIndexAlgorithm.h>
#include <NOxGasIndexAlgorithm.h>

#define WIRE Wire

#define intPin 2

DHT20 DHT(&WIRE);
SensirionI2CSgp41 SGP41;
SensirionI2cScd4x SCD41;
Max31328 RTC(&WIRE);

// Gas Algorithms
VOCGasIndexAlgorithm voc_algorithm;
NOxGasIndexAlgorithm nox_algorithm;

// State Variables
int32_t currentVocIndex = 0;
int32_t currentNoxIndex = 0;
unsigned long lastGasTick = 0;
uint16_t conditioning_countdown = 10; // Seconds of NOx conditioning needed on boot

float tempC = -40.0;
float humidity = -1.0;
int TEMP = -40;
int RH = 0;
int CO2 = 0;
int32_t VOC = 0;
int32_t NOx = 0;

static int16_t error;

/*
 * return true if all sensors initialize correctly
 */
bool initializeSensors()
{
  bool success = WIRE.setPins(22, 23) && WIRE.begin(22, 23) && WIRE.setClock(100000) && DHT.begin();
  SGP41.begin(WIRE);       // has void return
  SCD41.begin(WIRE, 0x62); // also void return, can check reinit and wakeup though
  error = SCD41.wakeUp();
  if (error)
  {
    printSensirionError(error, "SCD41 wakeUp");
    success = false;
  }
  error = SCD41.stopPeriodicMeasurement();
  if (error)
  {
    printSensirionError(error, "SCD41 stopPeriodicMeasurement");
    success = false;
  }
  error = SCD41.reinit();
  if (error)
  {
    printSensirionError(error, "SCD41 reinit");
    success = false;
  }
  return success;
}

void initGasAlgorithms() {
    int32_t index_offset, learning_time_offset_hours, learning_time_gain_hours, gating_max_duration_minutes, std_initial, gain_factor;
  
    // Initialize VOC Params
    voc_algorithm.get_tuning_parameters(index_offset, learning_time_offset_hours, learning_time_gain_hours, gating_max_duration_minutes, std_initial, gain_factor);
    
    // Initialize NOx Params
    nox_algorithm.get_tuning_parameters(index_offset, learning_time_offset_hours, learning_time_gain_hours, gating_max_duration_minutes, std_initial, gain_factor);
}

// DHT Functions
bool updateDHT()
{
  int status = DHT.read();

  switch (status)
  {
  case DHT20_OK:
    TEMP = (json["unit_c"].as<int>() == 1) ? (int)round(static_cast<double>(DHT.getTemperature())) : (int)round((static_cast<double>(DHT.getTemperature()) * 1.8) + 32.0);
    RH = (int)round(static_cast<double>(DHT.getHumidity()));
    return true;
  case DHT20_ERROR_CHECKSUM:
    Serial.print("[DHT] Checksum error");
    break;
  case DHT20_ERROR_CONNECT:
    Serial.print("[DHT] Connect error");
    break;
  case DHT20_MISSING_BYTES:
    Serial.print("[DHT] Missing bytes");
    break;
  case DHT20_ERROR_BYTES_ALL_ZERO:
    Serial.print("[DHT] All bytes read zero");
    break;
  case DHT20_ERROR_READ_TIMEOUT:
    Serial.print("[DHT] Read time out");
    break;
  case DHT20_ERROR_LASTREAD:
    Serial.print("[DHT] Error read too fast");
    break;
  default:
    Serial.print("[DHT] Unknown error");
    break;
  }
  Serial.print("\n");
  return false;
}

double getTemp()
{
  printf("[DHT20] Temp: %.1f\n", tempC);
  if (json["unit_c"].as<int>() == 1)
  {
    return static_cast<double>(tempC);
  }
  return (static_cast<double>(tempC) * 1.8) + 32.0;
}

double getHumidity()
{
  return static_cast<double>(humidity);
}

// SGP41 Functions
void printSensirionError(uint16_t error, const char *functionName)
{
  char errorMessage[256];
  Serial.print("[SENSIRION] Error trying to execute ");
  Serial.print(functionName);
  Serial.print("(): ");
  errorToString(error, errorMessage, 256);
  Serial.println(errorMessage);
}

void processGasSensors() {
    // Non-blocking 1Hz Timer
    if (millis() - lastGasTick >= 1000) {
        lastGasTick = millis();

        uint16_t srawVoc = 0;
        uint16_t srawNox = 0;
        uint16_t compRh = 0x8000;
        uint16_t compT = 0x6666;

        // 1. Get Compensation Data (Use cached DHT values)
        // Note: We don't need to read DHT every second, just use the latest available
        if (humidity >= 0 && humidity <= 100 && tempC >= -40 && tempC <= 80) {
            compT = static_cast<uint16_t>((tempC + 45) * 65535 / 175);
            compRh = static_cast<uint16_t>(humidity * 65535 / 100);
        }

        // 2. Read Sensor
        if (conditioning_countdown > 0) {
            // Case A: Still warming up (First 10s)
            // Execute Conditioning (Returns VOC raw, but NOx is not ready)
            error = SGP41.executeConditioning(compRh, compT, srawVoc);
            if (error) {
                printSensirionError(error, "SGP41 executeConditioning");
            } else {
                // We CAN feed VOC during conditioning
                currentVocIndex = voc_algorithm.process(srawVoc);
                // We CANNOT feed NOx yet (value is 0/invalid)
                currentNoxIndex = 0; 
                conditioning_countdown--;
                // Serial.println("[SGP41] Conditioning...");
            }
        } else {
            // Case B: Normal Operation
            error = SGP41.measureRawSignals(compRh, compT, srawVoc, srawNox);
            if (error) {
                printSensirionError(error, "SGP41 measureRawSignals");
            } else {
                // Feed both algorithms
                currentVocIndex = voc_algorithm.process(srawVoc);
                currentNoxIndex = nox_algorithm.process(srawNox);
                // Serial.printf("[SGP41] VOC: %d, NOx: %d\n", currentVocIndex, currentNoxIndex);
                NOx = currentNoxIndex;
                VOC = currentVocIndex;
            }
        }
    }
}

int32_t getVOCIndex() { return VOC; }
int32_t getNOxIndex() { return NOx; }
void printSGP() {
  Serial.printf("[SGP41] VOC Index: %d\n", getVOCIndex());
  Serial.printf("[SGP41] NOx Index: %d\n", getNOxIndex());
}

// SCD41 Functions
int getCO2()
{
  return CO2;
}

bool updateCO2()
{
  uint16_t co2Concentration = 0;
  float temperature = 0.0;
  float relativeHumidity = 0.0;
  //
  // Wake the sensor up from sleep mode.
  //
  error = SCD41.wakeUp();
  if (error)
  {
    printSensirionError(error, "SCD41 wakeUp");
    CO2 = -1;
    return false;
  }
  //
  // Ignore first measurement after wake up.
  //
  error = SCD41.measureSingleShot();
  if (error)
  {
    printSensirionError(error, "SCD41 measureSingleShot()");
    CO2 = -1;
    return false;
  }
  //
  // Perform single shot measurement and read data.
  //
  error = SCD41.measureAndReadSingleShot(co2Concentration, temperature,
                                         relativeHumidity);
  if (error)
  {
    printSensirionError(error, "SCD41 measureAndReadSingleShot()");
    CO2 = -1;
    return false;
  }
  Serial.printf("[SCD41] CO2 concentration: %d\n", co2Concentration);
  Serial.printf("[SCD41] Temp: %.1f\n", temperature);
  Serial.printf("[SCD41] Humidity: %.1f\n", relativeHumidity);
  CO2 = co2Concentration;
  return true;
}

volatile bool minute_interrupt = false;
volatile bool rtc_interrupt_occured = false;

void rtc_interrupt_handler()
{
  rtc_interrupt_occured = true;
}

// RTC Functions
bool setRTCAlarms()
{
  pinMode(intPin, INPUT);

  // Attach interrupt on INT pin of MAX31328.
  attachInterrupt(digitalPinToInterrupt(intPin), rtc_interrupt_handler, FALLING);

  // Configure alarm to fire once per second
  max31328_alrm_t alarm1 = {0};
  alarm1.am1 = 0;
  alarm1.am2 = 1;
  alarm1.am3 = 1;
  alarm1.am4 = 1;
  alarm1.seconds = 50;
  alarm1.minutes = 0;
  alarm1.hours = 0;
  alarm1.day = 1;
  alarm1.date = 1;
  alarm1.dy_dt = 0;

  max31328_alrm_t alarm2 = {0};
  alarm2.am1 = 0;
  alarm2.am2 = 1;
  alarm2.am3 = 1;
  alarm2.am4 = 1;
  alarm2.day = 1;
  alarm2.date = 1;

  max31328_cntl_stat_t regs;
  if (RTC.get_cntl_stat_reg(&regs))
  {
    Serial.println("[RTC] ERROR: Cannot read control register.");
  }

  regs.control |= INTCN | A1IE | A2IE;
  regs.status &= ~(A1F | A2F);

  if (RTC.set_cntl_stat_reg(regs))
  {
    Serial.println("[RTC] ERROR: Cannot set control register.");
  }
  // Set Alarm 1
  if (RTC.set_alarm(alarm1, true))
  {
    Serial.println("[RTC] ERROR: Cannot set alarm 1.");
    return false;
  }
  // Set Alarm 2
  if (RTC.set_alarm(alarm2, false))
  {
    Serial.println("[RTC] ERROR: Cannot set alarm 2.");
    return false;
  }
  return true;
}

void clearRTCInt()
{
  max31328_cntl_stat_t regs;
  RTC.get_cntl_stat_reg(&regs);
  // Alarm 1 fired
  if (regs.status & A1F)
    minute_interrupt = false;
  // else Alarm 2 fired
  else
    minute_interrupt = true;
  regs.status = 0;
  RTC.set_cntl_stat_reg(regs);
}

bool rtcLostPower()
{
  max31328_calendar_t rtcDate = getRTCcal();
  return rtcDate.year < 26;
}

bool setRTCTime(uint32_t hour, uint32_t minute, uint32_t second)
{
  bool hr24 = json["hr24_enable"].as<int>() == 1;
  bool isPM = hour >= 12;
  if (hour > 12)
    hour = hour - 12;
  if (hour == 0)
    hour = 12; // midnight is 12 AM
  max31328_time_t time;
  time.seconds = second;
  time.minutes = minute;
  time.hours = hour;
  time.am_pm = isPM;
  time.mode = !hr24; // 1 = 12-hour mode
  uint16_t error = RTC.set_time(time);
  if (error != 0)
  {
    Serial.println("[RTC] Unable to set RTC time from NTP");
    // TODO: add when B&W screenPrint("Unable to set time on RTC from NTP given these WiFi credentials. Restart device and try again.");
    return false;
  }
  return true;
}

bool setRTCdate(uint32_t weekday, uint32_t date, uint32_t month, uint32_t year)
{
  max31328_calendar_t cal;
  cal.date = date;
  cal.month = month;
  cal.year = year - 2000;
  cal.day = weekday;
  uint16_t error = RTC.set_calendar(cal);
  if (error != 0)
  {
    Serial.println("[RTC] Unable to set RTC date from NTP");
    // TODO: add when B&W screenPrint("Unable to set date on RTC from NTP given these WiFi credentials. Restart device and try again.");
    return false;
  }
  return true;
}

max31328_time_t getRTCTime()
{
  max31328_time_t time;
  RTC.get_time(&time);
  return time;
}

max31328_calendar_t getRTCcal()
{
  max31328_calendar_t cal;
  RTC.get_calendar(&cal);
  return cal;
}