# hello_world | ATMega328P-based Portable Game Console

A minimalist, Arduino-compatible portable game console based on the ATMega328P-PU microcontroller.

Learn more about this project at https://axelespinal.com/projects/hello_world.

## Status
**Work In Progress (WIP)**: The console is in early development stages. I need to finish the hardware selection, PCB design, and software development.

## Hardware
- **Microcontroller**: ATMega328P-PU
- **Display**: ST7735 TFT (might change)
- **PCB**: Design files are available in the `kicad` directory. 

## Software
The firmware is written in Arduino (C++). 

- `arduino/test_cube.ino`: A 3D rotating cube demo to test the display and performance.
- `arduino/tetris.ino`: A Tetris clone.
