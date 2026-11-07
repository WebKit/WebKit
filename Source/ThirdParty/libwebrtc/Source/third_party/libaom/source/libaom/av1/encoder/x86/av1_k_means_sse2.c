/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <emmintrin.h>  // SSE2

#include "config/av1_rtcd.h"
#include "aom_dsp/x86/synonyms.h"
#include "aom_ports/mem.h"
#include "av1/encoder/random.h"

static int64_t k_means_horizontal_sum_sse2(__m128i a) {
  const __m128i sum1 = _mm_unpackhi_epi64(a, a);
  const __m128i sum2 = _mm_add_epi64(a, sum1);
  int64_t res;
  _mm_storel_epi64((__m128i *)&res, sum2);
  return res;
}

void av1_calc_indices_dim1_sse2(const int16_t *data, const int16_t *centroids,
                                uint8_t *indices, int64_t *total_dist, int n,
                                int k) {
  const __m128i v_zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128();
  __m128i cents[PALETTE_MAX_SIZE];
  for (int j = 0; j < k; ++j) {
    cents[j] = _mm_set1_epi16(centroids[j]);
  }

  for (int i = 0; i < n; i += 8) {
    const __m128i in = _mm_loadu_si128((__m128i *)data);
    __m128i ind = _mm_setzero_si128();
    // Compute the distance to the first centroid.
    __m128i d1 = _mm_sub_epi16(in, cents[0]);
    __m128i d2 = _mm_sub_epi16(cents[0], in);
    __m128i dist_min = _mm_max_epi16(d1, d2);

    for (int j = 1; j < k; ++j) {
      // Compute the distance to the centroid.
      d1 = _mm_sub_epi16(in, cents[j]);
      d2 = _mm_sub_epi16(cents[j], in);
      const __m128i dist = _mm_max_epi16(d1, d2);
      // Compare to the minimal one.
      const __m128i cmp = _mm_cmpgt_epi16(dist_min, dist);
      dist_min = _mm_min_epi16(dist_min, dist);
      const __m128i ind1 = _mm_set1_epi16(j);
      ind = _mm_or_si128(_mm_andnot_si128(cmp, ind), _mm_and_si128(cmp, ind1));
    }
    if (total_dist) {
      // Square, convert to 32 bit and add together.
      dist_min = _mm_madd_epi16(dist_min, dist_min);
      // Convert to 64 bit and add to sum.
      const __m128i dist1 = _mm_unpacklo_epi32(dist_min, v_zero);
      const __m128i dist2 = _mm_unpackhi_epi32(dist_min, v_zero);
      sum = _mm_add_epi64(sum, dist1);
      sum = _mm_add_epi64(sum, dist2);
    }
    __m128i p2 = _mm_packus_epi16(ind, v_zero);
    _mm_storel_epi64((__m128i *)indices, p2);
    indices += 8;
    data += 8;
  }
  if (total_dist) {
    *total_dist = k_means_horizontal_sum_sse2(sum);
  }
}

void av1_calc_indices_dim2_sse2(const int16_t *data, const int16_t *centroids,
                                uint8_t *indices, int64_t *total_dist, int n,
                                int k) {
  const __m128i v_zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128();
  __m128i ind[2];
  __m128i cents[PALETTE_MAX_SIZE];
  for (int j = 0; j < k; ++j) {
    const int16_t cx = centroids[2 * j], cy = centroids[2 * j + 1];
    cents[j] = _mm_set_epi16(cy, cx, cy, cx, cy, cx, cy, cx);
  }

  for (int i = 0; i < n; i += 8) {
    for (int l = 0; l < 2; ++l) {
      const __m128i in = _mm_loadu_si128((__m128i *)data);
      ind[l] = _mm_setzero_si128();
      // Compute the distance to the first centroid.
      __m128i d1 = _mm_sub_epi16(in, cents[0]);
      __m128i dist_min = _mm_madd_epi16(d1, d1);

      for (int j = 1; j < k; ++j) {
        // Compute the distance to the centroid.
        d1 = _mm_sub_epi16(in, cents[j]);
        const __m128i dist = _mm_madd_epi16(d1, d1);
        // Compare to the minimal one.
        const __m128i cmp = _mm_cmpgt_epi32(dist_min, dist);
        const __m128i dist1 = _mm_andnot_si128(cmp, dist_min);
        const __m128i dist2 = _mm_and_si128(cmp, dist);
        dist_min = _mm_or_si128(dist1, dist2);
        const __m128i ind1 = _mm_set1_epi32(j);
        ind[l] = _mm_or_si128(_mm_andnot_si128(cmp, ind[l]),
                              _mm_and_si128(cmp, ind1));
      }
      if (total_dist) {
        // Convert to 64 bit and add to sum.
        const __m128i dist1 = _mm_unpacklo_epi32(dist_min, v_zero);
        const __m128i dist2 = _mm_unpackhi_epi32(dist_min, v_zero);
        sum = _mm_add_epi64(sum, dist1);
        sum = _mm_add_epi64(sum, dist2);
      }
      data += 8;
    }
    // Cast to 8 bit and store.
    const __m128i d2 = _mm_packus_epi16(ind[0], ind[1]);
    const __m128i d3 = _mm_packus_epi16(d2, v_zero);
    _mm_storel_epi64((__m128i *)indices, d3);
    indices += 8;
  }
  if (total_dist) {
    *total_dist = k_means_horizontal_sum_sse2(sum);
  }
}

// Adds the pixels of d0 and d1 picked by the byte mask to the count and sum
// accumulators.
static inline void accumulate_cluster_sse2(__m128i mask, __m128i d0, __m128i d1,
                                           __m128i *count_acc,
                                           __m128i *sum_acc) {
  const __m128i ones = _mm_set1_epi16(1);
  const __m128i mask0 = _mm_unpacklo_epi8(mask, mask);
  const __m128i mask1 = _mm_unpackhi_epi8(mask, mask);
  // Each int16 lane counts at most 2 * 32768 / 16 pixels.
  *count_acc = _mm_sub_epi16(*count_acc, _mm_add_epi16(mask0, mask1));
  *sum_acc = _mm_add_epi32(
      *sum_acc, _mm_add_epi32(_mm_madd_epi16(_mm_and_si128(d0, mask0), ones),
                              _mm_madd_epi16(_mm_and_si128(d1, mask1), ones)));
}

static inline void add_cluster_sse2(__m128i count_acc, __m128i sum_acc,
                                    int *count, int *sum) {
  const __m128i ones = _mm_set1_epi16(1);
  // Add up the sum and the count together, they end up in lanes 0 and 2.
  const __m128i count32 = _mm_madd_epi16(count_acc, ones);
  __m128i t = _mm_add_epi32(_mm_unpacklo_epi64(sum_acc, count32),
                            _mm_unpackhi_epi64(sum_acc, count32));
  t = _mm_add_epi32(t, _mm_srli_epi64(t, 32));
  *sum += _mm_cvtsi128_si32(t);
  *count += _mm_cvtsi128_si32(_mm_srli_si128(t, 8));
}

// Adds the size and the pixel sum of cluster j to count and sum.
static inline void count_and_sum_sse2(const int16_t *data,
                                      const uint8_t *indices, int n16, int j,
                                      int *count, int *sum) {
  const __m128i index = _mm_set1_epi8(j);
  __m128i count_acc = _mm_setzero_si128();
  __m128i sum_acc = _mm_setzero_si128();
  for (int i = 0; i < n16; i += 16) {
    // All ones in the lanes that belong to cluster j.
    const __m128i mask =
        _mm_cmpeq_epi8(_mm_loadu_si128((const __m128i *)(indices + i)), index);
    const __m128i d0 = _mm_loadu_si128((const __m128i *)(data + i));
    const __m128i d1 = _mm_loadu_si128((const __m128i *)(data + i + 8));
    accumulate_cluster_sse2(mask, d0, d1, &count_acc, &sum_acc);
  }
  add_cluster_sse2(count_acc, sum_acc, &count[j], &sum[j]);
}

// Adds the size and the pixel sum of the clusters j and j + 1 to count and
// sum. Two clusters per pass share the loads.
static inline void count_and_sum_pair_sse2(const int16_t *data,
                                           const uint8_t *indices, int n16,
                                           int j, int *count, int *sum) {
  const __m128i index0 = _mm_set1_epi8(j);
  const __m128i index1 = _mm_set1_epi8(j + 1);
  __m128i count_acc[2] = { _mm_setzero_si128(), _mm_setzero_si128() };
  __m128i sum_acc[2] = { _mm_setzero_si128(), _mm_setzero_si128() };
  for (int i = 0; i < n16; i += 16) {
    const __m128i idx = _mm_loadu_si128((const __m128i *)(indices + i));
    const __m128i d0 = _mm_loadu_si128((const __m128i *)(data + i));
    const __m128i d1 = _mm_loadu_si128((const __m128i *)(data + i + 8));
    // All ones in the lanes that belong to cluster j or j + 1.
    accumulate_cluster_sse2(_mm_cmpeq_epi8(idx, index0), d0, d1, &count_acc[0],
                            &sum_acc[0]);
    accumulate_cluster_sse2(_mm_cmpeq_epi8(idx, index1), d0, d1, &count_acc[1],
                            &sum_acc[1]);
  }
  add_cluster_sse2(count_acc[0], sum_acc[0], &count[j], &sum[j]);
  add_cluster_sse2(count_acc[1], sum_acc[1], &count[j + 1], &sum[j + 1]);
}

void av1_calc_centroids_dim1_sse2(const int16_t *data, int16_t *centroids,
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
    count_and_sum_pair_sse2(data, indices, n16, j, count, sum);
  }
  if (j < k) {
    count_and_sum_sse2(data, indices, n16, j, count, sum);
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
