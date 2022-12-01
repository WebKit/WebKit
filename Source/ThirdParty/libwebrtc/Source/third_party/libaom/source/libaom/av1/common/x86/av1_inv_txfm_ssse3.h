/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#ifndef AOM_AV1_COMMON_X86_AV1_INV_TXFM_SSSE3_H_
#define AOM_AV1_COMMON_X86_AV1_INV_TXFM_SSSE3_H_

#include <emmintrin.h>  // SSE2
#include <tmmintrin.h>  // SSSE3

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/x86/transpose_sse2.h"
#include "aom_dsp/x86/txfm_common_sse2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define btf_16_ssse3(w0, w1, in, out0, out1)    \
  do {                                          \
    const __m128i _w0 = _mm_set1_epi16(w0 * 8); \
    const __m128i _w1 = _mm_set1_epi16(w1 * 8); \
    const __m128i _in = in;                     \
    out0 = _mm_mulhrs_epi16(_in, _w0);          \
    out1 = _mm_mulhrs_epi16(_in, _w1);          \
  } while (0)

#define btf_16_adds_subs_sse2(in0, in1) \
  do {                                  \
    const __m128i _in0 = in0;           \
    const __m128i _in1 = in1;           \
    in0 = _mm_adds_epi16(_in0, _in1);   \
    in1 = _mm_subs_epi16(_in0, _in1);   \
  } while (0)

#define btf_16_subs_adds_sse2(in0, in1) \
  do {                                  \
    const __m128i _in0 = in0;           \
    const __m128i _in1 = in1;           \
    in1 = _mm_subs_epi16(_in0, _in1);   \
    in0 = _mm_adds_epi16(_in0, _in1);   \
  } while (0)

#define btf_16_adds_subs_out_sse2(out0, out1, in0, in1) \
  do {                                                  \
    const __m128i _in0 = in0;                           \
    const __m128i _in1 = in1;                           \
    out0 = _mm_adds_epi16(_in0, _in1);                  \
    out1 = _mm_subs_epi16(_in0, _in1);                  \
  } while (0)

static INLINE void round_shift_16bit_ssse3(__m128i *in, int size, int bit) {
  if (bit < 0) {
    const __m128i scale = _mm_set1_epi16(1 << (15 + bit));
    for (int i = 0; i < size; ++i) {
      in[i] = _mm_mulhrs_epi16(in[i], scale);
    }
  } else if (bit > 0) {
    for (int i = 0; i < size; ++i) {
      in[i] = _mm_slli_epi16(in[i], bit);
    }
  }
}

// 1D itx types
enum {
  IDCT_1D,
  IADST_1D,
  IFLIPADST_1D = IADST_1D,
  IIDENTITY_1D,
  ITX_TYPES_1D,
} UENUM1BYTE(ITX_TYPE_1D);

static const ITX_TYPE_1D vitx_1d_tab[TX_TYPES] = {
  IDCT_1D,      IADST_1D,     IDCT_1D,      IADST_1D,
  IFLIPADST_1D, IDCT_1D,      IFLIPADST_1D, IADST_1D,
  IFLIPADST_1D, IIDENTITY_1D, IDCT_1D,      IIDENTITY_1D,
  IADST_1D,     IIDENTITY_1D, IFLIPADST_1D, IIDENTITY_1D,
};

static const ITX_TYPE_1D hitx_1d_tab[TX_TYPES] = {
  IDCT_1D,      IDCT_1D,      IADST_1D,     IADST_1D,
  IDCT_1D,      IFLIPADST_1D, IFLIPADST_1D, IFLIPADST_1D,
  IADST_1D,     IIDENTITY_1D, IIDENTITY_1D, IDCT_1D,
  IIDENTITY_1D, IADST_1D,     IIDENTITY_1D, IFLIPADST_1D,
};

DECLARE_ALIGNED(16, static const int16_t, av1_eob_to_eobxy_8x8_default[8]) = {
  0x0707, 0x0707, 0x0707, 0x0707, 0x0707, 0x0707, 0x0707, 0x0707,
};

DECLARE_ALIGNED(16, static const int16_t,
                av1_eob_to_eobxy_16x16_default[16]) = {
  0x0707, 0x0707, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f,
  0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f,
};

DECLARE_ALIGNED(16, static const int16_t,
                av1_eob_to_eobxy_32x32_default[32]) = {
  0x0707, 0x0f0f, 0x0f0f, 0x0f0f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f,
  0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f,
  0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f,
  0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f,
};

DECLARE_ALIGNED(16, static const int16_t, av1_eob_to_eobxy_8x16_default[16]) = {
  0x0707, 0x0707, 0x0707, 0x0707, 0x0707, 0x0f07, 0x0f07, 0x0f07,
  0x0f07, 0x0f07, 0x0f07, 0x0f07, 0x0f07, 0x0f07, 0x0f07, 0x0f07,
};

DECLARE_ALIGNED(16, static const int16_t, av1_eob_to_eobxy_16x8_default[8]) = {
  0x0707, 0x0707, 0x070f, 0x070f, 0x070f, 0x070f, 0x070f, 0x070f,
};

DECLARE_ALIGNED(16, static const int16_t,
                av1_eob_to_eobxy_16x32_default[32]) = {
  0x0707, 0x0707, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f,
  0x0f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f,
  0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f,
  0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f,
};

DECLARE_ALIGNED(16, static const int16_t,
                av1_eob_to_eobxy_32x16_default[16]) = {
  0x0707, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f1f, 0x0f1f, 0x0f1f, 0x0f1f,
  0x0f1f, 0x0f1f, 0x0f1f, 0x0f1f, 0x0f1f, 0x0f1f, 0x0f1f, 0x0f1f,
};

DECLARE_ALIGNED(16, static const int16_t, av1_eob_to_eobxy_8x32_default[32]) = {
  0x0707, 0x0707, 0x0707, 0x0707, 0x0707, 0x0f07, 0x0f07, 0x0f07,
  0x0f07, 0x0f07, 0x0f07, 0x0f07, 0x0f07, 0x1f07, 0x1f07, 0x1f07,
  0x1f07, 0x1f07, 0x1f07, 0x1f07, 0x1f07, 0x1f07, 0x1f07, 0x1f07,
  0x1f07, 0x1f07, 0x1f07, 0x1f07, 0x1f07, 0x1f07, 0x1f07, 0x1f07,
};

DECLARE_ALIGNED(16, static const int16_t, av1_eob_to_eobxy_32x8_default[8]) = {
  0x0707, 0x070f, 0x070f, 0x071f, 0x071f, 0x071f, 0x071f, 0x071f,
};

DECLARE_ALIGNED(16, static const int16_t *,
                av1_eob_to_eobxy_default[TX_SIZES_ALL]) = {
  NULL,
  av1_eob_to_eobxy_8x8_default,
  av1_eob_to_eobxy_16x16_default,
  av1_eob_to_eobxy_32x32_default,
  av1_eob_to_eobxy_32x32_default,
  NULL,
  NULL,
  av1_eob_to_eobxy_8x16_default,
  av1_eob_to_eobxy_16x8_default,
  av1_eob_to_eobxy_16x32_default,
  av1_eob_to_eobxy_32x16_default,
  av1_eob_to_eobxy_32x32_default,
  av1_eob_to_eobxy_32x32_default,
  NULL,
  NULL,
  av1_eob_to_eobxy_8x32_default,
  av1_eob_to_eobxy_32x8_default,
  av1_eob_to_eobxy_16x32_default,
  av1_eob_to_eobxy_32x16_default,
};

static const int lowbd_txfm_all_1d_zeros_idx[32] = {
  0, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
};

// Transform block width in log2 for eob (size of 64 map to 32)
static const int tx_size_wide_log2_eob[TX_SIZES_ALL] = {
  2, 3, 4, 5, 5, 2, 3, 3, 4, 4, 5, 5, 5, 2, 4, 3, 5, 4, 5,
};

static INLINE void get_eobx_eoby_scan_default(int *eobx, int *eoby,
                                              TX_SIZE tx_size, int eob) {
  if (eob == 1) {
    *eobx = 0;
    *eoby = 0;
    return;
  }

  const int tx_w_log2 = tx_size_wide_log2_eob[tx_size];
  const int eob_row = (eob - 1) >> tx_w_log2;
  const int eobxy = av1_eob_to_eobxy_default[tx_size][eob_row];
  *eobx = eobxy & 0xFF;
  *eoby = eobxy >> 8;
}

static int eob_fill[32] = {
  0,  7,  7,  7,  7,  7,  7,  7,  15, 15, 15, 15, 15, 15, 15, 15,
  31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
};

static INLINE void get_eobx_eoby_scan_h_identity(int *eobx, int *eoby,
                                                 TX_SIZE tx_size, int eob) {
  eob -= 1;
  const int txfm_size_col = tx_size_wide[tx_size];
  const int eobx_max = AOMMIN(32, txfm_size_col) - 1;
  *eobx = (eob >= eobx_max) ? eobx_max : eob_fill[eob];
  const int temp_eoby = eob / (eobx_max + 1);
  assert(temp_eoby < 32);
  *eoby = eob_fill[temp_eoby];
}

static INLINE void get_eobx_eoby_scan_v_identity(int *eobx, int *eoby,
                                                 TX_SIZE tx_size, int eob) {
  eob -= 1;
  const int txfm_size_row = tx_size_high[tx_size];
  const int eoby_max = AOMMIN(32, txfm_size_row) - 1;
  *eobx = eob / (eoby_max + 1);
  *eoby = (eob >= eoby_max) ? eoby_max : eob_fill[eob];
}

typedef void (*transform_1d_ssse3)(const __m128i *input, __m128i *output);

void av1_lowbd_inv_txfm2d_add_ssse3(const int32_t *input, uint8_t *output,
                                    int stride, TX_TYPE tx_type,
                                    TX_SIZE tx_size, int eob);
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_COMMON_X86_AV1_INV_TXFM_SSSE3_H_
