Ensure you have esptool installed 
## Erase Flash:
esptool --chip esp32c6 --port "/dev/cu.usbmodem101" erase-flash
## Flash AQM:
esptool --chip esp32c6 --port "/dev/cu.usbmodem101" --baud 460800 --before default-reset --after hard-reset write-flash -z --flash-mode dio --flash-freq 80m --flash-size detect 0x0 bootloader.bin 0x5000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin