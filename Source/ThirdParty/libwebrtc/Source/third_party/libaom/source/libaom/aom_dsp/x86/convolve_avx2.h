/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AOM_DSP_X86_CONVOLVE_AVX2_H_
#define AOM_AOM_DSP_X86_CONVOLVE_AVX2_H_

#include <immintrin.h>

#include "aom_ports/mem.h"

#include "aom_dsp/x86/mem_sse2.h"
#include "aom_dsp/x86/synonyms.h"

#include "av1/common/convolve.h"
#include "av1/common/filter.h"

#define SECOND_32_BLK (32)
#define THIRD_32_BLK (32 << 1)
#define FOURTH_32_BLK (SECOND_32_BLK + THIRD_32_BLK)

// filters for 16
DECLARE_ALIGNED(32, static const uint8_t, filt_global_avx2[]) = {
  0,  1,  1,  2,  2, 3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  0,  1,  1,
  2,  2,  3,  3,  4, 4,  5,  5,  6,  6,  7,  7,  8,  2,  3,  3,  4,  4,  5,
  5,  6,  6,  7,  7, 8,  8,  9,  9,  10, 2,  3,  3,  4,  4,  5,  5,  6,  6,
  7,  7,  8,  8,  9, 9,  10, 4,  5,  5,  6,  6,  7,  7,  8,  8,  9,  9,  10,
  10, 11, 11, 12, 4, 5,  5,  6,  6,  7,  7,  8,  8,  9,  9,  10, 10, 11, 11,
  12, 6,  7,  7,  8, 8,  9,  9,  10, 10, 11, 11, 12, 12, 13, 13, 14, 6,  7,
  7,  8,  8,  9,  9, 10, 10, 11, 11, 12, 12, 13, 13, 14
};

DECLARE_ALIGNED(32, static const uint8_t, filt_d4_global_avx2[]) = {
  0, 1, 2, 3,  1, 2, 3, 4, 2, 3, 4, 5, 3, 4, 5, 6, 0, 1, 2, 3,  1, 2,
  3, 4, 2, 3,  4, 5, 3, 4, 5, 6, 4, 5, 6, 7, 5, 6, 7, 8, 6, 7,  8, 9,
  7, 8, 9, 10, 4, 5, 6, 7, 5, 6, 7, 8, 6, 7, 8, 9, 7, 8, 9, 10,
};

DECLARE_ALIGNED(32, static const uint8_t, filt4_d4_global_avx2[]) = {
  2, 3, 4, 5, 3, 4, 5, 6, 4, 5, 6, 7, 5, 6, 7, 8,
  2, 3, 4, 5, 3, 4, 5, 6, 4, 5, 6, 7, 5, 6, 7, 8,
};

DECLARE_ALIGNED(32, static const uint8_t, filt_center_global_avx2[32]) = {
  3, 255, 4, 255, 5, 255, 6, 255, 7, 255, 8, 255, 9, 255, 10, 255,
  3, 255, 4, 255, 5, 255, 6, 255, 7, 255, 8, 255, 9, 255, 10, 255
};

DECLARE_ALIGNED(32, static const uint8_t,
                filt1_global_sse2[16]) = { 0, 1, 1, 2,  2,  3,  3,  4,
                                           8, 9, 9, 10, 10, 11, 11, 12 };

DECLARE_ALIGNED(32, static const uint8_t,
                filt2_global_sse2[16]) = { 2,  3,  3,  4,  4,  5,  5,  6,
                                           10, 11, 11, 12, 12, 13, 13, 14 };

DECLARE_ALIGNED(32, static const uint8_t,
                filt3_global_sse2[16]) = { 0, 1, 1, 2, 8, 9, 9, 10,
                                           0, 0, 0, 0, 0, 0, 0, 0 };

DECLARE_ALIGNED(32, static const uint8_t,
                filt4_global_sse2[16]) = { 2, 3, 3, 4, 10, 11, 11, 12,
                                           0, 0, 0, 0, 0,  0,  0,  0 };

DECLARE_ALIGNED(32, static const uint8_t,
                filt5_global_sse2[16]) = { 0, 1, 1, 2, 4, 5, 5, 6,
                                           0, 0, 0, 0, 0, 0, 0, 0 };

DECLARE_ALIGNED(32, static const uint8_t,
                filt1_global_avx2[32]) = { 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5,
                                           6, 6, 7, 7, 8, 0, 1, 1, 2, 2, 3,
                                           3, 4, 4, 5, 5, 6, 6, 7, 7, 8 };

DECLARE_ALIGNED(32, static const uint8_t,
                filt2_global_avx2[32]) = { 2, 3, 3, 4, 4,  5, 5, 6, 6, 7, 7,
                                           8, 8, 9, 9, 10, 2, 3, 3, 4, 4, 5,
                                           5, 6, 6, 7, 7,  8, 8, 9, 9, 10 };

DECLARE_ALIGNED(32, static const uint8_t, filt3_global_avx2[32]) = {
  4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12,
  4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12
};

DECLARE_ALIGNED(32, static const uint8_t, filt4_global_avx2[32]) = {
  6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14,
  6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14
};

#define CONVOLVE_SR_HOR_FILTER_W4(CONVOLVE_LOWBD)                            \
  for (i = 0; i < (im_h - 2); i += 2) {                                      \
    __m128i data =                                                           \
        load_8bit_8x2_to_1_reg_sse2(&src_ptr[(i * src_stride)], src_stride); \
    __m128i res = CONVOLVE_LOWBD(data, coeffs_h, filt);                      \
    res = _mm_srai_epi16(_mm_add_epi16(res, round_const_h), 2);              \
    _mm_store_si128((__m128i *)&im_block[i * 4], res);                       \
  }                                                                          \
  __m128i data_1 = _mm_loadl_epi64((__m128i *)&src_ptr[(i * src_stride)]);   \
  __m128i res = CONVOLVE_LOWBD(data_1, coeffs_h, filt);                      \
  res = _mm_srai_epi16(_mm_add_epi16(res, round_const_h), 2);                \
  _mm_storel_epi64((__m128i *)&im_block[i * 4], res);

#define CONVOLVE_SR_HOR_FILTER_2TAP_W4 \
  CONVOLVE_SR_HOR_FILTER_W4(convolve_lowbd_x_2tap_ssse3)

#define CONVOLVE_SR_HOR_FILTER_4TAP_W4 \
  CONVOLVE_SR_HOR_FILTER_W4(convolve_lowbd_x_4tap_ssse3)

static inline void sr_2d_ver_round_and_store_w4(int w, __m256i res,
                                                uint8_t *dst, int dst_stride,
                                                __m256i round_const_v) {
  const __m256i res_round =
      _mm256_srai_epi32(_mm256_add_epi32(res, round_const_v), 11);

  const __m256i res_16bit = _mm256_packs_epi32(res_round, res_round);
  const __m256i res_8b = _mm256_packus_epi16(res_16bit, res_16bit);

  const __m128i r0 = _mm256_castsi256_si128(res_8b);
  const __m128i r1 = _mm256_extracti128_si256(res_8b, 1);

  __m128i *const p0 = (__m128i *)dst;
  __m128i *const p1 = (__m128i *)(dst + dst_stride);

  if (w == 4) {
    xx_storel_32(p0, r0);
    xx_storel_32(p1, r1);
  } else {
    assert(w == 2);
    *(uint16_t *)p0 = (uint16_t)_mm_cvtsi128_si32(r0);
    *(uint16_t *)p1 = (uint16_t)_mm_cvtsi128_si32(r1);
  }
}

#define CONVOLVE_SR_VER_FILTER_2TAP_W4                                        \
  __m128i s[2];                                                               \
  s[0] = _mm_loadl_epi64((__m128i *)(im_block + 0 * 4));                      \
                                                                              \
  for (i = 0; i < h; i += 2) {                                                \
    const int16_t *data = &im_block[i * 4];                                   \
    s[1] = _mm_loadl_epi64((__m128i *)(data + 1 * 4));                        \
    const __m256i src_0 = _mm256_setr_m128i(s[0], s[1]);                      \
    s[0] = _mm_loadl_epi64((__m128i *)(data + 2 * 4));                        \
    const __m256i src_1 = _mm256_setr_m128i(s[1], s[0]);                      \
    const __m256i ss = _mm256_unpacklo_epi16(src_0, src_1);                   \
                                                                              \
    const __m256i res = _mm256_madd_epi16(ss, coeffs_v[0]);                   \
                                                                              \
    sr_2d_ver_round_and_store_w4(w, res, dst_ptr, dst_stride, round_const_v); \
    dst_ptr += 2 * dst_stride;                                                \
  }

#define CONVOLVE_SR_VER_FILTER_4TAP_W4                                        \
  __m128i s[4];                                                               \
  __m256i ss[2];                                                              \
  s[0] = _mm_loadl_epi64((__m128i *)(im_block + 0 * 4));                      \
  s[1] = _mm_loadl_epi64((__m128i *)(im_block + 1 * 4));                      \
  s[2] = _mm_loadl_epi64((__m128i *)(im_block + 2 * 4));                      \
                                                                              \
  const __m256i src_0 = _mm256_setr_m128i(s[0], s[1]);                        \
  const __m256i src_1 = _mm256_setr_m128i(s[1], s[2]);                        \
                                                                              \
  ss[0] = _mm256_unpacklo_epi16(src_0, src_1);                                \
                                                                              \
  for (i = 0; i < h; i += 2) {                                                \
    const int16_t *data = &im_block[i * 4];                                   \
    s[3] = _mm_loadl_epi64((__m128i *)(data + 3 * 4));                        \
    const __m256i src_2 = _mm256_setr_m128i(s[2], s[3]);                      \
    s[2] = _mm_loadl_epi64((__m128i *)(data + 4 * 4));                        \
    const __m256i src_3 = _mm256_setr_m128i(s[3], s[2]);                      \
    ss[1] = _mm256_unpacklo_epi16(src_2, src_3);                              \
                                                                              \
    const __m256i res = convolve_4tap(ss, coeffs_v);                          \
                                                                              \
    sr_2d_ver_round_and_store_w4(w, res, dst_ptr, dst_stride, round_const_v); \
    dst_ptr += 2 * dst_stride;                                                \
                                                                              \
    ss[0] = ss[1];                                                            \
  }

#define CONVOLVE_SR_VER_FILTER_6TAP_W4                                        \
  __m128i s[6];                                                               \
  __m256i ss[3];                                                              \
  s[0] = _mm_loadl_epi64((__m128i *)(im_block + 0 * 4));                      \
  s[1] = _mm_loadl_epi64((__m128i *)(im_block + 1 * 4));                      \
  s[2] = _mm_loadl_epi64((__m128i *)(im_block + 2 * 4));                      \
  s[3] = _mm_loadl_epi64((__m128i *)(im_block + 3 * 4));                      \
  s[4] = _mm_loadl_epi64((__m128i *)(im_block + 4 * 4));                      \
                                                                              \
  const __m256i src_0 = _mm256_setr_m128i(s[0], s[1]);                        \
  const __m256i src_1 = _mm256_setr_m128i(s[1], s[2]);                        \
  const __m256i src_2 = _mm256_setr_m128i(s[2], s[3]);                        \
  const __m256i src_3 = _mm256_setr_m128i(s[3], s[4]);                        \
                                                                              \
  ss[0] = _mm256_unpacklo_epi16(src_0, src_1);                                \
  ss[1] = _mm256_unpacklo_epi16(src_2, src_3);                                \
                                                                              \
  for (i = 0; i < h; i += 2) {                                                \
    const int16_t *data = &im_block[i * 4];                                   \
    s[5] = _mm_loadl_epi64((__m128i *)(data + 5 * 4));                        \
    const __m256i src_4 = _mm256_setr_m128i(s[4], s[5]);                      \
    s[4] = _mm_loadl_epi64((__m128i *)(data + 6 * 4));                        \
    const __m256i src_5 = _mm256_setr_m128i(s[5], s[4]);                      \
    ss[2] = _mm256_unpacklo_epi16(src_4, src_5);                              \
                                                                              \
    const __m256i res = convolve_6tap(ss, coeffs_v);                          \
                                                                              \
    sr_2d_ver_round_and_store_w4(w, res, dst_ptr, dst_stride, round_const_v); \
    dst_ptr += 2 * dst_stride;                                                \
                                                                              \
    ss[0] = ss[1];                                                            \
    ss[1] = ss[2];                                                            \
  }

#define CONVOLVE_SR_VER_FILTER_8TAP_W4                                        \
  __m128i s[8];                                                               \
  __m256i ss[4];                                                              \
  s[0] = _mm_loadl_epi64((__m128i *)(im_block + 0 * 4));                      \
  s[1] = _mm_loadl_epi64((__m128i *)(im_block + 1 * 4));                      \
  s[2] = _mm_loadl_epi64((__m128i *)(im_block + 2 * 4));                      \
  s[3] = _mm_loadl_epi64((__m128i *)(im_block + 3 * 4));                      \
  s[4] = _mm_loadl_epi64((__m128i *)(im_block + 4 * 4));                      \
  s[5] = _mm_loadl_epi64((__m128i *)(im_block + 5 * 4));                      \
  s[6] = _mm_loadl_epi64((__m128i *)(im_block + 6 * 4));                      \
                                                                              \
  const __m256i src_0 = _mm256_setr_m128i(s[0], s[1]);                        \
  const __m256i src_1 = _mm256_setr_m128i(s[1], s[2]);                        \
  const __m256i src_2 = _mm256_setr_m128i(s[2], s[3]);                        \
  const __m256i src_3 = _mm256_setr_m128i(s[3], s[4]);                        \
  const __m256i src_4 = _mm256_setr_m128i(s[4], s[5]);                        \
  const __m256i src_5 = _mm256_setr_m128i(s[5], s[6]);                        \
                                                                              \
  ss[0] = _mm256_unpacklo_epi16(src_0, src_1);                                \
  ss[1] = _mm256_unpacklo_epi16(src_2, src_3);                                \
  ss[2] = _mm256_unpacklo_epi16(src_4, src_5);                                \
                                                                              \
  for (i = 0; i < h; i += 2) {                                                \
    const int16_t *data = &im_block[i * 4];                                   \
    s[7] = _mm_loadl_epi64((__m128i *)(data + 7 * 4));                        \
    const __m256i src_6 = _mm256_setr_m128i(s[6], s[7]);                      \
    s[6] = _mm_loadl_epi64((__m128i *)(data + 8 * 4));                        \
    const __m256i src_7 = _mm256_setr_m128i(s[7], s[6]);                      \
    ss[3] = _mm256_unpacklo_epi16(src_6, src_7);                              \
                                                                              \
    const __m256i res = convolve(ss, coeffs_v);                               \
                                                                              \
    sr_2d_ver_round_and_store_w4(w, res, dst_ptr, dst_stride, round_const_v); \
    dst_ptr += 2 * dst_stride;                                                \
                                                                              \
    ss[0] = ss[1];                                                            \
    ss[1] = ss[2];                                                            \
    ss[2] = ss[3];                                                            \
  }

#define CONVOLVE_SR_HORIZONTAL_FILTER(CONVOLVE_LOWBD)                 \
  for (i = 0; i < (im_h - 2); i += 2) {                               \
    __m256i data = _mm256_castsi128_si256(                            \
        _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + j]));  \
    data = _mm256_inserti128_si256(                                   \
        data,                                                         \
        _mm_loadu_si128(                                              \
            (__m128i *)&src_ptr[(i * src_stride) + j + src_stride]),  \
        1);                                                           \
    __m256i res = CONVOLVE_LOWBD(data, coeffs_h, filt);               \
    res = _mm256_srai_epi16(_mm256_add_epi16(res, round_const_h), 2); \
    _mm256_store_si256((__m256i *)&im_block[i * im_stride], res);     \
  }                                                                   \
  __m256i data_1 = _mm256_castsi128_si256(                            \
      _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + j]));    \
  __m256i res = CONVOLVE_LOWBD(data_1, coeffs_h, filt);               \
  res = _mm256_srai_epi16(_mm256_add_epi16(res, round_const_h), 2);   \
  _mm256_store_si256((__m256i *)&im_block[i * im_stride], res);

#define CONVOLVE_SR_HORIZONTAL_FILTER_2TAP \
  CONVOLVE_SR_HORIZONTAL_FILTER(convolve_lowbd_x_2tap)

#define CONVOLVE_SR_HORIZONTAL_FILTER_4TAP \
  CONVOLVE_SR_HORIZONTAL_FILTER(convolve_lowbd_x_4tap)

#define CONVOLVE_SR_HORIZONTAL_FILTER_6TAP \
  CONVOLVE_SR_HORIZONTAL_FILTER(convolve_lowbd_x_6tap)

#define CONVOLVE_SR_HORIZONTAL_FILTER_8TAP \
  CONVOLVE_SR_HORIZONTAL_FILTER(convolve_lowbd_x)

static inline void sr_2d_ver_round_and_store(__m256i res_a, __m256i res_b,
                                             uint8_t *dst, int dst_stride,
                                             __m256i round_const_v) {
  const __m256i res_a_round =
      _mm256_srai_epi32(_mm256_add_epi32(res_a, round_const_v), 11);
  const __m256i res_b_round =
      _mm256_srai_epi32(_mm256_add_epi32(res_b, round_const_v), 11);
  const __m256i r16 = _mm256_packs_epi32(res_a_round, res_b_round);
  const __m256i r8 = _mm256_packus_epi16(r16, r16);

  _mm_storel_epi64((__m128i *)dst, _mm256_castsi256_si128(r8));
  _mm_storel_epi64((__m128i *)(dst + dst_stride),
                   _mm256_extracti128_si256(r8, 1));
}

#define CONVOLVE_SR_VERTICAL_FILTER_2TAP                                      \
  for (i = 0; i < h; i += 2) {                                                \
    __m256i s[2];                                                             \
    const int16_t *data = &im_block[i * im_stride];                           \
    const __m256i s1 = _mm256_loadu_si256((__m256i *)(data + 0 * im_stride)); \
    const __m256i s2 = _mm256_loadu_si256((__m256i *)(data + 1 * im_stride)); \
    s[0] = _mm256_unpacklo_epi16(s1, s2);                                     \
    s[1] = _mm256_unpackhi_epi16(s1, s2);                                     \
                                                                              \
    __m256i res_a = _mm256_madd_epi16(s[0], coeffs_v[0]);                     \
    __m256i res_b = _mm256_madd_epi16(s[1], coeffs_v[0]);                     \
                                                                              \
    sr_2d_ver_round_and_store(res_a, res_b, dst_ptr, dst_stride,              \
                              round_const_v);                                 \
    dst_ptr += 2 * dst_stride;                                                \
  }

#define CONVOLVE_SR_VERTICAL_FILTER_4TAP                                      \
  __m256i s[6];                                                               \
  __m256i src_0 = _mm256_loadu_si256((__m256i *)(im_block + 0 * im_stride));  \
  __m256i src_1 = _mm256_loadu_si256((__m256i *)(im_block + 1 * im_stride));  \
                                                                              \
  s[0] = _mm256_unpacklo_epi16(src_0, src_1);                                 \
  s[2] = _mm256_unpackhi_epi16(src_0, src_1);                                 \
                                                                              \
  for (i = 0; i < h; i += 2) {                                                \
    const int16_t *data = &im_block[i * im_stride];                           \
    const __m256i s4 = _mm256_loadu_si256((__m256i *)(data + 2 * im_stride)); \
    const __m256i s5 = _mm256_loadu_si256((__m256i *)(data + 3 * im_stride)); \
    s[1] = _mm256_unpacklo_epi16(s4, s5);                                     \
    s[3] = _mm256_unpackhi_epi16(s4, s5);                                     \
                                                                              \
    __m256i res_a = convolve_4tap(s, coeffs_v);                               \
    __m256i res_b = convolve_4tap(s + 2, coeffs_v);                           \
                                                                              \
    sr_2d_ver_round_and_store(res_a, res_b, dst_ptr, dst_stride,              \
                              round_const_v);                                 \
    dst_ptr += 2 * dst_stride;                                                \
                                                                              \
    s[0] = s[1];                                                              \
    s[2] = s[3];                                                              \
  }

#define CONVOLVE_SR_VERTICAL_FILTER_6TAP                                      \
  __m256i src_0 = _mm256_loadu_si256((__m256i *)(im_block + 0 * im_stride));  \
  __m256i src_1 = _mm256_loadu_si256((__m256i *)(im_block + 1 * im_stride));  \
  __m256i src_2 = _mm256_loadu_si256((__m256i *)(im_block + 2 * im_stride));  \
  __m256i src_3 = _mm256_loadu_si256((__m256i *)(im_block + 3 * im_stride));  \
                                                                              \
  __m256i s[8];                                                               \
  s[0] = _mm256_unpacklo_epi16(src_0, src_1);                                 \
  s[1] = _mm256_unpacklo_epi16(src_2, src_3);                                 \
                                                                              \
  s[3] = _mm256_unpackhi_epi16(src_0, src_1);                                 \
  s[4] = _mm256_unpackhi_epi16(src_2, src_3);                                 \
                                                                              \
  for (i = 0; i < h; i += 2) {                                                \
    const int16_t *data = &im_block[i * im_stride];                           \
                                                                              \
    const __m256i s6 = _mm256_loadu_si256((__m256i *)(data + 4 * im_stride)); \
    const __m256i s7 = _mm256_loadu_si256((__m256i *)(data + 5 * im_stride)); \
                                                                              \
    s[2] = _mm256_unpacklo_epi16(s6, s7);                                     \
    s[5] = _mm256_unpackhi_epi16(s6, s7);                                     \
                                                                              \
    __m256i res_a = convolve_6tap(s, coeffs_v);                               \
    __m256i res_b = convolve_6tap(s + 3, coeffs_v);                           \
                                                                              \
    sr_2d_ver_round_and_store(res_a, res_b, dst_ptr, dst_stride,              \
                              round_const_v);                                 \
    dst_ptr += 2 * dst_stride;                                                \
                                                                              \
    s[0] = s[1];                                                              \
    s[1] = s[2];                                                              \
                                                                              \
    s[3] = s[4];                                                              \
    s[4] = s[5];                                                              \
  }

#define CONVOLVE_SR_VERTICAL_FILTER_8TAP                                      \
  __m256i src_0 = _mm256_loadu_si256((__m256i *)(im_block + 0 * im_stride));  \
  __m256i src_1 = _mm256_loadu_si256((__m256i *)(im_block + 1 * im_stride));  \
  __m256i src_2 = _mm256_loadu_si256((__m256i *)(im_block + 2 * im_stride));  \
  __m256i src_3 = _mm256_loadu_si256((__m256i *)(im_block + 3 * im_stride));  \
  __m256i src_4 = _mm256_loadu_si256((__m256i *)(im_block + 4 * im_stride));  \
  __m256i src_5 = _mm256_loadu_si256((__m256i *)(im_block + 5 * im_stride));  \
                                                                              \
  __m256i s[8];                                                               \
  s[0] = _mm256_unpacklo_epi16(src_0, src_1);                                 \
  s[1] = _mm256_unpacklo_epi16(src_2, src_3);                                 \
  s[2] = _mm256_unpacklo_epi16(src_4, src_5);                                 \
                                                                              \
  s[4] = _mm256_unpackhi_epi16(src_0, src_1);                                 \
  s[5] = _mm256_unpackhi_epi16(src_2, src_3);                                 \
  s[6] = _mm256_unpackhi_epi16(src_4, src_5);                                 \
                                                                              \
  for (i = 0; i < h; i += 2) {                                                \
    const int16_t *data = &im_block[i * im_stride];                           \
                                                                              \
    const __m256i s6 = _mm256_loadu_si256((__m256i *)(data + 6 * im_stride)); \
    const __m256i s7 = _mm256_loadu_si256((__m256i *)(data + 7 * im_stride)); \
                                                                              \
    s[3] = _mm256_unpacklo_epi16(s6, s7);                                     \
    s[7] = _mm256_unpackhi_epi16(s6, s7);                                     \
                                                                              \
    __m256i res_a = convolve(s, coeffs_v);                                    \
    __m256i res_b = convolve(s + 4, coeffs_v);                                \
                                                                              \
    sr_2d_ver_round_and_store(res_a, res_b, dst_ptr, dst_stride,              \
                              round_const_v);                                 \
    dst_ptr += 2 * dst_stride;                                                \
                                                                              \
    s[0] = s[1];                                                              \
    s[1] = s[2];                                                              \
    s[2] = s[3];                                                              \
                                                                              \
    s[4] = s[5];                                                              \
    s[5] = s[6];                                                              \
    s[6] = s[7];                                                              \
  }

#define CONVOLVE_SR_VERTICAL_FILTER_12TAP                                      \
  __m256i src_0 = _mm256_loadu_si256((__m256i *)(im_block + 0 * im_stride));   \
  __m256i src_1 = _mm256_loadu_si256((__m256i *)(im_block + 1 * im_stride));   \
  __m256i src_2 = _mm256_loadu_si256((__m256i *)(im_block + 2 * im_stride));   \
  __m256i src_3 = _mm256_loadu_si256((__m256i *)(im_block + 3 * im_stride));   \
  __m256i src_4 = _mm256_loadu_si256((__m256i *)(im_block + 4 * im_stride));   \
  __m256i src_5 = _mm256_loadu_si256((__m256i *)(im_block + 5 * im_stride));   \
  __m256i src_6 = _mm256_loadu_si256((__m256i *)(im_block + 6 * im_stride));   \
  __m256i src_7 = _mm256_loadu_si256((__m256i *)(im_block + 7 * im_stride));   \
  __m256i src_8 = _mm256_loadu_si256((__m256i *)(im_block + 8 * im_stride));   \
  __m256i src_9 = _mm256_loadu_si256((__m256i *)(im_block + 9 * im_stride));   \
                                                                               \
  s[0] = _mm256_unpacklo_epi16(src_0, src_1);                                  \
  s[1] = _mm256_unpacklo_epi16(src_2, src_3);                                  \
  s[2] = _mm256_unpacklo_epi16(src_4, src_5);                                  \
  s[3] = _mm256_unpacklo_epi16(src_6, src_7);                                  \
  s[4] = _mm256_unpacklo_epi16(src_8, src_9);                                  \
                                                                               \
  s[6] = _mm256_unpackhi_epi16(src_0, src_1);                                  \
  s[7] = _mm256_unpackhi_epi16(src_2, src_3);                                  \
  s[8] = _mm256_unpackhi_epi16(src_4, src_5);                                  \
  s[9] = _mm256_unpackhi_epi16(src_6, src_7);                                  \
  s[10] = _mm256_unpackhi_epi16(src_8, src_9);                                 \
                                                                               \
  for (i = 0; i < h; i += 2) {                                                 \
    const int16_t *data = &im_block[i * im_stride];                            \
                                                                               \
    const __m256i s6 = _mm256_loadu_si256((__m256i *)(data + 10 * im_stride)); \
    const __m256i s7 = _mm256_loadu_si256((__m256i *)(data + 11 * im_stride)); \
                                                                               \
    s[5] = _mm256_unpacklo_epi16(s6, s7);                                      \
    s[11] = _mm256_unpackhi_epi16(s6, s7);                                     \
                                                                               \
    __m256i res_a = convolve_12taps(s, coeffs_v);                              \
    __m256i res_b = convolve_12taps(s + 6, coeffs_v);                          \
                                                                               \
    res_a =                                                                    \
        _mm256_sra_epi32(_mm256_add_epi32(res_a, sum_round_v), sum_shift_v);   \
    res_b =                                                                    \
        _mm256_sra_epi32(_mm256_add_epi32(res_b, sum_round_v), sum_shift_v);   \
                                                                               \
    const __m256i res_a_round = _mm256_sra_epi32(                              \
        _mm256_add_epi32(res_a, round_const_v), round_shift_v);                \
    const __m256i res_b_round = _mm256_sra_epi32(                              \
        _mm256_add_epi32(res_b, round_const_v), round_shift_v);                \
                                                                               \
    const __m256i res_16bit = _mm256_packs_epi32(res_a_round, res_b_round);    \
    const __m256i res_8b = _mm256_packus_epi16(res_16bit, res_16bit);          \
                                                                               \
    const __m128i res_0 = _mm256_castsi256_si128(res_8b);                      \
    const __m128i res_1 = _mm256_extracti128_si256(res_8b, 1);                 \
                                                                               \
    __m128i *const p_0 = (__m128i *)&dst[i * dst_stride + j];                  \
    __m128i *const p_1 = (__m128i *)&dst[i * dst_stride + j + dst_stride];     \
    if (w - j > 4) {                                                           \
      _mm_storel_epi64(p_0, res_0);                                            \
      _mm_storel_epi64(p_1, res_1);                                            \
    } else if (w == 4) {                                                       \
      xx_storel_32(p_0, res_0);                                                \
      xx_storel_32(p_1, res_1);                                                \
    } else {                                                                   \
      *(uint16_t *)p_0 = (uint16_t)_mm_cvtsi128_si32(res_0);                   \
      *(uint16_t *)p_1 = (uint16_t)_mm_cvtsi128_si32(res_1);                   \
    }                                                                          \
                                                                               \
    s[0] = s[1];                                                               \
    s[1] = s[2];                                                               \
    s[2] = s[3];                                                               \
    s[3] = s[4];                                                               \
    s[4] = s[5];                                                               \
                                                                               \
    s[6] = s[7];                                                               \
    s[7] = s[8];                                                               \
    s[8] = s[9];                                                               \
    s[9] = s[10];                                                              \
    s[10] = s[11];                                                             \
  }

#define JNT_CONVOLVE_PROCESS_OUTPUT(res_unsigned, j_off)                       \
  do {                                                                         \
    if (do_average) {                                                          \
      const __m256i data_ref_0 =                                               \
          load_line2_avx2(&dst[i * dst_stride + (j_off)],                      \
                          &dst[i * dst_stride + (j_off) + dst_stride]);        \
      const __m256i comp_avg_res =                                             \
          comp_avg(&data_ref_0, &(res_unsigned), &wt, use_dist_wtd_comp_avg);  \
      const __m256i res_signed = _mm256_sub_epi16(comp_avg_res, offset_const); \
      const __m256i round_result =                                             \
          _mm256_srai_epi16(_mm256_add_epi16(res_signed, rounding_const), 4);  \
      const __m256i res_8 = _mm256_packus_epi16(round_result, round_result);   \
      const __m128i res_0 = _mm256_castsi256_si128(res_8);                     \
      const __m128i res_1 = _mm256_extracti128_si256(res_8, 1);                \
      if (w - (j_off) > 4) {                                                   \
        _mm_storel_epi64((__m128i *)(&dst0[i * dst_stride0 + (j_off)]),        \
                         res_0);                                               \
        _mm_storel_epi64(                                                      \
            (__m128i *)(&dst0[i * dst_stride0 + (j_off) + dst_stride0]),       \
            res_1);                                                            \
      } else {                                                                 \
        *(int *)(&dst0[i * dst_stride0 + (j_off)]) = _mm_cvtsi128_si32(res_0); \
        *(int *)(&dst0[i * dst_stride0 + (j_off) + dst_stride0]) =             \
            _mm_cvtsi128_si32(res_1);                                          \
      }                                                                        \
    } else {                                                                   \
      const __m128i res_0 = _mm256_castsi256_si128(res_unsigned);              \
      _mm_store_si128((__m128i *)(&dst[i * dst_stride + (j_off)]), res_0);     \
      const __m128i res_1 = _mm256_extracti128_si256(res_unsigned, 1);         \
      _mm_store_si128(                                                         \
          (__m128i *)(&dst[i * dst_stride + (j_off) + dst_stride]), res_1);    \
    }                                                                          \
  } while (0)

#define JNT_CONVOLVE_HORIZONTAL_FILTER(src_h_start, convolve_fn, coeffs) \
  do {                                                                   \
    const uint8_t *src_h = (src_h_start);                                \
    for (i = 0; i < im_h; i += 2) {                                      \
      const __m256i data = load_line2_avx2(src_h, src_h + src_stride);   \
      src_h += (src_stride << 1);                                        \
      __m256i res = convolve_fn(data, coeffs, filt);                     \
      res = _mm256_srai_epi16(_mm256_add_epi16(res, round_const_h), 2);  \
      _mm256_store_si256((__m256i *)&im_block[i * im_stride], res);      \
    }                                                                    \
  } while (0)

#define JNT_CONVOLVE_VERTICAL_FILTER_8TAP                                     \
  do {                                                                        \
    __m256i s[8];                                                             \
    __m256i s0 = _mm256_loadu_si256((__m256i *)(im_block + 0 * im_stride));   \
    __m256i s1 = _mm256_loadu_si256((__m256i *)(im_block + 1 * im_stride));   \
    __m256i s2 = _mm256_loadu_si256((__m256i *)(im_block + 2 * im_stride));   \
    __m256i s3 = _mm256_loadu_si256((__m256i *)(im_block + 3 * im_stride));   \
    __m256i s4 = _mm256_loadu_si256((__m256i *)(im_block + 4 * im_stride));   \
    __m256i s5 = _mm256_loadu_si256((__m256i *)(im_block + 5 * im_stride));   \
                                                                              \
    s[0] = _mm256_unpacklo_epi16(s0, s1);                                     \
    s[1] = _mm256_unpacklo_epi16(s2, s3);                                     \
    s[2] = _mm256_unpacklo_epi16(s4, s5);                                     \
                                                                              \
    s[4] = _mm256_unpackhi_epi16(s0, s1);                                     \
    s[5] = _mm256_unpackhi_epi16(s2, s3);                                     \
    s[6] = _mm256_unpackhi_epi16(s4, s5);                                     \
                                                                              \
    for (i = 0; i < h; i += 2) {                                              \
      const int16_t *data = &im_block[i * im_stride];                         \
                                                                              \
      const __m256i s6 =                                                      \
          _mm256_loadu_si256((__m256i *)(data + 6 * im_stride));              \
      const __m256i s7 =                                                      \
          _mm256_loadu_si256((__m256i *)(data + 7 * im_stride));              \
                                                                              \
      s[3] = _mm256_unpacklo_epi16(s6, s7);                                   \
      s[7] = _mm256_unpackhi_epi16(s6, s7);                                   \
                                                                              \
      const __m256i res_a = convolve(s, coeffs_y);                            \
      const __m256i res_a_round =                                             \
          _mm256_srai_epi32(_mm256_add_epi32(res_a, round_const_v), 7);       \
                                                                              \
      if (w - j > 4) {                                                        \
        const __m256i res_b = convolve(s + 4, coeffs_y);                      \
        const __m256i res_b_round =                                           \
            _mm256_srai_epi32(_mm256_add_epi32(res_b, round_const_v), 7);     \
        const __m256i res_16b = _mm256_packs_epi32(res_a_round, res_b_round); \
        const __m256i res_unsigned = _mm256_add_epi16(res_16b, offset_const); \
        JNT_CONVOLVE_PROCESS_OUTPUT(res_unsigned, j);                         \
      } else {                                                                \
        const __m256i res_16b = _mm256_packs_epi32(res_a_round, res_a_round); \
        const __m256i res_unsigned = _mm256_add_epi16(res_16b, offset_const); \
        JNT_CONVOLVE_PROCESS_OUTPUT(res_unsigned, j);                         \
      }                                                                       \
                                                                              \
      s[0] = s[1];                                                            \
      s[1] = s[2];                                                            \
      s[2] = s[3];                                                            \
                                                                              \
      s[4] = s[5];                                                            \
      s[5] = s[6];                                                            \
      s[6] = s[7];                                                            \
    }                                                                         \
  } while (0)

static inline void prepare_coeffs_2t_ssse3(
    const InterpFilterParams *const filter_params, const int32_t subpel_q4,
    __m128i *const coeffs /* [4] */) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_8 = _mm_loadu_si128((__m128i *)filter);

  // right shift all filter co-efficients by 1 to reduce the bits required.
  // This extra right shift will be taken care of at the end while rounding
  // the result.
  // Since all filter co-efficients are even, this change will not affect the
  // end result
  assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
                            _mm_set1_epi16((short)0xffff)));

  const __m128i coeffs_1 = _mm_srai_epi16(coeffs_8, 1);

  // coeffs 3 4 3 4 3 4 3 4
  coeffs[0] = _mm_shuffle_epi8(coeffs_1, _mm_set1_epi16(0x0806u));
}

static inline void prepare_coeffs_4t_ssse3(
    const InterpFilterParams *const filter_params, const int32_t subpel_q4,
    __m128i *const coeffs /* [4] */) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_8 = _mm_loadu_si128((__m128i *)filter);

  // right shift all filter co-efficients by 1 to reduce the bits required.
  // This extra right shift will be taken care of at the end while rounding
  // the result.
  // Since all filter co-efficients are even, this change will not affect the
  // end result
  assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
                            _mm_set1_epi16((short)0xffff)));

  const __m128i coeffs_1 = _mm_srai_epi16(coeffs_8, 1);

  // coeffs 2 3 2 3 2 3 2 3
  coeffs[0] = _mm_shuffle_epi8(coeffs_1, _mm_set1_epi16(0x0604u));
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[1] = _mm_shuffle_epi8(coeffs_1, _mm_set1_epi16(0x0a08u));
}

static inline void prepare_coeffs_6t_ssse3(
    const InterpFilterParams *const filter_params, const int32_t subpel_q4,
    __m128i *const coeffs /* [4] */) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_8 = _mm_loadu_si128((__m128i *)filter);

  // right shift all filter co-efficients by 1 to reduce the bits required.
  // This extra right shift will be taken care of at the end while rounding
  // the result.
  // Since all filter co-efficients are even, this change will not affect the
  // end result
  assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
                            _mm_set1_epi16((short)0xffff)));

  const __m128i coeffs_1 = _mm_srai_epi16(coeffs_8, 1);

  // coeffs 2 3 2 3 2 3 2 3
  coeffs[0] = _mm_shuffle_epi8(coeffs_1, _mm_set1_epi16(0x0402u));
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[1] = _mm_shuffle_epi8(coeffs_1, _mm_set1_epi16(0x0806u));
  // coeffs 5 6 5 6 5 6 5 6
  coeffs[2] = _mm_shuffle_epi8(coeffs_1, _mm_set1_epi16(0x0c0au));
}

static inline void prepare_coeffs_ssse3(
    const InterpFilterParams *const filter_params, const int32_t subpel_q4,
    __m128i *const coeffs /* [4] */) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_8 = _mm_loadu_si128((__m128i *)filter);

  // right shift all filter co-efficients by 1 to reduce the bits required.
  // This extra right shift will be taken care of at the end while rounding
  // the result.
  // Since all filter co-efficients are even, this change will not affect the
  // end result
  assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
                            _mm_set1_epi16((short)0xffff)));

  const __m128i coeffs_1 = _mm_srai_epi16(coeffs_8, 1);

  // coeffs 0 1 0 1 0 1 0 1
  coeffs[0] = _mm_shuffle_epi8(coeffs_1, _mm_set1_epi16(0x0200u));
  // coeffs 2 3 2 3 2 3 2 3
  coeffs[1] = _mm_shuffle_epi8(coeffs_1, _mm_set1_epi16(0x0604u));
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[2] = _mm_shuffle_epi8(coeffs_1, _mm_set1_epi16(0x0a08u));
  // coeffs 6 7 6 7 6 7 6 7
  coeffs[3] = _mm_shuffle_epi8(coeffs_1, _mm_set1_epi16(0x0e0cu));
}

static inline void prepare_coeffs_2t_lowbd(
    const InterpFilterParams *const filter_params, const int subpel_q4,
    __m256i *const coeffs /* [4] */) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_8 = _mm_loadu_si128((__m128i *)filter);
  const __m256i filter_coeffs = _mm256_broadcastsi128_si256(coeffs_8);

  // right shift all filter co-efficients by 1 to reduce the bits required.
  // This extra right shift will be taken care of at the end while rounding
  // the result.
  // Since all filter co-efficients are even, this change will not affect the
  // end result
  assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
                            _mm_set1_epi16((int16_t)0xffff)));

  const __m256i coeffs_1 = _mm256_srai_epi16(filter_coeffs, 1);

  // coeffs 3 4 3 4 3 4 3 4
  coeffs[0] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0806u));
}

static inline void prepare_coeffs_4t_lowbd(
    const InterpFilterParams *const filter_params, const int subpel_q4,
    __m256i *const coeffs /* [4] */) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_8 = _mm_loadu_si128((__m128i *)filter);
  const __m256i filter_coeffs = _mm256_broadcastsi128_si256(coeffs_8);

  // right shift all filter co-efficients by 1 to reduce the bits required.
  // This extra right shift will be taken care of at the end while rounding
  // the result.
  // Since all filter co-efficients are even, this change will not affect the
  // end result
  assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
                            _mm_set1_epi16((short)0xffff)));

  const __m256i coeffs_1 = _mm256_srai_epi16(filter_coeffs, 1);

  // coeffs 2 3 2 3 2 3 2 3
  coeffs[0] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0604u));
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[1] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0a08u));
}

static inline void prepare_coeffs_6t_lowbd(
    const InterpFilterParams *const filter_params, const int subpel_q4,
    __m256i *const coeffs /* [4] */) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_8 = _mm_loadu_si128((__m128i *)filter);
  const __m256i filter_coeffs = _mm256_broadcastsi128_si256(coeffs_8);

  // right shift all filter co-efficients by 1 to reduce the bits required.
  // This extra right shift will be taken care of at the end while rounding
  // the result.
  // Since all filter co-efficients are even, this change will not affect the
  // end result
  assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
                            _mm_set1_epi16((int16_t)0xffff)));

  const __m256i coeffs_1 = _mm256_srai_epi16(filter_coeffs, 1);

  // coeffs 1 2 1 2 1 2 1 2
  coeffs[0] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0402u));
  // coeffs 3 4 3 4 3 4 3 4
  coeffs[1] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0806u));
  // coeffs 5 6 5 6 5 6 5 6
  coeffs[2] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0c0au));
}

static inline void prepare_coeffs_lowbd(
    const InterpFilterParams *const filter_params, const int subpel_q4,
    __m256i *const coeffs /* [4] */) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_8 = _mm_loadu_si128((__m128i *)filter);
  const __m256i filter_coeffs = _mm256_broadcastsi128_si256(coeffs_8);

  // right shift all filter co-efficients by 1 to reduce the bits required.
  // This extra right shift will be taken care of at the end while rounding
  // the result.
  // Since all filter co-efficients are even, this change will not affect the
  // end result
  assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
                            _mm_set1_epi16((short)0xffff)));

  const __m256i coeffs_1 = _mm256_srai_epi16(filter_coeffs, 1);

  // coeffs 0 1 0 1 0 1 0 1
  coeffs[0] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0200u));
  // coeffs 2 3 2 3 2 3 2 3
  coeffs[1] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0604u));
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[2] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0a08u));
  // coeffs 6 7 6 7 6 7 6 7
  coeffs[3] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0e0cu));
}

static inline void prepare_coeffs_2t(
    const InterpFilterParams *const filter_params, const int subpel_q4,
    __m256i *const coeffs /* [4] */) {
  const int16_t *filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);

  const __m128i coeff_8 = _mm_loadu_si128((__m128i *)(filter + 1));
  const __m256i coeff = _mm256_broadcastsi128_si256(coeff_8);

  // coeffs 3 4 3 4 3 4 3 4
  coeffs[0] = _mm256_shuffle_epi32(coeff, 0x55);
}

static inline void prepare_coeffs_4t(
    const InterpFilterParams *const filter_params, const int subpel_q4,
    __m256i *const coeffs /* [4] */) {
  const int16_t *filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);

  const __m128i coeff_8 = _mm_loadu_si128((__m128i *)filter);
  const __m256i coeff = _mm256_broadcastsi128_si256(coeff_8);
  // coeffs 2 3 2 3 2 3 2 3
  coeffs[0] = _mm256_shuffle_epi32(coeff, 0x55);
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[1] = _mm256_shuffle_epi32(coeff, 0xaa);
}

static inline void prepare_coeffs_6t(
    const InterpFilterParams *const filter_params, const int subpel_q4,
    __m256i *const coeffs /* [4] */) {
  const int16_t *filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);

  const __m128i coeff_8 = _mm_loadu_si128((__m128i *)(filter + 1));
  const __m256i coeff = _mm256_broadcastsi128_si256(coeff_8);

  // coeffs 1 2 1 2 1 2 1 2
  coeffs[0] = _mm256_shuffle_epi32(coeff, 0x00);
  // coeffs 3 4 3 4 3 4 3 4
  coeffs[1] = _mm256_shuffle_epi32(coeff, 0x55);
  // coeffs 5 6 5 6 5 6 5 6
  coeffs[2] = _mm256_shuffle_epi32(coeff, 0xaa);
}

static inline void prepare_coeffs(const InterpFilterParams *const filter_params,
                                  const int subpel_q4,
                                  __m256i *const coeffs /* [4] */) {
  const int16_t *filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);

  const __m128i coeff_8 = _mm_loadu_si128((__m128i *)filter);
  const __m256i coeff = _mm256_broadcastsi128_si256(coeff_8);

  // coeffs 0 1 0 1 0 1 0 1
  coeffs[0] = _mm256_shuffle_epi32(coeff, 0x00);
  // coeffs 2 3 2 3 2 3 2 3
  coeffs[1] = _mm256_shuffle_epi32(coeff, 0x55);
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[2] = _mm256_shuffle_epi32(coeff, 0xaa);
  // coeffs 6 7 6 7 6 7 6 7
  coeffs[3] = _mm256_shuffle_epi32(coeff, 0xff);
}

static inline void prepare_coeffs_12taps(
    const InterpFilterParams *const filter_params, const int subpel_q4,
    __m256i *const coeffs /* [4] */) {
  const int16_t *filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);

  __m128i coeff_8 = _mm_loadu_si128((__m128i *)filter);
  __m256i coeff = _mm256_broadcastsi128_si256(coeff_8);

  // coeffs 0 1 0 1 0 1 0 1
  coeffs[0] = _mm256_shuffle_epi32(coeff, 0x00);
  // coeffs 2 3 2 3 2 3 2 3
  coeffs[1] = _mm256_shuffle_epi32(coeff, 0x55);
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[2] = _mm256_shuffle_epi32(coeff, 0xaa);
  // coeffs 6 7 6 7 6 7 6 7
  coeffs[3] = _mm256_shuffle_epi32(coeff, 0xff);
  // coeffs 8 9 10 11 0 0 0 0
  coeff_8 = _mm_loadl_epi64((__m128i *)(filter + 8));
  coeff = _mm256_broadcastq_epi64(coeff_8);
  coeffs[4] = _mm256_shuffle_epi32(coeff, 0x00);  // coeffs 8 9 8 9 8 9 8 9
  coeffs[5] = _mm256_shuffle_epi32(coeff, 0x55);  // coeffs 10 11 10 11.. 10 11
}

static inline __m128i convolve_lowbd_4tap_ssse3(const __m128i ss[2],
                                                const __m128i coeffs[2]) {
  const __m128i res_01 = _mm_maddubs_epi16(ss[0], coeffs[0]);
  const __m128i res_23 = _mm_maddubs_epi16(ss[1], coeffs[1]);

  return _mm_add_epi16(res_01, res_23);
}

static inline __m128i convolve_lowbd_6tap_ssse3(const __m128i ss[3],
                                                const __m128i coeffs[3]) {
  const __m128i res_01 = _mm_maddubs_epi16(ss[0], coeffs[0]);
  const __m128i res_23 = _mm_maddubs_epi16(ss[1], coeffs[1]);
  const __m128i res_45 = _mm_maddubs_epi16(ss[2], coeffs[2]);

  const __m128i res = _mm_add_epi16(_mm_add_epi16(res_01, res_45), res_23);

  return res;
}

static inline __m128i convolve_lowbd_ssse3(const __m128i ss[4],
                                           const __m128i coeffs[4]) {
  const __m128i res_01 = _mm_maddubs_epi16(ss[0], coeffs[0]);
  const __m128i res_23 = _mm_maddubs_epi16(ss[1], coeffs[1]);
  const __m128i res_45 = _mm_maddubs_epi16(ss[2], coeffs[2]);
  const __m128i res_67 = _mm_maddubs_epi16(ss[3], coeffs[3]);

  const __m128i res = _mm_add_epi16(_mm_add_epi16(res_01, res_45),
                                    _mm_add_epi16(res_23, res_67));

  return res;
}

static inline __m256i convolve_lowbd(const __m256i *const s,
                                     const __m256i *const coeffs) {
  const __m256i res_01 = _mm256_maddubs_epi16(s[0], coeffs[0]);
  const __m256i res_23 = _mm256_maddubs_epi16(s[1], coeffs[1]);
  const __m256i res_45 = _mm256_maddubs_epi16(s[2], coeffs[2]);
  const __m256i res_67 = _mm256_maddubs_epi16(s[3], coeffs[3]);

  // order: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
  const __m256i res = _mm256_add_epi16(_mm256_add_epi16(res_01, res_45),
                                       _mm256_add_epi16(res_23, res_67));

  return res;
}

static inline __m256i convolve_lowbd_6tap(const __m256i *const s,
                                          const __m256i *const coeffs) {
  const __m256i res_01 = _mm256_maddubs_epi16(s[0], coeffs[0]);
  const __m256i res_23 = _mm256_maddubs_epi16(s[1], coeffs[1]);
  const __m256i res_45 = _mm256_maddubs_epi16(s[2], coeffs[2]);

  // order: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
  const __m256i res =
      _mm256_add_epi16(_mm256_add_epi16(res_01, res_45), res_23);

  return res;
}

static inline __m256i convolve_lowbd_4tap(const __m256i *const s,
                                          const __m256i *const coeffs) {
  const __m256i res_23 = _mm256_maddubs_epi16(s[0], coeffs[0]);
  const __m256i res_45 = _mm256_maddubs_epi16(s[1], coeffs[1]);

  // order: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
  const __m256i res = _mm256_add_epi16(res_45, res_23);

  return res;
}

static inline __m256i convolve_6tap(const __m256i *const s,
                                    const __m256i *const coeffs) {
  const __m256i res_0 = _mm256_madd_epi16(s[0], coeffs[0]);
  const __m256i res_1 = _mm256_madd_epi16(s[1], coeffs[1]);
  const __m256i res_2 = _mm256_madd_epi16(s[2], coeffs[2]);

  const __m256i res = _mm256_add_epi32(_mm256_add_epi32(res_0, res_1), res_2);

  return res;
}

static inline __m256i convolve_12taps(const __m256i *const s,
                                      const __m256i *const coeffs) {
  const __m256i res_0 = _mm256_madd_epi16(s[0], coeffs[0]);
  const __m256i res_1 = _mm256_madd_epi16(s[1], coeffs[1]);
  const __m256i res_2 = _mm256_madd_epi16(s[2], coeffs[2]);
  const __m256i res_3 = _mm256_madd_epi16(s[3], coeffs[3]);
  const __m256i res_4 = _mm256_madd_epi16(s[4], coeffs[4]);
  const __m256i res_5 = _mm256_madd_epi16(s[5], coeffs[5]);

  const __m256i res1 = _mm256_add_epi32(_mm256_add_epi32(res_0, res_1),
                                        _mm256_add_epi32(res_2, res_3));
  const __m256i res = _mm256_add_epi32(_mm256_add_epi32(res_4, res_5), res1);

  return res;
}

static inline __m256i convolve(const __m256i *const s,
                               const __m256i *const coeffs) {
  const __m256i res_0 = _mm256_madd_epi16(s[0], coeffs[0]);
  const __m256i res_1 = _mm256_madd_epi16(s[1], coeffs[1]);
  const __m256i res_2 = _mm256_madd_epi16(s[2], coeffs[2]);
  const __m256i res_3 = _mm256_madd_epi16(s[3], coeffs[3]);

  const __m256i res = _mm256_add_epi32(_mm256_add_epi32(res_0, res_1),
                                       _mm256_add_epi32(res_2, res_3));

  return res;
}

static inline __m256i convolve_4tap(const __m256i *const s,
                                    const __m256i *const coeffs) {
  const __m256i res_1 = _mm256_madd_epi16(s[0], coeffs[0]);
  const __m256i res_2 = _mm256_madd_epi16(s[1], coeffs[1]);

  const __m256i res = _mm256_add_epi32(res_1, res_2);
  return res;
}

static inline __m128i convolve_lowbd_x_2tap_ssse3(const __m128i data,
                                                  const __m128i *const coeffs,
                                                  const __m128i *const filt) {
  __m128i s;
  s = _mm_shuffle_epi8(data, filt[0]);

  return _mm_maddubs_epi16(s, coeffs[0]);
}

static inline __m128i convolve_lowbd_x_4tap_ssse3(const __m128i data,
                                                  const __m128i *const coeffs,
                                                  const __m128i *const filt) {
  __m128i s[2];

  s[0] = _mm_shuffle_epi8(data, filt[0]);
  s[1] = _mm_shuffle_epi8(data, filt[1]);

  return convolve_lowbd_4tap_ssse3(s, coeffs);
}

static inline __m256i convolve_lowbd_x(const __m256i data,
                                       const __m256i *const coeffs,
                                       const __m256i *const filt) {
  __m256i s[4];

  s[0] = _mm256_shuffle_epi8(data, filt[0]);
  s[1] = _mm256_shuffle_epi8(data, filt[1]);
  s[2] = _mm256_shuffle_epi8(data, filt[2]);
  s[3] = _mm256_shuffle_epi8(data, filt[3]);

  return convolve_lowbd(s, coeffs);
}

static inline __m256i convolve_lowbd_x_6tap(const __m256i data,
                                            const __m256i *const coeffs,
                                            const __m256i *const filt) {
  __m256i s[4];

  s[0] = _mm256_shuffle_epi8(data, filt[0]);
  s[1] = _mm256_shuffle_epi8(data, filt[1]);
  s[2] = _mm256_shuffle_epi8(data, filt[2]);

  return convolve_lowbd_6tap(s, coeffs);
}

static inline __m256i convolve_lowbd_x_4tap(const __m256i data,
                                            const __m256i *const coeffs,
                                            const __m256i *const filt) {
  __m256i s[2];

  s[0] = _mm256_shuffle_epi8(data, filt[0]);
  s[1] = _mm256_shuffle_epi8(data, filt[1]);

  return convolve_lowbd_4tap(s, coeffs);
}

static inline __m256i convolve_lowbd_x_2tap(const __m256i data,
                                            const __m256i *const coeffs,
                                            const __m256i *const filt) {
  __m256i s;
  s = _mm256_shuffle_epi8(data, filt[0]);

  return _mm256_maddubs_epi16(s, coeffs[0]);
}

static inline void add_store_aligned_256(CONV_BUF_TYPE *const dst,
                                         const __m256i *const res,
                                         const int do_average) {
  __m256i d;
  if (do_average) {
    d = _mm256_load_si256((__m256i *)dst);
    d = _mm256_add_epi32(d, *res);
    d = _mm256_srai_epi32(d, 1);
  } else {
    d = *res;
  }
  _mm256_store_si256((__m256i *)dst, d);
}

static inline __m256i comp_avg(const __m256i *const data_ref_0,
                               const __m256i *const res_unsigned,
                               const __m256i *const wt,
                               const int use_dist_wtd_comp_avg) {
  __m256i res;
  if (use_dist_wtd_comp_avg) {
    const __m256i data_lo = _mm256_unpacklo_epi16(*data_ref_0, *res_unsigned);
    const __m256i data_hi = _mm256_unpackhi_epi16(*data_ref_0, *res_unsigned);

    const __m256i wt_res_lo = _mm256_madd_epi16(data_lo, *wt);
    const __m256i wt_res_hi = _mm256_madd_epi16(data_hi, *wt);

    const __m256i res_lo = _mm256_srai_epi32(wt_res_lo, DIST_PRECISION_BITS);
    const __m256i res_hi = _mm256_srai_epi32(wt_res_hi, DIST_PRECISION_BITS);

    res = _mm256_packs_epi32(res_lo, res_hi);
  } else {
    const __m256i wt_res = _mm256_add_epi16(*data_ref_0, *res_unsigned);
    res = _mm256_srai_epi16(wt_res, 1);
  }
  return res;
}

static inline __m256i convolve_rounding(const __m256i *const res_unsigned,
                                        const __m256i *const offset_const,
                                        const __m256i *const round_const,
                                        const int round_shift) {
  const __m256i res_signed = _mm256_sub_epi16(*res_unsigned, *offset_const);
  const __m256i res_round = _mm256_srai_epi16(
      _mm256_add_epi16(res_signed, *round_const), round_shift);
  return res_round;
}

static inline __m256i highbd_comp_avg(const __m256i *const data_ref_0,
                                      const __m256i *const res_unsigned,
                                      const __m256i *const wt0,
                                      const __m256i *const wt1,
                                      const int use_dist_wtd_comp_avg) {
  __m256i res;
  if (use_dist_wtd_comp_avg) {
    const __m256i wt0_res = _mm256_mullo_epi32(*data_ref_0, *wt0);
    const __m256i wt1_res = _mm256_mullo_epi32(*res_unsigned, *wt1);
    const __m256i wt_res = _mm256_add_epi32(wt0_res, wt1_res);
    res = _mm256_srai_epi32(wt_res, DIST_PRECISION_BITS);
  } else {
    const __m256i wt_res = _mm256_add_epi32(*data_ref_0, *res_unsigned);
    res = _mm256_srai_epi32(wt_res, 1);
  }
  return res;
}

static inline __m256i highbd_convolve_rounding(
    const __m256i *const res_unsigned, const __m256i *const offset_const,
    const __m256i *const round_const, const int round_shift) {
  const __m256i res_signed = _mm256_sub_epi32(*res_unsigned, *offset_const);
  const __m256i res_round = _mm256_srai_epi32(
      _mm256_add_epi32(res_signed, *round_const), round_shift);

  return res_round;
}

static inline __m256i round_sr_x_avx2(const __m256i data) {
  // we can perform the below steps:
  // data = (data + 2) >> 2
  // data = (data + 8) >> 4,
  // in the below form as well
  // data = (data + 0x22) >> 6
  const __m256i value = _mm256_set1_epi16(34);
  const __m256i reg = _mm256_add_epi16(data, value);
  return _mm256_srai_epi16(reg, 6);
}

static inline __m128i convolve_x_4tap_4x2_ssse3(const uint8_t *const src,
                                                const ptrdiff_t src_stride,
                                                __m128i *const coeffs) {
  __m128i data[2];
  const __m128i f_l0 = _mm_load_si128((__m128i const *)filt1_global_sse2);
  const __m128i f_l1 = _mm_load_si128((__m128i const *)filt2_global_sse2);
  const __m128i src_1 =
      load_8bit_8x2_to_1_reg_sse2(src, (int)(sizeof(*src) * src_stride));

  data[0] = _mm_shuffle_epi8(src_1, f_l0);
  data[1] = _mm_shuffle_epi8(src_1, f_l1);
  return convolve_lowbd_4tap_ssse3(data, coeffs);
}

static inline __m128i round_sr_x_ssse3(const __m128i data) {
  const __m128i val = _mm_set1_epi16(34);
  const __m128i reg = _mm_add_epi16(data, val);
  return _mm_srai_epi16(reg, 6);
}

static inline void store_8bit_4x2_sse2(const __m128i reg, uint8_t *const dst,
                                       const ptrdiff_t dst_stride) {
  xx_storel_32(dst, reg);
  *(uint32_t *)(dst + dst_stride) =
      ((uint32_t)_mm_extract_epi16(reg, 3) << 16) | _mm_extract_epi16(reg, 2);
}

static inline void pack_store_u8_4x2_sse2(const __m128i reg, uint8_t *const dst,
                                          const ptrdiff_t dst_stride) {
  const __m128i reg_pack = _mm_packus_epi16(reg, reg);
  store_8bit_4x2_sse2(reg_pack, dst, dst_stride);
}

static inline __m128i convolve_x_4tap_2x2_ssse3(const uint8_t *const src,
                                                const ptrdiff_t src_stride,
                                                __m128i *const coeffs) {
  __m128i data[2];
  const __m128i f_0 = _mm_load_si128((__m128i const *)filt3_global_sse2);
  const __m128i f_1 = _mm_load_si128((__m128i const *)filt4_global_sse2);
  const __m128i reg =
      load_8bit_8x2_to_1_reg_sse2(src, (int)(sizeof(*src) * src_stride));

  data[0] = _mm_shuffle_epi8(reg, f_0);
  data[1] = _mm_shuffle_epi8(reg, f_1);
  return convolve_lowbd_4tap_ssse3(data, coeffs);
}

static inline void pack_store_u8_2x2_sse2(const __m128i reg, uint8_t *const dst,
                                          const ptrdiff_t dst_stride) {
  const __m128i data = _mm_packus_epi16(reg, reg);
  *(int16_t *)dst = (int16_t)_mm_cvtsi128_si32(data);
  *(int16_t *)(dst + dst_stride) = (int16_t)_mm_extract_epi16(data, 1);
}

static inline __m128i convolve_x_2tap_ssse3(const __m128i *data,
                                            const __m128i *coeff) {
  return _mm_maddubs_epi16(data[0], coeff[0]);
}

static inline __m128i load8_x_4x2_sse4(const void *const src,
                                       const ptrdiff_t offset) {
  const __m128i s = _mm_cvtsi32_si128(loadu_int32(src));
  return _mm_insert_epi32(s, loadu_int32((uint8_t *)src + offset), 1);
}

static inline __m128i load_x_u8_4x2_sse4(const uint8_t *const src,
                                         const ptrdiff_t stride) {
  return load8_x_4x2_sse4(src, sizeof(*src) * stride);
}

static inline __m128i convolve_x_2tap_2x2_ssse3(const uint8_t *const src,
                                                const ptrdiff_t stride,
                                                const __m128i *coeffs) {
  const __m128i flt = _mm_load_si128((__m128i const *)filt5_global_sse2);
  const __m128i reg = load_x_u8_4x2_sse4(src, stride);
  const __m128i data = _mm_shuffle_epi8(reg, flt);
  return convolve_x_2tap_ssse3(&data, coeffs);
}

static inline __m128i convolve_x_2tap_4x2_ssse3(const uint8_t *const src,
                                                const ptrdiff_t stride,
                                                const __m128i *coeffs) {
  const __m128i flt = _mm_load_si128((__m128i const *)filt1_global_sse2);
  const __m128i data =
      load_8bit_8x2_to_1_reg_sse2(src, (int)(sizeof(*src) * stride));
  const __m128i res = _mm_shuffle_epi8(data, flt);
  return convolve_x_2tap_ssse3(&res, coeffs);
}

static inline void convolve_x_2tap_8x2_ssse3(const uint8_t *const src,
                                             const ptrdiff_t stride,
                                             const __m128i *coeffs,
                                             __m128i *data) {
  __m128i res[2];
  const __m128i reg_00 = _mm_loadu_si128((__m128i *)src);
  const __m128i reg_10 = _mm_loadu_si128((__m128i *)(src + stride));
  const __m128i reg_01 = _mm_srli_si128(reg_00, 1);
  const __m128i reg_11 = _mm_srli_si128(reg_10, 1);
  res[0] = _mm_unpacklo_epi8(reg_00, reg_01);
  res[1] = _mm_unpacklo_epi8(reg_10, reg_11);

  data[0] = convolve_x_2tap_ssse3(&res[0], coeffs);
  data[1] = convolve_x_2tap_ssse3(&res[1], coeffs);
}

static inline __m256i loadu_x_8bit_16x2_avx2(const void *const src,
                                             const ptrdiff_t offset) {
  const __m128i reg0 = _mm_loadu_si128((__m128i *)src);
  const __m128i reg1 = _mm_loadu_si128((__m128i *)((uint8_t *)src + offset));
  return _mm256_setr_m128i(reg0, reg1);
}

static inline __m256i convolve_x_2tap_avx2(const __m256i *data,
                                           const __m256i *coeffs) {
  return _mm256_maddubs_epi16(data[0], coeffs[0]);
}

static inline void convolve_x_2tap_16x2_avx2(const uint8_t *const src,
                                             const ptrdiff_t stride,
                                             const __m256i *coeffs,
                                             __m256i *data) {
  const __m256i reg0 = loadu_x_8bit_16x2_avx2(src, stride);
  const __m256i reg1 = loadu_x_8bit_16x2_avx2(src + 1, stride);
  const __m256i res0 = _mm256_unpacklo_epi8(reg0, reg1);
  const __m256i res1 = _mm256_unpackhi_epi8(reg0, reg1);
  data[0] = convolve_x_2tap_avx2(&res0, coeffs);
  data[1] = convolve_x_2tap_avx2(&res1, coeffs);
}

static inline void store_u8_16x2_avx2(const __m256i src, uint8_t *const dst,
                                      const ptrdiff_t stride) {
  const __m128i reg0 = _mm256_castsi256_si128(src);
  const __m128i reg1 = _mm256_extracti128_si256(src, 1);
  _mm_storeu_si128((__m128i *)dst, reg0);
  _mm_storeu_si128((__m128i *)((uint8_t *)dst + stride), reg1);
}

static inline void store_u8_8x2_avx2(const __m256i src, uint8_t *const dst,
                                     const ptrdiff_t stride) {
  const __m128i reg0 = _mm256_castsi256_si128(src);
  const __m128i reg1 = _mm256_extracti128_si256(src, 1);
  _mm_storel_epi64((__m128i *)dst, reg0);
  _mm_storel_epi64((__m128i *)(dst + stride), reg1);
}

static inline void pack_store_16x2_avx2(const __m256i data0,
                                        const __m256i data1, uint8_t *const dst,
                                        const ptrdiff_t stride) {
  const __m256i res = _mm256_packus_epi16(data0, data1);
  store_u8_16x2_avx2(res, dst, stride);
}

static inline void pack_store_8x2_avx2(const __m256i data, uint8_t *const dst,
                                       const ptrdiff_t stride) {
  const __m256i res = _mm256_packus_epi16(data, data);
  store_u8_8x2_avx2(res, dst, stride);
}

static inline void round_pack_store_16x2_avx2(const __m256i *data,
                                              uint8_t *const dst,
                                              const ptrdiff_t dst_stride) {
  __m256i reg[2];

  reg[0] = round_sr_x_avx2(data[0]);
  reg[1] = round_sr_x_avx2(data[1]);
  pack_store_16x2_avx2(reg[0], reg[1], dst, dst_stride);
}

static inline void convolve_x_2tap_32_avx2(const uint8_t *const src,
                                           const __m256i *coeffs,
                                           __m256i *data) {
  const __m256i res0 = _mm256_loadu_si256((__m256i *)src);
  const __m256i res1 = _mm256_loadu_si256((__m256i *)(src + 1));
  const __m256i reg0 = _mm256_unpacklo_epi8(res0, res1);
  const __m256i reg1 = _mm256_unpackhi_epi8(res0, res1);

  data[0] = convolve_x_2tap_avx2(&reg0, coeffs);
  data[1] = convolve_x_2tap_avx2(&reg1, coeffs);
}

static inline void pack_store_32_avx2(const __m256i data0, const __m256i data1,
                                      uint8_t *const dst) {
  const __m256i reg = _mm256_packus_epi16(data0, data1);
  _mm256_storeu_si256((__m256i *)dst, reg);
}

static inline void round_pack_store_32_avx2(const __m256i *data,
                                            uint8_t *const dst) {
  __m256i reg[2];

  reg[0] = round_sr_x_avx2(data[0]);
  reg[1] = round_sr_x_avx2(data[1]);
  pack_store_32_avx2(reg[0], reg[1], dst);
}

static inline void convolve_round_2tap_32_avx2(const uint8_t *const src,
                                               const __m256i *coeffs,
                                               uint8_t *const dst) {
  __m256i data[2];

  convolve_x_2tap_32_avx2(src, coeffs, data);
  round_pack_store_32_avx2(data, dst);
}

static inline void load_avg_store_2tap_32_avx2(const uint8_t *const src,
                                               uint8_t *const dst) {
  const __m256i res0 = _mm256_loadu_si256((__m256i *)src);
  const __m256i res1 = _mm256_loadu_si256((__m256i *)(src + 1));
  const __m256i data = _mm256_avg_epu8(res0, res1);
  _mm256_storeu_si256((__m256i *)dst, data);
}

static inline __m256i load_convolve_8tap_8x2_avx2(const uint8_t *const src,
                                                  const ptrdiff_t stride,
                                                  const __m256i *coeffs,
                                                  const __m256i *flt) {
  const __m256i res = loadu_x_8bit_16x2_avx2(src, stride);
  return convolve_lowbd_x(res, coeffs, flt);
}

static inline void load_convolve_8tap_16x2_avx2(const uint8_t *const src,
                                                const int32_t src_stride,
                                                const __m256i *coeffs,
                                                const __m256i *flt,
                                                __m256i *reg) {
  reg[0] = load_convolve_8tap_8x2_avx2(src + 0, src_stride, coeffs, flt);
  reg[1] = load_convolve_8tap_8x2_avx2(src + 8, src_stride, coeffs, flt);
}

static inline void load_convolve_8tap_32_avx2(const uint8_t *const src,
                                              const __m256i *coeffs,
                                              const __m256i *filt,
                                              __m256i *data) {
  const __m256i reg_0 = _mm256_loadu_si256((__m256i *)src);
  const __m256i reg_8 = _mm256_loadu_si256((__m256i *)(src + 8));

  data[0] = convolve_lowbd_x(reg_0, coeffs, filt);
  data[1] = convolve_lowbd_x(reg_8, coeffs, filt);
}

static inline void load_convolve_round_8tap_32_avx2(const uint8_t *const src,
                                                    const __m256i *coeffs,
                                                    const __m256i *filt,
                                                    uint8_t *const dst) {
  __m256i data[2];

  load_convolve_8tap_32_avx2(src, coeffs, filt, data);
  round_pack_store_32_avx2(data, dst);
}

static inline void load_convolve_6tap_32_avx2(const uint8_t *const src,
                                              const __m256i *coeffs,
                                              const __m256i *filt,
                                              __m256i *data) {
  const __m256i reg0 = _mm256_loadu_si256((__m256i *)src);
  const __m256i reg1 = _mm256_loadu_si256((__m256i *)(src + 8));

  data[0] = convolve_lowbd_x_6tap(reg0, coeffs, filt);
  data[1] = convolve_lowbd_x_6tap(reg1, coeffs, filt);
}

static inline void convolve_sr_store_6tap_32_avx2(const uint8_t *const src,
                                                  const __m256i *coeffs,
                                                  const __m256i *filt,
                                                  uint8_t *const dst) {
  __m256i data[2];

  load_convolve_6tap_32_avx2(src, coeffs, filt, data);
  round_pack_store_32_avx2(data, dst);
}

static inline __m256i load_convolve_6tap_8x2_avx2(const uint8_t *const src,
                                                  const ptrdiff_t stride,
                                                  const __m256i *coeffs,
                                                  const __m256i *filt) {
  const __m256i data = loadu_x_8bit_16x2_avx2(src, stride);
  return convolve_lowbd_x_6tap(data, coeffs, filt);
}

static inline void load_convolve_6tap_16x2_avx2(const uint8_t *const src,
                                                const int32_t src_stride,
                                                const __m256i *coeffs,
                                                const __m256i *filt,
                                                __m256i *data) {
  data[0] = load_convolve_6tap_8x2_avx2(src + 0, src_stride, coeffs, filt);
  data[1] = load_convolve_6tap_8x2_avx2(src + 8, src_stride, coeffs, filt);
}

static inline __m128i round_sr_y_ssse3(const __m128i data) {
  const __m128i value = _mm_set1_epi16(32);
  const __m128i reg = _mm_add_epi16(data, value);
  return _mm_srai_epi16(reg, FILTER_BITS - 1);
}

static inline __m256i round_sr_y_avx2(const __m256i data) {
  const __m256i value = _mm256_set1_epi16(32);
  const __m256i reg = _mm256_add_epi16(data, value);
  return _mm256_srai_epi16(reg, FILTER_BITS - 1);
}

static inline void round_pack_store_y_8x2_avx2(const __m256i res,
                                               uint8_t *const dst,
                                               const ptrdiff_t dst_stride) {
  __m256i r;

  r = round_sr_y_avx2(res);
  pack_store_8x2_avx2(r, dst, dst_stride);
}

static inline void round_pack_store_y_16x2_avx2(const __m256i res[2],
                                                uint8_t *const dst,
                                                const ptrdiff_t dst_stride) {
  __m256i r[2];

  r[0] = round_sr_y_avx2(res[0]);
  r[1] = round_sr_y_avx2(res[1]);
  pack_store_16x2_avx2(r[0], r[1], dst, dst_stride);
}

static inline void round_pack_store_y_32_avx2(const __m256i res[2],
                                              uint8_t *const dst) {
  __m256i r[2];

  r[0] = round_sr_y_avx2(res[0]);
  r[1] = round_sr_y_avx2(res[1]);
  pack_store_32_avx2(r[0], r[1], dst);
}

static inline void round_pack_store_y_32x2_avx2(const __m256i res[4],
                                                uint8_t *const dst,
                                                const ptrdiff_t dst_stride) {
  round_pack_store_y_32_avx2(res, dst);
  round_pack_store_y_32_avx2(res + 2, dst + dst_stride);
}

static inline void convolve_y_2tap_2x2_ssse3(const uint8_t *const data,
                                             const ptrdiff_t stride,
                                             const __m128i *coeffs,
                                             __m128i d[2], __m128i *res) {
  d[1] = _mm_cvtsi32_si128(loadu_int16(data + 1 * stride));
  const __m128i src_01a = _mm_unpacklo_epi16(d[0], d[1]);
  d[0] = _mm_cvtsi32_si128(loadu_int16(data + 2 * stride));
  const __m128i src_12a = _mm_unpacklo_epi16(d[1], d[0]);

  const __m128i s = _mm_unpacklo_epi8(src_01a, src_12a);

  *res = _mm_maddubs_epi16(s, coeffs[0]);
}

static inline void convolve_y_4tap_2x2_ssse3(const uint8_t *const data,
                                             const ptrdiff_t stride,
                                             const __m128i coeffs[2],
                                             __m128i d[4], __m128i s[2],
                                             __m128i *res) {
  d[3] = _mm_cvtsi32_si128(loadu_int16(data + 3 * stride));
  const __m128i src_23a = _mm_unpacklo_epi16(d[2], d[3]);
  d[2] = _mm_cvtsi32_si128(loadu_int16(data + 4 * stride));
  const __m128i src_34a = _mm_unpacklo_epi16(d[3], d[2]);

  s[1] = _mm_unpacklo_epi8(src_23a, src_34a);

  *res = convolve_lowbd_4tap_ssse3(s, coeffs);
}

static inline void convolve_y_6tap_2x2_ssse3(const uint8_t *const data,
                                             const ptrdiff_t stride,
                                             const __m128i coeffs[3],
                                             __m128i d[6], __m128i s[3],
                                             __m128i *res) {
  d[5] = _mm_cvtsi32_si128(loadu_int16(data + 5 * stride));
  const __m128i src_45a = _mm_unpacklo_epi16(d[4], d[5]);
  d[4] = _mm_cvtsi32_si128(loadu_int16(data + 6 * stride));
  const __m128i src_56a = _mm_unpacklo_epi16(d[5], d[4]);

  s[2] = _mm_unpacklo_epi8(src_45a, src_56a);

  *res = convolve_lowbd_6tap_ssse3(s, coeffs);
}

static inline void convolve_y_8tap_2x2_ssse3(const uint8_t *const data,
                                             const ptrdiff_t stride,
                                             const __m128i coeffs[4],
                                             __m128i d[8], __m128i s[4],
                                             __m128i *res) {
  d[7] = _mm_cvtsi32_si128(loadu_int16(data + 7 * stride));
  const __m128i src_67a = _mm_unpacklo_epi16(d[6], d[7]);
  d[6] = _mm_cvtsi32_si128(loadu_int16(data + 8 * stride));
  const __m128i src_78a = _mm_unpacklo_epi16(d[7], d[6]);

  s[3] = _mm_unpacklo_epi8(src_67a, src_78a);

  *res = convolve_lowbd_ssse3(s, coeffs);
}

static inline void convolve_y_2tap_4x2_ssse3(const uint8_t *const data,
                                             const ptrdiff_t stride,
                                             const __m128i *coeffs,
                                             __m128i d[2], __m128i *res) {
  d[1] = _mm_cvtsi32_si128(loadu_int32(data + 1 * stride));
  const __m128i src_01a = _mm_unpacklo_epi32(d[0], d[1]);
  d[0] = _mm_cvtsi32_si128(loadu_int32(data + 2 * stride));
  const __m128i src_12a = _mm_unpacklo_epi32(d[1], d[0]);

  const __m128i s = _mm_unpacklo_epi8(src_01a, src_12a);

  *res = _mm_maddubs_epi16(s, coeffs[0]);
}

static inline void convolve_y_4tap_4x2_ssse3(const uint8_t *const data,
                                             const ptrdiff_t stride,
                                             const __m128i coeffs[2],
                                             __m128i d[4], __m128i s[2],
                                             __m128i *res) {
  d[3] = _mm_cvtsi32_si128(loadu_int32(data + 3 * stride));
  const __m128i src_23a = _mm_unpacklo_epi32(d[2], d[3]);
  d[2] = _mm_cvtsi32_si128(loadu_int32(data + 4 * stride));
  const __m128i src_34a = _mm_unpacklo_epi32(d[3], d[2]);

  s[1] = _mm_unpacklo_epi8(src_23a, src_34a);

  *res = convolve_lowbd_4tap_ssse3(s, coeffs);
}

static inline void convolve_y_6tap_4x2_ssse3(const uint8_t *const data,
                                             const ptrdiff_t stride,
                                             const __m128i coeffs[3],
                                             __m128i d[6], __m128i s[3],
                                             __m128i *res) {
  d[5] = _mm_cvtsi32_si128(loadu_int32(data + 5 * stride));
  const __m128i src_45a = _mm_unpacklo_epi32(d[4], d[5]);
  d[4] = _mm_cvtsi32_si128(loadu_int32(data + 6 * stride));
  const __m128i src_56a = _mm_unpacklo_epi32(d[5], d[4]);

  s[2] = _mm_unpacklo_epi8(src_45a, src_56a);

  *res = convolve_lowbd_6tap_ssse3(s, coeffs);
}

static inline void convolve_y_8tap_4x2_ssse3(const uint8_t *const data,
                                             const ptrdiff_t stride,
                                             const __m128i coeffs[4],
                                             __m128i d[8], __m128i s[4],
                                             __m128i *res) {
  d[7] = _mm_cvtsi32_si128(loadu_int32(data + 7 * stride));
  const __m128i src_67a = _mm_unpacklo_epi32(d[6], d[7]);
  d[6] = _mm_cvtsi32_si128(loadu_int32(data + 8 * stride));
  const __m128i src_78a = _mm_unpacklo_epi32(d[7], d[6]);

  s[3] = _mm_unpacklo_epi8(src_67a, src_78a);

  res[0] = convolve_lowbd_ssse3(s, coeffs);
}

static inline void convolve_y_2tap_8x2_avx2(const uint8_t *const data,
                                            const ptrdiff_t stride,
                                            const __m256i *coeffs, __m128i d[2],
                                            __m256i *res) {
  d[1] = _mm_loadu_si128((__m128i *)(data + 1 * stride));
  const __m256i src_01a = _mm256_setr_m128i(d[0], d[1]);
  d[0] = _mm_loadu_si128((__m128i *)(data + 2 * stride));
  const __m256i src_12a = _mm256_setr_m128i(d[1], d[0]);

  const __m256i s = _mm256_unpacklo_epi8(src_01a, src_12a);

  *res = _mm256_maddubs_epi16(s, coeffs[0]);
}

static inline void convolve_y_4tap_8x2_avx2(const uint8_t *const data,
                                            const ptrdiff_t stride,
                                            const __m256i coeffs[2],
                                            __m128i d[4], __m256i s[2],
                                            __m256i *res) {
  d[3] = _mm_loadu_si128((__m128i *)(data + 3 * stride));
  const __m256i src_23a = _mm256_setr_m128i(d[2], d[3]);
  d[2] = _mm_loadu_si128((__m128i *)(data + 4 * stride));
  const __m256i src_34a = _mm256_setr_m128i(d[3], d[2]);

  s[1] = _mm256_unpacklo_epi8(src_23a, src_34a);

  *res = convolve_lowbd_4tap(s, coeffs);
}

static inline void convolve_y_6tap_8x2_avx2(const uint8_t *const data,
                                            const ptrdiff_t stride,
                                            const __m256i coeffs[3],
                                            __m128i d[6], __m256i s[3],
                                            __m256i *res) {
  d[5] = _mm_loadu_si128((__m128i *)(data + 5 * stride));
  const __m256i src_45a = _mm256_setr_m128i(d[4], d[5]);
  d[4] = _mm_loadu_si128((__m128i *)(data + 6 * stride));
  const __m256i src_56a = _mm256_setr_m128i(d[5], d[4]);

  s[2] = _mm256_unpacklo_epi8(src_45a, src_56a);

  *res = convolve_lowbd_6tap(s, coeffs);
}

static inline void convolve_y_8tap_8x2_avx2(const uint8_t *const data,
                                            const ptrdiff_t stride,
                                            const __m256i coeffs[4],
                                            __m128i d[8], __m256i s[4],
                                            __m256i *res) {
  d[7] = _mm_loadu_si128((__m128i *)(data + 7 * stride));
  const __m256i src_67a = _mm256_setr_m128i(d[6], d[7]);
  d[6] = _mm_loadu_si128((__m128i *)(data + 8 * stride));
  const __m256i src_78a = _mm256_setr_m128i(d[7], d[6]);

  s[3] = _mm256_unpacklo_epi8(src_67a, src_78a);

  *res = convolve_lowbd(s, coeffs);
}

static inline void convolve_y_2tap_16x2_avx2(const uint8_t *const data,
                                             const ptrdiff_t stride,
                                             const __m256i *coeffs,
                                             __m128i d[2], __m256i res[2]) {
  d[1] = _mm_loadu_si128((__m128i *)(data + 1 * stride));
  const __m256i src_01a = _mm256_setr_m128i(d[0], d[1]);
  d[0] = _mm_loadu_si128((__m128i *)(data + 2 * stride));
  const __m256i src_12a = _mm256_setr_m128i(d[1], d[0]);

  const __m256i s0 = _mm256_unpacklo_epi8(src_01a, src_12a);
  const __m256i s1 = _mm256_unpackhi_epi8(src_01a, src_12a);

  res[0] = _mm256_maddubs_epi16(s0, coeffs[0]);
  res[1] = _mm256_maddubs_epi16(s1, coeffs[0]);
}

static inline void convolve_y_4tap_16x2_avx2(const uint8_t *const data,
                                             const ptrdiff_t stride,
                                             const __m256i coeffs[2],
                                             __m128i d[4], __m256i s[4],
                                             __m256i res[2]) {
  d[3] = _mm_loadu_si128((__m128i *)(data + 3 * stride));
  const __m256i src_23a = _mm256_setr_m128i(d[2], d[3]);
  d[2] = _mm_loadu_si128((__m128i *)(data + 4 * stride));
  const __m256i src_34a = _mm256_setr_m128i(d[3], d[2]);

  s[1] = _mm256_unpacklo_epi8(src_23a, src_34a);
  s[3] = _mm256_unpackhi_epi8(src_23a, src_34a);

  res[0] = convolve_lowbd_4tap(s, coeffs);
  res[1] = convolve_lowbd_4tap(s + 2, coeffs);
}

static inline void convolve_y_6tap_16x2_avx2(const uint8_t *const data,
                                             const ptrdiff_t stride,
                                             const __m256i coeffs[3],
                                             __m128i d[6], __m256i s[6],
                                             __m256i res[2]) {
  d[5] = _mm_loadu_si128((__m128i *)(data + 5 * stride));
  const __m256i src_45a = _mm256_setr_m128i(d[4], d[5]);
  d[4] = _mm_loadu_si128((__m128i *)(data + 6 * stride));
  const __m256i src_56a = _mm256_setr_m128i(d[5], d[4]);

  s[2] = _mm256_unpacklo_epi8(src_45a, src_56a);
  s[5] = _mm256_unpackhi_epi8(src_45a, src_56a);

  res[0] = convolve_lowbd_6tap(s, coeffs);
  res[1] = convolve_lowbd_6tap(s + 3, coeffs);
}

static inline void convolve_y_8tap_16x2_avx2(const uint8_t *const data,
                                             const ptrdiff_t stride,
                                             const __m256i coeffs[4],
                                             __m128i d[8], __m256i s[8],
                                             __m256i res[2]) {
  d[7] = _mm_loadu_si128((__m128i *)(data + 7 * stride));
  const __m256i src_67a = _mm256_setr_m128i(d[6], d[7]);
  d[6] = _mm_loadu_si128((__m128i *)(data + 8 * stride));
  const __m256i src_78a = _mm256_setr_m128i(d[7], d[6]);

  s[3] = _mm256_unpacklo_epi8(src_67a, src_78a);
  s[7] = _mm256_unpackhi_epi8(src_67a, src_78a);

  res[0] = convolve_lowbd(s, coeffs);
  res[1] = convolve_lowbd(s + 4, coeffs);
}

static inline void convolve_y_2tap_32x2_avx2(const uint8_t *const data,
                                             const ptrdiff_t stride,
                                             const __m256i *coeffs,
                                             __m256i d[2], __m256i res[4]) {
  d[1] = _mm256_loadu_si256((__m256i *)(data + 1 * stride));
  const __m256i s00 = _mm256_unpacklo_epi8(d[0], d[1]);
  const __m256i s01 = _mm256_unpackhi_epi8(d[0], d[1]);
  d[0] = _mm256_loadu_si256((__m256i *)(data + 2 * stride));
  const __m256i s10 = _mm256_unpacklo_epi8(d[1], d[0]);
  const __m256i s11 = _mm256_unpackhi_epi8(d[1], d[0]);

  res[0] = _mm256_maddubs_epi16(s00, coeffs[0]);
  res[1] = _mm256_maddubs_epi16(s01, coeffs[0]);
  res[2] = _mm256_maddubs_epi16(s10, coeffs[0]);
  res[3] = _mm256_maddubs_epi16(s11, coeffs[0]);
}

static inline void convolve_y_4tap_32x2_avx2(const uint8_t *const data,
                                             const ptrdiff_t stride,
                                             const __m256i coeffs[2],
                                             __m256i d[4], __m256i s1[4],
                                             __m256i s2[4], __m256i res[4]) {
  d[3] = _mm256_loadu_si256((__m256i *)(data + 3 * stride));
  s1[1] = _mm256_unpacklo_epi8(d[2], d[3]);
  s1[3] = _mm256_unpackhi_epi8(d[2], d[3]);
  d[2] = _mm256_loadu_si256((__m256i *)(data + 4 * stride));
  s2[1] = _mm256_unpacklo_epi8(d[3], d[2]);
  s2[3] = _mm256_unpackhi_epi8(d[3], d[2]);

  res[0] = convolve_lowbd_4tap(s1, coeffs);
  res[1] = convolve_lowbd_4tap(s1 + 2, coeffs);
  res[2] = convolve_lowbd_4tap(s2, coeffs);
  res[3] = convolve_lowbd_4tap(s2 + 2, coeffs);
}
#endif  // AOM_AOM_DSP_X86_CONVOLVE_AVX2_H_
