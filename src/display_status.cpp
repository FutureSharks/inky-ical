/*
 * The non-calendar screens: the splash/init screen and the status lines that
 * accumulate on it during startup, plus the two terminal screens shown just
 * before the device powers down (low battery, startup failure).
 */

#include "display.h"
#include "display_internal.h"
#include "config.h"
#include <ArduinoLog.h>
#include <epd_driver.h>
#include <roboto_font.h>
#include <cstdarg>

using namespace layout;

// Where the next init/status line goes, and the Y of the first one, so the
// status area can be wiped and rewritten between retries without redrawing
// the splash text. Zero until display_init() has run.
static int32_t status_y = 0;
static int32_t status_origin_y = 0;

// Allocates the framebuffer and powers up the panel (display.cpp).
bool display_hardware_init();

bool display_init()
{
  Log.notice("Initializing display..." CR);

  if (!display_hardware_init())
  {
    return false;
  }

  int32_t x = MESSAGE_LEFT;
  int32_t y = MESSAGE_TITLE_Y;
  write_roboto("inky-ical", &x, &y, framebuffer, SIZE_SPLASH, BLACK);

  x = MESSAGE_LEFT;
  y += 60;
  write_roboto("github.com/FutureSharks/inky-ical", &x, &y, framebuffer, SIZE_SMALL, BLACK);

  x = MESSAGE_LEFT;
  y += 40;
  write_roboto("by Max Williams", &x, &y, framebuffer, SIZE_SMALL, BLACK);

#if DEBUG_MODE
  int32_t debug_message_x = EPD_WIDTH - MARGIN;
  int32_t debug_message_y = EPD_HEIGHT - MARGIN;
  write_roboto_right("DEBUG MODE", debug_message_x, debug_message_y, framebuffer, SIZE_SMALL, BLACK);
#endif

  display_draw_battery_icon();

  const int32_t rule_y = 240;
  epd_draw_line(MESSAGE_RULE_INSET, rule_y, EPD_WIDTH - MESSAGE_RULE_INSET, rule_y,
                RULE_HEAVY, framebuffer);

  display_flush();

  // Status/error lines are appended below the splash text as init proceeds.
  status_y = y + 60;
  status_origin_y = status_y;

  return true;
}

void display_status_line(const char *text)
{
  if (!display_ready() || status_origin_y == 0)
  {
    return;
  }

  int32_t x = MESSAGE_LEFT;
  int32_t y = status_y;
  write_roboto(text, &x, &y, framebuffer, SIZE_SMALL, BLACK);
  display_flush();

  status_y += STATUS_LINE_HEIGHT;
}

void display_init_status(const char *fmt, ...)
{
  char message[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);

  display_status_line(message);
}

void display_error(const char *fmt, ...)
{
  char message[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);

  char line[136];
  snprintf(line, sizeof(line), "ERROR: %s", message);

  display_status_line(line);
}

void display_init_status_reset()
{
  if (!display_ready() || status_origin_y == 0)
  {
    return;
  }

  // Wipe everything below the first status line, so accumulated lines from a
  // failed attempt don't run off the bottom of the screen over several retries.
  int32_t top = status_origin_y - STATUS_LINE_HEIGHT;
  epd_fill_rect(0, top, EPD_WIDTH, EPD_HEIGHT - top, 0xFF, framebuffer);

  // A full repaint, not a flush: the old status lines are ink on the panel and
  // only epd_clear() removes them. The splash text is still in the framebuffer,
  // so it is redrawn as part of the same repaint.
  display_repaint();

  status_y = status_origin_y;
}

void display_low_battery()
{
  if (!display_ready())
  {
    return;
  }

  std::vector<String> body;
  body.push_back("Please recharge the device.");
  display_message_screen("Battery low", body, "Powered down.", layout::SIZE_BODY);

  Log.notice("Displayed low battery message" CR);
}

void display_init_failure_summary(int attempts, const std::vector<String> &failures)
{
  if (!display_ready())
  {
    return;
  }

  std::vector<String> body;
  body.push_back(String("Gave up after ") + attempts + " attempts. Errors:");
  for (size_t i = 0; i < failures.size(); i++)
  {
    body.push_back(String((int)(i + 1)) + ". " + failures[i]);
  }

  // Errors are long, so the list is set small enough to fit the panel width.
  display_message_screen("Startup failed", body, "Powered down. Press reset to retry.",
                         layout::SIZE_SMALL);

  Log.notice("Displayed startup failure summary" CR);
}
