/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
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
#include <xmmintrin.h>

#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/x86/quantize_x86.h"

void aom_quantize_b_sse2(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                         const int16_t *zbin_ptr, const int16_t *round_ptr,
                         const int16_t *quant_ptr,
                         const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
                         tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr,
                         uint16_t *eob_ptr, const int16_t *scan_ptr,
                         const int16_t *iscan_ptr) {
  const __m128i zero = _mm_setzero_si128();
  int index = 16;

  __m128i zbin, round, quant, dequant, shift;
  __m128i coeff0, coeff1, coeff0_sign, coeff1_sign;
  __m128i qcoeff0, qcoeff1;
  __m128i cmp_mask0, cmp_mask1;
  __m128i eob, eob0;

  (void)scan_ptr;

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

  cmp_mask0 = _mm_cmpgt_epi16(qcoeff0, zbin);
  zbin = _mm_unpackhi_epi64(zbin, zbin);  // Switch DC to AC
  cmp_mask1 = _mm_cmpgt_epi16(qcoeff1, zbin);

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

  eob =
      scan_for_eob(&coeff0, &coeff1, cmp_mask0, cmp_mask1, iscan_ptr, 0, zero);

  // AC only loop.
  while (index < n_coeffs) {
    coeff0 = load_coefficients(coeff_ptr + index);
    coeff1 = load_coefficients(coeff_ptr + index + 8);

    coeff0_sign = _mm_srai_epi16(coeff0, 15);
    coeff1_sign = _mm_srai_epi16(coeff1, 15);
    qcoeff0 = invert_sign_sse2(coeff0, coeff0_sign);
    qcoeff1 = invert_sign_sse2(coeff1, coeff1_sign);

    cmp_mask0 = _mm_cmpgt_epi16(qcoeff0, zbin);
    cmp_mask1 = _mm_cmpgt_epi16(qcoeff1, zbin);

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

    eob0 = scan_for_eob(&coeff0, &coeff1, cmp_mask0, cmp_mask1, iscan_ptr,
                        index, zero);
    eob = _mm_max_epi16(eob, eob0);

    index += 16;
  }

  *eob_ptr = accumulate_eob(eob);
}
