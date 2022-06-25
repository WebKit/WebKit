/*
 * Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "./vp9_rtcd.h"
#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "./vpx_scale_rtcd.h"
#include "vpx_dsp/psnr.h"
#include "vpx_dsp/vpx_dsp_common.h"
#include "vpx_dsp/vpx_filter.h"
#if CONFIG_INTERNAL_STATS
#include "vpx_dsp/ssim.h"
#endif
#include "vpx_ports/mem.h"
#include "vpx_ports/system_state.h"
#include "vpx_ports/vpx_timer.h"
#if CONFIG_BITSTREAM_DEBUG || CONFIG_MISMATCH_DEBUG
#include "vpx_util/vpx_debug_util.h"
#endif  // CONFIG_BITSTREAM_DEBUG || CONFIG_MISMATCH_DEBUG

#include "vp9/common/vp9_alloccommon.h"
#include "vp9/common/vp9_filter.h"
#include "vp9/common/vp9_idct.h"
#if CONFIG_NON_GREEDY_MV
#include "vp9/common/vp9_mvref_common.h"
#endif
#if CONFIG_VP9_POSTPROC
#include "vp9/common/vp9_postproc.h"
#endif
#include "vp9/common/vp9_reconinter.h"
#include "vp9/common/vp9_reconintra.h"
#include "vp9/common/vp9_tile_common.h"
#include "vp9/common/vp9_scan.h"

#if !CONFIG_REALTIME_ONLY
#include "vp9/encoder/vp9_alt_ref_aq.h"
#include "vp9/encoder/vp9_aq_360.h"
#include "vp9/encoder/vp9_aq_complexity.h"
#endif
#include "vp9/encoder/vp9_aq_cyclicrefresh.h"
#if !CONFIG_REALTIME_ONLY
#include "vp9/encoder/vp9_aq_variance.h"
#endif
#include "vp9/encoder/vp9_bitstream.h"
#if CONFIG_INTERNAL_STATS
#include "vp9/encoder/vp9_blockiness.h"
#endif
#include "vp9/encoder/vp9_context_tree.h"
#include "vp9/encoder/vp9_encodeframe.h"
#include "vp9/encoder/vp9_encodemb.h"
#include "vp9/encoder/vp9_encodemv.h"
#include "vp9/encoder/vp9_encoder.h"
#include "vp9/encoder/vp9_ethread.h"
#include "vp9/encoder/vp9_extend.h"
#include "vp9/encoder/vp9_firstpass.h"
#include "vp9/encoder/vp9_mbgraph.h"
#if CONFIG_NON_GREEDY_MV
#include "vp9/encoder/vp9_mcomp.h"
#endif
#include "vp9/encoder/vp9_multi_thread.h"
#include "vp9/encoder/vp9_noise_estimate.h"
#include "vp9/encoder/vp9_picklpf.h"
#include "vp9/encoder/vp9_ratectrl.h"
#include "vp9/encoder/vp9_rd.h"
#include "vp9/encoder/vp9_resize.h"
#include "vp9/encoder/vp9_segmentation.h"
#include "vp9/encoder/vp9_skin_detection.h"
#include "vp9/encoder/vp9_speed_features.h"
#include "vp9/encoder/vp9_svc_layercontext.h"
#include "vp9/encoder/vp9_temporal_filter.h"
#include "vp9/vp9_cx_iface.h"

#define AM_SEGMENT_ID_INACTIVE 7
#define AM_SEGMENT_ID_ACTIVE 0

// Whether to use high precision mv for altref computation.
#define ALTREF_HIGH_PRECISION_MV 1

// Q threshold for high precision mv. Choose a very high value for now so that
// HIGH_PRECISION is always chosen.
#define HIGH_PRECISION_MV_QTHRESH 200

#define FRAME_SIZE_FACTOR 128  // empirical params for context model threshold
#define FRAME_RATE_FACTOR 8

#ifdef OUTPUT_YUV_DENOISED
FILE *yuv_denoised_file = NULL;
#endif
#ifdef OUTPUT_YUV_SKINMAP
static FILE *yuv_skinmap_file = NULL;
#endif
#ifdef OUTPUT_YUV_REC
FILE *yuv_rec_file;
#endif
#ifdef OUTPUT_YUV_SVC_SRC
FILE *yuv_svc_src[3] = { NULL, NULL, NULL };
#endif

#if 0
FILE *framepsnr;
FILE *kf_list;
FILE *keyfile;
#endif

#ifdef ENABLE_KF_DENOISE
// Test condition for spatial denoise of source.
static int is_spatial_denoise_enabled(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;

  return (oxcf->pass != 1) && !is_lossless_requested(&cpi->oxcf) &&
         frame_is_intra_only(cm);
}
#endif

#if CONFIG_VP9_HIGHBITDEPTH
void highbd_wht_fwd_txfm(int16_t *src_diff, int bw, tran_low_t *coeff,
                         TX_SIZE tx_size);
#endif
void wht_fwd_txfm(int16_t *src_diff, int bw, tran_low_t *coeff,
                  TX_SIZE tx_size);

#if !CONFIG_REALTIME_ONLY
// compute adaptive threshold for skip recoding
static int compute_context_model_thresh(const VP9_COMP *const cpi) {
  const VP9_COMMON *const cm = &cpi->common;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  const int frame_size = (cm->width * cm->height) >> 10;
  const int bitrate = (int)(oxcf->target_bandwidth >> 10);
  const int qindex_factor = cm->base_qindex + (MAXQ >> 1);

  // This equation makes the threshold adaptive to frame size.
  // Coding gain obtained by recoding comes from alternate frames of large
  // content change. We skip recoding if the difference of previous and current
  // frame context probability model is less than a certain threshold.
  // The first component is the most critical part to guarantee adaptivity.
  // Other parameters are estimated based on normal setting of hd resolution
  // parameters. e.g frame_size = 1920x1080, bitrate = 8000, qindex_factor < 50
  const int thresh =
      ((FRAME_SIZE_FACTOR * frame_size - FRAME_RATE_FACTOR * bitrate) *
       qindex_factor) >>
      9;

  return thresh;
}

// compute the total cost difference between current
// and previous frame context prob model.
static int compute_context_model_diff(const VP9_COMMON *const cm) {
  const FRAME_CONTEXT *const pre_fc =
      &cm->frame_contexts[cm->frame_context_idx];
  const FRAME_CONTEXT *const cur_fc = cm->fc;
  const FRAME_COUNTS *counts = &cm->counts;
  vpx_prob pre_last_prob, cur_last_prob;
  int diff = 0;
  int i, j, k, l, m, n;

  // y_mode_prob
  for (i = 0; i < BLOCK_SIZE_GROUPS; ++i) {
    for (j = 0; j < INTRA_MODES - 1; ++j) {
      diff += (int)counts->y_mode[i][j] *
              (pre_fc->y_mode_prob[i][j] - cur_fc->y_mode_prob[i][j]);
    }
    pre_last_prob = MAX_PROB - pre_fc->y_mode_prob[i][INTRA_MODES - 2];
    cur_last_prob = MAX_PROB - cur_fc->y_mode_prob[i][INTRA_MODES - 2];

    diff += (int)counts->y_mode[i][INTRA_MODES - 1] *
            (pre_last_prob - cur_last_prob);
  }

  // uv_mode_prob
  for (i = 0; i < INTRA_MODES; ++i) {
    for (j = 0; j < INTRA_MODES - 1; ++j) {
      diff += (int)counts->uv_mode[i][j] *
              (pre_fc->uv_mode_prob[i][j] - cur_fc->uv_mode_prob[i][j]);
    }
    pre_last_prob = MAX_PROB - pre_fc->uv_mode_prob[i][INTRA_MODES - 2];
    cur_last_prob = MAX_PROB - cur_fc->uv_mode_prob[i][INTRA_MODES - 2];

    diff += (int)counts->uv_mode[i][INTRA_MODES - 1] *
            (pre_last_prob - cur_last_prob);
  }

  // partition_prob
  for (i = 0; i < PARTITION_CONTEXTS; ++i) {
    for (j = 0; j < PARTITION_TYPES - 1; ++j) {
      diff += (int)counts->partition[i][j] *
              (pre_fc->partition_prob[i][j] - cur_fc->partition_prob[i][j]);
    }
    pre_last_prob = MAX_PROB - pre_fc->partition_prob[i][PARTITION_TYPES - 2];
    cur_last_prob = MAX_PROB - cur_fc->partition_prob[i][PARTITION_TYPES - 2];

    diff += (int)counts->partition[i][PARTITION_TYPES - 1] *
            (pre_last_prob - cur_last_prob);
  }

  // coef_probs
  for (i = 0; i < TX_SIZES; ++i) {
    for (j = 0; j < PLANE_TYPES; ++j) {
      for (k = 0; k < REF_TYPES; ++k) {
        for (l = 0; l < COEF_BANDS; ++l) {
          for (m = 0; m < BAND_COEFF_CONTEXTS(l); ++m) {
            for (n = 0; n < UNCONSTRAINED_NODES; ++n) {
              diff += (int)counts->coef[i][j][k][l][m][n] *
                      (pre_fc->coef_probs[i][j][k][l][m][n] -
                       cur_fc->coef_probs[i][j][k][l][m][n]);
            }

            pre_last_prob =
                MAX_PROB -
                pre_fc->coef_probs[i][j][k][l][m][UNCONSTRAINED_NODES - 1];
            cur_last_prob =
                MAX_PROB -
                cur_fc->coef_probs[i][j][k][l][m][UNCONSTRAINED_NODES - 1];

            diff += (int)counts->coef[i][j][k][l][m][UNCONSTRAINED_NODES] *
                    (pre_last_prob - cur_last_prob);
          }
        }
      }
    }
  }

  // switchable_interp_prob
  for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; ++i) {
    for (j = 0; j < SWITCHABLE_FILTERS - 1; ++j) {
      diff += (int)counts->switchable_interp[i][j] *
              (pre_fc->switchable_interp_prob[i][j] -
               cur_fc->switchable_interp_prob[i][j]);
    }
    pre_last_prob =
        MAX_PROB - pre_fc->switchable_interp_prob[i][SWITCHABLE_FILTERS - 2];
    cur_last_prob =
        MAX_PROB - cur_fc->switchable_interp_prob[i][SWITCHABLE_FILTERS - 2];

    diff += (int)counts->switchable_interp[i][SWITCHABLE_FILTERS - 1] *
            (pre_last_prob - cur_last_prob);
  }

  // inter_mode_probs
  for (i = 0; i < INTER_MODE_CONTEXTS; ++i) {
    for (j = 0; j < INTER_MODES - 1; ++j) {
      diff += (int)counts->inter_mode[i][j] *
              (pre_fc->inter_mode_probs[i][j] - cur_fc->inter_mode_probs[i][j]);
    }
    pre_last_prob = MAX_PROB - pre_fc->inter_mode_probs[i][INTER_MODES - 2];
    cur_last_prob = MAX_PROB - cur_fc->inter_mode_probs[i][INTER_MODES - 2];

    diff += (int)counts->inter_mode[i][INTER_MODES - 1] *
            (pre_last_prob - cur_last_prob);
  }

  // intra_inter_prob
  for (i = 0; i < INTRA_INTER_CONTEXTS; ++i) {
    diff += (int)counts->intra_inter[i][0] *
            (pre_fc->intra_inter_prob[i] - cur_fc->intra_inter_prob[i]);

    pre_last_prob = MAX_PROB - pre_fc->intra_inter_prob[i];
    cur_last_prob = MAX_PROB - cur_fc->intra_inter_prob[i];

    diff += (int)counts->intra_inter[i][1] * (pre_last_prob - cur_last_prob);
  }

  // comp_inter_prob
  for (i = 0; i < COMP_INTER_CONTEXTS; ++i) {
    diff += (int)counts->comp_inter[i][0] *
            (pre_fc->comp_inter_prob[i] - cur_fc->comp_inter_prob[i]);

    pre_last_prob = MAX_PROB - pre_fc->comp_inter_prob[i];
    cur_last_prob = MAX_PROB - cur_fc->comp_inter_prob[i];

    diff += (int)counts->comp_inter[i][1] * (pre_last_prob - cur_last_prob);
  }

  // single_ref_prob
  for (i = 0; i < REF_CONTEXTS; ++i) {
    for (j = 0; j < 2; ++j) {
      diff += (int)counts->single_ref[i][j][0] *
              (pre_fc->single_ref_prob[i][j] - cur_fc->single_ref_prob[i][j]);

      pre_last_prob = MAX_PROB - pre_fc->single_ref_prob[i][j];
      cur_last_prob = MAX_PROB - cur_fc->single_ref_prob[i][j];

      diff +=
          (int)counts->single_ref[i][j][1] * (pre_last_prob - cur_last_prob);
    }
  }

  // comp_ref_prob
  for (i = 0; i < REF_CONTEXTS; ++i) {
    diff += (int)counts->comp_ref[i][0] *
            (pre_fc->comp_ref_prob[i] - cur_fc->comp_ref_prob[i]);

    pre_last_prob = MAX_PROB - pre_fc->comp_ref_prob[i];
    cur_last_prob = MAX_PROB - cur_fc->comp_ref_prob[i];

    diff += (int)counts->comp_ref[i][1] * (pre_last_prob - cur_last_prob);
  }

  // tx_probs
  for (i = 0; i < TX_SIZE_CONTEXTS; ++i) {
    // p32x32
    for (j = 0; j < TX_SIZES - 1; ++j) {
      diff += (int)counts->tx.p32x32[i][j] *
              (pre_fc->tx_probs.p32x32[i][j] - cur_fc->tx_probs.p32x32[i][j]);
    }
    pre_last_prob = MAX_PROB - pre_fc->tx_probs.p32x32[i][TX_SIZES - 2];
    cur_last_prob = MAX_PROB - cur_fc->tx_probs.p32x32[i][TX_SIZES - 2];

    diff += (int)counts->tx.p32x32[i][TX_SIZES - 1] *
            (pre_last_prob - cur_last_prob);

    // p16x16
    for (j = 0; j < TX_SIZES - 2; ++j) {
      diff += (int)counts->tx.p16x16[i][j] *
              (pre_fc->tx_probs.p16x16[i][j] - cur_fc->tx_probs.p16x16[i][j]);
    }
    pre_last_prob = MAX_PROB - pre_fc->tx_probs.p16x16[i][TX_SIZES - 3];
    cur_last_prob = MAX_PROB - cur_fc->tx_probs.p16x16[i][TX_SIZES - 3];

    diff += (int)counts->tx.p16x16[i][TX_SIZES - 2] *
            (pre_last_prob - cur_last_prob);

    // p8x8
    for (j = 0; j < TX_SIZES - 3; ++j) {
      diff += (int)counts->tx.p8x8[i][j] *
              (pre_fc->tx_probs.p8x8[i][j] - cur_fc->tx_probs.p8x8[i][j]);
    }
    pre_last_prob = MAX_PROB - pre_fc->tx_probs.p8x8[i][TX_SIZES - 4];
    cur_last_prob = MAX_PROB - cur_fc->tx_probs.p8x8[i][TX_SIZES - 4];

    diff +=
        (int)counts->tx.p8x8[i][TX_SIZES - 3] * (pre_last_prob - cur_last_prob);
  }

  // skip_probs
  for (i = 0; i < SKIP_CONTEXTS; ++i) {
    diff += (int)counts->skip[i][0] *
            (pre_fc->skip_probs[i] - cur_fc->skip_probs[i]);

    pre_last_prob = MAX_PROB - pre_fc->skip_probs[i];
    cur_last_prob = MAX_PROB - cur_fc->skip_probs[i];

    diff += (int)counts->skip[i][1] * (pre_last_prob - cur_last_prob);
  }

  // mv
  for (i = 0; i < MV_JOINTS - 1; ++i) {
    diff += (int)counts->mv.joints[i] *
            (pre_fc->nmvc.joints[i] - cur_fc->nmvc.joints[i]);
  }
  pre_last_prob = MAX_PROB - pre_fc->nmvc.joints[MV_JOINTS - 2];
  cur_last_prob = MAX_PROB - cur_fc->nmvc.joints[MV_JOINTS - 2];

  diff +=
      (int)counts->mv.joints[MV_JOINTS - 1] * (pre_last_prob - cur_last_prob);

  for (i = 0; i < 2; ++i) {
    const nmv_component_counts *nmv_count = &counts->mv.comps[i];
    const nmv_component *pre_nmv_prob = &pre_fc->nmvc.comps[i];
    const nmv_component *cur_nmv_prob = &cur_fc->nmvc.comps[i];

    // sign
    diff += (int)nmv_count->sign[0] * (pre_nmv_prob->sign - cur_nmv_prob->sign);

    pre_last_prob = MAX_PROB - pre_nmv_prob->sign;
    cur_last_prob = MAX_PROB - cur_nmv_prob->sign;

    diff += (int)nmv_count->sign[1] * (pre_last_prob - cur_last_prob);

    // classes
    for (j = 0; j < MV_CLASSES - 1; ++j) {
      diff += (int)nmv_count->classes[j] *
              (pre_nmv_prob->classes[j] - cur_nmv_prob->classes[j]);
    }
    pre_last_prob = MAX_PROB - pre_nmv_prob->classes[MV_CLASSES - 2];
    cur_last_prob = MAX_PROB - cur_nmv_prob->classes[MV_CLASSES - 2];

    diff += (int)nmv_count->classes[MV_CLASSES - 1] *
            (pre_last_prob - cur_last_prob);

    // class0
    for (j = 0; j < CLASS0_SIZE - 1; ++j) {
      diff += (int)nmv_count->class0[j] *
              (pre_nmv_prob->class0[j] - cur_nmv_prob->class0[j]);
    }
    pre_last_prob = MAX_PROB - pre_nmv_prob->class0[CLASS0_SIZE - 2];
    cur_last_prob = MAX_PROB - cur_nmv_prob->class0[CLASS0_SIZE - 2];

    diff += (int)nmv_count->class0[CLASS0_SIZE - 1] *
            (pre_last_prob - cur_last_prob);

    // bits
    for (j = 0; j < MV_OFFSET_BITS; ++j) {
      diff += (int)nmv_count->bits[j][0] *
              (pre_nmv_prob->bits[j] - cur_nmv_prob->bits[j]);

      pre_last_prob = MAX_PROB - pre_nmv_prob->bits[j];
      cur_last_prob = MAX_PROB - cur_nmv_prob->bits[j];

      diff += (int)nmv_count->bits[j][1] * (pre_last_prob - cur_last_prob);
    }

    // class0_fp
    for (j = 0; j < CLASS0_SIZE; ++j) {
      for (k = 0; k < MV_FP_SIZE - 1; ++k) {
        diff += (int)nmv_count->class0_fp[j][k] *
                (pre_nmv_prob->class0_fp[j][k] - cur_nmv_prob->class0_fp[j][k]);
      }
      pre_last_prob = MAX_PROB - pre_nmv_prob->class0_fp[j][MV_FP_SIZE - 2];
      cur_last_prob = MAX_PROB - cur_nmv_prob->class0_fp[j][MV_FP_SIZE - 2];

      diff += (int)nmv_count->class0_fp[j][MV_FP_SIZE - 1] *
              (pre_last_prob - cur_last_prob);
    }

    // fp
    for (j = 0; j < MV_FP_SIZE - 1; ++j) {
      diff +=
          (int)nmv_count->fp[j] * (pre_nmv_prob->fp[j] - cur_nmv_prob->fp[j]);
    }
    pre_last_prob = MAX_PROB - pre_nmv_prob->fp[MV_FP_SIZE - 2];
    cur_last_prob = MAX_PROB - cur_nmv_prob->fp[MV_FP_SIZE - 2];

    diff +=
        (int)nmv_count->fp[MV_FP_SIZE - 1] * (pre_last_prob - cur_last_prob);

    // class0_hp
    diff += (int)nmv_count->class0_hp[0] *
            (pre_nmv_prob->class0_hp - cur_nmv_prob->class0_hp);

    pre_last_prob = MAX_PROB - pre_nmv_prob->class0_hp;
    cur_last_prob = MAX_PROB - cur_nmv_prob->class0_hp;

    diff += (int)nmv_count->class0_hp[1] * (pre_last_prob - cur_last_prob);

    // hp
    diff += (int)nmv_count->hp[0] * (pre_nmv_prob->hp - cur_nmv_prob->hp);

    pre_last_prob = MAX_PROB - pre_nmv_prob->hp;
    cur_last_prob = MAX_PROB - cur_nmv_prob->hp;

    diff += (int)nmv_count->hp[1] * (pre_last_prob - cur_last_prob);
  }

  return -diff;
}
#endif  // !CONFIG_REALTIME_ONLY

// Test for whether to calculate metrics for the frame.
static int is_psnr_calc_enabled(const VP9_COMP *cpi) {
  const VP9_COMMON *const cm = &cpi->common;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;

  return cpi->b_calculate_psnr && (oxcf->pass != 1) && cm->show_frame;
}

/* clang-format off */
const Vp9LevelSpec vp9_level_defs[VP9_LEVELS] = {
  //         sample rate    size   breadth  bitrate  cpb
  { LEVEL_1,   829440,      36864,    512,   200,    400,    2, 1,  4,  8 },
  { LEVEL_1_1, 2764800,     73728,    768,   800,    1000,   2, 1,  4,  8 },
  { LEVEL_2,   4608000,     122880,   960,   1800,   1500,   2, 1,  4,  8 },
  { LEVEL_2_1, 9216000,     245760,   1344,  3600,   2800,   2, 2,  4,  8 },
  { LEVEL_3,   20736000,    552960,   2048,  7200,   6000,   2, 4,  4,  8 },
  { LEVEL_3_1, 36864000,    983040,   2752,  12000,  10000,  2, 4,  4,  8 },
  { LEVEL_4,   83558400,    2228224,  4160,  18000,  16000,  4, 4,  4,  8 },
  { LEVEL_4_1, 160432128,   2228224,  4160,  30000,  18000,  4, 4,  5,  6 },
  { LEVEL_5,   311951360,   8912896,  8384,  60000,  36000,  6, 8,  6,  4 },
  { LEVEL_5_1, 588251136,   8912896,  8384,  120000, 46000,  8, 8,  10, 4 },
  // TODO(huisu): update max_cpb_size for level 5_2 ~ 6_2 when
  // they are finalized (currently tentative).
  { LEVEL_5_2, 1176502272,  8912896,  8384,  180000, 90000,  8, 8,  10, 4 },
  { LEVEL_6,   1176502272,  35651584, 16832, 180000, 90000,  8, 16, 10, 4 },
  { LEVEL_6_1, 2353004544u, 35651584, 16832, 240000, 180000, 8, 16, 10, 4 },
  { LEVEL_6_2, 4706009088u, 35651584, 16832, 480000, 360000, 8, 16, 10, 4 },
};
/* clang-format on */

static const char *level_fail_messages[TARGET_LEVEL_FAIL_IDS] = {
  "The average bit-rate is too high.",
  "The picture size is too large.",
  "The picture width/height is too large.",
  "The luma sample rate is too large.",
  "The CPB size is too large.",
  "The compression ratio is too small",
  "Too many column tiles are used.",
  "The alt-ref distance is too small.",
  "Too many reference buffers are used."
};

static INLINE void Scale2Ratio(VPX_SCALING mode, int *hr, int *hs) {
  switch (mode) {
    case NORMAL:
      *hr = 1;
      *hs = 1;
      break;
    case FOURFIVE:
      *hr = 4;
      *hs = 5;
      break;
    case THREEFIVE:
      *hr = 3;
      *hs = 5;
      break;
    default:
      assert(mode == ONETWO);
      *hr = 1;
      *hs = 2;
      break;
  }
}

// Mark all inactive blocks as active. Other segmentation features may be set
// so memset cannot be used, instead only inactive blocks should be reset.
static void suppress_active_map(VP9_COMP *cpi) {
  unsigned char *const seg_map = cpi->segmentation_map;

  if (cpi->active_map.enabled || cpi->active_map.update) {
    const int rows = cpi->common.mi_rows;
    const int cols = cpi->common.mi_cols;
    int i;

    for (i = 0; i < rows * cols; ++i)
      if (seg_map[i] == AM_SEGMENT_ID_INACTIVE)
        seg_map[i] = AM_SEGMENT_ID_ACTIVE;
  }
}

static void apply_active_map(VP9_COMP *cpi) {
  struct segmentation *const seg = &cpi->common.seg;
  unsigned char *const seg_map = cpi->segmentation_map;
  const unsigned char *const active_map = cpi->active_map.map;
  int i;

  assert(AM_SEGMENT_ID_ACTIVE == CR_SEGMENT_ID_BASE);

  if (frame_is_intra_only(&cpi->common)) {
    cpi->active_map.enabled = 0;
    cpi->active_map.update = 1;
  }

  if (cpi->active_map.update) {
    if (cpi->active_map.enabled) {
      for (i = 0; i < cpi->common.mi_rows * cpi->common.mi_cols; ++i)
        if (seg_map[i] == AM_SEGMENT_ID_ACTIVE) seg_map[i] = active_map[i];
      vp9_enable_segmentation(seg);
      vp9_enable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_SKIP);
      vp9_enable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF);
      // Setting the data to -MAX_LOOP_FILTER will result in the computed loop
      // filter level being zero regardless of the value of seg->abs_delta.
      vp9_set_segdata(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF,
                      -MAX_LOOP_FILTER);
    } else {
      vp9_disable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_SKIP);
      vp9_disable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF);
      if (seg->enabled) {
        seg->update_data = 1;
        seg->update_map = 1;
      }
    }
    cpi->active_map.update = 0;
  }
}

static void apply_roi_map(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  struct segmentation *const seg = &cm->seg;
  vpx_roi_map_t *roi = &cpi->roi;
  const int *delta_q = roi->delta_q;
  const int *delta_lf = roi->delta_lf;
  const int *skip = roi->skip;
  int ref_frame[8];
  int internal_delta_q[MAX_SEGMENTS];
  int i;
  static const int flag_list[4] = { 0, VP9_LAST_FLAG, VP9_GOLD_FLAG,
                                    VP9_ALT_FLAG };

  // TODO(jianj): Investigate why ROI not working in speed < 5 or in non
  // realtime mode.
  if (cpi->oxcf.mode != REALTIME || cpi->oxcf.speed < 5) return;
  if (!roi->enabled) return;

  memcpy(&ref_frame, roi->ref_frame, sizeof(ref_frame));

  vp9_enable_segmentation(seg);
  vp9_clearall_segfeatures(seg);
  // Select delta coding method;
  seg->abs_delta = SEGMENT_DELTADATA;

  memcpy(cpi->segmentation_map, roi->roi_map, (cm->mi_rows * cm->mi_cols));

  for (i = 0; i < MAX_SEGMENTS; ++i) {
    // Translate the external delta q values to internal values.
    internal_delta_q[i] = vp9_quantizer_to_qindex(abs(delta_q[i]));
    if (delta_q[i] < 0) internal_delta_q[i] = -internal_delta_q[i];
    vp9_disable_segfeature(seg, i, SEG_LVL_ALT_Q);
    vp9_disable_segfeature(seg, i, SEG_LVL_ALT_LF);
    if (internal_delta_q[i] != 0) {
      vp9_enable_segfeature(seg, i, SEG_LVL_ALT_Q);
      vp9_set_segdata(seg, i, SEG_LVL_ALT_Q, internal_delta_q[i]);
    }
    if (delta_lf[i] != 0) {
      vp9_enable_segfeature(seg, i, SEG_LVL_ALT_LF);
      vp9_set_segdata(seg, i, SEG_LVL_ALT_LF, delta_lf[i]);
    }
    if (skip[i] != 0) {
      vp9_enable_segfeature(seg, i, SEG_LVL_SKIP);
      vp9_set_segdata(seg, i, SEG_LVL_SKIP, skip[i]);
    }
    if (ref_frame[i] >= 0) {
      int valid_ref = 1;
      // ALTREF is not used as reference for nonrd_pickmode with 0 lag.
      if (ref_frame[i] == ALTREF_FRAME && cpi->sf.use_nonrd_pick_mode)
        valid_ref = 0;
      // If GOLDEN is selected, make sure it's set as reference.
      if (ref_frame[i] == GOLDEN_FRAME &&
          !(cpi->ref_frame_flags & flag_list[ref_frame[i]])) {
        valid_ref = 0;
      }
      // GOLDEN was updated in previous encoded frame, so GOLDEN and LAST are
      // same reference.
      if (ref_frame[i] == GOLDEN_FRAME && cpi->rc.frames_since_golden == 0)
        ref_frame[i] = LAST_FRAME;
      if (valid_ref) {
        vp9_enable_segfeature(seg, i, SEG_LVL_REF_FRAME);
        vp9_set_segdata(seg, i, SEG_LVL_REF_FRAME, ref_frame[i]);
      }
    }
  }
  roi->enabled = 1;
}

static void init_level_info(Vp9LevelInfo *level_info) {
  Vp9LevelStats *const level_stats = &level_info->level_stats;
  Vp9LevelSpec *const level_spec = &level_info->level_spec;

  memset(level_stats, 0, sizeof(*level_stats));
  memset(level_spec, 0, sizeof(*level_spec));
  level_spec->level = LEVEL_UNKNOWN;
  level_spec->min_altref_distance = INT_MAX;
}

static int check_seg_range(int seg_data[8], int range) {
  return !(abs(seg_data[0]) > range || abs(seg_data[1]) > range ||
           abs(seg_data[2]) > range || abs(seg_data[3]) > range ||
           abs(seg_data[4]) > range || abs(seg_data[5]) > range ||
           abs(seg_data[6]) > range || abs(seg_data[7]) > range);
}

VP9_LEVEL vp9_get_level(const Vp9LevelSpec *const level_spec) {
  int i;
  const Vp9LevelSpec *this_level;

  vpx_clear_system_state();

  for (i = 0; i < VP9_LEVELS; ++i) {
    this_level = &vp9_level_defs[i];
    if ((double)level_spec->max_luma_sample_rate >
            (double)this_level->max_luma_sample_rate *
                (1 + SAMPLE_RATE_GRACE_P) ||
        level_spec->max_luma_picture_size > this_level->max_luma_picture_size ||
        level_spec->max_luma_picture_breadth >
            this_level->max_luma_picture_breadth ||
        level_spec->average_bitrate > this_level->average_bitrate ||
        level_spec->max_cpb_size > this_level->max_cpb_size ||
        level_spec->compression_ratio < this_level->compression_ratio ||
        level_spec->max_col_tiles > this_level->max_col_tiles ||
        level_spec->min_altref_distance < this_level->min_altref_distance ||
        level_spec->max_ref_frame_buffers > this_level->max_ref_frame_buffers)
      continue;
    break;
  }
  return (i == VP9_LEVELS) ? LEVEL_UNKNOWN : vp9_level_defs[i].level;
}

int vp9_set_roi_map(VP9_COMP *cpi, unsigned char *map, unsigned int rows,
                    unsigned int cols, int delta_q[8], int delta_lf[8],
                    int skip[8], int ref_frame[8]) {
  VP9_COMMON *cm = &cpi->common;
  vpx_roi_map_t *roi = &cpi->roi;
  const int range = 63;
  const int ref_frame_range = 3;  // Alt-ref
  const int skip_range = 1;
  const int frame_rows = cpi->common.mi_rows;
  const int frame_cols = cpi->common.mi_cols;

  // Check number of rows and columns match
  if (frame_rows != (int)rows || frame_cols != (int)cols) {
    return -1;
  }

  if (!check_seg_range(delta_q, range) || !check_seg_range(delta_lf, range) ||
      !check_seg_range(ref_frame, ref_frame_range) ||
      !check_seg_range(skip, skip_range))
    return -1;

  // Also disable segmentation if no deltas are specified.
  if (!map ||
      (!(delta_q[0] | delta_q[1] | delta_q[2] | delta_q[3] | delta_q[4] |
         delta_q[5] | delta_q[6] | delta_q[7] | delta_lf[0] | delta_lf[1] |
         delta_lf[2] | delta_lf[3] | delta_lf[4] | delta_lf[5] | delta_lf[6] |
         delta_lf[7] | skip[0] | skip[1] | skip[2] | skip[3] | skip[4] |
         skip[5] | skip[6] | skip[7]) &&
       (ref_frame[0] == -1 && ref_frame[1] == -1 && ref_frame[2] == -1 &&
        ref_frame[3] == -1 && ref_frame[4] == -1 && ref_frame[5] == -1 &&
        ref_frame[6] == -1 && ref_frame[7] == -1))) {
    vp9_disable_segmentation(&cm->seg);
    cpi->roi.enabled = 0;
    return 0;
  }

  if (roi->roi_map) {
    vpx_free(roi->roi_map);
    roi->roi_map = NULL;
  }
  CHECK_MEM_ERROR(cm, roi->roi_map, vpx_malloc(rows * cols));

  // Copy to ROI structure in the compressor.
  memcpy(roi->roi_map, map, rows * cols);
  memcpy(&roi->delta_q, delta_q, MAX_SEGMENTS * sizeof(delta_q[0]));
  memcpy(&roi->delta_lf, delta_lf, MAX_SEGMENTS * sizeof(delta_lf[0]));
  memcpy(&roi->skip, skip, MAX_SEGMENTS * sizeof(skip[0]));
  memcpy(&roi->ref_frame, ref_frame, MAX_SEGMENTS * sizeof(ref_frame[0]));
  roi->enabled = 1;
  roi->rows = rows;
  roi->cols = cols;

  return 0;
}

int vp9_set_active_map(VP9_COMP *cpi, unsigned char *new_map_16x16, int rows,
                       int cols) {
  if (rows == cpi->common.mb_rows && cols == cpi->common.mb_cols) {
    unsigned char *const active_map_8x8 = cpi->active_map.map;
    const int mi_rows = cpi->common.mi_rows;
    const int mi_cols = cpi->common.mi_cols;
    cpi->active_map.update = 1;
    if (new_map_16x16) {
      int r, c;
      for (r = 0; r < mi_rows; ++r) {
        for (c = 0; c < mi_cols; ++c) {
          active_map_8x8[r * mi_cols + c] =
              new_map_16x16[(r >> 1) * cols + (c >> 1)]
                  ? AM_SEGMENT_ID_ACTIVE
                  : AM_SEGMENT_ID_INACTIVE;
        }
      }
      cpi->active_map.enabled = 1;
    } else {
      cpi->active_map.enabled = 0;
    }
    return 0;
  } else {
    return -1;
  }
}

int vp9_get_active_map(VP9_COMP *cpi, unsigned char *new_map_16x16, int rows,
                       int cols) {
  if (rows == cpi->common.mb_rows && cols == cpi->common.mb_cols &&
      new_map_16x16) {
    unsigned char *const seg_map_8x8 = cpi->segmentation_map;
    const int mi_rows = cpi->common.mi_rows;
    const int mi_cols = cpi->common.mi_cols;
    memset(new_map_16x16, !cpi->active_map.enabled, rows * cols);
    if (cpi->active_map.enabled) {
      int r, c;
      for (r = 0; r < mi_rows; ++r) {
        for (c = 0; c < mi_cols; ++c) {
          // Cyclic refresh segments are considered active despite not having
          // AM_SEGMENT_ID_ACTIVE
          new_map_16x16[(r >> 1) * cols + (c >> 1)] |=
              seg_map_8x8[r * mi_cols + c] != AM_SEGMENT_ID_INACTIVE;
        }
      }
    }
    return 0;
  } else {
    return -1;
  }
}

void vp9_set_high_precision_mv(VP9_COMP *cpi, int allow_high_precision_mv) {
  MACROBLOCK *const mb = &cpi->td.mb;
  cpi->common.allow_high_precision_mv = allow_high_precision_mv;
  if (cpi->common.allow_high_precision_mv) {
    mb->mvcost = mb->nmvcost_hp;
    mb->mvsadcost = mb->nmvsadcost_hp;
  } else {
    mb->mvcost = mb->nmvcost;
    mb->mvsadcost = mb->nmvsadcost;
  }
}

static void setup_frame(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  // Set up entropy context depending on frame type. The decoder mandates
  // the use of the default context, index 0, for keyframes and inter
  // frames where the error_resilient_mode or intra_only flag is set. For
  // other inter-frames the encoder currently uses only two contexts;
  // context 1 for ALTREF frames and context 0 for the others.
  if (frame_is_intra_only(cm) || cm->error_resilient_mode) {
    vp9_setup_past_independence(cm);
  } else {
    if (!cpi->use_svc) cm->frame_context_idx = cpi->refresh_alt_ref_frame;
  }

  // TODO(jingning): Overwrite the frame_context_idx index in multi-layer ARF
  // case. Need some further investigation on if we could apply this to single
  // layer ARF case as well.
  if (cpi->multi_layer_arf && !cpi->use_svc) {
    GF_GROUP *const gf_group = &cpi->twopass.gf_group;
    const int gf_group_index = gf_group->index;
    const int boost_frame =
        !cpi->rc.is_src_frame_alt_ref &&
        (cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame);

    // frame_context_idx           Frame Type
    //        0              Intra only frame, base layer ARF
    //        1              ARFs with layer depth = 2,3
    //        2              ARFs with layer depth > 3
    //        3              Non-boosted frames
    if (frame_is_intra_only(cm)) {
      cm->frame_context_idx = 0;
    } else if (boost_frame) {
      if (gf_group->rf_level[gf_group_index] == GF_ARF_STD)
        cm->frame_context_idx = 0;
      else if (gf_group->layer_depth[gf_group_index] <= 3)
        cm->frame_context_idx = 1;
      else
        cm->frame_context_idx = 2;
    } else {
      cm->frame_context_idx = 3;
    }
  }

  if (cm->frame_type == KEY_FRAME) {
    cpi->refresh_golden_frame = 1;
    cpi->refresh_alt_ref_frame = 1;
    vp9_zero(cpi->interp_filter_selected);
  } else {
    *cm->fc = cm->frame_contexts[cm->frame_context_idx];
    vp9_zero(cpi->interp_filter_selected[0]);
  }
}

static void vp9_enc_setup_mi(VP9_COMMON *cm) {
  int i;
  cm->mi = cm->mip + cm->mi_stride + 1;
  memset(cm->mip, 0, cm->mi_stride * (cm->mi_rows + 1) * sizeof(*cm->mip));
  cm->prev_mi = cm->prev_mip + cm->mi_stride + 1;
  // Clear top border row
  memset(cm->prev_mip, 0, sizeof(*cm->prev_mip) * cm->mi_stride);
  // Clear left border column
  for (i = 1; i < cm->mi_rows + 1; ++i)
    memset(&cm->prev_mip[i * cm->mi_stride], 0, sizeof(*cm->prev_mip));

  cm->mi_grid_visible = cm->mi_grid_base + cm->mi_stride + 1;
  cm->prev_mi_grid_visible = cm->prev_mi_grid_base + cm->mi_stride + 1;

  memset(cm->mi_grid_base, 0,
         cm->mi_stride * (cm->mi_rows + 1) * sizeof(*cm->mi_grid_base));
}

static int vp9_enc_alloc_mi(VP9_COMMON *cm, int mi_size) {
  cm->mip = vpx_calloc(mi_size, sizeof(*cm->mip));
  if (!cm->mip) return 1;
  cm->prev_mip = vpx_calloc(mi_size, sizeof(*cm->prev_mip));
  if (!cm->prev_mip) return 1;
  cm->mi_alloc_size = mi_size;

  cm->mi_grid_base = (MODE_INFO **)vpx_calloc(mi_size, sizeof(MODE_INFO *));
  if (!cm->mi_grid_base) return 1;
  cm->prev_mi_grid_base =
      (MODE_INFO **)vpx_calloc(mi_size, sizeof(MODE_INFO *));
  if (!cm->prev_mi_grid_base) return 1;

  return 0;
}

static void vp9_enc_free_mi(VP9_COMMON *cm) {
  vpx_free(cm->mip);
  cm->mip = NULL;
  vpx_free(cm->prev_mip);
  cm->prev_mip = NULL;
  vpx_free(cm->mi_grid_base);
  cm->mi_grid_base = NULL;
  vpx_free(cm->prev_mi_grid_base);
  cm->prev_mi_grid_base = NULL;
  cm->mi_alloc_size = 0;
}

static void vp9_swap_mi_and_prev_mi(VP9_COMMON *cm) {
  // Current mip will be the prev_mip for the next frame.
  MODE_INFO **temp_base = cm->prev_mi_grid_base;
  MODE_INFO *temp = cm->prev_mip;

  // Skip update prev_mi frame in show_existing_frame mode.
  if (cm->show_existing_frame) return;

  cm->prev_mip = cm->mip;
  cm->mip = temp;

  // Update the upper left visible macroblock ptrs.
  cm->mi = cm->mip + cm->mi_stride + 1;
  cm->prev_mi = cm->prev_mip + cm->mi_stride + 1;

  cm->prev_mi_grid_base = cm->mi_grid_base;
  cm->mi_grid_base = temp_base;
  cm->mi_grid_visible = cm->mi_grid_base + cm->mi_stride + 1;
  cm->prev_mi_grid_visible = cm->prev_mi_grid_base + cm->mi_stride + 1;
}

void vp9_initialize_enc(void) {
  static volatile int init_done = 0;

  if (!init_done) {
    vp9_rtcd();
    vpx_dsp_rtcd();
    vpx_scale_rtcd();
    vp9_init_intra_predictors();
    vp9_init_me_luts();
    vp9_rc_init_minq_luts();
    vp9_entropy_mv_init();
#if !CONFIG_REALTIME_ONLY
    vp9_temporal_filter_init();
#endif
    init_done = 1;
  }
}

static void dealloc_compressor_data(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  int i;

  vpx_free(cpi->mbmi_ext_base);
  cpi->mbmi_ext_base = NULL;

  vpx_free(cpi->tile_data);
  cpi->tile_data = NULL;

  vpx_free(cpi->segmentation_map);
  cpi->segmentation_map = NULL;
  vpx_free(cpi->coding_context.last_frame_seg_map_copy);
  cpi->coding_context.last_frame_seg_map_copy = NULL;

  vpx_free(cpi->nmvcosts[0]);
  vpx_free(cpi->nmvcosts[1]);
  cpi->nmvcosts[0] = NULL;
  cpi->nmvcosts[1] = NULL;

  vpx_free(cpi->nmvcosts_hp[0]);
  vpx_free(cpi->nmvcosts_hp[1]);
  cpi->nmvcosts_hp[0] = NULL;
  cpi->nmvcosts_hp[1] = NULL;

  vpx_free(cpi->nmvsadcosts[0]);
  vpx_free(cpi->nmvsadcosts[1]);
  cpi->nmvsadcosts[0] = NULL;
  cpi->nmvsadcosts[1] = NULL;

  vpx_free(cpi->nmvsadcosts_hp[0]);
  vpx_free(cpi->nmvsadcosts_hp[1]);
  cpi->nmvsadcosts_hp[0] = NULL;
  cpi->nmvsadcosts_hp[1] = NULL;

  vpx_free(cpi->skin_map);
  cpi->skin_map = NULL;

  vpx_free(cpi->prev_partition);
  cpi->prev_partition = NULL;

  vpx_free(cpi->svc.prev_partition_svc);
  cpi->svc.prev_partition_svc = NULL;

  vpx_free(cpi->prev_segment_id);
  cpi->prev_segment_id = NULL;

  vpx_free(cpi->prev_variance_low);
  cpi->prev_variance_low = NULL;

  vpx_free(cpi->copied_frame_cnt);
  cpi->copied_frame_cnt = NULL;

  vpx_free(cpi->content_state_sb_fd);
  cpi->content_state_sb_fd = NULL;

  vpx_free(cpi->count_arf_frame_usage);
  cpi->count_arf_frame_usage = NULL;
  vpx_free(cpi->count_lastgolden_frame_usage);
  cpi->count_lastgolden_frame_usage = NULL;

  vp9_cyclic_refresh_free(cpi->cyclic_refresh);
  cpi->cyclic_refresh = NULL;

  vpx_free(cpi->active_map.map);
  cpi->active_map.map = NULL;

  vpx_free(cpi->roi.roi_map);
  cpi->roi.roi_map = NULL;

  vpx_free(cpi->consec_zero_mv);
  cpi->consec_zero_mv = NULL;

  vpx_free(cpi->mb_wiener_variance);
  cpi->mb_wiener_variance = NULL;

  vpx_free(cpi->mi_ssim_rdmult_scaling_factors);
  cpi->mi_ssim_rdmult_scaling_factors = NULL;

#if CONFIG_RATE_CTRL
  free_partition_info(cpi);
  free_motion_vector_info(cpi);
  free_fp_motion_vector_info(cpi);
  free_tpl_stats_info(cpi);
#endif

  vp9_free_ref_frame_buffers(cm->buffer_pool);
#if CONFIG_VP9_POSTPROC
  vp9_free_postproc_buffers(cm);
#endif
  vp9_free_context_buffers(cm);

  vpx_free_frame_buffer(&cpi->last_frame_uf);
  vpx_free_frame_buffer(&cpi->scaled_source);
  vpx_free_frame_buffer(&cpi->scaled_last_source);
  vpx_free_frame_buffer(&cpi->alt_ref_buffer);
#ifdef ENABLE_KF_DENOISE
  vpx_free_frame_buffer(&cpi->raw_unscaled_source);
  vpx_free_frame_buffer(&cpi->raw_scaled_source);
#endif

  vp9_lookahead_destroy(cpi->lookahead);

  vpx_free(cpi->tile_tok[0][0]);
  cpi->tile_tok[0][0] = 0;

  vpx_free(cpi->tplist[0][0]);
  cpi->tplist[0][0] = NULL;

  vp9_free_pc_tree(&cpi->td);

  for (i = 0; i < cpi->svc.number_spatial_layers; ++i) {
    LAYER_CONTEXT *const lc = &cpi->svc.layer_context[i];
    vpx_free(lc->rc_twopass_stats_in.buf);
    lc->rc_twopass_stats_in.buf = NULL;
    lc->rc_twopass_stats_in.sz = 0;
  }

  if (cpi->source_diff_var != NULL) {
    vpx_free(cpi->source_diff_var);
    cpi->source_diff_var = NULL;
  }

  for (i = 0; i < MAX_LAG_BUFFERS; ++i) {
    vpx_free_frame_buffer(&cpi->svc.scaled_frames[i]);
  }
  memset(&cpi->svc.scaled_frames[0], 0,
         MAX_LAG_BUFFERS * sizeof(cpi->svc.scaled_frames[0]));

  vpx_free_frame_buffer(&cpi->svc.scaled_temp);
  memset(&cpi->svc.scaled_temp, 0, sizeof(cpi->svc.scaled_temp));

  vpx_free_frame_buffer(&cpi->svc.empty_frame.img);
  memset(&cpi->svc.empty_frame, 0, sizeof(cpi->svc.empty_frame));

  vp9_free_svc_cyclic_refresh(cpi);
}

static void save_coding_context(VP9_COMP *cpi) {
  CODING_CONTEXT *const cc = &cpi->coding_context;
  VP9_COMMON *cm = &cpi->common;

  // Stores a snapshot of key state variables which can subsequently be
  // restored with a call to vp9_restore_coding_context. These functions are
  // intended for use in a re-code loop in vp9_compress_frame where the
  // quantizer value is adjusted between loop iterations.
  vp9_copy(cc->nmvjointcost, cpi->td.mb.nmvjointcost);

  memcpy(cc->nmvcosts[0], cpi->nmvcosts[0],
         MV_VALS * sizeof(*cpi->nmvcosts[0]));
  memcpy(cc->nmvcosts[1], cpi->nmvcosts[1],
         MV_VALS * sizeof(*cpi->nmvcosts[1]));
  memcpy(cc->nmvcosts_hp[0], cpi->nmvcosts_hp[0],
         MV_VALS * sizeof(*cpi->nmvcosts_hp[0]));
  memcpy(cc->nmvcosts_hp[1], cpi->nmvcosts_hp[1],
         MV_VALS * sizeof(*cpi->nmvcosts_hp[1]));

  vp9_copy(cc->segment_pred_probs, cm->seg.pred_probs);

  memcpy(cpi->coding_context.last_frame_seg_map_copy, cm->last_frame_seg_map,
         (cm->mi_rows * cm->mi_cols));

  vp9_copy(cc->last_ref_lf_deltas, cm->lf.last_ref_deltas);
  vp9_copy(cc->last_mode_lf_deltas, cm->lf.last_mode_deltas);

  cc->fc = *cm->fc;
}

static void restore_coding_context(VP9_COMP *cpi) {
  CODING_CONTEXT *const cc = &cpi->coding_context;
  VP9_COMMON *cm = &cpi->common;

  // Restore key state variables to the snapshot state stored in the
  // previous call to vp9_save_coding_context.
  vp9_copy(cpi->td.mb.nmvjointcost, cc->nmvjointcost);

  memcpy(cpi->nmvcosts[0], cc->nmvcosts[0], MV_VALS * sizeof(*cc->nmvcosts[0]));
  memcpy(cpi->nmvcosts[1], cc->nmvcosts[1], MV_VALS * sizeof(*cc->nmvcosts[1]));
  memcpy(cpi->nmvcosts_hp[0], cc->nmvcosts_hp[0],
         MV_VALS * sizeof(*cc->nmvcosts_hp[0]));
  memcpy(cpi->nmvcosts_hp[1], cc->nmvcosts_hp[1],
         MV_VALS * sizeof(*cc->nmvcosts_hp[1]));

  vp9_copy(cm->seg.pred_probs, cc->segment_pred_probs);

  memcpy(cm->last_frame_seg_map, cpi->coding_context.last_frame_seg_map_copy,
         (cm->mi_rows * cm->mi_cols));

  vp9_copy(cm->lf.last_ref_deltas, cc->last_ref_lf_deltas);
  vp9_copy(cm->lf.last_mode_deltas, cc->last_mode_lf_deltas);

  *cm->fc = cc->fc;
}

#if !CONFIG_REALTIME_ONLY
static void configure_static_seg_features(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  struct segmentation *const seg = &cm->seg;

  int high_q = (int)(rc->avg_q > 48.0);
  int qi_delta;

  // Disable and clear down for KF
  if (cm->frame_type == KEY_FRAME) {
    // Clear down the global segmentation map
    memset(cpi->segmentation_map, 0, cm->mi_rows * cm->mi_cols);
    seg->update_map = 0;
    seg->update_data = 0;
    cpi->static_mb_pct = 0;

    // Disable segmentation
    vp9_disable_segmentation(seg);

    // Clear down the segment features.
    vp9_clearall_segfeatures(seg);
  } else if (cpi->refresh_alt_ref_frame) {
    // If this is an alt ref frame
    // Clear down the global segmentation map
    memset(cpi->segmentation_map, 0, cm->mi_rows * cm->mi_cols);
    seg->update_map = 0;
    seg->update_data = 0;
    cpi->static_mb_pct = 0;

    // Disable segmentation and individual segment features by default
    vp9_disable_segmentation(seg);
    vp9_clearall_segfeatures(seg);

    // Scan frames from current to arf frame.
    // This function re-enables segmentation if appropriate.
    vp9_update_mbgraph_stats(cpi);

    // If segmentation was enabled set those features needed for the
    // arf itself.
    if (seg->enabled) {
      seg->update_map = 1;
      seg->update_data = 1;

      qi_delta =
          vp9_compute_qdelta(rc, rc->avg_q, rc->avg_q * 0.875, cm->bit_depth);
      vp9_set_segdata(seg, 1, SEG_LVL_ALT_Q, qi_delta - 2);
      vp9_set_segdata(seg, 1, SEG_LVL_ALT_LF, -2);

      vp9_enable_segfeature(seg, 1, SEG_LVL_ALT_Q);
      vp9_enable_segfeature(seg, 1, SEG_LVL_ALT_LF);

      // Where relevant assume segment data is delta data
      seg->abs_delta = SEGMENT_DELTADATA;
    }
  } else if (seg->enabled) {
    // All other frames if segmentation has been enabled

    // First normal frame in a valid gf or alt ref group
    if (rc->frames_since_golden == 0) {
      // Set up segment features for normal frames in an arf group
      if (rc->source_alt_ref_active) {
        seg->update_map = 0;
        seg->update_data = 1;
        seg->abs_delta = SEGMENT_DELTADATA;

        qi_delta =
            vp9_compute_qdelta(rc, rc->avg_q, rc->avg_q * 1.125, cm->bit_depth);
        vp9_set_segdata(seg, 1, SEG_LVL_ALT_Q, qi_delta + 2);
        vp9_enable_segfeature(seg, 1, SEG_LVL_ALT_Q);

        vp9_set_segdata(seg, 1, SEG_LVL_ALT_LF, -2);
        vp9_enable_segfeature(seg, 1, SEG_LVL_ALT_LF);

        // Segment coding disabled for compred testing
        if (high_q || (cpi->static_mb_pct == 100)) {
          vp9_set_segdata(seg, 1, SEG_LVL_REF_FRAME, ALTREF_FRAME);
          vp9_enable_segfeature(seg, 1, SEG_LVL_REF_FRAME);
          vp9_enable_segfeature(seg, 1, SEG_LVL_SKIP);
        }
      } else {
        // Disable segmentation and clear down features if alt ref
        // is not active for this group

        vp9_disable_segmentation(seg);

        memset(cpi->segmentation_map, 0, cm->mi_rows * cm->mi_cols);

        seg->update_map = 0;
        seg->update_data = 0;

        vp9_clearall_segfeatures(seg);
      }
    } else if (rc->is_src_frame_alt_ref) {
      // Special case where we are coding over the top of a previous
      // alt ref frame.
      // Segment coding disabled for compred testing

      // Enable ref frame features for segment 0 as well
      vp9_enable_segfeature(seg, 0, SEG_LVL_REF_FRAME);
      vp9_enable_segfeature(seg, 1, SEG_LVL_REF_FRAME);

      // All mbs should use ALTREF_FRAME
      vp9_clear_segdata(seg, 0, SEG_LVL_REF_FRAME);
      vp9_set_segdata(seg, 0, SEG_LVL_REF_FRAME, ALTREF_FRAME);
      vp9_clear_segdata(seg, 1, SEG_LVL_REF_FRAME);
      vp9_set_segdata(seg, 1, SEG_LVL_REF_FRAME, ALTREF_FRAME);

      // Skip all MBs if high Q (0,0 mv and skip coeffs)
      if (high_q) {
        vp9_enable_segfeature(seg, 0, SEG_LVL_SKIP);
        vp9_enable_segfeature(seg, 1, SEG_LVL_SKIP);
      }
      // Enable data update
      seg->update_data = 1;
    } else {
      // All other frames.

      // No updates.. leave things as they are.
      seg->update_map = 0;
      seg->update_data = 0;
    }
  }
}
#endif  // !CONFIG_REALTIME_ONLY

static void update_reference_segmentation_map(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  MODE_INFO **mi_8x8_ptr = cm->mi_grid_visible;
  uint8_t *cache_ptr = cm->last_frame_seg_map;
  int row, col;

  for (row = 0; row < cm->mi_rows; row++) {
    MODE_INFO **mi_8x8 = mi_8x8_ptr;
    uint8_t *cache = cache_ptr;
    for (col = 0; col < cm->mi_cols; col++, mi_8x8++, cache++)
      cache[0] = mi_8x8[0]->segment_id;
    mi_8x8_ptr += cm->mi_stride;
    cache_ptr += cm->mi_cols;
  }
}

static void alloc_raw_frame_buffers(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  const VP9EncoderConfig *oxcf = &cpi->oxcf;

  if (!cpi->lookahead)
    cpi->lookahead = vp9_lookahead_init(oxcf->width, oxcf->height,
                                        cm->subsampling_x, cm->subsampling_y,
#if CONFIG_VP9_HIGHBITDEPTH
                                        cm->use_highbitdepth,
#endif
                                        oxcf->lag_in_frames);
  if (!cpi->lookahead)
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate lag buffers");

  // TODO(agrange) Check if ARF is enabled and skip allocation if not.
  if (vpx_realloc_frame_buffer(&cpi->alt_ref_buffer, oxcf->width, oxcf->height,
                               cm->subsampling_x, cm->subsampling_y,
#if CONFIG_VP9_HIGHBITDEPTH
                               cm->use_highbitdepth,
#endif
                               VP9_ENC_BORDER_IN_PIXELS, cm->byte_alignment,
                               NULL, NULL, NULL))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate altref buffer");
}

static void alloc_util_frame_buffers(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  if (vpx_realloc_frame_buffer(&cpi->last_frame_uf, cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
#if CONFIG_VP9_HIGHBITDEPTH
                               cm->use_highbitdepth,
#endif
                               VP9_ENC_BORDER_IN_PIXELS, cm->byte_alignment,
                               NULL, NULL, NULL))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate last frame buffer");

  if (vpx_realloc_frame_buffer(&cpi->scaled_source, cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
#if CONFIG_VP9_HIGHBITDEPTH
                               cm->use_highbitdepth,
#endif
                               VP9_ENC_BORDER_IN_PIXELS, cm->byte_alignment,
                               NULL, NULL, NULL))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate scaled source buffer");

  // For 1 pass cbr: allocate scaled_frame that may be used as an intermediate
  // buffer for a 2 stage down-sampling: two stages of 1:2 down-sampling for a
  // target of 1/4x1/4. number_spatial_layers must be greater than 2.
  if (is_one_pass_cbr_svc(cpi) && !cpi->svc.scaled_temp_is_alloc &&
      cpi->svc.number_spatial_layers > 2) {
    cpi->svc.scaled_temp_is_alloc = 1;
    if (vpx_realloc_frame_buffer(
            &cpi->svc.scaled_temp, cm->width >> 1, cm->height >> 1,
            cm->subsampling_x, cm->subsampling_y,
#if CONFIG_VP9_HIGHBITDEPTH
            cm->use_highbitdepth,
#endif
            VP9_ENC_BORDER_IN_PIXELS, cm->byte_alignment, NULL, NULL, NULL))
      vpx_internal_error(&cpi->common.error, VPX_CODEC_MEM_ERROR,
                         "Failed to allocate scaled_frame for svc ");
  }

  if (vpx_realloc_frame_buffer(&cpi->scaled_last_source, cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
#if CONFIG_VP9_HIGHBITDEPTH
                               cm->use_highbitdepth,
#endif
                               VP9_ENC_BORDER_IN_PIXELS, cm->byte_alignment,
                               NULL, NULL, NULL))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate scaled last source buffer");
#ifdef ENABLE_KF_DENOISE
  if (vpx_realloc_frame_buffer(&cpi->raw_unscaled_source, cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
#if CONFIG_VP9_HIGHBITDEPTH
                               cm->use_highbitdepth,
#endif
                               VP9_ENC_BORDER_IN_PIXELS, cm->byte_alignment,
                               NULL, NULL, NULL))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate unscaled raw source frame buffer");

  if (vpx_realloc_frame_buffer(&cpi->raw_scaled_source, cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
#if CONFIG_VP9_HIGHBITDEPTH
                               cm->use_highbitdepth,
#endif
                               VP9_ENC_BORDER_IN_PIXELS, cm->byte_alignment,
                               NULL, NULL, NULL))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate scaled raw source frame buffer");
#endif
}

static int alloc_context_buffers_ext(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  int mi_size = cm->mi_cols * cm->mi_rows;

  cpi->mbmi_ext_base = vpx_calloc(mi_size, sizeof(*cpi->mbmi_ext_base));
  if (!cpi->mbmi_ext_base) return 1;

  return 0;
}

static void alloc_compressor_data(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  int sb_rows;

  vp9_alloc_context_buffers(cm, cm->width, cm->height);

  alloc_context_buffers_ext(cpi);

  vpx_free(cpi->tile_tok[0][0]);

  {
    unsigned int tokens = get_token_alloc(cm->mb_rows, cm->mb_cols);
    CHECK_MEM_ERROR(cm, cpi->tile_tok[0][0],
                    vpx_calloc(tokens, sizeof(*cpi->tile_tok[0][0])));
  }

  sb_rows = mi_cols_aligned_to_sb(cm->mi_rows) >> MI_BLOCK_SIZE_LOG2;
  vpx_free(cpi->tplist[0][0]);
  CHECK_MEM_ERROR(
      cm, cpi->tplist[0][0],
      vpx_calloc(sb_rows * 4 * (1 << 6), sizeof(*cpi->tplist[0][0])));

  vp9_setup_pc_tree(&cpi->common, &cpi->td);
}

void vp9_new_framerate(VP9_COMP *cpi, double framerate) {
  cpi->framerate = framerate < 0.1 ? 30 : framerate;
  vp9_rc_update_framerate(cpi);
}

static void set_tile_limits(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;

  int min_log2_tile_cols, max_log2_tile_cols;
  vp9_get_tile_n_bits(cm->mi_cols, &min_log2_tile_cols, &max_log2_tile_cols);

  cm->log2_tile_cols =
      clamp(cpi->oxcf.tile_columns, min_log2_tile_cols, max_log2_tile_cols);
  cm->log2_tile_rows = cpi->oxcf.tile_rows;

  if (cpi->oxcf.target_level == LEVEL_AUTO) {
    const int level_tile_cols =
        log_tile_cols_from_picsize_level(cpi->common.width, cpi->common.height);
    if (cm->log2_tile_cols > level_tile_cols) {
      cm->log2_tile_cols = VPXMAX(level_tile_cols, min_log2_tile_cols);
    }
  }
}

static void update_frame_size(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;

  vp9_set_mb_mi(cm, cm->width, cm->height);
  vp9_init_context_buffers(cm);
  vp9_init_macroblockd(cm, xd, NULL);
  cpi->td.mb.mbmi_ext_base = cpi->mbmi_ext_base;
  memset(cpi->mbmi_ext_base, 0,
         cm->mi_rows * cm->mi_cols * sizeof(*cpi->mbmi_ext_base));

  set_tile_limits(cpi);
}

static void init_buffer_indices(VP9_COMP *cpi) {
  int ref_frame;

  for (ref_frame = 0; ref_frame < REF_FRAMES; ++ref_frame)
    cpi->ref_fb_idx[ref_frame] = ref_frame;

  cpi->lst_fb_idx = cpi->ref_fb_idx[LAST_FRAME - 1];
  cpi->gld_fb_idx = cpi->ref_fb_idx[GOLDEN_FRAME - 1];
  cpi->alt_fb_idx = cpi->ref_fb_idx[ALTREF_FRAME - 1];
}

static void init_level_constraint(LevelConstraint *lc) {
  lc->level_index = -1;
  lc->max_cpb_size = INT_MAX;
  lc->max_frame_size = INT_MAX;
  lc->fail_flag = 0;
}

static void set_level_constraint(LevelConstraint *ls, int8_t level_index) {
  vpx_clear_system_state();
  ls->level_index = level_index;
  if (level_index >= 0) {
    ls->max_cpb_size = vp9_level_defs[level_index].max_cpb_size * (double)1000;
  }
}

static void init_config(struct VP9_COMP *cpi, const VP9EncoderConfig *oxcf) {
  VP9_COMMON *const cm = &cpi->common;

  cpi->oxcf = *oxcf;
  cpi->framerate = oxcf->init_framerate;
  cm->profile = oxcf->profile;
  cm->bit_depth = oxcf->bit_depth;
#if CONFIG_VP9_HIGHBITDEPTH
  cm->use_highbitdepth = oxcf->use_highbitdepth;
#endif
  cm->color_space = oxcf->color_space;
  cm->color_range = oxcf->color_range;

  cpi->target_level = oxcf->target_level;
  cpi->keep_level_stats = oxcf->target_level != LEVEL_MAX;
  set_level_constraint(&cpi->level_constraint,
                       get_level_index(cpi->target_level));

  cm->width = oxcf->width;
  cm->height = oxcf->height;
  alloc_compressor_data(cpi);

  cpi->svc.temporal_layering_mode = oxcf->temporal_layering_mode;

  // Single thread case: use counts in common.
  cpi->td.counts = &cm->counts;

  // Spatial scalability.
  cpi->svc.number_spatial_layers = oxcf->ss_number_layers;
  // Temporal scalability.
  cpi->svc.number_temporal_layers = oxcf->ts_number_layers;

  if ((cpi->svc.number_temporal_layers > 1 && cpi->oxcf.rc_mode == VPX_CBR) ||
      ((cpi->svc.number_temporal_layers > 1 ||
        cpi->svc.number_spatial_layers > 1) &&
       cpi->oxcf.pass != 1)) {
    vp9_init_layer_context(cpi);
  }

  // change includes all joint functionality
  vp9_change_config(cpi, oxcf);

  cpi->static_mb_pct = 0;
  cpi->ref_frame_flags = 0;

  init_buffer_indices(cpi);

  vp9_noise_estimate_init(&cpi->noise_estimate, cm->width, cm->height);
}

void vp9_check_reset_rc_flag(VP9_COMP *cpi) {
  RATE_CONTROL *rc = &cpi->rc;

  if (cpi->common.current_video_frame >
      (unsigned int)cpi->svc.number_spatial_layers) {
    if (cpi->use_svc) {
      vp9_svc_check_reset_layer_rc_flag(cpi);
    } else {
      if (rc->avg_frame_bandwidth > (3 * rc->last_avg_frame_bandwidth >> 1) ||
          rc->avg_frame_bandwidth < (rc->last_avg_frame_bandwidth >> 1)) {
        rc->rc_1_frame = 0;
        rc->rc_2_frame = 0;
        rc->bits_off_target = rc->optimal_buffer_level;
        rc->buffer_level = rc->optimal_buffer_level;
      }
    }
  }
}

void vp9_set_rc_buffer_sizes(VP9_COMP *cpi) {
  RATE_CONTROL *rc = &cpi->rc;
  const VP9EncoderConfig *oxcf = &cpi->oxcf;

  const int64_t bandwidth = oxcf->target_bandwidth;
  const int64_t starting = oxcf->starting_buffer_level_ms;
  const int64_t optimal = oxcf->optimal_buffer_level_ms;
  const int64_t maximum = oxcf->maximum_buffer_size_ms;

  rc->starting_buffer_level = starting * bandwidth / 1000;
  rc->optimal_buffer_level =
      (optimal == 0) ? bandwidth / 8 : optimal * bandwidth / 1000;
  rc->maximum_buffer_size =
      (maximum == 0) ? bandwidth / 8 : maximum * bandwidth / 1000;

  // Under a configuration change, where maximum_buffer_size may change,
  // keep buffer level clipped to the maximum allowed buffer size.
  rc->bits_off_target = VPXMIN(rc->bits_off_target, rc->maximum_buffer_size);
  rc->buffer_level = VPXMIN(rc->buffer_level, rc->maximum_buffer_size);
}

#if CONFIG_VP9_HIGHBITDEPTH
// TODO(angiebird): make sdx8f available for highbitdepth if needed
#define HIGHBD_BFP(BT, SDF, SDAF, VF, SVF, SVAF, SDX4DF) \
  cpi->fn_ptr[BT].sdf = SDF;                             \
  cpi->fn_ptr[BT].sdaf = SDAF;                           \
  cpi->fn_ptr[BT].vf = VF;                               \
  cpi->fn_ptr[BT].svf = SVF;                             \
  cpi->fn_ptr[BT].svaf = SVAF;                           \
  cpi->fn_ptr[BT].sdx4df = SDX4DF;                       \
  cpi->fn_ptr[BT].sdx8f = NULL;

#define MAKE_BFP_SAD_WRAPPER(fnname)                                           \
  static unsigned int fnname##_bits8(const uint8_t *src_ptr,                   \
                                     int source_stride,                        \
                                     const uint8_t *ref_ptr, int ref_stride) { \
    return fnname(src_ptr, source_stride, ref_ptr, ref_stride);                \
  }                                                                            \
  static unsigned int fnname##_bits10(                                         \
      const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr,       \
      int ref_stride) {                                                        \
    return fnname(src_ptr, source_stride, ref_ptr, ref_stride) >> 2;           \
  }                                                                            \
  static unsigned int fnname##_bits12(                                         \
      const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr,       \
      int ref_stride) {                                                        \
    return fnname(src_ptr, source_stride, ref_ptr, ref_stride) >> 4;           \
  }

#define MAKE_BFP_SADAVG_WRAPPER(fnname)                                        \
  static unsigned int fnname##_bits8(                                          \
      const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr,       \
      int ref_stride, const uint8_t *second_pred) {                            \
    return fnname(src_ptr, source_stride, ref_ptr, ref_stride, second_pred);   \
  }                                                                            \
  static unsigned int fnname##_bits10(                                         \
      const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr,       \
      int ref_stride, const uint8_t *second_pred) {                            \
    return fnname(src_ptr, source_stride, ref_ptr, ref_stride, second_pred) >> \
           2;                                                                  \
  }                                                                            \
  static unsigned int fnname##_bits12(                                         \
      const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr,       \
      int ref_stride, const uint8_t *second_pred) {                            \
    return fnname(src_ptr, source_stride, ref_ptr, ref_stride, second_pred) >> \
           4;                                                                  \
  }

#define MAKE_BFP_SAD4D_WRAPPER(fnname)                                        \
  static void fnname##_bits8(const uint8_t *src_ptr, int source_stride,       \
                             const uint8_t *const ref_ptr[], int ref_stride,  \
                             unsigned int *sad_array) {                       \
    fnname(src_ptr, source_stride, ref_ptr, ref_stride, sad_array);           \
  }                                                                           \
  static void fnname##_bits10(const uint8_t *src_ptr, int source_stride,      \
                              const uint8_t *const ref_ptr[], int ref_stride, \
                              unsigned int *sad_array) {                      \
    int i;                                                                    \
    fnname(src_ptr, source_stride, ref_ptr, ref_stride, sad_array);           \
    for (i = 0; i < 4; i++) sad_array[i] >>= 2;                               \
  }                                                                           \
  static void fnname##_bits12(const uint8_t *src_ptr, int source_stride,      \
                              const uint8_t *const ref_ptr[], int ref_stride, \
                              unsigned int *sad_array) {                      \
    int i;                                                                    \
    fnname(src_ptr, source_stride, ref_ptr, ref_stride, sad_array);           \
    for (i = 0; i < 4; i++) sad_array[i] >>= 4;                               \
  }

MAKE_BFP_SAD_WRAPPER(vpx_highbd_sad32x16)
MAKE_BFP_SADAVG_WRAPPER(vpx_highbd_sad32x16_avg)
MAKE_BFP_SAD4D_WRAPPER(vpx_highbd_sad32x16x4d)
MAKE_BFP_SAD_WRAPPER(vpx_highbd_sad16x32)
MAKE_BFP_SADAVG_WRAPPER(vpx_highbd_sad16x32_avg)
MAKE_BFP_SAD4D_WRAPPER(vpx_highbd_sad16x32x4d)
MAKE_BFP_SAD_WRAPPER(vpx_highbd_sad64x32)
MAKE_BFP_SADAVG_WRAPPER(vpx_highbd_sad64x32_avg)
MAKE_BFP_SAD4D_WRAPPER(vpx_highbd_sad64x32x4d)
MAKE_BFP_SAD_WRAPPER(vpx_highbd_sad32x64)
MAKE_BFP_SADAVG_WRAPPER(vpx_highbd_sad32x64_avg)
MAKE_BFP_SAD4D_WRAPPER(vpx_highbd_sad32x64x4d)
MAKE_BFP_SAD_WRAPPER(vpx_highbd_sad32x32)
MAKE_BFP_SADAVG_WRAPPER(vpx_highbd_sad32x32_avg)
MAKE_BFP_SAD4D_WRAPPER(vpx_highbd_sad32x32x4d)
MAKE_BFP_SAD_WRAPPER(vpx_highbd_sad64x64)
MAKE_BFP_SADAVG_WRAPPER(vpx_highbd_sad64x64_avg)
MAKE_BFP_SAD4D_WRAPPER(vpx_highbd_sad64x64x4d)
MAKE_BFP_SAD_WRAPPER(vpx_highbd_sad16x16)
MAKE_BFP_SADAVG_WRAPPER(vpx_highbd_sad16x16_avg)
MAKE_BFP_SAD4D_WRAPPER(vpx_highbd_sad16x16x4d)
MAKE_BFP_SAD_WRAPPER(vpx_highbd_sad16x8)
MAKE_BFP_SADAVG_WRAPPER(vpx_highbd_sad16x8_avg)
MAKE_BFP_SAD4D_WRAPPER(vpx_highbd_sad16x8x4d)
MAKE_BFP_SAD_WRAPPER(vpx_highbd_sad8x16)
MAKE_BFP_SADAVG_WRAPPER(vpx_highbd_sad8x16_avg)
MAKE_BFP_SAD4D_WRAPPER(vpx_highbd_sad8x16x4d)
MAKE_BFP_SAD_WRAPPER(vpx_highbd_sad8x8)
MAKE_BFP_SADAVG_WRAPPER(vpx_highbd_sad8x8_avg)
MAKE_BFP_SAD4D_WRAPPER(vpx_highbd_sad8x8x4d)
MAKE_BFP_SAD_WRAPPER(vpx_highbd_sad8x4)
MAKE_BFP_SADAVG_WRAPPER(vpx_highbd_sad8x4_avg)
MAKE_BFP_SAD4D_WRAPPER(vpx_highbd_sad8x4x4d)
MAKE_BFP_SAD_WRAPPER(vpx_highbd_sad4x8)
MAKE_BFP_SADAVG_WRAPPER(vpx_highbd_sad4x8_avg)
MAKE_BFP_SAD4D_WRAPPER(vpx_highbd_sad4x8x4d)
MAKE_BFP_SAD_WRAPPER(vpx_highbd_sad4x4)
MAKE_BFP_SADAVG_WRAPPER(vpx_highbd_sad4x4_avg)
MAKE_BFP_SAD4D_WRAPPER(vpx_highbd_sad4x4x4d)

static void highbd_set_var_fns(VP9_COMP *const cpi) {
  VP9_COMMON *const cm = &cpi->common;
  if (cm->use_highbitdepth) {
    switch (cm->bit_depth) {
      case VPX_BITS_8:
        HIGHBD_BFP(BLOCK_32X16, vpx_highbd_sad32x16_bits8,
                   vpx_highbd_sad32x16_avg_bits8, vpx_highbd_8_variance32x16,
                   vpx_highbd_8_sub_pixel_variance32x16,
                   vpx_highbd_8_sub_pixel_avg_variance32x16,
                   vpx_highbd_sad32x16x4d_bits8)

        HIGHBD_BFP(BLOCK_16X32, vpx_highbd_sad16x32_bits8,
                   vpx_highbd_sad16x32_avg_bits8, vpx_highbd_8_variance16x32,
                   vpx_highbd_8_sub_pixel_variance16x32,
                   vpx_highbd_8_sub_pixel_avg_variance16x32,
                   vpx_highbd_sad16x32x4d_bits8)

        HIGHBD_BFP(BLOCK_64X32, vpx_highbd_sad64x32_bits8,
                   vpx_highbd_sad64x32_avg_bits8, vpx_highbd_8_variance64x32,
                   vpx_highbd_8_sub_pixel_variance64x32,
                   vpx_highbd_8_sub_pixel_avg_variance64x32,
                   vpx_highbd_sad64x32x4d_bits8)

        HIGHBD_BFP(BLOCK_32X64, vpx_highbd_sad32x64_bits8,
                   vpx_highbd_sad32x64_avg_bits8, vpx_highbd_8_variance32x64,
                   vpx_highbd_8_sub_pixel_variance32x64,
                   vpx_highbd_8_sub_pixel_avg_variance32x64,
                   vpx_highbd_sad32x64x4d_bits8)

        HIGHBD_BFP(BLOCK_32X32, vpx_highbd_sad32x32_bits8,
                   vpx_highbd_sad32x32_avg_bits8, vpx_highbd_8_variance32x32,
                   vpx_highbd_8_sub_pixel_variance32x32,
                   vpx_highbd_8_sub_pixel_avg_variance32x32,
                   vpx_highbd_sad32x32x4d_bits8)

        HIGHBD_BFP(BLOCK_64X64, vpx_highbd_sad64x64_bits8,
                   vpx_highbd_sad64x64_avg_bits8, vpx_highbd_8_variance64x64,
                   vpx_highbd_8_sub_pixel_variance64x64,
                   vpx_highbd_8_sub_pixel_avg_variance64x64,
                   vpx_highbd_sad64x64x4d_bits8)

        HIGHBD_BFP(BLOCK_16X16, vpx_highbd_sad16x16_bits8,
                   vpx_highbd_sad16x16_avg_bits8, vpx_highbd_8_variance16x16,
                   vpx_highbd_8_sub_pixel_variance16x16,
                   vpx_highbd_8_sub_pixel_avg_variance16x16,
                   vpx_highbd_sad16x16x4d_bits8)

        HIGHBD_BFP(BLOCK_16X8, vpx_highbd_sad16x8_bits8,
                   vpx_highbd_sad16x8_avg_bits8, vpx_highbd_8_variance16x8,
                   vpx_highbd_8_sub_pixel_variance16x8,
                   vpx_highbd_8_sub_pixel_avg_variance16x8,
                   vpx_highbd_sad16x8x4d_bits8)

        HIGHBD_BFP(BLOCK_8X16, vpx_highbd_sad8x16_bits8,
                   vpx_highbd_sad8x16_avg_bits8, vpx_highbd_8_variance8x16,
                   vpx_highbd_8_sub_pixel_variance8x16,
                   vpx_highbd_8_sub_pixel_avg_variance8x16,
                   vpx_highbd_sad8x16x4d_bits8)

        HIGHBD_BFP(
            BLOCK_8X8, vpx_highbd_sad8x8_bits8, vpx_highbd_sad8x8_avg_bits8,
            vpx_highbd_8_variance8x8, vpx_highbd_8_sub_pixel_variance8x8,
            vpx_highbd_8_sub_pixel_avg_variance8x8, vpx_highbd_sad8x8x4d_bits8)

        HIGHBD_BFP(
            BLOCK_8X4, vpx_highbd_sad8x4_bits8, vpx_highbd_sad8x4_avg_bits8,
            vpx_highbd_8_variance8x4, vpx_highbd_8_sub_pixel_variance8x4,
            vpx_highbd_8_sub_pixel_avg_variance8x4, vpx_highbd_sad8x4x4d_bits8)

        HIGHBD_BFP(
            BLOCK_4X8, vpx_highbd_sad4x8_bits8, vpx_highbd_sad4x8_avg_bits8,
            vpx_highbd_8_variance4x8, vpx_highbd_8_sub_pixel_variance4x8,
            vpx_highbd_8_sub_pixel_avg_variance4x8, vpx_highbd_sad4x8x4d_bits8)

        HIGHBD_BFP(
            BLOCK_4X4, vpx_highbd_sad4x4_bits8, vpx_highbd_sad4x4_avg_bits8,
            vpx_highbd_8_variance4x4, vpx_highbd_8_sub_pixel_variance4x4,
            vpx_highbd_8_sub_pixel_avg_variance4x4, vpx_highbd_sad4x4x4d_bits8)
        break;

      case VPX_BITS_10:
        HIGHBD_BFP(BLOCK_32X16, vpx_highbd_sad32x16_bits10,
                   vpx_highbd_sad32x16_avg_bits10, vpx_highbd_10_variance32x16,
                   vpx_highbd_10_sub_pixel_variance32x16,
                   vpx_highbd_10_sub_pixel_avg_variance32x16,
                   vpx_highbd_sad32x16x4d_bits10)

        HIGHBD_BFP(BLOCK_16X32, vpx_highbd_sad16x32_bits10,
                   vpx_highbd_sad16x32_avg_bits10, vpx_highbd_10_variance16x32,
                   vpx_highbd_10_sub_pixel_variance16x32,
                   vpx_highbd_10_sub_pixel_avg_variance16x32,
                   vpx_highbd_sad16x32x4d_bits10)

        HIGHBD_BFP(BLOCK_64X32, vpx_highbd_sad64x32_bits10,
                   vpx_highbd_sad64x32_avg_bits10, vpx_highbd_10_variance64x32,
                   vpx_highbd_10_sub_pixel_variance64x32,
                   vpx_highbd_10_sub_pixel_avg_variance64x32,
                   vpx_highbd_sad64x32x4d_bits10)

        HIGHBD_BFP(BLOCK_32X64, vpx_highbd_sad32x64_bits10,
                   vpx_highbd_sad32x64_avg_bits10, vpx_highbd_10_variance32x64,
                   vpx_highbd_10_sub_pixel_variance32x64,
                   vpx_highbd_10_sub_pixel_avg_variance32x64,
                   vpx_highbd_sad32x64x4d_bits10)

        HIGHBD_BFP(BLOCK_32X32, vpx_highbd_sad32x32_bits10,
                   vpx_highbd_sad32x32_avg_bits10, vpx_highbd_10_variance32x32,
                   vpx_highbd_10_sub_pixel_variance32x32,
                   vpx_highbd_10_sub_pixel_avg_variance32x32,
                   vpx_highbd_sad32x32x4d_bits10)

        HIGHBD_BFP(BLOCK_64X64, vpx_highbd_sad64x64_bits10,
                   vpx_highbd_sad64x64_avg_bits10, vpx_highbd_10_variance64x64,
                   vpx_highbd_10_sub_pixel_variance64x64,
                   vpx_highbd_10_sub_pixel_avg_variance64x64,
                   vpx_highbd_sad64x64x4d_bits10)

        HIGHBD_BFP(BLOCK_16X16, vpx_highbd_sad16x16_bits10,
                   vpx_highbd_sad16x16_avg_bits10, vpx_highbd_10_variance16x16,
                   vpx_highbd_10_sub_pixel_variance16x16,
                   vpx_highbd_10_sub_pixel_avg_variance16x16,
                   vpx_highbd_sad16x16x4d_bits10)

        HIGHBD_BFP(BLOCK_16X8, vpx_highbd_sad16x8_bits10,
                   vpx_highbd_sad16x8_avg_bits10, vpx_highbd_10_variance16x8,
                   vpx_highbd_10_sub_pixel_variance16x8,
                   vpx_highbd_10_sub_pixel_avg_variance16x8,
                   vpx_highbd_sad16x8x4d_bits10)

        HIGHBD_BFP(BLOCK_8X16, vpx_highbd_sad8x16_bits10,
                   vpx_highbd_sad8x16_avg_bits10, vpx_highbd_10_variance8x16,
                   vpx_highbd_10_sub_pixel_variance8x16,
                   vpx_highbd_10_sub_pixel_avg_variance8x16,
                   vpx_highbd_sad8x16x4d_bits10)

        HIGHBD_BFP(BLOCK_8X8, vpx_highbd_sad8x8_bits10,
                   vpx_highbd_sad8x8_avg_bits10, vpx_highbd_10_variance8x8,
                   vpx_highbd_10_sub_pixel_variance8x8,
                   vpx_highbd_10_sub_pixel_avg_variance8x8,
                   vpx_highbd_sad8x8x4d_bits10)

        HIGHBD_BFP(BLOCK_8X4, vpx_highbd_sad8x4_bits10,
                   vpx_highbd_sad8x4_avg_bits10, vpx_highbd_10_variance8x4,
                   vpx_highbd_10_sub_pixel_variance8x4,
                   vpx_highbd_10_sub_pixel_avg_variance8x4,
                   vpx_highbd_sad8x4x4d_bits10)

        HIGHBD_BFP(BLOCK_4X8, vpx_highbd_sad4x8_bits10,
                   vpx_highbd_sad4x8_avg_bits10, vpx_highbd_10_variance4x8,
                   vpx_highbd_10_sub_pixel_variance4x8,
                   vpx_highbd_10_sub_pixel_avg_variance4x8,
                   vpx_highbd_sad4x8x4d_bits10)

        HIGHBD_BFP(BLOCK_4X4, vpx_highbd_sad4x4_bits10,
                   vpx_highbd_sad4x4_avg_bits10, vpx_highbd_10_variance4x4,
                   vpx_highbd_10_sub_pixel_variance4x4,
                   vpx_highbd_10_sub_pixel_avg_variance4x4,
                   vpx_highbd_sad4x4x4d_bits10)
        break;

      default:
        assert(cm->bit_depth == VPX_BITS_12);
        HIGHBD_BFP(BLOCK_32X16, vpx_highbd_sad32x16_bits12,
                   vpx_highbd_sad32x16_avg_bits12, vpx_highbd_12_variance32x16,
                   vpx_highbd_12_sub_pixel_variance32x16,
                   vpx_highbd_12_sub_pixel_avg_variance32x16,
                   vpx_highbd_sad32x16x4d_bits12)

        HIGHBD_BFP(BLOCK_16X32, vpx_highbd_sad16x32_bits12,
                   vpx_highbd_sad16x32_avg_bits12, vpx_highbd_12_variance16x32,
                   vpx_highbd_12_sub_pixel_variance16x32,
                   vpx_highbd_12_sub_pixel_avg_variance16x32,
                   vpx_highbd_sad16x32x4d_bits12)

        HIGHBD_BFP(BLOCK_64X32, vpx_highbd_sad64x32_bits12,
                   vpx_highbd_sad64x32_avg_bits12, vpx_highbd_12_variance64x32,
                   vpx_highbd_12_sub_pixel_variance64x32,
                   vpx_highbd_12_sub_pixel_avg_variance64x32,
                   vpx_highbd_sad64x32x4d_bits12)

        HIGHBD_BFP(BLOCK_32X64, vpx_highbd_sad32x64_bits12,
                   vpx_highbd_sad32x64_avg_bits12, vpx_highbd_12_variance32x64,
                   vpx_highbd_12_sub_pixel_variance32x64,
                   vpx_highbd_12_sub_pixel_avg_variance32x64,
                   vpx_highbd_sad32x64x4d_bits12)

        HIGHBD_BFP(BLOCK_32X32, vpx_highbd_sad32x32_bits12,
                   vpx_highbd_sad32x32_avg_bits12, vpx_highbd_12_variance32x32,
                   vpx_highbd_12_sub_pixel_variance32x32,
                   vpx_highbd_12_sub_pixel_avg_variance32x32,
                   vpx_highbd_sad32x32x4d_bits12)

        HIGHBD_BFP(BLOCK_64X64, vpx_highbd_sad64x64_bits12,
                   vpx_highbd_sad64x64_avg_bits12, vpx_highbd_12_variance64x64,
                   vpx_highbd_12_sub_pixel_variance64x64,
                   vpx_highbd_12_sub_pixel_avg_variance64x64,
                   vpx_highbd_sad64x64x4d_bits12)

        HIGHBD_BFP(BLOCK_16X16, vpx_highbd_sad16x16_bits12,
                   vpx_highbd_sad16x16_avg_bits12, vpx_highbd_12_variance16x16,
                   vpx_highbd_12_sub_pixel_variance16x16,
                   vpx_highbd_12_sub_pixel_avg_variance16x16,
                   vpx_highbd_sad16x16x4d_bits12)

        HIGHBD_BFP(BLOCK_16X8, vpx_highbd_sad16x8_bits12,
                   vpx_highbd_sad16x8_avg_bits12, vpx_highbd_12_variance16x8,
                   vpx_highbd_12_sub_pixel_variance16x8,
                   vpx_highbd_12_sub_pixel_avg_variance16x8,
                   vpx_highbd_sad16x8x4d_bits12)

        HIGHBD_BFP(BLOCK_8X16, vpx_highbd_sad8x16_bits12,
                   vpx_highbd_sad8x16_avg_bits12, vpx_highbd_12_variance8x16,
                   vpx_highbd_12_sub_pixel_variance8x16,
                   vpx_highbd_12_sub_pixel_avg_variance8x16,
                   vpx_highbd_sad8x16x4d_bits12)

        HIGHBD_BFP(BLOCK_8X8, vpx_highbd_sad8x8_bits12,
                   vpx_highbd_sad8x8_avg_bits12, vpx_highbd_12_variance8x8,
                   vpx_highbd_12_sub_pixel_variance8x8,
                   vpx_highbd_12_sub_pixel_avg_variance8x8,
                   vpx_highbd_sad8x8x4d_bits12)

        HIGHBD_BFP(BLOCK_8X4, vpx_highbd_sad8x4_bits12,
                   vpx_highbd_sad8x4_avg_bits12, vpx_highbd_12_variance8x4,
                   vpx_highbd_12_sub_pixel_variance8x4,
                   vpx_highbd_12_sub_pixel_avg_variance8x4,
                   vpx_highbd_sad8x4x4d_bits12)

        HIGHBD_BFP(BLOCK_4X8, vpx_highbd_sad4x8_bits12,
                   vpx_highbd_sad4x8_avg_bits12, vpx_highbd_12_variance4x8,
                   vpx_highbd_12_sub_pixel_variance4x8,
                   vpx_highbd_12_sub_pixel_avg_variance4x8,
                   vpx_highbd_sad4x8x4d_bits12)

        HIGHBD_BFP(BLOCK_4X4, vpx_highbd_sad4x4_bits12,
                   vpx_highbd_sad4x4_avg_bits12, vpx_highbd_12_variance4x4,
                   vpx_highbd_12_sub_pixel_variance4x4,
                   vpx_highbd_12_sub_pixel_avg_variance4x4,
                   vpx_highbd_sad4x4x4d_bits12)
        break;
    }
  }
}
#endif  // CONFIG_VP9_HIGHBITDEPTH

static void realloc_segmentation_maps(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;

  // Create the encoder segmentation map and set all entries to 0
  vpx_free(cpi->segmentation_map);
  CHECK_MEM_ERROR(cm, cpi->segmentation_map,
                  vpx_calloc(cm->mi_rows * cm->mi_cols, 1));

  // Create a map used for cyclic background refresh.
  if (cpi->cyclic_refresh) vp9_cyclic_refresh_free(cpi->cyclic_refresh);
  CHECK_MEM_ERROR(cm, cpi->cyclic_refresh,
                  vp9_cyclic_refresh_alloc(cm->mi_rows, cm->mi_cols));

  // Create a map used to mark inactive areas.
  vpx_free(cpi->active_map.map);
  CHECK_MEM_ERROR(cm, cpi->active_map.map,
                  vpx_calloc(cm->mi_rows * cm->mi_cols, 1));

  // And a place holder structure is the coding context
  // for use if we want to save and restore it
  vpx_free(cpi->coding_context.last_frame_seg_map_copy);
  CHECK_MEM_ERROR(cm, cpi->coding_context.last_frame_seg_map_copy,
                  vpx_calloc(cm->mi_rows * cm->mi_cols, 1));
}

static void alloc_copy_partition_data(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  if (cpi->prev_partition == NULL) {
    CHECK_MEM_ERROR(cm, cpi->prev_partition,
                    (BLOCK_SIZE *)vpx_calloc(cm->mi_stride * cm->mi_rows,
                                             sizeof(*cpi->prev_partition)));
  }
  if (cpi->prev_segment_id == NULL) {
    CHECK_MEM_ERROR(
        cm, cpi->prev_segment_id,
        (int8_t *)vpx_calloc((cm->mi_stride >> 3) * ((cm->mi_rows >> 3) + 1),
                             sizeof(*cpi->prev_segment_id)));
  }
  if (cpi->prev_variance_low == NULL) {
    CHECK_MEM_ERROR(cm, cpi->prev_variance_low,
                    (uint8_t *)vpx_calloc(
                        (cm->mi_stride >> 3) * ((cm->mi_rows >> 3) + 1) * 25,
                        sizeof(*cpi->prev_variance_low)));
  }
  if (cpi->copied_frame_cnt == NULL) {
    CHECK_MEM_ERROR(
        cm, cpi->copied_frame_cnt,
        (uint8_t *)vpx_calloc((cm->mi_stride >> 3) * ((cm->mi_rows >> 3) + 1),
                              sizeof(*cpi->copied_frame_cnt)));
  }
}

void vp9_change_config(struct VP9_COMP *cpi, const VP9EncoderConfig *oxcf) {
  VP9_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  int last_w = cpi->oxcf.width;
  int last_h = cpi->oxcf.height;

  vp9_init_quantizer(cpi);
  if (cm->profile != oxcf->profile) cm->profile = oxcf->profile;
  cm->bit_depth = oxcf->bit_depth;
  cm->color_space = oxcf->color_space;
  cm->color_range = oxcf->color_range;

  cpi->target_level = oxcf->target_level;
  cpi->keep_level_stats = oxcf->target_level != LEVEL_MAX;
  set_level_constraint(&cpi->level_constraint,
                       get_level_index(cpi->target_level));

  if (cm->profile <= PROFILE_1)
    assert(cm->bit_depth == VPX_BITS_8);
  else
    assert(cm->bit_depth > VPX_BITS_8);

  cpi->oxcf = *oxcf;
#if CONFIG_VP9_HIGHBITDEPTH
  cpi->td.mb.e_mbd.bd = (int)cm->bit_depth;
#endif  // CONFIG_VP9_HIGHBITDEPTH

  if ((oxcf->pass == 0) && (oxcf->rc_mode == VPX_Q)) {
    rc->baseline_gf_interval = FIXED_GF_INTERVAL;
  } else {
    rc->baseline_gf_interval = (MIN_GF_INTERVAL + MAX_GF_INTERVAL) / 2;
  }

  cpi->refresh_golden_frame = 0;
  cpi->refresh_last_frame = 1;
  cm->refresh_frame_context = 1;
  cm->reset_frame_context = 0;

  vp9_reset_segment_features(&cm->seg);
  vp9_set_high_precision_mv(cpi, 0);

  {
    int i;

    for (i = 0; i < MAX_SEGMENTS; i++)
      cpi->segment_encode_breakout[i] = cpi->oxcf.encode_breakout;
  }
  cpi->encode_breakout = cpi->oxcf.encode_breakout;

  vp9_set_rc_buffer_sizes(cpi);

  // Set up frame rate and related parameters rate control values.
  vp9_new_framerate(cpi, cpi->framerate);

  // Set absolute upper and lower quality limits
  rc->worst_quality = cpi->oxcf.worst_allowed_q;
  rc->best_quality = cpi->oxcf.best_allowed_q;

  cm->interp_filter = cpi->sf.default_interp_filter;

  if (cpi->oxcf.render_width > 0 && cpi->oxcf.render_height > 0) {
    cm->render_width = cpi->oxcf.render_width;
    cm->render_height = cpi->oxcf.render_height;
  } else {
    cm->render_width = cpi->oxcf.width;
    cm->render_height = cpi->oxcf.height;
  }
  if (last_w != cpi->oxcf.width || last_h != cpi->oxcf.height) {
    cm->width = cpi->oxcf.width;
    cm->height = cpi->oxcf.height;
    cpi->external_resize = 1;
  }

  if (cpi->initial_width) {
    int new_mi_size = 0;
    vp9_set_mb_mi(cm, cm->width, cm->height);
    new_mi_size = cm->mi_stride * calc_mi_size(cm->mi_rows);
    if (cm->mi_alloc_size < new_mi_size) {
      vp9_free_context_buffers(cm);
      alloc_compressor_data(cpi);
      realloc_segmentation_maps(cpi);
      cpi->initial_width = cpi->initial_height = 0;
      cpi->external_resize = 0;
    } else if (cm->mi_alloc_size == new_mi_size &&
               (cpi->oxcf.width > last_w || cpi->oxcf.height > last_h)) {
      vp9_alloc_loop_filter(cm);
    }
  }

  if (cm->current_video_frame == 0 || last_w != cpi->oxcf.width ||
      last_h != cpi->oxcf.height)
    update_frame_size(cpi);

  if (last_w != cpi->oxcf.width || last_h != cpi->oxcf.height) {
    memset(cpi->consec_zero_mv, 0,
           cm->mi_rows * cm->mi_cols * sizeof(*cpi->consec_zero_mv));
    if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ)
      vp9_cyclic_refresh_reset_resize(cpi);
    rc->rc_1_frame = 0;
    rc->rc_2_frame = 0;
  }

  if ((cpi->svc.number_temporal_layers > 1 && cpi->oxcf.rc_mode == VPX_CBR) ||
      ((cpi->svc.number_temporal_layers > 1 ||
        cpi->svc.number_spatial_layers > 1) &&
       cpi->oxcf.pass != 1)) {
    vp9_update_layer_context_change_config(cpi,
                                           (int)cpi->oxcf.target_bandwidth);
  }

  vp9_check_reset_rc_flag(cpi);

  cpi->alt_ref_source = NULL;
  rc->is_src_frame_alt_ref = 0;

#if 0
  // Experimental RD Code
  cpi->frame_distortion = 0;
  cpi->last_frame_distortion = 0;
#endif

  set_tile_limits(cpi);

  cpi->ext_refresh_frame_flags_pending = 0;
  cpi->ext_refresh_frame_context_pending = 0;

#if CONFIG_VP9_HIGHBITDEPTH
  highbd_set_var_fns(cpi);
#endif

  vp9_set_row_mt(cpi);
}

#ifndef M_LOG2_E
#define M_LOG2_E 0.693147180559945309417
#endif
#define log2f(x) (log(x) / (float)M_LOG2_E)

/***********************************************************************
 * Read before modifying 'cal_nmvjointsadcost' or 'cal_nmvsadcosts'    *
 ***********************************************************************
 * The following 2 functions ('cal_nmvjointsadcost' and                *
 * 'cal_nmvsadcosts') are used to calculate cost lookup tables         *
 * used by 'vp9_diamond_search_sad'. The C implementation of the       *
 * function is generic, but the AVX intrinsics optimised version       *
 * relies on the following properties of the computed tables:          *
 * For cal_nmvjointsadcost:                                            *
 *   - mvjointsadcost[1] == mvjointsadcost[2] == mvjointsadcost[3]     *
 * For cal_nmvsadcosts:                                                *
 *   - For all i: mvsadcost[0][i] == mvsadcost[1][i]                   *
 *         (Equal costs for both components)                           *
 *   - For all i: mvsadcost[0][i] == mvsadcost[0][-i]                  *
 *         (Cost function is even)                                     *
 * If these do not hold, then the AVX optimised version of the         *
 * 'vp9_diamond_search_sad' function cannot be used as it is, in which *
 * case you can revert to using the C function instead.                *
 ***********************************************************************/

static void cal_nmvjointsadcost(int *mvjointsadcost) {
  /*********************************************************************
   * Warning: Read the comments above before modifying this function   *
   *********************************************************************/
  mvjointsadcost[0] = 600;
  mvjointsadcost[1] = 300;
  mvjointsadcost[2] = 300;
  mvjointsadcost[3] = 300;
}

static void cal_nmvsadcosts(int *mvsadcost[2]) {
  /*********************************************************************
   * Warning: Read the comments above before modifying this function   *
   *********************************************************************/
  int i = 1;

  mvsadcost[0][0] = 0;
  mvsadcost[1][0] = 0;

  do {
    double z = 256 * (2 * (log2f(8 * i) + .6));
    mvsadcost[0][i] = (int)z;
    mvsadcost[1][i] = (int)z;
    mvsadcost[0][-i] = (int)z;
    mvsadcost[1][-i] = (int)z;
  } while (++i <= MV_MAX);
}

static void cal_nmvsadcosts_hp(int *mvsadcost[2]) {
  int i = 1;

  mvsadcost[0][0] = 0;
  mvsadcost[1][0] = 0;

  do {
    double z = 256 * (2 * (log2f(8 * i) + .6));
    mvsadcost[0][i] = (int)z;
    mvsadcost[1][i] = (int)z;
    mvsadcost[0][-i] = (int)z;
    mvsadcost[1][-i] = (int)z;
  } while (++i <= MV_MAX);
}

static void init_ref_frame_bufs(VP9_COMMON *cm) {
  int i;
  BufferPool *const pool = cm->buffer_pool;
  cm->new_fb_idx = INVALID_IDX;
  for (i = 0; i < REF_FRAMES; ++i) {
    cm->ref_frame_map[i] = INVALID_IDX;
  }
  for (i = 0; i < FRAME_BUFFERS; ++i) {
    pool->frame_bufs[i].ref_count = 0;
  }
}

static void update_initial_width(VP9_COMP *cpi, int use_highbitdepth,
                                 int subsampling_x, int subsampling_y) {
  VP9_COMMON *const cm = &cpi->common;
#if !CONFIG_VP9_HIGHBITDEPTH
  (void)use_highbitdepth;
  assert(use_highbitdepth == 0);
#endif

  if (!cpi->initial_width ||
#if CONFIG_VP9_HIGHBITDEPTH
      cm->use_highbitdepth != use_highbitdepth ||
#endif
      cm->subsampling_x != subsampling_x ||
      cm->subsampling_y != subsampling_y) {
    cm->subsampling_x = subsampling_x;
    cm->subsampling_y = subsampling_y;
#if CONFIG_VP9_HIGHBITDEPTH
    cm->use_highbitdepth = use_highbitdepth;
#endif
    alloc_util_frame_buffers(cpi);
    cpi->initial_width = cm->width;
    cpi->initial_height = cm->height;
    cpi->initial_mbs = cm->MBs;
  }
}

// TODO(angiebird): Check whether we can move this function to vpx_image.c
static INLINE void vpx_img_chroma_subsampling(vpx_img_fmt_t fmt,
                                              unsigned int *subsampling_x,
                                              unsigned int *subsampling_y) {
  switch (fmt) {
    case VPX_IMG_FMT_I420:
    case VPX_IMG_FMT_YV12:
    case VPX_IMG_FMT_I422:
    case VPX_IMG_FMT_I42016:
    case VPX_IMG_FMT_I42216: *subsampling_x = 1; break;
    default: *subsampling_x = 0; break;
  }

  switch (fmt) {
    case VPX_IMG_FMT_I420:
    case VPX_IMG_FMT_I440:
    case VPX_IMG_FMT_YV12:
    case VPX_IMG_FMT_I42016:
    case VPX_IMG_FMT_I44016: *subsampling_y = 1; break;
    default: *subsampling_y = 0; break;
  }
}

// TODO(angiebird): Check whether we can move this function to vpx_image.c
static INLINE int vpx_img_use_highbitdepth(vpx_img_fmt_t fmt) {
  return fmt & VPX_IMG_FMT_HIGHBITDEPTH;
}

#if CONFIG_VP9_TEMPORAL_DENOISING
static void setup_denoiser_buffer(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  if (cpi->oxcf.noise_sensitivity > 0 &&
      !cpi->denoiser.frame_buffer_initialized) {
    if (vp9_denoiser_alloc(cm, &cpi->svc, &cpi->denoiser, cpi->use_svc,
                           cpi->oxcf.noise_sensitivity, cm->width, cm->height,
                           cm->subsampling_x, cm->subsampling_y,
#if CONFIG_VP9_HIGHBITDEPTH
                           cm->use_highbitdepth,
#endif
                           VP9_ENC_BORDER_IN_PIXELS))
      vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                         "Failed to allocate denoiser");
  }
}
#endif

void vp9_update_compressor_with_img_fmt(VP9_COMP *cpi, vpx_img_fmt_t img_fmt) {
  const VP9EncoderConfig *oxcf = &cpi->oxcf;
  unsigned int subsampling_x, subsampling_y;
  const int use_highbitdepth = vpx_img_use_highbitdepth(img_fmt);
  vpx_img_chroma_subsampling(img_fmt, &subsampling_x, &subsampling_y);

  update_initial_width(cpi, use_highbitdepth, subsampling_x, subsampling_y);
#if CONFIG_VP9_TEMPORAL_DENOISING
  setup_denoiser_buffer(cpi);
#endif

  assert(cpi->lookahead == NULL);
  cpi->lookahead = vp9_lookahead_init(oxcf->width, oxcf->height, subsampling_x,
                                      subsampling_y,
#if CONFIG_VP9_HIGHBITDEPTH
                                      use_highbitdepth,
#endif
                                      oxcf->lag_in_frames);
  alloc_raw_frame_buffers(cpi);
}

VP9_COMP *vp9_create_compressor(const VP9EncoderConfig *oxcf,
                                BufferPool *const pool) {
  unsigned int i;
  VP9_COMP *volatile const cpi = vpx_memalign(32, sizeof(VP9_COMP));
  VP9_COMMON *volatile const cm = cpi != NULL ? &cpi->common : NULL;

  if (!cm) return NULL;

  vp9_zero(*cpi);

  if (setjmp(cm->error.jmp)) {
    cm->error.setjmp = 0;
    vp9_remove_compressor(cpi);
    return 0;
  }

  cm->error.setjmp = 1;
  cm->alloc_mi = vp9_enc_alloc_mi;
  cm->free_mi = vp9_enc_free_mi;
  cm->setup_mi = vp9_enc_setup_mi;

  CHECK_MEM_ERROR(cm, cm->fc, (FRAME_CONTEXT *)vpx_calloc(1, sizeof(*cm->fc)));
  CHECK_MEM_ERROR(
      cm, cm->frame_contexts,
      (FRAME_CONTEXT *)vpx_calloc(FRAME_CONTEXTS, sizeof(*cm->frame_contexts)));

  cpi->use_svc = 0;
  cpi->resize_state = ORIG;
  cpi->external_resize = 0;
  cpi->resize_avg_qp = 0;
  cpi->resize_buffer_underflow = 0;
  cpi->use_skin_detection = 0;
  cpi->common.buffer_pool = pool;
  init_ref_frame_bufs(cm);

  cpi->force_update_segmentation = 0;

  init_config(cpi, oxcf);
  cpi->frame_info = vp9_get_frame_info(oxcf);

  vp9_rc_init(&cpi->oxcf, oxcf->pass, &cpi->rc);

  init_frame_indexes(cm);
  cpi->partition_search_skippable_frame = 0;
  cpi->tile_data = NULL;

  realloc_segmentation_maps(cpi);

  CHECK_MEM_ERROR(
      cm, cpi->skin_map,
      vpx_calloc(cm->mi_rows * cm->mi_cols, sizeof(cpi->skin_map[0])));

#if !CONFIG_REALTIME_ONLY
  CHECK_MEM_ERROR(cm, cpi->alt_ref_aq, vp9_alt_ref_aq_create());
#endif

  CHECK_MEM_ERROR(
      cm, cpi->consec_zero_mv,
      vpx_calloc(cm->mi_rows * cm->mi_cols, sizeof(*cpi->consec_zero_mv)));

  CHECK_MEM_ERROR(cm, cpi->nmvcosts[0],
                  vpx_calloc(MV_VALS, sizeof(*cpi->nmvcosts[0])));
  CHECK_MEM_ERROR(cm, cpi->nmvcosts[1],
                  vpx_calloc(MV_VALS, sizeof(*cpi->nmvcosts[1])));
  CHECK_MEM_ERROR(cm, cpi->nmvcosts_hp[0],
                  vpx_calloc(MV_VALS, sizeof(*cpi->nmvcosts_hp[0])));
  CHECK_MEM_ERROR(cm, cpi->nmvcosts_hp[1],
                  vpx_calloc(MV_VALS, sizeof(*cpi->nmvcosts_hp[1])));
  CHECK_MEM_ERROR(cm, cpi->nmvsadcosts[0],
                  vpx_calloc(MV_VALS, sizeof(*cpi->nmvsadcosts[0])));
  CHECK_MEM_ERROR(cm, cpi->nmvsadcosts[1],
                  vpx_calloc(MV_VALS, sizeof(*cpi->nmvsadcosts[1])));
  CHECK_MEM_ERROR(cm, cpi->nmvsadcosts_hp[0],
                  vpx_calloc(MV_VALS, sizeof(*cpi->nmvsadcosts_hp[0])));
  CHECK_MEM_ERROR(cm, cpi->nmvsadcosts_hp[1],
                  vpx_calloc(MV_VALS, sizeof(*cpi->nmvsadcosts_hp[1])));

  for (i = 0; i < (sizeof(cpi->mbgraph_stats) / sizeof(cpi->mbgraph_stats[0]));
       i++) {
    CHECK_MEM_ERROR(
        cm, cpi->mbgraph_stats[i].mb_stats,
        vpx_calloc(cm->MBs * sizeof(*cpi->mbgraph_stats[i].mb_stats), 1));
  }

#if CONFIG_FP_MB_STATS
  cpi->use_fp_mb_stats = 0;
  if (cpi->use_fp_mb_stats) {
    // a place holder used to store the first pass mb stats in the first pass
    CHECK_MEM_ERROR(cm, cpi->twopass.frame_mb_stats_buf,
                    vpx_calloc(cm->MBs * sizeof(uint8_t), 1));
  } else {
    cpi->twopass.frame_mb_stats_buf = NULL;
  }
#endif

  cpi->refresh_alt_ref_frame = 0;
  cpi->b_calculate_psnr = CONFIG_INTERNAL_STATS;

  init_level_info(&cpi->level_info);
  init_level_constraint(&cpi->level_constraint);

#if CONFIG_INTERNAL_STATS
  cpi->b_calculate_blockiness = 1;
  cpi->b_calculate_consistency = 1;
  cpi->total_inconsistency = 0;
  cpi->psnr.worst = 100.0;
  cpi->worst_ssim = 100.0;

  cpi->count = 0;
  cpi->bytes = 0;

  if (cpi->b_calculate_psnr) {
    cpi->total_sq_error = 0;
    cpi->total_samples = 0;

    cpi->totalp_sq_error = 0;
    cpi->totalp_samples = 0;

    cpi->tot_recode_hits = 0;
    cpi->summed_quality = 0;
    cpi->summed_weights = 0;
    cpi->summedp_quality = 0;
    cpi->summedp_weights = 0;
  }

  cpi->fastssim.worst = 100.0;

  cpi->psnrhvs.worst = 100.0;

  if (cpi->b_calculate_blockiness) {
    cpi->total_blockiness = 0;
    cpi->worst_blockiness = 0.0;
  }

  if (cpi->b_calculate_consistency) {
    CHECK_MEM_ERROR(cm, cpi->ssim_vars,
                    vpx_calloc(cpi->common.mi_rows * cpi->common.mi_cols,
                               sizeof(*cpi->ssim_vars) * 4));
    cpi->worst_consistency = 100.0;
  } else {
    cpi->ssim_vars = NULL;
  }

#endif

  cpi->first_time_stamp_ever = INT64_MAX;

  /*********************************************************************
   * Warning: Read the comments around 'cal_nmvjointsadcost' and       *
   * 'cal_nmvsadcosts' before modifying how these tables are computed. *
   *********************************************************************/
  cal_nmvjointsadcost(cpi->td.mb.nmvjointsadcost);
  cpi->td.mb.nmvcost[0] = &cpi->nmvcosts[0][MV_MAX];
  cpi->td.mb.nmvcost[1] = &cpi->nmvcosts[1][MV_MAX];
  cpi->td.mb.nmvsadcost[0] = &cpi->nmvsadcosts[0][MV_MAX];
  cpi->td.mb.nmvsadcost[1] = &cpi->nmvsadcosts[1][MV_MAX];
  cal_nmvsadcosts(cpi->td.mb.nmvsadcost);

  cpi->td.mb.nmvcost_hp[0] = &cpi->nmvcosts_hp[0][MV_MAX];
  cpi->td.mb.nmvcost_hp[1] = &cpi->nmvcosts_hp[1][MV_MAX];
  cpi->td.mb.nmvsadcost_hp[0] = &cpi->nmvsadcosts_hp[0][MV_MAX];
  cpi->td.mb.nmvsadcost_hp[1] = &cpi->nmvsadcosts_hp[1][MV_MAX];
  cal_nmvsadcosts_hp(cpi->td.mb.nmvsadcost_hp);

#if CONFIG_VP9_TEMPORAL_DENOISING
#ifdef OUTPUT_YUV_DENOISED
  yuv_denoised_file = fopen("denoised.yuv", "ab");
#endif
#endif
#ifdef OUTPUT_YUV_SKINMAP
  yuv_skinmap_file = fopen("skinmap.yuv", "wb");
#endif
#ifdef OUTPUT_YUV_REC
  yuv_rec_file = fopen("rec.yuv", "wb");
#endif
#ifdef OUTPUT_YUV_SVC_SRC
  yuv_svc_src[0] = fopen("svc_src_0.yuv", "wb");
  yuv_svc_src[1] = fopen("svc_src_1.yuv", "wb");
  yuv_svc_src[2] = fopen("svc_src_2.yuv", "wb");
#endif

#if 0
  framepsnr = fopen("framepsnr.stt", "a");
  kf_list = fopen("kf_list.stt", "w");
#endif

  cpi->allow_encode_breakout = ENCODE_BREAKOUT_ENABLED;

  {
    vpx_codec_err_t codec_status = vp9_extrc_init(&cpi->ext_ratectrl);
    if (codec_status != VPX_CODEC_OK) {
      vpx_internal_error(&cm->error, codec_status, "vp9_extrc_init() failed");
    }
  }

#if !CONFIG_REALTIME_ONLY
  if (oxcf->pass == 1) {
    vp9_init_first_pass(cpi);
  } else if (oxcf->pass == 2) {
    const size_t packet_sz = sizeof(FIRSTPASS_STATS);
    const int packets = (int)(oxcf->two_pass_stats_in.sz / packet_sz);

    if (cpi->svc.number_spatial_layers > 1 ||
        cpi->svc.number_temporal_layers > 1) {
      FIRSTPASS_STATS *const stats = oxcf->two_pass_stats_in.buf;
      FIRSTPASS_STATS *stats_copy[VPX_SS_MAX_LAYERS] = { 0 };
      int i;

      for (i = 0; i < oxcf->ss_number_layers; ++i) {
        FIRSTPASS_STATS *const last_packet_for_layer =
            &stats[packets - oxcf->ss_number_layers + i];
        const int layer_id = (int)last_packet_for_layer->spatial_layer_id;
        const int packets_in_layer = (int)last_packet_for_layer->count + 1;
        if (layer_id >= 0 && layer_id < oxcf->ss_number_layers) {
          int num_frames;
          LAYER_CONTEXT *const lc = &cpi->svc.layer_context[layer_id];

          vpx_free(lc->rc_twopass_stats_in.buf);

          lc->rc_twopass_stats_in.sz = packets_in_layer * packet_sz;
          CHECK_MEM_ERROR(cm, lc->rc_twopass_stats_in.buf,
                          vpx_malloc(lc->rc_twopass_stats_in.sz));
          lc->twopass.stats_in_start = lc->rc_twopass_stats_in.buf;
          lc->twopass.stats_in = lc->twopass.stats_in_start;
          lc->twopass.stats_in_end =
              lc->twopass.stats_in_start + packets_in_layer - 1;
          // Note the last packet is cumulative first pass stats.
          // So the number of frames is packet number minus one
          num_frames = packets_in_layer - 1;
          fps_init_first_pass_info(&lc->twopass.first_pass_info,
                                   lc->rc_twopass_stats_in.buf, num_frames);
          stats_copy[layer_id] = lc->rc_twopass_stats_in.buf;
        }
      }

      for (i = 0; i < packets; ++i) {
        const int layer_id = (int)stats[i].spatial_layer_id;
        if (layer_id >= 0 && layer_id < oxcf->ss_number_layers &&
            stats_copy[layer_id] != NULL) {
          *stats_copy[layer_id] = stats[i];
          ++stats_copy[layer_id];
        }
      }

      vp9_init_second_pass_spatial_svc(cpi);
    } else {
      int num_frames;
#if CONFIG_FP_MB_STATS
      if (cpi->use_fp_mb_stats) {
        const size_t psz = cpi->common.MBs * sizeof(uint8_t);
        const int ps = (int)(oxcf->firstpass_mb_stats_in.sz / psz);

        cpi->twopass.firstpass_mb_stats.mb_stats_start =
            oxcf->firstpass_mb_stats_in.buf;
        cpi->twopass.firstpass_mb_stats.mb_stats_end =
            cpi->twopass.firstpass_mb_stats.mb_stats_start +
            (ps - 1) * cpi->common.MBs * sizeof(uint8_t);
      }
#endif

      cpi->twopass.stats_in_start = oxcf->two_pass_stats_in.buf;
      cpi->twopass.stats_in = cpi->twopass.stats_in_start;
      cpi->twopass.stats_in_end = &cpi->twopass.stats_in[packets - 1];
      // Note the last packet is cumulative first pass stats.
      // So the number of frames is packet number minus one
      num_frames = packets - 1;
      fps_init_first_pass_info(&cpi->twopass.first_pass_info,
                               oxcf->two_pass_stats_in.buf, num_frames);

      vp9_init_second_pass(cpi);
    }
  }
#endif  // !CONFIG_REALTIME_ONLY

  cpi->mb_wiener_var_cols = 0;
  cpi->mb_wiener_var_rows = 0;
  cpi->mb_wiener_variance = NULL;

  vp9_set_speed_features_framesize_independent(cpi, oxcf->speed);
  vp9_set_speed_features_framesize_dependent(cpi, oxcf->speed);

  {
    const int bsize = BLOCK_16X16;
    const int w = num_8x8_blocks_wide_lookup[bsize];
    const int h = num_8x8_blocks_high_lookup[bsize];
    const int num_cols = (cm->mi_cols + w - 1) / w;
    const int num_rows = (cm->mi_rows + h - 1) / h;
    CHECK_MEM_ERROR(cm, cpi->mi_ssim_rdmult_scaling_factors,
                    vpx_calloc(num_rows * num_cols,
                               sizeof(*cpi->mi_ssim_rdmult_scaling_factors)));
  }

  cpi->kmeans_data_arr_alloc = 0;
#if CONFIG_NON_GREEDY_MV
  cpi->tpl_ready = 0;
#endif  // CONFIG_NON_GREEDY_MV
  for (i = 0; i < MAX_ARF_GOP_SIZE; ++i) cpi->tpl_stats[i].tpl_stats_ptr = NULL;

  // Allocate memory to store variances for a frame.
  CHECK_MEM_ERROR(cm, cpi->source_diff_var, vpx_calloc(cm->MBs, sizeof(diff)));
  cpi->source_var_thresh = 0;
  cpi->frames_till_next_var_check = 0;
#define BFP(BT, SDF, SDAF, VF, SVF, SVAF, SDX4DF, SDX8F) \
  cpi->fn_ptr[BT].sdf = SDF;                             \
  cpi->fn_ptr[BT].sdaf = SDAF;                           \
  cpi->fn_ptr[BT].vf = VF;                               \
  cpi->fn_ptr[BT].svf = SVF;                             \
  cpi->fn_ptr[BT].svaf = SVAF;                           \
  cpi->fn_ptr[BT].sdx4df = SDX4DF;                       \
  cpi->fn_ptr[BT].sdx8f = SDX8F;

  // TODO(angiebird): make sdx8f available for every block size
  BFP(BLOCK_32X16, vpx_sad32x16, vpx_sad32x16_avg, vpx_variance32x16,
      vpx_sub_pixel_variance32x16, vpx_sub_pixel_avg_variance32x16,
      vpx_sad32x16x4d, NULL)

  BFP(BLOCK_16X32, vpx_sad16x32, vpx_sad16x32_avg, vpx_variance16x32,
      vpx_sub_pixel_variance16x32, vpx_sub_pixel_avg_variance16x32,
      vpx_sad16x32x4d, NULL)

  BFP(BLOCK_64X32, vpx_sad64x32, vpx_sad64x32_avg, vpx_variance64x32,
      vpx_sub_pixel_variance64x32, vpx_sub_pixel_avg_variance64x32,
      vpx_sad64x32x4d, NULL)

  BFP(BLOCK_32X64, vpx_sad32x64, vpx_sad32x64_avg, vpx_variance32x64,
      vpx_sub_pixel_variance32x64, vpx_sub_pixel_avg_variance32x64,
      vpx_sad32x64x4d, NULL)

  BFP(BLOCK_32X32, vpx_sad32x32, vpx_sad32x32_avg, vpx_variance32x32,
      vpx_sub_pixel_variance32x32, vpx_sub_pixel_avg_variance32x32,
      vpx_sad32x32x4d, vpx_sad32x32x8)

  BFP(BLOCK_64X64, vpx_sad64x64, vpx_sad64x64_avg, vpx_variance64x64,
      vpx_sub_pixel_variance64x64, vpx_sub_pixel_avg_variance64x64,
      vpx_sad64x64x4d, NULL)

  BFP(BLOCK_16X16, vpx_sad16x16, vpx_sad16x16_avg, vpx_variance16x16,
      vpx_sub_pixel_variance16x16, vpx_sub_pixel_avg_variance16x16,
      vpx_sad16x16x4d, vpx_sad16x16x8)

  BFP(BLOCK_16X8, vpx_sad16x8, vpx_sad16x8_avg, vpx_variance16x8,
      vpx_sub_pixel_variance16x8, vpx_sub_pixel_avg_variance16x8,
      vpx_sad16x8x4d, vpx_sad16x8x8)

  BFP(BLOCK_8X16, vpx_sad8x16, vpx_sad8x16_avg, vpx_variance8x16,
      vpx_sub_pixel_variance8x16, vpx_sub_pixel_avg_variance8x16,
      vpx_sad8x16x4d, vpx_sad8x16x8)

  BFP(BLOCK_8X8, vpx_sad8x8, vpx_sad8x8_avg, vpx_variance8x8,
      vpx_sub_pixel_variance8x8, vpx_sub_pixel_avg_variance8x8, vpx_sad8x8x4d,
      vpx_sad8x8x8)

  BFP(BLOCK_8X4, vpx_sad8x4, vpx_sad8x4_avg, vpx_variance8x4,
      vpx_sub_pixel_variance8x4, vpx_sub_pixel_avg_variance8x4, vpx_sad8x4x4d,
      NULL)

  BFP(BLOCK_4X8, vpx_sad4x8, vpx_sad4x8_avg, vpx_variance4x8,
      vpx_sub_pixel_variance4x8, vpx_sub_pixel_avg_variance4x8, vpx_sad4x8x4d,
      NULL)

  BFP(BLOCK_4X4, vpx_sad4x4, vpx_sad4x4_avg, vpx_variance4x4,
      vpx_sub_pixel_variance4x4, vpx_sub_pixel_avg_variance4x4, vpx_sad4x4x4d,
      vpx_sad4x4x8)

#if CONFIG_VP9_HIGHBITDEPTH
  highbd_set_var_fns(cpi);
#endif

  /* vp9_init_quantizer() is first called here. Add check in
   * vp9_frame_init_quantizer() so that vp9_init_quantizer is only
   * called later when needed. This will avoid unnecessary calls of
   * vp9_init_quantizer() for every frame.
   */
  vp9_init_quantizer(cpi);

  vp9_loop_filter_init(cm);

  // Set up the unit scaling factor used during motion search.
#if CONFIG_VP9_HIGHBITDEPTH
  vp9_setup_scale_factors_for_frame(&cpi->me_sf, cm->width, cm->height,
                                    cm->width, cm->height,
                                    cm->use_highbitdepth);
#else
  vp9_setup_scale_factors_for_frame(&cpi->me_sf, cm->width, cm->height,
                                    cm->width, cm->height);
#endif  // CONFIG_VP9_HIGHBITDEPTH
  cpi->td.mb.me_sf = &cpi->me_sf;

  cm->error.setjmp = 0;

#if CONFIG_RATE_CTRL
  encode_command_init(&cpi->encode_command);
  partition_info_init(cpi);
  motion_vector_info_init(cpi);
  fp_motion_vector_info_init(cpi);
  tpl_stats_info_init(cpi);
#endif

  return cpi;
}

#if CONFIG_INTERNAL_STATS
#define SNPRINT(H, T) snprintf((H) + strlen(H), sizeof(H) - strlen(H), (T))

#define SNPRINT2(H, T, V) \
  snprintf((H) + strlen(H), sizeof(H) - strlen(H), (T), (V))
#endif  // CONFIG_INTERNAL_STATS

static void free_tpl_buffer(VP9_COMP *cpi);

void vp9_remove_compressor(VP9_COMP *cpi) {
  VP9_COMMON *cm;
  unsigned int i;
  int t;

  if (!cpi) return;

#if CONFIG_INTERNAL_STATS
  vpx_free(cpi->ssim_vars);
#endif

  cm = &cpi->common;
  if (cm->current_video_frame > 0) {
#if CONFIG_INTERNAL_STATS
    vpx_clear_system_state();

    if (cpi->oxcf.pass != 1) {
      char headings[512] = { 0 };
      char results[512] = { 0 };
      FILE *f = fopen("opsnr.stt", "a");
      double time_encoded =
          (cpi->last_end_time_stamp_seen - cpi->first_time_stamp_ever) /
          10000000.000;
      double total_encode_time =
          (cpi->time_receive_data + cpi->time_compress_data) / 1000.000;
      const double dr =
          (double)cpi->bytes * (double)8 / (double)1000 / time_encoded;
      const double peak = (double)((1 << cpi->oxcf.input_bit_depth) - 1);
      const double target_rate = (double)cpi->oxcf.target_bandwidth / 1000;
      const double rate_err = ((100.0 * (dr - target_rate)) / target_rate);

      if (cpi->b_calculate_psnr) {
        const double total_psnr = vpx_sse_to_psnr(
            (double)cpi->total_samples, peak, (double)cpi->total_sq_error);
        const double totalp_psnr = vpx_sse_to_psnr(
            (double)cpi->totalp_samples, peak, (double)cpi->totalp_sq_error);
        const double total_ssim =
            100 * pow(cpi->summed_quality / cpi->summed_weights, 8.0);
        const double totalp_ssim =
            100 * pow(cpi->summedp_quality / cpi->summedp_weights, 8.0);

        snprintf(headings, sizeof(headings),
                 "Bitrate\tAVGPsnr\tGLBPsnr\tAVPsnrP\tGLPsnrP\t"
                 "VPXSSIM\tVPSSIMP\tFASTSIM\tPSNRHVS\t"
                 "WstPsnr\tWstSsim\tWstFast\tWstHVS\t"
                 "AVPsnrY\tAPsnrCb\tAPsnrCr");
        snprintf(results, sizeof(results),
                 "%7.2f\t%7.3f\t%7.3f\t%7.3f\t%7.3f\t"
                 "%7.3f\t%7.3f\t%7.3f\t%7.3f\t"
                 "%7.3f\t%7.3f\t%7.3f\t%7.3f\t"
                 "%7.3f\t%7.3f\t%7.3f",
                 dr, cpi->psnr.stat[ALL] / cpi->count, total_psnr,
                 cpi->psnrp.stat[ALL] / cpi->count, totalp_psnr, total_ssim,
                 totalp_ssim, cpi->fastssim.stat[ALL] / cpi->count,
                 cpi->psnrhvs.stat[ALL] / cpi->count, cpi->psnr.worst,
                 cpi->worst_ssim, cpi->fastssim.worst, cpi->psnrhvs.worst,
                 cpi->psnr.stat[Y] / cpi->count, cpi->psnr.stat[U] / cpi->count,
                 cpi->psnr.stat[V] / cpi->count);

        if (cpi->b_calculate_blockiness) {
          SNPRINT(headings, "\t  Block\tWstBlck");
          SNPRINT2(results, "\t%7.3f", cpi->total_blockiness / cpi->count);
          SNPRINT2(results, "\t%7.3f", cpi->worst_blockiness);
        }

        if (cpi->b_calculate_consistency) {
          double consistency =
              vpx_sse_to_psnr((double)cpi->totalp_samples, peak,
                              (double)cpi->total_inconsistency);

          SNPRINT(headings, "\tConsist\tWstCons");
          SNPRINT2(results, "\t%7.3f", consistency);
          SNPRINT2(results, "\t%7.3f", cpi->worst_consistency);
        }

        SNPRINT(headings, "\t    Time\tRcErr\tAbsErr");
        SNPRINT2(results, "\t%8.0f", total_encode_time);
        SNPRINT2(results, "\t%7.2f", rate_err);
        SNPRINT2(results, "\t%7.2f", fabs(rate_err));

        fprintf(f, "%s\tAPsnr611\n", headings);
        fprintf(
            f, "%s\t%7.3f\n", results,
            (6 * cpi->psnr.stat[Y] + cpi->psnr.stat[U] + cpi->psnr.stat[V]) /
                (cpi->count * 8));
      }

      fclose(f);
    }
#endif

#if 0
    {
      printf("\n_pick_loop_filter_level:%d\n", cpi->time_pick_lpf / 1000);
      printf("\n_frames recive_data encod_mb_row compress_frame  Total\n");
      printf("%6d %10ld %10ld %10ld %10ld\n", cpi->common.current_video_frame,
             cpi->time_receive_data / 1000, cpi->time_encode_sb_row / 1000,
             cpi->time_compress_data / 1000,
             (cpi->time_receive_data + cpi->time_compress_data) / 1000);
    }
#endif
  }

#if CONFIG_VP9_TEMPORAL_DENOISING
  vp9_denoiser_free(&(cpi->denoiser));
#endif

  if (cpi->kmeans_data_arr_alloc) {
#if CONFIG_MULTITHREAD
    pthread_mutex_destroy(&cpi->kmeans_mutex);
#endif
    vpx_free(cpi->kmeans_data_arr);
  }

  free_tpl_buffer(cpi);

  for (t = 0; t < cpi->num_workers; ++t) {
    VPxWorker *const worker = &cpi->workers[t];
    EncWorkerData *const thread_data = &cpi->tile_thr_data[t];

    // Deallocate allocated threads.
    vpx_get_worker_interface()->end(worker);

    // Deallocate allocated thread data.
    if (t < cpi->num_workers - 1) {
      vpx_free(thread_data->td->counts);
      vp9_free_pc_tree(thread_data->td);
      vpx_free(thread_data->td);
    }
  }
  vpx_free(cpi->tile_thr_data);
  vpx_free(cpi->workers);
  vp9_row_mt_mem_dealloc(cpi);

  if (cpi->num_workers > 1) {
    vp9_loop_filter_dealloc(&cpi->lf_row_sync);
    vp9_bitstream_encode_tiles_buffer_dealloc(cpi);
  }

#if !CONFIG_REALTIME_ONLY
  vp9_alt_ref_aq_destroy(cpi->alt_ref_aq);
#endif

  dealloc_compressor_data(cpi);

  for (i = 0; i < sizeof(cpi->mbgraph_stats) / sizeof(cpi->mbgraph_stats[0]);
       ++i) {
    vpx_free(cpi->mbgraph_stats[i].mb_stats);
  }

#if CONFIG_FP_MB_STATS
  if (cpi->use_fp_mb_stats) {
    vpx_free(cpi->twopass.frame_mb_stats_buf);
    cpi->twopass.frame_mb_stats_buf = NULL;
  }
#endif

  vp9_extrc_delete(&cpi->ext_ratectrl);

  vp9_remove_common(cm);
  vp9_free_ref_frame_buffers(cm->buffer_pool);
#if CONFIG_VP9_POSTPROC
  vp9_free_postproc_buffers(cm);
#endif
  vpx_free(cpi);

#if CONFIG_VP9_TEMPORAL_DENOISING
#ifdef OUTPUT_YUV_DENOISED
  fclose(yuv_denoised_file);
#endif
#endif
#ifdef OUTPUT_YUV_SKINMAP
  fclose(yuv_skinmap_file);
#endif
#ifdef OUTPUT_YUV_REC
  fclose(yuv_rec_file);
#endif
#ifdef OUTPUT_YUV_SVC_SRC
  fclose(yuv_svc_src[0]);
  fclose(yuv_svc_src[1]);
  fclose(yuv_svc_src[2]);
#endif

#if 0

  if (keyfile)
    fclose(keyfile);

  if (framepsnr)
    fclose(framepsnr);

  if (kf_list)
    fclose(kf_list);

#endif
}

int vp9_get_psnr(const VP9_COMP *cpi, PSNR_STATS *psnr) {
  if (is_psnr_calc_enabled(cpi)) {
#if CONFIG_VP9_HIGHBITDEPTH
    vpx_calc_highbd_psnr(cpi->raw_source_frame, cpi->common.frame_to_show, psnr,
                         cpi->td.mb.e_mbd.bd, cpi->oxcf.input_bit_depth);
#else
    vpx_calc_psnr(cpi->raw_source_frame, cpi->common.frame_to_show, psnr);
#endif
    return 1;
  } else {
    vp9_zero(*psnr);
    return 0;
  }
}

int vp9_use_as_reference(VP9_COMP *cpi, int ref_frame_flags) {
  if (ref_frame_flags > 7) return -1;

  cpi->ref_frame_flags = ref_frame_flags;
  return 0;
}

void vp9_update_reference(VP9_COMP *cpi, int ref_frame_flags) {
  cpi->ext_refresh_golden_frame = (ref_frame_flags & VP9_GOLD_FLAG) != 0;
  cpi->ext_refresh_alt_ref_frame = (ref_frame_flags & VP9_ALT_FLAG) != 0;
  cpi->ext_refresh_last_frame = (ref_frame_flags & VP9_LAST_FLAG) != 0;
  cpi->ext_refresh_frame_flags_pending = 1;
}

static YV12_BUFFER_CONFIG *get_vp9_ref_frame_buffer(
    VP9_COMP *cpi, VP9_REFFRAME ref_frame_flag) {
  MV_REFERENCE_FRAME ref_frame = NONE;
  if (ref_frame_flag == VP9_LAST_FLAG)
    ref_frame = LAST_FRAME;
  else if (ref_frame_flag == VP9_GOLD_FLAG)
    ref_frame = GOLDEN_FRAME;
  else if (ref_frame_flag == VP9_ALT_FLAG)
    ref_frame = ALTREF_FRAME;

  return ref_frame == NONE ? NULL : get_ref_frame_buffer(cpi, ref_frame);
}

int vp9_copy_reference_enc(VP9_COMP *cpi, VP9_REFFRAME ref_frame_flag,
                           YV12_BUFFER_CONFIG *sd) {
  YV12_BUFFER_CONFIG *cfg = get_vp9_ref_frame_buffer(cpi, ref_frame_flag);
  if (cfg) {
    vpx_yv12_copy_frame(cfg, sd);
    return 0;
  } else {
    return -1;
  }
}

int vp9_set_reference_enc(VP9_COMP *cpi, VP9_REFFRAME ref_frame_flag,
                          YV12_BUFFER_CONFIG *sd) {
  YV12_BUFFER_CONFIG *cfg = get_vp9_ref_frame_buffer(cpi, ref_frame_flag);
  if (cfg) {
    vpx_yv12_copy_frame(sd, cfg);
    return 0;
  } else {
    return -1;
  }
}

int vp9_update_entropy(VP9_COMP *cpi, int update) {
  cpi->ext_refresh_frame_context = update;
  cpi->ext_refresh_frame_context_pending = 1;
  return 0;
}

#ifdef OUTPUT_YUV_REC
void vp9_write_yuv_rec_frame(VP9_COMMON *cm) {
  YV12_BUFFER_CONFIG *s = cm->frame_to_show;
  uint8_t *src = s->y_buffer;
  int h = cm->height;

#if CONFIG_VP9_HIGHBITDEPTH
  if (s->flags & YV12_FLAG_HIGHBITDEPTH) {
    uint16_t *src16 = CONVERT_TO_SHORTPTR(s->y_buffer);

    do {
      fwrite(src16, s->y_width, 2, yuv_rec_file);
      src16 += s->y_stride;
    } while (--h);

    src16 = CONVERT_TO_SHORTPTR(s->u_buffer);
    h = s->uv_height;

    do {
      fwrite(src16, s->uv_width, 2, yuv_rec_file);
      src16 += s->uv_stride;
    } while (--h);

    src16 = CONVERT_TO_SHORTPTR(s->v_buffer);
    h = s->uv_height;

    do {
      fwrite(src16, s->uv_width, 2, yuv_rec_file);
      src16 += s->uv_stride;
    } while (--h);

    fflush(yuv_rec_file);
    return;
  }
#endif  // CONFIG_VP9_HIGHBITDEPTH

  do {
    fwrite(src, s->y_width, 1, yuv_rec_file);
    src += s->y_stride;
  } while (--h);

  src = s->u_buffer;
  h = s->uv_height;

  do {
    fwrite(src, s->uv_width, 1, yuv_rec_file);
    src += s->uv_stride;
  } while (--h);

  src = s->v_buffer;
  h = s->uv_height;

  do {
    fwrite(src, s->uv_width, 1, yuv_rec_file);
    src += s->uv_stride;
  } while (--h);

  fflush(yuv_rec_file);
}
#endif

#if CONFIG_VP9_HIGHBITDEPTH
static void scale_and_extend_frame_nonnormative(const YV12_BUFFER_CONFIG *src,
                                                YV12_BUFFER_CONFIG *dst,
                                                int bd) {
#else
static void scale_and_extend_frame_nonnormative(const YV12_BUFFER_CONFIG *src,
                                                YV12_BUFFER_CONFIG *dst) {
#endif  // CONFIG_VP9_HIGHBITDEPTH
  // TODO(dkovalev): replace YV12_BUFFER_CONFIG with vpx_image_t
  int i;
  const uint8_t *const srcs[3] = { src->y_buffer, src->u_buffer,
                                   src->v_buffer };
  const int src_strides[3] = { src->y_stride, src->uv_stride, src->uv_stride };
  const int src_widths[3] = { src->y_crop_width, src->uv_crop_width,
                              src->uv_crop_width };
  const int src_heights[3] = { src->y_crop_height, src->uv_crop_height,
                               src->uv_crop_height };
  uint8_t *const dsts[3] = { dst->y_buffer, dst->u_buffer, dst->v_buffer };
  const int dst_strides[3] = { dst->y_stride, dst->uv_stride, dst->uv_stride };
  const int dst_widths[3] = { dst->y_crop_width, dst->uv_crop_width,
                              dst->uv_crop_width };
  const int dst_heights[3] = { dst->y_crop_height, dst->uv_crop_height,
                               dst->uv_crop_height };

  for (i = 0; i < MAX_MB_PLANE; ++i) {
#if CONFIG_VP9_HIGHBITDEPTH
    if (src->flags & YV12_FLAG_HIGHBITDEPTH) {
      vp9_highbd_resize_plane(srcs[i], src_heights[i], src_widths[i],
                              src_strides[i], dsts[i], dst_heights[i],
                              dst_widths[i], dst_strides[i], bd);
    } else {
      vp9_resize_plane(srcs[i], src_heights[i], src_widths[i], src_strides[i],
                       dsts[i], dst_heights[i], dst_widths[i], dst_strides[i]);
    }
#else
    vp9_resize_plane(srcs[i], src_heights[i], src_widths[i], src_strides[i],
                     dsts[i], dst_heights[i], dst_widths[i], dst_strides[i]);
#endif  // CONFIG_VP9_HIGHBITDEPTH
  }
  vpx_extend_frame_borders(dst);
}

#if CONFIG_VP9_HIGHBITDEPTH
static void scale_and_extend_frame(const YV12_BUFFER_CONFIG *src,
                                   YV12_BUFFER_CONFIG *dst, int bd,
                                   INTERP_FILTER filter_type,
                                   int phase_scaler) {
  const int src_w = src->y_crop_width;
  const int src_h = src->y_crop_height;
  const int dst_w = dst->y_crop_width;
  const int dst_h = dst->y_crop_height;
  const uint8_t *const srcs[3] = { src->y_buffer, src->u_buffer,
                                   src->v_buffer };
  const int src_strides[3] = { src->y_stride, src->uv_stride, src->uv_stride };
  uint8_t *const dsts[3] = { dst->y_buffer, dst->u_buffer, dst->v_buffer };
  const int dst_strides[3] = { dst->y_stride, dst->uv_stride, dst->uv_stride };
  const InterpKernel *const kernel = vp9_filter_kernels[filter_type];
  int x, y, i;

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    const int factor = (i == 0 || i == 3 ? 1 : 2);
    const int src_stride = src_strides[i];
    const int dst_stride = dst_strides[i];
    for (y = 0; y < dst_h; y += 16) {
      const int y_q4 = y * (16 / factor) * src_h / dst_h + phase_scaler;
      for (x = 0; x < dst_w; x += 16) {
        const int x_q4 = x * (16 / factor) * src_w / dst_w + phase_scaler;
        const uint8_t *src_ptr = srcs[i] +
                                 (y / factor) * src_h / dst_h * src_stride +
                                 (x / factor) * src_w / dst_w;
        uint8_t *dst_ptr = dsts[i] + (y / factor) * dst_stride + (x / factor);

        if (src->flags & YV12_FLAG_HIGHBITDEPTH) {
          vpx_highbd_convolve8(CONVERT_TO_SHORTPTR(src_ptr), src_stride,
                               CONVERT_TO_SHORTPTR(dst_ptr), dst_stride, kernel,
                               x_q4 & 0xf, 16 * src_w / dst_w, y_q4 & 0xf,
                               16 * src_h / dst_h, 16 / factor, 16 / factor,
                               bd);
        } else {
          vpx_scaled_2d(src_ptr, src_stride, dst_ptr, dst_stride, kernel,
                        x_q4 & 0xf, 16 * src_w / dst_w, y_q4 & 0xf,
                        16 * src_h / dst_h, 16 / factor, 16 / factor);
        }
      }
    }
  }

  vpx_extend_frame_borders(dst);
}
#endif  // CONFIG_VP9_HIGHBITDEPTH

#if !CONFIG_REALTIME_ONLY
static int scale_down(VP9_COMP *cpi, int q) {
  RATE_CONTROL *const rc = &cpi->rc;
  GF_GROUP *const gf_group = &cpi->twopass.gf_group;
  int scale = 0;
  assert(frame_is_kf_gf_arf(cpi));

  if (rc->frame_size_selector == UNSCALED &&
      q >= rc->rf_level_maxq[gf_group->rf_level[gf_group->index]]) {
    const int max_size_thresh =
        (int)(rate_thresh_mult[SCALE_STEP1] *
              VPXMAX(rc->this_frame_target, rc->avg_frame_bandwidth));
    scale = rc->projected_frame_size > max_size_thresh ? 1 : 0;
  }
  return scale;
}

static int big_rate_miss_high_threshold(VP9_COMP *cpi) {
  const RATE_CONTROL *const rc = &cpi->rc;
  int big_miss_high;

  if (frame_is_kf_gf_arf(cpi))
    big_miss_high = rc->this_frame_target * 3 / 2;
  else
    big_miss_high = rc->this_frame_target * 2;

  return big_miss_high;
}

static int big_rate_miss(VP9_COMP *cpi) {
  const RATE_CONTROL *const rc = &cpi->rc;
  int big_miss_high;
  int big_miss_low;

  // Ignore for overlay frames
  if (rc->is_src_frame_alt_ref) {
    return 0;
  } else {
    big_miss_low = (rc->this_frame_target / 2);
    big_miss_high = big_rate_miss_high_threshold(cpi);

    return (rc->projected_frame_size > big_miss_high) ||
           (rc->projected_frame_size < big_miss_low);
  }
}

// test in two pass for the first
static int two_pass_first_group_inter(VP9_COMP *cpi) {
  if (cpi->oxcf.pass == 2) {
    TWO_PASS *const twopass = &cpi->twopass;
    GF_GROUP *const gf_group = &twopass->gf_group;
    const int gfg_index = gf_group->index;

    if (gfg_index == 0) return gf_group->update_type[gfg_index] == LF_UPDATE;
    return gf_group->update_type[gfg_index - 1] != LF_UPDATE &&
           gf_group->update_type[gfg_index] == LF_UPDATE;
  } else {
    return 0;
  }
}

// Function to test for conditions that indicate we should loop
// back and recode a frame.
static int recode_loop_test(VP9_COMP *cpi, int high_limit, int low_limit, int q,
                            int maxq, int minq) {
  const RATE_CONTROL *const rc = &cpi->rc;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  const int frame_is_kfgfarf = frame_is_kf_gf_arf(cpi);
  int force_recode = 0;

  if ((rc->projected_frame_size >= rc->max_frame_bandwidth) ||
      big_rate_miss(cpi) || (cpi->sf.recode_loop == ALLOW_RECODE) ||
      (two_pass_first_group_inter(cpi) &&
       (cpi->sf.recode_loop == ALLOW_RECODE_FIRST)) ||
      (frame_is_kfgfarf && (cpi->sf.recode_loop >= ALLOW_RECODE_KFARFGF))) {
    if (frame_is_kfgfarf && (oxcf->resize_mode == RESIZE_DYNAMIC) &&
        scale_down(cpi, q)) {
      // Code this group at a lower resolution.
      cpi->resize_pending = 1;
      return 1;
    }

    // Force recode for extreme overshoot.
    if ((rc->projected_frame_size >= rc->max_frame_bandwidth) ||
        (cpi->sf.recode_loop >= ALLOW_RECODE_KFARFGF &&
         rc->projected_frame_size >= big_rate_miss_high_threshold(cpi))) {
      return 1;
    }

    // TODO(agrange) high_limit could be greater than the scale-down threshold.
    if ((rc->projected_frame_size > high_limit && q < maxq) ||
        (rc->projected_frame_size < low_limit && q > minq)) {
      force_recode = 1;
    } else if (cpi->oxcf.rc_mode == VPX_CQ) {
      // Deal with frame undershoot and whether or not we are
      // below the automatically set cq level.
      if (q > oxcf->cq_level &&
          rc->projected_frame_size < ((rc->this_frame_target * 7) >> 3)) {
        force_recode = 1;
      }
    }
  }
  return force_recode;
}
#endif  // !CONFIG_REALTIME_ONLY

static void update_ref_frames(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  BufferPool *const pool = cm->buffer_pool;
  GF_GROUP *const gf_group = &cpi->twopass.gf_group;

  if (cpi->rc.show_arf_as_gld) {
    int tmp = cpi->alt_fb_idx;
    cpi->alt_fb_idx = cpi->gld_fb_idx;
    cpi->gld_fb_idx = tmp;
  } else if (cm->show_existing_frame) {
    // Pop ARF.
    cpi->lst_fb_idx = cpi->alt_fb_idx;
    cpi->alt_fb_idx =
        stack_pop(gf_group->arf_index_stack, gf_group->stack_size);
    --gf_group->stack_size;
  }

  // At this point the new frame has been encoded.
  // If any buffer copy / swapping is signaled it should be done here.
  if (cm->frame_type == KEY_FRAME) {
    ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[cpi->gld_fb_idx],
               cm->new_fb_idx);
    ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[cpi->alt_fb_idx],
               cm->new_fb_idx);
  } else if (vp9_preserve_existing_gf(cpi)) {
    // We have decided to preserve the previously existing golden frame as our
    // new ARF frame. However, in the short term in function
    // vp9_get_refresh_mask() we left it in the GF slot and, if
    // we're updating the GF with the current decoded frame, we save it to the
    // ARF slot instead.
    // We now have to update the ARF with the current frame and swap gld_fb_idx
    // and alt_fb_idx so that, overall, we've stored the old GF in the new ARF
    // slot and, if we're updating the GF, the current frame becomes the new GF.
    int tmp;

    ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[cpi->alt_fb_idx],
               cm->new_fb_idx);

    tmp = cpi->alt_fb_idx;
    cpi->alt_fb_idx = cpi->gld_fb_idx;
    cpi->gld_fb_idx = tmp;
  } else { /* For non key/golden frames */
    if (cpi->refresh_alt_ref_frame) {
      int arf_idx = gf_group->top_arf_idx;

      // Push new ARF into stack.
      stack_push(gf_group->arf_index_stack, cpi->alt_fb_idx,
                 gf_group->stack_size);
      ++gf_group->stack_size;

      assert(arf_idx < REF_FRAMES);

      ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[arf_idx], cm->new_fb_idx);
      memcpy(cpi->interp_filter_selected[ALTREF_FRAME],
             cpi->interp_filter_selected[0],
             sizeof(cpi->interp_filter_selected[0]));

      cpi->alt_fb_idx = arf_idx;
    }

    if (cpi->refresh_golden_frame) {
      ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[cpi->gld_fb_idx],
                 cm->new_fb_idx);
      if (!cpi->rc.is_src_frame_alt_ref)
        memcpy(cpi->interp_filter_selected[GOLDEN_FRAME],
               cpi->interp_filter_selected[0],
               sizeof(cpi->interp_filter_selected[0]));
      else
        memcpy(cpi->interp_filter_selected[GOLDEN_FRAME],
               cpi->interp_filter_selected[ALTREF_FRAME],
               sizeof(cpi->interp_filter_selected[ALTREF_FRAME]));
    }
  }

  if (cpi->refresh_last_frame) {
    ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[cpi->lst_fb_idx],
               cm->new_fb_idx);
    if (!cpi->rc.is_src_frame_alt_ref)
      memcpy(cpi->interp_filter_selected[LAST_FRAME],
             cpi->interp_filter_selected[0],
             sizeof(cpi->interp_filter_selected[0]));
  }

  if (gf_group->update_type[gf_group->index] == MID_OVERLAY_UPDATE) {
    cpi->alt_fb_idx =
        stack_pop(gf_group->arf_index_stack, gf_group->stack_size);
    --gf_group->stack_size;
  }
}

void vp9_update_reference_frames(VP9_COMP *cpi) {
  update_ref_frames(cpi);

#if CONFIG_VP9_TEMPORAL_DENOISING
  vp9_denoiser_update_ref_frame(cpi);
#endif

  if (is_one_pass_cbr_svc(cpi)) vp9_svc_update_ref_frame(cpi);
}

static void loopfilter_frame(VP9_COMP *cpi, VP9_COMMON *cm) {
  MACROBLOCKD *xd = &cpi->td.mb.e_mbd;
  struct loopfilter *lf = &cm->lf;
  int is_reference_frame =
      (cm->frame_type == KEY_FRAME || cpi->refresh_last_frame ||
       cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame);
  if (cpi->use_svc &&
      cpi->svc.temporal_layering_mode == VP9E_TEMPORAL_LAYERING_MODE_BYPASS)
    is_reference_frame = !cpi->svc.non_reference_frame;

  // Skip loop filter in show_existing_frame mode.
  if (cm->show_existing_frame) {
    lf->filter_level = 0;
    return;
  }

  if (cpi->loopfilter_ctrl == NO_LOOPFILTER ||
      (!is_reference_frame && cpi->loopfilter_ctrl == LOOPFILTER_REFERENCE)) {
    lf->filter_level = 0;
    vpx_extend_frame_inner_borders(cm->frame_to_show);
    return;
  }

  if (xd->lossless) {
    lf->filter_level = 0;
    lf->last_filt_level = 0;
  } else {
    struct vpx_usec_timer timer;

    vpx_clear_system_state();

    vpx_usec_timer_start(&timer);

    if (!cpi->rc.is_src_frame_alt_ref) {
      if ((cpi->common.frame_type == KEY_FRAME) &&
          (!cpi->rc.this_key_frame_forced)) {
        lf->last_filt_level = 0;
      }
      vp9_pick_filter_level(cpi->Source, cpi, cpi->sf.lpf_pick);
      lf->last_filt_level = lf->filter_level;
    } else {
      lf->filter_level = 0;
    }

    vpx_usec_timer_mark(&timer);
    cpi->time_pick_lpf += vpx_usec_timer_elapsed(&timer);
  }

  if (lf->filter_level > 0 && is_reference_frame) {
    vp9_build_mask_frame(cm, lf->filter_level, 0);

    if (cpi->num_workers > 1)
      vp9_loop_filter_frame_mt(cm->frame_to_show, cm, xd->plane,
                               lf->filter_level, 0, 0, cpi->workers,
                               cpi->num_workers, &cpi->lf_row_sync);
    else
      vp9_loop_filter_frame(cm->frame_to_show, cm, xd, lf->filter_level, 0, 0);
  }

  vpx_extend_frame_inner_borders(cm->frame_to_show);
}

static INLINE void alloc_frame_mvs(VP9_COMMON *const cm, int buffer_idx) {
  RefCntBuffer *const new_fb_ptr = &cm->buffer_pool->frame_bufs[buffer_idx];
  if (new_fb_ptr->mvs == NULL || new_fb_ptr->mi_rows < cm->mi_rows ||
      new_fb_ptr->mi_cols < cm->mi_cols) {
    vpx_free(new_fb_ptr->mvs);
    CHECK_MEM_ERROR(cm, new_fb_ptr->mvs,
                    (MV_REF *)vpx_calloc(cm->mi_rows * cm->mi_cols,
                                         sizeof(*new_fb_ptr->mvs)));
    new_fb_ptr->mi_rows = cm->mi_rows;
    new_fb_ptr->mi_cols = cm->mi_cols;
  }
}

void vp9_scale_references(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  MV_REFERENCE_FRAME ref_frame;
  const VP9_REFFRAME ref_mask[3] = { VP9_LAST_FLAG, VP9_GOLD_FLAG,
                                     VP9_ALT_FLAG };

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    // Need to convert from VP9_REFFRAME to index into ref_mask (subtract 1).
    if (cpi->ref_frame_flags & ref_mask[ref_frame - 1]) {
      BufferPool *const pool = cm->buffer_pool;
      const YV12_BUFFER_CONFIG *const ref =
          get_ref_frame_buffer(cpi, ref_frame);

      if (ref == NULL) {
        cpi->scaled_ref_idx[ref_frame - 1] = INVALID_IDX;
        continue;
      }

#if CONFIG_VP9_HIGHBITDEPTH
      if (ref->y_crop_width != cm->width || ref->y_crop_height != cm->height) {
        RefCntBuffer *new_fb_ptr = NULL;
        int force_scaling = 0;
        int new_fb = cpi->scaled_ref_idx[ref_frame - 1];
        if (new_fb == INVALID_IDX) {
          new_fb = get_free_fb(cm);
          force_scaling = 1;
        }
        if (new_fb == INVALID_IDX) return;
        new_fb_ptr = &pool->frame_bufs[new_fb];
        if (force_scaling || new_fb_ptr->buf.y_crop_width != cm->width ||
            new_fb_ptr->buf.y_crop_height != cm->height) {
          if (vpx_realloc_frame_buffer(&new_fb_ptr->buf, cm->width, cm->height,
                                       cm->subsampling_x, cm->subsampling_y,
                                       cm->use_highbitdepth,
                                       VP9_ENC_BORDER_IN_PIXELS,
                                       cm->byte_alignment, NULL, NULL, NULL))
            vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                               "Failed to allocate frame buffer");
          scale_and_extend_frame(ref, &new_fb_ptr->buf, (int)cm->bit_depth,
                                 EIGHTTAP, 0);
          cpi->scaled_ref_idx[ref_frame - 1] = new_fb;
          alloc_frame_mvs(cm, new_fb);
        }
#else
      if (ref->y_crop_width != cm->width || ref->y_crop_height != cm->height) {
        RefCntBuffer *new_fb_ptr = NULL;
        int force_scaling = 0;
        int new_fb = cpi->scaled_ref_idx[ref_frame - 1];
        if (new_fb == INVALID_IDX) {
          new_fb = get_free_fb(cm);
          force_scaling = 1;
        }
        if (new_fb == INVALID_IDX) return;
        new_fb_ptr = &pool->frame_bufs[new_fb];
        if (force_scaling || new_fb_ptr->buf.y_crop_width != cm->width ||
            new_fb_ptr->buf.y_crop_height != cm->height) {
          if (vpx_realloc_frame_buffer(&new_fb_ptr->buf, cm->width, cm->height,
                                       cm->subsampling_x, cm->subsampling_y,
                                       VP9_ENC_BORDER_IN_PIXELS,
                                       cm->byte_alignment, NULL, NULL, NULL))
            vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                               "Failed to allocate frame buffer");
          vp9_scale_and_extend_frame(ref, &new_fb_ptr->buf, EIGHTTAP, 0);
          cpi->scaled_ref_idx[ref_frame - 1] = new_fb;
          alloc_frame_mvs(cm, new_fb);
        }
#endif  // CONFIG_VP9_HIGHBITDEPTH
      } else {
        int buf_idx;
        RefCntBuffer *buf = NULL;
        if (cpi->oxcf.pass == 0 && !cpi->use_svc) {
          // Check for release of scaled reference.
          buf_idx = cpi->scaled_ref_idx[ref_frame - 1];
          if (buf_idx != INVALID_IDX) {
            buf = &pool->frame_bufs[buf_idx];
            --buf->ref_count;
            cpi->scaled_ref_idx[ref_frame - 1] = INVALID_IDX;
          }
        }
        buf_idx = get_ref_frame_buf_idx(cpi, ref_frame);
        buf = &pool->frame_bufs[buf_idx];
        buf->buf.y_crop_width = ref->y_crop_width;
        buf->buf.y_crop_height = ref->y_crop_height;
        cpi->scaled_ref_idx[ref_frame - 1] = buf_idx;
        ++buf->ref_count;
      }
    } else {
      if (cpi->oxcf.pass != 0 || cpi->use_svc)
        cpi->scaled_ref_idx[ref_frame - 1] = INVALID_IDX;
    }
  }
}

static void release_scaled_references(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  int i;
  if (cpi->oxcf.pass == 0 && !cpi->use_svc) {
    // Only release scaled references under certain conditions:
    // if reference will be updated, or if scaled reference has same resolution.
    int refresh[3];
    refresh[0] = (cpi->refresh_last_frame) ? 1 : 0;
    refresh[1] = (cpi->refresh_golden_frame) ? 1 : 0;
    refresh[2] = (cpi->refresh_alt_ref_frame) ? 1 : 0;
    for (i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
      const int idx = cpi->scaled_ref_idx[i - 1];
      if (idx != INVALID_IDX) {
        RefCntBuffer *const buf = &cm->buffer_pool->frame_bufs[idx];
        const YV12_BUFFER_CONFIG *const ref = get_ref_frame_buffer(cpi, i);
        if (refresh[i - 1] || (buf->buf.y_crop_width == ref->y_crop_width &&
                               buf->buf.y_crop_height == ref->y_crop_height)) {
          --buf->ref_count;
          cpi->scaled_ref_idx[i - 1] = INVALID_IDX;
        }
      }
    }
  } else {
    for (i = 0; i < REFS_PER_FRAME; ++i) {
      const int idx = cpi->scaled_ref_idx[i];
      if (idx != INVALID_IDX) {
        RefCntBuffer *const buf = &cm->buffer_pool->frame_bufs[idx];
        --buf->ref_count;
        cpi->scaled_ref_idx[i] = INVALID_IDX;
      }
    }
  }
}

static void full_to_model_count(unsigned int *model_count,
                                unsigned int *full_count) {
  int n;
  model_count[ZERO_TOKEN] = full_count[ZERO_TOKEN];
  model_count[ONE_TOKEN] = full_count[ONE_TOKEN];
  model_count[TWO_TOKEN] = full_count[TWO_TOKEN];
  for (n = THREE_TOKEN; n < EOB_TOKEN; ++n)
    model_count[TWO_TOKEN] += full_count[n];
  model_count[EOB_MODEL_TOKEN] = full_count[EOB_TOKEN];
}

static void full_to_model_counts(vp9_coeff_count_model *model_count,
                                 vp9_coeff_count *full_count) {
  int i, j, k, l;

  for (i = 0; i < PLANE_TYPES; ++i)
    for (j = 0; j < REF_TYPES; ++j)
      for (k = 0; k < COEF_BANDS; ++k)
        for (l = 0; l < BAND_COEFF_CONTEXTS(k); ++l)
          full_to_model_count(model_count[i][j][k][l], full_count[i][j][k][l]);
}

#if 0 && CONFIG_INTERNAL_STATS
static void output_frame_level_debug_stats(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  FILE *const f = fopen("tmp.stt", cm->current_video_frame ? "a" : "w");
  int64_t recon_err;

  vpx_clear_system_state();

#if CONFIG_VP9_HIGHBITDEPTH
  if (cm->use_highbitdepth) {
    recon_err = vpx_highbd_get_y_sse(cpi->Source, get_frame_new_buffer(cm));
  } else {
    recon_err = vpx_get_y_sse(cpi->Source, get_frame_new_buffer(cm));
  }
#else
  recon_err = vpx_get_y_sse(cpi->Source, get_frame_new_buffer(cm));
#endif  // CONFIG_VP9_HIGHBITDEPTH


  if (cpi->twopass.total_left_stats.coded_error != 0.0) {
    double dc_quant_devisor;
#if CONFIG_VP9_HIGHBITDEPTH
    switch (cm->bit_depth) {
      case VPX_BITS_8:
        dc_quant_devisor = 4.0;
        break;
      case VPX_BITS_10:
        dc_quant_devisor = 16.0;
        break;
      default:
        assert(cm->bit_depth == VPX_BITS_12);
        dc_quant_devisor = 64.0;
        break;
    }
#else
    dc_quant_devisor = 4.0;
#endif

    if (!cm->current_video_frame) {
      fprintf(f, "frame, width, height, last ts, last end ts, "
          "source_alt_ref_pending, source_alt_ref_active, "
          "this_frame_target, projected_frame_size, "
          "projected_frame_size / MBs, "
          "projected_frame_size - this_frame_target, "
          "vbr_bits_off_target, vbr_bits_off_target_fast, "
          "twopass.extend_minq, twopass.extend_minq_fast, "
          "total_target_vs_actual, "
          "starting_buffer_level - bits_off_target, "
          "total_actual_bits, base_qindex, q for base_qindex, "
          "dc quant, q for active_worst_quality, avg_q, q for oxcf.cq_level, "
          "refresh_last_frame, refresh_golden_frame, refresh_alt_ref_frame, "
          "frame_type, gfu_boost, "
          "twopass.bits_left, "
          "twopass.total_left_stats.coded_error, "
          "twopass.bits_left / (1 + twopass.total_left_stats.coded_error), "
          "tot_recode_hits, recon_err, kf_boost, "
          "twopass.kf_zeromotion_pct, twopass.fr_content_type, "
          "filter_level, seg.aq_av_offset\n");
    }

    fprintf(f, "%10u, %d, %d, %10"PRId64", %10"PRId64", %d, %d, %10d, %10d, "
        "%10d, %10d, %10"PRId64", %10"PRId64", %5d, %5d, %10"PRId64", "
        "%10"PRId64", %10"PRId64", %10d, %7.2lf, %7.2lf, %7.2lf, %7.2lf, "
        "%7.2lf, %6d, %6d, %5d, %5d, %5d, %10"PRId64", %10.3lf, %10lf, %8u, "
        "%10"PRId64", %10d, %10d, %10d, %10d, %10d\n",
        cpi->common.current_video_frame,
        cm->width, cm->height,
        cpi->last_time_stamp_seen,
        cpi->last_end_time_stamp_seen,
        cpi->rc.source_alt_ref_pending,
        cpi->rc.source_alt_ref_active,
        cpi->rc.this_frame_target,
        cpi->rc.projected_frame_size,
        cpi->rc.projected_frame_size / cpi->common.MBs,
        (cpi->rc.projected_frame_size - cpi->rc.this_frame_target),
        cpi->rc.vbr_bits_off_target,
        cpi->rc.vbr_bits_off_target_fast,
        cpi->twopass.extend_minq,
        cpi->twopass.extend_minq_fast,
        cpi->rc.total_target_vs_actual,
        (cpi->rc.starting_buffer_level - cpi->rc.bits_off_target),
        cpi->rc.total_actual_bits, cm->base_qindex,
        vp9_convert_qindex_to_q(cm->base_qindex, cm->bit_depth),
        (double)vp9_dc_quant(cm->base_qindex, 0, cm->bit_depth) /
            dc_quant_devisor,
        vp9_convert_qindex_to_q(cpi->twopass.active_worst_quality,
                                cm->bit_depth),
        cpi->rc.avg_q,
        vp9_convert_qindex_to_q(cpi->oxcf.cq_level, cm->bit_depth),
        cpi->refresh_last_frame, cpi->refresh_golden_frame,
        cpi->refresh_alt_ref_frame, cm->frame_type, cpi->rc.gfu_boost,
        cpi->twopass.bits_left,
        cpi->twopass.total_left_stats.coded_error,
        cpi->twopass.bits_left /
            (1 + cpi->twopass.total_left_stats.coded_error),
        cpi->tot_recode_hits, recon_err, cpi->rc.kf_boost,
        cpi->twopass.kf_zeromotion_pct,
        cpi->twopass.fr_content_type,
        cm->lf.filter_level,
        cm->seg.aq_av_offset);
  }
  fclose(f);

  if (0) {
    FILE *const fmodes = fopen("Modes.stt", "a");
    int i;

    fprintf(fmodes, "%6d:%1d:%1d:%1d ", cpi->common.current_video_frame,
            cm->frame_type, cpi->refresh_golden_frame,
            cpi->refresh_alt_ref_frame);

    for (i = 0; i < MAX_MODES; ++i)
      fprintf(fmodes, "%5d ", cpi->mode_chosen_counts[i]);

    fprintf(fmodes, "\n");

    fclose(fmodes);
  }
}
#endif

static void set_mv_search_params(VP9_COMP *cpi) {
  const VP9_COMMON *const cm = &cpi->common;
  const unsigned int max_mv_def = VPXMIN(cm->width, cm->height);

  // Default based on max resolution.
  cpi->mv_step_param = vp9_init_search_range(max_mv_def);

  if (cpi->sf.mv.auto_mv_step_size) {
    if (frame_is_intra_only(cm)) {
      // Initialize max_mv_magnitude for use in the first INTER frame
      // after a key/intra-only frame.
      cpi->max_mv_magnitude = max_mv_def;
    } else {
      if (cm->show_frame) {
        // Allow mv_steps to correspond to twice the max mv magnitude found
        // in the previous frame, capped by the default max_mv_magnitude based
        // on resolution.
        cpi->mv_step_param = vp9_init_search_range(
            VPXMIN(max_mv_def, 2 * cpi->max_mv_magnitude));
      }
      cpi->max_mv_magnitude = 0;
    }
  }
}

static void set_size_independent_vars(VP9_COMP *cpi) {
  vp9_set_speed_features_framesize_independent(cpi, cpi->oxcf.speed);
  vp9_set_rd_speed_thresholds(cpi);
  vp9_set_rd_speed_thresholds_sub8x8(cpi);
  cpi->common.interp_filter = cpi->sf.default_interp_filter;
}

static void set_size_dependent_vars(VP9_COMP *cpi, int *q, int *bottom_index,
                                    int *top_index) {
  VP9_COMMON *const cm = &cpi->common;

  // Setup variables that depend on the dimensions of the frame.
  vp9_set_speed_features_framesize_dependent(cpi, cpi->oxcf.speed);

  // Decide q and q bounds.
  *q = vp9_rc_pick_q_and_bounds(cpi, bottom_index, top_index);

  if (cpi->oxcf.rc_mode == VPX_CBR && cpi->rc.force_max_q) {
    *q = cpi->rc.worst_quality;
    cpi->rc.force_max_q = 0;
  }

  if (!frame_is_intra_only(cm)) {
    vp9_set_high_precision_mv(cpi, (*q) < HIGH_PRECISION_MV_QTHRESH);
  }

#if !CONFIG_REALTIME_ONLY
  // Configure experimental use of segmentation for enhanced coding of
  // static regions if indicated.
  // Only allowed in the second pass of a two pass encode, as it requires
  // lagged coding, and if the relevant speed feature flag is set.
  if (cpi->oxcf.pass == 2 && cpi->sf.static_segmentation)
    configure_static_seg_features(cpi);
#endif  // !CONFIG_REALTIME_ONLY

#if CONFIG_VP9_POSTPROC && !(CONFIG_VP9_TEMPORAL_DENOISING)
  if (cpi->oxcf.noise_sensitivity > 0) {
    int l = 0;
    switch (cpi->oxcf.noise_sensitivity) {
      case 1: l = 20; break;
      case 2: l = 40; break;
      case 3: l = 60; break;
      case 4:
      case 5: l = 100; break;
      case 6: l = 150; break;
    }
    if (!cpi->common.postproc_state.limits) {
      cpi->common.postproc_state.limits =
          vpx_calloc(cpi->un_scaled_source->y_width,
                     sizeof(*cpi->common.postproc_state.limits));
    }
    vp9_denoise(&cpi->common, cpi->Source, cpi->Source, l,
                cpi->common.postproc_state.limits);
  }
#endif  // CONFIG_VP9_POSTPROC
}

static void init_motion_estimation(VP9_COMP *cpi) {
  int y_stride = cpi->scaled_source.y_stride;

  if (cpi->sf.mv.search_method == NSTEP) {
    vp9_init3smotion_compensation(&cpi->ss_cfg, y_stride);
  } else if (cpi->sf.mv.search_method == DIAMOND) {
    vp9_init_dsmotion_compensation(&cpi->ss_cfg, y_stride);
  }
}

static void set_frame_size(VP9_COMP *cpi) {
  int ref_frame;
  VP9_COMMON *const cm = &cpi->common;
  VP9EncoderConfig *const oxcf = &cpi->oxcf;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;

#if !CONFIG_REALTIME_ONLY
  if (oxcf->pass == 2 && oxcf->rc_mode == VPX_VBR &&
      ((oxcf->resize_mode == RESIZE_FIXED && cm->current_video_frame == 0) ||
       (oxcf->resize_mode == RESIZE_DYNAMIC && cpi->resize_pending))) {
    calculate_coded_size(cpi, &oxcf->scaled_frame_width,
                         &oxcf->scaled_frame_height);

    // There has been a change in frame size.
    vp9_set_size_literal(cpi, oxcf->scaled_frame_width,
                         oxcf->scaled_frame_height);
  }
#endif  // !CONFIG_REALTIME_ONLY

  if (oxcf->pass == 0 && oxcf->rc_mode == VPX_CBR &&
      oxcf->resize_mode == RESIZE_DYNAMIC && cpi->resize_pending != 0) {
    // For SVC scaled width/height will have been set (svc->resize_set=1)
    // in get_svc_params based on the layer width/height.
    if (!cpi->use_svc || !cpi->svc.resize_set) {
      oxcf->scaled_frame_width =
          (oxcf->width * cpi->resize_scale_num) / cpi->resize_scale_den;
      oxcf->scaled_frame_height =
          (oxcf->height * cpi->resize_scale_num) / cpi->resize_scale_den;
      // There has been a change in frame size.
      vp9_set_size_literal(cpi, oxcf->scaled_frame_width,
                           oxcf->scaled_frame_height);
    }

    // TODO(agrange) Scale cpi->max_mv_magnitude if frame-size has changed.
    set_mv_search_params(cpi);

    vp9_noise_estimate_init(&cpi->noise_estimate, cm->width, cm->height);
#if CONFIG_VP9_TEMPORAL_DENOISING
    // Reset the denoiser on the resized frame.
    if (cpi->oxcf.noise_sensitivity > 0) {
      vp9_denoiser_free(&(cpi->denoiser));
      setup_denoiser_buffer(cpi);
      // Dynamic resize is only triggered for non-SVC, so we can force
      // golden frame update here as temporary fix to denoiser.
      cpi->refresh_golden_frame = 1;
    }
#endif
  }

  if ((oxcf->pass == 2) && !cpi->use_svc) {
    vp9_set_target_rate(cpi);
  }

  alloc_frame_mvs(cm, cm->new_fb_idx);

  // Reset the frame pointers to the current frame size.
  if (vpx_realloc_frame_buffer(get_frame_new_buffer(cm), cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
#if CONFIG_VP9_HIGHBITDEPTH
                               cm->use_highbitdepth,
#endif
                               VP9_ENC_BORDER_IN_PIXELS, cm->byte_alignment,
                               NULL, NULL, NULL))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate frame buffer");

  alloc_util_frame_buffers(cpi);
  init_motion_estimation(cpi);

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    RefBuffer *const ref_buf = &cm->frame_refs[ref_frame - 1];
    const int buf_idx = get_ref_frame_buf_idx(cpi, ref_frame);

    ref_buf->idx = buf_idx;

    if (buf_idx != INVALID_IDX) {
      YV12_BUFFER_CONFIG *const buf = &cm->buffer_pool->frame_bufs[buf_idx].buf;
      ref_buf->buf = buf;
#if CONFIG_VP9_HIGHBITDEPTH
      vp9_setup_scale_factors_for_frame(
          &ref_buf->sf, buf->y_crop_width, buf->y_crop_height, cm->width,
          cm->height, (buf->flags & YV12_FLAG_HIGHBITDEPTH) ? 1 : 0);
#else
      vp9_setup_scale_factors_for_frame(&ref_buf->sf, buf->y_crop_width,
                                        buf->y_crop_height, cm->width,
                                        cm->height);
#endif  // CONFIG_VP9_HIGHBITDEPTH
      if (vp9_is_scaled(&ref_buf->sf)) vpx_extend_frame_borders(buf);
    } else {
      ref_buf->buf = NULL;
    }
  }

  set_ref_ptrs(cm, xd, LAST_FRAME, LAST_FRAME);
}

#if CONFIG_CONSISTENT_RECODE || CONFIG_RATE_CTRL
static void save_encode_params(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  const int tile_cols = 1 << cm->log2_tile_cols;
  const int tile_rows = 1 << cm->log2_tile_rows;
  int tile_col, tile_row;
  int i, j;
  RD_OPT *rd_opt = &cpi->rd;
  for (i = 0; i < MAX_REF_FRAMES; i++) {
    for (j = 0; j < REFERENCE_MODES; j++)
      rd_opt->prediction_type_threshes_prev[i][j] =
          rd_opt->prediction_type_threshes[i][j];

    for (j = 0; j < SWITCHABLE_FILTER_CONTEXTS; j++)
      rd_opt->filter_threshes_prev[i][j] = rd_opt->filter_threshes[i][j];
  }

  if (cpi->tile_data != NULL) {
    for (tile_row = 0; tile_row < tile_rows; ++tile_row)
      for (tile_col = 0; tile_col < tile_cols; ++tile_col) {
        TileDataEnc *tile_data =
            &cpi->tile_data[tile_row * tile_cols + tile_col];
        for (i = 0; i < BLOCK_SIZES; ++i) {
          for (j = 0; j < MAX_MODES; ++j) {
            tile_data->thresh_freq_fact_prev[i][j] =
                tile_data->thresh_freq_fact[i][j];
          }
        }
      }
  }
}
#endif  // CONFIG_CONSISTENT_RECODE || CONFIG_RATE_CTRL

static INLINE void set_raw_source_frame(VP9_COMP *cpi) {
#ifdef ENABLE_KF_DENOISE
  if (is_spatial_denoise_enabled(cpi)) {
    cpi->raw_source_frame = vp9_scale_if_required(
        cm, &cpi->raw_unscaled_source, &cpi->raw_scaled_source,
        (oxcf->pass == 0), EIGHTTAP, 0);
  } else {
    cpi->raw_source_frame = cpi->Source;
  }
#else
  cpi->raw_source_frame = cpi->Source;
#endif
}

static int encode_without_recode_loop(VP9_COMP *cpi, size_t *size,
                                      uint8_t *dest) {
  VP9_COMMON *const cm = &cpi->common;
  SVC *const svc = &cpi->svc;
  int q = 0, bottom_index = 0, top_index = 0;
  int no_drop_scene_change = 0;
  const INTERP_FILTER filter_scaler =
      (is_one_pass_cbr_svc(cpi))
          ? svc->downsample_filter_type[svc->spatial_layer_id]
          : EIGHTTAP;
  const int phase_scaler =
      (is_one_pass_cbr_svc(cpi))
          ? svc->downsample_filter_phase[svc->spatial_layer_id]
          : 0;

  if (cm->show_existing_frame) {
    cpi->rc.this_frame_target = 0;
    if (is_psnr_calc_enabled(cpi)) set_raw_source_frame(cpi);
    return 1;
  }

  svc->time_stamp_prev[svc->spatial_layer_id] = svc->time_stamp_superframe;

  // Flag to check if its valid to compute the source sad (used for
  // scene detection and for superblock content state in CBR mode).
  // The flag may get reset below based on SVC or resizing state.
  cpi->compute_source_sad_onepass = cpi->oxcf.mode == REALTIME;

  vpx_clear_system_state();

  set_frame_size(cpi);

  if (is_one_pass_cbr_svc(cpi) &&
      cpi->un_scaled_source->y_width == cm->width << 2 &&
      cpi->un_scaled_source->y_height == cm->height << 2 &&
      svc->scaled_temp.y_width == cm->width << 1 &&
      svc->scaled_temp.y_height == cm->height << 1) {
    // For svc, if it is a 1/4x1/4 downscaling, do a two-stage scaling to take
    // advantage of the 1:2 optimized scaler. In the process, the 1/2x1/2
    // result will be saved in scaled_temp and might be used later.
    const INTERP_FILTER filter_scaler2 = svc->downsample_filter_type[1];
    const int phase_scaler2 = svc->downsample_filter_phase[1];
    cpi->Source = vp9_svc_twostage_scale(
        cm, cpi->un_scaled_source, &cpi->scaled_source, &svc->scaled_temp,
        filter_scaler, phase_scaler, filter_scaler2, phase_scaler2);
    svc->scaled_one_half = 1;
  } else if (is_one_pass_cbr_svc(cpi) &&
             cpi->un_scaled_source->y_width == cm->width << 1 &&
             cpi->un_scaled_source->y_height == cm->height << 1 &&
             svc->scaled_one_half) {
    // If the spatial layer is 1/2x1/2 and the scaling is already done in the
    // two-stage scaling, use the result directly.
    cpi->Source = &svc->scaled_temp;
    svc->scaled_one_half = 0;
  } else {
    cpi->Source = vp9_scale_if_required(
        cm, cpi->un_scaled_source, &cpi->scaled_source, (cpi->oxcf.pass == 0),
        filter_scaler, phase_scaler);
  }
#ifdef OUTPUT_YUV_SVC_SRC
  // Write out at most 3 spatial layers.
  if (is_one_pass_cbr_svc(cpi) && svc->spatial_layer_id < 3) {
    vpx_write_yuv_frame(yuv_svc_src[svc->spatial_layer_id], cpi->Source);
  }
#endif
  // Unfiltered raw source used in metrics calculation if the source
  // has been filtered.
  if (is_psnr_calc_enabled(cpi)) {
#ifdef ENABLE_KF_DENOISE
    if (is_spatial_denoise_enabled(cpi)) {
      cpi->raw_source_frame = vp9_scale_if_required(
          cm, &cpi->raw_unscaled_source, &cpi->raw_scaled_source,
          (cpi->oxcf.pass == 0), EIGHTTAP, phase_scaler);
    } else {
      cpi->raw_source_frame = cpi->Source;
    }
#else
    cpi->raw_source_frame = cpi->Source;
#endif
  }

  if ((cpi->use_svc &&
       (svc->spatial_layer_id < svc->number_spatial_layers - 1 ||
        svc->temporal_layer_id < svc->number_temporal_layers - 1 ||
        svc->current_superframe < 1)) ||
      cpi->resize_pending || cpi->resize_state || cpi->external_resize ||
      cpi->resize_state != ORIG) {
    cpi->compute_source_sad_onepass = 0;
    if (cpi->content_state_sb_fd != NULL)
      memset(cpi->content_state_sb_fd, 0,
             (cm->mi_stride >> 3) * ((cm->mi_rows >> 3) + 1) *
                 sizeof(*cpi->content_state_sb_fd));
  }

  // Avoid scaling last_source unless its needed.
  // Last source is needed if avg_source_sad() is used, or if
  // partition_search_type == SOURCE_VAR_BASED_PARTITION, or if noise
  // estimation is enabled.
  if (cpi->unscaled_last_source != NULL &&
      (cpi->oxcf.content == VP9E_CONTENT_SCREEN ||
       (cpi->oxcf.pass == 0 && cpi->oxcf.rc_mode == VPX_VBR &&
        cpi->oxcf.mode == REALTIME && cpi->oxcf.speed >= 5) ||
       cpi->sf.partition_search_type == SOURCE_VAR_BASED_PARTITION ||
       (cpi->noise_estimate.enabled && !cpi->oxcf.noise_sensitivity) ||
       cpi->compute_source_sad_onepass))
    cpi->Last_Source = vp9_scale_if_required(
        cm, cpi->unscaled_last_source, &cpi->scaled_last_source,
        (cpi->oxcf.pass == 0), EIGHTTAP, 0);

  if (cpi->Last_Source == NULL ||
      cpi->Last_Source->y_width != cpi->Source->y_width ||
      cpi->Last_Source->y_height != cpi->Source->y_height)
    cpi->compute_source_sad_onepass = 0;

  if (frame_is_intra_only(cm) || cpi->resize_pending != 0) {
    memset(cpi->consec_zero_mv, 0,
           cm->mi_rows * cm->mi_cols * sizeof(*cpi->consec_zero_mv));
  }

#if CONFIG_VP9_TEMPORAL_DENOISING
  if (cpi->oxcf.noise_sensitivity > 0 && cpi->use_svc)
    vp9_denoiser_reset_on_first_frame(cpi);
#endif

  // Scene detection is always used for VBR mode or screen-content case.
  // For other cases (e.g., CBR mode) use it for 5 <= speed < 8 for now
  // (need to check encoding time cost for doing this for speed 8).
  cpi->rc.high_source_sad = 0;
  cpi->rc.hybrid_intra_scene_change = 0;
  cpi->rc.re_encode_maxq_scene_change = 0;
  if (cm->show_frame && cpi->oxcf.mode == REALTIME &&
      (cpi->oxcf.rc_mode == VPX_VBR ||
       cpi->oxcf.content == VP9E_CONTENT_SCREEN ||
       (cpi->oxcf.speed >= 5 && cpi->oxcf.speed < 8)))
    vp9_scene_detection_onepass(cpi);

  if (svc->spatial_layer_id == svc->first_spatial_layer_to_encode) {
    svc->high_source_sad_superframe = cpi->rc.high_source_sad;
    svc->high_num_blocks_with_motion = cpi->rc.high_num_blocks_with_motion;
    // On scene change reset temporal layer pattern to TL0.
    // Note that if the base/lower spatial layers are skipped: instead of
    // inserting base layer here, we force max-q for the next superframe
    // with lower spatial layers: this is done in vp9_encodedframe_overshoot()
    // when max-q is decided for the current layer.
    // Only do this reset for bypass/flexible mode.
    if (svc->high_source_sad_superframe && svc->temporal_layer_id > 0 &&
        svc->temporal_layering_mode == VP9E_TEMPORAL_LAYERING_MODE_BYPASS) {
      // rc->high_source_sad will get reset so copy it to restore it.
      int tmp_high_source_sad = cpi->rc.high_source_sad;
      vp9_svc_reset_temporal_layers(cpi, cm->frame_type == KEY_FRAME);
      cpi->rc.high_source_sad = tmp_high_source_sad;
    }
  }

  vp9_update_noise_estimate(cpi);

  // For 1 pass CBR, check if we are dropping this frame.
  // Never drop on key frame, if base layer is key for svc,
  // on scene change, or if superframe has layer sync.
  if ((cpi->rc.high_source_sad || svc->high_source_sad_superframe) &&
      !(cpi->rc.use_post_encode_drop && svc->last_layer_dropped[0]))
    no_drop_scene_change = 1;
  if (cpi->oxcf.pass == 0 && cpi->oxcf.rc_mode == VPX_CBR &&
      !frame_is_intra_only(cm) && !no_drop_scene_change &&
      !svc->superframe_has_layer_sync &&
      (!cpi->use_svc ||
       !svc->layer_context[svc->temporal_layer_id].is_key_frame)) {
    if (vp9_rc_drop_frame(cpi)) return 0;
  }

  // For 1 pass CBR SVC, only ZEROMV is allowed for spatial reference frame
  // when svc->force_zero_mode_spatial_ref = 1. Under those conditions we can
  // avoid this frame-level upsampling (for non intra_only frames).
  // For SVC single_layer mode, dynamic resize is allowed and we need to
  // scale references for this case.
  if (frame_is_intra_only(cm) == 0 &&
      ((svc->single_layer_svc && cpi->oxcf.resize_mode == RESIZE_DYNAMIC) ||
       !(is_one_pass_cbr_svc(cpi) && svc->force_zero_mode_spatial_ref))) {
    vp9_scale_references(cpi);
  }

  set_size_independent_vars(cpi);
  set_size_dependent_vars(cpi, &q, &bottom_index, &top_index);

  // search method and step parameter might be changed in speed settings.
  init_motion_estimation(cpi);

  if (cpi->sf.copy_partition_flag) alloc_copy_partition_data(cpi);

  if (cpi->sf.svc_use_lowres_part &&
      svc->spatial_layer_id == svc->number_spatial_layers - 2) {
    if (svc->prev_partition_svc == NULL) {
      CHECK_MEM_ERROR(
          cm, svc->prev_partition_svc,
          (BLOCK_SIZE *)vpx_calloc(cm->mi_stride * cm->mi_rows,
                                   sizeof(*svc->prev_partition_svc)));
    }
  }

  // TODO(jianj): Look into issue of skin detection with high bitdepth.
  if (cm->bit_depth == 8 && cpi->oxcf.speed >= 5 && cpi->oxcf.pass == 0 &&
      cpi->oxcf.rc_mode == VPX_CBR &&
      cpi->oxcf.content != VP9E_CONTENT_SCREEN &&
      cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ) {
    cpi->use_skin_detection = 1;
  }

  // Enable post encode frame dropping for CBR on non key frame, when
  // ext_use_post_encode_drop is specified by user.
  cpi->rc.use_post_encode_drop = cpi->rc.ext_use_post_encode_drop &&
                                 cpi->oxcf.rc_mode == VPX_CBR &&
                                 cm->frame_type != KEY_FRAME;

  vp9_set_quantizer(cpi, q);
  vp9_set_variance_partition_thresholds(cpi, q, 0);

  setup_frame(cpi);

  suppress_active_map(cpi);

  if (cpi->use_svc) {
    // On non-zero spatial layer, check for disabling inter-layer
    // prediction.
    if (svc->spatial_layer_id > 0) vp9_svc_constrain_inter_layer_pred(cpi);
    vp9_svc_assert_constraints_pattern(cpi);
  }

  if (cpi->rc.last_post_encode_dropped_scene_change) {
    cpi->rc.high_source_sad = 1;
    svc->high_source_sad_superframe = 1;
    // For now disable use_source_sad since Last_Source will not be the previous
    // encoded but the dropped one.
    cpi->sf.use_source_sad = 0;
    cpi->rc.last_post_encode_dropped_scene_change = 0;
  }
  // Check if this high_source_sad (scene/slide change) frame should be
  // encoded at high/max QP, and if so, set the q and adjust some rate
  // control parameters.
  if (cpi->sf.overshoot_detection_cbr_rt == FAST_DETECTION_MAXQ &&
      (cpi->rc.high_source_sad ||
       (cpi->use_svc && svc->high_source_sad_superframe))) {
    if (vp9_encodedframe_overshoot(cpi, -1, &q)) {
      vp9_set_quantizer(cpi, q);
      vp9_set_variance_partition_thresholds(cpi, q, 0);
    }
  }

#if !CONFIG_REALTIME_ONLY
  // Variance adaptive and in frame q adjustment experiments are mutually
  // exclusive.
  if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
    vp9_vaq_frame_setup(cpi);
  } else if (cpi->oxcf.aq_mode == EQUATOR360_AQ) {
    vp9_360aq_frame_setup(cpi);
  } else if (cpi->oxcf.aq_mode == COMPLEXITY_AQ) {
    vp9_setup_in_frame_q_adj(cpi);
  } else if (cpi->oxcf.aq_mode == LOOKAHEAD_AQ) {
    // it may be pretty bad for rate-control,
    // and I should handle it somehow
    vp9_alt_ref_aq_setup_map(cpi->alt_ref_aq, cpi);
  } else {
#endif
    if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ) {
      vp9_cyclic_refresh_setup(cpi);
    } else if (cpi->roi.enabled && !frame_is_intra_only(cm)) {
      apply_roi_map(cpi);
    }
#if !CONFIG_REALTIME_ONLY
  }
#endif

  apply_active_map(cpi);

  vp9_encode_frame(cpi);

  // Check if we should re-encode this frame at high Q because of high
  // overshoot based on the encoded frame size. Only for frames where
  // high temporal-source SAD is detected.
  // For SVC: all spatial layers are checked for re-encoding.
  if (cpi->sf.overshoot_detection_cbr_rt == RE_ENCODE_MAXQ &&
      (cpi->rc.high_source_sad ||
       (cpi->use_svc && svc->high_source_sad_superframe))) {
    int frame_size = 0;
    // Get an estimate of the encoded frame size.
    save_coding_context(cpi);
    vp9_pack_bitstream(cpi, dest, size);
    restore_coding_context(cpi);
    frame_size = (int)(*size) << 3;
    // Check if encoded frame will overshoot too much, and if so, set the q and
    // adjust some rate control parameters, and return to re-encode the frame.
    if (vp9_encodedframe_overshoot(cpi, frame_size, &q)) {
      vpx_clear_system_state();
      vp9_set_quantizer(cpi, q);
      vp9_set_variance_partition_thresholds(cpi, q, 0);
      suppress_active_map(cpi);
      // Turn-off cyclic refresh for re-encoded frame.
      if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ) {
        CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
        unsigned char *const seg_map = cpi->segmentation_map;
        memset(seg_map, 0, cm->mi_rows * cm->mi_cols);
        memset(cr->last_coded_q_map, MAXQ,
               cm->mi_rows * cm->mi_cols * sizeof(*cr->last_coded_q_map));
        cr->sb_index = 0;
        vp9_disable_segmentation(&cm->seg);
      }
      apply_active_map(cpi);
      vp9_encode_frame(cpi);
    }
  }

  // Update some stats from cyclic refresh, and check for golden frame update.
  if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ && cm->seg.enabled &&
      !frame_is_intra_only(cm))
    vp9_cyclic_refresh_postencode(cpi);

  // Update the skip mb flag probabilities based on the distribution
  // seen in the last encoder iteration.
  // update_base_skip_probs(cpi);
  vpx_clear_system_state();
  return 1;
}

static int get_ref_frame_flags(const VP9_COMP *cpi) {
  const int *const map = cpi->common.ref_frame_map;
  const int gold_is_last = map[cpi->gld_fb_idx] == map[cpi->lst_fb_idx];
  const int alt_is_last = map[cpi->alt_fb_idx] == map[cpi->lst_fb_idx];
  const int gold_is_alt = map[cpi->gld_fb_idx] == map[cpi->alt_fb_idx];
  int flags = VP9_ALT_FLAG | VP9_GOLD_FLAG | VP9_LAST_FLAG;

  if (gold_is_last) flags &= ~VP9_GOLD_FLAG;

  if (cpi->rc.frames_till_gf_update_due == INT_MAX &&
      (cpi->svc.number_temporal_layers == 1 &&
       cpi->svc.number_spatial_layers == 1))
    flags &= ~VP9_GOLD_FLAG;

  if (alt_is_last) flags &= ~VP9_ALT_FLAG;

  if (gold_is_alt) flags &= ~VP9_ALT_FLAG;

  return flags;
}

#if !CONFIG_REALTIME_ONLY
#define MAX_QSTEP_ADJ 4
static int get_qstep_adj(int rate_excess, int rate_limit) {
  int qstep =
      rate_limit ? ((rate_excess + rate_limit / 2) / rate_limit) : INT_MAX;
  return VPXMIN(qstep, MAX_QSTEP_ADJ);
}

#if CONFIG_RATE_CTRL
static void init_rq_history(RATE_QINDEX_HISTORY *rq_history) {
  rq_history->recode_count = 0;
  rq_history->q_index_high = 255;
  rq_history->q_index_low = 0;
}

static void update_rq_history(RATE_QINDEX_HISTORY *rq_history, int target_bits,
                              int actual_bits, int q_index) {
  rq_history->q_index_history[rq_history->recode_count] = q_index;
  rq_history->rate_history[rq_history->recode_count] = actual_bits;
  if (actual_bits <= target_bits) {
    rq_history->q_index_high = q_index;
  }
  if (actual_bits >= target_bits) {
    rq_history->q_index_low = q_index;
  }
  rq_history->recode_count += 1;
}

static int guess_q_index_from_model(const RATE_QSTEP_MODEL *rq_model,
                                    int target_bits) {
  // The model predicts bits as follows.
  // target_bits = bias - ratio * log2(q_step)
  // Given the target_bits, we compute the q_step as follows.
  double q_step;
  assert(rq_model->ratio > 0);
  q_step = pow(2.0, (rq_model->bias - target_bits) / rq_model->ratio);
  // TODO(angiebird): Make this function support highbitdepth.
  return vp9_convert_q_to_qindex(q_step, VPX_BITS_8);
}

static int guess_q_index_linear(int prev_q_index, int target_bits,
                                int actual_bits, int gap) {
  int q_index = prev_q_index;
  if (actual_bits < target_bits) {
    q_index -= gap;
    q_index = VPXMAX(q_index, 0);
  } else {
    q_index += gap;
    q_index = VPXMIN(q_index, 255);
  }
  return q_index;
}

static double get_bits_percent_diff(int target_bits, int actual_bits) {
  double diff;
  target_bits = VPXMAX(target_bits, 1);
  diff = abs(target_bits - actual_bits) * 1. / target_bits;
  return diff * 100;
}

static int rq_model_predict_q_index(const RATE_QSTEP_MODEL *rq_model,
                                    const RATE_QINDEX_HISTORY *rq_history,
                                    int target_bits) {
  int q_index = 128;
  if (rq_history->recode_count > 0) {
    const int actual_bits =
        rq_history->rate_history[rq_history->recode_count - 1];
    const int prev_q_index =
        rq_history->q_index_history[rq_history->recode_count - 1];
    const double percent_diff = get_bits_percent_diff(target_bits, actual_bits);
    if (percent_diff > 50) {
      // Binary search.
      // When the actual_bits and target_bits are far apart, binary search
      // q_index is faster.
      q_index = (rq_history->q_index_low + rq_history->q_index_high) / 2;
    } else {
      if (rq_model->ready) {
        q_index = guess_q_index_from_model(rq_model, target_bits);
      } else {
        // TODO(angiebird): Find a better way to set the gap.
        q_index =
            guess_q_index_linear(prev_q_index, target_bits, actual_bits, 20);
      }
    }
  } else {
    if (rq_model->ready) {
      q_index = guess_q_index_from_model(rq_model, target_bits);
    }
  }

  assert(rq_history->q_index_low <= rq_history->q_index_high);
  if (q_index <= rq_history->q_index_low) {
    q_index = rq_history->q_index_low + 1;
  }
  if (q_index >= rq_history->q_index_high) {
    q_index = rq_history->q_index_high - 1;
  }
  return q_index;
}

static void rq_model_update(const RATE_QINDEX_HISTORY *rq_history,
                            int target_bits, RATE_QSTEP_MODEL *rq_model) {
  const int recode_count = rq_history->recode_count;
  const double delta = 0.00001;
  if (recode_count >= 2) {
    const int q_index1 = rq_history->q_index_history[recode_count - 2];
    const int q_index2 = rq_history->q_index_history[recode_count - 1];
    const int r1 = rq_history->rate_history[recode_count - 2];
    const int r2 = rq_history->rate_history[recode_count - 1];
    int valid = 0;
    // lower q_index should yield higher bit rate
    if (q_index1 < q_index2) {
      valid = r1 > r2;
    } else if (q_index1 > q_index2) {
      valid = r1 < r2;
    }
    // Only update the model when the q_index and rate behave normally.
    if (valid) {
      // Fit the ratio and bias of rq_model based on last two recode histories.
      const double s1 = vp9_convert_qindex_to_q(q_index1, VPX_BITS_8);
      const double s2 = vp9_convert_qindex_to_q(q_index2, VPX_BITS_8);
      if (fabs(log2(s1) - log2(s2)) > delta) {
        rq_model->ratio = (r2 - r1) / (log2(s1) - log2(s2));
        rq_model->bias = r1 + (rq_model->ratio) * log2(s1);
        if (rq_model->ratio > delta && rq_model->bias > delta) {
          rq_model->ready = 1;
        }
      }
    }
  } else if (recode_count == 1) {
    if (rq_model->ready) {
      // Update the ratio only when the initial model exists and we only have
      // one recode history.
      const int prev_q = rq_history->q_index_history[recode_count - 1];
      const double prev_q_step = vp9_convert_qindex_to_q(prev_q, VPX_BITS_8);
      if (fabs(log2(prev_q_step)) > delta) {
        const int actual_bits = rq_history->rate_history[recode_count - 1];
        rq_model->ratio =
            rq_model->ratio + (target_bits - actual_bits) / log2(prev_q_step);
      }
    }
  }
}
#endif  // CONFIG_RATE_CTRL

static void encode_with_recode_loop(VP9_COMP *cpi, size_t *size, uint8_t *dest
#if CONFIG_RATE_CTRL
                                    ,
                                    RATE_QINDEX_HISTORY *rq_history
#endif  // CONFIG_RATE_CTRL
) {
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  VP9_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  int bottom_index, top_index;
  int loop_count = 0;
  int loop_at_this_size = 0;
  int loop = 0;
  int overshoot_seen = 0;
  int undershoot_seen = 0;
  int frame_over_shoot_limit;
  int frame_under_shoot_limit;
  int q = 0, q_low = 0, q_high = 0;
  int enable_acl;
#ifdef AGGRESSIVE_VBR
  int qrange_adj = 1;
#endif

#if CONFIG_RATE_CTRL
  const FRAME_UPDATE_TYPE update_type =
      cpi->twopass.gf_group.update_type[cpi->twopass.gf_group.index];
  const ENCODE_FRAME_TYPE frame_type = get_encode_frame_type(update_type);
  RATE_QSTEP_MODEL *rq_model = &cpi->rq_model[frame_type];
  init_rq_history(rq_history);
#endif  // CONFIG_RATE_CTRL

  if (cm->show_existing_frame) {
    rc->this_frame_target = 0;
    if (is_psnr_calc_enabled(cpi)) set_raw_source_frame(cpi);
    return;
  }

  set_size_independent_vars(cpi);

  enable_acl = cpi->sf.allow_acl ? (cm->frame_type == KEY_FRAME) ||
                                       (cpi->twopass.gf_group.index == 1)
                                 : 0;

  do {
    vpx_clear_system_state();

    set_frame_size(cpi);

    if (loop_count == 0 || cpi->resize_pending != 0) {
      set_size_dependent_vars(cpi, &q, &bottom_index, &top_index);

#ifdef AGGRESSIVE_VBR
      if (two_pass_first_group_inter(cpi)) {
        // Adjustment limits for min and max q
        qrange_adj = VPXMAX(1, (top_index - bottom_index) / 2);

        bottom_index =
            VPXMAX(bottom_index - qrange_adj / 2, oxcf->best_allowed_q);
        top_index = VPXMIN(oxcf->worst_allowed_q, top_index + qrange_adj / 2);
      }
#endif
      // TODO(agrange) Scale cpi->max_mv_magnitude if frame-size has changed.
      set_mv_search_params(cpi);

      // Reset the loop state for new frame size.
      overshoot_seen = 0;
      undershoot_seen = 0;

      // Reconfiguration for change in frame size has concluded.
      cpi->resize_pending = 0;

      q_low = bottom_index;
      q_high = top_index;

      loop_at_this_size = 0;
    }

#if CONFIG_RATE_CTRL
    if (cpi->encode_command.use_external_target_frame_bits) {
      q = rq_model_predict_q_index(rq_model, rq_history, rc->this_frame_target);
    }
#endif  // CONFIG_RATE_CTRL
    // Decide frame size bounds first time through.
    if (loop_count == 0) {
      vp9_rc_compute_frame_size_bounds(cpi, rc->this_frame_target,
                                       &frame_under_shoot_limit,
                                       &frame_over_shoot_limit);
    }

    cpi->Source =
        vp9_scale_if_required(cm, cpi->un_scaled_source, &cpi->scaled_source,
                              (oxcf->pass == 0), EIGHTTAP, 0);

    // Unfiltered raw source used in metrics calculation if the source
    // has been filtered.
    if (is_psnr_calc_enabled(cpi)) {
#ifdef ENABLE_KF_DENOISE
      if (is_spatial_denoise_enabled(cpi)) {
        cpi->raw_source_frame = vp9_scale_if_required(
            cm, &cpi->raw_unscaled_source, &cpi->raw_scaled_source,
            (oxcf->pass == 0), EIGHTTAP, 0);
      } else {
        cpi->raw_source_frame = cpi->Source;
      }
#else
      cpi->raw_source_frame = cpi->Source;
#endif
    }

    if (cpi->unscaled_last_source != NULL)
      cpi->Last_Source = vp9_scale_if_required(cm, cpi->unscaled_last_source,
                                               &cpi->scaled_last_source,
                                               (oxcf->pass == 0), EIGHTTAP, 0);

    if (frame_is_intra_only(cm) == 0) {
      if (loop_count > 0) {
        release_scaled_references(cpi);
      }
      vp9_scale_references(cpi);
    }

#if CONFIG_RATE_CTRL
    // TODO(angiebird): This is a hack for making sure the encoder use the
    // external_quantize_index exactly. Avoid this kind of hack later.
    if (cpi->encode_command.use_external_quantize_index) {
      q = cpi->encode_command.external_quantize_index;
    }
#endif
    if (cpi->ext_ratectrl.ready) {
      vpx_codec_err_t codec_status;
      const GF_GROUP *gf_group = &cpi->twopass.gf_group;
      vpx_rc_encodeframe_decision_t encode_frame_decision;
      FRAME_UPDATE_TYPE update_type = gf_group->update_type[gf_group->index];
      const int ref_frame_flags = get_ref_frame_flags(cpi);
      RefCntBuffer *ref_frame_bufs[MAX_INTER_REF_FRAMES];
      const RefCntBuffer *curr_frame_buf =
          get_ref_cnt_buffer(cm, cm->new_fb_idx);
      get_ref_frame_bufs(cpi, ref_frame_bufs);
      codec_status = vp9_extrc_get_encodeframe_decision(
          &cpi->ext_ratectrl, curr_frame_buf->frame_index,
          cm->current_frame_coding_index, gf_group->index, update_type,
          ref_frame_bufs, ref_frame_flags, &encode_frame_decision);
      if (codec_status != VPX_CODEC_OK) {
        vpx_internal_error(&cm->error, codec_status,
                           "vp9_extrc_get_encodeframe_decision() failed");
      }
      q = encode_frame_decision.q_index;
    }

    vp9_set_quantizer(cpi, q);

    if (loop_count == 0) setup_frame(cpi);

    // Variance adaptive and in frame q adjustment experiments are mutually
    // exclusive.
    if (oxcf->aq_mode == VARIANCE_AQ) {
      vp9_vaq_frame_setup(cpi);
    } else if (oxcf->aq_mode == EQUATOR360_AQ) {
      vp9_360aq_frame_setup(cpi);
    } else if (oxcf->aq_mode == COMPLEXITY_AQ) {
      vp9_setup_in_frame_q_adj(cpi);
    } else if (oxcf->aq_mode == LOOKAHEAD_AQ) {
      vp9_alt_ref_aq_setup_map(cpi->alt_ref_aq, cpi);
    } else if (oxcf->aq_mode == PSNR_AQ) {
      vp9_psnr_aq_mode_setup(&cm->seg);
    }

    vp9_encode_frame(cpi);

    // Update the skip mb flag probabilities based on the distribution
    // seen in the last encoder iteration.
    // update_base_skip_probs(cpi);

    vpx_clear_system_state();

    // Dummy pack of the bitstream using up to date stats to get an
    // accurate estimate of output frame size to determine if we need
    // to recode.
    if (cpi->sf.recode_loop >= ALLOW_RECODE_KFARFGF) {
      save_coding_context(cpi);
      if (!cpi->sf.use_nonrd_pick_mode) vp9_pack_bitstream(cpi, dest, size);

      rc->projected_frame_size = (int)(*size) << 3;

      if (frame_over_shoot_limit == 0) frame_over_shoot_limit = 1;
    }

    if (cpi->ext_ratectrl.ready) {
      break;
    }
#if CONFIG_RATE_CTRL
    // This part needs to be after save_coding_context() because
    // restore_coding_context will be called in the end of this function.
    // TODO(angiebird): This is a hack for making sure the encoder use the
    // external_quantize_index exactly. Avoid this kind of hack later.
    if (cpi->encode_command.use_external_quantize_index) {
      break;
    }

    if (cpi->encode_command.use_external_target_frame_bits) {
      const double percent_diff = get_bits_percent_diff(
          rc->this_frame_target, rc->projected_frame_size);
      update_rq_history(rq_history, rc->this_frame_target,
                        rc->projected_frame_size, q);
      loop_count += 1;

      rq_model_update(rq_history, rc->this_frame_target, rq_model);

      // Check if we hit the target bitrate.
      if (percent_diff <= cpi->encode_command.target_frame_bits_error_percent ||
          rq_history->recode_count >= RATE_CTRL_MAX_RECODE_NUM ||
          rq_history->q_index_low >= rq_history->q_index_high) {
        break;
      }

      loop = 1;
      restore_coding_context(cpi);
      continue;
    }
#endif  // CONFIG_RATE_CTRL

    if (oxcf->rc_mode == VPX_Q) {
      loop = 0;
    } else {
      if ((cm->frame_type == KEY_FRAME) && rc->this_key_frame_forced &&
          (rc->projected_frame_size < rc->max_frame_bandwidth)) {
        int last_q = q;
        int64_t kf_err;

        int64_t high_err_target = cpi->ambient_err;
        int64_t low_err_target = cpi->ambient_err >> 1;

#if CONFIG_VP9_HIGHBITDEPTH
        if (cm->use_highbitdepth) {
          kf_err = vpx_highbd_get_y_sse(cpi->Source, get_frame_new_buffer(cm));
        } else {
          kf_err = vpx_get_y_sse(cpi->Source, get_frame_new_buffer(cm));
        }
#else
        kf_err = vpx_get_y_sse(cpi->Source, get_frame_new_buffer(cm));
#endif  // CONFIG_VP9_HIGHBITDEPTH

        // Prevent possible divide by zero error below for perfect KF
        kf_err += !kf_err;

        // The key frame is not good enough or we can afford
        // to make it better without undue risk of popping.
        if ((kf_err > high_err_target &&
             rc->projected_frame_size <= frame_over_shoot_limit) ||
            (kf_err > low_err_target &&
             rc->projected_frame_size <= frame_under_shoot_limit)) {
          // Lower q_high
          q_high = q > q_low ? q - 1 : q_low;

          // Adjust Q
          q = (int)((q * high_err_target) / kf_err);
          q = VPXMIN(q, (q_high + q_low) >> 1);
        } else if (kf_err < low_err_target &&
                   rc->projected_frame_size >= frame_under_shoot_limit) {
          // The key frame is much better than the previous frame
          // Raise q_low
          q_low = q < q_high ? q + 1 : q_high;

          // Adjust Q
          q = (int)((q * low_err_target) / kf_err);
          q = VPXMIN(q, (q_high + q_low + 1) >> 1);
        }

        // Clamp Q to upper and lower limits:
        q = clamp(q, q_low, q_high);

        loop = q != last_q;
      } else if (recode_loop_test(cpi, frame_over_shoot_limit,
                                  frame_under_shoot_limit, q,
                                  VPXMAX(q_high, top_index), bottom_index)) {
        // Is the projected frame size out of range and are we allowed
        // to attempt to recode.
        int last_q = q;
        int retries = 0;
        int qstep;

        if (cpi->resize_pending == 1) {
          // Change in frame size so go back around the recode loop.
          cpi->rc.frame_size_selector =
              SCALE_STEP1 - cpi->rc.frame_size_selector;
          cpi->rc.next_frame_size_selector = cpi->rc.frame_size_selector;

#if CONFIG_INTERNAL_STATS
          ++cpi->tot_recode_hits;
#endif
          ++loop_count;
          loop = 1;
          continue;
        }

        // Frame size out of permitted range:
        // Update correction factor & compute new Q to try...

        // Frame is too large
        if (rc->projected_frame_size > rc->this_frame_target) {
          // Special case if the projected size is > the max allowed.
          if ((q == q_high) &&
              ((rc->projected_frame_size >= rc->max_frame_bandwidth) ||
               (!rc->is_src_frame_alt_ref &&
                (rc->projected_frame_size >=
                 big_rate_miss_high_threshold(cpi))))) {
            int max_rate = VPXMAX(1, VPXMIN(rc->max_frame_bandwidth,
                                            big_rate_miss_high_threshold(cpi)));
            double q_val_high;
            q_val_high = vp9_convert_qindex_to_q(q_high, cm->bit_depth);
            q_val_high =
                q_val_high * ((double)rc->projected_frame_size / max_rate);
            q_high = vp9_convert_q_to_qindex(q_val_high, cm->bit_depth);
            q_high = clamp(q_high, rc->best_quality, rc->worst_quality);
          }

          // Raise Qlow as to at least the current value
          qstep =
              get_qstep_adj(rc->projected_frame_size, rc->this_frame_target);
          q_low = VPXMIN(q + qstep, q_high);

          if (undershoot_seen || loop_at_this_size > 1) {
            // Update rate_correction_factor unless
            vp9_rc_update_rate_correction_factors(cpi);

            q = (q_high + q_low + 1) / 2;
          } else {
            // Update rate_correction_factor unless
            vp9_rc_update_rate_correction_factors(cpi);

            q = vp9_rc_regulate_q(cpi, rc->this_frame_target, bottom_index,
                                  VPXMAX(q_high, top_index));

            while (q < q_low && retries < 10) {
              vp9_rc_update_rate_correction_factors(cpi);
              q = vp9_rc_regulate_q(cpi, rc->this_frame_target, bottom_index,
                                    VPXMAX(q_high, top_index));
              retries++;
            }
          }

          overshoot_seen = 1;
        } else {
          // Frame is too small
          qstep =
              get_qstep_adj(rc->this_frame_target, rc->projected_frame_size);
          q_high = VPXMAX(q - qstep, q_low);

          if (overshoot_seen || loop_at_this_size > 1) {
            vp9_rc_update_rate_correction_factors(cpi);
            q = (q_high + q_low) / 2;
          } else {
            vp9_rc_update_rate_correction_factors(cpi);
            q = vp9_rc_regulate_q(cpi, rc->this_frame_target,
                                  VPXMIN(q_low, bottom_index), top_index);
            // Special case reset for qlow for constrained quality.
            // This should only trigger where there is very substantial
            // undershoot on a frame and the auto cq level is above
            // the user passed in value.
            if (oxcf->rc_mode == VPX_CQ && q < q_low) {
              q_low = q;
            }

            while (q > q_high && retries < 10) {
              vp9_rc_update_rate_correction_factors(cpi);
              q = vp9_rc_regulate_q(cpi, rc->this_frame_target,
                                    VPXMIN(q_low, bottom_index), top_index);
              retries++;
            }
          }
          undershoot_seen = 1;
        }

        // Clamp Q to upper and lower limits:
        q = clamp(q, q_low, q_high);

        loop = (q != last_q);
      } else {
        loop = 0;
      }
    }

    // Special case for overlay frame.
    if (rc->is_src_frame_alt_ref &&
        rc->projected_frame_size < rc->max_frame_bandwidth)
      loop = 0;

    if (loop) {
      ++loop_count;
      ++loop_at_this_size;

#if CONFIG_INTERNAL_STATS
      ++cpi->tot_recode_hits;
#endif
    }

    if (cpi->sf.recode_loop >= ALLOW_RECODE_KFARFGF)
      if (loop) restore_coding_context(cpi);
  } while (loop);

#ifdef AGGRESSIVE_VBR
  if (two_pass_first_group_inter(cpi)) {
    cpi->twopass.active_worst_quality =
        VPXMIN(q + qrange_adj, oxcf->worst_allowed_q);
  } else if (!frame_is_kf_gf_arf(cpi)) {
#else
  if (!frame_is_kf_gf_arf(cpi)) {
#endif
    // Have we been forced to adapt Q outside the expected range by an extreme
    // rate miss. If so adjust the active maxQ for the subsequent frames.
    if (!rc->is_src_frame_alt_ref && (q > cpi->twopass.active_worst_quality)) {
      cpi->twopass.active_worst_quality = q;
    } else if (oxcf->vbr_corpus_complexity && q == q_low &&
               rc->projected_frame_size < rc->this_frame_target) {
      cpi->twopass.active_worst_quality =
          VPXMAX(q, cpi->twopass.active_worst_quality - 1);
    }
  }

  if (enable_acl) {
    // Skip recoding, if model diff is below threshold
    const int thresh = compute_context_model_thresh(cpi);
    const int diff = compute_context_model_diff(cm);
    if (diff >= thresh) {
      vp9_encode_frame(cpi);
    }
  }
  if (cpi->sf.recode_loop >= ALLOW_RECODE_KFARFGF) {
    vpx_clear_system_state();
    restore_coding_context(cpi);
  }
}
#endif  // !CONFIG_REALTIME_ONLY

static void set_ext_overrides(VP9_COMP *cpi) {
  // Overrides the defaults with the externally supplied values with
  // vp9_update_reference() and vp9_update_entropy() calls
  // Note: The overrides are valid only for the next frame passed
  // to encode_frame_to_data_rate() function
  if (cpi->ext_refresh_frame_context_pending) {
    cpi->common.refresh_frame_context = cpi->ext_refresh_frame_context;
    cpi->ext_refresh_frame_context_pending = 0;
  }
  if (cpi->ext_refresh_frame_flags_pending) {
    cpi->refresh_last_frame = cpi->ext_refresh_last_frame;
    cpi->refresh_golden_frame = cpi->ext_refresh_golden_frame;
    cpi->refresh_alt_ref_frame = cpi->ext_refresh_alt_ref_frame;
  }
}

YV12_BUFFER_CONFIG *vp9_svc_twostage_scale(
    VP9_COMMON *cm, YV12_BUFFER_CONFIG *unscaled, YV12_BUFFER_CONFIG *scaled,
    YV12_BUFFER_CONFIG *scaled_temp, INTERP_FILTER filter_type,
    int phase_scaler, INTERP_FILTER filter_type2, int phase_scaler2) {
  if (cm->mi_cols * MI_SIZE != unscaled->y_width ||
      cm->mi_rows * MI_SIZE != unscaled->y_height) {
#if CONFIG_VP9_HIGHBITDEPTH
    if (cm->bit_depth == VPX_BITS_8) {
      vp9_scale_and_extend_frame(unscaled, scaled_temp, filter_type2,
                                 phase_scaler2);
      vp9_scale_and_extend_frame(scaled_temp, scaled, filter_type,
                                 phase_scaler);
    } else {
      scale_and_extend_frame(unscaled, scaled_temp, (int)cm->bit_depth,
                             filter_type2, phase_scaler2);
      scale_and_extend_frame(scaled_temp, scaled, (int)cm->bit_depth,
                             filter_type, phase_scaler);
    }
#else
    vp9_scale_and_extend_frame(unscaled, scaled_temp, filter_type2,
                               phase_scaler2);
    vp9_scale_and_extend_frame(scaled_temp, scaled, filter_type, phase_scaler);
#endif  // CONFIG_VP9_HIGHBITDEPTH
    return scaled;
  } else {
    return unscaled;
  }
}

YV12_BUFFER_CONFIG *vp9_scale_if_required(
    VP9_COMMON *cm, YV12_BUFFER_CONFIG *unscaled, YV12_BUFFER_CONFIG *scaled,
    int use_normative_scaler, INTERP_FILTER filter_type, int phase_scaler) {
  if (cm->mi_cols * MI_SIZE != unscaled->y_width ||
      cm->mi_rows * MI_SIZE != unscaled->y_height) {
#if CONFIG_VP9_HIGHBITDEPTH
    if (use_normative_scaler && unscaled->y_width <= (scaled->y_width << 1) &&
        unscaled->y_height <= (scaled->y_height << 1))
      if (cm->bit_depth == VPX_BITS_8)
        vp9_scale_and_extend_frame(unscaled, scaled, filter_type, phase_scaler);
      else
        scale_and_extend_frame(unscaled, scaled, (int)cm->bit_depth,
                               filter_type, phase_scaler);
    else
      scale_and_extend_frame_nonnormative(unscaled, scaled, (int)cm->bit_depth);
#else
    if (use_normative_scaler && unscaled->y_width <= (scaled->y_width << 1) &&
        unscaled->y_height <= (scaled->y_height << 1))
      vp9_scale_and_extend_frame(unscaled, scaled, filter_type, phase_scaler);
    else
      scale_and_extend_frame_nonnormative(unscaled, scaled);
#endif  // CONFIG_VP9_HIGHBITDEPTH
    return scaled;
  } else {
    return unscaled;
  }
}

static void set_ref_sign_bias(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  RefCntBuffer *const ref_buffer = get_ref_cnt_buffer(cm, cm->new_fb_idx);
  const int cur_frame_index = ref_buffer->frame_index;
  MV_REFERENCE_FRAME ref_frame;

  for (ref_frame = LAST_FRAME; ref_frame < MAX_REF_FRAMES; ++ref_frame) {
    const int buf_idx = get_ref_frame_buf_idx(cpi, ref_frame);
    const RefCntBuffer *const ref_cnt_buf =
        get_ref_cnt_buffer(&cpi->common, buf_idx);
    if (ref_cnt_buf) {
      cm->ref_frame_sign_bias[ref_frame] =
          cur_frame_index < ref_cnt_buf->frame_index;
    }
  }
}

static int setup_interp_filter_search_mask(VP9_COMP *cpi) {
  INTERP_FILTER ifilter;
  int ref_total[MAX_REF_FRAMES] = { 0 };
  MV_REFERENCE_FRAME ref;
  int mask = 0;
  if (cpi->common.last_frame_type == KEY_FRAME || cpi->refresh_alt_ref_frame)
    return mask;
  for (ref = LAST_FRAME; ref <= ALTREF_FRAME; ++ref)
    for (ifilter = EIGHTTAP; ifilter <= EIGHTTAP_SHARP; ++ifilter)
      ref_total[ref] += cpi->interp_filter_selected[ref][ifilter];

  for (ifilter = EIGHTTAP; ifilter <= EIGHTTAP_SHARP; ++ifilter) {
    if ((ref_total[LAST_FRAME] &&
         cpi->interp_filter_selected[LAST_FRAME][ifilter] == 0) &&
        (ref_total[GOLDEN_FRAME] == 0 ||
         cpi->interp_filter_selected[GOLDEN_FRAME][ifilter] * 50 <
             ref_total[GOLDEN_FRAME]) &&
        (ref_total[ALTREF_FRAME] == 0 ||
         cpi->interp_filter_selected[ALTREF_FRAME][ifilter] * 50 <
             ref_total[ALTREF_FRAME]))
      mask |= 1 << ifilter;
  }
  return mask;
}

#ifdef ENABLE_KF_DENOISE
// Baseline kernel weights for denoise
static uint8_t dn_kernal_3[9] = { 1, 2, 1, 2, 4, 2, 1, 2, 1 };
static uint8_t dn_kernal_5[25] = { 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 2, 4,
                                   2, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1 };

static INLINE void add_denoise_point(int centre_val, int data_val, int thresh,
                                     uint8_t point_weight, int *sum_val,
                                     int *sum_weight) {
  if (abs(centre_val - data_val) <= thresh) {
    *sum_weight += point_weight;
    *sum_val += (int)data_val * (int)point_weight;
  }
}

static void spatial_denoise_point(uint8_t *src_ptr, const int stride,
                                  const int strength) {
  int sum_weight = 0;
  int sum_val = 0;
  int thresh = strength;
  int kernal_size = 5;
  int half_k_size = 2;
  int i, j;
  int max_diff = 0;
  uint8_t *tmp_ptr;
  uint8_t *kernal_ptr;

  // Find the maximum deviation from the source point in the locale.
  tmp_ptr = src_ptr - (stride * (half_k_size + 1)) - (half_k_size + 1);
  for (i = 0; i < kernal_size + 2; ++i) {
    for (j = 0; j < kernal_size + 2; ++j) {
      max_diff = VPXMAX(max_diff, abs((int)*src_ptr - (int)tmp_ptr[j]));
    }
    tmp_ptr += stride;
  }

  // Select the kernel size.
  if (max_diff > (strength + (strength >> 1))) {
    kernal_size = 3;
    half_k_size = 1;
    thresh = thresh >> 1;
  }
  kernal_ptr = (kernal_size == 3) ? dn_kernal_3 : dn_kernal_5;

  // Apply the kernel
  tmp_ptr = src_ptr - (stride * half_k_size) - half_k_size;
  for (i = 0; i < kernal_size; ++i) {
    for (j = 0; j < kernal_size; ++j) {
      add_denoise_point((int)*src_ptr, (int)tmp_ptr[j], thresh, *kernal_ptr,
                        &sum_val, &sum_weight);
      ++kernal_ptr;
    }
    tmp_ptr += stride;
  }

  // Update the source value with the new filtered value
  *src_ptr = (uint8_t)((sum_val + (sum_weight >> 1)) / sum_weight);
}

#if CONFIG_VP9_HIGHBITDEPTH
static void highbd_spatial_denoise_point(uint16_t *src_ptr, const int stride,
                                         const int strength) {
  int sum_weight = 0;
  int sum_val = 0;
  int thresh = strength;
  int kernal_size = 5;
  int half_k_size = 2;
  int i, j;
  int max_diff = 0;
  uint16_t *tmp_ptr;
  uint8_t *kernal_ptr;

  // Find the maximum deviation from the source point in the locale.
  tmp_ptr = src_ptr - (stride * (half_k_size + 1)) - (half_k_size + 1);
  for (i = 0; i < kernal_size + 2; ++i) {
    for (j = 0; j < kernal_size + 2; ++j) {
      max_diff = VPXMAX(max_diff, abs((int)src_ptr - (int)tmp_ptr[j]));
    }
    tmp_ptr += stride;
  }

  // Select the kernel size.
  if (max_diff > (strength + (strength >> 1))) {
    kernal_size = 3;
    half_k_size = 1;
    thresh = thresh >> 1;
  }
  kernal_ptr = (kernal_size == 3) ? dn_kernal_3 : dn_kernal_5;

  // Apply the kernel
  tmp_ptr = src_ptr - (stride * half_k_size) - half_k_size;
  for (i = 0; i < kernal_size; ++i) {
    for (j = 0; j < kernal_size; ++j) {
      add_denoise_point((int)*src_ptr, (int)tmp_ptr[j], thresh, *kernal_ptr,
                        &sum_val, &sum_weight);
      ++kernal_ptr;
    }
    tmp_ptr += stride;
  }

  // Update the source value with the new filtered value
  *src_ptr = (uint16_t)((sum_val + (sum_weight >> 1)) / sum_weight);
}
#endif  // CONFIG_VP9_HIGHBITDEPTH

// Apply thresholded spatial noise suppression to a given buffer.
static void spatial_denoise_buffer(VP9_COMP *cpi, uint8_t *buffer,
                                   const int stride, const int width,
                                   const int height, const int strength) {
  VP9_COMMON *const cm = &cpi->common;
  uint8_t *src_ptr = buffer;
  int row;
  int col;

  for (row = 0; row < height; ++row) {
    for (col = 0; col < width; ++col) {
#if CONFIG_VP9_HIGHBITDEPTH
      if (cm->use_highbitdepth)
        highbd_spatial_denoise_point(CONVERT_TO_SHORTPTR(&src_ptr[col]), stride,
                                     strength);
      else
        spatial_denoise_point(&src_ptr[col], stride, strength);
#else
      spatial_denoise_point(&src_ptr[col], stride, strength);
#endif  // CONFIG_VP9_HIGHBITDEPTH
    }
    src_ptr += stride;
  }
}

// Apply thresholded spatial noise suppression to source.
static void spatial_denoise_frame(VP9_COMP *cpi) {
  YV12_BUFFER_CONFIG *src = cpi->Source;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  TWO_PASS *const twopass = &cpi->twopass;
  VP9_COMMON *const cm = &cpi->common;

  // Base the filter strength on the current active max Q.
  const int q = (int)(vp9_convert_qindex_to_q(twopass->active_worst_quality,
                                              cm->bit_depth));
  int strength =
      VPXMAX(oxcf->arnr_strength >> 2, VPXMIN(oxcf->arnr_strength, (q >> 4)));

  // Denoise each of Y,U and V buffers.
  spatial_denoise_buffer(cpi, src->y_buffer, src->y_stride, src->y_width,
                         src->y_height, strength);

  strength += (strength >> 1);
  spatial_denoise_buffer(cpi, src->u_buffer, src->uv_stride, src->uv_width,
                         src->uv_height, strength << 1);

  spatial_denoise_buffer(cpi, src->v_buffer, src->uv_stride, src->uv_width,
                         src->uv_height, strength << 1);
}
#endif  // ENABLE_KF_DENOISE

#if !CONFIG_REALTIME_ONLY
static void vp9_try_disable_lookahead_aq(VP9_COMP *cpi, size_t *size,
                                         uint8_t *dest) {
  if (cpi->common.seg.enabled)
    if (ALT_REF_AQ_PROTECT_GAIN) {
      size_t nsize = *size;
      int overhead;

      // TODO(yuryg): optimize this, as
      // we don't really need to repack

      save_coding_context(cpi);
      vp9_disable_segmentation(&cpi->common.seg);
      vp9_pack_bitstream(cpi, dest, &nsize);
      restore_coding_context(cpi);

      overhead = (int)*size - (int)nsize;

      if (vp9_alt_ref_aq_disable_if(cpi->alt_ref_aq, overhead, (int)*size))
        vp9_encode_frame(cpi);
      else
        vp9_enable_segmentation(&cpi->common.seg);
    }
}
#endif

static void set_frame_index(VP9_COMP *cpi, VP9_COMMON *cm) {
  RefCntBuffer *const ref_buffer = get_ref_cnt_buffer(cm, cm->new_fb_idx);

  if (ref_buffer) {
    const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
    ref_buffer->frame_index =
        cm->current_video_frame + gf_group->arf_src_offset[gf_group->index];
    ref_buffer->frame_coding_index = cm->current_frame_coding_index;
  }
}

static void set_mb_ssim_rdmult_scaling(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  ThreadData *td = &cpi->td;
  MACROBLOCK *x = &td->mb;
  MACROBLOCKD *xd = &x->e_mbd;
  uint8_t *y_buffer = cpi->Source->y_buffer;
  const int y_stride = cpi->Source->y_stride;
  const int block_size = BLOCK_16X16;

  const int num_8x8_w = num_8x8_blocks_wide_lookup[block_size];
  const int num_8x8_h = num_8x8_blocks_high_lookup[block_size];
  const int num_cols = (cm->mi_cols + num_8x8_w - 1) / num_8x8_w;
  const int num_rows = (cm->mi_rows + num_8x8_h - 1) / num_8x8_h;
  double log_sum = 0.0;
  int row, col;

  // Loop through each 64x64 block.
  for (row = 0; row < num_rows; ++row) {
    for (col = 0; col < num_cols; ++col) {
      int mi_row, mi_col;
      double var = 0.0, num_of_var = 0.0;
      const int index = row * num_cols + col;

      for (mi_row = row * num_8x8_h;
           mi_row < cm->mi_rows && mi_row < (row + 1) * num_8x8_h; ++mi_row) {
        for (mi_col = col * num_8x8_w;
             mi_col < cm->mi_cols && mi_col < (col + 1) * num_8x8_w; ++mi_col) {
          struct buf_2d buf;
          const int row_offset_y = mi_row << 3;
          const int col_offset_y = mi_col << 3;

          buf.buf = y_buffer + row_offset_y * y_stride + col_offset_y;
          buf.stride = y_stride;

          // In order to make SSIM_VAR_SCALE in a same scale for both 8 bit
          // and high bit videos, the variance needs to be divided by 2.0 or
          // 64.0 separately.
          // TODO(sdeng): need to tune for 12bit videos.
#if CONFIG_VP9_HIGHBITDEPTH
          if (cpi->Source->flags & YV12_FLAG_HIGHBITDEPTH)
            var += vp9_high_get_sby_variance(cpi, &buf, BLOCK_8X8, xd->bd);
          else
#endif
            var += vp9_get_sby_variance(cpi, &buf, BLOCK_8X8);

          num_of_var += 1.0;
        }
      }
      var = var / num_of_var / 64.0;

      // Curve fitting with an exponential model on all 16x16 blocks from the
      // Midres dataset.
      var = 67.035434 * (1 - exp(-0.0021489 * var)) + 17.492222;
      cpi->mi_ssim_rdmult_scaling_factors[index] = var;
      log_sum += log(var);
    }
  }
  log_sum = exp(log_sum / (double)(num_rows * num_cols));

  for (row = 0; row < num_rows; ++row) {
    for (col = 0; col < num_cols; ++col) {
      const int index = row * num_cols + col;
      cpi->mi_ssim_rdmult_scaling_factors[index] /= log_sum;
    }
  }

  (void)xd;
}

// Process the wiener variance in 16x16 block basis.
static int qsort_comp(const void *elem1, const void *elem2) {
  int a = *((const int *)elem1);
  int b = *((const int *)elem2);
  if (a > b) return 1;
  if (a < b) return -1;
  return 0;
}

static void init_mb_wiener_var_buffer(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;

  if (cpi->mb_wiener_variance && cpi->mb_wiener_var_rows >= cm->mb_rows &&
      cpi->mb_wiener_var_cols >= cm->mb_cols)
    return;

  vpx_free(cpi->mb_wiener_variance);
  cpi->mb_wiener_variance = NULL;

  CHECK_MEM_ERROR(
      cm, cpi->mb_wiener_variance,
      vpx_calloc(cm->mb_rows * cm->mb_cols, sizeof(*cpi->mb_wiener_variance)));
  cpi->mb_wiener_var_rows = cm->mb_rows;
  cpi->mb_wiener_var_cols = cm->mb_cols;
}

static void set_mb_wiener_variance(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  uint8_t *buffer = cpi->Source->y_buffer;
  int buf_stride = cpi->Source->y_stride;

#if CONFIG_VP9_HIGHBITDEPTH
  ThreadData *td = &cpi->td;
  MACROBLOCK *x = &td->mb;
  MACROBLOCKD *xd = &x->e_mbd;
  DECLARE_ALIGNED(16, uint16_t, zero_pred16[32 * 32]);
  DECLARE_ALIGNED(16, uint8_t, zero_pred8[32 * 32]);
  uint8_t *zero_pred;
#else
  DECLARE_ALIGNED(16, uint8_t, zero_pred[32 * 32]);
#endif

  DECLARE_ALIGNED(16, int16_t, src_diff[32 * 32]);
  DECLARE_ALIGNED(16, tran_low_t, coeff[32 * 32]);

  int mb_row, mb_col, count = 0;
  // Hard coded operating block size
  const int block_size = 16;
  const int coeff_count = block_size * block_size;
  const TX_SIZE tx_size = TX_16X16;

#if CONFIG_VP9_HIGHBITDEPTH
  xd->cur_buf = cpi->Source;
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    zero_pred = CONVERT_TO_BYTEPTR(zero_pred16);
    memset(zero_pred16, 0, sizeof(*zero_pred16) * coeff_count);
  } else {
    zero_pred = zero_pred8;
    memset(zero_pred8, 0, sizeof(*zero_pred8) * coeff_count);
  }
#else
  memset(zero_pred, 0, sizeof(*zero_pred) * coeff_count);
#endif

  cpi->norm_wiener_variance = 0;

  for (mb_row = 0; mb_row < cm->mb_rows; ++mb_row) {
    for (mb_col = 0; mb_col < cm->mb_cols; ++mb_col) {
      int idx;
      int16_t median_val = 0;
      uint8_t *mb_buffer =
          buffer + mb_row * block_size * buf_stride + mb_col * block_size;
      int64_t wiener_variance = 0;

#if CONFIG_VP9_HIGHBITDEPTH
      if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
        vpx_highbd_subtract_block(block_size, block_size, src_diff, block_size,
                                  mb_buffer, buf_stride, zero_pred, block_size,
                                  xd->bd);
        highbd_wht_fwd_txfm(src_diff, block_size, coeff, tx_size);
      } else {
        vpx_subtract_block(block_size, block_size, src_diff, block_size,
                           mb_buffer, buf_stride, zero_pred, block_size);
        wht_fwd_txfm(src_diff, block_size, coeff, tx_size);
      }
#else
      vpx_subtract_block(block_size, block_size, src_diff, block_size,
                         mb_buffer, buf_stride, zero_pred, block_size);
      wht_fwd_txfm(src_diff, block_size, coeff, tx_size);
#endif  // CONFIG_VP9_HIGHBITDEPTH

      coeff[0] = 0;
      for (idx = 1; idx < coeff_count; ++idx) coeff[idx] = abs(coeff[idx]);

      qsort(coeff, coeff_count - 1, sizeof(*coeff), qsort_comp);

      // Noise level estimation
      median_val = coeff[coeff_count / 2];

      // Wiener filter
      for (idx = 1; idx < coeff_count; ++idx) {
        int64_t sqr_coeff = (int64_t)coeff[idx] * coeff[idx];
        int64_t tmp_coeff = (int64_t)coeff[idx];
        if (median_val) {
          tmp_coeff = (sqr_coeff * coeff[idx]) /
                      (sqr_coeff + (int64_t)median_val * median_val);
        }
        wiener_variance += tmp_coeff * tmp_coeff;
      }
      cpi->mb_wiener_variance[mb_row * cm->mb_cols + mb_col] =
          wiener_variance / coeff_count;
      cpi->norm_wiener_variance +=
          cpi->mb_wiener_variance[mb_row * cm->mb_cols + mb_col];
      ++count;
    }
  }

  if (count) cpi->norm_wiener_variance /= count;
  cpi->norm_wiener_variance = VPXMAX(1, cpi->norm_wiener_variance);
}

#if !CONFIG_REALTIME_ONLY
static void update_encode_frame_result(
    int ref_frame_flags, FRAME_UPDATE_TYPE update_type,
    const YV12_BUFFER_CONFIG *source_frame, const RefCntBuffer *coded_frame_buf,
    RefCntBuffer *ref_frame_buf[MAX_INTER_REF_FRAMES], int quantize_index,
    uint32_t bit_depth, uint32_t input_bit_depth, const FRAME_COUNTS *counts,
#if CONFIG_RATE_CTRL
    const PARTITION_INFO *partition_info,
    const MOTION_VECTOR_INFO *motion_vector_info,
    const TplDepStats *tpl_stats_info,
#endif  // CONFIG_RATE_CTRL
    ENCODE_FRAME_RESULT *encode_frame_result);
#endif  // !CONFIG_REALTIME_ONLY

static void encode_frame_to_data_rate(
    VP9_COMP *cpi, size_t *size, uint8_t *dest, unsigned int *frame_flags,
    ENCODE_FRAME_RESULT *encode_frame_result) {
  VP9_COMMON *const cm = &cpi->common;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  struct segmentation *const seg = &cm->seg;
  TX_SIZE t;

  // SVC: skip encoding of enhancement layer if the layer target bandwidth = 0.
  // No need to set svc.skip_enhancement_layer if whole superframe will be
  // dropped.
  if (cpi->use_svc && cpi->svc.spatial_layer_id > 0 &&
      cpi->oxcf.target_bandwidth == 0 &&
      !(cpi->svc.framedrop_mode != LAYER_DROP &&
        (cpi->svc.framedrop_mode != CONSTRAINED_FROM_ABOVE_DROP ||
         cpi->svc
             .force_drop_constrained_from_above[cpi->svc.number_spatial_layers -
                                                1]) &&
        cpi->svc.drop_spatial_layer[0])) {
    cpi->svc.skip_enhancement_layer = 1;
    vp9_rc_postencode_update_drop_frame(cpi);
    cpi->ext_refresh_frame_flags_pending = 0;
    cpi->last_frame_dropped = 1;
    cpi->svc.last_layer_dropped[cpi->svc.spatial_layer_id] = 1;
    cpi->svc.drop_spatial_layer[cpi->svc.spatial_layer_id] = 1;
    vp9_inc_frame_in_layer(cpi);
    return;
  }

  set_ext_overrides(cpi);
  vpx_clear_system_state();

#ifdef ENABLE_KF_DENOISE
  // Spatial denoise of key frame.
  if (is_spatial_denoise_enabled(cpi)) spatial_denoise_frame(cpi);
#endif

  if (cm->show_existing_frame == 0) {
    // Update frame index
    set_frame_index(cpi, cm);

    // Set the arf sign bias for this frame.
    set_ref_sign_bias(cpi);
  }

  // Set default state for segment based loop filter update flags.
  cm->lf.mode_ref_delta_update = 0;

  if (cpi->oxcf.pass == 2 && cpi->sf.adaptive_interp_filter_search)
    cpi->sf.interp_filter_search_mask = setup_interp_filter_search_mask(cpi);

  // Set various flags etc to special state if it is a key frame.
  if (frame_is_intra_only(cm)) {
    // Reset the loop filter deltas and segmentation map.
    vp9_reset_segment_features(&cm->seg);

    // If segmentation is enabled force a map update for key frames.
    if (seg->enabled) {
      seg->update_map = 1;
      seg->update_data = 1;
    }

    // The alternate reference frame cannot be active for a key frame.
    cpi->rc.source_alt_ref_active = 0;

    cm->error_resilient_mode = oxcf->error_resilient_mode;
    cm->frame_parallel_decoding_mode = oxcf->frame_parallel_decoding_mode;

    // By default, encoder assumes decoder can use prev_mi.
    if (cm->error_resilient_mode) {
      cm->frame_parallel_decoding_mode = 1;
      cm->reset_frame_context = 0;
      cm->refresh_frame_context = 0;
    } else if (cm->intra_only) {
      // Only reset the current context.
      cm->reset_frame_context = 2;
    }
  }

  if (oxcf->tuning == VP8_TUNE_SSIM) set_mb_ssim_rdmult_scaling(cpi);

  if (oxcf->aq_mode == PERCEPTUAL_AQ) {
    init_mb_wiener_var_buffer(cpi);
    set_mb_wiener_variance(cpi);
  }

  vpx_clear_system_state();

#if CONFIG_INTERNAL_STATS
  memset(cpi->mode_chosen_counts, 0,
         MAX_MODES * sizeof(*cpi->mode_chosen_counts));
#endif
#if CONFIG_CONSISTENT_RECODE || CONFIG_RATE_CTRL
  // Backup to ensure consistency between recodes
  save_encode_params(cpi);
#endif  // CONFIG_CONSISTENT_RECODE || CONFIG_RATE_CTRL

  if (cpi->sf.recode_loop == DISALLOW_RECODE) {
    if (!encode_without_recode_loop(cpi, size, dest)) return;
  } else {
#if !CONFIG_REALTIME_ONLY
#if CONFIG_RATE_CTRL
    encode_with_recode_loop(cpi, size, dest, &encode_frame_result->rq_history);
#else   // CONFIG_RATE_CTRL
    encode_with_recode_loop(cpi, size, dest);
#endif  // CONFIG_RATE_CTRL
#endif  // !CONFIG_REALTIME_ONLY
  }

  // TODO(jingning): When using show existing frame mode, we assume that the
  // current ARF will be directly used as the final reconstructed frame. This is
  // an encoder control scheme. One could in principle explore other
  // possibilities to arrange the reference frame buffer and their coding order.
  if (cm->show_existing_frame) {
    ref_cnt_fb(cm->buffer_pool->frame_bufs, &cm->new_fb_idx,
               cm->ref_frame_map[cpi->alt_fb_idx]);
  }

#if !CONFIG_REALTIME_ONLY
  // Disable segmentation if it decrease rate/distortion ratio
  if (cpi->oxcf.aq_mode == LOOKAHEAD_AQ)
    vp9_try_disable_lookahead_aq(cpi, size, dest);
#endif

#if CONFIG_VP9_TEMPORAL_DENOISING
#ifdef OUTPUT_YUV_DENOISED
  if (oxcf->noise_sensitivity > 0 && denoise_svc(cpi)) {
    vpx_write_yuv_frame(yuv_denoised_file,
                        &cpi->denoiser.running_avg_y[INTRA_FRAME]);
  }
#endif
#endif
#ifdef OUTPUT_YUV_SKINMAP
  if (cpi->common.current_video_frame > 1) {
    vp9_output_skin_map(cpi, yuv_skinmap_file);
  }
#endif

  // Special case code to reduce pulsing when key frames are forced at a
  // fixed interval. Note the reconstruction error if it is the frame before
  // the force key frame
  if (cpi->rc.next_key_frame_forced && cpi->rc.frames_to_key == 1) {
#if CONFIG_VP9_HIGHBITDEPTH
    if (cm->use_highbitdepth) {
      cpi->ambient_err =
          vpx_highbd_get_y_sse(cpi->Source, get_frame_new_buffer(cm));
    } else {
      cpi->ambient_err = vpx_get_y_sse(cpi->Source, get_frame_new_buffer(cm));
    }
#else
    cpi->ambient_err = vpx_get_y_sse(cpi->Source, get_frame_new_buffer(cm));
#endif  // CONFIG_VP9_HIGHBITDEPTH
  }

  // If the encoder forced a KEY_FRAME decision
  if (cm->frame_type == KEY_FRAME) cpi->refresh_last_frame = 1;

  cm->frame_to_show = get_frame_new_buffer(cm);
  cm->frame_to_show->color_space = cm->color_space;
  cm->frame_to_show->color_range = cm->color_range;
  cm->frame_to_show->render_width = cm->render_width;
  cm->frame_to_show->render_height = cm->render_height;

  // Pick the loop filter level for the frame.
  loopfilter_frame(cpi, cm);

  if (cpi->rc.use_post_encode_drop) save_coding_context(cpi);

  // build the bitstream
  vp9_pack_bitstream(cpi, dest, size);

  {
    const RefCntBuffer *coded_frame_buf =
        get_ref_cnt_buffer(cm, cm->new_fb_idx);
    vpx_codec_err_t codec_status = vp9_extrc_update_encodeframe_result(
        &cpi->ext_ratectrl, (*size) << 3, cpi->Source, &coded_frame_buf->buf,
        cm->bit_depth, cpi->oxcf.input_bit_depth);
    if (codec_status != VPX_CODEC_OK) {
      vpx_internal_error(&cm->error, codec_status,
                         "vp9_extrc_update_encodeframe_result() failed");
    }
  }
#if CONFIG_REALTIME_ONLY
  (void)encode_frame_result;
  assert(encode_frame_result == NULL);
#else  // CONFIG_REALTIME_ONLY
  if (encode_frame_result != NULL) {
    const int ref_frame_flags = get_ref_frame_flags(cpi);
    const RefCntBuffer *coded_frame_buf =
        get_ref_cnt_buffer(cm, cm->new_fb_idx);
    RefCntBuffer *ref_frame_bufs[MAX_INTER_REF_FRAMES];
    get_ref_frame_bufs(cpi, ref_frame_bufs);
    // update_encode_frame_result() depends on twopass.gf_group.index and
    // cm->new_fb_idx, cpi->Source, cpi->lst_fb_idx, cpi->gld_fb_idx and
    // cpi->alt_fb_idx are updated for current frame and have
    // not been updated for the next frame yet.
    // The update locations are as follows.
    // 1) twopass.gf_group.index is initialized at define_gf_group by vp9_zero()
    // for the first frame in the gf_group and is updated for the next frame at
    // vp9_twopass_postencode_update().
    // 2) cpi->Source is updated at the beginning of vp9_get_compressed_data()
    // 3) cm->new_fb_idx is updated at the beginning of
    // vp9_get_compressed_data() by get_free_fb(cm).
    // 4) cpi->lst_fb_idx/gld_fb_idx/alt_fb_idx will be updated for the next
    // frame at vp9_update_reference_frames().
    // This function needs to be called before vp9_update_reference_frames().
    // TODO(angiebird): Improve the codebase to make the update of frame
    // dependent variables more robust.
    update_encode_frame_result(
        ref_frame_flags,
        cpi->twopass.gf_group.update_type[cpi->twopass.gf_group.index],
        cpi->Source, coded_frame_buf, ref_frame_bufs, vp9_get_quantizer(cpi),
        cm->bit_depth, cpi->oxcf.input_bit_depth, cpi->td.counts,
#if CONFIG_RATE_CTRL
        cpi->partition_info, cpi->motion_vector_info, cpi->tpl_stats_info,
#endif  // CONFIG_RATE_CTRL
        encode_frame_result);
  }
#endif  // CONFIG_REALTIME_ONLY

  if (cpi->rc.use_post_encode_drop && cm->base_qindex < cpi->rc.worst_quality &&
      cpi->svc.spatial_layer_id == 0 && post_encode_drop_cbr(cpi, size)) {
    restore_coding_context(cpi);
    return;
  }

  cpi->last_frame_dropped = 0;
  cpi->svc.last_layer_dropped[cpi->svc.spatial_layer_id] = 0;
  if (cpi->svc.spatial_layer_id == cpi->svc.number_spatial_layers - 1)
    cpi->svc.num_encoded_top_layer++;

  // Keep track of the frame buffer index updated/refreshed for the
  // current encoded TL0 superframe.
  if (cpi->svc.temporal_layer_id == 0) {
    if (cpi->refresh_last_frame)
      cpi->svc.fb_idx_upd_tl0[cpi->svc.spatial_layer_id] = cpi->lst_fb_idx;
    else if (cpi->refresh_golden_frame)
      cpi->svc.fb_idx_upd_tl0[cpi->svc.spatial_layer_id] = cpi->gld_fb_idx;
    else if (cpi->refresh_alt_ref_frame)
      cpi->svc.fb_idx_upd_tl0[cpi->svc.spatial_layer_id] = cpi->alt_fb_idx;
  }

  if (cm->seg.update_map) update_reference_segmentation_map(cpi);

  if (frame_is_intra_only(cm) == 0) {
    release_scaled_references(cpi);
  }
  vp9_update_reference_frames(cpi);

  if (!cm->show_existing_frame) {
    for (t = TX_4X4; t <= TX_32X32; ++t) {
      full_to_model_counts(cpi->td.counts->coef[t],
                           cpi->td.rd_counts.coef_counts[t]);
    }

    if (!cm->error_resilient_mode && !cm->frame_parallel_decoding_mode) {
      if (!frame_is_intra_only(cm)) {
        vp9_adapt_mode_probs(cm);
        vp9_adapt_mv_probs(cm, cm->allow_high_precision_mv);
      }
      vp9_adapt_coef_probs(cm);
    }
  }

  cpi->ext_refresh_frame_flags_pending = 0;

  if (cpi->refresh_golden_frame == 1)
    cpi->frame_flags |= FRAMEFLAGS_GOLDEN;
  else
    cpi->frame_flags &= ~FRAMEFLAGS_GOLDEN;

  if (cpi->refresh_alt_ref_frame == 1)
    cpi->frame_flags |= FRAMEFLAGS_ALTREF;
  else
    cpi->frame_flags &= ~FRAMEFLAGS_ALTREF;

  cpi->ref_frame_flags = get_ref_frame_flags(cpi);

  cm->last_frame_type = cm->frame_type;

  vp9_rc_postencode_update(cpi, *size);

  if (oxcf->pass == 0 && !frame_is_intra_only(cm) &&
      (!cpi->use_svc ||
       (cpi->use_svc &&
        !cpi->svc.layer_context[cpi->svc.temporal_layer_id].is_key_frame &&
        cpi->svc.spatial_layer_id == cpi->svc.number_spatial_layers - 1))) {
    vp9_compute_frame_low_motion(cpi);
  }

  *size = VPXMAX(1, *size);

#if 0
  output_frame_level_debug_stats(cpi);
#endif

  if (cm->frame_type == KEY_FRAME) {
    // Tell the caller that the frame was coded as a key frame
    *frame_flags = cpi->frame_flags | FRAMEFLAGS_KEY;
  } else {
    *frame_flags = cpi->frame_flags & ~FRAMEFLAGS_KEY;
  }

  // Clear the one shot update flags for segmentation map and mode/ref loop
  // filter deltas.
  cm->seg.update_map = 0;
  cm->seg.update_data = 0;
  cm->lf.mode_ref_delta_update = 0;

  // keep track of the last coded dimensions
  cm->last_width = cm->width;
  cm->last_height = cm->height;

  // reset to normal state now that we are done.
  if (!cm->show_existing_frame) {
    cm->last_show_frame = cm->show_frame;
    cm->prev_frame = cm->cur_frame;
  }

  if (cm->show_frame) {
    vp9_swap_mi_and_prev_mi(cm);
    if (cpi->use_svc) vp9_inc_frame_in_layer(cpi);
  }
  update_frame_indexes(cm, cm->show_frame);

  if (cpi->use_svc) {
    cpi->svc
        .layer_context[cpi->svc.spatial_layer_id *
                           cpi->svc.number_temporal_layers +
                       cpi->svc.temporal_layer_id]
        .last_frame_type = cm->frame_type;
    // Reset layer_sync back to 0 for next frame.
    cpi->svc.spatial_layer_sync[cpi->svc.spatial_layer_id] = 0;
  }

  cpi->force_update_segmentation = 0;

#if !CONFIG_REALTIME_ONLY
  if (cpi->oxcf.aq_mode == LOOKAHEAD_AQ)
    vp9_alt_ref_aq_unset_all(cpi->alt_ref_aq, cpi);
#endif

  cpi->svc.previous_frame_is_intra_only = cm->intra_only;
  cpi->svc.set_intra_only_frame = 0;
}

static void SvcEncode(VP9_COMP *cpi, size_t *size, uint8_t *dest,
                      unsigned int *frame_flags) {
  vp9_rc_get_svc_params(cpi);
  encode_frame_to_data_rate(cpi, size, dest, frame_flags,
                            /*encode_frame_result = */ NULL);
}

static void Pass0Encode(VP9_COMP *cpi, size_t *size, uint8_t *dest,
                        unsigned int *frame_flags) {
  if (cpi->oxcf.rc_mode == VPX_CBR) {
    vp9_rc_get_one_pass_cbr_params(cpi);
  } else {
    vp9_rc_get_one_pass_vbr_params(cpi);
  }
  encode_frame_to_data_rate(cpi, size, dest, frame_flags,
                            /*encode_frame_result = */ NULL);
}

#if !CONFIG_REALTIME_ONLY
static void Pass2Encode(VP9_COMP *cpi, size_t *size, uint8_t *dest,
                        unsigned int *frame_flags,
                        ENCODE_FRAME_RESULT *encode_frame_result) {
  cpi->allow_encode_breakout = ENCODE_BREAKOUT_ENABLED;

  if (cpi->common.current_frame_coding_index == 0) {
    VP9_COMMON *cm = &cpi->common;
    const vpx_codec_err_t codec_status = vp9_extrc_send_firstpass_stats(
        &cpi->ext_ratectrl, &cpi->twopass.first_pass_info);
    if (codec_status != VPX_CODEC_OK) {
      vpx_internal_error(&cm->error, codec_status,
                         "vp9_extrc_send_firstpass_stats() failed");
    }
  }
#if CONFIG_MISMATCH_DEBUG
  mismatch_move_frame_idx_w();
#endif
  encode_frame_to_data_rate(cpi, size, dest, frame_flags, encode_frame_result);
}
#endif  // !CONFIG_REALTIME_ONLY

int vp9_receive_raw_frame(VP9_COMP *cpi, vpx_enc_frame_flags_t frame_flags,
                          YV12_BUFFER_CONFIG *sd, int64_t time_stamp,
                          int64_t end_time) {
  VP9_COMMON *const cm = &cpi->common;
  struct vpx_usec_timer timer;
  int res = 0;
  const int subsampling_x = sd->subsampling_x;
  const int subsampling_y = sd->subsampling_y;
#if CONFIG_VP9_HIGHBITDEPTH
  const int use_highbitdepth = (sd->flags & YV12_FLAG_HIGHBITDEPTH) != 0;
#else
  const int use_highbitdepth = 0;
#endif

  update_initial_width(cpi, use_highbitdepth, subsampling_x, subsampling_y);
#if CONFIG_VP9_TEMPORAL_DENOISING
  setup_denoiser_buffer(cpi);
#endif

  alloc_raw_frame_buffers(cpi);

  vpx_usec_timer_start(&timer);

  if (vp9_lookahead_push(cpi->lookahead, sd, time_stamp, end_time,
                         use_highbitdepth, frame_flags))
    res = -1;
  vpx_usec_timer_mark(&timer);
  cpi->time_receive_data += vpx_usec_timer_elapsed(&timer);

  if ((cm->profile == PROFILE_0 || cm->profile == PROFILE_2) &&
      (subsampling_x != 1 || subsampling_y != 1)) {
    vpx_internal_error(&cm->error, VPX_CODEC_INVALID_PARAM,
                       "Non-4:2:0 color format requires profile 1 or 3");
    res = -1;
  }
  if ((cm->profile == PROFILE_1 || cm->profile == PROFILE_3) &&
      (subsampling_x == 1 && subsampling_y == 1)) {
    vpx_internal_error(&cm->error, VPX_CODEC_INVALID_PARAM,
                       "4:2:0 color format requires profile 0 or 2");
    res = -1;
  }

  return res;
}

static int frame_is_reference(const VP9_COMP *cpi) {
  const VP9_COMMON *cm = &cpi->common;

  return cm->frame_type == KEY_FRAME || cpi->refresh_last_frame ||
         cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame ||
         cm->refresh_frame_context || cm->lf.mode_ref_delta_update ||
         cm->seg.update_map || cm->seg.update_data;
}

static void adjust_frame_rate(VP9_COMP *cpi,
                              const struct lookahead_entry *source) {
  int64_t this_duration;
  int step = 0;

  if (source->ts_start == cpi->first_time_stamp_ever) {
    this_duration = source->ts_end - source->ts_start;
    step = 1;
  } else {
    int64_t last_duration =
        cpi->last_end_time_stamp_seen - cpi->last_time_stamp_seen;

    this_duration = source->ts_end - cpi->last_end_time_stamp_seen;

    // do a step update if the duration changes by 10%
    if (last_duration)
      step = (int)((this_duration - last_duration) * 10 / last_duration);
  }

  if (this_duration) {
    if (step) {
      vp9_new_framerate(cpi, 10000000.0 / this_duration);
    } else {
      // Average this frame's rate into the last second's average
      // frame rate. If we haven't seen 1 second yet, then average
      // over the whole interval seen.
      const double interval = VPXMIN(
          (double)(source->ts_end - cpi->first_time_stamp_ever), 10000000.0);
      double avg_duration = 10000000.0 / cpi->framerate;
      avg_duration *= (interval - avg_duration + this_duration);
      avg_duration /= interval;

      vp9_new_framerate(cpi, 10000000.0 / avg_duration);
    }
  }
  cpi->last_time_stamp_seen = source->ts_start;
  cpi->last_end_time_stamp_seen = source->ts_end;
}

// Returns 0 if this is not an alt ref else the offset of the source frame
// used as the arf midpoint.
static int get_arf_src_index(VP9_COMP *cpi) {
  RATE_CONTROL *const rc = &cpi->rc;
  int arf_src_index = 0;
  if (is_altref_enabled(cpi)) {
    if (cpi->oxcf.pass == 2) {
      const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
      if (gf_group->update_type[gf_group->index] == ARF_UPDATE) {
        arf_src_index = gf_group->arf_src_offset[gf_group->index];
      }
    } else if (rc->source_alt_ref_pending) {
      arf_src_index = rc->frames_till_gf_update_due;
    }
  }
  return arf_src_index;
}

static void check_src_altref(VP9_COMP *cpi,
                             const struct lookahead_entry *source) {
  RATE_CONTROL *const rc = &cpi->rc;

  if (cpi->oxcf.pass == 2) {
    const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
    rc->is_src_frame_alt_ref =
        (gf_group->update_type[gf_group->index] == OVERLAY_UPDATE);
  } else {
    rc->is_src_frame_alt_ref =
        cpi->alt_ref_source && (source == cpi->alt_ref_source);
  }

  if (rc->is_src_frame_alt_ref) {
    // Current frame is an ARF overlay frame.
    cpi->alt_ref_source = NULL;

    // Don't refresh the last buffer for an ARF overlay frame. It will
    // become the GF so preserve last as an alternative prediction option.
    cpi->refresh_last_frame = 0;
  }
}

#if CONFIG_INTERNAL_STATS
static void adjust_image_stat(double y, double u, double v, double all,
                              ImageStat *s) {
  s->stat[Y] += y;
  s->stat[U] += u;
  s->stat[V] += v;
  s->stat[ALL] += all;
  s->worst = VPXMIN(s->worst, all);
}
#endif  // CONFIG_INTERNAL_STATS

// Adjust the maximum allowable frame size for the target level.
static void level_rc_framerate(VP9_COMP *cpi, int arf_src_index) {
  RATE_CONTROL *const rc = &cpi->rc;
  LevelConstraint *const ls = &cpi->level_constraint;
  VP9_COMMON *const cm = &cpi->common;
  const double max_cpb_size = ls->max_cpb_size;
  vpx_clear_system_state();
  rc->max_frame_bandwidth = VPXMIN(rc->max_frame_bandwidth, ls->max_frame_size);
  if (frame_is_intra_only(cm)) {
    rc->max_frame_bandwidth =
        VPXMIN(rc->max_frame_bandwidth, (int)(max_cpb_size * 0.5));
  } else if (arf_src_index > 0) {
    rc->max_frame_bandwidth =
        VPXMIN(rc->max_frame_bandwidth, (int)(max_cpb_size * 0.4));
  } else {
    rc->max_frame_bandwidth =
        VPXMIN(rc->max_frame_bandwidth, (int)(max_cpb_size * 0.2));
  }
}

static void update_level_info(VP9_COMP *cpi, size_t *size, int arf_src_index) {
  VP9_COMMON *const cm = &cpi->common;
  Vp9LevelInfo *const level_info = &cpi->level_info;
  Vp9LevelSpec *const level_spec = &level_info->level_spec;
  Vp9LevelStats *const level_stats = &level_info->level_stats;
  int i, idx;
  uint64_t luma_samples, dur_end;
  const uint32_t luma_pic_size = cm->width * cm->height;
  const uint32_t luma_pic_breadth = VPXMAX(cm->width, cm->height);
  LevelConstraint *const level_constraint = &cpi->level_constraint;
  const int8_t level_index = level_constraint->level_index;
  double cpb_data_size;

  vpx_clear_system_state();

  // update level_stats
  level_stats->total_compressed_size += *size;
  if (cm->show_frame) {
    level_stats->total_uncompressed_size +=
        luma_pic_size +
        2 * (luma_pic_size >> (cm->subsampling_x + cm->subsampling_y));
    level_stats->time_encoded =
        (cpi->last_end_time_stamp_seen - cpi->first_time_stamp_ever) /
        (double)TICKS_PER_SEC;
  }

  if (arf_src_index > 0) {
    if (!level_stats->seen_first_altref) {
      level_stats->seen_first_altref = 1;
    } else if (level_stats->frames_since_last_altref <
               level_spec->min_altref_distance) {
      level_spec->min_altref_distance = level_stats->frames_since_last_altref;
    }
    level_stats->frames_since_last_altref = 0;
  } else {
    ++level_stats->frames_since_last_altref;
  }

  if (level_stats->frame_window_buffer.len < FRAME_WINDOW_SIZE - 1) {
    idx = (level_stats->frame_window_buffer.start +
           level_stats->frame_window_buffer.len++) %
          FRAME_WINDOW_SIZE;
  } else {
    idx = level_stats->frame_window_buffer.start;
    level_stats->frame_window_buffer.start = (idx + 1) % FRAME_WINDOW_SIZE;
  }
  level_stats->frame_window_buffer.buf[idx].ts = cpi->last_time_stamp_seen;
  level_stats->frame_window_buffer.buf[idx].size = (uint32_t)(*size);
  level_stats->frame_window_buffer.buf[idx].luma_samples = luma_pic_size;

  if (cm->frame_type == KEY_FRAME) {
    level_stats->ref_refresh_map = 0;
  } else {
    int count = 0;
    level_stats->ref_refresh_map |= vp9_get_refresh_mask(cpi);
    // Also need to consider the case where the encoder refers to a buffer
    // that has been implicitly refreshed after encoding a keyframe.
    if (!cm->intra_only) {
      level_stats->ref_refresh_map |= (1 << cpi->lst_fb_idx);
      level_stats->ref_refresh_map |= (1 << cpi->gld_fb_idx);
      level_stats->ref_refresh_map |= (1 << cpi->alt_fb_idx);
    }
    for (i = 0; i < REF_FRAMES; ++i) {
      count += (level_stats->ref_refresh_map >> i) & 1;
    }
    if (count > level_spec->max_ref_frame_buffers) {
      level_spec->max_ref_frame_buffers = count;
    }
  }

  // update average_bitrate
  level_spec->average_bitrate = (double)level_stats->total_compressed_size /
                                125.0 / level_stats->time_encoded;

  // update max_luma_sample_rate
  luma_samples = 0;
  for (i = 0; i < level_stats->frame_window_buffer.len; ++i) {
    idx = (level_stats->frame_window_buffer.start +
           level_stats->frame_window_buffer.len - 1 - i) %
          FRAME_WINDOW_SIZE;
    if (i == 0) {
      dur_end = level_stats->frame_window_buffer.buf[idx].ts;
    }
    if (dur_end - level_stats->frame_window_buffer.buf[idx].ts >=
        TICKS_PER_SEC) {
      break;
    }
    luma_samples += level_stats->frame_window_buffer.buf[idx].luma_samples;
  }
  if (luma_samples > level_spec->max_luma_sample_rate) {
    level_spec->max_luma_sample_rate = luma_samples;
  }

  // update max_cpb_size
  cpb_data_size = 0;
  for (i = 0; i < CPB_WINDOW_SIZE; ++i) {
    if (i >= level_stats->frame_window_buffer.len) break;
    idx = (level_stats->frame_window_buffer.start +
           level_stats->frame_window_buffer.len - 1 - i) %
          FRAME_WINDOW_SIZE;
    cpb_data_size += level_stats->frame_window_buffer.buf[idx].size;
  }
  cpb_data_size = cpb_data_size / 125.0;
  if (cpb_data_size > level_spec->max_cpb_size) {
    level_spec->max_cpb_size = cpb_data_size;
  }

  // update max_luma_picture_size
  if (luma_pic_size > level_spec->max_luma_picture_size) {
    level_spec->max_luma_picture_size = luma_pic_size;
  }

  // update max_luma_picture_breadth
  if (luma_pic_breadth > level_spec->max_luma_picture_breadth) {
    level_spec->max_luma_picture_breadth = luma_pic_breadth;
  }

  // update compression_ratio
  level_spec->compression_ratio = (double)level_stats->total_uncompressed_size *
                                  cm->bit_depth /
                                  level_stats->total_compressed_size / 8.0;

  // update max_col_tiles
  if (level_spec->max_col_tiles < (1 << cm->log2_tile_cols)) {
    level_spec->max_col_tiles = (1 << cm->log2_tile_cols);
  }

  if (level_index >= 0 && level_constraint->fail_flag == 0) {
    if (level_spec->max_luma_picture_size >
        vp9_level_defs[level_index].max_luma_picture_size) {
      level_constraint->fail_flag |= (1 << LUMA_PIC_SIZE_TOO_LARGE);
      vpx_internal_error(&cm->error, VPX_CODEC_ERROR,
                         "Failed to encode to the target level %d. %s",
                         vp9_level_defs[level_index].level,
                         level_fail_messages[LUMA_PIC_SIZE_TOO_LARGE]);
    }

    if (level_spec->max_luma_picture_breadth >
        vp9_level_defs[level_index].max_luma_picture_breadth) {
      level_constraint->fail_flag |= (1 << LUMA_PIC_BREADTH_TOO_LARGE);
      vpx_internal_error(&cm->error, VPX_CODEC_ERROR,
                         "Failed to encode to the target level %d. %s",
                         vp9_level_defs[level_index].level,
                         level_fail_messages[LUMA_PIC_BREADTH_TOO_LARGE]);
    }

    if ((double)level_spec->max_luma_sample_rate >
        (double)vp9_level_defs[level_index].max_luma_sample_rate *
            (1 + SAMPLE_RATE_GRACE_P)) {
      level_constraint->fail_flag |= (1 << LUMA_SAMPLE_RATE_TOO_LARGE);
      vpx_internal_error(&cm->error, VPX_CODEC_ERROR,
                         "Failed to encode to the target level %d. %s",
                         vp9_level_defs[level_index].level,
                         level_fail_messages[LUMA_SAMPLE_RATE_TOO_LARGE]);
    }

    if (level_spec->max_col_tiles > vp9_level_defs[level_index].max_col_tiles) {
      level_constraint->fail_flag |= (1 << TOO_MANY_COLUMN_TILE);
      vpx_internal_error(&cm->error, VPX_CODEC_ERROR,
                         "Failed to encode to the target level %d. %s",
                         vp9_level_defs[level_index].level,
                         level_fail_messages[TOO_MANY_COLUMN_TILE]);
    }

    if (level_spec->min_altref_distance <
        vp9_level_defs[level_index].min_altref_distance) {
      level_constraint->fail_flag |= (1 << ALTREF_DIST_TOO_SMALL);
      vpx_internal_error(&cm->error, VPX_CODEC_ERROR,
                         "Failed to encode to the target level %d. %s",
                         vp9_level_defs[level_index].level,
                         level_fail_messages[ALTREF_DIST_TOO_SMALL]);
    }

    if (level_spec->max_ref_frame_buffers >
        vp9_level_defs[level_index].max_ref_frame_buffers) {
      level_constraint->fail_flag |= (1 << TOO_MANY_REF_BUFFER);
      vpx_internal_error(&cm->error, VPX_CODEC_ERROR,
                         "Failed to encode to the target level %d. %s",
                         vp9_level_defs[level_index].level,
                         level_fail_messages[TOO_MANY_REF_BUFFER]);
    }

    if (level_spec->max_cpb_size > vp9_level_defs[level_index].max_cpb_size) {
      level_constraint->fail_flag |= (1 << CPB_TOO_LARGE);
      vpx_internal_error(&cm->error, VPX_CODEC_ERROR,
                         "Failed to encode to the target level %d. %s",
                         vp9_level_defs[level_index].level,
                         level_fail_messages[CPB_TOO_LARGE]);
    }

    // Set an upper bound for the next frame size. It will be used in
    // level_rc_framerate() before encoding the next frame.
    cpb_data_size = 0;
    for (i = 0; i < CPB_WINDOW_SIZE - 1; ++i) {
      if (i >= level_stats->frame_window_buffer.len) break;
      idx = (level_stats->frame_window_buffer.start +
             level_stats->frame_window_buffer.len - 1 - i) %
            FRAME_WINDOW_SIZE;
      cpb_data_size += level_stats->frame_window_buffer.buf[idx].size;
    }
    cpb_data_size = cpb_data_size / 125.0;
    level_constraint->max_frame_size =
        (int)((vp9_level_defs[level_index].max_cpb_size - cpb_data_size) *
              1000.0);
    if (level_stats->frame_window_buffer.len < CPB_WINDOW_SIZE - 1)
      level_constraint->max_frame_size >>= 1;
  }
}

typedef struct GF_PICTURE {
  YV12_BUFFER_CONFIG *frame;
  int ref_frame[3];
  FRAME_UPDATE_TYPE update_type;
} GF_PICTURE;

static void init_gop_frames(VP9_COMP *cpi, GF_PICTURE *gf_picture,
                            const GF_GROUP *gf_group, int *tpl_group_frames) {
  VP9_COMMON *cm = &cpi->common;
  int frame_idx = 0;
  int i;
  int gld_index = -1;
  int alt_index = -1;
  int lst_index = -1;
  int arf_index_stack[MAX_ARF_LAYERS];
  int arf_stack_size = 0;
  int extend_frame_count = 0;
  int pframe_qindex = cpi->tpl_stats[2].base_qindex;
  int frame_gop_offset = 0;

  RefCntBuffer *frame_bufs = cm->buffer_pool->frame_bufs;
  int8_t recon_frame_index[REFS_PER_FRAME + MAX_ARF_LAYERS];

  memset(recon_frame_index, -1, sizeof(recon_frame_index));
  stack_init(arf_index_stack, MAX_ARF_LAYERS);

  // TODO(jingning): To be used later for gf frame type parsing.
  (void)gf_group;

  for (i = 0; i < FRAME_BUFFERS; ++i) {
    if (frame_bufs[i].ref_count == 0) {
      alloc_frame_mvs(cm, i);
      if (vpx_realloc_frame_buffer(&frame_bufs[i].buf, cm->width, cm->height,
                                   cm->subsampling_x, cm->subsampling_y,
#if CONFIG_VP9_HIGHBITDEPTH
                                   cm->use_highbitdepth,
#endif
                                   VP9_ENC_BORDER_IN_PIXELS, cm->byte_alignment,
                                   NULL, NULL, NULL))
        vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                           "Failed to allocate frame buffer");

      recon_frame_index[frame_idx] = i;
      ++frame_idx;

      if (frame_idx >= REFS_PER_FRAME + cpi->oxcf.enable_auto_arf) break;
    }
  }

  for (i = 0; i < REFS_PER_FRAME + 1; ++i) {
    assert(recon_frame_index[i] >= 0);
    cpi->tpl_recon_frames[i] = &frame_bufs[recon_frame_index[i]].buf;
  }

  *tpl_group_frames = 0;

  // Initialize Golden reference frame.
  gf_picture[0].frame = get_ref_frame_buffer(cpi, GOLDEN_FRAME);
  for (i = 0; i < 3; ++i) gf_picture[0].ref_frame[i] = -1;
  gf_picture[0].update_type = gf_group->update_type[0];
  gld_index = 0;
  ++*tpl_group_frames;

  // Initialize base layer ARF frame
  gf_picture[1].frame = cpi->Source;
  gf_picture[1].ref_frame[0] = gld_index;
  gf_picture[1].ref_frame[1] = lst_index;
  gf_picture[1].ref_frame[2] = alt_index;
  gf_picture[1].update_type = gf_group->update_type[1];
  alt_index = 1;
  ++*tpl_group_frames;

  // Initialize P frames
  for (frame_idx = 2; frame_idx < MAX_ARF_GOP_SIZE; ++frame_idx) {
    struct lookahead_entry *buf;
    frame_gop_offset = gf_group->frame_gop_index[frame_idx];
    buf = vp9_lookahead_peek(cpi->lookahead, frame_gop_offset - 1);

    if (buf == NULL) break;

    gf_picture[frame_idx].frame = &buf->img;
    gf_picture[frame_idx].ref_frame[0] = gld_index;
    gf_picture[frame_idx].ref_frame[1] = lst_index;
    gf_picture[frame_idx].ref_frame[2] = alt_index;
    gf_picture[frame_idx].update_type = gf_group->update_type[frame_idx];

    switch (gf_group->update_type[frame_idx]) {
      case ARF_UPDATE:
        stack_push(arf_index_stack, alt_index, arf_stack_size);
        ++arf_stack_size;
        alt_index = frame_idx;
        break;
      case LF_UPDATE: lst_index = frame_idx; break;
      case OVERLAY_UPDATE:
        gld_index = frame_idx;
        alt_index = stack_pop(arf_index_stack, arf_stack_size);
        --arf_stack_size;
        break;
      case USE_BUF_FRAME:
        lst_index = alt_index;
        alt_index = stack_pop(arf_index_stack, arf_stack_size);
        --arf_stack_size;
        break;
      default: break;
    }

    ++*tpl_group_frames;

    // The length of group of pictures is baseline_gf_interval, plus the
    // beginning golden frame from last GOP, plus the last overlay frame in
    // the same GOP.
    if (frame_idx == gf_group->gf_group_size) break;
  }

  alt_index = -1;
  ++frame_idx;
  ++frame_gop_offset;

  // Extend two frames outside the current gf group.
  for (; frame_idx < MAX_LAG_BUFFERS && extend_frame_count < 2; ++frame_idx) {
    struct lookahead_entry *buf =
        vp9_lookahead_peek(cpi->lookahead, frame_gop_offset - 1);

    if (buf == NULL) break;

    cpi->tpl_stats[frame_idx].base_qindex = pframe_qindex;

    gf_picture[frame_idx].frame = &buf->img;
    gf_picture[frame_idx].ref_frame[0] = gld_index;
    gf_picture[frame_idx].ref_frame[1] = lst_index;
    gf_picture[frame_idx].ref_frame[2] = alt_index;
    gf_picture[frame_idx].update_type = LF_UPDATE;
    lst_index = frame_idx;
    ++*tpl_group_frames;
    ++extend_frame_count;
    ++frame_gop_offset;
  }
}

static void init_tpl_stats(VP9_COMP *cpi) {
  int frame_idx;
  for (frame_idx = 0; frame_idx < MAX_ARF_GOP_SIZE; ++frame_idx) {
    TplDepFrame *tpl_frame = &cpi->tpl_stats[frame_idx];
    memset(tpl_frame->tpl_stats_ptr, 0,
           tpl_frame->height * tpl_frame->width *
               sizeof(*tpl_frame->tpl_stats_ptr));
    tpl_frame->is_valid = 0;
  }
}

#if CONFIG_NON_GREEDY_MV
static uint32_t full_pixel_motion_search(VP9_COMP *cpi, ThreadData *td,
                                         MotionField *motion_field,
                                         int frame_idx, uint8_t *cur_frame_buf,
                                         uint8_t *ref_frame_buf, int stride,
                                         BLOCK_SIZE bsize, int mi_row,
                                         int mi_col, MV *mv) {
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MV_SPEED_FEATURES *const mv_sf = &cpi->sf.mv;
  int step_param;
  uint32_t bestsme = UINT_MAX;
  const MvLimits tmp_mv_limits = x->mv_limits;
  // lambda is used to adjust the importance of motion vector consistency.
  // TODO(angiebird): Figure out lambda's proper value.
  const int lambda = cpi->tpl_stats[frame_idx].lambda;
  int_mv nb_full_mvs[NB_MVS_NUM];
  int nb_full_mv_num;

  MV best_ref_mv1 = { 0, 0 };
  MV best_ref_mv1_full; /* full-pixel value of best_ref_mv1 */

  best_ref_mv1_full.col = best_ref_mv1.col >> 3;
  best_ref_mv1_full.row = best_ref_mv1.row >> 3;

  // Setup frame pointers
  x->plane[0].src.buf = cur_frame_buf;
  x->plane[0].src.stride = stride;
  xd->plane[0].pre[0].buf = ref_frame_buf;
  xd->plane[0].pre[0].stride = stride;

  step_param = mv_sf->reduce_first_step_size;
  step_param = VPXMIN(step_param, MAX_MVSEARCH_STEPS - 2);

  vp9_set_mv_search_range(&x->mv_limits, &best_ref_mv1);

  nb_full_mv_num =
      vp9_prepare_nb_full_mvs(motion_field, mi_row, mi_col, nb_full_mvs);
  vp9_full_pixel_diamond_new(cpi, x, bsize, &best_ref_mv1_full, step_param,
                             lambda, 1, nb_full_mvs, nb_full_mv_num, mv);

  /* restore UMV window */
  x->mv_limits = tmp_mv_limits;

  return bestsme;
}

static uint32_t sub_pixel_motion_search(VP9_COMP *cpi, ThreadData *td,
                                        uint8_t *cur_frame_buf,
                                        uint8_t *ref_frame_buf, int stride,
                                        BLOCK_SIZE bsize, MV *mv) {
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MV_SPEED_FEATURES *const mv_sf = &cpi->sf.mv;
  uint32_t bestsme = UINT_MAX;
  uint32_t distortion;
  uint32_t sse;
  int cost_list[5];

  MV best_ref_mv1 = { 0, 0 };

  // Setup frame pointers
  x->plane[0].src.buf = cur_frame_buf;
  x->plane[0].src.stride = stride;
  xd->plane[0].pre[0].buf = ref_frame_buf;
  xd->plane[0].pre[0].stride = stride;

  // TODO(yunqing): may use higher tap interp filter than 2 taps.
  // Ignore mv costing by sending NULL pointer instead of cost array
  bestsme = cpi->find_fractional_mv_step(
      x, mv, &best_ref_mv1, cpi->common.allow_high_precision_mv, x->errorperbit,
      &cpi->fn_ptr[bsize], 0, mv_sf->subpel_search_level,
      cond_cost_list(cpi, cost_list), NULL, NULL, &distortion, &sse, NULL, 0, 0,
      USE_2_TAPS);

  return bestsme;
}

#else  // CONFIG_NON_GREEDY_MV
static uint32_t motion_compensated_prediction(VP9_COMP *cpi, ThreadData *td,
                                              uint8_t *cur_frame_buf,
                                              uint8_t *ref_frame_buf,
                                              int stride, BLOCK_SIZE bsize,
                                              MV *mv) {
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MV_SPEED_FEATURES *const mv_sf = &cpi->sf.mv;
  const SEARCH_METHODS search_method = NSTEP;
  int step_param;
  int sadpb = x->sadperbit16;
  uint32_t bestsme = UINT_MAX;
  uint32_t distortion;
  uint32_t sse;
  int cost_list[5];
  const MvLimits tmp_mv_limits = x->mv_limits;

  MV best_ref_mv1 = { 0, 0 };
  MV best_ref_mv1_full; /* full-pixel value of best_ref_mv1 */

  best_ref_mv1_full.col = best_ref_mv1.col >> 3;
  best_ref_mv1_full.row = best_ref_mv1.row >> 3;

  // Setup frame pointers
  x->plane[0].src.buf = cur_frame_buf;
  x->plane[0].src.stride = stride;
  xd->plane[0].pre[0].buf = ref_frame_buf;
  xd->plane[0].pre[0].stride = stride;

  step_param = mv_sf->reduce_first_step_size;
  step_param = VPXMIN(step_param, MAX_MVSEARCH_STEPS - 2);

  vp9_set_mv_search_range(&x->mv_limits, &best_ref_mv1);

  vp9_full_pixel_search(cpi, x, bsize, &best_ref_mv1_full, step_param,
                        search_method, sadpb, cond_cost_list(cpi, cost_list),
                        &best_ref_mv1, mv, 0, 0);

  /* restore UMV window */
  x->mv_limits = tmp_mv_limits;

  // TODO(yunqing): may use higher tap interp filter than 2 taps.
  // Ignore mv costing by sending NULL pointer instead of cost array
  bestsme = cpi->find_fractional_mv_step(
      x, mv, &best_ref_mv1, cpi->common.allow_high_precision_mv, x->errorperbit,
      &cpi->fn_ptr[bsize], 0, mv_sf->subpel_search_level,
      cond_cost_list(cpi, cost_list), NULL, NULL, &distortion, &sse, NULL, 0, 0,
      USE_2_TAPS);

  return bestsme;
}
#endif

static int get_overlap_area(int grid_pos_row, int grid_pos_col, int ref_pos_row,
                            int ref_pos_col, int block, BLOCK_SIZE bsize) {
  int width = 0, height = 0;
  int bw = 4 << b_width_log2_lookup[bsize];
  int bh = 4 << b_height_log2_lookup[bsize];

  switch (block) {
    case 0:
      width = grid_pos_col + bw - ref_pos_col;
      height = grid_pos_row + bh - ref_pos_row;
      break;
    case 1:
      width = ref_pos_col + bw - grid_pos_col;
      height = grid_pos_row + bh - ref_pos_row;
      break;
    case 2:
      width = grid_pos_col + bw - ref_pos_col;
      height = ref_pos_row + bh - grid_pos_row;
      break;
    case 3:
      width = ref_pos_col + bw - grid_pos_col;
      height = ref_pos_row + bh - grid_pos_row;
      break;
    default: assert(0);
  }

  return width * height;
}

static int round_floor(int ref_pos, int bsize_pix) {
  int round;
  if (ref_pos < 0)
    round = -(1 + (-ref_pos - 1) / bsize_pix);
  else
    round = ref_pos / bsize_pix;

  return round;
}

static void tpl_model_store(TplDepStats *tpl_stats, int mi_row, int mi_col,
                            BLOCK_SIZE bsize, int stride) {
  const int mi_height = num_8x8_blocks_high_lookup[bsize];
  const int mi_width = num_8x8_blocks_wide_lookup[bsize];
  const TplDepStats *src_stats = &tpl_stats[mi_row * stride + mi_col];
  int idx, idy;

  for (idy = 0; idy < mi_height; ++idy) {
    for (idx = 0; idx < mi_width; ++idx) {
      TplDepStats *tpl_ptr = &tpl_stats[(mi_row + idy) * stride + mi_col + idx];
      const int64_t mc_flow = tpl_ptr->mc_flow;
      const int64_t mc_ref_cost = tpl_ptr->mc_ref_cost;
      *tpl_ptr = *src_stats;
      tpl_ptr->mc_flow = mc_flow;
      tpl_ptr->mc_ref_cost = mc_ref_cost;
      tpl_ptr->mc_dep_cost = tpl_ptr->intra_cost + tpl_ptr->mc_flow;
    }
  }
}

static void tpl_model_update_b(TplDepFrame *tpl_frame, TplDepStats *tpl_stats,
                               int mi_row, int mi_col, const BLOCK_SIZE bsize) {
  TplDepFrame *ref_tpl_frame = &tpl_frame[tpl_stats->ref_frame_index];
  TplDepStats *ref_stats = ref_tpl_frame->tpl_stats_ptr;
  MV mv = tpl_stats->mv.as_mv;
  int mv_row = mv.row >> 3;
  int mv_col = mv.col >> 3;

  int ref_pos_row = mi_row * MI_SIZE + mv_row;
  int ref_pos_col = mi_col * MI_SIZE + mv_col;

  const int bw = 4 << b_width_log2_lookup[bsize];
  const int bh = 4 << b_height_log2_lookup[bsize];
  const int mi_height = num_8x8_blocks_high_lookup[bsize];
  const int mi_width = num_8x8_blocks_wide_lookup[bsize];
  const int pix_num = bw * bh;

  // top-left on grid block location in pixel
  int grid_pos_row_base = round_floor(ref_pos_row, bh) * bh;
  int grid_pos_col_base = round_floor(ref_pos_col, bw) * bw;
  int block;

  for (block = 0; block < 4; ++block) {
    int grid_pos_row = grid_pos_row_base + bh * (block >> 1);
    int grid_pos_col = grid_pos_col_base + bw * (block & 0x01);

    if (grid_pos_row >= 0 && grid_pos_row < ref_tpl_frame->mi_rows * MI_SIZE &&
        grid_pos_col >= 0 && grid_pos_col < ref_tpl_frame->mi_cols * MI_SIZE) {
      int overlap_area = get_overlap_area(
          grid_pos_row, grid_pos_col, ref_pos_row, ref_pos_col, block, bsize);
      int ref_mi_row = round_floor(grid_pos_row, bh) * mi_height;
      int ref_mi_col = round_floor(grid_pos_col, bw) * mi_width;

      int64_t mc_flow = tpl_stats->mc_dep_cost -
                        (tpl_stats->mc_dep_cost * tpl_stats->inter_cost) /
                            tpl_stats->intra_cost;

      int idx, idy;

      for (idy = 0; idy < mi_height; ++idy) {
        for (idx = 0; idx < mi_width; ++idx) {
          TplDepStats *des_stats =
              &ref_stats[(ref_mi_row + idy) * ref_tpl_frame->stride +
                         (ref_mi_col + idx)];

          des_stats->mc_flow += (mc_flow * overlap_area) / pix_num;
          des_stats->mc_ref_cost +=
              ((tpl_stats->intra_cost - tpl_stats->inter_cost) * overlap_area) /
              pix_num;
          assert(overlap_area >= 0);
        }
      }
    }
  }
}

static void tpl_model_update(TplDepFrame *tpl_frame, TplDepStats *tpl_stats,
                             int mi_row, int mi_col, const BLOCK_SIZE bsize) {
  int idx, idy;
  const int mi_height = num_8x8_blocks_high_lookup[bsize];
  const int mi_width = num_8x8_blocks_wide_lookup[bsize];

  for (idy = 0; idy < mi_height; ++idy) {
    for (idx = 0; idx < mi_width; ++idx) {
      TplDepStats *tpl_ptr =
          &tpl_stats[(mi_row + idy) * tpl_frame->stride + (mi_col + idx)];
      tpl_model_update_b(tpl_frame, tpl_ptr, mi_row + idy, mi_col + idx,
                         BLOCK_8X8);
    }
  }
}

static void get_quantize_error(MACROBLOCK *x, int plane, tran_low_t *coeff,
                               tran_low_t *qcoeff, tran_low_t *dqcoeff,
                               TX_SIZE tx_size, int64_t *recon_error,
                               int64_t *sse) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblock_plane *const p = &x->plane[plane];
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const scan_order *const scan_order = &vp9_default_scan_orders[tx_size];
  uint16_t eob;
  int pix_num = 1 << num_pels_log2_lookup[txsize_to_bsize[tx_size]];
  const int shift = tx_size == TX_32X32 ? 0 : 2;

#if CONFIG_VP9_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    vp9_highbd_quantize_fp_32x32(coeff, pix_num, x->skip_block, p->round_fp,
                                 p->quant_fp, qcoeff, dqcoeff, pd->dequant,
                                 &eob, scan_order->scan, scan_order->iscan);
  } else {
    vp9_quantize_fp_32x32(coeff, pix_num, x->skip_block, p->round_fp,
                          p->quant_fp, qcoeff, dqcoeff, pd->dequant, &eob,
                          scan_order->scan, scan_order->iscan);
  }
#else
  vp9_quantize_fp_32x32(coeff, pix_num, x->skip_block, p->round_fp, p->quant_fp,
                        qcoeff, dqcoeff, pd->dequant, &eob, scan_order->scan,
                        scan_order->iscan);
#endif  // CONFIG_VP9_HIGHBITDEPTH

  *recon_error = vp9_block_error(coeff, dqcoeff, pix_num, sse) >> shift;
  *recon_error = VPXMAX(*recon_error, 1);

  *sse = (*sse) >> shift;
  *sse = VPXMAX(*sse, 1);
}

#if CONFIG_VP9_HIGHBITDEPTH
void highbd_wht_fwd_txfm(int16_t *src_diff, int bw, tran_low_t *coeff,
                         TX_SIZE tx_size) {
  // TODO(sdeng): Implement SIMD based high bit-depth Hadamard transforms.
  switch (tx_size) {
    case TX_8X8: vpx_highbd_hadamard_8x8(src_diff, bw, coeff); break;
    case TX_16X16: vpx_highbd_hadamard_16x16(src_diff, bw, coeff); break;
    case TX_32X32: vpx_highbd_hadamard_32x32(src_diff, bw, coeff); break;
    default: assert(0);
  }
}
#endif  // CONFIG_VP9_HIGHBITDEPTH

void wht_fwd_txfm(int16_t *src_diff, int bw, tran_low_t *coeff,
                  TX_SIZE tx_size) {
  switch (tx_size) {
    case TX_8X8: vpx_hadamard_8x8(src_diff, bw, coeff); break;
    case TX_16X16: vpx_hadamard_16x16(src_diff, bw, coeff); break;
    case TX_32X32: vpx_hadamard_32x32(src_diff, bw, coeff); break;
    default: assert(0);
  }
}

static void set_mv_limits(const VP9_COMMON *cm, MACROBLOCK *x, int mi_row,
                          int mi_col) {
  x->mv_limits.row_min = -((mi_row * MI_SIZE) + (17 - 2 * VP9_INTERP_EXTEND));
  x->mv_limits.row_max =
      (cm->mi_rows - 1 - mi_row) * MI_SIZE + (17 - 2 * VP9_INTERP_EXTEND);
  x->mv_limits.col_min = -((mi_col * MI_SIZE) + (17 - 2 * VP9_INTERP_EXTEND));
  x->mv_limits.col_max =
      ((cm->mi_cols - 1 - mi_col) * MI_SIZE) + (17 - 2 * VP9_INTERP_EXTEND);
}

static void mode_estimation(VP9_COMP *cpi, MACROBLOCK *x, MACROBLOCKD *xd,
                            struct scale_factors *sf, GF_PICTURE *gf_picture,
                            int frame_idx, TplDepFrame *tpl_frame,
                            int16_t *src_diff, tran_low_t *coeff,
                            tran_low_t *qcoeff, tran_low_t *dqcoeff, int mi_row,
                            int mi_col, BLOCK_SIZE bsize, TX_SIZE tx_size,
                            YV12_BUFFER_CONFIG *ref_frame[], uint8_t *predictor,
                            int64_t *recon_error, int64_t *sse) {
  VP9_COMMON *cm = &cpi->common;
  ThreadData *td = &cpi->td;

  const int bw = 4 << b_width_log2_lookup[bsize];
  const int bh = 4 << b_height_log2_lookup[bsize];
  const int pix_num = bw * bh;
  int best_rf_idx = -1;
  int_mv best_mv;
  int64_t best_inter_cost = INT64_MAX;
  int64_t inter_cost;
  int rf_idx;
  const InterpKernel *const kernel = vp9_filter_kernels[EIGHTTAP];

  int64_t best_intra_cost = INT64_MAX;
  int64_t intra_cost;
  PREDICTION_MODE mode;
  int mb_y_offset = mi_row * MI_SIZE * xd->cur_buf->y_stride + mi_col * MI_SIZE;
  MODE_INFO mi_above, mi_left;
  const int mi_height = num_8x8_blocks_high_lookup[bsize];
  const int mi_width = num_8x8_blocks_wide_lookup[bsize];
  TplDepStats *tpl_stats =
      &tpl_frame->tpl_stats_ptr[mi_row * tpl_frame->stride + mi_col];

  xd->mb_to_top_edge = -((mi_row * MI_SIZE) * 8);
  xd->mb_to_bottom_edge = ((cm->mi_rows - 1 - mi_row) * MI_SIZE) * 8;
  xd->mb_to_left_edge = -((mi_col * MI_SIZE) * 8);
  xd->mb_to_right_edge = ((cm->mi_cols - 1 - mi_col) * MI_SIZE) * 8;
  xd->above_mi = (mi_row > 0) ? &mi_above : NULL;
  xd->left_mi = (mi_col > 0) ? &mi_left : NULL;

  // Intra prediction search
  for (mode = DC_PRED; mode <= TM_PRED; ++mode) {
    uint8_t *src, *dst;
    int src_stride, dst_stride;

    src = xd->cur_buf->y_buffer + mb_y_offset;
    src_stride = xd->cur_buf->y_stride;

    dst = &predictor[0];
    dst_stride = bw;

    xd->mi[0]->sb_type = bsize;
    xd->mi[0]->ref_frame[0] = INTRA_FRAME;

    vp9_predict_intra_block(xd, b_width_log2_lookup[bsize], tx_size, mode, src,
                            src_stride, dst, dst_stride, 0, 0, 0);

#if CONFIG_VP9_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      vpx_highbd_subtract_block(bh, bw, src_diff, bw, src, src_stride, dst,
                                dst_stride, xd->bd);
      highbd_wht_fwd_txfm(src_diff, bw, coeff, tx_size);
      intra_cost = vpx_highbd_satd(coeff, pix_num);
    } else {
      vpx_subtract_block(bh, bw, src_diff, bw, src, src_stride, dst,
                         dst_stride);
      wht_fwd_txfm(src_diff, bw, coeff, tx_size);
      intra_cost = vpx_satd(coeff, pix_num);
    }
#else
    vpx_subtract_block(bh, bw, src_diff, bw, src, src_stride, dst, dst_stride);
    wht_fwd_txfm(src_diff, bw, coeff, tx_size);
    intra_cost = vpx_satd(coeff, pix_num);
#endif  // CONFIG_VP9_HIGHBITDEPTH

    if (intra_cost < best_intra_cost) best_intra_cost = intra_cost;
  }

  // Motion compensated prediction
  best_mv.as_int = 0;

  set_mv_limits(cm, x, mi_row, mi_col);

  for (rf_idx = 0; rf_idx < MAX_INTER_REF_FRAMES; ++rf_idx) {
    int_mv mv;
#if CONFIG_NON_GREEDY_MV
    MotionField *motion_field;
#endif
    if (ref_frame[rf_idx] == NULL) continue;

#if CONFIG_NON_GREEDY_MV
    (void)td;
    motion_field = vp9_motion_field_info_get_motion_field(
        &cpi->motion_field_info, frame_idx, rf_idx, bsize);
    mv = vp9_motion_field_mi_get_mv(motion_field, mi_row, mi_col);
#else
    motion_compensated_prediction(cpi, td, xd->cur_buf->y_buffer + mb_y_offset,
                                  ref_frame[rf_idx]->y_buffer + mb_y_offset,
                                  xd->cur_buf->y_stride, bsize, &mv.as_mv);
#endif

#if CONFIG_VP9_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      vp9_highbd_build_inter_predictor(
          CONVERT_TO_SHORTPTR(ref_frame[rf_idx]->y_buffer + mb_y_offset),
          ref_frame[rf_idx]->y_stride, CONVERT_TO_SHORTPTR(&predictor[0]), bw,
          &mv.as_mv, sf, bw, bh, 0, kernel, MV_PRECISION_Q3, mi_col * MI_SIZE,
          mi_row * MI_SIZE, xd->bd);
      vpx_highbd_subtract_block(
          bh, bw, src_diff, bw, xd->cur_buf->y_buffer + mb_y_offset,
          xd->cur_buf->y_stride, &predictor[0], bw, xd->bd);
      highbd_wht_fwd_txfm(src_diff, bw, coeff, tx_size);
      inter_cost = vpx_highbd_satd(coeff, pix_num);
    } else {
      vp9_build_inter_predictor(
          ref_frame[rf_idx]->y_buffer + mb_y_offset,
          ref_frame[rf_idx]->y_stride, &predictor[0], bw, &mv.as_mv, sf, bw, bh,
          0, kernel, MV_PRECISION_Q3, mi_col * MI_SIZE, mi_row * MI_SIZE);
      vpx_subtract_block(bh, bw, src_diff, bw,
                         xd->cur_buf->y_buffer + mb_y_offset,
                         xd->cur_buf->y_stride, &predictor[0], bw);
      wht_fwd_txfm(src_diff, bw, coeff, tx_size);
      inter_cost = vpx_satd(coeff, pix_num);
    }
#else
    vp9_build_inter_predictor(ref_frame[rf_idx]->y_buffer + mb_y_offset,
                              ref_frame[rf_idx]->y_stride, &predictor[0], bw,
                              &mv.as_mv, sf, bw, bh, 0, kernel, MV_PRECISION_Q3,
                              mi_col * MI_SIZE, mi_row * MI_SIZE);
    vpx_subtract_block(bh, bw, src_diff, bw,
                       xd->cur_buf->y_buffer + mb_y_offset,
                       xd->cur_buf->y_stride, &predictor[0], bw);
    wht_fwd_txfm(src_diff, bw, coeff, tx_size);
    inter_cost = vpx_satd(coeff, pix_num);
#endif

    if (inter_cost < best_inter_cost) {
      best_rf_idx = rf_idx;
      best_inter_cost = inter_cost;
      best_mv.as_int = mv.as_int;
      get_quantize_error(x, 0, coeff, qcoeff, dqcoeff, tx_size, recon_error,
                         sse);
    }
  }
  best_intra_cost = VPXMAX(best_intra_cost, 1);
  best_inter_cost = VPXMIN(best_intra_cost, best_inter_cost);
  tpl_stats->inter_cost = VPXMAX(
      1, (best_inter_cost << TPL_DEP_COST_SCALE_LOG2) / (mi_height * mi_width));
  tpl_stats->intra_cost = VPXMAX(
      1, (best_intra_cost << TPL_DEP_COST_SCALE_LOG2) / (mi_height * mi_width));
  tpl_stats->ref_frame_index = gf_picture[frame_idx].ref_frame[best_rf_idx];
  tpl_stats->mv.as_int = best_mv.as_int;
}

#if CONFIG_NON_GREEDY_MV
static int get_block_src_pred_buf(MACROBLOCKD *xd, GF_PICTURE *gf_picture,
                                  int frame_idx, int rf_idx, int mi_row,
                                  int mi_col, struct buf_2d *src,
                                  struct buf_2d *pre) {
  const int mb_y_offset =
      mi_row * MI_SIZE * xd->cur_buf->y_stride + mi_col * MI_SIZE;
  YV12_BUFFER_CONFIG *ref_frame = NULL;
  int ref_frame_idx = gf_picture[frame_idx].ref_frame[rf_idx];
  if (ref_frame_idx != -1) {
    ref_frame = gf_picture[ref_frame_idx].frame;
    src->buf = xd->cur_buf->y_buffer + mb_y_offset;
    src->stride = xd->cur_buf->y_stride;
    pre->buf = ref_frame->y_buffer + mb_y_offset;
    pre->stride = ref_frame->y_stride;
    assert(src->stride == pre->stride);
    return 1;
  } else {
    printf("invalid ref_frame_idx");
    assert(ref_frame_idx != -1);
    return 0;
  }
}

#define kMvPreCheckLines 5
#define kMvPreCheckSize 15

#define MV_REF_POS_NUM 3
POSITION mv_ref_pos[MV_REF_POS_NUM] = {
  { -1, 0 },
  { 0, -1 },
  { -1, -1 },
};

static int_mv *get_select_mv(VP9_COMP *cpi, TplDepFrame *tpl_frame, int mi_row,
                             int mi_col) {
  return &cpi->select_mv_arr[mi_row * tpl_frame->stride + mi_col];
}

static int_mv find_ref_mv(int mv_mode, VP9_COMP *cpi, TplDepFrame *tpl_frame,
                          BLOCK_SIZE bsize, int mi_row, int mi_col) {
  int i;
  const int mi_height = num_8x8_blocks_high_lookup[bsize];
  const int mi_width = num_8x8_blocks_wide_lookup[bsize];
  int_mv nearest_mv, near_mv, invalid_mv;
  nearest_mv.as_int = INVALID_MV;
  near_mv.as_int = INVALID_MV;
  invalid_mv.as_int = INVALID_MV;
  for (i = 0; i < MV_REF_POS_NUM; ++i) {
    int nb_row = mi_row + mv_ref_pos[i].row * mi_height;
    int nb_col = mi_col + mv_ref_pos[i].col * mi_width;
    assert(mv_ref_pos[i].row <= 0);
    assert(mv_ref_pos[i].col <= 0);
    if (nb_row >= 0 && nb_col >= 0) {
      if (nearest_mv.as_int == INVALID_MV) {
        nearest_mv = *get_select_mv(cpi, tpl_frame, nb_row, nb_col);
      } else {
        int_mv mv = *get_select_mv(cpi, tpl_frame, nb_row, nb_col);
        if (mv.as_int == nearest_mv.as_int) {
          continue;
        } else {
          near_mv = mv;
          break;
        }
      }
    }
  }
  if (nearest_mv.as_int == INVALID_MV) {
    nearest_mv.as_mv.row = 0;
    nearest_mv.as_mv.col = 0;
  }
  if (near_mv.as_int == INVALID_MV) {
    near_mv.as_mv.row = 0;
    near_mv.as_mv.col = 0;
  }
  if (mv_mode == NEAREST_MV_MODE) {
    return nearest_mv;
  }
  if (mv_mode == NEAR_MV_MODE) {
    return near_mv;
  }
  assert(0);
  return invalid_mv;
}

static int_mv get_mv_from_mv_mode(int mv_mode, VP9_COMP *cpi,
                                  MotionField *motion_field,
                                  TplDepFrame *tpl_frame, BLOCK_SIZE bsize,
                                  int mi_row, int mi_col) {
  int_mv mv;
  switch (mv_mode) {
    case ZERO_MV_MODE:
      mv.as_mv.row = 0;
      mv.as_mv.col = 0;
      break;
    case NEW_MV_MODE:
      mv = vp9_motion_field_mi_get_mv(motion_field, mi_row, mi_col);
      break;
    case NEAREST_MV_MODE:
      mv = find_ref_mv(mv_mode, cpi, tpl_frame, bsize, mi_row, mi_col);
      break;
    case NEAR_MV_MODE:
      mv = find_ref_mv(mv_mode, cpi, tpl_frame, bsize, mi_row, mi_col);
      break;
    default:
      mv.as_int = INVALID_MV;
      assert(0);
      break;
  }
  return mv;
}

static double get_mv_dist(int mv_mode, VP9_COMP *cpi, MACROBLOCKD *xd,
                          GF_PICTURE *gf_picture, MotionField *motion_field,
                          int frame_idx, TplDepFrame *tpl_frame, int rf_idx,
                          BLOCK_SIZE bsize, int mi_row, int mi_col,
                          int_mv *mv) {
  uint32_t sse;
  struct buf_2d src;
  struct buf_2d pre;
  MV full_mv;
  *mv = get_mv_from_mv_mode(mv_mode, cpi, motion_field, tpl_frame, bsize,
                            mi_row, mi_col);
  full_mv = get_full_mv(&mv->as_mv);
  if (get_block_src_pred_buf(xd, gf_picture, frame_idx, rf_idx, mi_row, mi_col,
                             &src, &pre)) {
    // TODO(angiebird): Consider subpixel when computing the sse.
    cpi->fn_ptr[bsize].vf(src.buf, src.stride, get_buf_from_mv(&pre, &full_mv),
                          pre.stride, &sse);
    return (double)(sse << VP9_DIST_SCALE_LOG2);
  } else {
    assert(0);
    return 0;
  }
}

static int get_mv_mode_cost(int mv_mode) {
  // TODO(angiebird): The probabilities are roughly inferred from
  // default_inter_mode_probs. Check if there is a better way to set the
  // probabilities.
  const int zero_mv_prob = 16;
  const int new_mv_prob = 24 * 1;
  const int ref_mv_prob = 256 - zero_mv_prob - new_mv_prob;
  assert(zero_mv_prob + new_mv_prob + ref_mv_prob == 256);
  switch (mv_mode) {
    case ZERO_MV_MODE: return vp9_prob_cost[zero_mv_prob]; break;
    case NEW_MV_MODE: return vp9_prob_cost[new_mv_prob]; break;
    case NEAREST_MV_MODE: return vp9_prob_cost[ref_mv_prob]; break;
    case NEAR_MV_MODE: return vp9_prob_cost[ref_mv_prob]; break;
    default: assert(0); return -1;
  }
}

static INLINE double get_mv_diff_cost(MV *new_mv, MV *ref_mv) {
  double mv_diff_cost = log2(1 + abs(new_mv->row - ref_mv->row)) +
                        log2(1 + abs(new_mv->col - ref_mv->col));
  mv_diff_cost *= (1 << VP9_PROB_COST_SHIFT);
  return mv_diff_cost;
}
static double get_mv_cost(int mv_mode, VP9_COMP *cpi, MotionField *motion_field,
                          TplDepFrame *tpl_frame, BLOCK_SIZE bsize, int mi_row,
                          int mi_col) {
  double mv_cost = get_mv_mode_cost(mv_mode);
  if (mv_mode == NEW_MV_MODE) {
    MV new_mv = get_mv_from_mv_mode(mv_mode, cpi, motion_field, tpl_frame,
                                    bsize, mi_row, mi_col)
                    .as_mv;
    MV nearest_mv = get_mv_from_mv_mode(NEAREST_MV_MODE, cpi, motion_field,
                                        tpl_frame, bsize, mi_row, mi_col)
                        .as_mv;
    MV near_mv = get_mv_from_mv_mode(NEAR_MV_MODE, cpi, motion_field, tpl_frame,
                                     bsize, mi_row, mi_col)
                     .as_mv;
    double nearest_cost = get_mv_diff_cost(&new_mv, &nearest_mv);
    double near_cost = get_mv_diff_cost(&new_mv, &near_mv);
    mv_cost += nearest_cost < near_cost ? nearest_cost : near_cost;
  }
  return mv_cost;
}

static double eval_mv_mode(int mv_mode, VP9_COMP *cpi, MACROBLOCK *x,
                           GF_PICTURE *gf_picture, MotionField *motion_field,
                           int frame_idx, TplDepFrame *tpl_frame, int rf_idx,
                           BLOCK_SIZE bsize, int mi_row, int mi_col,
                           int_mv *mv) {
  MACROBLOCKD *xd = &x->e_mbd;
  double mv_dist =
      get_mv_dist(mv_mode, cpi, xd, gf_picture, motion_field, frame_idx,
                  tpl_frame, rf_idx, bsize, mi_row, mi_col, mv);
  double mv_cost =
      get_mv_cost(mv_mode, cpi, motion_field, tpl_frame, bsize, mi_row, mi_col);
  double mult = 180;

  return mv_cost + mult * log2f(1 + mv_dist);
}

static int find_best_ref_mv_mode(VP9_COMP *cpi, MACROBLOCK *x,
                                 GF_PICTURE *gf_picture,
                                 MotionField *motion_field, int frame_idx,
                                 TplDepFrame *tpl_frame, int rf_idx,
                                 BLOCK_SIZE bsize, int mi_row, int mi_col,
                                 double *rd, int_mv *mv) {
  int best_mv_mode = ZERO_MV_MODE;
  int update = 0;
  int mv_mode;
  *rd = 0;
  for (mv_mode = 0; mv_mode < MAX_MV_MODE; ++mv_mode) {
    double this_rd;
    int_mv this_mv;
    if (mv_mode == NEW_MV_MODE) {
      continue;
    }
    this_rd = eval_mv_mode(mv_mode, cpi, x, gf_picture, motion_field, frame_idx,
                           tpl_frame, rf_idx, bsize, mi_row, mi_col, &this_mv);
    if (update == 0) {
      *rd = this_rd;
      *mv = this_mv;
      best_mv_mode = mv_mode;
      update = 1;
    } else {
      if (this_rd < *rd) {
        *rd = this_rd;
        *mv = this_mv;
        best_mv_mode = mv_mode;
      }
    }
  }
  return best_mv_mode;
}

static void predict_mv_mode(VP9_COMP *cpi, MACROBLOCK *x,
                            GF_PICTURE *gf_picture, MotionField *motion_field,
                            int frame_idx, TplDepFrame *tpl_frame, int rf_idx,
                            BLOCK_SIZE bsize, int mi_row, int mi_col) {
  const int mi_height = num_8x8_blocks_high_lookup[bsize];
  const int mi_width = num_8x8_blocks_wide_lookup[bsize];
  int tmp_mv_mode_arr[kMvPreCheckSize];
  int *mv_mode_arr = tpl_frame->mv_mode_arr[rf_idx];
  double *rd_diff_arr = tpl_frame->rd_diff_arr[rf_idx];
  int_mv *select_mv_arr = cpi->select_mv_arr;
  int_mv tmp_select_mv_arr[kMvPreCheckSize];
  int stride = tpl_frame->stride;
  double new_mv_rd = 0;
  double no_new_mv_rd = 0;
  double this_new_mv_rd = 0;
  double this_no_new_mv_rd = 0;
  int idx;
  int tmp_idx;
  assert(kMvPreCheckSize == (kMvPreCheckLines * (kMvPreCheckLines + 1)) >> 1);

  // no new mv
  // diagonal scan order
  tmp_idx = 0;
  for (idx = 0; idx < kMvPreCheckLines; ++idx) {
    int r;
    for (r = 0; r <= idx; ++r) {
      int c = idx - r;
      int nb_row = mi_row + r * mi_height;
      int nb_col = mi_col + c * mi_width;
      if (nb_row < tpl_frame->mi_rows && nb_col < tpl_frame->mi_cols) {
        double this_rd;
        int_mv *mv = &select_mv_arr[nb_row * stride + nb_col];
        mv_mode_arr[nb_row * stride + nb_col] = find_best_ref_mv_mode(
            cpi, x, gf_picture, motion_field, frame_idx, tpl_frame, rf_idx,
            bsize, nb_row, nb_col, &this_rd, mv);
        if (r == 0 && c == 0) {
          this_no_new_mv_rd = this_rd;
        }
        no_new_mv_rd += this_rd;
        tmp_mv_mode_arr[tmp_idx] = mv_mode_arr[nb_row * stride + nb_col];
        tmp_select_mv_arr[tmp_idx] = select_mv_arr[nb_row * stride + nb_col];
        ++tmp_idx;
      }
    }
  }

  // new mv
  mv_mode_arr[mi_row * stride + mi_col] = NEW_MV_MODE;
  this_new_mv_rd = eval_mv_mode(
      NEW_MV_MODE, cpi, x, gf_picture, motion_field, frame_idx, tpl_frame,
      rf_idx, bsize, mi_row, mi_col, &select_mv_arr[mi_row * stride + mi_col]);
  new_mv_rd = this_new_mv_rd;
  // We start from idx = 1 because idx = 0 is evaluated as NEW_MV_MODE
  // beforehand.
  for (idx = 1; idx < kMvPreCheckLines; ++idx) {
    int r;
    for (r = 0; r <= idx; ++r) {
      int c = idx - r;
      int nb_row = mi_row + r * mi_height;
      int nb_col = mi_col + c * mi_width;
      if (nb_row < tpl_frame->mi_rows && nb_col < tpl_frame->mi_cols) {
        double this_rd;
        int_mv *mv = &select_mv_arr[nb_row * stride + nb_col];
        mv_mode_arr[nb_row * stride + nb_col] = find_best_ref_mv_mode(
            cpi, x, gf_picture, motion_field, frame_idx, tpl_frame, rf_idx,
            bsize, nb_row, nb_col, &this_rd, mv);
        new_mv_rd += this_rd;
      }
    }
  }

  // update best_mv_mode
  tmp_idx = 0;
  if (no_new_mv_rd < new_mv_rd) {
    for (idx = 0; idx < kMvPreCheckLines; ++idx) {
      int r;
      for (r = 0; r <= idx; ++r) {
        int c = idx - r;
        int nb_row = mi_row + r * mi_height;
        int nb_col = mi_col + c * mi_width;
        if (nb_row < tpl_frame->mi_rows && nb_col < tpl_frame->mi_cols) {
          mv_mode_arr[nb_row * stride + nb_col] = tmp_mv_mode_arr[tmp_idx];
          select_mv_arr[nb_row * stride + nb_col] = tmp_select_mv_arr[tmp_idx];
          ++tmp_idx;
        }
      }
    }
    rd_diff_arr[mi_row * stride + mi_col] = 0;
  } else {
    rd_diff_arr[mi_row * stride + mi_col] =
        (no_new_mv_rd - this_no_new_mv_rd) - (new_mv_rd - this_new_mv_rd);
  }
}

static void predict_mv_mode_arr(VP9_COMP *cpi, MACROBLOCK *x,
                                GF_PICTURE *gf_picture,
                                MotionField *motion_field, int frame_idx,
                                TplDepFrame *tpl_frame, int rf_idx,
                                BLOCK_SIZE bsize) {
  const int mi_height = num_8x8_blocks_high_lookup[bsize];
  const int mi_width = num_8x8_blocks_wide_lookup[bsize];
  const int unit_rows = tpl_frame->mi_rows / mi_height;
  const int unit_cols = tpl_frame->mi_cols / mi_width;
  const int max_diagonal_lines = unit_rows + unit_cols - 1;
  int idx;
  for (idx = 0; idx < max_diagonal_lines; ++idx) {
    int r;
    for (r = VPXMAX(idx - unit_cols + 1, 0); r <= VPXMIN(idx, unit_rows - 1);
         ++r) {
      int c = idx - r;
      int mi_row = r * mi_height;
      int mi_col = c * mi_width;
      assert(c >= 0 && c < unit_cols);
      assert(mi_row >= 0 && mi_row < tpl_frame->mi_rows);
      assert(mi_col >= 0 && mi_col < tpl_frame->mi_cols);
      predict_mv_mode(cpi, x, gf_picture, motion_field, frame_idx, tpl_frame,
                      rf_idx, bsize, mi_row, mi_col);
    }
  }
}

static void do_motion_search(VP9_COMP *cpi, ThreadData *td,
                             MotionField *motion_field, int frame_idx,
                             YV12_BUFFER_CONFIG *ref_frame, BLOCK_SIZE bsize,
                             int mi_row, int mi_col) {
  VP9_COMMON *cm = &cpi->common;
  MACROBLOCK *x = &td->mb;
  MACROBLOCKD *xd = &x->e_mbd;
  const int mb_y_offset =
      mi_row * MI_SIZE * xd->cur_buf->y_stride + mi_col * MI_SIZE;
  assert(ref_frame != NULL);
  set_mv_limits(cm, x, mi_row, mi_col);
  {
    int_mv mv = vp9_motion_field_mi_get_mv(motion_field, mi_row, mi_col);
    uint8_t *cur_frame_buf = xd->cur_buf->y_buffer + mb_y_offset;
    uint8_t *ref_frame_buf = ref_frame->y_buffer + mb_y_offset;
    const int stride = xd->cur_buf->y_stride;
    full_pixel_motion_search(cpi, td, motion_field, frame_idx, cur_frame_buf,
                             ref_frame_buf, stride, bsize, mi_row, mi_col,
                             &mv.as_mv);
    sub_pixel_motion_search(cpi, td, cur_frame_buf, ref_frame_buf, stride,
                            bsize, &mv.as_mv);
    vp9_motion_field_mi_set_mv(motion_field, mi_row, mi_col, mv);
  }
}

static void build_motion_field(
    VP9_COMP *cpi, int frame_idx,
    YV12_BUFFER_CONFIG *ref_frame[MAX_INTER_REF_FRAMES], BLOCK_SIZE bsize) {
  VP9_COMMON *cm = &cpi->common;
  ThreadData *td = &cpi->td;
  TplDepFrame *tpl_frame = &cpi->tpl_stats[frame_idx];
  const int mi_height = num_8x8_blocks_high_lookup[bsize];
  const int mi_width = num_8x8_blocks_wide_lookup[bsize];
  const int pw = num_4x4_blocks_wide_lookup[bsize] << 2;
  const int ph = num_4x4_blocks_high_lookup[bsize] << 2;
  int mi_row, mi_col;
  int rf_idx;

  tpl_frame->lambda = (pw * ph) >> 2;
  assert(pw * ph == tpl_frame->lambda << 2);

  for (rf_idx = 0; rf_idx < MAX_INTER_REF_FRAMES; ++rf_idx) {
    MotionField *motion_field = vp9_motion_field_info_get_motion_field(
        &cpi->motion_field_info, frame_idx, rf_idx, bsize);
    if (ref_frame[rf_idx] == NULL) {
      continue;
    }
    vp9_motion_field_reset_mvs(motion_field);
    for (mi_row = 0; mi_row < cm->mi_rows; mi_row += mi_height) {
      for (mi_col = 0; mi_col < cm->mi_cols; mi_col += mi_width) {
        do_motion_search(cpi, td, motion_field, frame_idx, ref_frame[rf_idx],
                         bsize, mi_row, mi_col);
      }
    }
  }
}
#endif  // CONFIG_NON_GREEDY_MV

static void mc_flow_dispenser(VP9_COMP *cpi, GF_PICTURE *gf_picture,
                              int frame_idx, BLOCK_SIZE bsize) {
  TplDepFrame *tpl_frame = &cpi->tpl_stats[frame_idx];
  YV12_BUFFER_CONFIG *this_frame = gf_picture[frame_idx].frame;
  YV12_BUFFER_CONFIG *ref_frame[MAX_INTER_REF_FRAMES] = { NULL, NULL, NULL };

  VP9_COMMON *cm = &cpi->common;
  struct scale_factors sf;
  int rdmult, idx;
  ThreadData *td = &cpi->td;
  MACROBLOCK *x = &td->mb;
  MACROBLOCKD *xd = &x->e_mbd;
  int mi_row, mi_col;

#if CONFIG_VP9_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint16_t, predictor16[32 * 32 * 3]);
  DECLARE_ALIGNED(16, uint8_t, predictor8[32 * 32 * 3]);
  uint8_t *predictor;
#else
  DECLARE_ALIGNED(16, uint8_t, predictor[32 * 32 * 3]);
#endif
  DECLARE_ALIGNED(16, int16_t, src_diff[32 * 32]);
  DECLARE_ALIGNED(16, tran_low_t, coeff[32 * 32]);
  DECLARE_ALIGNED(16, tran_low_t, qcoeff[32 * 32]);
  DECLARE_ALIGNED(16, tran_low_t, dqcoeff[32 * 32]);

  const TX_SIZE tx_size = max_txsize_lookup[bsize];
  const int mi_height = num_8x8_blocks_high_lookup[bsize];
  const int mi_width = num_8x8_blocks_wide_lookup[bsize];
  int64_t recon_error, sse;
#if CONFIG_NON_GREEDY_MV
  int square_block_idx;
  int rf_idx;
#endif

  // Setup scaling factor
#if CONFIG_VP9_HIGHBITDEPTH
  vp9_setup_scale_factors_for_frame(
      &sf, this_frame->y_crop_width, this_frame->y_crop_height,
      this_frame->y_crop_width, this_frame->y_crop_height,
      cpi->common.use_highbitdepth);

  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
    predictor = CONVERT_TO_BYTEPTR(predictor16);
  else
    predictor = predictor8;
#else
  vp9_setup_scale_factors_for_frame(
      &sf, this_frame->y_crop_width, this_frame->y_crop_height,
      this_frame->y_crop_width, this_frame->y_crop_height);
#endif  // CONFIG_VP9_HIGHBITDEPTH

  // Prepare reference frame pointers. If any reference frame slot is
  // unavailable, the pointer will be set to Null.
  for (idx = 0; idx < MAX_INTER_REF_FRAMES; ++idx) {
    int rf_idx = gf_picture[frame_idx].ref_frame[idx];
    if (rf_idx != -1) ref_frame[idx] = gf_picture[rf_idx].frame;
  }

  xd->mi = cm->mi_grid_visible;
  xd->mi[0] = cm->mi;
  xd->cur_buf = this_frame;

  // Get rd multiplier set up.
  rdmult = vp9_compute_rd_mult_based_on_qindex(cpi, tpl_frame->base_qindex);
  set_error_per_bit(&cpi->td.mb, rdmult);
  vp9_initialize_me_consts(cpi, &cpi->td.mb, tpl_frame->base_qindex);

  tpl_frame->is_valid = 1;

  cm->base_qindex = tpl_frame->base_qindex;
  vp9_frame_init_quantizer(cpi);

#if CONFIG_NON_GREEDY_MV
  for (square_block_idx = 0; square_block_idx < SQUARE_BLOCK_SIZES;
       ++square_block_idx) {
    BLOCK_SIZE square_bsize = square_block_idx_to_bsize(square_block_idx);
    build_motion_field(cpi, frame_idx, ref_frame, square_bsize);
  }
  for (rf_idx = 0; rf_idx < MAX_INTER_REF_FRAMES; ++rf_idx) {
    int ref_frame_idx = gf_picture[frame_idx].ref_frame[rf_idx];
    if (ref_frame_idx != -1) {
      MotionField *motion_field = vp9_motion_field_info_get_motion_field(
          &cpi->motion_field_info, frame_idx, rf_idx, bsize);
      predict_mv_mode_arr(cpi, x, gf_picture, motion_field, frame_idx,
                          tpl_frame, rf_idx, bsize);
    }
  }
#endif

  for (mi_row = 0; mi_row < cm->mi_rows; mi_row += mi_height) {
    for (mi_col = 0; mi_col < cm->mi_cols; mi_col += mi_width) {
      mode_estimation(cpi, x, xd, &sf, gf_picture, frame_idx, tpl_frame,
                      src_diff, coeff, qcoeff, dqcoeff, mi_row, mi_col, bsize,
                      tx_size, ref_frame, predictor, &recon_error, &sse);
      // Motion flow dependency dispenser.
      tpl_model_store(tpl_frame->tpl_stats_ptr, mi_row, mi_col, bsize,
                      tpl_frame->stride);

      tpl_model_update(cpi->tpl_stats, tpl_frame->tpl_stats_ptr, mi_row, mi_col,
                       bsize);
    }
  }
}

#if CONFIG_NON_GREEDY_MV
#define DUMP_TPL_STATS 0
#if DUMP_TPL_STATS
static void dump_buf(uint8_t *buf, int stride, int row, int col, int h, int w) {
  int i, j;
  printf("%d %d\n", h, w);
  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; ++j) {
      printf("%d ", buf[(row + i) * stride + col + j]);
    }
  }
  printf("\n");
}

static void dump_frame_buf(const YV12_BUFFER_CONFIG *frame_buf) {
  dump_buf(frame_buf->y_buffer, frame_buf->y_stride, 0, 0, frame_buf->y_height,
           frame_buf->y_width);
  dump_buf(frame_buf->u_buffer, frame_buf->uv_stride, 0, 0,
           frame_buf->uv_height, frame_buf->uv_width);
  dump_buf(frame_buf->v_buffer, frame_buf->uv_stride, 0, 0,
           frame_buf->uv_height, frame_buf->uv_width);
}

static void dump_tpl_stats(const VP9_COMP *cpi, int tpl_group_frames,
                           const GF_GROUP *gf_group,
                           const GF_PICTURE *gf_picture, BLOCK_SIZE bsize) {
  int frame_idx;
  const VP9_COMMON *cm = &cpi->common;
  int rf_idx;
  for (frame_idx = 1; frame_idx < tpl_group_frames; ++frame_idx) {
    for (rf_idx = 0; rf_idx < MAX_INTER_REF_FRAMES; ++rf_idx) {
      const TplDepFrame *tpl_frame = &cpi->tpl_stats[frame_idx];
      int mi_row, mi_col;
      int ref_frame_idx;
      const int mi_height = num_8x8_blocks_high_lookup[bsize];
      const int mi_width = num_8x8_blocks_wide_lookup[bsize];
      ref_frame_idx = gf_picture[frame_idx].ref_frame[rf_idx];
      if (ref_frame_idx != -1) {
        YV12_BUFFER_CONFIG *ref_frame_buf = gf_picture[ref_frame_idx].frame;
        const int gf_frame_offset = gf_group->frame_gop_index[frame_idx];
        const int ref_gf_frame_offset =
            gf_group->frame_gop_index[ref_frame_idx];
        printf("=\n");
        printf(
            "frame_idx %d mi_rows %d mi_cols %d bsize %d ref_frame_idx %d "
            "rf_idx %d gf_frame_offset %d ref_gf_frame_offset %d\n",
            frame_idx, cm->mi_rows, cm->mi_cols, mi_width * MI_SIZE,
            ref_frame_idx, rf_idx, gf_frame_offset, ref_gf_frame_offset);
        for (mi_row = 0; mi_row < cm->mi_rows; ++mi_row) {
          for (mi_col = 0; mi_col < cm->mi_cols; ++mi_col) {
            if ((mi_row % mi_height) == 0 && (mi_col % mi_width) == 0) {
              int_mv mv = vp9_motion_field_info_get_mv(&cpi->motion_field_info,
                                                       frame_idx, rf_idx, bsize,
                                                       mi_row, mi_col);
              printf("%d %d %d %d\n", mi_row, mi_col, mv.as_mv.row,
                     mv.as_mv.col);
            }
          }
        }
        for (mi_row = 0; mi_row < cm->mi_rows; ++mi_row) {
          for (mi_col = 0; mi_col < cm->mi_cols; ++mi_col) {
            if ((mi_row % mi_height) == 0 && (mi_col % mi_width) == 0) {
              const TplDepStats *tpl_ptr =
                  &tpl_frame
                       ->tpl_stats_ptr[mi_row * tpl_frame->stride + mi_col];
              printf("%f ", tpl_ptr->feature_score);
            }
          }
        }
        printf("\n");

        for (mi_row = 0; mi_row < cm->mi_rows; mi_row += mi_height) {
          for (mi_col = 0; mi_col < cm->mi_cols; mi_col += mi_width) {
            const int mv_mode =
                tpl_frame
                    ->mv_mode_arr[rf_idx][mi_row * tpl_frame->stride + mi_col];
            printf("%d ", mv_mode);
          }
        }
        printf("\n");

        dump_frame_buf(gf_picture[frame_idx].frame);
        dump_frame_buf(ref_frame_buf);
      }
    }
  }
}
#endif  // DUMP_TPL_STATS
#endif  // CONFIG_NON_GREEDY_MV

static void init_tpl_buffer(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  int frame;

  const int mi_cols = mi_cols_aligned_to_sb(cm->mi_cols);
  const int mi_rows = mi_cols_aligned_to_sb(cm->mi_rows);
#if CONFIG_NON_GREEDY_MV
  int rf_idx;

  vpx_free(cpi->select_mv_arr);
  CHECK_MEM_ERROR(
      cm, cpi->select_mv_arr,
      vpx_calloc(mi_rows * mi_cols * 4, sizeof(*cpi->select_mv_arr)));
#endif

  // TODO(jingning): Reduce the actual memory use for tpl model build up.
  for (frame = 0; frame < MAX_ARF_GOP_SIZE; ++frame) {
    if (cpi->tpl_stats[frame].width >= mi_cols &&
        cpi->tpl_stats[frame].height >= mi_rows &&
        cpi->tpl_stats[frame].tpl_stats_ptr)
      continue;

#if CONFIG_NON_GREEDY_MV
    for (rf_idx = 0; rf_idx < MAX_INTER_REF_FRAMES; ++rf_idx) {
      vpx_free(cpi->tpl_stats[frame].mv_mode_arr[rf_idx]);
      CHECK_MEM_ERROR(
          cm, cpi->tpl_stats[frame].mv_mode_arr[rf_idx],
          vpx_calloc(mi_rows * mi_cols * 4,
                     sizeof(*cpi->tpl_stats[frame].mv_mode_arr[rf_idx])));
      vpx_free(cpi->tpl_stats[frame].rd_diff_arr[rf_idx]);
      CHECK_MEM_ERROR(
          cm, cpi->tpl_stats[frame].rd_diff_arr[rf_idx],
          vpx_calloc(mi_rows * mi_cols * 4,
                     sizeof(*cpi->tpl_stats[frame].rd_diff_arr[rf_idx])));
    }
#endif
    vpx_free(cpi->tpl_stats[frame].tpl_stats_ptr);
    CHECK_MEM_ERROR(cm, cpi->tpl_stats[frame].tpl_stats_ptr,
                    vpx_calloc(mi_rows * mi_cols,
                               sizeof(*cpi->tpl_stats[frame].tpl_stats_ptr)));
    cpi->tpl_stats[frame].is_valid = 0;
    cpi->tpl_stats[frame].width = mi_cols;
    cpi->tpl_stats[frame].height = mi_rows;
    cpi->tpl_stats[frame].stride = mi_cols;
    cpi->tpl_stats[frame].mi_rows = cm->mi_rows;
    cpi->tpl_stats[frame].mi_cols = cm->mi_cols;
  }

  for (frame = 0; frame < REF_FRAMES; ++frame) {
    cpi->enc_frame_buf[frame].mem_valid = 0;
    cpi->enc_frame_buf[frame].released = 1;
  }
}

static void free_tpl_buffer(VP9_COMP *cpi) {
  int frame;
#if CONFIG_NON_GREEDY_MV
  vp9_free_motion_field_info(&cpi->motion_field_info);
  vpx_free(cpi->select_mv_arr);
#endif
  for (frame = 0; frame < MAX_ARF_GOP_SIZE; ++frame) {
#if CONFIG_NON_GREEDY_MV
    int rf_idx;
    for (rf_idx = 0; rf_idx < MAX_INTER_REF_FRAMES; ++rf_idx) {
      vpx_free(cpi->tpl_stats[frame].mv_mode_arr[rf_idx]);
      vpx_free(cpi->tpl_stats[frame].rd_diff_arr[rf_idx]);
    }
#endif
    vpx_free(cpi->tpl_stats[frame].tpl_stats_ptr);
    cpi->tpl_stats[frame].is_valid = 0;
  }
}

#if CONFIG_RATE_CTRL
static void accumulate_frame_tpl_stats(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  const GF_GROUP *gf_group = &cpi->twopass.gf_group;
  int show_frame_count = 0;
  int frame_idx;
  // Accumulate tpl stats for each frame in the current group of picture.
  for (frame_idx = 1; frame_idx < gf_group->gf_group_size; ++frame_idx) {
    TplDepFrame *tpl_frame = &cpi->tpl_stats[frame_idx];
    TplDepStats *tpl_stats = tpl_frame->tpl_stats_ptr;
    const int tpl_stride = tpl_frame->stride;
    int64_t intra_cost_base = 0;
    int64_t inter_cost_base = 0;
    int64_t mc_dep_cost_base = 0;
    int64_t mc_ref_cost_base = 0;
    int64_t mc_flow_base = 0;
    int row, col;

    if (!tpl_frame->is_valid) continue;

    for (row = 0; row < cm->mi_rows && tpl_frame->is_valid; ++row) {
      for (col = 0; col < cm->mi_cols; ++col) {
        TplDepStats *this_stats = &tpl_stats[row * tpl_stride + col];
        intra_cost_base += this_stats->intra_cost;
        inter_cost_base += this_stats->inter_cost;
        mc_dep_cost_base += this_stats->mc_dep_cost;
        mc_ref_cost_base += this_stats->mc_ref_cost;
        mc_flow_base += this_stats->mc_flow;
      }
    }

    cpi->tpl_stats_info[show_frame_count].intra_cost = intra_cost_base;
    cpi->tpl_stats_info[show_frame_count].inter_cost = inter_cost_base;
    cpi->tpl_stats_info[show_frame_count].mc_dep_cost = mc_dep_cost_base;
    cpi->tpl_stats_info[show_frame_count].mc_ref_cost = mc_ref_cost_base;
    cpi->tpl_stats_info[show_frame_count].mc_flow = mc_flow_base;

    ++show_frame_count;
  }
}
#endif  // CONFIG_RATE_CTRL

static void setup_tpl_stats(VP9_COMP *cpi) {
  GF_PICTURE gf_picture[MAX_ARF_GOP_SIZE];
  const GF_GROUP *gf_group = &cpi->twopass.gf_group;
  int tpl_group_frames = 0;
  int frame_idx;
  cpi->tpl_bsize = BLOCK_32X32;

  init_gop_frames(cpi, gf_picture, gf_group, &tpl_group_frames);

  init_tpl_stats(cpi);

  // Backward propagation from tpl_group_frames to 1.
  for (frame_idx = tpl_group_frames - 1; frame_idx > 0; --frame_idx) {
    if (gf_picture[frame_idx].update_type == USE_BUF_FRAME) continue;
    mc_flow_dispenser(cpi, gf_picture, frame_idx, cpi->tpl_bsize);
  }
#if CONFIG_NON_GREEDY_MV
  cpi->tpl_ready = 1;
#if DUMP_TPL_STATS
  dump_tpl_stats(cpi, tpl_group_frames, gf_group, gf_picture, cpi->tpl_bsize);
#endif  // DUMP_TPL_STATS
#endif  // CONFIG_NON_GREEDY_MV

#if CONFIG_RATE_CTRL
  accumulate_frame_tpl_stats(cpi);
#endif  // CONFIG_RATE_CTRL
}

void vp9_get_ref_frame_info(FRAME_UPDATE_TYPE update_type, int ref_frame_flags,
                            RefCntBuffer *ref_frame_bufs[MAX_INTER_REF_FRAMES],
                            int *ref_frame_coding_indexes,
                            int *ref_frame_valid_list) {
  if (update_type != KF_UPDATE) {
    const VP9_REFFRAME inter_ref_flags[MAX_INTER_REF_FRAMES] = { VP9_LAST_FLAG,
                                                                 VP9_GOLD_FLAG,
                                                                 VP9_ALT_FLAG };
    int i;
    for (i = 0; i < MAX_INTER_REF_FRAMES; ++i) {
      assert(ref_frame_bufs[i] != NULL);
      ref_frame_coding_indexes[i] = ref_frame_bufs[i]->frame_coding_index;
      ref_frame_valid_list[i] = (ref_frame_flags & inter_ref_flags[i]) != 0;
    }
  } else {
    // No reference frame is available when this is a key frame.
    int i;
    for (i = 0; i < MAX_INTER_REF_FRAMES; ++i) {
      ref_frame_coding_indexes[i] = -1;
      ref_frame_valid_list[i] = 0;
    }
  }
}

#if !CONFIG_REALTIME_ONLY
#if CONFIG_RATE_CTRL
static void copy_frame_counts(const FRAME_COUNTS *input_counts,
                              FRAME_COUNTS *output_counts) {
  int i, j, k, l, m, n;
  for (i = 0; i < BLOCK_SIZE_GROUPS; ++i) {
    for (j = 0; j < INTRA_MODES; ++j) {
      output_counts->y_mode[i][j] = input_counts->y_mode[i][j];
    }
  }
  for (i = 0; i < INTRA_MODES; ++i) {
    for (j = 0; j < INTRA_MODES; ++j) {
      output_counts->uv_mode[i][j] = input_counts->uv_mode[i][j];
    }
  }
  for (i = 0; i < PARTITION_CONTEXTS; ++i) {
    for (j = 0; j < PARTITION_TYPES; ++j) {
      output_counts->partition[i][j] = input_counts->partition[i][j];
    }
  }
  for (i = 0; i < TX_SIZES; ++i) {
    for (j = 0; j < PLANE_TYPES; ++j) {
      for (k = 0; k < REF_TYPES; ++k) {
        for (l = 0; l < COEF_BANDS; ++l) {
          for (m = 0; m < COEFF_CONTEXTS; ++m) {
            output_counts->eob_branch[i][j][k][l][m] =
                input_counts->eob_branch[i][j][k][l][m];
            for (n = 0; n < UNCONSTRAINED_NODES + 1; ++n) {
              output_counts->coef[i][j][k][l][m][n] =
                  input_counts->coef[i][j][k][l][m][n];
            }
          }
        }
      }
    }
  }
  for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; ++i) {
    for (j = 0; j < SWITCHABLE_FILTERS; ++j) {
      output_counts->switchable_interp[i][j] =
          input_counts->switchable_interp[i][j];
    }
  }
  for (i = 0; i < INTER_MODE_CONTEXTS; ++i) {
    for (j = 0; j < INTER_MODES; ++j) {
      output_counts->inter_mode[i][j] = input_counts->inter_mode[i][j];
    }
  }
  for (i = 0; i < INTRA_INTER_CONTEXTS; ++i) {
    for (j = 0; j < 2; ++j) {
      output_counts->intra_inter[i][j] = input_counts->intra_inter[i][j];
    }
  }
  for (i = 0; i < COMP_INTER_CONTEXTS; ++i) {
    for (j = 0; j < 2; ++j) {
      output_counts->comp_inter[i][j] = input_counts->comp_inter[i][j];
    }
  }
  for (i = 0; i < REF_CONTEXTS; ++i) {
    for (j = 0; j < 2; ++j) {
      for (k = 0; k < 2; ++k) {
        output_counts->single_ref[i][j][k] = input_counts->single_ref[i][j][k];
      }
    }
  }
  for (i = 0; i < REF_CONTEXTS; ++i) {
    for (j = 0; j < 2; ++j) {
      output_counts->comp_ref[i][j] = input_counts->comp_ref[i][j];
    }
  }
  for (i = 0; i < SKIP_CONTEXTS; ++i) {
    for (j = 0; j < 2; ++j) {
      output_counts->skip[i][j] = input_counts->skip[i][j];
    }
  }
  for (i = 0; i < TX_SIZE_CONTEXTS; i++) {
    for (j = 0; j < TX_SIZES; j++) {
      output_counts->tx.p32x32[i][j] = input_counts->tx.p32x32[i][j];
    }
    for (j = 0; j < TX_SIZES - 1; j++) {
      output_counts->tx.p16x16[i][j] = input_counts->tx.p16x16[i][j];
    }
    for (j = 0; j < TX_SIZES - 2; j++) {
      output_counts->tx.p8x8[i][j] = input_counts->tx.p8x8[i][j];
    }
  }
  for (i = 0; i < TX_SIZES; i++) {
    output_counts->tx.tx_totals[i] = input_counts->tx.tx_totals[i];
  }
  for (i = 0; i < MV_JOINTS; i++) {
    output_counts->mv.joints[i] = input_counts->mv.joints[i];
  }
  for (k = 0; k < 2; k++) {
    nmv_component_counts *const comps = &output_counts->mv.comps[k];
    const nmv_component_counts *const comps_t = &input_counts->mv.comps[k];
    for (i = 0; i < 2; i++) {
      comps->sign[i] = comps_t->sign[i];
      comps->class0_hp[i] = comps_t->class0_hp[i];
      comps->hp[i] = comps_t->hp[i];
    }
    for (i = 0; i < MV_CLASSES; i++) {
      comps->classes[i] = comps_t->classes[i];
    }
    for (i = 0; i < CLASS0_SIZE; i++) {
      comps->class0[i] = comps_t->class0[i];
      for (j = 0; j < MV_FP_SIZE; j++) {
        comps->class0_fp[i][j] = comps_t->class0_fp[i][j];
      }
    }
    for (i = 0; i < MV_OFFSET_BITS; i++) {
      for (j = 0; j < 2; j++) {
        comps->bits[i][j] = comps_t->bits[i][j];
      }
    }
    for (i = 0; i < MV_FP_SIZE; i++) {
      comps->fp[i] = comps_t->fp[i];
    }
  }
}

static void yv12_buffer_to_image_buffer(const YV12_BUFFER_CONFIG *yv12_buffer,
                                        IMAGE_BUFFER *image_buffer) {
  const uint8_t *src_buf_ls[3] = { yv12_buffer->y_buffer, yv12_buffer->u_buffer,
                                   yv12_buffer->v_buffer };
  const int src_stride_ls[3] = { yv12_buffer->y_stride, yv12_buffer->uv_stride,
                                 yv12_buffer->uv_stride };
  const int w_ls[3] = { yv12_buffer->y_crop_width, yv12_buffer->uv_crop_width,
                        yv12_buffer->uv_crop_width };
  const int h_ls[3] = { yv12_buffer->y_crop_height, yv12_buffer->uv_crop_height,
                        yv12_buffer->uv_crop_height };
  int plane;
  for (plane = 0; plane < 3; ++plane) {
    const int src_stride = src_stride_ls[plane];
    const int w = w_ls[plane];
    const int h = h_ls[plane];
    const uint8_t *src_buf = src_buf_ls[plane];
    uint8_t *dst_buf = image_buffer->plane_buffer[plane];
    int r;
    assert(image_buffer->plane_width[plane] == w);
    assert(image_buffer->plane_height[plane] == h);
    for (r = 0; r < h; ++r) {
      memcpy(dst_buf, src_buf, sizeof(*src_buf) * w);
      src_buf += src_stride;
      dst_buf += w;
    }
  }
}
#endif  // CONFIG_RATE_CTRL

static void update_encode_frame_result(
    int ref_frame_flags, FRAME_UPDATE_TYPE update_type,
    const YV12_BUFFER_CONFIG *source_frame, const RefCntBuffer *coded_frame_buf,
    RefCntBuffer *ref_frame_bufs[MAX_INTER_REF_FRAMES], int quantize_index,
    uint32_t bit_depth, uint32_t input_bit_depth, const FRAME_COUNTS *counts,
#if CONFIG_RATE_CTRL
    const PARTITION_INFO *partition_info,
    const MOTION_VECTOR_INFO *motion_vector_info,
    const TplDepStats *tpl_stats_info,
#endif  // CONFIG_RATE_CTRL
    ENCODE_FRAME_RESULT *encode_frame_result) {
#if CONFIG_RATE_CTRL
  PSNR_STATS psnr;
#if CONFIG_VP9_HIGHBITDEPTH
  vpx_calc_highbd_psnr(source_frame, &coded_frame_buf->buf, &psnr, bit_depth,
                       input_bit_depth);
#else   // CONFIG_VP9_HIGHBITDEPTH
  (void)bit_depth;
  (void)input_bit_depth;
  vpx_calc_psnr(source_frame, &coded_frame_buf->buf, &psnr);
#endif  // CONFIG_VP9_HIGHBITDEPTH
  encode_frame_result->frame_coding_index = coded_frame_buf->frame_coding_index;

  vp9_get_ref_frame_info(update_type, ref_frame_flags, ref_frame_bufs,
                         encode_frame_result->ref_frame_coding_indexes,
                         encode_frame_result->ref_frame_valid_list);

  encode_frame_result->psnr = psnr.psnr[0];
  encode_frame_result->sse = psnr.sse[0];
  copy_frame_counts(counts, &encode_frame_result->frame_counts);
  encode_frame_result->partition_info = partition_info;
  encode_frame_result->motion_vector_info = motion_vector_info;
  encode_frame_result->tpl_stats_info = tpl_stats_info;
  if (encode_frame_result->coded_frame.allocated) {
    yv12_buffer_to_image_buffer(&coded_frame_buf->buf,
                                &encode_frame_result->coded_frame);
  }
#else   // CONFIG_RATE_CTRL
  (void)ref_frame_flags;
  (void)bit_depth;
  (void)input_bit_depth;
  (void)source_frame;
  (void)coded_frame_buf;
  (void)ref_frame_bufs;
  (void)counts;
#endif  // CONFIG_RATE_CTRL
  encode_frame_result->show_idx = coded_frame_buf->frame_index;
  encode_frame_result->update_type = update_type;
  encode_frame_result->quantize_index = quantize_index;
}
#endif  // !CONFIG_REALTIME_ONLY

void vp9_init_encode_frame_result(ENCODE_FRAME_RESULT *encode_frame_result) {
  encode_frame_result->show_idx = -1;  // Actual encoding doesn't happen.
#if CONFIG_RATE_CTRL
  encode_frame_result->frame_coding_index = -1;
  vp9_zero(encode_frame_result->coded_frame);
  encode_frame_result->coded_frame.allocated = 0;
  init_rq_history(&encode_frame_result->rq_history);
#endif  // CONFIG_RATE_CTRL
}

int vp9_get_compressed_data(VP9_COMP *cpi, unsigned int *frame_flags,
                            size_t *size, uint8_t *dest, int64_t *time_stamp,
                            int64_t *time_end, int flush,
                            ENCODE_FRAME_RESULT *encode_frame_result) {
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  VP9_COMMON *const cm = &cpi->common;
  BufferPool *const pool = cm->buffer_pool;
  RATE_CONTROL *const rc = &cpi->rc;
  struct vpx_usec_timer cmptimer;
  YV12_BUFFER_CONFIG *force_src_buffer = NULL;
  struct lookahead_entry *last_source = NULL;
  struct lookahead_entry *source = NULL;
  int arf_src_index;
  const int gf_group_index = cpi->twopass.gf_group.index;
  int i;

  if (is_one_pass_cbr_svc(cpi)) {
    vp9_one_pass_cbr_svc_start_layer(cpi);
  }

  vpx_usec_timer_start(&cmptimer);

  vp9_set_high_precision_mv(cpi, ALTREF_HIGH_PRECISION_MV);

  // Is multi-arf enabled.
  // Note that at the moment multi_arf is only configured for 2 pass VBR and
  // will not work properly with svc.
  // Enable the Jingning's new "multi_layer_arf" code if "enable_auto_arf"
  // is greater than or equal to 2.
  if ((oxcf->pass == 2) && !cpi->use_svc && (cpi->oxcf.enable_auto_arf >= 2))
    cpi->multi_layer_arf = 1;
  else
    cpi->multi_layer_arf = 0;

  // Normal defaults
  cm->reset_frame_context = 0;
  cm->refresh_frame_context = 1;
  if (!is_one_pass_cbr_svc(cpi)) {
    cpi->refresh_last_frame = 1;
    cpi->refresh_golden_frame = 0;
    cpi->refresh_alt_ref_frame = 0;
  }

  // Should we encode an arf frame.
  arf_src_index = get_arf_src_index(cpi);

  if (arf_src_index) {
    for (i = 0; i <= arf_src_index; ++i) {
      struct lookahead_entry *e = vp9_lookahead_peek(cpi->lookahead, i);
      // Avoid creating an alt-ref if there's a forced keyframe pending.
      if (e == NULL) {
        break;
      } else if (e->flags == VPX_EFLAG_FORCE_KF) {
        arf_src_index = 0;
        flush = 1;
        break;
      }
    }
  }

  // Clear arf index stack before group of pictures processing starts.
  if (gf_group_index == 1) {
    stack_init(cpi->twopass.gf_group.arf_index_stack, MAX_LAG_BUFFERS * 2);
    cpi->twopass.gf_group.stack_size = 0;
  }

  if (arf_src_index) {
    assert(arf_src_index <= rc->frames_to_key);
    if ((source = vp9_lookahead_peek(cpi->lookahead, arf_src_index)) != NULL) {
      cpi->alt_ref_source = source;

#if !CONFIG_REALTIME_ONLY
      if ((oxcf->mode != REALTIME) && (oxcf->arnr_max_frames > 0) &&
          (oxcf->arnr_strength > 0)) {
        int bitrate = cpi->rc.avg_frame_bandwidth / 40;
        int not_low_bitrate = bitrate > ALT_REF_AQ_LOW_BITRATE_BOUNDARY;

        int not_last_frame = (cpi->lookahead->sz - arf_src_index > 1);
        not_last_frame |= ALT_REF_AQ_APPLY_TO_LAST_FRAME;

        // Produce the filtered ARF frame.
        vp9_temporal_filter(cpi, arf_src_index);
        vpx_extend_frame_borders(&cpi->alt_ref_buffer);

        // for small bitrates segmentation overhead usually
        // eats all bitrate gain from enabling delta quantizers
        if (cpi->oxcf.alt_ref_aq != 0 && not_low_bitrate && not_last_frame)
          vp9_alt_ref_aq_setup_mode(cpi->alt_ref_aq, cpi);

        force_src_buffer = &cpi->alt_ref_buffer;
      }
#endif
      cm->show_frame = 0;
      cm->intra_only = 0;
      cpi->refresh_alt_ref_frame = 1;
      cpi->refresh_golden_frame = 0;
      cpi->refresh_last_frame = 0;
      rc->is_src_frame_alt_ref = 0;
      rc->source_alt_ref_pending = 0;
    } else {
      rc->source_alt_ref_pending = 0;
    }
  }

  if (!source) {
    // Get last frame source.
    if (cm->current_video_frame > 0) {
      if ((last_source = vp9_lookahead_peek(cpi->lookahead, -1)) == NULL)
        return -1;
    }

    // Read in the source frame.
    if (cpi->use_svc || cpi->svc.set_intra_only_frame)
      source = vp9_svc_lookahead_pop(cpi, cpi->lookahead, flush);
    else
      source = vp9_lookahead_pop(cpi->lookahead, flush);

    if (source != NULL) {
      cm->show_frame = 1;
      cm->intra_only = 0;
      // If the flags indicate intra frame, but if the current picture is for
      // spatial layer above first_spatial_layer_to_encode, it should not be an
      // intra picture.
      if ((source->flags & VPX_EFLAG_FORCE_KF) && cpi->use_svc &&
          cpi->svc.spatial_layer_id > cpi->svc.first_spatial_layer_to_encode) {
        source->flags &= ~(unsigned int)(VPX_EFLAG_FORCE_KF);
      }

      // Check to see if the frame should be encoded as an arf overlay.
      check_src_altref(cpi, source);
    }
  }

  if (source) {
    cpi->un_scaled_source = cpi->Source =
        force_src_buffer ? force_src_buffer : &source->img;

#ifdef ENABLE_KF_DENOISE
    // Copy of raw source for metrics calculation.
    if (is_psnr_calc_enabled(cpi))
      vp9_copy_and_extend_frame(cpi->Source, &cpi->raw_unscaled_source);
#endif

    cpi->unscaled_last_source = last_source != NULL ? &last_source->img : NULL;

    *time_stamp = source->ts_start;
    *time_end = source->ts_end;
    *frame_flags = (source->flags & VPX_EFLAG_FORCE_KF) ? FRAMEFLAGS_KEY : 0;
  } else {
    *size = 0;
    return -1;
  }

  if (source->ts_start < cpi->first_time_stamp_ever) {
    cpi->first_time_stamp_ever = source->ts_start;
    cpi->last_end_time_stamp_seen = source->ts_start;
  }

  // Clear down mmx registers
  vpx_clear_system_state();

  // adjust frame rates based on timestamps given
  if (cm->show_frame) {
    if (cpi->use_svc && cpi->svc.use_set_ref_frame_config &&
        cpi->svc.duration[cpi->svc.spatial_layer_id] > 0)
      vp9_svc_adjust_frame_rate(cpi);
    else
      adjust_frame_rate(cpi, source);
  }

  if (is_one_pass_cbr_svc(cpi)) {
    vp9_update_temporal_layer_framerate(cpi);
    vp9_restore_layer_context(cpi);
  }

  // Find a free buffer for the new frame, releasing the reference previously
  // held.
  if (cm->new_fb_idx != INVALID_IDX) {
    --pool->frame_bufs[cm->new_fb_idx].ref_count;
  }
  cm->new_fb_idx = get_free_fb(cm);

  if (cm->new_fb_idx == INVALID_IDX) return -1;
  cm->cur_frame = &pool->frame_bufs[cm->new_fb_idx];
  // If the frame buffer for current frame is the same as previous frame, MV in
  // the base layer shouldn't be used as it'll cause data race.
  if (cpi->svc.spatial_layer_id > 0 && cm->cur_frame == cm->prev_frame) {
    cpi->svc.use_base_mv = 0;
  }
  // Start with a 0 size frame.
  *size = 0;

  cpi->frame_flags = *frame_flags;

#if !CONFIG_REALTIME_ONLY
  if ((oxcf->pass == 2) && !cpi->use_svc) {
    vp9_rc_get_second_pass_params(cpi);
  } else if (oxcf->pass == 1) {
    set_frame_size(cpi);
  }
#endif  // !CONFIG_REALTIME_ONLY

  if (oxcf->pass != 1 && cpi->level_constraint.level_index >= 0 &&
      cpi->level_constraint.fail_flag == 0)
    level_rc_framerate(cpi, arf_src_index);

  if (cpi->oxcf.pass != 0 || cpi->use_svc || frame_is_intra_only(cm) == 1) {
    for (i = 0; i < REFS_PER_FRAME; ++i) cpi->scaled_ref_idx[i] = INVALID_IDX;
  }

  if (cpi->kmeans_data_arr_alloc == 0) {
    const int mi_cols = mi_cols_aligned_to_sb(cm->mi_cols);
    const int mi_rows = mi_cols_aligned_to_sb(cm->mi_rows);
#if CONFIG_MULTITHREAD
    pthread_mutex_init(&cpi->kmeans_mutex, NULL);
#endif
    CHECK_MEM_ERROR(
        cm, cpi->kmeans_data_arr,
        vpx_calloc(mi_rows * mi_cols, sizeof(*cpi->kmeans_data_arr)));
    cpi->kmeans_data_stride = mi_cols;
    cpi->kmeans_data_arr_alloc = 1;
  }

#if CONFIG_NON_GREEDY_MV
  {
    const int mi_cols = mi_cols_aligned_to_sb(cm->mi_cols);
    const int mi_rows = mi_cols_aligned_to_sb(cm->mi_rows);
    Status status = vp9_alloc_motion_field_info(
        &cpi->motion_field_info, MAX_ARF_GOP_SIZE, mi_rows, mi_cols);
    if (status == STATUS_FAILED) {
      vpx_internal_error(&(cm)->error, VPX_CODEC_MEM_ERROR,
                         "vp9_alloc_motion_field_info failed");
    }
  }
#endif  // CONFIG_NON_GREEDY_MV

  if (gf_group_index == 1 &&
      cpi->twopass.gf_group.update_type[gf_group_index] == ARF_UPDATE &&
      cpi->sf.enable_tpl_model) {
    init_tpl_buffer(cpi);
    vp9_estimate_qp_gop(cpi);
    setup_tpl_stats(cpi);
  }

#if CONFIG_BITSTREAM_DEBUG
  assert(cpi->oxcf.max_threads == 0 &&
         "bitstream debug tool does not support multithreading");
  bitstream_queue_record_write();
#endif
#if CONFIG_BITSTREAM_DEBUG || CONFIG_MISMATCH_DEBUG
  bitstream_queue_set_frame_write(cm->current_video_frame * 2 + cm->show_frame);
#endif

  cpi->td.mb.fp_src_pred = 0;
#if CONFIG_REALTIME_ONLY
  (void)encode_frame_result;
  if (cpi->use_svc) {
    SvcEncode(cpi, size, dest, frame_flags);
  } else {
    // One pass encode
    Pass0Encode(cpi, size, dest, frame_flags);
  }
#else  // !CONFIG_REALTIME_ONLY
  if (oxcf->pass == 1 && !cpi->use_svc) {
    const int lossless = is_lossless_requested(oxcf);
#if CONFIG_VP9_HIGHBITDEPTH
    if (cpi->oxcf.use_highbitdepth)
      cpi->td.mb.fwd_txfm4x4 =
          lossless ? vp9_highbd_fwht4x4 : vpx_highbd_fdct4x4;
    else
      cpi->td.mb.fwd_txfm4x4 = lossless ? vp9_fwht4x4 : vpx_fdct4x4;
    cpi->td.mb.highbd_inv_txfm_add =
        lossless ? vp9_highbd_iwht4x4_add : vp9_highbd_idct4x4_add;
#else
    cpi->td.mb.fwd_txfm4x4 = lossless ? vp9_fwht4x4 : vpx_fdct4x4;
#endif  // CONFIG_VP9_HIGHBITDEPTH
    cpi->td.mb.inv_txfm_add = lossless ? vp9_iwht4x4_add : vp9_idct4x4_add;
    vp9_first_pass(cpi, source);
  } else if (oxcf->pass == 2 && !cpi->use_svc) {
    Pass2Encode(cpi, size, dest, frame_flags, encode_frame_result);
    vp9_twopass_postencode_update(cpi);
  } else if (cpi->use_svc) {
    SvcEncode(cpi, size, dest, frame_flags);
  } else {
    // One pass encode
    Pass0Encode(cpi, size, dest, frame_flags);
  }
#endif  // CONFIG_REALTIME_ONLY

  if (cm->show_frame) cm->cur_show_frame_fb_idx = cm->new_fb_idx;

  if (cm->refresh_frame_context)
    cm->frame_contexts[cm->frame_context_idx] = *cm->fc;

  // No frame encoded, or frame was dropped, release scaled references.
  if ((*size == 0) && (frame_is_intra_only(cm) == 0)) {
    release_scaled_references(cpi);
  }

  if (*size > 0) {
    cpi->droppable = !frame_is_reference(cpi);
  }

  // Save layer specific state.
  if (is_one_pass_cbr_svc(cpi) || ((cpi->svc.number_temporal_layers > 1 ||
                                    cpi->svc.number_spatial_layers > 1) &&
                                   oxcf->pass == 2)) {
    vp9_save_layer_context(cpi);
  }

  vpx_usec_timer_mark(&cmptimer);
  cpi->time_compress_data += vpx_usec_timer_elapsed(&cmptimer);

  if (cpi->keep_level_stats && oxcf->pass != 1)
    update_level_info(cpi, size, arf_src_index);

#if CONFIG_INTERNAL_STATS

  if (oxcf->pass != 1) {
    double samples = 0.0;
    cpi->bytes += (int)(*size);

    if (cm->show_frame) {
      uint32_t bit_depth = 8;
      uint32_t in_bit_depth = 8;
      cpi->count++;
#if CONFIG_VP9_HIGHBITDEPTH
      if (cm->use_highbitdepth) {
        in_bit_depth = cpi->oxcf.input_bit_depth;
        bit_depth = cm->bit_depth;
      }
#endif

      if (cpi->b_calculate_psnr) {
        YV12_BUFFER_CONFIG *orig = cpi->raw_source_frame;
        YV12_BUFFER_CONFIG *recon = cpi->common.frame_to_show;
        YV12_BUFFER_CONFIG *pp = &cm->post_proc_buffer;
        PSNR_STATS psnr;
#if CONFIG_VP9_HIGHBITDEPTH
        vpx_calc_highbd_psnr(orig, recon, &psnr, cpi->td.mb.e_mbd.bd,
                             in_bit_depth);
#else
        vpx_calc_psnr(orig, recon, &psnr);
#endif  // CONFIG_VP9_HIGHBITDEPTH

        adjust_image_stat(psnr.psnr[1], psnr.psnr[2], psnr.psnr[3],
                          psnr.psnr[0], &cpi->psnr);
        cpi->total_sq_error += psnr.sse[0];
        cpi->total_samples += psnr.samples[0];
        samples = psnr.samples[0];

        {
          PSNR_STATS psnr2;
          double frame_ssim2 = 0, weight = 0;
#if CONFIG_VP9_POSTPROC
          if (vpx_alloc_frame_buffer(
                  pp, recon->y_crop_width, recon->y_crop_height,
                  cm->subsampling_x, cm->subsampling_y,
#if CONFIG_VP9_HIGHBITDEPTH
                  cm->use_highbitdepth,
#endif
                  VP9_ENC_BORDER_IN_PIXELS, cm->byte_alignment) < 0) {
            vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                               "Failed to allocate post processing buffer");
          }
          {
            vp9_ppflags_t ppflags;
            ppflags.post_proc_flag = VP9D_DEBLOCK;
            ppflags.deblocking_level = 0;  // not used in vp9_post_proc_frame()
            ppflags.noise_level = 0;       // not used in vp9_post_proc_frame()
            vp9_post_proc_frame(cm, pp, &ppflags,
                                cpi->un_scaled_source->y_width);
          }
#endif
          vpx_clear_system_state();

#if CONFIG_VP9_HIGHBITDEPTH
          vpx_calc_highbd_psnr(orig, pp, &psnr2, cpi->td.mb.e_mbd.bd,
                               cpi->oxcf.input_bit_depth);
#else
          vpx_calc_psnr(orig, pp, &psnr2);
#endif  // CONFIG_VP9_HIGHBITDEPTH

          cpi->totalp_sq_error += psnr2.sse[0];
          cpi->totalp_samples += psnr2.samples[0];
          adjust_image_stat(psnr2.psnr[1], psnr2.psnr[2], psnr2.psnr[3],
                            psnr2.psnr[0], &cpi->psnrp);

#if CONFIG_VP9_HIGHBITDEPTH
          if (cm->use_highbitdepth) {
            frame_ssim2 = vpx_highbd_calc_ssim(orig, recon, &weight, bit_depth,
                                               in_bit_depth);
          } else {
            frame_ssim2 = vpx_calc_ssim(orig, recon, &weight);
          }
#else
          frame_ssim2 = vpx_calc_ssim(orig, recon, &weight);
#endif  // CONFIG_VP9_HIGHBITDEPTH

          cpi->worst_ssim = VPXMIN(cpi->worst_ssim, frame_ssim2);
          cpi->summed_quality += frame_ssim2 * weight;
          cpi->summed_weights += weight;

#if CONFIG_VP9_HIGHBITDEPTH
          if (cm->use_highbitdepth) {
            frame_ssim2 = vpx_highbd_calc_ssim(orig, pp, &weight, bit_depth,
                                               in_bit_depth);
          } else {
            frame_ssim2 = vpx_calc_ssim(orig, pp, &weight);
          }
#else
          frame_ssim2 = vpx_calc_ssim(orig, pp, &weight);
#endif  // CONFIG_VP9_HIGHBITDEPTH

          cpi->summedp_quality += frame_ssim2 * weight;
          cpi->summedp_weights += weight;
#if 0
          if (cm->show_frame) {
            FILE *f = fopen("q_used.stt", "a");
            fprintf(f, "%5d : Y%f7.3:U%f7.3:V%f7.3:F%f7.3:S%7.3f\n",
                    cpi->common.current_video_frame, psnr2.psnr[1],
                    psnr2.psnr[2], psnr2.psnr[3], psnr2.psnr[0], frame_ssim2);
            fclose(f);
          }
#endif
        }
      }
      if (cpi->b_calculate_blockiness) {
#if CONFIG_VP9_HIGHBITDEPTH
        if (!cm->use_highbitdepth)
#endif
        {
          double frame_blockiness = vp9_get_blockiness(
              cpi->Source->y_buffer, cpi->Source->y_stride,
              cm->frame_to_show->y_buffer, cm->frame_to_show->y_stride,
              cpi->Source->y_width, cpi->Source->y_height);
          cpi->worst_blockiness =
              VPXMAX(cpi->worst_blockiness, frame_blockiness);
          cpi->total_blockiness += frame_blockiness;
        }
      }

      if (cpi->b_calculate_consistency) {
#if CONFIG_VP9_HIGHBITDEPTH
        if (!cm->use_highbitdepth)
#endif
        {
          double this_inconsistency = vpx_get_ssim_metrics(
              cpi->Source->y_buffer, cpi->Source->y_stride,
              cm->frame_to_show->y_buffer, cm->frame_to_show->y_stride,
              cpi->Source->y_width, cpi->Source->y_height, cpi->ssim_vars,
              &cpi->metrics, 1);

          const double peak = (double)((1 << cpi->oxcf.input_bit_depth) - 1);
          double consistency =
              vpx_sse_to_psnr(samples, peak, (double)cpi->total_inconsistency);
          if (consistency > 0.0)
            cpi->worst_consistency =
                VPXMIN(cpi->worst_consistency, consistency);
          cpi->total_inconsistency += this_inconsistency;
        }
      }

      {
        double y, u, v, frame_all;
        frame_all = vpx_calc_fastssim(cpi->Source, cm->frame_to_show, &y, &u,
                                      &v, bit_depth, in_bit_depth);
        adjust_image_stat(y, u, v, frame_all, &cpi->fastssim);
      }
      {
        double y, u, v, frame_all;
        frame_all = vpx_psnrhvs(cpi->Source, cm->frame_to_show, &y, &u, &v,
                                bit_depth, in_bit_depth);
        adjust_image_stat(y, u, v, frame_all, &cpi->psnrhvs);
      }
    }
  }

#endif

  if (is_one_pass_cbr_svc(cpi)) {
    if (cm->show_frame) {
      ++cpi->svc.spatial_layer_to_encode;
      if (cpi->svc.spatial_layer_to_encode >= cpi->svc.number_spatial_layers)
        cpi->svc.spatial_layer_to_encode = 0;
    }
  }

  vpx_clear_system_state();
  return 0;
}

int vp9_get_preview_raw_frame(VP9_COMP *cpi, YV12_BUFFER_CONFIG *dest,
                              vp9_ppflags_t *flags) {
  VP9_COMMON *cm = &cpi->common;
#if !CONFIG_VP9_POSTPROC
  (void)flags;
#endif

  if (!cm->show_frame) {
    return -1;
  } else {
    int ret;
#if CONFIG_VP9_POSTPROC
    ret = vp9_post_proc_frame(cm, dest, flags, cpi->un_scaled_source->y_width);
#else
    if (cm->frame_to_show) {
      *dest = *cm->frame_to_show;
      dest->y_width = cm->width;
      dest->y_height = cm->height;
      dest->uv_width = cm->width >> cm->subsampling_x;
      dest->uv_height = cm->height >> cm->subsampling_y;
      ret = 0;
    } else {
      ret = -1;
    }
#endif  // !CONFIG_VP9_POSTPROC
    vpx_clear_system_state();
    return ret;
  }
}

int vp9_set_internal_size(VP9_COMP *cpi, VPX_SCALING horiz_mode,
                          VPX_SCALING vert_mode) {
  VP9_COMMON *cm = &cpi->common;
  int hr = 0, hs = 0, vr = 0, vs = 0;

  if (horiz_mode > ONETWO || vert_mode > ONETWO) return -1;

  Scale2Ratio(horiz_mode, &hr, &hs);
  Scale2Ratio(vert_mode, &vr, &vs);

  // always go to the next whole number
  cm->width = (hs - 1 + cpi->oxcf.width * hr) / hs;
  cm->height = (vs - 1 + cpi->oxcf.height * vr) / vs;
  if (cm->current_video_frame) {
    assert(cm->width <= cpi->initial_width);
    assert(cm->height <= cpi->initial_height);
  }

  update_frame_size(cpi);

  return 0;
}

int vp9_set_size_literal(VP9_COMP *cpi, unsigned int width,
                         unsigned int height) {
  VP9_COMMON *cm = &cpi->common;
#if CONFIG_VP9_HIGHBITDEPTH
  update_initial_width(cpi, cm->use_highbitdepth, 1, 1);
#else
  update_initial_width(cpi, 0, 1, 1);
#endif  // CONFIG_VP9_HIGHBITDEPTH

#if CONFIG_VP9_TEMPORAL_DENOISING
  setup_denoiser_buffer(cpi);
#endif
  alloc_raw_frame_buffers(cpi);
  if (width) {
    cm->width = width;
    if (cm->width > cpi->initial_width) {
      cm->width = cpi->initial_width;
      printf("Warning: Desired width too large, changed to %d\n", cm->width);
    }
  }

  if (height) {
    cm->height = height;
    if (cm->height > cpi->initial_height) {
      cm->height = cpi->initial_height;
      printf("Warning: Desired height too large, changed to %d\n", cm->height);
    }
  }
  assert(cm->width <= cpi->initial_width);
  assert(cm->height <= cpi->initial_height);

  update_frame_size(cpi);

  return 0;
}

void vp9_set_svc(VP9_COMP *cpi, int use_svc) {
  cpi->use_svc = use_svc;
  return;
}

int vp9_get_quantizer(const VP9_COMP *cpi) { return cpi->common.base_qindex; }

void vp9_apply_encoding_flags(VP9_COMP *cpi, vpx_enc_frame_flags_t flags) {
  if (flags &
      (VP8_EFLAG_NO_REF_LAST | VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF)) {
    int ref = 7;

    if (flags & VP8_EFLAG_NO_REF_LAST) ref ^= VP9_LAST_FLAG;

    if (flags & VP8_EFLAG_NO_REF_GF) ref ^= VP9_GOLD_FLAG;

    if (flags & VP8_EFLAG_NO_REF_ARF) ref ^= VP9_ALT_FLAG;

    vp9_use_as_reference(cpi, ref);
  }

  if (flags &
      (VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
       VP8_EFLAG_FORCE_GF | VP8_EFLAG_FORCE_ARF)) {
    int upd = 7;

    if (flags & VP8_EFLAG_NO_UPD_LAST) upd ^= VP9_LAST_FLAG;

    if (flags & VP8_EFLAG_NO_UPD_GF) upd ^= VP9_GOLD_FLAG;

    if (flags & VP8_EFLAG_NO_UPD_ARF) upd ^= VP9_ALT_FLAG;

    vp9_update_reference(cpi, upd);
  }

  if (flags & VP8_EFLAG_NO_UPD_ENTROPY) {
    vp9_update_entropy(cpi, 0);
  }
}

void vp9_set_row_mt(VP9_COMP *cpi) {
  // Enable row based multi-threading for supported modes of encoding
  cpi->row_mt = 0;
  if (((cpi->oxcf.mode == GOOD || cpi->oxcf.mode == BEST) &&
       cpi->oxcf.speed < 5 && cpi->oxcf.pass == 1) &&
      cpi->oxcf.row_mt && !cpi->use_svc)
    cpi->row_mt = 1;

  if (cpi->oxcf.mode == GOOD && cpi->oxcf.speed < 5 &&
      (cpi->oxcf.pass == 0 || cpi->oxcf.pass == 2) && cpi->oxcf.row_mt &&
      !cpi->use_svc)
    cpi->row_mt = 1;

  // In realtime mode, enable row based multi-threading for all the speed levels
  // where non-rd path is used.
  if (cpi->oxcf.mode == REALTIME && cpi->oxcf.speed >= 5 && cpi->oxcf.row_mt) {
    cpi->row_mt = 1;
  }

  if (cpi->row_mt)
    cpi->row_mt_bit_exact = 1;
  else
    cpi->row_mt_bit_exact = 0;
}
