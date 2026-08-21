#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, Arduino IDE -> tools -> PSRAM -> OPI !!!"
#endif

#include "src/secrets.h"
#include "src/config.h"
#include "src/calendar_sync.h"
#include "src/display.h"
#include "src/setup_wifi.h"
#include "src/setup_time.h"
#include "src/battery.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_sleep.h>
#include <time.h>
#include <ArduinoLog.h>
#include <epd_driver.h>

// Set when startup could not complete; loop() then powers the device down for
// good rather than scheduling another refresh.
static bool init_failed = false;

// Seconds from now (local time) until the next SYNC_HOUR:SYNC_MINUTE,
// rolling over to tomorrow if that time has already passed today. Falls back
// to a fixed 24h sleep if the clock was never set.
static uint64_t seconds_until_next_sync()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
    return 24ULL * 3600ULL;

  time_t now = mktime(&timeinfo);

  struct tm next_tm = timeinfo;
  next_tm.tm_hour = SYNC_HOUR;
  next_tm.tm_min = SYNC_MINUTE;
  next_tm.tm_sec = 0;
  time_t next = mktime(&next_tm);

  if (next <= now)
  {
    next_tm.tm_mday += 1;
    next = mktime(&next_tm);
  }

  return (uint64_t)difftime(next, now);
}

/*
 * One attempt at the network-dependent part of startup: WiFi association then
 * NTP sync. On failure the reason is written to `reason` for the summary screen.
 */
static bool init_network(char *reason, size_t reason_len)
{
  if (!wifi_connect())
  {
    snprintf(reason, reason_len, "WiFi connection to %s failed", WIFI_SSID);
    return false;
  }

  if (!time_set())
  {
    snprintf(reason, reason_len, "NTP time sync from %s failed", NTP_SERVER);
    return false;
  }

  return true;
}

// Tear down WiFi between attempts so the next wifi_connect() starts clean
static void reset_network()
{
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(500);
}

/*
 * Bring up WiFi and the clock, retrying up to MAX_INIT_ATTEMPTS times. Every
 * failure is appended to `failures` so all of them can be shown on the
 * summary screen. `attempts_made` receives the number of attempts tried.
 */
static bool init_network_with_retries(std::vector<String> &failures, int &attempts_made)
{
  for (int attempt = 1; attempt <= MAX_INIT_ATTEMPTS; attempt++)
  {
    attempts_made = attempt;
    Log.notice("Init attempt %d of %d" CR, attempt, (int)MAX_INIT_ATTEMPTS);

    if (attempt > 1)
    {
      // Start each retry from a clean status area, otherwise the accumulated
      // lines run off the bottom of the panel
      display_init_status_reset();
      display_init_status("Init attempt %d of %d", attempt, (int)MAX_INIT_ATTEMPTS);
    }

    char reason[128];
    if (init_network(reason, sizeof(reason)))
    {
      return true;
    }

    Log.error("Init attempt %d failed: %s" CR, attempt, reason);
    failures.push_back(String("Attempt ") + attempt + ": " + reason);

    reset_network();

    if (attempt < MAX_INIT_ATTEMPTS)
    {
      display_init_status("Retrying in %ds...", (int)INIT_RETRY_DELAY_SECONDS);
      delay(INIT_RETRY_DELAY_SECONDS * 1000UL);
    }
  }

  return false;
}

// The URLs configured in secrets.h, as a vector.
static std::vector<String> calendar_urls()
{
  const char *urls[] = CALENDAR_URLS;
  return std::vector<String>(urls, urls + (sizeof(urls) / sizeof(urls[0])));
}

void setup()
{
  Serial.begin(115200);
  delay(100);
  Log.begin(LOG_LEVEL, &Serial);

  Log.notice("=== inky-ical Starting ===" CR);
  Log.notice("Daily sync time: %02d:%02d" CR, (int)SYNC_HOUR, (int)SYNC_MINUTE);

  // Shows the init/splash screen, which stays up until the calendar is
  // successfully rendered
  if (!display_init())
  {
    Log.fatal("Display initialization failed, powering down" CR);
    init_failed = true;
    return;
  }

  // Check battery before touching WiFi/network so a critically low battery
  // powers straight down instead of burning what little charge is left on a sync.
  uint8_t battery_percent = battery_get_percentage();
  if (battery_percent <= BATTERY_LOW_SHUTDOWN_PERCENT)
  {
    Log.fatal("Battery critically low (%d%%), powering down without syncing" CR, battery_percent);
    display_low_battery();
    init_failed = true;
    return;
  }

  std::vector<String> failures;
  int attempts = 0;
  if (!init_network_with_retries(failures, attempts))
  {
    Log.fatal("Init failed after %d attempts, powering down" CR, attempts);
    display_init_failure_summary(attempts, failures);
    init_failed = true;
    return;
  }

  Log.notice("WiFi connection good, starting calendar sync" CR);
  std::vector<CalendarEvent> events = fetch_calendars(calendar_urls(), DISPLAY_DAYS, MAX_EVENTS_TO_DISPLAY);
  Log.notice("Fetched calendars and filtered %d events" CR, events.size());

  display_calendar(events);
}

// Power the panel and radio down and deep sleep. With wake_seconds of zero no
// wake-up timer is armed, so the device sleeps until the reset button is pressed.
static void power_down(uint64_t wake_seconds)
{
  WiFi.disconnect(true);
  Serial.end();
  epd_poweroff_all();
  if (wake_seconds > 0)
  {
    esp_sleep_enable_timer_wakeup(wake_seconds * 1000000ULL);
  }
  esp_deep_sleep_start();
}

void loop()
{
  // Keep Serial alive for debugging instead of sleeping, which would cut it off
  if (DEBUG_MODE || Serial.isConnected())
  {
    Log.notice("Pausing indefinitely instead of deep sleep as DEBUG_MODE=%d or Serial=%d" CR,
               DEBUG_MODE, Serial.isConnected());
    delay(999999);
    return;
  }

  if (init_failed)
  {
    // Sleep with no wake-up timer so the failure summary stays on the e-ink
    // panel until the reset button is pressed.
    Log.fatal("Powering down indefinitely after failed init" CR);
    power_down(0);
  }

  uint64_t sleep_seconds = seconds_until_next_sync();
  Log.notice("Entering deep sleep for %d seconds, until next sync at %02d:%02d" CR,
             (int)sleep_seconds, (int)SYNC_HOUR, (int)SYNC_MINUTE);
  power_down(sleep_seconds);
}
