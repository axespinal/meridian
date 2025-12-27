#ifndef TFT_H
#define TFT_H

#include <Arduino.h>
#include <SPI.h>

// --- Pin Definitions ---
#define TFT_CS 6
#define TFT_DC 8
#define TFT_RST 9

// --- Screen Dimensions (After Rotation) ---
#define TFT_WIDTH 160
#define TFT_HEIGHT 128

// --- Colors (RGB565, BGR Swapped for ST7735) ---
#define C_BLACK 0x0000
#define C_WHITE 0xFFFF
#define C_GREY 0x5AEB
#define C_RED 0xF800  // RGB565 Red
#define C_BLUE 0x001F // RGB565 Blue
#define C_GREEN 0x07E0
#define C_CYAN 0x07FF   // RGB565 Cyan (Standard G+B)
#define C_YELLOW 0xFFE0 // RGB565 Yellow (Standard R+G)
#define C_ORANGE 0xFD20 // RGB565 Orange (Standard)
#define C_PURPLE 0xF81F

// --- API ---
void tftInit();
void tftFillScreen(uint16_t color);
void tftFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void tftDrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void tftDrawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
void tftDrawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
void tftDrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void tftDrawPixel(int16_t x, int16_t y, uint16_t color);

void tftSetCursor(int16_t x, int16_t y);
void tftSetTextColor(uint16_t fg);
void tftSetTextColor(uint16_t fg, uint16_t bg);
void tftSetTextSize(uint8_t s);
void tftPrint(const char *str);
void tftPrint(const __FlashStringHelper *str);
void tftPrint(long num);

// --- 5x7 Font (ASCII 32-126) ---
extern const uint8_t font5x7[] PROGMEM;

#endif
