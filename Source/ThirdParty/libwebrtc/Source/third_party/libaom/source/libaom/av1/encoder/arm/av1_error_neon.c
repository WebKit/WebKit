/*
 *  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
 *  Copyright (c) 2019, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>
#include <assert.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"

int64_t av1_block_error_neon(const tran_low_t *coeff, const tran_low_t *dqcoeff,
                             intptr_t block_size, int64_t *ssz) {
  uint64x2_t err_u64 = vdupq_n_u64(0);
  int64x2_t ssz_s64 = vdupq_n_s64(0);

  assert(block_size >= 16);
  assert((block_size % 16) == 0);

  do {
    const int16x8_t c0 = load_tran_low_to_s16q(coeff);
    const int16x8_t c1 = load_tran_low_to_s16q(coeff + 8);
    const int16x8_t d0 = load_tran_low_to_s16q(dqcoeff);
    const int16x8_t d1 = load_tran_low_to_s16q(dqcoeff + 8);

    const uint16x8_t diff0 = vreinterpretq_u16_s16(vabdq_s16(c0, d0));
    const uint16x8_t diff1 = vreinterpretq_u16_s16(vabdq_s16(c1, d1));

    // By operating on unsigned integers we can store up to 4 squared diff in a
    // 32-bit element before having to widen to 64 bits.
    uint32x4_t err = vmull_u16(vget_low_u16(diff0), vget_low_u16(diff0));
    err = vmlal_u16(err, vget_high_u16(diff0), vget_high_u16(diff0));
    err = vmlal_u16(err, vget_low_u16(diff1), vget_low_u16(diff1));
    err = vmlal_u16(err, vget_high_u16(diff1), vget_high_u16(diff1));
    err_u64 = vpadalq_u32(err_u64, err);

    // We can't do the same here as we're operating on signed integers, so we
    // can only accumulate 2 squares.
    int32x4_t ssz0 = vmull_s16(vget_low_s16(c0), vget_low_s16(c0));
    ssz0 = vmlal_s16(ssz0, vget_high_s16(c0), vget_high_s16(c0));
    ssz_s64 = vpadalq_s32(ssz_s64, ssz0);

    int32x4_t ssz1 = vmull_s16(vget_low_s16(c1), vget_low_s16(c1));
    ssz1 = vmlal_s16(ssz1, vget_high_s16(c1), vget_high_s16(c1));
    ssz_s64 = vpadalq_s32(ssz_s64, ssz1);

    coeff += 16;
    dqcoeff += 16;
    block_size -= 16;
  } while (block_size != 0);

  *ssz = horizontal_add_s64x2(ssz_s64);
  return (int64_t)horizontal_add_u64x2(err_u64);
}

int64_t av1_block_error_lp_neon(const int16_t *coeff, const int16_t *dqcoeff,
                                intptr_t block_size) {
  uint64x2_t err_u64 = vdupq_n_u64(0);

  assert(block_size >= 16);
  assert((block_size % 16) == 0);

  do {
    const int16x8_t c0 = vld1q_s16(coeff);
    const int16x8_t c1 = vld1q_s16(coeff + 8);
    const int16x8_t d0 = vld1q_s16(dqcoeff);
    const int16x8_t d1 = vld1q_s16(dqcoeff + 8);

    const uint16x8_t diff0 = vreinterpretq_u16_s16(vabdq_s16(c0, d0));
    const uint16x8_t diff1 = vreinterpretq_u16_s16(vabdq_s16(c1, d1));

    // By operating on unsigned integers we can store up to 4 squared diff in a
    // 32-bit element before having to widen to 64 bits.
    uint32x4_t err = vmull_u16(vget_low_u16(diff0), vget_low_u16(diff0));
    err = vmlal_u16(err, vget_high_u16(diff0), vget_high_u16(diff0));
    err = vmlal_u16(err, vget_low_u16(diff1), vget_low_u16(diff1));
    err = vmlal_u16(err, vget_high_u16(diff1), vget_high_u16(diff1));
    err_u64 = vpadalq_u32(err_u64, err);

    coeff += 16;
    dqcoeff += 16;
    block_size -= 16;
  } while (block_size != 0);

  return (int64_t)horizontal_add_u64x2(err_u64);
}
