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

#define HWY_BASELINE_TARGETS HWY_AVX3_DL
#define HWY_BROKEN_32BIT 0

#include "av1/encoder/av1_fwd_txfm2d_hwy.h"

FOR_EACH_TXFM2D(MAKE_HIGHBD_TXFM2D, avx512)
MAKE_LOWBD_TXFM2D_DISPATCH(avx512)
