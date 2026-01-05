#ifndef BUTTONS_H
#define BUTTONS_H

#define BTN1 1
#define BTN2 16 

void setupButtons();
bool checkBootHold(int pin, unsigned long holdTime);

#endif