#include "tft.h"

/*
 firmware.ino | part of project meridian

 Unified firmware for NotTetris and Snake.
 Uses custom minimal TFT driver for reduced flash usage.

 [//] axelespinal.com/projects

 last updated: 2025-12-27
*/

#define C_RED 0xF800  // RGB565 Red
#define C_BLUE 0x001F // RGB565 Blue
#define C_GREEN 0x07E0
#define C_CYAN 0x07FF   // RGB565 Cyan (Standard G+B)
#define C_YELLOW 0xFFE0 // RGB565 Yellow (Standard R+G)
#define C_ORANGE 0xFD20 // RGB565 Orange (Standard)

// Port C (Analog)
#define BTN_LEFT_BIT 0
#define BTN_RIGHT_BIT 1
#define BTN_UP_BIT 2
#define BTN_DOWN_BIT 3
#define BTN_MENU_BIT 4
#define BTN_START_BIT 5

// Port D (Digital)
#define BTN_A_BIT 2 // D2
#define BTN_B_BIT 5 // D5 (Physical Pin 11)

#define IS_PRESSED_C(bit) (!(PINC & (1 << bit)))
#define IS_PRESSED_D(bit) (!(PIND & (1 << bit)))

// --- SHARED CONSTANTS ---
#define SCREEN_W 160
#define SCREEN_H 128
#define BLOCK_SIZE_SNAKE 8
#define BLOCK_SIZE_TETRIS 6

// --- MEMORY STRUCTURES ---

struct SnakeVars {
  uint8_t x[160 / 8 * 128 / 8]; // 320 bytes
  uint8_t y[160 / 8 * 128 / 8]; // 320 bytes
  uint8_t grid[20][16];         // 320 bytes
  uint16_t headIdx;
  uint16_t tailIdx;
  uint16_t len;
  uint8_t dir;
  uint8_t nextDir;
  uint8_t foodX;
  uint8_t foodY;
  long score;
  bool gameOver;
  bool paused;
  unsigned long lastMoveTime;
  int moveInterval;
};

struct TetrisVars {
  int8_t board[10][20]; // 200 bytes
  int8_t bag[7];
  int8_t bagIndex;
  int8_t px, py, pRot, pType;
  int8_t nextType;
  int8_t holdType;
  int8_t lastGhostY;
  bool canHold;
  long score;
  unsigned long lastDropTime;
  int dropInterval;
  bool gameOver;
  bool paused;
  bool prevPaused;
  bool prevGameOver;

  // Input State for Detection
  bool lastBtnA;
  bool lastBtnB;
  bool lastBtnStart;
};

struct AboutVars {
  float angleX, angleY, angleZ;
  int oldProjected2D[8][2];
  bool firstFrame;
};

union GameMemory {
  SnakeVars snake;
  TetrisVars tetris;
  AboutVars about;
};

GameMemory game;
enum AppState { STATE_MENU, STATE_SNAKE, STATE_TETRIS, STATE_ABOUT };
AppState currentState = STATE_MENU;

// Global Inputs to debounce across apps
bool lastMenuBtn = HIGH;
bool lastStartBtn = HIGH;

// --- PROTOTYPES ---
void runMenu();
void setupSnake();
void loopSnake();
void setupTetris();
void loopTetris();
void setupAbout();
void loopAbout();
bool checkGlobalMenu();

// --- MAIN SETUP/LOOP ---

void setup() {
  // Init Pins
  for (int i = 0; i <= 5; i++)
    pinMode(A0 + i, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);

  tftInit();
  tftFillScreen(C_BLACK);
}

void loop() {
  switch (currentState) {
  case STATE_MENU:
    runMenu();
    break;
  case STATE_SNAKE:
    loopSnake();
    break;
  case STATE_TETRIS:
    loopTetris();
    break;
  case STATE_ABOUT:
    loopAbout();
    break;
  }
}

// --- GLOBAL HELPERS ---

// Returns true if we should exit to menu
bool checkGlobalMenu() {
  bool curr = IS_PRESSED_C(BTN_MENU_BIT);
  if (lastMenuBtn == LOW && curr == HIGH) {
    currentState = STATE_MENU;
    lastMenuBtn = curr;
    return true;
  }
  lastMenuBtn = curr;
  return false;
}

// --- MAIN MENU ---

void runMenu() {
  static int8_t selection = 0;
  static bool needsDraw = true;

  if (needsDraw) {
    tftFillScreen(C_BLACK);
    tftSetTextSize(1);

    // Header
    tftSetCursor(2, 5);
    tftSetTextColor(C_WHITE);
    tftPrint(F("[//] meridian"));
    tftDrawFastHLine(0, 15, 160, C_WHITE);

    // Options
    tftSetCursor(25, 30);
    tftSetTextColor(C_GREY);
    tftPrint(F("(not)"));
    tftSetTextColor(C_WHITE);
    tftPrint(F(" tetris"));

    tftSetCursor(25, 48);
    tftPrint(F("snake"));

    tftSetCursor(25, 66);
    tftPrint(F("about meridian"));

    // Footer
    int fx = 10, fy = 105;
    tftSetCursor(fx, fy);
    tftSetTextColor(C_GREY);
    tftPrint(F("press "));

    int ax = fx + 42, ay = fy + 3;
    tftDrawCircle(ax, ay, 6, C_GREY);
    tftSetCursor(ax - 2, ay - 3);
    tftSetTextSize(1);
    tftSetTextColor(C_GREY);
    tftPrint(F("A"));

    tftSetCursor(ax + 8, fy);
    tftPrint(F("|-> to select"));

    tftSetCursor(8, 118);
    tftPrint(F("axelespinal.com/projects"));

    needsDraw = false;
  }

  // Draw Cursor
  tftSetCursor(15, 30);
  tftSetTextColor((selection == 0) ? C_CYAN : C_BLACK);
  tftPrint(F(">"));

  tftSetCursor(15, 48);
  tftSetTextColor((selection == 1) ? C_CYAN : C_BLACK);
  tftPrint(F(">"));

  tftSetCursor(15, 66);
  tftSetTextColor((selection == 2) ? C_CYAN : C_BLACK);
  tftPrint(F(">"));

  // Input
  static unsigned long lastInput = 0;
  if (millis() - lastInput > 150) {
    if (IS_PRESSED_C(BTN_UP_BIT)) {
      selection--;
      if (selection < 0)
        selection = 2;
      lastInput = millis();
    }
    if (IS_PRESSED_C(BTN_DOWN_BIT)) {
      selection++;
      if (selection > 2)
        selection = 0;
      lastInput = millis();
    }

    if (IS_PRESSED_C(BTN_START_BIT) || IS_PRESSED_D(BTN_A_BIT) ||
        IS_PRESSED_C(BTN_RIGHT_BIT)) {
      tftFillScreen(C_BLACK);
      if (selection == 0) {
        currentState = STATE_TETRIS;
        setupTetris();
      } else if (selection == 1) {
        currentState = STATE_SNAKE;
        setupSnake();
      } else {
        currentState = STATE_ABOUT;
        setupAbout();
      }
      needsDraw = true;
      lastInput = millis() + 500;
    }
  }
}

// ======================================================================================
// === SNAKE IMPLEMENTATION ===
// ======================================================================================

void drawSnakeBlock(uint8_t x, uint8_t y, uint16_t color) {
  tftFillRect(x * BLOCK_SIZE_SNAKE, y * BLOCK_SIZE_SNAKE, BLOCK_SIZE_SNAKE - 1,
              BLOCK_SIZE_SNAKE - 1, color);
}

void spawnSnakeFood() {
  while (true) {
    uint8_t fx = random(0, 20);
    uint8_t fy = random(0, 16);
    if (game.snake.grid[fx][fy] == 0) {
      game.snake.foodX = fx;
      game.snake.foodY = fy;
      game.snake.grid[fx][fy] = 2;
      drawSnakeBlock(fx, fy, C_RED);
      break;
    }
  }
}

void resetSnakeGame() {
  tftFillScreen(C_BLACK);
  memset(game.snake.grid, 0, sizeof(game.snake.grid));

  game.snake.len = 3;
  game.snake.headIdx = 2;
  game.snake.tailIdx = 0;

  game.snake.x[0] = 8;
  game.snake.y[0] = 8;
  game.snake.x[1] = 9;
  game.snake.y[1] = 8;
  game.snake.x[2] = 10;
  game.snake.y[2] = 8;

  for (int i = 0; i < 3; i++) {
    game.snake.grid[game.snake.x[i]][game.snake.y[i]] = 1;
    drawSnakeBlock(game.snake.x[i], game.snake.y[i],
                   (i == 2) ? C_GREEN : C_WHITE);
  }

  game.snake.dir = 0;
  game.snake.nextDir = 0;
  game.snake.score = 0;
  game.snake.moveInterval = 150;
  game.snake.gameOver = false;
  game.snake.paused = false;

  spawnSnakeFood();
}

void setupSnake() {
  tftFillScreen(C_BLACK);
  while ((~PINC & 0x3F) != 0) {
    delay(10);
  }

  tftSetTextSize(1);
  tftSetTextColor(C_GREEN);
  tftSetCursor(65, 50);
  tftPrint(F("snake"));

  tftSetTextColor(C_WHITE);
  tftSetCursor(38, 65);
  tftPrint(F("press to start"));

  tftSetTextColor(C_GREY);
  tftSetCursor(29, 108);
  tftPrint(F("[//] axel espinal"));
  tftSetCursor(8, 118);
  tftPrint(F("axelespinal.com/projects"));

  while (true) {
    if (checkGlobalMenu())
      return;
    // Start on any key except Menu (Bit 4)
    if (((~PINC & 0x3F) & ~(1 << BTN_MENU_BIT)) != 0 || !digitalRead(2) ||
        !digitalRead(5)) {
      randomSeed(micros());
      break;
    }
    delay(10);
  }

  resetSnakeGame();
}

void loopSnake() {
  if (checkGlobalMenu())
    return;

  if (IS_PRESSED_C(BTN_LEFT_BIT) && game.snake.dir != 0)
    game.snake.nextDir = 2;
  if (IS_PRESSED_C(BTN_RIGHT_BIT) && game.snake.dir != 2)
    game.snake.nextDir = 0;
  if (IS_PRESSED_C(BTN_UP_BIT) && game.snake.dir != 1)
    game.snake.nextDir = 3;
  if (IS_PRESSED_C(BTN_DOWN_BIT) && game.snake.dir != 3)
    game.snake.nextDir = 1;

  bool currStart = IS_PRESSED_C(BTN_START_BIT);
  if (lastStartBtn == LOW && currStart == HIGH) {
    if (game.snake.gameOver) {
      resetSnakeGame();
    } else {
      game.snake.paused = !game.snake.paused;
      if (game.snake.paused) {
        tftSetCursor(62, 60);
        tftSetTextColor(C_WHITE, C_BLACK);
        tftPrint(F("PAUSED"));
      } else {
        tftFillRect(60, 60, 40, 10, C_BLACK);
        drawSnakeBlock(game.snake.foodX, game.snake.foodY, C_RED);
        int idx = game.snake.tailIdx;
        for (int i = 0; i < game.snake.len; i++) {
          drawSnakeBlock(game.snake.x[idx], game.snake.y[idx],
                         (i == game.snake.len - 1) ? C_GREEN : C_WHITE);
          idx = (idx + 1) % 320;
        }
      }
    }
  }
  lastStartBtn = currStart;

  if (game.snake.paused || game.snake.gameOver) {
    delay(10);
    return;
  }

  if (millis() - game.snake.lastMoveTime > game.snake.moveInterval) {
    game.snake.lastMoveTime = millis();

    game.snake.dir = game.snake.nextDir;
    uint8_t nx = game.snake.x[game.snake.headIdx];
    uint8_t ny = game.snake.y[game.snake.headIdx];

    switch (game.snake.dir) {
    case 0:
      nx++;
      break;
    case 1:
      ny++;
      break;
    case 2:
      nx--;
      break;
    case 3:
      ny--;
      break;
    }

    if (nx >= 20 || nx < 0 || ny >= 16 || ny < 0 ||
        (game.snake.grid[nx][ny] == 1 &&
         !(nx == game.snake.x[game.snake.tailIdx] &&
           ny == game.snake.y[game.snake.tailIdx]))) {
      game.snake.gameOver = true;
      tftFillRect(40, 50, 80, 20, C_BLACK);
      tftDrawRect(40, 50, 80, 20, C_RED);
      tftSetCursor(52, 56);
      tftSetTextColor(C_RED);
      tftPrint(F("GAME OVER"));
      return;
    }

    game.snake.headIdx = (game.snake.headIdx + 1) % 320;
    game.snake.x[game.snake.headIdx] = nx;
    game.snake.y[game.snake.headIdx] = ny;

    bool ate = (game.snake.grid[nx][ny] == 2);
    game.snake.grid[nx][ny] = 1;
    drawSnakeBlock(nx, ny, C_GREEN);

    int prevHead = (game.snake.headIdx == 0) ? 319 : game.snake.headIdx - 1;
    if (game.snake.len > 1)
      drawSnakeBlock(game.snake.x[prevHead], game.snake.y[prevHead], C_WHITE);

    if (ate) {
      game.snake.len++;
      game.snake.score += 10;
      if (game.snake.moveInterval > 50)
        game.snake.moveInterval -= 2;
      spawnSnakeFood();
    } else {
      uint8_t tx = game.snake.x[game.snake.tailIdx];
      uint8_t ty = game.snake.y[game.snake.tailIdx];
      game.snake.grid[tx][ty] = 0;
      drawSnakeBlock(tx, ty, C_BLACK);
      game.snake.tailIdx = (game.snake.tailIdx + 1) % 320;
    }

    tftSetCursor(2, 2);
    tftSetTextColor(C_GREY, C_BLACK);
    tftPrint(game.snake.score);
  }
}

// ======================================================================================
// === NOT TETRIS IMPLEMENTATION ===
// ======================================================================================

const uint16_t shapes[7][4] PROGMEM = {
    {0x00F0, 0x4444, 0x0F00, 0x2222}, {0x0660, 0x0660, 0x0660, 0x0660},
    {0x06C0, 0x4620, 0x0360, 0x0462}, {0x0C60, 0x2640, 0x0630, 0x0264},
    {0x0E20, 0x2260, 0x0470, 0x0644}, {0x0E80, 0x6220, 0x0170, 0x0446},
    {0x0E40, 0x2620, 0x0270, 0x0464},
};
const uint16_t tetrisColors[] PROGMEM = {C_BLACK, C_CYAN, C_YELLOW, C_GREEN,
                                         C_RED,   C_BLUE, C_ORANGE, C_PURPLE};

int getTetShape(int type, int x, int y, int rot) {
  uint16_t s = pgm_read_word(&shapes[type][rot]);
  return (s >> (y * 4 + x)) & 1;
}

bool isValidTetMove(int x, int y, int rot) {
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (getTetShape(game.tetris.pType, i, j, rot)) {
        int nx = x + i;
        int ny = y + j;
        if (nx < 0 || nx >= 10 || ny >= 20)
          return false;
        if (ny >= 0 && game.tetris.board[nx][ny] != 0)
          return false;
      }
    }
  }
  return true;
}

int getTetGhostY(int x, int y, int rot) {
  int gy = y;
  while (isValidTetMove(x, gy + 1, rot))
    gy++;
  return gy;
}

void drawTetBlock(int x, int y, uint16_t color) {
  if (y < 0)
    return;
  tftFillRect(5 + x * 6, 4 + y * 6, 5, 5, color);
}

void drawTetGhost(int x, int y, int rot) {
  if (y == game.tetris.py && x == game.tetris.px)
    return;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (getTetShape(game.tetris.pType, i, j, rot)) {
        int sy = 4 + (y + j) * 6;
        int sx = 5 + (x + i) * 6;
        if (sy >= 4 && sy < 128)
          tftDrawRect(sx, sy, 5, 5, C_GREY);
      }
    }
  }
}

void eraseTetGhost(int x, int y, int rot) {
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (getTetShape(game.tetris.pType, i, j, rot)) {
        int bx = x + i;
        int by = y + j;
        if (bx >= 0 && bx < 10 && by >= 0 && by < 20) {
          if (game.tetris.board[bx][by])
            drawTetBlock(
                bx, by,
                pgm_read_word(&tetrisColors[game.tetris.board[bx][by]]));
          else {
            int sx = 5 + bx * 6;
            int sy = 4 + by * 6;
            if (sy >= 4)
              tftDrawRect(sx, sy, 5, 5, C_BLACK);
          }
        }
      }
    }
  }
}

void drawTetPiece(bool eraseOld, int x, int y, int rot) {
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (getTetShape(game.tetris.pType, i, j, rot)) {
        if (!eraseOld)
          drawTetBlock(x + i, y + j,
                       pgm_read_word(&tetrisColors[game.tetris.pType + 1]));
      }
    }
  }
}

void updateTetPiecePos(int nx, int ny, int nr) {
  int oldGy = game.tetris.lastGhostY;
  int newGy = (nx == game.tetris.px && nr == game.tetris.pRot)
                  ? oldGy
                  : getTetGhostY(nx, ny, nr);

  if (oldGy != newGy || game.tetris.px != nx || game.tetris.pRot != nr) {
    eraseTetGhost(game.tetris.px, oldGy, game.tetris.pRot);
    drawTetGhost(nx, newGy, nr);
    game.tetris.lastGhostY = newGy;
  }

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (getTetShape(game.tetris.pType, i, j, game.tetris.pRot)) {
        int ox = game.tetris.px + i;
        int oy = game.tetris.py + j;
        int lx = ox - nx;
        int ly = oy - ny;
        bool kept = false;
        if (lx >= 0 && lx < 4 && ly >= 0 && ly < 4 &&
            getTetShape(game.tetris.pType, lx, ly, nr))
          kept = true;

        if (!kept) {
          int gx = ox - nx;
          int gy = oy - newGy;
          bool isG = (gx >= 0 && gx < 4 && gy >= 0 && gy < 4 &&
                      getTetShape(game.tetris.pType, gx, gy, nr));
          if (isG)
            tftDrawRect(5 + ox * 6, 4 + oy * 6, 5, 5, C_GREY);
          else
            tftFillRect(5 + ox * 6, 4 + oy * 6, 5, 5, C_BLACK);
        }
      }
    }
  }

  game.tetris.px = nx;
  game.tetris.py = ny;
  game.tetris.pRot = nr;
  drawTetPiece(false, nx, ny, nr);
}

void spawnTetPiece() {
  game.tetris.pType = game.tetris.nextType;
  if (game.tetris.bagIndex >= 7) {
    for (int i = 0; i < 7; i++)
      game.tetris.bag[i] = i;
    for (int i = 6; i > 0; i--) {
      int j = random(0, i + 1);
      int t = game.tetris.bag[i];
      game.tetris.bag[i] = game.tetris.bag[j];
      game.tetris.bag[j] = t;
    }
    game.tetris.bagIndex = 0;
  }
  game.tetris.nextType = game.tetris.bag[game.tetris.bagIndex++];

  tftFillRect(75 + 1, 5 + 10, 28, 28, C_BLACK);
  uint16_t c = pgm_read_word(&tetrisColors[game.tetris.nextType + 1]);
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      if (getTetShape(game.tetris.nextType, i, j, 0))
        tftFillRect(75 + 4 + i * 5, 5 + 14 + j * 5, 4, 4, c);

  game.tetris.px = 3;
  game.tetris.py = 0;
  game.tetris.pRot = 0;
  game.tetris.lastGhostY = getTetGhostY(3, 0, 0);

  if (!isValidTetMove(3, 0, 0))
    game.tetris.gameOver = true;
  else {
    drawTetGhost(3, game.tetris.lastGhostY, 0);
    drawTetPiece(false, 3, 0, 0);
  }
}

void updateTetScore(long added) {
  game.tetris.score += added;
  tftFillRect(75, 55, 50, 10, C_BLACK);
  tftSetCursor(75, 55);
  tftSetTextColor(C_WHITE);
  tftPrint(game.tetris.score);
}

void lockTetPiece() {
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      if (getTetShape(game.tetris.pType, i, j, game.tetris.pRot))
        if (game.tetris.py + j >= 0)
          game.tetris.board[game.tetris.px + i][game.tetris.py + j] =
              game.tetris.pType + 1;

  int lines = 0;
  for (int y = 19; y >= 0; y--) {
    bool f = true;
    for (int x = 0; x < 10; x++)
      if (!game.tetris.board[x][y])
        f = false;
    if (f) {
      lines++;
      tftFillRect(5, 4 + y * 6, 60, 6, C_WHITE);
      delay(20);
      for (int k = y; k > 0; k--)
        for (int x = 0; x < 10; x++)
          game.tetris.board[x][k] = game.tetris.board[x][k - 1];
      y++;
    }
  }
  if (lines > 0) {
    int sc = (lines == 1) ? 40 : (lines == 2) ? 100 : (lines == 3) ? 300 : 1200;
    updateTetScore(sc);

    tftFillRect(5, 4, 60, 120, C_BLACK);
    for (int x = 0; x < 10; x++)
      for (int y = 0; y < 20; y++)
        if (game.tetris.board[x][y])
          drawTetBlock(x, y,
                       pgm_read_word(&tetrisColors[game.tetris.board[x][y]]));
  }

  game.tetris.canHold = true;
  tftFillRect(116, 15, 28, 28, C_BLACK);
  if (game.tetris.holdType != -1) {
    uint16_t c = pgm_read_word(&tetrisColors[game.tetris.holdType + 1]);
    for (int i = 0; i < 4; i++)
      for (int j = 0; j < 4; j++)
        if (getTetShape(game.tetris.holdType, i, j, 0))
          tftFillRect(119 + i * 5, 19 + j * 5, 4, 4, c);
  }

  spawnTetPiece();
}

void setupTetris() {
  tftFillScreen(C_BLACK);
  while ((~PINC & 0x3F) != 0) {
    delay(10);
  }

  tftSetTextSize(1);
  tftSetCursor(44, 40);
  tftSetTextColor(C_GREY);
  tftPrint(F("(not)"));
  tftSetTextColor(C_WHITE);
  tftPrint(F(" tetris"));
  tftSetCursor(14, 60);
  tftPrint(F("press any key to start"));
  tftSetTextColor(C_GREY);
  tftSetCursor(29, 108);
  tftPrint(F("[//] axel espinal"));
  tftSetCursor(8, 118);
  tftPrint(F("axelespinal.com/projects"));

  while (true) {
    if (checkGlobalMenu())
      return;
    // Start on any key except Menu (Bit 4)
    if (((~PINC & 0x3F) & ~(1 << BTN_MENU_BIT)) != 0 || !digitalRead(2) ||
        !digitalRead(5)) {
      randomSeed(micros());
      break;
    }
    delay(10);
  }

  tftFillScreen(C_BLACK);
  tftDrawRect(5 - 2, 4 - 2, 60 + 4, 120 + 4, C_WHITE);
  tftSetTextColor(C_WHITE);
  tftSetCursor(75, 5);
  tftPrint(F("NEXT"));
  tftDrawRect(75, 14, 30, 30, C_GREY);
  tftSetCursor(115, 5);
  tftPrint(F("HOLD"));
  tftDrawRect(115, 14, 30, 30, C_GREY);
  tftSetCursor(75, 47);
  tftPrint(F("SCORE"));

  memset(game.tetris.board, 0, sizeof(game.tetris.board));
  game.tetris.score = 0;
  game.tetris.bagIndex = 7;
  game.tetris.holdType = -1;
  game.tetris.canHold = true;
  game.tetris.dropInterval = 800;
  game.tetris.gameOver = false;
  game.tetris.paused = false;

  game.tetris.lastBtnA = IS_PRESSED_D(BTN_A_BIT);
  game.tetris.lastBtnB = IS_PRESSED_D(BTN_B_BIT);
  game.tetris.lastBtnStart = IS_PRESSED_C(BTN_START_BIT);

  updateTetScore(0);
  spawnTetPiece();
}

void loopTetris() {
  if (checkGlobalMenu())
    return;

  if (game.tetris.gameOver) {
    if (!game.tetris.prevGameOver) {
      tftFillRect(75, 75, 80, 10, C_BLACK);
      tftSetCursor(75, 75);
      tftSetTextColor(C_RED);
      tftPrint(F("GAME OVER"));
      game.tetris.prevGameOver = true;
    }
    if (IS_PRESSED_C(BTN_START_BIT)) {
      if (!game.tetris.lastBtnStart)
        setupTetris();
    }
    game.tetris.lastBtnStart = IS_PRESSED_C(BTN_START_BIT);
    return;
  }

  bool currStart = IS_PRESSED_C(BTN_START_BIT);
  if (game.tetris.lastBtnStart == LOW && currStart == HIGH) {
    game.tetris.paused = !game.tetris.paused;
    if (game.tetris.paused) {
      tftFillRect(75, 75, 80, 10, C_BLACK);
      tftSetCursor(75, 75);
      tftSetTextColor(C_WHITE);
      tftPrint(F("PAUSED"));
    } else {
      tftFillRect(75, 75, 80, 10, C_BLACK);
      eraseTetGhost(game.tetris.px, game.tetris.lastGhostY, game.tetris.pRot);
      tftFillRect(5, 4, 60, 120, C_BLACK);
      for (int x = 0; x < 10; x++)
        for (int y = 0; y < 20; y++)
          if (game.tetris.board[x][y])
            drawTetBlock(x, y,
                         pgm_read_word(&tetrisColors[game.tetris.board[x][y]]));
      drawTetGhost(game.tetris.px, game.tetris.lastGhostY, game.tetris.pRot);
      drawTetPiece(false, game.tetris.px, game.tetris.py, game.tetris.pRot);
    }
  }
  game.tetris.lastBtnStart = currStart;

  if (game.tetris.paused)
    return;

  static unsigned long lastIn = 0;
  if (millis() - lastIn > 100) {
    if (IS_PRESSED_C(BTN_LEFT_BIT) &&
        isValidTetMove(game.tetris.px - 1, game.tetris.py, game.tetris.pRot)) {
      updateTetPiecePos(game.tetris.px - 1, game.tetris.py, game.tetris.pRot);
      lastIn = millis();
    }
    if (IS_PRESSED_C(BTN_RIGHT_BIT) &&
        isValidTetMove(game.tetris.px + 1, game.tetris.py, game.tetris.pRot)) {
      updateTetPiecePos(game.tetris.px + 1, game.tetris.py, game.tetris.pRot);
      lastIn = millis();
    }
    if (IS_PRESSED_C(BTN_DOWN_BIT) &&
        isValidTetMove(game.tetris.px, game.tetris.py + 1, game.tetris.pRot)) {
      updateTetPiecePos(game.tetris.px, game.tetris.py + 1, game.tetris.pRot);
      lastIn = millis() - 50;
      updateTetScore(1);
    }
  }

  static bool lu = HIGH;
  bool cu = IS_PRESSED_C(BTN_UP_BIT);
  if (lu == LOW && cu == HIGH && game.tetris.pType != 1) {
    int nr = (game.tetris.pRot + 1) % 4;
    if (isValidTetMove(game.tetris.px, game.tetris.py, nr))
      updateTetPiecePos(game.tetris.px, game.tetris.py, nr);
  }
  lu = cu;

  bool cb = IS_PRESSED_D(BTN_B_BIT);

  if (game.tetris.lastBtnB == LOW && cb == HIGH && game.tetris.canHold) {
    eraseTetGhost(game.tetris.px, game.tetris.lastGhostY, game.tetris.pRot);
    for (int i = 0; i < 4; i++)
      for (int j = 0; j < 4; j++)
        if (getTetShape(game.tetris.pType, i, j, game.tetris.pRot))
          tftFillRect(5 + (game.tetris.px + i) * 6,
                      4 + (game.tetris.py + j) * 6, 5, 5, C_BLACK);

    if (game.tetris.holdType == -1) {
      game.tetris.holdType = game.tetris.pType;
      spawnTetPiece();
    } else {
      int temp = game.tetris.holdType;
      game.tetris.holdType = game.tetris.pType;
      game.tetris.pType = temp;
      game.tetris.px = 3;
      game.tetris.py = 0;
      game.tetris.pRot = 0;
      game.tetris.lastGhostY = getTetGhostY(3, 0, 0);
      drawTetGhost(3, game.tetris.lastGhostY, 0);
      drawTetPiece(false, 3, 0, 0);
    }
    game.tetris.canHold = false;

    tftFillRect(116, 15, 28, 28, C_BLACK);
    // Draw Hold Piece in GREY to indicate it's locked for this turn
    uint16_t c = C_GREY;
    for (int i = 0; i < 4; i++)
      for (int j = 0; j < 4; j++)
        if (getTetShape(game.tetris.holdType, i, j, 0))
          tftFillRect(119 + i * 5, 19 + j * 5, 4, 4, c);
  }
  game.tetris.lastBtnB = cb;

  bool ca = IS_PRESSED_D(BTN_A_BIT);
  if (game.tetris.lastBtnA == LOW && ca == HIGH) {
    int dy = getTetGhostY(game.tetris.px, game.tetris.py, game.tetris.pRot);
    updateTetScore((dy - game.tetris.py) * 2);
    updateTetPiecePos(game.tetris.px, dy, game.tetris.pRot);
    lockTetPiece();
  }
  game.tetris.lastBtnA = ca;

  if (millis() - game.tetris.lastDropTime > game.tetris.dropInterval) {
    if (isValidTetMove(game.tetris.px, game.tetris.py + 1, game.tetris.pRot)) {
      updateTetPiecePos(game.tetris.px, game.tetris.py + 1, game.tetris.pRot);
    } else {
      lockTetPiece();
    }
    game.tetris.lastDropTime = millis();
  }
}

// ======================================================================================
// === ABOUT / CUBE IMPLEMENTATION ===
// ======================================================================================

const float cube_vertex[8][3] PROGMEM = {
    {-20, -20, 20},  {20, -20, 20},  {20, 20, 20},  {-20, 20, 20},
    {-20, -20, -20}, {20, -20, -20}, {20, 20, -20}, {-20, 20, -20}};

const uint8_t cube_face[12][2] PROGMEM = {{0, 1}, {1, 2}, {2, 3}, {3, 0},
                                          {4, 5}, {5, 6}, {6, 7}, {7, 4},
                                          {0, 4}, {1, 5}, {2, 6}, {3, 7}};

#define REAL_CYAN C_CYAN

void tftDrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                 uint16_t color) {
  int16_t steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    int16_t temp;
    temp = x0;
    x0 = y0;
    y0 = temp;
    temp = x1;
    x1 = y1;
    y1 = temp;
  }
  if (x0 > x1) {
    int16_t temp;
    temp = x0;
    x0 = x1;
    x1 = temp;
    temp = y0;
    y0 = y1;
    y1 = temp;
  }

  int16_t dx, dy;
  dx = x1 - x0;
  dy = abs(y1 - y0);

  int16_t err = dx / 2;
  int16_t ystep;

  if (y0 < y1) {
    ystep = 1;
  } else {
    ystep = -1;
  }

  for (; x0 <= x1; x0++) {
    if (steep) {
      tftDrawPixel(y0, x0, color);
    } else {
      tftDrawPixel(x0, y0, color);
    }
    err -= dy;
    if (err < 0) {
      y0 += ystep;
      err += dx;
    }
  }
}

void drawFooterAbout() {
  tftSetTextSize(1);
  tftSetTextColor(C_WHITE);

  int x = 65;
  int y = 35;
  int spacing = 10;

  tftSetCursor(x, y);
  tftPrint(F("[//] meridian"));

  tftSetCursor(x, y + spacing);
  tftPrint(F("axel was here"));

  tftSetCursor(x, y + spacing * 2);
  tftPrint(F("2025-12-27"));

  // Footer URL
  tftSetCursor(5, 100);
  tftPrint(F("learn more:"));
  tftSetCursor(5, 110);
  tftPrint(F("https://axelespinal.com/"));
  tftSetCursor(5, 120);
  tftPrint(F("projects/meridian"));
}

void setupAbout() {
  tftFillScreen(C_BLACK);

  game.about.angleX = 0;
  game.about.angleY = 0;
  game.about.angleZ = 0;
  game.about.firstFrame = true;

  drawFooterAbout();
}

void loopAbout() {
  if (checkGlobalMenu())
    return;

  float sx = sin(game.about.angleX);
  float cx = cos(game.about.angleX);
  float sy = sin(game.about.angleY);
  float cy = cos(game.about.angleY);
  float sz = sin(game.about.angleZ);
  float cz = cos(game.about.angleZ);

  int xOffset = 33;
  int yOffset = 50;

  int newProjected2D[8][2];

  for (int i = 0; i < 8; i++) {
    float x = pgm_read_float(&cube_vertex[i][0]);
    float y = pgm_read_float(&cube_vertex[i][1]);
    float z = pgm_read_float(&cube_vertex[i][2]);

    float xy = cx * y - sx * z;
    float xz = sx * y + cx * z;
    float yz = cy * xz - sy * x;
    float yx = sy * xz + cy * x;
    float zx = cz * yx - sz * xy;
    float zy = sz * yx + cz * xy;

    float scale = 64 / (100 - yz);
    newProjected2D[i][0] = xOffset + (zx * scale);
    newProjected2D[i][1] = yOffset + (zy * scale);
  }

  // Optimize: Interleave Erase and Draw to reduce flicker
  for (int i = 0; i < 12; i++) {
    uint8_t p1 = pgm_read_byte(&cube_face[i][0]);
    uint8_t p2 = pgm_read_byte(&cube_face[i][1]);

    // Erase Old Line
    if (!game.about.firstFrame) {
      tftDrawLine(game.about.oldProjected2D[p1][0],
                  game.about.oldProjected2D[p1][1],
                  game.about.oldProjected2D[p2][0],
                  game.about.oldProjected2D[p2][1], C_BLACK);
    }

    // Draw New Line
    tftDrawLine(newProjected2D[p1][0], newProjected2D[p1][1],
                newProjected2D[p2][0], newProjected2D[p2][1], C_CYAN);
  }

  // Save coords
  for (int i = 0; i < 8; i++) {
    game.about.oldProjected2D[i][0] = newProjected2D[i][0];
    game.about.oldProjected2D[i][1] = newProjected2D[i][1];
  }
  game.about.firstFrame = false;

  game.about.angleX += 0.04;
  game.about.angleY += 0.03;
  game.about.angleZ += 0.02;

  // No delay for maximum speed
}
