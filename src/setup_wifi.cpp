#include <WiFi.h>
#include <ArduinoLog.h>
#include "secrets.h"
#include "setup_wifi.h"
#include "display.h"

bool wifi_connect()
{
  Log.notice("Connecting to WiFi: %s" CR, WIFI_SSID);
  display_init_status("Connecting to WiFi: %s", WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20)
  {
    delay(500);
    Log.trace("." CR);
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    Log.error("WiFi connection failed" CR);
    display_error("WiFi connection failed");
    return false;
  }

  delay(1000);
  Log.notice("Connected! IP: %s" CR, WiFi.localIP().toString().c_str());
  display_init_status("Connected, IP: %s", WiFi.localIP().toString().c_str());
  return true;
}
