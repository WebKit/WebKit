/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <riscv_vector.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"
#include "av1/common/cdef_block.h"

// partial A is a 16-bit vector of the form:
// [x8 x7 x6 x5 x4 x3 x2 x1] and partial B has the form:
// [0  y1 y2 y3 y4 y5 y6 y7].
// This function computes (x1^2+y1^2)*C1 + (x2^2+y2^2)*C2 + ...
// (x7^2+y2^7)*C7 + (x8^2+0^2)*C8 where the C1..C8 constants are in const1
// and const2.
static inline vuint32m1_t fold_mul_and_sum_rvv(vint16m1_t partiala,
                                               vint16m1_t partialb,
                                               vuint32m1_t const1,
                                               vuint32m1_t const2) {
  // Square and add the corresponding x and y values.
  vint32m2_t cost = __riscv_vwmul_vv_i32m2(partiala, partiala, 8);
  cost = __riscv_vwmacc_vv_i32m2(cost, partialb, partialb, 8);

  // Multiply by constant.
  vuint32m2_t tmp1_u32m2 = __riscv_vreinterpret_v_i32m2_u32m2(cost);
  vuint32m1_t cost_u32m1 = __riscv_vmul_vv_u32m1(
      __riscv_vlmul_trunc_v_u32m2_u32m1(tmp1_u32m2), const1, 4);
  tmp1_u32m2 = __riscv_vslidedown_vx_u32m2(tmp1_u32m2, 4, 8);
  vuint32m1_t ret = __riscv_vmacc_vv_u32m1(
      cost_u32m1, __riscv_vlmul_trunc_v_u32m2_u32m1(tmp1_u32m2), const2, 4);
  return ret;
}

// This function computes the cost along directions 4, 5, 6, 7. (4 is diagonal
// down-right, 6 is vertical).
//
// For each direction the lines are shifted so that we can perform a
// basic sum on each vector element. For example, direction 5 is "south by
// southeast", so we need to add the pixels along each line i below:
//
// 0  1 2 3 4 5 6 7
// 0  1 2 3 4 5 6 7
// 8  0 1 2 3 4 5 6
// 8  0 1 2 3 4 5 6
// 9  8 0 1 2 3 4 5
// 9  8 0 1 2 3 4 5
// 10 9 8 0 1 2 3 4
// 10 9 8 0 1 2 3 4
//
// For this to fit nicely in vectors, the lines need to be shifted like so:
//        0 1 2 3 4 5 6 7
//        0 1 2 3 4 5 6 7
//      8 0 1 2 3 4 5 6
//      8 0 1 2 3 4 5 6
//    9 8 0 1 2 3 4 5
//    9 8 0 1 2 3 4 5
// 10 9 8 0 1 2 3 4
// 10 9 8 0 1 2 3 4
//
// In this configuration we can now perform SIMD additions to get the cost
// along direction 5. Since this won't fit into a single 128-bit vector, we use
// two of them to compute each half of the new configuration, and pad the empty
// spaces with zeros. Similar shifting is done for other directions, except
// direction 6 which is straightforward as it's the vertical direction.
static vuint32m1_t compute_vert_directions_rvv(
    vint16m1_t lines_0, vint16m1_t lines_1, vint16m1_t lines_2,
    vint16m1_t lines_3, vint16m1_t lines_4, vint16m1_t lines_5,
    vint16m1_t lines_6, vint16m1_t lines_7, uint32_t cost[4], size_t vl) {
  size_t VL_SLIDE_DOWN = __riscv_vsetvl_e16m1(16);
  vint16m1_t vec_zero_i16m1 = __riscv_vmv_v_x_i16m1(0, vl);

  // Partial sums for lines 0 and 1.
  vint16m1_t partial4a =
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_0, (8 - 1), vl);
  vint16m1_t tmp1_i16m1 =
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_1, (8 - 2), vl);
  partial4a = __riscv_vadd_vv_i16m1(partial4a, tmp1_i16m1, vl);
  vint16m1_t partial4b = __riscv_vslide1down_vx_i16m1(lines_0, 0, vl);
  tmp1_i16m1 = __riscv_vslidedown_vx_i16m1(lines_1, 2, VL_SLIDE_DOWN);
  partial4b = __riscv_vadd_vv_i16m1(partial4b, tmp1_i16m1, vl);
  tmp1_i16m1 = __riscv_vadd_vv_i16m1(lines_0, lines_1, VL_SLIDE_DOWN);
  vint16m1_t partial5a =
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, tmp1_i16m1, (8 - 3), vl);
  vint16m1_t partial5b =
      __riscv_vslidedown_vx_i16m1(tmp1_i16m1, 3, VL_SLIDE_DOWN);
  vint16m1_t partial7a =
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, tmp1_i16m1, (8 - 6), vl);
  vint16m1_t partial7b =
      __riscv_vslidedown_vx_i16m1(tmp1_i16m1, 6, VL_SLIDE_DOWN);
  vint16m1_t partial6 = __riscv_vmv_v_v_i16m1(tmp1_i16m1, vl);

  // Partial sums for lines 2 and 3.
  tmp1_i16m1 = __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_2, (8 - 3), vl);
  partial4a = __riscv_vadd_vv_i16m1(partial4a, tmp1_i16m1, vl);
  tmp1_i16m1 = __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_3, (8 - 4), vl);
  partial4a = __riscv_vadd_vv_i16m1(partial4a, tmp1_i16m1, vl);
  tmp1_i16m1 = __riscv_vslidedown_vx_i16m1(lines_2, 3, VL_SLIDE_DOWN);
  partial4b = __riscv_vadd_vv_i16m1(partial4b, tmp1_i16m1, vl);
  tmp1_i16m1 = __riscv_vslidedown_vx_i16m1(lines_3, 4, VL_SLIDE_DOWN);
  partial4b = __riscv_vadd_vv_i16m1(partial4b, tmp1_i16m1, vl);
  tmp1_i16m1 = __riscv_vadd_vv_i16m1(lines_2, lines_3, VL_SLIDE_DOWN);
  partial5a = __riscv_vadd_vv_i16m1(
      partial5a,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, tmp1_i16m1, (8 - 4), vl), vl);
  partial5b = __riscv_vadd_vv_i16m1(
      partial5b, __riscv_vslidedown_vx_i16m1(tmp1_i16m1, 4, VL_SLIDE_DOWN), vl);
  partial7a = __riscv_vadd_vv_i16m1(
      partial7a,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, tmp1_i16m1, (8 - 5), vl), vl);
  partial7b = __riscv_vadd_vv_i16m1(
      partial7b, __riscv_vslidedown_vx_i16m1(tmp1_i16m1, 5, VL_SLIDE_DOWN), vl);
  partial6 = __riscv_vadd_vv_i16m1(partial6, tmp1_i16m1, vl);

  // Partial sums for lines 4 and 5.
  partial4a = __riscv_vadd_vv_i16m1(
      partial4a,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_4, (8 - 5), vl), vl);
  partial4a = __riscv_vadd_vv_i16m1(
      partial4a,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_5, (8 - 6), vl), vl);
  partial4b = __riscv_vadd_vv_i16m1(
      partial4b, __riscv_vslidedown_vx_i16m1(lines_4, 5, VL_SLIDE_DOWN), vl);
  partial4b = __riscv_vadd_vv_i16m1(
      partial4b, __riscv_vslidedown_vx_i16m1(lines_5, 6, VL_SLIDE_DOWN), vl);
  tmp1_i16m1 = __riscv_vadd_vv_i16m1(lines_4, lines_5, VL_SLIDE_DOWN);
  partial5a = __riscv_vadd_vv_i16m1(
      partial5a,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, tmp1_i16m1, (8 - 5), vl), vl);
  partial5b = __riscv_vadd_vv_i16m1(
      partial5b, __riscv_vslidedown_vx_i16m1(tmp1_i16m1, 5, VL_SLIDE_DOWN), vl);
  partial7a = __riscv_vadd_vv_i16m1(
      partial7a,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, tmp1_i16m1, (8 - 4), vl), vl);
  partial7b = __riscv_vadd_vv_i16m1(
      partial7b, __riscv_vslidedown_vx_i16m1(tmp1_i16m1, 4, VL_SLIDE_DOWN), vl);
  partial6 = __riscv_vadd_vv_i16m1(partial6, tmp1_i16m1, vl);

  // Partial sums for lines 6 and 7.
  partial4a = __riscv_vadd_vv_i16m1(
      partial4a,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_6, (8 - 7), vl), vl);
  partial4a = __riscv_vadd_vv_i16m1(partial4a, lines_7, vl);
  partial4b = __riscv_vadd_vv_i16m1(
      partial4b, __riscv_vslidedown_vx_i16m1(lines_6, 7, VL_SLIDE_DOWN), vl);
  tmp1_i16m1 = __riscv_vadd_vv_i16m1(lines_6, lines_7, VL_SLIDE_DOWN);
  partial5a = __riscv_vadd_vv_i16m1(
      partial5a,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, tmp1_i16m1, (8 - 6), vl), vl);
  partial5b = __riscv_vadd_vv_i16m1(
      partial5b, __riscv_vslidedown_vx_i16m1(tmp1_i16m1, 6, VL_SLIDE_DOWN), vl);
  partial7a = __riscv_vadd_vv_i16m1(
      partial7a,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, tmp1_i16m1, (8 - 3), vl), vl);
  partial7b = __riscv_vadd_vv_i16m1(
      partial7b, __riscv_vslidedown_vx_i16m1(tmp1_i16m1, 3, VL_SLIDE_DOWN), vl);
  partial6 = __riscv_vadd_vv_i16m1(partial6, tmp1_i16m1, vl);

  // const0 = { 840, 420, 280, 210, }
  vuint32m1_t const0 = __riscv_vmv_s_x_u32m1(210, 4);
  const0 = __riscv_vslide1up_vx_u32m1(const0, 280, 4);
  const0 = __riscv_vslide1up_vx_u32m1(const0, 420, 4);
  const0 = __riscv_vslide1up_vx_u32m1(const0, 840, 4);

  // const1 = { 168, 140, 120, 105, }
  vuint32m1_t const1 = __riscv_vmv_s_x_u32m1(105, 4);
  const1 = __riscv_vslide1up_vx_u32m1(const1, 120, 4);
  const1 = __riscv_vslide1up_vx_u32m1(const1, 140, 4);
  const1 = __riscv_vslide1up_vx_u32m1(const1, 168, 4);

  // const2 = { 0, 0, 420, 210, }
  vuint32m1_t const2 = __riscv_vmv_v_x_u32m1(0, 4);
  const2 = __riscv_vslide1down_vx_u32m1(const2, 420, 4);
  const2 = __riscv_vslide1down_vx_u32m1(const2, 210, 4);

  // const3 = { 140, 105, 105, 105, };
  vuint32m1_t const3 = __riscv_vmv_v_x_u32m1(105, 4);
  const3 = __riscv_vslide1up_vx_u32m1(const3, 140, 4);

  // Compute costs in terms of partial sums.
  vint32m2_t tmp1_i32m2 = __riscv_vwmul_vv_i32m2(partial6, partial6, vl);
  vint32m2_t partial6_s32 = __riscv_vslidedown_vx_i32m2(tmp1_i32m2, 4, vl);
  partial6_s32 = __riscv_vadd_vv_i32m2(partial6_s32, tmp1_i32m2, 4);

  // Reverse partial B.
  // pattern = { 6, 5, 4, 3, 2, 1, 0, 7, }.
  vuint32m1_t costs_0, costs_1, costs_2, costs_3;
  static const uint16_t tab_u16[8] = {
    6, 5, 4, 3, 2, 1, 0, 7,
  };
  vuint16m1_t index_u16m1 = __riscv_vle16_v_u16m1(tab_u16, 8);
  vint16m1_t partial4b_rv =
      __riscv_vrgather_vv_i16m1(partial4b, index_u16m1, 8);
  costs_0 = fold_mul_and_sum_rvv(partial4a, partial4b_rv, const0, const1);
  vuint32m1_t partial6_u32 = __riscv_vreinterpret_v_i32m1_u32m1(
      __riscv_vlmul_trunc_v_i32m2_i32m1(partial6_s32));
  costs_2 = __riscv_vmul_vx_u32m1(partial6_u32, 105, 4);
  vint16m1_t partial5b_rv =
      __riscv_vrgather_vv_i16m1(partial5b, index_u16m1, 8);
  costs_1 = fold_mul_and_sum_rvv(partial5a, partial5b_rv, const2, const3);
  vint16m1_t partial7b_rv =
      __riscv_vrgather_vv_i16m1(partial7b, index_u16m1, 8);
  costs_3 = fold_mul_and_sum_rvv(partial7a, partial7b_rv, const2, const3);

  // combine values
  vuint32m1_t vec_scalar_u32m1 = __riscv_vmv_s_x_u32m1(0, 1);
  vuint32m1_t cost0_sum =
      __riscv_vredsum_vs_u32m1_u32m1(costs_0, vec_scalar_u32m1, 4);
  vuint32m1_t cost1_sum =
      __riscv_vredsum_vs_u32m1_u32m1(costs_1, vec_scalar_u32m1, 4);
  vuint32m1_t cost2_sum =
      __riscv_vredsum_vs_u32m1_u32m1(costs_2, vec_scalar_u32m1, 4);
  vuint32m1_t cost3_sum =
      __riscv_vredsum_vs_u32m1_u32m1(costs_3, vec_scalar_u32m1, 4);

  vuint32m1_t cost47 = __riscv_vslideup_vx_u32m1(cost0_sum, cost1_sum, 1, 4);
  cost47 = __riscv_vslideup_vx_u32m1(cost47, cost2_sum, 2, 4);
  cost47 = __riscv_vslideup_vx_u32m1(cost47, cost3_sum, 3, 4);
  __riscv_vse32_v_u32m1(&cost[0], cost47, 4);
  return cost47;
}

static inline vuint32m1_t fold_mul_and_sum_pairwise_rvv(vint16m1_t partiala,
                                                        vint16m1_t partialb,
                                                        vint16m1_t partialc,
                                                        vuint32m1_t const0) {
  vuint16m1_t vid_u16m1 = __riscv_vid_v_u16m1(4);
  vuint16m1_t index_u16m1 = __riscv_vsll_vx_u16m1(vid_u16m1, 1, 4);
  vint16m1_t tmp_i16m1 = __riscv_vslide1down_vx_i16m1(partiala, 0, 8);
  vint32m2_t partiala_i32m2 = __riscv_vwadd_vv_i32m2(partiala, tmp_i16m1, 8);
  tmp_i16m1 = __riscv_vslide1down_vx_i16m1(partialb, 0, 8);
  vint32m2_t partialb_i32m2 = __riscv_vwadd_vv_i32m2(partialb, tmp_i16m1, 8);

  tmp_i16m1 = __riscv_vslide1down_vx_i16m1(partialc, 0, 8);
  vint32m2_t partialc_i32m2 = __riscv_vwadd_vv_i32m2(partialc, tmp_i16m1, 8);
  partiala_i32m2 = __riscv_vmul_vv_i32m2(partiala_i32m2, partiala_i32m2, 8);
  partialb_i32m2 = __riscv_vmul_vv_i32m2(partialb_i32m2, partialb_i32m2, 8);
  vint32m1_t partialb_i32m1 = __riscv_vlmul_trunc_v_i32m2_i32m1(
      __riscv_vrgatherei16_vv_i32m2(partialb_i32m2, index_u16m1, 4));
  partialc_i32m2 = __riscv_vmul_vv_i32m2(partialc_i32m2, partialc_i32m2, 8);
  partiala_i32m2 = __riscv_vadd_vv_i32m2(partiala_i32m2, partialc_i32m2, 8);
  vint32m1_t partiala_i32m1 = __riscv_vlmul_trunc_v_i32m2_i32m1(
      __riscv_vrgatherei16_vv_i32m2(partiala_i32m2, index_u16m1, 4));

  vuint32m1_t cost = __riscv_vmul_vx_u32m1(
      __riscv_vreinterpret_v_i32m1_u32m1(partialb_i32m1), 105, 4);
  cost = __riscv_vmacc_vv_u32m1(
      cost, __riscv_vreinterpret_v_i32m1_u32m1(partiala_i32m1), const0, 4);
  return cost;
}

static inline vint32m1_t horizontal_add_4d_s16x8(vint16m1_t lines_0,
                                                 vint16m1_t lines_1,
                                                 vint16m1_t lines_2,
                                                 vint16m1_t lines_3) {
  vint32m1_t vec_scalar_i32m1 = __riscv_vmv_s_x_i32m1(0, 1);
  vint32m1_t lines0_sum =
      __riscv_vwredsum_vs_i16m1_i32m1(lines_0, vec_scalar_i32m1, 8);
  vint32m1_t lines1_sum =
      __riscv_vwredsum_vs_i16m1_i32m1(lines_1, vec_scalar_i32m1, 8);
  vint32m1_t lines2_sum =
      __riscv_vwredsum_vs_i16m1_i32m1(lines_2, vec_scalar_i32m1, 8);
  vint32m1_t lines3_sum =
      __riscv_vwredsum_vs_i16m1_i32m1(lines_3, vec_scalar_i32m1, 8);

  vint32m1_t ret = __riscv_vslideup_vx_i32m1(lines0_sum, lines1_sum, 1, 4);
  ret = __riscv_vslideup_vx_i32m1(ret, lines2_sum, 2, 4);
  ret = __riscv_vslideup_vx_i32m1(ret, lines3_sum, 3, 4);
  return ret;
}

// This function computes the cost along directions 0, 1, 2, 3. (0 means
// 45-degree up-right, 2 is horizontal).
//
// For direction 1 and 3 ("east northeast" and "east southeast") the shifted
// lines need three vectors instead of two. For direction 1 for example, we need
// to compute the sums along the line i below:
// 0 0 1 1 2 2 3  3
// 1 1 2 2 3 3 4  4
// 2 2 3 3 4 4 5  5
// 3 3 4 4 5 5 6  6
// 4 4 5 5 6 6 7  7
// 5 5 6 6 7 7 8  8
// 6 6 7 7 8 8 9  9
// 7 7 8 8 9 9 10 10
//
// Which means we need the following configuration:
// 0 0 1 1 2 2 3 3
//     1 1 2 2 3 3 4 4
//         2 2 3 3 4 4 5 5
//             3 3 4 4 5 5 6 6
//                 4 4 5 5 6 6 7 7
//                     5 5 6 6 7 7 8 8
//                         6 6 7 7 8 8 9 9
//                             7 7 8 8 9 9 10 10
//
// Three vectors are needed to compute this, as well as some extra pairwise
// additions.
static vuint32m1_t compute_horiz_directions_rvv(
    vint16m1_t lines_0, vint16m1_t lines_1, vint16m1_t lines_2,
    vint16m1_t lines_3, vint16m1_t lines_4, vint16m1_t lines_5,
    vint16m1_t lines_6, vint16m1_t lines_7, uint32_t cost[4], size_t vl) {
  // Compute diagonal directions (1, 2, 3).
  // Partial sums for lines 0 and 1.
  size_t VL_SLIDE_DOWN = __riscv_vsetvl_e16m1(16);
  vint16m1_t vec_zero_i16m1 = __riscv_vmv_v_x_i16m1(0, vl);
  vint16m1_t partial0a = __riscv_vmv_v_v_i16m1(lines_0, vl);
  partial0a = __riscv_vadd_vv_i16m1(
      partial0a,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_1, (8 - 7), vl), vl);
  vint16m1_t partial0b = __riscv_vslidedown_vx_i16m1(lines_1, 7, VL_SLIDE_DOWN);
  vint16m1_t partial1a = __riscv_vadd_vv_i16m1(
      lines_0, __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_1, (8 - 6), vl),
      vl);
  vint16m1_t partial1b = __riscv_vslidedown_vx_i16m1(lines_1, 6, VL_SLIDE_DOWN);
  vint16m1_t partial3a = __riscv_vslidedown_vx_i16m1(lines_0, 2, VL_SLIDE_DOWN);
  partial3a = __riscv_vadd_vv_i16m1(
      partial3a, __riscv_vslidedown_vx_i16m1(lines_1, 4, VL_SLIDE_DOWN), vl);
  vint16m1_t partial3b =
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_0, (8 - 2), vl);
  partial3b = __riscv_vadd_vv_i16m1(
      partial3b, __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_1, 4, vl), vl);

  // Partial sums for lines 2 and 3.
  partial0a = __riscv_vadd_vv_i16m1(
      partial0a,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_2, (8 - 6), vl), vl);
  partial0a = __riscv_vadd_vv_i16m1(
      partial0a,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_3, (8 - 5), vl), vl);
  partial0b = __riscv_vadd_vv_i16m1(
      partial0b, __riscv_vslidedown_vx_i16m1(lines_2, 6, VL_SLIDE_DOWN), vl);
  partial0b = __riscv_vadd_vv_i16m1(
      partial0b, __riscv_vslidedown_vx_i16m1(lines_3, 5, VL_SLIDE_DOWN), vl);
  partial1a = __riscv_vadd_vv_i16m1(
      partial1a,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_2, (8 - 4), vl), vl);
  partial1a = __riscv_vadd_vv_i16m1(
      partial1a,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_3, (8 - 2), vl), vl);
  partial1b = __riscv_vadd_vv_i16m1(
      partial1b, __riscv_vslidedown_vx_i16m1(lines_2, 4, VL_SLIDE_DOWN), vl);
  partial1b = __riscv_vadd_vv_i16m1(
      partial1b, __riscv_vslidedown_vx_i16m1(lines_3, 2, VL_SLIDE_DOWN), vl);
  partial3a = __riscv_vadd_vv_i16m1(
      partial3a, __riscv_vslidedown_vx_i16m1(lines_2, 6, VL_SLIDE_DOWN), vl);
  partial3b = __riscv_vadd_vv_i16m1(
      partial3b,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_2, (8 - 6), vl), vl);
  partial3b = __riscv_vadd_vv_i16m1(partial3b, lines_3, vl);

  // Partial sums for lines 4 and 5.
  partial0a = __riscv_vadd_vv_i16m1(
      partial0a,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_4, (8 - 4), vl), vl);
  partial0a = __riscv_vadd_vv_i16m1(
      partial0a,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_5, (8 - 3), vl), vl);
  partial0b = __riscv_vadd_vv_i16m1(
      partial0b, __riscv_vslidedown_vx_i16m1(lines_4, 4, VL_SLIDE_DOWN), vl);
  partial0b = __riscv_vadd_vv_i16m1(
      partial0b, __riscv_vslidedown_vx_i16m1(lines_5, 3, VL_SLIDE_DOWN), vl);
  partial1b = __riscv_vadd_vv_i16m1(partial1b, lines_4, vl);
  partial1b = __riscv_vadd_vv_i16m1(
      partial1b,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_5, (8 - 6), vl), vl);
  vint16m1_t partial1c = __riscv_vslidedown_vx_i16m1(lines_5, 6, VL_SLIDE_DOWN);
  partial3b = __riscv_vadd_vv_i16m1(
      partial3b, __riscv_vslidedown_vx_i16m1(lines_4, 2, VL_SLIDE_DOWN), vl);
  partial3b = __riscv_vadd_vv_i16m1(
      partial3b, __riscv_vslidedown_vx_i16m1(lines_5, 4, VL_SLIDE_DOWN), vl);
  vint16m1_t partial3c =
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_4, (8 - 2), vl);
  partial3c = __riscv_vadd_vv_i16m1(
      partial3c,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_5, (8 - 4), vl), vl);

  // Partial sums for lines 6 and 7.
  partial0a = __riscv_vadd_vv_i16m1(
      partial0a,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_6, (8 - 2), vl), vl);
  partial0a = __riscv_vadd_vv_i16m1(
      partial0a,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_7, (8 - 1), vl), vl);
  partial0b = __riscv_vadd_vv_i16m1(
      partial0b, __riscv_vslidedown_vx_i16m1(lines_6, 2, VL_SLIDE_DOWN), vl);
  partial0b = __riscv_vadd_vv_i16m1(
      partial0b, __riscv_vslide1down_vx_i16m1(lines_7, 0, vl), vl);
  partial1b = __riscv_vadd_vv_i16m1(
      partial1b,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_6, (8 - 4), vl), vl);
  partial1b = __riscv_vadd_vv_i16m1(
      partial1b,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_7, (8 - 2), vl), vl);
  partial1c = __riscv_vadd_vv_i16m1(
      partial1c, __riscv_vslidedown_vx_i16m1(lines_6, 4, VL_SLIDE_DOWN), vl);
  partial1c = __riscv_vadd_vv_i16m1(
      partial1c, __riscv_vslidedown_vx_i16m1(lines_7, 2, VL_SLIDE_DOWN), vl);
  partial3b = __riscv_vadd_vv_i16m1(
      partial3b, __riscv_vslidedown_vx_i16m1(lines_6, 6, VL_SLIDE_DOWN), vl);
  partial3c = __riscv_vadd_vv_i16m1(
      partial3c,
      __riscv_vslideup_vx_i16m1(vec_zero_i16m1, lines_6, (8 - 6), vl), vl);
  partial3c = __riscv_vadd_vv_i16m1(partial3c, lines_7, vl);

  // Special case for direction 2 as it's just a sum along each line.
  vint32m1_t partial2a =
      horizontal_add_4d_s16x8(lines_0, lines_1, lines_2, lines_3);
  vint32m1_t partial2b =
      horizontal_add_4d_s16x8(lines_4, lines_5, lines_6, lines_7);
  vuint32m1_t partial2a_u32 = __riscv_vreinterpret_v_i32m1_u32m1(
      __riscv_vmul_vv_i32m1(partial2a, partial2a, 4));
  vuint32m1_t partial2b_u32 = __riscv_vreinterpret_v_i32m1_u32m1(
      __riscv_vmul_vv_i32m1(partial2b, partial2b, 4));

  // const0 = { 840, 420, 280, 210, }
  vuint32m1_t const0 = __riscv_vmv_s_x_u32m1(210, 4);
  const0 = __riscv_vslide1up_vx_u32m1(const0, 280, 4);
  const0 = __riscv_vslide1up_vx_u32m1(const0, 420, 4);
  const0 = __riscv_vslide1up_vx_u32m1(const0, 840, 4);

  // const1 = { 168, 140, 120, 105, }
  vuint32m1_t const1 = __riscv_vmv_s_x_u32m1(105, 4);
  const1 = __riscv_vslide1up_vx_u32m1(const1, 120, 4);
  const1 = __riscv_vslide1up_vx_u32m1(const1, 140, 4);
  const1 = __riscv_vslide1up_vx_u32m1(const1, 168, 4);

  // const2 = { 420, 210, 140, 105, };
  vuint32m1_t const2 = __riscv_vmv_s_x_u32m1(105, 4);
  const2 = __riscv_vslide1up_vx_u32m1(const2, 140, 4);
  const2 = __riscv_vslide1up_vx_u32m1(const2, 210, 4);
  const2 = __riscv_vslide1up_vx_u32m1(const2, 420, 4);

  static const uint16_t tab_u16[8] = {
    0, 6, 5, 4, 3, 2, 1, 0,
  };
  vuint32m1_t costs_0, costs_1, costs_2, costs_3;
  vuint16m1_t template_u16m1 = __riscv_vle16_v_u16m1(tab_u16, 8);

  // Reverse partial c.
  // pattern = { 6, 5, 4, 3, 2, 1, 0, 7, }
  vuint16m1_t index_u16m1 = __riscv_vslide1down_vx_u16m1(template_u16m1, 7, 8);
  vint16m1_t partial0b_rv =
      __riscv_vrgather_vv_i16m1(partial0b, index_u16m1, 8);
  costs_0 = fold_mul_and_sum_rvv(partial0a, partial0b_rv, const0, const1);

  // Reverse partial c.
  // pattern = { 5, 4, 3, 2, 1, 0, 6, 7, }
  vuint16m1_t index_pair_u16m1 =
      __riscv_vslide1down_vx_u16m1(template_u16m1, 6, 8);
  index_pair_u16m1 = __riscv_vslide1down_vx_u16m1(index_pair_u16m1, 7, 8);
  vint16m1_t partialc_rv =
      __riscv_vrgather_vv_i16m1(partial1c, index_pair_u16m1, 8);
  costs_1 =
      fold_mul_and_sum_pairwise_rvv(partial1a, partial1b, partialc_rv, const2);

  costs_2 = __riscv_vadd_vv_u32m1(partial2a_u32, partial2b_u32, 4);
  costs_2 = __riscv_vmul_vx_u32m1(costs_2, 105, 4);

  vint16m1_t partial3a_rv =
      __riscv_vrgather_vv_i16m1(partial3a, index_pair_u16m1, 8);
  costs_3 =
      fold_mul_and_sum_pairwise_rvv(partial3c, partial3b, partial3a_rv, const2);

  // combine values
  vuint32m1_t vec_scalar_u32m1 = __riscv_vmv_s_x_u32m1(0, 1);
  vuint32m1_t cost0_sum =
      __riscv_vredsum_vs_u32m1_u32m1(costs_0, vec_scalar_u32m1, 4);
  vuint32m1_t cost1_sum =
      __riscv_vredsum_vs_u32m1_u32m1(costs_1, vec_scalar_u32m1, 4);
  vuint32m1_t cost2_sum =
      __riscv_vredsum_vs_u32m1_u32m1(costs_2, vec_scalar_u32m1, 4);
  vuint32m1_t cost3_sum =
      __riscv_vredsum_vs_u32m1_u32m1(costs_3, vec_scalar_u32m1, 4);

  costs_0 = __riscv_vslideup_vx_u32m1(cost0_sum, cost1_sum, 1, 4);
  costs_0 = __riscv_vslideup_vx_u32m1(costs_0, cost2_sum, 2, 4);
  costs_0 = __riscv_vslideup_vx_u32m1(costs_0, cost3_sum, 3, 4);
  __riscv_vse32_v_u32m1(&cost[0], costs_0, 4);
  return costs_0;
}

int cdef_find_dir_rvv(const uint16_t *img, int stride, int32_t *var,
                      int coeff_shift) {
  size_t vl = 8;
  size_t vlmax = __riscv_vsetvlmax_e16m1();
  vuint16m1_t s;
  vint16m1_t lines_0, lines_1, lines_2, lines_3;
  vint16m1_t lines_4, lines_5, lines_6, lines_7;
  vuint16m1_t vec_zero_u16m1 =
      __riscv_vmv_v_x_u16m1(0, __riscv_vsetvl_e16m1(16));

  if (vlmax == 8)
    s = __riscv_vle16_v_u16m1(img, vl);
  else
    s = __riscv_vle16_v_u16m1_tu(vec_zero_u16m1, img, vl);
  lines_0 = __riscv_vreinterpret_v_u16m1_i16m1(
      __riscv_vsrl_vx_u16m1(s, coeff_shift, vl));
  lines_0 = __riscv_vsub_vx_i16m1(lines_0, 128, vl);

  img += stride;
  if (vlmax == 8)
    s = __riscv_vle16_v_u16m1(img, vl);
  else
    s = __riscv_vle16_v_u16m1_tu(vec_zero_u16m1, img, vl);
  lines_1 = __riscv_vreinterpret_v_u16m1_i16m1(
      __riscv_vsrl_vx_u16m1(s, coeff_shift, vl));
  lines_1 = __riscv_vsub_vx_i16m1(lines_1, 128, vl);

  img += stride;
  if (vlmax == 8)
    s = __riscv_vle16_v_u16m1(img, vl);
  else
    s = __riscv_vle16_v_u16m1_tu(vec_zero_u16m1, img, vl);
  lines_2 = __riscv_vreinterpret_v_u16m1_i16m1(
      __riscv_vsrl_vx_u16m1(s, coeff_shift, vl));
  lines_2 = __riscv_vsub_vx_i16m1(lines_2, 128, vl);

  img += stride;
  if (vlmax == 8)
    s = __riscv_vle16_v_u16m1(img, vl);
  else
    s = __riscv_vle16_v_u16m1_tu(vec_zero_u16m1, img, vl);
  lines_3 = __riscv_vreinterpret_v_u16m1_i16m1(
      __riscv_vsrl_vx_u16m1(s, coeff_shift, vl));
  lines_3 = __riscv_vsub_vx_i16m1(lines_3, 128, vl);

  img += stride;
  if (vlmax == 8)
    s = __riscv_vle16_v_u16m1(img, vl);
  else
    s = __riscv_vle16_v_u16m1_tu(vec_zero_u16m1, img, vl);
  lines_4 = __riscv_vreinterpret_v_u16m1_i16m1(
      __riscv_vsrl_vx_u16m1(s, coeff_shift, vl));
  lines_4 = __riscv_vsub_vx_i16m1(lines_4, 128, vl);

  img += stride;
  if (vlmax == 8)
    s = __riscv_vle16_v_u16m1(img, vl);
  else
    s = __riscv_vle16_v_u16m1_tu(vec_zero_u16m1, img, vl);
  lines_5 = __riscv_vreinterpret_v_u16m1_i16m1(
      __riscv_vsrl_vx_u16m1(s, coeff_shift, vl));
  lines_5 = __riscv_vsub_vx_i16m1(lines_5, 128, vl);

  img += stride;
  if (vlmax == 8)
    s = __riscv_vle16_v_u16m1(img, vl);
  else
    s = __riscv_vle16_v_u16m1_tu(vec_zero_u16m1, img, vl);
  lines_6 = __riscv_vreinterpret_v_u16m1_i16m1(
      __riscv_vsrl_vx_u16m1(s, coeff_shift, vl));
  lines_6 = __riscv_vsub_vx_i16m1(lines_6, 128, vl);

  img += stride;
  if (vlmax == 8)
    s = __riscv_vle16_v_u16m1(img, vl);
  else
    s = __riscv_vle16_v_u16m1_tu(vec_zero_u16m1, img, vl);
  lines_7 = __riscv_vreinterpret_v_u16m1_i16m1(
      __riscv_vsrl_vx_u16m1(s, coeff_shift, vl));
  lines_7 = __riscv_vsub_vx_i16m1(lines_7, 128, vl);

  // Compute "mostly vertical" directions.
  uint32_t cost[8];
  vuint32m1_t cost47 =
      compute_vert_directions_rvv(lines_0, lines_1, lines_2, lines_3, lines_4,
                                  lines_5, lines_6, lines_7, cost + 4, vl);

  // Compute "mostly horizontal" directions.
  vuint32m1_t cost03 =
      compute_horiz_directions_rvv(lines_0, lines_1, lines_2, lines_3, lines_4,
                                   lines_5, lines_6, lines_7, cost, vl);

  // Find max cost as well as its index to get best_dir.
  // The max cost needs to be propagated in the whole vector to find its
  // position in the original cost vectors cost03 and cost47.
  vuint32m1_t vec_scalar_u32m1 = __riscv_vmv_s_x_u32m1(0, 1);
  vuint32m1_t cost07 = __riscv_vmaxu_vv_u32m1(cost03, cost47, 4);
  uint32_t best_cost = __riscv_vmv_x_s_u32m1_u32(
      __riscv_vredmaxu_vs_u32m1_u32m1(cost07, vec_scalar_u32m1, 4));
  vbool32_t mask_cost = __riscv_vmseq_vx_u32m1_b32(cost03, best_cost, 4);
  long best_dir = __riscv_vfirst_m_b32(mask_cost, 4);
  if (best_dir == -1) {
    mask_cost = __riscv_vmseq_vx_u32m1_b32(cost47, best_cost, 4);
    best_dir = __riscv_vfirst_m_b32(mask_cost, 4);
    best_dir += 4;
  }

  // Difference between the optimal variance and the variance along the
  // orthogonal direction. Again, the sum(x^2) terms cancel out.
  *var = best_cost - cost[(best_dir + 4) & 7];

  // We'd normally divide by 840, but dividing by 1024 is close enough
  // for what we're going to do with this.
  *var >>= 10;
  return (int)best_dir;
}

void cdef_copy_rect8_8bit_to_16bit_rvv(uint16_t *dst, int dstride,
                                       const uint8_t *src, int sstride,
                                       int width, int height) {
  do {
    int w = 0;
    size_t num_cols = width;
    while (num_cols > 0) {
      size_t vl = __riscv_vsetvl_e8mf2(num_cols);
      vuint8mf2_t u8_src = __riscv_vle8_v_u8mf2(src + w, vl);
      vuint16m1_t u16_src = __riscv_vwcvtu_x_x_v_u16m1(u8_src, vl);
      __riscv_vse16_v_u16m1(dst + w, u16_src, vl);

      w += vl;
      num_cols -= vl;
    }
    src += sstride;
    dst += dstride;
  } while (--height != 0);
}

void cdef_copy_rect8_16bit_to_16bit_rvv(uint16_t *dst, int dstride,
                                        const uint16_t *src, int sstride,
                                        int width, int height) {
  do {
    int w = 0;
    size_t num_cols = width;
    while (num_cols > 0) {
      size_t vl = __riscv_vsetvl_e16m1(num_cols);
      vuint16m1_t u16_src = __riscv_vle16_v_u16m1(src + w, vl);
      __riscv_vse16_v_u16m1(dst + w, u16_src, vl);

      w += vl;
      num_cols -= vl;
    }
    src += sstride;
    dst += dstride;
  } while (--height != 0);
}

static inline vint16m1_t constrain16(vint16m1_t a, vint16m1_t b,
                                     int16_t threshold, int16_t adjdamp,
                                     size_t vl) {
  if (!threshold) return __riscv_vmv_v_x_i16m1(0, vl);
  const vbool16_t mask = __riscv_vmslt_vv_i16m1_b16(a, b, vl);
  const vint16m1_t diff = __riscv_vsub_vv_i16m1(a, b, vl);
  const vint16m1_t abs_diff = __riscv_vneg_v_i16m1_tumu(mask, diff, diff, vl);
  const vint16m1_t shift = __riscv_vsra_vx_i16m1(abs_diff, adjdamp, vl);
  const vint16m1_t thr = __riscv_vmv_v_x_i16m1(threshold, vl);
  const vint16m1_t sub = __riscv_vsub_vv_i16m1(thr, shift, vl);
  const vint16m1_t max = __riscv_vmax_vx_i16m1(sub, 0, vl);
  const vint16m1_t min = __riscv_vmin_vv_i16m1(abs_diff, max, vl);
  return __riscv_vneg_v_i16m1_tumu(mask, min, min, vl);
}

static inline vint16m1_t vmax_mask(vint16m1_t a, vint16m1_t b, size_t vl) {
  const vbool16_t mask =
      __riscv_vmseq_vx_i16m1_b16(a, (int16_t)CDEF_VERY_LARGE, vl);
  const vint16m1_t val = __riscv_vmerge_vvm_i16m1(a, b, mask, vl);
  return __riscv_vmax_vv_i16m1(val, b, vl);
}

static inline vint16m1_t load_strided_i16_4x2(int16_t *addr,
                                              const ptrdiff_t stride,
                                              size_t vl) {
  const vint16m1_t px_l1 = __riscv_vle16_v_i16m1(addr + stride, vl);
  const vint16m1_t px_l0 = __riscv_vle16_v_i16m1(addr, vl);
  return __riscv_vslideup_vx_i16m1(px_l0, px_l1, 4, vl);
}

static inline void store_strided_u8_4x2(uint8_t *addr, vuint8mf2_t vdst,
                                        const ptrdiff_t stride, size_t vl) {
  __riscv_vse8_v_u8mf2(addr, vdst, vl >> 1);
  vdst = __riscv_vslidedown_vx_u8mf2(vdst, 4, vl);
  __riscv_vse8_v_u8mf2(addr + stride, vdst, vl >> 1);
}

static inline void store_strided_u16_4x2(uint16_t *addr, vuint16m1_t vdst,
                                         const ptrdiff_t stride, size_t vl) {
  __riscv_vse16_v_u16m1(addr, vdst, vl >> 1);
  vdst = __riscv_vslidedown_vx_u16m1(vdst, 4, vl);
  __riscv_vse16_v_u16m1(addr + stride, vdst, vl >> 1);
}

#define LOAD_PIX(addr)                                              \
  const vint16m1_t px = __riscv_vle16_v_i16m1((int16_t *)addr, vl); \
  vint16m1_t sum = __riscv_vmv_v_x_i16m1(0, vl)

#define LOAD_PIX4(addr)                                        \
  const vint16m1_t px =                                        \
      load_strided_i16_4x2((int16_t *)addr, CDEF_BSTRIDE, vl); \
  vint16m1_t sum = __riscv_vmv_v_x_i16m1(0, vl)

#define LOAD_DIR(p, addr, o0, o1)                                          \
  const vint16m1_t p##0 = __riscv_vle16_v_i16m1((int16_t *)addr + o0, vl); \
  const vint16m1_t p##1 = __riscv_vle16_v_i16m1((int16_t *)addr - o0, vl); \
  const vint16m1_t p##2 = __riscv_vle16_v_i16m1((int16_t *)addr + o1, vl); \
  const vint16m1_t p##3 = __riscv_vle16_v_i16m1((int16_t *)addr - o1, vl)

#define LOAD_DIR4(p, addr, o0, o1)                                  \
  const vint16m1_t p##0 =                                           \
      load_strided_i16_4x2((int16_t *)addr + o0, CDEF_BSTRIDE, vl); \
  const vint16m1_t p##1 =                                           \
      load_strided_i16_4x2((int16_t *)addr - o0, CDEF_BSTRIDE, vl); \
  const vint16m1_t p##2 =                                           \
      load_strided_i16_4x2((int16_t *)addr + o1, CDEF_BSTRIDE, vl); \
  const vint16m1_t p##3 =                                           \
      load_strided_i16_4x2((int16_t *)addr - o1, CDEF_BSTRIDE, vl)

#define MAKE_TAPS                                                         \
  const int *pri_taps = cdef_pri_taps[(pri_strength >> coeff_shift) & 1]; \
  const int16_t tap0 = (int16_t)(pri_taps[0]);                            \
  const int16_t tap1 = (int16_t)(pri_taps[1])

#define CONSTRAIN(p, strength, shift)                               \
  vint16m1_t p##_c0 =                                               \
      constrain16(p##0, px, (int16_t)strength, (int16_t)shift, vl); \
  vint16m1_t p##_c1 =                                               \
      constrain16(p##1, px, (int16_t)strength, (int16_t)shift, vl); \
  vint16m1_t p##_c2 =                                               \
      constrain16(p##2, px, (int16_t)strength, (int16_t)shift, vl); \
  vint16m1_t p##_c3 =                                               \
      constrain16(p##3, px, (int16_t)strength, (int16_t)shift, vl)

#define SETUP_MINMAX   \
  vint16m1_t max = px; \
  vint16m1_t min = px

#define MIN_MAX(p)                              \
  do {                                          \
    max = vmax_mask(p##0, max, vl);             \
    min = __riscv_vmin_vv_i16m1(p##0, min, vl); \
    max = vmax_mask(p##1, max, vl);             \
    min = __riscv_vmin_vv_i16m1(p##1, min, vl); \
    max = vmax_mask(p##2, max, vl);             \
    min = __riscv_vmin_vv_i16m1(p##2, min, vl); \
    max = vmax_mask(p##3, max, vl);             \
    min = __riscv_vmin_vv_i16m1(p##3, min, vl); \
  } while (0)

#define PRI_0_UPDATE_SUM(p)                                             \
  const vint16m1_t p##sum0 = __riscv_vadd_vv_i16m1(p##_c0, p##_c1, vl); \
  const vint16m1_t p##sum1 = __riscv_vadd_vv_i16m1(p##_c2, p##_c3, vl); \
  sum = __riscv_vmacc_vx_i16m1(sum, tap0, p##sum0, vl);                 \
  sum = __riscv_vmacc_vx_i16m1(sum, tap1, p##sum1, vl)

#define UPDATE_SUM(p)                                                   \
  const vint16m1_t p##sum0 = __riscv_vadd_vv_i16m1(p##_c0, p##_c1, vl); \
  const vint16m1_t p##sum1 = __riscv_vadd_vv_i16m1(p##_c2, p##_c3, vl); \
  sum = __riscv_vadd_vv_i16m1(sum, p##sum0, vl);                        \
  sum = __riscv_vadd_vv_i16m1(sum, p##sum1, vl)

#define SEC_0_UPDATE_SUM(p)                                               \
  const vint16m1_t p##sum0 = __riscv_vadd_vv_i16m1(p##_c0, p##_c1, vl);   \
  const vint16m1_t p##sum1 = __riscv_vadd_vv_i16m1(p##_c2, p##_c3, vl);   \
  const vint16m1_t p##sum2 = __riscv_vadd_vv_i16m1(p##sum0, p##sum1, vl); \
  sum = __riscv_vadd_vv_i16m1(sum, __riscv_vsll_vx_i16m1(p##sum2, 1, vl), vl)

#define BIAS                                                                  \
  const vbool16_t mask = __riscv_vmslt_vx_i16m1_b16(sum, 0, vl);              \
  const vint16m1_t v_8 = __riscv_vmv_v_x_i16m1(8, vl);                        \
  const vint16m1_t bias = __riscv_vsub_vx_i16m1_tumu(mask, v_8, v_8, 1, vl);  \
  const vint16m1_t unclamped = __riscv_vadd_vv_i16m1(                         \
      px, __riscv_vsra_vx_i16m1(__riscv_vadd_vv_i16m1(bias, sum, vl), 4, vl), \
      vl)

#define STORE4                                     \
  do {                                             \
    store_strided_u8_4x2(dst8, vdst, dstride, vl); \
                                                   \
    in += (CDEF_BSTRIDE << 1);                     \
    dst8 += (dstride << 1);                        \
  } while (0)

#define STORE4_CLAMPED                                       \
  do {                                                       \
    BIAS;                                                    \
    vint16m1_t clamped = __riscv_vmin_vv_i16m1(              \
        __riscv_vmax_vv_i16m1(unclamped, min, vl), max, vl); \
    vuint8mf2_t vdst = __riscv_vncvt_x_x_w_u8mf2(            \
        __riscv_vreinterpret_v_i16m1_u16m1(clamped), vl);    \
    STORE4;                                                  \
  } while (0)

#define STORE4_UNCLAMPED                                    \
  do {                                                      \
    BIAS;                                                   \
    vuint8mf2_t vdst = __riscv_vncvt_x_x_w_u8mf2(           \
        __riscv_vreinterpret_v_i16m1_u16m1(unclamped), vl); \
    STORE4;                                                 \
  } while (0)

#define STORE8                            \
  do {                                    \
    __riscv_vse8_v_u8mf2(dst8, vdst, vl); \
                                          \
    in += CDEF_BSTRIDE;                   \
    dst8 += dstride;                      \
  } while (0)

#define STORE8_CLAMPED                                       \
  do {                                                       \
    BIAS;                                                    \
    vint16m1_t clamped = __riscv_vmin_vv_i16m1(              \
        __riscv_vmax_vv_i16m1(unclamped, min, vl), max, vl); \
    vuint8mf2_t vdst = __riscv_vncvt_x_x_w_u8mf2(            \
        __riscv_vreinterpret_v_i16m1_u16m1(clamped), vl);    \
    STORE8;                                                  \
  } while (0)

#define STORE8_UNCLAMPED                                    \
  do {                                                      \
    BIAS;                                                   \
    vuint8mf2_t vdst = __riscv_vncvt_x_x_w_u8mf2(           \
        __riscv_vreinterpret_v_i16m1_u16m1(unclamped), vl); \
    STORE8;                                                 \
  } while (0)

#define STORE16_4                                    \
  do {                                               \
    store_strided_u16_4x2(dst16, vdst, dstride, vl); \
                                                     \
    in += (CDEF_BSTRIDE << 1);                       \
    dst16 += (dstride << 1);                         \
  } while (0)

#define STORE16_4_CLAMPED                                           \
  do {                                                              \
    BIAS;                                                           \
    vint16m1_t clamped = __riscv_vmin_vv_i16m1(                     \
        __riscv_vmax_vv_i16m1(unclamped, min, vl), max, vl);        \
    vuint16m1_t vdst = __riscv_vreinterpret_v_i16m1_u16m1(clamped); \
    STORE16_4;                                                      \
  } while (0)

#define STORE16_4_UNCLAMPED                                           \
  do {                                                                \
    BIAS;                                                             \
    vuint16m1_t vdst = __riscv_vreinterpret_v_i16m1_u16m1(unclamped); \
    STORE16_4;                                                        \
  } while (0)

#define STORE16                             \
  do {                                      \
    __riscv_vse16_v_u16m1(dst16, vdst, vl); \
                                            \
    in += CDEF_BSTRIDE;                     \
    dst16 += dstride;                       \
  } while (0)

#define STORE16_CLAMPED                                             \
  do {                                                              \
    BIAS;                                                           \
    vint16m1_t clamped = __riscv_vmin_vv_i16m1(                     \
        __riscv_vmax_vv_i16m1(unclamped, min, vl), max, vl);        \
    vuint16m1_t vdst = __riscv_vreinterpret_v_i16m1_u16m1(clamped); \
    STORE16;                                                        \
  } while (0)

#define STORE16_UNCLAMPED                                             \
  do {                                                                \
    BIAS;                                                             \
    vuint16m1_t vdst = __riscv_vreinterpret_v_i16m1_u16m1(unclamped); \
    STORE16;                                                          \
  } while (0)

void cdef_filter_8_0_rvv(void *dest, int dstride, const uint16_t *in,
                         int pri_strength, int sec_strength, int dir,
                         int pri_damping, int sec_damping, int coeff_shift,
                         int block_width, int block_height) {
  const int po1 = cdef_directions[dir][0];
  const int po2 = cdef_directions[dir][1];
  const int s1o1 = cdef_directions[dir + 2][0];
  const int s1o2 = cdef_directions[dir + 2][1];
  const int s2o1 = cdef_directions[dir - 2][0];
  const int s2o2 = cdef_directions[dir - 2][1];
  MAKE_TAPS;

  if (pri_strength) {
    pri_damping = AOMMAX(0, pri_damping - get_msb(pri_strength));
  }
  if (sec_strength) {
    sec_damping = AOMMAX(0, sec_damping - get_msb(sec_strength));
  }

  if (block_width == 8) {
    uint8_t *dst8 = (uint8_t *)dest;

    int h = block_height;
    const size_t vl = block_width;
    do {
      LOAD_PIX(in);
      SETUP_MINMAX;

      // Primary pass
      LOAD_DIR(p, in, po1, po2);
      CONSTRAIN(p, pri_strength, pri_damping);
      MIN_MAX(p);
      PRI_0_UPDATE_SUM(p);

      // Secondary pass 1
      LOAD_DIR(s, in, s1o1, s2o1);
      CONSTRAIN(s, sec_strength, sec_damping);
      MIN_MAX(s);
      SEC_0_UPDATE_SUM(s);

      // Secondary pass 2
      LOAD_DIR(s2, in, s1o2, s2o2);
      CONSTRAIN(s2, sec_strength, sec_damping);
      MIN_MAX(s2);
      UPDATE_SUM(s2);

      // Store
      STORE8_CLAMPED;
    } while (--h != 0);
  } else {
    uint8_t *dst8 = (uint8_t *)dest;

    int h = block_height;
    const size_t vl = block_width << 1;
    do {
      LOAD_PIX4(in);
      SETUP_MINMAX;

      // Primary pass
      LOAD_DIR4(p, in, po1, po2);
      CONSTRAIN(p, pri_strength, pri_damping);
      MIN_MAX(p);
      PRI_0_UPDATE_SUM(p);

      // Secondary pass 1
      LOAD_DIR4(s, in, s1o1, s2o1);
      CONSTRAIN(s, sec_strength, sec_damping);
      MIN_MAX(s);
      SEC_0_UPDATE_SUM(s);

      // Secondary pass 2
      LOAD_DIR4(s2, in, s1o2, s2o2);
      CONSTRAIN(s2, sec_strength, sec_damping);
      MIN_MAX(s2);
      UPDATE_SUM(s2);

      // Store
      STORE4_CLAMPED;

      h -= 2;
    } while (h != 0);
  }
}

void cdef_filter_8_1_rvv(void *dest, int dstride, const uint16_t *in,
                         int pri_strength, int sec_strength, int dir,
                         int pri_damping, int sec_damping, int coeff_shift,
                         int block_width, int block_height) {
  (void)sec_strength;
  (void)sec_damping;

  const int po1 = cdef_directions[dir][0];
  const int po2 = cdef_directions[dir][1];
  MAKE_TAPS;

  if (pri_strength) {
    pri_damping = AOMMAX(0, pri_damping - get_msb(pri_strength));
  }

  if (block_width == 8) {
    uint8_t *dst8 = (uint8_t *)dest;

    int h = block_height;
    const size_t vl = block_width;
    do {
      LOAD_PIX(in);

      // Primary pass
      LOAD_DIR(p, in, po1, po2);
      CONSTRAIN(p, pri_strength, pri_damping);
      PRI_0_UPDATE_SUM(p);

      // Store
      STORE8_UNCLAMPED;
    } while (--h != 0);
  } else {
    uint8_t *dst8 = (uint8_t *)dest;

    int h = block_height;
    const size_t vl = block_width << 1;
    do {
      LOAD_PIX4(in);

      // Primary pass
      LOAD_DIR4(p, in, po1, po2);
      CONSTRAIN(p, pri_strength, pri_damping);
      PRI_0_UPDATE_SUM(p);

      // Store
      STORE4_UNCLAMPED;

      h -= 2;
    } while (h != 0);
  }
}

void cdef_filter_8_2_rvv(void *dest, int dstride, const uint16_t *in,
                         int pri_strength, int sec_strength, int dir,
                         int pri_damping, int sec_damping, int coeff_shift,
                         int block_width, int block_height) {
  (void)pri_strength;
  (void)pri_damping;
  (void)coeff_shift;

  const int s1o1 = cdef_directions[dir + 2][0];
  const int s1o2 = cdef_directions[dir + 2][1];
  const int s2o1 = cdef_directions[dir - 2][0];
  const int s2o2 = cdef_directions[dir - 2][1];

  if (sec_strength) {
    sec_damping = AOMMAX(0, sec_damping - get_msb(sec_strength));
  }

  if (block_width == 8) {
    uint8_t *dst8 = (uint8_t *)dest;

    int h = block_height;
    const size_t vl = block_width;
    do {
      LOAD_PIX(in);

      // Secondary pass 1
      LOAD_DIR(s, in, s1o1, s2o1);
      CONSTRAIN(s, sec_strength, sec_damping);
      SEC_0_UPDATE_SUM(s);

      // Secondary pass 2
      LOAD_DIR(s2, in, s1o2, s2o2);
      CONSTRAIN(s2, sec_strength, sec_damping);
      UPDATE_SUM(s2);

      // Store
      STORE8_UNCLAMPED;
    } while (--h != 0);
  } else {
    uint8_t *dst8 = (uint8_t *)dest;

    int h = block_height;
    const size_t vl = block_width << 1;
    do {
      LOAD_PIX4(in);

      // Secondary pass 1
      LOAD_DIR4(s, in, s1o1, s2o1);
      CONSTRAIN(s, sec_strength, sec_damping);
      SEC_0_UPDATE_SUM(s);

      // Secondary pass 2
      LOAD_DIR4(s2, in, s1o2, s2o2);
      CONSTRAIN(s2, sec_strength, sec_damping);
      UPDATE_SUM(s2);

      // Store
      STORE4_UNCLAMPED;

      h -= 2;
    } while (h != 0);
  }
}

void cdef_filter_8_3_rvv(void *dest, int dstride, const uint16_t *in,
                         int pri_strength, int sec_strength, int dir,
                         int pri_damping, int sec_damping, int coeff_shift,
                         int block_width, int block_height) {
  (void)pri_strength;
  (void)sec_strength;
  (void)dir;
  (void)pri_damping;
  (void)sec_damping;
  (void)coeff_shift;

  if (block_width == 8) {
    uint8_t *dst8 = (uint8_t *)dest;

    int h = block_height;
    const size_t vl = block_width;
    do {
      const vuint16m1_t px = __riscv_vle16_v_u16m1(in, vl);
      const vuint8mf2_t vdst = __riscv_vncvt_x_x_w_u8mf2(px, vl);
      __riscv_vse8_v_u8mf2(dst8, vdst, vl);

      in += CDEF_BSTRIDE;
      dst8 += dstride;
    } while (--h != 0);
  } else {
    uint8_t *dst8 = (uint8_t *)dest;

    int h = block_height;
    const size_t vl = block_width << 1;
    do {
      const vint16m1_t px =
          load_strided_i16_4x2((int16_t *)in, CDEF_BSTRIDE, vl);
      vuint8mf2_t vdst =
          __riscv_vncvt_x_x_w_u8mf2(__riscv_vreinterpret_v_i16m1_u16m1(px), vl);
      store_strided_u8_4x2(dst8, vdst, dstride, vl);

      in += 2 * CDEF_BSTRIDE;
      dst8 += 2 * dstride;
      h -= 2;
    } while (h != 0);
  }
}

void cdef_filter_16_0_rvv(void *dest, int dstride, const uint16_t *in,
                          int pri_strength, int sec_strength, int dir,
                          int pri_damping, int sec_damping, int coeff_shift,
                          int block_width, int block_height) {
  const int po1 = cdef_directions[dir][0];
  const int po2 = cdef_directions[dir][1];
  const int s1o1 = cdef_directions[dir + 2][0];
  const int s1o2 = cdef_directions[dir + 2][1];
  const int s2o1 = cdef_directions[dir - 2][0];
  const int s2o2 = cdef_directions[dir - 2][1];
  MAKE_TAPS;

  if (pri_strength) {
    pri_damping = AOMMAX(0, pri_damping - get_msb(pri_strength));
  }
  if (sec_strength) {
    sec_damping = AOMMAX(0, sec_damping - get_msb(sec_strength));
  }

  if (block_width == 8) {
    uint16_t *dst16 = (uint16_t *)dest;

    int h = block_height;
    const size_t vl = block_width;
    do {
      LOAD_PIX(in);
      SETUP_MINMAX;

      // Primary pass
      LOAD_DIR(p, in, po1, po2);
      CONSTRAIN(p, pri_strength, pri_damping);
      MIN_MAX(p);
      PRI_0_UPDATE_SUM(p);

      // Secondary pass 1
      LOAD_DIR(s, in, s1o1, s2o1);
      CONSTRAIN(s, sec_strength, sec_damping);
      MIN_MAX(s);
      SEC_0_UPDATE_SUM(s);

      // Secondary pass 2
      LOAD_DIR(s2, in, s1o2, s2o2);
      CONSTRAIN(s2, sec_strength, sec_damping);
      MIN_MAX(s2);
      UPDATE_SUM(s2);

      // Store
      STORE16_CLAMPED;
    } while (--h != 0);
  } else {
    uint16_t *dst16 = (uint16_t *)dest;

    int h = block_height;
    const size_t vl = block_width << 1;
    do {
      LOAD_PIX4(in);
      SETUP_MINMAX;

      // Primary pass
      LOAD_DIR4(p, in, po1, po2);
      CONSTRAIN(p, pri_strength, pri_damping);
      MIN_MAX(p);
      PRI_0_UPDATE_SUM(p);

      // Secondary pass 1
      LOAD_DIR4(s, in, s1o1, s2o1);
      CONSTRAIN(s, sec_strength, sec_damping);
      MIN_MAX(s);
      SEC_0_UPDATE_SUM(s);

      // Secondary pass 2
      LOAD_DIR4(s2, in, s1o2, s2o2);
      CONSTRAIN(s2, sec_strength, sec_damping);
      MIN_MAX(s2);
      UPDATE_SUM(s2);

      // Store
      STORE16_4_CLAMPED;

      h -= 2;
    } while (h != 0);
  }
}

void cdef_filter_16_1_rvv(void *dest, int dstride, const uint16_t *in,
                          int pri_strength, int sec_strength, int dir,
                          int pri_damping, int sec_damping, int coeff_shift,
                          int block_width, int block_height) {
  (void)sec_strength;
  (void)sec_damping;

  const int po1 = cdef_directions[dir][0];
  const int po2 = cdef_directions[dir][1];
  MAKE_TAPS;

  if (pri_strength) {
    pri_damping = AOMMAX(0, pri_damping - get_msb(pri_strength));
  }

  if (block_width == 8) {
    uint16_t *dst16 = (uint16_t *)dest;

    int h = block_height;
    const size_t vl = block_width;
    do {
      LOAD_PIX(in);

      // Primary pass
      LOAD_DIR(p, in, po1, po2);
      CONSTRAIN(p, pri_strength, pri_damping);
      PRI_0_UPDATE_SUM(p);

      // Store
      STORE16_UNCLAMPED;
    } while (--h != 0);
  } else {
    uint16_t *dst16 = (uint16_t *)dest;

    int h = block_height;
    const size_t vl = block_width << 1;
    do {
      LOAD_PIX4(in);

      // Primary pass
      LOAD_DIR4(p, in, po1, po2);
      CONSTRAIN(p, pri_strength, pri_damping);
      PRI_0_UPDATE_SUM(p);

      // Store
      STORE16_4_UNCLAMPED;

      h -= 2;
    } while (h != 0);
  }
}

void cdef_filter_16_2_rvv(void *dest, int dstride, const uint16_t *in,
                          int pri_strength, int sec_strength, int dir,
                          int pri_damping, int sec_damping, int coeff_shift,
                          int block_width, int block_height) {
  (void)pri_strength;
  (void)pri_damping;
  (void)coeff_shift;

  const int s1o1 = cdef_directions[dir + 2][0];
  const int s1o2 = cdef_directions[dir + 2][1];
  const int s2o1 = cdef_directions[dir - 2][0];
  const int s2o2 = cdef_directions[dir - 2][1];

  if (sec_strength) {
    sec_damping = AOMMAX(0, sec_damping - get_msb(sec_strength));
  }

  if (block_width == 8) {
    uint16_t *dst16 = (uint16_t *)dest;

    int h = block_height;
    const size_t vl = block_width;
    do {
      LOAD_PIX(in);

      // Secondary pass 1
      LOAD_DIR(s, in, s1o1, s2o1);
      CONSTRAIN(s, sec_strength, sec_damping);
      SEC_0_UPDATE_SUM(s);

      // Secondary pass 2
      LOAD_DIR(s2, in, s1o2, s2o2);
      CONSTRAIN(s2, sec_strength, sec_damping);
      UPDATE_SUM(s2);

      // Store
      STORE16_UNCLAMPED;
    } while (--h != 0);
  } else {
    uint16_t *dst16 = (uint16_t *)dest;

    int h = block_height;
    const size_t vl = block_width << 1;
    do {
      LOAD_PIX4(in);

      // Secondary pass 1
      LOAD_DIR4(s, in, s1o1, s2o1);
      CONSTRAIN(s, sec_strength, sec_damping);
      SEC_0_UPDATE_SUM(s);

      // Secondary pass 2
      LOAD_DIR4(s2, in, s1o2, s2o2);
      CONSTRAIN(s2, sec_strength, sec_damping);
      UPDATE_SUM(s2);

      // Store
      STORE16_4_UNCLAMPED;

      h -= 2;
    } while (h != 0);
  }
}

void cdef_filter_16_3_rvv(void *dest, int dstride, const uint16_t *in,
                          int pri_strength, int sec_strength, int dir,
                          int pri_damping, int sec_damping, int coeff_shift,
                          int block_width, int block_height) {
  (void)pri_strength;
  (void)sec_strength;
  (void)dir;
  (void)pri_damping;
  (void)sec_damping;
  (void)coeff_shift;

  if (block_width == 8) {
    uint16_t *dst16 = (uint16_t *)dest;

    int h = block_height;
    const size_t vl = block_width;
    do {
      const vuint16m1_t px = __riscv_vle16_v_u16m1(in, vl);
      __riscv_vse16_v_u16m1(dst16, px, vl);

      in += CDEF_BSTRIDE;
      dst16 += dstride;
    } while (--h != 0);
  } else {
    uint16_t *dst16 = (uint16_t *)dest;

    int h = block_height;
    const size_t vl = block_width << 1;
    do {
      const vint16m1_t px =
          load_strided_i16_4x2((int16_t *)in, CDEF_BSTRIDE, vl);
      vuint16m1_t vdst = __riscv_vreinterpret_v_i16m1_u16m1(px);
      store_strided_u16_4x2(dst16, vdst, dstride, vl);

      in += 2 * CDEF_BSTRIDE;
      dst16 += 2 * dstride;
      h -= 2;
    } while (h != 0);
  }
}
