/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include <assert.h>
#include <stdlib.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_ports/mem.h"

#include "aom_dsp/aom_filter.h"
#include "aom_dsp/blend.h"
#include "aom_dsp/variance.h"

#include "av1/common/filter.h"
#include "av1/common/reconinter.h"

uint32_t aom_get_mb_ss_c(const int16_t *a) {
  unsigned int i, sum = 0;

  for (i = 0; i < 256; ++i) {
    sum += a[i] * a[i];
  }

  return sum;
}

static void variance(const uint8_t *a, int a_stride, const uint8_t *b,
                     int b_stride, int w, int h, uint32_t *sse, int *sum) {
  int i, j;

  *sum = 0;
  *sse = 0;

  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; ++j) {
      const int diff = a[j] - b[j];
      *sum += diff;
      *sse += diff * diff;
    }

    a += a_stride;
    b += b_stride;
  }
}

uint32_t aom_sse_odd_size(const uint8_t *a, int a_stride, const uint8_t *b,
                          int b_stride, int w, int h) {
  uint32_t sse;
  int sum;
  variance(a, a_stride, b, b_stride, w, h, &sse, &sum);
  return sse;
}

// Applies a 1-D 2-tap bilinear filter to the source block in either horizontal
// or vertical direction to produce the filtered output block. Used to implement
// the first-pass of 2-D separable filter.
//
// Produces int16_t output to retain precision for the next pass. Two filter
// taps should sum to FILTER_WEIGHT. pixel_step defines whether the filter is
// applied horizontally (pixel_step = 1) or vertically (pixel_step = stride).
// It defines the offset required to move from one input to the next.
static void var_filter_block2d_bil_first_pass_c(
    const uint8_t *a, uint16_t *b, unsigned int src_pixels_per_line,
    unsigned int pixel_step, unsigned int output_height,
    unsigned int output_width, const uint8_t *filter) {
  unsigned int i, j;

  for (i = 0; i < output_height; ++i) {
    for (j = 0; j < output_width; ++j) {
      b[j] = ROUND_POWER_OF_TWO(
          (int)a[0] * filter[0] + (int)a[pixel_step] * filter[1], FILTER_BITS);

      ++a;
    }

    a += src_pixels_per_line - output_width;
    b += output_width;
  }
}

// Applies a 1-D 2-tap bilinear filter to the source block in either horizontal
// or vertical direction to produce the filtered output block. Used to implement
// the second-pass of 2-D separable filter.
//
// Requires 16-bit input as produced by filter_block2d_bil_first_pass. Two
// filter taps should sum to FILTER_WEIGHT. pixel_step defines whether the
// filter is applied horizontally (pixel_step = 1) or vertically
// (pixel_step = stride). It defines the offset required to move from one input
// to the next. Output is 8-bit.
static void var_filter_block2d_bil_second_pass_c(
    const uint16_t *a, uint8_t *b, unsigned int src_pixels_per_line,
    unsigned int pixel_step, unsigned int output_height,
    unsigned int output_width, const uint8_t *filter) {
  unsigned int i, j;

  for (i = 0; i < output_height; ++i) {
    for (j = 0; j < output_width; ++j) {
      b[j] = ROUND_POWER_OF_TWO(
          (int)a[0] * filter[0] + (int)a[pixel_step] * filter[1], FILTER_BITS);
      ++a;
    }

    a += src_pixels_per_line - output_width;
    b += output_width;
  }
}

#define VAR(W, H)                                                    \
  uint32_t aom_variance##W##x##H##_c(const uint8_t *a, int a_stride, \
                                     const uint8_t *b, int b_stride, \
                                     uint32_t *sse) {                \
    int sum;                                                         \
    variance(a, a_stride, b, b_stride, W, H, sse, &sum);             \
    return *sse - (uint32_t)(((int64_t)sum * sum) / (W * H));        \
  }

#define SUBPIX_VAR(W, H)                                                  \
  uint32_t aom_sub_pixel_variance##W##x##H##_c(                           \
      const uint8_t *a, int a_stride, int xoffset, int yoffset,           \
      const uint8_t *b, int b_stride, uint32_t *sse) {                    \
    uint16_t fdata3[(H + 1) * W];                                         \
    uint8_t temp2[H * W];                                                 \
                                                                          \
    var_filter_block2d_bil_first_pass_c(a, fdata3, a_stride, 1, H + 1, W, \
                                        bilinear_filters_2t[xoffset]);    \
    var_filter_block2d_bil_second_pass_c(fdata3, temp2, W, W, H, W,       \
                                         bilinear_filters_2t[yoffset]);   \
                                                                          \
    return aom_variance##W##x##H##_c(temp2, W, b, b_stride, sse);         \
  }

#define SUBPIX_AVG_VAR(W, H)                                                   \
  uint32_t aom_sub_pixel_avg_variance##W##x##H##_c(                            \
      const uint8_t *a, int a_stride, int xoffset, int yoffset,                \
      const uint8_t *b, int b_stride, uint32_t *sse,                           \
      const uint8_t *second_pred) {                                            \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint8_t temp2[H * W];                                                      \
    DECLARE_ALIGNED(16, uint8_t, temp3[H * W]);                                \
                                                                               \
    var_filter_block2d_bil_first_pass_c(a, fdata3, a_stride, 1, H + 1, W,      \
                                        bilinear_filters_2t[xoffset]);         \
    var_filter_block2d_bil_second_pass_c(fdata3, temp2, W, W, H, W,            \
                                         bilinear_filters_2t[yoffset]);        \
                                                                               \
    aom_comp_avg_pred(temp3, second_pred, W, H, temp2, W);                     \
                                                                               \
    return aom_variance##W##x##H##_c(temp3, W, b, b_stride, sse);              \
  }                                                                            \
  uint32_t aom_dist_wtd_sub_pixel_avg_variance##W##x##H##_c(                   \
      const uint8_t *a, int a_stride, int xoffset, int yoffset,                \
      const uint8_t *b, int b_stride, uint32_t *sse,                           \
      const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param) {     \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint8_t temp2[H * W];                                                      \
    DECLARE_ALIGNED(16, uint8_t, temp3[H * W]);                                \
                                                                               \
    var_filter_block2d_bil_first_pass_c(a, fdata3, a_stride, 1, H + 1, W,      \
                                        bilinear_filters_2t[xoffset]);         \
    var_filter_block2d_bil_second_pass_c(fdata3, temp2, W, W, H, W,            \
                                         bilinear_filters_2t[yoffset]);        \
                                                                               \
    aom_dist_wtd_comp_avg_pred(temp3, second_pred, W, H, temp2, W, jcp_param); \
                                                                               \
    return aom_variance##W##x##H(temp3, W, b, b_stride, sse);                  \
  }

void aom_get_var_sse_sum_8x8_quad_c(const uint8_t *a, int a_stride,
                                    const uint8_t *b, int b_stride,
                                    uint32_t *sse8x8, int *sum8x8,
                                    unsigned int *tot_sse, int *tot_sum,
                                    uint32_t *var8x8) {
  // Loop over 4 8x8 blocks. Process one 8x32 block.
  for (int k = 0; k < 4; k++) {
    variance(a + (k * 8), a_stride, b + (k * 8), b_stride, 8, 8, &sse8x8[k],
             &sum8x8[k]);
  }

  // Calculate variance at 8x8 level and total sse, sum of 8x32 block.
  *tot_sse += sse8x8[0] + sse8x8[1] + sse8x8[2] + sse8x8[3];
  *tot_sum += sum8x8[0] + sum8x8[1] + sum8x8[2] + sum8x8[3];
  for (int i = 0; i < 4; i++)
    var8x8[i] = sse8x8[i] - (uint32_t)(((int64_t)sum8x8[i] * sum8x8[i]) >> 6);
}

void aom_get_var_sse_sum_16x16_dual_c(const uint8_t *src_ptr, int source_stride,
                                      const uint8_t *ref_ptr, int ref_stride,
                                      uint32_t *sse16x16, unsigned int *tot_sse,
                                      int *tot_sum, uint32_t *var16x16) {
  int sum16x16[2] = { 0 };
  // Loop over two consecutive 16x16 blocks and process as one 16x32 block.
  for (int k = 0; k < 2; k++) {
    variance(src_ptr + (k * 16), source_stride, ref_ptr + (k * 16), ref_stride,
             16, 16, &sse16x16[k], &sum16x16[k]);
  }

  // Calculate variance at 16x16 level and total sse, sum of 16x32 block.
  *tot_sse += sse16x16[0] + sse16x16[1];
  *tot_sum += sum16x16[0] + sum16x16[1];
  for (int i = 0; i < 2; i++)
    var16x16[i] =
        sse16x16[i] - (uint32_t)(((int64_t)sum16x16[i] * sum16x16[i]) >> 8);
}

/* Identical to the variance call except it does not calculate the
 * sse - sum^2 / w*h and returns sse in addtion to modifying the passed in
 * variable.
 */
#define MSE(W, H)                                               \
  uint32_t aom_mse##W##x##H##_c(const uint8_t *a, int a_stride, \
                                const uint8_t *b, int b_stride, \
                                uint32_t *sse) {                \
    int sum;                                                    \
    variance(a, a_stride, b, b_stride, W, H, sse, &sum);        \
    return *sse;                                                \
  }

/* All three forms of the variance are available in the same sizes. */
#define VARIANCES(W, H) \
  VAR(W, H)             \
  SUBPIX_VAR(W, H)      \
  SUBPIX_AVG_VAR(W, H)

VARIANCES(128, 128)
VARIANCES(128, 64)
VARIANCES(64, 128)
VARIANCES(64, 64)
VARIANCES(64, 32)
VARIANCES(32, 64)
VARIANCES(32, 32)
VARIANCES(32, 16)
VARIANCES(16, 32)
VARIANCES(16, 16)
VARIANCES(16, 8)
VARIANCES(8, 16)
VARIANCES(8, 8)
VARIANCES(8, 4)
VARIANCES(4, 8)
VARIANCES(4, 4)

// Realtime mode doesn't use rectangular blocks.
#if !CONFIG_REALTIME_ONLY
VARIANCES(4, 16)
VARIANCES(16, 4)
VARIANCES(8, 32)
VARIANCES(32, 8)
VARIANCES(16, 64)
VARIANCES(64, 16)
#endif

MSE(16, 16)
MSE(16, 8)
MSE(8, 16)
MSE(8, 8)

void aom_comp_avg_pred_c(uint8_t *comp_pred, const uint8_t *pred, int width,
                         int height, const uint8_t *ref, int ref_stride) {
  int i, j;

  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const int tmp = pred[j] + ref[j];
      comp_pred[j] = ROUND_POWER_OF_TWO(tmp, 1);
    }
    comp_pred += width;
    pred += width;
    ref += ref_stride;
  }
}

void aom_dist_wtd_comp_avg_pred_c(uint8_t *comp_pred, const uint8_t *pred,
                                  int width, int height, const uint8_t *ref,
                                  int ref_stride,
                                  const DIST_WTD_COMP_PARAMS *jcp_param) {
  int i, j;
  const int fwd_offset = jcp_param->fwd_offset;
  const int bck_offset = jcp_param->bck_offset;

  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      int tmp = pred[j] * bck_offset + ref[j] * fwd_offset;
      tmp = ROUND_POWER_OF_TWO(tmp, DIST_PRECISION_BITS);
      comp_pred[j] = (uint8_t)tmp;
    }
    comp_pred += width;
    pred += width;
    ref += ref_stride;
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
static void highbd_variance64(const uint8_t *a8, int a_stride,
                              const uint8_t *b8, int b_stride, int w, int h,
                              uint64_t *sse, int64_t *sum) {
  const uint16_t *a = CONVERT_TO_SHORTPTR(a8);
  const uint16_t *b = CONVERT_TO_SHORTPTR(b8);
  int64_t tsum = 0;
  uint64_t tsse = 0;
  for (int i = 0; i < h; ++i) {
    int32_t lsum = 0;
    for (int j = 0; j < w; ++j) {
      const int diff = a[j] - b[j];
      lsum += diff;
      tsse += (uint32_t)(diff * diff);
    }
    tsum += lsum;
    a += a_stride;
    b += b_stride;
  }
  *sum = tsum;
  *sse = tsse;
}

uint64_t aom_highbd_sse_odd_size(const uint8_t *a, int a_stride,
                                 const uint8_t *b, int b_stride, int w, int h) {
  uint64_t sse;
  int64_t sum;
  highbd_variance64(a, a_stride, b, b_stride, w, h, &sse, &sum);
  return sse;
}

static void highbd_8_variance(const uint8_t *a8, int a_stride,
                              const uint8_t *b8, int b_stride, int w, int h,
                              uint32_t *sse, int *sum) {
  uint64_t sse_long = 0;
  int64_t sum_long = 0;
  highbd_variance64(a8, a_stride, b8, b_stride, w, h, &sse_long, &sum_long);
  *sse = (uint32_t)sse_long;
  *sum = (int)sum_long;
}

static void highbd_10_variance(const uint8_t *a8, int a_stride,
                               const uint8_t *b8, int b_stride, int w, int h,
                               uint32_t *sse, int *sum) {
  uint64_t sse_long = 0;
  int64_t sum_long = 0;
  highbd_variance64(a8, a_stride, b8, b_stride, w, h, &sse_long, &sum_long);
  *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_long, 4);
  *sum = (int)ROUND_POWER_OF_TWO(sum_long, 2);
}

static void highbd_12_variance(const uint8_t *a8, int a_stride,
                               const uint8_t *b8, int b_stride, int w, int h,
                               uint32_t *sse, int *sum) {
  uint64_t sse_long = 0;
  int64_t sum_long = 0;
  highbd_variance64(a8, a_stride, b8, b_stride, w, h, &sse_long, &sum_long);
  *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_long, 8);
  *sum = (int)ROUND_POWER_OF_TWO(sum_long, 4);
}

#define HIGHBD_VAR(W, H)                                                       \
  uint32_t aom_highbd_8_variance##W##x##H##_c(const uint8_t *a, int a_stride,  \
                                              const uint8_t *b, int b_stride,  \
                                              uint32_t *sse) {                 \
    int sum;                                                                   \
    highbd_8_variance(a, a_stride, b, b_stride, W, H, sse, &sum);              \
    return *sse - (uint32_t)(((int64_t)sum * sum) / (W * H));                  \
  }                                                                            \
                                                                               \
  uint32_t aom_highbd_10_variance##W##x##H##_c(const uint8_t *a, int a_stride, \
                                               const uint8_t *b, int b_stride, \
                                               uint32_t *sse) {                \
    int sum;                                                                   \
    int64_t var;                                                               \
    highbd_10_variance(a, a_stride, b, b_stride, W, H, sse, &sum);             \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) / (W * H));                  \
    return (var >= 0) ? (uint32_t)var : 0;                                     \
  }                                                                            \
                                                                               \
  uint32_t aom_highbd_12_variance##W##x##H##_c(const uint8_t *a, int a_stride, \
                                               const uint8_t *b, int b_stride, \
                                               uint32_t *sse) {                \
    int sum;                                                                   \
    int64_t var;                                                               \
    highbd_12_variance(a, a_stride, b, b_stride, W, H, sse, &sum);             \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) / (W * H));                  \
    return (var >= 0) ? (uint32_t)var : 0;                                     \
  }

#define HIGHBD_MSE(W, H)                                                      \
  uint32_t aom_highbd_8_mse##W##x##H##_c(const uint8_t *src, int src_stride,  \
                                         const uint8_t *ref, int ref_stride,  \
                                         uint32_t *sse) {                     \
    int sum;                                                                  \
    highbd_8_variance(src, src_stride, ref, ref_stride, W, H, sse, &sum);     \
    return *sse;                                                              \
  }                                                                           \
                                                                              \
  uint32_t aom_highbd_10_mse##W##x##H##_c(const uint8_t *src, int src_stride, \
                                          const uint8_t *ref, int ref_stride, \
                                          uint32_t *sse) {                    \
    int sum;                                                                  \
    highbd_10_variance(src, src_stride, ref, ref_stride, W, H, sse, &sum);    \
    return *sse;                                                              \
  }                                                                           \
                                                                              \
  uint32_t aom_highbd_12_mse##W##x##H##_c(const uint8_t *src, int src_stride, \
                                          const uint8_t *ref, int ref_stride, \
                                          uint32_t *sse) {                    \
    int sum;                                                                  \
    highbd_12_variance(src, src_stride, ref, ref_stride, W, H, sse, &sum);    \
    return *sse;                                                              \
  }

void aom_highbd_var_filter_block2d_bil_first_pass(
    const uint8_t *src_ptr8, uint16_t *output_ptr,
    unsigned int src_pixels_per_line, int pixel_step,
    unsigned int output_height, unsigned int output_width,
    const uint8_t *filter) {
  unsigned int i, j;
  uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src_ptr8);
  for (i = 0; i < output_height; ++i) {
    for (j = 0; j < output_width; ++j) {
      output_ptr[j] = ROUND_POWER_OF_TWO(
          (int)src_ptr[0] * filter[0] + (int)src_ptr[pixel_step] * filter[1],
          FILTER_BITS);

      ++src_ptr;
    }

    // Next row...
    src_ptr += src_pixels_per_line - output_width;
    output_ptr += output_width;
  }
}

void aom_highbd_var_filter_block2d_bil_second_pass(
    const uint16_t *src_ptr, uint16_t *output_ptr,
    unsigned int src_pixels_per_line, unsigned int pixel_step,
    unsigned int output_height, unsigned int output_width,
    const uint8_t *filter) {
  unsigned int i, j;

  for (i = 0; i < output_height; ++i) {
    for (j = 0; j < output_width; ++j) {
      output_ptr[j] = ROUND_POWER_OF_TWO(
          (int)src_ptr[0] * filter[0] + (int)src_ptr[pixel_step] * filter[1],
          FILTER_BITS);
      ++src_ptr;
    }

    src_ptr += src_pixels_per_line - output_width;
    output_ptr += output_width;
  }
}

#define HIGHBD_SUBPIX_VAR(W, H)                                              \
  uint32_t aom_highbd_8_sub_pixel_variance##W##x##H##_c(                     \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,          \
      const uint8_t *dst, int dst_stride, uint32_t *sse) {                   \
    uint16_t fdata3[(H + 1) * W];                                            \
    uint16_t temp2[H * W];                                                   \
                                                                             \
    aom_highbd_var_filter_block2d_bil_first_pass(                            \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]); \
    aom_highbd_var_filter_block2d_bil_second_pass(                           \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);            \
                                                                             \
    return aom_highbd_8_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp2), W,  \
                                              dst, dst_stride, sse);         \
  }                                                                          \
                                                                             \
  uint32_t aom_highbd_10_sub_pixel_variance##W##x##H##_c(                    \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,          \
      const uint8_t *dst, int dst_stride, uint32_t *sse) {                   \
    uint16_t fdata3[(H + 1) * W];                                            \
    uint16_t temp2[H * W];                                                   \
                                                                             \
    aom_highbd_var_filter_block2d_bil_first_pass(                            \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]); \
    aom_highbd_var_filter_block2d_bil_second_pass(                           \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);            \
                                                                             \
    return aom_highbd_10_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp2), W, \
                                               dst, dst_stride, sse);        \
  }                                                                          \
                                                                             \
  uint32_t aom_highbd_12_sub_pixel_variance##W##x##H##_c(                    \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,          \
      const uint8_t *dst, int dst_stride, uint32_t *sse) {                   \
    uint16_t fdata3[(H + 1) * W];                                            \
    uint16_t temp2[H * W];                                                   \
                                                                             \
    aom_highbd_var_filter_block2d_bil_first_pass(                            \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]); \
    aom_highbd_var_filter_block2d_bil_second_pass(                           \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);            \
                                                                             \
    return aom_highbd_12_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp2), W, \
                                               dst, dst_stride, sse);        \
  }

#define HIGHBD_SUBPIX_AVG_VAR(W, H)                                           \
  uint32_t aom_highbd_8_sub_pixel_avg_variance##W##x##H##_c(                  \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *dst, int dst_stride, uint32_t *sse,                      \
      const uint8_t *second_pred) {                                           \
    uint16_t fdata3[(H + 1) * W];                                             \
    uint16_t temp2[H * W];                                                    \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                              \
                                                                              \
    aom_highbd_var_filter_block2d_bil_first_pass(                             \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);  \
    aom_highbd_var_filter_block2d_bil_second_pass(                            \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);             \
                                                                              \
    aom_highbd_comp_avg_pred_c(CONVERT_TO_BYTEPTR(temp3), second_pred, W, H,  \
                               CONVERT_TO_BYTEPTR(temp2), W);                 \
                                                                              \
    return aom_highbd_8_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp3), W,   \
                                              dst, dst_stride, sse);          \
  }                                                                           \
                                                                              \
  uint32_t aom_highbd_10_sub_pixel_avg_variance##W##x##H##_c(                 \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *dst, int dst_stride, uint32_t *sse,                      \
      const uint8_t *second_pred) {                                           \
    uint16_t fdata3[(H + 1) * W];                                             \
    uint16_t temp2[H * W];                                                    \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                              \
                                                                              \
    aom_highbd_var_filter_block2d_bil_first_pass(                             \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);  \
    aom_highbd_var_filter_block2d_bil_second_pass(                            \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);             \
                                                                              \
    aom_highbd_comp_avg_pred_c(CONVERT_TO_BYTEPTR(temp3), second_pred, W, H,  \
                               CONVERT_TO_BYTEPTR(temp2), W);                 \
                                                                              \
    return aom_highbd_10_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp3), W,  \
                                               dst, dst_stride, sse);         \
  }                                                                           \
                                                                              \
  uint32_t aom_highbd_12_sub_pixel_avg_variance##W##x##H##_c(                 \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *dst, int dst_stride, uint32_t *sse,                      \
      const uint8_t *second_pred) {                                           \
    uint16_t fdata3[(H + 1) * W];                                             \
    uint16_t temp2[H * W];                                                    \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                              \
                                                                              \
    aom_highbd_var_filter_block2d_bil_first_pass(                             \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);  \
    aom_highbd_var_filter_block2d_bil_second_pass(                            \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);             \
                                                                              \
    aom_highbd_comp_avg_pred_c(CONVERT_TO_BYTEPTR(temp3), second_pred, W, H,  \
                               CONVERT_TO_BYTEPTR(temp2), W);                 \
                                                                              \
    return aom_highbd_12_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp3), W,  \
                                               dst, dst_stride, sse);         \
  }                                                                           \
                                                                              \
  uint32_t aom_highbd_8_dist_wtd_sub_pixel_avg_variance##W##x##H##_c(         \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *dst, int dst_stride, uint32_t *sse,                      \
      const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param) {    \
    uint16_t fdata3[(H + 1) * W];                                             \
    uint16_t temp2[H * W];                                                    \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                              \
                                                                              \
    aom_highbd_var_filter_block2d_bil_first_pass(                             \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);  \
    aom_highbd_var_filter_block2d_bil_second_pass(                            \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);             \
                                                                              \
    aom_highbd_dist_wtd_comp_avg_pred(CONVERT_TO_BYTEPTR(temp3), second_pred, \
                                      W, H, CONVERT_TO_BYTEPTR(temp2), W,     \
                                      jcp_param);                             \
                                                                              \
    return aom_highbd_8_variance##W##x##H(CONVERT_TO_BYTEPTR(temp3), W, dst,  \
                                          dst_stride, sse);                   \
  }                                                                           \
                                                                              \
  uint32_t aom_highbd_10_dist_wtd_sub_pixel_avg_variance##W##x##H##_c(        \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *dst, int dst_stride, uint32_t *sse,                      \
      const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param) {    \
    uint16_t fdata3[(H + 1) * W];                                             \
    uint16_t temp2[H * W];                                                    \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                              \
                                                                              \
    aom_highbd_var_filter_block2d_bil_first_pass(                             \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);  \
    aom_highbd_var_filter_block2d_bil_second_pass(                            \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);             \
                                                                              \
    aom_highbd_dist_wtd_comp_avg_pred(CONVERT_TO_BYTEPTR(temp3), second_pred, \
                                      W, H, CONVERT_TO_BYTEPTR(temp2), W,     \
                                      jcp_param);                             \
                                                                              \
    return aom_highbd_10_variance##W##x##H(CONVERT_TO_BYTEPTR(temp3), W, dst, \
                                           dst_stride, sse);                  \
  }                                                                           \
                                                                              \
  uint32_t aom_highbd_12_dist_wtd_sub_pixel_avg_variance##W##x##H##_c(        \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *dst, int dst_stride, uint32_t *sse,                      \
      const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param) {    \
    uint16_t fdata3[(H + 1) * W];                                             \
    uint16_t temp2[H * W];                                                    \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                              \
                                                                              \
    aom_highbd_var_filter_block2d_bil_first_pass(                             \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);  \
    aom_highbd_var_filter_block2d_bil_second_pass(                            \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);             \
                                                                              \
    aom_highbd_dist_wtd_comp_avg_pred(CONVERT_TO_BYTEPTR(temp3), second_pred, \
                                      W, H, CONVERT_TO_BYTEPTR(temp2), W,     \
                                      jcp_param);                             \
                                                                              \
    return aom_highbd_12_variance##W##x##H(CONVERT_TO_BYTEPTR(temp3), W, dst, \
                                           dst_stride, sse);                  \
  }

/* All three forms of the variance are available in the same sizes. */
#define HIGHBD_VARIANCES(W, H) \
  HIGHBD_VAR(W, H)             \
  HIGHBD_SUBPIX_VAR(W, H)      \
  HIGHBD_SUBPIX_AVG_VAR(W, H)

HIGHBD_VARIANCES(128, 128)
HIGHBD_VARIANCES(128, 64)
HIGHBD_VARIANCES(64, 128)
HIGHBD_VARIANCES(64, 64)
HIGHBD_VARIANCES(64, 32)
HIGHBD_VARIANCES(32, 64)
HIGHBD_VARIANCES(32, 32)
HIGHBD_VARIANCES(32, 16)
HIGHBD_VARIANCES(16, 32)
HIGHBD_VARIANCES(16, 16)
HIGHBD_VARIANCES(16, 8)
HIGHBD_VARIANCES(8, 16)
HIGHBD_VARIANCES(8, 8)
HIGHBD_VARIANCES(8, 4)
HIGHBD_VARIANCES(4, 8)
HIGHBD_VARIANCES(4, 4)

// Realtime mode doesn't use 4x rectangular blocks.
#if !CONFIG_REALTIME_ONLY
HIGHBD_VARIANCES(4, 16)
HIGHBD_VARIANCES(16, 4)
HIGHBD_VARIANCES(8, 32)
HIGHBD_VARIANCES(32, 8)
HIGHBD_VARIANCES(16, 64)
HIGHBD_VARIANCES(64, 16)
#endif

HIGHBD_MSE(16, 16)
HIGHBD_MSE(16, 8)
HIGHBD_MSE(8, 16)
HIGHBD_MSE(8, 8)

void aom_highbd_comp_avg_pred_c(uint8_t *comp_pred8, const uint8_t *pred8,
                                int width, int height, const uint8_t *ref8,
                                int ref_stride) {
  int i, j;
  uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const int tmp = pred[j] + ref[j];
      comp_pred[j] = ROUND_POWER_OF_TWO(tmp, 1);
    }
    comp_pred += width;
    pred += width;
    ref += ref_stride;
  }
}

void aom_highbd_dist_wtd_comp_avg_pred_c(
    uint8_t *comp_pred8, const uint8_t *pred8, int width, int height,
    const uint8_t *ref8, int ref_stride,
    const DIST_WTD_COMP_PARAMS *jcp_param) {
  int i, j;
  const int fwd_offset = jcp_param->fwd_offset;
  const int bck_offset = jcp_param->bck_offset;
  uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);

  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      int tmp = pred[j] * bck_offset + ref[j] * fwd_offset;
      tmp = ROUND_POWER_OF_TWO(tmp, DIST_PRECISION_BITS);
      comp_pred[j] = (uint16_t)tmp;
    }
    comp_pred += width;
    pred += width;
    ref += ref_stride;
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

void aom_comp_mask_pred_c(uint8_t *comp_pred, const uint8_t *pred, int width,
                          int height, const uint8_t *ref, int ref_stride,
                          const uint8_t *mask, int mask_stride,
                          int invert_mask) {
  int i, j;
  const uint8_t *src0 = invert_mask ? pred : ref;
  const uint8_t *src1 = invert_mask ? ref : pred;
  const int stride0 = invert_mask ? width : ref_stride;
  const int stride1 = invert_mask ? ref_stride : width;
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      comp_pred[j] = AOM_BLEND_A64(mask[j], src0[j], src1[j]);
    }
    comp_pred += width;
    src0 += stride0;
    src1 += stride1;
    mask += mask_stride;
  }
}

#define MASK_SUBPIX_VAR(W, H)                                                 \
  unsigned int aom_masked_sub_pixel_variance##W##x##H##_c(                    \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *ref, int ref_stride, const uint8_t *second_pred,         \
      const uint8_t *msk, int msk_stride, int invert_mask,                    \
      unsigned int *sse) {                                                    \
    uint16_t fdata3[(H + 1) * W];                                             \
    uint8_t temp2[H * W];                                                     \
    DECLARE_ALIGNED(16, uint8_t, temp3[H * W]);                               \
                                                                              \
    var_filter_block2d_bil_first_pass_c(src, fdata3, src_stride, 1, H + 1, W, \
                                        bilinear_filters_2t[xoffset]);        \
    var_filter_block2d_bil_second_pass_c(fdata3, temp2, W, W, H, W,           \
                                         bilinear_filters_2t[yoffset]);       \
                                                                              \
    aom_comp_mask_pred_c(temp3, second_pred, W, H, temp2, W, msk, msk_stride, \
                         invert_mask);                                        \
    return aom_variance##W##x##H##_c(temp3, W, ref, ref_stride, sse);         \
  }

MASK_SUBPIX_VAR(4, 4)
MASK_SUBPIX_VAR(4, 8)
MASK_SUBPIX_VAR(8, 4)
MASK_SUBPIX_VAR(8, 8)
MASK_SUBPIX_VAR(8, 16)
MASK_SUBPIX_VAR(16, 8)
MASK_SUBPIX_VAR(16, 16)
MASK_SUBPIX_VAR(16, 32)
MASK_SUBPIX_VAR(32, 16)
MASK_SUBPIX_VAR(32, 32)
MASK_SUBPIX_VAR(32, 64)
MASK_SUBPIX_VAR(64, 32)
MASK_SUBPIX_VAR(64, 64)
MASK_SUBPIX_VAR(64, 128)
MASK_SUBPIX_VAR(128, 64)
MASK_SUBPIX_VAR(128, 128)

// Realtime mode doesn't use 4x rectangular blocks.
#if !CONFIG_REALTIME_ONLY
MASK_SUBPIX_VAR(4, 16)
MASK_SUBPIX_VAR(16, 4)
MASK_SUBPIX_VAR(8, 32)
MASK_SUBPIX_VAR(32, 8)
MASK_SUBPIX_VAR(16, 64)
MASK_SUBPIX_VAR(64, 16)
#endif

#if CONFIG_AV1_HIGHBITDEPTH
void aom_highbd_comp_mask_pred_c(uint8_t *comp_pred8, const uint8_t *pred8,
                                 int width, int height, const uint8_t *ref8,
                                 int ref_stride, const uint8_t *mask,
                                 int mask_stride, int invert_mask) {
  int i, j;
  uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      if (!invert_mask)
        comp_pred[j] = AOM_BLEND_A64(mask[j], ref[j], pred[j]);
      else
        comp_pred[j] = AOM_BLEND_A64(mask[j], pred[j], ref[j]);
    }
    comp_pred += width;
    pred += width;
    ref += ref_stride;
    mask += mask_stride;
  }
}

#define HIGHBD_MASK_SUBPIX_VAR(W, H)                                           \
  unsigned int aom_highbd_8_masked_sub_pixel_variance##W##x##H##_c(            \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,            \
      const uint8_t *ref, int ref_stride, const uint8_t *second_pred,          \
      const uint8_t *msk, int msk_stride, int invert_mask,                     \
      unsigned int *sse) {                                                     \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint16_t temp2[H * W];                                                     \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                               \
                                                                               \
    aom_highbd_var_filter_block2d_bil_first_pass(                              \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);   \
    aom_highbd_var_filter_block2d_bil_second_pass(                             \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);              \
                                                                               \
    aom_highbd_comp_mask_pred_c(CONVERT_TO_BYTEPTR(temp3), second_pred, W, H,  \
                                CONVERT_TO_BYTEPTR(temp2), W, msk, msk_stride, \
                                invert_mask);                                  \
                                                                               \
    return aom_highbd_8_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp3), W,    \
                                              ref, ref_stride, sse);           \
  }                                                                            \
                                                                               \
  unsigned int aom_highbd_10_masked_sub_pixel_variance##W##x##H##_c(           \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,            \
      const uint8_t *ref, int ref_stride, const uint8_t *second_pred,          \
      const uint8_t *msk, int msk_stride, int invert_mask,                     \
      unsigned int *sse) {                                                     \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint16_t temp2[H * W];                                                     \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                               \
                                                                               \
    aom_highbd_var_filter_block2d_bil_first_pass(                              \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);   \
    aom_highbd_var_filter_block2d_bil_second_pass(                             \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);              \
                                                                               \
    aom_highbd_comp_mask_pred_c(CONVERT_TO_BYTEPTR(temp3), second_pred, W, H,  \
                                CONVERT_TO_BYTEPTR(temp2), W, msk, msk_stride, \
                                invert_mask);                                  \
                                                                               \
    return aom_highbd_10_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp3), W,   \
                                               ref, ref_stride, sse);          \
  }                                                                            \
                                                                               \
  unsigned int aom_highbd_12_masked_sub_pixel_variance##W##x##H##_c(           \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,            \
      const uint8_t *ref, int ref_stride, const uint8_t *second_pred,          \
      const uint8_t *msk, int msk_stride, int invert_mask,                     \
      unsigned int *sse) {                                                     \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint16_t temp2[H * W];                                                     \
    DECLARE_ALIGNED(16, uint16_t, temp3[H * W]);                               \
                                                                               \
    aom_highbd_var_filter_block2d_bil_first_pass(                              \
        src, fdata3, src_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);   \
    aom_highbd_var_filter_block2d_bil_second_pass(                             \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);              \
                                                                               \
    aom_highbd_comp_mask_pred_c(CONVERT_TO_BYTEPTR(temp3), second_pred, W, H,  \
                                CONVERT_TO_BYTEPTR(temp2), W, msk, msk_stride, \
                                invert_mask);                                  \
                                                                               \
    return aom_highbd_12_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp3), W,   \
                                               ref, ref_stride, sse);          \
  }

HIGHBD_MASK_SUBPIX_VAR(4, 4)
HIGHBD_MASK_SUBPIX_VAR(4, 8)
HIGHBD_MASK_SUBPIX_VAR(8, 4)
HIGHBD_MASK_SUBPIX_VAR(8, 8)
HIGHBD_MASK_SUBPIX_VAR(8, 16)
HIGHBD_MASK_SUBPIX_VAR(16, 8)
HIGHBD_MASK_SUBPIX_VAR(16, 16)
HIGHBD_MASK_SUBPIX_VAR(16, 32)
HIGHBD_MASK_SUBPIX_VAR(32, 16)
HIGHBD_MASK_SUBPIX_VAR(32, 32)
HIGHBD_MASK_SUBPIX_VAR(32, 64)
HIGHBD_MASK_SUBPIX_VAR(64, 32)
HIGHBD_MASK_SUBPIX_VAR(64, 64)
HIGHBD_MASK_SUBPIX_VAR(64, 128)
HIGHBD_MASK_SUBPIX_VAR(128, 64)
HIGHBD_MASK_SUBPIX_VAR(128, 128)
#if !CONFIG_REALTIME_ONLY
HIGHBD_MASK_SUBPIX_VAR(4, 16)
HIGHBD_MASK_SUBPIX_VAR(16, 4)
HIGHBD_MASK_SUBPIX_VAR(8, 32)
HIGHBD_MASK_SUBPIX_VAR(32, 8)
HIGHBD_MASK_SUBPIX_VAR(16, 64)
HIGHBD_MASK_SUBPIX_VAR(64, 16)
#endif
#endif  // CONFIG_AV1_HIGHBITDEPTH

#if !CONFIG_REALTIME_ONLY
static INLINE void obmc_variance(const uint8_t *pre, int pre_stride,
                                 const int32_t *wsrc, const int32_t *mask,
                                 int w, int h, unsigned int *sse, int *sum) {
  int i, j;

  *sse = 0;
  *sum = 0;

  for (i = 0; i < h; i++) {
    for (j = 0; j < w; j++) {
      int diff = ROUND_POWER_OF_TWO_SIGNED(wsrc[j] - pre[j] * mask[j], 12);
      *sum += diff;
      *sse += diff * diff;
    }

    pre += pre_stride;
    wsrc += w;
    mask += w;
  }
}

#define OBMC_VAR(W, H)                                            \
  unsigned int aom_obmc_variance##W##x##H##_c(                    \
      const uint8_t *pre, int pre_stride, const int32_t *wsrc,    \
      const int32_t *mask, unsigned int *sse) {                   \
    int sum;                                                      \
    obmc_variance(pre, pre_stride, wsrc, mask, W, H, sse, &sum);  \
    return *sse - (unsigned int)(((int64_t)sum * sum) / (W * H)); \
  }

#define OBMC_SUBPIX_VAR(W, H)                                                 \
  unsigned int aom_obmc_sub_pixel_variance##W##x##H##_c(                      \
      const uint8_t *pre, int pre_stride, int xoffset, int yoffset,           \
      const int32_t *wsrc, const int32_t *mask, unsigned int *sse) {          \
    uint16_t fdata3[(H + 1) * W];                                             \
    uint8_t temp2[H * W];                                                     \
                                                                              \
    var_filter_block2d_bil_first_pass_c(pre, fdata3, pre_stride, 1, H + 1, W, \
                                        bilinear_filters_2t[xoffset]);        \
    var_filter_block2d_bil_second_pass_c(fdata3, temp2, W, W, H, W,           \
                                         bilinear_filters_2t[yoffset]);       \
                                                                              \
    return aom_obmc_variance##W##x##H##_c(temp2, W, wsrc, mask, sse);         \
  }

OBMC_VAR(4, 4)
OBMC_SUBPIX_VAR(4, 4)

OBMC_VAR(4, 8)
OBMC_SUBPIX_VAR(4, 8)

OBMC_VAR(8, 4)
OBMC_SUBPIX_VAR(8, 4)

OBMC_VAR(8, 8)
OBMC_SUBPIX_VAR(8, 8)

OBMC_VAR(8, 16)
OBMC_SUBPIX_VAR(8, 16)

OBMC_VAR(16, 8)
OBMC_SUBPIX_VAR(16, 8)

OBMC_VAR(16, 16)
OBMC_SUBPIX_VAR(16, 16)

OBMC_VAR(16, 32)
OBMC_SUBPIX_VAR(16, 32)

OBMC_VAR(32, 16)
OBMC_SUBPIX_VAR(32, 16)

OBMC_VAR(32, 32)
OBMC_SUBPIX_VAR(32, 32)

OBMC_VAR(32, 64)
OBMC_SUBPIX_VAR(32, 64)

OBMC_VAR(64, 32)
OBMC_SUBPIX_VAR(64, 32)

OBMC_VAR(64, 64)
OBMC_SUBPIX_VAR(64, 64)

OBMC_VAR(64, 128)
OBMC_SUBPIX_VAR(64, 128)

OBMC_VAR(128, 64)
OBMC_SUBPIX_VAR(128, 64)

OBMC_VAR(128, 128)
OBMC_SUBPIX_VAR(128, 128)

OBMC_VAR(4, 16)
OBMC_SUBPIX_VAR(4, 16)
OBMC_VAR(16, 4)
OBMC_SUBPIX_VAR(16, 4)
OBMC_VAR(8, 32)
OBMC_SUBPIX_VAR(8, 32)
OBMC_VAR(32, 8)
OBMC_SUBPIX_VAR(32, 8)
OBMC_VAR(16, 64)
OBMC_SUBPIX_VAR(16, 64)
OBMC_VAR(64, 16)
OBMC_SUBPIX_VAR(64, 16)

#if CONFIG_AV1_HIGHBITDEPTH
static INLINE void highbd_obmc_variance64(const uint8_t *pre8, int pre_stride,
                                          const int32_t *wsrc,
                                          const int32_t *mask, int w, int h,
                                          uint64_t *sse, int64_t *sum) {
  int i, j;
  uint16_t *pre = CONVERT_TO_SHORTPTR(pre8);

  *sse = 0;
  *sum = 0;

  for (i = 0; i < h; i++) {
    for (j = 0; j < w; j++) {
      int diff = ROUND_POWER_OF_TWO_SIGNED(wsrc[j] - pre[j] * mask[j], 12);
      *sum += diff;
      *sse += diff * diff;
    }

    pre += pre_stride;
    wsrc += w;
    mask += w;
  }
}

static INLINE void highbd_obmc_variance(const uint8_t *pre8, int pre_stride,
                                        const int32_t *wsrc,
                                        const int32_t *mask, int w, int h,
                                        unsigned int *sse, int *sum) {
  int64_t sum64;
  uint64_t sse64;
  highbd_obmc_variance64(pre8, pre_stride, wsrc, mask, w, h, &sse64, &sum64);
  *sum = (int)sum64;
  *sse = (unsigned int)sse64;
}

static INLINE void highbd_10_obmc_variance(const uint8_t *pre8, int pre_stride,
                                           const int32_t *wsrc,
                                           const int32_t *mask, int w, int h,
                                           unsigned int *sse, int *sum) {
  int64_t sum64;
  uint64_t sse64;
  highbd_obmc_variance64(pre8, pre_stride, wsrc, mask, w, h, &sse64, &sum64);
  *sum = (int)ROUND_POWER_OF_TWO(sum64, 2);
  *sse = (unsigned int)ROUND_POWER_OF_TWO(sse64, 4);
}

static INLINE void highbd_12_obmc_variance(const uint8_t *pre8, int pre_stride,
                                           const int32_t *wsrc,
                                           const int32_t *mask, int w, int h,
                                           unsigned int *sse, int *sum) {
  int64_t sum64;
  uint64_t sse64;
  highbd_obmc_variance64(pre8, pre_stride, wsrc, mask, w, h, &sse64, &sum64);
  *sum = (int)ROUND_POWER_OF_TWO(sum64, 4);
  *sse = (unsigned int)ROUND_POWER_OF_TWO(sse64, 8);
}

#define HIGHBD_OBMC_VAR(W, H)                                              \
  unsigned int aom_highbd_8_obmc_variance##W##x##H##_c(                    \
      const uint8_t *pre, int pre_stride, const int32_t *wsrc,             \
      const int32_t *mask, unsigned int *sse) {                            \
    int sum;                                                               \
    highbd_obmc_variance(pre, pre_stride, wsrc, mask, W, H, sse, &sum);    \
    return *sse - (unsigned int)(((int64_t)sum * sum) / (W * H));          \
  }                                                                        \
                                                                           \
  unsigned int aom_highbd_10_obmc_variance##W##x##H##_c(                   \
      const uint8_t *pre, int pre_stride, const int32_t *wsrc,             \
      const int32_t *mask, unsigned int *sse) {                            \
    int sum;                                                               \
    int64_t var;                                                           \
    highbd_10_obmc_variance(pre, pre_stride, wsrc, mask, W, H, sse, &sum); \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) / (W * H));              \
    return (var >= 0) ? (uint32_t)var : 0;                                 \
  }                                                                        \
                                                                           \
  unsigned int aom_highbd_12_obmc_variance##W##x##H##_c(                   \
      const uint8_t *pre, int pre_stride, const int32_t *wsrc,             \
      const int32_t *mask, unsigned int *sse) {                            \
    int sum;                                                               \
    int64_t var;                                                           \
    highbd_12_obmc_variance(pre, pre_stride, wsrc, mask, W, H, sse, &sum); \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) / (W * H));              \
    return (var >= 0) ? (uint32_t)var : 0;                                 \
  }

#define HIGHBD_OBMC_SUBPIX_VAR(W, H)                                           \
  unsigned int aom_highbd_8_obmc_sub_pixel_variance##W##x##H##_c(              \
      const uint8_t *pre, int pre_stride, int xoffset, int yoffset,            \
      const int32_t *wsrc, const int32_t *mask, unsigned int *sse) {           \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint16_t temp2[H * W];                                                     \
                                                                               \
    aom_highbd_var_filter_block2d_bil_first_pass(                              \
        pre, fdata3, pre_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);   \
    aom_highbd_var_filter_block2d_bil_second_pass(                             \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);              \
                                                                               \
    return aom_highbd_8_obmc_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp2),  \
                                                   W, wsrc, mask, sse);        \
  }                                                                            \
                                                                               \
  unsigned int aom_highbd_10_obmc_sub_pixel_variance##W##x##H##_c(             \
      const uint8_t *pre, int pre_stride, int xoffset, int yoffset,            \
      const int32_t *wsrc, const int32_t *mask, unsigned int *sse) {           \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint16_t temp2[H * W];                                                     \
                                                                               \
    aom_highbd_var_filter_block2d_bil_first_pass(                              \
        pre, fdata3, pre_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);   \
    aom_highbd_var_filter_block2d_bil_second_pass(                             \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);              \
                                                                               \
    return aom_highbd_10_obmc_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp2), \
                                                    W, wsrc, mask, sse);       \
  }                                                                            \
                                                                               \
  unsigned int aom_highbd_12_obmc_sub_pixel_variance##W##x##H##_c(             \
      const uint8_t *pre, int pre_stride, int xoffset, int yoffset,            \
      const int32_t *wsrc, const int32_t *mask, unsigned int *sse) {           \
    uint16_t fdata3[(H + 1) * W];                                              \
    uint16_t temp2[H * W];                                                     \
                                                                               \
    aom_highbd_var_filter_block2d_bil_first_pass(                              \
        pre, fdata3, pre_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);   \
    aom_highbd_var_filter_block2d_bil_second_pass(                             \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);              \
                                                                               \
    return aom_highbd_12_obmc_variance##W##x##H##_c(CONVERT_TO_BYTEPTR(temp2), \
                                                    W, wsrc, mask, sse);       \
  }

HIGHBD_OBMC_VAR(4, 4)
HIGHBD_OBMC_SUBPIX_VAR(4, 4)

HIGHBD_OBMC_VAR(4, 8)
HIGHBD_OBMC_SUBPIX_VAR(4, 8)

HIGHBD_OBMC_VAR(8, 4)
HIGHBD_OBMC_SUBPIX_VAR(8, 4)

HIGHBD_OBMC_VAR(8, 8)
HIGHBD_OBMC_SUBPIX_VAR(8, 8)

HIGHBD_OBMC_VAR(8, 16)
HIGHBD_OBMC_SUBPIX_VAR(8, 16)

HIGHBD_OBMC_VAR(16, 8)
HIGHBD_OBMC_SUBPIX_VAR(16, 8)

HIGHBD_OBMC_VAR(16, 16)
HIGHBD_OBMC_SUBPIX_VAR(16, 16)

HIGHBD_OBMC_VAR(16, 32)
HIGHBD_OBMC_SUBPIX_VAR(16, 32)

HIGHBD_OBMC_VAR(32, 16)
HIGHBD_OBMC_SUBPIX_VAR(32, 16)

HIGHBD_OBMC_VAR(32, 32)
HIGHBD_OBMC_SUBPIX_VAR(32, 32)

HIGHBD_OBMC_VAR(32, 64)
HIGHBD_OBMC_SUBPIX_VAR(32, 64)

HIGHBD_OBMC_VAR(64, 32)
HIGHBD_OBMC_SUBPIX_VAR(64, 32)

HIGHBD_OBMC_VAR(64, 64)
HIGHBD_OBMC_SUBPIX_VAR(64, 64)

HIGHBD_OBMC_VAR(64, 128)
HIGHBD_OBMC_SUBPIX_VAR(64, 128)

HIGHBD_OBMC_VAR(128, 64)
HIGHBD_OBMC_SUBPIX_VAR(128, 64)

HIGHBD_OBMC_VAR(128, 128)
HIGHBD_OBMC_SUBPIX_VAR(128, 128)

HIGHBD_OBMC_VAR(4, 16)
HIGHBD_OBMC_SUBPIX_VAR(4, 16)
HIGHBD_OBMC_VAR(16, 4)
HIGHBD_OBMC_SUBPIX_VAR(16, 4)
HIGHBD_OBMC_VAR(8, 32)
HIGHBD_OBMC_SUBPIX_VAR(8, 32)
HIGHBD_OBMC_VAR(32, 8)
HIGHBD_OBMC_SUBPIX_VAR(32, 8)
HIGHBD_OBMC_VAR(16, 64)
HIGHBD_OBMC_SUBPIX_VAR(16, 64)
HIGHBD_OBMC_VAR(64, 16)
HIGHBD_OBMC_SUBPIX_VAR(64, 16)
#endif  // CONFIG_AV1_HIGHBITDEPTH
#endif  // !CONFIG_REALTIME_ONLY

uint64_t aom_mse_wxh_16bit_c(uint8_t *dst, int dstride, uint16_t *src,
                             int sstride, int w, int h) {
  uint64_t sum = 0;
  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {
      int e = (uint16_t)dst[i * dstride + j] - src[i * sstride + j];
      sum += e * e;
    }
  }
  return sum;
}

uint64_t aom_mse_16xh_16bit_c(uint8_t *dst, int dstride, uint16_t *src, int w,
                              int h) {
  uint16_t *src_temp = src;
  uint8_t *dst_temp = dst;
  const int num_blks = 16 / w;
  int64_t sum = 0;
  for (int i = 0; i < num_blks; i++) {
    sum += aom_mse_wxh_16bit_c(dst_temp, dstride, src_temp, w, w, h);
    dst_temp += w;
    src_temp += (w * h);
  }
  return sum;
}

#if CONFIG_AV1_HIGHBITDEPTH
uint64_t aom_mse_wxh_16bit_highbd_c(uint16_t *dst, int dstride, uint16_t *src,
                                    int sstride, int w, int h) {
  uint64_t sum = 0;
  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {
      int e = dst[i * dstride + j] - src[i * sstride + j];
      sum += e * e;
    }
  }
  return sum;
}
#endif  // CONFIG_AV1_HIGHBITDEPTH
