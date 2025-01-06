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

#include <emmintrin.h>  // SSE2

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/x86/fwd_txfm_sse2.h"

#define DCT_HIGH_BIT_DEPTH 0
#define FDCT4x4_2D_HELPER fdct4x4_helper
#define FDCT4x4_2D aom_fdct4x4_sse2
#define FDCT4x4_2D_LP aom_fdct4x4_lp_sse2
#define FDCT8x8_2D aom_fdct8x8_sse2
#include "aom_dsp/x86/fwd_txfm_impl_sse2.h"
#undef FDCT4x4_2D_HELPER
#undef FDCT4x4_2D
#undef FDCT4x4_2D_LP
#undef FDCT8x8_2D

#if CONFIG_AV1_HIGHBITDEPTH

#undef DCT_HIGH_BIT_DEPTH
#define DCT_HIGH_BIT_DEPTH 1
#define FDCT8x8_2D aom_highbd_fdct8x8_sse2
#include "aom_dsp/x86/fwd_txfm_impl_sse2.h"  // NOLINT
#undef FDCT8x8_2D

#endif
