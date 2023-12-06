/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "config/aom_dsp_rtcd.h"

#include "av1/common/reconinter.h"

#include "av1/encoder/encodemv.h"
#include "av1/encoder/nonrd_opt.h"
#include "av1/encoder/rdopt.h"

static const SCAN_ORDER av1_fast_idtx_scan_order_16x16 = {
  av1_fast_idtx_scan_16x16, av1_fast_idtx_iscan_16x16
};

#define DECLARE_BLOCK_YRD_BUFFERS()                      \
  DECLARE_ALIGNED(64, tran_low_t, dqcoeff_buf[16 * 16]); \
  DECLARE_ALIGNED(64, tran_low_t, qcoeff_buf[16 * 16]);  \
  DECLARE_ALIGNED(64, tran_low_t, coeff_buf[16 * 16]);   \
  uint16_t eob[1];

#define DECLARE_BLOCK_YRD_VARS()                                          \
  /* When is_tx_8x8_dual_applicable is true, we compute the txfm for the  \
   * entire bsize and write macroblock_plane::coeff. So low_coeff is kept \
   * as a non-const so we can reassign it to macroblock_plane::coeff. */  \
  int16_t *low_coeff = (int16_t *)coeff_buf;                              \
  int16_t *const low_qcoeff = (int16_t *)qcoeff_buf;                      \
  int16_t *const low_dqcoeff = (int16_t *)dqcoeff_buf;                    \
  const int diff_stride = bw;

#define DECLARE_LOOP_VARS_BLOCK_YRD() \
  const int16_t *src_diff = &p->src_diff[(r * diff_stride + c) << 2];

static AOM_FORCE_INLINE void update_yrd_loop_vars(
    MACROBLOCK *x, int *skippable, int step, int ncoeffs,
    int16_t *const low_coeff, int16_t *const low_qcoeff,
    int16_t *const low_dqcoeff, RD_STATS *this_rdc, int *eob_cost,
    int tx_blk_id) {
  const int is_txfm_skip = (ncoeffs == 0);
  *skippable &= is_txfm_skip;
  x->txfm_search_info.blk_skip[tx_blk_id] = is_txfm_skip;
  *eob_cost += get_msb(ncoeffs + 1);
  if (ncoeffs == 1)
    this_rdc->rate += (int)abs(low_qcoeff[0]);
  else if (ncoeffs > 1)
    this_rdc->rate += aom_satd_lp(low_qcoeff, step << 4);

  this_rdc->dist += av1_block_error_lp(low_coeff, low_dqcoeff, step << 4) >> 2;
}

static INLINE void aom_process_hadamard_lp_8x16(MACROBLOCK *x,
                                                int max_blocks_high,
                                                int max_blocks_wide,
                                                int num_4x4_w, int step,
                                                int block_step) {
  struct macroblock_plane *const p = &x->plane[AOM_PLANE_Y];
  const int bw = 4 * num_4x4_w;
  const int num_4x4 = AOMMIN(num_4x4_w, max_blocks_wide);
  int block = 0;

  for (int r = 0; r < max_blocks_high; r += block_step) {
    for (int c = 0; c < num_4x4; c += 2 * block_step) {
      const int16_t *src_diff = &p->src_diff[(r * bw + c) << 2];
      int16_t *low_coeff = (int16_t *)p->coeff + BLOCK_OFFSET(block);
      aom_hadamard_lp_8x8_dual(src_diff, (ptrdiff_t)bw, low_coeff);
      block += 2 * step;
    }
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
#define DECLARE_BLOCK_YRD_HBD_VARS()     \
  tran_low_t *const coeff = coeff_buf;   \
  tran_low_t *const qcoeff = qcoeff_buf; \
  tran_low_t *const dqcoeff = dqcoeff_buf;

static AOM_FORCE_INLINE void update_yrd_loop_vars_hbd(
    MACROBLOCK *x, int *skippable, int step, int ncoeffs,
    tran_low_t *const coeff, tran_low_t *const qcoeff,
    tran_low_t *const dqcoeff, RD_STATS *this_rdc, int *eob_cost,
    int tx_blk_id) {
  const MACROBLOCKD *xd = &x->e_mbd;
  const int is_txfm_skip = (ncoeffs == 0);
  *skippable &= is_txfm_skip;
  x->txfm_search_info.blk_skip[tx_blk_id] = is_txfm_skip;
  *eob_cost += get_msb(ncoeffs + 1);

  int64_t dummy;
  if (ncoeffs == 1)
    this_rdc->rate += (int)abs(qcoeff[0]);
  else if (ncoeffs > 1)
    this_rdc->rate += aom_satd(qcoeff, step << 4);
  this_rdc->dist +=
      av1_highbd_block_error(coeff, dqcoeff, step << 4, &dummy, xd->bd) >> 2;
}
#endif

/*!\brief Calculates RD Cost using Hadamard transform.
 *
 * \ingroup nonrd_mode_search
 * \callgraph
 * \callergraph
 * Calculates RD Cost using Hadamard transform. For low bit depth this function
 * uses low-precision set of functions (16-bit) and 32 bit for high bit depth
 * \param[in]    x              Pointer to structure holding all the data for
                                the current macroblock
 * \param[in]    this_rdc       Pointer to calculated RD Cost
 * \param[in]    skippable      Pointer to a flag indicating possible tx skip
 * \param[in]    bsize          Current block size
 * \param[in]    tx_size        Transform size
 * \param[in]    is_inter_mode  Flag to indicate inter mode
 *
 * \remark Nothing is returned. Instead, calculated RD cost is placed to
 * \c this_rdc. \c skippable flag is set if there is no non-zero quantized
 * coefficients for Hadamard transform
 */
void av1_block_yrd(MACROBLOCK *x, RD_STATS *this_rdc, int *skippable,
                   BLOCK_SIZE bsize, TX_SIZE tx_size) {
  MACROBLOCKD *xd = &x->e_mbd;
  const struct macroblockd_plane *pd = &xd->plane[AOM_PLANE_Y];
  struct macroblock_plane *const p = &x->plane[AOM_PLANE_Y];
  assert(bsize < BLOCK_SIZES_ALL);
  const int num_4x4_w = mi_size_wide[bsize];
  const int num_4x4_h = mi_size_high[bsize];
  const int step = 1 << (tx_size << 1);
  const int block_step = (1 << tx_size);
  const int row_step = step * num_4x4_w >> tx_size;
  int block = 0;
  const int max_blocks_wide =
      num_4x4_w + (xd->mb_to_right_edge >= 0 ? 0 : xd->mb_to_right_edge >> 5);
  const int max_blocks_high =
      num_4x4_h + (xd->mb_to_bottom_edge >= 0 ? 0 : xd->mb_to_bottom_edge >> 5);
  int eob_cost = 0;
  const int bw = 4 * num_4x4_w;
  const int bh = 4 * num_4x4_h;
  const int use_hbd = is_cur_buf_hbd(xd);
  int num_blk_skip_w = num_4x4_w;

#if CONFIG_AV1_HIGHBITDEPTH
  if (use_hbd) {
    aom_highbd_subtract_block(bh, bw, p->src_diff, bw, p->src.buf,
                              p->src.stride, pd->dst.buf, pd->dst.stride);
  } else {
    aom_subtract_block(bh, bw, p->src_diff, bw, p->src.buf, p->src.stride,
                       pd->dst.buf, pd->dst.stride);
  }
#else
  aom_subtract_block(bh, bw, p->src_diff, bw, p->src.buf, p->src.stride,
                     pd->dst.buf, pd->dst.stride);
#endif

  // Keep the intermediate value on the stack here. Writing directly to
  // skippable causes speed regression due to load-and-store issues in
  // update_yrd_loop_vars.
  int temp_skippable = 1;
  this_rdc->dist = 0;
  this_rdc->rate = 0;
  // For block sizes 8x16 or above, Hadamard txfm of two adjacent 8x8 blocks
  // can be done per function call. Hence the call of Hadamard txfm is
  // abstracted here for the specified cases.
  int is_tx_8x8_dual_applicable =
      (tx_size == TX_8X8 && block_size_wide[bsize] >= 16 &&
       block_size_high[bsize] >= 8);

#if CONFIG_AV1_HIGHBITDEPTH
  // As of now, dual implementation of hadamard txfm is available for low
  // bitdepth.
  if (use_hbd) is_tx_8x8_dual_applicable = 0;
#endif

  if (is_tx_8x8_dual_applicable) {
    aom_process_hadamard_lp_8x16(x, max_blocks_high, max_blocks_wide, num_4x4_w,
                                 step, block_step);
  }

  const SCAN_ORDER *const scan_order = &av1_scan_orders[tx_size][DCT_DCT];
  DECLARE_BLOCK_YRD_BUFFERS()
  DECLARE_BLOCK_YRD_VARS()
#if CONFIG_AV1_HIGHBITDEPTH
  DECLARE_BLOCK_YRD_HBD_VARS()
#else
  (void)use_hbd;
#endif

  // Keep track of the row and column of the blocks we use so that we know
  // if we are in the unrestricted motion border.
  for (int r = 0; r < max_blocks_high; r += block_step) {
    for (int c = 0, s = 0; c < max_blocks_wide; c += block_step, s += step) {
      DECLARE_LOOP_VARS_BLOCK_YRD()

      switch (tx_size) {
#if CONFIG_AV1_HIGHBITDEPTH
        case TX_16X16:
          if (use_hbd) {
            aom_hadamard_16x16(src_diff, diff_stride, coeff);
            av1_quantize_fp(coeff, 16 * 16, p->zbin_QTX, p->round_fp_QTX,
                            p->quant_fp_QTX, p->quant_shift_QTX, qcoeff,
                            dqcoeff, p->dequant_QTX, eob,
                            // default_scan_fp_16x16_transpose and
                            // av1_default_iscan_fp_16x16_transpose have to be
                            // used together.
                            default_scan_fp_16x16_transpose,
                            av1_default_iscan_fp_16x16_transpose);
          } else {
            aom_hadamard_lp_16x16(src_diff, diff_stride, low_coeff);
            av1_quantize_lp(low_coeff, 16 * 16, p->round_fp_QTX,
                            p->quant_fp_QTX, low_qcoeff, low_dqcoeff,
                            p->dequant_QTX, eob,
                            // default_scan_lp_16x16_transpose and
                            // av1_default_iscan_lp_16x16_transpose have to be
                            // used together.
                            default_scan_lp_16x16_transpose,
                            av1_default_iscan_lp_16x16_transpose);
          }
          break;
        case TX_8X8:
          if (use_hbd) {
            aom_hadamard_8x8(src_diff, diff_stride, coeff);
            av1_quantize_fp(
                coeff, 8 * 8, p->zbin_QTX, p->round_fp_QTX, p->quant_fp_QTX,
                p->quant_shift_QTX, qcoeff, dqcoeff, p->dequant_QTX, eob,
                default_scan_8x8_transpose, av1_default_iscan_8x8_transpose);
          } else {
            if (is_tx_8x8_dual_applicable) {
              // The coeffs are pre-computed for the whole block, so re-assign
              // low_coeff to the appropriate location.
              const int block_offset = BLOCK_OFFSET(block + s);
              low_coeff = (int16_t *)p->coeff + block_offset;
            } else {
              aom_hadamard_lp_8x8(src_diff, diff_stride, low_coeff);
            }
            av1_quantize_lp(
                low_coeff, 8 * 8, p->round_fp_QTX, p->quant_fp_QTX, low_qcoeff,
                low_dqcoeff, p->dequant_QTX, eob,
                // default_scan_8x8_transpose and
                // av1_default_iscan_8x8_transpose have to be used together.
                default_scan_8x8_transpose, av1_default_iscan_8x8_transpose);
          }
          break;
        default:
          assert(tx_size == TX_4X4);
          // In tx_size=4x4 case, aom_fdct4x4 and aom_fdct4x4_lp generate
          // normal coefficients order, so we don't need to change the scan
          // order here.
          if (use_hbd) {
            aom_fdct4x4(src_diff, coeff, diff_stride);
            av1_quantize_fp(coeff, 4 * 4, p->zbin_QTX, p->round_fp_QTX,
                            p->quant_fp_QTX, p->quant_shift_QTX, qcoeff,
                            dqcoeff, p->dequant_QTX, eob, scan_order->scan,
                            scan_order->iscan);
          } else {
            aom_fdct4x4_lp(src_diff, low_coeff, diff_stride);
            av1_quantize_lp(low_coeff, 4 * 4, p->round_fp_QTX, p->quant_fp_QTX,
                            low_qcoeff, low_dqcoeff, p->dequant_QTX, eob,
                            scan_order->scan, scan_order->iscan);
          }
          break;
#else
        case TX_16X16:
          aom_hadamard_lp_16x16(src_diff, diff_stride, low_coeff);
          av1_quantize_lp(low_coeff, 16 * 16, p->round_fp_QTX, p->quant_fp_QTX,
                          low_qcoeff, low_dqcoeff, p->dequant_QTX, eob,
                          default_scan_lp_16x16_transpose,
                          av1_default_iscan_lp_16x16_transpose);
          break;
        case TX_8X8:
          if (is_tx_8x8_dual_applicable) {
            // The coeffs are pre-computed for the whole block, so re-assign
            // low_coeff to the appropriate location.
            const int block_offset = BLOCK_OFFSET(block + s);
            low_coeff = (int16_t *)p->coeff + block_offset;
          } else {
            aom_hadamard_lp_8x8(src_diff, diff_stride, low_coeff);
          }
          av1_quantize_lp(low_coeff, 8 * 8, p->round_fp_QTX, p->quant_fp_QTX,
                          low_qcoeff, low_dqcoeff, p->dequant_QTX, eob,
                          default_scan_8x8_transpose,
                          av1_default_iscan_8x8_transpose);
          break;
        default:
          aom_fdct4x4_lp(src_diff, low_coeff, diff_stride);
          av1_quantize_lp(low_coeff, 4 * 4, p->round_fp_QTX, p->quant_fp_QTX,
                          low_qcoeff, low_dqcoeff, p->dequant_QTX, eob,
                          scan_order->scan, scan_order->iscan);
          break;
#endif
      }
      assert(*eob <= 1024);
#if CONFIG_AV1_HIGHBITDEPTH
      if (use_hbd)
        update_yrd_loop_vars_hbd(x, &temp_skippable, step, *eob, coeff, qcoeff,
                                 dqcoeff, this_rdc, &eob_cost,
                                 r * num_blk_skip_w + c);
      else
#endif
        update_yrd_loop_vars(x, &temp_skippable, step, *eob, low_coeff,
                             low_qcoeff, low_dqcoeff, this_rdc, &eob_cost,
                             r * num_blk_skip_w + c);
    }
    block += row_step;
  }

  this_rdc->skip_txfm = *skippable = temp_skippable;
  if (this_rdc->sse < INT64_MAX) {
    this_rdc->sse = (this_rdc->sse << 6) >> 2;
    if (temp_skippable) {
      this_rdc->dist = 0;
      this_rdc->dist = this_rdc->sse;
      return;
    }
  }

  // If skippable is set, rate gets clobbered later.
  this_rdc->rate <<= (2 + AV1_PROB_COST_SHIFT);
  this_rdc->rate += (eob_cost << AV1_PROB_COST_SHIFT);
}

// Explicitly enumerate the cases so the compiler can generate SIMD for the
// function. According to the disassembler, gcc generates SSE codes for each of
// the possible block sizes. The hottest case is tx_width 16, which takes up
// about 8% of the self cycle of av1_nonrd_pick_inter_mode_sb. Since
// av1_nonrd_pick_inter_mode_sb takes up about 3% of total encoding time, the
// potential room of improvement for writing AVX2 optimization is only 3% * 8% =
// 0.24% of total encoding time.
static AOM_INLINE void scale_square_buf_vals(int16_t *dst, int tx_width,
                                             const int16_t *src,
                                             int src_stride) {
#define DO_SCALING                                                   \
  do {                                                               \
    for (int idy = 0; idy < tx_width; ++idy) {                       \
      for (int idx = 0; idx < tx_width; ++idx) {                     \
        dst[idy * tx_width + idx] = src[idy * src_stride + idx] * 8; \
      }                                                              \
    }                                                                \
  } while (0)

  if (tx_width == 4) {
    DO_SCALING;
  } else if (tx_width == 8) {
    DO_SCALING;
  } else if (tx_width == 16) {
    DO_SCALING;
  } else {
    assert(0);
  }

#undef DO_SCALING
}

/*!\brief Calculates RD Cost when the block uses Identity transform.
 * Note that this function is only for low bit depth encoding, since it
 * is called in real-time mode for now, which sets high bit depth to 0:
 * -DCONFIG_AV1_HIGHBITDEPTH=0
 *
 * \ingroup nonrd_mode_search
 * \callgraph
 * \callergraph
 * Calculates RD Cost. For low bit depth this function
 * uses low-precision set of functions (16-bit) and 32 bit for high bit depth
 * \param[in]    x              Pointer to structure holding all the data for
                                the current macroblock
 * \param[in]    pred_buf       Pointer to the prediction buffer
 * \param[in]    pred_stride    Stride for the prediction buffer
 * \param[in]    this_rdc       Pointer to calculated RD Cost
 * \param[in]    skippable      Pointer to a flag indicating possible tx skip
 * \param[in]    bsize          Current block size
 * \param[in]    tx_size        Transform size
 *
 * \remark Nothing is returned. Instead, calculated RD cost is placed to
 * \c this_rdc. \c skippable flag is set if all coefficients are zero.
 */
void av1_block_yrd_idtx(MACROBLOCK *x, const uint8_t *const pred_buf,
                        int pred_stride, RD_STATS *this_rdc, int *skippable,
                        BLOCK_SIZE bsize, TX_SIZE tx_size) {
  MACROBLOCKD *xd = &x->e_mbd;
  struct macroblock_plane *const p = &x->plane[AOM_PLANE_Y];
  assert(bsize < BLOCK_SIZES_ALL);
  const int num_4x4_w = mi_size_wide[bsize];
  const int num_4x4_h = mi_size_high[bsize];
  const int step = 1 << (tx_size << 1);
  const int block_step = (1 << tx_size);
  const int max_blocks_wide =
      num_4x4_w + (xd->mb_to_right_edge >= 0 ? 0 : xd->mb_to_right_edge >> 5);
  const int max_blocks_high =
      num_4x4_h + (xd->mb_to_bottom_edge >= 0 ? 0 : xd->mb_to_bottom_edge >> 5);
  int eob_cost = 0;
  const int bw = 4 * num_4x4_w;
  const int bh = 4 * num_4x4_h;
  const int num_blk_skip_w = num_4x4_w;
  // Keep the intermediate value on the stack here. Writing directly to
  // skippable causes speed regression due to load-and-store issues in
  // update_yrd_loop_vars.
  int temp_skippable = 1;
  int tx_wd = 0;
  const SCAN_ORDER *scan_order = NULL;
  switch (tx_size) {
    case TX_64X64:
      assert(0);  // Not implemented
      break;
    case TX_32X32:
      assert(0);  // Not used
      break;
    case TX_16X16:
      scan_order = &av1_fast_idtx_scan_order_16x16;
      tx_wd = 16;
      break;
    case TX_8X8:
      scan_order = &av1_fast_idtx_scan_order_8x8;
      tx_wd = 8;
      break;
    default:
      assert(tx_size == TX_4X4);
      scan_order = &av1_fast_idtx_scan_order_4x4;
      tx_wd = 4;
      break;
  }
  assert(scan_order != NULL);

  this_rdc->dist = 0;
  this_rdc->rate = 0;
  aom_subtract_block(bh, bw, p->src_diff, bw, p->src.buf, p->src.stride,
                     pred_buf, pred_stride);
  // Keep track of the row and column of the blocks we use so that we know
  // if we are in the unrestricted motion border.
  DECLARE_BLOCK_YRD_BUFFERS()
  DECLARE_BLOCK_YRD_VARS()
  for (int r = 0; r < max_blocks_high; r += block_step) {
    for (int c = 0, s = 0; c < max_blocks_wide; c += block_step, s += step) {
      DECLARE_LOOP_VARS_BLOCK_YRD()
      scale_square_buf_vals(low_coeff, tx_wd, src_diff, diff_stride);
      av1_quantize_lp(low_coeff, tx_wd * tx_wd, p->round_fp_QTX,
                      p->quant_fp_QTX, low_qcoeff, low_dqcoeff, p->dequant_QTX,
                      eob, scan_order->scan, scan_order->iscan);
      assert(*eob <= 1024);
      update_yrd_loop_vars(x, &temp_skippable, step, *eob, low_coeff,
                           low_qcoeff, low_dqcoeff, this_rdc, &eob_cost,
                           r * num_blk_skip_w + c);
    }
  }
  this_rdc->skip_txfm = *skippable = temp_skippable;
  if (this_rdc->sse < INT64_MAX) {
    this_rdc->sse = (this_rdc->sse << 6) >> 2;
    if (temp_skippable) {
      this_rdc->dist = 0;
      this_rdc->dist = this_rdc->sse;
      return;
    }
  }
  // If skippable is set, rate gets clobbered later.
  this_rdc->rate <<= (2 + AV1_PROB_COST_SHIFT);
  this_rdc->rate += (eob_cost << AV1_PROB_COST_SHIFT);
}

int64_t av1_model_rd_for_sb_uv(AV1_COMP *cpi, BLOCK_SIZE plane_bsize,
                               MACROBLOCK *x, MACROBLOCKD *xd,
                               RD_STATS *this_rdc, int start_plane,
                               int stop_plane) {
  // Note our transform coeffs are 8 times an orthogonal transform.
  // Hence quantizer step is also 8 times. To get effective quantizer
  // we need to divide by 8 before sending to modeling function.
  unsigned int sse;
  int rate;
  int64_t dist;
  int plane;
  int64_t tot_sse = 0;

  this_rdc->rate = 0;
  this_rdc->dist = 0;
  this_rdc->skip_txfm = 0;

  for (plane = start_plane; plane <= stop_plane; ++plane) {
    struct macroblock_plane *const p = &x->plane[plane];
    struct macroblockd_plane *const pd = &xd->plane[plane];
    const uint32_t dc_quant = p->dequant_QTX[0];
    const uint32_t ac_quant = p->dequant_QTX[1];
    const BLOCK_SIZE bs = plane_bsize;
    unsigned int var;
    if (!x->color_sensitivity[COLOR_SENS_IDX(plane)]) continue;

    var = cpi->ppi->fn_ptr[bs].vf(p->src.buf, p->src.stride, pd->dst.buf,
                                  pd->dst.stride, &sse);
    assert(sse >= var);
    tot_sse += sse;

    av1_model_rd_from_var_lapndz(sse - var, num_pels_log2_lookup[bs],
                                 dc_quant >> 3, &rate, &dist);

    this_rdc->rate += rate >> 1;
    this_rdc->dist += dist << 3;

    av1_model_rd_from_var_lapndz(var, num_pels_log2_lookup[bs], ac_quant >> 3,
                                 &rate, &dist);

    this_rdc->rate += rate;
    this_rdc->dist += dist << 4;
  }

  if (this_rdc->rate == 0) {
    this_rdc->skip_txfm = 1;
  }

  if (RDCOST(x->rdmult, this_rdc->rate, this_rdc->dist) >=
      RDCOST(x->rdmult, 0, tot_sse << 4)) {
    this_rdc->rate = 0;
    this_rdc->dist = tot_sse << 4;
    this_rdc->skip_txfm = 1;
  }

  return tot_sse;
}

static void compute_intra_yprediction(const AV1_COMMON *cm,
                                      PREDICTION_MODE mode, BLOCK_SIZE bsize,
                                      MACROBLOCK *x, MACROBLOCKD *xd) {
  const SequenceHeader *seq_params = cm->seq_params;
  struct macroblockd_plane *const pd = &xd->plane[AOM_PLANE_Y];
  struct macroblock_plane *const p = &x->plane[AOM_PLANE_Y];
  uint8_t *const src_buf_base = p->src.buf;
  uint8_t *const dst_buf_base = pd->dst.buf;
  const int src_stride = p->src.stride;
  const int dst_stride = pd->dst.stride;
  int plane = 0;
  int row, col;
  // block and transform sizes, in number of 4x4 blocks log 2 ("*_b")
  // 4x4=0, 8x8=2, 16x16=4, 32x32=6, 64x64=8
  // transform size varies per plane, look it up in a common way.
  const TX_SIZE tx_size = max_txsize_lookup[bsize];
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(bsize, pd->subsampling_x, pd->subsampling_y);
  // If mb_to_right_edge is < 0 we are in a situation in which
  // the current block size extends into the UMV and we won't
  // visit the sub blocks that are wholly within the UMV.
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  // Keep track of the row and column of the blocks we use so that we know
  // if we are in the unrestricted motion border.
  for (row = 0; row < max_blocks_high; row += (1 << tx_size)) {
    // Skip visiting the sub blocks that are wholly within the UMV.
    for (col = 0; col < max_blocks_wide; col += (1 << tx_size)) {
      p->src.buf = &src_buf_base[4 * (row * (int64_t)src_stride + col)];
      pd->dst.buf = &dst_buf_base[4 * (row * (int64_t)dst_stride + col)];
      av1_predict_intra_block(
          xd, seq_params->sb_size, seq_params->enable_intra_edge_filter,
          block_size_wide[bsize], block_size_high[bsize], tx_size, mode, 0, 0,
          FILTER_INTRA_MODES, pd->dst.buf, dst_stride, pd->dst.buf, dst_stride,
          0, 0, plane);
    }
  }
  p->src.buf = src_buf_base;
  pd->dst.buf = dst_buf_base;
}

// Checks whether Intra mode needs to be pruned based on
// 'intra_y_mode_bsize_mask_nrd' and 'prune_hv_pred_modes_using_blksad'
// speed features.
static INLINE bool is_prune_intra_mode(
    AV1_COMP *cpi, int mode_index, int force_intra_check, BLOCK_SIZE bsize,
    uint8_t segment_id, SOURCE_SAD source_sad_nonrd,
    uint8_t color_sensitivity[MAX_MB_PLANE - 1]) {
  const PREDICTION_MODE this_mode = intra_mode_list[mode_index];
  if (mode_index > 2 || force_intra_check == 0) {
    if (!((1 << this_mode) & cpi->sf.rt_sf.intra_y_mode_bsize_mask_nrd[bsize]))
      return true;

    if (this_mode == DC_PRED) return false;

    if (!cpi->sf.rt_sf.prune_hv_pred_modes_using_src_sad) return false;

    const bool has_color_sensitivity =
        color_sensitivity[COLOR_SENS_IDX(AOM_PLANE_U)] &&
        color_sensitivity[COLOR_SENS_IDX(AOM_PLANE_V)];
    if (has_color_sensitivity &&
        (cpi->rc.frame_source_sad > 1.1 * cpi->rc.avg_source_sad ||
         cyclic_refresh_segment_id_boosted(segment_id) ||
         source_sad_nonrd > kMedSad))
      return false;

    return true;
  }
  return false;
}

/*!\brief Estimation of RD cost of an intra mode for Non-RD optimized case.
 *
 * \ingroup nonrd_mode_search
 * \callgraph
 * \callergraph
 * Calculates RD Cost for an intra mode for a single TX block using Hadamard
 * transform.
 * \param[in]    plane          Color plane
 * \param[in]    block          Index of a TX block in a prediction block
 * \param[in]    row            Row of a current TX block
 * \param[in]    col            Column of a current TX block
 * \param[in]    plane_bsize    Block size of a current prediction block
 * \param[in]    tx_size        Transform size
 * \param[in]    arg            Pointer to a structure that holds parameters
 *                              for intra mode search
 *
 * \remark Nothing is returned. Instead, best mode and RD Cost of the best mode
 * are set in \c args->rdc and \c args->mode
 */
void av1_estimate_block_intra(int plane, int block, int row, int col,
                              BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                              void *arg) {
  struct estimate_block_intra_args *const args = arg;
  AV1_COMP *const cpi = args->cpi;
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const BLOCK_SIZE bsize_tx = txsize_to_bsize[tx_size];
  uint8_t *const src_buf_base = p->src.buf;
  uint8_t *const dst_buf_base = pd->dst.buf;
  const int64_t src_stride = p->src.stride;
  const int64_t dst_stride = pd->dst.stride;

  (void)block;

  av1_predict_intra_block_facade(cm, xd, plane, col, row, tx_size);

  if (args->prune_mode_based_on_sad) {
    unsigned int this_sad = cpi->ppi->fn_ptr[plane_bsize].sdf(
        p->src.buf, p->src.stride, pd->dst.buf, pd->dst.stride);
    const unsigned int sad_threshold =
        args->best_sad != UINT_MAX ? args->best_sad + (args->best_sad >> 4)
                                   : UINT_MAX;
    // Skip the evaluation of current mode if its SAD is more than a threshold.
    if (this_sad > sad_threshold) {
      // For the current mode, set rate and distortion to maximum possible
      // values and return.
      // Note: args->rdc->rate is checked in av1_nonrd_pick_intra_mode() to skip
      // the evaluation of the current mode.
      args->rdc->rate = INT_MAX;
      args->rdc->dist = INT64_MAX;
      return;
    }
    if (this_sad < args->best_sad) {
      args->best_sad = this_sad;
    }
  }

  RD_STATS this_rdc;
  av1_invalid_rd_stats(&this_rdc);

  p->src.buf = &src_buf_base[4 * (row * src_stride + col)];
  pd->dst.buf = &dst_buf_base[4 * (row * dst_stride + col)];

  if (plane == 0) {
    av1_block_yrd(x, &this_rdc, &args->skippable, bsize_tx,
                  AOMMIN(tx_size, TX_16X16));
  } else {
    av1_model_rd_for_sb_uv(cpi, bsize_tx, x, xd, &this_rdc, plane, plane);
  }

  p->src.buf = src_buf_base;
  pd->dst.buf = dst_buf_base;
  assert(args->rdc->rate != INT_MAX && args->rdc->dist != INT64_MAX);
  args->rdc->rate += this_rdc.rate;
  args->rdc->dist += this_rdc.dist;
}

/*!\brief Estimates best intra mode for inter mode search
 *
 * \ingroup nonrd_mode_search
 * \callgraph
 * \callergraph
 *
 * Using heuristics based on best inter mode, block size, and other decides
 * whether to check intra modes. If so, estimates and selects best intra mode
 * from the reduced set of intra modes (max 4 intra modes checked)
 *
 * \param[in]    cpi                      Top-level encoder structure
 * \param[in]    x                        Pointer to structure holding all the
 *                                        data for the current macroblock
 * \param[in]    bsize                    Current block size
 * \param[in]    best_early_term          Flag, indicating that TX for the
 *                                        best inter mode was skipped
 * \param[in]    ref_cost_intra           Cost of signalling intra mode
 * \param[in]    reuse_prediction         Flag, indicating prediction re-use
 * \param[in]    orig_dst                 Original destination buffer
 * \param[in]    tmp_buffers              Pointer to a temporary buffers for
 *                                        prediction re-use
 * \param[out]   this_mode_pred           Pointer to store prediction buffer
 *                                        for prediction re-use
 * \param[in]    best_rdc                 Pointer to RD cost for the best
 *                                        selected intra mode
 * \param[in]    best_pickmode            Pointer to a structure containing
 *                                        best mode picked so far
 * \param[in]    ctx                      Pointer to structure holding coding
 *                                        contexts and modes for the block
 *
 * \remark Nothing is returned. Instead, calculated RD cost is placed to
 * \c best_rdc and best selected mode is placed to \c best_pickmode
 *
 */
void av1_estimate_intra_mode(AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bsize,
                             int best_early_term, unsigned int ref_cost_intra,
                             int reuse_prediction, struct buf_2d *orig_dst,
                             PRED_BUFFER *tmp_buffers,
                             PRED_BUFFER **this_mode_pred, RD_STATS *best_rdc,
                             BEST_PICKMODE *best_pickmode,
                             PICK_MODE_CONTEXT *ctx) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mi = xd->mi[0];
  const TxfmSearchParams *txfm_params = &x->txfm_search_params;
  const unsigned char segment_id = mi->segment_id;
  const int *const rd_threshes = cpi->rd.threshes[segment_id][bsize];
  const int *const rd_thresh_freq_fact = x->thresh_freq_fact[bsize];
  const bool is_screen_content =
      cpi->oxcf.tune_cfg.content == AOM_CONTENT_SCREEN;
  struct macroblockd_plane *const pd = &xd->plane[AOM_PLANE_Y];
  const REAL_TIME_SPEED_FEATURES *const rt_sf = &cpi->sf.rt_sf;

  const CommonQuantParams *quant_params = &cm->quant_params;

  RD_STATS this_rdc;

  int intra_cost_penalty = av1_get_intra_cost_penalty(
      quant_params->base_qindex, quant_params->y_dc_delta_q,
      cm->seq_params->bit_depth);
  int64_t inter_mode_thresh =
      RDCOST(x->rdmult, ref_cost_intra + intra_cost_penalty, 0);
  int perform_intra_pred = rt_sf->check_intra_pred_nonrd;
  int force_intra_check = 0;
  // For spatial enhancement layer: turn off intra prediction if the
  // previous spatial layer as golden ref is not chosen as best reference.
  // only do this for temporal enhancement layer and on non-key frames.
  if (cpi->svc.spatial_layer_id > 0 &&
      best_pickmode->best_ref_frame != GOLDEN_FRAME &&
      cpi->svc.temporal_layer_id > 0 &&
      !cpi->svc.layer_context[cpi->svc.temporal_layer_id].is_key_frame)
    perform_intra_pred = 0;

  int do_early_exit_rdthresh = 1;

  uint32_t spatial_var_thresh = 50;
  int motion_thresh = 32;
  // Adjust thresholds to make intra mode likely tested if the other
  // references (golden, alt) are skipped/not checked. For now always
  // adjust for svc mode.
  if (cpi->ppi->use_svc || (rt_sf->use_nonrd_altref_frame == 0 &&
                            rt_sf->nonrd_prune_ref_frame_search > 0)) {
    spatial_var_thresh = 150;
    motion_thresh = 0;
  }

  // Some adjustments to checking intra mode based on source variance.
  if (x->source_variance < spatial_var_thresh) {
    // If the best inter mode is large motion or non-LAST ref reduce intra cost
    // penalty, so intra mode is more likely tested.
    if (best_rdc->rdcost != INT64_MAX &&
        (best_pickmode->best_ref_frame != LAST_FRAME ||
         abs(mi->mv[0].as_mv.row) >= motion_thresh ||
         abs(mi->mv[0].as_mv.col) >= motion_thresh)) {
      intra_cost_penalty = intra_cost_penalty >> 2;
      inter_mode_thresh =
          RDCOST(x->rdmult, ref_cost_intra + intra_cost_penalty, 0);
      do_early_exit_rdthresh = 0;
    }
    if ((x->source_variance < AOMMAX(50, (spatial_var_thresh >> 1)) &&
         x->content_state_sb.source_sad_nonrd >= kHighSad) ||
        (is_screen_content && x->source_variance < 50 &&
         ((bsize >= BLOCK_32X32 &&
           x->content_state_sb.source_sad_nonrd != kZeroSad) ||
          x->color_sensitivity[COLOR_SENS_IDX(AOM_PLANE_U)] == 1 ||
          x->color_sensitivity[COLOR_SENS_IDX(AOM_PLANE_V)] == 1)))
      force_intra_check = 1;
    // For big blocks worth checking intra (since only DC will be checked),
    // even if best_early_term is set.
    if (bsize >= BLOCK_32X32) best_early_term = 0;
  } else if (rt_sf->source_metrics_sb_nonrd &&
             x->content_state_sb.source_sad_nonrd <= kLowSad) {
    perform_intra_pred = 0;
  }

  if (best_rdc->skip_txfm && best_pickmode->best_mode_initial_skip_flag) {
    if (rt_sf->skip_intra_pred == 1 && best_pickmode->best_mode != NEWMV)
      perform_intra_pred = 0;
    else if (rt_sf->skip_intra_pred == 2)
      perform_intra_pred = 0;
  }

  if (!(best_rdc->rdcost == INT64_MAX || force_intra_check ||
        (perform_intra_pred && !best_early_term &&
         bsize <= cpi->sf.part_sf.max_intra_bsize))) {
    return;
  }

  // Early exit based on RD cost calculated using known rate. When
  // is_screen_content is true, more bias is given to intra modes. Hence,
  // considered conservative threshold in early exit for the same.
  const int64_t known_rd = is_screen_content
                               ? CALC_BIASED_RDCOST(inter_mode_thresh)
                               : inter_mode_thresh;
  if (known_rd > best_rdc->rdcost) return;

  struct estimate_block_intra_args args;
  init_estimate_block_intra_args(&args, cpi, x);
  TX_SIZE intra_tx_size = AOMMIN(
      AOMMIN(max_txsize_lookup[bsize],
             tx_mode_to_biggest_tx_size[txfm_params->tx_mode_search_type]),
      TX_16X16);
  if (is_screen_content && cpi->rc.high_source_sad &&
      x->source_variance > spatial_var_thresh && bsize <= BLOCK_16X16)
    intra_tx_size = TX_4X4;

  PRED_BUFFER *const best_pred = best_pickmode->best_pred;
  if (reuse_prediction && best_pred != NULL) {
    const int bh = block_size_high[bsize];
    const int bw = block_size_wide[bsize];
    if (best_pred->data == orig_dst->buf) {
      *this_mode_pred = &tmp_buffers[get_pred_buffer(tmp_buffers, 3)];
      aom_convolve_copy(best_pred->data, best_pred->stride,
                        (*this_mode_pred)->data, (*this_mode_pred)->stride, bw,
                        bh);
      best_pickmode->best_pred = *this_mode_pred;
    }
  }
  pd->dst = *orig_dst;

  for (int midx = 0; midx < RTC_INTRA_MODES; ++midx) {
    const PREDICTION_MODE this_mode = intra_mode_list[midx];
    const THR_MODES mode_index = mode_idx[INTRA_FRAME][mode_offset(this_mode)];
    const int64_t mode_rd_thresh = rd_threshes[mode_index];

    if (is_prune_intra_mode(cpi, midx, force_intra_check, bsize, segment_id,
                            x->content_state_sb.source_sad_nonrd,
                            x->color_sensitivity))
      continue;

    if (is_screen_content && rt_sf->source_metrics_sb_nonrd) {
      // For spatially flat blocks with zero motion only check
      // DC mode.
      if (x->content_state_sb.source_sad_nonrd == kZeroSad &&
          x->source_variance == 0 && this_mode != DC_PRED)
        continue;
      // Only test Intra for big blocks if spatial_variance is small.
      else if (bsize > BLOCK_32X32 && x->source_variance > 50)
        continue;
    }

    if (rd_less_than_thresh(best_rdc->rdcost, mode_rd_thresh,
                            rd_thresh_freq_fact[mode_index]) &&
        (do_early_exit_rdthresh || this_mode == SMOOTH_PRED)) {
      continue;
    }
    const BLOCK_SIZE uv_bsize =
        get_plane_block_size(bsize, xd->plane[AOM_PLANE_U].subsampling_x,
                             xd->plane[AOM_PLANE_U].subsampling_y);

    mi->mode = this_mode;
    mi->ref_frame[0] = INTRA_FRAME;
    mi->ref_frame[1] = NONE_FRAME;

    av1_invalid_rd_stats(&this_rdc);
    args.mode = this_mode;
    args.skippable = 1;
    args.rdc = &this_rdc;
    mi->tx_size = intra_tx_size;
    compute_intra_yprediction(cm, this_mode, bsize, x, xd);
    // Look into selecting tx_size here, based on prediction residual.
    av1_block_yrd(x, &this_rdc, &args.skippable, bsize, mi->tx_size);
    // TODO(kyslov@) Need to account for skippable
    if (x->color_sensitivity[COLOR_SENS_IDX(AOM_PLANE_U)]) {
      av1_foreach_transformed_block_in_plane(xd, uv_bsize, AOM_PLANE_U,
                                             av1_estimate_block_intra, &args);
    }
    if (x->color_sensitivity[COLOR_SENS_IDX(AOM_PLANE_V)]) {
      av1_foreach_transformed_block_in_plane(xd, uv_bsize, AOM_PLANE_V,
                                             av1_estimate_block_intra, &args);
    }

    int mode_cost = 0;
    if (av1_is_directional_mode(this_mode) && av1_use_angle_delta(bsize)) {
      mode_cost +=
          x->mode_costs.angle_delta_cost[this_mode - V_PRED]
                                        [MAX_ANGLE_DELTA +
                                         mi->angle_delta[PLANE_TYPE_Y]];
    }
    if (this_mode == DC_PRED && av1_filter_intra_allowed_bsize(cm, bsize)) {
      mode_cost += x->mode_costs.filter_intra_cost[bsize][0];
    }
    this_rdc.rate += ref_cost_intra;
    this_rdc.rate += intra_cost_penalty;
    this_rdc.rate += mode_cost;
    this_rdc.rdcost = RDCOST(x->rdmult, this_rdc.rate, this_rdc.dist);

    if (is_screen_content && rt_sf->source_metrics_sb_nonrd) {
      // For blocks with low spatial variance and color sad,
      // favor the intra-modes, only on scene/slide change.
      if (cpi->rc.high_source_sad && x->source_variance < 800 &&
          (x->color_sensitivity[COLOR_SENS_IDX(AOM_PLANE_U)] ||
           x->color_sensitivity[COLOR_SENS_IDX(AOM_PLANE_V)]))
        this_rdc.rdcost = CALC_BIASED_RDCOST(this_rdc.rdcost);
      // Otherwise bias against intra for blocks with zero
      // motion and no color, on non-scene/slide changes.
      else if (!cpi->rc.high_source_sad && x->source_variance > 0 &&
               x->content_state_sb.source_sad_nonrd == kZeroSad &&
               x->color_sensitivity[COLOR_SENS_IDX(AOM_PLANE_U)] == 0 &&
               x->color_sensitivity[COLOR_SENS_IDX(AOM_PLANE_V)] == 0)
        this_rdc.rdcost = (3 * this_rdc.rdcost) >> 1;
    }

    if (this_rdc.rdcost < best_rdc->rdcost) {
      *best_rdc = this_rdc;
      best_pickmode->best_mode = this_mode;
      best_pickmode->best_tx_size = mi->tx_size;
      best_pickmode->best_ref_frame = INTRA_FRAME;
      best_pickmode->best_second_ref_frame = NONE;
      best_pickmode->best_mode_skip_txfm = this_rdc.skip_txfm;
      mi->uv_mode = this_mode;
      mi->mv[0].as_int = INVALID_MV;
      mi->mv[1].as_int = INVALID_MV;
      if (!this_rdc.skip_txfm)
        memset(ctx->blk_skip, 0,
               sizeof(x->txfm_search_info.blk_skip[0]) * ctx->num_4x4_blk);
    }
  }
  if (best_pickmode->best_ref_frame == INTRA_FRAME)
    memset(ctx->blk_skip, 0,
           sizeof(x->txfm_search_info.blk_skip[0]) * ctx->num_4x4_blk);
  mi->tx_size = best_pickmode->best_tx_size;
}
