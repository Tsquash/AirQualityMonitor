#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Timezone.h>
#include "utils.h"

class TimeManager {
public:
    TimeManager();
    bool begin();
    void update();
    
    int getHour();
    int getMinute();
    int getSecond();
    int getWeekday();
    int getDay();
    int getMonth();
    int getYear();
    bool isTimeSynced();
    int getFailedAttempts();
    
private:
    WiFiUDP udp;
    Timezone* tz;
    TimeChangeRule* dstRule;
    TimeChangeRule* stdRule;
    int failedAttempts;
    unsigned int localPort;
    
    void setupTimezone();
    time_t getNtpTime();
    time_t getNtpLocalTime();
    void sendNTPpacket(IPAddress& address);
    
    static TimeManager* instance;
    static time_t getNtpLocalTimeWrapper();
};

#endif // TIME_MANAGER_H
