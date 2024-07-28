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

#include "av1/common/common.h"
#include "av1/common/pred_common.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#include "av1/common/seg_common.h"

// Returns a context number for the given MB prediction signal
static InterpFilter get_ref_filter_type(const MB_MODE_INFO *ref_mbmi,
                                        const MACROBLOCKD *xd, int dir,
                                        MV_REFERENCE_FRAME ref_frame) {
  (void)xd;

  return ((ref_mbmi->ref_frame[0] == ref_frame ||
           ref_mbmi->ref_frame[1] == ref_frame)
              ? av1_extract_interp_filter(ref_mbmi->interp_filters, dir & 0x01)
              : SWITCHABLE_FILTERS);
}

int av1_get_pred_context_switchable_interp(const MACROBLOCKD *xd, int dir) {
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const int ctx_offset =
      (mbmi->ref_frame[1] > INTRA_FRAME) * INTER_FILTER_COMP_OFFSET;
  assert(dir == 0 || dir == 1);
  const MV_REFERENCE_FRAME ref_frame = mbmi->ref_frame[0];
  // Note:
  // The mode info data structure has a one element border above and to the
  // left of the entries corresponding to real macroblocks.
  // The prediction flags in these dummy entries are initialized to 0.
  int filter_type_ctx = ctx_offset + (dir & 0x01) * INTER_FILTER_DIR_OFFSET;
  int left_type = SWITCHABLE_FILTERS;
  int above_type = SWITCHABLE_FILTERS;

  if (xd->left_available)
    left_type = get_ref_filter_type(xd->mi[-1], xd, dir, ref_frame);

  if (xd->up_available)
    above_type =
        get_ref_filter_type(xd->mi[-xd->mi_stride], xd, dir, ref_frame);

  if (left_type == above_type) {
    filter_type_ctx += left_type;
  } else if (left_type == SWITCHABLE_FILTERS) {
    assert(above_type != SWITCHABLE_FILTERS);
    filter_type_ctx += above_type;
  } else if (above_type == SWITCHABLE_FILTERS) {
    assert(left_type != SWITCHABLE_FILTERS);
    filter_type_ctx += left_type;
  } else {
    filter_type_ctx += SWITCHABLE_FILTERS;
  }

  return filter_type_ctx;
}

static void palette_add_to_cache(uint16_t *cache, int *n, uint16_t val) {
  // Do not add an already existing value
  if (*n > 0 && val == cache[*n - 1]) return;

  cache[(*n)++] = val;
}

int av1_get_palette_cache(const MACROBLOCKD *const xd, int plane,
                          uint16_t *cache) {
  const int row = -xd->mb_to_top_edge >> 3;
  // Do not refer to above SB row when on SB boundary.
  const MB_MODE_INFO *const above_mi =
      (row % (1 << MIN_SB_SIZE_LOG2)) ? xd->above_mbmi : NULL;
  const MB_MODE_INFO *const left_mi = xd->left_mbmi;
  int above_n = 0, left_n = 0;
  if (above_mi) above_n = above_mi->palette_mode_info.palette_size[plane != 0];
  if (left_mi) left_n = left_mi->palette_mode_info.palette_size[plane != 0];
  if (above_n == 0 && left_n == 0) return 0;
  int above_idx = plane * PALETTE_MAX_SIZE;
  int left_idx = plane * PALETTE_MAX_SIZE;
  int n = 0;
  const uint16_t *above_colors =
      above_mi ? above_mi->palette_mode_info.palette_colors : NULL;
  const uint16_t *left_colors =
      left_mi ? left_mi->palette_mode_info.palette_colors : NULL;
  // Merge the sorted lists of base colors from above and left to get
  // combined sorted color cache.
  while (above_n > 0 && left_n > 0) {
    uint16_t v_above = above_colors[above_idx];
    uint16_t v_left = left_colors[left_idx];
    if (v_left < v_above) {
      palette_add_to_cache(cache, &n, v_left);
      ++left_idx, --left_n;
    } else {
      palette_add_to_cache(cache, &n, v_above);
      ++above_idx, --above_n;
      if (v_left == v_above) ++left_idx, --left_n;
    }
  }
  while (above_n-- > 0) {
    uint16_t val = above_colors[above_idx++];
    palette_add_to_cache(cache, &n, val);
  }
  while (left_n-- > 0) {
    uint16_t val = left_colors[left_idx++];
    palette_add_to_cache(cache, &n, val);
  }
  assert(n <= 2 * PALETTE_MAX_SIZE);
  return n;
}

// The mode info data structure has a one element border above and to the
// left of the entries corresponding to real macroblocks.
// The prediction flags in these dummy entries are initialized to 0.
// 0 - inter/inter, inter/--, --/inter, --/--
// 1 - intra/inter, inter/intra
// 2 - intra/--, --/intra
// 3 - intra/intra
int av1_get_intra_inter_context(const MACROBLOCKD *xd) {
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int has_above = xd->up_available;
  const int has_left = xd->left_available;

  if (has_above && has_left) {  // both edges available
    const int above_intra = !is_inter_block(above_mbmi);
    const int left_intra = !is_inter_block(left_mbmi);
    return left_intra && above_intra ? 3 : left_intra || above_intra;
  } else if (has_above || has_left) {  // one edge available
    return 2 * !is_inter_block(has_above ? above_mbmi : left_mbmi);
  } else {
    return 0;
  }
}

#define CHECK_BACKWARD_REFS(ref_frame) \
  (((ref_frame) >= BWDREF_FRAME) && ((ref_frame) <= ALTREF_FRAME))
#define IS_BACKWARD_REF_FRAME(ref_frame) CHECK_BACKWARD_REFS(ref_frame)

int av1_get_reference_mode_context(const MACROBLOCKD *xd) {
  int ctx;
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int has_above = xd->up_available;
  const int has_left = xd->left_available;

  // Note:
  // The mode info data structure has a one element border above and to the
  // left of the entries corresponding to real macroblocks.
  // The prediction flags in these dummy entries are initialized to 0.
  if (has_above && has_left) {  // both edges available
    if (!has_second_ref(above_mbmi) && !has_second_ref(left_mbmi))
      // neither edge uses comp pred (0/1)
      ctx = IS_BACKWARD_REF_FRAME(above_mbmi->ref_frame[0]) ^
            IS_BACKWARD_REF_FRAME(left_mbmi->ref_frame[0]);
    else if (!has_second_ref(above_mbmi))
      // one of two edges uses comp pred (2/3)
      ctx = 2 + (IS_BACKWARD_REF_FRAME(above_mbmi->ref_frame[0]) ||
                 !is_inter_block(above_mbmi));
    else if (!has_second_ref(left_mbmi))
      // one of two edges uses comp pred (2/3)
      ctx = 2 + (IS_BACKWARD_REF_FRAME(left_mbmi->ref_frame[0]) ||
                 !is_inter_block(left_mbmi));
    else  // both edges use comp pred (4)
      ctx = 4;
  } else if (has_above || has_left) {  // one edge available
    const MB_MODE_INFO *edge_mbmi = has_above ? above_mbmi : left_mbmi;

    if (!has_second_ref(edge_mbmi))
      // edge does not use comp pred (0/1)
      ctx = IS_BACKWARD_REF_FRAME(edge_mbmi->ref_frame[0]);
    else
      // edge uses comp pred (3)
      ctx = 3;
  } else {  // no edges available (1)
    ctx = 1;
  }
  assert(ctx >= 0 && ctx < COMP_INTER_CONTEXTS);
  return ctx;
}

int av1_get_comp_reference_type_context(const MACROBLOCKD *xd) {
  int pred_context;
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int above_in_image = xd->up_available;
  const int left_in_image = xd->left_available;

  if (above_in_image && left_in_image) {  // both edges available
    const int above_intra = !is_inter_block(above_mbmi);
    const int left_intra = !is_inter_block(left_mbmi);

    if (above_intra && left_intra) {  // intra/intra
      pred_context = 2;
    } else if (above_intra || left_intra) {  // intra/inter
      const MB_MODE_INFO *inter_mbmi = above_intra ? left_mbmi : above_mbmi;

      if (!has_second_ref(inter_mbmi))  // single pred
        pred_context = 2;
      else  // comp pred
        pred_context = 1 + 2 * has_uni_comp_refs(inter_mbmi);
    } else {  // inter/inter
      const int a_sg = !has_second_ref(above_mbmi);
      const int l_sg = !has_second_ref(left_mbmi);
      const MV_REFERENCE_FRAME frfa = above_mbmi->ref_frame[0];
      const MV_REFERENCE_FRAME frfl = left_mbmi->ref_frame[0];

      if (a_sg && l_sg) {  // single/single
        pred_context = 1 + 2 * (!(IS_BACKWARD_REF_FRAME(frfa) ^
                                  IS_BACKWARD_REF_FRAME(frfl)));
      } else if (l_sg || a_sg) {  // single/comp
        const int uni_rfc =
            a_sg ? has_uni_comp_refs(left_mbmi) : has_uni_comp_refs(above_mbmi);

        if (!uni_rfc)  // comp bidir
          pred_context = 1;
        else  // comp unidir
          pred_context = 3 + (!(IS_BACKWARD_REF_FRAME(frfa) ^
                                IS_BACKWARD_REF_FRAME(frfl)));
      } else {  // comp/comp
        const int a_uni_rfc = has_uni_comp_refs(above_mbmi);
        const int l_uni_rfc = has_uni_comp_refs(left_mbmi);

        if (!a_uni_rfc && !l_uni_rfc)  // bidir/bidir
          pred_context = 0;
        else if (!a_uni_rfc || !l_uni_rfc)  // unidir/bidir
          pred_context = 2;
        else  // unidir/unidir
          pred_context =
              3 + (!((frfa == BWDREF_FRAME) ^ (frfl == BWDREF_FRAME)));
      }
    }
  } else if (above_in_image || left_in_image) {  // one edge available
    const MB_MODE_INFO *edge_mbmi = above_in_image ? above_mbmi : left_mbmi;

    if (!is_inter_block(edge_mbmi)) {  // intra
      pred_context = 2;
    } else {                           // inter
      if (!has_second_ref(edge_mbmi))  // single pred
        pred_context = 2;
      else  // comp pred
        pred_context = 4 * has_uni_comp_refs(edge_mbmi);
    }
  } else {  // no edges available
    pred_context = 2;
  }

  assert(pred_context >= 0 && pred_context < COMP_REF_TYPE_CONTEXTS);
  return pred_context;
}

// Returns a context number for the given MB prediction signal
//
// Signal the uni-directional compound reference frame pair as either
// (BWDREF, ALTREF), or (LAST, LAST2) / (LAST, LAST3) / (LAST, GOLDEN),
// conditioning on the pair is known as uni-directional.
//
// 3 contexts: Voting is used to compare the count of forward references with
//             that of backward references from the spatial neighbors.
int av1_get_pred_context_uni_comp_ref_p(const MACROBLOCKD *xd) {
  const uint8_t *const ref_counts = &xd->neighbors_ref_counts[0];

  // Count of forward references (L, L2, L3, or G)
  const int frf_count = ref_counts[LAST_FRAME] + ref_counts[LAST2_FRAME] +
                        ref_counts[LAST3_FRAME] + ref_counts[GOLDEN_FRAME];
  // Count of backward references (B or A)
  const int brf_count = ref_counts[BWDREF_FRAME] + ref_counts[ALTREF2_FRAME] +
                        ref_counts[ALTREF_FRAME];

  const int pred_context =
      (frf_count == brf_count) ? 1 : ((frf_count < brf_count) ? 0 : 2);

  assert(pred_context >= 0 && pred_context < UNI_COMP_REF_CONTEXTS);
  return pred_context;
}

// Returns a context number for the given MB prediction signal
//
// Signal the uni-directional compound reference frame pair as
// either (LAST, LAST2), or (LAST, LAST3) / (LAST, GOLDEN),
// conditioning on the pair is known as one of the above three.
//
// 3 contexts: Voting is used to compare the count of LAST2_FRAME with the
//             total count of LAST3/GOLDEN from the spatial neighbors.
int av1_get_pred_context_uni_comp_ref_p1(const MACROBLOCKD *xd) {
  const uint8_t *const ref_counts = &xd->neighbors_ref_counts[0];

  // Count of LAST2
  const int last2_count = ref_counts[LAST2_FRAME];
  // Count of LAST3 or GOLDEN
  const int last3_or_gld_count =
      ref_counts[LAST3_FRAME] + ref_counts[GOLDEN_FRAME];

  const int pred_context = (last2_count == last3_or_gld_count)
                               ? 1
                               : ((last2_count < last3_or_gld_count) ? 0 : 2);

  assert(pred_context >= 0 && pred_context < UNI_COMP_REF_CONTEXTS);
  return pred_context;
}

// Returns a context number for the given MB prediction signal
//
// Signal the uni-directional compound reference frame pair as
// either (LAST, LAST3) or (LAST, GOLDEN),
// conditioning on the pair is known as one of the above two.
//
// 3 contexts: Voting is used to compare the count of LAST3_FRAME with the
//             total count of GOLDEN_FRAME from the spatial neighbors.
int av1_get_pred_context_uni_comp_ref_p2(const MACROBLOCKD *xd) {
  const uint8_t *const ref_counts = &xd->neighbors_ref_counts[0];

  // Count of LAST3
  const int last3_count = ref_counts[LAST3_FRAME];
  // Count of GOLDEN
  const int gld_count = ref_counts[GOLDEN_FRAME];

  const int pred_context =
      (last3_count == gld_count) ? 1 : ((last3_count < gld_count) ? 0 : 2);

  assert(pred_context >= 0 && pred_context < UNI_COMP_REF_CONTEXTS);
  return pred_context;
}

// == Common context functions for both comp and single ref ==
//
// Obtain contexts to signal a reference frame to be either LAST/LAST2 or
// LAST3/GOLDEN.
static int get_pred_context_ll2_or_l3gld(const MACROBLOCKD *xd) {
  const uint8_t *const ref_counts = &xd->neighbors_ref_counts[0];

  // Count of LAST + LAST2
  const int last_last2_count = ref_counts[LAST_FRAME] + ref_counts[LAST2_FRAME];
  // Count of LAST3 + GOLDEN
  const int last3_gld_count =
      ref_counts[LAST3_FRAME] + ref_counts[GOLDEN_FRAME];

  const int pred_context = (last_last2_count == last3_gld_count)
                               ? 1
                               : ((last_last2_count < last3_gld_count) ? 0 : 2);

  assert(pred_context >= 0 && pred_context < REF_CONTEXTS);
  return pred_context;
}

// Obtain contexts to signal a reference frame to be either LAST or LAST2.
static int get_pred_context_last_or_last2(const MACROBLOCKD *xd) {
  const uint8_t *const ref_counts = &xd->neighbors_ref_counts[0];

  // Count of LAST
  const int last_count = ref_counts[LAST_FRAME];
  // Count of LAST2
  const int last2_count = ref_counts[LAST2_FRAME];

  const int pred_context =
      (last_count == last2_count) ? 1 : ((last_count < last2_count) ? 0 : 2);

  assert(pred_context >= 0 && pred_context < REF_CONTEXTS);
  return pred_context;
}

// Obtain contexts to signal a reference frame to be either LAST3 or GOLDEN.
static int get_pred_context_last3_or_gld(const MACROBLOCKD *xd) {
  const uint8_t *const ref_counts = &xd->neighbors_ref_counts[0];

  // Count of LAST3
  const int last3_count = ref_counts[LAST3_FRAME];
  // Count of GOLDEN
  const int gld_count = ref_counts[GOLDEN_FRAME];

  const int pred_context =
      (last3_count == gld_count) ? 1 : ((last3_count < gld_count) ? 0 : 2);

  assert(pred_context >= 0 && pred_context < REF_CONTEXTS);
  return pred_context;
}

// Obtain contexts to signal a reference frame be either BWDREF/ALTREF2, or
// ALTREF.
static int get_pred_context_brfarf2_or_arf(const MACROBLOCKD *xd) {
  const uint8_t *const ref_counts = &xd->neighbors_ref_counts[0];

  // Counts of BWDREF, ALTREF2, or ALTREF frames (B, A2, or A)
  const int brfarf2_count =
      ref_counts[BWDREF_FRAME] + ref_counts[ALTREF2_FRAME];
  const int arf_count = ref_counts[ALTREF_FRAME];

  const int pred_context =
      (brfarf2_count == arf_count) ? 1 : ((brfarf2_count < arf_count) ? 0 : 2);

  assert(pred_context >= 0 && pred_context < REF_CONTEXTS);
  return pred_context;
}

// Obtain contexts to signal a reference frame be either BWDREF or ALTREF2.
static int get_pred_context_brf_or_arf2(const MACROBLOCKD *xd) {
  const uint8_t *const ref_counts = &xd->neighbors_ref_counts[0];

  // Count of BWDREF frames (B)
  const int brf_count = ref_counts[BWDREF_FRAME];
  // Count of ALTREF2 frames (A2)
  const int arf2_count = ref_counts[ALTREF2_FRAME];

  const int pred_context =
      (brf_count == arf2_count) ? 1 : ((brf_count < arf2_count) ? 0 : 2);

  assert(pred_context >= 0 && pred_context < REF_CONTEXTS);
  return pred_context;
}

// == Context functions for comp ref ==
//
// Returns a context number for the given MB prediction signal
// Signal the first reference frame for a compound mode be either
// GOLDEN/LAST3, or LAST/LAST2.
int av1_get_pred_context_comp_ref_p(const MACROBLOCKD *xd) {
  return get_pred_context_ll2_or_l3gld(xd);
}

// Returns a context number for the given MB prediction signal
// Signal the first reference frame for a compound mode be LAST,
// conditioning on that it is known either LAST/LAST2.
int av1_get_pred_context_comp_ref_p1(const MACROBLOCKD *xd) {
  return get_pred_context_last_or_last2(xd);
}

// Returns a context number for the given MB prediction signal
// Signal the first reference frame for a compound mode be GOLDEN,
// conditioning on that it is known either GOLDEN or LAST3.
int av1_get_pred_context_comp_ref_p2(const MACROBLOCKD *xd) {
  return get_pred_context_last3_or_gld(xd);
}

// Signal the 2nd reference frame for a compound mode be either
// ALTREF, or ALTREF2/BWDREF.
int av1_get_pred_context_comp_bwdref_p(const MACROBLOCKD *xd) {
  return get_pred_context_brfarf2_or_arf(xd);
}

// Signal the 2nd reference frame for a compound mode be either
// ALTREF2 or BWDREF.
int av1_get_pred_context_comp_bwdref_p1(const MACROBLOCKD *xd) {
  return get_pred_context_brf_or_arf2(xd);
}

// == Context functions for single ref ==
//
// For the bit to signal whether the single reference is a forward reference
// frame or a backward reference frame.
int av1_get_pred_context_single_ref_p1(const MACROBLOCKD *xd) {
  const uint8_t *const ref_counts = &xd->neighbors_ref_counts[0];

  // Count of forward reference frames
  const int fwd_count = ref_counts[LAST_FRAME] + ref_counts[LAST2_FRAME] +
                        ref_counts[LAST3_FRAME] + ref_counts[GOLDEN_FRAME];
  // Count of backward reference frames
  const int bwd_count = ref_counts[BWDREF_FRAME] + ref_counts[ALTREF2_FRAME] +
                        ref_counts[ALTREF_FRAME];

  const int pred_context =
      (fwd_count == bwd_count) ? 1 : ((fwd_count < bwd_count) ? 0 : 2);

  assert(pred_context >= 0 && pred_context < REF_CONTEXTS);
  return pred_context;
}

// For the bit to signal whether the single reference is ALTREF_FRAME or
// non-ALTREF backward reference frame, knowing that it shall be either of
// these 2 choices.
int av1_get_pred_context_single_ref_p2(const MACROBLOCKD *xd) {
  return get_pred_context_brfarf2_or_arf(xd);
}

// For the bit to signal whether the single reference is LAST3/GOLDEN or
// LAST2/LAST, knowing that it shall be either of these 2 choices.
int av1_get_pred_context_single_ref_p3(const MACROBLOCKD *xd) {
  return get_pred_context_ll2_or_l3gld(xd);
}

// For the bit to signal whether the single reference is LAST2_FRAME or
// LAST_FRAME, knowing that it shall be either of these 2 choices.
int av1_get_pred_context_single_ref_p4(const MACROBLOCKD *xd) {
  return get_pred_context_last_or_last2(xd);
}

// For the bit to signal whether the single reference is GOLDEN_FRAME or
// LAST3_FRAME, knowing that it shall be either of these 2 choices.
int av1_get_pred_context_single_ref_p5(const MACROBLOCKD *xd) {
  return get_pred_context_last3_or_gld(xd);
}

// For the bit to signal whether the single reference is ALTREF2_FRAME or
// BWDREF_FRAME, knowing that it shall be either of these 2 choices.
int av1_get_pred_context_single_ref_p6(const MACROBLOCKD *xd) {
  return get_pred_context_brf_or_arf2(xd);
}
