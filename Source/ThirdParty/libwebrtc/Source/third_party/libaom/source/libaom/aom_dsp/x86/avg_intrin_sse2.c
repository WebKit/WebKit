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

#include <immintrin.h>

#include "config/aom_dsp_rtcd.h"
#include "aom/aom_integer.h"
#include "aom_dsp/x86/bitdepth_conversion_sse2.h"
#include "aom_dsp/x86/mem_sse2.h"
#include "aom_ports/mem.h"

static INLINE void sign_extend_16bit_to_32bit_sse2(__m128i in, __m128i zero,
                                                   __m128i *out_lo,
                                                   __m128i *out_hi) {
  const __m128i sign_bits = _mm_cmplt_epi16(in, zero);
  *out_lo = _mm_unpacklo_epi16(in, sign_bits);
  *out_hi = _mm_unpackhi_epi16(in, sign_bits);
}

static INLINE __m128i invert_sign_32_sse2(__m128i a, __m128i sign) {
  a = _mm_xor_si128(a, sign);
  return _mm_sub_epi32(a, sign);
}

void aom_minmax_8x8_sse2(const uint8_t *s, int p, const uint8_t *d, int dp,
                         int *min, int *max) {
  __m128i u0, s0, d0, diff, maxabsdiff, minabsdiff, negdiff, absdiff0, absdiff;
  u0 = _mm_setzero_si128();
  // Row 0
  s0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(s)), u0);
  d0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(d)), u0);
  diff = _mm_subs_epi16(s0, d0);
  negdiff = _mm_subs_epi16(u0, diff);
  absdiff0 = _mm_max_epi16(diff, negdiff);
  // Row 1
  s0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(s + p)), u0);
  d0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(d + dp)), u0);
  diff = _mm_subs_epi16(s0, d0);
  negdiff = _mm_subs_epi16(u0, diff);
  absdiff = _mm_max_epi16(diff, negdiff);
  maxabsdiff = _mm_max_epi16(absdiff0, absdiff);
  minabsdiff = _mm_min_epi16(absdiff0, absdiff);
  // Row 2
  s0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(s + 2 * p)), u0);
  d0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(d + 2 * dp)), u0);
  diff = _mm_subs_epi16(s0, d0);
  negdiff = _mm_subs_epi16(u0, diff);
  absdiff = _mm_max_epi16(diff, negdiff);
  maxabsdiff = _mm_max_epi16(maxabsdiff, absdiff);
  minabsdiff = _mm_min_epi16(minabsdiff, absdiff);
  // Row 3
  s0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(s + 3 * p)), u0);
  d0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(d + 3 * dp)), u0);
  diff = _mm_subs_epi16(s0, d0);
  negdiff = _mm_subs_epi16(u0, diff);
  absdiff = _mm_max_epi16(diff, negdiff);
  maxabsdiff = _mm_max_epi16(maxabsdiff, absdiff);
  minabsdiff = _mm_min_epi16(minabsdiff, absdiff);
  // Row 4
  s0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(s + 4 * p)), u0);
  d0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(d + 4 * dp)), u0);
  diff = _mm_subs_epi16(s0, d0);
  negdiff = _mm_subs_epi16(u0, diff);
  absdiff = _mm_max_epi16(diff, negdiff);
  maxabsdiff = _mm_max_epi16(maxabsdiff, absdiff);
  minabsdiff = _mm_min_epi16(minabsdiff, absdiff);
  // Row 5
  s0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(s + 5 * p)), u0);
  d0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(d + 5 * dp)), u0);
  diff = _mm_subs_epi16(s0, d0);
  negdiff = _mm_subs_epi16(u0, diff);
  absdiff = _mm_max_epi16(diff, negdiff);
  maxabsdiff = _mm_max_epi16(maxabsdiff, absdiff);
  minabsdiff = _mm_min_epi16(minabsdiff, absdiff);
  // Row 6
  s0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(s + 6 * p)), u0);
  d0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(d + 6 * dp)), u0);
  diff = _mm_subs_epi16(s0, d0);
  negdiff = _mm_subs_epi16(u0, diff);
  absdiff = _mm_max_epi16(diff, negdiff);
  maxabsdiff = _mm_max_epi16(maxabsdiff, absdiff);
  minabsdiff = _mm_min_epi16(minabsdiff, absdiff);
  // Row 7
  s0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(s + 7 * p)), u0);
  d0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(d + 7 * dp)), u0);
  diff = _mm_subs_epi16(s0, d0);
  negdiff = _mm_subs_epi16(u0, diff);
  absdiff = _mm_max_epi16(diff, negdiff);
  maxabsdiff = _mm_max_epi16(maxabsdiff, absdiff);
  minabsdiff = _mm_min_epi16(minabsdiff, absdiff);

  maxabsdiff = _mm_max_epi16(maxabsdiff, _mm_srli_si128(maxabsdiff, 8));
  maxabsdiff = _mm_max_epi16(maxabsdiff, _mm_srli_epi64(maxabsdiff, 32));
  maxabsdiff = _mm_max_epi16(maxabsdiff, _mm_srli_epi64(maxabsdiff, 16));
  *max = _mm_extract_epi16(maxabsdiff, 0);

  minabsdiff = _mm_min_epi16(minabsdiff, _mm_srli_si128(minabsdiff, 8));
  minabsdiff = _mm_min_epi16(minabsdiff, _mm_srli_epi64(minabsdiff, 32));
  minabsdiff = _mm_min_epi16(minabsdiff, _mm_srli_epi64(minabsdiff, 16));
  *min = _mm_extract_epi16(minabsdiff, 0);
}

unsigned int aom_avg_8x8_sse2(const uint8_t *s, int p) {
  __m128i sum0, sum1, s0, s1, s2, s3, u0;
  unsigned int avg = 0;
  u0 = _mm_setzero_si128();
  s0 = loadh_epi64((const __m128i *)(s + p),
                   _mm_loadl_epi64((const __m128i *)(s)));
  s1 = loadh_epi64((const __m128i *)(s + 3 * p),
                   _mm_loadl_epi64((const __m128i *)(s + 2 * p)));
  s2 = loadh_epi64((const __m128i *)(s + 5 * p),
                   _mm_loadl_epi64((const __m128i *)(s + 4 * p)));
  s3 = loadh_epi64((const __m128i *)(s + 7 * p),
                   _mm_loadl_epi64((const __m128i *)(s + 6 * p)));
  s0 = _mm_sad_epu8(s0, u0);
  s1 = _mm_sad_epu8(s1, u0);
  s2 = _mm_sad_epu8(s2, u0);
  s3 = _mm_sad_epu8(s3, u0);

  sum0 = _mm_add_epi16(s0, s1);
  sum1 = _mm_add_epi16(s2, s3);
  sum0 = _mm_add_epi16(sum0, sum1);
  sum0 = _mm_add_epi16(sum0, _mm_srli_si128(sum0, 8));
  avg = _mm_cvtsi128_si32(sum0);
  return (avg + 32) >> 6;
}

static void calc_avg_8x8_dual_sse2(const uint8_t *s, int p, int *avg) {
  __m128i sum0, sum1, s0, s1, s2, s3, u0;
  u0 = _mm_setzero_si128();
  s0 = _mm_sad_epu8(_mm_loadu_si128((const __m128i *)(s)), u0);
  s1 = _mm_sad_epu8(_mm_loadu_si128((const __m128i *)(s + p)), u0);
  s2 = _mm_sad_epu8(_mm_loadu_si128((const __m128i *)(s + 2 * p)), u0);
  s3 = _mm_sad_epu8(_mm_loadu_si128((const __m128i *)(s + 3 * p)), u0);
  sum0 = _mm_add_epi16(s0, s1);
  sum1 = _mm_add_epi16(s2, s3);
  s0 = _mm_sad_epu8(_mm_loadu_si128((const __m128i *)(s + 4 * p)), u0);
  s1 = _mm_sad_epu8(_mm_loadu_si128((const __m128i *)(s + 5 * p)), u0);
  s2 = _mm_sad_epu8(_mm_loadu_si128((const __m128i *)(s + 6 * p)), u0);
  s3 = _mm_sad_epu8(_mm_loadu_si128((const __m128i *)(s + 7 * p)), u0);
  sum0 = _mm_add_epi16(sum0, _mm_add_epi16(s0, s1));
  sum1 = _mm_add_epi16(sum1, _mm_add_epi16(s2, s3));
  sum0 = _mm_add_epi16(sum0, sum1);

  // (avg + 32) >> 6
  __m128i rounding = _mm_set1_epi32(32);
  sum0 = _mm_add_epi32(sum0, rounding);
  sum0 = _mm_srli_epi32(sum0, 6);
  avg[0] = _mm_cvtsi128_si32(sum0);
  avg[1] = _mm_extract_epi16(sum0, 4);
}

void aom_avg_8x8_quad_sse2(const uint8_t *s, int p, int x16_idx, int y16_idx,
                           int *avg) {
  const uint8_t *s_ptr = s + y16_idx * p + x16_idx;
  for (int k = 0; k < 2; k++) {
    calc_avg_8x8_dual_sse2(s_ptr, p, avg + k * 2);
    s_ptr += 8 * p;
  }
}

unsigned int aom_avg_4x4_sse2(const uint8_t *s, int p) {
  __m128i s0, s1, u0;
  unsigned int avg = 0;
  u0 = _mm_setzero_si128();
  s0 = _mm_unpacklo_epi32(_mm_cvtsi32_si128(*(const int *)(s)),
                          _mm_cvtsi32_si128(*(const int *)(s + p)));
  s1 = _mm_unpacklo_epi32(_mm_cvtsi32_si128(*(const int *)(s + p * 2)),
                          _mm_cvtsi32_si128(*(const int *)(s + p * 3)));
  s0 = _mm_sad_epu8(s0, u0);
  s1 = _mm_sad_epu8(s1, u0);
  s0 = _mm_add_epi16(s0, s1);
  avg = _mm_cvtsi128_si32(s0);
  return (avg + 8) >> 4;
}

static INLINE void hadamard_col4_sse2(__m128i *in, int iter) {
  const __m128i a0 = in[0];
  const __m128i a1 = in[1];
  const __m128i a2 = in[2];
  const __m128i a3 = in[3];
  const __m128i b0 = _mm_srai_epi16(_mm_add_epi16(a0, a1), 1);
  const __m128i b1 = _mm_srai_epi16(_mm_sub_epi16(a0, a1), 1);
  const __m128i b2 = _mm_srai_epi16(_mm_add_epi16(a2, a3), 1);
  const __m128i b3 = _mm_srai_epi16(_mm_sub_epi16(a2, a3), 1);
  in[0] = _mm_add_epi16(b0, b2);
  in[1] = _mm_add_epi16(b1, b3);
  in[2] = _mm_sub_epi16(b0, b2);
  in[3] = _mm_sub_epi16(b1, b3);

  if (iter == 0) {
    const __m128i ba = _mm_unpacklo_epi16(in[0], in[1]);
    const __m128i dc = _mm_unpacklo_epi16(in[2], in[3]);
    const __m128i dcba_lo = _mm_unpacklo_epi32(ba, dc);
    const __m128i dcba_hi = _mm_unpackhi_epi32(ba, dc);
    in[0] = dcba_lo;
    in[1] = _mm_srli_si128(dcba_lo, 8);
    in[2] = dcba_hi;
    in[3] = _mm_srli_si128(dcba_hi, 8);
  }
}

void aom_hadamard_4x4_sse2(const int16_t *src_diff, ptrdiff_t src_stride,
                           tran_low_t *coeff) {
  __m128i src[4];
  src[0] = _mm_loadl_epi64((const __m128i *)src_diff);
  src[1] = _mm_loadl_epi64((const __m128i *)(src_diff += src_stride));
  src[2] = _mm_loadl_epi64((const __m128i *)(src_diff += src_stride));
  src[3] = _mm_loadl_epi64((const __m128i *)(src_diff + src_stride));

  hadamard_col4_sse2(src, 0);
  hadamard_col4_sse2(src, 1);

  store_tran_low(_mm_unpacklo_epi64(src[0], src[1]), coeff);
  coeff += 8;
  store_tran_low(_mm_unpacklo_epi64(src[2], src[3]), coeff);
}

static INLINE void hadamard_col8_sse2(__m128i *in, int iter) {
  __m128i a0 = in[0];
  __m128i a1 = in[1];
  __m128i a2 = in[2];
  __m128i a3 = in[3];
  __m128i a4 = in[4];
  __m128i a5 = in[5];
  __m128i a6 = in[6];
  __m128i a7 = in[7];

  __m128i b0 = _mm_add_epi16(a0, a1);
  __m128i b1 = _mm_sub_epi16(a0, a1);
  __m128i b2 = _mm_add_epi16(a2, a3);
  __m128i b3 = _mm_sub_epi16(a2, a3);
  __m128i b4 = _mm_add_epi16(a4, a5);
  __m128i b5 = _mm_sub_epi16(a4, a5);
  __m128i b6 = _mm_add_epi16(a6, a7);
  __m128i b7 = _mm_sub_epi16(a6, a7);

  a0 = _mm_add_epi16(b0, b2);
  a1 = _mm_add_epi16(b1, b3);
  a2 = _mm_sub_epi16(b0, b2);
  a3 = _mm_sub_epi16(b1, b3);
  a4 = _mm_add_epi16(b4, b6);
  a5 = _mm_add_epi16(b5, b7);
  a6 = _mm_sub_epi16(b4, b6);
  a7 = _mm_sub_epi16(b5, b7);

  if (iter == 0) {
    b0 = _mm_add_epi16(a0, a4);
    b7 = _mm_add_epi16(a1, a5);
    b3 = _mm_add_epi16(a2, a6);
    b4 = _mm_add_epi16(a3, a7);
    b2 = _mm_sub_epi16(a0, a4);
    b6 = _mm_sub_epi16(a1, a5);
    b1 = _mm_sub_epi16(a2, a6);
    b5 = _mm_sub_epi16(a3, a7);

    a0 = _mm_unpacklo_epi16(b0, b1);
    a1 = _mm_unpacklo_epi16(b2, b3);
    a2 = _mm_unpackhi_epi16(b0, b1);
    a3 = _mm_unpackhi_epi16(b2, b3);
    a4 = _mm_unpacklo_epi16(b4, b5);
    a5 = _mm_unpacklo_epi16(b6, b7);
    a6 = _mm_unpackhi_epi16(b4, b5);
    a7 = _mm_unpackhi_epi16(b6, b7);

    b0 = _mm_unpacklo_epi32(a0, a1);
    b1 = _mm_unpacklo_epi32(a4, a5);
    b2 = _mm_unpackhi_epi32(a0, a1);
    b3 = _mm_unpackhi_epi32(a4, a5);
    b4 = _mm_unpacklo_epi32(a2, a3);
    b5 = _mm_unpacklo_epi32(a6, a7);
    b6 = _mm_unpackhi_epi32(a2, a3);
    b7 = _mm_unpackhi_epi32(a6, a7);

    in[0] = _mm_unpacklo_epi64(b0, b1);
    in[1] = _mm_unpackhi_epi64(b0, b1);
    in[2] = _mm_unpacklo_epi64(b2, b3);
    in[3] = _mm_unpackhi_epi64(b2, b3);
    in[4] = _mm_unpacklo_epi64(b4, b5);
    in[5] = _mm_unpackhi_epi64(b4, b5);
    in[6] = _mm_unpacklo_epi64(b6, b7);
    in[7] = _mm_unpackhi_epi64(b6, b7);
  } else {
    in[0] = _mm_add_epi16(a0, a4);
    in[7] = _mm_add_epi16(a1, a5);
    in[3] = _mm_add_epi16(a2, a6);
    in[4] = _mm_add_epi16(a3, a7);
    in[2] = _mm_sub_epi16(a0, a4);
    in[6] = _mm_sub_epi16(a1, a5);
    in[1] = _mm_sub_epi16(a2, a6);
    in[5] = _mm_sub_epi16(a3, a7);
  }
}

static INLINE void hadamard_8x8_sse2(const int16_t *src_diff,
                                     ptrdiff_t src_stride, tran_low_t *coeff,
                                     int is_final) {
  __m128i src[8];
  src[0] = _mm_load_si128((const __m128i *)src_diff);
  src[1] = _mm_load_si128((const __m128i *)(src_diff += src_stride));
  src[2] = _mm_load_si128((const __m128i *)(src_diff += src_stride));
  src[3] = _mm_load_si128((const __m128i *)(src_diff += src_stride));
  src[4] = _mm_load_si128((const __m128i *)(src_diff += src_stride));
  src[5] = _mm_load_si128((const __m128i *)(src_diff += src_stride));
  src[6] = _mm_load_si128((const __m128i *)(src_diff += src_stride));
  src[7] = _mm_load_si128((const __m128i *)(src_diff + src_stride));

  hadamard_col8_sse2(src, 0);
  hadamard_col8_sse2(src, 1);

  if (is_final) {
    store_tran_low(src[0], coeff);
    coeff += 8;
    store_tran_low(src[1], coeff);
    coeff += 8;
    store_tran_low(src[2], coeff);
    coeff += 8;
    store_tran_low(src[3], coeff);
    coeff += 8;
    store_tran_low(src[4], coeff);
    coeff += 8;
    store_tran_low(src[5], coeff);
    coeff += 8;
    store_tran_low(src[6], coeff);
    coeff += 8;
    store_tran_low(src[7], coeff);
  } else {
    int16_t *coeff16 = (int16_t *)coeff;
    _mm_store_si128((__m128i *)coeff16, src[0]);
    coeff16 += 8;
    _mm_store_si128((__m128i *)coeff16, src[1]);
    coeff16 += 8;
    _mm_store_si128((__m128i *)coeff16, src[2]);
    coeff16 += 8;
    _mm_store_si128((__m128i *)coeff16, src[3]);
    coeff16 += 8;
    _mm_store_si128((__m128i *)coeff16, src[4]);
    coeff16 += 8;
    _mm_store_si128((__m128i *)coeff16, src[5]);
    coeff16 += 8;
    _mm_store_si128((__m128i *)coeff16, src[6]);
    coeff16 += 8;
    _mm_store_si128((__m128i *)coeff16, src[7]);
  }
}

void aom_hadamard_8x8_sse2(const int16_t *src_diff, ptrdiff_t src_stride,
                           tran_low_t *coeff) {
  hadamard_8x8_sse2(src_diff, src_stride, coeff, 1);
}

static INLINE void hadamard_lp_8x8_sse2(const int16_t *src_diff,
                                        ptrdiff_t src_stride, int16_t *coeff) {
  __m128i src[8];
  src[0] = _mm_load_si128((const __m128i *)src_diff);
  src[1] = _mm_load_si128((const __m128i *)(src_diff += src_stride));
  src[2] = _mm_load_si128((const __m128i *)(src_diff += src_stride));
  src[3] = _mm_load_si128((const __m128i *)(src_diff += src_stride));
  src[4] = _mm_load_si128((const __m128i *)(src_diff += src_stride));
  src[5] = _mm_load_si128((const __m128i *)(src_diff += src_stride));
  src[6] = _mm_load_si128((const __m128i *)(src_diff += src_stride));
  src[7] = _mm_load_si128((const __m128i *)(src_diff + src_stride));

  hadamard_col8_sse2(src, 0);
  hadamard_col8_sse2(src, 1);

  _mm_store_si128((__m128i *)coeff, src[0]);
  coeff += 8;
  _mm_store_si128((__m128i *)coeff, src[1]);
  coeff += 8;
  _mm_store_si128((__m128i *)coeff, src[2]);
  coeff += 8;
  _mm_store_si128((__m128i *)coeff, src[3]);
  coeff += 8;
  _mm_store_si128((__m128i *)coeff, src[4]);
  coeff += 8;
  _mm_store_si128((__m128i *)coeff, src[5]);
  coeff += 8;
  _mm_store_si128((__m128i *)coeff, src[6]);
  coeff += 8;
  _mm_store_si128((__m128i *)coeff, src[7]);
}

void aom_hadamard_lp_8x8_sse2(const int16_t *src_diff, ptrdiff_t src_stride,
                              int16_t *coeff) {
  hadamard_lp_8x8_sse2(src_diff, src_stride, coeff);
}

void aom_hadamard_lp_8x8_dual_sse2(const int16_t *src_diff,
                                   ptrdiff_t src_stride, int16_t *coeff) {
  for (int i = 0; i < 2; i++) {
    hadamard_lp_8x8_sse2(src_diff + (i * 8), src_stride, coeff + (i * 64));
  }
}

void aom_hadamard_lp_16x16_sse2(const int16_t *src_diff, ptrdiff_t src_stride,
                                int16_t *coeff) {
  for (int idx = 0; idx < 4; ++idx) {
    const int16_t *src_ptr =
        src_diff + (idx >> 1) * 8 * src_stride + (idx & 0x01) * 8;
    hadamard_lp_8x8_sse2(src_ptr, src_stride, coeff + idx * 64);
  }

  int16_t *t_coeff = coeff;
  for (int idx = 0; idx < 64; idx += 8) {
    __m128i coeff0 = _mm_load_si128((const __m128i *)t_coeff);
    __m128i coeff1 = _mm_load_si128((const __m128i *)(t_coeff + 64));
    __m128i coeff2 = _mm_load_si128((const __m128i *)(t_coeff + 128));
    __m128i coeff3 = _mm_load_si128((const __m128i *)(t_coeff + 192));

    __m128i b0 = _mm_add_epi16(coeff0, coeff1);
    __m128i b1 = _mm_sub_epi16(coeff0, coeff1);
    __m128i b2 = _mm_add_epi16(coeff2, coeff3);
    __m128i b3 = _mm_sub_epi16(coeff2, coeff3);

    b0 = _mm_srai_epi16(b0, 1);
    b1 = _mm_srai_epi16(b1, 1);
    b2 = _mm_srai_epi16(b2, 1);
    b3 = _mm_srai_epi16(b3, 1);

    coeff0 = _mm_add_epi16(b0, b2);
    coeff1 = _mm_add_epi16(b1, b3);
    coeff2 = _mm_sub_epi16(b0, b2);
    coeff3 = _mm_sub_epi16(b1, b3);

    _mm_store_si128((__m128i *)t_coeff, coeff0);
    _mm_store_si128((__m128i *)(t_coeff + 64), coeff1);
    _mm_store_si128((__m128i *)(t_coeff + 128), coeff2);
    _mm_store_si128((__m128i *)(t_coeff + 192), coeff3);

    t_coeff += 8;
  }
}

static INLINE void hadamard_16x16_sse2(const int16_t *src_diff,
                                       ptrdiff_t src_stride, tran_low_t *coeff,
                                       int is_final) {
  // For high bitdepths, it is unnecessary to store_tran_low
  // (mult/unpack/store), then load_tran_low (load/pack) the same memory in the
  // next stage.  Output to an intermediate buffer first, then store_tran_low()
  // in the final stage.
  DECLARE_ALIGNED(32, int16_t, temp_coeff[16 * 16]);
  int16_t *t_coeff = temp_coeff;
  int16_t *coeff16 = (int16_t *)coeff;
  int idx;
  for (idx = 0; idx < 4; ++idx) {
    const int16_t *src_ptr =
        src_diff + (idx >> 1) * 8 * src_stride + (idx & 0x01) * 8;
    hadamard_8x8_sse2(src_ptr, src_stride, (tran_low_t *)(t_coeff + idx * 64),
                      0);
  }

  for (idx = 0; idx < 64; idx += 8) {
    __m128i coeff0 = _mm_load_si128((const __m128i *)t_coeff);
    __m128i coeff1 = _mm_load_si128((const __m128i *)(t_coeff + 64));
    __m128i coeff2 = _mm_load_si128((const __m128i *)(t_coeff + 128));
    __m128i coeff3 = _mm_load_si128((const __m128i *)(t_coeff + 192));

    __m128i b0 = _mm_add_epi16(coeff0, coeff1);
    __m128i b1 = _mm_sub_epi16(coeff0, coeff1);
    __m128i b2 = _mm_add_epi16(coeff2, coeff3);
    __m128i b3 = _mm_sub_epi16(coeff2, coeff3);

    b0 = _mm_srai_epi16(b0, 1);
    b1 = _mm_srai_epi16(b1, 1);
    b2 = _mm_srai_epi16(b2, 1);
    b3 = _mm_srai_epi16(b3, 1);

    coeff0 = _mm_add_epi16(b0, b2);
    coeff1 = _mm_add_epi16(b1, b3);
    coeff2 = _mm_sub_epi16(b0, b2);
    coeff3 = _mm_sub_epi16(b1, b3);

    if (is_final) {
      store_tran_low_offset_4(coeff0, coeff);
      store_tran_low_offset_4(coeff1, coeff + 64);
      store_tran_low_offset_4(coeff2, coeff + 128);
      store_tran_low_offset_4(coeff3, coeff + 192);
      coeff += 4;
    } else {
      _mm_store_si128((__m128i *)coeff16, coeff0);
      _mm_store_si128((__m128i *)(coeff16 + 64), coeff1);
      _mm_store_si128((__m128i *)(coeff16 + 128), coeff2);
      _mm_store_si128((__m128i *)(coeff16 + 192), coeff3);
      coeff16 += 8;
    }

    t_coeff += 8;
    // Increment the pointer additionally by 0 and 8 in alternate
    // iterations(instead of 8) to ensure the coherency with the implementation
    // of store_tran_low_offset_4()
    coeff += (((idx >> 3) & 1) << 3);
  }
}

void aom_hadamard_16x16_sse2(const int16_t *src_diff, ptrdiff_t src_stride,
                             tran_low_t *coeff) {
  hadamard_16x16_sse2(src_diff, src_stride, coeff, 1);
}

void aom_hadamard_32x32_sse2(const int16_t *src_diff, ptrdiff_t src_stride,
                             tran_low_t *coeff) {
  // For high bitdepths, it is unnecessary to store_tran_low
  // (mult/unpack/store), then load_tran_low (load/pack) the same memory in the
  // next stage.  Output to an intermediate buffer first, then store_tran_low()
  // in the final stage.
  DECLARE_ALIGNED(32, int16_t, temp_coeff[32 * 32]);
  int16_t *t_coeff = temp_coeff;
  int idx;
  __m128i coeff0_lo, coeff1_lo, coeff2_lo, coeff3_lo, b0_lo, b1_lo, b2_lo,
      b3_lo;
  __m128i coeff0_hi, coeff1_hi, coeff2_hi, coeff3_hi, b0_hi, b1_hi, b2_hi,
      b3_hi;
  __m128i b0, b1, b2, b3;
  const __m128i zero = _mm_setzero_si128();
  for (idx = 0; idx < 4; ++idx) {
    const int16_t *src_ptr =
        src_diff + (idx >> 1) * 16 * src_stride + (idx & 0x01) * 16;
    hadamard_16x16_sse2(src_ptr, src_stride,
                        (tran_low_t *)(t_coeff + idx * 256), 0);
  }

  for (idx = 0; idx < 256; idx += 8) {
    __m128i coeff0 = _mm_load_si128((const __m128i *)t_coeff);
    __m128i coeff1 = _mm_load_si128((const __m128i *)(t_coeff + 256));
    __m128i coeff2 = _mm_load_si128((const __m128i *)(t_coeff + 512));
    __m128i coeff3 = _mm_load_si128((const __m128i *)(t_coeff + 768));

    // Sign extend 16 bit to 32 bit.
    sign_extend_16bit_to_32bit_sse2(coeff0, zero, &coeff0_lo, &coeff0_hi);
    sign_extend_16bit_to_32bit_sse2(coeff1, zero, &coeff1_lo, &coeff1_hi);
    sign_extend_16bit_to_32bit_sse2(coeff2, zero, &coeff2_lo, &coeff2_hi);
    sign_extend_16bit_to_32bit_sse2(coeff3, zero, &coeff3_lo, &coeff3_hi);

    b0_lo = _mm_add_epi32(coeff0_lo, coeff1_lo);
    b0_hi = _mm_add_epi32(coeff0_hi, coeff1_hi);

    b1_lo = _mm_sub_epi32(coeff0_lo, coeff1_lo);
    b1_hi = _mm_sub_epi32(coeff0_hi, coeff1_hi);

    b2_lo = _mm_add_epi32(coeff2_lo, coeff3_lo);
    b2_hi = _mm_add_epi32(coeff2_hi, coeff3_hi);

    b3_lo = _mm_sub_epi32(coeff2_lo, coeff3_lo);
    b3_hi = _mm_sub_epi32(coeff2_hi, coeff3_hi);

    b0_lo = _mm_srai_epi32(b0_lo, 2);
    b1_lo = _mm_srai_epi32(b1_lo, 2);
    b2_lo = _mm_srai_epi32(b2_lo, 2);
    b3_lo = _mm_srai_epi32(b3_lo, 2);

    b0_hi = _mm_srai_epi32(b0_hi, 2);
    b1_hi = _mm_srai_epi32(b1_hi, 2);
    b2_hi = _mm_srai_epi32(b2_hi, 2);
    b3_hi = _mm_srai_epi32(b3_hi, 2);

    b0 = _mm_packs_epi32(b0_lo, b0_hi);
    b1 = _mm_packs_epi32(b1_lo, b1_hi);
    b2 = _mm_packs_epi32(b2_lo, b2_hi);
    b3 = _mm_packs_epi32(b3_lo, b3_hi);

    coeff0 = _mm_add_epi16(b0, b2);
    coeff1 = _mm_add_epi16(b1, b3);
    store_tran_low_offset_4(coeff0, coeff);
    store_tran_low_offset_4(coeff1, coeff + 256);

    coeff2 = _mm_sub_epi16(b0, b2);
    coeff3 = _mm_sub_epi16(b1, b3);
    store_tran_low_offset_4(coeff2, coeff + 512);
    store_tran_low_offset_4(coeff3, coeff + 768);

    // Increment the pointer by 4 and 12 in alternate iterations(instead of 8)
    // to ensure the coherency with the implementation of
    // store_tran_low_offset_4()
    coeff += (4 + (((idx >> 3) & 1) << 3));
    t_coeff += 8;
  }
}

int aom_satd_sse2(const tran_low_t *coeff, int length) {
  int i;
  const __m128i zero = _mm_setzero_si128();
  __m128i accum = zero;

  for (i = 0; i < length; i += 4) {
    const __m128i src_line = _mm_load_si128((const __m128i *)coeff);
    const __m128i coeff_sign = _mm_srai_epi32(src_line, 31);
    const __m128i abs_coeff = invert_sign_32_sse2(src_line, coeff_sign);
    accum = _mm_add_epi32(accum, abs_coeff);
    coeff += 4;
  }

  {  // cascading summation of accum
    __m128i hi = _mm_srli_si128(accum, 8);
    accum = _mm_add_epi32(accum, hi);
    hi = _mm_srli_epi64(accum, 32);
    accum = _mm_add_epi32(accum, hi);
  }

  return _mm_cvtsi128_si32(accum);
}

int aom_satd_lp_sse2(const int16_t *coeff, int length) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i one = _mm_set1_epi16(1);
  __m128i accum = zero;

  for (int i = 0; i < length; i += 16) {
    const __m128i src_line0 = _mm_loadu_si128((const __m128i *)coeff);
    const __m128i src_line1 = _mm_loadu_si128((const __m128i *)(coeff + 8));
    const __m128i inv0 = _mm_sub_epi16(zero, src_line0);
    const __m128i inv1 = _mm_sub_epi16(zero, src_line1);
    const __m128i abs0 = _mm_max_epi16(src_line0, inv0);  // abs(src_line)
    const __m128i abs1 = _mm_max_epi16(src_line1, inv1);  // abs(src_line)
    const __m128i sum0 = _mm_madd_epi16(abs0, one);
    const __m128i sum1 = _mm_madd_epi16(abs1, one);
    accum = _mm_add_epi32(accum, sum0);
    accum = _mm_add_epi32(accum, sum1);
    coeff += 16;
  }

  {  // cascading summation of accum
    __m128i hi = _mm_srli_si128(accum, 8);
    accum = _mm_add_epi32(accum, hi);
    hi = _mm_srli_epi64(accum, 32);
    accum = _mm_add_epi32(accum, hi);
  }

  return _mm_cvtsi128_si32(accum);
}

void aom_int_pro_row_sse2(int16_t *hbuf, const uint8_t *ref,
                          const int ref_stride, const int width,
                          const int height, int norm_factor) {
  // SIMD implementation assumes width and height to be multiple of 16 and 2
  // respectively. For any odd width or height, SIMD support needs to be added.
  assert(width % 16 == 0 && height % 2 == 0);
  __m128i zero = _mm_setzero_si128();

  for (int wd = 0; wd < width; wd += 16) {
    const uint8_t *ref_tmp = ref + wd;
    int16_t *hbuf_tmp = hbuf + wd;
    __m128i s0 = zero;
    __m128i s1 = zero;
    int idx = 0;
    do {
      __m128i src_line = _mm_loadu_si128((const __m128i *)ref_tmp);
      __m128i t0 = _mm_unpacklo_epi8(src_line, zero);
      __m128i t1 = _mm_unpackhi_epi8(src_line, zero);
      s0 = _mm_add_epi16(s0, t0);
      s1 = _mm_add_epi16(s1, t1);
      ref_tmp += ref_stride;

      src_line = _mm_loadu_si128((const __m128i *)ref_tmp);
      t0 = _mm_unpacklo_epi8(src_line, zero);
      t1 = _mm_unpackhi_epi8(src_line, zero);
      s0 = _mm_add_epi16(s0, t0);
      s1 = _mm_add_epi16(s1, t1);
      ref_tmp += ref_stride;
      idx += 2;
    } while (idx < height);

    s0 = _mm_srai_epi16(s0, norm_factor);
    s1 = _mm_srai_epi16(s1, norm_factor);
    _mm_storeu_si128((__m128i *)(hbuf_tmp), s0);
    _mm_storeu_si128((__m128i *)(hbuf_tmp + 8), s1);
  }
}

void aom_int_pro_col_sse2(int16_t *vbuf, const uint8_t *ref,
                          const int ref_stride, const int width,
                          const int height, int norm_factor) {
  // SIMD implementation assumes width to be multiple of 16.
  assert(width % 16 == 0);

  for (int ht = 0; ht < height; ht++) {
    const uint8_t *ref_tmp = ref + (ht * ref_stride);
    __m128i zero = _mm_setzero_si128();
    __m128i s0 = zero;
    __m128i s1, src_line;
    for (int i = 0; i < width; i += 16) {
      src_line = _mm_loadu_si128((const __m128i *)ref_tmp);
      s1 = _mm_sad_epu8(src_line, zero);
      s0 = _mm_add_epi16(s0, s1);
      ref_tmp += 16;
    }

    s1 = _mm_srli_si128(s0, 8);
    s0 = _mm_add_epi16(s0, s1);
    vbuf[ht] = _mm_cvtsi128_si32(s0) >> norm_factor;
  }
}
