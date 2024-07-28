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

#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>

#include "config/aom_scale_rtcd.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/binary_codes_writer.h"
#include "aom_dsp/mathutils.h"
#include "aom_dsp/psnr.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "av1/common/av1_common_int.h"
#include "av1/common/quant_common.h"
#include "av1/common/restoration.h"

#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/picklpf.h"
#include "av1/encoder/pickrst.h"

// Number of Wiener iterations
#define NUM_WIENER_ITERS 5

// Penalty factor for use of dual sgr
#define DUAL_SGR_PENALTY_MULT 0.01

// Working precision for Wiener filter coefficients
#define WIENER_TAP_SCALE_FACTOR ((int64_t)1 << 16)

#define SGRPROJ_EP_GRP1_START_IDX 0
#define SGRPROJ_EP_GRP1_END_IDX 9
#define SGRPROJ_EP_GRP1_SEARCH_COUNT 4
#define SGRPROJ_EP_GRP2_3_SEARCH_COUNT 2
static const int sgproj_ep_grp1_seed[SGRPROJ_EP_GRP1_SEARCH_COUNT] = { 0, 3, 6,
                                                                       9 };
static const int sgproj_ep_grp2_3[SGRPROJ_EP_GRP2_3_SEARCH_COUNT][14] = {
  { 10, 10, 11, 11, 12, 12, 13, 13, 13, 13, -1, -1, -1, -1 },
  { 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15 }
};

#if DEBUG_LR_COSTING
RestorationUnitInfo lr_ref_params[RESTORE_TYPES][MAX_MB_PLANE]
                                 [MAX_LR_UNITS_W * MAX_LR_UNITS_H];
#endif  // DEBUG_LR_COSTING

typedef int64_t (*sse_extractor_type)(const YV12_BUFFER_CONFIG *a,
                                      const YV12_BUFFER_CONFIG *b);
typedef int64_t (*sse_part_extractor_type)(const YV12_BUFFER_CONFIG *a,
                                           const YV12_BUFFER_CONFIG *b,
                                           int hstart, int width, int vstart,
                                           int height);
typedef uint64_t (*var_part_extractor_type)(const YV12_BUFFER_CONFIG *a,
                                            int hstart, int width, int vstart,
                                            int height);

#if CONFIG_AV1_HIGHBITDEPTH
#define NUM_EXTRACTORS (3 * (1 + 1))
#else
#define NUM_EXTRACTORS 3
#endif
static const sse_part_extractor_type sse_part_extractors[NUM_EXTRACTORS] = {
  aom_get_y_sse_part,        aom_get_u_sse_part,
  aom_get_v_sse_part,
#if CONFIG_AV1_HIGHBITDEPTH
  aom_highbd_get_y_sse_part, aom_highbd_get_u_sse_part,
  aom_highbd_get_v_sse_part,
#endif
};
static const var_part_extractor_type var_part_extractors[NUM_EXTRACTORS] = {
  aom_get_y_var,        aom_get_u_var,        aom_get_v_var,
#if CONFIG_AV1_HIGHBITDEPTH
  aom_highbd_get_y_var, aom_highbd_get_u_var, aom_highbd_get_v_var,
#endif
};

static int64_t sse_restoration_unit(const RestorationTileLimits *limits,
                                    const YV12_BUFFER_CONFIG *src,
                                    const YV12_BUFFER_CONFIG *dst, int plane,
                                    int highbd) {
  return sse_part_extractors[3 * highbd + plane](
      src, dst, limits->h_start, limits->h_end - limits->h_start,
      limits->v_start, limits->v_end - limits->v_start);
}

static uint64_t var_restoration_unit(const RestorationTileLimits *limits,
                                     const YV12_BUFFER_CONFIG *src, int plane,
                                     int highbd) {
  return var_part_extractors[3 * highbd + plane](
      src, limits->h_start, limits->h_end - limits->h_start, limits->v_start,
      limits->v_end - limits->v_start);
}

typedef struct {
  const YV12_BUFFER_CONFIG *src;
  YV12_BUFFER_CONFIG *dst;

  const AV1_COMMON *cm;
  const MACROBLOCK *x;
  int plane;
  int plane_w;
  int plane_h;
  RestUnitSearchInfo *rusi;

  // Speed features
  const LOOP_FILTER_SPEED_FEATURES *lpf_sf;

  uint8_t *dgd_buffer;
  int dgd_stride;
  const uint8_t *src_buffer;
  int src_stride;

  // SSE values for each restoration mode for the current RU
  // These are saved by each search function for use in search_switchable()
  int64_t sse[RESTORE_SWITCHABLE_TYPES];

  // This flag will be set based on the speed feature
  // 'prune_sgr_based_on_wiener'. 0 implies no pruning and 1 implies pruning.
  uint8_t skip_sgr_eval;

  // Total rate and distortion so far for each restoration type
  // These are initialised by reset_rsc in search_rest_type
  int64_t total_sse[RESTORE_TYPES];
  int64_t total_bits[RESTORE_TYPES];

  // Reference parameters for delta-coding
  //
  // For each restoration type, we need to store the latest parameter set which
  // has been used, so that we can properly cost up the next parameter set.
  // Note that we have two sets of these - one for the single-restoration-mode
  // search (ie, frame_restoration_type = RESTORE_WIENER or RESTORE_SGRPROJ)
  // and one for the switchable mode. This is because these two cases can lead
  // to different sets of parameters being signaled, but we don't know which
  // we will pick for sure until the end of the search process.
  WienerInfo ref_wiener;
  SgrprojInfo ref_sgrproj;
  WienerInfo switchable_ref_wiener;
  SgrprojInfo switchable_ref_sgrproj;

  // Buffers used to hold dgd-avg and src-avg data respectively during SIMD
  // call of Wiener filter.
  int16_t *dgd_avg;
  int16_t *src_avg;
} RestSearchCtxt;

static AOM_INLINE void rsc_on_tile(void *priv) {
  RestSearchCtxt *rsc = (RestSearchCtxt *)priv;
  set_default_wiener(&rsc->ref_wiener);
  set_default_sgrproj(&rsc->ref_sgrproj);
  set_default_wiener(&rsc->switchable_ref_wiener);
  set_default_sgrproj(&rsc->switchable_ref_sgrproj);
}

static AOM_INLINE void reset_rsc(RestSearchCtxt *rsc) {
  memset(rsc->total_sse, 0, sizeof(rsc->total_sse));
  memset(rsc->total_bits, 0, sizeof(rsc->total_bits));
}

static AOM_INLINE void init_rsc(const YV12_BUFFER_CONFIG *src,
                                const AV1_COMMON *cm, const MACROBLOCK *x,
                                const LOOP_FILTER_SPEED_FEATURES *lpf_sf,
                                int plane, RestUnitSearchInfo *rusi,
                                YV12_BUFFER_CONFIG *dst, RestSearchCtxt *rsc) {
  rsc->src = src;
  rsc->dst = dst;
  rsc->cm = cm;
  rsc->x = x;
  rsc->plane = plane;
  rsc->rusi = rusi;
  rsc->lpf_sf = lpf_sf;

  const YV12_BUFFER_CONFIG *dgd = &cm->cur_frame->buf;
  const int is_uv = plane != AOM_PLANE_Y;
  int plane_w, plane_h;
  av1_get_upsampled_plane_size(cm, is_uv, &plane_w, &plane_h);
  assert(plane_w == src->crop_widths[is_uv]);
  assert(plane_h == src->crop_heights[is_uv]);
  assert(src->crop_widths[is_uv] == dgd->crop_widths[is_uv]);
  assert(src->crop_heights[is_uv] == dgd->crop_heights[is_uv]);

  rsc->plane_w = plane_w;
  rsc->plane_h = plane_h;
  rsc->src_buffer = src->buffers[plane];
  rsc->src_stride = src->strides[is_uv];
  rsc->dgd_buffer = dgd->buffers[plane];
  rsc->dgd_stride = dgd->strides[is_uv];
}

static int64_t try_restoration_unit(const RestSearchCtxt *rsc,
                                    const RestorationTileLimits *limits,
                                    const RestorationUnitInfo *rui) {
  const AV1_COMMON *const cm = rsc->cm;
  const int plane = rsc->plane;
  const int is_uv = plane > 0;
  const RestorationInfo *rsi = &cm->rst_info[plane];
  RestorationLineBuffers rlbs;
  const int bit_depth = cm->seq_params->bit_depth;
  const int highbd = cm->seq_params->use_highbitdepth;

  const YV12_BUFFER_CONFIG *fts = &cm->cur_frame->buf;
  // TODO(yunqing): For now, only use optimized LR filter in decoder. Can be
  // also used in encoder.
  const int optimized_lr = 0;

  av1_loop_restoration_filter_unit(
      limits, rui, &rsi->boundaries, &rlbs, rsc->plane_w, rsc->plane_h,
      is_uv && cm->seq_params->subsampling_x,
      is_uv && cm->seq_params->subsampling_y, highbd, bit_depth,
      fts->buffers[plane], fts->strides[is_uv], rsc->dst->buffers[plane],
      rsc->dst->strides[is_uv], cm->rst_tmpbuf, optimized_lr, cm->error);

  return sse_restoration_unit(limits, rsc->src, rsc->dst, plane, highbd);
}

int64_t av1_lowbd_pixel_proj_error_c(const uint8_t *src8, int width, int height,
                                     int src_stride, const uint8_t *dat8,
                                     int dat_stride, int32_t *flt0,
                                     int flt0_stride, int32_t *flt1,
                                     int flt1_stride, int xq[2],
                                     const sgr_params_type *params) {
  int i, j;
  const uint8_t *src = src8;
  const uint8_t *dat = dat8;
  int64_t err = 0;
  if (params->r[0] > 0 && params->r[1] > 0) {
    for (i = 0; i < height; ++i) {
      for (j = 0; j < width; ++j) {
        assert(flt1[j] < (1 << 15) && flt1[j] > -(1 << 15));
        assert(flt0[j] < (1 << 15) && flt0[j] > -(1 << 15));
        const int32_t u = (int32_t)(dat[j] << SGRPROJ_RST_BITS);
        int32_t v = u << SGRPROJ_PRJ_BITS;
        v += xq[0] * (flt0[j] - u) + xq[1] * (flt1[j] - u);
        const int32_t e =
            ROUND_POWER_OF_TWO(v, SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS) - src[j];
        err += ((int64_t)e * e);
      }
      dat += dat_stride;
      src += src_stride;
      flt0 += flt0_stride;
      flt1 += flt1_stride;
    }
  } else if (params->r[0] > 0) {
    for (i = 0; i < height; ++i) {
      for (j = 0; j < width; ++j) {
        assert(flt0[j] < (1 << 15) && flt0[j] > -(1 << 15));
        const int32_t u = (int32_t)(dat[j] << SGRPROJ_RST_BITS);
        int32_t v = u << SGRPROJ_PRJ_BITS;
        v += xq[0] * (flt0[j] - u);
        const int32_t e =
            ROUND_POWER_OF_TWO(v, SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS) - src[j];
        err += ((int64_t)e * e);
      }
      dat += dat_stride;
      src += src_stride;
      flt0 += flt0_stride;
    }
  } else if (params->r[1] > 0) {
    for (i = 0; i < height; ++i) {
      for (j = 0; j < width; ++j) {
        assert(flt1[j] < (1 << 15) && flt1[j] > -(1 << 15));
        const int32_t u = (int32_t)(dat[j] << SGRPROJ_RST_BITS);
        int32_t v = u << SGRPROJ_PRJ_BITS;
        v += xq[1] * (flt1[j] - u);
        const int32_t e =
            ROUND_POWER_OF_TWO(v, SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS) - src[j];
        err += ((int64_t)e * e);
      }
      dat += dat_stride;
      src += src_stride;
      flt1 += flt1_stride;
    }
  } else {
    for (i = 0; i < height; ++i) {
      for (j = 0; j < width; ++j) {
        const int32_t e = (int32_t)(dat[j]) - src[j];
        err += ((int64_t)e * e);
      }
      dat += dat_stride;
      src += src_stride;
    }
  }

  return err;
}

#if CONFIG_AV1_HIGHBITDEPTH
int64_t av1_highbd_pixel_proj_error_c(const uint8_t *src8, int width,
                                      int height, int src_stride,
                                      const uint8_t *dat8, int dat_stride,
                                      int32_t *flt0, int flt0_stride,
                                      int32_t *flt1, int flt1_stride, int xq[2],
                                      const sgr_params_type *params) {
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dat = CONVERT_TO_SHORTPTR(dat8);
  int i, j;
  int64_t err = 0;
  const int32_t half = 1 << (SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS - 1);
  if (params->r[0] > 0 && params->r[1] > 0) {
    int xq0 = xq[0];
    int xq1 = xq[1];
    for (i = 0; i < height; ++i) {
      for (j = 0; j < width; ++j) {
        const int32_t d = dat[j];
        const int32_t s = src[j];
        const int32_t u = (int32_t)(d << SGRPROJ_RST_BITS);
        int32_t v0 = flt0[j] - u;
        int32_t v1 = flt1[j] - u;
        int32_t v = half;
        v += xq0 * v0;
        v += xq1 * v1;
        const int32_t e = (v >> (SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS)) + d - s;
        err += ((int64_t)e * e);
      }
      dat += dat_stride;
      flt0 += flt0_stride;
      flt1 += flt1_stride;
      src += src_stride;
    }
  } else if (params->r[0] > 0 || params->r[1] > 0) {
    int exq;
    int32_t *flt;
    int flt_stride;
    if (params->r[0] > 0) {
      exq = xq[0];
      flt = flt0;
      flt_stride = flt0_stride;
    } else {
      exq = xq[1];
      flt = flt1;
      flt_stride = flt1_stride;
    }
    for (i = 0; i < height; ++i) {
      for (j = 0; j < width; ++j) {
        const int32_t d = dat[j];
        const int32_t s = src[j];
        const int32_t u = (int32_t)(d << SGRPROJ_RST_BITS);
        int32_t v = half;
        v += exq * (flt[j] - u);
        const int32_t e = (v >> (SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS)) + d - s;
        err += ((int64_t)e * e);
      }
      dat += dat_stride;
      flt += flt_stride;
      src += src_stride;
    }
  } else {
    for (i = 0; i < height; ++i) {
      for (j = 0; j < width; ++j) {
        const int32_t d = dat[j];
        const int32_t s = src[j];
        const int32_t e = d - s;
        err += ((int64_t)e * e);
      }
      dat += dat_stride;
      src += src_stride;
    }
  }
  return err;
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

static int64_t get_pixel_proj_error(const uint8_t *src8, int width, int height,
                                    int src_stride, const uint8_t *dat8,
                                    int dat_stride, int use_highbitdepth,
                                    int32_t *flt0, int flt0_stride,
                                    int32_t *flt1, int flt1_stride, int *xqd,
                                    const sgr_params_type *params) {
  int xq[2];
  av1_decode_xq(xqd, xq, params);

#if CONFIG_AV1_HIGHBITDEPTH
  if (use_highbitdepth) {
    return av1_highbd_pixel_proj_error(src8, width, height, src_stride, dat8,
                                       dat_stride, flt0, flt0_stride, flt1,
                                       flt1_stride, xq, params);

  } else {
    return av1_lowbd_pixel_proj_error(src8, width, height, src_stride, dat8,
                                      dat_stride, flt0, flt0_stride, flt1,
                                      flt1_stride, xq, params);
  }
#else
  (void)use_highbitdepth;
  return av1_lowbd_pixel_proj_error(src8, width, height, src_stride, dat8,
                                    dat_stride, flt0, flt0_stride, flt1,
                                    flt1_stride, xq, params);
#endif
}

#define USE_SGRPROJ_REFINEMENT_SEARCH 1
static int64_t finer_search_pixel_proj_error(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int use_highbitdepth, int32_t *flt0,
    int flt0_stride, int32_t *flt1, int flt1_stride, int start_step, int *xqd,
    const sgr_params_type *params) {
  int64_t err = get_pixel_proj_error(
      src8, width, height, src_stride, dat8, dat_stride, use_highbitdepth, flt0,
      flt0_stride, flt1, flt1_stride, xqd, params);
  (void)start_step;
#if USE_SGRPROJ_REFINEMENT_SEARCH
  int64_t err2;
  int tap_min[] = { SGRPROJ_PRJ_MIN0, SGRPROJ_PRJ_MIN1 };
  int tap_max[] = { SGRPROJ_PRJ_MAX0, SGRPROJ_PRJ_MAX1 };
  for (int s = start_step; s >= 1; s >>= 1) {
    for (int p = 0; p < 2; ++p) {
      if ((params->r[0] == 0 && p == 0) || (params->r[1] == 0 && p == 1)) {
        continue;
      }
      int skip = 0;
      do {
        if (xqd[p] - s >= tap_min[p]) {
          xqd[p] -= s;
          err2 =
              get_pixel_proj_error(src8, width, height, src_stride, dat8,
                                   dat_stride, use_highbitdepth, flt0,
                                   flt0_stride, flt1, flt1_stride, xqd, params);
          if (err2 > err) {
            xqd[p] += s;
          } else {
            err = err2;
            skip = 1;
            // At the highest step size continue moving in the same direction
            if (s == start_step) continue;
          }
        }
        break;
      } while (1);
      if (skip) break;
      do {
        if (xqd[p] + s <= tap_max[p]) {
          xqd[p] += s;
          err2 =
              get_pixel_proj_error(src8, width, height, src_stride, dat8,
                                   dat_stride, use_highbitdepth, flt0,
                                   flt0_stride, flt1, flt1_stride, xqd, params);
          if (err2 > err) {
            xqd[p] -= s;
          } else {
            err = err2;
            // At the highest step size continue moving in the same direction
            if (s == start_step) continue;
          }
        }
        break;
      } while (1);
    }
  }
#endif  // USE_SGRPROJ_REFINEMENT_SEARCH
  return err;
}

static int64_t signed_rounded_divide(int64_t dividend, int64_t divisor) {
  if (dividend < 0)
    return (dividend - divisor / 2) / divisor;
  else
    return (dividend + divisor / 2) / divisor;
}

static AOM_INLINE void calc_proj_params_r0_r1_c(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride,
    int32_t *flt1, int flt1_stride, int64_t H[2][2], int64_t C[2]) {
  const int size = width * height;
  const uint8_t *src = src8;
  const uint8_t *dat = dat8;
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      const int32_t u = (int32_t)(dat[i * dat_stride + j] << SGRPROJ_RST_BITS);
      const int32_t s =
          (int32_t)(src[i * src_stride + j] << SGRPROJ_RST_BITS) - u;
      const int32_t f1 = (int32_t)flt0[i * flt0_stride + j] - u;
      const int32_t f2 = (int32_t)flt1[i * flt1_stride + j] - u;
      H[0][0] += (int64_t)f1 * f1;
      H[1][1] += (int64_t)f2 * f2;
      H[0][1] += (int64_t)f1 * f2;
      C[0] += (int64_t)f1 * s;
      C[1] += (int64_t)f2 * s;
    }
  }
  H[0][0] /= size;
  H[0][1] /= size;
  H[1][1] /= size;
  H[1][0] = H[0][1];
  C[0] /= size;
  C[1] /= size;
}

#if CONFIG_AV1_HIGHBITDEPTH
static AOM_INLINE void calc_proj_params_r0_r1_high_bd_c(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride,
    int32_t *flt1, int flt1_stride, int64_t H[2][2], int64_t C[2]) {
  const int size = width * height;
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dat = CONVERT_TO_SHORTPTR(dat8);
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      const int32_t u = (int32_t)(dat[i * dat_stride + j] << SGRPROJ_RST_BITS);
      const int32_t s =
          (int32_t)(src[i * src_stride + j] << SGRPROJ_RST_BITS) - u;
      const int32_t f1 = (int32_t)flt0[i * flt0_stride + j] - u;
      const int32_t f2 = (int32_t)flt1[i * flt1_stride + j] - u;
      H[0][0] += (int64_t)f1 * f1;
      H[1][1] += (int64_t)f2 * f2;
      H[0][1] += (int64_t)f1 * f2;
      C[0] += (int64_t)f1 * s;
      C[1] += (int64_t)f2 * s;
    }
  }
  H[0][0] /= size;
  H[0][1] /= size;
  H[1][1] /= size;
  H[1][0] = H[0][1];
  C[0] /= size;
  C[1] /= size;
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

static AOM_INLINE void calc_proj_params_r0_c(const uint8_t *src8, int width,
                                             int height, int src_stride,
                                             const uint8_t *dat8,
                                             int dat_stride, int32_t *flt0,
                                             int flt0_stride, int64_t H[2][2],
                                             int64_t C[2]) {
  const int size = width * height;
  const uint8_t *src = src8;
  const uint8_t *dat = dat8;
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      const int32_t u = (int32_t)(dat[i * dat_stride + j] << SGRPROJ_RST_BITS);
      const int32_t s =
          (int32_t)(src[i * src_stride + j] << SGRPROJ_RST_BITS) - u;
      const int32_t f1 = (int32_t)flt0[i * flt0_stride + j] - u;
      H[0][0] += (int64_t)f1 * f1;
      C[0] += (int64_t)f1 * s;
    }
  }
  H[0][0] /= size;
  C[0] /= size;
}

#if CONFIG_AV1_HIGHBITDEPTH
static AOM_INLINE void calc_proj_params_r0_high_bd_c(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride,
    int64_t H[2][2], int64_t C[2]) {
  const int size = width * height;
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dat = CONVERT_TO_SHORTPTR(dat8);
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      const int32_t u = (int32_t)(dat[i * dat_stride + j] << SGRPROJ_RST_BITS);
      const int32_t s =
          (int32_t)(src[i * src_stride + j] << SGRPROJ_RST_BITS) - u;
      const int32_t f1 = (int32_t)flt0[i * flt0_stride + j] - u;
      H[0][0] += (int64_t)f1 * f1;
      C[0] += (int64_t)f1 * s;
    }
  }
  H[0][0] /= size;
  C[0] /= size;
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

static AOM_INLINE void calc_proj_params_r1_c(const uint8_t *src8, int width,
                                             int height, int src_stride,
                                             const uint8_t *dat8,
                                             int dat_stride, int32_t *flt1,
                                             int flt1_stride, int64_t H[2][2],
                                             int64_t C[2]) {
  const int size = width * height;
  const uint8_t *src = src8;
  const uint8_t *dat = dat8;
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      const int32_t u = (int32_t)(dat[i * dat_stride + j] << SGRPROJ_RST_BITS);
      const int32_t s =
          (int32_t)(src[i * src_stride + j] << SGRPROJ_RST_BITS) - u;
      const int32_t f2 = (int32_t)flt1[i * flt1_stride + j] - u;
      H[1][1] += (int64_t)f2 * f2;
      C[1] += (int64_t)f2 * s;
    }
  }
  H[1][1] /= size;
  C[1] /= size;
}

#if CONFIG_AV1_HIGHBITDEPTH
static AOM_INLINE void calc_proj_params_r1_high_bd_c(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt1, int flt1_stride,
    int64_t H[2][2], int64_t C[2]) {
  const int size = width * height;
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dat = CONVERT_TO_SHORTPTR(dat8);
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      const int32_t u = (int32_t)(dat[i * dat_stride + j] << SGRPROJ_RST_BITS);
      const int32_t s =
          (int32_t)(src[i * src_stride + j] << SGRPROJ_RST_BITS) - u;
      const int32_t f2 = (int32_t)flt1[i * flt1_stride + j] - u;
      H[1][1] += (int64_t)f2 * f2;
      C[1] += (int64_t)f2 * s;
    }
  }
  H[1][1] /= size;
  C[1] /= size;
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

// The function calls 3 subfunctions for the following cases :
// 1) When params->r[0] > 0 and params->r[1] > 0. In this case all elements
// of C and H need to be computed.
// 2) When only params->r[0] > 0. In this case only H[0][0] and C[0] are
// non-zero and need to be computed.
// 3) When only params->r[1] > 0. In this case only H[1][1] and C[1] are
// non-zero and need to be computed.
void av1_calc_proj_params_c(const uint8_t *src8, int width, int height,
                            int src_stride, const uint8_t *dat8, int dat_stride,
                            int32_t *flt0, int flt0_stride, int32_t *flt1,
                            int flt1_stride, int64_t H[2][2], int64_t C[2],
                            const sgr_params_type *params) {
  if ((params->r[0] > 0) && (params->r[1] > 0)) {
    calc_proj_params_r0_r1_c(src8, width, height, src_stride, dat8, dat_stride,
                             flt0, flt0_stride, flt1, flt1_stride, H, C);
  } else if (params->r[0] > 0) {
    calc_proj_params_r0_c(src8, width, height, src_stride, dat8, dat_stride,
                          flt0, flt0_stride, H, C);
  } else if (params->r[1] > 0) {
    calc_proj_params_r1_c(src8, width, height, src_stride, dat8, dat_stride,
                          flt1, flt1_stride, H, C);
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
void av1_calc_proj_params_high_bd_c(const uint8_t *src8, int width, int height,
                                    int src_stride, const uint8_t *dat8,
                                    int dat_stride, int32_t *flt0,
                                    int flt0_stride, int32_t *flt1,
                                    int flt1_stride, int64_t H[2][2],
                                    int64_t C[2],
                                    const sgr_params_type *params) {
  if ((params->r[0] > 0) && (params->r[1] > 0)) {
    calc_proj_params_r0_r1_high_bd_c(src8, width, height, src_stride, dat8,
                                     dat_stride, flt0, flt0_stride, flt1,
                                     flt1_stride, H, C);
  } else if (params->r[0] > 0) {
    calc_proj_params_r0_high_bd_c(src8, width, height, src_stride, dat8,
                                  dat_stride, flt0, flt0_stride, H, C);
  } else if (params->r[1] > 0) {
    calc_proj_params_r1_high_bd_c(src8, width, height, src_stride, dat8,
                                  dat_stride, flt1, flt1_stride, H, C);
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

static AOM_INLINE void get_proj_subspace(const uint8_t *src8, int width,
                                         int height, int src_stride,
                                         const uint8_t *dat8, int dat_stride,
                                         int use_highbitdepth, int32_t *flt0,
                                         int flt0_stride, int32_t *flt1,
                                         int flt1_stride, int *xq,
                                         const sgr_params_type *params) {
  int64_t H[2][2] = { { 0, 0 }, { 0, 0 } };
  int64_t C[2] = { 0, 0 };

  // Default values to be returned if the problem becomes ill-posed
  xq[0] = 0;
  xq[1] = 0;

  if (!use_highbitdepth) {
    if ((width & 0x7) == 0) {
      av1_calc_proj_params(src8, width, height, src_stride, dat8, dat_stride,
                           flt0, flt0_stride, flt1, flt1_stride, H, C, params);
    } else {
      av1_calc_proj_params_c(src8, width, height, src_stride, dat8, dat_stride,
                             flt0, flt0_stride, flt1, flt1_stride, H, C,
                             params);
    }
  }
#if CONFIG_AV1_HIGHBITDEPTH
  else {  // NOLINT
    if ((width & 0x7) == 0) {
      av1_calc_proj_params_high_bd(src8, width, height, src_stride, dat8,
                                   dat_stride, flt0, flt0_stride, flt1,
                                   flt1_stride, H, C, params);
    } else {
      av1_calc_proj_params_high_bd_c(src8, width, height, src_stride, dat8,
                                     dat_stride, flt0, flt0_stride, flt1,
                                     flt1_stride, H, C, params);
    }
  }
#endif

  if (params->r[0] == 0) {
    // H matrix is now only the scalar H[1][1]
    // C vector is now only the scalar C[1]
    const int64_t Det = H[1][1];
    if (Det == 0) return;  // ill-posed, return default values
    xq[0] = 0;
    xq[1] = (int)signed_rounded_divide(C[1] * (1 << SGRPROJ_PRJ_BITS), Det);
  } else if (params->r[1] == 0) {
    // H matrix is now only the scalar H[0][0]
    // C vector is now only the scalar C[0]
    const int64_t Det = H[0][0];
    if (Det == 0) return;  // ill-posed, return default values
    xq[0] = (int)signed_rounded_divide(C[0] * (1 << SGRPROJ_PRJ_BITS), Det);
    xq[1] = 0;
  } else {
    const int64_t Det = H[0][0] * H[1][1] - H[0][1] * H[1][0];
    if (Det == 0) return;  // ill-posed, return default values

    // If scaling up dividend would overflow, instead scale down the divisor
    const int64_t div1 = H[1][1] * C[0] - H[0][1] * C[1];
    if ((div1 > 0 && INT64_MAX / (1 << SGRPROJ_PRJ_BITS) < div1) ||
        (div1 < 0 && INT64_MIN / (1 << SGRPROJ_PRJ_BITS) > div1))
      xq[0] = (int)signed_rounded_divide(div1, Det / (1 << SGRPROJ_PRJ_BITS));
    else
      xq[0] = (int)signed_rounded_divide(div1 * (1 << SGRPROJ_PRJ_BITS), Det);

    const int64_t div2 = H[0][0] * C[1] - H[1][0] * C[0];
    if ((div2 > 0 && INT64_MAX / (1 << SGRPROJ_PRJ_BITS) < div2) ||
        (div2 < 0 && INT64_MIN / (1 << SGRPROJ_PRJ_BITS) > div2))
      xq[1] = (int)signed_rounded_divide(div2, Det / (1 << SGRPROJ_PRJ_BITS));
    else
      xq[1] = (int)signed_rounded_divide(div2 * (1 << SGRPROJ_PRJ_BITS), Det);
  }
}

static AOM_INLINE void encode_xq(int *xq, int *xqd,
                                 const sgr_params_type *params) {
  if (params->r[0] == 0) {
    xqd[0] = 0;
    xqd[1] = clamp((1 << SGRPROJ_PRJ_BITS) - xq[1], SGRPROJ_PRJ_MIN1,
                   SGRPROJ_PRJ_MAX1);
  } else if (params->r[1] == 0) {
    xqd[0] = clamp(xq[0], SGRPROJ_PRJ_MIN0, SGRPROJ_PRJ_MAX0);
    xqd[1] = clamp((1 << SGRPROJ_PRJ_BITS) - xqd[0], SGRPROJ_PRJ_MIN1,
                   SGRPROJ_PRJ_MAX1);
  } else {
    xqd[0] = clamp(xq[0], SGRPROJ_PRJ_MIN0, SGRPROJ_PRJ_MAX0);
    xqd[1] = clamp((1 << SGRPROJ_PRJ_BITS) - xqd[0] - xq[1], SGRPROJ_PRJ_MIN1,
                   SGRPROJ_PRJ_MAX1);
  }
}

// Apply the self-guided filter across an entire restoration unit.
static AOM_INLINE void apply_sgr(int sgr_params_idx, const uint8_t *dat8,
                                 int width, int height, int dat_stride,
                                 int use_highbd, int bit_depth, int pu_width,
                                 int pu_height, int32_t *flt0, int32_t *flt1,
                                 int flt_stride,
                                 struct aom_internal_error_info *error_info) {
  for (int i = 0; i < height; i += pu_height) {
    const int h = AOMMIN(pu_height, height - i);
    int32_t *flt0_row = flt0 + i * flt_stride;
    int32_t *flt1_row = flt1 + i * flt_stride;
    const uint8_t *dat8_row = dat8 + i * dat_stride;

    // Iterate over the stripe in blocks of width pu_width
    for (int j = 0; j < width; j += pu_width) {
      const int w = AOMMIN(pu_width, width - j);
      if (av1_selfguided_restoration(
              dat8_row + j, w, h, dat_stride, flt0_row + j, flt1_row + j,
              flt_stride, sgr_params_idx, bit_depth, use_highbd) != 0) {
        aom_internal_error(
            error_info, AOM_CODEC_MEM_ERROR,
            "Error allocating buffer in av1_selfguided_restoration");
      }
    }
  }
}

static AOM_INLINE void compute_sgrproj_err(
    const uint8_t *dat8, const int width, const int height,
    const int dat_stride, const uint8_t *src8, const int src_stride,
    const int use_highbitdepth, const int bit_depth, const int pu_width,
    const int pu_height, const int ep, int32_t *flt0, int32_t *flt1,
    const int flt_stride, int *exqd, int64_t *err,
    struct aom_internal_error_info *error_info) {
  int exq[2];
  apply_sgr(ep, dat8, width, height, dat_stride, use_highbitdepth, bit_depth,
            pu_width, pu_height, flt0, flt1, flt_stride, error_info);
  const sgr_params_type *const params = &av1_sgr_params[ep];
  get_proj_subspace(src8, width, height, src_stride, dat8, dat_stride,
                    use_highbitdepth, flt0, flt_stride, flt1, flt_stride, exq,
                    params);
  encode_xq(exq, exqd, params);
  *err = finer_search_pixel_proj_error(
      src8, width, height, src_stride, dat8, dat_stride, use_highbitdepth, flt0,
      flt_stride, flt1, flt_stride, 2, exqd, params);
}

static AOM_INLINE void get_best_error(int64_t *besterr, const int64_t err,
                                      const int *exqd, int *bestxqd,
                                      int *bestep, const int ep) {
  if (*besterr == -1 || err < *besterr) {
    *bestep = ep;
    *besterr = err;
    bestxqd[0] = exqd[0];
    bestxqd[1] = exqd[1];
  }
}

static SgrprojInfo search_selfguided_restoration(
    const uint8_t *dat8, int width, int height, int dat_stride,
    const uint8_t *src8, int src_stride, int use_highbitdepth, int bit_depth,
    int pu_width, int pu_height, int32_t *rstbuf, int enable_sgr_ep_pruning,
    struct aom_internal_error_info *error_info) {
  int32_t *flt0 = rstbuf;
  int32_t *flt1 = flt0 + RESTORATION_UNITPELS_MAX;
  int ep, idx, bestep = 0;
  int64_t besterr = -1;
  int exqd[2], bestxqd[2] = { 0, 0 };
  int flt_stride = ((width + 7) & ~7) + 8;
  assert(pu_width == (RESTORATION_PROC_UNIT_SIZE >> 1) ||
         pu_width == RESTORATION_PROC_UNIT_SIZE);
  assert(pu_height == (RESTORATION_PROC_UNIT_SIZE >> 1) ||
         pu_height == RESTORATION_PROC_UNIT_SIZE);
  if (!enable_sgr_ep_pruning) {
    for (ep = 0; ep < SGRPROJ_PARAMS; ep++) {
      int64_t err;
      compute_sgrproj_err(dat8, width, height, dat_stride, src8, src_stride,
                          use_highbitdepth, bit_depth, pu_width, pu_height, ep,
                          flt0, flt1, flt_stride, exqd, &err, error_info);
      get_best_error(&besterr, err, exqd, bestxqd, &bestep, ep);
    }
  } else {
    // evaluate first four seed ep in first group
    for (idx = 0; idx < SGRPROJ_EP_GRP1_SEARCH_COUNT; idx++) {
      ep = sgproj_ep_grp1_seed[idx];
      int64_t err;
      compute_sgrproj_err(dat8, width, height, dat_stride, src8, src_stride,
                          use_highbitdepth, bit_depth, pu_width, pu_height, ep,
                          flt0, flt1, flt_stride, exqd, &err, error_info);
      get_best_error(&besterr, err, exqd, bestxqd, &bestep, ep);
    }
    // evaluate left and right ep of winner in seed ep
    int bestep_ref = bestep;
    for (ep = bestep_ref - 1; ep < bestep_ref + 2; ep += 2) {
      if (ep < SGRPROJ_EP_GRP1_START_IDX || ep > SGRPROJ_EP_GRP1_END_IDX)
        continue;
      int64_t err;
      compute_sgrproj_err(dat8, width, height, dat_stride, src8, src_stride,
                          use_highbitdepth, bit_depth, pu_width, pu_height, ep,
                          flt0, flt1, flt_stride, exqd, &err, error_info);
      get_best_error(&besterr, err, exqd, bestxqd, &bestep, ep);
    }
    // evaluate last two group
    for (idx = 0; idx < SGRPROJ_EP_GRP2_3_SEARCH_COUNT; idx++) {
      ep = sgproj_ep_grp2_3[idx][bestep];
      int64_t err;
      compute_sgrproj_err(dat8, width, height, dat_stride, src8, src_stride,
                          use_highbitdepth, bit_depth, pu_width, pu_height, ep,
                          flt0, flt1, flt_stride, exqd, &err, error_info);
      get_best_error(&besterr, err, exqd, bestxqd, &bestep, ep);
    }
  }

  SgrprojInfo ret;
  ret.ep = bestep;
  ret.xqd[0] = bestxqd[0];
  ret.xqd[1] = bestxqd[1];
  return ret;
}

static int count_sgrproj_bits(SgrprojInfo *sgrproj_info,
                              SgrprojInfo *ref_sgrproj_info) {
  int bits = SGRPROJ_PARAMS_BITS;
  const sgr_params_type *params = &av1_sgr_params[sgrproj_info->ep];
  if (params->r[0] > 0)
    bits += aom_count_primitive_refsubexpfin(
        SGRPROJ_PRJ_MAX0 - SGRPROJ_PRJ_MIN0 + 1, SGRPROJ_PRJ_SUBEXP_K,
        ref_sgrproj_info->xqd[0] - SGRPROJ_PRJ_MIN0,
        sgrproj_info->xqd[0] - SGRPROJ_PRJ_MIN0);
  if (params->r[1] > 0)
    bits += aom_count_primitive_refsubexpfin(
        SGRPROJ_PRJ_MAX1 - SGRPROJ_PRJ_MIN1 + 1, SGRPROJ_PRJ_SUBEXP_K,
        ref_sgrproj_info->xqd[1] - SGRPROJ_PRJ_MIN1,
        sgrproj_info->xqd[1] - SGRPROJ_PRJ_MIN1);
  return bits;
}

static AOM_INLINE void search_sgrproj(
    const RestorationTileLimits *limits, int rest_unit_idx, void *priv,
    int32_t *tmpbuf, RestorationLineBuffers *rlbs,
    struct aom_internal_error_info *error_info) {
  (void)rlbs;
  RestSearchCtxt *rsc = (RestSearchCtxt *)priv;
  RestUnitSearchInfo *rusi = &rsc->rusi[rest_unit_idx];

  const MACROBLOCK *const x = rsc->x;
  const AV1_COMMON *const cm = rsc->cm;
  const int highbd = cm->seq_params->use_highbitdepth;
  const int bit_depth = cm->seq_params->bit_depth;

  const int64_t bits_none = x->mode_costs.sgrproj_restore_cost[0];
  // Prune evaluation of RESTORE_SGRPROJ if 'skip_sgr_eval' is set
  if (rsc->skip_sgr_eval) {
    rsc->total_bits[RESTORE_SGRPROJ] += bits_none;
    rsc->total_sse[RESTORE_SGRPROJ] += rsc->sse[RESTORE_NONE];
    rusi->best_rtype[RESTORE_SGRPROJ - 1] = RESTORE_NONE;
    rsc->sse[RESTORE_SGRPROJ] = INT64_MAX;
    return;
  }

  uint8_t *dgd_start =
      rsc->dgd_buffer + limits->v_start * rsc->dgd_stride + limits->h_start;
  const uint8_t *src_start =
      rsc->src_buffer + limits->v_start * rsc->src_stride + limits->h_start;

  const int is_uv = rsc->plane > 0;
  const int ss_x = is_uv && cm->seq_params->subsampling_x;
  const int ss_y = is_uv && cm->seq_params->subsampling_y;
  const int procunit_width = RESTORATION_PROC_UNIT_SIZE >> ss_x;
  const int procunit_height = RESTORATION_PROC_UNIT_SIZE >> ss_y;

  rusi->sgrproj = search_selfguided_restoration(
      dgd_start, limits->h_end - limits->h_start,
      limits->v_end - limits->v_start, rsc->dgd_stride, src_start,
      rsc->src_stride, highbd, bit_depth, procunit_width, procunit_height,
      tmpbuf, rsc->lpf_sf->enable_sgr_ep_pruning, error_info);

  RestorationUnitInfo rui;
  rui.restoration_type = RESTORE_SGRPROJ;
  rui.sgrproj_info = rusi->sgrproj;

  rsc->sse[RESTORE_SGRPROJ] = try_restoration_unit(rsc, limits, &rui);

  const int64_t bits_sgr =
      x->mode_costs.sgrproj_restore_cost[1] +
      (count_sgrproj_bits(&rusi->sgrproj, &rsc->ref_sgrproj)
       << AV1_PROB_COST_SHIFT);
  double cost_none = RDCOST_DBL_WITH_NATIVE_BD_DIST(
      x->rdmult, bits_none >> 4, rsc->sse[RESTORE_NONE], bit_depth);
  double cost_sgr = RDCOST_DBL_WITH_NATIVE_BD_DIST(
      x->rdmult, bits_sgr >> 4, rsc->sse[RESTORE_SGRPROJ], bit_depth);
  if (rusi->sgrproj.ep < 10)
    cost_sgr *=
        (1 + DUAL_SGR_PENALTY_MULT * rsc->lpf_sf->dual_sgr_penalty_level);

  RestorationType rtype =
      (cost_sgr < cost_none) ? RESTORE_SGRPROJ : RESTORE_NONE;
  rusi->best_rtype[RESTORE_SGRPROJ - 1] = rtype;

#if DEBUG_LR_COSTING
  // Store ref params for later checking
  lr_ref_params[RESTORE_SGRPROJ][rsc->plane][rest_unit_idx].sgrproj_info =
      rsc->ref_sgrproj;
#endif  // DEBUG_LR_COSTING

  rsc->total_sse[RESTORE_SGRPROJ] += rsc->sse[rtype];
  rsc->total_bits[RESTORE_SGRPROJ] +=
      (cost_sgr < cost_none) ? bits_sgr : bits_none;
  if (cost_sgr < cost_none) rsc->ref_sgrproj = rusi->sgrproj;
}

static void acc_stat_one_line(const uint8_t *dgd, const uint8_t *src,
                              int dgd_stride, int h_start, int h_end,
                              uint8_t avg, const int wiener_halfwin,
                              const int wiener_win2, int32_t *M_int32,
                              int32_t *H_int32, int count) {
  int j, k, l;
  int16_t Y[WIENER_WIN2];

  for (j = h_start; j < h_end; j++) {
    const int16_t X = (int16_t)src[j] - (int16_t)avg;
    int idx = 0;
    for (k = -wiener_halfwin; k <= wiener_halfwin; k++) {
      for (l = -wiener_halfwin; l <= wiener_halfwin; l++) {
        Y[idx] =
            (int16_t)dgd[(count + l) * dgd_stride + (j + k)] - (int16_t)avg;
        idx++;
      }
    }
    assert(idx == wiener_win2);
    for (k = 0; k < wiener_win2; ++k) {
      M_int32[k] += (int32_t)Y[k] * X;
      for (l = k; l < wiener_win2; ++l) {
        // H is a symmetric matrix, so we only need to fill out the upper
        // triangle here. We can copy it down to the lower triangle outside
        // the (i, j) loops.
        H_int32[k * wiener_win2 + l] += (int32_t)Y[k] * Y[l];
      }
    }
  }
}

void av1_compute_stats_c(int wiener_win, const uint8_t *dgd, const uint8_t *src,
                         int16_t *dgd_avg, int16_t *src_avg, int h_start,
                         int h_end, int v_start, int v_end, int dgd_stride,
                         int src_stride, int64_t *M, int64_t *H,
                         int use_downsampled_wiener_stats) {
  (void)dgd_avg;
  (void)src_avg;
  int i, k, l;
  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin = (wiener_win >> 1);
  uint8_t avg = find_average(dgd, h_start, h_end, v_start, v_end, dgd_stride);
  int32_t M_row[WIENER_WIN2] = { 0 };
  int32_t H_row[WIENER_WIN2 * WIENER_WIN2] = { 0 };
  int downsample_factor =
      use_downsampled_wiener_stats ? WIENER_STATS_DOWNSAMPLE_FACTOR : 1;

  memset(M, 0, sizeof(*M) * wiener_win2);
  memset(H, 0, sizeof(*H) * wiener_win2 * wiener_win2);

  for (i = v_start; i < v_end; i = i + downsample_factor) {
    if (use_downsampled_wiener_stats &&
        (v_end - i < WIENER_STATS_DOWNSAMPLE_FACTOR)) {
      downsample_factor = v_end - i;
    }

    memset(M_row, 0, sizeof(int32_t) * WIENER_WIN2);
    memset(H_row, 0, sizeof(int32_t) * WIENER_WIN2 * WIENER_WIN2);
    acc_stat_one_line(dgd, src + i * src_stride, dgd_stride, h_start, h_end,
                      avg, wiener_halfwin, wiener_win2, M_row, H_row, i);

    for (k = 0; k < wiener_win2; ++k) {
      // Scale M matrix based on the downsampling factor
      M[k] += ((int64_t)M_row[k] * downsample_factor);
      for (l = k; l < wiener_win2; ++l) {
        // H is a symmetric matrix, so we only need to fill out the upper
        // triangle here. We can copy it down to the lower triangle outside
        // the (i, j) loops.
        // Scale H Matrix based on the downsampling factor
        H[k * wiener_win2 + l] +=
            ((int64_t)H_row[k * wiener_win2 + l] * downsample_factor);
      }
    }
  }

  for (k = 0; k < wiener_win2; ++k) {
    for (l = k + 1; l < wiener_win2; ++l) {
      H[l * wiener_win2 + k] = H[k * wiener_win2 + l];
    }
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
void av1_compute_stats_highbd_c(int wiener_win, const uint8_t *dgd8,
                                const uint8_t *src8, int16_t *dgd_avg,
                                int16_t *src_avg, int h_start, int h_end,
                                int v_start, int v_end, int dgd_stride,
                                int src_stride, int64_t *M, int64_t *H,
                                aom_bit_depth_t bit_depth) {
  (void)dgd_avg;
  (void)src_avg;
  int i, j, k, l;
  int32_t Y[WIENER_WIN2];
  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin = (wiener_win >> 1);
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dgd = CONVERT_TO_SHORTPTR(dgd8);
  uint16_t avg =
      find_average_highbd(dgd, h_start, h_end, v_start, v_end, dgd_stride);

  uint8_t bit_depth_divider = 1;
  if (bit_depth == AOM_BITS_12)
    bit_depth_divider = 16;
  else if (bit_depth == AOM_BITS_10)
    bit_depth_divider = 4;

  memset(M, 0, sizeof(*M) * wiener_win2);
  memset(H, 0, sizeof(*H) * wiener_win2 * wiener_win2);
  for (i = v_start; i < v_end; i++) {
    for (j = h_start; j < h_end; j++) {
      const int32_t X = (int32_t)src[i * src_stride + j] - (int32_t)avg;
      int idx = 0;
      for (k = -wiener_halfwin; k <= wiener_halfwin; k++) {
        for (l = -wiener_halfwin; l <= wiener_halfwin; l++) {
          Y[idx] = (int32_t)dgd[(i + l) * dgd_stride + (j + k)] - (int32_t)avg;
          idx++;
        }
      }
      assert(idx == wiener_win2);
      for (k = 0; k < wiener_win2; ++k) {
        M[k] += (int64_t)Y[k] * X;
        for (l = k; l < wiener_win2; ++l) {
          // H is a symmetric matrix, so we only need to fill out the upper
          // triangle here. We can copy it down to the lower triangle outside
          // the (i, j) loops.
          H[k * wiener_win2 + l] += (int64_t)Y[k] * Y[l];
        }
      }
    }
  }
  for (k = 0; k < wiener_win2; ++k) {
    M[k] /= bit_depth_divider;
    H[k * wiener_win2 + k] /= bit_depth_divider;
    for (l = k + 1; l < wiener_win2; ++l) {
      H[k * wiener_win2 + l] /= bit_depth_divider;
      H[l * wiener_win2 + k] = H[k * wiener_win2 + l];
    }
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

static INLINE int wrap_index(int i, int wiener_win) {
  const int wiener_halfwin1 = (wiener_win >> 1) + 1;
  return (i >= wiener_halfwin1 ? wiener_win - 1 - i : i);
}

// Splits each w[i] into smaller components w1[i] and w2[i] such that
// w[i] = w1[i] * WIENER_TAP_SCALE_FACTOR + w2[i].
static INLINE void split_wiener_filter_coefficients(int wiener_win,
                                                    const int32_t *w,
                                                    int32_t *w1, int32_t *w2) {
  for (int i = 0; i < wiener_win; i++) {
    w1[i] = w[i] / WIENER_TAP_SCALE_FACTOR;
    w2[i] = w[i] - w1[i] * WIENER_TAP_SCALE_FACTOR;
    assert(w[i] == w1[i] * WIENER_TAP_SCALE_FACTOR + w2[i]);
  }
}

// Calculates x * w / WIENER_TAP_SCALE_FACTOR, where
// w = w1 * WIENER_TAP_SCALE_FACTOR + w2.
//
// The multiplication x * w may overflow, so we multiply x by the components of
// w (w1 and w2) and combine the multiplication with the division.
static INLINE int64_t multiply_and_scale(int64_t x, int32_t w1, int32_t w2) {
  // Let y = x * w / WIENER_TAP_SCALE_FACTOR
  //       = x * (w1 * WIENER_TAP_SCALE_FACTOR + w2) / WIENER_TAP_SCALE_FACTOR
  const int64_t y = x * w1 + x * w2 / WIENER_TAP_SCALE_FACTOR;
  return y;
}

// Solve linear equations to find Wiener filter tap values
// Taps are output scaled by WIENER_FILT_STEP
static int linsolve_wiener(int n, int64_t *A, int stride, int64_t *b,
                           int64_t *x) {
  for (int k = 0; k < n - 1; k++) {
    // Partial pivoting: bring the row with the largest pivot to the top
    for (int i = n - 1; i > k; i--) {
      // If row i has a better (bigger) pivot than row (i-1), swap them
      if (llabs(A[(i - 1) * stride + k]) < llabs(A[i * stride + k])) {
        for (int j = 0; j < n; j++) {
          const int64_t c = A[i * stride + j];
          A[i * stride + j] = A[(i - 1) * stride + j];
          A[(i - 1) * stride + j] = c;
        }
        const int64_t c = b[i];
        b[i] = b[i - 1];
        b[i - 1] = c;
      }
    }

    // b/278065963: The multiplies
    //   c / 256 * A[k * stride + j] / cd * 256
    // and
    //   c / 256 * b[k] / cd * 256
    // within Gaussian elimination can cause a signed integer overflow. Rework
    // the multiplies so that larger scaling is used without significantly
    // impacting the overall precision.
    //
    // Precision guidance:
    //   scale_threshold: Pick as high as possible.
    // For max_abs_akj >= scale_threshold scenario:
    //   scaler_A: Pick as low as possible. Needed for A[(i + 1) * stride + j].
    //   scaler_c: Pick as low as possible while maintaining scaler_c >=
    //     (1 << 7). Needed for A[(i + 1) * stride + j] and b[i + 1].
    int64_t max_abs_akj = 0;
    for (int j = 0; j < n; j++) {
      const int64_t abs_akj = llabs(A[k * stride + j]);
      if (abs_akj > max_abs_akj) max_abs_akj = abs_akj;
    }
    const int scale_threshold = 1 << 22;
    const int scaler_A = max_abs_akj < scale_threshold ? 1 : (1 << 6);
    const int scaler_c = max_abs_akj < scale_threshold ? 1 : (1 << 7);
    const int scaler = scaler_c * scaler_A;

    // Forward elimination (convert A to row-echelon form)
    for (int i = k; i < n - 1; i++) {
      if (A[k * stride + k] == 0) return 0;
      const int64_t c = A[(i + 1) * stride + k] / scaler_c;
      const int64_t cd = A[k * stride + k];
      for (int j = 0; j < n; j++) {
        A[(i + 1) * stride + j] -=
            A[k * stride + j] / scaler_A * c / cd * scaler;
      }
      b[i + 1] -= c * b[k] / cd * scaler_c;
    }
  }
  // Back-substitution
  for (int i = n - 1; i >= 0; i--) {
    if (A[i * stride + i] == 0) return 0;
    int64_t c = 0;
    for (int j = i + 1; j <= n - 1; j++) {
      c += A[i * stride + j] * x[j] / WIENER_TAP_SCALE_FACTOR;
    }
    // Store filter taps x in scaled form.
    x[i] = WIENER_TAP_SCALE_FACTOR * (b[i] - c) / A[i * stride + i];
  }

  return 1;
}

// Fix vector b, update vector a
static AOM_INLINE void update_a_sep_sym(int wiener_win, int64_t **Mc,
                                        int64_t **Hc, int32_t *a,
                                        const int32_t *b) {
  int i, j;
  int64_t S[WIENER_WIN];
  int64_t A[WIENER_HALFWIN1], B[WIENER_HALFWIN1 * WIENER_HALFWIN1];
  int32_t b1[WIENER_WIN], b2[WIENER_WIN];
  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin1 = (wiener_win >> 1) + 1;
  memset(A, 0, sizeof(A));
  memset(B, 0, sizeof(B));
  for (i = 0; i < wiener_win; i++) {
    for (j = 0; j < wiener_win; ++j) {
      const int jj = wrap_index(j, wiener_win);
      A[jj] += Mc[i][j] * b[i] / WIENER_TAP_SCALE_FACTOR;
    }
  }
  split_wiener_filter_coefficients(wiener_win, b, b1, b2);

  for (i = 0; i < wiener_win; i++) {
    for (j = 0; j < wiener_win; j++) {
      int k, l;
      for (k = 0; k < wiener_win; ++k) {
        const int kk = wrap_index(k, wiener_win);
        for (l = 0; l < wiener_win; ++l) {
          const int ll = wrap_index(l, wiener_win);
          // Calculate
          // B[ll * wiener_halfwin1 + kk] +=
          //    Hc[j * wiener_win + i][k * wiener_win2 + l] * b[i] /
          //    WIENER_TAP_SCALE_FACTOR * b[j] / WIENER_TAP_SCALE_FACTOR;
          //
          // The last multiplication may overflow, so we combine the last
          // multiplication with the last division.
          const int64_t x = Hc[j * wiener_win + i][k * wiener_win2 + l] * b[i] /
                            WIENER_TAP_SCALE_FACTOR;
          // b[j] = b1[j] * WIENER_TAP_SCALE_FACTOR + b2[j]
          B[ll * wiener_halfwin1 + kk] += multiply_and_scale(x, b1[j], b2[j]);
        }
      }
    }
  }
  // Normalization enforcement in the system of equations itself
  for (i = 0; i < wiener_halfwin1 - 1; ++i) {
    A[i] -=
        A[wiener_halfwin1 - 1] * 2 +
        B[i * wiener_halfwin1 + wiener_halfwin1 - 1] -
        2 * B[(wiener_halfwin1 - 1) * wiener_halfwin1 + (wiener_halfwin1 - 1)];
  }
  for (i = 0; i < wiener_halfwin1 - 1; ++i) {
    for (j = 0; j < wiener_halfwin1 - 1; ++j) {
      B[i * wiener_halfwin1 + j] -=
          2 * (B[i * wiener_halfwin1 + (wiener_halfwin1 - 1)] +
               B[(wiener_halfwin1 - 1) * wiener_halfwin1 + j] -
               2 * B[(wiener_halfwin1 - 1) * wiener_halfwin1 +
                     (wiener_halfwin1 - 1)]);
    }
  }
  if (linsolve_wiener(wiener_halfwin1 - 1, B, wiener_halfwin1, A, S)) {
    S[wiener_halfwin1 - 1] = WIENER_TAP_SCALE_FACTOR;
    for (i = wiener_halfwin1; i < wiener_win; ++i) {
      S[i] = S[wiener_win - 1 - i];
      S[wiener_halfwin1 - 1] -= 2 * S[i];
    }
    for (i = 0; i < wiener_win; ++i) {
      a[i] = (int32_t)CLIP(S[i], -(1 << (WIENER_FILT_BITS - 1)),
                           (1 << (WIENER_FILT_BITS - 1)) - 1);
    }
  }
}

// Fix vector a, update vector b
static AOM_INLINE void update_b_sep_sym(int wiener_win, int64_t **Mc,
                                        int64_t **Hc, const int32_t *a,
                                        int32_t *b) {
  int i, j;
  int64_t S[WIENER_WIN];
  int64_t A[WIENER_HALFWIN1], B[WIENER_HALFWIN1 * WIENER_HALFWIN1];
  int32_t a1[WIENER_WIN], a2[WIENER_WIN];
  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin1 = (wiener_win >> 1) + 1;
  memset(A, 0, sizeof(A));
  memset(B, 0, sizeof(B));
  for (i = 0; i < wiener_win; i++) {
    const int ii = wrap_index(i, wiener_win);
    for (j = 0; j < wiener_win; j++) {
      A[ii] += Mc[i][j] * a[j] / WIENER_TAP_SCALE_FACTOR;
    }
  }
  split_wiener_filter_coefficients(wiener_win, a, a1, a2);

  for (i = 0; i < wiener_win; i++) {
    const int ii = wrap_index(i, wiener_win);
    for (j = 0; j < wiener_win; j++) {
      const int jj = wrap_index(j, wiener_win);
      int k, l;
      for (k = 0; k < wiener_win; ++k) {
        for (l = 0; l < wiener_win; ++l) {
          // Calculate
          // B[jj * wiener_halfwin1 + ii] +=
          //     Hc[i * wiener_win + j][k * wiener_win2 + l] * a[k] /
          //     WIENER_TAP_SCALE_FACTOR * a[l] / WIENER_TAP_SCALE_FACTOR;
          //
          // The last multiplication may overflow, so we combine the last
          // multiplication with the last division.
          const int64_t x = Hc[i * wiener_win + j][k * wiener_win2 + l] * a[k] /
                            WIENER_TAP_SCALE_FACTOR;
          // a[l] = a1[l] * WIENER_TAP_SCALE_FACTOR + a2[l]
          B[jj * wiener_halfwin1 + ii] += multiply_and_scale(x, a1[l], a2[l]);
        }
      }
    }
  }
  // Normalization enforcement in the system of equations itself
  for (i = 0; i < wiener_halfwin1 - 1; ++i) {
    A[i] -=
        A[wiener_halfwin1 - 1] * 2 +
        B[i * wiener_halfwin1 + wiener_halfwin1 - 1] -
        2 * B[(wiener_halfwin1 - 1) * wiener_halfwin1 + (wiener_halfwin1 - 1)];
  }
  for (i = 0; i < wiener_halfwin1 - 1; ++i) {
    for (j = 0; j < wiener_halfwin1 - 1; ++j) {
      B[i * wiener_halfwin1 + j] -=
          2 * (B[i * wiener_halfwin1 + (wiener_halfwin1 - 1)] +
               B[(wiener_halfwin1 - 1) * wiener_halfwin1 + j] -
               2 * B[(wiener_halfwin1 - 1) * wiener_halfwin1 +
                     (wiener_halfwin1 - 1)]);
    }
  }
  if (linsolve_wiener(wiener_halfwin1 - 1, B, wiener_halfwin1, A, S)) {
    S[wiener_halfwin1 - 1] = WIENER_TAP_SCALE_FACTOR;
    for (i = wiener_halfwin1; i < wiener_win; ++i) {
      S[i] = S[wiener_win - 1 - i];
      S[wiener_halfwin1 - 1] -= 2 * S[i];
    }
    for (i = 0; i < wiener_win; ++i) {
      b[i] = (int32_t)CLIP(S[i], -(1 << (WIENER_FILT_BITS - 1)),
                           (1 << (WIENER_FILT_BITS - 1)) - 1);
    }
  }
}

static void wiener_decompose_sep_sym(int wiener_win, int64_t *M, int64_t *H,
                                     int32_t *a, int32_t *b) {
  static const int32_t init_filt[WIENER_WIN] = {
    WIENER_FILT_TAP0_MIDV, WIENER_FILT_TAP1_MIDV, WIENER_FILT_TAP2_MIDV,
    WIENER_FILT_TAP3_MIDV, WIENER_FILT_TAP2_MIDV, WIENER_FILT_TAP1_MIDV,
    WIENER_FILT_TAP0_MIDV,
  };
  int64_t *Hc[WIENER_WIN2];
  int64_t *Mc[WIENER_WIN];
  int i, j, iter;
  const int plane_off = (WIENER_WIN - wiener_win) >> 1;
  const int wiener_win2 = wiener_win * wiener_win;
  for (i = 0; i < wiener_win; i++) {
    a[i] = b[i] =
        WIENER_TAP_SCALE_FACTOR / WIENER_FILT_STEP * init_filt[i + plane_off];
  }
  for (i = 0; i < wiener_win; i++) {
    Mc[i] = M + i * wiener_win;
    for (j = 0; j < wiener_win; j++) {
      Hc[i * wiener_win + j] =
          H + i * wiener_win * wiener_win2 + j * wiener_win;
    }
  }

  iter = 1;
  while (iter < NUM_WIENER_ITERS) {
    update_a_sep_sym(wiener_win, Mc, Hc, a, b);
    update_b_sep_sym(wiener_win, Mc, Hc, a, b);
    iter++;
  }
}

// Computes the function x'*H*x - x'*M for the learned 2D filter x, and compares
// against identity filters; Final score is defined as the difference between
// the function values
static int64_t compute_score(int wiener_win, int64_t *M, int64_t *H,
                             InterpKernel vfilt, InterpKernel hfilt) {
  int32_t ab[WIENER_WIN * WIENER_WIN];
  int16_t a[WIENER_WIN], b[WIENER_WIN];
  int64_t P = 0, Q = 0;
  int64_t iP = 0, iQ = 0;
  int64_t Score, iScore;
  int i, k, l;
  const int plane_off = (WIENER_WIN - wiener_win) >> 1;
  const int wiener_win2 = wiener_win * wiener_win;

  a[WIENER_HALFWIN] = b[WIENER_HALFWIN] = WIENER_FILT_STEP;
  for (i = 0; i < WIENER_HALFWIN; ++i) {
    a[i] = a[WIENER_WIN - i - 1] = vfilt[i];
    b[i] = b[WIENER_WIN - i - 1] = hfilt[i];
    a[WIENER_HALFWIN] -= 2 * a[i];
    b[WIENER_HALFWIN] -= 2 * b[i];
  }
  memset(ab, 0, sizeof(ab));
  for (k = 0; k < wiener_win; ++k) {
    for (l = 0; l < wiener_win; ++l)
      ab[k * wiener_win + l] = a[l + plane_off] * b[k + plane_off];
  }
  for (k = 0; k < wiener_win2; ++k) {
    P += ab[k] * M[k] / WIENER_FILT_STEP / WIENER_FILT_STEP;
    for (l = 0; l < wiener_win2; ++l) {
      Q += ab[k] * H[k * wiener_win2 + l] * ab[l] / WIENER_FILT_STEP /
           WIENER_FILT_STEP / WIENER_FILT_STEP / WIENER_FILT_STEP;
    }
  }
  Score = Q - 2 * P;

  iP = M[wiener_win2 >> 1];
  iQ = H[(wiener_win2 >> 1) * wiener_win2 + (wiener_win2 >> 1)];
  iScore = iQ - 2 * iP;

  return Score - iScore;
}

static AOM_INLINE void finalize_sym_filter(int wiener_win, int32_t *f,
                                           InterpKernel fi) {
  int i;
  const int wiener_halfwin = (wiener_win >> 1);

  for (i = 0; i < wiener_halfwin; ++i) {
    const int64_t dividend = (int64_t)f[i] * WIENER_FILT_STEP;
    const int64_t divisor = WIENER_TAP_SCALE_FACTOR;
    // Perform this division with proper rounding rather than truncation
    if (dividend < 0) {
      fi[i] = (int16_t)((dividend - (divisor / 2)) / divisor);
    } else {
      fi[i] = (int16_t)((dividend + (divisor / 2)) / divisor);
    }
  }
  // Specialize for 7-tap filter
  if (wiener_win == WIENER_WIN) {
    fi[0] = CLIP(fi[0], WIENER_FILT_TAP0_MINV, WIENER_FILT_TAP0_MAXV);
    fi[1] = CLIP(fi[1], WIENER_FILT_TAP1_MINV, WIENER_FILT_TAP1_MAXV);
    fi[2] = CLIP(fi[2], WIENER_FILT_TAP2_MINV, WIENER_FILT_TAP2_MAXV);
  } else {
    fi[2] = CLIP(fi[1], WIENER_FILT_TAP2_MINV, WIENER_FILT_TAP2_MAXV);
    fi[1] = CLIP(fi[0], WIENER_FILT_TAP1_MINV, WIENER_FILT_TAP1_MAXV);
    fi[0] = 0;
  }
  // Satisfy filter constraints
  fi[WIENER_WIN - 1] = fi[0];
  fi[WIENER_WIN - 2] = fi[1];
  fi[WIENER_WIN - 3] = fi[2];
  // The central element has an implicit +WIENER_FILT_STEP
  fi[3] = -2 * (fi[0] + fi[1] + fi[2]);
}

static int count_wiener_bits(int wiener_win, WienerInfo *wiener_info,
                             WienerInfo *ref_wiener_info) {
  int bits = 0;
  if (wiener_win == WIENER_WIN)
    bits += aom_count_primitive_refsubexpfin(
        WIENER_FILT_TAP0_MAXV - WIENER_FILT_TAP0_MINV + 1,
        WIENER_FILT_TAP0_SUBEXP_K,
        ref_wiener_info->vfilter[0] - WIENER_FILT_TAP0_MINV,
        wiener_info->vfilter[0] - WIENER_FILT_TAP0_MINV);
  bits += aom_count_primitive_refsubexpfin(
      WIENER_FILT_TAP1_MAXV - WIENER_FILT_TAP1_MINV + 1,
      WIENER_FILT_TAP1_SUBEXP_K,
      ref_wiener_info->vfilter[1] - WIENER_FILT_TAP1_MINV,
      wiener_info->vfilter[1] - WIENER_FILT_TAP1_MINV);
  bits += aom_count_primitive_refsubexpfin(
      WIENER_FILT_TAP2_MAXV - WIENER_FILT_TAP2_MINV + 1,
      WIENER_FILT_TAP2_SUBEXP_K,
      ref_wiener_info->vfilter[2] - WIENER_FILT_TAP2_MINV,
      wiener_info->vfilter[2] - WIENER_FILT_TAP2_MINV);
  if (wiener_win == WIENER_WIN)
    bits += aom_count_primitive_refsubexpfin(
        WIENER_FILT_TAP0_MAXV - WIENER_FILT_TAP0_MINV + 1,
        WIENER_FILT_TAP0_SUBEXP_K,
        ref_wiener_info->hfilter[0] - WIENER_FILT_TAP0_MINV,
        wiener_info->hfilter[0] - WIENER_FILT_TAP0_MINV);
  bits += aom_count_primitive_refsubexpfin(
      WIENER_FILT_TAP1_MAXV - WIENER_FILT_TAP1_MINV + 1,
      WIENER_FILT_TAP1_SUBEXP_K,
      ref_wiener_info->hfilter[1] - WIENER_FILT_TAP1_MINV,
      wiener_info->hfilter[1] - WIENER_FILT_TAP1_MINV);
  bits += aom_count_primitive_refsubexpfin(
      WIENER_FILT_TAP2_MAXV - WIENER_FILT_TAP2_MINV + 1,
      WIENER_FILT_TAP2_SUBEXP_K,
      ref_wiener_info->hfilter[2] - WIENER_FILT_TAP2_MINV,
      wiener_info->hfilter[2] - WIENER_FILT_TAP2_MINV);
  return bits;
}

static int64_t finer_search_wiener(const RestSearchCtxt *rsc,
                                   const RestorationTileLimits *limits,
                                   RestorationUnitInfo *rui, int wiener_win) {
  const int plane_off = (WIENER_WIN - wiener_win) >> 1;
  int64_t err = try_restoration_unit(rsc, limits, rui);

  if (rsc->lpf_sf->disable_wiener_coeff_refine_search) return err;

  // Refinement search around the wiener filter coefficients.
  int64_t err2;
  int tap_min[] = { WIENER_FILT_TAP0_MINV, WIENER_FILT_TAP1_MINV,
                    WIENER_FILT_TAP2_MINV };
  int tap_max[] = { WIENER_FILT_TAP0_MAXV, WIENER_FILT_TAP1_MAXV,
                    WIENER_FILT_TAP2_MAXV };

  WienerInfo *plane_wiener = &rui->wiener_info;

  // printf("err  pre = %"PRId64"\n", err);
  const int start_step = 4;
  for (int s = start_step; s >= 1; s >>= 1) {
    for (int p = plane_off; p < WIENER_HALFWIN; ++p) {
      int skip = 0;
      do {
        if (plane_wiener->hfilter[p] - s >= tap_min[p]) {
          plane_wiener->hfilter[p] -= s;
          plane_wiener->hfilter[WIENER_WIN - p - 1] -= s;
          plane_wiener->hfilter[WIENER_HALFWIN] += 2 * s;
          err2 = try_restoration_unit(rsc, limits, rui);
          if (err2 > err) {
            plane_wiener->hfilter[p] += s;
            plane_wiener->hfilter[WIENER_WIN - p - 1] += s;
            plane_wiener->hfilter[WIENER_HALFWIN] -= 2 * s;
          } else {
            err = err2;
            skip = 1;
            // At the highest step size continue moving in the same direction
            if (s == start_step) continue;
          }
        }
        break;
      } while (1);
      if (skip) break;
      do {
        if (plane_wiener->hfilter[p] + s <= tap_max[p]) {
          plane_wiener->hfilter[p] += s;
          plane_wiener->hfilter[WIENER_WIN - p - 1] += s;
          plane_wiener->hfilter[WIENER_HALFWIN] -= 2 * s;
          err2 = try_restoration_unit(rsc, limits, rui);
          if (err2 > err) {
            plane_wiener->hfilter[p] -= s;
            plane_wiener->hfilter[WIENER_WIN - p - 1] -= s;
            plane_wiener->hfilter[WIENER_HALFWIN] += 2 * s;
          } else {
            err = err2;
            // At the highest step size continue moving in the same direction
            if (s == start_step) continue;
          }
        }
        break;
      } while (1);
    }
    for (int p = plane_off; p < WIENER_HALFWIN; ++p) {
      int skip = 0;
      do {
        if (plane_wiener->vfilter[p] - s >= tap_min[p]) {
          plane_wiener->vfilter[p] -= s;
          plane_wiener->vfilter[WIENER_WIN - p - 1] -= s;
          plane_wiener->vfilter[WIENER_HALFWIN] += 2 * s;
          err2 = try_restoration_unit(rsc, limits, rui);
          if (err2 > err) {
            plane_wiener->vfilter[p] += s;
            plane_wiener->vfilter[WIENER_WIN - p - 1] += s;
            plane_wiener->vfilter[WIENER_HALFWIN] -= 2 * s;
          } else {
            err = err2;
            skip = 1;
            // At the highest step size continue moving in the same direction
            if (s == start_step) continue;
          }
        }
        break;
      } while (1);
      if (skip) break;
      do {
        if (plane_wiener->vfilter[p] + s <= tap_max[p]) {
          plane_wiener->vfilter[p] += s;
          plane_wiener->vfilter[WIENER_WIN - p - 1] += s;
          plane_wiener->vfilter[WIENER_HALFWIN] -= 2 * s;
          err2 = try_restoration_unit(rsc, limits, rui);
          if (err2 > err) {
            plane_wiener->vfilter[p] -= s;
            plane_wiener->vfilter[WIENER_WIN - p - 1] -= s;
            plane_wiener->vfilter[WIENER_HALFWIN] += 2 * s;
          } else {
            err = err2;
            // At the highest step size continue moving in the same direction
            if (s == start_step) continue;
          }
        }
        break;
      } while (1);
    }
  }
  // printf("err post = %"PRId64"\n", err);
  return err;
}

static AOM_INLINE void search_wiener(
    const RestorationTileLimits *limits, int rest_unit_idx, void *priv,
    int32_t *tmpbuf, RestorationLineBuffers *rlbs,
    struct aom_internal_error_info *error_info) {
  (void)tmpbuf;
  (void)rlbs;
  (void)error_info;
  RestSearchCtxt *rsc = (RestSearchCtxt *)priv;
  RestUnitSearchInfo *rusi = &rsc->rusi[rest_unit_idx];

  const MACROBLOCK *const x = rsc->x;
  const int64_t bits_none = x->mode_costs.wiener_restore_cost[0];

  // Skip Wiener search for low variance contents
  if (rsc->lpf_sf->prune_wiener_based_on_src_var) {
    const int scale[3] = { 0, 1, 2 };
    // Obtain the normalized Qscale
    const int qs = av1_dc_quant_QTX(rsc->cm->quant_params.base_qindex, 0,
                                    rsc->cm->seq_params->bit_depth) >>
                   3;
    // Derive threshold as sqr(normalized Qscale) * scale / 16,
    const uint64_t thresh =
        (qs * qs * scale[rsc->lpf_sf->prune_wiener_based_on_src_var]) >> 4;
    const int highbd = rsc->cm->seq_params->use_highbitdepth;
    const uint64_t src_var =
        var_restoration_unit(limits, rsc->src, rsc->plane, highbd);
    // Do not perform Wiener search if source variance is lower than threshold
    // or if the reconstruction error is zero
    int prune_wiener = (src_var < thresh) || (rsc->sse[RESTORE_NONE] == 0);
    if (prune_wiener) {
      rsc->total_bits[RESTORE_WIENER] += bits_none;
      rsc->total_sse[RESTORE_WIENER] += rsc->sse[RESTORE_NONE];
      rusi->best_rtype[RESTORE_WIENER - 1] = RESTORE_NONE;
      rsc->sse[RESTORE_WIENER] = INT64_MAX;
      if (rsc->lpf_sf->prune_sgr_based_on_wiener == 2) rsc->skip_sgr_eval = 1;
      return;
    }
  }

  const int wiener_win =
      (rsc->plane == AOM_PLANE_Y) ? WIENER_WIN : WIENER_WIN_CHROMA;

  int reduced_wiener_win = wiener_win;
  if (rsc->lpf_sf->reduce_wiener_window_size) {
    reduced_wiener_win =
        (rsc->plane == AOM_PLANE_Y) ? WIENER_WIN_REDUCED : WIENER_WIN_CHROMA;
  }

  int64_t M[WIENER_WIN2];
  int64_t H[WIENER_WIN2 * WIENER_WIN2];
  int32_t vfilter[WIENER_WIN], hfilter[WIENER_WIN];

#if CONFIG_AV1_HIGHBITDEPTH
  const AV1_COMMON *const cm = rsc->cm;
  if (cm->seq_params->use_highbitdepth) {
    // TODO(any) : Add support for use_downsampled_wiener_stats SF in HBD
    // functions. Optimize intrinsics of HBD design similar to LBD (i.e.,
    // pre-calculate d and s buffers and avoid most of the C operations).
    av1_compute_stats_highbd(reduced_wiener_win, rsc->dgd_buffer,
                             rsc->src_buffer, rsc->dgd_avg, rsc->src_avg,
                             limits->h_start, limits->h_end, limits->v_start,
                             limits->v_end, rsc->dgd_stride, rsc->src_stride, M,
                             H, cm->seq_params->bit_depth);
  } else {
    av1_compute_stats(reduced_wiener_win, rsc->dgd_buffer, rsc->src_buffer,
                      rsc->dgd_avg, rsc->src_avg, limits->h_start,
                      limits->h_end, limits->v_start, limits->v_end,
                      rsc->dgd_stride, rsc->src_stride, M, H,
                      rsc->lpf_sf->use_downsampled_wiener_stats);
  }
#else
  av1_compute_stats(reduced_wiener_win, rsc->dgd_buffer, rsc->src_buffer,
                    rsc->dgd_avg, rsc->src_avg, limits->h_start, limits->h_end,
                    limits->v_start, limits->v_end, rsc->dgd_stride,
                    rsc->src_stride, M, H,
                    rsc->lpf_sf->use_downsampled_wiener_stats);
#endif

  wiener_decompose_sep_sym(reduced_wiener_win, M, H, vfilter, hfilter);

  RestorationUnitInfo rui;
  memset(&rui, 0, sizeof(rui));
  rui.restoration_type = RESTORE_WIENER;
  finalize_sym_filter(reduced_wiener_win, vfilter, rui.wiener_info.vfilter);
  finalize_sym_filter(reduced_wiener_win, hfilter, rui.wiener_info.hfilter);

  // Filter score computes the value of the function x'*A*x - x'*b for the
  // learned filter and compares it against identity filer. If there is no
  // reduction in the function, the filter is reverted back to identity
  if (compute_score(reduced_wiener_win, M, H, rui.wiener_info.vfilter,
                    rui.wiener_info.hfilter) > 0) {
    rsc->total_bits[RESTORE_WIENER] += bits_none;
    rsc->total_sse[RESTORE_WIENER] += rsc->sse[RESTORE_NONE];
    rusi->best_rtype[RESTORE_WIENER - 1] = RESTORE_NONE;
    rsc->sse[RESTORE_WIENER] = INT64_MAX;
    if (rsc->lpf_sf->prune_sgr_based_on_wiener == 2) rsc->skip_sgr_eval = 1;
    return;
  }

  rsc->sse[RESTORE_WIENER] =
      finer_search_wiener(rsc, limits, &rui, reduced_wiener_win);
  rusi->wiener = rui.wiener_info;

  if (reduced_wiener_win != WIENER_WIN) {
    assert(rui.wiener_info.vfilter[0] == 0 &&
           rui.wiener_info.vfilter[WIENER_WIN - 1] == 0);
    assert(rui.wiener_info.hfilter[0] == 0 &&
           rui.wiener_info.hfilter[WIENER_WIN - 1] == 0);
  }

  const int64_t bits_wiener =
      x->mode_costs.wiener_restore_cost[1] +
      (count_wiener_bits(wiener_win, &rusi->wiener, &rsc->ref_wiener)
       << AV1_PROB_COST_SHIFT);

  double cost_none = RDCOST_DBL_WITH_NATIVE_BD_DIST(
      x->rdmult, bits_none >> 4, rsc->sse[RESTORE_NONE],
      rsc->cm->seq_params->bit_depth);
  double cost_wiener = RDCOST_DBL_WITH_NATIVE_BD_DIST(
      x->rdmult, bits_wiener >> 4, rsc->sse[RESTORE_WIENER],
      rsc->cm->seq_params->bit_depth);

  RestorationType rtype =
      (cost_wiener < cost_none) ? RESTORE_WIENER : RESTORE_NONE;
  rusi->best_rtype[RESTORE_WIENER - 1] = rtype;

  // Set 'skip_sgr_eval' based on rdcost ratio of RESTORE_WIENER and
  // RESTORE_NONE or based on best_rtype
  if (rsc->lpf_sf->prune_sgr_based_on_wiener == 1) {
    rsc->skip_sgr_eval = cost_wiener > (1.01 * cost_none);
  } else if (rsc->lpf_sf->prune_sgr_based_on_wiener == 2) {
    rsc->skip_sgr_eval = rusi->best_rtype[RESTORE_WIENER - 1] == RESTORE_NONE;
  }

#if DEBUG_LR_COSTING
  // Store ref params for later checking
  lr_ref_params[RESTORE_WIENER][rsc->plane][rest_unit_idx].wiener_info =
      rsc->ref_wiener;
#endif  // DEBUG_LR_COSTING

  rsc->total_sse[RESTORE_WIENER] += rsc->sse[rtype];
  rsc->total_bits[RESTORE_WIENER] +=
      (cost_wiener < cost_none) ? bits_wiener : bits_none;
  if (cost_wiener < cost_none) rsc->ref_wiener = rusi->wiener;
}

static AOM_INLINE void search_norestore(
    const RestorationTileLimits *limits, int rest_unit_idx, void *priv,
    int32_t *tmpbuf, RestorationLineBuffers *rlbs,
    struct aom_internal_error_info *error_info) {
  (void)rest_unit_idx;
  (void)tmpbuf;
  (void)rlbs;
  (void)error_info;

  RestSearchCtxt *rsc = (RestSearchCtxt *)priv;

  const int highbd = rsc->cm->seq_params->use_highbitdepth;
  rsc->sse[RESTORE_NONE] = sse_restoration_unit(
      limits, rsc->src, &rsc->cm->cur_frame->buf, rsc->plane, highbd);

  rsc->total_sse[RESTORE_NONE] += rsc->sse[RESTORE_NONE];
}

static AOM_INLINE void search_switchable(
    const RestorationTileLimits *limits, int rest_unit_idx, void *priv,
    int32_t *tmpbuf, RestorationLineBuffers *rlbs,
    struct aom_internal_error_info *error_info) {
  (void)limits;
  (void)tmpbuf;
  (void)rlbs;
  (void)error_info;
  RestSearchCtxt *rsc = (RestSearchCtxt *)priv;
  RestUnitSearchInfo *rusi = &rsc->rusi[rest_unit_idx];

  const MACROBLOCK *const x = rsc->x;

  const int wiener_win =
      (rsc->plane == AOM_PLANE_Y) ? WIENER_WIN : WIENER_WIN_CHROMA;

  double best_cost = 0;
  int64_t best_bits = 0;
  RestorationType best_rtype = RESTORE_NONE;

  for (RestorationType r = 0; r < RESTORE_SWITCHABLE_TYPES; ++r) {
    // If this restoration mode was skipped, or could not find a solution
    // that was better than RESTORE_NONE, then we can't select it here either.
    //
    // Note: It is possible for the restoration search functions to find a
    // filter which is better than RESTORE_NONE when looking purely at SSE, but
    // for it to be rejected overall due to its rate cost. In this case, there
    // is a chance that it may be have a lower rate cost when looking at
    // RESTORE_SWITCHABLE, and so it might be acceptable here.
    //
    // Therefore we prune based on SSE, rather than on whether or not the
    // previous search function selected this mode.
    if (r > RESTORE_NONE) {
      if (rsc->sse[r] > rsc->sse[RESTORE_NONE]) continue;
    }

    const int64_t sse = rsc->sse[r];
    int64_t coeff_pcost = 0;
    switch (r) {
      case RESTORE_NONE: coeff_pcost = 0; break;
      case RESTORE_WIENER:
        coeff_pcost = count_wiener_bits(wiener_win, &rusi->wiener,
                                        &rsc->switchable_ref_wiener);
        break;
      case RESTORE_SGRPROJ:
        coeff_pcost =
            count_sgrproj_bits(&rusi->sgrproj, &rsc->switchable_ref_sgrproj);
        break;
      default: assert(0); break;
    }
    const int64_t coeff_bits = coeff_pcost << AV1_PROB_COST_SHIFT;
    const int64_t bits = x->mode_costs.switchable_restore_cost[r] + coeff_bits;
    double cost = RDCOST_DBL_WITH_NATIVE_BD_DIST(
        x->rdmult, bits >> 4, sse, rsc->cm->seq_params->bit_depth);
    if (r == RESTORE_SGRPROJ && rusi->sgrproj.ep < 10)
      cost *= (1 + DUAL_SGR_PENALTY_MULT * rsc->lpf_sf->dual_sgr_penalty_level);
    if (r == 0 || cost < best_cost) {
      best_cost = cost;
      best_bits = bits;
      best_rtype = r;
    }
  }

  rusi->best_rtype[RESTORE_SWITCHABLE - 1] = best_rtype;

#if DEBUG_LR_COSTING
  // Store ref params for later checking
  lr_ref_params[RESTORE_SWITCHABLE][rsc->plane][rest_unit_idx].wiener_info =
      rsc->switchable_ref_wiener;
  lr_ref_params[RESTORE_SWITCHABLE][rsc->plane][rest_unit_idx].sgrproj_info =
      rsc->switchable_ref_sgrproj;
#endif  // DEBUG_LR_COSTING

  rsc->total_sse[RESTORE_SWITCHABLE] += rsc->sse[best_rtype];
  rsc->total_bits[RESTORE_SWITCHABLE] += best_bits;
  if (best_rtype == RESTORE_WIENER) rsc->switchable_ref_wiener = rusi->wiener;
  if (best_rtype == RESTORE_SGRPROJ)
    rsc->switchable_ref_sgrproj = rusi->sgrproj;
}

static AOM_INLINE void copy_unit_info(RestorationType frame_rtype,
                                      const RestUnitSearchInfo *rusi,
                                      RestorationUnitInfo *rui) {
  assert(frame_rtype > 0);
  rui->restoration_type = rusi->best_rtype[frame_rtype - 1];
  if (rui->restoration_type == RESTORE_WIENER)
    rui->wiener_info = rusi->wiener;
  else
    rui->sgrproj_info = rusi->sgrproj;
}

static void restoration_search(AV1_COMMON *cm, int plane, RestSearchCtxt *rsc,
                               bool *disable_lr_filter) {
  const BLOCK_SIZE sb_size = cm->seq_params->sb_size;
  const int mib_size_log2 = cm->seq_params->mib_size_log2;
  const CommonTileParams *tiles = &cm->tiles;
  const int is_uv = plane > 0;
  const int ss_y = is_uv && cm->seq_params->subsampling_y;
  RestorationInfo *rsi = &cm->rst_info[plane];
  const int ru_size = rsi->restoration_unit_size;
  const int ext_size = ru_size * 3 / 2;

  int plane_w, plane_h;
  av1_get_upsampled_plane_size(cm, is_uv, &plane_w, &plane_h);

  static const rest_unit_visitor_t funs[RESTORE_TYPES] = {
    search_norestore, search_wiener, search_sgrproj, search_switchable
  };

  const int plane_num_units = rsi->num_rest_units;
  const RestorationType num_rtypes =
      (plane_num_units > 1) ? RESTORE_TYPES : RESTORE_SWITCHABLE_TYPES;

  reset_rsc(rsc);

  // Iterate over restoration units in encoding order, so that each RU gets
  // the correct reference parameters when we cost it up. This is effectively
  // a nested iteration over:
  // * Each tile, order does not matter
  //   * Each superblock within that tile, in raster order
  //     * Each LR unit which is coded within that superblock, in raster order
  for (int tile_row = 0; tile_row < tiles->rows; tile_row++) {
    int sb_row_start = tiles->row_start_sb[tile_row];
    int sb_row_end = tiles->row_start_sb[tile_row + 1];
    for (int tile_col = 0; tile_col < tiles->cols; tile_col++) {
      int sb_col_start = tiles->col_start_sb[tile_col];
      int sb_col_end = tiles->col_start_sb[tile_col + 1];

      // Reset reference parameters for delta-coding at the start of each tile
      rsc_on_tile(rsc);

      for (int sb_row = sb_row_start; sb_row < sb_row_end; sb_row++) {
        int mi_row = sb_row << mib_size_log2;
        for (int sb_col = sb_col_start; sb_col < sb_col_end; sb_col++) {
          int mi_col = sb_col << mib_size_log2;

          int rcol0, rcol1, rrow0, rrow1;
          int has_lr_info = av1_loop_restoration_corners_in_sb(
              cm, plane, mi_row, mi_col, sb_size, &rcol0, &rcol1, &rrow0,
              &rrow1);

          if (!has_lr_info) continue;

          RestorationTileLimits limits;
          for (int rrow = rrow0; rrow < rrow1; rrow++) {
            int y0 = rrow * ru_size;
            int remaining_h = plane_h - y0;
            int h = (remaining_h < ext_size) ? remaining_h : ru_size;

            limits.v_start = y0;
            limits.v_end = y0 + h;
            assert(limits.v_end <= plane_h);
            // Offset upwards to align with the restoration processing stripe
            const int voffset = RESTORATION_UNIT_OFFSET >> ss_y;
            limits.v_start = AOMMAX(0, limits.v_start - voffset);
            if (limits.v_end < plane_h) limits.v_end -= voffset;

            for (int rcol = rcol0; rcol < rcol1; rcol++) {
              int x0 = rcol * ru_size;
              int remaining_w = plane_w - x0;
              int w = (remaining_w < ext_size) ? remaining_w : ru_size;

              limits.h_start = x0;
              limits.h_end = x0 + w;
              assert(limits.h_end <= plane_w);

              const int unit_idx = rrow * rsi->horz_units + rcol;

              rsc->skip_sgr_eval = 0;
              for (RestorationType r = RESTORE_NONE; r < num_rtypes; r++) {
                if (disable_lr_filter[r]) continue;

                funs[r](&limits, unit_idx, rsc, rsc->cm->rst_tmpbuf, NULL,
                        cm->error);
              }
            }
          }
        }
      }
    }
  }
}

static INLINE void av1_derive_flags_for_lr_processing(
    const LOOP_FILTER_SPEED_FEATURES *lpf_sf, bool *disable_lr_filter) {
  const bool is_wiener_disabled = lpf_sf->disable_wiener_filter;
  const bool is_sgr_disabled = lpf_sf->disable_sgr_filter;

  // Enable None Loop restoration filter if either of Wiener or Self-guided is
  // enabled.
  disable_lr_filter[RESTORE_NONE] = (is_wiener_disabled && is_sgr_disabled);

  disable_lr_filter[RESTORE_WIENER] = is_wiener_disabled;
  disable_lr_filter[RESTORE_SGRPROJ] = is_sgr_disabled;

  // Enable Swicthable Loop restoration filter if both of the Wiener and
  // Self-guided are enabled.
  disable_lr_filter[RESTORE_SWITCHABLE] =
      (is_wiener_disabled || is_sgr_disabled);
}

#define COUPLED_CHROMA_FROM_LUMA_RESTORATION 0
// Allocate both decoder-side and encoder-side info structs for a single plane.
// The unit size passed in should be the minimum size which we are going to
// search; before each search, set_restoration_unit_size() must be called to
// configure the actual size.
static RestUnitSearchInfo *allocate_search_structs(AV1_COMMON *cm,
                                                   RestorationInfo *rsi,
                                                   int is_uv,
                                                   int min_luma_unit_size) {
#if COUPLED_CHROMA_FROM_LUMA_RESTORATION
  int sx = cm->seq_params.subsampling_x;
  int sy = cm->seq_params.subsampling_y;
  int s = (p > 0) ? AOMMIN(sx, sy) : 0;
#else
  int s = 0;
#endif  // !COUPLED_CHROMA_FROM_LUMA_RESTORATION
  int min_unit_size = min_luma_unit_size >> s;

  int plane_w, plane_h;
  av1_get_upsampled_plane_size(cm, is_uv, &plane_w, &plane_h);

  const int max_horz_units = av1_lr_count_units(min_unit_size, plane_w);
  const int max_vert_units = av1_lr_count_units(min_unit_size, plane_h);
  const int max_num_units = max_horz_units * max_vert_units;

  aom_free(rsi->unit_info);
  CHECK_MEM_ERROR(cm, rsi->unit_info,
                  (RestorationUnitInfo *)aom_memalign(
                      16, sizeof(*rsi->unit_info) * max_num_units));

  RestUnitSearchInfo *rusi;
  CHECK_MEM_ERROR(
      cm, rusi,
      (RestUnitSearchInfo *)aom_memalign(16, sizeof(*rusi) * max_num_units));

  // If the restoration unit dimensions are not multiples of
  // rsi->restoration_unit_size then some elements of the rusi array may be
  // left uninitialised when we reach copy_unit_info(...). This is not a
  // problem, as these elements are ignored later, but in order to quiet
  // Valgrind's warnings we initialise the array below.
  memset(rusi, 0, sizeof(*rusi) * max_num_units);

  return rusi;
}

static void set_restoration_unit_size(AV1_COMMON *cm, RestorationInfo *rsi,
                                      int is_uv, int luma_unit_size) {
#if COUPLED_CHROMA_FROM_LUMA_RESTORATION
  int sx = cm->seq_params.subsampling_x;
  int sy = cm->seq_params.subsampling_y;
  int s = (p > 0) ? AOMMIN(sx, sy) : 0;
#else
  int s = 0;
#endif  // !COUPLED_CHROMA_FROM_LUMA_RESTORATION
  int unit_size = luma_unit_size >> s;

  int plane_w, plane_h;
  av1_get_upsampled_plane_size(cm, is_uv, &plane_w, &plane_h);

  const int horz_units = av1_lr_count_units(unit_size, plane_w);
  const int vert_units = av1_lr_count_units(unit_size, plane_h);

  rsi->restoration_unit_size = unit_size;
  rsi->num_rest_units = horz_units * vert_units;
  rsi->horz_units = horz_units;
  rsi->vert_units = vert_units;
}

void av1_pick_filter_restoration(const YV12_BUFFER_CONFIG *src, AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->td.mb;
  const SequenceHeader *const seq_params = cm->seq_params;
  const LOOP_FILTER_SPEED_FEATURES *lpf_sf = &cpi->sf.lpf_sf;
  const int num_planes = av1_num_planes(cm);
  const int highbd = cm->seq_params->use_highbitdepth;
  assert(!cm->features.all_lossless);

  av1_fill_lr_rates(&x->mode_costs, x->e_mbd.tile_ctx);

  // Select unit size based on speed feature settings, and allocate
  // rui structs based on this size
  int min_lr_unit_size = cpi->sf.lpf_sf.min_lr_unit_size;
  int max_lr_unit_size = cpi->sf.lpf_sf.max_lr_unit_size;

  // The minimum allowed unit size at a syntax level is 1 superblock.
  // Apply this constraint here so that the speed features code which sets
  // cpi->sf.lpf_sf.min_lr_unit_size does not need to know the superblock size
  min_lr_unit_size =
      AOMMAX(min_lr_unit_size, block_size_wide[cm->seq_params->sb_size]);

  for (int plane = 0; plane < num_planes; ++plane) {
    cpi->pick_lr_ctxt.rusi[plane] = allocate_search_structs(
        cm, &cm->rst_info[plane], plane > 0, min_lr_unit_size);
  }

  x->rdmult = cpi->rd.RDMULT;

  // Allocate the frame buffer trial_frame_rst, which is used to temporarily
  // store the loop restored frame.
  if (aom_realloc_frame_buffer(
          &cpi->trial_frame_rst, cm->superres_upscaled_width,
          cm->superres_upscaled_height, seq_params->subsampling_x,
          seq_params->subsampling_y, highbd, AOM_RESTORATION_FRAME_BORDER,
          cm->features.byte_alignment, NULL, NULL, NULL, false, 0))
    aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate trial restored frame buffer");

  RestSearchCtxt rsc;

  // The buffers 'src_avg' and 'dgd_avg' are used to compute H and M buffers.
  // These buffers are only required for the AVX2 and NEON implementations of
  // av1_compute_stats. The buffer size required is calculated based on maximum
  // width and height of the LRU (i.e., from foreach_rest_unit_in_plane() 1.5
  // times the RESTORATION_UNITSIZE_MAX) allowed for Wiener filtering. The width
  // and height aligned to multiple of 16 is considered for intrinsic purpose.
  rsc.dgd_avg = NULL;
  rsc.src_avg = NULL;
#if HAVE_AVX2 || HAVE_SVE
  // The buffers allocated below are used during Wiener filter processing.
  // Hence, allocate the same when Wiener filter is enabled. Make sure to
  // allocate these buffers only for the SIMD extensions that make use of them
  // (i.e. AVX2 for low bitdepth and SVE for low and high bitdepth).
#if HAVE_AVX2
  bool allocate_buffers = !cpi->sf.lpf_sf.disable_wiener_filter && !highbd;
#elif HAVE_SVE
  bool allocate_buffers = !cpi->sf.lpf_sf.disable_wiener_filter;
#endif
  if (allocate_buffers) {
    const int buf_size = sizeof(*cpi->pick_lr_ctxt.dgd_avg) * 6 *
                         RESTORATION_UNITSIZE_MAX * RESTORATION_UNITSIZE_MAX;
    CHECK_MEM_ERROR(cm, cpi->pick_lr_ctxt.dgd_avg,
                    (int16_t *)aom_memalign(32, buf_size));

    rsc.dgd_avg = cpi->pick_lr_ctxt.dgd_avg;
    // When LRU width isn't multiple of 16, the 256 bits load instruction used
    // in AVX2 intrinsic can read data beyond valid LRU. Hence, in order to
    // silence Valgrind warning this buffer is initialized with zero. Overhead
    // due to this initialization is negligible since it is done at frame level.
    memset(rsc.dgd_avg, 0, buf_size);
    rsc.src_avg =
        rsc.dgd_avg + 3 * RESTORATION_UNITSIZE_MAX * RESTORATION_UNITSIZE_MAX;
    // Asserts the starting address of src_avg is always 32-bytes aligned.
    assert(!((intptr_t)rsc.src_avg % 32));
  }
#endif

  // Initialize all planes, so that any planes we skip searching will still have
  // valid data
  for (int plane = 0; plane < num_planes; plane++) {
    cm->rst_info[plane].frame_restoration_type = RESTORE_NONE;
  }

  // Decide which planes to search
  int plane_start, plane_end;

  if (lpf_sf->disable_loop_restoration_luma) {
    plane_start = AOM_PLANE_U;
  } else {
    plane_start = AOM_PLANE_Y;
  }

  if (num_planes == 1 || lpf_sf->disable_loop_restoration_chroma) {
    plane_end = AOM_PLANE_Y;
  } else {
    plane_end = AOM_PLANE_V;
  }

  // Derive the flags to enable/disable Loop restoration filters based on the
  // speed features 'disable_wiener_filter' and 'disable_sgr_filter'.
  bool disable_lr_filter[RESTORE_TYPES] = { false };
  av1_derive_flags_for_lr_processing(lpf_sf, disable_lr_filter);

  for (int plane = plane_start; plane <= plane_end; plane++) {
    const YV12_BUFFER_CONFIG *dgd = &cm->cur_frame->buf;
    const int is_uv = plane != AOM_PLANE_Y;
    int plane_w, plane_h;
    av1_get_upsampled_plane_size(cm, is_uv, &plane_w, &plane_h);
    av1_extend_frame(dgd->buffers[plane], plane_w, plane_h, dgd->strides[is_uv],
                     RESTORATION_BORDER, RESTORATION_BORDER, highbd);
  }

  double best_cost = DBL_MAX;
  int best_luma_unit_size = max_lr_unit_size;
  for (int luma_unit_size = max_lr_unit_size;
       luma_unit_size >= min_lr_unit_size; luma_unit_size >>= 1) {
    int64_t bits_this_size = 0;
    int64_t sse_this_size = 0;
    RestorationType best_rtype[MAX_MB_PLANE] = { RESTORE_NONE, RESTORE_NONE,
                                                 RESTORE_NONE };
    for (int plane = plane_start; plane <= plane_end; ++plane) {
      set_restoration_unit_size(cm, &cm->rst_info[plane], plane > 0,
                                luma_unit_size);
      init_rsc(src, &cpi->common, x, lpf_sf, plane,
               cpi->pick_lr_ctxt.rusi[plane], &cpi->trial_frame_rst, &rsc);

      restoration_search(cm, plane, &rsc, disable_lr_filter);

      const int plane_num_units = cm->rst_info[plane].num_rest_units;
      const RestorationType num_rtypes =
          (plane_num_units > 1) ? RESTORE_TYPES : RESTORE_SWITCHABLE_TYPES;
      double best_cost_this_plane = DBL_MAX;
      for (RestorationType r = 0; r < num_rtypes; ++r) {
        // Disable Loop restoration filter based on the flags set using speed
        // feature 'disable_wiener_filter' and 'disable_sgr_filter'.
        if (disable_lr_filter[r]) continue;

        double cost_this_plane = RDCOST_DBL_WITH_NATIVE_BD_DIST(
            x->rdmult, rsc.total_bits[r] >> 4, rsc.total_sse[r],
            cm->seq_params->bit_depth);

        if (cost_this_plane < best_cost_this_plane) {
          best_cost_this_plane = cost_this_plane;
          best_rtype[plane] = r;
        }
      }

      bits_this_size += rsc.total_bits[best_rtype[plane]];
      sse_this_size += rsc.total_sse[best_rtype[plane]];
    }

    double cost_this_size = RDCOST_DBL_WITH_NATIVE_BD_DIST(
        x->rdmult, bits_this_size >> 4, sse_this_size,
        cm->seq_params->bit_depth);

    if (cost_this_size < best_cost) {
      best_cost = cost_this_size;
      best_luma_unit_size = luma_unit_size;
      // Copy parameters out of rusi struct, before we overwrite it at
      // the start of the next iteration
      bool all_none = true;
      for (int plane = plane_start; plane <= plane_end; ++plane) {
        cm->rst_info[plane].frame_restoration_type = best_rtype[plane];
        if (best_rtype[plane] != RESTORE_NONE) {
          all_none = false;
          const int plane_num_units = cm->rst_info[plane].num_rest_units;
          for (int u = 0; u < plane_num_units; ++u) {
            copy_unit_info(best_rtype[plane], &cpi->pick_lr_ctxt.rusi[plane][u],
                           &cm->rst_info[plane].unit_info[u]);
          }
        }
      }
      // Heuristic: If all best_rtype entries are RESTORE_NONE, this means we
      // couldn't find any good filters at this size. So we likely won't find
      // any good filters at a smaller size either, so skip
      if (all_none) {
        break;
      }
    } else {
      // Heuristic: If this size is worse than the previous (larger) size, then
      // the next size down will likely be even worse, so skip
      break;
    }
  }

  // Final fixup to set the correct unit size
  // We set this for all planes, even ones we have skipped searching,
  // so that other code does not need to care which planes were and weren't
  // searched
  for (int plane = 0; plane < num_planes; ++plane) {
    set_restoration_unit_size(cm, &cm->rst_info[plane], plane > 0,
                              best_luma_unit_size);
  }

#if HAVE_AVX2 || HAVE_SVE
#if HAVE_AVX2
  bool free_buffers = !cpi->sf.lpf_sf.disable_wiener_filter && !highbd;
#elif HAVE_SVE
  bool free_buffers = !cpi->sf.lpf_sf.disable_wiener_filter;
#endif
  if (free_buffers) {
    aom_free(cpi->pick_lr_ctxt.dgd_avg);
    cpi->pick_lr_ctxt.dgd_avg = NULL;
  }
#endif
  for (int plane = 0; plane < num_planes; plane++) {
    aom_free(cpi->pick_lr_ctxt.rusi[plane]);
    cpi->pick_lr_ctxt.rusi[plane] = NULL;
  }
}
