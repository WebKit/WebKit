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
#ifndef AOM_AV1_ENCODER_PICKRST_H_
#define AOM_AV1_ENCODER_PICKRST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "av1/encoder/encoder.h"

struct yv12_buffer_config;
struct AV1_COMP;

static const uint8_t g_shuffle_stats_data[16] = {
  0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8,
};

static const uint8_t g_shuffle_stats_highbd_data[32] = {
  0, 1, 2, 3, 2, 3, 4, 5, 4, 5, 6, 7, 6, 7, 8, 9,
  0, 1, 2, 3, 2, 3, 4, 5, 4, 5, 6, 7, 6, 7, 8, 9,
};

static INLINE uint8_t find_average(const uint8_t *src, int h_start, int h_end,
                                   int v_start, int v_end, int stride) {
  uint64_t sum = 0;
  for (int i = v_start; i < v_end; i++) {
    for (int j = h_start; j < h_end; j++) {
      sum += src[i * stride + j];
    }
  }
  uint64_t avg = sum / ((v_end - v_start) * (h_end - h_start));
  return (uint8_t)avg;
}

#if CONFIG_AV1_HIGHBITDEPTH
static INLINE uint16_t find_average_highbd(const uint16_t *src, int h_start,
                                           int h_end, int v_start, int v_end,
                                           int stride) {
  uint64_t sum = 0;
  for (int i = v_start; i < v_end; i++) {
    for (int j = h_start; j < h_end; j++) {
      sum += src[i * stride + j];
    }
  }
  uint64_t avg = sum / ((v_end - v_start) * (h_end - h_start));
  return (uint16_t)avg;
}
#endif

/*!\brief Algorithm for AV1 loop restoration search and estimation.
 *
 * \ingroup in_loop_restoration
 * This function determines proper restoration filter types and
 * associated parameters for each restoration unit in a frame.
 *
 * \param[in]       sd           Source frame buffer
 * \param[in,out]   cpi          Top-level encoder structure
 *
 * \return Nothing is returned. Instead, chosen restoration filter
 * types and parameters are stored per plane in the \c rst_info structure
 * of type \ref RestorationInfo inside \c cpi->common:
 * \arg \c rst_info[ \c 0 ]: Chosen parameters for Y plane
 * \arg \c rst_info[ \c 1 ]: Chosen parameters for U plane if it exists
 * \arg \c rst_info[ \c 2 ]: Chosen parameters for V plane if it exists
 * \par
 * The following fields in each \c rst_info[ \c p], \c p = 0, 1, 2
 * are populated:
 * \arg \c rst_info[ \c p ].\c frame_restoration_type
 * \arg \c rst_info[ \c p ].\c unit_info[ \c u ],
 * for each \c u in 0, 1, ..., \c n( \c p ) - 1,
 * where \c n( \c p ) is the number of restoration units in plane \c p.
 * \par
 * The following fields in each \c rst_info[ \c p ].\c unit_info[ \c u ],
 * \c p = 0, 1, 2 and \c u = 0, 1, ..., \c n( \c p ) - 1, of type
 * \ref RestorationUnitInfo are populated:
 * \arg \c rst_info[ \c p ].\c unit_info[ \c u ].\c restoration_type
 * \arg \c rst_info[ \c p ].\c unit_info[ \c u ].\c wiener_info OR
 *      \c rst_info[ \c p ].\c unit_info[ \c u ].\c sgrproj_info OR
 *      neither, depending on
 *      \c rst_info[ \c p ].\c unit_info[ \c u ].\c restoration_type
 *
 */
void av1_pick_filter_restoration(const YV12_BUFFER_CONFIG *sd, AV1_COMP *cpi);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_PICKRST_H_
