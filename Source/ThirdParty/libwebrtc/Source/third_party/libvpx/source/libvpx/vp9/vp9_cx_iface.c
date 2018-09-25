/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <string.h>

#include "./vpx_config.h"
#include "vpx/vpx_encoder.h"
#include "vpx_ports/vpx_once.h"
#include "vpx_ports/system_state.h"
#include "vpx/internal/vpx_codec_internal.h"
#include "./vpx_version.h"
#include "vp9/encoder/vp9_encoder.h"
#include "vpx/vp8cx.h"
#include "vp9/encoder/vp9_firstpass.h"
#include "vp9/vp9_iface_common.h"

struct vp9_extracfg {
  int cpu_used;  // available cpu percentage in 1/16
  unsigned int enable_auto_alt_ref;
  unsigned int noise_sensitivity;
  unsigned int sharpness;
  unsigned int static_thresh;
  unsigned int tile_columns;
  unsigned int tile_rows;
  unsigned int arnr_max_frames;
  unsigned int arnr_strength;
  unsigned int min_gf_interval;
  unsigned int max_gf_interval;
  vp8e_tuning tuning;
  unsigned int cq_level;  // constrained quality level
  unsigned int rc_max_intra_bitrate_pct;
  unsigned int rc_max_inter_bitrate_pct;
  unsigned int gf_cbr_boost_pct;
  unsigned int lossless;
  unsigned int target_level;
  unsigned int frame_parallel_decoding_mode;
  AQ_MODE aq_mode;
  int alt_ref_aq;
  unsigned int frame_periodic_boost;
  vpx_bit_depth_t bit_depth;
  vp9e_tune_content content;
  vpx_color_space_t color_space;
  vpx_color_range_t color_range;
  int render_width;
  int render_height;
  unsigned int row_mt;
  unsigned int motion_vector_unit_test;
};

static struct vp9_extracfg default_extra_cfg = {
  0,                     // cpu_used
  1,                     // enable_auto_alt_ref
  0,                     // noise_sensitivity
  0,                     // sharpness
  0,                     // static_thresh
  6,                     // tile_columns
  0,                     // tile_rows
  7,                     // arnr_max_frames
  5,                     // arnr_strength
  0,                     // min_gf_interval; 0 -> default decision
  0,                     // max_gf_interval; 0 -> default decision
  VP8_TUNE_PSNR,         // tuning
  10,                    // cq_level
  0,                     // rc_max_intra_bitrate_pct
  0,                     // rc_max_inter_bitrate_pct
  0,                     // gf_cbr_boost_pct
  0,                     // lossless
  255,                   // target_level
  1,                     // frame_parallel_decoding_mode
  NO_AQ,                 // aq_mode
  0,                     // alt_ref_aq
  0,                     // frame_periodic_delta_q
  VPX_BITS_8,            // Bit depth
  VP9E_CONTENT_DEFAULT,  // content
  VPX_CS_UNKNOWN,        // color space
  0,                     // color range
  0,                     // render width
  0,                     // render height
  0,                     // row_mt
  0,                     // motion_vector_unit_test
};

struct vpx_codec_alg_priv {
  vpx_codec_priv_t base;
  vpx_codec_enc_cfg_t cfg;
  struct vp9_extracfg extra_cfg;
  VP9EncoderConfig oxcf;
  VP9_COMP *cpi;
  unsigned char *cx_data;
  size_t cx_data_sz;
  unsigned char *pending_cx_data;
  size_t pending_cx_data_sz;
  int pending_frame_count;
  size_t pending_frame_sizes[8];
  size_t pending_frame_magnitude;
  vpx_image_t preview_img;
  vpx_enc_frame_flags_t next_frame_flags;
  vp8_postproc_cfg_t preview_ppcfg;
  vpx_codec_pkt_list_decl(256) pkt_list;
  unsigned int fixed_kf_cntr;
  vpx_codec_priv_output_cx_pkt_cb_pair_t output_cx_pkt_cb;
  // BufferPool that holds all reference frames.
  BufferPool *buffer_pool;
};

static vpx_codec_err_t update_error_state(
    vpx_codec_alg_priv_t *ctx, const struct vpx_internal_error_info *error) {
  const vpx_codec_err_t res = error->error_code;

  if (res != VPX_CODEC_OK)
    ctx->base.err_detail = error->has_detail ? error->detail : NULL;

  return res;
}

#undef ERROR
#define ERROR(str)                  \
  do {                              \
    ctx->base.err_detail = str;     \
    return VPX_CODEC_INVALID_PARAM; \
  } while (0)

#define RANGE_CHECK(p, memb, lo, hi)                                 \
  do {                                                               \
    if (!(((p)->memb == lo || (p)->memb > (lo)) && (p)->memb <= hi)) \
      ERROR(#memb " out of range [" #lo ".." #hi "]");               \
  } while (0)

#define RANGE_CHECK_HI(p, memb, hi)                                     \
  do {                                                                  \
    if (!((p)->memb <= (hi))) ERROR(#memb " out of range [.." #hi "]"); \
  } while (0)

#define RANGE_CHECK_LO(p, memb, lo)                                     \
  do {                                                                  \
    if (!((p)->memb >= (lo))) ERROR(#memb " out of range [" #lo "..]"); \
  } while (0)

#define RANGE_CHECK_BOOL(p, memb)                                     \
  do {                                                                \
    if (!!((p)->memb) != (p)->memb) ERROR(#memb " expected boolean"); \
  } while (0)

static vpx_codec_err_t validate_config(vpx_codec_alg_priv_t *ctx,
                                       const vpx_codec_enc_cfg_t *cfg,
                                       const struct vp9_extracfg *extra_cfg) {
  RANGE_CHECK(cfg, g_w, 1, 65535);  // 16 bits available
  RANGE_CHECK(cfg, g_h, 1, 65535);  // 16 bits available
  RANGE_CHECK(cfg, g_timebase.den, 1, 1000000000);
  RANGE_CHECK(cfg, g_timebase.num, 1, 1000000000);
  RANGE_CHECK_HI(cfg, g_profile, 3);

  RANGE_CHECK_HI(cfg, rc_max_quantizer, 63);
  RANGE_CHECK_HI(cfg, rc_min_quantizer, cfg->rc_max_quantizer);
  RANGE_CHECK_BOOL(extra_cfg, lossless);
  RANGE_CHECK_BOOL(extra_cfg, frame_parallel_decoding_mode);
  RANGE_CHECK(extra_cfg, aq_mode, 0, AQ_MODE_COUNT - 2);
  RANGE_CHECK(extra_cfg, alt_ref_aq, 0, 1);
  RANGE_CHECK(extra_cfg, frame_periodic_boost, 0, 1);
  RANGE_CHECK_HI(cfg, g_threads, 64);
  RANGE_CHECK_HI(cfg, g_lag_in_frames, MAX_LAG_BUFFERS);
  RANGE_CHECK(cfg, rc_end_usage, VPX_VBR, VPX_Q);
  RANGE_CHECK_HI(cfg, rc_undershoot_pct, 100);
  RANGE_CHECK_HI(cfg, rc_overshoot_pct, 100);
  RANGE_CHECK_HI(cfg, rc_2pass_vbr_bias_pct, 100);
  RANGE_CHECK(cfg, rc_2pass_vbr_corpus_complexity, 0, 10000);
  RANGE_CHECK(cfg, kf_mode, VPX_KF_DISABLED, VPX_KF_AUTO);
  RANGE_CHECK_BOOL(cfg, rc_resize_allowed);
  RANGE_CHECK_HI(cfg, rc_dropframe_thresh, 100);
  RANGE_CHECK_HI(cfg, rc_resize_up_thresh, 100);
  RANGE_CHECK_HI(cfg, rc_resize_down_thresh, 100);
#if CONFIG_REALTIME_ONLY
  RANGE_CHECK(cfg, g_pass, VPX_RC_ONE_PASS, VPX_RC_ONE_PASS);
#else
  RANGE_CHECK(cfg, g_pass, VPX_RC_ONE_PASS, VPX_RC_LAST_PASS);
#endif
  RANGE_CHECK(extra_cfg, min_gf_interval, 0, (MAX_LAG_BUFFERS - 1));
  RANGE_CHECK(extra_cfg, max_gf_interval, 0, (MAX_LAG_BUFFERS - 1));
  if (extra_cfg->max_gf_interval > 0) {
    RANGE_CHECK(extra_cfg, max_gf_interval, 2, (MAX_LAG_BUFFERS - 1));
  }
  if (extra_cfg->min_gf_interval > 0 && extra_cfg->max_gf_interval > 0) {
    RANGE_CHECK(extra_cfg, max_gf_interval, extra_cfg->min_gf_interval,
                (MAX_LAG_BUFFERS - 1));
  }

  // For formation of valid ARF groups lag_in _frames should be 0 or greater
  // than the max_gf_interval + 2
  if (cfg->g_lag_in_frames > 0 && extra_cfg->max_gf_interval > 0 &&
      cfg->g_lag_in_frames < extra_cfg->max_gf_interval + 2) {
    ERROR("Set lag in frames to 0 (low delay) or >= (max-gf-interval + 2)");
  }

  if (cfg->rc_resize_allowed == 1) {
    RANGE_CHECK(cfg, rc_scaled_width, 0, cfg->g_w);
    RANGE_CHECK(cfg, rc_scaled_height, 0, cfg->g_h);
  }

  RANGE_CHECK(cfg, ss_number_layers, 1, VPX_SS_MAX_LAYERS);
  RANGE_CHECK(cfg, ts_number_layers, 1, VPX_TS_MAX_LAYERS);

  {
    unsigned int level = extra_cfg->target_level;
    if (level != LEVEL_1 && level != LEVEL_1_1 && level != LEVEL_2 &&
        level != LEVEL_2_1 && level != LEVEL_3 && level != LEVEL_3_1 &&
        level != LEVEL_4 && level != LEVEL_4_1 && level != LEVEL_5 &&
        level != LEVEL_5_1 && level != LEVEL_5_2 && level != LEVEL_6 &&
        level != LEVEL_6_1 && level != LEVEL_6_2 && level != LEVEL_UNKNOWN &&
        level != LEVEL_AUTO && level != LEVEL_MAX)
      ERROR("target_level is invalid");
  }

  if (cfg->ss_number_layers * cfg->ts_number_layers > VPX_MAX_LAYERS)
    ERROR("ss_number_layers * ts_number_layers is out of range");
  if (cfg->ts_number_layers > 1) {
    unsigned int sl, tl;
    for (sl = 1; sl < cfg->ss_number_layers; ++sl) {
      for (tl = 1; tl < cfg->ts_number_layers; ++tl) {
        const int layer = LAYER_IDS_TO_IDX(sl, tl, cfg->ts_number_layers);
        if (cfg->layer_target_bitrate[layer] <
            cfg->layer_target_bitrate[layer - 1])
          ERROR("ts_target_bitrate entries are not increasing");
      }
    }

    RANGE_CHECK(cfg, ts_rate_decimator[cfg->ts_number_layers - 1], 1, 1);
    for (tl = cfg->ts_number_layers - 2; tl > 0; --tl)
      if (cfg->ts_rate_decimator[tl - 1] != 2 * cfg->ts_rate_decimator[tl])
        ERROR("ts_rate_decimator factors are not powers of 2");
  }

#if CONFIG_SPATIAL_SVC

  if ((cfg->ss_number_layers > 1 || cfg->ts_number_layers > 1) &&
      cfg->g_pass == VPX_RC_LAST_PASS) {
    unsigned int i, alt_ref_sum = 0;
    for (i = 0; i < cfg->ss_number_layers; ++i) {
      if (cfg->ss_enable_auto_alt_ref[i]) ++alt_ref_sum;
    }
    if (alt_ref_sum > REF_FRAMES - cfg->ss_number_layers)
      ERROR("Not enough ref buffers for svc alt ref frames");
    if (cfg->ss_number_layers * cfg->ts_number_layers > 3 &&
        cfg->g_error_resilient == 0)
      ERROR("Multiple frame context are not supported for more than 3 layers");
  }
#endif

  // VP9 does not support a lower bound on the keyframe interval in
  // automatic keyframe placement mode.
  if (cfg->kf_mode != VPX_KF_DISABLED && cfg->kf_min_dist != cfg->kf_max_dist &&
      cfg->kf_min_dist > 0)
    ERROR(
        "kf_min_dist not supported in auto mode, use 0 "
        "or kf_max_dist instead.");

  RANGE_CHECK(extra_cfg, row_mt, 0, 1);
  RANGE_CHECK(extra_cfg, motion_vector_unit_test, 0, 2);
  RANGE_CHECK(extra_cfg, enable_auto_alt_ref, 0, 2);
  RANGE_CHECK(extra_cfg, cpu_used, -8, 8);
  RANGE_CHECK_HI(extra_cfg, noise_sensitivity, 6);
  RANGE_CHECK(extra_cfg, tile_columns, 0, 6);
  RANGE_CHECK(extra_cfg, tile_rows, 0, 2);
  RANGE_CHECK_HI(extra_cfg, sharpness, 7);
  RANGE_CHECK(extra_cfg, arnr_max_frames, 0, 15);
  RANGE_CHECK_HI(extra_cfg, arnr_strength, 6);
  RANGE_CHECK(extra_cfg, cq_level, 0, 63);
  RANGE_CHECK(cfg, g_bit_depth, VPX_BITS_8, VPX_BITS_12);
  RANGE_CHECK(cfg, g_input_bit_depth, 8, 12);
  RANGE_CHECK(extra_cfg, content, VP9E_CONTENT_DEFAULT,
              VP9E_CONTENT_INVALID - 1);

  // TODO(yaowu): remove this when ssim tuning is implemented for vp9
  if (extra_cfg->tuning == VP8_TUNE_SSIM)
    ERROR("Option --tune=ssim is not currently supported in VP9.");

#if !CONFIG_REALTIME_ONLY
  if (cfg->g_pass == VPX_RC_LAST_PASS) {
    const size_t packet_sz = sizeof(FIRSTPASS_STATS);
    const int n_packets = (int)(cfg->rc_twopass_stats_in.sz / packet_sz);
    const FIRSTPASS_STATS *stats;

    if (cfg->rc_twopass_stats_in.buf == NULL)
      ERROR("rc_twopass_stats_in.buf not set.");

    if (cfg->rc_twopass_stats_in.sz % packet_sz)
      ERROR("rc_twopass_stats_in.sz indicates truncated packet.");

    if (cfg->ss_number_layers > 1 || cfg->ts_number_layers > 1) {
      int i;
      unsigned int n_packets_per_layer[VPX_SS_MAX_LAYERS] = { 0 };

      stats = cfg->rc_twopass_stats_in.buf;
      for (i = 0; i < n_packets; ++i) {
        const int layer_id = (int)stats[i].spatial_layer_id;
        if (layer_id >= 0 && layer_id < (int)cfg->ss_number_layers) {
          ++n_packets_per_layer[layer_id];
        }
      }

      for (i = 0; i < (int)cfg->ss_number_layers; ++i) {
        unsigned int layer_id;
        if (n_packets_per_layer[i] < 2) {
          ERROR(
              "rc_twopass_stats_in requires at least two packets for each "
              "layer.");
        }

        stats = (const FIRSTPASS_STATS *)cfg->rc_twopass_stats_in.buf +
                n_packets - cfg->ss_number_layers + i;
        layer_id = (int)stats->spatial_layer_id;

        if (layer_id >= cfg->ss_number_layers ||
            (unsigned int)(stats->count + 0.5) !=
                n_packets_per_layer[layer_id] - 1)
          ERROR("rc_twopass_stats_in missing EOS stats packet");
      }
    } else {
      if (cfg->rc_twopass_stats_in.sz < 2 * packet_sz)
        ERROR("rc_twopass_stats_in requires at least two packets.");

      stats =
          (const FIRSTPASS_STATS *)cfg->rc_twopass_stats_in.buf + n_packets - 1;

      if ((int)(stats->count + 0.5) != n_packets - 1)
        ERROR("rc_twopass_stats_in missing EOS stats packet");
    }
  }
#endif  // !CONFIG_REALTIME_ONLY

#if !CONFIG_VP9_HIGHBITDEPTH
  if (cfg->g_profile > (unsigned int)PROFILE_1) {
    ERROR("Profile > 1 not supported in this build configuration");
  }
#endif
  if (cfg->g_profile <= (unsigned int)PROFILE_1 &&
      cfg->g_bit_depth > VPX_BITS_8) {
    ERROR("Codec high bit-depth not supported in profile < 2");
  }
  if (cfg->g_profile <= (unsigned int)PROFILE_1 && cfg->g_input_bit_depth > 8) {
    ERROR("Source high bit-depth not supported in profile < 2");
  }
  if (cfg->g_profile > (unsigned int)PROFILE_1 &&
      cfg->g_bit_depth == VPX_BITS_8) {
    ERROR("Codec bit-depth 8 not supported in profile > 1");
  }
  RANGE_CHECK(extra_cfg, color_space, VPX_CS_UNKNOWN, VPX_CS_SRGB);
  RANGE_CHECK(extra_cfg, color_range, VPX_CR_STUDIO_RANGE, VPX_CR_FULL_RANGE);
  return VPX_CODEC_OK;
}

static vpx_codec_err_t validate_img(vpx_codec_alg_priv_t *ctx,
                                    const vpx_image_t *img) {
  switch (img->fmt) {
    case VPX_IMG_FMT_YV12:
    case VPX_IMG_FMT_I420:
    case VPX_IMG_FMT_I42016: break;
    case VPX_IMG_FMT_I422:
    case VPX_IMG_FMT_I444:
    case VPX_IMG_FMT_I440:
      if (ctx->cfg.g_profile != (unsigned int)PROFILE_1) {
        ERROR(
            "Invalid image format. I422, I444, I440 images are "
            "not supported in profile.");
      }
      break;
    case VPX_IMG_FMT_I42216:
    case VPX_IMG_FMT_I44416:
    case VPX_IMG_FMT_I44016:
      if (ctx->cfg.g_profile != (unsigned int)PROFILE_1 &&
          ctx->cfg.g_profile != (unsigned int)PROFILE_3) {
        ERROR(
            "Invalid image format. 16-bit I422, I444, I440 images are "
            "not supported in profile.");
      }
      break;
    default:
      ERROR(
          "Invalid image format. Only YV12, I420, I422, I444 images are "
          "supported.");
      break;
  }

  if (img->d_w != ctx->cfg.g_w || img->d_h != ctx->cfg.g_h)
    ERROR("Image size must match encoder init configuration size");

  return VPX_CODEC_OK;
}

static int get_image_bps(const vpx_image_t *img) {
  switch (img->fmt) {
    case VPX_IMG_FMT_YV12:
    case VPX_IMG_FMT_I420: return 12;
    case VPX_IMG_FMT_I422: return 16;
    case VPX_IMG_FMT_I444: return 24;
    case VPX_IMG_FMT_I440: return 16;
    case VPX_IMG_FMT_I42016: return 24;
    case VPX_IMG_FMT_I42216: return 32;
    case VPX_IMG_FMT_I44416: return 48;
    case VPX_IMG_FMT_I44016: return 32;
    default: assert(0 && "Invalid image format"); break;
  }
  return 0;
}

// Modify the encoder config for the target level.
static void config_target_level(VP9EncoderConfig *oxcf) {
  double max_average_bitrate;  // in bits per second
  int max_over_shoot_pct;
  const int target_level_index = get_level_index(oxcf->target_level);

  vpx_clear_system_state();
  assert(target_level_index >= 0);
  assert(target_level_index < VP9_LEVELS);

  // Maximum target bit-rate is level_limit * 80%.
  max_average_bitrate =
      vp9_level_defs[target_level_index].average_bitrate * 800.0;
  if ((double)oxcf->target_bandwidth > max_average_bitrate)
    oxcf->target_bandwidth = (int64_t)(max_average_bitrate);
  if (oxcf->ss_number_layers == 1 && oxcf->pass != 0)
    oxcf->ss_target_bitrate[0] = (int)oxcf->target_bandwidth;

  // Adjust max over-shoot percentage.
  max_over_shoot_pct =
      (int)((max_average_bitrate * 1.10 - (double)oxcf->target_bandwidth) *
            100 / (double)(oxcf->target_bandwidth));
  if (oxcf->over_shoot_pct > max_over_shoot_pct)
    oxcf->over_shoot_pct = max_over_shoot_pct;

  // Adjust worst allowed quantizer.
  oxcf->worst_allowed_q = vp9_quantizer_to_qindex(63);

  // Adjust minimum art-ref distance.
  // min_gf_interval should be no less than min_altref_distance + 1,
  // as the encoder may produce bitstream with alt-ref distance being
  // min_gf_interval - 1.
  if (oxcf->min_gf_interval <=
      (int)vp9_level_defs[target_level_index].min_altref_distance) {
    oxcf->min_gf_interval =
        (int)vp9_level_defs[target_level_index].min_altref_distance + 1;
    // If oxcf->max_gf_interval == 0, it will be assigned with a default value
    // in vp9_rc_set_gf_interval_range().
    if (oxcf->max_gf_interval != 0) {
      oxcf->max_gf_interval =
          VPXMAX(oxcf->max_gf_interval, oxcf->min_gf_interval);
    }
  }

  // Adjust maximum column tiles.
  if (vp9_level_defs[target_level_index].max_col_tiles <
      (1 << oxcf->tile_columns)) {
    while (oxcf->tile_columns > 0 &&
           vp9_level_defs[target_level_index].max_col_tiles <
               (1 << oxcf->tile_columns))
      --oxcf->tile_columns;
  }
}

static vpx_codec_err_t set_encoder_config(
    VP9EncoderConfig *oxcf, const vpx_codec_enc_cfg_t *cfg,
    const struct vp9_extracfg *extra_cfg) {
  const int is_vbr = cfg->rc_end_usage == VPX_VBR;
  int sl, tl;
  oxcf->profile = cfg->g_profile;
  oxcf->max_threads = (int)cfg->g_threads;
  oxcf->width = cfg->g_w;
  oxcf->height = cfg->g_h;
  oxcf->bit_depth = cfg->g_bit_depth;
  oxcf->input_bit_depth = cfg->g_input_bit_depth;
  // guess a frame rate if out of whack, use 30
  oxcf->init_framerate = (double)cfg->g_timebase.den / cfg->g_timebase.num;
  if (oxcf->init_framerate > 180) oxcf->init_framerate = 30;

  oxcf->mode = GOOD;

  switch (cfg->g_pass) {
    case VPX_RC_ONE_PASS: oxcf->pass = 0; break;
    case VPX_RC_FIRST_PASS: oxcf->pass = 1; break;
    case VPX_RC_LAST_PASS: oxcf->pass = 2; break;
  }

  oxcf->lag_in_frames =
      cfg->g_pass == VPX_RC_FIRST_PASS ? 0 : cfg->g_lag_in_frames;
  oxcf->rc_mode = cfg->rc_end_usage;

  // Convert target bandwidth from Kbit/s to Bit/s
  oxcf->target_bandwidth = 1000 * cfg->rc_target_bitrate;
  oxcf->rc_max_intra_bitrate_pct = extra_cfg->rc_max_intra_bitrate_pct;
  oxcf->rc_max_inter_bitrate_pct = extra_cfg->rc_max_inter_bitrate_pct;
  oxcf->gf_cbr_boost_pct = extra_cfg->gf_cbr_boost_pct;

  oxcf->best_allowed_q =
      extra_cfg->lossless ? 0 : vp9_quantizer_to_qindex(cfg->rc_min_quantizer);
  oxcf->worst_allowed_q =
      extra_cfg->lossless ? 0 : vp9_quantizer_to_qindex(cfg->rc_max_quantizer);
  oxcf->cq_level = vp9_quantizer_to_qindex(extra_cfg->cq_level);
  oxcf->fixed_q = -1;

  oxcf->under_shoot_pct = cfg->rc_undershoot_pct;
  oxcf->over_shoot_pct = cfg->rc_overshoot_pct;

  oxcf->scaled_frame_width = cfg->rc_scaled_width;
  oxcf->scaled_frame_height = cfg->rc_scaled_height;
  if (cfg->rc_resize_allowed == 1) {
    oxcf->resize_mode =
        (oxcf->scaled_frame_width == 0 || oxcf->scaled_frame_height == 0)
            ? RESIZE_DYNAMIC
            : RESIZE_FIXED;
  } else {
    oxcf->resize_mode = RESIZE_NONE;
  }

  oxcf->maximum_buffer_size_ms = is_vbr ? 240000 : cfg->rc_buf_sz;
  oxcf->starting_buffer_level_ms = is_vbr ? 60000 : cfg->rc_buf_initial_sz;
  oxcf->optimal_buffer_level_ms = is_vbr ? 60000 : cfg->rc_buf_optimal_sz;

  oxcf->drop_frames_water_mark = cfg->rc_dropframe_thresh;

  oxcf->two_pass_vbrbias = cfg->rc_2pass_vbr_bias_pct;
  oxcf->two_pass_vbrmin_section = cfg->rc_2pass_vbr_minsection_pct;
  oxcf->two_pass_vbrmax_section = cfg->rc_2pass_vbr_maxsection_pct;
  oxcf->vbr_corpus_complexity = cfg->rc_2pass_vbr_corpus_complexity;

  oxcf->auto_key =
      cfg->kf_mode == VPX_KF_AUTO && cfg->kf_min_dist != cfg->kf_max_dist;

  oxcf->key_freq = cfg->kf_max_dist;

  oxcf->speed = abs(extra_cfg->cpu_used);
  oxcf->encode_breakout = extra_cfg->static_thresh;
  oxcf->enable_auto_arf = extra_cfg->enable_auto_alt_ref;
  oxcf->noise_sensitivity = extra_cfg->noise_sensitivity;
  oxcf->sharpness = extra_cfg->sharpness;

  oxcf->two_pass_stats_in = cfg->rc_twopass_stats_in;

#if CONFIG_FP_MB_STATS
  oxcf->firstpass_mb_stats_in = cfg->rc_firstpass_mb_stats_in;
#endif

  oxcf->color_space = extra_cfg->color_space;
  oxcf->color_range = extra_cfg->color_range;
  oxcf->render_width = extra_cfg->render_width;
  oxcf->render_height = extra_cfg->render_height;
  oxcf->arnr_max_frames = extra_cfg->arnr_max_frames;
  oxcf->arnr_strength = extra_cfg->arnr_strength;
  oxcf->min_gf_interval = extra_cfg->min_gf_interval;
  oxcf->max_gf_interval = extra_cfg->max_gf_interval;

  oxcf->tuning = extra_cfg->tuning;
  oxcf->content = extra_cfg->content;

  oxcf->tile_columns = extra_cfg->tile_columns;

  // TODO(yunqing): The dependencies between row tiles cause error in multi-
  // threaded encoding. For now, tile_rows is forced to be 0 in this case.
  // The further fix can be done by adding synchronizations after a tile row
  // is encoded. But this will hurt multi-threaded encoder performance. So,
  // it is recommended to use tile-rows=0 while encoding with threads > 1.
  if (oxcf->max_threads > 1 && oxcf->tile_columns > 0)
    oxcf->tile_rows = 0;
  else
    oxcf->tile_rows = extra_cfg->tile_rows;

  oxcf->error_resilient_mode = cfg->g_error_resilient;
  oxcf->frame_parallel_decoding_mode = extra_cfg->frame_parallel_decoding_mode;

  oxcf->aq_mode = extra_cfg->aq_mode;
  oxcf->alt_ref_aq = extra_cfg->alt_ref_aq;

  oxcf->frame_periodic_boost = extra_cfg->frame_periodic_boost;

  oxcf->ss_number_layers = cfg->ss_number_layers;
  oxcf->ts_number_layers = cfg->ts_number_layers;
  oxcf->temporal_layering_mode =
      (enum vp9e_temporal_layering_mode)cfg->temporal_layering_mode;

  oxcf->target_level = extra_cfg->target_level;

  oxcf->row_mt = extra_cfg->row_mt;
  oxcf->motion_vector_unit_test = extra_cfg->motion_vector_unit_test;

  for (sl = 0; sl < oxcf->ss_number_layers; ++sl) {
#if CONFIG_SPATIAL_SVC
    oxcf->ss_enable_auto_arf[sl] = cfg->ss_enable_auto_alt_ref[sl];
#endif
    for (tl = 0; tl < oxcf->ts_number_layers; ++tl) {
      oxcf->layer_target_bitrate[sl * oxcf->ts_number_layers + tl] =
          1000 * cfg->layer_target_bitrate[sl * oxcf->ts_number_layers + tl];
    }
  }
  if (oxcf->ss_number_layers == 1 && oxcf->pass != 0) {
    oxcf->ss_target_bitrate[0] = (int)oxcf->target_bandwidth;
#if CONFIG_SPATIAL_SVC
    oxcf->ss_enable_auto_arf[0] = extra_cfg->enable_auto_alt_ref;
#endif
  }
  if (oxcf->ts_number_layers > 1) {
    for (tl = 0; tl < VPX_TS_MAX_LAYERS; ++tl) {
      oxcf->ts_rate_decimator[tl] =
          cfg->ts_rate_decimator[tl] ? cfg->ts_rate_decimator[tl] : 1;
    }
  } else if (oxcf->ts_number_layers == 1) {
    oxcf->ts_rate_decimator[0] = 1;
  }

  if (get_level_index(oxcf->target_level) >= 0) config_target_level(oxcf);
  /*
  printf("Current VP9 Settings: \n");
  printf("target_bandwidth: %d\n", oxcf->target_bandwidth);
  printf("target_level: %d\n", oxcf->target_level);
  printf("noise_sensitivity: %d\n", oxcf->noise_sensitivity);
  printf("sharpness: %d\n",    oxcf->sharpness);
  printf("cpu_used: %d\n",  oxcf->cpu_used);
  printf("Mode: %d\n",     oxcf->mode);
  printf("auto_key: %d\n",  oxcf->auto_key);
  printf("key_freq: %d\n", oxcf->key_freq);
  printf("end_usage: %d\n", oxcf->end_usage);
  printf("under_shoot_pct: %d\n", oxcf->under_shoot_pct);
  printf("over_shoot_pct: %d\n", oxcf->over_shoot_pct);
  printf("starting_buffer_level: %d\n", oxcf->starting_buffer_level);
  printf("optimal_buffer_level: %d\n",  oxcf->optimal_buffer_level);
  printf("maximum_buffer_size: %d\n", oxcf->maximum_buffer_size);
  printf("fixed_q: %d\n",  oxcf->fixed_q);
  printf("worst_allowed_q: %d\n", oxcf->worst_allowed_q);
  printf("best_allowed_q: %d\n", oxcf->best_allowed_q);
  printf("allow_spatial_resampling: %d\n", oxcf->allow_spatial_resampling);
  printf("scaled_frame_width: %d\n", oxcf->scaled_frame_width);
  printf("scaled_frame_height: %d\n", oxcf->scaled_frame_height);
  printf("two_pass_vbrbias: %d\n",  oxcf->two_pass_vbrbias);
  printf("two_pass_vbrmin_section: %d\n", oxcf->two_pass_vbrmin_section);
  printf("two_pass_vbrmax_section: %d\n", oxcf->two_pass_vbrmax_section);
  printf("vbr_corpus_complexity: %d\n",  oxcf->vbr_corpus_complexity);
  printf("lag_in_frames: %d\n", oxcf->lag_in_frames);
  printf("enable_auto_arf: %d\n", oxcf->enable_auto_arf);
  printf("Version: %d\n", oxcf->Version);
  printf("encode_breakout: %d\n", oxcf->encode_breakout);
  printf("error resilient: %d\n", oxcf->error_resilient_mode);
  printf("frame parallel detokenization: %d\n",
         oxcf->frame_parallel_decoding_mode);
  */
  return VPX_CODEC_OK;
}

static vpx_codec_err_t encoder_set_config(vpx_codec_alg_priv_t *ctx,
                                          const vpx_codec_enc_cfg_t *cfg) {
  vpx_codec_err_t res;
  int force_key = 0;

  if (cfg->g_w != ctx->cfg.g_w || cfg->g_h != ctx->cfg.g_h) {
    if (cfg->g_lag_in_frames > 1 || cfg->g_pass != VPX_RC_ONE_PASS)
      ERROR("Cannot change width or height after initialization");
    if (!valid_ref_frame_size(ctx->cfg.g_w, ctx->cfg.g_h, cfg->g_w, cfg->g_h) ||
        (ctx->cpi->initial_width && (int)cfg->g_w > ctx->cpi->initial_width) ||
        (ctx->cpi->initial_height && (int)cfg->g_h > ctx->cpi->initial_height))
      force_key = 1;
  }

  // Prevent increasing lag_in_frames. This check is stricter than it needs
  // to be -- the limit is not increasing past the first lag_in_frames
  // value, but we don't track the initial config, only the last successful
  // config.
  if (cfg->g_lag_in_frames > ctx->cfg.g_lag_in_frames)
    ERROR("Cannot increase lag_in_frames");

  res = validate_config(ctx, cfg, &ctx->extra_cfg);

  if (res == VPX_CODEC_OK) {
    ctx->cfg = *cfg;
    set_encoder_config(&ctx->oxcf, &ctx->cfg, &ctx->extra_cfg);
    // On profile change, request a key frame
    force_key |= ctx->cpi->common.profile != ctx->oxcf.profile;
    vp9_change_config(ctx->cpi, &ctx->oxcf);
  }

  if (force_key) ctx->next_frame_flags |= VPX_EFLAG_FORCE_KF;

  return res;
}

static vpx_codec_err_t ctrl_get_quantizer(vpx_codec_alg_priv_t *ctx,
                                          va_list args) {
  int *const arg = va_arg(args, int *);
  if (arg == NULL) return VPX_CODEC_INVALID_PARAM;
  *arg = vp9_get_quantizer(ctx->cpi);
  return VPX_CODEC_OK;
}

static vpx_codec_err_t ctrl_get_quantizer64(vpx_codec_alg_priv_t *ctx,
                                            va_list args) {
  int *const arg = va_arg(args, int *);
  if (arg == NULL) return VPX_CODEC_INVALID_PARAM;
  *arg = vp9_qindex_to_quantizer(vp9_get_quantizer(ctx->cpi));
  return VPX_CODEC_OK;
}

static vpx_codec_err_t update_extra_cfg(vpx_codec_alg_priv_t *ctx,
                                        const struct vp9_extracfg *extra_cfg) {
  const vpx_codec_err_t res = validate_config(ctx, &ctx->cfg, extra_cfg);
  if (res == VPX_CODEC_OK) {
    ctx->extra_cfg = *extra_cfg;
    set_encoder_config(&ctx->oxcf, &ctx->cfg, &ctx->extra_cfg);
    vp9_change_config(ctx->cpi, &ctx->oxcf);
  }
  return res;
}

static vpx_codec_err_t ctrl_set_cpuused(vpx_codec_alg_priv_t *ctx,
                                        va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.cpu_used = CAST(VP8E_SET_CPUUSED, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_enable_auto_alt_ref(vpx_codec_alg_priv_t *ctx,
                                                    va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.enable_auto_alt_ref = CAST(VP8E_SET_ENABLEAUTOALTREF, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_noise_sensitivity(vpx_codec_alg_priv_t *ctx,
                                                  va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.noise_sensitivity = CAST(VP9E_SET_NOISE_SENSITIVITY, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_sharpness(vpx_codec_alg_priv_t *ctx,
                                          va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.sharpness = CAST(VP8E_SET_SHARPNESS, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_static_thresh(vpx_codec_alg_priv_t *ctx,
                                              va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.static_thresh = CAST(VP8E_SET_STATIC_THRESHOLD, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_tile_columns(vpx_codec_alg_priv_t *ctx,
                                             va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.tile_columns = CAST(VP9E_SET_TILE_COLUMNS, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_tile_rows(vpx_codec_alg_priv_t *ctx,
                                          va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.tile_rows = CAST(VP9E_SET_TILE_ROWS, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_arnr_max_frames(vpx_codec_alg_priv_t *ctx,
                                                va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.arnr_max_frames = CAST(VP8E_SET_ARNR_MAXFRAMES, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_arnr_strength(vpx_codec_alg_priv_t *ctx,
                                              va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.arnr_strength = CAST(VP8E_SET_ARNR_STRENGTH, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_arnr_type(vpx_codec_alg_priv_t *ctx,
                                          va_list args) {
  (void)ctx;
  (void)args;
  return VPX_CODEC_OK;
}

static vpx_codec_err_t ctrl_set_tuning(vpx_codec_alg_priv_t *ctx,
                                       va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.tuning = CAST(VP8E_SET_TUNING, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_cq_level(vpx_codec_alg_priv_t *ctx,
                                         va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.cq_level = CAST(VP8E_SET_CQ_LEVEL, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_rc_max_intra_bitrate_pct(
    vpx_codec_alg_priv_t *ctx, va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.rc_max_intra_bitrate_pct =
      CAST(VP8E_SET_MAX_INTRA_BITRATE_PCT, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_rc_max_inter_bitrate_pct(
    vpx_codec_alg_priv_t *ctx, va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.rc_max_inter_bitrate_pct =
      CAST(VP8E_SET_MAX_INTER_BITRATE_PCT, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_rc_gf_cbr_boost_pct(vpx_codec_alg_priv_t *ctx,
                                                    va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.gf_cbr_boost_pct = CAST(VP9E_SET_GF_CBR_BOOST_PCT, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_lossless(vpx_codec_alg_priv_t *ctx,
                                         va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.lossless = CAST(VP9E_SET_LOSSLESS, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_frame_parallel_decoding_mode(
    vpx_codec_alg_priv_t *ctx, va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.frame_parallel_decoding_mode =
      CAST(VP9E_SET_FRAME_PARALLEL_DECODING, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_aq_mode(vpx_codec_alg_priv_t *ctx,
                                        va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.aq_mode = CAST(VP9E_SET_AQ_MODE, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_alt_ref_aq(vpx_codec_alg_priv_t *ctx,
                                           va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.alt_ref_aq = CAST(VP9E_SET_ALT_REF_AQ, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_min_gf_interval(vpx_codec_alg_priv_t *ctx,
                                                va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.min_gf_interval = CAST(VP9E_SET_MIN_GF_INTERVAL, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_max_gf_interval(vpx_codec_alg_priv_t *ctx,
                                                va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.max_gf_interval = CAST(VP9E_SET_MAX_GF_INTERVAL, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_frame_periodic_boost(vpx_codec_alg_priv_t *ctx,
                                                     va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.frame_periodic_boost = CAST(VP9E_SET_FRAME_PERIODIC_BOOST, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_target_level(vpx_codec_alg_priv_t *ctx,
                                             va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.target_level = CAST(VP9E_SET_TARGET_LEVEL, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_row_mt(vpx_codec_alg_priv_t *ctx,
                                       va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.row_mt = CAST(VP9E_SET_ROW_MT, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_enable_motion_vector_unit_test(
    vpx_codec_alg_priv_t *ctx, va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.motion_vector_unit_test =
      CAST(VP9E_ENABLE_MOTION_VECTOR_UNIT_TEST, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_get_level(vpx_codec_alg_priv_t *ctx, va_list args) {
  int *const arg = va_arg(args, int *);
  if (arg == NULL) return VPX_CODEC_INVALID_PARAM;
  *arg = (int)vp9_get_level(&ctx->cpi->level_info.level_spec);
  return VPX_CODEC_OK;
}

static vpx_codec_err_t encoder_init(vpx_codec_ctx_t *ctx,
                                    vpx_codec_priv_enc_mr_cfg_t *data) {
  vpx_codec_err_t res = VPX_CODEC_OK;
  (void)data;

  if (ctx->priv == NULL) {
    vpx_codec_alg_priv_t *const priv = vpx_calloc(1, sizeof(*priv));
    if (priv == NULL) return VPX_CODEC_MEM_ERROR;

    ctx->priv = (vpx_codec_priv_t *)priv;
    ctx->priv->init_flags = ctx->init_flags;
    ctx->priv->enc.total_encoders = 1;
    priv->buffer_pool = (BufferPool *)vpx_calloc(1, sizeof(BufferPool));
    if (priv->buffer_pool == NULL) return VPX_CODEC_MEM_ERROR;

    if (ctx->config.enc) {
      // Update the reference to the config structure to an internal copy.
      priv->cfg = *ctx->config.enc;
      ctx->config.enc = &priv->cfg;
    }

    priv->extra_cfg = default_extra_cfg;
    once(vp9_initialize_enc);

    res = validate_config(priv, &priv->cfg, &priv->extra_cfg);

    if (res == VPX_CODEC_OK) {
      set_encoder_config(&priv->oxcf, &priv->cfg, &priv->extra_cfg);
#if CONFIG_VP9_HIGHBITDEPTH
      priv->oxcf.use_highbitdepth =
          (ctx->init_flags & VPX_CODEC_USE_HIGHBITDEPTH) ? 1 : 0;
#endif
      priv->cpi = vp9_create_compressor(&priv->oxcf, priv->buffer_pool);
      if (priv->cpi == NULL)
        res = VPX_CODEC_MEM_ERROR;
      else
        priv->cpi->output_pkt_list = &priv->pkt_list.head;
    }
  }

  return res;
}

static vpx_codec_err_t encoder_destroy(vpx_codec_alg_priv_t *ctx) {
  free(ctx->cx_data);
  vp9_remove_compressor(ctx->cpi);
  vpx_free(ctx->buffer_pool);
  vpx_free(ctx);
  return VPX_CODEC_OK;
}

static void pick_quickcompress_mode(vpx_codec_alg_priv_t *ctx,
                                    unsigned long duration,
                                    unsigned long deadline) {
  MODE new_mode = BEST;

#if CONFIG_REALTIME_ONLY
  (void)duration;
  deadline = VPX_DL_REALTIME;
#else
  switch (ctx->cfg.g_pass) {
    case VPX_RC_ONE_PASS:
      if (deadline > 0) {
        const vpx_codec_enc_cfg_t *const cfg = &ctx->cfg;

        // Convert duration parameter from stream timebase to microseconds.
        const uint64_t duration_us = (uint64_t)duration * 1000000 *
                                     (uint64_t)cfg->g_timebase.num /
                                     (uint64_t)cfg->g_timebase.den;

        // If the deadline is more that the duration this frame is to be shown,
        // use good quality mode. Otherwise use realtime mode.
        new_mode = (deadline > duration_us) ? GOOD : REALTIME;
      } else {
        new_mode = BEST;
      }
      break;
    case VPX_RC_FIRST_PASS: break;
    case VPX_RC_LAST_PASS: new_mode = deadline > 0 ? GOOD : BEST; break;
  }
#endif  // CONFIG_REALTIME_ONLY

  if (deadline == VPX_DL_REALTIME) {
    ctx->oxcf.pass = 0;
    new_mode = REALTIME;
  }

  if (ctx->oxcf.mode != new_mode) {
    ctx->oxcf.mode = new_mode;
    vp9_change_config(ctx->cpi, &ctx->oxcf);
  }
}

// Turn on to test if supplemental superframe data breaks decoding
// #define TEST_SUPPLEMENTAL_SUPERFRAME_DATA
static int write_superframe_index(vpx_codec_alg_priv_t *ctx) {
  uint8_t marker = 0xc0;
  unsigned int mask;
  int mag, index_sz;

  assert(ctx->pending_frame_count);
  assert(ctx->pending_frame_count <= 8);

  // Add the number of frames to the marker byte
  marker |= ctx->pending_frame_count - 1;

  // Choose the magnitude
  for (mag = 0, mask = 0xff; mag < 4; mag++) {
    if (ctx->pending_frame_magnitude < mask) break;
    mask <<= 8;
    mask |= 0xff;
  }
  marker |= mag << 3;

  // Write the index
  index_sz = 2 + (mag + 1) * ctx->pending_frame_count;
  if (ctx->pending_cx_data_sz + index_sz < ctx->cx_data_sz) {
    uint8_t *x = ctx->pending_cx_data + ctx->pending_cx_data_sz;
    int i, j;
#ifdef TEST_SUPPLEMENTAL_SUPERFRAME_DATA
    uint8_t marker_test = 0xc0;
    int mag_test = 2;     // 1 - 4
    int frames_test = 4;  // 1 - 8
    int index_sz_test = 2 + mag_test * frames_test;
    marker_test |= frames_test - 1;
    marker_test |= (mag_test - 1) << 3;
    *x++ = marker_test;
    for (i = 0; i < mag_test * frames_test; ++i)
      *x++ = 0;  // fill up with arbitrary data
    *x++ = marker_test;
    ctx->pending_cx_data_sz += index_sz_test;
    printf("Added supplemental superframe data\n");
#endif

    *x++ = marker;
    for (i = 0; i < ctx->pending_frame_count; i++) {
      unsigned int this_sz = (unsigned int)ctx->pending_frame_sizes[i];

      for (j = 0; j <= mag; j++) {
        *x++ = this_sz & 0xff;
        this_sz >>= 8;
      }
    }
    *x++ = marker;
    ctx->pending_cx_data_sz += index_sz;
#ifdef TEST_SUPPLEMENTAL_SUPERFRAME_DATA
    index_sz += index_sz_test;
#endif
  }
  return index_sz;
}

static int64_t timebase_units_to_ticks(const vpx_rational_t *timebase,
                                       int64_t n) {
  return n * TICKS_PER_SEC * timebase->num / timebase->den;
}

static int64_t ticks_to_timebase_units(const vpx_rational_t *timebase,
                                       int64_t n) {
  const int64_t round = (int64_t)TICKS_PER_SEC * timebase->num / 2 - 1;
  return (n * timebase->den + round) / timebase->num / TICKS_PER_SEC;
}

static vpx_codec_frame_flags_t get_frame_pkt_flags(const VP9_COMP *cpi,
                                                   unsigned int lib_flags) {
  vpx_codec_frame_flags_t flags = lib_flags << 16;

  if (lib_flags & FRAMEFLAGS_KEY ||
      (cpi->use_svc && cpi->svc
                           .layer_context[cpi->svc.spatial_layer_id *
                                              cpi->svc.number_temporal_layers +
                                          cpi->svc.temporal_layer_id]
                           .is_key_frame))
    flags |= VPX_FRAME_IS_KEY;

  if (cpi->droppable) flags |= VPX_FRAME_IS_DROPPABLE;

  return flags;
}

const size_t kMinCompressedSize = 8192;
static vpx_codec_err_t encoder_encode(vpx_codec_alg_priv_t *ctx,
                                      const vpx_image_t *img,
                                      vpx_codec_pts_t pts,
                                      unsigned long duration,
                                      vpx_enc_frame_flags_t enc_flags,
                                      unsigned long deadline) {
  volatile vpx_codec_err_t res = VPX_CODEC_OK;
  volatile vpx_enc_frame_flags_t flags = enc_flags;
  VP9_COMP *const cpi = ctx->cpi;
  const vpx_rational_t *const timebase = &ctx->cfg.g_timebase;
  size_t data_sz;

  if (cpi == NULL) return VPX_CODEC_INVALID_PARAM;

  if (cpi->oxcf.pass == 2 && cpi->level_constraint.level_index >= 0 &&
      !cpi->level_constraint.rc_config_updated) {
    SVC *const svc = &cpi->svc;
    const int is_two_pass_svc =
        (svc->number_spatial_layers > 1) || (svc->number_temporal_layers > 1);
    const VP9EncoderConfig *const oxcf = &cpi->oxcf;
    TWO_PASS *const twopass = &cpi->twopass;
    FIRSTPASS_STATS *stats = &twopass->total_stats;
    if (is_two_pass_svc) {
      const double frame_rate = 10000000.0 * stats->count / stats->duration;
      vp9_update_spatial_layer_framerate(cpi, frame_rate);
      twopass->bits_left =
          (int64_t)(stats->duration *
                    svc->layer_context[svc->spatial_layer_id].target_bandwidth /
                    10000000.0);
    } else {
      twopass->bits_left =
          (int64_t)(stats->duration * oxcf->target_bandwidth / 10000000.0);
    }
    cpi->level_constraint.rc_config_updated = 1;
  }

  if (img != NULL) {
    res = validate_img(ctx, img);
    if (res == VPX_CODEC_OK) {
      // There's no codec control for multiple alt-refs so check the encoder
      // instance for its status to determine the compressed data size.
      data_sz = ctx->cfg.g_w * ctx->cfg.g_h * get_image_bps(img) / 8 *
                (cpi->multi_arf_allowed ? 8 : 2);
      if (data_sz < kMinCompressedSize) data_sz = kMinCompressedSize;
      if (ctx->cx_data == NULL || ctx->cx_data_sz < data_sz) {
        ctx->cx_data_sz = data_sz;
        free(ctx->cx_data);
        ctx->cx_data = (unsigned char *)malloc(ctx->cx_data_sz);
        if (ctx->cx_data == NULL) {
          return VPX_CODEC_MEM_ERROR;
        }
      }
    }
  }

  pick_quickcompress_mode(ctx, duration, deadline);
  vpx_codec_pkt_list_init(&ctx->pkt_list);

  // Handle Flags
  if (((flags & VP8_EFLAG_NO_UPD_GF) && (flags & VP8_EFLAG_FORCE_GF)) ||
      ((flags & VP8_EFLAG_NO_UPD_ARF) && (flags & VP8_EFLAG_FORCE_ARF))) {
    ctx->base.err_detail = "Conflicting flags.";
    return VPX_CODEC_INVALID_PARAM;
  }

  if (setjmp(cpi->common.error.jmp)) {
    cpi->common.error.setjmp = 0;
    res = update_error_state(ctx, &cpi->common.error);
    vpx_clear_system_state();
    return res;
  }
  cpi->common.error.setjmp = 1;

  if (res == VPX_CODEC_OK) vp9_apply_encoding_flags(cpi, flags);

  // Handle fixed keyframe intervals
  if (ctx->cfg.kf_mode == VPX_KF_AUTO &&
      ctx->cfg.kf_min_dist == ctx->cfg.kf_max_dist) {
    if (++ctx->fixed_kf_cntr > ctx->cfg.kf_min_dist) {
      flags |= VPX_EFLAG_FORCE_KF;
      ctx->fixed_kf_cntr = 1;
    }
  }

  if (res == VPX_CODEC_OK) {
    unsigned int lib_flags = 0;
    YV12_BUFFER_CONFIG sd;
    int64_t dst_time_stamp = timebase_units_to_ticks(timebase, pts);
    int64_t dst_end_time_stamp =
        timebase_units_to_ticks(timebase, pts + duration);
    size_t size, cx_data_sz;
    unsigned char *cx_data;

    // Set up internal flags
    if (ctx->base.init_flags & VPX_CODEC_USE_PSNR) cpi->b_calculate_psnr = 1;

    if (img != NULL) {
      res = image2yuvconfig(img, &sd);

      // Store the original flags in to the frame buffer. Will extract the
      // key frame flag when we actually encode this frame.
      if (vp9_receive_raw_frame(cpi, flags | ctx->next_frame_flags, &sd,
                                dst_time_stamp, dst_end_time_stamp)) {
        res = update_error_state(ctx, &cpi->common.error);
      }
      ctx->next_frame_flags = 0;
    }

    cx_data = ctx->cx_data;
    cx_data_sz = ctx->cx_data_sz;

    /* Any pending invisible frames? */
    if (ctx->pending_cx_data) {
      memmove(cx_data, ctx->pending_cx_data, ctx->pending_cx_data_sz);
      ctx->pending_cx_data = cx_data;
      cx_data += ctx->pending_cx_data_sz;
      cx_data_sz -= ctx->pending_cx_data_sz;

      /* TODO: this is a minimal check, the underlying codec doesn't respect
       * the buffer size anyway.
       */
      if (cx_data_sz < ctx->cx_data_sz / 2) {
        vpx_internal_error(&cpi->common.error, VPX_CODEC_ERROR,
                           "Compressed data buffer too small");
        return VPX_CODEC_ERROR;
      }
    }

    while (cx_data_sz >= ctx->cx_data_sz / 2 &&
           -1 != vp9_get_compressed_data(cpi, &lib_flags, &size, cx_data,
                                         &dst_time_stamp, &dst_end_time_stamp,
                                         !img)) {
      if (size || (cpi->use_svc && cpi->svc.skip_enhancement_layer)) {
        vpx_codec_cx_pkt_t pkt;

#if CONFIG_SPATIAL_SVC
        if (cpi->use_svc)
          cpi->svc
              .layer_context[cpi->svc.spatial_layer_id *
                             cpi->svc.number_temporal_layers]
              .layer_size += size;
#endif

        // Pack invisible frames with the next visible frame
        if (!cpi->common.show_frame ||
            (cpi->use_svc &&
             cpi->svc.spatial_layer_id < cpi->svc.number_spatial_layers - 1)) {
          if (ctx->pending_cx_data == 0) ctx->pending_cx_data = cx_data;
          ctx->pending_cx_data_sz += size;
          ctx->pending_frame_sizes[ctx->pending_frame_count++] = size;
          ctx->pending_frame_magnitude |= size;
          cx_data += size;
          cx_data_sz -= size;
          pkt.data.frame.width[cpi->svc.spatial_layer_id] = cpi->common.width;
          pkt.data.frame.height[cpi->svc.spatial_layer_id] = cpi->common.height;

          if (ctx->output_cx_pkt_cb.output_cx_pkt) {
            pkt.kind = VPX_CODEC_CX_FRAME_PKT;
            pkt.data.frame.pts =
                ticks_to_timebase_units(timebase, dst_time_stamp);
            pkt.data.frame.duration = (unsigned long)ticks_to_timebase_units(
                timebase, dst_end_time_stamp - dst_time_stamp);
            pkt.data.frame.flags = get_frame_pkt_flags(cpi, lib_flags);
            pkt.data.frame.buf = ctx->pending_cx_data;
            pkt.data.frame.sz = size;
            ctx->pending_cx_data = NULL;
            ctx->pending_cx_data_sz = 0;
            ctx->pending_frame_count = 0;
            ctx->pending_frame_magnitude = 0;
            ctx->output_cx_pkt_cb.output_cx_pkt(
                &pkt, ctx->output_cx_pkt_cb.user_priv);
          }
          continue;
        }

        // Add the frame packet to the list of returned packets.
        pkt.kind = VPX_CODEC_CX_FRAME_PKT;
        pkt.data.frame.pts = ticks_to_timebase_units(timebase, dst_time_stamp);
        pkt.data.frame.duration = (unsigned long)ticks_to_timebase_units(
            timebase, dst_end_time_stamp - dst_time_stamp);
        pkt.data.frame.flags = get_frame_pkt_flags(cpi, lib_flags);
        pkt.data.frame.width[cpi->svc.spatial_layer_id] = cpi->common.width;
        pkt.data.frame.height[cpi->svc.spatial_layer_id] = cpi->common.height;

        if (ctx->pending_cx_data) {
          if (size) ctx->pending_frame_sizes[ctx->pending_frame_count++] = size;
          ctx->pending_frame_magnitude |= size;
          ctx->pending_cx_data_sz += size;
          // write the superframe only for the case when
          if (!ctx->output_cx_pkt_cb.output_cx_pkt)
            size += write_superframe_index(ctx);
          pkt.data.frame.buf = ctx->pending_cx_data;
          pkt.data.frame.sz = ctx->pending_cx_data_sz;
          ctx->pending_cx_data = NULL;
          ctx->pending_cx_data_sz = 0;
          ctx->pending_frame_count = 0;
          ctx->pending_frame_magnitude = 0;
        } else {
          pkt.data.frame.buf = cx_data;
          pkt.data.frame.sz = size;
        }
        pkt.data.frame.partition_id = -1;

        if (ctx->output_cx_pkt_cb.output_cx_pkt)
          ctx->output_cx_pkt_cb.output_cx_pkt(&pkt,
                                              ctx->output_cx_pkt_cb.user_priv);
        else
          vpx_codec_pkt_list_add(&ctx->pkt_list.head, &pkt);

        cx_data += size;
        cx_data_sz -= size;
#if CONFIG_SPATIAL_SVC && defined(VPX_TEST_SPATIAL_SVC)
        if (cpi->use_svc && !ctx->output_cx_pkt_cb.output_cx_pkt) {
          vpx_codec_cx_pkt_t pkt_sizes, pkt_psnr;
          int sl;
          vp9_zero(pkt_sizes);
          vp9_zero(pkt_psnr);
          pkt_sizes.kind = VPX_CODEC_SPATIAL_SVC_LAYER_SIZES;
          pkt_psnr.kind = VPX_CODEC_SPATIAL_SVC_LAYER_PSNR;
          for (sl = 0; sl < cpi->svc.number_spatial_layers; ++sl) {
            LAYER_CONTEXT *lc =
                &cpi->svc.layer_context[sl * cpi->svc.number_temporal_layers];
            pkt_sizes.data.layer_sizes[sl] = lc->layer_size;
            pkt_psnr.data.layer_psnr[sl] = lc->psnr_pkt;
            lc->layer_size = 0;
          }

          vpx_codec_pkt_list_add(&ctx->pkt_list.head, &pkt_sizes);

          vpx_codec_pkt_list_add(&ctx->pkt_list.head, &pkt_psnr);
        }
#endif
        if (is_one_pass_cbr_svc(cpi) &&
            (cpi->svc.spatial_layer_id == cpi->svc.number_spatial_layers - 1)) {
          // Encoded all spatial layers; exit loop.
          break;
        }
      }
    }
  }

  cpi->common.error.setjmp = 0;
  return res;
}

static const vpx_codec_cx_pkt_t *encoder_get_cxdata(vpx_codec_alg_priv_t *ctx,
                                                    vpx_codec_iter_t *iter) {
  return vpx_codec_pkt_list_get(&ctx->pkt_list.head, iter);
}

static vpx_codec_err_t ctrl_set_reference(vpx_codec_alg_priv_t *ctx,
                                          va_list args) {
  vpx_ref_frame_t *const frame = va_arg(args, vpx_ref_frame_t *);

  if (frame != NULL) {
    YV12_BUFFER_CONFIG sd;

    image2yuvconfig(&frame->img, &sd);
    vp9_set_reference_enc(ctx->cpi, ref_frame_to_vp9_reframe(frame->frame_type),
                          &sd);
    return VPX_CODEC_OK;
  }
  return VPX_CODEC_INVALID_PARAM;
}

static vpx_codec_err_t ctrl_copy_reference(vpx_codec_alg_priv_t *ctx,
                                           va_list args) {
  vpx_ref_frame_t *const frame = va_arg(args, vpx_ref_frame_t *);

  if (frame != NULL) {
    YV12_BUFFER_CONFIG sd;

    image2yuvconfig(&frame->img, &sd);
    vp9_copy_reference_enc(ctx->cpi,
                           ref_frame_to_vp9_reframe(frame->frame_type), &sd);
    return VPX_CODEC_OK;
  }
  return VPX_CODEC_INVALID_PARAM;
}

static vpx_codec_err_t ctrl_get_reference(vpx_codec_alg_priv_t *ctx,
                                          va_list args) {
  vp9_ref_frame_t *const frame = va_arg(args, vp9_ref_frame_t *);

  if (frame != NULL) {
    YV12_BUFFER_CONFIG *fb = get_ref_frame(&ctx->cpi->common, frame->idx);
    if (fb == NULL) return VPX_CODEC_ERROR;

    yuvconfig2image(&frame->img, fb, NULL);
    return VPX_CODEC_OK;
  }
  return VPX_CODEC_INVALID_PARAM;
}

static vpx_codec_err_t ctrl_set_previewpp(vpx_codec_alg_priv_t *ctx,
                                          va_list args) {
#if CONFIG_VP9_POSTPROC
  vp8_postproc_cfg_t *config = va_arg(args, vp8_postproc_cfg_t *);
  if (config != NULL) {
    ctx->preview_ppcfg = *config;
    return VPX_CODEC_OK;
  }
  return VPX_CODEC_INVALID_PARAM;
#else
  (void)ctx;
  (void)args;
  return VPX_CODEC_INCAPABLE;
#endif
}

static vpx_image_t *encoder_get_preview(vpx_codec_alg_priv_t *ctx) {
  YV12_BUFFER_CONFIG sd;
  vp9_ppflags_t flags;
  vp9_zero(flags);

  if (ctx->preview_ppcfg.post_proc_flag) {
    flags.post_proc_flag = ctx->preview_ppcfg.post_proc_flag;
    flags.deblocking_level = ctx->preview_ppcfg.deblocking_level;
    flags.noise_level = ctx->preview_ppcfg.noise_level;
  }

  if (vp9_get_preview_raw_frame(ctx->cpi, &sd, &flags) == 0) {
    yuvconfig2image(&ctx->preview_img, &sd, NULL);
    return &ctx->preview_img;
  }
  return NULL;
}

static vpx_codec_err_t ctrl_set_roi_map(vpx_codec_alg_priv_t *ctx,
                                        va_list args) {
  vpx_roi_map_t *data = va_arg(args, vpx_roi_map_t *);

  if (data) {
    vpx_roi_map_t *roi = (vpx_roi_map_t *)data;

    if (!vp9_set_roi_map(ctx->cpi, roi->roi_map, roi->rows, roi->cols,
                         roi->delta_q, roi->delta_lf, roi->skip,
                         roi->ref_frame)) {
      return VPX_CODEC_OK;
    }
    return VPX_CODEC_INVALID_PARAM;
  }
  return VPX_CODEC_INVALID_PARAM;
}

static vpx_codec_err_t ctrl_set_active_map(vpx_codec_alg_priv_t *ctx,
                                           va_list args) {
  vpx_active_map_t *const map = va_arg(args, vpx_active_map_t *);

  if (map) {
    if (!vp9_set_active_map(ctx->cpi, map->active_map, (int)map->rows,
                            (int)map->cols))
      return VPX_CODEC_OK;

    return VPX_CODEC_INVALID_PARAM;
  }
  return VPX_CODEC_INVALID_PARAM;
}

static vpx_codec_err_t ctrl_get_active_map(vpx_codec_alg_priv_t *ctx,
                                           va_list args) {
  vpx_active_map_t *const map = va_arg(args, vpx_active_map_t *);

  if (map) {
    if (!vp9_get_active_map(ctx->cpi, map->active_map, (int)map->rows,
                            (int)map->cols))
      return VPX_CODEC_OK;

    return VPX_CODEC_INVALID_PARAM;
  }
  return VPX_CODEC_INVALID_PARAM;
}

static vpx_codec_err_t ctrl_set_scale_mode(vpx_codec_alg_priv_t *ctx,
                                           va_list args) {
  vpx_scaling_mode_t *const mode = va_arg(args, vpx_scaling_mode_t *);

  if (mode) {
    const int res =
        vp9_set_internal_size(ctx->cpi, (VPX_SCALING)mode->h_scaling_mode,
                              (VPX_SCALING)mode->v_scaling_mode);
    return (res == 0) ? VPX_CODEC_OK : VPX_CODEC_INVALID_PARAM;
  }
  return VPX_CODEC_INVALID_PARAM;
}

static vpx_codec_err_t ctrl_set_svc(vpx_codec_alg_priv_t *ctx, va_list args) {
  int data = va_arg(args, int);
  const vpx_codec_enc_cfg_t *cfg = &ctx->cfg;
  // Both one-pass and two-pass RC are supported now.
  // User setting this has to make sure of the following.
  // In two-pass setting: either (but not both)
  //      cfg->ss_number_layers > 1, or cfg->ts_number_layers > 1
  // In one-pass setting:
  //      either or both cfg->ss_number_layers > 1, or cfg->ts_number_layers > 1

  vp9_set_svc(ctx->cpi, data);

  if (data == 1 &&
      (cfg->g_pass == VPX_RC_FIRST_PASS || cfg->g_pass == VPX_RC_LAST_PASS) &&
      cfg->ss_number_layers > 1 && cfg->ts_number_layers > 1) {
    return VPX_CODEC_INVALID_PARAM;
  }

  vp9_set_row_mt(ctx->cpi);

  return VPX_CODEC_OK;
}

static vpx_codec_err_t ctrl_set_svc_layer_id(vpx_codec_alg_priv_t *ctx,
                                             va_list args) {
  vpx_svc_layer_id_t *const data = va_arg(args, vpx_svc_layer_id_t *);
  VP9_COMP *const cpi = (VP9_COMP *)ctx->cpi;
  SVC *const svc = &cpi->svc;

  svc->first_spatial_layer_to_encode = data->spatial_layer_id;
  svc->spatial_layer_to_encode = data->spatial_layer_id;
  svc->temporal_layer_id = data->temporal_layer_id;
  // Checks on valid layer_id input.
  if (svc->temporal_layer_id < 0 ||
      svc->temporal_layer_id >= (int)ctx->cfg.ts_number_layers) {
    return VPX_CODEC_INVALID_PARAM;
  }
  if (svc->first_spatial_layer_to_encode < 0 ||
      svc->first_spatial_layer_to_encode >= (int)ctx->cfg.ss_number_layers) {
    return VPX_CODEC_INVALID_PARAM;
  }
  // First spatial layer to encode not implemented for two-pass.
  if (is_two_pass_svc(cpi) && svc->first_spatial_layer_to_encode > 0)
    return VPX_CODEC_INVALID_PARAM;
  return VPX_CODEC_OK;
}

static vpx_codec_err_t ctrl_get_svc_layer_id(vpx_codec_alg_priv_t *ctx,
                                             va_list args) {
  vpx_svc_layer_id_t *data = va_arg(args, vpx_svc_layer_id_t *);
  VP9_COMP *const cpi = (VP9_COMP *)ctx->cpi;
  SVC *const svc = &cpi->svc;

  data->spatial_layer_id = svc->spatial_layer_id;
  data->temporal_layer_id = svc->temporal_layer_id;

  return VPX_CODEC_OK;
}

static vpx_codec_err_t ctrl_set_svc_parameters(vpx_codec_alg_priv_t *ctx,
                                               va_list args) {
  VP9_COMP *const cpi = ctx->cpi;
  vpx_svc_extra_cfg_t *const params = va_arg(args, vpx_svc_extra_cfg_t *);
  int sl, tl;

  // Number of temporal layers and number of spatial layers have to be set
  // properly before calling this control function.
  for (sl = 0; sl < cpi->svc.number_spatial_layers; ++sl) {
    for (tl = 0; tl < cpi->svc.number_temporal_layers; ++tl) {
      const int layer =
          LAYER_IDS_TO_IDX(sl, tl, cpi->svc.number_temporal_layers);
      LAYER_CONTEXT *lc = &cpi->svc.layer_context[layer];
      lc->max_q = params->max_quantizers[layer];
      lc->min_q = params->min_quantizers[layer];
      lc->scaling_factor_num = params->scaling_factor_num[sl];
      lc->scaling_factor_den = params->scaling_factor_den[sl];
      lc->speed = params->speed_per_layer[sl];
    }
  }

  return VPX_CODEC_OK;
}

static vpx_codec_err_t ctrl_set_svc_ref_frame_config(vpx_codec_alg_priv_t *ctx,
                                                     va_list args) {
  VP9_COMP *const cpi = ctx->cpi;
  vpx_svc_ref_frame_config_t *data = va_arg(args, vpx_svc_ref_frame_config_t *);
  int sl;
  for (sl = 0; sl < cpi->svc.number_spatial_layers; ++sl) {
    cpi->svc.ext_frame_flags[sl] = data->frame_flags[sl];
    cpi->svc.ext_lst_fb_idx[sl] = data->lst_fb_idx[sl];
    cpi->svc.ext_gld_fb_idx[sl] = data->gld_fb_idx[sl];
    cpi->svc.ext_alt_fb_idx[sl] = data->alt_fb_idx[sl];
  }
  return VPX_CODEC_OK;
}

static vpx_codec_err_t ctrl_register_cx_callback(vpx_codec_alg_priv_t *ctx,
                                                 va_list args) {
  vpx_codec_priv_output_cx_pkt_cb_pair_t *cbp =
      (vpx_codec_priv_output_cx_pkt_cb_pair_t *)va_arg(args, void *);
  ctx->output_cx_pkt_cb.output_cx_pkt = cbp->output_cx_pkt;
  ctx->output_cx_pkt_cb.user_priv = cbp->user_priv;

  return VPX_CODEC_OK;
}

static vpx_codec_err_t ctrl_set_tune_content(vpx_codec_alg_priv_t *ctx,
                                             va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.content = CAST(VP9E_SET_TUNE_CONTENT, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_color_space(vpx_codec_alg_priv_t *ctx,
                                            va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.color_space = CAST(VP9E_SET_COLOR_SPACE, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_color_range(vpx_codec_alg_priv_t *ctx,
                                            va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.color_range = CAST(VP9E_SET_COLOR_RANGE, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_err_t ctrl_set_render_size(vpx_codec_alg_priv_t *ctx,
                                            va_list args) {
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;
  int *const render_size = va_arg(args, int *);
  extra_cfg.render_width = render_size[0];
  extra_cfg.render_height = render_size[1];
  return update_extra_cfg(ctx, &extra_cfg);
}

static vpx_codec_ctrl_fn_map_t encoder_ctrl_maps[] = {
  { VP8_COPY_REFERENCE, ctrl_copy_reference },

  // Setters
  { VP8_SET_REFERENCE, ctrl_set_reference },
  { VP8_SET_POSTPROC, ctrl_set_previewpp },
  { VP9E_SET_ROI_MAP, ctrl_set_roi_map },
  { VP8E_SET_ACTIVEMAP, ctrl_set_active_map },
  { VP8E_SET_SCALEMODE, ctrl_set_scale_mode },
  { VP8E_SET_CPUUSED, ctrl_set_cpuused },
  { VP8E_SET_ENABLEAUTOALTREF, ctrl_set_enable_auto_alt_ref },
  { VP8E_SET_SHARPNESS, ctrl_set_sharpness },
  { VP8E_SET_STATIC_THRESHOLD, ctrl_set_static_thresh },
  { VP9E_SET_TILE_COLUMNS, ctrl_set_tile_columns },
  { VP9E_SET_TILE_ROWS, ctrl_set_tile_rows },
  { VP8E_SET_ARNR_MAXFRAMES, ctrl_set_arnr_max_frames },
  { VP8E_SET_ARNR_STRENGTH, ctrl_set_arnr_strength },
  { VP8E_SET_ARNR_TYPE, ctrl_set_arnr_type },
  { VP8E_SET_TUNING, ctrl_set_tuning },
  { VP8E_SET_CQ_LEVEL, ctrl_set_cq_level },
  { VP8E_SET_MAX_INTRA_BITRATE_PCT, ctrl_set_rc_max_intra_bitrate_pct },
  { VP9E_SET_MAX_INTER_BITRATE_PCT, ctrl_set_rc_max_inter_bitrate_pct },
  { VP9E_SET_GF_CBR_BOOST_PCT, ctrl_set_rc_gf_cbr_boost_pct },
  { VP9E_SET_LOSSLESS, ctrl_set_lossless },
  { VP9E_SET_FRAME_PARALLEL_DECODING, ctrl_set_frame_parallel_decoding_mode },
  { VP9E_SET_AQ_MODE, ctrl_set_aq_mode },
  { VP9E_SET_ALT_REF_AQ, ctrl_set_alt_ref_aq },
  { VP9E_SET_FRAME_PERIODIC_BOOST, ctrl_set_frame_periodic_boost },
  { VP9E_SET_SVC, ctrl_set_svc },
  { VP9E_SET_SVC_PARAMETERS, ctrl_set_svc_parameters },
  { VP9E_REGISTER_CX_CALLBACK, ctrl_register_cx_callback },
  { VP9E_SET_SVC_LAYER_ID, ctrl_set_svc_layer_id },
  { VP9E_SET_TUNE_CONTENT, ctrl_set_tune_content },
  { VP9E_SET_COLOR_SPACE, ctrl_set_color_space },
  { VP9E_SET_COLOR_RANGE, ctrl_set_color_range },
  { VP9E_SET_NOISE_SENSITIVITY, ctrl_set_noise_sensitivity },
  { VP9E_SET_MIN_GF_INTERVAL, ctrl_set_min_gf_interval },
  { VP9E_SET_MAX_GF_INTERVAL, ctrl_set_max_gf_interval },
  { VP9E_SET_SVC_REF_FRAME_CONFIG, ctrl_set_svc_ref_frame_config },
  { VP9E_SET_RENDER_SIZE, ctrl_set_render_size },
  { VP9E_SET_TARGET_LEVEL, ctrl_set_target_level },
  { VP9E_SET_ROW_MT, ctrl_set_row_mt },
  { VP9E_ENABLE_MOTION_VECTOR_UNIT_TEST, ctrl_enable_motion_vector_unit_test },

  // Getters
  { VP8E_GET_LAST_QUANTIZER, ctrl_get_quantizer },
  { VP8E_GET_LAST_QUANTIZER_64, ctrl_get_quantizer64 },
  { VP9_GET_REFERENCE, ctrl_get_reference },
  { VP9E_GET_SVC_LAYER_ID, ctrl_get_svc_layer_id },
  { VP9E_GET_ACTIVEMAP, ctrl_get_active_map },
  { VP9E_GET_LEVEL, ctrl_get_level },

  { -1, NULL },
};

static vpx_codec_enc_cfg_map_t encoder_usage_cfg_map[] = {
  { 0,
    {
        // NOLINT
        0,  // g_usage
        8,  // g_threads
        0,  // g_profile

        320,         // g_width
        240,         // g_height
        VPX_BITS_8,  // g_bit_depth
        8,           // g_input_bit_depth

        { 1, 30 },  // g_timebase

        0,  // g_error_resilient

        VPX_RC_ONE_PASS,  // g_pass

        25,  // g_lag_in_frames

        0,   // rc_dropframe_thresh
        0,   // rc_resize_allowed
        0,   // rc_scaled_width
        0,   // rc_scaled_height
        60,  // rc_resize_down_thresold
        30,  // rc_resize_up_thresold

        VPX_VBR,      // rc_end_usage
        { NULL, 0 },  // rc_twopass_stats_in
        { NULL, 0 },  // rc_firstpass_mb_stats_in
        256,          // rc_target_bandwidth
        0,            // rc_min_quantizer
        63,           // rc_max_quantizer
        25,           // rc_undershoot_pct
        25,           // rc_overshoot_pct

        6000,  // rc_max_buffer_size
        4000,  // rc_buffer_initial_size
        5000,  // rc_buffer_optimal_size

        50,    // rc_two_pass_vbrbias
        0,     // rc_two_pass_vbrmin_section
        2000,  // rc_two_pass_vbrmax_section
        0,     // rc_2pass_vbr_corpus_complexity (non 0 for corpus vbr)

        // keyframing settings (kf)
        VPX_KF_AUTO,  // g_kfmode
        0,            // kf_min_dist
        128,          // kf_max_dist

        VPX_SS_DEFAULT_LAYERS,  // ss_number_layers
        { 0 },
        { 0 },  // ss_target_bitrate
        1,      // ts_number_layers
        { 0 },  // ts_target_bitrate
        { 0 },  // ts_rate_decimator
        0,      // ts_periodicity
        { 0 },  // ts_layer_id
        { 0 },  // layer_taget_bitrate
        0       // temporal_layering_mode
    } },
};

#ifndef VERSION_STRING
#define VERSION_STRING
#endif
CODEC_INTERFACE(vpx_codec_vp9_cx) = {
  "WebM Project VP9 Encoder" VERSION_STRING,
  VPX_CODEC_INTERNAL_ABI_VERSION,
#if CONFIG_VP9_HIGHBITDEPTH
  VPX_CODEC_CAP_HIGHBITDEPTH |
#endif
      VPX_CODEC_CAP_ENCODER | VPX_CODEC_CAP_PSNR,  // vpx_codec_caps_t
  encoder_init,                                    // vpx_codec_init_fn_t
  encoder_destroy,                                 // vpx_codec_destroy_fn_t
  encoder_ctrl_maps,                               // vpx_codec_ctrl_fn_map_t
  {
      // NOLINT
      NULL,  // vpx_codec_peek_si_fn_t
      NULL,  // vpx_codec_get_si_fn_t
      NULL,  // vpx_codec_decode_fn_t
      NULL,  // vpx_codec_frame_get_fn_t
      NULL   // vpx_codec_set_fb_fn_t
  },
  {
      // NOLINT
      1,                      // 1 cfg map
      encoder_usage_cfg_map,  // vpx_codec_enc_cfg_map_t
      encoder_encode,         // vpx_codec_encode_fn_t
      encoder_get_cxdata,     // vpx_codec_get_cx_data_fn_t
      encoder_set_config,     // vpx_codec_enc_config_set_fn_t
      NULL,                   // vpx_codec_get_global_headers_fn_t
      encoder_get_preview,    // vpx_codec_get_preview_frame_fn_t
      NULL                    // vpx_codec_enc_mr_get_mem_loc_fn_t
  }
};
