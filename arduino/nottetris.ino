#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// --- SCREEN SETUP ---
#define TFT_CS     6
#define TFT_RST    9
#define TFT_DC     8

// --- BUTTON PINS ---
#define BTN_LEFT   A0
#define BTN_RIGHT  A1
#define BTN_UP     A2 // Rotate
#define BTN_DOWN   A3
#define BTN_DROP   A4
#define BTN_START  A5

// Use Standard Library (Green Tab fixes geometry)
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// --- LAYOUT CONSTANTS ---
#define BLOCK_SIZE 6   
#define BOARD_W    10  
#define BOARD_H    20  

// Layout Positions
#define BOARD_X    5   
#define BOARD_Y    5   
#define NEXT_X     80
#define NEXT_Y     20
#define SCORE_X    75
#define SCORE_Y    80
#define STATUS_X   10
#define STATUS_Y   130 // Moved up slightly to fit footer

// --- COLORS (BGR Fix) ---
#define C_BLACK   ST7735_BLACK
#define C_WHITE   ST7735_WHITE
#define C_GREY    0x5AEB
#define C_RED     ST7735_BLUE    
#define C_BLUE    ST7735_RED     
#define C_CYAN    ST7735_YELLOW  
#define C_YELLOW  ST7735_CYAN    
#define C_ORANGE  0x041F         
#define C_GREEN   ST7735_GREEN   
#define C_PURPLE  ST7735_MAGENTA 

// --- GLOBALS ---
int board[BOARD_W][BOARD_H]; 

const uint8_t shapes[7][4][4] = {
  {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}}, // I
  {{0,0,0,0},{0,2,2,0},{0,2,2,0},{0,0,0,0}}, // O
  {{0,0,0,0},{0,0,3,3},{0,3,3,0},{0,0,0,0}}, // S
  {{0,0,0,0},{0,4,4,0},{0,0,4,4},{0,0,0,0}}, // Z
  {{0,0,0,0},{0,5,0,0},{0,5,5,5},{0,0,0,0}}, // J
  {{0,0,0,0},{0,0,0,6},{0,6,6,6},{0,0,0,0}}, // L
  {{0,0,0,0},{0,0,7,0},{0,7,7,7},{0,0,0,0}}  // T
};

uint16_t colors[] = {C_BLACK, C_CYAN, C_YELLOW, C_GREEN, C_RED, C_BLUE, C_ORANGE, C_PURPLE};

int px, py, pRot, pType; 
int nextType; 
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
bool lastDropBtn = HIGH;

void setup() {
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_DROP, INPUT_PULLUP);
  pinMode(BTN_START, INPUT_PULLUP);

  tft.initR(INITR_GREENTAB); 
  tft.fillScreen(C_BLACK);
  tft.setRotation(0); 
  
  // UI Setup
  tft.drawRect(BOARD_X-2, BOARD_Y-2, (BOARD_W*BLOCK_SIZE)+4, (BOARD_H*BLOCK_SIZE)+4, C_WHITE);
  
  tft.setTextColor(C_WHITE);
  tft.setCursor(NEXT_X, NEXT_Y - 10);
  tft.print("NEXT");
  tft.drawRect(NEXT_X-4, NEXT_Y-4, 30, 30, C_GREY);

  tft.setCursor(SCORE_X, SCORE_Y - 10);
  tft.print("SCORE");
  
  // Footer Text
  drawFooter();

  updateScore(0);
  nextType = random(0, 7);
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
      tft.fillRect(STATUS_X, STATUS_Y, 100, 10, C_BLACK); // Clear once exiting
      redrawBoard(); // Redraw board to fix any text overlap
      drawPiece();
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
  int footerY = 142; // Bottom area
  
  tft.setTextSize(1);
  tft.setCursor(5, footerY);
  tft.setTextColor(C_WHITE);
  tft.print("hello, world // tetris");
  
  tft.setCursor(5, footerY + 10);
  tft.setTextColor(C_PURPLE);
  tft.print("[//] axel espinal");
  
  tft.setCursor(5, footerY + 20);
  tft.setTextColor(C_GREY);
  tft.print("2025-11-28");
}

void drawStatus(char* text, uint16_t color) {
  tft.fillRect(STATUS_X, STATUS_Y, 100, 10, C_BLACK);
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
  
  if (score > 500) dropInterval = 600;
  if (score > 1000) dropInterval = 400;
  if (score > 2000) dropInterval = 200;
}

void drawPreview() {
  tft.fillRect(NEXT_X, NEXT_Y, 24, 24, C_BLACK);
  uint16_t color = colors[nextType + 1];
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (shapes[nextType][j][i]) { 
        tft.fillRect(NEXT_X + i*6, NEXT_Y + j*6, 5, 5, color);
      }
    }
  }
}

// --- ENGINE LOGIC ---

void handleInput() {
  static unsigned long lastInput = 0;
  // Reduced debounce for responsiveness
  if (millis() - lastInput < 100) return; 

  if (!digitalRead(BTN_LEFT) && isValidMove(px - 1, py, pRot)) {
    movePiece(-1, 0); lastInput = millis();
  }
  if (!digitalRead(BTN_RIGHT) && isValidMove(px + 1, py, pRot)) {
    movePiece(1, 0); lastInput = millis();
  }
  if (!digitalRead(BTN_UP)) { 
    int newRot = (pRot + 1) % 4;
    if (isValidMove(px, py, newRot)) {
      erasePiece(); pRot = newRot; drawPiece(); lastInput = millis();
    }
  }
  if (!digitalRead(BTN_DOWN) && isValidMove(px, py + 1, pRot)) {
    movePiece(0, 1); updateScore(1); lastInput = millis() - 50; // Fast soft drop
  }
  
  // -- DROP ON RELEASE LOGIC --
  bool currDropBtn = digitalRead(BTN_DROP);
  // Trigger only when button goes from LOW (Pressed) to HIGH (Released)
  if (lastDropBtn == LOW && currDropBtn == HIGH) {
    while (isValidMove(px, py + 1, pRot)) { 
      movePiece(0, 1); 
      updateScore(2); 
      delay(2); 
    }
    lockPiece(); 
    lastInput = millis() + 300; // Delay to prevent accidental next-piece moves
  }
  lastDropBtn = currDropBtn;
}

// (Standard Helper Functions remain identical below)

void newPiece() {
  pType = nextType;
  nextType = random(0, 7);
  drawPreview(); 
  px = 3; py = 0; pRot = 0;
  if (!isValidMove(px, py, pRot)) gameOver = true;
  else drawPiece();
}

void lockPiece() {
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (getShape(i, j, pRot)) {
        if (py + j >= 0) board[px + i][py + j] = pType + 1;
      }
    }
  }
  checkLines(); newPiece();
}

void checkLines() {
  int linesCleared = 0;
  for (int y = BOARD_H - 1; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < BOARD_W; x++) if (board[x][y] == 0) full = false;
    if (full) {
      linesCleared++;
      tft.fillRect(BOARD_X, BOARD_Y + y*BLOCK_SIZE, BOARD_W*BLOCK_SIZE, BLOCK_SIZE, C_WHITE);
      delay(30);
      for (int ny = y; ny > 0; ny--) {
        for (int nx = 0; nx < BOARD_W; nx++) board[nx][ny] = board[nx][ny - 1];
      }
      redrawBoard(); y++; 
    }
  }
  if (linesCleared == 1) updateScore(40);
  else if (linesCleared == 2) updateScore(100);
  else if (linesCleared == 3) updateScore(300);
  else if (linesCleared == 4) updateScore(1200);
}

void movePiece(int dx, int dy) {
  erasePiece(); px += dx; py += dy; drawPiece();
}

bool isValidMove(int x, int y, int rot) {
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (getShape(i, j, rot)) {
        int nx = x + i; int ny = y + j;
        if (nx < 0 || nx >= BOARD_W || ny >= BOARD_H) return false;
        if (ny >= 0 && board[nx][ny] != 0) return false;
      }
    }
  }
  return true;
}

int getShape(int x, int y, int rot) {
  switch (rot) {
    case 0: return shapes[pType][y][x];
    case 1: return shapes[pType][3-x][y];
    case 2: return shapes[pType][3-y][3-x];
    case 3: return shapes[pType][x][3-y];
  }
  return 0;
}

void drawPiece() { drawPieceHelper(colors[pType + 1]); }
void erasePiece() { drawPieceHelper(C_BLACK); }
void drawPieceHelper(uint16_t color) {
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (getShape(i, j, pRot)) {
        int screenY = BOARD_Y + (py + j) * BLOCK_SIZE;
        if (screenY >= BOARD_Y) {
          tft.fillRect(BOARD_X + (px + i) * BLOCK_SIZE, screenY, BLOCK_SIZE-1, BLOCK_SIZE-1, color);
        }
      }
    }
  }
}

void redrawBoard() {
  tft.fillRect(BOARD_X, BOARD_Y, BOARD_W*BLOCK_SIZE, BOARD_H*BLOCK_SIZE, C_BLACK);
  for (int x = 0; x < BOARD_W; x++) {
    for (int y = 0; y < BOARD_H; y++) {
      if (board[x][y] > 0) tft.fillRect(BOARD_X + x*BLOCK_SIZE, BOARD_Y + y*BLOCK_SIZE, BLOCK_SIZE-1, BLOCK_SIZE-1, colors[board[x][y]]);
    }
  }
}

void resetGame() {
  for (int x = 0; x < BOARD_W; x++) for (int y = 0; y < BOARD_H; y++) board[x][y] = 0;
  score = 0;
  prevGameOver = false; // Reset the flag
  
  tft.fillScreen(C_BLACK);
  tft.drawRect(BOARD_X-2, BOARD_Y-2, (BOARD_W*BLOCK_SIZE)+4, (BOARD_H*BLOCK_SIZE)+4, C_WHITE);
  tft.setTextColor(C_WHITE);
  tft.setCursor(NEXT_X, NEXT_Y - 10); tft.print("NEXT");
  tft.drawRect(NEXT_X-4, NEXT_Y-4, 30, 30, C_GREY);
  tft.setCursor(SCORE_X, SCORE_Y - 10); tft.print("SCORE");
  
  drawFooter();
  updateScore(0);
  
  gameOver = false; 
  nextType = random(0, 7);
  newPiece();
}