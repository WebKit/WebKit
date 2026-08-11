/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>

#include "aom_dsp/arm/aom_neon_sve_bridge.h"
#include "aom_dsp/arm/mem_neon.h"
#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

static inline uint64_t aom_sum_squares_2d_i16_4xh_sve(const int16_t *src,
                                                      int stride, int height) {
  int64x2_t sum_squares = vdupq_n_s64(0);

  do {
    int16x8_t s = vcombine_s16(vld1_s16(src), vld1_s16(src + stride));

    sum_squares = aom_sdotq_s16(sum_squares, s, s);

    src += 2 * stride;
    height -= 2;
  } while (height != 0);

  return (uint64_t)vaddvq_s64(sum_squares);
}

static inline uint64_t aom_sum_squares_2d_i16_8xh_sve(const int16_t *src,
                                                      int stride, int height) {
  int64x2_t sum_squares[2] = { vdupq_n_s64(0), vdupq_n_s64(0) };

  do {
    int16x8_t s0 = vld1q_s16(src + 0 * stride);
    int16x8_t s1 = vld1q_s16(src + 1 * stride);

    sum_squares[0] = aom_sdotq_s16(sum_squares[0], s0, s0);
    sum_squares[1] = aom_sdotq_s16(sum_squares[1], s1, s1);

    src += 2 * stride;
    height -= 2;
  } while (height != 0);

  sum_squares[0] = vaddq_s64(sum_squares[0], sum_squares[1]);
  return (uint64_t)vaddvq_s64(sum_squares[0]);
}

static inline uint64_t aom_sum_squares_2d_i16_large_sve(const int16_t *src,
                                                        int stride, int width,
                                                        int height) {
  int64x2_t sum_squares[2] = { vdupq_n_s64(0), vdupq_n_s64(0) };

  do {
    const int16_t *src_ptr = src;
    int w = width;
    do {
      int16x8_t s0 = vld1q_s16(src_ptr);
      int16x8_t s1 = vld1q_s16(src_ptr + 8);

      sum_squares[0] = aom_sdotq_s16(sum_squares[0], s0, s0);
      sum_squares[1] = aom_sdotq_s16(sum_squares[1], s1, s1);

      src_ptr += 16;
      w -= 16;
    } while (w != 0);

    src += stride;
  } while (--height != 0);

  sum_squares[0] = vaddq_s64(sum_squares[0], sum_squares[1]);
  return (uint64_t)vaddvq_s64(sum_squares[0]);
}

static inline uint64_t aom_sum_squares_2d_i16_wxh_sve(const int16_t *src,
                                                      int stride, int width,
                                                      int height) {
  svint64_t sum_squares = svdup_n_s64(0);
  uint64_t step = svcnth();

  do {
    const int16_t *src_ptr = src;
    int w = 0;
    do {
      svbool_t pred = svwhilelt_b16_u32(w, width);
      svint16_t s0 = svld1_s16(pred, src_ptr);

      sum_squares = svdot_s64(sum_squares, s0, s0);

      src_ptr += step;
      w += step;
    } while (w < width);

    src += stride;
  } while (--height != 0);

  return (uint64_t)svaddv_s64(svptrue_b64(), sum_squares);
}

uint64_t aom_sum_squares_2d_i16_sve(const int16_t *src, int stride, int width,
                                    int height) {
  if (width == 4) {
    return aom_sum_squares_2d_i16_4xh_sve(src, stride, height);
  }
  if (width == 8) {
    return aom_sum_squares_2d_i16_8xh_sve(src, stride, height);
  }
  if (width % 16 == 0) {
    return aom_sum_squares_2d_i16_large_sve(src, stride, width, height);
  }
  return aom_sum_squares_2d_i16_wxh_sve(src, stride, width, height);
}

uint64_t aom_sum_squares_i16_sve(const int16_t *src, uint32_t n) {
  // This function seems to be called only for values of N >= 64. See
  // av1/encoder/compound_type.c. Additionally, because N = width x height for
  // width and height between the standard block sizes, N will also be a
  // multiple of 64.
  if (LIKELY(n % 64 == 0)) {
    int64x2_t sum[4] = { vdupq_n_s64(0), vdupq_n_s64(0), vdupq_n_s64(0),
                         vdupq_n_s64(0) };

    do {
      int16x8_t s0 = vld1q_s16(src);
      int16x8_t s1 = vld1q_s16(src + 8);
      int16x8_t s2 = vld1q_s16(src + 16);
      int16x8_t s3 = vld1q_s16(src + 24);

      sum[0] = aom_sdotq_s16(sum[0], s0, s0);
      sum[1] = aom_sdotq_s16(sum[1], s1, s1);
      sum[2] = aom_sdotq_s16(sum[2], s2, s2);
      sum[3] = aom_sdotq_s16(sum[3], s3, s3);

      src += 32;
      n -= 32;
    } while (n != 0);

    sum[0] = vaddq_s64(sum[0], sum[1]);
    sum[2] = vaddq_s64(sum[2], sum[3]);
    sum[0] = vaddq_s64(sum[0], sum[2]);
    return vaddvq_s64(sum[0]);
  }
  return aom_sum_squares_i16_c(src, n);
}

static inline uint64_t aom_sum_sse_2d_i16_4xh_sve(const int16_t *src,
                                                  int stride, int height,
                                                  int *sum) {
  int64x2_t sse = vdupq_n_s64(0);
  int32x4_t sum_s32 = vdupq_n_s32(0);

  do {
    int16x8_t s = vcombine_s16(vld1_s16(src), vld1_s16(src + stride));

    sse = aom_sdotq_s16(sse, s, s);

    sum_s32 = vpadalq_s16(sum_s32, s);

    src += 2 * stride;
    height -= 2;
  } while (height != 0);

  *sum += vaddvq_s32(sum_s32);
  return vaddvq_s64(sse);
}

static inline uint64_t aom_sum_sse_2d_i16_8xh_sve(const int16_t *src,
                                                  int stride, int height,
                                                  int *sum) {
  int64x2_t sse[2] = { vdupq_n_s64(0), vdupq_n_s64(0) };
  int32x4_t sum_acc[2] = { vdupq_n_s32(0), vdupq_n_s32(0) };

  do {
    int16x8_t s0 = vld1q_s16(src);
    int16x8_t s1 = vld1q_s16(src + stride);

    sse[0] = aom_sdotq_s16(sse[0], s0, s0);
    sse[1] = aom_sdotq_s16(sse[1], s1, s1);

    sum_acc[0] = vpadalq_s16(sum_acc[0], s0);
    sum_acc[1] = vpadalq_s16(sum_acc[1], s1);

    src += 2 * stride;
    height -= 2;
  } while (height != 0);

  *sum += vaddvq_s32(vaddq_s32(sum_acc[0], sum_acc[1]));
  return vaddvq_s64(vaddq_s64(sse[0], sse[1]));
}

static inline uint64_t aom_sum_sse_2d_i16_16xh_sve(const int16_t *src,
                                                   int stride, int width,
                                                   int height, int *sum) {
  int64x2_t sse[2] = { vdupq_n_s64(0), vdupq_n_s64(0) };
  int32x4_t sum_acc[2] = { vdupq_n_s32(0), vdupq_n_s32(0) };

  do {
    int w = 0;
    do {
      int16x8_t s0 = vld1q_s16(src + w);
      int16x8_t s1 = vld1q_s16(src + w + 8);

      sse[0] = aom_sdotq_s16(sse[0], s0, s0);
      sse[1] = aom_sdotq_s16(sse[1], s1, s1);

      sum_acc[0] = vpadalq_s16(sum_acc[0], s0);
      sum_acc[1] = vpadalq_s16(sum_acc[1], s1);

      w += 16;
    } while (w < width);

    src += stride;
  } while (--height != 0);

  *sum += vaddvq_s32(vaddq_s32(sum_acc[0], sum_acc[1]));
  return vaddvq_s64(vaddq_s64(sse[0], sse[1]));
}

uint64_t aom_sum_sse_2d_i16_sve(const int16_t *src, int stride, int width,
                                int height, int *sum) {
  uint64_t sse;

  if (width == 4) {
    sse = aom_sum_sse_2d_i16_4xh_sve(src, stride, height, sum);
  } else if (width == 8) {
    sse = aom_sum_sse_2d_i16_8xh_sve(src, stride, height, sum);
  } else if (width % 16 == 0) {
    sse = aom_sum_sse_2d_i16_16xh_sve(src, stride, width, height, sum);
  } else {
    sse = aom_sum_sse_2d_i16_c(src, stride, width, height, sum);
  }

  return sse;
}

#if CONFIG_AV1_HIGHBITDEPTH
static inline uint64_t aom_var_2d_u16_4xh_sve(uint8_t *src, int src_stride,
                                              int width, int height) {
  uint16_t *src_u16 = CONVERT_TO_SHORTPTR(src);
  uint64_t sum = 0;
  uint64_t sse = 0;
  uint32x4_t sum_u32 = vdupq_n_u32(0);
  uint64x2_t sse_u64 = vdupq_n_u64(0);

  int h = height;
  do {
    uint16x8_t s0 =
        vcombine_u16(vld1_u16(src_u16), vld1_u16(src_u16 + src_stride));

    sum_u32 = vpadalq_u16(sum_u32, s0);

    sse_u64 = aom_udotq_u16(sse_u64, s0, s0);

    src_u16 += 2 * src_stride;
    h -= 2;
  } while (h != 0);

  sum += vaddlvq_u32(sum_u32);
  sse += vaddvq_u64(sse_u64);

  return sse - sum * sum / (width * height);
}

static inline uint64_t aom_var_2d_u16_8xh_sve(uint8_t *src, int src_stride,
                                              int width, int height) {
  uint16_t *src_u16 = CONVERT_TO_SHORTPTR(src);
  uint64_t sum = 0;
  uint64_t sse = 0;
  uint32x4_t sum_u32 = vdupq_n_u32(0);
  uint64x2_t sse_u64 = vdupq_n_u64(0);

  int h = height;
  do {
    int w = width;
    uint16_t *src_ptr = src_u16;
    do {
      uint16x8_t s0 = vld1q_u16(src_ptr);

      sum_u32 = vpadalq_u16(sum_u32, s0);

      sse_u64 = aom_udotq_u16(sse_u64, s0, s0);

      src_ptr += 8;
      w -= 8;
    } while (w != 0);

    src_u16 += src_stride;
  } while (--h != 0);

  sum += vaddlvq_u32(sum_u32);
  sse += vaddvq_u64(sse_u64);

  return sse - sum * sum / (width * height);
}

static inline uint64_t aom_var_2d_u16_16xh_sve(uint8_t *src, int src_stride,
                                               int width, int height) {
  uint16_t *src_u16 = CONVERT_TO_SHORTPTR(src);
  uint64_t sum = 0;
  uint64_t sse = 0;
  uint32x4_t sum_u32[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };
  uint64x2_t sse_u64[2] = { vdupq_n_u64(0), vdupq_n_u64(0) };

  int h = height;
  do {
    int w = width;
    uint16_t *src_ptr = src_u16;
    do {
      uint16x8_t s0 = vld1q_u16(src_ptr);
      uint16x8_t s1 = vld1q_u16(src_ptr + 8);

      sum_u32[0] = vpadalq_u16(sum_u32[0], s0);
      sum_u32[1] = vpadalq_u16(sum_u32[1], s1);

      sse_u64[0] = aom_udotq_u16(sse_u64[0], s0, s0);
      sse_u64[1] = aom_udotq_u16(sse_u64[1], s1, s1);

      src_ptr += 16;
      w -= 16;
    } while (w != 0);

    src_u16 += src_stride;
  } while (--h != 0);

  sum_u32[0] = vaddq_u32(sum_u32[0], sum_u32[1]);
  sse_u64[0] = vaddq_u64(sse_u64[0], sse_u64[1]);

  sum += vaddlvq_u32(sum_u32[0]);
  sse += vaddvq_u64(sse_u64[0]);

  return sse - sum * sum / (width * height);
}

static inline uint64_t aom_var_2d_u16_large_sve(uint8_t *src, int src_stride,
                                                int width, int height) {
  uint16_t *src_u16 = CONVERT_TO_SHORTPTR(src);
  uint64_t sum = 0;
  uint64_t sse = 0;
  uint32x4_t sum_u32[4] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0),
                            vdupq_n_u32(0) };
  uint64x2_t sse_u64[4] = { vdupq_n_u64(0), vdupq_n_u64(0), vdupq_n_u64(0),
                            vdupq_n_u64(0) };

  int h = height;
  do {
    int w = width;
    uint16_t *src_ptr = src_u16;
    do {
      uint16x8_t s0 = vld1q_u16(src_ptr);
      uint16x8_t s1 = vld1q_u16(src_ptr + 8);
      uint16x8_t s2 = vld1q_u16(src_ptr + 16);
      uint16x8_t s3 = vld1q_u16(src_ptr + 24);

      sum_u32[0] = vpadalq_u16(sum_u32[0], s0);
      sum_u32[1] = vpadalq_u16(sum_u32[1], s1);
      sum_u32[2] = vpadalq_u16(sum_u32[2], s2);
      sum_u32[3] = vpadalq_u16(sum_u32[3], s3);

      sse_u64[0] = aom_udotq_u16(sse_u64[0], s0, s0);
      sse_u64[1] = aom_udotq_u16(sse_u64[1], s1, s1);
      sse_u64[2] = aom_udotq_u16(sse_u64[2], s2, s2);
      sse_u64[3] = aom_udotq_u16(sse_u64[3], s3, s3);

      src_ptr += 32;
      w -= 32;
    } while (w != 0);

    src_u16 += src_stride;
  } while (--h != 0);

  sum_u32[0] = vaddq_u32(sum_u32[0], sum_u32[1]);
  sum_u32[2] = vaddq_u32(sum_u32[2], sum_u32[3]);
  sum_u32[0] = vaddq_u32(sum_u32[0], sum_u32[2]);
  sse_u64[0] = vaddq_u64(sse_u64[0], sse_u64[1]);
  sse_u64[2] = vaddq_u64(sse_u64[2], sse_u64[3]);
  sse_u64[0] = vaddq_u64(sse_u64[0], sse_u64[2]);

  sum += vaddlvq_u32(sum_u32[0]);
  sse += vaddvq_u64(sse_u64[0]);

  return sse - sum * sum / (width * height);
}

uint64_t aom_var_2d_u16_sve(uint8_t *src, int src_stride, int width,
                            int height) {
  if (width == 4) {
    return aom_var_2d_u16_4xh_sve(src, src_stride, width, height);
  }
  if (width == 8) {
    return aom_var_2d_u16_8xh_sve(src, src_stride, width, height);
  }
  if (width == 16) {
    return aom_var_2d_u16_16xh_sve(src, src_stride, width, height);
  }
  if (width % 32 == 0) {
    return aom_var_2d_u16_large_sve(src, src_stride, width, height);
  }
  return aom_var_2d_u16_neon(src, src_stride, width, height);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH
