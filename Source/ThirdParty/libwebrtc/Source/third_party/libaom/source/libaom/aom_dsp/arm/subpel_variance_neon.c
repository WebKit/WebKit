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

#include <arm_neon.h>

#include "config/aom_dsp_rtcd.h"
#include "config/aom_config.h"

#include "aom_ports/mem.h"
#include "aom/aom_integer.h"

#include "aom_dsp/aom_filter.h"
#include "aom_dsp/variance.h"

// Load 2 sets of 4 bytes when alignment is not guaranteed.
static INLINE uint8x8_t load_unaligned_u8(const uint8_t *buf, int stride) {
  uint32_t a;
  uint32x2_t a_u32 = vdup_n_u32(0);
  if (stride == 4) return vld1_u8(buf);
  memcpy(&a, buf, 4);
  buf += stride;
  a_u32 = vld1_lane_u32(&a, a_u32, 0);
  memcpy(&a, buf, 4);
  a_u32 = vld1_lane_u32(&a, a_u32, 1);
  return vreinterpret_u8_u32(a_u32);
}

// Process a block exactly 4 wide and a multiple of 2 high.
static void var_filter_block2d_bil_w4(const uint8_t *src_ptr,
                                      uint8_t *output_ptr,
                                      unsigned int src_pixels_per_line,
                                      int pixel_step,
                                      unsigned int output_height,
                                      const uint8_t *filter) {
  const uint8x8_t f0 = vdup_n_u8(filter[0]);
  const uint8x8_t f1 = vdup_n_u8(filter[1]);
  unsigned int i;
  for (i = 0; i < output_height; i += 2) {
    const uint8x8_t src_0 = load_unaligned_u8(src_ptr, src_pixels_per_line);
    const uint8x8_t src_1 =
        load_unaligned_u8(src_ptr + pixel_step, src_pixels_per_line);
    const uint16x8_t a = vmull_u8(src_0, f0);
    const uint16x8_t b = vmlal_u8(a, src_1, f1);
    const uint8x8_t out = vrshrn_n_u16(b, FILTER_BITS);
    vst1_u8(output_ptr, out);
    src_ptr += 2 * src_pixels_per_line;
    output_ptr += 8;
  }
}

static void var_filter_block2d_bil_w8(const uint8_t *src_ptr,
                                      uint8_t *output_ptr,
                                      unsigned int src_pixels_per_line,
                                      int pixel_step,
                                      unsigned int output_height,
                                      unsigned int output_width,
                                      const uint8_t *filter) {
  const uint8x8_t f0 = vdup_n_u8(filter[0]);
  const uint8x8_t f1 = vdup_n_u8(filter[1]);
  unsigned int i;
  for (i = 0; i < output_height; ++i) {
    const uint8x8_t src_0 = vld1_u8(&src_ptr[0]);
    const uint8x8_t src_1 = vld1_u8(&src_ptr[pixel_step]);
    const uint16x8_t a = vmull_u8(src_0, f0);
    const uint16x8_t b = vmlal_u8(a, src_1, f1);
    const uint8x8_t out = vrshrn_n_u16(b, FILTER_BITS);
    vst1_u8(output_ptr, out);
    // Next row...
    src_ptr += src_pixels_per_line;
    output_ptr += output_width;
  }
}

// Process a block which is a mutiple of 16 wide and any height.
static void var_filter_block2d_bil_w16(const uint8_t *src_ptr,
                                       uint8_t *output_ptr,
                                       unsigned int src_pixels_per_line,
                                       int pixel_step,
                                       unsigned int output_height,
                                       unsigned int output_width,
                                       const uint8_t *filter) {
  const uint8x8_t f0 = vdup_n_u8(filter[0]);
  const uint8x8_t f1 = vdup_n_u8(filter[1]);
  unsigned int i, j;
  for (i = 0; i < output_height; ++i) {
    for (j = 0; j < output_width; j += 16) {
      const uint8x16_t src_0 = vld1q_u8(&src_ptr[j]);
      const uint8x16_t src_1 = vld1q_u8(&src_ptr[j + pixel_step]);
      const uint16x8_t a = vmull_u8(vget_low_u8(src_0), f0);
      const uint16x8_t b = vmlal_u8(a, vget_low_u8(src_1), f1);
      const uint8x8_t out_lo = vrshrn_n_u16(b, FILTER_BITS);
      const uint16x8_t c = vmull_u8(vget_high_u8(src_0), f0);
      const uint16x8_t d = vmlal_u8(c, vget_high_u8(src_1), f1);
      const uint8x8_t out_hi = vrshrn_n_u16(d, FILTER_BITS);
      vst1q_u8(output_ptr + j, vcombine_u8(out_lo, out_hi));
    }
    src_ptr += src_pixels_per_line;
    output_ptr += output_width;
  }
}

unsigned int aom_sub_pixel_variance8x8_neon(const uint8_t *src, int src_stride,
                                            int xoffset, int yoffset,
                                            const uint8_t *dst, int dst_stride,
                                            unsigned int *sse) {
  DECLARE_ALIGNED(16, uint8_t, temp2[8 * 8]);
  DECLARE_ALIGNED(16, uint8_t, fdata3[9 * 8]);

  var_filter_block2d_bil_w8(src, fdata3, src_stride, 1, 9, 8,
                            bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w8(fdata3, temp2, 8, 8, 8, 8,
                            bilinear_filters_2t[yoffset]);
  return aom_variance8x8_neon(temp2, 8, dst, dst_stride, sse);
}

unsigned int aom_sub_pixel_variance16x16_neon(const uint8_t *src,
                                              int src_stride, int xoffset,
                                              int yoffset, const uint8_t *dst,
                                              int dst_stride,
                                              unsigned int *sse) {
  DECLARE_ALIGNED(16, uint8_t, temp2[16 * 16]);
  DECLARE_ALIGNED(16, uint8_t, fdata3[17 * 16]);

  var_filter_block2d_bil_w16(src, fdata3, src_stride, 1, 17, 16,
                             bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w16(fdata3, temp2, 16, 16, 16, 16,
                             bilinear_filters_2t[yoffset]);
  return aom_variance16x16_neon(temp2, 16, dst, dst_stride, sse);
}

unsigned int aom_sub_pixel_variance32x32_neon(const uint8_t *src,
                                              int src_stride, int xoffset,
                                              int yoffset, const uint8_t *dst,
                                              int dst_stride,
                                              unsigned int *sse) {
  DECLARE_ALIGNED(16, uint8_t, temp2[32 * 32]);
  DECLARE_ALIGNED(16, uint8_t, fdata3[33 * 32]);

  var_filter_block2d_bil_w16(src, fdata3, src_stride, 1, 33, 32,
                             bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w16(fdata3, temp2, 32, 32, 32, 32,
                             bilinear_filters_2t[yoffset]);
  return aom_variance32x32_neon(temp2, 32, dst, dst_stride, sse);
}

unsigned int aom_sub_pixel_variance64x64_neon(const uint8_t *src,
                                              int src_stride, int xoffset,
                                              int yoffset, const uint8_t *dst,
                                              int dst_stride,
                                              unsigned int *sse) {
  DECLARE_ALIGNED(16, uint8_t, temp2[64 * 64]);
  DECLARE_ALIGNED(16, uint8_t, fdata3[65 * 64]);

  var_filter_block2d_bil_w16(src, fdata3, src_stride, 1, 65, 64,
                             bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w16(fdata3, temp2, 64, 64, 64, 64,
                             bilinear_filters_2t[yoffset]);
  return aom_variance64x64_neon(temp2, 64, dst, dst_stride, sse);
}

unsigned int aom_sub_pixel_variance4x4_neon(const uint8_t *a, int a_stride,
                                            int xoffset, int yoffset,
                                            const uint8_t *b, int b_stride,
                                            uint32_t *sse) {
  uint8_t temp0[4 * (4 + 2)];
  uint8_t temp1[4 * 4];

  var_filter_block2d_bil_w4(a, temp0, a_stride, 1, (4 + 2),
                            bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w4(temp0, temp1, 4, 4, 4,
                            bilinear_filters_2t[yoffset]);

  return aom_variance4x4(temp1, 4, b, b_stride, sse);
}

unsigned int aom_sub_pixel_variance4x8_neon(const uint8_t *a, int a_stride,
                                            int xoffset, int yoffset,
                                            const uint8_t *b, int b_stride,
                                            uint32_t *sse) {
  uint8_t temp0[4 * (8 + 2)];
  uint8_t temp1[4 * 8];

  var_filter_block2d_bil_w4(a, temp0, a_stride, 1, (8 + 2),
                            bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w4(temp0, temp1, 4, 4, 8,
                            bilinear_filters_2t[yoffset]);

  return aom_variance4x8(temp1, 4, b, b_stride, sse);
}

unsigned int aom_sub_pixel_variance8x4_neon(const uint8_t *a, int a_stride,
                                            int xoffset, int yoffset,
                                            const uint8_t *b, int b_stride,
                                            uint32_t *sse) {
  uint8_t temp0[8 * (4 + 1)];
  uint8_t temp1[8 * 4];

  var_filter_block2d_bil_w8(a, temp0, a_stride, 1, (4 + 1), 8,
                            bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w8(temp0, temp1, 8, 8, 4, 8,
                            bilinear_filters_2t[yoffset]);

  return aom_variance8x4(temp1, 8, b, b_stride, sse);
}

unsigned int aom_sub_pixel_variance8x16_neon(const uint8_t *a, int a_stride,
                                             int xoffset, int yoffset,
                                             const uint8_t *b, int b_stride,
                                             uint32_t *sse) {
  uint8_t temp0[8 * (16 + 1)];
  uint8_t temp1[8 * 16];

  var_filter_block2d_bil_w8(a, temp0, a_stride, 1, (16 + 1), 8,
                            bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w8(temp0, temp1, 8, 8, 16, 8,
                            bilinear_filters_2t[yoffset]);

  return aom_variance8x16(temp1, 8, b, b_stride, sse);
}

unsigned int aom_sub_pixel_variance16x8_neon(const uint8_t *a, int a_stride,
                                             int xoffset, int yoffset,
                                             const uint8_t *b, int b_stride,
                                             uint32_t *sse) {
  uint8_t temp0[16 * (8 + 1)];
  uint8_t temp1[16 * 8];

  var_filter_block2d_bil_w16(a, temp0, a_stride, 1, (8 + 1), 16,
                             bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w16(temp0, temp1, 16, 16, 8, 16,
                             bilinear_filters_2t[yoffset]);

  return aom_variance16x8(temp1, 16, b, b_stride, sse);
}

unsigned int aom_sub_pixel_variance16x32_neon(const uint8_t *a, int a_stride,
                                              int xoffset, int yoffset,
                                              const uint8_t *b, int b_stride,
                                              uint32_t *sse) {
  uint8_t temp0[16 * (32 + 1)];
  uint8_t temp1[16 * 32];

  var_filter_block2d_bil_w16(a, temp0, a_stride, 1, (32 + 1), 16,
                             bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w16(temp0, temp1, 16, 16, 32, 16,
                             bilinear_filters_2t[yoffset]);

  return aom_variance16x32(temp1, 16, b, b_stride, sse);
}

unsigned int aom_sub_pixel_variance32x16_neon(const uint8_t *a, int a_stride,
                                              int xoffset, int yoffset,
                                              const uint8_t *b, int b_stride,
                                              uint32_t *sse) {
  uint8_t temp0[32 * (16 + 1)];
  uint8_t temp1[32 * 16];

  var_filter_block2d_bil_w16(a, temp0, a_stride, 1, (16 + 1), 32,
                             bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w16(temp0, temp1, 32, 32, 16, 32,
                             bilinear_filters_2t[yoffset]);

  return aom_variance32x16(temp1, 32, b, b_stride, sse);
}

unsigned int aom_sub_pixel_variance32x64_neon(const uint8_t *a, int a_stride,
                                              int xoffset, int yoffset,
                                              const uint8_t *b, int b_stride,
                                              uint32_t *sse) {
  uint8_t temp0[32 * (64 + 1)];
  uint8_t temp1[32 * 64];

  var_filter_block2d_bil_w16(a, temp0, a_stride, 1, (64 + 1), 32,
                             bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w16(temp0, temp1, 32, 32, 64, 32,
                             bilinear_filters_2t[yoffset]);

  return aom_variance32x64(temp1, 32, b, b_stride, sse);
}

unsigned int aom_sub_pixel_variance64x32_neon(const uint8_t *a, int a_stride,
                                              int xoffset, int yoffset,
                                              const uint8_t *b, int b_stride,
                                              uint32_t *sse) {
  uint8_t temp0[64 * (32 + 1)];
  uint8_t temp1[64 * 32];

  var_filter_block2d_bil_w16(a, temp0, a_stride, 1, (32 + 1), 64,
                             bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w16(temp0, temp1, 64, 64, 32, 64,
                             bilinear_filters_2t[yoffset]);

  return aom_variance64x32(temp1, 64, b, b_stride, sse);
}

unsigned int aom_sub_pixel_variance64x128_neon(const uint8_t *a, int a_stride,
                                               int xoffset, int yoffset,
                                               const uint8_t *b, int b_stride,
                                               uint32_t *sse) {
  uint8_t temp0[64 * (128 + 1)];
  uint8_t temp1[64 * 128];

  var_filter_block2d_bil_w16(a, temp0, a_stride, 1, (128 + 1), 64,
                             bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w16(temp0, temp1, 64, 64, 128, 64,
                             bilinear_filters_2t[yoffset]);

  return aom_variance64x128(temp1, 64, b, b_stride, sse);
}

unsigned int aom_sub_pixel_variance128x64_neon(const uint8_t *a, int a_stride,
                                               int xoffset, int yoffset,
                                               const uint8_t *b, int b_stride,
                                               uint32_t *sse) {
  uint8_t temp0[128 * (64 + 1)];
  uint8_t temp1[128 * 64];

  var_filter_block2d_bil_w16(a, temp0, a_stride, 1, (64 + 1), 128,
                             bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w16(temp0, temp1, 128, 128, 64, 128,
                             bilinear_filters_2t[yoffset]);

  return aom_variance128x64(temp1, 128, b, b_stride, sse);
}

unsigned int aom_sub_pixel_variance128x128_neon(const uint8_t *a, int a_stride,
                                                int xoffset, int yoffset,
                                                const uint8_t *b, int b_stride,
                                                uint32_t *sse) {
  uint8_t temp0[128 * (128 + 1)];
  uint8_t temp1[128 * 128];

  var_filter_block2d_bil_w16(a, temp0, a_stride, 1, (128 + 1), 128,
                             bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w16(temp0, temp1, 128, 128, 128, 128,
                             bilinear_filters_2t[yoffset]);

  return aom_variance128x128(temp1, 128, b, b_stride, sse);
}

// Realtime mode doesn't use 4x rectangular blocks.
#if !CONFIG_REALTIME_ONLY
unsigned int aom_sub_pixel_variance4x16_neon(const uint8_t *a, int a_stride,
                                             int xoffset, int yoffset,
                                             const uint8_t *b, int b_stride,
                                             uint32_t *sse) {
  uint8_t temp0[4 * (16 + 2)];
  uint8_t temp1[4 * 16];

  var_filter_block2d_bil_w4(a, temp0, a_stride, 1, (16 + 2),
                            bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w4(temp0, temp1, 4, 4, 16,
                            bilinear_filters_2t[yoffset]);

  return aom_variance4x16(temp1, 4, b, b_stride, sse);
}

unsigned int aom_sub_pixel_variance8x32_neon(const uint8_t *a, int a_stride,
                                             int xoffset, int yoffset,
                                             const uint8_t *b, int b_stride,
                                             uint32_t *sse) {
  uint8_t temp0[8 * (32 + 1)];
  uint8_t temp1[8 * 32];

  var_filter_block2d_bil_w8(a, temp0, a_stride, 1, (32 + 1), 8,
                            bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w8(temp0, temp1, 8, 8, 32, 8,
                            bilinear_filters_2t[yoffset]);

  return aom_variance8x32(temp1, 8, b, b_stride, sse);
}

unsigned int aom_sub_pixel_variance16x4_neon(const uint8_t *a, int a_stride,
                                             int xoffset, int yoffset,
                                             const uint8_t *b, int b_stride,
                                             uint32_t *sse) {
  uint8_t temp0[16 * (4 + 1)];
  uint8_t temp1[16 * 4];

  var_filter_block2d_bil_w16(a, temp0, a_stride, 1, (4 + 1), 16,
                             bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w16(temp0, temp1, 16, 16, 4, 16,
                             bilinear_filters_2t[yoffset]);

  return aom_variance16x4(temp1, 16, b, b_stride, sse);
}

unsigned int aom_sub_pixel_variance64x16_neon(const uint8_t *a, int a_stride,
                                              int xoffset, int yoffset,
                                              const uint8_t *b, int b_stride,
                                              uint32_t *sse) {
  uint8_t temp0[64 * (16 + 1)];
  uint8_t temp1[64 * 16];

  var_filter_block2d_bil_w16(a, temp0, a_stride, 1, (16 + 1), 64,
                             bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w16(temp0, temp1, 64, 64, 16, 64,
                             bilinear_filters_2t[yoffset]);

  return aom_variance64x16(temp1, 64, b, b_stride, sse);
}

unsigned int aom_sub_pixel_variance16x64_neon(const uint8_t *a, int a_stride,
                                              int xoffset, int yoffset,
                                              const uint8_t *b, int b_stride,
                                              uint32_t *sse) {
  uint8_t temp0[16 * (64 + 1)];
  uint8_t temp1[16 * 64];

  var_filter_block2d_bil_w16(a, temp0, a_stride, 1, (64 + 1), 16,
                             bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w16(temp0, temp1, 16, 16, 64, 16,
                             bilinear_filters_2t[yoffset]);

  return aom_variance16x64(temp1, 16, b, b_stride, sse);
}

unsigned int aom_sub_pixel_variance32x8_neon(const uint8_t *a, int a_stride,
                                             int xoffset, int yoffset,
                                             const uint8_t *b, int b_stride,
                                             uint32_t *sse) {
  uint8_t temp0[32 * (8 + 1)];
  uint8_t temp1[32 * 8];

  var_filter_block2d_bil_w16(a, temp0, a_stride, 1, (8 + 1), 32,
                             bilinear_filters_2t[xoffset]);
  var_filter_block2d_bil_w16(temp0, temp1, 32, 32, 8, 32,
                             bilinear_filters_2t[yoffset]);

  return aom_variance32x8(temp1, 32, b, b_stride, sse);
}
#endif  // !CONFIG_REALTIME_ONLY
