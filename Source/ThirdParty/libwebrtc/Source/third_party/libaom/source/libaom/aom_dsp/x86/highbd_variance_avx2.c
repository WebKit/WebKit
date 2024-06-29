/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <immintrin.h>  // AVX2

#include "config/aom_dsp_rtcd.h"
#include "aom_dsp/aom_filter.h"
#include "aom_dsp/x86/synonyms.h"

typedef void (*high_variance_fn_t)(const uint16_t *src, int src_stride,
                                   const uint16_t *ref, int ref_stride,
                                   uint32_t *sse, int *sum);

static uint32_t aom_highbd_var_filter_block2d_bil_avx2(
    const uint8_t *src_ptr8, unsigned int src_pixels_per_line, int pixel_step,
    unsigned int output_height, unsigned int output_width,
    const uint32_t xoffset, const uint32_t yoffset, const uint8_t *dst_ptr8,
    int dst_stride, uint32_t *sse) {
  const __m256i filter1 =
      _mm256_set1_epi32((int)(bilinear_filters_2t[xoffset][1] << 16) |
                        bilinear_filters_2t[xoffset][0]);
  const __m256i filter2 =
      _mm256_set1_epi32((int)(bilinear_filters_2t[yoffset][1] << 16) |
                        bilinear_filters_2t[yoffset][0]);
  const __m256i one = _mm256_set1_epi16(1);
  const int bitshift = 0x40;
  (void)pixel_step;
  unsigned int i, j, prev = 0, curr = 2;
  uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src_ptr8);
  uint16_t *dst_ptr = CONVERT_TO_SHORTPTR(dst_ptr8);
  uint16_t *src_ptr_ref = src_ptr;
  uint16_t *dst_ptr_ref = dst_ptr;
  int64_t sum_long = 0;
  uint64_t sse_long = 0;
  unsigned int rshift = 0, inc = 1;
  __m256i rbias = _mm256_set1_epi32(bitshift);
  __m256i opointer[8];
  unsigned int range;
  if (xoffset == 0) {
    if (yoffset == 0) {  // xoffset==0 && yoffset==0
      range = output_width / 16;
      if (output_height == 8) inc = 2;
      if (output_height == 4) inc = 4;
      for (j = 0; j < range * output_height * inc / 16; j++) {
        if (j % (output_height * inc / 16) == 0) {
          src_ptr = src_ptr_ref;
          src_ptr_ref += 16;
          dst_ptr = dst_ptr_ref;
          dst_ptr_ref += 16;
        }
        __m256i sum1 = _mm256_setzero_si256();
        __m256i sse1 = _mm256_setzero_si256();
        for (i = 0; i < 16 / inc; ++i) {
          __m256i V_S_SRC = _mm256_loadu_si256((const __m256i *)src_ptr);
          src_ptr += src_pixels_per_line;
          __m256i V_D_DST = _mm256_loadu_si256((const __m256i *)dst_ptr);
          dst_ptr += dst_stride;

          __m256i V_R_SUB = _mm256_sub_epi16(V_S_SRC, V_D_DST);
          __m256i V_R_MAD = _mm256_madd_epi16(V_R_SUB, V_R_SUB);

          sum1 = _mm256_add_epi16(sum1, V_R_SUB);
          sse1 = _mm256_add_epi32(sse1, V_R_MAD);
        }

        __m256i v_sum0 = _mm256_madd_epi16(sum1, one);
        __m256i v_d_l = _mm256_unpacklo_epi32(v_sum0, sse1);
        __m256i v_d_h = _mm256_unpackhi_epi32(v_sum0, sse1);
        __m256i v_d_lh = _mm256_add_epi32(v_d_l, v_d_h);
        const __m128i v_d0_d = _mm256_castsi256_si128(v_d_lh);
        const __m128i v_d1_d = _mm256_extracti128_si256(v_d_lh, 1);
        __m128i v_d = _mm_add_epi32(v_d0_d, v_d1_d);
        v_d = _mm_add_epi32(v_d, _mm_srli_si128(v_d, 8));
        sum_long += _mm_extract_epi32(v_d, 0);
        sse_long += _mm_extract_epi32(v_d, 1);
      }

      rshift = get_msb(output_height) + get_msb(output_width);

    } else if (yoffset == 4) {  // xoffset==0 && yoffset==4
      range = output_width / 16;
      if (output_height == 8) inc = 2;
      if (output_height == 4) inc = 4;
      for (j = 0; j < range * output_height * inc / 16; j++) {
        if (j % (output_height * inc / 16) == 0) {
          src_ptr = src_ptr_ref;
          src_ptr_ref += 16;
          dst_ptr = dst_ptr_ref;
          dst_ptr_ref += 16;

          opointer[0] = _mm256_loadu_si256((const __m256i *)src_ptr);
          src_ptr += src_pixels_per_line;
          curr = 0;
        }

        __m256i sum1 = _mm256_setzero_si256();
        __m256i sse1 = _mm256_setzero_si256();

        for (i = 0; i < 16 / inc; ++i) {
          prev = curr;
          curr = (curr == 0) ? 1 : 0;
          opointer[curr] = _mm256_loadu_si256((const __m256i *)src_ptr);
          src_ptr += src_pixels_per_line;

          __m256i V_S_SRC = _mm256_avg_epu16(opointer[curr], opointer[prev]);

          __m256i V_D_DST = _mm256_loadu_si256((const __m256i *)dst_ptr);
          dst_ptr += dst_stride;
          __m256i V_R_SUB = _mm256_sub_epi16(V_S_SRC, V_D_DST);
          __m256i V_R_MAD = _mm256_madd_epi16(V_R_SUB, V_R_SUB);
          sum1 = _mm256_add_epi16(sum1, V_R_SUB);
          sse1 = _mm256_add_epi32(sse1, V_R_MAD);
        }

        __m256i v_sum0 = _mm256_madd_epi16(sum1, one);
        __m256i v_d_l = _mm256_unpacklo_epi32(v_sum0, sse1);
        __m256i v_d_h = _mm256_unpackhi_epi32(v_sum0, sse1);
        __m256i v_d_lh = _mm256_add_epi32(v_d_l, v_d_h);
        const __m128i v_d0_d = _mm256_castsi256_si128(v_d_lh);
        const __m128i v_d1_d = _mm256_extracti128_si256(v_d_lh, 1);
        __m128i v_d = _mm_add_epi32(v_d0_d, v_d1_d);
        v_d = _mm_add_epi32(v_d, _mm_srli_si128(v_d, 8));
        sum_long += _mm_extract_epi32(v_d, 0);
        sse_long += _mm_extract_epi32(v_d, 1);
      }

      rshift = get_msb(output_height) + get_msb(output_width);

    } else {  // xoffset==0 && yoffset==1,2,3,5,6,7
      range = output_width / 16;
      if (output_height == 8) inc = 2;
      if (output_height == 4) inc = 4;
      for (j = 0; j < range * output_height * inc / 16; j++) {
        if (j % (output_height * inc / 16) == 0) {
          src_ptr = src_ptr_ref;
          src_ptr_ref += 16;
          dst_ptr = dst_ptr_ref;
          dst_ptr_ref += 16;

          opointer[0] = _mm256_loadu_si256((const __m256i *)src_ptr);
          src_ptr += src_pixels_per_line;
          curr = 0;
        }

        __m256i sum1 = _mm256_setzero_si256();
        __m256i sse1 = _mm256_setzero_si256();

        for (i = 0; i < 16 / inc; ++i) {
          prev = curr;
          curr = (curr == 0) ? 1 : 0;
          opointer[curr] = _mm256_loadu_si256((const __m256i *)src_ptr);
          src_ptr += src_pixels_per_line;

          __m256i V_S_M1 =
              _mm256_unpacklo_epi16(opointer[prev], opointer[curr]);
          __m256i V_S_M2 =
              _mm256_unpackhi_epi16(opointer[prev], opointer[curr]);

          __m256i V_S_MAD1 = _mm256_madd_epi16(V_S_M1, filter2);
          __m256i V_S_MAD2 = _mm256_madd_epi16(V_S_M2, filter2);

          __m256i V_S_S1 =
              _mm256_srli_epi32(_mm256_add_epi32(V_S_MAD1, rbias), 7);
          __m256i V_S_S2 =
              _mm256_srli_epi32(_mm256_add_epi32(V_S_MAD2, rbias), 7);

          __m256i V_S_SRC = _mm256_packus_epi32(V_S_S1, V_S_S2);

          __m256i V_D_DST = _mm256_loadu_si256((const __m256i *)dst_ptr);
          dst_ptr += dst_stride;

          __m256i V_R_SUB = _mm256_sub_epi16(V_S_SRC, V_D_DST);
          __m256i V_R_MAD = _mm256_madd_epi16(V_R_SUB, V_R_SUB);

          sum1 = _mm256_add_epi16(sum1, V_R_SUB);
          sse1 = _mm256_add_epi32(sse1, V_R_MAD);
        }

        __m256i v_sum0 = _mm256_madd_epi16(sum1, one);
        __m256i v_d_l = _mm256_unpacklo_epi32(v_sum0, sse1);
        __m256i v_d_h = _mm256_unpackhi_epi32(v_sum0, sse1);
        __m256i v_d_lh = _mm256_add_epi32(v_d_l, v_d_h);
        const __m128i v_d0_d = _mm256_castsi256_si128(v_d_lh);
        const __m128i v_d1_d = _mm256_extracti128_si256(v_d_lh, 1);
        __m128i v_d = _mm_add_epi32(v_d0_d, v_d1_d);
        v_d = _mm_add_epi32(v_d, _mm_srli_si128(v_d, 8));
        sum_long += _mm_extract_epi32(v_d, 0);
        sse_long += _mm_extract_epi32(v_d, 1);
      }

      rshift = get_msb(output_height) + get_msb(output_width);
    }
  } else if (xoffset == 4) {
    if (yoffset == 0) {  // xoffset==4 && yoffset==0
      range = output_width / 16;
      if (output_height == 8) inc = 2;
      if (output_height == 4) inc = 4;
      for (j = 0; j < range * output_height * inc / 16; j++) {
        if (j % (output_height * inc / 16) == 0) {
          src_ptr = src_ptr_ref;
          src_ptr_ref += 16;
          dst_ptr = dst_ptr_ref;
          dst_ptr_ref += 16;
          __m256i V_H_D1 = _mm256_loadu_si256((const __m256i *)src_ptr);
          __m256i V_H_D2 = _mm256_loadu_si256((const __m256i *)(src_ptr + 1));
          src_ptr += src_pixels_per_line;

          opointer[0] = _mm256_avg_epu16(V_H_D1, V_H_D2);

          curr = 0;
        }

        __m256i sum1 = _mm256_setzero_si256();
        __m256i sse1 = _mm256_setzero_si256();

        for (i = 0; i < 16 / inc; ++i) {
          prev = curr;
          curr = (curr == 0) ? 1 : 0;
          __m256i V_V_D1 = _mm256_loadu_si256((const __m256i *)src_ptr);
          __m256i V_V_D2 = _mm256_loadu_si256((const __m256i *)(src_ptr + 1));
          src_ptr += src_pixels_per_line;

          opointer[curr] = _mm256_avg_epu16(V_V_D1, V_V_D2);

          __m256i V_S_M1 =
              _mm256_unpacklo_epi16(opointer[prev], opointer[curr]);
          __m256i V_S_M2 =
              _mm256_unpackhi_epi16(opointer[prev], opointer[curr]);

          __m256i V_S_MAD1 = _mm256_madd_epi16(V_S_M1, filter2);
          __m256i V_S_MAD2 = _mm256_madd_epi16(V_S_M2, filter2);

          __m256i V_S_S1 =
              _mm256_srli_epi32(_mm256_add_epi32(V_S_MAD1, rbias), 7);
          __m256i V_S_S2 =
              _mm256_srli_epi32(_mm256_add_epi32(V_S_MAD2, rbias), 7);

          __m256i V_S_SRC = _mm256_packus_epi32(V_S_S1, V_S_S2);

          __m256i V_D_DST = _mm256_loadu_si256((const __m256i *)dst_ptr);
          dst_ptr += dst_stride;

          __m256i V_R_SUB = _mm256_sub_epi16(V_S_SRC, V_D_DST);
          __m256i V_R_MAD = _mm256_madd_epi16(V_R_SUB, V_R_SUB);

          sum1 = _mm256_add_epi16(sum1, V_R_SUB);
          sse1 = _mm256_add_epi32(sse1, V_R_MAD);
        }

        __m256i v_sum0 = _mm256_madd_epi16(sum1, one);
        __m256i v_d_l = _mm256_unpacklo_epi32(v_sum0, sse1);
        __m256i v_d_h = _mm256_unpackhi_epi32(v_sum0, sse1);
        __m256i v_d_lh = _mm256_add_epi32(v_d_l, v_d_h);
        const __m128i v_d0_d = _mm256_castsi256_si128(v_d_lh);
        const __m128i v_d1_d = _mm256_extracti128_si256(v_d_lh, 1);
        __m128i v_d = _mm_add_epi32(v_d0_d, v_d1_d);
        v_d = _mm_add_epi32(v_d, _mm_srli_si128(v_d, 8));
        sum_long += _mm_extract_epi32(v_d, 0);
        sse_long += _mm_extract_epi32(v_d, 1);
      }

      rshift = get_msb(output_height) + get_msb(output_width);

    } else if (yoffset == 4) {  // xoffset==4 && yoffset==4
      range = output_width / 16;
      if (output_height == 8) inc = 2;
      if (output_height == 4) inc = 4;
      for (j = 0; j < range * output_height * inc / 16; j++) {
        if (j % (output_height * inc / 16) == 0) {
          src_ptr = src_ptr_ref;
          src_ptr_ref += 16;
          dst_ptr = dst_ptr_ref;
          dst_ptr_ref += 16;

          __m256i V_H_D1 = _mm256_loadu_si256((const __m256i *)src_ptr);
          __m256i V_H_D2 = _mm256_loadu_si256((const __m256i *)(src_ptr + 1));
          src_ptr += src_pixels_per_line;
          opointer[0] = _mm256_avg_epu16(V_H_D1, V_H_D2);
          curr = 0;
        }

        __m256i sum1 = _mm256_setzero_si256();
        __m256i sse1 = _mm256_setzero_si256();

        for (i = 0; i < 16 / inc; ++i) {
          prev = curr;
          curr = (curr == 0) ? 1 : 0;
          __m256i V_V_D1 = _mm256_loadu_si256((const __m256i *)src_ptr);
          __m256i V_V_D2 = _mm256_loadu_si256((const __m256i *)(src_ptr + 1));
          src_ptr += src_pixels_per_line;
          opointer[curr] = _mm256_avg_epu16(V_V_D1, V_V_D2);
          __m256i V_S_SRC = _mm256_avg_epu16(opointer[curr], opointer[prev]);

          __m256i V_D_DST = _mm256_loadu_si256((const __m256i *)dst_ptr);
          dst_ptr += dst_stride;
          __m256i V_R_SUB = _mm256_sub_epi16(V_S_SRC, V_D_DST);
          __m256i V_R_MAD = _mm256_madd_epi16(V_R_SUB, V_R_SUB);
          sum1 = _mm256_add_epi16(sum1, V_R_SUB);
          sse1 = _mm256_add_epi32(sse1, V_R_MAD);
        }

        __m256i v_sum0 = _mm256_madd_epi16(sum1, one);
        __m256i v_d_l = _mm256_unpacklo_epi32(v_sum0, sse1);
        __m256i v_d_h = _mm256_unpackhi_epi32(v_sum0, sse1);
        __m256i v_d_lh = _mm256_add_epi32(v_d_l, v_d_h);
        const __m128i v_d0_d = _mm256_castsi256_si128(v_d_lh);
        const __m128i v_d1_d = _mm256_extracti128_si256(v_d_lh, 1);
        __m128i v_d = _mm_add_epi32(v_d0_d, v_d1_d);
        v_d = _mm_add_epi32(v_d, _mm_srli_si128(v_d, 8));
        sum_long += _mm_extract_epi32(v_d, 0);
        sse_long += _mm_extract_epi32(v_d, 1);
      }

      rshift = get_msb(output_height) + get_msb(output_width);

    } else {  // xoffset==4 && yoffset==1,2,3,5,6,7
      range = output_width / 16;
      if (output_height == 8) inc = 2;
      if (output_height == 4) inc = 4;
      for (j = 0; j < range * output_height * inc / 16; j++) {
        if (j % (output_height * inc / 16) == 0) {
          src_ptr = src_ptr_ref;
          src_ptr_ref += 16;
          dst_ptr = dst_ptr_ref;
          dst_ptr_ref += 16;

          __m256i V_H_D1 = _mm256_loadu_si256((const __m256i *)src_ptr);
          __m256i V_H_D2 = _mm256_loadu_si256((const __m256i *)(src_ptr + 1));
          src_ptr += src_pixels_per_line;
          opointer[0] = _mm256_avg_epu16(V_H_D1, V_H_D2);
          curr = 0;
        }

        __m256i sum1 = _mm256_setzero_si256();
        __m256i sse1 = _mm256_setzero_si256();

        for (i = 0; i < 16 / inc; ++i) {
          prev = curr;
          curr = (curr == 0) ? 1 : 0;
          __m256i V_V_D1 = _mm256_loadu_si256((const __m256i *)src_ptr);
          __m256i V_V_D2 = _mm256_loadu_si256((const __m256i *)(src_ptr + 1));
          src_ptr += src_pixels_per_line;
          opointer[curr] = _mm256_avg_epu16(V_V_D1, V_V_D2);

          __m256i V_S_M1 =
              _mm256_unpacklo_epi16(opointer[prev], opointer[curr]);
          __m256i V_S_M2 =
              _mm256_unpackhi_epi16(opointer[prev], opointer[curr]);

          __m256i V_S_MAD1 = _mm256_madd_epi16(V_S_M1, filter2);
          __m256i V_S_MAD2 = _mm256_madd_epi16(V_S_M2, filter2);

          __m256i V_S_S1 =
              _mm256_srli_epi32(_mm256_add_epi32(V_S_MAD1, rbias), 7);
          __m256i V_S_S2 =
              _mm256_srli_epi32(_mm256_add_epi32(V_S_MAD2, rbias), 7);

          __m256i V_S_SRC = _mm256_packus_epi32(V_S_S1, V_S_S2);

          __m256i V_D_DST = _mm256_loadu_si256((const __m256i *)dst_ptr);
          dst_ptr += dst_stride;

          __m256i V_R_SUB = _mm256_sub_epi16(V_S_SRC, V_D_DST);
          __m256i V_R_MAD = _mm256_madd_epi16(V_R_SUB, V_R_SUB);

          sum1 = _mm256_add_epi16(sum1, V_R_SUB);
          sse1 = _mm256_add_epi32(sse1, V_R_MAD);
        }

        __m256i v_sum0 = _mm256_madd_epi16(sum1, one);
        __m256i v_d_l = _mm256_unpacklo_epi32(v_sum0, sse1);
        __m256i v_d_h = _mm256_unpackhi_epi32(v_sum0, sse1);
        __m256i v_d_lh = _mm256_add_epi32(v_d_l, v_d_h);
        const __m128i v_d0_d = _mm256_castsi256_si128(v_d_lh);
        const __m128i v_d1_d = _mm256_extracti128_si256(v_d_lh, 1);
        __m128i v_d = _mm_add_epi32(v_d0_d, v_d1_d);
        v_d = _mm_add_epi32(v_d, _mm_srli_si128(v_d, 8));
        sum_long += _mm_extract_epi32(v_d, 0);
        sse_long += _mm_extract_epi32(v_d, 1);
      }

      rshift = get_msb(output_height) + get_msb(output_width);
    }
  } else if (yoffset == 0) {  // xoffset==1,2,3,5,6,7 && yoffset==0
    range = output_width / 16;
    if (output_height == 8) inc = 2;
    if (output_height == 4) inc = 4;
    for (j = 0; j < range * output_height * inc / 16; j++) {
      if (j % (output_height * inc / 16) == 0) {
        src_ptr = src_ptr_ref;
        src_ptr_ref += 16;
        dst_ptr = dst_ptr_ref;
        dst_ptr_ref += 16;

        curr = 0;
      }

      __m256i sum1 = _mm256_setzero_si256();
      __m256i sse1 = _mm256_setzero_si256();

      for (i = 0; i < 16 / inc; ++i) {
        __m256i V_V_D1 = _mm256_loadu_si256((const __m256i *)src_ptr);
        __m256i V_V_D2 = _mm256_loadu_si256((const __m256i *)(src_ptr + 1));
        src_ptr += src_pixels_per_line;
        __m256i V_V_M1 = _mm256_unpacklo_epi16(V_V_D1, V_V_D2);
        __m256i V_V_M2 = _mm256_unpackhi_epi16(V_V_D1, V_V_D2);
        __m256i V_V_MAD1 = _mm256_madd_epi16(V_V_M1, filter1);
        __m256i V_V_MAD2 = _mm256_madd_epi16(V_V_M2, filter1);
        __m256i V_V_S1 =
            _mm256_srli_epi32(_mm256_add_epi32(V_V_MAD1, rbias), 7);
        __m256i V_V_S2 =
            _mm256_srli_epi32(_mm256_add_epi32(V_V_MAD2, rbias), 7);
        opointer[curr] = _mm256_packus_epi32(V_V_S1, V_V_S2);

        __m256i V_D_DST = _mm256_loadu_si256((const __m256i *)dst_ptr);
        dst_ptr += dst_stride;
        __m256i V_R_SUB = _mm256_sub_epi16(opointer[curr], V_D_DST);
        __m256i V_R_MAD = _mm256_madd_epi16(V_R_SUB, V_R_SUB);

        sum1 = _mm256_add_epi16(sum1, V_R_SUB);
        sse1 = _mm256_add_epi32(sse1, V_R_MAD);
      }

      __m256i v_sum0 = _mm256_madd_epi16(sum1, one);
      __m256i v_d_l = _mm256_unpacklo_epi32(v_sum0, sse1);
      __m256i v_d_h = _mm256_unpackhi_epi32(v_sum0, sse1);
      __m256i v_d_lh = _mm256_add_epi32(v_d_l, v_d_h);
      const __m128i v_d0_d = _mm256_castsi256_si128(v_d_lh);
      const __m128i v_d1_d = _mm256_extracti128_si256(v_d_lh, 1);
      __m128i v_d = _mm_add_epi32(v_d0_d, v_d1_d);
      v_d = _mm_add_epi32(v_d, _mm_srli_si128(v_d, 8));
      sum_long += _mm_extract_epi32(v_d, 0);
      sse_long += _mm_extract_epi32(v_d, 1);
    }

    rshift = get_msb(output_height) + get_msb(output_width);

  } else if (yoffset == 4) {  // xoffset==1,2,3,5,6,7 && yoffset==4

    range = output_width / 16;
    if (output_height == 8) inc = 2;
    if (output_height == 4) inc = 4;
    for (j = 0; j < range * output_height * inc / 16; j++) {
      if (j % (output_height * inc / 16) == 0) {
        src_ptr = src_ptr_ref;
        src_ptr_ref += 16;
        dst_ptr = dst_ptr_ref;
        dst_ptr_ref += 16;

        __m256i V_H_D1 = _mm256_loadu_si256((const __m256i *)src_ptr);
        __m256i V_H_D2 = _mm256_loadu_si256((const __m256i *)(src_ptr + 1));
        src_ptr += src_pixels_per_line;

        __m256i V_H_M1 = _mm256_unpacklo_epi16(V_H_D1, V_H_D2);
        __m256i V_H_M2 = _mm256_unpackhi_epi16(V_H_D1, V_H_D2);

        __m256i V_H_MAD1 = _mm256_madd_epi16(V_H_M1, filter1);
        __m256i V_H_MAD2 = _mm256_madd_epi16(V_H_M2, filter1);

        __m256i V_H_S1 =
            _mm256_srli_epi32(_mm256_add_epi32(V_H_MAD1, rbias), 7);
        __m256i V_H_S2 =
            _mm256_srli_epi32(_mm256_add_epi32(V_H_MAD2, rbias), 7);

        opointer[0] = _mm256_packus_epi32(V_H_S1, V_H_S2);

        curr = 0;
      }

      __m256i sum1 = _mm256_setzero_si256();
      __m256i sse1 = _mm256_setzero_si256();

      for (i = 0; i < 16 / inc; ++i) {
        prev = curr;
        curr = (curr == 0) ? 1 : 0;
        __m256i V_V_D1 = _mm256_loadu_si256((const __m256i *)src_ptr);
        __m256i V_V_D2 = _mm256_loadu_si256((const __m256i *)(src_ptr + 1));
        src_ptr += src_pixels_per_line;
        __m256i V_V_M1 = _mm256_unpacklo_epi16(V_V_D1, V_V_D2);
        __m256i V_V_M2 = _mm256_unpackhi_epi16(V_V_D1, V_V_D2);
        __m256i V_V_MAD1 = _mm256_madd_epi16(V_V_M1, filter1);
        __m256i V_V_MAD2 = _mm256_madd_epi16(V_V_M2, filter1);
        __m256i V_V_S1 =
            _mm256_srli_epi32(_mm256_add_epi32(V_V_MAD1, rbias), 7);
        __m256i V_V_S2 =
            _mm256_srli_epi32(_mm256_add_epi32(V_V_MAD2, rbias), 7);
        opointer[curr] = _mm256_packus_epi32(V_V_S1, V_V_S2);

        __m256i V_S_SRC = _mm256_avg_epu16(opointer[prev], opointer[curr]);

        __m256i V_D_DST = _mm256_loadu_si256((const __m256i *)dst_ptr);
        dst_ptr += dst_stride;

        __m256i V_R_SUB = _mm256_sub_epi16(V_S_SRC, V_D_DST);
        __m256i V_R_MAD = _mm256_madd_epi16(V_R_SUB, V_R_SUB);

        sum1 = _mm256_add_epi16(sum1, V_R_SUB);
        sse1 = _mm256_add_epi32(sse1, V_R_MAD);
      }

      __m256i v_sum0 = _mm256_madd_epi16(sum1, one);
      __m256i v_d_l = _mm256_unpacklo_epi32(v_sum0, sse1);
      __m256i v_d_h = _mm256_unpackhi_epi32(v_sum0, sse1);
      __m256i v_d_lh = _mm256_add_epi32(v_d_l, v_d_h);
      const __m128i v_d0_d = _mm256_castsi256_si128(v_d_lh);
      const __m128i v_d1_d = _mm256_extracti128_si256(v_d_lh, 1);
      __m128i v_d = _mm_add_epi32(v_d0_d, v_d1_d);
      v_d = _mm_add_epi32(v_d, _mm_srli_si128(v_d, 8));
      sum_long += _mm_extract_epi32(v_d, 0);
      sse_long += _mm_extract_epi32(v_d, 1);
    }

    rshift = get_msb(output_height) + get_msb(output_width);

  } else {  // xoffset==1,2,3,5,6,7 && yoffset==1,2,3,5,6,7
    range = output_width / 16;
    if (output_height == 8) inc = 2;
    if (output_height == 4) inc = 4;
    unsigned int nloop = 16 / inc;
    for (j = 0; j < range * output_height * inc / 16; j++) {
      if (j % (output_height * inc / 16) == 0) {
        src_ptr = src_ptr_ref;
        src_ptr_ref += 16;
        dst_ptr = dst_ptr_ref;
        dst_ptr_ref += 16;

        __m256i V_H_D1 = _mm256_loadu_si256((const __m256i *)src_ptr);
        __m256i V_H_D2 = _mm256_loadu_si256((const __m256i *)(src_ptr + 1));
        src_ptr += src_pixels_per_line;

        __m256i V_H_M1 = _mm256_unpacklo_epi16(V_H_D1, V_H_D2);
        __m256i V_H_M2 = _mm256_unpackhi_epi16(V_H_D1, V_H_D2);

        __m256i V_H_MAD1 = _mm256_madd_epi16(V_H_M1, filter1);
        __m256i V_H_MAD2 = _mm256_madd_epi16(V_H_M2, filter1);

        __m256i V_H_S1 =
            _mm256_srli_epi32(_mm256_add_epi32(V_H_MAD1, rbias), 7);
        __m256i V_H_S2 =
            _mm256_srli_epi32(_mm256_add_epi32(V_H_MAD2, rbias), 7);

        opointer[0] = _mm256_packus_epi32(V_H_S1, V_H_S2);

        curr = 0;
      }

      __m256i sum1 = _mm256_setzero_si256();
      __m256i sse1 = _mm256_setzero_si256();

      for (i = 0; i < nloop; ++i) {
        prev = curr;
        curr = !curr;
        __m256i V_V_D1 = _mm256_loadu_si256((const __m256i *)src_ptr);
        __m256i V_V_D2 = _mm256_loadu_si256((const __m256i *)(src_ptr + 1));
        src_ptr += src_pixels_per_line;
        __m256i V_V_M1 = _mm256_unpacklo_epi16(V_V_D1, V_V_D2);
        __m256i V_V_M2 = _mm256_unpackhi_epi16(V_V_D1, V_V_D2);
        __m256i V_V_MAD1 = _mm256_madd_epi16(V_V_M1, filter1);
        __m256i V_V_MAD2 = _mm256_madd_epi16(V_V_M2, filter1);
        __m256i V_V_S1 =
            _mm256_srli_epi32(_mm256_add_epi32(V_V_MAD1, rbias), 7);
        __m256i V_V_S2 =
            _mm256_srli_epi32(_mm256_add_epi32(V_V_MAD2, rbias), 7);
        opointer[curr] = _mm256_packus_epi32(V_V_S1, V_V_S2);

        __m256i V_S_M1 = _mm256_unpacklo_epi16(opointer[prev], opointer[curr]);
        __m256i V_S_M2 = _mm256_unpackhi_epi16(opointer[prev], opointer[curr]);

        __m256i V_S_MAD1 = _mm256_madd_epi16(V_S_M1, filter2);
        __m256i V_S_MAD2 = _mm256_madd_epi16(V_S_M2, filter2);

        __m256i V_S_S1 =
            _mm256_srli_epi32(_mm256_add_epi32(V_S_MAD1, rbias), 7);
        __m256i V_S_S2 =
            _mm256_srli_epi32(_mm256_add_epi32(V_S_MAD2, rbias), 7);

        __m256i V_S_SRC = _mm256_packus_epi32(V_S_S1, V_S_S2);

        __m256i V_D_DST = _mm256_loadu_si256((const __m256i *)dst_ptr);
        dst_ptr += dst_stride;

        __m256i V_R_SUB = _mm256_sub_epi16(V_S_SRC, V_D_DST);
        __m256i V_R_MAD = _mm256_madd_epi16(V_R_SUB, V_R_SUB);

        sum1 = _mm256_add_epi16(sum1, V_R_SUB);
        sse1 = _mm256_add_epi32(sse1, V_R_MAD);
      }

      __m256i v_sum0 = _mm256_madd_epi16(sum1, one);
      __m256i v_d_l = _mm256_unpacklo_epi32(v_sum0, sse1);
      __m256i v_d_h = _mm256_unpackhi_epi32(v_sum0, sse1);
      __m256i v_d_lh = _mm256_add_epi32(v_d_l, v_d_h);
      const __m128i v_d0_d = _mm256_castsi256_si128(v_d_lh);
      const __m128i v_d1_d = _mm256_extracti128_si256(v_d_lh, 1);
      __m128i v_d = _mm_add_epi32(v_d0_d, v_d1_d);
      v_d = _mm_add_epi32(v_d, _mm_srli_si128(v_d, 8));
      sum_long += _mm_extract_epi32(v_d, 0);
      sse_long += _mm_extract_epi32(v_d, 1);
    }

    rshift = get_msb(output_height) + get_msb(output_width);
  }

  *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_long, 4);
  int sum = (int)ROUND_POWER_OF_TWO(sum_long, 2);

  int32_t var = *sse - (uint32_t)(((int64_t)sum * sum) >> rshift);

  return (var > 0) ? var : 0;
}

static void highbd_calc8x8var_avx2(const uint16_t *src, int src_stride,
                                   const uint16_t *ref, int ref_stride,
                                   uint32_t *sse, int *sum) {
  __m256i v_sum_d = _mm256_setzero_si256();
  __m256i v_sse_d = _mm256_setzero_si256();
  for (int i = 0; i < 8; i += 2) {
    const __m128i v_p_a0 = _mm_loadu_si128((const __m128i *)src);
    const __m128i v_p_a1 = _mm_loadu_si128((const __m128i *)(src + src_stride));
    const __m128i v_p_b0 = _mm_loadu_si128((const __m128i *)ref);
    const __m128i v_p_b1 = _mm_loadu_si128((const __m128i *)(ref + ref_stride));
    __m256i v_p_a = _mm256_castsi128_si256(v_p_a0);
    __m256i v_p_b = _mm256_castsi128_si256(v_p_b0);
    v_p_a = _mm256_inserti128_si256(v_p_a, v_p_a1, 1);
    v_p_b = _mm256_inserti128_si256(v_p_b, v_p_b1, 1);
    const __m256i v_diff = _mm256_sub_epi16(v_p_a, v_p_b);
    const __m256i v_sqrdiff = _mm256_madd_epi16(v_diff, v_diff);
    v_sum_d = _mm256_add_epi16(v_sum_d, v_diff);
    v_sse_d = _mm256_add_epi32(v_sse_d, v_sqrdiff);
    src += src_stride * 2;
    ref += ref_stride * 2;
  }
  __m256i v_sum00 = _mm256_cvtepi16_epi32(_mm256_castsi256_si128(v_sum_d));
  __m256i v_sum01 = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(v_sum_d, 1));
  __m256i v_sum0 = _mm256_add_epi32(v_sum00, v_sum01);
  __m256i v_d_l = _mm256_unpacklo_epi32(v_sum0, v_sse_d);
  __m256i v_d_h = _mm256_unpackhi_epi32(v_sum0, v_sse_d);
  __m256i v_d_lh = _mm256_add_epi32(v_d_l, v_d_h);
  const __m128i v_d0_d = _mm256_castsi256_si128(v_d_lh);
  const __m128i v_d1_d = _mm256_extracti128_si256(v_d_lh, 1);
  __m128i v_d = _mm_add_epi32(v_d0_d, v_d1_d);
  v_d = _mm_add_epi32(v_d, _mm_srli_si128(v_d, 8));
  *sum = _mm_extract_epi32(v_d, 0);
  *sse = _mm_extract_epi32(v_d, 1);
}

static void highbd_calc16x16var_avx2(const uint16_t *src, int src_stride,
                                     const uint16_t *ref, int ref_stride,
                                     uint32_t *sse, int *sum) {
  __m256i v_sum_d = _mm256_setzero_si256();
  __m256i v_sse_d = _mm256_setzero_si256();
  const __m256i one = _mm256_set1_epi16(1);
  for (int i = 0; i < 16; ++i) {
    const __m256i v_p_a = _mm256_loadu_si256((const __m256i *)src);
    const __m256i v_p_b = _mm256_loadu_si256((const __m256i *)ref);
    const __m256i v_diff = _mm256_sub_epi16(v_p_a, v_p_b);
    const __m256i v_sqrdiff = _mm256_madd_epi16(v_diff, v_diff);
    v_sum_d = _mm256_add_epi16(v_sum_d, v_diff);
    v_sse_d = _mm256_add_epi32(v_sse_d, v_sqrdiff);
    src += src_stride;
    ref += ref_stride;
  }
  __m256i v_sum0 = _mm256_madd_epi16(v_sum_d, one);
  __m256i v_d_l = _mm256_unpacklo_epi32(v_sum0, v_sse_d);
  __m256i v_d_h = _mm256_unpackhi_epi32(v_sum0, v_sse_d);
  __m256i v_d_lh = _mm256_add_epi32(v_d_l, v_d_h);
  const __m128i v_d0_d = _mm256_castsi256_si128(v_d_lh);
  const __m128i v_d1_d = _mm256_extracti128_si256(v_d_lh, 1);
  __m128i v_d = _mm_add_epi32(v_d0_d, v_d1_d);
  v_d = _mm_add_epi32(v_d, _mm_srli_si128(v_d, 8));
  *sum = _mm_extract_epi32(v_d, 0);
  *sse = _mm_extract_epi32(v_d, 1);
}

static void highbd_10_variance_avx2(const uint16_t *src, int src_stride,
                                    const uint16_t *ref, int ref_stride, int w,
                                    int h, uint32_t *sse, int *sum,
                                    high_variance_fn_t var_fn, int block_size) {
  int i, j;
  uint64_t sse_long = 0;
  int32_t sum_long = 0;

  for (i = 0; i < h; i += block_size) {
    for (j = 0; j < w; j += block_size) {
      unsigned int sse0;
      int sum0;
      var_fn(src + src_stride * i + j, src_stride, ref + ref_stride * i + j,
             ref_stride, &sse0, &sum0);
      sse_long += sse0;
      sum_long += sum0;
    }
  }
  *sum = ROUND_POWER_OF_TWO(sum_long, 2);
  *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_long, 4);
}

#define VAR_FN(w, h, block_size, shift)                                        \
  uint32_t aom_highbd_10_variance##w##x##h##_avx2(                             \
      const uint8_t *src8, int src_stride, const uint8_t *ref8,                \
      int ref_stride, uint32_t *sse) {                                         \
    int sum;                                                                   \
    int64_t var;                                                               \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                                 \
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);                                 \
    highbd_10_variance_avx2(src, src_stride, ref, ref_stride, w, h, sse, &sum, \
                            highbd_calc##block_size##x##block_size##var_avx2,  \
                            block_size);                                       \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) >> shift);                   \
    return (var >= 0) ? (uint32_t)var : 0;                                     \
  }

VAR_FN(128, 128, 16, 14)
VAR_FN(128, 64, 16, 13)
VAR_FN(64, 128, 16, 13)
VAR_FN(64, 64, 16, 12)
VAR_FN(64, 32, 16, 11)
VAR_FN(32, 64, 16, 11)
VAR_FN(32, 32, 16, 10)
VAR_FN(32, 16, 16, 9)
VAR_FN(16, 32, 16, 9)
VAR_FN(16, 16, 16, 8)
VAR_FN(16, 8, 8, 7)
VAR_FN(8, 16, 8, 7)
VAR_FN(8, 8, 8, 6)

#if !CONFIG_REALTIME_ONLY
VAR_FN(16, 64, 16, 10)
VAR_FN(32, 8, 8, 8)
VAR_FN(64, 16, 16, 10)
VAR_FN(8, 32, 8, 8)
#endif  // !CONFIG_REALTIME_ONLY

#undef VAR_FN

unsigned int aom_highbd_10_mse16x16_avx2(const uint8_t *src8, int src_stride,
                                         const uint8_t *ref8, int ref_stride,
                                         unsigned int *sse) {
  int sum;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  highbd_10_variance_avx2(src, src_stride, ref, ref_stride, 16, 16, sse, &sum,
                          highbd_calc16x16var_avx2, 16);
  return *sse;
}

#define SSE2_HEIGHT(H)                                                 \
  uint32_t aom_highbd_10_sub_pixel_variance8x##H##_sse2(               \
      const uint8_t *src8, int src_stride, int x_offset, int y_offset, \
      const uint8_t *dst8, int dst_stride, uint32_t *sse_ptr);

SSE2_HEIGHT(8)
SSE2_HEIGHT(16)

#undef SSE2_HEIGHT

#define HIGHBD_SUBPIX_VAR(W, H)                                              \
  uint32_t aom_highbd_10_sub_pixel_variance##W##x##H##_avx2(                 \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,          \
      const uint8_t *dst, int dst_stride, uint32_t *sse) {                   \
    if (W == 8 && H == 16)                                                   \
      return aom_highbd_10_sub_pixel_variance8x16_sse2(                      \
          src, src_stride, xoffset, yoffset, dst, dst_stride, sse);          \
    else if (W == 8 && H == 8)                                               \
      return aom_highbd_10_sub_pixel_variance8x8_sse2(                       \
          src, src_stride, xoffset, yoffset, dst, dst_stride, sse);          \
    else                                                                     \
      return aom_highbd_var_filter_block2d_bil_avx2(                         \
          src, src_stride, 1, H, W, xoffset, yoffset, dst, dst_stride, sse); \
  }

HIGHBD_SUBPIX_VAR(128, 128)
HIGHBD_SUBPIX_VAR(128, 64)
HIGHBD_SUBPIX_VAR(64, 128)
HIGHBD_SUBPIX_VAR(64, 64)
HIGHBD_SUBPIX_VAR(64, 32)
HIGHBD_SUBPIX_VAR(32, 64)
HIGHBD_SUBPIX_VAR(32, 32)
HIGHBD_SUBPIX_VAR(32, 16)
HIGHBD_SUBPIX_VAR(16, 32)
HIGHBD_SUBPIX_VAR(16, 16)
HIGHBD_SUBPIX_VAR(16, 8)
HIGHBD_SUBPIX_VAR(8, 16)
HIGHBD_SUBPIX_VAR(8, 8)

#undef HIGHBD_SUBPIX_VAR

static uint64_t mse_4xh_16bit_highbd_avx2(uint16_t *dst, int dstride,
                                          uint16_t *src, int sstride, int h) {
  uint64_t sum = 0;
  __m128i reg0_4x16, reg1_4x16, reg2_4x16, reg3_4x16;
  __m256i src0_8x16, src1_8x16, src_16x16;
  __m256i dst0_8x16, dst1_8x16, dst_16x16;
  __m256i res0_4x64, res1_4x64, res2_4x64, res3_4x64;
  __m256i sub_result;
  const __m256i zeros = _mm256_broadcastsi128_si256(_mm_setzero_si128());
  __m256i square_result = _mm256_broadcastsi128_si256(_mm_setzero_si128());
  for (int i = 0; i < h; i += 4) {
    reg0_4x16 = _mm_loadl_epi64((__m128i const *)(&dst[(i + 0) * dstride]));
    reg1_4x16 = _mm_loadl_epi64((__m128i const *)(&dst[(i + 1) * dstride]));
    reg2_4x16 = _mm_loadl_epi64((__m128i const *)(&dst[(i + 2) * dstride]));
    reg3_4x16 = _mm_loadl_epi64((__m128i const *)(&dst[(i + 3) * dstride]));
    dst0_8x16 =
        _mm256_castsi128_si256(_mm_unpacklo_epi64(reg0_4x16, reg1_4x16));
    dst1_8x16 =
        _mm256_castsi128_si256(_mm_unpacklo_epi64(reg2_4x16, reg3_4x16));
    dst_16x16 = _mm256_permute2x128_si256(dst0_8x16, dst1_8x16, 0x20);

    reg0_4x16 = _mm_loadl_epi64((__m128i const *)(&src[(i + 0) * sstride]));
    reg1_4x16 = _mm_loadl_epi64((__m128i const *)(&src[(i + 1) * sstride]));
    reg2_4x16 = _mm_loadl_epi64((__m128i const *)(&src[(i + 2) * sstride]));
    reg3_4x16 = _mm_loadl_epi64((__m128i const *)(&src[(i + 3) * sstride]));
    src0_8x16 =
        _mm256_castsi128_si256(_mm_unpacklo_epi64(reg0_4x16, reg1_4x16));
    src1_8x16 =
        _mm256_castsi128_si256(_mm_unpacklo_epi64(reg2_4x16, reg3_4x16));
    src_16x16 = _mm256_permute2x128_si256(src0_8x16, src1_8x16, 0x20);

    sub_result = _mm256_abs_epi16(_mm256_sub_epi16(src_16x16, dst_16x16));

    src_16x16 = _mm256_unpacklo_epi16(sub_result, zeros);
    dst_16x16 = _mm256_unpackhi_epi16(sub_result, zeros);

    src_16x16 = _mm256_madd_epi16(src_16x16, src_16x16);
    dst_16x16 = _mm256_madd_epi16(dst_16x16, dst_16x16);

    res0_4x64 = _mm256_unpacklo_epi32(src_16x16, zeros);
    res1_4x64 = _mm256_unpackhi_epi32(src_16x16, zeros);
    res2_4x64 = _mm256_unpacklo_epi32(dst_16x16, zeros);
    res3_4x64 = _mm256_unpackhi_epi32(dst_16x16, zeros);

    square_result = _mm256_add_epi64(
        square_result,
        _mm256_add_epi64(
            _mm256_add_epi64(_mm256_add_epi64(res0_4x64, res1_4x64), res2_4x64),
            res3_4x64));
  }
  const __m128i sum_2x64 =
      _mm_add_epi64(_mm256_castsi256_si128(square_result),
                    _mm256_extracti128_si256(square_result, 1));
  const __m128i sum_1x64 = _mm_add_epi64(sum_2x64, _mm_srli_si128(sum_2x64, 8));
  xx_storel_64(&sum, sum_1x64);
  return sum;
}

static uint64_t mse_8xh_16bit_highbd_avx2(uint16_t *dst, int dstride,
                                          uint16_t *src, int sstride, int h) {
  uint64_t sum = 0;
  __m256i src0_8x16, src1_8x16, src_16x16;
  __m256i dst0_8x16, dst1_8x16, dst_16x16;
  __m256i res0_4x64, res1_4x64, res2_4x64, res3_4x64;
  __m256i sub_result;
  const __m256i zeros = _mm256_broadcastsi128_si256(_mm_setzero_si128());
  __m256i square_result = _mm256_broadcastsi128_si256(_mm_setzero_si128());

  for (int i = 0; i < h; i += 2) {
    dst0_8x16 =
        _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)&dst[i * dstride]));
    dst1_8x16 = _mm256_castsi128_si256(
        _mm_loadu_si128((__m128i *)&dst[(i + 1) * dstride]));
    dst_16x16 = _mm256_permute2x128_si256(dst0_8x16, dst1_8x16, 0x20);

    src0_8x16 =
        _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)&src[i * sstride]));
    src1_8x16 = _mm256_castsi128_si256(
        _mm_loadu_si128((__m128i *)&src[(i + 1) * sstride]));
    src_16x16 = _mm256_permute2x128_si256(src0_8x16, src1_8x16, 0x20);

    sub_result = _mm256_abs_epi16(_mm256_sub_epi16(src_16x16, dst_16x16));

    src_16x16 = _mm256_unpacklo_epi16(sub_result, zeros);
    dst_16x16 = _mm256_unpackhi_epi16(sub_result, zeros);

    src_16x16 = _mm256_madd_epi16(src_16x16, src_16x16);
    dst_16x16 = _mm256_madd_epi16(dst_16x16, dst_16x16);

    res0_4x64 = _mm256_unpacklo_epi32(src_16x16, zeros);
    res1_4x64 = _mm256_unpackhi_epi32(src_16x16, zeros);
    res2_4x64 = _mm256_unpacklo_epi32(dst_16x16, zeros);
    res3_4x64 = _mm256_unpackhi_epi32(dst_16x16, zeros);

    square_result = _mm256_add_epi64(
        square_result,
        _mm256_add_epi64(
            _mm256_add_epi64(_mm256_add_epi64(res0_4x64, res1_4x64), res2_4x64),
            res3_4x64));
  }

  const __m128i sum_2x64 =
      _mm_add_epi64(_mm256_castsi256_si128(square_result),
                    _mm256_extracti128_si256(square_result, 1));
  const __m128i sum_1x64 = _mm_add_epi64(sum_2x64, _mm_srli_si128(sum_2x64, 8));
  xx_storel_64(&sum, sum_1x64);
  return sum;
}

uint64_t aom_mse_wxh_16bit_highbd_avx2(uint16_t *dst, int dstride,
                                       uint16_t *src, int sstride, int w,
                                       int h) {
  assert((w == 8 || w == 4) && (h == 8 || h == 4) &&
         "w=8/4 and h=8/4 must satisfy");
  switch (w) {
    case 4: return mse_4xh_16bit_highbd_avx2(dst, dstride, src, sstride, h);
    case 8: return mse_8xh_16bit_highbd_avx2(dst, dstride, src, sstride, h);
    default: assert(0 && "unsupported width"); return -1;
  }
}
