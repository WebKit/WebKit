/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved.
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
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_ports/mem.h"
#include "av1/common/convolve.h"
#include "av1/common/filter.h"
#include "av1/common/arm/highbd_convolve_neon.h"

static inline void highbd_dist_wtd_comp_avg_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, ConvolveParams *conv_params, const int round_bits,
    const int offset, const int bd) {
  CONV_BUF_TYPE *ref_ptr = conv_params->dst;
  const int ref_stride = conv_params->dst_stride;
  const int32x4_t round_shift = vdupq_n_s32(-round_bits);
  const uint32x4_t offset_vec = vdupq_n_u32(offset);
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
  uint16x4_t fwd_offset = vdup_n_u16(conv_params->fwd_offset);
  uint16x4_t bck_offset = vdup_n_u16(conv_params->bck_offset);

  // Weighted averaging
  if (w <= 4) {
    do {
      const uint16x4_t src = vld1_u16(src_ptr);
      const uint16x4_t ref = vld1_u16(ref_ptr);

      uint32x4_t wtd_avg = vmull_u16(ref, fwd_offset);
      wtd_avg = vmlal_u16(wtd_avg, src, bck_offset);
      wtd_avg = vshrq_n_u32(wtd_avg, DIST_PRECISION_BITS);
      int32x4_t d0 = vreinterpretq_s32_u32(vsubq_u32(wtd_avg, offset_vec));
      d0 = vqrshlq_s32(d0, round_shift);

      uint16x4_t d0_u16 = vqmovun_s32(d0);
      d0_u16 = vmin_u16(d0_u16, vget_low_u16(max));

      if (w == 2) {
        store_u16_2x1(dst_ptr, d0_u16);
      } else {
        vst1_u16(dst_ptr, d0_u16);
      }

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
        d0 = vqrshlq_s32(d0, round_shift);

        uint32x4_t wtd_avg1 = vmull_u16(vget_high_u16(r), fwd_offset);
        wtd_avg1 = vmlal_u16(wtd_avg1, vget_high_u16(s), bck_offset);
        wtd_avg1 = vshrq_n_u32(wtd_avg1, DIST_PRECISION_BITS);
        int32x4_t d1 = vreinterpretq_s32_u32(vsubq_u32(wtd_avg1, offset_vec));
        d1 = vqrshlq_s32(d1, round_shift);

        uint16x8_t d01 = vcombine_u16(vqmovun_s32(d0), vqmovun_s32(d1));
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

static inline void highbd_comp_avg_neon(const uint16_t *src_ptr, int src_stride,
                                        uint16_t *dst_ptr, int dst_stride,
                                        int w, int h,
                                        ConvolveParams *conv_params,
                                        const int round_bits, const int offset,
                                        const int bd) {
  CONV_BUF_TYPE *ref_ptr = conv_params->dst;
  const int ref_stride = conv_params->dst_stride;
  const int32x4_t round_shift = vdupq_n_s32(-round_bits);
  const uint16x4_t offset_vec = vdup_n_u16(offset);
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

  if (w <= 4) {
    do {
      const uint16x4_t src = vld1_u16(src_ptr);
      const uint16x4_t ref = vld1_u16(ref_ptr);

      uint16x4_t avg = vhadd_u16(src, ref);
      int32x4_t d0 = vreinterpretq_s32_u32(vsubl_u16(avg, offset_vec));
      d0 = vqrshlq_s32(d0, round_shift);

      uint16x4_t d0_u16 = vqmovun_s32(d0);
      d0_u16 = vmin_u16(d0_u16, vget_low_u16(max));

      if (w == 2) {
        store_u16_2x1(dst_ptr, d0_u16);
      } else {
        vst1_u16(dst_ptr, d0_u16);
      }

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
        d0_lo = vqrshlq_s32(d0_lo, round_shift);
        d0_hi = vqrshlq_s32(d0_hi, round_shift);

        uint16x8_t d0 = vcombine_u16(vqmovun_s32(d0_lo), vqmovun_s32(d0_hi));
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

static inline void highbd_convolve_2d_x_scale_8tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int subpel_x_qn, const int x_step_qn,
    const InterpFilterParams *filter_params, ConvolveParams *conv_params,
    const int offset) {
  static const uint32_t kIdx[4] = { 0, 1, 2, 3 };
  const uint32x4_t idx = vld1q_u32(kIdx);
  const uint32x4_t subpel_mask = vdupq_n_u32(SCALE_SUBPEL_MASK);
  const int32x4_t shift_s32 = vdupq_n_s32(-conv_params->round_0);
  const int32x4_t offset_s32 = vdupq_n_s32(offset);

  if (w <= 4) {
    int height = h;
    uint16_t *d = dst_ptr;

    do {
      int x_qn = subpel_x_qn;

      // Load 4 src vectors at a time, they might be the same, but we have to
      // calculate the indices anyway. Doing it in SIMD and then storing the
      // indices is faster than having to calculate the expression
      // &src_ptr[((x_qn + 0*x_step_qn) >> SCALE_SUBPEL_BITS)] 4 times
      // Ideally this should be a gather using the indices, but NEON does not
      // have that, so have to emulate
      const uint32x4_t xqn_idx = vmlaq_n_u32(vdupq_n_u32(x_qn), idx, x_step_qn);
      // We have to multiply x2 to get the actual pointer as sizeof(uint16_t) =
      // 2
      const uint32x4_t src_idx_u32 =
          vshlq_n_u32(vshrq_n_u32(xqn_idx, SCALE_SUBPEL_BITS), 1);
#if AOM_ARCH_AARCH64
      uint64x2_t src4[2];
      src4[0] = vaddw_u32(vdupq_n_u64((const uint64_t)src_ptr),
                          vget_low_u32(src_idx_u32));
      src4[1] = vaddw_u32(vdupq_n_u64((const uint64_t)src_ptr),
                          vget_high_u32(src_idx_u32));
      int16_t *src4_ptr[4];
      uint64_t *tmp_ptr = (uint64_t *)&src4_ptr;
      vst1q_u64(tmp_ptr, src4[0]);
      vst1q_u64(tmp_ptr + 2, src4[1]);
#else
      uint32x4_t src4;
      src4 = vaddq_u32(vdupq_n_u32((const uint32_t)src_ptr), src_idx_u32);
      int16_t *src4_ptr[4];
      uint32_t *tmp_ptr = (uint32_t *)&src4_ptr;
      vst1q_u32(tmp_ptr, src4);
#endif  // AOM_ARCH_AARCH64
      // Same for the filter vectors
      const int32x4_t filter_idx_s32 = vreinterpretq_s32_u32(
          vshrq_n_u32(vandq_u32(xqn_idx, subpel_mask), SCALE_EXTRA_BITS));
      int32_t x_filter4_idx[4];
      vst1q_s32(x_filter4_idx, filter_idx_s32);
      const int16_t *x_filter4_ptr[4];

      // Load source
      int16x8_t s0 = vld1q_s16(src4_ptr[0]);
      int16x8_t s1 = vld1q_s16(src4_ptr[1]);
      int16x8_t s2 = vld1q_s16(src4_ptr[2]);
      int16x8_t s3 = vld1q_s16(src4_ptr[3]);

      // We could easily do this using SIMD as well instead of calling the
      // inline function 4 times.
      x_filter4_ptr[0] =
          av1_get_interp_filter_subpel_kernel(filter_params, x_filter4_idx[0]);
      x_filter4_ptr[1] =
          av1_get_interp_filter_subpel_kernel(filter_params, x_filter4_idx[1]);
      x_filter4_ptr[2] =
          av1_get_interp_filter_subpel_kernel(filter_params, x_filter4_idx[2]);
      x_filter4_ptr[3] =
          av1_get_interp_filter_subpel_kernel(filter_params, x_filter4_idx[3]);

      // Actually load the filters
      const int16x8_t x_filter0 = vld1q_s16(x_filter4_ptr[0]);
      const int16x8_t x_filter1 = vld1q_s16(x_filter4_ptr[1]);
      const int16x8_t x_filter2 = vld1q_s16(x_filter4_ptr[2]);
      const int16x8_t x_filter3 = vld1q_s16(x_filter4_ptr[3]);

      // Group low and high parts and transpose
      int16x4_t filters_lo[] = { vget_low_s16(x_filter0),
                                 vget_low_s16(x_filter1),
                                 vget_low_s16(x_filter2),
                                 vget_low_s16(x_filter3) };
      int16x4_t filters_hi[] = { vget_high_s16(x_filter0),
                                 vget_high_s16(x_filter1),
                                 vget_high_s16(x_filter2),
                                 vget_high_s16(x_filter3) };
      transpose_array_inplace_u16_4x4((uint16x4_t *)filters_lo);
      transpose_array_inplace_u16_4x4((uint16x4_t *)filters_hi);

      // Run the 2D Scale convolution
      uint16x4_t d0 = highbd_convolve8_2d_scale_horiz4x8_s32_s16(
          s0, s1, s2, s3, filters_lo, filters_hi, shift_s32, offset_s32);

      if (w == 2) {
        store_u16_2x1(d, d0);
      } else {
        vst1_u16(d, d0);
      }

      src_ptr += src_stride;
      d += dst_stride;
      height--;
    } while (height > 0);
  } else {
    int height = h;

    do {
      int width = w;
      int x_qn = subpel_x_qn;
      uint16_t *d = dst_ptr;
      const uint16_t *s = src_ptr;

      do {
        // Load 4 src vectors at a time, they might be the same, but we have to
        // calculate the indices anyway. Doing it in SIMD and then storing the
        // indices is faster than having to calculate the expression
        // &src_ptr[((x_qn + 0*x_step_qn) >> SCALE_SUBPEL_BITS)] 4 times
        // Ideally this should be a gather using the indices, but NEON does not
        // have that, so have to emulate
        const uint32x4_t xqn_idx =
            vmlaq_n_u32(vdupq_n_u32(x_qn), idx, x_step_qn);
        // We have to multiply x2 to get the actual pointer as sizeof(uint16_t)
        // = 2
        const uint32x4_t src_idx_u32 =
            vshlq_n_u32(vshrq_n_u32(xqn_idx, SCALE_SUBPEL_BITS), 1);
#if AOM_ARCH_AARCH64
        uint64x2_t src4[2];
        src4[0] = vaddw_u32(vdupq_n_u64((const uint64_t)s),
                            vget_low_u32(src_idx_u32));
        src4[1] = vaddw_u32(vdupq_n_u64((const uint64_t)s),
                            vget_high_u32(src_idx_u32));
        int16_t *src4_ptr[4];
        uint64_t *tmp_ptr = (uint64_t *)&src4_ptr;
        vst1q_u64(tmp_ptr, src4[0]);
        vst1q_u64(tmp_ptr + 2, src4[1]);
#else
        uint32x4_t src4;
        src4 = vaddq_u32(vdupq_n_u32((const uint32_t)s), src_idx_u32);
        int16_t *src4_ptr[4];
        uint32_t *tmp_ptr = (uint32_t *)&src4_ptr;
        vst1q_u32(tmp_ptr, src4);
#endif  // AOM_ARCH_AARCH64
        // Same for the filter vectors
        const int32x4_t filter_idx_s32 = vreinterpretq_s32_u32(
            vshrq_n_u32(vandq_u32(xqn_idx, subpel_mask), SCALE_EXTRA_BITS));
        int32_t x_filter4_idx[4];
        vst1q_s32(x_filter4_idx, filter_idx_s32);
        const int16_t *x_filter4_ptr[4];

        // Load source
        int16x8_t s0 = vld1q_s16(src4_ptr[0]);
        int16x8_t s1 = vld1q_s16(src4_ptr[1]);
        int16x8_t s2 = vld1q_s16(src4_ptr[2]);
        int16x8_t s3 = vld1q_s16(src4_ptr[3]);

        // We could easily do this using SIMD as well instead of calling the
        // inline function 4 times.
        x_filter4_ptr[0] = av1_get_interp_filter_subpel_kernel(
            filter_params, x_filter4_idx[0]);
        x_filter4_ptr[1] = av1_get_interp_filter_subpel_kernel(
            filter_params, x_filter4_idx[1]);
        x_filter4_ptr[2] = av1_get_interp_filter_subpel_kernel(
            filter_params, x_filter4_idx[2]);
        x_filter4_ptr[3] = av1_get_interp_filter_subpel_kernel(
            filter_params, x_filter4_idx[3]);

        // Actually load the filters
        const int16x8_t x_filter0 = vld1q_s16(x_filter4_ptr[0]);
        const int16x8_t x_filter1 = vld1q_s16(x_filter4_ptr[1]);
        const int16x8_t x_filter2 = vld1q_s16(x_filter4_ptr[2]);
        const int16x8_t x_filter3 = vld1q_s16(x_filter4_ptr[3]);

        // Group low and high parts and transpose
        int16x4_t filters_lo[] = { vget_low_s16(x_filter0),
                                   vget_low_s16(x_filter1),
                                   vget_low_s16(x_filter2),
                                   vget_low_s16(x_filter3) };
        int16x4_t filters_hi[] = { vget_high_s16(x_filter0),
                                   vget_high_s16(x_filter1),
                                   vget_high_s16(x_filter2),
                                   vget_high_s16(x_filter3) };
        transpose_array_inplace_u16_4x4((uint16x4_t *)filters_lo);
        transpose_array_inplace_u16_4x4((uint16x4_t *)filters_hi);

        // Run the 2D Scale X convolution
        uint16x4_t d0 = highbd_convolve8_2d_scale_horiz4x8_s32_s16(
            s0, s1, s2, s3, filters_lo, filters_hi, shift_s32, offset_s32);

        vst1_u16(d, d0);

        x_qn += 4 * x_step_qn;
        d += 4;
        width -= 4;
      } while (width > 0);

      src_ptr += src_stride;
      dst_ptr += dst_stride;
      height--;
    } while (height > 0);
  }
}

static inline void highbd_convolve_2d_y_scale_8tap_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int subpel_y_qn, const int y_step_qn,
    const InterpFilterParams *filter_params, const int round1_bits,
    const int offset) {
  const int32x4_t offset_s32 = vdupq_n_s32(1 << offset);

  const int32x4_t round1_shift_s32 = vdupq_n_s32(-round1_bits);
  if (w <= 4) {
    int height = h;
    uint16_t *d = dst_ptr;
    int y_qn = subpel_y_qn;

    do {
      const int16_t *s =
          (const int16_t *)&src_ptr[(y_qn >> SCALE_SUBPEL_BITS) * src_stride];

      int16x4_t s0, s1, s2, s3, s4, s5, s6, s7;
      load_s16_4x8(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

      const int y_filter_idx = (y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
      const int16_t *y_filter_ptr =
          av1_get_interp_filter_subpel_kernel(filter_params, y_filter_idx);
      const int16x8_t y_filter = vld1q_s16(y_filter_ptr);

      uint16x4_t d0 = highbd_convolve8_4_srsub_s32_s16(
          s0, s1, s2, s3, s4, s5, s6, s7, y_filter, round1_shift_s32,
          offset_s32, vdupq_n_s32(0));

      if (w == 2) {
        store_u16_2x1(d, d0);
      } else {
        vst1_u16(d, d0);
      }

      y_qn += y_step_qn;
      d += dst_stride;
      height--;
    } while (height > 0);
  } else {
    int width = w;

    do {
      int height = h;
      int y_qn = subpel_y_qn;

      uint16_t *d = dst_ptr;

      do {
        const int16_t *s =
            (const int16_t *)&src_ptr[(y_qn >> SCALE_SUBPEL_BITS) * src_stride];
        int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;
        load_s16_8x8(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

        const int y_filter_idx = (y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
        const int16_t *y_filter_ptr =
            av1_get_interp_filter_subpel_kernel(filter_params, y_filter_idx);
        const int16x8_t y_filter = vld1q_s16(y_filter_ptr);

        uint16x8_t d0 = highbd_convolve8_8_srsub_s32_s16(
            s0, s1, s2, s3, s4, s5, s6, s7, y_filter, round1_shift_s32,
            offset_s32, vdupq_n_s32(0));
        vst1q_u16(d, d0);

        y_qn += y_step_qn;
        d += dst_stride;
        height--;
      } while (height > 0);
      src_ptr += 8;
      dst_ptr += 8;
      width -= 8;
    } while (width > 0);
  }
}

static inline void highbd_convolve_correct_offset_neon(
    const uint16_t *src_ptr, int src_stride, uint16_t *dst_ptr, int dst_stride,
    int w, int h, const int round_bits, const int offset, const int bd) {
  const int32x4_t round_shift_s32 = vdupq_n_s32(-round_bits);
  const int16x4_t offset_s16 = vdup_n_s16(offset);
  const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

  if (w <= 4) {
    for (int y = 0; y < h; ++y) {
      const int16x4_t s = vld1_s16((const int16_t *)src_ptr + y * src_stride);
      const int32x4_t d0 =
          vqrshlq_s32(vsubl_s16(s, offset_s16), round_shift_s32);
      uint16x4_t d = vqmovun_s32(d0);
      d = vmin_u16(d, vget_low_u16(max));
      if (w == 2) {
        store_u16_2x1(dst_ptr + y * dst_stride, d);
      } else {
        vst1_u16(dst_ptr + y * dst_stride, d);
      }
    }
  } else {
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; x += 8) {
        // Subtract round offset and convolve round
        const int16x8_t s =
            vld1q_s16((const int16_t *)src_ptr + y * src_stride + x);
        const int32x4_t d0 = vqrshlq_s32(vsubl_s16(vget_low_s16(s), offset_s16),
                                         round_shift_s32);
        const int32x4_t d1 = vqrshlq_s32(
            vsubl_s16(vget_high_s16(s), offset_s16), round_shift_s32);
        uint16x8_t d01 = vcombine_u16(vqmovun_s32(d0), vqmovun_s32(d1));
        d01 = vminq_u16(d01, max);
        vst1q_u16(dst_ptr + y * dst_stride + x, d01);
      }
    }
  }
}

void av1_highbd_convolve_2d_scale_neon(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_x,
    const InterpFilterParams *filter_params_y, const int subpel_x_qn,
    const int x_step_qn, const int subpel_y_qn, const int y_step_qn,
    ConvolveParams *conv_params, int bd) {
  uint16_t *im_block = (uint16_t *)aom_memalign(
      16, 2 * sizeof(uint16_t) * MAX_SB_SIZE * (MAX_SB_SIZE + MAX_FILTER_TAP));
  if (!im_block) return;
  uint16_t *im_block2 = (uint16_t *)aom_memalign(
      16, 2 * sizeof(uint16_t) * MAX_SB_SIZE * (MAX_SB_SIZE + MAX_FILTER_TAP));
  if (!im_block2) {
    aom_free(im_block);  // free the first block and return.
    return;
  }

  int im_h = (((h - 1) * y_step_qn + subpel_y_qn) >> SCALE_SUBPEL_BITS) +
             filter_params_y->taps;
  const int im_stride = MAX_SB_SIZE;
  const int bits =
      FILTER_BITS * 2 - conv_params->round_0 - conv_params->round_1;
  assert(bits >= 0);

  const int vert_offset = filter_params_y->taps / 2 - 1;
  const int horiz_offset = filter_params_x->taps / 2 - 1;
  const int x_offset_bits = (1 << (bd + FILTER_BITS - 1));
  const int y_offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int y_offset_correction =
      ((1 << (y_offset_bits - conv_params->round_1)) +
       (1 << (y_offset_bits - conv_params->round_1 - 1)));

  CONV_BUF_TYPE *dst16 = conv_params->dst;
  const int dst16_stride = conv_params->dst_stride;

  const uint16_t *src_ptr = src - vert_offset * src_stride - horiz_offset;

  highbd_convolve_2d_x_scale_8tap_neon(
      src_ptr, src_stride, im_block, im_stride, w, im_h, subpel_x_qn, x_step_qn,
      filter_params_x, conv_params, x_offset_bits);
  if (conv_params->is_compound && !conv_params->do_average) {
    highbd_convolve_2d_y_scale_8tap_neon(
        im_block, im_stride, dst16, dst16_stride, w, h, subpel_y_qn, y_step_qn,
        filter_params_y, conv_params->round_1, y_offset_bits);
  } else {
    highbd_convolve_2d_y_scale_8tap_neon(
        im_block, im_stride, im_block2, im_stride, w, h, subpel_y_qn, y_step_qn,
        filter_params_y, conv_params->round_1, y_offset_bits);
  }

  // Do the compound averaging outside the loop, avoids branching within the
  // main loop
  if (conv_params->is_compound) {
    if (conv_params->do_average) {
      if (conv_params->use_dist_wtd_comp_avg) {
        highbd_dist_wtd_comp_avg_neon(im_block2, im_stride, dst, dst_stride, w,
                                      h, conv_params, bits, y_offset_correction,
                                      bd);
      } else {
        highbd_comp_avg_neon(im_block2, im_stride, dst, dst_stride, w, h,
                             conv_params, bits, y_offset_correction, bd);
      }
    }
  } else {
    highbd_convolve_correct_offset_neon(im_block2, im_stride, dst, dst_stride,
                                        w, h, bits, y_offset_correction, bd);
  }
  aom_free(im_block);
  aom_free(im_block2);
}
