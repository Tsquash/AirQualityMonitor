#include <Arduino.h>
#include <SPI.h>
#include "screen.h"
#include "sense.h" 
#include "battery.h"

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

void screenTest() {
    display.clearDisplay();
    display.clearBuffer();
    display.setTextSize(2);
    display.setCursor(10, 10);
    display.setTextColor(EPD_BLACK);
    display.printf("Battery: ");
    display.setTextColor(EPD_RED);
    display.printf("%d%%\n", getBatteryPercentage());
    display.setTextColor(EPD_BLACK);
    display.printf("Temperature: ");
    display.setTextColor(EPD_RED);
    display.printf("%.1f C\n", getTemp());
    display.setTextColor(EPD_BLACK);
    display.printf("Humidity: ");
    display.setTextColor(EPD_RED);
    display.printf("%.1f %%\n", getHumidity());
    display.display();
}