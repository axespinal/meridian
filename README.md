![Project Meridian Logo](https://axelespinal.com/projects/meridian/images/meridian_logo.JPEG)

# Project Meridian

A fully open portable game console, powered by the ATMega328P microcontroller.

Learn more about this project at https://axelespinal.com/projects/meridian.

PCB and Schematics are available in the `kicad` directory. 

## Firmware
The firmware is written in Arduino (C++). 

- `arduino/firmware`: The main firmware for the console. Includes custom TFT library for the ST7735 display.
- `arduino/test_cube`: A 3D rotating cube demo to test the display and performance.
- `arduino/nottetris`: A game that looks like Tetris, but due to international copyright laws, it's not.
- `arduino/snake`: The classic Snake game, the non-Metal Gear one.
