#include <Arduino.h>
#include <SPI.h>
#include "screen.h"
#include "sense.h" 
#include "battery.h"
#include "utils.h"

#include "Adafruit_ThinkInk.h"

#define EPD_DC 21
#define EPD_CS 17
#define EPD_SCK   19
#define EPD_MOSI  18
#define EPD_MISO -1 // not reading from screen
#define EPD_BUSY -1 // can set to -1 to not use a pin (will wait a fixed delay)
#define SRAM_CS -1 // use internal RAM
#define EPD_RESET 20  // can set to -1 and share with microcontroller Reset!
#define EPD_SPI &epd_spi // primary SPI

// use XIAO C6 SPI bus
SPIClass epd_spi(FSPI);
// 3.7" Tricolor Display with 420x240 pixels and UC8253 chipset
ThinkInk_370_Tricolor_BABMFGNR display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY, EPD_SPI);

void initializeScreen(){ 
    epd_spi.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
    display.begin(THINKINK_TRICOLOR);
}

void screenPrint(String message){
    Serial.printf("[SCREEN] %s", message);
    display.clearDisplay();
    display.clearBuffer();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.setTextColor(EPD_BLACK);
    display.print(message);
    display.display();
}

void displayAP(uint8_t* mac) {
    display.clearDisplay();
    display.clearBuffer();
    display.setTextSize(3);
    display.setCursor(0, 10);
    display.setTextColor(EPD_BLACK);
    display.printf("Config Mode:\n");
    display.setTextSize(2);
    display.printf("Connect to WiFi SSID:\n");
    display.setTextColor(EPD_RED);
    display.printf("AirQuality-%s\n", macLastThreeSegments(mac).c_str());
    display.setTextColor(EPD_BLACK);
    display.printf("to configure this sensor.");
    Serial.printf("[SCREEN] AP MODE AirQuality-%06X ", macLastThreeSegments(mac));
    display.display();
}

void screenTest() {
    display.clearDisplay();
    display.clearBuffer();
    display.setTextSize(2);
    display.setCursor(0, 10);
    display.setTextColor(EPD_BLACK);
    display.printf("Battery: ");
    display.setTextColor(EPD_RED);
    display.printf("%d%%\n", getBatteryPercentage());
    display.setTextColor(EPD_BLACK);
    display.printf("Temperature: ");
    display.setTextColor(EPD_RED);
    // TODO change unit depending on config
    display.printf("%.1fF\n", getTemp());
    display.setTextColor(EPD_BLACK);
    display.printf("Humidity: ");
    display.setTextColor(EPD_RED);
    display.printf("%.1f%%\n", getHumidity());
    display.setTextColor(EPD_BLACK);
    display.printf("Time: ");
    display.setTextColor(EPD_RED);
    display.printf("%02d:%02d\n", getRTCTime().hours, getRTCTime().minutes);
    display.display();
    display.setTextColor(EPD_BLACK);
    display.printf("Date: ");
    display.setTextColor(EPD_RED);
    display.printf("%02d/%02d/%02d\n", getRTCdate().month, getRTCdate().day, getRTCdate().year);
    display.display();
}