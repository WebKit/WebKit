/*
 *
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved.
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

#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "av1/common/arm/resize_neon.h"
#include "av1/common/resize.h"
#include "config/aom_scale_rtcd.h"
#include "config/av1_rtcd.h"

static inline void scale_plane_2_to_1_phase_0(const uint8_t *src,
                                              const int src_stride,
                                              uint8_t *dst,
                                              const int dst_stride, int w,
                                              int h) {
  assert(w > 0 && h > 0);

  do {
    const uint8_t *s = src;
    uint8_t *d = dst;
    int width = w;

    do {
      const uint8x16x2_t s0 = vld2q_u8(s);

      vst1q_u8(d, s0.val[0]);

      s += 32;
      d += 16;
      width -= 16;
    } while (width > 0);

    src += 2 * src_stride;
    dst += dst_stride;
  } while (--h != 0);
}

static inline void scale_plane_4_to_1_phase_0(const uint8_t *src,
                                              const int src_stride,
                                              uint8_t *dst,
                                              const int dst_stride, int w,
                                              int h) {
  assert(w > 0 && h > 0);

  do {
    const uint8_t *s = src;
    uint8_t *d = dst;
    int width = w;

    do {
      const uint8x16x4_t s0 = vld4q_u8(s);

      vst1q_u8(d, s0.val[0]);

      s += 64;
      d += 16;
      width -= 16;
    } while (width > 0);

    src += 4 * src_stride;
    dst += dst_stride;
  } while (--h != 0);
}

static inline uint8x16_t scale_plane_bilinear_kernel(
    const uint8x16_t s0_even, const uint8x16_t s0_odd, const uint8x16_t s1_even,
    const uint8x16_t s1_odd, const uint8x8_t filter0, const uint8x8_t filter1) {
  // A shim of 1 << (FILTER_BITS - 1) enables us to use non-rounding
  // shifts - which are generally faster than rounding shifts on modern CPUs.
  uint16x8_t offset = vdupq_n_u16(1 << (FILTER_BITS - 1));

  // Horizontal filtering
  uint16x8_t h0_lo = vmlal_u8(offset, vget_low_u8(s0_even), filter0);
  uint16x8_t h0_hi = vmlal_u8(offset, vget_high_u8(s0_even), filter0);
  uint16x8_t h1_lo = vmlal_u8(offset, vget_low_u8(s1_even), filter0);
  uint16x8_t h1_hi = vmlal_u8(offset, vget_high_u8(s1_even), filter0);

  h0_lo = vmlal_u8(h0_lo, vget_low_u8(s0_odd), filter1);
  h0_hi = vmlal_u8(h0_hi, vget_high_u8(s0_odd), filter1);
  h1_lo = vmlal_u8(h1_lo, vget_low_u8(s1_odd), filter1);
  h1_hi = vmlal_u8(h1_hi, vget_high_u8(s1_odd), filter1);

  const uint8x8_t h0_lo_u8 = vshrn_n_u16(h0_lo, FILTER_BITS);
  const uint8x8_t h0_hi_u8 = vshrn_n_u16(h0_hi, FILTER_BITS);
  const uint8x8_t h1_lo_u8 = vshrn_n_u16(h1_lo, FILTER_BITS);
  const uint8x8_t h1_hi_u8 = vshrn_n_u16(h1_hi, FILTER_BITS);

  // Vertical filtering
  uint16x8_t v_lo = vmlal_u8(offset, h0_lo_u8, filter0);
  uint16x8_t v_hi = vmlal_u8(offset, h0_hi_u8, filter0);

  v_lo = vmlal_u8(v_lo, h1_lo_u8, filter1);
  v_hi = vmlal_u8(v_hi, h1_hi_u8, filter1);

  return vcombine_u8(vshrn_n_u16(v_lo, FILTER_BITS),
                     vshrn_n_u16(v_hi, FILTER_BITS));
}

static inline void scale_plane_2_to_1_bilinear(
    const uint8_t *src, const int src_stride, uint8_t *dst,
    const int dst_stride, int w, int h, const int16_t f0, const int16_t f1) {
  assert(w > 0 && h > 0);
  const uint8x8_t filter0 = vdup_n_u8(f0);
  const uint8x8_t filter1 = vdup_n_u8(f1);

  do {
    const uint8_t *s = src;
    uint8_t *d = dst;
    int width = w;

    do {
      const uint8x16x2_t s0 = vld2q_u8(s + 0 * src_stride);
      const uint8x16x2_t s1 = vld2q_u8(s + 1 * src_stride);

      uint8x16_t d0 = scale_plane_bilinear_kernel(
          s0.val[0], s0.val[1], s1.val[0], s1.val[1], filter0, filter1);

      vst1q_u8(d, d0);

      s += 32;
      d += 16;
      width -= 16;
    } while (width > 0);

    src += 2 * src_stride;
    dst += dst_stride;
  } while (--h != 0);
}

static inline void scale_plane_4_to_1_bilinear(
    const uint8_t *src, const int src_stride, uint8_t *dst,
    const int dst_stride, int w, int h, const int16_t f0, const int16_t f1) {
  assert(w > 0 && h > 0);
  const uint8x8_t filter0 = vdup_n_u8(f0);
  const uint8x8_t filter1 = vdup_n_u8(f1);

  do {
    const uint8_t *s = src;
    uint8_t *d = dst;
    int width = w;

    do {
      const uint8x16x4_t s0 = vld4q_u8(s + 0 * src_stride);
      const uint8x16x4_t s1 = vld4q_u8(s + 1 * src_stride);

      uint8x16_t d0 = scale_plane_bilinear_kernel(
          s0.val[0], s0.val[1], s1.val[0], s1.val[1], filter0, filter1);

      vst1q_u8(d, d0);

      s += 64;
      d += 16;
      width -= 16;
    } while (width > 0);

    src += 4 * src_stride;
    dst += dst_stride;
  } while (--h != 0);
}

static inline void scale_2_to_1_horiz_6tap(const uint8_t *src,
                                           const int src_stride, int w, int h,
                                           uint8_t *dst, const int dst_stride,
                                           const int16x8_t filters) {
  do {
    uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7;
    load_u8_8x8(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

    transpose_elems_inplace_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

    int16x8_t s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
    int16x8_t s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
    int16x8_t s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
    int16x8_t s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
    int16x8_t s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
    int16x8_t s5 = vreinterpretq_s16_u16(vmovl_u8(t5));

    const uint8_t *s = src + 6;
    uint8_t *d = dst;
    int width = w;

    do {
      uint8x8_t t8, t9, t10, t11, t12, t13;
      load_u8_8x8(s, src_stride, &t6, &t7, &t8, &t9, &t10, &t11, &t12, &t13);

      transpose_elems_inplace_u8_8x8(&t6, &t7, &t8, &t9, &t10, &t11, &t12,
                                     &t13);

      int16x8_t s6 = vreinterpretq_s16_u16(vmovl_u8(t6));
      int16x8_t s7 = vreinterpretq_s16_u16(vmovl_u8(t7));
      int16x8_t s8 = vreinterpretq_s16_u16(vmovl_u8(t8));
      int16x8_t s9 = vreinterpretq_s16_u16(vmovl_u8(t9));
      int16x8_t s10 = vreinterpretq_s16_u16(vmovl_u8(t10));
      int16x8_t s11 = vreinterpretq_s16_u16(vmovl_u8(t11));
      int16x8_t s12 = vreinterpretq_s16_u16(vmovl_u8(t12));
      int16x8_t s13 = vreinterpretq_s16_u16(vmovl_u8(t13));

      uint8x8_t d0 = scale_filter6_8(s0, s1, s2, s3, s4, s5, filters);
      uint8x8_t d1 = scale_filter6_8(s2, s3, s4, s5, s6, s7, filters);
      uint8x8_t d2 = scale_filter6_8(s4, s5, s6, s7, s8, s9, filters);
      uint8x8_t d3 = scale_filter6_8(s6, s7, s8, s9, s10, s11, filters);

      transpose_elems_inplace_u8_8x4(&d0, &d1, &d2, &d3);

      store_u8x4_strided_x2(d + 0 * dst_stride, 4 * dst_stride, d0);
      store_u8x4_strided_x2(d + 1 * dst_stride, 4 * dst_stride, d1);
      store_u8x4_strided_x2(d + 2 * dst_stride, 4 * dst_stride, d2);
      store_u8x4_strided_x2(d + 3 * dst_stride, 4 * dst_stride, d3);

      s0 = s8;
      s1 = s9;
      s2 = s10;
      s3 = s11;
      s4 = s12;
      s5 = s13;

      d += 4;
      s += 8;
      width -= 4;
    } while (width > 0);

    dst += 8 * dst_stride;
    src += 8 * src_stride;
    h -= 8;
  } while (h > 0);
}

static inline void scale_plane_2_to_1_6tap(const uint8_t *src,
                                           const int src_stride, uint8_t *dst,
                                           const int dst_stride, const int w,
                                           const int h,
                                           const int16_t *const filter_ptr,
                                           uint8_t *const im_block) {
  assert(w > 0 && h > 0);
  const int im_h = 2 * h + SUBPEL_TAPS - 3;
  const int im_stride = (w + 3) & ~3;

  // All filter values are even, halve them to stay in 16-bit elements when
  // applying filter.
  const int16x8_t filters = vshrq_n_s16(vld1q_s16(filter_ptr), 1);

  const ptrdiff_t horiz_offset = SUBPEL_TAPS / 2 - 2;
  const ptrdiff_t vert_offset = (SUBPEL_TAPS / 2 - 2) * src_stride;

  scale_2_to_1_horiz_6tap(src - horiz_offset - vert_offset, src_stride, w, im_h,
                          im_block, im_stride, filters);

  scale_2_to_1_vert_6tap(im_block, im_stride, w, h, dst, dst_stride, filters);
}

static inline void scale_4_to_1_horiz_6tap(const uint8_t *src,
                                           const int src_stride, int w, int h,
                                           uint8_t *dst, const int dst_stride,
                                           const int16x8_t filters) {
  do {
    uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7;
    load_u8_8x8(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

    transpose_elems_u8_4x8(t0, t1, t2, t3, t4, t5, t6, t7, &t0, &t1, &t2, &t3);

    int16x8_t s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
    int16x8_t s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
    int16x8_t s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
    int16x8_t s3 = vreinterpretq_s16_u16(vmovl_u8(t3));

    const uint8_t *s = src + 4;
    uint8_t *d = dst;
    int width = w;

    do {
      uint8x8_t t8, t9, t10, t11;
      load_u8_8x8(s, src_stride, &t4, &t5, &t6, &t7, &t8, &t9, &t10, &t11);

      transpose_elems_inplace_u8_8x8(&t4, &t5, &t6, &t7, &t8, &t9, &t10, &t11);

      int16x8_t s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
      int16x8_t s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
      int16x8_t s6 = vreinterpretq_s16_u16(vmovl_u8(t6));
      int16x8_t s7 = vreinterpretq_s16_u16(vmovl_u8(t7));
      int16x8_t s8 = vreinterpretq_s16_u16(vmovl_u8(t8));
      int16x8_t s9 = vreinterpretq_s16_u16(vmovl_u8(t9));
      int16x8_t s10 = vreinterpretq_s16_u16(vmovl_u8(t10));
      int16x8_t s11 = vreinterpretq_s16_u16(vmovl_u8(t11));

      uint8x8_t d0 = scale_filter6_8(s0, s1, s2, s3, s4, s5, filters);
      uint8x8_t d1 = scale_filter6_8(s4, s5, s6, s7, s8, s9, filters);

      uint8x8x2_t d01 = vtrn_u8(d0, d1);

      store_u8x2_strided_x4(d + 0 * dst_stride, 2 * dst_stride, d01.val[0]);
      store_u8x2_strided_x4(d + 1 * dst_stride, 2 * dst_stride, d01.val[1]);

      s0 = s8;
      s1 = s9;
      s2 = s10;
      s3 = s11;

      d += 2;
      s += 8;
      width -= 2;
    } while (width > 0);

    dst += 8 * dst_stride;
    src += 8 * src_stride;
    h -= 8;
  } while (h > 0);
}

static inline void scale_plane_4_to_1_6tap(const uint8_t *src,
                                           const int src_stride, uint8_t *dst,
                                           const int dst_stride, const int w,
                                           const int h,
                                           const int16_t *const filter_ptr,
                                           uint8_t *const im_block) {
  assert(w > 0 && h > 0);
  const int im_h = 4 * h + SUBPEL_TAPS - 3;
  const int im_stride = (w + 1) & ~1;
  // All filter values are even, halve them to stay in 16-bit elements when
  // applying filter.
  const int16x8_t filters = vshrq_n_s16(vld1q_s16(filter_ptr), 1);

  const ptrdiff_t horiz_offset = SUBPEL_TAPS / 2 - 2;
  const ptrdiff_t vert_offset = (SUBPEL_TAPS / 2 - 2) * src_stride;

  scale_4_to_1_horiz_6tap(src - horiz_offset - vert_offset, src_stride, w, im_h,
                          im_block, im_stride, filters);

  scale_4_to_1_vert_6tap(im_block, im_stride, w, h, dst, dst_stride, filters);
}

static inline uint8x8_t scale_filter_bilinear(const uint8x8_t *const s,
                                              const uint8x8_t *const coef) {
  const uint16x8_t h0 = vmull_u8(s[0], coef[0]);
  const uint16x8_t h1 = vmlal_u8(h0, s[1], coef[1]);

  return vrshrn_n_u16(h1, 7);
}

// Notes for 4 to 3 scaling:
//
// 1. 6 rows are calculated in each horizontal inner loop, so width_hor must be
// multiple of 6, and no less than w.
//
// 2. 8 rows are calculated in each vertical inner loop, so width_ver must be
// multiple of 8, and no less than w.
//
// 3. 8 columns are calculated in each horizontal inner loop for further
// vertical scaling, so height_hor must be multiple of 8, and no less than
// 4 * h / 3.
//
// 4. 6 columns are calculated in each vertical inner loop, so height_ver must
// be multiple of 6, and no less than h.
//
// 5. The physical location of the last row of the 4 to 3 scaled frame is
// decided by phase_scaler, and are always less than 1 pixel below the last row
// of the original image.
static inline void scale_plane_4_to_3_bilinear(
    const uint8_t *src, const int src_stride, uint8_t *dst,
    const int dst_stride, const int w, const int h, const int phase_scaler,
    uint8_t *const temp_buffer) {
  static const int step_q4 = 16 * 4 / 3;
  const int width_hor = (w + 5) - ((w + 5) % 6);
  const int stride_hor = width_hor + 2;  // store 2 extra pixels
  const int width_ver = (w + 7) & ~7;
  // We only need 1 extra row below because there are only 2 bilinear
  // coefficients.
  const int height_hor = (4 * h / 3 + 1 + 7) & ~7;
  const int height_ver = (h + 5) - ((h + 5) % 6);
  int x, y = height_hor;
  uint8_t *t = temp_buffer;
  uint8x8_t s[9], d[8], c[6];
  const InterpKernel *interp_kernel =
      (const InterpKernel *)av1_interp_filter_params_list[BILINEAR].filter_ptr;
  assert(w && h);

  c[0] = vdup_n_u8((uint8_t)interp_kernel[phase_scaler][3]);
  c[1] = vdup_n_u8((uint8_t)interp_kernel[phase_scaler][4]);
  c[2] = vdup_n_u8(
      (uint8_t)interp_kernel[(phase_scaler + 1 * step_q4) & SUBPEL_MASK][3]);
  c[3] = vdup_n_u8(
      (uint8_t)interp_kernel[(phase_scaler + 1 * step_q4) & SUBPEL_MASK][4]);
  c[4] = vdup_n_u8(
      (uint8_t)interp_kernel[(phase_scaler + 2 * step_q4) & SUBPEL_MASK][3]);
  c[5] = vdup_n_u8(
      (uint8_t)interp_kernel[(phase_scaler + 2 * step_q4) & SUBPEL_MASK][4]);

  d[6] = vdup_n_u8(0);
  d[7] = vdup_n_u8(0);

  // horizontal 6x8
  do {
    load_u8_8x8(src, src_stride, &s[0], &s[1], &s[2], &s[3], &s[4], &s[5],
                &s[6], &s[7]);
    src += 1;
    transpose_elems_inplace_u8_8x8(&s[0], &s[1], &s[2], &s[3], &s[4], &s[5],
                                   &s[6], &s[7]);
    x = width_hor;

    do {
      load_u8_8x8(src, src_stride, &s[1], &s[2], &s[3], &s[4], &s[5], &s[6],
                  &s[7], &s[8]);
      src += 8;
      transpose_elems_inplace_u8_8x8(&s[1], &s[2], &s[3], &s[4], &s[5], &s[6],
                                     &s[7], &s[8]);

      // 00 10 20 30 40 50 60 70
      // 01 11 21 31 41 51 61 71
      // 02 12 22 32 42 52 62 72
      // 03 13 23 33 43 53 63 73
      // 04 14 24 34 44 54 64 74
      // 05 15 25 35 45 55 65 75
      d[0] = scale_filter_bilinear(&s[0], &c[0]);
      d[1] =
          scale_filter_bilinear(&s[(phase_scaler + 1 * step_q4) >> 4], &c[2]);
      d[2] =
          scale_filter_bilinear(&s[(phase_scaler + 2 * step_q4) >> 4], &c[4]);
      d[3] = scale_filter_bilinear(&s[4], &c[0]);
      d[4] = scale_filter_bilinear(&s[4 + ((phase_scaler + 1 * step_q4) >> 4)],
                                   &c[2]);
      d[5] = scale_filter_bilinear(&s[4 + ((phase_scaler + 2 * step_q4) >> 4)],
                                   &c[4]);

      // 00 01 02 03 04 05 xx xx
      // 10 11 12 13 14 15 xx xx
      // 20 21 22 23 24 25 xx xx
      // 30 31 32 33 34 35 xx xx
      // 40 41 42 43 44 45 xx xx
      // 50 51 52 53 54 55 xx xx
      // 60 61 62 63 64 65 xx xx
      // 70 71 72 73 74 75 xx xx
      transpose_elems_inplace_u8_8x8(&d[0], &d[1], &d[2], &d[3], &d[4], &d[5],
                                     &d[6], &d[7]);
      // store 2 extra pixels
      vst1_u8(t + 0 * stride_hor, d[0]);
      vst1_u8(t + 1 * stride_hor, d[1]);
      vst1_u8(t + 2 * stride_hor, d[2]);
      vst1_u8(t + 3 * stride_hor, d[3]);
      vst1_u8(t + 4 * stride_hor, d[4]);
      vst1_u8(t + 5 * stride_hor, d[5]);
      vst1_u8(t + 6 * stride_hor, d[6]);
      vst1_u8(t + 7 * stride_hor, d[7]);

      s[0] = s[8];

      t += 6;
      x -= 6;
    } while (x);
    src += 8 * src_stride - 4 * width_hor / 3 - 1;
    t += 7 * stride_hor + 2;
    y -= 8;
  } while (y);

  // vertical 8x6
  x = width_ver;
  t = temp_buffer;
  do {
    load_u8_8x8(t, stride_hor, &s[0], &s[1], &s[2], &s[3], &s[4], &s[5], &s[6],
                &s[7]);
    t += stride_hor;
    y = height_ver;

    do {
      load_u8_8x8(t, stride_hor, &s[1], &s[2], &s[3], &s[4], &s[5], &s[6],
                  &s[7], &s[8]);
      t += 8 * stride_hor;

      d[0] = scale_filter_bilinear(&s[0], &c[0]);
      d[1] =
          scale_filter_bilinear(&s[(phase_scaler + 1 * step_q4) >> 4], &c[2]);
      d[2] =
          scale_filter_bilinear(&s[(phase_scaler + 2 * step_q4) >> 4], &c[4]);
      d[3] = scale_filter_bilinear(&s[4], &c[0]);
      d[4] = scale_filter_bilinear(&s[4 + ((phase_scaler + 1 * step_q4) >> 4)],
                                   &c[2]);
      d[5] = scale_filter_bilinear(&s[4 + ((phase_scaler + 2 * step_q4) >> 4)],
                                   &c[4]);
      vst1_u8(dst + 0 * dst_stride, d[0]);
      vst1_u8(dst + 1 * dst_stride, d[1]);
      vst1_u8(dst + 2 * dst_stride, d[2]);
      vst1_u8(dst + 3 * dst_stride, d[3]);
      vst1_u8(dst + 4 * dst_stride, d[4]);
      vst1_u8(dst + 5 * dst_stride, d[5]);

      s[0] = s[8];

      dst += 6 * dst_stride;
      y -= 6;
    } while (y);
    t -= stride_hor * (4 * height_ver / 3 + 1);
    t += 8;
    dst -= height_ver * dst_stride;
    dst += 8;
    x -= 8;
  } while (x);
}

static inline uint8x8_t scale_filter_8(const uint8x8_t *const s,
                                       const int16x8_t filter) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);

  int16x8_t ss0 = vreinterpretq_s16_u16(vmovl_u8(s[0]));
  int16x8_t ss1 = vreinterpretq_s16_u16(vmovl_u8(s[1]));
  int16x8_t ss2 = vreinterpretq_s16_u16(vmovl_u8(s[2]));
  int16x8_t ss3 = vreinterpretq_s16_u16(vmovl_u8(s[3]));
  int16x8_t ss4 = vreinterpretq_s16_u16(vmovl_u8(s[4]));
  int16x8_t ss5 = vreinterpretq_s16_u16(vmovl_u8(s[5]));
  int16x8_t ss6 = vreinterpretq_s16_u16(vmovl_u8(s[6]));
  int16x8_t ss7 = vreinterpretq_s16_u16(vmovl_u8(s[7]));

  int16x8_t sum = vmulq_lane_s16(ss0, filter_lo, 0);
  sum = vmlaq_lane_s16(sum, ss1, filter_lo, 1);
  sum = vmlaq_lane_s16(sum, ss2, filter_lo, 2);
  sum = vmlaq_lane_s16(sum, ss5, filter_hi, 1);
  sum = vmlaq_lane_s16(sum, ss6, filter_hi, 2);
  sum = vmlaq_lane_s16(sum, ss7, filter_hi, 3);
  sum = vqaddq_s16(sum, vmulq_lane_s16(ss3, filter_lo, 3));
  sum = vqaddq_s16(sum, vmulq_lane_s16(ss4, filter_hi, 0));

  return vqrshrun_n_s16(sum, FILTER_BITS);
}

static inline void scale_plane_4_to_3_8tap(const uint8_t *src,
                                           const int src_stride, uint8_t *dst,
                                           const int dst_stride, const int w,
                                           const int h,
                                           const InterpKernel *const coef,
                                           const int phase_scaler,
                                           uint8_t *const temp_buffer) {
  static const int step_q4 = 16 * 4 / 3;
  const int width_hor = (w + 5) - ((w + 5) % 6);
  const int stride_hor = width_hor + 2;  // store 2 extra pixels
  const int width_ver = (w + 7) & ~7;
  // We need (SUBPEL_TAPS - 1) extra rows: (SUBPEL_TAPS / 2 - 1) extra rows
  // above and (SUBPEL_TAPS / 2) extra rows below.
  const int height_hor = (4 * h / 3 + SUBPEL_TAPS - 1 + 7) & ~7;
  const int height_ver = (h + 5) - ((h + 5) % 6);
  const int16x8_t filters0 = vld1q_s16(
      (const int16_t *)&coef[(phase_scaler + 0 * step_q4) & SUBPEL_MASK]);
  const int16x8_t filters1 = vld1q_s16(
      (const int16_t *)&coef[(phase_scaler + 1 * step_q4) & SUBPEL_MASK]);
  const int16x8_t filters2 = vld1q_s16(
      (const int16_t *)&coef[(phase_scaler + 2 * step_q4) & SUBPEL_MASK]);
  int x, y = height_hor;
  uint8_t *t = temp_buffer;
  uint8x8_t s[15], d[8];

  assert(w > 0 && h > 0);

  src -= (SUBPEL_TAPS / 2 - 1) * src_stride + SUBPEL_TAPS / 2;
  d[6] = vdup_n_u8(0);
  d[7] = vdup_n_u8(0);

  // horizontal 6x8
  do {
    load_u8_8x8(src + 1, src_stride, &s[0], &s[1], &s[2], &s[3], &s[4], &s[5],
                &s[6], &s[7]);
    transpose_elems_inplace_u8_8x8(&s[0], &s[1], &s[2], &s[3], &s[4], &s[5],
                                   &s[6], &s[7]);
    x = width_hor;

    do {
      src += 8;
      load_u8_8x8(src, src_stride, &s[7], &s[8], &s[9], &s[10], &s[11], &s[12],
                  &s[13], &s[14]);
      transpose_elems_inplace_u8_8x8(&s[7], &s[8], &s[9], &s[10], &s[11],
                                     &s[12], &s[13], &s[14]);

      // 00 10 20 30 40 50 60 70
      // 01 11 21 31 41 51 61 71
      // 02 12 22 32 42 52 62 72
      // 03 13 23 33 43 53 63 73
      // 04 14 24 34 44 54 64 74
      // 05 15 25 35 45 55 65 75
      d[0] = scale_filter_8(&s[0], filters0);
      d[1] = scale_filter_8(&s[(phase_scaler + 1 * step_q4) >> 4], filters1);
      d[2] = scale_filter_8(&s[(phase_scaler + 2 * step_q4) >> 4], filters2);
      d[3] = scale_filter_8(&s[4], filters0);
      d[4] =
          scale_filter_8(&s[4 + ((phase_scaler + 1 * step_q4) >> 4)], filters1);
      d[5] =
          scale_filter_8(&s[4 + ((phase_scaler + 2 * step_q4) >> 4)], filters2);

      // 00 01 02 03 04 05 xx xx
      // 10 11 12 13 14 15 xx xx
      // 20 21 22 23 24 25 xx xx
      // 30 31 32 33 34 35 xx xx
      // 40 41 42 43 44 45 xx xx
      // 50 51 52 53 54 55 xx xx
      // 60 61 62 63 64 65 xx xx
      // 70 71 72 73 74 75 xx xx
      transpose_elems_inplace_u8_8x8(&d[0], &d[1], &d[2], &d[3], &d[4], &d[5],
                                     &d[6], &d[7]);
      // store 2 extra pixels
      vst1_u8(t + 0 * stride_hor, d[0]);
      vst1_u8(t + 1 * stride_hor, d[1]);
      vst1_u8(t + 2 * stride_hor, d[2]);
      vst1_u8(t + 3 * stride_hor, d[3]);
      vst1_u8(t + 4 * stride_hor, d[4]);
      vst1_u8(t + 5 * stride_hor, d[5]);
      vst1_u8(t + 6 * stride_hor, d[6]);
      vst1_u8(t + 7 * stride_hor, d[7]);

      s[0] = s[8];
      s[1] = s[9];
      s[2] = s[10];
      s[3] = s[11];
      s[4] = s[12];
      s[5] = s[13];
      s[6] = s[14];

      t += 6;
      x -= 6;
    } while (x);
    src += 8 * src_stride - 4 * width_hor / 3;
    t += 7 * stride_hor + 2;
    y -= 8;
  } while (y);

  // vertical 8x6
  x = width_ver;
  t = temp_buffer;
  do {
    load_u8_8x8(t, stride_hor, &s[0], &s[1], &s[2], &s[3], &s[4], &s[5], &s[6],
                &s[7]);
    t += 7 * stride_hor;
    y = height_ver;

    do {
      load_u8_8x8(t, stride_hor, &s[7], &s[8], &s[9], &s[10], &s[11], &s[12],
                  &s[13], &s[14]);
      t += 8 * stride_hor;

      d[0] = scale_filter_8(&s[0], filters0);
      d[1] = scale_filter_8(&s[(phase_scaler + 1 * step_q4) >> 4], filters1);
      d[2] = scale_filter_8(&s[(phase_scaler + 2 * step_q4) >> 4], filters2);
      d[3] = scale_filter_8(&s[4], filters0);
      d[4] =
          scale_filter_8(&s[4 + ((phase_scaler + 1 * step_q4) >> 4)], filters1);
      d[5] =
          scale_filter_8(&s[4 + ((phase_scaler + 2 * step_q4) >> 4)], filters2);
      vst1_u8(dst + 0 * dst_stride, d[0]);
      vst1_u8(dst + 1 * dst_stride, d[1]);
      vst1_u8(dst + 2 * dst_stride, d[2]);
      vst1_u8(dst + 3 * dst_stride, d[3]);
      vst1_u8(dst + 4 * dst_stride, d[4]);
      vst1_u8(dst + 5 * dst_stride, d[5]);

      s[0] = s[8];
      s[1] = s[9];
      s[2] = s[10];
      s[3] = s[11];
      s[4] = s[12];
      s[5] = s[13];
      s[6] = s[14];

      dst += 6 * dst_stride;
      y -= 6;
    } while (y);
    t -= stride_hor * (4 * height_ver / 3 + 7);
    t += 8;
    dst -= height_ver * dst_stride;
    dst += 8;
    x -= 8;
  } while (x);
}

// There's SIMD optimizations for 1/4, 1/2 and 3/4 downscaling in NEON.
static inline bool has_normative_scaler_neon(const int src_width,
                                             const int src_height,
                                             const int dst_width,
                                             const int dst_height) {
  const bool has_normative_scaler =
      (2 * dst_width == src_width && 2 * dst_height == src_height) ||
      (4 * dst_width == src_width && 4 * dst_height == src_height) ||
      (4 * dst_width == 3 * src_width && 4 * dst_height == 3 * src_height);

  return has_normative_scaler;
}

void av1_resize_and_extend_frame_neon(const YV12_BUFFER_CONFIG *src,
                                      YV12_BUFFER_CONFIG *dst,
                                      const InterpFilter filter,
                                      const int phase, const int num_planes) {
  assert(filter == BILINEAR || filter == EIGHTTAP_SMOOTH ||
         filter == EIGHTTAP_REGULAR);

  bool has_normative_scaler =
      has_normative_scaler_neon(src->y_crop_width, src->y_crop_height,
                                dst->y_crop_width, dst->y_crop_height);

  if (num_planes > 1) {
    has_normative_scaler =
        has_normative_scaler &&
        has_normative_scaler_neon(src->uv_crop_width, src->uv_crop_height,
                                  dst->uv_crop_width, dst->uv_crop_height);
  }

  if (!has_normative_scaler) {
    av1_resize_and_extend_frame_c(src, dst, filter, phase, num_planes);
    return;
  }

  // We use AOMMIN(num_planes, MAX_MB_PLANE) instead of num_planes to quiet
  // the static analysis warnings.
  int malloc_failed = 0;
  for (int i = 0; i < AOMMIN(num_planes, MAX_MB_PLANE); ++i) {
    const int is_uv = i > 0;
    const int src_w = src->crop_widths[is_uv];
    const int src_h = src->crop_heights[is_uv];
    const int dst_w = dst->crop_widths[is_uv];
    const int dst_h = dst->crop_heights[is_uv];
    const int dst_y_w = (dst->crop_widths[0] + 1) & ~1;
    const int dst_y_h = (dst->crop_heights[0] + 1) & ~1;

    if (2 * dst_w == src_w && 2 * dst_h == src_h) {
      if (phase == 0) {
        scale_plane_2_to_1_phase_0(src->buffers[i], src->strides[is_uv],
                                   dst->buffers[i], dst->strides[is_uv], dst_w,
                                   dst_h);
      } else if (filter == BILINEAR) {
        const int16_t c0 = av1_bilinear_filters[phase][3];
        const int16_t c1 = av1_bilinear_filters[phase][4];
        scale_plane_2_to_1_bilinear(src->buffers[i], src->strides[is_uv],
                                    dst->buffers[i], dst->strides[is_uv], dst_w,
                                    dst_h, c0, c1);
      } else {
        const int buffer_stride = (dst_y_w + 3) & ~3;
        const int buffer_height = (2 * dst_y_h + SUBPEL_TAPS - 2 + 7) & ~7;
        uint8_t *const temp_buffer =
            (uint8_t *)malloc(buffer_stride * buffer_height);
        if (!temp_buffer) {
          malloc_failed = 1;
          break;
        }
        const InterpKernel *interp_kernel =
            (const InterpKernel *)av1_interp_filter_params_list[filter]
                .filter_ptr;
        scale_plane_2_to_1_6tap(src->buffers[i], src->strides[is_uv],
                                dst->buffers[i], dst->strides[is_uv], dst_w,
                                dst_h, interp_kernel[phase], temp_buffer);
        free(temp_buffer);
      }
    } else if (4 * dst_w == src_w && 4 * dst_h == src_h) {
      if (phase == 0) {
        scale_plane_4_to_1_phase_0(src->buffers[i], src->strides[is_uv],
                                   dst->buffers[i], dst->strides[is_uv], dst_w,
                                   dst_h);
      } else if (filter == BILINEAR) {
        const int16_t c0 = av1_bilinear_filters[phase][3];
        const int16_t c1 = av1_bilinear_filters[phase][4];
        scale_plane_4_to_1_bilinear(src->buffers[i], src->strides[is_uv],
                                    dst->buffers[i], dst->strides[is_uv], dst_w,
                                    dst_h, c0, c1);
      } else {
        const int buffer_stride = (dst_y_w + 1) & ~1;
        const int buffer_height = (4 * dst_y_h + SUBPEL_TAPS - 2 + 7) & ~7;
        uint8_t *const temp_buffer =
            (uint8_t *)malloc(buffer_stride * buffer_height);
        if (!temp_buffer) {
          malloc_failed = 1;
          break;
        }
        const InterpKernel *interp_kernel =
            (const InterpKernel *)av1_interp_filter_params_list[filter]
                .filter_ptr;
        scale_plane_4_to_1_6tap(src->buffers[i], src->strides[is_uv],
                                dst->buffers[i], dst->strides[is_uv], dst_w,
                                dst_h, interp_kernel[phase], temp_buffer);
        free(temp_buffer);
      }
    } else {
      assert(4 * dst_w == 3 * src_w && 4 * dst_h == 3 * src_h);
      // 4 to 3
      const int buffer_stride = (dst_y_w + 5) - ((dst_y_w + 5) % 6) + 2;
      const int buffer_height = (4 * dst_y_h / 3 + SUBPEL_TAPS - 1 + 7) & ~7;
      uint8_t *const temp_buffer =
          (uint8_t *)malloc(buffer_stride * buffer_height);
      if (!temp_buffer) {
        malloc_failed = 1;
        break;
      }
      if (filter == BILINEAR) {
        scale_plane_4_to_3_bilinear(src->buffers[i], src->strides[is_uv],
                                    dst->buffers[i], dst->strides[is_uv], dst_w,
                                    dst_h, phase, temp_buffer);
      } else {
        const InterpKernel *interp_kernel =
            (const InterpKernel *)av1_interp_filter_params_list[filter]
                .filter_ptr;
        scale_plane_4_to_3_8tap(src->buffers[i], src->strides[is_uv],
                                dst->buffers[i], dst->strides[is_uv], dst_w,
                                dst_h, interp_kernel, phase, temp_buffer);
      }
      free(temp_buffer);
    }
  }

  if (malloc_failed) {
    av1_resize_and_extend_frame_c(src, dst, filter, phase, num_planes);
  } else {
    aom_extend_frame_borders(dst, num_planes);
  }
}
