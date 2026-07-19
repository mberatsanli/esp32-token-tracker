#include "webui.h"
#include "../core/config.h"
#include "../ui/display.h"
#include <ArduinoJson.h>
#include <WebServer.h>

static WebServer server(80);
static bool changed = false;

// web/index.html embedded at link time via board_build.embed_txtfiles
// (null-terminated by the build system)
extern const char INDEX_HTML[] asm("_binary_web_index_html_start");

// Basic auth gate for every endpoint. Returns false and sends 401 if not authed.
static bool requireAuth() {
  if (server.authenticate("admin", config.webPass.c_str())) return true;
  server.requestAuthentication(BASIC_AUTH, "TokenTracker", "Login required");
  return false;
}


static void handleConfigGet() {
  if (!requireAuth()) return;
  server.send(200, "application/json", configToJson(false));
}

static void handleConfigPost() {
  if (!requireAuth()) return;
  if (!configFromJson(server.arg("plain"))) {
    server.send(400, "text/plain", "bad json");
    return;
  }
  configSave();
  changed = true;
  server.send(200, "application/json", "{\"ok\":true}");
}

// POST /api/push {"id":"claude","used":123,"limit":500,"label":"35k/500k"}
// Escape hatch for providers with no public usage API: push from a script.
static void handlePush() {
  if (!requireAuth()) return;
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "bad json");
    return;
  }
  Provider *p = findProvider(doc["id"].as<String>());
  if (!p) {
    server.send(404, "text/plain", "unknown provider");
    return;
  }
  if (doc["used"].is<float>()) p->used = doc["used"].as<float>();
  if (doc["limit"].is<float>()) p->limit = doc["limit"].as<float>();
  if (doc["label"].is<const char *>()) p->label = doc["label"].as<String>();
  p->fetchOk = true;
  changed = true;
  server.send(200, "application/json", "{\"ok\":true}");
}

void webInit() {
  server.on("/", HTTP_GET, []() {
    if (!requireAuth()) return;
    server.send_P(200, "text/html", INDEX_HTML);
  });
  server.on("/screenshot", HTTP_GET, []() {
    if (!requireAuth()) return;
    WiFiClient c = server.client();
    c.print("HTTP/1.1 200 OK\r\nContent-Type: image/bmp\r\nConnection: close\r\n\r\n");
    displayStreamBMP(c);
    c.stop();
  });
  server.on("/api/config", HTTP_GET, handleConfigGet);
  server.on("/api/config", HTTP_POST, handleConfigPost);
  server.on("/api/push", HTTP_POST, handlePush);
  server.onNotFound([]() {
    // captive-portal style redirect in AP mode
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });
  server.begin();
}

void webLoop() { server.handleClient(); }

bool webConfigChanged() {
  bool c = changed;
  changed = false;
  return c;
}
