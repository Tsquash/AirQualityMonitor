#ifndef SENSE_H
#define SENSE_H 

#include "max31328.h"

bool initializeSensors();

// DHT Functions
bool updateDHT();
double getTemp();
double getHumidity();

// SGP41 Functions
void printSGPError(uint16_t error, const char* functionName);
bool startSGP41Conditioning(); // Call this immediately on wake from deep sleep (@ T-10s)
bool readSGP41Raw(uint16_t &voc, uint16_t &nox); // Call this after 10s of conditioning during light sleep
bool turnOffSGP41(); // Call this after you have read the raw values

// RTC Functions
extern volatile bool interrupt_occured;
extern volatile bool minute_interrupt;

bool setRTCAlarms();
void clearRTCInt();
bool rtcLostPower(); // compare epoch to some arbitrary low (right now?) 
bool setRTCTime(uint32_t hour, uint32_t minute, uint32_t second);  // should set calendar and time from ntp 
bool setRTCdate(uint32_t weekday, uint32_t day, uint32_t month, uint32_t year);
max31328_time_t getRTCTime(); // returns the max time struct
max31328_calendar_t getRTCcal(); // returns the max date struct

#endif