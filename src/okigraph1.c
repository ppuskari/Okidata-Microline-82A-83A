#include "okigraph1.h"

#define OKG1_ETX              0x03u
#define OKG1_EXIT_GRAPHICS    0x02u
#define OKG1_GRAPHICS_LF_CR   0x0eu
#define OKG1_CAN              0x18u
#define OKG1_FF               0x0cu

static int
put_byte(okg1_stream *s, unsigned int value)
{
    if (s == NULL || s->out == NULL || s->error)
        return -1;

    if (fputc((int)(value & 0xffu), s->out) == EOF) {
        s->error = 1;
        return -1;
    }

    return 0;
}

static int
put_command(okg1_stream *s, unsigned int command)
{
    if (put_byte(s, OKG1_ETX) != 0)
        return -1;
    return put_byte(s, command);
}

int
okg1_job_begin(okg1_stream *s, FILE *out, int send_cancel)
{
    if (s == NULL || out == NULL)
        return -1;

    s->out = out;
    s->in_graphics = 0;
    s->error = 0;

    if (send_cancel && put_byte(s, OKG1_CAN) != 0)
        return -1;

    return 0;
}

int
okg1_graphics_begin(okg1_stream *s)
{
    if (s == NULL || s->out == NULL || s->error)
        return -1;

    if (s->in_graphics)
        return 0;

    if (put_byte(s, OKG1_ETX) != 0)
        return -1;

    s->in_graphics = 1;
    return 0;
}

int
okg1_write_column(okg1_stream *s, unsigned char dot_mask)
{
    unsigned char wire_byte;

    if (s == NULL || !s->in_graphics || s->error)
        return -1;

    /*
     * OkiGraph consumes seven host graphics bits.  Keep bit 7 set on the
     * wire; this mirrors historical Okidata graphics practice and makes
     * the data byte unambiguous with respect to ETX ($03).
     */
    wire_byte = (unsigned char)(0x80u | (dot_mask & 0x7fu));
    return put_byte(s, wire_byte);
}

int
okg1_write_columns(okg1_stream *s,
                   const unsigned char *dot_masks,
                   size_t count)
{
    size_t i;

    if (s == NULL || dot_masks == NULL)
        return -1;

    for (i = 0; i < count; ++i) {
        if (okg1_write_column(s, dot_masks[i]) != 0)
            return -1;
    }

    return 0;
}

int
okg1_graphics_feed_cr(okg1_stream *s)
{
    if (s == NULL || !s->in_graphics || s->error)
        return -1;

    /* This command keeps the printer in graphics mode. */
    return put_command(s, OKG1_GRAPHICS_LF_CR);
}

int
okg1_graphics_end(okg1_stream *s)
{
    if (s == NULL || s->out == NULL || s->error)
        return -1;

    if (!s->in_graphics)
        return 0;

    if (put_command(s, OKG1_EXIT_GRAPHICS) != 0)
        return -1;

    s->in_graphics = 0;
    return 0;
}

int
okg1_job_end(okg1_stream *s, int form_feed)
{
    if (s == NULL || s->out == NULL || s->error)
        return -1;

    if (okg1_graphics_end(s) != 0)
        return -1;

    if (form_feed && put_byte(s, OKG1_FF) != 0)
        return -1;

    if (fflush(s->out) != 0) {
        s->error = 1;
        return -1;
    }

    return 0;
}
