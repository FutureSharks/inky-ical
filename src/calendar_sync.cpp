#include "calendar_sync.h"
#include "config.h"
#include <algorithm>
#include <ArduinoLog.h>
#include <HTTPClient.h>
#include <time.h>
#include "uICAL.h"
#include "buffer_reader.h"
#include "chunked_decoder.h"
#include "http_byte_source.h"
#include "psram_buffer.h"
#include "display.h"

// How long to wait for the next burst of response body before giving up on
// the download.
static const unsigned long HTTP_STREAM_TIMEOUT_MS = 10000;

// Convert a uICAL DateStamp (already in local time) to an epoch time_t.
static time_t to_epoch(const uICAL::DateStamp &ds)
{
  struct tm tm_value = {0};
  tm_value.tm_year = ds.year - 1900;
  tm_value.tm_mon = ds.month - 1;
  tm_value.tm_mday = ds.day;
  tm_value.tm_hour = ds.hour;
  tm_value.tm_min = ds.minute;
  tm_value.tm_sec = ds.second;
  tm_value.tm_isdst = -1;
  return mktime(&tm_value);
}

// Pull the X-WR-CALNAME property (calendar display name) out of a buffered
// iCalendar body. Returns an empty string if the property is absent.
static String extract_calendar_name(const char *data, size_t length)
{
  static const char *PROP = "X-WR-CALNAME:";
  const size_t prop_len = strlen(PROP);

  for (size_t i = 0; i + prop_len <= length; i++)
  {
    if (memcmp(data + i, PROP, prop_len) != 0)
    {
      continue;
    }
    size_t start = i + prop_len;
    size_t end = start;
    while (end < length && data[end] != '\r' && data[end] != '\n')
    {
      end++;
    }
    String name;
    name.concat(data + start, end - start);
    name.trim();
    return name;
  }
  return String();
}

// Walk a parsed calendar and pull out the events falling within the next
// `days` days, tagged with the calendar's display name.
static std::vector<CalendarEvent> collect_events(uICAL::Calendar_ptr &loaded_calendar,
                                                 const String &calendar_name, int days)
{
  std::vector<CalendarEvent> events;

  time_t now = time(nullptr);
  uICAL::DateTime range_begin(now);
  uICAL::DateTime range_end(now + (time_t)days * 86400);

  try
  {
    uICAL::CalendarIter_ptr filtered_events_iterator =
        uICAL::new_ptr<uICAL::CalendarIter>(loaded_calendar, range_begin, range_end);

    while (filtered_events_iterator->next())
    {
      uICAL::CalendarEntry_ptr calendar_event = filtered_events_iterator->current();

      uICAL::DateStamp start_ds = calendar_event->start().datestamp();
      uICAL::DateStamp end_ds = calendar_event->end().datestamp();

      CalendarEvent event;
      event.title = String(calendar_event->summary().c_str());
      event.calendar_name = calendar_name;
      event.start_epoch = to_epoch(start_ds);
      event.end_epoch = to_epoch(end_ds);
      // Midnight-to-midnight means the feed gave dates without times.
      event.all_day = start_ds.hour == 0 && start_ds.minute == 0 && start_ds.second == 0 &&
                      end_ds.hour == 0 && end_ds.minute == 0 && end_ds.second == 0;
      events.push_back(event);

      // Bound the work a single pathological feed can cause. Well above
      // MAX_EVENTS_TO_DISPLAY, since events are merged and sorted across all
      // calendars before being trimmed to what fits on the panel.
      if (events.size() >= (size_t)MAX_EVENTS_PER_CALENDAR)
      {
        Log.warning("Stopped collecting at %d events from this calendar" CR,
                    (int)MAX_EVENTS_PER_CALENDAR);
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

std::vector<CalendarEvent> fetch_calendar(const char *url, int days)
{
  Log.notice("Fetching calendar from: %s" CR, url);

  std::vector<CalendarEvent> events;

  HTTPClient http;
  http.begin(url);

  // getStreamPtr() exposes the socket with the transfer framing intact, so we
  // need to know whether the body is chunked in order to strip it.
  // collectHeaders() takes a non-const array.
  static const char *HEADERS[] = {"Transfer-Encoding"};
  http.collectHeaders(HEADERS, 1);

  int http_code = http.GET();

  if (http_code != HTTP_CODE_OK)
  {
    Log.error("Calendar fetch error: %d" CR, http_code);
    display_error("Calendar fetch error: HTTP %d", http_code);
    http.end();
    return events;
  }

  display_init_status("Downloading calendar");

  bool is_chunked = http.header("Transfer-Encoding").equalsIgnoreCase("chunked");
  Log.trace("Response transfer encoding: %s, content length %d" CR,
            is_chunked ? "chunked" : "identity", http.getSize());

  // Drain the socket without pausing to parse. Parsing inline is slower than
  // the server sends, which lets the TCP receive window fill; the transfer
  // then stalls mid-body with the connection still open.
  HttpByteSource byte_source(*http.getStreamPtr(), HTTP_STREAM_TIMEOUT_MS);
  ChunkedDecoder decoder(byte_source, is_chunked);

  PsramBuffer body;
  bool out_of_memory = false;
  unsigned long download_start = millis();

  for (int ch = decoder.read(); ch >= 0; ch = decoder.read())
  {
    if (!body.append((char)ch))
    {
      out_of_memory = true;
      break;
    }
  }

  unsigned long download_ms = millis() - download_start;
  http.end();

  Log.trace("Downloaded %u body bytes from %u socket bytes in %u ms" CR,
            (unsigned)body.size(), (unsigned)byte_source.bytesRead(), (unsigned)download_ms);

  if (out_of_memory)
  {
    Log.error("Calendar buffer allocation failed after %u bytes" CR, (unsigned)body.size());
    display_error("Out of memory buffering calendar (%u bytes)", (unsigned)body.size());
    return events;
  }

  // Check the framing before parsing: a body cut short can still parse if it
  // happened to stop on a record boundary, and would then silently lose
  // events rather than report a problem.
  if (decoder.failed())
  {
    Log.error("Calendar body incomplete after %u bytes; "
              "socket timeout %T, truncated %T, malformed %T" CR,
              (unsigned)body.size(), byte_source.timedOut(),
              decoder.wasTruncated(), decoder.wasMalformed());
    display_error("Calendar download incomplete (%u bytes)", (unsigned)body.size());
    return events;
  }

  display_init_status("Parsing calendar");

  uICAL::Calendar_ptr loaded_calendar;
  try
  {
    BufferReader reader(body.data(), body.size());
    loaded_calendar = uICAL::Calendar::load(reader);
  }
  catch (uICAL::Error &ex)
  {
    Log.error("Calendar parse error: %s (after %u bytes)" CR,
              ex.message.c_str(), (unsigned)body.size());
    display_error("Calendar parse error: %s", ex.message.c_str());
    return events;
  }

  return collect_events(loaded_calendar, extract_calendar_name(body.data(), body.size()), days);
}

std::vector<CalendarEvent> fetch_calendars(const std::vector<String> &urls, int days, size_t max_events)
{
  std::vector<CalendarEvent> merged;

  for (size_t i = 0; i < urls.size(); i++)
  {
    // Each call fetches and parses one URL's worth of data; its HTTP response
    // buffer is freed as soon as fetch_calendar() returns, before the next
    // URL is fetched.
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
