#include "registry.h"
#include "claude_code/claude_code_client.h"
#include "kimi_code/kimi_code_client.h"
#include <WiFi.h>

// Stateless client singletons — all per-provider state lives in config.
// Registering a new provider = add its client here; nothing else in the
// core needs to know it exists.
static ClaudeCodeClient claudeCodeClient;
static KimiCodeClient kimiCodeClient;
static std::vector<ProviderClient *> registry = {&claudeCodeClient, &kimiCodeClient};

const std::vector<ProviderClient *> &providerClients() { return registry; }

ProviderClient *providerClientFor(const String &type) {
  for (ProviderClient *c : registry)
    if (type == c->type()) return c;
  return nullptr;  // e.g. "push": data arrives via POST /api/push
}

// Base-behavior stand-in for client-less providers (push type): never
// fetches, but supplies the generic mode/widget defaults.
namespace {
class GenericClient : public ProviderClient {
 public:
  const char *type() const override { return "push"; }
  bool fetch(Provider &) override { return false; }
};
GenericClient genericClient;
}  // namespace

ProviderClient &providerClientOrGeneric(const Provider &p) {
  ProviderClient *c = providerClientFor(p.type);
  return c ? *c : genericClient;
}

// visible state that should trigger a redraw when it changes
static String renderSignature(const Provider &p) {
  return p.label + "|" + p.error + "|" + p.userCode + "|" + String(p.fetchOk);
}

bool providersRefresh(bool force) {
  if (WiFi.status() != WL_CONNECTED) return false;
  bool changed = false;
  uint32_t now = millis();

  for (auto &p : config.providers) {
    if (!p.enabled) continue;
    ProviderClient *client = providerClientFor(p.type);
    if (!client) continue;

    uint32_t interval = config.refreshSec * 1000UL;
    if (interval < client->minIntervalMs()) interval = client->minIntervalMs();
    bool keyless = p.apiKey.length() == 0;
    bool pairing = keyless && client->worksWithoutKey(p);  // paces itself
    bool mustWait = (!force || !client->allowsForcedRefresh()) && !pairing &&
                    p.lastFetch && now - p.lastFetch < interval;
    if (mustWait) continue;
    if (keyless && !pairing) continue;

    String before = renderSignature(p);
    bool ok = client->fetch(p);
    p.lastFetch = now;
    if (!ok) Serial.printf("[fetch] %s failed: %s\n", p.id.c_str(), client->error().c_str());
    // pairing-in-progress is not an error state
    p.error = ok || p.userCode.length()
                ? ""
                : (client->error().length() ? client->error() : "fetch failed");
    p.fetchOk = ok;
    if (before != renderSignature(p)) changed = true;
  }
  return changed;
}
