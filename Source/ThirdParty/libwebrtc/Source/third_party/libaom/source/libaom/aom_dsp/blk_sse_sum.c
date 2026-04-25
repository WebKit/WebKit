/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "config/aom_dsp_rtcd.h"

void aom_get_blk_sse_sum_c(const int16_t *data, int stride, int bw, int bh,
                           int *x_sum, int64_t *x2_sum) {
  *x_sum = 0;
  *x2_sum = 0;
  for (int i = 0; i < bh; ++i) {
    for (int j = 0; j < bw; ++j) {
      const int val = data[j];
      *x_sum += val;
      *x2_sum += val * val;
    }
    data += stride;
  }
}
