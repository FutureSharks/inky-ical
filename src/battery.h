#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>

/*
 * Read the battery level from the ADC pin and return it as a percentage
 * (0-100), based on a linear mapping between empty and full LiPo voltage.
 */
uint8_t battery_get_percentage();

#endif
