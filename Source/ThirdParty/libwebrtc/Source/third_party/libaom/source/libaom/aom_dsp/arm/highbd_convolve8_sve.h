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

#ifndef AOM_AOM_DSP_ARM_HIGHBD_CONVOLVE8_SVE_H_
#define AOM_AOM_DSP_ARM_HIGHBD_CONVOLVE8_SVE_H_

#include "aom_ports/mem.h"

DECLARE_ALIGNED(16, extern const uint16_t, kHbdDotProdTbl[32]);
DECLARE_ALIGNED(16, extern const uint16_t, kDeinterleaveTbl[8]);

#endif  // AOM_AOM_DSP_ARM_HIGHBD_CONVOLVE8_SVE_H_
