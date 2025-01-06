/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/common/common.h"
#include "av1/encoder/external_partition.h"
#include "config/aom_config.h"

aom_codec_err_t av1_ext_part_create(aom_ext_part_funcs_t funcs,
                                    aom_ext_part_config_t config,
                                    ExtPartController *ext_part_controller) {
  if (ext_part_controller == NULL) {
    return AOM_CODEC_INVALID_PARAM;
  }
  ext_part_controller->funcs = funcs;
  ext_part_controller->config = config;
  const aom_ext_part_status_t status = ext_part_controller->funcs.create_model(
      ext_part_controller->funcs.priv, &ext_part_controller->config,
      &ext_part_controller->model);
  if (status == AOM_EXT_PART_ERROR) {
    return AOM_CODEC_ERROR;
  } else if (status == AOM_EXT_PART_TEST) {
    ext_part_controller->test_mode = 1;
    ext_part_controller->ready = 0;
    return AOM_CODEC_OK;
  }
  assert(status == AOM_EXT_PART_OK);
  ext_part_controller->ready = 1;
  return AOM_CODEC_OK;
}

static aom_codec_err_t ext_part_init(ExtPartController *ext_part_controller) {
  if (ext_part_controller == NULL) {
    return AOM_CODEC_INVALID_PARAM;
  }
  av1_zero(ext_part_controller);
  return AOM_CODEC_OK;
}

aom_codec_err_t av1_ext_part_delete(ExtPartController *ext_part_controller) {
  if (ext_part_controller == NULL) {
    return AOM_CODEC_INVALID_PARAM;
  }
  if (ext_part_controller->ready) {
    const aom_ext_part_status_t status =
        ext_part_controller->funcs.delete_model(ext_part_controller->model);
    if (status != AOM_EXT_PART_OK) {
      return AOM_CODEC_ERROR;
    }
  }
  return ext_part_init(ext_part_controller);
}

bool av1_ext_part_get_partition_decision(ExtPartController *ext_part_controller,
                                         aom_partition_decision_t *decision) {
  assert(ext_part_controller != NULL);
  assert(ext_part_controller->ready);
  assert(decision != NULL);
  const aom_ext_part_status_t status =
      ext_part_controller->funcs.get_partition_decision(
          ext_part_controller->model, decision);
  if (status != AOM_EXT_PART_OK) return false;
  return true;
}

bool av1_ext_part_send_features(ExtPartController *ext_part_controller,
                                const aom_partition_features_t *features) {
  assert(ext_part_controller != NULL);
  assert(ext_part_controller->ready);
  assert(features != NULL);
  const aom_ext_part_status_t status = ext_part_controller->funcs.send_features(
      ext_part_controller->model, features);
  if (status != AOM_EXT_PART_OK) return false;
  return true;
}

#if CONFIG_PARTITION_SEARCH_ORDER
bool av1_ext_part_send_partition_stats(ExtPartController *ext_part_controller,
                                       const aom_partition_stats_t *stats) {
  assert(ext_part_controller != NULL);
  assert(ext_part_controller->ready);
  assert(stats != NULL);
  const aom_ext_part_status_t status =
      ext_part_controller->funcs.send_partition_stats(
          ext_part_controller->model, stats);
  if (status != AOM_EXT_PART_OK) return false;
  return true;
}

aom_ext_part_decision_mode_t av1_get_ext_part_decision_mode(
    const ExtPartController *ext_part_controller) {
  return ext_part_controller->funcs.decision_mode;
}
#endif  // CONFIG_PARTITION_SEARCH_ORDER
