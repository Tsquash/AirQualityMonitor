# AIR QUALITY MONITOR
<img src="cover-orange.png" width="50%" height="50%">

## Background

## Setup
Requirements: Thread border router connected to your network. 
1. Scan the QR code with your phone to add the device in the "Home" app. 
2. Plug in the device. The screen will flash before showing the QR code screen again.
3. Continue with setup in the home app. Select "Add Anyways" when prompted that the device is not certified.
4. After selecting which room to add the device to, the readings should be visible under the "Climate" widget. 

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
    - **Allow 1 hour for reliable VOC detection, and 6 hours for reliable NOx event detection.**
    - Once these times are met, events can be relaibly detected within 60 seconds.