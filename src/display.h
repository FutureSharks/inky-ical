#ifndef DISPLAY_H
#define DISPLAY_H

#include "calendar_sync.h"
#include <Arduino.h>
#include <vector>

/*
 * The display layer is split across three translation units:
 *   display.cpp          - framebuffer lifecycle and shared drawing primitives
 *   display_status.cpp   - splash/init screen, status lines, terminal screens
 *   display_calendar.cpp - the calendar screen and all event formatting
 */

/*
 * Initialize display hardware and show the init/splash screen. This screen
 * remains on the display until display_calendar() successfully renders, so
 * it stays visible through the whole startup sequence.
 *
 * Returns false if the framebuffer could not be allocated, in which case
 * nothing can be shown on the panel and every other call here is a no-op.
 */
bool display_init();

/*
 * Append a status line to the init screen, below the splash text (e.g.
 * "Connecting to WiFi...", "Connected, IP: 192.168.1.5"). Lines accumulate
 * until display_calendar() clears the screen.
 */
void display_init_status(const char* fmt, ...);

/*
 * Wipe the accumulated status/error lines from the init screen and reset the
 * cursor back to the first status line, leaving the splash text intact. Called
 * between startup retry attempts so lines don't run off the bottom of the panel.
 */
void display_init_status_reset();

/*
 * Replace the init screen with a summary of a permanently failed startup: the
 * number of attempts made and the error recorded for each one. Shown just
 * before the device powers down.
 *
 * Args:
 *   attempts: Number of attempts made
 *   failures: One error message per failed attempt
 */
void display_init_failure_summary(int attempts, const std::vector<String>& failures);

/*
 * Replace the init screen with a low battery message and instruction to
 * recharge. Shown just before the device powers down without syncing.
 */
void display_low_battery();

/*
 * Render calendar events to e-ink display. Clears the init screen. Events are
 * passed as data; all wording and layout is decided here.
 *
 * Args:
 *   events: Vector of CalendarEvent structs, sorted by start time
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
