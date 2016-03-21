#ifndef hx_buf_h
#define hx_buf_h

#include "nghttp2_int.h"
#include "nghttp2_mem.h"
#include "hx_random.h"

#define HX_FRAME_PAYLOAD_LEN_MIN 36
#define HX_FRAME_PAYLOAD_LEN_MAX 1024

ssize_t hx_get_buf_chunk_length(hx_normal_distribution *dist);
void hx_nghttp2_buf_reset(nghttp2_buf *buf, hx_normal_distribution *dist);
void hx_nghttp2_bufs_init_buf_chunk_length_generator(nghttp2_bufs *bufs, nghttp2_mem *mem);
void hx_nghttp2_buf_resize(nghttp2_buf *buf, size_t new_cap);

#endif
