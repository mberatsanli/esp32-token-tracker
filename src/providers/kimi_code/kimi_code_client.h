#pragma once
#include "../provider_client.h"
#include "logo.h"

// Kimi Code subscription quota, fully on-device:
//  1. no refresh token yet -> OAuth device flow (screen shows the pair code)
//  2. refresh the short-lived (15 min) access token as needed
//  3. GET /coding/v1/usages: weekly quota + rolling 5h window
//
// Provider state mapping: apiKey persists the refresh token; accessToken,
// deviceCode/userCode/verifyUrl and timers are runtime-only.
class KimiCodeClient : public ProviderClient {
 public:
  const char *type() const override { return "kimi-code"; }
  const char *displayName() const override { return "Kimi Code"; }
  bool fetch(Provider &p) override;

  // own onboarding: OAuth device-flow pairing screen
  void connect(Screen &s, const Provider &p) const override;

  // two quota windows: rolling 5h + weekly (pairing screen comes from the
  // base connect() while mode() is Waiting)
  std::vector<Widget> usages(const Provider &p) const override {
    if (p.pct5 < 0) return ProviderClient::usages(p);
    Widget cur, wk;
    cur.kind = wk.kind = Widget::ProgressBar;
    cur.title = "Current"; cur.pct = p.pct5; cur.resetAt = p.reset5;
    wk.title = "Weekly";   wk.pct = p.used;  wk.resetAt = p.reset7;
    return {cur, wk};
  }

  ProviderLogo logo() const override { return {LOGO_KIMI, 28, 28}; }
  uint16_t brandColor() const override { return rgb565(0x0A, 0x8F, 0xFF); }  // Moonshot blue

  // keyless fetch drives the pairing flow
  bool worksWithoutKey(const Provider &p) const override { return true; }

 private:
  bool runPairing(Provider &p);
  bool ensureAccessToken(Provider &p);
  bool fetchUsages(Provider &p);
};
