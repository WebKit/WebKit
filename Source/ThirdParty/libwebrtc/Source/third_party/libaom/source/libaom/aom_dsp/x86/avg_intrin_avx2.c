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
#include "aom_dsp/x86/bitdepth_conversion_avx2.h"
#include "aom_ports/mem.h"

static INLINE void sign_extend_16bit_to_32bit_avx2(__m256i in, __m256i zero,
                                                   __m256i *out_lo,
                                                   __m256i *out_hi) {
  const __m256i sign_bits = _mm256_cmpgt_epi16(zero, in);
  *out_lo = _mm256_unpacklo_epi16(in, sign_bits);
  *out_hi = _mm256_unpackhi_epi16(in, sign_bits);
}

static void hadamard_col8x2_avx2(__m256i *in, int iter) {
  __m256i a0 = in[0];
  __m256i a1 = in[1];
  __m256i a2 = in[2];
  __m256i a3 = in[3];
  __m256i a4 = in[4];
  __m256i a5 = in[5];
  __m256i a6 = in[6];
  __m256i a7 = in[7];

  __m256i b0 = _mm256_add_epi16(a0, a1);
  __m256i b1 = _mm256_sub_epi16(a0, a1);
  __m256i b2 = _mm256_add_epi16(a2, a3);
  __m256i b3 = _mm256_sub_epi16(a2, a3);
  __m256i b4 = _mm256_add_epi16(a4, a5);
  __m256i b5 = _mm256_sub_epi16(a4, a5);
  __m256i b6 = _mm256_add_epi16(a6, a7);
  __m256i b7 = _mm256_sub_epi16(a6, a7);

  a0 = _mm256_add_epi16(b0, b2);
  a1 = _mm256_add_epi16(b1, b3);
  a2 = _mm256_sub_epi16(b0, b2);
  a3 = _mm256_sub_epi16(b1, b3);
  a4 = _mm256_add_epi16(b4, b6);
  a5 = _mm256_add_epi16(b5, b7);
  a6 = _mm256_sub_epi16(b4, b6);
  a7 = _mm256_sub_epi16(b5, b7);

  if (iter == 0) {
    b0 = _mm256_add_epi16(a0, a4);
    b7 = _mm256_add_epi16(a1, a5);
    b3 = _mm256_add_epi16(a2, a6);
    b4 = _mm256_add_epi16(a3, a7);
    b2 = _mm256_sub_epi16(a0, a4);
    b6 = _mm256_sub_epi16(a1, a5);
    b1 = _mm256_sub_epi16(a2, a6);
    b5 = _mm256_sub_epi16(a3, a7);

    a0 = _mm256_unpacklo_epi16(b0, b1);
    a1 = _mm256_unpacklo_epi16(b2, b3);
    a2 = _mm256_unpackhi_epi16(b0, b1);
    a3 = _mm256_unpackhi_epi16(b2, b3);
    a4 = _mm256_unpacklo_epi16(b4, b5);
    a5 = _mm256_unpacklo_epi16(b6, b7);
    a6 = _mm256_unpackhi_epi16(b4, b5);
    a7 = _mm256_unpackhi_epi16(b6, b7);

    b0 = _mm256_unpacklo_epi32(a0, a1);
    b1 = _mm256_unpacklo_epi32(a4, a5);
    b2 = _mm256_unpackhi_epi32(a0, a1);
    b3 = _mm256_unpackhi_epi32(a4, a5);
    b4 = _mm256_unpacklo_epi32(a2, a3);
    b5 = _mm256_unpacklo_epi32(a6, a7);
    b6 = _mm256_unpackhi_epi32(a2, a3);
    b7 = _mm256_unpackhi_epi32(a6, a7);

    in[0] = _mm256_unpacklo_epi64(b0, b1);
    in[1] = _mm256_unpackhi_epi64(b0, b1);
    in[2] = _mm256_unpacklo_epi64(b2, b3);
    in[3] = _mm256_unpackhi_epi64(b2, b3);
    in[4] = _mm256_unpacklo_epi64(b4, b5);
    in[5] = _mm256_unpackhi_epi64(b4, b5);
    in[6] = _mm256_unpacklo_epi64(b6, b7);
    in[7] = _mm256_unpackhi_epi64(b6, b7);
  } else {
    in[0] = _mm256_add_epi16(a0, a4);
    in[7] = _mm256_add_epi16(a1, a5);
    in[3] = _mm256_add_epi16(a2, a6);
    in[4] = _mm256_add_epi16(a3, a7);
    in[2] = _mm256_sub_epi16(a0, a4);
    in[6] = _mm256_sub_epi16(a1, a5);
    in[1] = _mm256_sub_epi16(a2, a6);
    in[5] = _mm256_sub_epi16(a3, a7);
  }
}

void aom_hadamard_lp_8x8_dual_avx2(const int16_t *src_diff,
                                   ptrdiff_t src_stride, int16_t *coeff) {
  __m256i src[8];
  src[0] = _mm256_loadu_si256((const __m256i *)src_diff);
  src[1] = _mm256_loadu_si256((const __m256i *)(src_diff += src_stride));
  src[2] = _mm256_loadu_si256((const __m256i *)(src_diff += src_stride));
  src[3] = _mm256_loadu_si256((const __m256i *)(src_diff += src_stride));
  src[4] = _mm256_loadu_si256((const __m256i *)(src_diff += src_stride));
  src[5] = _mm256_loadu_si256((const __m256i *)(src_diff += src_stride));
  src[6] = _mm256_loadu_si256((const __m256i *)(src_diff += src_stride));
  src[7] = _mm256_loadu_si256((const __m256i *)(src_diff + src_stride));

  hadamard_col8x2_avx2(src, 0);
  hadamard_col8x2_avx2(src, 1);

  _mm256_storeu_si256((__m256i *)coeff,
                      _mm256_permute2x128_si256(src[0], src[1], 0x20));
  coeff += 16;
  _mm256_storeu_si256((__m256i *)coeff,
                      _mm256_permute2x128_si256(src[2], src[3], 0x20));
  coeff += 16;
  _mm256_storeu_si256((__m256i *)coeff,
                      _mm256_permute2x128_si256(src[4], src[5], 0x20));
  coeff += 16;
  _mm256_storeu_si256((__m256i *)coeff,
                      _mm256_permute2x128_si256(src[6], src[7], 0x20));
  coeff += 16;
  _mm256_storeu_si256((__m256i *)coeff,
                      _mm256_permute2x128_si256(src[0], src[1], 0x31));
  coeff += 16;
  _mm256_storeu_si256((__m256i *)coeff,
                      _mm256_permute2x128_si256(src[2], src[3], 0x31));
  coeff += 16;
  _mm256_storeu_si256((__m256i *)coeff,
                      _mm256_permute2x128_si256(src[4], src[5], 0x31));
  coeff += 16;
  _mm256_storeu_si256((__m256i *)coeff,
                      _mm256_permute2x128_si256(src[6], src[7], 0x31));
}

static INLINE void hadamard_16x16_avx2(const int16_t *src_diff,
                                       ptrdiff_t src_stride, tran_low_t *coeff,
                                       int is_final) {
  DECLARE_ALIGNED(32, int16_t, temp_coeff[16 * 16]);
  int16_t *t_coeff = temp_coeff;
  int16_t *coeff16 = (int16_t *)coeff;
  int idx;
  for (idx = 0; idx < 2; ++idx) {
    const int16_t *src_ptr = src_diff + idx * 8 * src_stride;
    aom_hadamard_lp_8x8_dual_avx2(src_ptr, src_stride,
                                  t_coeff + (idx * 64 * 2));
  }

  for (idx = 0; idx < 64; idx += 16) {
    const __m256i coeff0 = _mm256_loadu_si256((const __m256i *)t_coeff);
    const __m256i coeff1 = _mm256_loadu_si256((const __m256i *)(t_coeff + 64));
    const __m256i coeff2 = _mm256_loadu_si256((const __m256i *)(t_coeff + 128));
    const __m256i coeff3 = _mm256_loadu_si256((const __m256i *)(t_coeff + 192));

    __m256i b0 = _mm256_add_epi16(coeff0, coeff1);
    __m256i b1 = _mm256_sub_epi16(coeff0, coeff1);
    __m256i b2 = _mm256_add_epi16(coeff2, coeff3);
    __m256i b3 = _mm256_sub_epi16(coeff2, coeff3);

    b0 = _mm256_srai_epi16(b0, 1);
    b1 = _mm256_srai_epi16(b1, 1);
    b2 = _mm256_srai_epi16(b2, 1);
    b3 = _mm256_srai_epi16(b3, 1);
    if (is_final) {
      store_tran_low(_mm256_add_epi16(b0, b2), coeff);
      store_tran_low(_mm256_add_epi16(b1, b3), coeff + 64);
      store_tran_low(_mm256_sub_epi16(b0, b2), coeff + 128);
      store_tran_low(_mm256_sub_epi16(b1, b3), coeff + 192);
      coeff += 16;
    } else {
      _mm256_storeu_si256((__m256i *)coeff16, _mm256_add_epi16(b0, b2));
      _mm256_storeu_si256((__m256i *)(coeff16 + 64), _mm256_add_epi16(b1, b3));
      _mm256_storeu_si256((__m256i *)(coeff16 + 128), _mm256_sub_epi16(b0, b2));
      _mm256_storeu_si256((__m256i *)(coeff16 + 192), _mm256_sub_epi16(b1, b3));
      coeff16 += 16;
    }
    t_coeff += 16;
  }
}

void aom_hadamard_16x16_avx2(const int16_t *src_diff, ptrdiff_t src_stride,
                             tran_low_t *coeff) {
  hadamard_16x16_avx2(src_diff, src_stride, coeff, 1);
}

void aom_hadamard_lp_16x16_avx2(const int16_t *src_diff, ptrdiff_t src_stride,
                                int16_t *coeff) {
  int16_t *t_coeff = coeff;
  for (int idx = 0; idx < 2; ++idx) {
    const int16_t *src_ptr = src_diff + idx * 8 * src_stride;
    aom_hadamard_lp_8x8_dual_avx2(src_ptr, src_stride,
                                  t_coeff + (idx * 64 * 2));
  }

  for (int idx = 0; idx < 64; idx += 16) {
    const __m256i coeff0 = _mm256_loadu_si256((const __m256i *)t_coeff);
    const __m256i coeff1 = _mm256_loadu_si256((const __m256i *)(t_coeff + 64));
    const __m256i coeff2 = _mm256_loadu_si256((const __m256i *)(t_coeff + 128));
    const __m256i coeff3 = _mm256_loadu_si256((const __m256i *)(t_coeff + 192));

    __m256i b0 = _mm256_add_epi16(coeff0, coeff1);
    __m256i b1 = _mm256_sub_epi16(coeff0, coeff1);
    __m256i b2 = _mm256_add_epi16(coeff2, coeff3);
    __m256i b3 = _mm256_sub_epi16(coeff2, coeff3);

    b0 = _mm256_srai_epi16(b0, 1);
    b1 = _mm256_srai_epi16(b1, 1);
    b2 = _mm256_srai_epi16(b2, 1);
    b3 = _mm256_srai_epi16(b3, 1);
    _mm256_storeu_si256((__m256i *)coeff, _mm256_add_epi16(b0, b2));
    _mm256_storeu_si256((__m256i *)(coeff + 64), _mm256_add_epi16(b1, b3));
    _mm256_storeu_si256((__m256i *)(coeff + 128), _mm256_sub_epi16(b0, b2));
    _mm256_storeu_si256((__m256i *)(coeff + 192), _mm256_sub_epi16(b1, b3));
    coeff += 16;
    t_coeff += 16;
  }
}

void aom_hadamard_32x32_avx2(const int16_t *src_diff, ptrdiff_t src_stride,
                             tran_low_t *coeff) {
  // For high bitdepths, it is unnecessary to store_tran_low
  // (mult/unpack/store), then load_tran_low (load/pack) the same memory in the
  // next stage.  Output to an intermediate buffer first, then store_tran_low()
  // in the final stage.
  DECLARE_ALIGNED(32, int16_t, temp_coeff[32 * 32]);
  int16_t *t_coeff = temp_coeff;
  int idx;
  __m256i coeff0_lo, coeff1_lo, coeff2_lo, coeff3_lo, b0_lo, b1_lo, b2_lo,
      b3_lo;
  __m256i coeff0_hi, coeff1_hi, coeff2_hi, coeff3_hi, b0_hi, b1_hi, b2_hi,
      b3_hi;
  __m256i b0, b1, b2, b3;
  const __m256i zero = _mm256_setzero_si256();
  for (idx = 0; idx < 4; ++idx) {
    // src_diff: 9 bit, dynamic range [-255, 255]
    const int16_t *src_ptr =
        src_diff + (idx >> 1) * 16 * src_stride + (idx & 0x01) * 16;
    hadamard_16x16_avx2(src_ptr, src_stride,
                        (tran_low_t *)(t_coeff + idx * 256), 0);
  }

  for (idx = 0; idx < 256; idx += 16) {
    const __m256i coeff0 = _mm256_loadu_si256((const __m256i *)t_coeff);
    const __m256i coeff1 = _mm256_loadu_si256((const __m256i *)(t_coeff + 256));
    const __m256i coeff2 = _mm256_loadu_si256((const __m256i *)(t_coeff + 512));
    const __m256i coeff3 = _mm256_loadu_si256((const __m256i *)(t_coeff + 768));

    // Sign extend 16 bit to 32 bit.
    sign_extend_16bit_to_32bit_avx2(coeff0, zero, &coeff0_lo, &coeff0_hi);
    sign_extend_16bit_to_32bit_avx2(coeff1, zero, &coeff1_lo, &coeff1_hi);
    sign_extend_16bit_to_32bit_avx2(coeff2, zero, &coeff2_lo, &coeff2_hi);
    sign_extend_16bit_to_32bit_avx2(coeff3, zero, &coeff3_lo, &coeff3_hi);

    b0_lo = _mm256_add_epi32(coeff0_lo, coeff1_lo);
    b0_hi = _mm256_add_epi32(coeff0_hi, coeff1_hi);

    b1_lo = _mm256_sub_epi32(coeff0_lo, coeff1_lo);
    b1_hi = _mm256_sub_epi32(coeff0_hi, coeff1_hi);

    b2_lo = _mm256_add_epi32(coeff2_lo, coeff3_lo);
    b2_hi = _mm256_add_epi32(coeff2_hi, coeff3_hi);

    b3_lo = _mm256_sub_epi32(coeff2_lo, coeff3_lo);
    b3_hi = _mm256_sub_epi32(coeff2_hi, coeff3_hi);

    b0_lo = _mm256_srai_epi32(b0_lo, 2);
    b1_lo = _mm256_srai_epi32(b1_lo, 2);
    b2_lo = _mm256_srai_epi32(b2_lo, 2);
    b3_lo = _mm256_srai_epi32(b3_lo, 2);

    b0_hi = _mm256_srai_epi32(b0_hi, 2);
    b1_hi = _mm256_srai_epi32(b1_hi, 2);
    b2_hi = _mm256_srai_epi32(b2_hi, 2);
    b3_hi = _mm256_srai_epi32(b3_hi, 2);

    b0 = _mm256_packs_epi32(b0_lo, b0_hi);
    b1 = _mm256_packs_epi32(b1_lo, b1_hi);
    b2 = _mm256_packs_epi32(b2_lo, b2_hi);
    b3 = _mm256_packs_epi32(b3_lo, b3_hi);

    store_tran_low(_mm256_add_epi16(b0, b2), coeff);
    store_tran_low(_mm256_add_epi16(b1, b3), coeff + 256);
    store_tran_low(_mm256_sub_epi16(b0, b2), coeff + 512);
    store_tran_low(_mm256_sub_epi16(b1, b3), coeff + 768);

    coeff += 16;
    t_coeff += 16;
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
static void highbd_hadamard_col8_avx2(__m256i *in, int iter) {
  __m256i a0 = in[0];
  __m256i a1 = in[1];
  __m256i a2 = in[2];
  __m256i a3 = in[3];
  __m256i a4 = in[4];
  __m256i a5 = in[5];
  __m256i a6 = in[6];
  __m256i a7 = in[7];

  __m256i b0 = _mm256_add_epi32(a0, a1);
  __m256i b1 = _mm256_sub_epi32(a0, a1);
  __m256i b2 = _mm256_add_epi32(a2, a3);
  __m256i b3 = _mm256_sub_epi32(a2, a3);
  __m256i b4 = _mm256_add_epi32(a4, a5);
  __m256i b5 = _mm256_sub_epi32(a4, a5);
  __m256i b6 = _mm256_add_epi32(a6, a7);
  __m256i b7 = _mm256_sub_epi32(a6, a7);

  a0 = _mm256_add_epi32(b0, b2);
  a1 = _mm256_add_epi32(b1, b3);
  a2 = _mm256_sub_epi32(b0, b2);
  a3 = _mm256_sub_epi32(b1, b3);
  a4 = _mm256_add_epi32(b4, b6);
  a5 = _mm256_add_epi32(b5, b7);
  a6 = _mm256_sub_epi32(b4, b6);
  a7 = _mm256_sub_epi32(b5, b7);

  if (iter == 0) {
    b0 = _mm256_add_epi32(a0, a4);
    b7 = _mm256_add_epi32(a1, a5);
    b3 = _mm256_add_epi32(a2, a6);
    b4 = _mm256_add_epi32(a3, a7);
    b2 = _mm256_sub_epi32(a0, a4);
    b6 = _mm256_sub_epi32(a1, a5);
    b1 = _mm256_sub_epi32(a2, a6);
    b5 = _mm256_sub_epi32(a3, a7);

    a0 = _mm256_unpacklo_epi32(b0, b1);
    a1 = _mm256_unpacklo_epi32(b2, b3);
    a2 = _mm256_unpackhi_epi32(b0, b1);
    a3 = _mm256_unpackhi_epi32(b2, b3);
    a4 = _mm256_unpacklo_epi32(b4, b5);
    a5 = _mm256_unpacklo_epi32(b6, b7);
    a6 = _mm256_unpackhi_epi32(b4, b5);
    a7 = _mm256_unpackhi_epi32(b6, b7);

    b0 = _mm256_unpacklo_epi64(a0, a1);
    b1 = _mm256_unpacklo_epi64(a4, a5);
    b2 = _mm256_unpackhi_epi64(a0, a1);
    b3 = _mm256_unpackhi_epi64(a4, a5);
    b4 = _mm256_unpacklo_epi64(a2, a3);
    b5 = _mm256_unpacklo_epi64(a6, a7);
    b6 = _mm256_unpackhi_epi64(a2, a3);
    b7 = _mm256_unpackhi_epi64(a6, a7);

    in[0] = _mm256_permute2x128_si256(b0, b1, 0x20);
    in[1] = _mm256_permute2x128_si256(b0, b1, 0x31);
    in[2] = _mm256_permute2x128_si256(b2, b3, 0x20);
    in[3] = _mm256_permute2x128_si256(b2, b3, 0x31);
    in[4] = _mm256_permute2x128_si256(b4, b5, 0x20);
    in[5] = _mm256_permute2x128_si256(b4, b5, 0x31);
    in[6] = _mm256_permute2x128_si256(b6, b7, 0x20);
    in[7] = _mm256_permute2x128_si256(b6, b7, 0x31);
  } else {
    in[0] = _mm256_add_epi32(a0, a4);
    in[7] = _mm256_add_epi32(a1, a5);
    in[3] = _mm256_add_epi32(a2, a6);
    in[4] = _mm256_add_epi32(a3, a7);
    in[2] = _mm256_sub_epi32(a0, a4);
    in[6] = _mm256_sub_epi32(a1, a5);
    in[1] = _mm256_sub_epi32(a2, a6);
    in[5] = _mm256_sub_epi32(a3, a7);
  }
}

void aom_highbd_hadamard_8x8_avx2(const int16_t *src_diff, ptrdiff_t src_stride,
                                  tran_low_t *coeff) {
  __m128i src16[8];
  __m256i src32[8];

  src16[0] = _mm_loadu_si128((const __m128i *)src_diff);
  src16[1] = _mm_loadu_si128((const __m128i *)(src_diff += src_stride));
  src16[2] = _mm_loadu_si128((const __m128i *)(src_diff += src_stride));
  src16[3] = _mm_loadu_si128((const __m128i *)(src_diff += src_stride));
  src16[4] = _mm_loadu_si128((const __m128i *)(src_diff += src_stride));
  src16[5] = _mm_loadu_si128((const __m128i *)(src_diff += src_stride));
  src16[6] = _mm_loadu_si128((const __m128i *)(src_diff += src_stride));
  src16[7] = _mm_loadu_si128((const __m128i *)(src_diff + src_stride));

  src32[0] = _mm256_cvtepi16_epi32(src16[0]);
  src32[1] = _mm256_cvtepi16_epi32(src16[1]);
  src32[2] = _mm256_cvtepi16_epi32(src16[2]);
  src32[3] = _mm256_cvtepi16_epi32(src16[3]);
  src32[4] = _mm256_cvtepi16_epi32(src16[4]);
  src32[5] = _mm256_cvtepi16_epi32(src16[5]);
  src32[6] = _mm256_cvtepi16_epi32(src16[6]);
  src32[7] = _mm256_cvtepi16_epi32(src16[7]);

  highbd_hadamard_col8_avx2(src32, 0);
  highbd_hadamard_col8_avx2(src32, 1);

  _mm256_storeu_si256((__m256i *)coeff, src32[0]);
  coeff += 8;
  _mm256_storeu_si256((__m256i *)coeff, src32[1]);
  coeff += 8;
  _mm256_storeu_si256((__m256i *)coeff, src32[2]);
  coeff += 8;
  _mm256_storeu_si256((__m256i *)coeff, src32[3]);
  coeff += 8;
  _mm256_storeu_si256((__m256i *)coeff, src32[4]);
  coeff += 8;
  _mm256_storeu_si256((__m256i *)coeff, src32[5]);
  coeff += 8;
  _mm256_storeu_si256((__m256i *)coeff, src32[6]);
  coeff += 8;
  _mm256_storeu_si256((__m256i *)coeff, src32[7]);
}

void aom_highbd_hadamard_16x16_avx2(const int16_t *src_diff,
                                    ptrdiff_t src_stride, tran_low_t *coeff) {
  int idx;
  tran_low_t *t_coeff = coeff;
  for (idx = 0; idx < 4; ++idx) {
    const int16_t *src_ptr =
        src_diff + (idx >> 1) * 8 * src_stride + (idx & 0x01) * 8;
    aom_highbd_hadamard_8x8_avx2(src_ptr, src_stride, t_coeff + idx * 64);
  }

  for (idx = 0; idx < 64; idx += 8) {
    __m256i coeff0 = _mm256_loadu_si256((const __m256i *)t_coeff);
    __m256i coeff1 = _mm256_loadu_si256((const __m256i *)(t_coeff + 64));
    __m256i coeff2 = _mm256_loadu_si256((const __m256i *)(t_coeff + 128));
    __m256i coeff3 = _mm256_loadu_si256((const __m256i *)(t_coeff + 192));

    __m256i b0 = _mm256_add_epi32(coeff0, coeff1);
    __m256i b1 = _mm256_sub_epi32(coeff0, coeff1);
    __m256i b2 = _mm256_add_epi32(coeff2, coeff3);
    __m256i b3 = _mm256_sub_epi32(coeff2, coeff3);

    b0 = _mm256_srai_epi32(b0, 1);
    b1 = _mm256_srai_epi32(b1, 1);
    b2 = _mm256_srai_epi32(b2, 1);
    b3 = _mm256_srai_epi32(b3, 1);

    coeff0 = _mm256_add_epi32(b0, b2);
    coeff1 = _mm256_add_epi32(b1, b3);
    coeff2 = _mm256_sub_epi32(b0, b2);
    coeff3 = _mm256_sub_epi32(b1, b3);

    _mm256_storeu_si256((__m256i *)coeff, coeff0);
    _mm256_storeu_si256((__m256i *)(coeff + 64), coeff1);
    _mm256_storeu_si256((__m256i *)(coeff + 128), coeff2);
    _mm256_storeu_si256((__m256i *)(coeff + 192), coeff3);

    coeff += 8;
    t_coeff += 8;
  }
}

void aom_highbd_hadamard_32x32_avx2(const int16_t *src_diff,
                                    ptrdiff_t src_stride, tran_low_t *coeff) {
  int idx;
  tran_low_t *t_coeff = coeff;
  for (idx = 0; idx < 4; ++idx) {
    const int16_t *src_ptr =
        src_diff + (idx >> 1) * 16 * src_stride + (idx & 0x01) * 16;
    aom_highbd_hadamard_16x16_avx2(src_ptr, src_stride, t_coeff + idx * 256);
  }

  for (idx = 0; idx < 256; idx += 8) {
    __m256i coeff0 = _mm256_loadu_si256((const __m256i *)t_coeff);
    __m256i coeff1 = _mm256_loadu_si256((const __m256i *)(t_coeff + 256));
    __m256i coeff2 = _mm256_loadu_si256((const __m256i *)(t_coeff + 512));
    __m256i coeff3 = _mm256_loadu_si256((const __m256i *)(t_coeff + 768));

    __m256i b0 = _mm256_add_epi32(coeff0, coeff1);
    __m256i b1 = _mm256_sub_epi32(coeff0, coeff1);
    __m256i b2 = _mm256_add_epi32(coeff2, coeff3);
    __m256i b3 = _mm256_sub_epi32(coeff2, coeff3);

    b0 = _mm256_srai_epi32(b0, 2);
    b1 = _mm256_srai_epi32(b1, 2);
    b2 = _mm256_srai_epi32(b2, 2);
    b3 = _mm256_srai_epi32(b3, 2);

    coeff0 = _mm256_add_epi32(b0, b2);
    coeff1 = _mm256_add_epi32(b1, b3);
    coeff2 = _mm256_sub_epi32(b0, b2);
    coeff3 = _mm256_sub_epi32(b1, b3);

    _mm256_storeu_si256((__m256i *)coeff, coeff0);
    _mm256_storeu_si256((__m256i *)(coeff + 256), coeff1);
    _mm256_storeu_si256((__m256i *)(coeff + 512), coeff2);
    _mm256_storeu_si256((__m256i *)(coeff + 768), coeff3);

    coeff += 8;
    t_coeff += 8;
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

int aom_satd_avx2(const tran_low_t *coeff, int length) {
  __m256i accum = _mm256_setzero_si256();
  int i;

  for (i = 0; i < length; i += 8, coeff += 8) {
    const __m256i src_line = _mm256_loadu_si256((const __m256i *)coeff);
    const __m256i abs = _mm256_abs_epi32(src_line);
    accum = _mm256_add_epi32(accum, abs);
  }

  {  // 32 bit horizontal add
    const __m256i a = _mm256_srli_si256(accum, 8);
    const __m256i b = _mm256_add_epi32(accum, a);
    const __m256i c = _mm256_srli_epi64(b, 32);
    const __m256i d = _mm256_add_epi32(b, c);
    const __m128i accum_128 = _mm_add_epi32(_mm256_castsi256_si128(d),
                                            _mm256_extractf128_si256(d, 1));
    return _mm_cvtsi128_si32(accum_128);
  }
}

int aom_satd_lp_avx2(const int16_t *coeff, int length) {
  const __m256i one = _mm256_set1_epi16(1);
  __m256i accum = _mm256_setzero_si256();

  for (int i = 0; i < length; i += 16) {
    const __m256i src_line = _mm256_loadu_si256((const __m256i *)coeff);
    const __m256i abs = _mm256_abs_epi16(src_line);
    const __m256i sum = _mm256_madd_epi16(abs, one);
    accum = _mm256_add_epi32(accum, sum);
    coeff += 16;
  }

  {  // 32 bit horizontal add
    const __m256i a = _mm256_srli_si256(accum, 8);
    const __m256i b = _mm256_add_epi32(accum, a);
    const __m256i c = _mm256_srli_epi64(b, 32);
    const __m256i d = _mm256_add_epi32(b, c);
    const __m128i accum_128 = _mm_add_epi32(_mm256_castsi256_si128(d),
                                            _mm256_extractf128_si256(d, 1));
    return _mm_cvtsi128_si32(accum_128);
  }
}

static INLINE __m256i xx_loadu2_mi128(const void *hi, const void *lo) {
  __m256i a = _mm256_castsi128_si256(_mm_loadu_si128((const __m128i *)(lo)));
  a = _mm256_inserti128_si256(a, _mm_loadu_si128((const __m128i *)(hi)), 1);
  return a;
}

void aom_avg_8x8_quad_avx2(const uint8_t *s, int p, int x16_idx, int y16_idx,
                           int *avg) {
  const uint8_t *s_y0 = s + y16_idx * p + x16_idx;
  const uint8_t *s_y1 = s_y0 + 8 * p;
  __m256i sum0, sum1, s0, s1, s2, s3, u0;
  u0 = _mm256_setzero_si256();
  s0 = _mm256_sad_epu8(xx_loadu2_mi128(s_y1, s_y0), u0);
  s1 = _mm256_sad_epu8(xx_loadu2_mi128(s_y1 + p, s_y0 + p), u0);
  s2 = _mm256_sad_epu8(xx_loadu2_mi128(s_y1 + 2 * p, s_y0 + 2 * p), u0);
  s3 = _mm256_sad_epu8(xx_loadu2_mi128(s_y1 + 3 * p, s_y0 + 3 * p), u0);
  sum0 = _mm256_add_epi16(s0, s1);
  sum1 = _mm256_add_epi16(s2, s3);
  s0 = _mm256_sad_epu8(xx_loadu2_mi128(s_y1 + 4 * p, s_y0 + 4 * p), u0);
  s1 = _mm256_sad_epu8(xx_loadu2_mi128(s_y1 + 5 * p, s_y0 + 5 * p), u0);
  s2 = _mm256_sad_epu8(xx_loadu2_mi128(s_y1 + 6 * p, s_y0 + 6 * p), u0);
  s3 = _mm256_sad_epu8(xx_loadu2_mi128(s_y1 + 7 * p, s_y0 + 7 * p), u0);
  sum0 = _mm256_add_epi16(sum0, _mm256_add_epi16(s0, s1));
  sum1 = _mm256_add_epi16(sum1, _mm256_add_epi16(s2, s3));
  sum0 = _mm256_add_epi16(sum0, sum1);

  // (avg + 32) >> 6
  __m256i rounding = _mm256_set1_epi32(32);
  sum0 = _mm256_add_epi32(sum0, rounding);
  sum0 = _mm256_srli_epi32(sum0, 6);
  __m128i lo = _mm256_castsi256_si128(sum0);
  __m128i hi = _mm256_extracti128_si256(sum0, 1);
  avg[0] = _mm_cvtsi128_si32(lo);
  avg[1] = _mm_extract_epi32(lo, 2);
  avg[2] = _mm_cvtsi128_si32(hi);
  avg[3] = _mm_extract_epi32(hi, 2);
}

void aom_int_pro_row_avx2(int16_t *hbuf, const uint8_t *ref,
                          const int ref_stride, const int width,
                          const int height, int norm_factor) {
  // SIMD implementation assumes width and height to be multiple of 16 and 2
  // respectively. For any odd width or height, SIMD support needs to be added.
  assert(width % 16 == 0 && height % 2 == 0);

  if (width % 32 == 0) {
    const __m256i zero = _mm256_setzero_si256();
    for (int wd = 0; wd < width; wd += 32) {
      const uint8_t *ref_tmp = ref + wd;
      int16_t *hbuf_tmp = hbuf + wd;
      __m256i s0 = zero;
      __m256i s1 = zero;
      int idx = 0;
      do {
        __m256i src_line = _mm256_loadu_si256((const __m256i *)ref_tmp);
        __m256i t0 = _mm256_unpacklo_epi8(src_line, zero);
        __m256i t1 = _mm256_unpackhi_epi8(src_line, zero);
        s0 = _mm256_add_epi16(s0, t0);
        s1 = _mm256_add_epi16(s1, t1);
        ref_tmp += ref_stride;

        src_line = _mm256_loadu_si256((const __m256i *)ref_tmp);
        t0 = _mm256_unpacklo_epi8(src_line, zero);
        t1 = _mm256_unpackhi_epi8(src_line, zero);
        s0 = _mm256_add_epi16(s0, t0);
        s1 = _mm256_add_epi16(s1, t1);
        ref_tmp += ref_stride;
        idx += 2;
      } while (idx < height);
      s0 = _mm256_srai_epi16(s0, norm_factor);
      s1 = _mm256_srai_epi16(s1, norm_factor);
      _mm_storeu_si128((__m128i *)(hbuf_tmp), _mm256_castsi256_si128(s0));
      _mm_storeu_si128((__m128i *)(hbuf_tmp + 8), _mm256_castsi256_si128(s1));
      _mm_storeu_si128((__m128i *)(hbuf_tmp + 16),
                       _mm256_extractf128_si256(s0, 1));
      _mm_storeu_si128((__m128i *)(hbuf_tmp + 24),
                       _mm256_extractf128_si256(s1, 1));
    }
  } else if (width % 16 == 0) {
    aom_int_pro_row_sse2(hbuf, ref, ref_stride, width, height, norm_factor);
  }
}

static INLINE void load_from_src_buf(const uint8_t *ref1, __m256i *src,
                                     const int stride) {
  src[0] = _mm256_loadu_si256((const __m256i *)ref1);
  src[1] = _mm256_loadu_si256((const __m256i *)(ref1 + stride));
  src[2] = _mm256_loadu_si256((const __m256i *)(ref1 + (2 * stride)));
  src[3] = _mm256_loadu_si256((const __m256i *)(ref1 + (3 * stride)));
}

#define CALC_TOT_SAD_AND_STORE                                                \
  /* r00 r10 x x r01 r11 x x | r02 r12 x x r03 r13 x x */                     \
  const __m256i r01 = _mm256_add_epi16(_mm256_slli_si256(r1, 2), r0);         \
  /* r00 r10 r20 x r01 r11 r21 x | r02 r12 r22 x r03 r13 r23 x */             \
  const __m256i r012 = _mm256_add_epi16(_mm256_slli_si256(r2, 4), r01);       \
  /* r00 r10 r20 r30 r01 r11 r21 r31 | r02 r12 r22 r32 r03 r13 r23 r33 */     \
  const __m256i result0 = _mm256_add_epi16(_mm256_slli_si256(r3, 6), r012);   \
                                                                              \
  const __m128i results0 = _mm_add_epi16(                                     \
      _mm256_castsi256_si128(result0), _mm256_extractf128_si256(result0, 1)); \
  const __m128i results1 =                                                    \
      _mm_add_epi16(results0, _mm_srli_si128(results0, 8));                   \
  _mm_storel_epi64((__m128i *)vbuf, _mm_srli_epi16(results1, norm_factor));

static INLINE void aom_int_pro_col_16wd_avx2(int16_t *vbuf, const uint8_t *ref,
                                             const int ref_stride,
                                             const int height,
                                             int norm_factor) {
  const __m256i zero = _mm256_setzero_si256();
  int ht = 0;
  // Post sad operation, the data is present in lower 16-bit of each 64-bit lane
  // and higher 16-bits are Zero. Here, we are processing 8 rows at a time to
  // utilize the higher 16-bits efficiently.
  do {
    __m256i src_00 =
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i *)(ref)));
    src_00 = _mm256_inserti128_si256(
        src_00, _mm_loadu_si128((const __m128i *)(ref + ref_stride * 4)), 1);
    __m256i src_01 = _mm256_castsi128_si256(
        _mm_loadu_si128((const __m128i *)(ref + ref_stride * 1)));
    src_01 = _mm256_inserti128_si256(
        src_01, _mm_loadu_si128((const __m128i *)(ref + ref_stride * 5)), 1);
    __m256i src_10 = _mm256_castsi128_si256(
        _mm_loadu_si128((const __m128i *)(ref + ref_stride * 2)));
    src_10 = _mm256_inserti128_si256(
        src_10, _mm_loadu_si128((const __m128i *)(ref + ref_stride * 6)), 1);
    __m256i src_11 = _mm256_castsi128_si256(
        _mm_loadu_si128((const __m128i *)(ref + ref_stride * 3)));
    src_11 = _mm256_inserti128_si256(
        src_11, _mm_loadu_si128((const __m128i *)(ref + ref_stride * 7)), 1);

    // s00 x x x s01 x x x | s40 x x x s41 x x x
    const __m256i s0 = _mm256_sad_epu8(src_00, zero);
    // s10 x x x s11 x x x | s50 x x x s51 x x x
    const __m256i s1 = _mm256_sad_epu8(src_01, zero);
    // s20 x x x s21 x x x | s60 x x x s61 x x x
    const __m256i s2 = _mm256_sad_epu8(src_10, zero);
    // s30 x x x s31 x x x | s70 x x x s71 x x x
    const __m256i s3 = _mm256_sad_epu8(src_11, zero);

    // s00 s10 x x x x x x | s40 s50 x x x x x x
    const __m256i s0_lo = _mm256_unpacklo_epi16(s0, s1);
    // s01 s11 x x x x x x | s41 s51 x x x x x x
    const __m256i s0_hi = _mm256_unpackhi_epi16(s0, s1);
    // s20 s30 x x x x x x | s60 s70 x x x x x x
    const __m256i s1_lo = _mm256_unpacklo_epi16(s2, s3);
    // s21 s31 x x x x x x | s61 s71 x x x x x x
    const __m256i s1_hi = _mm256_unpackhi_epi16(s2, s3);

    // s0 s1 x x x x x x | s4 s5 x x x x x x
    const __m256i s0_add = _mm256_add_epi16(s0_lo, s0_hi);
    // s2 s3 x x x x x x | s6 s7 x x x x x x
    const __m256i s1_add = _mm256_add_epi16(s1_lo, s1_hi);

    // s1 s1 s2 s3 s4 s5 s6 s7
    const __m128i results = _mm256_castsi256_si128(
        _mm256_permute4x64_epi64(_mm256_unpacklo_epi32(s0_add, s1_add), 0x08));
    _mm_storeu_si128((__m128i *)vbuf, _mm_srli_epi16(results, norm_factor));
    vbuf += 8;
    ref += (ref_stride << 3);
    ht += 8;
  } while (ht < height);
}

void aom_int_pro_col_avx2(int16_t *vbuf, const uint8_t *ref,
                          const int ref_stride, const int width,
                          const int height, int norm_factor) {
  assert(width % 16 == 0);
  if (width == 128) {
    const __m256i zero = _mm256_setzero_si256();
    for (int ht = 0; ht < height; ht += 4) {
      __m256i src[16];
      // Load source data.
      load_from_src_buf(ref, &src[0], ref_stride);
      load_from_src_buf(ref + 32, &src[4], ref_stride);
      load_from_src_buf(ref + 64, &src[8], ref_stride);
      load_from_src_buf(ref + 96, &src[12], ref_stride);

      // Row0 output: r00 x x x r01 x x x | r02 x x x r03 x x x
      const __m256i s0 = _mm256_add_epi16(_mm256_sad_epu8(src[0], zero),
                                          _mm256_sad_epu8(src[4], zero));
      const __m256i s1 = _mm256_add_epi16(_mm256_sad_epu8(src[8], zero),
                                          _mm256_sad_epu8(src[12], zero));
      const __m256i r0 = _mm256_add_epi16(s0, s1);
      // Row1 output: r10 x x x r11 x x x | r12 x x x r13 x x x
      const __m256i s2 = _mm256_add_epi16(_mm256_sad_epu8(src[1], zero),
                                          _mm256_sad_epu8(src[5], zero));
      const __m256i s3 = _mm256_add_epi16(_mm256_sad_epu8(src[9], zero),
                                          _mm256_sad_epu8(src[13], zero));
      const __m256i r1 = _mm256_add_epi16(s2, s3);
      // Row2 output: r20 x x x r21 x x x | r22 x x x r23 x x x
      const __m256i s4 = _mm256_add_epi16(_mm256_sad_epu8(src[2], zero),
                                          _mm256_sad_epu8(src[6], zero));
      const __m256i s5 = _mm256_add_epi16(_mm256_sad_epu8(src[10], zero),
                                          _mm256_sad_epu8(src[14], zero));
      const __m256i r2 = _mm256_add_epi16(s4, s5);
      // Row3 output: r30 x x x r31 x x x | r32 x x x r33 x x x
      const __m256i s6 = _mm256_add_epi16(_mm256_sad_epu8(src[3], zero),
                                          _mm256_sad_epu8(src[7], zero));
      const __m256i s7 = _mm256_add_epi16(_mm256_sad_epu8(src[11], zero),
                                          _mm256_sad_epu8(src[15], zero));
      const __m256i r3 = _mm256_add_epi16(s6, s7);

      CALC_TOT_SAD_AND_STORE
      vbuf += 4;
      ref += ref_stride << 2;
    }
  } else if (width == 64) {
    const __m256i zero = _mm256_setzero_si256();
    for (int ht = 0; ht < height; ht += 4) {
      __m256i src[8];
      // Load source data.
      load_from_src_buf(ref, &src[0], ref_stride);
      load_from_src_buf(ref + 32, &src[4], ref_stride);

      // Row0 output: r00 x x x r01 x x x | r02 x x x r03 x x x
      const __m256i s0 = _mm256_sad_epu8(src[0], zero);
      const __m256i s1 = _mm256_sad_epu8(src[4], zero);
      const __m256i r0 = _mm256_add_epi16(s0, s1);
      // Row1 output: r10 x x x r11 x x x | r12 x x x r13 x x x
      const __m256i s2 = _mm256_sad_epu8(src[1], zero);
      const __m256i s3 = _mm256_sad_epu8(src[5], zero);
      const __m256i r1 = _mm256_add_epi16(s2, s3);
      // Row2 output: r20 x x x r21 x x x | r22 x x x r23 x x x
      const __m256i s4 = _mm256_sad_epu8(src[2], zero);
      const __m256i s5 = _mm256_sad_epu8(src[6], zero);
      const __m256i r2 = _mm256_add_epi16(s4, s5);
      // Row3 output: r30 x x x r31 x x x | r32 x x x r33 x x x
      const __m256i s6 = _mm256_sad_epu8(src[3], zero);
      const __m256i s7 = _mm256_sad_epu8(src[7], zero);
      const __m256i r3 = _mm256_add_epi16(s6, s7);

      CALC_TOT_SAD_AND_STORE
      vbuf += 4;
      ref += ref_stride << 2;
    }
  } else if (width == 32) {
    assert(height % 2 == 0);
    const __m256i zero = _mm256_setzero_si256();
    for (int ht = 0; ht < height; ht += 4) {
      __m256i src[4];
      // Load source data.
      load_from_src_buf(ref, &src[0], ref_stride);

      // s00 x x x s01 x x x s02 x x x s03 x x x
      const __m256i r0 = _mm256_sad_epu8(src[0], zero);
      // s10 x x x s11 x x x s12 x x x s13 x x x
      const __m256i r1 = _mm256_sad_epu8(src[1], zero);
      // s20 x x x s21 x x x s22 x x x s23 x x x
      const __m256i r2 = _mm256_sad_epu8(src[2], zero);
      // s30 x x x s31 x x x s32 x x x s33 x x x
      const __m256i r3 = _mm256_sad_epu8(src[3], zero);

      CALC_TOT_SAD_AND_STORE
      vbuf += 4;
      ref += ref_stride << 2;
    }
  } else if (width == 16) {
    aom_int_pro_col_16wd_avx2(vbuf, ref, ref_stride, height, norm_factor);
  }
}

static inline void calc_vector_mean_sse_64wd(const int16_t *ref,
                                             const int16_t *src, __m256i *mean,
                                             __m256i *sse) {
  const __m256i src_line0 = _mm256_loadu_si256((const __m256i *)src);
  const __m256i src_line1 = _mm256_loadu_si256((const __m256i *)(src + 16));
  const __m256i src_line2 = _mm256_loadu_si256((const __m256i *)(src + 32));
  const __m256i src_line3 = _mm256_loadu_si256((const __m256i *)(src + 48));
  const __m256i ref_line0 = _mm256_loadu_si256((const __m256i *)ref);
  const __m256i ref_line1 = _mm256_loadu_si256((const __m256i *)(ref + 16));
  const __m256i ref_line2 = _mm256_loadu_si256((const __m256i *)(ref + 32));
  const __m256i ref_line3 = _mm256_loadu_si256((const __m256i *)(ref + 48));

  const __m256i diff0 = _mm256_sub_epi16(ref_line0, src_line0);
  const __m256i diff1 = _mm256_sub_epi16(ref_line1, src_line1);
  const __m256i diff2 = _mm256_sub_epi16(ref_line2, src_line2);
  const __m256i diff3 = _mm256_sub_epi16(ref_line3, src_line3);
  const __m256i diff_sqr0 = _mm256_madd_epi16(diff0, diff0);
  const __m256i diff_sqr1 = _mm256_madd_epi16(diff1, diff1);
  const __m256i diff_sqr2 = _mm256_madd_epi16(diff2, diff2);
  const __m256i diff_sqr3 = _mm256_madd_epi16(diff3, diff3);

  *mean = _mm256_add_epi16(*mean, _mm256_add_epi16(diff0, diff1));
  *mean = _mm256_add_epi16(*mean, diff2);
  *mean = _mm256_add_epi16(*mean, diff3);
  *sse = _mm256_add_epi32(*sse, _mm256_add_epi32(diff_sqr0, diff_sqr1));
  *sse = _mm256_add_epi32(*sse, diff_sqr2);
  *sse = _mm256_add_epi32(*sse, diff_sqr3);
}

#define CALC_VAR_FROM_MEAN_SSE(mean, sse)                                    \
  {                                                                          \
    mean = _mm256_madd_epi16(mean, _mm256_set1_epi16(1));                    \
    mean = _mm256_hadd_epi32(mean, sse);                                     \
    mean = _mm256_add_epi32(mean, _mm256_bsrli_epi128(mean, 4));             \
    const __m128i result = _mm_add_epi32(_mm256_castsi256_si128(mean),       \
                                         _mm256_extractf128_si256(mean, 1)); \
    /*(mean * mean): dynamic range 31 bits.*/                                \
    const int mean_int = _mm_extract_epi32(result, 0);                       \
    const int sse_int = _mm_extract_epi32(result, 2);                        \
    const unsigned int mean_abs = abs(mean_int);                             \
    var = sse_int - ((mean_abs * mean_abs) >> (bwl + 2));                    \
  }

// ref: [0 - 510]
// src: [0 - 510]
// bwl: {2, 3, 4, 5}
int aom_vector_var_avx2(const int16_t *ref, const int16_t *src, int bwl) {
  const int width = 4 << bwl;
  assert(width % 16 == 0 && width <= 128);
  int var = 0;

  // Instead of having a loop over width 16, considered loop unrolling to avoid
  // some addition operations.
  if (width == 128) {
    __m256i mean = _mm256_setzero_si256();
    __m256i sse = _mm256_setzero_si256();

    calc_vector_mean_sse_64wd(src, ref, &mean, &sse);
    calc_vector_mean_sse_64wd(src + 64, ref + 64, &mean, &sse);
    CALC_VAR_FROM_MEAN_SSE(mean, sse)
  } else if (width == 64) {
    __m256i mean = _mm256_setzero_si256();
    __m256i sse = _mm256_setzero_si256();

    calc_vector_mean_sse_64wd(src, ref, &mean, &sse);
    CALC_VAR_FROM_MEAN_SSE(mean, sse)
  } else if (width == 32) {
    const __m256i src_line0 = _mm256_loadu_si256((const __m256i *)src);
    const __m256i ref_line0 = _mm256_loadu_si256((const __m256i *)ref);
    const __m256i src_line1 = _mm256_loadu_si256((const __m256i *)(src + 16));
    const __m256i ref_line1 = _mm256_loadu_si256((const __m256i *)(ref + 16));

    const __m256i diff0 = _mm256_sub_epi16(ref_line0, src_line0);
    const __m256i diff1 = _mm256_sub_epi16(ref_line1, src_line1);
    const __m256i diff_sqr0 = _mm256_madd_epi16(diff0, diff0);
    const __m256i diff_sqr1 = _mm256_madd_epi16(diff1, diff1);
    const __m256i sse = _mm256_add_epi32(diff_sqr0, diff_sqr1);
    __m256i mean = _mm256_add_epi16(diff0, diff1);

    CALC_VAR_FROM_MEAN_SSE(mean, sse)
  } else if (width == 16) {
    const __m256i src_line = _mm256_loadu_si256((const __m256i *)src);
    const __m256i ref_line = _mm256_loadu_si256((const __m256i *)ref);
    __m256i mean = _mm256_sub_epi16(ref_line, src_line);
    const __m256i sse = _mm256_madd_epi16(mean, mean);

    CALC_VAR_FROM_MEAN_SSE(mean, sse)
  }
  return var;
}
