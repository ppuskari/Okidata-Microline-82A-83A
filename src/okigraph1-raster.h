#ifndef OKIGRAPH1_RASTER_H
#define OKIGRAPH1_RASTER_H

#include "okigraph1.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OKG1_NATIVE_X_DPI       60u
#define OKG1_PIN_Y_DPI          72u
#define OKG1_FEED_STEPS         15u
#define OKG1_FEED_STEP_DPI      144u
#define OKG1_PINS_PER_BAND      7u

typedef struct okg1_bitmap {
    const unsigned char *data;
    unsigned int width;
    unsigned int height;
    size_t row_bytes;
    unsigned int dpi_x;
    unsigned int dpi_y;
} okg1_bitmap;

typedef struct okg1_raster_options {
    unsigned int max_columns;
    unsigned int threshold_percent;
} okg1_raster_options;

typedef struct okg1_raster_stats {
    unsigned int source_width;
    unsigned int source_height;
    unsigned int source_dpi_x;
    unsigned int source_dpi_y;
    unsigned int output_columns;
    unsigned int output_bands;
    unsigned int output_dot_rows;
    unsigned int clipped_columns;
} okg1_raster_stats;

/*
 * Resample a conventional rectangular source bitmap into the native OkiGraph I
 * geometry. Source pixels are packed MSB-first, one bit per pixel, with 1 =
 * black. The destination is 60 columns/inch horizontally. Vertically, the
 * seven physical pins are spaced at 1/72 inch and successive band origins are
 * separated by 15/144 inch, so destination rows are intentionally nonuniform.
 */
int okg1_render_bitmap(okg1_stream *stream,
                       const okg1_bitmap *bitmap,
                       const okg1_raster_options *options,
                       okg1_raster_stats *stats);

#ifdef __cplusplus
}
#endif

#endif
