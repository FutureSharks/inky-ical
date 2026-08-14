#include "display.h"
#include "config.h"
#include "calendar_sync.h"
#include "battery.h"
#include <ArduinoLog.h>
#include <epd_driver.h>
#include "utilities.h"
#include <roboto_font.h>
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

// Draws "<percentage>%" right-aligned so its right edge sits at right_edge_x.
// Measures the text width with a dry run (NULL framebuffer) since
// write_roboto only draws left-to-right from a starting cursor position.
static void draw_battery_percentage(int32_t right_edge_x, int32_t y, uint8_t size)
{
  char text[16];
  snprintf(text, sizeof(text), "%d%%", battery_get_percentage());

  int32_t measure_x = 0;
  int32_t measure_y = 0;
  write_roboto(text, &measure_x, &measure_y, NULL, size, 0);
  int32_t text_width = measure_x;

  int32_t x = right_edge_x - text_width;
  int32_t draw_y = y;
  write_roboto(text, &x, &draw_y, framebuffer, size, 7);
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

  int32_t cursor_x = 100;
  int32_t cursor_y = 120;

  write_roboto("inky-ical", &cursor_x, &cursor_y, framebuffer, 48, 0);

  cursor_x = 100;
  cursor_y += 60;
  write_roboto("github.com/FutureSharks/inky-ical", &cursor_x, &cursor_y, framebuffer, 12, 0);

  cursor_x = 100;
  cursor_y += 40;
  write_roboto("by Max Williams", &cursor_x, &cursor_y, framebuffer, 12, 0);

  draw_battery_percentage(EPD_WIDTH - 20, 42, 12);

  int32_t line_y = 240;
  epd_draw_line(40, line_y, EPD_WIDTH - 40, line_y, 8, framebuffer);

  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();

  // Status/error lines are appended below the splash text as init proceeds.
  init_status_x = 100;
  init_status_y = cursor_y + 60;

  delay(1000);
}

static void display_init_line(const char *text)
{
  int32_t x = init_status_x;
  int32_t y = init_status_y;

  epd_poweron();
  write_roboto(text, &x, &y, framebuffer, 12, 0);
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

static void display_events_style_one(const std::vector<CalendarEvent> &events)
{
  // Column layout: date | time | title, each in its own aligned column so
  // the eye can scan straight down instead of parsing one long string.
  const int32_t title_x = 480;
  const int32_t row_height = 54;
  const int32_t border_buffer = 20;

  // Set starting place for event loop
  int32_t event_cursor_y = 110;
  int event_count = 0;

  for (int i = 0; i < events.size() && event_count < MAX_EVENTS_TO_DISPLAY; i++)
  {
    const CalendarEvent &event = events[i];

    if (event_cursor_y > EPD_HEIGHT - row_height)
      break;

    String title = event.title;
    if (event.calendar_name.length() && (DISPLAY_CALENDAR_NAME) > 0)
      title = title + " (" + event.calendar_name + ")";

    // Truncate the title if it would run off the right edge
    const int max_title_len = 26;
    if (title.length() > max_title_len)
      title = title.substring(0, max_title_len - 3) + "...";

    int32_t x;
    int32_t y;

    // today, tomorrow, in n days
    x = border_buffer;
    y = event_cursor_y;
    String time_text = String(event.date.c_str()) + ", " + event.time.c_str();
    write_roboto(time_text.c_str(), &x, &y, framebuffer, 18, 0);

    // event title
    x = title_x;
    y = event_cursor_y;
    write_roboto(title.c_str(), &x, &y, framebuffer, 18, 0);

    event_cursor_y += row_height;

    Log.trace("%s %s %s" CR, event.date.c_str(), event.time.c_str(), title.c_str());
    event_count++;
  }
}

static void display_events_style_two(const std::vector<CalendarEvent> &events)
{
  const int32_t title_x = 340;
  const int32_t row_height = 63;
  const int32_t border_buffer = 20;

  // Set starting place for event loop
  int32_t event_cursor_y = 92;
  int event_count = 0;

  for (int i = 0; i < events.size() && event_count < MAX_EVENTS_TO_DISPLAY; i++)
  {
    const CalendarEvent &event = events[i];

    if (event_cursor_y > EPD_HEIGHT - row_height)
      break;

    String title = event.title;
    if (event.calendar_name.length() && (DISPLAY_CALENDAR_NAME) > 0)
      title = title + " (" + event.calendar_name + ")";

    // Truncate the title if it would run off the right edge
    const int max_title_len = 26;
    if (title.length() > max_title_len)
      title = title.substring(0, max_title_len - 3) + "...";

    int32_t x;
    int32_t y;

    // today, tomorrow, in n days
    x = border_buffer;
    y = event_cursor_y;
    write_roboto(event.date.c_str(), &x, &y, framebuffer, 12, 0);

    // event time
    x = border_buffer;
    y = event_cursor_y + 24;
    String time_text = " > " + String(event.time.c_str());
    write_roboto(time_text.c_str(), &x, &y, framebuffer, 12, 0);

    // event title
    x = title_x;
    y = event_cursor_y + 20;
    write_roboto(title.c_str(), &x, &y, framebuffer, 18, 0);

    event_cursor_y += row_height;

    Log.trace("%s %s %s" CR, event.date.c_str(), event.time.c_str(), title.c_str());
    event_count++;
  }
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

  const int32_t border_buffer = 20;

  // Write display header
  char header_text[32];
  format_header_date(header_text, sizeof(header_text));
  int32_t header_x = border_buffer;
  int32_t header_y = 47;
  write_roboto(header_text, &header_x, &header_y, framebuffer, 18, 0);
  draw_battery_percentage(EPD_WIDTH - 20, 42, 12);
  epd_draw_line(0, header_y + 15, EPD_WIDTH, header_y + 15, 1, framebuffer);

#if DISPLAY_EVENTS_STYLE == 2
  display_events_style_two(events);
#else
  display_events_style_one(events);
#endif

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
