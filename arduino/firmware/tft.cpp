#include "tft.h"

// --- Internal State ---
static int16_t cursorX = 0, cursorY = 0;
static uint16_t textFg = C_WHITE, textBg = C_BLACK;
static uint8_t textSize = 1;
static bool hasBg = false;

// Offset for ST7735 Green Tab (aligns 160x128 window to 162x132 RAM)
#define TFT_X_OFFSET 1
#define TFT_Y_OFFSET 2

// --- Fast I/O Macros (ATmega328P) ---
// CS = Pin 6 (PORTD, Bit 6)
// DC = Pin 8 (PORTB, Bit 0)
// RST = Pin 9 (PORTB, Bit 1)

#define CS_LO() PORTD &= ~(1 << 6)
#define CS_HI() PORTD |= (1 << 6)
#define DC_LO() PORTB &= ~(1 << 0)
#define DC_HI() PORTB |= (1 << 0)

// --- 5x7 Font Data (ASCII 32-126) ---
const uint8_t font5x7[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00,
    0x00, // 32: space
    0x00, 0x00, 0x5F, 0x00,
    0x00, // 33: !
    0x00, 0x07, 0x00, 0x07,
    0x00, // 34: "
    0x14, 0x7F, 0x14, 0x7F,
    0x14, // 35: #
    0x24, 0x2A, 0x7F, 0x2A,
    0x12, // 36: $
    0x23, 0x13, 0x08, 0x64,
    0x62, // 37: %
    0x36, 0x49, 0x55, 0x22,
    0x50, // 38: &
    0x00, 0x05, 0x03, 0x00,
    0x00, // 39: '
    0x00, 0x1C, 0x22, 0x41,
    0x00, // 40: (
    0x00, 0x41, 0x22, 0x1C,
    0x00, // 41: )
    0x08, 0x2A, 0x1C, 0x2A,
    0x08, // 42: *
    0x08, 0x08, 0x3E, 0x08,
    0x08, // 43: +
    0x00, 0x50, 0x30, 0x00,
    0x00, // 44: ,
    0x08, 0x08, 0x08, 0x08,
    0x08, // 45: -
    0x00, 0x60, 0x60, 0x00,
    0x00, // 46: .
    0x20, 0x10, 0x08, 0x04,
    0x02, // 47: /
    0x3E, 0x51, 0x49, 0x45,
    0x3E, // 48: 0
    0x00, 0x42, 0x7F, 0x40,
    0x00, // 49: 1
    0x42, 0x61, 0x51, 0x49,
    0x46, // 50: 2
    0x21, 0x41, 0x45, 0x4B,
    0x31, // 51: 3
    0x18, 0x14, 0x12, 0x7F,
    0x10, // 52: 4
    0x27, 0x45, 0x45, 0x45,
    0x39, // 53: 5
    0x3C, 0x4A, 0x49, 0x49,
    0x30, // 54: 6
    0x01, 0x71, 0x09, 0x05,
    0x03, // 55: 7
    0x36, 0x49, 0x49, 0x49,
    0x36, // 56: 8
    0x06, 0x49, 0x49, 0x29,
    0x1E,                         // 57: 9
    0x00, 0x36, 0x36, 0x00, 0x00, // 58: :
    0x00, 0x56, 0x36, 0x00, 0x00, // 59: ;
    0x00, 0x08, 0x14, 0x22, 0x41, // 60: <
    0x14, 0x14, 0x14, 0x14,
    0x14, // 61: =
    0x41, 0x22, 0x14, 0x08,
    0x00, // 62: >
    0x02, 0x01, 0x51, 0x09,
    0x06, // 63: ?
    0x32, 0x49, 0x79, 0x41,
    0x3E, // 64: @
    0x7E, 0x11, 0x11, 0x11,
    0x7E, // 65: A
    0x7F, 0x49, 0x49, 0x49,
    0x36, // 66: B
    0x3E, 0x41, 0x41, 0x41,
    0x22, // 67: C
    0x7F, 0x41, 0x41, 0x22,
    0x1C, // 68: D
    0x7F, 0x49, 0x49, 0x49,
    0x41, // 69: E
    0x7F, 0x09, 0x09, 0x01,
    0x01, // 70: F
    0x3E, 0x41, 0x41, 0x51,
    0x32, // 71: G
    0x7F, 0x08, 0x08, 0x08,
    0x7F, // 72: H
    0x00, 0x41, 0x7F, 0x41,
    0x00, // 73: I
    0x20, 0x40, 0x41, 0x3F,
    0x01, // 74: J
    0x7F, 0x08, 0x14, 0x22,
    0x41, // 75: K
    0x7F, 0x40, 0x40, 0x40,
    0x40, // 76: L
    0x7F, 0x02, 0x04, 0x02,
    0x7F, // 77: M
    0x7F, 0x04, 0x08, 0x10,
    0x7F, // 78: N
    0x3E, 0x41, 0x41, 0x41,
    0x3E, // 79: O
    0x7F, 0x09, 0x09, 0x09,
    0x06, // 80: P
    0x3E, 0x41, 0x51, 0x21,
    0x5E, // 81: Q
    0x7F, 0x09, 0x19, 0x29,
    0x46, // 82: R
    0x46, 0x49, 0x49, 0x49,
    0x31, // 83: S
    0x01, 0x01, 0x7F, 0x01,
    0x01, // 84: T
    0x3F, 0x40, 0x40, 0x40,
    0x3F, // 85: U
    0x1F, 0x20, 0x40, 0x20,
    0x1F, // 86: V
    0x7F, 0x20, 0x18, 0x20,
    0x7F, // 87: W
    0x63, 0x14, 0x08, 0x14,
    0x63, // 88: X
    0x03, 0x04, 0x78, 0x04,
    0x03, // 89: Y
    0x61, 0x51, 0x49, 0x45,
    0x43, // 90: Z
    0x00, 0x00, 0x7F, 0x41,
    0x41, // 91: [
    0x02, 0x04, 0x08, 0x10,
    0x20, // 92: backslash
    0x41, 0x41, 0x7F, 0x00,
    0x00, // 93: ]
    0x04, 0x02, 0x01, 0x02,
    0x04, // 94: ^
    0x40, 0x40, 0x40, 0x40,
    0x40, // 95: _
    0x00, 0x01, 0x02, 0x04,
    0x00, // 96: `
    0x20, 0x54, 0x54, 0x54,
    0x78, // 97: a
    0x7F, 0x48, 0x44, 0x44,
    0x38, // 98: b
    0x38, 0x44, 0x44, 0x44,
    0x20, // 99: c
    0x38, 0x44, 0x44, 0x48,
    0x7F, // 100: d
    0x38, 0x54, 0x54, 0x54,
    0x18, // 101: e
    0x08, 0x7E, 0x09, 0x01,
    0x02, // 102: f
    0x08, 0x14, 0x54, 0x54,
    0x3C, // 103: g
    0x7F, 0x08, 0x04, 0x04,
    0x78, // 104: h
    0x00, 0x44, 0x7D, 0x40,
    0x00, // 105: i
    0x20, 0x40, 0x44, 0x3D,
    0x00, // 106: j
    0x00, 0x7F, 0x10, 0x28,
    0x44, // 107: k
    0x00, 0x41, 0x7F, 0x40,
    0x00, // 108: l
    0x7C, 0x04, 0x18, 0x04,
    0x78, // 109: m
    0x7C, 0x08, 0x04, 0x04,
    0x78, // 110: n
    0x38, 0x44, 0x44, 0x44,
    0x38, // 111: o
    0x7C, 0x14, 0x14, 0x14,
    0x08, // 112: p
    0x08, 0x14, 0x14, 0x18,
    0x7C, // 113: q
    0x7C, 0x08, 0x04, 0x04,
    0x08, // 114: r
    0x48, 0x54, 0x54, 0x54,
    0x20, // 115: s
    0x04, 0x3F, 0x44, 0x40,
    0x20, // 116: t
    0x3C, 0x40, 0x40, 0x20,
    0x7C, // 117: u
    0x1C, 0x20, 0x40, 0x20,
    0x1C, // 118: v
    0x3C, 0x40, 0x30, 0x40,
    0x3C, // 119: w
    0x44, 0x28, 0x10, 0x28,
    0x44, // 120: x
    0x0C, 0x50, 0x50, 0x50,
    0x3C, // 121: y
    0x44, 0x64, 0x54, 0x4C,
    0x44, // 122: z
    0x00, 0x08, 0x36, 0x41,
    0x00, // 123: {
    0x00, 0x00, 0x7F, 0x00,
    0x00, // 124: |
    0x00, 0x41, 0x36, 0x08,
    0x00, // 125: }
    0x08, 0x08, 0x2A, 0x1C,
    0x08, // 126: ~
};

// --- SPI Helpers ---
static inline void spiWrite(uint8_t d) { SPI.transfer(d); }

static inline void writeCommand(uint8_t cmd) {
  DC_LO();
  CS_LO();
  spiWrite(cmd);
  CS_HI();
}

static inline void writeData(uint8_t d) {
  DC_HI();
  CS_LO();
  spiWrite(d);
  CS_HI();
}

static void setAddrWindow(int16_t x, int16_t y, int16_t w, int16_t h) {
  // Column Address Set (CASET)
  writeCommand(0x2A);
  writeData(0x00);
  writeData(x + TFT_X_OFFSET);
  writeData(0x00);
  writeData(x + w + TFT_X_OFFSET - 1);
  // Row Address Set (RASET)
  writeCommand(0x2B);
  writeData(0x00);
  writeData(y + TFT_Y_OFFSET);
  writeData(0x00);
  writeData(y + h + TFT_Y_OFFSET - 1);
  // Memory Write (RAMWR)
  writeCommand(0x2C);
}

// --- Init Sequence for ST7735 (Green Tab 160x128) ---
void tftInit() {
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_RST, OUTPUT);

  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV2); // Fast SPI

  // Hardware Reset
  digitalWrite(TFT_RST, HIGH);
  delay(5);
  digitalWrite(TFT_RST, LOW);
  delay(20);
  digitalWrite(TFT_RST, HIGH);
  delay(150);

  writeCommand(0x01);
  delay(150); // Software Reset
  writeCommand(0x11);
  delay(500); // Sleep Out

  // Frame Rate Control
  writeCommand(0xB1);
  writeData(0x01);
  writeData(0x2C);
  writeData(0x2D);
  writeCommand(0xB2);
  writeData(0x01);
  writeData(0x2C);
  writeData(0x2D);
  writeCommand(0xB3);
  writeData(0x01);
  writeData(0x2C);
  writeData(0x2D);
  writeData(0x01);
  writeData(0x2C);
  writeData(0x2D);

  writeCommand(0xB4);
  writeData(0x07); // Column Inversion

  // Power Sequence
  writeCommand(0xC0);
  writeData(0xA2);
  writeData(0x02);
  writeData(0x84);
  writeCommand(0xC1);
  writeData(0xC5);
  writeCommand(0xC2);
  writeData(0x0A);
  writeData(0x00);
  writeCommand(0xC3);
  writeData(0x8A);
  writeData(0x2A);
  writeCommand(0xC4);
  writeData(0x8A);
  writeData(0xEE);
  writeCommand(0xC5);
  writeData(0x0E); // VCOM

  writeCommand(0x36);
  writeData(0xA0); // Memory Access Control: Rotation 1 (landscape) (RGB mode)
  writeCommand(0x3A);
  writeData(0x05); // Pixel Format: 16-bit

  // Gamma
  writeCommand(0xE0);
  writeData(0x02);
  writeData(0x1C);
  writeData(0x07);
  writeData(0x12);
  writeData(0x37);
  writeData(0x32);
  writeData(0x29);
  writeData(0x2D);
  writeData(0x29);
  writeData(0x25);
  writeData(0x2B);
  writeData(0x39);
  writeData(0x00);
  writeData(0x01);
  writeData(0x03);
  writeData(0x10);
  writeCommand(0xE1);
  writeData(0x03);
  writeData(0x1D);
  writeData(0x07);
  writeData(0x06);
  writeData(0x2E);
  writeData(0x2C);
  writeData(0x29);
  writeData(0x2D);
  writeData(0x2E);
  writeData(0x2E);
  writeData(0x37);
  writeData(0x3F);
  writeData(0x00);
  writeData(0x00);
  writeData(0x02);
  writeData(0x10);

  writeCommand(0x29);
  delay(100); // Display On
}

// --- Drawing Functions ---

void tftFillScreen(uint16_t color) {
  tftFillRect(0, 0, TFT_WIDTH, TFT_HEIGHT, color);
}

void tftFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > TFT_WIDTH)
    w = TFT_WIDTH - x;
  if (y + h > TFT_HEIGHT)
    h = TFT_HEIGHT - y;
  if (w <= 0 || h <= 0)
    return;

  setAddrWindow(x, y, w, h);
  uint8_t hi = color >> 8, lo = color & 0xFF;
  DC_HI();
  CS_LO();
  for (int32_t i = (int32_t)w * h; i > 0; i--) {
    spiWrite(hi);
    spiWrite(lo);
  }
  CS_HI();
}

void tftDrawPixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || x >= TFT_WIDTH || y < 0 || y >= TFT_HEIGHT)
    return;
  setAddrWindow(x, y, 1, 1);
  DC_HI();
  CS_LO();
  spiWrite(color >> 8);
  spiWrite(color);
  CS_HI();
}

void tftDrawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  tftFillRect(x, y, w, 1, color);
}

void tftDrawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  tftFillRect(x, y, 1, h, color);
}

void tftDrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  tftDrawFastHLine(x, y, w, color);
  tftDrawFastHLine(x, y + h - 1, w, color);
  tftDrawFastVLine(x, y, h, color);
  tftDrawFastVLine(x + w - 1, y, h, color);
}

void tftDrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  tftDrawPixel(x0, y0 + r, color);
  tftDrawPixel(x0, y0 - r, color);
  tftDrawPixel(x0 + r, y0, color);
  tftDrawPixel(x0 - r, y0, color);

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;
    tftDrawPixel(x0 + x, y0 + y, color);
    tftDrawPixel(x0 - x, y0 + y, color);
    tftDrawPixel(x0 + x, y0 - y, color);
    tftDrawPixel(x0 - x, y0 - y, color);
    tftDrawPixel(x0 + y, y0 + x, color);
    tftDrawPixel(x0 - y, y0 + x, color);
    tftDrawPixel(x0 + y, y0 - x, color);
    tftDrawPixel(x0 - y, y0 - x, color);
  }
}

// --- Text Functions ---

void tftSetCursor(int16_t x, int16_t y) {
  cursorX = x;
  cursorY = y;
}
void tftSetTextColor(uint16_t fg) {
  textFg = fg;
  hasBg = false;
}
void tftSetTextColor(uint16_t fg, uint16_t bg) {
  textFg = fg;
  textBg = bg;
  hasBg = true;
}
void tftSetTextSize(uint8_t s) { textSize = s; }

static void drawChar(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg,
                     uint8_t size, bool opaque) {
  if (c < 32 || c > 126)
    c = '?';
  uint8_t idx = c - 32;

  for (uint8_t col = 0; col < 5; col++) {
    uint8_t line = pgm_read_byte(&font5x7[idx * 5 + col]);
    for (uint8_t row = 0; row < 7; row++) {
      if (line & (1 << row)) {
        if (size == 1)
          tftDrawPixel(x + col, y + row, fg);
        else
          tftFillRect(x + col * size, y + row * size, size, size, fg);
      } else if (opaque) {
        if (size == 1)
          tftDrawPixel(x + col, y + row, bg);
        else
          tftFillRect(x + col * size, y + row * size, size, size, bg);
      }
    }
  }
  // Spacing column
  if (opaque) {
    if (size == 1)
      for (uint8_t row = 0; row < 7; row++)
        tftDrawPixel(x + 5, y + row, bg);
    else
      tftFillRect(x + 5 * size, y, size, 7 * size, bg);
  }
}

void tftPrint(const char *str) {
  while (*str) {
    drawChar(cursorX, cursorY, *str, textFg, textBg, textSize, hasBg);
    cursorX += 6 * textSize;
    str++;
  }
}

void tftPrint(const __FlashStringHelper *str) {
  PGM_P p = reinterpret_cast<PGM_P>(str);
  char c;
  while ((c = pgm_read_byte(p++)) != 0) {
    drawChar(cursorX, cursorY, c, textFg, textBg, textSize, hasBg);
    cursorX += 6 * textSize;
  }
}

void tftPrint(long num) {
  char buf[12];
  ltoa(num, buf, 10);
  tftPrint(buf);
}
