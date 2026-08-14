#include "battery.h"
#include <Arduino.h>

#define BATTERY_ADC_PIN 14
#define BATTERY_ADC_MAX 4095.0
#define BATTERY_ADC_REFERENCE_VOLTAGE 3.3
// Board has a 2:1 voltage divider between the battery and the ADC pin.
#define BATTERY_VOLTAGE_DIVIDER 2.0
#define BATTERY_MIN_VOLTAGE 3.3 // 0%, typical LiPo cutoff
#define BATTERY_MAX_VOLTAGE 4.2 // 100%, typical LiPo full charge

uint8_t battery_get_percentage()
{
  uint32_t raw = analogRead(BATTERY_ADC_PIN);
  float voltage = (raw / BATTERY_ADC_MAX) * BATTERY_ADC_REFERENCE_VOLTAGE * BATTERY_VOLTAGE_DIVIDER;

  float percentage = (voltage - BATTERY_MIN_VOLTAGE) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE) * 100.0;
  if (percentage > 100.0)
    percentage = 100.0;
  if (percentage < 0.0)
    percentage = 0.0;

  return (uint8_t)percentage;
}
