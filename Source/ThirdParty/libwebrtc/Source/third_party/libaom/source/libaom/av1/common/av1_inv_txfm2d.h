/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_COMMON_AV1_INV_TXFM2D_H_
#define AOM_AV1_COMMON_AV1_INV_TXFM2D_H_

#include "config/aom_config.h"

#include "aom_dsp/txfm_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#if HAVE_SSSE3
void av1_lowbd_inv_txfm2d_add_ssse3(const int32_t *input, uint8_t *output,
                                    int stride, TX_TYPE tx_type,
                                    TX_SIZE tx_size, int eob);
#endif

#if HAVE_AVX2
void av1_lowbd_inv_txfm2d_add_avx2(const int32_t *input, uint8_t *output,
                                   int stride, TX_TYPE tx_type, TX_SIZE tx_size,
                                   int eob);
#endif

#if HAVE_NEON
// This function is used by av1_inv_txfm2d_test.cc.
void av1_lowbd_inv_txfm2d_add_neon(const int32_t *input, uint8_t *output,
                                   int stride, TX_TYPE tx_type, TX_SIZE tx_size,
                                   int eob);
#endif

#ifdef __cplusplus
}
#endif

#endif  // AOM_AV1_COMMON_AV1_INV_TXFM2D_H_
