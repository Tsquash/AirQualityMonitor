#ifndef BUTTONS_H
#define BUTTONS_H

#define BTN1 1
#define BTN2 0

extern volatile bool pageChangeRequested;

void setupButtons();
bool checkBootHold(int pin, unsigned long holdTime);
#endif