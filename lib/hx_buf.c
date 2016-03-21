#include "hx_buf.h"
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#include "nghttp2_helper.h"

ssize_t hx_get_buf_chunk_length(hx_normal_distribution *dist) {
    ssize_t chunk_len = 0;
    do {
        chunk_len = hx_randn(dist);
    } while (chunk_len < HX_FRAME_PAYLOAD_LEN_MIN || chunk_len > HX_FRAME_PAYLOAD_LEN_MAX);
    return chunk_len + 10; // 9 bytes for frame header, 1 byte for possible pad length field
}


void hx_nghttp2_buf_reset(nghttp2_buf *buf, hx_normal_distribution *dist) {
    buf->pos = buf->last = buf->mark = buf->begin;

    ssize_t actual_chunk_len = hx_get_buf_chunk_length(dist); 

    buf->end = buf->begin + actual_chunk_len;
    DEBUGF(fprintf(stderr, "[h1994st] Reset buffer %p, new chunk length=%ld\n", buf, actual_chunk_len));
}


void hx_nghttp2_bufs_init_buf_chunk_length_generator(nghttp2_bufs *bufs, nghttp2_mem *mem) {
    assert(bufs->random_enabled);
    if (bufs->buf_chunk_length_gen) return;

    double mean = hx_rand(HX_FRAME_PAYLOAD_LEN_MIN, HX_FRAME_PAYLOAD_LEN_MAX);
    double sd = hx_rand(100, nghttp2_max(mean - HX_FRAME_PAYLOAD_LEN_MIN, HX_FRAME_PAYLOAD_LEN_MAX - mean) / 3);
    hx_normal_dist_new(&(bufs->buf_chunk_length_gen), mean, sd, mem);

    DEBUGF(fprintf(stderr, "[h1994st] Initializing buffer chunk length generator for bufs %p\n", bufs));
}

void hx_nghttp2_buf_resize(nghttp2_buf *buf, size_t new_cap) {
    assert(new_cap >= HX_FRAME_PAYLOAD_LEN_MIN + 10); // Magic or 6 setting entries
    size_t cap;

    cap = nghttp2_buf_cap(buf);

    assert(new_cap <= cap);

    if (new_cap == cap) return;

    buf->end = buf->begin + new_cap;

    DEBUGF(fprintf(stderr, "[h1994st] Changing the size of buffer %p, %zu -> %zu\n", buf, cap, new_cap));
}

