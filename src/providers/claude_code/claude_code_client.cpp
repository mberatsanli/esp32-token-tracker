#include "claude_code_client.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// Onboarding screen: how to obtain the OAuth token and where to paste it.
void ClaudeCodeClient::connect(Screen &s, const Provider &p) const {
  if (p.apiKey.length()) {  // token set, first fetch pending
    ProviderClient::connect(s, p);
    return;
  }
  int cy = s.height() / 2;
  s.centerText("Connect Claude Code:", cy - 40, 2, s.muted());
  s.centerText("claude setup-token", cy - 12, 4, s.fg());
  s.centerText("run on your computer, then", cy + 16, 2, s.muted());
  s.centerText("paste the token in the web panel", cy + 34, 2, s.muted());
}

bool ClaudeCodeClient::fetch(Provider &p) {
  static const char *RL[] = {
      "anthropic-ratelimit-unified-5h-utilization",
      "anthropic-ratelimit-unified-7d-utilization",
      "anthropic-ratelimit-unified-5h-reset",
      "anthropic-ratelimit-unified-7d-reset",
  };
  err_ = "";

  // raw HTTPClient (not JsonHttp): the payload we need lives in the response
  // headers, the body is irrelevant
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(15000);
  if (!http.begin(client, "https://api.anthropic.com/v1/messages")) {
    err_ = "bad url";
    return false;
  }
  String auth = "Bearer " + p.apiKey;
  http.addHeader("Authorization", auth);
  http.addHeader("anthropic-version", "2023-06-01");
  http.addHeader("anthropic-beta", "oauth-2025-04-20");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "claude-code/2.1.5");
  http.collectHeaders(RL, 4);
  int code = http.POST(
      "{\"model\":\"claude-haiku-4-5-20251001\",\"max_tokens\":1,"
      "\"messages\":[{\"role\":\"user\",\"content\":\".\"}]}");
  String u5 = http.header(RL[0]);
  String u7 = http.header(RL[1]);
  String r5 = http.header(RL[2]);
  String r7 = http.header(RL[3]);
  http.end();

  Serial.printf("[claude] probe HTTP %d, 5h=%s 7d=%s\n", code, u5.c_str(), u7.c_str());
  if (u5.length() == 0 && u7.length() == 0) {
    err_ = code == 401 ? "bad/expired token"
         : code < 0    ? "no connection"
                       : "HTTP " + String(code);
    return false;
  }

  // utilization headers are 0.0-1.0, reset headers are epoch seconds
  p.pct5 = round(u5.toFloat() * 100.0f);
  p.used = round(u7.toFloat() * 100.0f);
  p.limit = 100;
  p.reset5 = (uint32_t)r5.toInt();
  p.reset7 = (uint32_t)r7.toInt();
  p.label = "5h " + String((int)p.pct5) + "% | wk " + String((int)p.used) + "%";
  return true;
}
