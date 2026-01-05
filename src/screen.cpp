#include <Arduino.h>
#include <SPI.h>
#include "screen.h"
#include "sense.h" 
#include "battery.h"
#include "utils.h"

#include <Adafruit_GFX.h>
#include "Adafruit_ThinkInk.h"

#include "myFont/RubikItalic49pt7b.h"
#include "myFont/RubikRegular12pt7b.h"
#include "myFont/RubikRegular9pt7b.h"
#include "myFont/RubikMedium12pt7b.h"
#include "myFont/RubikRegular6pt7b.h"
#include "myBitmap/bitmaps.h"

#define EPD_DC 21
#define EPD_CS 17
#define EPD_SCK   19
#define EPD_MOSI  18
#define EPD_MISO -1 // not reading from screen
#define EPD_BUSY -1 // can set to -1 to not use a pin (will wait a fixed delay)
#define SRAM_CS -1 // use internal RAM
#define EPD_RESET 20  // can set to -1 and share with microcontroller Reset!
#define EPD_SPI &epd_spi // primary SPI

String DoWs[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

// use XIAO C6 SPI bus
SPIClass epd_spi(FSPI);
// 3.7" Tricolor Display with 416x240 pixels and UC8253 chipset
ThinkInk_370_Tricolor_BABMFGNR display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY, EPD_SPI);


String padStart(String str) {
  while (str.length() < 2) {
    str = '0' + str;
  }
  return str;
}

void initializeScreen(){ 
    epd_spi.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
    display.begin(THINKINK_TRICOLOR);
}

void screenPrint(String message){
    Serial.printf("[SCREEN] %s", message);
    display.clearDisplay();
    display.clearBuffer();
    display.setFont(&Rubik_Regular12pt7b);
    display.setTextColor(EPD_BLACK);
    display.setCursor(0, 0);
    display.print(message);
    display.display();
}

void displayAP(uint8_t* mac) {
    display.clearDisplay();
    display.clearBuffer();
    display.setFont(&Rubik_Medium12pt7b);
    display.setTextColor(EPD_BLACK);
    display.setCursor(0, 10);
    display.printf("Config Mode:\n");
    display.setFont(&Rubik_Regular12pt7b);
    display.printf("Connect to WiFi SSID:\n");
    display.printf("AirQuality-%s\n", macLastThreeSegments(mac).c_str());
    display.printf("to configure the device.");
    Serial.printf("[SCREEN] AP MODE AirQuality-%06X ", macLastThreeSegments(mac));
    display.display();
}

void drawPage1() {
    display.clearDisplay();
    display.clearBuffer();
    
    // dimension test
    // display.drawRect(0,0,416,240, EPD_BLACK);
    
    display.setTextColor(EPD_BLACK);
    int16_t x1, y1; uint16_t w, h;
    // Date
    display.setFont(&Rubik_Regular12pt7b);
    String dateStr = DoWs[getRTCcal().day-1] + "  " + (int)getRTCcal().month + "/" + (int)getRTCcal().date;
    display.getTextBounds(dateStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((416 - w) / 2, 54);
    display.printf("%s %02d/%02d", DoWs[getRTCcal().day-1], getRTCcal().month, getRTCcal().date);
    // Time
    display.setFont(&Rubik_Italic49pt7b);
    String timeStr = String(getRTCTime().hours) + ":" + padStart(String(getRTCTime().minutes));
    display.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((416 - w) / 2, 133);
    display.printf("%d:%02d", getRTCTime().hours, getRTCTime().minutes);
    // Tem, Humidity, CO2
    display.setFont(&Rubik_Regular12pt7b);
    String tempStr = String((int)round(getTemp())) + "\x7F  " + (int)round(getHumidity()) + "%  342 ppm";
    display.getTextBounds(tempStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(((416 - w) / 2), 161); // the minus 4 accounts for the width of the degree symbol
    display.printf("%d\x7F  %d%%  %d ppm\n", (int)round(getTemp()), (int)round(getHumidity()), 342); // TODO: C02 Hardcoded, document degree is x7F
    // VOC Scale
    display.drawRoundRect(111, 175, 194, 16, 1, EPD_BLACK);
    // TODO: greyscale dither scale
    int VOC = 75;
    int smaller_index = ((VOC - 1.0) / (500 - 1.0)) * (187 - 1) + 1; // map 1-500 to 1-187
    display.drawBitmap(109+smaller_index, 190, lower_index_pointer, L_POINTER_WIDTH, L_POINTER_HEIGHT, EPD_BLACK);
    display.drawBitmap(112+smaller_index, 187, upper_index_pointer, U_POINTER_WIDTH, U_POINTER_HEIGHT, EPD_BLACK);
    String quality = VOC<=100 ? "Good" : VOC<=250 ? "Fair" : "Poor";
    display.setCursor(121+smaller_index, 200);
    display.setFont(&Rubik_Regular6pt7b);
    display.printf("%d - %s", VOC, quality);
    // TODO: update 'true' to be when any VOC, NOx, or CO2 is too high
    if(true){
        display.drawBitmap(85, 210, cautionBitmap, CAUITON_WIDTH, CAUTION_HEIGHT, EPD_BLACK);
        display.setCursor(116, 228);
        display.setFont(&Rubik_Regular9pt7b);
        // TODO: update "VOC" to be whatever caused the notification
        display.printf("High %s Levels Detected", "VOC");
        // TODO: set flag that there is a notification (time based?)
    }
    
    display.display();
}

void drawPage2(){

}