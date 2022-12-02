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

#include <assert.h>

#include "config/aom_dsp_rtcd.h"

uint64_t aom_sum_squares_2d_i16_c(const int16_t *src, int src_stride, int width,
                                  int height) {
  int r, c;
  uint64_t ss = 0;

  for (r = 0; r < height; r++) {
    for (c = 0; c < width; c++) {
      const int16_t v = src[c];
      ss += v * v;
    }
    src += src_stride;
  }

  return ss;
}

uint64_t aom_sum_squares_i16_c(const int16_t *src, uint32_t n) {
  uint64_t ss = 0;
  do {
    const int16_t v = *src++;
    ss += v * v;
  } while (--n);

  return ss;
}

uint64_t aom_var_2d_u8_c(uint8_t *src, int src_stride, int width, int height) {
  int r, c;
  uint64_t ss = 0, s = 0;

  for (r = 0; r < height; r++) {
    for (c = 0; c < width; c++) {
      const uint8_t v = src[c];
      ss += v * v;
      s += v;
    }
    src += src_stride;
  }

  return (ss - s * s / (width * height));
}

uint64_t aom_var_2d_u16_c(uint8_t *src, int src_stride, int width, int height) {
  uint16_t *srcp = CONVERT_TO_SHORTPTR(src);
  int r, c;
  uint64_t ss = 0, s = 0;

  for (r = 0; r < height; r++) {
    for (c = 0; c < width; c++) {
      const uint16_t v = srcp[c];
      ss += v * v;
      s += v;
    }
    srcp += src_stride;
  }

  return (ss - s * s / (width * height));
}

uint64_t aom_sum_sse_2d_i16_c(const int16_t *src, int src_stride, int width,
                              int height, int *sum) {
  int r, c;
  int16_t *srcp = (int16_t *)src;
  int64_t ss = 0;

  for (r = 0; r < height; r++) {
    for (c = 0; c < width; c++) {
      const int16_t v = srcp[c];
      ss += v * v;
      *sum += v;
    }
    srcp += src_stride;
  }
  return ss;
}
