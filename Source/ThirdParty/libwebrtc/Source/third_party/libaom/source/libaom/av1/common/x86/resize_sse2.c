/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
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

#include "av1/common/resize.h"

#include "aom_dsp/x86/synonyms.h"

#define PROCESS_RESIZE_Y_WD8                                           \
  /* ah0 ah1 ... ah7 */                                                \
  const __m128i AH = _mm_add_epi16(l0, l7);                            \
  /* bg0 bg1 ... bh7 */                                                \
  const __m128i BG = _mm_add_epi16(l1, l6);                            \
  /* cf0 cf1 ... cf7 */                                                \
  const __m128i CF = _mm_add_epi16(l2, l5);                            \
  /* de0 de1 ... de7 */                                                \
  const __m128i DE = _mm_add_epi16(l3, l4);                            \
                                                                       \
  /* ah0 bg0 ... ah3 bg3 */                                            \
  const __m128i AHBG_low = _mm_unpacklo_epi16(AH, BG);                 \
  /*cf0 de0 ... cf2 de2 */                                             \
  const __m128i CFDE_low = _mm_unpacklo_epi16(CF, DE);                 \
                                                                       \
  /* ah4 bg4... ah7 bg7 */                                             \
  const __m128i AHBG_hi = _mm_unpackhi_epi16(AH, BG);                  \
  /* cf4 de4... cf7 de7 */                                             \
  const __m128i CFDE_hi = _mm_unpackhi_epi16(CF, DE);                  \
                                                                       \
  /* r00 r01 r02 r03 */                                                \
  const __m128i r00 = _mm_madd_epi16(AHBG_low, coeffs_y[0]);           \
  const __m128i r01 = _mm_madd_epi16(CFDE_low, coeffs_y[1]);           \
  __m128i r0 = _mm_add_epi32(r00, r01);                                \
  /* r04 r05 r06 r07 */                                                \
  const __m128i r10 = _mm_madd_epi16(AHBG_hi, coeffs_y[0]);            \
  const __m128i r11 = _mm_madd_epi16(CFDE_hi, coeffs_y[1]);            \
  __m128i r1 = _mm_add_epi32(r10, r11);                                \
                                                                       \
  r0 = _mm_add_epi32(r0, round_const_bits);                            \
  r1 = _mm_add_epi32(r1, round_const_bits);                            \
  r0 = _mm_sra_epi32(r0, round_shift_bits);                            \
  r1 = _mm_sra_epi32(r1, round_shift_bits);                            \
                                                                       \
  /* r00 ... r07 (8 values of each 16bit) */                           \
  const __m128i res_16b = _mm_packs_epi32(r0, r1);                     \
  /* r00 ... r07 | r00 ... r07 (16 values of each 8bit) */             \
  const __m128i res_8b0 = _mm_packus_epi16(res_16b, res_16b);          \
                                                                       \
  __m128i res = _mm_min_epu8(res_8b0, clip_pixel);                     \
  res = _mm_max_epu8(res, zero);                                       \
  _mm_storel_epi64((__m128i *)&output[(i / 2) * out_stride + j], res); \
                                                                       \
  l0 = l2;                                                             \
  l1 = l3;                                                             \
  l2 = l4;                                                             \
  l3 = l5;                                                             \
  l4 = l6;                                                             \
  l5 = l7;                                                             \
  data += 2 * stride;

static INLINE void prepare_filter_coeffs(const int16_t *filter,
                                         __m128i *const coeffs /* [2] */) {
  // f0 f1 f2 f3 x x x x
  const __m128i sym_even_filter = _mm_loadl_epi64((__m128i *)filter);

  // f1 f0 f3 f2 x x x x
  const __m128i tmp1 = _mm_shufflelo_epi16(sym_even_filter, 0xb1);

  // f3 f2 f3 f2 ...
  coeffs[0] = _mm_shuffle_epi32(tmp1, 0x55);
  // f1 f0 f1 f0 ...
  coeffs[1] = _mm_shuffle_epi32(tmp1, 0x00);
}

bool av1_resize_vert_dir_sse2(uint8_t *intbuf, uint8_t *output, int out_stride,
                              int height, int height2, int stride,
                              int start_col) {
  // For the GM tool, the input layer height or width is assured to be an even
  // number. Hence the function 'down2_symodd()' is not invoked and SIMD
  // optimization of the same is not implemented.
  // When the input height is less than 8 and even, the potential input
  // heights are limited to 2, 4, or 6. These scenarios require seperate
  // handling due to padding requirements. Invoking the C function here will
  // eliminate the need for conditional statements within the subsequent SIMD
  // code to manage these cases.
  if (height & 1 || height < 8) {
    return av1_resize_vert_dir_c(intbuf, output, out_stride, height, height2,
                                 stride, start_col);
  }

  __m128i coeffs_y[2];
  const int bits = FILTER_BITS;
  const __m128i round_const_bits = _mm_set1_epi32((1 << bits) >> 1);
  const __m128i round_shift_bits = _mm_cvtsi32_si128(bits);
  const uint8_t max_pixel = 255;
  const __m128i clip_pixel = _mm_set1_epi8(max_pixel);
  const __m128i zero = _mm_setzero_si128();
  prepare_filter_coeffs(av1_down2_symeven_half_filter, coeffs_y);

  const int remain_col = stride % 8;

  for (int j = start_col; j < stride - remain_col; j += 8) {
    uint8_t *data = &intbuf[j];
    // d0 ... d7
    const __m128i l8_3 = _mm_loadl_epi64((__m128i *)(data + 0 * stride));
    // Padding top 3 rows with the last available row at the top.
    // a0 ... a7
    const __m128i l8_0 = l8_3;
    // b0 ... b7
    const __m128i l8_1 = l8_3;
    // c0 ... c7
    const __m128i l8_2 = l8_3;
    // e0 ... e7
    const __m128i l8_4 = _mm_loadl_epi64((__m128i *)(data + 1 * stride));
    // f0 ... f7
    const __m128i l8_5 = _mm_loadl_epi64((__m128i *)(data + 2 * stride));

    // Convert to 16bit as addition of 2 source pixel crosses 8 bit.
    __m128i l0 = _mm_unpacklo_epi8(l8_0, zero);  // A(128bit) = a0 - a7(16 bit)
    __m128i l1 = _mm_unpacklo_epi8(l8_1, zero);  // B(128bit) = b0 - b7(16 bit)
    __m128i l2 = _mm_unpacklo_epi8(l8_2, zero);  // C(128bit) = c0 - c7(16 bit)
    __m128i l3 = _mm_unpacklo_epi8(l8_3, zero);  // D(128bit) = d0 - d7(16 bit)
    __m128i l4 = _mm_unpacklo_epi8(l8_4, zero);  // E(128bit) = e0 - e7(16 bit)
    __m128i l5 = _mm_unpacklo_epi8(l8_5, zero);  // F(128bit) = f0 - f7(16 bit)

    // Increment the pointer such that the loading starts from row G.
    data = data + 3 * stride;
    // The core vertical SIMD processes 2 input rows simultaneously to generate
    // output corresponding to 1 row. To streamline the core loop and eliminate
    // the need for conditional checks, the remaining rows 4 are processed
    // separately.
    for (int i = 0; i < height - 4; i += 2) {
      // g0 ... g7
      __m128i l8_6 = _mm_loadl_epi64((__m128i *)(data));
      // h0 ... h7
      __m128i l8_7 = _mm_loadl_epi64((__m128i *)(data + stride));
      __m128i l6 = _mm_unpacklo_epi8(l8_6, zero);  // G(128bit):g0-g7(16b)
      __m128i l7 = _mm_unpacklo_epi8(l8_7, zero);  // H(128bit):h0-h7(16b)

      PROCESS_RESIZE_Y_WD8
    }

    __m128i l8_6 = _mm_loadl_epi64((__m128i *)(data));
    __m128i l6 = _mm_unpacklo_epi8(l8_6, zero);
    // Process the last 4 input rows here.
    for (int i = height - 4; i < height; i += 2) {
      __m128i l7 = l6;
      PROCESS_RESIZE_Y_WD8
    }
  }

  if (remain_col)
    return av1_resize_vert_dir_c(intbuf, output, out_stride, height, height2,
                                 stride, stride - remain_col);

  return true;
}
