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
 * \brief Provides the high level interface to wrap decoder algorithms.
 *
 */
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>

#include "config/aom_config.h"
#include "config/aom_version.h"

#include "aom/aom_integer.h"
#include "aom/internal/aom_codec_internal.h"

int aom_codec_version(void) { return VERSION_PACKED; }

const char *aom_codec_version_str(void) { return VERSION_STRING_NOSP; }

const char *aom_codec_version_extra_str(void) { return VERSION_EXTRA; }

const char *aom_codec_iface_name(aom_codec_iface_t *iface) {
  return iface ? iface->name : "<invalid interface>";
}

const char *aom_codec_err_to_string(aom_codec_err_t err) {
  switch (err) {
    case AOM_CODEC_OK: return "Success";
    case AOM_CODEC_ERROR: return "Unspecified internal error";
    case AOM_CODEC_MEM_ERROR: return "Memory allocation error";
    case AOM_CODEC_ABI_MISMATCH: return "ABI version mismatch";
    case AOM_CODEC_INCAPABLE:
      return "Codec does not implement requested capability";
    case AOM_CODEC_UNSUP_BITSTREAM:
      return "Bitstream not supported by this decoder";
    case AOM_CODEC_UNSUP_FEATURE:
      return "Bitstream required feature not supported by this decoder";
    case AOM_CODEC_CORRUPT_FRAME: return "Corrupt frame detected";
    case AOM_CODEC_INVALID_PARAM: return "Invalid parameter";
    case AOM_CODEC_LIST_END: return "End of iterated list";
  }

  return "Unrecognized error code";
}

const char *aom_codec_error(const aom_codec_ctx_t *ctx) {
  return (ctx) ? aom_codec_err_to_string(ctx->err)
               : aom_codec_err_to_string(AOM_CODEC_INVALID_PARAM);
}

const char *aom_codec_error_detail(const aom_codec_ctx_t *ctx) {
  if (ctx && ctx->err)
    return ctx->priv ? ctx->priv->err_detail : ctx->err_detail;

  return NULL;
}

aom_codec_err_t aom_codec_destroy(aom_codec_ctx_t *ctx) {
  if (!ctx) {
    return AOM_CODEC_INVALID_PARAM;
  }
  if (!ctx->iface || !ctx->priv) {
    ctx->err = AOM_CODEC_ERROR;
    return AOM_CODEC_ERROR;
  }
  ctx->iface->destroy((aom_codec_alg_priv_t *)ctx->priv);
  ctx->iface = NULL;
  ctx->name = NULL;
  ctx->priv = NULL;
  ctx->err = AOM_CODEC_OK;
  return AOM_CODEC_OK;
}

aom_codec_caps_t aom_codec_get_caps(aom_codec_iface_t *iface) {
  return iface ? iface->caps : 0;
}

aom_codec_err_t aom_codec_control(aom_codec_ctx_t *ctx, int ctrl_id, ...) {
  if (!ctx) {
    return AOM_CODEC_INVALID_PARAM;
  }
  // Control ID must be non-zero.
  if (!ctrl_id) {
    ctx->err = AOM_CODEC_INVALID_PARAM;
    return AOM_CODEC_INVALID_PARAM;
  }
  if (!ctx->iface || !ctx->priv || !ctx->iface->ctrl_maps) {
    ctx->err = AOM_CODEC_ERROR;
    return AOM_CODEC_ERROR;
  }

  // "ctrl_maps" is an array of (control ID, function pointer) elements,
  // with CTRL_MAP_END as a sentinel.
  for (aom_codec_ctrl_fn_map_t *entry = ctx->iface->ctrl_maps;
       !at_ctrl_map_end(entry); ++entry) {
    if (entry->ctrl_id == ctrl_id) {
      va_list ap;
      va_start(ap, ctrl_id);
      ctx->err = entry->fn((aom_codec_alg_priv_t *)ctx->priv, ap);
      va_end(ap);
      return ctx->err;
    }
  }
  ctx->err = AOM_CODEC_ERROR;
  ctx->priv->err_detail = "Invalid control ID";
  return AOM_CODEC_ERROR;
}

aom_codec_err_t aom_codec_set_option(aom_codec_ctx_t *ctx, const char *name,
                                     const char *value) {
  if (!ctx) {
    return AOM_CODEC_INVALID_PARAM;
  }
  if (!ctx->iface || !ctx->priv || !ctx->iface->set_option) {
    ctx->err = AOM_CODEC_ERROR;
    return AOM_CODEC_ERROR;
  }
  ctx->err =
      ctx->iface->set_option((aom_codec_alg_priv_t *)ctx->priv, name, value);
  return ctx->err;
}

LIBAOM_FORMAT_PRINTF(3, 0)
static void set_error(struct aom_internal_error_info *info,
                      aom_codec_err_t error, const char *fmt, va_list ap) {
  info->error_code = error;
  info->has_detail = 0;

  if (fmt) {
    size_t sz = sizeof(info->detail);

    info->has_detail = 1;
    vsnprintf(info->detail, sz - 1, fmt, ap);
    info->detail[sz - 1] = '\0';
  }
}

void aom_set_error(struct aom_internal_error_info *info, aom_codec_err_t error,
                   const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  set_error(info, error, fmt, ap);
  va_end(ap);

  assert(!info->setjmp);
}

void aom_internal_error(struct aom_internal_error_info *info,
                        aom_codec_err_t error, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  set_error(info, error, fmt, ap);
  va_end(ap);

  if (info->setjmp) longjmp(info->jmp, info->error_code);
}

void aom_internal_error_copy(struct aom_internal_error_info *info,
                             const struct aom_internal_error_info *src) {
  assert(info != src);
  assert(!src->setjmp);

  if (!src->has_detail) {
    aom_internal_error(info, src->error_code, NULL);
  } else {
    aom_internal_error(info, src->error_code, "%s", src->detail);
  }
}

void aom_merge_corrupted_flag(int *corrupted, int value) {
  *corrupted |= value;
}

const char *aom_obu_type_to_string(OBU_TYPE type) {
  switch (type) {
    case OBU_SEQUENCE_HEADER: return "OBU_SEQUENCE_HEADER";
    case OBU_TEMPORAL_DELIMITER: return "OBU_TEMPORAL_DELIMITER";
    case OBU_FRAME_HEADER: return "OBU_FRAME_HEADER";
    case OBU_REDUNDANT_FRAME_HEADER: return "OBU_REDUNDANT_FRAME_HEADER";
    case OBU_FRAME: return "OBU_FRAME";
    case OBU_TILE_GROUP: return "OBU_TILE_GROUP";
    case OBU_METADATA: return "OBU_METADATA";
    case OBU_TILE_LIST: return "OBU_TILE_LIST";
    case OBU_PADDING: return "OBU_PADDING";
    default: break;
  }
  return "<Invalid OBU Type>";
}
