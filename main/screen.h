#ifndef SCREEN_H
#define SCREEN_H

#include "utils.h"

extern bool page1;
extern DataQueue vocQueue;
extern DataQueue co2Queue;
extern DataQueue noxQueue;

void initializeScreen();
void initializeQueues();
void screenPrint(String message);
void displayAP(uint8_t* mac);
void changePage();
void refreshDisplay(bool forceFull);
void drawPage1();
void drawPage2();
void drawStartup();
/*
bool updateMinute(); // updates the minute value and sets air quality vars
void updateHour(); // called by update minute
void updateDay(); // called by update hour
*/
#endif