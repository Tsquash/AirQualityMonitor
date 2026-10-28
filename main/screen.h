#ifndef SCREEN_H
#define SCREEN_H

#include "utils.h"

extern bool page1;
extern DataQueue vocQueue;
extern DataQueue co2Queue;
extern DataQueue noxQueue;

enum class PairingDisplayStatus {
    Pairing,
    Paired,
    TimedOut,
};

void initializeScreen();
void drawCenteredMessage(const char* message);
void drawPairingStatus(PairingDisplayStatus status);
void changePage();
void drawPage1();
void drawPage2();
void drawCommission();
void drawStartup();
#endif
