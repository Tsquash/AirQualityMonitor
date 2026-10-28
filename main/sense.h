#ifndef SENSE_H
#define SENSE_H 

#include "../lib/Max31328RTC/src/max31328.h"

bool initializeSensors();

extern float tempC;
extern int TEMP;
extern int RH;
extern int CO2;
extern int32_t VOC;
extern int32_t NOx;

uint8_t getOverallAirQuality(); // Matter enum: Good=1 through ExtremelyPoor=6

// DHT Functions
bool updateDHT();

void printSensirionError(uint16_t error, const char* functionName);
// SGP41 Functions
void processGasSensors();
int32_t getVOCIndex();
int32_t getNOxIndex();

// RTC Functions
extern volatile bool rtc_interrupt_occured;
extern volatile bool minute_interrupt;

bool setRTCAlarms();
void clearRTCInt();
bool rtcLostPower(); // compare epoch to some arbitrary low (right now?) 
bool setRTCTime(uint32_t hour, uint32_t minute, uint32_t second);  // should set calendar and time from ntp 
bool setRTCdate(uint32_t weekday, uint32_t day, uint32_t month, uint32_t year);
max31328_time_t getRTCTime(); // returns the max time struct
max31328_calendar_t getRTCcal(); // returns the max date struct

// CO2 Functions
int getCO2();
bool updateCO2(bool discard = false);

// NTP -> Hardware RTC
void setRTCFromNTP();

#endif
