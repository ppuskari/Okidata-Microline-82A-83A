#ifndef OKIGRAPH1_H
#define OKIGRAPH1_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * OkiGraph I native graphics stream helper for the Okidata MICROLINE
 * 82A/83A firmware family.
 *
 * Host graphics bytes use bits 0..6 for the seven exposed graphics pins.
 * Bit 0 is the top graphics pin. Bit 7 is set on the wire so that graphics
 * data cannot collide with the ETX command prefix.
 */

typedef struct okg1_stream {
    FILE *out;
    int in_graphics;
    int error;
} okg1_stream;

/* Begin a job from normal text mode. send_cancel != 0 emits CAN ($18). */
int okg1_job_begin(okg1_stream *s, FILE *out, int send_cancel);

/* Enter OkiGraph graphics mode by emitting ETX ($03). */
int okg1_graphics_begin(okg1_stream *s);

/*
 * Emit one logical seven-pin column.
 * dot_mask bit n == 1 means fire graphics pin n; only bits 0..6 are used.
 */
int okg1_write_column(okg1_stream *s, unsigned char dot_mask);

/* Emit an array of logical seven-pin columns. */
int okg1_write_columns(okg1_stream *s,
                       const unsigned char *dot_masks,
                       size_t count);

/* Native OkiGraph graphics feed + carriage return: ETX, $0E. */
int okg1_graphics_feed_cr(okg1_stream *s);

/* Exit graphics mode: ETX, $02. */
int okg1_graphics_end(okg1_stream *s);

/* Finish the job, optionally ejecting the page with FF ($0C). */
int okg1_job_end(okg1_stream *s, int form_feed);

#ifdef __cplusplus
}
#endif

#endif
