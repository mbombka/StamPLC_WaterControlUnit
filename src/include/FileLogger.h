#pragma once
#include <LittleFS.h>
#include <time.h>

namespace FileLogger {
    void addLog(const String &msg);   // add log with pruning older than 24h
    String getLogHTML();  // get log
    void clearLog(); //clear log
}