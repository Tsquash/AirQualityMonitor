# Air Quality Monitor

AQM is an ESP32-C6-based, Matter compatible, indoor air-quality monitor. It measures carbon
dioxide, temperature, humidity, VOC, and NOx, displays the latest readings on an e-paper  
display, and publishes its measurements through Matter over Wi-Fi. It can also pair with  
the companion PM sensor [particle](https://github.com/Tsquash/particle/) over Bluetooth
Low Energy (BLE) to add PM2.5 and PM10 measurements to its existing Matter endpoint.

## Hardware

- **MCU:** Seeed Studio XIAO ESP32-C6
- **Carbon-dioxide sensor:** Sensirion SCD41
- **Temperature and humidity sensor:** ASAIR DHT20
- **VOC and NOx sensor:** Sensirion SGP41
  - uses the Sensirion Gas Index Algorithm
- **RTC:** Analog Devices MAX31328
- **Display:** 3.7-inch, 416 x 240 pixel monochrome e-paper display 
  - GDEY037T03 panel/UC8253 driver, driven with GxEPD2 and Adafruit GFX libaries
- **PM interconnect:** BLE link to the companion Matter PM sensor
- **Enclosure:** acrylic sandwich-style enclosure

The primary pin assignments are defined in
[`main/sense.cpp`](main/sense.cpp), [`main/screen.cpp`](main/screen.cpp), and
[`main/buttons.h`](main/buttons.h):

| Signal | GPIO |
| --- | ---: |
| I2C SDA | 22 |
| I2C SCL | 23 |
| RTC interrupt | 2 |
| E-paper MOSI | 18 |
| E-paper SCK | 19 |
| E-paper chip select | 17 |
| E-paper data/command | 21 |
| E-paper reset | 20 |
| Top button | 0 |
| Bottom button | 1 |

## Operating modes

### Matter commissioning

The AQM is a Matter-over-Wi-Fi device and requires a 2.4 GHz Wi-Fi network and a
Matter controller. On an uncommissioned device, the display shows the Matter  
QR code and manual pairing code. Scan the QR code with a Matter-compatible  
controller app and complete the app's setup flow for Matter devices. This project  
uses Matter development credentials and is not Matter-certified, so the controller  
may ask for confirmation before adding it. Although all devices have the same pairing
code, multiple AQM devices on one network is tested working.

After commissioning, the AQM obtains local time over the network. It determines
the UTC offset from `https://ipapi.co/utc_offset/`, synchronizes system time with
`pool.ntp.org`, writes that time to the MAX31328 RTC, and attempts another RTC
sync once per hour. The hourly sync re-queries the current UTC offset, and supports DST.  
If the RTC already contains valid time, the local display can continue operating
before Matter commissioning completes.

### Normal operation

The SGP41 is sampled once per second to maintain the VOC and NOx gas-index
algorithms. Ten seconds before each minute, the RTC wakes the firmware to take
new DHT20 and SCD41 readings, finalize any PM reading received for that minute,
update the Matter attributes, and append CO2, VOC, and NOx values to the
60-sample history. At the start of the next minute, the active display page is
redrawn.

The main page shows the date, time, temperature, relative humidity, CO2, VOC,
NOx, and the overall air-quality level. The second page graphs the most recent
60 one-minute CO2, VOC, and NOx readings. The display performs a full refresh at
`:00` and `:30` each hour and uses partial refreshes between them.

### Linked to a PM sensor

The AQM can pair with the companion Matter PM sensor over BLE.

#### Pairing protocol

Put both devices into pairing mode and keep them nearby:

1. Hold the AQM's bottom button while booting it. Release the button when the
   AQM displays `Pairing with PM`.
2. Start or restart the PM sensor in pairing mode. A new PM sensor automatically
   enters pairing mode on its first boot. On later boots, hold its pair
   button or factory reset button  to force another pairing attempt.
3. Leave both devices powered and nearby until each displays `Pairing
   Successful`. Both pairing windows last up to 120 seconds.

The devices discover and authenticate each other automatically. If either device displays `Pairing
Timed Out`, put both devices back into pairing mode and retry with them nearby.
On success, the PM peer is saved in non-volatile storage and the AQM reconnects
to it automatically on later boots.

The AQM sends its RTC time and measurement schedule to the PM sensor. The PM
sensor returns one-minute PM1.0, PM2.5, and PM10 averages, which the AQM exposes
through Matter. If the BLE connection is interrupted, the AQM retains the last
PM reading for up to three missed minutes and clears it after the fourth. All
other AQM sensing and display functions continue without a linked PM sensor.

## Buttons


| Button | GPIO | At boot | During operation |
| --- | ---: | --- | --- |
| Top | 0 | Erase the complete NVS partition, including Matter commissioning, Wi-Fi credentials, and the saved PM peer | No action |
| Bottom | 1 | Enter PM-pairing mode and replace the previously saved PM peer after successful pairing | Change display page |

A boot action requires the button to be held continuously for at least one second
during the two-second startup check. Factory reset takes priority if both buttons are held. 
After a factory reset, the device restarts and displays its Matter commissioning screen.

## Readings

The AQM measures or receives the following values:

- ☁️ **CO2:** measured by the SCD41 and reported in ppm
  - +/- (50 ppm + 2.5% of the reading) from 400 to 1,000 ppm
  - +/- (50 ppm + 3% of the reading) from 1,001 to 2,000 ppm
  - +/- (40 ppm + 5% of the reading) from 2,001 to 5,000 ppm
- 🌡️ **Temperature and 💧relative humidity:** measured by the DHT20
  - Temperature accuracy: +/- 0.75 degrees C
  - Relative-humidity accuracy: +/- 4% RH
- 🍃 **VOC and 💨 NOx indices:** measured by the SGP41 and calculated with the
  Sensirion Gas Index Algorithm
  - VOC's long-term average is 100; lower values indicate improving conditions
    and higher values indicate worsening conditions
  - NOx's typical baseline is 1; values above 1 indicate NOx events
  - Allow approximately one hour for reliable VOC detection and six hours for
    reliable NOx event detection after startup
- **PM1.0, PM2.5, and PM10:** received in micrograms per cubic meter from a
  linked Matter PM sensor

The overall air-quality value is the worst of the current CO2, VOC, NOx, PM1.0,
PM2.5, and PM10 bands. It is reported as Good, Fair, Moderate, Poor, Very Poor,
or Extremely Poor.

## Build, upload, and serial output

Install [PlatformIO](https://platformio.org/) and the Espressif platform
dependencies, connect the XIAO ESP32-C6, then run these commands from the
project directory:

```sh
pio run -e esp32c6
pio run -e esp32c6 -t upload
pio device monitor -b 115200
```

The PlatformIO environment is defined in [`platformio.ini`](platformio.ini).
The project uses the Arduino and ESP-IDF frameworks and is configured for the
`seeed_xiao_esp32c6` board. Serial diagnostics run at 115200 baud. Upload-port
selection may need to be configured for the host computer.

## Project layout

```text
main/                  Firmware source, embedded display assets, and component config
  myBitmap/            Embedded monochrome display bitmaps
  myFont/              Embedded Adafruit GFX fonts
lib/                   Project-local libraries, including the altered MAX31328 driver
KICAD/                 PCB schematic, layout, symbols, and footprints
Production/            Manufacturing BOMs and Gerber archive
2D/                    Enclosure and PCB cutout drawings
3D/                    Board, spacer, and enclosure models
UI/                    Display mockups, fonts, and bitmaps
.vscode/               VS Code project configuration
platformio.ini         PlatformIO build environment
sdkconfig.defaults.*   ESP-IDF default configurations
huge_app.csv           ESP-IDF flash partition table
```

The primary firmware files are:

- [`main/main.cpp`](main/main.cpp): boot flow, Matter startup, scheduling, and
  the main loop
- [`main/sense.cpp`](main/sense.cpp): sensor initialization and sampling, gas
  indices, RTC alarms, network time, and overall air-quality calculation
- [`main/screen.cpp`](main/screen.cpp): e-paper initialization, status screens,
  measurement pages, and history graphs
- [`main/buttons.cpp`](main/buttons.cpp): boot-hold detection and display-page
  button handling
- [`main/matter_air_quality_sensor.cpp`](main/matter_air_quality_sensor.cpp):
  Matter endpoint, measurement clusters, and attribute updates
- [`main/pm_link.cpp`](main/pm_link.cpp): BLE PM pairing, reconnection, timing,
  and PM-reading reception
- [`main/pm_protocol.h`](main/pm_protocol.h): BLE data structures shared with
  the companion PM project

## Troubleshooting

- **Matter commissioning does not complete:** Confirm that the Matter controller
  and 2.4 GHz Wi-Fi network are online and that the phone is close enough for
  BLE commissioning. Factory-reset the AQM before retrying it on a different
  Matter fabric or Wi-Fi network.
- **Time or date is incorrect:** The AQM needs network access to the timezone
  API and NTP server. Check the Wi-Fi network's Internet connectivity and
  inspect the serial log for `[Time]` or `[RTC]` errors.
- **A sensor reports invalid or fixed readings:** Check the shared I2C wiring,
  sensor power, and serial output. The firmware logs DHT20, SCD41, SGP41, and RTC
  communication errors.
- **VOC or NOx readings seem unstable after startup:** Leave the device powered
  while the gas-index algorithms learn their baseline. VOC needs about one hour
  and NOx about six hours for reliable event detection.
- **For most other errors:** Check the serial log! I tried to leave debugging print statements that were helpful to me during development.

