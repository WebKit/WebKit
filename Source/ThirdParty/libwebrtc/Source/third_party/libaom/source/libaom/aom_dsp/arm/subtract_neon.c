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

#include <arm_neon.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_ports/mem.h"

void aom_subtract_block_neon(int rows, int cols, int16_t *diff,
                             ptrdiff_t diff_stride, const uint8_t *src,
                             ptrdiff_t src_stride, const uint8_t *pred,
                             ptrdiff_t pred_stride) {
  if (cols > 16) {
    int r = rows;
    do {
      int c = 0;
      do {
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
        c += 32;
      } while (c < cols);
      diff += diff_stride;
      pred += pred_stride;
      src += src_stride;
    } while (--r != 0);
  } else if (cols > 8) {
    int r = rows;
    do {
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
    } while (--r != 0);
  } else if (cols > 4) {
    int r = rows;
    do {
      const uint8x8_t v_src = vld1_u8(&src[0]);
      const uint8x8_t v_pred = vld1_u8(&pred[0]);
      const uint16x8_t v_diff = vsubl_u8(v_src, v_pred);
      vst1q_s16(&diff[0], vreinterpretq_s16_u16(v_diff));
      diff += diff_stride;
      pred += pred_stride;
      src += src_stride;
    } while (--r != 0);
  } else {
    int r = rows;
    do {
      int c = 0;
      do {
        diff[c] = src[c] - pred[c];
      } while (++c < cols);
      diff += diff_stride;
      pred += pred_stride;
      src += src_stride;
    } while (--r != 0);
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
void aom_highbd_subtract_block_neon(int rows, int cols, int16_t *diff,
                                    ptrdiff_t diff_stride, const uint8_t *src8,
                                    ptrdiff_t src_stride, const uint8_t *pred8,
                                    ptrdiff_t pred_stride) {
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);

  if (cols > 16) {
    int r = rows;
    do {
      int c = 0;
      do {
        const uint16x8_t v_src_00 = vld1q_u16(&src[c + 0]);
        const uint16x8_t v_pred_00 = vld1q_u16(&pred[c + 0]);
        const uint16x8_t v_diff_00 = vsubq_u16(v_src_00, v_pred_00);
        const uint16x8_t v_src_08 = vld1q_u16(&src[c + 8]);
        const uint16x8_t v_pred_08 = vld1q_u16(&pred[c + 8]);
        const uint16x8_t v_diff_08 = vsubq_u16(v_src_08, v_pred_08);
        vst1q_s16(&diff[c + 0], vreinterpretq_s16_u16(v_diff_00));
        vst1q_s16(&diff[c + 8], vreinterpretq_s16_u16(v_diff_08));
        c += 16;
      } while (c < cols);
      diff += diff_stride;
      pred += pred_stride;
      src += src_stride;
    } while (--r != 0);
  } else if (cols > 8) {
    int r = rows;
    do {
      const uint16x8_t v_src_00 = vld1q_u16(&src[0]);
      const uint16x8_t v_pred_00 = vld1q_u16(&pred[0]);
      const uint16x8_t v_diff_00 = vsubq_u16(v_src_00, v_pred_00);
      const uint16x8_t v_src_08 = vld1q_u16(&src[8]);
      const uint16x8_t v_pred_08 = vld1q_u16(&pred[8]);
      const uint16x8_t v_diff_08 = vsubq_u16(v_src_08, v_pred_08);
      vst1q_s16(&diff[0], vreinterpretq_s16_u16(v_diff_00));
      vst1q_s16(&diff[8], vreinterpretq_s16_u16(v_diff_08));
      diff += diff_stride;
      pred += pred_stride;
      src += src_stride;
    } while (--r != 0);
  } else if (cols > 4) {
    int r = rows;
    do {
      const uint16x8_t v_src_r0 = vld1q_u16(&src[0]);
      const uint16x8_t v_src_r1 = vld1q_u16(&src[src_stride]);
      const uint16x8_t v_pred_r0 = vld1q_u16(&pred[0]);
      const uint16x8_t v_pred_r1 = vld1q_u16(&pred[pred_stride]);
      const uint16x8_t v_diff_r0 = vsubq_u16(v_src_r0, v_pred_r0);
      const uint16x8_t v_diff_r1 = vsubq_u16(v_src_r1, v_pred_r1);
      vst1q_s16(&diff[0], vreinterpretq_s16_u16(v_diff_r0));
      vst1q_s16(&diff[diff_stride], vreinterpretq_s16_u16(v_diff_r1));
      diff += diff_stride << 1;
      pred += pred_stride << 1;
      src += src_stride << 1;
      r -= 2;
    } while (r != 0);
  } else {
    int r = rows;
    do {
      const uint16x4_t v_src_r0 = vld1_u16(&src[0]);
      const uint16x4_t v_src_r1 = vld1_u16(&src[src_stride]);
      const uint16x4_t v_pred_r0 = vld1_u16(&pred[0]);
      const uint16x4_t v_pred_r1 = vld1_u16(&pred[pred_stride]);
      const uint16x4_t v_diff_r0 = vsub_u16(v_src_r0, v_pred_r0);
      const uint16x4_t v_diff_r1 = vsub_u16(v_src_r1, v_pred_r1);
      vst1_s16(&diff[0], vreinterpret_s16_u16(v_diff_r0));
      vst1_s16(&diff[diff_stride], vreinterpret_s16_u16(v_diff_r1));
      diff += diff_stride << 1;
      pred += pred_stride << 1;
      src += src_stride << 1;
      r -= 2;
    } while (r != 0);
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH
