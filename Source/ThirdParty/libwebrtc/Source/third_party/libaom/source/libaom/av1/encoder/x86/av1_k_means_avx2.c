/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include <assert.h>
#include <immintrin.h>  // AVX2

#include "config/av1_rtcd.h"
#include "aom_dsp/x86/synonyms.h"
#include "aom_ports/mem.h"
#include "av1/encoder/random.h"

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

// Adds the pixels of d picked by the 16 bit mask to the count and sum
// accumulators.
static inline void accumulate_cluster_avx2(__m256i mask, __m256i d,
                                           __m256i *count_acc,
                                           __m256i *sum_acc) {
  const __m256i ones = _mm256_set1_epi16(1);
  *count_acc = _mm256_sub_epi16(*count_acc, mask);
  *sum_acc = _mm256_add_epi32(
      *sum_acc, _mm256_madd_epi16(_mm256_and_si256(d, mask), ones));
}

static inline void add_cluster_avx2(__m256i count_acc, __m256i sum_acc,
                                    int *count, int *sum) {
  const __m256i ones = _mm256_set1_epi16(1);
  // Add up the sum and the count together, they end up in lanes 0 and 2.
  const __m256i count32 = _mm256_madd_epi16(count_acc, ones);
  const __m256i t256 =
      _mm256_add_epi32(_mm256_unpacklo_epi64(sum_acc, count32),
                       _mm256_unpackhi_epi64(sum_acc, count32));
  __m128i t = _mm_add_epi32(_mm256_castsi256_si128(t256),
                            _mm256_extracti128_si256(t256, 1));
  t = _mm_add_epi32(t, _mm_srli_epi64(t, 32));
  *sum += _mm_cvtsi128_si32(t);
  *count += _mm_cvtsi128_si32(_mm_srli_si128(t, 8));
}

// Adds the size and the pixel sum of cluster j to count and sum.
static inline void count_and_sum_avx2(const int16_t *data,
                                      const uint8_t *indices, int n16, int j,
                                      int *count, int *sum) {
  const __m128i index = _mm_set1_epi8(j);
  __m256i count_acc = _mm256_setzero_si256();
  __m256i sum_acc = _mm256_setzero_si256();
  for (int i = 0; i < n16; i += 16) {
    // All ones in the lanes that belong to cluster j. The sign extension
    // keeps the 16 bit mask in the same order as the data.
    const __m256i mask = _mm256_cvtepi8_epi16(
        _mm_cmpeq_epi8(_mm_loadu_si128((const __m128i *)(indices + i)), index));
    const __m256i d = _mm256_loadu_si256((const __m256i *)(data + i));
    accumulate_cluster_avx2(mask, d, &count_acc, &sum_acc);
  }
  add_cluster_avx2(count_acc, sum_acc, &count[j], &sum[j]);
}

// Adds the size and the pixel sum of the clusters j and j + 1 to count and
// sum. Two clusters per pass share the loads.
static inline void count_and_sum_pair_avx2(const int16_t *data,
                                           const uint8_t *indices, int n16,
                                           int j, int *count, int *sum) {
  const __m128i index0 = _mm_set1_epi8(j);
  const __m128i index1 = _mm_set1_epi8(j + 1);
  __m256i count_acc[2] = { _mm256_setzero_si256(), _mm256_setzero_si256() };
  __m256i sum_acc[2] = { _mm256_setzero_si256(), _mm256_setzero_si256() };
  for (int i = 0; i < n16; i += 16) {
    const __m128i idx = _mm_loadu_si128((const __m128i *)(indices + i));
    const __m256i d = _mm256_loadu_si256((const __m256i *)(data + i));
    // All ones in the lanes that belong to cluster j or j + 1.
    const __m256i mask0 = _mm256_cvtepi8_epi16(_mm_cmpeq_epi8(idx, index0));
    const __m256i mask1 = _mm256_cvtepi8_epi16(_mm_cmpeq_epi8(idx, index1));
    accumulate_cluster_avx2(mask0, d, &count_acc[0], &sum_acc[0]);
    accumulate_cluster_avx2(mask1, d, &count_acc[1], &sum_acc[1]);
  }
  add_cluster_avx2(count_acc[0], sum_acc[0], &count[j], &sum[j]);
  add_cluster_avx2(count_acc[1], sum_acc[1], &count[j + 1], &sum[j + 1]);
}

void av1_calc_centroids_dim1_avx2(const int16_t *data, int16_t *centroids,
                                  const uint8_t *indices, int n, int k) {
  int count[PALETTE_MAX_SIZE] = { 0 };
  int sum[PALETTE_MAX_SIZE] = { 0 };
  const int n16 = n & ~15;
  assert(n <= 32768);

  for (int i = n16; i < n; ++i) {
    ++count[indices[i]];
    sum[indices[i]] += data[i];
  }

  int j = 0;
  for (; j + 1 < k; j += 2) {
    count_and_sum_pair_avx2(data, indices, n16, j, count, sum);
  }
  if (j < k) {
    count_and_sum_avx2(data, indices, n16, j, count, sum);
  }

  unsigned int rand_state = (unsigned int)data[0];
  for (j = 0; j < k; ++j) {
    if (count[j] == 0) {
      centroids[j] = data[lcg_rand16(&rand_state) % n];
    } else {
      centroids[j] = DIVIDE_AND_ROUND(sum[j], count[j]);
    }
  }
}
