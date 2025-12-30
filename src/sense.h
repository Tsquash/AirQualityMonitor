#ifndef SENSE_H
#define SENSE_H

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

#endif