/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <emmintrin.h>
#include "config/aom_dsp_rtcd.h"
#include "aom/aom_integer.h"
#include "aom_dsp/quantize.h"
#include "aom_dsp/x86/quantize_x86.h"

void aom_quantize_b_adaptive_sse2(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan) {
  int index = 16;
  int non_zero_count = 0;
  int non_zero_count_prescan_add_zero = 0;
  int is_found0 = 0, is_found1 = 0;
  int eob = -1;
  const __m128i zero = _mm_setzero_si128();
  __m128i zbin, round, quant, dequant, shift;
  __m128i coeff0, coeff1, coeff0_sign, coeff1_sign;
  __m128i qcoeff0, qcoeff1;
  __m128i cmp_mask0, cmp_mask1;
  __m128i all_zero;
  __m128i mask0 = zero, mask1 = zero;

  int prescan_add[2];
  int thresh[4];
  const qm_val_t wt = (1 << AOM_QM_BITS);
  for (int i = 0; i < 2; ++i) {
    prescan_add[i] = ROUND_POWER_OF_TWO(dequant_ptr[i] * EOB_FACTOR, 7);
    thresh[i] = (zbin_ptr[i] * wt + prescan_add[i]) - 1;
  }
  thresh[2] = thresh[3] = thresh[1];
  __m128i threshold[2];
  threshold[0] = _mm_loadu_si128((__m128i *)&thresh[0]);
  threshold[1] = _mm_unpackhi_epi64(threshold[0], threshold[0]);

#if SKIP_EOB_FACTOR_ADJUST
  int first = -1;
#endif
  // Setup global values.
  load_b_values(zbin_ptr, &zbin, round_ptr, &round, quant_ptr, &quant,
                dequant_ptr, &dequant, quant_shift_ptr, &shift);

  // Do DC and first 15 AC.
  coeff0 = load_coefficients(coeff_ptr);
  coeff1 = load_coefficients(coeff_ptr + 8);

  // Poor man's abs().
  coeff0_sign = _mm_srai_epi16(coeff0, 15);
  coeff1_sign = _mm_srai_epi16(coeff1, 15);
  qcoeff0 = invert_sign_sse2(coeff0, coeff0_sign);
  qcoeff1 = invert_sign_sse2(coeff1, coeff1_sign);

  update_mask0(&qcoeff0, &qcoeff1, threshold, iscan, &is_found0, &mask0);

  cmp_mask0 = _mm_cmpgt_epi16(qcoeff0, zbin);
  zbin = _mm_unpackhi_epi64(zbin, zbin);  // Switch DC to AC
  cmp_mask1 = _mm_cmpgt_epi16(qcoeff1, zbin);

  update_mask1(&cmp_mask0, &cmp_mask1, iscan, &is_found1, &mask1);

  threshold[0] = threshold[1];
  all_zero = _mm_or_si128(cmp_mask0, cmp_mask1);
  if (_mm_movemask_epi8(all_zero) == 0) {
    _mm_store_si128((__m128i *)(qcoeff_ptr), zero);
    _mm_store_si128((__m128i *)(qcoeff_ptr + 4), zero);
    _mm_store_si128((__m128i *)(qcoeff_ptr + 8), zero);
    _mm_store_si128((__m128i *)(qcoeff_ptr + 12), zero);
    _mm_store_si128((__m128i *)(dqcoeff_ptr), zero);
    _mm_store_si128((__m128i *)(dqcoeff_ptr + 4), zero);
    _mm_store_si128((__m128i *)(dqcoeff_ptr + 8), zero);
    _mm_store_si128((__m128i *)(dqcoeff_ptr + 12), zero);
    round = _mm_unpackhi_epi64(round, round);
    quant = _mm_unpackhi_epi64(quant, quant);
    shift = _mm_unpackhi_epi64(shift, shift);
    dequant = _mm_unpackhi_epi64(dequant, dequant);
  } else {
    calculate_qcoeff(&qcoeff0, round, quant, shift);

    round = _mm_unpackhi_epi64(round, round);
    quant = _mm_unpackhi_epi64(quant, quant);
    shift = _mm_unpackhi_epi64(shift, shift);

    calculate_qcoeff(&qcoeff1, round, quant, shift);

    // Reinsert signs
    qcoeff0 = invert_sign_sse2(qcoeff0, coeff0_sign);
    qcoeff1 = invert_sign_sse2(qcoeff1, coeff1_sign);

    // Mask out zbin threshold coeffs
    qcoeff0 = _mm_and_si128(qcoeff0, cmp_mask0);
    qcoeff1 = _mm_and_si128(qcoeff1, cmp_mask1);

    store_coefficients(qcoeff0, qcoeff_ptr);
    store_coefficients(qcoeff1, qcoeff_ptr + 8);

    coeff0 = calculate_dqcoeff(qcoeff0, dequant);
    dequant = _mm_unpackhi_epi64(dequant, dequant);
    coeff1 = calculate_dqcoeff(qcoeff1, dequant);

    store_coefficients(coeff0, dqcoeff_ptr);
    store_coefficients(coeff1, dqcoeff_ptr + 8);
  }

  // AC only loop.
  while (index < n_coeffs) {
    coeff0 = load_coefficients(coeff_ptr + index);
    coeff1 = load_coefficients(coeff_ptr + index + 8);

    coeff0_sign = _mm_srai_epi16(coeff0, 15);
    coeff1_sign = _mm_srai_epi16(coeff1, 15);
    qcoeff0 = invert_sign_sse2(coeff0, coeff0_sign);
    qcoeff1 = invert_sign_sse2(coeff1, coeff1_sign);

    update_mask0(&qcoeff0, &qcoeff1, threshold, iscan + index, &is_found0,
                 &mask0);

    cmp_mask0 = _mm_cmpgt_epi16(qcoeff0, zbin);
    cmp_mask1 = _mm_cmpgt_epi16(qcoeff1, zbin);

    update_mask1(&cmp_mask0, &cmp_mask1, iscan + index, &is_found1, &mask1);

    all_zero = _mm_or_si128(cmp_mask0, cmp_mask1);
    if (_mm_movemask_epi8(all_zero) == 0) {
      _mm_store_si128((__m128i *)(qcoeff_ptr + index), zero);
      _mm_store_si128((__m128i *)(qcoeff_ptr + index + 4), zero);
      _mm_store_si128((__m128i *)(qcoeff_ptr + index + 8), zero);
      _mm_store_si128((__m128i *)(qcoeff_ptr + index + 12), zero);
      _mm_store_si128((__m128i *)(dqcoeff_ptr + index), zero);
      _mm_store_si128((__m128i *)(dqcoeff_ptr + index + 4), zero);
      _mm_store_si128((__m128i *)(dqcoeff_ptr + index + 8), zero);
      _mm_store_si128((__m128i *)(dqcoeff_ptr + index + 12), zero);
      index += 16;
      continue;
    }
    calculate_qcoeff(&qcoeff0, round, quant, shift);
    calculate_qcoeff(&qcoeff1, round, quant, shift);

    qcoeff0 = invert_sign_sse2(qcoeff0, coeff0_sign);
    qcoeff1 = invert_sign_sse2(qcoeff1, coeff1_sign);

    qcoeff0 = _mm_and_si128(qcoeff0, cmp_mask0);
    qcoeff1 = _mm_and_si128(qcoeff1, cmp_mask1);

    store_coefficients(qcoeff0, qcoeff_ptr + index);
    store_coefficients(qcoeff1, qcoeff_ptr + index + 8);

    coeff0 = calculate_dqcoeff(qcoeff0, dequant);
    coeff1 = calculate_dqcoeff(qcoeff1, dequant);

    store_coefficients(coeff0, dqcoeff_ptr + index);
    store_coefficients(coeff1, dqcoeff_ptr + index + 8);

    index += 16;
  }
  if (is_found0) non_zero_count = calculate_non_zero_count(mask0);
  if (is_found1)
    non_zero_count_prescan_add_zero = calculate_non_zero_count(mask1);

  for (int i = non_zero_count_prescan_add_zero - 1; i >= non_zero_count; i--) {
    const int rc = scan[i];
    qcoeff_ptr[rc] = 0;
    dqcoeff_ptr[rc] = 0;
  }

  for (int i = non_zero_count - 1; i >= 0; i--) {
    const int rc = scan[i];
    if (qcoeff_ptr[rc]) {
      eob = i;
      break;
    }
  }

  *eob_ptr = eob + 1;
#if SKIP_EOB_FACTOR_ADJUST
  // TODO(Aniket): Experiment the following loop with intrinsic by combining
  // with the quantization loop above
  for (int i = 0; i < non_zero_count; i++) {
    const int rc = scan[i];
    const int qcoeff = qcoeff_ptr[rc];
    if (qcoeff) {
      first = i;
      break;
    }
  }
  if ((*eob_ptr - 1) >= 0 && first == (*eob_ptr - 1)) {
    const int rc = scan[(*eob_ptr - 1)];
    if (qcoeff_ptr[rc] == 1 || qcoeff_ptr[rc] == -1) {
      const int coeff = coeff_ptr[rc] * wt;
      const int coeff_sign = AOMSIGN(coeff);
      const int abs_coeff = (coeff ^ coeff_sign) - coeff_sign;
      const int factor = EOB_FACTOR + SKIP_EOB_FACTOR_ADJUST;
      const int prescan_add_val =
          ROUND_POWER_OF_TWO(dequant_ptr[rc != 0] * factor, 7);
      if (abs_coeff <
          (zbin_ptr[rc != 0] * (1 << AOM_QM_BITS) + prescan_add_val)) {
        qcoeff_ptr[rc] = 0;
        dqcoeff_ptr[rc] = 0;
        *eob_ptr = 0;
      }
    }
  }
#endif
}

void aom_quantize_b_32x32_adaptive_sse2(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan) {
  int index = 16;
  const int log_scale = 1;
  int non_zero_count = 0;
  int non_zero_count_prescan_add_zero = 0;
  int is_found0 = 0, is_found1 = 0;
  int eob = -1;
  const __m128i zero = _mm_setzero_si128();
  const __m128i one = _mm_set1_epi16(1);
  const __m128i log_scale_vec = _mm_set1_epi16(log_scale);
  __m128i zbin, round, quant, dequant, shift;
  __m128i coeff0, coeff1, coeff0_sign, coeff1_sign;
  __m128i qcoeff0, qcoeff1;
  __m128i cmp_mask0, cmp_mask1;
  __m128i all_zero;
  __m128i mask0 = zero, mask1 = zero;

  const int zbins[2] = { ROUND_POWER_OF_TWO(zbin_ptr[0], log_scale),
                         ROUND_POWER_OF_TWO(zbin_ptr[1], log_scale) };
  int prescan_add[2];
  int thresh[4];
  const qm_val_t wt = (1 << AOM_QM_BITS);
  for (int i = 0; i < 2; ++i) {
    prescan_add[i] = ROUND_POWER_OF_TWO(dequant_ptr[i] * EOB_FACTOR, 7);
    thresh[i] = (zbins[i] * wt + prescan_add[i]) - 1;
  }
  thresh[2] = thresh[3] = thresh[1];
  __m128i threshold[2];
  threshold[0] = _mm_loadu_si128((__m128i *)&thresh[0]);
  threshold[1] = _mm_unpackhi_epi64(threshold[0], threshold[0]);

#if SKIP_EOB_FACTOR_ADJUST
  int first = -1;
#endif
  // Setup global values.
  zbin = _mm_load_si128((const __m128i *)zbin_ptr);
  round = _mm_load_si128((const __m128i *)round_ptr);
  quant = _mm_load_si128((const __m128i *)quant_ptr);
  dequant = _mm_load_si128((const __m128i *)dequant_ptr);
  shift = _mm_load_si128((const __m128i *)quant_shift_ptr);

  // Shift with rounding.
  zbin = _mm_add_epi16(zbin, log_scale_vec);
  round = _mm_add_epi16(round, log_scale_vec);
  zbin = _mm_srli_epi16(zbin, log_scale);
  round = _mm_srli_epi16(round, log_scale);
  zbin = _mm_sub_epi16(zbin, one);

  // Do DC and first 15 AC.
  coeff0 = load_coefficients(coeff_ptr);
  coeff1 = load_coefficients(coeff_ptr + 8);

  coeff0_sign = _mm_srai_epi16(coeff0, 15);
  coeff1_sign = _mm_srai_epi16(coeff1, 15);
  qcoeff0 = invert_sign_sse2(coeff0, coeff0_sign);
  qcoeff1 = invert_sign_sse2(coeff1, coeff1_sign);

  update_mask0(&qcoeff0, &qcoeff1, threshold, iscan, &is_found0, &mask0);

  cmp_mask0 = _mm_cmpgt_epi16(qcoeff0, zbin);
  zbin = _mm_unpackhi_epi64(zbin, zbin);  // Switch DC to AC
  cmp_mask1 = _mm_cmpgt_epi16(qcoeff1, zbin);

  update_mask1(&cmp_mask0, &cmp_mask1, iscan, &is_found1, &mask1);

  threshold[0] = threshold[1];
  all_zero = _mm_or_si128(cmp_mask0, cmp_mask1);
  if (_mm_movemask_epi8(all_zero) == 0) {
    _mm_store_si128((__m128i *)(qcoeff_ptr), zero);
    _mm_store_si128((__m128i *)(qcoeff_ptr + 4), zero);
    _mm_store_si128((__m128i *)(qcoeff_ptr + 8), zero);
    _mm_store_si128((__m128i *)(qcoeff_ptr + 12), zero);
    _mm_store_si128((__m128i *)(dqcoeff_ptr), zero);
    _mm_store_si128((__m128i *)(dqcoeff_ptr + 4), zero);
    _mm_store_si128((__m128i *)(dqcoeff_ptr + 8), zero);
    _mm_store_si128((__m128i *)(dqcoeff_ptr + 12), zero);
    round = _mm_unpackhi_epi64(round, round);
    quant = _mm_unpackhi_epi64(quant, quant);
    shift = _mm_unpackhi_epi64(shift, shift);
    dequant = _mm_unpackhi_epi64(dequant, dequant);
  } else {
    calculate_qcoeff_log_scale(&qcoeff0, round, quant, &shift, &log_scale);
    round = _mm_unpackhi_epi64(round, round);
    quant = _mm_unpackhi_epi64(quant, quant);
    shift = _mm_unpackhi_epi64(shift, shift);
    calculate_qcoeff_log_scale(&qcoeff1, round, quant, &shift, &log_scale);

    // Reinsert signs
    qcoeff0 = invert_sign_sse2(qcoeff0, coeff0_sign);
    qcoeff1 = invert_sign_sse2(qcoeff1, coeff1_sign);

    // Mask out zbin threshold coeffs
    qcoeff0 = _mm_and_si128(qcoeff0, cmp_mask0);
    qcoeff1 = _mm_and_si128(qcoeff1, cmp_mask1);

    store_coefficients(qcoeff0, qcoeff_ptr);
    store_coefficients(qcoeff1, qcoeff_ptr + 8);

    calculate_dqcoeff_and_store_log_scale(qcoeff0, dequant, zero, dqcoeff_ptr,
                                          &log_scale);
    dequant = _mm_unpackhi_epi64(dequant, dequant);
    calculate_dqcoeff_and_store_log_scale(qcoeff1, dequant, zero,
                                          dqcoeff_ptr + 8, &log_scale);
  }

  // AC only loop.
  while (index < n_coeffs) {
    coeff0 = load_coefficients(coeff_ptr + index);
    coeff1 = load_coefficients(coeff_ptr + index + 8);

    coeff0_sign = _mm_srai_epi16(coeff0, 15);
    coeff1_sign = _mm_srai_epi16(coeff1, 15);
    qcoeff0 = invert_sign_sse2(coeff0, coeff0_sign);
    qcoeff1 = invert_sign_sse2(coeff1, coeff1_sign);

    update_mask0(&qcoeff0, &qcoeff1, threshold, iscan + index, &is_found0,
                 &mask0);

    cmp_mask0 = _mm_cmpgt_epi16(qcoeff0, zbin);
    cmp_mask1 = _mm_cmpgt_epi16(qcoeff1, zbin);

    update_mask1(&cmp_mask0, &cmp_mask1, iscan + index, &is_found1, &mask1);

    all_zero = _mm_or_si128(cmp_mask0, cmp_mask1);
    if (_mm_movemask_epi8(all_zero) == 0) {
      _mm_store_si128((__m128i *)(qcoeff_ptr + index), zero);
      _mm_store_si128((__m128i *)(qcoeff_ptr + index + 4), zero);
      _mm_store_si128((__m128i *)(qcoeff_ptr + index + 8), zero);
      _mm_store_si128((__m128i *)(qcoeff_ptr + index + 12), zero);
      _mm_store_si128((__m128i *)(dqcoeff_ptr + index), zero);
      _mm_store_si128((__m128i *)(dqcoeff_ptr + index + 4), zero);
      _mm_store_si128((__m128i *)(dqcoeff_ptr + index + 8), zero);
      _mm_store_si128((__m128i *)(dqcoeff_ptr + index + 12), zero);
      index += 16;
      continue;
    }
    calculate_qcoeff_log_scale(&qcoeff0, round, quant, &shift, &log_scale);
    calculate_qcoeff_log_scale(&qcoeff1, round, quant, &shift, &log_scale);

    qcoeff0 = invert_sign_sse2(qcoeff0, coeff0_sign);
    qcoeff1 = invert_sign_sse2(qcoeff1, coeff1_sign);

    qcoeff0 = _mm_and_si128(qcoeff0, cmp_mask0);
    qcoeff1 = _mm_and_si128(qcoeff1, cmp_mask1);

    store_coefficients(qcoeff0, qcoeff_ptr + index);
    store_coefficients(qcoeff1, qcoeff_ptr + index + 8);

    calculate_dqcoeff_and_store_log_scale(qcoeff0, dequant, zero,
                                          dqcoeff_ptr + index, &log_scale);
    calculate_dqcoeff_and_store_log_scale(qcoeff1, dequant, zero,
                                          dqcoeff_ptr + index + 8, &log_scale);
    index += 16;
  }
  if (is_found0) non_zero_count = calculate_non_zero_count(mask0);
  if (is_found1)
    non_zero_count_prescan_add_zero = calculate_non_zero_count(mask1);

  for (int i = non_zero_count_prescan_add_zero - 1; i >= non_zero_count; i--) {
    const int rc = scan[i];
    qcoeff_ptr[rc] = 0;
    dqcoeff_ptr[rc] = 0;
  }

  for (int i = non_zero_count - 1; i >= 0; i--) {
    const int rc = scan[i];
    if (qcoeff_ptr[rc]) {
      eob = i;
      break;
    }
  }

  *eob_ptr = eob + 1;
#if SKIP_EOB_FACTOR_ADJUST
  // TODO(Aniket): Experiment the following loop with intrinsic by combining
  // with the quantization loop above
  for (int i = 0; i < non_zero_count; i++) {
    const int rc = scan[i];
    const int qcoeff = qcoeff_ptr[rc];
    if (qcoeff) {
      first = i;
      break;
    }
  }
  if ((*eob_ptr - 1) >= 0 && first == (*eob_ptr - 1)) {
    const int rc = scan[(*eob_ptr - 1)];
    if (qcoeff_ptr[rc] == 1 || qcoeff_ptr[rc] == -1) {
      const int coeff = coeff_ptr[rc] * wt;
      const int coeff_sign = AOMSIGN(coeff);
      const int abs_coeff = (coeff ^ coeff_sign) - coeff_sign;
      const int factor = EOB_FACTOR + SKIP_EOB_FACTOR_ADJUST;
      const int prescan_add_val =
          ROUND_POWER_OF_TWO(dequant_ptr[rc != 0] * factor, 7);
      if (abs_coeff < (zbins[rc != 0] * (1 << AOM_QM_BITS) + prescan_add_val)) {
        qcoeff_ptr[rc] = 0;
        dqcoeff_ptr[rc] = 0;
        *eob_ptr = 0;
      }
    }
  }
#endif
}

void aom_quantize_b_64x64_adaptive_sse2(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan) {
  int index = 16;
  const int log_scale = 2;
  int non_zero_count = 0;
  int non_zero_count_prescan_add_zero = 0;
  int is_found0 = 0, is_found1 = 0;
  int eob = -1;
  const __m128i zero = _mm_setzero_si128();
  const __m128i one = _mm_set1_epi16(1);
  const __m128i log_scale_vec = _mm_set1_epi16(log_scale);
  __m128i zbin, round, quant, dequant, shift;
  __m128i coeff0, coeff1, coeff0_sign, coeff1_sign;
  __m128i qcoeff0, qcoeff1;
  __m128i cmp_mask0, cmp_mask1;
  __m128i all_zero;
  __m128i mask0 = zero, mask1 = zero;

  const int zbins[2] = { ROUND_POWER_OF_TWO(zbin_ptr[0], log_scale),
                         ROUND_POWER_OF_TWO(zbin_ptr[1], log_scale) };
  int prescan_add[2];
  int thresh[4];
  const qm_val_t wt = (1 << AOM_QM_BITS);
  for (int i = 0; i < 2; ++i) {
    prescan_add[i] = ROUND_POWER_OF_TWO(dequant_ptr[i] * EOB_FACTOR, 7);
    thresh[i] = (zbins[i] * wt + prescan_add[i]) - 1;
  }
  thresh[2] = thresh[3] = thresh[1];
  __m128i threshold[2];
  threshold[0] = _mm_loadu_si128((__m128i *)&thresh[0]);
  threshold[1] = _mm_unpackhi_epi64(threshold[0], threshold[0]);

#if SKIP_EOB_FACTOR_ADJUST
  int first = -1;
#endif
  // Setup global values.
  zbin = _mm_load_si128((const __m128i *)zbin_ptr);
  round = _mm_load_si128((const __m128i *)round_ptr);
  quant = _mm_load_si128((const __m128i *)quant_ptr);
  dequant = _mm_load_si128((const __m128i *)dequant_ptr);
  shift = _mm_load_si128((const __m128i *)quant_shift_ptr);

  // Shift with rounding.
  zbin = _mm_add_epi16(zbin, log_scale_vec);
  round = _mm_add_epi16(round, log_scale_vec);
  zbin = _mm_srli_epi16(zbin, log_scale);
  round = _mm_srli_epi16(round, log_scale);
  zbin = _mm_sub_epi16(zbin, one);

  // Do DC and first 15 AC.
  coeff0 = load_coefficients(coeff_ptr);
  coeff1 = load_coefficients(coeff_ptr + 8);

  coeff0_sign = _mm_srai_epi16(coeff0, 15);
  coeff1_sign = _mm_srai_epi16(coeff1, 15);
  qcoeff0 = invert_sign_sse2(coeff0, coeff0_sign);
  qcoeff1 = invert_sign_sse2(coeff1, coeff1_sign);

  update_mask0(&qcoeff0, &qcoeff1, threshold, iscan, &is_found0, &mask0);

  cmp_mask0 = _mm_cmpgt_epi16(qcoeff0, zbin);
  zbin = _mm_unpackhi_epi64(zbin, zbin);  // Switch DC to AC
  cmp_mask1 = _mm_cmpgt_epi16(qcoeff1, zbin);

  update_mask1(&cmp_mask0, &cmp_mask1, iscan, &is_found1, &mask1);

  threshold[0] = threshold[1];
  all_zero = _mm_or_si128(cmp_mask0, cmp_mask1);
  if (_mm_movemask_epi8(all_zero) == 0) {
    _mm_store_si128((__m128i *)(qcoeff_ptr), zero);
    _mm_store_si128((__m128i *)(qcoeff_ptr + 4), zero);
    _mm_store_si128((__m128i *)(qcoeff_ptr + 8), zero);
    _mm_store_si128((__m128i *)(qcoeff_ptr + 12), zero);
    _mm_store_si128((__m128i *)(dqcoeff_ptr), zero);
    _mm_store_si128((__m128i *)(dqcoeff_ptr + 4), zero);
    _mm_store_si128((__m128i *)(dqcoeff_ptr + 8), zero);
    _mm_store_si128((__m128i *)(dqcoeff_ptr + 12), zero);
    round = _mm_unpackhi_epi64(round, round);
    quant = _mm_unpackhi_epi64(quant, quant);
    shift = _mm_unpackhi_epi64(shift, shift);
    dequant = _mm_unpackhi_epi64(dequant, dequant);
  } else {
    calculate_qcoeff_log_scale(&qcoeff0, round, quant, &shift, &log_scale);
    round = _mm_unpackhi_epi64(round, round);
    quant = _mm_unpackhi_epi64(quant, quant);
    shift = _mm_unpackhi_epi64(shift, shift);
    calculate_qcoeff_log_scale(&qcoeff1, round, quant, &shift, &log_scale);

    // Reinsert signs
    qcoeff0 = invert_sign_sse2(qcoeff0, coeff0_sign);
    qcoeff1 = invert_sign_sse2(qcoeff1, coeff1_sign);

    // Mask out zbin threshold coeffs
    qcoeff0 = _mm_and_si128(qcoeff0, cmp_mask0);
    qcoeff1 = _mm_and_si128(qcoeff1, cmp_mask1);

    store_coefficients(qcoeff0, qcoeff_ptr);
    store_coefficients(qcoeff1, qcoeff_ptr + 8);

    calculate_dqcoeff_and_store_log_scale(qcoeff0, dequant, zero, dqcoeff_ptr,
                                          &log_scale);
    dequant = _mm_unpackhi_epi64(dequant, dequant);
    calculate_dqcoeff_and_store_log_scale(qcoeff1, dequant, zero,
                                          dqcoeff_ptr + 8, &log_scale);
  }

  // AC only loop.
  while (index < n_coeffs) {
    coeff0 = load_coefficients(coeff_ptr + index);
    coeff1 = load_coefficients(coeff_ptr + index + 8);

    coeff0_sign = _mm_srai_epi16(coeff0, 15);
    coeff1_sign = _mm_srai_epi16(coeff1, 15);
    qcoeff0 = invert_sign_sse2(coeff0, coeff0_sign);
    qcoeff1 = invert_sign_sse2(coeff1, coeff1_sign);

    update_mask0(&qcoeff0, &qcoeff1, threshold, iscan + index, &is_found0,
                 &mask0);

    cmp_mask0 = _mm_cmpgt_epi16(qcoeff0, zbin);
    cmp_mask1 = _mm_cmpgt_epi16(qcoeff1, zbin);

    update_mask1(&cmp_mask0, &cmp_mask1, iscan + index, &is_found1, &mask1);

    all_zero = _mm_or_si128(cmp_mask0, cmp_mask1);
    if (_mm_movemask_epi8(all_zero) == 0) {
      _mm_store_si128((__m128i *)(qcoeff_ptr + index), zero);
      _mm_store_si128((__m128i *)(qcoeff_ptr + index + 4), zero);
      _mm_store_si128((__m128i *)(qcoeff_ptr + index + 8), zero);
      _mm_store_si128((__m128i *)(qcoeff_ptr + index + 12), zero);
      _mm_store_si128((__m128i *)(dqcoeff_ptr + index), zero);
      _mm_store_si128((__m128i *)(dqcoeff_ptr + index + 4), zero);
      _mm_store_si128((__m128i *)(dqcoeff_ptr + index + 8), zero);
      _mm_store_si128((__m128i *)(dqcoeff_ptr + index + 12), zero);
      index += 16;
      continue;
    }
    calculate_qcoeff_log_scale(&qcoeff0, round, quant, &shift, &log_scale);
    calculate_qcoeff_log_scale(&qcoeff1, round, quant, &shift, &log_scale);

    qcoeff0 = invert_sign_sse2(qcoeff0, coeff0_sign);
    qcoeff1 = invert_sign_sse2(qcoeff1, coeff1_sign);

    qcoeff0 = _mm_and_si128(qcoeff0, cmp_mask0);
    qcoeff1 = _mm_and_si128(qcoeff1, cmp_mask1);

    store_coefficients(qcoeff0, qcoeff_ptr + index);
    store_coefficients(qcoeff1, qcoeff_ptr + index + 8);

    calculate_dqcoeff_and_store_log_scale(qcoeff0, dequant, zero,
                                          dqcoeff_ptr + index, &log_scale);
    calculate_dqcoeff_and_store_log_scale(qcoeff1, dequant, zero,
                                          dqcoeff_ptr + index + 8, &log_scale);
    index += 16;
  }
  if (is_found0) non_zero_count = calculate_non_zero_count(mask0);
  if (is_found1)
    non_zero_count_prescan_add_zero = calculate_non_zero_count(mask1);

  for (int i = non_zero_count_prescan_add_zero - 1; i >= non_zero_count; i--) {
    const int rc = scan[i];
    qcoeff_ptr[rc] = 0;
    dqcoeff_ptr[rc] = 0;
  }

  for (int i = non_zero_count - 1; i >= 0; i--) {
    const int rc = scan[i];
    if (qcoeff_ptr[rc]) {
      eob = i;
      break;
    }
  }

  *eob_ptr = eob + 1;
#if SKIP_EOB_FACTOR_ADJUST
  // TODO(Aniket): Experiment the following loop with intrinsic by combining
  // with the quantization loop above
  for (int i = 0; i < non_zero_count; i++) {
    const int rc = scan[i];
    const int qcoeff = qcoeff_ptr[rc];
    if (qcoeff) {
      first = i;
      break;
    }
  }
  if ((*eob_ptr - 1) >= 0 && first == (*eob_ptr - 1)) {
    const int rc = scan[(*eob_ptr - 1)];
    if (qcoeff_ptr[rc] == 1 || qcoeff_ptr[rc] == -1) {
      const int coeff = coeff_ptr[rc] * wt;
      const int coeff_sign = AOMSIGN(coeff);
      const int abs_coeff = (coeff ^ coeff_sign) - coeff_sign;
      const int factor = EOB_FACTOR + SKIP_EOB_FACTOR_ADJUST;
      const int prescan_add_val =
          ROUND_POWER_OF_TWO(dequant_ptr[rc != 0] * factor, 7);
      if (abs_coeff < (zbins[rc != 0] * (1 << AOM_QM_BITS) + prescan_add_val)) {
        qcoeff_ptr[rc] = 0;
        dqcoeff_ptr[rc] = 0;
        *eob_ptr = 0;
      }
    }
  }
#endif
}
