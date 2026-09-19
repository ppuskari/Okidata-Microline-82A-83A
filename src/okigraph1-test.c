#include "okigraph1.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MODEL_82A_WIDTH 480u  /* 8.0 in at native 60 columns/in */
#define MODEL_83A_WIDTH 792u  /* 13.2 in at native 60 columns/in */
#define DEFAULT_BANDS   49u   /* band 0 -> band 48 = 48 native feeds */

enum pattern_kind {
    PATTERN_CALIBRATION,
    PATTERN_SEAM,
    PATTERN_RULER
};

struct options {
    const char *output_name;
    unsigned int width;
    unsigned int bands;
    enum pattern_kind pattern;
    int send_cancel;
    int form_feed;
};

static void
usage(FILE *fp, const char *prog)
{
    fprintf(fp,
        "Usage: %s [options]\n"
        "\n"
        "Generate a raw OkiGraph I graphics calibration stream.\n"
        "\n"
        "Options:\n"
        "  -o FILE              output file (default: okigraph1-test.oki)\n"
        "  --model 82a|83a      native carriage width (default: 82a)\n"
        "  --width COLUMNS      override graphics columns\n"
        "  --bands N            number of 7-pin bands (default: 49)\n"
        "  --pattern NAME       calibration, seam, or ruler\n"
        "  --no-cancel          omit initial CAN ($18)\n"
        "  --no-ff              omit final form feed ($0C)\n"
        "  -h, --help           show this help\n"
        "\n"
        "Native OkiGraph I horizontal pitch is 60 columns/inch.\n"
        "The default 49-band page places band 0 and band 48 exactly\n"
        "48 graphics-feed commands apart for the 5-inch feed test.\n",
        prog);
}

static int
parse_uint(const char *text, unsigned int *value)
{
    char *end = NULL;
    unsigned long n;

    errno = 0;
    n = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || n == 0 || n > 65535ul)
        return -1;

    *value = (unsigned int)n;
    return 0;
}

static int
parse_args(int argc, char **argv, struct options *opt)
{
    int i;

    opt->output_name = "okigraph1-test.oki";
    opt->width = MODEL_82A_WIDTH;
    opt->bands = DEFAULT_BANDS;
    opt->pattern = PATTERN_CALIBRATION;
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
                opt->width = MODEL_82A_WIDTH;
            else if (strcmp(argv[i], "83a") == 0 || strcmp(argv[i], "83A") == 0)
                opt->width = MODEL_83A_WIDTH;
            else
                return -1;
        } else if (strcmp(arg, "--width") == 0) {
            if (++i >= argc || parse_uint(argv[i], &opt->width) != 0)
                return -1;
        } else if (strcmp(arg, "--bands") == 0) {
            if (++i >= argc || parse_uint(argv[i], &opt->bands) != 0)
                return -1;
        } else if (strcmp(arg, "--pattern") == 0) {
            if (++i >= argc)
                return -1;
            if (strcmp(argv[i], "calibration") == 0)
                opt->pattern = PATTERN_CALIBRATION;
            else if (strcmp(argv[i], "seam") == 0)
                opt->pattern = PATTERN_SEAM;
            else if (strcmp(argv[i], "ruler") == 0)
                opt->pattern = PATTERN_RULER;
            else
                return -1;
        } else if (strcmp(arg, "--no-cancel") == 0) {
            opt->send_cancel = 0;
        } else if (strcmp(arg, "--no-ff") == 0) {
            opt->form_feed = 0;
        } else {
            return -1;
        }
    }

    return 0;
}

/*
 * Calibration pattern layout:
 *
 *   - Every 60th column is a full seven-pin vertical ruler mark.
 *   - Left 36 columns form a 50% duty dense field to expose band seams.
 *   - Next 72 columns contain a diagonal seven-pin pattern.
 *   - Remaining area contains sparse checker/ruler information.
 *   - Bands 0 and 48 carry a top-pin horizontal reference.  Since there
 *     are exactly 48 native graphics-feed commands between them, their
 *     top-pin baselines are the firmware-derived 5-inch feed check.
 */
static unsigned char
calibration_mask(unsigned int band, unsigned int x, unsigned int width)
{
    unsigned char mask = 0;
    unsigned int pin;

    (void)width;

    if ((x % 60u) == 0u)
        mask |= 0x7fu;

    if (x < 36u) {
        if ((x & 1u) == 0u)
            mask |= 0x7fu;
    } else if (x < 108u) {
        pin = (x + band) % 7u;
        mask |= (unsigned char)(1u << pin);
    } else {
        if (((x / 4u) + band) & 1u)
            mask |= 0x55u; /* pins 0,2,4,6 */
        else
            mask |= 0x2au; /* pins 1,3,5 */

        /* Keep the large field sparse enough to avoid unnecessary head heat. */
        if ((x % 3u) != 0u)
            mask = (unsigned char)(mask & (1u << ((x + 2u * band) % 7u)));
    }

    if (band == 0u || band == 48u)
        mask |= 0x01u;

    return mask;
}

static unsigned char
seam_mask(unsigned int band, unsigned int x, unsigned int width)
{
    unsigned char mask = 0;

    (void)width;

    /* 50% duty dense block: horizontal gaps between bands show immediately. */
    if (x < 60u && ((x + band) & 1u) == 0u)
        mask |= 0x7fu;

    /* Bottom/top pin pair across a second one-inch region. */
    if (x >= 72u && x < 132u) {
        mask |= 0x40u; /* bottom graphics pin */
        mask |= 0x01u; /* top graphics pin */
    }

    if ((x % 60u) == 0u)
        mask |= 0x7fu;

    return mask;
}

static unsigned char
ruler_mask(unsigned int band, unsigned int x, unsigned int width)
{
    unsigned char mask = 0;

    (void)width;

    if ((x % 60u) == 0u)
        mask |= 0x7fu;
    else if ((x % 30u) == 0u)
        mask |= 0x15u;
    else if ((x % 10u) == 0u)
        mask |= 0x01u;

    if ((band % 12u) == 0u)
        mask |= 0x01u;

    return mask;
}

static unsigned char
pattern_mask(enum pattern_kind pattern,
             unsigned int band,
             unsigned int x,
             unsigned int width)
{
    switch (pattern) {
    case PATTERN_SEAM:
        return seam_mask(band, x, width);
    case PATTERN_RULER:
        return ruler_mask(band, x, width);
    case PATTERN_CALIBRATION:
    default:
        return calibration_mask(band, x, width);
    }
}

int
main(int argc, char **argv)
{
    struct options opt;
    okg1_stream stream;
    FILE *out;
    unsigned int band;
    unsigned int x;
    int rc;

    rc = parse_args(argc, argv, &opt);
    if (rc > 0)
        return 0;
    if (rc < 0) {
        usage(stderr, argv[0]);
        return 2;
    }

    out = fopen(opt.output_name, "wb");
    if (out == NULL) {
        fprintf(stderr, "error: cannot open '%s': %s\n",
                opt.output_name, strerror(errno));
        return 1;
    }

    if (okg1_job_begin(&stream, out, opt.send_cancel) != 0 ||
        okg1_graphics_begin(&stream) != 0) {
        fprintf(stderr, "error: failed to begin OkiGraph stream\n");
        fclose(out);
        return 1;
    }

    for (band = 0; band < opt.bands; ++band) {
        for (x = 0; x < opt.width; ++x) {
            unsigned char mask = pattern_mask(opt.pattern, band, x, opt.width);
            if (okg1_write_column(&stream, mask) != 0) {
                fprintf(stderr, "error: write failed in band %u column %u\n",
                        band, x);
                fclose(out);
                return 1;
            }
        }

        if (band + 1u < opt.bands) {
            if (okg1_graphics_feed_cr(&stream) != 0) {
                fprintf(stderr, "error: graphics feed failed after band %u\n", band);
                fclose(out);
                return 1;
            }
        }
    }

    if (okg1_job_end(&stream, opt.form_feed) != 0) {
        fprintf(stderr, "error: failed to finish stream\n");
        fclose(out);
        return 1;
    }

    if (fclose(out) != 0) {
        fprintf(stderr, "error: close failed for '%s'\n", opt.output_name);
        return 1;
    }

    fprintf(stderr,
        "Wrote %s: %u columns x %u native 7-pin bands.\n",
        opt.output_name, opt.width, opt.bands);

    if (opt.bands >= 49u) {
        fprintf(stderr,
            "Bands 0 and 48 are 48 native graphics feeds apart;\n"
            "measure their top-pin baselines for the 5.000-inch feed check.\n");
    }

    return 0;
}
