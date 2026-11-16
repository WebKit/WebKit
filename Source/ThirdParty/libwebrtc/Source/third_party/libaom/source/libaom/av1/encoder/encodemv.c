/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <math.h>

#include "av1/common/common.h"
#include "av1/common/entropymode.h"

#include "av1/encoder/cost.h"
#include "av1/encoder/encodemv.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/bitops.h"

static void update_mv_component_stats(int comp, nmv_component *mvcomp,
                                      MvSubpelPrecision precision) {
  assert(comp != 0);
  int offset;
  const int sign = comp < 0;
  const int mag = sign ? -comp : comp;
  const int mv_class = av1_get_mv_class(mag - 1, &offset);
  const int d = offset >> 3;         // int mv data
  const int fr = (offset >> 1) & 3;  // fractional mv data
  const int hp = offset & 1;         // high precision mv data

  // Sign
  update_cdf(mvcomp->sign_cdf, sign, 2);

  // Class
  update_cdf(mvcomp->classes_cdf, mv_class, MV_CLASSES);

  // Integer bits
  if (mv_class == MV_CLASS_0) {
    update_cdf(mvcomp->class0_cdf, d, CLASS0_SIZE);
  } else {
    const int n = mv_class + CLASS0_BITS - 1;  // number of bits
    for (int i = 0; i < n; ++i)
      update_cdf(mvcomp->bits_cdf[i], (d >> i) & 1, 2);
  }
  // Fractional bits
  if (precision > MV_SUBPEL_NONE) {
    aom_cdf_prob *fp_cdf =
        mv_class == MV_CLASS_0 ? mvcomp->class0_fp_cdf[d] : mvcomp->fp_cdf;
    update_cdf(fp_cdf, fr, MV_FP_SIZE);
  }

  // High precision bit
  if (precision > MV_SUBPEL_LOW_PRECISION) {
    aom_cdf_prob *hp_cdf =
        mv_class == MV_CLASS_0 ? mvcomp->class0_hp_cdf : mvcomp->hp_cdf;
    update_cdf(hp_cdf, hp, 2);
  }
}

void av1_update_mv_stats(const MV *mv, const MV *ref, nmv_context *mvctx,
                         MvSubpelPrecision precision) {
  const MV diff = { mv->row - ref->row, mv->col - ref->col };
  const MV_JOINT_TYPE j = av1_get_mv_joint(&diff);

  update_cdf(mvctx->joints_cdf, j, MV_JOINTS);

  if (mv_joint_vertical(j))
    update_mv_component_stats(diff.row, &mvctx->comps[0], precision);

  if (mv_joint_horizontal(j))
    update_mv_component_stats(diff.col, &mvctx->comps[1], precision);
}

static void encode_mv_component(aom_writer *w, int comp, nmv_component *mvcomp,
                                MvSubpelPrecision precision) {
  assert(comp != 0);
  int offset;
  const int sign = comp < 0;
  const int mag = sign ? -comp : comp;
  const int mv_class = av1_get_mv_class(mag - 1, &offset);
  const int d = offset >> 3;         // int mv data
  const int fr = (offset >> 1) & 3;  // fractional mv data
  const int hp = offset & 1;         // high precision mv data

  // Sign
  aom_write_symbol(w, sign, mvcomp->sign_cdf, 2);

  // Class
  aom_write_symbol(w, mv_class, mvcomp->classes_cdf, MV_CLASSES);

  // Integer bits
  if (mv_class == MV_CLASS_0) {
    aom_write_symbol(w, d, mvcomp->class0_cdf, CLASS0_SIZE);
  } else {
    int i;
    const int n = mv_class + CLASS0_BITS - 1;  // number of bits
    for (i = 0; i < n; ++i)
      aom_write_symbol(w, (d >> i) & 1, mvcomp->bits_cdf[i], 2);
  }
  // Fractional bits
  if (precision > MV_SUBPEL_NONE) {
    aom_write_symbol(
        w, fr,
        mv_class == MV_CLASS_0 ? mvcomp->class0_fp_cdf[d] : mvcomp->fp_cdf,
        MV_FP_SIZE);
  }

  // High precision bit
  if (precision > MV_SUBPEL_LOW_PRECISION)
    aom_write_symbol(
        w, hp, mv_class == MV_CLASS_0 ? mvcomp->class0_hp_cdf : mvcomp->hp_cdf,
        2);
}

/* TODO(siekyleb@amazon.com): This function writes MV_VALS ints or 128 KiB. This
 *   is more than most L1D caches and is a significant chunk of L2. Write
 *   SIMD that uses streaming writes to avoid loading all of that into L1, or
 *   just don't update the larger component costs every time this called
 *   (or both).
 */
void av1_build_nmv_component_cost_table(int *mvcost,
                                        const nmv_component *const mvcomp,
                                        MvSubpelPrecision precision) {
  int i, j, v, o, mantissa;
  int sign_cost[2], class_cost[MV_CLASSES], class0_cost[CLASS0_SIZE];
  int bits_cost[MV_OFFSET_BITS][2];
  int class0_fp_cost[CLASS0_SIZE][MV_FP_SIZE] = { 0 },
      fp_cost[MV_FP_SIZE] = { 0 };
  int class0_hp_cost[2] = { 0 }, hp_cost[2] = { 0 };

  av1_cost_tokens_from_cdf(sign_cost, mvcomp->sign_cdf, NULL);
  av1_cost_tokens_from_cdf(class_cost, mvcomp->classes_cdf, NULL);
  av1_cost_tokens_from_cdf(class0_cost, mvcomp->class0_cdf, NULL);
  for (i = 0; i < MV_OFFSET_BITS; ++i) {
    av1_cost_tokens_from_cdf(bits_cost[i], mvcomp->bits_cdf[i], NULL);
  }

  if (precision > MV_SUBPEL_NONE) {
    for (i = 0; i < CLASS0_SIZE; ++i)
      av1_cost_tokens_from_cdf(class0_fp_cost[i], mvcomp->class0_fp_cdf[i],
                               NULL);
    av1_cost_tokens_from_cdf(fp_cost, mvcomp->fp_cdf, NULL);
  }

  if (precision > MV_SUBPEL_LOW_PRECISION) {
    av1_cost_tokens_from_cdf(class0_hp_cost, mvcomp->class0_hp_cdf, NULL);
    av1_cost_tokens_from_cdf(hp_cost, mvcomp->hp_cdf, NULL);
  }

  // Instead of accumulating the cost of each vector component's bits
  //   individually, compute the costs based on smaller vectors. Costs for
  //   [2^exp, 2 * 2^exp - 1] are calculated based on [0, 2^exp - 1]
  //   respectively. Offsets are maintained to swap both 1) class costs when
  //   treated as a complete vector component with the highest set bit when
  //   treated as a mantissa (significand) and 2) leading zeros to account for
  //   the current exponent.

  // Cost offsets
  int cost_swap[MV_OFFSET_BITS] = { 0 };
  // Delta to convert positive vector to negative vector costs
  int negate_sign = sign_cost[1] - sign_cost[0];

  // Initialize with offsets to swap the class costs with the costs of the
  //   highest set bit.
  for (i = 1; i < MV_OFFSET_BITS; ++i) {
    cost_swap[i] = bits_cost[i - 1][1];
    if (i > CLASS0_BITS) cost_swap[i] -= class_cost[i - CLASS0_BITS];
  }

  // Seed the fractional costs onto the output (overwritten latter).
  for (o = 0; o < MV_FP_SIZE; ++o) {
    int hp;
    for (hp = 0; hp < 2; ++hp) {
      v = 2 * o + hp + 1;
      mvcost[v] = fp_cost[o] + hp_cost[hp] + sign_cost[0];
    }
  }

  mvcost[0] = 0;
  // Fill the costs for each exponent's vectors, using the costs set in the
  //   previous exponents.
  for (i = 0; i < MV_OFFSET_BITS; ++i) {
    const int exponent = (2 * MV_FP_SIZE) << i;

    int class = 0;
    if (i >= CLASS0_BITS) {
      class = class_cost[i - CLASS0_BITS + 1];
    }

    // Iterate through mantissas, keeping track of the location
    //   of the highest set bit for the mantissa.
    // To be clear: in the outer loop, the position of the highest set bit
    //   (exponent) is tracked and, in this loop, the highest set bit of the
    //   mantissa is tracked.
    mantissa = 0;
    for (j = 0; j <= i; ++j) {
      for (; mantissa < (2 * MV_FP_SIZE) << j; ++mantissa) {
        int cost = mvcost[mantissa + 1] + class + cost_swap[j];
        v = exponent + mantissa + 1;
        mvcost[v] = cost;
        mvcost[-v] = cost + negate_sign;
      }
      cost_swap[j] += bits_cost[i][0];
    }
  }

  // Special case to avoid buffer overrun
  {
    int exponent = (2 * MV_FP_SIZE) << MV_OFFSET_BITS;
    int class = class_cost[MV_CLASSES - 1];
    mantissa = 0;
    for (j = 0; j < MV_OFFSET_BITS; ++j) {
      for (; mantissa < (2 * MV_FP_SIZE) << j; ++mantissa) {
        int cost = mvcost[mantissa + 1] + class + cost_swap[j];
        v = exponent + mantissa + 1;
        mvcost[v] = cost;
        mvcost[-v] = cost + negate_sign;
      }
    }
    // At this point: mantissa = exponent >> 1

    // Manually calculate the final cost offset
    int cost_swap_hi =
        bits_cost[MV_OFFSET_BITS - 1][1] - class_cost[MV_CLASSES - 2];
    for (; mantissa < exponent - 1; ++mantissa) {
      int cost = mvcost[mantissa + 1] + class + cost_swap_hi;
      v = exponent + mantissa + 1;
      mvcost[v] = cost;
      mvcost[-v] = cost + negate_sign;
    }
  }

  // Fill costs for class0 vectors, overwriting previous placeholder values
  //   used for calculating the costs of the larger vectors.
  for (i = 0; i < CLASS0_SIZE; ++i) {
    const int top = i * 2 * MV_FP_SIZE;
    for (o = 0; o < MV_FP_SIZE; ++o) {
      int hp;
      int cost = class0_fp_cost[i][o] + class_cost[0] + class0_cost[i];
      for (hp = 0; hp < 2; ++hp) {
        v = top + 2 * o + hp + 1;
        mvcost[v] = cost + class0_hp_cost[hp] + sign_cost[0];
        mvcost[-v] = cost + class0_hp_cost[hp] + sign_cost[1];
      }
    }
  }
}

void av1_encode_mv(AV1_COMP *cpi, aom_writer *w, ThreadData *td, const MV *mv,
                   const MV *ref, nmv_context *mvctx, int usehp) {
  const MV diff = { mv->row - ref->row, mv->col - ref->col };
  const MV_JOINT_TYPE j = av1_get_mv_joint(&diff);
  // If the mv_diff is zero, then we should have used near or nearest instead.
  assert(j != MV_JOINT_ZERO);
  if (cpi->common.features.cur_frame_force_integer_mv) {
    usehp = MV_SUBPEL_NONE;
  }
  aom_write_symbol(w, j, mvctx->joints_cdf, MV_JOINTS);
  if (mv_joint_vertical(j))
    encode_mv_component(w, diff.row, &mvctx->comps[0], usehp);

  if (mv_joint_horizontal(j))
    encode_mv_component(w, diff.col, &mvctx->comps[1], usehp);

  // If auto_mv_step_size is enabled then keep track of the largest
  // motion vector component used.
  if (cpi->sf.mv_sf.auto_mv_step_size) {
    int maxv = AOMMAX(abs(mv->row), abs(mv->col)) >> 3;
    td->max_mv_magnitude = AOMMAX(maxv, td->max_mv_magnitude);
  }
}

void av1_encode_dv(aom_writer *w, const MV *mv, const MV *ref,
                   nmv_context *mvctx) {
  // DV and ref DV should not have sub-pel.
  assert((mv->col & 7) == 0);
  assert((mv->row & 7) == 0);
  assert((ref->col & 7) == 0);
  assert((ref->row & 7) == 0);
  const MV diff = { mv->row - ref->row, mv->col - ref->col };
  const MV_JOINT_TYPE j = av1_get_mv_joint(&diff);

  aom_write_symbol(w, j, mvctx->joints_cdf, MV_JOINTS);
  if (mv_joint_vertical(j))
    encode_mv_component(w, diff.row, &mvctx->comps[0], MV_SUBPEL_NONE);

  if (mv_joint_horizontal(j))
    encode_mv_component(w, diff.col, &mvctx->comps[1], MV_SUBPEL_NONE);
}

void av1_build_nmv_cost_table(int *mvjoint, int *mvcost[2],
                              const nmv_context *ctx,
                              MvSubpelPrecision precision) {
  av1_cost_tokens_from_cdf(mvjoint, ctx->joints_cdf, NULL);
  av1_build_nmv_component_cost_table(mvcost[0], &ctx->comps[0], precision);
  av1_build_nmv_component_cost_table(mvcost[1], &ctx->comps[1], precision);
}

int_mv av1_get_ref_mv_from_stack(int ref_idx,
                                 const MV_REFERENCE_FRAME *ref_frame,
                                 int ref_mv_idx,
                                 const MB_MODE_INFO_EXT *mbmi_ext) {
  const int8_t ref_frame_type = av1_ref_frame_type(ref_frame);
  const CANDIDATE_MV *curr_ref_mv_stack =
      mbmi_ext->ref_mv_stack[ref_frame_type];

  if (ref_frame[1] > INTRA_FRAME) {
    assert(ref_idx == 0 || ref_idx == 1);
    return ref_idx ? curr_ref_mv_stack[ref_mv_idx].comp_mv
                   : curr_ref_mv_stack[ref_mv_idx].this_mv;
  }

  assert(ref_idx == 0);
  return ref_mv_idx < mbmi_ext->ref_mv_count[ref_frame_type]
             ? curr_ref_mv_stack[ref_mv_idx].this_mv
             : mbmi_ext->global_mvs[ref_frame_type];
}

int_mv av1_get_ref_mv(const MACROBLOCK *x, int ref_idx) {
  const MACROBLOCKD *xd = &x->e_mbd;
  const MB_MODE_INFO *mbmi = xd->mi[0];
  int ref_mv_idx = mbmi->ref_mv_idx;
  if (mbmi->mode == NEAR_NEWMV || mbmi->mode == NEW_NEARMV) {
    assert(has_second_ref(mbmi));
    ref_mv_idx += 1;
  }
  return av1_get_ref_mv_from_stack(ref_idx, mbmi->ref_frame, ref_mv_idx,
                                   &x->mbmi_ext);
}

void av1_find_best_ref_mvs_from_stack(int allow_hp,
                                      const MB_MODE_INFO_EXT *mbmi_ext,
                                      MV_REFERENCE_FRAME ref_frame,
                                      int_mv *nearest_mv, int_mv *near_mv,
                                      int is_integer) {
  const int ref_idx = 0;
  MV_REFERENCE_FRAME ref_frames[2] = { ref_frame, NONE_FRAME };
  *nearest_mv = av1_get_ref_mv_from_stack(ref_idx, ref_frames, 0, mbmi_ext);
  lower_mv_precision(&nearest_mv->as_mv, allow_hp, is_integer);
  *near_mv = av1_get_ref_mv_from_stack(ref_idx, ref_frames, 1, mbmi_ext);
  lower_mv_precision(&near_mv->as_mv, allow_hp, is_integer);
}
