#pragma once

#include <Arduino.h>

namespace MyDevice {
    void setupWiFi();   // connect and start OTA/mDNS
    void handleWiFi();  // call periodically in loop()
}