/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <immintrin.h>

#include "config/av1_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"

static INLINE void write_zero(tran_low_t *qcoeff) {
  const __m256i zero = _mm256_setzero_si256();
  _mm256_storeu_si256((__m256i *)qcoeff, zero);
  _mm256_storeu_si256((__m256i *)qcoeff + 1, zero);
}

static INLINE void init_one_qp(const __m128i *p, __m256i *qp) {
  const __m128i ac = _mm_unpackhi_epi64(*p, *p);
  *qp = _mm256_insertf128_si256(_mm256_castsi128_si256(*p), ac, 1);
}

static INLINE void init_qp(const int16_t *round_ptr, const int16_t *quant_ptr,
                           const int16_t *dequant_ptr, int log_scale,
                           __m256i *thr, __m256i *qp) {
  __m128i round = _mm_loadu_si128((const __m128i *)round_ptr);
  const __m128i quant = _mm_loadu_si128((const __m128i *)quant_ptr);
  const __m128i dequant = _mm_loadu_si128((const __m128i *)dequant_ptr);

  if (log_scale > 0) {
    const __m128i rnd = _mm_set1_epi16((int16_t)1 << (log_scale - 1));
    round = _mm_add_epi16(round, rnd);
    round = _mm_srai_epi16(round, log_scale);
  }

  init_one_qp(&round, &qp[0]);
  init_one_qp(&quant, &qp[1]);

  if (log_scale == 1) {
    qp[1] = _mm256_slli_epi16(qp[1], log_scale);
  }

  init_one_qp(&dequant, &qp[2]);
  *thr = _mm256_srai_epi16(qp[2], 1 + log_scale);
  // Subtracting 1 here eliminates a _mm256_cmpeq_epi16() instruction when
  // calculating the zbin mask.
  *thr = _mm256_sub_epi16(*thr, _mm256_set1_epi16(1));
}

static INLINE void update_qp(__m256i *thr, __m256i *qp) {
  qp[0] = _mm256_permute2x128_si256(qp[0], qp[0], 0x11);
  qp[1] = _mm256_permute2x128_si256(qp[1], qp[1], 0x11);
  qp[2] = _mm256_permute2x128_si256(qp[2], qp[2], 0x11);
  *thr = _mm256_permute2x128_si256(*thr, *thr, 0x11);
}

static INLINE __m256i load_coefficients_avx2(const tran_low_t *coeff_ptr) {
  const __m256i coeff1 = _mm256_load_si256((__m256i *)coeff_ptr);
  const __m256i coeff2 = _mm256_load_si256((__m256i *)(coeff_ptr + 8));
  return _mm256_packs_epi32(coeff1, coeff2);
}

static INLINE void store_coefficients_avx2(__m256i coeff_vals,
                                           tran_low_t *coeff_ptr) {
  __m256i coeff_sign = _mm256_srai_epi16(coeff_vals, 15);
  __m256i coeff_vals_lo = _mm256_unpacklo_epi16(coeff_vals, coeff_sign);
  __m256i coeff_vals_hi = _mm256_unpackhi_epi16(coeff_vals, coeff_sign);
  _mm256_store_si256((__m256i *)coeff_ptr, coeff_vals_lo);
  _mm256_store_si256((__m256i *)(coeff_ptr + 8), coeff_vals_hi);
}

static INLINE uint16_t quant_gather_eob(__m256i eob) {
  const __m128i eob_lo = _mm256_castsi256_si128(eob);
  const __m128i eob_hi = _mm256_extractf128_si256(eob, 1);
  __m128i eob_s = _mm_max_epi16(eob_lo, eob_hi);
  eob_s = _mm_subs_epu16(_mm_set1_epi16(INT16_MAX), eob_s);
  eob_s = _mm_minpos_epu16(eob_s);
  return INT16_MAX - _mm_extract_epi16(eob_s, 0);
}

static INLINE int16_t accumulate_eob256(__m256i eob256) {
  const __m128i eob_lo = _mm256_castsi256_si128(eob256);
  const __m128i eob_hi = _mm256_extractf128_si256(eob256, 1);
  __m128i eob = _mm_max_epi16(eob_lo, eob_hi);
  __m128i eob_shuffled = _mm_shuffle_epi32(eob, 0xe);
  eob = _mm_max_epi16(eob, eob_shuffled);
  eob_shuffled = _mm_shufflelo_epi16(eob, 0xe);
  eob = _mm_max_epi16(eob, eob_shuffled);
  eob_shuffled = _mm_shufflelo_epi16(eob, 0x1);
  eob = _mm_max_epi16(eob, eob_shuffled);
  return _mm_extract_epi16(eob, 1);
}

static AOM_FORCE_INLINE void quantize_lp_16(
    const int16_t *coeff_ptr, intptr_t n_coeffs, const int16_t *iscan_ptr,
    int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr, __m256i *round256,
    __m256i *quant256, __m256i *dequant256, __m256i *eob) {
  const __m256i coeff =
      _mm256_loadu_si256((const __m256i *)(coeff_ptr + n_coeffs));
  const __m256i abs_coeff = _mm256_abs_epi16(coeff);
  const __m256i tmp_rnd = _mm256_adds_epi16(abs_coeff, *round256);
  const __m256i abs_qcoeff = _mm256_mulhi_epi16(tmp_rnd, *quant256);
  const __m256i qcoeff = _mm256_sign_epi16(abs_qcoeff, coeff);
  const __m256i dqcoeff = _mm256_mullo_epi16(qcoeff, *dequant256);
  const __m256i nz_mask =
      _mm256_cmpgt_epi16(abs_qcoeff, _mm256_setzero_si256());

  _mm256_storeu_si256((__m256i *)(qcoeff_ptr + n_coeffs), qcoeff);
  _mm256_storeu_si256((__m256i *)(dqcoeff_ptr + n_coeffs), dqcoeff);

  const __m256i iscan =
      _mm256_loadu_si256((const __m256i *)(iscan_ptr + n_coeffs));
  const __m256i iscan_plus1 = _mm256_sub_epi16(iscan, nz_mask);
  const __m256i nz_iscan = _mm256_and_si256(iscan_plus1, nz_mask);
  *eob = _mm256_max_epi16(*eob, nz_iscan);
}

void av1_quantize_lp_avx2(const int16_t *coeff_ptr, intptr_t n_coeffs,
                          const int16_t *round_ptr, const int16_t *quant_ptr,
                          int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr,
                          const int16_t *dequant_ptr, uint16_t *eob_ptr,
                          const int16_t *scan, const int16_t *iscan) {
  (void)scan;
  __m256i eob256 = _mm256_setzero_si256();

  // Setup global values.
  __m256i round256 =
      _mm256_castsi128_si256(_mm_load_si128((const __m128i *)round_ptr));
  __m256i quant256 =
      _mm256_castsi128_si256(_mm_load_si128((const __m128i *)quant_ptr));
  __m256i dequant256 =
      _mm256_castsi128_si256(_mm_load_si128((const __m128i *)dequant_ptr));

  // Populate upper AC values.
  round256 = _mm256_permute4x64_epi64(round256, 0x54);
  quant256 = _mm256_permute4x64_epi64(quant256, 0x54);
  dequant256 = _mm256_permute4x64_epi64(dequant256, 0x54);

  coeff_ptr += n_coeffs;
  iscan += n_coeffs;
  qcoeff_ptr += n_coeffs;
  dqcoeff_ptr += n_coeffs;
  n_coeffs = -n_coeffs;

  // Process DC and the first 15 AC coeffs.
  quantize_lp_16(coeff_ptr, n_coeffs, iscan, qcoeff_ptr, dqcoeff_ptr, &round256,
                 &quant256, &dequant256, &eob256);

  // Overwrite the DC constants with AC constants
  dequant256 = _mm256_permute2x128_si256(dequant256, dequant256, 0x31);
  quant256 = _mm256_permute2x128_si256(quant256, quant256, 0x31);
  round256 = _mm256_permute2x128_si256(round256, round256, 0x31);

  n_coeffs += 8 * 2;

  // AC only loop.
  while (n_coeffs < 0) {
    quantize_lp_16(coeff_ptr, n_coeffs, iscan, qcoeff_ptr, dqcoeff_ptr,
                   &round256, &quant256, &dequant256, &eob256);

    n_coeffs += 8 * 2;
  }

  *eob_ptr = accumulate_eob256(eob256);
}

static AOM_FORCE_INLINE __m256i get_max_lane_eob(const int16_t *iscan,
                                                 __m256i v_eobmax,
                                                 __m256i v_mask) {
  const __m256i v_iscan = _mm256_loadu_si256((const __m256i *)iscan);
  const __m256i v_iscan_perm = _mm256_permute4x64_epi64(v_iscan, 0xD8);
  const __m256i v_iscan_plus1 = _mm256_sub_epi16(v_iscan_perm, v_mask);
  const __m256i v_nz_iscan = _mm256_and_si256(v_iscan_plus1, v_mask);
  return _mm256_max_epi16(v_eobmax, v_nz_iscan);
}

static AOM_FORCE_INLINE void quantize_fp_16(
    const __m256i *thr, const __m256i *qp, const tran_low_t *coeff_ptr,
    const int16_t *iscan_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
    __m256i *eob) {
  const __m256i coeff = load_coefficients_avx2(coeff_ptr);
  const __m256i abs_coeff = _mm256_abs_epi16(coeff);
  const __m256i mask = _mm256_cmpgt_epi16(abs_coeff, *thr);
  const int nzflag = _mm256_movemask_epi8(mask);

  if (nzflag) {
    const __m256i tmp_rnd = _mm256_adds_epi16(abs_coeff, qp[0]);
    const __m256i abs_q = _mm256_mulhi_epi16(tmp_rnd, qp[1]);
    const __m256i q = _mm256_sign_epi16(abs_q, coeff);
    const __m256i dq = _mm256_mullo_epi16(q, qp[2]);
    const __m256i nz_mask = _mm256_cmpgt_epi16(abs_q, _mm256_setzero_si256());

    store_coefficients_avx2(q, qcoeff_ptr);
    store_coefficients_avx2(dq, dqcoeff_ptr);

    *eob = get_max_lane_eob(iscan_ptr, *eob, nz_mask);
  } else {
    write_zero(qcoeff_ptr);
    write_zero(dqcoeff_ptr);
  }
}

void av1_quantize_fp_avx2(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                          const int16_t *zbin_ptr, const int16_t *round_ptr,
                          const int16_t *quant_ptr,
                          const int16_t *quant_shift_ptr,
                          tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                          const int16_t *dequant_ptr, uint16_t *eob_ptr,
                          const int16_t *scan_ptr, const int16_t *iscan_ptr) {
  (void)scan_ptr;
  (void)zbin_ptr;
  (void)quant_shift_ptr;

  const int log_scale = 0;
  const int step = 16;
  __m256i qp[3], thr;
  __m256i eob = _mm256_setzero_si256();

  init_qp(round_ptr, quant_ptr, dequant_ptr, log_scale, &thr, qp);

  quantize_fp_16(&thr, qp, coeff_ptr, iscan_ptr, qcoeff_ptr, dqcoeff_ptr, &eob);

  coeff_ptr += step;
  qcoeff_ptr += step;
  dqcoeff_ptr += step;
  iscan_ptr += step;
  n_coeffs -= step;

  update_qp(&thr, qp);

  while (n_coeffs > 0) {
    quantize_fp_16(&thr, qp, coeff_ptr, iscan_ptr, qcoeff_ptr, dqcoeff_ptr,
                   &eob);

    coeff_ptr += step;
    qcoeff_ptr += step;
    dqcoeff_ptr += step;
    iscan_ptr += step;
    n_coeffs -= step;
  }
  *eob_ptr = quant_gather_eob(eob);
}

static AOM_FORCE_INLINE void quantize_fp_32x32(
    const __m256i *thr, const __m256i *qp, const tran_low_t *coeff_ptr,
    const int16_t *iscan_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
    __m256i *eob) {
  const __m256i coeff = load_coefficients_avx2(coeff_ptr);
  const __m256i abs_coeff = _mm256_abs_epi16(coeff);
  const __m256i mask = _mm256_cmpgt_epi16(abs_coeff, *thr);
  const int nzflag = _mm256_movemask_epi8(mask);

  if (nzflag) {
    const __m256i tmp_rnd = _mm256_adds_epi16(abs_coeff, qp[0]);
    const __m256i abs_q = _mm256_mulhi_epu16(tmp_rnd, qp[1]);
    const __m256i q = _mm256_sign_epi16(abs_q, coeff);
    const __m256i abs_dq =
        _mm256_srli_epi16(_mm256_mullo_epi16(abs_q, qp[2]), 1);
    const __m256i nz_mask = _mm256_cmpgt_epi16(abs_q, _mm256_setzero_si256());
    const __m256i dq = _mm256_sign_epi16(abs_dq, coeff);

    store_coefficients_avx2(q, qcoeff_ptr);
    store_coefficients_avx2(dq, dqcoeff_ptr);

    *eob = get_max_lane_eob(iscan_ptr, *eob, nz_mask);
  } else {
    write_zero(qcoeff_ptr);
    write_zero(dqcoeff_ptr);
  }
}

void av1_quantize_fp_32x32_avx2(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan_ptr, const int16_t *iscan_ptr) {
  (void)scan_ptr;
  (void)zbin_ptr;
  (void)quant_shift_ptr;

  const int log_scale = 1;
  const unsigned int step = 16;
  __m256i qp[3], thr;
  __m256i eob = _mm256_setzero_si256();

  init_qp(round_ptr, quant_ptr, dequant_ptr, log_scale, &thr, qp);

  quantize_fp_32x32(&thr, qp, coeff_ptr, iscan_ptr, qcoeff_ptr, dqcoeff_ptr,
                    &eob);

  coeff_ptr += step;
  qcoeff_ptr += step;
  dqcoeff_ptr += step;
  iscan_ptr += step;
  n_coeffs -= step;

  update_qp(&thr, qp);

  while (n_coeffs > 0) {
    quantize_fp_32x32(&thr, qp, coeff_ptr, iscan_ptr, qcoeff_ptr, dqcoeff_ptr,
                      &eob);

    coeff_ptr += step;
    qcoeff_ptr += step;
    dqcoeff_ptr += step;
    iscan_ptr += step;
    n_coeffs -= step;
  }
  *eob_ptr = quant_gather_eob(eob);
}

static INLINE void quantize_fp_64x64(const __m256i *thr, const __m256i *qp,
                                     const tran_low_t *coeff_ptr,
                                     const int16_t *iscan_ptr,
                                     tran_low_t *qcoeff_ptr,
                                     tran_low_t *dqcoeff_ptr, __m256i *eob) {
  const __m256i coeff = load_coefficients_avx2(coeff_ptr);
  const __m256i abs_coeff = _mm256_abs_epi16(coeff);
  const __m256i mask = _mm256_cmpgt_epi16(abs_coeff, *thr);
  const int nzflag = _mm256_movemask_epi8(mask);

  if (nzflag) {
    const __m256i tmp_rnd =
        _mm256_and_si256(_mm256_adds_epi16(abs_coeff, qp[0]), mask);
    const __m256i qh = _mm256_slli_epi16(_mm256_mulhi_epi16(tmp_rnd, qp[1]), 2);
    const __m256i ql =
        _mm256_srli_epi16(_mm256_mullo_epi16(tmp_rnd, qp[1]), 14);
    const __m256i abs_q = _mm256_or_si256(qh, ql);
    const __m256i dqh = _mm256_slli_epi16(_mm256_mulhi_epi16(abs_q, qp[2]), 14);
    const __m256i dql = _mm256_srli_epi16(_mm256_mullo_epi16(abs_q, qp[2]), 2);
    const __m256i abs_dq = _mm256_or_si256(dqh, dql);
    const __m256i q = _mm256_sign_epi16(abs_q, coeff);
    const __m256i dq = _mm256_sign_epi16(abs_dq, coeff);
    // Check the signed q/dq value here instead of the absolute value. When
    // dequant equals 4, the dequant threshold (*thr) becomes 0 after being
    // scaled down by (1 + log_scale). See init_qp(). When *thr is 0 and the
    // abs_coeff is 0, the nzflag will be set. As a result, the eob will be
    // incorrectly calculated. The psign instruction corrects the error by
    // zeroing out q/dq if coeff is zero.
    const __m256i z_mask = _mm256_cmpeq_epi16(dq, _mm256_setzero_si256());
    const __m256i nz_mask = _mm256_cmpeq_epi16(z_mask, _mm256_setzero_si256());

    store_coefficients_avx2(q, qcoeff_ptr);
    store_coefficients_avx2(dq, dqcoeff_ptr);

    *eob = get_max_lane_eob(iscan_ptr, *eob, nz_mask);
  } else {
    write_zero(qcoeff_ptr);
    write_zero(dqcoeff_ptr);
  }
}

void av1_quantize_fp_64x64_avx2(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan_ptr, const int16_t *iscan_ptr) {
  (void)scan_ptr;
  (void)zbin_ptr;
  (void)quant_shift_ptr;

  const int log_scale = 2;
  const unsigned int step = 16;
  __m256i qp[3], thr;
  __m256i eob = _mm256_setzero_si256();

  init_qp(round_ptr, quant_ptr, dequant_ptr, log_scale, &thr, qp);

  quantize_fp_64x64(&thr, qp, coeff_ptr, iscan_ptr, qcoeff_ptr, dqcoeff_ptr,
                    &eob);

  coeff_ptr += step;
  qcoeff_ptr += step;
  dqcoeff_ptr += step;
  iscan_ptr += step;
  n_coeffs -= step;

  update_qp(&thr, qp);

  while (n_coeffs > 0) {
    quantize_fp_64x64(&thr, qp, coeff_ptr, iscan_ptr, qcoeff_ptr, dqcoeff_ptr,
                      &eob);

    coeff_ptr += step;
    qcoeff_ptr += step;
    dqcoeff_ptr += step;
    iscan_ptr += step;
    n_coeffs -= step;
  }
  *eob_ptr = quant_gather_eob(eob);
}
