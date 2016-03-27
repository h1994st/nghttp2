#include "hx_frame.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "nghttp2_helper.h"
#include "nghttp2_net.h"

int hx_nghttp2_frame_pack_dummy(nghttp2_bufs *bufs, hx_nghttp2_dummy *frame) {
    nghttp2_buf *buf;

    buf = &bufs->cur->buf;

    /* Ensure there are other frames in the buffer */
    assert(buf->pos != buf-> last);

    /* 9 bytes of frame header, and the rest part is dummy content. */
    assert(nghttp2_buf_avail(buf) == frame->hd.length + NGHTTP2_FRAME_HDLEN);

    /* Write frame header to buffer (9 bytes). */
    nghttp2_frame_pack_frame_hd(buf->last, &frame->hd);

    buf->last += NGHTTP2_FRAME_HDLEN;

    assert(nghttp2_buf_avail(buf) == frame->hd.length);

    /**
     * Write dummy content. For security consideration, using memset in case 
     * memory leakage.
     */
    memset(buf->last, 1, nghttp2_buf_avail(buf));

    buf->last += nghttp2_buf_avail(buf);

    /* Ensure the buffer is full. */
    assert(buf->last == buf->end);

    return 0;
}

/**
 * Unpacks DUMMY frame byte sequence into |frame|.
 */
void hx_nghttp2_frame_unpack_dummy_payload(hx_nghttp2_dummy *frame _U_,
                                          const uint8_t *payload _U_,
                                          size_t payloadlen _U_) {}

void hx_nghttp2_frame_dummy_init(hx_nghttp2_dummy *frame, size_t payloadlen) {
    nghttp2_frame_hd_init(&frame->hd, payloadlen, HX_NGHTTP2_DUMMY,
                          NGHTTP2_FLAG_NONE, 0);
}

void hx_nghttp2_frame_dummy_free(hx_nghttp2_dummy *frame _U_) {}

