#include "okigraph1-raster.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* 1/288 inch gives exact integer positions for 1/72 pin pitch and 15/144 feed. */
#define Y_UNITS_PER_INCH       288u
#define BAND_ORIGIN_Y_UNITS     30u  /* 15/144 inch */
#define PIN_PITCH_Y_UNITS        4u  /* 1/72 inch */

static int
pixel_is_black(const okg1_bitmap *bitmap, unsigned int x, unsigned int y)
{
    const unsigned char *row =
        bitmap->data + (size_t)y * bitmap->row_bytes;
    unsigned int byte_index = x >> 3;
    unsigned int bit_index = x & 7u;

    return (row[byte_index] &
            (unsigned char)(0x80u >> bit_index)) != 0;
}

static uint64_t
ceil_div_u64(uint64_t n, uint64_t d)
{
    return (n + d - 1u) / d;
}

static unsigned int
target_center_y_units(unsigned int dot_row)
{
    unsigned int band = dot_row / OKG1_PINS_PER_BAND;
    unsigned int pin = dot_row % OKG1_PINS_PER_BAND;

    return band * BAND_ORIGIN_Y_UNITS +
           pin * PIN_PITCH_Y_UNITS;
}

static unsigned int
count_target_dot_rows(const okg1_bitmap *bitmap)
{
    unsigned int row = 0;
    uint64_t page_height_scaled =
        (uint64_t)bitmap->height * Y_UNITS_PER_INCH;

    for (;;) {
        uint64_t center_scaled =
            (uint64_t)target_center_y_units(row) *
            bitmap->dpi_y;

        if (center_scaled >= page_height_scaled)
            break;

        ++row;
    }

    return row;
}

static unsigned int
source_center_boundary(unsigned int boundary_units,
                       unsigned int units_per_inch,
                       unsigned int source_dpi,
                       unsigned int source_limit)
{
    uint64_t twice_position =
        2u * (uint64_t)boundary_units * source_dpi;
    uint64_t numerator;
    uint64_t denominator =
        2u * (uint64_t)units_per_inch;
    uint64_t index;

    /*
     * Count source pixel centers strictly below the physical boundary:
     * ceil(boundary * dpi / units_per_inch - 0.5).
     */
    if (twice_position <= units_per_inch)
        return 0u;

    numerator = twice_position - units_per_inch;
    index = ceil_div_u64(numerator, denominator);
    if (index > source_limit)
        index = source_limit;

    return (unsigned int)index;
}

static void
source_x_range(const okg1_bitmap *bitmap,
               unsigned int out_x,
               unsigned int *x0,
               unsigned int *x1)
{
    /* Native X centers are j/60 inch; use 1/120-inch midpoint units. */
    unsigned int low =
        out_x == 0u ? 0u : 2u * out_x - 1u;
    unsigned int high = 2u * out_x + 1u;

    *x0 = source_center_boundary(
        low, 120u, bitmap->dpi_x, bitmap->width);
    *x1 = source_center_boundary(
        high, 120u, bitmap->dpi_x, bitmap->width);

    if (*x1 <= *x0 && *x0 < bitmap->width)
        *x1 = *x0 + 1u;
}

static void
source_y_range(const okg1_bitmap *bitmap,
               unsigned int dot_row,
               unsigned int target_rows,
               unsigned int *y0,
               unsigned int *y1)
{
    unsigned int center =
        target_center_y_units(dot_row);
    unsigned int low;
    unsigned int high;

    if (dot_row == 0u) {
        low = 0u;
    } else {
        unsigned int prev =
            target_center_y_units(dot_row - 1u);
        low = (prev + center) / 2u;
    }

    if (dot_row + 1u < target_rows) {
        unsigned int next =
            target_center_y_units(dot_row + 1u);
        high = (center + next) / 2u;
    } else {
        uint64_t page_units = ceil_div_u64(
            (uint64_t)bitmap->height *
            Y_UNITS_PER_INCH,
            bitmap->dpi_y);

        high = page_units > (uint64_t)center
             ? (unsigned int)page_units
             : center + 1u;
    }

    *y0 = source_center_boundary(
        low, Y_UNITS_PER_INCH,
        bitmap->dpi_y, bitmap->height);
    *y1 = source_center_boundary(
        high, Y_UNITS_PER_INCH,
        bitmap->dpi_y, bitmap->height);

    if (*y1 <= *y0 && *y0 < bitmap->height)
        *y1 = *y0 + 1u;
}

static int
box_is_black(const okg1_bitmap *bitmap,
             unsigned int x0,
             unsigned int x1,
             unsigned int y0,
             unsigned int y1,
             unsigned int threshold_percent)
{
    uint64_t black = 0;
    uint64_t total;
    unsigned int x;
    unsigned int y;

    if (x1 <= x0 || y1 <= y0)
        return 0;

    total = (uint64_t)(x1 - x0) *
            (uint64_t)(y1 - y0);

    for (y = y0; y < y1; ++y) {
        for (x = x0; x < x1; ++x) {
            if (pixel_is_black(bitmap, x, y))
                ++black;
        }
    }

    return black * 100u >=
           total * threshold_percent;
}

int
okg1_render_bitmap(okg1_stream *stream,
                   const okg1_bitmap *bitmap,
                   const okg1_raster_options *options,
                   okg1_raster_stats *stats)
{
    unsigned int requested_columns;
    unsigned int output_columns;
    unsigned int target_rows;
    unsigned int output_bands;
    unsigned int band;
    unsigned int x;
    unsigned int threshold;
    unsigned char *columns;

    if (stream == NULL ||
        bitmap == NULL ||
        options == NULL ||
        bitmap->data == NULL ||
        bitmap->width == 0u ||
        bitmap->height == 0u ||
        bitmap->dpi_x == 0u ||
        bitmap->dpi_y == 0u ||
        options->max_columns == 0u)
        return -1;

    threshold = options->threshold_percent;
    if (threshold == 0u || threshold > 100u)
        return -1;

    requested_columns =
        (unsigned int)ceil_div_u64(
            (uint64_t)bitmap->width *
            OKG1_NATIVE_X_DPI,
            bitmap->dpi_x);

    output_columns = requested_columns;
    if (output_columns > options->max_columns)
        output_columns = options->max_columns;

    target_rows = count_target_dot_rows(bitmap);
    output_bands =
        (target_rows + OKG1_PINS_PER_BAND - 1u) /
        OKG1_PINS_PER_BAND;

    if (output_columns == 0u ||
        output_bands == 0u)
        return -1;

    columns =
        (unsigned char *)malloc(output_columns);
    if (columns == NULL)
        return -1;

    for (band = 0;
         band < output_bands;
         ++band) {
        memset(columns, 0, output_columns);

        for (x = 0;
             x < output_columns;
             ++x) {
            unsigned int x0;
            unsigned int x1;
            unsigned int pin;
            unsigned char mask = 0;

            source_x_range(
                bitmap, x, &x0, &x1);

            for (pin = 0;
                 pin < OKG1_PINS_PER_BAND;
                 ++pin) {
                unsigned int dot_row =
                    band * OKG1_PINS_PER_BAND +
                    pin;
                unsigned int y0;
                unsigned int y1;

                if (dot_row >= target_rows)
                    break;

                source_y_range(
                    bitmap, dot_row,
                    target_rows, &y0, &y1);

                if (box_is_black(
                        bitmap,
                        x0, x1, y0, y1,
                        threshold)) {
                    mask |=
                        (unsigned char)
                        (1u << pin);
                }
            }

            columns[x] = mask;
        }

        if (okg1_write_columns(
                stream, columns,
                output_columns) != 0) {
            free(columns);
            return -1;
        }

        if (band + 1u < output_bands &&
            okg1_graphics_feed_cr(stream) != 0) {
            free(columns);
            return -1;
        }
    }

    free(columns);

    if (stats != NULL) {
        stats->source_width = bitmap->width;
        stats->source_height = bitmap->height;
        stats->source_dpi_x = bitmap->dpi_x;
        stats->source_dpi_y = bitmap->dpi_y;
        stats->output_columns = output_columns;
        stats->output_bands = output_bands;
        stats->output_dot_rows = target_rows;
        stats->clipped_columns =
            requested_columns > output_columns
            ? requested_columns - output_columns
            : 0u;
    }

    return 0;
}
