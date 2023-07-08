/*
  Conway's Game of Life for M5Stack AtomS3

  Copyright (c) 2023 Takashi Satou

  Released under the MIT License
*/

#include <M5AtomS3.h>
#include <stdlib.h>

const int SZ_LIST[] = {4, 2, 1}; // pixel size
const int W = 128; // max width
const int H = 128; // max height
unsigned char buf[2][W * H]; // buffer for two states
unsigned char history[2][W * H]; // history to detect stable states

int cur_sz = 1; // index of current pixel size
int sz, w, h;   // current pixel size, width and height
int cur;        // current state
int generation; // generation counter
int heatmap[256]; // heatmap for display

/* calc heatmap
   0        black
   1..84    cyan - green
   85..169  green - yellow
   170..254 yellow - red
   255      red
*/
void init_heatmap() {
  static int C[4][3] = {
    {0, 255, 255}, // cyan
    {0, 255, 0}, // green
    {255, 255, 0}, // yellow
    {255, 0, 0}, // red
  };
  const int D = 255 / 3;
  for (int i = 0; i < 256; i++) {
    int j = i / D;
    int m = i - j * D;
    int n = D - m;
    int r = (n * C[j][0] + m * C[j+1][0]) / 255;
    int g = (n * C[j][1] + m * C[j+1][1]) / 255;
    int b = (n * C[j][2] + m * C[j+1][2]) / 255;
    heatmap[i] = M5.Lcd.color565(r, g, b);
  }
  heatmap[0] = 0;
  heatmap[255] = heatmap[254];
}

/* initialize current buffer randomly */
void init_life() {
  sz = SZ_LIST[cur_sz];
  w = W / sz;
  h = H / sz;

  int i = 0;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      buf[0][i] = (rand() % 100 < 10) ? 1 : 0;
      buf[1][i] = 0;
      i++;
    }
  }
  cur = 0;
  generation = 1;
  M5.Lcd.fillScreen(heatmap[0]);
}

/* convert coordinate (x mod w, y mod h) to the index of buffer */
inline int ind(int x, int y) {
  return (x + w) % w + (y + h) % h * w;
}

/* update generation */
void do_life() {
  unsigned char* cbuf = buf[cur];
  unsigned char* dbuf = buf[1 - cur];
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int ct = 0;
      if (cbuf[ind(x - 1, y - 1)]) ct++;
      if (cbuf[ind(x, y - 1)]) ct++;
      if (cbuf[ind(x + 1, y - 1)]) ct++;
      if (cbuf[ind(x - 1, y)]) ct++;
      if (cbuf[ind(x + 1, y)]) ct++;
      if (cbuf[ind(x - 1, y + 1)]) ct++;
      if (cbuf[ind(x, y + 1)]) ct++;
      if (cbuf[ind(x + 1, y + 1)]) ct++;
      int cv = cbuf[ind(x, y)];
      if (cv) { // live
        dbuf[ind(x, y)] = (ct == 2 || ct == 3)
          ? cv < 255 ? (cv + 1) : 255 // live
          : 0; // die
      } else { // die
        dbuf[ind(x, y)] = (ct == 3) ? 1 : 0; // live or die
      }
    }
  }
  generation++;
  cur = 1 - cur;
}

bool detect_stable() {
  if (memcmp(history[cur], buf[cur], W * H) == 0) return true;
  memcpy(history[cur], buf[cur], W * H);
  return false;
}

void draw_life() {
  M5.Lcd.startWrite(); // speed up!

  unsigned char* cbuf = buf[cur];
  unsigned char* dbuf = buf[1 - cur];
  if (sz == 1) {
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++, cbuf++, dbuf++) {
        if (*cbuf != *dbuf) {
          M5.Lcd.drawPixel(x, y, heatmap[*cbuf]);
        }
      }
    }
  } else {
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++, cbuf++, dbuf++) {
        if (*cbuf != *dbuf) {
          M5.Lcd.fillRect(x * sz, y * sz, sz, sz, heatmap[*cbuf]);
        }
      }
    }
  }
#if 0
  // draw heatmap for debug
  for (i = 0; i < 128; i++) {
    M5.Lcd.fillRect(i, 0, 1, 20, heatmap[i]);
  }
  for (i = 128; i < 256; i++) {
    M5.Lcd.fillRect(i - 128, 20, 1, 20, heatmap[i]);
  }
#endif
  M5.Lcd.endWrite();
}

enum { IDLE, RUNNING } mode;
time_t start_time = 0;

void setup() {
  M5.begin(true, true, true, false);
  M5.Lcd.begin();
  M5.Lcd.setRotation(2);
  M5.Lcd.fillScreen(M5.Lcd.color565(0, 0, 32));
  M5.IMU.begin();
  init_heatmap();

  // draw initial screen
  mode = IDLE;
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.drawString("Conway's", 10, 20, 2);
  M5.Lcd.drawString("Game of Life", 30, 40, 2);
  M5.Lcd.drawString("Game of Life", 31, 40, 2);
  M5.Lcd.drawString("Click to Start", 25, 100, 2);
  M5.Lcd.setTextColor(YELLOW);
  M5.Lcd.drawString("Tilt: Reset", 15, 70, 1);
  M5.Lcd.drawString("Click: Change Size", 15, 80, 1);
}

void loop() {
  M5.update();

  if (mode != RUNNING) {
    generation++;
    // check click to start
    if (M5.Btn.wasPressed() ||
        (start_time != 0 && time(NULL) >= start_time))  {
      srand(generation);
      init_life();
      mode = RUNNING;
    }
    return;
  }

  // check click to change the pixel size
  if (M5.Btn.wasPressed()) {
    cur_sz = (cur_sz + 1) % 3;
    init_life();
  }

  // check tilt to reset
  float ax, ay, az;
  M5.IMU.getAccel(&ax, &ay, &az);
  if (ax > 0.5) {
    init_life();
  }

  do_life();
  draw_life();

  if (detect_stable()) {
    char b[25];
    sprintf(b, "%d", generation);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.drawString(b, 8, 8, 2);
    M5.Lcd.drawString("generations", 8, 24, 2);
    mode = IDLE;
    start_time = time(NULL) + 5;
  }
}
