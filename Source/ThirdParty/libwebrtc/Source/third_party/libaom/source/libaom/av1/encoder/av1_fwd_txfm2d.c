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

#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/txfm_common.h"
#include "av1/common/enums.h"
#include "av1/common/av1_txfm.h"
#include "av1/encoder/av1_fwd_txfm1d.h"
#include "av1/encoder/av1_fwd_txfm1d_cfg.h"

static INLINE TxfmFunc fwd_txfm_type_to_func(TXFM_TYPE txfm_type) {
  switch (txfm_type) {
    case TXFM_TYPE_DCT4: return av1_fdct4;
    case TXFM_TYPE_DCT8: return av1_fdct8;
    case TXFM_TYPE_DCT16: return av1_fdct16;
    case TXFM_TYPE_DCT32: return av1_fdct32;
    case TXFM_TYPE_DCT64: return av1_fdct64;
    case TXFM_TYPE_ADST4: return av1_fadst4;
    case TXFM_TYPE_ADST8: return av1_fadst8;
    case TXFM_TYPE_ADST16: return av1_fadst16;
    case TXFM_TYPE_IDENTITY4: return av1_fidentity4_c;
    case TXFM_TYPE_IDENTITY8: return av1_fidentity8_c;
    case TXFM_TYPE_IDENTITY16: return av1_fidentity16_c;
    case TXFM_TYPE_IDENTITY32: return av1_fidentity32_c;
    default: assert(0); return NULL;
  }
}

void av1_gen_fwd_stage_range(int8_t *stage_range_col, int8_t *stage_range_row,
                             const TXFM_2D_FLIP_CFG *cfg, int bd) {
  // Take the shift from the larger dimension in the rectangular case.
  const int8_t *shift = cfg->shift;
  // i < MAX_TXFM_STAGE_NUM will mute above array bounds warning
  for (int i = 0; i < cfg->stage_num_col && i < MAX_TXFM_STAGE_NUM; ++i) {
    stage_range_col[i] = cfg->stage_range_col[i] + shift[0] + bd + 1;
  }

  // i < MAX_TXFM_STAGE_NUM will mute above array bounds warning
  for (int i = 0; i < cfg->stage_num_row && i < MAX_TXFM_STAGE_NUM; ++i) {
    stage_range_row[i] = cfg->stage_range_row[i] + shift[0] + shift[1] + bd + 1;
  }
}

static INLINE void fwd_txfm2d_c(const int16_t *input, int32_t *output,
                                const int stride, const TXFM_2D_FLIP_CFG *cfg,
                                int32_t *buf, int bd) {
  int c, r;
  // Note when assigning txfm_size_col, we use the txfm_size from the
  // row configuration and vice versa. This is intentionally done to
  // accurately perform rectangular transforms. When the transform is
  // rectangular, the number of columns will be the same as the
  // txfm_size stored in the row cfg struct. It will make no difference
  // for square transforms.
  const int txfm_size_col = tx_size_wide[cfg->tx_size];
  const int txfm_size_row = tx_size_high[cfg->tx_size];
  // Take the shift from the larger dimension in the rectangular case.
  const int8_t *shift = cfg->shift;
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
  int8_t stage_range_col[MAX_TXFM_STAGE_NUM];
  int8_t stage_range_row[MAX_TXFM_STAGE_NUM];
  assert(cfg->stage_num_col <= MAX_TXFM_STAGE_NUM);
  assert(cfg->stage_num_row <= MAX_TXFM_STAGE_NUM);
  av1_gen_fwd_stage_range(stage_range_col, stage_range_row, cfg, bd);

  const int8_t cos_bit_col = cfg->cos_bit_col;
  const int8_t cos_bit_row = cfg->cos_bit_row;
  const TxfmFunc txfm_func_col = fwd_txfm_type_to_func(cfg->txfm_type_col);
  const TxfmFunc txfm_func_row = fwd_txfm_type_to_func(cfg->txfm_type_row);

  // use output buffer as temp buffer
  int32_t *temp_in = output;
  int32_t *temp_out = output + txfm_size_row;

  // Columns
  for (c = 0; c < txfm_size_col; ++c) {
    if (cfg->ud_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r) temp_in[r] = input[r * stride + c];
    } else {
      for (r = 0; r < txfm_size_row; ++r)
        // flip upside down
        temp_in[r] = input[(txfm_size_row - r - 1) * stride + c];
    }
    av1_round_shift_array(temp_in, txfm_size_row, -shift[0]);
    txfm_func_col(temp_in, temp_out, cos_bit_col, stage_range_col);
    av1_round_shift_array(temp_out, txfm_size_row, -shift[1]);
    if (cfg->lr_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r)
        buf[r * txfm_size_col + c] = temp_out[r];
    } else {
      for (r = 0; r < txfm_size_row; ++r)
        // flip from left to right
        buf[r * txfm_size_col + (txfm_size_col - c - 1)] = temp_out[r];
    }
  }

  // Rows
  for (r = 0; r < txfm_size_row; ++r) {
    txfm_func_row(buf + r * txfm_size_col, output + r * txfm_size_col,
                  cos_bit_row, stage_range_row);
    av1_round_shift_array(output + r * txfm_size_col, txfm_size_col, -shift[2]);
    if (abs(rect_type) == 1) {
      // Multiply everything by Sqrt2 if the transform is rectangular and the
      // size difference is a factor of 2.
      for (c = 0; c < txfm_size_col; ++c) {
        output[r * txfm_size_col + c] = round_shift(
            (int64_t)output[r * txfm_size_col + c] * NewSqrt2, NewSqrt2Bits);
      }
    }
  }
}

void av1_fwd_txfm2d_4x8_c(const int16_t *input, int32_t *output, int stride,
                          TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int32_t, txfm_buf[4 * 8]);
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_4X8, &cfg);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_8x4_c(const int16_t *input, int32_t *output, int stride,
                          TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[8 * 4];
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_8X4, &cfg);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_8x16_c(const int16_t *input, int32_t *output, int stride,
                           TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int32_t, txfm_buf[8 * 16]);
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_8X16, &cfg);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_16x8_c(const int16_t *input, int32_t *output, int stride,
                           TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[16 * 8];
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_16X8, &cfg);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_16x32_c(const int16_t *input, int32_t *output, int stride,
                            TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int32_t, txfm_buf[16 * 32]);
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_16X32, &cfg);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_32x16_c(const int16_t *input, int32_t *output, int stride,
                            TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[32 * 16];
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_32X16, &cfg);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_4x16_c(const int16_t *input, int32_t *output, int stride,
                           TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int32_t, txfm_buf[4 * 16]);
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_4X16, &cfg);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_16x4_c(const int16_t *input, int32_t *output, int stride,
                           TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[16 * 4];
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_16X4, &cfg);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_8x32_c(const int16_t *input, int32_t *output, int stride,
                           TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int32_t, txfm_buf[32 * 8]);
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_8X32, &cfg);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_32x8_c(const int16_t *input, int32_t *output, int stride,
                           TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[32 * 8];
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_32X8, &cfg);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_4x4_c(const int16_t *input, int32_t *output, int stride,
                          TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[4 * 4];
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_4X4, &cfg);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_8x8_c(const int16_t *input, int32_t *output, int stride,
                          TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[8 * 8];
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_8X8, &cfg);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_16x16_c(const int16_t *input, int32_t *output, int stride,
                            TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[16 * 16];
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_16X16, &cfg);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_32x32_c(const int16_t *input, int32_t *output, int stride,
                            TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[32 * 32];
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_32X32, &cfg);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_64x64_c(const int16_t *input, int32_t *output, int stride,
                            TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[64 * 64];
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_64X64, &cfg);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);

  // Zero out top-right 32x32 area.
  for (int row = 0; row < 32; ++row) {
    memset(output + row * 64 + 32, 0, 32 * sizeof(*output));
  }
  // Zero out the bottom 64x32 area.
  memset(output + 32 * 64, 0, 32 * 64 * sizeof(*output));
  // Re-pack non-zero coeffs in the first 32x32 indices.
  for (int row = 1; row < 32; ++row) {
    memcpy(output + row * 32, output + row * 64, 32 * sizeof(*output));
  }
}

void av1_fwd_txfm2d_32x64_c(const int16_t *input, int32_t *output, int stride,
                            TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int32_t, txfm_buf[32 * 64]);
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_32X64, &cfg);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
  // Zero out the bottom 32x32 area.
  memset(output + 32 * 32, 0, 32 * 32 * sizeof(*output));
  // Note: no repacking needed here.
}

void av1_fwd_txfm2d_64x32_c(const int16_t *input, int32_t *output, int stride,
                            TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[64 * 32];
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_64X32, &cfg);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);

  // Zero out right 32x32 area.
  for (int row = 0; row < 32; ++row) {
    memset(output + row * 64 + 32, 0, 32 * sizeof(*output));
  }
  // Re-pack non-zero coeffs in the first 32x32 indices.
  for (int row = 1; row < 32; ++row) {
    memcpy(output + row * 32, output + row * 64, 32 * sizeof(*output));
  }
}

void av1_fwd_txfm2d_16x64_c(const int16_t *input, int32_t *output, int stride,
                            TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(32, int32_t, txfm_buf[64 * 16]);
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_16X64, &cfg);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
  // Zero out the bottom 16x32 area.
  memset(output + 16 * 32, 0, 16 * 32 * sizeof(*output));
  // Note: no repacking needed here.
}

void av1_fwd_txfm2d_64x16_c(const int16_t *input, int32_t *output, int stride,
                            TX_TYPE tx_type, int bd) {
  int32_t txfm_buf[64 * 16];
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_64X16, &cfg);
  fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
  // Zero out right 32x16 area.
  for (int row = 0; row < 16; ++row) {
    memset(output + row * 64 + 32, 0, 32 * sizeof(*output));
  }
  // Re-pack non-zero coeffs in the first 32x16 indices.
  for (int row = 1; row < 16; ++row) {
    memcpy(output + row * 32, output + row * 64, 32 * sizeof(*output));
  }
}

static const int8_t fwd_shift_4x4[3] = { 2, 0, 0 };
static const int8_t fwd_shift_8x8[3] = { 2, -1, 0 };
static const int8_t fwd_shift_16x16[3] = { 2, -2, 0 };
static const int8_t fwd_shift_32x32[3] = { 2, -4, 0 };
static const int8_t fwd_shift_64x64[3] = { 0, -2, -2 };
static const int8_t fwd_shift_4x8[3] = { 2, -1, 0 };
static const int8_t fwd_shift_8x4[3] = { 2, -1, 0 };
static const int8_t fwd_shift_8x16[3] = { 2, -2, 0 };
static const int8_t fwd_shift_16x8[3] = { 2, -2, 0 };
static const int8_t fwd_shift_16x32[3] = { 2, -4, 0 };
static const int8_t fwd_shift_32x16[3] = { 2, -4, 0 };
static const int8_t fwd_shift_32x64[3] = { 0, -2, -2 };
static const int8_t fwd_shift_64x32[3] = { 2, -4, -2 };
static const int8_t fwd_shift_4x16[3] = { 2, -1, 0 };
static const int8_t fwd_shift_16x4[3] = { 2, -1, 0 };
static const int8_t fwd_shift_8x32[3] = { 2, -2, 0 };
static const int8_t fwd_shift_32x8[3] = { 2, -2, 0 };
static const int8_t fwd_shift_16x64[3] = { 0, -2, 0 };
static const int8_t fwd_shift_64x16[3] = { 2, -4, 0 };

const int8_t *av1_fwd_txfm_shift_ls[TX_SIZES_ALL] = {
  fwd_shift_4x4,   fwd_shift_8x8,   fwd_shift_16x16, fwd_shift_32x32,
  fwd_shift_64x64, fwd_shift_4x8,   fwd_shift_8x4,   fwd_shift_8x16,
  fwd_shift_16x8,  fwd_shift_16x32, fwd_shift_32x16, fwd_shift_32x64,
  fwd_shift_64x32, fwd_shift_4x16,  fwd_shift_16x4,  fwd_shift_8x32,
  fwd_shift_32x8,  fwd_shift_16x64, fwd_shift_64x16,
};

const int8_t av1_fwd_cos_bit_col[MAX_TXWH_IDX /*txw_idx*/]
                                [MAX_TXWH_IDX /*txh_idx*/] = {
                                  { 13, 13, 13, 0, 0 },
                                  { 13, 13, 13, 12, 0 },
                                  { 13, 13, 13, 12, 13 },
                                  { 0, 13, 13, 12, 13 },
                                  { 0, 0, 13, 12, 13 }
                                };

const int8_t av1_fwd_cos_bit_row[MAX_TXWH_IDX /*txw_idx*/]
                                [MAX_TXWH_IDX /*txh_idx*/] = {
                                  { 13, 13, 12, 0, 0 },
                                  { 13, 13, 13, 12, 0 },
                                  { 13, 13, 12, 13, 12 },
                                  { 0, 12, 13, 12, 11 },
                                  { 0, 0, 12, 11, 10 }
                                };

static const int8_t fdct4_range_mult2[4] = { 0, 2, 3, 3 };
static const int8_t fdct8_range_mult2[6] = { 0, 2, 4, 5, 5, 5 };
static const int8_t fdct16_range_mult2[8] = { 0, 2, 4, 6, 7, 7, 7, 7 };
static const int8_t fdct32_range_mult2[10] = { 0, 2, 4, 6, 8, 9, 9, 9, 9, 9 };
static const int8_t fdct64_range_mult2[12] = { 0,  2,  4,  6,  8,  10,
                                               11, 11, 11, 11, 11, 11 };

static const int8_t fadst4_range_mult2[7] = { 0, 2, 4, 3, 3, 3, 3 };
static const int8_t fadst8_range_mult2[8] = { 0, 0, 1, 3, 3, 5, 5, 5 };
static const int8_t fadst16_range_mult2[10] = { 0, 0, 1, 3, 3, 5, 5, 7, 7, 7 };

static const int8_t fidtx4_range_mult2[1] = { 1 };
static const int8_t fidtx8_range_mult2[1] = { 2 };
static const int8_t fidtx16_range_mult2[1] = { 3 };
static const int8_t fidtx32_range_mult2[1] = { 4 };

#if 0
const int8_t fwd_idtx_range_row[MAX_TXWH_IDX /*txw_idx*/]
                               [MAX_TXWH_IDX /*txh_idx*/] = { { 2, 4, 5, 0, 0 },
                                                              { 3, 4, 5, 6, 0 },
                                                              { 4, 5, 6, 7, 8 },
                                                              { 0, 5, 6, 7, 8 },
                                                              { 0, 0, 7, 8,
                                                                9 } };
#endif

static const int8_t *fwd_txfm_range_mult2_list[TXFM_TYPES] = {
  fdct4_range_mult2,  fdct8_range_mult2,   fdct16_range_mult2,
  fdct32_range_mult2, fdct64_range_mult2,  fadst4_range_mult2,
  fadst8_range_mult2, fadst16_range_mult2, fidtx4_range_mult2,
  fidtx8_range_mult2, fidtx16_range_mult2, fidtx32_range_mult2
};

static INLINE void set_fwd_txfm_non_scale_range(TXFM_2D_FLIP_CFG *cfg) {
  av1_zero(cfg->stage_range_col);
  av1_zero(cfg->stage_range_row);

  const int8_t *range_mult2_col = fwd_txfm_range_mult2_list[cfg->txfm_type_col];
  if (cfg->txfm_type_col != TXFM_TYPE_INVALID) {
    int stage_num_col = cfg->stage_num_col;
    for (int i = 0; i < stage_num_col; ++i)
      cfg->stage_range_col[i] = (range_mult2_col[i] + 1) >> 1;
  }

  if (cfg->txfm_type_row != TXFM_TYPE_INVALID) {
    int stage_num_row = cfg->stage_num_row;
    const int8_t *range_mult2_row =
        fwd_txfm_range_mult2_list[cfg->txfm_type_row];
    for (int i = 0; i < stage_num_row; ++i) {
      cfg->stage_range_row[i] =
          (range_mult2_col[cfg->stage_num_col - 1] + range_mult2_row[i] + 1) >>
          1;
    }
  }
}

void av1_get_fwd_txfm_cfg(TX_TYPE tx_type, TX_SIZE tx_size,
                          TXFM_2D_FLIP_CFG *cfg) {
  assert(cfg != NULL);
  cfg->tx_size = tx_size;
  set_flip_cfg(tx_type, cfg);
  const TX_TYPE_1D tx_type_1d_col = vtx_tab[tx_type];
  const TX_TYPE_1D tx_type_1d_row = htx_tab[tx_type];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  cfg->shift = av1_fwd_txfm_shift_ls[tx_size];
  cfg->cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  cfg->cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  cfg->txfm_type_col = av1_txfm_type_ls[txh_idx][tx_type_1d_col];
  cfg->txfm_type_row = av1_txfm_type_ls[txw_idx][tx_type_1d_row];
  cfg->stage_num_col = av1_txfm_stage_num_list[cfg->txfm_type_col];
  cfg->stage_num_row = av1_txfm_stage_num_list[cfg->txfm_type_row];
  set_fwd_txfm_non_scale_range(cfg);
}
