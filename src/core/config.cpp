#include "config.h"
#include "../providers/registry.h"
#include <ArduinoJson.h>
#include <Preferences.h>

Config config;
static Preferences prefs;

Provider *findProvider(const String &id) {
  for (auto &p : config.providers)
    if (p.id == id) return &p;
  return nullptr;
}

String configToJson(bool includeKeys) {
  JsonDocument doc;
  doc["wifiSsid"] = config.wifiSsid;
  if (includeKeys) {
    doc["wifiPass"] = config.wifiPass;
    doc["apPass"] = config.apPass;
    doc["webPass"] = config.webPass;
  }
  doc["rotation"] = config.rotation;
  doc["refreshSec"] = config.refreshSec;
  JsonArray arr = doc["providers"].to<JsonArray>();
  for (auto &p : config.providers) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = p.id;
    o["name"] = p.name;
    o["type"] = p.type;
    o["enabled"] = p.enabled;
    if (includeKeys) o["apiKey"] = p.apiKey;
    else o["hasKey"] = p.apiKey.length() > 0;
    o["label"] = p.label;
    o["error"] = p.error;
    o["userCode"] = p.userCode;
    o["verifyUrl"] = p.verifyUrl;
    o["used"] = p.used;
    o["limit"] = p.limit;
    o["fetchOk"] = p.fetchOk;
  }
  String out;
  serializeJson(doc, out);
  return out;
}

bool configFromJson(const String &json) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;

  if (doc["wifiSsid"].is<const char *>()) config.wifiSsid = doc["wifiSsid"].as<String>();
  if (doc["wifiPass"].is<const char *>()) config.wifiPass = doc["wifiPass"].as<String>();
  if (doc["apPass"].is<const char *>() && strlen(doc["apPass"]) >= 8)
    config.apPass = doc["apPass"].as<String>();
  if (doc["webPass"].is<const char *>() && strlen(doc["webPass"]) >= 4)
    config.webPass = doc["webPass"].as<String>();
  if (doc["rotation"].is<int>()) config.rotation = doc["rotation"].as<uint8_t>() & 3;
  if (doc["refreshSec"].is<int>()) config.refreshSec = max(30, doc["refreshSec"].as<int>());

  if (doc["providers"].is<JsonArray>()) {
    std::vector<Provider> next;
    for (JsonObject o : doc["providers"].as<JsonArray>()) {
      Provider p;
      p.id = o["id"].as<String>();
      if (p.id.length() == 0) continue;
      p.name = o["name"] | p.id.c_str();
      p.type = o["type"] | "push";
      p.enabled = o["enabled"] | true;
      // keep existing key unless a new one is sent
      Provider *old = findProvider(p.id);
      if (o["apiKey"].is<const char *>() && strlen(o["apiKey"]) > 0)
        p.apiKey = o["apiKey"].as<String>();
      else if (old)
        p.apiKey = old->apiKey;
      if (old) { // preserve runtime state
        p.used = old->used; p.limit = old->limit;
        p.label = old->label; p.fetchOk = old->fetchOk;
        p.error = old->error; p.pct5 = old->pct5;
        p.reset5 = old->reset5; p.reset7 = old->reset7;
        p.accessToken = old->accessToken; p.accessExpMs = old->accessExpMs;
        p.deviceCode = old->deviceCode; p.userCode = old->userCode;
        p.verifyUrl = old->verifyUrl; p.nextPollMs = old->nextPollMs;
      }
      if (o["clearKey"] | false) {  // forget token, restart pairing from scratch
        p.apiKey = ""; p.accessToken = "";
        p.deviceCode = ""; p.userCode = ""; p.verifyUrl = "";
      }
      next.push_back(p);
    }
    config.providers = next;
  }
  return true;
}

void configSave() {
  String json = configToJson(true);
  prefs.begin("tracker", false);
  // skip identical writes: token-refresh paths call this often and NVS flash
  // cycles are finite
  if (prefs.getString("cfg", "") != json) {
    size_t written = prefs.putString("cfg", json);
    if (written != json.length())
      Serial.printf("[config] NVS write FAILED (%u/%u bytes)\n",
                    (unsigned)written, (unsigned)json.length());
  }
  prefs.end();
}

// No secrets are compiled into the firmware: on first boot a random
// password is generated, stored in NVS and shown on the setup screen.
static String randomPassword(size_t len) {
  static const char alpha[] = "abcdefghjkmnpqrstuvwxyz23456789";  // no 0/O 1/l/i
  String out;
  for (size_t i = 0; i < len; i++) out += alpha[esp_random() % (sizeof(alpha) - 1)];
  return out;
}

void configLoad() {
  prefs.begin("tracker", true);
  String json = prefs.getString("cfg", "");
  prefs.end();
  if (json.length()) {
    configFromJson(json);
  } else {
    // defaults: seed one disabled provider per registered client
    for (ProviderClient *c : providerClients()) {
      Provider p;
      p.id = c->type();
      p.name = c->displayName();
      p.type = c->type();
      p.enabled = false;
      config.providers.push_back(p);
    }
  }
  if (config.apPass.length() < 8 || config.webPass.length() < 4) {
    String pass = randomPassword(8);  // one password for AP and panel,
    config.apPass = pass;             // both visible on the setup screen
    config.webPass = pass;
    configSave();
  }
}
