/*
 * rastertookigraph1 - CUPS raster filter for Okidata MICROLINE 82A/83A
 * with OkiGraph I firmware.
 *
 * The CUPS-facing layer intentionally contains no printer geometry beyond
 * selecting the carriage width.  Raster geometry is handled by the same
 * okigraph1-raster mapper that was validated on physical ML82A hardware.
 */

#include "okigraph1.h"
#include "okigraph1-raster.h"

#include <cups/ppd.h>
#include <cups/raster.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_MAX_COLUMNS       480u
#define DEFAULT_THRESHOLD_PERCENT 50u

static volatile sig_atomic_t canceled = 0;

struct filter_config {
    unsigned int max_columns;
    unsigned int threshold_percent;
};

static void
cancel_job(int sig)
{
    (void)sig;
    canceled = 1;
}

static int
parse_uint(const char *text,
           unsigned int min_value,
           unsigned int max_value,
           unsigned int *value)
{
    char *end = NULL;
    unsigned long n;

    if (text == NULL || *text == '\0')
        return -1;

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

static void
load_ppd_config(struct filter_config *cfg)
{
    const char *ppd_name;
    const char *env;
    ppd_file_t *ppd = NULL;
    ppd_attr_t *attr;
    unsigned int value;

    cfg->max_columns = DEFAULT_MAX_COLUMNS;
    cfg->threshold_percent = DEFAULT_THRESHOLD_PERCENT;

    ppd_name = getenv("PPD");
    if (ppd_name != NULL && *ppd_name != '\0')
        ppd = ppdOpenFile(ppd_name);

    if (ppd != NULL) {
        attr = ppdFindAttr(ppd, "OkiGraphMaxColumns", NULL);
        if (attr != NULL &&
            parse_uint(attr->value, 1u, 4096u, &value) == 0)
            cfg->max_columns = value;

        attr = ppdFindAttr(ppd, "OkiGraphThreshold", NULL);
        if (attr != NULL &&
            parse_uint(attr->value, 1u, 100u, &value) == 0)
            cfg->threshold_percent = value;

        ppdClose(ppd);
    }

    /*
     * Environment overrides make the filter easy to exercise outside a
     * configured queue and are also useful during hardware development.
     */
    env = getenv("OKIGRAPH_MAX_COLUMNS");
    if (env != NULL &&
        parse_uint(env, 1u, 4096u, &value) == 0)
        cfg->max_columns = value;

    env = getenv("OKIGRAPH_THRESHOLD");
    if (env != NULL &&
        parse_uint(env, 1u, 100u, &value) == 0)
        cfg->threshold_percent = value;
}

static int
supported_color_space(cups_cspace_t space)
{
    return space == CUPS_CSPACE_K ||
           space == CUPS_CSPACE_W ||
           space == CUPS_CSPACE_SW;
}

static int
source_pixel_black(const cups_page_header2_t *header,
                   const unsigned char *line,
                   unsigned int x)
{
    unsigned int value;
    int k_space = header->cupsColorSpace == CUPS_CSPACE_K;

    if (header->cupsBitsPerPixel == 1u) {
        value = (line[x >> 3] >>
                 (7u - (x & 7u))) & 1u;

        /*
         * CUPS K uses 1 = black.  W/SW are luminance spaces, where
         * 0 = black and 1 = white at one bit per pixel.
         */
        return k_space ? value != 0u : value == 0u;
    }

    value = line[x];

    /*
     * For 8-bit K, larger values mean more black ink.  For W/SW,
     * lower luminance values are darker.
     */
    return k_space ? value >= 128u : value < 128u;
}

static int
read_page_bitmap(cups_raster_t *ras,
                 const cups_page_header2_t *header,
                 unsigned char **packed_data,
                 size_t *packed_row_bytes)
{
    unsigned char *data = NULL;
    unsigned char *line = NULL;
    size_t row_bytes;
    size_t bytes;
    unsigned int x;
    unsigned int y;

    if (!supported_color_space(header->cupsColorSpace)) {
        fprintf(stderr,
            "ERROR: OkiGraph I supports only K/W/SW monochrome raster; "
            "got cupsColorSpace=%u.\n",
            (unsigned int)header->cupsColorSpace);
        return -1;
    }

    if (header->cupsColorOrder != CUPS_ORDER_CHUNKED) {
        fprintf(stderr,
            "ERROR: Unsupported CUPS color order %u; expected chunked.\n",
            (unsigned int)header->cupsColorOrder);
        return -1;
    }

    if (!((header->cupsBitsPerColor == 1u &&
           header->cupsBitsPerPixel == 1u) ||
          (header->cupsBitsPerColor == 8u &&
           header->cupsBitsPerPixel == 8u))) {
        fprintf(stderr,
            "ERROR: Unsupported raster depth: %u bits/color, "
            "%u bits/pixel.\n",
            header->cupsBitsPerColor,
            header->cupsBitsPerPixel);
        return -1;
    }

    if (header->cupsWidth == 0u ||
        header->cupsHeight == 0u ||
        header->HWResolution[0] == 0u ||
        header->HWResolution[1] == 0u) {
        fputs("ERROR: Invalid CUPS raster page geometry.\n", stderr);
        return -1;
    }

    row_bytes = (header->cupsWidth + 7u) / 8u;

    if (header->cupsBitsPerPixel == 1u) {
        if (header->cupsBytesPerLine < row_bytes) {
            fputs("ERROR: CUPS raster line is shorter than packed width.\n",
                  stderr);
            return -1;
        }
    } else if (header->cupsBytesPerLine < header->cupsWidth) {
        fputs("ERROR: CUPS raster line is shorter than 8-bit width.\n",
              stderr);
        return -1;
    }

    if (header->cupsHeight > (size_t)-1 / row_bytes) {
        fputs("ERROR: Raster page is too large.\n", stderr);
        return -1;
    }

    bytes = (size_t)header->cupsHeight * row_bytes;
    data = (unsigned char *)calloc(1u, bytes);
    line = (unsigned char *)malloc(header->cupsBytesPerLine);

    if (data == NULL || line == NULL) {
        fputs("ERROR: Unable to allocate raster page buffer.\n", stderr);
        free(data);
        free(line);
        return -1;
    }

    for (y = 0; y < header->cupsHeight; ++y) {
        if (canceled) {
            free(data);
            free(line);
            return -1;
        }

        if (cupsRasterReadPixels(ras,
                                 line,
                                 header->cupsBytesPerLine) !=
            header->cupsBytesPerLine) {
            fprintf(stderr,
                "ERROR: Truncated CUPS raster at row %u.\n", y);
            free(data);
            free(line);
            return -1;
        }

        for (x = 0; x < header->cupsWidth; ++x) {
            if (source_pixel_black(header, line, x)) {
                unsigned char *dst =
                    data + (size_t)y * row_bytes + (x >> 3);
                *dst |= (unsigned char)(0x80u >> (x & 7u));
            }
        }
    }

    free(line);
    *packed_data = data;
    *packed_row_bytes = row_bytes;
    return 0;
}

static int
render_page(okg1_stream *stream,
            const cups_page_header2_t *header,
            const struct filter_config *cfg,
            const unsigned char *data,
            size_t row_bytes,
            unsigned int page)
{
    okg1_bitmap bitmap;
    okg1_raster_options raster_opt;
    okg1_raster_stats stats;

    bitmap.data = data;
    bitmap.width = header->cupsWidth;
    bitmap.height = header->cupsHeight;
    bitmap.row_bytes = row_bytes;
    bitmap.dpi_x = header->HWResolution[0];
    bitmap.dpi_y = header->HWResolution[1];

    raster_opt.max_columns = cfg->max_columns;
    raster_opt.threshold_percent = cfg->threshold_percent;

    if (okg1_graphics_begin(stream) != 0) {
        fputs("ERROR: Unable to enter OkiGraph I graphics mode.\n", stderr);
        return -1;
    }

    if (okg1_render_bitmap(stream,
                           &bitmap,
                           &raster_opt,
                           &stats) != 0) {
        fputs("ERROR: OkiGraph I raster mapping failed.\n", stderr);
        return -1;
    }

    /*
     * End graphics and form-feed this page.  The stream remains reusable;
     * the next page re-enters graphics mode without another CAN reset.
     */
    if (okg1_job_end(stream, 1) != 0) {
        fputs("ERROR: Unable to finish OkiGraph I page.\n", stderr);
        return -1;
    }

    fprintf(stderr,
        "DEBUG: Page %u mapped %ux%u @ %ux%u DPI -> "
        "%u columns, %u dot rows, %u bands.\n",
        page,
        stats.source_width,
        stats.source_height,
        stats.source_dpi_x,
        stats.source_dpi_y,
        stats.output_columns,
        stats.output_dot_rows,
        stats.output_bands);

    if (stats.clipped_columns != 0u) {
        fprintf(stderr,
            "DEBUG: Page %u clipped %u native columns at carriage width.\n",
            page, stats.clipped_columns);
    }

    fprintf(stderr, "ATTR: job-impressions-completed=%u\n", page);
    return 0;
}

int
main(int argc, char **argv)
{
    struct filter_config cfg;
    cups_raster_t *ras = NULL;
    cups_page_header2_t header;
    okg1_stream stream;
    int fd = 0;
    int close_fd = 0;
    int stream_started = 0;
    unsigned int page = 0;
    int result = 1;

    if (argc < 6 || argc > 7) {
        fputs("ERROR: Usage: rastertookigraph1 "
              "job-id user title copies options [file]\n",
              stderr);
        return 1;
    }

    signal(SIGTERM, cancel_job);
    signal(SIGINT, cancel_job);

    load_ppd_config(&cfg);

    fprintf(stderr,
        "DEBUG: OkiGraph I CUPS filter: max_columns=%u threshold=%u%%.\n",
        cfg.max_columns, cfg.threshold_percent);

    if (argc == 7) {
        fd = open(argv[6], O_RDONLY);
        if (fd < 0) {
            fprintf(stderr,
                "ERROR: Unable to open raster file '%s': %s\n",
                argv[6], strerror(errno));
            return 1;
        }
        close_fd = 1;
    }

    ras = cupsRasterOpen(fd, CUPS_RASTER_READ);
    if (ras == NULL) {
        fputs("ERROR: Unable to open CUPS raster input.\n", stderr);
        goto done;
    }

    while (!canceled &&
           cupsRasterReadHeader2(ras, &header)) {
        unsigned char *data = NULL;
        size_t row_bytes = 0;

        ++page;

        fprintf(stderr,
            "DEBUG: Page %u: cupsWidth=%u cupsHeight=%u "
            "resolution=%ux%u bits=%u/%u colorspace=%u.\n",
            page,
            header.cupsWidth,
            header.cupsHeight,
            header.HWResolution[0],
            header.HWResolution[1],
            header.cupsBitsPerColor,
            header.cupsBitsPerPixel,
            (unsigned int)header.cupsColorSpace);

        if (read_page_bitmap(ras,
                             &header,
                             &data,
                             &row_bytes) != 0) {
            free(data);
            goto done;
        }

        if (!stream_started) {
            if (okg1_job_begin(&stream, stdout, 1) != 0) {
                fputs("ERROR: Unable to begin OkiGraph I job.\n", stderr);
                free(data);
                goto done;
            }
            stream_started = 1;
        }

        if (render_page(&stream,
                        &header,
                        &cfg,
                        data,
                        row_bytes,
                        page) != 0) {
            free(data);
            goto done;
        }

        free(data);
    }

    if (canceled) {
        fputs("INFO: OkiGraph I job canceled.\n", stderr);
        goto done;
    }

    if (page == 0u) {
        fputs("ERROR: No raster pages received.\n", stderr);
        goto done;
    }

    fprintf(stderr, "ATTR: job-impressions=%u\n", page);
    result = 0;

done:
    if (ras != NULL)
        cupsRasterClose(ras);

    if (close_fd)
        close(fd);

    return result;
}
