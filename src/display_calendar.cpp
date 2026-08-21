/*
 * The calendar screen: the date header and the event rows, plus all the
 * formatting of event data into display text. Nothing upstream of here knows
 * how an event is worded — calendar_sync.cpp hands over epochs and a flag.
 */

#include "display.h"
#include "display_internal.h"
#include "config.h"
#include <ArduinoLog.h>
#include <epd_driver.h>
#include <roboto_font.h>
#include <time.h>

using namespace layout;

// Header: the weekday on its own line in large type with the day/month
// beneath it, and the battery icon on the right.
static void draw_header()
{
  struct tm timeinfo;
  char weekday[24] = "Unknown";
  char daymonth[24] = "";
  if (getLocalTime(&timeinfo))
  {
    strftime(weekday, sizeof(weekday), "%A", &timeinfo);
    strftime(daymonth, sizeof(daymonth), "%e %B", &timeinfo);
  }

  int32_t x = MARGIN;
  int32_t y = HEADER_WEEKDAY_Y;
  write_roboto(weekday, &x, &y, framebuffer, SIZE_HEADING, BLACK);

  // %e pads single-digit days with a space; skip it so the two header
  // lines stay left-aligned with each other.
  const char *daymonth_text = daymonth[0] == ' ' ? daymonth + 1 : daymonth;
  x = MARGIN;
  y = HEADER_DAYMONTH_Y;
  write_roboto(daymonth_text, &x, &y, framebuffer, SIZE_BODY, GREY);

  display_draw_battery_icon();

  epd_draw_line(0, HEADER_RULE_Y, EPD_WIDTH, HEADER_RULE_Y, 1, framebuffer);
}

// How far an event's start date is from today, in whole local calendar days,
// comparing dates rather than elapsed seconds so an event this evening still
// reads as "Today".
static long days_from_today(time_t start_epoch, time_t now)
{
  struct tm now_tm;
  localtime_r(&now, &now_tm);
  now_tm.tm_hour = 12;
  now_tm.tm_min = 0;
  now_tm.tm_sec = 0;
  now_tm.tm_isdst = -1;
  time_t today_noon = mktime(&now_tm);

  struct tm event_tm;
  localtime_r(&start_epoch, &event_tm);
  event_tm.tm_hour = 12;
  event_tm.tm_min = 0;
  event_tm.tm_sec = 0;
  event_tm.tm_isdst = -1;
  time_t event_noon = mktime(&event_tm);

  return (long)((event_noon - today_noon + 43200) / 86400);
}

// "Today", "Tomorrow" or "In 4 days".
static String format_relative_date(time_t start_epoch, time_t now)
{
  long diff_days = days_from_today(start_epoch, now);

  if (diff_days <= 0)
    return "Today";
  if (diff_days == 1)
    return "Tomorrow";

  char buf[24];
  snprintf(buf, sizeof(buf), "In %ld days", diff_days);
  return String(buf);
}

// "at 09:00", or "All day" for an event with no time-of-day component.
static String format_event_time(const CalendarEvent &event)
{
  if (event.all_day)
    return "All day";

  struct tm start_tm;
  localtime_r(&event.start_epoch, &start_tm);

  char buf[16];
  strftime(buf, sizeof(buf), "at %H:%M", &start_tm);
  return String(buf);
}

// The event title, suffixed with the calendar name when configured, and
// truncated so it cannot run into the date column.
static String format_title(const CalendarEvent &event)
{
  String title = event.title;
  if (DISPLAY_CALENDAR_NAME && event.calendar_name.length())
    title = title + " (" + event.calendar_name + ")";

  if (title.length() > MAX_TITLE_LENGTH)
    title = title.substring(0, MAX_TITLE_LENGTH - 3) + "...";

  return title;
}

void display_draw_calendar(const std::vector<CalendarEvent> &events)
{
  draw_header();

  // Three columns: title flush left, relative date right-aligned at
  // ROW_DATE_RIGHT_X and the time right-aligned at the right margin, with a
  // hairline rule under each row so the eye can track across the gap between
  // title and time.
  const time_t now = time(nullptr);
  int32_t row_y = HEADER_RULE_Y;

  for (size_t i = 0; i < events.size() && i < (size_t)MAX_EVENTS_TO_DISPLAY; i++)
  {
    const CalendarEvent &event = events[i];

    if (row_y + ROW_HEIGHT > EPD_HEIGHT)
      break;

    const int32_t baseline_y = row_y + ROW_BASELINE_OFFSET;

    String title = format_title(event);
    String date_text = format_relative_date(event.start_epoch, now);
    String time_text = format_event_time(event);

    int32_t x = MARGIN;
    int32_t y = baseline_y;

    write_roboto(title.c_str(), &x, &y, framebuffer, SIZE_EVENT, BLACK);
    write_roboto_right(date_text.c_str(), ROW_DATE_RIGHT_X, baseline_y, framebuffer,
                       SIZE_EVENT_META, BLACK);
    write_roboto_right(time_text.c_str(), EPD_WIDTH - MARGIN, baseline_y, framebuffer,
                       SIZE_EVENT_META, GREY);

    row_y += ROW_HEIGHT;

    // Hairline separator below the row, inset from both margins, drawn only
    // when another row follows it.
    if (row_y + ROW_HEIGHT <= EPD_HEIGHT)
      epd_draw_line(MARGIN, row_y, EPD_WIDTH - MARGIN, row_y, RULE_HAIRLINE, framebuffer);

    Log.trace("%s %s %s" CR, date_text.c_str(), time_text.c_str(), title.c_str());
  }
}
