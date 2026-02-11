#include "time_manager.h"

// Static instance pointer for TimeLib callback
TimeManager* TimeManager::instance = nullptr;

TimeManager::TimeManager() : failedAttempts(0), localPort(8888), tz(nullptr), dstRule(nullptr), stdRule(nullptr) {
    instance = this;
}

bool TimeManager::begin() {
    Serial.println("[TIME] Initializing time manager...");
    
    // Start UDP for NTP
    udp.begin(localPort);
    Serial.print("[TIME] UDP started on port: ");
    Serial.println(localPort);
    
    // Setup timezone from config
    setupTimezone();
    
    // Set the time sync provider
    setSyncProvider(getNtpLocalTimeWrapper);
    setSyncInterval(3600); // Sync every hour
    
    Serial.println("[TIME] Time manager initialized");
    return true;
}

void TimeManager::setupTimezone() {
    // Read timezone configuration from JSON
    int stdOffset = json["std_offset"].isNull() ? -300 : json["std_offset"].as<int>();
    int dstOffset = json["dst_offset"].isNull() ? -240 : json["dst_offset"].as<int>();
    bool dstEnable = json["dst_enable"].isNull() ? false : json["dst_enable"].as<bool>();
    
    // Standard time rule
    int stdWeek = json["std_week"].isNull() ? First : json["std_week"].as<int>();
    int stdDay = json["std_day"].isNull() ? Sun : json["std_day"].as<int>();
    int stdMonth = json["std_month"].isNull() ? Nov : json["std_month"].as<int>();
    int stdHour = json["std_hour"].isNull() ? 2 : json["std_hour"].as<int>();
    
    // Daylight saving time rule
    int dstWeek = json["dst_week"].isNull() ? Second : json["dst_week"].as<int>();
    int dstDay = json["dst_day"].isNull() ? Sun : json["dst_day"].as<int>();
    int dstMonth = json["dst_month"].isNull() ? Mar : json["dst_month"].as<int>();
    int dstHour = json["dst_hour"].isNull() ? 2 : json["dst_hour"].as<int>();
    
    TimeChangeRule EDT2 = {"EDT", (uint8_t)dstWeek, (uint8_t)dstDay, (uint8_t)dstMonth, (uint8_t)dstHour, dstOffset};
    TimeChangeRule EST2 = {"EST", (uint8_t)stdWeek, (uint8_t)stdDay, (uint8_t)stdMonth, (uint8_t)stdHour, stdOffset};
    
    stdRule = new TimeChangeRule(EST2);
    dstRule = new TimeChangeRule(EDT2);
    
    if (dstEnable) {
        tz = new Timezone(*dstRule, *stdRule);
        Serial.println("[TIME] Timezone configured with DST");
    } else {
        tz = new Timezone(*stdRule, *stdRule);
        Serial.println("[TIME] Timezone configured without DST");
    }
    
    Serial.print("[TIME] Standard offset: ");
    Serial.print(stdOffset);
    Serial.println(" minutes");
    if (dstEnable) {
        Serial.print("[TIME] DST offset: ");
        Serial.print(dstOffset);
        Serial.println(" minutes");
    }
}

time_t TimeManager::getNtpLocalTimeWrapper() {
    if (instance) {
        return instance->getNtpLocalTime();
    }
    return 0;
}

time_t TimeManager::getNtpLocalTime() {
    const int retries = 3;
    int iterator = 0;
    time_t receivedTime = 0;
    
    while (iterator < retries && receivedTime == 0) {
        receivedTime = getNtpTime();
        iterator++;
    }
    
    if (receivedTime == 0) {
        failedAttempts++;
        Serial.print("[NTP] Sync failed. Attempt: ");
        Serial.println(failedAttempts);
        return 0;
    }
    
    Serial.print("[NTP] Sync success! Received NTP timestamp: ");
    Serial.println(receivedTime);
    failedAttempts = 0;
    
    // Convert to local time using timezone
    return tz->toLocal(receivedTime);
}

time_t TimeManager::getNtpTime() {
    const int NTP_PACKET_SIZE = 48;
    byte packetBuffer[NTP_PACKET_SIZE];
    IPAddress ntpServerIP;
    
    // Discard any previously received packets
    while (udp.parsePacket() > 0);
    
    // Get NTP server from config or use default
    const char* server = json["ntp"].as<const char*>();
    if (server == NULL || server[0] == '\0') {
        server = "pool.ntp.org";
    }
    
    Serial.print("[NTP] Connecting to server: ");
    Serial.println(server);
    
    if (WiFi.hostByName(server, ntpServerIP) != 1) {
        Serial.println("[NTP] Server not found...");
        return 0;
    }
    
    Serial.print("[NTP] Server IP: ");
    Serial.println(ntpServerIP);
    
    sendNTPpacket(ntpServerIP);
    
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500) {
        int size = udp.parsePacket();
        if (size >= NTP_PACKET_SIZE) {
            Serial.println("[NTP] Receiving data...");
            udp.read(packetBuffer, NTP_PACKET_SIZE);
            
            // Convert four bytes starting at location 40 to a long integer
            unsigned long secsSince1900;
            secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
            secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
            secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
            secsSince1900 |= (unsigned long)packetBuffer[43];
            
            return secsSince1900 - 2208988800UL;
        }
    }
    
    Serial.println("[NTP] Server not responding...");
    return 0;
}

void TimeManager::sendNTPpacket(IPAddress& address) {
    const int NTP_PACKET_SIZE = 48;
    byte packetBuffer[NTP_PACKET_SIZE];
    
    // Set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    
    // Initialize values needed to form NTP request
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;            // Stratum, or type of clock
    packetBuffer[2] = 6;            // Polling Interval
    packetBuffer[3] = 0xEC;         // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;
    
    // Send NTP request
    udp.beginPacket(address, 123); // NTP requests are to port 123
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
}

void TimeManager::update() {
    // TimeLib handles automatic sync based on setSyncInterval
    // Just call now() to let TimeLib do its work
    now();
}

int TimeManager::getHour() {
    return hour();
}

int TimeManager::getMinute() {
    return minute();
}

int TimeManager::getSecond() {
    return second();
}
int TimeManager::getWeekday() {
    return weekday();
}

int TimeManager::getDay() {
    return day();
}
int TimeManager::getMonth() {
    return month();
}
int TimeManager::getYear() {
    return year();
} 

bool TimeManager::isTimeSynced() {
    return timeStatus() != timeNotSet;
}

int TimeManager::getFailedAttempts() {
    return failedAttempts;
}
