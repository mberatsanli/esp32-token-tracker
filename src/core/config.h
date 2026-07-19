#pragma once
#include <Arduino.h>
#include <vector>

struct Provider {
  String id;            // short slug, e.g. "claude"
  String name;          // display name
  String type = "push"; // matches a registered ProviderClient::type();
                        // "push" = no client, values arrive via POST /api/push
  String apiKey;
  bool enabled = true;

  // runtime state (not persisted except via push)
  float used = -1;    // consumed amount (tokens/requests/$)
  float limit = -1;   // budget/limit, -1 = unknown
  String label;       // rendered value text, e.g. "$12.40" or "35k/500k"
  String error;       // last fetch error, shown on screen when fetch fails
  float pct5 = -1;    // 5h window %, enables the two-bar page layout
  uint32_t reset5 = 0, reset7 = 0;  // window reset times (epoch)

  // OAuth device-flow runtime state (apiKey holds the persisted refresh token)
  String accessToken;
  uint32_t accessExpMs = 0;
  String deviceCode, userCode;  // pairing in progress when set
  String verifyUrl;             // verification_uri_complete (code prefilled)
  uint32_t nextPollMs = 0;
  bool fetchOk = false;
  uint32_t lastFetch = 0;
};

struct Config {
  String wifiSsid;
  String wifiPass;
  String apPass;   // device's own AP password (min 8 chars); random on first boot
  String webPass;  // web panel password (basic auth, user "admin"); same random
  uint8_t rotation = 0;      // 0/2 portrait, 1/3 landscape
  uint16_t refreshSec = 120; // provider poll interval
  std::vector<Provider> providers;
};

extern Config config;

void configLoad();
void configSave();
String configToJson(bool includeKeys);
bool configFromJson(const String &json);
Provider *findProvider(const String &id);
