#ifndef SENSE_H
#define SENSE_H 

#include "../lib/Max31328RTC/src/max31328.h"

#define WARN_CO2 2000
#define WARN_VOC 120
#define WARN_NOX 2

bool initializeSensors();
void initGasAlgorithms(); // Setup gas algorithm parameters

extern int TEMP;
extern int RH;
extern int CO2;
extern int32_t VOC;
extern int32_t NOx;

// DHT Functions
bool updateDHT();
double getTemp();
double getHumidity();

void printSensirionError(uint16_t error, const char* functionName);
// SGP41 Functions
void initGasAlgorithms();
void processGasSensors();
int32_t getVOCIndex();
int32_t getNOxIndex();
void printSGP();

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
bool updateCO2();

#endif