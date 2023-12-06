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

#include "aom_dsp/variance.h"
#include "aom_dsp/arm/dist_wtd_avg_neon.h"
#include "aom_dsp/arm/mem_neon.h"

static void var_filter_block2d_bil_w4(const uint8_t *src_ptr, uint8_t *dst_ptr,
                                      int src_stride, int pixel_step,
                                      int dst_height, int filter_offset) {
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

static void var_filter_block2d_bil_w8(const uint8_t *src_ptr, uint8_t *dst_ptr,
                                      int src_stride, int pixel_step,
                                      int dst_height, int filter_offset) {
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

static void var_filter_block2d_bil_large(const uint8_t *src_ptr,
                                         uint8_t *dst_ptr, int src_stride,
                                         int pixel_step, int dst_width,
                                         int dst_height, int filter_offset) {
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

static void var_filter_block2d_bil_w16(const uint8_t *src_ptr, uint8_t *dst_ptr,
                                       int src_stride, int pixel_step,
                                       int dst_height, int filter_offset) {
  var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride, pixel_step, 16,
                               dst_height, filter_offset);
}

static void var_filter_block2d_bil_w32(const uint8_t *src_ptr, uint8_t *dst_ptr,
                                       int src_stride, int pixel_step,
                                       int dst_height, int filter_offset) {
  var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride, pixel_step, 32,
                               dst_height, filter_offset);
}

static void var_filter_block2d_bil_w64(const uint8_t *src_ptr, uint8_t *dst_ptr,
                                       int src_stride, int pixel_step,
                                       int dst_height, int filter_offset) {
  var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride, pixel_step, 64,
                               dst_height, filter_offset);
}

static void var_filter_block2d_bil_w128(const uint8_t *src_ptr,
                                        uint8_t *dst_ptr, int src_stride,
                                        int pixel_step, int dst_height,
                                        int filter_offset) {
  var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride, pixel_step, 128,
                               dst_height, filter_offset);
}

static void var_filter_block2d_avg(const uint8_t *src_ptr, uint8_t *dst_ptr,
                                   int src_stride, int pixel_step,
                                   int dst_width, int dst_height) {
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

#define SUBPEL_VARIANCE_WXH_NEON(w, h, padding)                          \
  unsigned int aom_sub_pixel_variance##w##x##h##_neon(                   \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,      \
      const uint8_t *ref, int ref_stride, uint32_t *sse) {               \
    uint8_t tmp0[w * (h + padding)];                                     \
    uint8_t tmp1[w * h];                                                 \
    var_filter_block2d_bil_w##w(src, tmp0, src_stride, 1, (h + padding), \
                                xoffset);                                \
    var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);           \
    return aom_variance##w##x##h(tmp1, w, ref, ref_stride, sse);         \
  }

#define SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(w, h, padding)                  \
  unsigned int aom_sub_pixel_variance##w##x##h##_neon(                       \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,          \
      const uint8_t *ref, int ref_stride, unsigned int *sse) {               \
    if (xoffset == 0) {                                                      \
      if (yoffset == 0) {                                                    \
        return aom_variance##w##x##h(src, src_stride, ref, ref_stride, sse); \
      } else if (yoffset == 4) {                                             \
        uint8_t tmp[w * h];                                                  \
        var_filter_block2d_avg(src, tmp, src_stride, src_stride, w, h);      \
        return aom_variance##w##x##h(tmp, w, ref, ref_stride, sse);          \
      } else {                                                               \
        uint8_t tmp[w * h];                                                  \
        var_filter_block2d_bil_w##w(src, tmp, src_stride, src_stride, h,     \
                                    yoffset);                                \
        return aom_variance##w##x##h(tmp, w, ref, ref_stride, sse);          \
      }                                                                      \
    } else if (xoffset == 4) {                                               \
      uint8_t tmp0[w * (h + padding)];                                       \
      if (yoffset == 0) {                                                    \
        var_filter_block2d_avg(src, tmp0, src_stride, 1, w, h);              \
        return aom_variance##w##x##h(tmp0, w, ref, ref_stride, sse);         \
      } else if (yoffset == 4) {                                             \
        uint8_t tmp1[w * (h + padding)];                                     \
        var_filter_block2d_avg(src, tmp0, src_stride, 1, w, (h + padding));  \
        var_filter_block2d_avg(tmp0, tmp1, w, w, w, h);                      \
        return aom_variance##w##x##h(tmp1, w, ref, ref_stride, sse);         \
      } else {                                                               \
        uint8_t tmp1[w * (h + padding)];                                     \
        var_filter_block2d_avg(src, tmp0, src_stride, 1, w, (h + padding));  \
        var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);           \
        return aom_variance##w##x##h(tmp1, w, ref, ref_stride, sse);         \
      }                                                                      \
    } else {                                                                 \
      uint8_t tmp0[w * (h + padding)];                                       \
      if (yoffset == 0) {                                                    \
        var_filter_block2d_bil_w##w(src, tmp0, src_stride, 1, h, xoffset);   \
        return aom_variance##w##x##h(tmp0, w, ref, ref_stride, sse);         \
      } else if (yoffset == 4) {                                             \
        uint8_t tmp1[w * h];                                                 \
        var_filter_block2d_bil_w##w(src, tmp0, src_stride, 1, (h + padding), \
                                    xoffset);                                \
        var_filter_block2d_avg(tmp0, tmp1, w, w, w, h);                      \
        return aom_variance##w##x##h(tmp1, w, ref, ref_stride, sse);         \
      } else {                                                               \
        uint8_t tmp1[w * h];                                                 \
        var_filter_block2d_bil_w##w(src, tmp0, src_stride, 1, (h + padding), \
                                    xoffset);                                \
        var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);           \
        return aom_variance##w##x##h(tmp1, w, ref, ref_stride, sse);         \
      }                                                                      \
    }                                                                        \
  }

SUBPEL_VARIANCE_WXH_NEON(4, 4, 2)
SUBPEL_VARIANCE_WXH_NEON(4, 8, 2)

SUBPEL_VARIANCE_WXH_NEON(8, 4, 1)
SUBPEL_VARIANCE_WXH_NEON(8, 8, 1)
SUBPEL_VARIANCE_WXH_NEON(8, 16, 1)

SUBPEL_VARIANCE_WXH_NEON(16, 8, 1)
SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(16, 16, 1)
SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(16, 32, 1)

SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(32, 16, 1)
SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(32, 32, 1)
SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(32, 64, 1)

SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(64, 32, 1)
SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(64, 64, 1)
SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(64, 128, 1)

SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(128, 64, 1)
SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(128, 128, 1)

// Realtime mode doesn't use 4x rectangular blocks.
#if !CONFIG_REALTIME_ONLY

SUBPEL_VARIANCE_WXH_NEON(4, 16, 2)

SUBPEL_VARIANCE_WXH_NEON(8, 32, 1)

SUBPEL_VARIANCE_WXH_NEON(16, 4, 1)
SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(16, 64, 1)

SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(32, 8, 1)

SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(64, 16, 1)

#endif  // !CONFIG_REALTIME_ONLY

#undef SUBPEL_VARIANCE_WXH_NEON
#undef SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON

// Combine bilinear filter with aom_comp_avg_pred for blocks having width 4.
static void avg_pred_var_filter_block2d_bil_w4(const uint8_t *src_ptr,
                                               uint8_t *dst_ptr, int src_stride,
                                               int pixel_step, int dst_height,
                                               int filter_offset,
                                               const uint8_t *second_pred) {
  const uint8x8_t f0 = vdup_n_u8(8 - filter_offset);
  const uint8x8_t f1 = vdup_n_u8(filter_offset);

  int i = dst_height;
  do {
    uint8x8_t s0 = load_unaligned_u8(src_ptr, src_stride);
    uint8x8_t s1 = load_unaligned_u8(src_ptr + pixel_step, src_stride);
    uint16x8_t blend = vmull_u8(s0, f0);
    blend = vmlal_u8(blend, s1, f1);
    uint8x8_t blend_u8 = vrshrn_n_u16(blend, 3);

    uint8x8_t p = vld1_u8(second_pred);
    uint8x8_t avg = vrhadd_u8(blend_u8, p);

    vst1_u8(dst_ptr, avg);

    src_ptr += 2 * src_stride;
    dst_ptr += 2 * 4;
    second_pred += 2 * 4;
    i -= 2;
  } while (i != 0);
}

// Combine bilinear filter with aom_dist_wtd_comp_avg_pred for blocks having
// width 4.
static void dist_wtd_avg_pred_var_filter_block2d_bil_w4(
    const uint8_t *src_ptr, uint8_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint8_t *second_pred,
    const DIST_WTD_COMP_PARAMS *jcp_param) {
  const uint8x8_t fwd_offset = vdup_n_u8(jcp_param->fwd_offset);
  const uint8x8_t bck_offset = vdup_n_u8(jcp_param->bck_offset);
  const uint8x8_t f0 = vdup_n_u8(8 - filter_offset);
  const uint8x8_t f1 = vdup_n_u8(filter_offset);

  int i = dst_height;
  do {
    uint8x8_t s0 = load_unaligned_u8(src_ptr, src_stride);
    uint8x8_t s1 = load_unaligned_u8(src_ptr + pixel_step, src_stride);
    uint8x8_t p = vld1_u8(second_pred);
    uint16x8_t blend = vmull_u8(s0, f0);
    blend = vmlal_u8(blend, s1, f1);
    uint8x8_t blend_u8 = vrshrn_n_u16(blend, 3);
    uint8x8_t avg = dist_wtd_avg_u8x8(blend_u8, p, fwd_offset, bck_offset);

    vst1_u8(dst_ptr, avg);

    src_ptr += 2 * src_stride;
    dst_ptr += 2 * 4;
    second_pred += 2 * 4;
    i -= 2;
  } while (i != 0);
}

// Combine bilinear filter with aom_comp_avg_pred for blocks having width 8.
static void avg_pred_var_filter_block2d_bil_w8(const uint8_t *src_ptr,
                                               uint8_t *dst_ptr, int src_stride,
                                               int pixel_step, int dst_height,
                                               int filter_offset,
                                               const uint8_t *second_pred) {
  const uint8x8_t f0 = vdup_n_u8(8 - filter_offset);
  const uint8x8_t f1 = vdup_n_u8(filter_offset);

  int i = dst_height;
  do {
    uint8x8_t s0 = vld1_u8(src_ptr);
    uint8x8_t s1 = vld1_u8(src_ptr + pixel_step);
    uint16x8_t blend = vmull_u8(s0, f0);
    blend = vmlal_u8(blend, s1, f1);
    uint8x8_t blend_u8 = vrshrn_n_u16(blend, 3);

    uint8x8_t p = vld1_u8(second_pred);
    uint8x8_t avg = vrhadd_u8(blend_u8, p);

    vst1_u8(dst_ptr, avg);

    src_ptr += src_stride;
    dst_ptr += 8;
    second_pred += 8;
  } while (--i > 0);
}

// Combine bilinear filter with aom_dist_wtd_comp_avg_pred for blocks having
// width 8.
static void dist_wtd_avg_pred_var_filter_block2d_bil_w8(
    const uint8_t *src_ptr, uint8_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint8_t *second_pred,
    const DIST_WTD_COMP_PARAMS *jcp_param) {
  const uint8x8_t fwd_offset = vdup_n_u8(jcp_param->fwd_offset);
  const uint8x8_t bck_offset = vdup_n_u8(jcp_param->bck_offset);
  const uint8x8_t f0 = vdup_n_u8(8 - filter_offset);
  const uint8x8_t f1 = vdup_n_u8(filter_offset);

  int i = dst_height;
  do {
    uint8x8_t s0 = vld1_u8(src_ptr);
    uint8x8_t s1 = vld1_u8(src_ptr + pixel_step);
    uint8x8_t p = vld1_u8(second_pred);
    uint16x8_t blend = vmull_u8(s0, f0);
    blend = vmlal_u8(blend, s1, f1);
    uint8x8_t blend_u8 = vrshrn_n_u16(blend, 3);
    uint8x8_t avg = dist_wtd_avg_u8x8(blend_u8, p, fwd_offset, bck_offset);

    vst1_u8(dst_ptr, avg);

    src_ptr += src_stride;
    dst_ptr += 8;
    second_pred += 8;
  } while (--i > 0);
}

// Combine bilinear filter with aom_comp_avg_pred for large blocks.
static void avg_pred_var_filter_block2d_bil_large(
    const uint8_t *src_ptr, uint8_t *dst_ptr, int src_stride, int pixel_step,
    int dst_width, int dst_height, int filter_offset,
    const uint8_t *second_pred) {
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

      uint8x16_t p = vld1q_u8(second_pred);
      uint8x16_t avg = vrhaddq_u8(blend_u8, p);

      vst1q_u8(dst_ptr + j, avg);

      j += 16;
      second_pred += 16;
    } while (j < dst_width);

    src_ptr += src_stride;
    dst_ptr += dst_width;
  } while (--i != 0);
}

// Combine bilinear filter with aom_dist_wtd_comp_avg_pred for large blocks.
static void dist_wtd_avg_pred_var_filter_block2d_bil_large(
    const uint8_t *src_ptr, uint8_t *dst_ptr, int src_stride, int pixel_step,
    int dst_width, int dst_height, int filter_offset,
    const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param) {
  const uint8x16_t fwd_offset = vdupq_n_u8(jcp_param->fwd_offset);
  const uint8x16_t bck_offset = vdupq_n_u8(jcp_param->bck_offset);
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

      uint8x16_t p = vld1q_u8(second_pred);
      uint8x16_t avg = dist_wtd_avg_u8x16(blend_u8, p, fwd_offset, bck_offset);

      vst1q_u8(dst_ptr + j, avg);

      j += 16;
      second_pred += 16;
    } while (j < dst_width);

    src_ptr += src_stride;
    dst_ptr += dst_width;
  } while (--i != 0);
}

// Combine bilinear filter with aom_comp_avg_pred for blocks having width 16.
static void avg_pred_var_filter_block2d_bil_w16(
    const uint8_t *src_ptr, uint8_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint8_t *second_pred) {
  avg_pred_var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride,
                                        pixel_step, 16, dst_height,
                                        filter_offset, second_pred);
}

// Combine bilinear filter with aom_comp_avg_pred for blocks having width 32.
static void avg_pred_var_filter_block2d_bil_w32(
    const uint8_t *src_ptr, uint8_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint8_t *second_pred) {
  avg_pred_var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride,
                                        pixel_step, 32, dst_height,
                                        filter_offset, second_pred);
}

// Combine bilinear filter with aom_comp_avg_pred for blocks having width 64.
static void avg_pred_var_filter_block2d_bil_w64(
    const uint8_t *src_ptr, uint8_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint8_t *second_pred) {
  avg_pred_var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride,
                                        pixel_step, 64, dst_height,
                                        filter_offset, second_pred);
}

// Combine bilinear filter with aom_comp_avg_pred for blocks having width 128.
static void avg_pred_var_filter_block2d_bil_w128(
    const uint8_t *src_ptr, uint8_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint8_t *second_pred) {
  avg_pred_var_filter_block2d_bil_large(src_ptr, dst_ptr, src_stride,
                                        pixel_step, 128, dst_height,
                                        filter_offset, second_pred);
}

// Combine bilinear filter with aom_comp_avg_pred for blocks having width 16.
static void dist_wtd_avg_pred_var_filter_block2d_bil_w16(
    const uint8_t *src_ptr, uint8_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint8_t *second_pred,
    const DIST_WTD_COMP_PARAMS *jcp_param) {
  dist_wtd_avg_pred_var_filter_block2d_bil_large(
      src_ptr, dst_ptr, src_stride, pixel_step, 16, dst_height, filter_offset,
      second_pred, jcp_param);
}

// Combine bilinear filter with aom_comp_avg_pred for blocks having width 32.
static void dist_wtd_avg_pred_var_filter_block2d_bil_w32(
    const uint8_t *src_ptr, uint8_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint8_t *second_pred,
    const DIST_WTD_COMP_PARAMS *jcp_param) {
  dist_wtd_avg_pred_var_filter_block2d_bil_large(
      src_ptr, dst_ptr, src_stride, pixel_step, 32, dst_height, filter_offset,
      second_pred, jcp_param);
}

// Combine bilinear filter with aom_comp_avg_pred for blocks having width 64.
static void dist_wtd_avg_pred_var_filter_block2d_bil_w64(
    const uint8_t *src_ptr, uint8_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint8_t *second_pred,
    const DIST_WTD_COMP_PARAMS *jcp_param) {
  dist_wtd_avg_pred_var_filter_block2d_bil_large(
      src_ptr, dst_ptr, src_stride, pixel_step, 64, dst_height, filter_offset,
      second_pred, jcp_param);
}

// Combine bilinear filter with aom_comp_avg_pred for blocks having width 128.
static void dist_wtd_avg_pred_var_filter_block2d_bil_w128(
    const uint8_t *src_ptr, uint8_t *dst_ptr, int src_stride, int pixel_step,
    int dst_height, int filter_offset, const uint8_t *second_pred,
    const DIST_WTD_COMP_PARAMS *jcp_param) {
  dist_wtd_avg_pred_var_filter_block2d_bil_large(
      src_ptr, dst_ptr, src_stride, pixel_step, 128, dst_height, filter_offset,
      second_pred, jcp_param);
}

// Combine averaging subpel filter with aom_comp_avg_pred.
static void avg_pred_var_filter_block2d_avg(const uint8_t *src_ptr,
                                            uint8_t *dst_ptr, int src_stride,
                                            int pixel_step, int dst_width,
                                            int dst_height,
                                            const uint8_t *second_pred) {
  // We only specialise on the filter values for large block sizes (>= 16x16.)
  assert(dst_width >= 16 && dst_width % 16 == 0);

  int i = dst_height;
  do {
    int j = 0;
    do {
      uint8x16_t s0 = vld1q_u8(src_ptr + j);
      uint8x16_t s1 = vld1q_u8(src_ptr + j + pixel_step);
      uint8x16_t avg = vrhaddq_u8(s0, s1);

      uint8x16_t p = vld1q_u8(second_pred);
      avg = vrhaddq_u8(avg, p);

      vst1q_u8(dst_ptr + j, avg);

      j += 16;
      second_pred += 16;
    } while (j < dst_width);

    src_ptr += src_stride;
    dst_ptr += dst_width;
  } while (--i != 0);
}

// Combine averaging subpel filter with aom_dist_wtd_comp_avg_pred.
static void dist_wtd_avg_pred_var_filter_block2d_avg(
    const uint8_t *src_ptr, uint8_t *dst_ptr, int src_stride, int pixel_step,
    int dst_width, int dst_height, const uint8_t *second_pred,
    const DIST_WTD_COMP_PARAMS *jcp_param) {
  // We only specialise on the filter values for large block sizes (>= 16x16.)
  assert(dst_width >= 16 && dst_width % 16 == 0);
  const uint8x16_t fwd_offset = vdupq_n_u8(jcp_param->fwd_offset);
  const uint8x16_t bck_offset = vdupq_n_u8(jcp_param->bck_offset);

  int i = dst_height;
  do {
    int j = 0;
    do {
      uint8x16_t s0 = vld1q_u8(src_ptr + j);
      uint8x16_t s1 = vld1q_u8(src_ptr + j + pixel_step);
      uint8x16_t p = vld1q_u8(second_pred);
      uint8x16_t avg = vrhaddq_u8(s0, s1);
      avg = dist_wtd_avg_u8x16(avg, p, fwd_offset, bck_offset);

      vst1q_u8(dst_ptr + j, avg);

      j += 16;
      second_pred += 16;
    } while (j < dst_width);

    src_ptr += src_stride;
    dst_ptr += dst_width;
  } while (--i != 0);
}

// Implementation of aom_comp_avg_pred for blocks having width >= 16.
static void avg_pred(const uint8_t *src_ptr, uint8_t *dst_ptr, int src_stride,
                     int dst_width, int dst_height,
                     const uint8_t *second_pred) {
  // We only specialise on the filter values for large block sizes (>= 16x16.)
  assert(dst_width >= 16 && dst_width % 16 == 0);

  int i = dst_height;
  do {
    int j = 0;
    do {
      uint8x16_t s = vld1q_u8(src_ptr + j);
      uint8x16_t p = vld1q_u8(second_pred);

      uint8x16_t avg = vrhaddq_u8(s, p);

      vst1q_u8(dst_ptr + j, avg);

      j += 16;
      second_pred += 16;
    } while (j < dst_width);

    src_ptr += src_stride;
    dst_ptr += dst_width;
  } while (--i != 0);
}

// Implementation of aom_dist_wtd_comp_avg_pred for blocks having width >= 16.
static void dist_wtd_avg_pred(const uint8_t *src_ptr, uint8_t *dst_ptr,
                              int src_stride, int dst_width, int dst_height,
                              const uint8_t *second_pred,
                              const DIST_WTD_COMP_PARAMS *jcp_param) {
  // We only specialise on the filter values for large block sizes (>= 16x16.)
  assert(dst_width >= 16 && dst_width % 16 == 0);
  const uint8x16_t fwd_offset = vdupq_n_u8(jcp_param->fwd_offset);
  const uint8x16_t bck_offset = vdupq_n_u8(jcp_param->bck_offset);

  int i = dst_height;
  do {
    int j = 0;
    do {
      uint8x16_t s = vld1q_u8(src_ptr + j);
      uint8x16_t p = vld1q_u8(second_pred);

      uint8x16_t avg = dist_wtd_avg_u8x16(s, p, fwd_offset, bck_offset);

      vst1q_u8(dst_ptr + j, avg);

      j += 16;
      second_pred += 16;
    } while (j < dst_width);

    src_ptr += src_stride;
    dst_ptr += dst_width;
  } while (--i != 0);
}

#define SUBPEL_AVG_VARIANCE_WXH_NEON(w, h, padding)                         \
  unsigned int aom_sub_pixel_avg_variance##w##x##h##_neon(                  \
      const uint8_t *src, int source_stride, int xoffset, int yoffset,      \
      const uint8_t *ref, int ref_stride, uint32_t *sse,                    \
      const uint8_t *second_pred) {                                         \
    uint8_t tmp0[w * (h + padding)];                                        \
    uint8_t tmp1[w * h];                                                    \
    var_filter_block2d_bil_w##w(src, tmp0, source_stride, 1, (h + padding), \
                                xoffset);                                   \
    avg_pred_var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset,      \
                                         second_pred);                      \
    return aom_variance##w##x##h(tmp1, w, ref, ref_stride, sse);            \
  }

#define SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(w, h, padding)                \
  unsigned int aom_sub_pixel_avg_variance##w##x##h##_neon(                     \
      const uint8_t *src, int source_stride, int xoffset, int yoffset,         \
      const uint8_t *ref, int ref_stride, unsigned int *sse,                   \
      const uint8_t *second_pred) {                                            \
    if (xoffset == 0) {                                                        \
      uint8_t tmp[w * h];                                                      \
      if (yoffset == 0) {                                                      \
        avg_pred(src, tmp, source_stride, w, h, second_pred);                  \
        return aom_variance##w##x##h(tmp, w, ref, ref_stride, sse);            \
      } else if (yoffset == 4) {                                               \
        avg_pred_var_filter_block2d_avg(src, tmp, source_stride,               \
                                        source_stride, w, h, second_pred);     \
        return aom_variance##w##x##h(tmp, w, ref, ref_stride, sse);            \
      } else {                                                                 \
        avg_pred_var_filter_block2d_bil_w##w(                                  \
            src, tmp, source_stride, source_stride, h, yoffset, second_pred);  \
        return aom_variance##w##x##h(tmp, w, ref, ref_stride, sse);            \
      }                                                                        \
    } else if (xoffset == 4) {                                                 \
      uint8_t tmp0[w * (h + padding)];                                         \
      if (yoffset == 0) {                                                      \
        avg_pred_var_filter_block2d_avg(src, tmp0, source_stride, 1, w, h,     \
                                        second_pred);                          \
        return aom_variance##w##x##h(tmp0, w, ref, ref_stride, sse);           \
      } else if (yoffset == 4) {                                               \
        uint8_t tmp1[w * (h + padding)];                                       \
        var_filter_block2d_avg(src, tmp0, source_stride, 1, w, (h + padding)); \
        avg_pred_var_filter_block2d_avg(tmp0, tmp1, w, w, w, h, second_pred);  \
        return aom_variance##w##x##h(tmp1, w, ref, ref_stride, sse);           \
      } else {                                                                 \
        uint8_t tmp1[w * (h + padding)];                                       \
        var_filter_block2d_avg(src, tmp0, source_stride, 1, w, (h + padding)); \
        avg_pred_var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset,     \
                                             second_pred);                     \
        return aom_variance##w##x##h(tmp1, w, ref, ref_stride, sse);           \
      }                                                                        \
    } else {                                                                   \
      uint8_t tmp0[w * (h + padding)];                                         \
      if (yoffset == 0) {                                                      \
        avg_pred_var_filter_block2d_bil_w##w(src, tmp0, source_stride, 1, h,   \
                                             xoffset, second_pred);            \
        return aom_variance##w##x##h(tmp0, w, ref, ref_stride, sse);           \
      } else if (yoffset == 4) {                                               \
        uint8_t tmp1[w * h];                                                   \
        var_filter_block2d_bil_w##w(src, tmp0, source_stride, 1,               \
                                    (h + padding), xoffset);                   \
        avg_pred_var_filter_block2d_avg(tmp0, tmp1, w, w, w, h, second_pred);  \
        return aom_variance##w##x##h(tmp1, w, ref, ref_stride, sse);           \
      } else {                                                                 \
        uint8_t tmp1[w * h];                                                   \
        var_filter_block2d_bil_w##w(src, tmp0, source_stride, 1,               \
                                    (h + padding), xoffset);                   \
        avg_pred_var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset,     \
                                             second_pred);                     \
        return aom_variance##w##x##h(tmp1, w, ref, ref_stride, sse);           \
      }                                                                        \
    }                                                                          \
  }

SUBPEL_AVG_VARIANCE_WXH_NEON(4, 4, 2)
SUBPEL_AVG_VARIANCE_WXH_NEON(4, 8, 2)

SUBPEL_AVG_VARIANCE_WXH_NEON(8, 4, 1)
SUBPEL_AVG_VARIANCE_WXH_NEON(8, 8, 1)
SUBPEL_AVG_VARIANCE_WXH_NEON(8, 16, 1)

SUBPEL_AVG_VARIANCE_WXH_NEON(16, 8, 1)
SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(16, 16, 1)
SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(16, 32, 1)

SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(32, 16, 1)
SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(32, 32, 1)
SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(32, 64, 1)

SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(64, 32, 1)
SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(64, 64, 1)
SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(64, 128, 1)

SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(128, 64, 1)
SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(128, 128, 1)

#if !CONFIG_REALTIME_ONLY

SUBPEL_AVG_VARIANCE_WXH_NEON(4, 16, 2)

SUBPEL_AVG_VARIANCE_WXH_NEON(8, 32, 1)

SUBPEL_AVG_VARIANCE_WXH_NEON(16, 4, 1)
SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(16, 64, 1)

SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(32, 8, 1)

SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON(64, 16, 1)

#endif  // !CONFIG_REALTIME_ONLY

#undef SUBPEL_AVG_VARIANCE_WXH_NEON
#undef SPECIALIZED_SUBPEL_AVG_VARIANCE_WXH_NEON

#define DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(w, h, padding)                \
  unsigned int aom_dist_wtd_sub_pixel_avg_variance##w##x##h##_neon(         \
      const uint8_t *src, int source_stride, int xoffset, int yoffset,      \
      const uint8_t *ref, int ref_stride, uint32_t *sse,                    \
      const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param) {  \
    uint8_t tmp0[w * (h + padding)];                                        \
    uint8_t tmp1[w * h];                                                    \
    var_filter_block2d_bil_w##w(src, tmp0, source_stride, 1, (h + padding), \
                                xoffset);                                   \
    dist_wtd_avg_pred_var_filter_block2d_bil_w##w(                          \
        tmp0, tmp1, w, w, h, yoffset, second_pred, jcp_param);              \
    return aom_variance##w##x##h(tmp1, w, ref, ref_stride, sse);            \
  }

#define SPECIALIZED_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(w, h, padding)       \
  unsigned int aom_dist_wtd_sub_pixel_avg_variance##w##x##h##_neon(            \
      const uint8_t *src, int source_stride, int xoffset, int yoffset,         \
      const uint8_t *ref, int ref_stride, unsigned int *sse,                   \
      const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param) {     \
    if (xoffset == 0) {                                                        \
      uint8_t tmp[w * h];                                                      \
      if (yoffset == 0) {                                                      \
        dist_wtd_avg_pred(src, tmp, source_stride, w, h, second_pred,          \
                          jcp_param);                                          \
        return aom_variance##w##x##h(tmp, w, ref, ref_stride, sse);            \
      } else if (yoffset == 4) {                                               \
        dist_wtd_avg_pred_var_filter_block2d_avg(src, tmp, source_stride,      \
                                                 source_stride, w, h,          \
                                                 second_pred, jcp_param);      \
        return aom_variance##w##x##h(tmp, w, ref, ref_stride, sse);            \
      } else {                                                                 \
        dist_wtd_avg_pred_var_filter_block2d_bil_w##w(                         \
            src, tmp, source_stride, source_stride, h, yoffset, second_pred,   \
            jcp_param);                                                        \
        return aom_variance##w##x##h(tmp, w, ref, ref_stride, sse);            \
      }                                                                        \
    } else if (xoffset == 4) {                                                 \
      uint8_t tmp0[w * (h + padding)];                                         \
      if (yoffset == 0) {                                                      \
        dist_wtd_avg_pred_var_filter_block2d_avg(                              \
            src, tmp0, source_stride, 1, w, h, second_pred, jcp_param);        \
        return aom_variance##w##x##h(tmp0, w, ref, ref_stride, sse);           \
      } else if (yoffset == 4) {                                               \
        uint8_t tmp1[w * (h + padding)];                                       \
        var_filter_block2d_avg(src, tmp0, source_stride, 1, w, (h + padding)); \
        dist_wtd_avg_pred_var_filter_block2d_avg(tmp0, tmp1, w, w, w, h,       \
                                                 second_pred, jcp_param);      \
        return aom_variance##w##x##h(tmp1, w, ref, ref_stride, sse);           \
      } else {                                                                 \
        uint8_t tmp1[w * (h + padding)];                                       \
        var_filter_block2d_avg(src, tmp0, source_stride, 1, w, (h + padding)); \
        dist_wtd_avg_pred_var_filter_block2d_bil_w##w(                         \
            tmp0, tmp1, w, w, h, yoffset, second_pred, jcp_param);             \
        return aom_variance##w##x##h(tmp1, w, ref, ref_stride, sse);           \
      }                                                                        \
    } else {                                                                   \
      uint8_t tmp0[w * (h + padding)];                                         \
      if (yoffset == 0) {                                                      \
        dist_wtd_avg_pred_var_filter_block2d_bil_w##w(                         \
            src, tmp0, source_stride, 1, h, xoffset, second_pred, jcp_param);  \
        return aom_variance##w##x##h(tmp0, w, ref, ref_stride, sse);           \
      } else if (yoffset == 4) {                                               \
        uint8_t tmp1[w * h];                                                   \
        var_filter_block2d_bil_w##w(src, tmp0, source_stride, 1,               \
                                    (h + padding), xoffset);                   \
        dist_wtd_avg_pred_var_filter_block2d_avg(tmp0, tmp1, w, w, w, h,       \
                                                 second_pred, jcp_param);      \
        return aom_variance##w##x##h(tmp1, w, ref, ref_stride, sse);           \
      } else {                                                                 \
        uint8_t tmp1[w * h];                                                   \
        var_filter_block2d_bil_w##w(src, tmp0, source_stride, 1,               \
                                    (h + padding), xoffset);                   \
        dist_wtd_avg_pred_var_filter_block2d_bil_w##w(                         \
            tmp0, tmp1, w, w, h, yoffset, second_pred, jcp_param);             \
        return aom_variance##w##x##h(tmp1, w, ref, ref_stride, sse);           \
      }                                                                        \
    }                                                                          \
  }

DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(4, 4, 2)
DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(4, 8, 2)

DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 4, 1)
DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 8, 1)
DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 16, 1)

DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(16, 8, 1)
SPECIALIZED_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(16, 16, 1)
SPECIALIZED_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(16, 32, 1)

SPECIALIZED_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(32, 16, 1)
SPECIALIZED_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(32, 32, 1)
SPECIALIZED_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(32, 64, 1)

SPECIALIZED_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(64, 32, 1)
SPECIALIZED_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(64, 64, 1)
SPECIALIZED_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(64, 128, 1)

SPECIALIZED_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(128, 64, 1)
SPECIALIZED_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(128, 128, 1)

#if !CONFIG_REALTIME_ONLY

DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(4, 16, 2)

DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(8, 32, 1)

DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(16, 4, 1)
SPECIALIZED_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(16, 64, 1)

SPECIALIZED_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(32, 8, 1)

SPECIALIZED_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON(64, 16, 1)

#endif  // !CONFIG_REALTIME_ONLY

#undef DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON
#undef SPECIALIZED_DIST_WTD_SUBPEL_AVG_VARIANCE_WXH_NEON

#if !CONFIG_REALTIME_ONLY

#define OBMC_SUBPEL_VARIANCE_WXH_NEON(w, h, padding)                   \
  unsigned int aom_obmc_sub_pixel_variance##w##x##h##_neon(            \
      const uint8_t *pre, int pre_stride, int xoffset, int yoffset,    \
      const int32_t *wsrc, const int32_t *mask, unsigned int *sse) {   \
    uint8_t tmp0[w * (h + padding)];                                   \
    uint8_t tmp1[w * h];                                               \
    var_filter_block2d_bil_w##w(pre, tmp0, pre_stride, 1, h + padding, \
                                xoffset);                              \
    var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);         \
    return aom_obmc_variance##w##x##h(tmp1, w, wsrc, mask, sse);       \
  }

#define SPECIALIZED_OBMC_SUBPEL_VARIANCE_WXH_NEON(w, h, padding)              \
  unsigned int aom_obmc_sub_pixel_variance##w##x##h##_neon(                   \
      const uint8_t *pre, int pre_stride, int xoffset, int yoffset,           \
      const int32_t *wsrc, const int32_t *mask, unsigned int *sse) {          \
    if (xoffset == 0) {                                                       \
      if (yoffset == 0) {                                                     \
        return aom_obmc_variance##w##x##h##_neon(pre, pre_stride, wsrc, mask, \
                                                 sse);                        \
      } else if (yoffset == 4) {                                              \
        uint8_t tmp[w * h];                                                   \
        var_filter_block2d_avg(pre, tmp, pre_stride, pre_stride, w, h);       \
        return aom_obmc_variance##w##x##h##_neon(tmp, w, wsrc, mask, sse);    \
      } else {                                                                \
        uint8_t tmp[w * h];                                                   \
        var_filter_block2d_bil_w##w(pre, tmp, pre_stride, pre_stride, h,      \
                                    yoffset);                                 \
        return aom_obmc_variance##w##x##h##_neon(tmp, w, wsrc, mask, sse);    \
      }                                                                       \
    } else if (xoffset == 4) {                                                \
      uint8_t tmp0[w * (h + padding)];                                        \
      if (yoffset == 0) {                                                     \
        var_filter_block2d_avg(pre, tmp0, pre_stride, 1, w, h);               \
        return aom_obmc_variance##w##x##h##_neon(tmp0, w, wsrc, mask, sse);   \
      } else if (yoffset == 4) {                                              \
        uint8_t tmp1[w * (h + padding)];                                      \
        var_filter_block2d_avg(pre, tmp0, pre_stride, 1, w, h + padding);     \
        var_filter_block2d_avg(tmp0, tmp1, w, w, w, h);                       \
        return aom_obmc_variance##w##x##h##_neon(tmp1, w, wsrc, mask, sse);   \
      } else {                                                                \
        uint8_t tmp1[w * (h + padding)];                                      \
        var_filter_block2d_avg(pre, tmp0, pre_stride, 1, w, h + padding);     \
        var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);            \
        return aom_obmc_variance##w##x##h##_neon(tmp1, w, wsrc, mask, sse);   \
      }                                                                       \
    } else {                                                                  \
      uint8_t tmp0[w * (h + padding)];                                        \
      if (yoffset == 0) {                                                     \
        var_filter_block2d_bil_w##w(pre, tmp0, pre_stride, 1, h, xoffset);    \
        return aom_obmc_variance##w##x##h##_neon(tmp0, w, wsrc, mask, sse);   \
      } else if (yoffset == 4) {                                              \
        uint8_t tmp1[w * h];                                                  \
        var_filter_block2d_bil_w##w(pre, tmp0, pre_stride, 1, h + padding,    \
                                    xoffset);                                 \
        var_filter_block2d_avg(tmp0, tmp1, w, w, w, h);                       \
        return aom_obmc_variance##w##x##h##_neon(tmp1, w, wsrc, mask, sse);   \
      } else {                                                                \
        uint8_t tmp1[w * h];                                                  \
        var_filter_block2d_bil_w##w(pre, tmp0, pre_stride, 1, h + padding,    \
                                    xoffset);                                 \
        var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);            \
        return aom_obmc_variance##w##x##h##_neon(tmp1, w, wsrc, mask, sse);   \
      }                                                                       \
    }                                                                         \
  }

OBMC_SUBPEL_VARIANCE_WXH_NEON(4, 4, 2)
OBMC_SUBPEL_VARIANCE_WXH_NEON(4, 8, 2)
OBMC_SUBPEL_VARIANCE_WXH_NEON(4, 16, 2)

OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 4, 1)
OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 8, 1)
OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 16, 1)
OBMC_SUBPEL_VARIANCE_WXH_NEON(8, 32, 1)

OBMC_SUBPEL_VARIANCE_WXH_NEON(16, 4, 1)
OBMC_SUBPEL_VARIANCE_WXH_NEON(16, 8, 1)
SPECIALIZED_OBMC_SUBPEL_VARIANCE_WXH_NEON(16, 16, 1)
SPECIALIZED_OBMC_SUBPEL_VARIANCE_WXH_NEON(16, 32, 1)
SPECIALIZED_OBMC_SUBPEL_VARIANCE_WXH_NEON(16, 64, 1)

SPECIALIZED_OBMC_SUBPEL_VARIANCE_WXH_NEON(32, 8, 1)
SPECIALIZED_OBMC_SUBPEL_VARIANCE_WXH_NEON(32, 16, 1)
SPECIALIZED_OBMC_SUBPEL_VARIANCE_WXH_NEON(32, 32, 1)
SPECIALIZED_OBMC_SUBPEL_VARIANCE_WXH_NEON(32, 64, 1)

SPECIALIZED_OBMC_SUBPEL_VARIANCE_WXH_NEON(64, 16, 1)
SPECIALIZED_OBMC_SUBPEL_VARIANCE_WXH_NEON(64, 32, 1)
SPECIALIZED_OBMC_SUBPEL_VARIANCE_WXH_NEON(64, 64, 1)
SPECIALIZED_OBMC_SUBPEL_VARIANCE_WXH_NEON(64, 128, 1)

SPECIALIZED_OBMC_SUBPEL_VARIANCE_WXH_NEON(128, 64, 1)
SPECIALIZED_OBMC_SUBPEL_VARIANCE_WXH_NEON(128, 128, 1)

#undef OBMC_SUBPEL_VARIANCE_WXH_NEON
#undef SPECIALIZED_OBMC_SUBPEL_VARIANCE_WXH_NEON
#endif  // !CONFIG_REALTIME_ONLY

#define MASKED_SUBPEL_VARIANCE_WXH_NEON(w, h, padding)                         \
  unsigned int aom_masked_sub_pixel_variance##w##x##h##_neon(                  \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,            \
      const uint8_t *ref, int ref_stride, const uint8_t *second_pred,          \
      const uint8_t *msk, int msk_stride, int invert_mask,                     \
      unsigned int *sse) {                                                     \
    uint8_t tmp0[w * (h + padding)];                                           \
    uint8_t tmp1[w * h];                                                       \
    uint8_t tmp2[w * h];                                                       \
    var_filter_block2d_bil_w##w(src, tmp0, src_stride, 1, (h + padding),       \
                                xoffset);                                      \
    var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);                 \
    aom_comp_mask_pred_neon(tmp2, second_pred, w, h, tmp1, w, msk, msk_stride, \
                            invert_mask);                                      \
    return aom_variance##w##x##h(tmp2, w, ref, ref_stride, sse);               \
  }

#define SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(w, h, padding)             \
  unsigned int aom_masked_sub_pixel_variance##w##x##h##_neon(                  \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,            \
      const uint8_t *ref, int ref_stride, const uint8_t *second_pred,          \
      const uint8_t *msk, int msk_stride, int invert_mask,                     \
      unsigned int *sse) {                                                     \
    if (xoffset == 0) {                                                        \
      uint8_t tmp0[w * h];                                                     \
      if (yoffset == 0) {                                                      \
        aom_comp_mask_pred_neon(tmp0, second_pred, w, h, src, src_stride, msk, \
                                msk_stride, invert_mask);                      \
        return aom_variance##w##x##h(tmp0, w, ref, ref_stride, sse);           \
      } else if (yoffset == 4) {                                               \
        uint8_t tmp1[w * h];                                                   \
        var_filter_block2d_avg(src, tmp0, src_stride, src_stride, w, h);       \
        aom_comp_mask_pred_neon(tmp1, second_pred, w, h, tmp0, w, msk,         \
                                msk_stride, invert_mask);                      \
        return aom_variance##w##x##h(tmp1, w, ref, ref_stride, sse);           \
      } else {                                                                 \
        uint8_t tmp1[w * h];                                                   \
        var_filter_block2d_bil_w##w(src, tmp0, src_stride, src_stride, h,      \
                                    yoffset);                                  \
        aom_comp_mask_pred_neon(tmp1, second_pred, w, h, tmp0, w, msk,         \
                                msk_stride, invert_mask);                      \
        return aom_variance##w##x##h(tmp1, w, ref, ref_stride, sse);           \
      }                                                                        \
    } else if (xoffset == 4) {                                                 \
      uint8_t tmp0[w * (h + padding)];                                         \
      if (yoffset == 0) {                                                      \
        uint8_t tmp1[w * h];                                                   \
        var_filter_block2d_avg(src, tmp0, src_stride, 1, w, h);                \
        aom_comp_mask_pred_neon(tmp1, second_pred, w, h, tmp0, w, msk,         \
                                msk_stride, invert_mask);                      \
        return aom_variance##w##x##h(tmp1, w, ref, ref_stride, sse);           \
      } else if (yoffset == 4) {                                               \
        uint8_t tmp1[w * h];                                                   \
        uint8_t tmp2[w * h];                                                   \
        var_filter_block2d_avg(src, tmp0, src_stride, 1, w, (h + padding));    \
        var_filter_block2d_avg(tmp0, tmp1, w, w, w, h);                        \
        aom_comp_mask_pred_neon(tmp2, second_pred, w, h, tmp1, w, msk,         \
                                msk_stride, invert_mask);                      \
        return aom_variance##w##x##h(tmp2, w, ref, ref_stride, sse);           \
      } else {                                                                 \
        uint8_t tmp1[w * h];                                                   \
        uint8_t tmp2[w * h];                                                   \
        var_filter_block2d_avg(src, tmp0, src_stride, 1, w, (h + padding));    \
        var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);             \
        aom_comp_mask_pred_neon(tmp2, second_pred, w, h, tmp1, w, msk,         \
                                msk_stride, invert_mask);                      \
        return aom_variance##w##x##h(tmp2, w, ref, ref_stride, sse);           \
      }                                                                        \
    } else {                                                                   \
      if (yoffset == 0) {                                                      \
        uint8_t tmp0[w * h];                                                   \
        uint8_t tmp1[w * h];                                                   \
        var_filter_block2d_bil_w##w(src, tmp0, src_stride, 1, h, xoffset);     \
        aom_comp_mask_pred_neon(tmp1, second_pred, w, h, tmp0, w, msk,         \
                                msk_stride, invert_mask);                      \
        return aom_variance##w##x##h(tmp1, w, ref, ref_stride, sse);           \
      } else if (yoffset == 4) {                                               \
        uint8_t tmp0[w * (h + padding)];                                       \
        uint8_t tmp1[w * h];                                                   \
        uint8_t tmp2[w * h];                                                   \
        var_filter_block2d_bil_w##w(src, tmp0, src_stride, 1, (h + padding),   \
                                    xoffset);                                  \
        var_filter_block2d_avg(tmp0, tmp1, w, w, w, h);                        \
        aom_comp_mask_pred_neon(tmp2, second_pred, w, h, tmp1, w, msk,         \
                                msk_stride, invert_mask);                      \
        return aom_variance##w##x##h(tmp2, w, ref, ref_stride, sse);           \
      } else {                                                                 \
        uint8_t tmp0[w * (h + padding)];                                       \
        uint8_t tmp1[w * (h + padding)];                                       \
        uint8_t tmp2[w * h];                                                   \
        var_filter_block2d_bil_w##w(src, tmp0, src_stride, 1, (h + padding),   \
                                    xoffset);                                  \
        var_filter_block2d_bil_w##w(tmp0, tmp1, w, w, h, yoffset);             \
        aom_comp_mask_pred_neon(tmp2, second_pred, w, h, tmp1, w, msk,         \
                                msk_stride, invert_mask);                      \
        return aom_variance##w##x##h(tmp2, w, ref, ref_stride, sse);           \
      }                                                                        \
    }                                                                          \
  }

MASKED_SUBPEL_VARIANCE_WXH_NEON(4, 4, 2)
MASKED_SUBPEL_VARIANCE_WXH_NEON(4, 8, 2)

MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 4, 1)
MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 8, 1)
MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 16, 1)

MASKED_SUBPEL_VARIANCE_WXH_NEON(16, 8, 1)
SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(16, 16, 1)
SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(16, 32, 1)

SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(32, 16, 1)
SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(32, 32, 1)
SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(32, 64, 1)

SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(64, 32, 1)
SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(64, 64, 1)
SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(64, 128, 1)

SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(128, 64, 1)
SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(128, 128, 1)

// Realtime mode doesn't use 4x rectangular blocks.
#if !CONFIG_REALTIME_ONLY
MASKED_SUBPEL_VARIANCE_WXH_NEON(4, 16, 2)
MASKED_SUBPEL_VARIANCE_WXH_NEON(8, 32, 1)
MASKED_SUBPEL_VARIANCE_WXH_NEON(16, 4, 1)
SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(16, 64, 1)
SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(32, 8, 1)
SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON(64, 16, 1)
#endif  // !CONFIG_REALTIME_ONLY

#undef MASKED_SUBPEL_VARIANCE_WXH_NEON
#undef SPECIALIZED_MASKED_SUBPEL_VARIANCE_WXH_NEON
