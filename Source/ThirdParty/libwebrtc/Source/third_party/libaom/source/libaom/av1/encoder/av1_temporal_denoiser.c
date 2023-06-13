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

#include <assert.h>
#include <limits.h>
#include <math.h>

#include "config/aom_dsp_rtcd.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_scale/yv12config.h"
#include "aom/aom_integer.h"
#include "av1/common/reconinter.h"
#include "av1/encoder/reconinter_enc.h"
#include "av1/encoder/context_tree.h"
#include "av1/encoder/av1_temporal_denoiser.h"
#include "av1/encoder/encoder.h"

#ifdef OUTPUT_YUV_DENOISED
static void make_grayscale(YV12_BUFFER_CONFIG *yuv);
#endif

static int absdiff_thresh(BLOCK_SIZE bs, int increase_denoising) {
  (void)bs;
  return 3 + (increase_denoising ? 1 : 0);
}

static int delta_thresh(BLOCK_SIZE bs, int increase_denoising) {
  (void)bs;
  (void)increase_denoising;
  return 4;
}

static int noise_motion_thresh(BLOCK_SIZE bs, int increase_denoising) {
  (void)bs;
  (void)increase_denoising;
  return 625;
}

static unsigned int sse_thresh(BLOCK_SIZE bs, int increase_denoising) {
  return (1 << num_pels_log2_lookup[bs]) * (increase_denoising ? 80 : 40);
}

static int sse_diff_thresh(BLOCK_SIZE bs, int increase_denoising,
                           int motion_magnitude) {
  if (motion_magnitude > noise_motion_thresh(bs, increase_denoising)) {
    if (increase_denoising)
      return (1 << num_pels_log2_lookup[bs]) << 2;
    else
      return 0;
  } else {
    return (1 << num_pels_log2_lookup[bs]) << 4;
  }
}

static int total_adj_weak_thresh(BLOCK_SIZE bs, int increase_denoising) {
  return (1 << num_pels_log2_lookup[bs]) * (increase_denoising ? 3 : 2);
}

// TODO(kyslov): If increase_denoising is enabled in the future,
// we might need to update the code for calculating 'total_adj' in
// case the C code is not bit-exact with corresponding sse2 code.
int av1_denoiser_filter_c(const uint8_t *sig, int sig_stride,
                          const uint8_t *mc_avg, int mc_avg_stride,
                          uint8_t *avg, int avg_stride, int increase_denoising,
                          BLOCK_SIZE bs, int motion_magnitude) {
  int r, c;
  const uint8_t *sig_start = sig;
  const uint8_t *mc_avg_start = mc_avg;
  uint8_t *avg_start = avg;
  int diff, adj, absdiff, delta;
  int adj_val[] = { 3, 4, 6 };
  int total_adj = 0;
  int shift_inc = 1;

  // If motion_magnitude is small, making the denoiser more aggressive by
  // increasing the adjustment for each level. Add another increment for
  // blocks that are labeled for increase denoising.
  if (motion_magnitude <= MOTION_MAGNITUDE_THRESHOLD) {
    if (increase_denoising) {
      shift_inc = 2;
    }
    adj_val[0] += shift_inc;
    adj_val[1] += shift_inc;
    adj_val[2] += shift_inc;
  }

  // First attempt to apply a strong temporal denoising filter.
  for (r = 0; r < block_size_high[bs]; ++r) {
    for (c = 0; c < block_size_wide[bs]; ++c) {
      diff = mc_avg[c] - sig[c];
      absdiff = abs(diff);

      if (absdiff <= absdiff_thresh(bs, increase_denoising)) {
        avg[c] = mc_avg[c];
        total_adj += diff;
      } else {
        switch (absdiff) {
          case 4:
          case 5:
          case 6:
          case 7: adj = adj_val[0]; break;
          case 8:
          case 9:
          case 10:
          case 11:
          case 12:
          case 13:
          case 14:
          case 15: adj = adj_val[1]; break;
          default: adj = adj_val[2];
        }
        if (diff > 0) {
          avg[c] = AOMMIN(UINT8_MAX, sig[c] + adj);
          total_adj += adj;
        } else {
          avg[c] = AOMMAX(0, sig[c] - adj);
          total_adj -= adj;
        }
      }
    }
    sig += sig_stride;
    avg += avg_stride;
    mc_avg += mc_avg_stride;
  }

  // If the strong filter did not modify the signal too much, we're all set.
  if (abs(total_adj) <= total_adj_strong_thresh(bs, increase_denoising)) {
    return FILTER_BLOCK;
  }

  // Otherwise, we try to dampen the filter if the delta is not too high.
  delta = ((abs(total_adj) - total_adj_strong_thresh(bs, increase_denoising)) >>
           num_pels_log2_lookup[bs]) +
          1;

  if (delta >= delta_thresh(bs, increase_denoising)) {
    return COPY_BLOCK;
  }

  mc_avg = mc_avg_start;
  avg = avg_start;
  sig = sig_start;
  for (r = 0; r < block_size_high[bs]; ++r) {
    for (c = 0; c < block_size_wide[bs]; ++c) {
      diff = mc_avg[c] - sig[c];
      adj = abs(diff);
      if (adj > delta) {
        adj = delta;
      }
      if (diff > 0) {
        // Diff positive means we made positive adjustment above
        // (in first try/attempt), so now make negative adjustment to bring
        // denoised signal down.
        avg[c] = AOMMAX(0, avg[c] - adj);
        total_adj -= adj;
      } else {
        // Diff negative means we made negative adjustment above
        // (in first try/attempt), so now make positive adjustment to bring
        // denoised signal up.
        avg[c] = AOMMIN(UINT8_MAX, avg[c] + adj);
        total_adj += adj;
      }
    }
    sig += sig_stride;
    avg += avg_stride;
    mc_avg += mc_avg_stride;
  }

  // We can use the filter if it has been sufficiently dampened
  if (abs(total_adj) <= total_adj_weak_thresh(bs, increase_denoising)) {
    return FILTER_BLOCK;
  }
  return COPY_BLOCK;
}

static uint8_t *block_start(uint8_t *framebuf, int stride, int mi_row,
                            int mi_col) {
  return framebuf + (stride * mi_row << 2) + (mi_col << 2);
}

static AV1_DENOISER_DECISION perform_motion_compensation(
    AV1_COMMON *const cm, AV1_DENOISER *denoiser, MACROBLOCK *mb, BLOCK_SIZE bs,
    int increase_denoising, int mi_row, int mi_col, PICK_MODE_CONTEXT *ctx,
    int motion_magnitude, int *zeromv_filter, int num_spatial_layers, int width,
    int lst_fb_idx, int gld_fb_idx, int use_svc, int spatial_layer,
    int use_gf_temporal_ref) {
  const int sse_diff = (ctx->newmv_sse == UINT_MAX)
                           ? 0
                           : ((int)ctx->zeromv_sse - (int)ctx->newmv_sse);
  int frame;
  int denoise_layer_idx = 0;
  MACROBLOCKD *filter_mbd = &mb->e_mbd;
  MB_MODE_INFO *mi = filter_mbd->mi[0];
  MB_MODE_INFO saved_mi;
  int i;
  struct buf_2d saved_dst[MAX_MB_PLANE];
  struct buf_2d saved_pre[MAX_MB_PLANE];
  // const RefBuffer *saved_block_refs[2];
  MV_REFERENCE_FRAME saved_frame;

  frame = ctx->best_reference_frame;

  saved_mi = *mi;

  // Avoid denoising small blocks. When noise > kDenLow or frame width > 480,
  // denoise 16x16 blocks.
  if (bs == BLOCK_8X8 || bs == BLOCK_8X16 || bs == BLOCK_16X8 ||
      (bs == BLOCK_16X16 && width > 480 &&
       denoiser->denoising_level <= kDenLow))
    return COPY_BLOCK;

  // If the best reference frame uses inter-prediction and there is enough of a
  // difference in sum-squared-error, use it.
  if (frame != INTRA_FRAME && frame != ALTREF_FRAME && frame != GOLDEN_FRAME &&
      sse_diff > sse_diff_thresh(bs, increase_denoising, motion_magnitude)) {
    mi->ref_frame[0] = ctx->best_reference_frame;
    mi->mode = ctx->best_sse_inter_mode;
    mi->mv[0] = ctx->best_sse_mv;
  } else {
    // Otherwise, use the zero reference frame.
    frame = ctx->best_zeromv_reference_frame;
    ctx->newmv_sse = ctx->zeromv_sse;
    // Bias to last reference.
    if ((num_spatial_layers > 1 && !use_gf_temporal_ref) ||
        frame == ALTREF_FRAME ||
        (frame == GOLDEN_FRAME && use_gf_temporal_ref) ||
        (frame != LAST_FRAME &&
         ((ctx->zeromv_lastref_sse < (5 * ctx->zeromv_sse) >> 2) ||
          denoiser->denoising_level >= kDenHigh))) {
      frame = LAST_FRAME;
      ctx->newmv_sse = ctx->zeromv_lastref_sse;
    }
    mi->ref_frame[0] = frame;
    mi->mode = GLOBALMV;
    mi->mv[0].as_int = 0;
    ctx->best_sse_inter_mode = GLOBALMV;
    ctx->best_sse_mv.as_int = 0;
    *zeromv_filter = 1;
    if (denoiser->denoising_level > kDenMedium) {
      motion_magnitude = 0;
    }
  }

  saved_frame = frame;
  // When using SVC, we need to map REF_FRAME to the frame buffer index.
  if (use_svc) {
    if (frame == LAST_FRAME)
      frame = lst_fb_idx + 1;
    else if (frame == GOLDEN_FRAME)
      frame = gld_fb_idx + 1;
    // Shift for the second spatial layer.
    if (num_spatial_layers - spatial_layer == 2)
      frame = frame + denoiser->num_ref_frames;
    denoise_layer_idx = num_spatial_layers - spatial_layer - 1;
  }

  // Force copy (no denoise, copy source in denoised buffer) if
  // running_avg_y[frame] is NULL.
  if (denoiser->running_avg_y[frame].buffer_alloc == NULL) {
    // Restore everything to its original state
    *mi = saved_mi;
    return COPY_BLOCK;
  }

  if (ctx->newmv_sse > sse_thresh(bs, increase_denoising)) {
    // Restore everything to its original state
    *mi = saved_mi;
    return COPY_BLOCK;
  }
  if (motion_magnitude > (noise_motion_thresh(bs, increase_denoising) << 3)) {
    // Restore everything to its original state
    *mi = saved_mi;
    return COPY_BLOCK;
  }

  // We will restore these after motion compensation.
  for (i = 0; i < MAX_MB_PLANE; ++i) {
    saved_pre[i] = filter_mbd->plane[i].pre[0];
    saved_dst[i] = filter_mbd->plane[i].dst;
  }

  // Set the pointers in the MACROBLOCKD to point to the buffers in the denoiser
  // struct.
  set_ref_ptrs(cm, filter_mbd, saved_frame, NONE);
  av1_setup_pre_planes(filter_mbd, 0, &(denoiser->running_avg_y[frame]), mi_row,
                       mi_col, filter_mbd->block_ref_scale_factors[0], 1);
  av1_setup_dst_planes(filter_mbd->plane, bs,
                       &(denoiser->mc_running_avg_y[denoise_layer_idx]), mi_row,
                       mi_col, 0, 1);

  av1_enc_build_inter_predictor_y(filter_mbd, mi_row, mi_col);

  // Restore everything to its original state
  *mi = saved_mi;
  for (i = 0; i < MAX_MB_PLANE; ++i) {
    filter_mbd->plane[i].pre[0] = saved_pre[i];
    filter_mbd->plane[i].dst = saved_dst[i];
  }

  return FILTER_BLOCK;
}

void av1_denoiser_denoise(AV1_COMP *cpi, MACROBLOCK *mb, int mi_row, int mi_col,
                          BLOCK_SIZE bs, PICK_MODE_CONTEXT *ctx,
                          AV1_DENOISER_DECISION *denoiser_decision,
                          int use_gf_temporal_ref) {
  int mv_col, mv_row;
  int motion_magnitude = 0;
  int zeromv_filter = 0;
  AV1_DENOISER *denoiser = &cpi->denoiser;
  AV1_DENOISER_DECISION decision = COPY_BLOCK;

  const int shift =
      cpi->svc.number_spatial_layers - cpi->svc.spatial_layer_id == 2
          ? denoiser->num_ref_frames
          : 0;
  YV12_BUFFER_CONFIG avg = denoiser->running_avg_y[INTRA_FRAME + shift];
  const int denoise_layer_index =
      cpi->svc.number_spatial_layers - cpi->svc.spatial_layer_id - 1;
  YV12_BUFFER_CONFIG mc_avg = denoiser->mc_running_avg_y[denoise_layer_index];
  uint8_t *avg_start = block_start(avg.y_buffer, avg.y_stride, mi_row, mi_col);

  uint8_t *mc_avg_start =
      block_start(mc_avg.y_buffer, mc_avg.y_stride, mi_row, mi_col);
  struct buf_2d src = mb->plane[0].src;
  int increase_denoising = 0;
  int last_is_reference = cpi->ref_frame_flags & AOM_LAST_FLAG;
  mv_col = ctx->best_sse_mv.as_mv.col;
  mv_row = ctx->best_sse_mv.as_mv.row;
  motion_magnitude = mv_row * mv_row + mv_col * mv_col;

  if (denoiser->denoising_level == kDenHigh) increase_denoising = 1;

  // Copy block if LAST_FRAME is not a reference.
  // Last doesn't always exist when SVC layers are dynamically changed, e.g. top
  // spatial layer doesn't have last reference when it's brought up for the
  // first time on the fly.
  if (last_is_reference && denoiser->denoising_level >= kDenLow &&
      !ctx->sb_skip_denoising)
    decision = perform_motion_compensation(
        &cpi->common, denoiser, mb, bs, increase_denoising, mi_row, mi_col, ctx,
        motion_magnitude, &zeromv_filter, cpi->svc.number_spatial_layers,
        cpi->source->y_width, cpi->ppi->rtc_ref.ref_idx[0],
        cpi->ppi->rtc_ref.ref_idx[3], cpi->ppi->use_svc,
        cpi->svc.spatial_layer_id, use_gf_temporal_ref);

  if (decision == FILTER_BLOCK) {
    decision = av1_denoiser_filter(src.buf, src.stride, mc_avg_start,
                                   mc_avg.y_stride, avg_start, avg.y_stride,
                                   increase_denoising, bs, motion_magnitude);
  }

  if (decision == FILTER_BLOCK) {
    aom_convolve_copy(avg_start, avg.y_stride, src.buf, src.stride,
                      block_size_wide[bs], block_size_high[bs]);
  } else {  // COPY_BLOCK
    aom_convolve_copy(src.buf, src.stride, avg_start, avg.y_stride,
                      block_size_wide[bs], block_size_high[bs]);
  }
  *denoiser_decision = decision;
  if (decision == FILTER_BLOCK && zeromv_filter == 1)
    *denoiser_decision = FILTER_ZEROMV_BLOCK;
}

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

static void swap_frame_buffer(YV12_BUFFER_CONFIG *const dest,
                              YV12_BUFFER_CONFIG *const src) {
  uint8_t *tmp_buf = dest->y_buffer;
  assert(dest->y_width == src->y_width);
  assert(dest->y_height == src->y_height);
  dest->y_buffer = src->y_buffer;
  src->y_buffer = tmp_buf;
}

void av1_denoiser_update_frame_info(
    AV1_DENOISER *denoiser, YV12_BUFFER_CONFIG src, struct RTC_REF *rtc_ref,
    struct SVC *svc, FRAME_TYPE frame_type, int refresh_alt_ref_frame,
    int refresh_golden_frame, int refresh_last_frame, int alt_fb_idx,
    int gld_fb_idx, int lst_fb_idx, int resized,
    int svc_refresh_denoiser_buffers, int second_spatial_layer) {
  const int shift = second_spatial_layer ? denoiser->num_ref_frames : 0;
  // Copy source into denoised reference buffers on KEY_FRAME or
  // if the just encoded frame was resized. For SVC, copy source if the base
  // spatial layer was key frame.
  if (frame_type == KEY_FRAME || resized != 0 || denoiser->reset ||
      svc_refresh_denoiser_buffers) {
    int i;
    // Start at 1 so as not to overwrite the INTRA_FRAME
    for (i = 1; i < denoiser->num_ref_frames; ++i) {
      if (denoiser->running_avg_y[i + shift].buffer_alloc != NULL)
        copy_frame(&denoiser->running_avg_y[i + shift], &src);
    }
    denoiser->reset = 0;
    return;
  }

  if (rtc_ref->set_ref_frame_config) {
    int i;
    for (i = 0; i < REF_FRAMES; i++) {
      if (rtc_ref->refresh[svc->spatial_layer_id] & (1 << i))
        copy_frame(&denoiser->running_avg_y[i + 1 + shift],
                   &denoiser->running_avg_y[INTRA_FRAME + shift]);
    }
  } else {
    // If more than one refresh occurs, must copy frame buffer.
    if ((refresh_alt_ref_frame + refresh_golden_frame + refresh_last_frame) >
        1) {
      if (refresh_alt_ref_frame) {
        copy_frame(&denoiser->running_avg_y[alt_fb_idx + 1 + shift],
                   &denoiser->running_avg_y[INTRA_FRAME + shift]);
      }
      if (refresh_golden_frame) {
        copy_frame(&denoiser->running_avg_y[gld_fb_idx + 1 + shift],
                   &denoiser->running_avg_y[INTRA_FRAME + shift]);
      }
      if (refresh_last_frame) {
        copy_frame(&denoiser->running_avg_y[lst_fb_idx + 1 + shift],
                   &denoiser->running_avg_y[INTRA_FRAME + shift]);
      }
    } else {
      if (refresh_alt_ref_frame) {
        swap_frame_buffer(&denoiser->running_avg_y[alt_fb_idx + 1 + shift],
                          &denoiser->running_avg_y[INTRA_FRAME + shift]);
      }
      if (refresh_golden_frame) {
        swap_frame_buffer(&denoiser->running_avg_y[gld_fb_idx + 1 + shift],
                          &denoiser->running_avg_y[INTRA_FRAME + shift]);
      }
      if (refresh_last_frame) {
        swap_frame_buffer(&denoiser->running_avg_y[lst_fb_idx + 1 + shift],
                          &denoiser->running_avg_y[INTRA_FRAME + shift]);
      }
    }
  }
}

void av1_denoiser_reset_frame_stats(PICK_MODE_CONTEXT *ctx) {
  ctx->zeromv_sse = INT64_MAX;
  ctx->newmv_sse = INT64_MAX;
  ctx->zeromv_lastref_sse = INT64_MAX;
  ctx->best_sse_mv.as_int = 0;
}

void av1_denoiser_update_frame_stats(MB_MODE_INFO *mi, int64_t sse,
                                     PREDICTION_MODE mode,
                                     PICK_MODE_CONTEXT *ctx) {
  if (mi->mv[0].as_int == 0 && sse < ctx->zeromv_sse) {
    ctx->zeromv_sse = sse;
    ctx->best_zeromv_reference_frame = mi->ref_frame[0];
    if (mi->ref_frame[0] == LAST_FRAME) ctx->zeromv_lastref_sse = sse;
  }

  if (mi->mv[0].as_int != 0 && sse < ctx->newmv_sse) {
    ctx->newmv_sse = sse;
    ctx->best_sse_inter_mode = mode;
    ctx->best_sse_mv = mi->mv[0];
    ctx->best_reference_frame = mi->ref_frame[0];
  }
}

static int av1_denoiser_realloc_svc_helper(AV1_COMMON *cm,
                                           AV1_DENOISER *denoiser, int fb_idx) {
  int fail = 0;
  if (denoiser->running_avg_y[fb_idx].buffer_alloc == NULL) {
    fail = aom_alloc_frame_buffer(
        &denoiser->running_avg_y[fb_idx], cm->width, cm->height,
        cm->seq_params->subsampling_x, cm->seq_params->subsampling_y,
        cm->seq_params->use_highbitdepth, AOM_BORDER_IN_PIXELS,
        cm->features.byte_alignment, 0, 0);
    if (fail) {
      av1_denoiser_free(denoiser);
      return 1;
    }
  }
  return 0;
}

int av1_denoiser_realloc_svc(AV1_COMMON *cm, AV1_DENOISER *denoiser,
                             struct RTC_REF *rtc_ref, struct SVC *svc,
                             int svc_buf_shift, int refresh_alt,
                             int refresh_gld, int refresh_lst, int alt_fb_idx,
                             int gld_fb_idx, int lst_fb_idx) {
  int fail = 0;
  if (rtc_ref->set_ref_frame_config) {
    int i;
    for (i = 0; i < REF_FRAMES; i++) {
      if (cm->current_frame.frame_type == KEY_FRAME ||
          rtc_ref->refresh[svc->spatial_layer_id] & (1 << i)) {
        fail = av1_denoiser_realloc_svc_helper(cm, denoiser,
                                               i + 1 + svc_buf_shift);
      }
    }
  } else {
    if (refresh_alt) {
      // Increase the frame buffer index by 1 to map it to the buffer index in
      // the denoiser.
      fail = av1_denoiser_realloc_svc_helper(cm, denoiser,
                                             alt_fb_idx + 1 + svc_buf_shift);
      if (fail) return 1;
    }
    if (refresh_gld) {
      fail = av1_denoiser_realloc_svc_helper(cm, denoiser,
                                             gld_fb_idx + 1 + svc_buf_shift);
      if (fail) return 1;
    }
    if (refresh_lst) {
      fail = av1_denoiser_realloc_svc_helper(cm, denoiser,
                                             lst_fb_idx + 1 + svc_buf_shift);
      if (fail) return 1;
    }
  }
  return 0;
}

int av1_denoiser_alloc(AV1_COMMON *cm, struct SVC *svc, AV1_DENOISER *denoiser,
                       int use_svc, int noise_sen, int width, int height,
                       int ssx, int ssy, int use_highbitdepth, int border) {
  int i, layer, fail, init_num_ref_frames;
  const int legacy_byte_alignment = 0;
  int num_layers = 1;
  int scaled_width = width;
  int scaled_height = height;
  if (use_svc) {
    LAYER_CONTEXT *lc = &svc->layer_context[svc->spatial_layer_id *
                                                svc->number_temporal_layers +
                                            svc->temporal_layer_id];
    av1_get_layer_resolution(width, height, lc->scaling_factor_num,
                             lc->scaling_factor_den, &scaled_width,
                             &scaled_height);
    // For SVC: only denoise at most 2 spatial (highest) layers.
    if (noise_sen >= 2)
      // Denoise from one spatial layer below the top.
      svc->first_layer_denoise = AOMMAX(svc->number_spatial_layers - 2, 0);
    else
      // Only denoise the top spatial layer.
      svc->first_layer_denoise = AOMMAX(svc->number_spatial_layers - 1, 0);
    num_layers = svc->number_spatial_layers - svc->first_layer_denoise;
  }
  assert(denoiser != NULL);
  denoiser->num_ref_frames = use_svc ? SVC_REF_FRAMES : NONSVC_REF_FRAMES;
  init_num_ref_frames = use_svc ? REF_FRAMES : NONSVC_REF_FRAMES;
  denoiser->num_layers = num_layers;
  CHECK_MEM_ERROR(cm, denoiser->running_avg_y,
                  aom_calloc(denoiser->num_ref_frames * num_layers,
                             sizeof(denoiser->running_avg_y[0])));
  CHECK_MEM_ERROR(
      cm, denoiser->mc_running_avg_y,
      aom_calloc(num_layers, sizeof(denoiser->mc_running_avg_y[0])));

  for (layer = 0; layer < num_layers; ++layer) {
    const int denoise_width = (layer == 0) ? width : scaled_width;
    const int denoise_height = (layer == 0) ? height : scaled_height;
    for (i = 0; i < init_num_ref_frames; ++i) {
      fail = aom_alloc_frame_buffer(
          &denoiser->running_avg_y[i + denoiser->num_ref_frames * layer],
          denoise_width, denoise_height, ssx, ssy, use_highbitdepth, border,
          legacy_byte_alignment, 0, 0);
      if (fail) {
        av1_denoiser_free(denoiser);
        return 1;
      }
#ifdef OUTPUT_YUV_DENOISED
      make_grayscale(&denoiser->running_avg_y[i]);
#endif
    }

    fail = aom_alloc_frame_buffer(
        &denoiser->mc_running_avg_y[layer], denoise_width, denoise_height, ssx,
        ssy, use_highbitdepth, border, legacy_byte_alignment, 0, 0);
    if (fail) {
      av1_denoiser_free(denoiser);
      return 1;
    }
  }

  // denoiser->last_source only used for noise_estimation, so only for top
  // layer.
  fail = aom_alloc_frame_buffer(&denoiser->last_source, width, height, ssx, ssy,
                                use_highbitdepth, border, legacy_byte_alignment,
                                0, 0);
  if (fail) {
    av1_denoiser_free(denoiser);
    return 1;
  }
#ifdef OUTPUT_YUV_DENOISED
  make_grayscale(&denoiser->running_avg_y[i]);
#endif
  denoiser->frame_buffer_initialized = 1;
  denoiser->denoising_level = kDenMedium;
  denoiser->prev_denoising_level = kDenMedium;
  denoiser->reset = 0;
  denoiser->current_denoiser_frame = 0;
  return 0;
}

void av1_denoiser_free(AV1_DENOISER *denoiser) {
  int i;
  if (denoiser == NULL) {
    return;
  }
  denoiser->frame_buffer_initialized = 0;
  for (i = 0; i < denoiser->num_ref_frames * denoiser->num_layers; ++i) {
    aom_free_frame_buffer(&denoiser->running_avg_y[i]);
  }
  aom_free(denoiser->running_avg_y);
  denoiser->running_avg_y = NULL;

  for (i = 0; i < denoiser->num_layers; ++i) {
    aom_free_frame_buffer(&denoiser->mc_running_avg_y[i]);
  }

  aom_free(denoiser->mc_running_avg_y);
  denoiser->mc_running_avg_y = NULL;
  aom_free_frame_buffer(&denoiser->last_source);
}

// TODO(kyslov) Enable when SVC temporal denosing is implemented
#if 0
static void force_refresh_longterm_ref(AV1_COMP *const cpi) {
  SVC *const svc = &cpi->svc;
  // If long term reference is used, force refresh of that slot, so
  // denoiser buffer for long term reference stays in sync.
  if (svc->use_gf_temporal_ref_current_layer) {
    int index = svc->spatial_layer_id;
    if (svc->number_spatial_layers == 3) index = svc->spatial_layer_id - 1;
    assert(index >= 0);
    cpi->alt_fb_idx = svc->buffer_gf_temporal_ref[index].idx;
    cpi->refresh_alt_ref_frame = 1;
  }
}
#endif

void av1_denoiser_set_noise_level(AV1_COMP *const cpi, int noise_level) {
  AV1_DENOISER *const denoiser = &cpi->denoiser;
  denoiser->denoising_level = noise_level;
  if (denoiser->denoising_level > kDenLowLow &&
      denoiser->prev_denoising_level == kDenLowLow) {
    denoiser->reset = 1;
// TODO(kyslov) Enable when SVC temporal denosing is implemented
#if 0
    force_refresh_longterm_ref(cpi);
#endif
  } else {
    denoiser->reset = 0;
  }
  denoiser->prev_denoising_level = denoiser->denoising_level;
}

// Scale/increase the partition threshold
// for denoiser speed-up.
int64_t av1_scale_part_thresh(int64_t threshold, AV1_DENOISER_LEVEL noise_level,
                              CONTENT_STATE_SB content_state,
                              int temporal_layer_id) {
  if ((content_state.source_sad_nonrd <= kLowSad &&
       content_state.low_sumdiff) ||
      (content_state.source_sad_nonrd == kHighSad &&
       content_state.low_sumdiff) ||
      (content_state.lighting_change && !content_state.low_sumdiff) ||
      (noise_level == kDenHigh) || (temporal_layer_id != 0)) {
    int64_t scaled_thr =
        (temporal_layer_id < 2) ? (3 * threshold) >> 1 : (7 * threshold) >> 2;
    return scaled_thr;
  } else {
    return (5 * threshold) >> 2;
  }
}

//  Scale/increase the ac skip threshold for
//  denoiser speed-up.
int64_t av1_scale_acskip_thresh(int64_t threshold,
                                AV1_DENOISER_LEVEL noise_level, int abs_sumdiff,
                                int temporal_layer_id) {
  if (noise_level >= kDenLow && abs_sumdiff < 5)
    threshold *= (noise_level == kDenLow)   ? 2
                 : (temporal_layer_id == 2) ? 10
                                            : 6;
  return threshold;
}

void av1_denoiser_reset_on_first_frame(AV1_COMP *const cpi) {
  if (/*av1_denoise_svc_non_key(cpi) &&*/
      cpi->denoiser.current_denoiser_frame == 0) {
    cpi->denoiser.reset = 1;
// TODO(kyslov) Enable when SVC temporal denosing is implemented
#if 0
    force_refresh_longterm_ref(cpi);
#endif
  }
}

void av1_denoiser_update_ref_frame(AV1_COMP *const cpi) {
  AV1_COMMON *const cm = &cpi->common;
  RTC_REF *const rtc_ref = &cpi->ppi->rtc_ref;
  SVC *const svc = &cpi->svc;

  if (cpi->oxcf.noise_sensitivity > 0 && denoise_svc(cpi) &&
      cpi->denoiser.denoising_level > kDenLowLow) {
    int svc_refresh_denoiser_buffers = 0;
    int denoise_svc_second_layer = 0;
    FRAME_TYPE frame_type = cm->current_frame.frame_type == INTRA_ONLY_FRAME
                                ? KEY_FRAME
                                : cm->current_frame.frame_type;
    cpi->denoiser.current_denoiser_frame++;
    const int resize_pending = is_frame_resize_pending(cpi);

    if (cpi->ppi->use_svc) {
// TODO(kyslov) Enable when SVC temporal denosing is implemented
#if 0
      const int svc_buf_shift =
          svc->number_spatial_layers - svc->spatial_layer_id == 2
              ? cpi->denoiser.num_ref_frames
              : 0;
      int layer =
          LAYER_IDS_TO_IDX(svc->spatial_layer_id, svc->temporal_layer_id,
                           svc->number_temporal_layers);
      LAYER_CONTEXT *const lc = &svc->layer_context[layer];
      svc_refresh_denoiser_buffers =
          lc->is_key_frame || svc->spatial_layer_sync[svc->spatial_layer_id];
      denoise_svc_second_layer =
          svc->number_spatial_layers - svc->spatial_layer_id == 2 ? 1 : 0;
      // Check if we need to allocate extra buffers in the denoiser
      // for refreshed frames.
      if (av1_denoiser_realloc_svc(cm, &cpi->denoiser, rtc_ref,
                                   svc, svc_buf_shift,
                                   cpi->refresh_alt_ref_frame,
                                   cpi->refresh_golden_frame,
                                   cpi->refresh_last_frame, cpi->alt_fb_idx,
                                   cpi->gld_fb_idx, cpi->lst_fb_idx))
        aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR,
                           "Failed to re-allocate denoiser for SVC");
#endif
    }
    av1_denoiser_update_frame_info(
        &cpi->denoiser, *cpi->source, rtc_ref, svc, frame_type,
        cpi->refresh_frame.alt_ref_frame, cpi->refresh_frame.golden_frame, 1,
        rtc_ref->ref_idx[6], rtc_ref->ref_idx[3], rtc_ref->ref_idx[0],
        resize_pending, svc_refresh_denoiser_buffers, denoise_svc_second_layer);
  }
}

#ifdef OUTPUT_YUV_DENOISED
static void make_grayscale(YV12_BUFFER_CONFIG *yuv) {
  int r, c;
  uint8_t *u = yuv->u_buffer;
  uint8_t *v = yuv->v_buffer;

  for (r = 0; r < yuv->uv_height; ++r) {
    for (c = 0; c < yuv->uv_width; ++c) {
      u[c] = UINT8_MAX / 2;
      v[c] = UINT8_MAX / 2;
    }
    u += yuv->uv_stride;
    v += yuv->uv_stride;
  }
}

void aom_write_yuv_frame(FILE *yuv_file, YV12_BUFFER_CONFIG *s) {
  unsigned char *src = s->y_buffer;
  int h = s->y_crop_height;

  do {
    fwrite(src, s->y_width, 1, yuv_file);
    src += s->y_stride;
  } while (--h);

  src = s->u_buffer;
  h = s->uv_crop_height;

  do {
    fwrite(src, s->uv_width, 1, yuv_file);
    src += s->uv_stride;
  } while (--h);

  src = s->v_buffer;
  h = s->uv_crop_height;

  do {
    fwrite(src, s->uv_width, 1, yuv_file);
    src += s->uv_stride;
  } while (--h);
}
#endif
