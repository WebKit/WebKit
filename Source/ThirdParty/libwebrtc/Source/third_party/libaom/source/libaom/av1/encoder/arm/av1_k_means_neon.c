/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>
#include <assert.h>

#include "aom_dsp/arm/sum_neon.h"
#include "aom_ports/mem.h"
#include "av1/encoder/random.h"
#include "config/aom_config.h"
#include "config/av1_rtcd.h"

static int32x4_t k_means_multiply_add_neon(const int16x8_t a) {
  const int32x4_t l = vmull_s16(vget_low_s16(a), vget_low_s16(a));
  const int32x4_t h = vmull_s16(vget_high_s16(a), vget_high_s16(a));
#if AOM_ARCH_AARCH64
  return vpaddq_s32(l, h);
#else
  const int32x2_t dl = vpadd_s32(vget_low_s32(l), vget_high_s32(l));
  const int32x2_t dh = vpadd_s32(vget_low_s32(h), vget_high_s32(h));
  return vcombine_s32(dl, dh);
#endif
}

void av1_calc_indices_dim1_neon(const int16_t *data, const int16_t *centroids,
                                uint8_t *indices, int64_t *total_dist, int n,
                                int k) {
  int64x2_t sum = vdupq_n_s64(0);
  int16x8_t cents[PALETTE_MAX_SIZE];
  for (int j = 0; j < k; ++j) {
    cents[j] = vdupq_n_s16(centroids[j]);
  }

  for (int i = 0; i < n; i += 8) {
    const int16x8_t in = vld1q_s16(data);
    uint16x8_t ind = vdupq_n_u16(0);
    // Compute the distance to the first centroid.
    int16x8_t dist_min = vabdq_s16(in, cents[0]);

    for (int j = 1; j < k; ++j) {
      // Compute the distance to the centroid.
      const int16x8_t dist = vabdq_s16(in, cents[j]);
      // Compare to the minimal one.
      const uint16x8_t cmp = vcgtq_s16(dist_min, dist);
      dist_min = vminq_s16(dist_min, dist);
      const uint16x8_t ind1 = vdupq_n_u16(j);
      ind = vbslq_u16(cmp, ind1, ind);
    }
    if (total_dist) {
      // Square, convert to 32 bit and add together.
      const int32x4_t l =
          vmull_s16(vget_low_s16(dist_min), vget_low_s16(dist_min));
      const int32x4_t sum32_tmp =
          vmlal_s16(l, vget_high_s16(dist_min), vget_high_s16(dist_min));
      // Pairwise sum, convert to 64 bit and add to sum.
      sum = vpadalq_s32(sum, sum32_tmp);
    }
    vst1_u8(indices, vmovn_u16(ind));
    indices += 8;
    data += 8;
  }
  if (total_dist) {
    *total_dist = horizontal_add_s64x2(sum);
  }
}

void av1_calc_indices_dim2_neon(const int16_t *data, const int16_t *centroids,
                                uint8_t *indices, int64_t *total_dist, int n,
                                int k) {
  int64x2_t sum = vdupq_n_s64(0);
  uint32x4_t ind[2];
  int16x8_t cents[PALETTE_MAX_SIZE];
  for (int j = 0; j < k; ++j) {
    const int16_t cx = centroids[2 * j], cy = centroids[2 * j + 1];
    const int16_t cxcy[8] = { cx, cy, cx, cy, cx, cy, cx, cy };
    cents[j] = vld1q_s16(cxcy);
  }

  for (int i = 0; i < n; i += 8) {
    for (int l = 0; l < 2; ++l) {
      const int16x8_t in = vld1q_s16(data);
      ind[l] = vdupq_n_u32(0);
      // Compute the distance to the first centroid.
      int16x8_t d1 = vsubq_s16(in, cents[0]);
      int32x4_t dist_min = k_means_multiply_add_neon(d1);

      for (int j = 1; j < k; ++j) {
        // Compute the distance to the centroid.
        d1 = vsubq_s16(in, cents[j]);
        const int32x4_t dist = k_means_multiply_add_neon(d1);
        // Compare to the minimal one.
        const uint32x4_t cmp = vcgtq_s32(dist_min, dist);
        dist_min = vminq_s32(dist_min, dist);
        const uint32x4_t ind1 = vdupq_n_u32(j);
        ind[l] = vbslq_u32(cmp, ind1, ind[l]);
      }
      if (total_dist) {
        // Pairwise sum, convert to 64 bit and add to sum.
        sum = vpadalq_s32(sum, dist_min);
      }
      data += 8;
    }
    // Cast to 8 bit and store.
    vst1_u8(indices,
            vmovn_u16(vcombine_u16(vmovn_u32(ind[0]), vmovn_u32(ind[1]))));
    indices += 8;
  }
  if (total_dist) {
    *total_dist = horizontal_add_s64x2(sum);
  }
}

// Adds the size and the pixel sum of cluster j to count and sum.
static inline void count_and_sum_neon(const int16_t *data,
                                      const uint8_t *indices, int n16, int j,
                                      int *count, int *sum) {
  const uint8x16_t index = vdupq_n_u8(j);
  int16x8_t count_acc = vdupq_n_s16(0);
  int32x4_t sum_acc[2] = { vdupq_n_s32(0), vdupq_n_s32(0) };
  for (int i = 0; i < n16; i += 16) {
    // All ones in the lanes that belong to cluster j.
    const uint8x16_t mask = vceqq_u8(vld1q_u8(indices + i), index);
    const uint8x16x2_t mask16 = vzipq_u8(mask, mask);
    const int16x8_t d0 = vld1q_s16(data + i);
    const int16x8_t d1 = vld1q_s16(data + i + 8);
    count_acc = vpadalq_s8(count_acc, vreinterpretq_s8_u8(mask));
    sum_acc[0] = vpadalq_s16(
        sum_acc[0], vandq_s16(d0, vreinterpretq_s16_u8(mask16.val[0])));
    sum_acc[1] = vpadalq_s16(
        sum_acc[1], vandq_s16(d1, vreinterpretq_s16_u8(mask16.val[1])));
  }
  count[j] -= horizontal_add_s16x8(count_acc);
  sum[j] += horizontal_add_s32x4(vaddq_s32(sum_acc[0], sum_acc[1]));
}

// Adds the size and the pixel sum of the clusters j and j + 1 to count and
// sum. Two clusters per pass share the loads.
static inline void count_and_sum_pair_neon(const int16_t *data,
                                           const uint8_t *indices, int n16,
                                           int j, int *count, int *sum) {
  const uint8x16_t index0 = vdupq_n_u8(j);
  const uint8x16_t index1 = vdupq_n_u8(j + 1);
  int16x8_t count_acc[2] = { vdupq_n_s16(0), vdupq_n_s16(0) };
  int32x4_t sum_acc[4] = { vdupq_n_s32(0), vdupq_n_s32(0), vdupq_n_s32(0),
                           vdupq_n_s32(0) };
  for (int i = 0; i < n16; i += 16) {
    const uint8x16_t idx = vld1q_u8(indices + i);
    const int16x8_t d0 = vld1q_s16(data + i);
    const int16x8_t d1 = vld1q_s16(data + i + 8);
    // All ones in the lanes that belong to cluster j or j + 1.
    const uint8x16_t mask0 = vceqq_u8(idx, index0);
    const uint8x16_t mask1 = vceqq_u8(idx, index1);
    const uint8x16x2_t mask0_16 = vzipq_u8(mask0, mask0);
    const uint8x16x2_t mask1_16 = vzipq_u8(mask1, mask1);
    count_acc[0] = vpadalq_s8(count_acc[0], vreinterpretq_s8_u8(mask0));
    count_acc[1] = vpadalq_s8(count_acc[1], vreinterpretq_s8_u8(mask1));
    sum_acc[0] = vpadalq_s16(
        sum_acc[0], vandq_s16(d0, vreinterpretq_s16_u8(mask0_16.val[0])));
    sum_acc[1] = vpadalq_s16(
        sum_acc[1], vandq_s16(d1, vreinterpretq_s16_u8(mask0_16.val[1])));
    sum_acc[2] = vpadalq_s16(
        sum_acc[2], vandq_s16(d0, vreinterpretq_s16_u8(mask1_16.val[0])));
    sum_acc[3] = vpadalq_s16(
        sum_acc[3], vandq_s16(d1, vreinterpretq_s16_u8(mask1_16.val[1])));
  }
  count[j] -= horizontal_add_s16x8(count_acc[0]);
  count[j + 1] -= horizontal_add_s16x8(count_acc[1]);
  sum[j] += horizontal_add_s32x4(vaddq_s32(sum_acc[0], sum_acc[1]));
  sum[j + 1] += horizontal_add_s32x4(vaddq_s32(sum_acc[2], sum_acc[3]));
}

void av1_calc_centroids_dim1_neon(const int16_t *data, int16_t *centroids,
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
    count_and_sum_pair_neon(data, indices, n16, j, count, sum);
  }
  if (j < k) {
    count_and_sum_neon(data, indices, n16, j, count, sum);
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
