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

#define HWY_BASELINE_TARGETS HWY_AVX2
#define HWY_BROKEN_32BIT 0

#include "aom_dsp/sad_hwy.h"

// FSAD_4D, FSAD_3D and FSAD_4D_SKIP have similar performance, but migrating
// to highway adds test coverage and simplifies avx2 when it's enabled by
// default.
FOR_EACH_SAD_BLOCK_SIZE(FSAD, avx2)
FOR_EACH_SAD_BLOCK_SIZE(FSAD_SKIP, avx2)
FOR_EACH_SAD_BLOCK_SIZE(FSAD_AVG, avx2)
FOR_EACH_SAD_BLOCK_SIZE(FSAD_4D, avx2)
FOR_EACH_SAD_BLOCK_SIZE(FSAD_4D_SKIP, avx2)
FOR_EACH_SAD_BLOCK_SIZE(FSAD_3D, avx2)
