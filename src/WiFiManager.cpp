#include "include/WiFiManager.h"
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

#include "include/secrets.h"  // contains WIFI_SSID and WIFI_PASSWORD

namespace MyDevice {

    static unsigned long lastReconnectAttempt = 0;

    void setupWiFi() {
        Serial.println("\n[WiFi] Connecting...");

        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        unsigned long startAttemptTime = millis();

        // Try for up to 10 seconds
        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
            Serial.print(".");
            delay(500);
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\n[WiFi] Connected to %s\n", WIFI_SSID);
            Serial.printf("[WiFi] IP address: %s\n", WiFi.localIP().toString().c_str());

            // Start OTA
            ArduinoOTA.setHostname("M5StamPLC");
            ArduinoOTA.begin();
            Serial.println("[OTA] Ready for uploads (use 'Upload via OTA' in PlatformIO)");

            // Start mDNS for hostname.local access
            if (MDNS.begin("M5StamPLC")) {
                Serial.println("[mDNS] Service started: http://M5StamPLC.local");
            }
        } else {
            Serial.println("\n[WiFi] Connection failed!");
        }
    }

    void handleWiFi() {
        // Handle OTA events
        ArduinoOTA.handle();

        // Reconnect logic if disconnected
        if (WiFi.status() != WL_CONNECTED) {
            unsigned long now = millis();
            if (now - lastReconnectAttempt > 10000) { // every 10 sec
                Serial.println("[WiFi] Disconnected, retrying...");
                WiFi.reconnect();
                lastReconnectAttempt = now;
            }
        }
    }

}  // namespace MyDevice
