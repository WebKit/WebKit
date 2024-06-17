/*
 *
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
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
#include "av1/common/resize.h"
#include "config/av1_rtcd.h"
#include "config/aom_dsp_rtcd.h"
#include "config/aom_scale_rtcd.h"

static INLINE int16x4_t convolve8_4(const int16x4_t s0, const int16x4_t s1,
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
  return sum;
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
  return vqrshrun_n_s16(sum, 7);
}

static INLINE uint8x8_t scale_filter_8(const uint8x8_t *const s,
                                       const int16x8_t filter) {
  int16x8_t ss0 = vreinterpretq_s16_u16(vmovl_u8(s[0]));
  int16x8_t ss1 = vreinterpretq_s16_u16(vmovl_u8(s[1]));
  int16x8_t ss2 = vreinterpretq_s16_u16(vmovl_u8(s[2]));
  int16x8_t ss3 = vreinterpretq_s16_u16(vmovl_u8(s[3]));
  int16x8_t ss4 = vreinterpretq_s16_u16(vmovl_u8(s[4]));
  int16x8_t ss5 = vreinterpretq_s16_u16(vmovl_u8(s[5]));
  int16x8_t ss6 = vreinterpretq_s16_u16(vmovl_u8(s[6]));
  int16x8_t ss7 = vreinterpretq_s16_u16(vmovl_u8(s[7]));

  return convolve8_8(ss0, ss1, ss2, ss3, ss4, ss5, ss6, ss7, filter);
}

static INLINE void scale_plane_2_to_1_phase_0(const uint8_t *src,
                                              const int src_stride,
                                              uint8_t *dst,
                                              const int dst_stride, const int w,
                                              const int h) {
  const int max_width = (w + 15) & ~15;
  int y = h;

  assert(w && h);

  do {
    int x = max_width;
    do {
      const uint8x16x2_t s = vld2q_u8(src);
      vst1q_u8(dst, s.val[0]);
      src += 32;
      dst += 16;
      x -= 16;
    } while (x);
    src += 2 * (src_stride - max_width);
    dst += dst_stride - max_width;
  } while (--y);
}

static INLINE void scale_plane_4_to_1_phase_0(const uint8_t *src,
                                              const int src_stride,
                                              uint8_t *dst,
                                              const int dst_stride, const int w,
                                              const int h) {
  const int max_width = (w + 15) & ~15;
  int y = h;

  assert(w && h);

  do {
    int x = max_width;
    do {
      const uint8x16x4_t s = vld4q_u8(src);
      vst1q_u8(dst, s.val[0]);
      src += 64;
      dst += 16;
      x -= 16;
    } while (x);
    src += 4 * (src_stride - max_width);
    dst += dst_stride - max_width;
  } while (--y);
}

static INLINE void scale_plane_bilinear_kernel(
    const uint8x16_t in0, const uint8x16_t in1, const uint8x16_t in2,
    const uint8x16_t in3, const uint8x8_t coef0, const uint8x8_t coef1,
    uint8_t *const dst) {
  const uint16x8_t h0 = vmull_u8(vget_low_u8(in0), coef0);
  const uint16x8_t h1 = vmull_u8(vget_high_u8(in0), coef0);
  const uint16x8_t h2 = vmull_u8(vget_low_u8(in2), coef0);
  const uint16x8_t h3 = vmull_u8(vget_high_u8(in2), coef0);
  const uint16x8_t h4 = vmlal_u8(h0, vget_low_u8(in1), coef1);
  const uint16x8_t h5 = vmlal_u8(h1, vget_high_u8(in1), coef1);
  const uint16x8_t h6 = vmlal_u8(h2, vget_low_u8(in3), coef1);
  const uint16x8_t h7 = vmlal_u8(h3, vget_high_u8(in3), coef1);

  const uint8x8_t hor0 = vrshrn_n_u16(h4, 7);  // temp: 00 01 02 03 04 05 06 07
  const uint8x8_t hor1 = vrshrn_n_u16(h5, 7);  // temp: 08 09 0A 0B 0C 0D 0E 0F
  const uint8x8_t hor2 = vrshrn_n_u16(h6, 7);  // temp: 10 11 12 13 14 15 16 17
  const uint8x8_t hor3 = vrshrn_n_u16(h7, 7);  // temp: 18 19 1A 1B 1C 1D 1E 1F
  const uint16x8_t v0 = vmull_u8(hor0, coef0);
  const uint16x8_t v1 = vmull_u8(hor1, coef0);
  const uint16x8_t v2 = vmlal_u8(v0, hor2, coef1);
  const uint16x8_t v3 = vmlal_u8(v1, hor3, coef1);
  // dst: 0 1 2 3 4 5 6 7  8 9 A B C D E F
  const uint8x16_t d = vcombine_u8(vrshrn_n_u16(v2, 7), vrshrn_n_u16(v3, 7));
  vst1q_u8(dst, d);
}

static INLINE void scale_plane_2_to_1_bilinear(
    const uint8_t *const src, const int src_stride, uint8_t *dst,
    const int dst_stride, const int w, const int h, const int16_t c0,
    const int16_t c1) {
  const int max_width = (w + 15) & ~15;
  const uint8_t *src0 = src;
  const uint8_t *src1 = src + src_stride;
  const uint8x8_t coef0 = vdup_n_u8(c0);
  const uint8x8_t coef1 = vdup_n_u8(c1);
  int y = h;

  assert(w && h);

  do {
    int x = max_width;
    do {
      // 000 002 004 006 008 00A 00C 00E  010 012 014 016 018 01A 01C 01E
      // 001 003 005 007 009 00B 00D 00F  011 013 015 017 019 01B 01D 01F
      const uint8x16x2_t s0 = vld2q_u8(src0);
      // 100 102 104 106 108 10A 10C 10E  110 112 114 116 118 11A 11C 11E
      // 101 103 105 107 109 10B 10D 10F  111 113 115 117 119 11B 11D 11F
      const uint8x16x2_t s1 = vld2q_u8(src1);
      scale_plane_bilinear_kernel(s0.val[0], s0.val[1], s1.val[0], s1.val[1],
                                  coef0, coef1, dst);
      src0 += 32;
      src1 += 32;
      dst += 16;
      x -= 16;
    } while (x);
    src0 += 2 * (src_stride - max_width);
    src1 += 2 * (src_stride - max_width);
    dst += dst_stride - max_width;
  } while (--y);
}

static INLINE void scale_plane_4_to_1_bilinear(
    const uint8_t *const src, const int src_stride, uint8_t *dst,
    const int dst_stride, const int w, const int h, const int16_t c0,
    const int16_t c1) {
  const int max_width = (w + 15) & ~15;
  const uint8_t *src0 = src;
  const uint8_t *src1 = src + src_stride;
  const uint8x8_t coef0 = vdup_n_u8(c0);
  const uint8x8_t coef1 = vdup_n_u8(c1);
  int y = h;

  assert(w && h);

  do {
    int x = max_width;
    do {
      // (*) -- useless
      // 000 004 008 00C 010 014 018 01C  020 024 028 02C 030 034 038 03C
      // 001 005 009 00D 011 015 019 01D  021 025 029 02D 031 035 039 03D
      // 002 006 00A 00E 012 016 01A 01E  022 026 02A 02E 032 036 03A 03E (*)
      // 003 007 00B 00F 013 017 01B 01F  023 027 02B 02F 033 037 03B 03F (*)
      const uint8x16x4_t s0 = vld4q_u8(src0);
      // 100 104 108 10C 110 114 118 11C  120 124 128 12C 130 134 138 13C
      // 101 105 109 10D 111 115 119 11D  121 125 129 12D 131 135 139 13D
      // 102 106 10A 10E 112 116 11A 11E  122 126 12A 12E 132 136 13A 13E (*)
      // 103 107 10B 10F 113 117 11B 11F  123 127 12B 12F 133 137 13B 13F (*)
      const uint8x16x4_t s1 = vld4q_u8(src1);
      scale_plane_bilinear_kernel(s0.val[0], s0.val[1], s1.val[0], s1.val[1],
                                  coef0, coef1, dst);
      src0 += 64;
      src1 += 64;
      dst += 16;
      x -= 16;
    } while (x);
    src0 += 4 * (src_stride - max_width);
    src1 += 4 * (src_stride - max_width);
    dst += dst_stride - max_width;
  } while (--y);
}

static void scale_plane_2_to_1_general(const uint8_t *src, const int src_stride,
                                       uint8_t *dst, const int dst_stride,
                                       const int w, const int h,
                                       const int16_t *const coef,
                                       uint8_t *const temp_buffer) {
  const int width_hor = (w + 3) & ~3;
  const int width_ver = (w + 7) & ~7;
  const int height_hor = (2 * h + SUBPEL_TAPS - 2 + 7) & ~7;
  const int height_ver = (h + 3) & ~3;
  const int16x8_t filters = vld1q_s16(coef);
  int x, y = height_hor;
  uint8_t *t = temp_buffer;
  uint8x8_t s[14], d[4];

  assert(w && h);

  src -= (SUBPEL_TAPS / 2 - 1) * src_stride + SUBPEL_TAPS / 2 + 1;

  // horizontal 4x8
  // Note: processing 4x8 is about 20% faster than processing row by row using
  // vld4_u8().
  do {
    load_u8_8x8(src + 2, src_stride, &s[0], &s[1], &s[2], &s[3], &s[4], &s[5],
                &s[6], &s[7]);
    transpose_elems_inplace_u8_8x8(&s[0], &s[1], &s[2], &s[3], &s[4], &s[5],
                                   &s[6], &s[7]);
    x = width_hor;

    do {
      src += 8;
      load_u8_8x8(src, src_stride, &s[6], &s[7], &s[8], &s[9], &s[10], &s[11],
                  &s[12], &s[13]);
      transpose_elems_inplace_u8_8x8(&s[6], &s[7], &s[8], &s[9], &s[10], &s[11],
                                     &s[12], &s[13]);

      d[0] = scale_filter_8(&s[0], filters);  // 00 10 20 30 40 50 60 70
      d[1] = scale_filter_8(&s[2], filters);  // 01 11 21 31 41 51 61 71
      d[2] = scale_filter_8(&s[4], filters);  // 02 12 22 32 42 52 62 72
      d[3] = scale_filter_8(&s[6], filters);  // 03 13 23 33 43 53 63 73
      // 00 01 02 03 40 41 42 43
      // 10 11 12 13 50 51 52 53
      // 20 21 22 23 60 61 62 63
      // 30 31 32 33 70 71 72 73
      transpose_elems_inplace_u8_8x4(&d[0], &d[1], &d[2], &d[3]);
      vst1_lane_u32((uint32_t *)(t + 0 * width_hor), vreinterpret_u32_u8(d[0]),
                    0);
      vst1_lane_u32((uint32_t *)(t + 1 * width_hor), vreinterpret_u32_u8(d[1]),
                    0);
      vst1_lane_u32((uint32_t *)(t + 2 * width_hor), vreinterpret_u32_u8(d[2]),
                    0);
      vst1_lane_u32((uint32_t *)(t + 3 * width_hor), vreinterpret_u32_u8(d[3]),
                    0);
      vst1_lane_u32((uint32_t *)(t + 4 * width_hor), vreinterpret_u32_u8(d[0]),
                    1);
      vst1_lane_u32((uint32_t *)(t + 5 * width_hor), vreinterpret_u32_u8(d[1]),
                    1);
      vst1_lane_u32((uint32_t *)(t + 6 * width_hor), vreinterpret_u32_u8(d[2]),
                    1);
      vst1_lane_u32((uint32_t *)(t + 7 * width_hor), vreinterpret_u32_u8(d[3]),
                    1);

      s[0] = s[8];
      s[1] = s[9];
      s[2] = s[10];
      s[3] = s[11];
      s[4] = s[12];
      s[5] = s[13];

      t += 4;
      x -= 4;
    } while (x);
    src += 8 * src_stride - 2 * width_hor;
    t += 7 * width_hor;
    y -= 8;
  } while (y);

  // vertical 8x4
  x = width_ver;
  t = temp_buffer;
  do {
    load_u8_8x8(t, width_hor, &s[0], &s[1], &s[2], &s[3], &s[4], &s[5], &s[6],
                &s[7]);
    t += 6 * width_hor;
    y = height_ver;

    do {
      load_u8_8x8(t, width_hor, &s[6], &s[7], &s[8], &s[9], &s[10], &s[11],
                  &s[12], &s[13]);
      t += 8 * width_hor;

      d[0] = scale_filter_8(&s[0], filters);  // 00 01 02 03 04 05 06 07
      d[1] = scale_filter_8(&s[2], filters);  // 10 11 12 13 14 15 16 17
      d[2] = scale_filter_8(&s[4], filters);  // 20 21 22 23 24 25 26 27
      d[3] = scale_filter_8(&s[6], filters);  // 30 31 32 33 34 35 36 37
      vst1_u8(dst + 0 * dst_stride, d[0]);
      vst1_u8(dst + 1 * dst_stride, d[1]);
      vst1_u8(dst + 2 * dst_stride, d[2]);
      vst1_u8(dst + 3 * dst_stride, d[3]);

      s[0] = s[8];
      s[1] = s[9];
      s[2] = s[10];
      s[3] = s[11];
      s[4] = s[12];
      s[5] = s[13];

      dst += 4 * dst_stride;
      y -= 4;
    } while (y);
    t -= width_hor * (2 * height_ver + 6);
    t += 8;
    dst -= height_ver * dst_stride;
    dst += 8;
    x -= 8;
  } while (x);
}

static void scale_plane_4_to_1_general(const uint8_t *src, const int src_stride,
                                       uint8_t *dst, const int dst_stride,
                                       const int w, const int h,
                                       const int16_t *const coef,
                                       uint8_t *const temp_buffer) {
  const int width_hor = (w + 1) & ~1;
  const int width_ver = (w + 7) & ~7;
  const int height_hor = (4 * h + SUBPEL_TAPS - 2 + 7) & ~7;
  const int height_ver = (h + 1) & ~1;
  const int16x8_t filters = vld1q_s16(coef);
  int x, y = height_hor;
  uint8_t *t = temp_buffer;
  uint8x8_t s[12], d[2];

  assert(w && h);

  src -= (SUBPEL_TAPS / 2 - 1) * src_stride + SUBPEL_TAPS / 2 + 3;

  // horizontal 2x8
  // Note: processing 2x8 is about 20% faster than processing row by row using
  // vld4_u8().
  do {
    load_u8_8x8(src + 4, src_stride, &s[0], &s[1], &s[2], &s[3], &s[4], &s[5],
                &s[6], &s[7]);
    transpose_elems_u8_4x8(s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7],
                           &s[0], &s[1], &s[2], &s[3]);
    x = width_hor;

    do {
      uint8x8x2_t dd;
      src += 8;
      load_u8_8x8(src, src_stride, &s[4], &s[5], &s[6], &s[7], &s[8], &s[9],
                  &s[10], &s[11]);
      transpose_elems_inplace_u8_8x8(&s[4], &s[5], &s[6], &s[7], &s[8], &s[9],
                                     &s[10], &s[11]);

      d[0] = scale_filter_8(&s[0], filters);  // 00 10 20 30 40 50 60 70
      d[1] = scale_filter_8(&s[4], filters);  // 01 11 21 31 41 51 61 71
      // dd.val[0]: 00 01 20 21 40 41 60 61
      // dd.val[1]: 10 11 30 31 50 51 70 71
      dd = vtrn_u8(d[0], d[1]);
      vst1_lane_u16((uint16_t *)(t + 0 * width_hor),
                    vreinterpret_u16_u8(dd.val[0]), 0);
      vst1_lane_u16((uint16_t *)(t + 1 * width_hor),
                    vreinterpret_u16_u8(dd.val[1]), 0);
      vst1_lane_u16((uint16_t *)(t + 2 * width_hor),
                    vreinterpret_u16_u8(dd.val[0]), 1);
      vst1_lane_u16((uint16_t *)(t + 3 * width_hor),
                    vreinterpret_u16_u8(dd.val[1]), 1);
      vst1_lane_u16((uint16_t *)(t + 4 * width_hor),
                    vreinterpret_u16_u8(dd.val[0]), 2);
      vst1_lane_u16((uint16_t *)(t + 5 * width_hor),
                    vreinterpret_u16_u8(dd.val[1]), 2);
      vst1_lane_u16((uint16_t *)(t + 6 * width_hor),
                    vreinterpret_u16_u8(dd.val[0]), 3);
      vst1_lane_u16((uint16_t *)(t + 7 * width_hor),
                    vreinterpret_u16_u8(dd.val[1]), 3);

      s[0] = s[8];
      s[1] = s[9];
      s[2] = s[10];
      s[3] = s[11];

      t += 2;
      x -= 2;
    } while (x);
    src += 8 * src_stride - 4 * width_hor;
    t += 7 * width_hor;
    y -= 8;
  } while (y);

  // vertical 8x2
  x = width_ver;
  t = temp_buffer;
  do {
    load_u8_8x4(t, width_hor, &s[0], &s[1], &s[2], &s[3]);
    t += 4 * width_hor;
    y = height_ver;

    do {
      load_u8_8x8(t, width_hor, &s[4], &s[5], &s[6], &s[7], &s[8], &s[9],
                  &s[10], &s[11]);
      t += 8 * width_hor;

      d[0] = scale_filter_8(&s[0], filters);  // 00 01 02 03 04 05 06 07
      d[1] = scale_filter_8(&s[4], filters);  // 10 11 12 13 14 15 16 17
      vst1_u8(dst + 0 * dst_stride, d[0]);
      vst1_u8(dst + 1 * dst_stride, d[1]);

      s[0] = s[8];
      s[1] = s[9];
      s[2] = s[10];
      s[3] = s[11];

      dst += 2 * dst_stride;
      y -= 2;
    } while (y);
    t -= width_hor * (4 * height_ver + 4);
    t += 8;
    dst -= height_ver * dst_stride;
    dst += 8;
    x -= 8;
  } while (x);
}

static INLINE uint8x8_t scale_filter_bilinear(const uint8x8_t *const s,
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
static void scale_plane_4_to_3_bilinear(const uint8_t *src,
                                        const int src_stride, uint8_t *dst,
                                        const int dst_stride, const int w,
                                        const int h, const int phase_scaler,
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

static void scale_plane_4_to_3_general(const uint8_t *src, const int src_stride,
                                       uint8_t *dst, const int dst_stride,
                                       const int w, const int h,
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

  assert(w && h);

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
static INLINE bool has_normative_scaler_neon(const int src_width,
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
        scale_plane_2_to_1_general(src->buffers[i], src->strides[is_uv],
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
        scale_plane_4_to_1_general(src->buffers[i], src->strides[is_uv],
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
        scale_plane_4_to_3_general(src->buffers[i], src->strides[is_uv],
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

static INLINE void scaledconvolve_horiz_w4(
    const uint8_t *src, const ptrdiff_t src_stride, uint8_t *dst,
    const ptrdiff_t dst_stride, const InterpKernel *const x_filters,
    const int x0_q4, const int x_step_q4, const int w, const int h) {
  DECLARE_ALIGNED(16, uint8_t, temp[4 * 4]);
  int x, y, z;

  src -= SUBPEL_TAPS / 2 - 1;

  y = h;
  do {
    int x_q4 = x0_q4;
    x = 0;
    do {
      // process 4 src_x steps
      for (z = 0; z < 4; ++z) {
        const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
        if (x_q4 & SUBPEL_MASK) {
          const int16x8_t filters = vld1q_s16(x_filters[x_q4 & SUBPEL_MASK]);
          uint8x8_t s[8], d;
          int16x8_t ss[4];
          int16x4_t t[8], tt;

          load_u8_8x4(src_x, src_stride, &s[0], &s[1], &s[2], &s[3]);
          transpose_elems_inplace_u8_8x4(&s[0], &s[1], &s[2], &s[3]);

          ss[0] = vreinterpretq_s16_u16(vmovl_u8(s[0]));
          ss[1] = vreinterpretq_s16_u16(vmovl_u8(s[1]));
          ss[2] = vreinterpretq_s16_u16(vmovl_u8(s[2]));
          ss[3] = vreinterpretq_s16_u16(vmovl_u8(s[3]));
          t[0] = vget_low_s16(ss[0]);
          t[1] = vget_low_s16(ss[1]);
          t[2] = vget_low_s16(ss[2]);
          t[3] = vget_low_s16(ss[3]);
          t[4] = vget_high_s16(ss[0]);
          t[5] = vget_high_s16(ss[1]);
          t[6] = vget_high_s16(ss[2]);
          t[7] = vget_high_s16(ss[3]);

          tt = convolve8_4(t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7],
                           filters);
          d = vqrshrun_n_s16(vcombine_s16(tt, tt), 7);
          store_u8_4x1(&temp[4 * z], d);
        } else {
          int i;
          for (i = 0; i < 4; ++i) {
            temp[z * 4 + i] = src_x[i * src_stride + 3];
          }
        }
        x_q4 += x_step_q4;
      }

      // transpose the 4x4 filters values back to dst
      {
        const uint8x8x4_t d4 = vld4_u8(temp);
        store_u8_4x1(&dst[x + 0 * dst_stride], d4.val[0]);
        store_u8_4x1(&dst[x + 1 * dst_stride], d4.val[1]);
        store_u8_4x1(&dst[x + 2 * dst_stride], d4.val[2]);
        store_u8_4x1(&dst[x + 3 * dst_stride], d4.val[3]);
      }
      x += 4;
    } while (x < w);

    src += src_stride * 4;
    dst += dst_stride * 4;
    y -= 4;
  } while (y > 0);
}

static INLINE void scaledconvolve_horiz_w8(
    const uint8_t *src, const ptrdiff_t src_stride, uint8_t *dst,
    const ptrdiff_t dst_stride, const InterpKernel *const x_filters,
    const int x0_q4, const int x_step_q4, const int w, const int h) {
  DECLARE_ALIGNED(16, uint8_t, temp[8 * 8]);
  int x, y, z;
  src -= SUBPEL_TAPS / 2 - 1;

  // This function processes 8x8 areas. The intermediate height is not always
  // a multiple of 8, so force it to be a multiple of 8 here.
  y = (h + 7) & ~7;

  do {
    int x_q4 = x0_q4;
    x = 0;
    do {
      uint8x8_t d[8];
      // process 8 src_x steps
      for (z = 0; z < 8; ++z) {
        const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];

        if (x_q4 & SUBPEL_MASK) {
          const int16x8_t filters = vld1q_s16(x_filters[x_q4 & SUBPEL_MASK]);
          uint8x8_t s[8];
          load_u8_8x8(src_x, src_stride, &s[0], &s[1], &s[2], &s[3], &s[4],
                      &s[5], &s[6], &s[7]);
          transpose_elems_inplace_u8_8x8(&s[0], &s[1], &s[2], &s[3], &s[4],
                                         &s[5], &s[6], &s[7]);
          d[0] = scale_filter_8(s, filters);
          vst1_u8(&temp[8 * z], d[0]);
        } else {
          int i;
          for (i = 0; i < 8; ++i) {
            temp[z * 8 + i] = src_x[i * src_stride + 3];
          }
        }
        x_q4 += x_step_q4;
      }

      // transpose the 8x8 filters values back to dst
      load_u8_8x8(temp, 8, &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6],
                  &d[7]);
      transpose_elems_inplace_u8_8x8(&d[0], &d[1], &d[2], &d[3], &d[4], &d[5],
                                     &d[6], &d[7]);
      store_u8_8x8(dst + x, dst_stride, d[0], d[1], d[2], d[3], d[4], d[5],
                   d[6], d[7]);
      x += 8;
    } while (x < w);

    src += src_stride * 8;
    dst += dst_stride * 8;
  } while (y -= 8);
}

static INLINE void scaledconvolve_vert_w4(
    const uint8_t *src, const ptrdiff_t src_stride, uint8_t *dst,
    const ptrdiff_t dst_stride, const InterpKernel *const y_filters,
    const int y0_q4, const int y_step_q4, const int w, const int h) {
  int y;
  int y_q4 = y0_q4;

  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  y = h;
  do {
    const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];

    if (y_q4 & SUBPEL_MASK) {
      const int16x8_t filters = vld1q_s16(y_filters[y_q4 & SUBPEL_MASK]);
      uint8x8_t s[8], d;
      int16x4_t t[8], tt;

      load_u8_8x8(src_y, src_stride, &s[0], &s[1], &s[2], &s[3], &s[4], &s[5],
                  &s[6], &s[7]);
      t[0] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[0])));
      t[1] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[1])));
      t[2] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[2])));
      t[3] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[3])));
      t[4] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[4])));
      t[5] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[5])));
      t[6] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[6])));
      t[7] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[7])));

      tt = convolve8_4(t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7], filters);
      d = vqrshrun_n_s16(vcombine_s16(tt, tt), 7);
      store_u8_4x1(dst, d);
    } else {
      memcpy(dst, &src_y[3 * src_stride], w);
    }

    dst += dst_stride;
    y_q4 += y_step_q4;
  } while (--y);
}

static INLINE void scaledconvolve_vert_w8(
    const uint8_t *src, const ptrdiff_t src_stride, uint8_t *dst,
    const ptrdiff_t dst_stride, const InterpKernel *const y_filters,
    const int y0_q4, const int y_step_q4, const int w, const int h) {
  int y;
  int y_q4 = y0_q4;

  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  y = h;
  do {
    const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
    if (y_q4 & SUBPEL_MASK) {
      const int16x8_t filters = vld1q_s16(y_filters[y_q4 & SUBPEL_MASK]);
      uint8x8_t s[8], d;
      load_u8_8x8(src_y, src_stride, &s[0], &s[1], &s[2], &s[3], &s[4], &s[5],
                  &s[6], &s[7]);
      d = scale_filter_8(s, filters);
      vst1_u8(dst, d);
    } else {
      memcpy(dst, &src_y[3 * src_stride], w);
    }
    dst += dst_stride;
    y_q4 += y_step_q4;
  } while (--y);
}

static INLINE void scaledconvolve_vert_w16(
    const uint8_t *src, const ptrdiff_t src_stride, uint8_t *dst,
    const ptrdiff_t dst_stride, const InterpKernel *const y_filters,
    const int y0_q4, const int y_step_q4, const int w, const int h) {
  int x, y;
  int y_q4 = y0_q4;

  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  y = h;
  do {
    const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
    if (y_q4 & SUBPEL_MASK) {
      x = 0;
      do {
        const int16x8_t filters = vld1q_s16(y_filters[y_q4 & SUBPEL_MASK]);
        uint8x16_t ss[8];
        uint8x8_t s[8], d[2];
        load_u8_16x8(src_y, src_stride, &ss[0], &ss[1], &ss[2], &ss[3], &ss[4],
                     &ss[5], &ss[6], &ss[7]);
        s[0] = vget_low_u8(ss[0]);
        s[1] = vget_low_u8(ss[1]);
        s[2] = vget_low_u8(ss[2]);
        s[3] = vget_low_u8(ss[3]);
        s[4] = vget_low_u8(ss[4]);
        s[5] = vget_low_u8(ss[5]);
        s[6] = vget_low_u8(ss[6]);
        s[7] = vget_low_u8(ss[7]);
        d[0] = scale_filter_8(s, filters);

        s[0] = vget_high_u8(ss[0]);
        s[1] = vget_high_u8(ss[1]);
        s[2] = vget_high_u8(ss[2]);
        s[3] = vget_high_u8(ss[3]);
        s[4] = vget_high_u8(ss[4]);
        s[5] = vget_high_u8(ss[5]);
        s[6] = vget_high_u8(ss[6]);
        s[7] = vget_high_u8(ss[7]);
        d[1] = scale_filter_8(s, filters);
        vst1q_u8(&dst[x], vcombine_u8(d[0], d[1]));
        src_y += 16;
        x += 16;
      } while (x < w);
    } else {
      memcpy(dst, &src_y[3 * src_stride], w);
    }
    dst += dst_stride;
    y_q4 += y_step_q4;
  } while (--y);
}

void aom_scaled_2d_neon(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                        ptrdiff_t dst_stride, const InterpKernel *filter,
                        int x0_q4, int x_step_q4, int y0_q4, int y_step_q4,
                        int w, int h) {
  // Note: Fixed size intermediate buffer, temp, places limits on parameters.
  // 2d filtering proceeds in 2 steps:
  //   (1) Interpolate horizontally into an intermediate buffer, temp.
  //   (2) Interpolate temp vertically to derive the sub-pixel result.
  // Deriving the maximum number of rows in the temp buffer (135):
  // --Smallest scaling factor is x1/2 ==> y_step_q4 = 32 (Normative).
  // --Largest block size is 64x64 pixels.
  // --64 rows in the downscaled frame span a distance of (64 - 1) * 32 in the
  //   original frame (in 1/16th pixel units).
  // --Must round-up because block may be located at sub-pixel position.
  // --Require an additional SUBPEL_TAPS rows for the 8-tap filter tails.
  // --((64 - 1) * 32 + 15) >> 4 + 8 = 135.
  // --Require an additional 8 rows for the horiz_w8 transpose tail.
  // When calling in frame scaling function, the smallest scaling factor is x1/4
  // ==> y_step_q4 = 64. Since w and h are at most 16, the temp buffer is still
  // big enough.
  DECLARE_ALIGNED(16, uint8_t, temp[(135 + 8) * 64]);
  const int intermediate_height =
      (((h - 1) * y_step_q4 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;

  assert(w <= 64);
  assert(h <= 64);
  assert(y_step_q4 <= 32 || (y_step_q4 <= 64 && h <= 32));
  assert(x_step_q4 <= 64);

  if (w >= 8) {
    scaledconvolve_horiz_w8(src - src_stride * (SUBPEL_TAPS / 2 - 1),
                            src_stride, temp, 64, filter, x0_q4, x_step_q4, w,
                            intermediate_height);
  } else {
    scaledconvolve_horiz_w4(src - src_stride * (SUBPEL_TAPS / 2 - 1),
                            src_stride, temp, 64, filter, x0_q4, x_step_q4, w,
                            intermediate_height);
  }

  if (w >= 16) {
    scaledconvolve_vert_w16(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst,
                            dst_stride, filter, y0_q4, y_step_q4, w, h);
  } else if (w == 8) {
    scaledconvolve_vert_w8(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst,
                           dst_stride, filter, y0_q4, y_step_q4, w, h);
  } else {
    scaledconvolve_vert_w4(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst,
                           dst_stride, filter, y0_q4, y_step_q4, w, h);
  }
}
