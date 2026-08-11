/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <immintrin.h>
#include <stdint.h>

#include "config/av1_rtcd.h"

#include "aom_dsp/x86/convolve_avx2.h"
#include "aom_dsp/aom_filter.h"
#include "aom_dsp/x86/synonyms.h"

#include "av1/common/convolve.h"

static void convolve_2d_sr_w4_avx2(
    const uint8_t *src, int32_t src_stride, uint8_t *dst, int32_t dst_stride,
    int32_t w, int32_t h, const InterpFilterParams *filter_params_x,
    const InterpFilterParams *filter_params_y, const int32_t subpel_x_qn,
    const int32_t subpel_y_qn, ConvolveParams *conv_params) {
  int i;
  DECLARE_ALIGNED(32, int16_t, im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * 4]);
  uint8_t *dst_ptr = dst;
  assert(conv_params->round_0 == 3);
  assert(conv_params->round_1 == 11);

  const __m128i round_const_h = _mm_set1_epi16(1 << (conv_params->round_0 - 2));
  const __m256i round_const_v =
      _mm256_set1_epi32(1 << (conv_params->round_1 - 1));

  __m128i filt[2], coeffs_h[2] = { 0 };
  __m256i coeffs_v[4] = { 0 };

  const int horiz_tap = get_filter_tap(filter_params_x, subpel_x_qn);
  const int vert_tap = get_filter_tap(filter_params_y, subpel_y_qn);

  assert(horiz_tap == 2 || horiz_tap == 4);
  assert(vert_tap == 2 || vert_tap == 4 || vert_tap == 6 || vert_tap == 8);

  if (horiz_tap == 2)
    prepare_coeffs_2t_ssse3(filter_params_x, subpel_x_qn, coeffs_h);
  else
    prepare_coeffs_4t_ssse3(filter_params_x, subpel_x_qn, coeffs_h);

  if (vert_tap == 2)
    prepare_coeffs_2t(filter_params_y, subpel_y_qn, coeffs_v);
  else if (vert_tap == 4)
    prepare_coeffs_4t(filter_params_y, subpel_y_qn, coeffs_v);
  else if (vert_tap == 6)
    prepare_coeffs_6t(filter_params_y, subpel_y_qn, coeffs_v);
  else
    prepare_coeffs(filter_params_y, subpel_y_qn, coeffs_v);

  int im_h = h + vert_tap - 1;
  const int fo_vert = vert_tap / 2 - 1;
  const int fo_horiz = horiz_tap / 2 - 1;
  const uint8_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

  filt[0] = _mm_load_si128((__m128i const *)filt1_global_sse2);
  filt[1] = _mm_load_si128((__m128i const *)filt2_global_sse2);

  if (horiz_tap == 2) {
    CONVOLVE_SR_HOR_FILTER_2TAP_W4
  } else {
    CONVOLVE_SR_HOR_FILTER_4TAP_W4
  }

  if (vert_tap == 2) {
    CONVOLVE_SR_VER_FILTER_2TAP_W4
  } else if (vert_tap == 4) {
    CONVOLVE_SR_VER_FILTER_4TAP_W4
  } else if (vert_tap == 6) {
    CONVOLVE_SR_VER_FILTER_6TAP_W4
  } else {
    CONVOLVE_SR_VER_FILTER_8TAP_W4
  }
}

static void convolve_2d_sr_avx2(const uint8_t *src, int src_stride,
                                uint8_t *dst, int dst_stride, int w, int h,
                                const InterpFilterParams *filter_params_x,
                                const InterpFilterParams *filter_params_y,
                                const int subpel_x_qn, const int subpel_y_qn,
                                ConvolveParams *conv_params) {
  if (filter_params_x->taps > 8) {
    const int bd = 8;
    int im_stride = 8, i;
    const int strip_stride = (MAX_SB_SIZE + MAX_FILTER_TAP) * 8;
    DECLARE_ALIGNED(
        32, int16_t,
        im_block_buf[(MAX_SB_SIZE / 8) * (MAX_SB_SIZE + MAX_FILTER_TAP) * 8]);
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

    const __m256i v_zero = _mm256_setzero_si256();
    __m256i s[12];
    if (w <= 4) {
      for (i = 0; i < im_h; i += 2) {
        for (int j = 0; j < w; j += 8) {
          int16_t *strip_im_block = &im_block_buf[(j / 8) * strip_stride];
          const __m256i data = _mm256_permute2x128_si256(
              _mm256_castsi128_si256(
                  _mm_loadu_si128((__m128i *)(&src_ptr[i * src_stride + j]))),
              _mm256_castsi128_si256(_mm_loadu_si128(
                  (__m128i *)(&src_ptr[i * src_stride + src_stride + j]))),
              0x20);
          const __m256i s_16lo = _mm256_unpacklo_epi8(data, v_zero);
          const __m256i s_16hi = _mm256_unpackhi_epi8(data, v_zero);
          const __m256i s_lolo = _mm256_unpacklo_epi16(s_16lo, s_16lo);
          const __m256i s_lohi = _mm256_unpackhi_epi16(s_16lo, s_16lo);

          const __m256i s_hilo = _mm256_unpacklo_epi16(s_16hi, s_16hi);
          const __m256i s_hihi = _mm256_unpackhi_epi16(s_16hi, s_16hi);

          s[0] = _mm256_alignr_epi8(s_lohi, s_lolo, 2);
          s[1] = _mm256_alignr_epi8(s_lohi, s_lolo, 10);
          s[2] = _mm256_alignr_epi8(s_hilo, s_lohi, 2);
          s[3] = _mm256_alignr_epi8(s_hilo, s_lohi, 10);
          s[4] = _mm256_alignr_epi8(s_hihi, s_hilo, 2);
          s[5] = _mm256_alignr_epi8(s_hihi, s_hilo, 10);

          const __m256i res_lo = convolve_12taps(s, coeffs_h);

          __m256i res_32b_lo = _mm256_sra_epi32(
              _mm256_add_epi32(res_lo, round_const_h12), round_shift_h12);
          __m256i res_16b_lo = _mm256_packs_epi32(res_32b_lo, res_32b_lo);
          const __m128i res_0 = _mm256_extracti128_si256(res_16b_lo, 0);
          const __m128i res_1 = _mm256_extracti128_si256(res_16b_lo, 1);
          if (w > 2) {
            _mm_storel_epi64((__m128i *)&strip_im_block[i * im_stride], res_0);
            _mm_storel_epi64(
                (__m128i *)&strip_im_block[i * im_stride + im_stride], res_1);
          } else {
            uint32_t horiz_2;
            horiz_2 = (uint32_t)_mm_cvtsi128_si32(res_0);
            strip_im_block[i * im_stride] = (uint16_t)horiz_2;
            strip_im_block[i * im_stride + 1] = (uint16_t)(horiz_2 >> 16);
            horiz_2 = (uint32_t)_mm_cvtsi128_si32(res_1);
            strip_im_block[i * im_stride + im_stride] = (uint16_t)horiz_2;
            strip_im_block[i * im_stride + im_stride + 1] =
                (uint16_t)(horiz_2 >> 16);
          }
        }
      }
    } else {
      for (i = 0; i < im_h; i++) {
        for (int j = 0; j < w; j += 8) {
          int16_t *strip_im_block = &im_block_buf[(j / 8) * strip_stride];
          const __m256i data = _mm256_permute2x128_si256(
              _mm256_castsi128_si256(
                  _mm_loadu_si128((__m128i *)(&src_ptr[i * src_stride + j]))),
              _mm256_castsi128_si256(_mm_loadu_si128(
                  (__m128i *)(&src_ptr[i * src_stride + j + 4]))),
              0x20);
          const __m256i s_16lo = _mm256_unpacklo_epi8(data, v_zero);
          const __m256i s_16hi = _mm256_unpackhi_epi8(data, v_zero);

          const __m256i s_lolo = _mm256_unpacklo_epi16(s_16lo, s_16lo);
          const __m256i s_lohi = _mm256_unpackhi_epi16(s_16lo, s_16lo);

          const __m256i s_hilo = _mm256_unpacklo_epi16(s_16hi, s_16hi);
          const __m256i s_hihi = _mm256_unpackhi_epi16(s_16hi, s_16hi);

          s[0] = _mm256_alignr_epi8(s_lohi, s_lolo, 2);
          s[1] = _mm256_alignr_epi8(s_lohi, s_lolo, 10);
          s[2] = _mm256_alignr_epi8(s_hilo, s_lohi, 2);
          s[3] = _mm256_alignr_epi8(s_hilo, s_lohi, 10);
          s[4] = _mm256_alignr_epi8(s_hihi, s_hilo, 2);
          s[5] = _mm256_alignr_epi8(s_hihi, s_hilo, 10);

          const __m256i res_lo = convolve_12taps(s, coeffs_h);

          __m256i res_32b_lo = _mm256_sra_epi32(
              _mm256_add_epi32(res_lo, round_const_h12), round_shift_h12);

          __m256i res_16b_lo = _mm256_packs_epi32(res_32b_lo, res_32b_lo);
          _mm_store_si128((__m128i *)&strip_im_block[i * im_stride],
                          _mm256_extracti128_si256(
                              _mm256_permute4x64_epi64(res_16b_lo, 0x88), 0));
        }
      }
    }

    for (int j = 0; j < w; j += 8) {
      const int16_t *im_block = &im_block_buf[(j / 8) * strip_stride];
      CONVOLVE_SR_VERTICAL_FILTER_12TAP
    }
  } else {
    int im_stride = 8, i;
    const int strip_stride = (MAX_SB_SIZE + MAX_FILTER_TAP) * 8;
    DECLARE_ALIGNED(
        32, int16_t,
        im_block_buf[(MAX_SB_SIZE / 8) * (MAX_SB_SIZE + MAX_FILTER_TAP) * 8]);

    assert(conv_params->round_0 == 3);
    assert(conv_params->round_1 == 11);

    const __m256i round_const_h =
        _mm256_set1_epi16(1 << (conv_params->round_0 - 2));
    const __m256i round_const_v =
        _mm256_set1_epi32(1 << (conv_params->round_1 - 1));

    __m256i filt[4], coeffs_h[4] = { 0 }, coeffs_v[4] = { 0 };

    int horiz_tap = get_filter_tap(filter_params_x, subpel_x_qn);
    int vert_tap = get_filter_tap(filter_params_y, subpel_y_qn);

    assert(horiz_tap == 2 || horiz_tap == 4 || horiz_tap == 6 ||
           horiz_tap == 8);
    assert(vert_tap == 2 || vert_tap == 4 || vert_tap == 6 || vert_tap == 8);

    if (horiz_tap == 2)
      prepare_coeffs_2t_lowbd(filter_params_x, subpel_x_qn, coeffs_h);
    else if (horiz_tap == 4)
      prepare_coeffs_4t_lowbd(filter_params_x, subpel_x_qn, coeffs_h);
    else if (horiz_tap == 6)
      prepare_coeffs_6t_lowbd(filter_params_x, subpel_x_qn, coeffs_h);
    else
      prepare_coeffs_lowbd(filter_params_x, subpel_x_qn, coeffs_h);

    if (vert_tap == 2)
      prepare_coeffs_2t(filter_params_y, subpel_y_qn, coeffs_v);
    else if (vert_tap == 4)
      prepare_coeffs_4t(filter_params_y, subpel_y_qn, coeffs_v);
    else if (vert_tap == 6)
      prepare_coeffs_6t(filter_params_y, subpel_y_qn, coeffs_v);
    else
      prepare_coeffs(filter_params_y, subpel_y_qn, coeffs_v);

    int im_h = h + vert_tap - 1;
    const int fo_vert = vert_tap / 2 - 1;
    const int fo_horiz = horiz_tap / 2 - 1;
    const uint8_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

    filt[0] = _mm256_load_si256((__m256i const *)filt1_global_avx2);
    filt[1] = _mm256_load_si256((__m256i const *)filt2_global_avx2);
    filt[2] = _mm256_load_si256((__m256i const *)filt3_global_avx2);
    filt[3] = _mm256_load_si256((__m256i const *)filt4_global_avx2);

    if (subpel_x_qn == 0 && subpel_y_qn == 0) {
      for (i = 0; i < h; ++i) {
        for (int j = 0; j < w; j += 8) {
          _mm_storel_epi64(
              (__m128i *)&dst[i * dst_stride + j],
              _mm_loadl_epi64((const __m128i *)&src[i * src_stride + j]));
        }
      }
      return;
    }

    for (i = 0; i < (im_h - 1); i += 2) {
      const uint8_t *src_row0 = &src_ptr[i * src_stride];
      const uint8_t *src_row1 = &src_ptr[(i + 1) * src_stride];
      for (int j = 0; j < w; j += 8) {
        int16_t *strip_im_block = &im_block_buf[(j / 8) * strip_stride];
        __m256i data =
            _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)&src_row0[j]));
        data = _mm256_inserti128_si256(
            data, _mm_loadu_si128((__m128i *)&src_row1[j]), 1);

        __m256i res;
        if (horiz_tap == 2)
          res = convolve_lowbd_x_2tap(data, coeffs_h, filt);
        else if (horiz_tap == 4)
          res = convolve_lowbd_x_4tap(data, coeffs_h, filt);
        else if (horiz_tap == 6)
          res = convolve_lowbd_x_6tap(data, coeffs_h, filt);
        else
          res = convolve_lowbd_x(data, coeffs_h, filt);

        res = _mm256_srai_epi16(_mm256_add_epi16(res, round_const_h), 2);
        _mm256_store_si256((__m256i *)&strip_im_block[i * 8], res);
      }
    }
    {
      const uint8_t *src_row0 = &src_ptr[i * src_stride];
      for (int j = 0; j < w; j += 8) {
        int16_t *strip_im_block = &im_block_buf[(j / 8) * strip_stride];
        __m256i data_1 =
            _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)&src_row0[j]));
        __m256i res;
        if (horiz_tap == 2)
          res = convolve_lowbd_x_2tap(data_1, coeffs_h, filt);
        else if (horiz_tap == 4)
          res = convolve_lowbd_x_4tap(data_1, coeffs_h, filt);
        else if (horiz_tap == 6)
          res = convolve_lowbd_x_6tap(data_1, coeffs_h, filt);
        else
          res = convolve_lowbd_x(data_1, coeffs_h, filt);

        res = _mm256_srai_epi16(_mm256_add_epi16(res, round_const_h), 2);
        _mm_store_si128((__m128i *)&strip_im_block[i * 8],
                        _mm256_castsi256_si128(res));
      }
    }

    for (int j = 0; j < w; j += 8) {
      const int16_t *im_block = &im_block_buf[(j / 8) * strip_stride];
      uint8_t *dst_ptr = dst + j;
      if (vert_tap == 2) {
        CONVOLVE_SR_VERTICAL_FILTER_2TAP
      } else if (vert_tap == 4) {
        CONVOLVE_SR_VERTICAL_FILTER_4TAP
      } else if (vert_tap == 6) {
        CONVOLVE_SR_VERTICAL_FILTER_6TAP
      } else {
        CONVOLVE_SR_VERTICAL_FILTER_8TAP
      }
    }
  }
}

void av1_convolve_2d_sr_avx2(
    const uint8_t *src, int32_t src_stride, uint8_t *dst, int32_t dst_stride,
    int32_t w, int32_t h, const InterpFilterParams *filter_params_x,
    const InterpFilterParams *filter_params_y, const int32_t subpel_x_qn,
    const int32_t subpel_y_qn, ConvolveParams *conv_params) {
  const int32_t tap_x = get_filter_tap(filter_params_x, subpel_x_qn);
  const int32_t tap_y = get_filter_tap(filter_params_y, subpel_y_qn);

  const bool use_12tap = (tap_x == 12 || tap_y == 12);
  if (w <= 4 && !use_12tap) {
    convolve_2d_sr_w4_avx2(src, src_stride, dst, dst_stride, w, h,
                           filter_params_x, filter_params_y, subpel_x_qn,
                           subpel_y_qn, conv_params);
  } else {
    convolve_2d_sr_avx2(src, src_stride, dst, dst_stride, w, h, filter_params_x,
                        filter_params_y, subpel_x_qn, subpel_y_qn, conv_params);
  }
}
