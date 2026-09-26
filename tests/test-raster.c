#include "okigraph1.h"
#include "okigraph1-raster.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
render_black_page(unsigned int width,
                  unsigned int height,
                  unsigned int dpi,
                  unsigned int max_columns,
                  okg1_raster_stats *stats)
{
    size_t row_bytes =
        (width + 7u) / 8u;
    size_t bytes =
        row_bytes * height;
    unsigned char *data =
        (unsigned char *)malloc(bytes);
    okg1_bitmap bitmap;
    okg1_raster_options opt;
    okg1_stream stream;
    FILE *out;
    int rc = 1;

    if (data == NULL)
        return 1;

    memset(data, 0xff, bytes);

    out = tmpfile();
    if (out == NULL) {
        free(data);
        return 1;
    }

    bitmap.data = data;
    bitmap.width = width;
    bitmap.height = height;
    bitmap.row_bytes = row_bytes;
    bitmap.dpi_x = dpi;
    bitmap.dpi_y = dpi;

    opt.max_columns =
        max_columns;
    opt.threshold_percent = 50u;

    if (okg1_job_begin(
            &stream, out, 0) != 0 ||
        okg1_graphics_begin(
            &stream) != 0 ||
        okg1_render_bitmap(
            &stream,
            &bitmap,
            &opt,
            stats) != 0 ||
        okg1_job_end(
            &stream, 0) != 0) {
        goto done;
    }

    rc = 0;

done:
    fclose(out);
    free(data);
    return rc;
}

static int
run_black_one_inch(void)
{
    okg1_raster_stats stats;

    if (render_black_page(
            360u, 360u,
            360u, 480u,
            &stats) != 0) {
        return 1;
    }

    if (stats.output_columns != 60u) {
        fprintf(stderr,
            "expected 60 columns, got %u\n",
            stats.output_columns);
        return 1;
    }

    if (stats.output_dot_rows != 68u) {
        fprintf(stderr,
            "expected 68 physical dot rows, "
            "got %u\n",
            stats.output_dot_rows);
        return 1;
    }

    if (stats.output_bands != 10u) {
        fprintf(stderr,
            "expected 10 bands, got %u\n",
            stats.output_bands);
        return 1;
    }

    if (stats.clipped_columns != 0u) {
        fprintf(stderr,
            "unexpected clipping: %u columns\n",
            stats.clipped_columns);
        return 1;
    }

    return 0;
}

static int
run_ml82a_width_clip(void)
{
    okg1_raster_stats stats;

    /*
     * 8.5 inches at 360 DPI requests
     * 510 native columns; ML82A keeps 480.
     */
    if (render_black_page(
            3060u, 72u,
            360u, 480u,
            &stats) != 0) {
        return 1;
    }

    if (stats.output_columns != 480u ||
        stats.clipped_columns != 30u) {
        fprintf(stderr,
            "expected 480 columns + "
            "30 clipped, got %u + "
            "%u clipped\n",
            stats.output_columns,
            stats.clipped_columns);
        return 1;
    }

    return 0;
}

int
main(void)
{
    if (run_black_one_inch() != 0)
        return 1;

    if (run_ml82a_width_clip() != 0)
        return 1;

    puts("okigraph1 raster tests: PASS");
    return 0;
}
