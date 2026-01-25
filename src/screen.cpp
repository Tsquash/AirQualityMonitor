#include <Arduino.h>
#include <SPI.h>
#include "screen.h"
#include "sense.h" 
#include "utils.h"

#include <Adafruit_GFX.h>
#include "Adafruit_ThinkInk.h"

#include "myFont/RubikItalic49pt7b.h"
#include "myFont/RubikRegular12pt7b.h"
#include "myFont/RubikRegular9pt7b.h"
#include "myFont/RubikMedium12pt7b.h"
#include "myFont/RubikRegular6pt7b.h"
#include "myFont/RubikItalic9pt7b.h"
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

// flag so that change page knows the current page
bool page1 = false;

// use XIAO C6 SPI bus
SPIClass epd_spi(FSPI);
// 3.7" Tricolor Display with 416x240 pixels and UC8253 chipset
// ThinkInk_370_Tricolor_BABMFGNR display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY, EPD_SPI);
// 3.7" Mono display
ThinkInk_370_Mono_BAAMFGN display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY, EPD_SPI);

String padStart(String str) {
  while (str.length() < 2) {
    str = '0' + str;
  }
  return str;
}

void initializeScreen(){ 
    epd_spi.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
    // display.begin(THINKINK_TRICOLOR);
    display.begin(THINKINK_MONO);
}

// example: drawGraphSkeleton("CO2", getCO2(), 1); 
// where 1 tells it to occupy the first graph space
void drawGraphSkeleton(String title, int value, uint8_t pos){
    // vert line for UI
    display.drawRect(10,36+((pos-1)*71),2,37,EPD_BLACK);
    // title next to vert line
    display.setFont(&Rubik_Regular9pt7b);
    display.setCursor(15,49+((pos-1)*71));
    display.print(title);
    // print the current values next to the labels
    display.setCursor(15,70+((pos-1)*71));
    display.print(value);
    // draw the graph axis
    display.drawRect(104,29+((pos-1)*71), 2, 48, EPD_BLACK); 
    display.drawBitmap(104, 77+((pos-1)*71), xAxisBitmap, XAXIS_WIDTH, XAXIS_HEIGHT, EPD_BLACK);
    
}

// position is just which one of the three graphs on the board you draw the data points onto. can be 1-3, each 71 y values apart. 
// allow CO2 values (pos 1) to be 1-2000, anything above is just 2000. Need to be mapped to Y values 1-50
// allow VOC, NOX values (pos 2&3) to be 1-500. Need to be mapped to Y values 1-50
// get size of queue that was passed in. the first value you deque will be at 402 minus 5*size? 
// deque and draw pixel until the queue is empty and you are drawing your last pixel at x=402. 
// data point queue size will be constrained to 60 since axis is 300 pixels, one 2x2 pixel per 5 x pixels.
// y value of a data value of 1 would be 77+((pos-1)*71) 
// draw a line (width 2) from the previous data point to the current data point
void drawGraphPoints(DataQueue q, uint8_t pos) {
    if (q.size() == 0) return;

    // --- 1. CONFIGURATION ---
    // Base Y is the bottom line of the graph. 
    // From skeleton: 77 + ((pos-1)*71).
    int baseY = 77 + ((pos - 1) * 71);
    
    // Max height for the graph plot (keeping inside the box)
    const int GRAPH_HEIGHT = 48; 
    
    // Determine scaling based on position (1=CO2, 2&3=VOC/NOx)
    int maxVal = (pos == 1) ? 2000 : 500;
    
    // Calculate starting X pixel so the newest data point always hits x=402
    // Space between points is 5px.
    int startX = 402 - (5 * (q.size() - 1));

    int16_t prevX = -1;
    int16_t prevY = -1;

    // --- 2. DRAWING LOOP ---
    for (int i = 0; i < q.size(); i++) {
        // Get value (0 is oldest)
        int rawVal = q.get(i);
        
        // Map Value to Y Coordinate
        // 1. Constrain value to limits
        int constrainedVal = constrain(rawVal, 0, maxVal);
        // 2. Map to pixel height (0 to 48)
        int pixelHeight = map(constrainedVal, 0, maxVal, 0, GRAPH_HEIGHT);
        // 3. Invert because screen Y=0 is top, Y=240 is bottom
        int currentY = baseY - pixelHeight;
        
        // Calculate X Coordinate
        int currentX = startX + (i * 5);

        // Draw Line from previous point (if not the first point)
        if (prevX != -1) {
            // Draw main line
            display.drawLine(prevX, prevY, currentX, currentY, EPD_BLACK);
            // Draw adjacent line to simulate Width = 2
            // We offset Y by 1 for a thicker look
            // display.drawLine(prevX, prevY + 1, currentX, currentY + 1, EPD_BLACK);
        }

        // Draw data point
        display.fillRect(currentX, currentY, 1, 1, EPD_BLACK);

        // Save current as previous for next loop
        prevX = currentX;
        prevY = currentY;
    }
}

void screenPrint(String message){
    page1 = false;
    // TODO: Test this functionality
    /*
    Serial.printf("[SCREEN] test");
    display.clearDisplay();
    display.clearBuffer();
    display.setFont(&Rubik_Regular12pt7b);
    display.setTextColor(EPD_BLACK);
    display.setCursor(50, 50);
    display.print(message);
    display.display();
    */
}

// TODO: display AP is sometimes called after an error. this error message should be able to be displayed below the AP info 
void displayAP(uint8_t* mac) {
    page1 = false;
    display.clearDisplay();
    display.clearBuffer();
    display.setFont(&Rubik_Medium12pt7b);
    display.setTextColor(EPD_BLACK);
    display.setCursor(0, 20);
    display.printf("Config Mode:\n");
    display.setFont(&Rubik_Regular12pt7b);
    display.printf("Connect to WiFi SSID:\n");
    display.printf("AirQuality-%s\n", macLastThreeSegments(mac).c_str());
    display.printf("to configure the device.");
    Serial.printf("[SCREEN] AP MODE AirQuality-%06X ", macLastThreeSegments(mac));
    display.display();
}

void changePage(){
    Serial.printf("changePage called, page1: %d\n", page1);
    if(page1) drawPage2();
    else drawPage1();
}

void drawPage1() {
    page1 = true;
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
    String tempStr = String((int)round(getTemp())) + "\x7F  " + (int)round(getHumidity()) + "%  " + getCO2() + " ppm";
    display.getTextBounds(tempStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(((416 - w) / 2), 161); // the minus 4 accounts for the width of the degree symbol
    display.printf("%d\x7F  %d%%  %d ppm\n", (int)round(getTemp()), (int)round(getHumidity()), getCO2()); // TODO: CO2 Hardcoded, document degree is x7F
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

DataQueue vocQueue; 
DataQueue co2Queue;
DataQueue noxQueue;

// TODO: Remove
void seedTestQueue(DataQueue &q, int startVal, int maxChange) {
    int currentVal = startVal;
    
    // Fill the max buffer size (60)
    for(int i = 0; i < 60; i++) {
        // Randomly wander up or down by a small amount
        // random(min, max) is inclusive of min but exclusive of max
        int drift = random(-maxChange, maxChange + 1); 
        
        currentVal += drift;

        // Clamp values so they don't go off the chart (VOC max is 500)
        if(currentVal < 0) currentVal = 0;
        if(currentVal > 500) currentVal = 500;

        q.push(currentVal);
    }
}

void drawPage2(){
    page1 = false;
    randomSeed(analogRead(0)); 
    seedTestQueue(vocQueue, 120, 20); 
    seedTestQueue(co2Queue, 800, 50);
    seedTestQueue(noxQueue, 1, 5);

    display.clearDisplay();
    display.clearBuffer();
    display.setTextColor(EPD_BLACK);
    // line across top seperating graphs from time/date
    display.drawRect(0,20,416,2,EPD_BLACK); 
    display.setFont(&Rubik_Italic9pt7b);
    display.setCursor(4,16);
    display.printf("%02d/%02d  %d:%02d %s", getRTCcal().month, getRTCcal().date, getRTCTime().hours, getRTCTime().minutes, getRTCTime().am_pm ? "PM" : "AM");
    String tempUnit = json["unit_c"].as<int>() == 1 ? "C" : "F";
    String tempHumStr = String((int)round(getTemp())) + tempUnit + "  " + (int)round(getHumidity()) + "%";
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(tempHumStr, 0, 0, &x1, &y1, &w, &h);
    // want the rightmost pixel to be at 412 (416-4)
    display.setCursor(413-w, 16);
    display.printf("%d%s  %d%%", (int)round(getTemp()), tempUnit, (int)round(getHumidity()));
    // three empty graphs
    drawGraphSkeleton("CO2", 632, 1); 
    drawGraphSkeleton("VOC", 78, 2); 
    drawGraphSkeleton("NOx", 1, 3); 
    // populate the graphs with points
    drawGraphPoints(co2Queue, 1); 
    drawGraphPoints(vocQueue, 2); 
    drawGraphPoints(noxQueue, 3);
    display.display();
}