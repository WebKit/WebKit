/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved.
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
#include "aom_dsp/arm/transpose_neon.h"
#include "av1/common/arm/resize_neon.h"
#include "av1/common/resize.h"
#include "config/aom_scale_rtcd.h"
#include "config/av1_rtcd.h"

// clang-format off
DECLARE_ALIGNED(16, static const uint8_t, kScale2DotProdPermuteTbl[32]) = {
  0, 1, 2, 3, 2, 3, 4, 5, 4, 5,  6,  7,  6,  7,  8,  9,
  4, 5, 6, 7, 6, 7, 8, 9, 8, 9, 10, 11, 10, 11, 12, 13
};
DECLARE_ALIGNED(16, static const uint8_t, kScale4DotProdPermuteTbl[16]) = {
  0, 1, 2, 3, 4, 5, 6, 7, 4, 5, 6, 7, 8, 9, 10, 11
};
// clang-format on

static inline uint8x8_t scale_2_to_1_filter8_8(const uint8x16_t s0,
                                               const uint8x16_t s1,
                                               const uint8x16x2_t permute_tbl,
                                               const int8x8_t filter) {
  // Transform sample range to [-128, 127] for 8-bit signed dot product.
  int8x16_t s0_128 = vreinterpretq_s8_u8(vsubq_u8(s0, vdupq_n_u8(128)));
  int8x16_t s1_128 = vreinterpretq_s8_u8(vsubq_u8(s1, vdupq_n_u8(128)));

  // Permute samples ready for dot product.
  int8x16_t perm_samples[4] = { vqtbl1q_s8(s0_128, permute_tbl.val[0]),
                                vqtbl1q_s8(s0_128, permute_tbl.val[1]),
                                vqtbl1q_s8(s1_128, permute_tbl.val[0]),
                                vqtbl1q_s8(s1_128, permute_tbl.val[1]) };

  // Dot product constant:
  // The shim of 128 << FILTER_BITS is needed because we are subtracting 128
  // from every source value. The additional right shift by one is needed
  // because we halve the filter values.
  const int32x4_t acc = vdupq_n_s32((128 << FILTER_BITS) >> 1);

  // First 4 output values.
  int32x4_t sum0123 = vdotq_lane_s32(acc, perm_samples[0], filter, 0);
  sum0123 = vdotq_lane_s32(sum0123, perm_samples[1], filter, 1);
  // Second 4 output values.
  int32x4_t sum4567 = vdotq_lane_s32(acc, perm_samples[2], filter, 0);
  sum4567 = vdotq_lane_s32(sum4567, perm_samples[3], filter, 1);

  int16x8_t sum = vcombine_s16(vmovn_s32(sum0123), vmovn_s32(sum4567));

  // We halved the filter values so -1 from right shift.
  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

static inline void scale_2_to_1_horiz_8tap(const uint8_t *src,
                                           const int src_stride, int w, int h,
                                           uint8_t *dst, const int dst_stride,
                                           const int16x8_t filters) {
  const int8x8_t filter = vmovn_s16(filters);
  const uint8x16x2_t permute_tbl = vld1q_u8_x2(kScale2DotProdPermuteTbl);

  do {
    const uint8_t *s = src;
    uint8_t *d = dst;
    int width = w;
    do {
      uint8x16_t s0[2], s1[2], s2[2], s3[2], s4[2], s5[2], s6[2], s7[2];
      load_u8_16x8(s, src_stride, &s0[0], &s1[0], &s2[0], &s3[0], &s4[0],
                   &s5[0], &s6[0], &s7[0]);
      load_u8_16x8(s + 8, src_stride, &s0[1], &s1[1], &s2[1], &s3[1], &s4[1],
                   &s5[1], &s6[1], &s7[1]);

      uint8x8_t d0 = scale_2_to_1_filter8_8(s0[0], s0[1], permute_tbl, filter);
      uint8x8_t d1 = scale_2_to_1_filter8_8(s1[0], s1[1], permute_tbl, filter);
      uint8x8_t d2 = scale_2_to_1_filter8_8(s2[0], s2[1], permute_tbl, filter);
      uint8x8_t d3 = scale_2_to_1_filter8_8(s3[0], s3[1], permute_tbl, filter);

      uint8x8_t d4 = scale_2_to_1_filter8_8(s4[0], s4[1], permute_tbl, filter);
      uint8x8_t d5 = scale_2_to_1_filter8_8(s5[0], s5[1], permute_tbl, filter);
      uint8x8_t d6 = scale_2_to_1_filter8_8(s6[0], s6[1], permute_tbl, filter);
      uint8x8_t d7 = scale_2_to_1_filter8_8(s7[0], s7[1], permute_tbl, filter);

      store_u8_8x8(d, dst_stride, d0, d1, d2, d3, d4, d5, d6, d7);

      d += 8;
      s += 16;
      width -= 8;
    } while (width > 0);

    dst += 8 * dst_stride;
    src += 8 * src_stride;
    h -= 8;
  } while (h > 0);
}

static inline void scale_plane_2_to_1_8tap(const uint8_t *src,
                                           const int src_stride, uint8_t *dst,
                                           const int dst_stride, const int w,
                                           const int h,
                                           const int16_t *const filter_ptr,
                                           uint8_t *const im_block) {
  assert(w > 0 && h > 0);

  const int im_h = 2 * h + SUBPEL_TAPS - 3;
  const int im_stride = (w + 7) & ~7;
  // All filter values are even, halve them to fit in int8_t when applying
  // horizontal filter and stay in 16-bit elements when applying vertical
  // filter.
  const int16x8_t filters = vshrq_n_s16(vld1q_s16(filter_ptr), 1);

  const ptrdiff_t horiz_offset = SUBPEL_TAPS / 2 - 1;
  const ptrdiff_t vert_offset = (SUBPEL_TAPS / 2 - 1) * src_stride;

  scale_2_to_1_horiz_8tap(src - horiz_offset - vert_offset, src_stride, w, im_h,
                          im_block, im_stride, filters);

  // We can specialise the vertical filtering for 6-tap filters given that the
  // EIGHTTAP_SMOOTH and EIGHTTAP_REGULAR filters are 0-padded.
  scale_2_to_1_vert_6tap(im_block + im_stride, im_stride, w, h, dst, dst_stride,
                         filters);
}

static inline uint8x8_t scale_4_to_1_filter8_8(
    const uint8x16_t s0, const uint8x16_t s1, const uint8x16_t s2,
    const uint8x16_t s3, const uint8x16_t permute_tbl, const int8x8_t filter) {
  int8x16_t filters = vcombine_s8(filter, filter);

  // Transform sample range to [-128, 127] for 8-bit signed dot product.
  int8x16_t s0_128 = vreinterpretq_s8_u8(vsubq_u8(s0, vdupq_n_u8(128)));
  int8x16_t s1_128 = vreinterpretq_s8_u8(vsubq_u8(s1, vdupq_n_u8(128)));
  int8x16_t s2_128 = vreinterpretq_s8_u8(vsubq_u8(s2, vdupq_n_u8(128)));
  int8x16_t s3_128 = vreinterpretq_s8_u8(vsubq_u8(s3, vdupq_n_u8(128)));

  int8x16_t perm_samples[4] = { vqtbl1q_s8(s0_128, permute_tbl),
                                vqtbl1q_s8(s1_128, permute_tbl),
                                vqtbl1q_s8(s2_128, permute_tbl),
                                vqtbl1q_s8(s3_128, permute_tbl) };

  // Dot product constant:
  // The shim of 128 << FILTER_BITS is needed because we are subtracting 128
  // from every source value. The additional right shift by one is needed
  // because we halved the filter values and will use a pairwise add.
  const int32x4_t acc = vdupq_n_s32((128 << FILTER_BITS) >> 2);

  int32x4_t sum0 = vdotq_s32(acc, perm_samples[0], filters);
  int32x4_t sum1 = vdotq_s32(acc, perm_samples[1], filters);
  int32x4_t sum2 = vdotq_s32(acc, perm_samples[2], filters);
  int32x4_t sum3 = vdotq_s32(acc, perm_samples[3], filters);

  int32x4_t sum01 = vpaddq_s32(sum0, sum1);
  int32x4_t sum23 = vpaddq_s32(sum2, sum3);

  int16x8_t sum = vcombine_s16(vmovn_s32(sum01), vmovn_s32(sum23));

  // We halved the filter values so -1 from right shift.
  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

static inline void scale_4_to_1_horiz_8tap(const uint8_t *src,
                                           const int src_stride, int w, int h,
                                           uint8_t *dst, const int dst_stride,
                                           const int16x8_t filters) {
  const int8x8_t filter = vmovn_s16(filters);
  const uint8x16_t permute_tbl = vld1q_u8(kScale4DotProdPermuteTbl);

  do {
    const uint8_t *s = src;
    uint8_t *d = dst;
    int width = w;

    do {
      uint8x16_t s0, s1, s2, s3, s4, s5, s6, s7;
      load_u8_16x8(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

      uint8x8_t d0 =
          scale_4_to_1_filter8_8(s0, s1, s2, s3, permute_tbl, filter);
      uint8x8_t d1 =
          scale_4_to_1_filter8_8(s4, s5, s6, s7, permute_tbl, filter);

      store_u8x2_strided_x4(d + 0 * dst_stride, dst_stride, d0);
      store_u8x2_strided_x4(d + 4 * dst_stride, dst_stride, d1);

      d += 2;
      s += 8;
      width -= 2;
    } while (width > 0);

    dst += 8 * dst_stride;
    src += 8 * src_stride;
    h -= 8;
  } while (h > 0);
}

static inline void scale_plane_4_to_1_8tap(const uint8_t *src,
                                           const int src_stride, uint8_t *dst,
                                           const int dst_stride, const int w,
                                           const int h,
                                           const int16_t *const filter_ptr,
                                           uint8_t *const im_block) {
  assert(w > 0 && h > 0);
  const int im_h = 4 * h + SUBPEL_TAPS - 2;
  const int im_stride = (w + 1) & ~1;
  // All filter values are even, halve them to fit in int8_t when applying
  // horizontal filter and stay in 16-bit elements when applying vertical
  // filter.
  const int16x8_t filters = vshrq_n_s16(vld1q_s16(filter_ptr), 1);

  const ptrdiff_t horiz_offset = SUBPEL_TAPS / 2 - 1;
  const ptrdiff_t vert_offset = (SUBPEL_TAPS / 2 - 1) * src_stride;

  scale_4_to_1_horiz_8tap(src - horiz_offset - vert_offset, src_stride, w, im_h,
                          im_block, im_stride, filters);

  // We can specialise the vertical filtering for 6-tap filters given that the
  // EIGHTTAP_SMOOTH and EIGHTTAP_REGULAR filters are 0-padded.
  scale_4_to_1_vert_6tap(im_block + im_stride, im_stride, w, h, dst, dst_stride,
                         filters);
}

static inline bool has_normative_scaler_neon_dotprod(const int src_width,
                                                     const int src_height,
                                                     const int dst_width,
                                                     const int dst_height) {
  return (2 * dst_width == src_width && 2 * dst_height == src_height) ||
         (4 * dst_width == src_width && 4 * dst_height == src_height);
}

void av1_resize_and_extend_frame_neon_dotprod(const YV12_BUFFER_CONFIG *src,
                                              YV12_BUFFER_CONFIG *dst,
                                              const InterpFilter filter,
                                              const int phase,
                                              const int num_planes) {
  assert(filter == BILINEAR || filter == EIGHTTAP_SMOOTH ||
         filter == EIGHTTAP_REGULAR);

  bool has_normative_scaler =
      has_normative_scaler_neon_dotprod(src->y_crop_width, src->y_crop_height,
                                        dst->y_crop_width, dst->y_crop_height);

  if (num_planes > 1) {
    has_normative_scaler =
        has_normative_scaler && has_normative_scaler_neon_dotprod(
                                    src->uv_crop_width, src->uv_crop_height,
                                    dst->uv_crop_width, dst->uv_crop_height);
  }

  if (!has_normative_scaler || filter == BILINEAR || phase == 0) {
    av1_resize_and_extend_frame_neon(src, dst, filter, phase, num_planes);
    return;
  }

  // We use AOMMIN(num_planes, MAX_MB_PLANE) instead of num_planes to quiet
  // the static analysis warnings.
  int malloc_failed = 0;
  for (int i = 0; i < AOMMIN(num_planes, MAX_MB_PLANE); ++i) {
    const int is_uv = i > 0;
    const int src_w = src->crop_widths[is_uv];
    const int src_h = src->crop_heights[is_uv];
    const int dst_w = dst->crop_widths[is_uv];
    const int dst_h = dst->crop_heights[is_uv];
    const int dst_y_w = (dst->crop_widths[0] + 1) & ~1;
    const int dst_y_h = (dst->crop_heights[0] + 1) & ~1;

    if (2 * dst_w == src_w && 2 * dst_h == src_h) {
      const int buffer_stride = (dst_y_w + 7) & ~7;
      const int buffer_height = (2 * dst_y_h + SUBPEL_TAPS - 2 + 7) & ~7;
      uint8_t *const temp_buffer =
          (uint8_t *)malloc(buffer_stride * buffer_height);
      if (!temp_buffer) {
        malloc_failed = 1;
        break;
      }
      const InterpKernel *interp_kernel =
          (const InterpKernel *)av1_interp_filter_params_list[filter]
              .filter_ptr;
      scale_plane_2_to_1_8tap(src->buffers[i], src->strides[is_uv],
                              dst->buffers[i], dst->strides[is_uv], dst_w,
                              dst_h, interp_kernel[phase], temp_buffer);
      free(temp_buffer);
    } else if (4 * dst_w == src_w && 4 * dst_h == src_h) {
      const int buffer_stride = (dst_y_w + 1) & ~1;
      const int buffer_height = (4 * dst_y_h + SUBPEL_TAPS - 2 + 7) & ~7;
      uint8_t *const temp_buffer =
          (uint8_t *)malloc(buffer_stride * buffer_height);
      if (!temp_buffer) {
        malloc_failed = 1;
        break;
      }
      const InterpKernel *interp_kernel =
          (const InterpKernel *)av1_interp_filter_params_list[filter]
              .filter_ptr;
      scale_plane_4_to_1_8tap(src->buffers[i], src->strides[is_uv],
                              dst->buffers[i], dst->strides[is_uv], dst_w,
                              dst_h, interp_kernel[phase], temp_buffer);
      free(temp_buffer);
    }
  }

  if (malloc_failed) {
    av1_resize_and_extend_frame_c(src, dst, filter, phase, num_planes);
  } else {
    aom_extend_frame_borders(dst, num_planes);
  }
}
