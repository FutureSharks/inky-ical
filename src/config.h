#ifndef CONFIG_H
#define CONFIG_H

#include <ArduinoLog.h>

// ===== Display Configuration =====
#define REFRESH_INTERVAL_HOURS 6    // Wake and refresh every N hours
#define TIMEZONE_OFFSET 0           // UTC offset (e.g., 2 for UTC+2, -5 for UTC-5)
#define DISPLAY_DAYS 180            // Show events for next N days
#define MAX_EVENTS_TO_DISPLAY 9     // Max events shown, merged across all calendars
#define NTP_SERVER "pool.ntp.org"   // SNTP server
#define DISPLAY_CALENDAR_NAME false // Show the name of the calendar, if available

// ===== Logging Configuration =====
#define DEBUG_MODE true // Set to true for logging and no deep sleep

#if DEBUG_MODE
#define LOG_LEVEL LOG_LEVEL_TRACE
#else
#define LOG_LEVEL LOG_LEVEL_NOTICE
#endif

#endif
