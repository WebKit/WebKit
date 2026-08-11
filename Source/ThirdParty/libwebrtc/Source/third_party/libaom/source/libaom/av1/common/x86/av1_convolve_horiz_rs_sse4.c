/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <smmintrin.h>

#include "config/av1_rtcd.h"

#include "av1/common/convolve.h"
#include "av1/common/resize.h"
#include "aom_dsp/x86/synonyms.h"

// Note: If the crop width is not a multiple of 4, then, unlike the C version,
// this function will overwrite some of the padding on the right hand side of
// the frame. This padding appears to be trashed anyway, so this should not
// affect the running of the decoder.
void av1_convolve_horiz_rs_sse4_1(const uint8_t *src, int src_stride,
                                  uint8_t *dst, int dst_stride, int w, int h,
                                  const int16_t *x_filters, int x0_qn,
                                  int x_step_qn) {
  assert(UPSCALE_NORMATIVE_TAPS == 8);

  src -= UPSCALE_NORMATIVE_TAPS / 2 - 1;

  const __m128i round_add = _mm_set1_epi32((1 << FILTER_BITS) >> 1);
  const __m128i zero = _mm_setzero_si128();

  const uint8_t *src_y;
  uint8_t *dst_y;
  int x_qn = x0_qn;
  for (int x = 0; x < w; x += 4, x_qn += 4 * x_step_qn) {
    const int x_filter_idx0 =
        ((x_qn + 0 * x_step_qn) & RS_SCALE_SUBPEL_MASK) >> RS_SCALE_EXTRA_BITS;
    const int x_filter_idx1 =
        ((x_qn + 1 * x_step_qn) & RS_SCALE_SUBPEL_MASK) >> RS_SCALE_EXTRA_BITS;
    const int x_filter_idx2 =
        ((x_qn + 2 * x_step_qn) & RS_SCALE_SUBPEL_MASK) >> RS_SCALE_EXTRA_BITS;
    const int x_filter_idx3 =
        ((x_qn + 3 * x_step_qn) & RS_SCALE_SUBPEL_MASK) >> RS_SCALE_EXTRA_BITS;

    assert(x_filter_idx0 <= RS_SUBPEL_MASK);
    assert(x_filter_idx1 <= RS_SUBPEL_MASK);
    assert(x_filter_idx2 <= RS_SUBPEL_MASK);
    assert(x_filter_idx3 <= RS_SUBPEL_MASK);

    const int16_t *const x_filter0 =
        &x_filters[x_filter_idx0 * UPSCALE_NORMATIVE_TAPS];
    const int16_t *const x_filter1 =
        &x_filters[x_filter_idx1 * UPSCALE_NORMATIVE_TAPS];
    const int16_t *const x_filter2 =
        &x_filters[x_filter_idx2 * UPSCALE_NORMATIVE_TAPS];
    const int16_t *const x_filter3 =
        &x_filters[x_filter_idx3 * UPSCALE_NORMATIVE_TAPS];

    const __m128i fil0_16 = xx_loadu_128(x_filter0);
    const __m128i fil1_16 = xx_loadu_128(x_filter1);
    const __m128i fil2_16 = xx_loadu_128(x_filter2);
    const __m128i fil3_16 = xx_loadu_128(x_filter3);

    src_y = src;
    dst_y = dst;
    for (int y = 0; y < h; y++, src_y += src_stride, dst_y += dst_stride) {
      const uint8_t *const src_x0 =
          &src_y[(x_qn + 0 * x_step_qn) >> RS_SCALE_SUBPEL_BITS];
      const uint8_t *const src_x1 =
          &src_y[(x_qn + 1 * x_step_qn) >> RS_SCALE_SUBPEL_BITS];
      const uint8_t *const src_x2 =
          &src_y[(x_qn + 2 * x_step_qn) >> RS_SCALE_SUBPEL_BITS];
      const uint8_t *const src_x3 =
          &src_y[(x_qn + 3 * x_step_qn) >> RS_SCALE_SUBPEL_BITS];

      // Load up the source data. This is 8-bit input data, so each load
      // gets 8 pixels.
      const __m128i src0_8 = xx_loadl_64(src_x0);
      const __m128i src1_8 = xx_loadl_64(src_x1);
      const __m128i src2_8 = xx_loadl_64(src_x2);
      const __m128i src3_8 = xx_loadl_64(src_x3);

      // Now zero-extend up to 16-bit precision, i.e.
      // [ 00 00 00 00 hg fe dc ba ] -> [ 0h 0g 0f 0e 0d 0c 0b 0a ]
      const __m128i src0_16 = _mm_cvtepu8_epi16(src0_8);
      const __m128i src1_16 = _mm_cvtepu8_epi16(src1_8);
      const __m128i src2_16 = _mm_cvtepu8_epi16(src2_8);
      const __m128i src3_16 = _mm_cvtepu8_epi16(src3_8);

      // Multiply by filter coefficients (results in a 32-bit value),
      // and add adjacent pairs, i.e.
      // ([ s7 s6 s5 s4 s3 s2 s1 s0], [ f7 f6 f5 f4 f3 f2 f1 f0 ])
      // -> [ {s7*f7+s6*f6} {s5*f5+s4*f4} {s3*f3+s2*f2} {s1*f1+s0*f0} ]
      const __m128i conv0_32 = _mm_madd_epi16(src0_16, fil0_16);
      const __m128i conv1_32 = _mm_madd_epi16(src1_16, fil1_16);
      const __m128i conv2_32 = _mm_madd_epi16(src2_16, fil2_16);
      const __m128i conv3_32 = _mm_madd_epi16(src3_16, fil3_16);

      // Reduce horizontally and add, i.e.
      // ([ D C B A ], [ S R Q P ]) -> [ S+R Q+P D+C B+A ]
      const __m128i conv01_32 = _mm_hadd_epi32(conv0_32, conv1_32);
      const __m128i conv23_32 = _mm_hadd_epi32(conv2_32, conv3_32);

      const __m128i conv0123_32 = _mm_hadd_epi32(conv01_32, conv23_32);

      // Divide down by (1 << FILTER_BITS), rounding to nearest.
      const __m128i shifted_32 =
          _mm_srai_epi32(_mm_add_epi32(conv0123_32, round_add), FILTER_BITS);

      // Pack 32-bit values into 16-bit values, i.e.
      // ([ D C B A ], [ 0 0 0 0 ]) -> [ 0 0 0 0 D C B A ]
      const __m128i shifted_16 = _mm_packus_epi32(shifted_32, zero);

      // Pack 16-bit values into 8-bit values, i.e.
      // ([ 0 0 0 0 D C B A ], [ 0 0 0 0 0 0 0 0 ])
      // -> [ 0 0 0 0 0 0 DC BA ]
      const __m128i shifted_8 = _mm_packus_epi16(shifted_16, zero);

      // Write to the output
      xx_storel_32(&dst_y[x], shifted_8);
    }
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
// Note: If the crop width is not a multiple of 4, then, unlike the C version,
// this function will overwrite some of the padding on the right hand side of
// the frame. This padding appears to be trashed anyway, so this should not
// affect the running of the decoder.
void av1_highbd_convolve_horiz_rs_sse4_1(const uint16_t *src, int src_stride,
                                         uint16_t *dst, int dst_stride, int w,
                                         int h, const int16_t *x_filters,
                                         int x0_qn, int x_step_qn, int bd) {
  assert(UPSCALE_NORMATIVE_TAPS == 8);
  assert(bd == 8 || bd == 10 || bd == 12);

  src -= UPSCALE_NORMATIVE_TAPS / 2 - 1;

  const __m128i round_add = _mm_set1_epi32((1 << FILTER_BITS) >> 1);
  const __m128i zero = _mm_setzero_si128();
  const __m128i clip_maximum = _mm_set1_epi16((1 << bd) - 1);

  const uint16_t *src_y;
  uint16_t *dst_y;
  int x_qn = x0_qn;
  for (int x = 0; x < w; x += 4, x_qn += 4 * x_step_qn) {
    const int x_filter_idx0 =
        ((x_qn + 0 * x_step_qn) & RS_SCALE_SUBPEL_MASK) >> RS_SCALE_EXTRA_BITS;
    const int x_filter_idx1 =
        ((x_qn + 1 * x_step_qn) & RS_SCALE_SUBPEL_MASK) >> RS_SCALE_EXTRA_BITS;
    const int x_filter_idx2 =
        ((x_qn + 2 * x_step_qn) & RS_SCALE_SUBPEL_MASK) >> RS_SCALE_EXTRA_BITS;
    const int x_filter_idx3 =
        ((x_qn + 3 * x_step_qn) & RS_SCALE_SUBPEL_MASK) >> RS_SCALE_EXTRA_BITS;

    assert(x_filter_idx0 <= RS_SUBPEL_MASK);
    assert(x_filter_idx1 <= RS_SUBPEL_MASK);
    assert(x_filter_idx2 <= RS_SUBPEL_MASK);
    assert(x_filter_idx3 <= RS_SUBPEL_MASK);

    const int16_t *const x_filter0 =
        &x_filters[x_filter_idx0 * UPSCALE_NORMATIVE_TAPS];
    const int16_t *const x_filter1 =
        &x_filters[x_filter_idx1 * UPSCALE_NORMATIVE_TAPS];
    const int16_t *const x_filter2 =
        &x_filters[x_filter_idx2 * UPSCALE_NORMATIVE_TAPS];
    const int16_t *const x_filter3 =
        &x_filters[x_filter_idx3 * UPSCALE_NORMATIVE_TAPS];

    const __m128i fil0_16 = xx_loadu_128(x_filter0);
    const __m128i fil1_16 = xx_loadu_128(x_filter1);
    const __m128i fil2_16 = xx_loadu_128(x_filter2);
    const __m128i fil3_16 = xx_loadu_128(x_filter3);

    src_y = src;
    dst_y = dst;
    for (int y = 0; y < h; y++, src_y += src_stride, dst_y += dst_stride) {
      const uint16_t *const src_x0 =
          &src_y[(x_qn + 0 * x_step_qn) >> RS_SCALE_SUBPEL_BITS];
      const uint16_t *const src_x1 =
          &src_y[(x_qn + 1 * x_step_qn) >> RS_SCALE_SUBPEL_BITS];
      const uint16_t *const src_x2 =
          &src_y[(x_qn + 2 * x_step_qn) >> RS_SCALE_SUBPEL_BITS];
      const uint16_t *const src_x3 =
          &src_y[(x_qn + 3 * x_step_qn) >> RS_SCALE_SUBPEL_BITS];

      // Load up the source data. This is 16-bit input data, so each load
      // gets 8 pixels.
      const __m128i src0_16 = xx_loadu_128(src_x0);
      const __m128i src1_16 = xx_loadu_128(src_x1);
      const __m128i src2_16 = xx_loadu_128(src_x2);
      const __m128i src3_16 = xx_loadu_128(src_x3);

      // Multiply by filter coefficients (results in a 32-bit value),
      // and add adjacent pairs, i.e.
      // ([ s7 s6 s5 s4 s3 s2 s1 s0], [ f7 f6 f5 f4 f3 f2 f1 f0 ])
      // -> [ {s7*f7+s6*f6} {s5*f5+s4*f4} {s3*f3+s2*f2} {s1*f1+s0*f0} ]
      const __m128i conv0_32 = _mm_madd_epi16(src0_16, fil0_16);
      const __m128i conv1_32 = _mm_madd_epi16(src1_16, fil1_16);
      const __m128i conv2_32 = _mm_madd_epi16(src2_16, fil2_16);
      const __m128i conv3_32 = _mm_madd_epi16(src3_16, fil3_16);

      // Reduce horizontally and add, i.e.
      // ([ D C B A ], [ S R Q P ]) -> [ S+R Q+P D+C B+A ]
      const __m128i conv01_32 = _mm_hadd_epi32(conv0_32, conv1_32);
      const __m128i conv23_32 = _mm_hadd_epi32(conv2_32, conv3_32);

      const __m128i conv0123_32 = _mm_hadd_epi32(conv01_32, conv23_32);

      // Divide down by (1 << FILTER_BITS), rounding to nearest.
      const __m128i shifted_32 =
          _mm_srai_epi32(_mm_add_epi32(conv0123_32, round_add), FILTER_BITS);

      // Pack 32-bit values into 16-bit values, i.e.
      // ([ D C B A ], [ 0 0 0 0 ]) -> [ 0 0 0 0 D C B A ]
      const __m128i shifted_16 = _mm_packus_epi32(shifted_32, zero);

      // Clip the values at (1 << bd) - 1
      const __m128i clipped_16 = _mm_min_epi16(shifted_16, clip_maximum);

      // Write to the output
      xx_storel_64(&dst_y[x], clipped_16);
    }
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH
