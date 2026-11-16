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

#define HWY_BASELINE_TARGETS HWY_AVX3_DL
#define HWY_BROKEN_32BIT 0

#include "third_party/highway/hwy/detect_targets.h"

#include "av1/common/convolve_2d_sr_hwy.h"

MAKE_CONVOLVE_2D_SR(avx512)
