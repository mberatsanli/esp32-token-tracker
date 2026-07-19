#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <utility>
#include <vector>

// Thin HTTPS+JSON transport shared by provider clients.
// v1 trust model: TLS without certificate validation (setInsecure) — the
// device talks only to fixed, well-known hosts; CA pinning is a known TODO.
class JsonHttp {
 public:
  using Header = std::pair<const char *, String>;

  // GET url with headers, parse JSON body into out.
  bool get(const String &url, const std::vector<Header> &headers, JsonDocument &out);

  // POST x-www-form-urlencoded body, parse JSON response into out.
  // Returns true only for 2xx; out still holds the parsed error body when
  // the server answered 4xx/5xx with JSON (needed for OAuth error codes).
  bool postForm(const String &url, const String &body, JsonDocument &out,
                int *httpCode = nullptr);

  const String &error() const { return err_; }

 private:
  String err_;
};
