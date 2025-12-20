#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// --- SCREEN SETUP ---
#define TFT_CS 6
#define TFT_RST 9
#define TFT_DC 8

// --- BUTTON PINS ---
#define BTN_LEFT A0
#define BTN_RIGHT A1
#define BTN_UP A2 // Rotate
#define BTN_DOWN A3
#define BTN_HOLD A4 // Reassigned from DROP
#define BTN_START A5

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
int board[BOARD_W][BOARD_H];

const uint8_t shapes[7][4][4] = {
    {{0, 0, 0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}}, // I
    {{0, 0, 0, 0}, {0, 2, 2, 0}, {0, 2, 2, 0}, {0, 0, 0, 0}}, // O
    {{0, 0, 0, 0}, {0, 0, 3, 3}, {0, 3, 3, 0}, {0, 0, 0, 0}}, // S
    {{0, 0, 0, 0}, {0, 4, 4, 0}, {0, 0, 4, 4}, {0, 0, 0, 0}}, // Z
    {{0, 0, 0, 0}, {0, 5, 0, 0}, {0, 5, 5, 5}, {0, 0, 0, 0}}, // J
    {{0, 0, 0, 0}, {0, 0, 0, 6}, {0, 6, 6, 6}, {0, 0, 0, 0}}, // L
    {{0, 0, 0, 0}, {0, 0, 7, 0}, {0, 7, 7, 7}, {0, 0, 0, 0}}  // T
};

uint16_t colors[] = {C_BLACK, C_CYAN, C_YELLOW, C_GREEN,
                     C_RED,   C_BLUE, C_ORANGE, C_PURPLE};

int px, py, pRot, pType;
int nextType;
int holdType = -1;
bool canHold = true;

// 7-bag randomizer
int bag[7];
int bagIndex = 7; // Start at 7 to trigger initial shuffle

// Forward declarations
void shuffleBag();
int getNextFromBag();
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
bool lastHoldBtn = HIGH;

void setup() {
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_HOLD, INPUT_PULLUP);
  pinMode(BTN_START, INPUT_PULLUP);

  tft.initR(INITR_GREENTAB);
  tft.fillScreen(C_BLACK);
  tft.setRotation(1);

  // --- START SCREEN FOR RNG SEEDING ---

  tft.fillRect(0, 0, 160, 128, C_BLACK); // Clear full 160x128 landscape

  // "(not) tetris"
  // "(not)" in Gray, " tetris" in White
  tft.setTextSize(1);
  // Center on 160px width: (160 - (12*6))/2 = 44
  int startX = 44;
  tft.setCursor(startX, 40);
  tft.setTextColor(C_GREY);
  tft.print("(not)");
  tft.setTextColor(C_WHITE);
  tft.print(" tetris");

  // "press any key to start"
  // Center on 160px: (160 - (22*6))/2 = 14
  tft.setCursor(14, 60);
  tft.print("press any key to start");

  // Footer (Tethered to bottom 128px)
  tft.setTextColor(C_GREY);
  // Center on 160px: (160 - (17*6))/2 = 29
  tft.setCursor(29, 108);
  tft.print("[//] axel espinal");

  // Center on 160px: (160 - (24*6))/2 = 8
  tft.setCursor(8, 118);
  tft.print("axelespinal.com/projects");

  // Wait for ANY key press
  while (true) {
    bool pressed = false;
    if (digitalRead(BTN_LEFT) == LOW)
      pressed = true;
    if (digitalRead(BTN_RIGHT) == LOW)
      pressed = true;
    if (digitalRead(BTN_UP) == LOW)
      pressed = true;
    if (digitalRead(BTN_DOWN) == LOW)
      pressed = true;
    if (digitalRead(BTN_HOLD) == LOW)
      pressed = true;
    if (digitalRead(BTN_START) == LOW)
      pressed = true;

    if (pressed) {
      randomSeed(micros()); // Seed with current time
      break;
    }
    delay(10);
  }

  // Initialize bag and get first piece (Fixing RNG bug)
  shuffleBag();
  nextType = getNextFromBag();

  // Clear screen and start game
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

  // Footer Text
  drawFooter();

  updateScore(0);
  newPiece();
}

void loop() {
  // --- STATE 1: GAME OVER ---
  if (gameOver) {
    if (!prevGameOver) { // Only draw ONCE
      drawStatus("GAME OVER", C_RED);
      prevGameOver = true;
    }
    // Check for Restart (On Release)
    bool currStartBtn = digitalRead(BTN_START);
    if (lastStartBtn == LOW && currStartBtn == HIGH) {
      resetGame();
    }
    lastStartBtn = currStartBtn;
    return;
  }

  // --- STATE 2: PAUSED ---
  // Check Pause Toggle (On Release)
  bool currStartBtn = digitalRead(BTN_START);
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

  delay(20);
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
  tft.print("(not) tetris");

  tft.setCursor(footerX, footerY + 10);
  tft.print("axel espinal");

  tft.setCursor(footerX, footerY + 20);
  tft.print("2025-12-20");
}

void drawStatus(char *text, uint16_t color) {
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

  uint16_t color = colors[nextType + 1];
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (shapes[nextType][j][i]) {
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

  uint16_t color = (canHold) ? colors[holdType + 1] : C_GREY;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (shapes[holdType][j][i]) {
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

  if (!digitalRead(BTN_LEFT) && isValidMove(px - 1, py, pRot)) {
    updatePiecePosition(px - 1, py, pRot);
    lastInput = millis();
  }
  if (!digitalRead(BTN_RIGHT) && isValidMove(px + 1, py, pRot)) {
    updatePiecePosition(px + 1, py, pRot);
    lastInput = millis();
  }
  // -- ROTATE ON RELEASE LOGIC --
  static bool lastUpBtn = HIGH;
  bool currUpBtn = digitalRead(BTN_UP);
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
  if (!digitalRead(BTN_DOWN) && isValidMove(px, py + 1, pRot)) {
    updatePiecePosition(px, py + 1, pRot);
    updateScore(1);
    lastInput = millis() - 50;
  }

  // -- HOLD ON RELEASE LOGIC --
  bool currHoldBtn = digitalRead(BTN_HOLD);
  if (lastHoldBtn == LOW && currHoldBtn == HIGH) {
    holdPiece();
  }
  lastHoldBtn = currHoldBtn;
}

// (Standard Helper Functions remain identical below)

void newPiece() {
  pType = nextType;
  nextType = getNextFromBag();
  drawPreview();
  px = 3;
  py = 0;
  pRot = 0;

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
  int oldGhostY = getGhostY(px, py, pRot);
  int newGhostY = getGhostY(newX, newY, newRot);

  // Re-draw ghost ONLY if it moved or if the piece's X/Rot changed (which
  // changes ghost's X/shape)
  if (oldGhostY != newGhostY || px != newX || pRot != newRot) {
    eraseGhost(px, oldGhostY, pRot);
    drawGhost(newX, newGhostY, newRot);
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
  drawPieceHelper(newX, newY, newRot, colors[pType + 1]);

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
  switch (rot) {
  case 0:
    return shapes[pType][y][x];
  case 1:
    return shapes[pType][3 - x][y];
  case 2:
    return shapes[pType][3 - y][3 - x];
  case 3:
    return shapes[pType][x][3 - y];
  }
  return 0;
}

// Basic helpers still needed for initialization/locking
void drawPiece() {
  drawGhost(px, getGhostY(px, py, pRot), pRot);
  drawPieceHelper(px, py, pRot, colors[pType + 1]);
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
                         BLOCK_SIZE - 1, colors[board[bx][by]]);
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
                     BLOCK_SIZE - 1, BLOCK_SIZE - 1, colors[board[x][y]]);
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