/*
 * Framebuffer lifecycle and the drawing primitives shared by every screen.
 * The screens themselves live in display_status.cpp (splash, status lines,
 * low battery, startup failure) and display_calendar.cpp.
 */

#include "display.h"
#include "display_internal.h"
#include "battery.h"
#include <ArduinoLog.h>
#include <epd_driver.h>
#include "utilities.h"
#include <roboto_font.h>

using namespace layout;

uint8_t *framebuffer = NULL;

// 4 bits per pixel, so two pixels per byte.
static const size_t FRAMEBUFFER_BYTES = EPD_WIDTH * EPD_HEIGHT / 2;

bool display_ready()
{
  return framebuffer != NULL;
}

void display_clear_framebuffer()
{
  memset(framebuffer, 0xFF, FRAMEBUFFER_BYTES);
}

void display_flush()
{
  epd_poweron();
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();
}

void display_repaint()
{
  // Writing white pixels does not erase ink already on the panel, so anything
  // that replaces (rather than adds to) what is on screen has to run a full
  // epd_clear() first, otherwise the new screen ghosts over the old one.
  epd_poweron();
  epd_clear();
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();
}

// Allocate the framebuffer and bring up the panel. Returns false if the
// framebuffer could not be allocated, in which case nothing can be drawn.
bool display_hardware_init()
{
  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), FRAMEBUFFER_BYTES);
  if (!framebuffer)
  {
    Log.fatal("Framebuffer allocation of %u bytes failed" CR, (unsigned)FRAMEBUFFER_BYTES);
    return false;
  }
  display_clear_framebuffer();

  epd_init();
  epd_poweron();
  epd_clear();
  epd_poweroff();
  return true;
}

void display_draw_battery_icon()
{
  const int right_edge_x = EPD_WIDTH - MARGIN;
  const int y = 38;
  const int percentage = battery_get_percentage();

  char text[8];
  snprintf(text, sizeof(text), "%d%%", percentage);
  int32_t text_left_x = write_roboto_right(text, right_edge_x, y, framebuffer, SIZE_BATTERY, BLACK);

  const int32_t body_w = 40;
  const int32_t body_h = 20;
  const int32_t tip_w = 4;
  const int32_t tip_h = 8;
  const int32_t gap = 10;

  int32_t body_x = text_left_x - gap - tip_w - body_w;
  int32_t body_y = y - body_h + 2;

  epd_draw_rect(body_x, body_y, body_w, body_h, BLACK, framebuffer);
  epd_fill_rect(body_x + body_w, body_y + (body_h - tip_h) / 2, tip_w, tip_h, BLACK, framebuffer);

  // Inner fill, inset by the 1px outline plus 2px of padding
  const int32_t fill_max_w = body_w - 6;
  int32_t fill_w = (fill_max_w * percentage) / 100;
  if (fill_w > 0)
    epd_fill_rect(body_x + 3, body_y + 3, fill_w, body_h - 6, BLACK, framebuffer);
}

void display_message_screen(const char *title,
                            const std::vector<String> &body_lines,
                            const char *footer,
                            uint8_t body_size)
{
  display_clear_framebuffer();

  int32_t x = MESSAGE_LEFT;
  int32_t y = MESSAGE_TITLE_Y;
  write_roboto(title, &x, &y, framebuffer, SIZE_TITLE, BLACK);

  display_draw_battery_icon();

  int32_t rule_y = MESSAGE_TITLE_Y + MESSAGE_RULE_INSET;
  epd_draw_line(MESSAGE_RULE_INSET, rule_y, EPD_WIDTH - MESSAGE_RULE_INSET, rule_y,
                RULE_HEAVY, framebuffer);

  const int32_t footer_y = EPD_HEIGHT - MESSAGE_FOOTER_UP;
  y = MESSAGE_BODY_Y;
  for (size_t i = 0; i < body_lines.size(); i++)
  {
    // Stop before running into the footer at the bottom of the panel
    if (y + MESSAGE_LINE_HEIGHT > footer_y)
    {
      Log.warning("Dropped %u message lines that did not fit" CR,
                  (unsigned)(body_lines.size() - i));
      break;
    }
    x = MESSAGE_LEFT;
    write_roboto(body_lines[i].c_str(), &x, &y, framebuffer, body_size, BLACK);
    y += MESSAGE_LINE_HEIGHT;
  }

  if (footer)
  {
    x = MESSAGE_LEFT;
    y = footer_y;
    write_roboto(footer, &x, &y, framebuffer, SIZE_SMALL, BLACK);
  }

  display_repaint();
}

void display_calendar(const std::vector<CalendarEvent> &events)
{
  Log.notice("Rendering calendar events to display..." CR);

  if (!display_ready())
  {
    return;
  }

  if (events.empty())
  {
    Log.warning("No events to display" CR);
    display_error("No Events");
    return;
  }

  display_clear_framebuffer();
  display_draw_calendar(events);
  display_repaint();

  Log.notice("Display updated successfully" CR);
}
