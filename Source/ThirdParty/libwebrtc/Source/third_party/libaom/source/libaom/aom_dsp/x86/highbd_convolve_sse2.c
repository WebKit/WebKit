/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include <emmintrin.h>

#include "config/aom_dsp_rtcd.h"
#include "aom_dsp/x86/convolve.h"

// -----------------------------------------------------------------------------

static void aom_highbd_filter_block1d4_v4_sse2(
    const uint16_t *src_ptr, ptrdiff_t src_pitch, uint16_t *dst_ptr,
    ptrdiff_t dst_pitch, uint32_t height, const int16_t *filter, int bd) {
  __m128i filtersReg;
  __m128i srcReg2, srcReg3, srcReg4, srcReg5, srcReg6;
  __m128i srcReg23_lo, srcReg34_lo;
  __m128i srcReg45_lo, srcReg56_lo;
  __m128i resReg23_lo, resReg34_lo, resReg45_lo, resReg56_lo;
  __m128i resReg23_45_lo, resReg34_56_lo;
  __m128i resReg23_45, resReg34_56;
  __m128i addFilterReg64, secondFilters, thirdFilters;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;

  const __m128i max = _mm_set1_epi16((1 << bd) - 1);
  addFilterReg64 = _mm_set1_epi32(64);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);

  // coeffs 0 1 0 1 2 3 2 3
  const __m128i tmp0 = _mm_unpacklo_epi32(filtersReg, filtersReg);
  // coeffs 4 5 4 5 6 7 6 7
  const __m128i tmp1 = _mm_unpackhi_epi32(filtersReg, filtersReg);

  secondFilters = _mm_unpackhi_epi64(tmp0, tmp0);  // coeffs 2 3 2 3 2 3 2 3
  thirdFilters = _mm_unpacklo_epi64(tmp1, tmp1);   // coeffs 4 5 4 5 4 5 4 5

  // multiply the size of the source and destination stride by two
  src_stride = src_pitch << 1;
  dst_stride = dst_pitch << 1;

  srcReg2 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 2));
  srcReg3 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 3));
  srcReg23_lo = _mm_unpacklo_epi16(srcReg2, srcReg3);

  srcReg4 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 4));
  srcReg34_lo = _mm_unpacklo_epi16(srcReg3, srcReg4);

  for (i = height; i > 1; i -= 2) {
    srcReg5 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 5));
    srcReg45_lo = _mm_unpacklo_epi16(srcReg4, srcReg5);

    srcReg6 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 6));
    srcReg56_lo = _mm_unpacklo_epi16(srcReg5, srcReg6);

    // multiply 2 adjacent elements with the filter and add the result

    resReg23_lo = _mm_madd_epi16(srcReg23_lo, secondFilters);
    resReg34_lo = _mm_madd_epi16(srcReg34_lo, secondFilters);
    resReg45_lo = _mm_madd_epi16(srcReg45_lo, thirdFilters);
    resReg56_lo = _mm_madd_epi16(srcReg56_lo, thirdFilters);

    resReg23_45_lo = _mm_add_epi32(resReg23_lo, resReg45_lo);
    resReg34_56_lo = _mm_add_epi32(resReg34_lo, resReg56_lo);

    // shift by 7 bit each 32 bit
    resReg23_45_lo = _mm_add_epi32(resReg23_45_lo, addFilterReg64);
    resReg34_56_lo = _mm_add_epi32(resReg34_56_lo, addFilterReg64);
    resReg23_45_lo = _mm_srai_epi32(resReg23_45_lo, 7);
    resReg34_56_lo = _mm_srai_epi32(resReg34_56_lo, 7);

    // shrink to 16 bit each 32 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve
    // result
    resReg23_45 = _mm_packs_epi32(resReg23_45_lo, _mm_setzero_si128());
    resReg34_56 = _mm_packs_epi32(resReg34_56_lo, _mm_setzero_si128());

    resReg23_45 = _mm_max_epi16(resReg23_45, _mm_setzero_si128());
    resReg23_45 = _mm_min_epi16(resReg23_45, max);
    resReg34_56 = _mm_max_epi16(resReg34_56, _mm_setzero_si128());
    resReg34_56 = _mm_min_epi16(resReg34_56, max);

    src_ptr += src_stride;

    _mm_storel_epi64((__m128i *)dst_ptr, (resReg23_45));
    _mm_storel_epi64((__m128i *)(dst_ptr + dst_pitch), (resReg34_56));

    dst_ptr += dst_stride;

    // save part of the registers for next strides
    srcReg23_lo = srcReg45_lo;
    srcReg34_lo = srcReg56_lo;
    srcReg4 = srcReg6;
  }
}

static void aom_highbd_filter_block1d4_h4_sse2(
    const uint16_t *src_ptr, ptrdiff_t src_pitch, uint16_t *dst_ptr,
    ptrdiff_t dst_pitch, uint32_t height, const int16_t *filter, int bd) {
  __m128i filtersReg;
  __m128i addFilterReg64;
  __m128i secondFilters, thirdFilters;
  __m128i srcRegFilt32b1_1;
  __m128i srcReg32b1;
  unsigned int i;
  src_ptr -= 3;
  addFilterReg64 = _mm_set1_epi32(64);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  const __m128i max = _mm_set1_epi16((1 << bd) - 1);

  // coeffs 0 1 0 1 2 3 2 3
  const __m128i tmp_0 = _mm_unpacklo_epi32(filtersReg, filtersReg);
  // coeffs 4 5 4 5 6 7 6 7
  const __m128i tmp_1 = _mm_unpackhi_epi32(filtersReg, filtersReg);

  secondFilters = _mm_unpackhi_epi64(tmp_0, tmp_0);  // coeffs 2 3 2 3 2 3 2 3
  thirdFilters = _mm_unpacklo_epi64(tmp_1, tmp_1);   // coeffs 4 5 4 5 4 5 4 5

  for (i = height; i > 0; i -= 1) {
    srcReg32b1 = _mm_loadu_si128((const __m128i *)(src_ptr + 2));

    __m128i ss_3_1 = _mm_srli_si128(srcReg32b1, 2);
    __m128i ss_4_1 = _mm_srli_si128(srcReg32b1, 4);
    __m128i ss_5_1 = _mm_srli_si128(srcReg32b1, 6);
    __m128i ss_23 = _mm_unpacklo_epi32(srcReg32b1, ss_3_1);
    __m128i ss_45 = _mm_unpacklo_epi32(ss_4_1, ss_5_1);

    ss_23 = _mm_madd_epi16(ss_23, secondFilters);
    ss_45 = _mm_madd_epi16(ss_45, thirdFilters);
    srcRegFilt32b1_1 = _mm_add_epi32(ss_23, ss_45);

    // shift by 7 bit each 32 bit
    srcRegFilt32b1_1 = _mm_add_epi32(srcRegFilt32b1_1, addFilterReg64);
    srcRegFilt32b1_1 = _mm_srai_epi32(srcRegFilt32b1_1, 7);

    srcRegFilt32b1_1 = _mm_packs_epi32(srcRegFilt32b1_1, _mm_setzero_si128());
    srcRegFilt32b1_1 = _mm_max_epi16(srcRegFilt32b1_1, _mm_setzero_si128());
    srcRegFilt32b1_1 = _mm_min_epi16(srcRegFilt32b1_1, max);

    src_ptr += src_pitch;

    _mm_storel_epi64((__m128i *)dst_ptr, srcRegFilt32b1_1);

    dst_ptr += dst_pitch;
  }
}

static void aom_highbd_filter_block1d8_v4_sse2(
    const uint16_t *src_ptr, ptrdiff_t src_pitch, uint16_t *dst_ptr,
    ptrdiff_t dst_pitch, uint32_t height, const int16_t *filter, int bd) {
  __m128i filtersReg;
  __m128i srcReg2, srcReg3, srcReg4, srcReg5, srcReg6;
  __m128i srcReg23_lo, srcReg23_hi, srcReg34_lo, srcReg34_hi;
  __m128i srcReg45_lo, srcReg45_hi, srcReg56_lo, srcReg56_hi;
  __m128i resReg23_lo, resReg34_lo, resReg45_lo, resReg56_lo;
  __m128i resReg23_hi, resReg34_hi, resReg45_hi, resReg56_hi;
  __m128i resReg23_45_lo, resReg34_56_lo, resReg23_45_hi, resReg34_56_hi;
  __m128i resReg23_45, resReg34_56;
  __m128i addFilterReg64, secondFilters, thirdFilters;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;

  const __m128i max = _mm_set1_epi16((1 << bd) - 1);
  addFilterReg64 = _mm_set1_epi32(64);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);

  // coeffs 0 1 0 1 2 3 2 3
  const __m128i tmp0 = _mm_unpacklo_epi32(filtersReg, filtersReg);
  // coeffs 4 5 4 5 6 7 6 7
  const __m128i tmp1 = _mm_unpackhi_epi32(filtersReg, filtersReg);

  secondFilters = _mm_unpackhi_epi64(tmp0, tmp0);  // coeffs 2 3 2 3 2 3 2 3
  thirdFilters = _mm_unpacklo_epi64(tmp1, tmp1);   // coeffs 4 5 4 5 4 5 4 5

  // multiple the size of the source and destination stride by two
  src_stride = src_pitch << 1;
  dst_stride = dst_pitch << 1;

  srcReg2 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 2));
  srcReg3 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 3));
  srcReg23_lo = _mm_unpacklo_epi16(srcReg2, srcReg3);
  srcReg23_hi = _mm_unpackhi_epi16(srcReg2, srcReg3);

  srcReg4 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 4));
  srcReg34_lo = _mm_unpacklo_epi16(srcReg3, srcReg4);
  srcReg34_hi = _mm_unpackhi_epi16(srcReg3, srcReg4);

  for (i = height; i > 1; i -= 2) {
    srcReg5 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 5));

    srcReg45_lo = _mm_unpacklo_epi16(srcReg4, srcReg5);
    srcReg45_hi = _mm_unpackhi_epi16(srcReg4, srcReg5);

    srcReg6 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 6));

    srcReg56_lo = _mm_unpacklo_epi16(srcReg5, srcReg6);
    srcReg56_hi = _mm_unpackhi_epi16(srcReg5, srcReg6);

    // multiply 2 adjacent elements with the filter and add the result

    resReg23_lo = _mm_madd_epi16(srcReg23_lo, secondFilters);
    resReg34_lo = _mm_madd_epi16(srcReg34_lo, secondFilters);
    resReg45_lo = _mm_madd_epi16(srcReg45_lo, thirdFilters);
    resReg56_lo = _mm_madd_epi16(srcReg56_lo, thirdFilters);

    resReg23_45_lo = _mm_add_epi32(resReg23_lo, resReg45_lo);
    resReg34_56_lo = _mm_add_epi32(resReg34_lo, resReg56_lo);

    // multiply 2 adjacent elements with the filter and add the result

    resReg23_hi = _mm_madd_epi16(srcReg23_hi, secondFilters);
    resReg34_hi = _mm_madd_epi16(srcReg34_hi, secondFilters);
    resReg45_hi = _mm_madd_epi16(srcReg45_hi, thirdFilters);
    resReg56_hi = _mm_madd_epi16(srcReg56_hi, thirdFilters);

    resReg23_45_hi = _mm_add_epi32(resReg23_hi, resReg45_hi);
    resReg34_56_hi = _mm_add_epi32(resReg34_hi, resReg56_hi);

    // shift by 7 bit each 32 bit
    resReg23_45_lo = _mm_add_epi32(resReg23_45_lo, addFilterReg64);
    resReg34_56_lo = _mm_add_epi32(resReg34_56_lo, addFilterReg64);
    resReg23_45_hi = _mm_add_epi32(resReg23_45_hi, addFilterReg64);
    resReg34_56_hi = _mm_add_epi32(resReg34_56_hi, addFilterReg64);
    resReg23_45_lo = _mm_srai_epi32(resReg23_45_lo, 7);
    resReg34_56_lo = _mm_srai_epi32(resReg34_56_lo, 7);
    resReg23_45_hi = _mm_srai_epi32(resReg23_45_hi, 7);
    resReg34_56_hi = _mm_srai_epi32(resReg34_56_hi, 7);

    // shrink to 16 bit each 32 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve
    // result
    resReg23_45 = _mm_packs_epi32(resReg23_45_lo, resReg23_45_hi);
    resReg34_56 = _mm_packs_epi32(resReg34_56_lo, resReg34_56_hi);

    resReg23_45 = _mm_max_epi16(resReg23_45, _mm_setzero_si128());
    resReg23_45 = _mm_min_epi16(resReg23_45, max);
    resReg34_56 = _mm_max_epi16(resReg34_56, _mm_setzero_si128());
    resReg34_56 = _mm_min_epi16(resReg34_56, max);

    src_ptr += src_stride;

    _mm_store_si128((__m128i *)dst_ptr, (resReg23_45));
    _mm_store_si128((__m128i *)(dst_ptr + dst_pitch), (resReg34_56));

    dst_ptr += dst_stride;

    // save part of the registers for next strides
    srcReg23_lo = srcReg45_lo;
    srcReg23_hi = srcReg45_hi;
    srcReg34_lo = srcReg56_lo;
    srcReg34_hi = srcReg56_hi;
    srcReg4 = srcReg6;
  }
}

static void aom_highbd_filter_block1d8_h4_sse2(
    const uint16_t *src_ptr, ptrdiff_t src_pitch, uint16_t *dst_ptr,
    ptrdiff_t dst_pitch, uint32_t height, const int16_t *filter, int bd) {
  __m128i filtersReg;
  __m128i addFilterReg64;
  __m128i secondFilters, thirdFilters;
  __m128i srcRegFilt32b1_1, srcRegFilt32b1_2;
  __m128i srcReg32b1, srcReg32b2;
  unsigned int i;
  src_ptr -= 3;
  addFilterReg64 = _mm_set1_epi32(64);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  const __m128i max = _mm_set1_epi16((1 << bd) - 1);

  // coeffs 0 1 0 1 2 3 2 3
  const __m128i tmp_0 = _mm_unpacklo_epi32(filtersReg, filtersReg);
  // coeffs 4 5 4 5 6 7 6 7
  const __m128i tmp_1 = _mm_unpackhi_epi32(filtersReg, filtersReg);

  secondFilters = _mm_unpackhi_epi64(tmp_0, tmp_0);  // coeffs 2 3 2 3 2 3 2 3
  thirdFilters = _mm_unpacklo_epi64(tmp_1, tmp_1);   // coeffs 4 5 4 5 4 5 4 5

  for (i = height; i > 0; i -= 1) {
    srcReg32b1 = _mm_loadu_si128((const __m128i *)(src_ptr + 2));
    srcReg32b2 = _mm_loadu_si128((const __m128i *)(src_ptr + 6));

    __m128i ss_4_1 = _mm_srli_si128(srcReg32b1, 4);
    __m128i ss_4_2 = _mm_srli_si128(srcReg32b2, 4);
    __m128i ss_4 = _mm_unpacklo_epi64(ss_4_1, ss_4_2);

    __m128i d1 = _mm_madd_epi16(srcReg32b1, secondFilters);
    __m128i d2 = _mm_madd_epi16(ss_4, thirdFilters);
    srcRegFilt32b1_1 = _mm_add_epi32(d1, d2);

    __m128i ss_3_1 = _mm_srli_si128(srcReg32b1, 2);
    __m128i ss_5_1 = _mm_srli_si128(srcReg32b1, 6);
    __m128i ss_3_2 = _mm_srli_si128(srcReg32b2, 2);
    __m128i ss_5_2 = _mm_srli_si128(srcReg32b2, 6);
    __m128i ss_3 = _mm_unpacklo_epi64(ss_3_1, ss_3_2);
    __m128i ss_5 = _mm_unpacklo_epi64(ss_5_1, ss_5_2);

    d1 = _mm_madd_epi16(ss_3, secondFilters);
    d2 = _mm_madd_epi16(ss_5, thirdFilters);
    srcRegFilt32b1_2 = _mm_add_epi32(d1, d2);

    __m128i res_lo_1 = _mm_unpacklo_epi32(srcRegFilt32b1_1, srcRegFilt32b1_2);
    __m128i res_hi_1 = _mm_unpackhi_epi32(srcRegFilt32b1_1, srcRegFilt32b1_2);

    // shift by 7 bit each 32 bit
    res_lo_1 = _mm_add_epi32(res_lo_1, addFilterReg64);
    res_hi_1 = _mm_add_epi32(res_hi_1, addFilterReg64);
    res_lo_1 = _mm_srai_epi32(res_lo_1, 7);
    res_hi_1 = _mm_srai_epi32(res_hi_1, 7);

    srcRegFilt32b1_1 = _mm_packs_epi32(res_lo_1, res_hi_1);

    srcRegFilt32b1_1 = _mm_max_epi16(srcRegFilt32b1_1, _mm_setzero_si128());
    srcRegFilt32b1_1 = _mm_min_epi16(srcRegFilt32b1_1, max);

    src_ptr += src_pitch;

    _mm_store_si128((__m128i *)dst_ptr, srcRegFilt32b1_1);

    dst_ptr += dst_pitch;
  }
}

static void aom_highbd_filter_block1d16_v4_sse2(
    const uint16_t *src_ptr, ptrdiff_t src_pitch, uint16_t *dst_ptr,
    ptrdiff_t dst_pitch, uint32_t height, const int16_t *filter, int bd) {
  aom_highbd_filter_block1d8_v4_sse2(src_ptr, src_pitch, dst_ptr, dst_pitch,
                                     height, filter, bd);
  aom_highbd_filter_block1d8_v4_sse2((src_ptr + 8), src_pitch, (dst_ptr + 8),
                                     dst_pitch, height, filter, bd);
}

static void aom_highbd_filter_block1d16_h4_sse2(
    const uint16_t *src_ptr, ptrdiff_t src_pitch, uint16_t *dst_ptr,
    ptrdiff_t dst_pitch, uint32_t height, const int16_t *filter, int bd) {
  aom_highbd_filter_block1d8_h4_sse2(src_ptr, src_pitch, dst_ptr, dst_pitch,
                                     height, filter, bd);
  aom_highbd_filter_block1d8_h4_sse2((src_ptr + 8), src_pitch, (dst_ptr + 8),
                                     dst_pitch, height, filter, bd);
}

// From aom_dsp/x86/aom_high_subpixel_8t_sse2.asm
highbd_filter8_1dfunction aom_highbd_filter_block1d16_v8_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d16_h8_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d8_v8_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d8_h8_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d4_v8_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d4_h8_sse2;

// From aom_dsp/x86/aom_high_subpixel_bilinear_sse2.asm
highbd_filter8_1dfunction aom_highbd_filter_block1d16_v2_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d16_h2_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d8_v2_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d8_h2_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d4_v2_sse2;
highbd_filter8_1dfunction aom_highbd_filter_block1d4_h2_sse2;

// void aom_highbd_convolve8_horiz_sse2(const uint8_t *src,
//                                      ptrdiff_t src_stride,
//                                      uint8_t *dst,
//                                      ptrdiff_t dst_stride,
//                                      const int16_t *filter_x,
//                                      int x_step_q4,
//                                      const int16_t *filter_y,
//                                      int y_step_q4,
//                                      int w, int h, int bd);
// void aom_highbd_convolve8_vert_sse2(const uint8_t *src,
//                                     ptrdiff_t src_stride,
//                                     uint8_t *dst,
//                                     ptrdiff_t dst_stride,
//                                     const int16_t *filter_x,
//                                     int x_step_q4,
//                                     const int16_t *filter_y,
//                                     int y_step_q4,
//                                     int w, int h, int bd);
HIGH_FUN_CONV_1D(horiz, x_step_q4, filter_x, h, src, , sse2)
HIGH_FUN_CONV_1D(vert, y_step_q4, filter_y, v, src - src_stride * 3, , sse2)
