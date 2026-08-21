#ifndef CALENDAR_SYNC_H
#define CALENDAR_SYNC_H

#include <Arduino.h>
#include <vector>
#include <time.h>

/*
 * One calendar event, as data rather than as display text: formatting for the
 * panel is entirely the display layer's job (see src/display_calendar.cpp).
 * Keeping it that way means a layout change never reaches back into the fetch
 * and parse code.
 */
struct CalendarEvent {
  String title;
  String calendar_name; // iCalendar X-WR-CALNAME, empty if the feed omits it
  time_t start_epoch;   // also the sort key when merging multiple calendars
  time_t end_epoch;
  bool all_day;         // event has no time-of-day component
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

#endif
