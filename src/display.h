#ifndef DISPLAY_H
#define DISPLAY_H

#include "calendar_sync.h"
#include <vector>

/*
 * Initialize display hardware and show the init/splash screen. This screen
 * remains on the display until display_calendar() successfully renders, so
 * it stays visible through the whole startup sequence.
 */
void display_init();

/*
 * Append a status line to the init screen, below the splash text (e.g.
 * "Connecting to WiFi...", "Connected, IP: 192.168.1.5"). Lines accumulate
 * until display_calendar() clears the screen.
 */
void display_init_status(const char* fmt, ...);

/*
 * Render calendar events to e-ink display. Clears the init screen.
 *
 * Args:
 *   events: Vector of CalendarEvent structs
 */
void display_calendar(const std::vector<CalendarEvent>& events);

/*
 * Append an error line to the init screen (e.g. WiFi connection failed).
 * Does not clear prior init status lines, so errors are shown alongside
 * the initialization steps that already ran.
 *
 * Args:
 *   fmt: printf-style error message
 */
void display_error(const char* fmt, ...);

#endif
