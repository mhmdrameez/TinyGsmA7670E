/**
 * @file   TinyGsmA7670EHttp.h
 * @brief  Plain HTTP/1.1 over a TinyGSM TCP client, built byte by byte.
 *
 * The A7670E's own HTTP stack (AT+HTTPPARA="USERDATA") can't take more
 * than one header cleanly: CR/LF can't go inside the quoted AT argument, so
 * headers get merged into one line and the server rejects or ignores them.
 * This helper opens a TCP socket and writes the request itself, the same
 * bytes curl sends:
 *
 *   POST /path HTTP/1.1\r\n
 *   Host: example.com\r\n
 *   User-Agent: curl/8.4.0\r\n
 *   Accept: * / *\r\n
 *   <your headers, one per line>\r\n
 *   Content-Length: N\r\n
 *   Connection: close\r\n
 *   \r\n
 *   <body>
 *
 * Example:
 *   const char* hdrs[] = { "Content-Type: application/json",
 *                          "apikey: xxx",
 *                          "Authorization: Bearer yyy" };
 *   String resp;
 *   int status = A7670EHttp::request(client, "POST", "example.com", 80,
 *                                    "/rest/v1/table", hdrs, 3,
 *                                    "{\"a\":1}", &resp);
 *
 * Plain http:// only. HTTPS needs the modem's SSL socket commands.
 */

#ifndef TINYGSM_A7670E_HTTP_H_
#define TINYGSM_A7670E_HTTP_H_

#include <Arduino.h>
#include <Client.h>

#ifndef A7670E_HTTP_USER_AGENT
#define A7670E_HTTP_USER_AGENT "curl/8.4.0"
#endif
#ifndef A7670E_HTTP_CHUNK
#define A7670E_HTTP_CHUNK 1024  // bytes per AT+CIPSEND (A7670E max is 1500)
#endif

namespace A7670EHttp {

enum Error : int {
  ERR_CONNECT  = -1,  // TCP connect failed
  ERR_SEND     = -2,  // modem accepted fewer bytes than sent
  ERR_TIMEOUT  = -3,  // no status line before timeout
  ERR_RESPONSE = -4,  // status line not "HTTP/1.x NNN"
};

// Write all of buf, in chunks the modem accepts.
inline bool writeAll(Client& c, const uint8_t* buf, size_t len) {
  while (len > 0) {
    size_t n    = len > A7670E_HTTP_CHUNK ? A7670E_HTTP_CHUNK : len;
    size_t sent = c.write(buf, n);
    if (sent != n) return false;
    buf += n;
    len -= n;
  }
  return true;
}

// Build the request text exactly as it will go on the wire.
inline String build(const char* method, const char* host, uint16_t port,
                    const String& path, const char* const* headers,
                    size_t headerCount, const String& body) {
  String req;
  req.reserve(160 + path.length() + body.length() + headerCount * 48);
  req += method;
  req += ' ';
  req += path.length() ? path : String("/");
  req += " HTTP/1.1\r\n";
  req += "Host: ";
  req += host;
  if (port != 80) {
    req += ':';
    req += port;
  }
  req += "\r\n";
  req += "User-Agent: " A7670E_HTTP_USER_AGENT "\r\n";
  req += "Accept: */*\r\n";
  for (size_t i = 0; i < headerCount; i++) {
    if (!headers[i] || !headers[i][0]) continue;
    req += headers[i];
    req += "\r\n";
  }
  if (body.length() > 0 || strcmp(method, "POST") == 0 ||
      strcmp(method, "PUT") == 0 || strcmp(method, "PATCH") == 0) {
    req += "Content-Length: ";
    req += body.length();
    req += "\r\n";
  }
  req += "Connection: close\r\n\r\n";
  req += body;
  return req;
}

// Read one line (without CR/LF). Returns false on timeout.
inline bool readLine(Client& c, String& line, uint32_t deadline) {
  line = "";
  while ((int32_t)(deadline - millis()) > 0) {
    if (!c.available()) {
      if (!c.connected()) return line.length() > 0;
      delay(5);
      continue;
    }
    char ch = static_cast<char>(c.read());
    if (ch == '\n') return true;
    if (ch != '\r') line += ch;
  }
  return false;
}

/**
 * Send one request and read the reply.
 * @return HTTP status (e.g. 200, 201, 400), or a negative Error.
 * @param respBody  if not null, receives up to maxBody bytes of the body.
 */
inline int request(Client& c, const char* method, const char* host,
                   uint16_t port, const String& path,
                   const char* const* headers, size_t headerCount,
                   const String& body, String* respBody = nullptr,
                   uint32_t timeoutMs = 30000UL, size_t maxBody = 512) {
  if (respBody) *respBody = "";

  if (!c.connect(host, port)) return ERR_CONNECT;

  const String req = build(method, host, port, path, headers, headerCount,
                           body);
  if (!writeAll(c, reinterpret_cast<const uint8_t*>(req.c_str()),
                req.length())) {
    c.stop();
    return ERR_SEND;
  }

  const uint32_t deadline = millis() + timeoutMs;

  // Status line: "HTTP/1.1 201 Created"
  String line;
  if (!readLine(c, line, deadline)) {
    c.stop();
    return ERR_TIMEOUT;
  }
  int sp = line.indexOf(' ');
  if (!line.startsWith("HTTP/") || sp < 0) {
    c.stop();
    return ERR_RESPONSE;
  }
  const int status = line.substring(sp + 1, sp + 4).toInt();

  // Headers until the empty line.
  long contentLength = -1;
  while (readLine(c, line, deadline)) {
    if (line.length() == 0) break;
    String lower = line;
    lower.toLowerCase();
    if (lower.startsWith("content-length:")) {
      contentLength = line.substring(15).toInt();
    }
  }

  // Body (until Content-Length, close, timeout or maxBody).
  if (respBody) {
    long got = 0;
    while ((int32_t)(deadline - millis()) > 0 &&
           (contentLength < 0 || got < contentLength)) {
      if (c.available()) {
        char ch = static_cast<char>(c.read());
        got++;
        if (respBody->length() < maxBody) *respBody += ch;
      } else if (!c.connected()) {
        break;
      } else {
        delay(5);
      }
    }
  }

  c.stop();
  return status;
}

inline int get(Client& c, const char* host, uint16_t port, const String& path,
               const char* const* headers, size_t headerCount,
               String* respBody = nullptr, uint32_t timeoutMs = 30000UL) {
  return request(c, "GET", host, port, path, headers, headerCount, String(),
                 respBody, timeoutMs);
}

inline int post(Client& c, const char* host, uint16_t port,
                const String& path, const char* const* headers,
                size_t headerCount, const String& body,
                String* respBody = nullptr, uint32_t timeoutMs = 30000UL) {
  return request(c, "POST", host, port, path, headers, headerCount, body,
                 respBody, timeoutMs);
}

}  // namespace A7670EHttp

#endif  // TINYGSM_A7670E_HTTP_H_
