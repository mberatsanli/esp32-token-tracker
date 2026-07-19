#include "provider_client.h"

// Default mode/widget implementations, derived from generic Provider state.
// Providers override these to declare their own content; the core only ever
// dispatches mode() -> connect()/usages()/failure() and draws the result.

ProviderMode ProviderClient::mode(const Provider &p) const {
  if (p.fetchOk) return ProviderMode::Connected;
  if (p.error.length()) return ProviderMode::Failed;
  return ProviderMode::Waiting;
}

void ProviderClient::connect(Screen &s, const Provider &p) const {
  // knows nothing about HOW a provider onboards — providers with a real
  // flow (OAuth pairing, token setup) override this and draw their own
  String hint = (p.apiKey.length() || p.type == "push") ? "waiting for data"
                                                        : "no key configured";
  s.centerText(hint, s.height() / 2 - 4, 4, s.muted());
}

std::vector<Widget> ProviderClient::usages(const Provider &p) const {
  Widget w;
  if (p.limit > 0 && p.used >= 0) {
    w.kind = Widget::ProgressBar;
    w.title = "Usage";
    w.pct = min(100.0f, p.used / p.limit * 100.0f);
  } else if (p.label.length()) {
    w.text = p.label;
  } else {
    w.text = "waiting for data";
    w.muted = true;
  }
  return {w};
}

std::vector<Widget> ProviderClient::failure(const Provider &p) const {
  Widget w;
  w.text = p.error.length() ? p.error : "fetch failed";
  w.isError = true;
  return {w};
}
