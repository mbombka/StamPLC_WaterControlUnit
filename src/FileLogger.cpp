#include "include/FileLogger.h"
#include <LittleFS.h>
#include <time.h>

namespace FileLogger {
    void addLog(const String &msg) {
        // Get current time
        time_t now;
        time(&now);

        // Read existing logs
        File f = LittleFS.open("/log.txt", "r");
        String newLog;
        while(f.available()) {
            String line = f.readStringUntil('\n');
            if(line.length() < 19) continue; // invalid
            struct tm tmOld;
            strptime(line.substring(0, 19).c_str(), "%Y-%m-%d %H:%M:%S", &tmOld);
            time_t t = mktime(&tmOld);
            if(difftime(now, t) <= 24*3600) { // keep only last 24h
                newLog += line + "\n";
            }
        }
        f.close();

        // Add new entry with timestamp
        struct tm *timeinfo = localtime(&now);
        char buf[20];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", timeinfo);
        newLog += String(buf) + " - " + msg + "\n";

        // Write back
        File f2 = LittleFS.open("/log.txt", "w");
        f2.print(newLog);
        f2.close();
    }

    String getLogHTML() {
    File f = LittleFS.open("/log.txt", "r");
    String html = "<html><body><h1>ESP32 Log (Last 24h)</h1><pre>";
    while (f.available()) {
        html += char(f.read());
    }
    f.close();
    html += "</pre></body></html>";
    return html; 
    }
    void clearLog() {
    File f = LittleFS.open("/log.txt", "w");  // "w" truncates the file
    if (f) {
        f.print("");   // optional, file is already empty
        f.close();
    }
}
}
