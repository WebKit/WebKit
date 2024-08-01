/*
 * Copyright (c) 2023 The WebM project authors. All rights reserved.
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

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/aom_filter.h"
#include "aom_dsp/arm/dist_wtd_avg_neon.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_dsp/variance.h"

// The bilinear filters look like this:
//
// {{ 128,  0 }, { 112, 16 }, { 96, 32 }, { 80,  48 },
//  {  64, 64 }, {  48, 80 }, { 32, 96 }, { 16, 112 }}
//
// We can factor out the highest common multiple, such that the sum of both
// weights will be 8 instead of 128. The benefits of this are two-fold:
//
// 1) We can infer the filter values from the filter_offset parameter in the
// bilinear filter functions below - we don't have to actually load the values
// from memory:
// f0 = 8 - filter_offset
// f1 = filter_offset
//
// 2) Scaling the pixel values by 8, instead of 128 enables us to operate on
// 16-bit data types at all times, rather than widening out to 32-bit and
// requiring double the number of data processing instructions. (12-bit * 8 =
// 15-bit.)

// Process a block exactly 4 wide and any height.
static void highbd_var_filter_block2d_bil_w4(const uint16_t *src_ptr,
                                             uint16_t *dst_ptr, int src_stride,
                                             int pixel_step, int dst_height,
                                             int filter_offset) {
  const uint16x4_t f0 = vdup_n_u16(8 - filter_offset);
  const uint16x4_t f1 = vdup_n_u16(filter_offset);

  int i = dst_height;
  do {
    uint16x4_t s0 = load_unaligned_u16_4x1(src_ptr);
    uint16x4_t s1 = load_unaligned_u16_4x1(src_ptr + pixel_step);

    uint16x4_t blend = vmul_u16(s0, f0);
    blend = vmla_u16(blend, s1, f1);
    blend = vrshr_n_u16(blend, 3);

    vst1_u16(dst_ptr, blend);

    src_ptr += src_stride;
    dst_ptr += 4;
  } while (--i != 0);
}

// Process a block which is a multiple of 8 and any height.
static void highbd_var_filter_block2d_bil_large(const uint16_t *src_ptr,
                                                uint16_t *dst_ptr,
                                                int src_stride, int pixel_step,
                                                int dst_width, int dst_height,
                                                int filter_offset) {
  const uint16x8_t f0 = vdupq_n_u16(8 - filter_offset);
  const uint16x8_t f1 = vdupq_n_u16(filter_offset);

  int i = dst_height;
  do {
    int j = 0;
    do {
      uint16x8_t s0 = vld1q_u16(src_ptr + j);
      uint16x8_t s1 = vld1q_u16(src_ptr + j + pixel_step);

      uint16x8_t blend = vmulq_u16(s0, f0);
      blend = vmlaq_u16(blend, s1, f1);
      blend = vrshrq_n_u16(blend, 3);

      vst1q_u16(dst_ptr + j, blend);

      j += 8;
    } while (j < dst_width);

    src_ptr += src_stride;
    dst_ptr += dst_width;
  } while (--i != 0);
}

static void highbd_var_filter_block2d_bil_w8(const uint16_t *src_ptr,
                                             uint16_t *dst_ptr, int src_stride,
                                             int pixel_step, int dst_height,
                                             int filter_offset) {
  highbd_var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride, pixel_step,
                                      8, dst_height, filter_offset);
}

static void highbd_var_filter_block2d_bil_w16(const uint16_t *src_ptr,
                                              uint16_t *dst_ptr, int src_stride,
                                              int pixel_step, int dst_height,
                                              int filter_offset) {
  highbd_var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride, pixel_step,
                                      16, dst_height, filter_offset);
}

static void highbd_var_filter_block2d_bil_w32(const uint16_t *src_ptr,
                                              uint16_t *dst_ptr, int src_stride,
                                              int pixel_step, int dst_height,
                                              int filter_offset) {
  highbd_var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride, pixel_step,
                                      32, dst_height, filter_offset);
}

static void highbd_var_filter_block2d_bil_w64(const uint16_t *src_ptr,
                                              uint16_t *dst_ptr, int src_stride,
                                              int pixel_step, int dst_height,
                                              int filter_offset) {
  highbd_var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride, pixel_step,
                                      64, dst_height, filter_offset);
}

static void highbd_var_filter_block2d_bil_w128(const uint16_t *src_ptr,
                                               uint16_t *dst_ptr,
                                               int src_stride, int pixel_step,
                                               int dst_height,
                                               int filter_offset) {
  highbd_var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride, pixel_step,
                                      128, dst_height, filter_offset);
}

static void highbd_var_filter_block2d_avg(const uint16_t *src_ptr,
                                          uint16_t *dst_ptr, int src_stride,
                                          int pixel_step, int dst_width,
                                          int dst_height) {
  int i = dst_height;

  // We only specialize on the filter values for large block sizes (>= 16x16.)
  assert(dst_width >= 16 && dst_width % 16 == 0);

  do {
    int j = 0;
    do {
      uint16x8_t s0 = vld1q_u16(src_ptr + j);
      uint16x8_t s1 = vld1q_u16(src_ptr + j + pixel_step);
      uint16x8_t avg = vrhaddq_u16(s0, s1);
      vst1q_u16(dst_ptr + j, avg);

      j += 8;
    } while (j < dst_width);

    src_ptr += src_stride;
    dst_ptr += dst_width;
  } while (--i != 0);
}

#define HBD_SUBPEL_VARIANCE_WXH_NEON(bitdepth, w, h)                           \
  unsigned int aom_highbd_##bitdepth##_sub_pixel_variance##w##x##h##_neon(     \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,            \
      const uint8_t *ref, int ref_stride, uint32_t *sse) {                     \
    uint16_t tmp0[w * (h + 1)];                                                \
    uint16_t tmp1[w * h];                                                      \
    uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src);                              \
                                                                               \
    highbd_var_filter_block2d_bil_w##w(src_ptr, tmp0, src_stride, 1, (h + 1),  \
                                       xoffset);                               \
    highbd_var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);          \
                                                                               \
    return aom_highbd_##bitdepth##_variance##w##x##h(CONVERT_TO_BYTEPTR(tmp1), \
                                                     w, ref, ref_stride, sse); \
  }

#define HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(bitdepth, w, h)               \
  unsigned int aom_highbd_##bitdepth##_sub_pixel_variance##w##x##h##_neon(     \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,            \
      const uint8_t *ref, int ref_stride, unsigned int *sse) {                 \
    uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src);                              \
                                                                               \
    if (xoffset == 0) {                                                        \
      if (yoffset == 0) {                                                      \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(src_ptr), src_stride, ref, ref_stride, sse);    \
      } else if (yoffset == 4) {                                               \
        uint16_t tmp[w * h];                                                   \
        highbd_var_filter_block2d_avg(src_ptr, tmp, src_stride, src_stride, w, \
                                      h);                                      \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp), w, ref, ref_stride, sse);                 \
      } else {                                                                 \
        uint16_t tmp[w * h];                                                   \
        highbd_var_filter_block2d_bil_w##w(src_ptr, tmp, src_stride,           \
                                           src_stride, h, yoffset);            \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp), w, ref, ref_stride, sse);                 \
      }                                                                        \
    } else if (xoffset == 4) {                                                 \
      uint16_t tmp0[w * (h + 1)];                                              \
      if (yoffset == 0) {                                                      \
        highbd_var_filter_block2d_avg(src_ptr, tmp0, src_stride, 1, w, h);     \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp0), w, ref, ref_stride, sse);                \
      } else if (yoffset == 4) {                                               \
        uint16_t tmp1[w * (h + 1)];                                            \
        highbd_var_filter_block2d_avg(src_ptr, tmp0, src_stride, 1, w,         \
                                      (h + 1));                                \
        highbd_var_filter_block2d_avg(tmp0, tmp1, w, w, w, h);                 \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp1), w, ref, ref_stride, sse);                \
      } else {                                                                 \
        uint16_t tmp1[w * (h + 1)];                                            \
        highbd_var_filter_block2d_avg(src_ptr, tmp0, src_stride, 1, w,         \
                                      (h + 1));                                \
        highbd_var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);      \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp1), w, ref, ref_stride, sse);                \
      }                                                                        \
    } else {                                                                   \
      uint16_t tmp0[w * (h + 1)];                                              \
      if (yoffset == 0) {                                                      \
        highbd_var_filter_block2d_bil_w##w(src_ptr, tmp0, src_stride, 1, h,    \
                                           xoffset);                           \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp0), w, ref, ref_stride, sse);                \
      } else if (yoffset == 4) {                                               \
        uint16_t tmp1[w * h];                                                  \
        highbd_var_filter_block2d_bil_w##w(src_ptr, tmp0, src_stride, 1,       \
                                           (h + 1), xoffset);                  \
        highbd_var_filter_block2d_avg(tmp0, tmp1, w, w, w, h);                 \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp1), w, ref, ref_stride, sse);                \
      } else {                                                                 \
        uint16_t tmp1[w * h];                                                  \
        highbd_var_filter_block2d_bil_w##w(src_ptr, tmp0, src_stride, 1,       \
                                           (h + 1), xoffset);                  \
        highbd_var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);      \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp1), w, ref, ref_stride, sse);                \
      }                                                                        \
    }                                                                          \
  }

// 8-bit
HBD_SUBPEL_VARIANCE_WXH_NEON(8, 4, 4)
HBD_SUBPEL_VARIANCE_WXH_NEON(8, 4, 8)

HBD_SUBPEL_VARIANCE_WXH_NEON(8, 8, 4)
HBD_SUBPEL_VARIANCE_WXH_NEON(8, 8, 8)
HBD_SUBPEL_VARIANCE_WXH_NEON(8, 8, 16)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(8, 16, 8)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(8, 16, 16)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(8, 16, 32)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(8, 32, 16)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(8, 32, 32)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(8, 32, 64)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(8, 64, 32)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(8, 64, 64)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(8, 64, 128)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(8, 128, 64)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(8, 128, 128)

#if !CONFIG_REALTIME_ONLY
HBD_SUBPEL_VARIANCE_WXH_NEON(8, 4, 16)

HBD_SUBPEL_VARIANCE_WXH_NEON(8, 8, 32)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(8, 16, 4)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(8, 16, 64)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(8, 32, 8)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(8, 64, 16)
#endif  // !CONFIG_REALTIME_ONLY

// 10-bit
HBD_SUBPEL_VARIANCE_WXH_NEON(10, 4, 4)
HBD_SUBPEL_VARIANCE_WXH_NEON(10, 4, 8)

HBD_SUBPEL_VARIANCE_WXH_NEON(10, 8, 4)
HBD_SUBPEL_VARIANCE_WXH_NEON(10, 8, 8)
HBD_SUBPEL_VARIANCE_WXH_NEON(10, 8, 16)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(10, 16, 8)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(10, 16, 16)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(10, 16, 32)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(10, 32, 16)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(10, 32, 32)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(10, 32, 64)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(10, 64, 32)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(10, 64, 64)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(10, 64, 128)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(10, 128, 64)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(10, 128, 128)

#if !CONFIG_REALTIME_ONLY
HBD_SUBPEL_VARIANCE_WXH_NEON(10, 4, 16)

HBD_SUBPEL_VARIANCE_WXH_NEON(10, 8, 32)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(10, 16, 4)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(10, 16, 64)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(10, 32, 8)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(10, 64, 16)
#endif  // !CONFIG_REALTIME_ONLY

// 12-bit
HBD_SUBPEL_VARIANCE_WXH_NEON(12, 4, 4)
HBD_SUBPEL_VARIANCE_WXH_NEON(12, 4, 8)

HBD_SUBPEL_VARIANCE_WXH_NEON(12, 8, 4)
HBD_SUBPEL_VARIANCE_WXH_NEON(12, 8, 8)
HBD_SUBPEL_VARIANCE_WXH_NEON(12, 8, 16)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(12, 16, 8)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(12, 16, 16)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(12, 16, 32)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(12, 32, 16)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(12, 32, 32)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(12, 32, 64)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(12, 64, 32)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(12, 64, 64)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(12, 64, 128)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(12, 128, 64)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(12, 128, 128)

#if !CONFIG_REALTIME_ONLY
HBD_SUBPEL_VARIANCE_WXH_NEON(12, 4, 16)

HBD_SUBPEL_VARIANCE_WXH_NEON(12, 8, 32)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(12, 16, 4)
HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(12, 16, 64)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(12, 32, 8)

HBD_SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(12, 64, 16)
#endif  // !CONFIG_REALTIME_ONLY

// Combine bilinear filter with aom_highbd_comp_avg_pred for blocks having
// width 4.
static void highbd_avg_pred_var_filter_block2d_bil_w4(
    const uint16_t *src_ptr, uint16_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint16_t *second_pred) {
  const uint16x4_t f0 = vdup_n_u16(8 - filter_offset);
  const uint16x4_t f1 = vdup_n_u16(filter_offset);

  int i = dst_height;
  do {
    uint16x4_t s0 = load_unaligned_u16_4x1(src_ptr);
    uint16x4_t s1 = load_unaligned_u16_4x1(src_ptr + pixel_step);
    uint16x4_t p = vld1_u16(second_pred);

    uint16x4_t blend = vmul_u16(s0, f0);
    blend = vmla_u16(blend, s1, f1);
    blend = vrshr_n_u16(blend, 3);

    vst1_u16(dst_ptr, vrhadd_u16(blend, p));

    src_ptr += src_stride;
    dst_ptr += 4;
    second_pred += 4;
  } while (--i != 0);
}

// Combine bilinear filter with aom_highbd_comp_avg_pred for large blocks.
static void highbd_avg_pred_var_filter_block2d_bil_large(
    const uint16_t *src_ptr, uint16_t *dst_ptr, int src_stride, int pixel_step,
    int dst_width, int dst_height, int filter_offset,
    const uint16_t *second_pred) {
  const uint16x8_t f0 = vdupq_n_u16(8 - filter_offset);
  const uint16x8_t f1 = vdupq_n_u16(filter_offset);

  int i = dst_height;
  do {
    int j = 0;
    do {
      uint16x8_t s0 = vld1q_u16(src_ptr + j);
      uint16x8_t s1 = vld1q_u16(src_ptr + j + pixel_step);
      uint16x8_t p = vld1q_u16(second_pred);

      uint16x8_t blend = vmulq_u16(s0, f0);
      blend = vmlaq_u16(blend, s1, f1);
      blend = vrshrq_n_u16(blend, 3);

      vst1q_u16(dst_ptr + j, vrhaddq_u16(blend, p));

      j += 8;
      second_pred += 8;
    } while (j < dst_width);

    src_ptr += src_stride;
    dst_ptr += dst_width;
  } while (--i != 0);
}

static void highbd_avg_pred_var_filter_block2d_bil_w8(
    const uint16_t *src_ptr, uint16_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint16_t *second_pred) {
  highbd_avg_pred_var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride,
                                               pixel_step, 8, dst_height,
                                               filter_offset, second_pred);
}

static void highbd_avg_pred_var_filter_block2d_bil_w16(
    const uint16_t *src_ptr, uint16_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint16_t *second_pred) {
  highbd_avg_pred_var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride,
                                               pixel_step, 16, dst_height,
                                               filter_offset, second_pred);
}

static void highbd_avg_pred_var_filter_block2d_bil_w32(
    const uint16_t *src_ptr, uint16_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint16_t *second_pred) {
  highbd_avg_pred_var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride,
                                               pixel_step, 32, dst_height,
                                               filter_offset, second_pred);
}

static void highbd_avg_pred_var_filter_block2d_bil_w64(
    const uint16_t *src_ptr, uint16_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint16_t *second_pred) {
  highbd_avg_pred_var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride,
                                               pixel_step, 64, dst_height,
                                               filter_offset, second_pred);
}

static void highbd_avg_pred_var_filter_block2d_bil_w128(
    const uint16_t *src_ptr, uint16_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint16_t *second_pred) {
  highbd_avg_pred_var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride,
                                               pixel_step, 128, dst_height,
                                               filter_offset, second_pred);
}

// Combine averaging subpel filter with aom_highbd_comp_avg_pred.
static void highbd_avg_pred_var_filter_block2d_avg(
    const uint16_t *src_ptr, uint16_t *dst_ptr, int src_stride, int pixel_step,
    int dst_width, int dst_height, const uint16_t *second_pred) {
  int i = dst_height;

  // We only specialize on the filter values for large block sizes (>= 16x16.)
  assert(dst_width >= 16 && dst_width % 16 == 0);

  do {
    int j = 0;
    do {
      uint16x8_t s0 = vld1q_u16(src_ptr + j);
      uint16x8_t s1 = vld1q_u16(src_ptr + j + pixel_step);
      uint16x8_t avg = vrhaddq_u16(s0, s1);

      uint16x8_t p = vld1q_u16(second_pred);
      avg = vrhaddq_u16(avg, p);

      vst1q_u16(dst_ptr + j, avg);

      j += 8;
      second_pred += 8;
    } while (j < dst_width);

    src_ptr += src_stride;
    dst_ptr += dst_width;
  } while (--i != 0);
}

// Implementation of aom_highbd_comp_avg_pred for blocks having width >= 16.
static void highbd_avg_pred(const uint16_t *src_ptr, uint16_t *dst_ptr,
                            int src_stride, int dst_width, int dst_height,
                            const uint16_t *second_pred) {
  int i = dst_height;

  // We only specialize on the filter values for large block sizes (>= 16x16.)
  assert(dst_width >= 16 && dst_width % 16 == 0);

  do {
    int j = 0;
    do {
      uint16x8_t s = vld1q_u16(src_ptr + j);
      uint16x8_t p = vld1q_u16(second_pred);

      uint16x8_t avg = vrhaddq_u16(s, p);

      vst1q_u16(dst_ptr + j, avg);

      j += 8;
      second_pred += 8;
    } while (j < dst_width);

    src_ptr += src_stride;
    dst_ptr += dst_width;
  } while (--i != 0);
}

#define HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(bitdepth, w, h)                       \
  uint32_t aom_highbd_##bitdepth##_sub_pixel_avg_variance##w##x##h##_neon(     \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,            \
      const uint8_t *ref, int ref_stride, uint32_t *sse,                       \
      const uint8_t *second_pred) {                                            \
    uint16_t tmp0[w * (h + 1)];                                                \
    uint16_t tmp1[w * h];                                                      \
    uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src);                              \
                                                                               \
    highbd_var_filter_block2d_bil_w##w(src_ptr, tmp0, src_stride, 1, (h + 1),  \
                                       xoffset);                               \
    highbd_avg_pred_var_filter_block2d_bil_w##w(                               \
        tmp0, tmp1, w, w, h, yoffset, CONVERT_TO_SHORTPTR(second_pred));       \
                                                                               \
    return aom_highbd_##bitdepth##_variance##w##x##h(CONVERT_TO_BYTEPTR(tmp1), \
                                                     w, ref, ref_stride, sse); \
  }

#define HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(bitdepth, w, h)           \
  unsigned int aom_highbd_##bitdepth##_sub_pixel_avg_variance##w##x##h##_neon( \
      const uint8_t *src, int source_stride, int xoffset, int yoffset,         \
      const uint8_t *ref, int ref_stride, uint32_t *sse,                       \
      const uint8_t *second_pred) {                                            \
    uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src);                              \
                                                                               \
    if (xoffset == 0) {                                                        \
      uint16_t tmp[w * h];                                                     \
      if (yoffset == 0) {                                                      \
        highbd_avg_pred(src_ptr, tmp, source_stride, w, h,                     \
                        CONVERT_TO_SHORTPTR(second_pred));                     \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp), w, ref, ref_stride, sse);                 \
      } else if (yoffset == 4) {                                               \
        highbd_avg_pred_var_filter_block2d_avg(                                \
            src_ptr, tmp, source_stride, source_stride, w, h,                  \
            CONVERT_TO_SHORTPTR(second_pred));                                 \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp), w, ref, ref_stride, sse);                 \
      } else {                                                                 \
        highbd_avg_pred_var_filter_block2d_bil_w##w(                           \
            src_ptr, tmp, source_stride, source_stride, h, yoffset,            \
            CONVERT_TO_SHORTPTR(second_pred));                                 \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp), w, ref, ref_stride, sse);                 \
      }                                                                        \
    } else if (xoffset == 4) {                                                 \
      uint16_t tmp0[w * (h + 1)];                                              \
      if (yoffset == 0) {                                                      \
        highbd_avg_pred_var_filter_block2d_avg(                                \
            src_ptr, tmp0, source_stride, 1, w, h,                             \
            CONVERT_TO_SHORTPTR(second_pred));                                 \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp0), w, ref, ref_stride, sse);                \
      } else if (yoffset == 4) {                                               \
        uint16_t tmp1[w * (h + 1)];                                            \
        highbd_var_filter_block2d_avg(src_ptr, tmp0, source_stride, 1, w,      \
                                      (h + 1));                                \
        highbd_avg_pred_var_filter_block2d_avg(                                \
            tmp0, tmp1, w, w, w, h, CONVERT_TO_SHORTPTR(second_pred));         \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp1), w, ref, ref_stride, sse);                \
      } else {                                                                 \
        uint16_t tmp1[w * (h + 1)];                                            \
        highbd_var_filter_block2d_avg(src_ptr, tmp0, source_stride, 1, w,      \
                                      (h + 1));                                \
        highbd_avg_pred_var_filter_block2d_bil_w##w(                           \
            tmp0, tmp1, w, w, h, yoffset, CONVERT_TO_SHORTPTR(second_pred));   \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp1), w, ref, ref_stride, sse);                \
      }                                                                        \
    } else {                                                                   \
      uint16_t tmp0[w * (h + 1)];                                              \
      if (yoffset == 0) {                                                      \
        highbd_avg_pred_var_filter_block2d_bil_w##w(                           \
            src_ptr, tmp0, source_stride, 1, h, xoffset,                       \
            CONVERT_TO_SHORTPTR(second_pred));                                 \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp0), w, ref, ref_stride, sse);                \
      } else if (yoffset == 4) {                                               \
        uint16_t tmp1[w * h];                                                  \
        highbd_var_filter_block2d_bil_w##w(src_ptr, tmp0, source_stride, 1,    \
                                           (h + 1), xoffset);                  \
        highbd_avg_pred_var_filter_block2d_avg(                                \
            tmp0, tmp1, w, w, w, h, CONVERT_TO_SHORTPTR(second_pred));         \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp1), w, ref, ref_stride, sse);                \
      } else {                                                                 \
        uint16_t tmp1[w * h];                                                  \
        highbd_var_filter_block2d_bil_w##w(src_ptr, tmp0, source_stride, 1,    \
                                           (h + 1), xoffset);                  \
        highbd_avg_pred_var_filter_block2d_bil_w##w(                           \
            tmp0, tmp1, w, w, h, yoffset, CONVERT_TO_SHORTPTR(second_pred));   \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp1), w, ref, ref_stride, sse);                \
      }                                                                        \
    }                                                                          \
  }

// 8-bit
HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 4, 4)
HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 4, 8)

HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 8, 4)
HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 8, 8)
HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 8, 16)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 16, 8)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 16, 16)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 16, 32)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 32, 16)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 32, 32)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 32, 64)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 64, 32)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 64, 64)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 64, 128)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 128, 64)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 128, 128)

#if !CONFIG_REALTIME_ONLY
HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 4, 16)

HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 8, 32)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 16, 4)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 16, 64)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 32, 8)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 64, 16)
#endif  // !CONFIG_REALTIME_ONLY

// 10-bit
HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 4, 4)
HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 4, 8)

HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 8, 4)
HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 8, 8)
HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 8, 16)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 16, 8)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 16, 16)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 16, 32)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 32, 16)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 32, 32)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 32, 64)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 64, 32)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 64, 64)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 64, 128)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 128, 64)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 128, 128)

#if !CONFIG_REALTIME_ONLY
HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 4, 16)

HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 8, 32)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 16, 4)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 16, 64)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 32, 8)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 64, 16)
#endif  // !CONFIG_REALTIME_ONLY

// 12-bit
HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 4, 4)
HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 4, 8)

HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 8, 4)
HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 8, 8)
HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 8, 16)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 16, 8)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 16, 16)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 16, 32)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 32, 16)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 32, 32)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 32, 64)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 64, 32)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 64, 64)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 64, 128)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 128, 64)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 128, 128)

#if !CONFIG_REALTIME_ONLY
HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 4, 16)

HBD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 8, 32)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 16, 4)
HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 16, 64)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 32, 8)

HBD_SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 64, 16)
#endif  // !CONFIG_REALTIME_ONLY

#define HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(bitdepth, w, h)                    \
  unsigned int                                                                 \
      aom_highbd_##bitdepth##_masked_sub_pixel_variance##w##x##h##_neon(       \
          const uint8_t *src, int src_stride, int xoffset, int yoffset,        \
          const uint8_t *ref, int ref_stride, const uint8_t *second_pred,      \
          const uint8_t *msk, int msk_stride, int invert_mask,                 \
          unsigned int *sse) {                                                 \
    uint16_t tmp0[w * (h + 1)];                                                \
    uint16_t tmp1[w * (h + 1)];                                                \
    uint16_t tmp2[w * h];                                                      \
    uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src);                              \
    highbd_var_filter_block2d_bil_w##w(src_ptr, tmp0, src_stride, 1, (h + 1),  \
                                       xoffset);                               \
    highbd_var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);          \
    aom_highbd_comp_mask_pred_neon(CONVERT_TO_BYTEPTR(tmp2), second_pred, w,   \
                                   h, CONVERT_TO_BYTEPTR(tmp1), w, msk,        \
                                   msk_stride, invert_mask);                   \
    return aom_highbd_##bitdepth##_variance##w##x##h(CONVERT_TO_BYTEPTR(tmp2), \
                                                     w, ref, ref_stride, sse); \
  }

#define HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(bitdepth, w, h)        \
  unsigned int                                                                 \
      aom_highbd_##bitdepth##_masked_sub_pixel_variance##w##x##h##_neon(       \
          const uint8_t *src, int src_stride, int xoffset, int yoffset,        \
          const uint8_t *ref, int ref_stride, const uint8_t *second_pred,      \
          const uint8_t *msk, int msk_stride, int invert_mask,                 \
          unsigned int *sse) {                                                 \
    uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src);                              \
    if (xoffset == 0) {                                                        \
      uint16_t tmp0[w * h];                                                    \
      if (yoffset == 0) {                                                      \
        aom_highbd_comp_mask_pred_neon(CONVERT_TO_BYTEPTR(tmp0), second_pred,  \
                                       w, h, src, src_stride, msk, msk_stride, \
                                       invert_mask);                           \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp0), w, ref, ref_stride, sse);                \
      } else if (yoffset == 4) {                                               \
        uint16_t tmp1[w * h];                                                  \
        highbd_var_filter_block2d_avg(src_ptr, tmp0, src_stride, src_stride,   \
                                      w, h);                                   \
        aom_highbd_comp_mask_pred_neon(CONVERT_TO_BYTEPTR(tmp1), second_pred,  \
                                       w, h, CONVERT_TO_BYTEPTR(tmp0), w, msk, \
                                       msk_stride, invert_mask);               \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp1), w, ref, ref_stride, sse);                \
      } else {                                                                 \
        uint16_t tmp1[w * h];                                                  \
        highbd_var_filter_block2d_bil_w##w(src_ptr, tmp0, src_stride,          \
                                           src_stride, h, yoffset);            \
        aom_highbd_comp_mask_pred_neon(CONVERT_TO_BYTEPTR(tmp1), second_pred,  \
                                       w, h, CONVERT_TO_BYTEPTR(tmp0), w, msk, \
                                       msk_stride, invert_mask);               \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp1), w, ref, ref_stride, sse);                \
      }                                                                        \
    } else if (xoffset == 4) {                                                 \
      uint16_t tmp0[w * (h + 1)];                                              \
      if (yoffset == 0) {                                                      \
        uint16_t tmp1[w * h];                                                  \
        highbd_var_filter_block2d_avg(src_ptr, tmp0, src_stride, 1, w, h);     \
        aom_highbd_comp_mask_pred_neon(CONVERT_TO_BYTEPTR(tmp1), second_pred,  \
                                       w, h, CONVERT_TO_BYTEPTR(tmp0), w, msk, \
                                       msk_stride, invert_mask);               \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp1), w, ref, ref_stride, sse);                \
      } else if (yoffset == 4) {                                               \
        uint16_t tmp1[w * h];                                                  \
        uint16_t tmp2[w * h];                                                  \
        highbd_var_filter_block2d_avg(src_ptr, tmp0, src_stride, 1, w,         \
                                      (h + 1));                                \
        highbd_var_filter_block2d_avg(tmp0, tmp1, w, w, w, h);                 \
        aom_highbd_comp_mask_pred_neon(CONVERT_TO_BYTEPTR(tmp2), second_pred,  \
                                       w, h, CONVERT_TO_BYTEPTR(tmp1), w, msk, \
                                       msk_stride, invert_mask);               \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp2), w, ref, ref_stride, sse);                \
      } else {                                                                 \
        uint16_t tmp1[w * h];                                                  \
        uint16_t tmp2[w * h];                                                  \
        highbd_var_filter_block2d_avg(src_ptr, tmp0, src_stride, 1, w,         \
                                      (h + 1));                                \
        highbd_var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);      \
        aom_highbd_comp_mask_pred_neon(CONVERT_TO_BYTEPTR(tmp2), second_pred,  \
                                       w, h, CONVERT_TO_BYTEPTR(tmp1), w, msk, \
                                       msk_stride, invert_mask);               \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp2), w, ref, ref_stride, sse);                \
      }                                                                        \
    } else {                                                                   \
      if (yoffset == 0) {                                                      \
        uint16_t tmp0[w * h];                                                  \
        uint16_t tmp1[w * h];                                                  \
        highbd_var_filter_block2d_bil_w##w(src_ptr, tmp0, src_stride, 1, h,    \
                                           xoffset);                           \
        aom_highbd_comp_mask_pred_neon(CONVERT_TO_BYTEPTR(tmp1), second_pred,  \
                                       w, h, CONVERT_TO_BYTEPTR(tmp0), w, msk, \
                                       msk_stride, invert_mask);               \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp1), w, ref, ref_stride, sse);                \
      } else if (yoffset == 4) {                                               \
        uint16_t tmp0[w * (h + 1)];                                            \
        uint16_t tmp1[w * h];                                                  \
        uint16_t tmp2[w * h];                                                  \
        highbd_var_filter_block2d_bil_w##w(src_ptr, tmp0, src_stride, 1,       \
                                           (h + 1), xoffset);                  \
        highbd_var_filter_block2d_avg(tmp0, tmp1, w, w, w, h);                 \
        aom_highbd_comp_mask_pred_neon(CONVERT_TO_BYTEPTR(tmp2), second_pred,  \
                                       w, h, CONVERT_TO_BYTEPTR(tmp1), w, msk, \
                                       msk_stride, invert_mask);               \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp2), w, ref, ref_stride, sse);                \
      } else {                                                                 \
        uint16_t tmp0[w * (h + 1)];                                            \
        uint16_t tmp1[w * (h + 1)];                                            \
        uint16_t tmp2[w * h];                                                  \
        highbd_var_filter_block2d_bil_w##w(src_ptr, tmp0, src_stride, 1,       \
                                           (h + 1), xoffset);                  \
        highbd_var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);      \
        aom_highbd_comp_mask_pred_neon(CONVERT_TO_BYTEPTR(tmp2), second_pred,  \
                                       w, h, CONVERT_TO_BYTEPTR(tmp1), w, msk, \
                                       msk_stride, invert_mask);               \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp2), w, ref, ref_stride, sse);                \
      }                                                                        \
    }                                                                          \
  }

// 8-bit
HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 4, 4)
HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 4, 8)

HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 8, 4)
HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 8, 8)
HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 8, 16)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 16, 8)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 16, 16)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 16, 32)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 32, 16)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 32, 32)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 32, 64)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 64, 32)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 64, 64)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 64, 128)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 128, 64)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 128, 128)

#if !CONFIG_REALTIME_ONLY
HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 4, 16)

HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 8, 32)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 16, 4)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 16, 64)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 32, 8)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 64, 16)
#endif  // !CONFIG_REALTIME_ONLY

// 10-bit
HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 4, 4)
HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 4, 8)

HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 8, 4)
HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 8, 8)
HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 8, 16)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 16, 8)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 16, 16)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 16, 32)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 32, 16)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 32, 32)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 32, 64)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 64, 32)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 64, 64)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 64, 128)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 128, 64)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 128, 128)

#if !CONFIG_REALTIME_ONLY
HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 4, 16)

HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 8, 32)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 16, 4)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 16, 64)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 32, 8)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(10, 64, 16)
#endif  // !CONFIG_REALTIME_ONLY

// 12-bit
HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 4, 4)
HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 4, 8)

HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 8, 4)
HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 8, 8)
HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 8, 16)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 16, 8)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 16, 16)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 16, 32)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 32, 16)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 32, 32)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 32, 64)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 64, 32)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 64, 64)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 64, 128)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 128, 64)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 128, 128)

#if !CONFIG_REALTIME_ONLY
HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 4, 16)

HBD_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 8, 32)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 16, 4)
HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 16, 64)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 32, 8)

HBD_SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(12, 64, 16)
#endif  // !CONFIG_REALTIME_ONLY

#if !CONFIG_REALTIME_ONLY
#define HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(bitdepth, w, h)                \
  unsigned int                                                              \
      aom_highbd_##bitdepth##_obmc_sub_pixel_variance##w##x##h##_neon(      \
          const uint8_t *pre, int pre_stride, int xoffset, int yoffset,     \
          const int32_t *wsrc, const int32_t *mask, unsigned int *sse) {    \
    uint16_t *pre_ptr = CONVERT_TO_SHORTPTR(pre);                           \
    uint16_t tmp0[w * (h + 1)];                                             \
    uint16_t tmp1[w * h];                                                   \
    highbd_var_filter_block2d_bil_w##w(pre_ptr, tmp0, pre_stride, 1, h + 1, \
                                       xoffset);                            \
    highbd_var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);       \
    return aom_highbd_##bitdepth##_obmc_variance##w##x##h##_neon(           \
        CONVERT_TO_BYTEPTR(tmp1), w, wsrc, mask, sse);                      \
  }

#define SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(bitdepth, w, h)       \
  unsigned int                                                                 \
      aom_highbd_##bitdepth##_obmc_sub_pixel_variance##w##x##h##_neon(         \
          const uint8_t *pre, int pre_stride, int xoffset, int yoffset,        \
          const int32_t *wsrc, const int32_t *mask, unsigned int *sse) {       \
    uint16_t *pre_ptr = CONVERT_TO_SHORTPTR(pre);                              \
    if (xoffset == 0) {                                                        \
      if (yoffset == 0) {                                                      \
        return aom_highbd_##bitdepth##_obmc_variance##w##x##h##_neon(          \
            pre, pre_stride, wsrc, mask, sse);                                 \
      } else if (yoffset == 4) {                                               \
        uint16_t tmp[w * h];                                                   \
        highbd_var_filter_block2d_avg(pre_ptr, tmp, pre_stride, pre_stride, w, \
                                      h);                                      \
        return aom_highbd_##bitdepth##_obmc_variance##w##x##h##_neon(          \
            CONVERT_TO_BYTEPTR(tmp), w, wsrc, mask, sse);                      \
      } else {                                                                 \
        uint16_t tmp[w * h];                                                   \
        highbd_var_filter_block2d_bil_w##w(pre_ptr, tmp, pre_stride,           \
                                           pre_stride, h, yoffset);            \
        return aom_highbd_##bitdepth##_obmc_variance##w##x##h##_neon(          \
            CONVERT_TO_BYTEPTR(tmp), w, wsrc, mask, sse);                      \
      }                                                                        \
    } else if (xoffset == 4) {                                                 \
      uint16_t tmp0[w * (h + 1)];                                              \
      if (yoffset == 0) {                                                      \
        highbd_var_filter_block2d_avg(pre_ptr, tmp0, pre_stride, 1, w, h);     \
        return aom_highbd_##bitdepth##_obmc_variance##w##x##h##_neon(          \
            CONVERT_TO_BYTEPTR(tmp0), w, wsrc, mask, sse);                     \
      } else if (yoffset == 4) {                                               \
        uint16_t tmp1[w * (h + 1)];                                            \
        highbd_var_filter_block2d_avg(pre_ptr, tmp0, pre_stride, 1, w, h + 1); \
        highbd_var_filter_block2d_avg(tmp0, tmp1, w, w, w, h);                 \
        return aom_highbd_##bitdepth##_obmc_variance##w##x##h##_neon(          \
            CONVERT_TO_BYTEPTR(tmp1), w, wsrc, mask, sse);                     \
      } else {                                                                 \
        uint16_t tmp1[w * (h + 1)];                                            \
        highbd_var_filter_block2d_avg(pre_ptr, tmp0, pre_stride, 1, w, h + 1); \
        highbd_var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);      \
        return aom_highbd_##bitdepth##_obmc_variance##w##x##h##_neon(          \
            CONVERT_TO_BYTEPTR(tmp1), w, wsrc, mask, sse);                     \
      }                                                                        \
    } else {                                                                   \
      uint16_t tmp0[w * (h + 1)];                                              \
      if (yoffset == 0) {                                                      \
        highbd_var_filter_block2d_bil_w##w(pre_ptr, tmp0, pre_stride, 1, h,    \
                                           xoffset);                           \
        return aom_highbd_##bitdepth##_obmc_variance##w##x##h##_neon(          \
            CONVERT_TO_BYTEPTR(tmp0), w, wsrc, mask, sse);                     \
      } else if (yoffset == 4) {                                               \
        uint16_t tmp1[w * h];                                                  \
        highbd_var_filter_block2d_bil_w##w(pre_ptr, tmp0, pre_stride, 1,       \
                                           h + 1, xoffset);                    \
        highbd_var_filter_block2d_avg(tmp0, tmp1, w, w, w, h);                 \
        return aom_highbd_##bitdepth##_obmc_variance##w##x##h##_neon(          \
            CONVERT_TO_BYTEPTR(tmp1), w, wsrc, mask, sse);                     \
      } else {                                                                 \
        uint16_t tmp1[w * h];                                                  \
        highbd_var_filter_block2d_bil_w##w(pre_ptr, tmp0, pre_stride, 1,       \
                                           h + 1, xoffset);                    \
        highbd_var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);      \
        return aom_highbd_##bitdepth##_obmc_variance##w##x##h##_neon(          \
            CONVERT_TO_BYTEPTR(tmp1), w, wsrc, mask, sse);                     \
      }                                                                        \
    }                                                                          \
  }

// 8-bit
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 4, 4)
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 4, 8)
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 4, 16)

HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 8, 4)
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 8, 8)
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 8, 16)
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 8, 32)

HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 16, 4)
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 16, 8)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 16, 16)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 16, 32)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 16, 64)

SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 32, 8)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 32, 16)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 32, 32)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 32, 64)

SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 64, 16)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 64, 32)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 64, 64)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 64, 128)

SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 128, 64)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 128, 128)

// 10-bit
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 4, 4)
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 4, 8)
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 4, 16)

HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 8, 4)
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 8, 8)
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 8, 16)
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 8, 32)

HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 16, 4)
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 16, 8)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 16, 16)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 16, 32)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 16, 64)

SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 32, 8)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 32, 16)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 32, 32)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 32, 64)

SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 64, 16)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 64, 32)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 64, 64)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 64, 128)

SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 128, 64)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(10, 128, 128)

// 12-bit
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 4, 4)
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 4, 8)
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 4, 16)

HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 8, 4)
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 8, 8)
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 8, 16)
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 8, 32)

HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 16, 4)
HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 16, 8)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 16, 16)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 16, 32)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 16, 64)

SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 32, 8)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 32, 16)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 32, 32)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 32, 64)

SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 64, 16)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 64, 32)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 64, 64)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 64, 128)

SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 128, 64)
SPECIALIZED_HIGHBD_OBMC_SUBPEL_VARIANCE_WXH_NEON(12, 128, 128)
#endif  // !CONFIG_REALTIME_ONLY

static void highbd_dist_wtd_avg_pred(const uint16_t *src_ptr, uint16_t *dst_ptr,
                                     int src_stride, int dst_width,
                                     int dst_height,
                                     const uint16_t *second_pred,
                                     const DIST_WTD_COMP_PARAMS *jcp_param) {
  // We only specialise on the filter values for large block sizes (>= 16x16.)
  assert(dst_width >= 16 && dst_width % 16 == 0);
  const uint16x8_t fwd_offset = vdupq_n_u16(jcp_param->fwd_offset);
  const uint16x8_t bck_offset = vdupq_n_u16(jcp_param->bck_offset);

  int i = dst_height;
  do {
    int j = 0;
    do {
      uint16x8_t s = vld1q_u16(src_ptr + j);
      uint16x8_t p = vld1q_u16(second_pred);

      uint16x8_t avg = dist_wtd_avg_u16x8(s, p, fwd_offset, bck_offset);

      vst1q_u16(dst_ptr + j, avg);

      second_pred += 8;
      j += 8;
    } while (j < dst_width);

    src_ptr += src_stride;
    dst_ptr += dst_width;
  } while (--i != 0);
}

static void highbd_dist_wtd_avg_pred_var_filter_block2d_avg(
    const uint16_t *src_ptr, uint16_t *dst_ptr, int src_stride, int pixel_step,
    int dst_width, int dst_height, const uint16_t *second_pred,
    const DIST_WTD_COMP_PARAMS *jcp_param) {
  // We only specialise on the filter values for large block sizes (>= 16x16.)
  assert(dst_width >= 16 && dst_width % 16 == 0);
  const uint16x8_t fwd_offset = vdupq_n_u16(jcp_param->fwd_offset);
  const uint16x8_t bck_offset = vdupq_n_u16(jcp_param->bck_offset);

  int i = dst_height;
  do {
    int j = 0;
    do {
      uint16x8_t s0 = vld1q_u16(src_ptr + j);
      uint16x8_t s1 = vld1q_u16(src_ptr + j + pixel_step);
      uint16x8_t p = vld1q_u16(second_pred);
      uint16x8_t avg = vrhaddq_u16(s0, s1);
      avg = dist_wtd_avg_u16x8(avg, p, fwd_offset, bck_offset);

      vst1q_u16(dst_ptr + j, avg);

      second_pred += 8;
      j += 8;
    } while (j < dst_width);

    src_ptr += src_stride;
    dst_ptr += dst_width;
  } while (--i != 0);
}

static void highbd_dist_wtd_avg_pred_var_filter_block2d_bil_w4(
    const uint16_t *src_ptr, uint16_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint16_t *second_pred,
    const DIST_WTD_COMP_PARAMS *jcp_param) {
  const uint16x4_t fwd_offset = vdup_n_u16(jcp_param->fwd_offset);
  const uint16x4_t bck_offset = vdup_n_u16(jcp_param->bck_offset);
  const uint16x4_t f0 = vdup_n_u16(8 - filter_offset);
  const uint16x4_t f1 = vdup_n_u16(filter_offset);

  int i = dst_height;
  do {
    uint16x4_t s0 = load_unaligned_u16_4x1(src_ptr);
    uint16x4_t s1 = load_unaligned_u16_4x1(src_ptr + pixel_step);
    uint16x4_t p = vld1_u16(second_pred);

    uint16x4_t blend = vmul_u16(s0, f0);
    blend = vmla_u16(blend, s1, f1);
    blend = vrshr_n_u16(blend, 3);

    uint16x4_t avg = dist_wtd_avg_u16x4(blend, p, fwd_offset, bck_offset);

    vst1_u16(dst_ptr, avg);

    src_ptr += src_stride;
    dst_ptr += 4;
    second_pred += 4;
  } while (--i != 0);
}

// Combine bilinear filter with aom_dist_wtd_comp_avg_pred for large blocks.
static void highbd_dist_wtd_avg_pred_var_filter_block2d_bil_large(
    const uint16_t *src_ptr, uint16_t *dst_ptr, int src_stride, int pixel_step,
    int dst_width, int dst_height, int filter_offset,
    const uint16_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param) {
  const uint16x8_t fwd_offset = vdupq_n_u16(jcp_param->fwd_offset);
  const uint16x8_t bck_offset = vdupq_n_u16(jcp_param->bck_offset);
  const uint16x8_t f0 = vdupq_n_u16(8 - filter_offset);
  const uint16x8_t f1 = vdupq_n_u16(filter_offset);

  int i = dst_height;
  do {
    int j = 0;
    do {
      uint16x8_t s0 = vld1q_u16(src_ptr + j);
      uint16x8_t s1 = vld1q_u16(src_ptr + j + pixel_step);
      uint16x8_t p = vld1q_u16(second_pred);

      uint16x8_t blend = vmulq_u16(s0, f0);
      blend = vmlaq_u16(blend, s1, f1);
      blend = vrshrq_n_u16(blend, 3);

      uint16x8_t avg = dist_wtd_avg_u16x8(blend, p, fwd_offset, bck_offset);

      vst1q_u16(dst_ptr + j, avg);

      second_pred += 8;
      j += 8;
    } while (j < dst_width);

    src_ptr += src_stride;
    dst_ptr += dst_width;
  } while (--i != 0);
}

static void highbd_dist_wtd_avg_pred_var_filter_block2d_bil_w8(
    const uint16_t *src_ptr, uint16_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint16_t *second_pred,
    const DIST_WTD_COMP_PARAMS *jcp_param) {
  highbd_dist_wtd_avg_pred_var_filter_block2d_bil_large(
      src_ptr, dst_ptr, src_stride, pixel_step, 8, dst_height, filter_offset,
      second_pred, jcp_param);
}

// Combine bilinear filter with aom_comp_avg_pred for blocks having width 16.
static void highbd_dist_wtd_avg_pred_var_filter_block2d_bil_w16(
    const uint16_t *src_ptr, uint16_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint16_t *second_pred,
    const DIST_WTD_COMP_PARAMS *jcp_param) {
  highbd_dist_wtd_avg_pred_var_filter_block2d_bil_large(
      src_ptr, dst_ptr, src_stride, pixel_step, 16, dst_height, filter_offset,
      second_pred, jcp_param);
}

// Combine bilinear filter with aom_comp_avg_pred for blocks having width 32.
static void highbd_dist_wtd_avg_pred_var_filter_block2d_bil_w32(
    const uint16_t *src_ptr, uint16_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint16_t *second_pred,
    const DIST_WTD_COMP_PARAMS *jcp_param) {
  highbd_dist_wtd_avg_pred_var_filter_block2d_bil_large(
      src_ptr, dst_ptr, src_stride, pixel_step, 32, dst_height, filter_offset,
      second_pred, jcp_param);
}

// Combine bilinear filter with aom_comp_avg_pred for blocks having width 64.
static void highbd_dist_wtd_avg_pred_var_filter_block2d_bil_w64(
    const uint16_t *src_ptr, uint16_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint16_t *second_pred,
    const DIST_WTD_COMP_PARAMS *jcp_param) {
  highbd_dist_wtd_avg_pred_var_filter_block2d_bil_large(
      src_ptr, dst_ptr, src_stride, pixel_step, 64, dst_height, filter_offset,
      second_pred, jcp_param);
}

// Combine bilinear filter with aom_comp_avg_pred for blocks having width 128.
static void highbd_dist_wtd_avg_pred_var_filter_block2d_bil_w128(
    const uint16_t *src_ptr, uint16_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint16_t *second_pred,
    const DIST_WTD_COMP_PARAMS *jcp_param) {
  highbd_dist_wtd_avg_pred_var_filter_block2d_bil_large(
      src_ptr, dst_ptr, src_stride, pixel_step, 128, dst_height, filter_offset,
      second_pred, jcp_param);
}

#define HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(bitdepth, w, h)              \
  unsigned int                                                                 \
      aom_highbd_##bitdepth##_dist_wtd_sub_pixel_avg_variance##w##x##h##_neon( \
          const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, \
          const uint8_t *ref_ptr, int ref_stride, uint32_t *sse,               \
          const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param) { \
    uint16_t *src = CONVERT_TO_SHORTPTR(src_ptr);                              \
    uint16_t *second = CONVERT_TO_SHORTPTR(second_pred);                       \
    uint16_t tmp0[w * (h + 1)];                                                \
    uint16_t tmp1[w * h];                                                      \
    highbd_var_filter_block2d_bil_w##w(src, tmp0, source_stride, 1, h + 1,     \
                                       xoffset);                               \
    highbd_dist_wtd_avg_pred_var_filter_block2d_bil_w##w(                      \
        tmp0, tmp1, w, w, h, yoffset, second, jcp_param);                      \
    return aom_highbd_##bitdepth##_variance##w##x##h(                          \
        CONVERT_TO_BYTEPTR(tmp1), w, ref_ptr, ref_stride, sse);                \
  }

#define SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(bitdepth, w, h)  \
  unsigned int                                                                 \
      aom_highbd_##bitdepth##_dist_wtd_sub_pixel_avg_variance##w##x##h##_neon( \
          const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, \
          const uint8_t *ref_ptr, int ref_stride, uint32_t *sse,               \
          const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param) { \
    uint16_t *src = CONVERT_TO_SHORTPTR(src_ptr);                              \
    uint16_t *second = CONVERT_TO_SHORTPTR(second_pred);                       \
    if (xoffset == 0) {                                                        \
      uint16_t tmp[w * h];                                                     \
      if (yoffset == 0) {                                                      \
        highbd_dist_wtd_avg_pred(src, tmp, source_stride, w, h, second,        \
                                 jcp_param);                                   \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp), w, ref_ptr, ref_stride, sse);             \
      } else if (yoffset == 4) {                                               \
        highbd_dist_wtd_avg_pred_var_filter_block2d_avg(                       \
            src, tmp, source_stride, source_stride, w, h, second, jcp_param);  \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp), w, ref_ptr, ref_stride, sse);             \
      } else {                                                                 \
        highbd_dist_wtd_avg_pred_var_filter_block2d_bil_w##w(                  \
            src, tmp, source_stride, source_stride, h, yoffset, second,        \
            jcp_param);                                                        \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp), w, ref_ptr, ref_stride, sse);             \
      }                                                                        \
    } else if (xoffset == 4) {                                                 \
      uint16_t tmp0[w * (h + 1)];                                              \
      if (yoffset == 0) {                                                      \
        highbd_dist_wtd_avg_pred_var_filter_block2d_avg(                       \
            src, tmp0, source_stride, 1, w, h, second, jcp_param);             \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp0), w, ref_ptr, ref_stride, sse);            \
      } else if (yoffset == 4) {                                               \
        uint16_t tmp1[w * (h + 1)];                                            \
        highbd_var_filter_block2d_avg(src, tmp0, source_stride, 1, w, h + 1);  \
        highbd_dist_wtd_avg_pred_var_filter_block2d_avg(tmp0, tmp1, w, w, w,   \
                                                        h, second, jcp_param); \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp1), w, ref_ptr, ref_stride, sse);            \
      } else {                                                                 \
        uint16_t tmp1[w * (h + 1)];                                            \
        highbd_var_filter_block2d_avg(src, tmp0, source_stride, 1, w, h + 1);  \
        highbd_dist_wtd_avg_pred_var_filter_block2d_bil_w##w(                  \
            tmp0, tmp1, w, w, h, yoffset, second, jcp_param);                  \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp1), w, ref_ptr, ref_stride, sse);            \
      }                                                                        \
    } else {                                                                   \
      uint16_t tmp0[w * (h + 1)];                                              \
      if (yoffset == 0) {                                                      \
        highbd_dist_wtd_avg_pred_var_filter_block2d_bil_w##w(                  \
            src, tmp0, source_stride, 1, h, xoffset, second, jcp_param);       \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp0), w, ref_ptr, ref_stride, sse);            \
      } else if (yoffset == 4) {                                               \
        uint16_t tmp1[w * h];                                                  \
        highbd_var_filter_block2d_bil_w##w(src, tmp0, source_stride, 1, h + 1, \
                                           xoffset);                           \
        highbd_dist_wtd_avg_pred_var_filter_block2d_avg(tmp0, tmp1, w, w, w,   \
                                                        h, second, jcp_param); \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp1), w, ref_ptr, ref_stride, sse);            \
      } else {                                                                 \
        uint16_t tmp1[w * h];                                                  \
        highbd_var_filter_block2d_bil_w##w(src, tmp0, source_stride, 1, h + 1, \
                                           xoffset);                           \
        highbd_dist_wtd_avg_pred_var_filter_block2d_bil_w##w(                  \
            tmp0, tmp1, w, w, h, yoffset, second, jcp_param);                  \
        return aom_highbd_##bitdepth##_variance##w##x##h(                      \
            CONVERT_TO_BYTEPTR(tmp1), w, ref_ptr, ref_stride, sse);            \
      }                                                                        \
    }                                                                          \
  }

// 8-bit
HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 4, 4)
HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 4, 8)

HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 8, 4)
HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 8, 8)
HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 8, 16)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 16, 8)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 16, 16)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 16, 32)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 32, 16)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 32, 32)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 32, 64)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 64, 32)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 64, 64)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 64, 128)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 128, 64)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 128, 128)

#if !CONFIG_REALTIME_ONLY
HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 4, 16)

HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 8, 32)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 16, 4)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 16, 64)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 32, 8)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 64, 16)
#endif  // !CONFIG_REALTIME_ONLY

// 10-bit
HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 4, 4)
HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 4, 8)

HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 8, 4)
HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 8, 8)
HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 8, 16)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 16, 8)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 16, 16)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 16, 32)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 32, 16)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 32, 32)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 32, 64)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 64, 32)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 64, 64)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 64, 128)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 128, 64)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 128, 128)

#if !CONFIG_REALTIME_ONLY
HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 4, 16)

HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 8, 32)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 16, 4)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 16, 64)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 32, 8)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(10, 64, 16)
#endif  // !CONFIG_REALTIME_ONLY

// 12-bit
HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 4, 4)
HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 4, 8)

HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 8, 4)
HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 8, 8)
HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 8, 16)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 16, 8)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 16, 16)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 16, 32)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 32, 16)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 32, 32)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 32, 64)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 64, 32)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 64, 64)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 64, 128)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 128, 64)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 128, 128)

#if !CONFIG_REALTIME_ONLY
HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 4, 16)

HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 8, 32)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 16, 4)
SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 16, 64)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 32, 8)

SPECIALIZED_HBD_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(12, 64, 16)
#endif  // !CONFIG_REALTIME_ONLY
