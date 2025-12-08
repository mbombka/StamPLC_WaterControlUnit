#include <Arduino.h>  // optional if you're using ESP32
class TimeHM {
public:
    int hour;
    int minute;
    int duration; //duration in minutes

    // Constructor with default values
    TimeHM(int h = 0, int m = 0, int d = 15) {
        // Normalize
        hour = h % 24;
        minute = m % 60;
        duration = d;
    }

    // Comparison operators
    bool operator==(const TimeHM& other) const {
        return hour == other.hour && minute == other.minute;
    }
    void print() const {
        Serial.printf("%02d:%02d duration: %02d", hour, minute, duration);
    }
};
