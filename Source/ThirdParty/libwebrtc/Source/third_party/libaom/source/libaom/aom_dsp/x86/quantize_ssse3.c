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
#include <tmmintrin.h>
#include <emmintrin.h>
#include <xmmintrin.h>

#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/x86/quantize_x86.h"

static INLINE void calculate_qcoeff_64x64(__m128i *coeff, const __m128i round,
                                          const __m128i quant,
                                          const __m128i *shift) {
  __m128i tmp, qcoeff, tmp1;
  qcoeff = _mm_adds_epi16(*coeff, round);
  tmp = _mm_mulhi_epi16(qcoeff, quant);
  qcoeff = _mm_add_epi16(tmp, qcoeff);
  tmp = _mm_mullo_epi16(qcoeff, *shift);
  tmp = _mm_srli_epi16(tmp, 14);
  tmp1 = _mm_mulhi_epi16(qcoeff, *shift);
  tmp1 = _mm_slli_epi16(tmp1, 2);
  *coeff = _mm_or_si128(tmp, tmp1);
}

static INLINE void calculate_dqcoeff_and_store_64x64(const __m128i qcoeff,
                                                     const __m128i dequant,
                                                     const __m128i zero,
                                                     tran_low_t *dqcoeff) {
  // Un-sign to bias rounding like C.
  const __m128i coeff = _mm_abs_epi16(qcoeff);

  const __m128i sign_0 = _mm_unpacklo_epi16(zero, qcoeff);
  const __m128i sign_1 = _mm_unpackhi_epi16(zero, qcoeff);

  const __m128i low = _mm_mullo_epi16(coeff, dequant);
  const __m128i high = _mm_mulhi_epi16(coeff, dequant);
  __m128i dqcoeff32_0 = _mm_unpacklo_epi16(low, high);
  __m128i dqcoeff32_1 = _mm_unpackhi_epi16(low, high);

  // "Divide" by 4.
  dqcoeff32_0 = _mm_srli_epi32(dqcoeff32_0, 2);
  dqcoeff32_1 = _mm_srli_epi32(dqcoeff32_1, 2);

  dqcoeff32_0 = _mm_sign_epi32(dqcoeff32_0, sign_0);
  dqcoeff32_1 = _mm_sign_epi32(dqcoeff32_1, sign_1);

  _mm_store_si128((__m128i *)(dqcoeff), dqcoeff32_0);
  _mm_store_si128((__m128i *)(dqcoeff + 4), dqcoeff32_1);
}

void aom_quantize_b_64x64_ssse3(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                                const int16_t *zbin_ptr,
                                const int16_t *round_ptr,
                                const int16_t *quant_ptr,
                                const int16_t *quant_shift_ptr,
                                tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                                const int16_t *dequant_ptr, uint16_t *eob_ptr,
                                const int16_t *scan, const int16_t *iscan) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i one = _mm_set1_epi16(1);
  const __m128i two = _mm_set1_epi16(2);
  int index;

  __m128i zbin, round, quant, dequant, shift;
  __m128i coeff0, coeff1, qcoeff0, qcoeff1;
  __m128i cmp_mask0, cmp_mask1, all_zero;
  __m128i eob = zero, eob0;

  (void)scan;
  (void)n_coeffs;

  // Setup global values.
  zbin = _mm_load_si128((const __m128i *)zbin_ptr);
  round = _mm_load_si128((const __m128i *)round_ptr);
  quant = _mm_load_si128((const __m128i *)quant_ptr);
  dequant = _mm_load_si128((const __m128i *)dequant_ptr);
  shift = _mm_load_si128((const __m128i *)quant_shift_ptr);

  // Shift with rounding.
  zbin = _mm_add_epi16(zbin, two);
  round = _mm_add_epi16(round, two);
  zbin = _mm_srli_epi16(zbin, 2);
  round = _mm_srli_epi16(round, 2);
  zbin = _mm_sub_epi16(zbin, one);
  // Do DC and first 15 AC.
  coeff0 = load_coefficients(coeff_ptr);
  coeff1 = load_coefficients(coeff_ptr + 8);

  qcoeff0 = _mm_abs_epi16(coeff0);
  qcoeff1 = _mm_abs_epi16(coeff1);

  cmp_mask0 = _mm_cmpgt_epi16(qcoeff0, zbin);
  zbin = _mm_unpackhi_epi64(zbin, zbin);
  cmp_mask1 = _mm_cmpgt_epi16(qcoeff1, zbin);
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
    calculate_qcoeff_64x64(&qcoeff0, round, quant, &shift);
    round = _mm_unpackhi_epi64(round, round);
    quant = _mm_unpackhi_epi64(quant, quant);
    shift = _mm_unpackhi_epi64(shift, shift);
    calculate_qcoeff_64x64(&qcoeff1, round, quant, &shift);

    // Reinsert signs.
    qcoeff0 = _mm_sign_epi16(qcoeff0, coeff0);
    qcoeff1 = _mm_sign_epi16(qcoeff1, coeff1);

    // Mask out zbin threshold coeffs.
    qcoeff0 = _mm_and_si128(qcoeff0, cmp_mask0);
    qcoeff1 = _mm_and_si128(qcoeff1, cmp_mask1);

    store_coefficients(qcoeff0, qcoeff_ptr);
    store_coefficients(qcoeff1, qcoeff_ptr + 8);

    calculate_dqcoeff_and_store_64x64(qcoeff0, dequant, zero, dqcoeff_ptr);
    dequant = _mm_unpackhi_epi64(dequant, dequant);
    calculate_dqcoeff_and_store_64x64(qcoeff1, dequant, zero, dqcoeff_ptr + 8);

    eob =
        scan_for_eob(&qcoeff0, &qcoeff1, cmp_mask0, cmp_mask1, iscan, 0, zero);
  }

  // AC only loop.
  for (index = 16; index < 1024; index += 16) {
    coeff0 = load_coefficients(coeff_ptr + index);
    coeff1 = load_coefficients(coeff_ptr + index + 8);

    qcoeff0 = _mm_abs_epi16(coeff0);
    qcoeff1 = _mm_abs_epi16(coeff1);

    cmp_mask0 = _mm_cmpgt_epi16(qcoeff0, zbin);
    cmp_mask1 = _mm_cmpgt_epi16(qcoeff1, zbin);

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
      continue;
    }
    calculate_qcoeff_64x64(&qcoeff0, round, quant, &shift);
    calculate_qcoeff_64x64(&qcoeff1, round, quant, &shift);

    qcoeff0 = _mm_sign_epi16(qcoeff0, coeff0);
    qcoeff1 = _mm_sign_epi16(qcoeff1, coeff1);

    qcoeff0 = _mm_and_si128(qcoeff0, cmp_mask0);
    qcoeff1 = _mm_and_si128(qcoeff1, cmp_mask1);

    store_coefficients(qcoeff0, qcoeff_ptr + index);
    store_coefficients(qcoeff1, qcoeff_ptr + index + 8);

    calculate_dqcoeff_and_store_64x64(qcoeff0, dequant, zero,
                                      dqcoeff_ptr + index);
    calculate_dqcoeff_and_store_64x64(qcoeff1, dequant, zero,
                                      dqcoeff_ptr + 8 + index);

    eob0 = scan_for_eob(&qcoeff0, &qcoeff1, cmp_mask0, cmp_mask1, iscan, index,
                        zero);
    eob = _mm_max_epi16(eob, eob0);
  }

  *eob_ptr = accumulate_eob(eob);
}
