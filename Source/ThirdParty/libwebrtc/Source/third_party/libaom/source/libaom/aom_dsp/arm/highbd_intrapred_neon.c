/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved.
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
#include "config/av1_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_dsp/intrapred_common.h"

// -----------------------------------------------------------------------------
// DC

static inline void highbd_dc_store_4xh(uint16_t *dst, ptrdiff_t stride, int h,
                                       uint16x4_t dc) {
  for (int i = 0; i < h; ++i) {
    vst1_u16(dst + i * stride, dc);
  }
}

static inline void highbd_dc_store_8xh(uint16_t *dst, ptrdiff_t stride, int h,
                                       uint16x8_t dc) {
  for (int i = 0; i < h; ++i) {
    vst1q_u16(dst + i * stride, dc);
  }
}

static inline void highbd_dc_store_16xh(uint16_t *dst, ptrdiff_t stride, int h,
                                        uint16x8_t dc) {
  for (int i = 0; i < h; ++i) {
    vst1q_u16(dst + i * stride, dc);
    vst1q_u16(dst + i * stride + 8, dc);
  }
}

static inline void highbd_dc_store_32xh(uint16_t *dst, ptrdiff_t stride, int h,
                                        uint16x8_t dc) {
  for (int i = 0; i < h; ++i) {
    vst1q_u16(dst + i * stride, dc);
    vst1q_u16(dst + i * stride + 8, dc);
    vst1q_u16(dst + i * stride + 16, dc);
    vst1q_u16(dst + i * stride + 24, dc);
  }
}

static inline void highbd_dc_store_64xh(uint16_t *dst, ptrdiff_t stride, int h,
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

static inline uint32x4_t horizontal_add_and_broadcast_long_u16x8(uint16x8_t a) {
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

static inline uint16x8_t highbd_dc_load_partial_sum_4(const uint16_t *left) {
  // Nothing to do since sum is already one vector, but saves needing to
  // special case w=4 or h=4 cases. The combine will be zero cost for a sane
  // compiler since vld1 already sets the top half of a vector to zero as part
  // of the operation.
  return vcombine_u16(vld1_u16(left), vdup_n_u16(0));
}

static inline uint16x8_t highbd_dc_load_partial_sum_8(const uint16_t *left) {
  // Nothing to do since sum is already one vector, but saves needing to
  // special case w=8 or h=8 cases.
  return vld1q_u16(left);
}

static inline uint16x8_t highbd_dc_load_partial_sum_16(const uint16_t *left) {
  const uint16x8_t a0 = vld1q_u16(left + 0);  // up to 12 bits
  const uint16x8_t a1 = vld1q_u16(left + 8);
  return vaddq_u16(a0, a1);  // up to 13 bits
}

static inline uint16x8_t highbd_dc_load_partial_sum_32(const uint16_t *left) {
  const uint16x8_t a0 = vld1q_u16(left + 0);  // up to 12 bits
  const uint16x8_t a1 = vld1q_u16(left + 8);
  const uint16x8_t a2 = vld1q_u16(left + 16);
  const uint16x8_t a3 = vld1q_u16(left + 24);
  const uint16x8_t b0 = vaddq_u16(a0, a1);  // up to 13 bits
  const uint16x8_t b1 = vaddq_u16(a2, a3);
  return vaddq_u16(b0, b1);  // up to 14 bits
}

static inline uint16x8_t highbd_dc_load_partial_sum_64(const uint16_t *left) {
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

static inline int divide_using_multiply_shift(int num, int shift1,
                                              int multiplier, int shift2) {
  const int interm = num >> shift1;
  return interm * multiplier >> shift2;
}

#define HIGHBD_DC_MULTIPLIER_1X2 0xAAAB
#define HIGHBD_DC_MULTIPLIER_1X4 0x6667
#define HIGHBD_DC_SHIFT2 17

static inline int highbd_dc_predictor_rect(int bw, int bh, int sum, int shift1,
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

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
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
#else
HIGHBD_DC_PREDICTOR_RECT(4, 8, , 2, HIGHBD_DC_MULTIPLIER_1X2)
HIGHBD_DC_PREDICTOR_RECT(8, 4, q, 2, HIGHBD_DC_MULTIPLIER_1X2)
HIGHBD_DC_PREDICTOR_RECT(8, 16, q, 3, HIGHBD_DC_MULTIPLIER_1X2)
HIGHBD_DC_PREDICTOR_RECT(16, 8, q, 3, HIGHBD_DC_MULTIPLIER_1X2)
HIGHBD_DC_PREDICTOR_RECT(16, 32, q, 4, HIGHBD_DC_MULTIPLIER_1X2)
HIGHBD_DC_PREDICTOR_RECT(32, 16, q, 4, HIGHBD_DC_MULTIPLIER_1X2)
HIGHBD_DC_PREDICTOR_RECT(32, 64, q, 5, HIGHBD_DC_MULTIPLIER_1X2)
HIGHBD_DC_PREDICTOR_RECT(64, 32, q, 5, HIGHBD_DC_MULTIPLIER_1X2)
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

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

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
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
#else
HIGHBD_DC_PREDICTOR_128(4, 4, )
HIGHBD_DC_PREDICTOR_128(4, 8, )
HIGHBD_DC_PREDICTOR_128(8, 4, q)
HIGHBD_DC_PREDICTOR_128(8, 8, q)
HIGHBD_DC_PREDICTOR_128(8, 16, q)
HIGHBD_DC_PREDICTOR_128(16, 8, q)
HIGHBD_DC_PREDICTOR_128(16, 16, q)
HIGHBD_DC_PREDICTOR_128(16, 32, q)
HIGHBD_DC_PREDICTOR_128(32, 16, q)
HIGHBD_DC_PREDICTOR_128(32, 32, q)
HIGHBD_DC_PREDICTOR_128(32, 64, q)
HIGHBD_DC_PREDICTOR_128(64, 32, q)
HIGHBD_DC_PREDICTOR_128(64, 64, q)
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

#undef HIGHBD_DC_PREDICTOR_128

// -----------------------------------------------------------------------------
// DC_LEFT

static inline uint32x4_t highbd_dc_load_sum_4(const uint16_t *left) {
  const uint16x4_t a = vld1_u16(left);   // up to 12 bits
  const uint16x4_t b = vpadd_u16(a, a);  // up to 13 bits
  return vcombine_u32(vpaddl_u16(b), vdup_n_u32(0));
}

static inline uint32x4_t highbd_dc_load_sum_8(const uint16_t *left) {
  return horizontal_add_and_broadcast_long_u16x8(vld1q_u16(left));
}

static inline uint32x4_t highbd_dc_load_sum_16(const uint16_t *left) {
  return horizontal_add_and_broadcast_long_u16x8(
      highbd_dc_load_partial_sum_16(left));
}

static inline uint32x4_t highbd_dc_load_sum_32(const uint16_t *left) {
  return horizontal_add_and_broadcast_long_u16x8(
      highbd_dc_load_partial_sum_32(left));
}

static inline uint32x4_t highbd_dc_load_sum_64(const uint16_t *left) {
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

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
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
#else
DC_PREDICTOR_LEFT(4, 4, 2, )
DC_PREDICTOR_LEFT(4, 8, 3, )
DC_PREDICTOR_LEFT(8, 4, 2, q)
DC_PREDICTOR_LEFT(8, 8, 3, q)
DC_PREDICTOR_LEFT(8, 16, 4, q)
DC_PREDICTOR_LEFT(16, 8, 3, q)
DC_PREDICTOR_LEFT(16, 16, 4, q)
DC_PREDICTOR_LEFT(16, 32, 5, q)
DC_PREDICTOR_LEFT(32, 16, 4, q)
DC_PREDICTOR_LEFT(32, 32, 5, q)
DC_PREDICTOR_LEFT(32, 64, 6, q)
DC_PREDICTOR_LEFT(64, 32, 5, q)
DC_PREDICTOR_LEFT(64, 64, 6, q)
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

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

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
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
#else
DC_PREDICTOR_TOP(4, 4, 2, )
DC_PREDICTOR_TOP(4, 8, 2, )
DC_PREDICTOR_TOP(8, 4, 3, q)
DC_PREDICTOR_TOP(8, 8, 3, q)
DC_PREDICTOR_TOP(8, 16, 3, q)
DC_PREDICTOR_TOP(16, 8, 4, q)
DC_PREDICTOR_TOP(16, 16, 4, q)
DC_PREDICTOR_TOP(16, 32, 4, q)
DC_PREDICTOR_TOP(32, 16, 5, q)
DC_PREDICTOR_TOP(32, 32, 5, q)
DC_PREDICTOR_TOP(32, 64, 5, q)
DC_PREDICTOR_TOP(64, 32, 6, q)
DC_PREDICTOR_TOP(64, 64, 6, q)
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

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

static inline uint16x8x2_t load_uint16x8x2(uint16_t const *ptr) {
  uint16x8x2_t x;
  // Clang/gcc uses ldp here.
  x.val[0] = vld1q_u16(ptr);
  x.val[1] = vld1q_u16(ptr + 8);
  return x;
}

static inline void store_uint16x8x2(uint16_t *ptr, uint16x8x2_t x) {
  vst1q_u16(ptr, x.val[0]);
  vst1q_u16(ptr + 8, x.val[1]);
}

static inline void vertical4xh_neon(uint16_t *dst, ptrdiff_t stride,
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

static inline void vertical8xh_neon(uint16_t *dst, ptrdiff_t stride,
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

static inline void vertical16xh_neon(uint16_t *dst, ptrdiff_t stride,
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

static inline uint16x8x4_t load_uint16x8x4(uint16_t const *ptr) {
  uint16x8x4_t x;
  // Clang/gcc uses ldp here.
  x.val[0] = vld1q_u16(ptr);
  x.val[1] = vld1q_u16(ptr + 8);
  x.val[2] = vld1q_u16(ptr + 16);
  x.val[3] = vld1q_u16(ptr + 24);
  return x;
}

static inline void store_uint16x8x4(uint16_t *ptr, uint16x8x4_t x) {
  vst1q_u16(ptr, x.val[0]);
  vst1q_u16(ptr + 8, x.val[1]);
  vst1q_u16(ptr + 16, x.val[2]);
  vst1q_u16(ptr + 24, x.val[3]);
}

static inline void vertical32xh_neon(uint16_t *dst, ptrdiff_t stride,
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

static inline void vertical64xh_neon(uint16_t *dst, ptrdiff_t stride,
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

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
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
#else
HIGHBD_V_NXM(4, 4)
HIGHBD_V_NXM(4, 8)

HIGHBD_V_NXM(8, 4)
HIGHBD_V_NXM(8, 8)
HIGHBD_V_NXM(8, 16)

HIGHBD_V_NXM(16, 8)
HIGHBD_V_NXM(16, 16)
HIGHBD_V_NXM(16, 32)

HIGHBD_V_NXM(32, 16)
HIGHBD_V_NXM(32, 32)
HIGHBD_V_NXM(32, 64)

HIGHBD_V_NXM(64, 32)
HIGHBD_V_NXM(64, 64)
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

// -----------------------------------------------------------------------------
// H_PRED

static inline void highbd_h_store_4x4(uint16_t *dst, ptrdiff_t stride,
                                      uint16x4_t left) {
  vst1_u16(dst + 0 * stride, vdup_lane_u16(left, 0));
  vst1_u16(dst + 1 * stride, vdup_lane_u16(left, 1));
  vst1_u16(dst + 2 * stride, vdup_lane_u16(left, 2));
  vst1_u16(dst + 3 * stride, vdup_lane_u16(left, 3));
}

static inline void highbd_h_store_8x4(uint16_t *dst, ptrdiff_t stride,
                                      uint16x4_t left) {
  vst1q_u16(dst + 0 * stride, vdupq_lane_u16(left, 0));
  vst1q_u16(dst + 1 * stride, vdupq_lane_u16(left, 1));
  vst1q_u16(dst + 2 * stride, vdupq_lane_u16(left, 2));
  vst1q_u16(dst + 3 * stride, vdupq_lane_u16(left, 3));
}

static inline void highbd_h_store_16x1(uint16_t *dst, uint16x8_t left) {
  vst1q_u16(dst + 0, left);
  vst1q_u16(dst + 8, left);
}

static inline void highbd_h_store_16x4(uint16_t *dst, ptrdiff_t stride,
                                       uint16x4_t left) {
  highbd_h_store_16x1(dst + 0 * stride, vdupq_lane_u16(left, 0));
  highbd_h_store_16x1(dst + 1 * stride, vdupq_lane_u16(left, 1));
  highbd_h_store_16x1(dst + 2 * stride, vdupq_lane_u16(left, 2));
  highbd_h_store_16x1(dst + 3 * stride, vdupq_lane_u16(left, 3));
}

static inline void highbd_h_store_32x1(uint16_t *dst, uint16x8_t left) {
  vst1q_u16(dst + 0, left);
  vst1q_u16(dst + 8, left);
  vst1q_u16(dst + 16, left);
  vst1q_u16(dst + 24, left);
}

static inline void highbd_h_store_32x4(uint16_t *dst, ptrdiff_t stride,
                                       uint16x4_t left) {
  highbd_h_store_32x1(dst + 0 * stride, vdupq_lane_u16(left, 0));
  highbd_h_store_32x1(dst + 1 * stride, vdupq_lane_u16(left, 1));
  highbd_h_store_32x1(dst + 2 * stride, vdupq_lane_u16(left, 2));
  highbd_h_store_32x1(dst + 3 * stride, vdupq_lane_u16(left, 3));
}

static inline void highbd_h_store_64x1(uint16_t *dst, uint16x8_t left) {
  vst1q_u16(dst + 0, left);
  vst1q_u16(dst + 8, left);
  vst1q_u16(dst + 16, left);
  vst1q_u16(dst + 24, left);
  vst1q_u16(dst + 32, left);
  vst1q_u16(dst + 40, left);
  vst1q_u16(dst + 48, left);
  vst1q_u16(dst + 56, left);
}

static inline void highbd_h_store_64x4(uint16_t *dst, ptrdiff_t stride,
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

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_highbd_h_predictor_16x4_neon(uint16_t *dst, ptrdiff_t stride,
                                      const uint16_t *above,
                                      const uint16_t *left, int bd) {
  (void)above;
  (void)bd;
  highbd_h_store_16x4(dst, stride, vld1_u16(left));
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_highbd_h_predictor_16x8_neon(uint16_t *dst, ptrdiff_t stride,
                                      const uint16_t *above,
                                      const uint16_t *left, int bd) {
  (void)above;
  (void)bd;
  uint16x8_t l = vld1q_u16(left);
  highbd_h_store_16x4(dst + 0 * stride, stride, vget_low_u16(l));
  highbd_h_store_16x4(dst + 4 * stride, stride, vget_high_u16(l));
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_highbd_h_predictor_32x8_neon(uint16_t *dst, ptrdiff_t stride,
                                      const uint16_t *above,
                                      const uint16_t *left, int bd) {
  (void)above;
  (void)bd;
  uint16x8_t l = vld1q_u16(left);
  highbd_h_store_32x4(dst + 0 * stride, stride, vget_low_u16(l));
  highbd_h_store_32x4(dst + 4 * stride, stride, vget_high_u16(l));
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

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

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
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
#else
HIGHBD_H_WXH_LARGE(8, 16)
HIGHBD_H_WXH_LARGE(16, 16)
HIGHBD_H_WXH_LARGE(16, 32)
HIGHBD_H_WXH_LARGE(32, 16)
HIGHBD_H_WXH_LARGE(32, 32)
HIGHBD_H_WXH_LARGE(32, 64)
HIGHBD_H_WXH_LARGE(64, 32)
HIGHBD_H_WXH_LARGE(64, 64)
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

#undef HIGHBD_H_WXH_LARGE

// -----------------------------------------------------------------------------
// PAETH

static inline void highbd_paeth_4or8_x_h_neon(uint16_t *dest, ptrdiff_t stride,
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

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
HIGHBD_PAETH_NXM(4, 4)
HIGHBD_PAETH_NXM(4, 8)
HIGHBD_PAETH_NXM(4, 16)
HIGHBD_PAETH_NXM(8, 4)
HIGHBD_PAETH_NXM(8, 8)
HIGHBD_PAETH_NXM(8, 16)
HIGHBD_PAETH_NXM(8, 32)
#else
HIGHBD_PAETH_NXM(4, 4)
HIGHBD_PAETH_NXM(4, 8)
HIGHBD_PAETH_NXM(8, 4)
HIGHBD_PAETH_NXM(8, 8)
HIGHBD_PAETH_NXM(8, 16)
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

// Select the closest values and collect them.
static inline uint16x8_t select_paeth(const uint16x8_t top,
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

static inline void highbd_paeth16_plus_x_h_neon(
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

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
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
#else
HIGHBD_PAETH_NXM_WIDE(16, 8)
HIGHBD_PAETH_NXM_WIDE(16, 16)
HIGHBD_PAETH_NXM_WIDE(16, 32)
HIGHBD_PAETH_NXM_WIDE(32, 16)
HIGHBD_PAETH_NXM_WIDE(32, 32)
HIGHBD_PAETH_NXM_WIDE(32, 64)
HIGHBD_PAETH_NXM_WIDE(64, 32)
HIGHBD_PAETH_NXM_WIDE(64, 64)
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

// -----------------------------------------------------------------------------
// SMOOTH

// 256 - v = vneg_s8(v)
static inline uint16x4_t negate_s8(const uint16x4_t v) {
  return vreinterpret_u16_s8(vneg_s8(vreinterpret_s8_u16(v)));
}

static inline void highbd_smooth_4xh_neon(uint16_t *dst, ptrdiff_t stride,
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
static inline void highbd_calculate_pred8(
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

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
HIGHBD_SMOOTH_NXM(4, 4)
HIGHBD_SMOOTH_NXM(4, 8)
HIGHBD_SMOOTH_NXM(8, 4)
HIGHBD_SMOOTH_NXM(8, 8)
HIGHBD_SMOOTH_NXM(4, 16)
HIGHBD_SMOOTH_NXM(8, 16)
HIGHBD_SMOOTH_NXM(8, 32)
#else
HIGHBD_SMOOTH_NXM(4, 4)
HIGHBD_SMOOTH_NXM(4, 8)
HIGHBD_SMOOTH_NXM(8, 4)
HIGHBD_SMOOTH_NXM(8, 8)
HIGHBD_SMOOTH_NXM(8, 16)
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

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

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
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
#else
HIGHBD_SMOOTH_NXM_WIDE(16, 8)
HIGHBD_SMOOTH_NXM_WIDE(16, 16)
HIGHBD_SMOOTH_NXM_WIDE(16, 32)
HIGHBD_SMOOTH_NXM_WIDE(32, 16)
HIGHBD_SMOOTH_NXM_WIDE(32, 32)
HIGHBD_SMOOTH_NXM_WIDE(32, 64)
HIGHBD_SMOOTH_NXM_WIDE(64, 32)
HIGHBD_SMOOTH_NXM_WIDE(64, 64)
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

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

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
HIGHBD_SMOOTH_V_NXM(4, 4)
HIGHBD_SMOOTH_V_NXM(4, 8)
HIGHBD_SMOOTH_V_NXM(4, 16)
HIGHBD_SMOOTH_V_NXM(8, 4)
HIGHBD_SMOOTH_V_NXM(8, 8)
HIGHBD_SMOOTH_V_NXM(8, 16)
HIGHBD_SMOOTH_V_NXM(8, 32)
#else
HIGHBD_SMOOTH_V_NXM(4, 4)
HIGHBD_SMOOTH_V_NXM(4, 8)
HIGHBD_SMOOTH_V_NXM(8, 4)
HIGHBD_SMOOTH_V_NXM(8, 8)
HIGHBD_SMOOTH_V_NXM(8, 16)
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

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

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
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
#else
HIGHBD_SMOOTH_V_NXM_WIDE(16, 8)
HIGHBD_SMOOTH_V_NXM_WIDE(16, 16)
HIGHBD_SMOOTH_V_NXM_WIDE(16, 32)
HIGHBD_SMOOTH_V_NXM_WIDE(32, 16)
HIGHBD_SMOOTH_V_NXM_WIDE(32, 32)
HIGHBD_SMOOTH_V_NXM_WIDE(32, 64)
HIGHBD_SMOOTH_V_NXM_WIDE(64, 32)
HIGHBD_SMOOTH_V_NXM_WIDE(64, 64)
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

#undef HIGHBD_SMOOTH_V_NXM_WIDE

static inline void highbd_smooth_h_4xh_neon(uint16_t *dst, ptrdiff_t stride,
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

static inline void highbd_smooth_h_8xh_neon(uint16_t *dst, ptrdiff_t stride,
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

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
HIGHBD_SMOOTH_H_NXM(4, 4)
HIGHBD_SMOOTH_H_NXM(4, 8)
HIGHBD_SMOOTH_H_NXM(4, 16)
HIGHBD_SMOOTH_H_NXM(8, 4)
HIGHBD_SMOOTH_H_NXM(8, 8)
HIGHBD_SMOOTH_H_NXM(8, 16)
HIGHBD_SMOOTH_H_NXM(8, 32)
#else
HIGHBD_SMOOTH_H_NXM(4, 4)
HIGHBD_SMOOTH_H_NXM(4, 8)
HIGHBD_SMOOTH_H_NXM(8, 4)
HIGHBD_SMOOTH_H_NXM(8, 8)
HIGHBD_SMOOTH_H_NXM(8, 16)
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

#undef HIGHBD_SMOOTH_H_NXM

// For width 16 and above.
#define HIGHBD_SMOOTH_H_PREDICTOR(W)                                          \
  static void highbd_smooth_h_##W##xh_neon(                                   \
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

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
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
#else
HIGHBD_SMOOTH_H_NXM_WIDE(16, 8)
HIGHBD_SMOOTH_H_NXM_WIDE(16, 16)
HIGHBD_SMOOTH_H_NXM_WIDE(16, 32)
HIGHBD_SMOOTH_H_NXM_WIDE(32, 16)
HIGHBD_SMOOTH_H_NXM_WIDE(32, 32)
HIGHBD_SMOOTH_H_NXM_WIDE(32, 64)
HIGHBD_SMOOTH_H_NXM_WIDE(64, 32)
HIGHBD_SMOOTH_H_NXM_WIDE(64, 64)
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

#undef HIGHBD_SMOOTH_H_NXM_WIDE

// -----------------------------------------------------------------------------
// Z1

static int16_t iota1_s16[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
static int16_t iota2_s16[] = { 0, 2, 4, 6, 8, 10, 12, 14 };

static AOM_FORCE_INLINE uint16x4_t highbd_dr_z1_apply_shift_x4(uint16x4_t a0,
                                                               uint16x4_t a1,
                                                               int shift) {
  // The C implementation of the z1 predictor uses (32 - shift) and a right
  // shift by 5, however we instead double shift to avoid an unnecessary right
  // shift by 1.
  uint32x4_t res = vmull_n_u16(a1, shift);
  res = vmlal_n_u16(res, a0, 64 - shift);
  return vrshrn_n_u32(res, 6);
}

static AOM_FORCE_INLINE uint16x8_t highbd_dr_z1_apply_shift_x8(uint16x8_t a0,
                                                               uint16x8_t a1,
                                                               int shift) {
  return vcombine_u16(
      highbd_dr_z1_apply_shift_x4(vget_low_u16(a0), vget_low_u16(a1), shift),
      highbd_dr_z1_apply_shift_x4(vget_high_u16(a0), vget_high_u16(a1), shift));
}

// clang-format off
static const uint8_t kLoadMaxShuffles[] = {
  14, 15, 14, 15, 14, 15, 14, 15, 14, 15, 14, 15, 14, 15, 14, 15,
  12, 13, 14, 15, 14, 15, 14, 15, 14, 15, 14, 15, 14, 15, 14, 15,
  10, 11, 12, 13, 14, 15, 14, 15, 14, 15, 14, 15, 14, 15, 14, 15,
   8,  9, 10, 11, 12, 13, 14, 15, 14, 15, 14, 15, 14, 15, 14, 15,
   6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 14, 15, 14, 15, 14, 15,
   4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 14, 15, 14, 15,
   2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 14, 15,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
};
// clang-format on

static inline uint16x8_t zn_load_masked_neon(const uint16_t *ptr,
                                             int shuffle_idx) {
  uint8x16_t shuffle = vld1q_u8(&kLoadMaxShuffles[16 * shuffle_idx]);
  uint8x16_t src = vreinterpretq_u8_u16(vld1q_u16(ptr));
#if AOM_ARCH_AARCH64
  return vreinterpretq_u16_u8(vqtbl1q_u8(src, shuffle));
#else
  uint8x8x2_t src2 = { { vget_low_u8(src), vget_high_u8(src) } };
  uint8x8_t lo = vtbl2_u8(src2, vget_low_u8(shuffle));
  uint8x8_t hi = vtbl2_u8(src2, vget_high_u8(shuffle));
  return vreinterpretq_u16_u8(vcombine_u8(lo, hi));
#endif
}

static void highbd_dr_prediction_z1_upsample0_neon(uint16_t *dst,
                                                   ptrdiff_t stride, int bw,
                                                   int bh,
                                                   const uint16_t *above,
                                                   int dx) {
  assert(bw % 4 == 0);
  assert(bh % 4 == 0);
  assert(dx > 0);

  const int max_base_x = (bw + bh) - 1;
  const int above_max = above[max_base_x];

  const int16x8_t iota1x8 = vld1q_s16(iota1_s16);
  const int16x4_t iota1x4 = vget_low_s16(iota1x8);

  int x = dx;
  int r = 0;
  do {
    const int base = x >> 6;
    if (base >= max_base_x) {
      for (int i = r; i < bh; ++i) {
        aom_memset16(dst, above_max, bw);
        dst += stride;
      }
      return;
    }

    // The C implementation of the z1 predictor when not upsampling uses:
    // ((x & 0x3f) >> 1)
    // The right shift is unnecessary here since we instead shift by +1 later,
    // so adjust the mask to 0x3e to ensure we don't consider the extra bit.
    const int shift = x & 0x3e;

    if (bw == 4) {
      const uint16x4_t a0 = vld1_u16(&above[base]);
      const uint16x4_t a1 = vld1_u16(&above[base + 1]);
      const uint16x4_t val = highbd_dr_z1_apply_shift_x4(a0, a1, shift);
      const uint16x4_t cmp = vcgt_s16(vdup_n_s16(max_base_x - base), iota1x4);
      const uint16x4_t res = vbsl_u16(cmp, val, vdup_n_u16(above_max));
      vst1_u16(dst, res);
    } else {
      int c = 0;
      do {
        uint16x8_t a0;
        uint16x8_t a1;
        if (base + c >= max_base_x) {
          a0 = a1 = vdupq_n_u16(above_max);
        } else {
          if (base + c + 7 >= max_base_x) {
            int shuffle_idx = max_base_x - base - c;
            a0 = zn_load_masked_neon(above + (max_base_x - 7), shuffle_idx);
          } else {
            a0 = vld1q_u16(above + base + c);
          }
          if (base + c + 8 >= max_base_x) {
            int shuffle_idx = max_base_x - base - c - 1;
            a1 = zn_load_masked_neon(above + (max_base_x - 7), shuffle_idx);
          } else {
            a1 = vld1q_u16(above + base + c + 1);
          }
        }

        vst1q_u16(dst + c, highbd_dr_z1_apply_shift_x8(a0, a1, shift));
        c += 8;
      } while (c < bw);
    }

    dst += stride;
    x += dx;
  } while (++r < bh);
}

static void highbd_dr_prediction_z1_upsample1_neon(uint16_t *dst,
                                                   ptrdiff_t stride, int bw,
                                                   int bh,
                                                   const uint16_t *above,
                                                   int dx) {
  assert(bw % 4 == 0);
  assert(bh % 4 == 0);
  assert(dx > 0);

  const int max_base_x = ((bw + bh) - 1) << 1;
  const int above_max = above[max_base_x];

  const int16x8_t iota2x8 = vld1q_s16(iota2_s16);
  const int16x4_t iota2x4 = vget_low_s16(iota2x8);

  int x = dx;
  int r = 0;
  do {
    const int base = x >> 5;
    if (base >= max_base_x) {
      for (int i = r; i < bh; ++i) {
        aom_memset16(dst, above_max, bw);
        dst += stride;
      }
      return;
    }

    // The C implementation of the z1 predictor when upsampling uses:
    // (((x << 1) & 0x3f) >> 1)
    // The right shift is unnecessary here since we instead shift by +1 later,
    // so adjust the mask to 0x3e to ensure we don't consider the extra bit.
    const int shift = (x << 1) & 0x3e;

    if (bw == 4) {
      const uint16x4x2_t a01 = vld2_u16(&above[base]);
      const uint16x4_t val =
          highbd_dr_z1_apply_shift_x4(a01.val[0], a01.val[1], shift);
      const uint16x4_t cmp = vcgt_s16(vdup_n_s16(max_base_x - base), iota2x4);
      const uint16x4_t res = vbsl_u16(cmp, val, vdup_n_u16(above_max));
      vst1_u16(dst, res);
    } else {
      int c = 0;
      do {
        const uint16x8x2_t a01 = vld2q_u16(&above[base + 2 * c]);
        const uint16x8_t val =
            highbd_dr_z1_apply_shift_x8(a01.val[0], a01.val[1], shift);
        const uint16x8_t cmp =
            vcgtq_s16(vdupq_n_s16(max_base_x - base - 2 * c), iota2x8);
        const uint16x8_t res = vbslq_u16(cmp, val, vdupq_n_u16(above_max));
        vst1q_u16(dst + c, res);
        c += 8;
      } while (c < bw);
    }

    dst += stride;
    x += dx;
  } while (++r < bh);
}

// Directional prediction, zone 1: 0 < angle < 90
void av1_highbd_dr_prediction_z1_neon(uint16_t *dst, ptrdiff_t stride, int bw,
                                      int bh, const uint16_t *above,
                                      const uint16_t *left, int upsample_above,
                                      int dx, int dy, int bd) {
  (void)left;
  (void)dy;
  (void)bd;
  assert(dy == 1);

  if (upsample_above) {
    highbd_dr_prediction_z1_upsample1_neon(dst, stride, bw, bh, above, dx);
  } else {
    highbd_dr_prediction_z1_upsample0_neon(dst, stride, bw, bh, above, dx);
  }
}

// -----------------------------------------------------------------------------
// Z2

#if AOM_ARCH_AARCH64
// Incrementally shift more elements from `above` into the result, merging with
// existing `left` elements.
// X0, X1, X2, X3
// Y0, X0, X1, X2
// Y0, Y1, X0, X1
// Y0, Y1, Y2, X0
// Y0, Y1, Y2, Y3
// clang-format off
static const uint8_t z2_merge_shuffles_u16x4[5][8] = {
  {  8,  9, 10, 11, 12, 13, 14, 15 },
  {  0,  1,  8,  9, 10, 11, 12, 13 },
  {  0,  1,  2,  3,  8,  9, 10, 11 },
  {  0,  1,  2,  3,  4,  5,  8,  9 },
  {  0,  1,  2,  3,  4,  5,  6,  7 },
};
// clang-format on

// Incrementally shift more elements from `above` into the result, merging with
// existing `left` elements.
// X0, X1, X2, X3, X4, X5, X6, X7
// Y0, X0, X1, X2, X3, X4, X5, X6
// Y0, Y1, X0, X1, X2, X3, X4, X5
// Y0, Y1, Y2, X0, X1, X2, X3, X4
// Y0, Y1, Y2, Y3, X0, X1, X2, X3
// Y0, Y1, Y2, Y3, Y4, X0, X1, X2
// Y0, Y1, Y2, Y3, Y4, Y5, X0, X1
// Y0, Y1, Y2, Y3, Y4, Y5, Y6, X0
// Y0, Y1, Y2, Y3, Y4, Y5, Y6, Y7
// clang-format off
static const uint8_t z2_merge_shuffles_u16x8[9][16] = {
  { 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 },
  {  0,  1, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29 },
  {  0,  1,  2,  3, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27 },
  {  0,  1,  2,  3,  4,  5, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25 },
  {  0,  1,  2,  3,  4,  5,  6,  7, 16, 17, 18, 19, 20, 21, 22, 23 },
  {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 16, 17, 18, 19, 20, 21 },
  {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 16, 17, 18, 19 },
  {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 16, 17 },
  {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
};
// clang-format on

// clang-format off
static const uint16_t z2_y_iter_masks_u16x4[5][4] = {
  {      0U,      0U,      0U,      0U },
  { 0xffffU,      0U,      0U,      0U },
  { 0xffffU, 0xffffU,      0U,      0U },
  { 0xffffU, 0xffffU, 0xffffU,      0U },
  { 0xffffU, 0xffffU, 0xffffU, 0xffffU },
};
// clang-format on

// clang-format off
static const uint16_t z2_y_iter_masks_u16x8[9][8] = {
  {      0U,      0U,      0U,      0U,      0U,      0U,      0U,      0U },
  { 0xffffU,      0U,      0U,      0U,      0U,      0U,      0U,      0U },
  { 0xffffU, 0xffffU,      0U,      0U,      0U,      0U,      0U,      0U },
  { 0xffffU, 0xffffU, 0xffffU,      0U,      0U,      0U,      0U,      0U },
  { 0xffffU, 0xffffU, 0xffffU, 0xffffU,      0U,      0U,      0U,      0U },
  { 0xffffU, 0xffffU, 0xffffU, 0xffffU, 0xffffU,      0U,      0U,      0U },
  { 0xffffU, 0xffffU, 0xffffU, 0xffffU, 0xffffU, 0xffffU,      0U,      0U },
  { 0xffffU, 0xffffU, 0xffffU, 0xffffU, 0xffffU, 0xffffU, 0xffffU,      0U },
  { 0xffffU, 0xffffU, 0xffffU, 0xffffU, 0xffffU, 0xffffU, 0xffffU, 0xffffU },
};
// clang-format on

static AOM_FORCE_INLINE uint16x4_t highbd_dr_prediction_z2_tbl_left_x4_from_x8(
    const uint16x8_t left_data, const int16x4_t indices, int base, int n) {
  // Need to adjust indices to operate on 0-based indices rather than
  // `base`-based indices and then adjust from uint16x4 indices to uint8x8
  // indices so we can use a tbl instruction (which only operates on bytes).
  uint8x8_t left_indices =
      vreinterpret_u8_s16(vsub_s16(indices, vdup_n_s16(base)));
  left_indices = vtrn1_u8(left_indices, left_indices);
  left_indices = vadd_u8(left_indices, left_indices);
  left_indices = vadd_u8(left_indices, vreinterpret_u8_u16(vdup_n_u16(0x0100)));
  const uint16x4_t ret = vreinterpret_u16_u8(
      vqtbl1_u8(vreinterpretq_u8_u16(left_data), left_indices));
  return vand_u16(ret, vld1_u16(z2_y_iter_masks_u16x4[n]));
}

static AOM_FORCE_INLINE uint16x4_t highbd_dr_prediction_z2_tbl_left_x4_from_x16(
    const uint16x8x2_t left_data, const int16x4_t indices, int base, int n) {
  // Need to adjust indices to operate on 0-based indices rather than
  // `base`-based indices and then adjust from uint16x4 indices to uint8x8
  // indices so we can use a tbl instruction (which only operates on bytes).
  uint8x8_t left_indices =
      vreinterpret_u8_s16(vsub_s16(indices, vdup_n_s16(base)));
  left_indices = vtrn1_u8(left_indices, left_indices);
  left_indices = vadd_u8(left_indices, left_indices);
  left_indices = vadd_u8(left_indices, vreinterpret_u8_u16(vdup_n_u16(0x0100)));
  uint8x16x2_t data_u8 = { { vreinterpretq_u8_u16(left_data.val[0]),
                             vreinterpretq_u8_u16(left_data.val[1]) } };
  const uint16x4_t ret = vreinterpret_u16_u8(vqtbl2_u8(data_u8, left_indices));
  return vand_u16(ret, vld1_u16(z2_y_iter_masks_u16x4[n]));
}

static AOM_FORCE_INLINE uint16x8_t highbd_dr_prediction_z2_tbl_left_x8_from_x8(
    const uint16x8_t left_data, const int16x8_t indices, int base, int n) {
  // Need to adjust indices to operate on 0-based indices rather than
  // `base`-based indices and then adjust from uint16x4 indices to uint8x8
  // indices so we can use a tbl instruction (which only operates on bytes).
  uint8x16_t left_indices =
      vreinterpretq_u8_s16(vsubq_s16(indices, vdupq_n_s16(base)));
  left_indices = vtrn1q_u8(left_indices, left_indices);
  left_indices = vaddq_u8(left_indices, left_indices);
  left_indices =
      vaddq_u8(left_indices, vreinterpretq_u8_u16(vdupq_n_u16(0x0100)));
  const uint16x8_t ret = vreinterpretq_u16_u8(
      vqtbl1q_u8(vreinterpretq_u8_u16(left_data), left_indices));
  return vandq_u16(ret, vld1q_u16(z2_y_iter_masks_u16x8[n]));
}

static AOM_FORCE_INLINE uint16x8_t highbd_dr_prediction_z2_tbl_left_x8_from_x16(
    const uint16x8x2_t left_data, const int16x8_t indices, int base, int n) {
  // Need to adjust indices to operate on 0-based indices rather than
  // `base`-based indices and then adjust from uint16x4 indices to uint8x8
  // indices so we can use a tbl instruction (which only operates on bytes).
  uint8x16_t left_indices =
      vreinterpretq_u8_s16(vsubq_s16(indices, vdupq_n_s16(base)));
  left_indices = vtrn1q_u8(left_indices, left_indices);
  left_indices = vaddq_u8(left_indices, left_indices);
  left_indices =
      vaddq_u8(left_indices, vreinterpretq_u8_u16(vdupq_n_u16(0x0100)));
  uint8x16x2_t data_u8 = { { vreinterpretq_u8_u16(left_data.val[0]),
                             vreinterpretq_u8_u16(left_data.val[1]) } };
  const uint16x8_t ret =
      vreinterpretq_u16_u8(vqtbl2q_u8(data_u8, left_indices));
  return vandq_u16(ret, vld1q_u16(z2_y_iter_masks_u16x8[n]));
}
#endif  // AOM_ARCH_AARCH64

static AOM_FORCE_INLINE uint16x4x2_t highbd_dr_prediction_z2_gather_left_x4(
    const uint16_t *left, const int16x4_t indices, int n) {
  assert(n > 0);
  assert(n <= 4);
  // Load two elements at a time and then uzp them into separate vectors, to
  // reduce the number of memory accesses.
  uint32x2_t ret0_u32 = vdup_n_u32(0);
  uint32x2_t ret1_u32 = vdup_n_u32(0);

  // Use a single vget_lane_u64 to minimize vector to general purpose register
  // transfers and then mask off the bits we actually want.
  const uint64_t indices0123 = vget_lane_u64(vreinterpret_u64_s16(indices), 0);
  const int idx0 = (int16_t)((indices0123 >> 0) & 0xffffU);
  const int idx1 = (int16_t)((indices0123 >> 16) & 0xffffU);
  const int idx2 = (int16_t)((indices0123 >> 32) & 0xffffU);
  const int idx3 = (int16_t)((indices0123 >> 48) & 0xffffU);

  // At time of writing both Clang and GCC produced better code with these
  // nested if-statements compared to a switch statement with fallthrough.
  load_unaligned_u32_2x1_lane(ret0_u32, left + idx0, 0);
  if (n > 1) {
    load_unaligned_u32_2x1_lane(ret0_u32, left + idx1, 1);
    if (n > 2) {
      load_unaligned_u32_2x1_lane(ret1_u32, left + idx2, 0);
      if (n > 3) {
        load_unaligned_u32_2x1_lane(ret1_u32, left + idx3, 1);
      }
    }
  }
  return vuzp_u16(vreinterpret_u16_u32(ret0_u32),
                  vreinterpret_u16_u32(ret1_u32));
}

static AOM_FORCE_INLINE uint16x8x2_t highbd_dr_prediction_z2_gather_left_x8(
    const uint16_t *left, const int16x8_t indices, int n) {
  assert(n > 0);
  assert(n <= 8);
  // Load two elements at a time and then uzp them into separate vectors, to
  // reduce the number of memory accesses.
  uint32x4_t ret0_u32 = vdupq_n_u32(0);
  uint32x4_t ret1_u32 = vdupq_n_u32(0);

  // Use a pair of vget_lane_u64 to minimize vector to general purpose register
  // transfers and then mask off the bits we actually want.
  const uint64_t indices0123 =
      vgetq_lane_u64(vreinterpretq_u64_s16(indices), 0);
  const uint64_t indices4567 =
      vgetq_lane_u64(vreinterpretq_u64_s16(indices), 1);
  const int idx0 = (int16_t)((indices0123 >> 0) & 0xffffU);
  const int idx1 = (int16_t)((indices0123 >> 16) & 0xffffU);
  const int idx2 = (int16_t)((indices0123 >> 32) & 0xffffU);
  const int idx3 = (int16_t)((indices0123 >> 48) & 0xffffU);
  const int idx4 = (int16_t)((indices4567 >> 0) & 0xffffU);
  const int idx5 = (int16_t)((indices4567 >> 16) & 0xffffU);
  const int idx6 = (int16_t)((indices4567 >> 32) & 0xffffU);
  const int idx7 = (int16_t)((indices4567 >> 48) & 0xffffU);

  // At time of writing both Clang and GCC produced better code with these
  // nested if-statements compared to a switch statement with fallthrough.
  load_unaligned_u32_4x1_lane(ret0_u32, left + idx0, 0);
  if (n > 1) {
    load_unaligned_u32_4x1_lane(ret0_u32, left + idx1, 1);
    if (n > 2) {
      load_unaligned_u32_4x1_lane(ret0_u32, left + idx2, 2);
      if (n > 3) {
        load_unaligned_u32_4x1_lane(ret0_u32, left + idx3, 3);
        if (n > 4) {
          load_unaligned_u32_4x1_lane(ret1_u32, left + idx4, 0);
          if (n > 5) {
            load_unaligned_u32_4x1_lane(ret1_u32, left + idx5, 1);
            if (n > 6) {
              load_unaligned_u32_4x1_lane(ret1_u32, left + idx6, 2);
              if (n > 7) {
                load_unaligned_u32_4x1_lane(ret1_u32, left + idx7, 3);
              }
            }
          }
        }
      }
    }
  }
  return vuzpq_u16(vreinterpretq_u16_u32(ret0_u32),
                   vreinterpretq_u16_u32(ret1_u32));
}

static AOM_FORCE_INLINE uint16x4_t highbd_dr_prediction_z2_merge_x4(
    uint16x4_t out_x, uint16x4_t out_y, int base_shift) {
  assert(base_shift >= 0);
  assert(base_shift <= 4);
  // On AArch64 we can permute the data from the `above` and `left` vectors
  // into a single vector in a single load (of the permute vector) + tbl.
#if AOM_ARCH_AARCH64
  const uint8x8x2_t out_yx = { { vreinterpret_u8_u16(out_y),
                                 vreinterpret_u8_u16(out_x) } };
  return vreinterpret_u16_u8(
      vtbl2_u8(out_yx, vld1_u8(z2_merge_shuffles_u16x4[base_shift])));
#else
  uint16x4_t out = out_y;
  for (int c2 = base_shift, x_idx = 0; c2 < 4; ++c2, ++x_idx) {
    out[c2] = out_x[x_idx];
  }
  return out;
#endif
}

static AOM_FORCE_INLINE uint16x8_t highbd_dr_prediction_z2_merge_x8(
    uint16x8_t out_x, uint16x8_t out_y, int base_shift) {
  assert(base_shift >= 0);
  assert(base_shift <= 8);
  // On AArch64 we can permute the data from the `above` and `left` vectors
  // into a single vector in a single load (of the permute vector) + tbl.
#if AOM_ARCH_AARCH64
  const uint8x16x2_t out_yx = { { vreinterpretq_u8_u16(out_y),
                                  vreinterpretq_u8_u16(out_x) } };
  return vreinterpretq_u16_u8(
      vqtbl2q_u8(out_yx, vld1q_u8(z2_merge_shuffles_u16x8[base_shift])));
#else
  uint16x8_t out = out_y;
  for (int c2 = base_shift, x_idx = 0; c2 < 8; ++c2, ++x_idx) {
    out[c2] = out_x[x_idx];
  }
  return out;
#endif
}

static AOM_FORCE_INLINE uint16x4_t highbd_dr_prediction_z2_apply_shift_x4(
    uint16x4_t a0, uint16x4_t a1, int16x4_t shift) {
  uint32x4_t res = vmull_u16(a1, vreinterpret_u16_s16(shift));
  res =
      vmlal_u16(res, a0, vsub_u16(vdup_n_u16(32), vreinterpret_u16_s16(shift)));
  return vrshrn_n_u32(res, 5);
}

static AOM_FORCE_INLINE uint16x8_t highbd_dr_prediction_z2_apply_shift_x8(
    uint16x8_t a0, uint16x8_t a1, int16x8_t shift) {
  return vcombine_u16(
      highbd_dr_prediction_z2_apply_shift_x4(vget_low_u16(a0), vget_low_u16(a1),
                                             vget_low_s16(shift)),
      highbd_dr_prediction_z2_apply_shift_x4(
          vget_high_u16(a0), vget_high_u16(a1), vget_high_s16(shift)));
}

static AOM_FORCE_INLINE uint16x4_t highbd_dr_prediction_z2_step_x4(
    const uint16_t *above, const uint16x4_t above0, const uint16x4_t above1,
    const uint16_t *left, int dx, int dy, int r, int c) {
  const int16x4_t iota = vld1_s16(iota1_s16);

  const int x0 = (c << 6) - (r + 1) * dx;
  const int y0 = (r << 6) - (c + 1) * dy;

  const int16x4_t x0123 = vadd_s16(vdup_n_s16(x0), vshl_n_s16(iota, 6));
  const int16x4_t y0123 = vsub_s16(vdup_n_s16(y0), vmul_n_s16(iota, dy));
  const int16x4_t shift_x0123 =
      vshr_n_s16(vand_s16(x0123, vdup_n_s16(0x3F)), 1);
  const int16x4_t shift_y0123 =
      vshr_n_s16(vand_s16(y0123, vdup_n_s16(0x3F)), 1);
  const int16x4_t base_y0123 = vshr_n_s16(y0123, 6);

  const int base_shift = ((((r + 1) * dx) - 1) >> 6) - c;

  // Based on the value of `base_shift` there are three possible cases to
  // compute the result:
  // 1) base_shift <= 0: We can load and operate entirely on data from the
  //                     `above` input vector.
  // 2) base_shift < vl: We can load from `above[-1]` and shift
  //                     `vl - base_shift` elements across to the end of the
  //                     vector, then compute the remainder from `left`.
  // 3) base_shift >= vl: We can load and operate entirely on data from the
  //                      `left` input vector.

  if (base_shift <= 0) {
    const int base_x = x0 >> 6;
    const uint16x4_t a0 = vld1_u16(above + base_x);
    const uint16x4_t a1 = vld1_u16(above + base_x + 1);
    return highbd_dr_prediction_z2_apply_shift_x4(a0, a1, shift_x0123);
  } else if (base_shift < 4) {
    const uint16x4x2_t l01 = highbd_dr_prediction_z2_gather_left_x4(
        left + 1, base_y0123, base_shift);
    const uint16x4_t out16_y = highbd_dr_prediction_z2_apply_shift_x4(
        l01.val[0], l01.val[1], shift_y0123);

    // No need to reload from above in the loop, just use pre-loaded constants.
    const uint16x4_t out16_x =
        highbd_dr_prediction_z2_apply_shift_x4(above0, above1, shift_x0123);

    return highbd_dr_prediction_z2_merge_x4(out16_x, out16_y, base_shift);
  } else {
    const uint16x4x2_t l01 =
        highbd_dr_prediction_z2_gather_left_x4(left + 1, base_y0123, 4);
    return highbd_dr_prediction_z2_apply_shift_x4(l01.val[0], l01.val[1],
                                                  shift_y0123);
  }
}

static AOM_FORCE_INLINE uint16x8_t highbd_dr_prediction_z2_step_x8(
    const uint16_t *above, const uint16x8_t above0, const uint16x8_t above1,
    const uint16_t *left, int dx, int dy, int r, int c) {
  const int16x8_t iota = vld1q_s16(iota1_s16);

  const int x0 = (c << 6) - (r + 1) * dx;
  const int y0 = (r << 6) - (c + 1) * dy;

  const int16x8_t x01234567 = vaddq_s16(vdupq_n_s16(x0), vshlq_n_s16(iota, 6));
  const int16x8_t y01234567 = vsubq_s16(vdupq_n_s16(y0), vmulq_n_s16(iota, dy));
  const int16x8_t shift_x01234567 =
      vshrq_n_s16(vandq_s16(x01234567, vdupq_n_s16(0x3F)), 1);
  const int16x8_t shift_y01234567 =
      vshrq_n_s16(vandq_s16(y01234567, vdupq_n_s16(0x3F)), 1);
  const int16x8_t base_y01234567 = vshrq_n_s16(y01234567, 6);

  const int base_shift = ((((r + 1) * dx) - 1) >> 6) - c;

  // Based on the value of `base_shift` there are three possible cases to
  // compute the result:
  // 1) base_shift <= 0: We can load and operate entirely on data from the
  //                     `above` input vector.
  // 2) base_shift < vl: We can load from `above[-1]` and shift
  //                     `vl - base_shift` elements across to the end of the
  //                     vector, then compute the remainder from `left`.
  // 3) base_shift >= vl: We can load and operate entirely on data from the
  //                      `left` input vector.

  if (base_shift <= 0) {
    const int base_x = x0 >> 6;
    const uint16x8_t a0 = vld1q_u16(above + base_x);
    const uint16x8_t a1 = vld1q_u16(above + base_x + 1);
    return highbd_dr_prediction_z2_apply_shift_x8(a0, a1, shift_x01234567);
  } else if (base_shift < 8) {
    const uint16x8x2_t l01 = highbd_dr_prediction_z2_gather_left_x8(
        left + 1, base_y01234567, base_shift);
    const uint16x8_t out16_y = highbd_dr_prediction_z2_apply_shift_x8(
        l01.val[0], l01.val[1], shift_y01234567);

    // No need to reload from above in the loop, just use pre-loaded constants.
    const uint16x8_t out16_x =
        highbd_dr_prediction_z2_apply_shift_x8(above0, above1, shift_x01234567);

    return highbd_dr_prediction_z2_merge_x8(out16_x, out16_y, base_shift);
  } else {
    const uint16x8x2_t l01 =
        highbd_dr_prediction_z2_gather_left_x8(left + 1, base_y01234567, 8);
    return highbd_dr_prediction_z2_apply_shift_x8(l01.val[0], l01.val[1],
                                                  shift_y01234567);
  }
}

// Left array is accessed from -1 through `bh - 1` inclusive.
// Above array is accessed from -1 through `bw - 1` inclusive.
#define HIGHBD_DR_PREDICTOR_Z2_WXH(bw, bh)                                 \
  static void highbd_dr_prediction_z2_##bw##x##bh##_neon(                  \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,              \
      const uint16_t *left, int upsample_above, int upsample_left, int dx, \
      int dy, int bd) {                                                    \
    (void)bd;                                                              \
    (void)upsample_above;                                                  \
    (void)upsample_left;                                                   \
    assert(!upsample_above);                                               \
    assert(!upsample_left);                                                \
    assert(bw % 4 == 0);                                                   \
    assert(bh % 4 == 0);                                                   \
    assert(dx > 0);                                                        \
    assert(dy > 0);                                                        \
                                                                           \
    uint16_t left_data[bh + 1];                                            \
    memcpy(left_data, left - 1, (bh + 1) * sizeof(uint16_t));              \
                                                                           \
    uint16x8_t a0, a1;                                                     \
    if (bw == 4) {                                                         \
      a0 = vcombine_u16(vld1_u16(above - 1), vdup_n_u16(0));               \
      a1 = vcombine_u16(vld1_u16(above + 0), vdup_n_u16(0));               \
    } else {                                                               \
      a0 = vld1q_u16(above - 1);                                           \
      a1 = vld1q_u16(above + 0);                                           \
    }                                                                      \
                                                                           \
    int r = 0;                                                             \
    do {                                                                   \
      if (bw == 4) {                                                       \
        vst1_u16(dst, highbd_dr_prediction_z2_step_x4(                     \
                          above, vget_low_u16(a0), vget_low_u16(a1),       \
                          left_data, dx, dy, r, 0));                       \
      } else {                                                             \
        int c = 0;                                                         \
        do {                                                               \
          vst1q_u16(dst + c, highbd_dr_prediction_z2_step_x8(              \
                                 above, a0, a1, left_data, dx, dy, r, c)); \
          c += 8;                                                          \
        } while (c < bw);                                                  \
      }                                                                    \
      dst += stride;                                                       \
    } while (++r < bh);                                                    \
  }

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
HIGHBD_DR_PREDICTOR_Z2_WXH(4, 16)
HIGHBD_DR_PREDICTOR_Z2_WXH(8, 16)
HIGHBD_DR_PREDICTOR_Z2_WXH(8, 32)
HIGHBD_DR_PREDICTOR_Z2_WXH(16, 4)
HIGHBD_DR_PREDICTOR_Z2_WXH(16, 8)
HIGHBD_DR_PREDICTOR_Z2_WXH(16, 16)
HIGHBD_DR_PREDICTOR_Z2_WXH(16, 32)
HIGHBD_DR_PREDICTOR_Z2_WXH(16, 64)
HIGHBD_DR_PREDICTOR_Z2_WXH(32, 8)
HIGHBD_DR_PREDICTOR_Z2_WXH(32, 16)
HIGHBD_DR_PREDICTOR_Z2_WXH(32, 32)
HIGHBD_DR_PREDICTOR_Z2_WXH(32, 64)
HIGHBD_DR_PREDICTOR_Z2_WXH(64, 16)
HIGHBD_DR_PREDICTOR_Z2_WXH(64, 32)
HIGHBD_DR_PREDICTOR_Z2_WXH(64, 64)
#else
HIGHBD_DR_PREDICTOR_Z2_WXH(8, 16)
HIGHBD_DR_PREDICTOR_Z2_WXH(16, 8)
HIGHBD_DR_PREDICTOR_Z2_WXH(16, 16)
HIGHBD_DR_PREDICTOR_Z2_WXH(16, 32)
HIGHBD_DR_PREDICTOR_Z2_WXH(32, 32)
HIGHBD_DR_PREDICTOR_Z2_WXH(32, 64)
HIGHBD_DR_PREDICTOR_Z2_WXH(64, 32)
HIGHBD_DR_PREDICTOR_Z2_WXH(64, 64)
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

#undef HIGHBD_DR_PREDICTOR_Z2_WXH

typedef void (*highbd_dr_prediction_z2_ptr)(uint16_t *dst, ptrdiff_t stride,
                                            const uint16_t *above,
                                            const uint16_t *left,
                                            int upsample_above,
                                            int upsample_left, int dx, int dy,
                                            int bd);

static void highbd_dr_prediction_z2_4x4_neon(uint16_t *dst, ptrdiff_t stride,
                                             const uint16_t *above,
                                             const uint16_t *left,
                                             int upsample_above,
                                             int upsample_left, int dx, int dy,
                                             int bd) {
  (void)bd;
  assert(dx > 0);
  assert(dy > 0);

  const int frac_bits_x = 6 - upsample_above;
  const int frac_bits_y = 6 - upsample_left;
  const int min_base_x = -(1 << (upsample_above + frac_bits_x));

  // if `upsample_left` then we need -2 through 6 inclusive from `left`.
  // else we only need -1 through 3 inclusive.

#if AOM_ARCH_AARCH64
  uint16x8_t left_data0, left_data1;
  if (upsample_left) {
    left_data0 = vld1q_u16(left - 2);
    left_data1 = vld1q_u16(left - 1);
  } else {
    left_data0 = vcombine_u16(vld1_u16(left - 1), vdup_n_u16(0));
    left_data1 = vcombine_u16(vld1_u16(left + 0), vdup_n_u16(0));
  }
#endif

  const int16x4_t iota0123 = vld1_s16(iota1_s16);
  const int16x4_t iota1234 = vld1_s16(iota1_s16 + 1);

  for (int r = 0; r < 4; ++r) {
    const int base_shift = (min_base_x + (r + 1) * dx + 63) >> 6;
    const int x0 = (r + 1) * dx;
    const int16x4_t x0123 = vsub_s16(vshl_n_s16(iota0123, 6), vdup_n_s16(x0));
    const int base_x0 = (-x0) >> frac_bits_x;
    if (base_shift <= 0) {
      uint16x4_t a0, a1;
      int16x4_t shift_x0123;
      if (upsample_above) {
        const uint16x4x2_t a01 = vld2_u16(above + base_x0);
        a0 = a01.val[0];
        a1 = a01.val[1];
        shift_x0123 = vand_s16(x0123, vdup_n_s16(0x1F));
      } else {
        a0 = vld1_u16(above + base_x0);
        a1 = vld1_u16(above + base_x0 + 1);
        shift_x0123 = vshr_n_s16(vand_s16(x0123, vdup_n_s16(0x3F)), 1);
      }
      vst1_u16(dst,
               highbd_dr_prediction_z2_apply_shift_x4(a0, a1, shift_x0123));
    } else if (base_shift < 4) {
      // Calculate Y component from `left`.
      const int y_iters = base_shift;
      const int16x4_t y0123 =
          vsub_s16(vdup_n_s16(r << 6), vmul_n_s16(iota1234, dy));
      const int16x4_t base_y0123 = vshl_s16(y0123, vdup_n_s16(-frac_bits_y));
      const int16x4_t shift_y0123 = vshr_n_s16(
          vand_s16(vmul_n_s16(y0123, 1 << upsample_left), vdup_n_s16(0x3F)), 1);
      uint16x4_t l0, l1;
#if AOM_ARCH_AARCH64
      const int left_data_base = upsample_left ? -2 : -1;
      l0 = highbd_dr_prediction_z2_tbl_left_x4_from_x8(left_data0, base_y0123,
                                                       left_data_base, y_iters);
      l1 = highbd_dr_prediction_z2_tbl_left_x4_from_x8(left_data1, base_y0123,
                                                       left_data_base, y_iters);
#else
      const uint16x4x2_t l01 =
          highbd_dr_prediction_z2_gather_left_x4(left, base_y0123, y_iters);
      l0 = l01.val[0];
      l1 = l01.val[1];
#endif

      const uint16x4_t out_y =
          highbd_dr_prediction_z2_apply_shift_x4(l0, l1, shift_y0123);

      // Calculate X component from `above`.
      const int16x4_t shift_x0123 = vshr_n_s16(
          vand_s16(vmul_n_s16(x0123, 1 << upsample_above), vdup_n_s16(0x3F)),
          1);
      uint16x4_t a0, a1;
      if (upsample_above) {
        const uint16x4x2_t a01 = vld2_u16(above + (base_x0 % 2 == 0 ? -2 : -1));
        a0 = a01.val[0];
        a1 = a01.val[1];
      } else {
        a0 = vld1_u16(above - 1);
        a1 = vld1_u16(above + 0);
      }
      const uint16x4_t out_x =
          highbd_dr_prediction_z2_apply_shift_x4(a0, a1, shift_x0123);

      // Combine X and Y vectors.
      const uint16x4_t out =
          highbd_dr_prediction_z2_merge_x4(out_x, out_y, base_shift);
      vst1_u16(dst, out);
    } else {
      const int16x4_t y0123 =
          vsub_s16(vdup_n_s16(r << 6), vmul_n_s16(iota1234, dy));
      const int16x4_t base_y0123 = vshl_s16(y0123, vdup_n_s16(-frac_bits_y));
      const int16x4_t shift_y0123 = vshr_n_s16(
          vand_s16(vmul_n_s16(y0123, 1 << upsample_left), vdup_n_s16(0x3F)), 1);
      uint16x4_t l0, l1;
#if AOM_ARCH_AARCH64
      const int left_data_base = upsample_left ? -2 : -1;
      l0 = highbd_dr_prediction_z2_tbl_left_x4_from_x8(left_data0, base_y0123,
                                                       left_data_base, 4);
      l1 = highbd_dr_prediction_z2_tbl_left_x4_from_x8(left_data1, base_y0123,
                                                       left_data_base, 4);
#else
      const uint16x4x2_t l01 =
          highbd_dr_prediction_z2_gather_left_x4(left, base_y0123, 4);
      l0 = l01.val[0];
      l1 = l01.val[1];
#endif
      vst1_u16(dst,
               highbd_dr_prediction_z2_apply_shift_x4(l0, l1, shift_y0123));
    }
    dst += stride;
  }
}

static void highbd_dr_prediction_z2_4x8_neon(uint16_t *dst, ptrdiff_t stride,
                                             const uint16_t *above,
                                             const uint16_t *left,
                                             int upsample_above,
                                             int upsample_left, int dx, int dy,
                                             int bd) {
  (void)bd;
  assert(dx > 0);
  assert(dy > 0);

  const int frac_bits_x = 6 - upsample_above;
  const int frac_bits_y = 6 - upsample_left;
  const int min_base_x = -(1 << (upsample_above + frac_bits_x));

  // if `upsample_left` then we need -2 through 14 inclusive from `left`.
  // else we only need -1 through 6 inclusive.

#if AOM_ARCH_AARCH64
  uint16x8x2_t left_data0, left_data1;
  if (upsample_left) {
    left_data0 = vld1q_u16_x2(left - 2);
    left_data1 = vld1q_u16_x2(left - 1);
  } else {
    left_data0 = (uint16x8x2_t){ { vld1q_u16(left - 1), vdupq_n_u16(0) } };
    left_data1 = (uint16x8x2_t){ { vld1q_u16(left + 0), vdupq_n_u16(0) } };
  }
#endif

  const int16x4_t iota0123 = vld1_s16(iota1_s16);
  const int16x4_t iota1234 = vld1_s16(iota1_s16 + 1);

  for (int r = 0; r < 8; ++r) {
    const int base_shift = (min_base_x + (r + 1) * dx + 63) >> 6;
    const int x0 = (r + 1) * dx;
    const int16x4_t x0123 = vsub_s16(vshl_n_s16(iota0123, 6), vdup_n_s16(x0));
    const int base_x0 = (-x0) >> frac_bits_x;
    if (base_shift <= 0) {
      uint16x4_t a0, a1;
      int16x4_t shift_x0123;
      if (upsample_above) {
        const uint16x4x2_t a01 = vld2_u16(above + base_x0);
        a0 = a01.val[0];
        a1 = a01.val[1];
        shift_x0123 = vand_s16(x0123, vdup_n_s16(0x1F));
      } else {
        a0 = vld1_u16(above + base_x0);
        a1 = vld1_u16(above + base_x0 + 1);
        shift_x0123 = vand_s16(vshr_n_s16(x0123, 1), vdup_n_s16(0x1F));
      }
      vst1_u16(dst,
               highbd_dr_prediction_z2_apply_shift_x4(a0, a1, shift_x0123));
    } else if (base_shift < 4) {
      // Calculate Y component from `left`.
      const int y_iters = base_shift;
      const int16x4_t y0123 =
          vsub_s16(vdup_n_s16(r << 6), vmul_n_s16(iota1234, dy));
      const int16x4_t base_y0123 = vshl_s16(y0123, vdup_n_s16(-frac_bits_y));
      const int16x4_t shift_y0123 = vshr_n_s16(
          vand_s16(vmul_n_s16(y0123, 1 << upsample_left), vdup_n_s16(0x3F)), 1);

      uint16x4_t l0, l1;
#if AOM_ARCH_AARCH64
      const int left_data_base = upsample_left ? -2 : -1;
      l0 = highbd_dr_prediction_z2_tbl_left_x4_from_x16(
          left_data0, base_y0123, left_data_base, y_iters);
      l1 = highbd_dr_prediction_z2_tbl_left_x4_from_x16(
          left_data1, base_y0123, left_data_base, y_iters);
#else
      const uint16x4x2_t l01 =
          highbd_dr_prediction_z2_gather_left_x4(left, base_y0123, y_iters);
      l0 = l01.val[0];
      l1 = l01.val[1];
#endif

      const uint16x4_t out_y =
          highbd_dr_prediction_z2_apply_shift_x4(l0, l1, shift_y0123);

      // Calculate X component from `above`.
      uint16x4_t a0, a1;
      int16x4_t shift_x0123;
      if (upsample_above) {
        const uint16x4x2_t a01 = vld2_u16(above + (base_x0 % 2 == 0 ? -2 : -1));
        a0 = a01.val[0];
        a1 = a01.val[1];
        shift_x0123 = vand_s16(x0123, vdup_n_s16(0x1F));
      } else {
        a0 = vld1_u16(above - 1);
        a1 = vld1_u16(above + 0);
        shift_x0123 = vand_s16(vshr_n_s16(x0123, 1), vdup_n_s16(0x1F));
      }
      const uint16x4_t out_x =
          highbd_dr_prediction_z2_apply_shift_x4(a0, a1, shift_x0123);

      // Combine X and Y vectors.
      const uint16x4_t out =
          highbd_dr_prediction_z2_merge_x4(out_x, out_y, base_shift);
      vst1_u16(dst, out);
    } else {
      const int16x4_t y0123 =
          vsub_s16(vdup_n_s16(r << 6), vmul_n_s16(iota1234, dy));
      const int16x4_t base_y0123 = vshl_s16(y0123, vdup_n_s16(-frac_bits_y));
      const int16x4_t shift_y0123 = vshr_n_s16(
          vand_s16(vmul_n_s16(y0123, 1 << upsample_left), vdup_n_s16(0x3F)), 1);

      uint16x4_t l0, l1;
#if AOM_ARCH_AARCH64
      const int left_data_base = upsample_left ? -2 : -1;
      l0 = highbd_dr_prediction_z2_tbl_left_x4_from_x16(left_data0, base_y0123,
                                                        left_data_base, 4);
      l1 = highbd_dr_prediction_z2_tbl_left_x4_from_x16(left_data1, base_y0123,
                                                        left_data_base, 4);
#else
      const uint16x4x2_t l01 =
          highbd_dr_prediction_z2_gather_left_x4(left, base_y0123, 4);
      l0 = l01.val[0];
      l1 = l01.val[1];
#endif

      vst1_u16(dst,
               highbd_dr_prediction_z2_apply_shift_x4(l0, l1, shift_y0123));
    }
    dst += stride;
  }
}

static void highbd_dr_prediction_z2_8x4_neon(uint16_t *dst, ptrdiff_t stride,
                                             const uint16_t *above,
                                             const uint16_t *left,
                                             int upsample_above,
                                             int upsample_left, int dx, int dy,
                                             int bd) {
  (void)bd;
  assert(dx > 0);
  assert(dy > 0);

  const int frac_bits_x = 6 - upsample_above;
  const int frac_bits_y = 6 - upsample_left;
  const int min_base_x = -(1 << (upsample_above + frac_bits_x));

  // if `upsample_left` then we need -2 through 6 inclusive from `left`.
  // else we only need -1 through 3 inclusive.

#if AOM_ARCH_AARCH64
  uint16x8_t left_data0, left_data1;
  if (upsample_left) {
    left_data0 = vld1q_u16(left - 2);
    left_data1 = vld1q_u16(left - 1);
  } else {
    left_data0 = vcombine_u16(vld1_u16(left - 1), vdup_n_u16(0));
    left_data1 = vcombine_u16(vld1_u16(left + 0), vdup_n_u16(0));
  }
#endif

  const int16x8_t iota01234567 = vld1q_s16(iota1_s16);
  const int16x8_t iota12345678 = vld1q_s16(iota1_s16 + 1);

  for (int r = 0; r < 4; ++r) {
    const int base_shift = (min_base_x + (r + 1) * dx + 63) >> 6;
    const int x0 = (r + 1) * dx;
    const int16x8_t x01234567 =
        vsubq_s16(vshlq_n_s16(iota01234567, 6), vdupq_n_s16(x0));
    const int base_x0 = (-x0) >> frac_bits_x;
    if (base_shift <= 0) {
      uint16x8_t a0, a1;
      int16x8_t shift_x01234567;
      if (upsample_above) {
        const uint16x8x2_t a01 = vld2q_u16(above + base_x0);
        a0 = a01.val[0];
        a1 = a01.val[1];
        shift_x01234567 = vandq_s16(x01234567, vdupq_n_s16(0x1F));
      } else {
        a0 = vld1q_u16(above + base_x0);
        a1 = vld1q_u16(above + base_x0 + 1);
        shift_x01234567 =
            vandq_s16(vshrq_n_s16(x01234567, 1), vdupq_n_s16(0x1F));
      }
      vst1q_u16(
          dst, highbd_dr_prediction_z2_apply_shift_x8(a0, a1, shift_x01234567));
    } else if (base_shift < 8) {
      // Calculate Y component from `left`.
      const int y_iters = base_shift;
      const int16x8_t y01234567 =
          vsubq_s16(vdupq_n_s16(r << 6), vmulq_n_s16(iota12345678, dy));
      const int16x8_t base_y01234567 =
          vshlq_s16(y01234567, vdupq_n_s16(-frac_bits_y));
      const int16x8_t shift_y01234567 =
          vshrq_n_s16(vandq_s16(vmulq_n_s16(y01234567, 1 << upsample_left),
                                vdupq_n_s16(0x3F)),
                      1);

      uint16x8_t l0, l1;
#if AOM_ARCH_AARCH64
      const int left_data_base = upsample_left ? -2 : -1;
      l0 = highbd_dr_prediction_z2_tbl_left_x8_from_x8(
          left_data0, base_y01234567, left_data_base, y_iters);
      l1 = highbd_dr_prediction_z2_tbl_left_x8_from_x8(
          left_data1, base_y01234567, left_data_base, y_iters);
#else
      const uint16x8x2_t l01 =
          highbd_dr_prediction_z2_gather_left_x8(left, base_y01234567, y_iters);
      l0 = l01.val[0];
      l1 = l01.val[1];
#endif

      const uint16x8_t out_y =
          highbd_dr_prediction_z2_apply_shift_x8(l0, l1, shift_y01234567);

      // Calculate X component from `above`.
      uint16x8_t a0, a1;
      int16x8_t shift_x01234567;
      if (upsample_above) {
        const uint16x8x2_t a01 =
            vld2q_u16(above + (base_x0 % 2 == 0 ? -2 : -1));
        a0 = a01.val[0];
        a1 = a01.val[1];
        shift_x01234567 = vandq_s16(x01234567, vdupq_n_s16(0x1F));
      } else {
        a0 = vld1q_u16(above - 1);
        a1 = vld1q_u16(above + 0);
        shift_x01234567 =
            vandq_s16(vshrq_n_s16(x01234567, 1), vdupq_n_s16(0x1F));
      }
      const uint16x8_t out_x =
          highbd_dr_prediction_z2_apply_shift_x8(a0, a1, shift_x01234567);

      // Combine X and Y vectors.
      const uint16x8_t out =
          highbd_dr_prediction_z2_merge_x8(out_x, out_y, base_shift);
      vst1q_u16(dst, out);
    } else {
      const int16x8_t y01234567 =
          vsubq_s16(vdupq_n_s16(r << 6), vmulq_n_s16(iota12345678, dy));
      const int16x8_t base_y01234567 =
          vshlq_s16(y01234567, vdupq_n_s16(-frac_bits_y));
      const int16x8_t shift_y01234567 =
          vshrq_n_s16(vandq_s16(vmulq_n_s16(y01234567, 1 << upsample_left),
                                vdupq_n_s16(0x3F)),
                      1);

      uint16x8_t l0, l1;
#if AOM_ARCH_AARCH64
      const int left_data_base = upsample_left ? -2 : -1;
      l0 = highbd_dr_prediction_z2_tbl_left_x8_from_x8(
          left_data0, base_y01234567, left_data_base, 8);
      l1 = highbd_dr_prediction_z2_tbl_left_x8_from_x8(
          left_data1, base_y01234567, left_data_base, 8);
#else
      const uint16x8x2_t l01 =
          highbd_dr_prediction_z2_gather_left_x8(left, base_y01234567, 8);
      l0 = l01.val[0];
      l1 = l01.val[1];
#endif

      vst1q_u16(
          dst, highbd_dr_prediction_z2_apply_shift_x8(l0, l1, shift_y01234567));
    }
    dst += stride;
  }
}

static void highbd_dr_prediction_z2_8x8_neon(uint16_t *dst, ptrdiff_t stride,
                                             const uint16_t *above,
                                             const uint16_t *left,
                                             int upsample_above,
                                             int upsample_left, int dx, int dy,
                                             int bd) {
  (void)bd;
  assert(dx > 0);
  assert(dy > 0);

  const int frac_bits_x = 6 - upsample_above;
  const int frac_bits_y = 6 - upsample_left;
  const int min_base_x = -(1 << (upsample_above + frac_bits_x));

  // if `upsample_left` then we need -2 through 14 inclusive from `left`.
  // else we only need -1 through 6 inclusive.

#if AOM_ARCH_AARCH64
  uint16x8x2_t left_data0, left_data1;
  if (upsample_left) {
    left_data0 = vld1q_u16_x2(left - 2);
    left_data1 = vld1q_u16_x2(left - 1);
  } else {
    left_data0 = (uint16x8x2_t){ { vld1q_u16(left - 1), vdupq_n_u16(0) } };
    left_data1 = (uint16x8x2_t){ { vld1q_u16(left + 0), vdupq_n_u16(0) } };
  }
#endif

  const int16x8_t iota01234567 = vld1q_s16(iota1_s16);
  const int16x8_t iota12345678 = vld1q_s16(iota1_s16 + 1);

  for (int r = 0; r < 8; ++r) {
    const int base_shift = (min_base_x + (r + 1) * dx + 63) >> 6;
    const int x0 = (r + 1) * dx;
    const int16x8_t x01234567 =
        vsubq_s16(vshlq_n_s16(iota01234567, 6), vdupq_n_s16(x0));
    const int base_x0 = (-x0) >> frac_bits_x;
    if (base_shift <= 0) {
      uint16x8_t a0, a1;
      int16x8_t shift_x01234567;
      if (upsample_above) {
        const uint16x8x2_t a01 = vld2q_u16(above + base_x0);
        a0 = a01.val[0];
        a1 = a01.val[1];
        shift_x01234567 = vandq_s16(x01234567, vdupq_n_s16(0x1F));
      } else {
        a0 = vld1q_u16(above + base_x0);
        a1 = vld1q_u16(above + base_x0 + 1);
        shift_x01234567 =
            vandq_s16(vshrq_n_s16(x01234567, 1), vdupq_n_s16(0x1F));
      }
      vst1q_u16(
          dst, highbd_dr_prediction_z2_apply_shift_x8(a0, a1, shift_x01234567));
    } else if (base_shift < 8) {
      // Calculate Y component from `left`.
      const int y_iters = base_shift;
      const int16x8_t y01234567 =
          vsubq_s16(vdupq_n_s16(r << 6), vmulq_n_s16(iota12345678, dy));
      const int16x8_t base_y01234567 =
          vshlq_s16(y01234567, vdupq_n_s16(-frac_bits_y));
      const int16x8_t shift_y01234567 =
          vshrq_n_s16(vandq_s16(vmulq_n_s16(y01234567, 1 << upsample_left),
                                vdupq_n_s16(0x3F)),
                      1);

      uint16x8_t l0, l1;
#if AOM_ARCH_AARCH64
      const int left_data_base = upsample_left ? -2 : -1;
      l0 = highbd_dr_prediction_z2_tbl_left_x8_from_x16(
          left_data0, base_y01234567, left_data_base, y_iters);
      l1 = highbd_dr_prediction_z2_tbl_left_x8_from_x16(
          left_data1, base_y01234567, left_data_base, y_iters);
#else
      const uint16x8x2_t l01 =
          highbd_dr_prediction_z2_gather_left_x8(left, base_y01234567, y_iters);
      l0 = l01.val[0];
      l1 = l01.val[1];
#endif

      const uint16x8_t out_y =
          highbd_dr_prediction_z2_apply_shift_x8(l0, l1, shift_y01234567);

      // Calculate X component from `above`.
      uint16x8_t a0, a1;
      int16x8_t shift_x01234567;
      if (upsample_above) {
        const uint16x8x2_t a01 =
            vld2q_u16(above + (base_x0 % 2 == 0 ? -2 : -1));
        a0 = a01.val[0];
        a1 = a01.val[1];
        shift_x01234567 = vandq_s16(x01234567, vdupq_n_s16(0x1F));
      } else {
        a0 = vld1q_u16(above - 1);
        a1 = vld1q_u16(above + 0);
        shift_x01234567 =
            vandq_s16(vshrq_n_s16(x01234567, 1), vdupq_n_s16(0x1F));
      }
      const uint16x8_t out_x =
          highbd_dr_prediction_z2_apply_shift_x8(a0, a1, shift_x01234567);

      // Combine X and Y vectors.
      const uint16x8_t out =
          highbd_dr_prediction_z2_merge_x8(out_x, out_y, base_shift);
      vst1q_u16(dst, out);
    } else {
      const int16x8_t y01234567 =
          vsubq_s16(vdupq_n_s16(r << 6), vmulq_n_s16(iota12345678, dy));
      const int16x8_t base_y01234567 =
          vshlq_s16(y01234567, vdupq_n_s16(-frac_bits_y));
      const int16x8_t shift_y01234567 =
          vshrq_n_s16(vandq_s16(vmulq_n_s16(y01234567, 1 << upsample_left),
                                vdupq_n_s16(0x3F)),
                      1);

      uint16x8_t l0, l1;
#if AOM_ARCH_AARCH64
      const int left_data_base = upsample_left ? -2 : -1;
      l0 = highbd_dr_prediction_z2_tbl_left_x8_from_x16(
          left_data0, base_y01234567, left_data_base, 8);
      l1 = highbd_dr_prediction_z2_tbl_left_x8_from_x16(
          left_data1, base_y01234567, left_data_base, 8);
#else
      const uint16x8x2_t l01 =
          highbd_dr_prediction_z2_gather_left_x8(left, base_y01234567, 8);
      l0 = l01.val[0];
      l1 = l01.val[1];
#endif

      vst1q_u16(
          dst, highbd_dr_prediction_z2_apply_shift_x8(l0, l1, shift_y01234567));
    }
    dst += stride;
  }
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
static highbd_dr_prediction_z2_ptr dr_predictor_z2_arr_neon[7][7] = {
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  { NULL, NULL, &highbd_dr_prediction_z2_4x4_neon,
    &highbd_dr_prediction_z2_4x8_neon, &highbd_dr_prediction_z2_4x16_neon, NULL,
    NULL },
  { NULL, NULL, &highbd_dr_prediction_z2_8x4_neon,
    &highbd_dr_prediction_z2_8x8_neon, &highbd_dr_prediction_z2_8x16_neon,
    &highbd_dr_prediction_z2_8x32_neon, NULL },
  { NULL, NULL, &highbd_dr_prediction_z2_16x4_neon,
    &highbd_dr_prediction_z2_16x8_neon, &highbd_dr_prediction_z2_16x16_neon,
    &highbd_dr_prediction_z2_16x32_neon, &highbd_dr_prediction_z2_16x64_neon },
  { NULL, NULL, NULL, &highbd_dr_prediction_z2_32x8_neon,
    &highbd_dr_prediction_z2_32x16_neon, &highbd_dr_prediction_z2_32x32_neon,
    &highbd_dr_prediction_z2_32x64_neon },
  { NULL, NULL, NULL, NULL, &highbd_dr_prediction_z2_64x16_neon,
    &highbd_dr_prediction_z2_64x32_neon, &highbd_dr_prediction_z2_64x64_neon },
};
#else
static highbd_dr_prediction_z2_ptr dr_predictor_z2_arr_neon[7][7] = {
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  { NULL, NULL, &highbd_dr_prediction_z2_4x4_neon,
    &highbd_dr_prediction_z2_4x8_neon, NULL, NULL, NULL },
  { NULL, NULL, &highbd_dr_prediction_z2_8x4_neon,
    &highbd_dr_prediction_z2_8x8_neon, &highbd_dr_prediction_z2_8x16_neon, NULL,
    NULL },
  { NULL, NULL, NULL, &highbd_dr_prediction_z2_16x8_neon,
    &highbd_dr_prediction_z2_16x16_neon, &highbd_dr_prediction_z2_16x32_neon,
    NULL },
  { NULL, NULL, NULL, NULL, NULL, &highbd_dr_prediction_z2_32x32_neon,
    &highbd_dr_prediction_z2_32x64_neon },
  { NULL, NULL, NULL, NULL, NULL, &highbd_dr_prediction_z2_64x32_neon,
    &highbd_dr_prediction_z2_64x64_neon },
};
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

// Directional prediction, zone 2: 90 < angle < 180
void av1_highbd_dr_prediction_z2_neon(uint16_t *dst, ptrdiff_t stride, int bw,
                                      int bh, const uint16_t *above,
                                      const uint16_t *left, int upsample_above,
                                      int upsample_left, int dx, int dy,
                                      int bd) {
  highbd_dr_prediction_z2_ptr f =
      dr_predictor_z2_arr_neon[get_msb(bw)][get_msb(bh)];
  assert(f != NULL);
  f(dst, stride, above, left, upsample_above, upsample_left, dx, dy, bd);
}

// -----------------------------------------------------------------------------
// Z3

// Both the lane to the use and the shift amount must be immediates.
#define HIGHBD_DR_PREDICTOR_Z3_STEP_X4(out, iota, base, in0, in1, s0, s1, \
                                       lane, shift)                       \
  do {                                                                    \
    uint32x4_t val = vmull_lane_u16((in0), (s0), (lane));                 \
    val = vmlal_lane_u16(val, (in1), (s1), (lane));                       \
    const uint16x4_t cmp = vadd_u16((iota), vdup_n_u16(base));            \
    const uint16x4_t res = vrshrn_n_u32(val, (shift));                    \
    *(out) = vbsl_u16(vclt_u16(cmp, vdup_n_u16(max_base_y)), res,         \
                      vdup_n_u16(left_max));                              \
  } while (0)

#define HIGHBD_DR_PREDICTOR_Z3_STEP_X8(out, iota, base, in0, in1, s0, s1, \
                                       lane, shift)                       \
  do {                                                                    \
    uint32x4_t val_lo = vmull_lane_u16(vget_low_u16(in0), (s0), (lane));  \
    val_lo = vmlal_lane_u16(val_lo, vget_low_u16(in1), (s1), (lane));     \
    uint32x4_t val_hi = vmull_lane_u16(vget_high_u16(in0), (s0), (lane)); \
    val_hi = vmlal_lane_u16(val_hi, vget_high_u16(in1), (s1), (lane));    \
    *(out) = vcombine_u16(vrshrn_n_u32(val_lo, (shift)),                  \
                          vrshrn_n_u32(val_hi, (shift)));                 \
  } while (0)

static inline uint16x8x2_t z3_load_left_neon(const uint16_t *left0, int ofs,
                                             int max_ofs) {
  uint16x8_t r0;
  uint16x8_t r1;
  if (ofs + 7 >= max_ofs) {
    int shuffle_idx = max_ofs - ofs;
    r0 = zn_load_masked_neon(left0 + (max_ofs - 7), shuffle_idx);
  } else {
    r0 = vld1q_u16(left0 + ofs);
  }
  if (ofs + 8 >= max_ofs) {
    int shuffle_idx = max_ofs - ofs - 1;
    r1 = zn_load_masked_neon(left0 + (max_ofs - 7), shuffle_idx);
  } else {
    r1 = vld1q_u16(left0 + ofs + 1);
  }
  return (uint16x8x2_t){ { r0, r1 } };
}

static void highbd_dr_prediction_z3_upsample0_neon(uint16_t *dst,
                                                   ptrdiff_t stride, int bw,
                                                   int bh, const uint16_t *left,
                                                   int dy) {
  assert(bw % 4 == 0);
  assert(bh % 4 == 0);
  assert(dy > 0);

  // Factor out left + 1 to give the compiler a better chance of recognising
  // that the offsets used for the loads from left and left + 1 are otherwise
  // identical.
  const uint16_t *left1 = left + 1;

  const int max_base_y = (bw + bh - 1);
  const int left_max = left[max_base_y];
  const int frac_bits = 6;

  const uint16x8_t iota1x8 = vreinterpretq_u16_s16(vld1q_s16(iota1_s16));
  const uint16x4_t iota1x4 = vget_low_u16(iota1x8);

  // The C implementation of the z3 predictor when not upsampling uses:
  // ((y & 0x3f) >> 1)
  // The right shift is unnecessary here since we instead shift by +1 later,
  // so adjust the mask to 0x3e to ensure we don't consider the extra bit.
  const uint16x4_t shift_mask = vdup_n_u16(0x3e);

  if (bh == 4) {
    int y = dy;
    int c = 0;
    do {
      // Fully unroll the 4x4 block to allow us to use immediate lane-indexed
      // multiply instructions.
      const uint16x4_t shifts1 =
          vand_u16(vmla_n_u16(vdup_n_u16(y), iota1x4, dy), shift_mask);
      const uint16x4_t shifts0 = vsub_u16(vdup_n_u16(64), shifts1);
      const int base0 = (y + 0 * dy) >> frac_bits;
      const int base1 = (y + 1 * dy) >> frac_bits;
      const int base2 = (y + 2 * dy) >> frac_bits;
      const int base3 = (y + 3 * dy) >> frac_bits;
      uint16x4_t out[4];
      if (base0 >= max_base_y) {
        out[0] = vdup_n_u16(left_max);
      } else {
        const uint16x4_t l00 = vld1_u16(left + base0);
        const uint16x4_t l01 = vld1_u16(left1 + base0);
        HIGHBD_DR_PREDICTOR_Z3_STEP_X4(&out[0], iota1x4, base0, l00, l01,
                                       shifts0, shifts1, 0, 6);
      }
      if (base1 >= max_base_y) {
        out[1] = vdup_n_u16(left_max);
      } else {
        const uint16x4_t l10 = vld1_u16(left + base1);
        const uint16x4_t l11 = vld1_u16(left1 + base1);
        HIGHBD_DR_PREDICTOR_Z3_STEP_X4(&out[1], iota1x4, base1, l10, l11,
                                       shifts0, shifts1, 1, 6);
      }
      if (base2 >= max_base_y) {
        out[2] = vdup_n_u16(left_max);
      } else {
        const uint16x4_t l20 = vld1_u16(left + base2);
        const uint16x4_t l21 = vld1_u16(left1 + base2);
        HIGHBD_DR_PREDICTOR_Z3_STEP_X4(&out[2], iota1x4, base2, l20, l21,
                                       shifts0, shifts1, 2, 6);
      }
      if (base3 >= max_base_y) {
        out[3] = vdup_n_u16(left_max);
      } else {
        const uint16x4_t l30 = vld1_u16(left + base3);
        const uint16x4_t l31 = vld1_u16(left1 + base3);
        HIGHBD_DR_PREDICTOR_Z3_STEP_X4(&out[3], iota1x4, base3, l30, l31,
                                       shifts0, shifts1, 3, 6);
      }
      transpose_array_inplace_u16_4x4(out);
      for (int r2 = 0; r2 < 4; ++r2) {
        vst1_u16(dst + r2 * stride + c, out[r2]);
      }
      y += 4 * dy;
      c += 4;
    } while (c < bw);
  } else {
    int y = dy;
    int c = 0;
    do {
      int r = 0;
      do {
        // Fully unroll the 4x4 block to allow us to use immediate lane-indexed
        // multiply instructions.
        const uint16x4_t shifts1 =
            vand_u16(vmla_n_u16(vdup_n_u16(y), iota1x4, dy), shift_mask);
        const uint16x4_t shifts0 = vsub_u16(vdup_n_u16(64), shifts1);
        const int base0 = ((y + 0 * dy) >> frac_bits) + r;
        const int base1 = ((y + 1 * dy) >> frac_bits) + r;
        const int base2 = ((y + 2 * dy) >> frac_bits) + r;
        const int base3 = ((y + 3 * dy) >> frac_bits) + r;
        uint16x8_t out[4];
        if (base0 >= max_base_y) {
          out[0] = vdupq_n_u16(left_max);
        } else {
          const uint16x8x2_t l0 = z3_load_left_neon(left, base0, max_base_y);
          HIGHBD_DR_PREDICTOR_Z3_STEP_X8(&out[0], iota1x8, base0, l0.val[0],
                                         l0.val[1], shifts0, shifts1, 0, 6);
        }
        if (base1 >= max_base_y) {
          out[1] = vdupq_n_u16(left_max);
        } else {
          const uint16x8x2_t l1 = z3_load_left_neon(left, base1, max_base_y);
          HIGHBD_DR_PREDICTOR_Z3_STEP_X8(&out[1], iota1x8, base1, l1.val[0],
                                         l1.val[1], shifts0, shifts1, 1, 6);
        }
        if (base2 >= max_base_y) {
          out[2] = vdupq_n_u16(left_max);
        } else {
          const uint16x8x2_t l2 = z3_load_left_neon(left, base2, max_base_y);
          HIGHBD_DR_PREDICTOR_Z3_STEP_X8(&out[2], iota1x8, base2, l2.val[0],
                                         l2.val[1], shifts0, shifts1, 2, 6);
        }
        if (base3 >= max_base_y) {
          out[3] = vdupq_n_u16(left_max);
        } else {
          const uint16x8x2_t l3 = z3_load_left_neon(left, base3, max_base_y);
          HIGHBD_DR_PREDICTOR_Z3_STEP_X8(&out[3], iota1x8, base3, l3.val[0],
                                         l3.val[1], shifts0, shifts1, 3, 6);
        }
        transpose_array_inplace_u16_4x8(out);
        for (int r2 = 0; r2 < 4; ++r2) {
          vst1_u16(dst + (r + r2) * stride + c, vget_low_u16(out[r2]));
        }
        for (int r2 = 0; r2 < 4; ++r2) {
          vst1_u16(dst + (r + r2 + 4) * stride + c, vget_high_u16(out[r2]));
        }
        r += 8;
      } while (r < bh);
      y += 4 * dy;
      c += 4;
    } while (c < bw);
  }
}

static void highbd_dr_prediction_z3_upsample1_neon(uint16_t *dst,
                                                   ptrdiff_t stride, int bw,
                                                   int bh, const uint16_t *left,
                                                   int dy) {
  assert(bw % 4 == 0);
  assert(bh % 4 == 0);
  assert(dy > 0);

  const int max_base_y = (bw + bh - 1) << 1;
  const int left_max = left[max_base_y];
  const int frac_bits = 5;

  const uint16x4_t iota1x4 = vreinterpret_u16_s16(vld1_s16(iota1_s16));
  const uint16x8_t iota2x8 = vreinterpretq_u16_s16(vld1q_s16(iota2_s16));
  const uint16x4_t iota2x4 = vget_low_u16(iota2x8);

  // The C implementation of the z3 predictor when upsampling uses:
  // (((x << 1) & 0x3f) >> 1)
  // The two shifts are unnecessary here since the lowest bit is guaranteed to
  // be zero when the mask is applied, so adjust the mask to 0x1f to avoid
  // needing the shifts at all.
  const uint16x4_t shift_mask = vdup_n_u16(0x1F);

  if (bh == 4) {
    int y = dy;
    int c = 0;
    do {
      // Fully unroll the 4x4 block to allow us to use immediate lane-indexed
      // multiply instructions.
      const uint16x4_t shifts1 =
          vand_u16(vmla_n_u16(vdup_n_u16(y), iota1x4, dy), shift_mask);
      const uint16x4_t shifts0 = vsub_u16(vdup_n_u16(32), shifts1);
      const int base0 = (y + 0 * dy) >> frac_bits;
      const int base1 = (y + 1 * dy) >> frac_bits;
      const int base2 = (y + 2 * dy) >> frac_bits;
      const int base3 = (y + 3 * dy) >> frac_bits;
      const uint16x4x2_t l0 = vld2_u16(left + base0);
      const uint16x4x2_t l1 = vld2_u16(left + base1);
      const uint16x4x2_t l2 = vld2_u16(left + base2);
      const uint16x4x2_t l3 = vld2_u16(left + base3);
      uint16x4_t out[4];
      HIGHBD_DR_PREDICTOR_Z3_STEP_X4(&out[0], iota2x4, base0, l0.val[0],
                                     l0.val[1], shifts0, shifts1, 0, 5);
      HIGHBD_DR_PREDICTOR_Z3_STEP_X4(&out[1], iota2x4, base1, l1.val[0],
                                     l1.val[1], shifts0, shifts1, 1, 5);
      HIGHBD_DR_PREDICTOR_Z3_STEP_X4(&out[2], iota2x4, base2, l2.val[0],
                                     l2.val[1], shifts0, shifts1, 2, 5);
      HIGHBD_DR_PREDICTOR_Z3_STEP_X4(&out[3], iota2x4, base3, l3.val[0],
                                     l3.val[1], shifts0, shifts1, 3, 5);
      transpose_array_inplace_u16_4x4(out);
      for (int r2 = 0; r2 < 4; ++r2) {
        vst1_u16(dst + r2 * stride + c, out[r2]);
      }
      y += 4 * dy;
      c += 4;
    } while (c < bw);
  } else {
    assert(bh % 8 == 0);

    int y = dy;
    int c = 0;
    do {
      int r = 0;
      do {
        // Fully unroll the 4x8 block to allow us to use immediate lane-indexed
        // multiply instructions.
        const uint16x4_t shifts1 =
            vand_u16(vmla_n_u16(vdup_n_u16(y), iota1x4, dy), shift_mask);
        const uint16x4_t shifts0 = vsub_u16(vdup_n_u16(32), shifts1);
        const int base0 = ((y + 0 * dy) >> frac_bits) + (r * 2);
        const int base1 = ((y + 1 * dy) >> frac_bits) + (r * 2);
        const int base2 = ((y + 2 * dy) >> frac_bits) + (r * 2);
        const int base3 = ((y + 3 * dy) >> frac_bits) + (r * 2);
        const uint16x8x2_t l0 = vld2q_u16(left + base0);
        const uint16x8x2_t l1 = vld2q_u16(left + base1);
        const uint16x8x2_t l2 = vld2q_u16(left + base2);
        const uint16x8x2_t l3 = vld2q_u16(left + base3);
        uint16x8_t out[4];
        HIGHBD_DR_PREDICTOR_Z3_STEP_X8(&out[0], iota2x8, base0, l0.val[0],
                                       l0.val[1], shifts0, shifts1, 0, 5);
        HIGHBD_DR_PREDICTOR_Z3_STEP_X8(&out[1], iota2x8, base1, l1.val[0],
                                       l1.val[1], shifts0, shifts1, 1, 5);
        HIGHBD_DR_PREDICTOR_Z3_STEP_X8(&out[2], iota2x8, base2, l2.val[0],
                                       l2.val[1], shifts0, shifts1, 2, 5);
        HIGHBD_DR_PREDICTOR_Z3_STEP_X8(&out[3], iota2x8, base3, l3.val[0],
                                       l3.val[1], shifts0, shifts1, 3, 5);
        transpose_array_inplace_u16_4x8(out);
        for (int r2 = 0; r2 < 4; ++r2) {
          vst1_u16(dst + (r + r2) * stride + c, vget_low_u16(out[r2]));
        }
        for (int r2 = 0; r2 < 4; ++r2) {
          vst1_u16(dst + (r + r2 + 4) * stride + c, vget_high_u16(out[r2]));
        }
        r += 8;
      } while (r < bh);
      y += 4 * dy;
      c += 4;
    } while (c < bw);
  }
}

// Directional prediction, zone 3: 180 < angle < 270
void av1_highbd_dr_prediction_z3_neon(uint16_t *dst, ptrdiff_t stride, int bw,
                                      int bh, const uint16_t *above,
                                      const uint16_t *left, int upsample_left,
                                      int dx, int dy, int bd) {
  (void)above;
  (void)dx;
  (void)bd;
  assert(bw % 4 == 0);
  assert(bh % 4 == 0);
  assert(dx == 1);
  assert(dy > 0);

  if (upsample_left) {
    highbd_dr_prediction_z3_upsample1_neon(dst, stride, bw, bh, left, dy);
  } else {
    highbd_dr_prediction_z3_upsample0_neon(dst, stride, bw, bh, left, dy);
  }
}

#undef HIGHBD_DR_PREDICTOR_Z3_STEP_X4
#undef HIGHBD_DR_PREDICTOR_Z3_STEP_X8
