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

#include "config/av1_rtcd.h"

#include "av1/common/enums.h"
#include "av1/common/av1_txfm.h"
#include "av1/common/x86/av1_txfm_sse2.h"
#include "av1/common/x86/highbd_txfm_utility_sse4.h"
#include "av1/encoder/av1_fwd_txfm1d_cfg.h"
#include "av1/encoder/x86/av1_txfm1d_sse4.h"
#include "av1/encoder/x86/av1_fwd_txfm_sse2.h"

static INLINE void int16_array_with_stride_to_int32_array_without_stride(
    const int16_t *input, int stride, int32_t *output, int txfm1d_size) {
  int r, c;
  for (r = 0; r < txfm1d_size; r++) {
    for (c = 0; c < txfm1d_size; c++) {
      output[r * txfm1d_size + c] = (int32_t)input[r * stride + c];
    }
  }
}

typedef void (*TxfmFuncSSE2)(__m128i *input, __m128i *output,
                             const int8_t cos_bit, const int8_t *stage_range);

static void fdct32_sse4_1(__m128i *input, __m128i *output, const int8_t cos_bit,
                          const int8_t *stage_range) {
  const int txfm_size = 32;
  const int num_per_128 = 4;
  int col_num = txfm_size / num_per_128;
  int col;
  (void)stage_range;
  for (col = 0; col < col_num; col++) {
    av1_fdct32_sse4_1((input + col), (output + col), cos_bit, col_num);
  }
}

static void fdct64_new_sse4_1(__m128i *input, __m128i *output,
                              const int8_t cos_bit, const int8_t *stage_range) {
  const int txfm_size = 64;
  const int num_per_128 = 4;
  int col_num = txfm_size / num_per_128;
  (void)stage_range;
  for (int col = 0; col < col_num; col++) {
    av1_fdct64_sse4_1((input + col), (output + col), cos_bit, col_num, col_num);
  }
}
static void idtx32x32_sse4_1(__m128i *input, __m128i *output,
                             const int8_t cos_bit, const int8_t *stage_range) {
  (void)stage_range;

  for (int i = 0; i < 8; i++) {
    av1_idtx32_sse4_1(&input[i * 32], &output[i * 32], cos_bit, 1);
  }
}

static INLINE TxfmFuncSSE2 fwd_txfm_type_to_func(TXFM_TYPE txfm_type) {
  switch (txfm_type) {
    case TXFM_TYPE_DCT32: return fdct32_sse4_1; break;
    case TXFM_TYPE_DCT64: return fdct64_new_sse4_1; break;
    case TXFM_TYPE_IDENTITY32: return idtx32x32_sse4_1; break;
    default: assert(0);
  }
  return NULL;
}

static INLINE void fwd_txfm2d_sse4_1(const int16_t *input, int32_t *output,
                                     const int stride,
                                     const TXFM_2D_FLIP_CFG *cfg,
                                     int32_t *txfm_buf) {
  // TODO(sarahparker) This does not currently support rectangular transforms
  // and will break without splitting txfm_size out into row and col size.
  // Rectangular transforms use c code only, so it should be ok for now.
  // It will be corrected when there are sse implementations for rectangular
  // transforms.
  assert(cfg->tx_size < TX_SIZES);
  const int txfm_size = tx_size_wide[cfg->tx_size];
  const int8_t *shift = cfg->shift;
  const int8_t *stage_range_col = cfg->stage_range_col;
  const int8_t *stage_range_row = cfg->stage_range_row;
  const int8_t cos_bit_col = cfg->cos_bit_col;
  const int8_t cos_bit_row = cfg->cos_bit_row;
  const TxfmFuncSSE2 txfm_func_col = fwd_txfm_type_to_func(cfg->txfm_type_col);
  const TxfmFuncSSE2 txfm_func_row = fwd_txfm_type_to_func(cfg->txfm_type_row);

  __m128i *buf_128 = (__m128i *)txfm_buf;
  __m128i *out_128 = (__m128i *)output;
  int num_per_128 = 4;
  int txfm2d_size_128 = txfm_size * txfm_size / num_per_128;

  int16_array_with_stride_to_int32_array_without_stride(input, stride, txfm_buf,
                                                        txfm_size);
  av1_round_shift_array_32_sse4_1(buf_128, out_128, txfm2d_size_128, -shift[0]);
  txfm_func_col(out_128, buf_128, cos_bit_col, stage_range_col);
  av1_round_shift_array_32_sse4_1(buf_128, out_128, txfm2d_size_128, -shift[1]);
  transpose_32(txfm_size, out_128, buf_128);
  txfm_func_row(buf_128, out_128, cos_bit_row, stage_range_row);
  av1_round_shift_array_32_sse4_1(out_128, buf_128, txfm2d_size_128, -shift[2]);
  transpose_32(txfm_size, buf_128, out_128);
}

static INLINE void fwd_txfm2d_64x64_sse4_1(const int16_t *input,
                                           int32_t *output, const int stride,
                                           const TXFM_2D_FLIP_CFG *cfg,
                                           int32_t *txfm_buf) {
  assert(cfg->tx_size < TX_SIZES);
  const int txfm_size = tx_size_wide[cfg->tx_size];
  const int8_t *shift = cfg->shift;
  const int8_t *stage_range_col = cfg->stage_range_col;
  const int8_t cos_bit_col = cfg->cos_bit_col;
  const int8_t cos_bit_row = cfg->cos_bit_row;
  const TxfmFuncSSE2 txfm_func_col = fwd_txfm_type_to_func(cfg->txfm_type_col);
  __m128i *buf_128 = (__m128i *)txfm_buf;
  __m128i *out_128 = (__m128i *)output;

  const int num_per_128 = 4;
  int txfm2d_size_128 = txfm_size * txfm_size / num_per_128;
  int col_num = txfm_size / num_per_128;

  int16_array_with_stride_to_int32_array_without_stride(input, stride, output,
                                                        txfm_size);
  /*col wise transform*/
  txfm_func_col(out_128, buf_128, cos_bit_col, stage_range_col);
  av1_round_shift_array_32_sse4_1(buf_128, out_128, txfm2d_size_128, -shift[1]);
  transpose_32(txfm_size, out_128, buf_128);

  /*row wise transform*/
  for (int col = 0; col < (col_num >> 1); col++) {
    av1_fdct64_sse4_1((buf_128 + col), (out_128 + col), cos_bit_row, col_num,
                      (col_num >> 1));
  }

  txfm2d_size_128 = (col_num >> 1) * (txfm_size >> 1);
  av1_round_shift_array_32_sse4_1(out_128, buf_128, txfm2d_size_128, -shift[2]);
  transpose_8nx8n(buf_128, out_128, 32, 32);
}

void av1_fwd_txfm2d_32x32_sse4_1(const int16_t *input, int32_t *output,
                                 int stride, TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(16, int32_t, txfm_buf[1024]);
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_32X32, &cfg);
  (void)bd;
  fwd_txfm2d_sse4_1(input, output, stride, &cfg, txfm_buf);
}

void av1_fwd_txfm2d_64x64_sse4_1(const int16_t *input, int32_t *output,
                                 int stride, TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(16, int32_t, txfm_buf[4096]);
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_64X64, &cfg);
  (void)bd;
  fwd_txfm2d_64x64_sse4_1(input, output, stride, &cfg, txfm_buf);
}

static INLINE void transpose_32_4x4x2(int stride, const __m128i *inputA,
                                      const __m128i *inputB, __m128i *output) {
  __m128i temp0 = _mm_unpacklo_epi32(inputA[0], inputA[2]);
  __m128i temp1 = _mm_unpackhi_epi32(inputA[0], inputA[2]);
  __m128i temp2 = _mm_unpacklo_epi32(inputA[1], inputA[3]);
  __m128i temp3 = _mm_unpackhi_epi32(inputA[1], inputA[3]);

  output[0 * stride] = _mm_unpacklo_epi32(temp0, temp2);
  output[1 * stride] = _mm_unpackhi_epi32(temp0, temp2);
  output[2 * stride] = _mm_unpacklo_epi32(temp1, temp3);
  output[3 * stride] = _mm_unpackhi_epi32(temp1, temp3);

  temp0 = _mm_unpacklo_epi32(inputB[0], inputB[2]);
  temp1 = _mm_unpackhi_epi32(inputB[0], inputB[2]);
  temp2 = _mm_unpacklo_epi32(inputB[1], inputB[3]);
  temp3 = _mm_unpackhi_epi32(inputB[1], inputB[3]);

  output[4 * stride] = _mm_unpacklo_epi32(temp0, temp2);
  output[5 * stride] = _mm_unpackhi_epi32(temp0, temp2);
  output[6 * stride] = _mm_unpacklo_epi32(temp1, temp3);
  output[7 * stride] = _mm_unpackhi_epi32(temp1, temp3);
}

static void lowbd_fwd_txfm2d_64x64_sse4_1(const int16_t *input, int32_t *output,
                                          int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  (void)tx_type;
  assert(tx_type == DCT_DCT);
  const TX_SIZE tx_size = TX_64X64;
  __m128i buf0[64], buf1[512];
  const int8_t *shift = av1_fwd_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  const transform_1d_sse2 col_txfm = av1_fdct8x64_new_sse2;
  const int width_div8 = (width >> 3);
  const int height_div8 = (height >> 3);

  for (int i = 0; i < width_div8; i++) {
    load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
    round_shift_16bit(buf0, height, shift[0]);
    col_txfm(buf0, buf0, cos_bit_col);
    round_shift_16bit(buf0, height, shift[1]);
    for (int j = 0; j < AOMMIN(4, height_div8); ++j) {
      transpose_16bit_8x8(buf0 + j * 8, buf1 + j * width + 8 * i);
    }
  }
  for (int i = 0; i < AOMMIN(4, height_div8); i++) {
    __m128i bufA[64];
    __m128i bufB[64];
    __m128i *buf = buf1 + width * i;
    for (int j = 0; j < width; ++j) {
      bufA[j] = _mm_cvtepi16_epi32(buf[j]);
      bufB[j] = _mm_cvtepi16_epi32(_mm_unpackhi_epi64(buf[j], buf[j]));
    }
    av1_fdct64_sse4_1(bufA, bufA, cos_bit_row, 1, 1);
    av1_fdct64_sse4_1(bufB, bufB, cos_bit_row, 1, 1);
    av1_round_shift_array_32_sse4_1(bufA, bufA, 32, -shift[2]);
    av1_round_shift_array_32_sse4_1(bufB, bufB, 32, -shift[2]);

    int32_t *output8 = output + 8 * 32 * i;
    for (int j = 0; j < width_div8; ++j) {
      __m128i *out = (__m128i *)(output8 + 4 * j);
      transpose_32_4x4x2(8, bufA + 4 * j, bufB + 4 * j, out);
    }
  }
}

static void lowbd_fwd_txfm2d_64x32_sse4_1(const int16_t *input, int32_t *output,
                                          int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  const TX_SIZE tx_size = TX_64X32;
  __m128i buf0[64], buf1[256];
  const int8_t *shift = av1_fwd_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  const transform_1d_sse2 col_txfm = col_txfm8x32_arr[tx_type];
  const int width_div8 = (width >> 3);
  const int height_div8 = (height >> 3);

  for (int i = 0; i < width_div8; i++) {
    load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
    round_shift_16bit(buf0, height, shift[0]);
    col_txfm(buf0, buf0, cos_bit_col);
    round_shift_16bit(buf0, height, shift[1]);
    for (int j = 0; j < AOMMIN(4, height_div8); ++j) {
      transpose_16bit_8x8(buf0 + j * 8, buf1 + j * width + 8 * i);
    }
  }
  assert(tx_type == DCT_DCT);
  for (int i = 0; i < AOMMIN(4, height_div8); i++) {
    __m128i bufA[64];
    __m128i bufB[64];
    __m128i *buf = buf1 + width * i;
    for (int j = 0; j < width; ++j) {
      bufA[j] = _mm_cvtepi16_epi32(buf[j]);
      bufB[j] = _mm_cvtepi16_epi32(_mm_unpackhi_epi64(buf[j], buf[j]));
    }
    av1_fdct64_sse4_1(bufA, bufA, cos_bit_row, 1, 1);
    av1_fdct64_sse4_1(bufB, bufB, cos_bit_row, 1, 1);
    av1_round_shift_rect_array_32_sse4_1(bufA, bufA, 32, -shift[2], NewSqrt2);
    av1_round_shift_rect_array_32_sse4_1(bufB, bufB, 32, -shift[2], NewSqrt2);

    int32_t *output8 = output + 8 * 32 * i;
    for (int j = 0; j < width_div8; ++j) {
      __m128i *out = (__m128i *)(output8 + 4 * j);
      transpose_32_4x4x2(8, bufA + 4 * j, bufB + 4 * j, out);
    }
  }
}

static void lowbd_fwd_txfm2d_32x64_sse4_1(const int16_t *input, int32_t *output,
                                          int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  (void)tx_type;
  assert(tx_type == DCT_DCT);
  const TX_SIZE tx_size = TX_32X64;
  __m128i buf0[64], buf1[256];
  const int8_t *shift = av1_fwd_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  const transform_1d_sse2 col_txfm = av1_fdct8x64_new_sse2;
  const int width_div8 = (width >> 3);
  const int height_div8 = (height >> 3);

  for (int i = 0; i < width_div8; i++) {
    load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
    round_shift_16bit(buf0, height, shift[0]);
    col_txfm(buf0, buf0, cos_bit_col);
    round_shift_16bit(buf0, height, shift[1]);
    for (int j = 0; j < AOMMIN(4, height_div8); ++j) {
      transpose_16bit_8x8(buf0 + j * 8, buf1 + j * width + 8 * i);
    }
  }

  for (int i = 0; i < AOMMIN(4, height_div8); i++) {
    __m128i bufA[32];
    __m128i bufB[32];
    __m128i *buf = buf1 + width * i;
    for (int j = 0; j < width; ++j) {
      bufA[j] = _mm_cvtepi16_epi32(buf[j]);
      bufB[j] = _mm_cvtepi16_epi32(_mm_unpackhi_epi64(buf[j], buf[j]));
    }
    av1_fdct32_sse4_1(bufA, bufA, cos_bit_row, 1);
    av1_fdct32_sse4_1(bufB, bufB, cos_bit_row, 1);
    av1_round_shift_rect_array_32_sse4_1(bufA, bufA, 32, -shift[2], NewSqrt2);
    av1_round_shift_rect_array_32_sse4_1(bufB, bufB, 32, -shift[2], NewSqrt2);

    int32_t *output8 = output + 8 * 32 * i;
    for (int j = 0; j < (32 / 4); ++j) {
      __m128i *out = (__m128i *)(output8 + 4 * j);
      transpose_32_4x4x2(8, bufA + 4 * j, bufB + 4 * j, out);
    }
  }
}

static FwdTxfm2dFunc fwd_txfm2d_func_ls[TX_SIZES_ALL] = {
  av1_lowbd_fwd_txfm2d_4x4_sse2,    // 4x4 transform
  av1_lowbd_fwd_txfm2d_8x8_sse2,    // 8x8 transform
  av1_lowbd_fwd_txfm2d_16x16_sse2,  // 16x16 transform
  av1_lowbd_fwd_txfm2d_32x32_sse2,  // 32x32 transform
  lowbd_fwd_txfm2d_64x64_sse4_1,    // 64x64 transform
  av1_lowbd_fwd_txfm2d_4x8_sse2,    // 4x8 transform
  av1_lowbd_fwd_txfm2d_8x4_sse2,    // 8x4 transform
  av1_lowbd_fwd_txfm2d_8x16_sse2,   // 8x16 transform
  av1_lowbd_fwd_txfm2d_16x8_sse2,   // 16x8 transform
  av1_lowbd_fwd_txfm2d_16x32_sse2,  // 16x32 transform
  av1_lowbd_fwd_txfm2d_32x16_sse2,  // 32x16 transform
  lowbd_fwd_txfm2d_32x64_sse4_1,    // 32x64 transform
  lowbd_fwd_txfm2d_64x32_sse4_1,    // 64x32 transform
  av1_lowbd_fwd_txfm2d_4x16_sse2,   // 4x16 transform
  av1_lowbd_fwd_txfm2d_16x4_sse2,   // 16x4 transform
  av1_lowbd_fwd_txfm2d_8x32_sse2,   // 8x32 transform
  av1_lowbd_fwd_txfm2d_32x8_sse2,   // 32x8 transform
  av1_lowbd_fwd_txfm2d_16x64_sse2,  // 16x64 transform
  av1_lowbd_fwd_txfm2d_64x16_sse2,  // 64x16 transform
};

void av1_lowbd_fwd_txfm_sse4_1(const int16_t *src_diff, tran_low_t *coeff,
                               int diff_stride, TxfmParam *txfm_param) {
  FwdTxfm2dFunc fwd_txfm2d_func = fwd_txfm2d_func_ls[txfm_param->tx_size];
  if (txfm_param->lossless && txfm_param->tx_size == TX_4X4) {
    av1_lowbd_fwd_txfm_c(src_diff, coeff, diff_stride, txfm_param);
  } else {
    fwd_txfm2d_func(src_diff, coeff, diff_stride, txfm_param->tx_type,
                    txfm_param->bd);
  }
}
