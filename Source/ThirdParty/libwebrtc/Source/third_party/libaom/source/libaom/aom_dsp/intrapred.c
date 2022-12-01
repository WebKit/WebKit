/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <math.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/intrapred_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/bitops.h"

static INLINE void v_predictor(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                               const uint8_t *above, const uint8_t *left) {
  int r;
  (void)left;

  for (r = 0; r < bh; r++) {
    memcpy(dst, above, bw);
    dst += stride;
  }
}

static INLINE void h_predictor(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                               const uint8_t *above, const uint8_t *left) {
  int r;
  (void)above;

  for (r = 0; r < bh; r++) {
    memset(dst, left[r], bw);
    dst += stride;
  }
}

static INLINE int abs_diff(int a, int b) { return (a > b) ? a - b : b - a; }

static INLINE uint16_t paeth_predictor_single(uint16_t left, uint16_t top,
                                              uint16_t top_left) {
  const int base = top + left - top_left;
  const int p_left = abs_diff(base, left);
  const int p_top = abs_diff(base, top);
  const int p_top_left = abs_diff(base, top_left);

  // Return nearest to base of left, top and top_left.
  return (p_left <= p_top && p_left <= p_top_left)
             ? left
             : (p_top <= p_top_left) ? top : top_left;
}

static INLINE void paeth_predictor(uint8_t *dst, ptrdiff_t stride, int bw,
                                   int bh, const uint8_t *above,
                                   const uint8_t *left) {
  int r, c;
  const uint8_t ytop_left = above[-1];

  for (r = 0; r < bh; r++) {
    for (c = 0; c < bw; c++)
      dst[c] = (uint8_t)paeth_predictor_single(left[r], above[c], ytop_left);
    dst += stride;
  }
}

// Some basic checks on weights for smooth predictor.
#define sm_weights_sanity_checks(weights_w, weights_h, weights_scale, \
                                 pred_scale)                          \
  assert(weights_w[0] < weights_scale);                               \
  assert(weights_h[0] < weights_scale);                               \
  assert(weights_scale - weights_w[bw - 1] < weights_scale);          \
  assert(weights_scale - weights_h[bh - 1] < weights_scale);          \
  assert(pred_scale < 31)  // ensures no overflow when calculating predictor.

#define divide_round(value, bits) (((value) + (1 << ((bits)-1))) >> (bits))

static INLINE void smooth_predictor(uint8_t *dst, ptrdiff_t stride, int bw,
                                    int bh, const uint8_t *above,
                                    const uint8_t *left) {
  const uint8_t below_pred = left[bh - 1];   // estimated by bottom-left pixel
  const uint8_t right_pred = above[bw - 1];  // estimated by top-right pixel
  const uint8_t *const sm_weights_w = smooth_weights + bw - 4;
  const uint8_t *const sm_weights_h = smooth_weights + bh - 4;
  // scale = 2 * 2^SMOOTH_WEIGHT_LOG2_SCALE
  const int log2_scale = 1 + SMOOTH_WEIGHT_LOG2_SCALE;
  const uint16_t scale = (1 << SMOOTH_WEIGHT_LOG2_SCALE);
  sm_weights_sanity_checks(sm_weights_w, sm_weights_h, scale,
                           log2_scale + sizeof(*dst));
  int r;
  for (r = 0; r < bh; ++r) {
    int c;
    for (c = 0; c < bw; ++c) {
      const uint8_t pixels[] = { above[c], below_pred, left[r], right_pred };
      const uint8_t weights[] = { sm_weights_h[r], scale - sm_weights_h[r],
                                  sm_weights_w[c], scale - sm_weights_w[c] };
      uint32_t this_pred = 0;
      int i;
      assert(scale >= sm_weights_h[r] && scale >= sm_weights_w[c]);
      for (i = 0; i < 4; ++i) {
        this_pred += weights[i] * pixels[i];
      }
      dst[c] = divide_round(this_pred, log2_scale);
    }
    dst += stride;
  }
}

static INLINE void smooth_v_predictor(uint8_t *dst, ptrdiff_t stride, int bw,
                                      int bh, const uint8_t *above,
                                      const uint8_t *left) {
  const uint8_t below_pred = left[bh - 1];  // estimated by bottom-left pixel
  const uint8_t *const sm_weights = smooth_weights + bh - 4;
  // scale = 2^SMOOTH_WEIGHT_LOG2_SCALE
  const int log2_scale = SMOOTH_WEIGHT_LOG2_SCALE;
  const uint16_t scale = (1 << SMOOTH_WEIGHT_LOG2_SCALE);
  sm_weights_sanity_checks(sm_weights, sm_weights, scale,
                           log2_scale + sizeof(*dst));

  int r;
  for (r = 0; r < bh; r++) {
    int c;
    for (c = 0; c < bw; ++c) {
      const uint8_t pixels[] = { above[c], below_pred };
      const uint8_t weights[] = { sm_weights[r], scale - sm_weights[r] };
      uint32_t this_pred = 0;
      assert(scale >= sm_weights[r]);
      int i;
      for (i = 0; i < 2; ++i) {
        this_pred += weights[i] * pixels[i];
      }
      dst[c] = divide_round(this_pred, log2_scale);
    }
    dst += stride;
  }
}

static INLINE void smooth_h_predictor(uint8_t *dst, ptrdiff_t stride, int bw,
                                      int bh, const uint8_t *above,
                                      const uint8_t *left) {
  const uint8_t right_pred = above[bw - 1];  // estimated by top-right pixel
  const uint8_t *const sm_weights = smooth_weights + bw - 4;
  // scale = 2^SMOOTH_WEIGHT_LOG2_SCALE
  const int log2_scale = SMOOTH_WEIGHT_LOG2_SCALE;
  const uint16_t scale = (1 << SMOOTH_WEIGHT_LOG2_SCALE);
  sm_weights_sanity_checks(sm_weights, sm_weights, scale,
                           log2_scale + sizeof(*dst));

  int r;
  for (r = 0; r < bh; r++) {
    int c;
    for (c = 0; c < bw; ++c) {
      const uint8_t pixels[] = { left[r], right_pred };
      const uint8_t weights[] = { sm_weights[c], scale - sm_weights[c] };
      uint32_t this_pred = 0;
      assert(scale >= sm_weights[c]);
      int i;
      for (i = 0; i < 2; ++i) {
        this_pred += weights[i] * pixels[i];
      }
      dst[c] = divide_round(this_pred, log2_scale);
    }
    dst += stride;
  }
}

static INLINE void dc_128_predictor(uint8_t *dst, ptrdiff_t stride, int bw,
                                    int bh, const uint8_t *above,
                                    const uint8_t *left) {
  int r;
  (void)above;
  (void)left;

  for (r = 0; r < bh; r++) {
    memset(dst, 128, bw);
    dst += stride;
  }
}

static INLINE void dc_left_predictor(uint8_t *dst, ptrdiff_t stride, int bw,
                                     int bh, const uint8_t *above,
                                     const uint8_t *left) {
  int i, r, expected_dc, sum = 0;
  (void)above;

  for (i = 0; i < bh; i++) sum += left[i];
  expected_dc = (sum + (bh >> 1)) / bh;

  for (r = 0; r < bh; r++) {
    memset(dst, expected_dc, bw);
    dst += stride;
  }
}

static INLINE void dc_top_predictor(uint8_t *dst, ptrdiff_t stride, int bw,
                                    int bh, const uint8_t *above,
                                    const uint8_t *left) {
  int i, r, expected_dc, sum = 0;
  (void)left;

  for (i = 0; i < bw; i++) sum += above[i];
  expected_dc = (sum + (bw >> 1)) / bw;

  for (r = 0; r < bh; r++) {
    memset(dst, expected_dc, bw);
    dst += stride;
  }
}

static INLINE void dc_predictor(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                                const uint8_t *above, const uint8_t *left) {
  int i, r, expected_dc, sum = 0;
  const int count = bw + bh;

  for (i = 0; i < bw; i++) {
    sum += above[i];
  }
  for (i = 0; i < bh; i++) {
    sum += left[i];
  }

  expected_dc = (sum + (count >> 1)) / count;

  for (r = 0; r < bh; r++) {
    memset(dst, expected_dc, bw);
    dst += stride;
  }
}

static INLINE int divide_using_multiply_shift(int num, int shift1,
                                              int multiplier, int shift2) {
  const int interm = num >> shift1;
  return interm * multiplier >> shift2;
}

// The constants (multiplier and shifts) for a given block size are obtained
// as follows:
// - Let sum_w_h =  block width + block height.
// - Shift 'sum_w_h' right until we reach an odd number. Let the number of
// shifts for that block size be called 'shift1' (see the parameter in
// dc_predictor_rect() function), and let the odd number be 'd'. [d has only 2
// possible values: d = 3 for a 1:2 rect block and d = 5 for a 1:4 rect
// block].
// - Find multipliers for (i) dividing by 3, and (ii) dividing by 5,
// using the "Algorithm 1" in:
// http://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=1467632
// by ensuring that m + n = 16 (in that algorithm). This ensures that our 2nd
// shift will be 16, regardless of the block size.

// Note: For low bitdepth, assembly code may be optimized by using smaller
// constants for smaller block sizes, where the range of the 'sum' is
// restricted to fewer bits.

#define DC_MULTIPLIER_1X2 0x5556
#define DC_MULTIPLIER_1X4 0x3334

#define DC_SHIFT2 16

static INLINE void dc_predictor_rect(uint8_t *dst, ptrdiff_t stride, int bw,
                                     int bh, const uint8_t *above,
                                     const uint8_t *left, int shift1,
                                     int multiplier) {
  int sum = 0;

  for (int i = 0; i < bw; i++) {
    sum += above[i];
  }
  for (int i = 0; i < bh; i++) {
    sum += left[i];
  }

  const int expected_dc = divide_using_multiply_shift(
      sum + ((bw + bh) >> 1), shift1, multiplier, DC_SHIFT2);
  assert(expected_dc < (1 << 8));

  for (int r = 0; r < bh; r++) {
    memset(dst, expected_dc, bw);
    dst += stride;
  }
}

#undef DC_SHIFT2

void aom_dc_predictor_4x8_c(uint8_t *dst, ptrdiff_t stride,
                            const uint8_t *above, const uint8_t *left) {
  dc_predictor_rect(dst, stride, 4, 8, above, left, 2, DC_MULTIPLIER_1X2);
}

void aom_dc_predictor_8x4_c(uint8_t *dst, ptrdiff_t stride,
                            const uint8_t *above, const uint8_t *left) {
  dc_predictor_rect(dst, stride, 8, 4, above, left, 2, DC_MULTIPLIER_1X2);
}

void aom_dc_predictor_4x16_c(uint8_t *dst, ptrdiff_t stride,
                             const uint8_t *above, const uint8_t *left) {
  dc_predictor_rect(dst, stride, 4, 16, above, left, 2, DC_MULTIPLIER_1X4);
}

void aom_dc_predictor_16x4_c(uint8_t *dst, ptrdiff_t stride,
                             const uint8_t *above, const uint8_t *left) {
  dc_predictor_rect(dst, stride, 16, 4, above, left, 2, DC_MULTIPLIER_1X4);
}

void aom_dc_predictor_8x16_c(uint8_t *dst, ptrdiff_t stride,
                             const uint8_t *above, const uint8_t *left) {
  dc_predictor_rect(dst, stride, 8, 16, above, left, 3, DC_MULTIPLIER_1X2);
}

void aom_dc_predictor_16x8_c(uint8_t *dst, ptrdiff_t stride,
                             const uint8_t *above, const uint8_t *left) {
  dc_predictor_rect(dst, stride, 16, 8, above, left, 3, DC_MULTIPLIER_1X2);
}

void aom_dc_predictor_8x32_c(uint8_t *dst, ptrdiff_t stride,
                             const uint8_t *above, const uint8_t *left) {
  dc_predictor_rect(dst, stride, 8, 32, above, left, 3, DC_MULTIPLIER_1X4);
}

void aom_dc_predictor_32x8_c(uint8_t *dst, ptrdiff_t stride,
                             const uint8_t *above, const uint8_t *left) {
  dc_predictor_rect(dst, stride, 32, 8, above, left, 3, DC_MULTIPLIER_1X4);
}

void aom_dc_predictor_16x32_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  dc_predictor_rect(dst, stride, 16, 32, above, left, 4, DC_MULTIPLIER_1X2);
}

void aom_dc_predictor_32x16_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  dc_predictor_rect(dst, stride, 32, 16, above, left, 4, DC_MULTIPLIER_1X2);
}

void aom_dc_predictor_16x64_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  dc_predictor_rect(dst, stride, 16, 64, above, left, 4, DC_MULTIPLIER_1X4);
}

void aom_dc_predictor_64x16_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  dc_predictor_rect(dst, stride, 64, 16, above, left, 4, DC_MULTIPLIER_1X4);
}

void aom_dc_predictor_32x64_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  dc_predictor_rect(dst, stride, 32, 64, above, left, 5, DC_MULTIPLIER_1X2);
}

void aom_dc_predictor_64x32_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  dc_predictor_rect(dst, stride, 64, 32, above, left, 5, DC_MULTIPLIER_1X2);
}

#undef DC_MULTIPLIER_1X2
#undef DC_MULTIPLIER_1X4

static INLINE void highbd_v_predictor(uint16_t *dst, ptrdiff_t stride, int bw,
                                      int bh, const uint16_t *above,
                                      const uint16_t *left, int bd) {
  int r;
  (void)left;
  (void)bd;
  for (r = 0; r < bh; r++) {
    memcpy(dst, above, bw * sizeof(uint16_t));
    dst += stride;
  }
}

static INLINE void highbd_h_predictor(uint16_t *dst, ptrdiff_t stride, int bw,
                                      int bh, const uint16_t *above,
                                      const uint16_t *left, int bd) {
  int r;
  (void)above;
  (void)bd;
  for (r = 0; r < bh; r++) {
    aom_memset16(dst, left[r], bw);
    dst += stride;
  }
}

static INLINE void highbd_paeth_predictor(uint16_t *dst, ptrdiff_t stride,
                                          int bw, int bh, const uint16_t *above,
                                          const uint16_t *left, int bd) {
  int r, c;
  const uint16_t ytop_left = above[-1];
  (void)bd;

  for (r = 0; r < bh; r++) {
    for (c = 0; c < bw; c++)
      dst[c] = paeth_predictor_single(left[r], above[c], ytop_left);
    dst += stride;
  }
}

static INLINE void highbd_smooth_predictor(uint16_t *dst, ptrdiff_t stride,
                                           int bw, int bh,
                                           const uint16_t *above,
                                           const uint16_t *left, int bd) {
  (void)bd;
  const uint16_t below_pred = left[bh - 1];   // estimated by bottom-left pixel
  const uint16_t right_pred = above[bw - 1];  // estimated by top-right pixel
  const uint8_t *const sm_weights_w = smooth_weights + bw - 4;
  const uint8_t *const sm_weights_h = smooth_weights + bh - 4;
  // scale = 2 * 2^SMOOTH_WEIGHT_LOG2_SCALE
  const int log2_scale = 1 + SMOOTH_WEIGHT_LOG2_SCALE;
  const uint16_t scale = (1 << SMOOTH_WEIGHT_LOG2_SCALE);
  sm_weights_sanity_checks(sm_weights_w, sm_weights_h, scale,
                           log2_scale + sizeof(*dst));
  int r;
  for (r = 0; r < bh; ++r) {
    int c;
    for (c = 0; c < bw; ++c) {
      const uint16_t pixels[] = { above[c], below_pred, left[r], right_pred };
      const uint8_t weights[] = { sm_weights_h[r], scale - sm_weights_h[r],
                                  sm_weights_w[c], scale - sm_weights_w[c] };
      uint32_t this_pred = 0;
      int i;
      assert(scale >= sm_weights_h[r] && scale >= sm_weights_w[c]);
      for (i = 0; i < 4; ++i) {
        this_pred += weights[i] * pixels[i];
      }
      dst[c] = divide_round(this_pred, log2_scale);
    }
    dst += stride;
  }
}

static INLINE void highbd_smooth_v_predictor(uint16_t *dst, ptrdiff_t stride,
                                             int bw, int bh,
                                             const uint16_t *above,
                                             const uint16_t *left, int bd) {
  (void)bd;
  const uint16_t below_pred = left[bh - 1];  // estimated by bottom-left pixel
  const uint8_t *const sm_weights = smooth_weights + bh - 4;
  // scale = 2^SMOOTH_WEIGHT_LOG2_SCALE
  const int log2_scale = SMOOTH_WEIGHT_LOG2_SCALE;
  const uint16_t scale = (1 << SMOOTH_WEIGHT_LOG2_SCALE);
  sm_weights_sanity_checks(sm_weights, sm_weights, scale,
                           log2_scale + sizeof(*dst));

  int r;
  for (r = 0; r < bh; r++) {
    int c;
    for (c = 0; c < bw; ++c) {
      const uint16_t pixels[] = { above[c], below_pred };
      const uint8_t weights[] = { sm_weights[r], scale - sm_weights[r] };
      uint32_t this_pred = 0;
      assert(scale >= sm_weights[r]);
      int i;
      for (i = 0; i < 2; ++i) {
        this_pred += weights[i] * pixels[i];
      }
      dst[c] = divide_round(this_pred, log2_scale);
    }
    dst += stride;
  }
}

static INLINE void highbd_smooth_h_predictor(uint16_t *dst, ptrdiff_t stride,
                                             int bw, int bh,
                                             const uint16_t *above,
                                             const uint16_t *left, int bd) {
  (void)bd;
  const uint16_t right_pred = above[bw - 1];  // estimated by top-right pixel
  const uint8_t *const sm_weights = smooth_weights + bw - 4;
  // scale = 2^SMOOTH_WEIGHT_LOG2_SCALE
  const int log2_scale = SMOOTH_WEIGHT_LOG2_SCALE;
  const uint16_t scale = (1 << SMOOTH_WEIGHT_LOG2_SCALE);
  sm_weights_sanity_checks(sm_weights, sm_weights, scale,
                           log2_scale + sizeof(*dst));

  int r;
  for (r = 0; r < bh; r++) {
    int c;
    for (c = 0; c < bw; ++c) {
      const uint16_t pixels[] = { left[r], right_pred };
      const uint8_t weights[] = { sm_weights[c], scale - sm_weights[c] };
      uint32_t this_pred = 0;
      assert(scale >= sm_weights[c]);
      int i;
      for (i = 0; i < 2; ++i) {
        this_pred += weights[i] * pixels[i];
      }
      dst[c] = divide_round(this_pred, log2_scale);
    }
    dst += stride;
  }
}

static INLINE void highbd_dc_128_predictor(uint16_t *dst, ptrdiff_t stride,
                                           int bw, int bh,
                                           const uint16_t *above,
                                           const uint16_t *left, int bd) {
  int r;
  (void)above;
  (void)left;

  for (r = 0; r < bh; r++) {
    aom_memset16(dst, 128 << (bd - 8), bw);
    dst += stride;
  }
}

static INLINE void highbd_dc_left_predictor(uint16_t *dst, ptrdiff_t stride,
                                            int bw, int bh,
                                            const uint16_t *above,
                                            const uint16_t *left, int bd) {
  int i, r, expected_dc, sum = 0;
  (void)above;
  (void)bd;

  for (i = 0; i < bh; i++) sum += left[i];
  expected_dc = (sum + (bh >> 1)) / bh;

  for (r = 0; r < bh; r++) {
    aom_memset16(dst, expected_dc, bw);
    dst += stride;
  }
}

static INLINE void highbd_dc_top_predictor(uint16_t *dst, ptrdiff_t stride,
                                           int bw, int bh,
                                           const uint16_t *above,
                                           const uint16_t *left, int bd) {
  int i, r, expected_dc, sum = 0;
  (void)left;
  (void)bd;

  for (i = 0; i < bw; i++) sum += above[i];
  expected_dc = (sum + (bw >> 1)) / bw;

  for (r = 0; r < bh; r++) {
    aom_memset16(dst, expected_dc, bw);
    dst += stride;
  }
}

static INLINE void highbd_dc_predictor(uint16_t *dst, ptrdiff_t stride, int bw,
                                       int bh, const uint16_t *above,
                                       const uint16_t *left, int bd) {
  int i, r, expected_dc, sum = 0;
  const int count = bw + bh;
  (void)bd;

  for (i = 0; i < bw; i++) {
    sum += above[i];
  }
  for (i = 0; i < bh; i++) {
    sum += left[i];
  }

  expected_dc = (sum + (count >> 1)) / count;

  for (r = 0; r < bh; r++) {
    aom_memset16(dst, expected_dc, bw);
    dst += stride;
  }
}

// Obtained similarly as DC_MULTIPLIER_1X2 and DC_MULTIPLIER_1X4 above, but
// assume 2nd shift of 17 bits instead of 16.
// Note: Strictly speaking, 2nd shift needs to be 17 only when:
// - bit depth == 12, and
// - bw + bh is divisible by 5 (as opposed to divisible by 3).
// All other cases can use half the multipliers with a shift of 16 instead.
// This special optimization can be used when writing assembly code.
#define HIGHBD_DC_MULTIPLIER_1X2 0xAAAB
// Note: This constant is odd, but a smaller even constant (0x199a) with the
// appropriate shift should work for neon in 8/10-bit.
#define HIGHBD_DC_MULTIPLIER_1X4 0x6667

#define HIGHBD_DC_SHIFT2 17

static INLINE void highbd_dc_predictor_rect(uint16_t *dst, ptrdiff_t stride,
                                            int bw, int bh,
                                            const uint16_t *above,
                                            const uint16_t *left, int bd,
                                            int shift1, uint32_t multiplier) {
  int sum = 0;
  (void)bd;

  for (int i = 0; i < bw; i++) {
    sum += above[i];
  }
  for (int i = 0; i < bh; i++) {
    sum += left[i];
  }

  const int expected_dc = divide_using_multiply_shift(
      sum + ((bw + bh) >> 1), shift1, multiplier, HIGHBD_DC_SHIFT2);
  assert(expected_dc < (1 << bd));

  for (int r = 0; r < bh; r++) {
    aom_memset16(dst, expected_dc, bw);
    dst += stride;
  }
}

#undef HIGHBD_DC_SHIFT2

void aom_highbd_dc_predictor_4x8_c(uint16_t *dst, ptrdiff_t stride,
                                   const uint16_t *above, const uint16_t *left,
                                   int bd) {
  highbd_dc_predictor_rect(dst, stride, 4, 8, above, left, bd, 2,
                           HIGHBD_DC_MULTIPLIER_1X2);
}

void aom_highbd_dc_predictor_8x4_c(uint16_t *dst, ptrdiff_t stride,
                                   const uint16_t *above, const uint16_t *left,
                                   int bd) {
  highbd_dc_predictor_rect(dst, stride, 8, 4, above, left, bd, 2,
                           HIGHBD_DC_MULTIPLIER_1X2);
}

void aom_highbd_dc_predictor_4x16_c(uint16_t *dst, ptrdiff_t stride,
                                    const uint16_t *above, const uint16_t *left,
                                    int bd) {
  highbd_dc_predictor_rect(dst, stride, 4, 16, above, left, bd, 2,
                           HIGHBD_DC_MULTIPLIER_1X4);
}

void aom_highbd_dc_predictor_16x4_c(uint16_t *dst, ptrdiff_t stride,
                                    const uint16_t *above, const uint16_t *left,
                                    int bd) {
  highbd_dc_predictor_rect(dst, stride, 16, 4, above, left, bd, 2,
                           HIGHBD_DC_MULTIPLIER_1X4);
}

void aom_highbd_dc_predictor_8x16_c(uint16_t *dst, ptrdiff_t stride,
                                    const uint16_t *above, const uint16_t *left,
                                    int bd) {
  highbd_dc_predictor_rect(dst, stride, 8, 16, above, left, bd, 3,
                           HIGHBD_DC_MULTIPLIER_1X2);
}

void aom_highbd_dc_predictor_16x8_c(uint16_t *dst, ptrdiff_t stride,
                                    const uint16_t *above, const uint16_t *left,
                                    int bd) {
  highbd_dc_predictor_rect(dst, stride, 16, 8, above, left, bd, 3,
                           HIGHBD_DC_MULTIPLIER_1X2);
}

void aom_highbd_dc_predictor_8x32_c(uint16_t *dst, ptrdiff_t stride,
                                    const uint16_t *above, const uint16_t *left,
                                    int bd) {
  highbd_dc_predictor_rect(dst, stride, 8, 32, above, left, bd, 3,
                           HIGHBD_DC_MULTIPLIER_1X4);
}

void aom_highbd_dc_predictor_32x8_c(uint16_t *dst, ptrdiff_t stride,
                                    const uint16_t *above, const uint16_t *left,
                                    int bd) {
  highbd_dc_predictor_rect(dst, stride, 32, 8, above, left, bd, 3,
                           HIGHBD_DC_MULTIPLIER_1X4);
}

void aom_highbd_dc_predictor_16x32_c(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *above,
                                     const uint16_t *left, int bd) {
  highbd_dc_predictor_rect(dst, stride, 16, 32, above, left, bd, 4,
                           HIGHBD_DC_MULTIPLIER_1X2);
}

void aom_highbd_dc_predictor_32x16_c(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *above,
                                     const uint16_t *left, int bd) {
  highbd_dc_predictor_rect(dst, stride, 32, 16, above, left, bd, 4,
                           HIGHBD_DC_MULTIPLIER_1X2);
}

void aom_highbd_dc_predictor_16x64_c(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *above,
                                     const uint16_t *left, int bd) {
  highbd_dc_predictor_rect(dst, stride, 16, 64, above, left, bd, 4,
                           HIGHBD_DC_MULTIPLIER_1X4);
}

void aom_highbd_dc_predictor_64x16_c(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *above,
                                     const uint16_t *left, int bd) {
  highbd_dc_predictor_rect(dst, stride, 64, 16, above, left, bd, 4,
                           HIGHBD_DC_MULTIPLIER_1X4);
}

void aom_highbd_dc_predictor_32x64_c(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *above,
                                     const uint16_t *left, int bd) {
  highbd_dc_predictor_rect(dst, stride, 32, 64, above, left, bd, 5,
                           HIGHBD_DC_MULTIPLIER_1X2);
}

void aom_highbd_dc_predictor_64x32_c(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *above,
                                     const uint16_t *left, int bd) {
  highbd_dc_predictor_rect(dst, stride, 64, 32, above, left, bd, 5,
                           HIGHBD_DC_MULTIPLIER_1X2);
}

#undef HIGHBD_DC_MULTIPLIER_1X2
#undef HIGHBD_DC_MULTIPLIER_1X4

// This serves as a wrapper function, so that all the prediction functions
// can be unified and accessed as a pointer array. Note that the boundary
// above and left are not necessarily used all the time.
#define intra_pred_sized(type, width, height)                  \
  void aom_##type##_predictor_##width##x##height##_c(          \
      uint8_t *dst, ptrdiff_t stride, const uint8_t *above,    \
      const uint8_t *left) {                                   \
    type##_predictor(dst, stride, width, height, above, left); \
  }

#define intra_pred_highbd_sized(type, width, height)                        \
  void aom_highbd_##type##_predictor_##width##x##height##_c(                \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,               \
      const uint16_t *left, int bd) {                                       \
    highbd_##type##_predictor(dst, stride, width, height, above, left, bd); \
  }

/* clang-format off */
#define intra_pred_rectangular(type) \
  intra_pred_sized(type, 4, 8) \
  intra_pred_sized(type, 8, 4) \
  intra_pred_sized(type, 8, 16) \
  intra_pred_sized(type, 16, 8) \
  intra_pred_sized(type, 16, 32) \
  intra_pred_sized(type, 32, 16) \
  intra_pred_sized(type, 32, 64) \
  intra_pred_sized(type, 64, 32) \
  intra_pred_sized(type, 4, 16) \
  intra_pred_sized(type, 16, 4) \
  intra_pred_sized(type, 8, 32) \
  intra_pred_sized(type, 32, 8) \
  intra_pred_sized(type, 16, 64) \
  intra_pred_sized(type, 64, 16) \
  intra_pred_highbd_sized(type, 4, 8) \
  intra_pred_highbd_sized(type, 8, 4) \
  intra_pred_highbd_sized(type, 8, 16) \
  intra_pred_highbd_sized(type, 16, 8) \
  intra_pred_highbd_sized(type, 16, 32) \
  intra_pred_highbd_sized(type, 32, 16) \
  intra_pred_highbd_sized(type, 32, 64) \
  intra_pred_highbd_sized(type, 64, 32) \
  intra_pred_highbd_sized(type, 4, 16) \
  intra_pred_highbd_sized(type, 16, 4) \
  intra_pred_highbd_sized(type, 8, 32) \
  intra_pred_highbd_sized(type, 32, 8) \
  intra_pred_highbd_sized(type, 16, 64) \
  intra_pred_highbd_sized(type, 64, 16)

#define intra_pred_above_4x4(type) \
  intra_pred_sized(type, 8, 8) \
  intra_pred_sized(type, 16, 16) \
  intra_pred_sized(type, 32, 32) \
  intra_pred_sized(type, 64, 64) \
  intra_pred_highbd_sized(type, 4, 4) \
  intra_pred_highbd_sized(type, 8, 8) \
  intra_pred_highbd_sized(type, 16, 16) \
  intra_pred_highbd_sized(type, 32, 32) \
  intra_pred_highbd_sized(type, 64, 64) \
  intra_pred_rectangular(type)
#define intra_pred_allsizes(type) \
  intra_pred_sized(type, 4, 4) \
  intra_pred_above_4x4(type)
#define intra_pred_square(type) \
  intra_pred_sized(type, 4, 4) \
  intra_pred_sized(type, 8, 8) \
  intra_pred_sized(type, 16, 16) \
  intra_pred_sized(type, 32, 32) \
  intra_pred_sized(type, 64, 64) \
  intra_pred_highbd_sized(type, 4, 4) \
  intra_pred_highbd_sized(type, 8, 8) \
  intra_pred_highbd_sized(type, 16, 16) \
  intra_pred_highbd_sized(type, 32, 32) \
  intra_pred_highbd_sized(type, 64, 64)

intra_pred_allsizes(v)
intra_pred_allsizes(h)
intra_pred_allsizes(smooth)
intra_pred_allsizes(smooth_v)
intra_pred_allsizes(smooth_h)
intra_pred_allsizes(paeth)
intra_pred_allsizes(dc_128)
intra_pred_allsizes(dc_left)
intra_pred_allsizes(dc_top)
intra_pred_square(dc)
/* clang-format on */
#undef intra_pred_allsizes
