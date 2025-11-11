#include "include/WiFiLogger.h"
#include <Arduino.h>
#include <stdarg.h>

namespace {
    WiFiServer logServer(23);
    WiFiClient logClient;
}

namespace WiFiLogger {

void begin(uint16_t port) {
    logServer = WiFiServer(port);
    logServer.begin();
    logServer.setNoDelay(true);
    Serial.printf("[WiFiLog] Server on %u\n", port);
}

void handle() {
    if (!logClient || !logClient.connected()) {
        WiFiClient newClient = logServer.available();
        if (newClient) {
            if (logClient) logClient.stop();   // allow only one at a time
            logClient = newClient;
            logClient.println("[WiFiLog] Connected");
        }
    }
}

static size_t writeToBoth(const char* s, size_t len) {
    Serial.write((const uint8_t*)s, len);
    if (logClient && logClient.connected()) {
        logClient.write((const uint8_t*)s, len);
    }
    return len;
}

size_t print(const char* s)   { 
    return writeToBoth(s, strlen(s)); 
}
size_t println(const char* s) {
    size_t n = writeToBoth(s, strlen(s));
    return n + writeToBoth("\r\n", 2);
}

size_t print(const String& s) {
    return print(s.c_str());
}

size_t println(const String& s) {
    return println(s.c_str());
}

size_t printf(const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return 0;
    if ((size_t)n >= sizeof(buf)) n = sizeof(buf) - 1;
    return writeToBoth(buf, (size_t)n);
}

} // namespace WiFiLogger