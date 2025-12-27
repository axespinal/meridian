#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

/*
 snake.ino | part of project meridian

 not metal gear snake

 [//] axelespinal.com/projects

 last updated: 2025-12-26
*/

// --- SCREEN SETUP ---
#define TFT_CS 6
#define TFT_RST 9
#define TFT_DC 8

// --- BUTTON PINS ---
#define BTN_LEFT_BIT 0
#define BTN_RIGHT_BIT 1
#define BTN_UP_BIT 2
#define BTN_DOWN_BIT 3
#define BTN_MENU_BIT 4
#define BTN_START_BIT 5

#define IS_PRESSED(bit) (!(PINC & (1 << bit)))

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// --- LAYOUT CONSTANTS ---
#define BLOCK_SIZE 8
#define GRID_W 20 // 160 / 8
#define GRID_H 16 // 128 / 8

// --- COLORS ---
#define C_BLACK ST7735_BLACK
#define C_WHITE ST7735_WHITE
#define C_GREY 0x5AEB
#define C_RED ST7735_BLUE // BGR Fix
#define C_GREEN ST7735_GREEN
#define C_BLUE ST7735_RED // BGR Fix

// --- GLOBALS ---

// Circular Buffer for Snake Body
// Size = Total Grid Cells = 320
// Using uint8_t saves RAM.
// snakeBuffer[i] stores Direction to next segment? No, let's store Positions.
// To optimize properly:
// Storing X and Y separately is fine.
// Circular Buffer: headIdx points to current head, tailIdx points to current
// tail. When moving:
// 1. increment headIdx (wrap around).
// 2. Put new coordinate at headIdx.
// 3. If NOT eating: increment tailIdx (wrap around) -> this effectively
// 'removes' the tail.
//    If eating: do NOT increment tailIdx -> length increases.

uint8_t snakeX[GRID_W * GRID_H];
uint8_t snakeY[GRID_W * GRID_H];
uint16_t headIdx = 0;
uint16_t tailIdx = 0;
uint16_t snakeLen = 3;

// Grid Map for O(1) Collision & Food Spawn
// 0 = Empty, 1 = Snake Body, 2 = Food
uint8_t grid[GRID_W][GRID_H];

// Direction: 0=Right, 1=Down, 2=Left, 3=Up
uint8_t dir = 0;
uint8_t nextDir = 0;

uint8_t foodX, foodY;
long score = 0;
bool gameOver = false;
bool paused = false;
unsigned long lastMoveTime = 0;
int moveInterval = 150;

// Input State
bool lastStartBtn = HIGH;
bool lastMenuBtn = HIGH;

// Prototypes
void spawnFood();
void drawBlock(uint8_t x, uint8_t y, uint16_t color);
void showStartScreen();
void resetGame();
void doGameOver();
void updateGame();
void setGrid(uint8_t x, uint8_t y, uint8_t val);

void setup() {
  for (int i = 0; i <= 5; i++) {
    pinMode(A0 + i, INPUT_PULLUP);
  }
  tft.initR(INITR_GREENTAB);
  tft.fillScreen(C_BLACK);
  tft.setRotation(1);

  showStartScreen();
}

void showStartScreen() {
  // Wait for Release (Latch)
  while ((~PINC & 0x3F) != 0) {
    delay(10);
  }

  tft.fillScreen(C_BLACK);

  // Title: "snake" (lowercase, size 1, same as press text)
  tft.setTextSize(1);
  tft.setTextColor(C_GREEN);
  // Center logic: 160 width. "snake" is 5 chars * 6px = 30px. (160-30)/2 = 65
  tft.setCursor(65, 50);
  tft.print(F("snake"));

  tft.setTextColor(C_WHITE);
  // "press to start" is 14 chars * 6px = 84px. (160-84)/2 = 38
  tft.setCursor(38, 65);
  tft.print(F("press to start"));

  // Footer
  tft.setTextColor(C_GREY);
  tft.setCursor(29, 108);
  tft.print(F("[//] axel espinal"));
  tft.setCursor(8, 118);
  tft.print(F("axelespinal.com/projects"));

  long seedCounter = 0;
  while (true) {
    seedCounter++;
    if ((~PINC & 0x3F) != 0) {
      randomSeed(micros() + seedCounter);
      break;
    }
    delay(10);
  }

  // resetGame will be called in loop or here?
  // better here to start fresh.
  resetGame();
}

void loop() {
  // Determine input first
  if (gameOver) {
    if (IS_PRESSED(BTN_START_BIT) &&
        lastStartBtn == LOW) { // On Press? Or Release?
      // Simple Game Over logic
      resetGame();
      lastStartBtn = HIGH; // Prevent immediate pause
      return;
    }
  }

  // Input Handling
  if (IS_PRESSED(BTN_LEFT_BIT) && dir != 0)
    nextDir = 2;
  // Prevent reverse direction? Yes, handled by `&& dir != 0` etc.
  // wait, if im going RIGHT (0), I can't go LEFT (2).
  // if im going DOWN (1), I can't go UP (3).
  // if im going LEFT (2), I can't go RIGHT (0).
  // if im going UP (3), I can't go DOWN (1).
  if (IS_PRESSED(BTN_RIGHT_BIT) && dir != 2)
    nextDir = 0;
  if (IS_PRESSED(BTN_UP_BIT) && dir != 1)
    nextDir = 3;
  if (IS_PRESSED(BTN_DOWN_BIT) && dir != 3)
    nextDir = 1;

  // Menu Button (Return to start)
  bool currMenuBtn = IS_PRESSED(BTN_MENU_BIT);
  // Latch logic is effectively: trigger on Press, but we handled latch in
  // StartScreen. Here we just want to go there.
  if (lastMenuBtn == LOW && currMenuBtn == HIGH) {
    showStartScreen(); // This blocks until user presses a key
    lastMenuBtn = currMenuBtn;
    return;
  }
  lastMenuBtn = currMenuBtn;

  // Pause
  bool currStartBtn = IS_PRESSED(BTN_START_BIT);
  if (lastStartBtn == LOW && currStartBtn == HIGH) {
    if (!gameOver) {
      paused = !paused;
      if (paused) {
        tft.setCursor(62, 60);
        tft.setTextColor(C_WHITE, C_BLACK);
        tft.print(F("PAUSED"));
      } else {
        tft.fillRect(60, 60, 40, 10, C_BLACK);
        // Redraw food/snake to be sure
        drawBlock(foodX, foodY, C_RED);
        // Redraw head/tail from buffer is tricky without iterating.
        // We can just iterate buffer.
        int tempIdx = tailIdx;
        for (int i = 0; i < snakeLen; i++) {
          drawBlock(snakeX[tempIdx], snakeY[tempIdx],
                    (i == snakeLen - 1) ? C_GREEN : C_WHITE);
          tempIdx = (tempIdx + 1) % (GRID_W * GRID_H);
        }
      }
    }
  }
  lastStartBtn = currStartBtn;

  if (paused || gameOver) {
    delay(10);
    return; // Idle
  }

  if (millis() - lastMoveTime > moveInterval) {
    lastMoveTime = millis();
    updateGame();
  }
}

void updateGame() {
  // 1. Update Direction
  // Only allow direction change if it's not 180 turn directly (already checked
  // in input) But we need to check if we already moved in that direction?
  // Actually, simple input check is mostly fine for snake.
  dir = nextDir;

  uint8_t newX = snakeX[headIdx];
  uint8_t newY = snakeY[headIdx];

  switch (dir) {
  case 0:
    newX++;
    break;
  case 1:
    newY++;
    break;
  case 2:
    newX--;
    break;
  case 3:
    newY--;
    break;
  }

  // 2. Collision Check (O(1))
  // Wall
  if (newX >= GRID_W || newX < 0 || newY >= GRID_H || newY < 0) {
    doGameOver();
    return;
  }
  // Self (Grid Check)
  // Note: If we are not growing, the tail will move, so hitting the tail is
  // safe? Actually snake logic usually says hitting the tail segment that IS
  // ABOUT TO MOVE is safe. But simplistic grid check says 1. Let's check:
  if (grid[newX][newY] == 1) {
    // Safe only if it's the tail AND we're not eating.
    bool isTail = (newX == snakeX[tailIdx] && newY == snakeY[tailIdx]);
    // But we handle tail clearing logic *after* food check.
    // Standard snake: Hitting tail is valid iff tail moves away this turn.
    // So, if not eating, tail moves.
    if (isTail) {
      // Safe.
    } else {
      doGameOver();
      return;
    }
  }

  // 3. Move & Food
  // Advance Head
  headIdx = (headIdx + 1) % (GRID_W * GRID_H);
  snakeX[headIdx] = newX;
  snakeY[headIdx] = newY;

  bool ate = (grid[newX][newY] == 2); // Check map for food

  // Update Grid Map (New Head)
  grid[newX][newY] = 1;
  drawBlock(newX, newY, C_GREEN);

  // Old Head becomes Body
  // Determine previous head index
  int prevHeadIdx = (headIdx == 0) ? (GRID_W * GRID_H - 1) : headIdx - 1;
  if (snakeLen > 1)
    drawBlock(snakeX[prevHeadIdx], snakeY[prevHeadIdx], C_WHITE);

  if (ate) {
    snakeLen++;
    score += 10;
    if (moveInterval > 50)
      moveInterval -= 2;
    spawnFood();
    // Tail does not move
  } else {
    // Tail moves: Clear old tail from Grid and Screen
    uint8_t tX = snakeX[tailIdx];
    uint8_t tY = snakeY[tailIdx];
    grid[tX][tY] = 0;
    drawBlock(tX, tY, C_BLACK);

    tailIdx = (tailIdx + 1) % (GRID_W * GRID_H);
  }

  // Draw Score
  tft.setCursor(2, 2);
  tft.setTextColor(C_GREY, C_BLACK); // Opaque
  tft.print(score);
}

void spawnFood() {
  // O(1) Check? No, random Spawn is probabilistic.
  // But checking validity is O(1) via Grid.
  while (true) {
    uint8_t fx = random(0, GRID_W);
    uint8_t fy = random(0, GRID_H);
    if (grid[fx][fy] == 0) {
      foodX = fx;
      foodY = fy;
      grid[fx][fy] = 2;
      drawBlock(fx, fy, C_RED);
      break;
    }
  }
}

void drawBlock(uint8_t x, uint8_t y, uint16_t color) {
  tft.fillRect(x * BLOCK_SIZE, y * BLOCK_SIZE, BLOCK_SIZE - 1, BLOCK_SIZE - 1,
               color);
}

void resetGame() {
  tft.fillScreen(C_BLACK);
  // Clear Grid
  memset(grid, 0, sizeof(grid));

  snakeLen = 3;
  headIdx = 2; // 0, 1, 2
  tailIdx = 0;

  // Init Snake
  snakeX[0] = 8;
  snakeY[0] = 8;
  snakeX[1] = 9;
  snakeY[1] = 8;
  snakeX[2] = 10;
  snakeY[2] = 8;

  for (int i = 0; i < 3; i++) {
    grid[snakeX[i]][snakeY[i]] = 1;
    drawBlock(snakeX[i], snakeY[i], (i == 2) ? C_GREEN : C_WHITE);
  }

  dir = 0;
  nextDir = 0;
  score = 0;
  moveInterval = 150;
  gameOver = false;
  paused = false;

  spawnFood();
}

void doGameOver() {
  gameOver = true;
  tft.fillRect(40, 50, 80, 20, C_BLACK);
  tft.drawRect(40, 50, 80, 20, C_RED);
  tft.setCursor(52, 56);
  tft.setTextColor(C_RED);
  tft.print(F("GAME OVER"));
}
