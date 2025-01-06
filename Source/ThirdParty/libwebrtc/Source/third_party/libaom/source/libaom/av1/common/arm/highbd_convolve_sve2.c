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

#include <assert.h>
#include <arm_neon.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/arm/aom_neon_sve_bridge.h"
#include "aom_dsp/arm/aom_neon_sve2_bridge.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_ports/mem.h"
#include "av1/common/convolve.h"
#include "av1/common/filter.h"
#include "av1/common/arm/highbd_convolve_sve2.h"

DECLARE_ALIGNED(16, static const uint16_t, kDotProdTbl[32]) = {
  0, 1, 2, 3, 1, 2, 3, 4, 2, 3, 4, 5, 3, 4, 5, 6,
  4, 5, 6, 7, 5, 6, 7, 0, 6, 7, 0, 1, 7, 0, 1, 2,
};

static inline uint16x4_t convolve12_4_x(
    int16x8_t s0, int16x8_t s1, int16x8_t filter_0_7, int16x8_t filter_4_11,
    const int64x2_t offset, uint16x8x4_t permute_tbl, uint16x4_t max) {
  int16x8_t permuted_samples[6];
  permuted_samples[0] = aom_tbl_s16(s0, permute_tbl.val[0]);
  permuted_samples[1] = aom_tbl_s16(s0, permute_tbl.val[1]);
  permuted_samples[2] = aom_tbl2_s16(s0, s1, permute_tbl.val[2]);
  permuted_samples[3] = aom_tbl2_s16(s0, s1, permute_tbl.val[3]);
  permuted_samples[4] = aom_tbl_s16(s1, permute_tbl.val[0]);
  permuted_samples[5] = aom_tbl_s16(s1, permute_tbl.val[1]);

  int64x2_t sum01 =
      aom_svdot_lane_s16(offset, permuted_samples[0], filter_0_7, 0);
  sum01 = aom_svdot_lane_s16(sum01, permuted_samples[2], filter_0_7, 1);
  sum01 = aom_svdot_lane_s16(sum01, permuted_samples[4], filter_4_11, 1);

  int64x2_t sum23 =
      aom_svdot_lane_s16(offset, permuted_samples[1], filter_0_7, 0);
  sum23 = aom_svdot_lane_s16(sum23, permuted_samples[3], filter_0_7, 1);
  sum23 = aom_svdot_lane_s16(sum23, permuted_samples[5], filter_4_11, 1);

  int32x4_t res0123 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));
  uint16x4_t res = vqrshrun_n_s32(res0123, FILTER_BITS);

  return vmin_u16(res, max);
}

static inline uint16x8_t convolve12_8_x(int16x8_t s0, int16x8_t s1,
                                        int16x8_t s2, int16x8_t filter_0_7,
                                        int16x8_t filter_4_11, int64x2_t offset,
                                        uint16x8x4_t permute_tbl,
                                        uint16x8_t max) {
  int16x8_t permuted_samples[8];
  permuted_samples[0] = aom_tbl_s16(s0, permute_tbl.val[0]);
  permuted_samples[1] = aom_tbl_s16(s0, permute_tbl.val[1]);
  permuted_samples[2] = aom_tbl2_s16(s0, s1, permute_tbl.val[2]);
  permuted_samples[3] = aom_tbl2_s16(s0, s1, permute_tbl.val[3]);
  permuted_samples[4] = aom_tbl_s16(s1, permute_tbl.val[0]);
  permuted_samples[5] = aom_tbl_s16(s1, permute_tbl.val[1]);
  permuted_samples[6] = aom_tbl2_s16(s1, s2, permute_tbl.val[2]);
  permuted_samples[7] = aom_tbl2_s16(s1, s2, permute_tbl.val[3]);

  int64x2_t sum01 =
      aom_svdot_lane_s16(offset, permuted_samples[0], filter_0_7, 0);
  sum01 = aom_svdot_lane_s16(sum01, permuted_samples[2], filter_0_7, 1);
  sum01 = aom_svdot_lane_s16(sum01, permuted_samples[4], filter_4_11, 1);

  int64x2_t sum23 =
      aom_svdot_lane_s16(offset, permuted_samples[1], filter_0_7, 0);
  sum23 = aom_svdot_lane_s16(sum23, permuted_samples[3], filter_0_7, 1);
  sum23 = aom_svdot_lane_s16(sum23, permuted_samples[5], filter_4_11, 1);

  int64x2_t sum45 =
      aom_svdot_lane_s16(offset, permuted_samples[2], filter_0_7, 0);
  sum45 = aom_svdot_lane_s16(sum45, permuted_samples[4], filter_0_7, 1);
  sum45 = aom_svdot_lane_s16(sum45, permuted_samples[6], filter_4_11, 1);

  int64x2_t sum67 =
      aom_svdot_lane_s16(offset, permuted_samples[3], filter_0_7, 0);
  sum67 = aom_svdot_lane_s16(sum67, permuted_samples[5], filter_0_7, 1);
  sum67 = aom_svdot_lane_s16(sum67, permuted_samples[7], filter_4_11, 1);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));
  int32x4_t sum4567 = vcombine_s32(vmovn_s64(sum45), vmovn_s64(sum67));

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(sum0123, FILTER_BITS),
                                vqrshrun_n_s32(sum4567, FILTER_BITS));

  return vminq_u16(res, max);
}

static inline void highbd_convolve_x_sr_12tap_sve2(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride,
    int width, int height, const int16_t *y_filter_ptr,
    ConvolveParams *conv_params, int bd) {
  // This shim allows to do only one rounding shift instead of two.
  const int64x2_t offset = vdupq_n_s64(1 << (conv_params->round_0 - 1));

  const int16x8_t y_filter_0_7 = vld1q_s16(y_filter_ptr);
  const int16x8_t y_filter_4_11 = vld1q_s16(y_filter_ptr + 4);

  uint16x8x4_t permute_tbl = vld1q_u16_x4(kDotProdTbl);
  // Scale indices by size of the true vector length to avoid reading from an
  // 'undefined' portion of a vector on a system with SVE vectors > 128-bit.
  uint16x8_t correction0 = vreinterpretq_u16_u64(vcombine_u64(
      vdup_n_u64(0), vdup_n_u64(svcnth() * 0x0001000000000000ULL)));
  permute_tbl.val[2] = vaddq_u16(permute_tbl.val[2], correction0);

  uint16x8_t correction1 = vreinterpretq_u16_u64(
      vcombine_u64(vdup_n_u64(svcnth() * 0x0001000100000000ULL),
                   vdup_n_u64(svcnth() * 0x0001000100010000ULL)));
  permute_tbl.val[3] = vaddq_u16(permute_tbl.val[3], correction1);

  if (width == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    const int16_t *s = (const int16_t *)src;

    do {
      int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;
      load_s16_8x4(s, src_stride, &s0, &s2, &s4, &s6);
      load_s16_8x4(s + 8, src_stride, &s1, &s3, &s5, &s7);

      uint16x4_t d0 = convolve12_4_x(s0, s1, y_filter_0_7, y_filter_4_11,
                                     offset, permute_tbl, max);
      uint16x4_t d1 = convolve12_4_x(s2, s3, y_filter_0_7, y_filter_4_11,
                                     offset, permute_tbl, max);
      uint16x4_t d2 = convolve12_4_x(s4, s5, y_filter_0_7, y_filter_4_11,
                                     offset, permute_tbl, max);
      uint16x4_t d3 = convolve12_4_x(s6, s7, y_filter_0_7, y_filter_4_11,
                                     offset, permute_tbl, max);

      store_u16_4x4(dst, dst_stride, d0, d1, d2, d3);

      s += 4 * src_stride;
      dst += 4 * dst_stride;
      height -= 4;
    } while (height != 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

    do {
      const int16_t *s = (const int16_t *)src;
      uint16_t *d = dst;
      int w = width;

      do {
        int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
        load_s16_8x4(s, src_stride, &s0, &s3, &s6, &s9);
        load_s16_8x4(s + 8, src_stride, &s1, &s4, &s7, &s10);
        load_s16_8x4(s + 16, src_stride, &s2, &s5, &s8, &s11);

        uint16x8_t d0 = convolve12_8_x(s0, s1, s2, y_filter_0_7, y_filter_4_11,
                                       offset, permute_tbl, max);
        uint16x8_t d1 = convolve12_8_x(s3, s4, s5, y_filter_0_7, y_filter_4_11,
                                       offset, permute_tbl, max);
        uint16x8_t d2 = convolve12_8_x(s6, s7, s8, y_filter_0_7, y_filter_4_11,
                                       offset, permute_tbl, max);
        uint16x8_t d3 = convolve12_8_x(s9, s10, s11, y_filter_0_7,
                                       y_filter_4_11, offset, permute_tbl, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        w -= 8;
      } while (w != 0);
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      height -= 4;
    } while (height != 0);
  }
}

static inline uint16x8_t convolve8_8_x(int16x8_t s0[8], int16x8_t filter,
                                       int64x2_t offset, uint16x8_t max) {
  int64x2_t sum[8];
  sum[0] = aom_sdotq_s16(offset, s0[0], filter);
  sum[1] = aom_sdotq_s16(offset, s0[1], filter);
  sum[2] = aom_sdotq_s16(offset, s0[2], filter);
  sum[3] = aom_sdotq_s16(offset, s0[3], filter);
  sum[4] = aom_sdotq_s16(offset, s0[4], filter);
  sum[5] = aom_sdotq_s16(offset, s0[5], filter);
  sum[6] = aom_sdotq_s16(offset, s0[6], filter);
  sum[7] = aom_sdotq_s16(offset, s0[7], filter);

  sum[0] = vpaddq_s64(sum[0], sum[1]);
  sum[2] = vpaddq_s64(sum[2], sum[3]);
  sum[4] = vpaddq_s64(sum[4], sum[5]);
  sum[6] = vpaddq_s64(sum[6], sum[7]);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum[0]), vmovn_s64(sum[2]));
  int32x4_t sum4567 = vcombine_s32(vmovn_s64(sum[4]), vmovn_s64(sum[6]));

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(sum0123, FILTER_BITS),
                                vqrshrun_n_s32(sum4567, FILTER_BITS));

  return vminq_u16(res, max);
}

static inline void highbd_convolve_x_sr_8tap_sve2(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride,
    int width, int height, const int16_t *y_filter_ptr,
    ConvolveParams *conv_params, int bd) {
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
  // This shim allows to do only one rounding shift instead of two.
  const int64_t offset = 1 << (conv_params->round_0 - 1);
  const int64x2_t offset_lo = vcombine_s64((int64x1_t)(offset), vdup_n_s64(0));

  const int16x8_t filter = vld1q_s16(y_filter_ptr);

  do {
    const int16_t *s = (const int16_t *)src;
    uint16_t *d = dst;
    int w = width;

    do {
      int16x8_t s0[8], s1[8], s2[8], s3[8];
      load_s16_8x8(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                   &s0[4], &s0[5], &s0[6], &s0[7]);
      load_s16_8x8(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                   &s1[4], &s1[5], &s1[6], &s1[7]);
      load_s16_8x8(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                   &s2[4], &s2[5], &s2[6], &s2[7]);
      load_s16_8x8(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                   &s3[4], &s3[5], &s3[6], &s3[7]);

      uint16x8_t d0 = convolve8_8_x(s0, filter, offset_lo, max);
      uint16x8_t d1 = convolve8_8_x(s1, filter, offset_lo, max);
      uint16x8_t d2 = convolve8_8_x(s2, filter, offset_lo, max);
      uint16x8_t d3 = convolve8_8_x(s3, filter, offset_lo, max);

      store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

      s += 8;
      d += 8;
      w -= 8;
    } while (w != 0);
    src += 4 * src_stride;
    dst += 4 * dst_stride;
    height -= 4;
  } while (height != 0);
}

// clang-format off
DECLARE_ALIGNED(16, static const uint16_t, kDeinterleaveTbl[8]) = {
  0, 2, 4, 6, 1, 3, 5, 7,
};
// clang-format on

static inline uint16x4_t convolve4_4_x(int16x8_t s0, int16x8_t filter,
                                       int64x2_t offset,
                                       uint16x8x2_t permute_tbl,
                                       uint16x4_t max) {
  int16x8_t permuted_samples0 = aom_tbl_s16(s0, permute_tbl.val[0]);
  int16x8_t permuted_samples1 = aom_tbl_s16(s0, permute_tbl.val[1]);

  int64x2_t sum01 = aom_svdot_lane_s16(offset, permuted_samples0, filter, 0);
  int64x2_t sum23 = aom_svdot_lane_s16(offset, permuted_samples1, filter, 0);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));
  uint16x4_t res = vqrshrun_n_s32(sum0123, FILTER_BITS);

  return vmin_u16(res, max);
}

static inline uint16x8_t convolve4_8_x(int16x8_t s0[4], int16x8_t filter,
                                       int64x2_t offset, uint16x8_t tbl,
                                       uint16x8_t max) {
  int64x2_t sum04 = aom_svdot_lane_s16(offset, s0[0], filter, 0);
  int64x2_t sum15 = aom_svdot_lane_s16(offset, s0[1], filter, 0);
  int64x2_t sum26 = aom_svdot_lane_s16(offset, s0[2], filter, 0);
  int64x2_t sum37 = aom_svdot_lane_s16(offset, s0[3], filter, 0);

  int32x4_t sum0415 = vcombine_s32(vmovn_s64(sum04), vmovn_s64(sum15));
  int32x4_t sum2637 = vcombine_s32(vmovn_s64(sum26), vmovn_s64(sum37));

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(sum0415, FILTER_BITS),
                                vqrshrun_n_s32(sum2637, FILTER_BITS));
  res = aom_tbl_u16(res, tbl);

  return vminq_u16(res, max);
}

static inline void highbd_convolve_x_sr_4tap_sve2(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride,
    int width, int height, const int16_t *x_filter_ptr,
    ConvolveParams *conv_params, int bd) {
  // This shim allows to do only one rounding shift instead of two.
  const int64x2_t offset = vdupq_n_s64(1 << (conv_params->round_0 - 1));

  const int16x4_t x_filter = vld1_s16(x_filter_ptr + 2);
  const int16x8_t filter = vcombine_s16(x_filter, vdup_n_s16(0));

  if (width == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    uint16x8x2_t permute_tbl = vld1q_u16_x2(kDotProdTbl);

    const int16_t *s = (const int16_t *)(src);

    do {
      int16x8_t s0, s1, s2, s3;
      load_s16_8x4(s, src_stride, &s0, &s1, &s2, &s3);

      uint16x4_t d0 = convolve4_4_x(s0, filter, offset, permute_tbl, max);
      uint16x4_t d1 = convolve4_4_x(s1, filter, offset, permute_tbl, max);
      uint16x4_t d2 = convolve4_4_x(s2, filter, offset, permute_tbl, max);
      uint16x4_t d3 = convolve4_4_x(s3, filter, offset, permute_tbl, max);

      store_u16_4x4(dst, dst_stride, d0, d1, d2, d3);

      s += 4 * src_stride;
      dst += 4 * dst_stride;
      height -= 4;
    } while (height != 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
    uint16x8_t idx = vld1q_u16(kDeinterleaveTbl);

    do {
      const int16_t *s = (const int16_t *)(src);
      uint16_t *d = dst;
      int w = width;

      do {
        int16x8_t s0[4], s1[4], s2[4], s3[4];
        load_s16_8x4(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3]);
        load_s16_8x4(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3]);
        load_s16_8x4(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3]);
        load_s16_8x4(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3]);

        uint16x8_t d0 = convolve4_8_x(s0, filter, offset, idx, max);
        uint16x8_t d1 = convolve4_8_x(s1, filter, offset, idx, max);
        uint16x8_t d2 = convolve4_8_x(s2, filter, offset, idx, max);
        uint16x8_t d3 = convolve4_8_x(s3, filter, offset, idx, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        w -= 8;
      } while (w != 0);
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      height -= 4;
    } while (height != 0);
  }
}

void av1_highbd_convolve_x_sr_sve2(const uint16_t *src, int src_stride,
                                   uint16_t *dst, int dst_stride, int w, int h,
                                   const InterpFilterParams *filter_params_x,
                                   const int subpel_x_qn,
                                   ConvolveParams *conv_params, int bd) {
  if (w == 2 || h == 2) {
    av1_highbd_convolve_x_sr_c(src, src_stride, dst, dst_stride, w, h,
                               filter_params_x, subpel_x_qn, conv_params, bd);
    return;
  }

  const int x_filter_taps = get_filter_tap(filter_params_x, subpel_x_qn);

  if (x_filter_taps == 6) {
    av1_highbd_convolve_x_sr_neon(src, src_stride, dst, dst_stride, w, h,
                                  filter_params_x, subpel_x_qn, conv_params,
                                  bd);
    return;
  }

  const int horiz_offset = filter_params_x->taps / 2 - 1;
  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);

  src -= horiz_offset;

  if (x_filter_taps == 12) {
    highbd_convolve_x_sr_12tap_sve2(src, src_stride, dst, dst_stride, w, h,
                                    x_filter_ptr, conv_params, bd);
    return;
  }

  if (x_filter_taps == 8) {
    highbd_convolve_x_sr_8tap_sve2(src, src_stride, dst, dst_stride, w, h,
                                   x_filter_ptr, conv_params, bd);
    return;
  }

  highbd_convolve_x_sr_4tap_sve2(src + 2, src_stride, dst, dst_stride, w, h,
                                 x_filter_ptr, conv_params, bd);
}

static inline uint16x4_t highbd_convolve12_4_y(int16x8_t s0[2], int16x8_t s1[2],
                                               int16x8_t s2[2],
                                               int16x8_t filter_0_7,
                                               int16x8_t filter_4_11,
                                               uint16x4_t max) {
  int64x2_t sum[2];

  sum[0] = aom_svdot_lane_s16(vdupq_n_s64(0), s0[0], filter_0_7, 0);
  sum[0] = aom_svdot_lane_s16(sum[0], s1[0], filter_0_7, 1);
  sum[0] = aom_svdot_lane_s16(sum[0], s2[0], filter_4_11, 1);

  sum[1] = aom_svdot_lane_s16(vdupq_n_s64(0), s0[1], filter_0_7, 0);
  sum[1] = aom_svdot_lane_s16(sum[1], s1[1], filter_0_7, 1);
  sum[1] = aom_svdot_lane_s16(sum[1], s2[1], filter_4_11, 1);

  int32x4_t res_s32 = vcombine_s32(vmovn_s64(sum[0]), vmovn_s64(sum[1]));

  uint16x4_t res = vqrshrun_n_s32(res_s32, FILTER_BITS);

  return vmin_u16(res, max);
}

static inline void highbd_convolve_y_sr_12tap_sve2(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride,
    int width, int height, const int16_t *y_filter_ptr, int bd) {
  const int16x8_t y_filter_0_7 = vld1q_s16(y_filter_ptr);
  const int16x8_t y_filter_4_11 = vld1q_s16(y_filter_ptr + 4);

  uint16x8x3_t merge_block_tbl = vld1q_u16_x3(kDotProdMergeBlockTbl);
  // Scale indices by size of the true vector length to avoid reading from an
  // 'undefined' portion of a vector on a system with SVE vectors > 128-bit.
  uint16x8_t correction0 =
      vreinterpretq_u16_u64(vdupq_n_u64(svcnth() * 0x0001000000000000ULL));
  merge_block_tbl.val[0] = vaddq_u16(merge_block_tbl.val[0], correction0);

  uint16x8_t correction1 =
      vreinterpretq_u16_u64(vdupq_n_u64(svcnth() * 0x0001000100000000ULL));
  merge_block_tbl.val[1] = vaddq_u16(merge_block_tbl.val[1], correction1);

  uint16x8_t correction2 =
      vreinterpretq_u16_u64(vdupq_n_u64(svcnth() * 0x0001000100010000ULL));
  merge_block_tbl.val[2] = vaddq_u16(merge_block_tbl.val[2], correction2);

  const uint16x4_t max = vdup_n_u16((1 << bd) - 1);

  do {
    int16_t *s = (int16_t *)src;
    uint16_t *d = dst;
    int h = height;

    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, sA;
    load_s16_4x11(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7, &s8,
                  &s9, &sA);
    s += 11 * src_stride;

    int16x8_t s0123[2], s1234[2], s2345[2], s3456[2], s4567[2], s5678[2],
        s6789[2], s789A[2];
    transpose_concat_4x4(s0, s1, s2, s3, s0123);
    transpose_concat_4x4(s1, s2, s3, s4, s1234);
    transpose_concat_4x4(s2, s3, s4, s5, s2345);
    transpose_concat_4x4(s3, s4, s5, s6, s3456);
    transpose_concat_4x4(s4, s5, s6, s7, s4567);
    transpose_concat_4x4(s5, s6, s7, s8, s5678);
    transpose_concat_4x4(s6, s7, s8, s9, s6789);
    transpose_concat_4x4(s7, s8, s9, sA, s789A);

    do {
      int16x4_t sB, sC, sD, sE;
      load_s16_4x4(s, src_stride, &sB, &sC, &sD, &sE);

      int16x8_t s89AB[2], s9ABC[2], sABCD[2], sBCDE[2];
      transpose_concat_4x4(sB, sC, sD, sE, sBCDE);

      // Use the above transpose and reuse data from the previous loop to get
      // the rest.
      aom_tbl2x2_s16(s789A, sBCDE, merge_block_tbl.val[0], s89AB);
      aom_tbl2x2_s16(s789A, sBCDE, merge_block_tbl.val[1], s9ABC);
      aom_tbl2x2_s16(s789A, sBCDE, merge_block_tbl.val[2], sABCD);

      uint16x4_t d0 = highbd_convolve12_4_y(s0123, s4567, s89AB, y_filter_0_7,
                                            y_filter_4_11, max);
      uint16x4_t d1 = highbd_convolve12_4_y(s1234, s5678, s9ABC, y_filter_0_7,
                                            y_filter_4_11, max);
      uint16x4_t d2 = highbd_convolve12_4_y(s2345, s6789, sABCD, y_filter_0_7,
                                            y_filter_4_11, max);
      uint16x4_t d3 = highbd_convolve12_4_y(s3456, s789A, sBCDE, y_filter_0_7,
                                            y_filter_4_11, max);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      // Prepare block for next iteration - re-using as much as possible.
      // Shuffle everything up four rows.
      s0123[0] = s4567[0];
      s0123[1] = s4567[1];
      s1234[0] = s5678[0];
      s1234[1] = s5678[1];
      s2345[0] = s6789[0];
      s2345[1] = s6789[1];
      s3456[0] = s789A[0];
      s3456[1] = s789A[1];
      s4567[0] = s89AB[0];
      s4567[1] = s89AB[1];
      s5678[0] = s9ABC[0];
      s5678[1] = s9ABC[1];
      s6789[0] = sABCD[0];
      s6789[1] = sABCD[1];
      s789A[0] = sBCDE[0];
      s789A[1] = sBCDE[1];

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
    src += 4;
    dst += 4;
    width -= 4;
  } while (width != 0);
}

static inline uint16x4_t highbd_convolve8_4_y(int16x8_t samples_lo[2],
                                              int16x8_t samples_hi[2],
                                              int16x8_t filter,
                                              uint16x4_t max) {
  int64x2_t sum01 =
      aom_svdot_lane_s16(vdupq_n_s64(0), samples_lo[0], filter, 0);
  sum01 = aom_svdot_lane_s16(sum01, samples_hi[0], filter, 1);

  int64x2_t sum23 =
      aom_svdot_lane_s16(vdupq_n_s64(0), samples_lo[1], filter, 0);
  sum23 = aom_svdot_lane_s16(sum23, samples_hi[1], filter, 1);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));
  uint16x4_t res = vqrshrun_n_s32(sum0123, FILTER_BITS);
  return vmin_u16(res, max);
}

static inline uint16x8_t highbd_convolve8_8_y(int16x8_t samples_lo[4],
                                              int16x8_t samples_hi[4],
                                              int16x8_t filter,
                                              uint16x8_t max) {
  int64x2_t sum01 =
      aom_svdot_lane_s16(vdupq_n_s64(0), samples_lo[0], filter, 0);
  sum01 = aom_svdot_lane_s16(sum01, samples_hi[0], filter, 1);

  int64x2_t sum23 =
      aom_svdot_lane_s16(vdupq_n_s64(0), samples_lo[1], filter, 0);
  sum23 = aom_svdot_lane_s16(sum23, samples_hi[1], filter, 1);

  int64x2_t sum45 =
      aom_svdot_lane_s16(vdupq_n_s64(0), samples_lo[2], filter, 0);
  sum45 = aom_svdot_lane_s16(sum45, samples_hi[2], filter, 1);

  int64x2_t sum67 =
      aom_svdot_lane_s16(vdupq_n_s64(0), samples_lo[3], filter, 0);
  sum67 = aom_svdot_lane_s16(sum67, samples_hi[3], filter, 1);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));
  int32x4_t sum4567 = vcombine_s32(vmovn_s64(sum45), vmovn_s64(sum67));
  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(sum0123, FILTER_BITS),
                                vqrshrun_n_s32(sum4567, FILTER_BITS));
  return vminq_u16(res, max);
}

static void highbd_convolve_y_sr_8tap_sve2(const uint16_t *src,
                                           ptrdiff_t src_stride, uint16_t *dst,
                                           ptrdiff_t dst_stride, int width,
                                           int height, const int16_t *filter_y,
                                           int bd) {
  assert(width >= 4 && height >= 4);

  const int16x8_t y_filter = vld1q_s16(filter_y);

  uint16x8x3_t merge_block_tbl = vld1q_u16_x3(kDotProdMergeBlockTbl);
  // Scale indices by size of the true vector length to avoid reading from an
  // 'undefined' portion of a vector on a system with SVE vectors > 128-bit.
  uint16x8_t correction0 =
      vreinterpretq_u16_u64(vdupq_n_u64(svcnth() * 0x0001000000000000ULL));
  merge_block_tbl.val[0] = vaddq_u16(merge_block_tbl.val[0], correction0);

  uint16x8_t correction1 =
      vreinterpretq_u16_u64(vdupq_n_u64(svcnth() * 0x0001000100000000ULL));
  merge_block_tbl.val[1] = vaddq_u16(merge_block_tbl.val[1], correction1);

  uint16x8_t correction2 =
      vreinterpretq_u16_u64(vdupq_n_u64(svcnth() * 0x0001000100010000ULL));
  merge_block_tbl.val[2] = vaddq_u16(merge_block_tbl.val[2], correction2);

  if (width == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    int16_t *s = (int16_t *)src;

    int16x4_t s0, s1, s2, s3, s4, s5, s6;
    load_s16_4x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    s += 7 * src_stride;

    // This operation combines a conventional transpose and the sample permute
    // required before computing the dot product.
    int16x8_t s0123[2], s1234[2], s2345[2], s3456[2];
    transpose_concat_4x4(s0, s1, s2, s3, s0123);
    transpose_concat_4x4(s1, s2, s3, s4, s1234);
    transpose_concat_4x4(s2, s3, s4, s5, s2345);
    transpose_concat_4x4(s3, s4, s5, s6, s3456);

    do {
      int16x4_t s7, s8, s9, s10;
      load_s16_4x4(s, src_stride, &s7, &s8, &s9, &s10);

      int16x8_t s4567[2], s5678[2], s6789[2], s789A[2];
      // Transpose and shuffle the 4 lines that were loaded.
      transpose_concat_4x4(s7, s8, s9, s10, s789A);

      // Merge new data into block from previous iteration.
      aom_tbl2x2_s16(s3456, s789A, merge_block_tbl.val[0], s4567);
      aom_tbl2x2_s16(s3456, s789A, merge_block_tbl.val[1], s5678);
      aom_tbl2x2_s16(s3456, s789A, merge_block_tbl.val[2], s6789);

      uint16x4_t d0 = highbd_convolve8_4_y(s0123, s4567, y_filter, max);
      uint16x4_t d1 = highbd_convolve8_4_y(s1234, s5678, y_filter, max);
      uint16x4_t d2 = highbd_convolve8_4_y(s2345, s6789, y_filter, max);
      uint16x4_t d3 = highbd_convolve8_4_y(s3456, s789A, y_filter, max);

      store_u16_4x4(dst, dst_stride, d0, d1, d2, d3);

      // Prepare block for next iteration - re-using as much as possible.
      // Shuffle everything up four rows.
      s0123[0] = s4567[0];
      s0123[1] = s4567[1];
      s1234[0] = s5678[0];
      s1234[1] = s5678[1];
      s2345[0] = s6789[0];
      s2345[1] = s6789[1];
      s3456[0] = s789A[0];
      s3456[1] = s789A[1];
      s += 4 * src_stride;
      dst += 4 * dst_stride;
      height -= 4;
    } while (height != 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

    do {
      int h = height;
      int16_t *s = (int16_t *)src;
      uint16_t *d = dst;

      int16x8_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      // This operation combines a conventional transpose and the sample permute
      // required before computing the dot product.
      int16x8_t s0123[4], s1234[4], s2345[4], s3456[4];
      transpose_concat_8x4(s0, s1, s2, s3, s0123);
      transpose_concat_8x4(s1, s2, s3, s4, s1234);
      transpose_concat_8x4(s2, s3, s4, s5, s2345);
      transpose_concat_8x4(s3, s4, s5, s6, s3456);

      do {
        int16x8_t s7, s8, s9, s10;
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10);

        int16x8_t s4567[4], s5678[4], s6789[4], s789A[4];
        // Transpose and shuffle the 4 lines that were loaded.
        transpose_concat_8x4(s7, s8, s9, s10, s789A);

        // Merge new data into block from previous iteration.
        aom_tbl2x4_s16(s3456, s789A, merge_block_tbl.val[0], s4567);
        aom_tbl2x4_s16(s3456, s789A, merge_block_tbl.val[1], s5678);
        aom_tbl2x4_s16(s3456, s789A, merge_block_tbl.val[2], s6789);

        uint16x8_t d0 = highbd_convolve8_8_y(s0123, s4567, y_filter, max);
        uint16x8_t d1 = highbd_convolve8_8_y(s1234, s5678, y_filter, max);
        uint16x8_t d2 = highbd_convolve8_8_y(s2345, s6789, y_filter, max);
        uint16x8_t d3 = highbd_convolve8_8_y(s3456, s789A, y_filter, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        // Prepare block for next iteration - re-using as much as possible.
        // Shuffle everything up four rows.
        s0123[0] = s4567[0];
        s0123[1] = s4567[1];
        s0123[2] = s4567[2];
        s0123[3] = s4567[3];
        s1234[0] = s5678[0];
        s1234[1] = s5678[1];
        s1234[2] = s5678[2];
        s1234[3] = s5678[3];
        s2345[0] = s6789[0];
        s2345[1] = s6789[1];
        s2345[2] = s6789[2];
        s2345[3] = s6789[3];
        s3456[0] = s789A[0];
        s3456[1] = s789A[1];
        s3456[2] = s789A[2];
        s3456[3] = s789A[3];

        s += 4 * src_stride;
        d += 4 * dst_stride;
        h -= 4;
      } while (h != 0);
      src += 8;
      dst += 8;
      width -= 8;
    } while (width != 0);
  }
}

static inline uint16x4_t highbd_convolve4_4_y(int16x8_t samples[2],
                                              int16x8_t filter,
                                              uint16x4_t max) {
  int64x2_t sum01 = aom_svdot_lane_s16(vdupq_n_s64(0), samples[0], filter, 0);
  int64x2_t sum23 = aom_svdot_lane_s16(vdupq_n_s64(0), samples[1], filter, 0);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));
  uint16x4_t res = vqrshrun_n_s32(sum0123, FILTER_BITS);
  return vmin_u16(res, max);
}

static inline uint16x8_t highbd_convolve4_8_y(int16x8_t samples[4],
                                              int16x8_t filter,
                                              uint16x8_t max) {
  int64x2_t sum01 = aom_svdot_lane_s16(vdupq_n_s64(0), samples[0], filter, 0);
  int64x2_t sum23 = aom_svdot_lane_s16(vdupq_n_s64(0), samples[1], filter, 0);
  int64x2_t sum45 = aom_svdot_lane_s16(vdupq_n_s64(0), samples[2], filter, 0);
  int64x2_t sum67 = aom_svdot_lane_s16(vdupq_n_s64(0), samples[3], filter, 0);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));
  int32x4_t sum4567 = vcombine_s32(vmovn_s64(sum45), vmovn_s64(sum67));
  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(sum0123, FILTER_BITS),
                                vqrshrun_n_s32(sum4567, FILTER_BITS));
  return vminq_u16(res, max);
}

static void highbd_convolve_y_sr_4tap_sve2(const uint16_t *src,
                                           ptrdiff_t src_stride, uint16_t *dst,
                                           ptrdiff_t dst_stride, int width,
                                           int height, const int16_t *filter_y,
                                           int bd) {
  assert(width >= 4 && height >= 4);

  const int16x8_t y_filter =
      vcombine_s16(vld1_s16(filter_y + 2), vdup_n_s16(0));

  if (width == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    int16_t *s = (int16_t *)src;

    int16x4_t s0, s1, s2;
    load_s16_4x3(s, src_stride, &s0, &s1, &s2);
    s += 3 * src_stride;

    do {
      int16x4_t s3, s4, s5, s6;
      load_s16_4x4(s, src_stride, &s3, &s4, &s5, &s6);

      // This operation combines a conventional transpose and the sample permute
      // required before computing the dot product.
      int16x8_t s0123[2], s1234[2], s2345[2], s3456[2];
      transpose_concat_4x4(s0, s1, s2, s3, s0123);
      transpose_concat_4x4(s1, s2, s3, s4, s1234);
      transpose_concat_4x4(s2, s3, s4, s5, s2345);
      transpose_concat_4x4(s3, s4, s5, s6, s3456);

      uint16x4_t d0 = highbd_convolve4_4_y(s0123, y_filter, max);
      uint16x4_t d1 = highbd_convolve4_4_y(s1234, y_filter, max);
      uint16x4_t d2 = highbd_convolve4_4_y(s2345, y_filter, max);
      uint16x4_t d3 = highbd_convolve4_4_y(s3456, y_filter, max);

      store_u16_4x4(dst, dst_stride, d0, d1, d2, d3);

      // Shuffle everything up four rows.
      s0 = s4;
      s1 = s5;
      s2 = s6;

      s += 4 * src_stride;
      dst += 4 * dst_stride;
      height -= 4;
    } while (height != 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

    do {
      int h = height;
      int16_t *s = (int16_t *)src;
      uint16_t *d = dst;

      int16x8_t s0, s1, s2;
      load_s16_8x3(s, src_stride, &s0, &s1, &s2);
      s += 3 * src_stride;

      do {
        int16x8_t s3, s4, s5, s6;
        load_s16_8x4(s, src_stride, &s3, &s4, &s5, &s6);

        // This operation combines a conventional transpose and the sample
        // permute required before computing the dot product.
        int16x8_t s0123[4], s1234[4], s2345[4], s3456[4];
        transpose_concat_8x4(s0, s1, s2, s3, s0123);
        transpose_concat_8x4(s1, s2, s3, s4, s1234);
        transpose_concat_8x4(s2, s3, s4, s5, s2345);
        transpose_concat_8x4(s3, s4, s5, s6, s3456);

        uint16x8_t d0 = highbd_convolve4_8_y(s0123, y_filter, max);
        uint16x8_t d1 = highbd_convolve4_8_y(s1234, y_filter, max);
        uint16x8_t d2 = highbd_convolve4_8_y(s2345, y_filter, max);
        uint16x8_t d3 = highbd_convolve4_8_y(s3456, y_filter, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        // Shuffle everything up four rows.
        s0 = s4;
        s1 = s5;
        s2 = s6;

        s += 4 * src_stride;
        d += 4 * dst_stride;
        h -= 4;
      } while (h != 0);
      src += 8;
      dst += 8;
      width -= 8;
    } while (width != 0);
  }
}

void av1_highbd_convolve_y_sr_sve2(const uint16_t *src, int src_stride,
                                   uint16_t *dst, int dst_stride, int w, int h,
                                   const InterpFilterParams *filter_params_y,
                                   const int subpel_y_qn, int bd) {
  if (w == 2 || h == 2) {
    av1_highbd_convolve_y_sr_c(src, src_stride, dst, dst_stride, w, h,
                               filter_params_y, subpel_y_qn, bd);
    return;
  }
  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);

  if (y_filter_taps == 6) {
    av1_highbd_convolve_y_sr_neon(src, src_stride, dst, dst_stride, w, h,
                                  filter_params_y, subpel_y_qn, bd);
    return;
  }

  const int vert_offset = filter_params_y->taps / 2 - 1;
  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  src -= vert_offset * src_stride;

  if (y_filter_taps > 8) {
    highbd_convolve_y_sr_12tap_sve2(src, src_stride, dst, dst_stride, w, h,
                                    y_filter_ptr, bd);
    return;
  }

  if (y_filter_taps == 4) {
    highbd_convolve_y_sr_4tap_sve2(src + 2 * src_stride, src_stride, dst,
                                   dst_stride, w, h, y_filter_ptr, bd);
    return;
  }

  highbd_convolve_y_sr_8tap_sve2(src, src_stride, dst, dst_stride, w, h,
                                 y_filter_ptr, bd);
}

static inline uint16x4_t convolve12_4_2d_h(
    int16x8_t s0, int16x8_t s1, int16x8_t filter_0_7, int16x8_t filter_4_11,
    const int64x2_t offset, int32x4_t shift, uint16x8x4_t permute_tbl) {
  int16x8_t permuted_samples[6];
  permuted_samples[0] = aom_tbl_s16(s0, permute_tbl.val[0]);
  permuted_samples[1] = aom_tbl_s16(s0, permute_tbl.val[1]);
  permuted_samples[2] = aom_tbl2_s16(s0, s1, permute_tbl.val[2]);
  permuted_samples[3] = aom_tbl2_s16(s0, s1, permute_tbl.val[3]);
  permuted_samples[4] = aom_tbl_s16(s1, permute_tbl.val[0]);
  permuted_samples[5] = aom_tbl_s16(s1, permute_tbl.val[1]);

  int64x2_t sum01 =
      aom_svdot_lane_s16(offset, permuted_samples[0], filter_0_7, 0);
  sum01 = aom_svdot_lane_s16(sum01, permuted_samples[2], filter_0_7, 1);
  sum01 = aom_svdot_lane_s16(sum01, permuted_samples[4], filter_4_11, 1);

  int64x2_t sum23 =
      aom_svdot_lane_s16(offset, permuted_samples[1], filter_0_7, 0);
  sum23 = aom_svdot_lane_s16(sum23, permuted_samples[3], filter_0_7, 1);
  sum23 = aom_svdot_lane_s16(sum23, permuted_samples[5], filter_4_11, 1);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));
  sum0123 = vqrshlq_s32(sum0123, shift);
  return vqmovun_s32(sum0123);
}

static inline uint16x8_t convolve12_8_2d_h(int16x8_t s0, int16x8_t s1,
                                           int16x8_t s2, int16x8_t filter_0_7,
                                           int16x8_t filter_4_11,
                                           int64x2_t offset, int32x4_t shift,
                                           uint16x8x4_t permute_tbl) {
  int16x8_t permuted_samples[8];
  permuted_samples[0] = aom_tbl_s16(s0, permute_tbl.val[0]);
  permuted_samples[1] = aom_tbl_s16(s0, permute_tbl.val[1]);
  permuted_samples[2] = aom_tbl2_s16(s0, s1, permute_tbl.val[2]);
  permuted_samples[3] = aom_tbl2_s16(s0, s1, permute_tbl.val[3]);
  permuted_samples[4] = aom_tbl_s16(s1, permute_tbl.val[0]);
  permuted_samples[5] = aom_tbl_s16(s1, permute_tbl.val[1]);
  permuted_samples[6] = aom_tbl2_s16(s1, s2, permute_tbl.val[2]);
  permuted_samples[7] = aom_tbl2_s16(s1, s2, permute_tbl.val[3]);

  int64x2_t sum01 =
      aom_svdot_lane_s16(offset, permuted_samples[0], filter_0_7, 0);
  sum01 = aom_svdot_lane_s16(sum01, permuted_samples[2], filter_0_7, 1);
  sum01 = aom_svdot_lane_s16(sum01, permuted_samples[4], filter_4_11, 1);

  int64x2_t sum23 =
      aom_svdot_lane_s16(offset, permuted_samples[1], filter_0_7, 0);
  sum23 = aom_svdot_lane_s16(sum23, permuted_samples[3], filter_0_7, 1);
  sum23 = aom_svdot_lane_s16(sum23, permuted_samples[5], filter_4_11, 1);

  int64x2_t sum45 =
      aom_svdot_lane_s16(offset, permuted_samples[2], filter_0_7, 0);
  sum45 = aom_svdot_lane_s16(sum45, permuted_samples[4], filter_0_7, 1);
  sum45 = aom_svdot_lane_s16(sum45, permuted_samples[6], filter_4_11, 1);

  int64x2_t sum67 =
      aom_svdot_lane_s16(offset, permuted_samples[3], filter_0_7, 0);
  sum67 = aom_svdot_lane_s16(sum67, permuted_samples[5], filter_0_7, 1);
  sum67 = aom_svdot_lane_s16(sum67, permuted_samples[7], filter_4_11, 1);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));
  int32x4_t sum4567 = vcombine_s32(vmovn_s64(sum45), vmovn_s64(sum67));

  sum0123 = vqrshlq_s32(sum0123, shift);
  sum4567 = vqrshlq_s32(sum4567, shift);

  return vcombine_u16(vqmovun_s32(sum0123), vqmovun_s32(sum4567));
}

static inline void highbd_convolve_2d_sr_horiz_12tap_sve2(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride,
    int width, int height, const int16_t *y_filter_ptr,
    ConvolveParams *conv_params, const int x_offset) {
  const int64x2_t offset = vdupq_n_s64(x_offset);
  const int32x4_t shift = vdupq_n_s32(-conv_params->round_0);

  const int16x8_t y_filter_0_7 = vld1q_s16(y_filter_ptr);
  const int16x8_t y_filter_4_11 = vld1q_s16(y_filter_ptr + 4);

  uint16x8x4_t permute_tbl = vld1q_u16_x4(kDotProdTbl);
  // Scale indices by size of the true vector length to avoid reading from an
  // 'undefined' portion of a vector on a system with SVE vectors > 128-bit.
  uint16x8_t correction0 = vreinterpretq_u16_u64(vcombine_u64(
      vdup_n_u64(0), vdup_n_u64(svcnth() * 0x0001000000000000ULL)));
  permute_tbl.val[2] = vaddq_u16(permute_tbl.val[2], correction0);

  uint16x8_t correction1 = vreinterpretq_u16_u64(
      vcombine_u64(vdup_n_u64(svcnth() * 0x0001000100000000ULL),
                   vdup_n_u64(svcnth() * 0x0001000100010000ULL)));
  permute_tbl.val[3] = vaddq_u16(permute_tbl.val[3], correction1);

  if (width == 4) {
    const int16_t *s = (const int16_t *)src;

    do {
      int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;
      load_s16_8x4(s, src_stride, &s0, &s2, &s4, &s6);
      load_s16_8x4(s + 8, src_stride, &s1, &s3, &s5, &s7);

      uint16x4_t d0 = convolve12_4_2d_h(s0, s1, y_filter_0_7, y_filter_4_11,
                                        offset, shift, permute_tbl);
      uint16x4_t d1 = convolve12_4_2d_h(s2, s3, y_filter_0_7, y_filter_4_11,
                                        offset, shift, permute_tbl);
      uint16x4_t d2 = convolve12_4_2d_h(s4, s5, y_filter_0_7, y_filter_4_11,
                                        offset, shift, permute_tbl);
      uint16x4_t d3 = convolve12_4_2d_h(s6, s7, y_filter_0_7, y_filter_4_11,
                                        offset, shift, permute_tbl);

      store_u16_4x4(dst, dst_stride, d0, d1, d2, d3);

      dst += 4 * dst_stride;
      s += 4 * src_stride;
      height -= 4;
    } while (height > 0);
  } else {
    do {
      const int16_t *s = (const int16_t *)src;
      uint16_t *d = dst;
      int w = width;

      do {
        int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
        load_s16_8x4(s, src_stride, &s0, &s3, &s6, &s9);
        load_s16_8x4(s + 8, src_stride, &s1, &s4, &s7, &s10);
        load_s16_8x4(s + 16, src_stride, &s2, &s5, &s8, &s11);

        uint16x8_t d0 =
            convolve12_8_2d_h(s0, s1, s2, y_filter_0_7, y_filter_4_11, offset,
                              shift, permute_tbl);
        uint16x8_t d1 =
            convolve12_8_2d_h(s3, s4, s5, y_filter_0_7, y_filter_4_11, offset,
                              shift, permute_tbl);
        uint16x8_t d2 =
            convolve12_8_2d_h(s6, s7, s8, y_filter_0_7, y_filter_4_11, offset,
                              shift, permute_tbl);
        uint16x8_t d3 =
            convolve12_8_2d_h(s9, s10, s11, y_filter_0_7, y_filter_4_11, offset,
                              shift, permute_tbl);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        w -= 8;
      } while (w != 0);
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      height -= 4;
    } while (height > 0);
  }
}

static inline uint16x8_t convolve8_8_2d_h(int16x8_t s0[8], int16x8_t filter,
                                          int64x2_t offset, int32x4_t shift) {
  int64x2_t sum[8];
  sum[0] = aom_sdotq_s16(offset, s0[0], filter);
  sum[1] = aom_sdotq_s16(offset, s0[1], filter);
  sum[2] = aom_sdotq_s16(offset, s0[2], filter);
  sum[3] = aom_sdotq_s16(offset, s0[3], filter);
  sum[4] = aom_sdotq_s16(offset, s0[4], filter);
  sum[5] = aom_sdotq_s16(offset, s0[5], filter);
  sum[6] = aom_sdotq_s16(offset, s0[6], filter);
  sum[7] = aom_sdotq_s16(offset, s0[7], filter);

  sum[0] = vpaddq_s64(sum[0], sum[1]);
  sum[2] = vpaddq_s64(sum[2], sum[3]);
  sum[4] = vpaddq_s64(sum[4], sum[5]);
  sum[6] = vpaddq_s64(sum[6], sum[7]);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum[0]), vmovn_s64(sum[2]));
  int32x4_t sum4567 = vcombine_s32(vmovn_s64(sum[4]), vmovn_s64(sum[6]));

  sum0123 = vqrshlq_s32(sum0123, shift);
  sum4567 = vqrshlq_s32(sum4567, shift);

  return vcombine_u16(vqmovun_s32(sum0123), vqmovun_s32(sum4567));
}

static inline void highbd_convolve_2d_sr_horiz_8tap_sve2(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride,
    int width, int height, const int16_t *y_filter_ptr,
    ConvolveParams *conv_params, const int x_offset) {
  const int64x2_t offset = vdupq_n_s64(x_offset);
  const int64x2_t offset_lo = vcombine_s64(vget_low_s64(offset), vdup_n_s64(0));
  const int32x4_t shift = vdupq_n_s32(-conv_params->round_0);

  const int16x8_t filter = vld1q_s16(y_filter_ptr);

  do {
    const int16_t *s = (const int16_t *)src;
    uint16_t *d = dst;
    int w = width;

    do {
      int16x8_t s0[8], s1[8], s2[8], s3[8];
      load_s16_8x8(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                   &s0[4], &s0[5], &s0[6], &s0[7]);
      load_s16_8x8(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                   &s1[4], &s1[5], &s1[6], &s1[7]);
      load_s16_8x8(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                   &s2[4], &s2[5], &s2[6], &s2[7]);
      load_s16_8x8(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                   &s3[4], &s3[5], &s3[6], &s3[7]);

      uint16x8_t d0 = convolve8_8_2d_h(s0, filter, offset_lo, shift);
      uint16x8_t d1 = convolve8_8_2d_h(s1, filter, offset_lo, shift);
      uint16x8_t d2 = convolve8_8_2d_h(s2, filter, offset_lo, shift);
      uint16x8_t d3 = convolve8_8_2d_h(s3, filter, offset_lo, shift);

      store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

      s += 8;
      d += 8;
      w -= 8;
    } while (w != 0);
    src += 4 * src_stride;
    dst += 4 * dst_stride;
    height -= 4;
  } while (height > 0);
}

static inline uint16x4_t convolve4_4_2d_h(int16x8_t s0, int16x8_t filter,
                                          int64x2_t offset, int32x4_t shift,
                                          uint16x8x2_t permute_tbl) {
  int16x8_t permuted_samples0 = aom_tbl_s16(s0, permute_tbl.val[0]);
  int16x8_t permuted_samples1 = aom_tbl_s16(s0, permute_tbl.val[1]);

  int64x2_t sum01 = aom_svdot_lane_s16(offset, permuted_samples0, filter, 0);
  int64x2_t sum23 = aom_svdot_lane_s16(offset, permuted_samples1, filter, 0);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));
  sum0123 = vqrshlq_s32(sum0123, shift);
  return vqmovun_s32(sum0123);
}

static inline uint16x8_t convolve4_8_2d_h(int16x8_t s0[8], int16x8_t filter,
                                          int64x2_t offset, int32x4_t shift,
                                          uint16x8_t tbl) {
  int64x2_t sum04 = aom_svdot_lane_s16(offset, s0[0], filter, 0);
  int64x2_t sum15 = aom_svdot_lane_s16(offset, s0[1], filter, 0);
  int64x2_t sum26 = aom_svdot_lane_s16(offset, s0[2], filter, 0);
  int64x2_t sum37 = aom_svdot_lane_s16(offset, s0[3], filter, 0);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum04), vmovn_s64(sum15));
  int32x4_t sum4567 = vcombine_s32(vmovn_s64(sum26), vmovn_s64(sum37));

  sum0123 = vqrshlq_s32(sum0123, shift);
  sum4567 = vqrshlq_s32(sum4567, shift);

  uint16x8_t res = vcombine_u16(vqmovun_s32(sum0123), vqmovun_s32(sum4567));
  return aom_tbl_u16(res, tbl);
}

static inline void highbd_convolve_2d_sr_horiz_4tap_sve2(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride,
    int width, int height, const int16_t *x_filter_ptr,
    ConvolveParams *conv_params, const int x_offset) {
  const int64x2_t offset = vdupq_n_s64(x_offset);
  const int32x4_t shift = vdupq_n_s32(-conv_params->round_0);

  const int16x4_t x_filter = vld1_s16(x_filter_ptr + 2);
  const int16x8_t filter = vcombine_s16(x_filter, vdup_n_s16(0));

  if (width == 4) {
    const int16_t *s = (const int16_t *)(src);

    uint16x8x2_t permute_tbl = vld1q_u16_x2(kDotProdTbl);

    do {
      int16x8_t s0, s1, s2, s3;
      load_s16_8x4(s, src_stride, &s0, &s1, &s2, &s3);

      uint16x4_t d0 = convolve4_4_2d_h(s0, filter, offset, shift, permute_tbl);
      uint16x4_t d1 = convolve4_4_2d_h(s1, filter, offset, shift, permute_tbl);
      uint16x4_t d2 = convolve4_4_2d_h(s2, filter, offset, shift, permute_tbl);
      uint16x4_t d3 = convolve4_4_2d_h(s3, filter, offset, shift, permute_tbl);

      store_u16_4x4(dst, dst_stride, d0, d1, d2, d3);

      s += 4 * src_stride;
      dst += 4 * dst_stride;
      height -= 4;
    } while (height > 0);
  } else {
    uint16x8_t idx = vld1q_u16(kDeinterleaveTbl);

    do {
      const int16_t *s = (const int16_t *)(src);
      uint16_t *d = dst;
      int w = width;

      do {
        int16x8_t s0[8], s1[8], s2[8], s3[8];
        load_s16_8x8(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                     &s0[4], &s0[5], &s0[6], &s0[7]);
        load_s16_8x8(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                     &s1[4], &s1[5], &s1[6], &s1[7]);
        load_s16_8x8(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                     &s2[4], &s2[5], &s2[6], &s2[7]);
        load_s16_8x8(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                     &s3[4], &s3[5], &s3[6], &s3[7]);

        uint16x8_t d0 = convolve4_8_2d_h(s0, filter, offset, shift, idx);
        uint16x8_t d1 = convolve4_8_2d_h(s1, filter, offset, shift, idx);
        uint16x8_t d2 = convolve4_8_2d_h(s2, filter, offset, shift, idx);
        uint16x8_t d3 = convolve4_8_2d_h(s3, filter, offset, shift, idx);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        w -= 8;
      } while (w != 0);
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      height -= 4;
    } while (height > 0);
  }
}

static inline uint16x4_t highbd_convolve12_4_2d_v(
    int16x8_t s0[2], int16x8_t s1[2], int16x8_t s2[2], int16x8_t filter_0_7,
    int16x8_t filter_4_11, int32x4_t shift, int64x2_t offset, uint16x4_t max) {
  int64x2_t sum01 = aom_svdot_lane_s16(offset, s0[0], filter_0_7, 0);
  sum01 = aom_svdot_lane_s16(sum01, s1[0], filter_0_7, 1);
  sum01 = aom_svdot_lane_s16(sum01, s2[0], filter_4_11, 1);

  int64x2_t sum23 = aom_svdot_lane_s16(offset, s0[1], filter_0_7, 0);
  sum23 = aom_svdot_lane_s16(sum23, s1[1], filter_0_7, 1);
  sum23 = aom_svdot_lane_s16(sum23, s2[1], filter_4_11, 1);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));
  sum0123 = vshlq_s32(sum0123, shift);

  uint16x4_t res = vqmovun_s32(sum0123);

  return vmin_u16(res, max);
}

static inline void highbd_convolve_2d_sr_vert_12tap_sve2(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride,
    int width, int height, const int16_t *y_filter_ptr,
    ConvolveParams *conv_params, int bd, const int y_offset) {
  const int64x2_t offset = vdupq_n_s64(y_offset);
  const int32x4_t shift = vdupq_n_s32(-conv_params->round_1);

  const int16x8_t y_filter_0_7 = vld1q_s16(y_filter_ptr);
  const int16x8_t y_filter_4_11 = vld1q_s16(y_filter_ptr + 4);

  uint16x8x3_t merge_block_tbl = vld1q_u16_x3(kDotProdMergeBlockTbl);
  // Scale indices by size of the true vector length to avoid reading from an
  // 'undefined' portion of a vector on a system with SVE vectors > 128-bit.
  uint16x8_t correction0 =
      vreinterpretq_u16_u64(vdupq_n_u64(svcnth() * 0x0001000000000000ULL));
  merge_block_tbl.val[0] = vaddq_u16(merge_block_tbl.val[0], correction0);

  uint16x8_t correction1 =
      vreinterpretq_u16_u64(vdupq_n_u64(svcnth() * 0x0001000100000000ULL));
  merge_block_tbl.val[1] = vaddq_u16(merge_block_tbl.val[1], correction1);

  uint16x8_t correction2 =
      vreinterpretq_u16_u64(vdupq_n_u64(svcnth() * 0x0001000100010000ULL));
  merge_block_tbl.val[2] = vaddq_u16(merge_block_tbl.val[2], correction2);

  const uint16x4_t max = vdup_n_u16((1 << bd) - 1);

  do {
    int16_t *s = (int16_t *)src;
    uint16_t *d = (uint16_t *)dst;
    int h = height;

    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, sA;
    load_s16_4x11(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7, &s8,
                  &s9, &sA);
    s += 11 * src_stride;

    int16x8_t s0123[2], s1234[2], s2345[2], s3456[2], s4567[2], s5678[2],
        s6789[2], s789A[2];
    // This operation combines a conventional transpose and the sample permute
    // required before computing the dot product.
    transpose_concat_4x4(s0, s1, s2, s3, s0123);
    transpose_concat_4x4(s1, s2, s3, s4, s1234);
    transpose_concat_4x4(s2, s3, s4, s5, s2345);
    transpose_concat_4x4(s3, s4, s5, s6, s3456);
    transpose_concat_4x4(s4, s5, s6, s7, s4567);
    transpose_concat_4x4(s5, s6, s7, s8, s5678);
    transpose_concat_4x4(s6, s7, s8, s9, s6789);
    transpose_concat_4x4(s7, s8, s9, sA, s789A);

    do {
      int16x4_t sB, sC, sD, sE;
      load_s16_4x4(s, src_stride, &sB, &sC, &sD, &sE);

      int16x8_t s89AB[2], s9ABC[2], sABCD[2], sBCDE[2];
      transpose_concat_4x4(sB, sC, sD, sE, sBCDE);

      // Use the above transpose and reuse data from the previous loop to get
      // the rest.
      aom_tbl2x2_s16(s789A, sBCDE, merge_block_tbl.val[0], s89AB);
      aom_tbl2x2_s16(s789A, sBCDE, merge_block_tbl.val[1], s9ABC);
      aom_tbl2x2_s16(s789A, sBCDE, merge_block_tbl.val[2], sABCD);

      uint16x4_t d0 = highbd_convolve12_4_2d_v(
          s0123, s4567, s89AB, y_filter_0_7, y_filter_4_11, shift, offset, max);
      uint16x4_t d1 = highbd_convolve12_4_2d_v(
          s1234, s5678, s9ABC, y_filter_0_7, y_filter_4_11, shift, offset, max);
      uint16x4_t d2 = highbd_convolve12_4_2d_v(
          s2345, s6789, sABCD, y_filter_0_7, y_filter_4_11, shift, offset, max);
      uint16x4_t d3 = highbd_convolve12_4_2d_v(
          s3456, s789A, sBCDE, y_filter_0_7, y_filter_4_11, shift, offset, max);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      // Prepare block for next iteration - re-using as much as possible.
      // Shuffle everything up four rows.
      s0123[0] = s4567[0];
      s0123[1] = s4567[1];
      s1234[0] = s5678[0];
      s1234[1] = s5678[1];
      s2345[0] = s6789[0];
      s2345[1] = s6789[1];
      s3456[0] = s789A[0];
      s3456[1] = s789A[1];
      s4567[0] = s89AB[0];
      s4567[1] = s89AB[1];
      s5678[0] = s9ABC[0];
      s5678[1] = s9ABC[1];
      s6789[0] = sABCD[0];
      s6789[1] = sABCD[1];
      s789A[0] = sBCDE[0];
      s789A[1] = sBCDE[1];

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
    src += 4;
    dst += 4;
    width -= 4;
  } while (width != 0);
}

static inline uint16x4_t highbd_convolve8_4_2d_v(
    int16x8_t samples_lo[2], int16x8_t samples_hi[2], int16x8_t filter,
    int32x4_t shift, int64x2_t offset, uint16x4_t max) {
  int64x2_t sum01 = aom_svdot_lane_s16(offset, samples_lo[0], filter, 0);
  sum01 = aom_svdot_lane_s16(sum01, samples_hi[0], filter, 1);

  int64x2_t sum23 = aom_svdot_lane_s16(offset, samples_lo[1], filter, 0);
  sum23 = aom_svdot_lane_s16(sum23, samples_hi[1], filter, 1);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));
  sum0123 = vshlq_s32(sum0123, shift);

  uint16x4_t res = vqmovun_s32(sum0123);
  return vmin_u16(res, max);
}

static inline uint16x8_t highbd_convolve8_8_2d_v(
    int16x8_t samples_lo[4], int16x8_t samples_hi[4], int16x8_t filter,
    int32x4_t shift, int64x2_t offset, uint16x8_t max) {
  int64x2_t sum01 = aom_svdot_lane_s16(offset, samples_lo[0], filter, 0);
  sum01 = aom_svdot_lane_s16(sum01, samples_hi[0], filter, 1);

  int64x2_t sum23 = aom_svdot_lane_s16(offset, samples_lo[1], filter, 0);
  sum23 = aom_svdot_lane_s16(sum23, samples_hi[1], filter, 1);

  int64x2_t sum45 = aom_svdot_lane_s16(offset, samples_lo[2], filter, 0);
  sum45 = aom_svdot_lane_s16(sum45, samples_hi[2], filter, 1);

  int64x2_t sum67 = aom_svdot_lane_s16(offset, samples_lo[3], filter, 0);
  sum67 = aom_svdot_lane_s16(sum67, samples_hi[3], filter, 1);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));
  int32x4_t sum4567 = vcombine_s32(vmovn_s64(sum45), vmovn_s64(sum67));

  sum0123 = vshlq_s32(sum0123, shift);
  sum4567 = vshlq_s32(sum4567, shift);

  uint16x8_t res = vcombine_u16(vqmovun_s32(sum0123), vqmovun_s32(sum4567));
  return vminq_u16(res, max);
}

static void highbd_convolve_2d_sr_vert_8tap_sve2(
    const uint16_t *src, ptrdiff_t src_stride, uint16_t *dst,
    ptrdiff_t dst_stride, int width, int height, const int16_t *filter_y,
    ConvolveParams *conv_params, int bd, const int y_offset) {
  assert(width >= 4 && height >= 4);
  const int64x2_t offset = vdupq_n_s64(y_offset);
  const int32x4_t shift = vdupq_n_s32(-conv_params->round_1);
  const int16x8_t y_filter = vld1q_s16(filter_y);

  uint16x8x3_t merge_block_tbl = vld1q_u16_x3(kDotProdMergeBlockTbl);
  // Scale indices by size of the true vector length to avoid reading from an
  // 'undefined' portion of a vector on a system with SVE vectors > 128-bit.
  uint16x8_t correction0 =
      vreinterpretq_u16_u64(vdupq_n_u64(svcnth() * 0x0001000000000000ULL));
  merge_block_tbl.val[0] = vaddq_u16(merge_block_tbl.val[0], correction0);

  uint16x8_t correction1 =
      vreinterpretq_u16_u64(vdupq_n_u64(svcnth() * 0x0001000100000000ULL));
  merge_block_tbl.val[1] = vaddq_u16(merge_block_tbl.val[1], correction1);

  uint16x8_t correction2 =
      vreinterpretq_u16_u64(vdupq_n_u64(svcnth() * 0x0001000100010000ULL));
  merge_block_tbl.val[2] = vaddq_u16(merge_block_tbl.val[2], correction2);

  if (width == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    int16_t *s = (int16_t *)src;

    int16x4_t s0, s1, s2, s3, s4, s5, s6;
    load_s16_4x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    s += 7 * src_stride;

    // This operation combines a conventional transpose and the sample permute
    // required before computing the dot product.
    int16x8_t s0123[2], s1234[2], s2345[2], s3456[2];
    transpose_concat_4x4(s0, s1, s2, s3, s0123);
    transpose_concat_4x4(s1, s2, s3, s4, s1234);
    transpose_concat_4x4(s2, s3, s4, s5, s2345);
    transpose_concat_4x4(s3, s4, s5, s6, s3456);

    do {
      int16x4_t s7, s8, s9, s10;
      load_s16_4x4(s, src_stride, &s7, &s8, &s9, &s10);

      int16x8_t s4567[2], s5678[2], s6789[2], s789A[2];
      // Transpose and shuffle the 4 lines that were loaded.
      transpose_concat_4x4(s7, s8, s9, s10, s789A);

      // Merge new data into block from previous iteration.
      aom_tbl2x2_s16(s3456, s789A, merge_block_tbl.val[0], s4567);
      aom_tbl2x2_s16(s3456, s789A, merge_block_tbl.val[1], s5678);
      aom_tbl2x2_s16(s3456, s789A, merge_block_tbl.val[2], s6789);

      uint16x4_t d0 =
          highbd_convolve8_4_2d_v(s0123, s4567, y_filter, shift, offset, max);
      uint16x4_t d1 =
          highbd_convolve8_4_2d_v(s1234, s5678, y_filter, shift, offset, max);
      uint16x4_t d2 =
          highbd_convolve8_4_2d_v(s2345, s6789, y_filter, shift, offset, max);
      uint16x4_t d3 =
          highbd_convolve8_4_2d_v(s3456, s789A, y_filter, shift, offset, max);

      store_u16_4x4(dst, dst_stride, d0, d1, d2, d3);

      // Prepare block for next iteration - re-using as much as possible.
      // Shuffle everything up four rows.
      s0123[0] = s4567[0];
      s0123[1] = s4567[1];
      s1234[0] = s5678[0];
      s1234[1] = s5678[1];
      s2345[0] = s6789[0];
      s2345[1] = s6789[1];
      s3456[0] = s789A[0];
      s3456[1] = s789A[1];

      s += 4 * src_stride;
      dst += 4 * dst_stride;
      height -= 4;
    } while (height != 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

    do {
      int h = height;
      int16_t *s = (int16_t *)src;
      uint16_t *d = dst;

      int16x8_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      // This operation combines a conventional transpose and the sample permute
      // required before computing the dot product.
      int16x8_t s0123[4], s1234[4], s2345[4], s3456[4];
      transpose_concat_8x4(s0, s1, s2, s3, s0123);
      transpose_concat_8x4(s1, s2, s3, s4, s1234);
      transpose_concat_8x4(s2, s3, s4, s5, s2345);
      transpose_concat_8x4(s3, s4, s5, s6, s3456);

      do {
        int16x8_t s7, s8, s9, s10;
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10);

        int16x8_t s4567[4], s5678[4], s6789[4], s789A[4];
        // Transpose and shuffle the 4 lines that were loaded.
        transpose_concat_8x4(s7, s8, s9, s10, s789A);

        // Merge new data into block from previous iteration.
        aom_tbl2x4_s16(s3456, s789A, merge_block_tbl.val[0], s4567);
        aom_tbl2x4_s16(s3456, s789A, merge_block_tbl.val[1], s5678);
        aom_tbl2x4_s16(s3456, s789A, merge_block_tbl.val[2], s6789);

        uint16x8_t d0 =
            highbd_convolve8_8_2d_v(s0123, s4567, y_filter, shift, offset, max);
        uint16x8_t d1 =
            highbd_convolve8_8_2d_v(s1234, s5678, y_filter, shift, offset, max);
        uint16x8_t d2 =
            highbd_convolve8_8_2d_v(s2345, s6789, y_filter, shift, offset, max);
        uint16x8_t d3 =
            highbd_convolve8_8_2d_v(s3456, s789A, y_filter, shift, offset, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        // Prepare block for next iteration - re-using as much as possible.
        // Shuffle everything up four rows.
        s0123[0] = s4567[0];
        s0123[1] = s4567[1];
        s0123[2] = s4567[2];
        s0123[3] = s4567[3];
        s1234[0] = s5678[0];
        s1234[1] = s5678[1];
        s1234[2] = s5678[2];
        s1234[3] = s5678[3];
        s2345[0] = s6789[0];
        s2345[1] = s6789[1];
        s2345[2] = s6789[2];
        s2345[3] = s6789[3];
        s3456[0] = s789A[0];
        s3456[1] = s789A[1];
        s3456[2] = s789A[2];
        s3456[3] = s789A[3];

        s += 4 * src_stride;
        d += 4 * dst_stride;
        h -= 4;
      } while (h != 0);
      src += 8;
      dst += 8;
      width -= 8;
    } while (width != 0);
  }
}

static inline uint16x4_t highbd_convolve4_4_2d_v(int16x8_t samples[2],
                                                 int16x8_t filter,
                                                 int32x4_t shift,
                                                 int64x2_t offset,
                                                 uint16x4_t max) {
  int64x2_t sum01 = aom_svdot_lane_s16(offset, samples[0], filter, 0);
  int64x2_t sum23 = aom_svdot_lane_s16(offset, samples[1], filter, 0);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));
  sum0123 = vshlq_s32(sum0123, shift);

  uint16x4_t res = vqmovun_s32(sum0123);
  return vmin_u16(res, max);
}

static inline uint16x8_t highbd_convolve4_8_2d_v(int16x8_t samples[4],
                                                 int16x8_t filter,
                                                 int32x4_t shift,
                                                 int64x2_t offset,
                                                 uint16x8_t max) {
  int64x2_t sum01 = aom_svdot_lane_s16(offset, samples[0], filter, 0);
  int64x2_t sum23 = aom_svdot_lane_s16(offset, samples[1], filter, 0);
  int64x2_t sum45 = aom_svdot_lane_s16(offset, samples[2], filter, 0);
  int64x2_t sum67 = aom_svdot_lane_s16(offset, samples[3], filter, 0);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));
  int32x4_t sum4567 = vcombine_s32(vmovn_s64(sum45), vmovn_s64(sum67));

  sum0123 = vshlq_s32(sum0123, shift);
  sum4567 = vshlq_s32(sum4567, shift);

  uint16x8_t res = vcombine_u16(vqmovun_s32(sum0123), vqmovun_s32(sum4567));
  return vminq_u16(res, max);
}

static void highbd_convolve_2d_sr_vert_4tap_sve2(
    const uint16_t *src, ptrdiff_t src_stride, uint16_t *dst,
    ptrdiff_t dst_stride, int width, int height, const int16_t *filter_y,
    ConvolveParams *conv_params, int bd, const int y_offset) {
  assert(width >= 4 && height >= 4);
  const int64x2_t offset = vdupq_n_s64(y_offset);
  const int32x4_t shift = vdupq_n_s32(-conv_params->round_1);

  const int16x8_t y_filter =
      vcombine_s16(vld1_s16(filter_y + 2), vdup_n_s16(0));

  if (width == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    int16_t *s = (int16_t *)(src);

    int16x4_t s0, s1, s2;
    load_s16_4x3(s, src_stride, &s0, &s1, &s2);
    s += 3 * src_stride;

    do {
      int16x4_t s3, s4, s5, s6;
      load_s16_4x4(s, src_stride, &s3, &s4, &s5, &s6);

      // This operation combines a conventional transpose and the sample permute
      // required before computing the dot product.
      int16x8_t s0123[2], s1234[2], s2345[2], s3456[2];
      transpose_concat_4x4(s0, s1, s2, s3, s0123);
      transpose_concat_4x4(s1, s2, s3, s4, s1234);
      transpose_concat_4x4(s2, s3, s4, s5, s2345);
      transpose_concat_4x4(s3, s4, s5, s6, s3456);

      uint16x4_t d0 =
          highbd_convolve4_4_2d_v(s0123, y_filter, shift, offset, max);
      uint16x4_t d1 =
          highbd_convolve4_4_2d_v(s1234, y_filter, shift, offset, max);
      uint16x4_t d2 =
          highbd_convolve4_4_2d_v(s2345, y_filter, shift, offset, max);
      uint16x4_t d3 =
          highbd_convolve4_4_2d_v(s3456, y_filter, shift, offset, max);

      store_u16_4x4(dst, dst_stride, d0, d1, d2, d3);

      // Shuffle everything up four rows.
      s0 = s4;
      s1 = s5;
      s2 = s6;

      s += 4 * src_stride;
      dst += 4 * dst_stride;
      height -= 4;
    } while (height != 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

    do {
      int h = height;
      int16_t *s = (int16_t *)(src);
      uint16_t *d = dst;

      int16x8_t s0, s1, s2;
      load_s16_8x3(s, src_stride, &s0, &s1, &s2);
      s += 3 * src_stride;

      do {
        int16x8_t s3, s4, s5, s6;
        load_s16_8x4(s, src_stride, &s3, &s4, &s5, &s6);

        // This operation combines a conventional transpose and the sample
        // permute required before computing the dot product.
        int16x8_t s0123[4], s1234[4], s2345[4], s3456[4];
        transpose_concat_8x4(s0, s1, s2, s3, s0123);
        transpose_concat_8x4(s1, s2, s3, s4, s1234);
        transpose_concat_8x4(s2, s3, s4, s5, s2345);
        transpose_concat_8x4(s3, s4, s5, s6, s3456);

        uint16x8_t d0 =
            highbd_convolve4_8_2d_v(s0123, y_filter, shift, offset, max);
        uint16x8_t d1 =
            highbd_convolve4_8_2d_v(s1234, y_filter, shift, offset, max);
        uint16x8_t d2 =
            highbd_convolve4_8_2d_v(s2345, y_filter, shift, offset, max);
        uint16x8_t d3 =
            highbd_convolve4_8_2d_v(s3456, y_filter, shift, offset, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        // Shuffle everything up four rows.
        s0 = s4;
        s1 = s5;
        s2 = s6;

        s += 4 * src_stride;
        d += 4 * dst_stride;
        h -= 4;
      } while (h != 0);
      src += 8;
      dst += 8;
      width -= 8;
    } while (width != 0);
  }
}

void av1_highbd_convolve_2d_sr_sve2(const uint16_t *src, int src_stride,
                                    uint16_t *dst, int dst_stride, int w, int h,
                                    const InterpFilterParams *filter_params_x,
                                    const InterpFilterParams *filter_params_y,
                                    const int subpel_x_qn,
                                    const int subpel_y_qn,
                                    ConvolveParams *conv_params, int bd) {
  if (w == 2 || h == 2) {
    av1_highbd_convolve_2d_sr_c(src, src_stride, dst, dst_stride, w, h,
                                filter_params_x, filter_params_y, subpel_x_qn,
                                subpel_y_qn, conv_params, bd);
    return;
  }

  DECLARE_ALIGNED(16, uint16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE]);
  const int x_filter_taps = get_filter_tap(filter_params_x, subpel_x_qn);
  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);

  if (x_filter_taps == 6 || y_filter_taps == 6) {
    av1_highbd_convolve_2d_sr_neon(src, src_stride, dst, dst_stride, w, h,
                                   filter_params_x, filter_params_y,
                                   subpel_x_qn, subpel_y_qn, conv_params, bd);
    return;
  }

  const int clamped_x_taps = x_filter_taps < 4 ? 4 : x_filter_taps;
  const int clamped_y_taps = y_filter_taps < 4 ? 4 : y_filter_taps;

  const int im_stride = MAX_SB_SIZE;
  const int vert_offset = clamped_y_taps / 2 - 1;
  const int horiz_offset = clamped_x_taps / 2 - 1;
  const int x_offset = (1 << (bd + FILTER_BITS - 1));
  const int y_offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  // The extra shim of (1 << (conv_params->round_1 - 1)) allows us to do a
  // simple shift left instead of a rounding saturating shift left.
  const int y_offset =
      (1 << (conv_params->round_1 - 1)) - (1 << (y_offset_bits - 1));

  const uint16_t *src_ptr = src - vert_offset * src_stride - horiz_offset;

  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);
  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);
  const int im_h = h + clamped_y_taps - 1;

  if (x_filter_taps > 8) {
    highbd_convolve_2d_sr_horiz_12tap_sve2(src_ptr, src_stride, im_block,
                                           im_stride, w, im_h, x_filter_ptr,
                                           conv_params, x_offset);

    highbd_convolve_2d_sr_vert_12tap_sve2(im_block, im_stride, dst, dst_stride,
                                          w, h, y_filter_ptr, conv_params, bd,
                                          y_offset);
    return;
  }

  if (x_filter_taps <= 4) {
    highbd_convolve_2d_sr_horiz_4tap_sve2(src_ptr, src_stride, im_block,
                                          im_stride, w, im_h, x_filter_ptr,
                                          conv_params, x_offset);
  } else {
    highbd_convolve_2d_sr_horiz_8tap_sve2(src_ptr, src_stride, im_block,
                                          im_stride, w, im_h, x_filter_ptr,
                                          conv_params, x_offset);
  }

  if (y_filter_taps <= 4) {
    highbd_convolve_2d_sr_vert_4tap_sve2(im_block, im_stride, dst, dst_stride,
                                         w, h, y_filter_ptr, conv_params, bd,
                                         y_offset);
  } else {
    highbd_convolve_2d_sr_vert_8tap_sve2(im_block, im_stride, dst, dst_stride,
                                         w, h, y_filter_ptr, conv_params, bd,
                                         y_offset);
  }
}
