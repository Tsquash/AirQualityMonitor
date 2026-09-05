#pragma once
#include "pm_protocol.h"

extern PMReading remotePM;
void initializePMLink(bool pairAtBoot);
void processPMLink(bool rtcReady = true);
void finishPMPairing();
bool pmLinkPaired();
void clearPMLink();
void finishPMMinute();
