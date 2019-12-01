#ifndef _BATTERY_H_
#define _BATTERY_H_

#define BATTERY_PIN A13
#define BATTERY_MAX_VOLTAGE 4.2
#define BATTERY_MIN_VOLTAGE 3

float read_battery_voltage();
int read_battery_percent();

#endif