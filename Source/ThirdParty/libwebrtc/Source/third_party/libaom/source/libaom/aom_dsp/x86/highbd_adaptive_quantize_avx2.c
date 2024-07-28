/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <immintrin.h>

#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/quantize.h"
#include "aom_dsp/x86/quantize_x86.h"

static INLINE void highbd_load_b_values_avx2(
    const int16_t *zbin_ptr, __m256i *zbin, const int16_t *round_ptr,
    __m256i *round, const int16_t *quant_ptr, __m256i *quant,
    const int16_t *dequant_ptr, __m256i *dequant, const int16_t *shift_ptr,
    __m256i *shift) {
  *zbin = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *)zbin_ptr));
  *zbin = _mm256_sub_epi32(*zbin, _mm256_set1_epi32(1));
  *round = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *)round_ptr));
  *quant = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *)quant_ptr));
  *dequant =
      _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *)dequant_ptr));
  *shift = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *)shift_ptr));
}

static INLINE void highbd_update_mask1_avx2(__m256i *cmp_mask,
                                            const int16_t *iscan_ptr,
                                            int *is_found, __m256i *mask) {
  __m256i temp_mask = _mm256_setzero_si256();
  if (_mm256_movemask_epi8(*cmp_mask)) {
    __m256i iscan = _mm256_loadu_si256((const __m256i *)(iscan_ptr));
    temp_mask = _mm256_and_si256(*cmp_mask, iscan);
    *is_found = 1;
  }
  *mask = _mm256_max_epi16(temp_mask, *mask);
}

static INLINE void highbd_update_mask0_avx2(__m256i *qcoeff0, __m256i *qcoeff1,
                                            __m256i *threshold,
                                            const int16_t *iscan_ptr,
                                            int *is_found, __m256i *mask) {
  __m256i coeff[2], cmp_mask0, cmp_mask1;
  coeff[0] = _mm256_slli_epi32(*qcoeff0, AOM_QM_BITS);
  cmp_mask0 = _mm256_cmpgt_epi32(coeff[0], threshold[0]);
  coeff[1] = _mm256_slli_epi32(*qcoeff1, AOM_QM_BITS);
  cmp_mask1 = _mm256_cmpgt_epi32(coeff[1], threshold[1]);
  cmp_mask0 =
      _mm256_permute4x64_epi64(_mm256_packs_epi32(cmp_mask0, cmp_mask1), 0xd8);
  highbd_update_mask1_avx2(&cmp_mask0, iscan_ptr, is_found, mask);
}

static INLINE void highbd_mul_shift_avx2(const __m256i *x, const __m256i *y,
                                         __m256i *p, const int shift) {
  __m256i prod_lo = _mm256_mul_epi32(*x, *y);
  __m256i prod_hi = _mm256_srli_epi64(*x, 32);
  const __m256i mult_hi = _mm256_srli_epi64(*y, 32);
  prod_hi = _mm256_mul_epi32(prod_hi, mult_hi);

  prod_lo = _mm256_srli_epi64(prod_lo, shift);
  prod_hi = _mm256_srli_epi64(prod_hi, shift);

  prod_hi = _mm256_slli_epi64(prod_hi, 32);
  *p = _mm256_blend_epi32(prod_lo, prod_hi, 0xaa);
}

static INLINE void highbd_calculate_qcoeff_avx2(__m256i *coeff,
                                                const __m256i *round,
                                                const __m256i *quant,
                                                const __m256i *shift,
                                                const int *log_scale) {
  __m256i tmp, qcoeff;
  qcoeff = _mm256_add_epi32(*coeff, *round);
  highbd_mul_shift_avx2(&qcoeff, quant, &tmp, 16);
  qcoeff = _mm256_add_epi32(tmp, qcoeff);
  highbd_mul_shift_avx2(&qcoeff, shift, coeff, 16 - *log_scale);
}

static INLINE __m256i highbd_calculate_dqcoeff_avx2(__m256i qcoeff,
                                                    __m256i dequant) {
  return _mm256_mullo_epi32(qcoeff, dequant);
}

static INLINE __m256i highbd_calculate_dqcoeff_log_scale_avx2(
    __m256i qcoeff, __m256i dequant, const int log_scale) {
  __m256i abs_coeff = _mm256_abs_epi32(qcoeff);
  highbd_mul_shift_avx2(&abs_coeff, &dequant, &abs_coeff, log_scale);
  return _mm256_sign_epi32(abs_coeff, qcoeff);
}

static INLINE void highbd_store_coefficients_avx2(__m256i coeff0,
                                                  __m256i coeff1,
                                                  tran_low_t *coeff_ptr) {
  _mm256_store_si256((__m256i *)(coeff_ptr), coeff0);
  _mm256_store_si256((__m256i *)(coeff_ptr + 8), coeff1);
}

void aom_highbd_quantize_b_adaptive_avx2(
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
  const __m256i zero = _mm256_setzero_si256();
  __m256i zbin, round, quant, dequant, shift;
  __m256i coeff0, qcoeff0, coeff1, qcoeff1;
  __m256i cmp_mask, mask0 = zero, mask1 = zero;
  __m128i temp_mask0, temp_mask1;
  int prescan_add[2];
  int thresh[2];
  const int log_scale = 0;
  const qm_val_t wt = (1 << AOM_QM_BITS);
  for (int i = 0; i < 2; ++i) {
    prescan_add[i] = ROUND_POWER_OF_TWO(dequant_ptr[i] * EOB_FACTOR, 7);
    thresh[i] = (zbin_ptr[i] * wt + prescan_add[i]) - 1;
  }
  __m256i threshold[2];
  threshold[0] = _mm256_set1_epi32(thresh[0]);
  threshold[1] = _mm256_set1_epi32(thresh[1]);
  threshold[0] = _mm256_blend_epi32(threshold[0], threshold[1], 0xfe);

#if SKIP_EOB_FACTOR_ADJUST
  int first = -1;
#endif

  // Setup global values.
  highbd_load_b_values_avx2(zbin_ptr, &zbin, round_ptr, &round, quant_ptr,
                            &quant, dequant_ptr, &dequant, quant_shift_ptr,
                            &shift);

  // Do DC and first 15 AC.
  coeff0 = _mm256_load_si256((__m256i *)(coeff_ptr));
  qcoeff0 = _mm256_abs_epi32(coeff0);
  coeff1 = _mm256_load_si256((__m256i *)(coeff_ptr + 8));
  qcoeff1 = _mm256_abs_epi32(coeff1);
  highbd_update_mask0_avx2(&qcoeff0, &qcoeff1, threshold, iscan, &is_found0,
                           &mask0);
  __m256i temp0 = _mm256_cmpgt_epi32(qcoeff0, zbin);
  zbin = _mm256_unpackhi_epi64(zbin, zbin);
  __m256i temp1 = _mm256_cmpgt_epi32(qcoeff1, zbin);
  cmp_mask = _mm256_permute4x64_epi64(_mm256_packs_epi32(temp0, temp1), 0xd8);
  highbd_update_mask1_avx2(&cmp_mask, iscan, &is_found1, &mask1);
  threshold[0] = threshold[1];
  if (_mm256_movemask_epi8(cmp_mask) == 0) {
    _mm256_store_si256((__m256i *)(qcoeff_ptr), zero);
    _mm256_store_si256((__m256i *)(qcoeff_ptr + 8), zero);
    _mm256_store_si256((__m256i *)(dqcoeff_ptr), zero);
    _mm256_store_si256((__m256i *)(dqcoeff_ptr + 8), zero);
    round = _mm256_unpackhi_epi64(round, round);
    quant = _mm256_unpackhi_epi64(quant, quant);
    shift = _mm256_unpackhi_epi64(shift, shift);
    dequant = _mm256_unpackhi_epi64(dequant, dequant);
  } else {
    highbd_calculate_qcoeff_avx2(&qcoeff0, &round, &quant, &shift, &log_scale);
    round = _mm256_unpackhi_epi64(round, round);
    quant = _mm256_unpackhi_epi64(quant, quant);
    shift = _mm256_unpackhi_epi64(shift, shift);
    highbd_calculate_qcoeff_avx2(&qcoeff1, &round, &quant, &shift, &log_scale);
    // Reinsert signs
    qcoeff0 = _mm256_sign_epi32(qcoeff0, coeff0);
    qcoeff1 = _mm256_sign_epi32(qcoeff1, coeff1);
    // Mask out zbin threshold coeffs
    qcoeff0 = _mm256_and_si256(qcoeff0, temp0);
    qcoeff1 = _mm256_and_si256(qcoeff1, temp1);
    highbd_store_coefficients_avx2(qcoeff0, qcoeff1, qcoeff_ptr);
    coeff0 = highbd_calculate_dqcoeff_avx2(qcoeff0, dequant);
    dequant = _mm256_unpackhi_epi64(dequant, dequant);
    coeff1 = highbd_calculate_dqcoeff_avx2(qcoeff1, dequant);
    highbd_store_coefficients_avx2(coeff0, coeff1, dqcoeff_ptr);
  }

  // AC only loop.
  while (index < n_coeffs) {
    coeff0 = _mm256_load_si256((__m256i *)(coeff_ptr + index));
    qcoeff0 = _mm256_abs_epi32(coeff0);
    coeff1 = _mm256_load_si256((__m256i *)(coeff_ptr + index + 8));
    qcoeff1 = _mm256_abs_epi32(coeff1);
    highbd_update_mask0_avx2(&qcoeff0, &qcoeff1, threshold, iscan + index,
                             &is_found0, &mask0);
    temp0 = _mm256_cmpgt_epi32(qcoeff0, zbin);
    temp1 = _mm256_cmpgt_epi32(qcoeff1, zbin);
    cmp_mask = _mm256_permute4x64_epi64(_mm256_packs_epi32(temp0, temp1), 0xd8);
    highbd_update_mask1_avx2(&cmp_mask, iscan + index, &is_found1, &mask1);
    if (_mm256_movemask_epi8(cmp_mask) == 0) {
      _mm256_store_si256((__m256i *)(qcoeff_ptr + index), zero);
      _mm256_store_si256((__m256i *)(qcoeff_ptr + index + 8), zero);
      _mm256_store_si256((__m256i *)(dqcoeff_ptr + index), zero);
      _mm256_store_si256((__m256i *)(dqcoeff_ptr + index + 8), zero);
      index += 16;
      continue;
    }
    highbd_calculate_qcoeff_avx2(&qcoeff0, &round, &quant, &shift, &log_scale);
    highbd_calculate_qcoeff_avx2(&qcoeff1, &round, &quant, &shift, &log_scale);
    qcoeff0 = _mm256_sign_epi32(qcoeff0, coeff0);
    qcoeff1 = _mm256_sign_epi32(qcoeff1, coeff1);
    qcoeff0 = _mm256_and_si256(qcoeff0, temp0);
    qcoeff1 = _mm256_and_si256(qcoeff1, temp1);
    highbd_store_coefficients_avx2(qcoeff0, qcoeff1, qcoeff_ptr + index);
    coeff0 = highbd_calculate_dqcoeff_avx2(qcoeff0, dequant);
    coeff1 = highbd_calculate_dqcoeff_avx2(qcoeff1, dequant);
    highbd_store_coefficients_avx2(coeff0, coeff1, dqcoeff_ptr + index);
    index += 16;
  }
  if (is_found0) {
    temp_mask0 = _mm_max_epi16(_mm256_castsi256_si128(mask0),
                               _mm256_extracti128_si256(mask0, 1));
    non_zero_count = calculate_non_zero_count(temp_mask0);
  }
  if (is_found1) {
    temp_mask1 = _mm_max_epi16(_mm256_castsi256_si128(mask1),
                               _mm256_extracti128_si256(mask1, 1));
    non_zero_count_prescan_add_zero = calculate_non_zero_count(temp_mask1);
  }

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

void aom_highbd_quantize_b_32x32_adaptive_avx2(
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
  const int log_scale = 1;
  const __m256i zero = _mm256_setzero_si256();
  __m256i zbin, round, quant, dequant, shift;
  __m256i coeff0, qcoeff0, coeff1, qcoeff1;
  __m256i cmp_mask, mask0 = zero, mask1 = zero;
  __m128i temp_mask0, temp_mask1;
  const __m256i one = _mm256_set1_epi32(1);
  const __m256i log_scale_vec = _mm256_set1_epi32(log_scale);
  int prescan_add[2];
  int thresh[2];
  const int zbins[2] = { ROUND_POWER_OF_TWO(zbin_ptr[0], log_scale),
                         ROUND_POWER_OF_TWO(zbin_ptr[1], log_scale) };
  const qm_val_t wt = (1 << AOM_QM_BITS);
  for (int i = 0; i < 2; ++i) {
    prescan_add[i] = ROUND_POWER_OF_TWO(dequant_ptr[i] * EOB_FACTOR, 7);
    thresh[i] = (zbins[i] * wt + prescan_add[i]) - 1;
  }
  __m256i threshold[2];
  threshold[0] = _mm256_set1_epi32(thresh[0]);
  threshold[1] = _mm256_set1_epi32(thresh[1]);
  threshold[0] = _mm256_blend_epi32(threshold[0], threshold[1], 0xfe);

#if SKIP_EOB_FACTOR_ADJUST
  int first = -1;
#endif

  // Setup global values.
  zbin = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *)zbin_ptr));
  round = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *)round_ptr));
  quant = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *)quant_ptr));
  dequant = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *)dequant_ptr));
  shift =
      _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *)quant_shift_ptr));

  // Shift with rounding.
  zbin = _mm256_add_epi32(zbin, log_scale_vec);
  round = _mm256_add_epi32(round, log_scale_vec);
  zbin = _mm256_srli_epi32(zbin, log_scale);
  round = _mm256_srli_epi32(round, log_scale);
  zbin = _mm256_sub_epi32(zbin, one);

  // Do DC and first 15 AC.
  coeff0 = _mm256_load_si256((__m256i *)(coeff_ptr));
  qcoeff0 = _mm256_abs_epi32(coeff0);
  coeff1 = _mm256_load_si256((__m256i *)(coeff_ptr + 8));
  qcoeff1 = _mm256_abs_epi32(coeff1);
  highbd_update_mask0_avx2(&qcoeff0, &qcoeff1, threshold, iscan, &is_found0,
                           &mask0);
  __m256i temp0 = _mm256_cmpgt_epi32(qcoeff0, zbin);
  zbin = _mm256_permute2x128_si256(zbin, zbin, 0x11);
  __m256i temp1 = _mm256_cmpgt_epi32(qcoeff1, zbin);
  cmp_mask = _mm256_permute4x64_epi64(_mm256_packs_epi32(temp0, temp1), 0xd8);
  highbd_update_mask1_avx2(&cmp_mask, iscan, &is_found1, &mask1);
  threshold[0] = threshold[1];
  if (_mm256_movemask_epi8(cmp_mask) == 0) {
    _mm256_store_si256((__m256i *)(qcoeff_ptr), zero);
    _mm256_store_si256((__m256i *)(qcoeff_ptr + 8), zero);
    _mm256_store_si256((__m256i *)(dqcoeff_ptr), zero);
    _mm256_store_si256((__m256i *)(dqcoeff_ptr + 8), zero);
    round = _mm256_permute2x128_si256(round, round, 0x11);
    quant = _mm256_permute2x128_si256(quant, quant, 0x11);
    shift = _mm256_permute2x128_si256(shift, shift, 0x11);
    dequant = _mm256_permute2x128_si256(dequant, dequant, 0x11);
  } else {
    highbd_calculate_qcoeff_avx2(&qcoeff0, &round, &quant, &shift, &log_scale);
    round = _mm256_permute2x128_si256(round, round, 0x11);
    quant = _mm256_permute2x128_si256(quant, quant, 0x11);
    shift = _mm256_permute2x128_si256(shift, shift, 0x11);
    highbd_calculate_qcoeff_avx2(&qcoeff1, &round, &quant, &shift, &log_scale);
    // Reinsert signs
    qcoeff0 = _mm256_sign_epi32(qcoeff0, coeff0);
    qcoeff1 = _mm256_sign_epi32(qcoeff1, coeff1);
    // Mask out zbin threshold coeffs
    qcoeff0 = _mm256_and_si256(qcoeff0, temp0);
    qcoeff1 = _mm256_and_si256(qcoeff1, temp1);
    highbd_store_coefficients_avx2(qcoeff0, qcoeff1, qcoeff_ptr);
    coeff0 =
        highbd_calculate_dqcoeff_log_scale_avx2(qcoeff0, dequant, log_scale);
    dequant = _mm256_permute2x128_si256(dequant, dequant, 0x11);
    coeff1 =
        highbd_calculate_dqcoeff_log_scale_avx2(qcoeff1, dequant, log_scale);
    highbd_store_coefficients_avx2(coeff0, coeff1, dqcoeff_ptr);
  }

  // AC only loop.
  while (index < n_coeffs) {
    coeff0 = _mm256_load_si256((__m256i *)(coeff_ptr + index));
    qcoeff0 = _mm256_abs_epi32(coeff0);
    coeff1 = _mm256_load_si256((__m256i *)(coeff_ptr + index + 8));
    qcoeff1 = _mm256_abs_epi32(coeff1);
    highbd_update_mask0_avx2(&qcoeff0, &qcoeff1, threshold, iscan + index,
                             &is_found0, &mask0);
    temp0 = _mm256_cmpgt_epi32(qcoeff0, zbin);
    temp1 = _mm256_cmpgt_epi32(qcoeff1, zbin);
    cmp_mask = _mm256_permute4x64_epi64(_mm256_packs_epi32(temp0, temp1), 0xd8);
    highbd_update_mask1_avx2(&cmp_mask, iscan + index, &is_found1, &mask1);
    if (_mm256_movemask_epi8(cmp_mask) == 0) {
      _mm256_store_si256((__m256i *)(qcoeff_ptr + index), zero);
      _mm256_store_si256((__m256i *)(qcoeff_ptr + index + 8), zero);
      _mm256_store_si256((__m256i *)(dqcoeff_ptr + index), zero);
      _mm256_store_si256((__m256i *)(dqcoeff_ptr + index + 8), zero);
      index += 16;
      continue;
    }
    highbd_calculate_qcoeff_avx2(&qcoeff0, &round, &quant, &shift, &log_scale);
    highbd_calculate_qcoeff_avx2(&qcoeff1, &round, &quant, &shift, &log_scale);
    qcoeff0 = _mm256_sign_epi32(qcoeff0, coeff0);
    qcoeff1 = _mm256_sign_epi32(qcoeff1, coeff1);
    qcoeff0 = _mm256_and_si256(qcoeff0, temp0);
    qcoeff1 = _mm256_and_si256(qcoeff1, temp1);
    highbd_store_coefficients_avx2(qcoeff0, qcoeff1, qcoeff_ptr + index);
    coeff0 =
        highbd_calculate_dqcoeff_log_scale_avx2(qcoeff0, dequant, log_scale);
    coeff1 =
        highbd_calculate_dqcoeff_log_scale_avx2(qcoeff1, dequant, log_scale);
    highbd_store_coefficients_avx2(coeff0, coeff1, dqcoeff_ptr + index);
    index += 16;
  }
  if (is_found0) {
    temp_mask0 = _mm_max_epi16(_mm256_castsi256_si128(mask0),
                               _mm256_extracti128_si256(mask0, 1));
    non_zero_count = calculate_non_zero_count(temp_mask0);
  }
  if (is_found1) {
    temp_mask1 = _mm_max_epi16(_mm256_castsi256_si128(mask1),
                               _mm256_extracti128_si256(mask1, 1));
    non_zero_count_prescan_add_zero = calculate_non_zero_count(temp_mask1);
  }

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
