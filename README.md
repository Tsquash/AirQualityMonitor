# AIR QUALITY MONITOR
<img src="cover-orange.png" width="50%" height="50%">

## Background
Air Quality Monitor is an embedded IoT device with matter capability to read, display, and transmit air quality data. It runs on an ESP32C6 which interfaces the sensors and E-Paper display, while also handling the matter protocol over thread. The device is mounted in an acrylic sandwich style enclosure.

## Setup
Requirements: Thread border router connected to your network. 
1. Scan the QR code with your phone to add the device in the "Home" app. 
2. Plug in the device. The screen will flash before showing the QR code screen again.
3. Continue with setup in the home app. Select "Add Anyways" when prompted that the device is not certified.
4. After selecting which room to add the device to, the readings should be visible under the "Climate" widget. 

## Operation
After connecting the device to your thread network, the RTC is set via NTP. Your UTC offset is automatically calculated with your IP via <a href="https://ipapi.co/utc_offset/" target="_blank">an API</a>. 

Two buttons are available on the left side of the device. 
* The bottom one allows you to switch to a second page to show air quality data graphed over the last hour. Simply click the button again to return to the main page. 
* The top button is used to decommission the device from your matter network. This is useful when you want to fully reset the device or move the device to a new network. 
    * To do this, hold down the button before plugging the device in. Then, while still holding, plug in the device and only release once you see the matter QR code setup screen. This should take ~3s. 

## Readings
The Air Quality Montitor senses the following: 
- ☁️ CO2 via SCD41
    - ±(50 ppm + 2.5% of reading) for 400-1000PPM
    - ±(50 ppm + 3% of reading) for 1001-2000PPM
    - ±(40 ppm + 5% of reading) for 2001-5000PPM
- 🌡️ Temperature and 💧 Humidity via DHT20 
    - ± 4% Relative Humidity
    - ± 0.75°C
- 🍃 VOC and 💨 NOx Events via SGP41
    - ± 5 index points for both NOx and VOC
    - Average VOC index over the last 24 hours is 100. An index less than 100 indicates decreasing VOC events, while indices above 100 indicate worsening VOC conditions. 
    - Average NOx is 1, and values above 1 indicate NOx events are present. 
    - **Allow 1 hour for reliable VOC detection, and 6 hours for reliable NOx event detection.**
    - Once these times are met, events can be relaibly detected within 60 seconds.

## Setting up Local Development
Requirements: VS Code with pioarduino extension. 