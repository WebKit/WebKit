/*
 *  Copyright (c) 2023, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>

#include "aom_dsp/arm/sum_neon.h"
#include "config/aom_dsp_rtcd.h"

static int32x4_t k_means_multiply_add_neon(const int16x8_t a) {
  const int32x4_t l = vmull_s16(vget_low_s16(a), vget_low_s16(a));
  const int32x4_t h = vmull_s16(vget_high_s16(a), vget_high_s16(a));
#if defined(__aarch64__)
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
