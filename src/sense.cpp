#include <Arduino.h>
#include "sense.h"
#include "screen.h"

#include <Wire.h> 
#include "DHT20.h"
#include <SensirionI2CSgp41.h>

// Set I2C bus to use: Wire, Wire1, etc.
#define WIRE Wire

DHT20 DHT(&WIRE);
SensirionI2CSgp41 SGP41;
Max31328 RTC(&WIRE);

float tempC = -40.0;
float humidity = -1.0;

double tempF = -40.0;
double humidityPercent = -1.0;

/*
* return true if all sensors initialize correctly
*/
bool initializeSensors(){
  bool success = WIRE.setPins(22, 23)
        && WIRE.begin(22, 23)
        && WIRE.setClock(100000) 
        && DHT.begin();
  SGP41.begin(WIRE); // has void return
  return success;
}

// DHT Functions
bool updateDHT(){
    int status = DHT.read();

    switch (status)
    {
      case DHT20_OK:
        tempC = DHT.getTemperature();
        humidity = DHT.getHumidity();
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

double getTemp(){ 
  return tempF = (static_cast<double>(tempC) * 1.8) + 32.0;
}

double getHumidity(){
  return humidityPercent = static_cast<double>(humidity);
}

// SGP41 Functions
void printSGPError(uint16_t error, const char* functionName) {
    char errorMessage[256];
    Serial.print("[SGP41] Error trying to execute ");
    Serial.print(functionName);
    Serial.print("(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
}

bool startSGP41Conditioning() {
    uint16_t error;
    uint16_t srawVoc = 0; // dummy variable
    
    // default values for the conditioning phase are okay
    uint16_t defaultRh = 0x8000;
    uint16_t defaultT = 0x6666;

    // Send the command
    error = SGP41.executeConditioning(defaultRh, defaultT, srawVoc);

    if (error) {
        printSGPError(error, "executeConditioning");
        return false;
    }
    return true;
}

bool readSGP41Raw(uint16_t &voc, uint16_t &nox) {
    uint16_t error;
    
    // If DHT fails, default to standard values (50% RH, 25°C)
    uint16_t compensationRh = 0x8000;
    uint16_t compensationT = 0x6666; 
    
    // only use my cached DHT readings if they are valid, 
    // otherwise just use defaults
    if(humidity >= 0 && humidity <= 100 && tempC >= -40 && tempC <= 80) {
      compensationT = static_cast<uint16_t>((tempC + 45) * 0xFFFF / 175);
      compensationRh = static_cast<uint16_t>(humidity * 0xFFFF / 100);
    }
    
    // perform the measurement
    error = SGP41.measureRawSignals(compensationRh, compensationT, voc, nox);

    if (error) {
        printSGPError(error, "measureRawSignals");
        // Set values to 0 (or your preferred "invalid" flag) to be safe
        voc = -1;
        nox = -1;
        return false;
    }
    return true;
}

bool turnOffSGP41() {
  uint16_t error = SGP41.turnHeaterOff(); 

    if (error) {
        printSGPError(error, "turnHeaterOff");
        return false;
    }
    return true;
}

// RTC Functions
bool setRTCAlarms(){
  return true;
}
bool rtcLostPower(){
  max31328_calendar_t rtcDate = getRTCdate();
  return rtcDate.year < 26;
} 
bool setRTCTime(uint32_t hour, uint32_t minute, uint32_t second){
  bool isPM = hour >= 12;
  if(hour > 12) hour = hour - 12;
  if(hour == 0) hour = 12; // midnight is 12 AM
  max31328_time_t time;
  time.seconds = second;
  time.minutes = minute;
  time.hours   = hour;
  time.am_pm   = isPM;
  time.mode    = 1; // 1 = 12-hour mode
  uint16_t error = RTC.set_time(time);
  if(error != 0){ 
    Serial.println("[RTC] Unable to set RTC time from NTP"); 
    //TODO: add when B&W screenPrint("Unable to set time on RTC from NTP given these WiFi credentials. Restart device and try again.");
    return false;
  }
  return true;
}
bool setRTCdate(uint32_t weekday, uint32_t date, uint32_t month, uint32_t year){
  max31328_calendar_t cal;
  cal.date = date;
  cal.month = month;
  cal.year = year - 2000;
  cal.day = weekday;
  uint16_t error = RTC.set_calendar(cal);
  if(error != 0){ 
    Serial.println("[RTC] Unable to set RTC date from NTP"); 
    //TODO: add when B&W screenPrint("Unable to set date on RTC from NTP given these WiFi credentials. Restart device and try again.");
    return false;
  }
  return true;
}
max31328_time_t getRTCTime(){
  max31328_time_t time;
  RTC.get_time(&time);
  return time;
}
max31328_calendar_t getRTCdate(){
  max31328_calendar_t date;
  RTC.get_calendar(&date);
  return date;
}