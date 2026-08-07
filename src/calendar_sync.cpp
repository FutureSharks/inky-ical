#include "calendar_sync.h"
#include <algorithm>
#include <ArduinoLog.h>
#include <HTTPClient.h>
#include <time.h>
#include "uICAL.h"
#include "display.h"

// Format an event's start/end DateStamps as "09:00 - 11:00", or "All day"
// when the event has no time-of-day component.
static String format_event_time(const uICAL::DateStamp &start_ds, const uICAL::DateStamp &end_ds, bool has_time)
{
  if (!has_time)
  {
    return "All day";
  }
  char buf[16];
  sprintf(buf, "%02u:%02u - %02u:%02u", start_ds.hour, start_ds.minute, end_ds.hour, end_ds.minute);
  return String(buf);
}

// Format an event's start DateStamp relative to `now` as "today", "tomorrow"
// or "in N days", comparing local calendar dates rather than elapsed seconds.
static String format_relative_date(const uICAL::DateStamp &start_ds, time_t now)
{
  struct tm now_tm;
  localtime_r(&now, &now_tm);
  now_tm.tm_hour = 12;
  now_tm.tm_min = 0;
  now_tm.tm_sec = 0;
  time_t today_noon = mktime(&now_tm);

  struct tm event_tm = {0};
  event_tm.tm_year = start_ds.year - 1900;
  event_tm.tm_mon = start_ds.month - 1;
  event_tm.tm_mday = start_ds.day;
  event_tm.tm_hour = 12;
  time_t event_noon = mktime(&event_tm);

  long diff_days = (long)((event_noon - today_noon + 43200) / 86400);

  if (diff_days <= 0)
  {
    return "today";
  }
  if (diff_days == 1)
  {
    return "tomorrow";
  }
  char buf[24];
  sprintf(buf, "in %ld days", diff_days);
  return String(buf);
}

// Convert an event's start DateStamp to an epoch time_t, used as the sort
// key when merging events from multiple calendars chronologically.
static time_t compute_start_epoch(const uICAL::DateStamp &start_ds)
{
  struct tm start_tm = {0};
  start_tm.tm_year = start_ds.year - 1900;
  start_tm.tm_mon = start_ds.month - 1;
  start_tm.tm_mday = start_ds.day;
  start_tm.tm_hour = start_ds.hour;
  start_tm.tm_min = start_ds.minute;
  start_tm.tm_sec = start_ds.second;
  return mktime(&start_tm);
}

// Extract the X-WR-CALNAME property (calendar display name) from raw
// iCalendar text. Returns an empty string if the property is absent.
static String extract_calendar_name(const String &ical_text)
{
  const char *prop = "X-WR-CALNAME:";
  int start = ical_text.indexOf(prop);
  if (start < 0)
  {
    return String();
  }
  start += strlen(prop);
  int end = ical_text.indexOf('\n', start);
  if (end < 0)
  {
    end = ical_text.length();
  }
  String name = ical_text.substring(start, end);
  name.trim();
  return name;
}

// Sink that de-chunked response bytes get written to via HTTPClient::writeToStream(),
// which performs chunked-transfer-encoding decoding internally (unlike reading
// raw off getStreamPtr(), which exposes chunk-size/CRLF framing bytes).
class StatsSink : public Stream
{
public:
  String text;
  size_t total_bytes = 0;
  size_t line_count = 0;
  unsigned long sum = 0;

  size_t write(uint8_t b) override
  {
    return write(&b, 1);
  }

  size_t write(const uint8_t *buf, size_t len) override
  {
    for (size_t j = 0; j < len; j++)
    {
      sum += buf[j];
      if (buf[j] == '\n')
      {
        line_count++;
      }
    }
    total_bytes += len;
    text.concat(reinterpret_cast<const char *>(buf), len);
    return len;
  }

  // Read side is unused (writeToStream() only writes into this sink), but
  // Stream is abstract and requires these to be implemented.
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
};

String fetch_ics(const char *url)
{
  Log.notice("Fetching calendar from: %s" CR, url);
  display_init_status("Getting ICAL URL");

  HTTPClient http;
  http.begin(url);

  int http_code = http.GET();

  if (http_code != HTTP_CODE_OK)
  {
    Log.error("Calendar fetch error: %d" CR, http_code);
    display_error("Calendar fetch error: HTTP %d", http_code);
    http.end();
    return String();
  }

  StatsSink sink;
  int content_length = http.getSize();
  if (content_length > 0)
  {
    sink.text.reserve(content_length);
  }
  int written = http.writeToStream(&sink);
  http.end();

  if (written < 0)
  {
    Log.error("Calendar fetch writeToStream failed, error %d" CR, written);
    display_error("Calendar fetch writeToStream failed, error %d", written);
    return String();
  }

  Log.trace("Fetched %u bytes, %u lines" CR, (unsigned)sink.total_bytes, (unsigned)sink.line_count);
  display_init_status("Fetched %u bytes", (unsigned)sink.total_bytes);

  return sink.text;
}

std::vector<CalendarEvent> fetch_calendar(const char *url, int days)
{
  String ics_content = fetch_ics(url);
  if (ics_content.length() == 0)
  {
    Log.error("Calendar fetch failed, 0 bytes returned" CR);
    display_error("Calendar fetch failed, 0 bytes returned");
    return std::vector<CalendarEvent>();
  }

  return parse_ics(ics_content, days);
}

std::vector<CalendarEvent> fetch_calendars(const std::vector<String> &urls, int days, size_t max_events)
{
  std::vector<CalendarEvent> merged;

  for (size_t i = 0; i < urls.size(); i++)
  {
    // Each call fetches and parses one URL's worth of data; its HTTP response
    // buffer (StatsSink::text inside fetch_ics) is freed as soon as
    // fetch_calendar() returns, before the next URL is fetched.
    std::vector<CalendarEvent> calendar_events = fetch_calendar(urls[i].c_str(), days);
    merged.insert(merged.end(), calendar_events.begin(), calendar_events.end());
  }

  std::sort(merged.begin(), merged.end(), [](const CalendarEvent &a, const CalendarEvent &b) {
    return a.start_epoch < b.start_epoch;
  });

  if (merged.size() > max_events)
  {
    merged.resize(max_events);
  }

  return merged;
}

std::vector<CalendarEvent> parse_ics(const String &ical_text, int days)
{
  std::vector<CalendarEvent> events;

  String calendar_name = extract_calendar_name(ical_text);

  time_t now = time(nullptr);
  int display_days_as_seconds = days * 86400;
  uICAL::DateTime range_begin(now);
  uICAL::DateTime range_end(now + display_days_as_seconds);

  uICAL::Calendar_ptr loaded_calendar;
  try
  {
    uICAL::istream_String istm(ical_text);
    loaded_calendar = uICAL::Calendar::load(istm);
  }
  catch (uICAL::Error &ex)
  {
    Log.error("Calendar parse error: %s" CR, ex.message.c_str());
    display_error("Calendar parse error: %s", ex.message.c_str());
    return events;
  }

  try
  {
    uICAL::CalendarIter_ptr filtered_events_iterator = uICAL::new_ptr<uICAL::CalendarIter>(loaded_calendar, range_begin, range_end);

    while (filtered_events_iterator->next())
    {
      uICAL::CalendarEntry_ptr calendar_event = filtered_events_iterator->current();

      uICAL::DateStamp start_ds = calendar_event->start().datestamp();
      uICAL::DateStamp end_ds = calendar_event->end().datestamp();
      bool has_time = !(start_ds.hour == 0 && start_ds.minute == 0 && start_ds.second == 0 &&
                        end_ds.hour == 0 && end_ds.minute == 0 && end_ds.second == 0);

      CalendarEvent event;
      event.title = String(calendar_event->summary().c_str());
      event.time = format_event_time(start_ds, end_ds, has_time);
      event.calendar_name = calendar_name;
      event.date = format_relative_date(start_ds, now);
      event.start_epoch = compute_start_epoch(start_ds);
      events.push_back(event);

      if (events.size() >= (size_t)(days * 8))
      {
        break;
      }
    }
  }
  catch (uICAL::Error &ex)
  {
    Log.error("Calendar event filtering error: %s" CR, ex.message.c_str());
    display_error("Calendar event filtering error: %s", ex.message.c_str());
  }

  return events;
}
