/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>
#include <assert.h>

#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

static inline uint64_t aom_sum_squares_2d_i16_4x4_neon(const int16_t *src,
                                                       int stride) {
  int16x4_t s0 = vld1_s16(src + 0 * stride);
  int16x4_t s1 = vld1_s16(src + 1 * stride);
  int16x4_t s2 = vld1_s16(src + 2 * stride);
  int16x4_t s3 = vld1_s16(src + 3 * stride);

  int32x4_t sum_squares = vmull_s16(s0, s0);
  sum_squares = vmlal_s16(sum_squares, s1, s1);
  sum_squares = vmlal_s16(sum_squares, s2, s2);
  sum_squares = vmlal_s16(sum_squares, s3, s3);

  return horizontal_long_add_u32x4(vreinterpretq_u32_s32(sum_squares));
}

static inline uint64_t aom_sum_squares_2d_i16_4xn_neon(const int16_t *src,
                                                       int stride, int height) {
  int32x4_t sum_squares[2] = { vdupq_n_s32(0), vdupq_n_s32(0) };

  int h = height;
  do {
    int16x4_t s0 = vld1_s16(src + 0 * stride);
    int16x4_t s1 = vld1_s16(src + 1 * stride);
    int16x4_t s2 = vld1_s16(src + 2 * stride);
    int16x4_t s3 = vld1_s16(src + 3 * stride);

    sum_squares[0] = vmlal_s16(sum_squares[0], s0, s0);
    sum_squares[0] = vmlal_s16(sum_squares[0], s1, s1);
    sum_squares[1] = vmlal_s16(sum_squares[1], s2, s2);
    sum_squares[1] = vmlal_s16(sum_squares[1], s3, s3);

    src += 4 * stride;
    h -= 4;
  } while (h != 0);

  return horizontal_long_add_u32x4(
      vreinterpretq_u32_s32(vaddq_s32(sum_squares[0], sum_squares[1])));
}

static inline uint64_t aom_sum_squares_2d_i16_nxn_neon(const int16_t *src,
                                                       int stride, int width,
                                                       int height) {
  uint64x2_t sum_squares = vdupq_n_u64(0);

  int h = height;
  do {
    int32x4_t ss_row[2] = { vdupq_n_s32(0), vdupq_n_s32(0) };
    int w = 0;
    do {
      const int16_t *s = src + w;
      int16x8_t s0 = vld1q_s16(s + 0 * stride);
      int16x8_t s1 = vld1q_s16(s + 1 * stride);
      int16x8_t s2 = vld1q_s16(s + 2 * stride);
      int16x8_t s3 = vld1q_s16(s + 3 * stride);

      ss_row[0] = vmlal_s16(ss_row[0], vget_low_s16(s0), vget_low_s16(s0));
      ss_row[0] = vmlal_s16(ss_row[0], vget_low_s16(s1), vget_low_s16(s1));
      ss_row[0] = vmlal_s16(ss_row[0], vget_low_s16(s2), vget_low_s16(s2));
      ss_row[0] = vmlal_s16(ss_row[0], vget_low_s16(s3), vget_low_s16(s3));
      ss_row[1] = vmlal_s16(ss_row[1], vget_high_s16(s0), vget_high_s16(s0));
      ss_row[1] = vmlal_s16(ss_row[1], vget_high_s16(s1), vget_high_s16(s1));
      ss_row[1] = vmlal_s16(ss_row[1], vget_high_s16(s2), vget_high_s16(s2));
      ss_row[1] = vmlal_s16(ss_row[1], vget_high_s16(s3), vget_high_s16(s3));
      w += 8;
    } while (w < width);

    sum_squares = vpadalq_u32(
        sum_squares, vreinterpretq_u32_s32(vaddq_s32(ss_row[0], ss_row[1])));

    src += 4 * stride;
    h -= 4;
  } while (h != 0);

  return horizontal_add_u64x2(sum_squares);
}

uint64_t aom_sum_squares_2d_i16_neon(const int16_t *src, int stride, int width,
                                     int height) {
  // 4 elements per row only requires half an SIMD register, so this
  // must be a special case, but also note that over 75% of all calls
  // are with size == 4, so it is also the common case.
  if (LIKELY(width == 4 && height == 4)) {
    return aom_sum_squares_2d_i16_4x4_neon(src, stride);
  } else if (LIKELY(width == 4 && (height & 3) == 0)) {
    return aom_sum_squares_2d_i16_4xn_neon(src, stride, height);
  } else if (LIKELY((width & 7) == 0 && (height & 3) == 0)) {
    // Generic case
    return aom_sum_squares_2d_i16_nxn_neon(src, stride, width, height);
  } else {
    return aom_sum_squares_2d_i16_c(src, stride, width, height);
  }
}

static inline uint64_t aom_sum_sse_2d_i16_4x4_neon(const int16_t *src,
                                                   int stride, int *sum) {
  int16x4_t s0 = vld1_s16(src + 0 * stride);
  int16x4_t s1 = vld1_s16(src + 1 * stride);
  int16x4_t s2 = vld1_s16(src + 2 * stride);
  int16x4_t s3 = vld1_s16(src + 3 * stride);

  int32x4_t sse = vmull_s16(s0, s0);
  sse = vmlal_s16(sse, s1, s1);
  sse = vmlal_s16(sse, s2, s2);
  sse = vmlal_s16(sse, s3, s3);

  int32x4_t sum_01 = vaddl_s16(s0, s1);
  int32x4_t sum_23 = vaddl_s16(s2, s3);
  *sum += horizontal_add_s32x4(vaddq_s32(sum_01, sum_23));

  return horizontal_long_add_u32x4(vreinterpretq_u32_s32(sse));
}

static inline uint64_t aom_sum_sse_2d_i16_4xn_neon(const int16_t *src,
                                                   int stride, int height,
                                                   int *sum) {
  int32x4_t sse[2] = { vdupq_n_s32(0), vdupq_n_s32(0) };
  int32x2_t sum_acc[2] = { vdup_n_s32(0), vdup_n_s32(0) };

  int h = height;
  do {
    int16x4_t s0 = vld1_s16(src + 0 * stride);
    int16x4_t s1 = vld1_s16(src + 1 * stride);
    int16x4_t s2 = vld1_s16(src + 2 * stride);
    int16x4_t s3 = vld1_s16(src + 3 * stride);

    sse[0] = vmlal_s16(sse[0], s0, s0);
    sse[0] = vmlal_s16(sse[0], s1, s1);
    sse[1] = vmlal_s16(sse[1], s2, s2);
    sse[1] = vmlal_s16(sse[1], s3, s3);

    sum_acc[0] = vpadal_s16(sum_acc[0], s0);
    sum_acc[0] = vpadal_s16(sum_acc[0], s1);
    sum_acc[1] = vpadal_s16(sum_acc[1], s2);
    sum_acc[1] = vpadal_s16(sum_acc[1], s3);

    src += 4 * stride;
    h -= 4;
  } while (h != 0);

  *sum += horizontal_add_s32x4(vcombine_s32(sum_acc[0], sum_acc[1]));
  return horizontal_long_add_u32x4(
      vreinterpretq_u32_s32(vaddq_s32(sse[0], sse[1])));
}

static inline uint64_t aom_sum_sse_2d_i16_nxn_neon(const int16_t *src,
                                                   int stride, int width,
                                                   int height, int *sum) {
  uint64x2_t sse = vdupq_n_u64(0);
  int32x4_t sum_acc = vdupq_n_s32(0);

  int h = height;
  do {
    int32x4_t sse_row[2] = { vdupq_n_s32(0), vdupq_n_s32(0) };
    int w = 0;
    do {
      const int16_t *s = src + w;
      int16x8_t s0 = vld1q_s16(s + 0 * stride);
      int16x8_t s1 = vld1q_s16(s + 1 * stride);
      int16x8_t s2 = vld1q_s16(s + 2 * stride);
      int16x8_t s3 = vld1q_s16(s + 3 * stride);

      sse_row[0] = vmlal_s16(sse_row[0], vget_low_s16(s0), vget_low_s16(s0));
      sse_row[0] = vmlal_s16(sse_row[0], vget_low_s16(s1), vget_low_s16(s1));
      sse_row[0] = vmlal_s16(sse_row[0], vget_low_s16(s2), vget_low_s16(s2));
      sse_row[0] = vmlal_s16(sse_row[0], vget_low_s16(s3), vget_low_s16(s3));
      sse_row[1] = vmlal_s16(sse_row[1], vget_high_s16(s0), vget_high_s16(s0));
      sse_row[1] = vmlal_s16(sse_row[1], vget_high_s16(s1), vget_high_s16(s1));
      sse_row[1] = vmlal_s16(sse_row[1], vget_high_s16(s2), vget_high_s16(s2));
      sse_row[1] = vmlal_s16(sse_row[1], vget_high_s16(s3), vget_high_s16(s3));

      sum_acc = vpadalq_s16(sum_acc, s0);
      sum_acc = vpadalq_s16(sum_acc, s1);
      sum_acc = vpadalq_s16(sum_acc, s2);
      sum_acc = vpadalq_s16(sum_acc, s3);

      w += 8;
    } while (w < width);

    sse = vpadalq_u32(sse,
                      vreinterpretq_u32_s32(vaddq_s32(sse_row[0], sse_row[1])));

    src += 4 * stride;
    h -= 4;
  } while (h != 0);

  *sum += horizontal_add_s32x4(sum_acc);
  return horizontal_add_u64x2(sse);
}

uint64_t aom_sum_sse_2d_i16_neon(const int16_t *src, int stride, int width,
                                 int height, int *sum) {
  uint64_t sse;

  if (LIKELY(width == 4 && height == 4)) {
    sse = aom_sum_sse_2d_i16_4x4_neon(src, stride, sum);
  } else if (LIKELY(width == 4 && (height & 3) == 0)) {
    // width = 4, height is a multiple of 4.
    sse = aom_sum_sse_2d_i16_4xn_neon(src, stride, height, sum);
  } else if (LIKELY((width & 7) == 0 && (height & 3) == 0)) {
    // Generic case - width is multiple of 8, height is multiple of 4.
    sse = aom_sum_sse_2d_i16_nxn_neon(src, stride, width, height, sum);
  } else {
    sse = aom_sum_sse_2d_i16_c(src, stride, width, height, sum);
  }

  return sse;
}

static inline uint64_t aom_sum_squares_i16_4xn_neon(const int16_t *src,
                                                    uint32_t n) {
  uint64x2_t sum_u64 = vdupq_n_u64(0);

  int i = n;
  do {
    uint32x4_t sum;
    int16x4_t s0 = vld1_s16(src);

    sum = vreinterpretq_u32_s32(vmull_s16(s0, s0));

    sum_u64 = vpadalq_u32(sum_u64, sum);

    src += 4;
    i -= 4;
  } while (i >= 4);

  if (i > 0) {
    return horizontal_add_u64x2(sum_u64) + aom_sum_squares_i16_c(src, i);
  }
  return horizontal_add_u64x2(sum_u64);
}

static inline uint64_t aom_sum_squares_i16_8xn_neon(const int16_t *src,
                                                    uint32_t n) {
  uint64x2_t sum_u64[2] = { vdupq_n_u64(0), vdupq_n_u64(0) };

  int i = n;
  do {
    uint32x4_t sum[2];
    int16x8_t s0 = vld1q_s16(src);

    sum[0] =
        vreinterpretq_u32_s32(vmull_s16(vget_low_s16(s0), vget_low_s16(s0)));
    sum[1] =
        vreinterpretq_u32_s32(vmull_s16(vget_high_s16(s0), vget_high_s16(s0)));

    sum_u64[0] = vpadalq_u32(sum_u64[0], sum[0]);
    sum_u64[1] = vpadalq_u32(sum_u64[1], sum[1]);

    src += 8;
    i -= 8;
  } while (i >= 8);

  if (i > 0) {
    return horizontal_add_u64x2(vaddq_u64(sum_u64[0], sum_u64[1])) +
           aom_sum_squares_i16_c(src, i);
  }
  return horizontal_add_u64x2(vaddq_u64(sum_u64[0], sum_u64[1]));
}

uint64_t aom_sum_squares_i16_neon(const int16_t *src, uint32_t n) {
  // This function seems to be called only for values of N >= 64. See
  // av1/encoder/compound_type.c.
  if (LIKELY(n >= 8)) {
    return aom_sum_squares_i16_8xn_neon(src, n);
  }
  if (n >= 4) {
    return aom_sum_squares_i16_4xn_neon(src, n);
  }
  return aom_sum_squares_i16_c(src, n);
}

static inline uint64_t aom_var_2d_u8_4xh_neon(uint8_t *src, int src_stride,
                                              int width, int height) {
  uint64_t sum = 0;
  uint64_t sse = 0;
  uint32x2_t sum_u32 = vdup_n_u32(0);
  uint32x4_t sse_u32 = vdupq_n_u32(0);

  // 255*256 = 65280, so we can accumulate up to 256 8-bit elements in a 16-bit
  // element before we need to accumulate to 32-bit elements. Since we're
  // accumulating in uint16x4_t vectors, this means we can accumulate up to 4
  // rows of 256 elements. Therefore the limit can be computed as: h_limit = (4
  // * 256) / width.
  int h_limit = (4 * 256) / width;
  int h_tmp = height > h_limit ? h_limit : height;

  int h = 0;
  do {
    uint16x4_t sum_u16 = vdup_n_u16(0);
    do {
      uint8_t *src_ptr = src;
      int w = width;
      do {
        uint8x8_t s0 = load_unaligned_u8(src_ptr, src_stride);

        sum_u16 = vpadal_u8(sum_u16, s0);

        uint16x8_t sse_u16 = vmull_u8(s0, s0);

        sse_u32 = vpadalq_u16(sse_u32, sse_u16);

        src_ptr += 8;
        w -= 8;
      } while (w >= 8);

      // Process remaining columns in the row using C.
      while (w > 0) {
        int idx = width - w;
        const uint8_t v = src[idx];
        sum += v;
        sse += v * v;
        w--;
      }

      src += 2 * src_stride;
      h += 2;
    } while (h < h_tmp && h < height);

    sum_u32 = vpadal_u16(sum_u32, sum_u16);
    h_tmp += h_limit;
  } while (h < height);

  sum += horizontal_long_add_u32x2(sum_u32);
  sse += horizontal_long_add_u32x4(sse_u32);

  return sse - sum * sum / (width * height);
}

static inline uint64_t aom_var_2d_u8_8xh_neon(uint8_t *src, int src_stride,
                                              int width, int height) {
  uint64_t sum = 0;
  uint64_t sse = 0;
  uint32x2_t sum_u32 = vdup_n_u32(0);
  uint32x4_t sse_u32 = vdupq_n_u32(0);

  // 255*256 = 65280, so we can accumulate up to 256 8-bit elements in a 16-bit
  // element before we need to accumulate to 32-bit elements. Since we're
  // accumulating in uint16x4_t vectors, this means we can accumulate up to 4
  // rows of 256 elements. Therefore the limit can be computed as: h_limit = (4
  // * 256) / width.
  int h_limit = (4 * 256) / width;
  int h_tmp = height > h_limit ? h_limit : height;

  int h = 0;
  do {
    uint16x4_t sum_u16 = vdup_n_u16(0);
    do {
      uint8_t *src_ptr = src;
      int w = width;
      do {
        uint8x8_t s0 = vld1_u8(src_ptr);

        sum_u16 = vpadal_u8(sum_u16, s0);

        uint16x8_t sse_u16 = vmull_u8(s0, s0);

        sse_u32 = vpadalq_u16(sse_u32, sse_u16);

        src_ptr += 8;
        w -= 8;
      } while (w >= 8);

      // Process remaining columns in the row using C.
      while (w > 0) {
        int idx = width - w;
        const uint8_t v = src[idx];
        sum += v;
        sse += v * v;
        w--;
      }

      src += src_stride;
      ++h;
    } while (h < h_tmp && h < height);

    sum_u32 = vpadal_u16(sum_u32, sum_u16);
    h_tmp += h_limit;
  } while (h < height);

  sum += horizontal_long_add_u32x2(sum_u32);
  sse += horizontal_long_add_u32x4(sse_u32);

  return sse - sum * sum / (width * height);
}

static inline uint64_t aom_var_2d_u8_16xh_neon(uint8_t *src, int src_stride,
                                               int width, int height) {
  uint64_t sum = 0;
  uint64_t sse = 0;
  uint32x4_t sum_u32 = vdupq_n_u32(0);
  uint32x4_t sse_u32[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };

  // 255*256 = 65280, so we can accumulate up to 256 8-bit elements in a 16-bit
  // element before we need to accumulate to 32-bit elements. Since we're
  // accumulating in uint16x8_t vectors, this means we can accumulate up to 8
  // rows of 256 elements. Therefore the limit can be computed as: h_limit = (8
  // * 256) / width.
  int h_limit = (8 * 256) / width;
  int h_tmp = height > h_limit ? h_limit : height;

  int h = 0;
  do {
    uint16x8_t sum_u16 = vdupq_n_u16(0);
    do {
      int w = width;
      uint8_t *src_ptr = src;
      do {
        uint8x16_t s0 = vld1q_u8(src_ptr);

        sum_u16 = vpadalq_u8(sum_u16, s0);

        uint16x8_t sse_u16_lo = vmull_u8(vget_low_u8(s0), vget_low_u8(s0));
        uint16x8_t sse_u16_hi = vmull_u8(vget_high_u8(s0), vget_high_u8(s0));

        sse_u32[0] = vpadalq_u16(sse_u32[0], sse_u16_lo);
        sse_u32[1] = vpadalq_u16(sse_u32[1], sse_u16_hi);

        src_ptr += 16;
        w -= 16;
      } while (w >= 16);

      // Process remaining columns in the row using C.
      while (w > 0) {
        int idx = width - w;
        const uint8_t v = src[idx];
        sum += v;
        sse += v * v;
        w--;
      }

      src += src_stride;
      ++h;
    } while (h < h_tmp && h < height);

    sum_u32 = vpadalq_u16(sum_u32, sum_u16);
    h_tmp += h_limit;
  } while (h < height);

  sum += horizontal_long_add_u32x4(sum_u32);
  sse += horizontal_long_add_u32x4(vaddq_u32(sse_u32[0], sse_u32[1]));

  return sse - sum * sum / (width * height);
}

uint64_t aom_var_2d_u8_neon(uint8_t *src, int src_stride, int width,
                            int height) {
  if (width >= 16) {
    return aom_var_2d_u8_16xh_neon(src, src_stride, width, height);
  }
  if (width >= 8) {
    return aom_var_2d_u8_8xh_neon(src, src_stride, width, height);
  }
  if (width >= 4 && height % 2 == 0) {
    return aom_var_2d_u8_4xh_neon(src, src_stride, width, height);
  }
  return aom_var_2d_u8_c(src, src_stride, width, height);
}

#if CONFIG_AV1_HIGHBITDEPTH
static inline uint64_t aom_var_2d_u16_4xh_neon(uint8_t *src, int src_stride,
                                               int width, int height) {
  uint16_t *src_u16 = CONVERT_TO_SHORTPTR(src);
  uint64_t sum = 0;
  uint64_t sse = 0;
  uint32x2_t sum_u32 = vdup_n_u32(0);
  uint64x2_t sse_u64 = vdupq_n_u64(0);

  int h = height;
  do {
    int w = width;
    uint16_t *src_ptr = src_u16;
    do {
      uint16x4_t s0 = vld1_u16(src_ptr);

      sum_u32 = vpadal_u16(sum_u32, s0);

      uint32x4_t sse_u32 = vmull_u16(s0, s0);

      sse_u64 = vpadalq_u32(sse_u64, sse_u32);

      src_ptr += 4;
      w -= 4;
    } while (w >= 4);

    // Process remaining columns in the row using C.
    while (w > 0) {
      int idx = width - w;
      const uint16_t v = src_u16[idx];
      sum += v;
      sse += v * v;
      w--;
    }

    src_u16 += src_stride;
  } while (--h != 0);

  sum += horizontal_long_add_u32x2(sum_u32);
  sse += horizontal_add_u64x2(sse_u64);

  return sse - sum * sum / (width * height);
}

static inline uint64_t aom_var_2d_u16_8xh_neon(uint8_t *src, int src_stride,
                                               int width, int height) {
  uint16_t *src_u16 = CONVERT_TO_SHORTPTR(src);
  uint64_t sum = 0;
  uint64_t sse = 0;
  uint32x4_t sum_u32 = vdupq_n_u32(0);
  uint64x2_t sse_u64[2] = { vdupq_n_u64(0), vdupq_n_u64(0) };

  int h = height;
  do {
    int w = width;
    uint16_t *src_ptr = src_u16;
    do {
      uint16x8_t s0 = vld1q_u16(src_ptr);

      sum_u32 = vpadalq_u16(sum_u32, s0);

      uint32x4_t sse_u32_lo = vmull_u16(vget_low_u16(s0), vget_low_u16(s0));
      uint32x4_t sse_u32_hi = vmull_u16(vget_high_u16(s0), vget_high_u16(s0));

      sse_u64[0] = vpadalq_u32(sse_u64[0], sse_u32_lo);
      sse_u64[1] = vpadalq_u32(sse_u64[1], sse_u32_hi);

      src_ptr += 8;
      w -= 8;
    } while (w >= 8);

    // Process remaining columns in the row using C.
    while (w > 0) {
      int idx = width - w;
      const uint16_t v = src_u16[idx];
      sum += v;
      sse += v * v;
      w--;
    }

    src_u16 += src_stride;
  } while (--h != 0);

  sum += horizontal_long_add_u32x4(sum_u32);
  sse += horizontal_add_u64x2(vaddq_u64(sse_u64[0], sse_u64[1]));

  return sse - sum * sum / (width * height);
}

uint64_t aom_var_2d_u16_neon(uint8_t *src, int src_stride, int width,
                             int height) {
  if (width >= 8) {
    return aom_var_2d_u16_8xh_neon(src, src_stride, width, height);
  }
  if (width >= 4) {
    return aom_var_2d_u16_4xh_neon(src, src_stride, width, height);
  }
  return aom_var_2d_u16_c(src, src_stride, width, height);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH
