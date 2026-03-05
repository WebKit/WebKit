/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AOM_AOM_TPL_H_
#define AOM_AOM_AOM_TPL_H_

#include "./aom_integer.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!\brief Current ABI version number
 *
 * \internal
 * If this file is altered in any way that changes the ABI, this value
 * must be bumped.  Examples include, but are not limited to, changing
 * types, removing or reassigning enums, adding/removing/rearranging
 * fields to structures
 */
#define AOM_TPL_ABI_VERSION 1 /**<\hideinitializer*/

#define AOM_RC_INTER_REFS_PER_FRAME 7

// The motion vector in units of full pixel
typedef struct aom_fullpel_mv {
  int16_t row;
  int16_t col;
} AOM_FULLPEL_MV;

// The motion vector in units of 1/8-pel
typedef struct aom_mv {
  int16_t row;
  int16_t col;
} AOM_MV;

typedef union aom_int_mv {
  uint32_t as_int;
  AOM_MV as_mv;
  AOM_FULLPEL_MV as_fullmv;
} aom_int_mv; /* facilitates faster equality tests and copies */

/*!\brief Temporal dependency model stats for each block before propagation */
typedef struct AomTplBlockStats {
  int16_t row; /**< Pixel row of the top left corner */
  int16_t col; /**< Pixel col of the top left corner */
  int64_t srcrf_sse;
  int64_t srcrf_dist;
  int64_t recrf_sse;
  int64_t recrf_dist;
  int64_t intra_sse;
  int64_t intra_dist;
  int64_t cmp_recrf_dist[2];
  int64_t mc_dep_rate;
  int64_t mc_dep_dist;
  int64_t pred_error[AOM_RC_INTER_REFS_PER_FRAME];
  int32_t intra_cost;
  int32_t inter_cost;
  int32_t srcrf_rate;
  int32_t recrf_rate;
  int32_t intra_rate;
  int32_t cmp_recrf_rate[2];
  aom_int_mv mv[AOM_RC_INTER_REFS_PER_FRAME];
  int8_t ref_frame_index[2];
} AomTplBlockStats;

/*!\brief Temporal dependency model stats for each frame before propagation */
typedef struct AomTplFrameStats {
  int frame_width;  /**< Frame width */
  int frame_height; /**< Frame height */
  int num_blocks;   /**< Number of blocks. Size of block_stats_list */
  AomTplBlockStats *block_stats_list; /**< List of tpl stats for each block */
} AomTplFrameStats;

/*!\brief Temporal dependency model stats for each GOP before propagation */
typedef struct AomTplGopStats {
  int size; /**< GOP size, also the size of frame_stats_list. */
  AomTplFrameStats *frame_stats_list; /**< List of tpl stats for each frame */
} AomTplGopStats;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AOM_AOM_TPL_H_
