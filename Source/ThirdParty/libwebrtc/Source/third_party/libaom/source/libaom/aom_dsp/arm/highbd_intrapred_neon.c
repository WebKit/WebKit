/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_dsp/intrapred_common.h"

// -----------------------------------------------------------------------------
// DC

static INLINE void highbd_dc_store_4xh(uint16_t *dst, ptrdiff_t stride, int h,
                                       uint16x4_t dc) {
  for (int i = 0; i < h; ++i) {
    vst1_u16(dst + i * stride, dc);
  }
}

static INLINE void highbd_dc_store_8xh(uint16_t *dst, ptrdiff_t stride, int h,
                                       uint16x8_t dc) {
  for (int i = 0; i < h; ++i) {
    vst1q_u16(dst + i * stride, dc);
  }
}

static INLINE void highbd_dc_store_16xh(uint16_t *dst, ptrdiff_t stride, int h,
                                        uint16x8_t dc) {
  for (int i = 0; i < h; ++i) {
    vst1q_u16(dst + i * stride, dc);
    vst1q_u16(dst + i * stride + 8, dc);
  }
}

static INLINE void highbd_dc_store_32xh(uint16_t *dst, ptrdiff_t stride, int h,
                                        uint16x8_t dc) {
  for (int i = 0; i < h; ++i) {
    vst1q_u16(dst + i * stride, dc);
    vst1q_u16(dst + i * stride + 8, dc);
    vst1q_u16(dst + i * stride + 16, dc);
    vst1q_u16(dst + i * stride + 24, dc);
  }
}

static INLINE void highbd_dc_store_64xh(uint16_t *dst, ptrdiff_t stride, int h,
                                        uint16x8_t dc) {
  for (int i = 0; i < h; ++i) {
    vst1q_u16(dst + i * stride, dc);
    vst1q_u16(dst + i * stride + 8, dc);
    vst1q_u16(dst + i * stride + 16, dc);
    vst1q_u16(dst + i * stride + 24, dc);
    vst1q_u16(dst + i * stride + 32, dc);
    vst1q_u16(dst + i * stride + 40, dc);
    vst1q_u16(dst + i * stride + 48, dc);
    vst1q_u16(dst + i * stride + 56, dc);
  }
}

static INLINE uint32x4_t horizontal_add_and_broadcast_long_u16x8(uint16x8_t a) {
  // Need to assume input is up to 16 bits wide from dc 64x64 partial sum, so
  // promote first.
  const uint32x4_t b = vpaddlq_u16(a);
#if AOM_ARCH_AARCH64
  const uint32x4_t c = vpaddq_u32(b, b);
  return vpaddq_u32(c, c);
#else
  const uint32x2_t c = vadd_u32(vget_low_u32(b), vget_high_u32(b));
  const uint32x2_t d = vpadd_u32(c, c);
  return vcombine_u32(d, d);
#endif
}

static INLINE uint16x8_t highbd_dc_load_partial_sum_4(const uint16_t *left) {
  // Nothing to do since sum is already one vector, but saves needing to
  // special case w=4 or h=4 cases. The combine will be zero cost for a sane
  // compiler since vld1 already sets the top half of a vector to zero as part
  // of the operation.
  return vcombine_u16(vld1_u16(left), vdup_n_u16(0));
}

static INLINE uint16x8_t highbd_dc_load_partial_sum_8(const uint16_t *left) {
  // Nothing to do since sum is already one vector, but saves needing to
  // special case w=8 or h=8 cases.
  return vld1q_u16(left);
}

static INLINE uint16x8_t highbd_dc_load_partial_sum_16(const uint16_t *left) {
  const uint16x8_t a0 = vld1q_u16(left + 0);  // up to 12 bits
  const uint16x8_t a1 = vld1q_u16(left + 8);
  return vaddq_u16(a0, a1);  // up to 13 bits
}

static INLINE uint16x8_t highbd_dc_load_partial_sum_32(const uint16_t *left) {
  const uint16x8_t a0 = vld1q_u16(left + 0);  // up to 12 bits
  const uint16x8_t a1 = vld1q_u16(left + 8);
  const uint16x8_t a2 = vld1q_u16(left + 16);
  const uint16x8_t a3 = vld1q_u16(left + 24);
  const uint16x8_t b0 = vaddq_u16(a0, a1);  // up to 13 bits
  const uint16x8_t b1 = vaddq_u16(a2, a3);
  return vaddq_u16(b0, b1);  // up to 14 bits
}

static INLINE uint16x8_t highbd_dc_load_partial_sum_64(const uint16_t *left) {
  const uint16x8_t a0 = vld1q_u16(left + 0);  // up to 12 bits
  const uint16x8_t a1 = vld1q_u16(left + 8);
  const uint16x8_t a2 = vld1q_u16(left + 16);
  const uint16x8_t a3 = vld1q_u16(left + 24);
  const uint16x8_t a4 = vld1q_u16(left + 32);
  const uint16x8_t a5 = vld1q_u16(left + 40);
  const uint16x8_t a6 = vld1q_u16(left + 48);
  const uint16x8_t a7 = vld1q_u16(left + 56);
  const uint16x8_t b0 = vaddq_u16(a0, a1);  // up to 13 bits
  const uint16x8_t b1 = vaddq_u16(a2, a3);
  const uint16x8_t b2 = vaddq_u16(a4, a5);
  const uint16x8_t b3 = vaddq_u16(a6, a7);
  const uint16x8_t c0 = vaddq_u16(b0, b1);  // up to 14 bits
  const uint16x8_t c1 = vaddq_u16(b2, b3);
  return vaddq_u16(c0, c1);  // up to 15 bits
}

#define HIGHBD_DC_PREDICTOR(w, h, shift)                               \
  void aom_highbd_dc_predictor_##w##x##h##_neon(                       \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,          \
      const uint16_t *left, int bd) {                                  \
    (void)bd;                                                          \
    const uint16x8_t a = highbd_dc_load_partial_sum_##w(above);        \
    const uint16x8_t l = highbd_dc_load_partial_sum_##h(left);         \
    const uint32x4_t sum =                                             \
        horizontal_add_and_broadcast_long_u16x8(vaddq_u16(a, l));      \
    const uint16x4_t dc0 = vrshrn_n_u32(sum, shift);                   \
    highbd_dc_store_##w##xh(dst, stride, (h), vdupq_lane_u16(dc0, 0)); \
  }

void aom_highbd_dc_predictor_4x4_neon(uint16_t *dst, ptrdiff_t stride,
                                      const uint16_t *above,
                                      const uint16_t *left, int bd) {
  // In the rectangular cases we simply extend the shorter vector to uint16x8
  // in order to accumulate, however in the 4x4 case there is no shorter vector
  // to extend so it is beneficial to do the whole calculation in uint16x4
  // instead.
  (void)bd;
  const uint16x4_t a = vld1_u16(above);  // up to 12 bits
  const uint16x4_t l = vld1_u16(left);
  uint16x4_t sum = vpadd_u16(a, l);  // up to 13 bits
  sum = vpadd_u16(sum, sum);         // up to 14 bits
  sum = vpadd_u16(sum, sum);
  const uint16x4_t dc = vrshr_n_u16(sum, 3);
  highbd_dc_store_4xh(dst, stride, 4, dc);
}

HIGHBD_DC_PREDICTOR(8, 8, 4)
HIGHBD_DC_PREDICTOR(16, 16, 5)
HIGHBD_DC_PREDICTOR(32, 32, 6)
HIGHBD_DC_PREDICTOR(64, 64, 7)

#undef HIGHBD_DC_PREDICTOR

static INLINE int divide_using_multiply_shift(int num, int shift1,
                                              int multiplier, int shift2) {
  const int interm = num >> shift1;
  return interm * multiplier >> shift2;
}

#define HIGHBD_DC_MULTIPLIER_1X2 0xAAAB
#define HIGHBD_DC_MULTIPLIER_1X4 0x6667
#define HIGHBD_DC_SHIFT2 17

static INLINE int highbd_dc_predictor_rect(int bw, int bh, int sum, int shift1,
                                           uint32_t multiplier) {
  return divide_using_multiply_shift(sum + ((bw + bh) >> 1), shift1, multiplier,
                                     HIGHBD_DC_SHIFT2);
}

#undef HIGHBD_DC_SHIFT2

#define HIGHBD_DC_PREDICTOR_RECT(w, h, q, shift, mult)                  \
  void aom_highbd_dc_predictor_##w##x##h##_neon(                        \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,           \
      const uint16_t *left, int bd) {                                   \
    (void)bd;                                                           \
    uint16x8_t sum_above = highbd_dc_load_partial_sum_##w(above);       \
    uint16x8_t sum_left = highbd_dc_load_partial_sum_##h(left);         \
    uint16x8_t sum_vec = vaddq_u16(sum_left, sum_above);                \
    int sum = horizontal_add_u16x8(sum_vec);                            \
    int dc0 = highbd_dc_predictor_rect((w), (h), sum, (shift), (mult)); \
    highbd_dc_store_##w##xh(dst, stride, (h), vdup##q##_n_u16(dc0));    \
  }

HIGHBD_DC_PREDICTOR_RECT(4, 8, , 2, HIGHBD_DC_MULTIPLIER_1X2)
HIGHBD_DC_PREDICTOR_RECT(4, 16, , 2, HIGHBD_DC_MULTIPLIER_1X4)
HIGHBD_DC_PREDICTOR_RECT(8, 4, q, 2, HIGHBD_DC_MULTIPLIER_1X2)
HIGHBD_DC_PREDICTOR_RECT(8, 16, q, 3, HIGHBD_DC_MULTIPLIER_1X2)
HIGHBD_DC_PREDICTOR_RECT(8, 32, q, 3, HIGHBD_DC_MULTIPLIER_1X4)
HIGHBD_DC_PREDICTOR_RECT(16, 4, q, 2, HIGHBD_DC_MULTIPLIER_1X4)
HIGHBD_DC_PREDICTOR_RECT(16, 8, q, 3, HIGHBD_DC_MULTIPLIER_1X2)
HIGHBD_DC_PREDICTOR_RECT(16, 32, q, 4, HIGHBD_DC_MULTIPLIER_1X2)
HIGHBD_DC_PREDICTOR_RECT(16, 64, q, 4, HIGHBD_DC_MULTIPLIER_1X4)
HIGHBD_DC_PREDICTOR_RECT(32, 8, q, 3, HIGHBD_DC_MULTIPLIER_1X4)
HIGHBD_DC_PREDICTOR_RECT(32, 16, q, 4, HIGHBD_DC_MULTIPLIER_1X2)
HIGHBD_DC_PREDICTOR_RECT(32, 64, q, 5, HIGHBD_DC_MULTIPLIER_1X2)
HIGHBD_DC_PREDICTOR_RECT(64, 16, q, 4, HIGHBD_DC_MULTIPLIER_1X4)
HIGHBD_DC_PREDICTOR_RECT(64, 32, q, 5, HIGHBD_DC_MULTIPLIER_1X2)

#undef HIGHBD_DC_PREDICTOR_RECT
#undef HIGHBD_DC_MULTIPLIER_1X2
#undef HIGHBD_DC_MULTIPLIER_1X4

// -----------------------------------------------------------------------------
// DC_128

#define HIGHBD_DC_PREDICTOR_128(w, h, q)                        \
  void aom_highbd_dc_128_predictor_##w##x##h##_neon(            \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,   \
      const uint16_t *left, int bd) {                           \
    (void)above;                                                \
    (void)bd;                                                   \
    (void)left;                                                 \
    highbd_dc_store_##w##xh(dst, stride, (h),                   \
                            vdup##q##_n_u16(0x80 << (bd - 8))); \
  }

HIGHBD_DC_PREDICTOR_128(4, 4, )
HIGHBD_DC_PREDICTOR_128(4, 8, )
HIGHBD_DC_PREDICTOR_128(4, 16, )
HIGHBD_DC_PREDICTOR_128(8, 4, q)
HIGHBD_DC_PREDICTOR_128(8, 8, q)
HIGHBD_DC_PREDICTOR_128(8, 16, q)
HIGHBD_DC_PREDICTOR_128(8, 32, q)
HIGHBD_DC_PREDICTOR_128(16, 4, q)
HIGHBD_DC_PREDICTOR_128(16, 8, q)
HIGHBD_DC_PREDICTOR_128(16, 16, q)
HIGHBD_DC_PREDICTOR_128(16, 32, q)
HIGHBD_DC_PREDICTOR_128(16, 64, q)
HIGHBD_DC_PREDICTOR_128(32, 8, q)
HIGHBD_DC_PREDICTOR_128(32, 16, q)
HIGHBD_DC_PREDICTOR_128(32, 32, q)
HIGHBD_DC_PREDICTOR_128(32, 64, q)
HIGHBD_DC_PREDICTOR_128(64, 16, q)
HIGHBD_DC_PREDICTOR_128(64, 32, q)
HIGHBD_DC_PREDICTOR_128(64, 64, q)

#undef HIGHBD_DC_PREDICTOR_128

// -----------------------------------------------------------------------------
// DC_LEFT

static INLINE uint32x4_t highbd_dc_load_sum_4(const uint16_t *left) {
  const uint16x4_t a = vld1_u16(left);   // up to 12 bits
  const uint16x4_t b = vpadd_u16(a, a);  // up to 13 bits
  return vcombine_u32(vpaddl_u16(b), vdup_n_u32(0));
}

static INLINE uint32x4_t highbd_dc_load_sum_8(const uint16_t *left) {
  return horizontal_add_and_broadcast_long_u16x8(vld1q_u16(left));
}

static INLINE uint32x4_t highbd_dc_load_sum_16(const uint16_t *left) {
  return horizontal_add_and_broadcast_long_u16x8(
      highbd_dc_load_partial_sum_16(left));
}

static INLINE uint32x4_t highbd_dc_load_sum_32(const uint16_t *left) {
  return horizontal_add_and_broadcast_long_u16x8(
      highbd_dc_load_partial_sum_32(left));
}

static INLINE uint32x4_t highbd_dc_load_sum_64(const uint16_t *left) {
  return horizontal_add_and_broadcast_long_u16x8(
      highbd_dc_load_partial_sum_64(left));
}

#define DC_PREDICTOR_LEFT(w, h, shift, q)                                  \
  void aom_highbd_dc_left_predictor_##w##x##h##_neon(                      \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,              \
      const uint16_t *left, int bd) {                                      \
    (void)above;                                                           \
    (void)bd;                                                              \
    const uint32x4_t sum = highbd_dc_load_sum_##h(left);                   \
    const uint16x4_t dc0 = vrshrn_n_u32(sum, (shift));                     \
    highbd_dc_store_##w##xh(dst, stride, (h), vdup##q##_lane_u16(dc0, 0)); \
  }

DC_PREDICTOR_LEFT(4, 4, 2, )
DC_PREDICTOR_LEFT(4, 8, 3, )
DC_PREDICTOR_LEFT(4, 16, 4, )
DC_PREDICTOR_LEFT(8, 4, 2, q)
DC_PREDICTOR_LEFT(8, 8, 3, q)
DC_PREDICTOR_LEFT(8, 16, 4, q)
DC_PREDICTOR_LEFT(8, 32, 5, q)
DC_PREDICTOR_LEFT(16, 4, 2, q)
DC_PREDICTOR_LEFT(16, 8, 3, q)
DC_PREDICTOR_LEFT(16, 16, 4, q)
DC_PREDICTOR_LEFT(16, 32, 5, q)
DC_PREDICTOR_LEFT(16, 64, 6, q)
DC_PREDICTOR_LEFT(32, 8, 3, q)
DC_PREDICTOR_LEFT(32, 16, 4, q)
DC_PREDICTOR_LEFT(32, 32, 5, q)
DC_PREDICTOR_LEFT(32, 64, 6, q)
DC_PREDICTOR_LEFT(64, 16, 4, q)
DC_PREDICTOR_LEFT(64, 32, 5, q)
DC_PREDICTOR_LEFT(64, 64, 6, q)

#undef DC_PREDICTOR_LEFT

// -----------------------------------------------------------------------------
// DC_TOP

#define DC_PREDICTOR_TOP(w, h, shift, q)                                   \
  void aom_highbd_dc_top_predictor_##w##x##h##_neon(                       \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,              \
      const uint16_t *left, int bd) {                                      \
    (void)bd;                                                              \
    (void)left;                                                            \
    const uint32x4_t sum = highbd_dc_load_sum_##w(above);                  \
    const uint16x4_t dc0 = vrshrn_n_u32(sum, (shift));                     \
    highbd_dc_store_##w##xh(dst, stride, (h), vdup##q##_lane_u16(dc0, 0)); \
  }

DC_PREDICTOR_TOP(4, 4, 2, )
DC_PREDICTOR_TOP(4, 8, 2, )
DC_PREDICTOR_TOP(4, 16, 2, )
DC_PREDICTOR_TOP(8, 4, 3, q)
DC_PREDICTOR_TOP(8, 8, 3, q)
DC_PREDICTOR_TOP(8, 16, 3, q)
DC_PREDICTOR_TOP(8, 32, 3, q)
DC_PREDICTOR_TOP(16, 4, 4, q)
DC_PREDICTOR_TOP(16, 8, 4, q)
DC_PREDICTOR_TOP(16, 16, 4, q)
DC_PREDICTOR_TOP(16, 32, 4, q)
DC_PREDICTOR_TOP(16, 64, 4, q)
DC_PREDICTOR_TOP(32, 8, 5, q)
DC_PREDICTOR_TOP(32, 16, 5, q)
DC_PREDICTOR_TOP(32, 32, 5, q)
DC_PREDICTOR_TOP(32, 64, 5, q)
DC_PREDICTOR_TOP(64, 16, 6, q)
DC_PREDICTOR_TOP(64, 32, 6, q)
DC_PREDICTOR_TOP(64, 64, 6, q)

#undef DC_PREDICTOR_TOP

// -----------------------------------------------------------------------------
// V_PRED

#define HIGHBD_V_NXM(W, H)                                    \
  void aom_highbd_v_predictor_##W##x##H##_neon(               \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above, \
      const uint16_t *left, int bd) {                         \
    (void)left;                                               \
    (void)bd;                                                 \
    vertical##W##xh_neon(dst, stride, above, H);              \
  }

static INLINE uint16x8x2_t load_uint16x8x2(uint16_t const *ptr) {
  uint16x8x2_t x;
  // Clang/gcc uses ldp here.
  x.val[0] = vld1q_u16(ptr);
  x.val[1] = vld1q_u16(ptr + 8);
  return x;
}

static INLINE void store_uint16x8x2(uint16_t *ptr, uint16x8x2_t x) {
  vst1q_u16(ptr, x.val[0]);
  vst1q_u16(ptr + 8, x.val[1]);
}

static INLINE void vertical4xh_neon(uint16_t *dst, ptrdiff_t stride,
                                    const uint16_t *const above, int height) {
  const uint16x4_t row = vld1_u16(above);
  int y = height;
  do {
    vst1_u16(dst, row);
    vst1_u16(dst + stride, row);
    dst += stride << 1;
    y -= 2;
  } while (y != 0);
}

static INLINE void vertical8xh_neon(uint16_t *dst, ptrdiff_t stride,
                                    const uint16_t *const above, int height) {
  const uint16x8_t row = vld1q_u16(above);
  int y = height;
  do {
    vst1q_u16(dst, row);
    vst1q_u16(dst + stride, row);
    dst += stride << 1;
    y -= 2;
  } while (y != 0);
}

static INLINE void vertical16xh_neon(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *const above, int height) {
  const uint16x8x2_t row = load_uint16x8x2(above);
  int y = height;
  do {
    store_uint16x8x2(dst, row);
    store_uint16x8x2(dst + stride, row);
    dst += stride << 1;
    y -= 2;
  } while (y != 0);
}

static INLINE uint16x8x4_t load_uint16x8x4(uint16_t const *ptr) {
  uint16x8x4_t x;
  // Clang/gcc uses ldp here.
  x.val[0] = vld1q_u16(ptr);
  x.val[1] = vld1q_u16(ptr + 8);
  x.val[2] = vld1q_u16(ptr + 16);
  x.val[3] = vld1q_u16(ptr + 24);
  return x;
}

static INLINE void store_uint16x8x4(uint16_t *ptr, uint16x8x4_t x) {
  vst1q_u16(ptr, x.val[0]);
  vst1q_u16(ptr + 8, x.val[1]);
  vst1q_u16(ptr + 16, x.val[2]);
  vst1q_u16(ptr + 24, x.val[3]);
}

static INLINE void vertical32xh_neon(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *const above, int height) {
  const uint16x8x4_t row = load_uint16x8x4(above);
  int y = height;
  do {
    store_uint16x8x4(dst, row);
    store_uint16x8x4(dst + stride, row);
    dst += stride << 1;
    y -= 2;
  } while (y != 0);
}

static INLINE void vertical64xh_neon(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *const above, int height) {
  uint16_t *dst32 = dst + 32;
  const uint16x8x4_t row = load_uint16x8x4(above);
  const uint16x8x4_t row32 = load_uint16x8x4(above + 32);
  int y = height;
  do {
    store_uint16x8x4(dst, row);
    store_uint16x8x4(dst32, row32);
    store_uint16x8x4(dst + stride, row);
    store_uint16x8x4(dst32 + stride, row32);
    dst += stride << 1;
    dst32 += stride << 1;
    y -= 2;
  } while (y != 0);
}

HIGHBD_V_NXM(4, 4)
HIGHBD_V_NXM(4, 8)
HIGHBD_V_NXM(4, 16)

HIGHBD_V_NXM(8, 4)
HIGHBD_V_NXM(8, 8)
HIGHBD_V_NXM(8, 16)
HIGHBD_V_NXM(8, 32)

HIGHBD_V_NXM(16, 4)
HIGHBD_V_NXM(16, 8)
HIGHBD_V_NXM(16, 16)
HIGHBD_V_NXM(16, 32)
HIGHBD_V_NXM(16, 64)

HIGHBD_V_NXM(32, 8)
HIGHBD_V_NXM(32, 16)
HIGHBD_V_NXM(32, 32)
HIGHBD_V_NXM(32, 64)

HIGHBD_V_NXM(64, 16)
HIGHBD_V_NXM(64, 32)
HIGHBD_V_NXM(64, 64)

// -----------------------------------------------------------------------------
// H_PRED

static INLINE void highbd_h_store_4x4(uint16_t *dst, ptrdiff_t stride,
                                      uint16x4_t left) {
  vst1_u16(dst + 0 * stride, vdup_lane_u16(left, 0));
  vst1_u16(dst + 1 * stride, vdup_lane_u16(left, 1));
  vst1_u16(dst + 2 * stride, vdup_lane_u16(left, 2));
  vst1_u16(dst + 3 * stride, vdup_lane_u16(left, 3));
}

static INLINE void highbd_h_store_8x4(uint16_t *dst, ptrdiff_t stride,
                                      uint16x4_t left) {
  vst1q_u16(dst + 0 * stride, vdupq_lane_u16(left, 0));
  vst1q_u16(dst + 1 * stride, vdupq_lane_u16(left, 1));
  vst1q_u16(dst + 2 * stride, vdupq_lane_u16(left, 2));
  vst1q_u16(dst + 3 * stride, vdupq_lane_u16(left, 3));
}

static INLINE void highbd_h_store_16x1(uint16_t *dst, uint16x8_t left) {
  vst1q_u16(dst + 0, left);
  vst1q_u16(dst + 8, left);
}

static INLINE void highbd_h_store_16x4(uint16_t *dst, ptrdiff_t stride,
                                       uint16x4_t left) {
  highbd_h_store_16x1(dst + 0 * stride, vdupq_lane_u16(left, 0));
  highbd_h_store_16x1(dst + 1 * stride, vdupq_lane_u16(left, 1));
  highbd_h_store_16x1(dst + 2 * stride, vdupq_lane_u16(left, 2));
  highbd_h_store_16x1(dst + 3 * stride, vdupq_lane_u16(left, 3));
}

static INLINE void highbd_h_store_32x1(uint16_t *dst, uint16x8_t left) {
  vst1q_u16(dst + 0, left);
  vst1q_u16(dst + 8, left);
  vst1q_u16(dst + 16, left);
  vst1q_u16(dst + 24, left);
}

static INLINE void highbd_h_store_32x4(uint16_t *dst, ptrdiff_t stride,
                                       uint16x4_t left) {
  highbd_h_store_32x1(dst + 0 * stride, vdupq_lane_u16(left, 0));
  highbd_h_store_32x1(dst + 1 * stride, vdupq_lane_u16(left, 1));
  highbd_h_store_32x1(dst + 2 * stride, vdupq_lane_u16(left, 2));
  highbd_h_store_32x1(dst + 3 * stride, vdupq_lane_u16(left, 3));
}

static INLINE void highbd_h_store_64x1(uint16_t *dst, uint16x8_t left) {
  vst1q_u16(dst + 0, left);
  vst1q_u16(dst + 8, left);
  vst1q_u16(dst + 16, left);
  vst1q_u16(dst + 24, left);
  vst1q_u16(dst + 32, left);
  vst1q_u16(dst + 40, left);
  vst1q_u16(dst + 48, left);
  vst1q_u16(dst + 56, left);
}

static INLINE void highbd_h_store_64x4(uint16_t *dst, ptrdiff_t stride,
                                       uint16x4_t left) {
  highbd_h_store_64x1(dst + 0 * stride, vdupq_lane_u16(left, 0));
  highbd_h_store_64x1(dst + 1 * stride, vdupq_lane_u16(left, 1));
  highbd_h_store_64x1(dst + 2 * stride, vdupq_lane_u16(left, 2));
  highbd_h_store_64x1(dst + 3 * stride, vdupq_lane_u16(left, 3));
}

void aom_highbd_h_predictor_4x4_neon(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *above,
                                     const uint16_t *left, int bd) {
  (void)above;
  (void)bd;
  highbd_h_store_4x4(dst, stride, vld1_u16(left));
}

void aom_highbd_h_predictor_4x8_neon(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *above,
                                     const uint16_t *left, int bd) {
  (void)above;
  (void)bd;
  uint16x8_t l = vld1q_u16(left);
  highbd_h_store_4x4(dst + 0 * stride, stride, vget_low_u16(l));
  highbd_h_store_4x4(dst + 4 * stride, stride, vget_high_u16(l));
}

void aom_highbd_h_predictor_8x4_neon(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *above,
                                     const uint16_t *left, int bd) {
  (void)above;
  (void)bd;
  highbd_h_store_8x4(dst, stride, vld1_u16(left));
}

void aom_highbd_h_predictor_8x8_neon(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *above,
                                     const uint16_t *left, int bd) {
  (void)above;
  (void)bd;
  uint16x8_t l = vld1q_u16(left);
  highbd_h_store_8x4(dst + 0 * stride, stride, vget_low_u16(l));
  highbd_h_store_8x4(dst + 4 * stride, stride, vget_high_u16(l));
}

void aom_highbd_h_predictor_16x4_neon(uint16_t *dst, ptrdiff_t stride,
                                      const uint16_t *above,
                                      const uint16_t *left, int bd) {
  (void)above;
  (void)bd;
  highbd_h_store_16x4(dst, stride, vld1_u16(left));
}

void aom_highbd_h_predictor_16x8_neon(uint16_t *dst, ptrdiff_t stride,
                                      const uint16_t *above,
                                      const uint16_t *left, int bd) {
  (void)above;
  (void)bd;
  uint16x8_t l = vld1q_u16(left);
  highbd_h_store_16x4(dst + 0 * stride, stride, vget_low_u16(l));
  highbd_h_store_16x4(dst + 4 * stride, stride, vget_high_u16(l));
}

void aom_highbd_h_predictor_32x8_neon(uint16_t *dst, ptrdiff_t stride,
                                      const uint16_t *above,
                                      const uint16_t *left, int bd) {
  (void)above;
  (void)bd;
  uint16x8_t l = vld1q_u16(left);
  highbd_h_store_32x4(dst + 0 * stride, stride, vget_low_u16(l));
  highbd_h_store_32x4(dst + 4 * stride, stride, vget_high_u16(l));
}

// For cases where height >= 16 we use pairs of loads to get LDP instructions.
#define HIGHBD_H_WXH_LARGE(w, h)                                            \
  void aom_highbd_h_predictor_##w##x##h##_neon(                             \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,               \
      const uint16_t *left, int bd) {                                       \
    (void)above;                                                            \
    (void)bd;                                                               \
    for (int i = 0; i < (h) / 16; ++i) {                                    \
      uint16x8_t l0 = vld1q_u16(left + 0);                                  \
      uint16x8_t l1 = vld1q_u16(left + 8);                                  \
      highbd_h_store_##w##x4(dst + 0 * stride, stride, vget_low_u16(l0));   \
      highbd_h_store_##w##x4(dst + 4 * stride, stride, vget_high_u16(l0));  \
      highbd_h_store_##w##x4(dst + 8 * stride, stride, vget_low_u16(l1));   \
      highbd_h_store_##w##x4(dst + 12 * stride, stride, vget_high_u16(l1)); \
      left += 16;                                                           \
      dst += 16 * stride;                                                   \
    }                                                                       \
  }

HIGHBD_H_WXH_LARGE(4, 16)
HIGHBD_H_WXH_LARGE(8, 16)
HIGHBD_H_WXH_LARGE(8, 32)
HIGHBD_H_WXH_LARGE(16, 16)
HIGHBD_H_WXH_LARGE(16, 32)
HIGHBD_H_WXH_LARGE(16, 64)
HIGHBD_H_WXH_LARGE(32, 16)
HIGHBD_H_WXH_LARGE(32, 32)
HIGHBD_H_WXH_LARGE(32, 64)
HIGHBD_H_WXH_LARGE(64, 16)
HIGHBD_H_WXH_LARGE(64, 32)
HIGHBD_H_WXH_LARGE(64, 64)

#undef HIGHBD_H_WXH_LARGE

// -----------------------------------------------------------------------------
// PAETH

static INLINE void highbd_paeth_4or8_x_h_neon(uint16_t *dest, ptrdiff_t stride,
                                              const uint16_t *const top_row,
                                              const uint16_t *const left_column,
                                              int width, int height) {
  const uint16x8_t top_left = vdupq_n_u16(top_row[-1]);
  const uint16x8_t top_left_x2 = vdupq_n_u16(top_row[-1] + top_row[-1]);
  uint16x8_t top;
  if (width == 4) {
    top = vcombine_u16(vld1_u16(top_row), vdup_n_u16(0));
  } else {  // width == 8
    top = vld1q_u16(top_row);
  }

  for (int y = 0; y < height; ++y) {
    const uint16x8_t left = vdupq_n_u16(left_column[y]);

    const uint16x8_t left_dist = vabdq_u16(top, top_left);
    const uint16x8_t top_dist = vabdq_u16(left, top_left);
    const uint16x8_t top_left_dist =
        vabdq_u16(vaddq_u16(top, left), top_left_x2);

    const uint16x8_t left_le_top = vcleq_u16(left_dist, top_dist);
    const uint16x8_t left_le_top_left = vcleq_u16(left_dist, top_left_dist);
    const uint16x8_t top_le_top_left = vcleq_u16(top_dist, top_left_dist);

    // if (left_dist <= top_dist && left_dist <= top_left_dist)
    const uint16x8_t left_mask = vandq_u16(left_le_top, left_le_top_left);
    //   dest[x] = left_column[y];
    // Fill all the unused spaces with 'top'. They will be overwritten when
    // the positions for top_left are known.
    uint16x8_t result = vbslq_u16(left_mask, left, top);
    // else if (top_dist <= top_left_dist)
    //   dest[x] = top_row[x];
    // Add these values to the mask. They were already set.
    const uint16x8_t left_or_top_mask = vorrq_u16(left_mask, top_le_top_left);
    // else
    //   dest[x] = top_left;
    result = vbslq_u16(left_or_top_mask, result, top_left);

    if (width == 4) {
      vst1_u16(dest, vget_low_u16(result));
    } else {  // width == 8
      vst1q_u16(dest, result);
    }
    dest += stride;
  }
}

#define HIGHBD_PAETH_NXM(W, H)                                  \
  void aom_highbd_paeth_predictor_##W##x##H##_neon(             \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,   \
      const uint16_t *left, int bd) {                           \
    (void)bd;                                                   \
    highbd_paeth_4or8_x_h_neon(dst, stride, above, left, W, H); \
  }

HIGHBD_PAETH_NXM(4, 4)
HIGHBD_PAETH_NXM(4, 8)
HIGHBD_PAETH_NXM(4, 16)
HIGHBD_PAETH_NXM(8, 4)
HIGHBD_PAETH_NXM(8, 8)
HIGHBD_PAETH_NXM(8, 16)
HIGHBD_PAETH_NXM(8, 32)

// Select the closest values and collect them.
static INLINE uint16x8_t select_paeth(const uint16x8_t top,
                                      const uint16x8_t left,
                                      const uint16x8_t top_left,
                                      const uint16x8_t left_le_top,
                                      const uint16x8_t left_le_top_left,
                                      const uint16x8_t top_le_top_left) {
  // if (left_dist <= top_dist && left_dist <= top_left_dist)
  const uint16x8_t left_mask = vandq_u16(left_le_top, left_le_top_left);
  //   dest[x] = left_column[y];
  // Fill all the unused spaces with 'top'. They will be overwritten when
  // the positions for top_left are known.
  const uint16x8_t result = vbslq_u16(left_mask, left, top);
  // else if (top_dist <= top_left_dist)
  //   dest[x] = top_row[x];
  // Add these values to the mask. They were already set.
  const uint16x8_t left_or_top_mask = vorrq_u16(left_mask, top_le_top_left);
  // else
  //   dest[x] = top_left;
  return vbslq_u16(left_or_top_mask, result, top_left);
}

#define PAETH_PREDICTOR(num)                                                  \
  do {                                                                        \
    const uint16x8_t left_dist = vabdq_u16(top[num], top_left);               \
    const uint16x8_t top_left_dist =                                          \
        vabdq_u16(vaddq_u16(top[num], left), top_left_x2);                    \
    const uint16x8_t left_le_top = vcleq_u16(left_dist, top_dist);            \
    const uint16x8_t left_le_top_left = vcleq_u16(left_dist, top_left_dist);  \
    const uint16x8_t top_le_top_left = vcleq_u16(top_dist, top_left_dist);    \
    const uint16x8_t result =                                                 \
        select_paeth(top[num], left, top_left, left_le_top, left_le_top_left, \
                     top_le_top_left);                                        \
    vst1q_u16(dest + (num * 8), result);                                      \
  } while (0)

#define LOAD_TOP_ROW(num) vld1q_u16(top_row + (num * 8))

static INLINE void highbd_paeth16_plus_x_h_neon(
    uint16_t *dest, ptrdiff_t stride, const uint16_t *const top_row,
    const uint16_t *const left_column, int width, int height) {
  const uint16x8_t top_left = vdupq_n_u16(top_row[-1]);
  const uint16x8_t top_left_x2 = vdupq_n_u16(top_row[-1] + top_row[-1]);
  uint16x8_t top[8];
  top[0] = LOAD_TOP_ROW(0);
  top[1] = LOAD_TOP_ROW(1);
  if (width > 16) {
    top[2] = LOAD_TOP_ROW(2);
    top[3] = LOAD_TOP_ROW(3);
    if (width == 64) {
      top[4] = LOAD_TOP_ROW(4);
      top[5] = LOAD_TOP_ROW(5);
      top[6] = LOAD_TOP_ROW(6);
      top[7] = LOAD_TOP_ROW(7);
    }
  }

  for (int y = 0; y < height; ++y) {
    const uint16x8_t left = vdupq_n_u16(left_column[y]);
    const uint16x8_t top_dist = vabdq_u16(left, top_left);
    PAETH_PREDICTOR(0);
    PAETH_PREDICTOR(1);
    if (width > 16) {
      PAETH_PREDICTOR(2);
      PAETH_PREDICTOR(3);
      if (width == 64) {
        PAETH_PREDICTOR(4);
        PAETH_PREDICTOR(5);
        PAETH_PREDICTOR(6);
        PAETH_PREDICTOR(7);
      }
    }
    dest += stride;
  }
}

#define HIGHBD_PAETH_NXM_WIDE(W, H)                               \
  void aom_highbd_paeth_predictor_##W##x##H##_neon(               \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,     \
      const uint16_t *left, int bd) {                             \
    (void)bd;                                                     \
    highbd_paeth16_plus_x_h_neon(dst, stride, above, left, W, H); \
  }

HIGHBD_PAETH_NXM_WIDE(16, 4)
HIGHBD_PAETH_NXM_WIDE(16, 8)
HIGHBD_PAETH_NXM_WIDE(16, 16)
HIGHBD_PAETH_NXM_WIDE(16, 32)
HIGHBD_PAETH_NXM_WIDE(16, 64)
HIGHBD_PAETH_NXM_WIDE(32, 8)
HIGHBD_PAETH_NXM_WIDE(32, 16)
HIGHBD_PAETH_NXM_WIDE(32, 32)
HIGHBD_PAETH_NXM_WIDE(32, 64)
HIGHBD_PAETH_NXM_WIDE(64, 16)
HIGHBD_PAETH_NXM_WIDE(64, 32)
HIGHBD_PAETH_NXM_WIDE(64, 64)

// -----------------------------------------------------------------------------
// SMOOTH

// 256 - v = vneg_s8(v)
static INLINE uint16x4_t negate_s8(const uint16x4_t v) {
  return vreinterpret_u16_s8(vneg_s8(vreinterpret_s8_u16(v)));
}

static INLINE void highbd_smooth_4xh_neon(uint16_t *dst, ptrdiff_t stride,
                                          const uint16_t *const top_row,
                                          const uint16_t *const left_column,
                                          const int height) {
  const uint16_t top_right = top_row[3];
  const uint16_t bottom_left = left_column[height - 1];
  const uint16_t *const weights_y = smooth_weights_u16 + height - 4;

  const uint16x4_t top_v = vld1_u16(top_row);
  const uint16x4_t bottom_left_v = vdup_n_u16(bottom_left);
  const uint16x4_t weights_x_v = vld1_u16(smooth_weights_u16);
  const uint16x4_t scaled_weights_x = negate_s8(weights_x_v);
  const uint32x4_t weighted_tr = vmull_n_u16(scaled_weights_x, top_right);

  for (int y = 0; y < height; ++y) {
    // Each variable in the running summation is named for the last item to be
    // accumulated.
    const uint32x4_t weighted_top =
        vmlal_n_u16(weighted_tr, top_v, weights_y[y]);
    const uint32x4_t weighted_left =
        vmlal_n_u16(weighted_top, weights_x_v, left_column[y]);
    const uint32x4_t weighted_bl =
        vmlal_n_u16(weighted_left, bottom_left_v, 256 - weights_y[y]);

    const uint16x4_t pred =
        vrshrn_n_u32(weighted_bl, SMOOTH_WEIGHT_LOG2_SCALE + 1);
    vst1_u16(dst, pred);
    dst += stride;
  }
}

// Common code between 8xH and [16|32|64]xH.
static INLINE void highbd_calculate_pred8(
    uint16_t *dst, const uint32x4_t weighted_corners_low,
    const uint32x4_t weighted_corners_high, const uint16x4x2_t top_vals,
    const uint16x4x2_t weights_x, const uint16_t left_y,
    const uint16_t weight_y) {
  // Each variable in the running summation is named for the last item to be
  // accumulated.
  const uint32x4_t weighted_top_low =
      vmlal_n_u16(weighted_corners_low, top_vals.val[0], weight_y);
  const uint32x4_t weighted_edges_low =
      vmlal_n_u16(weighted_top_low, weights_x.val[0], left_y);

  const uint16x4_t pred_low =
      vrshrn_n_u32(weighted_edges_low, SMOOTH_WEIGHT_LOG2_SCALE + 1);
  vst1_u16(dst, pred_low);

  const uint32x4_t weighted_top_high =
      vmlal_n_u16(weighted_corners_high, top_vals.val[1], weight_y);
  const uint32x4_t weighted_edges_high =
      vmlal_n_u16(weighted_top_high, weights_x.val[1], left_y);

  const uint16x4_t pred_high =
      vrshrn_n_u32(weighted_edges_high, SMOOTH_WEIGHT_LOG2_SCALE + 1);
  vst1_u16(dst + 4, pred_high);
}

static void highbd_smooth_8xh_neon(uint16_t *dst, ptrdiff_t stride,
                                   const uint16_t *const top_row,
                                   const uint16_t *const left_column,
                                   const int height) {
  const uint16_t top_right = top_row[7];
  const uint16_t bottom_left = left_column[height - 1];
  const uint16_t *const weights_y = smooth_weights_u16 + height - 4;

  const uint16x4x2_t top_vals = { { vld1_u16(top_row),
                                    vld1_u16(top_row + 4) } };
  const uint16x4_t bottom_left_v = vdup_n_u16(bottom_left);
  const uint16x4x2_t weights_x = { { vld1_u16(smooth_weights_u16 + 4),
                                     vld1_u16(smooth_weights_u16 + 8) } };
  const uint32x4_t weighted_tr_low =
      vmull_n_u16(negate_s8(weights_x.val[0]), top_right);
  const uint32x4_t weighted_tr_high =
      vmull_n_u16(negate_s8(weights_x.val[1]), top_right);

  for (int y = 0; y < height; ++y) {
    const uint32x4_t weighted_bl =
        vmull_n_u16(bottom_left_v, 256 - weights_y[y]);
    const uint32x4_t weighted_corners_low =
        vaddq_u32(weighted_bl, weighted_tr_low);
    const uint32x4_t weighted_corners_high =
        vaddq_u32(weighted_bl, weighted_tr_high);
    highbd_calculate_pred8(dst, weighted_corners_low, weighted_corners_high,
                           top_vals, weights_x, left_column[y], weights_y[y]);
    dst += stride;
  }
}

#define HIGHBD_SMOOTH_NXM(W, H)                                 \
  void aom_highbd_smooth_predictor_##W##x##H##_neon(            \
      uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, \
      const uint16_t *left, int bd) {                           \
    (void)bd;                                                   \
    highbd_smooth_##W##xh_neon(dst, y_stride, above, left, H);  \
  }

HIGHBD_SMOOTH_NXM(4, 4)
HIGHBD_SMOOTH_NXM(4, 8)
HIGHBD_SMOOTH_NXM(8, 4)
HIGHBD_SMOOTH_NXM(8, 8)
HIGHBD_SMOOTH_NXM(4, 16)
HIGHBD_SMOOTH_NXM(8, 16)
HIGHBD_SMOOTH_NXM(8, 32)

#undef HIGHBD_SMOOTH_NXM

// For width 16 and above.
#define HIGHBD_SMOOTH_PREDICTOR(W)                                             \
  static void highbd_smooth_##W##xh_neon(                                      \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *const top_row,          \
      const uint16_t *const left_column, const int height) {                   \
    const uint16_t top_right = top_row[(W)-1];                                 \
    const uint16_t bottom_left = left_column[height - 1];                      \
    const uint16_t *const weights_y = smooth_weights_u16 + height - 4;         \
                                                                               \
    /* Precompute weighted values that don't vary with |y|. */                 \
    uint32x4_t weighted_tr_low[(W) >> 3];                                      \
    uint32x4_t weighted_tr_high[(W) >> 3];                                     \
    for (int i = 0; i < (W) >> 3; ++i) {                                       \
      const int x = i << 3;                                                    \
      const uint16x4_t weights_x_low =                                         \
          vld1_u16(smooth_weights_u16 + (W)-4 + x);                            \
      weighted_tr_low[i] = vmull_n_u16(negate_s8(weights_x_low), top_right);   \
      const uint16x4_t weights_x_high =                                        \
          vld1_u16(smooth_weights_u16 + (W) + x);                              \
      weighted_tr_high[i] = vmull_n_u16(negate_s8(weights_x_high), top_right); \
    }                                                                          \
                                                                               \
    const uint16x4_t bottom_left_v = vdup_n_u16(bottom_left);                  \
    for (int y = 0; y < height; ++y) {                                         \
      const uint32x4_t weighted_bl =                                           \
          vmull_n_u16(bottom_left_v, 256 - weights_y[y]);                      \
      uint16_t *dst_x = dst;                                                   \
      for (int i = 0; i < (W) >> 3; ++i) {                                     \
        const int x = i << 3;                                                  \
        const uint16x4x2_t top_vals = { { vld1_u16(top_row + x),               \
                                          vld1_u16(top_row + x + 4) } };       \
        const uint32x4_t weighted_corners_low =                                \
            vaddq_u32(weighted_bl, weighted_tr_low[i]);                        \
        const uint32x4_t weighted_corners_high =                               \
            vaddq_u32(weighted_bl, weighted_tr_high[i]);                       \
        /* Accumulate weighted edge values and store. */                       \
        const uint16x4x2_t weights_x = {                                       \
          { vld1_u16(smooth_weights_u16 + (W)-4 + x),                          \
            vld1_u16(smooth_weights_u16 + (W) + x) }                           \
        };                                                                     \
        highbd_calculate_pred8(dst_x, weighted_corners_low,                    \
                               weighted_corners_high, top_vals, weights_x,     \
                               left_column[y], weights_y[y]);                  \
        dst_x += 8;                                                            \
      }                                                                        \
      dst += stride;                                                           \
    }                                                                          \
  }

HIGHBD_SMOOTH_PREDICTOR(16)
HIGHBD_SMOOTH_PREDICTOR(32)
HIGHBD_SMOOTH_PREDICTOR(64)

#undef HIGHBD_SMOOTH_PREDICTOR

#define HIGHBD_SMOOTH_NXM_WIDE(W, H)                            \
  void aom_highbd_smooth_predictor_##W##x##H##_neon(            \
      uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, \
      const uint16_t *left, int bd) {                           \
    (void)bd;                                                   \
    highbd_smooth_##W##xh_neon(dst, y_stride, above, left, H);  \
  }

HIGHBD_SMOOTH_NXM_WIDE(16, 4)
HIGHBD_SMOOTH_NXM_WIDE(16, 8)
HIGHBD_SMOOTH_NXM_WIDE(16, 16)
HIGHBD_SMOOTH_NXM_WIDE(16, 32)
HIGHBD_SMOOTH_NXM_WIDE(16, 64)
HIGHBD_SMOOTH_NXM_WIDE(32, 8)
HIGHBD_SMOOTH_NXM_WIDE(32, 16)
HIGHBD_SMOOTH_NXM_WIDE(32, 32)
HIGHBD_SMOOTH_NXM_WIDE(32, 64)
HIGHBD_SMOOTH_NXM_WIDE(64, 16)
HIGHBD_SMOOTH_NXM_WIDE(64, 32)
HIGHBD_SMOOTH_NXM_WIDE(64, 64)

#undef HIGHBD_SMOOTH_NXM_WIDE

static void highbd_smooth_v_4xh_neon(uint16_t *dst, ptrdiff_t stride,
                                     const uint16_t *const top_row,
                                     const uint16_t *const left_column,
                                     const int height) {
  const uint16_t bottom_left = left_column[height - 1];
  const uint16_t *const weights_y = smooth_weights_u16 + height - 4;

  const uint16x4_t top_v = vld1_u16(top_row);
  const uint16x4_t bottom_left_v = vdup_n_u16(bottom_left);

  for (int y = 0; y < height; ++y) {
    const uint32x4_t weighted_bl =
        vmull_n_u16(bottom_left_v, 256 - weights_y[y]);
    const uint32x4_t weighted_top =
        vmlal_n_u16(weighted_bl, top_v, weights_y[y]);
    vst1_u16(dst, vrshrn_n_u32(weighted_top, SMOOTH_WEIGHT_LOG2_SCALE));

    dst += stride;
  }
}

static void highbd_smooth_v_8xh_neon(uint16_t *dst, const ptrdiff_t stride,
                                     const uint16_t *const top_row,
                                     const uint16_t *const left_column,
                                     const int height) {
  const uint16_t bottom_left = left_column[height - 1];
  const uint16_t *const weights_y = smooth_weights_u16 + height - 4;

  const uint16x4_t top_low = vld1_u16(top_row);
  const uint16x4_t top_high = vld1_u16(top_row + 4);
  const uint16x4_t bottom_left_v = vdup_n_u16(bottom_left);

  for (int y = 0; y < height; ++y) {
    const uint32x4_t weighted_bl =
        vmull_n_u16(bottom_left_v, 256 - weights_y[y]);

    const uint32x4_t weighted_top_low =
        vmlal_n_u16(weighted_bl, top_low, weights_y[y]);
    vst1_u16(dst, vrshrn_n_u32(weighted_top_low, SMOOTH_WEIGHT_LOG2_SCALE));

    const uint32x4_t weighted_top_high =
        vmlal_n_u16(weighted_bl, top_high, weights_y[y]);
    vst1_u16(dst + 4,
             vrshrn_n_u32(weighted_top_high, SMOOTH_WEIGHT_LOG2_SCALE));
    dst += stride;
  }
}

#define HIGHBD_SMOOTH_V_NXM(W, H)                                \
  void aom_highbd_smooth_v_predictor_##W##x##H##_neon(           \
      uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above,  \
      const uint16_t *left, int bd) {                            \
    (void)bd;                                                    \
    highbd_smooth_v_##W##xh_neon(dst, y_stride, above, left, H); \
  }

HIGHBD_SMOOTH_V_NXM(4, 4)
HIGHBD_SMOOTH_V_NXM(4, 8)
HIGHBD_SMOOTH_V_NXM(4, 16)
HIGHBD_SMOOTH_V_NXM(8, 4)
HIGHBD_SMOOTH_V_NXM(8, 8)
HIGHBD_SMOOTH_V_NXM(8, 16)
HIGHBD_SMOOTH_V_NXM(8, 32)

#undef HIGHBD_SMOOTH_V_NXM

// For width 16 and above.
#define HIGHBD_SMOOTH_V_PREDICTOR(W)                                         \
  static void highbd_smooth_v_##W##xh_neon(                                  \
      uint16_t *dst, const ptrdiff_t stride, const uint16_t *const top_row,  \
      const uint16_t *const left_column, const int height) {                 \
    const uint16_t bottom_left = left_column[height - 1];                    \
    const uint16_t *const weights_y = smooth_weights_u16 + height - 4;       \
                                                                             \
    uint16x4x2_t top_vals[(W) >> 3];                                         \
    for (int i = 0; i < (W) >> 3; ++i) {                                     \
      const int x = i << 3;                                                  \
      top_vals[i].val[0] = vld1_u16(top_row + x);                            \
      top_vals[i].val[1] = vld1_u16(top_row + x + 4);                        \
    }                                                                        \
                                                                             \
    const uint16x4_t bottom_left_v = vdup_n_u16(bottom_left);                \
    for (int y = 0; y < height; ++y) {                                       \
      const uint32x4_t weighted_bl =                                         \
          vmull_n_u16(bottom_left_v, 256 - weights_y[y]);                    \
                                                                             \
      uint16_t *dst_x = dst;                                                 \
      for (int i = 0; i < (W) >> 3; ++i) {                                   \
        const uint32x4_t weighted_top_low =                                  \
            vmlal_n_u16(weighted_bl, top_vals[i].val[0], weights_y[y]);      \
        vst1_u16(dst_x,                                                      \
                 vrshrn_n_u32(weighted_top_low, SMOOTH_WEIGHT_LOG2_SCALE));  \
                                                                             \
        const uint32x4_t weighted_top_high =                                 \
            vmlal_n_u16(weighted_bl, top_vals[i].val[1], weights_y[y]);      \
        vst1_u16(dst_x + 4,                                                  \
                 vrshrn_n_u32(weighted_top_high, SMOOTH_WEIGHT_LOG2_SCALE)); \
        dst_x += 8;                                                          \
      }                                                                      \
      dst += stride;                                                         \
    }                                                                        \
  }

HIGHBD_SMOOTH_V_PREDICTOR(16)
HIGHBD_SMOOTH_V_PREDICTOR(32)
HIGHBD_SMOOTH_V_PREDICTOR(64)

#undef HIGHBD_SMOOTH_V_PREDICTOR

#define HIGHBD_SMOOTH_V_NXM_WIDE(W, H)                           \
  void aom_highbd_smooth_v_predictor_##W##x##H##_neon(           \
      uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above,  \
      const uint16_t *left, int bd) {                            \
    (void)bd;                                                    \
    highbd_smooth_v_##W##xh_neon(dst, y_stride, above, left, H); \
  }

HIGHBD_SMOOTH_V_NXM_WIDE(16, 4)
HIGHBD_SMOOTH_V_NXM_WIDE(16, 8)
HIGHBD_SMOOTH_V_NXM_WIDE(16, 16)
HIGHBD_SMOOTH_V_NXM_WIDE(16, 32)
HIGHBD_SMOOTH_V_NXM_WIDE(16, 64)
HIGHBD_SMOOTH_V_NXM_WIDE(32, 8)
HIGHBD_SMOOTH_V_NXM_WIDE(32, 16)
HIGHBD_SMOOTH_V_NXM_WIDE(32, 32)
HIGHBD_SMOOTH_V_NXM_WIDE(32, 64)
HIGHBD_SMOOTH_V_NXM_WIDE(64, 16)
HIGHBD_SMOOTH_V_NXM_WIDE(64, 32)
HIGHBD_SMOOTH_V_NXM_WIDE(64, 64)

#undef HIGHBD_SMOOTH_V_NXM_WIDE

static INLINE void highbd_smooth_h_4xh_neon(uint16_t *dst, ptrdiff_t stride,
                                            const uint16_t *const top_row,
                                            const uint16_t *const left_column,
                                            const int height) {
  const uint16_t top_right = top_row[3];

  const uint16x4_t weights_x = vld1_u16(smooth_weights_u16);
  const uint16x4_t scaled_weights_x = negate_s8(weights_x);

  const uint32x4_t weighted_tr = vmull_n_u16(scaled_weights_x, top_right);
  for (int y = 0; y < height; ++y) {
    const uint32x4_t weighted_left =
        vmlal_n_u16(weighted_tr, weights_x, left_column[y]);
    vst1_u16(dst, vrshrn_n_u32(weighted_left, SMOOTH_WEIGHT_LOG2_SCALE));
    dst += stride;
  }
}

static INLINE void highbd_smooth_h_8xh_neon(uint16_t *dst, ptrdiff_t stride,
                                            const uint16_t *const top_row,
                                            const uint16_t *const left_column,
                                            const int height) {
  const uint16_t top_right = top_row[7];

  const uint16x4x2_t weights_x = { { vld1_u16(smooth_weights_u16 + 4),
                                     vld1_u16(smooth_weights_u16 + 8) } };

  const uint32x4_t weighted_tr_low =
      vmull_n_u16(negate_s8(weights_x.val[0]), top_right);
  const uint32x4_t weighted_tr_high =
      vmull_n_u16(negate_s8(weights_x.val[1]), top_right);

  for (int y = 0; y < height; ++y) {
    const uint16_t left_y = left_column[y];
    const uint32x4_t weighted_left_low =
        vmlal_n_u16(weighted_tr_low, weights_x.val[0], left_y);
    vst1_u16(dst, vrshrn_n_u32(weighted_left_low, SMOOTH_WEIGHT_LOG2_SCALE));

    const uint32x4_t weighted_left_high =
        vmlal_n_u16(weighted_tr_high, weights_x.val[1], left_y);
    vst1_u16(dst + 4,
             vrshrn_n_u32(weighted_left_high, SMOOTH_WEIGHT_LOG2_SCALE));
    dst += stride;
  }
}

#define HIGHBD_SMOOTH_H_NXM(W, H)                                \
  void aom_highbd_smooth_h_predictor_##W##x##H##_neon(           \
      uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above,  \
      const uint16_t *left, int bd) {                            \
    (void)bd;                                                    \
    highbd_smooth_h_##W##xh_neon(dst, y_stride, above, left, H); \
  }

HIGHBD_SMOOTH_H_NXM(4, 4)
HIGHBD_SMOOTH_H_NXM(4, 8)
HIGHBD_SMOOTH_H_NXM(4, 16)
HIGHBD_SMOOTH_H_NXM(8, 4)
HIGHBD_SMOOTH_H_NXM(8, 8)
HIGHBD_SMOOTH_H_NXM(8, 16)
HIGHBD_SMOOTH_H_NXM(8, 32)

#undef HIGHBD_SMOOTH_H_NXM

// For width 16 and above.
#define HIGHBD_SMOOTH_H_PREDICTOR(W)                                          \
  void highbd_smooth_h_##W##xh_neon(                                          \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *const top_row,         \
      const uint16_t *const left_column, const int height) {                  \
    const uint16_t top_right = top_row[(W)-1];                                \
                                                                              \
    uint16x4_t weights_x_low[(W) >> 3];                                       \
    uint16x4_t weights_x_high[(W) >> 3];                                      \
    uint32x4_t weighted_tr_low[(W) >> 3];                                     \
    uint32x4_t weighted_tr_high[(W) >> 3];                                    \
    for (int i = 0; i < (W) >> 3; ++i) {                                      \
      const int x = i << 3;                                                   \
      weights_x_low[i] = vld1_u16(smooth_weights_u16 + (W)-4 + x);            \
      weighted_tr_low[i] =                                                    \
          vmull_n_u16(negate_s8(weights_x_low[i]), top_right);                \
      weights_x_high[i] = vld1_u16(smooth_weights_u16 + (W) + x);             \
      weighted_tr_high[i] =                                                   \
          vmull_n_u16(negate_s8(weights_x_high[i]), top_right);               \
    }                                                                         \
                                                                              \
    for (int y = 0; y < height; ++y) {                                        \
      uint16_t *dst_x = dst;                                                  \
      const uint16_t left_y = left_column[y];                                 \
      for (int i = 0; i < (W) >> 3; ++i) {                                    \
        const uint32x4_t weighted_left_low =                                  \
            vmlal_n_u16(weighted_tr_low[i], weights_x_low[i], left_y);        \
        vst1_u16(dst_x,                                                       \
                 vrshrn_n_u32(weighted_left_low, SMOOTH_WEIGHT_LOG2_SCALE));  \
                                                                              \
        const uint32x4_t weighted_left_high =                                 \
            vmlal_n_u16(weighted_tr_high[i], weights_x_high[i], left_y);      \
        vst1_u16(dst_x + 4,                                                   \
                 vrshrn_n_u32(weighted_left_high, SMOOTH_WEIGHT_LOG2_SCALE)); \
        dst_x += 8;                                                           \
      }                                                                       \
      dst += stride;                                                          \
    }                                                                         \
  }

HIGHBD_SMOOTH_H_PREDICTOR(16)
HIGHBD_SMOOTH_H_PREDICTOR(32)
HIGHBD_SMOOTH_H_PREDICTOR(64)

#undef HIGHBD_SMOOTH_H_PREDICTOR

#define HIGHBD_SMOOTH_H_NXM_WIDE(W, H)                           \
  void aom_highbd_smooth_h_predictor_##W##x##H##_neon(           \
      uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above,  \
      const uint16_t *left, int bd) {                            \
    (void)bd;                                                    \
    highbd_smooth_h_##W##xh_neon(dst, y_stride, above, left, H); \
  }

HIGHBD_SMOOTH_H_NXM_WIDE(16, 4)
HIGHBD_SMOOTH_H_NXM_WIDE(16, 8)
HIGHBD_SMOOTH_H_NXM_WIDE(16, 16)
HIGHBD_SMOOTH_H_NXM_WIDE(16, 32)
HIGHBD_SMOOTH_H_NXM_WIDE(16, 64)
HIGHBD_SMOOTH_H_NXM_WIDE(32, 8)
HIGHBD_SMOOTH_H_NXM_WIDE(32, 16)
HIGHBD_SMOOTH_H_NXM_WIDE(32, 32)
HIGHBD_SMOOTH_H_NXM_WIDE(32, 64)
HIGHBD_SMOOTH_H_NXM_WIDE(64, 16)
HIGHBD_SMOOTH_H_NXM_WIDE(64, 32)
HIGHBD_SMOOTH_H_NXM_WIDE(64, 64)

#undef HIGHBD_SMOOTH_H_NXM_WIDE
