/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <limits.h>
#include <math.h>

#include "config/aom_dsp_rtcd.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_scale/yv12config.h"
#include "aom/aom_integer.h"
#include "av1/encoder/context_tree.h"
#include "av1/encoder/av1_noise_estimate.h"
#include "av1/encoder/encoder.h"
#if CONFIG_AV1_TEMPORAL_DENOISING
#include "av1/encoder/av1_temporal_denoiser.h"
#endif

#if CONFIG_AV1_TEMPORAL_DENOISING
// For SVC: only do noise estimation on top spatial layer.
static inline int noise_est_svc(const struct AV1_COMP *const cpi) {
  return (!cpi->ppi->use_svc ||
          (cpi->ppi->use_svc &&
           cpi->svc.spatial_layer_id == cpi->svc.number_spatial_layers - 1));
}
#endif

void av1_noise_estimate_init(NOISE_ESTIMATE *const ne, int width, int height) {
  const int64_t area = (int64_t)width * height;
  ne->enabled = 0;
  ne->level = (area < 1280 * 720) ? kLowLow : kLow;
  ne->value = 0;
  ne->count = 0;
  ne->thresh = 90;
  ne->last_w = 0;
  ne->last_h = 0;
  if (area >= 1920 * 1080) {
    ne->thresh = 200;
  } else if (area >= 1280 * 720) {
    ne->thresh = 140;
  } else if (area >= 640 * 360) {
    ne->thresh = 115;
  }
  ne->num_frames_estimate = 15;
  ne->adapt_thresh = (3 * ne->thresh) >> 1;
}

static int enable_noise_estimation(AV1_COMP *const cpi) {
  const int resize_pending = is_frame_resize_pending(cpi);

#if CONFIG_AV1_HIGHBITDEPTH
  if (cpi->common.seq_params->use_highbitdepth) return 0;
#endif
// Enable noise estimation if denoising is on.
#if CONFIG_AV1_TEMPORAL_DENOISING
  if (cpi->oxcf.noise_sensitivity > 0 && noise_est_svc(cpi) &&
      cpi->common.width >= 320 && cpi->common.height >= 180)
    return 1;
#endif
  // Only allow noise estimate under certain encoding mode.
  // Enabled for 1 pass CBR, speed >=5, and if resolution is same as original.
  // Not enabled for SVC mode and screen_content_mode.
  // Not enabled for low resolutions.
  if (cpi->oxcf.pass == AOM_RC_ONE_PASS && cpi->oxcf.rc_cfg.mode == AOM_CBR &&
      cpi->oxcf.q_cfg.aq_mode == CYCLIC_REFRESH_AQ && cpi->oxcf.speed >= 5 &&
      resize_pending == 0 && !cpi->ppi->use_svc &&
      cpi->oxcf.tune_cfg.content != AOM_CONTENT_SCREEN &&
      cpi->common.width * cpi->common.height >= 640 * 360)
    return 1;
  else
    return 0;
}

#if CONFIG_AV1_TEMPORAL_DENOISING
static void copy_frame(YV12_BUFFER_CONFIG *const dest,
                       const YV12_BUFFER_CONFIG *const src) {
  const uint8_t *srcbuf = src->y_buffer;
  uint8_t *destbuf = dest->y_buffer;

  assert(dest->y_width == src->y_width);
  assert(dest->y_height == src->y_height);

  for (int r = 0; r < dest->y_height; ++r) {
    memcpy(destbuf, srcbuf, dest->y_width);
    destbuf += dest->y_stride;
    srcbuf += src->y_stride;
  }
}
#endif  // CONFIG_AV1_TEMPORAL_DENOISING

NOISE_LEVEL av1_noise_estimate_extract_level(NOISE_ESTIMATE *const ne) {
  int noise_level = kLowLow;
  if (ne->value > (ne->thresh << 1)) {
    noise_level = kHigh;
  } else {
    if (ne->value > ne->thresh)
      noise_level = kMedium;
    else if (ne->value > (ne->thresh >> 1))
      noise_level = kLow;
    else
      noise_level = kLowLow;
  }
  return noise_level;
}

void av1_update_noise_estimate(AV1_COMP *const cpi) {
  const AV1_COMMON *const cm = &cpi->common;
  const CommonModeInfoParams *const mi_params = &cm->mi_params;

  NOISE_ESTIMATE *const ne = &cpi->noise_estimate;
  const int low_res = (cm->width <= 352 && cm->height <= 288);
  // Estimate of noise level every frame_period frames.
  int frame_period = 8;
  int thresh_consec_zeromv = 2;
  int frame_counter = cm->current_frame.frame_number;
  // Estimate is between current source and last source.
  YV12_BUFFER_CONFIG *last_source = cpi->last_source;
#if CONFIG_AV1_TEMPORAL_DENOISING
  if (cpi->oxcf.noise_sensitivity > 0 && noise_est_svc(cpi)) {
    last_source = &cpi->denoiser.last_source;
    // Tune these thresholds for different resolutions when denoising is
    // enabled.
    if (cm->width > 640 && cm->width <= 1920) {
      thresh_consec_zeromv = 2;
    }
  }
#endif
  ne->enabled = enable_noise_estimation(cpi);
  if (cpi->svc.number_spatial_layers > 1)
    frame_counter = cpi->svc.current_superframe;
  if (!ne->enabled || frame_counter % frame_period != 0 ||
      last_source == NULL ||
      (cpi->svc.number_spatial_layers == 1 &&
       (ne->last_w != cm->width || ne->last_h != cm->height))) {
#if CONFIG_AV1_TEMPORAL_DENOISING
    if (cpi->oxcf.noise_sensitivity > 0 && noise_est_svc(cpi))
      copy_frame(&cpi->denoiser.last_source, cpi->source);
#endif
    if (last_source != NULL) {
      ne->last_w = cm->width;
      ne->last_h = cm->height;
    }
    return;
  } else if (frame_counter > 60 && cpi->svc.num_encoded_top_layer > 1 &&
             cpi->rc.frames_since_key > cpi->svc.number_spatial_layers &&
             cpi->svc.spatial_layer_id == cpi->svc.number_spatial_layers - 1 &&
             cpi->rc.avg_frame_low_motion < (low_res ? 60 : 40)) {
    // Force noise estimation to 0 and denoiser off if content has high motion.
    ne->level = kLowLow;
    ne->count = 0;
    ne->num_frames_estimate = 10;
#if CONFIG_AV1_TEMPORAL_DENOISING
    if (cpi->oxcf.noise_sensitivity > 0 && noise_est_svc(cpi) &&
        cpi->svc.current_superframe > 1) {
      av1_denoiser_set_noise_level(cpi, ne->level);
      copy_frame(&cpi->denoiser.last_source, cpi->source);
    }
#endif
    return;
  } else {
    unsigned int bin_size = 100;
    unsigned int hist[MAX_VAR_HIST_BINS] = { 0 };
    unsigned int hist_avg[MAX_VAR_HIST_BINS];
    unsigned int max_bin = 0;
    unsigned int max_bin_count = 0;
    unsigned int bin_cnt;
    BLOCK_SIZE bsize = BLOCK_16X16;
    // Loop over sub-sample of 16x16 blocks of frame, and for blocks that have
    // been encoded as zero/small mv at least x consecutive frames, compute
    // the variance to update estimate of noise in the source.
    const uint8_t *src_y = cpi->source->y_buffer;
    const int src_ystride = cpi->source->y_stride;
    const uint8_t *last_src_y = last_source->y_buffer;
    const int last_src_ystride = last_source->y_stride;
    int mi_row, mi_col;
    int num_low_motion = 0;
    int frame_low_motion = 1;
    for (mi_row = 0; mi_row < mi_params->mi_rows; mi_row += 2) {
      for (mi_col = 0; mi_col < mi_params->mi_cols; mi_col += 2) {
        int bl_index =
            (mi_row >> 1) * (mi_params->mi_cols >> 1) + (mi_col >> 1);
        if (cpi->consec_zero_mv[bl_index] > thresh_consec_zeromv)
          num_low_motion++;
      }
    }
    if (num_low_motion <
        (((3 * (mi_params->mi_rows * mi_params->mi_cols) >> 2)) >> 3))
      frame_low_motion = 0;
    for (mi_row = 0; mi_row < mi_params->mi_rows; mi_row++) {
      for (mi_col = 0; mi_col < mi_params->mi_cols; mi_col++) {
        // 16x16 blocks, 1/4 sample of frame.
        if (mi_row % 8 == 0 && mi_col % 8 == 0 &&
            mi_row < mi_params->mi_rows - 3 &&
            mi_col < mi_params->mi_cols - 3) {
          int bl_index =
              (mi_row >> 1) * (mi_params->mi_cols >> 1) + (mi_col >> 1);
          int bl_index1 = bl_index + 1;
          int bl_index2 = bl_index + (mi_params->mi_cols >> 1);
          int bl_index3 = bl_index2 + 1;
          int consec_zeromv =
              AOMMIN(cpi->consec_zero_mv[bl_index],
                     AOMMIN(cpi->consec_zero_mv[bl_index1],
                            AOMMIN(cpi->consec_zero_mv[bl_index2],
                                   cpi->consec_zero_mv[bl_index3])));
          // Only consider blocks that are likely steady background. i.e, have
          // been encoded as zero/low motion x (= thresh_consec_zeromv) frames
          // in a row. consec_zero_mv[] defined for 8x8 blocks, so consider all
          // 4 sub-blocks for 16x16 block. And exclude this frame if
          // high_source_sad is true (i.e., scene/content change).
          if (frame_low_motion && consec_zeromv > thresh_consec_zeromv &&
              !cpi->rc.high_source_sad) {
            unsigned int sse;
            // Compute variance between co-located blocks from current and
            // last input frames.
            unsigned int variance = cpi->ppi->fn_ptr[bsize].vf(
                src_y, src_ystride, last_src_y, last_src_ystride, &sse);
            unsigned int hist_index = variance / bin_size;
            if (hist_index < MAX_VAR_HIST_BINS)
              hist[hist_index]++;
            else if (hist_index < 3 * (MAX_VAR_HIST_BINS >> 1))
              hist[MAX_VAR_HIST_BINS - 1]++;  // Account for the tail
          }
        }
        src_y += 4;
        last_src_y += 4;
      }
      src_y += (src_ystride << 2) - (mi_params->mi_cols << 2);
      last_src_y += (last_src_ystride << 2) - (mi_params->mi_cols << 2);
    }
    ne->last_w = cm->width;
    ne->last_h = cm->height;
    // Adjust histogram to account for effect that histogram flattens
    // and shifts to zero as scene darkens.
    if (hist[0] > 10 && (hist[MAX_VAR_HIST_BINS - 1] > hist[0] >> 2)) {
      hist[0] = 0;
      hist[1] >>= 2;
      hist[2] >>= 2;
      hist[3] >>= 2;
      hist[4] >>= 1;
      hist[5] >>= 1;
      hist[6] = 3 * hist[6] >> 1;
      hist[MAX_VAR_HIST_BINS - 1] >>= 1;
    }

    // Average hist[] and find largest bin
    for (bin_cnt = 0; bin_cnt < MAX_VAR_HIST_BINS; bin_cnt++) {
      if (bin_cnt == 0)
        hist_avg[bin_cnt] = (hist[0] + hist[1] + hist[2]) / 3;
      else if (bin_cnt == MAX_VAR_HIST_BINS - 1)
        hist_avg[bin_cnt] = hist[MAX_VAR_HIST_BINS - 1] >> 2;
      else if (bin_cnt == MAX_VAR_HIST_BINS - 2)
        hist_avg[bin_cnt] = (hist[bin_cnt - 1] + 2 * hist[bin_cnt] +
                             (hist[bin_cnt + 1] >> 1) + 2) >>
                            2;
      else
        hist_avg[bin_cnt] =
            (hist[bin_cnt - 1] + 2 * hist[bin_cnt] + hist[bin_cnt + 1] + 2) >>
            2;

      if (hist_avg[bin_cnt] > max_bin_count) {
        max_bin_count = hist_avg[bin_cnt];
        max_bin = bin_cnt;
      }
    }
    // Scale by 40 to work with existing thresholds
    ne->value = (int)((3 * ne->value + max_bin * 40) >> 2);
    // Quickly increase VNR strength when the noise level increases suddenly.
    if (ne->level < kMedium && ne->value > ne->adapt_thresh) {
      ne->count = ne->num_frames_estimate;
    } else {
      ne->count++;
    }
    if (ne->count == ne->num_frames_estimate) {
      // Reset counter and check noise level condition.
      ne->num_frames_estimate = 30;
      ne->count = 0;
      ne->level = av1_noise_estimate_extract_level(ne);
#if CONFIG_AV1_TEMPORAL_DENOISING
      if (cpi->oxcf.noise_sensitivity > 0 && noise_est_svc(cpi))
        av1_denoiser_set_noise_level(cpi, ne->level);
#endif
    }
  }
#if CONFIG_AV1_TEMPORAL_DENOISING
  if (cpi->oxcf.noise_sensitivity > 0 && noise_est_svc(cpi))
    copy_frame(&cpi->denoiser.last_source, cpi->source);
#endif
}
