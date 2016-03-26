#ifndef hx_frame_h
#define hx_frame_h

#include <nghttp2/nghttp2.h>
#include "nghttp2_hd.h"
#include "nghttp2_buf.h"
#include "nghttp2_frame.h"

struct hx_nghttp2_dummy {
    /**
     * THe frame header.
     */
    nghttp2_frame_hd hd;

    /**
     * Dummy payload. Must skip it.
     */
};

/**
 * Packs DUMMY frame |frame| in wire format and store it in |bufs|.
 * This function fill the current buffer to the full.
 *
 * The caller must take sure that there is at least one frame in the
 * current buffer before calling this function. Moreover, this function
 * must be called after padding.
 *
 * frame->hd.length must be assigned before packing process.
 *
 * This function returns 0 if it succeeds, or returns one of the
 * following negative error codes:
 * 
 * NGHTTP2_ERR_NOMEM
 *     Out of memory.
 */
int hx_nghttp2_frame_pack_dummy(nghttp2_bufs *bufs, hx_nghttp2_dummy *frame);

/**
 * Unpacks DUMMY frame byte sequence into |frame|.
 */
void hx_nghttp2_frame_unpack_dummy_payload(hx_nghttp2_dummy *frame,
                                          const uint8_t *payload,
                                          size_t payloadlen);

/**
 * Initializes DUMMY frame |frame| with given values. If |stream_id| is not
 * assigned yet, it must be -1.
 */
void hx_nghttp2_frame_dummy_init(hx_nghttp2_dummy *frame, int32_t flags,
                                 int32_t stream_id);

void hx_nghttp2_frame_dummy_free(hx_nghttp2_dummy *frame);

#endif /* hx_frame_h */
