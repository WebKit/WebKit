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
#include <assert.h>

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "vpx/vpx_integer.h"
#include "vpx_dsp/arm/mem_neon.h"
#include "vpx_dsp/arm/transpose_neon.h"
#include "vpx_dsp/arm/vpx_convolve8_neon.h"
#include "vpx_ports/mem.h"

// Note:
// 1. src is not always 32-bit aligned, so don't call vld1_lane_u32(src).
// 2. After refactoring the shared code in kernel loops with inline functions,
// the decoder speed dropped a lot when using gcc compiler. Therefore there is
// no refactoring for those parts by now.
// 3. For horizontal convolve, there is an alternative optimization that
// convolves a single row in each loop. For each row, 8 sample banks with 4 or 8
// samples in each are read from memory: src, (src+1), (src+2), (src+3),
// (src+4), (src+5), (src+6), (src+7), or prepared by vector extract
// instructions. This optimization is much faster in speed unit test, but slowed
// down the whole decoder by 5%.

#if defined(__aarch64__) && \
    (defined(__ARM_FEATURE_DOTPROD) || defined(__ARM_FEATURE_MATMUL_INT8))

DECLARE_ALIGNED(16, static const uint8_t, dot_prod_permute_tbl[48]) = {
  0, 1, 2,  3,  1, 2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6,
  4, 5, 6,  7,  5, 6,  7,  8,  6,  7,  8,  9,  7,  8,  9,  10,
  8, 9, 10, 11, 9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14
};

DECLARE_ALIGNED(16, static const uint8_t, dot_prod_tran_concat_tbl[32]) = {
  0, 8,  16, 24, 1, 9,  17, 25, 2, 10, 18, 26, 3, 11, 19, 27,
  4, 12, 20, 28, 5, 13, 21, 29, 6, 14, 22, 30, 7, 15, 23, 31
};

DECLARE_ALIGNED(16, static const uint8_t, dot_prod_merge_block_tbl[48]) = {
  /* Shift left and insert new last column in transposed 4x4 block. */
  1, 2, 3, 16, 5, 6, 7, 20, 9, 10, 11, 24, 13, 14, 15, 28,
  /* Shift left and insert two new columns in transposed 4x4 block. */
  2, 3, 16, 17, 6, 7, 20, 21, 10, 11, 24, 25, 14, 15, 28, 29,
  /* Shift left and insert three new columns in transposed 4x4 block. */
  3, 16, 17, 18, 7, 20, 21, 22, 11, 24, 25, 26, 15, 28, 29, 30
};

#if defined(__ARM_FEATURE_MATMUL_INT8)

void vpx_convolve8_horiz_neon(const uint8_t *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride,
                              const InterpKernel *filter, int x0_q4,
                              int x_step_q4, int y0_q4, int y_step_q4, int w,
                              int h) {
  const int8x8_t filters = vmovn_s16(vld1q_s16(filter[x0_q4]));
  uint8x16_t s0, s1, s2, s3;

  assert(!((intptr_t)dst & 3));
  assert(!(dst_stride & 3));
  assert(x_step_q4 == 16);

  (void)x_step_q4;
  (void)y0_q4;
  (void)y_step_q4;

  src -= 3;

  if (w == 4) {
    const uint8x16x2_t permute_tbl = vld1q_u8_x2(dot_prod_permute_tbl);
    do {
      int32x4_t t0, t1, t2, t3;
      int16x8_t t01, t23;
      uint8x8_t d01, d23;

      load_u8_16x4(src, src_stride, &s0, &s1, &s2, &s3);

      t0 = convolve8_4_usdot(s0, filters, permute_tbl);
      t1 = convolve8_4_usdot(s1, filters, permute_tbl);
      t2 = convolve8_4_usdot(s2, filters, permute_tbl);
      t3 = convolve8_4_usdot(s3, filters, permute_tbl);
      t01 = vcombine_s16(vqmovn_s32(t0), vqmovn_s32(t1));
      t23 = vcombine_s16(vqmovn_s32(t2), vqmovn_s32(t3));
      d01 = vqrshrun_n_s16(t01, 7);
      d23 = vqrshrun_n_s16(t23, 7);

      store_u8(dst + 0 * dst_stride, dst_stride, d01);
      store_u8(dst + 2 * dst_stride, dst_stride, d23);

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  } else {
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(dot_prod_permute_tbl);
    const uint8_t *s;
    uint8_t *d;
    int width;
    uint8x8_t d0, d1, d2, d3;

    do {
      width = w;
      s = src;
      d = dst;
      do {
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        d0 = convolve8_8_usdot(s0, filters, permute_tbl);
        d1 = convolve8_8_usdot(s1, filters, permute_tbl);
        d2 = convolve8_8_usdot(s2, filters, permute_tbl);
        d3 = convolve8_8_usdot(s3, filters, permute_tbl);

        store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  }
}

void vpx_convolve8_avg_horiz_neon(const uint8_t *src, ptrdiff_t src_stride,
                                  uint8_t *dst, ptrdiff_t dst_stride,
                                  const InterpKernel *filter, int x0_q4,
                                  int x_step_q4, int y0_q4, int y_step_q4,
                                  int w, int h) {
  const int8x8_t filters = vmovn_s16(vld1q_s16(filter[x0_q4]));
  uint8x16_t s0, s1, s2, s3;

  assert(!((intptr_t)dst & 3));
  assert(!(dst_stride & 3));
  assert(x_step_q4 == 16);

  (void)x_step_q4;
  (void)y0_q4;
  (void)y_step_q4;

  src -= 3;

  if (w == 4) {
    const uint8x16x2_t permute_tbl = vld1q_u8_x2(dot_prod_permute_tbl);
    do {
      int32x4_t t0, t1, t2, t3;
      int16x8_t t01, t23;
      uint8x8_t d01, d23, dd01, dd23;
      dd01 = vdup_n_u8(0);
      dd23 = vdup_n_u8(0);

      load_u8_16x4(src, src_stride, &s0, &s1, &s2, &s3);

      t0 = convolve8_4_usdot(s0, filters, permute_tbl);
      t1 = convolve8_4_usdot(s1, filters, permute_tbl);
      t2 = convolve8_4_usdot(s2, filters, permute_tbl);
      t3 = convolve8_4_usdot(s3, filters, permute_tbl);
      t01 = vcombine_s16(vqmovn_s32(t0), vqmovn_s32(t1));
      t23 = vcombine_s16(vqmovn_s32(t2), vqmovn_s32(t3));
      d01 = vqrshrun_n_s16(t01, 7);
      d23 = vqrshrun_n_s16(t23, 7);

      dd01 = load_u8(dst + 0 * dst_stride, dst_stride);
      dd23 = load_u8(dst + 2 * dst_stride, dst_stride);

      d01 = vrhadd_u8(d01, dd01);
      d23 = vrhadd_u8(d23, dd23);

      store_u8(dst + 0 * dst_stride, dst_stride, d01);
      store_u8(dst + 2 * dst_stride, dst_stride, d23);

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  } else {
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(dot_prod_permute_tbl);
    const uint8_t *s;
    uint8_t *d;
    int width;
    uint8x8_t d0, d1, d2, d3, dd0, dd1, dd2, dd3;

    do {
      width = w;
      s = src;
      d = dst;
      do {
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        d0 = convolve8_8_usdot(s0, filters, permute_tbl);
        d1 = convolve8_8_usdot(s1, filters, permute_tbl);
        d2 = convolve8_8_usdot(s2, filters, permute_tbl);
        d3 = convolve8_8_usdot(s3, filters, permute_tbl);

        load_u8_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3);

        d0 = vrhadd_u8(d0, dd0);
        d1 = vrhadd_u8(d1, dd1);
        d2 = vrhadd_u8(d2, dd2);
        d3 = vrhadd_u8(d3, dd3);

        store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  }
}

static INLINE void transpose_concat_4x4(uint8x8_t a0, uint8x8_t a1,
                                        uint8x8_t a2, uint8x8_t a3,
                                        uint8x16_t *b,
                                        const uint8x16_t permute_tbl) {
  /* Transpose 8-bit elements and concatenate result rows as follows:
   * a0: 00, 01, 02, 03, XX, XX, XX, XX
   * a1: 10, 11, 12, 13, XX, XX, XX, XX
   * a2: 20, 21, 22, 23, XX, XX, XX, XX
   * a3: 30, 31, 32, 33, XX, XX, XX, XX
   *
   * b: 00, 10, 20, 30, 01, 11, 21, 31, 02, 12, 22, 32, 03, 13, 23, 33
   *
   * The 'permute_tbl' is always 'dot_prod_tran_concat_tbl' above. Passing it
   * as an argument is preferable to loading it directly from memory as this
   * inline helper is called many times from the same parent function.
   */

  uint8x16x2_t samples = { { vcombine_u8(a0, a1), vcombine_u8(a2, a3) } };
  *b = vqtbl2q_u8(samples, permute_tbl);
}

static INLINE void transpose_concat_8x4(uint8x8_t a0, uint8x8_t a1,
                                        uint8x8_t a2, uint8x8_t a3,
                                        uint8x16_t *b0, uint8x16_t *b1,
                                        const uint8x16x2_t permute_tbl) {
  /* Transpose 8-bit elements and concatenate result rows as follows:
   * a0: 00, 01, 02, 03, 04, 05, 06, 07
   * a1: 10, 11, 12, 13, 14, 15, 16, 17
   * a2: 20, 21, 22, 23, 24, 25, 26, 27
   * a3: 30, 31, 32, 33, 34, 35, 36, 37
   *
   * b0: 00, 10, 20, 30, 01, 11, 21, 31, 02, 12, 22, 32, 03, 13, 23, 33
   * b1: 04, 14, 24, 34, 05, 15, 25, 35, 06, 16, 26, 36, 07, 17, 27, 37
   *
   * The 'permute_tbl' is always 'dot_prod_tran_concat_tbl' above. Passing it
   * as an argument is preferable to loading it directly from memory as this
   * inline helper is called many times from the same parent function.
   */

  uint8x16x2_t samples = { { vcombine_u8(a0, a1), vcombine_u8(a2, a3) } };
  *b0 = vqtbl2q_u8(samples, permute_tbl.val[0]);
  *b1 = vqtbl2q_u8(samples, permute_tbl.val[1]);
}

void vpx_convolve8_vert_neon(const uint8_t *src, ptrdiff_t src_stride,
                             uint8_t *dst, ptrdiff_t dst_stride,
                             const InterpKernel *filter, int x0_q4,
                             int x_step_q4, int y0_q4, int y_step_q4, int w,
                             int h) {
  const int8x8_t filters = vmovn_s16(vld1q_s16(filter[y0_q4]));
  const uint8x16x3_t merge_block_tbl = vld1q_u8_x3(dot_prod_merge_block_tbl);
  uint8x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
  uint8x16x2_t samples_LUT;

  assert(!((intptr_t)dst & 3));
  assert(!(dst_stride & 3));
  assert(y_step_q4 == 16);

  (void)x0_q4;
  (void)x_step_q4;
  (void)y_step_q4;

  src -= 3 * src_stride;

  if (w == 4) {
    const uint8x16_t tran_concat_tbl = vld1q_u8(dot_prod_tran_concat_tbl);
    uint8x16_t s0123, s1234, s2345, s3456, s4567, s5678, s6789, s78910;
    int32x4_t d0, d1, d2, d3;
    uint8x8_t d01, d23;

    load_u8_8x7(src, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    src += 7 * src_stride;

    s7 = vdup_n_u8(0);
    s8 = vdup_n_u8(0);
    s9 = vdup_n_u8(0);

    /* This operation combines a conventional transpose and the sample permute
     * (see horizontal case) required before computing the dot product.
     */
    transpose_concat_4x4(s0, s1, s2, s3, &s0123, tran_concat_tbl);
    transpose_concat_4x4(s1, s2, s3, s4, &s1234, tran_concat_tbl);
    transpose_concat_4x4(s2, s3, s4, s5, &s2345, tran_concat_tbl);
    transpose_concat_4x4(s3, s4, s5, s6, &s3456, tran_concat_tbl);
    transpose_concat_4x4(s4, s5, s6, s7, &s4567, tran_concat_tbl);
    transpose_concat_4x4(s5, s6, s7, s8, &s5678, tran_concat_tbl);
    transpose_concat_4x4(s6, s7, s8, s9, &s6789, tran_concat_tbl);

    do {
      load_u8_8x4(src, src_stride, &s7, &s8, &s9, &s10);

      transpose_concat_4x4(s7, s8, s9, s10, &s78910, tran_concat_tbl);

      /* Merge new data into block from previous iteration. */
      samples_LUT.val[0] = s3456;
      samples_LUT.val[1] = s78910;
      s4567 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[0]);
      s5678 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[1]);
      s6789 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[2]);

      d0 = convolve8_4_usdot_partial(s0123, s4567, filters);
      d1 = convolve8_4_usdot_partial(s1234, s5678, filters);
      d2 = convolve8_4_usdot_partial(s2345, s6789, filters);
      d3 = convolve8_4_usdot_partial(s3456, s78910, filters);
      d01 = vqrshrun_n_s16(vcombine_s16(vqmovn_s32(d0), vqmovn_s32(d1)), 7);
      d23 = vqrshrun_n_s16(vcombine_s16(vqmovn_s32(d2), vqmovn_s32(d3)), 7);

      store_u8(dst + 0 * dst_stride, dst_stride, d01);
      store_u8(dst + 2 * dst_stride, dst_stride, d23);

      /* Prepare block for next iteration - re-using as much as possible. */
      /* Shuffle everything up four rows. */
      s0123 = s4567;
      s1234 = s5678;
      s2345 = s6789;
      s3456 = s78910;

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  } else {
    const uint8x16x2_t tran_concat_tbl = vld1q_u8_x2(dot_prod_tran_concat_tbl);
    uint8x16_t s0123_lo, s0123_hi, s1234_lo, s1234_hi, s2345_lo, s2345_hi,
        s3456_lo, s3456_hi, s4567_lo, s4567_hi, s5678_lo, s5678_hi, s6789_lo,
        s6789_hi, s78910_lo, s78910_hi;
    uint8x8_t d0, d1, d2, d3;
    const uint8_t *s;
    uint8_t *d;
    int height;

    do {
      height = h;
      s = src;
      d = dst;

      load_u8_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      s7 = vdup_n_u8(0);
      s8 = vdup_n_u8(0);
      s9 = vdup_n_u8(0);

      /* This operation combines a conventional transpose and the sample permute
       * (see horizontal case) required before computing the dot product.
       */
      transpose_concat_8x4(s0, s1, s2, s3, &s0123_lo, &s0123_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s1, s2, s3, s4, &s1234_lo, &s1234_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s2, s3, s4, s5, &s2345_lo, &s2345_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s3, s4, s5, s6, &s3456_lo, &s3456_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s4, s5, s6, s7, &s4567_lo, &s4567_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s5, s6, s7, s8, &s5678_lo, &s5678_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s6, s7, s8, s9, &s6789_lo, &s6789_hi,
                           tran_concat_tbl);

      do {
        load_u8_8x4(s, src_stride, &s7, &s8, &s9, &s10);

        transpose_concat_8x4(s7, s8, s9, s10, &s78910_lo, &s78910_hi,
                             tran_concat_tbl);

        /* Merge new data into block from previous iteration. */
        samples_LUT.val[0] = s3456_lo;
        samples_LUT.val[1] = s78910_lo;
        s4567_lo = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[0]);
        s5678_lo = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[1]);
        s6789_lo = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[2]);

        samples_LUT.val[0] = s3456_hi;
        samples_LUT.val[1] = s78910_hi;
        s4567_hi = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[0]);
        s5678_hi = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[1]);
        s6789_hi = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[2]);

        d0 = convolve8_8_usdot_partial(s0123_lo, s4567_lo, s0123_hi, s4567_hi,
                                       filters);
        d1 = convolve8_8_usdot_partial(s1234_lo, s5678_lo, s1234_hi, s5678_hi,
                                       filters);
        d2 = convolve8_8_usdot_partial(s2345_lo, s6789_lo, s2345_hi, s6789_hi,
                                       filters);
        d3 = convolve8_8_usdot_partial(s3456_lo, s78910_lo, s3456_hi, s78910_hi,
                                       filters);

        store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

        /* Prepare block for next iteration - re-using as much as possible. */
        /* Shuffle everything up four rows. */
        s0123_lo = s4567_lo;
        s0123_hi = s4567_hi;
        s1234_lo = s5678_lo;
        s1234_hi = s5678_hi;
        s2345_lo = s6789_lo;
        s2345_hi = s6789_hi;
        s3456_lo = s78910_lo;
        s3456_hi = s78910_hi;

        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height > 0);
      src += 8;
      dst += 8;
      w -= 8;
    } while (w > 0);
  }
}

void vpx_convolve8_avg_vert_neon(const uint8_t *src, ptrdiff_t src_stride,
                                 uint8_t *dst, ptrdiff_t dst_stride,
                                 const InterpKernel *filter, int x0_q4,
                                 int x_step_q4, int y0_q4, int y_step_q4, int w,
                                 int h) {
  const int8x8_t filters = vmovn_s16(vld1q_s16(filter[y0_q4]));
  const uint8x16x3_t merge_block_tbl = vld1q_u8_x3(dot_prod_merge_block_tbl);
  uint8x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
  uint8x16x2_t samples_LUT;

  assert(!((intptr_t)dst & 3));
  assert(!(dst_stride & 3));
  assert(y_step_q4 == 16);

  (void)x0_q4;
  (void)x_step_q4;
  (void)y_step_q4;

  src -= 3 * src_stride;

  if (w == 4) {
    const uint8x16_t tran_concat_tbl = vld1q_u8(dot_prod_tran_concat_tbl);
    uint8x16_t s0123, s1234, s2345, s3456, s4567, s5678, s6789, s78910;
    int32x4_t d0, d1, d2, d3;
    uint8x8_t d01, d23, dd01, dd23;

    load_u8_8x7(src, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    src += 7 * src_stride;

    s7 = vdup_n_u8(0);
    s8 = vdup_n_u8(0);
    s9 = vdup_n_u8(0);

    /* This operation combines a conventional transpose and the sample permute
     * (see horizontal case) required before computing the dot product.
     */
    transpose_concat_4x4(s0, s1, s2, s3, &s0123, tran_concat_tbl);
    transpose_concat_4x4(s1, s2, s3, s4, &s1234, tran_concat_tbl);
    transpose_concat_4x4(s2, s3, s4, s5, &s2345, tran_concat_tbl);
    transpose_concat_4x4(s3, s4, s5, s6, &s3456, tran_concat_tbl);
    transpose_concat_4x4(s4, s5, s6, s7, &s4567, tran_concat_tbl);
    transpose_concat_4x4(s5, s6, s7, s8, &s5678, tran_concat_tbl);
    transpose_concat_4x4(s6, s7, s8, s9, &s6789, tran_concat_tbl);

    do {
      load_u8_8x4(src, src_stride, &s7, &s8, &s9, &s10);

      transpose_concat_4x4(s7, s8, s9, s10, &s78910, tran_concat_tbl);

      /* Merge new data into block from previous iteration. */
      samples_LUT.val[0] = s3456;
      samples_LUT.val[1] = s78910;
      s4567 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[0]);
      s5678 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[1]);
      s6789 = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[2]);

      d0 = convolve8_4_usdot_partial(s0123, s4567, filters);
      d1 = convolve8_4_usdot_partial(s1234, s5678, filters);
      d2 = convolve8_4_usdot_partial(s2345, s6789, filters);
      d3 = convolve8_4_usdot_partial(s3456, s78910, filters);
      d01 = vqrshrun_n_s16(vcombine_s16(vqmovn_s32(d0), vqmovn_s32(d1)), 7);
      d23 = vqrshrun_n_s16(vcombine_s16(vqmovn_s32(d2), vqmovn_s32(d3)), 7);

      dd01 = load_u8(dst + 0 * dst_stride, dst_stride);
      dd23 = load_u8(dst + 2 * dst_stride, dst_stride);

      d01 = vrhadd_u8(d01, dd01);
      d23 = vrhadd_u8(d23, dd23);

      store_u8(dst + 0 * dst_stride, dst_stride, d01);
      store_u8(dst + 2 * dst_stride, dst_stride, d23);

      /* Prepare block for next iteration - re-using as much as possible. */
      /* Shuffle everything up four rows. */
      s0123 = s4567;
      s1234 = s5678;
      s2345 = s6789;
      s3456 = s78910;

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  } else {
    const uint8x16x2_t tran_concat_tbl = vld1q_u8_x2(dot_prod_tran_concat_tbl);
    uint8x16_t s0123_lo, s0123_hi, s1234_lo, s1234_hi, s2345_lo, s2345_hi,
        s3456_lo, s3456_hi, s4567_lo, s4567_hi, s5678_lo, s5678_hi, s6789_lo,
        s6789_hi, s78910_lo, s78910_hi;
    uint8x8_t d0, d1, d2, d3, dd0, dd1, dd2, dd3;
    const uint8_t *s;
    uint8_t *d;
    int height;

    do {
      height = h;
      s = src;
      d = dst;

      load_u8_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      s7 = vdup_n_u8(0);
      s8 = vdup_n_u8(0);
      s9 = vdup_n_u8(0);

      /* This operation combines a conventional transpose and the sample permute
       * (see horizontal case) required before computing the dot product.
       */
      transpose_concat_8x4(s0, s1, s2, s3, &s0123_lo, &s0123_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s1, s2, s3, s4, &s1234_lo, &s1234_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s2, s3, s4, s5, &s2345_lo, &s2345_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s3, s4, s5, s6, &s3456_lo, &s3456_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s4, s5, s6, s7, &s4567_lo, &s4567_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s5, s6, s7, s8, &s5678_lo, &s5678_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s6, s7, s8, s9, &s6789_lo, &s6789_hi,
                           tran_concat_tbl);

      do {
        load_u8_8x4(s, src_stride, &s7, &s8, &s9, &s10);

        transpose_concat_8x4(s7, s8, s9, s10, &s78910_lo, &s78910_hi,
                             tran_concat_tbl);

        /* Merge new data into block from previous iteration. */
        samples_LUT.val[0] = s3456_lo;
        samples_LUT.val[1] = s78910_lo;
        s4567_lo = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[0]);
        s5678_lo = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[1]);
        s6789_lo = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[2]);

        samples_LUT.val[0] = s3456_hi;
        samples_LUT.val[1] = s78910_hi;
        s4567_hi = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[0]);
        s5678_hi = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[1]);
        s6789_hi = vqtbl2q_u8(samples_LUT, merge_block_tbl.val[2]);

        d0 = convolve8_8_usdot_partial(s0123_lo, s4567_lo, s0123_hi, s4567_hi,
                                       filters);
        d1 = convolve8_8_usdot_partial(s1234_lo, s5678_lo, s1234_hi, s5678_hi,
                                       filters);
        d2 = convolve8_8_usdot_partial(s2345_lo, s6789_lo, s2345_hi, s6789_hi,
                                       filters);
        d3 = convolve8_8_usdot_partial(s3456_lo, s78910_lo, s3456_hi, s78910_hi,
                                       filters);

        load_u8_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3);

        d0 = vrhadd_u8(d0, dd0);
        d1 = vrhadd_u8(d1, dd1);
        d2 = vrhadd_u8(d2, dd2);
        d3 = vrhadd_u8(d3, dd3);

        store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

        /* Prepare block for next iteration - re-using as much as possible. */
        /* Shuffle everything up four rows. */
        s0123_lo = s4567_lo;
        s0123_hi = s4567_hi;
        s1234_lo = s5678_lo;
        s1234_hi = s5678_hi;
        s2345_lo = s6789_lo;
        s2345_hi = s6789_hi;
        s3456_lo = s78910_lo;
        s3456_hi = s78910_hi;

        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height > 0);
      src += 8;
      dst += 8;
      w -= 8;
    } while (w > 0);
  }
}

#else  // !defined(__ARM_FEATURE_MATMUL_INT8)

void vpx_convolve8_horiz_neon(const uint8_t *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride,
                              const InterpKernel *filter, int x0_q4,
                              int x_step_q4, int y0_q4, int y_step_q4, int w,
                              int h) {
  const int8x8_t filters = vmovn_s16(vld1q_s16(filter[x0_q4]));
  const int16x8_t correct_tmp = vmulq_n_s16(vld1q_s16(filter[x0_q4]), 128);
  const int32x4_t correction = vdupq_n_s32((int32_t)vaddvq_s16(correct_tmp));
  const uint8x16_t range_limit = vdupq_n_u8(128);
  uint8x16_t s0, s1, s2, s3;

  assert(!((intptr_t)dst & 3));
  assert(!(dst_stride & 3));
  assert(x_step_q4 == 16);

  (void)x_step_q4;
  (void)y0_q4;
  (void)y_step_q4;

  src -= 3;

  if (w == 4) {
    const uint8x16x2_t permute_tbl = vld1q_u8_x2(dot_prod_permute_tbl);
    do {
      int32x4_t t0, t1, t2, t3;
      int16x8_t t01, t23;
      uint8x8_t d01, d23;

      load_u8_16x4(src, src_stride, &s0, &s1, &s2, &s3);

      t0 = convolve8_4_sdot(s0, filters, correction, range_limit, permute_tbl);
      t1 = convolve8_4_sdot(s1, filters, correction, range_limit, permute_tbl);
      t2 = convolve8_4_sdot(s2, filters, correction, range_limit, permute_tbl);
      t3 = convolve8_4_sdot(s3, filters, correction, range_limit, permute_tbl);
      t01 = vcombine_s16(vqmovn_s32(t0), vqmovn_s32(t1));
      t23 = vcombine_s16(vqmovn_s32(t2), vqmovn_s32(t3));
      d01 = vqrshrun_n_s16(t01, 7);
      d23 = vqrshrun_n_s16(t23, 7);

      store_u8(dst + 0 * dst_stride, dst_stride, d01);
      store_u8(dst + 2 * dst_stride, dst_stride, d23);

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  } else {
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(dot_prod_permute_tbl);
    const uint8_t *s;
    uint8_t *d;
    int width;
    uint8x8_t d0, d1, d2, d3;

    do {
      width = w;
      s = src;
      d = dst;
      do {
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        d0 =
            convolve8_8_sdot(s0, filters, correction, range_limit, permute_tbl);
        d1 =
            convolve8_8_sdot(s1, filters, correction, range_limit, permute_tbl);
        d2 =
            convolve8_8_sdot(s2, filters, correction, range_limit, permute_tbl);
        d3 =
            convolve8_8_sdot(s3, filters, correction, range_limit, permute_tbl);

        store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  }
}

void vpx_convolve8_avg_horiz_neon(const uint8_t *src, ptrdiff_t src_stride,
                                  uint8_t *dst, ptrdiff_t dst_stride,
                                  const InterpKernel *filter, int x0_q4,
                                  int x_step_q4, int y0_q4, int y_step_q4,
                                  int w, int h) {
  const int8x8_t filters = vmovn_s16(vld1q_s16(filter[x0_q4]));
  const int16x8_t correct_tmp = vmulq_n_s16(vld1q_s16(filter[x0_q4]), 128);
  const int32x4_t correction = vdupq_n_s32((int32_t)vaddvq_s16(correct_tmp));
  const uint8x16_t range_limit = vdupq_n_u8(128);
  uint8x16_t s0, s1, s2, s3;

  assert(!((intptr_t)dst & 3));
  assert(!(dst_stride & 3));
  assert(x_step_q4 == 16);

  (void)x_step_q4;
  (void)y0_q4;
  (void)y_step_q4;

  src -= 3;

  if (w == 4) {
    const uint8x16x2_t permute_tbl = vld1q_u8_x2(dot_prod_permute_tbl);
    do {
      int32x4_t t0, t1, t2, t3;
      int16x8_t t01, t23;
      uint8x8_t d01, d23, dd01, dd23;
      dd01 = vdup_n_u8(0);
      dd23 = vdup_n_u8(0);

      load_u8_16x4(src, src_stride, &s0, &s1, &s2, &s3);

      t0 = convolve8_4_sdot(s0, filters, correction, range_limit, permute_tbl);
      t1 = convolve8_4_sdot(s1, filters, correction, range_limit, permute_tbl);
      t2 = convolve8_4_sdot(s2, filters, correction, range_limit, permute_tbl);
      t3 = convolve8_4_sdot(s3, filters, correction, range_limit, permute_tbl);
      t01 = vcombine_s16(vqmovn_s32(t0), vqmovn_s32(t1));
      t23 = vcombine_s16(vqmovn_s32(t2), vqmovn_s32(t3));
      d01 = vqrshrun_n_s16(t01, 7);
      d23 = vqrshrun_n_s16(t23, 7);

      dd01 = load_u8(dst + 0 * dst_stride, dst_stride);
      dd23 = load_u8(dst + 2 * dst_stride, dst_stride);

      d01 = vrhadd_u8(d01, dd01);
      d23 = vrhadd_u8(d23, dd23);

      store_u8(dst + 0 * dst_stride, dst_stride, d01);
      store_u8(dst + 2 * dst_stride, dst_stride, d23);

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  } else {
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(dot_prod_permute_tbl);
    const uint8_t *s;
    uint8_t *d;
    int width;
    uint8x8_t d0, d1, d2, d3, dd0, dd1, dd2, dd3;

    do {
      width = w;
      s = src;
      d = dst;
      do {
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        d0 =
            convolve8_8_sdot(s0, filters, correction, range_limit, permute_tbl);
        d1 =
            convolve8_8_sdot(s1, filters, correction, range_limit, permute_tbl);
        d2 =
            convolve8_8_sdot(s2, filters, correction, range_limit, permute_tbl);
        d3 =
            convolve8_8_sdot(s3, filters, correction, range_limit, permute_tbl);

        load_u8_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3);

        d0 = vrhadd_u8(d0, dd0);
        d1 = vrhadd_u8(d1, dd1);
        d2 = vrhadd_u8(d2, dd2);
        d3 = vrhadd_u8(d3, dd3);

        store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  }
}

static INLINE void transpose_concat_4x4(int8x8_t a0, int8x8_t a1, int8x8_t a2,
                                        int8x8_t a3, int8x16_t *b,
                                        const uint8x16_t permute_tbl) {
  /* Transpose 8-bit elements and concatenate result rows as follows:
   * a0: 00, 01, 02, 03, XX, XX, XX, XX
   * a1: 10, 11, 12, 13, XX, XX, XX, XX
   * a2: 20, 21, 22, 23, XX, XX, XX, XX
   * a3: 30, 31, 32, 33, XX, XX, XX, XX
   *
   * b: 00, 10, 20, 30, 01, 11, 21, 31, 02, 12, 22, 32, 03, 13, 23, 33
   *
   * The 'permute_tbl' is always 'dot_prod_tran_concat_tbl' above. Passing it
   * as an argument is preferable to loading it directly from memory as this
   * inline helper is called many times from the same parent function.
   */

  int8x16x2_t samples = { { vcombine_s8(a0, a1), vcombine_s8(a2, a3) } };
  *b = vqtbl2q_s8(samples, permute_tbl);
}

static INLINE void transpose_concat_8x4(int8x8_t a0, int8x8_t a1, int8x8_t a2,
                                        int8x8_t a3, int8x16_t *b0,
                                        int8x16_t *b1,
                                        const uint8x16x2_t permute_tbl) {
  /* Transpose 8-bit elements and concatenate result rows as follows:
   * a0: 00, 01, 02, 03, 04, 05, 06, 07
   * a1: 10, 11, 12, 13, 14, 15, 16, 17
   * a2: 20, 21, 22, 23, 24, 25, 26, 27
   * a3: 30, 31, 32, 33, 34, 35, 36, 37
   *
   * b0: 00, 10, 20, 30, 01, 11, 21, 31, 02, 12, 22, 32, 03, 13, 23, 33
   * b1: 04, 14, 24, 34, 05, 15, 25, 35, 06, 16, 26, 36, 07, 17, 27, 37
   *
   * The 'permute_tbl' is always 'dot_prod_tran_concat_tbl' above. Passing it
   * as an argument is preferable to loading it directly from memory as this
   * inline helper is called many times from the same parent function.
   */

  int8x16x2_t samples = { { vcombine_s8(a0, a1), vcombine_s8(a2, a3) } };
  *b0 = vqtbl2q_s8(samples, permute_tbl.val[0]);
  *b1 = vqtbl2q_s8(samples, permute_tbl.val[1]);
}

void vpx_convolve8_vert_neon(const uint8_t *src, ptrdiff_t src_stride,
                             uint8_t *dst, ptrdiff_t dst_stride,
                             const InterpKernel *filter, int x0_q4,
                             int x_step_q4, int y0_q4, int y_step_q4, int w,
                             int h) {
  const int8x8_t filters = vmovn_s16(vld1q_s16(filter[y0_q4]));
  const int16x8_t correct_tmp = vmulq_n_s16(vld1q_s16(filter[y0_q4]), 128);
  const int32x4_t correction = vdupq_n_s32((int32_t)vaddvq_s16(correct_tmp));
  const uint8x8_t range_limit = vdup_n_u8(128);
  const uint8x16x3_t merge_block_tbl = vld1q_u8_x3(dot_prod_merge_block_tbl);
  uint8x8_t t0, t1, t2, t3, t4, t5, t6;
  int8x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
  int8x16x2_t samples_LUT;

  assert(!((intptr_t)dst & 3));
  assert(!(dst_stride & 3));
  assert(y_step_q4 == 16);

  (void)x0_q4;
  (void)x_step_q4;
  (void)y_step_q4;

  src -= 3 * src_stride;

  if (w == 4) {
    const uint8x16_t tran_concat_tbl = vld1q_u8(dot_prod_tran_concat_tbl);
    int8x16_t s0123, s1234, s2345, s3456, s4567, s5678, s6789, s78910;
    int32x4_t d0, d1, d2, d3;
    uint8x8_t d01, d23;

    load_u8_8x7(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6);
    src += 7 * src_stride;

    /* Clamp sample range to [-128, 127] for 8-bit signed dot product. */
    s0 = vreinterpret_s8_u8(vsub_u8(t0, range_limit));
    s1 = vreinterpret_s8_u8(vsub_u8(t1, range_limit));
    s2 = vreinterpret_s8_u8(vsub_u8(t2, range_limit));
    s3 = vreinterpret_s8_u8(vsub_u8(t3, range_limit));
    s4 = vreinterpret_s8_u8(vsub_u8(t4, range_limit));
    s5 = vreinterpret_s8_u8(vsub_u8(t5, range_limit));
    s6 = vreinterpret_s8_u8(vsub_u8(t6, range_limit));
    s7 = vdup_n_s8(0);
    s8 = vdup_n_s8(0);
    s9 = vdup_n_s8(0);

    /* This operation combines a conventional transpose and the sample permute
     * (see horizontal case) required before computing the dot product.
     */
    transpose_concat_4x4(s0, s1, s2, s3, &s0123, tran_concat_tbl);
    transpose_concat_4x4(s1, s2, s3, s4, &s1234, tran_concat_tbl);
    transpose_concat_4x4(s2, s3, s4, s5, &s2345, tran_concat_tbl);
    transpose_concat_4x4(s3, s4, s5, s6, &s3456, tran_concat_tbl);
    transpose_concat_4x4(s4, s5, s6, s7, &s4567, tran_concat_tbl);
    transpose_concat_4x4(s5, s6, s7, s8, &s5678, tran_concat_tbl);
    transpose_concat_4x4(s6, s7, s8, s9, &s6789, tran_concat_tbl);

    do {
      uint8x8_t t7, t8, t9, t10;

      load_u8_8x4(src, src_stride, &t7, &t8, &t9, &t10);

      s7 = vreinterpret_s8_u8(vsub_u8(t7, range_limit));
      s8 = vreinterpret_s8_u8(vsub_u8(t8, range_limit));
      s9 = vreinterpret_s8_u8(vsub_u8(t9, range_limit));
      s10 = vreinterpret_s8_u8(vsub_u8(t10, range_limit));

      transpose_concat_4x4(s7, s8, s9, s10, &s78910, tran_concat_tbl);

      /* Merge new data into block from previous iteration. */
      samples_LUT.val[0] = s3456;
      samples_LUT.val[1] = s78910;
      s4567 = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[0]);
      s5678 = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[1]);
      s6789 = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[2]);

      d0 = convolve8_4_sdot_partial(s0123, s4567, correction, filters);
      d1 = convolve8_4_sdot_partial(s1234, s5678, correction, filters);
      d2 = convolve8_4_sdot_partial(s2345, s6789, correction, filters);
      d3 = convolve8_4_sdot_partial(s3456, s78910, correction, filters);
      d01 = vqrshrun_n_s16(vcombine_s16(vqmovn_s32(d0), vqmovn_s32(d1)), 7);
      d23 = vqrshrun_n_s16(vcombine_s16(vqmovn_s32(d2), vqmovn_s32(d3)), 7);

      store_u8(dst + 0 * dst_stride, dst_stride, d01);
      store_u8(dst + 2 * dst_stride, dst_stride, d23);

      /* Prepare block for next iteration - re-using as much as possible. */
      /* Shuffle everything up four rows. */
      s0123 = s4567;
      s1234 = s5678;
      s2345 = s6789;
      s3456 = s78910;

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  } else {
    const uint8x16x2_t tran_concat_tbl = vld1q_u8_x2(dot_prod_tran_concat_tbl);
    int8x16_t s0123_lo, s0123_hi, s1234_lo, s1234_hi, s2345_lo, s2345_hi,
        s3456_lo, s3456_hi, s4567_lo, s4567_hi, s5678_lo, s5678_hi, s6789_lo,
        s6789_hi, s78910_lo, s78910_hi;
    uint8x8_t d0, d1, d2, d3;
    const uint8_t *s;
    uint8_t *d;
    int height;

    do {
      height = h;
      s = src;
      d = dst;

      load_u8_8x7(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6);
      s += 7 * src_stride;

      /* Clamp sample range to [-128, 127] for 8-bit signed dot product. */
      s0 = vreinterpret_s8_u8(vsub_u8(t0, range_limit));
      s1 = vreinterpret_s8_u8(vsub_u8(t1, range_limit));
      s2 = vreinterpret_s8_u8(vsub_u8(t2, range_limit));
      s3 = vreinterpret_s8_u8(vsub_u8(t3, range_limit));
      s4 = vreinterpret_s8_u8(vsub_u8(t4, range_limit));
      s5 = vreinterpret_s8_u8(vsub_u8(t5, range_limit));
      s6 = vreinterpret_s8_u8(vsub_u8(t6, range_limit));
      s7 = vdup_n_s8(0);
      s8 = vdup_n_s8(0);
      s9 = vdup_n_s8(0);

      /* This operation combines a conventional transpose and the sample permute
       * (see horizontal case) required before computing the dot product.
       */
      transpose_concat_8x4(s0, s1, s2, s3, &s0123_lo, &s0123_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s1, s2, s3, s4, &s1234_lo, &s1234_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s2, s3, s4, s5, &s2345_lo, &s2345_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s3, s4, s5, s6, &s3456_lo, &s3456_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s4, s5, s6, s7, &s4567_lo, &s4567_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s5, s6, s7, s8, &s5678_lo, &s5678_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s6, s7, s8, s9, &s6789_lo, &s6789_hi,
                           tran_concat_tbl);

      do {
        uint8x8_t t7, t8, t9, t10;

        load_u8_8x4(s, src_stride, &t7, &t8, &t9, &t10);

        s7 = vreinterpret_s8_u8(vsub_u8(t7, range_limit));
        s8 = vreinterpret_s8_u8(vsub_u8(t8, range_limit));
        s9 = vreinterpret_s8_u8(vsub_u8(t9, range_limit));
        s10 = vreinterpret_s8_u8(vsub_u8(t10, range_limit));

        transpose_concat_8x4(s7, s8, s9, s10, &s78910_lo, &s78910_hi,
                             tran_concat_tbl);

        /* Merge new data into block from previous iteration. */
        samples_LUT.val[0] = s3456_lo;
        samples_LUT.val[1] = s78910_lo;
        s4567_lo = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[0]);
        s5678_lo = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[1]);
        s6789_lo = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[2]);

        samples_LUT.val[0] = s3456_hi;
        samples_LUT.val[1] = s78910_hi;
        s4567_hi = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[0]);
        s5678_hi = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[1]);
        s6789_hi = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[2]);

        d0 = convolve8_8_sdot_partial(s0123_lo, s4567_lo, s0123_hi, s4567_hi,
                                      correction, filters);
        d1 = convolve8_8_sdot_partial(s1234_lo, s5678_lo, s1234_hi, s5678_hi,
                                      correction, filters);
        d2 = convolve8_8_sdot_partial(s2345_lo, s6789_lo, s2345_hi, s6789_hi,
                                      correction, filters);
        d3 = convolve8_8_sdot_partial(s3456_lo, s78910_lo, s3456_hi, s78910_hi,
                                      correction, filters);

        store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

        /* Prepare block for next iteration - re-using as much as possible. */
        /* Shuffle everything up four rows. */
        s0123_lo = s4567_lo;
        s0123_hi = s4567_hi;
        s1234_lo = s5678_lo;
        s1234_hi = s5678_hi;
        s2345_lo = s6789_lo;
        s2345_hi = s6789_hi;
        s3456_lo = s78910_lo;
        s3456_hi = s78910_hi;

        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height > 0);
      src += 8;
      dst += 8;
      w -= 8;
    } while (w > 0);
  }
}

void vpx_convolve8_avg_vert_neon(const uint8_t *src, ptrdiff_t src_stride,
                                 uint8_t *dst, ptrdiff_t dst_stride,
                                 const InterpKernel *filter, int x0_q4,
                                 int x_step_q4, int y0_q4, int y_step_q4, int w,
                                 int h) {
  const int8x8_t filters = vmovn_s16(vld1q_s16(filter[y0_q4]));
  const int16x8_t correct_tmp = vmulq_n_s16(vld1q_s16(filter[y0_q4]), 128);
  const int32x4_t correction = vdupq_n_s32((int32_t)vaddvq_s16(correct_tmp));
  const uint8x8_t range_limit = vdup_n_u8(128);
  const uint8x16x3_t merge_block_tbl = vld1q_u8_x3(dot_prod_merge_block_tbl);
  uint8x8_t t0, t1, t2, t3, t4, t5, t6;
  int8x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
  int8x16x2_t samples_LUT;

  assert(!((intptr_t)dst & 3));
  assert(!(dst_stride & 3));
  assert(y_step_q4 == 16);

  (void)x0_q4;
  (void)x_step_q4;
  (void)y_step_q4;

  src -= 3 * src_stride;

  if (w == 4) {
    const uint8x16_t tran_concat_tbl = vld1q_u8(dot_prod_tran_concat_tbl);
    int8x16_t s0123, s1234, s2345, s3456, s4567, s5678, s6789, s78910;
    int32x4_t d0, d1, d2, d3;
    uint8x8_t d01, d23, dd01, dd23;

    load_u8_8x7(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6);
    src += 7 * src_stride;

    /* Clamp sample range to [-128, 127] for 8-bit signed dot product. */
    s0 = vreinterpret_s8_u8(vsub_u8(t0, range_limit));
    s1 = vreinterpret_s8_u8(vsub_u8(t1, range_limit));
    s2 = vreinterpret_s8_u8(vsub_u8(t2, range_limit));
    s3 = vreinterpret_s8_u8(vsub_u8(t3, range_limit));
    s4 = vreinterpret_s8_u8(vsub_u8(t4, range_limit));
    s5 = vreinterpret_s8_u8(vsub_u8(t5, range_limit));
    s6 = vreinterpret_s8_u8(vsub_u8(t6, range_limit));
    s7 = vdup_n_s8(0);
    s8 = vdup_n_s8(0);
    s9 = vdup_n_s8(0);

    /* This operation combines a conventional transpose and the sample permute
     * (see horizontal case) required before computing the dot product.
     */
    transpose_concat_4x4(s0, s1, s2, s3, &s0123, tran_concat_tbl);
    transpose_concat_4x4(s1, s2, s3, s4, &s1234, tran_concat_tbl);
    transpose_concat_4x4(s2, s3, s4, s5, &s2345, tran_concat_tbl);
    transpose_concat_4x4(s3, s4, s5, s6, &s3456, tran_concat_tbl);
    transpose_concat_4x4(s4, s5, s6, s7, &s4567, tran_concat_tbl);
    transpose_concat_4x4(s5, s6, s7, s8, &s5678, tran_concat_tbl);
    transpose_concat_4x4(s6, s7, s8, s9, &s6789, tran_concat_tbl);

    do {
      uint8x8_t t7, t8, t9, t10;

      load_u8_8x4(src, src_stride, &t7, &t8, &t9, &t10);

      s7 = vreinterpret_s8_u8(vsub_u8(t7, range_limit));
      s8 = vreinterpret_s8_u8(vsub_u8(t8, range_limit));
      s9 = vreinterpret_s8_u8(vsub_u8(t9, range_limit));
      s10 = vreinterpret_s8_u8(vsub_u8(t10, range_limit));

      transpose_concat_4x4(s7, s8, s9, s10, &s78910, tran_concat_tbl);

      /* Merge new data into block from previous iteration. */
      samples_LUT.val[0] = s3456;
      samples_LUT.val[1] = s78910;
      s4567 = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[0]);
      s5678 = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[1]);
      s6789 = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[2]);

      d0 = convolve8_4_sdot_partial(s0123, s4567, correction, filters);
      d1 = convolve8_4_sdot_partial(s1234, s5678, correction, filters);
      d2 = convolve8_4_sdot_partial(s2345, s6789, correction, filters);
      d3 = convolve8_4_sdot_partial(s3456, s78910, correction, filters);
      d01 = vqrshrun_n_s16(vcombine_s16(vqmovn_s32(d0), vqmovn_s32(d1)), 7);
      d23 = vqrshrun_n_s16(vcombine_s16(vqmovn_s32(d2), vqmovn_s32(d3)), 7);

      dd01 = load_u8(dst + 0 * dst_stride, dst_stride);
      dd23 = load_u8(dst + 2 * dst_stride, dst_stride);

      d01 = vrhadd_u8(d01, dd01);
      d23 = vrhadd_u8(d23, dd23);

      store_u8(dst + 0 * dst_stride, dst_stride, d01);
      store_u8(dst + 2 * dst_stride, dst_stride, d23);

      /* Prepare block for next iteration - re-using as much as possible. */
      /* Shuffle everything up four rows. */
      s0123 = s4567;
      s1234 = s5678;
      s2345 = s6789;
      s3456 = s78910;

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  } else {
    const uint8x16x2_t tran_concat_tbl = vld1q_u8_x2(dot_prod_tran_concat_tbl);
    int8x16_t s0123_lo, s0123_hi, s1234_lo, s1234_hi, s2345_lo, s2345_hi,
        s3456_lo, s3456_hi, s4567_lo, s4567_hi, s5678_lo, s5678_hi, s6789_lo,
        s6789_hi, s78910_lo, s78910_hi;
    uint8x8_t d0, d1, d2, d3, dd0, dd1, dd2, dd3;
    const uint8_t *s;
    uint8_t *d;
    int height;

    do {
      height = h;
      s = src;
      d = dst;

      load_u8_8x7(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6);
      s += 7 * src_stride;

      /* Clamp sample range to [-128, 127] for 8-bit signed dot product. */
      s0 = vreinterpret_s8_u8(vsub_u8(t0, range_limit));
      s1 = vreinterpret_s8_u8(vsub_u8(t1, range_limit));
      s2 = vreinterpret_s8_u8(vsub_u8(t2, range_limit));
      s3 = vreinterpret_s8_u8(vsub_u8(t3, range_limit));
      s4 = vreinterpret_s8_u8(vsub_u8(t4, range_limit));
      s5 = vreinterpret_s8_u8(vsub_u8(t5, range_limit));
      s6 = vreinterpret_s8_u8(vsub_u8(t6, range_limit));
      s7 = vdup_n_s8(0);
      s8 = vdup_n_s8(0);
      s9 = vdup_n_s8(0);

      /* This operation combines a conventional transpose and the sample permute
       * (see horizontal case) required before computing the dot product.
       */
      transpose_concat_8x4(s0, s1, s2, s3, &s0123_lo, &s0123_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s1, s2, s3, s4, &s1234_lo, &s1234_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s2, s3, s4, s5, &s2345_lo, &s2345_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s3, s4, s5, s6, &s3456_lo, &s3456_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s4, s5, s6, s7, &s4567_lo, &s4567_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s5, s6, s7, s8, &s5678_lo, &s5678_hi,
                           tran_concat_tbl);
      transpose_concat_8x4(s6, s7, s8, s9, &s6789_lo, &s6789_hi,
                           tran_concat_tbl);

      do {
        uint8x8_t t7, t8, t9, t10;

        load_u8_8x4(s, src_stride, &t7, &t8, &t9, &t10);

        s7 = vreinterpret_s8_u8(vsub_u8(t7, range_limit));
        s8 = vreinterpret_s8_u8(vsub_u8(t8, range_limit));
        s9 = vreinterpret_s8_u8(vsub_u8(t9, range_limit));
        s10 = vreinterpret_s8_u8(vsub_u8(t10, range_limit));

        transpose_concat_8x4(s7, s8, s9, s10, &s78910_lo, &s78910_hi,
                             tran_concat_tbl);

        /* Merge new data into block from previous iteration. */
        samples_LUT.val[0] = s3456_lo;
        samples_LUT.val[1] = s78910_lo;
        s4567_lo = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[0]);
        s5678_lo = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[1]);
        s6789_lo = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[2]);

        samples_LUT.val[0] = s3456_hi;
        samples_LUT.val[1] = s78910_hi;
        s4567_hi = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[0]);
        s5678_hi = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[1]);
        s6789_hi = vqtbl2q_s8(samples_LUT, merge_block_tbl.val[2]);

        d0 = convolve8_8_sdot_partial(s0123_lo, s4567_lo, s0123_hi, s4567_hi,
                                      correction, filters);
        d1 = convolve8_8_sdot_partial(s1234_lo, s5678_lo, s1234_hi, s5678_hi,
                                      correction, filters);
        d2 = convolve8_8_sdot_partial(s2345_lo, s6789_lo, s2345_hi, s6789_hi,
                                      correction, filters);
        d3 = convolve8_8_sdot_partial(s3456_lo, s78910_lo, s3456_hi, s78910_hi,
                                      correction, filters);

        load_u8_8x4(d, dst_stride, &dd0, &dd1, &dd2, &dd3);

        d0 = vrhadd_u8(d0, dd0);
        d1 = vrhadd_u8(d1, dd1);
        d2 = vrhadd_u8(d2, dd2);
        d3 = vrhadd_u8(d3, dd3);

        store_u8_8x4(d, dst_stride, d0, d1, d2, d3);

        /* Prepare block for next iteration - re-using as much as possible. */
        /* Shuffle everything up four rows. */
        s0123_lo = s4567_lo;
        s0123_hi = s4567_hi;
        s1234_lo = s5678_lo;
        s1234_hi = s5678_hi;
        s2345_lo = s6789_lo;
        s2345_hi = s6789_hi;
        s3456_lo = s78910_lo;
        s3456_hi = s78910_hi;

        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height > 0);
      src += 8;
      dst += 8;
      w -= 8;
    } while (w > 0);
  }
}

#endif  // defined(__ARM_FEATURE_MATMUL_INT8)

#else  // !(defined(__aarch64__) &&
       //   (defined(__ARM_FEATURE_DOTPROD) ||
       //    defined(__ARM_FEATURE_MATMUL_INT8)))

void vpx_convolve8_horiz_neon(const uint8_t *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride,
                              const InterpKernel *filter, int x0_q4,
                              int x_step_q4, int y0_q4, int y_step_q4, int w,
                              int h) {
  const int16x8_t filters = vld1q_s16(filter[x0_q4]);
  uint8x8_t t0, t1, t2, t3;

  assert(!((intptr_t)dst & 3));
  assert(!(dst_stride & 3));
  assert(x_step_q4 == 16);

  (void)x_step_q4;
  (void)y0_q4;
  (void)y_step_q4;

  src -= 3;

  if (h == 4) {
    uint8x8_t d01, d23;
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;
    int16x8_t tt0, tt1, tt2, tt3;

    __builtin_prefetch(src + 0 * src_stride);
    __builtin_prefetch(src + 1 * src_stride);
    __builtin_prefetch(src + 2 * src_stride);
    __builtin_prefetch(src + 3 * src_stride);
    load_u8_8x4(src, src_stride, &t0, &t1, &t2, &t3);
    transpose_u8_8x4(&t0, &t1, &t2, &t3);
    tt0 = vreinterpretq_s16_u16(vmovl_u8(t0));
    tt1 = vreinterpretq_s16_u16(vmovl_u8(t1));
    tt2 = vreinterpretq_s16_u16(vmovl_u8(t2));
    tt3 = vreinterpretq_s16_u16(vmovl_u8(t3));
    s0 = vget_low_s16(tt0);
    s1 = vget_low_s16(tt1);
    s2 = vget_low_s16(tt2);
    s3 = vget_low_s16(tt3);
    s4 = vget_high_s16(tt0);
    s5 = vget_high_s16(tt1);
    s6 = vget_high_s16(tt2);
    __builtin_prefetch(dst + 0 * dst_stride);
    __builtin_prefetch(dst + 1 * dst_stride);
    __builtin_prefetch(dst + 2 * dst_stride);
    __builtin_prefetch(dst + 3 * dst_stride);
    src += 7;

    do {
      load_u8_8x4(src, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);
      tt0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      tt1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      tt2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      tt3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      s7 = vget_low_s16(tt0);
      s8 = vget_low_s16(tt1);
      s9 = vget_low_s16(tt2);
      s10 = vget_low_s16(tt3);

      d0 = convolve8_4(s0, s1, s2, s3, s4, s5, s6, s7, filters);
      d1 = convolve8_4(s1, s2, s3, s4, s5, s6, s7, s8, filters);
      d2 = convolve8_4(s2, s3, s4, s5, s6, s7, s8, s9, filters);
      d3 = convolve8_4(s3, s4, s5, s6, s7, s8, s9, s10, filters);

      d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), 7);
      d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), 7);
      transpose_u8_4x4(&d01, &d23);

      vst1_lane_u32((uint32_t *)(dst + 0 * dst_stride),
                    vreinterpret_u32_u8(d01), 0);
      vst1_lane_u32((uint32_t *)(dst + 1 * dst_stride),
                    vreinterpret_u32_u8(d23), 0);
      vst1_lane_u32((uint32_t *)(dst + 2 * dst_stride),
                    vreinterpret_u32_u8(d01), 1);
      vst1_lane_u32((uint32_t *)(dst + 3 * dst_stride),
                    vreinterpret_u32_u8(d23), 1);

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      src += 4;
      dst += 4;
      w -= 4;
    } while (w != 0);
  } else {
    int width;
    const uint8_t *s;
    uint8x8_t t4, t5, t6, t7;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;

    if (w == 4) {
      do {
        load_u8_8x8(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
        s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
        s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
        s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

        load_u8_8x8(src + 7, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6,
                    &t7);
        src += 8 * src_stride;
        __builtin_prefetch(dst + 0 * dst_stride);
        __builtin_prefetch(dst + 1 * dst_stride);
        __builtin_prefetch(dst + 2 * dst_stride);
        __builtin_prefetch(dst + 3 * dst_stride);
        __builtin_prefetch(dst + 4 * dst_stride);
        __builtin_prefetch(dst + 5 * dst_stride);
        __builtin_prefetch(dst + 6 * dst_stride);
        __builtin_prefetch(dst + 7 * dst_stride);
        transpose_u8_4x8(&t0, &t1, &t2, &t3, t4, t5, t6, t7);
        s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s10 = vreinterpretq_s16_u16(vmovl_u8(t3));

        __builtin_prefetch(src + 0 * src_stride);
        __builtin_prefetch(src + 1 * src_stride);
        __builtin_prefetch(src + 2 * src_stride);
        __builtin_prefetch(src + 3 * src_stride);
        __builtin_prefetch(src + 4 * src_stride);
        __builtin_prefetch(src + 5 * src_stride);
        __builtin_prefetch(src + 6 * src_stride);
        __builtin_prefetch(src + 7 * src_stride);
        t0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filters);
        t1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filters);
        t2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filters);
        t3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filters);

        transpose_u8_8x4(&t0, &t1, &t2, &t3);
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t0), 0);
        dst += dst_stride;
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t1), 0);
        dst += dst_stride;
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t2), 0);
        dst += dst_stride;
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t3), 0);
        dst += dst_stride;
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t0), 1);
        dst += dst_stride;
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t1), 1);
        dst += dst_stride;
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t2), 1);
        dst += dst_stride;
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t3), 1);
        dst += dst_stride;
        h -= 8;
      } while (h > 0);
    } else {
      uint8_t *d;
      int16x8_t s11, s12, s13, s14;

      do {
        __builtin_prefetch(src + 0 * src_stride);
        __builtin_prefetch(src + 1 * src_stride);
        __builtin_prefetch(src + 2 * src_stride);
        __builtin_prefetch(src + 3 * src_stride);
        __builtin_prefetch(src + 4 * src_stride);
        __builtin_prefetch(src + 5 * src_stride);
        __builtin_prefetch(src + 6 * src_stride);
        __builtin_prefetch(src + 7 * src_stride);
        load_u8_8x8(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
        s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
        s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
        s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

        width = w;
        s = src + 7;
        d = dst;
        __builtin_prefetch(dst + 0 * dst_stride);
        __builtin_prefetch(dst + 1 * dst_stride);
        __builtin_prefetch(dst + 2 * dst_stride);
        __builtin_prefetch(dst + 3 * dst_stride);
        __builtin_prefetch(dst + 4 * dst_stride);
        __builtin_prefetch(dst + 5 * dst_stride);
        __builtin_prefetch(dst + 6 * dst_stride);
        __builtin_prefetch(dst + 7 * dst_stride);

        do {
          load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
          transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
          s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
          s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
          s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
          s10 = vreinterpretq_s16_u16(vmovl_u8(t3));
          s11 = vreinterpretq_s16_u16(vmovl_u8(t4));
          s12 = vreinterpretq_s16_u16(vmovl_u8(t5));
          s13 = vreinterpretq_s16_u16(vmovl_u8(t6));
          s14 = vreinterpretq_s16_u16(vmovl_u8(t7));

          t0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filters);
          t1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filters);
          t2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filters);
          t3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filters);
          t4 = convolve8_8(s4, s5, s6, s7, s8, s9, s10, s11, filters);
          t5 = convolve8_8(s5, s6, s7, s8, s9, s10, s11, s12, filters);
          t6 = convolve8_8(s6, s7, s8, s9, s10, s11, s12, s13, filters);
          t7 = convolve8_8(s7, s8, s9, s10, s11, s12, s13, s14, filters);

          transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
          store_u8_8x8(d, dst_stride, t0, t1, t2, t3, t4, t5, t6, t7);

          s0 = s8;
          s1 = s9;
          s2 = s10;
          s3 = s11;
          s4 = s12;
          s5 = s13;
          s6 = s14;
          s += 8;
          d += 8;
          width -= 8;
        } while (width != 0);
        src += 8 * src_stride;
        dst += 8 * dst_stride;
        h -= 8;
      } while (h > 0);
    }
  }
}

void vpx_convolve8_avg_horiz_neon(const uint8_t *src, ptrdiff_t src_stride,
                                  uint8_t *dst, ptrdiff_t dst_stride,
                                  const InterpKernel *filter, int x0_q4,
                                  int x_step_q4, int y0_q4, int y_step_q4,
                                  int w, int h) {
  const int16x8_t filters = vld1q_s16(filter[x0_q4]);
  uint8x8_t t0, t1, t2, t3;

  assert(!((intptr_t)dst & 3));
  assert(!(dst_stride & 3));
  assert(x_step_q4 == 16);

  (void)x_step_q4;
  (void)y0_q4;
  (void)y_step_q4;

  src -= 3;

  if (h == 4) {
    uint8x8_t d01, d23;
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;
    int16x8_t tt0, tt1, tt2, tt3;
    uint32x4_t d0123 = vdupq_n_u32(0);

    __builtin_prefetch(src + 0 * src_stride);
    __builtin_prefetch(src + 1 * src_stride);
    __builtin_prefetch(src + 2 * src_stride);
    __builtin_prefetch(src + 3 * src_stride);
    load_u8_8x4(src, src_stride, &t0, &t1, &t2, &t3);
    transpose_u8_8x4(&t0, &t1, &t2, &t3);
    tt0 = vreinterpretq_s16_u16(vmovl_u8(t0));
    tt1 = vreinterpretq_s16_u16(vmovl_u8(t1));
    tt2 = vreinterpretq_s16_u16(vmovl_u8(t2));
    tt3 = vreinterpretq_s16_u16(vmovl_u8(t3));
    s0 = vget_low_s16(tt0);
    s1 = vget_low_s16(tt1);
    s2 = vget_low_s16(tt2);
    s3 = vget_low_s16(tt3);
    s4 = vget_high_s16(tt0);
    s5 = vget_high_s16(tt1);
    s6 = vget_high_s16(tt2);
    __builtin_prefetch(dst + 0 * dst_stride);
    __builtin_prefetch(dst + 1 * dst_stride);
    __builtin_prefetch(dst + 2 * dst_stride);
    __builtin_prefetch(dst + 3 * dst_stride);
    src += 7;

    do {
      load_u8_8x4(src, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);
      tt0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      tt1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      tt2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      tt3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      s7 = vget_low_s16(tt0);
      s8 = vget_low_s16(tt1);
      s9 = vget_low_s16(tt2);
      s10 = vget_low_s16(tt3);

      d0 = convolve8_4(s0, s1, s2, s3, s4, s5, s6, s7, filters);
      d1 = convolve8_4(s1, s2, s3, s4, s5, s6, s7, s8, filters);
      d2 = convolve8_4(s2, s3, s4, s5, s6, s7, s8, s9, filters);
      d3 = convolve8_4(s3, s4, s5, s6, s7, s8, s9, s10, filters);

      d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), 7);
      d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), 7);
      transpose_u8_4x4(&d01, &d23);

      d0123 = vld1q_lane_u32((uint32_t *)(dst + 0 * dst_stride), d0123, 0);
      d0123 = vld1q_lane_u32((uint32_t *)(dst + 1 * dst_stride), d0123, 2);
      d0123 = vld1q_lane_u32((uint32_t *)(dst + 2 * dst_stride), d0123, 1);
      d0123 = vld1q_lane_u32((uint32_t *)(dst + 3 * dst_stride), d0123, 3);
      d0123 = vreinterpretq_u32_u8(
          vrhaddq_u8(vreinterpretq_u8_u32(d0123), vcombine_u8(d01, d23)));

      vst1q_lane_u32((uint32_t *)(dst + 0 * dst_stride), d0123, 0);
      vst1q_lane_u32((uint32_t *)(dst + 1 * dst_stride), d0123, 2);
      vst1q_lane_u32((uint32_t *)(dst + 2 * dst_stride), d0123, 1);
      vst1q_lane_u32((uint32_t *)(dst + 3 * dst_stride), d0123, 3);

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      src += 4;
      dst += 4;
      w -= 4;
    } while (w != 0);
  } else {
    int width;
    const uint8_t *s;
    uint8x8_t t4, t5, t6, t7;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;

    if (w == 4) {
      uint32x4_t d0415 = vdupq_n_u32(0);
      uint32x4_t d2637 = vdupq_n_u32(0);
      do {
        load_u8_8x8(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
        s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
        s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
        s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

        load_u8_8x8(src + 7, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6,
                    &t7);
        src += 8 * src_stride;
        __builtin_prefetch(dst + 0 * dst_stride);
        __builtin_prefetch(dst + 1 * dst_stride);
        __builtin_prefetch(dst + 2 * dst_stride);
        __builtin_prefetch(dst + 3 * dst_stride);
        __builtin_prefetch(dst + 4 * dst_stride);
        __builtin_prefetch(dst + 5 * dst_stride);
        __builtin_prefetch(dst + 6 * dst_stride);
        __builtin_prefetch(dst + 7 * dst_stride);
        transpose_u8_4x8(&t0, &t1, &t2, &t3, t4, t5, t6, t7);
        s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s10 = vreinterpretq_s16_u16(vmovl_u8(t3));

        __builtin_prefetch(src + 0 * src_stride);
        __builtin_prefetch(src + 1 * src_stride);
        __builtin_prefetch(src + 2 * src_stride);
        __builtin_prefetch(src + 3 * src_stride);
        __builtin_prefetch(src + 4 * src_stride);
        __builtin_prefetch(src + 5 * src_stride);
        __builtin_prefetch(src + 6 * src_stride);
        __builtin_prefetch(src + 7 * src_stride);
        t0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filters);
        t1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filters);
        t2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filters);
        t3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filters);

        transpose_u8_8x4(&t0, &t1, &t2, &t3);

        d0415 = vld1q_lane_u32((uint32_t *)(dst + 0 * dst_stride), d0415, 0);
        d0415 = vld1q_lane_u32((uint32_t *)(dst + 1 * dst_stride), d0415, 2);
        d2637 = vld1q_lane_u32((uint32_t *)(dst + 2 * dst_stride), d2637, 0);
        d2637 = vld1q_lane_u32((uint32_t *)(dst + 3 * dst_stride), d2637, 2);
        d0415 = vld1q_lane_u32((uint32_t *)(dst + 4 * dst_stride), d0415, 1);
        d0415 = vld1q_lane_u32((uint32_t *)(dst + 5 * dst_stride), d0415, 3);
        d2637 = vld1q_lane_u32((uint32_t *)(dst + 6 * dst_stride), d2637, 1);
        d2637 = vld1q_lane_u32((uint32_t *)(dst + 7 * dst_stride), d2637, 3);
        d0415 = vreinterpretq_u32_u8(
            vrhaddq_u8(vreinterpretq_u8_u32(d0415), vcombine_u8(t0, t1)));
        d2637 = vreinterpretq_u32_u8(
            vrhaddq_u8(vreinterpretq_u8_u32(d2637), vcombine_u8(t2, t3)));

        vst1q_lane_u32((uint32_t *)dst, d0415, 0);
        dst += dst_stride;
        vst1q_lane_u32((uint32_t *)dst, d0415, 2);
        dst += dst_stride;
        vst1q_lane_u32((uint32_t *)dst, d2637, 0);
        dst += dst_stride;
        vst1q_lane_u32((uint32_t *)dst, d2637, 2);
        dst += dst_stride;
        vst1q_lane_u32((uint32_t *)dst, d0415, 1);
        dst += dst_stride;
        vst1q_lane_u32((uint32_t *)dst, d0415, 3);
        dst += dst_stride;
        vst1q_lane_u32((uint32_t *)dst, d2637, 1);
        dst += dst_stride;
        vst1q_lane_u32((uint32_t *)dst, d2637, 3);
        dst += dst_stride;
        h -= 8;
      } while (h > 0);
    } else {
      uint8_t *d;
      int16x8_t s11, s12, s13, s14;
      uint8x16_t d01, d23, d45, d67;

      do {
        __builtin_prefetch(src + 0 * src_stride);
        __builtin_prefetch(src + 1 * src_stride);
        __builtin_prefetch(src + 2 * src_stride);
        __builtin_prefetch(src + 3 * src_stride);
        __builtin_prefetch(src + 4 * src_stride);
        __builtin_prefetch(src + 5 * src_stride);
        __builtin_prefetch(src + 6 * src_stride);
        __builtin_prefetch(src + 7 * src_stride);
        load_u8_8x8(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
        s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
        s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
        s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

        width = w;
        s = src + 7;
        d = dst;
        __builtin_prefetch(dst + 0 * dst_stride);
        __builtin_prefetch(dst + 1 * dst_stride);
        __builtin_prefetch(dst + 2 * dst_stride);
        __builtin_prefetch(dst + 3 * dst_stride);
        __builtin_prefetch(dst + 4 * dst_stride);
        __builtin_prefetch(dst + 5 * dst_stride);
        __builtin_prefetch(dst + 6 * dst_stride);
        __builtin_prefetch(dst + 7 * dst_stride);

        do {
          load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
          transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
          s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
          s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
          s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
          s10 = vreinterpretq_s16_u16(vmovl_u8(t3));
          s11 = vreinterpretq_s16_u16(vmovl_u8(t4));
          s12 = vreinterpretq_s16_u16(vmovl_u8(t5));
          s13 = vreinterpretq_s16_u16(vmovl_u8(t6));
          s14 = vreinterpretq_s16_u16(vmovl_u8(t7));

          t0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filters);
          t1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filters);
          t2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filters);
          t3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filters);
          t4 = convolve8_8(s4, s5, s6, s7, s8, s9, s10, s11, filters);
          t5 = convolve8_8(s5, s6, s7, s8, s9, s10, s11, s12, filters);
          t6 = convolve8_8(s6, s7, s8, s9, s10, s11, s12, s13, filters);
          t7 = convolve8_8(s7, s8, s9, s10, s11, s12, s13, s14, filters);

          transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

          d01 = vcombine_u8(vld1_u8(d + 0 * dst_stride),
                            vld1_u8(d + 1 * dst_stride));
          d23 = vcombine_u8(vld1_u8(d + 2 * dst_stride),
                            vld1_u8(d + 3 * dst_stride));
          d45 = vcombine_u8(vld1_u8(d + 4 * dst_stride),
                            vld1_u8(d + 5 * dst_stride));
          d67 = vcombine_u8(vld1_u8(d + 6 * dst_stride),
                            vld1_u8(d + 7 * dst_stride));
          d01 = vrhaddq_u8(d01, vcombine_u8(t0, t1));
          d23 = vrhaddq_u8(d23, vcombine_u8(t2, t3));
          d45 = vrhaddq_u8(d45, vcombine_u8(t4, t5));
          d67 = vrhaddq_u8(d67, vcombine_u8(t6, t7));

          store_u8_8x8(d, dst_stride, vget_low_u8(d01), vget_high_u8(d01),
                       vget_low_u8(d23), vget_high_u8(d23), vget_low_u8(d45),
                       vget_high_u8(d45), vget_low_u8(d67), vget_high_u8(d67));

          s0 = s8;
          s1 = s9;
          s2 = s10;
          s3 = s11;
          s4 = s12;
          s5 = s13;
          s6 = s14;
          s += 8;
          d += 8;
          width -= 8;
        } while (width != 0);
        src += 8 * src_stride;
        dst += 8 * dst_stride;
        h -= 8;
      } while (h > 0);
    }
  }
}

void vpx_convolve8_vert_neon(const uint8_t *src, ptrdiff_t src_stride,
                             uint8_t *dst, ptrdiff_t dst_stride,
                             const InterpKernel *filter, int x0_q4,
                             int x_step_q4, int y0_q4, int y_step_q4, int w,
                             int h) {
  const int16x8_t filters = vld1q_s16(filter[y0_q4]);

  assert(!((intptr_t)dst & 3));
  assert(!(dst_stride & 3));
  assert(y_step_q4 == 16);

  (void)x0_q4;
  (void)x_step_q4;
  (void)y_step_q4;

  src -= 3 * src_stride;

  if (w == 4) {
    uint8x8_t d01, d23;
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;

    s0 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s1 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s2 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s3 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s4 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s5 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s6 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;

    do {
      s7 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
      src += src_stride;
      s8 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
      src += src_stride;
      s9 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
      src += src_stride;
      s10 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
      src += src_stride;

      __builtin_prefetch(dst + 0 * dst_stride);
      __builtin_prefetch(dst + 1 * dst_stride);
      __builtin_prefetch(dst + 2 * dst_stride);
      __builtin_prefetch(dst + 3 * dst_stride);
      __builtin_prefetch(src + 0 * src_stride);
      __builtin_prefetch(src + 1 * src_stride);
      __builtin_prefetch(src + 2 * src_stride);
      __builtin_prefetch(src + 3 * src_stride);
      d0 = convolve8_4(s0, s1, s2, s3, s4, s5, s6, s7, filters);
      d1 = convolve8_4(s1, s2, s3, s4, s5, s6, s7, s8, filters);
      d2 = convolve8_4(s2, s3, s4, s5, s6, s7, s8, s9, filters);
      d3 = convolve8_4(s3, s4, s5, s6, s7, s8, s9, s10, filters);

      d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), 7);
      d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), 7);
      vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d01), 0);
      dst += dst_stride;
      vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d01), 1);
      dst += dst_stride;
      vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d23), 0);
      dst += dst_stride;
      vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d23), 1);
      dst += dst_stride;

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      h -= 4;
    } while (h != 0);
  } else {
    int height;
    const uint8_t *s;
    uint8_t *d;
    uint8x8_t t0, t1, t2, t3;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;

    do {
      __builtin_prefetch(src + 0 * src_stride);
      __builtin_prefetch(src + 1 * src_stride);
      __builtin_prefetch(src + 2 * src_stride);
      __builtin_prefetch(src + 3 * src_stride);
      __builtin_prefetch(src + 4 * src_stride);
      __builtin_prefetch(src + 5 * src_stride);
      __builtin_prefetch(src + 6 * src_stride);
      s = src;
      s0 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s1 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s2 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s3 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s4 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s5 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s6 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      d = dst;
      height = h;

      do {
        s7 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
        s += src_stride;
        s8 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
        s += src_stride;
        s9 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
        s += src_stride;
        s10 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
        s += src_stride;

        __builtin_prefetch(d + 0 * dst_stride);
        __builtin_prefetch(d + 1 * dst_stride);
        __builtin_prefetch(d + 2 * dst_stride);
        __builtin_prefetch(d + 3 * dst_stride);
        __builtin_prefetch(s + 0 * src_stride);
        __builtin_prefetch(s + 1 * src_stride);
        __builtin_prefetch(s + 2 * src_stride);
        __builtin_prefetch(s + 3 * src_stride);
        t0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filters);
        t1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filters);
        t2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filters);
        t3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filters);

        vst1_u8(d, t0);
        d += dst_stride;
        vst1_u8(d, t1);
        d += dst_stride;
        vst1_u8(d, t2);
        d += dst_stride;
        vst1_u8(d, t3);
        d += dst_stride;

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        height -= 4;
      } while (height != 0);
      src += 8;
      dst += 8;
      w -= 8;
    } while (w != 0);
  }
}

void vpx_convolve8_avg_vert_neon(const uint8_t *src, ptrdiff_t src_stride,
                                 uint8_t *dst, ptrdiff_t dst_stride,
                                 const InterpKernel *filter, int x0_q4,
                                 int x_step_q4, int y0_q4, int y_step_q4, int w,
                                 int h) {
  const int16x8_t filters = vld1q_s16(filter[y0_q4]);

  assert(!((intptr_t)dst & 3));
  assert(!(dst_stride & 3));
  assert(y_step_q4 == 16);

  (void)x0_q4;
  (void)x_step_q4;
  (void)y_step_q4;

  src -= 3 * src_stride;

  if (w == 4) {
    uint8x8_t d01, d23;
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;
    uint32x4_t d0123 = vdupq_n_u32(0);

    s0 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s1 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s2 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s3 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s4 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s5 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s6 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;

    do {
      s7 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
      src += src_stride;
      s8 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
      src += src_stride;
      s9 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
      src += src_stride;
      s10 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
      src += src_stride;

      __builtin_prefetch(dst + 0 * dst_stride);
      __builtin_prefetch(dst + 1 * dst_stride);
      __builtin_prefetch(dst + 2 * dst_stride);
      __builtin_prefetch(dst + 3 * dst_stride);
      __builtin_prefetch(src + 0 * src_stride);
      __builtin_prefetch(src + 1 * src_stride);
      __builtin_prefetch(src + 2 * src_stride);
      __builtin_prefetch(src + 3 * src_stride);
      d0 = convolve8_4(s0, s1, s2, s3, s4, s5, s6, s7, filters);
      d1 = convolve8_4(s1, s2, s3, s4, s5, s6, s7, s8, filters);
      d2 = convolve8_4(s2, s3, s4, s5, s6, s7, s8, s9, filters);
      d3 = convolve8_4(s3, s4, s5, s6, s7, s8, s9, s10, filters);

      d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), 7);
      d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), 7);

      d0123 = vld1q_lane_u32((uint32_t *)(dst + 0 * dst_stride), d0123, 0);
      d0123 = vld1q_lane_u32((uint32_t *)(dst + 1 * dst_stride), d0123, 1);
      d0123 = vld1q_lane_u32((uint32_t *)(dst + 2 * dst_stride), d0123, 2);
      d0123 = vld1q_lane_u32((uint32_t *)(dst + 3 * dst_stride), d0123, 3);
      d0123 = vreinterpretq_u32_u8(
          vrhaddq_u8(vreinterpretq_u8_u32(d0123), vcombine_u8(d01, d23)));

      vst1q_lane_u32((uint32_t *)dst, d0123, 0);
      dst += dst_stride;
      vst1q_lane_u32((uint32_t *)dst, d0123, 1);
      dst += dst_stride;
      vst1q_lane_u32((uint32_t *)dst, d0123, 2);
      dst += dst_stride;
      vst1q_lane_u32((uint32_t *)dst, d0123, 3);
      dst += dst_stride;

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      h -= 4;
    } while (h != 0);
  } else {
    int height;
    const uint8_t *s;
    uint8_t *d;
    uint8x8_t t0, t1, t2, t3;
    uint8x16_t d01, d23, dd01, dd23;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;

    do {
      __builtin_prefetch(src + 0 * src_stride);
      __builtin_prefetch(src + 1 * src_stride);
      __builtin_prefetch(src + 2 * src_stride);
      __builtin_prefetch(src + 3 * src_stride);
      __builtin_prefetch(src + 4 * src_stride);
      __builtin_prefetch(src + 5 * src_stride);
      __builtin_prefetch(src + 6 * src_stride);
      s = src;
      s0 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s1 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s2 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s3 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s4 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s5 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s6 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      d = dst;
      height = h;

      do {
        s7 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
        s += src_stride;
        s8 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
        s += src_stride;
        s9 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
        s += src_stride;
        s10 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
        s += src_stride;

        __builtin_prefetch(d + 0 * dst_stride);
        __builtin_prefetch(d + 1 * dst_stride);
        __builtin_prefetch(d + 2 * dst_stride);
        __builtin_prefetch(d + 3 * dst_stride);
        __builtin_prefetch(s + 0 * src_stride);
        __builtin_prefetch(s + 1 * src_stride);
        __builtin_prefetch(s + 2 * src_stride);
        __builtin_prefetch(s + 3 * src_stride);
        t0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filters);
        t1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filters);
        t2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filters);
        t3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filters);

        d01 = vcombine_u8(t0, t1);
        d23 = vcombine_u8(t2, t3);
        dd01 = vcombine_u8(vld1_u8(d + 0 * dst_stride),
                           vld1_u8(d + 1 * dst_stride));
        dd23 = vcombine_u8(vld1_u8(d + 2 * dst_stride),
                           vld1_u8(d + 3 * dst_stride));
        dd01 = vrhaddq_u8(dd01, d01);
        dd23 = vrhaddq_u8(dd23, d23);

        vst1_u8(d, vget_low_u8(dd01));
        d += dst_stride;
        vst1_u8(d, vget_high_u8(dd01));
        d += dst_stride;
        vst1_u8(d, vget_low_u8(dd23));
        d += dst_stride;
        vst1_u8(d, vget_high_u8(dd23));
        d += dst_stride;

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        height -= 4;
      } while (height != 0);
      src += 8;
      dst += 8;
      w -= 8;
    } while (w != 0);
  }
}

#endif  // #if defined(__aarch64__) &&
        //     (defined(__ARM_FEATURE_DOTPROD) ||
        //      defined(__ARM_FEATURE_MATMUL_INT8))
