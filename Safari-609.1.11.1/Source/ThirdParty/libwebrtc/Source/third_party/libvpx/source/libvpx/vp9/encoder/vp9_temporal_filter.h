/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_ENCODER_VP9_TEMPORAL_FILTER_H_
#define VP9_ENCODER_VP9_TEMPORAL_FILTER_H_

#ifdef __cplusplus
extern "C" {
#endif

#define ARNR_FILT_QINDEX 128

void vp9_temporal_filter_init(void);
void vp9_temporal_filter(VP9_COMP *cpi, int distance);

void vp9_temporal_filter_iterate_row_c(VP9_COMP *cpi, ThreadData *td,
                                       int mb_row, int mb_col_start,
                                       int mb_col_end);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_VP9_TEMPORAL_FILTER_H_
