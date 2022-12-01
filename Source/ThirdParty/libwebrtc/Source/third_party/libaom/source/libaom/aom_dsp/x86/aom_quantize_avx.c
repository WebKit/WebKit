/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
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
#include "aom_dsp/x86/bitdepth_conversion_sse2.h"
#include "aom_dsp/x86/quantize_x86.h"

static INLINE void calculate_dqcoeff_and_store(__m128i qcoeff, __m128i dequant,
                                               tran_low_t *dqcoeff) {
  const __m128i low = _mm_mullo_epi16(qcoeff, dequant);
  const __m128i high = _mm_mulhi_epi16(qcoeff, dequant);

  const __m128i dqcoeff32_0 = _mm_unpacklo_epi16(low, high);
  const __m128i dqcoeff32_1 = _mm_unpackhi_epi16(low, high);

  _mm_store_si128((__m128i *)(dqcoeff), dqcoeff32_0);
  _mm_store_si128((__m128i *)(dqcoeff + 4), dqcoeff32_1);
}

void aom_quantize_b_avx(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                        const int16_t *zbin_ptr, const int16_t *round_ptr,
                        const int16_t *quant_ptr,
                        const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
                        tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr,
                        uint16_t *eob_ptr, const int16_t *scan,
                        const int16_t *iscan) {
  const __m128i zero = _mm_setzero_si128();
  const __m256i big_zero = _mm256_setzero_si256();
  int index;

  __m128i zbin, round, quant, dequant, shift;
  __m128i coeff0, coeff1;
  __m128i qcoeff0, qcoeff1;
  __m128i cmp_mask0, cmp_mask1;
  __m128i all_zero;
  __m128i eob = zero, eob0;

  (void)scan;

  *eob_ptr = 0;

  load_b_values(zbin_ptr, &zbin, round_ptr, &round, quant_ptr, &quant,
                dequant_ptr, &dequant, quant_shift_ptr, &shift);

  // Do DC and first 15 AC.
  coeff0 = load_tran_low(coeff_ptr);
  coeff1 = load_tran_low(coeff_ptr + 8);

  qcoeff0 = _mm_abs_epi16(coeff0);
  qcoeff1 = _mm_abs_epi16(coeff1);

  cmp_mask0 = _mm_cmpgt_epi16(qcoeff0, zbin);
  zbin = _mm_unpackhi_epi64(zbin, zbin);  // Switch DC to AC
  cmp_mask1 = _mm_cmpgt_epi16(qcoeff1, zbin);

  all_zero = _mm_or_si128(cmp_mask0, cmp_mask1);
  if (_mm_test_all_zeros(all_zero, all_zero)) {
    _mm256_store_si256((__m256i *)(qcoeff_ptr), big_zero);
    _mm256_store_si256((__m256i *)(dqcoeff_ptr), big_zero);
    _mm256_store_si256((__m256i *)(qcoeff_ptr + 8), big_zero);
    _mm256_store_si256((__m256i *)(dqcoeff_ptr + 8), big_zero);

    if (n_coeffs == 16) return;

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
    qcoeff0 = _mm_sign_epi16(qcoeff0, coeff0);
    qcoeff1 = _mm_sign_epi16(qcoeff1, coeff1);

    // Mask out zbin threshold coeffs
    qcoeff0 = _mm_and_si128(qcoeff0, cmp_mask0);
    qcoeff1 = _mm_and_si128(qcoeff1, cmp_mask1);

    store_tran_low(qcoeff0, qcoeff_ptr);
    store_tran_low(qcoeff1, qcoeff_ptr + 8);

    calculate_dqcoeff_and_store(qcoeff0, dequant, dqcoeff_ptr);
    dequant = _mm_unpackhi_epi64(dequant, dequant);
    calculate_dqcoeff_and_store(qcoeff1, dequant, dqcoeff_ptr + 8);

    eob =
        scan_for_eob(&qcoeff0, &qcoeff1, cmp_mask0, cmp_mask1, iscan, 0, zero);
  }

  // AC only loop.
  for (index = 16; index < n_coeffs; index += 16) {
    coeff0 = load_tran_low(coeff_ptr + index);
    coeff1 = load_tran_low(coeff_ptr + index + 8);

    qcoeff0 = _mm_abs_epi16(coeff0);
    qcoeff1 = _mm_abs_epi16(coeff1);

    cmp_mask0 = _mm_cmpgt_epi16(qcoeff0, zbin);
    cmp_mask1 = _mm_cmpgt_epi16(qcoeff1, zbin);

    all_zero = _mm_or_si128(cmp_mask0, cmp_mask1);
    if (_mm_test_all_zeros(all_zero, all_zero)) {
      _mm256_store_si256((__m256i *)(qcoeff_ptr + index), big_zero);
      _mm256_store_si256((__m256i *)(dqcoeff_ptr + index), big_zero);
      _mm256_store_si256((__m256i *)(qcoeff_ptr + index + 8), big_zero);
      _mm256_store_si256((__m256i *)(dqcoeff_ptr + index + 8), big_zero);
      continue;
    }

    calculate_qcoeff(&qcoeff0, round, quant, shift);
    calculate_qcoeff(&qcoeff1, round, quant, shift);

    qcoeff0 = _mm_sign_epi16(qcoeff0, coeff0);
    qcoeff1 = _mm_sign_epi16(qcoeff1, coeff1);

    qcoeff0 = _mm_and_si128(qcoeff0, cmp_mask0);
    qcoeff1 = _mm_and_si128(qcoeff1, cmp_mask1);

    store_tran_low(qcoeff0, qcoeff_ptr + index);
    store_tran_low(qcoeff1, qcoeff_ptr + index + 8);

    calculate_dqcoeff_and_store(qcoeff0, dequant, dqcoeff_ptr + index);
    calculate_dqcoeff_and_store(qcoeff1, dequant, dqcoeff_ptr + index + 8);

    eob0 = scan_for_eob(&qcoeff0, &qcoeff1, cmp_mask0, cmp_mask1, iscan, index,
                        zero);
    eob = _mm_max_epi16(eob, eob0);
  }

  *eob_ptr = accumulate_eob(eob);
}

void aom_quantize_b_32x32_avx(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                              const int16_t *zbin_ptr, const int16_t *round_ptr,
                              const int16_t *quant_ptr,
                              const int16_t *quant_shift_ptr,
                              tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                              const int16_t *dequant_ptr, uint16_t *eob_ptr,
                              const int16_t *scan, const int16_t *iscan) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i one = _mm_set1_epi16(1);
  const __m256i big_zero = _mm256_setzero_si256();
  int index;
  const int log_scale = 1;

  __m128i zbin, round, quant, dequant, shift;
  __m128i coeff0, coeff1;
  __m128i qcoeff0, qcoeff1;
  __m128i cmp_mask0, cmp_mask1;
  __m128i all_zero;
  __m128i eob = zero, eob0;

  (void)scan;

  // Setup global values.
  // The 32x32 halves zbin and round.
  zbin = _mm_load_si128((const __m128i *)zbin_ptr);
  // Shift with rounding.
  zbin = _mm_add_epi16(zbin, one);
  zbin = _mm_srli_epi16(zbin, 1);
  // x86 has no "greater *or equal*" comparison. Subtract 1 from zbin so
  // it is a strict "greater" comparison.
  zbin = _mm_sub_epi16(zbin, one);

  round = _mm_load_si128((const __m128i *)round_ptr);
  round = _mm_add_epi16(round, one);
  round = _mm_srli_epi16(round, 1);

  quant = _mm_load_si128((const __m128i *)quant_ptr);
  dequant = _mm_load_si128((const __m128i *)dequant_ptr);
  shift = _mm_load_si128((const __m128i *)quant_shift_ptr);

  // Do DC and first 15 AC.
  coeff0 = load_tran_low(coeff_ptr);
  coeff1 = load_tran_low(coeff_ptr + 8);

  qcoeff0 = _mm_abs_epi16(coeff0);
  qcoeff1 = _mm_abs_epi16(coeff1);

  cmp_mask0 = _mm_cmpgt_epi16(qcoeff0, zbin);
  zbin = _mm_unpackhi_epi64(zbin, zbin);  // Switch DC to AC.
  cmp_mask1 = _mm_cmpgt_epi16(qcoeff1, zbin);

  all_zero = _mm_or_si128(cmp_mask0, cmp_mask1);
  if (_mm_test_all_zeros(all_zero, all_zero)) {
    _mm256_store_si256((__m256i *)(qcoeff_ptr), big_zero);
    _mm256_store_si256((__m256i *)(dqcoeff_ptr), big_zero);
    _mm256_store_si256((__m256i *)(qcoeff_ptr + 8), big_zero);
    _mm256_store_si256((__m256i *)(dqcoeff_ptr + 8), big_zero);

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

    // Reinsert signs.
    qcoeff0 = _mm_sign_epi16(qcoeff0, coeff0);
    qcoeff1 = _mm_sign_epi16(qcoeff1, coeff1);

    // Mask out zbin threshold coeffs.
    qcoeff0 = _mm_and_si128(qcoeff0, cmp_mask0);
    qcoeff1 = _mm_and_si128(qcoeff1, cmp_mask1);

    store_tran_low(qcoeff0, qcoeff_ptr);
    store_tran_low(qcoeff1, qcoeff_ptr + 8);

    calculate_dqcoeff_and_store_log_scale(qcoeff0, dequant, zero, dqcoeff_ptr,
                                          &log_scale);
    dequant = _mm_unpackhi_epi64(dequant, dequant);
    calculate_dqcoeff_and_store_log_scale(qcoeff1, dequant, zero,
                                          dqcoeff_ptr + 8, &log_scale);

    eob =
        scan_for_eob(&qcoeff0, &qcoeff1, cmp_mask0, cmp_mask1, iscan, 0, zero);
  }

  // AC only loop.
  for (index = 16; index < n_coeffs; index += 16) {
    coeff0 = load_tran_low(coeff_ptr + index);
    coeff1 = load_tran_low(coeff_ptr + index + 8);

    qcoeff0 = _mm_abs_epi16(coeff0);
    qcoeff1 = _mm_abs_epi16(coeff1);

    cmp_mask0 = _mm_cmpgt_epi16(qcoeff0, zbin);
    cmp_mask1 = _mm_cmpgt_epi16(qcoeff1, zbin);

    all_zero = _mm_or_si128(cmp_mask0, cmp_mask1);
    if (_mm_test_all_zeros(all_zero, all_zero)) {
      _mm256_store_si256((__m256i *)(qcoeff_ptr + index), big_zero);
      _mm256_store_si256((__m256i *)(dqcoeff_ptr + index), big_zero);
      _mm256_store_si256((__m256i *)(qcoeff_ptr + index + 8), big_zero);
      _mm256_store_si256((__m256i *)(dqcoeff_ptr + index + 8), big_zero);
      continue;
    }

    calculate_qcoeff_log_scale(&qcoeff0, round, quant, &shift, &log_scale);
    calculate_qcoeff_log_scale(&qcoeff1, round, quant, &shift, &log_scale);

    qcoeff0 = _mm_sign_epi16(qcoeff0, coeff0);
    qcoeff1 = _mm_sign_epi16(qcoeff1, coeff1);

    qcoeff0 = _mm_and_si128(qcoeff0, cmp_mask0);
    qcoeff1 = _mm_and_si128(qcoeff1, cmp_mask1);

    store_tran_low(qcoeff0, qcoeff_ptr + index);
    store_tran_low(qcoeff1, qcoeff_ptr + index + 8);

    calculate_dqcoeff_and_store_log_scale(qcoeff0, dequant, zero,
                                          dqcoeff_ptr + index, &log_scale);
    calculate_dqcoeff_and_store_log_scale(qcoeff1, dequant, zero,
                                          dqcoeff_ptr + index + 8, &log_scale);

    eob0 = scan_for_eob(&qcoeff0, &qcoeff1, cmp_mask0, cmp_mask1, iscan, index,
                        zero);
    eob = _mm_max_epi16(eob, eob0);
  }

  *eob_ptr = accumulate_eob(eob);
}
