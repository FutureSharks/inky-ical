#ifndef DISPLAY_INTERNAL_H
#define DISPLAY_INTERNAL_H

/*
 * Shared internals of the display layer, for display.cpp,
 * display_status.cpp and display_calendar.cpp only. Not part of the
 * public interface in display.h.
 */

#include "calendar_sync.h"
#include <Arduino.h>
#include <vector>
#include <epd_driver.h>

// ===== Layout =====
// Everything positional lives here so the panel layout can be adjusted
// without hunting for numbers in the drawing code.
namespace layout
{
  // Margin kept clear on every edge of the panel.
  const int32_t MARGIN = 20;

  // Left edge of text on the full-screen message screens (splash, low
  // battery, startup failure), which are indented further than the calendar.
  const int32_t MESSAGE_LEFT = 100;

  // Full-screen message screens: title baseline, the rule beneath it, the
  // first body line, and the footer's distance up from the bottom edge.
  const int32_t MESSAGE_TITLE_Y = 120;
  const int32_t MESSAGE_RULE_INSET = 40;
  const int32_t MESSAGE_BODY_Y = 210;
  const int32_t MESSAGE_LINE_HEIGHT = 32;
  const int32_t MESSAGE_FOOTER_UP = 60;

  // Init status lines appended below the splash text.
  const int32_t STATUS_LINE_HEIGHT = 28;

  // Calendar header: weekday baseline, day/month baseline, and the rule that
  // separates the header from the event rows.
  const int32_t HEADER_WEEKDAY_Y = 52;
  const int32_t HEADER_DAYMONTH_Y = 88;
  const int32_t HEADER_RULE_Y = 100;

  // Event rows: pitch, the text baseline within a row, and the right edge the
  // relative-date column is aligned to.
  const int32_t ROW_HEIGHT = 61;
  const int32_t ROW_BASELINE_OFFSET = 46;
  const int32_t ROW_DATE_RIGHT_X = EPD_WIDTH - 260;

  // Longest event title drawn before it is truncated with an ellipsis, so it
  // cannot run into the date column.
  const int MAX_TITLE_LENGTH = 27;

  // ===== Type sizes (points) and greys (0 black - 15 white) =====
  const uint8_t SIZE_SPLASH = 48;
  const uint8_t SIZE_TITLE = 36;
  const uint8_t SIZE_EVENT = 22;
  const uint8_t SIZE_HEADING = 24;
  const uint8_t SIZE_EVENT_META = 18;
  const uint8_t SIZE_BODY = 16;
  const uint8_t SIZE_SMALL = 12;
  const uint8_t SIZE_BATTERY = 10;

  const uint8_t BLACK = 0;
  const uint8_t GREY = 7;
  const uint8_t RULE_HEAVY = 8;
  const uint8_t RULE_HAIRLINE = 200;
} // namespace layout

// The single full-panel 4bpp framebuffer, allocated once in display_init()
// and reused by every screen. NULL until then.
extern uint8_t *framebuffer;

// True once the framebuffer exists, i.e. drawing is safe.
bool display_ready();

// Paint the whole framebuffer white, without touching the panel.
void display_clear_framebuffer();

// Push the framebuffer to the panel, powering the display for just that time.
// Only adds ink: pixels that are white in the framebuffer leave whatever is
// already on the panel untouched, so this is for adding to the current screen.
void display_flush();

// Erase the panel, then push the framebuffer. Use whenever the framebuffer
// replaces what is on screen rather than adding to it. The erase is a dark
// soak followed by several clear cycles, so it takes a few seconds; that is
// what keeps deeply set ink (the splash) from ghosting through.
void display_repaint();

/*
 * Draw one of the full-screen message screens: a large title, a rule, some
 * body lines and a footer, with the battery percentage top right. Shared so
 * the low-battery and startup-failure screens cannot drift apart visually.
 * Body lines beyond what fits above the footer are dropped. body_size sets
 * the type size of the body lines, so a screen with long lines can set them
 * small enough to fit the panel width.
 */
void display_message_screen(const char *title,
                            const std::vector<String> &body_lines,
                            const char *footer,
                            uint8_t body_size);

/*
 * A battery icon with its charge level filled in, followed by
 * "<percentage>%", as a group whose right edge sits at right_edge_x.
 * y is the text baseline; the icon is centred on the text height.
 */
void display_draw_battery_icon();

// Append one line to the init screen's status area (display_status.cpp).
void display_status_line(const char *text);

// Draw the header and event rows of the calendar screen (display_calendar.cpp).
void display_draw_calendar(const std::vector<CalendarEvent> &events);

#endif
