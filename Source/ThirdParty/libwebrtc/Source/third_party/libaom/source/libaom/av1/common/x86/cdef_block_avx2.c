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

#include "aom_dsp/aom_simd.h"
#define SIMD_FUNC(name) name##_avx2
#include "av1/common/cdef_block_simd.h"

// Mask used to shuffle the elements present in 256bit register.
const int shuffle_reg_256bit[8] = { 0x0b0a0d0c, 0x07060908, 0x03020504,
                                    0x0f0e0100, 0x0b0a0d0c, 0x07060908,
                                    0x03020504, 0x0f0e0100 };

/* partial A is a 16-bit vector of the form:
[x8 - - x1 | x16 - - x9] and partial B has the form:
[0  y1 - y7 | 0 y9 - y15].
This function computes (x1^2+y1^2)*C1 + (x2^2+y2^2)*C2 + ...
(x7^2+y2^7)*C7 + (x8^2+0^2)*C8 on each 128-bit lane. Here the C1..C8 constants
are in const1 and const2. */
static INLINE __m256i fold_mul_and_sum_avx2(__m256i *partiala,
                                            __m256i *partialb,
                                            const __m256i *const1,
                                            const __m256i *const2) {
  __m256i tmp;
  /* Reverse partial B. */
  *partialb = _mm256_shuffle_epi8(
      *partialb, _mm256_loadu_si256((const __m256i *)shuffle_reg_256bit));

  /* Interleave the x and y values of identical indices and pair x8 with 0. */
  tmp = *partiala;
  *partiala = _mm256_unpacklo_epi16(*partiala, *partialb);
  *partialb = _mm256_unpackhi_epi16(tmp, *partialb);

  /* Square and add the corresponding x and y values. */
  *partiala = _mm256_madd_epi16(*partiala, *partiala);
  *partialb = _mm256_madd_epi16(*partialb, *partialb);
  /* Multiply by constant. */
  *partiala = _mm256_mullo_epi32(*partiala, *const1);
  *partialb = _mm256_mullo_epi32(*partialb, *const2);
  /* Sum all results. */
  *partiala = _mm256_add_epi32(*partiala, *partialb);
  return *partiala;
}

static INLINE __m256i hsum4_avx2(__m256i *x0, __m256i *x1, __m256i *x2,
                                 __m256i *x3) {
  const __m256i t0 = _mm256_unpacklo_epi32(*x0, *x1);
  const __m256i t1 = _mm256_unpacklo_epi32(*x2, *x3);
  const __m256i t2 = _mm256_unpackhi_epi32(*x0, *x1);
  const __m256i t3 = _mm256_unpackhi_epi32(*x2, *x3);

  *x0 = _mm256_unpacklo_epi64(t0, t1);
  *x1 = _mm256_unpackhi_epi64(t0, t1);
  *x2 = _mm256_unpacklo_epi64(t2, t3);
  *x3 = _mm256_unpackhi_epi64(t2, t3);
  return _mm256_add_epi32(_mm256_add_epi32(*x0, *x1),
                          _mm256_add_epi32(*x2, *x3));
}

/* Computes cost for directions 0, 5, 6 and 7. We can call this function again
to compute the remaining directions. */
static INLINE __m256i compute_directions_avx2(__m256i *lines,
                                              int32_t cost_frist_8x8[4],
                                              int32_t cost_second_8x8[4]) {
  __m256i partial4a, partial4b, partial5a, partial5b, partial7a, partial7b;
  __m256i partial6;
  __m256i tmp;
  /* Partial sums for lines 0 and 1. */
  partial4a = _mm256_slli_si256(lines[0], 14);
  partial4b = _mm256_srli_si256(lines[0], 2);
  partial4a = _mm256_add_epi16(partial4a, _mm256_slli_si256(lines[1], 12));
  partial4b = _mm256_add_epi16(partial4b, _mm256_srli_si256(lines[1], 4));
  tmp = _mm256_add_epi16(lines[0], lines[1]);
  partial5a = _mm256_slli_si256(tmp, 10);
  partial5b = _mm256_srli_si256(tmp, 6);
  partial7a = _mm256_slli_si256(tmp, 4);
  partial7b = _mm256_srli_si256(tmp, 12);
  partial6 = tmp;

  /* Partial sums for lines 2 and 3. */
  partial4a = _mm256_add_epi16(partial4a, _mm256_slli_si256(lines[2], 10));
  partial4b = _mm256_add_epi16(partial4b, _mm256_srli_si256(lines[2], 6));
  partial4a = _mm256_add_epi16(partial4a, _mm256_slli_si256(lines[3], 8));
  partial4b = _mm256_add_epi16(partial4b, _mm256_srli_si256(lines[3], 8));
  tmp = _mm256_add_epi16(lines[2], lines[3]);
  partial5a = _mm256_add_epi16(partial5a, _mm256_slli_si256(tmp, 8));
  partial5b = _mm256_add_epi16(partial5b, _mm256_srli_si256(tmp, 8));
  partial7a = _mm256_add_epi16(partial7a, _mm256_slli_si256(tmp, 6));
  partial7b = _mm256_add_epi16(partial7b, _mm256_srli_si256(tmp, 10));
  partial6 = _mm256_add_epi16(partial6, tmp);

  /* Partial sums for lines 4 and 5. */
  partial4a = _mm256_add_epi16(partial4a, _mm256_slli_si256(lines[4], 6));
  partial4b = _mm256_add_epi16(partial4b, _mm256_srli_si256(lines[4], 10));
  partial4a = _mm256_add_epi16(partial4a, _mm256_slli_si256(lines[5], 4));
  partial4b = _mm256_add_epi16(partial4b, _mm256_srli_si256(lines[5], 12));
  tmp = _mm256_add_epi16(lines[4], lines[5]);
  partial5a = _mm256_add_epi16(partial5a, _mm256_slli_si256(tmp, 6));
  partial5b = _mm256_add_epi16(partial5b, _mm256_srli_si256(tmp, 10));
  partial7a = _mm256_add_epi16(partial7a, _mm256_slli_si256(tmp, 8));
  partial7b = _mm256_add_epi16(partial7b, _mm256_srli_si256(tmp, 8));
  partial6 = _mm256_add_epi16(partial6, tmp);

  /* Partial sums for lines 6 and 7. */
  partial4a = _mm256_add_epi16(partial4a, _mm256_slli_si256(lines[6], 2));
  partial4b = _mm256_add_epi16(partial4b, _mm256_srli_si256(lines[6], 14));
  partial4a = _mm256_add_epi16(partial4a, lines[7]);
  tmp = _mm256_add_epi16(lines[6], lines[7]);
  partial5a = _mm256_add_epi16(partial5a, _mm256_slli_si256(tmp, 4));
  partial5b = _mm256_add_epi16(partial5b, _mm256_srli_si256(tmp, 12));
  partial7a = _mm256_add_epi16(partial7a, _mm256_slli_si256(tmp, 10));
  partial7b = _mm256_add_epi16(partial7b, _mm256_srli_si256(tmp, 6));
  partial6 = _mm256_add_epi16(partial6, tmp);

  const __m256i const_reg_1 =
      _mm256_set_epi32(210, 280, 420, 840, 210, 280, 420, 840);
  const __m256i const_reg_2 =
      _mm256_set_epi32(105, 120, 140, 168, 105, 120, 140, 168);
  const __m256i const_reg_3 = _mm256_set_epi32(210, 420, 0, 0, 210, 420, 0, 0);
  const __m256i const_reg_4 =
      _mm256_set_epi32(105, 105, 105, 140, 105, 105, 105, 140);

  /* Compute costs in terms of partial sums. */
  partial4a =
      fold_mul_and_sum_avx2(&partial4a, &partial4b, &const_reg_1, &const_reg_2);
  partial7a =
      fold_mul_and_sum_avx2(&partial7a, &partial7b, &const_reg_3, &const_reg_4);
  partial5a =
      fold_mul_and_sum_avx2(&partial5a, &partial5b, &const_reg_3, &const_reg_4);
  partial6 = _mm256_madd_epi16(partial6, partial6);
  partial6 = _mm256_mullo_epi32(partial6, _mm256_set1_epi32(105));

  partial4a = hsum4_avx2(&partial4a, &partial5a, &partial6, &partial7a);
  _mm_storeu_si128((__m128i *)cost_frist_8x8,
                   _mm256_castsi256_si128(partial4a));
  _mm_storeu_si128((__m128i *)cost_second_8x8,
                   _mm256_extractf128_si256(partial4a, 1));

  return partial4a;
}

/* transpose and reverse the order of the lines -- equivalent to a 90-degree
counter-clockwise rotation of the pixels. */
static INLINE void array_reverse_transpose_8x8_avx2(__m256i *in, __m256i *res) {
  const __m256i tr0_0 = _mm256_unpacklo_epi16(in[0], in[1]);
  const __m256i tr0_1 = _mm256_unpacklo_epi16(in[2], in[3]);
  const __m256i tr0_2 = _mm256_unpackhi_epi16(in[0], in[1]);
  const __m256i tr0_3 = _mm256_unpackhi_epi16(in[2], in[3]);
  const __m256i tr0_4 = _mm256_unpacklo_epi16(in[4], in[5]);
  const __m256i tr0_5 = _mm256_unpacklo_epi16(in[6], in[7]);
  const __m256i tr0_6 = _mm256_unpackhi_epi16(in[4], in[5]);
  const __m256i tr0_7 = _mm256_unpackhi_epi16(in[6], in[7]);

  const __m256i tr1_0 = _mm256_unpacklo_epi32(tr0_0, tr0_1);
  const __m256i tr1_1 = _mm256_unpacklo_epi32(tr0_4, tr0_5);
  const __m256i tr1_2 = _mm256_unpackhi_epi32(tr0_0, tr0_1);
  const __m256i tr1_3 = _mm256_unpackhi_epi32(tr0_4, tr0_5);
  const __m256i tr1_4 = _mm256_unpacklo_epi32(tr0_2, tr0_3);
  const __m256i tr1_5 = _mm256_unpacklo_epi32(tr0_6, tr0_7);
  const __m256i tr1_6 = _mm256_unpackhi_epi32(tr0_2, tr0_3);
  const __m256i tr1_7 = _mm256_unpackhi_epi32(tr0_6, tr0_7);

  res[7] = _mm256_unpacklo_epi64(tr1_0, tr1_1);
  res[6] = _mm256_unpackhi_epi64(tr1_0, tr1_1);
  res[5] = _mm256_unpacklo_epi64(tr1_2, tr1_3);
  res[4] = _mm256_unpackhi_epi64(tr1_2, tr1_3);
  res[3] = _mm256_unpacklo_epi64(tr1_4, tr1_5);
  res[2] = _mm256_unpackhi_epi64(tr1_4, tr1_5);
  res[1] = _mm256_unpacklo_epi64(tr1_6, tr1_7);
  res[0] = _mm256_unpackhi_epi64(tr1_6, tr1_7);
}

void cdef_find_dir_dual_avx2(const uint16_t *img1, const uint16_t *img2,
                             int stride, int32_t *var_out_1st,
                             int32_t *var_out_2nd, int coeff_shift,
                             int *out_dir_1st_8x8, int *out_dir_2nd_8x8) {
  int32_t cost_first_8x8[8];
  int32_t cost_second_8x8[8];
  // Used to store the best cost for 2 8x8's.
  int32_t best_cost[2] = { 0 };
  // Best direction for 2 8x8's.
  int best_dir[2] = { 0 };

  const __m128i const_coeff_shift_reg = _mm_cvtsi32_si128(coeff_shift);
  const __m256i const_128_reg = _mm256_set1_epi16(128);
  __m256i lines[8];
  for (int i = 0; i < 8; i++) {
    const __m128i src_1 = _mm_loadu_si128((const __m128i *)&img1[i * stride]);
    const __m128i src_2 = _mm_loadu_si128((const __m128i *)&img2[i * stride]);

    lines[i] = _mm256_insertf128_si256(_mm256_castsi128_si256(src_1), src_2, 1);
    lines[i] = _mm256_sub_epi16(
        _mm256_sra_epi16(lines[i], const_coeff_shift_reg), const_128_reg);
  }

  /* Compute "mostly vertical" directions. */
  const __m256i dir47 =
      compute_directions_avx2(lines, cost_first_8x8 + 4, cost_second_8x8 + 4);

  /* Transpose and reverse the order of the lines. */
  array_reverse_transpose_8x8_avx2(lines, lines);

  /* Compute "mostly horizontal" directions. */
  const __m256i dir03 =
      compute_directions_avx2(lines, cost_first_8x8, cost_second_8x8);

  __m256i max = _mm256_max_epi32(dir03, dir47);
  max =
      _mm256_max_epi32(max, _mm256_or_si256(_mm256_srli_si256(max, 8),
                                            _mm256_slli_si256(max, 16 - (8))));
  max =
      _mm256_max_epi32(max, _mm256_or_si256(_mm256_srli_si256(max, 4),
                                            _mm256_slli_si256(max, 16 - (4))));

  const __m128i first_8x8_output = _mm256_castsi256_si128(max);
  const __m128i second_8x8_output = _mm256_extractf128_si256(max, 1);
  const __m128i cmpeg_res_00 =
      _mm_cmpeq_epi32(first_8x8_output, _mm256_castsi256_si128(dir47));
  const __m128i cmpeg_res_01 =
      _mm_cmpeq_epi32(first_8x8_output, _mm256_castsi256_si128(dir03));
  const __m128i cmpeg_res_10 =
      _mm_cmpeq_epi32(second_8x8_output, _mm256_extractf128_si256(dir47, 1));
  const __m128i cmpeg_res_11 =
      _mm_cmpeq_epi32(second_8x8_output, _mm256_extractf128_si256(dir03, 1));
  const __m128i t_first_8x8 = _mm_packs_epi32(cmpeg_res_01, cmpeg_res_00);
  const __m128i t_second_8x8 = _mm_packs_epi32(cmpeg_res_11, cmpeg_res_10);

  best_cost[0] = _mm_cvtsi128_si32(_mm256_castsi256_si128(max));
  best_cost[1] = _mm_cvtsi128_si32(second_8x8_output);
  best_dir[0] = _mm_movemask_epi8(_mm_packs_epi16(t_first_8x8, t_first_8x8));
  best_dir[0] =
      get_msb(best_dir[0] ^ (best_dir[0] - 1));  // Count trailing zeros
  best_dir[1] = _mm_movemask_epi8(_mm_packs_epi16(t_second_8x8, t_second_8x8));
  best_dir[1] =
      get_msb(best_dir[1] ^ (best_dir[1] - 1));  // Count trailing zeros

  /* Difference between the optimal variance and the variance along the
     orthogonal direction. Again, the sum(x^2) terms cancel out. */
  *var_out_1st = best_cost[0] - cost_first_8x8[(best_dir[0] + 4) & 7];
  *var_out_2nd = best_cost[1] - cost_second_8x8[(best_dir[1] + 4) & 7];

  /* We'd normally divide by 840, but dividing by 1024 is close enough
  for what we're going to do with this. */
  *var_out_1st >>= 10;
  *var_out_2nd >>= 10;
  *out_dir_1st_8x8 = best_dir[0];
  *out_dir_2nd_8x8 = best_dir[1];
}

void cdef_copy_rect8_8bit_to_16bit_avx2(uint16_t *dst, int dstride,
                                        const uint8_t *src, int sstride,
                                        int width, int height) {
  int j = 0;
  int remaining_width = width;
  assert(height % 2 == 0);
  assert(height > 0);
  assert(width > 0);

  // Process multiple 32 pixels at a time.
  if (remaining_width > 31) {
    int i = 0;
    do {
      j = 0;
      do {
        __m128i row00 =
            _mm_loadu_si128((const __m128i *)&src[(i + 0) * sstride + (j + 0)]);
        __m128i row01 = _mm_loadu_si128(
            (const __m128i *)&src[(i + 0) * sstride + (j + 16)]);
        __m128i row10 =
            _mm_loadu_si128((const __m128i *)&src[(i + 1) * sstride + (j + 0)]);
        __m128i row11 = _mm_loadu_si128(
            (const __m128i *)&src[(i + 1) * sstride + (j + 16)]);
        _mm256_storeu_si256((__m256i *)&dst[(i + 0) * dstride + (j + 0)],
                            _mm256_cvtepu8_epi16(row00));
        _mm256_storeu_si256((__m256i *)&dst[(i + 0) * dstride + (j + 16)],
                            _mm256_cvtepu8_epi16(row01));
        _mm256_storeu_si256((__m256i *)&dst[(i + 1) * dstride + (j + 0)],
                            _mm256_cvtepu8_epi16(row10));
        _mm256_storeu_si256((__m256i *)&dst[(i + 1) * dstride + (j + 16)],
                            _mm256_cvtepu8_epi16(row11));
        j += 32;
      } while (j <= width - 32);
      i += 2;
    } while (i < height);
    remaining_width = width & 31;
  }

  // Process 16 pixels at a time.
  if (remaining_width > 15) {
    int i = 0;
    do {
      __m128i row0 =
          _mm_loadu_si128((const __m128i *)&src[(i + 0) * sstride + j]);
      __m128i row1 =
          _mm_loadu_si128((const __m128i *)&src[(i + 1) * sstride + j]);
      _mm256_storeu_si256((__m256i *)&dst[(i + 0) * dstride + j],
                          _mm256_cvtepu8_epi16(row0));
      _mm256_storeu_si256((__m256i *)&dst[(i + 1) * dstride + j],
                          _mm256_cvtepu8_epi16(row1));
      i += 2;
    } while (i < height);
    remaining_width = width & 15;
    j += 16;
  }

  // Process 8 pixels at a time.
  if (remaining_width > 7) {
    int i = 0;
    do {
      __m128i row0 =
          _mm_loadl_epi64((const __m128i *)&src[(i + 0) * sstride + j]);
      __m128i row1 =
          _mm_loadl_epi64((const __m128i *)&src[(i + 1) * sstride + j]);
      _mm_storeu_si128((__m128i *)&dst[(i + 0) * dstride + j],
                       _mm_unpacklo_epi8(row0, _mm_setzero_si128()));
      _mm_storeu_si128((__m128i *)&dst[(i + 1) * dstride + j],
                       _mm_unpacklo_epi8(row1, _mm_setzero_si128()));
      i += 2;
    } while (i < height);
    remaining_width = width & 7;
    j += 8;
  }

  // Process 4 pixels at a time.
  if (remaining_width > 3) {
    int i = 0;
    do {
      __m128i row0 =
          _mm_cvtsi32_si128(*((const int32_t *)&src[(i + 0) * sstride + j]));
      __m128i row1 =
          _mm_cvtsi32_si128(*((const int32_t *)&src[(i + 1) * sstride + j]));
      _mm_storel_epi64((__m128i *)&dst[(i + 0) * dstride + j],
                       _mm_unpacklo_epi8(row0, _mm_setzero_si128()));
      _mm_storel_epi64((__m128i *)&dst[(i + 1) * dstride + j],
                       _mm_unpacklo_epi8(row1, _mm_setzero_si128()));
      i += 2;
    } while (i < height);
    remaining_width = width & 3;
    j += 4;
  }

  // Process the remaining pixels.
  if (remaining_width) {
    for (int i = 0; i < height; i++) {
      for (int k = j; k < width; k++) {
        dst[i * dstride + k] = src[i * sstride + k];
      }
    }
  }
}
