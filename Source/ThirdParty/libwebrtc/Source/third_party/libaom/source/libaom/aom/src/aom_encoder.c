/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/*!\file
 * \brief Provides the high level interface to wrap encoder algorithms.
 *
 */
#include "config/aom_config.h"

#if HAVE_FEXCEPT
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <fenv.h>
#endif

#include <limits.h>
#include <string.h>

#include "aom/aom_encoder.h"
#include "aom/internal/aom_codec_internal.h"

#define SAVE_STATUS(ctx, var) (ctx ? (ctx->err = var) : var)

static aom_codec_alg_priv_t *get_alg_priv(aom_codec_ctx_t *ctx) {
  return (aom_codec_alg_priv_t *)ctx->priv;
}

aom_codec_err_t aom_codec_enc_init_ver(aom_codec_ctx_t *ctx,
                                       aom_codec_iface_t *iface,
                                       const aom_codec_enc_cfg_t *cfg,
                                       aom_codec_flags_t flags, int ver) {
  aom_codec_err_t res;
  // The value of AOM_ENCODER_ABI_VERSION in libaom v3.0.0 and v3.1.0 - v3.1.3.
  //
  // We are compatible with these older libaom releases. AOM_ENCODER_ABI_VERSION
  // was incremented after these releases for two reasons:
  // 1. AOM_ENCODER_ABI_VERSION takes contribution from
  //    AOM_EXT_PART_ABI_VERSION. The external partition API is still
  //    experimental, so it should not be considered as part of the stable ABI.
  //    fd9ed8366 External partition: Define APIs
  //    https://aomedia-review.googlesource.com/c/aom/+/135663
  // 2. As a way to detect the presence of speeds 7-9 in all-intra mode. I (wtc)
  //    suggested this change because I misunderstood how
  //    AOM_ENCODER_ABI_VERSION was used.
  //    bbdfa68d1 AllIntra: Redefine all-intra mode speed features for speed 7+
  //    https://aomedia-review.googlesource.com/c/aom/+/140624
  const int aom_encoder_abi_version_25 = 25;

  // TODO(bug aomedia:3228): Remove the check for aom_encoder_abi_version_25 in
  // libaom v4.0.0.
  if (ver != AOM_ENCODER_ABI_VERSION && ver != aom_encoder_abi_version_25)
    res = AOM_CODEC_ABI_MISMATCH;
  else if (!ctx || !iface || !cfg)
    res = AOM_CODEC_INVALID_PARAM;
  else if (iface->abi_version != AOM_CODEC_INTERNAL_ABI_VERSION)
    res = AOM_CODEC_ABI_MISMATCH;
  else if (!(iface->caps & AOM_CODEC_CAP_ENCODER))
    res = AOM_CODEC_INCAPABLE;
  else if ((flags & AOM_CODEC_USE_PSNR) && !(iface->caps & AOM_CODEC_CAP_PSNR))
    res = AOM_CODEC_INCAPABLE;
  else if ((flags & AOM_CODEC_USE_HIGHBITDEPTH) &&
           !(iface->caps & AOM_CODEC_CAP_HIGHBITDEPTH)) {
    res = AOM_CODEC_INCAPABLE;
  } else if (cfg->g_bit_depth > 8 &&
             (flags & AOM_CODEC_USE_HIGHBITDEPTH) == 0) {
    res = AOM_CODEC_INVALID_PARAM;
    ctx->err_detail =
        "High bit-depth used without the AOM_CODEC_USE_HIGHBITDEPTH flag.";
  } else {
    ctx->iface = iface;
    ctx->name = iface->name;
    ctx->priv = NULL;
    ctx->init_flags = flags;
    ctx->config.enc = cfg;
    res = ctx->iface->init(ctx);

    if (res) {
      // IMPORTANT: ctx->priv->err_detail must be null or point to a string
      // that remains valid after ctx->priv is destroyed, such as a C string
      // literal. This makes it safe to call aom_codec_error_detail() after
      // aom_codec_enc_init_ver() failed.
      ctx->err_detail = ctx->priv ? ctx->priv->err_detail : NULL;
      aom_codec_destroy(ctx);
    }
  }

  return SAVE_STATUS(ctx, res);
}

aom_codec_err_t aom_codec_enc_config_default(aom_codec_iface_t *iface,
                                             aom_codec_enc_cfg_t *cfg,
                                             unsigned int usage) {
  aom_codec_err_t res;

  if (!iface || !cfg)
    res = AOM_CODEC_INVALID_PARAM;
  else if (!(iface->caps & AOM_CODEC_CAP_ENCODER))
    res = AOM_CODEC_INCAPABLE;
  else {
    res = AOM_CODEC_INVALID_PARAM;

    for (int i = 0; i < iface->enc.cfg_count; ++i) {
      if (iface->enc.cfgs[i].g_usage == usage) {
        *cfg = iface->enc.cfgs[i];
        res = AOM_CODEC_OK;
        /* default values */
        memset(&cfg->encoder_cfg, 0, sizeof(cfg->encoder_cfg));
        cfg->encoder_cfg.super_block_size = 0;  // Dynamic
        cfg->encoder_cfg.max_partition_size = 128;
        cfg->encoder_cfg.min_partition_size = 4;
        cfg->encoder_cfg.disable_trellis_quant = 3;
        break;
      }
    }
  }
  return res;
}

#if AOM_ARCH_X86 || AOM_ARCH_X86_64
/* On X86, disable the x87 unit's internal 80 bit precision for better
 * consistency with the SSE unit's 64 bit precision.
 */
#include "aom_ports/x86.h"
#define FLOATING_POINT_SET_PRECISION \
  unsigned short x87_orig_mode = x87_set_double_precision();
#define FLOATING_POINT_RESTORE_PRECISION x87_set_control_word(x87_orig_mode);
#else
#define FLOATING_POINT_SET_PRECISION
#define FLOATING_POINT_RESTORE_PRECISION
#endif  // AOM_ARCH_X86 || AOM_ARCH_X86_64

#if HAVE_FEXCEPT && CONFIG_DEBUG
#define FLOATING_POINT_SET_EXCEPTIONS \
  const int float_excepts =           \
      feenableexcept(FE_DIVBYZERO | FE_UNDERFLOW | FE_OVERFLOW);
#define FLOATING_POINT_RESTORE_EXCEPTIONS \
  if (float_excepts != -1) {              \
    fedisableexcept(FE_ALL_EXCEPT);       \
    feenableexcept(float_excepts);        \
  }
#else
#define FLOATING_POINT_SET_EXCEPTIONS
#define FLOATING_POINT_RESTORE_EXCEPTIONS
#endif  // HAVE_FEXCEPT && CONFIG_DEBUG

/* clang-format off */
#define FLOATING_POINT_INIT    \
  do {                         \
  FLOATING_POINT_SET_PRECISION \
  FLOATING_POINT_SET_EXCEPTIONS

#define FLOATING_POINT_RESTORE      \
  FLOATING_POINT_RESTORE_EXCEPTIONS \
  FLOATING_POINT_RESTORE_PRECISION  \
  } while (0);
/* clang-format on */

aom_codec_err_t aom_codec_encode(aom_codec_ctx_t *ctx, const aom_image_t *img,
                                 aom_codec_pts_t pts, unsigned long duration,
                                 aom_enc_frame_flags_t flags) {
  aom_codec_err_t res = AOM_CODEC_OK;

  if (!ctx || (img && !duration))
    res = AOM_CODEC_INVALID_PARAM;
  else if (!ctx->iface || !ctx->priv)
    res = AOM_CODEC_ERROR;
  else if (!(ctx->iface->caps & AOM_CODEC_CAP_ENCODER))
    res = AOM_CODEC_INCAPABLE;
  else if (img && ((img->fmt & AOM_IMG_FMT_HIGHBITDEPTH) != 0) !=
                      ((ctx->init_flags & AOM_CODEC_USE_HIGHBITDEPTH) != 0)) {
    res = AOM_CODEC_INVALID_PARAM;
  } else {
    /* Execute in a normalized floating point environment, if the platform
     * requires it.
     */
    FLOATING_POINT_INIT
    res = ctx->iface->enc.encode(get_alg_priv(ctx), img, pts, duration, flags);
    FLOATING_POINT_RESTORE
  }

  return SAVE_STATUS(ctx, res);
}

const aom_codec_cx_pkt_t *aom_codec_get_cx_data(aom_codec_ctx_t *ctx,
                                                aom_codec_iter_t *iter) {
  const aom_codec_cx_pkt_t *pkt = NULL;

  if (ctx) {
    if (!iter)
      ctx->err = AOM_CODEC_INVALID_PARAM;
    else if (!ctx->iface || !ctx->priv)
      ctx->err = AOM_CODEC_ERROR;
    else if (!(ctx->iface->caps & AOM_CODEC_CAP_ENCODER))
      ctx->err = AOM_CODEC_INCAPABLE;
    else
      pkt = ctx->iface->enc.get_cx_data(get_alg_priv(ctx), iter);
  }

  if (pkt && pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
    // If the application has specified a destination area for the
    // compressed data, and the codec has not placed the data there,
    // and it fits, copy it.
    aom_codec_priv_t *const priv = ctx->priv;
    char *const dst_buf = (char *)priv->enc.cx_data_dst_buf.buf;

    if (dst_buf && pkt->data.raw.buf != dst_buf &&
        pkt->data.raw.sz + priv->enc.cx_data_pad_before +
                priv->enc.cx_data_pad_after <=
            priv->enc.cx_data_dst_buf.sz) {
      aom_codec_cx_pkt_t *modified_pkt = &priv->enc.cx_data_pkt;

      memcpy(dst_buf + priv->enc.cx_data_pad_before, pkt->data.raw.buf,
             pkt->data.raw.sz);
      *modified_pkt = *pkt;
      modified_pkt->data.raw.buf = dst_buf;
      modified_pkt->data.raw.sz +=
          priv->enc.cx_data_pad_before + priv->enc.cx_data_pad_after;
      pkt = modified_pkt;
    }

    if (dst_buf == pkt->data.raw.buf) {
      priv->enc.cx_data_dst_buf.buf = dst_buf + pkt->data.raw.sz;
      priv->enc.cx_data_dst_buf.sz -= pkt->data.raw.sz;
    }
  }

  return pkt;
}

aom_codec_err_t aom_codec_set_cx_data_buf(aom_codec_ctx_t *ctx,
                                          const aom_fixed_buf_t *buf,
                                          unsigned int pad_before,
                                          unsigned int pad_after) {
  if (!ctx || !ctx->priv) return AOM_CODEC_INVALID_PARAM;

  if (buf) {
    ctx->priv->enc.cx_data_dst_buf = *buf;
    ctx->priv->enc.cx_data_pad_before = pad_before;
    ctx->priv->enc.cx_data_pad_after = pad_after;
  } else {
    ctx->priv->enc.cx_data_dst_buf.buf = NULL;
    ctx->priv->enc.cx_data_dst_buf.sz = 0;
    ctx->priv->enc.cx_data_pad_before = 0;
    ctx->priv->enc.cx_data_pad_after = 0;
  }

  return AOM_CODEC_OK;
}

const aom_image_t *aom_codec_get_preview_frame(aom_codec_ctx_t *ctx) {
  aom_image_t *img = NULL;

  if (ctx) {
    if (!ctx->iface || !ctx->priv)
      ctx->err = AOM_CODEC_ERROR;
    else if (!(ctx->iface->caps & AOM_CODEC_CAP_ENCODER))
      ctx->err = AOM_CODEC_INCAPABLE;
    else if (!ctx->iface->enc.get_preview)
      ctx->err = AOM_CODEC_INCAPABLE;
    else
      img = ctx->iface->enc.get_preview(get_alg_priv(ctx));
  }

  return img;
}

aom_fixed_buf_t *aom_codec_get_global_headers(aom_codec_ctx_t *ctx) {
  aom_fixed_buf_t *buf = NULL;

  if (ctx) {
    if (!ctx->iface || !ctx->priv)
      ctx->err = AOM_CODEC_ERROR;
    else if (!(ctx->iface->caps & AOM_CODEC_CAP_ENCODER))
      ctx->err = AOM_CODEC_INCAPABLE;
    else if (!ctx->iface->enc.get_glob_hdrs)
      ctx->err = AOM_CODEC_INCAPABLE;
    else
      buf = ctx->iface->enc.get_glob_hdrs(get_alg_priv(ctx));
  }

  return buf;
}

aom_codec_err_t aom_codec_enc_config_set(aom_codec_ctx_t *ctx,
                                         const aom_codec_enc_cfg_t *cfg) {
  aom_codec_err_t res;

  if (!ctx || !ctx->iface || !ctx->priv || !cfg)
    res = AOM_CODEC_INVALID_PARAM;
  else if (!(ctx->iface->caps & AOM_CODEC_CAP_ENCODER))
    res = AOM_CODEC_INCAPABLE;
  else
    res = ctx->iface->enc.cfg_set(get_alg_priv(ctx), cfg);

  return SAVE_STATUS(ctx, res);
}

int aom_codec_pkt_list_add(struct aom_codec_pkt_list *list,
                           const struct aom_codec_cx_pkt *pkt) {
  if (list->cnt < list->max) {
    list->pkts[list->cnt++] = *pkt;
    return 0;
  }

  return 1;
}

const aom_codec_cx_pkt_t *aom_codec_pkt_list_get(
    struct aom_codec_pkt_list *list, aom_codec_iter_t *iter) {
  const aom_codec_cx_pkt_t *pkt;

  if (!(*iter)) {
    *iter = list->pkts;
  }

  pkt = (const aom_codec_cx_pkt_t *)*iter;

  if ((size_t)(pkt - list->pkts) < list->cnt)
    *iter = pkt + 1;
  else
    pkt = NULL;

  return pkt;
}
