#include <Arduino.h>
#include <ArduinoLog.h>
#include "time.h"
#include "esp_sntp.h"
#include "config.h"
#include "setup_time.h"
#include "display.h"

const long gmt_offset_sec = 3600;
const int daylight_offset_sec = 3600;
const char *time_zone = "CET-1CEST,M3.5.0,M10.5.0/3"; // TimeZone rule for Europe/Rome including daylight adjustment rules (optional)

void printLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Log.warning("No time available (yet)" CR);
    return;
  }
  char time_str[64];
  strftime(time_str, sizeof(time_str), "%A, %B %d %Y %H:%M:%S", &timeinfo);
  Log.notice("%s" CR, time_str);
}

void time_available(struct timeval *t)
{
  Log.notice("Got time adjustment from NTP!" CR);
  printLocalTime();
}

bool time_set()
{
  display_init_status("Syncing time from NTP: %s", NTP_SERVER);

  sntp_set_time_sync_notification_cb(time_available);
  configTzTime(time_zone, NTP_SERVER);

  // Allow SNTP to sync
  delay(5000);

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10000))
  {
    Log.error("Failed to sync time from NTP" CR);
    display_error("Failed to sync time from NTP");
    return false;
  }

  display_init_status("Time synced");
  return true;
}
