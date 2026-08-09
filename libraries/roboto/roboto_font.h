#pragma once
#include "epd_driver.h"
#include <stdint.h>

/**
 * Available pre-rendered Roboto sizes (points at 150dpi).
 * Generated via scripts/fontconvert.py, see src/roboto<size>.h.
 */
#define ROBOTO_MIN_SIZE 12
#define ROBOTO_MAX_SIZE 60

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the pre-rendered Roboto GFXfont closest to the requested size.
 *
 * @param size Desired font size in points. Snapped to the nearest
 *             available pre-rendered size
 *             (12/14/16/18/20/22/24/32/36/40/48/60).
 */
const GFXfont *roboto_font(uint8_t size);

/**
 * @brief Draw a Roboto string at the given size and brightness.
 *
 * @param string     Text to draw (UTF-8), may include Latin-1 and
 *                    Latin Extended-A characters (e.g. ö, é, ł, š).
 * @param cursor_x   Pointer to the x cursor, advanced past the drawn text.
 * @param cursor_y   Pointer to the y cursor (baseline).
 * @param framebuffer Target framebuffer, or NULL to draw to a temporary buffer.
 * @param size       Font size in points, snapped to the nearest available size.
 * @param brightness Text brightness/grayscale value, 0 (black) - 15 (white).
 */
void write_roboto(const char *string,
                   int32_t *cursor_x,
                   int32_t *cursor_y,
                   uint8_t *framebuffer,
                   uint8_t size,
                   uint8_t brightness);

#ifdef __cplusplus
}
#endif
