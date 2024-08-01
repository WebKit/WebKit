/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>
#include <assert.h>
#include <stdint.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "av1/common/resize.h"

static INLINE uint8x8_t convolve8_4(const int16x4_t s0, const int16x4_t s1,
                                    const int16x4_t s2, const int16x4_t s3,
                                    const int16x4_t s4, const int16x4_t s5,
                                    const int16x4_t s6, const int16x4_t s7,
                                    const int16x8_t filter) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);

  int16x4_t sum = vmul_lane_s16(s0, filter_lo, 0);
  sum = vmla_lane_s16(sum, s1, filter_lo, 1);
  sum = vmla_lane_s16(sum, s2, filter_lo, 2);
  sum = vmla_lane_s16(sum, s5, filter_hi, 1);
  sum = vmla_lane_s16(sum, s6, filter_hi, 2);
  sum = vmla_lane_s16(sum, s7, filter_hi, 3);
  sum = vqadd_s16(sum, vmul_lane_s16(s3, filter_lo, 3));
  sum = vqadd_s16(sum, vmul_lane_s16(s4, filter_hi, 0));

  return vqrshrun_n_s16(vcombine_s16(sum, vdup_n_s16(0)), FILTER_BITS);
}

static INLINE uint8x8_t convolve8_8(const int16x8_t s0, const int16x8_t s1,
                                    const int16x8_t s2, const int16x8_t s3,
                                    const int16x8_t s4, const int16x8_t s5,
                                    const int16x8_t s6, const int16x8_t s7,
                                    const int16x8_t filter) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);

  int16x8_t sum = vmulq_lane_s16(s0, filter_lo, 0);
  sum = vmlaq_lane_s16(sum, s1, filter_lo, 1);
  sum = vmlaq_lane_s16(sum, s2, filter_lo, 2);
  sum = vmlaq_lane_s16(sum, s5, filter_hi, 1);
  sum = vmlaq_lane_s16(sum, s6, filter_hi, 2);
  sum = vmlaq_lane_s16(sum, s7, filter_hi, 3);
  sum = vqaddq_s16(sum, vmulq_lane_s16(s3, filter_lo, 3));
  sum = vqaddq_s16(sum, vmulq_lane_s16(s4, filter_hi, 0));

  return vqrshrun_n_s16(sum, FILTER_BITS);
}

void av1_convolve_horiz_rs_neon(const uint8_t *src, int src_stride,
                                uint8_t *dst, int dst_stride, int w, int h,
                                const int16_t *x_filter, int x0_qn,
                                int x_step_qn) {
  if ((w == 4 && h % 4 != 0) || (w % 8 == 0 && h % 8 != 0) || w % 8 != 0) {
    av1_convolve_horiz_rs_c(src, src_stride, dst, dst_stride, w, h, x_filter,
                            x0_qn, x_step_qn);
    return;
  }

  DECLARE_ALIGNED(16, uint8_t, temp[8 * 8]);

  src -= UPSCALE_NORMATIVE_TAPS / 2 - 1;

  if (w == 4) {
    do {
      int x_qn = x0_qn;

      // Process a 4x4 tile.
      for (int r = 0; r < 4; ++r) {
        const uint8_t *const s = &src[x_qn >> RS_SCALE_SUBPEL_BITS];

        const ptrdiff_t filter_offset =
            UPSCALE_NORMATIVE_TAPS *
            ((x_qn & RS_SCALE_SUBPEL_MASK) >> RS_SCALE_EXTRA_BITS);
        const int16x8_t filter = vld1q_s16(x_filter + filter_offset);

        uint8x8_t t0, t1, t2, t3;
        load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);

        transpose_elems_inplace_u8_8x4(&t0, &t1, &t2, &t3);

        int16x4_t s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
        int16x4_t s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
        int16x4_t s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
        int16x4_t s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
        int16x4_t s4 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
        int16x4_t s5 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
        int16x4_t s6 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
        int16x4_t s7 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

        uint8x8_t d0 = convolve8_4(s0, s1, s2, s3, s4, s5, s6, s7, filter);

        store_u8_4x1(&temp[r * 4], d0);

        x_qn += x_step_qn;
      }

      // Transpose the 4x4 result tile and store.
      uint8x8_t d01 = vld1_u8(temp + 0);
      uint8x8_t d23 = vld1_u8(temp + 8);

      transpose_elems_inplace_u8_4x4(&d01, &d23);

      store_u8x4_strided_x2(dst + 0 * dst_stride, 2 * dst_stride, d01);
      store_u8x4_strided_x2(dst + 1 * dst_stride, 2 * dst_stride, d23);

      dst += 4 * dst_stride;
      src += 4 * src_stride;
      h -= 4;
    } while (h > 0);
  } else {
    do {
      int x_qn = x0_qn;
      uint8_t *d = dst;
      int width = w;

      do {
        // Process an 8x8 tile.
        for (int r = 0; r < 8; ++r) {
          const uint8_t *const s = &src[x_qn >> RS_SCALE_SUBPEL_BITS];

          const ptrdiff_t filter_offset =
              UPSCALE_NORMATIVE_TAPS *
              ((x_qn & RS_SCALE_SUBPEL_MASK) >> RS_SCALE_EXTRA_BITS);
          const int16x8_t filter = vld1q_s16(x_filter + filter_offset);

          uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7;
          load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

          transpose_elems_u8_8x8(t0, t1, t2, t3, t4, t5, t6, t7, &t0, &t1, &t2,
                                 &t3, &t4, &t5, &t6, &t7);

          int16x8_t s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
          int16x8_t s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
          int16x8_t s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
          int16x8_t s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
          int16x8_t s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
          int16x8_t s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
          int16x8_t s6 = vreinterpretq_s16_u16(vmovl_u8(t6));
          int16x8_t s7 = vreinterpretq_s16_u16(vmovl_u8(t7));

          uint8x8_t d0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filter);

          vst1_u8(&temp[r * 8], d0);

          x_qn += x_step_qn;
        }

        // Transpose the 8x8 result tile and store.
        uint8x8_t d0, d1, d2, d3, d4, d5, d6, d7;
        load_u8_8x8(temp, 8, &d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7);

        transpose_elems_inplace_u8_8x8(&d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7);

        store_u8_8x8(d, dst_stride, d0, d1, d2, d3, d4, d5, d6, d7);

        d += 8;
        width -= 8;
      } while (width != 0);

      dst += 8 * dst_stride;
      src += 8 * src_stride;
      h -= 8;

    } while (h > 0);
  }
}
