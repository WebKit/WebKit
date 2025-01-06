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

static inline void load_b_values_avx2(const int16_t *zbin_ptr, __m256i *zbin,
                                      const int16_t *round_ptr, __m256i *round,
                                      const int16_t *quant_ptr, __m256i *quant,
                                      const int16_t *dequant_ptr,
                                      __m256i *dequant,
                                      const int16_t *shift_ptr,
                                      __m256i *shift) {
  *zbin = _mm256_castsi128_si256(_mm_load_si128((const __m128i *)zbin_ptr));
  *zbin = _mm256_permute4x64_epi64(*zbin, 0x54);
  *zbin = _mm256_sub_epi16(*zbin, _mm256_set1_epi16(1));
  *round = _mm256_castsi128_si256(_mm_load_si128((const __m128i *)round_ptr));
  *round = _mm256_permute4x64_epi64(*round, 0x54);
  *quant = _mm256_castsi128_si256(_mm_load_si128((const __m128i *)quant_ptr));
  *quant = _mm256_permute4x64_epi64(*quant, 0x54);
  *dequant =
      _mm256_castsi128_si256(_mm_load_si128((const __m128i *)dequant_ptr));
  *dequant = _mm256_permute4x64_epi64(*dequant, 0x54);
  *shift = _mm256_castsi128_si256(_mm_load_si128((const __m128i *)shift_ptr));
  *shift = _mm256_permute4x64_epi64(*shift, 0x54);
}

static inline __m256i load_coefficients_avx2(const tran_low_t *coeff_ptr) {
  const __m256i coeff1 = _mm256_load_si256((__m256i *)(coeff_ptr));
  const __m256i coeff2 = _mm256_load_si256((__m256i *)(coeff_ptr + 8));
  return _mm256_packs_epi32(coeff1, coeff2);
}

static inline void update_mask1_avx2(__m256i *cmp_mask,
                                     const int16_t *iscan_ptr, int *is_found,
                                     __m256i *mask) {
  __m256i temp_mask = _mm256_setzero_si256();
  if (_mm256_movemask_epi8(*cmp_mask)) {
    __m256i iscan = _mm256_loadu_si256((const __m256i *)(iscan_ptr));
    temp_mask = _mm256_and_si256(*cmp_mask, iscan);
    *is_found = 1;
  }
  *mask = _mm256_max_epi16(temp_mask, *mask);
}

static inline void update_mask0_avx2(__m256i *qcoeff, __m256i *threshold,
                                     const int16_t *iscan_ptr, int *is_found,
                                     __m256i *mask) {
  __m256i zero = _mm256_setzero_si256();
  __m256i coeff[2], cmp_mask0, cmp_mask1;
  coeff[0] = _mm256_unpacklo_epi16(*qcoeff, zero);
  coeff[1] = _mm256_unpackhi_epi16(*qcoeff, zero);
  coeff[0] = _mm256_slli_epi32(coeff[0], AOM_QM_BITS);
  cmp_mask0 = _mm256_cmpgt_epi32(coeff[0], threshold[0]);
  coeff[1] = _mm256_slli_epi32(coeff[1], AOM_QM_BITS);
  cmp_mask1 = _mm256_cmpgt_epi32(coeff[1], threshold[1]);
  cmp_mask0 =
      _mm256_permute4x64_epi64(_mm256_packs_epi32(cmp_mask0, cmp_mask1), 0xd8);
  update_mask1_avx2(&cmp_mask0, iscan_ptr, is_found, mask);
}

static inline void calculate_qcoeff_avx2(__m256i *coeff, const __m256i *round,
                                         const __m256i *quant,
                                         const __m256i *shift) {
  __m256i tmp, qcoeff;
  qcoeff = _mm256_adds_epi16(*coeff, *round);
  tmp = _mm256_mulhi_epi16(qcoeff, *quant);
  qcoeff = _mm256_add_epi16(tmp, qcoeff);
  *coeff = _mm256_mulhi_epi16(qcoeff, *shift);
}

static inline __m256i calculate_dqcoeff_avx2(__m256i qcoeff, __m256i dequant) {
  return _mm256_mullo_epi16(qcoeff, dequant);
}

static inline void store_coefficients_avx2(__m256i coeff_vals,
                                           tran_low_t *coeff_ptr) {
  __m256i coeff_sign = _mm256_srai_epi16(coeff_vals, 15);
  __m256i coeff_vals_lo = _mm256_unpacklo_epi16(coeff_vals, coeff_sign);
  __m256i coeff_vals_hi = _mm256_unpackhi_epi16(coeff_vals, coeff_sign);
  _mm256_store_si256((__m256i *)(coeff_ptr), coeff_vals_lo);
  _mm256_store_si256((__m256i *)(coeff_ptr + 8), coeff_vals_hi);
}

void aom_quantize_b_adaptive_avx2(
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
  __m256i coeff, qcoeff;
  __m256i cmp_mask, mask0 = zero, mask1 = zero;
  __m128i temp_mask0, temp_mask1;
  int prescan_add[2];
  int thresh[2];
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
  load_b_values_avx2(zbin_ptr, &zbin, round_ptr, &round, quant_ptr, &quant,
                     dequant_ptr, &dequant, quant_shift_ptr, &shift);

  // Do DC and first 15 AC.
  coeff = load_coefficients_avx2(coeff_ptr);
  qcoeff = _mm256_abs_epi16(coeff);
  update_mask0_avx2(&qcoeff, threshold, iscan, &is_found0, &mask0);
  __m256i temp0 = _mm256_cmpgt_epi16(qcoeff, zbin);
  zbin = _mm256_unpackhi_epi64(zbin, zbin);
  cmp_mask = _mm256_permute4x64_epi64(temp0, 0xd8);
  update_mask1_avx2(&cmp_mask, iscan, &is_found1, &mask1);
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
    calculate_qcoeff_avx2(&qcoeff, &round, &quant, &shift);
    round = _mm256_unpackhi_epi64(round, round);
    quant = _mm256_unpackhi_epi64(quant, quant);
    shift = _mm256_unpackhi_epi64(shift, shift);
    // Reinsert signs
    qcoeff = _mm256_sign_epi16(qcoeff, coeff);
    // Mask out zbin threshold coeffs
    qcoeff = _mm256_and_si256(qcoeff, temp0);
    store_coefficients_avx2(qcoeff, qcoeff_ptr);
    coeff = calculate_dqcoeff_avx2(qcoeff, dequant);
    dequant = _mm256_unpackhi_epi64(dequant, dequant);
    store_coefficients_avx2(coeff, dqcoeff_ptr);
  }

  // AC only loop.
  while (index < n_coeffs) {
    coeff = load_coefficients_avx2(coeff_ptr + index);
    qcoeff = _mm256_abs_epi16(coeff);
    update_mask0_avx2(&qcoeff, threshold, iscan + index, &is_found0, &mask0);
    temp0 = _mm256_cmpgt_epi16(qcoeff, zbin);
    cmp_mask = _mm256_permute4x64_epi64(temp0, 0xd8);
    update_mask1_avx2(&cmp_mask, iscan + index, &is_found1, &mask1);
    if (_mm256_movemask_epi8(cmp_mask) == 0) {
      _mm256_store_si256((__m256i *)(qcoeff_ptr + index), zero);
      _mm256_store_si256((__m256i *)(qcoeff_ptr + index + 8), zero);
      _mm256_store_si256((__m256i *)(dqcoeff_ptr + index), zero);
      _mm256_store_si256((__m256i *)(dqcoeff_ptr + index + 8), zero);
      index += 16;
      continue;
    }
    calculate_qcoeff_avx2(&qcoeff, &round, &quant, &shift);
    qcoeff = _mm256_sign_epi16(qcoeff, coeff);
    qcoeff = _mm256_and_si256(qcoeff, temp0);
    store_coefficients_avx2(qcoeff, qcoeff_ptr + index);
    coeff = calculate_dqcoeff_avx2(qcoeff, dequant);
    store_coefficients_avx2(coeff, dqcoeff_ptr + index);
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
    const int qcoeff0 = qcoeff_ptr[rc];
    if (qcoeff0) {
      first = i;
      break;
    }
  }
  if ((*eob_ptr - 1) >= 0 && first == (*eob_ptr - 1)) {
    const int rc = scan[(*eob_ptr - 1)];
    if (qcoeff_ptr[rc] == 1 || qcoeff_ptr[rc] == -1) {
      const int coeff0 = coeff_ptr[rc] * wt;
      const int coeff_sign = AOMSIGN(coeff0);
      const int abs_coeff = (coeff0 ^ coeff_sign) - coeff_sign;
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
