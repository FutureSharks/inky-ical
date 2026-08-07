#ifndef CALENDAR_SYNC_H
#define CALENDAR_SYNC_H

#include <Arduino.h>
#include <vector>
#include <time.h>

struct CalendarEvent {
  String title;
  String time;          // e.g. "09:00 - 11:00" or "All day"
  String calendar_name;  // ICAL X-WR-CALNAME
  String date;           // e.g. "today", "tomorrow" or "in 3 days"
  time_t start_epoch;    // sort key, used to merge events from multiple calendars chronologically
};

/*
 * Fetch calendar events from iCalendar URL
 *
 * Args:
 *   url: Full .ics feed URL
 *   days: Only return events within next N days
 *
 * Returns:
 *   std::vector of CalendarEvent structs
 */
std::vector<CalendarEvent> fetch_calendar(const char* url, int days);

/*
 * Fetch and merge calendar events from multiple iCalendar URLs, sorted
 * chronologically and capped at max_events. Each URL is fetched and parsed
 * one at a time, freeing that URL's response buffer before moving on to
 * the next, to keep peak memory usage bounded.
 *
 * Args:
 *   urls: List of .ics feed URLs
 *   days: Only return events within next N days
 *   max_events: Maximum number of events to return across all calendars
 *
 * Returns:
 *   std::vector of CalendarEvent structs, sorted by start time
 */
std::vector<CalendarEvent> fetch_calendars(const std::vector<String>& urls, int days, size_t max_events);

/*
 * Parse iCalendar format (.ics) content
 *
 * Args:
 *   ics_text: Raw iCalendar data
 *   days: Only return events within next N days
 *
 * Returns:
 *   std::vector of CalendarEvent structs
 */
std::vector<CalendarEvent> parse_ics(const String& ics_text, int days);

#endif
