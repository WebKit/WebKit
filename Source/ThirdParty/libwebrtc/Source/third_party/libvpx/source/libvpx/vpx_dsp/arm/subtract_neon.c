/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>

#include "./vpx_config.h"
#include "vpx/vpx_integer.h"

void vpx_subtract_block_neon(int rows, int cols, int16_t *diff,
                             ptrdiff_t diff_stride, const uint8_t *src,
                             ptrdiff_t src_stride, const uint8_t *pred,
                             ptrdiff_t pred_stride) {
  int r, c;

  if (cols > 16) {
    for (r = 0; r < rows; ++r) {
      for (c = 0; c < cols; c += 32) {
        const uint8x16_t v_src_00 = vld1q_u8(&src[c + 0]);
        const uint8x16_t v_src_16 = vld1q_u8(&src[c + 16]);
        const uint8x16_t v_pred_00 = vld1q_u8(&pred[c + 0]);
        const uint8x16_t v_pred_16 = vld1q_u8(&pred[c + 16]);
        const uint16x8_t v_diff_lo_00 =
            vsubl_u8(vget_low_u8(v_src_00), vget_low_u8(v_pred_00));
        const uint16x8_t v_diff_hi_00 =
            vsubl_u8(vget_high_u8(v_src_00), vget_high_u8(v_pred_00));
        const uint16x8_t v_diff_lo_16 =
            vsubl_u8(vget_low_u8(v_src_16), vget_low_u8(v_pred_16));
        const uint16x8_t v_diff_hi_16 =
            vsubl_u8(vget_high_u8(v_src_16), vget_high_u8(v_pred_16));
        vst1q_s16(&diff[c + 0], vreinterpretq_s16_u16(v_diff_lo_00));
        vst1q_s16(&diff[c + 8], vreinterpretq_s16_u16(v_diff_hi_00));
        vst1q_s16(&diff[c + 16], vreinterpretq_s16_u16(v_diff_lo_16));
        vst1q_s16(&diff[c + 24], vreinterpretq_s16_u16(v_diff_hi_16));
      }
      diff += diff_stride;
      pred += pred_stride;
      src += src_stride;
    }
  } else if (cols > 8) {
    for (r = 0; r < rows; ++r) {
      const uint8x16_t v_src = vld1q_u8(&src[0]);
      const uint8x16_t v_pred = vld1q_u8(&pred[0]);
      const uint16x8_t v_diff_lo =
          vsubl_u8(vget_low_u8(v_src), vget_low_u8(v_pred));
      const uint16x8_t v_diff_hi =
          vsubl_u8(vget_high_u8(v_src), vget_high_u8(v_pred));
      vst1q_s16(&diff[0], vreinterpretq_s16_u16(v_diff_lo));
      vst1q_s16(&diff[8], vreinterpretq_s16_u16(v_diff_hi));
      diff += diff_stride;
      pred += pred_stride;
      src += src_stride;
    }
  } else if (cols > 4) {
    for (r = 0; r < rows; ++r) {
      const uint8x8_t v_src = vld1_u8(&src[0]);
      const uint8x8_t v_pred = vld1_u8(&pred[0]);
      const uint16x8_t v_diff = vsubl_u8(v_src, v_pred);
      vst1q_s16(&diff[0], vreinterpretq_s16_u16(v_diff));
      diff += diff_stride;
      pred += pred_stride;
      src += src_stride;
    }
  } else {
    for (r = 0; r < rows; ++r) {
      for (c = 0; c < cols; ++c) diff[c] = src[c] - pred[c];

      diff += diff_stride;
      pred += pred_stride;
      src += src_stride;
    }
  }
}
