#include <Arduino.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <time.h>
#include "core/config.h"
#include "ui/display.h"
#include "providers/registry.h"
#include "ui/touch.h"
#include "web/webui.h"

static DNSServer dns;
static bool staConnected = false;

// AP always on (password-protected), STA joins home network when configured.
static bool startWifi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("TokenTracker", config.apPass.c_str());
  dns.start(53, "*", WiFi.softAPIP());
  Serial.println("[wifi] AP up: http://192.168.4.1");

  if (config.wifiSsid.length() == 0) {
    displaySetupScreen(false);
    return false;
  }
  displayBootMessage("Connecting...", config.wifiSsid);
  WiFi.begin(config.wifiSsid.c_str(), config.wifiPass.c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000)
    delay(250);
  if (WiFi.status() != WL_CONNECTED) {
    displaySetupScreen(true);
    return false;
  }
  Serial.printf("[wifi] connected: %s\n", WiFi.localIP().toString().c_str());
  configTime(0, 0, "pool.ntp.org");  // UTC; needed for month-to-date queries
  return true;
}

void setup() {
  Serial.begin(115200);
  configLoad();
  displayInit();
  displayBootMessage("Token Tracker", "booting...");

  staConnected = startWifi();
  touchInit();
  webInit();
  if (staConnected) {
    providersRefresh(true);
    displayRender();
  }
}

void loop() {
  dns.processNextRequest();
  webLoop();

  // STA watchdog: keep retrying the home network, recover when it comes back
  if (config.wifiSsid.length()) {
    bool up = WiFi.status() == WL_CONNECTED;
    static uint32_t lastWifiTry = 0;
    if (!up && millis() - lastWifiTry > 15000) {
      lastWifiTry = millis();
      Serial.println("[wifi] retrying STA...");
      WiFi.begin(config.wifiSsid.c_str(), config.wifiPass.c_str());
    }
    if (up && !staConnected) {  // just (re)connected
      staConnected = true;
      Serial.printf("[wifi] STA up: %s\n", WiFi.localIP().toString().c_str());
      configTime(0, 0, "pool.ntp.org");
      displayRender();
    }
    if (!up && staConnected) staConnected = false;
  }

  if (webConfigChanged()) {
    displayApplyRotation();
    touchApplyRotation();
    if (!staConnected && config.wifiSsid.length()) {
      // credentials arrived via setup portal -> reboot to join home network
      displayBootMessage("Restarting...");
      delay(500);
      ESP.restart();
    }
    providersRefresh(true);
    displayRender();
  }

  static uint32_t lastPoll = 0;
  if (staConnected && millis() - lastPoll > 5000) {
    lastPoll = millis();
    if (providersRefresh()) displayRender();
  }

  // reset countdowns tick once a minute even without new data
  static uint32_t lastTick = 0;
  if (staConnected && millis() - lastTick > 60000) {
    lastTick = millis();
    displayRender();
  }

  // slider: auto-advance every 10s, tap left/right half to page manually
  static uint32_t lastSlide = 0;
  if (staConnected && displayPageCount() > 1 && millis() - lastSlide > 10000) {
    lastSlide = millis();
    displayNextPage(1);
  }
  int tap = touchTap();
  if (tap >= 0 && staConnected && displayPageCount() > 1) {
    lastSlide = millis();
    displayNextPage(tap == 1 ? 1 : -1);
  }
}
