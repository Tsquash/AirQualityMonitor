#include <Arduino.h>
#include <SPI.h>
#include "screen.h"
#include "sense.h" 
#include "utils.h"

#include <GxEPD2_BW.h>

#include "myFont/RubikItalic49pt7b.h"
#include "myFont/RubikItalic64pt7b.h"
#include "myFont/RubikRegular12pt7b.h"
#include "myFont/RubikRegular9pt7b.h"
#include "myFont/RubikMedium12pt7b.h"
#include "myFont/RubikRegular6pt7b.h"
#include "myFont/RubikItalic9pt7b.h"
#include "myFont/RubikRegular15pt7b.h"
#include "myFont/RubikRegular17pt7b.h"
#include "myFont/RubikRegular18pt7b.h"
#include "myBitmap/bitmaps.h"

#define GxEPD2_DISPLAY_CLASS GxEPD2_BW
#define GxEPD2_DRIVER_CLASS GxEPD2_370_GDEY037T03 // GDEY037T03 240x416, UC8253
#define EPD_DC 21
#define EPD_CS 17
#define EPD_SCK   19
#define EPD_MOSI  18
#define EPD_MISO -1 // not reading from screen
#define EPD_BUSY -1 // can set to -1 to not use a pin (will wait a fixed delay)
#define SRAM_CS -1 // use internal RAM
#define EPD_RESET 20  // can set to -1 and share with microcontroller Reset!
#define EPD_SPI &epd_spi // primary SPI

#define MINUTES_PER_FULL 30

// flag so that change page knows the current page
bool page1 = true;
bool forceFull = false;

// use XIAO C6 SPI bus
SPIClass epd_spi(FSPI);
// GxEPD2 Definition: 
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))
#define MAX_DISPLAY_BUFFER_SIZE 65536ul
GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=*/ EPD_CS, /*DC=*/ EPD_DC, /*RST=*/ EPD_RESET, /*BUSY=*/ EPD_BUSY));

DataQueue vocQueue;
DataQueue co2Queue;
DataQueue noxQueue;

String DoWs[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
}; 

String padStart(String str) {
  while (str.length() < 2) {
    str = '0' + str;
  }
  return str;
}

void changePage()
{
    page1 = !page1;
    forceFull = true;
    page1 ? drawPage1() : drawPage2();
}

void initializeScreen(){ 
    epd_spi.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
    display.epd2.selectSPI(epd_spi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    display.init(115200, true, 2, false);
    display.setRotation(3);
    display.fillRect(0,0,416,240, GxEPD_WHITE);
    display.display();
}

void initializeQueues() {
    for(int i = 0; i < 60; i++) {
        vocQueue.push(0);
        co2Queue.push(0);
        noxQueue.push(0);
    }
}

// example: drawGraphSkeleton("CO2", getCO2(), 1); 
// where 1 tells it to occupy the first graph space
void drawGraphSkeleton(String title, int value, uint8_t pos){
    // vert line for UI
    display.drawRect(10,36+((pos-1)*71),2,37,GxEPD_BLACK);
    // title next to vert line
    display.setFont(&Rubik_Regular9pt7b);
    display.setCursor(15,49+((pos-1)*71));
    display.print(title);
    // print the current values next to the labels
    display.setCursor(15,70+((pos-1)*71));
    display.print(value);
    // draw the graph axis
    display.drawRect(104,29+((pos-1)*71), 2, 48, GxEPD_BLACK); 
    display.drawBitmap(104, 77+((pos-1)*71), xAxisBitmap, XAXIS_WIDTH, XAXIS_HEIGHT, GxEPD_BLACK);
    
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
    
    // Determine scaling: use default max for normal ranges, adjust if exceeded
    int defaultMax = (pos == 1) ? 1500 : (pos == 2) ? 200 : 2;
    int minVal = (pos == 1) ? 0 : 1;
    int actualMax = INT_MIN;
    for(int i = 0; i < q.size(); i++) {
        int val = q.get(i);
        if(val > actualMax) actualMax = val;
    }
    int maxVal = max(defaultMax, actualMax);
    
    // print min and max on y-axis
    display.setFont(&Rubik_Regular6pt7b);
    String maxStr = String(maxVal);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(maxStr, 0, 0, &x1, &y1, &w, &h);
    int maxY = 39 + ((pos-1)*71);
    display.setCursor(101 - w, maxY);
    display.print(maxStr);
    String minStr = String(minVal);
    display.getTextBounds(minStr, 0, 0, &x1, &y1, &w, &h);
    int minY = 78 + ((pos-1)*71);
    display.setCursor(101 - w, minY);
    display.print(minStr);
    
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
        int constrainedVal = constrain(rawVal, minVal, maxVal);
        // 2. Map to pixel height (0 to 48)
        int pixelHeight = map(constrainedVal, minVal, maxVal, 0, GRAPH_HEIGHT);
        // 3. Invert because screen Y=0 is top, Y=240 is bottom
        int currentY = baseY - pixelHeight;
        
        // Calculate X Coordinate
        int currentX = startX + (i * 5);

        // Draw Line from previous point (if not the first point)
        if (prevX != -1) {
            // Draw main line
            display.drawLine(prevX, prevY, currentX, currentY, GxEPD_BLACK);
            // Draw adjacent line to simulate Width = 2
            // We offset Y by 1 for a thicker look
            // display.drawLine(prevX, prevY + 1, currentX, currentY + 1, GxEPD_BLACK);
        }

        // Draw data point
        display.fillRect(currentX, currentY, 1, 1, GxEPD_BLACK);

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
    display.clearScreen(); 
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&Rubik_Regular12pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(50, 50);
    display.print(message);
    display.display();
    */
}

void drawStartup() {
    page1 = false;
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);

    display.setTextColor(GxEPD_BLACK);
    display.setFont(&Rubik_Regular18pt7b);
    display.setCursor(21,46);
    display.printf("Air Quality Monitor");
    display.fillRect(21, 55, 307, 2, GxEPD_BLACK);
    display.setFont(&Rubik_Regular15pt7b);
    display.setCursor(29, 96);
    display.printf("Starting up...");
    display.setCursor(29, 152);
    display.printf("Allow 1 hour after start up");
    display.setCursor(29, 188);
    display.printf("for readings to normalize");
    display.display(true);
}

void drawCommission(){
    page1 = false;
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);

    display.setTextColor(GxEPD_BLACK);
    display.setFont(&Rubik_Regular15pt7b);
    display.setCursor(21,46);
    display.printf("Set up your matter device");
    display.fillRect(21, 55, 351, 2, GxEPD_BLACK);
    display.drawBitmap(153, 68, matterQRBitmap, MATTERQR_WIDTH, MATTERQR_HEIGHT, GxEPD_BLACK);
    display.setFont(&Rubik_Regular9pt7b);
    display.setCursor(19, 200);
    display.printf("Scan the QR code or use manual pairing code");
    display.setCursor(145,221);
    display.printf("3497-011-2332");
    display.display(true);
}

void drawPage1() {
    page1 = true;
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    int16_t x1, y1; uint16_t w, h;
    int16_t x2, y2; uint16_t w2, h2;
    // Date
    display.setFont(&Rubik_Regular17pt7b);
    String dateText = DoWs[getRTCcal().day - 1] + " " + padStart(String(getRTCcal().month)) + "/" + padStart(String(getRTCcal().date));
    display.getTextBounds(dateText, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(((416 - w) / 2) - x1, 49);
    display.print(dateText);
    // Time
    display.setFont(&Rubik_Italic64pt7b);
    String timeStr = String(getRTCTime().hours) + ":" + padStart(String(getRTCTime().minutes));
    display.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((416 - w) / 2, 153);
    display.printf("%d:%02d", getRTCTime().hours, getRTCTime().minutes);
    // Temp, humidity, CO2 cluster laid out from a centered left edge.
    const int tempIconY = 171;
    const int humidtyIconY = 171; 
    const int co2IconY = 178;
    const int vocIconY = 204;
    const int noxIconY = 205;
    const int aqIconY = 203;
    const int textBaselineY = 190;
    const int text2BaselineY = 221;
    const int padAfterIcon = 6;
    const int padBetweenGroups = 12;

    String tempText = String(TEMP) + "\x7F"; // for degree symbol
    String humidityText = String(RH) + "%";
    String co2Text = String(CO2);

    uint16_t tempTextW, humidityTextW, co2TextW, ppmTextW;
    display.setFont(&Rubik_Regular12pt7b);
    display.getTextBounds(tempText, 0, 0, &x1, &y1, &tempTextW, &h);
    display.getTextBounds(humidityText, 0, 0, &x1, &y1, &humidityTextW, &h);
    display.getTextBounds(co2Text, 0, 0, &x1, &y1, &co2TextW, &h);
    display.setFont(&Rubik_Regular9pt7b);
    display.getTextBounds("ppm", 0, 0, &x2, &y2, &ppmTextW, &h2);

    int clusterWidth =
        TEMP_WIDTH + padAfterIcon + tempTextW +
        padBetweenGroups + HUMIDITY_WIDTH + padAfterIcon + humidityTextW +
        padBetweenGroups + CO2_WIDTH + padAfterIcon + co2TextW +
        padAfterIcon + ppmTextW;

    int cursorX = (416 - clusterWidth) / 2;

    display.drawBitmap(cursorX, tempIconY, tempBitmap, TEMP_WIDTH, TEMP_HEIGHT, GxEPD_BLACK);
    cursorX += TEMP_WIDTH + padAfterIcon;
    display.setFont(&Rubik_Regular12pt7b);
    display.setCursor(cursorX, textBaselineY);
    display.print(tempText);
    cursorX += tempTextW + padBetweenGroups;

    display.drawBitmap(cursorX, humidtyIconY, humidityBitmap, HUMIDITY_WIDTH, HUMIDITY_HEIGHT, GxEPD_BLACK);
    cursorX += HUMIDITY_WIDTH + padAfterIcon;
    display.setCursor(cursorX, textBaselineY);
    display.print(humidityText);
    cursorX += humidityTextW + padBetweenGroups;

    display.drawBitmap(cursorX, co2IconY, co2Bitmap, CO2_WIDTH, CO2_HEIGHT, GxEPD_BLACK);
    cursorX += CO2_WIDTH + padAfterIcon;
    display.setCursor(cursorX, textBaselineY);
    display.print(co2Text);
    cursorX += co2TextW + padAfterIcon;

    display.setFont(&Rubik_Regular9pt7b);
    display.setCursor(cursorX, textBaselineY);
    display.print("ppm");

    String vocText = String(VOC);
    String noxText = String(NOx);

    // Mirror the Matter AQ threshold logic, but keep display code independent of Matter enums.
    auto determineLevel = [](float value, float t0, float t1, float t2, float t3, float t4) -> uint8_t {
        if (value <= t0) return 0; // Good
        if (value <= t1) return 1; // Fair
        if (value <= t2) return 2; // Moderate
        if (value <= t3) return 3; // Poor
        if (value <= t4) return 4; // Very Poor
        return 5;                  // Extremely Poor
    };

    uint8_t co2Quality = determineLevel(static_cast<float>(CO2), 800, 1200, 1500, 2000, 3000);
    uint8_t vocQuality = determineLevel(static_cast<float>(VOC), 100, 150, 200, 300, 400);
    uint8_t noxQuality = determineLevel(static_cast<float>(NOx), 1, 5, 20, 100, 250);

    uint8_t overallQuality = std::max({ co2Quality, vocQuality, noxQuality });

    const char* qualityLabels[] = {
        "Good", "Fair", "Moderate", "Poor", "Very Poor", "Bad"
    };
    String aqText = qualityLabels[overallQuality];
    
    uint16_t vocTextW, noxTextW, aqTextW;
    display.setFont(&Rubik_Regular12pt7b);
    display.getTextBounds(vocText, 0, 0, &x1, &y1, &vocTextW, &h);
    display.getTextBounds(noxText, 0, 0, &x1, &y1, &noxTextW, &h);
    display.getTextBounds(aqText, 0, 0, &x1, &y1, &aqTextW, &h);

    clusterWidth =
        VOC_WIDTH + padAfterIcon + vocTextW +
        padBetweenGroups + NOX_WIDTH + padAfterIcon + noxTextW +
        padBetweenGroups + AQ_WIDTH + padAfterIcon + aqTextW;

    cursorX = (416 - clusterWidth) / 2;

    display.drawBitmap(cursorX, vocIconY, vocBitmap, VOC_WIDTH, VOC_HEIGHT, GxEPD_BLACK);
    cursorX += VOC_WIDTH + padAfterIcon;
    display.setCursor(cursorX, text2BaselineY);
    display.print(vocText);
    cursorX += vocTextW + padBetweenGroups;

    display.drawBitmap(cursorX, noxIconY, noxBitmap, NOX_WIDTH, NOX_HEIGHT, GxEPD_BLACK);
    cursorX += NOX_WIDTH + padAfterIcon;
    display.setCursor(cursorX, text2BaselineY);
    display.print(noxText);
    cursorX += noxTextW + padBetweenGroups;

    display.drawBitmap(cursorX, aqIconY, aqBitmap, AQ_WIDTH, AQ_HEIGHT, GxEPD_BLACK);
    cursorX += AQ_WIDTH + padAfterIcon;
    display.setCursor(cursorX, text2BaselineY);
    display.print(aqText);
    cursorX += aqTextW + padAfterIcon;

    bool doFull = forceFull || (getRTCTime().minutes == 0) || (getRTCTime().minutes == 30);
    forceFull = false;
    display.display(!doFull);
    if (doFull) {
        display.display(true);
    }
}

void drawPage2(){
    page1 = false;
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    // line across top seperating graphs from time/date
    display.drawRect(0,20,416,2,GxEPD_BLACK); 
    display.setFont(&Rubik_Italic9pt7b);
    display.setCursor(4,16);
    display.printf("%02d/%02d  %d:%02d %s", getRTCcal().month, getRTCcal().date, getRTCTime().hours, getRTCTime().minutes, getRTCTime().am_pm ? "PM" : "AM");
    String tempUnit = "F";
    String tempHumStr = String(TEMP) + tempUnit + "  " + RH + "%";
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(tempHumStr, 0, 0, &x1, &y1, &w, &h);
    // want the rightmost pixel to be at 412 (416-4)
    display.setCursor(413-w, 16);
    display.printf("%d%s  %d%%", TEMP, tempUnit, RH);
    // three empty graphs
    drawGraphSkeleton("CO2", CO2, 1); 
    drawGraphSkeleton("VOC", VOC, 2); 
    drawGraphSkeleton("NOx", NOx, 3); 
    // populate the graphs with points
    drawGraphPoints(co2Queue, 1); 
    drawGraphPoints(vocQueue, 2); 
    drawGraphPoints(noxQueue, 3);
    bool doFull = forceFull || (getRTCTime().minutes == 0) || (getRTCTime().minutes == 30);
    forceFull = false;
    display.display(!doFull);
    if (doFull) {
        display.display(true);
    }
}