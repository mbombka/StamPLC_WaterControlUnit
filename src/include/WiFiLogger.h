#pragma once
#include <WiFi.h>

namespace WiFiLogger {
    void begin(uint16_t port = 23);
    void handle();
    size_t print(const char* s);
    size_t println(const char* s);
    size_t printf(const char* fmt, ...);
    size_t print(const String& s);
    size_t println(const String& s);
}