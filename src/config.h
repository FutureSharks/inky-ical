#ifndef CONFIG_H
#define CONFIG_H

#include <ArduinoLog.h>

// The local timezone is set by the POSIX TZ rule in src/setup_time.cpp,
// not here - change it there.

// ===== Display Configuration =====
#define SYNC_HOUR 4                 // Hour (0-23, local time) to wake and refresh each day
#define SYNC_MINUTE 0               // Minute (0-59, local time) to wake and refresh each day
#define DISPLAY_DAYS 180            // Show events for next N days
#define MAX_EVENTS_TO_DISPLAY 8     // Max events shown, merged across all calendars
#define MAX_EVENTS_PER_CALENDAR 500 // Safety cap on events collected from one feed before merging
#define NTP_SERVER "pool.ntp.org"   // SNTP server
#define DISPLAY_CALENDAR_NAME false // Show the name of the calendar, if available

// ===== Battery Configuration =====
#define BATTERY_LOW_SHUTDOWN_PERCENT 5 // At or below this level, show a low battery message and power down instead of syncing

// ===== Startup Retry Configuration =====
#define MAX_INIT_ATTEMPTS 5        // Attempts at WiFi + NTP before giving up and powering down
#define INIT_RETRY_DELAY_SECONDS 5 // Wait between failed init attempts

// ===== Logging Configuration =====
#define DEBUG_MODE false // Set to true for logging and no deep sleep

#if DEBUG_MODE
#define LOG_LEVEL LOG_LEVEL_TRACE
#else
#define LOG_LEVEL LOG_LEVEL_NOTICE
#endif

#endif
