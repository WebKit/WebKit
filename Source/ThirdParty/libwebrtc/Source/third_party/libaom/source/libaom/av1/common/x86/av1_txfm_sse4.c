/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "config/av1_rtcd.h"

#include "av1/common/av1_txfm.h"
#include "av1/common/x86/av1_txfm_sse4.h"

// This function assumes `arr` is 16-byte aligned.
void av1_round_shift_array_sse4_1(int32_t *arr, int size, int bit) {
  __m128i *const vec = (__m128i *)arr;
  const int vec_size = size >> 2;
  av1_round_shift_array_32_sse4_1(vec, vec, vec_size, bit);
}
