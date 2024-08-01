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

#include <emmintrin.h>
#include <xmmintrin.h>

#include "config/av1_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/x86/quantize_x86.h"

static INLINE void read_coeff(const tran_low_t *coeff, intptr_t offset,
                              __m128i *c0, __m128i *c1) {
  const tran_low_t *addr = coeff + offset;
  if (sizeof(tran_low_t) == 4) {
    const __m128i x0 = _mm_load_si128((const __m128i *)addr);
    const __m128i x1 = _mm_load_si128((const __m128i *)addr + 1);
    const __m128i x2 = _mm_load_si128((const __m128i *)addr + 2);
    const __m128i x3 = _mm_load_si128((const __m128i *)addr + 3);
    *c0 = _mm_packs_epi32(x0, x1);
    *c1 = _mm_packs_epi32(x2, x3);
  } else {
    *c0 = _mm_load_si128((const __m128i *)addr);
    *c1 = _mm_load_si128((const __m128i *)addr + 1);
  }
}

static INLINE void write_qcoeff(const __m128i *qc0, const __m128i *qc1,
                                tran_low_t *qcoeff, intptr_t offset) {
  tran_low_t *addr = qcoeff + offset;
  if (sizeof(tran_low_t) == 4) {
    const __m128i zero = _mm_setzero_si128();
    __m128i sign_bits = _mm_cmplt_epi16(*qc0, zero);
    __m128i y0 = _mm_unpacklo_epi16(*qc0, sign_bits);
    __m128i y1 = _mm_unpackhi_epi16(*qc0, sign_bits);
    _mm_store_si128((__m128i *)addr, y0);
    _mm_store_si128((__m128i *)addr + 1, y1);

    sign_bits = _mm_cmplt_epi16(*qc1, zero);
    y0 = _mm_unpacklo_epi16(*qc1, sign_bits);
    y1 = _mm_unpackhi_epi16(*qc1, sign_bits);
    _mm_store_si128((__m128i *)addr + 2, y0);
    _mm_store_si128((__m128i *)addr + 3, y1);
  } else {
    _mm_store_si128((__m128i *)addr, *qc0);
    _mm_store_si128((__m128i *)addr + 1, *qc1);
  }
}

static INLINE void write_zero(tran_low_t *qcoeff, intptr_t offset) {
  const __m128i zero = _mm_setzero_si128();
  tran_low_t *addr = qcoeff + offset;
  if (sizeof(tran_low_t) == 4) {
    _mm_store_si128((__m128i *)addr, zero);
    _mm_store_si128((__m128i *)addr + 1, zero);
    _mm_store_si128((__m128i *)addr + 2, zero);
    _mm_store_si128((__m128i *)addr + 3, zero);
  } else {
    _mm_store_si128((__m128i *)addr, zero);
    _mm_store_si128((__m128i *)addr + 1, zero);
  }
}

static INLINE void quantize(const int16_t *iscan_ptr,
                            const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                            tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                            const __m128i *round0, const __m128i *round1,
                            const __m128i *quant0, const __m128i *quant1,
                            const __m128i *dequant0, const __m128i *dequant1,
                            const __m128i *thr0, const __m128i *thr1,
                            __m128i *eob) {
  __m128i coeff0, coeff1;
  // Do DC and first 15 AC
  read_coeff(coeff_ptr, n_coeffs, &coeff0, &coeff1);

  // Poor man's sign extract
  const __m128i coeff0_sign = _mm_srai_epi16(coeff0, 15);
  const __m128i coeff1_sign = _mm_srai_epi16(coeff1, 15);
  __m128i qcoeff0 = _mm_xor_si128(coeff0, coeff0_sign);
  __m128i qcoeff1 = _mm_xor_si128(coeff1, coeff1_sign);
  qcoeff0 = _mm_sub_epi16(qcoeff0, coeff0_sign);
  qcoeff1 = _mm_sub_epi16(qcoeff1, coeff1_sign);
  const __m128i mask0 = _mm_or_si128(_mm_cmpgt_epi16(qcoeff0, *thr0),
                                     _mm_cmpeq_epi16(qcoeff0, *thr0));
  const __m128i mask1 = _mm_or_si128(_mm_cmpgt_epi16(qcoeff1, *thr1),
                                     _mm_cmpeq_epi16(qcoeff1, *thr1));
  const int nzflag = _mm_movemask_epi8(mask0) | _mm_movemask_epi8(mask1);

  if (nzflag) {
    qcoeff0 = _mm_adds_epi16(qcoeff0, *round0);
    qcoeff1 = _mm_adds_epi16(qcoeff1, *round1);
    const __m128i qtmp0 = _mm_mulhi_epi16(qcoeff0, *quant0);
    const __m128i qtmp1 = _mm_mulhi_epi16(qcoeff1, *quant1);

    // Reinsert signs
    qcoeff0 = _mm_xor_si128(qtmp0, coeff0_sign);
    qcoeff1 = _mm_xor_si128(qtmp1, coeff1_sign);
    qcoeff0 = _mm_sub_epi16(qcoeff0, coeff0_sign);
    qcoeff1 = _mm_sub_epi16(qcoeff1, coeff1_sign);

    write_qcoeff(&qcoeff0, &qcoeff1, qcoeff_ptr, n_coeffs);

    coeff0 = _mm_mullo_epi16(qcoeff0, *dequant0);
    coeff1 = _mm_mullo_epi16(qcoeff1, *dequant1);

    write_qcoeff(&coeff0, &coeff1, dqcoeff_ptr, n_coeffs);

    const __m128i zero = _mm_setzero_si128();
    // Scan for eob
    const __m128i zero_coeff0 = _mm_cmpeq_epi16(coeff0, zero);
    const __m128i zero_coeff1 = _mm_cmpeq_epi16(coeff1, zero);
    const __m128i nzero_coeff0 = _mm_cmpeq_epi16(zero_coeff0, zero);
    const __m128i nzero_coeff1 = _mm_cmpeq_epi16(zero_coeff1, zero);
    const __m128i iscan0 =
        _mm_load_si128((const __m128i *)(iscan_ptr + n_coeffs));
    const __m128i iscan1 =
        _mm_load_si128((const __m128i *)(iscan_ptr + n_coeffs) + 1);
    // Add one to convert from indices to counts
    const __m128i iscan0_nz = _mm_sub_epi16(iscan0, nzero_coeff0);
    const __m128i iscan1_nz = _mm_sub_epi16(iscan1, nzero_coeff1);
    const __m128i eob0 = _mm_and_si128(iscan0_nz, nzero_coeff0);
    const __m128i eob1 = _mm_and_si128(iscan1_nz, nzero_coeff1);
    const __m128i eob2 = _mm_max_epi16(eob0, eob1);
    *eob = _mm_max_epi16(*eob, eob2);
  } else {
    write_zero(qcoeff_ptr, n_coeffs);
    write_zero(dqcoeff_ptr, n_coeffs);
  }
}

void av1_quantize_fp_sse2(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                          const int16_t *zbin_ptr, const int16_t *round_ptr,
                          const int16_t *quant_ptr,
                          const int16_t *quant_shift_ptr,
                          tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                          const int16_t *dequant_ptr, uint16_t *eob_ptr,
                          const int16_t *scan_ptr, const int16_t *iscan_ptr) {
  (void)scan_ptr;
  (void)zbin_ptr;
  (void)quant_shift_ptr;

  coeff_ptr += n_coeffs;
  iscan_ptr += n_coeffs;
  qcoeff_ptr += n_coeffs;
  dqcoeff_ptr += n_coeffs;
  n_coeffs = -n_coeffs;

  const __m128i round0 = _mm_load_si128((const __m128i *)round_ptr);
  const __m128i round1 = _mm_unpackhi_epi64(round0, round0);
  const __m128i quant0 = _mm_load_si128((const __m128i *)quant_ptr);
  const __m128i quant1 = _mm_unpackhi_epi64(quant0, quant0);
  const __m128i dequant0 = _mm_load_si128((const __m128i *)dequant_ptr);
  const __m128i dequant1 = _mm_unpackhi_epi64(dequant0, dequant0);
  const __m128i thr0 = _mm_srai_epi16(dequant0, 1);
  const __m128i thr1 = _mm_srai_epi16(dequant1, 1);
  __m128i eob = _mm_setzero_si128();

  quantize(iscan_ptr, coeff_ptr, n_coeffs, qcoeff_ptr, dqcoeff_ptr, &round0,
           &round1, &quant0, &quant1, &dequant0, &dequant1, &thr0, &thr1, &eob);

  n_coeffs += 8 * 2;

  // AC only loop
  while (n_coeffs < 0) {
    quantize(iscan_ptr, coeff_ptr, n_coeffs, qcoeff_ptr, dqcoeff_ptr, &round1,
             &round1, &quant1, &quant1, &dequant1, &dequant1, &thr1, &thr1,
             &eob);
    n_coeffs += 8 * 2;
  }

  // Accumulate EOB
  {
    __m128i eob_shuffled;
    eob_shuffled = _mm_shuffle_epi32(eob, 0xe);
    eob = _mm_max_epi16(eob, eob_shuffled);
    eob_shuffled = _mm_shufflelo_epi16(eob, 0xe);
    eob = _mm_max_epi16(eob, eob_shuffled);
    eob_shuffled = _mm_shufflelo_epi16(eob, 0x1);
    eob = _mm_max_epi16(eob, eob_shuffled);
    *eob_ptr = _mm_extract_epi16(eob, 1);
  }
}

static INLINE void quantize_lp(const int16_t *iscan_ptr,
                               const int16_t *coeff_ptr, intptr_t n_coeffs,
                               int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr,
                               const __m128i *round0, const __m128i *round1,
                               const __m128i *quant0, const __m128i *quant1,
                               const __m128i *dequant0, const __m128i *dequant1,
                               __m128i *eob) {
  const int16_t *read = coeff_ptr + n_coeffs;
  __m128i coeff0 = _mm_load_si128((const __m128i *)read);
  __m128i coeff1 = _mm_load_si128((const __m128i *)read + 1);

  // Poor man's sign extract
  const __m128i coeff0_sign = _mm_srai_epi16(coeff0, 15);
  const __m128i coeff1_sign = _mm_srai_epi16(coeff1, 15);
  __m128i qcoeff0 = _mm_xor_si128(coeff0, coeff0_sign);
  __m128i qcoeff1 = _mm_xor_si128(coeff1, coeff1_sign);
  qcoeff0 = _mm_sub_epi16(qcoeff0, coeff0_sign);
  qcoeff1 = _mm_sub_epi16(qcoeff1, coeff1_sign);

  qcoeff0 = _mm_adds_epi16(qcoeff0, *round0);
  qcoeff1 = _mm_adds_epi16(qcoeff1, *round1);
  const __m128i qtmp0 = _mm_mulhi_epi16(qcoeff0, *quant0);
  const __m128i qtmp1 = _mm_mulhi_epi16(qcoeff1, *quant1);

  // Reinsert signs
  qcoeff0 = _mm_xor_si128(qtmp0, coeff0_sign);
  qcoeff1 = _mm_xor_si128(qtmp1, coeff1_sign);
  qcoeff0 = _mm_sub_epi16(qcoeff0, coeff0_sign);
  qcoeff1 = _mm_sub_epi16(qcoeff1, coeff1_sign);

  int16_t *addr = qcoeff_ptr + n_coeffs;
  _mm_store_si128((__m128i *)addr, qcoeff0);
  _mm_store_si128((__m128i *)addr + 1, qcoeff1);

  coeff0 = _mm_mullo_epi16(qcoeff0, *dequant0);
  coeff1 = _mm_mullo_epi16(qcoeff1, *dequant1);

  addr = dqcoeff_ptr + n_coeffs;
  _mm_store_si128((__m128i *)addr, coeff0);
  _mm_store_si128((__m128i *)addr + 1, coeff1);

  const __m128i zero = _mm_setzero_si128();
  // Scan for eob
  const __m128i zero_coeff0 = _mm_cmpeq_epi16(coeff0, zero);
  const __m128i zero_coeff1 = _mm_cmpeq_epi16(coeff1, zero);
  const __m128i nzero_coeff0 = _mm_cmpeq_epi16(zero_coeff0, zero);
  const __m128i nzero_coeff1 = _mm_cmpeq_epi16(zero_coeff1, zero);

  const __m128i iscan0 =
      _mm_load_si128((const __m128i *)(iscan_ptr + n_coeffs));
  const __m128i iscan1 =
      _mm_load_si128((const __m128i *)(iscan_ptr + n_coeffs) + 1);

  // Add one to convert from indices to counts
  const __m128i iscan0_nz = _mm_sub_epi16(iscan0, nzero_coeff0);
  const __m128i iscan1_nz = _mm_sub_epi16(iscan1, nzero_coeff1);
  const __m128i eob0 = _mm_and_si128(iscan0_nz, nzero_coeff0);
  const __m128i eob1 = _mm_and_si128(iscan1_nz, nzero_coeff1);
  const __m128i eob2 = _mm_max_epi16(eob0, eob1);
  *eob = _mm_max_epi16(*eob, eob2);
}

void av1_quantize_lp_sse2(const int16_t *coeff_ptr, intptr_t n_coeffs,
                          const int16_t *round_ptr, const int16_t *quant_ptr,
                          int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr,
                          const int16_t *dequant_ptr, uint16_t *eob_ptr,
                          const int16_t *scan, const int16_t *iscan) {
  (void)scan;
  coeff_ptr += n_coeffs;
  iscan += n_coeffs;
  qcoeff_ptr += n_coeffs;
  dqcoeff_ptr += n_coeffs;
  n_coeffs = -n_coeffs;

  // Setup global values
  const __m128i round0 = _mm_load_si128((const __m128i *)round_ptr);
  const __m128i round1 = _mm_unpackhi_epi64(round0, round0);
  const __m128i quant0 = _mm_load_si128((const __m128i *)quant_ptr);
  const __m128i quant1 = _mm_unpackhi_epi64(quant0, quant0);
  const __m128i dequant0 = _mm_load_si128((const __m128i *)dequant_ptr);
  const __m128i dequant1 = _mm_unpackhi_epi64(dequant0, dequant0);
  __m128i eob = _mm_setzero_si128();

  // DC and first 15 AC
  quantize_lp(iscan, coeff_ptr, n_coeffs, qcoeff_ptr, dqcoeff_ptr, &round0,
              &round1, &quant0, &quant1, &dequant0, &dequant1, &eob);
  n_coeffs += 8 * 2;

  // AC only loop
  while (n_coeffs < 0) {
    quantize_lp(iscan, coeff_ptr, n_coeffs, qcoeff_ptr, dqcoeff_ptr, &round1,
                &round1, &quant1, &quant1, &dequant1, &dequant1, &eob);
    n_coeffs += 8 * 2;
  }

  // Accumulate EOB
  *eob_ptr = accumulate_eob(eob);
}
