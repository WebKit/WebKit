/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include <immintrin.h>
#include <string.h>

#include "config/av1_rtcd.h"

#include "av1/common/resize.h"

#include "aom_dsp/x86/synonyms.h"

#define ROW_OFFSET 5
#define CAST_HI(x) _mm256_castsi128_si256(x)
#define CAST_LOW(x) _mm256_castsi256_si128(x)

#define PROCESS_RESIZE_Y_WD16                                               \
  const int idx1 = AOMMIN(height - 1, i + 5);                               \
  const int idx2 = AOMMIN(height - 1, i + 6);                               \
  l6 = l10;                                                                 \
  l7 = l11;                                                                 \
  l8 = _mm_loadu_si128((__m128i *)(data + idx1 * stride));                  \
  l9 = _mm_loadu_si128((__m128i *)(data + idx2 * stride));                  \
                                                                            \
  /* g0... g15 | i0... i15 */                                               \
  const __m256i s68 =                                                       \
      _mm256_permute2x128_si256(CAST_HI(l6), CAST_HI(l8), 0x20);            \
  /* h0... h15 | j0... j15 */                                               \
  const __m256i s79 =                                                       \
      _mm256_permute2x128_si256(CAST_HI(l7), CAST_HI(l9), 0x20);            \
                                                                            \
  /* g0h0... g7g7 | i0j0... i7j */                                          \
  s[3] = _mm256_unpacklo_epi8(s68, s79);                                    \
  /* g8h8... g15g15 | i8j8... i15j15 */                                     \
  s[8] = _mm256_unpackhi_epi8(s68, s79);                                    \
                                                                            \
  __m256i res_out[2] = { 0 };                                               \
  resize_convolve(s, coeffs_y, res_out);                                    \
                                                                            \
  /* r00... r07 */                                                          \
  __m256i res_a_round_1 = _mm256_add_epi32(res_out[0], round_const_bits);   \
  /* r20... r27 */                                                          \
  __m256i res_a_round_2 = _mm256_add_epi32(res_out[1], round_const_bits);   \
                                                                            \
  res_a_round_1 = _mm256_sra_epi32(res_a_round_1, round_shift_bits);        \
  res_a_round_2 = _mm256_sra_epi32(res_a_round_2, round_shift_bits);        \
                                                                            \
  __m256i res_out_b[2] = { 0 };                                             \
  resize_convolve(s + 5, coeffs_y, res_out_b);                              \
                                                                            \
  /* r08... r015 */                                                         \
  __m256i res_b_round_1 = _mm256_add_epi32(res_out_b[0], round_const_bits); \
  /* r28... r215 */                                                         \
  __m256i res_b_round_2 = _mm256_add_epi32(res_out_b[1], round_const_bits); \
  res_b_round_1 = _mm256_sra_epi32(res_b_round_1, round_shift_bits);        \
  res_b_round_2 = _mm256_sra_epi32(res_b_round_2, round_shift_bits);        \
                                                                            \
  /* r00... r03 r20... r23 | r04... r07 r24... r27 */                       \
  __m256i res_8bit0 = _mm256_packus_epi32(res_a_round_1, res_a_round_2);    \
  /* r08... r012 r28... r212 | r013... r015 r213... r215 */                 \
  __m256i res_8bit1 = _mm256_packus_epi32(res_b_round_1, res_b_round_2);    \
  /* r00... r07 | r20... r27 */                                             \
  res_8bit0 = _mm256_permute4x64_epi64(res_8bit0, 0xd8);                    \
  /* r08... r015 | r28... r215 */                                           \
  res_8bit1 = _mm256_permute4x64_epi64(res_8bit1, 0xd8);                    \
  /* r00... r015 | r20... r215 */                                           \
  res_8bit1 = _mm256_packus_epi16(res_8bit0, res_8bit1);                    \
  res_8bit0 = _mm256_min_epu8(res_8bit1, clip_pixel);                       \
  res_8bit0 = _mm256_max_epu8(res_8bit0, zero);

#define PROCESS_RESIZE_Y_WD8                                              \
  const int idx1 = AOMMIN(height - 1, i + 5);                             \
  const int idx2 = AOMMIN(height - 1, i + 6);                             \
  l6 = l10;                                                               \
  l7 = l11;                                                               \
  l8 = _mm_loadl_epi64((__m128i *)(data + idx1 * stride));                \
  l9 = _mm_loadl_epi64((__m128i *)(data + idx2 * stride));                \
                                                                          \
  /* g0h0... g7h7 */                                                      \
  s67 = _mm_unpacklo_epi8(l6, l7);                                        \
  /* i0j0...i7j7 */                                                       \
  __m128i s89 = _mm_unpacklo_epi8(l8, l9);                                \
                                                                          \
  /* g0h0...g7g7 | i0j0...i7j7 */                                         \
  s[3] = _mm256_permute2x128_si256(CAST_HI(s67), CAST_HI(s89), 0x20);     \
                                                                          \
  __m256i res_out[2] = { 0 };                                             \
  resize_convolve(s, coeffs_y, res_out);                                  \
                                                                          \
  /* r00... r07 */                                                        \
  __m256i res_a_round_1 = _mm256_add_epi32(res_out[0], round_const_bits); \
  /* r20...r27 */                                                         \
  __m256i res_a_round_2 = _mm256_add_epi32(res_out[1], round_const_bits); \
  res_a_round_1 = _mm256_sra_epi32(res_a_round_1, round_shift_bits);      \
  res_a_round_2 = _mm256_sra_epi32(res_a_round_2, round_shift_bits);      \
                                                                          \
  /* r00...r03 r20...r23 | r04...r07 r24...r27 */                         \
  res_a_round_1 = _mm256_packus_epi32(res_a_round_1, res_a_round_2);      \
  /* r00...r07 | r20...r27 */                                             \
  res_a_round_1 = _mm256_permute4x64_epi64(res_a_round_1, 0xd8);          \
  res_a_round_1 = _mm256_packus_epi16(res_a_round_1, res_a_round_1);      \
  res_a_round_1 = _mm256_min_epu8(res_a_round_1, clip_pixel);             \
  res_a_round_1 = _mm256_max_epu8(res_a_round_1, zero);

#define PROCESS_RESIZE_X_WD32                                                  \
  /* a0 a1 ..... a30 a31 */                                                    \
  __m256i row0 = _mm256_loadu_si256(                                           \
      (__m256i *)&input[i * in_stride + j - filter_offset]);                   \
  /* b0 b1 ..... b30 b31 */                                                    \
  __m256i row1 = _mm256_loadu_si256(                                           \
      (__m256i *)&input[(i + 1) * in_stride + j - filter_offset]);             \
  /* a0 .... a15 || b0.... b15 */                                              \
  __m256i r0 = _mm256_permute2x128_si256(row0, row1, 0x20);                    \
  /* a16 .... a31 || b16 .... b31 */                                           \
  __m256i r1 = _mm256_permute2x128_si256(row0, row1, 0x31);                    \
  filter_offset = 3;                                                           \
                                                                               \
  /* Pad start pixels to the left, while processing the first pixels in the    \
   * row. */                                                                   \
  if (j == 0) {                                                                \
    /* a0 a0 a0 a0 .... a12 || b0 b0 b0 b0 .... b12 */                         \
    row0 = _mm256_shuffle_epi8(r0, wd32_start_pad_mask);                       \
    /* a13 a14 a15 a16.....a28 || b13 b14 b15 b16.....b28 */                   \
    row1 = _mm256_alignr_epi8(r1, r0, 13);                                     \
    r0 = row0;                                                                 \
    r1 = row1;                                                                 \
  }                                                                            \
  const int is_last_cols32 = (j + 32 == filtered_length);                      \
  /* Avoid loading extra pixels at frame boundary.*/                           \
  if (is_last_cols32) row_offset = ROW_OFFSET;                                 \
  /* a29 a30 a31 a32 a33 a34 a35 a36 0 0 ....*/                                \
  __m128i row0_0 = _mm_loadl_epi64(                                            \
      (__m128i *)&input[i * in_stride + 32 + j - filter_offset - row_offset]); \
  /* b29 b30 b31 b32 b33 b34 b35 b36 0 0 .... */                               \
  __m128i row1_0 =                                                             \
      _mm_loadl_epi64((__m128i *)&input[(i + 1) * in_stride + 32 + j -         \
                                        filter_offset - row_offset]);          \
  __m256i r2 = _mm256_permute2x128_si256(                                      \
      _mm256_castsi128_si256(row0_0), _mm256_castsi128_si256(row1_0), 0x20);   \
                                                                               \
  /* Pad end pixels to the right, while processing the last pixels in the      \
   * row. */                                                                   \
  if (is_last_cols32) {                                                        \
    r2 = _mm256_shuffle_epi8(_mm256_srli_si256(r2, ROW_OFFSET),                \
                             wd32_end_pad_mask);                               \
  }                                                                            \
                                                                               \
  /* Process even pixels of the first row  */                                  \
  /* a0 a0 a0 a0 a1 a2 .... a12 | b0 b0 b0 b0 b1 b2 .... b12 */                \
  s0[0] = _mm256_alignr_epi8(r1, r0, 0);                                       \
  /* a0 a0 a1 a2 a3 a4 .... a14 | b0 b0 b1 b2 b3 b4 .... b14 */                \
  s0[1] = _mm256_alignr_epi8(r1, r0, 2);                                       \
  /* a1 a2 a3 a4 a5 a6 .... a16 | b1 b2 b3 b4 b5 b6 .... b16 */                \
  s0[2] = _mm256_alignr_epi8(r1, r0, 4);                                       \
  /* a3 a4 a5 a6 a7 a8 .... a18 | b3 b4 b5 b6 b7 b8 .... b18 */                \
  s0[3] = _mm256_alignr_epi8(r1, r0, 6);                                       \
                                                                               \
  /* Process even pixels of the second row  */                                 \
  /* a13 a14 a15 a16  ..... a28 | b13 b14 b15 b16 ..... b28 */                 \
  s1[0] = _mm256_alignr_epi8(r2, r1, 0);                                       \
  /* a15 a16 a17 a18  ..... a30 | b15 b16 b17 b18 ..... b30 */                 \
  s1[1] = _mm256_alignr_epi8(r2, r1, 2);                                       \
  /* a17 a18 a19 a20  ..... a32 | b17 b18 b19 b20 ..... b32 */                 \
  s1[2] = _mm256_alignr_epi8(r2, r1, 4);                                       \
  /* a19 a20 a21 a22  ..... a34 | b19 b20 b21 b22 ..... b34 */                 \
  s1[3] = _mm256_alignr_epi8(r2, r1, 6);                                       \
                                                                               \
  /* The register res_out_0 stores the result of start-16 pixels corresponding \
   * to the first and second rows whereas res_out_1 stores the end-16          \
   * pixels. */                                                                \
  __m256i res_out_0[2], res_out_1[2];                                          \
  res_out_1[0] = res_out_1[1] = zero;                                          \
  res_out_0[0] = res_out_0[1] = zero;                                          \
  resize_convolve(s0, coeffs_x, res_out_0);                                    \
  resize_convolve(s1, coeffs_x, res_out_1);                                    \
                                                                               \
  /* Result of 32 pixels of row0 (a0 to a32) */                                \
  res_out_0[0] = _mm256_sra_epi32(                                             \
      _mm256_add_epi32(res_out_0[0], round_const_bits), round_shift_bits);     \
  res_out_1[0] = _mm256_sra_epi32(                                             \
      _mm256_add_epi32(res_out_1[0], round_const_bits), round_shift_bits);     \
  /* r00-r03 r08-r011 | r04-r07 r012-r015 */                                   \
  __m256i res_out_r0 = _mm256_packus_epi32(res_out_0[0], res_out_1[0]);        \
                                                                               \
  /* Result of 32 pixels of row1 (b0 to b32) */                                \
  res_out_0[1] = _mm256_sra_epi32(                                             \
      _mm256_add_epi32(res_out_0[1], round_const_bits), round_shift_bits);     \
  res_out_1[1] = _mm256_sra_epi32(                                             \
      _mm256_add_epi32(res_out_1[1], round_const_bits), round_shift_bits);     \
  /* r10-r13 r18-r111 | r14-r17 r112-r115 */                                   \
  __m256i res_out_r1 = _mm256_packus_epi32(res_out_0[1], res_out_1[1]);        \
                                                                               \
  /* Convert the result from 16bit to 8bit */                                  \
  /* r00-r03 r08-r011 r10-r13 r18-r111 | r04-r07 r012-r015 r14-r17 r112-r115   \
   */                                                                          \
  __m256i res_out_r01 = _mm256_packus_epi16(res_out_r0, res_out_r1);           \
  __m256i res_out_row01 = _mm256_min_epu8(res_out_r01, clip_pixel);            \
  res_out_row01 = _mm256_max_epu8(res_out_r01, zero);                          \
  __m128i low_128 = CAST_LOW(res_out_row01);                                   \
  __m128i high_128 = _mm256_extracti128_si256(res_out_row01, 1);               \
                                                                               \
  _mm_storeu_si128((__m128i *)&intbuf[i * dst_stride + j / 2],                 \
                   _mm_unpacklo_epi32(low_128, high_128));                     \
  _mm_storeu_si128((__m128i *)&intbuf[(i + 1) * dst_stride + j / 2],           \
                   _mm_unpackhi_epi32(low_128, high_128));

static inline void resize_convolve(const __m256i *const s,
                                   const __m256i *const coeffs,
                                   __m256i *res_out) {
  const __m256i res_0 = _mm256_maddubs_epi16(s[0], coeffs[0]);
  const __m256i res_1 = _mm256_maddubs_epi16(s[1], coeffs[1]);
  const __m256i res_2 = _mm256_maddubs_epi16(s[2], coeffs[2]);
  const __m256i res_3 = _mm256_maddubs_epi16(s[3], coeffs[3]);

  const __m256i dst_0 = _mm256_add_epi16(res_0, res_1);
  const __m256i dst_1 = _mm256_add_epi16(res_2, res_3);
  // The sum of convolve operation crosses signed 16bit. Hence, the addition
  // should happen in 32bit.
  const __m256i dst_00 = _mm256_cvtepi16_epi32(CAST_LOW(dst_0));
  const __m256i dst_01 =
      _mm256_cvtepi16_epi32(_mm256_extracti128_si256(dst_0, 1));
  const __m256i dst_10 = _mm256_cvtepi16_epi32(CAST_LOW(dst_1));
  const __m256i dst_11 =
      _mm256_cvtepi16_epi32(_mm256_extracti128_si256(dst_1, 1));

  res_out[0] = _mm256_add_epi32(dst_00, dst_10);
  res_out[1] = _mm256_add_epi32(dst_01, dst_11);
}

static inline void prepare_filter_coeffs(const int16_t *filter,
                                         __m256i *const coeffs /* [4] */) {
  // f0 f1 f2 f3 x x x x
  const __m128i sym_even_filter = _mm_loadl_epi64((__m128i *)filter);
  // f0 f1 f2 f3 f0 f1 f2 f3
  const __m128i tmp0 = _mm_shuffle_epi32(sym_even_filter, 0x44);
  // f0 f1 f2 f3 f1 f0 f3 f2
  const __m128i tmp1 = _mm_shufflehi_epi16(tmp0, 0xb1);

  const __m128i filter_8bit = _mm_packs_epi16(tmp1, tmp1);

  // f0 f1 f0 f1 ..
  coeffs[2] = _mm256_broadcastw_epi16(filter_8bit);
  // f2 f3 f2 f3 ..
  coeffs[3] = _mm256_broadcastw_epi16(_mm_bsrli_si128(filter_8bit, 2));
  // f3 f2 f3 f2 ..
  coeffs[0] = _mm256_broadcastw_epi16(_mm_bsrli_si128(filter_8bit, 6));
  // f1 f0 f1 f0 ..
  coeffs[1] = _mm256_broadcastw_epi16(_mm_bsrli_si128(filter_8bit, 4));
}

bool av1_resize_vert_dir_avx2(uint8_t *intbuf, uint8_t *output, int out_stride,
                              int height, int height2, int stride,
                              int start_col) {
  assert(start_col <= stride);
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

  __m256i s[10], coeffs_y[4];
  const int bits = FILTER_BITS;

  const __m128i round_shift_bits = _mm_cvtsi32_si128(bits);
  const __m256i round_const_bits = _mm256_set1_epi32((1 << bits) >> 1);
  const uint8_t max_pixel = 255;
  const __m256i clip_pixel = _mm256_set1_epi8((char)max_pixel);
  const __m256i zero = _mm256_setzero_si256();

  prepare_filter_coeffs(av1_down2_symeven_half_filter, coeffs_y);

  const int num_col16 = stride / 16;
  int remain_col = stride % 16;
  // The core vertical SIMD processes 4 input rows simultaneously to generate
  // output corresponding to 2 rows. To streamline the core loop and eliminate
  // the need for conditional checks, the remaining rows (4 or 6) are processed
  // separately.
  const int remain_row = (height % 4 == 0) ? 4 : 6;

  for (int j = start_col; j < stride - remain_col; j += 16) {
    const uint8_t *data = &intbuf[j];
    const __m128i l3 = _mm_loadu_si128((__m128i *)(data + 0 * stride));
    // Padding top 3 rows with the last available row at the top.
    const __m128i l0 = l3;
    const __m128i l1 = l3;
    const __m128i l2 = l3;
    const __m128i l4 = _mm_loadu_si128((__m128i *)(data + 1 * stride));

    __m128i l6, l7, l8, l9;
    __m128i l5 = _mm_loadu_si128((__m128i *)(data + 2 * stride));
    __m128i l10 = _mm_loadu_si128((__m128i *)(data + 3 * stride));
    __m128i l11 = _mm_loadu_si128((__m128i *)(data + 4 * stride));

    // a0...a15 | c0...c15
    const __m256i s02 =
        _mm256_permute2x128_si256(CAST_HI(l0), CAST_HI(l2), 0x20);
    // b0...b15 | d0...d15
    const __m256i s13 =
        _mm256_permute2x128_si256(CAST_HI(l1), CAST_HI(l3), 0x20);
    // c0...c15 | e0...e15
    const __m256i s24 =
        _mm256_permute2x128_si256(CAST_HI(l2), CAST_HI(l4), 0x20);
    // d0...d15 | f0...f15
    const __m256i s35 =
        _mm256_permute2x128_si256(CAST_HI(l3), CAST_HI(l5), 0x20);
    // e0...e15 | g0...g15
    const __m256i s46 =
        _mm256_permute2x128_si256(CAST_HI(l4), CAST_HI(l10), 0x20);
    // f0...f15 | h0...h15
    const __m256i s57 =
        _mm256_permute2x128_si256(CAST_HI(l5), CAST_HI(l11), 0x20);

    // a0b0...a7b7 | c0d0...c7d7
    s[0] = _mm256_unpacklo_epi8(s02, s13);
    // c0d0...c7d7 | e0f0...e7f7
    s[1] = _mm256_unpacklo_epi8(s24, s35);
    // e0f0...e7f7 | g0h0...g7h7
    s[2] = _mm256_unpacklo_epi8(s46, s57);

    // a8b8...a15b15 | c8d8...c15d15
    s[5] = _mm256_unpackhi_epi8(s02, s13);
    // c8d8...c15d15 | e8f8...e15f15
    s[6] = _mm256_unpackhi_epi8(s24, s35);
    // e8f8...e15f15 | g8h8...g15h15
    s[7] = _mm256_unpackhi_epi8(s46, s57);

    // height to be processed here
    const int process_ht = height - remain_row;
    for (int i = 0; i < process_ht; i += 4) {
      PROCESS_RESIZE_Y_WD16

      _mm_storeu_si128((__m128i *)&output[(i / 2) * out_stride + j],
                       CAST_LOW(res_8bit0));

      _mm_storeu_si128(
          (__m128i *)&output[(i / 2) * out_stride + j + out_stride],
          _mm256_extracti128_si256(res_8bit0, 1));

      // Load the required data for processing of next 4 input rows.
      const int idx7 = AOMMIN(height - 1, i + 7);
      const int idx8 = AOMMIN(height - 1, i + 8);
      l10 = _mm_loadu_si128((__m128i *)(data + idx7 * stride));
      l11 = _mm_loadu_si128((__m128i *)(data + idx8 * stride));

      const __m256i s810 =
          _mm256_permute2x128_si256(CAST_HI(l8), CAST_HI(l10), 0x20);
      const __m256i s911 =
          _mm256_permute2x128_si256(CAST_HI(l9), CAST_HI(l11), 0x20);
      // i0j0... i7j7 | k0l0... k7l7
      s[4] = _mm256_unpacklo_epi8(s810, s911);
      // i8j8... i15j15 | k8l8... k15l15
      s[9] = _mm256_unpackhi_epi8(s810, s911);

      s[0] = s[2];
      s[1] = s[3];
      s[2] = s[4];

      s[5] = s[7];
      s[6] = s[8];
      s[7] = s[9];
    }

    // Process the remaining last 4 or 6 rows here.
    int i = process_ht;
    while (i < height - 1) {
      PROCESS_RESIZE_Y_WD16

      _mm_storeu_si128((__m128i *)&output[(i / 2) * out_stride + j],
                       CAST_LOW(res_8bit0));
      i += 2;

      const int is_store_valid = (i < height - 1);
      if (is_store_valid)
        _mm_storeu_si128((__m128i *)&output[(i / 2) * out_stride + j],
                         _mm256_extracti128_si256(res_8bit0, 1));
      i += 2;

      // Check if there is any remaining height to process. If so, perform the
      // necessary data loading for processing the next row.
      if (i < height - 1) {
        l10 = l11 = l9;
        const __m256i s810 =
            _mm256_permute2x128_si256(CAST_HI(l8), CAST_HI(l10), 0x20);
        const __m256i s911 =
            _mm256_permute2x128_si256(CAST_HI(l9), CAST_HI(l11), 0x20);
        // i0j0... i7j7 | k0l0... k7l7
        s[4] = _mm256_unpacklo_epi8(s810, s911);
        // i8j8... i15j15 | k8l8... k15l15
        s[9] = _mm256_unpackhi_epi8(s810, s911);

        s[0] = s[2];
        s[1] = s[3];
        s[2] = s[4];

        s[5] = s[7];
        s[6] = s[8];
        s[7] = s[9];
      }
    }
  }

  if (remain_col > 7) {
    const int processed_wd = num_col16 * 16;
    remain_col = stride % 8;

    const uint8_t *data = &intbuf[processed_wd];

    const __m128i l3 = _mm_loadl_epi64((__m128i *)(data + 0 * stride));
    // Padding top 3 rows with available top-most row.
    const __m128i l0 = l3;
    const __m128i l1 = l3;
    const __m128i l2 = l3;
    const __m128i l4 = _mm_loadl_epi64((__m128i *)(data + 1 * stride));

    __m128i l6, l7, l8, l9;
    __m128i l5 = _mm_loadl_epi64((__m128i *)(data + 2 * stride));
    __m128i l10 = _mm_loadl_epi64((__m128i *)(data + 3 * stride));
    __m128i l11 = _mm_loadl_epi64((__m128i *)(data + 4 * stride));

    // a0b0...a7b7
    const __m128i s01 = _mm_unpacklo_epi8(l0, l1);
    // c0d0...c7d7
    const __m128i s23 = _mm_unpacklo_epi8(l2, l3);
    // e0f0...e7f7
    const __m128i s45 = _mm_unpacklo_epi8(l4, l5);
    // g0h0...g7h7
    __m128i s67 = _mm_unpacklo_epi8(l10, l11);

    // a0b0...a7b7 | c0d0...c7d7
    s[0] = _mm256_permute2x128_si256(CAST_HI(s01), CAST_HI(s23), 0x20);
    // c0d0...c7d7 | e0f0...e7f7
    s[1] = _mm256_permute2x128_si256(CAST_HI(s23), CAST_HI(s45), 0x20);
    // e0f0...e7f7 | g0h0...g7h7
    s[2] = _mm256_permute2x128_si256(CAST_HI(s45), CAST_HI(s67), 0x20);

    // height to be processed here
    const int process_ht = height - remain_row;
    for (int i = 0; i < process_ht; i += 4) {
      PROCESS_RESIZE_Y_WD8

      _mm_storel_epi64((__m128i *)&output[(i / 2) * out_stride + processed_wd],
                       CAST_LOW(res_a_round_1));

      _mm_storel_epi64(
          (__m128i *)&output[(i / 2) * out_stride + processed_wd + out_stride],
          _mm256_extracti128_si256(res_a_round_1, 1));

      const int idx7 = AOMMIN(height - 1, i + 7);
      const int idx8 = AOMMIN(height - 1, i + 8);
      l10 = _mm_loadl_epi64((__m128i *)(data + idx7 * stride));
      l11 = _mm_loadl_epi64((__m128i *)(data + idx8 * stride));

      // k0l0... k7l7
      const __m128i s10s11 = _mm_unpacklo_epi8(l10, l11);
      // i0j0... i7j7 | k0l0... k7l7
      s[4] = _mm256_permute2x128_si256(CAST_HI(s89), CAST_HI(s10s11), 0x20);

      s[0] = s[2];
      s[1] = s[3];
      s[2] = s[4];
    }

    // Process the remaining last 4 or 6 rows here.
    int i = process_ht;
    while (i < height - 1) {
      PROCESS_RESIZE_Y_WD8

      _mm_storel_epi64((__m128i *)&output[(i / 2) * out_stride + processed_wd],
                       CAST_LOW(res_a_round_1));

      i += 2;

      const int is_store_valid = (i < height - 1);
      if (is_store_valid)
        _mm_storel_epi64(
            (__m128i *)&output[(i / 2) * out_stride + processed_wd],
            _mm256_extracti128_si256(res_a_round_1, 1));
      i += 2;

      // Check rows are still remaining for processing. If yes do the required
      // load of data for the next iteration.
      if (i < height - 1) {
        l10 = l11 = l9;
        // k0l0... k7l7
        const __m128i s10s11 = _mm_unpacklo_epi8(l10, l11);
        // i0j0... i7j7 | k0l0... k7l7
        s[4] = _mm256_permute2x128_si256(CAST_HI(s89), CAST_HI(s10s11), 0x20);

        s[0] = s[2];
        s[1] = s[3];
        s[2] = s[4];
      }
    }
  }

  if (remain_col)
    return av1_resize_vert_dir_c(intbuf, output, out_stride, height, height2,
                                 stride, stride - remain_col);

  return true;
}

// Masks used for width 32 and 8 pixels, with left and right padding
// requirements
static const uint8_t wd32_left_padding_mask[32] = { 0, 0, 0, 0, 1, 2,  3,  4,
                                                    5, 6, 7, 8, 9, 10, 11, 12,
                                                    0, 0, 0, 0, 1, 2,  3,  4,
                                                    5, 6, 7, 8, 9, 10, 11, 12 };

static const uint8_t wd32_right_padding_mask[32] = { 0, 1, 2, 2, 2, 2, 2, 2,
                                                     2, 2, 2, 2, 2, 2, 2, 2,
                                                     0, 1, 2, 2, 2, 2, 2, 2,
                                                     2, 2, 2, 2, 2, 2, 2, 2 };

static const uint8_t wd8_right_padding_mask[32] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 10, 10, 10, 10,
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 10, 10, 10, 10
};

void av1_resize_horz_dir_avx2(const uint8_t *const input, int in_stride,
                              uint8_t *intbuf, int height, int filtered_length,
                              int width2) {
  assert(height % 2 == 0);
  // Invoke SSE2 for width less than 32.
  if (filtered_length < 32) {
    av1_resize_horz_dir_sse2(input, in_stride, intbuf, height, filtered_length,
                             width2);
    return;
  }

  const int filt_length = sizeof(av1_down2_symeven_half_filter);
  assert(filt_length % 2 == 0);
  (void)filt_length;

  __m256i s0[4], s1[4], coeffs_x[4];

  const int bits = FILTER_BITS;
  const int dst_stride = width2;
  const __m128i round_shift_bits = _mm_cvtsi32_si128(bits);
  const __m256i round_const_bits = _mm256_set1_epi32((1 << bits) >> 1);

  const uint8_t max_pixel = 255;
  const __m256i clip_pixel = _mm256_set1_epi8((char)max_pixel);
  const __m256i zero = _mm256_setzero_si256();

  const __m256i wd32_start_pad_mask =
      _mm256_loadu_si256((__m256i *)wd32_left_padding_mask);
  const __m256i wd32_end_pad_mask =
      _mm256_loadu_si256((__m256i *)wd32_right_padding_mask);
  const __m256i wd8_end_pad_mask =
      _mm256_loadu_si256((__m256i *)wd8_right_padding_mask);
  prepare_filter_coeffs(av1_down2_symeven_half_filter, coeffs_x);

  // The core horizontal SIMD processes 32 input pixels of 2 rows simultaneously
  // to generate output corresponding to 2 rows. To streamline the core loop and
  // eliminate the need for conditional checks, the remaining columns (16 or 8)
  // are processed separately.
  if (filtered_length % 32 == 0) {
    for (int i = 0; i < height; i += 2) {
      int filter_offset = 0;
      int row_offset = 0;
      for (int j = 0; j < filtered_length; j += 32) {
        PROCESS_RESIZE_X_WD32
      }
    }
  } else {
    for (int i = 0; i < height; i += 2) {
      int filter_offset = 0;
      int remain_col = filtered_length;
      int row_offset = 0;
      // To avoid pixel over-read at frame boundary, processing of 32 pixels
      // is done using the core loop only if sufficient number of pixels
      // required for the load are present. The remaining pixels are processed
      // separately.
      for (int j = 0; j <= filtered_length - 32; j += 32) {
        if (remain_col == 34 || remain_col == 36) {
          break;
        }
        PROCESS_RESIZE_X_WD32
        remain_col -= 32;
      }

      int wd_processed = filtered_length - remain_col;
      // To avoid pixel over-read at frame boundary, processing of 16 pixels
      // is done only if sufficient number of pixels required for the
      // load are present. The remaining pixels are processed separately.
      if (remain_col > 15 && remain_col != 18 && remain_col != 20) {
        remain_col = filtered_length - wd_processed - 16;
        const int in_idx = i * in_stride + wd_processed;
        const int out_idx = (i * dst_stride) + wd_processed / 2;
        // a0 a1 --- a15
        __m128i row0 =
            _mm_loadu_si128((__m128i *)&input[in_idx - filter_offset]);
        // b0 b1 --- b15
        __m128i row1 = _mm_loadu_si128(
            (__m128i *)&input[in_idx + in_stride - filter_offset]);
        // a0 a1 --- a15 || b0 b1 --- b15
        __m256i r0 =
            _mm256_permute2x128_si256(CAST_HI(row0), CAST_HI(row1), 0x20);
        if (filter_offset == 0) {
          r0 = _mm256_shuffle_epi8(r0, wd32_start_pad_mask);
        }
        filter_offset = 3;
        const int is_last_cols16 = wd_processed + 16 == filtered_length;
        if (is_last_cols16) row_offset = ROW_OFFSET;

        // a16 a17 --- a23
        row0 = _mm_loadl_epi64(
            (__m128i *)&input[in_idx + 16 - row_offset - filter_offset]);
        // b16 b17 --- b23
        row1 = _mm_loadl_epi64((__m128i *)&input[in_idx + 16 + in_stride -
                                                 row_offset - filter_offset]);

        // a16-a23 x x x x| b16-b23 x x x x
        __m256i r1 =
            _mm256_permute2x128_si256(CAST_HI(row0), CAST_HI(row1), 0x20);

        // Pad end pixels to the right, while processing the last pixels in the
        // row.
        if (is_last_cols16) {
          r1 = _mm256_shuffle_epi8(_mm256_srli_si256(r1, ROW_OFFSET),
                                   wd32_end_pad_mask);
        }

        // a0 a1 --- a15 || b0 b1 --- b15
        s0[0] = r0;
        // a2 a3 --- a17 || b2 b3 --- b17
        s0[1] = _mm256_alignr_epi8(r1, r0, 2);
        // a4 a5 --- a19 || b4 b5 --- b19
        s0[2] = _mm256_alignr_epi8(r1, r0, 4);
        // a6 a7 --- a21 || b6 b7 --- b21
        s0[3] = _mm256_alignr_epi8(r1, r0, 6);

        // result for 16 pixels (a0 to a15) of row0 and row1
        __m256i res_out_0[2];
        res_out_0[0] = res_out_0[1] = zero;
        resize_convolve(s0, coeffs_x, res_out_0);

        // r00-r07
        res_out_0[0] = _mm256_sra_epi32(
            _mm256_add_epi32(res_out_0[0], round_const_bits), round_shift_bits);
        // r10-r17
        res_out_0[1] = _mm256_sra_epi32(
            _mm256_add_epi32(res_out_0[1], round_const_bits), round_shift_bits);
        // r00-r03 r10-r13 r04-r07 r14-r17
        __m256i res_out_row01 = _mm256_packus_epi32(res_out_0[0], res_out_0[1]);
        // r00-r03 r10-r13 r00-r03 r10-r13 | r04-r07 r14-r17 r04-r07 r14-r17
        res_out_row01 = _mm256_packus_epi16(res_out_row01, res_out_row01);
        res_out_row01 = _mm256_min_epu8(res_out_row01, clip_pixel);
        res_out_row01 = _mm256_max_epu8(res_out_row01, zero);
        // r00-r03 r10-r13 r04-r07 r14-r17
        __m128i low_result =
            CAST_LOW(_mm256_permute4x64_epi64(res_out_row01, 0xd8));
        // r00-r03 r04-r07 r10-r13 r14-r17
        low_result = _mm_shuffle_epi32(low_result, 0xd8);

        _mm_storel_epi64((__m128i *)&intbuf[out_idx], low_result);
        _mm_storel_epi64((__m128i *)&intbuf[out_idx + dst_stride],
                         _mm_unpackhi_epi64(low_result, low_result));
      }

      // To avoid pixel over-read at frame boundary, processing of 8 pixels
      // is done only if sufficient number of pixels required for the
      // load are present. The remaining pixels are processed by C function.
      wd_processed = filtered_length - remain_col;
      if (remain_col > 7 && remain_col != 10 && remain_col != 12) {
        remain_col = filtered_length - wd_processed - 8;
        const int in_idx = i * in_stride + wd_processed - filter_offset;
        const int out_idx = (i * dst_stride) + wd_processed / 2;
        const int is_last_cols_8 = wd_processed + 8 == filtered_length;
        if (is_last_cols_8) row_offset = ROW_OFFSET;
        // a0 a1 --- a15
        __m128i row0 = _mm_loadu_si128((__m128i *)&input[in_idx - row_offset]);
        // b0 b1 --- b15
        __m128i row1 =
            _mm_loadu_si128((__m128i *)&input[in_idx + in_stride - row_offset]);
        // a0 a1 --- a15 || b0 b1 --- b15
        __m256i r0 =
            _mm256_permute2x128_si256(CAST_HI(row0), CAST_HI(row1), 0x20);

        // Pad end pixels to the right, while processing the last pixels in the
        // row.
        if (is_last_cols_8)
          r0 = _mm256_shuffle_epi8(_mm256_srli_si256(r0, ROW_OFFSET),
                                   wd8_end_pad_mask);

        // a0 a1 a2 a3 a4 a5 a6 a7 | b0 b1 b2 b3 b4 b5 b6 b7
        s0[0] = r0;
        // a2 a3 a4 a5 a6 a7 a8 a9 | b2 b3 b4 b5 b6 b7 b8 b9
        s0[1] = _mm256_bsrli_epi128(r0, 2);
        // a4 a5 a6 a7 a8 a9 a10 a10 |  b4 b5 b6 b7 b8 b9 b10 b10
        s0[2] = _mm256_bsrli_epi128(r0, 4);
        // a6 a7 a8 a9 a10 a10 a10 a10 | b6 b7 b8 b9 b10 b10 b10 b10
        s0[3] = _mm256_bsrli_epi128(r0, 6);

        __m256i res_out_0[2];
        res_out_0[0] = res_out_0[1] = zero;
        resize_convolve(s0, coeffs_x, res_out_0);

        // r00 - r03 | r10 - r13
        __m256i res_out =
            _mm256_permute2x128_si256(res_out_0[0], res_out_0[1], 0x20);
        // r00 - r03 | r10 - r13
        res_out = _mm256_sra_epi32(_mm256_add_epi32(res_out, round_const_bits),
                                   round_shift_bits);
        // r00-r03 r00-r03 r10-r13 r10-r13
        __m256i res_out_row01 = _mm256_packus_epi32(res_out, res_out);
        // r00-r03 r00-r03 r00-r03 r00-r03 r10-r13 r10-r13 r10-r13 r10-r13
        res_out_row01 = _mm256_packus_epi16(res_out_row01, res_out_row01);
        res_out_row01 = _mm256_min_epu8(res_out_row01, clip_pixel);
        res_out_row01 = _mm256_max_epu8(res_out_row01, zero);

        xx_storel_32(intbuf + out_idx, CAST_LOW(res_out_row01));
        xx_storel_32(intbuf + out_idx + dst_stride,
                     _mm256_extracti128_si256(res_out_row01, 1));
      }

      wd_processed = filtered_length - remain_col;
      if (remain_col) {
        const int in_idx = (in_stride * i);
        const int out_idx = (wd_processed / 2) + width2 * i;

        down2_symeven(input + in_idx, filtered_length, intbuf + out_idx,
                      wd_processed);
        down2_symeven(input + in_idx + in_stride, filtered_length,
                      intbuf + out_idx + width2, wd_processed);
      }
    }
  }
}
