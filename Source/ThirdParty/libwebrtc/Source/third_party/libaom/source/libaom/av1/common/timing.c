/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/common/timing.h"

/* Tables for AV1 max bitrates for different levels of main and high tier.
 * The tables are in Kbps instead of Mbps in the specification.
 * Note that depending on the profile, a multiplier is needed.
 */
#define UNDEFINED_RATE \
  (1 << 21)  // Placeholder rate for levels with undefined rate
#define INVALID_RATE \
  (0)  // For invalid profile-level configuration, set rate to 0

/* Max Bitrates for levels of Main Tier in kbps. Bitrate in main_kbps [31] */
/* is a dummy value. The decoder model is not applicable for level 31. */
static int32_t main_kbps[1 << LEVEL_BITS] = {
  1500,           3000,           UNDEFINED_RATE, UNDEFINED_RATE,
  6000,           10000,          UNDEFINED_RATE, UNDEFINED_RATE,
  12000,          20000,          UNDEFINED_RATE, UNDEFINED_RATE,
  30000,          40000,          60000,          60000,
  60000,          100000,         160000,         160000,
  UNDEFINED_RATE, UNDEFINED_RATE, UNDEFINED_RATE, UNDEFINED_RATE,
  UNDEFINED_RATE, UNDEFINED_RATE, UNDEFINED_RATE, UNDEFINED_RATE,
  UNDEFINED_RATE, UNDEFINED_RATE, UNDEFINED_RATE, UNDEFINED_RATE
};

/* Max Bitrates for levels of High Tier in kbps. Bitrate in high_kbps [31] */
/* is a dummy value. The decoder model is not applicable for level 31. */
static int32_t high_kbps[1 << LEVEL_BITS] = {
  INVALID_RATE,   INVALID_RATE,   INVALID_RATE,   INVALID_RATE,
  INVALID_RATE,   INVALID_RATE,   INVALID_RATE,   INVALID_RATE,
  30000,          50000,          UNDEFINED_RATE, UNDEFINED_RATE,
  100000,         160000,         240000,         240000,
  240000,         480000,         800000,         800000,
  UNDEFINED_RATE, UNDEFINED_RATE, UNDEFINED_RATE, UNDEFINED_RATE,
  UNDEFINED_RATE, UNDEFINED_RATE, UNDEFINED_RATE, UNDEFINED_RATE,
  UNDEFINED_RATE, UNDEFINED_RATE, UNDEFINED_RATE, UNDEFINED_RATE
};

/* BitrateProfileFactor */
static int bitrate_profile_factor[1 << PROFILE_BITS] = {
  1, 2, 3, 0, 0, 0, 0, 0
};

int64_t av1_max_level_bitrate(BITSTREAM_PROFILE seq_profile, int seq_level_idx,
                              int seq_tier) {
  int64_t bitrate;

  if (seq_tier) {
    bitrate = high_kbps[seq_level_idx] * bitrate_profile_factor[seq_profile];
  } else {
    bitrate = main_kbps[seq_level_idx] * bitrate_profile_factor[seq_profile];
  }

  return bitrate * 1000;
}

void av1_set_aom_dec_model_info(aom_dec_model_info_t *decoder_model) {
  decoder_model->encoder_decoder_buffer_delay_length = 16;
  decoder_model->buffer_removal_time_length = 10;
  decoder_model->frame_presentation_time_length = 10;
}

void av1_set_dec_model_op_parameters(aom_dec_model_op_parameters_t *op_params) {
  op_params->decoder_model_param_present_flag = 1;
  op_params->decoder_buffer_delay = 90000 >> 1;  //  0.5 s
  op_params->encoder_buffer_delay = 90000 >> 1;  //  0.5 s
  op_params->low_delay_mode_flag = 0;
  op_params->display_model_param_present_flag = 1;
  op_params->initial_display_delay = 8;  // 8 frames delay
}

void av1_set_resource_availability_parameters(
    aom_dec_model_op_parameters_t *op_params) {
  op_params->decoder_model_param_present_flag = 0;
  op_params->decoder_buffer_delay =
      70000;  // Resource availability mode default
  op_params->encoder_buffer_delay =
      20000;                           // Resource availability mode default
  op_params->low_delay_mode_flag = 0;  // Resource availability mode default
  op_params->display_model_param_present_flag = 1;
  op_params->initial_display_delay = 8;  // 8 frames delay
}
