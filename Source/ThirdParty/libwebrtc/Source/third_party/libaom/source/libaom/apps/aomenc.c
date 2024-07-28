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

#include "apps/aomenc.h"

#include "config/aom_config.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if CONFIG_AV1_DECODER
#include "aom/aom_decoder.h"
#include "aom/aomdx.h"
#endif

#include "aom/aom_encoder.h"
#include "aom/aom_integer.h"
#include "aom/aomcx.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/mem_ops.h"
#include "common/args.h"
#include "common/ivfenc.h"
#include "common/tools_common.h"
#include "common/warnings.h"

#if CONFIG_WEBM_IO
#include "common/webmenc.h"
#endif

#include "common/y4minput.h"
#include "examples/encoder_util.h"
#include "stats/aomstats.h"
#include "stats/rate_hist.h"

#if CONFIG_LIBYUV
#include "third_party/libyuv/include/libyuv/scale.h"
#endif

/* Swallow warnings about unused results of fread/fwrite */
static size_t wrap_fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  return fread(ptr, size, nmemb, stream);
}
#define fread wrap_fread

static size_t wrap_fwrite(const void *ptr, size_t size, size_t nmemb,
                          FILE *stream) {
  return fwrite(ptr, size, nmemb, stream);
}
#define fwrite wrap_fwrite

static const char *exec_name;

static AOM_TOOLS_FORMAT_PRINTF(3, 0) void warn_or_exit_on_errorv(
    aom_codec_ctx_t *ctx, int fatal, const char *s, va_list ap) {
  if (ctx->err) {
    const char *detail = aom_codec_error_detail(ctx);

    vfprintf(stderr, s, ap);
    fprintf(stderr, ": %s\n", aom_codec_error(ctx));

    if (detail) fprintf(stderr, "    %s\n", detail);

    if (fatal) {
      aom_codec_destroy(ctx);
      exit(EXIT_FAILURE);
    }
  }
}

static AOM_TOOLS_FORMAT_PRINTF(2,
                               3) void ctx_exit_on_error(aom_codec_ctx_t *ctx,
                                                         const char *s, ...) {
  va_list ap;

  va_start(ap, s);
  warn_or_exit_on_errorv(ctx, 1, s, ap);
  va_end(ap);
}

static AOM_TOOLS_FORMAT_PRINTF(3, 4) void warn_or_exit_on_error(
    aom_codec_ctx_t *ctx, int fatal, const char *s, ...) {
  va_list ap;

  va_start(ap, s);
  warn_or_exit_on_errorv(ctx, fatal, s, ap);
  va_end(ap);
}

static int read_frame(struct AvxInputContext *input_ctx, aom_image_t *img) {
  FILE *f = input_ctx->file;
  y4m_input *y4m = &input_ctx->y4m;
  int shortread = 0;

  if (input_ctx->file_type == FILE_TYPE_Y4M) {
    if (y4m_input_fetch_frame(y4m, f, img) < 1) return 0;
  } else {
    shortread = read_yuv_frame(input_ctx, img);
  }

  return !shortread;
}

static int file_is_y4m(const char detect[4]) {
  if (memcmp(detect, "YUV4", 4) == 0) {
    return 1;
  }
  return 0;
}

static int fourcc_is_ivf(const char detect[4]) {
  if (memcmp(detect, "DKIF", 4) == 0) {
    return 1;
  }
  return 0;
}

static const int av1_arg_ctrl_map[] = { AOME_SET_CPUUSED,
                                        AOME_SET_ENABLEAUTOALTREF,
                                        AOME_SET_SHARPNESS,
                                        AOME_SET_STATIC_THRESHOLD,
                                        AV1E_SET_ROW_MT,
                                        AV1E_SET_FP_MT,
                                        AV1E_SET_TILE_COLUMNS,
                                        AV1E_SET_TILE_ROWS,
                                        AV1E_SET_ENABLE_TPL_MODEL,
                                        AV1E_SET_ENABLE_KEYFRAME_FILTERING,
                                        AOME_SET_ARNR_MAXFRAMES,
                                        AOME_SET_ARNR_STRENGTH,
                                        AOME_SET_TUNING,
                                        AOME_SET_CQ_LEVEL,
                                        AOME_SET_MAX_INTRA_BITRATE_PCT,
                                        AV1E_SET_MAX_INTER_BITRATE_PCT,
                                        AV1E_SET_GF_CBR_BOOST_PCT,
                                        AV1E_SET_LOSSLESS,
                                        AV1E_SET_ENABLE_CDEF,
                                        AV1E_SET_ENABLE_RESTORATION,
                                        AV1E_SET_ENABLE_RECT_PARTITIONS,
                                        AV1E_SET_ENABLE_AB_PARTITIONS,
                                        AV1E_SET_ENABLE_1TO4_PARTITIONS,
                                        AV1E_SET_MIN_PARTITION_SIZE,
                                        AV1E_SET_MAX_PARTITION_SIZE,
                                        AV1E_SET_ENABLE_DUAL_FILTER,
                                        AV1E_SET_ENABLE_CHROMA_DELTAQ,
                                        AV1E_SET_ENABLE_INTRA_EDGE_FILTER,
                                        AV1E_SET_ENABLE_ORDER_HINT,
                                        AV1E_SET_ENABLE_TX64,
                                        AV1E_SET_ENABLE_FLIP_IDTX,
                                        AV1E_SET_ENABLE_RECT_TX,
                                        AV1E_SET_ENABLE_DIST_WTD_COMP,
                                        AV1E_SET_ENABLE_MASKED_COMP,
                                        AV1E_SET_ENABLE_ONESIDED_COMP,
                                        AV1E_SET_ENABLE_INTERINTRA_COMP,
                                        AV1E_SET_ENABLE_SMOOTH_INTERINTRA,
                                        AV1E_SET_ENABLE_DIFF_WTD_COMP,
                                        AV1E_SET_ENABLE_INTERINTER_WEDGE,
                                        AV1E_SET_ENABLE_INTERINTRA_WEDGE,
                                        AV1E_SET_ENABLE_GLOBAL_MOTION,
                                        AV1E_SET_ENABLE_WARPED_MOTION,
                                        AV1E_SET_ENABLE_FILTER_INTRA,
                                        AV1E_SET_ENABLE_SMOOTH_INTRA,
                                        AV1E_SET_ENABLE_PAETH_INTRA,
                                        AV1E_SET_ENABLE_CFL_INTRA,
                                        AV1E_SET_ENABLE_DIAGONAL_INTRA,
                                        AV1E_SET_FORCE_VIDEO_MODE,
                                        AV1E_SET_ENABLE_OBMC,
                                        AV1E_SET_ENABLE_OVERLAY,
                                        AV1E_SET_ENABLE_PALETTE,
                                        AV1E_SET_ENABLE_INTRABC,
                                        AV1E_SET_ENABLE_ANGLE_DELTA,
                                        AV1E_SET_DISABLE_TRELLIS_QUANT,
                                        AV1E_SET_ENABLE_QM,
                                        AV1E_SET_QM_MIN,
                                        AV1E_SET_QM_MAX,
                                        AV1E_SET_REDUCED_TX_TYPE_SET,
                                        AV1E_SET_INTRA_DCT_ONLY,
                                        AV1E_SET_INTER_DCT_ONLY,
                                        AV1E_SET_INTRA_DEFAULT_TX_ONLY,
                                        AV1E_SET_QUANT_B_ADAPT,
                                        AV1E_SET_COEFF_COST_UPD_FREQ,
                                        AV1E_SET_MODE_COST_UPD_FREQ,
                                        AV1E_SET_MV_COST_UPD_FREQ,
                                        AV1E_SET_FRAME_PARALLEL_DECODING,
                                        AV1E_SET_ERROR_RESILIENT_MODE,
                                        AV1E_SET_AQ_MODE,
                                        AV1E_SET_DELTAQ_MODE,
                                        AV1E_SET_DELTAQ_STRENGTH,
                                        AV1E_SET_DELTALF_MODE,
                                        AV1E_SET_FRAME_PERIODIC_BOOST,
                                        AV1E_SET_NOISE_SENSITIVITY,
                                        AV1E_SET_TUNE_CONTENT,
                                        AV1E_SET_CDF_UPDATE_MODE,
                                        AV1E_SET_COLOR_PRIMARIES,
                                        AV1E_SET_TRANSFER_CHARACTERISTICS,
                                        AV1E_SET_MATRIX_COEFFICIENTS,
                                        AV1E_SET_CHROMA_SAMPLE_POSITION,
                                        AV1E_SET_MIN_GF_INTERVAL,
                                        AV1E_SET_MAX_GF_INTERVAL,
                                        AV1E_SET_GF_MIN_PYRAMID_HEIGHT,
                                        AV1E_SET_GF_MAX_PYRAMID_HEIGHT,
                                        AV1E_SET_SUPERBLOCK_SIZE,
                                        AV1E_SET_NUM_TG,
                                        AV1E_SET_MTU,
                                        AV1E_SET_TIMING_INFO_TYPE,
                                        AV1E_SET_FILM_GRAIN_TEST_VECTOR,
                                        AV1E_SET_FILM_GRAIN_TABLE,
#if CONFIG_DENOISE
                                        AV1E_SET_DENOISE_NOISE_LEVEL,
                                        AV1E_SET_DENOISE_BLOCK_SIZE,
                                        AV1E_SET_ENABLE_DNL_DENOISING,
#endif  // CONFIG_DENOISE
                                        AV1E_SET_MAX_REFERENCE_FRAMES,
                                        AV1E_SET_REDUCED_REFERENCE_SET,
                                        AV1E_SET_ENABLE_REF_FRAME_MVS,
                                        AV1E_SET_TARGET_SEQ_LEVEL_IDX,
                                        AV1E_SET_TIER_MASK,
                                        AV1E_SET_MIN_CR,
                                        AV1E_SET_VBR_CORPUS_COMPLEXITY_LAP,
                                        AV1E_SET_CHROMA_SUBSAMPLING_X,
                                        AV1E_SET_CHROMA_SUBSAMPLING_Y,
#if CONFIG_TUNE_VMAF
                                        AV1E_SET_VMAF_MODEL_PATH,
#endif
                                        AV1E_SET_DV_COST_UPD_FREQ,
                                        AV1E_SET_PARTITION_INFO_PATH,
                                        AV1E_SET_ENABLE_DIRECTIONAL_INTRA,
                                        AV1E_SET_ENABLE_TX_SIZE_SEARCH,
                                        AV1E_SET_LOOPFILTER_CONTROL,
                                        AV1E_SET_AUTO_INTRA_TOOLS_OFF,
                                        AV1E_ENABLE_RATE_GUIDE_DELTAQ,
                                        AV1E_SET_RATE_DISTRIBUTION_INFO,
                                        0 };

static const arg_def_t *const main_args[] = {
  &g_av1_codec_arg_defs.help,
  &g_av1_codec_arg_defs.use_cfg,
  &g_av1_codec_arg_defs.debugmode,
  &g_av1_codec_arg_defs.outputfile,
  &g_av1_codec_arg_defs.codecarg,
  &g_av1_codec_arg_defs.passes,
  &g_av1_codec_arg_defs.pass_arg,
  &g_av1_codec_arg_defs.fpf_name,
  &g_av1_codec_arg_defs.limit,
  &g_av1_codec_arg_defs.skip,
  &g_av1_codec_arg_defs.good_dl,
  &g_av1_codec_arg_defs.rt_dl,
  &g_av1_codec_arg_defs.ai_dl,
  &g_av1_codec_arg_defs.quietarg,
  &g_av1_codec_arg_defs.verbosearg,
  &g_av1_codec_arg_defs.psnrarg,
  &g_av1_codec_arg_defs.use_webm,
  &g_av1_codec_arg_defs.use_ivf,
  &g_av1_codec_arg_defs.use_obu,
  &g_av1_codec_arg_defs.q_hist_n,
  &g_av1_codec_arg_defs.rate_hist_n,
  &g_av1_codec_arg_defs.disable_warnings,
  &g_av1_codec_arg_defs.disable_warning_prompt,
  &g_av1_codec_arg_defs.recontest,
  NULL
};

static const arg_def_t *const global_args[] = {
  &g_av1_codec_arg_defs.use_nv12,
  &g_av1_codec_arg_defs.use_yv12,
  &g_av1_codec_arg_defs.use_i420,
  &g_av1_codec_arg_defs.use_i422,
  &g_av1_codec_arg_defs.use_i444,
  &g_av1_codec_arg_defs.usage,
  &g_av1_codec_arg_defs.threads,
  &g_av1_codec_arg_defs.profile,
  &g_av1_codec_arg_defs.width,
  &g_av1_codec_arg_defs.height,
  &g_av1_codec_arg_defs.forced_max_frame_width,
  &g_av1_codec_arg_defs.forced_max_frame_height,
#if CONFIG_WEBM_IO
  &g_av1_codec_arg_defs.stereo_mode,
#endif
  &g_av1_codec_arg_defs.timebase,
  &g_av1_codec_arg_defs.framerate,
  &g_av1_codec_arg_defs.global_error_resilient,
  &g_av1_codec_arg_defs.bitdeptharg,
  &g_av1_codec_arg_defs.inbitdeptharg,
  &g_av1_codec_arg_defs.lag_in_frames,
  &g_av1_codec_arg_defs.large_scale_tile,
  &g_av1_codec_arg_defs.monochrome,
  &g_av1_codec_arg_defs.full_still_picture_hdr,
  &g_av1_codec_arg_defs.use_16bit_internal,
  &g_av1_codec_arg_defs.save_as_annexb,
  NULL
};

static const arg_def_t *const rc_args[] = {
  &g_av1_codec_arg_defs.dropframe_thresh,
  &g_av1_codec_arg_defs.resize_mode,
  &g_av1_codec_arg_defs.resize_denominator,
  &g_av1_codec_arg_defs.resize_kf_denominator,
  &g_av1_codec_arg_defs.superres_mode,
  &g_av1_codec_arg_defs.superres_denominator,
  &g_av1_codec_arg_defs.superres_kf_denominator,
  &g_av1_codec_arg_defs.superres_qthresh,
  &g_av1_codec_arg_defs.superres_kf_qthresh,
  &g_av1_codec_arg_defs.end_usage,
  &g_av1_codec_arg_defs.target_bitrate,
  &g_av1_codec_arg_defs.min_quantizer,
  &g_av1_codec_arg_defs.max_quantizer,
  &g_av1_codec_arg_defs.undershoot_pct,
  &g_av1_codec_arg_defs.overshoot_pct,
  &g_av1_codec_arg_defs.buf_sz,
  &g_av1_codec_arg_defs.buf_initial_sz,
  &g_av1_codec_arg_defs.buf_optimal_sz,
  &g_av1_codec_arg_defs.bias_pct,
  &g_av1_codec_arg_defs.minsection_pct,
  &g_av1_codec_arg_defs.maxsection_pct,
  NULL
};

static const arg_def_t *const kf_args[] = {
  &g_av1_codec_arg_defs.fwd_kf_enabled,
  &g_av1_codec_arg_defs.kf_min_dist,
  &g_av1_codec_arg_defs.kf_max_dist,
  &g_av1_codec_arg_defs.kf_disabled,
  &g_av1_codec_arg_defs.sframe_dist,
  &g_av1_codec_arg_defs.sframe_mode,
  NULL
};

// TODO(bohanli): Currently all options are supported by the key & value API.
// Consider removing the control ID usages?
static const arg_def_t *const av1_ctrl_args[] = {
  &g_av1_codec_arg_defs.cpu_used_av1,
  &g_av1_codec_arg_defs.auto_altref,
  &g_av1_codec_arg_defs.sharpness,
  &g_av1_codec_arg_defs.static_thresh,
  &g_av1_codec_arg_defs.rowmtarg,
  &g_av1_codec_arg_defs.fpmtarg,
  &g_av1_codec_arg_defs.tile_cols,
  &g_av1_codec_arg_defs.tile_rows,
  &g_av1_codec_arg_defs.enable_tpl_model,
  &g_av1_codec_arg_defs.enable_keyframe_filtering,
  &g_av1_codec_arg_defs.arnr_maxframes,
  &g_av1_codec_arg_defs.arnr_strength,
  &g_av1_codec_arg_defs.tune_metric,
  &g_av1_codec_arg_defs.cq_level,
  &g_av1_codec_arg_defs.max_intra_rate_pct,
  &g_av1_codec_arg_defs.max_inter_rate_pct,
  &g_av1_codec_arg_defs.gf_cbr_boost_pct,
  &g_av1_codec_arg_defs.lossless,
  &g_av1_codec_arg_defs.enable_cdef,
  &g_av1_codec_arg_defs.enable_restoration,
  &g_av1_codec_arg_defs.enable_rect_partitions,
  &g_av1_codec_arg_defs.enable_ab_partitions,
  &g_av1_codec_arg_defs.enable_1to4_partitions,
  &g_av1_codec_arg_defs.min_partition_size,
  &g_av1_codec_arg_defs.max_partition_size,
  &g_av1_codec_arg_defs.enable_dual_filter,
  &g_av1_codec_arg_defs.enable_chroma_deltaq,
  &g_av1_codec_arg_defs.enable_intra_edge_filter,
  &g_av1_codec_arg_defs.enable_order_hint,
  &g_av1_codec_arg_defs.enable_tx64,
  &g_av1_codec_arg_defs.enable_flip_idtx,
  &g_av1_codec_arg_defs.enable_rect_tx,
  &g_av1_codec_arg_defs.enable_dist_wtd_comp,
  &g_av1_codec_arg_defs.enable_masked_comp,
  &g_av1_codec_arg_defs.enable_onesided_comp,
  &g_av1_codec_arg_defs.enable_interintra_comp,
  &g_av1_codec_arg_defs.enable_smooth_interintra,
  &g_av1_codec_arg_defs.enable_diff_wtd_comp,
  &g_av1_codec_arg_defs.enable_interinter_wedge,
  &g_av1_codec_arg_defs.enable_interintra_wedge,
  &g_av1_codec_arg_defs.enable_global_motion,
  &g_av1_codec_arg_defs.enable_warped_motion,
  &g_av1_codec_arg_defs.enable_filter_intra,
  &g_av1_codec_arg_defs.enable_smooth_intra,
  &g_av1_codec_arg_defs.enable_paeth_intra,
  &g_av1_codec_arg_defs.enable_cfl_intra,
  &g_av1_codec_arg_defs.enable_diagonal_intra,
  &g_av1_codec_arg_defs.force_video_mode,
  &g_av1_codec_arg_defs.enable_obmc,
  &g_av1_codec_arg_defs.enable_overlay,
  &g_av1_codec_arg_defs.enable_palette,
  &g_av1_codec_arg_defs.enable_intrabc,
  &g_av1_codec_arg_defs.enable_angle_delta,
  &g_av1_codec_arg_defs.disable_trellis_quant,
  &g_av1_codec_arg_defs.enable_qm,
  &g_av1_codec_arg_defs.qm_min,
  &g_av1_codec_arg_defs.qm_max,
  &g_av1_codec_arg_defs.reduced_tx_type_set,
  &g_av1_codec_arg_defs.use_intra_dct_only,
  &g_av1_codec_arg_defs.use_inter_dct_only,
  &g_av1_codec_arg_defs.use_intra_default_tx_only,
  &g_av1_codec_arg_defs.quant_b_adapt,
  &g_av1_codec_arg_defs.coeff_cost_upd_freq,
  &g_av1_codec_arg_defs.mode_cost_upd_freq,
  &g_av1_codec_arg_defs.mv_cost_upd_freq,
  &g_av1_codec_arg_defs.frame_parallel_decoding,
  &g_av1_codec_arg_defs.error_resilient_mode,
  &g_av1_codec_arg_defs.aq_mode,
  &g_av1_codec_arg_defs.deltaq_mode,
  &g_av1_codec_arg_defs.deltaq_strength,
  &g_av1_codec_arg_defs.deltalf_mode,
  &g_av1_codec_arg_defs.frame_periodic_boost,
  &g_av1_codec_arg_defs.noise_sens,
  &g_av1_codec_arg_defs.tune_content,
  &g_av1_codec_arg_defs.cdf_update_mode,
  &g_av1_codec_arg_defs.input_color_primaries,
  &g_av1_codec_arg_defs.input_transfer_characteristics,
  &g_av1_codec_arg_defs.input_matrix_coefficients,
  &g_av1_codec_arg_defs.input_chroma_sample_position,
  &g_av1_codec_arg_defs.min_gf_interval,
  &g_av1_codec_arg_defs.max_gf_interval,
  &g_av1_codec_arg_defs.gf_min_pyr_height,
  &g_av1_codec_arg_defs.gf_max_pyr_height,
  &g_av1_codec_arg_defs.superblock_size,
  &g_av1_codec_arg_defs.num_tg,
  &g_av1_codec_arg_defs.mtu_size,
  &g_av1_codec_arg_defs.timing_info,
  &g_av1_codec_arg_defs.film_grain_test,
  &g_av1_codec_arg_defs.film_grain_table,
#if CONFIG_DENOISE
  &g_av1_codec_arg_defs.denoise_noise_level,
  &g_av1_codec_arg_defs.denoise_block_size,
  &g_av1_codec_arg_defs.enable_dnl_denoising,
#endif  // CONFIG_DENOISE
  &g_av1_codec_arg_defs.max_reference_frames,
  &g_av1_codec_arg_defs.reduced_reference_set,
  &g_av1_codec_arg_defs.enable_ref_frame_mvs,
  &g_av1_codec_arg_defs.target_seq_level_idx,
  &g_av1_codec_arg_defs.set_tier_mask,
  &g_av1_codec_arg_defs.set_min_cr,
  &g_av1_codec_arg_defs.vbr_corpus_complexity_lap,
  &g_av1_codec_arg_defs.input_chroma_subsampling_x,
  &g_av1_codec_arg_defs.input_chroma_subsampling_y,
#if CONFIG_TUNE_VMAF
  &g_av1_codec_arg_defs.vmaf_model_path,
#endif
  &g_av1_codec_arg_defs.dv_cost_upd_freq,
  &g_av1_codec_arg_defs.partition_info_path,
  &g_av1_codec_arg_defs.enable_directional_intra,
  &g_av1_codec_arg_defs.enable_tx_size_search,
  &g_av1_codec_arg_defs.loopfilter_control,
  &g_av1_codec_arg_defs.auto_intra_tools_off,
  &g_av1_codec_arg_defs.enable_rate_guide_deltaq,
  &g_av1_codec_arg_defs.rate_distribution_info,
  NULL,
};

static const arg_def_t *const av1_key_val_args[] = {
  &g_av1_codec_arg_defs.passes,
  &g_av1_codec_arg_defs.two_pass_output,
  &g_av1_codec_arg_defs.second_pass_log,
  &g_av1_codec_arg_defs.fwd_kf_dist,
  &g_av1_codec_arg_defs.strict_level_conformance,
  &g_av1_codec_arg_defs.sb_qp_sweep,
  &g_av1_codec_arg_defs.dist_metric,
  &g_av1_codec_arg_defs.kf_max_pyr_height,
  &g_av1_codec_arg_defs.auto_tiles,
  NULL,
};

static const arg_def_t *const no_args[] = { NULL };

static void show_help(FILE *fout, int shorthelp) {
  fprintf(fout, "Usage: %s <options> -o dst_filename src_filename\n",
          exec_name);

  if (shorthelp) {
    fprintf(fout, "Use --help to see the full list of options.\n");
    return;
  }

  fprintf(fout, "\nOptions:\n");
  arg_show_usage(fout, main_args);
  fprintf(fout, "\nEncoder Global Options:\n");
  arg_show_usage(fout, global_args);
  fprintf(fout, "\nRate Control Options:\n");
  arg_show_usage(fout, rc_args);
  fprintf(fout, "\nKeyframe Placement Options:\n");
  arg_show_usage(fout, kf_args);
#if CONFIG_AV1_ENCODER
  fprintf(fout, "\nAV1 Specific Options:\n");
  arg_show_usage(fout, av1_ctrl_args);
  arg_show_usage(fout, av1_key_val_args);
#endif
  fprintf(fout,
          "\nStream timebase (--timebase):\n"
          "  The desired precision of timestamps in the output, expressed\n"
          "  in fractional seconds. Default is 1/1000.\n");
  fprintf(fout, "\nIncluded encoders:\n\n");

  const int num_encoder = get_aom_encoder_count();
  for (int i = 0; i < num_encoder; ++i) {
    aom_codec_iface_t *encoder = get_aom_encoder_by_index(i);
    const char *defstr = (i == (num_encoder - 1)) ? "(default)" : "";
    fprintf(fout, "    %-6s - %s %s\n", get_short_name_by_aom_encoder(encoder),
            aom_codec_iface_name(encoder), defstr);
  }
  fprintf(fout, "\n        ");
  fprintf(fout, "Use --codec to switch to a non-default encoder.\n\n");
}

void usage_exit(void) {
  show_help(stderr, 1);
  exit(EXIT_FAILURE);
}

#if CONFIG_AV1_ENCODER
#define ARG_CTRL_CNT_MAX NELEMENTS(av1_arg_ctrl_map)
#define ARG_KEY_VAL_CNT_MAX NELEMENTS(av1_key_val_args)
#endif

#if !CONFIG_WEBM_IO
typedef int stereo_format_t;
struct WebmOutputContext {
  int debug;
};
#endif

/* Per-stream configuration */
struct stream_config {
  struct aom_codec_enc_cfg cfg;
  const char *out_fn;
  const char *stats_fn;
  stereo_format_t stereo_fmt;
  int arg_ctrls[ARG_CTRL_CNT_MAX][2];
  int arg_ctrl_cnt;
  const char *arg_key_vals[ARG_KEY_VAL_CNT_MAX][2];
  int arg_key_val_cnt;
  int write_webm;
  const char *film_grain_filename;
  int write_ivf;
  // whether to use 16bit internal buffers
  int use_16bit_internal;
#if CONFIG_TUNE_VMAF
  const char *vmaf_model_path;
#endif
  const char *partition_info_path;
  unsigned int enable_rate_guide_deltaq;
  const char *rate_distribution_info;
  aom_color_range_t color_range;
  const char *two_pass_input;
  const char *two_pass_output;
  int two_pass_width;
  int two_pass_height;
};

struct stream_state {
  int index;
  struct stream_state *next;
  struct stream_config config;
  FILE *file;
  struct rate_hist *rate_hist;
  struct WebmOutputContext webm_ctx;
  uint64_t psnr_sse_total[2];
  uint64_t psnr_samples_total[2];
  double psnr_totals[2][4];
  int psnr_count[2];
  int counts[64];
  aom_codec_ctx_t encoder;
  unsigned int frames_out;
  uint64_t cx_time;
  size_t nbytes;
  stats_io_t stats;
  struct aom_image *img;
  aom_codec_ctx_t decoder;
  int mismatch_seen;
  unsigned int chroma_subsampling_x;
  unsigned int chroma_subsampling_y;
  const char *orig_out_fn;
  unsigned int orig_width;
  unsigned int orig_height;
  int orig_write_webm;
  int orig_write_ivf;
  char tmp_out_fn[1000];
};

static void validate_positive_rational(const char *msg,
                                       struct aom_rational *rat) {
  if (rat->den < 0) {
    rat->num *= -1;
    rat->den *= -1;
  }

  if (rat->num < 0) die("Error: %s must be positive\n", msg);

  if (!rat->den) die("Error: %s has zero denominator\n", msg);
}

static void init_config(cfg_options_t *config) {
  memset(config, 0, sizeof(cfg_options_t));
  config->super_block_size = 0;  // Dynamic
  config->max_partition_size = 128;
  config->min_partition_size = 4;
  config->disable_trellis_quant = 3;
}

/* Parses global config arguments into the AvxEncoderConfig. Note that
 * argv is modified and overwrites all parsed arguments.
 */
static void parse_global_config(struct AvxEncoderConfig *global, char ***argv) {
  char **argi, **argj;
  struct arg arg;
  const int num_encoder = get_aom_encoder_count();
  char **argv_local = (char **)*argv;
  if (num_encoder < 1) die("Error: no valid encoder available\n");

  /* Initialize default parameters */
  memset(global, 0, sizeof(*global));
  global->codec = get_aom_encoder_by_index(num_encoder - 1);
  global->passes = 0;
  global->color_type = I420;
  global->csp = AOM_CSP_UNKNOWN;
  global->show_psnr = 0;

  int cfg_included = 0;
  init_config(&global->encoder_config);

  for (argi = argj = argv_local; (*argj = *argi); argi += arg.argv_step) {
    arg.argv_step = 1;

    if (arg_match(&arg, &g_av1_codec_arg_defs.use_cfg, argi)) {
      if (!cfg_included) {
        parse_cfg(arg.val, &global->encoder_config);
        cfg_included = 1;
      }
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.help, argi)) {
      show_help(stdout, 0);
      exit(EXIT_SUCCESS);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.codecarg, argi)) {
      global->codec = get_aom_encoder_by_short_name(arg.val);
      if (!global->codec)
        die("Error: Unrecognized argument (%s) to --codec\n", arg.val);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.passes, argi)) {
      global->passes = arg_parse_uint(&arg);

      if (global->passes < 1 || global->passes > 3)
        die("Error: Invalid number of passes (%d)\n", global->passes);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.pass_arg, argi)) {
      global->pass = arg_parse_uint(&arg);

      if (global->pass < 1 || global->pass > 3)
        die("Error: Invalid pass selected (%d)\n", global->pass);
    } else if (arg_match(&arg,
                         &g_av1_codec_arg_defs.input_chroma_sample_position,
                         argi)) {
      global->csp = arg_parse_enum(&arg);
      /* Flag is used by later code as well, preserve it. */
      argj++;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.usage, argi)) {
      global->usage = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.good_dl, argi)) {
      global->usage = AOM_USAGE_GOOD_QUALITY;  // Good quality usage
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.rt_dl, argi)) {
      global->usage = AOM_USAGE_REALTIME;  // Real-time usage
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.ai_dl, argi)) {
      global->usage = AOM_USAGE_ALL_INTRA;  // All intra usage
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.use_nv12, argi)) {
      global->color_type = NV12;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.use_yv12, argi)) {
      global->color_type = YV12;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.use_i420, argi)) {
      global->color_type = I420;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.use_i422, argi)) {
      global->color_type = I422;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.use_i444, argi)) {
      global->color_type = I444;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.quietarg, argi)) {
      global->quiet = 1;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.verbosearg, argi)) {
      global->verbose = 1;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.limit, argi)) {
      global->limit = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.skip, argi)) {
      global->skip_frames = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.psnrarg, argi)) {
      if (arg.val)
        global->show_psnr = arg_parse_int(&arg);
      else
        global->show_psnr = 1;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.recontest, argi)) {
      global->test_decode = arg_parse_enum_or_int(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.framerate, argi)) {
      global->framerate = arg_parse_rational(&arg);
      validate_positive_rational(arg.name, &global->framerate);
      global->have_framerate = 1;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.debugmode, argi)) {
      global->debug = 1;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.q_hist_n, argi)) {
      global->show_q_hist_buckets = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.rate_hist_n, argi)) {
      global->show_rate_hist_buckets = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.disable_warnings, argi)) {
      global->disable_warnings = 1;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.disable_warning_prompt,
                         argi)) {
      global->disable_warning_prompt = 1;
    } else {
      argj++;
    }
  }

  if (global->pass) {
    /* DWIM: Assume the user meant passes=2 if pass=2 is specified */
    if (global->pass > global->passes) {
      aom_tools_warn("Assuming --pass=%d implies --passes=%d\n", global->pass,
                     global->pass);
      global->passes = global->pass;
    }
  }
  /* Validate global config */
  if (global->passes == 0) {
#if CONFIG_AV1_ENCODER
    // Make default AV1 passes = 2 until there is a better quality 1-pass
    // encoder
    if (global->codec != NULL)
      global->passes =
          (strcmp(get_short_name_by_aom_encoder(global->codec), "av1") == 0 &&
           global->usage != AOM_USAGE_REALTIME)
              ? 2
              : 1;
#else
    global->passes = 1;
#endif
  }

  if (global->usage == AOM_USAGE_REALTIME && global->passes > 1) {
    aom_tools_warn("Enforcing one-pass encoding in realtime mode\n");
    if (global->pass > 1)
      die("Error: Invalid --pass=%d for one-pass encoding\n", global->pass);
    global->passes = 1;
  }

  if (global->usage == AOM_USAGE_ALL_INTRA && global->passes > 1) {
    aom_tools_warn("Enforcing one-pass encoding in all intra mode\n");
    global->passes = 1;
  }
}

static void open_input_file(struct AvxInputContext *input,
                            aom_chroma_sample_position_t csp) {
  /* Parse certain options from the input file, if possible */
  input->file = strcmp(input->filename, "-") ? fopen(input->filename, "rb")
                                             : set_binary_mode(stdin);

  if (!input->file) fatal("Failed to open input file");

  if (!fseeko(input->file, 0, SEEK_END)) {
    /* Input file is seekable. Figure out how long it is, so we can get
     * progress info.
     */
    input->length = ftello(input->file);
    rewind(input->file);
  }

  /* Default to 1:1 pixel aspect ratio. */
  input->pixel_aspect_ratio.numerator = 1;
  input->pixel_aspect_ratio.denominator = 1;

  /* For RAW input sources, these bytes will applied on the first frame
   *  in read_frame().
   */
  input->detect.buf_read = fread(input->detect.buf, 1, 4, input->file);
  input->detect.position = 0;

  if (input->detect.buf_read == 4 && file_is_y4m(input->detect.buf)) {
    if (y4m_input_open(&input->y4m, input->file, input->detect.buf, 4, csp,
                       input->only_i420) >= 0) {
      input->file_type = FILE_TYPE_Y4M;
      input->width = input->y4m.pic_w;
      input->height = input->y4m.pic_h;
      input->pixel_aspect_ratio.numerator = input->y4m.par_n;
      input->pixel_aspect_ratio.denominator = input->y4m.par_d;
      input->framerate.numerator = input->y4m.fps_n;
      input->framerate.denominator = input->y4m.fps_d;
      input->fmt = input->y4m.aom_fmt;
      input->bit_depth = input->y4m.bit_depth;
      input->color_range = input->y4m.color_range;
    } else
      fatal("Unsupported Y4M stream.");
  } else if (input->detect.buf_read == 4 && fourcc_is_ivf(input->detect.buf)) {
    fatal("IVF is not supported as input.");
  } else {
    input->file_type = FILE_TYPE_RAW;
  }
}

static void close_input_file(struct AvxInputContext *input) {
  fclose(input->file);
  if (input->file_type == FILE_TYPE_Y4M) y4m_input_close(&input->y4m);
}

static struct stream_state *new_stream(struct AvxEncoderConfig *global,
                                       struct stream_state *prev) {
  struct stream_state *stream;

  stream = calloc(1, sizeof(*stream));
  if (stream == NULL) {
    fatal("Failed to allocate new stream.");
  }

  if (prev) {
    memcpy(stream, prev, sizeof(*stream));
    stream->index++;
    prev->next = stream;
  } else {
    aom_codec_err_t res;

    /* Populate encoder configuration */
    res = aom_codec_enc_config_default(global->codec, &stream->config.cfg,
                                       global->usage);
    if (res) fatal("Failed to get config: %s\n", aom_codec_err_to_string(res));

    /* Change the default timebase to a high enough value so that the
     * encoder will always create strictly increasing timestamps.
     */
    stream->config.cfg.g_timebase.den = 1000;

    /* Never use the library's default resolution, require it be parsed
     * from the file or set on the command line.
     */
    stream->config.cfg.g_w = 0;
    stream->config.cfg.g_h = 0;

    /* Initialize remaining stream parameters */
    stream->config.write_webm = 1;
    stream->config.write_ivf = 0;

#if CONFIG_WEBM_IO
    stream->config.stereo_fmt = STEREO_FORMAT_MONO;
    stream->webm_ctx.last_pts_ns = -1;
    stream->webm_ctx.writer = NULL;
    stream->webm_ctx.segment = NULL;
#endif

    /* Allows removal of the application version from the EBML tags */
    stream->webm_ctx.debug = global->debug;
    memcpy(&stream->config.cfg.encoder_cfg, &global->encoder_config,
           sizeof(stream->config.cfg.encoder_cfg));
  }

  /* Output files must be specified for each stream */
  stream->config.out_fn = NULL;
  stream->config.two_pass_input = NULL;
  stream->config.two_pass_output = NULL;
  stream->config.two_pass_width = 0;
  stream->config.two_pass_height = 0;

  stream->next = NULL;
  return stream;
}

static void set_config_arg_ctrls(struct stream_config *config, int key,
                                 const struct arg *arg) {
  int j;
  if (key == AV1E_SET_FILM_GRAIN_TABLE) {
    config->film_grain_filename = arg->val;
    return;
  }

  // For target level, the settings should accumulate rather than overwrite,
  // so we simply append it.
  if (key == AV1E_SET_TARGET_SEQ_LEVEL_IDX) {
    j = config->arg_ctrl_cnt;
    assert(j < ARG_CTRL_CNT_MAX);
    config->arg_ctrls[j][0] = key;
    config->arg_ctrls[j][1] = arg_parse_enum_or_int(arg);
    ++config->arg_ctrl_cnt;
    return;
  }

  /* Point either to the next free element or the first instance of this
   * control.
   */
  for (j = 0; j < config->arg_ctrl_cnt; j++)
    if (config->arg_ctrls[j][0] == key) break;

  /* Update/insert */
  assert(j < ARG_CTRL_CNT_MAX);
  config->arg_ctrls[j][0] = key;
  config->arg_ctrls[j][1] = arg_parse_enum_or_int(arg);

  if (key == AOME_SET_ENABLEAUTOALTREF && config->arg_ctrls[j][1] > 1) {
    aom_tools_warn(
        "auto-alt-ref > 1 is deprecated... setting auto-alt-ref=1\n");
    config->arg_ctrls[j][1] = 1;
  }

  if (j == config->arg_ctrl_cnt) config->arg_ctrl_cnt++;
}

static void set_config_arg_key_vals(struct stream_config *config,
                                    const char *name, const struct arg *arg) {
  int j;
  const char *val = arg->val;
  // For target level, the settings should accumulate rather than overwrite,
  // so we simply append it.
  if (strcmp(name, "target-seq-level-idx") == 0) {
    j = config->arg_key_val_cnt;
    assert(j < ARG_KEY_VAL_CNT_MAX);
    config->arg_key_vals[j][0] = name;
    config->arg_key_vals[j][1] = val;
    ++config->arg_key_val_cnt;
    return;
  }

  /* Point either to the next free element or the first instance of this
   * option.
   */
  for (j = 0; j < config->arg_key_val_cnt; j++)
    if (strcmp(name, config->arg_key_vals[j][0]) == 0) break;

  /* Update/insert */
  assert(j < ARG_KEY_VAL_CNT_MAX);
  config->arg_key_vals[j][0] = name;
  config->arg_key_vals[j][1] = val;

  if (strcmp(name, g_av1_codec_arg_defs.auto_altref.long_name) == 0) {
    int auto_altref = arg_parse_int(arg);
    if (auto_altref > 1) {
      aom_tools_warn(
          "auto-alt-ref > 1 is deprecated... setting auto-alt-ref=1\n");
      config->arg_key_vals[j][1] = "1";
    }
  }

  if (j == config->arg_key_val_cnt) config->arg_key_val_cnt++;
}

static int parse_stream_params(struct AvxEncoderConfig *global,
                               struct stream_state *stream, char **argv) {
  char **argi, **argj;
  struct arg arg;
  const arg_def_t *const *ctrl_args = no_args;
  const arg_def_t *const *key_val_args = no_args;
  const int *ctrl_args_map = NULL;
  struct stream_config *config = &stream->config;
  int eos_mark_found = 0;
  int webm_forced = 0;

  // Handle codec specific options
  if (0) {
#if CONFIG_AV1_ENCODER
  } else if (strcmp(get_short_name_by_aom_encoder(global->codec), "av1") == 0) {
    // TODO(jingning): Reuse AV1 specific encoder configuration parameters.
    // Consider to expand this set for AV1 encoder control.
#if __STDC_VERSION__ >= 201112L
    _Static_assert(NELEMENTS(av1_ctrl_args) == NELEMENTS(av1_arg_ctrl_map),
                   "The av1_ctrl_args and av1_arg_ctrl_map arrays must be of "
                   "the same size.");
#else
    assert(NELEMENTS(av1_ctrl_args) == NELEMENTS(av1_arg_ctrl_map));
#endif
    ctrl_args = av1_ctrl_args;
    ctrl_args_map = av1_arg_ctrl_map;
    key_val_args = av1_key_val_args;
#endif
  }

  for (argi = argj = argv; (*argj = *argi); argi += arg.argv_step) {
    arg.argv_step = 1;

    /* Once we've found an end-of-stream marker (--) we want to continue
     * shifting arguments but not consuming them.
     */
    if (eos_mark_found) {
      argj++;
      continue;
    } else if (!strcmp(*argj, "--")) {
      eos_mark_found = 1;
      continue;
    }

    if (arg_match(&arg, &g_av1_codec_arg_defs.outputfile, argi)) {
      config->out_fn = arg.val;
      if (!webm_forced) {
        const size_t out_fn_len = strlen(config->out_fn);
        if (out_fn_len >= 4 &&
            !strcmp(config->out_fn + out_fn_len - 4, ".ivf")) {
          config->write_webm = 0;
          config->write_ivf = 1;
        } else if (out_fn_len >= 4 &&
                   !strcmp(config->out_fn + out_fn_len - 4, ".obu")) {
          config->write_webm = 0;
          config->write_ivf = 0;
        }
      }
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.fpf_name, argi)) {
      config->stats_fn = arg.val;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.use_webm, argi)) {
#if CONFIG_WEBM_IO
      config->write_webm = 1;
      webm_forced = 1;
#else
      die("Error: --webm specified but webm is disabled.");
#endif
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.use_ivf, argi)) {
      config->write_webm = 0;
      config->write_ivf = 1;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.use_obu, argi)) {
      config->write_webm = 0;
      config->write_ivf = 0;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.threads, argi)) {
      config->cfg.g_threads = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.profile, argi)) {
      config->cfg.g_profile = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.width, argi)) {
      config->cfg.g_w = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.height, argi)) {
      config->cfg.g_h = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.forced_max_frame_width,
                         argi)) {
      config->cfg.g_forced_max_frame_width = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.forced_max_frame_height,
                         argi)) {
      config->cfg.g_forced_max_frame_height = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.bitdeptharg, argi)) {
      config->cfg.g_bit_depth = arg_parse_enum_or_int(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.inbitdeptharg, argi)) {
      config->cfg.g_input_bit_depth = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.input_chroma_subsampling_x,
                         argi)) {
      stream->chroma_subsampling_x = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.input_chroma_subsampling_y,
                         argi)) {
      stream->chroma_subsampling_y = arg_parse_uint(&arg);
#if CONFIG_WEBM_IO
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.stereo_mode, argi)) {
      config->stereo_fmt = arg_parse_enum_or_int(&arg);
#endif
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.timebase, argi)) {
      config->cfg.g_timebase = arg_parse_rational(&arg);
      validate_positive_rational(arg.name, &config->cfg.g_timebase);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.global_error_resilient,
                         argi)) {
      config->cfg.g_error_resilient = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.lag_in_frames, argi)) {
      config->cfg.g_lag_in_frames = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.large_scale_tile, argi)) {
      config->cfg.large_scale_tile = arg_parse_uint(&arg);
      if (config->cfg.large_scale_tile) {
        global->codec = get_aom_encoder_by_short_name("av1");
      }
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.monochrome, argi)) {
      config->cfg.monochrome = 1;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.full_still_picture_hdr,
                         argi)) {
      config->cfg.full_still_picture_hdr = 1;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.use_16bit_internal,
                         argi)) {
      config->use_16bit_internal = CONFIG_AV1_HIGHBITDEPTH;
      if (!config->use_16bit_internal) {
        aom_tools_warn("%s option ignored with CONFIG_AV1_HIGHBITDEPTH=0.\n",
                       arg.name);
      }
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.dropframe_thresh, argi)) {
      config->cfg.rc_dropframe_thresh = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.resize_mode, argi)) {
      config->cfg.rc_resize_mode = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.resize_denominator,
                         argi)) {
      config->cfg.rc_resize_denominator = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.resize_kf_denominator,
                         argi)) {
      config->cfg.rc_resize_kf_denominator = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.superres_mode, argi)) {
      config->cfg.rc_superres_mode = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.superres_denominator,
                         argi)) {
      config->cfg.rc_superres_denominator = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.superres_kf_denominator,
                         argi)) {
      config->cfg.rc_superres_kf_denominator = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.superres_qthresh, argi)) {
      config->cfg.rc_superres_qthresh = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.superres_kf_qthresh,
                         argi)) {
      config->cfg.rc_superres_kf_qthresh = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.end_usage, argi)) {
      config->cfg.rc_end_usage = arg_parse_enum_or_int(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.target_bitrate, argi)) {
      config->cfg.rc_target_bitrate = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.min_quantizer, argi)) {
      config->cfg.rc_min_quantizer = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.max_quantizer, argi)) {
      config->cfg.rc_max_quantizer = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.undershoot_pct, argi)) {
      config->cfg.rc_undershoot_pct = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.overshoot_pct, argi)) {
      config->cfg.rc_overshoot_pct = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.buf_sz, argi)) {
      config->cfg.rc_buf_sz = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.buf_initial_sz, argi)) {
      config->cfg.rc_buf_initial_sz = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.buf_optimal_sz, argi)) {
      config->cfg.rc_buf_optimal_sz = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.bias_pct, argi)) {
      config->cfg.rc_2pass_vbr_bias_pct = arg_parse_uint(&arg);
      if (global->passes < 2)
        aom_tools_warn("option %s ignored in one-pass mode.\n", arg.name);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.minsection_pct, argi)) {
      config->cfg.rc_2pass_vbr_minsection_pct = arg_parse_uint(&arg);

      if (global->passes < 2)
        aom_tools_warn("option %s ignored in one-pass mode.\n", arg.name);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.maxsection_pct, argi)) {
      config->cfg.rc_2pass_vbr_maxsection_pct = arg_parse_uint(&arg);

      if (global->passes < 2)
        aom_tools_warn("option %s ignored in one-pass mode.\n", arg.name);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.fwd_kf_enabled, argi)) {
      config->cfg.fwd_kf_enabled = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.kf_min_dist, argi)) {
      config->cfg.kf_min_dist = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.kf_max_dist, argi)) {
      config->cfg.kf_max_dist = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.kf_disabled, argi)) {
      config->cfg.kf_mode = AOM_KF_DISABLED;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.sframe_dist, argi)) {
      config->cfg.sframe_dist = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.sframe_mode, argi)) {
      config->cfg.sframe_mode = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.save_as_annexb, argi)) {
      config->cfg.save_as_annexb = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.tile_width, argi)) {
      config->cfg.tile_width_count =
          arg_parse_list(&arg, config->cfg.tile_widths, MAX_TILE_WIDTHS);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.tile_height, argi)) {
      config->cfg.tile_height_count =
          arg_parse_list(&arg, config->cfg.tile_heights, MAX_TILE_HEIGHTS);
#if CONFIG_TUNE_VMAF
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.vmaf_model_path, argi)) {
      config->vmaf_model_path = arg.val;
#endif
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.partition_info_path,
                         argi)) {
      config->partition_info_path = arg.val;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.enable_rate_guide_deltaq,
                         argi)) {
      config->enable_rate_guide_deltaq = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.rate_distribution_info,
                         argi)) {
      config->rate_distribution_info = arg.val;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.use_fixed_qp_offsets,
                         argi)) {
      config->cfg.use_fixed_qp_offsets = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.fixed_qp_offsets, argi)) {
      config->cfg.use_fixed_qp_offsets = 1;
    } else if (global->usage == AOM_USAGE_REALTIME &&
               arg_match(&arg, &g_av1_codec_arg_defs.enable_restoration,
                         argi)) {
      if (arg_parse_uint(&arg) == 1) {
        aom_tools_warn("non-zero %s option ignored in realtime mode.\n",
                       arg.name);
      }
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.two_pass_input, argi)) {
      config->two_pass_input = arg.val;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.two_pass_output, argi)) {
      config->two_pass_output = arg.val;
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.two_pass_width, argi)) {
      config->two_pass_width = arg_parse_int(&arg);
    } else if (arg_match(&arg, &g_av1_codec_arg_defs.two_pass_height, argi)) {
      config->two_pass_height = arg_parse_int(&arg);
    } else {
      int i, match = 0;
      // check if the control ID API supports this arg
      if (ctrl_args_map) {
        for (i = 0; ctrl_args[i]; i++) {
          if (arg_match(&arg, ctrl_args[i], argi)) {
            match = 1;
            set_config_arg_ctrls(config, ctrl_args_map[i], &arg);
            break;
          }
        }
      }
      if (!match) {
        // check if the key & value API supports this arg
        for (i = 0; key_val_args[i]; i++) {
          if (arg_match(&arg, key_val_args[i], argi)) {
            match = 1;
            set_config_arg_key_vals(config, key_val_args[i]->long_name, &arg);
            break;
          }
        }
      }
      if (!match) argj++;
    }
  }
  config->use_16bit_internal |= config->cfg.g_bit_depth > AOM_BITS_8;

  if (global->usage == AOM_USAGE_REALTIME && config->cfg.g_lag_in_frames != 0) {
    aom_tools_warn("non-zero lag-in-frames option ignored in realtime mode.\n");
    config->cfg.g_lag_in_frames = 0;
  }

  if (global->usage == AOM_USAGE_ALL_INTRA) {
    if (config->cfg.g_lag_in_frames != 0) {
      aom_tools_warn(
          "non-zero lag-in-frames option ignored in all intra mode.\n");
      config->cfg.g_lag_in_frames = 0;
    }
    if (config->cfg.kf_max_dist != 0) {
      aom_tools_warn(
          "non-zero max key frame distance option ignored in all intra "
          "mode.\n");
      config->cfg.kf_max_dist = 0;
    }
  }

  // set the passes field using key & val API
  if (config->arg_key_val_cnt >= ARG_KEY_VAL_CNT_MAX) {
    die("Not enough buffer for the key & value API.");
  }
  config->arg_key_vals[config->arg_key_val_cnt][0] = "passes";
  switch (global->passes) {
    case 0: config->arg_key_vals[config->arg_key_val_cnt][1] = "0"; break;
    case 1: config->arg_key_vals[config->arg_key_val_cnt][1] = "1"; break;
    case 2: config->arg_key_vals[config->arg_key_val_cnt][1] = "2"; break;
    case 3: config->arg_key_vals[config->arg_key_val_cnt][1] = "3"; break;
    default: die("Invalid value of --passes.");
  }
  config->arg_key_val_cnt++;

  // set the two_pass_output field
  if (!config->two_pass_output && global->passes == 3) {
    // If not specified, set the name of two_pass_output file here.
    snprintf(stream->tmp_out_fn, sizeof(stream->tmp_out_fn),
             "%.980s_pass2_%d.ivf", stream->config.out_fn, stream->index);
    stream->config.two_pass_output = stream->tmp_out_fn;
  }
  if (config->two_pass_output) {
    config->arg_key_vals[config->arg_key_val_cnt][0] = "two-pass-output";
    config->arg_key_vals[config->arg_key_val_cnt][1] = config->two_pass_output;
    config->arg_key_val_cnt++;
  }

  return eos_mark_found;
}

#define FOREACH_STREAM(iterator, list)                 \
  for (struct stream_state *iterator = list; iterator; \
       iterator = iterator->next)

static void validate_stream_config(const struct stream_state *stream,
                                   const struct AvxEncoderConfig *global) {
  const struct stream_state *streami;
  (void)global;

  if (!stream->config.cfg.g_w || !stream->config.cfg.g_h)
    fatal(
        "Stream %d: Specify stream dimensions with --width (-w) "
        " and --height (-h)",
        stream->index);

  /* Even if bit depth is set on the command line flag to be lower,
   * it is upgraded to at least match the input bit depth.
   */
  assert(stream->config.cfg.g_input_bit_depth <=
         (unsigned int)stream->config.cfg.g_bit_depth);

  for (streami = stream; streami; streami = streami->next) {
    /* All streams require output files */
    if (!streami->config.out_fn)
      fatal("Stream %d: Output file is required (specify with -o)",
            streami->index);

    /* Check for two streams outputting to the same file */
    if (streami != stream) {
      const char *a = stream->config.out_fn;
      const char *b = streami->config.out_fn;
      if (!strcmp(a, b) && strcmp(a, "/dev/null") && strcmp(a, ":nul"))
        fatal("Stream %d: duplicate output file (from stream %d)",
              streami->index, stream->index);
    }

    /* Check for two streams sharing a stats file. */
    if (streami != stream) {
      const char *a = stream->config.stats_fn;
      const char *b = streami->config.stats_fn;
      if (a && b && !strcmp(a, b))
        fatal("Stream %d: duplicate stats file (from stream %d)",
              streami->index, stream->index);
    }
  }
}

static void set_stream_dimensions(struct stream_state *stream, unsigned int w,
                                  unsigned int h) {
  if (!stream->config.cfg.g_w) {
    if (!stream->config.cfg.g_h)
      stream->config.cfg.g_w = w;
    else
      stream->config.cfg.g_w = w * stream->config.cfg.g_h / h;
  }
  if (!stream->config.cfg.g_h) {
    stream->config.cfg.g_h = h * stream->config.cfg.g_w / w;
  }
}

static const char *file_type_to_string(enum VideoFileType t) {
  switch (t) {
    case FILE_TYPE_RAW: return "RAW";
    case FILE_TYPE_Y4M: return "Y4M";
    default: return "Other";
  }
}

static void show_stream_config(struct stream_state *stream,
                               struct AvxEncoderConfig *global,
                               struct AvxInputContext *input) {
#define SHOW(field) \
  fprintf(stderr, "    %-28s = %d\n", #field, stream->config.cfg.field)

  if (stream->index == 0) {
    fprintf(stderr, "Codec: %s\n", aom_codec_iface_name(global->codec));
    fprintf(stderr, "Source file: %s File Type: %s Format: %s\n",
            input->filename, file_type_to_string(input->file_type),
            image_format_to_string(input->fmt));
  }
  if (stream->next || stream->index)
    fprintf(stderr, "\nStream Index: %d\n", stream->index);
  fprintf(stderr, "Destination file: %s\n", stream->config.out_fn);
  fprintf(stderr, "Coding path: %s\n",
          stream->config.use_16bit_internal ? "HBD" : "LBD");
  fprintf(stderr, "Encoder parameters:\n");

  SHOW(g_usage);
  SHOW(g_threads);
  SHOW(g_profile);
  SHOW(g_w);
  SHOW(g_h);
  SHOW(g_bit_depth);
  SHOW(g_input_bit_depth);
  SHOW(g_timebase.num);
  SHOW(g_timebase.den);
  SHOW(g_error_resilient);
  SHOW(g_pass);
  SHOW(g_lag_in_frames);
  SHOW(large_scale_tile);
  SHOW(rc_dropframe_thresh);
  SHOW(rc_resize_mode);
  SHOW(rc_resize_denominator);
  SHOW(rc_resize_kf_denominator);
  SHOW(rc_superres_mode);
  SHOW(rc_superres_denominator);
  SHOW(rc_superres_kf_denominator);
  SHOW(rc_superres_qthresh);
  SHOW(rc_superres_kf_qthresh);
  SHOW(rc_end_usage);
  SHOW(rc_target_bitrate);
  SHOW(rc_min_quantizer);
  SHOW(rc_max_quantizer);
  SHOW(rc_undershoot_pct);
  SHOW(rc_overshoot_pct);
  SHOW(rc_buf_sz);
  SHOW(rc_buf_initial_sz);
  SHOW(rc_buf_optimal_sz);
  SHOW(rc_2pass_vbr_bias_pct);
  SHOW(rc_2pass_vbr_minsection_pct);
  SHOW(rc_2pass_vbr_maxsection_pct);
  SHOW(fwd_kf_enabled);
  SHOW(kf_mode);
  SHOW(kf_min_dist);
  SHOW(kf_max_dist);

#define SHOW_PARAMS(field)                    \
  fprintf(stderr, "    %-28s = %d\n", #field, \
          stream->config.cfg.encoder_cfg.field)
  if (global->encoder_config.init_by_cfg_file) {
    SHOW_PARAMS(super_block_size);
    SHOW_PARAMS(max_partition_size);
    SHOW_PARAMS(min_partition_size);
    SHOW_PARAMS(disable_ab_partition_type);
    SHOW_PARAMS(disable_rect_partition_type);
    SHOW_PARAMS(disable_1to4_partition_type);
    SHOW_PARAMS(disable_flip_idtx);
    SHOW_PARAMS(disable_cdef);
    SHOW_PARAMS(disable_lr);
    SHOW_PARAMS(disable_obmc);
    SHOW_PARAMS(disable_warp_motion);
    SHOW_PARAMS(disable_global_motion);
    SHOW_PARAMS(disable_dist_wtd_comp);
    SHOW_PARAMS(disable_diff_wtd_comp);
    SHOW_PARAMS(disable_inter_intra_comp);
    SHOW_PARAMS(disable_masked_comp);
    SHOW_PARAMS(disable_one_sided_comp);
    SHOW_PARAMS(disable_palette);
    SHOW_PARAMS(disable_intrabc);
    SHOW_PARAMS(disable_cfl);
    SHOW_PARAMS(disable_smooth_intra);
    SHOW_PARAMS(disable_filter_intra);
    SHOW_PARAMS(disable_dual_filter);
    SHOW_PARAMS(disable_intra_angle_delta);
    SHOW_PARAMS(disable_intra_edge_filter);
    SHOW_PARAMS(disable_tx_64x64);
    SHOW_PARAMS(disable_smooth_inter_intra);
    SHOW_PARAMS(disable_inter_inter_wedge);
    SHOW_PARAMS(disable_inter_intra_wedge);
    SHOW_PARAMS(disable_paeth_intra);
    SHOW_PARAMS(disable_trellis_quant);
    SHOW_PARAMS(disable_ref_frame_mv);
    SHOW_PARAMS(reduced_reference_set);
    SHOW_PARAMS(reduced_tx_type_set);
  }
}

static void open_output_file(struct stream_state *stream,
                             struct AvxEncoderConfig *global,
                             const struct AvxRational *pixel_aspect_ratio,
                             const char *encoder_settings) {
  const char *fn = stream->config.out_fn;
  const struct aom_codec_enc_cfg *const cfg = &stream->config.cfg;

  if (cfg->g_pass == AOM_RC_FIRST_PASS) return;

  stream->file = strcmp(fn, "-") ? fopen(fn, "wb") : set_binary_mode(stdout);

  if (!stream->file) fatal("Failed to open output file");

  if (stream->config.write_webm && fseek(stream->file, 0, SEEK_CUR))
    fatal("WebM output to pipes not supported.");

#if CONFIG_WEBM_IO
  if (stream->config.write_webm) {
    stream->webm_ctx.stream = stream->file;
    if (write_webm_file_header(&stream->webm_ctx, &stream->encoder, cfg,
                               stream->config.stereo_fmt,
                               get_fourcc_by_aom_encoder(global->codec),
                               pixel_aspect_ratio, encoder_settings) != 0) {
      fatal("WebM writer initialization failed.");
    }
  }
#else
  (void)pixel_aspect_ratio;
  (void)encoder_settings;
#endif

  if (!stream->config.write_webm && stream->config.write_ivf) {
    ivf_write_file_header(stream->file, cfg,
                          get_fourcc_by_aom_encoder(global->codec), 0);
  }
}

static void close_output_file(struct stream_state *stream,
                              unsigned int fourcc) {
  const struct aom_codec_enc_cfg *const cfg = &stream->config.cfg;

  if (cfg->g_pass == AOM_RC_FIRST_PASS) return;

#if CONFIG_WEBM_IO
  if (stream->config.write_webm) {
    if (write_webm_file_footer(&stream->webm_ctx) != 0) {
      fatal("WebM writer finalization failed.");
    }
  }
#endif

  if (!stream->config.write_webm && stream->config.write_ivf) {
    if (!fseek(stream->file, 0, SEEK_SET))
      ivf_write_file_header(stream->file, &stream->config.cfg, fourcc,
                            stream->frames_out);
  }

  fclose(stream->file);
}

static void setup_pass(struct stream_state *stream,
                       struct AvxEncoderConfig *global, int pass) {
  if (stream->config.stats_fn) {
    if (!stats_open_file(&stream->stats, stream->config.stats_fn, pass))
      fatal("Failed to open statistics store");
  } else {
    if (!stats_open_mem(&stream->stats, pass))
      fatal("Failed to open statistics store");
  }

  if (global->passes == 1) {
    stream->config.cfg.g_pass = AOM_RC_ONE_PASS;
  } else {
    switch (pass) {
      case 0: stream->config.cfg.g_pass = AOM_RC_FIRST_PASS; break;
      case 1: stream->config.cfg.g_pass = AOM_RC_SECOND_PASS; break;
      case 2: stream->config.cfg.g_pass = AOM_RC_THIRD_PASS; break;
      default: fatal("Failed to set pass");
    }
  }

  if (pass) {
    stream->config.cfg.rc_twopass_stats_in = stats_get(&stream->stats);
  }

  stream->cx_time = 0;
  stream->nbytes = 0;
  stream->frames_out = 0;
}

static void initialize_encoder(struct stream_state *stream,
                               struct AvxEncoderConfig *global) {
  int i;
  int flags = 0;

  flags |= (global->show_psnr >= 1) ? AOM_CODEC_USE_PSNR : 0;
  flags |= stream->config.use_16bit_internal ? AOM_CODEC_USE_HIGHBITDEPTH : 0;

  /* Construct Encoder Context */
  aom_codec_enc_init(&stream->encoder, global->codec, &stream->config.cfg,
                     flags);
  ctx_exit_on_error(&stream->encoder, "Failed to initialize encoder");

  for (i = 0; i < stream->config.arg_ctrl_cnt; i++) {
    int ctrl = stream->config.arg_ctrls[i][0];
    int value = stream->config.arg_ctrls[i][1];
    if (aom_codec_control(&stream->encoder, ctrl, value))
      fprintf(stderr, "Error: Tried to set control %d = %d\n", ctrl, value);

    ctx_exit_on_error(&stream->encoder, "Failed to control codec");
  }

  for (i = 0; i < stream->config.arg_key_val_cnt; i++) {
    const char *name = stream->config.arg_key_vals[i][0];
    const char *val = stream->config.arg_key_vals[i][1];
    if (aom_codec_set_option(&stream->encoder, name, val))
      fprintf(stderr, "Error: Tried to set option %s = %s\n", name, val);

    ctx_exit_on_error(&stream->encoder, "Failed to set codec option");
  }

#if CONFIG_TUNE_VMAF
  if (stream->config.vmaf_model_path) {
    AOM_CODEC_CONTROL_TYPECHECKED(&stream->encoder, AV1E_SET_VMAF_MODEL_PATH,
                                  stream->config.vmaf_model_path);
    ctx_exit_on_error(&stream->encoder, "Failed to set vmaf model path");
  }
#endif
  if (stream->config.partition_info_path) {
    AOM_CODEC_CONTROL_TYPECHECKED(&stream->encoder,
                                  AV1E_SET_PARTITION_INFO_PATH,
                                  stream->config.partition_info_path);
    ctx_exit_on_error(&stream->encoder, "Failed to set partition info path");
  }
  if (stream->config.enable_rate_guide_deltaq) {
    AOM_CODEC_CONTROL_TYPECHECKED(&stream->encoder,
                                  AV1E_ENABLE_RATE_GUIDE_DELTAQ,
                                  stream->config.enable_rate_guide_deltaq);
    ctx_exit_on_error(&stream->encoder, "Failed to enable rate guide deltaq");
  }
  if (stream->config.rate_distribution_info) {
    AOM_CODEC_CONTROL_TYPECHECKED(&stream->encoder,
                                  AV1E_SET_RATE_DISTRIBUTION_INFO,
                                  stream->config.rate_distribution_info);
    ctx_exit_on_error(&stream->encoder, "Failed to set rate distribution info");
  }

  if (stream->config.film_grain_filename) {
    AOM_CODEC_CONTROL_TYPECHECKED(&stream->encoder, AV1E_SET_FILM_GRAIN_TABLE,
                                  stream->config.film_grain_filename);
    ctx_exit_on_error(&stream->encoder, "Failed to set film grain table");
  }
  AOM_CODEC_CONTROL_TYPECHECKED(&stream->encoder, AV1E_SET_COLOR_RANGE,
                                stream->config.color_range);
  ctx_exit_on_error(&stream->encoder, "Failed to set color range");

#if CONFIG_AV1_DECODER
  if (global->test_decode != TEST_DECODE_OFF) {
    aom_codec_iface_t *decoder = get_aom_decoder_by_short_name(
        get_short_name_by_aom_encoder(global->codec));
    aom_codec_dec_cfg_t cfg = { 0, 0, 0, !stream->config.use_16bit_internal };
    aom_codec_dec_init(&stream->decoder, decoder, &cfg, 0);

    if (strcmp(get_short_name_by_aom_encoder(global->codec), "av1") == 0) {
      AOM_CODEC_CONTROL_TYPECHECKED(&stream->decoder, AV1_SET_TILE_MODE,
                                    stream->config.cfg.large_scale_tile);
      ctx_exit_on_error(&stream->decoder, "Failed to set decode_tile_mode");

      AOM_CODEC_CONTROL_TYPECHECKED(&stream->decoder, AV1D_SET_IS_ANNEXB,
                                    stream->config.cfg.save_as_annexb);
      ctx_exit_on_error(&stream->decoder, "Failed to set is_annexb");

      AOM_CODEC_CONTROL_TYPECHECKED(&stream->decoder, AV1_SET_DECODE_TILE_ROW,
                                    -1);
      ctx_exit_on_error(&stream->decoder, "Failed to set decode_tile_row");

      AOM_CODEC_CONTROL_TYPECHECKED(&stream->decoder, AV1_SET_DECODE_TILE_COL,
                                    -1);
      ctx_exit_on_error(&stream->decoder, "Failed to set decode_tile_col");
    }
  }
#endif
}

// Convert the input image 'img' to a monochrome image. The Y plane of the
// output image is a shallow copy of the Y plane of the input image, therefore
// the input image must remain valid for the lifetime of the output image. The U
// and V planes of the output image are set to null pointers. The output image
// format is AOM_IMG_FMT_I420 because libaom does not have AOM_IMG_FMT_I400.
static void convert_image_to_monochrome(const struct aom_image *img,
                                        struct aom_image *monochrome_img) {
  *monochrome_img = *img;
  monochrome_img->fmt = AOM_IMG_FMT_I420;
  if (img->fmt & AOM_IMG_FMT_HIGHBITDEPTH) {
    monochrome_img->fmt |= AOM_IMG_FMT_HIGHBITDEPTH;
  }
  monochrome_img->monochrome = 1;
  monochrome_img->csp = AOM_CSP_UNKNOWN;
  monochrome_img->x_chroma_shift = 1;
  monochrome_img->y_chroma_shift = 1;
  monochrome_img->planes[AOM_PLANE_U] = NULL;
  monochrome_img->planes[AOM_PLANE_V] = NULL;
  monochrome_img->stride[AOM_PLANE_U] = 0;
  monochrome_img->stride[AOM_PLANE_V] = 0;
  monochrome_img->sz = 0;
  monochrome_img->bps = (img->fmt & AOM_IMG_FMT_HIGHBITDEPTH) ? 16 : 8;
  monochrome_img->img_data = NULL;
  monochrome_img->img_data_owner = 0;
  monochrome_img->self_allocd = 0;
}

static void encode_frame(struct stream_state *stream,
                         struct AvxEncoderConfig *global, struct aom_image *img,
                         unsigned int frames_in) {
  aom_codec_pts_t frame_start, next_frame_start;
  struct aom_codec_enc_cfg *cfg = &stream->config.cfg;
  struct aom_usec_timer timer;

  frame_start =
      (cfg->g_timebase.den * (int64_t)(frames_in - 1) * global->framerate.den) /
      cfg->g_timebase.num / global->framerate.num;
  next_frame_start =
      (cfg->g_timebase.den * (int64_t)(frames_in)*global->framerate.den) /
      cfg->g_timebase.num / global->framerate.num;

  /* Scale if necessary */
  if (img) {
    if ((img->fmt & AOM_IMG_FMT_HIGHBITDEPTH) &&
        (img->d_w != cfg->g_w || img->d_h != cfg->g_h)) {
      if (img->fmt != AOM_IMG_FMT_I42016) {
        fprintf(stderr, "%s can only scale 4:2:0 inputs\n", exec_name);
        exit(EXIT_FAILURE);
      }
#if CONFIG_LIBYUV
      if (!stream->img) {
        stream->img =
            aom_img_alloc(NULL, AOM_IMG_FMT_I42016, cfg->g_w, cfg->g_h, 16);
      }
      I420Scale_16(
          (uint16_t *)img->planes[AOM_PLANE_Y], img->stride[AOM_PLANE_Y] / 2,
          (uint16_t *)img->planes[AOM_PLANE_U], img->stride[AOM_PLANE_U] / 2,
          (uint16_t *)img->planes[AOM_PLANE_V], img->stride[AOM_PLANE_V] / 2,
          img->d_w, img->d_h, (uint16_t *)stream->img->planes[AOM_PLANE_Y],
          stream->img->stride[AOM_PLANE_Y] / 2,
          (uint16_t *)stream->img->planes[AOM_PLANE_U],
          stream->img->stride[AOM_PLANE_U] / 2,
          (uint16_t *)stream->img->planes[AOM_PLANE_V],
          stream->img->stride[AOM_PLANE_V] / 2, stream->img->d_w,
          stream->img->d_h, kFilterBox);
      img = stream->img;
#else
      stream->encoder.err = 1;
      ctx_exit_on_error(&stream->encoder,
                        "Stream %d: Failed to encode frame.\n"
                        "libyuv is required for scaling but is currently "
                        "disabled.\n"
                        "Be sure to specify -DCONFIG_LIBYUV=1 when running "
                        "cmake.\n",
                        stream->index);
#endif
    }
  }
  if (img && (img->d_w != cfg->g_w || img->d_h != cfg->g_h)) {
    if (img->fmt != AOM_IMG_FMT_I420 && img->fmt != AOM_IMG_FMT_YV12) {
      fprintf(stderr, "%s can only scale 4:2:0 8bpp inputs\n", exec_name);
      exit(EXIT_FAILURE);
    }
#if CONFIG_LIBYUV
    if (!stream->img)
      stream->img =
          aom_img_alloc(NULL, AOM_IMG_FMT_I420, cfg->g_w, cfg->g_h, 16);
    I420Scale(
        img->planes[AOM_PLANE_Y], img->stride[AOM_PLANE_Y],
        img->planes[AOM_PLANE_U], img->stride[AOM_PLANE_U],
        img->planes[AOM_PLANE_V], img->stride[AOM_PLANE_V], img->d_w, img->d_h,
        stream->img->planes[AOM_PLANE_Y], stream->img->stride[AOM_PLANE_Y],
        stream->img->planes[AOM_PLANE_U], stream->img->stride[AOM_PLANE_U],
        stream->img->planes[AOM_PLANE_V], stream->img->stride[AOM_PLANE_V],
        stream->img->d_w, stream->img->d_h, kFilterBox);
    img = stream->img;
#else
    stream->encoder.err = 1;
    ctx_exit_on_error(&stream->encoder,
                      "Stream %d: Failed to encode frame.\n"
                      "Scaling disabled in this configuration. \n"
                      "To enable, configure with --enable-libyuv\n",
                      stream->index);
#endif
  }

  struct aom_image monochrome_img;
  if (img && cfg->monochrome) {
    convert_image_to_monochrome(img, &monochrome_img);
    img = &monochrome_img;
  }

  aom_usec_timer_start(&timer);
  aom_codec_encode(&stream->encoder, img, frame_start,
                   (uint32_t)(next_frame_start - frame_start), 0);
  aom_usec_timer_mark(&timer);
  stream->cx_time += aom_usec_timer_elapsed(&timer);
  ctx_exit_on_error(&stream->encoder, "Stream %d: Failed to encode frame",
                    stream->index);
}

static void update_quantizer_histogram(struct stream_state *stream) {
  if (stream->config.cfg.g_pass != AOM_RC_FIRST_PASS) {
    int q;

    AOM_CODEC_CONTROL_TYPECHECKED(&stream->encoder, AOME_GET_LAST_QUANTIZER_64,
                                  &q);
    ctx_exit_on_error(&stream->encoder, "Failed to read quantizer");
    stream->counts[q]++;
  }
}

static void get_cx_data(struct stream_state *stream,
                        struct AvxEncoderConfig *global, int *got_data) {
  const aom_codec_cx_pkt_t *pkt;
  const struct aom_codec_enc_cfg *cfg = &stream->config.cfg;
  aom_codec_iter_t iter = NULL;

  *got_data = 0;
  while ((pkt = aom_codec_get_cx_data(&stream->encoder, &iter))) {
    static size_t fsize = 0;
    static FileOffset ivf_header_pos = 0;

    switch (pkt->kind) {
      case AOM_CODEC_CX_FRAME_PKT:
        ++stream->frames_out;
        if (!global->quiet)
          fprintf(stderr, " %6luF", (unsigned long)pkt->data.frame.sz);

        update_rate_histogram(stream->rate_hist, cfg, pkt);
#if CONFIG_WEBM_IO
        if (stream->config.write_webm) {
          if (write_webm_block(&stream->webm_ctx, cfg, pkt) != 0) {
            fatal("WebM writer failed.");
          }
        }
#endif
        if (!stream->config.write_webm) {
          if (stream->config.write_ivf) {
            if (pkt->data.frame.partition_id <= 0) {
              ivf_header_pos = ftello(stream->file);
              fsize = pkt->data.frame.sz;

              ivf_write_frame_header(stream->file, pkt->data.frame.pts, fsize);
            } else {
              fsize += pkt->data.frame.sz;

              const FileOffset currpos = ftello(stream->file);
              fseeko(stream->file, ivf_header_pos, SEEK_SET);
              ivf_write_frame_size(stream->file, fsize);
              fseeko(stream->file, currpos, SEEK_SET);
            }
          }

          (void)fwrite(pkt->data.frame.buf, 1, pkt->data.frame.sz,
                       stream->file);
        }
        stream->nbytes += pkt->data.raw.sz;

        *got_data = 1;
#if CONFIG_AV1_DECODER
        if (global->test_decode != TEST_DECODE_OFF && !stream->mismatch_seen) {
          aom_codec_decode(&stream->decoder, pkt->data.frame.buf,
                           pkt->data.frame.sz, NULL);
          if (stream->decoder.err) {
            warn_or_exit_on_error(&stream->decoder,
                                  global->test_decode == TEST_DECODE_FATAL,
                                  "Failed to decode frame %d in stream %d",
                                  stream->frames_out + 1, stream->index);
            stream->mismatch_seen = stream->frames_out + 1;
          }
        }
#endif
        break;
      case AOM_CODEC_STATS_PKT:
        stream->frames_out++;
        stats_write(&stream->stats, pkt->data.twopass_stats.buf,
                    pkt->data.twopass_stats.sz);
        stream->nbytes += pkt->data.raw.sz;
        break;
      case AOM_CODEC_PSNR_PKT:

        if (global->show_psnr >= 1) {
          int i;

          stream->psnr_sse_total[0] += pkt->data.psnr.sse[0];
          stream->psnr_samples_total[0] += pkt->data.psnr.samples[0];
          for (i = 0; i < 4; i++) {
            if (!global->quiet)
              fprintf(stderr, "%.3f ", pkt->data.psnr.psnr[i]);
            stream->psnr_totals[0][i] += pkt->data.psnr.psnr[i];
          }
          stream->psnr_count[0]++;

#if CONFIG_AV1_HIGHBITDEPTH
          if (stream->config.cfg.g_input_bit_depth <
              (unsigned int)stream->config.cfg.g_bit_depth) {
            stream->psnr_sse_total[1] += pkt->data.psnr.sse_hbd[0];
            stream->psnr_samples_total[1] += pkt->data.psnr.samples_hbd[0];
            for (i = 0; i < 4; i++) {
              if (!global->quiet)
                fprintf(stderr, "%.3f ", pkt->data.psnr.psnr_hbd[i]);
              stream->psnr_totals[1][i] += pkt->data.psnr.psnr_hbd[i];
            }
            stream->psnr_count[1]++;
          }
#endif
        }

        break;
      default: break;
    }
  }
}

static void show_psnr(struct stream_state *stream, double peak, int64_t bps) {
  int i;
  double ovpsnr;

  if (!stream->psnr_count[0]) return;

  fprintf(stderr, "Stream %d PSNR (Overall/Avg/Y/U/V)", stream->index);
  ovpsnr = sse_to_psnr((double)stream->psnr_samples_total[0], peak,
                       (double)stream->psnr_sse_total[0]);
  fprintf(stderr, " %.3f", ovpsnr);

  for (i = 0; i < 4; i++) {
    fprintf(stderr, " %.3f", stream->psnr_totals[0][i] / stream->psnr_count[0]);
  }
  if (bps > 0) {
    fprintf(stderr, " %7" PRId64 " bps", bps);
  }
  fprintf(stderr, " %7" PRId64 " ms", stream->cx_time / 1000);
  fprintf(stderr, "\n");
}

#if CONFIG_AV1_HIGHBITDEPTH
static void show_psnr_hbd(struct stream_state *stream, double peak,
                          int64_t bps) {
  int i;
  double ovpsnr;
  // Compute PSNR based on stream bit depth
  if (!stream->psnr_count[1]) return;

  fprintf(stderr, "Stream %d PSNR (Overall/Avg/Y/U/V)", stream->index);
  ovpsnr = sse_to_psnr((double)stream->psnr_samples_total[1], peak,
                       (double)stream->psnr_sse_total[1]);
  fprintf(stderr, " %.3f", ovpsnr);

  for (i = 0; i < 4; i++) {
    fprintf(stderr, " %.3f", stream->psnr_totals[1][i] / stream->psnr_count[1]);
  }
  if (bps > 0) {
    fprintf(stderr, " %7" PRId64 " bps", bps);
  }
  fprintf(stderr, " %7" PRId64 " ms", stream->cx_time / 1000);
  fprintf(stderr, "\n");
}
#endif

static float usec_to_fps(uint64_t usec, unsigned int frames) {
  return (float)(usec > 0 ? frames * 1000000.0 / (float)usec : 0);
}

static void test_decode(struct stream_state *stream,
                        enum TestDecodeFatality fatal) {
  aom_image_t enc_img, dec_img;

  if (stream->mismatch_seen) return;

  /* Get the internal reference frame */
  AOM_CODEC_CONTROL_TYPECHECKED(&stream->encoder, AV1_GET_NEW_FRAME_IMAGE,
                                &enc_img);
  AOM_CODEC_CONTROL_TYPECHECKED(&stream->decoder, AV1_GET_NEW_FRAME_IMAGE,
                                &dec_img);

  if ((enc_img.fmt & AOM_IMG_FMT_HIGHBITDEPTH) !=
      (dec_img.fmt & AOM_IMG_FMT_HIGHBITDEPTH)) {
    if (enc_img.fmt & AOM_IMG_FMT_HIGHBITDEPTH) {
      aom_image_t enc_hbd_img;
      aom_img_alloc(&enc_hbd_img, enc_img.fmt - AOM_IMG_FMT_HIGHBITDEPTH,
                    enc_img.d_w, enc_img.d_h, 16);
      aom_img_truncate_16_to_8(&enc_hbd_img, &enc_img);
      enc_img = enc_hbd_img;
    }
    if (dec_img.fmt & AOM_IMG_FMT_HIGHBITDEPTH) {
      aom_image_t dec_hbd_img;
      aom_img_alloc(&dec_hbd_img, dec_img.fmt - AOM_IMG_FMT_HIGHBITDEPTH,
                    dec_img.d_w, dec_img.d_h, 16);
      aom_img_truncate_16_to_8(&dec_hbd_img, &dec_img);
      dec_img = dec_hbd_img;
    }
  }

  ctx_exit_on_error(&stream->encoder, "Failed to get encoder reference frame");
  ctx_exit_on_error(&stream->decoder, "Failed to get decoder reference frame");

  if (!aom_compare_img(&enc_img, &dec_img)) {
    int y[4], u[4], v[4];
    if (enc_img.fmt & AOM_IMG_FMT_HIGHBITDEPTH) {
      aom_find_mismatch_high(&enc_img, &dec_img, y, u, v);
    } else {
      aom_find_mismatch(&enc_img, &dec_img, y, u, v);
    }
    stream->decoder.err = 1;
    warn_or_exit_on_error(&stream->decoder, fatal == TEST_DECODE_FATAL,
                          "Stream %d: Encode/decode mismatch on frame %d at"
                          " Y[%d, %d] {%d/%d},"
                          " U[%d, %d] {%d/%d},"
                          " V[%d, %d] {%d/%d}",
                          stream->index, stream->frames_out, y[0], y[1], y[2],
                          y[3], u[0], u[1], u[2], u[3], v[0], v[1], v[2], v[3]);
    stream->mismatch_seen = stream->frames_out;
  }

  aom_img_free(&enc_img);
  aom_img_free(&dec_img);
}

static void print_time(const char *label, int64_t etl) {
  int64_t hours;
  int64_t mins;
  int64_t secs;

  if (etl >= 0) {
    hours = etl / 3600;
    etl -= hours * 3600;
    mins = etl / 60;
    etl -= mins * 60;
    secs = etl;

    fprintf(stderr, "[%3s %2" PRId64 ":%02" PRId64 ":%02" PRId64 "] ", label,
            hours, mins, secs);
  } else {
    fprintf(stderr, "[%3s  unknown] ", label);
  }
}

static void clear_stream_count_state(struct stream_state *stream) {
  // PSNR counters
  for (int k = 0; k < 2; k++) {
    stream->psnr_sse_total[k] = 0;
    stream->psnr_samples_total[k] = 0;
    for (int i = 0; i < 4; i++) {
      stream->psnr_totals[k][i] = 0;
    }
    stream->psnr_count[k] = 0;
  }
  // q hist
  memset(stream->counts, 0, sizeof(stream->counts));
}

// aomenc will downscale the second pass if:
// 1. the specific pass is not given by commandline (aomenc will perform all
//    passes)
// 2. there are more than 2 passes in total
// 3. current pass is the second pass (the parameter pass starts with 0 so
//    pass == 1)
static int pass_need_downscale(int global_pass, int global_passes, int pass) {
  return !global_pass && global_passes > 2 && pass == 1;
}

int main(int argc, const char **argv_) {
  int pass;
  aom_image_t raw;
  aom_image_t raw_shift;
  int allocated_raw_shift = 0;
  int do_16bit_internal = 0;
  int input_shift = 0;
  int frame_avail, got_data;

  struct AvxInputContext input;
  struct AvxEncoderConfig global;
  struct stream_state *streams = NULL;
  char **argv, **argi;
  uint64_t cx_time = 0;
  int stream_cnt = 0;
  int res = 0;
  int profile_updated = 0;

  memset(&input, 0, sizeof(input));
  memset(&raw, 0, sizeof(raw));
  exec_name = argv_[0];

  /* Setup default input stream settings */
  input.framerate.numerator = 30;
  input.framerate.denominator = 1;
  input.only_i420 = 1;
  input.bit_depth = 0;

  /* First parse the global configuration values, because we want to apply
   * other parameters on top of the default configuration provided by the
   * codec.
   */
  argv = argv_dup(argc - 1, argv_ + 1);
  if (!argv) {
    fprintf(stderr, "Error allocating argument list\n");
    return EXIT_FAILURE;
  }
  parse_global_config(&global, &argv);

  if (argc < 2) usage_exit();

  switch (global.color_type) {
    case I420: input.fmt = AOM_IMG_FMT_I420; break;
    case I422: input.fmt = AOM_IMG_FMT_I422; break;
    case I444: input.fmt = AOM_IMG_FMT_I444; break;
    case YV12: input.fmt = AOM_IMG_FMT_YV12; break;
    case NV12: input.fmt = AOM_IMG_FMT_NV12; break;
  }

  {
    /* Now parse each stream's parameters. Using a local scope here
     * due to the use of 'stream' as loop variable in FOREACH_STREAM
     * loops
     */
    struct stream_state *stream = NULL;

    do {
      stream = new_stream(&global, stream);
      stream_cnt++;
      if (!streams) streams = stream;
    } while (parse_stream_params(&global, stream, argv));
  }

  /* Check for unrecognized options */
  for (argi = argv; *argi; argi++)
    if (argi[0][0] == '-' && argi[0][1])
      die("Error: Unrecognized option %s\n", *argi);

  FOREACH_STREAM(stream, streams) {
    check_encoder_config(global.disable_warning_prompt, &global,
                         &stream->config.cfg);

    // If large_scale_tile = 1, only support to output to ivf format.
    if (stream->config.cfg.large_scale_tile && !stream->config.write_ivf)
      die("only support ivf output format while large-scale-tile=1\n");
  }

  /* Handle non-option arguments */
  input.filename = argv[0];
  const char *orig_input_filename = input.filename;
  FOREACH_STREAM(stream, streams) {
    stream->orig_out_fn = stream->config.out_fn;
    stream->orig_width = stream->config.cfg.g_w;
    stream->orig_height = stream->config.cfg.g_h;
    stream->orig_write_ivf = stream->config.write_ivf;
    stream->orig_write_webm = stream->config.write_webm;
  }

  if (!input.filename) {
    fprintf(stderr, "No input file specified!\n");
    usage_exit();
  }

  /* Decide if other chroma subsamplings than 4:2:0 are supported */
  if (get_fourcc_by_aom_encoder(global.codec) == AV1_FOURCC)
    input.only_i420 = 0;

  for (pass = global.pass ? global.pass - 1 : 0; pass < global.passes; pass++) {
    if (pass > 1) {
      FOREACH_STREAM(stream, streams) { clear_stream_count_state(stream); }
    }

    int frames_in = 0, seen_frames = 0;
    int64_t estimated_time_left = -1;
    int64_t average_rate = -1;
    int64_t lagged_count = 0;
    const int need_downscale =
        pass_need_downscale(global.pass, global.passes, pass);

    // Set the output to the specified two-pass output file, and
    // restore the width and height to the original values.
    FOREACH_STREAM(stream, streams) {
      if (need_downscale) {
        stream->config.out_fn = stream->config.two_pass_output;
        // Libaom currently only supports the ivf format for the third pass.
        stream->config.write_ivf = 1;
        stream->config.write_webm = 0;
      } else {
        stream->config.out_fn = stream->orig_out_fn;
        stream->config.write_ivf = stream->orig_write_ivf;
        stream->config.write_webm = stream->orig_write_webm;
      }
      stream->config.cfg.g_w = stream->orig_width;
      stream->config.cfg.g_h = stream->orig_height;
    }

    // For second pass in three-pass encoding, set the input to
    // the given two-pass-input file if available. If the scaled input is not
    // given, we will attempt to re-scale the original input.
    input.filename = orig_input_filename;
    const char *two_pass_input = NULL;
    if (need_downscale) {
      FOREACH_STREAM(stream, streams) {
        if (stream->config.two_pass_input) {
          two_pass_input = stream->config.two_pass_input;
          input.filename = two_pass_input;
          break;
        }
      }
    }

    open_input_file(&input, global.csp);

    /* If the input file doesn't specify its w/h (raw files), try to get
     * the data from the first stream's configuration.
     */
    if (!input.width || !input.height) {
      if (two_pass_input) {
        FOREACH_STREAM(stream, streams) {
          if (stream->config.two_pass_width && stream->config.two_pass_height) {
            input.width = stream->config.two_pass_width;
            input.height = stream->config.two_pass_height;
            break;
          }
        }
      } else {
        FOREACH_STREAM(stream, streams) {
          if (stream->config.cfg.g_w && stream->config.cfg.g_h) {
            input.width = stream->config.cfg.g_w;
            input.height = stream->config.cfg.g_h;
            break;
          }
        }
      }
    }

    /* Update stream configurations from the input file's parameters */
    if (!input.width || !input.height) {
      if (two_pass_input) {
        fatal(
            "Specify downscaled stream dimensions with --two-pass-width "
            " and --two-pass-height");
      } else {
        fatal(
            "Specify stream dimensions with --width (-w) "
            " and --height (-h)");
      }
    }

    if (need_downscale) {
      FOREACH_STREAM(stream, streams) {
        if (stream->config.two_pass_width && stream->config.two_pass_height) {
          stream->config.cfg.g_w = stream->config.two_pass_width;
          stream->config.cfg.g_h = stream->config.two_pass_height;
        } else if (two_pass_input) {
          stream->config.cfg.g_w = input.width;
          stream->config.cfg.g_h = input.height;
        } else if (stream->orig_width && stream->orig_height) {
#if CONFIG_BITRATE_ACCURACY || CONFIG_BITRATE_ACCURACY_BL
          stream->config.cfg.g_w = stream->orig_width;
          stream->config.cfg.g_h = stream->orig_height;
#else   // CONFIG_BITRATE_ACCURACY || CONFIG_BITRATE_ACCURACY_BL
          stream->config.cfg.g_w = (stream->orig_width + 1) / 2;
          stream->config.cfg.g_h = (stream->orig_height + 1) / 2;
#endif  // CONFIG_BITRATE_ACCURACY || CONFIG_BITRATE_ACCURACY_BL
        } else {
#if CONFIG_BITRATE_ACCURACY || CONFIG_BITRATE_ACCURACY_BL
          stream->config.cfg.g_w = input.width;
          stream->config.cfg.g_h = input.height;
#else   // CONFIG_BITRATE_ACCURACY || CONFIG_BITRATE_ACCURACY_BL
          stream->config.cfg.g_w = (input.width + 1) / 2;
          stream->config.cfg.g_h = (input.height + 1) / 2;
#endif  // CONFIG_BITRATE_ACCURACY || CONFIG_BITRATE_ACCURACY_BL
        }
      }
    }

    /* If input file does not specify bit-depth but input-bit-depth parameter
     * exists, assume that to be the input bit-depth. However, if the
     * input-bit-depth paramter does not exist, assume the input bit-depth
     * to be the same as the codec bit-depth.
     */
    if (!input.bit_depth) {
      FOREACH_STREAM(stream, streams) {
        if (stream->config.cfg.g_input_bit_depth)
          input.bit_depth = stream->config.cfg.g_input_bit_depth;
        else
          input.bit_depth = stream->config.cfg.g_input_bit_depth =
              (int)stream->config.cfg.g_bit_depth;
      }
      if (input.bit_depth > 8) input.fmt |= AOM_IMG_FMT_HIGHBITDEPTH;
    } else {
      FOREACH_STREAM(stream, streams) {
        stream->config.cfg.g_input_bit_depth = input.bit_depth;
      }
    }

    FOREACH_STREAM(stream, streams) {
      if (input.fmt != AOM_IMG_FMT_I420 && input.fmt != AOM_IMG_FMT_I42016 &&
          input.fmt != AOM_IMG_FMT_NV12) {
        /* Automatically upgrade if input is non-4:2:0 but a 4:2:0 profile
           was selected. */
        switch (stream->config.cfg.g_profile) {
          case 0:
            if (input.bit_depth < 12 && (input.fmt == AOM_IMG_FMT_I444 ||
                                         input.fmt == AOM_IMG_FMT_I44416)) {
              if (!stream->config.cfg.monochrome) {
                stream->config.cfg.g_profile = 1;
                profile_updated = 1;
              }
            } else if (input.bit_depth == 12 ||
                       ((input.fmt == AOM_IMG_FMT_I422 ||
                         input.fmt == AOM_IMG_FMT_I42216) &&
                        !stream->config.cfg.monochrome)) {
              stream->config.cfg.g_profile = 2;
              profile_updated = 1;
            }
            break;
          case 1:
            if (input.bit_depth == 12 || input.fmt == AOM_IMG_FMT_I422 ||
                input.fmt == AOM_IMG_FMT_I42216) {
              stream->config.cfg.g_profile = 2;
              profile_updated = 1;
            } else if (input.bit_depth < 12 &&
                       (input.fmt == AOM_IMG_FMT_I420 ||
                        input.fmt == AOM_IMG_FMT_I42016)) {
              stream->config.cfg.g_profile = 0;
              profile_updated = 1;
            }
            break;
          case 2:
            if (input.bit_depth < 12 && (input.fmt == AOM_IMG_FMT_I444 ||
                                         input.fmt == AOM_IMG_FMT_I44416)) {
              stream->config.cfg.g_profile = 1;
              profile_updated = 1;
            } else if (input.bit_depth < 12 &&
                       (input.fmt == AOM_IMG_FMT_I420 ||
                        input.fmt == AOM_IMG_FMT_I42016)) {
              stream->config.cfg.g_profile = 0;
              profile_updated = 1;
            } else if (input.bit_depth == 12 &&
                       input.file_type == FILE_TYPE_Y4M) {
              // Note that here the input file values for chroma subsampling
              // are used instead of those from the command line.
              AOM_CODEC_CONTROL_TYPECHECKED(&stream->encoder,
                                            AV1E_SET_CHROMA_SUBSAMPLING_X,
                                            input.y4m.dst_c_dec_h >> 1);
              ctx_exit_on_error(&stream->encoder,
                                "Failed to set chroma subsampling x");
              AOM_CODEC_CONTROL_TYPECHECKED(&stream->encoder,
                                            AV1E_SET_CHROMA_SUBSAMPLING_Y,
                                            input.y4m.dst_c_dec_v >> 1);
              ctx_exit_on_error(&stream->encoder,
                                "Failed to set chroma subsampling y");
            } else if (input.bit_depth == 12 &&
                       input.file_type == FILE_TYPE_RAW) {
              AOM_CODEC_CONTROL_TYPECHECKED(&stream->encoder,
                                            AV1E_SET_CHROMA_SUBSAMPLING_X,
                                            stream->chroma_subsampling_x);
              ctx_exit_on_error(&stream->encoder,
                                "Failed to set chroma subsampling x");
              AOM_CODEC_CONTROL_TYPECHECKED(&stream->encoder,
                                            AV1E_SET_CHROMA_SUBSAMPLING_Y,
                                            stream->chroma_subsampling_y);
              ctx_exit_on_error(&stream->encoder,
                                "Failed to set chroma subsampling y");
            }
            break;
          default: break;
        }
      }
      /* Automatically set the codec bit depth to match the input bit depth.
       * Upgrade the profile if required. */
      if (stream->config.cfg.g_input_bit_depth >
          (unsigned int)stream->config.cfg.g_bit_depth) {
        stream->config.cfg.g_bit_depth = stream->config.cfg.g_input_bit_depth;
        if (!global.quiet) {
          fprintf(stderr,
                  "Warning: automatically updating bit depth to %d to "
                  "match input format.\n",
                  stream->config.cfg.g_input_bit_depth);
        }
      }
#if !CONFIG_AV1_HIGHBITDEPTH
      if (stream->config.cfg.g_bit_depth > 8) {
        fatal("Unsupported bit-depth with CONFIG_AV1_HIGHBITDEPTH=0\n");
      }
#endif  // CONFIG_AV1_HIGHBITDEPTH
      if (stream->config.cfg.g_bit_depth > 10) {
        switch (stream->config.cfg.g_profile) {
          case 0:
          case 1:
            stream->config.cfg.g_profile = 2;
            profile_updated = 1;
            break;
          default: break;
        }
      }
      if (stream->config.cfg.g_bit_depth > 8) {
        stream->config.use_16bit_internal = 1;
      }
      if (profile_updated && !global.quiet) {
        fprintf(stderr,
                "Warning: automatically updating to profile %d to "
                "match input format.\n",
                stream->config.cfg.g_profile);
      }
      if ((global.show_psnr == 2) && (stream->config.cfg.g_input_bit_depth ==
                                      stream->config.cfg.g_bit_depth)) {
        fprintf(stderr,
                "Warning: --psnr==2 and --psnr==1 will provide same "
                "results when input bit-depth == stream bit-depth, "
                "falling back to default psnr value\n");
        global.show_psnr = 1;
      }
      if (global.show_psnr < 0 || global.show_psnr > 2) {
        fprintf(stderr,
                "Warning: --psnr can take only 0,1,2 as values,"
                "falling back to default psnr value\n");
        global.show_psnr = 1;
      }
      /* Set limit */
      stream->config.cfg.g_limit = global.limit;
    }

    FOREACH_STREAM(stream, streams) {
      set_stream_dimensions(stream, input.width, input.height);
      stream->config.color_range = input.color_range;
    }
    FOREACH_STREAM(stream, streams) { validate_stream_config(stream, &global); }

    /* Ensure that --passes and --pass are consistent. If --pass is set and
     * --passes >= 2, ensure --fpf was set.
     */
    if (global.pass > 0 && global.pass <= 3 && global.passes >= 2) {
      FOREACH_STREAM(stream, streams) {
        if (!stream->config.stats_fn)
          die("Stream %d: Must specify --fpf when --pass=%d"
              " and --passes=%d\n",
              stream->index, global.pass, global.passes);
      }
    }

#if !CONFIG_WEBM_IO
    FOREACH_STREAM(stream, streams) {
      if (stream->config.write_webm) {
        stream->config.write_webm = 0;
        stream->config.write_ivf = 0;
        aom_tools_warn("aomenc compiled w/o WebM support. Writing OBU stream.");
      }
    }
#endif

    /* Use the frame rate from the file only if none was specified
     * on the command-line.
     */
    if (!global.have_framerate) {
      global.framerate.num = input.framerate.numerator;
      global.framerate.den = input.framerate.denominator;
    }
    FOREACH_STREAM(stream, streams) {
      stream->config.cfg.g_timebase.den = global.framerate.num;
      stream->config.cfg.g_timebase.num = global.framerate.den;
    }
    /* Show configuration */
    if (global.verbose && pass == 0) {
      FOREACH_STREAM(stream, streams) {
        show_stream_config(stream, &global, &input);
      }
    }

    if (pass == (global.pass ? global.pass - 1 : 0)) {
      // The Y4M reader does its own allocation.
      if (input.file_type != FILE_TYPE_Y4M) {
        aom_img_alloc(&raw, input.fmt, input.width, input.height, 32);
      }
      FOREACH_STREAM(stream, streams) {
        stream->rate_hist =
            init_rate_histogram(&stream->config.cfg, &global.framerate);
      }
    }

    FOREACH_STREAM(stream, streams) { setup_pass(stream, &global, pass); }
    FOREACH_STREAM(stream, streams) { initialize_encoder(stream, &global); }
    FOREACH_STREAM(stream, streams) {
      char *encoder_settings = NULL;
#if CONFIG_WEBM_IO
      // Test frameworks may compare outputs from different versions, but only
      // wish to check for bitstream changes. The encoder-settings tag, however,
      // can vary if the version is updated, even if no encoder algorithm
      // changes were made. To work around this issue, do not output
      // the encoder-settings tag when --debug is enabled (which is the flag
      // that test frameworks should use, when they want deterministic output
      // from the container format).
      if (stream->config.write_webm && !stream->webm_ctx.debug) {
        encoder_settings = extract_encoder_settings(
            aom_codec_version_str(), argv_, argc, input.filename);
        if (encoder_settings == NULL) {
          fprintf(
              stderr,
              "Warning: unable to extract encoder settings. Continuing...\n");
        }
      }
#endif
      open_output_file(stream, &global, &input.pixel_aspect_ratio,
                       encoder_settings);
      free(encoder_settings);
    }

    if (strcmp(get_short_name_by_aom_encoder(global.codec), "av1") == 0) {
      // Check to see if at least one stream uses 16 bit internal.
      // Currently assume that the bit_depths for all streams using
      // highbitdepth are the same.
      FOREACH_STREAM(stream, streams) {
        if (stream->config.use_16bit_internal) {
          do_16bit_internal = 1;
        }
        input_shift = (int)stream->config.cfg.g_bit_depth -
                      stream->config.cfg.g_input_bit_depth;
      }
    }

    frame_avail = 1;
    got_data = 0;

    while (frame_avail || got_data) {
      struct aom_usec_timer timer;

      if (!global.limit || frames_in < global.limit) {
        frame_avail = read_frame(&input, &raw);

        if (frame_avail) frames_in++;
        seen_frames =
            frames_in > global.skip_frames ? frames_in - global.skip_frames : 0;

        if (!global.quiet) {
          float fps = usec_to_fps(cx_time, seen_frames);
          fprintf(stderr, "\rPass %d/%d ", pass + 1, global.passes);

          if (stream_cnt == 1)
            fprintf(stderr, "frame %4d/%-4d %7" PRId64 "B ", frames_in,
                    streams->frames_out, (int64_t)streams->nbytes);
          else
            fprintf(stderr, "frame %4d ", frames_in);

          fprintf(stderr, "%7" PRId64 " %s %.2f %s ",
                  cx_time > 9999999 ? cx_time / 1000 : cx_time,
                  cx_time > 9999999 ? "ms" : "us", fps >= 1.0 ? fps : fps * 60,
                  fps >= 1.0 ? "fps" : "fpm");
          print_time("ETA", estimated_time_left);
          // mingw-w64 gcc does not match msvc for stderr buffering behavior
          // and uses line buffering, thus the progress output is not
          // real-time. The fflush() is here to make sure the progress output
          // is sent out while the clip is being processed.
          fflush(stderr);
        }

      } else {
        frame_avail = 0;
      }

      if (frames_in > global.skip_frames) {
        aom_image_t *frame_to_encode;
        if (input_shift || (do_16bit_internal && input.bit_depth == 8)) {
          assert(do_16bit_internal);
          // Input bit depth and stream bit depth do not match, so up
          // shift frame to stream bit depth
          if (!allocated_raw_shift) {
            aom_img_alloc(&raw_shift, raw.fmt | AOM_IMG_FMT_HIGHBITDEPTH,
                          input.width, input.height, 32);
            allocated_raw_shift = 1;
          }
          aom_img_upshift(&raw_shift, &raw, input_shift);
          frame_to_encode = &raw_shift;
        } else {
          frame_to_encode = &raw;
        }
        aom_usec_timer_start(&timer);
        if (do_16bit_internal) {
          assert(frame_to_encode->fmt & AOM_IMG_FMT_HIGHBITDEPTH);
          FOREACH_STREAM(stream, streams) {
            if (stream->config.use_16bit_internal)
              encode_frame(stream, &global,
                           frame_avail ? frame_to_encode : NULL, frames_in);
            else
              assert(0);
          }
        } else {
          assert((frame_to_encode->fmt & AOM_IMG_FMT_HIGHBITDEPTH) == 0);
          FOREACH_STREAM(stream, streams) {
            encode_frame(stream, &global, frame_avail ? frame_to_encode : NULL,
                         frames_in);
          }
        }
        aom_usec_timer_mark(&timer);
        cx_time += aom_usec_timer_elapsed(&timer);

        FOREACH_STREAM(stream, streams) { update_quantizer_histogram(stream); }

        got_data = 0;
        FOREACH_STREAM(stream, streams) {
          get_cx_data(stream, &global, &got_data);
        }

        if (!got_data && input.length && streams != NULL &&
            !streams->frames_out) {
          lagged_count = global.limit ? seen_frames : ftello(input.file);
        } else if (input.length) {
          int64_t remaining;
          int64_t rate;

          if (global.limit) {
            const int64_t frame_in_lagged = (seen_frames - lagged_count) * 1000;

            rate = cx_time ? frame_in_lagged * (int64_t)1000000 / cx_time : 0;
            remaining = 1000 * (global.limit - global.skip_frames -
                                seen_frames + lagged_count);
          } else {
            const int64_t input_pos = ftello(input.file);
            const int64_t input_pos_lagged = input_pos - lagged_count;
            const int64_t input_limit = input.length;

            rate = cx_time ? input_pos_lagged * (int64_t)1000000 / cx_time : 0;
            remaining = input_limit - input_pos + lagged_count;
          }

          average_rate =
              (average_rate <= 0) ? rate : (average_rate * 7 + rate) / 8;
          estimated_time_left = average_rate ? remaining / average_rate : -1;
        }

        if (got_data && global.test_decode != TEST_DECODE_OFF) {
          FOREACH_STREAM(stream, streams) {
            test_decode(stream, global.test_decode);
          }
        }
      }

      fflush(stdout);
      if (!global.quiet) fprintf(stderr, "\033[K");
    }

    if (stream_cnt > 1) fprintf(stderr, "\n");

    if (!global.quiet) {
      FOREACH_STREAM(stream, streams) {
        const int64_t bpf =
            seen_frames ? (int64_t)(stream->nbytes * 8 / seen_frames) : 0;
        const int64_t bps = bpf * global.framerate.num / global.framerate.den;
        fprintf(stderr,
                "\rPass %d/%d frame %4d/%-4d %7" PRId64 "B %7" PRId64
                "b/f %7" PRId64
                "b/s"
                " %7" PRId64 " %s (%.2f fps)\033[K\n",
                pass + 1, global.passes, frames_in, stream->frames_out,
                (int64_t)stream->nbytes, bpf, bps,
                stream->cx_time > 9999999 ? stream->cx_time / 1000
                                          : stream->cx_time,
                stream->cx_time > 9999999 ? "ms" : "us",
                usec_to_fps(stream->cx_time, seen_frames));
        // This instance of cr does not need fflush as it is followed by a
        // newline in the same string.
      }
    }

    if (global.show_psnr >= 1) {
      if (get_fourcc_by_aom_encoder(global.codec) == AV1_FOURCC) {
        FOREACH_STREAM(stream, streams) {
          int64_t bps = 0;
          if (global.show_psnr == 1) {
            if (stream->psnr_count[0] && seen_frames && global.framerate.den) {
              bps = (int64_t)stream->nbytes * 8 *
                    (int64_t)global.framerate.num / global.framerate.den /
                    seen_frames;
            }
            show_psnr(stream, (1 << stream->config.cfg.g_input_bit_depth) - 1,
                      bps);
          }
          if (global.show_psnr == 2) {
#if CONFIG_AV1_HIGHBITDEPTH
            if (stream->config.cfg.g_input_bit_depth <
                (unsigned int)stream->config.cfg.g_bit_depth)
              show_psnr_hbd(stream, (1 << stream->config.cfg.g_bit_depth) - 1,
                            bps);
#endif
          }
        }
      } else {
        FOREACH_STREAM(stream, streams) { show_psnr(stream, 255.0, 0); }
      }
    }

    if (pass == global.passes - 1) {
      FOREACH_STREAM(stream, streams) {
        int num_operating_points;
        int levels[32];
        int target_levels[32];
        aom_codec_control(&stream->encoder, AV1E_GET_NUM_OPERATING_POINTS,
                          &num_operating_points);
        aom_codec_control(&stream->encoder, AV1E_GET_SEQ_LEVEL_IDX, levels);
        aom_codec_control(&stream->encoder, AV1E_GET_TARGET_SEQ_LEVEL_IDX,
                          target_levels);

        for (int i = 0; i < num_operating_points; i++) {
          if (levels[i] > target_levels[i]) {
            if (levels[i] == 31) {
              aom_tools_warn(
                  "Failed to encode to target level %d.%d for operating point "
                  "%d. The output level is SEQ_LEVEL_MAX",
                  2 + (target_levels[i] >> 2), target_levels[i] & 3, i);
            } else {
              aom_tools_warn(
                  "Failed to encode to target level %d.%d for operating point "
                  "%d. The output level is %d.%d",
                  2 + (target_levels[i] >> 2), target_levels[i] & 3, i,
                  2 + (levels[i] >> 2), levels[i] & 3);
            }
          }
        }
      }
    }

    FOREACH_STREAM(stream, streams) { aom_codec_destroy(&stream->encoder); }

    if (global.test_decode != TEST_DECODE_OFF) {
      FOREACH_STREAM(stream, streams) { aom_codec_destroy(&stream->decoder); }
    }

    close_input_file(&input);

    if (global.test_decode == TEST_DECODE_FATAL) {
      FOREACH_STREAM(stream, streams) { res |= stream->mismatch_seen; }
    }
    FOREACH_STREAM(stream, streams) {
      close_output_file(stream, get_fourcc_by_aom_encoder(global.codec));
    }

    FOREACH_STREAM(stream, streams) {
      stats_close(&stream->stats, global.passes - 1);
    }

    if (global.pass) break;
  }

  if (global.show_q_hist_buckets) {
    FOREACH_STREAM(stream, streams) {
      show_q_histogram(stream->counts, global.show_q_hist_buckets);
    }
  }

  if (global.show_rate_hist_buckets) {
    FOREACH_STREAM(stream, streams) {
      show_rate_histogram(stream->rate_hist, &stream->config.cfg,
                          global.show_rate_hist_buckets);
    }
  }
  FOREACH_STREAM(stream, streams) { destroy_rate_histogram(stream->rate_hist); }

#if CONFIG_INTERNAL_STATS
  /* TODO(jkoleszar): This doesn't belong in this executable. Do it for now,
   * to match some existing utilities.
   */
  if (!(global.pass == 1 && global.passes == 2)) {
    FOREACH_STREAM(stream, streams) {
      FILE *f = fopen("opsnr.stt", "a");
      if (stream->mismatch_seen) {
        fprintf(f, "First mismatch occurred in frame %d\n",
                stream->mismatch_seen);
      } else {
        fprintf(f, "No mismatch detected in recon buffers\n");
      }
      fclose(f);
    }
  }
#endif

  if (allocated_raw_shift) aom_img_free(&raw_shift);
  aom_img_free(&raw);
  free(argv);
  free(streams);
  return res ? EXIT_FAILURE : EXIT_SUCCESS;
}
