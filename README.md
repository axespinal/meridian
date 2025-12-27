# Project Meridian | ATMega328P-based Portable Game Console

![Project Image](https://axelespinal.com/projects/images/meridian.webp)

A minimalist portable game console based on the ATMega328P-PU microcontroller.

Learn more about this project at https://axelespinal.com/projects/meridian.

## Status
**Work In Progress (WIP)**: The console is in early development stages. I need to finish the hardware selection, PCB design, and software development.

## Hardware
- **Microcontroller**: ATMega328P (same as Arduino Uno)
- **Display**: Unvision ST7735 1.8" TFT
- **Buttons**: 4-directional D-pad and 2 action buttons

PCB and Schematics are available in the `kicad` directory. 

## Software
The firmware is written in Arduino (C++). 

- `arduino/firmware`: The main firmware for the console. Includes custom TFT library for the ST7735 display.
- `arduino/test_cube`: A 3D rotating cube demo to test the display and performance.
- `arduino/nottetris`: A game that looks like Tetris, but due to international copyright laws, it's not.
- `arduino/snake`: The classic Snake game, the non-Metal Gear one.
