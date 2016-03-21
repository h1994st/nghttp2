#include "hx_random.h"

#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>

#include "nghttp2_int.h"

uint8_t hx_random_initialized = 0; // Default: 0

void hx_random_init() {
    if (hx_random_initialized) return;

    unsigned int seed = (unsigned int)time(NULL);
    srand(seed);
    DEBUGF(fprintf(stderr, "[h1994st] random module initialized, seed=%u\n", seed));
}

void hx_normal_dist_new(hx_normal_distribution **dist_ptr, double mean, double sigma, nghttp2_mem *mem) {
    assert(hx_random_initialized);

    *dist_ptr = nghttp2_mem_malloc(mem, sizeof(hx_normal_distribution));
    (*dist_ptr)->mean = mean;
    (*dist_ptr)->sd = sigma;
    (*dist_ptr)->X1 = 0;
    (*dist_ptr)->X2 = 0;
    (*dist_ptr)->call = 0;

    DEBUGF(fprintf(stderr, "[h1994st] Creating normal distribution %p, mean=%lf, sd=%lf\n", *dist_ptr, mean, sigma));
}

void hx_normal_dist_del(hx_normal_distribution *dist, nghttp2_mem *mem) {
    assert(hx_random_initialized);

    if (dist == NULL) return;

    DEBUGF(fprintf(stderr, "[h1994st] Destroying normal distribution %p\n", dist));

    nghttp2_mem_free(mem, dist);
}

double hx_randn(hx_normal_distribution *dist) {
    assert(hx_random_initialized);

    double U1, Y2, W, mult;

    if (dist->call) {
        dist->call = 0;
        return (dist->mean + dist->sd * dist->X2);
    }

    do {
        U1 = -1 + ((double)rand() / RAND_MAX) * 2;
        U2 = -1 + ((double)rand() / RAND_MAX) * 2;
        W = pow(U1, 2) + pow(U2, 2);
    } while (W >= 1 || W == 0);

    mult = sqrt((-2 * log(W)) / W);
    dist->X1 = U1 * mult;
    dist->X2 = U2 * mult;

    dist->call = 1;

    return (dist->mean + dist->sd * dist->X1);
}

int32_t hx_rand(int32_t min, int32_t max) {
    assert(hx_ranom_initialized);

    if (min >= max) return min;

    return (int32_t)rand() % (max - min + 1) + min;
}

