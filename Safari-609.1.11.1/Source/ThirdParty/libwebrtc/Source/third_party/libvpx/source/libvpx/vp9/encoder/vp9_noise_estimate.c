/*
 *  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <limits.h>
#include <math.h>

#include "./vpx_dsp_rtcd.h"
#include "vpx_dsp/vpx_dsp_common.h"
#include "vpx_scale/yv12config.h"
#include "vpx/vpx_integer.h"
#include "vp9/common/vp9_reconinter.h"
#include "vp9/encoder/vp9_context_tree.h"
#include "vp9/encoder/vp9_noise_estimate.h"
#include "vp9/encoder/vp9_encoder.h"

#if CONFIG_VP9_TEMPORAL_DENOISING
// For SVC: only do noise estimation on top spatial layer.
static INLINE int noise_est_svc(const struct VP9_COMP *const cpi) {
  return (!cpi->use_svc ||
          (cpi->use_svc &&
           cpi->svc.spatial_layer_id == cpi->svc.number_spatial_layers - 1));
}
#endif

void vp9_noise_estimate_init(NOISE_ESTIMATE *const ne, int width, int height) {
  ne->enabled = 0;
  ne->level = kLowLow;
  ne->value = 0;
  ne->count = 0;
  ne->thresh = 90;
  ne->last_w = 0;
  ne->last_h = 0;
  if (width * height >= 1920 * 1080) {
    ne->thresh = 200;
  } else if (width * height >= 1280 * 720) {
    ne->thresh = 140;
  } else if (width * height >= 640 * 360) {
    ne->thresh = 115;
  }
  ne->num_frames_estimate = 15;
}

static int enable_noise_estimation(VP9_COMP *const cpi) {
#if CONFIG_VP9_HIGHBITDEPTH
  if (cpi->common.use_highbitdepth) return 0;
#endif
// Enable noise estimation if denoising is on.
#if CONFIG_VP9_TEMPORAL_DENOISING
  if (cpi->oxcf.noise_sensitivity > 0 && noise_est_svc(cpi) &&
      cpi->common.width >= 320 && cpi->common.height >= 180)
    return 1;
#endif
  // Only allow noise estimate under certain encoding mode.
  // Enabled for 1 pass CBR, speed >=5, and if resolution is same as original.
  // Not enabled for SVC mode and screen_content_mode.
  // Not enabled for low resolutions.
  if (cpi->oxcf.pass == 0 && cpi->oxcf.rc_mode == VPX_CBR &&
      cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ && cpi->oxcf.speed >= 5 &&
      cpi->resize_state == ORIG && cpi->resize_pending == 0 && !cpi->use_svc &&
      cpi->oxcf.content != VP9E_CONTENT_SCREEN &&
      cpi->common.width * cpi->common.height >= 640 * 360)
    return 1;
  else
    return 0;
}

#if CONFIG_VP9_TEMPORAL_DENOISING
static void copy_frame(YV12_BUFFER_CONFIG *const dest,
                       const YV12_BUFFER_CONFIG *const src) {
  int r;
  const uint8_t *srcbuf = src->y_buffer;
  uint8_t *destbuf = dest->y_buffer;

  assert(dest->y_width == src->y_width);
  assert(dest->y_height == src->y_height);

  for (r = 0; r < dest->y_height; ++r) {
    memcpy(destbuf, srcbuf, dest->y_width);
    destbuf += dest->y_stride;
    srcbuf += src->y_stride;
  }
}
#endif  // CONFIG_VP9_TEMPORAL_DENOISING

NOISE_LEVEL vp9_noise_estimate_extract_level(NOISE_ESTIMATE *const ne) {
  int noise_level = kLowLow;
  if (ne->value > (ne->thresh << 1)) {
    noise_level = kHigh;
  } else {
    if (ne->value > ne->thresh)
      noise_level = kMedium;
    else if (ne->value > ((9 * ne->thresh) >> 4))
      noise_level = kLow;
    else
      noise_level = kLowLow;
  }
  return noise_level;
}

void vp9_update_noise_estimate(VP9_COMP *const cpi) {
  const VP9_COMMON *const cm = &cpi->common;
  NOISE_ESTIMATE *const ne = &cpi->noise_estimate;
  const int low_res = (cm->width <= 352 && cm->height <= 288);
  // Estimate of noise level every frame_period frames.
  int frame_period = 8;
  int thresh_consec_zeromv = 6;
  unsigned int thresh_sum_diff = 100;
  unsigned int thresh_sum_spatial = (200 * 200) << 8;
  unsigned int thresh_spatial_var = (32 * 32) << 8;
  int min_blocks_estimate = cm->mi_rows * cm->mi_cols >> 7;
  int frame_counter = cm->current_video_frame;
  // Estimate is between current source and last source.
  YV12_BUFFER_CONFIG *last_source = cpi->Last_Source;
#if CONFIG_VP9_TEMPORAL_DENOISING
  if (cpi->oxcf.noise_sensitivity > 0 && noise_est_svc(cpi)) {
    last_source = &cpi->denoiser.last_source;
    // Tune these thresholds for different resolutions when denoising is
    // enabled.
    if (cm->width > 640 && cm->width < 1920) {
      thresh_consec_zeromv = 4;
      thresh_sum_diff = 200;
      thresh_sum_spatial = (120 * 120) << 8;
      thresh_spatial_var = (48 * 48) << 8;
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
#if CONFIG_VP9_TEMPORAL_DENOISING
    if (cpi->oxcf.noise_sensitivity > 0 && noise_est_svc(cpi))
      copy_frame(&cpi->denoiser.last_source, cpi->Source);
#endif
    if (last_source != NULL) {
      ne->last_w = cm->width;
      ne->last_h = cm->height;
    }
    return;
  } else if (cm->current_video_frame > 60 &&
             cpi->rc.avg_frame_low_motion < (low_res ? 70 : 50)) {
    // Force noise estimation to 0 and denoiser off if content has high motion.
    ne->level = kLowLow;
    ne->count = 0;
    ne->num_frames_estimate = 10;
#if CONFIG_VP9_TEMPORAL_DENOISING
    if (cpi->oxcf.noise_sensitivity > 0 && noise_est_svc(cpi) &&
        cpi->svc.current_superframe > 1) {
      vp9_denoiser_set_noise_level(&cpi->denoiser, ne->level);
      copy_frame(&cpi->denoiser.last_source, cpi->Source);
    }
#endif
    return;
  } else {
    int num_samples = 0;
    uint64_t avg_est = 0;
    int bsize = BLOCK_16X16;
    static const unsigned char const_source[16] = { 0, 0, 0, 0, 0, 0, 0, 0,
                                                    0, 0, 0, 0, 0, 0, 0, 0 };
    // Loop over sub-sample of 16x16 blocks of frame, and for blocks that have
    // been encoded as zero/small mv at least x consecutive frames, compute
    // the variance to update estimate of noise in the source.
    const uint8_t *src_y = cpi->Source->y_buffer;
    const int src_ystride = cpi->Source->y_stride;
    const uint8_t *last_src_y = last_source->y_buffer;
    const int last_src_ystride = last_source->y_stride;
    const uint8_t *src_u = cpi->Source->u_buffer;
    const uint8_t *src_v = cpi->Source->v_buffer;
    const int src_uvstride = cpi->Source->uv_stride;
    int mi_row, mi_col;
    int num_low_motion = 0;
    int frame_low_motion = 1;
    for (mi_row = 0; mi_row < cm->mi_rows; mi_row++) {
      for (mi_col = 0; mi_col < cm->mi_cols; mi_col++) {
        int bl_index = mi_row * cm->mi_cols + mi_col;
        if (cpi->consec_zero_mv[bl_index] > thresh_consec_zeromv)
          num_low_motion++;
      }
    }
    if (num_low_motion < ((3 * cm->mi_rows * cm->mi_cols) >> 3))
      frame_low_motion = 0;
    for (mi_row = 0; mi_row < cm->mi_rows; mi_row++) {
      for (mi_col = 0; mi_col < cm->mi_cols; mi_col++) {
        // 16x16 blocks, 1/4 sample of frame.
        if (mi_row % 4 == 0 && mi_col % 4 == 0 && mi_row < cm->mi_rows - 1 &&
            mi_col < cm->mi_cols - 1) {
          int bl_index = mi_row * cm->mi_cols + mi_col;
          int bl_index1 = bl_index + 1;
          int bl_index2 = bl_index + cm->mi_cols;
          int bl_index3 = bl_index2 + 1;
          int consec_zeromv =
              VPXMIN(cpi->consec_zero_mv[bl_index],
                     VPXMIN(cpi->consec_zero_mv[bl_index1],
                            VPXMIN(cpi->consec_zero_mv[bl_index2],
                                   cpi->consec_zero_mv[bl_index3])));
          // Only consider blocks that are likely steady background. i.e, have
          // been encoded as zero/low motion x (= thresh_consec_zeromv) frames
          // in a row. consec_zero_mv[] defined for 8x8 blocks, so consider all
          // 4 sub-blocks for 16x16 block. Also, avoid skin blocks.
          if (frame_low_motion && consec_zeromv > thresh_consec_zeromv) {
            int is_skin = 0;
            if (cpi->use_skin_detection) {
              is_skin =
                  vp9_compute_skin_block(src_y, src_u, src_v, src_ystride,
                                         src_uvstride, bsize, consec_zeromv, 0);
            }
            if (!is_skin) {
              unsigned int sse;
              // Compute variance.
              unsigned int variance = cpi->fn_ptr[bsize].vf(
                  src_y, src_ystride, last_src_y, last_src_ystride, &sse);
              // Only consider this block as valid for noise measurement if the
              // average term (sse - variance = N * avg^{2}, N = 16X16) of the
              // temporal residual is small (avoid effects from lighting
              // change).
              if ((sse - variance) < thresh_sum_diff) {
                unsigned int sse2;
                const unsigned int spatial_variance = cpi->fn_ptr[bsize].vf(
                    src_y, src_ystride, const_source, 0, &sse2);
                // Avoid blocks with high brightness and high spatial variance.
                if ((sse2 - spatial_variance) < thresh_sum_spatial &&
                    spatial_variance < thresh_spatial_var) {
                  avg_est += low_res ? variance >> 4
                                     : variance / ((spatial_variance >> 9) + 1);
                  num_samples++;
                }
              }
            }
          }
        }
        src_y += 8;
        last_src_y += 8;
        src_u += 4;
        src_v += 4;
      }
      src_y += (src_ystride << 3) - (cm->mi_cols << 3);
      last_src_y += (last_src_ystride << 3) - (cm->mi_cols << 3);
      src_u += (src_uvstride << 2) - (cm->mi_cols << 2);
      src_v += (src_uvstride << 2) - (cm->mi_cols << 2);
    }
    ne->last_w = cm->width;
    ne->last_h = cm->height;
    // Update noise estimate if we have at a minimum number of block samples,
    // and avg_est > 0 (avg_est == 0 can happen if the application inputs
    // duplicate frames).
    if (num_samples > min_blocks_estimate && avg_est > 0) {
      // Normalize.
      avg_est = avg_est / num_samples;
      // Update noise estimate.
      ne->value = (int)((3 * ne->value + avg_est) >> 2);
      ne->count++;
      if (ne->count == ne->num_frames_estimate) {
        // Reset counter and check noise level condition.
        ne->num_frames_estimate = 30;
        ne->count = 0;
        ne->level = vp9_noise_estimate_extract_level(ne);
#if CONFIG_VP9_TEMPORAL_DENOISING
        if (cpi->oxcf.noise_sensitivity > 0 && noise_est_svc(cpi))
          vp9_denoiser_set_noise_level(&cpi->denoiser, ne->level);
#endif
      }
    }
  }
#if CONFIG_VP9_TEMPORAL_DENOISING
  if (cpi->oxcf.noise_sensitivity > 0 && noise_est_svc(cpi))
    copy_frame(&cpi->denoiser.last_source, cpi->Source);
#endif
}
