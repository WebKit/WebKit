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

#ifndef AOM_AV1_COMMON_QUANT_COMMON_H_
#define AOM_AV1_COMMON_QUANT_COMMON_H_

#include <stdbool.h>
#include "aom/aom_codec.h"
#include "av1/common/seg_common.h"
#include "av1/common/enums.h"
#include "av1/common/entropy.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MINQ 0
#define MAXQ 255
#define QINDEX_RANGE (MAXQ - MINQ + 1)
#define QINDEX_BITS 8
// Total number of QM sets stored
#define QM_LEVEL_BITS 4
#define NUM_QM_LEVELS (1 << QM_LEVEL_BITS)
/* Range of QMS is between first and last value, with offset applied to inter
 * blocks*/
#define DEFAULT_QM_Y 10
#define DEFAULT_QM_U 11
#define DEFAULT_QM_V 12
#define DEFAULT_QM_FIRST 5
#define DEFAULT_QM_LAST 9
#define DEFAULT_QM_FIRST_ALLINTRA 4
#define DEFAULT_QM_LAST_ALLINTRA 10
#define LOSSLESS_Q_STEP 4  // this should equal to dc/ac_qlookup_QTX[0]

struct AV1Common;
struct CommonQuantParams;
struct macroblockd;

int16_t av1_dc_quant_QTX(int qindex, int delta, aom_bit_depth_t bit_depth);
int16_t av1_ac_quant_QTX(int qindex, int delta, aom_bit_depth_t bit_depth);

int av1_get_qindex(const struct segmentation *seg, int segment_id,
                   int base_qindex);

// Returns true if we are using quantization matrix.
bool av1_use_qmatrix(const struct CommonQuantParams *quant_params,
                     const struct macroblockd *xd, int segment_id);

// Reduce the large number of quantizers to a smaller number of levels for which
// different matrices may be defined. This is an increasing function in qindex.
static inline int aom_get_qmlevel(int qindex, int first, int last) {
  return first + (qindex * (last + 1 - first)) / QINDEX_RANGE;
}

// QM levels tuned for allintra mode (including still images)
// This formula was empirically derived by encoding the CID22 validation
// testset for each QP/QM tuple, and building a convex hull that
// maximizes SSIMU2 scores, and a final subjective visual quality pass
// as a sanity check. This is a decreasing function in qindex.
static inline int aom_get_qmlevel_allintra(int qindex, int first, int last) {
  int qm_level = 0;

  if (qindex <= 40) {
    qm_level = 10;
  } else if (qindex <= 100) {
    qm_level = 9;
  } else if (qindex <= 160) {
    qm_level = 8;
  } else if (qindex <= 200) {
    qm_level = 7;
  } else if (qindex <= 220) {
    qm_level = 6;
  } else if (qindex <= 240) {
    qm_level = 5;
  } else {
    qm_level = 4;
  }

  return clamp(qm_level, first, last);
}

// Initialize all global quant/dequant matrices.
void av1_qm_init(struct CommonQuantParams *quant_params, int num_planes);

// Get either local / global dequant matrix as appropriate.
const qm_val_t *av1_get_iqmatrix(const struct CommonQuantParams *quant_params,
                                 const struct macroblockd *xd, int plane,
                                 TX_SIZE tx_size, TX_TYPE tx_type);
// Get either local / global quant matrix as appropriate.
const qm_val_t *av1_get_qmatrix(const struct CommonQuantParams *quant_params,
                                const struct macroblockd *xd, int plane,
                                TX_SIZE tx_size, TX_TYPE tx_type);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_COMMON_QUANT_COMMON_H_
