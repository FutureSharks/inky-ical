#include "display.h"
#include "config.h"
#include "calendar_sync.h"
#include <ArduinoLog.h>
#include <epd_driver.h>
#include "utilities.h"
#include "roboto.h"
#include "roboto12.h"
#include "roboto18.h"
#include "roboto32.h"
#include <cstdarg>

uint8_t *framebuffer = NULL;

// Position for the next init/status line drawn below the splash text on the
// display_init screen. Advances with each call to display_init_status()/display_error().
static int32_t init_status_x = 140;
static int32_t init_status_y = 0;

static void format_header_date(char *out, size_t out_len)
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    strlcpy(out, "Unknown date", out_len);
    return;
  }

  int day = timeinfo.tm_mday;
  const char *suffix = (day % 10 == 1 && day != 11)   ? "st"
                       : (day % 10 == 2 && day != 12) ? "nd"
                       : (day % 10 == 3 && day != 13) ? "rd"
                                                      : "th";
  char prefix[24];
  strftime(prefix, sizeof(prefix), "%A %d", &timeinfo);
  char month[16];
  strftime(month, sizeof(month), "%B", &timeinfo);
  snprintf(out, out_len, "%s%s of %s", prefix, suffix, month);
}

void display_init()
{
  Log.notice("Initializing display..." CR);

  delay(1000);
  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (!framebuffer)
  {
    Log.fatal("alloc memory failed !!!" CR);
    while (1)
      ;
  }
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  epd_init();

  epd_poweron();
  epd_clear();

  int32_t cursor_x = 140;
  int32_t cursor_y = 140;

  writeln(&Roboto32, "inky-ical", &cursor_x, &cursor_y, framebuffer);

  cursor_x = 140;
  cursor_y += 60;
  writeln(&Roboto12, "github.com/FutureSharks/inky-ical", &cursor_x, &cursor_y, framebuffer);

  cursor_x = 140;
  cursor_y += 40;
  writeln(&Roboto12, "by Max Williams", &cursor_x, &cursor_y, framebuffer);

  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();

  // Status/error lines are appended below the splash text as init proceeds.
  init_status_x = 140;
  init_status_y = cursor_y + 60;

  delay(1000);
}

static void display_init_line(const char *text)
{
  int32_t x = init_status_x;
  int32_t y = init_status_y;

  epd_poweron();
  writeln(&Roboto12, text, &x, &y, framebuffer);
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();

  init_status_y += 28;
}

void display_init_status(const char *fmt, ...)
{
  char message[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);

  display_init_line(message);
}

void display_calendar(const std::vector<CalendarEvent> &events)
{
  Log.notice("Rendering calendar events to display..." CR);

  if (events.empty())
  {
    Log.warning("No events to display" CR);
    display_error("No Events");
    return;
  }

  // Clear framebuffer (white background)
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  // Power on and prepare display
  epd_poweron();
  epd_clear();

  int32_t cursor_y = 42;
  int32_t border_buffer = 20;

  // Column layout: date | time | title, each in its own aligned column so
  // the eye can scan straight down instead of parsing one long string.
  const int32_t date_x = 20;
  const int32_t time_x = 260;
  const int32_t title_x = 420;
  const int32_t row_height = 42;

  int event_count = 0;

  // Write display header
  char header_text[32];
  format_header_date(header_text, sizeof(header_text));
  int32_t header_x = date_x;
  int32_t header_y = cursor_y + 5;
  writeln(&Roboto18, header_text, &header_x, &header_y, framebuffer);
  epd_draw_line(0, cursor_y + border_buffer, EPD_WIDTH, cursor_y + border_buffer, 1, framebuffer);
  cursor_y += cursor_y + border_buffer;

  for (int i = 0; i < events.size() && event_count < MAX_EVENTS_TO_DISPLAY; i++)
  {
    const CalendarEvent &event = events[i];

    if (cursor_y > EPD_HEIGHT - row_height)
      break;

    String title = event.title;
    if (event.calendar_name.length() && (DISPLAY_CALENDAR_NAME) > 0)
      title = title + " (" + event.calendar_name + ")";

    // Truncate the title if it would run off the right edge
    const int max_title_len = 26;
    if (title.length() > max_title_len)
      title = title.substring(0, max_title_len - 3) + "...";

    int32_t x;

    x = date_x;
    writeln(&Roboto18, event.date.c_str(), &x, &cursor_y, framebuffer);

    x = time_x;
    writeln(&Roboto18, event.time.c_str(), &x, &cursor_y, framebuffer);

    x = title_x;
    writeln(&Roboto18, title.c_str(), &x, &cursor_y, framebuffer);

    cursor_y += row_height;

    Log.trace("%s %s %s" CR, event.date.c_str(), event.time.c_str(), title.c_str());
    event_count++;
  }

  // Draw to display and power down
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();

  Log.notice("Display updated successfully" CR);
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

  display_init_line(line);
}
