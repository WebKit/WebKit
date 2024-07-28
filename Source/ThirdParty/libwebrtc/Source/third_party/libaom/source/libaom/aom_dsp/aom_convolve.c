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
#include <string.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "aom_ports/mem.h"

static INLINE int horz_scalar_product(const uint8_t *a, const int16_t *b) {
  int sum = 0;
  for (int k = 0; k < SUBPEL_TAPS; ++k) sum += a[k] * b[k];
  return sum;
}

static INLINE int vert_scalar_product(const uint8_t *a, ptrdiff_t a_stride,
                                      const int16_t *b) {
  int sum = 0;
  for (int k = 0; k < SUBPEL_TAPS; ++k) sum += a[k * a_stride] * b[k];
  return sum;
}

static void convolve_horiz(const uint8_t *src, ptrdiff_t src_stride,
                           uint8_t *dst, ptrdiff_t dst_stride,
                           const InterpKernel *x_filters, int x0_q4,
                           int x_step_q4, int w, int h) {
  src -= SUBPEL_TAPS / 2 - 1;
  for (int y = 0; y < h; ++y) {
    int x_q4 = x0_q4;
    for (int x = 0; x < w; ++x) {
      const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
      const int sum = horz_scalar_product(src_x, x_filter);
      dst[x] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void convolve_vert(const uint8_t *src, ptrdiff_t src_stride,
                          uint8_t *dst, ptrdiff_t dst_stride,
                          const InterpKernel *y_filters, int y0_q4,
                          int y_step_q4, int w, int h) {
  src -= src_stride * (SUBPEL_TAPS / 2 - 1);

  for (int x = 0; x < w; ++x) {
    int y_q4 = y0_q4;
    for (int y = 0; y < h; ++y) {
      const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
      const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
      const int sum = vert_scalar_product(src_y, src_stride, y_filter);
      dst[y * dst_stride] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

static const InterpKernel *get_filter_base(const int16_t *filter) {
  // NOTE: This assumes that the filter table is 256-byte aligned.
  return (const InterpKernel *)(((intptr_t)filter) & ~((intptr_t)0xFF));
}

static int get_filter_offset(const int16_t *f, const InterpKernel *base) {
  return (int)((const InterpKernel *)(intptr_t)f - base);
}

void aom_convolve8_horiz_c(const uint8_t *src, ptrdiff_t src_stride,
                           uint8_t *dst, ptrdiff_t dst_stride,
                           const int16_t *filter_x, int x_step_q4,
                           const int16_t *filter_y, int y_step_q4, int w,
                           int h) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);

  (void)filter_y;
  (void)y_step_q4;

  convolve_horiz(src, src_stride, dst, dst_stride, filters_x, x0_q4, x_step_q4,
                 w, h);
}

void aom_convolve8_vert_c(const uint8_t *src, ptrdiff_t src_stride,
                          uint8_t *dst, ptrdiff_t dst_stride,
                          const int16_t *filter_x, int x_step_q4,
                          const int16_t *filter_y, int y_step_q4, int w,
                          int h) {
  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);

  (void)filter_x;
  (void)x_step_q4;

  convolve_vert(src, src_stride, dst, dst_stride, filters_y, y0_q4, y_step_q4,
                w, h);
}

void aom_convolve8_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                     ptrdiff_t dst_stride, const InterpKernel *filter,
                     int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w,
                     int h) {
  // Note: Fixed size intermediate buffer, temp, places limits on parameters.
  // 2d filtering proceeds in 2 steps:
  //   (1) Interpolate horizontally into an intermediate buffer, temp.
  //   (2) Interpolate temp vertically to derive the sub-pixel result.
  // Deriving the maximum number of rows in the temp buffer (135):
  // --Smallest scaling factor is x1/2 ==> y_step_q4 = 32 (Normative).
  // --Largest block size is 64x64 pixels.
  // --64 rows in the downscaled frame span a distance of (64 - 1) * 32 in the
  //   original frame (in 1/16th pixel units).
  // --Must round-up because block may be located at sub-pixel position.
  // --Require an additional SUBPEL_TAPS rows for the 8-tap filter tails.
  // --((64 - 1) * 32 + 15) >> 4 + 8 = 135.
  // When calling in frame scaling function, the smallest scaling factor is x1/4
  // ==> y_step_q4 = 64. Since w and h are at most 16, the temp buffer is still
  // big enough.
  uint8_t temp[64 * 135];
  const int intermediate_height =
      (((h - 1) * y_step_q4 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;

  assert(w <= 64);
  assert(h <= 64);
  assert(y_step_q4 <= 32 || (y_step_q4 <= 64 && h <= 32));
  assert(x_step_q4 <= 64);

  convolve_horiz(src - src_stride * (SUBPEL_TAPS / 2 - 1), src_stride, temp, 64,
                 filter, x0_q4, x_step_q4, w, intermediate_height);
  convolve_vert(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst, dst_stride, filter,
                y0_q4, y_step_q4, w, h);
}

void aom_scaled_2d_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                     ptrdiff_t dst_stride, const InterpKernel *filter,
                     int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w,
                     int h) {
  aom_convolve8_c(src, src_stride, dst, dst_stride, filter, x0_q4, x_step_q4,
                  y0_q4, y_step_q4, w, h);
}

void aom_convolve_copy_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                         ptrdiff_t dst_stride, int w, int h) {
  for (int r = h; r > 0; --r) {
    memmove(dst, src, w);
    src += src_stride;
    dst += dst_stride;
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
static INLINE int highbd_vert_scalar_product(const uint16_t *a,
                                             ptrdiff_t a_stride,
                                             const int16_t *b) {
  int sum = 0;
  for (int k = 0; k < SUBPEL_TAPS; ++k) sum += a[k * a_stride] * b[k];
  return sum;
}

static INLINE int highbd_horz_scalar_product(const uint16_t *a,
                                             const int16_t *b) {
  int sum = 0;
  for (int k = 0; k < SUBPEL_TAPS; ++k) sum += a[k] * b[k];
  return sum;
}

static void highbd_convolve_horiz(const uint8_t *src8, ptrdiff_t src_stride,
                                  uint8_t *dst8, ptrdiff_t dst_stride,
                                  const InterpKernel *x_filters, int x0_q4,
                                  int x_step_q4, int w, int h, int bd) {
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  src -= SUBPEL_TAPS / 2 - 1;
  for (int y = 0; y < h; ++y) {
    int x_q4 = x0_q4;
    for (int x = 0; x < w; ++x) {
      const uint16_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
      const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
      const int sum = highbd_horz_scalar_product(src_x, x_filter);
      dst[x] = clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd);
      x_q4 += x_step_q4;
    }
    src += src_stride;
    dst += dst_stride;
  }
}

static void highbd_convolve_vert(const uint8_t *src8, ptrdiff_t src_stride,
                                 uint8_t *dst8, ptrdiff_t dst_stride,
                                 const InterpKernel *y_filters, int y0_q4,
                                 int y_step_q4, int w, int h, int bd) {
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  for (int x = 0; x < w; ++x) {
    int y_q4 = y0_q4;
    for (int y = 0; y < h; ++y) {
      const uint16_t *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
      const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
      const int sum = highbd_vert_scalar_product(src_y, src_stride, y_filter);
      dst[y * dst_stride] =
          clip_pixel_highbd(ROUND_POWER_OF_TWO(sum, FILTER_BITS), bd);
      y_q4 += y_step_q4;
    }
    ++src;
    ++dst;
  }
}

void aom_highbd_convolve8_horiz_c(const uint8_t *src, ptrdiff_t src_stride,
                                  uint8_t *dst, ptrdiff_t dst_stride,
                                  const int16_t *filter_x, int x_step_q4,
                                  const int16_t *filter_y, int y_step_q4, int w,
                                  int h, int bd) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);
  (void)filter_y;
  (void)y_step_q4;

  highbd_convolve_horiz(src, src_stride, dst, dst_stride, filters_x, x0_q4,
                        x_step_q4, w, h, bd);
}

void aom_highbd_convolve8_vert_c(const uint8_t *src, ptrdiff_t src_stride,
                                 uint8_t *dst, ptrdiff_t dst_stride,
                                 const int16_t *filter_x, int x_step_q4,
                                 const int16_t *filter_y, int y_step_q4, int w,
                                 int h, int bd) {
  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);
  (void)filter_x;
  (void)x_step_q4;

  highbd_convolve_vert(src, src_stride, dst, dst_stride, filters_y, y0_q4,
                       y_step_q4, w, h, bd);
}

void aom_highbd_convolve_copy_c(const uint16_t *src, ptrdiff_t src_stride,
                                uint16_t *dst, ptrdiff_t dst_stride, int w,
                                int h) {
  for (int y = 0; y < h; ++y) {
    memmove(dst, src, w * sizeof(src[0]));
    src += src_stride;
    dst += dst_stride;
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH
