#include "battery.h"
#include <Arduino.h>

#define BATTERY_ADC_PIN 14
// Board has a 2:1 voltage divider between the battery and the ADC pin.
#define BATTERY_VOLTAGE_DIVIDER 2.0
#define BATTERY_MIN_VOLTAGE 3300.0 // 0%, typical LiPo cutoff (mV)
#define BATTERY_MAX_VOLTAGE 4200.0 // 100%, typical LiPo full charge (mV)

uint8_t battery_get_percentage()
{
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogReadMilliVolts(BATTERY_ADC_PIN);
  }
  float voltage = (sum / 16.0) * BATTERY_VOLTAGE_DIVIDER;
  if (voltage > BATTERY_MAX_VOLTAGE)
    voltage = BATTERY_MAX_VOLTAGE;

  float percentage = (voltage - BATTERY_MIN_VOLTAGE) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE) * 100.0;
  if (percentage > 100.0)
    percentage = 100.0;
  if (percentage < 0.0)
    percentage = 0.0;

  return (uint8_t)percentage;
}
