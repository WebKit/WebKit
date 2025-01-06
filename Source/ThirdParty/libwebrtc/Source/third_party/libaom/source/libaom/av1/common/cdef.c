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

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "config/aom_scale_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_util/aom_pthread.h"
#include "av1/common/av1_common_int.h"
#include "av1/common/cdef.h"
#include "av1/common/cdef_block.h"
#include "av1/common/common.h"
#include "av1/common/common_data.h"
#include "av1/common/enums.h"
#include "av1/common/reconinter.h"
#include "av1/common/thread_common.h"

static int is_8x8_block_skip(MB_MODE_INFO **grid, int mi_row, int mi_col,
                             int mi_stride) {
  MB_MODE_INFO **mbmi = grid + mi_row * mi_stride + mi_col;
  for (int r = 0; r < mi_size_high[BLOCK_8X8]; ++r, mbmi += mi_stride) {
    for (int c = 0; c < mi_size_wide[BLOCK_8X8]; ++c) {
      if (!mbmi[c]->skip_txfm) return 0;
    }
  }

  return 1;
}

int av1_cdef_compute_sb_list(const CommonModeInfoParams *const mi_params,
                             int mi_row, int mi_col, cdef_list *dlist,
                             BLOCK_SIZE bs) {
  MB_MODE_INFO **grid = mi_params->mi_grid_base;
  int maxc = mi_params->mi_cols - mi_col;
  int maxr = mi_params->mi_rows - mi_row;

  if (bs == BLOCK_128X128 || bs == BLOCK_128X64)
    maxc = AOMMIN(maxc, MI_SIZE_128X128);
  else
    maxc = AOMMIN(maxc, MI_SIZE_64X64);
  if (bs == BLOCK_128X128 || bs == BLOCK_64X128)
    maxr = AOMMIN(maxr, MI_SIZE_128X128);
  else
    maxr = AOMMIN(maxr, MI_SIZE_64X64);

  const int r_step = 2;  // mi_size_high[BLOCK_8X8]
  const int c_step = 2;  // mi_size_wide[BLOCK_8X8]
  const int r_shift = 1;
  const int c_shift = 1;
  int count = 0;
  for (int r = 0; r < maxr; r += r_step) {
    for (int c = 0; c < maxc; c += c_step) {
      if (!is_8x8_block_skip(grid, mi_row + r, mi_col + c,
                             mi_params->mi_stride)) {
        dlist[count].by = r >> r_shift;
        dlist[count].bx = c >> c_shift;
        count++;
      }
    }
  }
  return count;
}

void cdef_copy_rect8_8bit_to_16bit_c(uint16_t *dst, int dstride,
                                     const uint8_t *src, int sstride, int width,
                                     int height) {
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      dst[i * dstride + j] = src[i * sstride + j];
    }
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
void cdef_copy_rect8_16bit_to_16bit_c(uint16_t *dst, int dstride,
                                      const uint16_t *src, int sstride,
                                      int width, int height) {
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      dst[i * dstride + j] = src[i * sstride + j];
    }
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

void av1_cdef_copy_sb8_16_lowbd(uint16_t *const dst, int dstride,
                                const uint8_t *src, int src_voffset,
                                int src_hoffset, int sstride, int vsize,
                                int hsize) {
  const uint8_t *base = &src[src_voffset * (ptrdiff_t)sstride + src_hoffset];
  cdef_copy_rect8_8bit_to_16bit(dst, dstride, base, sstride, hsize, vsize);
}

#if CONFIG_AV1_HIGHBITDEPTH
void av1_cdef_copy_sb8_16_highbd(uint16_t *const dst, int dstride,
                                 const uint8_t *src, int src_voffset,
                                 int src_hoffset, int sstride, int vsize,
                                 int hsize) {
  const uint16_t *base =
      &CONVERT_TO_SHORTPTR(src)[src_voffset * (ptrdiff_t)sstride + src_hoffset];
  cdef_copy_rect8_16bit_to_16bit(dst, dstride, base, sstride, hsize, vsize);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

void av1_cdef_copy_sb8_16(const AV1_COMMON *const cm, uint16_t *const dst,
                          int dstride, const uint8_t *src, int src_voffset,
                          int src_hoffset, int sstride, int vsize, int hsize) {
#if CONFIG_AV1_HIGHBITDEPTH
  if (cm->seq_params->use_highbitdepth) {
    av1_cdef_copy_sb8_16_highbd(dst, dstride, src, src_voffset, src_hoffset,
                                sstride, vsize, hsize);
    return;
  }
#else
  (void)cm;
#endif  // CONFIG_AV1_HIGHBITDEPTH
  av1_cdef_copy_sb8_16_lowbd(dst, dstride, src, src_voffset, src_hoffset,
                             sstride, vsize, hsize);
}

static inline void copy_rect(uint16_t *dst, int dstride, const uint16_t *src,
                             int sstride, int v, int h) {
  for (int i = 0; i < v; i++) {
    for (int j = 0; j < h; j++) {
      dst[i * dstride + j] = src[i * sstride + j];
    }
  }
}

// Prepares intermediate input buffer for CDEF.
// Inputs:
//   cm: Pointer to common structure.
//   fb_info: Pointer to the CDEF block-level parameter structure.
//   colbuf: Left column buffer for CDEF.
//   cdef_left: Left block is filtered or not.
//   fbc, fbr: col and row index of a block.
//   plane: plane index Y/CB/CR.
// Returns:
//   Nothing will be returned.
static void cdef_prepare_fb(const AV1_COMMON *const cm, CdefBlockInfo *fb_info,
                            uint16_t **const colbuf, const int cdef_left,
                            int fbc, int fbr, int plane) {
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  uint16_t *src = fb_info->src;
  const int luma_stride =
      ALIGN_POWER_OF_TWO(mi_params->mi_cols << MI_SIZE_LOG2, 4);
  const int nvfb = (mi_params->mi_rows + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;
  const int nhfb = (mi_params->mi_cols + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;
  int cstart = 0;
  if (!cdef_left) cstart = -CDEF_HBORDER;
  int rend, cend;
  const int nhb =
      AOMMIN(MI_SIZE_64X64, mi_params->mi_cols - MI_SIZE_64X64 * fbc);
  const int nvb =
      AOMMIN(MI_SIZE_64X64, mi_params->mi_rows - MI_SIZE_64X64 * fbr);
  const int hsize = nhb << fb_info->mi_wide_l2;
  const int vsize = nvb << fb_info->mi_high_l2;
  const uint16_t *top_linebuf = fb_info->top_linebuf[plane];
  const uint16_t *bot_linebuf = fb_info->bot_linebuf[plane];
  const int bot_offset = (vsize + CDEF_VBORDER) * CDEF_BSTRIDE;
  const int stride =
      luma_stride >> (plane == AOM_PLANE_Y ? 0 : cm->seq_params->subsampling_x);

  if (fbc == nhfb - 1)
    cend = hsize;
  else
    cend = hsize + CDEF_HBORDER;

  if (fbr == nvfb - 1)
    rend = vsize;
  else
    rend = vsize + CDEF_VBORDER;

  /* Copy in the pixels we need from the current superblock for
  deringing.*/
  av1_cdef_copy_sb8_16(
      cm, &src[CDEF_VBORDER * CDEF_BSTRIDE + CDEF_HBORDER + cstart],
      CDEF_BSTRIDE, fb_info->dst, fb_info->roffset, fb_info->coffset + cstart,
      fb_info->dst_stride, vsize, cend - cstart);

  /* Copy in the pixels we need for the current superblock from bottom buffer.*/
  if (fbr < nvfb - 1) {
    copy_rect(&src[bot_offset + CDEF_HBORDER], CDEF_BSTRIDE,
              &bot_linebuf[fb_info->coffset], stride, CDEF_VBORDER, hsize);
  } else {
    fill_rect(&src[bot_offset + CDEF_HBORDER], CDEF_BSTRIDE, CDEF_VBORDER,
              hsize, CDEF_VERY_LARGE);
  }
  if (fbr < nvfb - 1 && fbc > 0) {
    copy_rect(&src[bot_offset], CDEF_BSTRIDE,
              &bot_linebuf[fb_info->coffset - CDEF_HBORDER], stride,
              CDEF_VBORDER, CDEF_HBORDER);
  } else {
    fill_rect(&src[bot_offset], CDEF_BSTRIDE, CDEF_VBORDER, CDEF_HBORDER,
              CDEF_VERY_LARGE);
  }
  if (fbr < nvfb - 1 && fbc < nhfb - 1) {
    copy_rect(&src[bot_offset + hsize + CDEF_HBORDER], CDEF_BSTRIDE,
              &bot_linebuf[fb_info->coffset + hsize], stride, CDEF_VBORDER,
              CDEF_HBORDER);
  } else {
    fill_rect(&src[bot_offset + hsize + CDEF_HBORDER], CDEF_BSTRIDE,
              CDEF_VBORDER, CDEF_HBORDER, CDEF_VERY_LARGE);
  }

  /* Copy in the pixels we need from the current superblock from top buffer.*/
  if (fbr > 0) {
    copy_rect(&src[CDEF_HBORDER], CDEF_BSTRIDE, &top_linebuf[fb_info->coffset],
              stride, CDEF_VBORDER, hsize);
  } else {
    fill_rect(&src[CDEF_HBORDER], CDEF_BSTRIDE, CDEF_VBORDER, hsize,
              CDEF_VERY_LARGE);
  }
  if (fbr > 0 && fbc > 0) {
    copy_rect(src, CDEF_BSTRIDE, &top_linebuf[fb_info->coffset - CDEF_HBORDER],
              stride, CDEF_VBORDER, CDEF_HBORDER);
  } else {
    fill_rect(src, CDEF_BSTRIDE, CDEF_VBORDER, CDEF_HBORDER, CDEF_VERY_LARGE);
  }
  if (fbr > 0 && fbc < nhfb - 1) {
    copy_rect(&src[hsize + CDEF_HBORDER], CDEF_BSTRIDE,
              &top_linebuf[fb_info->coffset + hsize], stride, CDEF_VBORDER,
              CDEF_HBORDER);
  } else {
    fill_rect(&src[hsize + CDEF_HBORDER], CDEF_BSTRIDE, CDEF_VBORDER,
              CDEF_HBORDER, CDEF_VERY_LARGE);
  }
  if (cdef_left) {
    /* If we deringed the superblock on the left then we need to copy in
    saved pixels. */
    copy_rect(src, CDEF_BSTRIDE, colbuf[plane], CDEF_HBORDER,
              rend + CDEF_VBORDER, CDEF_HBORDER);
  }
  /* Saving pixels in case we need to dering the superblock on the
  right. */
  copy_rect(colbuf[plane], CDEF_HBORDER, src + hsize, CDEF_BSTRIDE,
            rend + CDEF_VBORDER, CDEF_HBORDER);

  if (fb_info->frame_boundary[LEFT]) {
    fill_rect(src, CDEF_BSTRIDE, vsize + 2 * CDEF_VBORDER, CDEF_HBORDER,
              CDEF_VERY_LARGE);
  }
  if (fb_info->frame_boundary[RIGHT]) {
    fill_rect(&src[hsize + CDEF_HBORDER], CDEF_BSTRIDE,
              vsize + 2 * CDEF_VBORDER, CDEF_HBORDER, CDEF_VERY_LARGE);
  }
}

static inline void cdef_filter_fb(CdefBlockInfo *const fb_info, int plane,
                                  uint8_t use_highbitdepth) {
  ptrdiff_t offset =
      (ptrdiff_t)fb_info->dst_stride * fb_info->roffset + fb_info->coffset;
  if (use_highbitdepth) {
    av1_cdef_filter_fb(
        NULL, CONVERT_TO_SHORTPTR(fb_info->dst + offset), fb_info->dst_stride,
        &fb_info->src[CDEF_VBORDER * CDEF_BSTRIDE + CDEF_HBORDER],
        fb_info->xdec, fb_info->ydec, fb_info->dir, NULL, fb_info->var, plane,
        fb_info->dlist, fb_info->cdef_count, fb_info->level,
        fb_info->sec_strength, fb_info->damping, fb_info->coeff_shift);
  } else {
    av1_cdef_filter_fb(
        fb_info->dst + offset, NULL, fb_info->dst_stride,
        &fb_info->src[CDEF_VBORDER * CDEF_BSTRIDE + CDEF_HBORDER],
        fb_info->xdec, fb_info->ydec, fb_info->dir, NULL, fb_info->var, plane,
        fb_info->dlist, fb_info->cdef_count, fb_info->level,
        fb_info->sec_strength, fb_info->damping, fb_info->coeff_shift);
  }
}

// Initializes block-level parameters for CDEF.
static inline void cdef_init_fb_col(const MACROBLOCKD *const xd,
                                    CdefBlockInfo *const fb_info, int *level,
                                    int *sec_strength, int fbc, int fbr,
                                    int plane) {
  const PLANE_TYPE plane_type = get_plane_type(plane);
  fb_info->level = level[plane_type];
  fb_info->sec_strength = sec_strength[plane_type];
  fb_info->dst = xd->plane[plane].dst.buf;
  fb_info->dst_stride = xd->plane[plane].dst.stride;

  fb_info->xdec = xd->plane[plane].subsampling_x;
  fb_info->ydec = xd->plane[plane].subsampling_y;
  fb_info->mi_wide_l2 = MI_SIZE_LOG2 - xd->plane[plane].subsampling_x;
  fb_info->mi_high_l2 = MI_SIZE_LOG2 - xd->plane[plane].subsampling_y;
  fb_info->roffset = MI_SIZE_64X64 * fbr << fb_info->mi_high_l2;
  fb_info->coffset = MI_SIZE_64X64 * fbc << fb_info->mi_wide_l2;
}

static void cdef_fb_col(const AV1_COMMON *const cm, const MACROBLOCKD *const xd,
                        CdefBlockInfo *const fb_info, uint16_t **const colbuf,
                        int *cdef_left, int fbc, int fbr) {
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  const int mbmi_cdef_strength =
      mi_params
          ->mi_grid_base[MI_SIZE_64X64 * fbr * mi_params->mi_stride +
                         MI_SIZE_64X64 * fbc]
          ->cdef_strength;
  const int num_planes = av1_num_planes(cm);
  int is_zero_level[PLANE_TYPES] = { 1, 1 };
  int level[PLANE_TYPES] = { 0 };
  int sec_strength[PLANE_TYPES] = { 0 };
  const CdefInfo *const cdef_info = &cm->cdef_info;

  if (mi_params->mi_grid_base[MI_SIZE_64X64 * fbr * mi_params->mi_stride +
                              MI_SIZE_64X64 * fbc] == NULL ||
      mbmi_cdef_strength == -1) {
    av1_zero_array(cdef_left, num_planes);
    return;
  }

  // Compute level and secondary strength for planes
  level[PLANE_TYPE_Y] =
      cdef_info->cdef_strengths[mbmi_cdef_strength] / CDEF_SEC_STRENGTHS;
  sec_strength[PLANE_TYPE_Y] =
      cdef_info->cdef_strengths[mbmi_cdef_strength] % CDEF_SEC_STRENGTHS;
  sec_strength[PLANE_TYPE_Y] += sec_strength[PLANE_TYPE_Y] == 3;
  is_zero_level[PLANE_TYPE_Y] =
      (level[PLANE_TYPE_Y] == 0) && (sec_strength[PLANE_TYPE_Y] == 0);

  if (num_planes > 1) {
    level[PLANE_TYPE_UV] =
        cdef_info->cdef_uv_strengths[mbmi_cdef_strength] / CDEF_SEC_STRENGTHS;
    sec_strength[PLANE_TYPE_UV] =
        cdef_info->cdef_uv_strengths[mbmi_cdef_strength] % CDEF_SEC_STRENGTHS;
    sec_strength[PLANE_TYPE_UV] += sec_strength[PLANE_TYPE_UV] == 3;
    is_zero_level[PLANE_TYPE_UV] =
        (level[PLANE_TYPE_UV] == 0) && (sec_strength[PLANE_TYPE_UV] == 0);
  }

  if (is_zero_level[PLANE_TYPE_Y] && is_zero_level[PLANE_TYPE_UV]) {
    av1_zero_array(cdef_left, num_planes);
    return;
  }

  fb_info->cdef_count = av1_cdef_compute_sb_list(mi_params, fbr * MI_SIZE_64X64,
                                                 fbc * MI_SIZE_64X64,
                                                 fb_info->dlist, BLOCK_64X64);
  if (!fb_info->cdef_count) {
    av1_zero_array(cdef_left, num_planes);
    return;
  }

  for (int plane = 0; plane < num_planes; plane++) {
    // Do not skip cdef filtering for luma plane as filter direction is
    // computed based on luma.
    if (plane && is_zero_level[get_plane_type(plane)]) {
      cdef_left[plane] = 0;
      continue;
    }
    cdef_init_fb_col(xd, fb_info, level, sec_strength, fbc, fbr, plane);
    cdef_prepare_fb(cm, fb_info, colbuf, cdef_left[plane], fbc, fbr, plane);
    cdef_filter_fb(fb_info, plane, cm->seq_params->use_highbitdepth);
    cdef_left[plane] = 1;
  }
}

// Initializes row-level parameters for CDEF frame.
void av1_cdef_init_fb_row(const AV1_COMMON *const cm,
                          const MACROBLOCKD *const xd,
                          CdefBlockInfo *const fb_info,
                          uint16_t **const linebuf, uint16_t *const src,
                          struct AV1CdefSyncData *const cdef_sync, int fbr) {
  (void)cdef_sync;
  const int num_planes = av1_num_planes(cm);
  const int nvfb = (cm->mi_params.mi_rows + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;
  const int luma_stride =
      ALIGN_POWER_OF_TWO(cm->mi_params.mi_cols << MI_SIZE_LOG2, 4);
  const bool ping_pong = fbr & 1;
  // for the current filter block, it's top left corner mi structure (mi_tl)
  // is first accessed to check whether the top and left boundaries are
  // frame boundaries. Then bottom-left and top-right mi structures are
  // accessed to check whether the bottom and right boundaries
  // (respectively) are frame boundaries.
  //
  // Note that we can't just check the bottom-right mi structure - eg. if
  // we're at the right-hand edge of the frame but not the bottom, then
  // the bottom-right mi is NULL but the bottom-left is not.
  fb_info->frame_boundary[TOP] = (MI_SIZE_64X64 * fbr == 0) ? 1 : 0;
  if (fbr != nvfb - 1)
    fb_info->frame_boundary[BOTTOM] =
        (MI_SIZE_64X64 * (fbr + 1) == cm->mi_params.mi_rows) ? 1 : 0;
  else
    fb_info->frame_boundary[BOTTOM] = 1;

  fb_info->src = src;
  fb_info->damping = cm->cdef_info.cdef_damping;
  fb_info->coeff_shift = AOMMAX(cm->seq_params->bit_depth - 8, 0);
  av1_zero(fb_info->dir);
  av1_zero(fb_info->var);

  for (int plane = 0; plane < num_planes; plane++) {
    const int mi_high_l2 = MI_SIZE_LOG2 - xd->plane[plane].subsampling_y;
    const int offset = MI_SIZE_64X64 * (fbr + 1) << mi_high_l2;
    const int stride = luma_stride >> xd->plane[plane].subsampling_x;
    // here ping-pong buffers are maintained for top linebuf
    // to avoid linebuf over-write by consecutive row.
    uint16_t *const top_linebuf =
        &linebuf[plane][ping_pong * CDEF_VBORDER * stride];
    fb_info->bot_linebuf[plane] = &linebuf[plane][(CDEF_VBORDER << 1) * stride];

    if (fbr != nvfb - 1)  // top line buffer copy
      av1_cdef_copy_sb8_16(cm, top_linebuf, stride, xd->plane[plane].dst.buf,
                           offset - CDEF_VBORDER, 0,
                           xd->plane[plane].dst.stride, CDEF_VBORDER, stride);
    fb_info->top_linebuf[plane] =
        &linebuf[plane][(!ping_pong) * CDEF_VBORDER * stride];

    if (fbr != nvfb - 1)  // bottom line buffer copy
      av1_cdef_copy_sb8_16(cm, fb_info->bot_linebuf[plane], stride,
                           xd->plane[plane].dst.buf, offset, 0,
                           xd->plane[plane].dst.stride, CDEF_VBORDER, stride);
  }
}

void av1_cdef_fb_row(const AV1_COMMON *const cm, MACROBLOCKD *xd,
                     uint16_t **const linebuf, uint16_t **const colbuf,
                     uint16_t *const src, int fbr,
                     cdef_init_fb_row_t cdef_init_fb_row_fn,
                     struct AV1CdefSyncData *const cdef_sync,
                     struct aom_internal_error_info *error_info) {
  // TODO(aomedia:3276): Pass error_info to the low-level functions as required
  // in future to handle error propagation.
  (void)error_info;
  CdefBlockInfo fb_info;
  int cdef_left[MAX_MB_PLANE] = { 1, 1, 1 };
  const int nhfb = (cm->mi_params.mi_cols + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;

  cdef_init_fb_row_fn(cm, xd, &fb_info, linebuf, src, cdef_sync, fbr);
#if CONFIG_MULTITHREAD
  if (cdef_sync && cm->cdef_info.allocated_num_workers > 1) {
    pthread_mutex_lock(cdef_sync->mutex_);
    const bool cdef_mt_exit = cdef_sync->cdef_mt_exit;
    pthread_mutex_unlock(cdef_sync->mutex_);
    // Exit in case any worker has encountered an error.
    if (cdef_mt_exit) return;
  }
#endif
  for (int fbc = 0; fbc < nhfb; fbc++) {
    fb_info.frame_boundary[LEFT] = (MI_SIZE_64X64 * fbc == 0) ? 1 : 0;
    if (fbc != nhfb - 1)
      fb_info.frame_boundary[RIGHT] =
          (MI_SIZE_64X64 * (fbc + 1) == cm->mi_params.mi_cols) ? 1 : 0;
    else
      fb_info.frame_boundary[RIGHT] = 1;
    cdef_fb_col(cm, xd, &fb_info, colbuf, &cdef_left[0], fbc, fbr);
  }
}

// Perform CDEF on input frame.
// Inputs:
//   frame: Pointer to input frame buffer.
//   cm: Pointer to common structure.
//   xd: Pointer to common current coding block structure.
// Returns:
//   Nothing will be returned.
void av1_cdef_frame(YV12_BUFFER_CONFIG *frame, AV1_COMMON *const cm,
                    MACROBLOCKD *xd, cdef_init_fb_row_t cdef_init_fb_row_fn) {
  const int num_planes = av1_num_planes(cm);
  const int nvfb = (cm->mi_params.mi_rows + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;

  av1_setup_dst_planes(xd->plane, cm->seq_params->sb_size, frame, 0, 0, 0,
                       num_planes);

  for (int fbr = 0; fbr < nvfb; fbr++)
    av1_cdef_fb_row(cm, xd, cm->cdef_info.linebuf, cm->cdef_info.colbuf,
                    cm->cdef_info.srcbuf, fbr, cdef_init_fb_row_fn, NULL,
                    xd->error_info);
}
