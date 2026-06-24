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

#define HWY_BASELINE_TARGETS HWY_AVX2
#define HWY_BROKEN_32BIT 0

#include <cstdint>

#include "aom_dsp/convolve_hwy.h"

extern "C" void aom_convolve8_vert_avx2(const uint8_t *src,
                                        ptrdiff_t src_stride, uint8_t *dst,
                                        ptrdiff_t dst_stride,
                                        const int16_t *filter_x, int x_step_q4,
                                        const int16_t *filter_y, int y_step_q4,
                                        int w, int h);

HWY_ATTR void aom_convolve8_vert_avx2(const uint8_t *src, ptrdiff_t src_stride,
                                      uint8_t *dst, ptrdiff_t dst_stride,
                                      const int16_t *filter_x, int x_step_q4,
                                      const int16_t *filter_y, int y_step_q4,
                                      int w, int h) {
  HWY_NAMESPACE::Convolve8Vert(src, src_stride, dst, dst_stride, filter_x,
                               x_step_q4, filter_y, y_step_q4, w, h);
}
