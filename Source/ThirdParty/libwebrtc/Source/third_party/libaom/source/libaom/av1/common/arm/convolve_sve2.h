/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_COMMON_ARM_CONVOLVE_SVE2_H_
#define AOM_AV1_COMMON_ARM_CONVOLVE_SVE2_H_

#include "aom_dsp/arm/aom_neon_sve2_bridge.h"
#include "aom_ports/mem.h"

DECLARE_ALIGNED(16, extern const uint16_t, kSVEDotProdMergeBlockTbl[24]);

static inline void aom_tbl2x2_s16(int16x8_t t0[2], int16x8_t t1[2],
                                  uint16x8_t tbl, int16x8_t res[2]) {
  res[0] = aom_tbl2_s16(t0[0], t1[0], tbl);
  res[1] = aom_tbl2_s16(t0[1], t1[1], tbl);
}

#endif  // AOM_AV1_COMMON_ARM_CONVOLVE_SVE2_H_
