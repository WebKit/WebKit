/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <immintrin.h>

#include "config/av1_rtcd.h"

#include "aom_dsp/x86/convolve_avx2.h"
#include "aom_dsp/x86/convolve_common_intrin.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "aom_dsp/x86/synonyms.h"
#include "av1/common/convolve.h"

void av1_convolve_2d_sr_avx2(const uint8_t *src, int src_stride, uint8_t *dst,
                             int dst_stride, int w, int h,
                             const InterpFilterParams *filter_params_x,
                             const InterpFilterParams *filter_params_y,
                             const int subpel_x_qn, const int subpel_y_qn,
                             ConvolveParams *conv_params) {
  if (filter_params_x->taps > 8) {
    const int bd = 8;
    int im_stride = 8, i;
    DECLARE_ALIGNED(32, int16_t, im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * 8]);
    const int bits =
        FILTER_BITS * 2 - conv_params->round_0 - conv_params->round_1;
    const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;

    assert(conv_params->round_0 > 0);

    const __m256i round_const_h12 = _mm256_set1_epi32(
        ((1 << (conv_params->round_0)) >> 1) + (1 << (bd + FILTER_BITS - 1)));
    const __m128i round_shift_h12 = _mm_cvtsi32_si128(conv_params->round_0);

    const __m256i sum_round_v = _mm256_set1_epi32(
        (1 << offset_bits) + ((1 << conv_params->round_1) >> 1));
    const __m128i sum_shift_v = _mm_cvtsi32_si128(conv_params->round_1);

    const __m256i round_const_v = _mm256_set1_epi32(
        ((1 << bits) >> 1) - (1 << (offset_bits - conv_params->round_1)) -
        ((1 << (offset_bits - conv_params->round_1)) >> 1));
    const __m128i round_shift_v = _mm_cvtsi32_si128(bits);

    __m256i coeffs_h[6] = { 0 }, coeffs_v[6] = { 0 };

    int horiz_tap = 12;
    int vert_tap = 12;

    prepare_coeffs_12taps(filter_params_x, subpel_x_qn, coeffs_h);
    prepare_coeffs_12taps(filter_params_y, subpel_y_qn, coeffs_v);

    int im_h = h + vert_tap - 1;
    const int fo_vert = vert_tap / 2 - 1;
    const int fo_horiz = horiz_tap / 2 - 1;
    const uint8_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

    for (int j = 0; j < w; j += 8) {
      CONVOLVE_SR_HORIZONTAL_FILTER_12TAP
      CONVOLVE_SR_VERTICAL_FILTER_12TAP
    }
  } else {
    const int bd = 8;
    int im_stride = 8, i;
    DECLARE_ALIGNED(32, int16_t, im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * 8]);
    const int bits =
        FILTER_BITS * 2 - conv_params->round_0 - conv_params->round_1;
    const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;

    assert(conv_params->round_0 > 0);

    const __m256i round_const_h =
        _mm256_set1_epi16(((1 << (conv_params->round_0 - 1)) >> 1) +
                          (1 << (bd + FILTER_BITS - 2)));
    const __m128i round_shift_h = _mm_cvtsi32_si128(conv_params->round_0 - 1);

    const __m256i sum_round_v = _mm256_set1_epi32(
        (1 << offset_bits) + ((1 << conv_params->round_1) >> 1));
    const __m128i sum_shift_v = _mm_cvtsi32_si128(conv_params->round_1);

    const __m256i round_const_v = _mm256_set1_epi32(
        ((1 << bits) >> 1) - (1 << (offset_bits - conv_params->round_1)) -
        ((1 << (offset_bits - conv_params->round_1)) >> 1));
    const __m128i round_shift_v = _mm_cvtsi32_si128(bits);

    __m256i filt[4], coeffs_h[4], coeffs_v[4];

    filt[0] = _mm256_load_si256((__m256i const *)(filt_global_avx2));
    filt[1] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32));

    prepare_coeffs_lowbd(filter_params_x, subpel_x_qn, coeffs_h);
    prepare_coeffs(filter_params_y, subpel_y_qn, coeffs_v);

    const int16_t *const filter_x = av1_get_interp_filter_subpel_kernel(
        filter_params_x, subpel_x_qn & SUBPEL_MASK);
    const int16_t *const filter_y = av1_get_interp_filter_subpel_kernel(
        filter_params_y, subpel_y_qn & SUBPEL_MASK);

    int horiz_tap = SUBPEL_TAPS;
    int vert_tap = SUBPEL_TAPS;

    if (!(filter_x[0] | filter_x[1] | filter_x[6] | filter_x[7]))
      horiz_tap = 4;
    else if (!(filter_x[0] | filter_x[7]))
      horiz_tap = 6;

    if (!(filter_y[0] | filter_y[1] | filter_y[6] | filter_y[7]))
      vert_tap = 4;
    else if (!(filter_y[0] | filter_y[7]))
      vert_tap = 6;

    if (horiz_tap == 6)
      prepare_coeffs_6t_lowbd(filter_params_x, subpel_x_qn, coeffs_h);
    else
      prepare_coeffs_lowbd(filter_params_x, subpel_x_qn, coeffs_h);

    if (vert_tap == 6)
      prepare_coeffs_6t(filter_params_y, subpel_y_qn, coeffs_v);
    else
      prepare_coeffs(filter_params_y, subpel_y_qn, coeffs_v);

    int im_h = h + vert_tap - 1;
    const int fo_vert = vert_tap / 2 - 1;
    const int fo_horiz = horiz_tap / 2 - 1;
    const uint8_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

    filt[2] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 2));
    filt[3] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 3));

    for (int j = 0; j < w; j += 8) {
      if (horiz_tap == 4) {
        CONVOLVE_SR_HORIZONTAL_FILTER_4TAP
      } else if (horiz_tap == 6) {
        CONVOLVE_SR_HORIZONTAL_FILTER_6TAP
      } else {
        CONVOLVE_SR_HORIZONTAL_FILTER_8TAP
      }

      if (vert_tap == 4) {
        CONVOLVE_SR_VERTICAL_FILTER_4TAP
      } else if (vert_tap == 6) {
        CONVOLVE_SR_VERTICAL_FILTER_6TAP
      } else {
        CONVOLVE_SR_VERTICAL_FILTER_8TAP
      }
    }
  }
}
