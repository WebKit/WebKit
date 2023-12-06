/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <arm_neon.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_ports/mem.h"
#include "av1/common/convolve.h"
#include "av1/common/filter.h"
#include "av1/common/arm/highbd_convolve_neon.h"

#define ROUND_SHIFT 2 * FILTER_BITS - ROUND0_BITS - COMPOUND_ROUND1_BITS

static INLINE void highbd_12_comp_avg_neon(const uint16_t *src_ptr,
                                           int src_stride, uint16_t *dst_ptr,
                                           int dst_stride, int w, int h,
                                           ConvolveParams *conv_params,
                                           const int offset, const int bd) {
  CONV_BUF_TYPE *ref_ptr = conv_params->dst;
  const int ref_stride = conv_params->dst_stride;
  const uint16x4_t offset_vec = vdup_n_u16(offset);
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

  if (w == 4) {
    do {
      const uint16x4_t src = vld1_u16(src_ptr);
      const uint16x4_t ref = vld1_u16(ref_ptr);

      uint16x4_t avg = vhadd_u16(src, ref);
      int32x4_t d0 = vreinterpretq_s32_u32(vsubl_u16(avg, offset_vec));

      uint16x4_t d0_u16 = vqrshrun_n_s32(d0, ROUND_SHIFT - 2);
      d0_u16 = vmin_u16(d0_u16, vget_low_u16(max));

      vst1_u16(dst_ptr, d0_u16);

      src_ptr += src_stride;
      ref_ptr += ref_stride;
      dst_ptr += dst_stride;
    } while (--h != 0);
  } else {
    do {
      int width = w;
      const uint16_t *src = src_ptr;
      const uint16_t *ref = ref_ptr;
      uint16_t *dst = dst_ptr;
      do {
        const uint16x8_t s = vld1q_u16(src);
        const uint16x8_t r = vld1q_u16(ref);

        uint16x8_t avg = vhaddq_u16(s, r);
        int32x4_t d0_lo =
            vreinterpretq_s32_u32(vsubl_u16(vget_low_u16(avg), offset_vec));
        int32x4_t d0_hi =
            vreinterpretq_s32_u32(vsubl_u16(vget_high_u16(avg), offset_vec));

        uint16x8_t d0 = vcombine_u16(vqrshrun_n_s32(d0_lo, ROUND_SHIFT - 2),
                                     vqrshrun_n_s32(d0_hi, ROUND_SHIFT - 2));
        d0 = vminq_u16(d0, max);
        vst1q_u16(dst, d0);

        src += 8;
        ref += 8;
        dst += 8;
        width -= 8;
      } while (width != 0);

      src_ptr += src_stride;
      ref_ptr += ref_stride;
      dst_ptr += dst_stride;
    } while (--h != 0);
  }
}

static INLINE void highbd_comp_avg_neon(const uint16_t *src_ptr, int src_stride,
                                        uint16_t *dst_ptr, int dst_stride,
                                        int w, int h,
                                        ConvolveParams *conv_params,
                                        const int offset, const int bd) {
  CONV_BUF_TYPE *ref_ptr = conv_params->dst;
  const int ref_stride = conv_params->dst_stride;
  const uint16x4_t offset_vec = vdup_n_u16(offset);
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

  if (w == 4) {
    do {
      const uint16x4_t src = vld1_u16(src_ptr);
      const uint16x4_t ref = vld1_u16(ref_ptr);

      uint16x4_t avg = vhadd_u16(src, ref);
      int32x4_t d0 = vreinterpretq_s32_u32(vsubl_u16(avg, offset_vec));

      uint16x4_t d0_u16 = vqrshrun_n_s32(d0, ROUND_SHIFT);
      d0_u16 = vmin_u16(d0_u16, vget_low_u16(max));

      vst1_u16(dst_ptr, d0_u16);

      src_ptr += src_stride;
      ref_ptr += ref_stride;
      dst_ptr += dst_stride;
    } while (--h != 0);
  } else {
    do {
      int width = w;
      const uint16_t *src = src_ptr;
      const uint16_t *ref = ref_ptr;
      uint16_t *dst = dst_ptr;
      do {
        const uint16x8_t s = vld1q_u16(src);
        const uint16x8_t r = vld1q_u16(ref);

        uint16x8_t avg = vhaddq_u16(s, r);
        int32x4_t d0_lo =
            vreinterpretq_s32_u32(vsubl_u16(vget_low_u16(avg), offset_vec));
        int32x4_t d0_hi =
            vreinterpretq_s32_u32(vsubl_u16(vget_high_u16(avg), offset_vec));

        uint16x8_t d0 = vcombine_u16(vqrshrun_n_s32(d0_lo, ROUND_SHIFT),
                                     vqrshrun_n_s32(d0_hi, ROUND_SHIFT));
        d0 = vminq_u16(d0, max);
        vst1q_u16(dst, d0);

        src += 8;
        ref += 8;
        dst += 8;
        width -= 8;
      } while (width != 0);

      src_ptr += src_stride;
      ref_ptr += ref_stride;
      dst_ptr += dst_stride;
    } while (--h != 0);
  }
}

static INLINE void highbd_12_dist_wtd_comp_avg_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, ConvolveParams *conv_params, const int offset, const int bd) {
  CONV_BUF_TYPE *ref_ptr = conv_params->dst;
  const int ref_stride = conv_params->dst_stride;
  const uint32x4_t offset_vec = vdupq_n_u32(offset);
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
  uint16x4_t fwd_offset = vdup_n_u16(conv_params->fwd_offset);
  uint16x4_t bck_offset = vdup_n_u16(conv_params->bck_offset);

  // Weighted averaging
  if (w == 4) {
    do {
      const uint16x4_t src = vld1_u16(src_ptr);
      const uint16x4_t ref = vld1_u16(ref_ptr);

      uint32x4_t wtd_avg = vmull_u16(ref, fwd_offset);
      wtd_avg = vmlal_u16(wtd_avg, src, bck_offset);
      wtd_avg = vshrq_n_u32(wtd_avg, DIST_PRECISION_BITS);
      int32x4_t d0 = vreinterpretq_s32_u32(vsubq_u32(wtd_avg, offset_vec));

      uint16x4_t d0_u16 = vqrshrun_n_s32(d0, ROUND_SHIFT - 2);
      d0_u16 = vmin_u16(d0_u16, vget_low_u16(max));

      vst1_u16(dst_ptr, d0_u16);

      src_ptr += src_stride;
      dst_ptr += dst_stride;
      ref_ptr += ref_stride;
    } while (--h != 0);
  } else {
    do {
      int width = w;
      const uint16_t *src = src_ptr;
      const uint16_t *ref = ref_ptr;
      uint16_t *dst = dst_ptr;
      do {
        const uint16x8_t s = vld1q_u16(src);
        const uint16x8_t r = vld1q_u16(ref);

        uint32x4_t wtd_avg0 = vmull_u16(vget_low_u16(r), fwd_offset);
        wtd_avg0 = vmlal_u16(wtd_avg0, vget_low_u16(s), bck_offset);
        wtd_avg0 = vshrq_n_u32(wtd_avg0, DIST_PRECISION_BITS);
        int32x4_t d0 = vreinterpretq_s32_u32(vsubq_u32(wtd_avg0, offset_vec));

        uint32x4_t wtd_avg1 = vmull_u16(vget_high_u16(r), fwd_offset);
        wtd_avg1 = vmlal_u16(wtd_avg1, vget_high_u16(s), bck_offset);
        wtd_avg1 = vshrq_n_u32(wtd_avg1, DIST_PRECISION_BITS);
        int32x4_t d1 = vreinterpretq_s32_u32(vsubq_u32(wtd_avg1, offset_vec));

        uint16x8_t d01 = vcombine_u16(vqrshrun_n_s32(d0, ROUND_SHIFT - 2),
                                      vqrshrun_n_s32(d1, ROUND_SHIFT - 2));
        d01 = vminq_u16(d01, max);
        vst1q_u16(dst, d01);

        src += 8;
        ref += 8;
        dst += 8;
        width -= 8;
      } while (width != 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
      ref_ptr += ref_stride;
    } while (--h != 0);
  }
}

static INLINE void highbd_dist_wtd_comp_avg_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, ConvolveParams *conv_params, const int offset, const int bd) {
  CONV_BUF_TYPE *ref_ptr = conv_params->dst;
  const int ref_stride = conv_params->dst_stride;
  const uint32x4_t offset_vec = vdupq_n_u32(offset);
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
  uint16x4_t fwd_offset = vdup_n_u16(conv_params->fwd_offset);
  uint16x4_t bck_offset = vdup_n_u16(conv_params->bck_offset);

  // Weighted averaging
  if (w == 4) {
    do {
      const uint16x4_t src = vld1_u16(src_ptr);
      const uint16x4_t ref = vld1_u16(ref_ptr);

      uint32x4_t wtd_avg = vmull_u16(ref, fwd_offset);
      wtd_avg = vmlal_u16(wtd_avg, src, bck_offset);
      wtd_avg = vshrq_n_u32(wtd_avg, DIST_PRECISION_BITS);
      int32x4_t d0 = vreinterpretq_s32_u32(vsubq_u32(wtd_avg, offset_vec));

      uint16x4_t d0_u16 = vqrshrun_n_s32(d0, ROUND_SHIFT);
      d0_u16 = vmin_u16(d0_u16, vget_low_u16(max));

      vst1_u16(dst_ptr, d0_u16);

      src_ptr += src_stride;
      dst_ptr += dst_stride;
      ref_ptr += ref_stride;
    } while (--h != 0);
  } else {
    do {
      int width = w;
      const uint16_t *src = src_ptr;
      const uint16_t *ref = ref_ptr;
      uint16_t *dst = dst_ptr;
      do {
        const uint16x8_t s = vld1q_u16(src);
        const uint16x8_t r = vld1q_u16(ref);

        uint32x4_t wtd_avg0 = vmull_u16(vget_low_u16(r), fwd_offset);
        wtd_avg0 = vmlal_u16(wtd_avg0, vget_low_u16(s), bck_offset);
        wtd_avg0 = vshrq_n_u32(wtd_avg0, DIST_PRECISION_BITS);
        int32x4_t d0 = vreinterpretq_s32_u32(vsubq_u32(wtd_avg0, offset_vec));

        uint32x4_t wtd_avg1 = vmull_u16(vget_high_u16(r), fwd_offset);
        wtd_avg1 = vmlal_u16(wtd_avg1, vget_high_u16(s), bck_offset);
        wtd_avg1 = vshrq_n_u32(wtd_avg1, DIST_PRECISION_BITS);
        int32x4_t d1 = vreinterpretq_s32_u32(vsubq_u32(wtd_avg1, offset_vec));

        uint16x8_t d01 = vcombine_u16(vqrshrun_n_s32(d0, ROUND_SHIFT),
                                      vqrshrun_n_s32(d1, ROUND_SHIFT));
        d01 = vminq_u16(d01, max);
        vst1q_u16(dst, d01);

        src += 8;
        ref += 8;
        dst += 8;
        width -= 8;
      } while (width != 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
      ref_ptr += ref_stride;
    } while (--h != 0);
  }
}

static INLINE uint16x4_t highbd_12_convolve6_4(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x8_t filter, const int32x4_t offset) {
  // Values at indices 0 and 7 of y_filter are zero.
  const int16x4_t filter_0_3 = vget_low_s16(filter);
  const int16x4_t filter_4_7 = vget_high_s16(filter);

  int32x4_t sum = vmlal_lane_s16(offset, s0, filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s1, filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s2, filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s3, filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s4, filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s5, filter_4_7, 2);

  return vqshrun_n_s32(sum, ROUND0_BITS + 2);
}

static INLINE uint16x4_t
highbd_convolve6_4(const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
                   const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
                   const int16x8_t filter, const int32x4_t offset) {
  // Values at indices 0 and 7 of y_filter are zero.
  const int16x4_t filter_0_3 = vget_low_s16(filter);
  const int16x4_t filter_4_7 = vget_high_s16(filter);

  int32x4_t sum = vmlal_lane_s16(offset, s0, filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s1, filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s2, filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s3, filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s4, filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s5, filter_4_7, 2);

  return vqshrun_n_s32(sum, ROUND0_BITS);
}

static INLINE uint16x8_t highbd_12_convolve6_8(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t filter, const int32x4_t offset) {
  // Values at indices 0 and 7 of y_filter are zero.
  const int16x4_t filter_0_3 = vget_low_s16(filter);
  const int16x4_t filter_4_7 = vget_high_s16(filter);

  int32x4_t sum0 = vmlal_lane_s16(offset, vget_low_s16(s0), filter_0_3, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), filter_0_3, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), filter_0_3, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), filter_4_7, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), filter_4_7, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), filter_4_7, 2);

  int32x4_t sum1 = vmlal_lane_s16(offset, vget_high_s16(s0), filter_0_3, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), filter_0_3, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), filter_0_3, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), filter_4_7, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), filter_4_7, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), filter_4_7, 2);

  return vcombine_u16(vqshrun_n_s32(sum0, ROUND0_BITS + 2),
                      vqshrun_n_s32(sum1, ROUND0_BITS + 2));
}

static INLINE uint16x8_t
highbd_convolve6_8(const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
                   const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
                   const int16x8_t filter, const int32x4_t offset) {
  // Values at indices 0 and 7 of y_filter are zero.
  const int16x4_t filter_0_3 = vget_low_s16(filter);
  const int16x4_t filter_4_7 = vget_high_s16(filter);

  int32x4_t sum0 = vmlal_lane_s16(offset, vget_low_s16(s0), filter_0_3, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), filter_0_3, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), filter_0_3, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), filter_4_7, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), filter_4_7, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), filter_4_7, 2);

  int32x4_t sum1 = vmlal_lane_s16(offset, vget_high_s16(s0), filter_0_3, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), filter_0_3, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), filter_0_3, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), filter_4_7, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), filter_4_7, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), filter_4_7, 2);

  return vcombine_u16(vqshrun_n_s32(sum0, 3), vqshrun_n_s32(sum1, ROUND0_BITS));
}

static INLINE void highbd_12_dist_wtd_convolve_x_6tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, const int offset) {
  const int32x4_t offset_vec = vdupq_n_s32(offset);

  const int16x8_t x_filter = vld1q_s16(x_filter_ptr);

  int height = h;

  do {
    int width = w;
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      int16x8_t s0[6], s1[6], s2[6], s3[6];
      load_s16_8x6(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                   &s0[4], &s0[5]);
      load_s16_8x6(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                   &s1[4], &s1[5]);
      load_s16_8x6(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                   &s2[4], &s2[5]);
      load_s16_8x6(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                   &s3[4], &s3[5]);

      uint16x8_t d0 = highbd_12_convolve6_8(s0[0], s0[1], s0[2], s0[3], s0[4],
                                            s0[5], x_filter, offset_vec);
      uint16x8_t d1 = highbd_12_convolve6_8(s1[0], s1[1], s1[2], s1[3], s1[4],
                                            s1[5], x_filter, offset_vec);
      uint16x8_t d2 = highbd_12_convolve6_8(s2[0], s2[1], s2[2], s2[3], s2[4],
                                            s2[5], x_filter, offset_vec);
      uint16x8_t d3 = highbd_12_convolve6_8(s3[0], s3[1], s3[2], s3[3], s3[4],
                                            s3[5], x_filter, offset_vec);

      store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

      s += 8;
      d += 8;
      width -= 8;
    } while (width != 0);
    src_ptr += 4 * src_stride;
    dst_ptr += 4 * dst_stride;
    height -= 4;
  } while (height != 0);
}

static INLINE void highbd_dist_wtd_convolve_x_6tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, const int offset) {
  const int32x4_t offset_vec = vdupq_n_s32(offset);

  const int16x8_t x_filter = vld1q_s16(x_filter_ptr);

  int height = h;

  do {
    int width = w;
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      int16x8_t s0[6], s1[6], s2[6], s3[6];
      load_s16_8x6(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                   &s0[4], &s0[5]);
      load_s16_8x6(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                   &s1[4], &s1[5]);
      load_s16_8x6(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                   &s2[4], &s2[5]);
      load_s16_8x6(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                   &s3[4], &s3[5]);

      uint16x8_t d0 = highbd_convolve6_8(s0[0], s0[1], s0[2], s0[3], s0[4],
                                         s0[5], x_filter, offset_vec);
      uint16x8_t d1 = highbd_convolve6_8(s1[0], s1[1], s1[2], s1[3], s1[4],
                                         s1[5], x_filter, offset_vec);
      uint16x8_t d2 = highbd_convolve6_8(s2[0], s2[1], s2[2], s2[3], s2[4],
                                         s2[5], x_filter, offset_vec);
      uint16x8_t d3 = highbd_convolve6_8(s3[0], s3[1], s3[2], s3[3], s3[4],
                                         s3[5], x_filter, offset_vec);

      store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

      s += 8;
      d += 8;
      width -= 8;
    } while (width != 0);
    src_ptr += 4 * src_stride;
    dst_ptr += 4 * dst_stride;
    height -= 4;
  } while (height != 0);
}

static INLINE uint16x4_t highbd_12_convolve8_4(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x8_t filter,
    const int32x4_t offset) {
  const int16x4_t filter_0_3 = vget_low_s16(filter);
  const int16x4_t filter_4_7 = vget_high_s16(filter);

  int32x4_t sum = vmlal_lane_s16(offset, s0, filter_0_3, 0);
  sum = vmlal_lane_s16(sum, s1, filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s2, filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s3, filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s4, filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s5, filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s6, filter_4_7, 2);
  sum = vmlal_lane_s16(sum, s7, filter_4_7, 3);

  return vqshrun_n_s32(sum, ROUND0_BITS + 2);
}

static INLINE uint16x4_t
highbd_convolve8_4(const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
                   const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
                   const int16x4_t s6, const int16x4_t s7,
                   const int16x8_t filter, const int32x4_t offset) {
  const int16x4_t filter_0_3 = vget_low_s16(filter);
  const int16x4_t filter_4_7 = vget_high_s16(filter);

  int32x4_t sum = vmlal_lane_s16(offset, s0, filter_0_3, 0);
  sum = vmlal_lane_s16(sum, s1, filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s2, filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s3, filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s4, filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s5, filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s6, filter_4_7, 2);
  sum = vmlal_lane_s16(sum, s7, filter_4_7, 3);

  return vqshrun_n_s32(sum, ROUND0_BITS);
}

static INLINE uint16x8_t highbd_12_convolve8_8(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16x8_t filter,
    const int32x4_t offset) {
  const int16x4_t filter_0_3 = vget_low_s16(filter);
  const int16x4_t filter_4_7 = vget_high_s16(filter);

  int32x4_t sum0 = vmlal_lane_s16(offset, vget_low_s16(s0), filter_0_3, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), filter_0_3, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), filter_0_3, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), filter_0_3, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), filter_4_7, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), filter_4_7, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s6), filter_4_7, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s7), filter_4_7, 3);

  int32x4_t sum1 = vmlal_lane_s16(offset, vget_high_s16(s0), filter_0_3, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), filter_0_3, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), filter_0_3, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), filter_0_3, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), filter_4_7, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), filter_4_7, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s6), filter_4_7, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s7), filter_4_7, 3);

  return vcombine_u16(vqshrun_n_s32(sum0, ROUND0_BITS + 2),
                      vqshrun_n_s32(sum1, ROUND0_BITS + 2));
}

static INLINE uint16x8_t
highbd_convolve8_8(const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
                   const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
                   const int16x8_t s6, const int16x8_t s7,
                   const int16x8_t filter, const int32x4_t offset) {
  const int16x4_t filter_0_3 = vget_low_s16(filter);
  const int16x4_t filter_4_7 = vget_high_s16(filter);

  int32x4_t sum0 = vmlal_lane_s16(offset, vget_low_s16(s0), filter_0_3, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), filter_0_3, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), filter_0_3, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), filter_0_3, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), filter_4_7, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), filter_4_7, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s6), filter_4_7, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s7), filter_4_7, 3);

  int32x4_t sum1 = vmlal_lane_s16(offset, vget_high_s16(s0), filter_0_3, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), filter_0_3, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), filter_0_3, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), filter_0_3, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), filter_4_7, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), filter_4_7, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s6), filter_4_7, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s7), filter_4_7, 3);

  return vcombine_u16(vqshrun_n_s32(sum0, ROUND0_BITS),
                      vqshrun_n_s32(sum1, ROUND0_BITS));
}

static INLINE uint16x4_t highbd_12_convolve4_4_x(const int16x4_t s[4],
                                                 const int16x4_t x_filter,
                                                 const int32x4_t offset) {
  int32x4_t sum = vmlal_lane_s16(offset, s[0], x_filter, 0);
  sum = vmlal_lane_s16(sum, s[1], x_filter, 1);
  sum = vmlal_lane_s16(sum, s[2], x_filter, 2);
  sum = vmlal_lane_s16(sum, s[3], x_filter, 3);

  return vqshrun_n_s32(sum, 5);
}

static INLINE uint16x4_t highbd_convolve4_4_x(const int16x4_t s[4],
                                              const int16x4_t x_filter,
                                              const int32x4_t offset) {
  int32x4_t sum = vmlal_lane_s16(offset, s[0], x_filter, 0);
  sum = vmlal_lane_s16(sum, s[1], x_filter, 1);
  sum = vmlal_lane_s16(sum, s[2], x_filter, 2);
  sum = vmlal_lane_s16(sum, s[3], x_filter, 3);

  return vqshrun_n_s32(sum, ROUND0_BITS);
}

static INLINE void highbd_12_dist_wtd_convolve_x_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, const int offset) {
  const int32x4_t offset_vec = vdupq_n_s32(offset);

  if (w == 4) {
    // 4-tap filters are used for blocks having width == 4.
    const int16x4_t x_filter = vld1_s16(x_filter_ptr + 2);
    const int16_t *s = (const int16_t *)(src_ptr + 2);
    uint16_t *d = dst_ptr;

    do {
      int16x4_t s0[4], s1[4], s2[4], s3[4];
      load_s16_4x4(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3]);
      load_s16_4x4(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3]);
      load_s16_4x4(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3]);
      load_s16_4x4(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3]);

      uint16x4_t d0 = highbd_12_convolve4_4_x(s0, x_filter, offset_vec);
      uint16x4_t d1 = highbd_12_convolve4_4_x(s1, x_filter, offset_vec);
      uint16x4_t d2 = highbd_12_convolve4_4_x(s2, x_filter, offset_vec);
      uint16x4_t d3 = highbd_12_convolve4_4_x(s3, x_filter, offset_vec);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    const int16x8_t x_filter = vld1q_s16(x_filter_ptr);
    int height = h;

    do {
      int width = w;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      do {
        int16x8_t s0[8], s1[8], s2[8], s3[8];
        load_s16_8x8(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                     &s0[4], &s0[5], &s0[6], &s0[7]);
        load_s16_8x8(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                     &s1[4], &s1[5], &s1[6], &s1[7]);
        load_s16_8x8(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                     &s2[4], &s2[5], &s2[6], &s2[7]);
        load_s16_8x8(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                     &s3[4], &s3[5], &s3[6], &s3[7]);

        uint16x8_t d0 =
            highbd_12_convolve8_8(s0[0], s0[1], s0[2], s0[3], s0[4], s0[5],
                                  s0[6], s0[7], x_filter, offset_vec);
        uint16x8_t d1 =
            highbd_12_convolve8_8(s1[0], s1[1], s1[2], s1[3], s1[4], s1[5],
                                  s1[6], s1[7], x_filter, offset_vec);
        uint16x8_t d2 =
            highbd_12_convolve8_8(s2[0], s2[1], s2[2], s2[3], s2[4], s2[5],
                                  s2[6], s2[7], x_filter, offset_vec);
        uint16x8_t d3 =
            highbd_12_convolve8_8(s3[0], s3[1], s3[2], s3[3], s3[4], s3[5],
                                  s3[6], s3[7], x_filter, offset_vec);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height != 0);
  }
}

static INLINE void highbd_dist_wtd_convolve_x_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, const int offset) {
  const int32x4_t offset_vec = vdupq_n_s32(offset);

  if (w == 4) {
    // 4-tap filters are used for blocks having width == 4.
    const int16x4_t x_filter = vld1_s16(x_filter_ptr + 2);
    const int16_t *s = (const int16_t *)(src_ptr + 2);
    uint16_t *d = dst_ptr;

    do {
      int16x4_t s0[4], s1[4], s2[4], s3[4];
      load_s16_4x4(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3]);
      load_s16_4x4(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3]);
      load_s16_4x4(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3]);
      load_s16_4x4(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3]);

      uint16x4_t d0 = highbd_convolve4_4_x(s0, x_filter, offset_vec);
      uint16x4_t d1 = highbd_convolve4_4_x(s1, x_filter, offset_vec);
      uint16x4_t d2 = highbd_convolve4_4_x(s2, x_filter, offset_vec);
      uint16x4_t d3 = highbd_convolve4_4_x(s3, x_filter, offset_vec);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    const int16x8_t x_filter = vld1q_s16(x_filter_ptr);
    int height = h;

    do {
      int width = w;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      do {
        int16x8_t s0[8], s1[8], s2[8], s3[8];
        load_s16_8x8(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                     &s0[4], &s0[5], &s0[6], &s0[7]);
        load_s16_8x8(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                     &s1[4], &s1[5], &s1[6], &s1[7]);
        load_s16_8x8(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                     &s2[4], &s2[5], &s2[6], &s2[7]);
        load_s16_8x8(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                     &s3[4], &s3[5], &s3[6], &s3[7]);

        uint16x8_t d0 =
            highbd_convolve8_8(s0[0], s0[1], s0[2], s0[3], s0[4], s0[5], s0[6],
                               s0[7], x_filter, offset_vec);
        uint16x8_t d1 =
            highbd_convolve8_8(s1[0], s1[1], s1[2], s1[3], s1[4], s1[5], s1[6],
                               s1[7], x_filter, offset_vec);
        uint16x8_t d2 =
            highbd_convolve8_8(s2[0], s2[1], s2[2], s2[3], s2[4], s2[5], s2[6],
                               s2[7], x_filter, offset_vec);
        uint16x8_t d3 =
            highbd_convolve8_8(s3[0], s3[1], s3[2], s3[3], s3[4], s3[5], s3[6],
                               s3[7], x_filter, offset_vec);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height != 0);
  }
}

void av1_highbd_dist_wtd_convolve_x_neon(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn,
    ConvolveParams *conv_params, int bd) {
  DECLARE_ALIGNED(16, uint16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE]);
  CONV_BUF_TYPE *dst16 = conv_params->dst;
  const int x_filter_taps = get_filter_tap(filter_params_x, subpel_x_qn);
  int dst16_stride = conv_params->dst_stride;
  const int im_stride = MAX_SB_SIZE;
  const int horiz_offset = filter_params_x->taps / 2 - 1;
  assert(FILTER_BITS == COMPOUND_ROUND1_BITS);
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int offset_avg = (1 << (offset_bits - conv_params->round_1)) +
                         (1 << (offset_bits - conv_params->round_1 - 1));
  const int offset_convolve = (1 << (conv_params->round_0 - 1)) +
                              (1 << (bd + FILTER_BITS)) +
                              (1 << (bd + FILTER_BITS - 1));

  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);

  src -= horiz_offset;

  // horizontal filter
  if (bd == 12) {
    if (conv_params->do_average) {
      if (x_filter_taps <= 6 && w != 4) {
        highbd_12_dist_wtd_convolve_x_6tap_neon(src + 1, src_stride, im_block,
                                                im_stride, w, h, x_filter_ptr,
                                                offset_convolve);
      } else {
        highbd_12_dist_wtd_convolve_x_neon(src, src_stride, im_block, im_stride,
                                           w, h, x_filter_ptr, offset_convolve);
      }
      if (conv_params->use_dist_wtd_comp_avg) {
        highbd_12_dist_wtd_comp_avg_neon(im_block, im_stride, dst, dst_stride,
                                         w, h, conv_params, offset_avg, bd);
      } else {
        highbd_12_comp_avg_neon(im_block, im_stride, dst, dst_stride, w, h,
                                conv_params, offset_avg, bd);
      }
    } else {
      if (x_filter_taps <= 6 && w != 4) {
        highbd_12_dist_wtd_convolve_x_6tap_neon(src + 1, src_stride, dst16,
                                                dst16_stride, w, h,
                                                x_filter_ptr, offset_convolve);
      } else {
        highbd_12_dist_wtd_convolve_x_neon(src, src_stride, dst16, dst16_stride,
                                           w, h, x_filter_ptr, offset_convolve);
      }
    }
  } else {
    if (conv_params->do_average) {
      if (x_filter_taps <= 6 && w != 4) {
        highbd_dist_wtd_convolve_x_6tap_neon(src + 1, src_stride, im_block,
                                             im_stride, w, h, x_filter_ptr,
                                             offset_convolve);
      } else {
        highbd_dist_wtd_convolve_x_neon(src, src_stride, im_block, im_stride, w,
                                        h, x_filter_ptr, offset_convolve);
      }
      if (conv_params->use_dist_wtd_comp_avg) {
        highbd_dist_wtd_comp_avg_neon(im_block, im_stride, dst, dst_stride, w,
                                      h, conv_params, offset_avg, bd);
      } else {
        highbd_comp_avg_neon(im_block, im_stride, dst, dst_stride, w, h,
                             conv_params, offset_avg, bd);
      }
    } else {
      if (x_filter_taps <= 6 && w != 4) {
        highbd_dist_wtd_convolve_x_6tap_neon(src + 1, src_stride, dst16,
                                             dst16_stride, w, h, x_filter_ptr,
                                             offset_convolve);
      } else {
        highbd_dist_wtd_convolve_x_neon(src, src_stride, dst16, dst16_stride, w,
                                        h, x_filter_ptr, offset_convolve);
      }
    }
  }
}

static INLINE void highbd_12_dist_wtd_convolve_y_6tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, const int offset) {
  const int16x8_t y_filter = vld1q_s16(y_filter_ptr);
  const int32x4_t offset_vec = vdupq_n_s32(offset);

  if (w == 4) {
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    int16x4_t s0, s1, s2, s3, s4;
    load_s16_4x5(s, src_stride, &s0, &s1, &s2, &s3, &s4);
    s += 5 * src_stride;

    do {
      int16x4_t s5, s6, s7, s8;
      load_s16_4x4(s, src_stride, &s5, &s6, &s7, &s8);

      uint16x4_t d0 =
          highbd_12_convolve6_4(s0, s1, s2, s3, s4, s5, y_filter, offset_vec);
      uint16x4_t d1 =
          highbd_12_convolve6_4(s1, s2, s3, s4, s5, s6, y_filter, offset_vec);
      uint16x4_t d2 =
          highbd_12_convolve6_4(s2, s3, s4, s5, s6, s7, y_filter, offset_vec);
      uint16x4_t d3 =
          highbd_12_convolve6_4(s3, s4, s5, s6, s7, s8, y_filter, offset_vec);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      int16x8_t s0, s1, s2, s3, s4;
      load_s16_8x5(s, src_stride, &s0, &s1, &s2, &s3, &s4);
      s += 5 * src_stride;

      do {
        int16x8_t s5, s6, s7, s8;
        load_s16_8x4(s, src_stride, &s5, &s6, &s7, &s8);

        uint16x8_t d0 =
            highbd_12_convolve6_8(s0, s1, s2, s3, s4, s5, y_filter, offset_vec);
        uint16x8_t d1 =
            highbd_12_convolve6_8(s1, s2, s3, s4, s5, s6, y_filter, offset_vec);
        uint16x8_t d2 =
            highbd_12_convolve6_8(s2, s3, s4, s5, s6, s7, y_filter, offset_vec);
        uint16x8_t d3 =
            highbd_12_convolve6_8(s3, s4, s5, s6, s7, s8, y_filter, offset_vec);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height != 0);
      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w != 0);
  }
}

static INLINE void highbd_dist_wtd_convolve_y_6tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, const int offset) {
  const int16x8_t y_filter = vld1q_s16(y_filter_ptr);
  const int32x4_t offset_vec = vdupq_n_s32(offset);

  if (w == 4) {
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    int16x4_t s0, s1, s2, s3, s4;
    load_s16_4x5(s, src_stride, &s0, &s1, &s2, &s3, &s4);
    s += 5 * src_stride;

    do {
      int16x4_t s5, s6, s7, s8;
      load_s16_4x4(s, src_stride, &s5, &s6, &s7, &s8);

      uint16x4_t d0 =
          highbd_convolve6_4(s0, s1, s2, s3, s4, s5, y_filter, offset_vec);
      uint16x4_t d1 =
          highbd_convolve6_4(s1, s2, s3, s4, s5, s6, y_filter, offset_vec);
      uint16x4_t d2 =
          highbd_convolve6_4(s2, s3, s4, s5, s6, s7, y_filter, offset_vec);
      uint16x4_t d3 =
          highbd_convolve6_4(s3, s4, s5, s6, s7, s8, y_filter, offset_vec);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      int16x8_t s0, s1, s2, s3, s4;
      load_s16_8x5(s, src_stride, &s0, &s1, &s2, &s3, &s4);
      s += 5 * src_stride;

      do {
        int16x8_t s5, s6, s7, s8;
        load_s16_8x4(s, src_stride, &s5, &s6, &s7, &s8);

        uint16x8_t d0 =
            highbd_convolve6_8(s0, s1, s2, s3, s4, s5, y_filter, offset_vec);
        uint16x8_t d1 =
            highbd_convolve6_8(s1, s2, s3, s4, s5, s6, y_filter, offset_vec);
        uint16x8_t d2 =
            highbd_convolve6_8(s2, s3, s4, s5, s6, s7, y_filter, offset_vec);
        uint16x8_t d3 =
            highbd_convolve6_8(s3, s4, s5, s6, s7, s8, y_filter, offset_vec);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height != 0);
      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w != 0);
  }
}

static INLINE void highbd_12_dist_wtd_convolve_y_8tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, const int offset) {
  const int16x8_t y_filter = vld1q_s16(y_filter_ptr);
  const int32x4_t offset_vec = vdupq_n_s32(offset);

  if (w == 4) {
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    int16x4_t s0, s1, s2, s3, s4, s5, s6;
    load_s16_4x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    s += 7 * src_stride;

    do {
      int16x4_t s7, s8, s9, s10;
      load_s16_4x4(s, src_stride, &s7, &s8, &s9, &s10);

      uint16x4_t d0 = highbd_12_convolve8_4(s0, s1, s2, s3, s4, s5, s6, s7,
                                            y_filter, offset_vec);
      uint16x4_t d1 = highbd_12_convolve8_4(s1, s2, s3, s4, s5, s6, s7, s8,
                                            y_filter, offset_vec);
      uint16x4_t d2 = highbd_12_convolve8_4(s2, s3, s4, s5, s6, s7, s8, s9,
                                            y_filter, offset_vec);
      uint16x4_t d3 = highbd_12_convolve8_4(s3, s4, s5, s6, s7, s8, s9, s10,
                                            y_filter, offset_vec);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      int16x8_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      do {
        int16x8_t s7, s8, s9, s10;
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10);

        uint16x8_t d0 = highbd_12_convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7,
                                              y_filter, offset_vec);
        uint16x8_t d1 = highbd_12_convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8,
                                              y_filter, offset_vec);
        uint16x8_t d2 = highbd_12_convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9,
                                              y_filter, offset_vec);
        uint16x8_t d3 = highbd_12_convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10,
                                              y_filter, offset_vec);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height != 0);
      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w != 0);
  }
}
static INLINE void highbd_dist_wtd_convolve_y_8tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, const int offset) {
  const int16x8_t y_filter = vld1q_s16(y_filter_ptr);
  const int32x4_t offset_vec = vdupq_n_s32(offset);

  if (w == 4) {
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    int16x4_t s0, s1, s2, s3, s4, s5, s6;
    load_s16_4x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    s += 7 * src_stride;

    do {
      int16x4_t s7, s8, s9, s10;
      load_s16_4x4(s, src_stride, &s7, &s8, &s9, &s10);

      uint16x4_t d0 = highbd_convolve8_4(s0, s1, s2, s3, s4, s5, s6, s7,
                                         y_filter, offset_vec);
      uint16x4_t d1 = highbd_convolve8_4(s1, s2, s3, s4, s5, s6, s7, s8,
                                         y_filter, offset_vec);
      uint16x4_t d2 = highbd_convolve8_4(s2, s3, s4, s5, s6, s7, s8, s9,
                                         y_filter, offset_vec);
      uint16x4_t d3 = highbd_convolve8_4(s3, s4, s5, s6, s7, s8, s9, s10,
                                         y_filter, offset_vec);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      int16x8_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      do {
        int16x8_t s7, s8, s9, s10;
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10);

        uint16x8_t d0 = highbd_convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7,
                                           y_filter, offset_vec);
        uint16x8_t d1 = highbd_convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8,
                                           y_filter, offset_vec);
        uint16x8_t d2 = highbd_convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9,
                                           y_filter, offset_vec);
        uint16x8_t d3 = highbd_convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10,
                                           y_filter, offset_vec);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height != 0);
      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w != 0);
  }
}

void av1_highbd_dist_wtd_convolve_y_neon(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_y, const int subpel_y_qn,
    ConvolveParams *conv_params, int bd) {
  DECLARE_ALIGNED(16, uint16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE]);
  CONV_BUF_TYPE *dst16 = conv_params->dst;
  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);
  int dst16_stride = conv_params->dst_stride;
  const int im_stride = MAX_SB_SIZE;
  const int vert_offset = filter_params_y->taps / 2 - 1;
  assert(FILTER_BITS == COMPOUND_ROUND1_BITS);
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int round_offset_avg = (1 << (offset_bits - conv_params->round_1)) +
                               (1 << (offset_bits - conv_params->round_1 - 1));
  const int round_offset_conv = (1 << (conv_params->round_0 - 1)) +
                                (1 << (bd + FILTER_BITS)) +
                                (1 << (bd + FILTER_BITS - 1));

  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  src -= vert_offset * src_stride;

  if (bd == 12) {
    if (conv_params->do_average) {
      if (y_filter_taps <= 6) {
        highbd_12_dist_wtd_convolve_y_6tap_neon(
            src + src_stride, src_stride, im_block, im_stride, w, h,
            y_filter_ptr, round_offset_conv);
      } else {
        highbd_12_dist_wtd_convolve_y_8tap_neon(src, src_stride, im_block,
                                                im_stride, w, h, y_filter_ptr,
                                                round_offset_conv);
      }
      if (conv_params->use_dist_wtd_comp_avg) {
        highbd_12_dist_wtd_comp_avg_neon(im_block, im_stride, dst, dst_stride,
                                         w, h, conv_params, round_offset_avg,
                                         bd);
      } else {
        highbd_12_comp_avg_neon(im_block, im_stride, dst, dst_stride, w, h,
                                conv_params, round_offset_avg, bd);
      }
    } else {
      if (y_filter_taps <= 6) {
        highbd_12_dist_wtd_convolve_y_6tap_neon(
            src + src_stride, src_stride, dst16, dst16_stride, w, h,
            y_filter_ptr, round_offset_conv);
      } else {
        highbd_12_dist_wtd_convolve_y_8tap_neon(
            src, src_stride, dst16, dst16_stride, w, h, y_filter_ptr,
            round_offset_conv);
      }
    }
  } else {
    if (conv_params->do_average) {
      if (y_filter_taps <= 6) {
        highbd_dist_wtd_convolve_y_6tap_neon(src + src_stride, src_stride,
                                             im_block, im_stride, w, h,
                                             y_filter_ptr, round_offset_conv);
      } else {
        highbd_dist_wtd_convolve_y_8tap_neon(src, src_stride, im_block,
                                             im_stride, w, h, y_filter_ptr,
                                             round_offset_conv);
      }
      if (conv_params->use_dist_wtd_comp_avg) {
        highbd_dist_wtd_comp_avg_neon(im_block, im_stride, dst, dst_stride, w,
                                      h, conv_params, round_offset_avg, bd);
      } else {
        highbd_comp_avg_neon(im_block, im_stride, dst, dst_stride, w, h,
                             conv_params, round_offset_avg, bd);
      }
    } else {
      if (y_filter_taps <= 6) {
        highbd_dist_wtd_convolve_y_6tap_neon(src + src_stride, src_stride,
                                             dst16, dst16_stride, w, h,
                                             y_filter_ptr, round_offset_conv);
      } else {
        highbd_dist_wtd_convolve_y_8tap_neon(src, src_stride, dst16,
                                             dst16_stride, w, h, y_filter_ptr,
                                             round_offset_conv);
      }
    }
  }
}

static INLINE void highbd_2d_copy_neon(const uint16_t *src_ptr, int src_stride,
                                       uint16_t *dst_ptr, int dst_stride, int w,
                                       int h, const int round_bits,
                                       const int offset) {
  if (w <= 4) {
    const int16x4_t round_shift_s16 = vdup_n_s16(round_bits);
    const uint16x4_t offset_u16 = vdup_n_u16(offset);

    for (int y = 0; y < h; ++y) {
      const uint16x4_t s = vld1_u16(src_ptr + y * src_stride);
      uint16x4_t d = vshl_u16(s, round_shift_s16);
      d = vadd_u16(d, offset_u16);
      if (w == 2) {
        store_u16_2x1(dst_ptr + y * dst_stride, d, 0);
      } else {
        vst1_u16(dst_ptr + y * dst_stride, d);
      }
    }
  } else {
    const int16x8_t round_shift_s16 = vdupq_n_s16(round_bits);
    const uint16x8_t offset_u16 = vdupq_n_u16(offset);

    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; x += 8) {
        const uint16x8_t s = vld1q_u16(src_ptr + y * src_stride + x);
        uint16x8_t d = vshlq_u16(s, round_shift_s16);
        d = vaddq_u16(d, offset_u16);
        vst1q_u16(dst_ptr + y * dst_stride + x, d);
      }
    }
  }
}

void av1_highbd_dist_wtd_convolve_2d_copy_neon(const uint16_t *src,
                                               int src_stride, uint16_t *dst,
                                               int dst_stride, int w, int h,
                                               ConvolveParams *conv_params,
                                               int bd) {
  DECLARE_ALIGNED(16, uint16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE]);

  const int im_stride = MAX_SB_SIZE;
  CONV_BUF_TYPE *dst16 = conv_params->dst;
  int dst16_stride = conv_params->dst_stride;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int round_offset = (1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1));
  const int round_bits =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  assert(round_bits >= 0);

  if (conv_params->do_average) {
    highbd_2d_copy_neon(src, src_stride, im_block, im_stride, w, h, round_bits,
                        round_offset);
  } else {
    highbd_2d_copy_neon(src, src_stride, dst16, dst16_stride, w, h, round_bits,
                        round_offset);
  }

  if (conv_params->do_average) {
    if (conv_params->use_dist_wtd_comp_avg) {
      if (bd == 12) {
        highbd_12_dist_wtd_comp_avg_neon(im_block, im_stride, dst, dst_stride,
                                         w, h, conv_params, round_offset, bd);
      } else {
        highbd_dist_wtd_comp_avg_neon(im_block, im_stride, dst, dst_stride, w,
                                      h, conv_params, round_offset, bd);
      }
    } else {
      if (bd == 12) {
        highbd_12_comp_avg_neon(im_block, im_stride, dst, dst_stride, w, h,
                                conv_params, round_offset, bd);
      } else {
        highbd_comp_avg_neon(im_block, im_stride, dst, dst_stride, w, h,
                             conv_params, round_offset, bd);
      }
    }
  }
}

static INLINE uint16x4_t highbd_convolve6_4_2d_v(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x8_t y_filter, const int32x4_t offset) {
  // Values at indices 0 and 7 of y_filter are zero.
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter);

  int32x4_t sum = vmlal_lane_s16(offset, s0, y_filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s1, y_filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s2, y_filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s3, y_filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s4, y_filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s5, y_filter_4_7, 2);

  return vqrshrun_n_s32(sum, COMPOUND_ROUND1_BITS);
}

static INLINE uint16x8_t highbd_convolve6_8_2d_v(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t y_filter, const int32x4_t offset) {
  // Values at indices 0 and 7 of y_filter are zero.
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter);

  int32x4_t sum0 = vmlal_lane_s16(offset, vget_low_s16(s0), y_filter_0_3, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), y_filter_0_3, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), y_filter_0_3, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), y_filter_4_7, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), y_filter_4_7, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), y_filter_4_7, 2);

  int32x4_t sum1 = vmlal_lane_s16(offset, vget_high_s16(s0), y_filter_0_3, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), y_filter_0_3, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), y_filter_0_3, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), y_filter_4_7, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), y_filter_4_7, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), y_filter_4_7, 2);

  return vcombine_u16(vqrshrun_n_s32(sum0, COMPOUND_ROUND1_BITS),
                      vqrshrun_n_s32(sum1, COMPOUND_ROUND1_BITS));
}

static INLINE void highbd_dist_wtd_convolve_2d_vert_6tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, int offset) {
  const int16x8_t y_filter = vld1q_s16(y_filter_ptr);
  const int32x4_t offset_vec = vdupq_n_s32(offset);

  if (w == 4) {
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    int16x4_t s0, s1, s2, s3, s4;
    load_s16_4x5(s, src_stride, &s0, &s1, &s2, &s3, &s4);
    s += 5 * src_stride;

    do {
      int16x4_t s5, s6, s7, s8;
      load_s16_4x4(s, src_stride, &s5, &s6, &s7, &s8);

      uint16x4_t d0 =
          highbd_convolve6_4_2d_v(s0, s1, s2, s3, s4, s5, y_filter, offset_vec);
      uint16x4_t d1 =
          highbd_convolve6_4_2d_v(s1, s2, s3, s4, s5, s6, y_filter, offset_vec);
      uint16x4_t d2 =
          highbd_convolve6_4_2d_v(s2, s3, s4, s5, s6, s7, y_filter, offset_vec);
      uint16x4_t d3 =
          highbd_convolve6_4_2d_v(s3, s4, s5, s6, s7, s8, y_filter, offset_vec);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      int16x8_t s0, s1, s2, s3, s4;
      load_s16_8x5(s, src_stride, &s0, &s1, &s2, &s3, &s4);
      s += 5 * src_stride;

      do {
        int16x8_t s5, s6, s7, s8;
        load_s16_8x4(s, src_stride, &s5, &s6, &s7, &s8);

        uint16x8_t d0 = highbd_convolve6_8_2d_v(s0, s1, s2, s3, s4, s5,
                                                y_filter, offset_vec);
        uint16x8_t d1 = highbd_convolve6_8_2d_v(s1, s2, s3, s4, s5, s6,
                                                y_filter, offset_vec);
        uint16x8_t d2 = highbd_convolve6_8_2d_v(s2, s3, s4, s5, s6, s7,
                                                y_filter, offset_vec);
        uint16x8_t d3 = highbd_convolve6_8_2d_v(s3, s4, s5, s6, s7, s8,
                                                y_filter, offset_vec);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height != 0);
      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w != 0);
  }
}

static INLINE uint16x4_t highbd_convolve8_4_2d_v(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x8_t y_filter,
    const int32x4_t offset) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter);

  int32x4_t sum = vmlal_lane_s16(offset, s0, y_filter_0_3, 0);
  sum = vmlal_lane_s16(sum, s1, y_filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s2, y_filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s3, y_filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s4, y_filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s5, y_filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s6, y_filter_4_7, 2);
  sum = vmlal_lane_s16(sum, s7, y_filter_4_7, 3);

  return vqrshrun_n_s32(sum, COMPOUND_ROUND1_BITS);
}

static INLINE uint16x8_t highbd_convolve8_8_2d_v(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16x8_t y_filter,
    const int32x4_t offset) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter);

  int32x4_t sum0 = vmlal_lane_s16(offset, vget_low_s16(s0), y_filter_0_3, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), y_filter_0_3, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), y_filter_0_3, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), y_filter_0_3, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), y_filter_4_7, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), y_filter_4_7, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s6), y_filter_4_7, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s7), y_filter_4_7, 3);

  int32x4_t sum1 = vmlal_lane_s16(offset, vget_high_s16(s0), y_filter_0_3, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), y_filter_0_3, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), y_filter_0_3, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), y_filter_0_3, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), y_filter_4_7, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), y_filter_4_7, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s6), y_filter_4_7, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s7), y_filter_4_7, 3);

  return vcombine_u16(vqrshrun_n_s32(sum0, COMPOUND_ROUND1_BITS),
                      vqrshrun_n_s32(sum1, COMPOUND_ROUND1_BITS));
}

static INLINE void highbd_dist_wtd_convolve_2d_vert_8tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *y_filter_ptr, int offset) {
  const int16x8_t y_filter = vld1q_s16(y_filter_ptr);
  const int32x4_t offset_vec = vdupq_n_s32(offset);

  if (w <= 4) {
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    int16x4_t s0, s1, s2, s3, s4, s5, s6;
    load_s16_4x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    s += 7 * src_stride;

    do {
      int16x4_t s7, s8, s9, s10;
      load_s16_4x4(s, src_stride, &s7, &s8, &s9, &s10);

      uint16x4_t d0 = highbd_convolve8_4_2d_v(s0, s1, s2, s3, s4, s5, s6, s7,
                                              y_filter, offset_vec);
      uint16x4_t d1 = highbd_convolve8_4_2d_v(s1, s2, s3, s4, s5, s6, s7, s8,
                                              y_filter, offset_vec);
      uint16x4_t d2 = highbd_convolve8_4_2d_v(s2, s3, s4, s5, s6, s7, s8, s9,
                                              y_filter, offset_vec);
      uint16x4_t d3 = highbd_convolve8_4_2d_v(s3, s4, s5, s6, s7, s8, s9, s10,
                                              y_filter, offset_vec);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    do {
      int height = h;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      int16x8_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      do {
        int16x8_t s7, s8, s9, s10;
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10);

        uint16x8_t d0 = highbd_convolve8_8_2d_v(s0, s1, s2, s3, s4, s5, s6, s7,
                                                y_filter, offset_vec);
        uint16x8_t d1 = highbd_convolve8_8_2d_v(s1, s2, s3, s4, s5, s6, s7, s8,
                                                y_filter, offset_vec);
        uint16x8_t d2 = highbd_convolve8_8_2d_v(s2, s3, s4, s5, s6, s7, s8, s9,
                                                y_filter, offset_vec);
        uint16x8_t d3 = highbd_convolve8_8_2d_v(s3, s4, s5, s6, s7, s8, s9, s10,
                                                y_filter, offset_vec);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height != 0);
      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w != 0);
  }
}

static INLINE void highbd_12_dist_wtd_convolve_2d_horiz_6tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, const int offset) {
  // The smallest block height is 4, and the horizontal convolution needs to
  // process an extra (filter_taps/2 - 1) lines for the vertical convolution.
  assert(h >= 5);
  const int32x4_t offset_vec = vdupq_n_s32(offset);

  const int16x8_t x_filter = vld1q_s16(x_filter_ptr);

  int height = h;

  do {
    int width = w;
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      int16x8_t s0[6], s1[6], s2[6], s3[6];
      load_s16_8x6(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                   &s0[4], &s0[5]);
      load_s16_8x6(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                   &s1[4], &s1[5]);
      load_s16_8x6(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                   &s2[4], &s2[5]);
      load_s16_8x6(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                   &s3[4], &s3[5]);

      uint16x8_t d0 = highbd_12_convolve6_8(s0[0], s0[1], s0[2], s0[3], s0[4],
                                            s0[5], x_filter, offset_vec);
      uint16x8_t d1 = highbd_12_convolve6_8(s1[0], s1[1], s1[2], s1[3], s1[4],
                                            s1[5], x_filter, offset_vec);
      uint16x8_t d2 = highbd_12_convolve6_8(s2[0], s2[1], s2[2], s2[3], s2[4],
                                            s2[5], x_filter, offset_vec);
      uint16x8_t d3 = highbd_12_convolve6_8(s3[0], s3[1], s3[2], s3[3], s3[4],
                                            s3[5], x_filter, offset_vec);

      store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

      s += 8;
      d += 8;
      width -= 8;
    } while (width != 0);
    src_ptr += 4 * src_stride;
    dst_ptr += 4 * dst_stride;
    height -= 4;
  } while (height > 4);

  do {
    int width = w;
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      int16x8_t s0[6];
      load_s16_8x6(s, 1, &s0[0], &s0[1], &s0[2], &s0[3], &s0[4], &s0[5]);

      uint16x8_t d0 = highbd_12_convolve6_8(s0[0], s0[1], s0[2], s0[3], s0[4],
                                            s0[5], x_filter, offset_vec);
      vst1q_u16(d, d0);

      s += 8;
      d += 8;
      width -= 8;
    } while (width != 0);
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  } while (--height != 0);
}

static INLINE void highbd_dist_wtd_convolve_2d_horiz_6tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, const int offset) {
  // The smallest block height is 4, and the horizontal convolution needs to
  // process an extra (filter_taps/2 - 1) lines for the vertical convolution.
  assert(h >= 5);
  const int32x4_t offset_vec = vdupq_n_s32(offset);

  const int16x8_t x_filter = vld1q_s16(x_filter_ptr);

  int height = h;

  do {
    int width = w;
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      int16x8_t s0[6], s1[6], s2[6], s3[6];
      load_s16_8x6(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                   &s0[4], &s0[5]);
      load_s16_8x6(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                   &s1[4], &s1[5]);
      load_s16_8x6(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                   &s2[4], &s2[5]);
      load_s16_8x6(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                   &s3[4], &s3[5]);

      uint16x8_t d0 = highbd_convolve6_8(s0[0], s0[1], s0[2], s0[3], s0[4],
                                         s0[5], x_filter, offset_vec);
      uint16x8_t d1 = highbd_convolve6_8(s1[0], s1[1], s1[2], s1[3], s1[4],
                                         s1[5], x_filter, offset_vec);
      uint16x8_t d2 = highbd_convolve6_8(s2[0], s2[1], s2[2], s2[3], s2[4],
                                         s2[5], x_filter, offset_vec);
      uint16x8_t d3 = highbd_convolve6_8(s3[0], s3[1], s3[2], s3[3], s3[4],
                                         s3[5], x_filter, offset_vec);

      store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

      s += 8;
      d += 8;
      width -= 8;
    } while (width != 0);
    src_ptr += 4 * src_stride;
    dst_ptr += 4 * dst_stride;
    height -= 4;
  } while (height > 4);

  do {
    int width = w;
    const int16_t *s = (const int16_t *)src_ptr;
    uint16_t *d = dst_ptr;

    do {
      int16x8_t s0[6];
      load_s16_8x6(s, 1, &s0[0], &s0[1], &s0[2], &s0[3], &s0[4], &s0[5]);

      uint16x8_t d0 = highbd_convolve6_8(s0[0], s0[1], s0[2], s0[3], s0[4],
                                         s0[5], x_filter, offset_vec);
      vst1q_u16(d, d0);

      s += 8;
      d += 8;
      width -= 8;
    } while (width != 0);
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  } while (--height != 0);
}

static INLINE void highbd_12_dist_wtd_convolve_2d_horiz_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, const int offset) {
  // The smallest block height is 4, and the horizontal convolution needs to
  // process an extra (filter_taps/2 - 1) lines for the vertical convolution.
  assert(h >= 5);
  const int32x4_t offset_vec = vdupq_n_s32(offset);

  if (w == 4) {
    // 4-tap filters are used for blocks having width == 4.
    const int16x4_t x_filter = vld1_s16(x_filter_ptr + 2);
    const int16_t *s = (const int16_t *)(src_ptr + 1);
    uint16_t *d = dst_ptr;

    do {
      int16x4_t s0[4], s1[4], s2[4], s3[4];
      load_s16_4x4(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3]);
      load_s16_4x4(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3]);
      load_s16_4x4(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3]);
      load_s16_4x4(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3]);

      uint16x4_t d0 = highbd_12_convolve4_4_x(s0, x_filter, offset_vec);
      uint16x4_t d1 = highbd_12_convolve4_4_x(s1, x_filter, offset_vec);
      uint16x4_t d2 = highbd_12_convolve4_4_x(s2, x_filter, offset_vec);
      uint16x4_t d3 = highbd_12_convolve4_4_x(s3, x_filter, offset_vec);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h > 4);

    do {
      int16x4_t s0[4];
      load_s16_4x4(s, 1, &s0[0], &s0[1], &s0[2], &s0[3]);

      uint16x4_t d0 = highbd_12_convolve4_4_x(s0, x_filter, offset_vec);
      vst1_u16(d, d0);

      s += src_stride;
      d += dst_stride;
    } while (--h != 0);
  } else {
    const int16x8_t x_filter = vld1q_s16(x_filter_ptr);
    int height = h;

    do {
      int width = w;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      do {
        int16x8_t s0[8], s1[8], s2[8], s3[8];
        load_s16_8x8(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                     &s0[4], &s0[5], &s0[6], &s0[7]);
        load_s16_8x8(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                     &s1[4], &s1[5], &s1[6], &s1[7]);
        load_s16_8x8(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                     &s2[4], &s2[5], &s2[6], &s2[7]);
        load_s16_8x8(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                     &s3[4], &s3[5], &s3[6], &s3[7]);

        uint16x8_t d0 =
            highbd_12_convolve8_8(s0[0], s0[1], s0[2], s0[3], s0[4], s0[5],
                                  s0[6], s0[7], x_filter, offset_vec);
        uint16x8_t d1 =
            highbd_12_convolve8_8(s1[0], s1[1], s1[2], s1[3], s1[4], s1[5],
                                  s1[6], s1[7], x_filter, offset_vec);
        uint16x8_t d2 =
            highbd_12_convolve8_8(s2[0], s2[1], s2[2], s2[3], s2[4], s2[5],
                                  s2[6], s2[7], x_filter, offset_vec);
        uint16x8_t d3 =
            highbd_12_convolve8_8(s3[0], s3[1], s3[2], s3[3], s3[4], s3[5],
                                  s3[6], s3[7], x_filter, offset_vec);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height > 4);

    do {
      int width = w;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      do {
        int16x8_t s0[8];
        load_s16_8x8(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                     &s0[4], &s0[5], &s0[6], &s0[7]);

        uint16x8_t d0 =
            highbd_12_convolve8_8(s0[0], s0[1], s0[2], s0[3], s0[4], s0[5],
                                  s0[6], s0[7], x_filter, offset_vec);
        vst1q_u16(d, d0);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--height != 0);
  }
}

static INLINE void highbd_dist_wtd_convolve_2d_horiz_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int16_t *x_filter_ptr, const int offset) {
  // The smallest block height is 4, and the horizontal convolution needs to
  // process an extra (filter_taps/2 - 1) lines for the vertical convolution.
  assert(h >= 5);
  const int32x4_t offset_vec = vdupq_n_s32(offset);

  if (w == 4) {
    // 4-tap filters are used for blocks having width == 4.
    const int16x4_t x_filter = vld1_s16(x_filter_ptr + 2);
    const int16_t *s = (const int16_t *)(src_ptr + 1);
    uint16_t *d = dst_ptr;

    do {
      int16x4_t s0[4], s1[4], s2[4], s3[4];
      load_s16_4x4(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3]);
      load_s16_4x4(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3]);
      load_s16_4x4(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3]);
      load_s16_4x4(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3]);

      uint16x4_t d0 = highbd_convolve4_4_x(s0, x_filter, offset_vec);
      uint16x4_t d1 = highbd_convolve4_4_x(s1, x_filter, offset_vec);
      uint16x4_t d2 = highbd_convolve4_4_x(s2, x_filter, offset_vec);
      uint16x4_t d3 = highbd_convolve4_4_x(s3, x_filter, offset_vec);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h > 4);

    do {
      int16x4_t s0[4];
      load_s16_4x4(s, 1, &s0[0], &s0[1], &s0[2], &s0[3]);

      uint16x4_t d0 = highbd_convolve4_4_x(s0, x_filter, offset_vec);
      vst1_u16(d, d0);

      s += src_stride;
      d += dst_stride;
    } while (--h != 0);
  } else {
    const int16x8_t x_filter = vld1q_s16(x_filter_ptr);
    int height = h;

    do {
      int width = w;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      do {
        int16x8_t s0[8], s1[8], s2[8], s3[8];
        load_s16_8x8(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                     &s0[4], &s0[5], &s0[6], &s0[7]);
        load_s16_8x8(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                     &s1[4], &s1[5], &s1[6], &s1[7]);
        load_s16_8x8(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                     &s2[4], &s2[5], &s2[6], &s2[7]);
        load_s16_8x8(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                     &s3[4], &s3[5], &s3[6], &s3[7]);

        uint16x8_t d0 =
            highbd_convolve8_8(s0[0], s0[1], s0[2], s0[3], s0[4], s0[5], s0[6],
                               s0[7], x_filter, offset_vec);
        uint16x8_t d1 =
            highbd_convolve8_8(s1[0], s1[1], s1[2], s1[3], s1[4], s1[5], s1[6],
                               s1[7], x_filter, offset_vec);
        uint16x8_t d2 =
            highbd_convolve8_8(s2[0], s2[1], s2[2], s2[3], s2[4], s2[5], s2[6],
                               s2[7], x_filter, offset_vec);
        uint16x8_t d3 =
            highbd_convolve8_8(s3[0], s3[1], s3[2], s3[3], s3[4], s3[5], s3[6],
                               s3[7], x_filter, offset_vec);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height > 4);

    do {
      int width = w;
      const int16_t *s = (const int16_t *)src_ptr;
      uint16_t *d = dst_ptr;

      do {
        int16x8_t s0[8];
        load_s16_8x8(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                     &s0[4], &s0[5], &s0[6], &s0[7]);

        uint16x8_t d0 =
            highbd_convolve8_8(s0[0], s0[1], s0[2], s0[3], s0[4], s0[5], s0[6],
                               s0[7], x_filter, offset_vec);
        vst1q_u16(d, d0);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--height != 0);
  }
}

void av1_highbd_dist_wtd_convolve_2d_neon(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_x,
    const InterpFilterParams *filter_params_y, const int subpel_x_qn,
    const int subpel_y_qn, ConvolveParams *conv_params, int bd) {
  DECLARE_ALIGNED(16, uint16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE]);
  DECLARE_ALIGNED(16, uint16_t,
                  im_block2[(MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE]);

  CONV_BUF_TYPE *dst16 = conv_params->dst;
  int dst16_stride = conv_params->dst_stride;
  const int x_filter_taps = get_filter_tap(filter_params_x, subpel_x_qn);
  const int clamped_x_taps = x_filter_taps < 6 ? 6 : x_filter_taps;
  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);
  const int clamped_y_taps = y_filter_taps < 6 ? 6 : y_filter_taps;

  const int im_h = h + clamped_y_taps - 1;
  const int im_stride = MAX_SB_SIZE;
  const int vert_offset = clamped_y_taps / 2 - 1;
  const int horiz_offset = clamped_x_taps / 2 - 1;
  // The extra shim of (1 << (conv_params->round_0 - 1)) allows us to use a
  // faster non-rounding non-saturating left shift.
  const int round_offset_conv_x =
      (1 << (bd + FILTER_BITS - 1)) + (1 << (conv_params->round_0 - 1));
  const int y_offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int round_offset_conv_y = (1 << y_offset_bits);
  const int round_offset_avg =
      ((1 << (y_offset_bits - conv_params->round_1)) +
       (1 << (y_offset_bits - conv_params->round_1 - 1)));

  const uint16_t *src_ptr = src - vert_offset * src_stride - horiz_offset;

  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);
  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  // horizontal filter
  if (bd == 12) {
    if (x_filter_taps <= 6 && w != 4) {
      highbd_12_dist_wtd_convolve_2d_horiz_6tap_neon(
          src_ptr, src_stride, im_block, im_stride, w, im_h, x_filter_ptr,
          round_offset_conv_x);
    } else {
      highbd_12_dist_wtd_convolve_2d_horiz_neon(
          src_ptr, src_stride, im_block, im_stride, w, im_h, x_filter_ptr,
          round_offset_conv_x);
    }
  } else {
    if (x_filter_taps <= 6 && w != 4) {
      highbd_dist_wtd_convolve_2d_horiz_6tap_neon(
          src_ptr, src_stride, im_block, im_stride, w, im_h, x_filter_ptr,
          round_offset_conv_x);
    } else {
      highbd_dist_wtd_convolve_2d_horiz_neon(src_ptr, src_stride, im_block,
                                             im_stride, w, im_h, x_filter_ptr,
                                             round_offset_conv_x);
    }
  }

  // vertical filter
  if (y_filter_taps <= 6) {
    if (conv_params->do_average) {
      highbd_dist_wtd_convolve_2d_vert_6tap_neon(im_block, im_stride, im_block2,
                                                 im_stride, w, h, y_filter_ptr,
                                                 round_offset_conv_y);
    } else {
      highbd_dist_wtd_convolve_2d_vert_6tap_neon(
          im_block, im_stride, dst16, dst16_stride, w, h, y_filter_ptr,
          round_offset_conv_y);
    }
  } else {
    if (conv_params->do_average) {
      highbd_dist_wtd_convolve_2d_vert_8tap_neon(im_block, im_stride, im_block2,
                                                 im_stride, w, h, y_filter_ptr,
                                                 round_offset_conv_y);
    } else {
      highbd_dist_wtd_convolve_2d_vert_8tap_neon(
          im_block, im_stride, dst16, dst16_stride, w, h, y_filter_ptr,
          round_offset_conv_y);
    }
  }

  // Do the compound averaging outside the loop, avoids branching within the
  // main loop
  if (conv_params->do_average) {
    if (conv_params->use_dist_wtd_comp_avg) {
      if (bd == 12) {
        highbd_12_dist_wtd_comp_avg_neon(im_block2, im_stride, dst, dst_stride,
                                         w, h, conv_params, round_offset_avg,
                                         bd);
      } else {
        highbd_dist_wtd_comp_avg_neon(im_block2, im_stride, dst, dst_stride, w,
                                      h, conv_params, round_offset_avg, bd);
      }
    } else {
      if (bd == 12) {
        highbd_12_comp_avg_neon(im_block2, im_stride, dst, dst_stride, w, h,
                                conv_params, round_offset_avg, bd);
      } else {
        highbd_comp_avg_neon(im_block2, im_stride, dst, dst_stride, w, h,
                             conv_params, round_offset_avg, bd);
      }
    }
  }
}
