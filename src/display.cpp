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

// What is currently ink on the panel, so the erase can drive the negative of
// it (see erase_panel). NULL until the panel content is known, which is the
// case at boot: the image left by the previous wake cycle is still on screen
// and nothing in RAM survived deep sleep to describe it.
static uint8_t *panel_frame = NULL;
static bool panel_frame_known = false;

static void panel_frame_record(const uint8_t *drawn, bool replaces);

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
  panel_frame_record(framebuffer, false);
}

// Erase parameters. epd_clear() is already a flash erase - four dark pushes
// then four white ones per cycle - so simply running more cycles does not
// shift a ghost. What it cannot do is treat pixels differently: every pixel
// gets the same drive, so ink that set deep (the splash text, latched for the
// whole sync) keeps its bias relative to the paper around it and stays
// faintly visible. Driving the negative of what is on the panel first puts
// the opposite drive exactly where the ghost is, cancelling it, and the clear
// cycles then take the whole panel back to white. This is what an e-reader is
// doing when it flashes an inverted page between screens.
static const int32_t ERASE_CYCLES = 4;
static const int32_t ERASE_CYCLE_TIME = 50;

// Cycles used when the panel content is unknown and no negative can be drawn,
// so the erase has nothing to work with but brute force.
static const int32_t ERASE_CYCLES_BLIND = 6;
static const int16_t ERASE_SOAK_TIME = 100;

// Remember what the panel now shows. A grayscale draw only ever adds ink, so
// a pixel ends up at the darker of what was there and what was drawn.
static void panel_frame_record(const uint8_t *drawn, bool replaces)
{
  if (!panel_frame)
  {
    return;
  }

  if (replaces || !panel_frame_known)
  {
    memcpy(panel_frame, drawn, FRAMEBUFFER_BYTES);
  }
  else
  {
    // Two 4-bit pixels per byte, each taken to the darker (lower) value.
    for (size_t i = 0; i < FRAMEBUFFER_BYTES; i++)
    {
      const uint8_t on_hi = panel_frame[i] >> 4, new_hi = drawn[i] >> 4;
      const uint8_t on_lo = panel_frame[i] & 0x0F, new_lo = drawn[i] & 0x0F;
      const uint8_t hi = new_hi < on_hi ? new_hi : on_hi;
      const uint8_t lo = new_lo < on_lo ? new_lo : on_lo;
      panel_frame[i] = (uint8_t)((hi << 4) | lo);
    }
  }

  panel_frame_known = true;
}

// Take the panel to white, cancelling the current image on the way. Assumes
// the panel is already powered on.
static void erase_panel()
{
  if (!panel_frame_known || !panel_frame)
  {
    // Nothing known to invert - flash the whole panel dark and clear harder.
    epd_push_pixels(epd_full_screen(), ERASE_SOAK_TIME, 0);
    epd_clear_area_cycles(epd_full_screen(), ERASE_CYCLES_BLIND, ERASE_CYCLE_TIME);
    return;
  }

  // Invert in place: a bitwise NOT maps each 4-bit pixel v to 15 - v, so this
  // is the exact negative. A grayscale draw only darkens, which is the half
  // that matters - it lays ink over everything the last image left white.
  for (size_t i = 0; i < FRAMEBUFFER_BYTES; i++)
  {
    panel_frame[i] = (uint8_t)~panel_frame[i];
  }
  epd_draw_grayscale_image(epd_full_screen(), panel_frame);

  epd_clear_area_cycles(epd_full_screen(), ERASE_CYCLES, ERASE_CYCLE_TIME);
  panel_frame_known = false;
}

void display_repaint()
{
  // Writing white pixels does not erase ink already on the panel, so anything
  // that replaces (rather than adds to) what is on screen has to erase the
  // panel first, otherwise the new screen ghosts over the old one.
  epd_poweron();
  erase_panel();
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();
  panel_frame_record(framebuffer, true);
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

  // Tracks what is on the panel so the erase can invert it. Not fatal if it
  // fails: erase_panel() falls back to a blind flash erase without it.
  panel_frame = (uint8_t *)ps_calloc(sizeof(uint8_t), FRAMEBUFFER_BYTES);
  if (!panel_frame)
  {
    Log.warning("Panel-content buffer allocation failed; erases will ghost more" CR);
  }
  display_clear_framebuffer();

  epd_init();
  epd_poweron();
  erase_panel();
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
