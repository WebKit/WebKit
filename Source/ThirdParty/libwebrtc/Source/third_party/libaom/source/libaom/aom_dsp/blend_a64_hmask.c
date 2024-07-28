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

#include "aom/aom_integer.h"
#include "aom_ports/mem.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/blend.h"

#include "config/aom_dsp_rtcd.h"

void aom_blend_a64_hmask_c(uint8_t *dst, uint32_t dst_stride,
                           const uint8_t *src0, uint32_t src0_stride,
                           const uint8_t *src1, uint32_t src1_stride,
                           const uint8_t *mask, int w, int h) {
  int i, j;

  assert(IMPLIES(src0 == dst, src0_stride == dst_stride));
  assert(IMPLIES(src1 == dst, src1_stride == dst_stride));

  assert(h >= 1);
  assert(w >= 1);
  assert(IS_POWER_OF_TWO(h));
  assert(IS_POWER_OF_TWO(w));

  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; ++j) {
      dst[i * dst_stride + j] = AOM_BLEND_A64(
          mask[j], src0[i * src0_stride + j], src1[i * src1_stride + j]);
    }
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
void aom_highbd_blend_a64_hmask_c(uint8_t *dst_8, uint32_t dst_stride,
                                  const uint8_t *src0_8, uint32_t src0_stride,
                                  const uint8_t *src1_8, uint32_t src1_stride,
                                  const uint8_t *mask, int w, int h, int bd) {
  int i, j;
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst_8);
  const uint16_t *src0 = CONVERT_TO_SHORTPTR(src0_8);
  const uint16_t *src1 = CONVERT_TO_SHORTPTR(src1_8);
  (void)bd;

  assert(IMPLIES(src0 == dst, src0_stride == dst_stride));
  assert(IMPLIES(src1 == dst, src1_stride == dst_stride));

  assert(h >= 1);
  assert(w >= 1);
  assert(IS_POWER_OF_TWO(h));
  assert(IS_POWER_OF_TWO(w));

  assert(bd == 8 || bd == 10 || bd == 12);

  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; ++j) {
      dst[i * dst_stride + j] = AOM_BLEND_A64(
          mask[j], src0[i * src0_stride + j], src1[i * src1_stride + j]);
    }
  }
}
#endif
