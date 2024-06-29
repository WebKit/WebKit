/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
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
#include <string.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/quantize.h"

static INLINE uint32_t sum_abs_coeff(const uint32x4_t a) {
#if AOM_ARCH_AARCH64
  return vaddvq_u32(a);
#else
  const uint64x2_t b = vpaddlq_u32(a);
  const uint64x1_t c = vadd_u64(vget_low_u64(b), vget_high_u64(b));
  return (uint32_t)vget_lane_u64(c, 0);
#endif
}

static INLINE uint16x4_t
quantize_4(const tran_low_t *coeff_ptr, tran_low_t *qcoeff_ptr,
           tran_low_t *dqcoeff_ptr, int32x4_t v_quant_s32,
           int32x4_t v_dequant_s32, int32x4_t v_round_s32, int32x4_t v_zbin_s32,
           int32x4_t v_quant_shift_s32, int log_scale) {
  const int32x4_t v_coeff = vld1q_s32(coeff_ptr);
  const int32x4_t v_coeff_sign =
      vreinterpretq_s32_u32(vcltq_s32(v_coeff, vdupq_n_s32(0)));
  const int32x4_t v_abs_coeff = vabsq_s32(v_coeff);
  // if (abs_coeff < zbins[rc != 0]),
  const uint32x4_t v_zbin_mask = vcgeq_s32(v_abs_coeff, v_zbin_s32);
  const int32x4_t v_log_scale = vdupq_n_s32(log_scale);
  // const int64_t tmp = (int64_t)abs_coeff + log_scaled_round;
  const int32x4_t v_tmp = vaddq_s32(v_abs_coeff, v_round_s32);
  //  const int32_t tmpw32 = tmp * wt;
  const int32x4_t v_tmpw32 = vmulq_s32(v_tmp, vdupq_n_s32((1 << AOM_QM_BITS)));
  //  const int32_t tmp2 = (int32_t)((tmpw32 * quant64) >> 16);
  const int32x4_t v_tmp2 = vqdmulhq_s32(v_tmpw32, v_quant_s32);
  // const int32_t tmp3 =
  //    ((((tmp2 + tmpw32)<< log_scale) * (int64_t)(quant_shift << 15)) >> 32);
  const int32x4_t v_tmp3 = vqdmulhq_s32(
      vshlq_s32(vaddq_s32(v_tmp2, v_tmpw32), v_log_scale), v_quant_shift_s32);
  // const int abs_qcoeff = vmask ? (int)tmp3 >> AOM_QM_BITS : 0;
  const int32x4_t v_abs_qcoeff = vandq_s32(vreinterpretq_s32_u32(v_zbin_mask),
                                           vshrq_n_s32(v_tmp3, AOM_QM_BITS));
  // const tran_low_t abs_dqcoeff = (abs_qcoeff * dequant_iwt) >> log_scale;
  // vshlq_s32 will shift right if shift value is negative.
  const int32x4_t v_abs_dqcoeff =
      vshlq_s32(vmulq_s32(v_abs_qcoeff, v_dequant_s32), vnegq_s32(v_log_scale));
  //  qcoeff_ptr[rc] = (tran_low_t)((abs_qcoeff ^ coeff_sign) - coeff_sign);
  const int32x4_t v_qcoeff =
      vsubq_s32(veorq_s32(v_abs_qcoeff, v_coeff_sign), v_coeff_sign);
  //  dqcoeff_ptr[rc] = (tran_low_t)((abs_dqcoeff ^ coeff_sign) - coeff_sign);
  const int32x4_t v_dqcoeff =
      vsubq_s32(veorq_s32(v_abs_dqcoeff, v_coeff_sign), v_coeff_sign);

  vst1q_s32(qcoeff_ptr, v_qcoeff);
  vst1q_s32(dqcoeff_ptr, v_dqcoeff);

  // Used to find eob.
  const uint32x4_t nz_qcoeff_mask = vcgtq_s32(v_abs_qcoeff, vdupq_n_s32(0));
  return vmovn_u32(nz_qcoeff_mask);
}

static INLINE int16x8_t get_max_lane_eob(const int16_t *iscan,
                                         int16x8_t v_eobmax,
                                         uint16x8_t v_mask) {
  const int16x8_t v_iscan = vld1q_s16(&iscan[0]);
  const int16x8_t v_iscan_plus1 = vaddq_s16(v_iscan, vdupq_n_s16(1));
  const int16x8_t v_nz_iscan = vbslq_s16(v_mask, v_iscan_plus1, vdupq_n_s16(0));
  return vmaxq_s16(v_eobmax, v_nz_iscan);
}

#if !CONFIG_REALTIME_ONLY
static INLINE void get_min_max_lane_eob(const int16_t *iscan,
                                        int16x8_t *v_eobmin,
                                        int16x8_t *v_eobmax, uint16x8_t v_mask,
                                        intptr_t n_coeffs) {
  const int16x8_t v_iscan = vld1q_s16(&iscan[0]);
  const int16x8_t v_nz_iscan_max = vbslq_s16(v_mask, v_iscan, vdupq_n_s16(-1));
#if SKIP_EOB_FACTOR_ADJUST
  const int16x8_t v_nz_iscan_min =
      vbslq_s16(v_mask, v_iscan, vdupq_n_s16((int16_t)n_coeffs));
  *v_eobmin = vminq_s16(*v_eobmin, v_nz_iscan_min);
#else
  (void)v_eobmin;
#endif
  *v_eobmax = vmaxq_s16(*v_eobmax, v_nz_iscan_max);
}
#endif  // !CONFIG_REALTIME_ONLY

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

#if SKIP_EOB_FACTOR_ADJUST && !CONFIG_REALTIME_ONLY
static INLINE uint16_t get_min_eob(int16x8_t v_eobmin) {
#if AOM_ARCH_AARCH64
  return (uint16_t)vminvq_s16(v_eobmin);
#else
  const int16x4_t v_eobmin_3210 =
      vmin_s16(vget_low_s16(v_eobmin), vget_high_s16(v_eobmin));
  const int64x1_t v_eobmin_xx32 =
      vshr_n_s64(vreinterpret_s64_s16(v_eobmin_3210), 32);
  const int16x4_t v_eobmin_tmp =
      vmin_s16(v_eobmin_3210, vreinterpret_s16_s64(v_eobmin_xx32));
  const int64x1_t v_eobmin_xxx3 =
      vshr_n_s64(vreinterpret_s64_s16(v_eobmin_tmp), 16);
  const int16x4_t v_eobmin_final =
      vmin_s16(v_eobmin_tmp, vreinterpret_s16_s64(v_eobmin_xxx3));
  return (uint16_t)vget_lane_s16(v_eobmin_final, 0);
#endif
}
#endif  // SKIP_EOB_FACTOR_ADJUST && !CONFIG_REALTIME_ONLY

static void highbd_quantize_b_neon(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan, const int log_scale) {
  (void)scan;
  const int16x4_t v_quant = vld1_s16(quant_ptr);
  const int16x4_t v_dequant = vld1_s16(dequant_ptr);
  const int16x4_t v_zero = vdup_n_s16(0);
  const uint16x4_t v_round_select = vcgt_s16(vdup_n_s16(log_scale), v_zero);
  const int16x4_t v_round_no_scale = vld1_s16(round_ptr);
  const int16x4_t v_round_log_scale =
      vqrdmulh_n_s16(v_round_no_scale, (int16_t)(1 << (15 - log_scale)));
  const int16x4_t v_round =
      vbsl_s16(v_round_select, v_round_log_scale, v_round_no_scale);
  const int16x4_t v_quant_shift = vld1_s16(quant_shift_ptr);
  const int16x4_t v_zbin_no_scale = vld1_s16(zbin_ptr);
  const int16x4_t v_zbin_log_scale =
      vqrdmulh_n_s16(v_zbin_no_scale, (int16_t)(1 << (15 - log_scale)));
  const int16x4_t v_zbin =
      vbsl_s16(v_round_select, v_zbin_log_scale, v_zbin_no_scale);
  int32x4_t v_round_s32 = vmovl_s16(v_round);
  int32x4_t v_quant_s32 = vshlq_n_s32(vmovl_s16(v_quant), 15);
  int32x4_t v_dequant_s32 = vmovl_s16(v_dequant);
  int32x4_t v_quant_shift_s32 = vshlq_n_s32(vmovl_s16(v_quant_shift), 15);
  int32x4_t v_zbin_s32 = vmovl_s16(v_zbin);
  uint16x4_t v_mask_lo, v_mask_hi;
  int16x8_t v_eobmax = vdupq_n_s16(-1);

  intptr_t non_zero_count = n_coeffs;

  assert(n_coeffs > 8);
  // Pre-scan pass
  const int32x4_t v_zbin_s32x = vdupq_lane_s32(vget_low_s32(v_zbin_s32), 1);
  intptr_t i = n_coeffs;
  do {
    const int32x4_t v_coeff_a = vld1q_s32(coeff_ptr + i - 4);
    const int32x4_t v_coeff_b = vld1q_s32(coeff_ptr + i - 8);
    const int32x4_t v_abs_coeff_a = vabsq_s32(v_coeff_a);
    const int32x4_t v_abs_coeff_b = vabsq_s32(v_coeff_b);
    const uint32x4_t v_mask_a = vcgeq_s32(v_abs_coeff_a, v_zbin_s32x);
    const uint32x4_t v_mask_b = vcgeq_s32(v_abs_coeff_b, v_zbin_s32x);
    // If the coefficient is in the base ZBIN range, then discard.
    if (sum_abs_coeff(v_mask_a) + sum_abs_coeff(v_mask_b) == 0) {
      non_zero_count -= 8;
    } else {
      break;
    }
    i -= 8;
  } while (i > 0);

  const intptr_t remaining_zcoeffs = n_coeffs - non_zero_count;
  memset(qcoeff_ptr + non_zero_count, 0,
         remaining_zcoeffs * sizeof(*qcoeff_ptr));
  memset(dqcoeff_ptr + non_zero_count, 0,
         remaining_zcoeffs * sizeof(*dqcoeff_ptr));

  // DC and first 3 AC
  v_mask_lo =
      quantize_4(coeff_ptr, qcoeff_ptr, dqcoeff_ptr, v_quant_s32, v_dequant_s32,
                 v_round_s32, v_zbin_s32, v_quant_shift_s32, log_scale);

  // overwrite the DC constants with AC constants
  v_round_s32 = vdupq_lane_s32(vget_low_s32(v_round_s32), 1);
  v_quant_s32 = vdupq_lane_s32(vget_low_s32(v_quant_s32), 1);
  v_dequant_s32 = vdupq_lane_s32(vget_low_s32(v_dequant_s32), 1);
  v_quant_shift_s32 = vdupq_lane_s32(vget_low_s32(v_quant_shift_s32), 1);
  v_zbin_s32 = vdupq_lane_s32(vget_low_s32(v_zbin_s32), 1);

  // 4 more AC
  v_mask_hi = quantize_4(coeff_ptr + 4, qcoeff_ptr + 4, dqcoeff_ptr + 4,
                         v_quant_s32, v_dequant_s32, v_round_s32, v_zbin_s32,
                         v_quant_shift_s32, log_scale);

  v_eobmax =
      get_max_lane_eob(iscan, v_eobmax, vcombine_u16(v_mask_lo, v_mask_hi));

  intptr_t count = non_zero_count - 8;
  for (; count > 0; count -= 8) {
    coeff_ptr += 8;
    qcoeff_ptr += 8;
    dqcoeff_ptr += 8;
    iscan += 8;
    v_mask_lo = quantize_4(coeff_ptr, qcoeff_ptr, dqcoeff_ptr, v_quant_s32,
                           v_dequant_s32, v_round_s32, v_zbin_s32,
                           v_quant_shift_s32, log_scale);
    v_mask_hi = quantize_4(coeff_ptr + 4, qcoeff_ptr + 4, dqcoeff_ptr + 4,
                           v_quant_s32, v_dequant_s32, v_round_s32, v_zbin_s32,
                           v_quant_shift_s32, log_scale);
    // Find the max lane eob for 8 coeffs.
    v_eobmax =
        get_max_lane_eob(iscan, v_eobmax, vcombine_u16(v_mask_lo, v_mask_hi));
  }

  *eob_ptr = get_max_eob(v_eobmax);
}

void aom_highbd_quantize_b_neon(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                                const int16_t *zbin_ptr,
                                const int16_t *round_ptr,
                                const int16_t *quant_ptr,
                                const int16_t *quant_shift_ptr,
                                tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                                const int16_t *dequant_ptr, uint16_t *eob_ptr,
                                const int16_t *scan, const int16_t *iscan) {
  highbd_quantize_b_neon(coeff_ptr, n_coeffs, zbin_ptr, round_ptr, quant_ptr,
                         quant_shift_ptr, qcoeff_ptr, dqcoeff_ptr, dequant_ptr,
                         eob_ptr, scan, iscan, 0);
}

void aom_highbd_quantize_b_32x32_neon(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan) {
  highbd_quantize_b_neon(coeff_ptr, n_coeffs, zbin_ptr, round_ptr, quant_ptr,
                         quant_shift_ptr, qcoeff_ptr, dqcoeff_ptr, dequant_ptr,
                         eob_ptr, scan, iscan, 1);
}

void aom_highbd_quantize_b_64x64_neon(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan) {
  highbd_quantize_b_neon(coeff_ptr, n_coeffs, zbin_ptr, round_ptr, quant_ptr,
                         quant_shift_ptr, qcoeff_ptr, dqcoeff_ptr, dequant_ptr,
                         eob_ptr, scan, iscan, 2);
}

#if !CONFIG_REALTIME_ONLY
static void highbd_quantize_b_adaptive_neon(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan, const int log_scale) {
  (void)scan;
  const int16x4_t v_quant = vld1_s16(quant_ptr);
  const int16x4_t v_dequant = vld1_s16(dequant_ptr);
  const int16x4_t v_zero = vdup_n_s16(0);
  const uint16x4_t v_round_select = vcgt_s16(vdup_n_s16(log_scale), v_zero);
  const int16x4_t v_round_no_scale = vld1_s16(round_ptr);
  const int16x4_t v_round_log_scale =
      vqrdmulh_n_s16(v_round_no_scale, (int16_t)(1 << (15 - log_scale)));
  const int16x4_t v_round =
      vbsl_s16(v_round_select, v_round_log_scale, v_round_no_scale);
  const int16x4_t v_quant_shift = vld1_s16(quant_shift_ptr);
  const int16x4_t v_zbin_no_scale = vld1_s16(zbin_ptr);
  const int16x4_t v_zbin_log_scale =
      vqrdmulh_n_s16(v_zbin_no_scale, (int16_t)(1 << (15 - log_scale)));
  const int16x4_t v_zbin =
      vbsl_s16(v_round_select, v_zbin_log_scale, v_zbin_no_scale);
  int32x4_t v_round_s32 = vmovl_s16(v_round);
  int32x4_t v_quant_s32 = vshlq_n_s32(vmovl_s16(v_quant), 15);
  int32x4_t v_dequant_s32 = vmovl_s16(v_dequant);
  int32x4_t v_quant_shift_s32 = vshlq_n_s32(vmovl_s16(v_quant_shift), 15);
  int32x4_t v_zbin_s32 = vmovl_s16(v_zbin);
  uint16x4_t v_mask_lo, v_mask_hi;
  int16x8_t v_eobmax = vdupq_n_s16(-1);
  int16x8_t v_eobmin = vdupq_n_s16((int16_t)n_coeffs);

  assert(n_coeffs > 8);
  // Pre-scan pass
  const int32x4_t v_zbin_s32x = vdupq_lane_s32(vget_low_s32(v_zbin_s32), 1);
  const int prescan_add_1 =
      ROUND_POWER_OF_TWO(dequant_ptr[1] * EOB_FACTOR, 7 + AOM_QM_BITS);
  const int32x4_t v_zbin_prescan =
      vaddq_s32(v_zbin_s32x, vdupq_n_s32(prescan_add_1));
  intptr_t non_zero_count = n_coeffs;
  intptr_t i = n_coeffs;
  do {
    const int32x4_t v_coeff_a = vld1q_s32(coeff_ptr + i - 4);
    const int32x4_t v_coeff_b = vld1q_s32(coeff_ptr + i - 8);
    const int32x4_t v_abs_coeff_a = vabsq_s32(v_coeff_a);
    const int32x4_t v_abs_coeff_b = vabsq_s32(v_coeff_b);
    const uint32x4_t v_mask_a = vcgeq_s32(v_abs_coeff_a, v_zbin_prescan);
    const uint32x4_t v_mask_b = vcgeq_s32(v_abs_coeff_b, v_zbin_prescan);
    // If the coefficient is in the base ZBIN range, then discard.
    if (sum_abs_coeff(v_mask_a) + sum_abs_coeff(v_mask_b) == 0) {
      non_zero_count -= 8;
    } else {
      break;
    }
    i -= 8;
  } while (i > 0);

  const intptr_t remaining_zcoeffs = n_coeffs - non_zero_count;
  memset(qcoeff_ptr + non_zero_count, 0,
         remaining_zcoeffs * sizeof(*qcoeff_ptr));
  memset(dqcoeff_ptr + non_zero_count, 0,
         remaining_zcoeffs * sizeof(*dqcoeff_ptr));

  // DC and first 3 AC
  v_mask_lo =
      quantize_4(coeff_ptr, qcoeff_ptr, dqcoeff_ptr, v_quant_s32, v_dequant_s32,
                 v_round_s32, v_zbin_s32, v_quant_shift_s32, log_scale);

  // overwrite the DC constants with AC constants
  v_round_s32 = vdupq_lane_s32(vget_low_s32(v_round_s32), 1);
  v_quant_s32 = vdupq_lane_s32(vget_low_s32(v_quant_s32), 1);
  v_dequant_s32 = vdupq_lane_s32(vget_low_s32(v_dequant_s32), 1);
  v_quant_shift_s32 = vdupq_lane_s32(vget_low_s32(v_quant_shift_s32), 1);
  v_zbin_s32 = vdupq_lane_s32(vget_low_s32(v_zbin_s32), 1);

  // 4 more AC
  v_mask_hi = quantize_4(coeff_ptr + 4, qcoeff_ptr + 4, dqcoeff_ptr + 4,
                         v_quant_s32, v_dequant_s32, v_round_s32, v_zbin_s32,
                         v_quant_shift_s32, log_scale);

  get_min_max_lane_eob(iscan, &v_eobmin, &v_eobmax,
                       vcombine_u16(v_mask_lo, v_mask_hi), n_coeffs);

  intptr_t count = non_zero_count - 8;
  for (; count > 0; count -= 8) {
    coeff_ptr += 8;
    qcoeff_ptr += 8;
    dqcoeff_ptr += 8;
    iscan += 8;
    v_mask_lo = quantize_4(coeff_ptr, qcoeff_ptr, dqcoeff_ptr, v_quant_s32,
                           v_dequant_s32, v_round_s32, v_zbin_s32,
                           v_quant_shift_s32, log_scale);
    v_mask_hi = quantize_4(coeff_ptr + 4, qcoeff_ptr + 4, dqcoeff_ptr + 4,
                           v_quant_s32, v_dequant_s32, v_round_s32, v_zbin_s32,
                           v_quant_shift_s32, log_scale);

    get_min_max_lane_eob(iscan, &v_eobmin, &v_eobmax,
                         vcombine_u16(v_mask_lo, v_mask_hi), n_coeffs);
  }

  int eob = get_max_eob(v_eobmax);

#if SKIP_EOB_FACTOR_ADJUST
  const int first = get_min_eob(v_eobmin);
  if (eob >= 0 && first == eob) {
    const int rc = scan[eob];
    if (qcoeff_ptr[rc] == 1 || qcoeff_ptr[rc] == -1) {
      const int zbins[2] = { ROUND_POWER_OF_TWO(zbin_ptr[0], log_scale),
                             ROUND_POWER_OF_TWO(zbin_ptr[1], log_scale) };
      const int nzbins[2] = { zbins[0] * -1, zbins[1] * -1 };
      const qm_val_t wt = (1 << AOM_QM_BITS);
      const int coeff = coeff_ptr[rc] * wt;
      const int factor = EOB_FACTOR + SKIP_EOB_FACTOR_ADJUST;
      const int prescan_add_val =
          ROUND_POWER_OF_TWO(dequant_ptr[rc != 0] * factor, 7);
      if (coeff < (zbins[rc != 0] * (1 << AOM_QM_BITS) + prescan_add_val) &&
          coeff > (nzbins[rc != 0] * (1 << AOM_QM_BITS) - prescan_add_val)) {
        qcoeff_ptr[rc] = 0;
        dqcoeff_ptr[rc] = 0;
        eob = -1;
      }
    }
  }
#endif  // SKIP_EOB_FACTOR_ADJUST
  *eob_ptr = eob + 1;
}

void aom_highbd_quantize_b_adaptive_neon(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan) {
  highbd_quantize_b_adaptive_neon(
      coeff_ptr, n_coeffs, zbin_ptr, round_ptr, quant_ptr, quant_shift_ptr,
      qcoeff_ptr, dqcoeff_ptr, dequant_ptr, eob_ptr, scan, iscan, 0);
}

void aom_highbd_quantize_b_32x32_adaptive_neon(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan) {
  highbd_quantize_b_adaptive_neon(
      coeff_ptr, n_coeffs, zbin_ptr, round_ptr, quant_ptr, quant_shift_ptr,
      qcoeff_ptr, dqcoeff_ptr, dequant_ptr, eob_ptr, scan, iscan, 1);
}

void aom_highbd_quantize_b_64x64_adaptive_neon(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan) {
  highbd_quantize_b_adaptive_neon(
      coeff_ptr, n_coeffs, zbin_ptr, round_ptr, quant_ptr, quant_shift_ptr,
      qcoeff_ptr, dqcoeff_ptr, dequant_ptr, eob_ptr, scan, iscan, 2);
}
#endif  // !CONFIG_REALTIME_ONLY
