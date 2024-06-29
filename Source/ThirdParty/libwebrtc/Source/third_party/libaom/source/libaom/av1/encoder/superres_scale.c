/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/encoder/encoder_alloc.h"
#include "av1/encoder/superres_scale.h"
#include "av1/encoder/random.h"

// Compute the horizontal frequency components' energy in a frame
// by calculuating the 16x4 Horizontal DCT. This is to be used to
// decide the superresolution parameters.
static void analyze_hor_freq(const AV1_COMP *cpi, double *energy) {
  uint64_t freq_energy[16] = { 0 };
  const YV12_BUFFER_CONFIG *buf = cpi->source;
  const int bd = cpi->td.mb.e_mbd.bd;
  const int width = buf->y_crop_width;
  const int height = buf->y_crop_height;
  DECLARE_ALIGNED(16, int32_t, coeff[16 * 4]);
  int n = 0;
  memset(freq_energy, 0, sizeof(freq_energy));
  if (buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    const int16_t *src16 = (const int16_t *)CONVERT_TO_SHORTPTR(buf->y_buffer);
    for (int i = 0; i < height - 4; i += 4) {
      for (int j = 0; j < width - 16; j += 16) {
        av1_fwd_txfm2d_16x4(src16 + i * buf->y_stride + j, coeff, buf->y_stride,
                            H_DCT, bd);
        for (int k = 1; k < 16; ++k) {
          const uint64_t this_energy =
              ((int64_t)coeff[k] * coeff[k]) +
              ((int64_t)coeff[k + 16] * coeff[k + 16]) +
              ((int64_t)coeff[k + 32] * coeff[k + 32]) +
              ((int64_t)coeff[k + 48] * coeff[k + 48]);
          freq_energy[k] += ROUND_POWER_OF_TWO(this_energy, 2 + 2 * (bd - 8));
        }
        n++;
      }
    }
  } else {
    assert(bd == 8);
    DECLARE_ALIGNED(16, int16_t, src16[16 * 4]);
    for (int i = 0; i < height - 4; i += 4) {
      for (int j = 0; j < width - 16; j += 16) {
        for (int ii = 0; ii < 4; ++ii)
          for (int jj = 0; jj < 16; ++jj)
            src16[ii * 16 + jj] =
                buf->y_buffer[(i + ii) * buf->y_stride + (j + jj)];
        av1_fwd_txfm2d_16x4(src16, coeff, 16, H_DCT, bd);
        for (int k = 1; k < 16; ++k) {
          const uint64_t this_energy =
              ((int64_t)coeff[k] * coeff[k]) +
              ((int64_t)coeff[k + 16] * coeff[k + 16]) +
              ((int64_t)coeff[k + 32] * coeff[k + 32]) +
              ((int64_t)coeff[k + 48] * coeff[k + 48]);
          freq_energy[k] += ROUND_POWER_OF_TWO(this_energy, 2);
        }
        n++;
      }
    }
  }
  if (n) {
    for (int k = 1; k < 16; ++k) energy[k] = (double)freq_energy[k] / n;
    // Convert to cumulative energy
    for (int k = 14; k > 0; --k) energy[k] += energy[k + 1];
  } else {
    for (int k = 1; k < 16; ++k) energy[k] = 1e+20;
  }
}

static uint8_t calculate_next_resize_scale(const AV1_COMP *cpi) {
  // Choose an arbitrary random number
  static unsigned int seed = 56789;
  const ResizeCfg *resize_cfg = &cpi->oxcf.resize_cfg;
  if (is_stat_generation_stage(cpi)) return SCALE_NUMERATOR;
  uint8_t new_denom = SCALE_NUMERATOR;

  if (cpi->common.seq_params->reduced_still_picture_hdr) return SCALE_NUMERATOR;
  switch (resize_cfg->resize_mode) {
    case RESIZE_NONE: new_denom = SCALE_NUMERATOR; break;
    case RESIZE_FIXED:
      if (cpi->common.current_frame.frame_type == KEY_FRAME)
        new_denom = resize_cfg->resize_kf_scale_denominator;
      else
        new_denom = resize_cfg->resize_scale_denominator;
      break;
    case RESIZE_RANDOM: new_denom = lcg_rand16(&seed) % 9 + 8; break;
    default: assert(0);
  }
  return new_denom;
}

int av1_superres_in_recode_allowed(const AV1_COMP *const cpi) {
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  // Empirically found to not be beneficial for image coding.
  return oxcf->superres_cfg.superres_mode == AOM_SUPERRES_AUTO &&
         cpi->sf.hl_sf.superres_auto_search_type != SUPERRES_AUTO_SOLO &&
         cpi->rc.frames_to_key > 1;
}

#define SUPERRES_ENERGY_BY_Q2_THRESH_KEYFRAME_SOLO 0.012
#define SUPERRES_ENERGY_BY_Q2_THRESH_KEYFRAME 0.008
#define SUPERRES_ENERGY_BY_Q2_THRESH_ARFFRAME 0.008
#define SUPERRES_ENERGY_BY_AC_THRESH 0.2

static double get_energy_by_q2_thresh(const GF_GROUP *gf_group,
                                      const RATE_CONTROL *rc,
                                      int gf_frame_index) {
  // TODO(now): Return keyframe thresh * factor based on frame type / pyramid
  // level.
  if (gf_group->update_type[gf_frame_index] == ARF_UPDATE) {
    return SUPERRES_ENERGY_BY_Q2_THRESH_ARFFRAME;
  } else if (gf_group->update_type[gf_frame_index] == KF_UPDATE) {
    if (rc->frames_to_key <= 1)
      return SUPERRES_ENERGY_BY_Q2_THRESH_KEYFRAME_SOLO;
    else
      return SUPERRES_ENERGY_BY_Q2_THRESH_KEYFRAME;
  } else {
    assert(0);
  }
  return 0;
}

static uint8_t get_superres_denom_from_qindex_energy(int qindex, double *energy,
                                                     double threshq,
                                                     double threshp) {
  const double q = av1_convert_qindex_to_q(qindex, AOM_BITS_8);
  const double tq = threshq * q * q;
  const double tp = threshp * energy[1];
  const double thresh = AOMMIN(tq, tp);
  int k;
  for (k = SCALE_NUMERATOR * 2; k > SCALE_NUMERATOR; --k) {
    if (energy[k - 1] > thresh) break;
  }
  return 3 * SCALE_NUMERATOR - k;
}

static uint8_t get_superres_denom_for_qindex(const AV1_COMP *cpi, int qindex,
                                             int sr_kf, int sr_arf) {
  // Use superres for Key-frames and Alt-ref frames only.
  const GF_GROUP *gf_group = &cpi->ppi->gf_group;
  if (gf_group->update_type[cpi->gf_frame_index] != KF_UPDATE &&
      gf_group->update_type[cpi->gf_frame_index] != ARF_UPDATE) {
    return SCALE_NUMERATOR;
  }
  if (gf_group->update_type[cpi->gf_frame_index] == KF_UPDATE && !sr_kf) {
    return SCALE_NUMERATOR;
  }
  if (gf_group->update_type[cpi->gf_frame_index] == ARF_UPDATE && !sr_arf) {
    return SCALE_NUMERATOR;
  }

  double energy[16];
  analyze_hor_freq(cpi, energy);

  const double energy_by_q2_thresh =
      get_energy_by_q2_thresh(gf_group, &cpi->rc, cpi->gf_frame_index);
  int denom = get_superres_denom_from_qindex_energy(
      qindex, energy, energy_by_q2_thresh, SUPERRES_ENERGY_BY_AC_THRESH);
  /*
  printf("\nenergy = [");
  for (int k = 1; k < 16; ++k) printf("%f, ", energy[k]);
  printf("]\n");
  printf("boost = %d\n",
         (gf_group->update_type[cpi->gf_frame_index] == KF_UPDATE)
             ? cpi->ppi->p_rc.kf_boost
             : cpi->rc.gfu_boost);
  printf("denom = %d\n", denom);
  */
  if (av1_superres_in_recode_allowed(cpi)) {
    assert(cpi->superres_mode != AOM_SUPERRES_NONE);
    // Force superres to be tried in the recode loop, as full-res is also going
    // to be tried anyway.
    denom = AOMMAX(denom, SCALE_NUMERATOR + 1);
  }
  return denom;
}

static uint8_t calculate_next_superres_scale(AV1_COMP *cpi) {
  // Choose an arbitrary random number
  static unsigned int seed = 34567;
  const AV1EncoderConfig *oxcf = &cpi->oxcf;
  const SuperResCfg *const superres_cfg = &oxcf->superres_cfg;
  const FrameDimensionCfg *const frm_dim_cfg = &oxcf->frm_dim_cfg;
  const RateControlCfg *const rc_cfg = &oxcf->rc_cfg;

  if (is_stat_generation_stage(cpi)) return SCALE_NUMERATOR;
  uint8_t new_denom = SCALE_NUMERATOR;

  // Make sure that superres mode of the frame is consistent with the
  // sequence-level flag.
  assert(IMPLIES(superres_cfg->superres_mode != AOM_SUPERRES_NONE,
                 cpi->common.seq_params->enable_superres));
  assert(IMPLIES(!cpi->common.seq_params->enable_superres,
                 superres_cfg->superres_mode == AOM_SUPERRES_NONE));
  // Make sure that superres mode for current encoding is consistent with user
  // provided superres mode.
  assert(IMPLIES(superres_cfg->superres_mode != AOM_SUPERRES_AUTO,
                 cpi->superres_mode == superres_cfg->superres_mode));

  // Note: we must look at the current superres_mode to be tried in 'cpi' here,
  // not the user given mode in 'oxcf'.
  switch (cpi->superres_mode) {
    case AOM_SUPERRES_NONE: new_denom = SCALE_NUMERATOR; break;
    case AOM_SUPERRES_FIXED:
      if (cpi->common.current_frame.frame_type == KEY_FRAME)
        new_denom = superres_cfg->superres_kf_scale_denominator;
      else
        new_denom = superres_cfg->superres_scale_denominator;
      break;
    case AOM_SUPERRES_RANDOM: new_denom = lcg_rand16(&seed) % 9 + 8; break;
    case AOM_SUPERRES_QTHRESH: {
      // Do not use superres when screen content tools are used.
      if (cpi->common.features.allow_screen_content_tools) break;
      if (rc_cfg->mode == AOM_VBR || rc_cfg->mode == AOM_CQ)
        av1_set_target_rate(cpi, frm_dim_cfg->width, frm_dim_cfg->height);

      // Now decide the use of superres based on 'q'.
      int bottom_index, top_index;
      const int q = av1_rc_pick_q_and_bounds(
          cpi, frm_dim_cfg->width, frm_dim_cfg->height, cpi->gf_frame_index,
          &bottom_index, &top_index);

      const int qthresh = (frame_is_intra_only(&cpi->common))
                              ? superres_cfg->superres_kf_qthresh
                              : superres_cfg->superres_qthresh;
      if (q <= qthresh) {
        new_denom = SCALE_NUMERATOR;
      } else {
        new_denom = get_superres_denom_for_qindex(cpi, q, 1, 1);
      }
      break;
    }
    case AOM_SUPERRES_AUTO: {
      if (cpi->common.features.allow_screen_content_tools) break;
      if (rc_cfg->mode == AOM_VBR || rc_cfg->mode == AOM_CQ)
        av1_set_target_rate(cpi, frm_dim_cfg->width, frm_dim_cfg->height);

      // Now decide the use of superres based on 'q'.
      int bottom_index, top_index;
      const int q = av1_rc_pick_q_and_bounds(
          cpi, frm_dim_cfg->width, frm_dim_cfg->height, cpi->gf_frame_index,
          &bottom_index, &top_index);

      const SUPERRES_AUTO_SEARCH_TYPE sr_search_type =
          cpi->sf.hl_sf.superres_auto_search_type;
      const int qthresh = (sr_search_type == SUPERRES_AUTO_SOLO) ? 128 : 0;
      if (q <= qthresh) {
        new_denom = SCALE_NUMERATOR;  // Don't use superres.
      } else {
        if (sr_search_type == SUPERRES_AUTO_ALL) {
          if (cpi->common.current_frame.frame_type == KEY_FRAME)
            new_denom = superres_cfg->superres_kf_scale_denominator;
          else
            new_denom = superres_cfg->superres_scale_denominator;
        } else {
          new_denom = get_superres_denom_for_qindex(cpi, q, 1, 1);
        }
      }
      break;
    }
    default: assert(0);
  }
  return new_denom;
}

static int dimension_is_ok(int orig_dim, int resized_dim, int denom) {
  return (resized_dim * SCALE_NUMERATOR >= orig_dim * denom / 2);
}

static int dimensions_are_ok(int owidth, int oheight, size_params_type *rsz) {
  // Only need to check the width, as scaling is horizontal only.
  (void)oheight;
  return dimension_is_ok(owidth, rsz->resize_width, rsz->superres_denom);
}

static int validate_size_scales(RESIZE_MODE resize_mode,
                                aom_superres_mode superres_mode, int owidth,
                                int oheight, size_params_type *rsz) {
  if (dimensions_are_ok(owidth, oheight, rsz)) {  // Nothing to do.
    return 1;
  }

  // Calculate current resize scale.
  int resize_denom =
      AOMMAX(DIVIDE_AND_ROUND(owidth * SCALE_NUMERATOR, rsz->resize_width),
             DIVIDE_AND_ROUND(oheight * SCALE_NUMERATOR, rsz->resize_height));

  if (resize_mode != RESIZE_RANDOM && superres_mode == AOM_SUPERRES_RANDOM) {
    // Alter superres scale as needed to enforce conformity.
    rsz->superres_denom =
        (2 * SCALE_NUMERATOR * SCALE_NUMERATOR) / resize_denom;
    if (!dimensions_are_ok(owidth, oheight, rsz)) {
      if (rsz->superres_denom > SCALE_NUMERATOR) --rsz->superres_denom;
    }
  } else if (resize_mode == RESIZE_RANDOM &&
             superres_mode != AOM_SUPERRES_RANDOM) {
    // Alter resize scale as needed to enforce conformity.
    resize_denom =
        (2 * SCALE_NUMERATOR * SCALE_NUMERATOR) / rsz->superres_denom;
    rsz->resize_width = owidth;
    rsz->resize_height = oheight;
    av1_calculate_scaled_size(&rsz->resize_width, &rsz->resize_height,
                              resize_denom);
    if (!dimensions_are_ok(owidth, oheight, rsz)) {
      if (resize_denom > SCALE_NUMERATOR) {
        --resize_denom;
        rsz->resize_width = owidth;
        rsz->resize_height = oheight;
        av1_calculate_scaled_size(&rsz->resize_width, &rsz->resize_height,
                                  resize_denom);
      }
    }
  } else if (resize_mode == RESIZE_RANDOM &&
             superres_mode == AOM_SUPERRES_RANDOM) {
    // Alter both resize and superres scales as needed to enforce conformity.
    do {
      if (resize_denom > rsz->superres_denom)
        --resize_denom;
      else
        --rsz->superres_denom;
      rsz->resize_width = owidth;
      rsz->resize_height = oheight;
      av1_calculate_scaled_size(&rsz->resize_width, &rsz->resize_height,
                                resize_denom);
    } while (!dimensions_are_ok(owidth, oheight, rsz) &&
             (resize_denom > SCALE_NUMERATOR ||
              rsz->superres_denom > SCALE_NUMERATOR));
  } else {  // We are allowed to alter neither resize scale nor superres
            // scale.
    return 0;
  }
  return dimensions_are_ok(owidth, oheight, rsz);
}

// Calculates resize and superres params for next frame
static size_params_type calculate_next_size_params(AV1_COMP *cpi) {
  const AV1EncoderConfig *oxcf = &cpi->oxcf;
  ResizePendingParams *resize_pending_params = &cpi->resize_pending_params;
  const FrameDimensionCfg *const frm_dim_cfg = &oxcf->frm_dim_cfg;
  size_params_type rsz = { frm_dim_cfg->width, frm_dim_cfg->height,
                           SCALE_NUMERATOR };
  int resize_denom = SCALE_NUMERATOR;
  if (has_no_stats_stage(cpi) && cpi->ppi->use_svc &&
      (cpi->common.width != cpi->oxcf.frm_dim_cfg.width ||
       cpi->common.height != cpi->oxcf.frm_dim_cfg.height)) {
    rsz.resize_width = cpi->common.width;
    rsz.resize_height = cpi->common.height;
    return rsz;
  }
  if (is_stat_generation_stage(cpi)) return rsz;
  if (resize_pending_params->width && resize_pending_params->height) {
    rsz.resize_width = resize_pending_params->width;
    rsz.resize_height = resize_pending_params->height;
    resize_pending_params->width = resize_pending_params->height = 0;
    if (oxcf->superres_cfg.superres_mode == AOM_SUPERRES_NONE) return rsz;
  } else {
    resize_denom = calculate_next_resize_scale(cpi);
    rsz.resize_width = frm_dim_cfg->width;
    rsz.resize_height = frm_dim_cfg->height;
    av1_calculate_scaled_size(&rsz.resize_width, &rsz.resize_height,
                              resize_denom);
  }
  rsz.superres_denom = calculate_next_superres_scale(cpi);
  if (!validate_size_scales(oxcf->resize_cfg.resize_mode, cpi->superres_mode,
                            frm_dim_cfg->width, frm_dim_cfg->height, &rsz))
    assert(0 && "Invalid scale parameters");
  return rsz;
}

static void setup_frame_size_from_params(AV1_COMP *cpi,
                                         const size_params_type *rsz) {
  int encode_width = rsz->resize_width;
  int encode_height = rsz->resize_height;

  AV1_COMMON *cm = &cpi->common;
  cm->superres_upscaled_width = encode_width;
  cm->superres_upscaled_height = encode_height;
  cm->superres_scale_denominator = rsz->superres_denom;
  av1_calculate_scaled_superres_size(&encode_width, &encode_height,
                                     rsz->superres_denom);
  av1_set_frame_size(cpi, encode_width, encode_height);
}

void av1_setup_frame_size(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  // Reset superres params from previous frame.
  cm->superres_scale_denominator = SCALE_NUMERATOR;
  const size_params_type rsz = calculate_next_size_params(cpi);
  setup_frame_size_from_params(cpi, &rsz);

  assert(av1_is_min_tile_width_satisfied(cm));
}

void av1_superres_post_encode(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;

  assert(cpi->oxcf.superres_cfg.enable_superres);
  assert(!is_lossless_requested(&cpi->oxcf.rc_cfg));
  assert(!cm->features.all_lossless);

  av1_superres_upscale(cm, NULL, cpi->alloc_pyramid);

  // If regular resizing is occurring the source will need to be downscaled to
  // match the upscaled superres resolution. Otherwise the original source is
  // used.
  if (!av1_resize_scaled(cm)) {
    cpi->source = cpi->unscaled_source;
    if (cpi->last_source != NULL) cpi->last_source = cpi->unscaled_last_source;
  } else {
    assert(cpi->unscaled_source->y_crop_width != cm->superres_upscaled_width);
    assert(cpi->unscaled_source->y_crop_height != cm->superres_upscaled_height);
    // Do downscale. cm->(width|height) has been updated by
    // av1_superres_upscale
    cpi->source = realloc_and_scale_source(cpi, cm->superres_upscaled_width,
                                           cm->superres_upscaled_height);
  }
}
