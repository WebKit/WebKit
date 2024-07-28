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

#include <assert.h>
#include <immintrin.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_ports/mem.h"
#include "aom/aom_integer.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "aom_dsp/x86/obmc_intrinsic_sse4.h"

////////////////////////////////////////////////////////////////////////////////
// 8 bit
////////////////////////////////////////////////////////////////////////////////

static INLINE void obmc_variance_w8n(const uint8_t *pre, const int pre_stride,
                                     const int32_t *wsrc, const int32_t *mask,
                                     unsigned int *const sse, int *const sum,
                                     const int w, const int h) {
  int n = 0, width, height = h;
  __m128i v_sum_d = _mm_setzero_si128();
  __m128i v_sse_d = _mm_setzero_si128();
  const __m256i v_bias_d = _mm256_set1_epi32((1 << 12) >> 1);
  __m128i v_d;
  const uint8_t *pre_temp;
  assert(w >= 8);
  assert(IS_POWER_OF_TWO(w));
  assert(IS_POWER_OF_TWO(h));
  do {
    width = w;
    pre_temp = pre;
    do {
      const __m128i v_p_b = _mm_loadl_epi64((const __m128i *)pre_temp);
      const __m256i v_m_d = _mm256_loadu_si256((__m256i const *)(mask + n));
      const __m256i v_w_d = _mm256_loadu_si256((__m256i const *)(wsrc + n));
      const __m256i v_p0_d = _mm256_cvtepu8_epi32(v_p_b);

      // Values in both pre and mask fit in 15 bits, and are packed at 32 bit
      // boundaries. We use pmaddwd, as it has lower latency on Haswell
      // than pmulld but produces the same result with these inputs.
      const __m256i v_pm_d = _mm256_madd_epi16(v_p0_d, v_m_d);
      const __m256i v_diff0_d = _mm256_sub_epi32(v_w_d, v_pm_d);

      const __m256i v_sign_d = _mm256_srai_epi32(v_diff0_d, 31);
      const __m256i v_tmp_d =
          _mm256_add_epi32(_mm256_add_epi32(v_diff0_d, v_bias_d), v_sign_d);
      const __m256i v_rdiff0_d = _mm256_srai_epi32(v_tmp_d, 12);
      const __m128i v_rdiff_d = _mm256_castsi256_si128(v_rdiff0_d);
      const __m128i v_rdiff1_d = _mm256_extracti128_si256(v_rdiff0_d, 1);

      const __m128i v_rdiff01_w = _mm_packs_epi32(v_rdiff_d, v_rdiff1_d);
      const __m128i v_sqrdiff_d = _mm_madd_epi16(v_rdiff01_w, v_rdiff01_w);

      v_sum_d = _mm_add_epi32(v_sum_d, v_rdiff_d);
      v_sum_d = _mm_add_epi32(v_sum_d, v_rdiff1_d);
      v_sse_d = _mm_add_epi32(v_sse_d, v_sqrdiff_d);

      pre_temp += 8;
      n += 8;
      width -= 8;
    } while (width > 0);
    pre += pre_stride;
    height -= 1;
  } while (height > 0);
  v_d = _mm_hadd_epi32(v_sum_d, v_sse_d);
  v_d = _mm_hadd_epi32(v_d, v_d);
  *sum = _mm_cvtsi128_si32(v_d);
  *sse = (unsigned int)_mm_cvtsi128_si32(_mm_srli_si128(v_d, 4));
}

static INLINE void obmc_variance_w16n(const uint8_t *pre, const int pre_stride,
                                      const int32_t *wsrc, const int32_t *mask,
                                      unsigned int *const sse, int *const sum,
                                      const int w, const int h) {
  int n = 0, width, height = h;
  __m256i v_d;
  __m128i res0;
  const uint8_t *pre_temp;
  const __m256i v_bias_d = _mm256_set1_epi32((1 << 12) >> 1);
  __m256i v_sum_d = _mm256_setzero_si256();
  __m256i v_sse_d = _mm256_setzero_si256();

  assert(w >= 16);
  assert(IS_POWER_OF_TWO(w));
  assert(IS_POWER_OF_TWO(h));
  do {
    width = w;
    pre_temp = pre;
    do {
      const __m128i v_p_b = _mm_loadu_si128((__m128i *)pre_temp);
      const __m256i v_m0_d = _mm256_loadu_si256((__m256i const *)(mask + n));
      const __m256i v_w0_d = _mm256_loadu_si256((__m256i const *)(wsrc + n));
      const __m256i v_m1_d =
          _mm256_loadu_si256((__m256i const *)(mask + n + 8));
      const __m256i v_w1_d =
          _mm256_loadu_si256((__m256i const *)(wsrc + n + 8));

      const __m256i v_p0_d = _mm256_cvtepu8_epi32(v_p_b);
      const __m256i v_p1_d = _mm256_cvtepu8_epi32(_mm_srli_si128(v_p_b, 8));

      const __m256i v_pm0_d = _mm256_madd_epi16(v_p0_d, v_m0_d);
      const __m256i v_pm1_d = _mm256_madd_epi16(v_p1_d, v_m1_d);

      const __m256i v_diff0_d = _mm256_sub_epi32(v_w0_d, v_pm0_d);
      const __m256i v_diff1_d = _mm256_sub_epi32(v_w1_d, v_pm1_d);

      const __m256i v_sign0_d = _mm256_srai_epi32(v_diff0_d, 31);
      const __m256i v_sign1_d = _mm256_srai_epi32(v_diff1_d, 31);

      const __m256i v_tmp0_d =
          _mm256_add_epi32(_mm256_add_epi32(v_diff0_d, v_bias_d), v_sign0_d);
      const __m256i v_tmp1_d =
          _mm256_add_epi32(_mm256_add_epi32(v_diff1_d, v_bias_d), v_sign1_d);

      const __m256i v_rdiff0_d = _mm256_srai_epi32(v_tmp0_d, 12);
      const __m256i v_rdiff2_d = _mm256_srai_epi32(v_tmp1_d, 12);

      const __m256i v_rdiff1_d = _mm256_add_epi32(v_rdiff0_d, v_rdiff2_d);
      const __m256i v_rdiff01_w = _mm256_packs_epi32(v_rdiff0_d, v_rdiff2_d);
      const __m256i v_sqrdiff_d = _mm256_madd_epi16(v_rdiff01_w, v_rdiff01_w);

      v_sum_d = _mm256_add_epi32(v_sum_d, v_rdiff1_d);
      v_sse_d = _mm256_add_epi32(v_sse_d, v_sqrdiff_d);

      pre_temp += 16;
      n += 16;
      width -= 16;
    } while (width > 0);
    pre += pre_stride;
    height -= 1;
  } while (height > 0);

  v_d = _mm256_hadd_epi32(v_sum_d, v_sse_d);
  v_d = _mm256_hadd_epi32(v_d, v_d);
  res0 = _mm256_castsi256_si128(v_d);
  res0 = _mm_add_epi32(res0, _mm256_extractf128_si256(v_d, 1));
  *sum = _mm_cvtsi128_si32(res0);
  *sse = (unsigned int)_mm_cvtsi128_si32(_mm_srli_si128(res0, 4));
}

#define OBMCVARWXH(W, H)                                                \
  unsigned int aom_obmc_variance##W##x##H##_avx2(                       \
      const uint8_t *pre, int pre_stride, const int32_t *wsrc,          \
      const int32_t *mask, unsigned int *sse) {                         \
    int sum;                                                            \
    if (W == 4) {                                                       \
      obmc_variance_w4(pre, pre_stride, wsrc, mask, sse, &sum, H);      \
    } else if (W == 8) {                                                \
      obmc_variance_w8n(pre, pre_stride, wsrc, mask, sse, &sum, W, H);  \
    } else {                                                            \
      obmc_variance_w16n(pre, pre_stride, wsrc, mask, sse, &sum, W, H); \
    }                                                                   \
                                                                        \
    return *sse - (unsigned int)(((int64_t)sum * sum) / (W * H));       \
  }

OBMCVARWXH(128, 128)
OBMCVARWXH(128, 64)
OBMCVARWXH(64, 128)
OBMCVARWXH(64, 64)
OBMCVARWXH(64, 32)
OBMCVARWXH(32, 64)
OBMCVARWXH(32, 32)
OBMCVARWXH(32, 16)
OBMCVARWXH(16, 32)
OBMCVARWXH(16, 16)
OBMCVARWXH(16, 8)
OBMCVARWXH(8, 16)
OBMCVARWXH(8, 8)
OBMCVARWXH(8, 4)
OBMCVARWXH(4, 8)
OBMCVARWXH(4, 4)
OBMCVARWXH(4, 16)
OBMCVARWXH(16, 4)
OBMCVARWXH(8, 32)
OBMCVARWXH(32, 8)
OBMCVARWXH(16, 64)
OBMCVARWXH(64, 16)
