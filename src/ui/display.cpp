#include "display.h"
#include "../core/config.h"
#include "../providers/registry.h"
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <time.h>
#include <vector>

static TFT_eSPI tft;
static TFT_eSprite spr(&tft);  // full-screen 8bpp sprite for slide animation
static bool sprOk = false;
static int curPage = 0;

static const uint16_t BG = TFT_BLACK;
static const uint16_t FG = TFT_WHITE;
static const uint16_t MUTED = 0x7BEF;  // mid gray
static const uint16_t ERRCOL = 0xE28A; // soft red

// /usage-style card palette
static uint16_t CARD, PILL, TRACK, GRAYTX;
static void initPalette(TFT_eSPI &t) {
  CARD = t.color565(0x17, 0x17, 0x17);
  PILL = t.color565(0x2E, 0x2E, 0x2E);
  TRACK = t.color565(0x48, 0x48, 0x48);
  GRAYTX = t.color565(0x9E, 0x9E, 0x9E);
}



static std::vector<const Provider *> shownList() {
  std::vector<const Provider *> v;
  for (auto &p : config.providers)
    if (p.enabled) v.push_back(&p);
  return v;
}

int displayPageCount() { return (int)shownList().size(); }

void displayInit() {
  tft.init();
  initPalette(tft);
  displayApplyRotation();
  tft.fillScreen(BG);
}

void displayApplyRotation() {
  tft.setRotation(config.rotation);
  if (sprOk) spr.deleteSprite();
  spr.setColorDepth(8);
  sprOk = spr.createSprite(tft.width(), tft.height()) != nullptr;
}

void displayBootMessage(const String &line1, const String &line2) {
  tft.fillScreen(BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FG, BG);
  tft.drawString(line1, tft.width() / 2, tft.height() / 2 - 12, 4);
  if (line2.length()) {
    tft.setTextColor(MUTED, BG);
    tft.drawString(line2, tft.width() / 2, tft.height() / 2 + 16, 2);
  }
}

void displaySetupScreen(bool wifiFailed) {
  tft.fillScreen(BG);
  int W = tft.width();
  uint16_t accent = tft.color565(0xD9, 0x77, 0x57);

  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(accent, BG);
  tft.drawString(wifiFailed ? "WiFi FAILED" : "SETUP", W / 2, 14, 4);

  struct { const char *head; String body; } steps[] = {
      {"1. Join network:", "TokenTracker"},
      {"2. Password:", config.apPass},
      {"3. Open in browser:", "http://192.168.4.1"},
      {"4. Login:", "admin / " + config.webPass},
  };
  int y = 46;
  for (auto &s : steps) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(MUTED, BG);
    tft.drawString(s.head, 12, y, 2);
    y += 17;
    int font = tft.textWidth(s.body, 4) <= W - 34 ? 4 : 2;
    tft.setTextColor(FG, BG);
    tft.drawString(s.body, 22, y, font);
    y += (font == 4 ? 27 : 17) + 3;
  }
}

// "2h14m" / "3d4h" until an epoch, empty when unknown or time not synced
static String fmtEta(uint32_t epoch) {
  time_t now = time(nullptr);
  if (!epoch || now < 1600000000 || (time_t)epoch <= now) return "";
  uint32_t d = epoch - now;
  char buf[20];
  if (d >= 86400) snprintf(buf, sizeof(buf), "%ud %uh", d / 86400, (d % 86400) / 3600);
  else if (d >= 3600) snprintf(buf, sizeof(buf), "%uh %um", d / 3600, (d % 3600) / 60);
  else snprintf(buf, sizeof(buf), "%um", d / 60);
  return String(buf);
}

// TFT_eSprite has no fillScreen override — the base method blasts the panel
// directly and leaves the sprite dirty, so clearing needs an overload pair.
static void clearAll(TFT_eSPI &g) { g.fillScreen(BG); }
static void clearAll(TFT_eSprite &g) { g.fillSprite(BG); }

// Screen implementation handed to ProviderClient::connect(). Binds the
// template target statically so provider draws land on sprite or panel
// correctly, while providers only see the abstract Screen.
template <typename GFX>
class ScreenImpl : public Screen {
 public:
  explicit ScreenImpl(GFX &g) : g_(g) {}
  int width() const override { return g_.width(); }
  int height() const override { return g_.height(); }
  uint16_t fg() const override { return FG; }
  uint16_t muted() const override { return MUTED; }
  void centerText(const String &s, int y, int font, uint16_t color,
                  uint8_t size) override {
    g_.setTextDatum(MC_DATUM);
    g_.setTextColor(color, BG);
    g_.setTextSize(size);
    g_.drawString(s, g_.width() / 2, y, font);
    g_.setTextSize(1);
  }

 private:
  GFX &g_;
};

// Templated on the target so sprite draws bind statically — passing a sprite
// as TFT_eSPI& routes non-virtual methods to the panel and ghosts the screen.
// /usage-style card: big percent, pill tag, bar, "Resets in ..."
template <typename GFX>
static void pageUsageCard(GFX &g, int y, int h, float pct, const char *pill,
                          const String &eta, uint16_t brand) {
  int W = g.width();
  int x = 8, w = W - 16;
  g.fillRoundRect(x, y, w, h, 12, CARD);

  // big percent, scaled font 4
  g.setTextDatum(TL_DATUM);
  g.setTextColor(FG, CARD);
  g.setTextSize(2);
  g.drawString(String((int)pct) + "%", x + 12, y + 6, 4);
  g.setTextSize(1);

  // pill tag, right-aligned
  int pw = g.textWidth(pill, 2) + 20, ph = 24;
  int px = x + w - pw - 10, py = y + 10;
  g.fillRoundRect(px, py, pw, ph, 12, PILL);
  g.setTextDatum(MC_DATUM);
  g.setTextColor(FG, PILL);
  g.drawString(pill, px + pw / 2, py + ph / 2 + 1, 2);

  // progress bar: brand color fill for contrast against the dark track
  int bx = x + 12, bw = w - 24, by = y + h - 34, bh = 12;
  g.fillRoundRect(bx, by, bw, bh, 6, TRACK);
  uint16_t fill = pct > 85 ? TFT_RED : pct > 60 ? TFT_ORANGE : brand;
  int fw = (int)(bw * min(1.0f, pct / 100.0f));
  if (fw > 5) g.fillRoundRect(bx, by, fw, bh, 6, fill);
  else if (pct > 0) g.fillCircle(bx + 6, by + bh / 2, bh / 2, fill);

  // reset countdown
  if (eta.length()) {
    g.setTextDatum(TL_DATUM);
    g.setTextColor(GRAYTX, CARD);
    g.drawString("Resets in " + eta, x + 12, y + h - 20, 2);
  }
}

// one provider, full screen
template <typename GFX>
static void drawPage(GFX &g, const Provider &p, int idx, int total) {
  clearAll(g);
  int W = g.width(), H = g.height();

  // visual identity and behavior both come from the provider's client
  ProviderClient &pc = providerClientOrGeneric(p);
  uint16_t col = pc.brandColor();
  ProviderLogo lg = pc.logo();
  int lw = lg.bits ? lg.w : 28, lh = lg.bits ? lg.h : 28;

  // header: logo (or letter badge fallback) + name, centered as one group
  int nameW = g.textWidth(p.name, 4);
  int gap = 10;
  int startX = (W - (lw + gap + nameW)) / 2;
  if (startX < 8) startX = 8;
  int cy = 8 + lh / 2;  // shared vertical center for logo and text
  if (lg.bits) {
    g.drawXBitmap(startX, 8, lg.bits, lw, lh, col);
  } else {
    g.fillRoundRect(startX, 8, lw, lh, 8, col);
    String letter = p.name.substring(0, 1);
    letter.toUpperCase();
    g.setTextDatum(MC_DATUM);
    g.setTextColor(TFT_WHITE, col);
    g.drawString(letter, startX + lw / 2, cy, 4);
  }
  // font 4 keeps descender space below its caps; +4 optically centers vs logo
  g.setTextDatum(ML_DATUM);
  g.setTextColor(FG, BG);
  g.drawString(p.name, startX + lw + gap, cy + 4, 4);

  // Body dispatch: Waiting -> provider draws its own onboarding via Screen;
  // Connected/Failed -> provider returns widget DTOs and this code draws.
  ProviderMode mode = pc.mode(p);
  std::vector<Widget> widgets;
  if (mode == ProviderMode::Connected) widgets = pc.usages(p);
  else if (mode == ProviderMode::Failed) widgets = pc.failure(p);

  std::vector<const Widget *> bars;
  const Widget *message = nullptr;
  for (const Widget &wg : widgets) {
    if (wg.kind == Widget::ProgressBar) bars.push_back(&wg);
    else if (!message) message = &wg;
  }

  if (mode == ProviderMode::Waiting) {
    ScreenImpl<GFX> screen(g);
    pc.connect(screen, p);
  } else if (!bars.empty()) {
    // stack /usage-style cards, vertically centered, height capped so
    // portrait doesn't stretch the gap between percent and bar
    int n = (int)bars.size();
    int top = 44, bottom = 22, gapY = 6;
    int avail = H - top - bottom;
    int ch = (avail - gapY * (n - 1)) / n;
    if (ch > 96) ch = 96;
    int y = top + (avail - (ch * n + gapY * (n - 1))) / 2;
    for (const Widget *b : bars) {
      pageUsageCard(g, y, ch, b->pct, b->title.c_str(), fmtEta(b->resetAt), col);
      y += ch + gapY;
    }
  } else if (message) {
    uint16_t vc = message->isError ? ERRCOL : message->muted ? MUTED : FG;
    g.setTextDatum(MC_DATUM);
    g.setTextColor(vc, BG);
    int font = g.textWidth(message->text, 4) <= W - 24 ? 4 : 2;
    g.drawString(message->text, W / 2, H / 2 - 4, font);
  }

  // footer: page dots + IP
  if (total > 1) {
    int dotY = H - 14;
    int startX = W / 2 - (total - 1) * 8;
    for (int i = 0; i < total; i++)
      g.fillCircle(startX + i * 16, dotY, i == idx ? 4 : 2, i == idx ? FG : MUTED);
  }
  g.setTextDatum(BL_DATUM);
  g.setTextColor(MUTED, BG);
  g.drawString(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "no wifi",
               8, H - 6, 1);
}

void displayRender() {
  auto shown = shownList();
  int W = tft.width(), H = tft.height();
  if (shown.empty()) {
    tft.fillScreen(BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(MUTED, BG);
    tft.drawString("No providers enabled", W / 2, H / 2, 2);
    tft.drawString("http://" + WiFi.localIP().toString(), W / 2, H / 2 + 20, 2);
    return;
  }
  if (curPage >= (int)shown.size()) curPage = 0;
  if (sprOk) {  // full-frame buffer: no flicker, no partial draws
    drawPage(spr, *shown[curPage], curPage, shown.size());
    spr.pushSprite(0, 0);
  } else {
    drawPage(tft, *shown[curPage], curPage, shown.size());
  }
}

// advance with a slide-in animation (falls back to instant redraw w/o sprite)
void displayNextPage(int dir) {
  auto shown = shownList();
  if (shown.empty()) { displayRender(); return; }
  curPage = ((curPage + dir) % (int)shown.size() + shown.size()) % shown.size();
  const Provider &p = *shown[curPage];
  if (!sprOk) {
    drawPage(tft, p, curPage, shown.size());
    return;
  }
  drawPage(spr, p, curPage, shown.size());
  int W = tft.width(), step = W / 8;
  if (dir >= 0)
    for (int o = W - step; o > 0; o -= step) spr.pushSprite(o, 0);
  else
    for (int o = step - W; o < 0; o += step) spr.pushSprite(o, 0);
  spr.pushSprite(0, 0);
}

// Read the panel back over SPI and stream as bottom-up 24-bit BMP.
void displayStreamBMP(Print &out) {
  int W = tft.width(), H = tft.height();
  uint32_t rowSize = W * 3;  // 240/320 px -> already 4-byte aligned
  uint32_t dataSize = rowSize * H;
  uint32_t fileSize = 54 + dataSize;
  uint8_t hdr[54] = {'B', 'M'};
  auto put32 = [&](int off, uint32_t v) {
    hdr[off] = v; hdr[off + 1] = v >> 8; hdr[off + 2] = v >> 16; hdr[off + 3] = v >> 24;
  };
  put32(2, fileSize);
  put32(10, 54);
  put32(14, 40);
  put32(18, W);
  put32(22, H);
  hdr[26] = 1;
  hdr[28] = 24;
  put32(34, dataSize);
  out.write(hdr, 54);

  static uint8_t row[320 * 3];
  for (int y = H - 1; y >= 0; y--) {
    for (int x = 0; x < W; x++) {
      uint16_t c = tft.readPixel(x, y);
      row[x * 3 + 0] = (c & 0x1F) << 3;
      row[x * 3 + 1] = ((c >> 5) & 0x3F) << 2;
      row[x * 3 + 2] = (c >> 11) << 3;
    }
    out.write(row, rowSize);
  }
}
