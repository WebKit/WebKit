/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/encoder/encodetxb.h"

#include <stdint.h>

#include "aom_ports/mem.h"
#include "av1/common/blockd.h"
#include "av1/common/idct.h"
#include "av1/common/pred_common.h"
#include "av1/common/scan.h"
#include "av1/encoder/bitstream.h"
#include "av1/encoder/cost.h"
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/hash.h"
#include "av1/encoder/rdopt.h"
#include "av1/encoder/tokenize.h"

void av1_alloc_txb_buf(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  CoeffBufferPool *coeff_buf_pool = &cpi->coeff_buffer_pool;
  const int num_sb_rows =
      CEIL_POWER_OF_TWO(cm->mi_params.mi_rows, cm->seq_params->mib_size_log2);
  const int num_sb_cols =
      CEIL_POWER_OF_TWO(cm->mi_params.mi_cols, cm->seq_params->mib_size_log2);
  const int size = num_sb_rows * num_sb_cols;
  const int num_planes = av1_num_planes(cm);
  const int subsampling_x = cm->seq_params->subsampling_x;
  const int subsampling_y = cm->seq_params->subsampling_y;
  const int luma_max_sb_square =
      1 << num_pels_log2_lookup[cm->seq_params->sb_size];
  const int chroma_max_sb_square =
      luma_max_sb_square >> (subsampling_x + subsampling_y);
  const int total_max_sb_square =
      (luma_max_sb_square + (num_planes - 1) * chroma_max_sb_square);
  if ((size_t)size > SIZE_MAX / (size_t)total_max_sb_square) {
    aom_internal_error(cm->error, AOM_CODEC_ERROR,
                       "A multiplication would overflow size_t");
  }
  const size_t num_tcoeffs = (size_t)size * (size_t)total_max_sb_square;
  const int txb_unit_size = TX_SIZE_W_MIN * TX_SIZE_H_MIN;

  av1_free_txb_buf(cpi);
  // TODO(jingning): This should be further reduced.
  CHECK_MEM_ERROR(cm, cpi->coeff_buffer_base,
                  aom_malloc(sizeof(*cpi->coeff_buffer_base) * size));
  if (sizeof(*coeff_buf_pool->tcoeff) > SIZE_MAX / num_tcoeffs) {
    aom_internal_error(cm->error, AOM_CODEC_ERROR,
                       "A multiplication would overflow size_t");
  }
  CHECK_MEM_ERROR(
      cm, coeff_buf_pool->tcoeff,
      aom_memalign(32, sizeof(*coeff_buf_pool->tcoeff) * num_tcoeffs));
  if (sizeof(*coeff_buf_pool->eobs) > SIZE_MAX / num_tcoeffs) {
    aom_internal_error(cm->error, AOM_CODEC_ERROR,
                       "A multiplication would overflow size_t");
  }
  CHECK_MEM_ERROR(
      cm, coeff_buf_pool->eobs,
      aom_malloc(sizeof(*coeff_buf_pool->eobs) * num_tcoeffs / txb_unit_size));
  if (sizeof(*coeff_buf_pool->entropy_ctx) > SIZE_MAX / num_tcoeffs) {
    aom_internal_error(cm->error, AOM_CODEC_ERROR,
                       "A multiplication would overflow size_t");
  }
  CHECK_MEM_ERROR(cm, coeff_buf_pool->entropy_ctx,
                  aom_malloc(sizeof(*coeff_buf_pool->entropy_ctx) *
                             num_tcoeffs / txb_unit_size));

  tran_low_t *tcoeff_ptr = coeff_buf_pool->tcoeff;
  uint16_t *eob_ptr = coeff_buf_pool->eobs;
  uint8_t *entropy_ctx_ptr = coeff_buf_pool->entropy_ctx;
  for (int i = 0; i < size; i++) {
    for (int plane = 0; plane < num_planes; plane++) {
      const int max_sb_square =
          (plane == AOM_PLANE_Y) ? luma_max_sb_square : chroma_max_sb_square;
      cpi->coeff_buffer_base[i].tcoeff[plane] = tcoeff_ptr;
      cpi->coeff_buffer_base[i].eobs[plane] = eob_ptr;
      cpi->coeff_buffer_base[i].entropy_ctx[plane] = entropy_ctx_ptr;
      tcoeff_ptr += max_sb_square;
      eob_ptr += max_sb_square / txb_unit_size;
      entropy_ctx_ptr += max_sb_square / txb_unit_size;
    }
  }
}

void av1_free_txb_buf(AV1_COMP *cpi) {
  CoeffBufferPool *coeff_buf_pool = &cpi->coeff_buffer_pool;
  aom_free(cpi->coeff_buffer_base);
  cpi->coeff_buffer_base = NULL;
  aom_free(coeff_buf_pool->tcoeff);
  coeff_buf_pool->tcoeff = NULL;
  aom_free(coeff_buf_pool->eobs);
  coeff_buf_pool->eobs = NULL;
  aom_free(coeff_buf_pool->entropy_ctx);
  coeff_buf_pool->entropy_ctx = NULL;
}

static void write_golomb(aom_writer *w, int level) {
  int x = level + 1;
  int i = x;
  int length = 0;

  while (i) {
    i >>= 1;
    ++length;
  }
  assert(length > 0);

  for (i = 0; i < length - 1; ++i) aom_write_bit(w, 0);

  for (i = length - 1; i >= 0; --i) aom_write_bit(w, (x >> i) & 0x01);
}

static const int8_t eob_to_pos_small[33] = {
  0, 1, 2,                                        // 0-2
  3, 3,                                           // 3-4
  4, 4, 4, 4,                                     // 5-8
  5, 5, 5, 5, 5, 5, 5, 5,                         // 9-16
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6  // 17-32
};

static const int8_t eob_to_pos_large[17] = {
  6,                               // place holder
  7,                               // 33-64
  8,  8,                           // 65-128
  9,  9,  9,  9,                   // 129-256
  10, 10, 10, 10, 10, 10, 10, 10,  // 257-512
  11                               // 513-
};

int av1_get_eob_pos_token(const int eob, int *const extra) {
  int t;

  if (eob < 33) {
    t = eob_to_pos_small[eob];
  } else {
    const int e = AOMMIN((eob - 1) >> 5, 16);
    t = eob_to_pos_large[e];
  }

  *extra = eob - av1_eob_group_start[t];

  return t;
}

#if CONFIG_ENTROPY_STATS
static void update_eob_context(int cdf_idx, int eob, TX_SIZE tx_size,
                               TX_CLASS tx_class, PLANE_TYPE plane,
                               FRAME_CONTEXT *ec_ctx, FRAME_COUNTS *counts,
                               uint8_t allow_update_cdf) {
#else
static void update_eob_context(int eob, TX_SIZE tx_size, TX_CLASS tx_class,
                               PLANE_TYPE plane, FRAME_CONTEXT *ec_ctx,
                               uint8_t allow_update_cdf) {
#endif
  int eob_extra;
  const int eob_pt = av1_get_eob_pos_token(eob, &eob_extra);
  TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);

  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const int eob_multi_ctx = (tx_class == TX_CLASS_2D) ? 0 : 1;

  switch (eob_multi_size) {
    case 0:
#if CONFIG_ENTROPY_STATS
      ++counts->eob_multi16[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
#endif
      if (allow_update_cdf)
        update_cdf(ec_ctx->eob_flag_cdf16[plane][eob_multi_ctx], eob_pt - 1, 5);
      break;
    case 1:
#if CONFIG_ENTROPY_STATS
      ++counts->eob_multi32[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
#endif
      if (allow_update_cdf)
        update_cdf(ec_ctx->eob_flag_cdf32[plane][eob_multi_ctx], eob_pt - 1, 6);
      break;
    case 2:
#if CONFIG_ENTROPY_STATS
      ++counts->eob_multi64[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
#endif
      if (allow_update_cdf)
        update_cdf(ec_ctx->eob_flag_cdf64[plane][eob_multi_ctx], eob_pt - 1, 7);
      break;
    case 3:
#if CONFIG_ENTROPY_STATS
      ++counts->eob_multi128[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
#endif
      if (allow_update_cdf) {
        update_cdf(ec_ctx->eob_flag_cdf128[plane][eob_multi_ctx], eob_pt - 1,
                   8);
      }
      break;
    case 4:
#if CONFIG_ENTROPY_STATS
      ++counts->eob_multi256[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
#endif
      if (allow_update_cdf) {
        update_cdf(ec_ctx->eob_flag_cdf256[plane][eob_multi_ctx], eob_pt - 1,
                   9);
      }
      break;
    case 5:
#if CONFIG_ENTROPY_STATS
      ++counts->eob_multi512[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
#endif
      if (allow_update_cdf) {
        update_cdf(ec_ctx->eob_flag_cdf512[plane][eob_multi_ctx], eob_pt - 1,
                   10);
      }
      break;
    case 6:
    default:
#if CONFIG_ENTROPY_STATS
      ++counts->eob_multi1024[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
#endif
      if (allow_update_cdf) {
        update_cdf(ec_ctx->eob_flag_cdf1024[plane][eob_multi_ctx], eob_pt - 1,
                   11);
      }
      break;
  }

  if (av1_eob_offset_bits[eob_pt] > 0) {
    int eob_ctx = eob_pt - 3;
    int eob_shift = av1_eob_offset_bits[eob_pt] - 1;
    int bit = (eob_extra & (1 << eob_shift)) ? 1 : 0;
#if CONFIG_ENTROPY_STATS
    counts->eob_extra[cdf_idx][txs_ctx][plane][eob_pt][bit]++;
#endif  // CONFIG_ENTROPY_STATS
    if (allow_update_cdf)
      update_cdf(ec_ctx->eob_extra_cdf[txs_ctx][plane][eob_ctx], bit, 2);
  }
}

static INLINE int get_nz_map_ctx(const uint8_t *const levels,
                                 const int coeff_idx, const int bhl,
                                 const int width, const int scan_idx,
                                 const int is_eob, const TX_SIZE tx_size,
                                 const TX_CLASS tx_class) {
  if (is_eob) {
    if (scan_idx == 0) return 0;
    if (scan_idx <= (width << bhl) / 8) return 1;
    if (scan_idx <= (width << bhl) / 4) return 2;
    return 3;
  }
  const int stats =
      get_nz_mag(levels + get_padded_idx(coeff_idx, bhl), bhl, tx_class);
  return get_nz_map_ctx_from_stats(stats, coeff_idx, bhl, tx_size, tx_class);
}

void av1_txb_init_levels_c(const tran_low_t *const coeff, const int width,
                           const int height, uint8_t *const levels) {
  const int stride = height + TX_PAD_HOR;
  uint8_t *ls = levels;

  memset(levels + stride * width, 0,
         sizeof(*levels) * (TX_PAD_BOTTOM * stride + TX_PAD_END));

  for (int i = 0; i < width; i++) {
    for (int j = 0; j < height; j++) {
      *ls++ = (uint8_t)clamp(abs(coeff[i * height + j]), 0, INT8_MAX);
    }
    for (int j = 0; j < TX_PAD_HOR; j++) {
      *ls++ = 0;
    }
  }
}

void av1_get_nz_map_contexts_c(const uint8_t *const levels,
                               const int16_t *const scan, const uint16_t eob,
                               const TX_SIZE tx_size, const TX_CLASS tx_class,
                               int8_t *const coeff_contexts) {
  const int bhl = get_txb_bhl(tx_size);
  const int width = get_txb_wide(tx_size);
  for (int i = 0; i < eob; ++i) {
    const int pos = scan[i];
    coeff_contexts[pos] = get_nz_map_ctx(levels, pos, bhl, width, i,
                                         i == eob - 1, tx_size, tx_class);
  }
}

void av1_write_coeffs_txb(const AV1_COMMON *const cm, MACROBLOCK *const x,
                          aom_writer *w, int blk_row, int blk_col, int plane,
                          int block, TX_SIZE tx_size) {
  MACROBLOCKD *xd = &x->e_mbd;
  const CB_COEFF_BUFFER *cb_coef_buff = x->cb_coef_buff;
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const int txb_offset = x->mbmi_ext_frame->cb_offset[plane_type] /
                         (TX_SIZE_W_MIN * TX_SIZE_H_MIN);
  const uint16_t *eob_txb = cb_coef_buff->eobs[plane] + txb_offset;
  const uint16_t eob = eob_txb[block];
  const uint8_t *entropy_ctx = cb_coef_buff->entropy_ctx[plane] + txb_offset;
  const int txb_skip_ctx = entropy_ctx[block] & TXB_SKIP_CTX_MASK;
  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  aom_write_symbol(w, eob == 0, ec_ctx->txb_skip_cdf[txs_ctx][txb_skip_ctx], 2);
  if (eob == 0) return;

  const TX_TYPE tx_type =
      av1_get_tx_type(xd, plane_type, blk_row, blk_col, tx_size,
                      cm->features.reduced_tx_set_used);
  // Only y plane's tx_type is transmitted
  if (plane == 0) {
    av1_write_tx_type(cm, xd, tx_type, tx_size, w);
  }

  int eob_extra;
  const int eob_pt = av1_get_eob_pos_token(eob, &eob_extra);
  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const TX_CLASS tx_class = tx_type_to_class[tx_type];
  const int eob_multi_ctx = (tx_class == TX_CLASS_2D) ? 0 : 1;
  switch (eob_multi_size) {
    case 0:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf16[plane_type][eob_multi_ctx], 5);
      break;
    case 1:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf32[plane_type][eob_multi_ctx], 6);
      break;
    case 2:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf64[plane_type][eob_multi_ctx], 7);
      break;
    case 3:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf128[plane_type][eob_multi_ctx], 8);
      break;
    case 4:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf256[plane_type][eob_multi_ctx], 9);
      break;
    case 5:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf512[plane_type][eob_multi_ctx], 10);
      break;
    default:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf1024[plane_type][eob_multi_ctx], 11);
      break;
  }

  const int eob_offset_bits = av1_eob_offset_bits[eob_pt];
  if (eob_offset_bits > 0) {
    const int eob_ctx = eob_pt - 3;
    int eob_shift = eob_offset_bits - 1;
    int bit = (eob_extra & (1 << eob_shift)) ? 1 : 0;
    aom_write_symbol(w, bit,
                     ec_ctx->eob_extra_cdf[txs_ctx][plane_type][eob_ctx], 2);
    for (int i = 1; i < eob_offset_bits; i++) {
      eob_shift = eob_offset_bits - 1 - i;
      bit = (eob_extra & (1 << eob_shift)) ? 1 : 0;
      aom_write_bit(w, bit);
    }
  }

  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, height);
  const tran_low_t *tcoeff_txb =
      cb_coef_buff->tcoeff[plane] + x->mbmi_ext_frame->cb_offset[plane_type];
  const tran_low_t *tcoeff = tcoeff_txb + BLOCK_OFFSET(block);
  av1_txb_init_levels(tcoeff, width, height, levels);
  const SCAN_ORDER *const scan_order = get_scan(tx_size, tx_type);
  const int16_t *const scan = scan_order->scan;
  DECLARE_ALIGNED(16, int8_t, coeff_contexts[MAX_TX_SQUARE]);
  av1_get_nz_map_contexts(levels, scan, eob, tx_size, tx_class, coeff_contexts);

  const int bhl = get_txb_bhl(tx_size);
  for (int c = eob - 1; c >= 0; --c) {
    const int pos = scan[c];
    const int coeff_ctx = coeff_contexts[pos];
    const tran_low_t v = tcoeff[pos];
    const tran_low_t level = abs(v);

    if (c == eob - 1) {
      aom_write_symbol(
          w, AOMMIN(level, 3) - 1,
          ec_ctx->coeff_base_eob_cdf[txs_ctx][plane_type][coeff_ctx], 3);
    } else {
      aom_write_symbol(w, AOMMIN(level, 3),
                       ec_ctx->coeff_base_cdf[txs_ctx][plane_type][coeff_ctx],
                       4);
    }
    if (level > NUM_BASE_LEVELS) {
      // level is above 1.
      const int base_range = level - 1 - NUM_BASE_LEVELS;
      const int br_ctx = get_br_ctx(levels, pos, bhl, tx_class);
      aom_cdf_prob *cdf =
          ec_ctx->coeff_br_cdf[AOMMIN(txs_ctx, TX_32X32)][plane_type][br_ctx];
      for (int idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
        const int k = AOMMIN(base_range - idx, BR_CDF_SIZE - 1);
        aom_write_symbol(w, k, cdf, BR_CDF_SIZE);
        if (k < BR_CDF_SIZE - 1) break;
      }
    }
  }

  // Loop to code all signs in the transform block,
  // starting with the sign of DC (if applicable)
  for (int c = 0; c < eob; ++c) {
    const tran_low_t v = tcoeff[scan[c]];
    const tran_low_t level = abs(v);
    const int sign = (v < 0) ? 1 : 0;
    if (level) {
      if (c == 0) {
        const int dc_sign_ctx =
            (entropy_ctx[block] >> DC_SIGN_CTX_SHIFT) & DC_SIGN_CTX_MASK;
        aom_write_symbol(w, sign, ec_ctx->dc_sign_cdf[plane_type][dc_sign_ctx],
                         2);
      } else {
        aom_write_bit(w, sign);
      }
      if (level > COEFF_BASE_RANGE + NUM_BASE_LEVELS)
        write_golomb(w, level - COEFF_BASE_RANGE - 1 - NUM_BASE_LEVELS);
    }
  }
}

void av1_write_intra_coeffs_mb(const AV1_COMMON *const cm, MACROBLOCK *x,
                               aom_writer *w, BLOCK_SIZE bsize) {
  MACROBLOCKD *xd = &x->e_mbd;
  const int num_planes = av1_num_planes(cm);
  int block[MAX_MB_PLANE] = { 0 };
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
        const TX_SIZE tx_size = av1_get_tx_size(plane, xd);
        const int stepr = tx_size_high_unit[tx_size];
        const int stepc = tx_size_wide_unit[tx_size];
        const int step = stepr * stepc;
        const struct macroblockd_plane *const pd = &xd->plane[plane];
        const int unit_height = ROUND_POWER_OF_TWO(
            AOMMIN(mu_blocks_high + row, max_blocks_high), pd->subsampling_y);
        const int unit_width = ROUND_POWER_OF_TWO(
            AOMMIN(mu_blocks_wide + col, max_blocks_wide), pd->subsampling_x);
        for (int blk_row = row >> pd->subsampling_y; blk_row < unit_height;
             blk_row += stepr) {
          for (int blk_col = col >> pd->subsampling_x; blk_col < unit_width;
               blk_col += stepc) {
            av1_write_coeffs_txb(cm, x, w, blk_row, blk_col, plane,
                                 block[plane], tx_size);
            block[plane] += step;
          }
        }
      }
    }
  }
}

uint8_t av1_get_txb_entropy_context(const tran_low_t *qcoeff,
                                    const SCAN_ORDER *scan_order, int eob) {
  const int16_t *const scan = scan_order->scan;
  int cul_level = 0;
  int c;

  if (eob == 0) return 0;
  for (c = 0; c < eob; ++c) {
    cul_level += abs(qcoeff[scan[c]]);
    if (cul_level > COEFF_CONTEXT_MASK) break;
  }

  cul_level = AOMMIN(COEFF_CONTEXT_MASK, cul_level);
  set_dc_sign(&cul_level, qcoeff[0]);

  return (uint8_t)cul_level;
}

static void update_tx_type_count(const AV1_COMP *cpi, const AV1_COMMON *cm,
                                 MACROBLOCKD *xd, int blk_row, int blk_col,
                                 int plane, TX_SIZE tx_size,
                                 FRAME_COUNTS *counts,
                                 uint8_t allow_update_cdf) {
  MB_MODE_INFO *mbmi = xd->mi[0];
  int is_inter = is_inter_block(mbmi);
  const int reduced_tx_set_used = cm->features.reduced_tx_set_used;
  FRAME_CONTEXT *fc = xd->tile_ctx;
#if !CONFIG_ENTROPY_STATS
  (void)counts;
#endif  // !CONFIG_ENTROPY_STATS

  // Only y plane's tx_type is updated
  if (plane > 0) return;
  const TX_TYPE tx_type = av1_get_tx_type(xd, PLANE_TYPE_Y, blk_row, blk_col,
                                          tx_size, reduced_tx_set_used);
  if (is_inter) {
    if (cpi->oxcf.txfm_cfg.use_inter_dct_only) {
      assert(tx_type == DCT_DCT);
    }
  } else {
    if (cpi->oxcf.txfm_cfg.use_intra_dct_only) {
      assert(tx_type == DCT_DCT);
    } else if (cpi->oxcf.txfm_cfg.use_intra_default_tx_only) {
      const TX_TYPE default_type = get_default_tx_type(
          PLANE_TYPE_Y, xd, tx_size, cpi->use_screen_content_tools);
      (void)default_type;
      // TODO(kyslov): We don't always respect use_intra_default_tx_only flag in
      // NonRD and REALTIME case. Specifically we ignore it in hybrid inta mode
      // search, when picking up intra mode in nonRD inter mode search and in RD
      // REALTIME mode when we limit TX type usage.
      // We need to fix txfm cfg for these cases. Meanwhile relieving the
      // assert.
      assert(tx_type == default_type || cpi->sf.rt_sf.use_nonrd_pick_mode ||
             cpi->oxcf.mode == REALTIME);
    }
  }

  if (get_ext_tx_types(tx_size, is_inter, reduced_tx_set_used) > 1 &&
      cm->quant_params.base_qindex > 0 && !mbmi->skip_txfm &&
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
    const int eset = get_ext_tx_set(tx_size, is_inter, reduced_tx_set_used);
    if (eset > 0) {
      const TxSetType tx_set_type =
          av1_get_ext_tx_set_type(tx_size, is_inter, reduced_tx_set_used);
      if (is_inter) {
        if (allow_update_cdf) {
          update_cdf(fc->inter_ext_tx_cdf[eset][txsize_sqr_map[tx_size]],
                     av1_ext_tx_ind[tx_set_type][tx_type],
                     av1_num_ext_tx_set[tx_set_type]);
        }
#if CONFIG_ENTROPY_STATS
        ++counts->inter_ext_tx[eset][txsize_sqr_map[tx_size]]
                              [av1_ext_tx_ind[tx_set_type][tx_type]];
#endif  // CONFIG_ENTROPY_STATS
      } else {
        PREDICTION_MODE intra_dir;
        if (mbmi->filter_intra_mode_info.use_filter_intra)
          intra_dir = fimode_to_intradir[mbmi->filter_intra_mode_info
                                             .filter_intra_mode];
        else
          intra_dir = mbmi->mode;
#if CONFIG_ENTROPY_STATS
        ++counts->intra_ext_tx[eset][txsize_sqr_map[tx_size]][intra_dir]
                              [av1_ext_tx_ind[tx_set_type][tx_type]];
#endif  // CONFIG_ENTROPY_STATS
        if (allow_update_cdf) {
          update_cdf(
              fc->intra_ext_tx_cdf[eset][txsize_sqr_map[tx_size]][intra_dir],
              av1_ext_tx_ind[tx_set_type][tx_type],
              av1_num_ext_tx_set[tx_set_type]);
        }
      }
    }
  }
}

void av1_update_and_record_txb_context(int plane, int block, int blk_row,
                                       int blk_col, BLOCK_SIZE plane_bsize,
                                       TX_SIZE tx_size, void *arg) {
  struct tokenize_b_args *const args = arg;
  const AV1_COMP *cpi = args->cpi;
  const AV1_COMMON *cm = &cpi->common;
  ThreadData *const td = args->td;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *p = &x->plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  const int eob = p->eobs[block];
  const int block_offset = BLOCK_OFFSET(block);
  tran_low_t *qcoeff = p->qcoeff + block_offset;
  const PLANE_TYPE plane_type = pd->plane_type;
  const TX_TYPE tx_type =
      av1_get_tx_type(xd, plane_type, blk_row, blk_col, tx_size,
                      cm->features.reduced_tx_set_used);
  const SCAN_ORDER *const scan_order = get_scan(tx_size, tx_type);
  tran_low_t *tcoeff;
  assert(args->dry_run != DRY_RUN_COSTCOEFFS);
  if (args->dry_run == OUTPUT_ENABLED) {
    MB_MODE_INFO *mbmi = xd->mi[0];
    TXB_CTX txb_ctx;
    get_txb_ctx(plane_bsize, tx_size, plane,
                pd->above_entropy_context + blk_col,
                pd->left_entropy_context + blk_row, &txb_ctx);
    const int bhl = get_txb_bhl(tx_size);
    const int width = get_txb_wide(tx_size);
    const int height = get_txb_high(tx_size);
    const uint8_t allow_update_cdf = args->allow_update_cdf;
    const TX_SIZE txsize_ctx = get_txsize_entropy_ctx(tx_size);
    FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
#if CONFIG_ENTROPY_STATS
    int cdf_idx = cm->coef_cdf_category;
    ++td->counts->txb_skip[cdf_idx][txsize_ctx][txb_ctx.txb_skip_ctx][eob == 0];
#endif  // CONFIG_ENTROPY_STATS
    if (allow_update_cdf) {
      update_cdf(ec_ctx->txb_skip_cdf[txsize_ctx][txb_ctx.txb_skip_ctx],
                 eob == 0, 2);
    }

    CB_COEFF_BUFFER *cb_coef_buff = x->cb_coef_buff;
    const int txb_offset = x->mbmi_ext_frame->cb_offset[plane_type] /
                           (TX_SIZE_W_MIN * TX_SIZE_H_MIN);
    uint16_t *eob_txb = cb_coef_buff->eobs[plane] + txb_offset;
    uint8_t *const entropy_ctx = cb_coef_buff->entropy_ctx[plane] + txb_offset;
    entropy_ctx[block] = txb_ctx.txb_skip_ctx;
    eob_txb[block] = eob;

    if (eob == 0) {
      av1_set_entropy_contexts(xd, pd, plane, plane_bsize, tx_size, 0, blk_col,
                               blk_row);
      return;
    }
    const int segment_id = mbmi->segment_id;
    const int seg_eob = av1_get_tx_eob(&cpi->common.seg, segment_id, tx_size);
    tran_low_t *tcoeff_txb =
        cb_coef_buff->tcoeff[plane] + x->mbmi_ext_frame->cb_offset[plane_type];
    tcoeff = tcoeff_txb + block_offset;
    memcpy(tcoeff, qcoeff, sizeof(*tcoeff) * seg_eob);

    uint8_t levels_buf[TX_PAD_2D];
    uint8_t *const levels = set_levels(levels_buf, height);
    av1_txb_init_levels(tcoeff, width, height, levels);
    update_tx_type_count(cpi, cm, xd, blk_row, blk_col, plane, tx_size,
                         td->counts, allow_update_cdf);

    const TX_CLASS tx_class = tx_type_to_class[tx_type];
    const int16_t *const scan = scan_order->scan;

    // record tx type usage
    td->rd_counts.tx_type_used[tx_size][tx_type]++;

#if CONFIG_ENTROPY_STATS
    update_eob_context(cdf_idx, eob, tx_size, tx_class, plane_type, ec_ctx,
                       td->counts, allow_update_cdf);
#else
    update_eob_context(eob, tx_size, tx_class, plane_type, ec_ctx,
                       allow_update_cdf);
#endif

    DECLARE_ALIGNED(16, int8_t, coeff_contexts[MAX_TX_SQUARE]);
    av1_get_nz_map_contexts(levels, scan, eob, tx_size, tx_class,
                            coeff_contexts);

    for (int c = eob - 1; c >= 0; --c) {
      const int pos = scan[c];
      const int coeff_ctx = coeff_contexts[pos];
      const tran_low_t v = qcoeff[pos];
      const tran_low_t level = abs(v);
      /* abs_sum_level is needed to decide the job scheduling order of
       * pack bitstream multi-threading. This data is not needed if
       * multi-threading is disabled. */
      if (cpi->mt_info.pack_bs_mt_enabled) td->abs_sum_level += level;

      if (allow_update_cdf) {
        if (c == eob - 1) {
          assert(coeff_ctx < 4);
          update_cdf(
              ec_ctx->coeff_base_eob_cdf[txsize_ctx][plane_type][coeff_ctx],
              AOMMIN(level, 3) - 1, 3);
        } else {
          update_cdf(ec_ctx->coeff_base_cdf[txsize_ctx][plane_type][coeff_ctx],
                     AOMMIN(level, 3), 4);
        }
      }
      if (c == eob - 1) {
        assert(coeff_ctx < 4);
#if CONFIG_ENTROPY_STATS
        ++td->counts->coeff_base_eob_multi[cdf_idx][txsize_ctx][plane_type]
                                          [coeff_ctx][AOMMIN(level, 3) - 1];
      } else {
        ++td->counts->coeff_base_multi[cdf_idx][txsize_ctx][plane_type]
                                      [coeff_ctx][AOMMIN(level, 3)];
#endif
      }
      if (level > NUM_BASE_LEVELS) {
        const int base_range = level - 1 - NUM_BASE_LEVELS;
        const int br_ctx = get_br_ctx(levels, pos, bhl, tx_class);
        for (int idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
          const int k = AOMMIN(base_range - idx, BR_CDF_SIZE - 1);
          if (allow_update_cdf) {
            update_cdf(ec_ctx->coeff_br_cdf[AOMMIN(txsize_ctx, TX_32X32)]
                                           [plane_type][br_ctx],
                       k, BR_CDF_SIZE);
          }
          for (int lps = 0; lps < BR_CDF_SIZE - 1; lps++) {
#if CONFIG_ENTROPY_STATS
            ++td->counts->coeff_lps[AOMMIN(txsize_ctx, TX_32X32)][plane_type]
                                   [lps][br_ctx][lps == k];
#endif  // CONFIG_ENTROPY_STATS
            if (lps == k) break;
          }
#if CONFIG_ENTROPY_STATS
          ++td->counts->coeff_lps_multi[cdf_idx][AOMMIN(txsize_ctx, TX_32X32)]
                                       [plane_type][br_ctx][k];
#endif
          if (k < BR_CDF_SIZE - 1) break;
        }
      }
    }
    // Update the context needed to code the DC sign (if applicable)
    if (tcoeff[0] != 0) {
      const int dc_sign = (tcoeff[0] < 0) ? 1 : 0;
      const int dc_sign_ctx = txb_ctx.dc_sign_ctx;
#if CONFIG_ENTROPY_STATS
      ++td->counts->dc_sign[plane_type][dc_sign_ctx][dc_sign];
#endif  // CONFIG_ENTROPY_STATS
      if (allow_update_cdf)
        update_cdf(ec_ctx->dc_sign_cdf[plane_type][dc_sign_ctx], dc_sign, 2);
      entropy_ctx[block] |= dc_sign_ctx << DC_SIGN_CTX_SHIFT;
    }
  } else {
    tcoeff = qcoeff;
  }
  const uint8_t cul_level =
      av1_get_txb_entropy_context(tcoeff, scan_order, eob);
  av1_set_entropy_contexts(xd, pd, plane, plane_bsize, tx_size, cul_level,
                           blk_col, blk_row);
}

void av1_record_txb_context(int plane, int block, int blk_row, int blk_col,
                            BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                            void *arg) {
  struct tokenize_b_args *const args = arg;
  const AV1_COMP *cpi = args->cpi;
  const AV1_COMMON *cm = &cpi->common;
  ThreadData *const td = args->td;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *p = &x->plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  const int eob = p->eobs[block];
  const int block_offset = BLOCK_OFFSET(block);
  tran_low_t *qcoeff = p->qcoeff + block_offset;
  const PLANE_TYPE plane_type = pd->plane_type;
  const TX_TYPE tx_type =
      av1_get_tx_type(xd, plane_type, blk_row, blk_col, tx_size,
                      cm->features.reduced_tx_set_used);
  const SCAN_ORDER *const scan_order = get_scan(tx_size, tx_type);
  tran_low_t *tcoeff;
  assert(args->dry_run != DRY_RUN_COSTCOEFFS);
  if (args->dry_run == OUTPUT_ENABLED) {
    MB_MODE_INFO *mbmi = xd->mi[0];
    TXB_CTX txb_ctx;
    get_txb_ctx(plane_bsize, tx_size, plane,
                pd->above_entropy_context + blk_col,
                pd->left_entropy_context + blk_row, &txb_ctx);
#if CONFIG_ENTROPY_STATS
    const TX_SIZE txsize_ctx = get_txsize_entropy_ctx(tx_size);
    const int bhl = get_txb_bhl(tx_size);
    const int width = get_txb_wide(tx_size);
    const int height = get_txb_high(tx_size);
    int cdf_idx = cm->coef_cdf_category;
    ++td->counts->txb_skip[cdf_idx][txsize_ctx][txb_ctx.txb_skip_ctx][eob == 0];
#endif  // CONFIG_ENTROPY_STATS

    CB_COEFF_BUFFER *cb_coef_buff = x->cb_coef_buff;
    const int txb_offset = x->mbmi_ext_frame->cb_offset[plane_type] /
                           (TX_SIZE_W_MIN * TX_SIZE_H_MIN);
    uint16_t *eob_txb = cb_coef_buff->eobs[plane] + txb_offset;
    uint8_t *const entropy_ctx = cb_coef_buff->entropy_ctx[plane] + txb_offset;
    entropy_ctx[block] = txb_ctx.txb_skip_ctx;
    eob_txb[block] = eob;

    if (eob == 0) {
      av1_set_entropy_contexts(xd, pd, plane, plane_bsize, tx_size, 0, blk_col,
                               blk_row);
      return;
    }
    const int segment_id = mbmi->segment_id;
    const int seg_eob = av1_get_tx_eob(&cpi->common.seg, segment_id, tx_size);
    tran_low_t *tcoeff_txb =
        cb_coef_buff->tcoeff[plane] + x->mbmi_ext_frame->cb_offset[plane_type];
    tcoeff = tcoeff_txb + block_offset;
    memcpy(tcoeff, qcoeff, sizeof(*tcoeff) * seg_eob);

#if CONFIG_ENTROPY_STATS
    uint8_t levels_buf[TX_PAD_2D];
    uint8_t *const levels = set_levels(levels_buf, height);
    av1_txb_init_levels(tcoeff, width, height, levels);
    update_tx_type_count(cpi, cm, xd, blk_row, blk_col, plane, tx_size,
                         td->counts, 0 /*allow_update_cdf*/);

    const TX_CLASS tx_class = tx_type_to_class[tx_type];
    const bool do_coeff_scan = true;
#else
    const bool do_coeff_scan = cpi->mt_info.pack_bs_mt_enabled;
#endif
    const int16_t *const scan = scan_order->scan;

    // record tx type usage
    td->rd_counts.tx_type_used[tx_size][tx_type]++;

#if CONFIG_ENTROPY_STATS
    FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
    update_eob_context(cdf_idx, eob, tx_size, tx_class, plane_type, ec_ctx,
                       td->counts, 0 /*allow_update_cdf*/);

    DECLARE_ALIGNED(16, int8_t, coeff_contexts[MAX_TX_SQUARE]);
    av1_get_nz_map_contexts(levels, scan, eob, tx_size, tx_class,
                            coeff_contexts);
#endif

    for (int c = eob - 1; (c >= 0) && do_coeff_scan; --c) {
      const int pos = scan[c];
      const tran_low_t v = qcoeff[pos];
      const tran_low_t level = abs(v);
      /* abs_sum_level is needed to decide the job scheduling order of
       * pack bitstream multi-threading. This data is not needed if
       * multi-threading is disabled. */
      if (cpi->mt_info.pack_bs_mt_enabled) td->abs_sum_level += level;

#if CONFIG_ENTROPY_STATS
      const int coeff_ctx = coeff_contexts[pos];
      if (c == eob - 1) {
        assert(coeff_ctx < 4);
        ++td->counts->coeff_base_eob_multi[cdf_idx][txsize_ctx][plane_type]
                                          [coeff_ctx][AOMMIN(level, 3) - 1];
      } else {
        ++td->counts->coeff_base_multi[cdf_idx][txsize_ctx][plane_type]
                                      [coeff_ctx][AOMMIN(level, 3)];
      }
      if (level > NUM_BASE_LEVELS) {
        const int base_range = level - 1 - NUM_BASE_LEVELS;
        const int br_ctx = get_br_ctx(levels, pos, bhl, tx_class);
        for (int idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
          const int k = AOMMIN(base_range - idx, BR_CDF_SIZE - 1);
          for (int lps = 0; lps < BR_CDF_SIZE - 1; lps++) {
            ++td->counts->coeff_lps[AOMMIN(txsize_ctx, TX_32X32)][plane_type]
                                   [lps][br_ctx][lps == k];
            if (lps == k) break;
          }
          ++td->counts->coeff_lps_multi[cdf_idx][AOMMIN(txsize_ctx, TX_32X32)]
                                       [plane_type][br_ctx][k];
          if (k < BR_CDF_SIZE - 1) break;
        }
      }
#endif
    }
    // Update the context needed to code the DC sign (if applicable)
    if (tcoeff[0] != 0) {
      const int dc_sign_ctx = txb_ctx.dc_sign_ctx;
#if CONFIG_ENTROPY_STATS
      const int dc_sign = (tcoeff[0] < 0) ? 1 : 0;
      ++td->counts->dc_sign[plane_type][dc_sign_ctx][dc_sign];
#endif  // CONFIG_ENTROPY_STATS
      entropy_ctx[block] |= dc_sign_ctx << DC_SIGN_CTX_SHIFT;
    }
  } else {
    tcoeff = qcoeff;
  }
  const uint8_t cul_level =
      av1_get_txb_entropy_context(tcoeff, scan_order, eob);
  av1_set_entropy_contexts(xd, pd, plane, plane_bsize, tx_size, cul_level,
                           blk_col, blk_row);
}

void av1_update_intra_mb_txb_context(const AV1_COMP *cpi, ThreadData *td,
                                     RUN_TYPE dry_run, BLOCK_SIZE bsize,
                                     uint8_t allow_update_cdf) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  struct tokenize_b_args arg = { cpi, td, 0, allow_update_cdf, dry_run };
  if (mbmi->skip_txfm) {
    av1_reset_entropy_context(xd, bsize, num_planes);
    return;
  }
  const foreach_transformed_block_visitor visit =
      allow_update_cdf ? av1_update_and_record_txb_context
                       : av1_record_txb_context;

  for (int plane = 0; plane < num_planes; ++plane) {
    if (plane && !xd->is_chroma_ref) break;
    const struct macroblockd_plane *const pd = &xd->plane[plane];
    const int ss_x = pd->subsampling_x;
    const int ss_y = pd->subsampling_y;
    const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, ss_x, ss_y);
    av1_foreach_transformed_block_in_plane(xd, plane_bsize, plane, visit, &arg);
  }
}

CB_COEFF_BUFFER *av1_get_cb_coeff_buffer(const struct AV1_COMP *cpi, int mi_row,
                                         int mi_col) {
  const AV1_COMMON *const cm = &cpi->common;
  const int mib_size_log2 = cm->seq_params->mib_size_log2;
  const int stride =
      CEIL_POWER_OF_TWO(cm->mi_params.mi_cols, cm->seq_params->mib_size_log2);
  const int offset =
      (mi_row >> mib_size_log2) * stride + (mi_col >> mib_size_log2);
  return cpi->coeff_buffer_base + offset;
}
