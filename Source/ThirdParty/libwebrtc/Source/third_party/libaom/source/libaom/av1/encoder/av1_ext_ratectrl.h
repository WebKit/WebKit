/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_ENCODER_AV1_EXT_RATECTRL_H_
#define AOM_AV1_ENCODER_AV1_EXT_RATECTRL_H_

#include "aom/aom_codec.h"
#include "aom/aom_ext_ratectrl.h"
#include "aom/aom_tpl.h"
#include "av1/encoder/firstpass.h"

typedef struct AOM_EXT_RATECTRL {
  int ready;
  int ext_rdmult;
  aom_rc_model_t model;
  aom_rc_funcs_t funcs;
  aom_rc_config_t ratectrl_config;
  aom_rc_firstpass_stats_t rc_firstpass_stats;
} AOM_EXT_RATECTRL;

aom_codec_err_t av1_extrc_init(AOM_EXT_RATECTRL *ext_ratectrl);

aom_codec_err_t av1_extrc_create(aom_rc_funcs_t funcs,
                                 aom_rc_config_t ratectrl_config,
                                 AOM_EXT_RATECTRL *ext_ratectrl);

aom_codec_err_t av1_extrc_delete(AOM_EXT_RATECTRL *ext_ratectrl);

aom_codec_err_t av1_extrc_send_firstpass_stats(
    AOM_EXT_RATECTRL *ext_ratectrl, const FIRSTPASS_INFO *first_pass_info);

aom_codec_err_t av1_extrc_send_tpl_stats(AOM_EXT_RATECTRL *ext_ratectrl,
                                         const AomTplGopStats *tpl_gop_stats);

aom_codec_err_t av1_extrc_get_encodeframe_decision(
    AOM_EXT_RATECTRL *ext_ratectrl, int gop_index,
    aom_rc_encodeframe_decision_t *encode_frame_decision);

aom_codec_err_t av1_extrc_update_encodeframe_result(
    AOM_EXT_RATECTRL *ext_ratectrl, int64_t bit_count,
    int actual_encoding_qindex);

aom_codec_err_t av1_extrc_get_key_frame_decision(
    AOM_EXT_RATECTRL *ext_ratectrl,
    aom_rc_key_frame_decision_t *key_frame_decision);

aom_codec_err_t av1_extrc_get_gop_decision(AOM_EXT_RATECTRL *ext_ratectrl,
                                           aom_rc_gop_decision_t *gop_decision);

aom_codec_err_t av1_extrc_get_frame_rdmult(
    AOM_EXT_RATECTRL *ext_ratectrl, int show_index, int coding_index,
    int gop_index, FRAME_UPDATE_TYPE update_type, int gop_size, int use_alt_ref,
    RefCntBuffer *ref_frame_bufs[AOM_RC_MAX_REF_FRAMES], int ref_frame_flags,
    int *rdmult);

#endif  // AOM_AV1_ENCODER_AV1_EXT_RATECTRL_H_
