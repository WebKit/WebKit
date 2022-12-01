/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "config/aom_dsp_rtcd.h"
#include "config/aom_scale_rtcd.h"

#include "aom/aom_integer.h"
#include "av1/common/av1_common_int.h"
#include "av1/common/reconinter.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/ethread.h"
#include "av1/encoder/pickcdef.h"

// Get primary and secondary filter strength for the given strength index and
// search method
static INLINE void get_cdef_filter_strengths(CDEF_PICK_METHOD pick_method,
                                             int *pri_strength,
                                             int *sec_strength,
                                             int strength_idx) {
  const int tot_sec_filter =
      (pick_method == CDEF_FAST_SEARCH_LVL5)
          ? REDUCED_SEC_STRENGTHS_LVL5
          : ((pick_method >= CDEF_FAST_SEARCH_LVL3) ? REDUCED_SEC_STRENGTHS_LVL3
                                                    : CDEF_SEC_STRENGTHS);
  const int pri_idx = strength_idx / tot_sec_filter;
  const int sec_idx = strength_idx % tot_sec_filter;
  *pri_strength = pri_idx;
  *sec_strength = sec_idx;
  if (pick_method == CDEF_FULL_SEARCH) return;

  switch (pick_method) {
    case CDEF_FAST_SEARCH_LVL1: *pri_strength = priconv_lvl1[pri_idx]; break;
    case CDEF_FAST_SEARCH_LVL2: *pri_strength = priconv_lvl2[pri_idx]; break;
    case CDEF_FAST_SEARCH_LVL3:
      *pri_strength = priconv_lvl2[pri_idx];
      *sec_strength = secconv_lvl3[sec_idx];
      break;
    case CDEF_FAST_SEARCH_LVL4:
      *pri_strength = priconv_lvl4[pri_idx];
      *sec_strength = secconv_lvl3[sec_idx];
      break;
    case CDEF_FAST_SEARCH_LVL5:
      *pri_strength = priconv_lvl5[pri_idx];
      *sec_strength = secconv_lvl5[sec_idx];
      break;
    default: assert(0 && "Invalid CDEF search method");
  }
}

// Store CDEF filter strength calculated from strength index for given search
// method
#define STORE_CDEF_FILTER_STRENGTH(cdef_strength, pick_method, strength_idx) \
  do {                                                                       \
    get_cdef_filter_strengths((pick_method), &pri_strength, &sec_strength,   \
                              (strength_idx));                               \
    cdef_strength = pri_strength * CDEF_SEC_STRENGTHS + sec_strength;        \
  } while (0)

/* Search for the best strength to add as an option, knowing we
   already selected nb_strengths options. */
static uint64_t search_one(int *lev, int nb_strengths,
                           uint64_t mse[][TOTAL_STRENGTHS], int sb_count,
                           CDEF_PICK_METHOD pick_method) {
  uint64_t tot_mse[TOTAL_STRENGTHS];
  const int total_strengths = nb_cdef_strengths[pick_method];
  int i, j;
  uint64_t best_tot_mse = (uint64_t)1 << 63;
  int best_id = 0;
  memset(tot_mse, 0, sizeof(tot_mse));
  for (i = 0; i < sb_count; i++) {
    int gi;
    uint64_t best_mse = (uint64_t)1 << 63;
    /* Find best mse among already selected options. */
    for (gi = 0; gi < nb_strengths; gi++) {
      if (mse[i][lev[gi]] < best_mse) {
        best_mse = mse[i][lev[gi]];
      }
    }
    /* Find best mse when adding each possible new option. */
    for (j = 0; j < total_strengths; j++) {
      uint64_t best = best_mse;
      if (mse[i][j] < best) best = mse[i][j];
      tot_mse[j] += best;
    }
  }
  for (j = 0; j < total_strengths; j++) {
    if (tot_mse[j] < best_tot_mse) {
      best_tot_mse = tot_mse[j];
      best_id = j;
    }
  }
  lev[nb_strengths] = best_id;
  return best_tot_mse;
}

/* Search for the best luma+chroma strength to add as an option, knowing we
   already selected nb_strengths options. */
static uint64_t search_one_dual(int *lev0, int *lev1, int nb_strengths,
                                uint64_t (**mse)[TOTAL_STRENGTHS], int sb_count,
                                CDEF_PICK_METHOD pick_method) {
  uint64_t tot_mse[TOTAL_STRENGTHS][TOTAL_STRENGTHS];
  int i, j;
  uint64_t best_tot_mse = (uint64_t)1 << 63;
  int best_id0 = 0;
  int best_id1 = 0;
  const int total_strengths = nb_cdef_strengths[pick_method];
  memset(tot_mse, 0, sizeof(tot_mse));
  for (i = 0; i < sb_count; i++) {
    int gi;
    uint64_t best_mse = (uint64_t)1 << 63;
    /* Find best mse among already selected options. */
    for (gi = 0; gi < nb_strengths; gi++) {
      uint64_t curr = mse[0][i][lev0[gi]];
      curr += mse[1][i][lev1[gi]];
      if (curr < best_mse) {
        best_mse = curr;
      }
    }
    /* Find best mse when adding each possible new option. */
    for (j = 0; j < total_strengths; j++) {
      int k;
      for (k = 0; k < total_strengths; k++) {
        uint64_t best = best_mse;
        uint64_t curr = mse[0][i][j];
        curr += mse[1][i][k];
        if (curr < best) best = curr;
        tot_mse[j][k] += best;
      }
    }
  }
  for (j = 0; j < total_strengths; j++) {
    int k;
    for (k = 0; k < total_strengths; k++) {
      if (tot_mse[j][k] < best_tot_mse) {
        best_tot_mse = tot_mse[j][k];
        best_id0 = j;
        best_id1 = k;
      }
    }
  }
  lev0[nb_strengths] = best_id0;
  lev1[nb_strengths] = best_id1;
  return best_tot_mse;
}

/* Search for the set of strengths that minimizes mse. */
static uint64_t joint_strength_search(int *best_lev, int nb_strengths,
                                      uint64_t mse[][TOTAL_STRENGTHS],
                                      int sb_count,
                                      CDEF_PICK_METHOD pick_method) {
  uint64_t best_tot_mse;
  int fast = (pick_method >= CDEF_FAST_SEARCH_LVL1 &&
              pick_method <= CDEF_FAST_SEARCH_LVL5);
  int i;
  best_tot_mse = (uint64_t)1 << 63;
  /* Greedy search: add one strength options at a time. */
  for (i = 0; i < nb_strengths; i++) {
    best_tot_mse = search_one(best_lev, i, mse, sb_count, pick_method);
  }
  /* Trying to refine the greedy search by reconsidering each
     already-selected option. */
  if (!fast) {
    for (i = 0; i < 4 * nb_strengths; i++) {
      int j;
      for (j = 0; j < nb_strengths - 1; j++) best_lev[j] = best_lev[j + 1];
      best_tot_mse =
          search_one(best_lev, nb_strengths - 1, mse, sb_count, pick_method);
    }
  }
  return best_tot_mse;
}

/* Search for the set of luma+chroma strengths that minimizes mse. */
static uint64_t joint_strength_search_dual(int *best_lev0, int *best_lev1,
                                           int nb_strengths,
                                           uint64_t (**mse)[TOTAL_STRENGTHS],
                                           int sb_count,
                                           CDEF_PICK_METHOD pick_method) {
  uint64_t best_tot_mse;
  int i;
  best_tot_mse = (uint64_t)1 << 63;
  /* Greedy search: add one strength options at a time. */
  for (i = 0; i < nb_strengths; i++) {
    best_tot_mse =
        search_one_dual(best_lev0, best_lev1, i, mse, sb_count, pick_method);
  }
  /* Trying to refine the greedy search by reconsidering each
     already-selected option. */
  for (i = 0; i < 4 * nb_strengths; i++) {
    int j;
    for (j = 0; j < nb_strengths - 1; j++) {
      best_lev0[j] = best_lev0[j + 1];
      best_lev1[j] = best_lev1[j + 1];
    }
    best_tot_mse = search_one_dual(best_lev0, best_lev1, nb_strengths - 1, mse,
                                   sb_count, pick_method);
  }
  return best_tot_mse;
}

#if CONFIG_AV1_HIGHBITDEPTH
static void copy_sb16_16_highbd(uint16_t *dst, int dstride, const void *src,
                                int src_voffset, int src_hoffset, int sstride,
                                int vsize, int hsize) {
  int r;
  const uint16_t *src16 = CONVERT_TO_SHORTPTR((uint8_t *)src);
  const uint16_t *base = &src16[src_voffset * sstride + src_hoffset];
  for (r = 0; r < vsize; r++)
    memcpy(dst + r * dstride, base + r * sstride, hsize * sizeof(*base));
}
#endif

static void copy_sb16_16(uint16_t *dst, int dstride, const void *src,
                         int src_voffset, int src_hoffset, int sstride,
                         int vsize, int hsize) {
  int r, c;
  const uint8_t *src8 = (uint8_t *)src;
  const uint8_t *base = &src8[src_voffset * sstride + src_hoffset];
  for (r = 0; r < vsize; r++)
    for (c = 0; c < hsize; c++)
      dst[r * dstride + c] = (uint16_t)base[r * sstride + c];
}

static INLINE void init_src_params(int *src_stride, int *width, int *height,
                                   int *width_log2, int *height_log2,
                                   BLOCK_SIZE bsize) {
  *src_stride = block_size_wide[bsize];
  *width = block_size_wide[bsize];
  *height = block_size_high[bsize];
  *width_log2 = MI_SIZE_LOG2 + mi_size_wide_log2[bsize];
  *height_log2 = MI_SIZE_LOG2 + mi_size_wide_log2[bsize];
}
#if CONFIG_AV1_HIGHBITDEPTH
/* Compute MSE only on the blocks we filtered. */
static uint64_t compute_cdef_dist_highbd(void *dst, int dstride, uint16_t *src,
                                         cdef_list *dlist, int cdef_count,
                                         BLOCK_SIZE bsize, int coeff_shift,
                                         int row, int col) {
  assert(bsize == BLOCK_4X4 || bsize == BLOCK_4X8 || bsize == BLOCK_8X4 ||
         bsize == BLOCK_8X8);
  uint64_t sum = 0;
  int bi, bx, by;
  uint16_t *dst16 = CONVERT_TO_SHORTPTR((uint8_t *)dst);
  uint16_t *dst_buff = &dst16[row * dstride + col];
  int src_stride, width, height, width_log2, height_log2;
  init_src_params(&src_stride, &width, &height, &width_log2, &height_log2,
                  bsize);
  for (bi = 0; bi < cdef_count; bi++) {
    by = dlist[bi].by;
    bx = dlist[bi].bx;
    sum += aom_mse_wxh_16bit_highbd(
        &dst_buff[(by << height_log2) * dstride + (bx << width_log2)], dstride,
        &src[bi << (height_log2 + width_log2)], src_stride, width, height);
  }
  return sum >> 2 * coeff_shift;
}
#endif
static uint64_t compute_cdef_dist(void *dst, int dstride, uint16_t *src,
                                  cdef_list *dlist, int cdef_count,
                                  BLOCK_SIZE bsize, int coeff_shift, int row,
                                  int col) {
  assert(bsize == BLOCK_4X4 || bsize == BLOCK_4X8 || bsize == BLOCK_8X4 ||
         bsize == BLOCK_8X8);
  uint64_t sum = 0;
  int bi, bx, by;
  uint8_t *dst8 = (uint8_t *)dst;
  uint8_t *dst_buff = &dst8[row * dstride + col];
  int src_stride, width, height, width_log2, height_log2;
  init_src_params(&src_stride, &width, &height, &width_log2, &height_log2,
                  bsize);
  for (bi = 0; bi < cdef_count; bi++) {
    by = dlist[bi].by;
    bx = dlist[bi].bx;
    sum += aom_mse_wxh_16bit(
        &dst_buff[(by << height_log2) * dstride + (bx << width_log2)], dstride,
        &src[bi << (height_log2 + width_log2)], src_stride, width, height);
  }
  return sum >> 2 * coeff_shift;
}

// Calculates MSE at block level.
// Inputs:
//   cdef_search_ctx: Pointer to the structure containing parameters related to
//   CDEF search context.
//   fbr: Row index in units of 64x64 block
//   fbc: Column index in units of 64x64 block
// Returns:
//   Nothing will be returned. Contents of cdef_search_ctx will be modified.
void av1_cdef_mse_calc_block(CdefSearchCtx *cdef_search_ctx, int fbr, int fbc,
                             int sb_count) {
  const CommonModeInfoParams *const mi_params = cdef_search_ctx->mi_params;
  const YV12_BUFFER_CONFIG *ref = cdef_search_ctx->ref;
  const int coeff_shift = cdef_search_ctx->coeff_shift;
  const int *mi_wide_l2 = cdef_search_ctx->mi_wide_l2;
  const int *mi_high_l2 = cdef_search_ctx->mi_high_l2;

  // Declare and initialize the temporary buffers.
  DECLARE_ALIGNED(32, uint16_t, tmp_dst[1 << (MAX_SB_SIZE_LOG2 * 2)]);
  DECLARE_ALIGNED(32, uint16_t, inbuf[CDEF_INBUF_SIZE]);
  cdef_list dlist[MI_SIZE_128X128 * MI_SIZE_128X128];
  int dir[CDEF_NBLOCKS][CDEF_NBLOCKS] = { { 0 } };
  int var[CDEF_NBLOCKS][CDEF_NBLOCKS] = { { 0 } };
  uint16_t *const in = inbuf + CDEF_VBORDER * CDEF_BSTRIDE + CDEF_HBORDER;
  int nhb = AOMMIN(MI_SIZE_64X64, mi_params->mi_cols - MI_SIZE_64X64 * fbc);
  int nvb = AOMMIN(MI_SIZE_64X64, mi_params->mi_rows - MI_SIZE_64X64 * fbr);
  int hb_step = 1, vb_step = 1;
  BLOCK_SIZE bs;

  const MB_MODE_INFO *const mbmi =
      mi_params->mi_grid_base[MI_SIZE_64X64 * fbr * mi_params->mi_stride +
                              MI_SIZE_64X64 * fbc];

  uint8_t *ref_buffer[MAX_MB_PLANE] = { ref->y_buffer, ref->u_buffer,
                                        ref->v_buffer };
  int ref_stride[MAX_MB_PLANE] = { ref->y_stride, ref->uv_stride,
                                   ref->uv_stride };

  if (mbmi->bsize == BLOCK_128X128 || mbmi->bsize == BLOCK_128X64 ||
      mbmi->bsize == BLOCK_64X128) {
    bs = mbmi->bsize;
    if (bs == BLOCK_128X128 || bs == BLOCK_128X64) {
      nhb = AOMMIN(MI_SIZE_128X128, mi_params->mi_cols - MI_SIZE_64X64 * fbc);
      hb_step = 2;
    }
    if (bs == BLOCK_128X128 || bs == BLOCK_64X128) {
      nvb = AOMMIN(MI_SIZE_128X128, mi_params->mi_rows - MI_SIZE_64X64 * fbr);
      vb_step = 2;
    }
  } else {
    bs = BLOCK_64X64;
  }
  // Get number of 8x8 blocks which are not skip. Cdef processing happens for
  // 8x8 blocks which are not skip.
  const int cdef_count = av1_cdef_compute_sb_list(
      mi_params, fbr * MI_SIZE_64X64, fbc * MI_SIZE_64X64, dlist, bs);

  const int yoff = CDEF_VBORDER * (fbr != 0);
  const int xoff = CDEF_HBORDER * (fbc != 0);
  int dirinit = 0;
  for (int pli = 0; pli < cdef_search_ctx->num_planes; pli++) {
    for (int i = 0; i < CDEF_INBUF_SIZE; i++) inbuf[i] = CDEF_VERY_LARGE;
    /* We avoid filtering the pixels for which some of the pixels to
    average are outside the frame. We could change the filter instead,
    but it would add special cases for any future vectorization. */
    const int ysize = (nvb << mi_high_l2[pli]) +
                      CDEF_VBORDER * (fbr + vb_step < cdef_search_ctx->nvfb) +
                      yoff;
    const int xsize = (nhb << mi_wide_l2[pli]) +
                      CDEF_HBORDER * (fbc + hb_step < cdef_search_ctx->nhfb) +
                      xoff;
    const int row = fbr * MI_SIZE_64X64 << mi_high_l2[pli];
    const int col = fbc * MI_SIZE_64X64 << mi_wide_l2[pli];
    struct macroblockd_plane pd = cdef_search_ctx->plane[pli];
    cdef_search_ctx->copy_fn(&in[(-yoff * CDEF_BSTRIDE - xoff)], CDEF_BSTRIDE,
                             pd.dst.buf, row - yoff, col - xoff, pd.dst.stride,
                             ysize, xsize);
    for (int gi = 0; gi < cdef_search_ctx->total_strengths; gi++) {
      int pri_strength, sec_strength;
      get_cdef_filter_strengths(cdef_search_ctx->pick_method, &pri_strength,
                                &sec_strength, gi);
      av1_cdef_filter_fb(NULL, tmp_dst, CDEF_BSTRIDE, in,
                         cdef_search_ctx->xdec[pli], cdef_search_ctx->ydec[pli],
                         dir, &dirinit, var, pli, dlist, cdef_count,
                         pri_strength, sec_strength + (sec_strength == 3),
                         cdef_search_ctx->damping, coeff_shift);
      const uint64_t curr_mse = cdef_search_ctx->compute_cdef_dist_fn(
          ref_buffer[pli], ref_stride[pli], tmp_dst, dlist, cdef_count,
          cdef_search_ctx->bsize[pli], coeff_shift, row, col);
      if (pli < 2)
        cdef_search_ctx->mse[pli][sb_count][gi] = curr_mse;
      else
        cdef_search_ctx->mse[1][sb_count][gi] += curr_mse;
    }
  }
  cdef_search_ctx->sb_index[sb_count] =
      MI_SIZE_64X64 * fbr * mi_params->mi_stride + MI_SIZE_64X64 * fbc;
}

// MSE calculation at frame level.
// Inputs:
//   cdef_search_ctx: Pointer to the structure containing parameters related to
//   CDEF search context.
// Returns:
//   Nothing will be returned. Contents of cdef_search_ctx will be modified.
static void cdef_mse_calc_frame(CdefSearchCtx *cdef_search_ctx) {
  // Loop over each sb.
  for (int fbr = 0; fbr < cdef_search_ctx->nvfb; ++fbr) {
    for (int fbc = 0; fbc < cdef_search_ctx->nhfb; ++fbc) {
      // Checks if cdef processing can be skipped for particular sb.
      if (cdef_sb_skip(cdef_search_ctx->mi_params, fbr, fbc)) continue;
      // Calculate mse for each sb and store the relevant sb index.
      av1_cdef_mse_calc_block(cdef_search_ctx, fbr, fbc,
                              cdef_search_ctx->sb_count);
      cdef_search_ctx->sb_count++;
    }
  }
}

// Allocates memory for members of CdefSearchCtx.
// Inputs:
//   cdef_search_ctx: Pointer to the structure containing parameters
//   related to CDEF search context.
// Returns:
//   Nothing will be returned. Contents of cdef_search_ctx will be modified.
static AOM_INLINE bool cdef_alloc_data(CdefSearchCtx *cdef_search_ctx) {
  const int nvfb = cdef_search_ctx->nvfb;
  const int nhfb = cdef_search_ctx->nhfb;
  cdef_search_ctx->sb_index =
      aom_malloc(nvfb * nhfb * sizeof(cdef_search_ctx->sb_index));
  cdef_search_ctx->sb_count = 0;
  cdef_search_ctx->mse[0] =
      aom_malloc(sizeof(**cdef_search_ctx->mse) * nvfb * nhfb);
  cdef_search_ctx->mse[1] =
      aom_malloc(sizeof(**cdef_search_ctx->mse) * nvfb * nhfb);
  if (!(cdef_search_ctx->sb_index && cdef_search_ctx->mse[0] &&
        cdef_search_ctx->mse[1])) {
    aom_free(cdef_search_ctx->sb_index);
    aom_free(cdef_search_ctx->mse[0]);
    aom_free(cdef_search_ctx->mse[1]);
    return false;
  }
  return true;
}

// Deallocates the memory allocated for members of CdefSearchCtx.
// Inputs:
//   cdef_search_ctx: Pointer to the structure containing parameters
//   related to CDEF search context.
// Returns:
//   Nothing will be returned.
static AOM_INLINE void cdef_dealloc_data(CdefSearchCtx *cdef_search_ctx) {
  aom_free(cdef_search_ctx->mse[0]);
  aom_free(cdef_search_ctx->mse[1]);
  aom_free(cdef_search_ctx->sb_index);
}

// Initialize the parameters related to CDEF search context.
// Inputs:
//   frame: Pointer to compressed frame buffer
//   ref: Pointer to the frame buffer holding the source frame
//   cm: Pointer to top level common structure
//   xd: Pointer to common current coding block structure
//   cdef_search_ctx: Pointer to the structure containing parameters related to
//   CDEF search context.
//   pick_method: Search method used to select CDEF parameters
// Returns:
//   Nothing will be returned. Contents of cdef_search_ctx will be modified.
static AOM_INLINE void cdef_params_init(const YV12_BUFFER_CONFIG *frame,
                                        const YV12_BUFFER_CONFIG *ref,
                                        AV1_COMMON *cm, MACROBLOCKD *xd,
                                        CdefSearchCtx *cdef_search_ctx,
                                        CDEF_PICK_METHOD pick_method) {
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  const int num_planes = av1_num_planes(cm);
  cdef_search_ctx->mi_params = &cm->mi_params;
  cdef_search_ctx->ref = ref;
  cdef_search_ctx->nvfb =
      (mi_params->mi_rows + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;
  cdef_search_ctx->nhfb =
      (mi_params->mi_cols + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;
  cdef_search_ctx->coeff_shift = AOMMAX(cm->seq_params->bit_depth - 8, 0);
  cdef_search_ctx->damping = 3 + (cm->quant_params.base_qindex >> 6);
  cdef_search_ctx->total_strengths = nb_cdef_strengths[pick_method];
  cdef_search_ctx->num_planes = num_planes;
  cdef_search_ctx->pick_method = pick_method;
  cdef_search_ctx->sb_count = 0;
  av1_setup_dst_planes(xd->plane, cm->seq_params->sb_size, frame, 0, 0, 0,
                       num_planes);
  // Initialize plane wise information.
  for (int pli = 0; pli < num_planes; pli++) {
    cdef_search_ctx->xdec[pli] = xd->plane[pli].subsampling_x;
    cdef_search_ctx->ydec[pli] = xd->plane[pli].subsampling_y;
    cdef_search_ctx->bsize[pli] =
        cdef_search_ctx->ydec[pli]
            ? (cdef_search_ctx->xdec[pli] ? BLOCK_4X4 : BLOCK_8X4)
            : (cdef_search_ctx->xdec[pli] ? BLOCK_4X8 : BLOCK_8X8);
    cdef_search_ctx->mi_wide_l2[pli] =
        MI_SIZE_LOG2 - xd->plane[pli].subsampling_x;
    cdef_search_ctx->mi_high_l2[pli] =
        MI_SIZE_LOG2 - xd->plane[pli].subsampling_y;
    cdef_search_ctx->plane[pli] = xd->plane[pli];
  }
  // Function pointer initialization.
#if CONFIG_AV1_HIGHBITDEPTH
  if (cm->seq_params->use_highbitdepth) {
    cdef_search_ctx->copy_fn = copy_sb16_16_highbd;
    cdef_search_ctx->compute_cdef_dist_fn = compute_cdef_dist_highbd;
  } else {
    cdef_search_ctx->copy_fn = copy_sb16_16;
    cdef_search_ctx->compute_cdef_dist_fn = compute_cdef_dist;
  }
#else
  cdef_search_ctx->copy_fn = copy_sb16_16;
  cdef_search_ctx->compute_cdef_dist_fn = compute_cdef_dist;
#endif
}

static void pick_cdef_from_qp(AV1_COMMON *const cm, int skip_cdef,
                              int frames_since_key, int is_screen_content) {
  const int bd = cm->seq_params->bit_depth;
  const int q =
      av1_ac_quant_QTX(cm->quant_params.base_qindex, 0, bd) >> (bd - 8);
  CdefInfo *const cdef_info = &cm->cdef_info;
  // Check the speed feature to avoid extra signaling.
  if (skip_cdef) {
    cdef_info->cdef_bits = 1;
    cdef_info->nb_cdef_strengths = 2;
  } else {
    cdef_info->cdef_bits = 0;
    cdef_info->nb_cdef_strengths = 1;
  }
  cdef_info->cdef_damping = 3 + (cm->quant_params.base_qindex >> 6);

  int predicted_y_f1 = 0;
  int predicted_y_f2 = 0;
  int predicted_uv_f1 = 0;
  int predicted_uv_f2 = 0;
  if (is_screen_content) {
    predicted_y_f1 =
        (int)(5.88217781e-06 * q * q + 6.10391455e-03 * q + 9.95043102e-02);
    predicted_y_f2 =
        (int)(-7.79934857e-06 * q * q + 6.58957830e-03 * q + 8.81045025e-01);
    predicted_uv_f1 =
        (int)(-6.79500136e-06 * q * q + 1.02695586e-02 * q + 1.36126802e-01);
    predicted_uv_f2 =
        (int)(-9.99613695e-08 * q * q - 1.79361339e-05 * q + 1.17022324e+0);
    predicted_y_f1 = clamp(predicted_y_f1, 0, 15);
    predicted_y_f2 = clamp(predicted_y_f2, 0, 3);
    predicted_uv_f1 = clamp(predicted_uv_f1, 0, 15);
    predicted_uv_f2 = clamp(predicted_uv_f2, 0, 3);
  } else {
    if (!frame_is_intra_only(cm)) {
      predicted_y_f1 = clamp((int)roundf(q * q * -0.0000023593946f +
                                         q * 0.0068615186f + 0.02709886f),
                             0, 15);
      predicted_y_f2 = clamp((int)roundf(q * q * -0.00000057629734f +
                                         q * 0.0013993345f + 0.03831067f),
                             0, 3);
      predicted_uv_f1 = clamp((int)roundf(q * q * -0.0000007095069f +
                                          q * 0.0034628846f + 0.00887099f),
                              0, 15);
      predicted_uv_f2 = clamp((int)roundf(q * q * 0.00000023874085f +
                                          q * 0.00028223585f + 0.05576307f),
                              0, 3);
    } else {
      predicted_y_f1 = clamp(
          (int)roundf(q * q * 0.0000033731974f + q * 0.008070594f + 0.0187634f),
          0, 15);
      predicted_y_f2 = clamp((int)roundf(q * q * 0.0000029167343f +
                                         q * 0.0027798624f + 0.0079405f),
                             0, 3);
      predicted_uv_f1 = clamp((int)roundf(q * q * -0.0000130790995f +
                                          q * 0.012892405f - 0.00748388f),
                              0, 15);
      predicted_uv_f2 = clamp((int)roundf(q * q * 0.0000032651783f +
                                          q * 0.00035520183f + 0.00228092f),
                              0, 3);
    }
  }
  cdef_info->cdef_strengths[0] =
      predicted_y_f1 * CDEF_SEC_STRENGTHS + predicted_y_f2;
  cdef_info->cdef_uv_strengths[0] =
      predicted_uv_f1 * CDEF_SEC_STRENGTHS + predicted_uv_f2;

  if (skip_cdef) {
    cdef_info->cdef_strengths[1] = 0;
    cdef_info->cdef_uv_strengths[1] = 0;
  }
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  const int nvfb = (mi_params->mi_rows + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;
  const int nhfb = (mi_params->mi_cols + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;
  MB_MODE_INFO **mbmi = mi_params->mi_grid_base;
  for (int r = 0; r < nvfb; ++r) {
    for (int c = 0; c < nhfb; ++c) {
      MB_MODE_INFO *current_mbmi = mbmi[MI_SIZE_64X64 * c];
      current_mbmi->cdef_strength = 0;
      if (skip_cdef && current_mbmi->skip_cdef_curr_sb &&
          frames_since_key > 10) {
        current_mbmi->cdef_strength = 1;
      }
    }
    mbmi += MI_SIZE_64X64 * mi_params->mi_stride;
  }
}

void av1_cdef_search(MultiThreadInfo *mt_info, const YV12_BUFFER_CONFIG *frame,
                     const YV12_BUFFER_CONFIG *ref, AV1_COMMON *cm,
                     MACROBLOCKD *xd, CDEF_PICK_METHOD pick_method, int rdmult,
                     int skip_cdef_feature, int frames_since_key,
                     CDEF_CONTROL cdef_control, const int is_screen_content,
                     int non_reference_frame) {
  assert(cdef_control != CDEF_NONE);
  if (cdef_control == CDEF_REFERENCE && non_reference_frame) {
    CdefInfo *const cdef_info = &cm->cdef_info;
    cdef_info->nb_cdef_strengths = 1;
    cdef_info->cdef_bits = 0;
    cdef_info->cdef_strengths[0] = 0;
    cdef_info->cdef_uv_strengths[0] = 0;
    return;
  }

  if (pick_method == CDEF_PICK_FROM_Q) {
    pick_cdef_from_qp(cm, skip_cdef_feature, frames_since_key,
                      is_screen_content);
    return;
  }
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  const int damping = 3 + (cm->quant_params.base_qindex >> 6);
  const int fast = (pick_method >= CDEF_FAST_SEARCH_LVL1 &&
                    pick_method <= CDEF_FAST_SEARCH_LVL5);
  const int num_planes = av1_num_planes(cm);
  CdefSearchCtx cdef_search_ctx;
  // Initialize parameters related to CDEF search context.
  cdef_params_init(frame, ref, cm, xd, &cdef_search_ctx, pick_method);
  // Allocate CDEF search context buffers.
  if (!cdef_alloc_data(&cdef_search_ctx)) {
    CdefInfo *const cdef_info = &cm->cdef_info;
    cdef_info->nb_cdef_strengths = 0;
    cdef_info->cdef_bits = 0;
    cdef_info->cdef_strengths[0] = 0;
    cdef_info->cdef_uv_strengths[0] = 0;
    return;
  }
  // Frame level mse calculation.
  if (mt_info->num_workers > 1) {
    av1_cdef_mse_calc_frame_mt(cm, mt_info, &cdef_search_ctx);
  } else {
    cdef_mse_calc_frame(&cdef_search_ctx);
  }

  /* Search for different number of signaling bits. */
  int nb_strength_bits = 0;
  uint64_t best_rd = UINT64_MAX;
  CdefInfo *const cdef_info = &cm->cdef_info;
  int sb_count = cdef_search_ctx.sb_count;
  uint64_t(*mse[2])[TOTAL_STRENGTHS];
  mse[0] = cdef_search_ctx.mse[0];
  mse[1] = cdef_search_ctx.mse[1];
  for (int i = 0; i <= 3; i++) {
    int best_lev0[CDEF_MAX_STRENGTHS];
    int best_lev1[CDEF_MAX_STRENGTHS] = { 0 };
    const int nb_strengths = 1 << i;
    uint64_t tot_mse;
    if (num_planes > 1) {
      tot_mse = joint_strength_search_dual(best_lev0, best_lev1, nb_strengths,
                                           mse, sb_count, pick_method);
    } else {
      tot_mse = joint_strength_search(best_lev0, nb_strengths, mse[0], sb_count,
                                      pick_method);
    }

    const int total_bits = sb_count * i + nb_strengths * CDEF_STRENGTH_BITS *
                                              (num_planes > 1 ? 2 : 1);
    const int rate_cost = av1_cost_literal(total_bits);
    const uint64_t dist = tot_mse * 16;
    const uint64_t rd = RDCOST(rdmult, rate_cost, dist);
    if (rd < best_rd) {
      best_rd = rd;
      nb_strength_bits = i;
      memcpy(cdef_info->cdef_strengths, best_lev0,
             nb_strengths * sizeof(best_lev0[0]));
      if (num_planes > 1) {
        memcpy(cdef_info->cdef_uv_strengths, best_lev1,
               nb_strengths * sizeof(best_lev1[0]));
      }
    }
  }

  cdef_info->cdef_bits = nb_strength_bits;
  cdef_info->nb_cdef_strengths = 1 << nb_strength_bits;
  for (int i = 0; i < sb_count; i++) {
    uint64_t best_mse = UINT64_MAX;
    int best_gi = 0;
    for (int gi = 0; gi < cdef_info->nb_cdef_strengths; gi++) {
      uint64_t curr = mse[0][i][cdef_info->cdef_strengths[gi]];
      if (num_planes > 1) curr += mse[1][i][cdef_info->cdef_uv_strengths[gi]];
      if (curr < best_mse) {
        best_gi = gi;
        best_mse = curr;
      }
    }
    mi_params->mi_grid_base[cdef_search_ctx.sb_index[i]]->cdef_strength =
        best_gi;
  }
  if (fast) {
    for (int j = 0; j < cdef_info->nb_cdef_strengths; j++) {
      const int luma_strength = cdef_info->cdef_strengths[j];
      const int chroma_strength = cdef_info->cdef_uv_strengths[j];
      int pri_strength, sec_strength;

      STORE_CDEF_FILTER_STRENGTH(cdef_info->cdef_strengths[j], pick_method,
                                 luma_strength);
      STORE_CDEF_FILTER_STRENGTH(cdef_info->cdef_uv_strengths[j], pick_method,
                                 chroma_strength);
    }
  }

  cdef_info->cdef_damping = damping;
  // Deallocate CDEF search context buffers.
  cdef_dealloc_data(&cdef_search_ctx);
}
