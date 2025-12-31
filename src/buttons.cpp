#include <Arduino.h>
#include "buttons.h"

#define BTN1 1
#define BTN2 16 

InterruptDrivenButton btn1(BTN1);
InterruptDrivenButton btn2(BTN2);

DEFINE_IDB_ISR(btn1) 
DEFINE_IDB_ISR(btn2)

void setupButtons(){
    btn1.setup(IDB_ISR(btn1)); 
    btn2.setup(IDB_ISR(btn2)); 

    unsigned long startCheck = millis();
    while (millis() - startCheck < 2000) { // Check for 2 seconds
        btn1.loop(); 
        btn2.loop();
    }
}

bool btn1Boot(){ 
    return btn1.pollEvent().type == IDB_EVENT_BOOT_HOLD;
}

bool btn2Boot(){
    return btn2.pollEvent().type == IDB_EVENT_BOOT_HOLD;
}