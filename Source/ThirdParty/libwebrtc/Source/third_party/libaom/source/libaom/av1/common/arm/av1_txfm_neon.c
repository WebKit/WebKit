/*
 *
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include <arm_neon.h>
#include <assert.h>

#include "config/av1_rtcd.h"

#include "aom_dsp/arm/mem_neon.h"
#include "aom_ports/mem.h"

void av1_round_shift_array_neon(int32_t *arr, int size, int bit) {
  assert(!(size % 4));
  if (!bit) return;
  const int32x4_t dup_bits_n_32x4 = vdupq_n_s32((int32_t)(-bit));
  for (int i = 0; i < size; i += 4) {
    int32x4_t tmp_q_s32 = vld1q_s32(arr);
    tmp_q_s32 = vrshlq_s32(tmp_q_s32, dup_bits_n_32x4);
    vst1q_s32(arr, tmp_q_s32);
    arr += 4;
  }
}
