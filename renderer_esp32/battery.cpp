#include <Arduino.h>
#include "battery.h"

float read_battery_voltage() {
    int raw;
    raw = analogRead(BATTERY_PIN);
    return (((float)raw) / 4095) * 2 * 3.3 * 1.1;
}

int read_battery_percent() {
    float v;
    v = read_battery_voltage();
    // This is a really rough estimate as the battery drain curve isn't constant
    // To get more accurate would need to monitor over time
    return ((v - BATTERY_MIN_VOLTAGE) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE)) * 100;
}