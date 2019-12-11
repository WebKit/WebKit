/*
 *  Copyright (c) 2017 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>
#include <assert.h>

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "vpx_dsp/arm/mem_neon.h"

void vpx_quantize_b_neon(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                         int skip_block, const int16_t *zbin_ptr,
                         const int16_t *round_ptr, const int16_t *quant_ptr,
                         const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
                         tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr,
                         uint16_t *eob_ptr, const int16_t *scan_ptr,
                         const int16_t *iscan_ptr) {
  const int16x8_t one = vdupq_n_s16(1);
  const int16x8_t neg_one = vdupq_n_s16(-1);
  uint16x8_t eob_max;
  (void)scan_ptr;
  (void)skip_block;
  assert(!skip_block);

  // Process first 8 values which include a dc component.
  {
    // Only the first element of each vector is DC.
    const int16x8_t zbin = vld1q_s16(zbin_ptr);
    const int16x8_t round = vld1q_s16(round_ptr);
    const int16x8_t quant = vld1q_s16(quant_ptr);
    const int16x8_t quant_shift = vld1q_s16(quant_shift_ptr);
    const int16x8_t dequant = vld1q_s16(dequant_ptr);
    // Add one because the eob does not index from 0.
    const uint16x8_t iscan =
        vreinterpretq_u16_s16(vaddq_s16(vld1q_s16(iscan_ptr), one));

    const int16x8_t coeff = load_tran_low_to_s16q(coeff_ptr);
    const int16x8_t coeff_sign = vshrq_n_s16(coeff, 15);
    const int16x8_t coeff_abs = vabsq_s16(coeff);

    const int16x8_t zbin_mask =
        vreinterpretq_s16_u16(vcgeq_s16(coeff_abs, zbin));

    const int16x8_t rounded = vqaddq_s16(coeff_abs, round);

    // (round * quant * 2) >> 16 >> 1 == (round * quant) >> 16
    int16x8_t qcoeff = vshrq_n_s16(vqdmulhq_s16(rounded, quant), 1);

    qcoeff = vaddq_s16(qcoeff, rounded);

    // (qcoeff * quant_shift * 2) >> 16 >> 1 == (qcoeff * quant_shift) >> 16
    qcoeff = vshrq_n_s16(vqdmulhq_s16(qcoeff, quant_shift), 1);

    // Restore the sign bit.
    qcoeff = veorq_s16(qcoeff, coeff_sign);
    qcoeff = vsubq_s16(qcoeff, coeff_sign);

    qcoeff = vandq_s16(qcoeff, zbin_mask);

    // Set non-zero elements to -1 and use that to extract values for eob.
    eob_max = vandq_u16(vtstq_s16(qcoeff, neg_one), iscan);

    coeff_ptr += 8;
    iscan_ptr += 8;

    store_s16q_to_tran_low(qcoeff_ptr, qcoeff);
    qcoeff_ptr += 8;

    qcoeff = vmulq_s16(qcoeff, dequant);

    store_s16q_to_tran_low(dqcoeff_ptr, qcoeff);
    dqcoeff_ptr += 8;
  }

  n_coeffs -= 8;

  {
    const int16x8_t zbin = vdupq_n_s16(zbin_ptr[1]);
    const int16x8_t round = vdupq_n_s16(round_ptr[1]);
    const int16x8_t quant = vdupq_n_s16(quant_ptr[1]);
    const int16x8_t quant_shift = vdupq_n_s16(quant_shift_ptr[1]);
    const int16x8_t dequant = vdupq_n_s16(dequant_ptr[1]);

    do {
      // Add one because the eob is not its index.
      const uint16x8_t iscan =
          vreinterpretq_u16_s16(vaddq_s16(vld1q_s16(iscan_ptr), one));

      const int16x8_t coeff = load_tran_low_to_s16q(coeff_ptr);
      const int16x8_t coeff_sign = vshrq_n_s16(coeff, 15);
      const int16x8_t coeff_abs = vabsq_s16(coeff);

      const int16x8_t zbin_mask =
          vreinterpretq_s16_u16(vcgeq_s16(coeff_abs, zbin));

      const int16x8_t rounded = vqaddq_s16(coeff_abs, round);

      // (round * quant * 2) >> 16 >> 1 == (round * quant) >> 16
      int16x8_t qcoeff = vshrq_n_s16(vqdmulhq_s16(rounded, quant), 1);

      qcoeff = vaddq_s16(qcoeff, rounded);

      // (qcoeff * quant_shift * 2) >> 16 >> 1 == (qcoeff * quant_shift) >> 16
      qcoeff = vshrq_n_s16(vqdmulhq_s16(qcoeff, quant_shift), 1);

      // Restore the sign bit.
      qcoeff = veorq_s16(qcoeff, coeff_sign);
      qcoeff = vsubq_s16(qcoeff, coeff_sign);

      qcoeff = vandq_s16(qcoeff, zbin_mask);

      // Set non-zero elements to -1 and use that to extract values for eob.
      eob_max =
          vmaxq_u16(eob_max, vandq_u16(vtstq_s16(qcoeff, neg_one), iscan));

      coeff_ptr += 8;
      iscan_ptr += 8;

      store_s16q_to_tran_low(qcoeff_ptr, qcoeff);
      qcoeff_ptr += 8;

      qcoeff = vmulq_s16(qcoeff, dequant);

      store_s16q_to_tran_low(dqcoeff_ptr, qcoeff);
      dqcoeff_ptr += 8;

      n_coeffs -= 8;
    } while (n_coeffs > 0);
  }

  {
    const uint16x4_t eob_max_0 =
        vmax_u16(vget_low_u16(eob_max), vget_high_u16(eob_max));
    const uint16x4_t eob_max_1 = vpmax_u16(eob_max_0, eob_max_0);
    const uint16x4_t eob_max_2 = vpmax_u16(eob_max_1, eob_max_1);
    vst1_lane_u16(eob_ptr, eob_max_2, 0);
  }
}

static INLINE int32x4_t extract_sign_bit(int32x4_t a) {
  return vreinterpretq_s32_u32(vshrq_n_u32(vreinterpretq_u32_s32(a), 31));
}

// Main difference is that zbin values are halved before comparison and dqcoeff
// values are divided by 2. zbin is rounded but dqcoeff is not.
void vpx_quantize_b_32x32_neon(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block,
    const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan_ptr, const int16_t *iscan_ptr) {
  const int16x8_t one = vdupq_n_s16(1);
  const int16x8_t neg_one = vdupq_n_s16(-1);
  uint16x8_t eob_max;
  int i;
  (void)scan_ptr;
  (void)n_coeffs;  // Because we will always calculate 32*32.
  (void)skip_block;
  assert(!skip_block);

  // Process first 8 values which include a dc component.
  {
    // Only the first element of each vector is DC.
    const int16x8_t zbin = vrshrq_n_s16(vld1q_s16(zbin_ptr), 1);
    const int16x8_t round = vrshrq_n_s16(vld1q_s16(round_ptr), 1);
    const int16x8_t quant = vld1q_s16(quant_ptr);
    const int16x8_t quant_shift = vld1q_s16(quant_shift_ptr);
    const int16x8_t dequant = vld1q_s16(dequant_ptr);
    // Add one because the eob does not index from 0.
    const uint16x8_t iscan =
        vreinterpretq_u16_s16(vaddq_s16(vld1q_s16(iscan_ptr), one));

    const int16x8_t coeff = load_tran_low_to_s16q(coeff_ptr);
    const int16x8_t coeff_sign = vshrq_n_s16(coeff, 15);
    const int16x8_t coeff_abs = vabsq_s16(coeff);

    const int16x8_t zbin_mask =
        vreinterpretq_s16_u16(vcgeq_s16(coeff_abs, zbin));

    const int16x8_t rounded = vqaddq_s16(coeff_abs, round);

    // (round * quant * 2) >> 16 >> 1 == (round * quant) >> 16
    int16x8_t qcoeff = vshrq_n_s16(vqdmulhq_s16(rounded, quant), 1);
    int16x8_t dqcoeff;
    int32x4_t dqcoeff_0, dqcoeff_1;

    qcoeff = vaddq_s16(qcoeff, rounded);

    // (qcoeff * quant_shift * 2) >> 16 == (qcoeff * quant_shift) >> 15
    qcoeff = vqdmulhq_s16(qcoeff, quant_shift);

    // Restore the sign bit.
    qcoeff = veorq_s16(qcoeff, coeff_sign);
    qcoeff = vsubq_s16(qcoeff, coeff_sign);

    qcoeff = vandq_s16(qcoeff, zbin_mask);

    // Set non-zero elements to -1 and use that to extract values for eob.
    eob_max = vandq_u16(vtstq_s16(qcoeff, neg_one), iscan);

    coeff_ptr += 8;
    iscan_ptr += 8;

    store_s16q_to_tran_low(qcoeff_ptr, qcoeff);
    qcoeff_ptr += 8;

    dqcoeff_0 = vmull_s16(vget_low_s16(qcoeff), vget_low_s16(dequant));
    dqcoeff_1 = vmull_s16(vget_high_s16(qcoeff), vget_high_s16(dequant));

    // Add 1 if negative to round towards zero because the C uses division.
    dqcoeff_0 = vaddq_s32(dqcoeff_0, extract_sign_bit(dqcoeff_0));
    dqcoeff_1 = vaddq_s32(dqcoeff_1, extract_sign_bit(dqcoeff_1));

    dqcoeff =
        vcombine_s16(vshrn_n_s32(dqcoeff_0, 1), vshrn_n_s32(dqcoeff_1, 1));

    store_s16q_to_tran_low(dqcoeff_ptr, dqcoeff);
    dqcoeff_ptr += 8;
  }

  {
    const int16x8_t zbin = vrshrq_n_s16(vdupq_n_s16(zbin_ptr[1]), 1);
    const int16x8_t round = vrshrq_n_s16(vdupq_n_s16(round_ptr[1]), 1);
    const int16x8_t quant = vdupq_n_s16(quant_ptr[1]);
    const int16x8_t quant_shift = vdupq_n_s16(quant_shift_ptr[1]);
    const int16x8_t dequant = vdupq_n_s16(dequant_ptr[1]);

    for (i = 1; i < 32 * 32 / 8; ++i) {
      // Add one because the eob is not its index.
      const uint16x8_t iscan =
          vreinterpretq_u16_s16(vaddq_s16(vld1q_s16(iscan_ptr), one));

      const int16x8_t coeff = load_tran_low_to_s16q(coeff_ptr);
      const int16x8_t coeff_sign = vshrq_n_s16(coeff, 15);
      const int16x8_t coeff_abs = vabsq_s16(coeff);

      const int16x8_t zbin_mask =
          vreinterpretq_s16_u16(vcgeq_s16(coeff_abs, zbin));

      const int16x8_t rounded = vqaddq_s16(coeff_abs, round);

      // (round * quant * 2) >> 16 >> 1 == (round * quant) >> 16
      int16x8_t qcoeff = vshrq_n_s16(vqdmulhq_s16(rounded, quant), 1);
      int16x8_t dqcoeff;
      int32x4_t dqcoeff_0, dqcoeff_1;

      qcoeff = vaddq_s16(qcoeff, rounded);

      // (qcoeff * quant_shift * 2) >> 16 == (qcoeff * quant_shift) >> 15
      qcoeff = vqdmulhq_s16(qcoeff, quant_shift);

      // Restore the sign bit.
      qcoeff = veorq_s16(qcoeff, coeff_sign);
      qcoeff = vsubq_s16(qcoeff, coeff_sign);

      qcoeff = vandq_s16(qcoeff, zbin_mask);

      // Set non-zero elements to -1 and use that to extract values for eob.
      eob_max =
          vmaxq_u16(eob_max, vandq_u16(vtstq_s16(qcoeff, neg_one), iscan));

      coeff_ptr += 8;
      iscan_ptr += 8;

      store_s16q_to_tran_low(qcoeff_ptr, qcoeff);
      qcoeff_ptr += 8;

      dqcoeff_0 = vmull_s16(vget_low_s16(qcoeff), vget_low_s16(dequant));
      dqcoeff_1 = vmull_s16(vget_high_s16(qcoeff), vget_high_s16(dequant));

      dqcoeff_0 = vaddq_s32(dqcoeff_0, extract_sign_bit(dqcoeff_0));
      dqcoeff_1 = vaddq_s32(dqcoeff_1, extract_sign_bit(dqcoeff_1));

      dqcoeff =
          vcombine_s16(vshrn_n_s32(dqcoeff_0, 1), vshrn_n_s32(dqcoeff_1, 1));

      store_s16q_to_tran_low(dqcoeff_ptr, dqcoeff);
      dqcoeff_ptr += 8;
    }
  }

  {
    const uint16x4_t eob_max_0 =
        vmax_u16(vget_low_u16(eob_max), vget_high_u16(eob_max));
    const uint16x4_t eob_max_1 = vpmax_u16(eob_max_0, eob_max_0);
    const uint16x4_t eob_max_2 = vpmax_u16(eob_max_1, eob_max_1);
    vst1_lane_u16(eob_ptr, eob_max_2, 0);
  }
}
