#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_WIDTH_INCHES  8u
#define PAGE_HEIGHT_INCHES 6u

static void
set_black(unsigned char *data,
          size_t row_bytes,
          unsigned int width,
          unsigned int height,
          unsigned int x,
          unsigned int y)
{
    unsigned char *row;

    if (x >= width || y >= height)
        return;

    row = data + (size_t)y * row_bytes;
    row[x >> 3] |=
        (unsigned char)(0x80u >> (x & 7u));
}

static void
fill_rect(unsigned char *data,
          size_t row_bytes,
          unsigned int width,
          unsigned int height,
          unsigned int x0,
          unsigned int y0,
          unsigned int x1,
          unsigned int y1)
{
    unsigned int x;
    unsigned int y;

    if (x1 > width)
        x1 = width;
    if (y1 > height)
        y1 = height;

    for (y = y0; y < y1; ++y) {
        for (x = x0; x < x1; ++x) {
            set_black(
                data, row_bytes,
                width, height,
                x, y);
        }
    }
}

static void
draw_diagonal(unsigned char *data,
              size_t row_bytes,
              unsigned int width,
              unsigned int height,
              unsigned int x0,
              unsigned int y0,
              unsigned int x1,
              unsigned int y1,
              unsigned int thickness)
{
    unsigned int steps;
    unsigned int i;

    steps =
        x1 > x0 ? x1 - x0 : 1u;
    if (y1 > y0 &&
        y1 - y0 > steps) {
        steps = y1 - y0;
    }

    for (i = 0; i <= steps; ++i) {
        unsigned int x =
            x0 + (unsigned int)
            (((unsigned long long)
              (x1 - x0) * i) / steps);
        unsigned int y =
            y0 + (unsigned int)
            (((unsigned long long)
              (y1 - y0) * i) / steps);
        unsigned int dx;
        unsigned int dy;

        for (dy = 0;
             dy < thickness;
             ++dy) {
            for (dx = 0;
                 dx < thickness;
                 ++dx) {
                set_black(
                    data, row_bytes,
                    width, height,
                    x + dx, y + dy);
            }
        }
    }
}

int
main(int argc, char **argv)
{
    const char *output_name =
        "okigraph1-360-test.pbm";
    unsigned int dpi = 360u;
    unsigned int width;
    unsigned int height;
    unsigned int x_stroke;
    unsigned int y_stroke;
    size_t row_bytes;
    size_t bytes;
    unsigned char *data;
    FILE *out;
    unsigned int inch;

    if (argc == 3 &&
        strcmp(argv[1], "-o") == 0) {
        output_name = argv[2];
    } else if (argc != 1) {
        fprintf(stderr,
            "Usage: %s [-o FILE]\n",
            argv[0]);
        return 2;
    }

    width = PAGE_WIDTH_INCHES * dpi;
    height = PAGE_HEIGHT_INCHES * dpi;
    row_bytes = (width + 7u) / 8u;
    bytes = row_bytes * height;

    /*
     * Use a 1/60-inch source stroke in both axes so the one-inch ruler
     * survives a 50% box threshold even across the larger inter-band cell.
     */
    x_stroke = dpi / 60u;
    y_stroke = dpi / 60u;
    if (x_stroke == 0u)
        x_stroke = 1u;
    if (y_stroke == 0u)
        y_stroke = 1u;

    data =
        (unsigned char *)calloc(1u, bytes);
    if (data == NULL) {
        fprintf(stderr,
            "error: out of memory\n");
        return 1;
    }

    fill_rect(
        data, row_bytes,
        width, height,
        0u, 0u,
        width, y_stroke);
    fill_rect(
        data, row_bytes,
        width, height,
        0u, height - y_stroke,
        width, height);
    fill_rect(
        data, row_bytes,
        width, height,
        0u, 0u,
        x_stroke, height);
    fill_rect(
        data, row_bytes,
        width, height,
        width - x_stroke, 0u,
        width, height);

    for (inch = 1u;
         inch < PAGE_WIDTH_INCHES;
         ++inch) {
        unsigned int x =
            inch * dpi;
        fill_rect(
            data, row_bytes,
            width, height,
            x, 0u,
            x + x_stroke, height);
    }

    for (inch = 1u;
         inch < PAGE_HEIGHT_INCHES;
         ++inch) {
        unsigned int y =
            inch * dpi;
        fill_rect(
            data, row_bytes,
            width, height,
            0u, y,
            width, y + y_stroke);
    }

    draw_diagonal(
        data, row_bytes,
        width, height,
        dpi, dpi,
        7u * dpi, 5u * dpi,
        x_stroke);
    draw_diagonal(
        data, row_bytes,
        width, height,
        dpi, 5u * dpi,
        7u * dpi, dpi,
        x_stroke);

    fill_rect(
        data, row_bytes,
        width, height,
        dpi / 4u, dpi / 4u,
        dpi / 2u, dpi / 2u);

    {
        unsigned int x;
        unsigned int y;

        for (y = dpi / 4u;
             y < dpi / 2u;
             ++y) {
            for (x = 3u * dpi / 4u;
                 x < dpi;
                 ++x) {
                if (((x + y) & 1u) == 0u) {
                    set_black(
                        data, row_bytes,
                        width, height,
                        x, y);
                }
            }
        }

        for (y = dpi / 4u;
             y < dpi / 2u;
             ++y) {
            for (x = 5u * dpi / 4u;
                 x < 3u * dpi / 2u;
                 ++x) {
                if ((x & 1u) == 0u &&
                    (y & 1u) == 0u) {
                    set_black(
                        data, row_bytes,
                        width, height,
                        x, y);
                }
            }
        }
    }

    out = fopen(output_name, "wb");
    if (out == NULL) {
        fprintf(stderr,
            "error: cannot open '%s': %s\n",
            output_name,
            strerror(errno));
        free(data);
        return 1;
    }

    fprintf(out,
        "P4\n%u %u\n",
        width, height);

    if (fwrite(
            data, 1u, bytes, out) != bytes ||
        fclose(out) != 0) {
        fprintf(stderr,
            "error: failed writing '%s'\n",
            output_name);
        free(data);
        return 1;
    }

    free(data);

    fprintf(stderr,
        "Wrote %s: %u x %u pixels at "
        "%u DPI (8 x 6 inches).\n",
        output_name,
        width, height, dpi);

    return 0;
}
