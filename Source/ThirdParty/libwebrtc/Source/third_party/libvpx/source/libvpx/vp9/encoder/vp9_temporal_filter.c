/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <math.h>
#include <limits.h>

#include "vp9/common/vp9_alloccommon.h"
#include "vp9/common/vp9_common.h"
#include "vp9/common/vp9_onyxc_int.h"
#include "vp9/common/vp9_quant_common.h"
#include "vp9/common/vp9_reconinter.h"
#include "vp9/encoder/vp9_encodeframe.h"
#include "vp9/encoder/vp9_ethread.h"
#include "vp9/encoder/vp9_extend.h"
#include "vp9/encoder/vp9_firstpass.h"
#include "vp9/encoder/vp9_mcomp.h"
#include "vp9/encoder/vp9_encoder.h"
#include "vp9/encoder/vp9_quantize.h"
#include "vp9/encoder/vp9_ratectrl.h"
#include "vp9/encoder/vp9_segmentation.h"
#include "vp9/encoder/vp9_temporal_filter.h"
#include "vpx_dsp/vpx_dsp_common.h"
#include "vpx_mem/vpx_mem.h"
#include "vpx_ports/mem.h"
#include "vpx_ports/vpx_timer.h"
#include "vpx_scale/vpx_scale.h"

static int fixed_divide[512];

static void temporal_filter_predictors_mb_c(
    MACROBLOCKD *xd, uint8_t *y_mb_ptr, uint8_t *u_mb_ptr, uint8_t *v_mb_ptr,
    int stride, int uv_block_width, int uv_block_height, int mv_row, int mv_col,
    uint8_t *pred, struct scale_factors *scale, int x, int y) {
  const int which_mv = 0;
  const MV mv = { mv_row, mv_col };
  const InterpKernel *const kernel = vp9_filter_kernels[EIGHTTAP_SHARP];

  enum mv_precision mv_precision_uv;
  int uv_stride;
  if (uv_block_width == 8) {
    uv_stride = (stride + 1) >> 1;
    mv_precision_uv = MV_PRECISION_Q4;
  } else {
    uv_stride = stride;
    mv_precision_uv = MV_PRECISION_Q3;
  }

#if CONFIG_VP9_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    vp9_highbd_build_inter_predictor(CONVERT_TO_SHORTPTR(y_mb_ptr), stride,
                                     CONVERT_TO_SHORTPTR(&pred[0]), 16, &mv,
                                     scale, 16, 16, which_mv, kernel,
                                     MV_PRECISION_Q3, x, y, xd->bd);

    vp9_highbd_build_inter_predictor(CONVERT_TO_SHORTPTR(u_mb_ptr), uv_stride,
                                     CONVERT_TO_SHORTPTR(&pred[256]),
                                     uv_block_width, &mv, scale, uv_block_width,
                                     uv_block_height, which_mv, kernel,
                                     mv_precision_uv, x, y, xd->bd);

    vp9_highbd_build_inter_predictor(CONVERT_TO_SHORTPTR(v_mb_ptr), uv_stride,
                                     CONVERT_TO_SHORTPTR(&pred[512]),
                                     uv_block_width, &mv, scale, uv_block_width,
                                     uv_block_height, which_mv, kernel,
                                     mv_precision_uv, x, y, xd->bd);
    return;
  }
#endif  // CONFIG_VP9_HIGHBITDEPTH
  (void)xd;
  vp9_build_inter_predictor(y_mb_ptr, stride, &pred[0], 16, &mv, scale, 16, 16,
                            which_mv, kernel, MV_PRECISION_Q3, x, y);

  vp9_build_inter_predictor(u_mb_ptr, uv_stride, &pred[256], uv_block_width,
                            &mv, scale, uv_block_width, uv_block_height,
                            which_mv, kernel, mv_precision_uv, x, y);

  vp9_build_inter_predictor(v_mb_ptr, uv_stride, &pred[512], uv_block_width,
                            &mv, scale, uv_block_width, uv_block_height,
                            which_mv, kernel, mv_precision_uv, x, y);
}

void vp9_temporal_filter_init(void) {
  int i;

  fixed_divide[0] = 0;
  for (i = 1; i < 512; ++i) fixed_divide[i] = 0x80000 / i;
}

void vp9_temporal_filter_apply_c(const uint8_t *frame1, unsigned int stride,
                                 const uint8_t *frame2,
                                 unsigned int block_width,
                                 unsigned int block_height, int strength,
                                 int filter_weight, uint32_t *accumulator,
                                 uint16_t *count) {
  unsigned int i, j, k;
  int modifier;
  int byte = 0;
  const int rounding = strength > 0 ? 1 << (strength - 1) : 0;

  assert(strength >= 0);
  assert(strength <= 6);

  assert(filter_weight >= 0);
  assert(filter_weight <= 2);

  for (i = 0, k = 0; i < block_height; i++) {
    for (j = 0; j < block_width; j++, k++) {
      int pixel_value = *frame2;

      // non-local mean approach
      int diff_sse[9] = { 0 };
      int idx, idy, index = 0;

      for (idy = -1; idy <= 1; ++idy) {
        for (idx = -1; idx <= 1; ++idx) {
          int row = (int)i + idy;
          int col = (int)j + idx;

          if (row >= 0 && row < (int)block_height && col >= 0 &&
              col < (int)block_width) {
            int diff = frame1[byte + idy * (int)stride + idx] -
                       frame2[idy * (int)block_width + idx];
            diff_sse[index] = diff * diff;
            ++index;
          }
        }
      }

      assert(index > 0);

      modifier = 0;
      for (idx = 0; idx < 9; ++idx) modifier += diff_sse[idx];

      modifier *= 3;
      modifier /= index;

      ++frame2;

      modifier += rounding;
      modifier >>= strength;

      if (modifier > 16) modifier = 16;

      modifier = 16 - modifier;
      modifier *= filter_weight;

      count[k] += modifier;
      accumulator[k] += modifier * pixel_value;

      byte++;
    }

    byte += stride - block_width;
  }
}

#if CONFIG_VP9_HIGHBITDEPTH
void vp9_highbd_temporal_filter_apply_c(
    const uint8_t *frame1_8, unsigned int stride, const uint8_t *frame2_8,
    unsigned int block_width, unsigned int block_height, int strength,
    int filter_weight, uint32_t *accumulator, uint16_t *count) {
  const uint16_t *frame1 = CONVERT_TO_SHORTPTR(frame1_8);
  const uint16_t *frame2 = CONVERT_TO_SHORTPTR(frame2_8);
  unsigned int i, j, k;
  int modifier;
  int byte = 0;
  const int rounding = strength > 0 ? 1 << (strength - 1) : 0;

  for (i = 0, k = 0; i < block_height; i++) {
    for (j = 0; j < block_width; j++, k++) {
      int pixel_value = *frame2;
      int diff_sse[9] = { 0 };
      int idx, idy, index = 0;

      for (idy = -1; idy <= 1; ++idy) {
        for (idx = -1; idx <= 1; ++idx) {
          int row = (int)i + idy;
          int col = (int)j + idx;

          if (row >= 0 && row < (int)block_height && col >= 0 &&
              col < (int)block_width) {
            int diff = frame1[byte + idy * (int)stride + idx] -
                       frame2[idy * (int)block_width + idx];
            diff_sse[index] = diff * diff;
            ++index;
          }
        }
      }
      assert(index > 0);

      modifier = 0;
      for (idx = 0; idx < 9; ++idx) modifier += diff_sse[idx];

      modifier *= 3;
      modifier /= index;

      ++frame2;
      modifier += rounding;
      modifier >>= strength;

      if (modifier > 16) modifier = 16;

      modifier = 16 - modifier;
      modifier *= filter_weight;

      count[k] += modifier;
      accumulator[k] += modifier * pixel_value;

      byte++;
    }

    byte += stride - block_width;
  }
}
#endif  // CONFIG_VP9_HIGHBITDEPTH

static uint32_t temporal_filter_find_matching_mb_c(VP9_COMP *cpi,
                                                   ThreadData *td,
                                                   uint8_t *arf_frame_buf,
                                                   uint8_t *frame_ptr_buf,
                                                   int stride, MV *ref_mv) {
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MV_SPEED_FEATURES *const mv_sf = &cpi->sf.mv;
  const SEARCH_METHODS search_method = HEX;
  int step_param;
  int sadpb = x->sadperbit16;
  uint32_t bestsme = UINT_MAX;
  uint32_t distortion;
  uint32_t sse;
  int cost_list[5];
  const MvLimits tmp_mv_limits = x->mv_limits;

  MV best_ref_mv1 = { 0, 0 };
  MV best_ref_mv1_full; /* full-pixel value of best_ref_mv1 */

  // Save input state
  struct buf_2d src = x->plane[0].src;
  struct buf_2d pre = xd->plane[0].pre[0];

  best_ref_mv1_full.col = best_ref_mv1.col >> 3;
  best_ref_mv1_full.row = best_ref_mv1.row >> 3;

  // Setup frame pointers
  x->plane[0].src.buf = arf_frame_buf;
  x->plane[0].src.stride = stride;
  xd->plane[0].pre[0].buf = frame_ptr_buf;
  xd->plane[0].pre[0].stride = stride;

  step_param = mv_sf->reduce_first_step_size;
  step_param = VPXMIN(step_param, MAX_MVSEARCH_STEPS - 2);

  vp9_set_mv_search_range(&x->mv_limits, &best_ref_mv1);

  vp9_full_pixel_search(cpi, x, BLOCK_16X16, &best_ref_mv1_full, step_param,
                        search_method, sadpb, cond_cost_list(cpi, cost_list),
                        &best_ref_mv1, ref_mv, 0, 0);

  /* restore UMV window */
  x->mv_limits = tmp_mv_limits;

  // Ignore mv costing by sending NULL pointer instead of cost array
  bestsme = cpi->find_fractional_mv_step(
      x, ref_mv, &best_ref_mv1, cpi->common.allow_high_precision_mv,
      x->errorperbit, &cpi->fn_ptr[BLOCK_16X16], 0,
      mv_sf->subpel_iters_per_step, cond_cost_list(cpi, cost_list), NULL, NULL,
      &distortion, &sse, NULL, 0, 0);

  // Restore input state
  x->plane[0].src = src;
  xd->plane[0].pre[0] = pre;

  return bestsme;
}

void vp9_temporal_filter_iterate_row_c(VP9_COMP *cpi, ThreadData *td,
                                       int mb_row, int mb_col_start,
                                       int mb_col_end) {
  ARNRFilterData *arnr_filter_data = &cpi->arnr_filter_data;
  YV12_BUFFER_CONFIG **frames = arnr_filter_data->frames;
  int frame_count = arnr_filter_data->frame_count;
  int alt_ref_index = arnr_filter_data->alt_ref_index;
  int strength = arnr_filter_data->strength;
  struct scale_factors *scale = &arnr_filter_data->sf;
  int byte;
  int frame;
  int mb_col;
  unsigned int filter_weight;
  int mb_cols = (frames[alt_ref_index]->y_crop_width + 15) >> 4;
  int mb_rows = (frames[alt_ref_index]->y_crop_height + 15) >> 4;
  DECLARE_ALIGNED(16, uint32_t, accumulator[16 * 16 * 3]);
  DECLARE_ALIGNED(16, uint16_t, count[16 * 16 * 3]);
  MACROBLOCKD *mbd = &td->mb.e_mbd;
  YV12_BUFFER_CONFIG *f = frames[alt_ref_index];
  uint8_t *dst1, *dst2;
#if CONFIG_VP9_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint16_t, predictor16[16 * 16 * 3]);
  DECLARE_ALIGNED(16, uint8_t, predictor8[16 * 16 * 3]);
  uint8_t *predictor;
#else
  DECLARE_ALIGNED(16, uint8_t, predictor[16 * 16 * 3]);
#endif
  const int mb_uv_height = 16 >> mbd->plane[1].subsampling_y;
  const int mb_uv_width = 16 >> mbd->plane[1].subsampling_x;
  // Addition of the tile col level offsets
  int mb_y_offset = mb_row * 16 * (f->y_stride) + 16 * mb_col_start;
  int mb_uv_offset =
      mb_row * mb_uv_height * f->uv_stride + mb_uv_width * mb_col_start;

#if CONFIG_VP9_HIGHBITDEPTH
  if (mbd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    predictor = CONVERT_TO_BYTEPTR(predictor16);
  } else {
    predictor = predictor8;
  }
#endif

  // Source frames are extended to 16 pixels. This is different than
  //  L/A/G reference frames that have a border of 32 (VP9ENCBORDERINPIXELS)
  // A 6/8 tap filter is used for motion search.  This requires 2 pixels
  //  before and 3 pixels after.  So the largest Y mv on a border would
  //  then be 16 - VP9_INTERP_EXTEND. The UV blocks are half the size of the
  //  Y and therefore only extended by 8.  The largest mv that a UV block
  //  can support is 8 - VP9_INTERP_EXTEND.  A UV mv is half of a Y mv.
  //  (16 - VP9_INTERP_EXTEND) >> 1 which is greater than
  //  8 - VP9_INTERP_EXTEND.
  // To keep the mv in play for both Y and UV planes the max that it
  //  can be on a border is therefore 16 - (2*VP9_INTERP_EXTEND+1).
  td->mb.mv_limits.row_min = -((mb_row * 16) + (17 - 2 * VP9_INTERP_EXTEND));
  td->mb.mv_limits.row_max =
      ((mb_rows - 1 - mb_row) * 16) + (17 - 2 * VP9_INTERP_EXTEND);

  for (mb_col = mb_col_start; mb_col < mb_col_end; mb_col++) {
    int i, j, k;
    int stride;
    MV ref_mv;

    vp9_zero_array(accumulator, 16 * 16 * 3);
    vp9_zero_array(count, 16 * 16 * 3);

    td->mb.mv_limits.col_min = -((mb_col * 16) + (17 - 2 * VP9_INTERP_EXTEND));
    td->mb.mv_limits.col_max =
        ((mb_cols - 1 - mb_col) * 16) + (17 - 2 * VP9_INTERP_EXTEND);

    if (cpi->oxcf.content == VP9E_CONTENT_FILM) {
      unsigned int src_variance;
      struct buf_2d src;

      src.buf = f->y_buffer + mb_y_offset;
      src.stride = f->y_stride;

#if CONFIG_VP9_HIGHBITDEPTH
      if (mbd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
        src_variance =
            vp9_high_get_sby_perpixel_variance(cpi, &src, BLOCK_16X16, mbd->bd);
      } else {
        src_variance = vp9_get_sby_perpixel_variance(cpi, &src, BLOCK_16X16);
      }
#else
      src_variance = vp9_get_sby_perpixel_variance(cpi, &src, BLOCK_16X16);
#endif  // CONFIG_VP9_HIGHBITDEPTH

      if (src_variance <= 2) strength = VPXMAX(0, (int)strength - 2);
    }

    for (frame = 0; frame < frame_count; frame++) {
      const uint32_t thresh_low = 10000;
      const uint32_t thresh_high = 20000;

      if (frames[frame] == NULL) continue;

      ref_mv.row = 0;
      ref_mv.col = 0;

      if (frame == alt_ref_index) {
        filter_weight = 2;
      } else {
        // Find best match in this frame by MC
        uint32_t err = temporal_filter_find_matching_mb_c(
            cpi, td, frames[alt_ref_index]->y_buffer + mb_y_offset,
            frames[frame]->y_buffer + mb_y_offset, frames[frame]->y_stride,
            &ref_mv);

        // Assign higher weight to matching MB if its error
        // score is lower. If not applying MC default behavior
        // is to weight all MBs equal.
        filter_weight = err < thresh_low ? 2 : err < thresh_high ? 1 : 0;
      }

      if (filter_weight != 0) {
        // Construct the predictors
        temporal_filter_predictors_mb_c(
            mbd, frames[frame]->y_buffer + mb_y_offset,
            frames[frame]->u_buffer + mb_uv_offset,
            frames[frame]->v_buffer + mb_uv_offset, frames[frame]->y_stride,
            mb_uv_width, mb_uv_height, ref_mv.row, ref_mv.col, predictor, scale,
            mb_col * 16, mb_row * 16);

#if CONFIG_VP9_HIGHBITDEPTH
        if (mbd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
          int adj_strength = strength + 2 * (mbd->bd - 8);
          // Apply the filter (YUV)
          vp9_highbd_temporal_filter_apply(
              f->y_buffer + mb_y_offset, f->y_stride, predictor, 16, 16,
              adj_strength, filter_weight, accumulator, count);
          vp9_highbd_temporal_filter_apply(
              f->u_buffer + mb_uv_offset, f->uv_stride, predictor + 256,
              mb_uv_width, mb_uv_height, adj_strength, filter_weight,
              accumulator + 256, count + 256);
          vp9_highbd_temporal_filter_apply(
              f->v_buffer + mb_uv_offset, f->uv_stride, predictor + 512,
              mb_uv_width, mb_uv_height, adj_strength, filter_weight,
              accumulator + 512, count + 512);
        } else {
          // Apply the filter (YUV)
          vp9_temporal_filter_apply(f->y_buffer + mb_y_offset, f->y_stride,
                                    predictor, 16, 16, strength, filter_weight,
                                    accumulator, count);
          vp9_temporal_filter_apply(f->u_buffer + mb_uv_offset, f->uv_stride,
                                    predictor + 256, mb_uv_width, mb_uv_height,
                                    strength, filter_weight, accumulator + 256,
                                    count + 256);
          vp9_temporal_filter_apply(f->v_buffer + mb_uv_offset, f->uv_stride,
                                    predictor + 512, mb_uv_width, mb_uv_height,
                                    strength, filter_weight, accumulator + 512,
                                    count + 512);
        }
#else
        // Apply the filter (YUV)
        vp9_temporal_filter_apply(f->y_buffer + mb_y_offset, f->y_stride,
                                  predictor, 16, 16, strength, filter_weight,
                                  accumulator, count);
        vp9_temporal_filter_apply(f->u_buffer + mb_uv_offset, f->uv_stride,
                                  predictor + 256, mb_uv_width, mb_uv_height,
                                  strength, filter_weight, accumulator + 256,
                                  count + 256);
        vp9_temporal_filter_apply(f->v_buffer + mb_uv_offset, f->uv_stride,
                                  predictor + 512, mb_uv_width, mb_uv_height,
                                  strength, filter_weight, accumulator + 512,
                                  count + 512);
#endif  // CONFIG_VP9_HIGHBITDEPTH
      }
    }

#if CONFIG_VP9_HIGHBITDEPTH
    if (mbd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      uint16_t *dst1_16;
      uint16_t *dst2_16;
      // Normalize filter output to produce AltRef frame
      dst1 = cpi->alt_ref_buffer.y_buffer;
      dst1_16 = CONVERT_TO_SHORTPTR(dst1);
      stride = cpi->alt_ref_buffer.y_stride;
      byte = mb_y_offset;
      for (i = 0, k = 0; i < 16; i++) {
        for (j = 0; j < 16; j++, k++) {
          unsigned int pval = accumulator[k] + (count[k] >> 1);
          pval *= fixed_divide[count[k]];
          pval >>= 19;

          dst1_16[byte] = (uint16_t)pval;

          // move to next pixel
          byte++;
        }

        byte += stride - 16;
      }

      dst1 = cpi->alt_ref_buffer.u_buffer;
      dst2 = cpi->alt_ref_buffer.v_buffer;
      dst1_16 = CONVERT_TO_SHORTPTR(dst1);
      dst2_16 = CONVERT_TO_SHORTPTR(dst2);
      stride = cpi->alt_ref_buffer.uv_stride;
      byte = mb_uv_offset;
      for (i = 0, k = 256; i < mb_uv_height; i++) {
        for (j = 0; j < mb_uv_width; j++, k++) {
          int m = k + 256;

          // U
          unsigned int pval = accumulator[k] + (count[k] >> 1);
          pval *= fixed_divide[count[k]];
          pval >>= 19;
          dst1_16[byte] = (uint16_t)pval;

          // V
          pval = accumulator[m] + (count[m] >> 1);
          pval *= fixed_divide[count[m]];
          pval >>= 19;
          dst2_16[byte] = (uint16_t)pval;

          // move to next pixel
          byte++;
        }

        byte += stride - mb_uv_width;
      }
    } else {
      // Normalize filter output to produce AltRef frame
      dst1 = cpi->alt_ref_buffer.y_buffer;
      stride = cpi->alt_ref_buffer.y_stride;
      byte = mb_y_offset;
      for (i = 0, k = 0; i < 16; i++) {
        for (j = 0; j < 16; j++, k++) {
          unsigned int pval = accumulator[k] + (count[k] >> 1);
          pval *= fixed_divide[count[k]];
          pval >>= 19;

          dst1[byte] = (uint8_t)pval;

          // move to next pixel
          byte++;
        }
        byte += stride - 16;
      }

      dst1 = cpi->alt_ref_buffer.u_buffer;
      dst2 = cpi->alt_ref_buffer.v_buffer;
      stride = cpi->alt_ref_buffer.uv_stride;
      byte = mb_uv_offset;
      for (i = 0, k = 256; i < mb_uv_height; i++) {
        for (j = 0; j < mb_uv_width; j++, k++) {
          int m = k + 256;

          // U
          unsigned int pval = accumulator[k] + (count[k] >> 1);
          pval *= fixed_divide[count[k]];
          pval >>= 19;
          dst1[byte] = (uint8_t)pval;

          // V
          pval = accumulator[m] + (count[m] >> 1);
          pval *= fixed_divide[count[m]];
          pval >>= 19;
          dst2[byte] = (uint8_t)pval;

          // move to next pixel
          byte++;
        }
        byte += stride - mb_uv_width;
      }
    }
#else
    // Normalize filter output to produce AltRef frame
    dst1 = cpi->alt_ref_buffer.y_buffer;
    stride = cpi->alt_ref_buffer.y_stride;
    byte = mb_y_offset;
    for (i = 0, k = 0; i < 16; i++) {
      for (j = 0; j < 16; j++, k++) {
        unsigned int pval = accumulator[k] + (count[k] >> 1);
        pval *= fixed_divide[count[k]];
        pval >>= 19;

        dst1[byte] = (uint8_t)pval;

        // move to next pixel
        byte++;
      }
      byte += stride - 16;
    }

    dst1 = cpi->alt_ref_buffer.u_buffer;
    dst2 = cpi->alt_ref_buffer.v_buffer;
    stride = cpi->alt_ref_buffer.uv_stride;
    byte = mb_uv_offset;
    for (i = 0, k = 256; i < mb_uv_height; i++) {
      for (j = 0; j < mb_uv_width; j++, k++) {
        int m = k + 256;

        // U
        unsigned int pval = accumulator[k] + (count[k] >> 1);
        pval *= fixed_divide[count[k]];
        pval >>= 19;
        dst1[byte] = (uint8_t)pval;

        // V
        pval = accumulator[m] + (count[m] >> 1);
        pval *= fixed_divide[count[m]];
        pval >>= 19;
        dst2[byte] = (uint8_t)pval;

        // move to next pixel
        byte++;
      }
      byte += stride - mb_uv_width;
    }
#endif  // CONFIG_VP9_HIGHBITDEPTH
    mb_y_offset += 16;
    mb_uv_offset += mb_uv_width;
  }
}

static void temporal_filter_iterate_tile_c(VP9_COMP *cpi, int tile_row,
                                           int tile_col) {
  VP9_COMMON *const cm = &cpi->common;
  const int tile_cols = 1 << cm->log2_tile_cols;
  TileInfo *tile_info =
      &cpi->tile_data[tile_row * tile_cols + tile_col].tile_info;
  const int mb_row_start = (tile_info->mi_row_start) >> 1;
  const int mb_row_end = (tile_info->mi_row_end + 1) >> 1;
  const int mb_col_start = (tile_info->mi_col_start) >> 1;
  const int mb_col_end = (tile_info->mi_col_end + 1) >> 1;
  int mb_row;

  for (mb_row = mb_row_start; mb_row < mb_row_end; mb_row++) {
    vp9_temporal_filter_iterate_row_c(cpi, &cpi->td, mb_row, mb_col_start,
                                      mb_col_end);
  }
}

static void temporal_filter_iterate_c(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  const int tile_cols = 1 << cm->log2_tile_cols;
  const int tile_rows = 1 << cm->log2_tile_rows;
  int tile_row, tile_col;
  MACROBLOCKD *mbd = &cpi->td.mb.e_mbd;
  // Save input state
  uint8_t *input_buffer[MAX_MB_PLANE];
  int i;

  for (i = 0; i < MAX_MB_PLANE; i++) input_buffer[i] = mbd->plane[i].pre[0].buf;

  vp9_init_tile_data(cpi);

  for (tile_row = 0; tile_row < tile_rows; ++tile_row) {
    for (tile_col = 0; tile_col < tile_cols; ++tile_col) {
      temporal_filter_iterate_tile_c(cpi, tile_row, tile_col);
    }
  }

  // Restore input state
  for (i = 0; i < MAX_MB_PLANE; i++) mbd->plane[i].pre[0].buf = input_buffer[i];
}

// Apply buffer limits and context specific adjustments to arnr filter.
static void adjust_arnr_filter(VP9_COMP *cpi, int distance, int group_boost,
                               int *arnr_frames, int *arnr_strength) {
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  const int frames_after_arf =
      vp9_lookahead_depth(cpi->lookahead) - distance - 1;
  int frames_fwd = (cpi->oxcf.arnr_max_frames - 1) >> 1;
  int frames_bwd;
  int q, frames, base_strength, strength;

  // Context dependent two pass adjustment to strength.
  if (oxcf->pass == 2) {
    base_strength = oxcf->arnr_strength + cpi->twopass.arnr_strength_adjustment;
    // Clip to allowed range.
    base_strength = VPXMIN(6, VPXMAX(0, base_strength));
  } else {
    base_strength = oxcf->arnr_strength;
  }

  // Define the forward and backwards filter limits for this arnr group.
  if (frames_fwd > frames_after_arf) frames_fwd = frames_after_arf;
  if (frames_fwd > distance) frames_fwd = distance;

  frames_bwd = frames_fwd;

  // For even length filter there is one more frame backward
  // than forward: e.g. len=6 ==> bbbAff, len=7 ==> bbbAfff.
  if (frames_bwd < distance) frames_bwd += (oxcf->arnr_max_frames + 1) & 0x1;

  // Set the baseline active filter size.
  frames = frames_bwd + 1 + frames_fwd;

  // Adjust the strength based on active max q.
  if (cpi->common.current_video_frame > 1)
    q = ((int)vp9_convert_qindex_to_q(cpi->rc.avg_frame_qindex[INTER_FRAME],
                                      cpi->common.bit_depth));
  else
    q = ((int)vp9_convert_qindex_to_q(cpi->rc.avg_frame_qindex[KEY_FRAME],
                                      cpi->common.bit_depth));
  if (q > 16) {
    strength = base_strength;
  } else {
    strength = base_strength - ((16 - q) / 2);
    if (strength < 0) strength = 0;
  }

  // Adjust number of frames in filter and strength based on gf boost level.
  if (frames > group_boost / 150) {
    frames = group_boost / 150;
    frames += !(frames & 1);
  }

  if (strength > group_boost / 300) {
    strength = group_boost / 300;
  }

  // Adjustments for second level arf in multi arf case.
  if (cpi->oxcf.pass == 2 && cpi->multi_arf_allowed) {
    const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
    if (gf_group->rf_level[gf_group->index] != GF_ARF_STD) {
      strength >>= 1;
    }
  }

  *arnr_frames = frames;
  *arnr_strength = strength;
}

void vp9_temporal_filter(VP9_COMP *cpi, int distance) {
  VP9_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  ARNRFilterData *arnr_filter_data = &cpi->arnr_filter_data;
  int frame;
  int frames_to_blur;
  int start_frame;
  int strength;
  int frames_to_blur_backward;
  int frames_to_blur_forward;
  struct scale_factors *sf = &arnr_filter_data->sf;
  YV12_BUFFER_CONFIG **frames = arnr_filter_data->frames;
  int rdmult;

  // Apply context specific adjustments to the arnr filter parameters.
  adjust_arnr_filter(cpi, distance, rc->gfu_boost, &frames_to_blur, &strength);
  frames_to_blur_backward = (frames_to_blur / 2);
  frames_to_blur_forward = ((frames_to_blur - 1) / 2);
  start_frame = distance + frames_to_blur_forward;

  arnr_filter_data->strength = strength;
  arnr_filter_data->frame_count = frames_to_blur;
  arnr_filter_data->alt_ref_index = frames_to_blur_backward;

  // Setup frame pointers, NULL indicates frame not included in filter.
  for (frame = 0; frame < frames_to_blur; ++frame) {
    const int which_buffer = start_frame - frame;
    struct lookahead_entry *buf =
        vp9_lookahead_peek(cpi->lookahead, which_buffer);
    frames[frames_to_blur - 1 - frame] = &buf->img;
  }

  if (frames_to_blur > 0) {
    // Setup scaling factors. Scaling on each of the arnr frames is not
    // supported.
    if (cpi->use_svc) {
      // In spatial svc the scaling factors might be less then 1/2.
      // So we will use non-normative scaling.
      int frame_used = 0;
#if CONFIG_VP9_HIGHBITDEPTH
      vp9_setup_scale_factors_for_frame(
          sf, get_frame_new_buffer(cm)->y_crop_width,
          get_frame_new_buffer(cm)->y_crop_height,
          get_frame_new_buffer(cm)->y_crop_width,
          get_frame_new_buffer(cm)->y_crop_height, cm->use_highbitdepth);
#else
      vp9_setup_scale_factors_for_frame(
          sf, get_frame_new_buffer(cm)->y_crop_width,
          get_frame_new_buffer(cm)->y_crop_height,
          get_frame_new_buffer(cm)->y_crop_width,
          get_frame_new_buffer(cm)->y_crop_height);
#endif  // CONFIG_VP9_HIGHBITDEPTH

      for (frame = 0; frame < frames_to_blur; ++frame) {
        if (cm->mi_cols * MI_SIZE != frames[frame]->y_width ||
            cm->mi_rows * MI_SIZE != frames[frame]->y_height) {
          if (vpx_realloc_frame_buffer(&cpi->svc.scaled_frames[frame_used],
                                       cm->width, cm->height, cm->subsampling_x,
                                       cm->subsampling_y,
#if CONFIG_VP9_HIGHBITDEPTH
                                       cm->use_highbitdepth,
#endif
                                       VP9_ENC_BORDER_IN_PIXELS,
                                       cm->byte_alignment, NULL, NULL, NULL)) {
            vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                               "Failed to reallocate alt_ref_buffer");
          }
          frames[frame] = vp9_scale_if_required(
              cm, frames[frame], &cpi->svc.scaled_frames[frame_used], 0,
              EIGHTTAP, 0);
          ++frame_used;
        }
      }
      cm->mi = cm->mip + cm->mi_stride + 1;
      xd->mi = cm->mi_grid_visible;
      xd->mi[0] = cm->mi;
    } else {
// ARF is produced at the native frame size and resized when coded.
#if CONFIG_VP9_HIGHBITDEPTH
      vp9_setup_scale_factors_for_frame(
          sf, frames[0]->y_crop_width, frames[0]->y_crop_height,
          frames[0]->y_crop_width, frames[0]->y_crop_height,
          cm->use_highbitdepth);
#else
      vp9_setup_scale_factors_for_frame(
          sf, frames[0]->y_crop_width, frames[0]->y_crop_height,
          frames[0]->y_crop_width, frames[0]->y_crop_height);
#endif  // CONFIG_VP9_HIGHBITDEPTH
    }
  }

  // Initialize errorperbit and sabperbit.
  rdmult = (int)vp9_compute_rd_mult_based_on_qindex(cpi, ARNR_FILT_QINDEX);
  if (rdmult < 1) rdmult = 1;
  set_error_per_bit(&cpi->td.mb, rdmult);
  vp9_initialize_me_consts(cpi, &cpi->td.mb, ARNR_FILT_QINDEX);

  if (!cpi->row_mt)
    temporal_filter_iterate_c(cpi);
  else
    vp9_temporal_filter_row_mt(cpi);
}
