#include "calendar_sync.h"
#include <algorithm>
#include <ArduinoLog.h>
#include <esp_heap_caps.h>
#include <HTTPClient.h>
#include <time.h>
#include "uICAL.h"
#include "buffer_reader.h"
#include "chunked_decoder.h"
#include "display.h"

// How long to wait for the next burst of response body before giving up on
// the download.
static const unsigned long HTTP_STREAM_TIMEOUT_MS = 10000;

static std::vector<CalendarEvent> collect_events(uICAL::Calendar_ptr &loaded_calendar, const String &calendar_name, int days);

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
  sprintf(buf, "In %ld days", diff_days);
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

// Blocking byte source over the HTTP socket.
//
// Reads in blocks rather than a byte at a time: every single-byte read on a
// TLS connection is a call into mbedTLS, and a feed of a few hundred KB makes
// that hundreds of thousands of them.
//
// Deciding when the body has actually ended is the delicate part. A
// WiFiClient returns nothing whenever its buffer happens to be empty, which
// says nothing about whether more is coming, and on a TLS socket available()
// can report zero while an undecrypted record is still pending. Treating
// either as the end truncates the body, so the end is only accepted once the
// connection is closed and a further read attempt also comes back empty.
class HttpByteSource : public ChunkedDecoder::Source
{
public:
  HttpByteSource(WiFiClient &client, unsigned long timeout_ms)
      : client(client), timeout_ms(timeout_ms) {}

  int readByte() override
  {
    if (buffer_pos >= buffer_len && !refill())
    {
      return -1;
    }
    return buffer[buffer_pos++];
  }

  size_t bytesRead() const { return total_bytes; }
  bool timedOut() const { return timed_out; }

private:
  static const size_t BUFFER_SIZE = 512;

  bool refill()
  {
    buffer_pos = 0;
    buffer_len = 0;

    unsigned long start = millis();
    while (true)
    {
      int ready = client.available();
      if (ready > 0)
      {
        size_t want = (size_t)ready < BUFFER_SIZE ? (size_t)ready : BUFFER_SIZE;
        int got = client.read(buffer, want);
        if (got > 0)
        {
          buffer_len = (size_t)got;
          total_bytes += (size_t)got;
          return true;
        }
      }

      if (!client.connected())
      {
        // Closed, but drain anything mbedTLS still has buffered before
        // accepting that as the end of the body.
        int got = client.read(buffer, BUFFER_SIZE);
        if (got > 0)
        {
          buffer_len = (size_t)got;
          total_bytes += (size_t)got;
          return true;
        }
        return false;
      }

      if (millis() - start > timeout_ms)
      {
        timed_out = true;
        return false;
      }
      delay(1);
    }
  }

  WiFiClient &client;
  unsigned long timeout_ms;
  uint8_t buffer[BUFFER_SIZE];
  size_t buffer_pos = 0;
  size_t buffer_len = 0;
  size_t total_bytes = 0;
  bool timed_out = false;
};

// Growable byte buffer held in PSRAM.
//
// The board carries 8MB of OPI PSRAM, so a few hundred KB of calendar costs
// nothing there, whereas the internal heap has only ~280KB total and is
// already carrying the WiFi and TLS buffers. Allocating explicitly rather
// than through String keeps the body out of internal RAM by construction
// instead of relying on the allocator's size heuristic.
class PsramBuffer
{
public:
  ~PsramBuffer()
  {
    if (data_)
    {
      heap_caps_free(data_);
    }
  }

  bool append(char ch)
  {
    if (length_ == capacity_ && !grow())
    {
      return false;
    }
    data_[length_++] = ch;
    return true;
  }

  const char *data() const { return data_; }
  size_t size() const { return length_; }

private:
  static const size_t INITIAL_CAPACITY = 64 * 1024;

  bool grow()
  {
    size_t wanted = capacity_ ? capacity_ * 2 : INITIAL_CAPACITY;
    char *moved = (char *)heap_caps_realloc(data_, wanted, MALLOC_CAP_SPIRAM);
    if (!moved)
    {
      // No PSRAM available (or not enabled): internal RAM is better than
      // failing outright, even though it may not fit.
      moved = (char *)heap_caps_realloc(data_, wanted, MALLOC_CAP_8BIT);
    }
    if (!moved)
    {
      return false;
    }
    data_ = moved;
    capacity_ = wanted;
    return true;
  }

  char *data_ = nullptr;
  size_t length_ = 0;
  size_t capacity_ = 0;
};

// Pull X-WR-CALNAME out of an already-buffered body.
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

std::vector<CalendarEvent> parse_ics(const String &ical_text, int days)
{
  std::vector<CalendarEvent> events;

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

  return collect_events(loaded_calendar, extract_calendar_name(ical_text), days);
}

// Walk a parsed calendar and pull out the events falling within the next
// `days` days, tagged with the calendar's display name.
static std::vector<CalendarEvent> collect_events(uICAL::Calendar_ptr &loaded_calendar, const String &calendar_name, int days)
{
  std::vector<CalendarEvent> events;

  time_t now = time(nullptr);
  int display_days_as_seconds = days * 86400;
  uICAL::DateTime range_begin(now);
  uICAL::DateTime range_end(now + display_days_as_seconds);

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
