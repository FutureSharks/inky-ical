#include "roboto_font.h"

#include <stddef.h>

#include "roboto8.h"
#include "roboto10.h"
#include "roboto12.h"
#include "roboto14.h"
#include "roboto16.h"
#include "roboto18.h"
#include "roboto20.h"
#include "roboto22.h"
#include "roboto24.h"
#include "roboto32.h"
#include "roboto36.h"
#include "roboto40.h"
#include "roboto48.h"
#include "roboto60.h"

typedef struct
{
    uint8_t size;
    const GFXfont *font;
} RobotoFontEntry;

static const RobotoFontEntry roboto_fonts[] = {
    { 8, &Roboto8 },
    { 10, &Roboto10 },
    { 12, &Roboto12 },
    { 14, &Roboto14 },
    { 16, &Roboto16 },
    { 18, &Roboto18 },
    { 20, &Roboto20 },
    { 22, &Roboto22 },
    { 24, &Roboto24 },
    { 32, &Roboto32 },
    { 36, &Roboto36 },
    { 40, &Roboto40 },
    { 48, &Roboto48 },
    { 60, &Roboto60 },
};

#define ROBOTO_FONT_COUNT (sizeof(roboto_fonts) / sizeof(roboto_fonts[0]))

const GFXfont *roboto_font(uint8_t size)
{
    const RobotoFontEntry *closest = &roboto_fonts[0];
    uint8_t closest_diff = size > closest->size ? size - closest->size : closest->size - size;

    for (uint32_t i = 1; i < ROBOTO_FONT_COUNT; i++)
    {
        const RobotoFontEntry *entry = &roboto_fonts[i];
        uint8_t diff = size > entry->size ? size - entry->size : entry->size - size;
        if (diff < closest_diff)
        {
            closest = entry;
            closest_diff = diff;
        }
    }

    return closest->font;
}

void write_roboto(const char *string,
                   int32_t *cursor_x,
                   int32_t *cursor_y,
                   uint8_t *framebuffer,
                   uint8_t size,
                   uint8_t brightness)
{
    FontProperties props = {
        .fg_color = brightness > 15 ? 15 : brightness,
        .bg_color = 15,
        .fallback_glyph = 0,
        .flags = 0,
    };

    write_mode(roboto_font(size), string, cursor_x, cursor_y, framebuffer, BLACK_ON_WHITE, &props);
}

int32_t measure_roboto(const char *string, uint8_t size)
{
    int32_t cursor_x = 0;
    int32_t cursor_y = 0;
    write_roboto(string, &cursor_x, &cursor_y, NULL, size, 0);
    return cursor_x;
}

int32_t write_roboto_right(const char *string,
                         int32_t right_edge_x,
                         int32_t baseline_y,
                         uint8_t *framebuffer,
                         uint8_t size,
                         uint8_t brightness)
{
    int32_t left_edge_x = right_edge_x - measure_roboto(string, size);
    int32_t cursor_x = left_edge_x;
    int32_t cursor_y = baseline_y;
    write_roboto(string, &cursor_x, &cursor_y, framebuffer, size, brightness);
    return left_edge_x;
}
