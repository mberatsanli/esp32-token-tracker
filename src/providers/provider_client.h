#pragma once
#include "../core/config.h"
#include <vector>

// Abstract usage-fetching strategy for one provider type.
//
// Clients are stateless singletons: all persistent and runtime state lives in
// the Provider struct (serialized by config.cpp), so one client instance can
// serve any number of configured providers of its type.
// Bitmap in XBM layout (lsb-first rows), drawable via drawXBitmap.
struct ProviderLogo {
  const uint8_t *bits;
  uint8_t w, h;
};

// Render DTO: a provider declares WHAT to show, the display decides HOW.
// Kinds map 1:1 to layouts this app can draw — nothing more generic.
struct Widget {
  enum Kind : uint8_t {
    ProgressBar,  // /usage-style card: pct + title pill + bar + reset eta
    Message,      // one centered line (status, error, waiting...)
  };
  Kind kind = Message;
  String title;        // ProgressBar: pill text ("Current", "Weekly")
  float pct = -1;      // ProgressBar: 0..100
  uint32_t resetAt = 0;// ProgressBar: epoch for "Resets in ..." (0 = hide)
  String text;         // Message: line
  bool isError = false;// Message: error styling
  bool muted = false;  // Message: dim styling ("waiting for data" etc.)
};

// Type-erased drawing surface handed to connect(). Hides the panel-vs-sprite
// template dispatch (TFT_eSprite shadows non-virtual TFT_eSPI methods) so
// providers can draw without knowing the target — or ghosting the screen.
class Screen {
 public:
  virtual ~Screen() = default;
  virtual int width() const = 0;
  virtual int height() const = 0;
  virtual uint16_t fg() const = 0;     // theme foreground
  virtual uint16_t muted() const = 0;  // theme dim text
  // one horizontally centered line; size multiplies the base font
  virtual void centerText(const String &s, int y, int font, uint16_t color,
                          uint8_t size = 1) = 0;
};

// Provider-reported lifecycle state. The core dispatches on this — which
// widget method gets called — and never inspects provider internals.
enum class ProviderMode : uint8_t {
  Waiting,    // not usable yet: pairing flow, missing key, no data
  Connected,  // fetch succeeded, usage data available
  Failed,     // last fetch failed
};

class ProviderClient {
 public:
  virtual ~ProviderClient() = default;

  virtual const char *type() const = 0;
  virtual const char *displayName() const { return type(); }

  // Visual identity, consumed by the display layer.
  virtual ProviderLogo logo() const { return {nullptr, 0, 0}; }
  virtual uint16_t brandColor() const { return rgb565(0x60, 0x60, 0x60); }

  static constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }

  // Fill p's metrics (pct5/used/limit/resets/label) from the remote API.
  // On failure return false and leave the reason in error().
  virtual bool fetch(Provider &p) = 0;

  // --- render contract ---------------------------------------------------
  // The provider declares its mode; the core dispatches on it:
  //
  //   Waiting   -> connect(screen)  provider draws its own onboarding
  //                                 (pairing code, "no key" hint, ...)
  //   Connected -> usages()         widget DTO list, core draws it
  //   Failed    -> failure()        widget DTO list, core draws it
  //
  // Defaults derive everything from generic Provider state; override to
  // declare provider-specific content (e.g. two rate-limit bars).

  virtual ProviderMode mode(const Provider &p) const;
  virtual void connect(Screen &s, const Provider &p) const;
  virtual std::vector<Widget> usages(const Provider &p) const;
  virtual std::vector<Widget> failure(const Provider &p) const;

  // Floor for the polling interval, regardless of the configured refresh.
  virtual uint32_t minIntervalMs() const { return 0; }

  // Whether a forced refresh (config save) may bypass the interval.
  virtual bool allowsForcedRefresh() const { return true; }

  // Whether fetch() is meaningful without an API key/token (e.g. to run an
  // OAuth pairing flow that will produce one).
  virtual bool worksWithoutKey(const Provider &p) const { return false; }

  const String &error() const { return err_; }

 protected:
  String err_;
};
