#pragma once
#include "../provider_client.h"
#include "logo.h"

// Claude subscription usage.
//
// The dedicated /api/oauth/usage endpoint rate-limits at any polling interval
// (anthropics/claude-code#31637), so instead we send a 1-token Messages probe
// and read the usage percentages from the anthropic-ratelimit-unified-*
// response headers (the claude-usage-stick approach).
// Requires an OAuth token: `claude setup-token` -> sk-ant-oat01-...
class ClaudeCodeClient : public ProviderClient {
 public:
  const char *type() const override { return "claude-code"; }
  const char *displayName() const override { return "Claude Code"; }
  bool fetch(Provider &p) override;

  // own onboarding: how to obtain and enter the OAuth token
  void connect(Screen &s, const Provider &p) const override;

  // two rate-limit windows: rolling 5h + weekly
  std::vector<Widget> usages(const Provider &p) const override {
    if (p.pct5 < 0) return ProviderClient::usages(p);
    Widget cur, wk;
    cur.kind = wk.kind = Widget::ProgressBar;
    cur.title = "Current"; cur.pct = p.pct5; cur.resetAt = p.reset5;
    wk.title = "Weekly";   wk.pct = p.used;  wk.resetAt = p.reset7;
    return {cur, wk};
  }

  ProviderLogo logo() const override { return {LOGO_CLAUDE, 28, 28}; }
  uint16_t brandColor() const override { return rgb565(0xD9, 0x77, 0x57); }  // Anthropic clay

  // each poll is a real (1-token) API request; keep a sane floor and never
  // let a config-save burst bypass it
  uint32_t minIntervalMs() const override { return 60000; }
  bool allowsForcedRefresh() const override { return false; }
};
