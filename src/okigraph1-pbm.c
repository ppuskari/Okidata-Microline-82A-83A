#include "okigraph1.h"
#include "okigraph1-raster.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MODEL_82A_WIDTH 480u
#define MODEL_83A_WIDTH 792u

struct options {
    const char *input_name;
    const char *output_name;
    unsigned int max_width;
    unsigned int source_dpi_x;
    unsigned int source_dpi_y;
    unsigned int threshold_percent;
    int scaled;
    int send_cancel;
    int form_feed;
};

static void
usage(FILE *fp, const char *prog)
{
    fprintf(fp,
        "Usage: %s [options] INPUT.pbm\n"
        "\n"
        "Convert a binary PBM (P4) bitmap to native OkiGraph I output.\n"
        "\n"
        "Modes:\n"
        "  native (default)     1 PBM pixel = 1 native horizontal column;\n"
        "                       each 7 PBM rows = one 7-pin band\n"
        "  scaled               --source-dpi N resamples a conventional\n"
        "                       square-DPI bitmap into physical OkiGraph\n"
        "                       geometry\n"
        "\n"
        "Options:\n"
        "  -o FILE              output .oki file (default: output.oki)\n"
        "  --model 82a|83a      carriage width (default: 82a)\n"
        "  --source-dpi N       set source X/Y DPI and enable scaling\n"
        "  --source-dpi-x N     set source horizontal DPI\n"
        "  --source-dpi-y N     set source vertical DPI\n"
        "  --threshold N        black coverage threshold, 1..100\n"
        "                       (default: 50 percent)\n"
        "  --no-cancel          omit initial CAN ($18)\n"
        "  --no-ff              omit final form feed ($0C)\n"
        "  -h, --help           show this help\n"
        "\n"
        "Scaled geometry:\n"
        "  horizontal: 60 native columns/inch\n"
        "  pin pitch:   1/72 inch inside each 7-pin band\n"
        "  band origin: 15/144 inch per native graphics feed\n",
        prog);
}

static int
parse_uint(const char *text,
           unsigned int min_value,
           unsigned int max_value,
           unsigned int *value)
{
    char *end = NULL;
    unsigned long n;

    errno = 0;
    n = strtoul(text, &end, 10);
    if (errno != 0 ||
        end == text ||
        *end != '\0' ||
        n < min_value ||
        n > max_value)
        return -1;

    *value = (unsigned int)n;
    return 0;
}

static int
parse_args(int argc,
           char **argv,
           struct options *opt)
{
    int i;

    memset(opt, 0, sizeof(*opt));
    opt->output_name = "output.oki";
    opt->max_width = MODEL_82A_WIDTH;
    opt->threshold_percent = 50u;
    opt->send_cancel = 1;
    opt->form_feed = 1;

    for (i = 1; i < argc; ++i) {
        const char *arg = argv[i];

        if (strcmp(arg, "-h") == 0 ||
            strcmp(arg, "--help") == 0) {
            usage(stdout, argv[0]);
            return 1;
        } else if (strcmp(arg, "-o") == 0) {
            if (++i >= argc)
                return -1;
            opt->output_name = argv[i];
        } else if (strcmp(arg, "--model") == 0) {
            if (++i >= argc)
                return -1;
            if (strcmp(argv[i], "82a") == 0 ||
                strcmp(argv[i], "82A") == 0)
                opt->max_width = MODEL_82A_WIDTH;
            else if (strcmp(argv[i], "83a") == 0 ||
                     strcmp(argv[i], "83A") == 0)
                opt->max_width = MODEL_83A_WIDTH;
            else
                return -1;
        } else if (strcmp(arg, "--source-dpi") == 0) {
            unsigned int dpi;
            if (++i >= argc ||
                parse_uint(argv[i], 1u, 9600u, &dpi) != 0)
                return -1;
            opt->source_dpi_x = dpi;
            opt->source_dpi_y = dpi;
            opt->scaled = 1;
        } else if (strcmp(arg, "--source-dpi-x") == 0) {
            if (++i >= argc ||
                parse_uint(argv[i], 1u, 9600u,
                           &opt->source_dpi_x) != 0)
                return -1;
            opt->scaled = 1;
        } else if (strcmp(arg, "--source-dpi-y") == 0) {
            if (++i >= argc ||
                parse_uint(argv[i], 1u, 9600u,
                           &opt->source_dpi_y) != 0)
                return -1;
            opt->scaled = 1;
        } else if (strcmp(arg, "--threshold") == 0) {
            if (++i >= argc ||
                parse_uint(argv[i], 1u, 100u,
                           &opt->threshold_percent) != 0)
                return -1;
        } else if (strcmp(arg, "--no-cancel") == 0) {
            opt->send_cancel = 0;
        } else if (strcmp(arg, "--no-ff") == 0) {
            opt->form_feed = 0;
        } else if (arg[0] == '-') {
            return -1;
        } else if (opt->input_name == NULL) {
            opt->input_name = arg;
        } else {
            return -1;
        }
    }

    if (opt->input_name == NULL)
        return -1;

    if (opt->scaled) {
        if (opt->source_dpi_x == 0u &&
            opt->source_dpi_y != 0u)
            opt->source_dpi_x =
                opt->source_dpi_y;

        if (opt->source_dpi_y == 0u &&
            opt->source_dpi_x != 0u)
            opt->source_dpi_y =
                opt->source_dpi_x;

        if (opt->source_dpi_x == 0u ||
            opt->source_dpi_y == 0u)
            return -1;
    }

    return 0;
}

static int
read_token(FILE *fp,
           char *buf,
           size_t size)
{
    int c;
    size_t n = 0;

    if (size == 0u)
        return -1;

    for (;;) {
        c = fgetc(fp);
        if (c == EOF)
            return -1;
        if (isspace((unsigned char)c))
            continue;
        if (c == '#') {
            do {
                c = fgetc(fp);
            } while (c != EOF && c != '\n');
            continue;
        }
        break;
    }

    for (;;) {
        if (c == EOF ||
            isspace((unsigned char)c))
            break;
        if (c == '#') {
            do {
                c = fgetc(fp);
            } while (c != EOF && c != '\n');
            break;
        }
        if (n + 1u >= size)
            return -1;
        buf[n++] = (char)c;
        c = fgetc(fp);
    }

    buf[n] = '\0';
    return n == 0u ? -1 : 0;
}

static int
read_pbm(FILE *in,
         unsigned char **data,
         unsigned int *width,
         unsigned int *height,
         size_t *row_bytes)
{
    char token[64];
    size_t bytes;
    unsigned char *image;

    if (read_token(in, token, sizeof(token)) != 0 ||
        strcmp(token, "P4") != 0)
        return -1;

    if (read_token(in, token, sizeof(token)) != 0 ||
        parse_uint(token, 1u, 1000000u, width) != 0 ||
        read_token(in, token, sizeof(token)) != 0 ||
        parse_uint(token, 1u, 1000000u, height) != 0)
        return -1;

    *row_bytes = (*width + 7u) / 8u;
    if (*height > (size_t)-1 / *row_bytes)
        return -1;

    bytes =
        (size_t)*height * *row_bytes;
    image =
        (unsigned char *)malloc(bytes);
    if (image == NULL)
        return -1;

    if (fread(image, 1u, bytes, in) != bytes) {
        free(image);
        return -1;
    }

    *data = image;
    return 0;
}

static int
pixel_is_black(const unsigned char *data,
               size_t row_bytes,
               unsigned int x,
               unsigned int y)
{
    const unsigned char *row =
        data + (size_t)y * row_bytes;

    return (row[x >> 3] &
            (unsigned char)
            (0x80u >> (x & 7u))) != 0;
}

static int
render_native(okg1_stream *stream,
              const unsigned char *data,
              unsigned int width,
              unsigned int height,
              size_t row_bytes,
              unsigned int max_width)
{
    unsigned int bands;
    unsigned int band;
    unsigned int x;
    unsigned char *columns;

    if (width > max_width) {
        fprintf(stderr,
            "error: native PBM width %u "
            "exceeds selected printer width %u\n",
            width, max_width);
        return -1;
    }

    bands = (height + 6u) / 7u;
    columns =
        (unsigned char *)malloc(width);
    if (columns == NULL)
        return -1;

    for (band = 0; band < bands; ++band) {
        for (x = 0; x < width; ++x) {
            unsigned int pin;
            unsigned char mask = 0;

            for (pin = 0; pin < 7u; ++pin) {
                unsigned int y =
                    band * 7u + pin;
                if (y < height &&
                    pixel_is_black(
                        data, row_bytes,
                        x, y)) {
                    mask |=
                        (unsigned char)
                        (1u << pin);
                }
            }
            columns[x] = mask;
        }

        if (okg1_write_columns(
                stream,
                columns,
                width) != 0) {
            free(columns);
            return -1;
        }

        if (band + 1u < bands &&
            okg1_graphics_feed_cr(
                stream) != 0) {
            free(columns);
            return -1;
        }
    }

    free(columns);
    fprintf(stderr,
        "Native mapping: %u x %u logical pixels "
        "-> %u 7-pin bands.\n",
        width, height, bands);
    return 0;
}

int
main(int argc, char **argv)
{
    struct options opt;
    FILE *in = NULL;
    FILE *out = NULL;
    unsigned char *data = NULL;
    unsigned int width = 0;
    unsigned int height = 0;
    size_t row_bytes = 0;
    okg1_stream stream;
    int rc;

    rc = parse_args(argc, argv, &opt);
    if (rc > 0)
        return 0;
    if (rc < 0) {
        usage(stderr, argv[0]);
        return 2;
    }

    in = fopen(opt.input_name, "rb");
    if (in == NULL) {
        fprintf(stderr,
            "error: cannot open '%s': %s\n",
            opt.input_name,
            strerror(errno));
        return 1;
    }

    if (read_pbm(in,
                 &data,
                 &width,
                 &height,
                 &row_bytes) != 0) {
        fprintf(stderr,
            "error: invalid or truncated "
            "binary PBM (P4)\n");
        fclose(in);
        return 1;
    }

    fclose(in);
    in = NULL;

    out = fopen(opt.output_name, "wb");
    if (out == NULL) {
        fprintf(stderr,
            "error: cannot open '%s': %s\n",
            opt.output_name,
            strerror(errno));
        free(data);
        return 1;
    }

    if (okg1_job_begin(
            &stream,
            out,
            opt.send_cancel) != 0 ||
        okg1_graphics_begin(
            &stream) != 0) {
        fprintf(stderr,
            "error: failed to begin "
            "OkiGraph stream\n");
        goto fail;
    }

    if (opt.scaled) {
        okg1_bitmap bitmap;
        okg1_raster_options raster_opt;
        okg1_raster_stats stats;

        bitmap.data = data;
        bitmap.width = width;
        bitmap.height = height;
        bitmap.row_bytes = row_bytes;
        bitmap.dpi_x =
            opt.source_dpi_x;
        bitmap.dpi_y =
            opt.source_dpi_y;

        raster_opt.max_columns =
            opt.max_width;
        raster_opt.threshold_percent =
            opt.threshold_percent;

        if (okg1_render_bitmap(
                &stream,
                &bitmap,
                &raster_opt,
                &stats) != 0) {
            fprintf(stderr,
                "error: scaled raster "
                "conversion failed\n");
            goto fail;
        }

        fprintf(stderr,
            "Scaled mapping: "
            "%u x %u @ %ux%u DPI -> "
            "%u columns, %u dot rows "
            "in %u native bands.\n",
            stats.source_width,
            stats.source_height,
            stats.source_dpi_x,
            stats.source_dpi_y,
            stats.output_columns,
            stats.output_dot_rows,
            stats.output_bands);

        if (stats.clipped_columns != 0u) {
            fprintf(stderr,
                "Clipped %u native columns "
                "at the selected carriage width.\n",
                stats.clipped_columns);
        }
    } else {
        if (render_native(
                &stream,
                data,
                width,
                height,
                row_bytes,
                opt.max_width) != 0)
            goto fail;
    }

    if (okg1_job_end(
            &stream,
            opt.form_feed) != 0) {
        fprintf(stderr,
            "error: failed to finish "
            "OkiGraph stream\n");
        goto fail;
    }

    if (fclose(out) != 0) {
        out = NULL;
        fprintf(stderr,
            "error: close failed for '%s'\n",
            opt.output_name);
        free(data);
        return 1;
    }
    out = NULL;

    free(data);
    fprintf(stderr,
        "Wrote %s\n",
        opt.output_name);
    return 0;

fail:
    if (out != NULL)
        fclose(out);
    if (in != NULL)
        fclose(in);
    free(data);
    return 1;
}
