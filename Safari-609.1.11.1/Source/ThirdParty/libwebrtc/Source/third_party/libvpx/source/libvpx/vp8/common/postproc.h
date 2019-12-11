/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP8_COMMON_POSTPROC_H_
#define VP8_COMMON_POSTPROC_H_

#include "vpx_ports/mem.h"
struct postproc_state {
  int last_q;
  int last_noise;
  int last_base_qindex;
  int last_frame_valid;
  int clamp;
  int8_t *generated_noise;
};
#include "onyxc_int.h"
#include "ppflags.h"

#ifdef __cplusplus
extern "C" {
#endif
int vp8_post_proc_frame(struct VP8Common *oci, YV12_BUFFER_CONFIG *dest,
                        vp8_ppflags_t *flags);

void vp8_de_noise(struct VP8Common *oci, YV12_BUFFER_CONFIG *source,
                  YV12_BUFFER_CONFIG *post, int q, int low_var_thresh, int flag,
                  int uvfilter);

void vp8_deblock(struct VP8Common *oci, YV12_BUFFER_CONFIG *source,
                 YV12_BUFFER_CONFIG *post, int q, int low_var_thresh, int flag);

#define MFQE_PRECISION 4

void vp8_multiframe_quality_enhance(struct VP8Common *cm);
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP8_COMMON_POSTPROC_H_
