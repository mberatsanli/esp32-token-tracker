#include "touch.h"
#if FEATURE_TOUCH
#include "../core/config.h"
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

// CYD touch pins (separate SPI bus from the TFT, which runs on HSPI)
#define XPT_CLK 25
#define XPT_MISO 39
#define XPT_MOSI 32
#define XPT_CS 33
#define XPT_IRQ 36

static SPIClass tsSpi(VSPI);
static XPT2046_Touchscreen ts(XPT_CS, XPT_IRQ);

void touchInit() {
  tsSpi.begin(XPT_CLK, XPT_MISO, XPT_MOSI, XPT_CS);
  ts.begin(tsSpi);
  ts.setRotation(config.rotation);
}

void touchApplyRotation() { ts.setRotation(config.rotation); }

int touchTap() {
  static uint32_t lastTap = 0;
  if (!ts.touched()) return -1;
  if (millis() - lastTap < 400) return -1;  // debounce
  TS_Point p = ts.getPoint();
  lastTap = millis();
  // raw coords span ~200-3900; only left/right half matters
  return p.x > 2050 ? 1 : 0;
}
#endif
