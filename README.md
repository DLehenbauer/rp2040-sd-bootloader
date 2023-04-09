# RP2040 SD Card Bootloader

THIS IS A PROOF OF CONCEPT, NOT A POLISHED PROJECT

This was forked from RP2040 Serial Bootloader to provide the similar functionality but instead read and load .uf2 file from SD card.

Blink example was copied from https://github.com/usedbytes/pico-blink-noboot2

uf2tool.py was copied from https://github.com/rhulme/pico-flashloader - licensing included in file (same, BSD-3-Clause license).

This example depends on the SD card library https://github.com/Tails86/no-OS-FatFS-SD-SPI-RPi-Pico which was forked from https://github.com/carlk3/no-OS-FatFS-SD-SPI-RPi-Pico


# RP2040 Serial Bootloader

This is a bootloader for the RP2040 which enables code upload via UART.

There's a more complete description at: https://blog.usedbytes.com/2021/12/pico-serial-bootloader/

There are currently two tools I know of which can be used to upload code to it:

* [serial-flash](https://github.com/usedbytes/serial-flash) - my tool written in `go`
* [pico-py-serial-flash](https://github.com/ConfedSolutions/pico-py-serial-flash) - a similar tool written in Python, contributed by another user.
