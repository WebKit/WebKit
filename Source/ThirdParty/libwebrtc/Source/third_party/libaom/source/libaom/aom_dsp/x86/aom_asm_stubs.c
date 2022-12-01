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

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/x86/convolve.h"

#if HAVE_SSE2
filter8_1dfunction aom_filter_block1d16_v8_sse2;
filter8_1dfunction aom_filter_block1d16_h8_sse2;
filter8_1dfunction aom_filter_block1d8_v8_sse2;
filter8_1dfunction aom_filter_block1d8_h8_sse2;
filter8_1dfunction aom_filter_block1d4_v8_sse2;
filter8_1dfunction aom_filter_block1d4_h8_sse2;
filter8_1dfunction aom_filter_block1d16_v4_sse2;
filter8_1dfunction aom_filter_block1d16_h4_sse2;

filter8_1dfunction aom_filter_block1d8_h4_sse2;
filter8_1dfunction aom_filter_block1d8_v4_sse2;
filter8_1dfunction aom_filter_block1d4_h4_sse2;
filter8_1dfunction aom_filter_block1d4_v4_sse2;

filter8_1dfunction aom_filter_block1d16_v2_sse2;
filter8_1dfunction aom_filter_block1d16_h2_sse2;
filter8_1dfunction aom_filter_block1d8_v2_sse2;
filter8_1dfunction aom_filter_block1d8_h2_sse2;
filter8_1dfunction aom_filter_block1d4_v2_sse2;
filter8_1dfunction aom_filter_block1d4_h2_sse2;

// void aom_convolve8_horiz_sse2(const uint8_t *src, ptrdiff_t src_stride,
//                               uint8_t *dst, ptrdiff_t dst_stride,
//                               const int16_t *filter_x, int x_step_q4,
//                               const int16_t *filter_y, int y_step_q4,
//                               int w, int h);
// void aom_convolve8_vert_sse2(const uint8_t *src, ptrdiff_t src_stride,
//                              uint8_t *dst, ptrdiff_t dst_stride,
//                              const int16_t *filter_x, int x_step_q4,
//                              const int16_t *filter_y, int y_step_q4,
//                              int w, int h);
FUN_CONV_1D(horiz, x_step_q4, filter_x, h, src, , sse2)
FUN_CONV_1D(vert, y_step_q4, filter_y, v, src - src_stride * 3, , sse2)

#if CONFIG_AV1_HIGHBITDEPTH
highbd_filter8_1dfunction aom_highbd_filter_block1d16_v8_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d16_h8_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d8_v8_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d8_h8_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d4_v8_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d4_h8_sse2;

highbd_filter8_1dfunction aom_highbd_filter_block1d16_v4_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d16_h4_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d8_v4_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d8_h4_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d4_v4_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d4_h4_sse2;

highbd_filter8_1dfunction aom_highbd_filter_block1d16_v2_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d16_h2_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d8_v2_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d8_h2_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d4_v2_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d4_h2_sse2;

// void aom_highbd_convolve8_horiz_sse2(const uint8_t *src,
//                                      ptrdiff_t src_stride,
//                                      uint8_t *dst,
//                                      ptrdiff_t dst_stride,
//                                      const int16_t *filter_x,
//                                      int x_step_q4,
//                                      const int16_t *filter_y,
//                                      int y_step_q4,
//                                      int w, int h, int bd);
// void aom_highbd_convolve8_vert_sse2(const uint8_t *src,
//                                     ptrdiff_t src_stride,
//                                     uint8_t *dst,
//                                     ptrdiff_t dst_stride,
//                                     const int16_t *filter_x,
//                                     int x_step_q4,
//                                     const int16_t *filter_y,
//                                     int y_step_q4,
//                                     int w, int h, int bd);
HIGH_FUN_CONV_1D(horiz, x_step_q4, filter_x, h, src, , sse2)
HIGH_FUN_CONV_1D(vert, y_step_q4, filter_y, v, src - src_stride * 3, , sse2)
#endif
#endif  // HAVE_SSE2
