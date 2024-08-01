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

#include <arm_neon.h>
#include <assert.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "aom_dsp/arm/aom_filter.h"
#include "aom_dsp/arm/aom_neon_sve_bridge.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_ports/mem.h"
#include "av1/common/arm/highbd_convolve_sve2.h"
#include "av1/common/arm/convolve_neon_i8mm.h"

static INLINE int32x4_t highbd_convolve12_4_2d_v(int16x8_t s0[2],
                                                 int16x8_t s1[2],
                                                 int16x8_t s2[2],
                                                 int16x8_t filter_0_7,
                                                 int16x8_t filter_4_11) {
  int64x2_t sum01 = aom_svdot_lane_s16(vdupq_n_s64(0), s0[0], filter_0_7, 0);
  sum01 = aom_svdot_lane_s16(sum01, s1[0], filter_0_7, 1);
  sum01 = aom_svdot_lane_s16(sum01, s2[0], filter_4_11, 1);

  int64x2_t sum23 = aom_svdot_lane_s16(vdupq_n_s64(0), s0[1], filter_0_7, 0);
  sum23 = aom_svdot_lane_s16(sum23, s1[1], filter_0_7, 1);
  sum23 = aom_svdot_lane_s16(sum23, s2[1], filter_4_11, 1);

  return vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));
}

static INLINE void convolve_2d_sr_vert_12tap_sve2(
    const int16_t *src_ptr, int src_stride, uint8_t *dst_ptr,
    const int dst_stride, int w, int h, const int16x8_t y_filter_0_7,
    const int16x8_t y_filter_4_11) {
  // The no-op filter should never be used here.
  assert(vgetq_lane_s16(y_filter_0_7, 5) != 128);

  const int bd = 8;
  const int16x8_t sub_const = vdupq_n_s16(1 << (bd - 1));

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

  do {
    int16_t *s = (int16_t *)src_ptr;
    uint8_t *d = (uint8_t *)dst_ptr;
    int height = h;

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

      // Merge new data into block from previous iteration.
      aom_tbl2x2_s16(s789A, sBCDE, merge_block_tbl.val[0], s89AB);
      aom_tbl2x2_s16(s789A, sBCDE, merge_block_tbl.val[1], s9ABC);
      aom_tbl2x2_s16(s789A, sBCDE, merge_block_tbl.val[2], sABCD);

      int32x4_t d0 = highbd_convolve12_4_2d_v(s0123, s4567, s89AB, y_filter_0_7,
                                              y_filter_4_11);
      int32x4_t d1 = highbd_convolve12_4_2d_v(s1234, s5678, s9ABC, y_filter_0_7,
                                              y_filter_4_11);
      int32x4_t d2 = highbd_convolve12_4_2d_v(s2345, s6789, sABCD, y_filter_0_7,
                                              y_filter_4_11);
      int32x4_t d3 = highbd_convolve12_4_2d_v(s3456, s789A, sBCDE, y_filter_0_7,
                                              y_filter_4_11);

      int16x8_t dd01 =
          vcombine_s16(vqrshrn_n_s32(d0, 2 * FILTER_BITS - ROUND0_BITS),
                       vqrshrn_n_s32(d1, 2 * FILTER_BITS - ROUND0_BITS));
      int16x8_t dd23 =
          vcombine_s16(vqrshrn_n_s32(d2, 2 * FILTER_BITS - ROUND0_BITS),
                       vqrshrn_n_s32(d3, 2 * FILTER_BITS - ROUND0_BITS));

      dd01 = vsubq_s16(dd01, sub_const);
      dd23 = vsubq_s16(dd23, sub_const);

      uint8x8_t d01 = vqmovun_s16(dd01);
      uint8x8_t d23 = vqmovun_s16(dd23);

      store_u8x4_strided_x2(d + 0 * dst_stride, dst_stride, d01);
      store_u8x4_strided_x2(d + 2 * dst_stride, dst_stride, d23);

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
      height -= 4;
    } while (height != 0);
    src_ptr += 4;
    dst_ptr += 4;
    w -= 4;
  } while (w != 0);
}

void av1_convolve_2d_sr_sve2(const uint8_t *src, int src_stride, uint8_t *dst,
                             int dst_stride, int w, int h,
                             const InterpFilterParams *filter_params_x,
                             const InterpFilterParams *filter_params_y,
                             const int subpel_x_qn, const int subpel_y_qn,
                             ConvolveParams *conv_params) {
  if (w == 2 || h == 2) {
    av1_convolve_2d_sr_c(src, src_stride, dst, dst_stride, w, h,
                         filter_params_x, filter_params_y, subpel_x_qn,
                         subpel_y_qn, conv_params);
    return;
  }

  if (filter_params_x->taps > 8) {
    const int im_h = h + filter_params_y->taps - 1;
    const int im_stride = MAX_SB_SIZE;
    const int vert_offset = filter_params_x->taps / 2 - 1;
    const int horiz_offset = filter_params_x->taps / 2 - 1;
    const uint8_t *src_ptr = src - vert_offset * src_stride - horiz_offset;

    const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
        filter_params_x, subpel_x_qn & SUBPEL_MASK);
    const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
        filter_params_y, subpel_y_qn & SUBPEL_MASK);

    DECLARE_ALIGNED(16, int16_t,
                    im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE]);

    const int16x8_t x_filter_0_7 = vld1q_s16(x_filter_ptr);
    const int16x4_t x_filter_8_11 = vld1_s16(x_filter_ptr + 8);
    const int16x8_t y_filter_0_7 = vld1q_s16(y_filter_ptr);
    const int16x8_t y_filter_4_11 = vld1q_s16(y_filter_ptr + 4);

    convolve_2d_sr_horiz_12tap_neon_i8mm(src_ptr, src_stride, im_block,
                                         im_stride, w, im_h, x_filter_0_7,
                                         x_filter_8_11);

    convolve_2d_sr_vert_12tap_sve2(im_block, im_stride, dst, dst_stride, w, h,
                                   y_filter_0_7, y_filter_4_11);
  } else {
    av1_convolve_2d_sr_neon_i8mm(src, src_stride, dst, dst_stride, w, h,
                                 filter_params_x, filter_params_y, subpel_x_qn,
                                 subpel_y_qn, conv_params);
  }
}
