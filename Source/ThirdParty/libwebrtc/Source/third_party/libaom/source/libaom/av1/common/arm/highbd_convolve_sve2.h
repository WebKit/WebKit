/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_COMMON_ARM_HIGHBD_CONVOLVE_SVE2_H_
#define AOM_AV1_COMMON_ARM_HIGHBD_CONVOLVE_SVE2_H_

#include <arm_neon.h>

#include "aom_dsp/arm/aom_neon_sve2_bridge.h"

// clang-format off
DECLARE_ALIGNED(16, static const uint16_t, kDotProdMergeBlockTbl[24]) = {
  // Shift left and insert new last column in transposed 4x4 block.
  1, 2, 3, 0, 5, 6, 7, 4,
  // Shift left and insert two new columns in transposed 4x4 block.
  2, 3, 0, 1, 6, 7, 4, 5,
  // Shift left and insert three new columns in transposed 4x4 block.
  3, 0, 1, 2, 7, 4, 5, 6,
};
// clang-format on

static inline void aom_tbl2x4_s16(int16x8_t t0[4], int16x8_t t1[4],
                                  uint16x8_t tbl, int16x8_t res[4]) {
  res[0] = aom_tbl2_s16(t0[0], t1[0], tbl);
  res[1] = aom_tbl2_s16(t0[1], t1[1], tbl);
  res[2] = aom_tbl2_s16(t0[2], t1[2], tbl);
  res[3] = aom_tbl2_s16(t0[3], t1[3], tbl);
}

static inline void aom_tbl2x2_s16(int16x8_t t0[2], int16x8_t t1[2],
                                  uint16x8_t tbl, int16x8_t res[2]) {
  res[0] = aom_tbl2_s16(t0[0], t1[0], tbl);
  res[1] = aom_tbl2_s16(t0[1], t1[1], tbl);
}

#endif  // AOM_AV1_COMMON_ARM_HIGHBD_CONVOLVE_SVE2_H_
