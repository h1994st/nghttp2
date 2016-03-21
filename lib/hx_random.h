#ifndef hx_nghttp2_h
#define hx_nghttp2_h

#include <stdint.h>
#include <math.h>

#include "nghttp2_int.h"
#include "nghttp2_mem.h"

typedef struct {
    double mean;
    double sd;

    double X1, X2;
    uint8_t call;
} hx_normal_distribution;

extern uint8_t hx_random_initialized;
void hx_random_init();

void hx_normal_dist_new(hx_normal_distribution **dist_ptr, double mean, double sigma, nghttp2_mem *mem);
void hx_normal_dist_del(hx_normal_distribution *dist, nghttp2_mem *mem);
double hx_randn(hx_normal_distribution *dist);

/**
 * Range: [min, max]
 */
int32_t hx_rand(int32_t min, int32_t max);

#endif
