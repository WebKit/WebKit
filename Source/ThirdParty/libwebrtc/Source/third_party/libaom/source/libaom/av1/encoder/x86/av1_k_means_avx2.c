/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include <immintrin.h>  // AVX2

#include "config/av1_rtcd.h"
#include "aom_dsp/x86/synonyms.h"

static int64_t k_means_horizontal_sum_avx2(__m256i a) {
  const __m128i low = _mm256_castsi256_si128(a);
  const __m128i high = _mm256_extracti128_si256(a, 1);
  const __m128i sum = _mm_add_epi64(low, high);
  const __m128i sum_high = _mm_unpackhi_epi64(sum, sum);
  int64_t res;
  _mm_storel_epi64((__m128i *)&res, _mm_add_epi64(sum, sum_high));
  return res;
}

void av1_calc_indices_dim1_avx2(const int16_t *data, const int16_t *centroids,
                                uint8_t *indices, int64_t *total_dist, int n,
                                int k) {
  const __m256i v_zero = _mm256_setzero_si256();
  __m256i sum = _mm256_setzero_si256();
  __m256i cents[PALETTE_MAX_SIZE];
  for (int j = 0; j < k; ++j) {
    cents[j] = _mm256_set1_epi16(centroids[j]);
  }

  for (int i = 0; i < n; i += 16) {
    const __m256i in = _mm256_loadu_si256((__m256i *)data);
    __m256i ind = _mm256_setzero_si256();
    // Compute the distance to the first centroid.
    __m256i d1 = _mm256_sub_epi16(in, cents[0]);
    __m256i dist_min = _mm256_abs_epi16(d1);

    for (int j = 1; j < k; ++j) {
      // Compute the distance to the centroid.
      d1 = _mm256_sub_epi16(in, cents[j]);
      const __m256i dist = _mm256_abs_epi16(d1);
      // Compare to the minimal one.
      const __m256i cmp = _mm256_cmpgt_epi16(dist_min, dist);
      dist_min = _mm256_min_epi16(dist_min, dist);
      const __m256i ind1 = _mm256_set1_epi16(j);
      ind = _mm256_or_si256(_mm256_andnot_si256(cmp, ind),
                            _mm256_and_si256(cmp, ind1));
    }

    const __m256i p1 = _mm256_packus_epi16(ind, v_zero);
    const __m256i px = _mm256_permute4x64_epi64(p1, 0x58);
    const __m128i d2 = _mm256_extracti128_si256(px, 0);

    _mm_storeu_si128((__m128i *)indices, d2);

    if (total_dist) {
      // Square, convert to 32 bit and add together.
      dist_min = _mm256_madd_epi16(dist_min, dist_min);
      // Convert to 64 bit and add to sum.
      const __m256i dist1 = _mm256_unpacklo_epi32(dist_min, v_zero);
      const __m256i dist2 = _mm256_unpackhi_epi32(dist_min, v_zero);
      sum = _mm256_add_epi64(sum, dist1);
      sum = _mm256_add_epi64(sum, dist2);
    }

    indices += 16;
    data += 16;
  }
  if (total_dist) {
    *total_dist = k_means_horizontal_sum_avx2(sum);
  }
}

void av1_calc_indices_dim2_avx2(const int16_t *data, const int16_t *centroids,
                                uint8_t *indices, int64_t *total_dist, int n,
                                int k) {
  const __m256i v_zero = _mm256_setzero_si256();
  const __m256i permute = _mm256_set_epi32(0, 0, 0, 0, 5, 1, 4, 0);
  __m256i sum = _mm256_setzero_si256();
  __m256i ind[2];
  __m256i cents[PALETTE_MAX_SIZE];
  for (int j = 0; j < k; ++j) {
    const int16_t cx = centroids[2 * j], cy = centroids[2 * j + 1];
    cents[j] = _mm256_set_epi16(cy, cx, cy, cx, cy, cx, cy, cx, cy, cx, cy, cx,
                                cy, cx, cy, cx);
  }

  for (int i = 0; i < n; i += 16) {
    for (int l = 0; l < 2; ++l) {
      const __m256i in = _mm256_loadu_si256((__m256i *)data);
      ind[l] = _mm256_setzero_si256();
      // Compute the distance to the first centroid.
      __m256i d1 = _mm256_sub_epi16(in, cents[0]);
      __m256i dist_min = _mm256_madd_epi16(d1, d1);

      for (int j = 1; j < k; ++j) {
        // Compute the distance to the centroid.
        d1 = _mm256_sub_epi16(in, cents[j]);
        const __m256i dist = _mm256_madd_epi16(d1, d1);
        // Compare to the minimal one.
        const __m256i cmp = _mm256_cmpgt_epi32(dist_min, dist);
        dist_min = _mm256_min_epi32(dist_min, dist);
        const __m256i ind1 = _mm256_set1_epi32(j);
        ind[l] = _mm256_or_si256(_mm256_andnot_si256(cmp, ind[l]),
                                 _mm256_and_si256(cmp, ind1));
      }
      if (total_dist) {
        // Convert to 64 bit and add to sum.
        const __m256i dist1 = _mm256_unpacklo_epi32(dist_min, v_zero);
        const __m256i dist2 = _mm256_unpackhi_epi32(dist_min, v_zero);
        sum = _mm256_add_epi64(sum, dist1);
        sum = _mm256_add_epi64(sum, dist2);
      }
      data += 16;
    }
    // Cast to 8 bit and store.
    const __m256i d2 = _mm256_packus_epi32(ind[0], ind[1]);
    const __m256i d3 = _mm256_packus_epi16(d2, v_zero);
    const __m256i d4 = _mm256_permutevar8x32_epi32(d3, permute);
    const __m128i d5 = _mm256_extracti128_si256(d4, 0);
    _mm_storeu_si128((__m128i *)indices, d5);
    indices += 16;
  }
  if (total_dist) {
    *total_dist = k_means_horizontal_sum_avx2(sum);
  }
}
