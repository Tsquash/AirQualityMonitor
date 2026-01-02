#ifndef BUTTONS_H
#define BUTTONS_H

#include "InterruptDrivenButton.h"

#define BTN1 1
#define BTN2 16 

void setupButtons();
bool checkBootHold(int pin, unsigned long holdTime);
// TODO: if buttons are not used besides boot, remove library
#endif