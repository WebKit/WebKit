/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AOM_DSP_ARM_SUBPEL_VARIANCE_NEON_H_
#define AOM_AOM_DSP_ARM_SUBPEL_VARIANCE_NEON_H_

#include <arm_neon.h>
#include "aom_dsp/arm/mem_neon.h"
#include "config/aom_dsp_rtcd.h"

static inline void var_filter_block2d_bil_w4(const uint8_t *src_ptr,
                                             uint8_t *dst_ptr, int src_stride,
                                             int pixel_step, int dst_height,
                                             int filter_offset) {
  const uint8x8_t f0 = vdup_n_u8(8 - filter_offset);
  const uint8x8_t f1 = vdup_n_u8(filter_offset);

  int i = dst_height;
  do {
    uint8x8_t s0 = load_unaligned_u8(src_ptr, src_stride);
    uint8x8_t s1 = load_unaligned_u8(src_ptr + pixel_step, src_stride);
    uint16x8_t blend = vmull_u8(s0, f0);
    blend = vmlal_u8(blend, s1, f1);
    uint8x8_t blend_u8 = vrshrn_n_u16(blend, 3);
    vst1_u8(dst_ptr, blend_u8);

    src_ptr += 2 * src_stride;
    dst_ptr += 2 * 4;
    i -= 2;
  } while (i != 0);
}

static inline void var_filter_block2d_bil_w8(const uint8_t *src_ptr,
                                             uint8_t *dst_ptr, int src_stride,
                                             int pixel_step, int dst_height,
                                             int filter_offset) {
  const uint8x8_t f0 = vdup_n_u8(8 - filter_offset);
  const uint8x8_t f1 = vdup_n_u8(filter_offset);

  int i = dst_height;
  do {
    uint8x8_t s0 = vld1_u8(src_ptr);
    uint8x8_t s1 = vld1_u8(src_ptr + pixel_step);
    uint16x8_t blend = vmull_u8(s0, f0);
    blend = vmlal_u8(blend, s1, f1);
    uint8x8_t blend_u8 = vrshrn_n_u16(blend, 3);
    vst1_u8(dst_ptr, blend_u8);

    src_ptr += src_stride;
    dst_ptr += 8;
  } while (--i != 0);
}

static inline void var_filter_block2d_bil_large(const uint8_t *src_ptr,
                                                uint8_t *dst_ptr,
                                                int src_stride, int pixel_step,
                                                int dst_width, int dst_height,
                                                int filter_offset) {
  const uint8x8_t f0 = vdup_n_u8(8 - filter_offset);
  const uint8x8_t f1 = vdup_n_u8(filter_offset);

  int i = dst_height;
  do {
    int j = 0;
    do {
      uint8x16_t s0 = vld1q_u8(src_ptr + j);
      uint8x16_t s1 = vld1q_u8(src_ptr + j + pixel_step);
      uint16x8_t blend_l = vmull_u8(vget_low_u8(s0), f0);
      blend_l = vmlal_u8(blend_l, vget_low_u8(s1), f1);
      uint16x8_t blend_h = vmull_u8(vget_high_u8(s0), f0);
      blend_h = vmlal_u8(blend_h, vget_high_u8(s1), f1);
      uint8x16_t blend_u8 =
          vcombine_u8(vrshrn_n_u16(blend_l, 3), vrshrn_n_u16(blend_h, 3));
      vst1q_u8(dst_ptr + j, blend_u8);

      j += 16;
    } while (j < dst_width);

    src_ptr += src_stride;
    dst_ptr += dst_width;
  } while (--i != 0);
}

static inline void var_filter_block2d_bil_w16(const uint8_t *src_ptr,
                                              uint8_t *dst_ptr, int src_stride,
                                              int pixel_step, int dst_height,
                                              int filter_offset) {
  var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride, pixel_step, 16,
                               dst_height, filter_offset);
}

static inline void var_filter_block2d_bil_w32(const uint8_t *src_ptr,
                                              uint8_t *dst_ptr, int src_stride,
                                              int pixel_step, int dst_height,
                                              int filter_offset) {
  var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride, pixel_step, 32,
                               dst_height, filter_offset);
}

static inline void var_filter_block2d_bil_w64(const uint8_t *src_ptr,
                                              uint8_t *dst_ptr, int src_stride,
                                              int pixel_step, int dst_height,
                                              int filter_offset) {
  var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride, pixel_step, 64,
                               dst_height, filter_offset);
}

static inline void var_filter_block2d_bil_w128(const uint8_t *src_ptr,
                                               uint8_t *dst_ptr, int src_stride,
                                               int pixel_step, int dst_height,
                                               int filter_offset) {
  var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride, pixel_step, 128,
                               dst_height, filter_offset);
}

static inline void var_filter_block2d_avg(const uint8_t *src_ptr,
                                          uint8_t *dst_ptr, int src_stride,
                                          int pixel_step, int dst_width,
                                          int dst_height) {
  // We only specialise on the filter values for large block sizes (>= 16x16.)
  assert(dst_width >= 16 && dst_width % 16 == 0);

  int i = dst_height;
  do {
    int j = 0;
    do {
      uint8x16_t s0 = vld1q_u8(src_ptr + j);
      uint8x16_t s1 = vld1q_u8(src_ptr + j + pixel_step);
      uint8x16_t avg = vrhaddq_u8(s0, s1);
      vst1q_u8(dst_ptr + j, avg);

      j += 16;
    } while (j < dst_width);

    src_ptr += src_stride;
    dst_ptr += dst_width;
  } while (--i != 0);
}

#endif  // AOM_AOM_DSP_ARM_SUBPEL_VARIANCE_NEON_H_
