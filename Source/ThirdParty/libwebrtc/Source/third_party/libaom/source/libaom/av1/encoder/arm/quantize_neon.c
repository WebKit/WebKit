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

#include <arm_neon.h>

#include <assert.h>
#include <math.h>

#include "config/aom_config.h"

#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_mem/aom_mem.h"

#include "av1/common/quant_common.h"
#include "av1/common/seg_common.h"

#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/rd.h"

static INLINE uint16_t get_max_eob(int16x8_t v_eobmax) {
#if AOM_ARCH_AARCH64
  return (uint16_t)vmaxvq_s16(v_eobmax);
#else
  const int16x4_t v_eobmax_3210 =
      vmax_s16(vget_low_s16(v_eobmax), vget_high_s16(v_eobmax));
  const int64x1_t v_eobmax_xx32 =
      vshr_n_s64(vreinterpret_s64_s16(v_eobmax_3210), 32);
  const int16x4_t v_eobmax_tmp =
      vmax_s16(v_eobmax_3210, vreinterpret_s16_s64(v_eobmax_xx32));
  const int64x1_t v_eobmax_xxx3 =
      vshr_n_s64(vreinterpret_s64_s16(v_eobmax_tmp), 16);
  const int16x4_t v_eobmax_final =
      vmax_s16(v_eobmax_tmp, vreinterpret_s16_s64(v_eobmax_xxx3));
  return (uint16_t)vget_lane_s16(v_eobmax_final, 0);
#endif
}

static INLINE int16x8_t get_max_lane_eob(const int16_t *iscan,
                                         int16x8_t v_eobmax,
                                         uint16x8_t v_mask) {
  const int16x8_t v_iscan = vld1q_s16(&iscan[0]);
  const int16x8_t v_iscan_plus1 = vaddq_s16(v_iscan, vdupq_n_s16(1));
  const int16x8_t v_nz_iscan = vbslq_s16(v_mask, v_iscan_plus1, vdupq_n_s16(0));
  return vmaxq_s16(v_eobmax, v_nz_iscan);
}

static INLINE uint16x8_t quantize_fp_8(const tran_low_t *coeff_ptr,
                                       tran_low_t *qcoeff_ptr,
                                       tran_low_t *dqcoeff_ptr,
                                       int16x8_t v_quant, int16x8_t v_dequant,
                                       int16x8_t v_round, int16x8_t v_zero) {
  const int16x8_t v_coeff = load_tran_low_to_s16q(&coeff_ptr[0]);
  const int16x8_t v_coeff_sign = vshrq_n_s16(v_coeff, 15);
  const int16x8_t v_abs = vabsq_s16(v_coeff);
  const int16x8_t v_tmp = vqaddq_s16(v_abs, v_round);
  const int16x8_t v_tmp2 = vshrq_n_s16(vqdmulhq_s16(v_tmp, v_quant), 1);
  const uint16x8_t v_nz_mask = vcgtq_s16(v_tmp2, v_zero);
  const int16x8_t v_qcoeff_a = veorq_s16(v_tmp2, v_coeff_sign);
  const int16x8_t v_qcoeff = vsubq_s16(v_qcoeff_a, v_coeff_sign);
  const int16x8_t v_dqcoeff = vmulq_s16(v_qcoeff, v_dequant);
  store_s16q_to_tran_low(&qcoeff_ptr[0], v_qcoeff);
  store_s16q_to_tran_low(&dqcoeff_ptr[0], v_dqcoeff);
  return v_nz_mask;
}

void av1_quantize_fp_neon(const tran_low_t *coeff_ptr, intptr_t count,
                          const int16_t *zbin_ptr, const int16_t *round_ptr,
                          const int16_t *quant_ptr,
                          const int16_t *quant_shift_ptr,
                          tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                          const int16_t *dequant_ptr, uint16_t *eob_ptr,
                          const int16_t *scan, const int16_t *iscan) {
  // TODO(jingning) Decide the need of these arguments after the
  // quantization process is completed.
  (void)zbin_ptr;
  (void)quant_shift_ptr;
  (void)scan;

  // Quantization pass: All coefficients with index >= zero_flag are
  // skippable. Note: zero_flag can be zero.
  const int16x8_t v_zero = vdupq_n_s16(0);
  int16x8_t v_quant = vld1q_s16(quant_ptr);
  int16x8_t v_dequant = vld1q_s16(dequant_ptr);
  int16x8_t v_round = vld1q_s16(round_ptr);
  int16x8_t v_eobmax_76543210 = vdupq_n_s16(-1);
  uint16x8_t v_nz_mask;
  // process dc and the first seven ac coeffs
  v_nz_mask = quantize_fp_8(coeff_ptr, qcoeff_ptr, dqcoeff_ptr, v_quant,
                            v_dequant, v_round, v_zero);
  v_eobmax_76543210 = get_max_lane_eob(&iscan[0], v_eobmax_76543210, v_nz_mask);
  // overwrite the dc constants with ac constants
  v_quant = vdupq_lane_s16(vget_low_s16(v_quant), 1);
  v_dequant = vdupq_lane_s16(vget_low_s16(v_dequant), 1);
  v_round = vdupq_lane_s16(vget_low_s16(v_round), 1);

  count -= 8;
  // now process the rest of the ac coeffs
  do {
    coeff_ptr += 8;
    qcoeff_ptr += 8;
    dqcoeff_ptr += 8;
    iscan += 8;
    v_nz_mask = quantize_fp_8(coeff_ptr, qcoeff_ptr, dqcoeff_ptr, v_quant,
                              v_dequant, v_round, v_zero);
    v_eobmax_76543210 = get_max_lane_eob(iscan, v_eobmax_76543210, v_nz_mask);
    count -= 8;
  } while (count > 0);
  *eob_ptr = get_max_eob(v_eobmax_76543210);
}

static INLINE uint16x8_t quantize_lp_8(const int16_t *coeff_ptr,
                                       int16_t *qcoeff_ptr,
                                       int16_t *dqcoeff_ptr, int16x8_t v_quant,
                                       int16x8_t v_dequant, int16x8_t v_round,
                                       int16x8_t v_zero) {
  const int16x8_t v_coeff = vld1q_s16(&coeff_ptr[0]);
  const int16x8_t v_coeff_sign = vshrq_n_s16(v_coeff, 15);
  const int16x8_t v_abs = vabsq_s16(v_coeff);
  const int16x8_t v_tmp = vqaddq_s16(v_abs, v_round);
  const int16x8_t v_tmp2 = vshrq_n_s16(vqdmulhq_s16(v_tmp, v_quant), 1);
  const uint16x8_t v_nz_mask = vcgtq_s16(v_tmp2, v_zero);
  const int16x8_t v_qcoeff_a = veorq_s16(v_tmp2, v_coeff_sign);
  const int16x8_t v_qcoeff = vsubq_s16(v_qcoeff_a, v_coeff_sign);
  const int16x8_t v_dqcoeff = vmulq_s16(v_qcoeff, v_dequant);
  vst1q_s16(qcoeff_ptr, v_qcoeff);
  vst1q_s16(dqcoeff_ptr, v_dqcoeff);
  return v_nz_mask;
}

void av1_quantize_lp_neon(const int16_t *coeff_ptr, intptr_t n_coeffs,
                          const int16_t *round_ptr, const int16_t *quant_ptr,
                          int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr,
                          const int16_t *dequant_ptr, uint16_t *eob_ptr,
                          const int16_t *scan, const int16_t *iscan) {
  (void)scan;
  // Quantization pass: All coefficients with index >= zero_flag are
  // skippable. Note: zero_flag can be zero.
  const int16x8_t v_zero = vdupq_n_s16(0);
  int16x8_t v_quant = vld1q_s16(quant_ptr);
  int16x8_t v_dequant = vld1q_s16(dequant_ptr);
  int16x8_t v_round = vld1q_s16(round_ptr);
  int16x8_t v_eobmax_76543210 = vdupq_n_s16(-1);
  uint16x8_t v_nz_mask;
  intptr_t count = n_coeffs;

  // process dc and the first seven ac coeffs
  v_nz_mask = quantize_lp_8(coeff_ptr, qcoeff_ptr, dqcoeff_ptr, v_quant,
                            v_dequant, v_round, v_zero);
  v_eobmax_76543210 = get_max_lane_eob(iscan, v_eobmax_76543210, v_nz_mask);
  // overwrite the dc constants with ac constants
  v_quant = vdupq_lane_s16(vget_low_s16(v_quant), 1);
  v_dequant = vdupq_lane_s16(vget_low_s16(v_dequant), 1);
  v_round = vdupq_lane_s16(vget_low_s16(v_round), 1);

  count -= 8;
  // now process the rest of the ac coeffs
  do {
    coeff_ptr += 8;
    qcoeff_ptr += 8;
    dqcoeff_ptr += 8;
    iscan += 8;
    v_nz_mask = quantize_lp_8(coeff_ptr, qcoeff_ptr, dqcoeff_ptr, v_quant,
                              v_dequant, v_round, v_zero);
    v_eobmax_76543210 = get_max_lane_eob(iscan, v_eobmax_76543210, v_nz_mask);
    count -= 8;
  } while (count != 0);
  *eob_ptr = get_max_eob(v_eobmax_76543210);
}

static AOM_FORCE_INLINE uint16x8_t quantize_fp_logscale_8(
    const tran_low_t *coeff_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, int16x8_t v_quant, int16x8_t v_dequant,
    int16x8_t v_round, int16x8_t v_zero, int log_scale) {
  const int16x8_t v_log_scale_minus_1 = vdupq_n_s16(log_scale - 1);
  const int16x8_t v_neg_log_scale_plus_1 = vdupq_n_s16(-(1 + log_scale));
  const int16x8_t v_coeff = load_tran_low_to_s16q(coeff_ptr);
  const int16x8_t v_coeff_sign = vshrq_n_s16(v_coeff, 15);
  const int16x8_t v_abs_coeff = vabsq_s16(v_coeff);
  const uint16x8_t v_mask =
      vcgeq_s16(v_abs_coeff, vshlq_s16(v_dequant, v_neg_log_scale_plus_1));
  // const int64_t tmp = vmask ? (int64_t)abs_coeff + log_scaled_round : 0
  const int16x8_t v_tmp = vandq_s16(vqaddq_s16(v_abs_coeff, v_round),
                                    vreinterpretq_s16_u16(v_mask));
  const int16x8_t v_tmp2 =
      vqdmulhq_s16(vshlq_s16(v_tmp, v_log_scale_minus_1), v_quant);
  const uint16x8_t v_nz_mask = vcgtq_s16(v_tmp2, v_zero);
  const int16x8_t v_qcoeff =
      vsubq_s16(veorq_s16(v_tmp2, v_coeff_sign), v_coeff_sign);
  // Multiplying by dequant here will use all 16 bits. Cast to unsigned before
  // shifting right. (vshlq_s16 will shift right if shift value is negative)
  const uint16x8_t v_abs_dqcoeff =
      vshlq_u16(vreinterpretq_u16_s16(vmulq_s16(v_tmp2, v_dequant)),
                vdupq_n_s16(-log_scale));
  const int16x8_t v_dqcoeff =
      vsubq_s16(veorq_s16(vreinterpretq_s16_u16(v_abs_dqcoeff), v_coeff_sign),
                v_coeff_sign);
  store_s16q_to_tran_low(qcoeff_ptr, v_qcoeff);
  store_s16q_to_tran_low(dqcoeff_ptr, v_dqcoeff);
  return v_nz_mask;
}

static AOM_FORCE_INLINE uint16x8_t quantize_fp_logscale2_8(
    const tran_low_t *coeff_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, int16x8_t v_quant, int16x8_t v_dequant,
    int16x8_t v_round, int16x8_t v_zero) {
  const int16x8_t v_coeff = load_tran_low_to_s16q(coeff_ptr);
  const int16x8_t v_coeff_sign = vshrq_n_s16(v_coeff, 15);
  const int16x8_t v_abs_coeff = vabsq_s16(v_coeff);
  const uint16x8_t v_mask =
      vcgeq_u16(vshlq_n_u16(vreinterpretq_u16_s16(v_abs_coeff), 1),
                vshrq_n_u16(vreinterpretq_u16_s16(v_dequant), 2));
  // abs_coeff = vmask ? (int64_t)abs_coeff + log_scaled_round : 0
  const int16x8_t v_tmp = vandq_s16(vqaddq_s16(v_abs_coeff, v_round),
                                    vreinterpretq_s16_u16(v_mask));
  // tmp32 = (int)((abs_coeff * quant_ptr[rc != 0]) >> (16 - log_scale));
  const int16x8_t v_tmp2 =
      vorrq_s16(vshlq_n_s16(vqdmulhq_s16(v_tmp, v_quant), 1),
                vreinterpretq_s16_u16(vshrq_n_u16(
                    vreinterpretq_u16_s16(vmulq_s16(v_tmp, v_quant)), 14)));
  const uint16x8_t v_nz_mask = vcgtq_s16(v_tmp2, v_zero);
  const int16x8_t v_qcoeff =
      vsubq_s16(veorq_s16(v_tmp2, v_coeff_sign), v_coeff_sign);
  // const tran_low_t abs_dqcoeff = (tmp32 * dequant_ptr[rc != 0]) >> log_scale;
  const int16x8_t v_abs_dqcoeff =
      vorrq_s16(vshlq_n_s16(vqdmulhq_s16(v_tmp2, v_dequant), 13),
                vreinterpretq_s16_u16(vshrq_n_u16(
                    vreinterpretq_u16_s16(vmulq_s16(v_tmp2, v_dequant)), 2)));
  const int16x8_t v_dqcoeff =
      vsubq_s16(veorq_s16(v_abs_dqcoeff, v_coeff_sign), v_coeff_sign);
  store_s16q_to_tran_low(qcoeff_ptr, v_qcoeff);
  store_s16q_to_tran_low(dqcoeff_ptr, v_dqcoeff);
  return v_nz_mask;
}

static AOM_FORCE_INLINE void quantize_fp_no_qmatrix_neon(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *round_ptr,
    const int16_t *quant_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
    const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *iscan,
    int log_scale) {
  const int16x8_t v_zero = vdupq_n_s16(0);
  int16x8_t v_quant = vld1q_s16(quant_ptr);
  int16x8_t v_dequant = vld1q_s16(dequant_ptr);
  const int16x8_t v_round_no_scale = vld1q_s16(round_ptr);
  int16x8_t v_round =
      vqrdmulhq_n_s16(v_round_no_scale, (int16_t)(1 << (15 - log_scale)));
  int16x8_t v_eobmax_76543210 = vdupq_n_s16(-1);
  intptr_t non_zero_count = n_coeffs;

  assert(n_coeffs > 16);
  // Pre-scan pass
  const int16x8_t v_dequant_scaled =
      vshlq_s16(v_dequant, vdupq_n_s16(-(1 + log_scale)));
  const int16x8_t v_zbin_s16 =
      vdupq_lane_s16(vget_low_s16(v_dequant_scaled), 1);
  intptr_t i = n_coeffs;
  do {
    const int16x8_t v_coeff_a = load_tran_low_to_s16q(coeff_ptr + i - 8);
    const int16x8_t v_coeff_b = load_tran_low_to_s16q(coeff_ptr + i - 16);
    const int16x8_t v_abs_coeff_a = vabsq_s16(v_coeff_a);
    const int16x8_t v_abs_coeff_b = vabsq_s16(v_coeff_b);
    const uint16x8_t v_mask_a = vcgeq_s16(v_abs_coeff_a, v_zbin_s16);
    const uint16x8_t v_mask_b = vcgeq_s16(v_abs_coeff_b, v_zbin_s16);
    // If the coefficient is in the base ZBIN range, then discard.
    if (horizontal_long_add_u16x8(v_mask_a, v_mask_b) == 0) {
      non_zero_count -= 16;
    } else {
      break;
    }
    i -= 16;
  } while (i > 0);

  const intptr_t remaining_zcoeffs = n_coeffs - non_zero_count;
  memset(qcoeff_ptr + non_zero_count, 0,
         remaining_zcoeffs * sizeof(*qcoeff_ptr));
  memset(dqcoeff_ptr + non_zero_count, 0,
         remaining_zcoeffs * sizeof(*dqcoeff_ptr));

  // process dc and the first seven ac coeffs
  uint16x8_t v_nz_mask;
  if (log_scale == 2) {
    v_nz_mask = quantize_fp_logscale2_8(coeff_ptr, qcoeff_ptr, dqcoeff_ptr,
                                        v_quant, v_dequant, v_round, v_zero);
  } else {
    v_nz_mask =
        quantize_fp_logscale_8(coeff_ptr, qcoeff_ptr, dqcoeff_ptr, v_quant,
                               v_dequant, v_round, v_zero, log_scale);
  }
  v_eobmax_76543210 = get_max_lane_eob(iscan, v_eobmax_76543210, v_nz_mask);
  // overwrite the dc constants with ac constants
  v_quant = vdupq_lane_s16(vget_low_s16(v_quant), 1);
  v_dequant = vdupq_lane_s16(vget_low_s16(v_dequant), 1);
  v_round = vdupq_lane_s16(vget_low_s16(v_round), 1);

  for (intptr_t count = non_zero_count - 8; count > 0; count -= 8) {
    coeff_ptr += 8;
    qcoeff_ptr += 8;
    dqcoeff_ptr += 8;
    iscan += 8;
    if (log_scale == 2) {
      v_nz_mask = quantize_fp_logscale2_8(coeff_ptr, qcoeff_ptr, dqcoeff_ptr,
                                          v_quant, v_dequant, v_round, v_zero);
    } else {
      v_nz_mask =
          quantize_fp_logscale_8(coeff_ptr, qcoeff_ptr, dqcoeff_ptr, v_quant,
                                 v_dequant, v_round, v_zero, log_scale);
    }
    v_eobmax_76543210 = get_max_lane_eob(iscan, v_eobmax_76543210, v_nz_mask);
  }
  *eob_ptr = get_max_eob(v_eobmax_76543210);
}

void av1_quantize_fp_32x32_neon(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                                const int16_t *zbin_ptr,
                                const int16_t *round_ptr,
                                const int16_t *quant_ptr,
                                const int16_t *quant_shift_ptr,
                                tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                                const int16_t *dequant_ptr, uint16_t *eob_ptr,
                                const int16_t *scan, const int16_t *iscan) {
  (void)zbin_ptr;
  (void)quant_shift_ptr;
  (void)scan;
  quantize_fp_no_qmatrix_neon(coeff_ptr, n_coeffs, round_ptr, quant_ptr,
                              qcoeff_ptr, dqcoeff_ptr, dequant_ptr, eob_ptr,
                              iscan, 1);
}

void av1_quantize_fp_64x64_neon(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                                const int16_t *zbin_ptr,
                                const int16_t *round_ptr,
                                const int16_t *quant_ptr,
                                const int16_t *quant_shift_ptr,
                                tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                                const int16_t *dequant_ptr, uint16_t *eob_ptr,
                                const int16_t *scan, const int16_t *iscan) {
  (void)zbin_ptr;
  (void)quant_shift_ptr;
  (void)scan;
  quantize_fp_no_qmatrix_neon(coeff_ptr, n_coeffs, round_ptr, quant_ptr,
                              qcoeff_ptr, dqcoeff_ptr, dequant_ptr, eob_ptr,
                              iscan, 2);
}

void aom_quantize_b_neon(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                         const int16_t *zbin_ptr, const int16_t *round_ptr,
                         const int16_t *quant_ptr,
                         const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
                         tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr,
                         uint16_t *eob_ptr, const int16_t *scan,
                         const int16_t *iscan) {
  (void)quant_shift_ptr;
  (void)scan;

  const int zbins[2] = { zbin_ptr[0], zbin_ptr[1] };

  memset(qcoeff_ptr, 0, n_coeffs * sizeof(*qcoeff_ptr));
  memset(dqcoeff_ptr, 0, n_coeffs * sizeof(*dqcoeff_ptr));

  const int16x8_t zero = vdupq_n_s16(0);
  int16x8_t v_eobmax_76543210 = vreinterpretq_s16_u16(vceqq_s16(zero, zero));

  int16x8_t vzbins = vdupq_n_s16(zbins[1]), vround = vdupq_n_s16(round_ptr[1]);
  int16x8_t vdequant = vdupq_n_s16(dequant_ptr[1]);
  int16x8_t vquant = vdupq_n_s16(quant_ptr[1]);
  int16x8_t vquant_shift = vdupq_n_s16(quant_shift_ptr[1]);

  int16x8_t v_coeff = load_tran_low_to_s16q(&coeff_ptr[0]);
  int16x8_t v_coeff_sign = vshrq_n_s16(v_coeff, 15);
  int16x8_t v_abs = vabsq_s16(v_coeff);

  vzbins = vsetq_lane_s16(zbins[0], vzbins, 0);

  uint16x8_t vcond = vcgeq_s16(v_abs, vzbins);
  uint64_t nz_check = vget_lane_u64(vreinterpret_u64_u8(vmovn_u16(vcond)), 0);
  if (nz_check) {
    vround = vsetq_lane_s16(round_ptr[0], vround, 0);
    vquant = vsetq_lane_s16(quant_ptr[0], vquant, 0);
    vdequant = vsetq_lane_s16(dequant_ptr[0], vdequant, 0);
    vquant_shift = vsetq_lane_s16(quant_shift_ptr[0], vquant_shift, 0);

    int16x8_t vtmp = vqaddq_s16(v_abs, vround);
    int16x8_t vtmp2 = vsraq_n_s16(vtmp, vqdmulhq_s16(vtmp, vquant), 1);
    vtmp2 = vshrq_n_s16(vqdmulhq_s16(vtmp2, vquant_shift), 1);

    int16x8_t vdest = vsubq_s16(veorq_s16(vtmp2, v_coeff_sign), v_coeff_sign);
    int16x8_t coeff_nz_mask =
        vbslq_s16(vcond, vdest, load_tran_low_to_s16q(&qcoeff_ptr[0]));
    store_s16q_to_tran_low(&qcoeff_ptr[0], coeff_nz_mask);
    int16x8_t v_deq_abs = vmulq_s16(vtmp2, vdequant);

    vdest = vsubq_s16(veorq_s16(v_deq_abs, v_coeff_sign), v_coeff_sign);
    coeff_nz_mask =
        vbslq_s16(vcond, vdest, load_tran_low_to_s16q(&dqcoeff_ptr[0]));
    store_s16q_to_tran_low(&dqcoeff_ptr[0], coeff_nz_mask);

    vround = vsetq_lane_s16(round_ptr[1], vround, 0);
    vquant = vsetq_lane_s16(quant_ptr[1], vquant, 0);
    vdequant = vsetq_lane_s16(dequant_ptr[1], vdequant, 0);
    vquant_shift = vsetq_lane_s16(quant_shift_ptr[1], vquant_shift, 0);

    uint16x8_t vtmp_mask = vcgtq_s16(vtmp2, zero);
    const uint16x8_t v_nz_mask = vandq_u16(vtmp_mask, vcond);
    int16x8_t v_iscan = vld1q_s16(&iscan[0]);
    vcond = vandq_u16(v_nz_mask, vcgtq_s16(v_iscan, v_eobmax_76543210));
    v_eobmax_76543210 = vbslq_s16(vcond, v_iscan, v_eobmax_76543210);
  }
  vzbins = vsetq_lane_s16(zbins[1], vzbins, 0);

  for (int i = 8; i < n_coeffs; i += 8) {
    v_coeff = load_tran_low_to_s16q(&coeff_ptr[i]);
    v_coeff_sign = vshrq_n_s16(v_coeff, 15);
    v_abs = vabsq_s16(v_coeff);
    vcond = vcgeq_s16(v_abs, vzbins);

    nz_check = vget_lane_u64(vreinterpret_u64_u8(vmovn_u16(vcond)), 0);
    if (nz_check) {
      int16x8_t vtmp = vqaddq_s16(v_abs, vround);
      int16x8_t vtmp2 = vsraq_n_s16(vtmp, vqdmulhq_s16(vtmp, vquant), 1);

      vtmp2 = vshrq_n_s16(vqdmulhq_s16(vtmp2, vquant_shift), 1);
      int16x8_t vdest = vsubq_s16(veorq_s16(vtmp2, v_coeff_sign), v_coeff_sign);
      int16x8_t coeff_nz_mask =
          vbslq_s16(vcond, vdest, load_tran_low_to_s16q(&qcoeff_ptr[i]));
      store_s16q_to_tran_low(&qcoeff_ptr[i], coeff_nz_mask);
      int16x8_t v_deq_abs = vmulq_s16(vtmp2, vdequant);
      vdest = vsubq_s16(veorq_s16(v_deq_abs, v_coeff_sign), v_coeff_sign);
      coeff_nz_mask =
          vbslq_s16(vcond, vdest, load_tran_low_to_s16q(&dqcoeff_ptr[i]));
      store_s16q_to_tran_low(&dqcoeff_ptr[i], coeff_nz_mask);

      uint16x8_t vtmp_mask = vcgtq_s16(vtmp2, zero);
      const uint16x8_t v_nz_mask = vandq_u16(vtmp_mask, vcond);
      int16x8_t v_iscan = vld1q_s16(&iscan[i]);
      vcond = vandq_u16(v_nz_mask, vcgtq_s16(v_iscan, v_eobmax_76543210));
      v_eobmax_76543210 = vbslq_s16(vcond, v_iscan, v_eobmax_76543210);
    }
  }
  *eob_ptr = get_max_eob(v_eobmax_76543210) + 1;
}

#define QM_MULL_SHIFT(x0, x1)                                              \
  vreinterpretq_s16_u16(vorrq_u16(                                         \
      vreinterpretq_u16_s16(vshlq_n_s16(                                   \
          vqdmulhq_s16(x0, vreinterpretq_s16_u16(x1)), 15 - AOM_QM_BITS)), \
      vshrq_n_u16(vmulq_u16(vreinterpretq_u16_s16(x0), x1), AOM_QM_BITS)))

static void aom_quantize_b_helper_16x16_neon(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan, const qm_val_t *qm_ptr,
    const qm_val_t *iqm_ptr) {
  (void)scan;

  uint16x8_t vwt, viwt;
  const int zbins[2] = { zbin_ptr[0], zbin_ptr[1] };

  memset(qcoeff_ptr, 0, n_coeffs * sizeof(*qcoeff_ptr));
  memset(dqcoeff_ptr, 0, n_coeffs * sizeof(*dqcoeff_ptr));

  const int16x8_t zero = vdupq_n_s16(0);
  int16x8_t v_eobmax_76543210 = vreinterpretq_s16_u16(vceqq_s16(zero, zero));

  int16x8_t vzbins = vdupq_n_s16(zbins[1]), vround = vdupq_n_s16(round_ptr[1]);
  int16x8_t vdequant = vdupq_n_s16(dequant_ptr[1]);
  int16x8_t vquant = vdupq_n_s16(quant_ptr[1]);
  int16x8_t vquant_shift = vdupq_n_s16(quant_shift_ptr[1]);

  int16x8_t v_coeff = load_tran_low_to_s16q(&coeff_ptr[0]);
  int16x8_t v_coeff_sign = vshrq_n_s16(v_coeff, 15);
  int16x8_t v_abs = vabsq_s16(v_coeff);
  vzbins = vsetq_lane_s16(zbins[0], vzbins, 0);
  uint16x8_t vcond;
  if (qm_ptr == NULL) {
    vcond = vcgeq_s16(v_abs, vzbins);
  } else {
    vwt = vmovl_u8(vld1_u8(&qm_ptr[0]));
    vcond = vcgeq_s16(QM_MULL_SHIFT(v_abs, vwt), vzbins);
  }
  uint64_t nz_check = vget_lane_u64(vreinterpret_u64_u8(vmovn_u16(vcond)), 0);
  if (nz_check) {
    vround = vsetq_lane_s16(round_ptr[0], vround, 0);
    vquant = vsetq_lane_s16(quant_ptr[0], vquant, 0);
    vdequant = vsetq_lane_s16(dequant_ptr[0], vdequant, 0);
    vquant_shift = vsetq_lane_s16(quant_shift_ptr[0], vquant_shift, 0);

    int16x8_t vtmp = vqaddq_s16(v_abs, vround);

    int16x8_t vtmp2;
    if (qm_ptr == NULL) {
      vtmp2 = vsraq_n_s16(vtmp, vqdmulhq_s16(vtmp, vquant), 1);
    } else {
      vtmp2 = QM_MULL_SHIFT(vtmp, vwt);
      vtmp2 = vaddq_s16(vtmp2, vtmp);
    }

    vtmp2 = vshrq_n_s16(vqdmulhq_s16(vtmp2, vquant_shift), 1);
    int16x8_t vdest = vsubq_s16(veorq_s16(vtmp2, v_coeff_sign), v_coeff_sign);
    int16x8_t coeff_nz_mask =
        vbslq_s16(vcond, vdest, load_tran_low_to_s16q(&qcoeff_ptr[0]));
    store_s16q_to_tran_low(&qcoeff_ptr[0], coeff_nz_mask);

    if (iqm_ptr != NULL) {
      viwt = vmovl_u8(vld1_u8(&iqm_ptr[0]));
      vdequant = QM_MULL_SHIFT(vdequant, viwt);
    }
    int16x8_t v_deq_abs = vmulq_s16(vtmp2, vdequant);
    vdest = vsubq_s16(veorq_s16(v_deq_abs, v_coeff_sign), v_coeff_sign);
    coeff_nz_mask =
        vbslq_s16(vcond, vdest, load_tran_low_to_s16q(&dqcoeff_ptr[0]));
    store_s16q_to_tran_low(&dqcoeff_ptr[0], coeff_nz_mask);

    vround = vsetq_lane_s16(round_ptr[1], vround, 0);
    vquant = vsetq_lane_s16(quant_ptr[1], vquant, 0);
    vdequant = vsetq_lane_s16(dequant_ptr[1], vdequant, 0);
    vquant_shift = vsetq_lane_s16(quant_shift_ptr[1], vquant_shift, 0);

    uint16x8_t vtmp_mask = vcgtq_s16(vtmp2, zero);
    const uint16x8_t v_nz_mask = vandq_u16(vtmp_mask, vcond);
    int16x8_t v_iscan = vld1q_s16(&iscan[0]);
    vcond = vandq_u16(v_nz_mask, vcgtq_s16(v_iscan, v_eobmax_76543210));
    v_eobmax_76543210 = vbslq_s16(vcond, v_iscan, v_eobmax_76543210);
  }
  vzbins = vsetq_lane_s16(zbins[1], vzbins, 0);

  for (int i = 8; i < n_coeffs; i += 8) {
    v_coeff = load_tran_low_to_s16q(&coeff_ptr[i]);
    v_coeff_sign = vshrq_n_s16(v_coeff, 15);
    v_abs = vabsq_s16(v_coeff);

    if (qm_ptr == NULL) {
      vcond = vcgeq_s16(v_abs, vzbins);
    } else {
      vwt = vmovl_u8(vld1_u8(&qm_ptr[i]));
      vcond = vcgeq_s16(QM_MULL_SHIFT(v_abs, vwt), vzbins);
    }
    nz_check = vget_lane_u64(vreinterpret_u64_u8(vmovn_u16(vcond)), 0);
    if (nz_check) {
      int16x8_t vtmp = vqaddq_s16(v_abs, vround);

      int16x8_t vtmp2;
      if (qm_ptr == NULL) {
        vtmp2 = vsraq_n_s16(vtmp, vqdmulhq_s16(vtmp, vquant), 1);
      } else {
        vtmp2 = QM_MULL_SHIFT(vtmp, vwt);
        vtmp2 = vaddq_s16(vtmp2, vtmp);
      }

      vtmp2 = vshrq_n_s16(vqdmulhq_s16(vtmp2, vquant_shift), 1);
      int16x8_t vdest = vsubq_s16(veorq_s16(vtmp2, v_coeff_sign), v_coeff_sign);
      int16x8_t coeff_nz_mask =
          vbslq_s16(vcond, vdest, load_tran_low_to_s16q(&qcoeff_ptr[i]));
      store_s16q_to_tran_low(&qcoeff_ptr[i], coeff_nz_mask);

      if (iqm_ptr != NULL) {
        viwt = vmovl_u8(vld1_u8(&iqm_ptr[i]));
        vdequant = QM_MULL_SHIFT(vdequant, viwt);
      }
      int16x8_t v_deq_abs = vmulq_s16(vtmp2, vdequant);
      vdest = vsubq_s16(veorq_s16(v_deq_abs, v_coeff_sign), v_coeff_sign);
      coeff_nz_mask =
          vbslq_s16(vcond, vdest, load_tran_low_to_s16q(&dqcoeff_ptr[i]));
      store_s16q_to_tran_low(&dqcoeff_ptr[i], coeff_nz_mask);

      uint16x8_t vtmp_mask = vcgtq_s16(vtmp2, zero);
      const uint16x8_t v_nz_mask = vandq_u16(vtmp_mask, vcond);
      int16x8_t v_iscan = vld1q_s16(&iscan[i]);
      vcond = vandq_u16(v_nz_mask, vcgtq_s16(v_iscan, v_eobmax_76543210));
      v_eobmax_76543210 = vbslq_s16(vcond, v_iscan, v_eobmax_76543210);
    }
  }
  *eob_ptr = get_max_eob(v_eobmax_76543210) + 1;
}

static void aom_quantize_b_helper_32x32_neon(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan, const qm_val_t *qm_ptr,
    const qm_val_t *iqm_ptr) {
  (void)scan;

  uint16x8_t vwt, viwt;
  const int log_scale = 1;
  const int zbins[2] = { ROUND_POWER_OF_TWO(zbin_ptr[0], log_scale),
                         ROUND_POWER_OF_TWO(zbin_ptr[1], log_scale) };

  memset(qcoeff_ptr, 0, n_coeffs * sizeof(*qcoeff_ptr));
  memset(dqcoeff_ptr, 0, n_coeffs * sizeof(*dqcoeff_ptr));

  const int16x8_t zero = vdupq_n_s16(0);
  int16x8_t v_eobmax_76543210 = vreinterpretq_s16_u16(vceqq_s16(zero, zero));
  const int16x8_t v_log_scale = v_eobmax_76543210;

  int16x8_t vzbins = vdupq_n_s16(zbins[1]),
            vround = vdupq_n_s16(ROUND_POWER_OF_TWO(round_ptr[1], log_scale));
  int16x8_t vdequant = vdupq_n_s16(dequant_ptr[1]);
  int16x8_t vquant = vdupq_n_s16(quant_ptr[1]);
  int16x8_t vquant_shift = vdupq_n_s16(quant_shift_ptr[1]);

  int16x8_t v_coeff = load_tran_low_to_s16q(&coeff_ptr[0]);
  int16x8_t v_coeff_sign = vshrq_n_s16(v_coeff, 15);
  int16x8_t v_abs = vabsq_s16(v_coeff);
  vzbins = vsetq_lane_s16(zbins[0], vzbins, 0);
  uint16x8_t vcond;
  if (qm_ptr == NULL) {
    vcond = vcgeq_s16(v_abs, vzbins);
  } else {
    vwt = vmovl_u8(vld1_u8(&qm_ptr[0]));
    vcond = vcgeq_s16(QM_MULL_SHIFT(v_abs, vwt), vzbins);
  }
  uint64_t nz_check = vget_lane_u64(vreinterpret_u64_u8(vmovn_u16(vcond)), 0);
  if (nz_check) {
    vround =
        vsetq_lane_s16(ROUND_POWER_OF_TWO(round_ptr[0], log_scale), vround, 0);
    vquant = vsetq_lane_s16(quant_ptr[0], vquant, 0);
    vdequant = vsetq_lane_s16(dequant_ptr[0], vdequant, 0);
    vquant_shift = vsetq_lane_s16(quant_shift_ptr[0], vquant_shift, 0);

    int16x8_t vtmp = vqaddq_s16(v_abs, vround);

    int16x8_t vtmp2;
    if (qm_ptr == NULL) {
      vtmp2 = vsraq_n_s16(vtmp, vqdmulhq_s16(vtmp, vquant), 1);
    } else {
      vtmp2 = QM_MULL_SHIFT(vtmp, vwt);
      vtmp2 = vaddq_s16(vtmp2, vtmp);
    }

    vtmp2 = vqdmulhq_s16(vtmp2, vquant_shift);
    int16x8_t vdest = vsubq_s16(veorq_s16(vtmp2, v_coeff_sign), v_coeff_sign);
    int16x8_t coeff_nz_mask =
        vbslq_s16(vcond, vdest, load_tran_low_to_s16q(&qcoeff_ptr[0]));
    store_s16q_to_tran_low(&qcoeff_ptr[0], coeff_nz_mask);

    if (iqm_ptr != NULL) {
      viwt = vmovl_u8(vld1_u8(&iqm_ptr[0]));
      vdequant = QM_MULL_SHIFT(vdequant, viwt);
    }
    int16x8_t v_deq_abs = vreinterpretq_s16_u16(vshlq_u16(
        vreinterpretq_u16_s16(vmulq_s16(vtmp2, vdequant)), v_log_scale));
    vdest = vsubq_s16(veorq_s16(v_deq_abs, v_coeff_sign), v_coeff_sign);
    coeff_nz_mask =
        vbslq_s16(vcond, vdest, load_tran_low_to_s16q(&dqcoeff_ptr[0]));
    store_s16q_to_tran_low(&dqcoeff_ptr[0], coeff_nz_mask);

    vzbins = vsetq_lane_s16(zbins[1], vzbins, 0);
    vround =
        vsetq_lane_s16(ROUND_POWER_OF_TWO(round_ptr[1], log_scale), vround, 0);
    vquant = vsetq_lane_s16(quant_ptr[1], vquant, 0);
    vdequant = vsetq_lane_s16(dequant_ptr[1], vdequant, 0);
    vquant_shift = vsetq_lane_s16(quant_shift_ptr[1], vquant_shift, 0);

    uint16x8_t vtmp_mask = vcgtq_s16(vtmp2, zero);
    const uint16x8_t v_nz_mask = vandq_u16(vtmp_mask, vcond);
    int16x8_t v_iscan = vld1q_s16(&iscan[0]);
    vcond = vandq_u16(v_nz_mask, vcgtq_s16(v_iscan, v_eobmax_76543210));
    v_eobmax_76543210 = vbslq_s16(vcond, v_iscan, v_eobmax_76543210);
  }
  vzbins = vsetq_lane_s16(zbins[1], vzbins, 0);

  for (int i = 8; i < n_coeffs; i += 8) {
    v_coeff = load_tran_low_to_s16q(&coeff_ptr[i]);
    v_coeff_sign = vshrq_n_s16(v_coeff, 15);
    v_abs = vabsq_s16(v_coeff);

    if (qm_ptr == NULL) {
      vcond = vcgeq_s16(v_abs, vzbins);
    } else {
      vwt = vmovl_u8(vld1_u8(&qm_ptr[i]));
      vcond = vcgeq_s16(QM_MULL_SHIFT(v_abs, vwt), vzbins);
    }
    nz_check = vget_lane_u64(vreinterpret_u64_u8(vmovn_u16(vcond)), 0);
    if (nz_check) {
      int16x8_t vtmp = vqaddq_s16(v_abs, vround);

      int16x8_t vtmp2;
      if (qm_ptr == NULL) {
        vtmp2 = vsraq_n_s16(vtmp, vqdmulhq_s16(vtmp, vquant), 1);
      } else {
        vtmp2 = QM_MULL_SHIFT(vtmp, vwt);
        vtmp2 = vaddq_s16(vtmp2, vtmp);
      }
      vtmp2 = vqdmulhq_s16(vtmp2, vquant_shift);

      int16x8_t vdest = vsubq_s16(veorq_s16(vtmp2, v_coeff_sign), v_coeff_sign);
      int16x8_t coeff_nz_mask =
          vbslq_s16(vcond, vdest, load_tran_low_to_s16q(&qcoeff_ptr[i]));
      store_s16q_to_tran_low(&qcoeff_ptr[i], coeff_nz_mask);

      if (iqm_ptr != NULL) {
        viwt = vmovl_u8(vld1_u8(&iqm_ptr[i]));
        vdequant = QM_MULL_SHIFT(vdequant, viwt);
      }
      int16x8_t v_deq_abs = vreinterpretq_s16_u16(vshlq_u16(
          vreinterpretq_u16_s16(vmulq_s16(vtmp2, vdequant)), v_log_scale));
      vdest = vsubq_s16(veorq_s16(v_deq_abs, v_coeff_sign), v_coeff_sign);
      coeff_nz_mask =
          vbslq_s16(vcond, vdest, load_tran_low_to_s16q(&dqcoeff_ptr[i]));
      store_s16q_to_tran_low(&dqcoeff_ptr[i], coeff_nz_mask);

      uint16x8_t vtmp_mask = vcgtq_s16(vtmp2, zero);
      const uint16x8_t v_nz_mask = vandq_u16(vtmp_mask, vcond);
      int16x8_t v_iscan = vld1q_s16(&iscan[i]);
      vcond = vandq_u16(v_nz_mask, vcgtq_s16(v_iscan, v_eobmax_76543210));
      v_eobmax_76543210 = vbslq_s16(vcond, v_iscan, v_eobmax_76543210);
    }
  }
  *eob_ptr = get_max_eob(v_eobmax_76543210) + 1;
}

static void aom_quantize_b_helper_64x64_neon(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan, const qm_val_t *qm_ptr,
    const qm_val_t *iqm_ptr) {
  (void)scan;

  uint16x8_t vwt, viwt;
  const int log_scale = 2;
  const int16x8_t v_log_scale =
      vreinterpretq_s16_s64(vdupq_n_s64(0xFFFEFFFEFFFEFFFE));

  const int zbins[2] = { ROUND_POWER_OF_TWO(zbin_ptr[0], log_scale),
                         ROUND_POWER_OF_TWO(zbin_ptr[1], log_scale) };

  memset(qcoeff_ptr, 0, n_coeffs * sizeof(*qcoeff_ptr));
  memset(dqcoeff_ptr, 0, n_coeffs * sizeof(*dqcoeff_ptr));

  const int16x8_t zero = vdupq_n_s16(0);
  int16x8_t v_eobmax_76543210 = vreinterpretq_s16_u16(vceqq_s16(zero, zero));
  int16x8_t v_ones = vnegq_s16(v_eobmax_76543210);

  int16x8_t vzbins = vdupq_n_s16(zbins[1]),
            vround = vdupq_n_s16(ROUND_POWER_OF_TWO(round_ptr[1], log_scale));
  int16x8_t vdequant = vdupq_n_s16(dequant_ptr[1]);
  int16x8_t vquant = vdupq_n_s16(quant_ptr[1]);
  int16x8_t vquant_shift = vdupq_n_s16(quant_shift_ptr[1]);

  int16x8_t v_coeff = load_tran_low_to_s16q(&coeff_ptr[0]);
  int16x8_t v_coeff_sign = vshrq_n_s16(v_coeff, 15);
  int16x8_t v_abs = vabsq_s16(v_coeff);
  vzbins = vsetq_lane_s16(zbins[0], vzbins, 0);
  uint16x8_t vcond;
  if (qm_ptr == NULL) {
    vcond = vcgeq_s16(v_abs, vzbins);
  } else {
    vwt = vmovl_u8(vld1_u8(&qm_ptr[0]));
    vcond = vcgeq_s16(QM_MULL_SHIFT(v_abs, vwt), vzbins);
  }
  uint64_t nz_check = vget_lane_u64(vreinterpret_u64_u8(vmovn_u16(vcond)), 0);
  if (nz_check) {
    vround =
        vsetq_lane_s16(ROUND_POWER_OF_TWO(round_ptr[0], log_scale), vround, 0);
    vquant = vsetq_lane_s16(quant_ptr[0], vquant, 0);
    vdequant = vsetq_lane_s16(dequant_ptr[0], vdequant, 0);
    vquant_shift = vsetq_lane_s16(quant_shift_ptr[0], vquant_shift, 0);
    int16x8_t vtmp = vqaddq_s16(v_abs, vround);

    int16x8_t vtmp2;
    if (qm_ptr == NULL) {
      vtmp2 = vsraq_n_s16(vtmp, vqdmulhq_s16(vtmp, vquant), 1);
    } else {
      vtmp2 = QM_MULL_SHIFT(vtmp, vwt);
      vtmp2 = vaddq_s16(vtmp2, vtmp);
    }

    int16x8_t ones =
        vandq_s16(vshrq_n_s16(vmulq_s16(vtmp2, vquant_shift), 14), v_ones);
    vtmp2 =
        vaddq_s16(vshlq_s16(vqdmulhq_s16(vtmp2, vquant_shift), v_ones), ones);
    int16x8_t vdest = vsubq_s16(veorq_s16(vtmp2, v_coeff_sign), v_coeff_sign);
    int16x8_t coeff_nz_mask =
        vbslq_s16(vcond, vdest, load_tran_low_to_s16q(&qcoeff_ptr[0]));
    store_s16q_to_tran_low(&qcoeff_ptr[0], coeff_nz_mask);

    if (iqm_ptr != NULL) {
      viwt = vmovl_u8(vld1_u8(&iqm_ptr[0]));
      vdequant = QM_MULL_SHIFT(vdequant, viwt);
    }
    int16x8_t v_deq_abs = vreinterpretq_s16_u16(vshlq_u16(
        vreinterpretq_u16_s16(vmulq_s16(vtmp2, vdequant)), v_log_scale));
    v_deq_abs =
        vorrq_s16(vshlq_n_s16(vqdmulhq_s16(vtmp2, vdequant), 13), v_deq_abs);
    vdest = vsubq_s16(veorq_s16(v_deq_abs, v_coeff_sign), v_coeff_sign);
    coeff_nz_mask =
        vbslq_s16(vcond, vdest, load_tran_low_to_s16q(&dqcoeff_ptr[0]));
    store_s16q_to_tran_low(&dqcoeff_ptr[0], coeff_nz_mask);

    vround =
        vsetq_lane_s16(ROUND_POWER_OF_TWO(round_ptr[1], log_scale), vround, 0);
    vquant = vsetq_lane_s16(quant_ptr[1], vquant, 0);
    vdequant = vsetq_lane_s16(dequant_ptr[1], vdequant, 0);
    vquant_shift = vsetq_lane_s16(quant_shift_ptr[1], vquant_shift, 0);

    uint16x8_t vtmp_mask = vcgtq_s16(vtmp2, zero);
    const uint16x8_t v_nz_mask = vandq_u16(vtmp_mask, vcond);
    int16x8_t v_iscan = vld1q_s16(&iscan[0]);
    vcond = vandq_u16(v_nz_mask, vcgtq_s16(v_iscan, v_eobmax_76543210));
    v_eobmax_76543210 = vbslq_s16(vcond, v_iscan, v_eobmax_76543210);
  }
  vzbins = vsetq_lane_s16(zbins[1], vzbins, 0);

  for (int i = 8; i < n_coeffs; i += 8) {
    v_coeff = load_tran_low_to_s16q(&coeff_ptr[i]);
    v_coeff_sign = vshrq_n_s16(v_coeff, 15);
    v_abs = vabsq_s16(v_coeff);

    if (qm_ptr == NULL) {
      vcond = vcgeq_s16(v_abs, vzbins);
    } else {
      vwt = vmovl_u8(vld1_u8(&qm_ptr[i]));
      vcond = vcgeq_s16(QM_MULL_SHIFT(v_abs, vwt), vzbins);
    }
    nz_check = vget_lane_u64(vreinterpret_u64_u8(vmovn_u16(vcond)), 0);
    if (nz_check) {
      int16x8_t vtmp = vqaddq_s16(v_abs, vround);

      int16x8_t vtmp2;
      if (qm_ptr == NULL) {
        vtmp2 = vsraq_n_s16(vtmp, vqdmulhq_s16(vtmp, vquant), 1);
      } else {
        vtmp2 = QM_MULL_SHIFT(vtmp, vwt);
        vtmp2 = vaddq_s16(vtmp2, vtmp);
      }

      int16x8_t ones =
          vandq_s16(vshrq_n_s16(vmulq_s16(vtmp2, vquant_shift), 14), v_ones);
      vtmp2 =
          vaddq_s16(vshlq_s16(vqdmulhq_s16(vtmp2, vquant_shift), v_ones), ones);
      int16x8_t vdest = vsubq_s16(veorq_s16(vtmp2, v_coeff_sign), v_coeff_sign);
      int16x8_t coeff_nz_mask =
          vbslq_s16(vcond, vdest, load_tran_low_to_s16q(&qcoeff_ptr[i]));
      store_s16q_to_tran_low(&qcoeff_ptr[i], coeff_nz_mask);

      if (iqm_ptr != NULL) {
        viwt = vmovl_u8(vld1_u8(&iqm_ptr[i]));
        vdequant = QM_MULL_SHIFT(vdequant, viwt);
      }
      int16x8_t v_deq_abs = vreinterpretq_s16_u16(vshlq_u16(
          vreinterpretq_u16_s16(vmulq_s16(vtmp2, vdequant)), v_log_scale));
      v_deq_abs =
          vorrq_s16(vshlq_n_s16(vqdmulhq_s16(vtmp2, vdequant), 13), v_deq_abs);
      vdest = vsubq_s16(veorq_s16(v_deq_abs, v_coeff_sign), v_coeff_sign);
      coeff_nz_mask =
          vbslq_s16(vcond, vdest, load_tran_low_to_s16q(&dqcoeff_ptr[i]));
      store_s16q_to_tran_low(&dqcoeff_ptr[i], coeff_nz_mask);

      uint16x8_t vtmp_mask = vcgtq_s16(vtmp2, zero);
      const uint16x8_t v_nz_mask = vandq_u16(vtmp_mask, vcond);
      int16x8_t v_iscan = vld1q_s16(&iscan[i]);
      vcond = vandq_u16(v_nz_mask, vcgtq_s16(v_iscan, v_eobmax_76543210));
      v_eobmax_76543210 = vbslq_s16(vcond, v_iscan, v_eobmax_76543210);
    }
  }
  *eob_ptr = get_max_eob(v_eobmax_76543210) + 1;
}

void aom_quantize_b_helper_neon(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan, const qm_val_t *qm_ptr,
    const qm_val_t *iqm_ptr, const int log_scale) {
  switch (log_scale) {  // log_scale for AV1 encoder can be only 0, 1, 2
    case 0:
      aom_quantize_b_helper_16x16_neon(coeff_ptr, n_coeffs, zbin_ptr, round_ptr,
                                       quant_ptr, quant_shift_ptr, qcoeff_ptr,
                                       dqcoeff_ptr, dequant_ptr, eob_ptr, scan,
                                       iscan, qm_ptr, iqm_ptr);
      break;
    case 1:
      aom_quantize_b_helper_32x32_neon(coeff_ptr, n_coeffs, zbin_ptr, round_ptr,
                                       quant_ptr, quant_shift_ptr, qcoeff_ptr,
                                       dqcoeff_ptr, dequant_ptr, eob_ptr, scan,
                                       iscan, qm_ptr, iqm_ptr);
      break;
    case 2:
      aom_quantize_b_helper_64x64_neon(coeff_ptr, n_coeffs, zbin_ptr, round_ptr,
                                       quant_ptr, quant_shift_ptr, qcoeff_ptr,
                                       dqcoeff_ptr, dequant_ptr, eob_ptr, scan,
                                       iscan, qm_ptr, iqm_ptr);
      break;
  }
}

void aom_quantize_b_32x32_neon(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                               const int16_t *zbin_ptr,
                               const int16_t *round_ptr,
                               const int16_t *quant_ptr,
                               const int16_t *quant_shift_ptr,
                               tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                               const int16_t *dequant_ptr, uint16_t *eob_ptr,
                               const int16_t *scan, const int16_t *iscan) {
  aom_quantize_b_helper_neon(coeff_ptr, n_coeffs, zbin_ptr, round_ptr,
                             quant_ptr, quant_shift_ptr, qcoeff_ptr,
                             dqcoeff_ptr, dequant_ptr, eob_ptr, scan, iscan,
                             NULL, NULL, 1);
}

void aom_quantize_b_64x64_neon(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                               const int16_t *zbin_ptr,
                               const int16_t *round_ptr,
                               const int16_t *quant_ptr,
                               const int16_t *quant_shift_ptr,
                               tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                               const int16_t *dequant_ptr, uint16_t *eob_ptr,
                               const int16_t *scan, const int16_t *iscan) {
  aom_quantize_b_helper_neon(coeff_ptr, n_coeffs, zbin_ptr, round_ptr,
                             quant_ptr, quant_shift_ptr, qcoeff_ptr,
                             dqcoeff_ptr, dequant_ptr, eob_ptr, scan, iscan,
                             NULL, NULL, 2);
}
