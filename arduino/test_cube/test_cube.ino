#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// PINS
#define TFT_CS     6
#define TFT_RST    9
#define TFT_DC     8

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// 3D Cube Constants
float cube_vertex[8][3] = {
  { -20, -20,  20 }, {  20, -20,  20 }, {  20,  20,  20 }, { -20,  20,  20 },
  { -20, -20, -20 }, {  20, -20, -20 }, {  20,  20, -20 }, { -20,  20, -20 }
};
int cube_face[12][2] = {
  {0,1}, {1,2}, {2,3}, {3,0},
  {4,5}, {5,6}, {6,7}, {7,4},
  {0,4}, {1,5}, {2,6}, {3,7}
};

float angleX = 0, angleY = 0, angleZ = 0;

// Memory for anti-flicker
int oldProjected2D[8][2]; 
bool firstFrame = true;

// --- COLOR CORRECTION ---
// The screen inverts colors for some reason.
#define REAL_CYAN  ST7735_YELLOW 

void setup() {
  tft.initR(INITR_GREENTAB); 

  tft.fillScreen(ST7735_BLACK);
  tft.setRotation(0); 
  
  drawFooter();
}

void loop() {
  float sx = sin(angleX); float cx = cos(angleX);
  float sy = sin(angleY); float cy = cos(angleY);
  float sz = sin(angleZ); float cz = cos(angleZ);

  int xOffset = tft.width() / 2;
  int yOffset = (tft.height() / 2) - 20;

  int newProjected2D[8][2];

  for(int i=0; i<8; i++) {
    float x = cube_vertex[i][0];
    float y = cube_vertex[i][1];
    float z = cube_vertex[i][2];

    float xy = cx*y - sx*z;
    float xz = sx*y + cx*z;
    float yz = cy*xz - sy*x;
    float yx = sy*xz + cy*x;
    float zx = cz*yx - sz*xy;
    float zy = sz*yx + cz*xy;

    float scale = 64 / (100 - yz);
    newProjected2D[i][0] = xOffset + (zx * scale);
    newProjected2D[i][1] = yOffset + (zy * scale);
  }

  // Erase OLD Cube
  if (!firstFrame) {
    for(int i=0; i<12; i++) {
      tft.drawLine(
        oldProjected2D[cube_face[i][0]][0], oldProjected2D[cube_face[i][0]][1],
        oldProjected2D[cube_face[i][1]][0], oldProjected2D[cube_face[i][1]][1],
        ST7735_BLACK
      );
    }
  }

  // Draw NEW Cube
  for(int i=0; i<12; i++) {
    tft.drawLine(
      newProjected2D[cube_face[i][0]][0], newProjected2D[cube_face[i][0]][1],
      newProjected2D[cube_face[i][1]][0], newProjected2D[cube_face[i][1]][1],
      REAL_CYAN 
    );
  }

  // Save coords
  for(int i=0; i<8; i++) {
    oldProjected2D[i][0] = newProjected2D[i][0];
    oldProjected2D[i][1] = newProjected2D[i][1];
  }
  firstFrame = false;

  angleX += 0.04;
  angleY += 0.03;
  angleZ += 0.02;
}

void drawFooter() {
  tft.setTextSize(1);
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(10, 130);
  tft.print("hello, world!");

  tft.setCursor(10, 140);
  tft.print("[//] axel was here");

  tft.setCursor(10, 150);
  tft.print("2025-11-28");
}