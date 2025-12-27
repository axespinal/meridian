#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

/*
 nottetris.ino | part of project meridian

 it looks like tetris, but due to international copyright laws, it's not.

 [//] axelespinal.com/projects

 last updated: 2025-12-26
*/

// --- SCREEN SETUP ---
#define TFT_CS 6
#define TFT_RST 9
#define TFT_DC 8

// --- BUTTON PINS ---
// PORT C (Analog Pins)
#define BTN_LEFT_BIT 0  // A0
#define BTN_RIGHT_BIT 1 // A1
#define BTN_UP_BIT 2    // A2
#define BTN_DOWN_BIT 3  // A3
#define BTN_MENU_BIT 4  // A4 (Was Hold, Now Menu)
#define BTN_START_BIT 5 // A5

// PORT D (Digital Pins)
#define BTN_A_BIT 2 // D2 (Hard Drop)
#define BTN_B_BIT 4 // D4 (Hold)

#define IS_PRESSED_C(bit) (!(PINC & (1 << bit)))
#define IS_PRESSED_D(bit) (!(PIND & (1 << bit)))

// Use Standard Library (Green Tab fixes geometry)
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// --- LAYOUT CONSTANTS ---
#define BLOCK_SIZE 6
#define BOARD_W 10
#define BOARD_H 20

// Layout Positions
#define BOARD_X 5
#define BOARD_Y 4
#define NEXT_X 75
#define NEXT_Y 5 // Top of right panel
#define HOLD_X 115
#define HOLD_Y 5
#define SCORE_X 75
#define SCORE_Y 55 // Below boxes (label at 47, value at 55)
#define STATUS_X 75
#define STATUS_Y 75 // Between score and footer

// --- COLORS (BGR Fix) ---
#define C_BLACK ST7735_BLACK
#define C_WHITE ST7735_WHITE
#define C_GREY 0x5AEB
#define C_RED ST7735_BLUE
#define C_BLUE ST7735_RED
#define C_CYAN ST7735_YELLOW
#define C_YELLOW ST7735_CYAN
#define C_ORANGE 0x041F
#define C_GREEN ST7735_GREEN
#define C_PURPLE ST7735_MAGENTA

// --- GLOBALS ---
int8_t board[BOARD_W][BOARD_H];

const uint16_t shapes[7][4] PROGMEM = {
    {0x00F0, 0x4444, 0x0F00, 0x2222}, // I
    {0x0660, 0x0660, 0x0660, 0x0660}, // O
    {0x06C0, 0x4620, 0x0360, 0x0462}, // S
    {0x0C60, 0x2640, 0x0630, 0x0264}, // Z
    {0x0E20, 0x2260, 0x0470, 0x0644}, // J
    {0x0E80, 0x6220, 0x0170, 0x0446}, // L
    {0x0E40, 0x2620, 0x0270, 0x0464}, // T
};

const uint16_t colors[] PROGMEM = {C_BLACK, C_CYAN, C_YELLOW, C_GREEN,
                                   C_RED,   C_BLUE, C_ORANGE, C_PURPLE};

int8_t px, py, pRot, pType;
int8_t nextType;
int8_t holdType = -1;
int8_t lastGhostY = -1; // Cache for optimization
bool canHold = true;

// 7-bag randomizer
int8_t bag[7];
int8_t bagIndex = 7; // Start at 7 to trigger initial shuffle

// Forward declarations
void shuffleBag();
int getNextFromBag();
void showStartScreen();
void hardDrop();

long score = 0;
unsigned long lastDropTime = 0;
int dropInterval = 800;

// State Variables
bool gameOver = false;
bool paused = false;
bool prevPaused = false;   // To fix flicker
bool prevGameOver = false; // To fix flicker

// Button State Tracking (For "On Release" logic)
bool lastStartBtn = HIGH;
bool lastButtonB = HIGH; // Hold
bool lastButtonA = HIGH; // Hard Drop
bool lastMenuBtn = HIGH; // Menu

void setup() {
  // Setup pins using A0 base for compatibility
  for (int i = 0; i <= 5; i++) {
    pinMode(A0 + i, INPUT_PULLUP);
  }
  // Setup Digital Pins
  pinMode(2, INPUT_PULLUP); // Button A
  pinMode(4, INPUT_PULLUP); // Button B

  tft.initR(INITR_GREENTAB);
  tft.fillScreen(C_BLACK);
  tft.setRotation(1);

  showStartScreen();
}

void showStartScreen() {
  // Wait for Release (Latch) - Prevent accidental re-entry
  while ((~PINC & 0x3F) != 0 || !digitalRead(2) || !digitalRead(4)) {
    delay(10);
  }

  tft.fillRect(0, 0, 160, 128, C_BLACK); // Clear full 160x128 landscape

  // "(not) tetris"
  // "(not)" in Gray, " tetris" in White
  tft.setTextSize(1);
  // Center on 160px width: (160 - (12*6))/2 = 44
  int startX = 44;
  tft.setCursor(startX, 40);
  tft.setTextColor(C_GREY);
  tft.print(F("(not)"));
  tft.setTextColor(C_WHITE);
  tft.print(F(" tetris"));

  // "press any key to start"
  // Center on 160px: (160 - (22*6))/2 = 14
  tft.setCursor(14, 60);
  tft.print(F("press any key to start"));

  // Footer (Tethered to bottom 128px)
  tft.setTextColor(C_GREY);
  // Center on 160px: (160 - (17*6))/2 = 29
  tft.setCursor(29, 108);
  tft.print(F("[//] axel espinal"));

  // Center on 160px: (160 - (24*6))/2 = 8
  tft.setCursor(8, 118);
  tft.print(F("axelespinal.com/projects"));

  // Wait for ANY key press (Check all bits 0-5 on Port C, plus D2/D4)
  // Simple check loop
  while (true) {
    if ((~PINC & 0x3F) != 0 || !digitalRead(2) || !digitalRead(4)) {
      randomSeed(micros());
      break;
    }
    delay(10);
  }

  // Initialize bag and get first piece (Fixing RNG bug)
  resetGame();
}

void loop() {
  // --- GLOBAL: CHECK MENU BUTTON ---
  bool currMenuBtn = IS_PRESSED_C(BTN_MENU_BIT);
  if (lastMenuBtn == LOW && currMenuBtn == HIGH) {
    showStartScreen();
    lastMenuBtn = currMenuBtn;
    return;
  }
  lastMenuBtn = currMenuBtn;

  // --- STATE 1: GAME OVER ---
  if (gameOver) {
    if (!prevGameOver) { // Only draw ONCE
      drawStatus("GAME OVER", C_RED);
      prevGameOver = true;
    }
    // Check for Restart (On Release)
    bool currStartBtn = IS_PRESSED_C(BTN_START_BIT);
    if (lastStartBtn == LOW && currStartBtn == HIGH) {
      resetGame();
    }
    lastStartBtn = currStartBtn;
    return;
  }

  // --- STATE 2: PAUSED ---
  // Check Pause Toggle (On Release)
  bool currStartBtn = IS_PRESSED_C(BTN_START_BIT);
  if (lastStartBtn == LOW && currStartBtn == HIGH) {
    paused = !paused;
    if (paused) {
      drawStatus("PAUSED", C_WHITE); // Draw once entering pause
    } else {
      tft.fillRect(STATUS_X, STATUS_Y, 80, 10, C_BLACK); // Clear once exiting
      eraseGhost(px, getGhostY(px, py, pRot),
                 pRot); // Erase old ghost before redraw
      redrawBoard();    // Redraw board to fix any text overlap
      drawGhost(px, getGhostY(px, py, pRot), pRot); // Draw ghost first
      drawPiece();                                  // Draw active piece on top
    }
  }
  lastStartBtn = currStartBtn;

  if (paused) {
    return; // Do nothing else while paused
  }

  // --- STATE 3: PLAYING ---
  handleInput();

  if (millis() - lastDropTime > dropInterval) {
    if (isValidMove(px, py + 1, pRot)) {
      movePiece(0, 1);
    } else {
      lockPiece();
    }
    lastDropTime = millis();
  }
}

// --- UI HELPERS ---

void drawFooter() {
  // Align with bottom of game panel (160x128 screen)
  // 3 lines, ~8px height + padding.
  // Bottom line at ~118.
  int footerY = 98;
  int footerX = 75;

  tft.setTextSize(1);
  tft.setTextColor(C_GREY);

  tft.setCursor(footerX, footerY);
  tft.print(F("(not) tetris"));

  tft.setCursor(footerX, footerY + 10);
  tft.print(F("axel espinal"));

  tft.setCursor(footerX, footerY + 20);
  tft.print(F("2025-12-20"));
}

void drawStatus(const char *text, uint16_t color) {
  tft.fillRect(STATUS_X, STATUS_Y, 80, 10, C_BLACK);
  tft.setCursor(STATUS_X, STATUS_Y);
  tft.setTextColor(color);
  tft.print(text);
}

void updateScore(int points) {
  score += points;
  tft.fillRect(SCORE_X, SCORE_Y, 50, 10, C_BLACK);
  tft.setCursor(SCORE_X, SCORE_Y);
  tft.setTextColor(C_WHITE);
  tft.print(score);

  if (score > 500)
    dropInterval = 600;
  if (score > 1000)
    dropInterval = 400;
  if (score > 2000)
    dropInterval = 200;
}

void drawPreview() {
  // Clear inside the box (box starts at NEXT_Y+9)
  tft.fillRect(NEXT_X + 1, NEXT_Y + 10, 28, 28, C_BLACK);

  // Center pieces in 30x30 box
  // Most pieces occupy rows 1-2 of the 4x4 grid (offset j=1)
  // Box inner area: 28x28 at (NEXT_X+1, NEXT_Y+10)
  // For 2-row pieces: 2*5=10px height, center = (28-10)/2 = 9 from top
  int offsetX = NEXT_X + 4;  // Adjusted for visual centering
  int offsetY = NEXT_Y + 14; // Box top + centering offset

  uint16_t color = pgm_read_word(&colors[nextType + 1]);
  uint16_t shape = pgm_read_word(&shapes[nextType][0]); // Default rotation 0

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if ((shape >> (j * 4 + i)) & 1) {
        tft.fillRect(offsetX + i * 5, offsetY + j * 5, 4, 4, color);
      }
    }
  }
}

void drawHold() {
  tft.fillRect(HOLD_X + 1, HOLD_Y + 10, 28, 28, C_BLACK);

  if (holdType == -1)
    return;

  int offsetX = HOLD_X + 4;
  int offsetY = HOLD_Y + 14;

  uint16_t color = (canHold) ? pgm_read_word(&colors[holdType + 1]) : C_GREY;
  uint16_t shape = pgm_read_word(&shapes[holdType][0]);

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if ((shape >> (j * 4 + i)) & 1) {
        tft.fillRect(offsetX + i * 5, offsetY + j * 5, 4, 4, color);
      }
    }
  }
}

// --- ENGINE LOGIC ---

void handleInput() {
  static unsigned long lastInput = 0;
  // Reduced debounce for responsiveness
  if (millis() - lastInput < 100)
    return;

  if (IS_PRESSED_C(BTN_LEFT_BIT) && isValidMove(px - 1, py, pRot)) {
    updatePiecePosition(px - 1, py, pRot);
    lastInput = millis();
  }
  if (IS_PRESSED_C(BTN_RIGHT_BIT) && isValidMove(px + 1, py, pRot)) {
    updatePiecePosition(px + 1, py, pRot);
    lastInput = millis();
  }
  // -- ROTATE ON RELEASE LOGIC --
  static bool lastUpBtn = HIGH;
  bool currUpBtn = IS_PRESSED_C(BTN_UP_BIT);
  if (lastUpBtn == LOW && currUpBtn == HIGH) {
    // Skip rotation for O piece (cube) - it's symmetrical
    if (pType != 1) {
      int newRot = (pRot + 1) % 4;
      if (isValidMove(px, py, newRot)) {
        updatePiecePosition(px, py, newRot);
      }
    }
  }
  lastUpBtn = currUpBtn;

  if (IS_PRESSED_C(BTN_DOWN_BIT) && isValidMove(px, py + 1, pRot)) {
    updatePiecePosition(px, py + 1, pRot);
    updateScore(1);
    lastInput = millis() - 50;
  }

  // -- HOLD ON RELEASE LOGIC (NOW BUTTON B) --
  bool currBtnB = IS_PRESSED_D(BTN_B_BIT);
  if (lastButtonB == LOW && currBtnB == HIGH) {
    holdPiece();
  }
  lastButtonB = currBtnB;

  // -- HARD DROP ON RELEASE LOGIC (BUTTON A) --
  bool currBtnA = IS_PRESSED_D(BTN_A_BIT);
  if (lastButtonA == LOW && currBtnA == HIGH) {
    hardDrop();
  }
  lastButtonA = currBtnA;
}

// (Standard Helper Functions remain identical below)

void newPiece() {
  pType = nextType;
  nextType = getNextFromBag();
  drawPreview();
  px = 3;
  py = 0;
  pRot = 0;
  lastGhostY = getGhostY(px, py, pRot); // Initialize ghost cache

  if (!isValidMove(px, py, pRot))
    gameOver = true;
  else
    drawPiece();
}

void shuffleBag() {
  // Fill bag with pieces 0-6
  for (int i = 0; i < 7; i++) {
    bag[i] = i;
  }
  // Fisher-Yates shuffle
  for (int i = 6; i > 0; i--) {
    int j = random(0, i + 1);
    int temp = bag[i];
    bag[i] = bag[j];
    bag[j] = temp;
  }
  bagIndex = 0;
}

int getNextFromBag() {
  if (bagIndex >= 7) {
    shuffleBag();
  }
  return bag[bagIndex++];
}

void holdPiece() {
  if (!canHold)
    return;

  erasePiece();

  if (holdType == -1) {
    holdType = pType;
    newPiece();
  } else {
    int temp = holdType;
    holdType = pType;
    pType = temp;
    px = 3;
    py = 0;
    pRot = 0; // Reset position
    drawPiece();
  }

  canHold = false;
  drawHold();
}

void hardDrop() {
  int dropY = getGhostY(px, py, pRot);
  // Instant score reward
  updateScore((dropY - py) * 2);
  // Draw directly at ghost location
  updatePiecePosition(px, dropY, pRot);
  // Lock effectively instantly
  lockPiece();
}

void lockPiece() {
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (getShape(i, j, pRot)) {
        if (py + j >= 0)
          board[px + i][py + j] = pType + 1;
      }
    }
  }
  checkLines();
  canHold = true; // Reset hold ability
  drawHold();     // Redraw to remove grey status
  newPiece();
}

void checkLines() {
  int linesCleared = 0;
  for (int y = BOARD_H - 1; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < BOARD_W; x++)
      if (board[x][y] == 0)
        full = false;
    if (full) {
      linesCleared++;
      tft.fillRect(BOARD_X, BOARD_Y + y * BLOCK_SIZE, BOARD_W * BLOCK_SIZE,
                   BLOCK_SIZE, C_WHITE);
      delay(30);
      for (int ny = y; ny > 0; ny--) {
        for (int nx = 0; nx < BOARD_W; nx++)
          board[nx][ny] = board[nx][ny - 1];
      }
      redrawBoard();
      y++;
    }
  }
  if (linesCleared == 1)
    updateScore(40);
  else if (linesCleared == 2)
    updateScore(100);
  else if (linesCleared == 3)
    updateScore(300);
  else if (linesCleared == 4)
    updateScore(1200);
}

void updatePiecePosition(int newX, int newY, int newRot) {
  // --- 1. Smart Ghost Update ---
  int oldGhostY = lastGhostY;
  int newGhostY;

  // Optimization: If X and Rot haven't changed, Ghost Y is likely same (unless
  // board changed, but updatePiecePosition is for moves) Strict check: if X and
  // Rot are same, Ghost Y MUST be same because we occupy same column on same
  // board state.
  if (newX == px && newRot == pRot) {
    newGhostY = oldGhostY;
  } else {
    newGhostY = getGhostY(newX, newY, newRot);
  }

  // Check if we essentially moved the ghost
  if (oldGhostY != newGhostY || px != newX || pRot != newRot) {
    eraseGhost(px, oldGhostY, pRot);
    drawGhost(newX, newGhostY, newRot);
    lastGhostY = newGhostY;
  }

  // --- 2. Smart Piece Update (Differential Draw) ---
  // Erase only pixels that are NOT present in the new position
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (getShape(i, j, pRot)) {
        int oldSi = px + i;
        int oldSj = py + j;

        // Transform this block's world coord to the new piece's local coord to
        // check overlap
        int localX = oldSi - newX;
        int localY = oldSj - newY;
        bool retained = false;
        if (localX >= 0 && localX < 4 && localY >= 0 && localY < 4) {
          if (getShape(localX, localY, newRot))
            retained = true;
        }

        if (!retained) {
          int screenY = BOARD_Y + oldSj * BLOCK_SIZE;
          if (screenY >= BOARD_Y) {
            // Before erasing to black, check if we should restore the GHOST
            // here The NEW ghost is at `newGhostY` Check if this (oldSi, oldSj)
            // overlaps with the NEW ghost
            int ghostLocalY = oldSj - newGhostY;
            int ghostLocalX = oldSi - newX; // Ghost X is same as Piece X

            bool isGhostHere = false;
            // Ghost shares the new X and new Rot
            if (ghostLocalX >= 0 && ghostLocalX < 4 && ghostLocalY >= 0 &&
                ghostLocalY < 4) {
              if (getShape(ghostLocalX, ghostLocalY, newRot))
                isGhostHere = true;
            }

            if (isGhostHere) {
              tft.drawRect(BOARD_X + oldSi * BLOCK_SIZE, screenY,
                           BLOCK_SIZE - 1, BLOCK_SIZE - 1, C_GREY);
            } else {
              tft.fillRect(BOARD_X + oldSi * BLOCK_SIZE, screenY,
                           BLOCK_SIZE - 1, BLOCK_SIZE - 1, C_BLACK);
            }
          }
        }
      }
    }
  }

  // Draw the new piece (It's okay to overwrite existing pixels, no flicker
  // there)
  drawPieceHelper(newX, newY, newRot, pgm_read_word(&colors[pType + 1]));

  // Update globals
  px = newX;
  py = newY;
  pRot = newRot;
}

void movePiece(int dx, int dy) { updatePiecePosition(px + dx, py + dy, pRot); }

bool isValidMove(int x, int y, int rot) {
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (getShape(i, j, rot)) {
        int nx = x + i;
        int ny = y + j;
        if (nx < 0 || nx >= BOARD_W || ny >= BOARD_H)
          return false;
        if (ny >= 0 && board[nx][ny] != 0)
          return false;
      }
    }
  }
  return true;
}

int getShape(int x, int y, int rot) {
  // Read pre-calculated bitmask for this rotation
  // rot 0-3 are stored sequentially in data[4]
  uint16_t shape = pgm_read_word(&shapes[pType][rot]);
  // Check bit at appropriate position
  return (shape >> (y * 4 + x)) & 1;
}

// Basic helpers still needed for initialization/locking
void drawPiece() {
  drawGhost(px, getGhostY(px, py, pRot), pRot);
  drawPieceHelper(px, py, pRot, pgm_read_word(&colors[pType + 1]));
}

void erasePiece() {
  eraseGhost(px, getGhostY(px, py, pRot), pRot);
  drawPieceHelper(px, py, pRot, C_BLACK);
}

void drawPieceHelper(int x, int y, int rot, uint16_t color) {
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (getShape(i, j, rot)) {
        int screenY = BOARD_Y + (y + j) * BLOCK_SIZE;
        // Strict bounds check to prevent crash on edge drawing
        // Screen Height is 128. Drawn blocks should be within [0, 128].
        if (screenY >= BOARD_Y && screenY < 128) {
          int screenX = BOARD_X + (x + i) * BLOCK_SIZE;
          if (screenX >= 0 && screenX < 160) {
            tft.fillRect(screenX, screenY, BLOCK_SIZE - 1, BLOCK_SIZE - 1,
                         color);
          }
        }
      }
    }
  }
}

// Ghost piece (landing shadow)
int getGhostY(int x, int y, int rot) {
  int ghostY = y;
  while (isValidMove(x, ghostY + 1, rot)) {
    ghostY++;
  }
  return ghostY;
}

void drawGhost(int x, int y, int rot) {
  if (y == py && x == px)
    return; // Don't draw if piece covers it

  uint16_t ghostColor = C_GREY;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (getShape(i, j, rot)) {
        int screenY = BOARD_Y + (y + j) * BLOCK_SIZE;
        // Bounds check
        if (screenY >= BOARD_Y && screenY < 128) {
          int screenX = BOARD_X + (x + i) * BLOCK_SIZE;
          if (screenX >= 0 && screenX < 160) {
            tft.drawRect(screenX, screenY, BLOCK_SIZE - 1, BLOCK_SIZE - 1,
                         ghostColor);
          }
        }
      }
    }
  }
}

void eraseGhost(int x, int y, int rot) {
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (getShape(i, j, rot)) {
        int bx = x + i;
        int by = y + j;
        int screenY = BOARD_Y + by * BLOCK_SIZE;

        // Bounds check before drawing to prevent crashes
        if (screenY >= BOARD_Y && screenY < 128 && by >= 0 && by < BOARD_H &&
            bx >= 0 && bx < BOARD_W) {
          // If there's a locked piece behind it, redrawing it is tricky without
          // pixel access. Since we track board state, we can redraw the locked
          // piece.
          if (board[bx][by] > 0) {
            tft.fillRect(BOARD_X + bx * BLOCK_SIZE, screenY, BLOCK_SIZE - 1,
                         BLOCK_SIZE - 1, pgm_read_word(&colors[board[bx][by]]));
          } else {
            tft.drawRect(BOARD_X + bx * BLOCK_SIZE, screenY, BLOCK_SIZE - 1,
                         BLOCK_SIZE - 1, C_BLACK);
          }
        }
      }
    }
  }
}

void redrawBoard() {
  tft.fillRect(BOARD_X, BOARD_Y, BOARD_W * BLOCK_SIZE, BOARD_H * BLOCK_SIZE,
               C_BLACK);
  for (int x = 0; x < BOARD_W; x++) {
    for (int y = 0; y < BOARD_H; y++) {
      if (board[x][y] > 0)
        tft.fillRect(BOARD_X + x * BLOCK_SIZE, BOARD_Y + y * BLOCK_SIZE,
                     BLOCK_SIZE - 1, BLOCK_SIZE - 1,
                     pgm_read_word(&colors[board[x][y]]));
    }
  }
}

void resetGame() {
  for (int x = 0; x < BOARD_W; x++)
    for (int y = 0; y < BOARD_H; y++)
      board[x][y] = 0;
  score = 0;
  dropInterval = 800;
  prevGameOver = false;
  holdType = -1;
  canHold = true;

  tft.fillScreen(C_BLACK);
  tft.drawRect(BOARD_X - 2, BOARD_Y - 2, (BOARD_W * BLOCK_SIZE) + 4,
               (BOARD_H * BLOCK_SIZE) + 4, C_WHITE);

  tft.setTextColor(C_WHITE);
  tft.setCursor(NEXT_X, NEXT_Y);
  tft.print("NEXT");
  tft.drawRect(NEXT_X, NEXT_Y + 9, 30, 30, C_GREY);

  tft.setCursor(HOLD_X, HOLD_Y);
  tft.print("HOLD");
  tft.drawRect(HOLD_X, HOLD_Y + 9, 30, 30, C_GREY);

  tft.setCursor(SCORE_X, SCORE_Y - 8);
  tft.print("SCORE");

  drawFooter();
  updateScore(0);

  gameOver = false;
  shuffleBag();
  nextType = getNextFromBag();
  newPiece();
}