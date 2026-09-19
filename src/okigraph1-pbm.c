#include "okigraph1.h"

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
    int send_cancel;
    int form_feed;
};

static void
usage(FILE *fp, const char *prog)
{
    fprintf(fp,
        "Usage: %s [options] INPUT.pbm\n"
        "\n"
        "Convert a binary PBM (P4) bitmap to a native OkiGraph I stream.\n"
        "One PBM pixel maps to one native 60-column/in graphics column,\n"
        "and each group of seven PBM rows maps to one seven-pin band.\n"
        "\n"
        "Options:\n"
        "  -o FILE              output .oki file (default: output.oki)\n"
        "  --model 82a|83a      enforce carriage width (default: 82a)\n"
        "  --no-cancel          omit initial CAN ($18)\n"
        "  --no-ff              omit final form feed ($0C)\n"
        "  -h, --help           show this help\n",
        prog);
}

static int
parse_args(int argc, char **argv, struct options *opt)
{
    int i;

    opt->input_name = NULL;
    opt->output_name = "output.oki";
    opt->max_width = MODEL_82A_WIDTH;
    opt->send_cancel = 1;
    opt->form_feed = 1;

    for (i = 1; i < argc; ++i) {
        const char *arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            usage(stdout, argv[0]);
            return 1;
        } else if (strcmp(arg, "-o") == 0) {
            if (++i >= argc)
                return -1;
            opt->output_name = argv[i];
        } else if (strcmp(arg, "--model") == 0) {
            if (++i >= argc)
                return -1;
            if (strcmp(argv[i], "82a") == 0 || strcmp(argv[i], "82A") == 0)
                opt->max_width = MODEL_82A_WIDTH;
            else if (strcmp(argv[i], "83a") == 0 || strcmp(argv[i], "83A") == 0)
                opt->max_width = MODEL_83A_WIDTH;
            else
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

    return opt->input_name == NULL ? -1 : 0;
}

static int
read_token(FILE *fp, char *buf, size_t size)
{
    int c;
    size_t n = 0;

    if (size == 0)
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
        if (c == EOF || isspace((unsigned char)c))
            break;

        if (c == '#') {
            do {
                c = fgetc(fp);
            } while (c != EOF && c != '\n');
            break;
        }

        if (n + 1 >= size)
            return -1;

        buf[n++] = (char)c;
        c = fgetc(fp);
    }

    buf[n] = '\0';
    return n == 0 ? -1 : 0;
}

static int
parse_dimension(const char *text, unsigned int *value)
{
    char *end = NULL;
    unsigned long n;

    errno = 0;
    n = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        n == 0 || n > 1000000ul)
        return -1;

    *value = (unsigned int)n;
    return 0;
}

static int
pixel_is_black(const unsigned char *row, unsigned int x)
{
    unsigned int byte_index = x >> 3;
    unsigned int bit_index = x & 7u;

    return (row[byte_index] & (unsigned char)(0x80u >> bit_index)) != 0;
}

int
main(int argc, char **argv)
{
    struct options opt;
    FILE *in = NULL;
    FILE *out = NULL;
    okg1_stream stream;
    char token[64];
    unsigned int width;
    unsigned int height;
    unsigned int row_bytes;
    unsigned int bands;
    unsigned int band;
    unsigned int y;
    unsigned int x;
    unsigned char *rows = NULL;
    unsigned char *columns = NULL;
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
        fprintf(stderr, "error: cannot open '%s': %s\n",
                opt.input_name, strerror(errno));
        return 1;
    }

    if (read_token(in, token, sizeof(token)) != 0 ||
        strcmp(token, "P4") != 0) {
        fprintf(stderr, "error: input is not binary PBM (P4)\n");
        fclose(in);
        return 1;
    }

    if (read_token(in, token, sizeof(token)) != 0 ||
        parse_dimension(token, &width) != 0 ||
        read_token(in, token, sizeof(token)) != 0 ||
        parse_dimension(token, &height) != 0) {
        fprintf(stderr, "error: invalid PBM dimensions\n");
        fclose(in);
        return 1;
    }

    if (width > opt.max_width) {
        fprintf(stderr,
            "error: PBM width %u exceeds selected printer width %u columns\n",
            width, opt.max_width);
        fclose(in);
        return 1;
    }

    row_bytes = (width + 7u) / 8u;
    bands = (height + 6u) / 7u;

    rows = (unsigned char *)calloc(7u, row_bytes);
    columns = (unsigned char *)malloc(width);
    if (rows == NULL || columns == NULL) {
        fprintf(stderr, "error: out of memory\n");
        free(rows);
        free(columns);
        fclose(in);
        return 1;
    }

    out = fopen(opt.output_name, "wb");
    if (out == NULL) {
        fprintf(stderr, "error: cannot open '%s': %s\n",
                opt.output_name, strerror(errno));
        free(rows);
        free(columns);
        fclose(in);
        return 1;
    }

    if (okg1_job_begin(&stream, out, opt.send_cancel) != 0 ||
        okg1_graphics_begin(&stream) != 0) {
        fprintf(stderr, "error: failed to begin OkiGraph stream\n");
        goto fail;
    }

    for (band = 0; band < bands; ++band) {
        memset(rows, 0, (size_t)7u * row_bytes);

        for (y = 0; y < 7u; ++y) {
            unsigned int source_y = band * 7u + y;

            if (source_y >= height)
                break;

            if (fread(rows + (size_t)y * row_bytes,
                      1, row_bytes, in) != row_bytes) {
                fprintf(stderr,
                    "error: truncated PBM raster at source row %u\n",
                    source_y);
                goto fail;
            }
        }

        for (x = 0; x < width; ++x) {
            unsigned char mask = 0;

            for (y = 0; y < 7u; ++y) {
                if (pixel_is_black(rows + (size_t)y * row_bytes, x))
                    mask |= (unsigned char)(1u << y);
            }

            columns[x] = mask;
        }

        if (okg1_write_columns(&stream, columns, width) != 0) {
            fprintf(stderr, "error: output failed in band %u\n", band);
            goto fail;
        }

        if (band + 1u < bands &&
            okg1_graphics_feed_cr(&stream) != 0) {
            fprintf(stderr, "error: graphics feed failed after band %u\n",
                    band);
            goto fail;
        }
    }

    if (okg1_job_end(&stream, opt.form_feed) != 0) {
        fprintf(stderr, "error: failed to finish OkiGraph stream\n");
        goto fail;
    }

    if (fclose(out) != 0) {
        out = NULL;
        fprintf(stderr, "error: close failed for '%s'\n", opt.output_name);
        goto fail;
    }
    out = NULL;

    fclose(in);
    free(rows);
    free(columns);

    fprintf(stderr,
        "Converted %s: %u x %u PBM pixels -> %u native 7-pin bands.\n",
        opt.input_name, width, height, bands);
    fprintf(stderr, "Wrote %s\n", opt.output_name);
    return 0;

fail:
    if (out != NULL)
        fclose(out);
    fclose(in);
    free(rows);
    free(columns);
    return 1;
}
