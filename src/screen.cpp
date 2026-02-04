#include <Arduino.h>
#include <SPI.h>
#include "screen.h"
#include "sense.h" 
#include "utils.h"

#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include "Adafruit_ThinkInk.h"

#include "myFont/RubikItalic49pt7b.h"
#include "myFont/RubikRegular12pt7b.h"
#include "myFont/RubikRegular9pt7b.h"
#include "myFont/RubikMedium12pt7b.h"
#include "myFont/RubikRegular6pt7b.h"
#include "myFont/RubikItalic9pt7b.h"
#include "myFont/RubikRegular15pt7b.h"
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

#define MINUTES_PER_FULL 10

// flag so that change page knows the current page
bool page1 = false;
int refreshCounter = 1;

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

void drawCurrentPage()
{
    page1 ? drawPage1() : drawPage2();
}

void changePage()
{
    page1 = !page1;
    refreshCounter = 0;
    refreshDisplay(true); // force full
}

void refreshDisplay(bool forceFull)
{
    bool doFull = forceFull || (refreshCounter % MINUTES_PER_FULL == 0);

    if (doFull)
    {
        display.setFullWindow();
        drawCurrentPage();
        display.display(false);   // full refresh
        // display.writeScreenBufferAgain(); // not avail on UC8253
        // IMPORTANT: reset partial state
        //display.setPartialWindow(0, 0, 416, 240);
        display.setFullWindow();
        refreshCounter = 1; // next update is partial
    }
    else
    {
        //display.setPartialWindow(0, 0, 416, 240);
        display.setFullWindow();
        drawCurrentPage();
        display.display(true);    // fast partial
        refreshCounter++;
    }
}

void initializeScreen(){ 
    epd_spi.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
    display.epd2.selectSPI(epd_spi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    display.init(115200, true, 2, false);
    delay(100);
    display.setRotation(3);
    display.clearScreen();
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

// TODO: display AP is sometimes called after an error. this error message should be able to be displayed below the AP info 
void displayAP(uint8_t* mac) {
    page1 = false;
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&Rubik_Regular18pt7b);
    display.setCursor(21,46);
    display.printf("Config Mode");
    display.fillRect(21, 56, 204, 2, GxEPD_BLACK);
    display.setFont(&Rubik_Regular15pt7b);
    display.setCursor(29, 96);
    display.printf("Connect to WiFi SSID:");
    display.setCursor(50, 134);
    display.printf("AirQuality-%s\n", macLastThreeSegments(mac).c_str());
    display.setCursor(29, 171);
    display.printf("to configure the device.");
    Serial.printf("[SCREEN] AP MODE AirQuality-%06X ", macLastThreeSegments(mac));
    refreshDisplay(true);
}

void drawStartup() {
    page1 = false;
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
    display.printf("for readings to normalize!");
    refreshDisplay(true);
}

void drawPage1() {
    page1 = true;
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
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
    String tempStr = String(TEMP) + "\x7F  " + RH + "%  " + CO2 + " ppm";
    display.getTextBounds(tempStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(((416 - w) / 2), 161); // the minus 4 accounts for the width of the degree symbol
    display.printf("%d\x7F  %d%%  %d ppm\n", TEMP, RH, CO2); // TODO: document degree is x7F
    // VOC Scale
    display.drawRoundRect(111, 175, 194, 16, 1, GxEPD_BLACK);
    // TODO: greyscale dither scale
    int smaller_index = ((VOC - 1.0) / (500 - 1.0)) * (187 - 1) + 1; // map 1-500 to 1-187
    display.drawBitmap(109+smaller_index, 190, lower_index_pointer, L_POINTER_WIDTH, L_POINTER_HEIGHT, GxEPD_BLACK);
    display.drawBitmap(112+smaller_index, 187, upper_index_pointer, U_POINTER_WIDTH, U_POINTER_HEIGHT, GxEPD_BLACK);
    String quality = VOC<=100 ? "Good" : VOC<=250 ? "Fair" : "Poor";
    display.setCursor(121+smaller_index, 200);
    display.setFont(&Rubik_Regular6pt7b);
    display.printf("%d - %s", VOC, quality);
    if(NOx >= WARN_NOX || VOC >= WARN_VOC || CO2 >= WARN_CO2){
        display.drawRect(0,0, 416, 240, GxEPD_BLACK);
        display.drawRect(1,1, 414, 238, GxEPD_BLACK);
        display.drawRect(2,2, 412, 236, GxEPD_BLACK);
        display.drawRect(3 ,3, 410, 234, GxEPD_BLACK);
        display.drawRect(4, 4, 408, 232, GxEPD_BLACK);
        display.drawRect(5, 5, 406, 230, GxEPD_BLACK);
    }
}

void drawPage2(){
    page1 = false;
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    // line across top seperating graphs from time/date
    display.drawRect(0,20,416,2,GxEPD_BLACK); 
    display.setFont(&Rubik_Italic9pt7b);
    display.setCursor(4,16);
    display.printf("%02d/%02d  %d:%02d %s", getRTCcal().month, getRTCcal().date, getRTCTime().hours, getRTCTime().minutes, getRTCTime().am_pm ? "PM" : "AM");
    String tempUnit = json["unit_c"].as<int>() == 1 ? "C" : "F";
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
}