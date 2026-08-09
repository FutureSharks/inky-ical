#include "roboto_font.h"

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
