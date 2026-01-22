# This file registers all source files in src/ as the main app component
FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

# Add library paths for Arduino libraries managed by PlatformIO
idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS 
        .
        myBitmap
        myFont
        ${CMAKE_SOURCE_DIR}/.pio/libdeps/seeed_xiao_esp32_c6/ArduinoJson/src
        ${CMAKE_SOURCE_DIR}/.pio/libdeps/seeed_xiao_esp32_c6/Time/
        ${CMAKE_SOURCE_DIR}/.pio/libdeps/seeed_xiao_esp32_c6/Timezone/
        ${CMAKE_SOURCE_DIR}/.pio/libdeps/seeed_xiao_esp32_c6/Sensirion\ Core
        ${CMAKE_SOURCE_DIR}/.pio/libdeps/seeed_xiao_esp32_c6/Sensirion\ I2C\ SGP41/ 
        # TODO: Remove comments
        #${CMAKE_SOURCE_DIR}/.pio/libdeps/seeed_xiao_esp32_c6/Adafruit\ BusIO/
        #${CMAKE_SOURCE_DIR}/.pio/libdeps/seeed_xiao_esp32_c6/Adafruit\ GFX\ Library/
        ${CMAKE_SOURCE_DIR}/.pio/libdeps/seeed_xiao_esp32_c6/DHT20/
    PRIV_REQUIRES 
        espressif__arduino-esp32
)
