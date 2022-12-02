/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_AV1_CX_IFACE_H_
#define AOM_AV1_AV1_CX_IFACE_H_
#include "av1/encoder/encoder.h"
#include "aom/aom_encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

AV1EncoderConfig av1_get_encoder_config(const aom_codec_enc_cfg_t *cfg);

aom_codec_err_t av1_create_stats_buffer(FIRSTPASS_STATS **frame_stats_buffer,
                                        STATS_BUFFER_CTX *stats_buf_context,
                                        int num_lap_buffers);

void av1_destroy_stats_buffer(STATS_BUFFER_CTX *stats_buf_context,
                              FIRSTPASS_STATS *frame_stats_buffer);

aom_codec_err_t av1_create_context_and_bufferpool(AV1_PRIMARY *ppi,
                                                  AV1_COMP **p_cpi,
                                                  BufferPool **p_buffer_pool,
                                                  const AV1EncoderConfig *oxcf,
                                                  COMPRESSOR_STAGE stage,
                                                  int lap_lag_in_frames);

void av1_destroy_context_and_bufferpool(AV1_COMP *cpi,
                                        BufferPool **p_buffer_pool);

int av1_get_image_bps(const aom_image_t *img);

void av1_reduce_ratio(aom_rational64_t *ratio);

int av1_gcd(int64_t a, int b);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_AV1_CX_IFACE_H_
