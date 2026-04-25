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

#include <smmintrin.h>
#include <stdint.h>

#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/x86/synonyms.h"

// Coefficient quantization phase 1
// param[0-2] : rounding/quan/dequan constants
static inline void quantize_coeff_phase1(__m128i *coeff, const __m128i *param,
                                         const int shift, const int scale,
                                         __m128i *qcoeff, __m128i *dquan,
                                         __m128i *sign) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i one = _mm_set1_epi32(1);

  *sign = _mm_cmplt_epi32(*coeff, zero);
  *sign = _mm_or_si128(*sign, one);
  *coeff = _mm_abs_epi32(*coeff);

  qcoeff[0] = _mm_add_epi32(*coeff, param[0]);
  qcoeff[1] = _mm_unpackhi_epi32(qcoeff[0], zero);
  qcoeff[0] = _mm_unpacklo_epi32(qcoeff[0], zero);

  qcoeff[0] = _mm_mul_epi32(qcoeff[0], param[1]);
  qcoeff[0] = _mm_srli_epi64(qcoeff[0], shift);
  dquan[0] = _mm_mul_epi32(qcoeff[0], param[2]);
  dquan[0] = _mm_srli_epi64(dquan[0], scale);
  const __m128i abs_s = _mm_slli_epi32(*coeff, 1 + scale);
  qcoeff[2] = _mm_cmplt_epi32(abs_s, param[3]);
}

// Coefficient quantization phase 2
static inline void quantize_coeff_phase2(__m128i *qcoeff, __m128i *dquan,
                                         const __m128i *sign,
                                         const __m128i *param, const int shift,
                                         const int scale, tran_low_t *qAddr,
                                         tran_low_t *dqAddr) {
  __m128i mask0L = _mm_set_epi32(-1, -1, 0, 0);
  __m128i mask0H = _mm_set_epi32(0, 0, -1, -1);

  qcoeff[1] = _mm_mul_epi32(qcoeff[1], param[1]);
  qcoeff[1] = _mm_srli_epi64(qcoeff[1], shift);
  dquan[1] = _mm_mul_epi32(qcoeff[1], param[2]);
  dquan[1] = _mm_srli_epi64(dquan[1], scale);

  // combine L&H
  qcoeff[0] = _mm_shuffle_epi32(qcoeff[0], 0xd8);
  qcoeff[1] = _mm_shuffle_epi32(qcoeff[1], 0x8d);

  qcoeff[0] = _mm_and_si128(qcoeff[0], mask0H);
  qcoeff[1] = _mm_and_si128(qcoeff[1], mask0L);

  dquan[0] = _mm_shuffle_epi32(dquan[0], 0xd8);
  dquan[1] = _mm_shuffle_epi32(dquan[1], 0x8d);

  dquan[0] = _mm_and_si128(dquan[0], mask0H);
  dquan[1] = _mm_and_si128(dquan[1], mask0L);

  qcoeff[0] = _mm_or_si128(qcoeff[0], qcoeff[1]);
  dquan[0] = _mm_or_si128(dquan[0], dquan[1]);

  qcoeff[0] = _mm_sign_epi32(qcoeff[0], *sign);
  dquan[0] = _mm_sign_epi32(dquan[0], *sign);
  qcoeff[0] = _mm_andnot_si128(qcoeff[2], qcoeff[0]);
  dquan[0] = _mm_andnot_si128(qcoeff[2], dquan[0]);
  _mm_storeu_si128((__m128i *)qAddr, qcoeff[0]);
  _mm_storeu_si128((__m128i *)dqAddr, dquan[0]);
}

static inline void find_eob(tran_low_t *qcoeff_ptr, const int16_t *iscan,
                            __m128i *eob) {
  const __m128i zero = _mm_setzero_si128();
  __m128i mask, iscanIdx;
  const __m128i q0 = _mm_loadu_si128((__m128i const *)qcoeff_ptr);
  const __m128i q1 = _mm_loadu_si128((__m128i const *)(qcoeff_ptr + 4));
  __m128i nz_flag0 = _mm_cmpeq_epi32(q0, zero);
  __m128i nz_flag1 = _mm_cmpeq_epi32(q1, zero);

  nz_flag0 = _mm_cmpeq_epi32(nz_flag0, zero);
  nz_flag1 = _mm_cmpeq_epi32(nz_flag1, zero);

  mask = _mm_packs_epi32(nz_flag0, nz_flag1);
  iscanIdx = _mm_loadu_si128((__m128i const *)iscan);
  iscanIdx = _mm_sub_epi16(iscanIdx, mask);
  iscanIdx = _mm_and_si128(iscanIdx, mask);
  *eob = _mm_max_epi16(*eob, iscanIdx);
}

static inline uint16_t get_accumulated_eob(__m128i *eob) {
  __m128i eob_shuffled;
  uint16_t eobValue;
  eob_shuffled = _mm_shuffle_epi32(*eob, 0xe);
  *eob = _mm_max_epi16(*eob, eob_shuffled);
  eob_shuffled = _mm_shufflelo_epi16(*eob, 0xe);
  *eob = _mm_max_epi16(*eob, eob_shuffled);
  eob_shuffled = _mm_shufflelo_epi16(*eob, 0x1);
  *eob = _mm_max_epi16(*eob, eob_shuffled);
  eobValue = _mm_extract_epi16(*eob, 0);
  return eobValue;
}

void av1_highbd_quantize_fp_sse4_1(
    const tran_low_t *coeff_ptr, intptr_t count, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan, int log_scale) {
  __m128i coeff[2], qcoeff[3], dequant[2], qparam[4], coeff_sign;
  __m128i eob = _mm_setzero_si128();
  const tran_low_t *src = coeff_ptr;
  tran_low_t *quanAddr = qcoeff_ptr;
  tran_low_t *dquanAddr = dqcoeff_ptr;
  const int shift = 16 - log_scale;
  const int coeff_stride = 4;
  const int quan_stride = coeff_stride;
  (void)zbin_ptr;
  (void)quant_shift_ptr;
  (void)scan;

  memset(quanAddr, 0, count * sizeof(quanAddr[0]));
  memset(dquanAddr, 0, count * sizeof(dquanAddr[0]));

  coeff[0] = _mm_loadu_si128((__m128i const *)src);
  const int round1 = ROUND_POWER_OF_TWO(round_ptr[1], log_scale);
  const int round0 = ROUND_POWER_OF_TWO(round_ptr[0], log_scale);

  qparam[0] = _mm_set_epi32(round1, round1, round1, round0);
  qparam[1] = _mm_set_epi64x((uint32_t)quant_ptr[1], (uint32_t)quant_ptr[0]);
  qparam[2] =
      _mm_set_epi64x((uint32_t)dequant_ptr[1], (uint32_t)dequant_ptr[0]);
  qparam[3] = _mm_set_epi32(dequant_ptr[1], dequant_ptr[1], dequant_ptr[1],
                            dequant_ptr[0]);

  // DC and first 3 AC
  quantize_coeff_phase1(&coeff[0], qparam, shift, log_scale, qcoeff, dequant,
                        &coeff_sign);

  // update round/quan/dquan for AC
  qparam[0] = _mm_unpackhi_epi64(qparam[0], qparam[0]);
  qparam[1] = _mm_set1_epi64x((uint32_t)quant_ptr[1]);
  qparam[2] = _mm_set1_epi64x((uint32_t)dequant_ptr[1]);
  qparam[3] = _mm_set1_epi32(dequant_ptr[1]);
  quantize_coeff_phase2(qcoeff, dequant, &coeff_sign, qparam, shift, log_scale,
                        quanAddr, dquanAddr);

  // next 4 AC
  coeff[1] = _mm_loadu_si128((__m128i const *)(src + coeff_stride));
  quantize_coeff_phase1(&coeff[1], qparam, shift, log_scale, qcoeff, dequant,
                        &coeff_sign);
  quantize_coeff_phase2(qcoeff, dequant, &coeff_sign, qparam, shift, log_scale,
                        quanAddr + quan_stride, dquanAddr + quan_stride);

  find_eob(quanAddr, iscan, &eob);

  count -= 8;

  // loop for the rest of AC
  while (count > 0) {
    src += coeff_stride << 1;
    quanAddr += quan_stride << 1;
    dquanAddr += quan_stride << 1;
    iscan += quan_stride << 1;

    coeff[0] = _mm_loadu_si128((__m128i const *)src);
    coeff[1] = _mm_loadu_si128((__m128i const *)(src + coeff_stride));

    quantize_coeff_phase1(&coeff[0], qparam, shift, log_scale, qcoeff, dequant,
                          &coeff_sign);
    quantize_coeff_phase2(qcoeff, dequant, &coeff_sign, qparam, shift,
                          log_scale, quanAddr, dquanAddr);

    quantize_coeff_phase1(&coeff[1], qparam, shift, log_scale, qcoeff, dequant,
                          &coeff_sign);
    quantize_coeff_phase2(qcoeff, dequant, &coeff_sign, qparam, shift,
                          log_scale, quanAddr + quan_stride,
                          dquanAddr + quan_stride);

    find_eob(quanAddr, iscan, &eob);

    count -= 8;
  }
  *eob_ptr = get_accumulated_eob(&eob);
}
