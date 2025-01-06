/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <emmintrin.h>
#include "aom_dsp/x86/intrapred_x86.h"
#include "config/aom_dsp_rtcd.h"

static inline void dc_store_4xh(uint32_t dc, int height, uint8_t *dst,
                                ptrdiff_t stride) {
  for (int i = 0; i < height; i += 2) {
    *(uint32_t *)dst = dc;
    dst += stride;
    *(uint32_t *)dst = dc;
    dst += stride;
  }
}

static inline void dc_store_8xh(const __m128i *row, int height, uint8_t *dst,
                                ptrdiff_t stride) {
  int i;
  for (i = 0; i < height; ++i) {
    _mm_storel_epi64((__m128i *)dst, *row);
    dst += stride;
  }
}

static inline void dc_store_16xh(const __m128i *row, int height, uint8_t *dst,
                                 ptrdiff_t stride) {
  int i;
  for (i = 0; i < height; ++i) {
    _mm_store_si128((__m128i *)dst, *row);
    dst += stride;
  }
}

static inline void dc_store_32xh(const __m128i *row, int height, uint8_t *dst,
                                 ptrdiff_t stride) {
  int i;
  for (i = 0; i < height; ++i) {
    _mm_store_si128((__m128i *)dst, *row);
    _mm_store_si128((__m128i *)(dst + 16), *row);
    dst += stride;
  }
}

static inline void dc_store_64xh(const __m128i *row, int height, uint8_t *dst,
                                 ptrdiff_t stride) {
  for (int i = 0; i < height; ++i) {
    _mm_store_si128((__m128i *)dst, *row);
    _mm_store_si128((__m128i *)(dst + 16), *row);
    _mm_store_si128((__m128i *)(dst + 32), *row);
    _mm_store_si128((__m128i *)(dst + 48), *row);
    dst += stride;
  }
}

static inline __m128i dc_sum_4(const uint8_t *ref) {
  __m128i x = _mm_loadl_epi64((__m128i const *)ref);
  const __m128i zero = _mm_setzero_si128();
  x = _mm_unpacklo_epi8(x, zero);
  return _mm_sad_epu8(x, zero);
}

static inline __m128i dc_sum_8(const uint8_t *ref) {
  __m128i x = _mm_loadl_epi64((__m128i const *)ref);
  const __m128i zero = _mm_setzero_si128();
  return _mm_sad_epu8(x, zero);
}

static inline __m128i dc_sum_64(const uint8_t *ref) {
  __m128i x0 = _mm_load_si128((__m128i const *)ref);
  __m128i x1 = _mm_load_si128((__m128i const *)(ref + 16));
  __m128i x2 = _mm_load_si128((__m128i const *)(ref + 32));
  __m128i x3 = _mm_load_si128((__m128i const *)(ref + 48));
  const __m128i zero = _mm_setzero_si128();
  x0 = _mm_sad_epu8(x0, zero);
  x1 = _mm_sad_epu8(x1, zero);
  x2 = _mm_sad_epu8(x2, zero);
  x3 = _mm_sad_epu8(x3, zero);
  x0 = _mm_add_epi16(x0, x1);
  x2 = _mm_add_epi16(x2, x3);
  x0 = _mm_add_epi16(x0, x2);
  const __m128i high = _mm_unpackhi_epi64(x0, x0);
  return _mm_add_epi16(x0, high);
}

#define DC_MULTIPLIER_1X2 0x5556
#define DC_MULTIPLIER_1X4 0x3334

#define DC_SHIFT2 16

static inline int divide_using_multiply_shift(int num, int shift1,
                                              int multiplier) {
  const int interm = num >> shift1;
  return interm * multiplier >> DC_SHIFT2;
}

// -----------------------------------------------------------------------------
// DC_PRED

void aom_dc_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const __m128i sum_left = dc_sum_8(left);
  __m128i sum_above = dc_sum_4(above);
  sum_above = _mm_add_epi16(sum_left, sum_above);

  uint32_t sum = (uint32_t)_mm_cvtsi128_si32(sum_above);
  sum += 6;
  sum = divide_using_multiply_shift(sum, 2, DC_MULTIPLIER_1X2);

  const __m128i row = _mm_set1_epi8((int8_t)sum);
  const uint32_t pred = (uint32_t)_mm_cvtsi128_si32(row);
  dc_store_4xh(pred, 8, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_dc_predictor_4x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m128i sum_left = dc_sum_16_sse2(left);
  __m128i sum_above = dc_sum_4(above);
  sum_above = _mm_add_epi16(sum_left, sum_above);

  uint32_t sum = (uint32_t)_mm_cvtsi128_si32(sum_above);
  sum += 10;
  sum = divide_using_multiply_shift(sum, 2, DC_MULTIPLIER_1X4);

  const __m128i row = _mm_set1_epi8((int8_t)sum);
  const uint32_t pred = (uint32_t)_mm_cvtsi128_si32(row);
  dc_store_4xh(pred, 16, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_dc_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const __m128i sum_left = dc_sum_4(left);
  __m128i sum_above = dc_sum_8(above);
  sum_above = _mm_add_epi16(sum_above, sum_left);

  uint32_t sum = (uint32_t)_mm_cvtsi128_si32(sum_above);
  sum += 6;
  sum = divide_using_multiply_shift(sum, 2, DC_MULTIPLIER_1X2);

  const __m128i row = _mm_set1_epi8((int8_t)sum);
  dc_store_8xh(&row, 4, dst, stride);
}

void aom_dc_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m128i sum_left = dc_sum_16_sse2(left);
  __m128i sum_above = dc_sum_8(above);
  sum_above = _mm_add_epi16(sum_above, sum_left);

  uint32_t sum = (uint32_t)_mm_cvtsi128_si32(sum_above);
  sum += 12;
  sum = divide_using_multiply_shift(sum, 3, DC_MULTIPLIER_1X2);
  const __m128i row = _mm_set1_epi8((int8_t)sum);
  dc_store_8xh(&row, 16, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_dc_predictor_8x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m128i sum_left = dc_sum_32_sse2(left);
  __m128i sum_above = dc_sum_8(above);
  sum_above = _mm_add_epi16(sum_above, sum_left);

  uint32_t sum = (uint32_t)_mm_cvtsi128_si32(sum_above);
  sum += 20;
  sum = divide_using_multiply_shift(sum, 3, DC_MULTIPLIER_1X4);
  const __m128i row = _mm_set1_epi8((int8_t)sum);
  dc_store_8xh(&row, 32, dst, stride);
}

void aom_dc_predictor_16x4_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m128i sum_left = dc_sum_4(left);
  __m128i sum_above = dc_sum_16_sse2(above);
  sum_above = _mm_add_epi16(sum_above, sum_left);

  uint32_t sum = (uint32_t)_mm_cvtsi128_si32(sum_above);
  sum += 10;
  sum = divide_using_multiply_shift(sum, 2, DC_MULTIPLIER_1X4);
  const __m128i row = _mm_set1_epi8((int8_t)sum);
  dc_store_16xh(&row, 4, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_dc_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m128i sum_left = dc_sum_8(left);
  __m128i sum_above = dc_sum_16_sse2(above);
  sum_above = _mm_add_epi16(sum_above, sum_left);

  uint32_t sum = (uint32_t)_mm_cvtsi128_si32(sum_above);
  sum += 12;
  sum = divide_using_multiply_shift(sum, 3, DC_MULTIPLIER_1X2);
  const __m128i row = _mm_set1_epi8((int8_t)sum);
  dc_store_16xh(&row, 8, dst, stride);
}

void aom_dc_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  const __m128i sum_left = dc_sum_32_sse2(left);
  __m128i sum_above = dc_sum_16_sse2(above);
  sum_above = _mm_add_epi16(sum_left, sum_above);

  uint32_t sum = (uint32_t)_mm_cvtsi128_si32(sum_above);
  sum += 24;
  sum = divide_using_multiply_shift(sum, 4, DC_MULTIPLIER_1X2);
  const __m128i row = _mm_set1_epi8((int8_t)sum);
  dc_store_16xh(&row, 32, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_dc_predictor_16x64_sse2(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  const __m128i sum_left = dc_sum_64(left);
  __m128i sum_above = dc_sum_16_sse2(above);
  sum_above = _mm_add_epi16(sum_left, sum_above);

  uint32_t sum = (uint32_t)_mm_cvtsi128_si32(sum_above);
  sum += 40;
  sum = divide_using_multiply_shift(sum, 4, DC_MULTIPLIER_1X4);
  const __m128i row = _mm_set1_epi8((int8_t)sum);
  dc_store_16xh(&row, 64, dst, stride);
}

void aom_dc_predictor_32x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  __m128i sum_above = dc_sum_32_sse2(above);
  const __m128i sum_left = dc_sum_8(left);
  sum_above = _mm_add_epi16(sum_above, sum_left);

  uint32_t sum = (uint32_t)_mm_cvtsi128_si32(sum_above);
  sum += 20;
  sum = divide_using_multiply_shift(sum, 3, DC_MULTIPLIER_1X4);
  const __m128i row = _mm_set1_epi8((int8_t)sum);
  dc_store_32xh(&row, 8, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_dc_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  __m128i sum_above = dc_sum_32_sse2(above);
  const __m128i sum_left = dc_sum_16_sse2(left);
  sum_above = _mm_add_epi16(sum_above, sum_left);

  uint32_t sum = (uint32_t)_mm_cvtsi128_si32(sum_above);
  sum += 24;
  sum = divide_using_multiply_shift(sum, 4, DC_MULTIPLIER_1X2);
  const __m128i row = _mm_set1_epi8((int8_t)sum);
  dc_store_32xh(&row, 16, dst, stride);
}

void aom_dc_predictor_32x64_sse2(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  __m128i sum_above = dc_sum_32_sse2(above);
  const __m128i sum_left = dc_sum_64(left);
  sum_above = _mm_add_epi16(sum_above, sum_left);

  uint32_t sum = (uint32_t)_mm_cvtsi128_si32(sum_above);
  sum += 48;
  sum = divide_using_multiply_shift(sum, 5, DC_MULTIPLIER_1X2);
  const __m128i row = _mm_set1_epi8((int8_t)sum);
  dc_store_32xh(&row, 64, dst, stride);
}

void aom_dc_predictor_64x64_sse2(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  __m128i sum_above = dc_sum_64(above);
  const __m128i sum_left = dc_sum_64(left);
  sum_above = _mm_add_epi16(sum_above, sum_left);

  uint32_t sum = (uint32_t)_mm_cvtsi128_si32(sum_above);
  sum += 64;
  sum /= 128;
  const __m128i row = _mm_set1_epi8((int8_t)sum);
  dc_store_64xh(&row, 64, dst, stride);
}

void aom_dc_predictor_64x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  __m128i sum_above = dc_sum_64(above);
  const __m128i sum_left = dc_sum_32_sse2(left);
  sum_above = _mm_add_epi16(sum_above, sum_left);

  uint32_t sum = (uint32_t)_mm_cvtsi128_si32(sum_above);
  sum += 48;
  sum = divide_using_multiply_shift(sum, 5, DC_MULTIPLIER_1X2);
  const __m128i row = _mm_set1_epi8((int8_t)sum);
  dc_store_64xh(&row, 32, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_dc_predictor_64x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                 const uint8_t *above, const uint8_t *left) {
  __m128i sum_above = dc_sum_64(above);
  const __m128i sum_left = dc_sum_16_sse2(left);
  sum_above = _mm_add_epi16(sum_above, sum_left);

  uint32_t sum = (uint32_t)_mm_cvtsi128_si32(sum_above);
  sum += 40;
  sum = divide_using_multiply_shift(sum, 4, DC_MULTIPLIER_1X4);
  const __m128i row = _mm_set1_epi8((int8_t)sum);
  dc_store_64xh(&row, 16, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

// -----------------------------------------------------------------------------
// DC_TOP

void aom_dc_top_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_4(above);
  const __m128i two = _mm_set1_epi16(2);
  sum_above = _mm_add_epi16(sum_above, two);
  sum_above = _mm_srai_epi16(sum_above, 2);
  sum_above = _mm_shufflelo_epi16(sum_above, 0);
  sum_above = _mm_packus_epi16(sum_above, sum_above);

  const uint32_t pred = (uint32_t)_mm_cvtsi128_si32(sum_above);
  dc_store_4xh(pred, 8, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_dc_top_predictor_4x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_4(above);
  const __m128i two = _mm_set1_epi16(2);
  sum_above = _mm_add_epi16(sum_above, two);
  sum_above = _mm_srai_epi16(sum_above, 2);
  sum_above = _mm_shufflelo_epi16(sum_above, 0);
  sum_above = _mm_packus_epi16(sum_above, sum_above);

  const uint32_t pred = (uint32_t)_mm_cvtsi128_si32(sum_above);
  dc_store_4xh(pred, 16, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_dc_top_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_8(above);
  const __m128i four = _mm_set1_epi16(4);
  sum_above = _mm_add_epi16(sum_above, four);
  sum_above = _mm_srai_epi16(sum_above, 3);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  const __m128i row = _mm_shufflelo_epi16(sum_above, 0);
  dc_store_8xh(&row, 4, dst, stride);
}

void aom_dc_top_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_8(above);
  const __m128i four = _mm_set1_epi16(4);
  sum_above = _mm_add_epi16(sum_above, four);
  sum_above = _mm_srai_epi16(sum_above, 3);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  const __m128i row = _mm_shufflelo_epi16(sum_above, 0);
  dc_store_8xh(&row, 16, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_dc_top_predictor_8x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_8(above);
  const __m128i four = _mm_set1_epi16(4);
  sum_above = _mm_add_epi16(sum_above, four);
  sum_above = _mm_srai_epi16(sum_above, 3);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  const __m128i row = _mm_shufflelo_epi16(sum_above, 0);
  dc_store_8xh(&row, 32, dst, stride);
}

void aom_dc_top_predictor_16x4_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_16_sse2(above);
  const __m128i eight = _mm_set1_epi16(8);
  sum_above = _mm_add_epi16(sum_above, eight);
  sum_above = _mm_srai_epi16(sum_above, 4);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  sum_above = _mm_shufflelo_epi16(sum_above, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_above, sum_above);
  dc_store_16xh(&row, 4, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_dc_top_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_16_sse2(above);
  const __m128i eight = _mm_set1_epi16(8);
  sum_above = _mm_add_epi16(sum_above, eight);
  sum_above = _mm_srai_epi16(sum_above, 4);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  sum_above = _mm_shufflelo_epi16(sum_above, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_above, sum_above);
  dc_store_16xh(&row, 8, dst, stride);
}

void aom_dc_top_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_16_sse2(above);
  const __m128i eight = _mm_set1_epi16(8);
  sum_above = _mm_add_epi16(sum_above, eight);
  sum_above = _mm_srai_epi16(sum_above, 4);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  sum_above = _mm_shufflelo_epi16(sum_above, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_above, sum_above);
  dc_store_16xh(&row, 32, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_dc_top_predictor_16x64_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_16_sse2(above);
  const __m128i eight = _mm_set1_epi16(8);
  sum_above = _mm_add_epi16(sum_above, eight);
  sum_above = _mm_srai_epi16(sum_above, 4);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  sum_above = _mm_shufflelo_epi16(sum_above, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_above, sum_above);
  dc_store_16xh(&row, 64, dst, stride);
}

void aom_dc_top_predictor_32x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_32_sse2(above);
  const __m128i sixteen = _mm_set1_epi16(16);
  sum_above = _mm_add_epi16(sum_above, sixteen);
  sum_above = _mm_srai_epi16(sum_above, 5);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  sum_above = _mm_shufflelo_epi16(sum_above, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_above, sum_above);
  dc_store_32xh(&row, 8, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_dc_top_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_32_sse2(above);
  const __m128i sixteen = _mm_set1_epi16(16);
  sum_above = _mm_add_epi16(sum_above, sixteen);
  sum_above = _mm_srai_epi16(sum_above, 5);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  sum_above = _mm_shufflelo_epi16(sum_above, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_above, sum_above);
  dc_store_32xh(&row, 16, dst, stride);
}

void aom_dc_top_predictor_32x64_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_32_sse2(above);
  const __m128i sixteen = _mm_set1_epi16(16);
  sum_above = _mm_add_epi16(sum_above, sixteen);
  sum_above = _mm_srai_epi16(sum_above, 5);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  sum_above = _mm_shufflelo_epi16(sum_above, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_above, sum_above);
  dc_store_32xh(&row, 64, dst, stride);
}

void aom_dc_top_predictor_64x64_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_64(above);
  const __m128i thirtytwo = _mm_set1_epi16(32);
  sum_above = _mm_add_epi16(sum_above, thirtytwo);
  sum_above = _mm_srai_epi16(sum_above, 6);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  sum_above = _mm_shufflelo_epi16(sum_above, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_above, sum_above);
  dc_store_64xh(&row, 64, dst, stride);
}

void aom_dc_top_predictor_64x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_64(above);
  const __m128i thirtytwo = _mm_set1_epi16(32);
  sum_above = _mm_add_epi16(sum_above, thirtytwo);
  sum_above = _mm_srai_epi16(sum_above, 6);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  sum_above = _mm_shufflelo_epi16(sum_above, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_above, sum_above);
  dc_store_64xh(&row, 32, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_dc_top_predictor_64x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)left;
  __m128i sum_above = dc_sum_64(above);
  const __m128i thirtytwo = _mm_set1_epi16(32);
  sum_above = _mm_add_epi16(sum_above, thirtytwo);
  sum_above = _mm_srai_epi16(sum_above, 6);
  sum_above = _mm_unpacklo_epi8(sum_above, sum_above);
  sum_above = _mm_shufflelo_epi16(sum_above, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_above, sum_above);
  dc_store_64xh(&row, 16, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

// -----------------------------------------------------------------------------
// DC_LEFT

void aom_dc_left_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_8(left);
  const __m128i four = _mm_set1_epi16(4);
  sum_left = _mm_add_epi16(sum_left, four);
  sum_left = _mm_srai_epi16(sum_left, 3);
  sum_left = _mm_shufflelo_epi16(sum_left, 0);
  sum_left = _mm_packus_epi16(sum_left, sum_left);

  const uint32_t pred = (uint32_t)_mm_cvtsi128_si32(sum_left);
  dc_store_4xh(pred, 8, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_dc_left_predictor_4x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_16_sse2(left);
  const __m128i eight = _mm_set1_epi16(8);
  sum_left = _mm_add_epi16(sum_left, eight);
  sum_left = _mm_srai_epi16(sum_left, 4);
  sum_left = _mm_shufflelo_epi16(sum_left, 0);
  sum_left = _mm_packus_epi16(sum_left, sum_left);

  const uint32_t pred = (uint32_t)_mm_cvtsi128_si32(sum_left);
  dc_store_4xh(pred, 16, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_dc_left_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_4(left);
  const __m128i two = _mm_set1_epi16(2);
  sum_left = _mm_add_epi16(sum_left, two);
  sum_left = _mm_srai_epi16(sum_left, 2);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  const __m128i row = _mm_shufflelo_epi16(sum_left, 0);
  dc_store_8xh(&row, 4, dst, stride);
}

void aom_dc_left_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_16_sse2(left);
  const __m128i eight = _mm_set1_epi16(8);
  sum_left = _mm_add_epi16(sum_left, eight);
  sum_left = _mm_srai_epi16(sum_left, 4);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  const __m128i row = _mm_shufflelo_epi16(sum_left, 0);
  dc_store_8xh(&row, 16, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_dc_left_predictor_8x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_32_sse2(left);
  const __m128i sixteen = _mm_set1_epi16(16);
  sum_left = _mm_add_epi16(sum_left, sixteen);
  sum_left = _mm_srai_epi16(sum_left, 5);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  const __m128i row = _mm_shufflelo_epi16(sum_left, 0);
  dc_store_8xh(&row, 32, dst, stride);
}

void aom_dc_left_predictor_16x4_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_4(left);
  const __m128i two = _mm_set1_epi16(2);
  sum_left = _mm_add_epi16(sum_left, two);
  sum_left = _mm_srai_epi16(sum_left, 2);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  sum_left = _mm_shufflelo_epi16(sum_left, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_left, sum_left);
  dc_store_16xh(&row, 4, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_dc_left_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_8(left);
  const __m128i four = _mm_set1_epi16(4);
  sum_left = _mm_add_epi16(sum_left, four);
  sum_left = _mm_srai_epi16(sum_left, 3);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  sum_left = _mm_shufflelo_epi16(sum_left, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_left, sum_left);
  dc_store_16xh(&row, 8, dst, stride);
}

void aom_dc_left_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_32_sse2(left);
  const __m128i sixteen = _mm_set1_epi16(16);
  sum_left = _mm_add_epi16(sum_left, sixteen);
  sum_left = _mm_srai_epi16(sum_left, 5);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  sum_left = _mm_shufflelo_epi16(sum_left, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_left, sum_left);
  dc_store_16xh(&row, 32, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_dc_left_predictor_16x64_sse2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_64(left);
  const __m128i thirtytwo = _mm_set1_epi16(32);
  sum_left = _mm_add_epi16(sum_left, thirtytwo);
  sum_left = _mm_srai_epi16(sum_left, 6);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  sum_left = _mm_shufflelo_epi16(sum_left, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_left, sum_left);
  dc_store_16xh(&row, 64, dst, stride);
}

void aom_dc_left_predictor_32x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_8(left);
  const __m128i four = _mm_set1_epi16(4);
  sum_left = _mm_add_epi16(sum_left, four);
  sum_left = _mm_srai_epi16(sum_left, 3);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  sum_left = _mm_shufflelo_epi16(sum_left, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_left, sum_left);
  dc_store_32xh(&row, 8, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_dc_left_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_16_sse2(left);
  const __m128i eight = _mm_set1_epi16(8);
  sum_left = _mm_add_epi16(sum_left, eight);
  sum_left = _mm_srai_epi16(sum_left, 4);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  sum_left = _mm_shufflelo_epi16(sum_left, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_left, sum_left);
  dc_store_32xh(&row, 16, dst, stride);
}

void aom_dc_left_predictor_32x64_sse2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_64(left);
  const __m128i thirtytwo = _mm_set1_epi16(32);
  sum_left = _mm_add_epi16(sum_left, thirtytwo);
  sum_left = _mm_srai_epi16(sum_left, 6);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  sum_left = _mm_shufflelo_epi16(sum_left, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_left, sum_left);
  dc_store_32xh(&row, 64, dst, stride);
}

void aom_dc_left_predictor_64x64_sse2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_64(left);
  const __m128i thirtytwo = _mm_set1_epi16(32);
  sum_left = _mm_add_epi16(sum_left, thirtytwo);
  sum_left = _mm_srai_epi16(sum_left, 6);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  sum_left = _mm_shufflelo_epi16(sum_left, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_left, sum_left);
  dc_store_64xh(&row, 64, dst, stride);
}

void aom_dc_left_predictor_64x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_32_sse2(left);
  const __m128i sixteen = _mm_set1_epi16(16);
  sum_left = _mm_add_epi16(sum_left, sixteen);
  sum_left = _mm_srai_epi16(sum_left, 5);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  sum_left = _mm_shufflelo_epi16(sum_left, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_left, sum_left);
  dc_store_64xh(&row, 32, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_dc_left_predictor_64x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  (void)above;
  __m128i sum_left = dc_sum_16_sse2(left);
  const __m128i eight = _mm_set1_epi16(8);
  sum_left = _mm_add_epi16(sum_left, eight);
  sum_left = _mm_srai_epi16(sum_left, 4);
  sum_left = _mm_unpacklo_epi8(sum_left, sum_left);
  sum_left = _mm_shufflelo_epi16(sum_left, 0);
  const __m128i row = _mm_unpacklo_epi64(sum_left, sum_left);
  dc_store_64xh(&row, 16, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

// -----------------------------------------------------------------------------
// DC_128

void aom_dc_128_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  (void)above;
  (void)left;
  const uint32_t pred = 0x80808080;
  dc_store_4xh(pred, 8, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_dc_128_predictor_4x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)above;
  (void)left;
  const uint32_t pred = 0x80808080;
  dc_store_4xh(pred, 16, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_dc_128_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((int8_t)128);
  dc_store_8xh(&row, 4, dst, stride);
}

void aom_dc_128_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((int8_t)128);
  dc_store_8xh(&row, 16, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_dc_128_predictor_8x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((int8_t)128);
  dc_store_8xh(&row, 32, dst, stride);
}

void aom_dc_128_predictor_16x4_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((int8_t)128);
  dc_store_16xh(&row, 4, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_dc_128_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((int8_t)128);
  dc_store_16xh(&row, 8, dst, stride);
}

void aom_dc_128_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((int8_t)128);
  dc_store_16xh(&row, 32, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_dc_128_predictor_16x64_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((int8_t)128);
  dc_store_16xh(&row, 64, dst, stride);
}

void aom_dc_128_predictor_32x8_sse2(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((int8_t)128);
  dc_store_32xh(&row, 8, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_dc_128_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((int8_t)128);
  dc_store_32xh(&row, 16, dst, stride);
}

void aom_dc_128_predictor_32x64_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((int8_t)128);
  dc_store_32xh(&row, 64, dst, stride);
}

void aom_dc_128_predictor_64x64_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((int8_t)128);
  dc_store_64xh(&row, 64, dst, stride);
}

void aom_dc_128_predictor_64x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((int8_t)128);
  dc_store_64xh(&row, 32, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_dc_128_predictor_64x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  (void)above;
  (void)left;
  const __m128i row = _mm_set1_epi8((int8_t)128);
  dc_store_64xh(&row, 16, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

// -----------------------------------------------------------------------------
// V_PRED

void aom_v_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const uint32_t pred = *(uint32_t *)above;
  (void)left;
  dc_store_4xh(pred, 8, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_v_predictor_4x16_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const uint32_t pred = *(uint32_t *)above;
  (void)left;
  dc_store_4xh(pred, 16, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_v_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const __m128i row = _mm_loadl_epi64((__m128i const *)above);
  (void)left;
  dc_store_8xh(&row, 4, dst, stride);
}

void aom_v_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const __m128i row = _mm_loadl_epi64((__m128i const *)above);
  (void)left;
  dc_store_8xh(&row, 16, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_v_predictor_8x32_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const __m128i row = _mm_loadl_epi64((__m128i const *)above);
  (void)left;
  dc_store_8xh(&row, 32, dst, stride);
}

void aom_v_predictor_16x4_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const __m128i row = _mm_load_si128((__m128i const *)above);
  (void)left;
  dc_store_16xh(&row, 4, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_v_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  const __m128i row = _mm_load_si128((__m128i const *)above);
  (void)left;
  dc_store_16xh(&row, 8, dst, stride);
}

void aom_v_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m128i row = _mm_load_si128((__m128i const *)above);
  (void)left;
  dc_store_16xh(&row, 32, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_v_predictor_16x64_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m128i row = _mm_load_si128((__m128i const *)above);
  (void)left;
  dc_store_16xh(&row, 64, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

static inline void v_predictor_32xh(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, int height) {
  const __m128i row0 = _mm_load_si128((__m128i const *)above);
  const __m128i row1 = _mm_load_si128((__m128i const *)(above + 16));
  for (int i = 0; i < height; ++i) {
    _mm_store_si128((__m128i *)dst, row0);
    _mm_store_si128((__m128i *)(dst + 16), row1);
    dst += stride;
  }
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_v_predictor_32x8_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_predictor_32xh(dst, stride, above, 8);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_v_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_predictor_32xh(dst, stride, above, 16);
}

void aom_v_predictor_32x64_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_predictor_32xh(dst, stride, above, 64);
}

static inline void v_predictor_64xh(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, int height) {
  const __m128i row0 = _mm_load_si128((__m128i const *)above);
  const __m128i row1 = _mm_load_si128((__m128i const *)(above + 16));
  const __m128i row2 = _mm_load_si128((__m128i const *)(above + 32));
  const __m128i row3 = _mm_load_si128((__m128i const *)(above + 48));
  for (int i = 0; i < height; ++i) {
    _mm_store_si128((__m128i *)dst, row0);
    _mm_store_si128((__m128i *)(dst + 16), row1);
    _mm_store_si128((__m128i *)(dst + 32), row2);
    _mm_store_si128((__m128i *)(dst + 48), row3);
    dst += stride;
  }
}

void aom_v_predictor_64x64_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_predictor_64xh(dst, stride, above, 64);
}

void aom_v_predictor_64x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_predictor_64xh(dst, stride, above, 32);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_v_predictor_64x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_predictor_64xh(dst, stride, above, 16);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

// -----------------------------------------------------------------------------
// H_PRED

void aom_h_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  (void)above;
  __m128i left_col = _mm_loadl_epi64((__m128i const *)left);
  left_col = _mm_unpacklo_epi8(left_col, left_col);
  __m128i row0 = _mm_shufflelo_epi16(left_col, 0);
  __m128i row1 = _mm_shufflelo_epi16(left_col, 0x55);
  __m128i row2 = _mm_shufflelo_epi16(left_col, 0xaa);
  __m128i row3 = _mm_shufflelo_epi16(left_col, 0xff);
  *(int *)dst = _mm_cvtsi128_si32(row0);
  dst += stride;
  *(int *)dst = _mm_cvtsi128_si32(row1);
  dst += stride;
  *(int *)dst = _mm_cvtsi128_si32(row2);
  dst += stride;
  *(int *)dst = _mm_cvtsi128_si32(row3);
  dst += stride;
  left_col = _mm_unpackhi_epi64(left_col, left_col);
  row0 = _mm_shufflelo_epi16(left_col, 0);
  row1 = _mm_shufflelo_epi16(left_col, 0x55);
  row2 = _mm_shufflelo_epi16(left_col, 0xaa);
  row3 = _mm_shufflelo_epi16(left_col, 0xff);
  *(int *)dst = _mm_cvtsi128_si32(row0);
  dst += stride;
  *(int *)dst = _mm_cvtsi128_si32(row1);
  dst += stride;
  *(int *)dst = _mm_cvtsi128_si32(row2);
  dst += stride;
  *(int *)dst = _mm_cvtsi128_si32(row3);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_h_predictor_4x16_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  (void)above;
  const __m128i left_col = _mm_load_si128((__m128i const *)left);
  __m128i left_col_low = _mm_unpacklo_epi8(left_col, left_col);
  __m128i left_col_high = _mm_unpackhi_epi8(left_col, left_col);

  __m128i row0 = _mm_shufflelo_epi16(left_col_low, 0);
  __m128i row1 = _mm_shufflelo_epi16(left_col_low, 0x55);
  __m128i row2 = _mm_shufflelo_epi16(left_col_low, 0xaa);
  __m128i row3 = _mm_shufflelo_epi16(left_col_low, 0xff);
  *(int *)dst = _mm_cvtsi128_si32(row0);
  dst += stride;
  *(int *)dst = _mm_cvtsi128_si32(row1);
  dst += stride;
  *(int *)dst = _mm_cvtsi128_si32(row2);
  dst += stride;
  *(int *)dst = _mm_cvtsi128_si32(row3);
  dst += stride;

  left_col_low = _mm_unpackhi_epi64(left_col_low, left_col_low);
  row0 = _mm_shufflelo_epi16(left_col_low, 0);
  row1 = _mm_shufflelo_epi16(left_col_low, 0x55);
  row2 = _mm_shufflelo_epi16(left_col_low, 0xaa);
  row3 = _mm_shufflelo_epi16(left_col_low, 0xff);
  *(int *)dst = _mm_cvtsi128_si32(row0);
  dst += stride;
  *(int *)dst = _mm_cvtsi128_si32(row1);
  dst += stride;
  *(int *)dst = _mm_cvtsi128_si32(row2);
  dst += stride;
  *(int *)dst = _mm_cvtsi128_si32(row3);
  dst += stride;

  row0 = _mm_shufflelo_epi16(left_col_high, 0);
  row1 = _mm_shufflelo_epi16(left_col_high, 0x55);
  row2 = _mm_shufflelo_epi16(left_col_high, 0xaa);
  row3 = _mm_shufflelo_epi16(left_col_high, 0xff);
  *(int *)dst = _mm_cvtsi128_si32(row0);
  dst += stride;
  *(int *)dst = _mm_cvtsi128_si32(row1);
  dst += stride;
  *(int *)dst = _mm_cvtsi128_si32(row2);
  dst += stride;
  *(int *)dst = _mm_cvtsi128_si32(row3);
  dst += stride;

  left_col_high = _mm_unpackhi_epi64(left_col_high, left_col_high);
  row0 = _mm_shufflelo_epi16(left_col_high, 0);
  row1 = _mm_shufflelo_epi16(left_col_high, 0x55);
  row2 = _mm_shufflelo_epi16(left_col_high, 0xaa);
  row3 = _mm_shufflelo_epi16(left_col_high, 0xff);
  *(int *)dst = _mm_cvtsi128_si32(row0);
  dst += stride;
  *(int *)dst = _mm_cvtsi128_si32(row1);
  dst += stride;
  *(int *)dst = _mm_cvtsi128_si32(row2);
  dst += stride;
  *(int *)dst = _mm_cvtsi128_si32(row3);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_h_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  (void)above;
  __m128i left_col = _mm_loadl_epi64((__m128i const *)left);
  left_col = _mm_unpacklo_epi8(left_col, left_col);
  __m128i row0 = _mm_shufflelo_epi16(left_col, 0);
  __m128i row1 = _mm_shufflelo_epi16(left_col, 0x55);
  __m128i row2 = _mm_shufflelo_epi16(left_col, 0xaa);
  __m128i row3 = _mm_shufflelo_epi16(left_col, 0xff);
  _mm_storel_epi64((__m128i *)dst, row0);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row1);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row2);
  dst += stride;
  _mm_storel_epi64((__m128i *)dst, row3);
}

static inline void h_predictor_8x16xc(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above, const uint8_t *left,
                                      int count) {
  (void)above;
  for (int i = 0; i < count; ++i) {
    const __m128i left_col = _mm_load_si128((__m128i const *)left);
    __m128i left_col_low = _mm_unpacklo_epi8(left_col, left_col);
    __m128i left_col_high = _mm_unpackhi_epi8(left_col, left_col);

    __m128i row0 = _mm_shufflelo_epi16(left_col_low, 0);
    __m128i row1 = _mm_shufflelo_epi16(left_col_low, 0x55);
    __m128i row2 = _mm_shufflelo_epi16(left_col_low, 0xaa);
    __m128i row3 = _mm_shufflelo_epi16(left_col_low, 0xff);
    _mm_storel_epi64((__m128i *)dst, row0);
    dst += stride;
    _mm_storel_epi64((__m128i *)dst, row1);
    dst += stride;
    _mm_storel_epi64((__m128i *)dst, row2);
    dst += stride;
    _mm_storel_epi64((__m128i *)dst, row3);
    dst += stride;

    left_col_low = _mm_unpackhi_epi64(left_col_low, left_col_low);
    row0 = _mm_shufflelo_epi16(left_col_low, 0);
    row1 = _mm_shufflelo_epi16(left_col_low, 0x55);
    row2 = _mm_shufflelo_epi16(left_col_low, 0xaa);
    row3 = _mm_shufflelo_epi16(left_col_low, 0xff);
    _mm_storel_epi64((__m128i *)dst, row0);
    dst += stride;
    _mm_storel_epi64((__m128i *)dst, row1);
    dst += stride;
    _mm_storel_epi64((__m128i *)dst, row2);
    dst += stride;
    _mm_storel_epi64((__m128i *)dst, row3);
    dst += stride;

    row0 = _mm_shufflelo_epi16(left_col_high, 0);
    row1 = _mm_shufflelo_epi16(left_col_high, 0x55);
    row2 = _mm_shufflelo_epi16(left_col_high, 0xaa);
    row3 = _mm_shufflelo_epi16(left_col_high, 0xff);
    _mm_storel_epi64((__m128i *)dst, row0);
    dst += stride;
    _mm_storel_epi64((__m128i *)dst, row1);
    dst += stride;
    _mm_storel_epi64((__m128i *)dst, row2);
    dst += stride;
    _mm_storel_epi64((__m128i *)dst, row3);
    dst += stride;

    left_col_high = _mm_unpackhi_epi64(left_col_high, left_col_high);
    row0 = _mm_shufflelo_epi16(left_col_high, 0);
    row1 = _mm_shufflelo_epi16(left_col_high, 0x55);
    row2 = _mm_shufflelo_epi16(left_col_high, 0xaa);
    row3 = _mm_shufflelo_epi16(left_col_high, 0xff);
    _mm_storel_epi64((__m128i *)dst, row0);
    dst += stride;
    _mm_storel_epi64((__m128i *)dst, row1);
    dst += stride;
    _mm_storel_epi64((__m128i *)dst, row2);
    dst += stride;
    _mm_storel_epi64((__m128i *)dst, row3);
    dst += stride;
    left += 16;
  }
}

void aom_h_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  h_predictor_8x16xc(dst, stride, above, left, 1);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_h_predictor_8x32_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  h_predictor_8x16xc(dst, stride, above, left, 2);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

static inline void h_pred_store_16xh(const __m128i *row, int h, uint8_t *dst,
                                     ptrdiff_t stride) {
  int i;
  for (i = 0; i < h; ++i) {
    _mm_store_si128((__m128i *)dst, row[i]);
    dst += stride;
  }
}

static inline void repeat_low_4pixels(const __m128i *x, __m128i *row) {
  const __m128i u0 = _mm_shufflelo_epi16(*x, 0);
  const __m128i u1 = _mm_shufflelo_epi16(*x, 0x55);
  const __m128i u2 = _mm_shufflelo_epi16(*x, 0xaa);
  const __m128i u3 = _mm_shufflelo_epi16(*x, 0xff);

  row[0] = _mm_unpacklo_epi64(u0, u0);
  row[1] = _mm_unpacklo_epi64(u1, u1);
  row[2] = _mm_unpacklo_epi64(u2, u2);
  row[3] = _mm_unpacklo_epi64(u3, u3);
}

static inline void repeat_high_4pixels(const __m128i *x, __m128i *row) {
  const __m128i u0 = _mm_shufflehi_epi16(*x, 0);
  const __m128i u1 = _mm_shufflehi_epi16(*x, 0x55);
  const __m128i u2 = _mm_shufflehi_epi16(*x, 0xaa);
  const __m128i u3 = _mm_shufflehi_epi16(*x, 0xff);

  row[0] = _mm_unpackhi_epi64(u0, u0);
  row[1] = _mm_unpackhi_epi64(u1, u1);
  row[2] = _mm_unpackhi_epi64(u2, u2);
  row[3] = _mm_unpackhi_epi64(u3, u3);
}

// Process 16x8, first 4 rows
// Use first 8 bytes of left register: xxxxxxxx33221100
static inline void h_prediction_16x8_1(const __m128i *left, uint8_t *dst,
                                       ptrdiff_t stride) {
  __m128i row[4];
  repeat_low_4pixels(left, row);
  h_pred_store_16xh(row, 4, dst, stride);
}

// Process 16x8, second 4 rows
// Use second 8 bytes of left register: 77665544xxxxxxxx
static inline void h_prediction_16x8_2(const __m128i *left, uint8_t *dst,
                                       ptrdiff_t stride) {
  __m128i row[4];
  repeat_high_4pixels(left, row);
  h_pred_store_16xh(row, 4, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_h_predictor_16x4_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  (void)above;
  const __m128i left_col = _mm_loadl_epi64((const __m128i *)left);
  const __m128i left_col_8p = _mm_unpacklo_epi8(left_col, left_col);
  h_prediction_16x8_1(&left_col_8p, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_h_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  (void)above;
  const __m128i left_col = _mm_loadl_epi64((const __m128i *)left);
  const __m128i left_col_8p = _mm_unpacklo_epi8(left_col, left_col);
  h_prediction_16x8_1(&left_col_8p, dst, stride);
  dst += stride << 2;
  h_prediction_16x8_2(&left_col_8p, dst, stride);
}

static inline void h_predictor_16xh(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *left, int count) {
  int i = 0;
  do {
    const __m128i left_col = _mm_load_si128((const __m128i *)left);
    const __m128i left_col_8p_lo = _mm_unpacklo_epi8(left_col, left_col);
    h_prediction_16x8_1(&left_col_8p_lo, dst, stride);
    dst += stride << 2;
    h_prediction_16x8_2(&left_col_8p_lo, dst, stride);
    dst += stride << 2;

    const __m128i left_col_8p_hi = _mm_unpackhi_epi8(left_col, left_col);
    h_prediction_16x8_1(&left_col_8p_hi, dst, stride);
    dst += stride << 2;
    h_prediction_16x8_2(&left_col_8p_hi, dst, stride);
    dst += stride << 2;

    left += 16;
    i++;
  } while (i < count);
}

void aom_h_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  (void)above;
  h_predictor_16xh(dst, stride, left, 2);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_h_predictor_16x64_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  (void)above;
  h_predictor_16xh(dst, stride, left, 4);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

static inline void h_pred_store_32xh(const __m128i *row, int h, uint8_t *dst,
                                     ptrdiff_t stride) {
  int i;
  for (i = 0; i < h; ++i) {
    _mm_store_si128((__m128i *)dst, row[i]);
    _mm_store_si128((__m128i *)(dst + 16), row[i]);
    dst += stride;
  }
}

// Process 32x8, first 4 rows
// Use first 8 bytes of left register: xxxxxxxx33221100
static inline void h_prediction_32x8_1(const __m128i *left, uint8_t *dst,
                                       ptrdiff_t stride) {
  __m128i row[4];
  repeat_low_4pixels(left, row);
  h_pred_store_32xh(row, 4, dst, stride);
}

// Process 32x8, second 4 rows
// Use second 8 bytes of left register: 77665544xxxxxxxx
static inline void h_prediction_32x8_2(const __m128i *left, uint8_t *dst,
                                       ptrdiff_t stride) {
  __m128i row[4];
  repeat_high_4pixels(left, row);
  h_pred_store_32xh(row, 4, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_h_predictor_32x8_sse2(uint8_t *dst, ptrdiff_t stride,
                               const uint8_t *above, const uint8_t *left) {
  __m128i left_col, left_col_8p;
  (void)above;

  left_col = _mm_load_si128((const __m128i *)left);

  left_col_8p = _mm_unpacklo_epi8(left_col, left_col);
  h_prediction_32x8_1(&left_col_8p, dst, stride);
  dst += stride << 2;
  h_prediction_32x8_2(&left_col_8p, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_h_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  __m128i left_col, left_col_8p;
  (void)above;

  left_col = _mm_load_si128((const __m128i *)left);

  left_col_8p = _mm_unpacklo_epi8(left_col, left_col);
  h_prediction_32x8_1(&left_col_8p, dst, stride);
  dst += stride << 2;
  h_prediction_32x8_2(&left_col_8p, dst, stride);
  dst += stride << 2;

  left_col_8p = _mm_unpackhi_epi8(left_col, left_col);
  h_prediction_32x8_1(&left_col_8p, dst, stride);
  dst += stride << 2;
  h_prediction_32x8_2(&left_col_8p, dst, stride);
}

static inline void h_predictor_32xh(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *left, int height) {
  int i = height >> 2;
  do {
    __m128i left4 = _mm_cvtsi32_si128(((int *)left)[0]);
    left4 = _mm_unpacklo_epi8(left4, left4);
    left4 = _mm_unpacklo_epi8(left4, left4);
    const __m128i r0 = _mm_shuffle_epi32(left4, 0x0);
    const __m128i r1 = _mm_shuffle_epi32(left4, 0x55);
    _mm_store_si128((__m128i *)dst, r0);
    _mm_store_si128((__m128i *)(dst + 16), r0);
    _mm_store_si128((__m128i *)(dst + stride), r1);
    _mm_store_si128((__m128i *)(dst + stride + 16), r1);
    const __m128i r2 = _mm_shuffle_epi32(left4, 0xaa);
    const __m128i r3 = _mm_shuffle_epi32(left4, 0xff);
    _mm_store_si128((__m128i *)(dst + stride * 2), r2);
    _mm_store_si128((__m128i *)(dst + stride * 2 + 16), r2);
    _mm_store_si128((__m128i *)(dst + stride * 3), r3);
    _mm_store_si128((__m128i *)(dst + stride * 3 + 16), r3);
    left += 4;
    dst += stride * 4;
  } while (--i);
}

void aom_h_predictor_32x64_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  (void)above;
  h_predictor_32xh(dst, stride, left, 64);
}

static inline void h_predictor_64xh(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *left, int height) {
  int i = height >> 2;
  do {
    __m128i left4 = _mm_cvtsi32_si128(((int *)left)[0]);
    left4 = _mm_unpacklo_epi8(left4, left4);
    left4 = _mm_unpacklo_epi8(left4, left4);
    const __m128i r0 = _mm_shuffle_epi32(left4, 0x0);
    const __m128i r1 = _mm_shuffle_epi32(left4, 0x55);
    _mm_store_si128((__m128i *)dst, r0);
    _mm_store_si128((__m128i *)(dst + 16), r0);
    _mm_store_si128((__m128i *)(dst + 32), r0);
    _mm_store_si128((__m128i *)(dst + 48), r0);
    _mm_store_si128((__m128i *)(dst + stride), r1);
    _mm_store_si128((__m128i *)(dst + stride + 16), r1);
    _mm_store_si128((__m128i *)(dst + stride + 32), r1);
    _mm_store_si128((__m128i *)(dst + stride + 48), r1);
    const __m128i r2 = _mm_shuffle_epi32(left4, 0xaa);
    const __m128i r3 = _mm_shuffle_epi32(left4, 0xff);
    _mm_store_si128((__m128i *)(dst + stride * 2), r2);
    _mm_store_si128((__m128i *)(dst + stride * 2 + 16), r2);
    _mm_store_si128((__m128i *)(dst + stride * 2 + 32), r2);
    _mm_store_si128((__m128i *)(dst + stride * 2 + 48), r2);
    _mm_store_si128((__m128i *)(dst + stride * 3), r3);
    _mm_store_si128((__m128i *)(dst + stride * 3 + 16), r3);
    _mm_store_si128((__m128i *)(dst + stride * 3 + 32), r3);
    _mm_store_si128((__m128i *)(dst + stride * 3 + 48), r3);
    left += 4;
    dst += stride * 4;
  } while (--i);
}

void aom_h_predictor_64x64_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  (void)above;
  h_predictor_64xh(dst, stride, left, 64);
}

void aom_h_predictor_64x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  (void)above;
  h_predictor_64xh(dst, stride, left, 32);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_h_predictor_64x16_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  (void)above;
  h_predictor_64xh(dst, stride, left, 16);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
