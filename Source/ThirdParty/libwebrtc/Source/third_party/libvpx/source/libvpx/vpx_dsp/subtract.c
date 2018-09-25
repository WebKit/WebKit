/*
 *  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"

#include "vpx/vpx_integer.h"
#include "vpx_ports/mem.h"

void vpx_subtract_block_c(int rows, int cols, int16_t *diff,
                          ptrdiff_t diff_stride, const uint8_t *src,
                          ptrdiff_t src_stride, const uint8_t *pred,
                          ptrdiff_t pred_stride) {
  int r, c;

  for (r = 0; r < rows; r++) {
    for (c = 0; c < cols; c++) diff[c] = src[c] - pred[c];

    diff += diff_stride;
    pred += pred_stride;
    src += src_stride;
  }
}

#if CONFIG_VP9_HIGHBITDEPTH
void vpx_highbd_subtract_block_c(int rows, int cols, int16_t *diff,
                                 ptrdiff_t diff_stride, const uint8_t *src8,
                                 ptrdiff_t src_stride, const uint8_t *pred8,
                                 ptrdiff_t pred_stride, int bd) {
  int r, c;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  (void)bd;

  for (r = 0; r < rows; r++) {
    for (c = 0; c < cols; c++) {
      diff[c] = src[c] - pred[c];
    }

    diff += diff_stride;
    pred += pred_stride;
    src += src_stride;
  }
}
#endif  // CONFIG_VP9_HIGHBITDEPTH
