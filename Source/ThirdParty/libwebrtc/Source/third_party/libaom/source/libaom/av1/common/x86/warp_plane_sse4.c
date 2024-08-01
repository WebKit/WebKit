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
#include <smmintrin.h>

#include "config/av1_rtcd.h"

#include "av1/common/warped_motion.h"

/* This is a modified version of 'av1_warped_filter' from warped_motion.c:
   * Each coefficient is stored in 8 bits instead of 16 bits
   * The coefficients are rearranged in the column order 0, 2, 4, 6, 1, 3, 5, 7

     This is done in order to avoid overflow: Since the tap with the largest
     coefficient could be any of taps 2, 3, 4 or 5, we can't use the summation
     order ((0 + 1) + (4 + 5)) + ((2 + 3) + (6 + 7)) used in the regular
     convolve functions.

     Instead, we use the summation order
     ((0 + 2) + (4 + 6)) + ((1 + 3) + (5 + 7)).
     The rearrangement of coefficients in this table is so that we can get the
     coefficients into the correct order more quickly.
*/
/* clang-format off */
DECLARE_ALIGNED(8, const int8_t,
                av1_filter_8bit[WARPEDPIXEL_PREC_SHIFTS * 3 + 1][8]) = {
  // [-1, 0)
  { 0, 127,   0, 0,   0,   1, 0, 0}, { 0, 127,   0, 0,  -1,   2, 0, 0},
  { 1, 127,  -1, 0,  -3,   4, 0, 0}, { 1, 126,  -2, 0,  -4,   6, 1, 0},
  { 1, 126,  -3, 0,  -5,   8, 1, 0}, { 1, 125,  -4, 0,  -6,  11, 1, 0},
  { 1, 124,  -4, 0,  -7,  13, 1, 0}, { 2, 123,  -5, 0,  -8,  15, 1, 0},
  { 2, 122,  -6, 0,  -9,  18, 1, 0}, { 2, 121,  -6, 0, -10,  20, 1, 0},
  { 2, 120,  -7, 0, -11,  22, 2, 0}, { 2, 119,  -8, 0, -12,  25, 2, 0},
  { 3, 117,  -8, 0, -13,  27, 2, 0}, { 3, 116,  -9, 0, -13,  29, 2, 0},
  { 3, 114, -10, 0, -14,  32, 3, 0}, { 3, 113, -10, 0, -15,  35, 2, 0},
  { 3, 111, -11, 0, -15,  37, 3, 0}, { 3, 109, -11, 0, -16,  40, 3, 0},
  { 3, 108, -12, 0, -16,  42, 3, 0}, { 4, 106, -13, 0, -17,  45, 3, 0},
  { 4, 104, -13, 0, -17,  47, 3, 0}, { 4, 102, -14, 0, -17,  50, 3, 0},
  { 4, 100, -14, 0, -17,  52, 3, 0}, { 4,  98, -15, 0, -18,  55, 4, 0},
  { 4,  96, -15, 0, -18,  58, 3, 0}, { 4,  94, -16, 0, -18,  60, 4, 0},
  { 4,  91, -16, 0, -18,  63, 4, 0}, { 4,  89, -16, 0, -18,  65, 4, 0},
  { 4,  87, -17, 0, -18,  68, 4, 0}, { 4,  85, -17, 0, -18,  70, 4, 0},
  { 4,  82, -17, 0, -18,  73, 4, 0}, { 4,  80, -17, 0, -18,  75, 4, 0},
  { 4,  78, -18, 0, -18,  78, 4, 0}, { 4,  75, -18, 0, -17,  80, 4, 0},
  { 4,  73, -18, 0, -17,  82, 4, 0}, { 4,  70, -18, 0, -17,  85, 4, 0},
  { 4,  68, -18, 0, -17,  87, 4, 0}, { 4,  65, -18, 0, -16,  89, 4, 0},
  { 4,  63, -18, 0, -16,  91, 4, 0}, { 4,  60, -18, 0, -16,  94, 4, 0},
  { 3,  58, -18, 0, -15,  96, 4, 0}, { 4,  55, -18, 0, -15,  98, 4, 0},
  { 3,  52, -17, 0, -14, 100, 4, 0}, { 3,  50, -17, 0, -14, 102, 4, 0},
  { 3,  47, -17, 0, -13, 104, 4, 0}, { 3,  45, -17, 0, -13, 106, 4, 0},
  { 3,  42, -16, 0, -12, 108, 3, 0}, { 3,  40, -16, 0, -11, 109, 3, 0},
  { 3,  37, -15, 0, -11, 111, 3, 0}, { 2,  35, -15, 0, -10, 113, 3, 0},
  { 3,  32, -14, 0, -10, 114, 3, 0}, { 2,  29, -13, 0,  -9, 116, 3, 0},
  { 2,  27, -13, 0,  -8, 117, 3, 0}, { 2,  25, -12, 0,  -8, 119, 2, 0},
  { 2,  22, -11, 0,  -7, 120, 2, 0}, { 1,  20, -10, 0,  -6, 121, 2, 0},
  { 1,  18,  -9, 0,  -6, 122, 2, 0}, { 1,  15,  -8, 0,  -5, 123, 2, 0},
  { 1,  13,  -7, 0,  -4, 124, 1, 0}, { 1,  11,  -6, 0,  -4, 125, 1, 0},
  { 1,   8,  -5, 0,  -3, 126, 1, 0}, { 1,   6,  -4, 0,  -2, 126, 1, 0},
  { 0,   4,  -3, 0,  -1, 127, 1, 0}, { 0,   2,  -1, 0,   0, 127, 0, 0},
  // [0, 1)
  { 0,   0,   1, 0, 0, 127,   0,  0}, { 0,  -1,   2, 0, 0, 127,   0,  0},
  { 0,  -3,   4, 1, 1, 127,  -2,  0}, { 0,  -5,   6, 1, 1, 127,  -2,  0},
  { 0,  -6,   8, 1, 2, 126,  -3,  0}, {-1,  -7,  11, 2, 2, 126,  -4, -1},
  {-1,  -8,  13, 2, 3, 125,  -5, -1}, {-1, -10,  16, 3, 3, 124,  -6, -1},
  {-1, -11,  18, 3, 4, 123,  -7, -1}, {-1, -12,  20, 3, 4, 122,  -7, -1},
  {-1, -13,  23, 3, 4, 121,  -8, -1}, {-2, -14,  25, 4, 5, 120,  -9, -1},
  {-1, -15,  27, 4, 5, 119, -10, -1}, {-1, -16,  30, 4, 5, 118, -11, -1},
  {-2, -17,  33, 5, 6, 116, -12, -1}, {-2, -17,  35, 5, 6, 114, -12, -1},
  {-2, -18,  38, 5, 6, 113, -13, -1}, {-2, -19,  41, 6, 7, 111, -14, -2},
  {-2, -19,  43, 6, 7, 110, -15, -2}, {-2, -20,  46, 6, 7, 108, -15, -2},
  {-2, -20,  49, 6, 7, 106, -16, -2}, {-2, -21,  51, 7, 7, 104, -16, -2},
  {-2, -21,  54, 7, 7, 102, -17, -2}, {-2, -21,  56, 7, 8, 100, -18, -2},
  {-2, -22,  59, 7, 8,  98, -18, -2}, {-2, -22,  62, 7, 8,  96, -19, -2},
  {-2, -22,  64, 7, 8,  94, -19, -2}, {-2, -22,  67, 8, 8,  91, -20, -2},
  {-2, -22,  69, 8, 8,  89, -20, -2}, {-2, -22,  72, 8, 8,  87, -21, -2},
  {-2, -21,  74, 8, 8,  84, -21, -2}, {-2, -22,  77, 8, 8,  82, -21, -2},
  {-2, -21,  79, 8, 8,  79, -21, -2}, {-2, -21,  82, 8, 8,  77, -22, -2},
  {-2, -21,  84, 8, 8,  74, -21, -2}, {-2, -21,  87, 8, 8,  72, -22, -2},
  {-2, -20,  89, 8, 8,  69, -22, -2}, {-2, -20,  91, 8, 8,  67, -22, -2},
  {-2, -19,  94, 8, 7,  64, -22, -2}, {-2, -19,  96, 8, 7,  62, -22, -2},
  {-2, -18,  98, 8, 7,  59, -22, -2}, {-2, -18, 100, 8, 7,  56, -21, -2},
  {-2, -17, 102, 7, 7,  54, -21, -2}, {-2, -16, 104, 7, 7,  51, -21, -2},
  {-2, -16, 106, 7, 6,  49, -20, -2}, {-2, -15, 108, 7, 6,  46, -20, -2},
  {-2, -15, 110, 7, 6,  43, -19, -2}, {-2, -14, 111, 7, 6,  41, -19, -2},
  {-1, -13, 113, 6, 5,  38, -18, -2}, {-1, -12, 114, 6, 5,  35, -17, -2},
  {-1, -12, 116, 6, 5,  33, -17, -2}, {-1, -11, 118, 5, 4,  30, -16, -1},
  {-1, -10, 119, 5, 4,  27, -15, -1}, {-1,  -9, 120, 5, 4,  25, -14, -2},
  {-1,  -8, 121, 4, 3,  23, -13, -1}, {-1,  -7, 122, 4, 3,  20, -12, -1},
  {-1,  -7, 123, 4, 3,  18, -11, -1}, {-1,  -6, 124, 3, 3,  16, -10, -1},
  {-1,  -5, 125, 3, 2,  13,  -8, -1}, {-1,  -4, 126, 2, 2,  11,  -7, -1},
  { 0,  -3, 126, 2, 1,   8,  -6,  0}, { 0,  -2, 127, 1, 1,   6,  -5,  0},
  { 0,  -2, 127, 1, 1,   4,  -3,  0}, { 0,   0, 127, 0, 0,   2,  -1,  0},
  // [1, 2)
  { 0, 0, 127,   0, 0,   1,   0, 0}, { 0, 0, 127,   0, 0,  -1,   2, 0},
  { 0, 1, 127,  -1, 0,  -3,   4, 0}, { 0, 1, 126,  -2, 0,  -4,   6, 1},
  { 0, 1, 126,  -3, 0,  -5,   8, 1}, { 0, 1, 125,  -4, 0,  -6,  11, 1},
  { 0, 1, 124,  -4, 0,  -7,  13, 1}, { 0, 2, 123,  -5, 0,  -8,  15, 1},
  { 0, 2, 122,  -6, 0,  -9,  18, 1}, { 0, 2, 121,  -6, 0, -10,  20, 1},
  { 0, 2, 120,  -7, 0, -11,  22, 2}, { 0, 2, 119,  -8, 0, -12,  25, 2},
  { 0, 3, 117,  -8, 0, -13,  27, 2}, { 0, 3, 116,  -9, 0, -13,  29, 2},
  { 0, 3, 114, -10, 0, -14,  32, 3}, { 0, 3, 113, -10, 0, -15,  35, 2},
  { 0, 3, 111, -11, 0, -15,  37, 3}, { 0, 3, 109, -11, 0, -16,  40, 3},
  { 0, 3, 108, -12, 0, -16,  42, 3}, { 0, 4, 106, -13, 0, -17,  45, 3},
  { 0, 4, 104, -13, 0, -17,  47, 3}, { 0, 4, 102, -14, 0, -17,  50, 3},
  { 0, 4, 100, -14, 0, -17,  52, 3}, { 0, 4,  98, -15, 0, -18,  55, 4},
  { 0, 4,  96, -15, 0, -18,  58, 3}, { 0, 4,  94, -16, 0, -18,  60, 4},
  { 0, 4,  91, -16, 0, -18,  63, 4}, { 0, 4,  89, -16, 0, -18,  65, 4},
  { 0, 4,  87, -17, 0, -18,  68, 4}, { 0, 4,  85, -17, 0, -18,  70, 4},
  { 0, 4,  82, -17, 0, -18,  73, 4}, { 0, 4,  80, -17, 0, -18,  75, 4},
  { 0, 4,  78, -18, 0, -18,  78, 4}, { 0, 4,  75, -18, 0, -17,  80, 4},
  { 0, 4,  73, -18, 0, -17,  82, 4}, { 0, 4,  70, -18, 0, -17,  85, 4},
  { 0, 4,  68, -18, 0, -17,  87, 4}, { 0, 4,  65, -18, 0, -16,  89, 4},
  { 0, 4,  63, -18, 0, -16,  91, 4}, { 0, 4,  60, -18, 0, -16,  94, 4},
  { 0, 3,  58, -18, 0, -15,  96, 4}, { 0, 4,  55, -18, 0, -15,  98, 4},
  { 0, 3,  52, -17, 0, -14, 100, 4}, { 0, 3,  50, -17, 0, -14, 102, 4},
  { 0, 3,  47, -17, 0, -13, 104, 4}, { 0, 3,  45, -17, 0, -13, 106, 4},
  { 0, 3,  42, -16, 0, -12, 108, 3}, { 0, 3,  40, -16, 0, -11, 109, 3},
  { 0, 3,  37, -15, 0, -11, 111, 3}, { 0, 2,  35, -15, 0, -10, 113, 3},
  { 0, 3,  32, -14, 0, -10, 114, 3}, { 0, 2,  29, -13, 0,  -9, 116, 3},
  { 0, 2,  27, -13, 0,  -8, 117, 3}, { 0, 2,  25, -12, 0,  -8, 119, 2},
  { 0, 2,  22, -11, 0,  -7, 120, 2}, { 0, 1,  20, -10, 0,  -6, 121, 2},
  { 0, 1,  18,  -9, 0,  -6, 122, 2}, { 0, 1,  15,  -8, 0,  -5, 123, 2},
  { 0, 1,  13,  -7, 0,  -4, 124, 1}, { 0, 1,  11,  -6, 0,  -4, 125, 1},
  { 0, 1,   8,  -5, 0,  -3, 126, 1}, { 0, 1,   6,  -4, 0,  -2, 126, 1},
  { 0, 0,   4,  -3, 0,  -1, 127, 1}, { 0, 0,   2,  -1, 0,   0, 127, 0},
  // dummy (replicate row index 191)
  { 0, 0,   2,  -1, 0,   0, 127, 0},
};
/* clang-format on */

// Shuffle masks: we want to convert a sequence of bytes 0, 1, 2, ..., 15
// in an SSE register into two sequences:
// 0, 2, 2, 4, ..., 12, 12, 14, <don't care>
// 1, 3, 3, 5, ..., 13, 13, 15, <don't care>
DECLARE_ALIGNED(16, static const uint8_t,
                even_mask[16]) = { 0, 2,  2,  4,  4,  6,  6,  8,
                                   8, 10, 10, 12, 12, 14, 14, 0 };

DECLARE_ALIGNED(16, static const uint8_t,
                odd_mask[16]) = { 1, 3,  3,  5,  5,  7,  7,  9,
                                  9, 11, 11, 13, 13, 15, 15, 0 };

DECLARE_ALIGNED(16, static const uint8_t,
                shuffle_alpha0_mask01[16]) = { 0, 1, 0, 1, 0, 1, 0, 1,
                                               0, 1, 0, 1, 0, 1, 0, 1 };

DECLARE_ALIGNED(16, static const uint8_t,
                shuffle_alpha0_mask23[16]) = { 2, 3, 2, 3, 2, 3, 2, 3,
                                               2, 3, 2, 3, 2, 3, 2, 3 };

DECLARE_ALIGNED(16, static const uint8_t,
                shuffle_alpha0_mask45[16]) = { 4, 5, 4, 5, 4, 5, 4, 5,
                                               4, 5, 4, 5, 4, 5, 4, 5 };

DECLARE_ALIGNED(16, static const uint8_t,
                shuffle_alpha0_mask67[16]) = { 6, 7, 6, 7, 6, 7, 6, 7,
                                               6, 7, 6, 7, 6, 7, 6, 7 };

DECLARE_ALIGNED(16, static const uint8_t,
                shuffle_gamma0_mask0[16]) = { 0, 1, 2, 3, 0, 1, 2, 3,
                                              0, 1, 2, 3, 0, 1, 2, 3 };

DECLARE_ALIGNED(16, static const uint8_t,
                shuffle_gamma0_mask1[16]) = { 4, 5, 6, 7, 4, 5, 6, 7,
                                              4, 5, 6, 7, 4, 5, 6, 7 };

DECLARE_ALIGNED(16, static const uint8_t,
                shuffle_gamma0_mask2[16]) = { 8, 9, 10, 11, 8, 9, 10, 11,
                                              8, 9, 10, 11, 8, 9, 10, 11 };

DECLARE_ALIGNED(16, static const uint8_t,
                shuffle_gamma0_mask3[16]) = { 12, 13, 14, 15, 12, 13, 14, 15,
                                              12, 13, 14, 15, 12, 13, 14, 15 };

static INLINE void filter_src_pixels(__m128i src, __m128i *tmp, __m128i *coeff,
                                     const int offset_bits_horiz,
                                     const int reduce_bits_horiz, int k) {
  const __m128i src_even =
      _mm_shuffle_epi8(src, _mm_load_si128((__m128i *)even_mask));
  const __m128i src_odd =
      _mm_shuffle_epi8(src, _mm_load_si128((__m128i *)odd_mask));
  // The pixel order we need for 'src' is:
  // 0 2 2 4 4 6 6 8 1 3 3 5 5 7 7 9
  const __m128i src_02 = _mm_unpacklo_epi64(src_even, src_odd);
  const __m128i res_02 = _mm_maddubs_epi16(src_02, coeff[0]);
  // 4 6 6 8 8 10 10 12 5 7 7 9 9 11 11 13
  const __m128i src_46 = _mm_unpacklo_epi64(_mm_srli_si128(src_even, 4),
                                            _mm_srli_si128(src_odd, 4));
  const __m128i res_46 = _mm_maddubs_epi16(src_46, coeff[1]);
  // 1 3 3 5 5 7 7 9 2 4 4 6 6 8 8 10
  const __m128i src_13 =
      _mm_unpacklo_epi64(src_odd, _mm_srli_si128(src_even, 2));
  const __m128i res_13 = _mm_maddubs_epi16(src_13, coeff[2]);
  // 5 7 7 9 9 11 11 13 6 8 8 10 10 12 12 14
  const __m128i src_57 = _mm_unpacklo_epi64(_mm_srli_si128(src_odd, 4),
                                            _mm_srli_si128(src_even, 6));
  const __m128i res_57 = _mm_maddubs_epi16(src_57, coeff[3]);

  const __m128i round_const = _mm_set1_epi16((1 << offset_bits_horiz) +
                                             ((1 << reduce_bits_horiz) >> 1));

  // Note: The values res_02 + res_46 and res_13 + res_57 both
  // fit into int16s at this point, but their sum may be too wide to fit
  // into an int16. However, once we also add round_const, the sum of
  // all of these fits into a uint16.
  //
  // The wrapping behaviour of _mm_add_* is used here to make sure we
  // get the correct result despite converting between different
  // (implicit) types.
  const __m128i res_even = _mm_add_epi16(res_02, res_46);
  const __m128i res_odd = _mm_add_epi16(res_13, res_57);
  const __m128i res =
      _mm_add_epi16(_mm_add_epi16(res_even, res_odd), round_const);
  tmp[k + 7] = _mm_srl_epi16(res, _mm_cvtsi32_si128(reduce_bits_horiz));
}

static INLINE void prepare_horizontal_filter_coeff(int alpha, int sx,
                                                   __m128i *coeff) {
  // Filter even-index pixels
  const __m128i tmp_0 = _mm_loadl_epi64(
      (__m128i *)&av1_filter_8bit[(sx + 0 * alpha) >> WARPEDDIFF_PREC_BITS]);
  const __m128i tmp_1 = _mm_loadl_epi64(
      (__m128i *)&av1_filter_8bit[(sx + 1 * alpha) >> WARPEDDIFF_PREC_BITS]);
  const __m128i tmp_2 = _mm_loadl_epi64(
      (__m128i *)&av1_filter_8bit[(sx + 2 * alpha) >> WARPEDDIFF_PREC_BITS]);
  const __m128i tmp_3 = _mm_loadl_epi64(
      (__m128i *)&av1_filter_8bit[(sx + 3 * alpha) >> WARPEDDIFF_PREC_BITS]);
  const __m128i tmp_4 = _mm_loadl_epi64(
      (__m128i *)&av1_filter_8bit[(sx + 4 * alpha) >> WARPEDDIFF_PREC_BITS]);
  const __m128i tmp_5 = _mm_loadl_epi64(
      (__m128i *)&av1_filter_8bit[(sx + 5 * alpha) >> WARPEDDIFF_PREC_BITS]);
  const __m128i tmp_6 = _mm_loadl_epi64(
      (__m128i *)&av1_filter_8bit[(sx + 6 * alpha) >> WARPEDDIFF_PREC_BITS]);
  const __m128i tmp_7 = _mm_loadl_epi64(
      (__m128i *)&av1_filter_8bit[(sx + 7 * alpha) >> WARPEDDIFF_PREC_BITS]);

  // Coeffs 0 2 0 2 4 6 4 6 1 3 1 3 5 7 5 7 for pixels 0 2
  const __m128i tmp_8 = _mm_unpacklo_epi16(tmp_0, tmp_2);
  // Coeffs 0 2 0 2 4 6 4 6 1 3 1 3 5 7 5 7 for pixels 1 3
  const __m128i tmp_9 = _mm_unpacklo_epi16(tmp_1, tmp_3);
  // Coeffs 0 2 0 2 4 6 4 6 1 3 1 3 5 7 5 7 for pixels 4 6
  const __m128i tmp_10 = _mm_unpacklo_epi16(tmp_4, tmp_6);
  // Coeffs 0 2 0 2 4 6 4 6 1 3 1 3 5 7 5 7 for pixels 5 7
  const __m128i tmp_11 = _mm_unpacklo_epi16(tmp_5, tmp_7);

  // Coeffs 0 2 0 2 0 2 0 2 4 6 4 6 4 6 4 6 for pixels 0 2 4 6
  const __m128i tmp_12 = _mm_unpacklo_epi32(tmp_8, tmp_10);
  // Coeffs 1 3 1 3 1 3 1 3 5 7 5 7 5 7 5 7 for pixels 0 2 4 6
  const __m128i tmp_13 = _mm_unpackhi_epi32(tmp_8, tmp_10);
  // Coeffs 0 2 0 2 0 2 0 2 4 6 4 6 4 6 4 6 for pixels 1 3 5 7
  const __m128i tmp_14 = _mm_unpacklo_epi32(tmp_9, tmp_11);
  // Coeffs 1 3 1 3 1 3 1 3 5 7 5 7 5 7 5 7 for pixels 1 3 5 7
  const __m128i tmp_15 = _mm_unpackhi_epi32(tmp_9, tmp_11);

  // Coeffs 0 2 for pixels 0 2 4 6 1 3 5 7
  coeff[0] = _mm_unpacklo_epi64(tmp_12, tmp_14);
  // Coeffs 4 6 for pixels 0 2 4 6 1 3 5 7
  coeff[1] = _mm_unpackhi_epi64(tmp_12, tmp_14);
  // Coeffs 1 3 for pixels 0 2 4 6 1 3 5 7
  coeff[2] = _mm_unpacklo_epi64(tmp_13, tmp_15);
  // Coeffs 5 7 for pixels 0 2 4 6 1 3 5 7
  coeff[3] = _mm_unpackhi_epi64(tmp_13, tmp_15);
}

static INLINE void prepare_horizontal_filter_coeff_alpha0(int sx,
                                                          __m128i *coeff) {
  // Filter even-index pixels
  const __m128i tmp_0 =
      _mm_loadl_epi64((__m128i *)&av1_filter_8bit[sx >> WARPEDDIFF_PREC_BITS]);

  // Coeffs 0 2 for pixels 0 2 4 6 1 3 5 7
  coeff[0] =
      _mm_shuffle_epi8(tmp_0, _mm_load_si128((__m128i *)shuffle_alpha0_mask01));
  // Coeffs 4 6 for pixels 0 2 4 6 1 3 5 7
  coeff[1] =
      _mm_shuffle_epi8(tmp_0, _mm_load_si128((__m128i *)shuffle_alpha0_mask23));
  // Coeffs 1 3 for pixels 0 2 4 6 1 3 5 7
  coeff[2] =
      _mm_shuffle_epi8(tmp_0, _mm_load_si128((__m128i *)shuffle_alpha0_mask45));
  // Coeffs 5 7 for pixels 0 2 4 6 1 3 5 7
  coeff[3] =
      _mm_shuffle_epi8(tmp_0, _mm_load_si128((__m128i *)shuffle_alpha0_mask67));
}

static INLINE void horizontal_filter(__m128i src, __m128i *tmp, int sx,
                                     int alpha, int k,
                                     const int offset_bits_horiz,
                                     const int reduce_bits_horiz) {
  __m128i coeff[4];
  prepare_horizontal_filter_coeff(alpha, sx, coeff);
  filter_src_pixels(src, tmp, coeff, offset_bits_horiz, reduce_bits_horiz, k);
}

static INLINE void warp_horizontal_filter(const uint8_t *ref, __m128i *tmp,
                                          int stride, int32_t ix4, int32_t iy4,
                                          int32_t sx4, int alpha, int beta,
                                          int p_height, int height, int i,
                                          const int offset_bits_horiz,
                                          const int reduce_bits_horiz) {
  int k;
  for (k = -7; k < AOMMIN(8, p_height - i); ++k) {
    int iy = iy4 + k;
    if (iy < 0)
      iy = 0;
    else if (iy > height - 1)
      iy = height - 1;
    int sx = sx4 + beta * (k + 4);

    // Load source pixels
    const __m128i src =
        _mm_loadu_si128((__m128i *)(ref + iy * stride + ix4 - 7));
    horizontal_filter(src, tmp, sx, alpha, k, offset_bits_horiz,
                      reduce_bits_horiz);
  }
}

static INLINE void warp_horizontal_filter_alpha0(
    const uint8_t *ref, __m128i *tmp, int stride, int32_t ix4, int32_t iy4,
    int32_t sx4, int alpha, int beta, int p_height, int height, int i,
    const int offset_bits_horiz, const int reduce_bits_horiz) {
  (void)alpha;
  int k;
  for (k = -7; k < AOMMIN(8, p_height - i); ++k) {
    int iy = iy4 + k;
    if (iy < 0)
      iy = 0;
    else if (iy > height - 1)
      iy = height - 1;
    int sx = sx4 + beta * (k + 4);

    // Load source pixels
    const __m128i src =
        _mm_loadu_si128((__m128i *)(ref + iy * stride + ix4 - 7));

    __m128i coeff[4];
    prepare_horizontal_filter_coeff_alpha0(sx, coeff);
    filter_src_pixels(src, tmp, coeff, offset_bits_horiz, reduce_bits_horiz, k);
  }
}

static INLINE void warp_horizontal_filter_beta0(
    const uint8_t *ref, __m128i *tmp, int stride, int32_t ix4, int32_t iy4,
    int32_t sx4, int alpha, int beta, int p_height, int height, int i,
    const int offset_bits_horiz, const int reduce_bits_horiz) {
  (void)beta;
  int k;
  __m128i coeff[4];
  prepare_horizontal_filter_coeff(alpha, sx4, coeff);

  for (k = -7; k < AOMMIN(8, p_height - i); ++k) {
    int iy = iy4 + k;
    if (iy < 0)
      iy = 0;
    else if (iy > height - 1)
      iy = height - 1;

    // Load source pixels
    const __m128i src =
        _mm_loadu_si128((__m128i *)(ref + iy * stride + ix4 - 7));
    filter_src_pixels(src, tmp, coeff, offset_bits_horiz, reduce_bits_horiz, k);
  }
}

static INLINE void warp_horizontal_filter_alpha0_beta0(
    const uint8_t *ref, __m128i *tmp, int stride, int32_t ix4, int32_t iy4,
    int32_t sx4, int alpha, int beta, int p_height, int height, int i,
    const int offset_bits_horiz, const int reduce_bits_horiz) {
  (void)beta;
  (void)alpha;
  int k;

  __m128i coeff[4];
  prepare_horizontal_filter_coeff_alpha0(sx4, coeff);

  for (k = -7; k < AOMMIN(8, p_height - i); ++k) {
    int iy = iy4 + k;
    if (iy < 0)
      iy = 0;
    else if (iy > height - 1)
      iy = height - 1;

    // Load source pixels
    const __m128i src =
        _mm_loadu_si128((__m128i *)(ref + iy * stride + ix4 - 7));
    filter_src_pixels(src, tmp, coeff, offset_bits_horiz, reduce_bits_horiz, k);
  }
}

static INLINE void unpack_weights_and_set_round_const(
    ConvolveParams *conv_params, const int round_bits, const int offset_bits,
    __m128i *res_sub_const, __m128i *round_bits_const, __m128i *wt) {
  *res_sub_const =
      _mm_set1_epi16(-(1 << (offset_bits - conv_params->round_1)) -
                     (1 << (offset_bits - conv_params->round_1 - 1)));
  *round_bits_const = _mm_set1_epi16(((1 << round_bits) >> 1));

  const int w0 = conv_params->fwd_offset;
  const int w1 = conv_params->bck_offset;
  const __m128i wt0 = _mm_set1_epi16((int16_t)w0);
  const __m128i wt1 = _mm_set1_epi16((int16_t)w1);
  *wt = _mm_unpacklo_epi16(wt0, wt1);
}

static INLINE void prepare_vertical_filter_coeffs(int gamma, int sy,
                                                  __m128i *coeffs) {
  const __m128i tmp_0 =
      _mm_loadu_si128((__m128i *)(av1_warped_filter +
                                  ((sy + 0 * gamma) >> WARPEDDIFF_PREC_BITS)));
  const __m128i tmp_2 =
      _mm_loadu_si128((__m128i *)(av1_warped_filter +
                                  ((sy + 2 * gamma) >> WARPEDDIFF_PREC_BITS)));
  const __m128i tmp_4 =
      _mm_loadu_si128((__m128i *)(av1_warped_filter +
                                  ((sy + 4 * gamma) >> WARPEDDIFF_PREC_BITS)));
  const __m128i tmp_6 =
      _mm_loadu_si128((__m128i *)(av1_warped_filter +
                                  ((sy + 6 * gamma) >> WARPEDDIFF_PREC_BITS)));

  const __m128i tmp_8 = _mm_unpacklo_epi32(tmp_0, tmp_2);
  const __m128i tmp_10 = _mm_unpacklo_epi32(tmp_4, tmp_6);
  const __m128i tmp_12 = _mm_unpackhi_epi32(tmp_0, tmp_2);
  const __m128i tmp_14 = _mm_unpackhi_epi32(tmp_4, tmp_6);

  // even coeffs
  coeffs[0] = _mm_unpacklo_epi64(tmp_8, tmp_10);
  coeffs[1] = _mm_unpackhi_epi64(tmp_8, tmp_10);
  coeffs[2] = _mm_unpacklo_epi64(tmp_12, tmp_14);
  coeffs[3] = _mm_unpackhi_epi64(tmp_12, tmp_14);

  const __m128i tmp_1 =
      _mm_loadu_si128((__m128i *)(av1_warped_filter +
                                  ((sy + 1 * gamma) >> WARPEDDIFF_PREC_BITS)));
  const __m128i tmp_3 =
      _mm_loadu_si128((__m128i *)(av1_warped_filter +
                                  ((sy + 3 * gamma) >> WARPEDDIFF_PREC_BITS)));
  const __m128i tmp_5 =
      _mm_loadu_si128((__m128i *)(av1_warped_filter +
                                  ((sy + 5 * gamma) >> WARPEDDIFF_PREC_BITS)));
  const __m128i tmp_7 =
      _mm_loadu_si128((__m128i *)(av1_warped_filter +
                                  ((sy + 7 * gamma) >> WARPEDDIFF_PREC_BITS)));

  const __m128i tmp_9 = _mm_unpacklo_epi32(tmp_1, tmp_3);
  const __m128i tmp_11 = _mm_unpacklo_epi32(tmp_5, tmp_7);
  const __m128i tmp_13 = _mm_unpackhi_epi32(tmp_1, tmp_3);
  const __m128i tmp_15 = _mm_unpackhi_epi32(tmp_5, tmp_7);

  // odd coeffs
  coeffs[4] = _mm_unpacklo_epi64(tmp_9, tmp_11);
  coeffs[5] = _mm_unpackhi_epi64(tmp_9, tmp_11);
  coeffs[6] = _mm_unpacklo_epi64(tmp_13, tmp_15);
  coeffs[7] = _mm_unpackhi_epi64(tmp_13, tmp_15);
}

static INLINE void prepare_vertical_filter_coeffs_gamma0(int sy,
                                                         __m128i *coeffs) {
  const __m128i tmp_0 = _mm_loadu_si128(
      (__m128i *)(av1_warped_filter + (sy >> WARPEDDIFF_PREC_BITS)));

  // even coeffs
  coeffs[0] =
      _mm_shuffle_epi8(tmp_0, _mm_load_si128((__m128i *)shuffle_gamma0_mask0));
  coeffs[1] =
      _mm_shuffle_epi8(tmp_0, _mm_load_si128((__m128i *)shuffle_gamma0_mask1));
  coeffs[2] =
      _mm_shuffle_epi8(tmp_0, _mm_load_si128((__m128i *)shuffle_gamma0_mask2));
  coeffs[3] =
      _mm_shuffle_epi8(tmp_0, _mm_load_si128((__m128i *)shuffle_gamma0_mask3));

  // odd coeffs
  coeffs[4] = coeffs[0];
  coeffs[5] = coeffs[1];
  coeffs[6] = coeffs[2];
  coeffs[7] = coeffs[3];
}

static INLINE void filter_src_pixels_vertical(__m128i *tmp, __m128i *coeffs,
                                              __m128i *res_lo, __m128i *res_hi,
                                              int k) {
  // Load from tmp and rearrange pairs of consecutive rows into the
  // column order 0 0 2 2 4 4 6 6; 1 1 3 3 5 5 7 7
  const __m128i *src = tmp + (k + 4);
  const __m128i src_0 = _mm_unpacklo_epi16(src[0], src[1]);
  const __m128i src_2 = _mm_unpacklo_epi16(src[2], src[3]);
  const __m128i src_4 = _mm_unpacklo_epi16(src[4], src[5]);
  const __m128i src_6 = _mm_unpacklo_epi16(src[6], src[7]);

  const __m128i res_0 = _mm_madd_epi16(src_0, coeffs[0]);
  const __m128i res_2 = _mm_madd_epi16(src_2, coeffs[1]);
  const __m128i res_4 = _mm_madd_epi16(src_4, coeffs[2]);
  const __m128i res_6 = _mm_madd_epi16(src_6, coeffs[3]);

  const __m128i res_even =
      _mm_add_epi32(_mm_add_epi32(res_0, res_2), _mm_add_epi32(res_4, res_6));

  // Filter odd-index pixels
  const __m128i src_1 = _mm_unpackhi_epi16(src[0], src[1]);
  const __m128i src_3 = _mm_unpackhi_epi16(src[2], src[3]);
  const __m128i src_5 = _mm_unpackhi_epi16(src[4], src[5]);
  const __m128i src_7 = _mm_unpackhi_epi16(src[6], src[7]);

  const __m128i res_1 = _mm_madd_epi16(src_1, coeffs[4]);
  const __m128i res_3 = _mm_madd_epi16(src_3, coeffs[5]);
  const __m128i res_5 = _mm_madd_epi16(src_5, coeffs[6]);
  const __m128i res_7 = _mm_madd_epi16(src_7, coeffs[7]);

  const __m128i res_odd =
      _mm_add_epi32(_mm_add_epi32(res_1, res_3), _mm_add_epi32(res_5, res_7));

  // Rearrange pixels back into the order 0 ... 7
  *res_lo = _mm_unpacklo_epi32(res_even, res_odd);
  *res_hi = _mm_unpackhi_epi32(res_even, res_odd);
}

static INLINE void store_vertical_filter_output(
    __m128i *res_lo, __m128i *res_hi, const __m128i *res_add_const,
    const __m128i *wt, const __m128i *res_sub_const, __m128i *round_bits_const,
    uint8_t *pred, ConvolveParams *conv_params, int i, int j, int k,
    const int reduce_bits_vert, int p_stride, int p_width,
    const int round_bits) {
  __m128i res_lo_1 = *res_lo;
  __m128i res_hi_1 = *res_hi;

  if (conv_params->is_compound) {
    __m128i *const p =
        (__m128i *)&conv_params->dst[(i + k + 4) * conv_params->dst_stride + j];
    res_lo_1 = _mm_srai_epi32(_mm_add_epi32(res_lo_1, *res_add_const),
                              reduce_bits_vert);
    const __m128i temp_lo_16 = _mm_packus_epi32(res_lo_1, res_lo_1);
    __m128i res_lo_16;
    if (conv_params->do_average) {
      __m128i *const dst8 = (__m128i *)&pred[(i + k + 4) * p_stride + j];
      const __m128i p_16 = _mm_loadl_epi64(p);

      if (conv_params->use_dist_wtd_comp_avg) {
        const __m128i p_16_lo = _mm_unpacklo_epi16(p_16, temp_lo_16);
        const __m128i wt_res_lo = _mm_madd_epi16(p_16_lo, *wt);
        const __m128i shifted_32 =
            _mm_srai_epi32(wt_res_lo, DIST_PRECISION_BITS);
        res_lo_16 = _mm_packus_epi32(shifted_32, shifted_32);
      } else {
        res_lo_16 = _mm_srai_epi16(_mm_add_epi16(p_16, temp_lo_16), 1);
      }

      res_lo_16 = _mm_add_epi16(res_lo_16, *res_sub_const);

      res_lo_16 = _mm_srai_epi16(_mm_add_epi16(res_lo_16, *round_bits_const),
                                 round_bits);
      __m128i res_8_lo = _mm_packus_epi16(res_lo_16, res_lo_16);
      *(int *)dst8 = _mm_cvtsi128_si32(res_8_lo);
    } else {
      _mm_storel_epi64(p, temp_lo_16);
    }
    if (p_width > 4) {
      __m128i *const p4 =
          (__m128i *)&conv_params
              ->dst[(i + k + 4) * conv_params->dst_stride + j + 4];
      res_hi_1 = _mm_srai_epi32(_mm_add_epi32(res_hi_1, *res_add_const),
                                reduce_bits_vert);
      const __m128i temp_hi_16 = _mm_packus_epi32(res_hi_1, res_hi_1);
      __m128i res_hi_16;

      if (conv_params->do_average) {
        __m128i *const dst8_4 =
            (__m128i *)&pred[(i + k + 4) * p_stride + j + 4];
        const __m128i p4_16 = _mm_loadl_epi64(p4);

        if (conv_params->use_dist_wtd_comp_avg) {
          const __m128i p_16_hi = _mm_unpacklo_epi16(p4_16, temp_hi_16);
          const __m128i wt_res_hi = _mm_madd_epi16(p_16_hi, *wt);
          const __m128i shifted_32 =
              _mm_srai_epi32(wt_res_hi, DIST_PRECISION_BITS);
          res_hi_16 = _mm_packus_epi32(shifted_32, shifted_32);
        } else {
          res_hi_16 = _mm_srai_epi16(_mm_add_epi16(p4_16, temp_hi_16), 1);
        }
        res_hi_16 = _mm_add_epi16(res_hi_16, *res_sub_const);

        res_hi_16 = _mm_srai_epi16(_mm_add_epi16(res_hi_16, *round_bits_const),
                                   round_bits);
        __m128i res_8_hi = _mm_packus_epi16(res_hi_16, res_hi_16);
        *(int *)dst8_4 = _mm_cvtsi128_si32(res_8_hi);

      } else {
        _mm_storel_epi64(p4, temp_hi_16);
      }
    }
  } else {
    const __m128i res_lo_round = _mm_srai_epi32(
        _mm_add_epi32(res_lo_1, *res_add_const), reduce_bits_vert);
    const __m128i res_hi_round = _mm_srai_epi32(
        _mm_add_epi32(res_hi_1, *res_add_const), reduce_bits_vert);

    const __m128i res_16bit = _mm_packs_epi32(res_lo_round, res_hi_round);
    __m128i res_8bit = _mm_packus_epi16(res_16bit, res_16bit);

    // Store, blending with 'pred' if needed
    __m128i *const p = (__m128i *)&pred[(i + k + 4) * p_stride + j];

    // Note: If we're outputting a 4x4 block, we need to be very careful
    // to only output 4 pixels at this point, to avoid encode/decode
    // mismatches when encoding with multiple threads.
    if (p_width == 4) {
      *(int *)p = _mm_cvtsi128_si32(res_8bit);
    } else {
      _mm_storel_epi64(p, res_8bit);
    }
  }
}

static INLINE void warp_vertical_filter(
    uint8_t *pred, __m128i *tmp, ConvolveParams *conv_params, int16_t gamma,
    int16_t delta, int p_height, int p_stride, int p_width, int i, int j,
    int sy4, const int reduce_bits_vert, const __m128i *res_add_const,
    const int round_bits, const int offset_bits) {
  int k;
  __m128i res_sub_const, round_bits_const, wt;
  unpack_weights_and_set_round_const(conv_params, round_bits, offset_bits,
                                     &res_sub_const, &round_bits_const, &wt);
  // Vertical filter
  for (k = -4; k < AOMMIN(4, p_height - i - 4); ++k) {
    int sy = sy4 + delta * (k + 4);

    __m128i coeffs[8];
    prepare_vertical_filter_coeffs(gamma, sy, coeffs);

    __m128i res_lo;
    __m128i res_hi;
    filter_src_pixels_vertical(tmp, coeffs, &res_lo, &res_hi, k);

    store_vertical_filter_output(&res_lo, &res_hi, res_add_const, &wt,
                                 &res_sub_const, &round_bits_const, pred,
                                 conv_params, i, j, k, reduce_bits_vert,
                                 p_stride, p_width, round_bits);
  }
}

static INLINE void warp_vertical_filter_gamma0(
    uint8_t *pred, __m128i *tmp, ConvolveParams *conv_params, int16_t gamma,
    int16_t delta, int p_height, int p_stride, int p_width, int i, int j,
    int sy4, const int reduce_bits_vert, const __m128i *res_add_const,
    const int round_bits, const int offset_bits) {
  int k;
  (void)gamma;
  __m128i res_sub_const, round_bits_const, wt;
  unpack_weights_and_set_round_const(conv_params, round_bits, offset_bits,
                                     &res_sub_const, &round_bits_const, &wt);
  // Vertical filter
  for (k = -4; k < AOMMIN(4, p_height - i - 4); ++k) {
    int sy = sy4 + delta * (k + 4);

    __m128i coeffs[8];
    prepare_vertical_filter_coeffs_gamma0(sy, coeffs);

    __m128i res_lo;
    __m128i res_hi;
    filter_src_pixels_vertical(tmp, coeffs, &res_lo, &res_hi, k);

    store_vertical_filter_output(&res_lo, &res_hi, res_add_const, &wt,
                                 &res_sub_const, &round_bits_const, pred,
                                 conv_params, i, j, k, reduce_bits_vert,
                                 p_stride, p_width, round_bits);
  }
}

static INLINE void warp_vertical_filter_delta0(
    uint8_t *pred, __m128i *tmp, ConvolveParams *conv_params, int16_t gamma,
    int16_t delta, int p_height, int p_stride, int p_width, int i, int j,
    int sy4, const int reduce_bits_vert, const __m128i *res_add_const,
    const int round_bits, const int offset_bits) {
  (void)delta;
  int k;
  __m128i res_sub_const, round_bits_const, wt;
  unpack_weights_and_set_round_const(conv_params, round_bits, offset_bits,
                                     &res_sub_const, &round_bits_const, &wt);

  __m128i coeffs[8];
  prepare_vertical_filter_coeffs(gamma, sy4, coeffs);
  // Vertical filter
  for (k = -4; k < AOMMIN(4, p_height - i - 4); ++k) {
    __m128i res_lo;
    __m128i res_hi;
    filter_src_pixels_vertical(tmp, coeffs, &res_lo, &res_hi, k);

    store_vertical_filter_output(&res_lo, &res_hi, res_add_const, &wt,
                                 &res_sub_const, &round_bits_const, pred,
                                 conv_params, i, j, k, reduce_bits_vert,
                                 p_stride, p_width, round_bits);
  }
}

static INLINE void warp_vertical_filter_gamma0_delta0(
    uint8_t *pred, __m128i *tmp, ConvolveParams *conv_params, int16_t gamma,
    int16_t delta, int p_height, int p_stride, int p_width, int i, int j,
    int sy4, const int reduce_bits_vert, const __m128i *res_add_const,
    const int round_bits, const int offset_bits) {
  (void)delta;
  (void)gamma;
  int k;
  __m128i res_sub_const, round_bits_const, wt;
  unpack_weights_and_set_round_const(conv_params, round_bits, offset_bits,
                                     &res_sub_const, &round_bits_const, &wt);

  __m128i coeffs[8];
  prepare_vertical_filter_coeffs_gamma0(sy4, coeffs);
  // Vertical filter
  for (k = -4; k < AOMMIN(4, p_height - i - 4); ++k) {
    __m128i res_lo;
    __m128i res_hi;
    filter_src_pixels_vertical(tmp, coeffs, &res_lo, &res_hi, k);

    store_vertical_filter_output(&res_lo, &res_hi, res_add_const, &wt,
                                 &res_sub_const, &round_bits_const, pred,
                                 conv_params, i, j, k, reduce_bits_vert,
                                 p_stride, p_width, round_bits);
  }
}

static INLINE void prepare_warp_vertical_filter(
    uint8_t *pred, __m128i *tmp, ConvolveParams *conv_params, int16_t gamma,
    int16_t delta, int p_height, int p_stride, int p_width, int i, int j,
    int sy4, const int reduce_bits_vert, const __m128i *res_add_const,
    const int round_bits, const int offset_bits) {
  if (gamma == 0 && delta == 0)
    warp_vertical_filter_gamma0_delta0(
        pred, tmp, conv_params, gamma, delta, p_height, p_stride, p_width, i, j,
        sy4, reduce_bits_vert, res_add_const, round_bits, offset_bits);
  else if (gamma == 0 && delta != 0)
    warp_vertical_filter_gamma0(pred, tmp, conv_params, gamma, delta, p_height,
                                p_stride, p_width, i, j, sy4, reduce_bits_vert,
                                res_add_const, round_bits, offset_bits);
  else if (gamma != 0 && delta == 0)
    warp_vertical_filter_delta0(pred, tmp, conv_params, gamma, delta, p_height,
                                p_stride, p_width, i, j, sy4, reduce_bits_vert,
                                res_add_const, round_bits, offset_bits);
  else
    warp_vertical_filter(pred, tmp, conv_params, gamma, delta, p_height,
                         p_stride, p_width, i, j, sy4, reduce_bits_vert,
                         res_add_const, round_bits, offset_bits);
}

static INLINE void prepare_warp_horizontal_filter(
    const uint8_t *ref, __m128i *tmp, int stride, int32_t ix4, int32_t iy4,
    int32_t sx4, int alpha, int beta, int p_height, int height, int i,
    const int offset_bits_horiz, const int reduce_bits_horiz) {
  if (alpha == 0 && beta == 0)
    warp_horizontal_filter_alpha0_beta0(ref, tmp, stride, ix4, iy4, sx4, alpha,
                                        beta, p_height, height, i,
                                        offset_bits_horiz, reduce_bits_horiz);
  else if (alpha == 0 && beta != 0)
    warp_horizontal_filter_alpha0(ref, tmp, stride, ix4, iy4, sx4, alpha, beta,
                                  p_height, height, i, offset_bits_horiz,
                                  reduce_bits_horiz);
  else if (alpha != 0 && beta == 0)
    warp_horizontal_filter_beta0(ref, tmp, stride, ix4, iy4, sx4, alpha, beta,
                                 p_height, height, i, offset_bits_horiz,
                                 reduce_bits_horiz);
  else
    warp_horizontal_filter(ref, tmp, stride, ix4, iy4, sx4, alpha, beta,
                           p_height, height, i, offset_bits_horiz,
                           reduce_bits_horiz);
}

void av1_warp_affine_sse4_1(const int32_t *mat, const uint8_t *ref, int width,
                            int height, int stride, uint8_t *pred, int p_col,
                            int p_row, int p_width, int p_height, int p_stride,
                            int subsampling_x, int subsampling_y,
                            ConvolveParams *conv_params, int16_t alpha,
                            int16_t beta, int16_t gamma, int16_t delta) {
  __m128i tmp[15];
  int i, j, k;
  const int bd = 8;
  const int reduce_bits_horiz = conv_params->round_0;
  const int reduce_bits_vert = conv_params->is_compound
                                   ? conv_params->round_1
                                   : 2 * FILTER_BITS - reduce_bits_horiz;
  const int offset_bits_horiz = bd + FILTER_BITS - 1;
  assert(IMPLIES(conv_params->is_compound, conv_params->dst != NULL));

  const int offset_bits_vert = bd + 2 * FILTER_BITS - reduce_bits_horiz;
  const __m128i reduce_bits_vert_const =
      _mm_set1_epi32(((1 << reduce_bits_vert) >> 1));
  const __m128i res_add_const = _mm_set1_epi32(1 << offset_bits_vert);
  const int round_bits =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  assert(IMPLIES(conv_params->do_average, conv_params->is_compound));

  /* Note: For this code to work, the left/right frame borders need to be
  extended by at least 13 pixels each. By the time we get here, other
  code will have set up this border, but we allow an explicit check
  for debugging purposes.
  */
  /*for (i = 0; i < height; ++i) {
  for (j = 0; j < 13; ++j) {
  assert(ref[i * stride - 13 + j] == ref[i * stride]);
  assert(ref[i * stride + width + j] == ref[i * stride + (width - 1)]);
  }
  }*/
  __m128i res_add_const_1;
  if (conv_params->is_compound == 1) {
    res_add_const_1 = _mm_add_epi32(reduce_bits_vert_const, res_add_const);
  } else {
    res_add_const_1 = _mm_set1_epi32(-(1 << (bd + reduce_bits_vert - 1)) +
                                     ((1 << reduce_bits_vert) >> 1));
  }

  for (i = 0; i < p_height; i += 8) {
    for (j = 0; j < p_width; j += 8) {
      const int32_t src_x = (p_col + j + 4) << subsampling_x;
      const int32_t src_y = (p_row + i + 4) << subsampling_y;
      const int64_t dst_x =
          (int64_t)mat[2] * src_x + (int64_t)mat[3] * src_y + (int64_t)mat[0];
      const int64_t dst_y =
          (int64_t)mat[4] * src_x + (int64_t)mat[5] * src_y + (int64_t)mat[1];
      const int64_t x4 = dst_x >> subsampling_x;
      const int64_t y4 = dst_y >> subsampling_y;

      int32_t ix4 = (int32_t)(x4 >> WARPEDMODEL_PREC_BITS);
      int32_t sx4 = x4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);
      int32_t iy4 = (int32_t)(y4 >> WARPEDMODEL_PREC_BITS);
      int32_t sy4 = y4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);

      // Add in all the constant terms, including rounding and offset
      sx4 += alpha * (-4) + beta * (-4) + (1 << (WARPEDDIFF_PREC_BITS - 1)) +
             (WARPEDPIXEL_PREC_SHIFTS << WARPEDDIFF_PREC_BITS);
      sy4 += gamma * (-4) + delta * (-4) + (1 << (WARPEDDIFF_PREC_BITS - 1)) +
             (WARPEDPIXEL_PREC_SHIFTS << WARPEDDIFF_PREC_BITS);

      sx4 &= ~((1 << WARP_PARAM_REDUCE_BITS) - 1);
      sy4 &= ~((1 << WARP_PARAM_REDUCE_BITS) - 1);

      // Horizontal filter
      // If the block is aligned such that, after clamping, every sample
      // would be taken from the leftmost/rightmost column, then we can
      // skip the expensive horizontal filter.
      if (ix4 <= -7) {
        for (k = -7; k < AOMMIN(8, p_height - i); ++k) {
          int iy = iy4 + k;
          if (iy < 0)
            iy = 0;
          else if (iy > height - 1)
            iy = height - 1;
          tmp[k + 7] = _mm_set1_epi16(
              (1 << (bd + FILTER_BITS - reduce_bits_horiz - 1)) +
              ref[iy * stride] * (1 << (FILTER_BITS - reduce_bits_horiz)));
        }
      } else if (ix4 >= width + 6) {
        for (k = -7; k < AOMMIN(8, p_height - i); ++k) {
          int iy = iy4 + k;
          if (iy < 0)
            iy = 0;
          else if (iy > height - 1)
            iy = height - 1;
          tmp[k + 7] =
              _mm_set1_epi16((1 << (bd + FILTER_BITS - reduce_bits_horiz - 1)) +
                             ref[iy * stride + (width - 1)] *
                                 (1 << (FILTER_BITS - reduce_bits_horiz)));
        }
      } else if (((ix4 - 7) < 0) || ((ix4 + 9) > width)) {
        const int out_of_boundary_left = -(ix4 - 6);
        const int out_of_boundary_right = (ix4 + 8) - width;
        for (k = -7; k < AOMMIN(8, p_height - i); ++k) {
          int iy = iy4 + k;
          if (iy < 0)
            iy = 0;
          else if (iy > height - 1)
            iy = height - 1;
          int sx = sx4 + beta * (k + 4);

          // Load source pixels
          __m128i src =
              _mm_loadu_si128((__m128i *)(ref + iy * stride + ix4 - 7));
          if (out_of_boundary_left >= 0) {
            const __m128i shuffle_reg_left =
                _mm_loadu_si128((__m128i *)warp_pad_left[out_of_boundary_left]);
            src = _mm_shuffle_epi8(src, shuffle_reg_left);
          }
          if (out_of_boundary_right >= 0) {
            const __m128i shuffle_reg_right = _mm_loadu_si128(
                (__m128i *)warp_pad_right[out_of_boundary_right]);
            src = _mm_shuffle_epi8(src, shuffle_reg_right);
          }
          horizontal_filter(src, tmp, sx, alpha, k, offset_bits_horiz,
                            reduce_bits_horiz);
        }
      } else {
        prepare_warp_horizontal_filter(ref, tmp, stride, ix4, iy4, sx4, alpha,
                                       beta, p_height, height, i,
                                       offset_bits_horiz, reduce_bits_horiz);
      }

      // Vertical filter
      prepare_warp_vertical_filter(
          pred, tmp, conv_params, gamma, delta, p_height, p_stride, p_width, i,
          j, sy4, reduce_bits_vert, &res_add_const_1, round_bits, offset_bits);
    }
  }
}
