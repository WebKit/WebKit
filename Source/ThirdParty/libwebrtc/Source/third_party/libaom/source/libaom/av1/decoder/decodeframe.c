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

#include <assert.h>
#include <stddef.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/aom_scale_rtcd.h"
#include "config/av1_rtcd.h"

#include "aom/aom_codec.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/binary_codes_reader.h"
#include "aom_dsp/bitreader.h"
#include "aom_dsp/bitreader_buffer.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/mem.h"
#include "aom_ports/mem_ops.h"
#include "aom_scale/aom_scale.h"
#include "aom_util/aom_thread.h"

#if CONFIG_BITSTREAM_DEBUG || CONFIG_MISMATCH_DEBUG
#include "aom_util/debug_util.h"
#endif  // CONFIG_BITSTREAM_DEBUG || CONFIG_MISMATCH_DEBUG

#include "av1/common/alloccommon.h"
#include "av1/common/cdef.h"
#include "av1/common/cfl.h"
#if CONFIG_INSPECTION
#include "av1/decoder/inspection.h"
#endif
#include "av1/common/common.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/entropymv.h"
#include "av1/common/frame_buffers.h"
#include "av1/common/idct.h"
#include "av1/common/mvref_common.h"
#include "av1/common/pred_common.h"
#include "av1/common/quant_common.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#include "av1/common/resize.h"
#include "av1/common/seg_common.h"
#include "av1/common/thread_common.h"
#include "av1/common/tile_common.h"
#include "av1/common/warped_motion.h"
#include "av1/common/obmc.h"
#include "av1/decoder/decodeframe.h"
#include "av1/decoder/decodemv.h"
#include "av1/decoder/decoder.h"
#include "av1/decoder/decodetxb.h"
#include "av1/decoder/detokenize.h"

#define ACCT_STR __func__

#define AOM_MIN_THREADS_PER_TILE 1
#define AOM_MAX_THREADS_PER_TILE 2

// This is needed by ext_tile related unit tests.
#define EXT_TILE_DEBUG 1
#define MC_TEMP_BUF_PELS                       \
  (((MAX_SB_SIZE)*2 + (AOM_INTERP_EXTEND)*2) * \
   ((MAX_SB_SIZE)*2 + (AOM_INTERP_EXTEND)*2))

// Checks that the remaining bits start with a 1 and ends with 0s.
// It consumes an additional byte, if already byte aligned before the check.
int av1_check_trailing_bits(AV1Decoder *pbi, struct aom_read_bit_buffer *rb) {
  // bit_offset is set to 0 (mod 8) when the reader is already byte aligned
  int bits_before_alignment = 8 - rb->bit_offset % 8;
  int trailing = aom_rb_read_literal(rb, bits_before_alignment);
  if (trailing != (1 << (bits_before_alignment - 1))) {
    pbi->error.error_code = AOM_CODEC_CORRUPT_FRAME;
    return -1;
  }
  return 0;
}

// Use only_chroma = 1 to only set the chroma planes
static AOM_INLINE void set_planes_to_neutral_grey(
    const SequenceHeader *const seq_params, const YV12_BUFFER_CONFIG *const buf,
    int only_chroma) {
  if (seq_params->use_highbitdepth) {
    const int val = 1 << (seq_params->bit_depth - 1);
    for (int plane = only_chroma; plane < MAX_MB_PLANE; plane++) {
      const int is_uv = plane > 0;
      uint16_t *const base = CONVERT_TO_SHORTPTR(buf->buffers[plane]);
      // Set the first row to neutral grey. Then copy the first row to all
      // subsequent rows.
      if (buf->crop_heights[is_uv] > 0) {
        aom_memset16(base, val, buf->crop_widths[is_uv]);
        for (int row_idx = 1; row_idx < buf->crop_heights[is_uv]; row_idx++) {
          memcpy(&base[row_idx * buf->strides[is_uv]], base,
                 sizeof(*base) * buf->crop_widths[is_uv]);
        }
      }
    }
  } else {
    for (int plane = only_chroma; plane < MAX_MB_PLANE; plane++) {
      const int is_uv = plane > 0;
      for (int row_idx = 0; row_idx < buf->crop_heights[is_uv]; row_idx++) {
        memset(&buf->buffers[plane][row_idx * buf->strides[is_uv]], 1 << 7,
               buf->crop_widths[is_uv]);
      }
    }
  }
}

static AOM_INLINE void loop_restoration_read_sb_coeffs(
    const AV1_COMMON *const cm, MACROBLOCKD *xd, aom_reader *const r, int plane,
    int runit_idx);

static int read_is_valid(const uint8_t *start, size_t len, const uint8_t *end) {
  return len != 0 && len <= (size_t)(end - start);
}

static TX_MODE read_tx_mode(struct aom_read_bit_buffer *rb,
                            int coded_lossless) {
  if (coded_lossless) return ONLY_4X4;
  return aom_rb_read_bit(rb) ? TX_MODE_SELECT : TX_MODE_LARGEST;
}

static REFERENCE_MODE read_frame_reference_mode(
    const AV1_COMMON *cm, struct aom_read_bit_buffer *rb) {
  if (frame_is_intra_only(cm)) {
    return SINGLE_REFERENCE;
  } else {
    return aom_rb_read_bit(rb) ? REFERENCE_MODE_SELECT : SINGLE_REFERENCE;
  }
}

static AOM_INLINE void inverse_transform_block(DecoderCodingBlock *dcb,
                                               int plane, const TX_TYPE tx_type,
                                               const TX_SIZE tx_size,
                                               uint8_t *dst, int stride,
                                               int reduced_tx_set) {
  tran_low_t *const dqcoeff = dcb->dqcoeff_block[plane] + dcb->cb_offset[plane];
  eob_info *eob_data = dcb->eob_data[plane] + dcb->txb_offset[plane];
  uint16_t scan_line = eob_data->max_scan_line;
  uint16_t eob = eob_data->eob;
  av1_inverse_transform_block(&dcb->xd, dqcoeff, plane, tx_type, tx_size, dst,
                              stride, eob, reduced_tx_set);
  memset(dqcoeff, 0, (scan_line + 1) * sizeof(dqcoeff[0]));
}

static AOM_INLINE void read_coeffs_tx_intra_block(
    const AV1_COMMON *const cm, DecoderCodingBlock *dcb, aom_reader *const r,
    const int plane, const int row, const int col, const TX_SIZE tx_size) {
  MB_MODE_INFO *mbmi = dcb->xd.mi[0];
  if (!mbmi->skip_txfm) {
#if TXCOEFF_TIMER
    struct aom_usec_timer timer;
    aom_usec_timer_start(&timer);
#endif
    av1_read_coeffs_txb_facade(cm, dcb, r, plane, row, col, tx_size);
#if TXCOEFF_TIMER
    aom_usec_timer_mark(&timer);
    const int64_t elapsed_time = aom_usec_timer_elapsed(&timer);
    cm->txcoeff_timer += elapsed_time;
    ++cm->txb_count;
#endif
  }
}

static AOM_INLINE void decode_block_void(const AV1_COMMON *const cm,
                                         DecoderCodingBlock *dcb,
                                         aom_reader *const r, const int plane,
                                         const int row, const int col,
                                         const TX_SIZE tx_size) {
  (void)cm;
  (void)dcb;
  (void)r;
  (void)plane;
  (void)row;
  (void)col;
  (void)tx_size;
}

static AOM_INLINE void predict_inter_block_void(AV1_COMMON *const cm,
                                                DecoderCodingBlock *dcb,
                                                BLOCK_SIZE bsize) {
  (void)cm;
  (void)dcb;
  (void)bsize;
}

static AOM_INLINE void cfl_store_inter_block_void(AV1_COMMON *const cm,
                                                  MACROBLOCKD *const xd) {
  (void)cm;
  (void)xd;
}

static AOM_INLINE void predict_and_reconstruct_intra_block(
    const AV1_COMMON *const cm, DecoderCodingBlock *dcb, aom_reader *const r,
    const int plane, const int row, const int col, const TX_SIZE tx_size) {
  (void)r;
  MACROBLOCKD *const xd = &dcb->xd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  PLANE_TYPE plane_type = get_plane_type(plane);

  av1_predict_intra_block_facade(cm, xd, plane, col, row, tx_size);

  if (!mbmi->skip_txfm) {
    eob_info *eob_data = dcb->eob_data[plane] + dcb->txb_offset[plane];
    if (eob_data->eob) {
      const bool reduced_tx_set_used = cm->features.reduced_tx_set_used;
      // tx_type was read out in av1_read_coeffs_txb.
      const TX_TYPE tx_type = av1_get_tx_type(xd, plane_type, row, col, tx_size,
                                              reduced_tx_set_used);
      struct macroblockd_plane *const pd = &xd->plane[plane];
      uint8_t *dst = &pd->dst.buf[(row * pd->dst.stride + col) << MI_SIZE_LOG2];
      inverse_transform_block(dcb, plane, tx_type, tx_size, dst, pd->dst.stride,
                              reduced_tx_set_used);
    }
  }
  if (plane == AOM_PLANE_Y && store_cfl_required(cm, xd)) {
    cfl_store_tx(xd, row, col, tx_size, mbmi->bsize);
  }
}

static AOM_INLINE void inverse_transform_inter_block(
    const AV1_COMMON *const cm, DecoderCodingBlock *dcb, aom_reader *const r,
    const int plane, const int blk_row, const int blk_col,
    const TX_SIZE tx_size) {
  (void)r;
  MACROBLOCKD *const xd = &dcb->xd;
  PLANE_TYPE plane_type = get_plane_type(plane);
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const bool reduced_tx_set_used = cm->features.reduced_tx_set_used;
  // tx_type was read out in av1_read_coeffs_txb.
  const TX_TYPE tx_type = av1_get_tx_type(xd, plane_type, blk_row, blk_col,
                                          tx_size, reduced_tx_set_used);

  uint8_t *dst =
      &pd->dst.buf[(blk_row * pd->dst.stride + blk_col) << MI_SIZE_LOG2];
  inverse_transform_block(dcb, plane, tx_type, tx_size, dst, pd->dst.stride,
                          reduced_tx_set_used);
#if CONFIG_MISMATCH_DEBUG
  int pixel_c, pixel_r;
  BLOCK_SIZE bsize = txsize_to_bsize[tx_size];
  int blk_w = block_size_wide[bsize];
  int blk_h = block_size_high[bsize];
  const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
  const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
  mi_to_pixel_loc(&pixel_c, &pixel_r, mi_col, mi_row, blk_col, blk_row,
                  pd->subsampling_x, pd->subsampling_y);
  mismatch_check_block_tx(dst, pd->dst.stride, cm->current_frame.order_hint,
                          plane, pixel_c, pixel_r, blk_w, blk_h,
                          xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH);
#endif
}

static AOM_INLINE void set_cb_buffer_offsets(DecoderCodingBlock *dcb,
                                             TX_SIZE tx_size, int plane) {
  dcb->cb_offset[plane] += tx_size_wide[tx_size] * tx_size_high[tx_size];
  dcb->txb_offset[plane] =
      dcb->cb_offset[plane] / (TX_SIZE_W_MIN * TX_SIZE_H_MIN);
}

static AOM_INLINE void decode_reconstruct_tx(
    AV1_COMMON *cm, ThreadData *const td, aom_reader *r,
    MB_MODE_INFO *const mbmi, int plane, BLOCK_SIZE plane_bsize, int blk_row,
    int blk_col, int block, TX_SIZE tx_size, int *eob_total) {
  DecoderCodingBlock *const dcb = &td->dcb;
  MACROBLOCKD *const xd = &dcb->xd;
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const TX_SIZE plane_tx_size =
      plane ? av1_get_max_uv_txsize(mbmi->bsize, pd->subsampling_x,
                                    pd->subsampling_y)
            : mbmi->inter_tx_size[av1_get_txb_size_index(plane_bsize, blk_row,
                                                         blk_col)];
  // Scale to match transform block unit.
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  if (tx_size == plane_tx_size || plane) {
    td->read_coeffs_tx_inter_block_visit(cm, dcb, r, plane, blk_row, blk_col,
                                         tx_size);

    td->inverse_tx_inter_block_visit(cm, dcb, r, plane, blk_row, blk_col,
                                     tx_size);
    eob_info *eob_data = dcb->eob_data[plane] + dcb->txb_offset[plane];
    *eob_total += eob_data->eob;
    set_cb_buffer_offsets(dcb, tx_size, plane);
  } else {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    assert(IMPLIES(tx_size <= TX_4X4, sub_txs == tx_size));
    assert(IMPLIES(tx_size > TX_4X4, sub_txs < tx_size));
    const int bsw = tx_size_wide_unit[sub_txs];
    const int bsh = tx_size_high_unit[sub_txs];
    const int sub_step = bsw * bsh;
    const int row_end =
        AOMMIN(tx_size_high_unit[tx_size], max_blocks_high - blk_row);
    const int col_end =
        AOMMIN(tx_size_wide_unit[tx_size], max_blocks_wide - blk_col);

    assert(bsw > 0 && bsh > 0);

    for (int row = 0; row < row_end; row += bsh) {
      const int offsetr = blk_row + row;
      for (int col = 0; col < col_end; col += bsw) {
        const int offsetc = blk_col + col;

        decode_reconstruct_tx(cm, td, r, mbmi, plane, plane_bsize, offsetr,
                              offsetc, block, sub_txs, eob_total);
        block += sub_step;
      }
    }
  }
}

static AOM_INLINE void set_offsets(AV1_COMMON *const cm, MACROBLOCKD *const xd,
                                   BLOCK_SIZE bsize, int mi_row, int mi_col,
                                   int bw, int bh, int x_mis, int y_mis) {
  const int num_planes = av1_num_planes(cm);
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  const TileInfo *const tile = &xd->tile;

  set_mi_offsets(mi_params, xd, mi_row, mi_col);
  xd->mi[0]->bsize = bsize;
#if CONFIG_RD_DEBUG
  xd->mi[0]->mi_row = mi_row;
  xd->mi[0]->mi_col = mi_col;
#endif

  assert(x_mis && y_mis);
  for (int x = 1; x < x_mis; ++x) xd->mi[x] = xd->mi[0];
  int idx = mi_params->mi_stride;
  for (int y = 1; y < y_mis; ++y) {
    memcpy(&xd->mi[idx], &xd->mi[0], x_mis * sizeof(xd->mi[0]));
    idx += mi_params->mi_stride;
  }

  set_plane_n4(xd, bw, bh, num_planes);
  set_entropy_context(xd, mi_row, mi_col, num_planes);

  // Distance of Mb to the various image edges. These are specified to 8th pel
  // as they are always compared to values that are in 1/8th pel units
  set_mi_row_col(xd, tile, mi_row, bh, mi_col, bw, mi_params->mi_rows,
                 mi_params->mi_cols);

  av1_setup_dst_planes(xd->plane, bsize, &cm->cur_frame->buf, mi_row, mi_col, 0,
                       num_planes);
}

static AOM_INLINE void decode_mbmi_block(AV1Decoder *const pbi,
                                         DecoderCodingBlock *dcb, int mi_row,
                                         int mi_col, aom_reader *r,
                                         PARTITION_TYPE partition,
                                         BLOCK_SIZE bsize) {
  AV1_COMMON *const cm = &pbi->common;
  const SequenceHeader *const seq_params = cm->seq_params;
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];
  const int x_mis = AOMMIN(bw, cm->mi_params.mi_cols - mi_col);
  const int y_mis = AOMMIN(bh, cm->mi_params.mi_rows - mi_row);
  MACROBLOCKD *const xd = &dcb->xd;

#if CONFIG_ACCOUNTING
  aom_accounting_set_context(&pbi->accounting, mi_col, mi_row);
#endif
  set_offsets(cm, xd, bsize, mi_row, mi_col, bw, bh, x_mis, y_mis);
  xd->mi[0]->partition = partition;
  av1_read_mode_info(pbi, dcb, r, x_mis, y_mis);
  if (bsize >= BLOCK_8X8 &&
      (seq_params->subsampling_x || seq_params->subsampling_y)) {
    const BLOCK_SIZE uv_subsize =
        ss_size_lookup[bsize][seq_params->subsampling_x]
                      [seq_params->subsampling_y];
    if (uv_subsize == BLOCK_INVALID)
      aom_internal_error(xd->error_info, AOM_CODEC_CORRUPT_FRAME,
                         "Invalid block size.");
  }
}

typedef struct PadBlock {
  int x0;
  int x1;
  int y0;
  int y1;
} PadBlock;

#if CONFIG_AV1_HIGHBITDEPTH
static AOM_INLINE void highbd_build_mc_border(const uint8_t *src8,
                                              int src_stride, uint8_t *dst8,
                                              int dst_stride, int x, int y,
                                              int b_w, int b_h, int w, int h) {
  // Get a pointer to the start of the real data for this row.
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  const uint16_t *ref_row = src - x - y * src_stride;

  if (y >= h)
    ref_row += (h - 1) * src_stride;
  else if (y > 0)
    ref_row += y * src_stride;

  do {
    int right = 0, copy;
    int left = x < 0 ? -x : 0;

    if (left > b_w) left = b_w;

    if (x + b_w > w) right = x + b_w - w;

    if (right > b_w) right = b_w;

    copy = b_w - left - right;

    if (left) aom_memset16(dst, ref_row[0], left);

    if (copy) memcpy(dst + left, ref_row + x + left, copy * sizeof(uint16_t));

    if (right) aom_memset16(dst + left + copy, ref_row[w - 1], right);

    dst += dst_stride;
    ++y;

    if (y > 0 && y < h) ref_row += src_stride;
  } while (--b_h);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

static AOM_INLINE void build_mc_border(const uint8_t *src, int src_stride,
                                       uint8_t *dst, int dst_stride, int x,
                                       int y, int b_w, int b_h, int w, int h) {
  // Get a pointer to the start of the real data for this row.
  const uint8_t *ref_row = src - x - y * src_stride;

  if (y >= h)
    ref_row += (h - 1) * src_stride;
  else if (y > 0)
    ref_row += y * src_stride;

  do {
    int right = 0, copy;
    int left = x < 0 ? -x : 0;

    if (left > b_w) left = b_w;

    if (x + b_w > w) right = x + b_w - w;

    if (right > b_w) right = b_w;

    copy = b_w - left - right;

    if (left) memset(dst, ref_row[0], left);

    if (copy) memcpy(dst + left, ref_row + x + left, copy);

    if (right) memset(dst + left + copy, ref_row[w - 1], right);

    dst += dst_stride;
    ++y;

    if (y > 0 && y < h) ref_row += src_stride;
  } while (--b_h);
}

static INLINE int update_extend_mc_border_params(
    const struct scale_factors *const sf, struct buf_2d *const pre_buf,
    MV32 scaled_mv, PadBlock *block, int subpel_x_mv, int subpel_y_mv,
    int do_warp, int is_intrabc, int *x_pad, int *y_pad) {
  const int is_scaled = av1_is_scaled(sf);
  // Get reference width and height.
  int frame_width = pre_buf->width;
  int frame_height = pre_buf->height;

  // Do border extension if there is motion or
  // width/height is not a multiple of 8 pixels.
  if ((!is_intrabc) && (!do_warp) &&
      (is_scaled || scaled_mv.col || scaled_mv.row || (frame_width & 0x7) ||
       (frame_height & 0x7))) {
    if (subpel_x_mv || (sf->x_step_q4 != SUBPEL_SHIFTS)) {
      block->x0 -= AOM_INTERP_EXTEND - 1;
      block->x1 += AOM_INTERP_EXTEND;
      *x_pad = 1;
    }

    if (subpel_y_mv || (sf->y_step_q4 != SUBPEL_SHIFTS)) {
      block->y0 -= AOM_INTERP_EXTEND - 1;
      block->y1 += AOM_INTERP_EXTEND;
      *y_pad = 1;
    }

    // Skip border extension if block is inside the frame.
    if (block->x0 < 0 || block->x1 > frame_width - 1 || block->y0 < 0 ||
        block->y1 > frame_height - 1) {
      return 1;
    }
  }
  return 0;
}

static INLINE void extend_mc_border(const struct scale_factors *const sf,
                                    struct buf_2d *const pre_buf,
                                    MV32 scaled_mv, PadBlock block,
                                    int subpel_x_mv, int subpel_y_mv,
                                    int do_warp, int is_intrabc, int highbd,
                                    uint8_t *mc_buf, uint8_t **pre,
                                    int *src_stride) {
  int x_pad = 0, y_pad = 0;
  if (update_extend_mc_border_params(sf, pre_buf, scaled_mv, &block,
                                     subpel_x_mv, subpel_y_mv, do_warp,
                                     is_intrabc, &x_pad, &y_pad)) {
    // Get reference block pointer.
    const uint8_t *const buf_ptr =
        pre_buf->buf0 + block.y0 * pre_buf->stride + block.x0;
    int buf_stride = pre_buf->stride;
    const int b_w = block.x1 - block.x0;
    const int b_h = block.y1 - block.y0;

#if CONFIG_AV1_HIGHBITDEPTH
    // Extend the border.
    if (highbd) {
      highbd_build_mc_border(buf_ptr, buf_stride, mc_buf, b_w, block.x0,
                             block.y0, b_w, b_h, pre_buf->width,
                             pre_buf->height);
    } else {
      build_mc_border(buf_ptr, buf_stride, mc_buf, b_w, block.x0, block.y0, b_w,
                      b_h, pre_buf->width, pre_buf->height);
    }
#else
    (void)highbd;
    build_mc_border(buf_ptr, buf_stride, mc_buf, b_w, block.x0, block.y0, b_w,
                    b_h, pre_buf->width, pre_buf->height);
#endif
    *src_stride = b_w;
    *pre = mc_buf + y_pad * (AOM_INTERP_EXTEND - 1) * b_w +
           x_pad * (AOM_INTERP_EXTEND - 1);
  }
}

static void dec_calc_subpel_params(const MV *const src_mv,
                                   InterPredParams *const inter_pred_params,
                                   const MACROBLOCKD *const xd, int mi_x,
                                   int mi_y, uint8_t **pre,
                                   SubpelParams *subpel_params, int *src_stride,
                                   PadBlock *block, MV32 *scaled_mv,
                                   int *subpel_x_mv, int *subpel_y_mv) {
  const struct scale_factors *sf = inter_pred_params->scale_factors;
  struct buf_2d *pre_buf = &inter_pred_params->ref_frame_buf;
  const int bw = inter_pred_params->block_width;
  const int bh = inter_pred_params->block_height;
  const int is_scaled = av1_is_scaled(sf);
  if (is_scaled) {
    int ssx = inter_pred_params->subsampling_x;
    int ssy = inter_pred_params->subsampling_y;
    int orig_pos_y = inter_pred_params->pix_row << SUBPEL_BITS;
    orig_pos_y += src_mv->row * (1 << (1 - ssy));
    int orig_pos_x = inter_pred_params->pix_col << SUBPEL_BITS;
    orig_pos_x += src_mv->col * (1 << (1 - ssx));
    int pos_y = sf->scale_value_y(orig_pos_y, sf);
    int pos_x = sf->scale_value_x(orig_pos_x, sf);
    pos_x += SCALE_EXTRA_OFF;
    pos_y += SCALE_EXTRA_OFF;

    const int top = -AOM_LEFT_TOP_MARGIN_SCALED(ssy);
    const int left = -AOM_LEFT_TOP_MARGIN_SCALED(ssx);
    const int bottom = (pre_buf->height + AOM_INTERP_EXTEND)
                       << SCALE_SUBPEL_BITS;
    const int right = (pre_buf->width + AOM_INTERP_EXTEND) << SCALE_SUBPEL_BITS;
    pos_y = clamp(pos_y, top, bottom);
    pos_x = clamp(pos_x, left, right);

    subpel_params->subpel_x = pos_x & SCALE_SUBPEL_MASK;
    subpel_params->subpel_y = pos_y & SCALE_SUBPEL_MASK;
    subpel_params->xs = sf->x_step_q4;
    subpel_params->ys = sf->y_step_q4;

    // Get reference block top left coordinate.
    block->x0 = pos_x >> SCALE_SUBPEL_BITS;
    block->y0 = pos_y >> SCALE_SUBPEL_BITS;

    // Get reference block bottom right coordinate.
    block->x1 =
        ((pos_x + (bw - 1) * subpel_params->xs) >> SCALE_SUBPEL_BITS) + 1;
    block->y1 =
        ((pos_y + (bh - 1) * subpel_params->ys) >> SCALE_SUBPEL_BITS) + 1;

    MV temp_mv;
    temp_mv = clamp_mv_to_umv_border_sb(xd, src_mv, bw, bh,
                                        inter_pred_params->subsampling_x,
                                        inter_pred_params->subsampling_y);
    *scaled_mv = av1_scale_mv(&temp_mv, mi_x, mi_y, sf);
    scaled_mv->row += SCALE_EXTRA_OFF;
    scaled_mv->col += SCALE_EXTRA_OFF;

    *subpel_x_mv = scaled_mv->col & SCALE_SUBPEL_MASK;
    *subpel_y_mv = scaled_mv->row & SCALE_SUBPEL_MASK;
  } else {
    // Get block position in current frame.
    int pos_x = inter_pred_params->pix_col << SUBPEL_BITS;
    int pos_y = inter_pred_params->pix_row << SUBPEL_BITS;

    const MV mv_q4 = clamp_mv_to_umv_border_sb(
        xd, src_mv, bw, bh, inter_pred_params->subsampling_x,
        inter_pred_params->subsampling_y);
    subpel_params->xs = subpel_params->ys = SCALE_SUBPEL_SHIFTS;
    subpel_params->subpel_x = (mv_q4.col & SUBPEL_MASK) << SCALE_EXTRA_BITS;
    subpel_params->subpel_y = (mv_q4.row & SUBPEL_MASK) << SCALE_EXTRA_BITS;

    // Get reference block top left coordinate.
    pos_x += mv_q4.col;
    pos_y += mv_q4.row;
    block->x0 = pos_x >> SUBPEL_BITS;
    block->y0 = pos_y >> SUBPEL_BITS;

    // Get reference block bottom right coordinate.
    block->x1 = (pos_x >> SUBPEL_BITS) + (bw - 1) + 1;
    block->y1 = (pos_y >> SUBPEL_BITS) + (bh - 1) + 1;

    scaled_mv->row = mv_q4.row;
    scaled_mv->col = mv_q4.col;
    *subpel_x_mv = scaled_mv->col & SUBPEL_MASK;
    *subpel_y_mv = scaled_mv->row & SUBPEL_MASK;
  }
  *pre = pre_buf->buf0 + block->y0 * pre_buf->stride + block->x0;
  *src_stride = pre_buf->stride;
}

static void dec_calc_subpel_params_and_extend(
    const MV *const src_mv, InterPredParams *const inter_pred_params,
    MACROBLOCKD *const xd, int mi_x, int mi_y, int ref, uint8_t **mc_buf,
    uint8_t **pre, SubpelParams *subpel_params, int *src_stride) {
  PadBlock block;
  MV32 scaled_mv;
  int subpel_x_mv, subpel_y_mv;
  dec_calc_subpel_params(src_mv, inter_pred_params, xd, mi_x, mi_y, pre,
                         subpel_params, src_stride, &block, &scaled_mv,
                         &subpel_x_mv, &subpel_y_mv);
  extend_mc_border(
      inter_pred_params->scale_factors, &inter_pred_params->ref_frame_buf,
      scaled_mv, block, subpel_x_mv, subpel_y_mv,
      inter_pred_params->mode == WARP_PRED, inter_pred_params->is_intrabc,
      inter_pred_params->use_hbd_buf, mc_buf[ref], pre, src_stride);
}

static void dec_build_inter_predictors(const AV1_COMMON *cm,
                                       DecoderCodingBlock *dcb, int plane,
                                       const MB_MODE_INFO *mi,
                                       int build_for_obmc, int bw, int bh,
                                       int mi_x, int mi_y) {
  av1_build_inter_predictors(cm, &dcb->xd, plane, mi, build_for_obmc, bw, bh,
                             mi_x, mi_y, dcb->mc_buf,
                             dec_calc_subpel_params_and_extend);
}

static AOM_INLINE void dec_build_inter_predictor(const AV1_COMMON *cm,
                                                 DecoderCodingBlock *dcb,
                                                 int mi_row, int mi_col,
                                                 BLOCK_SIZE bsize) {
  MACROBLOCKD *const xd = &dcb->xd;
  const int num_planes = av1_num_planes(cm);
  for (int plane = 0; plane < num_planes; ++plane) {
    if (plane && !xd->is_chroma_ref) break;
    const int mi_x = mi_col * MI_SIZE;
    const int mi_y = mi_row * MI_SIZE;
    dec_build_inter_predictors(cm, dcb, plane, xd->mi[0], 0,
                               xd->plane[plane].width, xd->plane[plane].height,
                               mi_x, mi_y);
    if (is_interintra_pred(xd->mi[0])) {
      BUFFER_SET ctx = { { xd->plane[0].dst.buf, xd->plane[1].dst.buf,
                           xd->plane[2].dst.buf },
                         { xd->plane[0].dst.stride, xd->plane[1].dst.stride,
                           xd->plane[2].dst.stride } };
      av1_build_interintra_predictor(cm, xd, xd->plane[plane].dst.buf,
                                     xd->plane[plane].dst.stride, &ctx, plane,
                                     bsize);
    }
  }
}

static INLINE void dec_build_prediction_by_above_pred(
    MACROBLOCKD *const xd, int rel_mi_row, int rel_mi_col, uint8_t op_mi_size,
    int dir, MB_MODE_INFO *above_mbmi, void *fun_ctxt, const int num_planes) {
  struct build_prediction_ctxt *ctxt = (struct build_prediction_ctxt *)fun_ctxt;
  const int above_mi_col = xd->mi_col + rel_mi_col;
  int mi_x, mi_y;
  MB_MODE_INFO backup_mbmi = *above_mbmi;

  (void)rel_mi_row;
  (void)dir;

  av1_setup_build_prediction_by_above_pred(xd, rel_mi_col, op_mi_size,
                                           &backup_mbmi, ctxt, num_planes);
  mi_x = above_mi_col << MI_SIZE_LOG2;
  mi_y = xd->mi_row << MI_SIZE_LOG2;

  const BLOCK_SIZE bsize = xd->mi[0]->bsize;

  for (int j = 0; j < num_planes; ++j) {
    const struct macroblockd_plane *pd = &xd->plane[j];
    int bw = (op_mi_size * MI_SIZE) >> pd->subsampling_x;
    int bh = clamp(block_size_high[bsize] >> (pd->subsampling_y + 1), 4,
                   block_size_high[BLOCK_64X64] >> (pd->subsampling_y + 1));

    if (av1_skip_u4x4_pred_in_obmc(bsize, pd, 0)) continue;
    dec_build_inter_predictors(ctxt->cm, (DecoderCodingBlock *)ctxt->dcb, j,
                               &backup_mbmi, 1, bw, bh, mi_x, mi_y);
  }
}

static AOM_INLINE void dec_build_prediction_by_above_preds(
    const AV1_COMMON *cm, DecoderCodingBlock *dcb,
    uint8_t *tmp_buf[MAX_MB_PLANE], int tmp_width[MAX_MB_PLANE],
    int tmp_height[MAX_MB_PLANE], int tmp_stride[MAX_MB_PLANE]) {
  MACROBLOCKD *const xd = &dcb->xd;
  if (!xd->up_available) return;

  // Adjust mb_to_bottom_edge to have the correct value for the OBMC
  // prediction block. This is half the height of the original block,
  // except for 128-wide blocks, where we only use a height of 32.
  const int this_height = xd->height * MI_SIZE;
  const int pred_height = AOMMIN(this_height / 2, 32);
  xd->mb_to_bottom_edge += GET_MV_SUBPEL(this_height - pred_height);
  struct build_prediction_ctxt ctxt = {
    cm, tmp_buf, tmp_width, tmp_height, tmp_stride, xd->mb_to_right_edge, dcb
  };
  const BLOCK_SIZE bsize = xd->mi[0]->bsize;
  foreach_overlappable_nb_above(cm, xd,
                                max_neighbor_obmc[mi_size_wide_log2[bsize]],
                                dec_build_prediction_by_above_pred, &ctxt);

  xd->mb_to_left_edge = -GET_MV_SUBPEL(xd->mi_col * MI_SIZE);
  xd->mb_to_right_edge = ctxt.mb_to_far_edge;
  xd->mb_to_bottom_edge -= GET_MV_SUBPEL(this_height - pred_height);
}

static INLINE void dec_build_prediction_by_left_pred(
    MACROBLOCKD *const xd, int rel_mi_row, int rel_mi_col, uint8_t op_mi_size,
    int dir, MB_MODE_INFO *left_mbmi, void *fun_ctxt, const int num_planes) {
  struct build_prediction_ctxt *ctxt = (struct build_prediction_ctxt *)fun_ctxt;
  const int left_mi_row = xd->mi_row + rel_mi_row;
  int mi_x, mi_y;
  MB_MODE_INFO backup_mbmi = *left_mbmi;

  (void)rel_mi_col;
  (void)dir;

  av1_setup_build_prediction_by_left_pred(xd, rel_mi_row, op_mi_size,
                                          &backup_mbmi, ctxt, num_planes);
  mi_x = xd->mi_col << MI_SIZE_LOG2;
  mi_y = left_mi_row << MI_SIZE_LOG2;
  const BLOCK_SIZE bsize = xd->mi[0]->bsize;

  for (int j = 0; j < num_planes; ++j) {
    const struct macroblockd_plane *pd = &xd->plane[j];
    int bw = clamp(block_size_wide[bsize] >> (pd->subsampling_x + 1), 4,
                   block_size_wide[BLOCK_64X64] >> (pd->subsampling_x + 1));
    int bh = (op_mi_size << MI_SIZE_LOG2) >> pd->subsampling_y;

    if (av1_skip_u4x4_pred_in_obmc(bsize, pd, 1)) continue;
    dec_build_inter_predictors(ctxt->cm, (DecoderCodingBlock *)ctxt->dcb, j,
                               &backup_mbmi, 1, bw, bh, mi_x, mi_y);
  }
}

static AOM_INLINE void dec_build_prediction_by_left_preds(
    const AV1_COMMON *cm, DecoderCodingBlock *dcb,
    uint8_t *tmp_buf[MAX_MB_PLANE], int tmp_width[MAX_MB_PLANE],
    int tmp_height[MAX_MB_PLANE], int tmp_stride[MAX_MB_PLANE]) {
  MACROBLOCKD *const xd = &dcb->xd;
  if (!xd->left_available) return;

  // Adjust mb_to_right_edge to have the correct value for the OBMC
  // prediction block. This is half the width of the original block,
  // except for 128-wide blocks, where we only use a width of 32.
  const int this_width = xd->width * MI_SIZE;
  const int pred_width = AOMMIN(this_width / 2, 32);
  xd->mb_to_right_edge += GET_MV_SUBPEL(this_width - pred_width);

  struct build_prediction_ctxt ctxt = {
    cm, tmp_buf, tmp_width, tmp_height, tmp_stride, xd->mb_to_bottom_edge, dcb
  };
  const BLOCK_SIZE bsize = xd->mi[0]->bsize;
  foreach_overlappable_nb_left(cm, xd,
                               max_neighbor_obmc[mi_size_high_log2[bsize]],
                               dec_build_prediction_by_left_pred, &ctxt);

  xd->mb_to_top_edge = -GET_MV_SUBPEL(xd->mi_row * MI_SIZE);
  xd->mb_to_right_edge -= GET_MV_SUBPEL(this_width - pred_width);
  xd->mb_to_bottom_edge = ctxt.mb_to_far_edge;
}

static AOM_INLINE void dec_build_obmc_inter_predictors_sb(
    const AV1_COMMON *cm, DecoderCodingBlock *dcb) {
  const int num_planes = av1_num_planes(cm);
  uint8_t *dst_buf1[MAX_MB_PLANE], *dst_buf2[MAX_MB_PLANE];
  int dst_stride1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_stride2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_width1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_width2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_height1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_height2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };

  MACROBLOCKD *const xd = &dcb->xd;
  av1_setup_obmc_dst_bufs(xd, dst_buf1, dst_buf2);

  dec_build_prediction_by_above_preds(cm, dcb, dst_buf1, dst_width1,
                                      dst_height1, dst_stride1);
  dec_build_prediction_by_left_preds(cm, dcb, dst_buf2, dst_width2, dst_height2,
                                     dst_stride2);
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  av1_setup_dst_planes(xd->plane, xd->mi[0]->bsize, &cm->cur_frame->buf, mi_row,
                       mi_col, 0, num_planes);
  av1_build_obmc_inter_prediction(cm, xd, dst_buf1, dst_stride1, dst_buf2,
                                  dst_stride2);
}

static AOM_INLINE void cfl_store_inter_block(AV1_COMMON *const cm,
                                             MACROBLOCKD *const xd) {
  MB_MODE_INFO *mbmi = xd->mi[0];
  if (store_cfl_required(cm, xd)) {
    cfl_store_block(xd, mbmi->bsize, mbmi->tx_size);
  }
}

static AOM_INLINE void predict_inter_block(AV1_COMMON *const cm,
                                           DecoderCodingBlock *dcb,
                                           BLOCK_SIZE bsize) {
  MACROBLOCKD *const xd = &dcb->xd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const int num_planes = av1_num_planes(cm);
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  for (int ref = 0; ref < 1 + has_second_ref(mbmi); ++ref) {
    const MV_REFERENCE_FRAME frame = mbmi->ref_frame[ref];
    if (frame < LAST_FRAME) {
      assert(is_intrabc_block(mbmi));
      assert(frame == INTRA_FRAME);
      assert(ref == 0);
    } else {
      const RefCntBuffer *ref_buf = get_ref_frame_buf(cm, frame);
      const struct scale_factors *ref_scale_factors =
          get_ref_scale_factors_const(cm, frame);

      xd->block_ref_scale_factors[ref] = ref_scale_factors;
      av1_setup_pre_planes(xd, ref, &ref_buf->buf, mi_row, mi_col,
                           ref_scale_factors, num_planes);
    }
  }

  dec_build_inter_predictor(cm, dcb, mi_row, mi_col, bsize);
  if (mbmi->motion_mode == OBMC_CAUSAL) {
    dec_build_obmc_inter_predictors_sb(cm, dcb);
  }
#if CONFIG_MISMATCH_DEBUG
  for (int plane = 0; plane < num_planes; ++plane) {
    const struct macroblockd_plane *pd = &xd->plane[plane];
    int pixel_c, pixel_r;
    mi_to_pixel_loc(&pixel_c, &pixel_r, mi_col, mi_row, 0, 0, pd->subsampling_x,
                    pd->subsampling_y);
    if (!is_chroma_reference(mi_row, mi_col, bsize, pd->subsampling_x,
                             pd->subsampling_y))
      continue;
    mismatch_check_block_pre(pd->dst.buf, pd->dst.stride,
                             cm->current_frame.order_hint, plane, pixel_c,
                             pixel_r, pd->width, pd->height,
                             xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH);
  }
#endif
}

static AOM_INLINE void set_color_index_map_offset(MACROBLOCKD *const xd,
                                                  int plane, aom_reader *r) {
  (void)r;
  Av1ColorMapParam params;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  av1_get_block_dimensions(mbmi->bsize, plane, xd, &params.plane_width,
                           &params.plane_height, NULL, NULL);
  xd->color_index_map_offset[plane] += params.plane_width * params.plane_height;
}

static AOM_INLINE void decode_token_recon_block(AV1Decoder *const pbi,
                                                ThreadData *const td,
                                                aom_reader *r,
                                                BLOCK_SIZE bsize) {
  AV1_COMMON *const cm = &pbi->common;
  DecoderCodingBlock *const dcb = &td->dcb;
  MACROBLOCKD *const xd = &dcb->xd;
  const int num_planes = av1_num_planes(cm);
  MB_MODE_INFO *mbmi = xd->mi[0];

  if (!is_inter_block(mbmi)) {
    int row, col;
    assert(bsize == get_plane_block_size(bsize, xd->plane[0].subsampling_x,
                                         xd->plane[0].subsampling_y));
    const int max_blocks_wide = max_block_wide(xd, bsize, 0);
    const int max_blocks_high = max_block_high(xd, bsize, 0);
    const BLOCK_SIZE max_unit_bsize = BLOCK_64X64;
    int mu_blocks_wide = mi_size_wide[max_unit_bsize];
    int mu_blocks_high = mi_size_high[max_unit_bsize];
    mu_blocks_wide = AOMMIN(max_blocks_wide, mu_blocks_wide);
    mu_blocks_high = AOMMIN(max_blocks_high, mu_blocks_high);

    for (row = 0; row < max_blocks_high; row += mu_blocks_high) {
      for (col = 0; col < max_blocks_wide; col += mu_blocks_wide) {
        for (int plane = 0; plane < num_planes; ++plane) {
          if (plane && !xd->is_chroma_ref) break;
          const struct macroblockd_plane *const pd = &xd->plane[plane];
          const TX_SIZE tx_size = av1_get_tx_size(plane, xd);
          const int stepr = tx_size_high_unit[tx_size];
          const int stepc = tx_size_wide_unit[tx_size];

          const int unit_height = ROUND_POWER_OF_TWO(
              AOMMIN(mu_blocks_high + row, max_blocks_high), pd->subsampling_y);
          const int unit_width = ROUND_POWER_OF_TWO(
              AOMMIN(mu_blocks_wide + col, max_blocks_wide), pd->subsampling_x);

          for (int blk_row = row >> pd->subsampling_y; blk_row < unit_height;
               blk_row += stepr) {
            for (int blk_col = col >> pd->subsampling_x; blk_col < unit_width;
                 blk_col += stepc) {
              td->read_coeffs_tx_intra_block_visit(cm, dcb, r, plane, blk_row,
                                                   blk_col, tx_size);
              td->predict_and_recon_intra_block_visit(
                  cm, dcb, r, plane, blk_row, blk_col, tx_size);
              set_cb_buffer_offsets(dcb, tx_size, plane);
            }
          }
        }
      }
    }
  } else {
    td->predict_inter_block_visit(cm, dcb, bsize);
    // Reconstruction
    if (!mbmi->skip_txfm) {
      int eobtotal = 0;

      const int max_blocks_wide = max_block_wide(xd, bsize, 0);
      const int max_blocks_high = max_block_high(xd, bsize, 0);
      int row, col;

      const BLOCK_SIZE max_unit_bsize = BLOCK_64X64;
      assert(max_unit_bsize ==
             get_plane_block_size(BLOCK_64X64, xd->plane[0].subsampling_x,
                                  xd->plane[0].subsampling_y));
      int mu_blocks_wide = mi_size_wide[max_unit_bsize];
      int mu_blocks_high = mi_size_high[max_unit_bsize];

      mu_blocks_wide = AOMMIN(max_blocks_wide, mu_blocks_wide);
      mu_blocks_high = AOMMIN(max_blocks_high, mu_blocks_high);

      for (row = 0; row < max_blocks_high; row += mu_blocks_high) {
        for (col = 0; col < max_blocks_wide; col += mu_blocks_wide) {
          for (int plane = 0; plane < num_planes; ++plane) {
            if (plane && !xd->is_chroma_ref) break;
            const struct macroblockd_plane *const pd = &xd->plane[plane];
            const int ss_x = pd->subsampling_x;
            const int ss_y = pd->subsampling_y;
            const BLOCK_SIZE plane_bsize =
                get_plane_block_size(bsize, ss_x, ss_y);
            const TX_SIZE max_tx_size =
                get_vartx_max_txsize(xd, plane_bsize, plane);
            const int bh_var_tx = tx_size_high_unit[max_tx_size];
            const int bw_var_tx = tx_size_wide_unit[max_tx_size];
            int block = 0;
            int step =
                tx_size_wide_unit[max_tx_size] * tx_size_high_unit[max_tx_size];
            int blk_row, blk_col;
            const int unit_height = ROUND_POWER_OF_TWO(
                AOMMIN(mu_blocks_high + row, max_blocks_high), ss_y);
            const int unit_width = ROUND_POWER_OF_TWO(
                AOMMIN(mu_blocks_wide + col, max_blocks_wide), ss_x);

            for (blk_row = row >> ss_y; blk_row < unit_height;
                 blk_row += bh_var_tx) {
              for (blk_col = col >> ss_x; blk_col < unit_width;
                   blk_col += bw_var_tx) {
                decode_reconstruct_tx(cm, td, r, mbmi, plane, plane_bsize,
                                      blk_row, blk_col, block, max_tx_size,
                                      &eobtotal);
                block += step;
              }
            }
          }
        }
      }
    }
    td->cfl_store_inter_block_visit(cm, xd);
  }

  av1_visit_palette(pbi, xd, r, set_color_index_map_offset);
}

static AOM_INLINE void set_inter_tx_size(MB_MODE_INFO *mbmi, int stride_log2,
                                         int tx_w_log2, int tx_h_log2,
                                         int min_txs, int split_size, int txs,
                                         int blk_row, int blk_col) {
  for (int idy = 0; idy < tx_size_high_unit[split_size];
       idy += tx_size_high_unit[min_txs]) {
    for (int idx = 0; idx < tx_size_wide_unit[split_size];
         idx += tx_size_wide_unit[min_txs]) {
      const int index = (((blk_row + idy) >> tx_h_log2) << stride_log2) +
                        ((blk_col + idx) >> tx_w_log2);
      mbmi->inter_tx_size[index] = txs;
    }
  }
}

static AOM_INLINE void read_tx_size_vartx(MACROBLOCKD *xd, MB_MODE_INFO *mbmi,
                                          TX_SIZE tx_size, int depth,
                                          int blk_row, int blk_col,
                                          aom_reader *r) {
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  int is_split = 0;
  const BLOCK_SIZE bsize = mbmi->bsize;
  const int max_blocks_high = max_block_high(xd, bsize, 0);
  const int max_blocks_wide = max_block_wide(xd, bsize, 0);
  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;
  assert(tx_size > TX_4X4);
  TX_SIZE txs = max_txsize_rect_lookup[bsize];
  for (int level = 0; level < MAX_VARTX_DEPTH - 1; ++level)
    txs = sub_tx_size_map[txs];
  const int tx_w_log2 = tx_size_wide_log2[txs] - MI_SIZE_LOG2;
  const int tx_h_log2 = tx_size_high_log2[txs] - MI_SIZE_LOG2;
  const int bw_log2 = mi_size_wide_log2[bsize];
  const int stride_log2 = bw_log2 - tx_w_log2;

  if (depth == MAX_VARTX_DEPTH) {
    set_inter_tx_size(mbmi, stride_log2, tx_w_log2, tx_h_log2, txs, tx_size,
                      tx_size, blk_row, blk_col);
    mbmi->tx_size = tx_size;
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);
    return;
  }

  const int ctx = txfm_partition_context(xd->above_txfm_context + blk_col,
                                         xd->left_txfm_context + blk_row,
                                         mbmi->bsize, tx_size);
  is_split = aom_read_symbol(r, ec_ctx->txfm_partition_cdf[ctx], 2, ACCT_STR);

  if (is_split) {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsw = tx_size_wide_unit[sub_txs];
    const int bsh = tx_size_high_unit[sub_txs];

    if (sub_txs == TX_4X4) {
      set_inter_tx_size(mbmi, stride_log2, tx_w_log2, tx_h_log2, txs, tx_size,
                        sub_txs, blk_row, blk_col);
      mbmi->tx_size = sub_txs;
      txfm_partition_update(xd->above_txfm_context + blk_col,
                            xd->left_txfm_context + blk_row, sub_txs, tx_size);
      return;
    }

    assert(bsw > 0 && bsh > 0);
    for (int row = 0; row < tx_size_high_unit[tx_size]; row += bsh) {
      for (int col = 0; col < tx_size_wide_unit[tx_size]; col += bsw) {
        int offsetr = blk_row + row;
        int offsetc = blk_col + col;
        read_tx_size_vartx(xd, mbmi, sub_txs, depth + 1, offsetr, offsetc, r);
      }
    }
  } else {
    set_inter_tx_size(mbmi, stride_log2, tx_w_log2, tx_h_log2, txs, tx_size,
                      tx_size, blk_row, blk_col);
    mbmi->tx_size = tx_size;
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);
  }
}

static TX_SIZE read_selected_tx_size(const MACROBLOCKD *const xd,
                                     aom_reader *r) {
  // TODO(debargha): Clean up the logic here. This function should only
  // be called for intra.
  const BLOCK_SIZE bsize = xd->mi[0]->bsize;
  const int32_t tx_size_cat = bsize_to_tx_size_cat(bsize);
  const int max_depths = bsize_to_max_depth(bsize);
  const int ctx = get_tx_size_context(xd);
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  const int depth = aom_read_symbol(r, ec_ctx->tx_size_cdf[tx_size_cat][ctx],
                                    max_depths + 1, ACCT_STR);
  assert(depth >= 0 && depth <= max_depths);
  const TX_SIZE tx_size = depth_to_tx_size(depth, bsize);
  return tx_size;
}

static TX_SIZE read_tx_size(const MACROBLOCKD *const xd, TX_MODE tx_mode,
                            int is_inter, int allow_select_inter,
                            aom_reader *r) {
  const BLOCK_SIZE bsize = xd->mi[0]->bsize;
  if (xd->lossless[xd->mi[0]->segment_id]) return TX_4X4;

  if (block_signals_txsize(bsize)) {
    if ((!is_inter || allow_select_inter) && tx_mode == TX_MODE_SELECT) {
      const TX_SIZE coded_tx_size = read_selected_tx_size(xd, r);
      return coded_tx_size;
    } else {
      return tx_size_from_tx_mode(bsize, tx_mode);
    }
  } else {
    assert(IMPLIES(tx_mode == ONLY_4X4, bsize == BLOCK_4X4));
    return max_txsize_rect_lookup[bsize];
  }
}

static AOM_INLINE void parse_decode_block(AV1Decoder *const pbi,
                                          ThreadData *const td, int mi_row,
                                          int mi_col, aom_reader *r,
                                          PARTITION_TYPE partition,
                                          BLOCK_SIZE bsize) {
  DecoderCodingBlock *const dcb = &td->dcb;
  MACROBLOCKD *const xd = &dcb->xd;
  decode_mbmi_block(pbi, dcb, mi_row, mi_col, r, partition, bsize);

  av1_visit_palette(pbi, xd, r, av1_decode_palette_tokens);

  AV1_COMMON *cm = &pbi->common;
  const int num_planes = av1_num_planes(cm);
  MB_MODE_INFO *mbmi = xd->mi[0];
  int inter_block_tx = is_inter_block(mbmi) || is_intrabc_block(mbmi);
  if (cm->features.tx_mode == TX_MODE_SELECT && block_signals_txsize(bsize) &&
      !mbmi->skip_txfm && inter_block_tx && !xd->lossless[mbmi->segment_id]) {
    const TX_SIZE max_tx_size = max_txsize_rect_lookup[bsize];
    const int bh = tx_size_high_unit[max_tx_size];
    const int bw = tx_size_wide_unit[max_tx_size];
    const int width = mi_size_wide[bsize];
    const int height = mi_size_high[bsize];

    for (int idy = 0; idy < height; idy += bh)
      for (int idx = 0; idx < width; idx += bw)
        read_tx_size_vartx(xd, mbmi, max_tx_size, 0, idy, idx, r);
  } else {
    mbmi->tx_size = read_tx_size(xd, cm->features.tx_mode, inter_block_tx,
                                 !mbmi->skip_txfm, r);
    if (inter_block_tx)
      memset(mbmi->inter_tx_size, mbmi->tx_size, sizeof(mbmi->inter_tx_size));
    set_txfm_ctxs(mbmi->tx_size, xd->width, xd->height,
                  mbmi->skip_txfm && is_inter_block(mbmi), xd);
  }

  if (cm->delta_q_info.delta_q_present_flag) {
    for (int i = 0; i < MAX_SEGMENTS; i++) {
      const int current_qindex =
          av1_get_qindex(&cm->seg, i, xd->current_base_qindex);
      const CommonQuantParams *const quant_params = &cm->quant_params;
      for (int j = 0; j < num_planes; ++j) {
        const int dc_delta_q = j == 0 ? quant_params->y_dc_delta_q
                                      : (j == 1 ? quant_params->u_dc_delta_q
                                                : quant_params->v_dc_delta_q);
        const int ac_delta_q = j == 0 ? 0
                                      : (j == 1 ? quant_params->u_ac_delta_q
                                                : quant_params->v_ac_delta_q);
        xd->plane[j].seg_dequant_QTX[i][0] = av1_dc_quant_QTX(
            current_qindex, dc_delta_q, cm->seq_params->bit_depth);
        xd->plane[j].seg_dequant_QTX[i][1] = av1_ac_quant_QTX(
            current_qindex, ac_delta_q, cm->seq_params->bit_depth);
      }
    }
  }
  if (mbmi->skip_txfm) av1_reset_entropy_context(xd, bsize, num_planes);

  decode_token_recon_block(pbi, td, r, bsize);
}

static AOM_INLINE void set_offsets_for_pred_and_recon(AV1Decoder *const pbi,
                                                      ThreadData *const td,
                                                      int mi_row, int mi_col,
                                                      BLOCK_SIZE bsize) {
  AV1_COMMON *const cm = &pbi->common;
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  DecoderCodingBlock *const dcb = &td->dcb;
  MACROBLOCKD *const xd = &dcb->xd;
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];
  const int num_planes = av1_num_planes(cm);

  const int offset = mi_row * mi_params->mi_stride + mi_col;
  const TileInfo *const tile = &xd->tile;

  xd->mi = mi_params->mi_grid_base + offset;
  xd->tx_type_map =
      &mi_params->tx_type_map[mi_row * mi_params->mi_stride + mi_col];
  xd->tx_type_map_stride = mi_params->mi_stride;

  set_plane_n4(xd, bw, bh, num_planes);

  // Distance of Mb to the various image edges. These are specified to 8th pel
  // as they are always compared to values that are in 1/8th pel units
  set_mi_row_col(xd, tile, mi_row, bh, mi_col, bw, mi_params->mi_rows,
                 mi_params->mi_cols);

  av1_setup_dst_planes(xd->plane, bsize, &cm->cur_frame->buf, mi_row, mi_col, 0,
                       num_planes);
}

static AOM_INLINE void decode_block(AV1Decoder *const pbi, ThreadData *const td,
                                    int mi_row, int mi_col, aom_reader *r,
                                    PARTITION_TYPE partition,
                                    BLOCK_SIZE bsize) {
  (void)partition;
  set_offsets_for_pred_and_recon(pbi, td, mi_row, mi_col, bsize);
  decode_token_recon_block(pbi, td, r, bsize);
}

static PARTITION_TYPE read_partition(MACROBLOCKD *xd, int mi_row, int mi_col,
                                     aom_reader *r, int has_rows, int has_cols,
                                     BLOCK_SIZE bsize) {
  const int ctx = partition_plane_context(xd, mi_row, mi_col, bsize);
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;

  if (!has_rows && !has_cols) return PARTITION_SPLIT;

  assert(ctx >= 0);
  aom_cdf_prob *partition_cdf = ec_ctx->partition_cdf[ctx];
  if (has_rows && has_cols) {
    return (PARTITION_TYPE)aom_read_symbol(
        r, partition_cdf, partition_cdf_length(bsize), ACCT_STR);
  } else if (!has_rows && has_cols) {
    assert(bsize > BLOCK_8X8);
    aom_cdf_prob cdf[2];
    partition_gather_vert_alike(cdf, partition_cdf, bsize);
    assert(cdf[1] == AOM_ICDF(CDF_PROB_TOP));
    return aom_read_cdf(r, cdf, 2, ACCT_STR) ? PARTITION_SPLIT : PARTITION_HORZ;
  } else {
    assert(has_rows && !has_cols);
    assert(bsize > BLOCK_8X8);
    aom_cdf_prob cdf[2];
    partition_gather_horz_alike(cdf, partition_cdf, bsize);
    assert(cdf[1] == AOM_ICDF(CDF_PROB_TOP));
    return aom_read_cdf(r, cdf, 2, ACCT_STR) ? PARTITION_SPLIT : PARTITION_VERT;
  }
}

// TODO(slavarnway): eliminate bsize and subsize in future commits
static AOM_INLINE void decode_partition(AV1Decoder *const pbi,
                                        ThreadData *const td, int mi_row,
                                        int mi_col, aom_reader *reader,
                                        BLOCK_SIZE bsize,
                                        int parse_decode_flag) {
  assert(bsize < BLOCK_SIZES_ALL);
  AV1_COMMON *const cm = &pbi->common;
  DecoderCodingBlock *const dcb = &td->dcb;
  MACROBLOCKD *const xd = &dcb->xd;
  const int bw = mi_size_wide[bsize];
  const int hbs = bw >> 1;
  PARTITION_TYPE partition;
  BLOCK_SIZE subsize;
  const int quarter_step = bw / 4;
  BLOCK_SIZE bsize2 = get_partition_subsize(bsize, PARTITION_SPLIT);
  const int has_rows = (mi_row + hbs) < cm->mi_params.mi_rows;
  const int has_cols = (mi_col + hbs) < cm->mi_params.mi_cols;

  if (mi_row >= cm->mi_params.mi_rows || mi_col >= cm->mi_params.mi_cols)
    return;

  // parse_decode_flag takes the following values :
  // 01 - do parse only
  // 10 - do decode only
  // 11 - do parse and decode
  static const block_visitor_fn_t block_visit[4] = { NULL, parse_decode_block,
                                                     decode_block,
                                                     parse_decode_block };

  if (parse_decode_flag & 1) {
    const int num_planes = av1_num_planes(cm);
    for (int plane = 0; plane < num_planes; ++plane) {
      int rcol0, rcol1, rrow0, rrow1;
      if (av1_loop_restoration_corners_in_sb(cm, plane, mi_row, mi_col, bsize,
                                             &rcol0, &rcol1, &rrow0, &rrow1)) {
        const int rstride = cm->rst_info[plane].horz_units_per_tile;
        for (int rrow = rrow0; rrow < rrow1; ++rrow) {
          for (int rcol = rcol0; rcol < rcol1; ++rcol) {
            const int runit_idx = rcol + rrow * rstride;
            loop_restoration_read_sb_coeffs(cm, xd, reader, plane, runit_idx);
          }
        }
      }
    }

    partition = (bsize < BLOCK_8X8) ? PARTITION_NONE
                                    : read_partition(xd, mi_row, mi_col, reader,
                                                     has_rows, has_cols, bsize);
  } else {
    partition = get_partition(cm, mi_row, mi_col, bsize);
  }
  subsize = get_partition_subsize(bsize, partition);
  if (subsize == BLOCK_INVALID) {
    // When an internal error occurs ensure that xd->mi_row is set appropriately
    // w.r.t. current tile, which is used to signal processing of current row is
    // done.
    xd->mi_row = mi_row;
    aom_internal_error(xd->error_info, AOM_CODEC_CORRUPT_FRAME,
                       "Partition is invalid for block size %dx%d",
                       block_size_wide[bsize], block_size_high[bsize]);
  }
  // Check the bitstream is conformant: if there is subsampling on the
  // chroma planes, subsize must subsample to a valid block size.
  const struct macroblockd_plane *const pd_u = &xd->plane[1];
  if (get_plane_block_size(subsize, pd_u->subsampling_x, pd_u->subsampling_y) ==
      BLOCK_INVALID) {
    // When an internal error occurs ensure that xd->mi_row is set appropriately
    // w.r.t. current tile, which is used to signal processing of current row is
    // done.
    xd->mi_row = mi_row;
    aom_internal_error(xd->error_info, AOM_CODEC_CORRUPT_FRAME,
                       "Block size %dx%d invalid with this subsampling mode",
                       block_size_wide[subsize], block_size_high[subsize]);
  }

#define DEC_BLOCK_STX_ARG
#define DEC_BLOCK_EPT_ARG partition,
#define DEC_BLOCK(db_r, db_c, db_subsize)                                  \
  block_visit[parse_decode_flag](pbi, td, DEC_BLOCK_STX_ARG(db_r), (db_c), \
                                 reader, DEC_BLOCK_EPT_ARG(db_subsize))
#define DEC_PARTITION(db_r, db_c, db_subsize)                        \
  decode_partition(pbi, td, DEC_BLOCK_STX_ARG(db_r), (db_c), reader, \
                   (db_subsize), parse_decode_flag)

  switch (partition) {
    case PARTITION_NONE: DEC_BLOCK(mi_row, mi_col, subsize); break;
    case PARTITION_HORZ:
      DEC_BLOCK(mi_row, mi_col, subsize);
      if (has_rows) DEC_BLOCK(mi_row + hbs, mi_col, subsize);
      break;
    case PARTITION_VERT:
      DEC_BLOCK(mi_row, mi_col, subsize);
      if (has_cols) DEC_BLOCK(mi_row, mi_col + hbs, subsize);
      break;
    case PARTITION_SPLIT:
      DEC_PARTITION(mi_row, mi_col, subsize);
      DEC_PARTITION(mi_row, mi_col + hbs, subsize);
      DEC_PARTITION(mi_row + hbs, mi_col, subsize);
      DEC_PARTITION(mi_row + hbs, mi_col + hbs, subsize);
      break;
    case PARTITION_HORZ_A:
      DEC_BLOCK(mi_row, mi_col, bsize2);
      DEC_BLOCK(mi_row, mi_col + hbs, bsize2);
      DEC_BLOCK(mi_row + hbs, mi_col, subsize);
      break;
    case PARTITION_HORZ_B:
      DEC_BLOCK(mi_row, mi_col, subsize);
      DEC_BLOCK(mi_row + hbs, mi_col, bsize2);
      DEC_BLOCK(mi_row + hbs, mi_col + hbs, bsize2);
      break;
    case PARTITION_VERT_A:
      DEC_BLOCK(mi_row, mi_col, bsize2);
      DEC_BLOCK(mi_row + hbs, mi_col, bsize2);
      DEC_BLOCK(mi_row, mi_col + hbs, subsize);
      break;
    case PARTITION_VERT_B:
      DEC_BLOCK(mi_row, mi_col, subsize);
      DEC_BLOCK(mi_row, mi_col + hbs, bsize2);
      DEC_BLOCK(mi_row + hbs, mi_col + hbs, bsize2);
      break;
    case PARTITION_HORZ_4:
      for (int i = 0; i < 4; ++i) {
        int this_mi_row = mi_row + i * quarter_step;
        if (i > 0 && this_mi_row >= cm->mi_params.mi_rows) break;
        DEC_BLOCK(this_mi_row, mi_col, subsize);
      }
      break;
    case PARTITION_VERT_4:
      for (int i = 0; i < 4; ++i) {
        int this_mi_col = mi_col + i * quarter_step;
        if (i > 0 && this_mi_col >= cm->mi_params.mi_cols) break;
        DEC_BLOCK(mi_row, this_mi_col, subsize);
      }
      break;
    default: assert(0 && "Invalid partition type");
  }

#undef DEC_PARTITION
#undef DEC_BLOCK
#undef DEC_BLOCK_EPT_ARG
#undef DEC_BLOCK_STX_ARG

  if (parse_decode_flag & 1)
    update_ext_partition_context(xd, mi_row, mi_col, subsize, bsize, partition);
}

static AOM_INLINE void setup_bool_decoder(
    MACROBLOCKD *const xd, const uint8_t *data, const uint8_t *data_end,
    const size_t read_size, struct aom_internal_error_info *error_info,
    aom_reader *r, uint8_t allow_update_cdf) {
  // Validate the calculated partition length. If the buffer
  // described by the partition can't be fully read, then restrict
  // it to the portion that can be (for EC mode) or throw an error.
  if (!read_is_valid(data, read_size, data_end)) {
    // When internal error occurs ensure that xd->mi_row is set appropriately
    // w.r.t. current tile, which is used to signal processing of current row is
    // done in row-mt decoding.
    xd->mi_row = xd->tile.mi_row_start;

    aom_internal_error(error_info, AOM_CODEC_CORRUPT_FRAME,
                       "Truncated packet or corrupt tile length");
  }
  if (aom_reader_init(r, data, read_size)) {
    // When internal error occurs ensure that xd->mi_row is set appropriately
    // w.r.t. current tile, which is used to signal processing of current row is
    // done in row-mt decoding.
    xd->mi_row = xd->tile.mi_row_start;

    aom_internal_error(error_info, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate bool decoder %d", 1);
  }

  r->allow_update_cdf = allow_update_cdf;
}

static AOM_INLINE void setup_segmentation(AV1_COMMON *const cm,
                                          struct aom_read_bit_buffer *rb) {
  struct segmentation *const seg = &cm->seg;

  seg->update_map = 0;
  seg->update_data = 0;
  seg->temporal_update = 0;

  seg->enabled = aom_rb_read_bit(rb);
  if (!seg->enabled) {
    if (cm->cur_frame->seg_map) {
      memset(cm->cur_frame->seg_map, 0,
             (cm->cur_frame->mi_rows * cm->cur_frame->mi_cols));
    }

    memset(seg, 0, sizeof(*seg));
    segfeatures_copy(&cm->cur_frame->seg, seg);
    return;
  }
  if (cm->seg.enabled && cm->prev_frame &&
      (cm->mi_params.mi_rows == cm->prev_frame->mi_rows) &&
      (cm->mi_params.mi_cols == cm->prev_frame->mi_cols)) {
    cm->last_frame_seg_map = cm->prev_frame->seg_map;
  } else {
    cm->last_frame_seg_map = NULL;
  }
  // Read update flags
  if (cm->features.primary_ref_frame == PRIMARY_REF_NONE) {
    // These frames can't use previous frames, so must signal map + features
    seg->update_map = 1;
    seg->temporal_update = 0;
    seg->update_data = 1;
  } else {
    seg->update_map = aom_rb_read_bit(rb);
    if (seg->update_map) {
      seg->temporal_update = aom_rb_read_bit(rb);
    } else {
      seg->temporal_update = 0;
    }
    seg->update_data = aom_rb_read_bit(rb);
  }

  // Segmentation data update
  if (seg->update_data) {
    av1_clearall_segfeatures(seg);

    for (int i = 0; i < MAX_SEGMENTS; i++) {
      for (int j = 0; j < SEG_LVL_MAX; j++) {
        int data = 0;
        const int feature_enabled = aom_rb_read_bit(rb);
        if (feature_enabled) {
          av1_enable_segfeature(seg, i, j);

          const int data_max = av1_seg_feature_data_max(j);
          const int data_min = -data_max;
          const int ubits = get_unsigned_bits(data_max);

          if (av1_is_segfeature_signed(j)) {
            data = aom_rb_read_inv_signed_literal(rb, ubits);
          } else {
            data = aom_rb_read_literal(rb, ubits);
          }

          data = clamp(data, data_min, data_max);
        }
        av1_set_segdata(seg, i, j, data);
      }
    }
    av1_calculate_segdata(seg);
  } else if (cm->prev_frame) {
    segfeatures_copy(seg, &cm->prev_frame->seg);
  }
  segfeatures_copy(&cm->cur_frame->seg, seg);
}

static AOM_INLINE void decode_restoration_mode(AV1_COMMON *cm,
                                               struct aom_read_bit_buffer *rb) {
  assert(!cm->features.all_lossless);
  const int num_planes = av1_num_planes(cm);
  if (cm->features.allow_intrabc) return;
  int all_none = 1, chroma_none = 1;
  for (int p = 0; p < num_planes; ++p) {
    RestorationInfo *rsi = &cm->rst_info[p];
    if (aom_rb_read_bit(rb)) {
      rsi->frame_restoration_type =
          aom_rb_read_bit(rb) ? RESTORE_SGRPROJ : RESTORE_WIENER;
    } else {
      rsi->frame_restoration_type =
          aom_rb_read_bit(rb) ? RESTORE_SWITCHABLE : RESTORE_NONE;
    }
    if (rsi->frame_restoration_type != RESTORE_NONE) {
      all_none = 0;
      chroma_none &= p == 0;
    }
  }
  if (!all_none) {
    assert(cm->seq_params->sb_size == BLOCK_64X64 ||
           cm->seq_params->sb_size == BLOCK_128X128);
    const int sb_size = cm->seq_params->sb_size == BLOCK_128X128 ? 128 : 64;

    for (int p = 0; p < num_planes; ++p)
      cm->rst_info[p].restoration_unit_size = sb_size;

    RestorationInfo *rsi = &cm->rst_info[0];

    if (sb_size == 64) {
      rsi->restoration_unit_size <<= aom_rb_read_bit(rb);
    }
    if (rsi->restoration_unit_size > 64) {
      rsi->restoration_unit_size <<= aom_rb_read_bit(rb);
    }
  } else {
    const int size = RESTORATION_UNITSIZE_MAX;
    for (int p = 0; p < num_planes; ++p)
      cm->rst_info[p].restoration_unit_size = size;
  }

  if (num_planes > 1) {
    int s =
        AOMMIN(cm->seq_params->subsampling_x, cm->seq_params->subsampling_y);
    if (s && !chroma_none) {
      cm->rst_info[1].restoration_unit_size =
          cm->rst_info[0].restoration_unit_size >> (aom_rb_read_bit(rb) * s);
    } else {
      cm->rst_info[1].restoration_unit_size =
          cm->rst_info[0].restoration_unit_size;
    }
    cm->rst_info[2].restoration_unit_size =
        cm->rst_info[1].restoration_unit_size;
  }
}

static AOM_INLINE void read_wiener_filter(int wiener_win,
                                          WienerInfo *wiener_info,
                                          WienerInfo *ref_wiener_info,
                                          aom_reader *rb) {
  memset(wiener_info->vfilter, 0, sizeof(wiener_info->vfilter));
  memset(wiener_info->hfilter, 0, sizeof(wiener_info->hfilter));

  if (wiener_win == WIENER_WIN)
    wiener_info->vfilter[0] = wiener_info->vfilter[WIENER_WIN - 1] =
        aom_read_primitive_refsubexpfin(
            rb, WIENER_FILT_TAP0_MAXV - WIENER_FILT_TAP0_MINV + 1,
            WIENER_FILT_TAP0_SUBEXP_K,
            ref_wiener_info->vfilter[0] - WIENER_FILT_TAP0_MINV, ACCT_STR) +
        WIENER_FILT_TAP0_MINV;
  else
    wiener_info->vfilter[0] = wiener_info->vfilter[WIENER_WIN - 1] = 0;
  wiener_info->vfilter[1] = wiener_info->vfilter[WIENER_WIN - 2] =
      aom_read_primitive_refsubexpfin(
          rb, WIENER_FILT_TAP1_MAXV - WIENER_FILT_TAP1_MINV + 1,
          WIENER_FILT_TAP1_SUBEXP_K,
          ref_wiener_info->vfilter[1] - WIENER_FILT_TAP1_MINV, ACCT_STR) +
      WIENER_FILT_TAP1_MINV;
  wiener_info->vfilter[2] = wiener_info->vfilter[WIENER_WIN - 3] =
      aom_read_primitive_refsubexpfin(
          rb, WIENER_FILT_TAP2_MAXV - WIENER_FILT_TAP2_MINV + 1,
          WIENER_FILT_TAP2_SUBEXP_K,
          ref_wiener_info->vfilter[2] - WIENER_FILT_TAP2_MINV, ACCT_STR) +
      WIENER_FILT_TAP2_MINV;
  // The central element has an implicit +WIENER_FILT_STEP
  wiener_info->vfilter[WIENER_HALFWIN] =
      -2 * (wiener_info->vfilter[0] + wiener_info->vfilter[1] +
            wiener_info->vfilter[2]);

  if (wiener_win == WIENER_WIN)
    wiener_info->hfilter[0] = wiener_info->hfilter[WIENER_WIN - 1] =
        aom_read_primitive_refsubexpfin(
            rb, WIENER_FILT_TAP0_MAXV - WIENER_FILT_TAP0_MINV + 1,
            WIENER_FILT_TAP0_SUBEXP_K,
            ref_wiener_info->hfilter[0] - WIENER_FILT_TAP0_MINV, ACCT_STR) +
        WIENER_FILT_TAP0_MINV;
  else
    wiener_info->hfilter[0] = wiener_info->hfilter[WIENER_WIN - 1] = 0;
  wiener_info->hfilter[1] = wiener_info->hfilter[WIENER_WIN - 2] =
      aom_read_primitive_refsubexpfin(
          rb, WIENER_FILT_TAP1_MAXV - WIENER_FILT_TAP1_MINV + 1,
          WIENER_FILT_TAP1_SUBEXP_K,
          ref_wiener_info->hfilter[1] - WIENER_FILT_TAP1_MINV, ACCT_STR) +
      WIENER_FILT_TAP1_MINV;
  wiener_info->hfilter[2] = wiener_info->hfilter[WIENER_WIN - 3] =
      aom_read_primitive_refsubexpfin(
          rb, WIENER_FILT_TAP2_MAXV - WIENER_FILT_TAP2_MINV + 1,
          WIENER_FILT_TAP2_SUBEXP_K,
          ref_wiener_info->hfilter[2] - WIENER_FILT_TAP2_MINV, ACCT_STR) +
      WIENER_FILT_TAP2_MINV;
  // The central element has an implicit +WIENER_FILT_STEP
  wiener_info->hfilter[WIENER_HALFWIN] =
      -2 * (wiener_info->hfilter[0] + wiener_info->hfilter[1] +
            wiener_info->hfilter[2]);
  memcpy(ref_wiener_info, wiener_info, sizeof(*wiener_info));
}

static AOM_INLINE void read_sgrproj_filter(SgrprojInfo *sgrproj_info,
                                           SgrprojInfo *ref_sgrproj_info,
                                           aom_reader *rb) {
  sgrproj_info->ep = aom_read_literal(rb, SGRPROJ_PARAMS_BITS, ACCT_STR);
  const sgr_params_type *params = &av1_sgr_params[sgrproj_info->ep];

  if (params->r[0] == 0) {
    sgrproj_info->xqd[0] = 0;
    sgrproj_info->xqd[1] =
        aom_read_primitive_refsubexpfin(
            rb, SGRPROJ_PRJ_MAX1 - SGRPROJ_PRJ_MIN1 + 1, SGRPROJ_PRJ_SUBEXP_K,
            ref_sgrproj_info->xqd[1] - SGRPROJ_PRJ_MIN1, ACCT_STR) +
        SGRPROJ_PRJ_MIN1;
  } else if (params->r[1] == 0) {
    sgrproj_info->xqd[0] =
        aom_read_primitive_refsubexpfin(
            rb, SGRPROJ_PRJ_MAX0 - SGRPROJ_PRJ_MIN0 + 1, SGRPROJ_PRJ_SUBEXP_K,
            ref_sgrproj_info->xqd[0] - SGRPROJ_PRJ_MIN0, ACCT_STR) +
        SGRPROJ_PRJ_MIN0;
    sgrproj_info->xqd[1] = clamp((1 << SGRPROJ_PRJ_BITS) - sgrproj_info->xqd[0],
                                 SGRPROJ_PRJ_MIN1, SGRPROJ_PRJ_MAX1);
  } else {
    sgrproj_info->xqd[0] =
        aom_read_primitive_refsubexpfin(
            rb, SGRPROJ_PRJ_MAX0 - SGRPROJ_PRJ_MIN0 + 1, SGRPROJ_PRJ_SUBEXP_K,
            ref_sgrproj_info->xqd[0] - SGRPROJ_PRJ_MIN0, ACCT_STR) +
        SGRPROJ_PRJ_MIN0;
    sgrproj_info->xqd[1] =
        aom_read_primitive_refsubexpfin(
            rb, SGRPROJ_PRJ_MAX1 - SGRPROJ_PRJ_MIN1 + 1, SGRPROJ_PRJ_SUBEXP_K,
            ref_sgrproj_info->xqd[1] - SGRPROJ_PRJ_MIN1, ACCT_STR) +
        SGRPROJ_PRJ_MIN1;
  }

  memcpy(ref_sgrproj_info, sgrproj_info, sizeof(*sgrproj_info));
}

static AOM_INLINE void loop_restoration_read_sb_coeffs(
    const AV1_COMMON *const cm, MACROBLOCKD *xd, aom_reader *const r, int plane,
    int runit_idx) {
  const RestorationInfo *rsi = &cm->rst_info[plane];
  RestorationUnitInfo *rui = &rsi->unit_info[runit_idx];
  assert(rsi->frame_restoration_type != RESTORE_NONE);

  assert(!cm->features.all_lossless);

  const int wiener_win = (plane > 0) ? WIENER_WIN_CHROMA : WIENER_WIN;
  WienerInfo *wiener_info = xd->wiener_info + plane;
  SgrprojInfo *sgrproj_info = xd->sgrproj_info + plane;

  if (rsi->frame_restoration_type == RESTORE_SWITCHABLE) {
    rui->restoration_type =
        aom_read_symbol(r, xd->tile_ctx->switchable_restore_cdf,
                        RESTORE_SWITCHABLE_TYPES, ACCT_STR);
    switch (rui->restoration_type) {
      case RESTORE_WIENER:
        read_wiener_filter(wiener_win, &rui->wiener_info, wiener_info, r);
        break;
      case RESTORE_SGRPROJ:
        read_sgrproj_filter(&rui->sgrproj_info, sgrproj_info, r);
        break;
      default: assert(rui->restoration_type == RESTORE_NONE); break;
    }
  } else if (rsi->frame_restoration_type == RESTORE_WIENER) {
    if (aom_read_symbol(r, xd->tile_ctx->wiener_restore_cdf, 2, ACCT_STR)) {
      rui->restoration_type = RESTORE_WIENER;
      read_wiener_filter(wiener_win, &rui->wiener_info, wiener_info, r);
    } else {
      rui->restoration_type = RESTORE_NONE;
    }
  } else if (rsi->frame_restoration_type == RESTORE_SGRPROJ) {
    if (aom_read_symbol(r, xd->tile_ctx->sgrproj_restore_cdf, 2, ACCT_STR)) {
      rui->restoration_type = RESTORE_SGRPROJ;
      read_sgrproj_filter(&rui->sgrproj_info, sgrproj_info, r);
    } else {
      rui->restoration_type = RESTORE_NONE;
    }
  }
}

static AOM_INLINE void setup_loopfilter(AV1_COMMON *cm,
                                        struct aom_read_bit_buffer *rb) {
  const int num_planes = av1_num_planes(cm);
  struct loopfilter *lf = &cm->lf;

  if (cm->features.allow_intrabc || cm->features.coded_lossless) {
    // write default deltas to frame buffer
    av1_set_default_ref_deltas(cm->cur_frame->ref_deltas);
    av1_set_default_mode_deltas(cm->cur_frame->mode_deltas);
    return;
  }
  assert(!cm->features.coded_lossless);
  if (cm->prev_frame) {
    // write deltas to frame buffer
    memcpy(lf->ref_deltas, cm->prev_frame->ref_deltas, REF_FRAMES);
    memcpy(lf->mode_deltas, cm->prev_frame->mode_deltas, MAX_MODE_LF_DELTAS);
  } else {
    av1_set_default_ref_deltas(lf->ref_deltas);
    av1_set_default_mode_deltas(lf->mode_deltas);
  }
  lf->filter_level[0] = aom_rb_read_literal(rb, 6);
  lf->filter_level[1] = aom_rb_read_literal(rb, 6);
  if (num_planes > 1) {
    if (lf->filter_level[0] || lf->filter_level[1]) {
      lf->filter_level_u = aom_rb_read_literal(rb, 6);
      lf->filter_level_v = aom_rb_read_literal(rb, 6);
    }
  }
  lf->sharpness_level = aom_rb_read_literal(rb, 3);

  // Read in loop filter deltas applied at the MB level based on mode or ref
  // frame.
  lf->mode_ref_delta_update = 0;

  lf->mode_ref_delta_enabled = aom_rb_read_bit(rb);
  if (lf->mode_ref_delta_enabled) {
    lf->mode_ref_delta_update = aom_rb_read_bit(rb);
    if (lf->mode_ref_delta_update) {
      for (int i = 0; i < REF_FRAMES; i++)
        if (aom_rb_read_bit(rb))
          lf->ref_deltas[i] = aom_rb_read_inv_signed_literal(rb, 6);

      for (int i = 0; i < MAX_MODE_LF_DELTAS; i++)
        if (aom_rb_read_bit(rb))
          lf->mode_deltas[i] = aom_rb_read_inv_signed_literal(rb, 6);
    }
  }

  // write deltas to frame buffer
  memcpy(cm->cur_frame->ref_deltas, lf->ref_deltas, REF_FRAMES);
  memcpy(cm->cur_frame->mode_deltas, lf->mode_deltas, MAX_MODE_LF_DELTAS);
}

static AOM_INLINE void setup_cdef(AV1_COMMON *cm,
                                  struct aom_read_bit_buffer *rb) {
  const int num_planes = av1_num_planes(cm);
  CdefInfo *const cdef_info = &cm->cdef_info;

  if (cm->features.allow_intrabc) return;
  cdef_info->cdef_damping = aom_rb_read_literal(rb, 2) + 3;
  cdef_info->cdef_bits = aom_rb_read_literal(rb, 2);
  cdef_info->nb_cdef_strengths = 1 << cdef_info->cdef_bits;
  for (int i = 0; i < cdef_info->nb_cdef_strengths; i++) {
    cdef_info->cdef_strengths[i] = aom_rb_read_literal(rb, CDEF_STRENGTH_BITS);
    cdef_info->cdef_uv_strengths[i] =
        num_planes > 1 ? aom_rb_read_literal(rb, CDEF_STRENGTH_BITS) : 0;
  }
}

static INLINE int read_delta_q(struct aom_read_bit_buffer *rb) {
  return aom_rb_read_bit(rb) ? aom_rb_read_inv_signed_literal(rb, 6) : 0;
}

static AOM_INLINE void setup_quantization(CommonQuantParams *quant_params,
                                          int num_planes,
                                          bool separate_uv_delta_q,
                                          struct aom_read_bit_buffer *rb) {
  quant_params->base_qindex = aom_rb_read_literal(rb, QINDEX_BITS);
  quant_params->y_dc_delta_q = read_delta_q(rb);
  if (num_planes > 1) {
    int diff_uv_delta = 0;
    if (separate_uv_delta_q) diff_uv_delta = aom_rb_read_bit(rb);
    quant_params->u_dc_delta_q = read_delta_q(rb);
    quant_params->u_ac_delta_q = read_delta_q(rb);
    if (diff_uv_delta) {
      quant_params->v_dc_delta_q = read_delta_q(rb);
      quant_params->v_ac_delta_q = read_delta_q(rb);
    } else {
      quant_params->v_dc_delta_q = quant_params->u_dc_delta_q;
      quant_params->v_ac_delta_q = quant_params->u_ac_delta_q;
    }
  } else {
    quant_params->u_dc_delta_q = 0;
    quant_params->u_ac_delta_q = 0;
    quant_params->v_dc_delta_q = 0;
    quant_params->v_ac_delta_q = 0;
  }
  quant_params->using_qmatrix = aom_rb_read_bit(rb);
  if (quant_params->using_qmatrix) {
    quant_params->qmatrix_level_y = aom_rb_read_literal(rb, QM_LEVEL_BITS);
    quant_params->qmatrix_level_u = aom_rb_read_literal(rb, QM_LEVEL_BITS);
    if (!separate_uv_delta_q)
      quant_params->qmatrix_level_v = quant_params->qmatrix_level_u;
    else
      quant_params->qmatrix_level_v = aom_rb_read_literal(rb, QM_LEVEL_BITS);
  } else {
    quant_params->qmatrix_level_y = 0;
    quant_params->qmatrix_level_u = 0;
    quant_params->qmatrix_level_v = 0;
  }
}

// Build y/uv dequant values based on segmentation.
static AOM_INLINE void setup_segmentation_dequant(AV1_COMMON *const cm,
                                                  MACROBLOCKD *const xd) {
  const int bit_depth = cm->seq_params->bit_depth;
  // When segmentation is disabled, only the first value is used.  The
  // remaining are don't cares.
  const int max_segments = cm->seg.enabled ? MAX_SEGMENTS : 1;
  CommonQuantParams *const quant_params = &cm->quant_params;
  for (int i = 0; i < max_segments; ++i) {
    const int qindex = xd->qindex[i];
    quant_params->y_dequant_QTX[i][0] =
        av1_dc_quant_QTX(qindex, quant_params->y_dc_delta_q, bit_depth);
    quant_params->y_dequant_QTX[i][1] = av1_ac_quant_QTX(qindex, 0, bit_depth);
    quant_params->u_dequant_QTX[i][0] =
        av1_dc_quant_QTX(qindex, quant_params->u_dc_delta_q, bit_depth);
    quant_params->u_dequant_QTX[i][1] =
        av1_ac_quant_QTX(qindex, quant_params->u_ac_delta_q, bit_depth);
    quant_params->v_dequant_QTX[i][0] =
        av1_dc_quant_QTX(qindex, quant_params->v_dc_delta_q, bit_depth);
    quant_params->v_dequant_QTX[i][1] =
        av1_ac_quant_QTX(qindex, quant_params->v_ac_delta_q, bit_depth);
    const int use_qmatrix = av1_use_qmatrix(quant_params, xd, i);
    // NB: depends on base index so there is only 1 set per frame
    // No quant weighting when lossless or signalled not using QM
    const int qmlevel_y =
        use_qmatrix ? quant_params->qmatrix_level_y : NUM_QM_LEVELS - 1;
    for (int j = 0; j < TX_SIZES_ALL; ++j) {
      quant_params->y_iqmatrix[i][j] =
          av1_iqmatrix(quant_params, qmlevel_y, AOM_PLANE_Y, j);
    }
    const int qmlevel_u =
        use_qmatrix ? quant_params->qmatrix_level_u : NUM_QM_LEVELS - 1;
    for (int j = 0; j < TX_SIZES_ALL; ++j) {
      quant_params->u_iqmatrix[i][j] =
          av1_iqmatrix(quant_params, qmlevel_u, AOM_PLANE_U, j);
    }
    const int qmlevel_v =
        use_qmatrix ? quant_params->qmatrix_level_v : NUM_QM_LEVELS - 1;
    for (int j = 0; j < TX_SIZES_ALL; ++j) {
      quant_params->v_iqmatrix[i][j] =
          av1_iqmatrix(quant_params, qmlevel_v, AOM_PLANE_V, j);
    }
  }
}

static InterpFilter read_frame_interp_filter(struct aom_read_bit_buffer *rb) {
  return aom_rb_read_bit(rb) ? SWITCHABLE
                             : aom_rb_read_literal(rb, LOG_SWITCHABLE_FILTERS);
}

static AOM_INLINE void setup_render_size(AV1_COMMON *cm,
                                         struct aom_read_bit_buffer *rb) {
  cm->render_width = cm->superres_upscaled_width;
  cm->render_height = cm->superres_upscaled_height;
  if (aom_rb_read_bit(rb))
    av1_read_frame_size(rb, 16, 16, &cm->render_width, &cm->render_height);
}

// TODO(afergs): make "struct aom_read_bit_buffer *const rb"?
static AOM_INLINE void setup_superres(AV1_COMMON *const cm,
                                      struct aom_read_bit_buffer *rb,
                                      int *width, int *height) {
  cm->superres_upscaled_width = *width;
  cm->superres_upscaled_height = *height;

  const SequenceHeader *const seq_params = cm->seq_params;
  if (!seq_params->enable_superres) return;

  if (aom_rb_read_bit(rb)) {
    cm->superres_scale_denominator =
        (uint8_t)aom_rb_read_literal(rb, SUPERRES_SCALE_BITS);
    cm->superres_scale_denominator += SUPERRES_SCALE_DENOMINATOR_MIN;
    // Don't edit cm->width or cm->height directly, or the buffers won't get
    // resized correctly
    av1_calculate_scaled_superres_size(width, height,
                                       cm->superres_scale_denominator);
  } else {
    // 1:1 scaling - ie. no scaling, scale not provided
    cm->superres_scale_denominator = SCALE_NUMERATOR;
  }
}

static AOM_INLINE void resize_context_buffers(AV1_COMMON *cm, int width,
                                              int height) {
#if CONFIG_SIZE_LIMIT
  if (width > DECODE_WIDTH_LIMIT || height > DECODE_HEIGHT_LIMIT)
    aom_internal_error(cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Dimensions of %dx%d beyond allowed size of %dx%d.",
                       width, height, DECODE_WIDTH_LIMIT, DECODE_HEIGHT_LIMIT);
#endif
  if (cm->width != width || cm->height != height) {
    const int new_mi_rows = CEIL_POWER_OF_TWO(height, MI_SIZE_LOG2);
    const int new_mi_cols = CEIL_POWER_OF_TWO(width, MI_SIZE_LOG2);

    // Allocations in av1_alloc_context_buffers() depend on individual
    // dimensions as well as the overall size.
    if (new_mi_cols > cm->mi_params.mi_cols ||
        new_mi_rows > cm->mi_params.mi_rows) {
      if (av1_alloc_context_buffers(cm, width, height, BLOCK_4X4)) {
        // The cm->mi_* values have been cleared and any existing context
        // buffers have been freed. Clear cm->width and cm->height to be
        // consistent and to force a realloc next time.
        cm->width = 0;
        cm->height = 0;
        aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR,
                           "Failed to allocate context buffers");
      }
    } else {
      cm->mi_params.set_mb_mi(&cm->mi_params, width, height, BLOCK_4X4);
    }
    av1_init_mi_buffers(&cm->mi_params);
    cm->width = width;
    cm->height = height;
  }

  ensure_mv_buffer(cm->cur_frame, cm);
  cm->cur_frame->width = cm->width;
  cm->cur_frame->height = cm->height;
}

static AOM_INLINE void setup_buffer_pool(AV1_COMMON *cm) {
  BufferPool *const pool = cm->buffer_pool;
  const SequenceHeader *const seq_params = cm->seq_params;

  lock_buffer_pool(pool);
  if (aom_realloc_frame_buffer(
          &cm->cur_frame->buf, cm->width, cm->height, seq_params->subsampling_x,
          seq_params->subsampling_y, seq_params->use_highbitdepth,
          AOM_DEC_BORDER_IN_PIXELS, cm->features.byte_alignment,
          &cm->cur_frame->raw_frame_buffer, pool->get_fb_cb, pool->cb_priv, 0,
          0)) {
    unlock_buffer_pool(pool);
    aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate frame buffer");
  }
  unlock_buffer_pool(pool);

  cm->cur_frame->buf.bit_depth = (unsigned int)seq_params->bit_depth;
  cm->cur_frame->buf.color_primaries = seq_params->color_primaries;
  cm->cur_frame->buf.transfer_characteristics =
      seq_params->transfer_characteristics;
  cm->cur_frame->buf.matrix_coefficients = seq_params->matrix_coefficients;
  cm->cur_frame->buf.monochrome = seq_params->monochrome;
  cm->cur_frame->buf.chroma_sample_position =
      seq_params->chroma_sample_position;
  cm->cur_frame->buf.color_range = seq_params->color_range;
  cm->cur_frame->buf.render_width = cm->render_width;
  cm->cur_frame->buf.render_height = cm->render_height;
}

static AOM_INLINE void setup_frame_size(AV1_COMMON *cm,
                                        int frame_size_override_flag,
                                        struct aom_read_bit_buffer *rb) {
  const SequenceHeader *const seq_params = cm->seq_params;
  int width, height;

  if (frame_size_override_flag) {
    int num_bits_width = seq_params->num_bits_width;
    int num_bits_height = seq_params->num_bits_height;
    av1_read_frame_size(rb, num_bits_width, num_bits_height, &width, &height);
    if (width > seq_params->max_frame_width ||
        height > seq_params->max_frame_height) {
      aom_internal_error(cm->error, AOM_CODEC_CORRUPT_FRAME,
                         "Frame dimensions are larger than the maximum values");
    }
  } else {
    width = seq_params->max_frame_width;
    height = seq_params->max_frame_height;
  }

  setup_superres(cm, rb, &width, &height);
  resize_context_buffers(cm, width, height);
  setup_render_size(cm, rb);
  setup_buffer_pool(cm);
}

static AOM_INLINE void setup_sb_size(SequenceHeader *seq_params,
                                     struct aom_read_bit_buffer *rb) {
  set_sb_size(seq_params, aom_rb_read_bit(rb) ? BLOCK_128X128 : BLOCK_64X64);
}

static INLINE int valid_ref_frame_img_fmt(aom_bit_depth_t ref_bit_depth,
                                          int ref_xss, int ref_yss,
                                          aom_bit_depth_t this_bit_depth,
                                          int this_xss, int this_yss) {
  return ref_bit_depth == this_bit_depth && ref_xss == this_xss &&
         ref_yss == this_yss;
}

static AOM_INLINE void setup_frame_size_with_refs(
    AV1_COMMON *cm, struct aom_read_bit_buffer *rb) {
  int width, height;
  int found = 0;
  int has_valid_ref_frame = 0;
  for (int i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
    if (aom_rb_read_bit(rb)) {
      const RefCntBuffer *const ref_buf = get_ref_frame_buf(cm, i);
      // This will never be NULL in a normal stream, as streams are required to
      // have a shown keyframe before any inter frames, which would refresh all
      // the reference buffers. However, it might be null if we're starting in
      // the middle of a stream, and static analysis will error if we don't do
      // a null check here.
      if (ref_buf == NULL) {
        aom_internal_error(cm->error, AOM_CODEC_CORRUPT_FRAME,
                           "Invalid condition: invalid reference buffer");
      } else {
        const YV12_BUFFER_CONFIG *const buf = &ref_buf->buf;
        width = buf->y_crop_width;
        height = buf->y_crop_height;
        cm->render_width = buf->render_width;
        cm->render_height = buf->render_height;
        setup_superres(cm, rb, &width, &height);
        resize_context_buffers(cm, width, height);
        found = 1;
        break;
      }
    }
  }

  const SequenceHeader *const seq_params = cm->seq_params;
  if (!found) {
    int num_bits_width = seq_params->num_bits_width;
    int num_bits_height = seq_params->num_bits_height;

    av1_read_frame_size(rb, num_bits_width, num_bits_height, &width, &height);
    setup_superres(cm, rb, &width, &height);
    resize_context_buffers(cm, width, height);
    setup_render_size(cm, rb);
  }

  if (width <= 0 || height <= 0)
    aom_internal_error(cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Invalid frame size");

  // Check to make sure at least one of frames that this frame references
  // has valid dimensions.
  for (int i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
    const RefCntBuffer *const ref_frame = get_ref_frame_buf(cm, i);
    has_valid_ref_frame |=
        valid_ref_frame_size(ref_frame->buf.y_crop_width,
                             ref_frame->buf.y_crop_height, width, height);
  }
  if (!has_valid_ref_frame)
    aom_internal_error(cm->error, AOM_CODEC_CORRUPT_FRAME,
                       "Referenced frame has invalid size");
  for (int i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
    const RefCntBuffer *const ref_frame = get_ref_frame_buf(cm, i);
    if (!valid_ref_frame_img_fmt(
            ref_frame->buf.bit_depth, ref_frame->buf.subsampling_x,
            ref_frame->buf.subsampling_y, seq_params->bit_depth,
            seq_params->subsampling_x, seq_params->subsampling_y))
      aom_internal_error(cm->error, AOM_CODEC_CORRUPT_FRAME,
                         "Referenced frame has incompatible color format");
  }
  setup_buffer_pool(cm);
}

// Same function as av1_read_uniform but reading from uncompresses header wb
static int rb_read_uniform(struct aom_read_bit_buffer *const rb, int n) {
  const int l = get_unsigned_bits(n);
  const int m = (1 << l) - n;
  const int v = aom_rb_read_literal(rb, l - 1);
  assert(l != 0);
  if (v < m)
    return v;
  else
    return (v << 1) - m + aom_rb_read_bit(rb);
}

static AOM_INLINE void read_tile_info_max_tile(
    AV1_COMMON *const cm, struct aom_read_bit_buffer *const rb) {
  const SequenceHeader *const seq_params = cm->seq_params;
  CommonTileParams *const tiles = &cm->tiles;
  int width_sb =
      CEIL_POWER_OF_TWO(cm->mi_params.mi_cols, seq_params->mib_size_log2);
  int height_sb =
      CEIL_POWER_OF_TWO(cm->mi_params.mi_rows, seq_params->mib_size_log2);

  av1_get_tile_limits(cm);
  tiles->uniform_spacing = aom_rb_read_bit(rb);

  // Read tile columns
  if (tiles->uniform_spacing) {
    tiles->log2_cols = tiles->min_log2_cols;
    while (tiles->log2_cols < tiles->max_log2_cols) {
      if (!aom_rb_read_bit(rb)) {
        break;
      }
      tiles->log2_cols++;
    }
  } else {
    int i;
    int start_sb;
    for (i = 0, start_sb = 0; width_sb > 0 && i < MAX_TILE_COLS; i++) {
      const int size_sb =
          1 + rb_read_uniform(rb, AOMMIN(width_sb, tiles->max_width_sb));
      tiles->col_start_sb[i] = start_sb;
      start_sb += size_sb;
      width_sb -= size_sb;
    }
    tiles->cols = i;
    tiles->col_start_sb[i] = start_sb + width_sb;
  }
  av1_calculate_tile_cols(seq_params, cm->mi_params.mi_rows,
                          cm->mi_params.mi_cols, tiles);

  // Read tile rows
  if (tiles->uniform_spacing) {
    tiles->log2_rows = tiles->min_log2_rows;
    while (tiles->log2_rows < tiles->max_log2_rows) {
      if (!aom_rb_read_bit(rb)) {
        break;
      }
      tiles->log2_rows++;
    }
  } else {
    int i;
    int start_sb;
    for (i = 0, start_sb = 0; height_sb > 0 && i < MAX_TILE_ROWS; i++) {
      const int size_sb =
          1 + rb_read_uniform(rb, AOMMIN(height_sb, tiles->max_height_sb));
      tiles->row_start_sb[i] = start_sb;
      start_sb += size_sb;
      height_sb -= size_sb;
    }
    tiles->rows = i;
    tiles->row_start_sb[i] = start_sb + height_sb;
  }
  av1_calculate_tile_rows(seq_params, cm->mi_params.mi_rows, tiles);
}

void av1_set_single_tile_decoding_mode(AV1_COMMON *const cm) {
  cm->tiles.single_tile_decoding = 0;
  if (cm->tiles.large_scale) {
    struct loopfilter *lf = &cm->lf;
    RestorationInfo *const rst_info = cm->rst_info;
    const CdefInfo *const cdef_info = &cm->cdef_info;

    // Figure out single_tile_decoding by loopfilter_level.
    const int no_loopfilter = !(lf->filter_level[0] || lf->filter_level[1]);
    const int no_cdef = cdef_info->cdef_bits == 0 &&
                        cdef_info->cdef_strengths[0] == 0 &&
                        cdef_info->cdef_uv_strengths[0] == 0;
    const int no_restoration =
        rst_info[0].frame_restoration_type == RESTORE_NONE &&
        rst_info[1].frame_restoration_type == RESTORE_NONE &&
        rst_info[2].frame_restoration_type == RESTORE_NONE;
    assert(IMPLIES(cm->features.coded_lossless, no_loopfilter && no_cdef));
    assert(IMPLIES(cm->features.all_lossless, no_restoration));
    cm->tiles.single_tile_decoding = no_loopfilter && no_cdef && no_restoration;
  }
}

static AOM_INLINE void read_tile_info(AV1Decoder *const pbi,
                                      struct aom_read_bit_buffer *const rb) {
  AV1_COMMON *const cm = &pbi->common;

  read_tile_info_max_tile(cm, rb);

  pbi->context_update_tile_id = 0;
  if (cm->tiles.rows * cm->tiles.cols > 1) {
    // tile to use for cdf update
    pbi->context_update_tile_id =
        aom_rb_read_literal(rb, cm->tiles.log2_rows + cm->tiles.log2_cols);
    if (pbi->context_update_tile_id >= cm->tiles.rows * cm->tiles.cols) {
      aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                         "Invalid context_update_tile_id");
    }
    // tile size magnitude
    pbi->tile_size_bytes = aom_rb_read_literal(rb, 2) + 1;
  }
}

#if EXT_TILE_DEBUG
static AOM_INLINE void read_ext_tile_info(
    AV1Decoder *const pbi, struct aom_read_bit_buffer *const rb) {
  AV1_COMMON *const cm = &pbi->common;

  // This information is stored as a separate byte.
  int mod = rb->bit_offset % CHAR_BIT;
  if (mod > 0) aom_rb_read_literal(rb, CHAR_BIT - mod);
  assert(rb->bit_offset % CHAR_BIT == 0);

  if (cm->tiles.cols * cm->tiles.rows > 1) {
    // Read the number of bytes used to store tile size
    pbi->tile_col_size_bytes = aom_rb_read_literal(rb, 2) + 1;
    pbi->tile_size_bytes = aom_rb_read_literal(rb, 2) + 1;
  }
}
#endif  // EXT_TILE_DEBUG

static size_t mem_get_varsize(const uint8_t *src, int sz) {
  switch (sz) {
    case 1: return src[0];
    case 2: return mem_get_le16(src);
    case 3: return mem_get_le24(src);
    case 4: return mem_get_le32(src);
    default: assert(0 && "Invalid size"); return -1;
  }
}

#if EXT_TILE_DEBUG
// Reads the next tile returning its size and adjusting '*data' accordingly
// based on 'is_last'. On return, '*data' is updated to point to the end of the
// raw tile buffer in the bit stream.
static AOM_INLINE void get_ls_tile_buffer(
    const uint8_t *const data_end, struct aom_internal_error_info *error_info,
    const uint8_t **data, TileBufferDec (*const tile_buffers)[MAX_TILE_COLS],
    int tile_size_bytes, int col, int row, int tile_copy_mode) {
  size_t size;

  size_t copy_size = 0;
  const uint8_t *copy_data = NULL;

  if (!read_is_valid(*data, tile_size_bytes, data_end))
    aom_internal_error(error_info, AOM_CODEC_CORRUPT_FRAME,
                       "Truncated packet or corrupt tile length");
  size = mem_get_varsize(*data, tile_size_bytes);

  // If tile_copy_mode = 1, then the top bit of the tile header indicates copy
  // mode.
  if (tile_copy_mode && (size >> (tile_size_bytes * 8 - 1)) == 1) {
    // The remaining bits in the top byte signal the row offset
    int offset = (size >> (tile_size_bytes - 1) * 8) & 0x7f;

    // Currently, only use tiles in same column as reference tiles.
    copy_data = tile_buffers[row - offset][col].data;
    copy_size = tile_buffers[row - offset][col].size;
    size = 0;
  } else {
    size += AV1_MIN_TILE_SIZE_BYTES;
  }

  *data += tile_size_bytes;

  if (size > (size_t)(data_end - *data))
    aom_internal_error(error_info, AOM_CODEC_CORRUPT_FRAME,
                       "Truncated packet or corrupt tile size");

  if (size > 0) {
    tile_buffers[row][col].data = *data;
    tile_buffers[row][col].size = size;
  } else {
    tile_buffers[row][col].data = copy_data;
    tile_buffers[row][col].size = copy_size;
  }

  *data += size;
}

// Returns the end of the last tile buffer
// (tile_buffers[cm->tiles.rows - 1][cm->tiles.cols - 1]).
static const uint8_t *get_ls_tile_buffers(
    AV1Decoder *pbi, const uint8_t *data, const uint8_t *data_end,
    TileBufferDec (*const tile_buffers)[MAX_TILE_COLS]) {
  AV1_COMMON *const cm = &pbi->common;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  const int have_tiles = tile_cols * tile_rows > 1;
  const uint8_t *raw_data_end;  // The end of the last tile buffer

  if (!have_tiles) {
    const size_t tile_size = data_end - data;
    tile_buffers[0][0].data = data;
    tile_buffers[0][0].size = tile_size;
    raw_data_end = NULL;
  } else {
    // We locate only the tile buffers that are required, which are the ones
    // specified by pbi->dec_tile_col and pbi->dec_tile_row. Also, we always
    // need the last (bottom right) tile buffer, as we need to know where the
    // end of the compressed frame buffer is for proper superframe decoding.

    const uint8_t *tile_col_data_end[MAX_TILE_COLS] = { NULL };
    const uint8_t *const data_start = data;

    const int dec_tile_row = AOMMIN(pbi->dec_tile_row, tile_rows);
    const int single_row = pbi->dec_tile_row >= 0;
    const int tile_rows_start = single_row ? dec_tile_row : 0;
    const int tile_rows_end = single_row ? tile_rows_start + 1 : tile_rows;
    const int dec_tile_col = AOMMIN(pbi->dec_tile_col, tile_cols);
    const int single_col = pbi->dec_tile_col >= 0;
    const int tile_cols_start = single_col ? dec_tile_col : 0;
    const int tile_cols_end = single_col ? tile_cols_start + 1 : tile_cols;

    const int tile_col_size_bytes = pbi->tile_col_size_bytes;
    const int tile_size_bytes = pbi->tile_size_bytes;
    int tile_width, tile_height;
    av1_get_uniform_tile_size(cm, &tile_width, &tile_height);
    const int tile_copy_mode =
        ((AOMMAX(tile_width, tile_height) << MI_SIZE_LOG2) <= 256) ? 1 : 0;
    // Read tile column sizes for all columns (we need the last tile buffer)
    for (int c = 0; c < tile_cols; ++c) {
      const int is_last = c == tile_cols - 1;
      size_t tile_col_size;

      if (!is_last) {
        tile_col_size = mem_get_varsize(data, tile_col_size_bytes);
        data += tile_col_size_bytes;
        tile_col_data_end[c] = data + tile_col_size;
      } else {
        tile_col_size = data_end - data;
        tile_col_data_end[c] = data_end;
      }
      data += tile_col_size;
    }

    data = data_start;

    // Read the required tile sizes.
    for (int c = tile_cols_start; c < tile_cols_end; ++c) {
      const int is_last = c == tile_cols - 1;

      if (c > 0) data = tile_col_data_end[c - 1];

      if (!is_last) data += tile_col_size_bytes;

      // Get the whole of the last column, otherwise stop at the required tile.
      for (int r = 0; r < (is_last ? tile_rows : tile_rows_end); ++r) {
        get_ls_tile_buffer(tile_col_data_end[c], &pbi->error, &data,
                           tile_buffers, tile_size_bytes, c, r, tile_copy_mode);
      }
    }

    // If we have not read the last column, then read it to get the last tile.
    if (tile_cols_end != tile_cols) {
      const int c = tile_cols - 1;

      data = tile_col_data_end[c - 1];

      for (int r = 0; r < tile_rows; ++r) {
        get_ls_tile_buffer(tile_col_data_end[c], &pbi->error, &data,
                           tile_buffers, tile_size_bytes, c, r, tile_copy_mode);
      }
    }
    raw_data_end = data;
  }
  return raw_data_end;
}
#endif  // EXT_TILE_DEBUG

static const uint8_t *get_ls_single_tile_buffer(
    AV1Decoder *pbi, const uint8_t *data,
    TileBufferDec (*const tile_buffers)[MAX_TILE_COLS]) {
  assert(pbi->dec_tile_row >= 0 && pbi->dec_tile_col >= 0);
  tile_buffers[pbi->dec_tile_row][pbi->dec_tile_col].data = data;
  tile_buffers[pbi->dec_tile_row][pbi->dec_tile_col].size =
      (size_t)pbi->coded_tile_data_size;
  return data + pbi->coded_tile_data_size;
}

// Reads the next tile returning its size and adjusting '*data' accordingly
// based on 'is_last'.
static AOM_INLINE void get_tile_buffer(
    const uint8_t *const data_end, const int tile_size_bytes, int is_last,
    struct aom_internal_error_info *error_info, const uint8_t **data,
    TileBufferDec *const buf) {
  size_t size;

  if (!is_last) {
    if (!read_is_valid(*data, tile_size_bytes, data_end))
      aom_internal_error(error_info, AOM_CODEC_CORRUPT_FRAME,
                         "Not enough data to read tile size");

    size = mem_get_varsize(*data, tile_size_bytes) + AV1_MIN_TILE_SIZE_BYTES;
    *data += tile_size_bytes;

    if (size > (size_t)(data_end - *data))
      aom_internal_error(error_info, AOM_CODEC_CORRUPT_FRAME,
                         "Truncated packet or corrupt tile size");
  } else {
    size = data_end - *data;
  }

  buf->data = *data;
  buf->size = size;

  *data += size;
}

static AOM_INLINE void get_tile_buffers(
    AV1Decoder *pbi, const uint8_t *data, const uint8_t *data_end,
    TileBufferDec (*const tile_buffers)[MAX_TILE_COLS], int start_tile,
    int end_tile) {
  AV1_COMMON *const cm = &pbi->common;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  int tc = 0;

  for (int r = 0; r < tile_rows; ++r) {
    for (int c = 0; c < tile_cols; ++c, ++tc) {
      TileBufferDec *const buf = &tile_buffers[r][c];

      const int is_last = (tc == end_tile);
      const size_t hdr_offset = 0;

      if (tc < start_tile || tc > end_tile) continue;

      if (data + hdr_offset >= data_end)
        aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                           "Data ended before all tiles were read.");
      data += hdr_offset;
      get_tile_buffer(data_end, pbi->tile_size_bytes, is_last, &pbi->error,
                      &data, buf);
    }
  }
}

static AOM_INLINE void set_cb_buffer(AV1Decoder *pbi, DecoderCodingBlock *dcb,
                                     CB_BUFFER *cb_buffer_base,
                                     const int num_planes, int mi_row,
                                     int mi_col) {
  AV1_COMMON *const cm = &pbi->common;
  int mib_size_log2 = cm->seq_params->mib_size_log2;
  int stride = (cm->mi_params.mi_cols >> mib_size_log2) + 1;
  int offset = (mi_row >> mib_size_log2) * stride + (mi_col >> mib_size_log2);
  CB_BUFFER *cb_buffer = cb_buffer_base + offset;

  for (int plane = 0; plane < num_planes; ++plane) {
    dcb->dqcoeff_block[plane] = cb_buffer->dqcoeff[plane];
    dcb->eob_data[plane] = cb_buffer->eob_data[plane];
    dcb->cb_offset[plane] = 0;
    dcb->txb_offset[plane] = 0;
  }
  MACROBLOCKD *const xd = &dcb->xd;
  xd->plane[0].color_index_map = cb_buffer->color_index_map[0];
  xd->plane[1].color_index_map = cb_buffer->color_index_map[1];
  xd->color_index_map_offset[0] = 0;
  xd->color_index_map_offset[1] = 0;
}

static AOM_INLINE void decoder_alloc_tile_data(AV1Decoder *pbi,
                                               const int n_tiles) {
  AV1_COMMON *const cm = &pbi->common;
  aom_free(pbi->tile_data);
  CHECK_MEM_ERROR(cm, pbi->tile_data,
                  aom_memalign(32, n_tiles * sizeof(*pbi->tile_data)));
  pbi->allocated_tiles = n_tiles;
  for (int i = 0; i < n_tiles; i++) {
    TileDataDec *const tile_data = pbi->tile_data + i;
    av1_zero(tile_data->dec_row_mt_sync);
  }
  pbi->allocated_row_mt_sync_rows = 0;
}

// Set up nsync by width.
static INLINE int get_sync_range(int width) {
// nsync numbers are picked by testing.
#if 0
  if (width < 640)
    return 1;
  else if (width <= 1280)
    return 2;
  else if (width <= 4096)
    return 4;
  else
    return 8;
#else
  (void)width;
#endif
  return 1;
}

// Allocate memory for decoder row synchronization
static AOM_INLINE void dec_row_mt_alloc(AV1DecRowMTSync *dec_row_mt_sync,
                                        AV1_COMMON *cm, int rows) {
  dec_row_mt_sync->allocated_sb_rows = rows;
#if CONFIG_MULTITHREAD
  {
    int i;

    CHECK_MEM_ERROR(cm, dec_row_mt_sync->mutex_,
                    aom_malloc(sizeof(*(dec_row_mt_sync->mutex_)) * rows));
    if (dec_row_mt_sync->mutex_) {
      for (i = 0; i < rows; ++i) {
        pthread_mutex_init(&dec_row_mt_sync->mutex_[i], NULL);
      }
    }

    CHECK_MEM_ERROR(cm, dec_row_mt_sync->cond_,
                    aom_malloc(sizeof(*(dec_row_mt_sync->cond_)) * rows));
    if (dec_row_mt_sync->cond_) {
      for (i = 0; i < rows; ++i) {
        pthread_cond_init(&dec_row_mt_sync->cond_[i], NULL);
      }
    }
  }
#endif  // CONFIG_MULTITHREAD

  CHECK_MEM_ERROR(cm, dec_row_mt_sync->cur_sb_col,
                  aom_malloc(sizeof(*(dec_row_mt_sync->cur_sb_col)) * rows));

  // Set up nsync.
  dec_row_mt_sync->sync_range = get_sync_range(cm->width);
}

// Deallocate decoder row synchronization related mutex and data
void av1_dec_row_mt_dealloc(AV1DecRowMTSync *dec_row_mt_sync) {
  if (dec_row_mt_sync != NULL) {
#if CONFIG_MULTITHREAD
    int i;
    if (dec_row_mt_sync->mutex_ != NULL) {
      for (i = 0; i < dec_row_mt_sync->allocated_sb_rows; ++i) {
        pthread_mutex_destroy(&dec_row_mt_sync->mutex_[i]);
      }
      aom_free(dec_row_mt_sync->mutex_);
    }
    if (dec_row_mt_sync->cond_ != NULL) {
      for (i = 0; i < dec_row_mt_sync->allocated_sb_rows; ++i) {
        pthread_cond_destroy(&dec_row_mt_sync->cond_[i]);
      }
      aom_free(dec_row_mt_sync->cond_);
    }
#endif  // CONFIG_MULTITHREAD
    aom_free(dec_row_mt_sync->cur_sb_col);

    // clear the structure as the source of this call may be a resize in which
    // case this call will be followed by an _alloc() which may fail.
    av1_zero(*dec_row_mt_sync);
  }
}

static INLINE void sync_read(AV1DecRowMTSync *const dec_row_mt_sync, int r,
                             int c) {
#if CONFIG_MULTITHREAD
  const int nsync = dec_row_mt_sync->sync_range;

  if (r && !(c & (nsync - 1))) {
    pthread_mutex_t *const mutex = &dec_row_mt_sync->mutex_[r - 1];
    pthread_mutex_lock(mutex);

    while (c > dec_row_mt_sync->cur_sb_col[r - 1] - nsync -
                   dec_row_mt_sync->intrabc_extra_top_right_sb_delay) {
      pthread_cond_wait(&dec_row_mt_sync->cond_[r - 1], mutex);
    }
    pthread_mutex_unlock(mutex);
  }
#else
  (void)dec_row_mt_sync;
  (void)r;
  (void)c;
#endif  // CONFIG_MULTITHREAD
}

static INLINE void sync_write(AV1DecRowMTSync *const dec_row_mt_sync, int r,
                              int c, const int sb_cols) {
#if CONFIG_MULTITHREAD
  const int nsync = dec_row_mt_sync->sync_range;
  int cur;
  int sig = 1;

  if (c < sb_cols - 1) {
    cur = c;
    if (c % nsync) sig = 0;
  } else {
    cur = sb_cols + nsync + dec_row_mt_sync->intrabc_extra_top_right_sb_delay;
  }

  if (sig) {
    pthread_mutex_lock(&dec_row_mt_sync->mutex_[r]);

    dec_row_mt_sync->cur_sb_col[r] = cur;

    pthread_cond_signal(&dec_row_mt_sync->cond_[r]);
    pthread_mutex_unlock(&dec_row_mt_sync->mutex_[r]);
  }
#else
  (void)dec_row_mt_sync;
  (void)r;
  (void)c;
  (void)sb_cols;
#endif  // CONFIG_MULTITHREAD
}

static INLINE void signal_decoding_done_for_erroneous_row(
    AV1Decoder *const pbi, const MACROBLOCKD *const xd) {
  AV1_COMMON *const cm = &pbi->common;
  const TileInfo *const tile = &xd->tile;
  const int sb_row_in_tile =
      ((xd->mi_row - tile->mi_row_start) >> cm->seq_params->mib_size_log2);
  const int sb_cols_in_tile = av1_get_sb_cols_in_tile(cm, tile);
  TileDataDec *const tile_data =
      pbi->tile_data + tile->tile_row * cm->tiles.cols + tile->tile_col;
  AV1DecRowMTSync *dec_row_mt_sync = &tile_data->dec_row_mt_sync;

  sync_write(dec_row_mt_sync, sb_row_in_tile, sb_cols_in_tile - 1,
             sb_cols_in_tile);
}

static AOM_INLINE void decode_tile_sb_row(AV1Decoder *pbi, ThreadData *const td,
                                          const TileInfo *tile_info,
                                          const int mi_row) {
  AV1_COMMON *const cm = &pbi->common;
  const int num_planes = av1_num_planes(cm);
  TileDataDec *const tile_data = pbi->tile_data +
                                 tile_info->tile_row * cm->tiles.cols +
                                 tile_info->tile_col;
  const int sb_cols_in_tile = av1_get_sb_cols_in_tile(cm, tile_info);
  const int sb_row_in_tile =
      (mi_row - tile_info->mi_row_start) >> cm->seq_params->mib_size_log2;
  int sb_col_in_tile = 0;
  int row_mt_exit = 0;

  for (int mi_col = tile_info->mi_col_start; mi_col < tile_info->mi_col_end;
       mi_col += cm->seq_params->mib_size, sb_col_in_tile++) {
    set_cb_buffer(pbi, &td->dcb, pbi->cb_buffer_base, num_planes, mi_row,
                  mi_col);

    sync_read(&tile_data->dec_row_mt_sync, sb_row_in_tile, sb_col_in_tile);

#if CONFIG_MULTITHREAD
    pthread_mutex_lock(pbi->row_mt_mutex_);
#endif
    row_mt_exit = pbi->frame_row_mt_info.row_mt_exit;
#if CONFIG_MULTITHREAD
    pthread_mutex_unlock(pbi->row_mt_mutex_);
#endif

    if (!row_mt_exit) {
      // Decoding of the super-block
      decode_partition(pbi, td, mi_row, mi_col, td->bit_reader,
                       cm->seq_params->sb_size, 0x2);
    }

    sync_write(&tile_data->dec_row_mt_sync, sb_row_in_tile, sb_col_in_tile,
               sb_cols_in_tile);
  }
}

static int check_trailing_bits_after_symbol_coder(aom_reader *r) {
  if (aom_reader_has_overflowed(r)) return -1;

  uint32_t nb_bits = aom_reader_tell(r);
  uint32_t nb_bytes = (nb_bits + 7) >> 3;
  const uint8_t *p = aom_reader_find_begin(r) + nb_bytes;

  // aom_reader_tell() returns 1 for a newly initialized decoder, and the
  // return value only increases as values are decoded. So nb_bits > 0, and
  // thus p > p_begin. Therefore accessing p[-1] is safe.
  uint8_t last_byte = p[-1];
  uint8_t pattern = 128 >> ((nb_bits - 1) & 7);
  if ((last_byte & (2 * pattern - 1)) != pattern) return -1;

  // Make sure that all padding bytes are zero as required by the spec.
  const uint8_t *p_end = aom_reader_find_end(r);
  while (p < p_end) {
    if (*p != 0) return -1;
    p++;
  }
  return 0;
}

static AOM_INLINE void set_decode_func_pointers(ThreadData *td,
                                                int parse_decode_flag) {
  td->read_coeffs_tx_intra_block_visit = decode_block_void;
  td->predict_and_recon_intra_block_visit = decode_block_void;
  td->read_coeffs_tx_inter_block_visit = decode_block_void;
  td->inverse_tx_inter_block_visit = decode_block_void;
  td->predict_inter_block_visit = predict_inter_block_void;
  td->cfl_store_inter_block_visit = cfl_store_inter_block_void;

  if (parse_decode_flag & 0x1) {
    td->read_coeffs_tx_intra_block_visit = read_coeffs_tx_intra_block;
    td->read_coeffs_tx_inter_block_visit = av1_read_coeffs_txb_facade;
  }
  if (parse_decode_flag & 0x2) {
    td->predict_and_recon_intra_block_visit =
        predict_and_reconstruct_intra_block;
    td->inverse_tx_inter_block_visit = inverse_transform_inter_block;
    td->predict_inter_block_visit = predict_inter_block;
    td->cfl_store_inter_block_visit = cfl_store_inter_block;
  }
}

static AOM_INLINE void decode_tile(AV1Decoder *pbi, ThreadData *const td,
                                   int tile_row, int tile_col) {
  TileInfo tile_info;

  AV1_COMMON *const cm = &pbi->common;
  const int num_planes = av1_num_planes(cm);

  av1_tile_set_row(&tile_info, cm, tile_row);
  av1_tile_set_col(&tile_info, cm, tile_col);
  DecoderCodingBlock *const dcb = &td->dcb;
  MACROBLOCKD *const xd = &dcb->xd;

  av1_zero_above_context(cm, xd, tile_info.mi_col_start, tile_info.mi_col_end,
                         tile_row);
  av1_reset_loop_filter_delta(xd, num_planes);
  av1_reset_loop_restoration(xd, num_planes);

  for (int mi_row = tile_info.mi_row_start; mi_row < tile_info.mi_row_end;
       mi_row += cm->seq_params->mib_size) {
    av1_zero_left_context(xd);

    for (int mi_col = tile_info.mi_col_start; mi_col < tile_info.mi_col_end;
         mi_col += cm->seq_params->mib_size) {
      set_cb_buffer(pbi, dcb, &td->cb_buffer_base, num_planes, 0, 0);

      // Bit-stream parsing and decoding of the superblock
      decode_partition(pbi, td, mi_row, mi_col, td->bit_reader,
                       cm->seq_params->sb_size, 0x3);

      if (aom_reader_has_overflowed(td->bit_reader)) {
        aom_merge_corrupted_flag(&dcb->corrupted, 1);
        return;
      }
    }
  }

  int corrupted =
      (check_trailing_bits_after_symbol_coder(td->bit_reader)) ? 1 : 0;
  aom_merge_corrupted_flag(&dcb->corrupted, corrupted);
}

static const uint8_t *decode_tiles(AV1Decoder *pbi, const uint8_t *data,
                                   const uint8_t *data_end, int start_tile,
                                   int end_tile) {
  AV1_COMMON *const cm = &pbi->common;
  ThreadData *const td = &pbi->td;
  CommonTileParams *const tiles = &cm->tiles;
  const int tile_cols = tiles->cols;
  const int tile_rows = tiles->rows;
  const int n_tiles = tile_cols * tile_rows;
  TileBufferDec(*const tile_buffers)[MAX_TILE_COLS] = pbi->tile_buffers;
  const int dec_tile_row = AOMMIN(pbi->dec_tile_row, tile_rows);
  const int single_row = pbi->dec_tile_row >= 0;
  const int dec_tile_col = AOMMIN(pbi->dec_tile_col, tile_cols);
  const int single_col = pbi->dec_tile_col >= 0;
  int tile_rows_start;
  int tile_rows_end;
  int tile_cols_start;
  int tile_cols_end;
  int inv_col_order;
  int inv_row_order;
  int tile_row, tile_col;
  uint8_t allow_update_cdf;
  const uint8_t *raw_data_end = NULL;

  if (tiles->large_scale) {
    tile_rows_start = single_row ? dec_tile_row : 0;
    tile_rows_end = single_row ? dec_tile_row + 1 : tile_rows;
    tile_cols_start = single_col ? dec_tile_col : 0;
    tile_cols_end = single_col ? tile_cols_start + 1 : tile_cols;
    inv_col_order = pbi->inv_tile_order && !single_col;
    inv_row_order = pbi->inv_tile_order && !single_row;
    allow_update_cdf = 0;
  } else {
    tile_rows_start = 0;
    tile_rows_end = tile_rows;
    tile_cols_start = 0;
    tile_cols_end = tile_cols;
    inv_col_order = pbi->inv_tile_order;
    inv_row_order = pbi->inv_tile_order;
    allow_update_cdf = 1;
  }

  // No tiles to decode.
  if (tile_rows_end <= tile_rows_start || tile_cols_end <= tile_cols_start ||
      // First tile is larger than end_tile.
      tile_rows_start * tiles->cols + tile_cols_start > end_tile ||
      // Last tile is smaller than start_tile.
      (tile_rows_end - 1) * tiles->cols + tile_cols_end - 1 < start_tile)
    return data;

  allow_update_cdf = allow_update_cdf && !cm->features.disable_cdf_update;

  assert(tile_rows <= MAX_TILE_ROWS);
  assert(tile_cols <= MAX_TILE_COLS);

#if EXT_TILE_DEBUG
  if (tiles->large_scale && !pbi->ext_tile_debug)
    raw_data_end = get_ls_single_tile_buffer(pbi, data, tile_buffers);
  else if (tiles->large_scale && pbi->ext_tile_debug)
    raw_data_end = get_ls_tile_buffers(pbi, data, data_end, tile_buffers);
  else
#endif  // EXT_TILE_DEBUG
    get_tile_buffers(pbi, data, data_end, tile_buffers, start_tile, end_tile);

  if (pbi->tile_data == NULL || n_tiles != pbi->allocated_tiles) {
    decoder_alloc_tile_data(pbi, n_tiles);
  }
  if (pbi->dcb.xd.seg_mask == NULL)
    CHECK_MEM_ERROR(cm, pbi->dcb.xd.seg_mask,
                    (uint8_t *)aom_memalign(
                        16, 2 * MAX_SB_SQUARE * sizeof(*pbi->dcb.xd.seg_mask)));
#if CONFIG_ACCOUNTING
  if (pbi->acct_enabled) {
    aom_accounting_reset(&pbi->accounting);
  }
#endif

  set_decode_func_pointers(&pbi->td, 0x3);

  // Load all tile information into thread_data.
  td->dcb = pbi->dcb;

  td->dcb.corrupted = 0;
  td->dcb.mc_buf[0] = td->mc_buf[0];
  td->dcb.mc_buf[1] = td->mc_buf[1];
  td->dcb.xd.tmp_conv_dst = td->tmp_conv_dst;
  for (int j = 0; j < 2; ++j) {
    td->dcb.xd.tmp_obmc_bufs[j] = td->tmp_obmc_bufs[j];
  }

  for (tile_row = tile_rows_start; tile_row < tile_rows_end; ++tile_row) {
    const int row = inv_row_order ? tile_rows - 1 - tile_row : tile_row;

    for (tile_col = tile_cols_start; tile_col < tile_cols_end; ++tile_col) {
      const int col = inv_col_order ? tile_cols - 1 - tile_col : tile_col;
      TileDataDec *const tile_data = pbi->tile_data + row * tiles->cols + col;
      const TileBufferDec *const tile_bs_buf = &tile_buffers[row][col];

      if (row * tiles->cols + col < start_tile ||
          row * tiles->cols + col > end_tile)
        continue;

      td->bit_reader = &tile_data->bit_reader;
      av1_zero(td->cb_buffer_base.dqcoeff);
      av1_tile_init(&td->dcb.xd.tile, cm, row, col);
      td->dcb.xd.current_base_qindex = cm->quant_params.base_qindex;
      setup_bool_decoder(&td->dcb.xd, tile_bs_buf->data, data_end,
                         tile_bs_buf->size, &pbi->error, td->bit_reader,
                         allow_update_cdf);
#if CONFIG_ACCOUNTING
      if (pbi->acct_enabled) {
        td->bit_reader->accounting = &pbi->accounting;
        td->bit_reader->accounting->last_tell_frac =
            aom_reader_tell_frac(td->bit_reader);
      } else {
        td->bit_reader->accounting = NULL;
      }
#endif
      av1_init_macroblockd(cm, &td->dcb.xd);
      av1_init_above_context(&cm->above_contexts, av1_num_planes(cm), row,
                             &td->dcb.xd);

      // Initialise the tile context from the frame context
      tile_data->tctx = *cm->fc;
      td->dcb.xd.tile_ctx = &tile_data->tctx;

      // decode tile
      decode_tile(pbi, td, row, col);
      aom_merge_corrupted_flag(&pbi->dcb.corrupted, td->dcb.corrupted);
      if (pbi->dcb.corrupted)
        aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                           "Failed to decode tile data");
    }
  }

  if (tiles->large_scale) {
    if (n_tiles == 1) {
      // Find the end of the single tile buffer
      return aom_reader_find_end(&pbi->tile_data->bit_reader);
    }
    // Return the end of the last tile buffer
    return raw_data_end;
  }
  TileDataDec *const tile_data = pbi->tile_data + end_tile;

  return aom_reader_find_end(&tile_data->bit_reader);
}

static TileJobsDec *get_dec_job_info(AV1DecTileMT *tile_mt_info) {
  TileJobsDec *cur_job_info = NULL;
#if CONFIG_MULTITHREAD
  pthread_mutex_lock(tile_mt_info->job_mutex);

  if (tile_mt_info->jobs_dequeued < tile_mt_info->jobs_enqueued) {
    cur_job_info = tile_mt_info->job_queue + tile_mt_info->jobs_dequeued;
    tile_mt_info->jobs_dequeued++;
  }

  pthread_mutex_unlock(tile_mt_info->job_mutex);
#else
  (void)tile_mt_info;
#endif
  return cur_job_info;
}

static AOM_INLINE void tile_worker_hook_init(
    AV1Decoder *const pbi, DecWorkerData *const thread_data,
    const TileBufferDec *const tile_buffer, TileDataDec *const tile_data,
    uint8_t allow_update_cdf) {
  AV1_COMMON *cm = &pbi->common;
  ThreadData *const td = thread_data->td;
  int tile_row = tile_data->tile_info.tile_row;
  int tile_col = tile_data->tile_info.tile_col;

  td->bit_reader = &tile_data->bit_reader;
  av1_zero(td->cb_buffer_base.dqcoeff);

  MACROBLOCKD *const xd = &td->dcb.xd;
  av1_tile_init(&xd->tile, cm, tile_row, tile_col);
  xd->current_base_qindex = cm->quant_params.base_qindex;

  setup_bool_decoder(xd, tile_buffer->data, thread_data->data_end,
                     tile_buffer->size, &thread_data->error_info,
                     td->bit_reader, allow_update_cdf);
#if CONFIG_ACCOUNTING
  if (pbi->acct_enabled) {
    td->bit_reader->accounting = &pbi->accounting;
    td->bit_reader->accounting->last_tell_frac =
        aom_reader_tell_frac(td->bit_reader);
  } else {
    td->bit_reader->accounting = NULL;
  }
#endif
  av1_init_macroblockd(cm, xd);
  xd->error_info = &thread_data->error_info;
  av1_init_above_context(&cm->above_contexts, av1_num_planes(cm), tile_row, xd);

  // Initialise the tile context from the frame context
  tile_data->tctx = *cm->fc;
  xd->tile_ctx = &tile_data->tctx;
#if CONFIG_ACCOUNTING
  if (pbi->acct_enabled) {
    tile_data->bit_reader.accounting->last_tell_frac =
        aom_reader_tell_frac(&tile_data->bit_reader);
  }
#endif
}

static int tile_worker_hook(void *arg1, void *arg2) {
  DecWorkerData *const thread_data = (DecWorkerData *)arg1;
  AV1Decoder *const pbi = (AV1Decoder *)arg2;
  AV1_COMMON *cm = &pbi->common;
  ThreadData *const td = thread_data->td;
  uint8_t allow_update_cdf;

  // The jmp_buf is valid only for the duration of the function that calls
  // setjmp(). Therefore, this function must reset the 'setjmp' field to 0
  // before it returns.
  if (setjmp(thread_data->error_info.jmp)) {
    thread_data->error_info.setjmp = 0;
    thread_data->td->dcb.corrupted = 1;
    return 0;
  }
  thread_data->error_info.setjmp = 1;

  allow_update_cdf = cm->tiles.large_scale ? 0 : 1;
  allow_update_cdf = allow_update_cdf && !cm->features.disable_cdf_update;

  set_decode_func_pointers(td, 0x3);

  assert(cm->tiles.cols > 0);
  while (!td->dcb.corrupted) {
    TileJobsDec *cur_job_info = get_dec_job_info(&pbi->tile_mt_info);

    if (cur_job_info != NULL) {
      const TileBufferDec *const tile_buffer = cur_job_info->tile_buffer;
      TileDataDec *const tile_data = cur_job_info->tile_data;
      tile_worker_hook_init(pbi, thread_data, tile_buffer, tile_data,
                            allow_update_cdf);
      // decode tile
      int tile_row = tile_data->tile_info.tile_row;
      int tile_col = tile_data->tile_info.tile_col;
      decode_tile(pbi, td, tile_row, tile_col);
    } else {
      break;
    }
  }
  thread_data->error_info.setjmp = 0;
  return !td->dcb.corrupted;
}

static INLINE int get_max_row_mt_workers_per_tile(AV1_COMMON *cm,
                                                  const TileInfo *tile) {
  // NOTE: Currently value of max workers is calculated based
  // on the parse and decode time. As per the theoretical estimate
  // when percentage of parse time is equal to percentage of decode
  // time, number of workers needed to parse + decode a tile can not
  // exceed more than 2.
  // TODO(any): Modify this value if parsing is optimized in future.
  int sb_rows = av1_get_sb_rows_in_tile(cm, tile);
  int max_workers =
      sb_rows == 1 ? AOM_MIN_THREADS_PER_TILE : AOM_MAX_THREADS_PER_TILE;
  return max_workers;
}

// The caller must hold pbi->row_mt_mutex_ when calling this function.
// Returns 1 if either the next job is stored in *next_job_info or 1 is stored
// in *end_of_frame.
// NOTE: The caller waits on pbi->row_mt_cond_ if this function returns 0.
// The return value of this function depends on the following variables:
// - frame_row_mt_info->mi_rows_parse_done
// - frame_row_mt_info->mi_rows_decode_started
// - frame_row_mt_info->row_mt_exit
// Therefore we may need to signal or broadcast pbi->row_mt_cond_ if any of
// these variables is modified.
static int get_next_job_info(AV1Decoder *const pbi,
                             AV1DecRowMTJobInfo *next_job_info,
                             int *end_of_frame) {
  AV1_COMMON *cm = &pbi->common;
  TileDataDec *tile_data;
  AV1DecRowMTSync *dec_row_mt_sync;
  AV1DecRowMTInfo *frame_row_mt_info = &pbi->frame_row_mt_info;
  const int tile_rows_start = frame_row_mt_info->tile_rows_start;
  const int tile_rows_end = frame_row_mt_info->tile_rows_end;
  const int tile_cols_start = frame_row_mt_info->tile_cols_start;
  const int tile_cols_end = frame_row_mt_info->tile_cols_end;
  const int start_tile = frame_row_mt_info->start_tile;
  const int end_tile = frame_row_mt_info->end_tile;
  const int sb_mi_size = mi_size_wide[cm->seq_params->sb_size];
  int num_mis_to_decode, num_threads_working;
  int num_mis_waiting_for_decode;
  int min_threads_working = INT_MAX;
  int max_mis_to_decode = 0;
  int tile_row_idx, tile_col_idx;
  int tile_row = -1;
  int tile_col = -1;

  memset(next_job_info, 0, sizeof(*next_job_info));

  // Frame decode is completed or error is encountered.
  *end_of_frame = (frame_row_mt_info->mi_rows_decode_started ==
                   frame_row_mt_info->mi_rows_to_decode) ||
                  (frame_row_mt_info->row_mt_exit == 1);
  if (*end_of_frame) {
    return 1;
  }

  // Decoding cannot start as bit-stream parsing is not complete.
  assert(frame_row_mt_info->mi_rows_parse_done >=
         frame_row_mt_info->mi_rows_decode_started);
  if (frame_row_mt_info->mi_rows_parse_done ==
      frame_row_mt_info->mi_rows_decode_started)
    return 0;

  // Choose the tile to decode.
  for (tile_row_idx = tile_rows_start; tile_row_idx < tile_rows_end;
       ++tile_row_idx) {
    for (tile_col_idx = tile_cols_start; tile_col_idx < tile_cols_end;
         ++tile_col_idx) {
      if (tile_row_idx * cm->tiles.cols + tile_col_idx < start_tile ||
          tile_row_idx * cm->tiles.cols + tile_col_idx > end_tile)
        continue;

      tile_data = pbi->tile_data + tile_row_idx * cm->tiles.cols + tile_col_idx;
      dec_row_mt_sync = &tile_data->dec_row_mt_sync;

      num_threads_working = dec_row_mt_sync->num_threads_working;
      num_mis_waiting_for_decode = (dec_row_mt_sync->mi_rows_parse_done -
                                    dec_row_mt_sync->mi_rows_decode_started) *
                                   dec_row_mt_sync->mi_cols;
      num_mis_to_decode =
          (dec_row_mt_sync->mi_rows - dec_row_mt_sync->mi_rows_decode_started) *
          dec_row_mt_sync->mi_cols;

      assert(num_mis_to_decode >= num_mis_waiting_for_decode);

      // Pick the tile which has minimum number of threads working on it.
      if (num_mis_waiting_for_decode > 0) {
        if (num_threads_working < min_threads_working) {
          min_threads_working = num_threads_working;
          max_mis_to_decode = 0;
        }
        if (num_threads_working == min_threads_working &&
            num_mis_to_decode > max_mis_to_decode &&
            num_threads_working <
                get_max_row_mt_workers_per_tile(cm, &tile_data->tile_info)) {
          max_mis_to_decode = num_mis_to_decode;
          tile_row = tile_row_idx;
          tile_col = tile_col_idx;
        }
      }
    }
  }
  // No job found to process
  if (tile_row == -1 || tile_col == -1) return 0;

  tile_data = pbi->tile_data + tile_row * cm->tiles.cols + tile_col;
  dec_row_mt_sync = &tile_data->dec_row_mt_sync;

  next_job_info->tile_row = tile_row;
  next_job_info->tile_col = tile_col;
  next_job_info->mi_row = dec_row_mt_sync->mi_rows_decode_started +
                          tile_data->tile_info.mi_row_start;

  dec_row_mt_sync->num_threads_working++;
  dec_row_mt_sync->mi_rows_decode_started += sb_mi_size;
  frame_row_mt_info->mi_rows_decode_started += sb_mi_size;
  assert(frame_row_mt_info->mi_rows_parse_done >=
         frame_row_mt_info->mi_rows_decode_started);
#if CONFIG_MULTITHREAD
  if (frame_row_mt_info->mi_rows_decode_started ==
      frame_row_mt_info->mi_rows_to_decode) {
    pthread_cond_broadcast(pbi->row_mt_cond_);
  }
#endif

  return 1;
}

static INLINE void signal_parse_sb_row_done(AV1Decoder *const pbi,
                                            TileDataDec *const tile_data,
                                            const int sb_mi_size) {
  AV1DecRowMTInfo *frame_row_mt_info = &pbi->frame_row_mt_info;
#if CONFIG_MULTITHREAD
  pthread_mutex_lock(pbi->row_mt_mutex_);
#endif
  assert(frame_row_mt_info->mi_rows_parse_done >=
         frame_row_mt_info->mi_rows_decode_started);
  tile_data->dec_row_mt_sync.mi_rows_parse_done += sb_mi_size;
  frame_row_mt_info->mi_rows_parse_done += sb_mi_size;
#if CONFIG_MULTITHREAD
  // A new decode job is available. Wake up one worker thread to handle the
  // new decode job.
  // NOTE: This assumes we bump mi_rows_parse_done and mi_rows_decode_started
  // by the same increment (sb_mi_size).
  pthread_cond_signal(pbi->row_mt_cond_);
  pthread_mutex_unlock(pbi->row_mt_mutex_);
#endif
}

// This function is very similar to decode_tile(). It would be good to figure
// out how to share code.
static AOM_INLINE void parse_tile_row_mt(AV1Decoder *pbi, ThreadData *const td,
                                         TileDataDec *const tile_data) {
  AV1_COMMON *const cm = &pbi->common;
  const int sb_mi_size = mi_size_wide[cm->seq_params->sb_size];
  const int num_planes = av1_num_planes(cm);
  const TileInfo *const tile_info = &tile_data->tile_info;
  int tile_row = tile_info->tile_row;
  DecoderCodingBlock *const dcb = &td->dcb;
  MACROBLOCKD *const xd = &dcb->xd;

  av1_zero_above_context(cm, xd, tile_info->mi_col_start, tile_info->mi_col_end,
                         tile_row);
  av1_reset_loop_filter_delta(xd, num_planes);
  av1_reset_loop_restoration(xd, num_planes);

  for (int mi_row = tile_info->mi_row_start; mi_row < tile_info->mi_row_end;
       mi_row += cm->seq_params->mib_size) {
    av1_zero_left_context(xd);

    for (int mi_col = tile_info->mi_col_start; mi_col < tile_info->mi_col_end;
         mi_col += cm->seq_params->mib_size) {
      set_cb_buffer(pbi, dcb, pbi->cb_buffer_base, num_planes, mi_row, mi_col);

      // Bit-stream parsing of the superblock
      decode_partition(pbi, td, mi_row, mi_col, td->bit_reader,
                       cm->seq_params->sb_size, 0x1);

      if (aom_reader_has_overflowed(td->bit_reader)) {
        aom_merge_corrupted_flag(&dcb->corrupted, 1);
        return;
      }
    }
    signal_parse_sb_row_done(pbi, tile_data, sb_mi_size);
  }

  int corrupted =
      (check_trailing_bits_after_symbol_coder(td->bit_reader)) ? 1 : 0;
  aom_merge_corrupted_flag(&dcb->corrupted, corrupted);
}

static int row_mt_worker_hook(void *arg1, void *arg2) {
  DecWorkerData *const thread_data = (DecWorkerData *)arg1;
  AV1Decoder *const pbi = (AV1Decoder *)arg2;
  ThreadData *const td = thread_data->td;
  uint8_t allow_update_cdf;
  AV1DecRowMTInfo *frame_row_mt_info = &pbi->frame_row_mt_info;
  td->dcb.corrupted = 0;

  // The jmp_buf is valid only for the duration of the function that calls
  // setjmp(). Therefore, this function must reset the 'setjmp' field to 0
  // before it returns.
  if (setjmp(thread_data->error_info.jmp)) {
    thread_data->error_info.setjmp = 0;
    thread_data->td->dcb.corrupted = 1;
#if CONFIG_MULTITHREAD
    pthread_mutex_lock(pbi->row_mt_mutex_);
#endif
    frame_row_mt_info->row_mt_exit = 1;

    // If any SB row (erroneous row) processed by a thread encounters an
    // internal error, there is a need to indicate other threads that decoding
    // of the erroneous row is complete. This ensures that other threads which
    // wait upon the completion of SB's present in erroneous row are not waiting
    // indefinitely.
    signal_decoding_done_for_erroneous_row(pbi, &thread_data->td->dcb.xd);

#if CONFIG_MULTITHREAD
    pthread_cond_broadcast(pbi->row_mt_cond_);
    pthread_mutex_unlock(pbi->row_mt_mutex_);
#endif
    return 0;
  }
  thread_data->error_info.setjmp = 1;

  AV1_COMMON *cm = &pbi->common;
  allow_update_cdf = cm->tiles.large_scale ? 0 : 1;
  allow_update_cdf = allow_update_cdf && !cm->features.disable_cdf_update;

  set_decode_func_pointers(td, 0x1);

  assert(cm->tiles.cols > 0);
  while (!td->dcb.corrupted) {
    TileJobsDec *cur_job_info = get_dec_job_info(&pbi->tile_mt_info);

    if (cur_job_info != NULL) {
      const TileBufferDec *const tile_buffer = cur_job_info->tile_buffer;
      TileDataDec *const tile_data = cur_job_info->tile_data;
      tile_worker_hook_init(pbi, thread_data, tile_buffer, tile_data,
                            allow_update_cdf);
#if CONFIG_MULTITHREAD
      pthread_mutex_lock(pbi->row_mt_mutex_);
#endif
      tile_data->dec_row_mt_sync.num_threads_working++;
#if CONFIG_MULTITHREAD
      pthread_mutex_unlock(pbi->row_mt_mutex_);
#endif
      // decode tile
      parse_tile_row_mt(pbi, td, tile_data);
#if CONFIG_MULTITHREAD
      pthread_mutex_lock(pbi->row_mt_mutex_);
#endif
      tile_data->dec_row_mt_sync.num_threads_working--;
#if CONFIG_MULTITHREAD
      pthread_mutex_unlock(pbi->row_mt_mutex_);
#endif
    } else {
      break;
    }
  }

  if (td->dcb.corrupted) {
    thread_data->error_info.setjmp = 0;
#if CONFIG_MULTITHREAD
    pthread_mutex_lock(pbi->row_mt_mutex_);
#endif
    frame_row_mt_info->row_mt_exit = 1;
#if CONFIG_MULTITHREAD
    pthread_cond_broadcast(pbi->row_mt_cond_);
    pthread_mutex_unlock(pbi->row_mt_mutex_);
#endif
    return 0;
  }

  set_decode_func_pointers(td, 0x2);

  while (1) {
    AV1DecRowMTJobInfo next_job_info;
    int end_of_frame = 0;

#if CONFIG_MULTITHREAD
    pthread_mutex_lock(pbi->row_mt_mutex_);
#endif
    while (!get_next_job_info(pbi, &next_job_info, &end_of_frame)) {
#if CONFIG_MULTITHREAD
      pthread_cond_wait(pbi->row_mt_cond_, pbi->row_mt_mutex_);
#endif
    }
#if CONFIG_MULTITHREAD
    pthread_mutex_unlock(pbi->row_mt_mutex_);
#endif

    if (end_of_frame) break;

    int tile_row = next_job_info.tile_row;
    int tile_col = next_job_info.tile_col;
    int mi_row = next_job_info.mi_row;

    TileDataDec *tile_data =
        pbi->tile_data + tile_row * cm->tiles.cols + tile_col;
    AV1DecRowMTSync *dec_row_mt_sync = &tile_data->dec_row_mt_sync;

    av1_tile_init(&td->dcb.xd.tile, cm, tile_row, tile_col);
    av1_init_macroblockd(cm, &td->dcb.xd);
    td->dcb.xd.error_info = &thread_data->error_info;

    decode_tile_sb_row(pbi, td, &tile_data->tile_info, mi_row);

#if CONFIG_MULTITHREAD
    pthread_mutex_lock(pbi->row_mt_mutex_);
#endif
    dec_row_mt_sync->num_threads_working--;
#if CONFIG_MULTITHREAD
    pthread_mutex_unlock(pbi->row_mt_mutex_);
#endif
  }
  thread_data->error_info.setjmp = 0;
  return !td->dcb.corrupted;
}

// sorts in descending order
static int compare_tile_buffers(const void *a, const void *b) {
  const TileJobsDec *const buf1 = (const TileJobsDec *)a;
  const TileJobsDec *const buf2 = (const TileJobsDec *)b;
  return (((int)buf2->tile_buffer->size) - ((int)buf1->tile_buffer->size));
}

static AOM_INLINE void enqueue_tile_jobs(AV1Decoder *pbi, AV1_COMMON *cm,
                                         int tile_rows_start, int tile_rows_end,
                                         int tile_cols_start, int tile_cols_end,
                                         int start_tile, int end_tile) {
  AV1DecTileMT *tile_mt_info = &pbi->tile_mt_info;
  TileJobsDec *tile_job_queue = tile_mt_info->job_queue;
  tile_mt_info->jobs_enqueued = 0;
  tile_mt_info->jobs_dequeued = 0;

  for (int row = tile_rows_start; row < tile_rows_end; row++) {
    for (int col = tile_cols_start; col < tile_cols_end; col++) {
      if (row * cm->tiles.cols + col < start_tile ||
          row * cm->tiles.cols + col > end_tile)
        continue;
      tile_job_queue->tile_buffer = &pbi->tile_buffers[row][col];
      tile_job_queue->tile_data = pbi->tile_data + row * cm->tiles.cols + col;
      tile_job_queue++;
      tile_mt_info->jobs_enqueued++;
    }
  }
}

static AOM_INLINE void alloc_dec_jobs(AV1DecTileMT *tile_mt_info,
                                      AV1_COMMON *cm, int tile_rows,
                                      int tile_cols) {
  tile_mt_info->alloc_tile_rows = tile_rows;
  tile_mt_info->alloc_tile_cols = tile_cols;
  int num_tiles = tile_rows * tile_cols;
#if CONFIG_MULTITHREAD
  {
    CHECK_MEM_ERROR(cm, tile_mt_info->job_mutex,
                    aom_malloc(sizeof(*tile_mt_info->job_mutex) * num_tiles));

    for (int i = 0; i < num_tiles; i++) {
      pthread_mutex_init(&tile_mt_info->job_mutex[i], NULL);
    }
  }
#endif
  CHECK_MEM_ERROR(cm, tile_mt_info->job_queue,
                  aom_malloc(sizeof(*tile_mt_info->job_queue) * num_tiles));
}

void av1_free_mc_tmp_buf(ThreadData *thread_data) {
  int ref;
  for (ref = 0; ref < 2; ref++) {
    if (thread_data->mc_buf_use_highbd)
      aom_free(CONVERT_TO_SHORTPTR(thread_data->mc_buf[ref]));
    else
      aom_free(thread_data->mc_buf[ref]);
    thread_data->mc_buf[ref] = NULL;
  }
  thread_data->mc_buf_size = 0;
  thread_data->mc_buf_use_highbd = 0;

  aom_free(thread_data->tmp_conv_dst);
  thread_data->tmp_conv_dst = NULL;
  aom_free(thread_data->seg_mask);
  thread_data->seg_mask = NULL;
  for (int i = 0; i < 2; ++i) {
    aom_free(thread_data->tmp_obmc_bufs[i]);
    thread_data->tmp_obmc_bufs[i] = NULL;
  }
}

static AOM_INLINE void allocate_mc_tmp_buf(AV1_COMMON *const cm,
                                           ThreadData *thread_data,
                                           int buf_size, int use_highbd) {
  for (int ref = 0; ref < 2; ref++) {
    // The mc_buf/hbd_mc_buf must be zeroed to fix a intermittent valgrind error
    // 'Conditional jump or move depends on uninitialised value' from the loop
    // filter. Uninitialized reads in convolve function (e.g. horiz_4tap path in
    // av1_convolve_2d_sr_avx2()) from mc_buf/hbd_mc_buf are seen to be the
    // potential reason for this issue.
    if (use_highbd) {
      uint16_t *hbd_mc_buf;
      CHECK_MEM_ERROR(cm, hbd_mc_buf, (uint16_t *)aom_memalign(16, buf_size));
      memset(hbd_mc_buf, 0, buf_size);
      thread_data->mc_buf[ref] = CONVERT_TO_BYTEPTR(hbd_mc_buf);
    } else {
      CHECK_MEM_ERROR(cm, thread_data->mc_buf[ref],
                      (uint8_t *)aom_memalign(16, buf_size));
      memset(thread_data->mc_buf[ref], 0, buf_size);
    }
  }
  thread_data->mc_buf_size = buf_size;
  thread_data->mc_buf_use_highbd = use_highbd;

  CHECK_MEM_ERROR(cm, thread_data->tmp_conv_dst,
                  aom_memalign(32, MAX_SB_SIZE * MAX_SB_SIZE *
                                       sizeof(*thread_data->tmp_conv_dst)));
  CHECK_MEM_ERROR(cm, thread_data->seg_mask,
                  (uint8_t *)aom_memalign(
                      16, 2 * MAX_SB_SQUARE * sizeof(*thread_data->seg_mask)));

  for (int i = 0; i < 2; ++i) {
    CHECK_MEM_ERROR(
        cm, thread_data->tmp_obmc_bufs[i],
        aom_memalign(16, 2 * MAX_MB_PLANE * MAX_SB_SQUARE *
                             sizeof(*thread_data->tmp_obmc_bufs[i])));
  }
}

static AOM_INLINE void reset_dec_workers(AV1Decoder *pbi,
                                         AVxWorkerHook worker_hook,
                                         int num_workers) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();

  // Reset tile decoding hook
  for (int worker_idx = 0; worker_idx < num_workers; ++worker_idx) {
    AVxWorker *const worker = &pbi->tile_workers[worker_idx];
    DecWorkerData *const thread_data = pbi->thread_data + worker_idx;
    thread_data->td->dcb = pbi->dcb;
    thread_data->td->dcb.corrupted = 0;
    thread_data->td->dcb.mc_buf[0] = thread_data->td->mc_buf[0];
    thread_data->td->dcb.mc_buf[1] = thread_data->td->mc_buf[1];
    thread_data->td->dcb.xd.tmp_conv_dst = thread_data->td->tmp_conv_dst;
    if (worker_idx)
      thread_data->td->dcb.xd.seg_mask = thread_data->td->seg_mask;
    for (int j = 0; j < 2; ++j) {
      thread_data->td->dcb.xd.tmp_obmc_bufs[j] =
          thread_data->td->tmp_obmc_bufs[j];
    }
    winterface->sync(worker);

    worker->hook = worker_hook;
    worker->data1 = thread_data;
    worker->data2 = pbi;
  }
#if CONFIG_ACCOUNTING
  if (pbi->acct_enabled) {
    aom_accounting_reset(&pbi->accounting);
  }
#endif
}

static AOM_INLINE void launch_dec_workers(AV1Decoder *pbi,
                                          const uint8_t *data_end,
                                          int num_workers) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();

  for (int worker_idx = num_workers - 1; worker_idx >= 0; --worker_idx) {
    AVxWorker *const worker = &pbi->tile_workers[worker_idx];
    DecWorkerData *const thread_data = (DecWorkerData *)worker->data1;

    thread_data->data_end = data_end;

    worker->had_error = 0;
    if (worker_idx == 0) {
      winterface->execute(worker);
    } else {
      winterface->launch(worker);
    }
  }
}

static AOM_INLINE void sync_dec_workers(AV1Decoder *pbi, int num_workers) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  int corrupted = 0;

  for (int worker_idx = num_workers; worker_idx > 0; --worker_idx) {
    AVxWorker *const worker = &pbi->tile_workers[worker_idx - 1];
    aom_merge_corrupted_flag(&corrupted, !winterface->sync(worker));
  }

  pbi->dcb.corrupted = corrupted;
}

static AOM_INLINE void decode_mt_init(AV1Decoder *pbi) {
  AV1_COMMON *const cm = &pbi->common;
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  int worker_idx;

  // Create workers and thread_data
  if (pbi->num_workers == 0) {
    const int num_threads = pbi->max_threads;
    CHECK_MEM_ERROR(cm, pbi->tile_workers,
                    aom_malloc(num_threads * sizeof(*pbi->tile_workers)));
    CHECK_MEM_ERROR(cm, pbi->thread_data,
                    aom_malloc(num_threads * sizeof(*pbi->thread_data)));

    for (worker_idx = 0; worker_idx < num_threads; ++worker_idx) {
      AVxWorker *const worker = &pbi->tile_workers[worker_idx];
      DecWorkerData *const thread_data = pbi->thread_data + worker_idx;
      ++pbi->num_workers;

      winterface->init(worker);
      worker->thread_name = "aom tile worker";
      if (worker_idx != 0 && !winterface->reset(worker)) {
        aom_internal_error(&pbi->error, AOM_CODEC_ERROR,
                           "Tile decoder thread creation failed");
      }

      if (worker_idx != 0) {
        // Allocate thread data.
        CHECK_MEM_ERROR(cm, thread_data->td,
                        aom_memalign(32, sizeof(*thread_data->td)));
        av1_zero(*thread_data->td);
      } else {
        // Main thread acts as a worker and uses the thread data in pbi
        thread_data->td = &pbi->td;
      }
      thread_data->error_info.error_code = AOM_CODEC_OK;
      thread_data->error_info.setjmp = 0;
    }
  }
  const int use_highbd = cm->seq_params->use_highbitdepth;
  const int buf_size = MC_TEMP_BUF_PELS << use_highbd;
  for (worker_idx = 1; worker_idx < pbi->max_threads; ++worker_idx) {
    DecWorkerData *const thread_data = pbi->thread_data + worker_idx;
    if (thread_data->td->mc_buf_size != buf_size) {
      av1_free_mc_tmp_buf(thread_data->td);
      allocate_mc_tmp_buf(cm, thread_data->td, buf_size, use_highbd);
    }
  }
}

static AOM_INLINE void tile_mt_queue(AV1Decoder *pbi, int tile_cols,
                                     int tile_rows, int tile_rows_start,
                                     int tile_rows_end, int tile_cols_start,
                                     int tile_cols_end, int start_tile,
                                     int end_tile) {
  AV1_COMMON *const cm = &pbi->common;
  if (pbi->tile_mt_info.alloc_tile_cols != tile_cols ||
      pbi->tile_mt_info.alloc_tile_rows != tile_rows) {
    av1_dealloc_dec_jobs(&pbi->tile_mt_info);
    alloc_dec_jobs(&pbi->tile_mt_info, cm, tile_rows, tile_cols);
  }
  enqueue_tile_jobs(pbi, cm, tile_rows_start, tile_rows_end, tile_cols_start,
                    tile_cols_end, start_tile, end_tile);
  qsort(pbi->tile_mt_info.job_queue, pbi->tile_mt_info.jobs_enqueued,
        sizeof(pbi->tile_mt_info.job_queue[0]), compare_tile_buffers);
}

static const uint8_t *decode_tiles_mt(AV1Decoder *pbi, const uint8_t *data,
                                      const uint8_t *data_end, int start_tile,
                                      int end_tile) {
  AV1_COMMON *const cm = &pbi->common;
  CommonTileParams *const tiles = &cm->tiles;
  const int tile_cols = tiles->cols;
  const int tile_rows = tiles->rows;
  const int n_tiles = tile_cols * tile_rows;
  TileBufferDec(*const tile_buffers)[MAX_TILE_COLS] = pbi->tile_buffers;
  const int dec_tile_row = AOMMIN(pbi->dec_tile_row, tile_rows);
  const int single_row = pbi->dec_tile_row >= 0;
  const int dec_tile_col = AOMMIN(pbi->dec_tile_col, tile_cols);
  const int single_col = pbi->dec_tile_col >= 0;
  int tile_rows_start;
  int tile_rows_end;
  int tile_cols_start;
  int tile_cols_end;
  int tile_count_tg;
  int num_workers;
  const uint8_t *raw_data_end = NULL;

  if (tiles->large_scale) {
    tile_rows_start = single_row ? dec_tile_row : 0;
    tile_rows_end = single_row ? dec_tile_row + 1 : tile_rows;
    tile_cols_start = single_col ? dec_tile_col : 0;
    tile_cols_end = single_col ? tile_cols_start + 1 : tile_cols;
  } else {
    tile_rows_start = 0;
    tile_rows_end = tile_rows;
    tile_cols_start = 0;
    tile_cols_end = tile_cols;
  }
  tile_count_tg = end_tile - start_tile + 1;
  num_workers = AOMMIN(pbi->max_threads, tile_count_tg);

  // No tiles to decode.
  if (tile_rows_end <= tile_rows_start || tile_cols_end <= tile_cols_start ||
      // First tile is larger than end_tile.
      tile_rows_start * tile_cols + tile_cols_start > end_tile ||
      // Last tile is smaller than start_tile.
      (tile_rows_end - 1) * tile_cols + tile_cols_end - 1 < start_tile)
    return data;

  assert(tile_rows <= MAX_TILE_ROWS);
  assert(tile_cols <= MAX_TILE_COLS);
  assert(tile_count_tg > 0);
  assert(num_workers > 0);
  assert(start_tile <= end_tile);
  assert(start_tile >= 0 && end_tile < n_tiles);

  decode_mt_init(pbi);

  // get tile size in tile group
#if EXT_TILE_DEBUG
  if (tiles->large_scale) assert(pbi->ext_tile_debug == 1);
  if (tiles->large_scale)
    raw_data_end = get_ls_tile_buffers(pbi, data, data_end, tile_buffers);
  else
#endif  // EXT_TILE_DEBUG
    get_tile_buffers(pbi, data, data_end, tile_buffers, start_tile, end_tile);

  if (pbi->tile_data == NULL || n_tiles != pbi->allocated_tiles) {
    decoder_alloc_tile_data(pbi, n_tiles);
  }
  if (pbi->dcb.xd.seg_mask == NULL)
    CHECK_MEM_ERROR(cm, pbi->dcb.xd.seg_mask,
                    (uint8_t *)aom_memalign(
                        16, 2 * MAX_SB_SQUARE * sizeof(*pbi->dcb.xd.seg_mask)));

  for (int row = 0; row < tile_rows; row++) {
    for (int col = 0; col < tile_cols; col++) {
      TileDataDec *tile_data = pbi->tile_data + row * tiles->cols + col;
      av1_tile_init(&tile_data->tile_info, cm, row, col);
    }
  }

  tile_mt_queue(pbi, tile_cols, tile_rows, tile_rows_start, tile_rows_end,
                tile_cols_start, tile_cols_end, start_tile, end_tile);

  reset_dec_workers(pbi, tile_worker_hook, num_workers);
  launch_dec_workers(pbi, data_end, num_workers);
  sync_dec_workers(pbi, num_workers);

  if (pbi->dcb.corrupted)
    aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                       "Failed to decode tile data");

  if (tiles->large_scale) {
    if (n_tiles == 1) {
      // Find the end of the single tile buffer
      return aom_reader_find_end(&pbi->tile_data->bit_reader);
    }
    // Return the end of the last tile buffer
    return raw_data_end;
  }
  TileDataDec *const tile_data = pbi->tile_data + end_tile;

  return aom_reader_find_end(&tile_data->bit_reader);
}

static AOM_INLINE void dec_alloc_cb_buf(AV1Decoder *pbi) {
  AV1_COMMON *const cm = &pbi->common;
  int size = ((cm->mi_params.mi_rows >> cm->seq_params->mib_size_log2) + 1) *
             ((cm->mi_params.mi_cols >> cm->seq_params->mib_size_log2) + 1);

  if (pbi->cb_buffer_alloc_size < size) {
    av1_dec_free_cb_buf(pbi);
    CHECK_MEM_ERROR(cm, pbi->cb_buffer_base,
                    aom_memalign(32, sizeof(*pbi->cb_buffer_base) * size));
    memset(pbi->cb_buffer_base, 0, sizeof(*pbi->cb_buffer_base) * size);
    pbi->cb_buffer_alloc_size = size;
  }
}

static AOM_INLINE void row_mt_frame_init(AV1Decoder *pbi, int tile_rows_start,
                                         int tile_rows_end, int tile_cols_start,
                                         int tile_cols_end, int start_tile,
                                         int end_tile, int max_sb_rows) {
  AV1_COMMON *const cm = &pbi->common;
  AV1DecRowMTInfo *frame_row_mt_info = &pbi->frame_row_mt_info;

  frame_row_mt_info->tile_rows_start = tile_rows_start;
  frame_row_mt_info->tile_rows_end = tile_rows_end;
  frame_row_mt_info->tile_cols_start = tile_cols_start;
  frame_row_mt_info->tile_cols_end = tile_cols_end;
  frame_row_mt_info->start_tile = start_tile;
  frame_row_mt_info->end_tile = end_tile;
  frame_row_mt_info->mi_rows_to_decode = 0;
  frame_row_mt_info->mi_rows_parse_done = 0;
  frame_row_mt_info->mi_rows_decode_started = 0;
  frame_row_mt_info->row_mt_exit = 0;

  for (int tile_row = tile_rows_start; tile_row < tile_rows_end; ++tile_row) {
    for (int tile_col = tile_cols_start; tile_col < tile_cols_end; ++tile_col) {
      if (tile_row * cm->tiles.cols + tile_col < start_tile ||
          tile_row * cm->tiles.cols + tile_col > end_tile)
        continue;

      TileDataDec *const tile_data =
          pbi->tile_data + tile_row * cm->tiles.cols + tile_col;
      const TileInfo *const tile_info = &tile_data->tile_info;

      tile_data->dec_row_mt_sync.mi_rows_parse_done = 0;
      tile_data->dec_row_mt_sync.mi_rows_decode_started = 0;
      tile_data->dec_row_mt_sync.num_threads_working = 0;
      tile_data->dec_row_mt_sync.mi_rows =
          ALIGN_POWER_OF_TWO(tile_info->mi_row_end - tile_info->mi_row_start,
                             cm->seq_params->mib_size_log2);
      tile_data->dec_row_mt_sync.mi_cols =
          ALIGN_POWER_OF_TWO(tile_info->mi_col_end - tile_info->mi_col_start,
                             cm->seq_params->mib_size_log2);
      tile_data->dec_row_mt_sync.intrabc_extra_top_right_sb_delay =
          av1_get_intrabc_extra_top_right_sb_delay(cm);

      frame_row_mt_info->mi_rows_to_decode +=
          tile_data->dec_row_mt_sync.mi_rows;

      // Initialize cur_sb_col to -1 for all SB rows.
      memset(tile_data->dec_row_mt_sync.cur_sb_col, -1,
             sizeof(*tile_data->dec_row_mt_sync.cur_sb_col) * max_sb_rows);
    }
  }

#if CONFIG_MULTITHREAD
  if (pbi->row_mt_mutex_ == NULL) {
    CHECK_MEM_ERROR(cm, pbi->row_mt_mutex_,
                    aom_malloc(sizeof(*(pbi->row_mt_mutex_))));
    if (pbi->row_mt_mutex_) {
      pthread_mutex_init(pbi->row_mt_mutex_, NULL);
    }
  }

  if (pbi->row_mt_cond_ == NULL) {
    CHECK_MEM_ERROR(cm, pbi->row_mt_cond_,
                    aom_malloc(sizeof(*(pbi->row_mt_cond_))));
    if (pbi->row_mt_cond_) {
      pthread_cond_init(pbi->row_mt_cond_, NULL);
    }
  }
#endif
}

static const uint8_t *decode_tiles_row_mt(AV1Decoder *pbi, const uint8_t *data,
                                          const uint8_t *data_end,
                                          int start_tile, int end_tile) {
  AV1_COMMON *const cm = &pbi->common;
  CommonTileParams *const tiles = &cm->tiles;
  const int tile_cols = tiles->cols;
  const int tile_rows = tiles->rows;
  const int n_tiles = tile_cols * tile_rows;
  TileBufferDec(*const tile_buffers)[MAX_TILE_COLS] = pbi->tile_buffers;
  const int dec_tile_row = AOMMIN(pbi->dec_tile_row, tile_rows);
  const int single_row = pbi->dec_tile_row >= 0;
  const int dec_tile_col = AOMMIN(pbi->dec_tile_col, tile_cols);
  const int single_col = pbi->dec_tile_col >= 0;
  int tile_rows_start;
  int tile_rows_end;
  int tile_cols_start;
  int tile_cols_end;
  int tile_count_tg;
  int num_workers = 0;
  int max_threads;
  const uint8_t *raw_data_end = NULL;
  int max_sb_rows = 0;

  if (tiles->large_scale) {
    tile_rows_start = single_row ? dec_tile_row : 0;
    tile_rows_end = single_row ? dec_tile_row + 1 : tile_rows;
    tile_cols_start = single_col ? dec_tile_col : 0;
    tile_cols_end = single_col ? tile_cols_start + 1 : tile_cols;
  } else {
    tile_rows_start = 0;
    tile_rows_end = tile_rows;
    tile_cols_start = 0;
    tile_cols_end = tile_cols;
  }
  tile_count_tg = end_tile - start_tile + 1;
  max_threads = pbi->max_threads;

  // No tiles to decode.
  if (tile_rows_end <= tile_rows_start || tile_cols_end <= tile_cols_start ||
      // First tile is larger than end_tile.
      tile_rows_start * tile_cols + tile_cols_start > end_tile ||
      // Last tile is smaller than start_tile.
      (tile_rows_end - 1) * tile_cols + tile_cols_end - 1 < start_tile)
    return data;

  assert(tile_rows <= MAX_TILE_ROWS);
  assert(tile_cols <= MAX_TILE_COLS);
  assert(tile_count_tg > 0);
  assert(max_threads > 0);
  assert(start_tile <= end_tile);
  assert(start_tile >= 0 && end_tile < n_tiles);

  (void)tile_count_tg;

  decode_mt_init(pbi);

  // get tile size in tile group
#if EXT_TILE_DEBUG
  if (tiles->large_scale) assert(pbi->ext_tile_debug == 1);
  if (tiles->large_scale)
    raw_data_end = get_ls_tile_buffers(pbi, data, data_end, tile_buffers);
  else
#endif  // EXT_TILE_DEBUG
    get_tile_buffers(pbi, data, data_end, tile_buffers, start_tile, end_tile);

  if (pbi->tile_data == NULL || n_tiles != pbi->allocated_tiles) {
    if (pbi->tile_data != NULL) {
      for (int i = 0; i < pbi->allocated_tiles; i++) {
        TileDataDec *const tile_data = pbi->tile_data + i;
        av1_dec_row_mt_dealloc(&tile_data->dec_row_mt_sync);
      }
    }
    decoder_alloc_tile_data(pbi, n_tiles);
  }
  if (pbi->dcb.xd.seg_mask == NULL)
    CHECK_MEM_ERROR(cm, pbi->dcb.xd.seg_mask,
                    (uint8_t *)aom_memalign(
                        16, 2 * MAX_SB_SQUARE * sizeof(*pbi->dcb.xd.seg_mask)));

  for (int row = 0; row < tile_rows; row++) {
    for (int col = 0; col < tile_cols; col++) {
      TileDataDec *tile_data = pbi->tile_data + row * tiles->cols + col;
      av1_tile_init(&tile_data->tile_info, cm, row, col);

      max_sb_rows = AOMMAX(max_sb_rows,
                           av1_get_sb_rows_in_tile(cm, &tile_data->tile_info));
      num_workers += get_max_row_mt_workers_per_tile(cm, &tile_data->tile_info);
    }
  }
  num_workers = AOMMIN(num_workers, max_threads);

  if (pbi->allocated_row_mt_sync_rows != max_sb_rows) {
    for (int i = 0; i < n_tiles; ++i) {
      TileDataDec *const tile_data = pbi->tile_data + i;
      av1_dec_row_mt_dealloc(&tile_data->dec_row_mt_sync);
      dec_row_mt_alloc(&tile_data->dec_row_mt_sync, cm, max_sb_rows);
    }
    pbi->allocated_row_mt_sync_rows = max_sb_rows;
  }

  tile_mt_queue(pbi, tile_cols, tile_rows, tile_rows_start, tile_rows_end,
                tile_cols_start, tile_cols_end, start_tile, end_tile);

  dec_alloc_cb_buf(pbi);

  row_mt_frame_init(pbi, tile_rows_start, tile_rows_end, tile_cols_start,
                    tile_cols_end, start_tile, end_tile, max_sb_rows);

  reset_dec_workers(pbi, row_mt_worker_hook, num_workers);
  launch_dec_workers(pbi, data_end, num_workers);
  sync_dec_workers(pbi, num_workers);

  if (pbi->dcb.corrupted)
    aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                       "Failed to decode tile data");

  if (tiles->large_scale) {
    if (n_tiles == 1) {
      // Find the end of the single tile buffer
      return aom_reader_find_end(&pbi->tile_data->bit_reader);
    }
    // Return the end of the last tile buffer
    return raw_data_end;
  }
  TileDataDec *const tile_data = pbi->tile_data + end_tile;

  return aom_reader_find_end(&tile_data->bit_reader);
}

static AOM_INLINE void error_handler(void *data) {
  AV1_COMMON *const cm = (AV1_COMMON *)data;
  aom_internal_error(cm->error, AOM_CODEC_CORRUPT_FRAME, "Truncated packet");
}

// Reads the high_bitdepth and twelve_bit fields in color_config() and sets
// seq_params->bit_depth based on the values of those fields and
// seq_params->profile. Reports errors by calling rb->error_handler() or
// aom_internal_error().
static AOM_INLINE void read_bitdepth(
    struct aom_read_bit_buffer *rb, SequenceHeader *seq_params,
    struct aom_internal_error_info *error_info) {
  const int high_bitdepth = aom_rb_read_bit(rb);
  if (seq_params->profile == PROFILE_2 && high_bitdepth) {
    const int twelve_bit = aom_rb_read_bit(rb);
    seq_params->bit_depth = twelve_bit ? AOM_BITS_12 : AOM_BITS_10;
  } else if (seq_params->profile <= PROFILE_2) {
    seq_params->bit_depth = high_bitdepth ? AOM_BITS_10 : AOM_BITS_8;
  } else {
    aom_internal_error(error_info, AOM_CODEC_UNSUP_BITSTREAM,
                       "Unsupported profile/bit-depth combination");
  }
#if !CONFIG_AV1_HIGHBITDEPTH
  if (seq_params->bit_depth > AOM_BITS_8) {
    aom_internal_error(error_info, AOM_CODEC_UNSUP_BITSTREAM,
                       "Bit-depth %d not supported", seq_params->bit_depth);
  }
#endif
}

void av1_read_film_grain_params(AV1_COMMON *cm,
                                struct aom_read_bit_buffer *rb) {
  aom_film_grain_t *pars = &cm->film_grain_params;
  const SequenceHeader *const seq_params = cm->seq_params;

  pars->apply_grain = aom_rb_read_bit(rb);
  if (!pars->apply_grain) {
    memset(pars, 0, sizeof(*pars));
    return;
  }

  pars->random_seed = aom_rb_read_literal(rb, 16);
  if (cm->current_frame.frame_type == INTER_FRAME)
    pars->update_parameters = aom_rb_read_bit(rb);
  else
    pars->update_parameters = 1;

  pars->bit_depth = seq_params->bit_depth;

  if (!pars->update_parameters) {
    // inherit parameters from a previous reference frame
    int film_grain_params_ref_idx = aom_rb_read_literal(rb, 3);
    // Section 6.8.20: It is a requirement of bitstream conformance that
    // film_grain_params_ref_idx is equal to ref_frame_idx[ j ] for some value
    // of j in the range 0 to REFS_PER_FRAME - 1.
    int found = 0;
    for (int i = 0; i < INTER_REFS_PER_FRAME; ++i) {
      if (film_grain_params_ref_idx == cm->remapped_ref_idx[i]) {
        found = 1;
        break;
      }
    }
    if (!found) {
      aom_internal_error(cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                         "Invalid film grain reference idx %d. ref_frame_idx = "
                         "{%d, %d, %d, %d, %d, %d, %d}",
                         film_grain_params_ref_idx, cm->remapped_ref_idx[0],
                         cm->remapped_ref_idx[1], cm->remapped_ref_idx[2],
                         cm->remapped_ref_idx[3], cm->remapped_ref_idx[4],
                         cm->remapped_ref_idx[5], cm->remapped_ref_idx[6]);
    }
    RefCntBuffer *const buf = cm->ref_frame_map[film_grain_params_ref_idx];
    if (buf == NULL) {
      aom_internal_error(cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                         "Invalid Film grain reference idx");
    }
    if (!buf->film_grain_params_present) {
      aom_internal_error(cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                         "Film grain reference parameters not available");
    }
    uint16_t random_seed = pars->random_seed;
    *pars = buf->film_grain_params;   // inherit paramaters
    pars->random_seed = random_seed;  // with new random seed
    return;
  }

  // Scaling functions parameters
  pars->num_y_points = aom_rb_read_literal(rb, 4);  // max 14
  if (pars->num_y_points > 14)
    aom_internal_error(cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                       "Number of points for film grain luma scaling function "
                       "exceeds the maximum value.");
  for (int i = 0; i < pars->num_y_points; i++) {
    pars->scaling_points_y[i][0] = aom_rb_read_literal(rb, 8);
    if (i && pars->scaling_points_y[i - 1][0] >= pars->scaling_points_y[i][0])
      aom_internal_error(cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                         "First coordinate of the scaling function points "
                         "shall be increasing.");
    pars->scaling_points_y[i][1] = aom_rb_read_literal(rb, 8);
  }

  if (!seq_params->monochrome)
    pars->chroma_scaling_from_luma = aom_rb_read_bit(rb);
  else
    pars->chroma_scaling_from_luma = 0;

  if (seq_params->monochrome || pars->chroma_scaling_from_luma ||
      ((seq_params->subsampling_x == 1) && (seq_params->subsampling_y == 1) &&
       (pars->num_y_points == 0))) {
    pars->num_cb_points = 0;
    pars->num_cr_points = 0;
  } else {
    pars->num_cb_points = aom_rb_read_literal(rb, 4);  // max 10
    if (pars->num_cb_points > 10)
      aom_internal_error(cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                         "Number of points for film grain cb scaling function "
                         "exceeds the maximum value.");
    for (int i = 0; i < pars->num_cb_points; i++) {
      pars->scaling_points_cb[i][0] = aom_rb_read_literal(rb, 8);
      if (i &&
          pars->scaling_points_cb[i - 1][0] >= pars->scaling_points_cb[i][0])
        aom_internal_error(cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                           "First coordinate of the scaling function points "
                           "shall be increasing.");
      pars->scaling_points_cb[i][1] = aom_rb_read_literal(rb, 8);
    }

    pars->num_cr_points = aom_rb_read_literal(rb, 4);  // max 10
    if (pars->num_cr_points > 10)
      aom_internal_error(cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                         "Number of points for film grain cr scaling function "
                         "exceeds the maximum value.");
    for (int i = 0; i < pars->num_cr_points; i++) {
      pars->scaling_points_cr[i][0] = aom_rb_read_literal(rb, 8);
      if (i &&
          pars->scaling_points_cr[i - 1][0] >= pars->scaling_points_cr[i][0])
        aom_internal_error(cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                           "First coordinate of the scaling function points "
                           "shall be increasing.");
      pars->scaling_points_cr[i][1] = aom_rb_read_literal(rb, 8);
    }

    if ((seq_params->subsampling_x == 1) && (seq_params->subsampling_y == 1) &&
        (((pars->num_cb_points == 0) && (pars->num_cr_points != 0)) ||
         ((pars->num_cb_points != 0) && (pars->num_cr_points == 0))))
      aom_internal_error(cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                         "In YCbCr 4:2:0, film grain shall be applied "
                         "to both chroma components or neither.");
  }

  pars->scaling_shift = aom_rb_read_literal(rb, 2) + 8;  // 8 + value

  // AR coefficients
  // Only sent if the corresponsing scaling function has
  // more than 0 points

  pars->ar_coeff_lag = aom_rb_read_literal(rb, 2);

  int num_pos_luma = 2 * pars->ar_coeff_lag * (pars->ar_coeff_lag + 1);
  int num_pos_chroma = num_pos_luma;
  if (pars->num_y_points > 0) ++num_pos_chroma;

  if (pars->num_y_points)
    for (int i = 0; i < num_pos_luma; i++)
      pars->ar_coeffs_y[i] = aom_rb_read_literal(rb, 8) - 128;

  if (pars->num_cb_points || pars->chroma_scaling_from_luma)
    for (int i = 0; i < num_pos_chroma; i++)
      pars->ar_coeffs_cb[i] = aom_rb_read_literal(rb, 8) - 128;

  if (pars->num_cr_points || pars->chroma_scaling_from_luma)
    for (int i = 0; i < num_pos_chroma; i++)
      pars->ar_coeffs_cr[i] = aom_rb_read_literal(rb, 8) - 128;

  pars->ar_coeff_shift = aom_rb_read_literal(rb, 2) + 6;  // 6 + value

  pars->grain_scale_shift = aom_rb_read_literal(rb, 2);

  if (pars->num_cb_points) {
    pars->cb_mult = aom_rb_read_literal(rb, 8);
    pars->cb_luma_mult = aom_rb_read_literal(rb, 8);
    pars->cb_offset = aom_rb_read_literal(rb, 9);
  }

  if (pars->num_cr_points) {
    pars->cr_mult = aom_rb_read_literal(rb, 8);
    pars->cr_luma_mult = aom_rb_read_literal(rb, 8);
    pars->cr_offset = aom_rb_read_literal(rb, 9);
  }

  pars->overlap_flag = aom_rb_read_bit(rb);

  pars->clip_to_restricted_range = aom_rb_read_bit(rb);
}

static AOM_INLINE void read_film_grain(AV1_COMMON *cm,
                                       struct aom_read_bit_buffer *rb) {
  if (cm->seq_params->film_grain_params_present &&
      (cm->show_frame || cm->showable_frame)) {
    av1_read_film_grain_params(cm, rb);
  } else {
    memset(&cm->film_grain_params, 0, sizeof(cm->film_grain_params));
  }
  cm->film_grain_params.bit_depth = cm->seq_params->bit_depth;
  memcpy(&cm->cur_frame->film_grain_params, &cm->film_grain_params,
         sizeof(aom_film_grain_t));
}

void av1_read_color_config(struct aom_read_bit_buffer *rb,
                           int allow_lowbitdepth, SequenceHeader *seq_params,
                           struct aom_internal_error_info *error_info) {
  read_bitdepth(rb, seq_params, error_info);

  seq_params->use_highbitdepth =
      seq_params->bit_depth > AOM_BITS_8 || !allow_lowbitdepth;
  // monochrome bit (not needed for PROFILE_1)
  const int is_monochrome =
      seq_params->profile != PROFILE_1 ? aom_rb_read_bit(rb) : 0;
  seq_params->monochrome = is_monochrome;
  int color_description_present_flag = aom_rb_read_bit(rb);
  if (color_description_present_flag) {
    seq_params->color_primaries = aom_rb_read_literal(rb, 8);
    seq_params->transfer_characteristics = aom_rb_read_literal(rb, 8);
    seq_params->matrix_coefficients = aom_rb_read_literal(rb, 8);
  } else {
    seq_params->color_primaries = AOM_CICP_CP_UNSPECIFIED;
    seq_params->transfer_characteristics = AOM_CICP_TC_UNSPECIFIED;
    seq_params->matrix_coefficients = AOM_CICP_MC_UNSPECIFIED;
  }
  if (is_monochrome) {
    // [16,235] (including xvycc) vs [0,255] range
    seq_params->color_range = aom_rb_read_bit(rb);
    seq_params->subsampling_y = seq_params->subsampling_x = 1;
    seq_params->chroma_sample_position = AOM_CSP_UNKNOWN;
    seq_params->separate_uv_delta_q = 0;
    return;
  }
  if (seq_params->color_primaries == AOM_CICP_CP_BT_709 &&
      seq_params->transfer_characteristics == AOM_CICP_TC_SRGB &&
      seq_params->matrix_coefficients == AOM_CICP_MC_IDENTITY) {
    seq_params->subsampling_y = seq_params->subsampling_x = 0;
    seq_params->color_range = 1;  // assume full color-range
    if (!(seq_params->profile == PROFILE_1 ||
          (seq_params->profile == PROFILE_2 &&
           seq_params->bit_depth == AOM_BITS_12))) {
      aom_internal_error(
          error_info, AOM_CODEC_UNSUP_BITSTREAM,
          "sRGB colorspace not compatible with specified profile");
    }
  } else {
    // [16,235] (including xvycc) vs [0,255] range
    seq_params->color_range = aom_rb_read_bit(rb);
    if (seq_params->profile == PROFILE_0) {
      // 420 only
      seq_params->subsampling_x = seq_params->subsampling_y = 1;
    } else if (seq_params->profile == PROFILE_1) {
      // 444 only
      seq_params->subsampling_x = seq_params->subsampling_y = 0;
    } else {
      assert(seq_params->profile == PROFILE_2);
      if (seq_params->bit_depth == AOM_BITS_12) {
        seq_params->subsampling_x = aom_rb_read_bit(rb);
        if (seq_params->subsampling_x)
          seq_params->subsampling_y = aom_rb_read_bit(rb);  // 422 or 420
        else
          seq_params->subsampling_y = 0;  // 444
      } else {
        // 422
        seq_params->subsampling_x = 1;
        seq_params->subsampling_y = 0;
      }
    }
    if (seq_params->matrix_coefficients == AOM_CICP_MC_IDENTITY &&
        (seq_params->subsampling_x || seq_params->subsampling_y)) {
      aom_internal_error(
          error_info, AOM_CODEC_UNSUP_BITSTREAM,
          "Identity CICP Matrix incompatible with non 4:4:4 color sampling");
    }
    if (seq_params->subsampling_x && seq_params->subsampling_y) {
      seq_params->chroma_sample_position = aom_rb_read_literal(rb, 2);
    }
  }
  seq_params->separate_uv_delta_q = aom_rb_read_bit(rb);
}

void av1_read_timing_info_header(aom_timing_info_t *timing_info,
                                 struct aom_internal_error_info *error,
                                 struct aom_read_bit_buffer *rb) {
  timing_info->num_units_in_display_tick =
      aom_rb_read_unsigned_literal(rb,
                                   32);  // Number of units in a display tick
  timing_info->time_scale = aom_rb_read_unsigned_literal(rb, 32);  // Time scale
  if (timing_info->num_units_in_display_tick == 0 ||
      timing_info->time_scale == 0) {
    aom_internal_error(
        error, AOM_CODEC_UNSUP_BITSTREAM,
        "num_units_in_display_tick and time_scale must be greater than 0.");
  }
  timing_info->equal_picture_interval =
      aom_rb_read_bit(rb);  // Equal picture interval bit
  if (timing_info->equal_picture_interval) {
    const uint32_t num_ticks_per_picture_minus_1 = aom_rb_read_uvlc(rb);
    if (num_ticks_per_picture_minus_1 == UINT32_MAX) {
      aom_internal_error(
          error, AOM_CODEC_UNSUP_BITSTREAM,
          "num_ticks_per_picture_minus_1 cannot be (1 << 32) - 1.");
    }
    timing_info->num_ticks_per_picture = num_ticks_per_picture_minus_1 + 1;
  }
}

void av1_read_decoder_model_info(aom_dec_model_info_t *decoder_model_info,
                                 struct aom_read_bit_buffer *rb) {
  decoder_model_info->encoder_decoder_buffer_delay_length =
      aom_rb_read_literal(rb, 5) + 1;
  decoder_model_info->num_units_in_decoding_tick =
      aom_rb_read_unsigned_literal(rb,
                                   32);  // Number of units in a decoding tick
  decoder_model_info->buffer_removal_time_length =
      aom_rb_read_literal(rb, 5) + 1;
  decoder_model_info->frame_presentation_time_length =
      aom_rb_read_literal(rb, 5) + 1;
}

void av1_read_op_parameters_info(aom_dec_model_op_parameters_t *op_params,
                                 int buffer_delay_length,
                                 struct aom_read_bit_buffer *rb) {
  op_params->decoder_buffer_delay =
      aom_rb_read_unsigned_literal(rb, buffer_delay_length);
  op_params->encoder_buffer_delay =
      aom_rb_read_unsigned_literal(rb, buffer_delay_length);
  op_params->low_delay_mode_flag = aom_rb_read_bit(rb);
}

static AOM_INLINE void read_temporal_point_info(
    AV1_COMMON *const cm, struct aom_read_bit_buffer *rb) {
  cm->frame_presentation_time = aom_rb_read_unsigned_literal(
      rb, cm->seq_params->decoder_model_info.frame_presentation_time_length);
}

void av1_read_sequence_header(AV1_COMMON *cm, struct aom_read_bit_buffer *rb,
                              SequenceHeader *seq_params) {
  const int num_bits_width = aom_rb_read_literal(rb, 4) + 1;
  const int num_bits_height = aom_rb_read_literal(rb, 4) + 1;
  const int max_frame_width = aom_rb_read_literal(rb, num_bits_width) + 1;
  const int max_frame_height = aom_rb_read_literal(rb, num_bits_height) + 1;

  seq_params->num_bits_width = num_bits_width;
  seq_params->num_bits_height = num_bits_height;
  seq_params->max_frame_width = max_frame_width;
  seq_params->max_frame_height = max_frame_height;

  if (seq_params->reduced_still_picture_hdr) {
    seq_params->frame_id_numbers_present_flag = 0;
  } else {
    seq_params->frame_id_numbers_present_flag = aom_rb_read_bit(rb);
  }
  if (seq_params->frame_id_numbers_present_flag) {
    // We must always have delta_frame_id_length < frame_id_length,
    // in order for a frame to be referenced with a unique delta.
    // Avoid wasting bits by using a coding that enforces this restriction.
    seq_params->delta_frame_id_length = aom_rb_read_literal(rb, 4) + 2;
    seq_params->frame_id_length =
        aom_rb_read_literal(rb, 3) + seq_params->delta_frame_id_length + 1;
    if (seq_params->frame_id_length > 16)
      aom_internal_error(cm->error, AOM_CODEC_CORRUPT_FRAME,
                         "Invalid frame_id_length");
  }

  setup_sb_size(seq_params, rb);

  seq_params->enable_filter_intra = aom_rb_read_bit(rb);
  seq_params->enable_intra_edge_filter = aom_rb_read_bit(rb);

  if (seq_params->reduced_still_picture_hdr) {
    seq_params->enable_interintra_compound = 0;
    seq_params->enable_masked_compound = 0;
    seq_params->enable_warped_motion = 0;
    seq_params->enable_dual_filter = 0;
    seq_params->order_hint_info.enable_order_hint = 0;
    seq_params->order_hint_info.enable_dist_wtd_comp = 0;
    seq_params->order_hint_info.enable_ref_frame_mvs = 0;
    seq_params->force_screen_content_tools = 2;  // SELECT_SCREEN_CONTENT_TOOLS
    seq_params->force_integer_mv = 2;            // SELECT_INTEGER_MV
    seq_params->order_hint_info.order_hint_bits_minus_1 = -1;
  } else {
    seq_params->enable_interintra_compound = aom_rb_read_bit(rb);
    seq_params->enable_masked_compound = aom_rb_read_bit(rb);
    seq_params->enable_warped_motion = aom_rb_read_bit(rb);
    seq_params->enable_dual_filter = aom_rb_read_bit(rb);

    seq_params->order_hint_info.enable_order_hint = aom_rb_read_bit(rb);
    seq_params->order_hint_info.enable_dist_wtd_comp =
        seq_params->order_hint_info.enable_order_hint ? aom_rb_read_bit(rb) : 0;
    seq_params->order_hint_info.enable_ref_frame_mvs =
        seq_params->order_hint_info.enable_order_hint ? aom_rb_read_bit(rb) : 0;

    if (aom_rb_read_bit(rb)) {
      seq_params->force_screen_content_tools =
          2;  // SELECT_SCREEN_CONTENT_TOOLS
    } else {
      seq_params->force_screen_content_tools = aom_rb_read_bit(rb);
    }

    if (seq_params->force_screen_content_tools > 0) {
      if (aom_rb_read_bit(rb)) {
        seq_params->force_integer_mv = 2;  // SELECT_INTEGER_MV
      } else {
        seq_params->force_integer_mv = aom_rb_read_bit(rb);
      }
    } else {
      seq_params->force_integer_mv = 2;  // SELECT_INTEGER_MV
    }
    seq_params->order_hint_info.order_hint_bits_minus_1 =
        seq_params->order_hint_info.enable_order_hint
            ? aom_rb_read_literal(rb, 3)
            : -1;
  }

  seq_params->enable_superres = aom_rb_read_bit(rb);
  seq_params->enable_cdef = aom_rb_read_bit(rb);
  seq_params->enable_restoration = aom_rb_read_bit(rb);
}

static int read_global_motion_params(WarpedMotionParams *params,
                                     const WarpedMotionParams *ref_params,
                                     struct aom_read_bit_buffer *rb,
                                     int allow_hp) {
  TransformationType type = aom_rb_read_bit(rb);
  if (type != IDENTITY) {
    if (aom_rb_read_bit(rb))
      type = ROTZOOM;
    else
      type = aom_rb_read_bit(rb) ? TRANSLATION : AFFINE;
  }

  *params = default_warp_params;
  params->wmtype = type;

  if (type >= ROTZOOM) {
    params->wmmat[2] = aom_rb_read_signed_primitive_refsubexpfin(
                           rb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                           (ref_params->wmmat[2] >> GM_ALPHA_PREC_DIFF) -
                               (1 << GM_ALPHA_PREC_BITS)) *
                           GM_ALPHA_DECODE_FACTOR +
                       (1 << WARPEDMODEL_PREC_BITS);
    params->wmmat[3] = aom_rb_read_signed_primitive_refsubexpfin(
                           rb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                           (ref_params->wmmat[3] >> GM_ALPHA_PREC_DIFF)) *
                       GM_ALPHA_DECODE_FACTOR;
  }

  if (type >= AFFINE) {
    params->wmmat[4] = aom_rb_read_signed_primitive_refsubexpfin(
                           rb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                           (ref_params->wmmat[4] >> GM_ALPHA_PREC_DIFF)) *
                       GM_ALPHA_DECODE_FACTOR;
    params->wmmat[5] = aom_rb_read_signed_primitive_refsubexpfin(
                           rb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                           (ref_params->wmmat[5] >> GM_ALPHA_PREC_DIFF) -
                               (1 << GM_ALPHA_PREC_BITS)) *
                           GM_ALPHA_DECODE_FACTOR +
                       (1 << WARPEDMODEL_PREC_BITS);
  } else {
    params->wmmat[4] = -params->wmmat[3];
    params->wmmat[5] = params->wmmat[2];
  }

  if (type >= TRANSLATION) {
    const int trans_bits = (type == TRANSLATION)
                               ? GM_ABS_TRANS_ONLY_BITS - !allow_hp
                               : GM_ABS_TRANS_BITS;
    const int trans_dec_factor =
        (type == TRANSLATION) ? GM_TRANS_ONLY_DECODE_FACTOR * (1 << !allow_hp)
                              : GM_TRANS_DECODE_FACTOR;
    const int trans_prec_diff = (type == TRANSLATION)
                                    ? GM_TRANS_ONLY_PREC_DIFF + !allow_hp
                                    : GM_TRANS_PREC_DIFF;
    params->wmmat[0] = aom_rb_read_signed_primitive_refsubexpfin(
                           rb, (1 << trans_bits) + 1, SUBEXPFIN_K,
                           (ref_params->wmmat[0] >> trans_prec_diff)) *
                       trans_dec_factor;
    params->wmmat[1] = aom_rb_read_signed_primitive_refsubexpfin(
                           rb, (1 << trans_bits) + 1, SUBEXPFIN_K,
                           (ref_params->wmmat[1] >> trans_prec_diff)) *
                       trans_dec_factor;
  }

  if (params->wmtype <= AFFINE) {
    int good_shear_params = av1_get_shear_params(params);
    if (!good_shear_params) return 0;
  }

  return 1;
}

static AOM_INLINE void read_global_motion(AV1_COMMON *cm,
                                          struct aom_read_bit_buffer *rb) {
  for (int frame = LAST_FRAME; frame <= ALTREF_FRAME; ++frame) {
    const WarpedMotionParams *ref_params =
        cm->prev_frame ? &cm->prev_frame->global_motion[frame]
                       : &default_warp_params;
    int good_params =
        read_global_motion_params(&cm->global_motion[frame], ref_params, rb,
                                  cm->features.allow_high_precision_mv);
    if (!good_params) {
#if WARPED_MOTION_DEBUG
      printf("Warning: unexpected global motion shear params from aomenc\n");
#endif
      cm->global_motion[frame].invalid = 1;
    }

    // TODO(sarahparker, debargha): The logic in the commented out code below
    // does not work currently and causes mismatches when resize is on. Fix it
    // before turning the optimization back on.
    /*
    YV12_BUFFER_CONFIG *ref_buf = get_ref_frame(cm, frame);
    if (cm->width == ref_buf->y_crop_width &&
        cm->height == ref_buf->y_crop_height) {
      read_global_motion_params(&cm->global_motion[frame],
                                &cm->prev_frame->global_motion[frame], rb,
                                cm->features.allow_high_precision_mv);
    } else {
      cm->global_motion[frame] = default_warp_params;
    }
    */
    /*
    printf("Dec Ref %d [%d/%d]: %d %d %d %d\n",
           frame, cm->current_frame.frame_number, cm->show_frame,
           cm->global_motion[frame].wmmat[0],
           cm->global_motion[frame].wmmat[1],
           cm->global_motion[frame].wmmat[2],
           cm->global_motion[frame].wmmat[3]);
           */
  }
  memcpy(cm->cur_frame->global_motion, cm->global_motion,
         REF_FRAMES * sizeof(WarpedMotionParams));
}

// Release the references to the frame buffers in cm->ref_frame_map and reset
// all elements of cm->ref_frame_map to NULL.
static AOM_INLINE void reset_ref_frame_map(AV1_COMMON *const cm) {
  BufferPool *const pool = cm->buffer_pool;

  for (int i = 0; i < REF_FRAMES; i++) {
    decrease_ref_count(cm->ref_frame_map[i], pool);
    cm->ref_frame_map[i] = NULL;
  }
}

// If the refresh_frame_flags bitmask is set, update reference frame id values
// and mark frames as valid for reference.
static AOM_INLINE void update_ref_frame_id(AV1Decoder *const pbi) {
  AV1_COMMON *const cm = &pbi->common;
  int refresh_frame_flags = cm->current_frame.refresh_frame_flags;
  for (int i = 0; i < REF_FRAMES; i++) {
    if ((refresh_frame_flags >> i) & 1) {
      cm->ref_frame_id[i] = cm->current_frame_id;
      pbi->valid_for_referencing[i] = 1;
    }
  }
}

static AOM_INLINE void show_existing_frame_reset(AV1Decoder *const pbi,
                                                 int existing_frame_idx) {
  AV1_COMMON *const cm = &pbi->common;

  assert(cm->show_existing_frame);

  cm->current_frame.frame_type = KEY_FRAME;

  cm->current_frame.refresh_frame_flags = (1 << REF_FRAMES) - 1;

  for (int i = 0; i < INTER_REFS_PER_FRAME; ++i) {
    cm->remapped_ref_idx[i] = INVALID_IDX;
  }

  if (pbi->need_resync) {
    reset_ref_frame_map(cm);
    pbi->need_resync = 0;
  }

  // Note that the displayed frame must be valid for referencing in order to
  // have been selected.
  cm->current_frame_id = cm->ref_frame_id[existing_frame_idx];
  update_ref_frame_id(pbi);

  cm->features.refresh_frame_context = REFRESH_FRAME_CONTEXT_DISABLED;
}

static INLINE void reset_frame_buffers(AV1_COMMON *cm) {
  RefCntBuffer *const frame_bufs = cm->buffer_pool->frame_bufs;
  int i;

  lock_buffer_pool(cm->buffer_pool);
  reset_ref_frame_map(cm);
  assert(cm->cur_frame->ref_count == 1);
  for (i = 0; i < FRAME_BUFFERS; ++i) {
    // Reset all unreferenced frame buffers. We can also reset cm->cur_frame
    // because we are the sole owner of cm->cur_frame.
    if (frame_bufs[i].ref_count > 0 && &frame_bufs[i] != cm->cur_frame) {
      continue;
    }
    frame_bufs[i].order_hint = 0;
    av1_zero(frame_bufs[i].ref_order_hints);
  }
  av1_zero_unused_internal_frame_buffers(&cm->buffer_pool->int_frame_buffers);
  unlock_buffer_pool(cm->buffer_pool);
}

// On success, returns 0. On failure, calls aom_internal_error and does not
// return.
static int read_uncompressed_header(AV1Decoder *pbi,
                                    struct aom_read_bit_buffer *rb) {
  AV1_COMMON *const cm = &pbi->common;
  const SequenceHeader *const seq_params = cm->seq_params;
  CurrentFrame *const current_frame = &cm->current_frame;
  FeatureFlags *const features = &cm->features;
  MACROBLOCKD *const xd = &pbi->dcb.xd;
  BufferPool *const pool = cm->buffer_pool;
  RefCntBuffer *const frame_bufs = pool->frame_bufs;
  aom_s_frame_info *sframe_info = &pbi->sframe_info;
  sframe_info->is_s_frame = 0;
  sframe_info->is_s_frame_at_altref = 0;

  if (!pbi->sequence_header_ready) {
    aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                       "No sequence header");
  }

  if (seq_params->reduced_still_picture_hdr) {
    cm->show_existing_frame = 0;
    cm->show_frame = 1;
    current_frame->frame_type = KEY_FRAME;
    if (pbi->sequence_header_changed) {
      // This is the start of a new coded video sequence.
      pbi->sequence_header_changed = 0;
      pbi->decoding_first_frame = 1;
      reset_frame_buffers(cm);
    }
    features->error_resilient_mode = 1;
  } else {
    cm->show_existing_frame = aom_rb_read_bit(rb);
    pbi->reset_decoder_state = 0;

    if (cm->show_existing_frame) {
      if (pbi->sequence_header_changed) {
        aom_internal_error(
            &pbi->error, AOM_CODEC_CORRUPT_FRAME,
            "New sequence header starts with a show_existing_frame.");
      }
      // Show an existing frame directly.
      const int existing_frame_idx = aom_rb_read_literal(rb, 3);
      RefCntBuffer *const frame_to_show = cm->ref_frame_map[existing_frame_idx];
      if (frame_to_show == NULL) {
        aom_internal_error(&pbi->error, AOM_CODEC_UNSUP_BITSTREAM,
                           "Buffer does not contain a decoded frame");
      }
      if (seq_params->decoder_model_info_present_flag &&
          seq_params->timing_info.equal_picture_interval == 0) {
        read_temporal_point_info(cm, rb);
      }
      if (seq_params->frame_id_numbers_present_flag) {
        int frame_id_length = seq_params->frame_id_length;
        int display_frame_id = aom_rb_read_literal(rb, frame_id_length);
        /* Compare display_frame_id with ref_frame_id and check valid for
         * referencing */
        if (display_frame_id != cm->ref_frame_id[existing_frame_idx] ||
            pbi->valid_for_referencing[existing_frame_idx] == 0)
          aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                             "Reference buffer frame ID mismatch");
      }
      lock_buffer_pool(pool);
      assert(frame_to_show->ref_count > 0);
      // cm->cur_frame should be the buffer referenced by the return value
      // of the get_free_fb() call in assign_cur_frame_new_fb() (called by
      // av1_receive_compressed_data()), so the ref_count should be 1.
      assert(cm->cur_frame->ref_count == 1);
      // assign_frame_buffer_p() decrements ref_count directly rather than
      // call decrease_ref_count(). If cm->cur_frame->raw_frame_buffer has
      // already been allocated, it will not be released by
      // assign_frame_buffer_p()!
      assert(!cm->cur_frame->raw_frame_buffer.data);
      assign_frame_buffer_p(&cm->cur_frame, frame_to_show);
      pbi->reset_decoder_state = frame_to_show->frame_type == KEY_FRAME;
      unlock_buffer_pool(pool);

      cm->lf.filter_level[0] = 0;
      cm->lf.filter_level[1] = 0;
      cm->show_frame = 1;
      current_frame->order_hint = frame_to_show->order_hint;

      // Section 6.8.2: It is a requirement of bitstream conformance that when
      // show_existing_frame is used to show a previous frame, that the value
      // of showable_frame for the previous frame was equal to 1.
      if (!frame_to_show->showable_frame) {
        aom_internal_error(&pbi->error, AOM_CODEC_UNSUP_BITSTREAM,
                           "Buffer does not contain a showable frame");
      }
      // Section 6.8.2: It is a requirement of bitstream conformance that when
      // show_existing_frame is used to show a previous frame with
      // RefFrameType[ frame_to_show_map_idx ] equal to KEY_FRAME, that the
      // frame is output via the show_existing_frame mechanism at most once.
      if (pbi->reset_decoder_state) frame_to_show->showable_frame = 0;

      cm->film_grain_params = frame_to_show->film_grain_params;

      if (pbi->reset_decoder_state) {
        show_existing_frame_reset(pbi, existing_frame_idx);
      } else {
        current_frame->refresh_frame_flags = 0;
      }

      return 0;
    }

    current_frame->frame_type = (FRAME_TYPE)aom_rb_read_literal(rb, 2);
    if (pbi->sequence_header_changed) {
      if (current_frame->frame_type == KEY_FRAME) {
        // This is the start of a new coded video sequence.
        pbi->sequence_header_changed = 0;
        pbi->decoding_first_frame = 1;
        reset_frame_buffers(cm);
      } else {
        aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                           "Sequence header has changed without a keyframe.");
      }
    }

    cm->show_frame = aom_rb_read_bit(rb);
    if (cm->show_frame == 0) pbi->is_arf_frame_present = 1;
    if (cm->show_frame == 0 && cm->current_frame.frame_type == KEY_FRAME)
      pbi->is_fwd_kf_present = 1;
    if (cm->current_frame.frame_type == S_FRAME) {
      sframe_info->is_s_frame = 1;
      sframe_info->is_s_frame_at_altref = cm->show_frame ? 0 : 1;
    }
    if (seq_params->still_picture &&
        (current_frame->frame_type != KEY_FRAME || !cm->show_frame)) {
      aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                         "Still pictures must be coded as shown keyframes");
    }
    cm->showable_frame = current_frame->frame_type != KEY_FRAME;
    if (cm->show_frame) {
      if (seq_params->decoder_model_info_present_flag &&
          seq_params->timing_info.equal_picture_interval == 0)
        read_temporal_point_info(cm, rb);
    } else {
      // See if this frame can be used as show_existing_frame in future
      cm->showable_frame = aom_rb_read_bit(rb);
    }
    cm->cur_frame->showable_frame = cm->showable_frame;
    features->error_resilient_mode =
        frame_is_sframe(cm) ||
                (current_frame->frame_type == KEY_FRAME && cm->show_frame)
            ? 1
            : aom_rb_read_bit(rb);
  }

  if (current_frame->frame_type == KEY_FRAME && cm->show_frame) {
    /* All frames need to be marked as not valid for referencing */
    for (int i = 0; i < REF_FRAMES; i++) {
      pbi->valid_for_referencing[i] = 0;
    }
  }
  features->disable_cdf_update = aom_rb_read_bit(rb);
  if (seq_params->force_screen_content_tools == 2) {
    features->allow_screen_content_tools = aom_rb_read_bit(rb);
  } else {
    features->allow_screen_content_tools =
        seq_params->force_screen_content_tools;
  }

  if (features->allow_screen_content_tools) {
    if (seq_params->force_integer_mv == 2) {
      features->cur_frame_force_integer_mv = aom_rb_read_bit(rb);
    } else {
      features->cur_frame_force_integer_mv = seq_params->force_integer_mv;
    }
  } else {
    features->cur_frame_force_integer_mv = 0;
  }

  int frame_size_override_flag = 0;
  features->allow_intrabc = 0;
  features->primary_ref_frame = PRIMARY_REF_NONE;

  if (!seq_params->reduced_still_picture_hdr) {
    if (seq_params->frame_id_numbers_present_flag) {
      int frame_id_length = seq_params->frame_id_length;
      int diff_len = seq_params->delta_frame_id_length;
      int prev_frame_id = 0;
      int have_prev_frame_id =
          !pbi->decoding_first_frame &&
          !(current_frame->frame_type == KEY_FRAME && cm->show_frame);
      if (have_prev_frame_id) {
        prev_frame_id = cm->current_frame_id;
      }
      cm->current_frame_id = aom_rb_read_literal(rb, frame_id_length);

      if (have_prev_frame_id) {
        int diff_frame_id;
        if (cm->current_frame_id > prev_frame_id) {
          diff_frame_id = cm->current_frame_id - prev_frame_id;
        } else {
          diff_frame_id =
              (1 << frame_id_length) + cm->current_frame_id - prev_frame_id;
        }
        /* Check current_frame_id for conformance */
        if (prev_frame_id == cm->current_frame_id ||
            diff_frame_id >= (1 << (frame_id_length - 1))) {
          aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                             "Invalid value of current_frame_id");
        }
      }
      /* Check if some frames need to be marked as not valid for referencing */
      for (int i = 0; i < REF_FRAMES; i++) {
        if (cm->current_frame_id - (1 << diff_len) > 0) {
          if (cm->ref_frame_id[i] > cm->current_frame_id ||
              cm->ref_frame_id[i] < cm->current_frame_id - (1 << diff_len))
            pbi->valid_for_referencing[i] = 0;
        } else {
          if (cm->ref_frame_id[i] > cm->current_frame_id &&
              cm->ref_frame_id[i] < (1 << frame_id_length) +
                                        cm->current_frame_id - (1 << diff_len))
            pbi->valid_for_referencing[i] = 0;
        }
      }
    }

    frame_size_override_flag = frame_is_sframe(cm) ? 1 : aom_rb_read_bit(rb);

    current_frame->order_hint = aom_rb_read_literal(
        rb, seq_params->order_hint_info.order_hint_bits_minus_1 + 1);

    if (seq_params->order_hint_info.enable_order_hint)
      current_frame->frame_number = current_frame->order_hint;

    if (!features->error_resilient_mode && !frame_is_intra_only(cm)) {
      features->primary_ref_frame = aom_rb_read_literal(rb, PRIMARY_REF_BITS);
    }
  }

  if (seq_params->decoder_model_info_present_flag) {
    pbi->buffer_removal_time_present = aom_rb_read_bit(rb);
    if (pbi->buffer_removal_time_present) {
      for (int op_num = 0;
           op_num < seq_params->operating_points_cnt_minus_1 + 1; op_num++) {
        if (seq_params->op_params[op_num].decoder_model_param_present_flag) {
          if (seq_params->operating_point_idc[op_num] == 0 ||
              (((seq_params->operating_point_idc[op_num] >>
                 cm->temporal_layer_id) &
                0x1) &&
               ((seq_params->operating_point_idc[op_num] >>
                 (cm->spatial_layer_id + 8)) &
                0x1))) {
            cm->buffer_removal_times[op_num] = aom_rb_read_unsigned_literal(
                rb, seq_params->decoder_model_info.buffer_removal_time_length);
          } else {
            cm->buffer_removal_times[op_num] = 0;
          }
        } else {
          cm->buffer_removal_times[op_num] = 0;
        }
      }
    }
  }
  if (current_frame->frame_type == KEY_FRAME) {
    if (!cm->show_frame) {  // unshown keyframe (forward keyframe)
      current_frame->refresh_frame_flags = aom_rb_read_literal(rb, REF_FRAMES);
    } else {  // shown keyframe
      current_frame->refresh_frame_flags = (1 << REF_FRAMES) - 1;
    }

    for (int i = 0; i < INTER_REFS_PER_FRAME; ++i) {
      cm->remapped_ref_idx[i] = INVALID_IDX;
    }
    if (pbi->need_resync) {
      reset_ref_frame_map(cm);
      pbi->need_resync = 0;
    }
  } else {
    if (current_frame->frame_type == INTRA_ONLY_FRAME) {
      current_frame->refresh_frame_flags = aom_rb_read_literal(rb, REF_FRAMES);
      if (current_frame->refresh_frame_flags == 0xFF) {
        aom_internal_error(&pbi->error, AOM_CODEC_UNSUP_BITSTREAM,
                           "Intra only frames cannot have refresh flags 0xFF");
      }
      if (pbi->need_resync) {
        reset_ref_frame_map(cm);
        pbi->need_resync = 0;
      }
    } else if (pbi->need_resync != 1) { /* Skip if need resync */
      current_frame->refresh_frame_flags =
          frame_is_sframe(cm) ? 0xFF : aom_rb_read_literal(rb, REF_FRAMES);
    }
  }

  if (!frame_is_intra_only(cm) || current_frame->refresh_frame_flags != 0xFF) {
    // Read all ref frame order hints if error_resilient_mode == 1
    if (features->error_resilient_mode &&
        seq_params->order_hint_info.enable_order_hint) {
      for (int ref_idx = 0; ref_idx < REF_FRAMES; ref_idx++) {
        // Read order hint from bit stream
        unsigned int order_hint = aom_rb_read_literal(
            rb, seq_params->order_hint_info.order_hint_bits_minus_1 + 1);
        // Get buffer
        RefCntBuffer *buf = cm->ref_frame_map[ref_idx];
        if (buf == NULL || order_hint != buf->order_hint) {
          if (buf != NULL) {
            lock_buffer_pool(pool);
            decrease_ref_count(buf, pool);
            unlock_buffer_pool(pool);
            cm->ref_frame_map[ref_idx] = NULL;
          }
          // If no corresponding buffer exists, allocate a new buffer with all
          // pixels set to neutral grey.
          int buf_idx = get_free_fb(cm);
          if (buf_idx == INVALID_IDX) {
            aom_internal_error(&pbi->error, AOM_CODEC_MEM_ERROR,
                               "Unable to find free frame buffer");
          }
          buf = &frame_bufs[buf_idx];
          lock_buffer_pool(pool);
          if (aom_realloc_frame_buffer(
                  &buf->buf, seq_params->max_frame_width,
                  seq_params->max_frame_height, seq_params->subsampling_x,
                  seq_params->subsampling_y, seq_params->use_highbitdepth,
                  AOM_BORDER_IN_PIXELS, features->byte_alignment,
                  &buf->raw_frame_buffer, pool->get_fb_cb, pool->cb_priv, 0,
                  0)) {
            decrease_ref_count(buf, pool);
            unlock_buffer_pool(pool);
            aom_internal_error(&pbi->error, AOM_CODEC_MEM_ERROR,
                               "Failed to allocate frame buffer");
          }
          unlock_buffer_pool(pool);
          // According to the specification, valid bitstreams are required to
          // never use missing reference frames so the filling process for
          // missing frames is not normatively defined and RefValid for missing
          // frames is set to 0.

          // To make libaom more robust when the bitstream has been corrupted
          // by the loss of some frames of data, this code adds a neutral grey
          // buffer in place of missing frames, i.e.
          //
          set_planes_to_neutral_grey(seq_params, &buf->buf, 0);
          //
          // and allows the frames to be used for referencing, i.e.
          //
          pbi->valid_for_referencing[ref_idx] = 1;
          //
          // Please note such behavior is not normative and other decoders may
          // use a different approach.
          cm->ref_frame_map[ref_idx] = buf;
          buf->order_hint = order_hint;
        }
      }
    }
  }

  if (current_frame->frame_type == KEY_FRAME) {
    setup_frame_size(cm, frame_size_override_flag, rb);

    if (features->allow_screen_content_tools && !av1_superres_scaled(cm))
      features->allow_intrabc = aom_rb_read_bit(rb);
    features->allow_ref_frame_mvs = 0;
    cm->prev_frame = NULL;
  } else {
    features->allow_ref_frame_mvs = 0;

    if (current_frame->frame_type == INTRA_ONLY_FRAME) {
      cm->cur_frame->film_grain_params_present =
          seq_params->film_grain_params_present;
      setup_frame_size(cm, frame_size_override_flag, rb);
      if (features->allow_screen_content_tools && !av1_superres_scaled(cm))
        features->allow_intrabc = aom_rb_read_bit(rb);

    } else if (pbi->need_resync != 1) { /* Skip if need resync */
      int frame_refs_short_signaling = 0;
      // Frame refs short signaling is off when error resilient mode is on.
      if (seq_params->order_hint_info.enable_order_hint)
        frame_refs_short_signaling = aom_rb_read_bit(rb);

      if (frame_refs_short_signaling) {
        // == LAST_FRAME ==
        const int lst_ref = aom_rb_read_literal(rb, REF_FRAMES_LOG2);
        const RefCntBuffer *const lst_buf = cm->ref_frame_map[lst_ref];

        // == GOLDEN_FRAME ==
        const int gld_ref = aom_rb_read_literal(rb, REF_FRAMES_LOG2);
        const RefCntBuffer *const gld_buf = cm->ref_frame_map[gld_ref];

        // Most of the time, streams start with a keyframe. In that case,
        // ref_frame_map will have been filled in at that point and will not
        // contain any NULLs. However, streams are explicitly allowed to start
        // with an intra-only frame, so long as they don't then signal a
        // reference to a slot that hasn't been set yet. That's what we are
        // checking here.
        if (lst_buf == NULL)
          aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                             "Inter frame requests nonexistent reference");
        if (gld_buf == NULL)
          aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                             "Inter frame requests nonexistent reference");

        av1_set_frame_refs(cm, cm->remapped_ref_idx, lst_ref, gld_ref);
      }

      for (int i = 0; i < INTER_REFS_PER_FRAME; ++i) {
        int ref = 0;
        if (!frame_refs_short_signaling) {
          ref = aom_rb_read_literal(rb, REF_FRAMES_LOG2);

          // Most of the time, streams start with a keyframe. In that case,
          // ref_frame_map will have been filled in at that point and will not
          // contain any NULLs. However, streams are explicitly allowed to start
          // with an intra-only frame, so long as they don't then signal a
          // reference to a slot that hasn't been set yet. That's what we are
          // checking here.
          if (cm->ref_frame_map[ref] == NULL)
            aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                               "Inter frame requests nonexistent reference");
          cm->remapped_ref_idx[i] = ref;
        } else {
          ref = cm->remapped_ref_idx[i];
        }
        // Check valid for referencing
        if (pbi->valid_for_referencing[ref] == 0)
          aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                             "Reference frame not valid for referencing");

        cm->ref_frame_sign_bias[LAST_FRAME + i] = 0;

        if (seq_params->frame_id_numbers_present_flag) {
          int frame_id_length = seq_params->frame_id_length;
          int diff_len = seq_params->delta_frame_id_length;
          int delta_frame_id_minus_1 = aom_rb_read_literal(rb, diff_len);
          int ref_frame_id =
              ((cm->current_frame_id - (delta_frame_id_minus_1 + 1) +
                (1 << frame_id_length)) %
               (1 << frame_id_length));
          // Compare values derived from delta_frame_id_minus_1 and
          // refresh_frame_flags.
          if (ref_frame_id != cm->ref_frame_id[ref])
            aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                               "Reference buffer frame ID mismatch");
        }
      }

      if (!features->error_resilient_mode && frame_size_override_flag) {
        setup_frame_size_with_refs(cm, rb);
      } else {
        setup_frame_size(cm, frame_size_override_flag, rb);
      }

      if (features->cur_frame_force_integer_mv) {
        features->allow_high_precision_mv = 0;
      } else {
        features->allow_high_precision_mv = aom_rb_read_bit(rb);
      }
      features->interp_filter = read_frame_interp_filter(rb);
      features->switchable_motion_mode = aom_rb_read_bit(rb);
    }

    cm->prev_frame = get_primary_ref_frame_buf(cm);
    if (features->primary_ref_frame != PRIMARY_REF_NONE &&
        get_primary_ref_frame_buf(cm) == NULL) {
      aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                         "Reference frame containing this frame's initial "
                         "frame context is unavailable.");
    }

    if (!(current_frame->frame_type == INTRA_ONLY_FRAME) &&
        pbi->need_resync != 1) {
      if (frame_might_allow_ref_frame_mvs(cm))
        features->allow_ref_frame_mvs = aom_rb_read_bit(rb);
      else
        features->allow_ref_frame_mvs = 0;

      for (int i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
        const RefCntBuffer *const ref_buf = get_ref_frame_buf(cm, i);
        struct scale_factors *const ref_scale_factors =
            get_ref_scale_factors(cm, i);
        av1_setup_scale_factors_for_frame(
            ref_scale_factors, ref_buf->buf.y_crop_width,
            ref_buf->buf.y_crop_height, cm->width, cm->height);
        if ((!av1_is_valid_scale(ref_scale_factors)))
          aom_internal_error(&pbi->error, AOM_CODEC_UNSUP_BITSTREAM,
                             "Reference frame has invalid dimensions");
      }
    }
  }

  av1_setup_frame_buf_refs(cm);

  av1_setup_frame_sign_bias(cm);

  cm->cur_frame->frame_type = current_frame->frame_type;

  update_ref_frame_id(pbi);

  const int might_bwd_adapt = !(seq_params->reduced_still_picture_hdr) &&
                              !(features->disable_cdf_update);
  if (might_bwd_adapt) {
    features->refresh_frame_context = aom_rb_read_bit(rb)
                                          ? REFRESH_FRAME_CONTEXT_DISABLED
                                          : REFRESH_FRAME_CONTEXT_BACKWARD;
  } else {
    features->refresh_frame_context = REFRESH_FRAME_CONTEXT_DISABLED;
  }

  cm->cur_frame->buf.bit_depth = seq_params->bit_depth;
  cm->cur_frame->buf.color_primaries = seq_params->color_primaries;
  cm->cur_frame->buf.transfer_characteristics =
      seq_params->transfer_characteristics;
  cm->cur_frame->buf.matrix_coefficients = seq_params->matrix_coefficients;
  cm->cur_frame->buf.monochrome = seq_params->monochrome;
  cm->cur_frame->buf.chroma_sample_position =
      seq_params->chroma_sample_position;
  cm->cur_frame->buf.color_range = seq_params->color_range;
  cm->cur_frame->buf.render_width = cm->render_width;
  cm->cur_frame->buf.render_height = cm->render_height;

  if (pbi->need_resync) {
    aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                       "Keyframe / intra-only frame required to reset decoder"
                       " state");
  }

  if (features->allow_intrabc) {
    // Set parameters corresponding to no filtering.
    struct loopfilter *lf = &cm->lf;
    lf->filter_level[0] = 0;
    lf->filter_level[1] = 0;
    cm->cdef_info.cdef_bits = 0;
    cm->cdef_info.cdef_strengths[0] = 0;
    cm->cdef_info.nb_cdef_strengths = 1;
    cm->cdef_info.cdef_uv_strengths[0] = 0;
    cm->rst_info[0].frame_restoration_type = RESTORE_NONE;
    cm->rst_info[1].frame_restoration_type = RESTORE_NONE;
    cm->rst_info[2].frame_restoration_type = RESTORE_NONE;
  }

  read_tile_info(pbi, rb);
  if (!av1_is_min_tile_width_satisfied(cm)) {
    aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                       "Minimum tile width requirement not satisfied");
  }

  CommonQuantParams *const quant_params = &cm->quant_params;
  setup_quantization(quant_params, av1_num_planes(cm),
                     cm->seq_params->separate_uv_delta_q, rb);
  xd->bd = (int)seq_params->bit_depth;

  CommonContexts *const above_contexts = &cm->above_contexts;
  if (above_contexts->num_planes < av1_num_planes(cm) ||
      above_contexts->num_mi_cols < cm->mi_params.mi_cols ||
      above_contexts->num_tile_rows < cm->tiles.rows) {
    av1_free_above_context_buffers(above_contexts);
    if (av1_alloc_above_context_buffers(above_contexts, cm->tiles.rows,
                                        cm->mi_params.mi_cols,
                                        av1_num_planes(cm))) {
      aom_internal_error(&pbi->error, AOM_CODEC_MEM_ERROR,
                         "Failed to allocate context buffers");
    }
  }

  if (features->primary_ref_frame == PRIMARY_REF_NONE) {
    av1_setup_past_independence(cm);
  }

  setup_segmentation(cm, rb);

  cm->delta_q_info.delta_q_res = 1;
  cm->delta_q_info.delta_lf_res = 1;
  cm->delta_q_info.delta_lf_present_flag = 0;
  cm->delta_q_info.delta_lf_multi = 0;
  cm->delta_q_info.delta_q_present_flag =
      quant_params->base_qindex > 0 ? aom_rb_read_bit(rb) : 0;
  if (cm->delta_q_info.delta_q_present_flag) {
    xd->current_base_qindex = quant_params->base_qindex;
    cm->delta_q_info.delta_q_res = 1 << aom_rb_read_literal(rb, 2);
    if (!features->allow_intrabc)
      cm->delta_q_info.delta_lf_present_flag = aom_rb_read_bit(rb);
    if (cm->delta_q_info.delta_lf_present_flag) {
      cm->delta_q_info.delta_lf_res = 1 << aom_rb_read_literal(rb, 2);
      cm->delta_q_info.delta_lf_multi = aom_rb_read_bit(rb);
      av1_reset_loop_filter_delta(xd, av1_num_planes(cm));
    }
  }

  xd->cur_frame_force_integer_mv = features->cur_frame_force_integer_mv;

  for (int i = 0; i < MAX_SEGMENTS; ++i) {
    const int qindex = av1_get_qindex(&cm->seg, i, quant_params->base_qindex);
    xd->lossless[i] =
        qindex == 0 && quant_params->y_dc_delta_q == 0 &&
        quant_params->u_dc_delta_q == 0 && quant_params->u_ac_delta_q == 0 &&
        quant_params->v_dc_delta_q == 0 && quant_params->v_ac_delta_q == 0;
    xd->qindex[i] = qindex;
  }
  features->coded_lossless = is_coded_lossless(cm, xd);
  features->all_lossless = features->coded_lossless && !av1_superres_scaled(cm);
  setup_segmentation_dequant(cm, xd);
  if (features->coded_lossless) {
    cm->lf.filter_level[0] = 0;
    cm->lf.filter_level[1] = 0;
  }
  if (features->coded_lossless || !seq_params->enable_cdef) {
    cm->cdef_info.cdef_bits = 0;
    cm->cdef_info.cdef_strengths[0] = 0;
    cm->cdef_info.cdef_uv_strengths[0] = 0;
  }
  if (features->all_lossless || !seq_params->enable_restoration) {
    cm->rst_info[0].frame_restoration_type = RESTORE_NONE;
    cm->rst_info[1].frame_restoration_type = RESTORE_NONE;
    cm->rst_info[2].frame_restoration_type = RESTORE_NONE;
  }
  setup_loopfilter(cm, rb);

  if (!features->coded_lossless && seq_params->enable_cdef) {
    setup_cdef(cm, rb);
  }
  if (!features->all_lossless && seq_params->enable_restoration) {
    decode_restoration_mode(cm, rb);
  }

  features->tx_mode = read_tx_mode(rb, features->coded_lossless);
  current_frame->reference_mode = read_frame_reference_mode(cm, rb);

  av1_setup_skip_mode_allowed(cm);
  current_frame->skip_mode_info.skip_mode_flag =
      current_frame->skip_mode_info.skip_mode_allowed ? aom_rb_read_bit(rb) : 0;

  if (frame_might_allow_warped_motion(cm))
    features->allow_warped_motion = aom_rb_read_bit(rb);
  else
    features->allow_warped_motion = 0;

  features->reduced_tx_set_used = aom_rb_read_bit(rb);

  if (features->allow_ref_frame_mvs && !frame_might_allow_ref_frame_mvs(cm)) {
    aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                       "Frame wrongly requests reference frame MVs");
  }

  if (!frame_is_intra_only(cm)) read_global_motion(cm, rb);

  cm->cur_frame->film_grain_params_present =
      seq_params->film_grain_params_present;
  read_film_grain(cm, rb);

#if EXT_TILE_DEBUG
  if (pbi->ext_tile_debug && cm->tiles.large_scale) {
    read_ext_tile_info(pbi, rb);
    av1_set_single_tile_decoding_mode(cm);
  }
#endif  // EXT_TILE_DEBUG
  return 0;
}

struct aom_read_bit_buffer *av1_init_read_bit_buffer(
    AV1Decoder *pbi, struct aom_read_bit_buffer *rb, const uint8_t *data,
    const uint8_t *data_end) {
  rb->bit_offset = 0;
  rb->error_handler = error_handler;
  rb->error_handler_data = &pbi->common;
  rb->bit_buffer = data;
  rb->bit_buffer_end = data_end;
  return rb;
}

void av1_read_frame_size(struct aom_read_bit_buffer *rb, int num_bits_width,
                         int num_bits_height, int *width, int *height) {
  *width = aom_rb_read_literal(rb, num_bits_width) + 1;
  *height = aom_rb_read_literal(rb, num_bits_height) + 1;
}

BITSTREAM_PROFILE av1_read_profile(struct aom_read_bit_buffer *rb) {
  int profile = aom_rb_read_literal(rb, PROFILE_BITS);
  return (BITSTREAM_PROFILE)profile;
}

static AOM_INLINE void superres_post_decode(AV1Decoder *pbi) {
  AV1_COMMON *const cm = &pbi->common;
  BufferPool *const pool = cm->buffer_pool;

  if (!av1_superres_scaled(cm)) return;
  assert(!cm->features.all_lossless);

  av1_superres_upscale(cm, pool);
}

uint32_t av1_decode_frame_headers_and_setup(AV1Decoder *pbi,
                                            struct aom_read_bit_buffer *rb,
                                            int trailing_bits_present) {
  AV1_COMMON *const cm = &pbi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &pbi->dcb.xd;

#if CONFIG_BITSTREAM_DEBUG
  if (cm->seq_params->order_hint_info.enable_order_hint) {
    aom_bitstream_queue_set_frame_read(cm->current_frame.order_hint * 2 +
                                       cm->show_frame);
  } else {
    // This is currently used in RTC encoding. cm->show_frame is always 1.
    assert(cm->show_frame);
    aom_bitstream_queue_set_frame_read(cm->current_frame.frame_number);
  }
#endif
#if CONFIG_MISMATCH_DEBUG
  mismatch_move_frame_idx_r();
#endif

  for (int i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
    cm->global_motion[i] = default_warp_params;
    cm->cur_frame->global_motion[i] = default_warp_params;
  }
  xd->global_motion = cm->global_motion;

  read_uncompressed_header(pbi, rb);

  if (trailing_bits_present) av1_check_trailing_bits(pbi, rb);

  if (!cm->tiles.single_tile_decoding &&
      (pbi->dec_tile_row >= 0 || pbi->dec_tile_col >= 0)) {
    pbi->dec_tile_row = -1;
    pbi->dec_tile_col = -1;
  }

  const uint32_t uncomp_hdr_size =
      (uint32_t)aom_rb_bytes_read(rb);  // Size of the uncompressed header
  YV12_BUFFER_CONFIG *new_fb = &cm->cur_frame->buf;
  xd->cur_buf = new_fb;
  if (av1_allow_intrabc(cm)) {
    av1_setup_scale_factors_for_frame(
        &cm->sf_identity, xd->cur_buf->y_crop_width, xd->cur_buf->y_crop_height,
        xd->cur_buf->y_crop_width, xd->cur_buf->y_crop_height);
  }

  // Showing a frame directly.
  if (cm->show_existing_frame) {
    if (pbi->reset_decoder_state) {
      // Use the default frame context values.
      *cm->fc = *cm->default_frame_context;
      if (!cm->fc->initialized)
        aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                           "Uninitialized entropy context.");
    }
    return uncomp_hdr_size;
  }

  cm->mi_params.setup_mi(&cm->mi_params);

  av1_calculate_ref_frame_side(cm);
  if (cm->features.allow_ref_frame_mvs) av1_setup_motion_field(cm);

  av1_setup_block_planes(xd, cm->seq_params->subsampling_x,
                         cm->seq_params->subsampling_y, num_planes);
  if (cm->features.primary_ref_frame == PRIMARY_REF_NONE) {
    // use the default frame context values
    *cm->fc = *cm->default_frame_context;
  } else {
    *cm->fc = get_primary_ref_frame_buf(cm)->frame_context;
  }
  if (!cm->fc->initialized)
    aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                       "Uninitialized entropy context.");

  pbi->dcb.corrupted = 0;
  return uncomp_hdr_size;
}

// Once-per-frame initialization
static AOM_INLINE void setup_frame_info(AV1Decoder *pbi) {
  AV1_COMMON *const cm = &pbi->common;

  if (cm->rst_info[0].frame_restoration_type != RESTORE_NONE ||
      cm->rst_info[1].frame_restoration_type != RESTORE_NONE ||
      cm->rst_info[2].frame_restoration_type != RESTORE_NONE) {
    av1_alloc_restoration_buffers(cm);
  }

  const int use_highbd = cm->seq_params->use_highbitdepth;
  const int buf_size = MC_TEMP_BUF_PELS << use_highbd;
  if (pbi->td.mc_buf_size != buf_size) {
    av1_free_mc_tmp_buf(&pbi->td);
    allocate_mc_tmp_buf(cm, &pbi->td, buf_size, use_highbd);
  }
}

void av1_decode_tg_tiles_and_wrapup(AV1Decoder *pbi, const uint8_t *data,
                                    const uint8_t *data_end,
                                    const uint8_t **p_data_end, int start_tile,
                                    int end_tile, int initialize_flag) {
  AV1_COMMON *const cm = &pbi->common;
  CommonTileParams *const tiles = &cm->tiles;
  MACROBLOCKD *const xd = &pbi->dcb.xd;
  const int tile_count_tg = end_tile - start_tile + 1;

  if (initialize_flag) setup_frame_info(pbi);
  const int num_planes = av1_num_planes(cm);

  if (pbi->max_threads > 1 && !(tiles->large_scale && !pbi->ext_tile_debug) &&
      pbi->row_mt)
    *p_data_end =
        decode_tiles_row_mt(pbi, data, data_end, start_tile, end_tile);
  else if (pbi->max_threads > 1 && tile_count_tg > 1 &&
           !(tiles->large_scale && !pbi->ext_tile_debug))
    *p_data_end = decode_tiles_mt(pbi, data, data_end, start_tile, end_tile);
  else
    *p_data_end = decode_tiles(pbi, data, data_end, start_tile, end_tile);

  // If the bit stream is monochrome, set the U and V buffers to a constant.
  if (num_planes < 3) {
    set_planes_to_neutral_grey(cm->seq_params, xd->cur_buf, 1);
  }

  if (end_tile != tiles->rows * tiles->cols - 1) {
    return;
  }

  av1_alloc_cdef_buffers(cm, &pbi->cdef_worker, &pbi->cdef_sync,
                         pbi->num_workers, 1);
  av1_alloc_cdef_sync(cm, &pbi->cdef_sync, pbi->num_workers);

  if (!cm->features.allow_intrabc && !tiles->single_tile_decoding) {
    if (cm->lf.filter_level[0] || cm->lf.filter_level[1]) {
      av1_loop_filter_frame_mt(&cm->cur_frame->buf, cm, &pbi->dcb.xd, 0,
                               num_planes, 0, pbi->tile_workers,
                               pbi->num_workers, &pbi->lf_row_sync, 0);
    }

    const int do_cdef =
        !pbi->skip_loop_filter && !cm->features.coded_lossless &&
        (cm->cdef_info.cdef_bits || cm->cdef_info.cdef_strengths[0] ||
         cm->cdef_info.cdef_uv_strengths[0]);
    const int do_superres = av1_superres_scaled(cm);
    const int optimized_loop_restoration = !do_cdef && !do_superres;
    const int do_loop_restoration =
        cm->rst_info[0].frame_restoration_type != RESTORE_NONE ||
        cm->rst_info[1].frame_restoration_type != RESTORE_NONE ||
        cm->rst_info[2].frame_restoration_type != RESTORE_NONE;
    if (!optimized_loop_restoration) {
      if (do_loop_restoration)
        av1_loop_restoration_save_boundary_lines(&pbi->common.cur_frame->buf,
                                                 cm, 0);

      if (do_cdef) {
        if (pbi->num_workers > 1) {
          av1_cdef_frame_mt(cm, &pbi->dcb.xd, pbi->cdef_worker,
                            pbi->tile_workers, &pbi->cdef_sync,
                            pbi->num_workers, av1_cdef_init_fb_row_mt);
        } else {
          av1_cdef_frame(&pbi->common.cur_frame->buf, cm, &pbi->dcb.xd,
                         av1_cdef_init_fb_row);
        }
      }

      superres_post_decode(pbi);

      if (do_loop_restoration) {
        av1_loop_restoration_save_boundary_lines(&pbi->common.cur_frame->buf,
                                                 cm, 1);
        if (pbi->num_workers > 1) {
          av1_loop_restoration_filter_frame_mt(
              (YV12_BUFFER_CONFIG *)xd->cur_buf, cm, optimized_loop_restoration,
              pbi->tile_workers, pbi->num_workers, &pbi->lr_row_sync,
              &pbi->lr_ctxt);
        } else {
          av1_loop_restoration_filter_frame((YV12_BUFFER_CONFIG *)xd->cur_buf,
                                            cm, optimized_loop_restoration,
                                            &pbi->lr_ctxt);
        }
      }
    } else {
      // In no cdef and no superres case. Provide an optimized version of
      // loop_restoration_filter.
      if (do_loop_restoration) {
        if (pbi->num_workers > 1) {
          av1_loop_restoration_filter_frame_mt(
              (YV12_BUFFER_CONFIG *)xd->cur_buf, cm, optimized_loop_restoration,
              pbi->tile_workers, pbi->num_workers, &pbi->lr_row_sync,
              &pbi->lr_ctxt);
        } else {
          av1_loop_restoration_filter_frame((YV12_BUFFER_CONFIG *)xd->cur_buf,
                                            cm, optimized_loop_restoration,
                                            &pbi->lr_ctxt);
        }
      }
    }
  }

  if (!pbi->dcb.corrupted) {
    if (cm->features.refresh_frame_context == REFRESH_FRAME_CONTEXT_BACKWARD) {
      assert(pbi->context_update_tile_id < pbi->allocated_tiles);
      *cm->fc = pbi->tile_data[pbi->context_update_tile_id].tctx;
      av1_reset_cdf_symbol_counters(cm->fc);
    }
  } else {
    aom_internal_error(&pbi->error, AOM_CODEC_CORRUPT_FRAME,
                       "Decode failed. Frame data is corrupted.");
  }

#if CONFIG_INSPECTION
  if (pbi->inspect_cb != NULL) {
    (*pbi->inspect_cb)(pbi, pbi->inspect_ctx);
  }
#endif

  // Non frame parallel update frame context here.
  if (!tiles->large_scale) {
    cm->cur_frame->frame_context = *cm->fc;
  }

  if (cm->show_frame && !cm->seq_params->order_hint_info.enable_order_hint) {
    ++cm->current_frame.frame_number;
  }
}
