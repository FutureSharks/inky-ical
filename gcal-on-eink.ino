#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, Arduino IDE -> tools -> PSRAM -> OPI !!!"
#endif

#include "src/secrets.h"
#include "src/config.h"
#include "src/calendar_sync.h"
#include "src/display.h"
#include "src/setup_wifi.h"
#include "src/setup_time.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_sleep.h>
#include <time.h>
#include <ArduinoLog.h>
#include <epd_driver.h>

#define REFRESH_INTERVAL_MICROSECONDS (REFRESH_INTERVAL_HOURS * 3600ULL * 1000000ULL)

void setup()
{
  Serial.begin(115200);
  delay(100);
  Log.begin(LOG_LEVEL, &Serial);

  Log.notice("=== gcal-on-eink Starting ===" CR);
  Log.notice("Refresh interval: %d hours" CR, (int)REFRESH_INTERVAL_HOURS);

  // Initialize display, shows init/splash screen which stays up until the
  // calendar is successfully rendered
  display_init();

  // Connect to WiFi
  if (!wifi_connect())
  {
    Log.warning("WiFi connection failed, skipping calendar sync" CR);
    return;
  }

  // Set the clock
  if (!time_set())
  {
    Log.warning("Setting of time failed, skipping calendar sync" CR);
    return;
  }

  Log.notice("WiFi connection good, starting calendar sync" CR);
  // Fetch and parse all configured calendars, merged chronologically
  const char *calendar_urls[] = CALENDAR_URLS;
  std::vector<String> urls(calendar_urls, calendar_urls + (sizeof(calendar_urls) / sizeof(calendar_urls[0])));
  std::vector<CalendarEvent> events = fetch_calendars(urls, DISPLAY_DAYS, MAX_EVENTS_TO_DISPLAY);
  urls.clear();
  Log.notice("Fetched calendars and filtered %d events" CR, events.size());

  // Render events to display
  display_calendar(events);

  // Clean up
  events.clear();

  if (DEBUG_MODE)
  {
    Log.notice("Pausing indefinitely instead of deep sleep as DEBUG_MODE is enabled" CR);
    return;
  }

  Log.notice("Entering deep sleep for %d hours" CR, (int)REFRESH_INTERVAL_HOURS);

  // Shutdown all the things
  WiFi.disconnect(true);
  Serial.end();
  epd_poweroff_all();
  esp_sleep_enable_timer_wakeup(REFRESH_INTERVAL_MICROSECONDS);
  esp_deep_sleep_start();
}

void loop()
{
  // Should only be here for debug mode so just sleep
  delay(1000);
}
