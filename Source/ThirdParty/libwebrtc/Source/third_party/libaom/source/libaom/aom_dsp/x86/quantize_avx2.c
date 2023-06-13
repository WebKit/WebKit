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

#include <immintrin.h>
#include "config/aom_dsp_rtcd.h"
#include "aom/aom_integer.h"
#include "aom_dsp/x86/quantize_x86.h"

static INLINE void load_b_values_avx2(const int16_t *zbin_ptr, __m256i *zbin,
                                      const int16_t *round_ptr, __m256i *round,
                                      const int16_t *quant_ptr, __m256i *quant,
                                      const int16_t *dequant_ptr,
                                      __m256i *dequant,
                                      const int16_t *shift_ptr, __m256i *shift,
                                      int log_scale) {
  *zbin = _mm256_castsi128_si256(_mm_load_si128((const __m128i *)zbin_ptr));
  *zbin = _mm256_permute4x64_epi64(*zbin, 0x54);
  if (log_scale > 0) {
    const __m256i rnd = _mm256_set1_epi16((int16_t)(1 << (log_scale - 1)));
    *zbin = _mm256_add_epi16(*zbin, rnd);
    *zbin = _mm256_srai_epi16(*zbin, log_scale);
  }
  // Subtracting 1 here eliminates a _mm256_cmpeq_epi16() instruction when
  // calculating the zbin mask. (See quantize_b_logscale{0,1,2}_16)
  *zbin = _mm256_sub_epi16(*zbin, _mm256_set1_epi16(1));

  *round = _mm256_castsi128_si256(_mm_load_si128((const __m128i *)round_ptr));
  *round = _mm256_permute4x64_epi64(*round, 0x54);
  if (log_scale > 0) {
    const __m256i rnd = _mm256_set1_epi16((int16_t)(1 << (log_scale - 1)));
    *round = _mm256_add_epi16(*round, rnd);
    *round = _mm256_srai_epi16(*round, log_scale);
  }

  *quant = _mm256_castsi128_si256(_mm_load_si128((const __m128i *)quant_ptr));
  *quant = _mm256_permute4x64_epi64(*quant, 0x54);
  *dequant =
      _mm256_castsi128_si256(_mm_load_si128((const __m128i *)dequant_ptr));
  *dequant = _mm256_permute4x64_epi64(*dequant, 0x54);
  *shift = _mm256_castsi128_si256(_mm_load_si128((const __m128i *)shift_ptr));
  *shift = _mm256_permute4x64_epi64(*shift, 0x54);
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

static AOM_FORCE_INLINE __m256i quantize_b_logscale0_16(
    const tran_low_t *coeff_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, __m256i *v_quant, __m256i *v_dequant,
    __m256i *v_round, __m256i *v_zbin, __m256i *v_quant_shift) {
  const __m256i v_coeff = load_coefficients_avx2(coeff_ptr);
  const __m256i v_abs_coeff = _mm256_abs_epi16(v_coeff);
  const __m256i v_zbin_mask = _mm256_cmpgt_epi16(v_abs_coeff, *v_zbin);

  if (_mm256_movemask_epi8(v_zbin_mask) == 0) {
    _mm256_store_si256((__m256i *)qcoeff_ptr, _mm256_setzero_si256());
    _mm256_store_si256((__m256i *)dqcoeff_ptr, _mm256_setzero_si256());
    _mm256_store_si256((__m256i *)(qcoeff_ptr + 8), _mm256_setzero_si256());
    _mm256_store_si256((__m256i *)(dqcoeff_ptr + 8), _mm256_setzero_si256());
    return _mm256_setzero_si256();
  }

  // tmp = v_zbin_mask ? (int64_t)abs_coeff + log_scaled_round : 0
  const __m256i v_tmp_rnd =
      _mm256_and_si256(_mm256_adds_epi16(v_abs_coeff, *v_round), v_zbin_mask);
  //  tmp32 = (int)(((((tmp * quant_ptr[rc != 0]) >> 16) + tmp) *
  //                 quant_shift_ptr[rc != 0]) >>
  //                (16 - log_scale + AOM_QM_BITS));
  const __m256i v_tmp32_a = _mm256_mulhi_epi16(v_tmp_rnd, *v_quant);
  const __m256i v_tmp32_b = _mm256_add_epi16(v_tmp32_a, v_tmp_rnd);
  const __m256i v_tmp32 = _mm256_mulhi_epi16(v_tmp32_b, *v_quant_shift);
  const __m256i v_nz_mask = _mm256_cmpgt_epi16(v_tmp32, _mm256_setzero_si256());
  const __m256i v_qcoeff = _mm256_sign_epi16(v_tmp32, v_coeff);
  const __m256i v_dqcoeff = _mm256_mullo_epi16(v_qcoeff, *v_dequant);
  store_coefficients_avx2(v_qcoeff, qcoeff_ptr);
  store_coefficients_avx2(v_dqcoeff, dqcoeff_ptr);
  return v_nz_mask;
}

static INLINE __m256i get_max_lane_eob(const int16_t *iscan, __m256i v_eobmax,
                                       __m256i v_mask) {
  const __m256i v_iscan = _mm256_loadu_si256((const __m256i *)iscan);
  const __m256i v_iscan_perm = _mm256_permute4x64_epi64(v_iscan, 0xD8);
  const __m256i v_iscan_plus1 = _mm256_sub_epi16(v_iscan_perm, v_mask);
  const __m256i v_nz_iscan = _mm256_and_si256(v_iscan_plus1, v_mask);
  return _mm256_max_epi16(v_eobmax, v_nz_iscan);
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

void aom_quantize_b_avx2(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                         const int16_t *zbin_ptr, const int16_t *round_ptr,
                         const int16_t *quant_ptr,
                         const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
                         tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr,
                         uint16_t *eob_ptr, const int16_t *scan,
                         const int16_t *iscan) {
  (void)scan;
  __m256i v_zbin, v_round, v_quant, v_dequant, v_quant_shift;
  __m256i v_eobmax = _mm256_setzero_si256();

  load_b_values_avx2(zbin_ptr, &v_zbin, round_ptr, &v_round, quant_ptr,
                     &v_quant, dequant_ptr, &v_dequant, quant_shift_ptr,
                     &v_quant_shift, 0);

  // Do DC and first 15 AC.
  __m256i v_nz_mask =
      quantize_b_logscale0_16(coeff_ptr, qcoeff_ptr, dqcoeff_ptr, &v_quant,
                              &v_dequant, &v_round, &v_zbin, &v_quant_shift);

  v_eobmax = get_max_lane_eob(iscan, v_eobmax, v_nz_mask);

  v_round = _mm256_unpackhi_epi64(v_round, v_round);
  v_quant = _mm256_unpackhi_epi64(v_quant, v_quant);
  v_dequant = _mm256_unpackhi_epi64(v_dequant, v_dequant);
  v_quant_shift = _mm256_unpackhi_epi64(v_quant_shift, v_quant_shift);
  v_zbin = _mm256_unpackhi_epi64(v_zbin, v_zbin);

  for (intptr_t count = n_coeffs - 16; count > 0; count -= 16) {
    coeff_ptr += 16;
    qcoeff_ptr += 16;
    dqcoeff_ptr += 16;
    iscan += 16;
    v_nz_mask =
        quantize_b_logscale0_16(coeff_ptr, qcoeff_ptr, dqcoeff_ptr, &v_quant,
                                &v_dequant, &v_round, &v_zbin, &v_quant_shift);

    v_eobmax = get_max_lane_eob(iscan, v_eobmax, v_nz_mask);
  }

  *eob_ptr = accumulate_eob256(v_eobmax);
}

static AOM_FORCE_INLINE __m256i quantize_b_logscale_16(
    const tran_low_t *coeff_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, __m256i *v_quant, __m256i *v_dequant,
    __m256i *v_round, __m256i *v_zbin, __m256i *v_quant_shift, int log_scale) {
  const __m256i v_coeff = load_coefficients_avx2(coeff_ptr);
  const __m256i v_abs_coeff = _mm256_abs_epi16(v_coeff);
  const __m256i v_zbin_mask = _mm256_cmpgt_epi16(v_abs_coeff, *v_zbin);

  if (_mm256_movemask_epi8(v_zbin_mask) == 0) {
    _mm256_store_si256((__m256i *)qcoeff_ptr, _mm256_setzero_si256());
    _mm256_store_si256((__m256i *)dqcoeff_ptr, _mm256_setzero_si256());
    _mm256_store_si256((__m256i *)(qcoeff_ptr + 8), _mm256_setzero_si256());
    _mm256_store_si256((__m256i *)(dqcoeff_ptr + 8), _mm256_setzero_si256());
    return _mm256_setzero_si256();
  }

  // tmp = v_zbin_mask ? (int64_t)abs_coeff + log_scaled_round : 0
  const __m256i v_tmp_rnd =
      _mm256_and_si256(_mm256_adds_epi16(v_abs_coeff, *v_round), v_zbin_mask);
  //  tmp32 = (int)(((((tmp * quant_ptr[rc != 0]) >> 16) + tmp) *
  //                 quant_shift_ptr[rc != 0]) >>
  //                (16 - log_scale + AOM_QM_BITS));
  const __m256i v_tmp32_a = _mm256_mulhi_epi16(v_tmp_rnd, *v_quant);
  const __m256i v_tmp32_b = _mm256_add_epi16(v_tmp32_a, v_tmp_rnd);
  const __m256i v_tmp32_hi = _mm256_slli_epi16(
      _mm256_mulhi_epi16(v_tmp32_b, *v_quant_shift), log_scale);
  const __m256i v_tmp32_lo = _mm256_srli_epi16(
      _mm256_mullo_epi16(v_tmp32_b, *v_quant_shift), 16 - log_scale);
  const __m256i v_tmp32 = _mm256_or_si256(v_tmp32_hi, v_tmp32_lo);
  const __m256i v_dqcoeff_hi = _mm256_slli_epi16(
      _mm256_mulhi_epi16(v_tmp32, *v_dequant), 16 - log_scale);
  const __m256i v_dqcoeff_lo =
      _mm256_srli_epi16(_mm256_mullo_epi16(v_tmp32, *v_dequant), log_scale);
  const __m256i v_dqcoeff =
      _mm256_sign_epi16(_mm256_or_si256(v_dqcoeff_hi, v_dqcoeff_lo), v_coeff);
  const __m256i v_qcoeff = _mm256_sign_epi16(v_tmp32, v_coeff);
  const __m256i v_nz_mask = _mm256_cmpgt_epi16(v_tmp32, _mm256_setzero_si256());
  store_coefficients_avx2(v_qcoeff, qcoeff_ptr);
  store_coefficients_avx2(v_dqcoeff, dqcoeff_ptr);
  return v_nz_mask;
}

static AOM_FORCE_INLINE void quantize_b_no_qmatrix_avx2(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *iscan, int log_scale) {
  __m256i v_zbin, v_round, v_quant, v_dequant, v_quant_shift;
  __m256i v_eobmax = _mm256_setzero_si256();

  load_b_values_avx2(zbin_ptr, &v_zbin, round_ptr, &v_round, quant_ptr,
                     &v_quant, dequant_ptr, &v_dequant, quant_shift_ptr,
                     &v_quant_shift, log_scale);

  // Do DC and first 15 AC.
  __m256i v_nz_mask = quantize_b_logscale_16(
      coeff_ptr, qcoeff_ptr, dqcoeff_ptr, &v_quant, &v_dequant, &v_round,
      &v_zbin, &v_quant_shift, log_scale);

  v_eobmax = get_max_lane_eob(iscan, v_eobmax, v_nz_mask);

  v_round = _mm256_unpackhi_epi64(v_round, v_round);
  v_quant = _mm256_unpackhi_epi64(v_quant, v_quant);
  v_dequant = _mm256_unpackhi_epi64(v_dequant, v_dequant);
  v_quant_shift = _mm256_unpackhi_epi64(v_quant_shift, v_quant_shift);
  v_zbin = _mm256_unpackhi_epi64(v_zbin, v_zbin);

  for (intptr_t count = n_coeffs - 16; count > 0; count -= 16) {
    coeff_ptr += 16;
    qcoeff_ptr += 16;
    dqcoeff_ptr += 16;
    iscan += 16;
    v_nz_mask = quantize_b_logscale_16(coeff_ptr, qcoeff_ptr, dqcoeff_ptr,
                                       &v_quant, &v_dequant, &v_round, &v_zbin,
                                       &v_quant_shift, log_scale);

    v_eobmax = get_max_lane_eob(iscan, v_eobmax, v_nz_mask);
  }

  *eob_ptr = accumulate_eob256(v_eobmax);
}

void aom_quantize_b_32x32_avx2(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                               const int16_t *zbin_ptr,
                               const int16_t *round_ptr,
                               const int16_t *quant_ptr,
                               const int16_t *quant_shift_ptr,
                               tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                               const int16_t *dequant_ptr, uint16_t *eob_ptr,
                               const int16_t *scan, const int16_t *iscan) {
  (void)scan;
  quantize_b_no_qmatrix_avx2(coeff_ptr, n_coeffs, zbin_ptr, round_ptr,
                             quant_ptr, quant_shift_ptr, qcoeff_ptr,
                             dqcoeff_ptr, dequant_ptr, eob_ptr, iscan, 1);
}

void aom_quantize_b_64x64_avx2(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                               const int16_t *zbin_ptr,
                               const int16_t *round_ptr,
                               const int16_t *quant_ptr,
                               const int16_t *quant_shift_ptr,
                               tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                               const int16_t *dequant_ptr, uint16_t *eob_ptr,
                               const int16_t *scan, const int16_t *iscan) {
  (void)scan;
  quantize_b_no_qmatrix_avx2(coeff_ptr, n_coeffs, zbin_ptr, round_ptr,
                             quant_ptr, quant_shift_ptr, qcoeff_ptr,
                             dqcoeff_ptr, dequant_ptr, eob_ptr, iscan, 2);
}
