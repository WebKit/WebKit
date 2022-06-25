/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include "vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "vp8_rtcd.h"
#include "blockd.h"
#include "reconintra4x4.h"
#include "vp8/common/common.h"
#include "vpx_ports/compiler_attributes.h"

typedef void (*intra_pred_fn)(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left);

static intra_pred_fn pred[10];

void vp8_init_intra4x4_predictors_internal(void) {
  pred[B_DC_PRED] = vpx_dc_predictor_4x4;
  pred[B_TM_PRED] = vpx_tm_predictor_4x4;
  pred[B_VE_PRED] = vpx_ve_predictor_4x4;
  pred[B_HE_PRED] = vpx_he_predictor_4x4;
  pred[B_LD_PRED] = vpx_d45e_predictor_4x4;
  pred[B_RD_PRED] = vpx_d135_predictor_4x4;
  pred[B_VR_PRED] = vpx_d117_predictor_4x4;
  pred[B_VL_PRED] = vpx_d63e_predictor_4x4;
  pred[B_HD_PRED] = vpx_d153_predictor_4x4;
  pred[B_HU_PRED] = vpx_d207_predictor_4x4;
}

void vp8_intra4x4_predict(unsigned char *above, unsigned char *yleft,
                          int left_stride, B_PREDICTION_MODE b_mode,
                          unsigned char *dst, int dst_stride,
                          unsigned char top_left) {
/* Power PC implementation uses "vec_vsx_ld" to read 16 bytes from
   Above (aka, Aboveb + 4). Play it safe by reserving enough stack
   space here. Similary for "Left". */
#if HAVE_VSX
  unsigned char Aboveb[20];
#else
  unsigned char Aboveb[12];
#endif
  unsigned char *Above = Aboveb + 4;
#if HAVE_NEON
  // Neon intrinsics are unable to load 32 bits, or 4 8 bit values. Instead, it
  // over reads but does not use the extra 4 values.
  unsigned char Left[8];
#if VPX_WITH_ASAN
  // Silence an 'uninitialized read' warning. Although uninitialized values are
  // indeed read, they are not used.
  vp8_zero_array(Left, 8);
#endif  // VPX_WITH_ASAN
#elif HAVE_VSX
  unsigned char Left[16];
#else
  unsigned char Left[4];
#endif  // HAVE_NEON

  Left[0] = yleft[0];
  Left[1] = yleft[left_stride];
  Left[2] = yleft[2 * left_stride];
  Left[3] = yleft[3 * left_stride];
  memcpy(Above, above, 8);
  Above[-1] = top_left;

  pred[b_mode](dst, dst_stride, Above, Left);
}
