/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>

#include "config/aom_dsp_rtcd.h"
#include "config/aom_config.h"

#include "aom/aom_integer.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"

static INLINE uint8x8_t lpf_mask(uint8x8_t p3q3, uint8x8_t p2q2, uint8x8_t p1q1,
                                 uint8x8_t p0q0, const uint8_t blimit,
                                 const uint8_t limit) {
  // Calculate mask values for four samples
  uint32x2x2_t p0q0_p1q1;
  uint16x8_t temp_16x8;
  uint16x4_t temp0_16x4, temp1_16x4;
  uint8x8_t mask_8x8, temp_8x8;
  const uint8x8_t limit_8x8 = vdup_n_u8(limit);
  const uint16x4_t blimit_16x4 = vdup_n_u16((uint16_t)blimit);

  mask_8x8 = vabd_u8(p3q3, p2q2);
  mask_8x8 = vmax_u8(mask_8x8, vabd_u8(p2q2, p1q1));
  mask_8x8 = vmax_u8(mask_8x8, vabd_u8(p1q1, p0q0));
  mask_8x8 = vcle_u8(mask_8x8, limit_8x8);

  temp_8x8 = vreinterpret_u8_u32(vrev64_u32(vreinterpret_u32_u8(mask_8x8)));
  mask_8x8 = vand_u8(mask_8x8, temp_8x8);

  p0q0_p1q1 = vtrn_u32(vreinterpret_u32_u8(p0q0), vreinterpret_u32_u8(p1q1));
  temp_8x8 = vabd_u8(vreinterpret_u8_u32(p0q0_p1q1.val[0]),
                     vreinterpret_u8_u32(p0q0_p1q1.val[1]));
  temp_16x8 = vmovl_u8(temp_8x8);
  temp0_16x4 = vshl_n_u16(vget_low_u16(temp_16x8), 1);
  temp1_16x4 = vshr_n_u16(vget_high_u16(temp_16x8), 1);
  temp0_16x4 = vadd_u16(temp0_16x4, temp1_16x4);
  temp0_16x4 = vcle_u16(temp0_16x4, blimit_16x4);
  temp_8x8 = vmovn_u16(vcombine_u16(temp0_16x4, temp0_16x4));

  mask_8x8 = vand_u8(mask_8x8, temp_8x8);

  return mask_8x8;
}

static INLINE uint8x8_t lpf_mask2(uint8x8_t p1q1, uint8x8_t p0q0,
                                  const uint8_t blimit, const uint8_t limit) {
  uint32x2x2_t p0q0_p1q1;
  uint16x8_t temp_16x8;
  uint16x4_t temp0_16x4, temp1_16x4;
  const uint16x4_t blimit_16x4 = vdup_n_u16(blimit);
  const uint8x8_t limit_8x8 = vdup_n_u8(limit);
  uint8x8_t mask_8x8, temp_8x8;

  mask_8x8 = vabd_u8(p1q1, p0q0);
  mask_8x8 = vcle_u8(mask_8x8, limit_8x8);

  temp_8x8 = vreinterpret_u8_u32(vrev64_u32(vreinterpret_u32_u8(mask_8x8)));
  mask_8x8 = vand_u8(mask_8x8, temp_8x8);

  p0q0_p1q1 = vtrn_u32(vreinterpret_u32_u8(p0q0), vreinterpret_u32_u8(p1q1));
  temp_8x8 = vabd_u8(vreinterpret_u8_u32(p0q0_p1q1.val[0]),
                     vreinterpret_u8_u32(p0q0_p1q1.val[1]));
  temp_16x8 = vmovl_u8(temp_8x8);
  temp0_16x4 = vshl_n_u16(vget_low_u16(temp_16x8), 1);
  temp1_16x4 = vshr_n_u16(vget_high_u16(temp_16x8), 1);
  temp0_16x4 = vadd_u16(temp0_16x4, temp1_16x4);
  temp0_16x4 = vcle_u16(temp0_16x4, blimit_16x4);
  temp_8x8 = vmovn_u16(vcombine_u16(temp0_16x4, temp0_16x4));

  mask_8x8 = vand_u8(mask_8x8, temp_8x8);

  return mask_8x8;
}

static INLINE uint8x8_t lpf_flat_mask4(uint8x8_t p3q3, uint8x8_t p2q2,
                                       uint8x8_t p1q1, uint8x8_t p0q0) {
  const uint8x8_t thresh_8x8 = vdup_n_u8(1);  // for bd==8 threshold is always 1
  uint8x8_t flat_8x8, temp_8x8;

  flat_8x8 = vabd_u8(p1q1, p0q0);
  flat_8x8 = vmax_u8(flat_8x8, vabd_u8(p2q2, p0q0));
  flat_8x8 = vmax_u8(flat_8x8, vabd_u8(p3q3, p0q0));
  flat_8x8 = vcle_u8(flat_8x8, thresh_8x8);

  temp_8x8 = vreinterpret_u8_u32(vrev64_u32(vreinterpret_u32_u8(flat_8x8)));
  flat_8x8 = vand_u8(flat_8x8, temp_8x8);

  return flat_8x8;
}

static INLINE uint8x8_t lpf_flat_mask3(uint8x8_t p2q2, uint8x8_t p1q1,
                                       uint8x8_t p0q0) {
  const uint8x8_t thresh_8x8 = vdup_n_u8(1);  // for bd==8 threshold is always 1
  uint8x8_t flat_8x8, temp_8x8;

  flat_8x8 = vabd_u8(p1q1, p0q0);
  flat_8x8 = vmax_u8(flat_8x8, vabd_u8(p2q2, p0q0));
  flat_8x8 = vcle_u8(flat_8x8, thresh_8x8);

  temp_8x8 = vreinterpret_u8_u32(vrev64_u32(vreinterpret_u32_u8(flat_8x8)));
  flat_8x8 = vand_u8(flat_8x8, temp_8x8);

  return flat_8x8;
}

static INLINE uint8x8_t lpf_mask3_chroma(uint8x8_t p2q2, uint8x8_t p1q1,
                                         uint8x8_t p0q0, const uint8_t blimit,
                                         const uint8_t limit) {
  // Calculate mask3 values for four samples
  uint32x2x2_t p0q0_p1q1;
  uint16x8_t temp_16x8;
  uint16x4_t temp0_16x4, temp1_16x4;
  uint8x8_t mask_8x8, temp_8x8;
  const uint8x8_t limit_8x8 = vdup_n_u8(limit);
  const uint16x4_t blimit_16x4 = vdup_n_u16((uint16_t)blimit);

  mask_8x8 = vabd_u8(p2q2, p1q1);
  mask_8x8 = vmax_u8(mask_8x8, vabd_u8(p1q1, p0q0));
  mask_8x8 = vcle_u8(mask_8x8, limit_8x8);

  temp_8x8 = vreinterpret_u8_u32(vrev64_u32(vreinterpret_u32_u8(mask_8x8)));
  mask_8x8 = vand_u8(mask_8x8, temp_8x8);

  p0q0_p1q1 = vtrn_u32(vreinterpret_u32_u8(p0q0), vreinterpret_u32_u8(p1q1));
  temp_8x8 = vabd_u8(vreinterpret_u8_u32(p0q0_p1q1.val[0]),
                     vreinterpret_u8_u32(p0q0_p1q1.val[1]));
  temp_16x8 = vmovl_u8(temp_8x8);
  temp0_16x4 = vshl_n_u16(vget_low_u16(temp_16x8), 1);
  temp1_16x4 = vshr_n_u16(vget_high_u16(temp_16x8), 1);
  temp0_16x4 = vadd_u16(temp0_16x4, temp1_16x4);
  temp0_16x4 = vcle_u16(temp0_16x4, blimit_16x4);
  temp_8x8 = vmovn_u16(vcombine_u16(temp0_16x4, temp0_16x4));

  mask_8x8 = vand_u8(mask_8x8, temp_8x8);

  return mask_8x8;
}

static void lpf_14_neon(uint8x8_t *p6q6, uint8x8_t *p5q5, uint8x8_t *p4q4,
                        uint8x8_t *p3q3, uint8x8_t *p2q2, uint8x8_t *p1q1,
                        uint8x8_t *p0q0, const uint8_t blimit,
                        const uint8_t limit, const uint8_t thresh) {
  uint16x8_t out;
  uint8x8_t out_f14_pq0, out_f14_pq1, out_f14_pq2, out_f14_pq3, out_f14_pq4,
      out_f14_pq5;
  uint8x8_t out_f7_pq0, out_f7_pq1, out_f7_pq2;
  uint8x8_t out_f4_pq0, out_f4_pq1;
  uint8x8_t mask_8x8, flat_8x8, flat2_8x8;
  uint8x8_t q0p0, q1p1, q2p2;

  // Calculate filter masks
  mask_8x8 = lpf_mask(*p3q3, *p2q2, *p1q1, *p0q0, blimit, limit);
  flat_8x8 = lpf_flat_mask4(*p3q3, *p2q2, *p1q1, *p0q0);
  flat2_8x8 = lpf_flat_mask4(*p6q6, *p5q5, *p4q4, *p0q0);
  {
    // filter 4
    int32x2x2_t ps0_qs0, ps1_qs1;
    int16x8_t filter_s16;
    const uint8x8_t thresh_f4 = vdup_n_u8(thresh);
    uint8x8_t temp0_8x8, temp1_8x8;
    int8x8_t ps0_s8, ps1_s8, qs0_s8, qs1_s8, temp_s8;
    int8x8_t op0, oq0, op1, oq1;
    int8x8_t pq_s0, pq_s1;
    int8x8_t filter_s8, filter1_s8, filter2_s8;
    int8x8_t hev_8x8;
    const int8x8_t sign_mask = vdup_n_s8(0x80);
    const int8x8_t val_4 = vdup_n_s8(4);
    const int8x8_t val_3 = vdup_n_s8(3);

    pq_s0 = veor_s8(vreinterpret_s8_u8(*p0q0), sign_mask);
    pq_s1 = veor_s8(vreinterpret_s8_u8(*p1q1), sign_mask);

    ps0_qs0 = vtrn_s32(vreinterpret_s32_s8(pq_s0), vreinterpret_s32_s8(pq_s0));
    ps1_qs1 = vtrn_s32(vreinterpret_s32_s8(pq_s1), vreinterpret_s32_s8(pq_s1));
    ps0_s8 = vreinterpret_s8_s32(ps0_qs0.val[0]);
    qs0_s8 = vreinterpret_s8_s32(ps0_qs0.val[1]);
    ps1_s8 = vreinterpret_s8_s32(ps1_qs1.val[0]);
    qs1_s8 = vreinterpret_s8_s32(ps1_qs1.val[1]);

    // hev_mask
    temp0_8x8 = vcgt_u8(vabd_u8(*p0q0, *p1q1), thresh_f4);
    temp1_8x8 = vreinterpret_u8_u32(vrev64_u32(vreinterpret_u32_u8(temp0_8x8)));
    hev_8x8 = vreinterpret_s8_u8(vorr_u8(temp0_8x8, temp1_8x8));

    // add outer taps if we have high edge variance
    filter_s8 = vqsub_s8(ps1_s8, qs1_s8);
    filter_s8 = vand_s8(filter_s8, hev_8x8);

    // inner taps
    temp_s8 = vqsub_s8(qs0_s8, ps0_s8);
    filter_s16 = vmovl_s8(filter_s8);
    filter_s16 = vmlal_s8(filter_s16, temp_s8, val_3);
    filter_s8 = vqmovn_s16(filter_s16);
    filter_s8 = vand_s8(filter_s8, vreinterpret_s8_u8(mask_8x8));

    filter1_s8 = vqadd_s8(filter_s8, val_4);
    filter2_s8 = vqadd_s8(filter_s8, val_3);
    filter1_s8 = vshr_n_s8(filter1_s8, 3);
    filter2_s8 = vshr_n_s8(filter2_s8, 3);

    oq0 = veor_s8(vqsub_s8(qs0_s8, filter1_s8), sign_mask);
    op0 = veor_s8(vqadd_s8(ps0_s8, filter2_s8), sign_mask);

    hev_8x8 = vmvn_s8(hev_8x8);
    filter_s8 = vrshr_n_s8(filter1_s8, 1);
    filter_s8 = vand_s8(filter_s8, hev_8x8);

    oq1 = veor_s8(vqsub_s8(qs1_s8, filter_s8), sign_mask);
    op1 = veor_s8(vqadd_s8(ps1_s8, filter_s8), sign_mask);

    out_f4_pq0 = vreinterpret_u8_s8(vext_s8(op0, oq0, 4));
    out_f4_pq1 = vreinterpret_u8_s8(vext_s8(op1, oq1, 4));
  }
  // reverse p and q
  q0p0 = vreinterpret_u8_u32(vrev64_u32(vreinterpret_u32_u8(*p0q0)));
  q1p1 = vreinterpret_u8_u32(vrev64_u32(vreinterpret_u32_u8(*p1q1)));
  q2p2 = vreinterpret_u8_u32(vrev64_u32(vreinterpret_u32_u8(*p2q2)));
  {
    // filter 8
    uint16x8_t out_pq0, out_pq1, out_pq2;
    out = vaddl_u8(*p3q3, *p2q2);
    out = vaddw_u8(out, *p1q1);
    out = vaddw_u8(out, *p0q0);

    out = vaddw_u8(out, q0p0);
    out_pq1 = vaddw_u8(out, *p3q3);
    out_pq2 = vaddw_u8(out_pq1, *p3q3);
    out_pq2 = vaddw_u8(out_pq2, *p2q2);
    out_pq1 = vaddw_u8(out_pq1, *p1q1);
    out_pq1 = vaddw_u8(out_pq1, q1p1);

    out_pq0 = vaddw_u8(out, *p0q0);
    out_pq0 = vaddw_u8(out_pq0, q1p1);
    out_pq0 = vaddw_u8(out_pq0, q2p2);

    out_f7_pq0 = vrshrn_n_u16(out_pq0, 3);
    out_f7_pq1 = vrshrn_n_u16(out_pq1, 3);
    out_f7_pq2 = vrshrn_n_u16(out_pq2, 3);
  }
  {
    // filter 14
    uint16x8_t out_pq0, out_pq1, out_pq2, out_pq3, out_pq4, out_pq5;
    uint16x8_t p6q6_2, p6q6_temp, qp_sum;
    uint8x8_t qp_rev;

    out = vaddw_u8(out, *p4q4);
    out = vaddw_u8(out, *p5q5);
    out = vaddw_u8(out, *p6q6);

    out_pq5 = vaddw_u8(out, *p4q4);
    out_pq4 = vaddw_u8(out_pq5, *p3q3);
    out_pq3 = vaddw_u8(out_pq4, *p2q2);

    out_pq5 = vaddw_u8(out_pq5, *p5q5);
    out_pq4 = vaddw_u8(out_pq4, *p5q5);

    out_pq0 = vaddw_u8(out, *p1q1);
    out_pq1 = vaddw_u8(out_pq0, *p2q2);
    out_pq2 = vaddw_u8(out_pq1, *p3q3);

    out_pq0 = vaddw_u8(out_pq0, *p0q0);
    out_pq1 = vaddw_u8(out_pq1, *p0q0);

    out_pq1 = vaddw_u8(out_pq1, *p6q6);
    p6q6_2 = vaddl_u8(*p6q6, *p6q6);
    out_pq2 = vaddq_u16(out_pq2, p6q6_2);
    p6q6_temp = vaddw_u8(p6q6_2, *p6q6);
    out_pq3 = vaddq_u16(out_pq3, p6q6_temp);
    p6q6_temp = vaddw_u8(p6q6_temp, *p6q6);
    out_pq4 = vaddq_u16(out_pq4, p6q6_temp);
    p6q6_temp = vaddq_u16(p6q6_temp, p6q6_2);
    out_pq5 = vaddq_u16(out_pq5, p6q6_temp);

    out_pq4 = vaddw_u8(out_pq4, q1p1);

    qp_sum = vaddl_u8(q2p2, q1p1);
    out_pq3 = vaddq_u16(out_pq3, qp_sum);

    qp_rev = vreinterpret_u8_u32(vrev64_u32(vreinterpret_u32_u8(*p3q3)));
    qp_sum = vaddw_u8(qp_sum, qp_rev);
    out_pq2 = vaddq_u16(out_pq2, qp_sum);

    qp_rev = vreinterpret_u8_u32(vrev64_u32(vreinterpret_u32_u8(*p4q4)));
    qp_sum = vaddw_u8(qp_sum, qp_rev);
    out_pq1 = vaddq_u16(out_pq1, qp_sum);

    qp_rev = vreinterpret_u8_u32(vrev64_u32(vreinterpret_u32_u8(*p5q5)));
    qp_sum = vaddw_u8(qp_sum, qp_rev);
    out_pq0 = vaddq_u16(out_pq0, qp_sum);

    out_pq0 = vaddw_u8(out_pq0, q0p0);

    out_f14_pq0 = vrshrn_n_u16(out_pq0, 4);
    out_f14_pq1 = vrshrn_n_u16(out_pq1, 4);
    out_f14_pq2 = vrshrn_n_u16(out_pq2, 4);
    out_f14_pq3 = vrshrn_n_u16(out_pq3, 4);
    out_f14_pq4 = vrshrn_n_u16(out_pq4, 4);
    out_f14_pq5 = vrshrn_n_u16(out_pq5, 4);
  }
  {
    uint8x8_t filter4_cond, filter8_cond, filter14_cond;
    filter8_cond = vand_u8(flat_8x8, mask_8x8);
    filter4_cond = vmvn_u8(filter8_cond);
    filter14_cond = vand_u8(filter8_cond, flat2_8x8);

    // filter4 outputs
    *p0q0 = vbsl_u8(filter4_cond, out_f4_pq0, *p0q0);
    *p1q1 = vbsl_u8(filter4_cond, out_f4_pq1, *p1q1);

    // filter8 outputs
    *p0q0 = vbsl_u8(filter8_cond, out_f7_pq0, *p0q0);
    *p1q1 = vbsl_u8(filter8_cond, out_f7_pq1, *p1q1);
    *p2q2 = vbsl_u8(filter8_cond, out_f7_pq2, *p2q2);

    // filter14 outputs
    *p0q0 = vbsl_u8(filter14_cond, out_f14_pq0, *p0q0);
    *p1q1 = vbsl_u8(filter14_cond, out_f14_pq1, *p1q1);
    *p2q2 = vbsl_u8(filter14_cond, out_f14_pq2, *p2q2);
    *p3q3 = vbsl_u8(filter14_cond, out_f14_pq3, *p3q3);
    *p4q4 = vbsl_u8(filter14_cond, out_f14_pq4, *p4q4);
    *p5q5 = vbsl_u8(filter14_cond, out_f14_pq5, *p5q5);
  }
}

static void lpf_8_neon(uint8x8_t *p3q3, uint8x8_t *p2q2, uint8x8_t *p1q1,
                       uint8x8_t *p0q0, const uint8_t blimit,
                       const uint8_t limit, const uint8_t thresh) {
  uint16x8_t out;
  uint8x8_t out_f7_pq0, out_f7_pq1, out_f7_pq2;
  uint8x8_t out_f4_pq0, out_f4_pq1;
  uint8x8_t mask_8x8, flat_8x8;

  // Calculate filter masks
  mask_8x8 = lpf_mask(*p3q3, *p2q2, *p1q1, *p0q0, blimit, limit);
  flat_8x8 = lpf_flat_mask4(*p3q3, *p2q2, *p1q1, *p0q0);
  {
    // filter 4
    int32x2x2_t ps0_qs0, ps1_qs1;
    int16x8_t filter_s16;
    const uint8x8_t thresh_f4 = vdup_n_u8(thresh);
    uint8x8_t temp0_8x8, temp1_8x8;
    int8x8_t ps0_s8, ps1_s8, qs0_s8, qs1_s8, temp_s8;
    int8x8_t op0, oq0, op1, oq1;
    int8x8_t pq_s0, pq_s1;
    int8x8_t filter_s8, filter1_s8, filter2_s8;
    int8x8_t hev_8x8;
    const int8x8_t sign_mask = vdup_n_s8(0x80);
    const int8x8_t val_4 = vdup_n_s8(4);
    const int8x8_t val_3 = vdup_n_s8(3);

    pq_s0 = veor_s8(vreinterpret_s8_u8(*p0q0), sign_mask);
    pq_s1 = veor_s8(vreinterpret_s8_u8(*p1q1), sign_mask);

    ps0_qs0 = vtrn_s32(vreinterpret_s32_s8(pq_s0), vreinterpret_s32_s8(pq_s0));
    ps1_qs1 = vtrn_s32(vreinterpret_s32_s8(pq_s1), vreinterpret_s32_s8(pq_s1));
    ps0_s8 = vreinterpret_s8_s32(ps0_qs0.val[0]);
    qs0_s8 = vreinterpret_s8_s32(ps0_qs0.val[1]);
    ps1_s8 = vreinterpret_s8_s32(ps1_qs1.val[0]);
    qs1_s8 = vreinterpret_s8_s32(ps1_qs1.val[1]);

    // hev_mask
    temp0_8x8 = vcgt_u8(vabd_u8(*p0q0, *p1q1), thresh_f4);
    temp1_8x8 = vreinterpret_u8_u32(vrev64_u32(vreinterpret_u32_u8(temp0_8x8)));
    hev_8x8 = vreinterpret_s8_u8(vorr_u8(temp0_8x8, temp1_8x8));

    // add outer taps if we have high edge variance
    filter_s8 = vqsub_s8(ps1_s8, qs1_s8);
    filter_s8 = vand_s8(filter_s8, hev_8x8);

    // inner taps
    temp_s8 = vqsub_s8(qs0_s8, ps0_s8);
    filter_s16 = vmovl_s8(filter_s8);
    filter_s16 = vmlal_s8(filter_s16, temp_s8, val_3);
    filter_s8 = vqmovn_s16(filter_s16);
    filter_s8 = vand_s8(filter_s8, vreinterpret_s8_u8(mask_8x8));

    filter1_s8 = vqadd_s8(filter_s8, val_4);
    filter2_s8 = vqadd_s8(filter_s8, val_3);
    filter1_s8 = vshr_n_s8(filter1_s8, 3);
    filter2_s8 = vshr_n_s8(filter2_s8, 3);

    oq0 = veor_s8(vqsub_s8(qs0_s8, filter1_s8), sign_mask);
    op0 = veor_s8(vqadd_s8(ps0_s8, filter2_s8), sign_mask);

    hev_8x8 = vmvn_s8(hev_8x8);
    filter_s8 = vrshr_n_s8(filter1_s8, 1);
    filter_s8 = vand_s8(filter_s8, hev_8x8);

    oq1 = veor_s8(vqsub_s8(qs1_s8, filter_s8), sign_mask);
    op1 = veor_s8(vqadd_s8(ps1_s8, filter_s8), sign_mask);

    out_f4_pq0 = vreinterpret_u8_s8(vext_s8(op0, oq0, 4));
    out_f4_pq1 = vreinterpret_u8_s8(vext_s8(op1, oq1, 4));
  }
  {
    // filter 8
    uint16x8_t out_pq0, out_pq1, out_pq2;
    uint8x8_t q0p0, q1p1, q2p2;

    out = vaddl_u8(*p3q3, *p2q2);
    out = vaddw_u8(out, *p1q1);
    out = vaddw_u8(out, *p0q0);

    // reverse p and q
    q0p0 = vreinterpret_u8_u32(vrev64_u32(vreinterpret_u32_u8(*p0q0)));
    q1p1 = vreinterpret_u8_u32(vrev64_u32(vreinterpret_u32_u8(*p1q1)));
    q2p2 = vreinterpret_u8_u32(vrev64_u32(vreinterpret_u32_u8(*p2q2)));

    out = vaddw_u8(out, q0p0);
    out_pq1 = vaddw_u8(out, *p3q3);
    out_pq2 = vaddw_u8(out_pq1, *p3q3);
    out_pq2 = vaddw_u8(out_pq2, *p2q2);
    out_pq1 = vaddw_u8(out_pq1, *p1q1);
    out_pq1 = vaddw_u8(out_pq1, q1p1);

    out_pq0 = vaddw_u8(out, *p0q0);
    out_pq0 = vaddw_u8(out_pq0, q1p1);
    out_pq0 = vaddw_u8(out_pq0, q2p2);

    out_f7_pq0 = vrshrn_n_u16(out_pq0, 3);
    out_f7_pq1 = vrshrn_n_u16(out_pq1, 3);
    out_f7_pq2 = vrshrn_n_u16(out_pq2, 3);
  }
  {
    uint8x8_t filter4_cond, filter8_cond;
    filter8_cond = vand_u8(flat_8x8, mask_8x8);
    filter4_cond = vmvn_u8(filter8_cond);

    // filter4 outputs
    *p0q0 = vbsl_u8(filter4_cond, out_f4_pq0, *p0q0);
    *p1q1 = vbsl_u8(filter4_cond, out_f4_pq1, *p1q1);

    // filter8 outputs
    *p0q0 = vbsl_u8(filter8_cond, out_f7_pq0, *p0q0);
    *p1q1 = vbsl_u8(filter8_cond, out_f7_pq1, *p1q1);
    *p2q2 = vbsl_u8(filter8_cond, out_f7_pq2, *p2q2);
  }
}

static void lpf_6_neon(uint8x8_t *p2q2, uint8x8_t *p1q1, uint8x8_t *p0q0,
                       const uint8_t blimit, const uint8_t limit,
                       const uint8_t thresh) {
  uint16x8_t out;
  uint8x8_t out_f6_pq0, out_f6_pq1;
  uint8x8_t out_f4_pq0, out_f4_pq1;
  uint8x8_t mask_8x8, flat_8x8;

  // Calculate filter masks
  mask_8x8 = lpf_mask3_chroma(*p2q2, *p1q1, *p0q0, blimit, limit);
  flat_8x8 = lpf_flat_mask3(*p2q2, *p1q1, *p0q0);
  {
    // filter 4
    int32x2x2_t ps0_qs0, ps1_qs1;
    int16x8_t filter_s16;
    const uint8x8_t thresh_f4 = vdup_n_u8(thresh);
    uint8x8_t temp0_8x8, temp1_8x8;
    int8x8_t ps0_s8, ps1_s8, qs0_s8, qs1_s8, temp_s8;
    int8x8_t op0, oq0, op1, oq1;
    int8x8_t pq_s0, pq_s1;
    int8x8_t filter_s8, filter1_s8, filter2_s8;
    int8x8_t hev_8x8;
    const int8x8_t sign_mask = vdup_n_s8(0x80);
    const int8x8_t val_4 = vdup_n_s8(4);
    const int8x8_t val_3 = vdup_n_s8(3);

    pq_s0 = veor_s8(vreinterpret_s8_u8(*p0q0), sign_mask);
    pq_s1 = veor_s8(vreinterpret_s8_u8(*p1q1), sign_mask);

    ps0_qs0 = vtrn_s32(vreinterpret_s32_s8(pq_s0), vreinterpret_s32_s8(pq_s0));
    ps1_qs1 = vtrn_s32(vreinterpret_s32_s8(pq_s1), vreinterpret_s32_s8(pq_s1));
    ps0_s8 = vreinterpret_s8_s32(ps0_qs0.val[0]);
    qs0_s8 = vreinterpret_s8_s32(ps0_qs0.val[1]);
    ps1_s8 = vreinterpret_s8_s32(ps1_qs1.val[0]);
    qs1_s8 = vreinterpret_s8_s32(ps1_qs1.val[1]);

    // hev_mask
    temp0_8x8 = vcgt_u8(vabd_u8(*p0q0, *p1q1), thresh_f4);
    temp1_8x8 = vreinterpret_u8_u32(vrev64_u32(vreinterpret_u32_u8(temp0_8x8)));
    hev_8x8 = vreinterpret_s8_u8(vorr_u8(temp0_8x8, temp1_8x8));

    // add outer taps if we have high edge variance
    filter_s8 = vqsub_s8(ps1_s8, qs1_s8);
    filter_s8 = vand_s8(filter_s8, hev_8x8);

    // inner taps
    temp_s8 = vqsub_s8(qs0_s8, ps0_s8);
    filter_s16 = vmovl_s8(filter_s8);
    filter_s16 = vmlal_s8(filter_s16, temp_s8, val_3);
    filter_s8 = vqmovn_s16(filter_s16);
    filter_s8 = vand_s8(filter_s8, vreinterpret_s8_u8(mask_8x8));

    filter1_s8 = vqadd_s8(filter_s8, val_4);
    filter2_s8 = vqadd_s8(filter_s8, val_3);
    filter1_s8 = vshr_n_s8(filter1_s8, 3);
    filter2_s8 = vshr_n_s8(filter2_s8, 3);

    oq0 = veor_s8(vqsub_s8(qs0_s8, filter1_s8), sign_mask);
    op0 = veor_s8(vqadd_s8(ps0_s8, filter2_s8), sign_mask);

    filter_s8 = vrshr_n_s8(filter1_s8, 1);
    filter_s8 = vbic_s8(filter_s8, hev_8x8);

    oq1 = veor_s8(vqsub_s8(qs1_s8, filter_s8), sign_mask);
    op1 = veor_s8(vqadd_s8(ps1_s8, filter_s8), sign_mask);

    out_f4_pq0 = vreinterpret_u8_s8(vext_s8(op0, oq0, 4));
    out_f4_pq1 = vreinterpret_u8_s8(vext_s8(op1, oq1, 4));
  }
  {
    // filter 6
    uint16x8_t out_pq0, out_pq1;
    uint8x8_t pq_rev;

    out = vaddl_u8(*p0q0, *p1q1);
    out = vaddq_u16(out, out);
    out = vaddw_u8(out, *p2q2);

    pq_rev = vreinterpret_u8_u32(vrev64_u32(vreinterpret_u32_u8(*p0q0)));
    out = vaddw_u8(out, pq_rev);

    out_pq0 = vaddw_u8(out, pq_rev);
    pq_rev = vreinterpret_u8_u32(vrev64_u32(vreinterpret_u32_u8(*p1q1)));
    out_pq0 = vaddw_u8(out_pq0, pq_rev);

    out_pq1 = vaddw_u8(out, *p2q2);
    out_pq1 = vaddw_u8(out_pq1, *p2q2);

    out_f6_pq0 = vrshrn_n_u16(out_pq0, 3);
    out_f6_pq1 = vrshrn_n_u16(out_pq1, 3);
  }
  {
    uint8x8_t filter4_cond, filter6_cond;
    filter6_cond = vand_u8(flat_8x8, mask_8x8);
    filter4_cond = vmvn_u8(filter6_cond);

    // filter4 outputs
    *p0q0 = vbsl_u8(filter4_cond, out_f4_pq0, *p0q0);
    *p1q1 = vbsl_u8(filter4_cond, out_f4_pq1, *p1q1);

    // filter6 outputs
    *p0q0 = vbsl_u8(filter6_cond, out_f6_pq0, *p0q0);
    *p1q1 = vbsl_u8(filter6_cond, out_f6_pq1, *p1q1);
  }
}

static void lpf_4_neon(uint8x8_t *p1q1, uint8x8_t *p0q0, const uint8_t blimit,
                       const uint8_t limit, const uint8_t thresh) {
  int32x2x2_t ps0_qs0, ps1_qs1;
  int16x8_t filter_s16;
  const uint8x8_t thresh_f4 = vdup_n_u8(thresh);
  uint8x8_t mask_8x8, temp0_8x8, temp1_8x8;
  int8x8_t ps0_s8, ps1_s8, qs0_s8, qs1_s8, temp_s8;
  int8x8_t op0, oq0, op1, oq1;
  int8x8_t pq_s0, pq_s1;
  int8x8_t filter_s8, filter1_s8, filter2_s8;
  int8x8_t hev_8x8;
  const int8x8_t sign_mask = vdup_n_s8(0x80);
  const int8x8_t val_4 = vdup_n_s8(4);
  const int8x8_t val_3 = vdup_n_s8(3);

  // Calculate filter mask
  mask_8x8 = lpf_mask2(*p1q1, *p0q0, blimit, limit);

  pq_s0 = veor_s8(vreinterpret_s8_u8(*p0q0), sign_mask);
  pq_s1 = veor_s8(vreinterpret_s8_u8(*p1q1), sign_mask);

  ps0_qs0 = vtrn_s32(vreinterpret_s32_s8(pq_s0), vreinterpret_s32_s8(pq_s0));
  ps1_qs1 = vtrn_s32(vreinterpret_s32_s8(pq_s1), vreinterpret_s32_s8(pq_s1));
  ps0_s8 = vreinterpret_s8_s32(ps0_qs0.val[0]);
  qs0_s8 = vreinterpret_s8_s32(ps0_qs0.val[1]);
  ps1_s8 = vreinterpret_s8_s32(ps1_qs1.val[0]);
  qs1_s8 = vreinterpret_s8_s32(ps1_qs1.val[1]);

  // hev_mask
  temp0_8x8 = vcgt_u8(vabd_u8(*p0q0, *p1q1), thresh_f4);
  temp1_8x8 = vreinterpret_u8_u32(vrev64_u32(vreinterpret_u32_u8(temp0_8x8)));
  hev_8x8 = vreinterpret_s8_u8(vorr_u8(temp0_8x8, temp1_8x8));

  // add outer taps if we have high edge variance
  filter_s8 = vqsub_s8(ps1_s8, qs1_s8);
  filter_s8 = vand_s8(filter_s8, hev_8x8);

  // inner taps
  temp_s8 = vqsub_s8(qs0_s8, ps0_s8);
  filter_s16 = vmovl_s8(filter_s8);
  filter_s16 = vmlal_s8(filter_s16, temp_s8, val_3);
  filter_s8 = vqmovn_s16(filter_s16);
  filter_s8 = vand_s8(filter_s8, vreinterpret_s8_u8(mask_8x8));

  filter1_s8 = vqadd_s8(filter_s8, val_4);
  filter2_s8 = vqadd_s8(filter_s8, val_3);
  filter1_s8 = vshr_n_s8(filter1_s8, 3);
  filter2_s8 = vshr_n_s8(filter2_s8, 3);

  oq0 = veor_s8(vqsub_s8(qs0_s8, filter1_s8), sign_mask);
  op0 = veor_s8(vqadd_s8(ps0_s8, filter2_s8), sign_mask);

  filter_s8 = vrshr_n_s8(filter1_s8, 1);
  filter_s8 = vbic_s8(filter_s8, hev_8x8);

  oq1 = veor_s8(vqsub_s8(qs1_s8, filter_s8), sign_mask);
  op1 = veor_s8(vqadd_s8(ps1_s8, filter_s8), sign_mask);

  *p0q0 = vreinterpret_u8_s8(vext_s8(op0, oq0, 4));
  *p1q1 = vreinterpret_u8_s8(vext_s8(op1, oq1, 4));
}

void aom_lpf_vertical_14_neon(uint8_t *src, int stride, const uint8_t *blimit,
                              const uint8_t *limit, const uint8_t *thresh) {
  uint8x16_t row0, row1, row2, row3;
  uint8x8_t pxp3, p6p2, p5p1, p4p0;
  uint8x8_t q0q4, q1q5, q2q6, q3qy;
  uint32x2x2_t p6q6_p2q2, p5q5_p1q1, p4q4_p0q0, pxqx_p3q3;
  uint32x2_t pq_rev;
  uint8x8_t p0q0, p1q1, p2q2, p3q3, p4q4, p5q5, p6q6;

  // row0: x p6 p5 p4 p3 p2 p1 p0 | q0 q1 q2 q3 q4 q5 q6 y
  // row1: x p6 p5 p4 p3 p2 p1 p0 | q0 q1 q2 q3 q4 q5 q6 y
  // row2: x p6 p5 p4 p3 p2 p1 p0 | q0 q1 q2 q3 q4 q5 q6 y
  // row3: x p6 p5 p4 p3 p2 p1 p0 | q0 q1 q2 q3 q4 q5 q6 y
  load_u8_16x4(src - 8, stride, &row0, &row1, &row2, &row3);

  pxp3 = vget_low_u8(row0);
  p6p2 = vget_low_u8(row1);
  p5p1 = vget_low_u8(row2);
  p4p0 = vget_low_u8(row3);
  transpose_u8_8x4(&pxp3, &p6p2, &p5p1, &p4p0);

  q0q4 = vget_high_u8(row0);
  q1q5 = vget_high_u8(row1);
  q2q6 = vget_high_u8(row2);
  q3qy = vget_high_u8(row3);
  transpose_u8_8x4(&q0q4, &q1q5, &q2q6, &q3qy);

  pq_rev = vrev64_u32(vreinterpret_u32_u8(q3qy));
  pxqx_p3q3 = vtrn_u32(vreinterpret_u32_u8(pxp3), pq_rev);

  pq_rev = vrev64_u32(vreinterpret_u32_u8(q1q5));
  p5q5_p1q1 = vtrn_u32(vreinterpret_u32_u8(p5p1), pq_rev);

  pq_rev = vrev64_u32(vreinterpret_u32_u8(q0q4));
  p4q4_p0q0 = vtrn_u32(vreinterpret_u32_u8(p4p0), pq_rev);

  pq_rev = vrev64_u32(vreinterpret_u32_u8(q2q6));
  p6q6_p2q2 = vtrn_u32(vreinterpret_u32_u8(p6p2), pq_rev);

  p0q0 = vreinterpret_u8_u32(p4q4_p0q0.val[1]);
  p1q1 = vreinterpret_u8_u32(p5q5_p1q1.val[1]);
  p2q2 = vreinterpret_u8_u32(p6q6_p2q2.val[1]);
  p3q3 = vreinterpret_u8_u32(pxqx_p3q3.val[1]);
  p4q4 = vreinterpret_u8_u32(p4q4_p0q0.val[0]);
  p5q5 = vreinterpret_u8_u32(p5q5_p1q1.val[0]);
  p6q6 = vreinterpret_u8_u32(p6q6_p2q2.val[0]);

  lpf_14_neon(&p6q6, &p5q5, &p4q4, &p3q3, &p2q2, &p1q1, &p0q0, *blimit, *limit,
              *thresh);

  pxqx_p3q3 = vtrn_u32(pxqx_p3q3.val[0], vreinterpret_u32_u8(p3q3));
  p5q5_p1q1 = vtrn_u32(vreinterpret_u32_u8(p5q5), vreinterpret_u32_u8(p1q1));
  p4q4_p0q0 = vtrn_u32(vreinterpret_u32_u8(p4q4), vreinterpret_u32_u8(p0q0));
  p6q6_p2q2 = vtrn_u32(vreinterpret_u32_u8(p6q6), vreinterpret_u32_u8(p2q2));

  pxqx_p3q3.val[1] = vrev64_u32(pxqx_p3q3.val[1]);
  p5q5_p1q1.val[1] = vrev64_u32(p5q5_p1q1.val[1]);
  p4q4_p0q0.val[1] = vrev64_u32(p4q4_p0q0.val[1]);
  p6q6_p2q2.val[1] = vrev64_u32(p6q6_p2q2.val[1]);

  q0q4 = vreinterpret_u8_u32(p4q4_p0q0.val[1]);
  q1q5 = vreinterpret_u8_u32(p5q5_p1q1.val[1]);
  q2q6 = vreinterpret_u8_u32(p6q6_p2q2.val[1]);
  q3qy = vreinterpret_u8_u32(pxqx_p3q3.val[1]);
  transpose_u8_8x4(&q0q4, &q1q5, &q2q6, &q3qy);

  pxp3 = vreinterpret_u8_u32(pxqx_p3q3.val[0]);
  p6p2 = vreinterpret_u8_u32(p6q6_p2q2.val[0]);
  p5p1 = vreinterpret_u8_u32(p5q5_p1q1.val[0]);
  p4p0 = vreinterpret_u8_u32(p4q4_p0q0.val[0]);
  transpose_u8_8x4(&pxp3, &p6p2, &p5p1, &p4p0);

  row0 = vcombine_u8(pxp3, q0q4);
  row1 = vcombine_u8(p6p2, q1q5);
  row2 = vcombine_u8(p5p1, q2q6);
  row3 = vcombine_u8(p4p0, q3qy);

  store_u8_8x16(src - 8, stride, row0, row1, row2, row3);
}

void aom_lpf_vertical_14_dual_neon(
    uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1) {
  aom_lpf_vertical_14_neon(s, pitch, blimit0, limit0, thresh0);
  aom_lpf_vertical_14_neon(s + 4 * pitch, pitch, blimit1, limit1, thresh1);
}

void aom_lpf_vertical_14_quad_neon(uint8_t *s, int pitch, const uint8_t *blimit,
                                   const uint8_t *limit,
                                   const uint8_t *thresh) {
  aom_lpf_vertical_14_dual_neon(s, pitch, blimit, limit, thresh, blimit, limit,
                                thresh);
  aom_lpf_vertical_14_dual_neon(s + 2 * MI_SIZE * pitch, pitch, blimit, limit,
                                thresh, blimit, limit, thresh);
}

void aom_lpf_vertical_8_neon(uint8_t *src, int stride, const uint8_t *blimit,
                             const uint8_t *limit, const uint8_t *thresh) {
  uint32x2x2_t p2q2_p1q1, p3q3_p0q0;
  uint32x2_t pq_rev;
  uint8x8_t p3q0, p2q1, p1q2, p0q3;
  uint8x8_t p0q0, p1q1, p2q2, p3q3;

  // row0: p3 p2 p1 p0 | q0 q1 q2 q3
  // row1: p3 p2 p1 p0 | q0 q1 q2 q3
  // row2: p3 p2 p1 p0 | q0 q1 q2 q3
  // row3: p3 p2 p1 p0 | q0 q1 q2 q3
  load_u8_8x4(src - 4, stride, &p3q0, &p2q1, &p1q2, &p0q3);

  transpose_u8_8x4(&p3q0, &p2q1, &p1q2, &p0q3);

  pq_rev = vrev64_u32(vreinterpret_u32_u8(p0q3));
  p3q3_p0q0 = vtrn_u32(vreinterpret_u32_u8(p3q0), pq_rev);

  pq_rev = vrev64_u32(vreinterpret_u32_u8(p1q2));
  p2q2_p1q1 = vtrn_u32(vreinterpret_u32_u8(p2q1), pq_rev);

  p0q0 = vreinterpret_u8_u32(vrev64_u32(p3q3_p0q0.val[1]));
  p1q1 = vreinterpret_u8_u32(vrev64_u32(p2q2_p1q1.val[1]));
  p2q2 = vreinterpret_u8_u32(p2q2_p1q1.val[0]);
  p3q3 = vreinterpret_u8_u32(p3q3_p0q0.val[0]);

  lpf_8_neon(&p3q3, &p2q2, &p1q1, &p0q0, *blimit, *limit, *thresh);

  pq_rev = vrev64_u32(vreinterpret_u32_u8(p0q0));
  p3q3_p0q0 = vtrn_u32(vreinterpret_u32_u8(p3q3), pq_rev);

  pq_rev = vrev64_u32(vreinterpret_u32_u8(p1q1));
  p2q2_p1q1 = vtrn_u32(vreinterpret_u32_u8(p2q2), pq_rev);

  p0q3 = vreinterpret_u8_u32(vrev64_u32(p3q3_p0q0.val[1]));
  p1q2 = vreinterpret_u8_u32(vrev64_u32(p2q2_p1q1.val[1]));
  p2q1 = vreinterpret_u8_u32(p2q2_p1q1.val[0]);
  p3q0 = vreinterpret_u8_u32(p3q3_p0q0.val[0]);
  transpose_u8_8x4(&p3q0, &p2q1, &p1q2, &p0q3);

  store_u8_8x4(src - 4, stride, p3q0, p2q1, p1q2, p0q3);
}

void aom_lpf_vertical_8_dual_neon(uint8_t *s, int pitch, const uint8_t *blimit0,
                                  const uint8_t *limit0, const uint8_t *thresh0,
                                  const uint8_t *blimit1, const uint8_t *limit1,
                                  const uint8_t *thresh1) {
  aom_lpf_vertical_8_neon(s, pitch, blimit0, limit0, thresh0);
  aom_lpf_vertical_8_neon(s + 4 * pitch, pitch, blimit1, limit1, thresh1);
}

void aom_lpf_vertical_8_quad_neon(uint8_t *s, int pitch, const uint8_t *blimit,
                                  const uint8_t *limit, const uint8_t *thresh) {
  aom_lpf_vertical_8_dual_neon(s, pitch, blimit, limit, thresh, blimit, limit,
                               thresh);
  aom_lpf_vertical_8_dual_neon(s + 2 * MI_SIZE * pitch, pitch, blimit, limit,
                               thresh, blimit, limit, thresh);
}

void aom_lpf_vertical_6_neon(uint8_t *src, int stride, const uint8_t *blimit,
                             const uint8_t *limit, const uint8_t *thresh) {
  uint32x2x2_t p2q2_p1q1, pxqy_p0q0;
  uint32x2_t pq_rev;
  uint8x8_t pxq0, p2q1, p1q2, p0qy;
  uint8x8_t p0q0, p1q1, p2q2, pxqy;

  // row0: px p2 p1 p0 | q0 q1 q2 qy
  // row1: px p2 p1 p0 | q0 q1 q2 qy
  // row2: px p2 p1 p0 | q0 q1 q2 qy
  // row3: px p2 p1 p0 | q0 q1 q2 qy
  load_u8_8x4(src - 4, stride, &pxq0, &p2q1, &p1q2, &p0qy);

  transpose_u8_8x4(&pxq0, &p2q1, &p1q2, &p0qy);

  pq_rev = vrev64_u32(vreinterpret_u32_u8(p0qy));
  pxqy_p0q0 = vtrn_u32(vreinterpret_u32_u8(pxq0), pq_rev);

  pq_rev = vrev64_u32(vreinterpret_u32_u8(p1q2));
  p2q2_p1q1 = vtrn_u32(vreinterpret_u32_u8(p2q1), pq_rev);

  p0q0 = vreinterpret_u8_u32(vrev64_u32(pxqy_p0q0.val[1]));
  p1q1 = vreinterpret_u8_u32(vrev64_u32(p2q2_p1q1.val[1]));
  p2q2 = vreinterpret_u8_u32(p2q2_p1q1.val[0]);
  pxqy = vreinterpret_u8_u32(pxqy_p0q0.val[0]);

  lpf_6_neon(&p2q2, &p1q1, &p0q0, *blimit, *limit, *thresh);

  pq_rev = vrev64_u32(vreinterpret_u32_u8(p0q0));
  pxqy_p0q0 = vtrn_u32(vreinterpret_u32_u8(pxqy), pq_rev);

  pq_rev = vrev64_u32(vreinterpret_u32_u8(p1q1));
  p2q2_p1q1 = vtrn_u32(vreinterpret_u32_u8(p2q2), pq_rev);

  p0qy = vreinterpret_u8_u32(vrev64_u32(pxqy_p0q0.val[1]));
  p1q2 = vreinterpret_u8_u32(vrev64_u32(p2q2_p1q1.val[1]));
  p2q1 = vreinterpret_u8_u32(p2q2_p1q1.val[0]);
  pxq0 = vreinterpret_u8_u32(pxqy_p0q0.val[0]);
  transpose_u8_8x4(&pxq0, &p2q1, &p1q2, &p0qy);

  store_u8_8x4(src - 4, stride, pxq0, p2q1, p1q2, p0qy);
}

void aom_lpf_vertical_6_dual_neon(uint8_t *s, int pitch, const uint8_t *blimit0,
                                  const uint8_t *limit0, const uint8_t *thresh0,
                                  const uint8_t *blimit1, const uint8_t *limit1,
                                  const uint8_t *thresh1) {
  aom_lpf_vertical_6_neon(s, pitch, blimit0, limit0, thresh0);
  aom_lpf_vertical_6_neon(s + 4 * pitch, pitch, blimit1, limit1, thresh1);
}

void aom_lpf_vertical_6_quad_neon(uint8_t *s, int pitch, const uint8_t *blimit,
                                  const uint8_t *limit, const uint8_t *thresh) {
  aom_lpf_vertical_6_dual_neon(s, pitch, blimit, limit, thresh, blimit, limit,
                               thresh);
  aom_lpf_vertical_6_dual_neon(s + 2 * MI_SIZE * pitch, pitch, blimit, limit,
                               thresh, blimit, limit, thresh);
}

void aom_lpf_vertical_4_neon(uint8_t *src, int stride, const uint8_t *blimit,
                             const uint8_t *limit, const uint8_t *thresh) {
  uint32x2x2_t p1q0_p0q1, p1q1_p0q0, p1p0_q1q0;
  uint32x2_t pq_rev;
  uint8x8_t UNINITIALIZED_IS_SAFE(p1p0), UNINITIALIZED_IS_SAFE(q0q1);
  uint8x8_t p0q0, p1q1;

  // row0: p1 p0 | q0 q1
  // row1: p1 p0 | q0 q1
  // row2: p1 p0 | q0 q1
  // row3: p1 p0 | q0 q1
  load_unaligned_u8_4x4(src - 2, stride, &p1p0, &q0q1);

  transpose_u8_4x4(&p1p0, &q0q1);

  p1q0_p0q1 = vtrn_u32(vreinterpret_u32_u8(p1p0), vreinterpret_u32_u8(q0q1));

  pq_rev = vrev64_u32(p1q0_p0q1.val[1]);
  p1q1_p0q0 = vtrn_u32(p1q0_p0q1.val[0], pq_rev);

  p1q1 = vreinterpret_u8_u32(p1q1_p0q0.val[0]);
  p0q0 = vreinterpret_u8_u32(p1q1_p0q0.val[1]);

  lpf_4_neon(&p1q1, &p0q0, *blimit, *limit, *thresh);

  p1p0_q1q0 = vtrn_u32(vreinterpret_u32_u8(p1q1), vreinterpret_u32_u8(p0q0));

  p1p0 = vreinterpret_u8_u32(p1p0_q1q0.val[0]);
  q0q1 = vreinterpret_u8_u32(vrev64_u32(p1p0_q1q0.val[1]));

  transpose_u8_4x4(&p1p0, &q0q1);

  store_unaligned_u8_4x1(src - 2, p1p0, 0);
  store_unaligned_u8_4x1((src - 2) + 1 * stride, q0q1, 0);
  store_unaligned_u8_4x1((src - 2) + 2 * stride, p1p0, 1);
  store_unaligned_u8_4x1((src - 2) + 3 * stride, q0q1, 1);
}

void aom_lpf_vertical_4_dual_neon(uint8_t *s, int pitch, const uint8_t *blimit0,
                                  const uint8_t *limit0, const uint8_t *thresh0,
                                  const uint8_t *blimit1, const uint8_t *limit1,
                                  const uint8_t *thresh1) {
  aom_lpf_vertical_4_neon(s, pitch, blimit0, limit0, thresh0);
  aom_lpf_vertical_4_neon(s + 4 * pitch, pitch, blimit1, limit1, thresh1);
}

void aom_lpf_vertical_4_quad_neon(uint8_t *s, int pitch, const uint8_t *blimit,
                                  const uint8_t *limit, const uint8_t *thresh) {
  aom_lpf_vertical_4_dual_neon(s, pitch, blimit, limit, thresh, blimit, limit,
                               thresh);
  aom_lpf_vertical_4_dual_neon(s + 2 * MI_SIZE * pitch, pitch, blimit, limit,
                               thresh, blimit, limit, thresh);
}

void aom_lpf_horizontal_14_neon(uint8_t *src, int stride, const uint8_t *blimit,
                                const uint8_t *limit, const uint8_t *thresh) {
  uint8x8_t UNINITIALIZED_IS_SAFE(p0q0), UNINITIALIZED_IS_SAFE(p1q1),
      UNINITIALIZED_IS_SAFE(p2q2), UNINITIALIZED_IS_SAFE(p3q3),
      UNINITIALIZED_IS_SAFE(p4q4), UNINITIALIZED_IS_SAFE(p5q5),
      UNINITIALIZED_IS_SAFE(p6q6);

  load_u8_4x1(src - 7 * stride, &p6q6, 0);
  load_u8_4x1(src - 6 * stride, &p5q5, 0);
  load_u8_4x1(src - 5 * stride, &p4q4, 0);
  load_u8_4x1(src - 4 * stride, &p3q3, 0);
  load_u8_4x1(src - 3 * stride, &p2q2, 0);
  load_u8_4x1(src - 2 * stride, &p1q1, 0);
  load_u8_4x1(src - 1 * stride, &p0q0, 0);
  load_u8_4x1(src + 0 * stride, &p0q0, 1);
  load_u8_4x1(src + 1 * stride, &p1q1, 1);
  load_u8_4x1(src + 2 * stride, &p2q2, 1);
  load_u8_4x1(src + 3 * stride, &p3q3, 1);
  load_u8_4x1(src + 4 * stride, &p4q4, 1);
  load_u8_4x1(src + 5 * stride, &p5q5, 1);
  load_u8_4x1(src + 6 * stride, &p6q6, 1);

  lpf_14_neon(&p6q6, &p5q5, &p4q4, &p3q3, &p2q2, &p1q1, &p0q0, *blimit, *limit,
              *thresh);

  store_u8_4x1(src - 6 * stride, p5q5, 0);
  store_u8_4x1(src - 5 * stride, p4q4, 0);
  store_u8_4x1(src - 4 * stride, p3q3, 0);
  store_u8_4x1(src - 3 * stride, p2q2, 0);
  store_u8_4x1(src - 2 * stride, p1q1, 0);
  store_u8_4x1(src - 1 * stride, p0q0, 0);
  store_u8_4x1(src + 0 * stride, p0q0, 1);
  store_u8_4x1(src + 1 * stride, p1q1, 1);
  store_u8_4x1(src + 2 * stride, p2q2, 1);
  store_u8_4x1(src + 3 * stride, p3q3, 1);
  store_u8_4x1(src + 4 * stride, p4q4, 1);
  store_u8_4x1(src + 5 * stride, p5q5, 1);
}

void aom_lpf_horizontal_14_dual_neon(
    uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1) {
  aom_lpf_horizontal_14_neon(s, pitch, blimit0, limit0, thresh0);
  aom_lpf_horizontal_14_neon(s + 4, pitch, blimit1, limit1, thresh1);
}

// TODO(any): Rewrite in NEON (similar to quad SSE2 functions) for better speed
// up.
void aom_lpf_horizontal_14_quad_neon(uint8_t *s, int pitch,
                                     const uint8_t *blimit,
                                     const uint8_t *limit,
                                     const uint8_t *thresh) {
  aom_lpf_horizontal_14_dual_neon(s, pitch, blimit, limit, thresh, blimit,
                                  limit, thresh);
  aom_lpf_horizontal_14_dual_neon(s + 2 * MI_SIZE, pitch, blimit, limit, thresh,
                                  blimit, limit, thresh);
}

void aom_lpf_horizontal_8_neon(uint8_t *src, int stride, const uint8_t *blimit,
                               const uint8_t *limit, const uint8_t *thresh) {
  uint8x8_t p0q0, p1q1, p2q2, p3q3;

  p3q3 = vreinterpret_u8_u32(vld1_dup_u32((uint32_t *)(src - 4 * stride)));
  p2q2 = vreinterpret_u8_u32(vld1_dup_u32((uint32_t *)(src - 3 * stride)));
  p1q1 = vreinterpret_u8_u32(vld1_dup_u32((uint32_t *)(src - 2 * stride)));
  p0q0 = vreinterpret_u8_u32(vld1_dup_u32((uint32_t *)(src - 1 * stride)));
  p0q0 = vreinterpret_u8_u32(vld1_lane_u32((uint32_t *)(src + 0 * stride),
                                           vreinterpret_u32_u8(p0q0), 1));
  p1q1 = vreinterpret_u8_u32(vld1_lane_u32((uint32_t *)(src + 1 * stride),
                                           vreinterpret_u32_u8(p1q1), 1));
  p2q2 = vreinterpret_u8_u32(vld1_lane_u32((uint32_t *)(src + 2 * stride),
                                           vreinterpret_u32_u8(p2q2), 1));
  p3q3 = vreinterpret_u8_u32(vld1_lane_u32((uint32_t *)(src + 3 * stride),
                                           vreinterpret_u32_u8(p3q3), 1));

  lpf_8_neon(&p3q3, &p2q2, &p1q1, &p0q0, *blimit, *limit, *thresh);

  vst1_lane_u32((uint32_t *)(src - 4 * stride), vreinterpret_u32_u8(p3q3), 0);
  vst1_lane_u32((uint32_t *)(src - 3 * stride), vreinterpret_u32_u8(p2q2), 0);
  vst1_lane_u32((uint32_t *)(src - 2 * stride), vreinterpret_u32_u8(p1q1), 0);
  vst1_lane_u32((uint32_t *)(src - 1 * stride), vreinterpret_u32_u8(p0q0), 0);
  vst1_lane_u32((uint32_t *)(src + 0 * stride), vreinterpret_u32_u8(p0q0), 1);
  vst1_lane_u32((uint32_t *)(src + 1 * stride), vreinterpret_u32_u8(p1q1), 1);
  vst1_lane_u32((uint32_t *)(src + 2 * stride), vreinterpret_u32_u8(p2q2), 1);
  vst1_lane_u32((uint32_t *)(src + 3 * stride), vreinterpret_u32_u8(p3q3), 1);
}

void aom_lpf_horizontal_8_dual_neon(
    uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1) {
  aom_lpf_horizontal_8_neon(s, pitch, blimit0, limit0, thresh0);
  aom_lpf_horizontal_8_neon(s + 4, pitch, blimit1, limit1, thresh1);
}

// TODO(any): Rewrite in NEON (similar to quad SSE2 functions) for better speed
// up.
void aom_lpf_horizontal_8_quad_neon(uint8_t *s, int pitch,
                                    const uint8_t *blimit, const uint8_t *limit,
                                    const uint8_t *thresh) {
  aom_lpf_horizontal_8_dual_neon(s, pitch, blimit, limit, thresh, blimit, limit,
                                 thresh);
  aom_lpf_horizontal_8_dual_neon(s + 2 * MI_SIZE, pitch, blimit, limit, thresh,
                                 blimit, limit, thresh);
}

void aom_lpf_horizontal_6_neon(uint8_t *src, int stride, const uint8_t *blimit,
                               const uint8_t *limit, const uint8_t *thresh) {
  uint8x8_t p0q0, p1q1, p2q2;

  p2q2 = vreinterpret_u8_u32(vld1_dup_u32((uint32_t *)(src - 3 * stride)));
  p1q1 = vreinterpret_u8_u32(vld1_dup_u32((uint32_t *)(src - 2 * stride)));
  p0q0 = vreinterpret_u8_u32(vld1_dup_u32((uint32_t *)(src - 1 * stride)));
  p0q0 = vreinterpret_u8_u32(vld1_lane_u32((uint32_t *)(src + 0 * stride),
                                           vreinterpret_u32_u8(p0q0), 1));
  p1q1 = vreinterpret_u8_u32(vld1_lane_u32((uint32_t *)(src + 1 * stride),
                                           vreinterpret_u32_u8(p1q1), 1));
  p2q2 = vreinterpret_u8_u32(vld1_lane_u32((uint32_t *)(src + 2 * stride),
                                           vreinterpret_u32_u8(p2q2), 1));

  lpf_6_neon(&p2q2, &p1q1, &p0q0, *blimit, *limit, *thresh);

  vst1_lane_u32((uint32_t *)(src - 3 * stride), vreinterpret_u32_u8(p2q2), 0);
  vst1_lane_u32((uint32_t *)(src - 2 * stride), vreinterpret_u32_u8(p1q1), 0);
  vst1_lane_u32((uint32_t *)(src - 1 * stride), vreinterpret_u32_u8(p0q0), 0);
  vst1_lane_u32((uint32_t *)(src + 0 * stride), vreinterpret_u32_u8(p0q0), 1);
  vst1_lane_u32((uint32_t *)(src + 1 * stride), vreinterpret_u32_u8(p1q1), 1);
  vst1_lane_u32((uint32_t *)(src + 2 * stride), vreinterpret_u32_u8(p2q2), 1);
}

void aom_lpf_horizontal_6_dual_neon(
    uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1) {
  aom_lpf_horizontal_6_neon(s, pitch, blimit0, limit0, thresh0);
  aom_lpf_horizontal_6_neon(s + 4, pitch, blimit1, limit1, thresh1);
}

// TODO(any): Rewrite in NEON (similar to quad SSE2 functions) for better speed
// up.
void aom_lpf_horizontal_6_quad_neon(uint8_t *s, int pitch,
                                    const uint8_t *blimit, const uint8_t *limit,
                                    const uint8_t *thresh) {
  aom_lpf_horizontal_6_dual_neon(s, pitch, blimit, limit, thresh, blimit, limit,
                                 thresh);
  aom_lpf_horizontal_6_dual_neon(s + 2 * MI_SIZE, pitch, blimit, limit, thresh,
                                 blimit, limit, thresh);
}

void aom_lpf_horizontal_4_neon(uint8_t *src, int stride, const uint8_t *blimit,
                               const uint8_t *limit, const uint8_t *thresh) {
  uint8x8_t UNINITIALIZED_IS_SAFE(p0q0), UNINITIALIZED_IS_SAFE(p1q1);

  load_u8_4x1(src - 2 * stride, &p1q1, 0);
  load_u8_4x1(src - 1 * stride, &p0q0, 0);
  load_u8_4x1(src + 0 * stride, &p0q0, 1);
  load_u8_4x1(src + 1 * stride, &p1q1, 1);

  lpf_4_neon(&p1q1, &p0q0, *blimit, *limit, *thresh);

  store_u8_4x1(src - 2 * stride, p1q1, 0);
  store_u8_4x1(src - 1 * stride, p0q0, 0);
  store_u8_4x1(src + 0 * stride, p0q0, 1);
  store_u8_4x1(src + 1 * stride, p1q1, 1);
}

void aom_lpf_horizontal_4_dual_neon(
    uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1) {
  aom_lpf_horizontal_4_neon(s, pitch, blimit0, limit0, thresh0);
  aom_lpf_horizontal_4_neon(s + 4, pitch, blimit1, limit1, thresh1);
}

// TODO(any): Rewrite in NEON (similar to quad SSE2 functions) for better speed
// up.
void aom_lpf_horizontal_4_quad_neon(uint8_t *s, int pitch,
                                    const uint8_t *blimit, const uint8_t *limit,
                                    const uint8_t *thresh) {
  aom_lpf_horizontal_4_dual_neon(s, pitch, blimit, limit, thresh, blimit, limit,
                                 thresh);
  aom_lpf_horizontal_4_dual_neon(s + 2 * MI_SIZE, pitch, blimit, limit, thresh,
                                 blimit, limit, thresh);
}
