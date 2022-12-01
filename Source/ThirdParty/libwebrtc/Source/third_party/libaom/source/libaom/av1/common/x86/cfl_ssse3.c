/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <tmmintrin.h>

#include "config/av1_rtcd.h"

#include "av1/common/cfl.h"

#include "av1/common/x86/cfl_simd.h"

// Load 32-bit integer from memory into the first element of dst.
static INLINE __m128i _mm_loadh_epi32(__m128i const *mem_addr) {
  return _mm_cvtsi32_si128(*((int *)mem_addr));
}

// Store 32-bit integer from the first element of a into memory.
static INLINE void _mm_storeh_epi32(__m128i const *mem_addr, __m128i a) {
  *((int *)mem_addr) = _mm_cvtsi128_si32(a);
}

/**
 * Adds 4 pixels (in a 2x2 grid) and multiplies them by 2. Resulting in a more
 * precise version of a box filter 4:2:0 pixel subsampling in Q3.
 *
 * The CfL prediction buffer is always of size CFL_BUF_SQUARE. However, the
 * active area is specified using width and height.
 *
 * Note: We don't need to worry about going over the active area, as long as we
 * stay inside the CfL prediction buffer.
 */
static INLINE void cfl_luma_subsampling_420_lbd_ssse3(const uint8_t *input,
                                                      int input_stride,
                                                      uint16_t *pred_buf_q3,
                                                      int width, int height) {
  const __m128i twos = _mm_set1_epi8(2);
  __m128i *pred_buf_m128i = (__m128i *)pred_buf_q3;
  const __m128i *end = pred_buf_m128i + (height >> 1) * CFL_BUF_LINE_I128;
  const int luma_stride = input_stride << 1;
  do {
    if (width == 4) {
      __m128i top = _mm_loadh_epi32((__m128i *)input);
      top = _mm_maddubs_epi16(top, twos);
      __m128i bot = _mm_loadh_epi32((__m128i *)(input + input_stride));
      bot = _mm_maddubs_epi16(bot, twos);
      const __m128i sum = _mm_add_epi16(top, bot);
      _mm_storeh_epi32(pred_buf_m128i, sum);
    } else if (width == 8) {
      __m128i top = _mm_loadl_epi64((__m128i *)input);
      top = _mm_maddubs_epi16(top, twos);
      __m128i bot = _mm_loadl_epi64((__m128i *)(input + input_stride));
      bot = _mm_maddubs_epi16(bot, twos);
      const __m128i sum = _mm_add_epi16(top, bot);
      _mm_storel_epi64(pred_buf_m128i, sum);
    } else {
      __m128i top = _mm_loadu_si128((__m128i *)input);
      top = _mm_maddubs_epi16(top, twos);
      __m128i bot = _mm_loadu_si128((__m128i *)(input + input_stride));
      bot = _mm_maddubs_epi16(bot, twos);
      const __m128i sum = _mm_add_epi16(top, bot);
      _mm_storeu_si128(pred_buf_m128i, sum);
      if (width == 32) {
        __m128i top_1 = _mm_loadu_si128(((__m128i *)input) + 1);
        __m128i bot_1 =
            _mm_loadu_si128(((__m128i *)(input + input_stride)) + 1);
        top_1 = _mm_maddubs_epi16(top_1, twos);
        bot_1 = _mm_maddubs_epi16(bot_1, twos);
        __m128i sum_1 = _mm_add_epi16(top_1, bot_1);
        _mm_storeu_si128(pred_buf_m128i + 1, sum_1);
      }
    }
    input += luma_stride;
    pred_buf_m128i += CFL_BUF_LINE_I128;
  } while (pred_buf_m128i < end);
}

/**
 * Adds 2 pixels (in a 2x1 grid) and multiplies them by 4. Resulting in a more
 * precise version of a box filter 4:2:2 pixel subsampling in Q3.
 *
 * The CfL prediction buffer is always of size CFL_BUF_SQUARE. However, the
 * active area is specified using width and height.
 *
 * Note: We don't need to worry about going over the active area, as long as we
 * stay inside the CfL prediction buffer.
 */
static INLINE void cfl_luma_subsampling_422_lbd_ssse3(const uint8_t *input,
                                                      int input_stride,
                                                      uint16_t *pred_buf_q3,
                                                      int width, int height) {
  const __m128i fours = _mm_set1_epi8(4);
  __m128i *pred_buf_m128i = (__m128i *)pred_buf_q3;
  const __m128i *end = pred_buf_m128i + height * CFL_BUF_LINE_I128;
  do {
    if (width == 4) {
      __m128i top = _mm_loadh_epi32((__m128i *)input);
      top = _mm_maddubs_epi16(top, fours);
      _mm_storeh_epi32(pred_buf_m128i, top);
    } else if (width == 8) {
      __m128i top = _mm_loadl_epi64((__m128i *)input);
      top = _mm_maddubs_epi16(top, fours);
      _mm_storel_epi64(pred_buf_m128i, top);
    } else {
      __m128i top = _mm_loadu_si128((__m128i *)input);
      top = _mm_maddubs_epi16(top, fours);
      _mm_storeu_si128(pred_buf_m128i, top);
      if (width == 32) {
        __m128i top_1 = _mm_loadu_si128(((__m128i *)input) + 1);
        top_1 = _mm_maddubs_epi16(top_1, fours);
        _mm_storeu_si128(pred_buf_m128i + 1, top_1);
      }
    }
    input += input_stride;
    pred_buf_m128i += CFL_BUF_LINE_I128;
  } while (pred_buf_m128i < end);
}

/**
 * Multiplies the pixels by 8 (scaling in Q3).
 *
 * The CfL prediction buffer is always of size CFL_BUF_SQUARE. However, the
 * active area is specified using width and height.
 *
 * Note: We don't need to worry about going over the active area, as long as we
 * stay inside the CfL prediction buffer.
 */
static INLINE void cfl_luma_subsampling_444_lbd_ssse3(const uint8_t *input,
                                                      int input_stride,
                                                      uint16_t *pred_buf_q3,
                                                      int width, int height) {
  const __m128i zeros = _mm_setzero_si128();
  const int luma_stride = input_stride;
  __m128i *pred_buf_m128i = (__m128i *)pred_buf_q3;
  const __m128i *end = pred_buf_m128i + height * CFL_BUF_LINE_I128;
  do {
    if (width == 4) {
      __m128i row = _mm_loadh_epi32((__m128i *)input);
      row = _mm_unpacklo_epi8(row, zeros);
      _mm_storel_epi64(pred_buf_m128i, _mm_slli_epi16(row, 3));
    } else if (width == 8) {
      __m128i row = _mm_loadl_epi64((__m128i *)input);
      row = _mm_unpacklo_epi8(row, zeros);
      _mm_storeu_si128(pred_buf_m128i, _mm_slli_epi16(row, 3));
    } else {
      __m128i row = _mm_loadu_si128((__m128i *)input);
      const __m128i row_lo = _mm_unpacklo_epi8(row, zeros);
      const __m128i row_hi = _mm_unpackhi_epi8(row, zeros);
      _mm_storeu_si128(pred_buf_m128i, _mm_slli_epi16(row_lo, 3));
      _mm_storeu_si128(pred_buf_m128i + 1, _mm_slli_epi16(row_hi, 3));
      if (width == 32) {
        __m128i row_1 = _mm_loadu_si128(((__m128i *)input) + 1);
        const __m128i row_1_lo = _mm_unpacklo_epi8(row_1, zeros);
        const __m128i row_1_hi = _mm_unpackhi_epi8(row_1, zeros);
        _mm_storeu_si128(pred_buf_m128i + 2, _mm_slli_epi16(row_1_lo, 3));
        _mm_storeu_si128(pred_buf_m128i + 3, _mm_slli_epi16(row_1_hi, 3));
      }
    }
    input += luma_stride;
    pred_buf_m128i += CFL_BUF_LINE_I128;
  } while (pred_buf_m128i < end);
}

#if CONFIG_AV1_HIGHBITDEPTH
/**
 * Adds 4 pixels (in a 2x2 grid) and multiplies them by 2. Resulting in a more
 * precise version of a box filter 4:2:0 pixel subsampling in Q3.
 *
 * The CfL prediction buffer is always of size CFL_BUF_SQUARE. However, the
 * active area is specified using width and height.
 *
 * Note: We don't need to worry about going over the active area, as long as we
 * stay inside the CfL prediction buffer.
 */
static INLINE void cfl_luma_subsampling_420_hbd_ssse3(const uint16_t *input,
                                                      int input_stride,
                                                      uint16_t *pred_buf_q3,
                                                      int width, int height) {
  const uint16_t *end = pred_buf_q3 + (height >> 1) * CFL_BUF_LINE;
  const int luma_stride = input_stride << 1;
  do {
    if (width == 4) {
      const __m128i top = _mm_loadl_epi64((__m128i *)input);
      const __m128i bot = _mm_loadl_epi64((__m128i *)(input + input_stride));
      __m128i sum = _mm_add_epi16(top, bot);
      sum = _mm_hadd_epi16(sum, sum);
      *((int *)pred_buf_q3) = _mm_cvtsi128_si32(_mm_add_epi16(sum, sum));
    } else {
      const __m128i top = _mm_loadu_si128((__m128i *)input);
      const __m128i bot = _mm_loadu_si128((__m128i *)(input + input_stride));
      __m128i sum = _mm_add_epi16(top, bot);
      if (width == 8) {
        sum = _mm_hadd_epi16(sum, sum);
        _mm_storel_epi64((__m128i *)pred_buf_q3, _mm_add_epi16(sum, sum));
      } else {
        const __m128i top_1 = _mm_loadu_si128(((__m128i *)input) + 1);
        const __m128i bot_1 =
            _mm_loadu_si128(((__m128i *)(input + input_stride)) + 1);
        sum = _mm_hadd_epi16(sum, _mm_add_epi16(top_1, bot_1));
        _mm_storeu_si128((__m128i *)pred_buf_q3, _mm_add_epi16(sum, sum));
        if (width == 32) {
          const __m128i top_2 = _mm_loadu_si128(((__m128i *)input) + 2);
          const __m128i bot_2 =
              _mm_loadu_si128(((__m128i *)(input + input_stride)) + 2);
          const __m128i top_3 = _mm_loadu_si128(((__m128i *)input) + 3);
          const __m128i bot_3 =
              _mm_loadu_si128(((__m128i *)(input + input_stride)) + 3);
          const __m128i sum_2 = _mm_add_epi16(top_2, bot_2);
          const __m128i sum_3 = _mm_add_epi16(top_3, bot_3);
          __m128i next_sum = _mm_hadd_epi16(sum_2, sum_3);
          _mm_storeu_si128(((__m128i *)pred_buf_q3) + 1,
                           _mm_add_epi16(next_sum, next_sum));
        }
      }
    }
    input += luma_stride;
  } while ((pred_buf_q3 += CFL_BUF_LINE) < end);
}

/**
 * Adds 2 pixels (in a 2x1 grid) and multiplies them by 4. Resulting in a more
 * precise version of a box filter 4:2:2 pixel subsampling in Q3.
 *
 * The CfL prediction buffer is always of size CFL_BUF_SQUARE. However, the
 * active area is specified using width and height.
 *
 * Note: We don't need to worry about going over the active area, as long as we
 * stay inside the CfL prediction buffer.
 */
static INLINE void cfl_luma_subsampling_422_hbd_ssse3(const uint16_t *input,
                                                      int input_stride,
                                                      uint16_t *pred_buf_q3,
                                                      int width, int height) {
  __m128i *pred_buf_m128i = (__m128i *)pred_buf_q3;
  const __m128i *end = pred_buf_m128i + height * CFL_BUF_LINE_I128;
  do {
    if (width == 4) {
      const __m128i top = _mm_loadl_epi64((__m128i *)input);
      const __m128i sum = _mm_slli_epi16(_mm_hadd_epi16(top, top), 2);
      _mm_storeh_epi32(pred_buf_m128i, sum);
    } else {
      const __m128i top = _mm_loadu_si128((__m128i *)input);
      if (width == 8) {
        const __m128i sum = _mm_slli_epi16(_mm_hadd_epi16(top, top), 2);
        _mm_storel_epi64(pred_buf_m128i, sum);
      } else {
        const __m128i top_1 = _mm_loadu_si128(((__m128i *)input) + 1);
        const __m128i sum = _mm_slli_epi16(_mm_hadd_epi16(top, top_1), 2);
        _mm_storeu_si128(pred_buf_m128i, sum);
        if (width == 32) {
          const __m128i top_2 = _mm_loadu_si128(((__m128i *)input) + 2);
          const __m128i top_3 = _mm_loadu_si128(((__m128i *)input) + 3);
          const __m128i sum_1 = _mm_slli_epi16(_mm_hadd_epi16(top_2, top_3), 2);
          _mm_storeu_si128(pred_buf_m128i + 1, sum_1);
        }
      }
    }
    pred_buf_m128i += CFL_BUF_LINE_I128;
    input += input_stride;
  } while (pred_buf_m128i < end);
}

static INLINE void cfl_luma_subsampling_444_hbd_ssse3(const uint16_t *input,
                                                      int input_stride,
                                                      uint16_t *pred_buf_q3,
                                                      int width, int height) {
  const uint16_t *end = pred_buf_q3 + height * CFL_BUF_LINE;
  do {
    if (width == 4) {
      const __m128i row = _mm_slli_epi16(_mm_loadl_epi64((__m128i *)input), 3);
      _mm_storel_epi64((__m128i *)pred_buf_q3, row);
    } else {
      const __m128i row = _mm_slli_epi16(_mm_loadu_si128((__m128i *)input), 3);
      _mm_storeu_si128((__m128i *)pred_buf_q3, row);
      if (width >= 16) {
        __m128i row_1 = _mm_loadu_si128(((__m128i *)input) + 1);
        row_1 = _mm_slli_epi16(row_1, 3);
        _mm_storeu_si128(((__m128i *)pred_buf_q3) + 1, row_1);
        if (width == 32) {
          __m128i row_2 = _mm_loadu_si128(((__m128i *)input) + 2);
          row_2 = _mm_slli_epi16(row_2, 3);
          _mm_storeu_si128(((__m128i *)pred_buf_q3) + 2, row_2);
          __m128i row_3 = _mm_loadu_si128(((__m128i *)input) + 3);
          row_3 = _mm_slli_epi16(row_3, 3);
          _mm_storeu_si128(((__m128i *)pred_buf_q3) + 3, row_3);
        }
      }
    }
    input += input_stride;
    pred_buf_q3 += CFL_BUF_LINE;
  } while (pred_buf_q3 < end);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

CFL_GET_SUBSAMPLE_FUNCTION(ssse3)

static INLINE __m128i predict_unclipped(const __m128i *input, __m128i alpha_q12,
                                        __m128i alpha_sign, __m128i dc_q0) {
  __m128i ac_q3 = _mm_loadu_si128(input);
  __m128i ac_sign = _mm_sign_epi16(alpha_sign, ac_q3);
  __m128i scaled_luma_q0 = _mm_mulhrs_epi16(_mm_abs_epi16(ac_q3), alpha_q12);
  scaled_luma_q0 = _mm_sign_epi16(scaled_luma_q0, ac_sign);
  return _mm_add_epi16(scaled_luma_q0, dc_q0);
}

static INLINE void cfl_predict_lbd_ssse3(const int16_t *pred_buf_q3,
                                         uint8_t *dst, int dst_stride,
                                         int alpha_q3, int width, int height) {
  const __m128i alpha_sign = _mm_set1_epi16(alpha_q3);
  const __m128i alpha_q12 = _mm_slli_epi16(_mm_abs_epi16(alpha_sign), 9);
  const __m128i dc_q0 = _mm_set1_epi16(*dst);
  __m128i *row = (__m128i *)pred_buf_q3;
  const __m128i *row_end = row + height * CFL_BUF_LINE_I128;
  do {
    __m128i res = predict_unclipped(row, alpha_q12, alpha_sign, dc_q0);
    if (width < 16) {
      res = _mm_packus_epi16(res, res);
      if (width == 4)
        _mm_storeh_epi32((__m128i *)dst, res);
      else
        _mm_storel_epi64((__m128i *)dst, res);
    } else {
      __m128i next = predict_unclipped(row + 1, alpha_q12, alpha_sign, dc_q0);
      res = _mm_packus_epi16(res, next);
      _mm_storeu_si128((__m128i *)dst, res);
      if (width == 32) {
        res = predict_unclipped(row + 2, alpha_q12, alpha_sign, dc_q0);
        next = predict_unclipped(row + 3, alpha_q12, alpha_sign, dc_q0);
        res = _mm_packus_epi16(res, next);
        _mm_storeu_si128((__m128i *)(dst + 16), res);
      }
    }
    dst += dst_stride;
  } while ((row += CFL_BUF_LINE_I128) < row_end);
}

CFL_PREDICT_FN(ssse3, lbd)

#if CONFIG_AV1_HIGHBITDEPTH
static INLINE __m128i highbd_max_epi16(int bd) {
  const __m128i neg_one = _mm_set1_epi16(-1);
  // (1 << bd) - 1 => -(-1 << bd) -1 => -1 - (-1 << bd) => -1 ^ (-1 << bd)
  return _mm_xor_si128(_mm_slli_epi16(neg_one, bd), neg_one);
}

static INLINE __m128i highbd_clamp_epi16(__m128i u, __m128i zero, __m128i max) {
  return _mm_max_epi16(_mm_min_epi16(u, max), zero);
}

static INLINE void cfl_predict_hbd_ssse3(const int16_t *pred_buf_q3,
                                         uint16_t *dst, int dst_stride,
                                         int alpha_q3, int bd, int width,
                                         int height) {
  const __m128i alpha_sign = _mm_set1_epi16(alpha_q3);
  const __m128i alpha_q12 = _mm_slli_epi16(_mm_abs_epi16(alpha_sign), 9);
  const __m128i dc_q0 = _mm_set1_epi16(*dst);
  const __m128i max = highbd_max_epi16(bd);
  const __m128i zeros = _mm_setzero_si128();
  __m128i *row = (__m128i *)pred_buf_q3;
  const __m128i *row_end = row + height * CFL_BUF_LINE_I128;
  do {
    __m128i res = predict_unclipped(row, alpha_q12, alpha_sign, dc_q0);
    res = highbd_clamp_epi16(res, zeros, max);
    if (width == 4) {
      _mm_storel_epi64((__m128i *)dst, res);
    } else {
      _mm_storeu_si128((__m128i *)dst, res);
    }
    if (width >= 16) {
      const __m128i res_1 =
          predict_unclipped(row + 1, alpha_q12, alpha_sign, dc_q0);
      _mm_storeu_si128(((__m128i *)dst) + 1,
                       highbd_clamp_epi16(res_1, zeros, max));
    }
    if (width == 32) {
      const __m128i res_2 =
          predict_unclipped(row + 2, alpha_q12, alpha_sign, dc_q0);
      _mm_storeu_si128((__m128i *)(dst + 16),
                       highbd_clamp_epi16(res_2, zeros, max));
      const __m128i res_3 =
          predict_unclipped(row + 3, alpha_q12, alpha_sign, dc_q0);
      _mm_storeu_si128((__m128i *)(dst + 24),
                       highbd_clamp_epi16(res_3, zeros, max));
    }
    dst += dst_stride;
  } while ((row += CFL_BUF_LINE_I128) < row_end);
}

CFL_PREDICT_FN(ssse3, hbd)
#endif  // CONFIG_AV1_HIGHBITDEPTH
