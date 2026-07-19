#include "kimi_code_client.h"
#include "../../net/json_http.h"
#include <time.h>

static const char *CLIENT_ID = "17e5f671-d194-4dfb-9706-5516cb48c098";
static const char *TOKEN_URL = "https://auth.kimi.com/api/oauth/token";
static const char *DEVICE_AUTH_URL = "https://auth.kimi.com/api/oauth/device_authorization";
static const char *USAGES_URL = "https://api.kimi.com/coding/v1/usages";

// "2026-07-25T08:32:26.224982Z" -> epoch (device clock runs on UTC)
static uint32_t parseIso(const char *s) {
  if (!s) return 0;
  struct tm t = {};
  int y, mo, d, h, mi, se;
  if (sscanf(s, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &se) != 6) return 0;
  t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d;
  t.tm_hour = h; t.tm_min = mi; t.tm_sec = se;
  return (uint32_t)mktime(&t);
}

// Onboarding screen: the pair code from the device flow, big and centered,
// with the (shortened) approval URL above it.
void KimiCodeClient::connect(Screen &s, const Provider &p) const {
  if (!p.userCode.length()) {  // pairing not started yet
    ProviderClient::connect(s, p);
    return;
  }
  String shortUrl = p.verifyUrl;
  shortUrl.replace("https://", "");
  shortUrl.replace("http://", "");
  int q = shortUrl.indexOf('?');
  if (q > 0) shortUrl = shortUrl.substring(0, q);
  s.centerText("Pair this device:", 88, 2, s.muted());
  s.centerText(shortUrl, 108, shortUrl.length() > 40 ? 1 : 2, s.muted());
  // font 6 is digits-only; scaled font 4 handles letters
  s.centerText(p.userCode, 152, 4, s.fg(), 2);
}

// Device flow: obtain a pair code, then poll the token endpoint until the
// user approves it in a browser. Returns true once a refresh token is stored.
bool KimiCodeClient::runPairing(Provider &p) {
  JsonHttp http;
  JsonDocument doc;
  uint32_t now = millis();

  if (p.deviceCode.length() == 0) {
    if (!http.postForm(DEVICE_AUTH_URL, String("client_id=") + CLIENT_ID, doc)) {
      err_ = "pair start failed";
      return false;
    }
    p.deviceCode = doc["device_code"].as<String>();
    p.userCode = doc["user_code"].as<String>();
    p.verifyUrl = doc["verification_uri_complete"] | "";
    if (p.verifyUrl.length() == 0)
      p.verifyUrl = "https://www.kimi.com/code/authorize_device?user_code=" + p.userCode;
    p.nextPollMs = now + (doc["interval"] | 5) * 1000UL;
    Serial.printf("[kimi] pair code: %s\n", p.userCode.c_str());
    err_ = "";
    return false;  // no data yet; the page shows the pairing code
  }

  if ((int32_t)(now - p.nextPollMs) < 0) return false;
  p.nextPollMs = now + 5000;

  String body = String("grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Adevice_code"
                       "&device_code=") + p.deviceCode + "&client_id=" + CLIENT_ID;
  if (!http.postForm(TOKEN_URL, body, doc)) {
    String oauthErr = doc["error"] | "";
    if (oauthErr == "authorization_pending" || oauthErr == "slow_down") return false;
    p.deviceCode = ""; p.userCode = ""; p.verifyUrl = "";  // expired/denied -> restart
    err_ = oauthErr.length() ? oauthErr : "pair failed";
    return false;
  }

  p.accessToken = doc["access_token"].as<String>();
  p.accessExpMs = now + (uint32_t)((doc["expires_in"] | 900) - 60) * 1000UL;
  p.apiKey = doc["refresh_token"] | "";
  p.deviceCode = ""; p.userCode = ""; p.verifyUrl = "";
  configSave();  // persist refresh token
  Serial.println("[kimi] paired");
  return true;
}

bool KimiCodeClient::ensureAccessToken(Provider &p) {
  if (p.accessToken.length() && (int32_t)(millis() - p.accessExpMs) < 0) return true;

  JsonHttp http;
  JsonDocument doc;
  String body = String("grant_type=refresh_token&refresh_token=") + p.apiKey +
                "&client_id=" + CLIENT_ID;
  if (!http.postForm(TOKEN_URL, body, doc)) {
    if (String(doc["error"] | "").length()) {  // real oauth error, not transient
      p.apiKey = "";
      configSave();
      err_ = "re-pair needed";
    } else {
      err_ = "token refresh failed";
    }
    return false;
  }
  p.accessToken = doc["access_token"].as<String>();
  p.accessExpMs = millis() + (uint32_t)((doc["expires_in"] | 900) - 60) * 1000UL;
  if (doc["refresh_token"].is<const char *>()) {  // rotated -> persist new one
    String rotated = doc["refresh_token"].as<String>();
    if (rotated != p.apiKey) {
      p.apiKey = rotated;
      configSave();
    }
  }
  return true;
}

bool KimiCodeClient::fetchUsages(Provider &p) {
  JsonHttp http;
  JsonDocument doc;
  if (!http.get(USAGES_URL, {{"Authorization", "Bearer " + p.accessToken}}, doc)) {
    err_ = http.error();
    return false;
  }

  JsonObject weekly = doc["usage"];
  int wUsed = atoi(weekly["used"] | "0"), wLim = atoi(weekly["limit"] | "0");
  if (wLim <= 0) {
    err_ = "no usage data";
    return false;
  }

  int fUsed = -1, fLim = 0;
  const char *fReset = nullptr;
  for (JsonObject l : doc["limits"].as<JsonArray>()) {  // rolling 5h window
    JsonObject d = l["detail"];
    fUsed = atoi(d["used"] | "0");
    fLim = atoi(d["limit"] | "0");
    fReset = d["resetTime"] | (const char *)nullptr;
    break;
  }

  p.pct5 = fLim > 0 ? 100.0f * fUsed / fLim : -1;
  p.used = 100.0f * wUsed / wLim;
  p.limit = 100;
  p.reset5 = parseIso(fReset);
  p.reset7 = parseIso(weekly["resetTime"] | (const char *)nullptr);
  p.label = "5h " + String((int)p.pct5) + "% | wk " + String((int)p.used) + "%";
  return true;
}

bool KimiCodeClient::fetch(Provider &p) {
  err_ = "";
  if (p.apiKey.length() == 0 && !runPairing(p)) return false;
  if (!ensureAccessToken(p)) return false;
  return fetchUsages(p);
}
