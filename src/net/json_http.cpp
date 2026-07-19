#include "json_http.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

bool JsonHttp::get(const String &url, const std::vector<Header> &headers,
                   JsonDocument &out) {
  err_ = "";
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(client, url)) {
    err_ = "bad url";
    return false;
  }
  for (auto &h : headers) http.addHeader(h.first, h.second);
  int code = http.GET();
  if (code < 200 || code >= 300) {
    Serial.printf("[http] GET %s -> %d\n", url.c_str(), code);
    err_ = code < 0 ? "no connection" : "HTTP " + String(code);
    http.end();
    return false;
  }
  // getString (not getStream): HTTPClient only decodes chunked encoding here
  String body = http.getString();
  http.end();
  DeserializationError jsonErr = deserializeJson(out, body);
  if (jsonErr) {
    Serial.printf("[http] json error %s, body: %.120s\n", jsonErr.c_str(), body.c_str());
    err_ = "bad json";
  }
  return !jsonErr;
}

bool JsonHttp::postForm(const String &url, const String &body, JsonDocument &out,
                        int *httpCode) {
  err_ = "";
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(12000);
  if (!http.begin(client, url)) {
    err_ = "bad url";
    return false;
  }
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int code = http.POST(body);
  if (httpCode) *httpCode = code;
  String resp = http.getString();
  http.end();
  if (deserializeJson(out, resp)) {
    err_ = code < 0 ? "no connection" : "bad json";
    return false;
  }
  if (code < 200 || code >= 300) {
    err_ = "HTTP " + String(code);
    return false;
  }
  return true;
}
