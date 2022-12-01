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

#include <emmintrin.h>  // SSE2

#include "config/aom_dsp_rtcd.h"
#include "aom_dsp/x86/convolve.h"
#include "aom_ports/mem.h"

void aom_filter_block1d16_h4_sse2(const uint8_t *src_ptr,
                                  ptrdiff_t src_pixels_per_line,
                                  uint8_t *output_ptr, ptrdiff_t output_pitch,
                                  uint32_t output_height,
                                  const int16_t *filter) {
  __m128i filtersReg;
  __m128i addFilterReg32;
  __m128i secondFilters, thirdFilters;
  __m128i srcRegFilt32b1_1, srcRegFilt32b1_2, srcRegFilt32b2_1,
      srcRegFilt32b2_2;
  __m128i srcReg32b1, srcReg32b2;
  unsigned int i;
  src_ptr -= 3;
  addFilterReg32 = _mm_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  filtersReg = _mm_srai_epi16(filtersReg, 1);

  // coeffs 0 1 0 1 2 3 2 3
  const __m128i tmp_0 = _mm_unpacklo_epi32(filtersReg, filtersReg);
  // coeffs 4 5 4 5 6 7 6 7
  const __m128i tmp_1 = _mm_unpackhi_epi32(filtersReg, filtersReg);

  secondFilters = _mm_unpackhi_epi64(tmp_0, tmp_0);  // coeffs 2 3 2 3 2 3 2 3
  thirdFilters = _mm_unpacklo_epi64(tmp_1, tmp_1);   // coeffs 4 5 4 5 4 5 4 5

  for (i = output_height; i > 0; i -= 1) {
    srcReg32b1 = _mm_loadu_si128((const __m128i *)src_ptr);

    __m128i ss_2 = _mm_srli_si128(srcReg32b1, 2);
    __m128i ss_4 = _mm_srli_si128(srcReg32b1, 4);
    __m128i ss_1_1 = _mm_unpacklo_epi8(ss_2, _mm_setzero_si128());
    __m128i ss_2_1 = _mm_unpacklo_epi8(ss_4, _mm_setzero_si128());
    __m128i d1 = _mm_madd_epi16(ss_1_1, secondFilters);
    __m128i d2 = _mm_madd_epi16(ss_2_1, thirdFilters);
    srcRegFilt32b1_1 = _mm_add_epi32(d1, d2);

    __m128i ss_1 = _mm_srli_si128(srcReg32b1, 3);
    __m128i ss_3 = _mm_srli_si128(srcReg32b1, 5);
    __m128i ss_1_2 = _mm_unpacklo_epi8(ss_1, _mm_setzero_si128());
    __m128i ss_2_2 = _mm_unpacklo_epi8(ss_3, _mm_setzero_si128());
    d1 = _mm_madd_epi16(ss_1_2, secondFilters);
    d2 = _mm_madd_epi16(ss_2_2, thirdFilters);
    srcRegFilt32b1_2 = _mm_add_epi32(d1, d2);

    __m128i res_lo = _mm_unpacklo_epi32(srcRegFilt32b1_1, srcRegFilt32b1_2);
    __m128i res_hi = _mm_unpackhi_epi32(srcRegFilt32b1_1, srcRegFilt32b1_2);
    srcRegFilt32b1_1 = _mm_packs_epi32(res_lo, res_hi);

    // reading stride of the next 16 bytes
    // (part of it was being read by earlier read)
    srcReg32b2 = _mm_loadu_si128((const __m128i *)(src_ptr + 8));

    ss_2 = _mm_srli_si128(srcReg32b2, 2);
    ss_4 = _mm_srli_si128(srcReg32b2, 4);
    ss_1_1 = _mm_unpacklo_epi8(ss_2, _mm_setzero_si128());
    ss_2_1 = _mm_unpacklo_epi8(ss_4, _mm_setzero_si128());
    d1 = _mm_madd_epi16(ss_1_1, secondFilters);
    d2 = _mm_madd_epi16(ss_2_1, thirdFilters);
    srcRegFilt32b2_1 = _mm_add_epi32(d1, d2);

    ss_1 = _mm_srli_si128(srcReg32b2, 3);
    ss_3 = _mm_srli_si128(srcReg32b2, 5);
    ss_1_2 = _mm_unpacklo_epi8(ss_1, _mm_setzero_si128());
    ss_2_2 = _mm_unpacklo_epi8(ss_3, _mm_setzero_si128());
    d1 = _mm_madd_epi16(ss_1_2, secondFilters);
    d2 = _mm_madd_epi16(ss_2_2, thirdFilters);
    srcRegFilt32b2_2 = _mm_add_epi32(d1, d2);

    res_lo = _mm_unpacklo_epi32(srcRegFilt32b2_1, srcRegFilt32b2_2);
    res_hi = _mm_unpackhi_epi32(srcRegFilt32b2_1, srcRegFilt32b2_2);
    srcRegFilt32b2_1 = _mm_packs_epi32(res_lo, res_hi);

    // shift by 6 bit each 16 bit
    srcRegFilt32b1_1 = _mm_adds_epi16(srcRegFilt32b1_1, addFilterReg32);
    srcRegFilt32b2_1 = _mm_adds_epi16(srcRegFilt32b2_1, addFilterReg32);
    srcRegFilt32b1_1 = _mm_srai_epi16(srcRegFilt32b1_1, 6);
    srcRegFilt32b2_1 = _mm_srai_epi16(srcRegFilt32b2_1, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve result
    srcRegFilt32b1_1 = _mm_packus_epi16(srcRegFilt32b1_1, srcRegFilt32b2_1);

    src_ptr += src_pixels_per_line;

    _mm_store_si128((__m128i *)output_ptr, srcRegFilt32b1_1);

    output_ptr += output_pitch;
  }
}

void aom_filter_block1d16_v4_sse2(const uint8_t *src_ptr, ptrdiff_t src_pitch,
                                  uint8_t *output_ptr, ptrdiff_t out_pitch,
                                  uint32_t output_height,
                                  const int16_t *filter) {
  __m128i filtersReg;
  __m128i srcReg2, srcReg3, srcReg4, srcReg5, srcReg6;
  __m128i srcReg23_lo, srcReg23_hi, srcReg34_lo, srcReg34_hi;
  __m128i srcReg45_lo, srcReg45_hi, srcReg56_lo, srcReg56_hi;
  __m128i resReg23_lo, resReg34_lo, resReg45_lo, resReg56_lo;
  __m128i resReg23_hi, resReg34_hi, resReg45_hi, resReg56_hi;
  __m128i resReg23_45_lo, resReg34_56_lo, resReg23_45_hi, resReg34_56_hi;
  __m128i resReg23_45, resReg34_56;
  __m128i addFilterReg32, secondFilters, thirdFilters;
  __m128i tmp_0, tmp_1;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;

  addFilterReg32 = _mm_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  filtersReg = _mm_srai_epi16(filtersReg, 1);

  // coeffs 0 1 0 1 2 3 2 3
  const __m128i tmp0 = _mm_unpacklo_epi32(filtersReg, filtersReg);
  // coeffs 4 5 4 5 6 7 6 7
  const __m128i tmp1 = _mm_unpackhi_epi32(filtersReg, filtersReg);

  secondFilters = _mm_unpackhi_epi64(tmp0, tmp0);  // coeffs 2 3 2 3 2 3 2 3
  thirdFilters = _mm_unpacklo_epi64(tmp1, tmp1);   // coeffs 4 5 4 5 4 5 4 5

  // multiply the size of the source and destination stride by two
  src_stride = src_pitch << 1;
  dst_stride = out_pitch << 1;

  srcReg2 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 2));
  srcReg3 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 3));
  srcReg23_lo = _mm_unpacklo_epi8(srcReg2, srcReg3);
  srcReg23_hi = _mm_unpackhi_epi8(srcReg2, srcReg3);
  __m128i resReg23_lo_1 = _mm_unpacklo_epi8(srcReg23_lo, _mm_setzero_si128());
  __m128i resReg23_lo_2 = _mm_unpackhi_epi8(srcReg23_lo, _mm_setzero_si128());
  __m128i resReg23_hi_1 = _mm_unpacklo_epi8(srcReg23_hi, _mm_setzero_si128());
  __m128i resReg23_hi_2 = _mm_unpackhi_epi8(srcReg23_hi, _mm_setzero_si128());

  srcReg4 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 4));
  srcReg34_lo = _mm_unpacklo_epi8(srcReg3, srcReg4);
  srcReg34_hi = _mm_unpackhi_epi8(srcReg3, srcReg4);
  __m128i resReg34_lo_1 = _mm_unpacklo_epi8(srcReg34_lo, _mm_setzero_si128());
  __m128i resReg34_lo_2 = _mm_unpackhi_epi8(srcReg34_lo, _mm_setzero_si128());
  __m128i resReg34_hi_1 = _mm_unpacklo_epi8(srcReg34_hi, _mm_setzero_si128());
  __m128i resReg34_hi_2 = _mm_unpackhi_epi8(srcReg34_hi, _mm_setzero_si128());

  for (i = output_height; i > 1; i -= 2) {
    srcReg5 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 5));

    srcReg45_lo = _mm_unpacklo_epi8(srcReg4, srcReg5);
    srcReg45_hi = _mm_unpackhi_epi8(srcReg4, srcReg5);

    srcReg6 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 6));

    srcReg56_lo = _mm_unpacklo_epi8(srcReg5, srcReg6);
    srcReg56_hi = _mm_unpackhi_epi8(srcReg5, srcReg6);

    // multiply 2 adjacent elements with the filter and add the result

    tmp_0 = _mm_madd_epi16(resReg23_lo_1, secondFilters);
    tmp_1 = _mm_madd_epi16(resReg23_lo_2, secondFilters);
    resReg23_lo = _mm_packs_epi32(tmp_0, tmp_1);

    tmp_0 = _mm_madd_epi16(resReg34_lo_1, secondFilters);
    tmp_1 = _mm_madd_epi16(resReg34_lo_2, secondFilters);
    resReg34_lo = _mm_packs_epi32(tmp_0, tmp_1);

    __m128i resReg45_lo_1 = _mm_unpacklo_epi8(srcReg45_lo, _mm_setzero_si128());
    __m128i resReg45_lo_2 = _mm_unpackhi_epi8(srcReg45_lo, _mm_setzero_si128());
    tmp_0 = _mm_madd_epi16(resReg45_lo_1, thirdFilters);
    tmp_1 = _mm_madd_epi16(resReg45_lo_2, thirdFilters);
    resReg45_lo = _mm_packs_epi32(tmp_0, tmp_1);

    __m128i resReg56_lo_1 = _mm_unpacklo_epi8(srcReg56_lo, _mm_setzero_si128());
    __m128i resReg56_lo_2 = _mm_unpackhi_epi8(srcReg56_lo, _mm_setzero_si128());
    tmp_0 = _mm_madd_epi16(resReg56_lo_1, thirdFilters);
    tmp_1 = _mm_madd_epi16(resReg56_lo_2, thirdFilters);
    resReg56_lo = _mm_packs_epi32(tmp_0, tmp_1);

    // add and saturate the results together
    resReg23_45_lo = _mm_adds_epi16(resReg23_lo, resReg45_lo);
    resReg34_56_lo = _mm_adds_epi16(resReg34_lo, resReg56_lo);

    // multiply 2 adjacent elements with the filter and add the result

    tmp_0 = _mm_madd_epi16(resReg23_hi_1, secondFilters);
    tmp_1 = _mm_madd_epi16(resReg23_hi_2, secondFilters);
    resReg23_hi = _mm_packs_epi32(tmp_0, tmp_1);

    tmp_0 = _mm_madd_epi16(resReg34_hi_1, secondFilters);
    tmp_1 = _mm_madd_epi16(resReg34_hi_2, secondFilters);
    resReg34_hi = _mm_packs_epi32(tmp_0, tmp_1);

    __m128i resReg45_hi_1 = _mm_unpacklo_epi8(srcReg45_hi, _mm_setzero_si128());
    __m128i resReg45_hi_2 = _mm_unpackhi_epi8(srcReg45_hi, _mm_setzero_si128());
    tmp_0 = _mm_madd_epi16(resReg45_hi_1, thirdFilters);
    tmp_1 = _mm_madd_epi16(resReg45_hi_2, thirdFilters);
    resReg45_hi = _mm_packs_epi32(tmp_0, tmp_1);

    __m128i resReg56_hi_1 = _mm_unpacklo_epi8(srcReg56_hi, _mm_setzero_si128());
    __m128i resReg56_hi_2 = _mm_unpackhi_epi8(srcReg56_hi, _mm_setzero_si128());
    tmp_0 = _mm_madd_epi16(resReg56_hi_1, thirdFilters);
    tmp_1 = _mm_madd_epi16(resReg56_hi_2, thirdFilters);
    resReg56_hi = _mm_packs_epi32(tmp_0, tmp_1);

    // add and saturate the results together
    resReg23_45_hi = _mm_adds_epi16(resReg23_hi, resReg45_hi);
    resReg34_56_hi = _mm_adds_epi16(resReg34_hi, resReg56_hi);

    // shift by 6 bit each 16 bit
    resReg23_45_lo = _mm_adds_epi16(resReg23_45_lo, addFilterReg32);
    resReg34_56_lo = _mm_adds_epi16(resReg34_56_lo, addFilterReg32);
    resReg23_45_hi = _mm_adds_epi16(resReg23_45_hi, addFilterReg32);
    resReg34_56_hi = _mm_adds_epi16(resReg34_56_hi, addFilterReg32);
    resReg23_45_lo = _mm_srai_epi16(resReg23_45_lo, 6);
    resReg34_56_lo = _mm_srai_epi16(resReg34_56_lo, 6);
    resReg23_45_hi = _mm_srai_epi16(resReg23_45_hi, 6);
    resReg34_56_hi = _mm_srai_epi16(resReg34_56_hi, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve
    // result
    resReg23_45 = _mm_packus_epi16(resReg23_45_lo, resReg23_45_hi);
    resReg34_56 = _mm_packus_epi16(resReg34_56_lo, resReg34_56_hi);

    src_ptr += src_stride;

    _mm_store_si128((__m128i *)output_ptr, (resReg23_45));
    _mm_store_si128((__m128i *)(output_ptr + out_pitch), (resReg34_56));

    output_ptr += dst_stride;

    // save part of the registers for next strides
    resReg23_lo_1 = resReg45_lo_1;
    resReg23_lo_2 = resReg45_lo_2;
    resReg23_hi_1 = resReg45_hi_1;
    resReg23_hi_2 = resReg45_hi_2;
    resReg34_lo_1 = resReg56_lo_1;
    resReg34_lo_2 = resReg56_lo_2;
    resReg34_hi_1 = resReg56_hi_1;
    resReg34_hi_2 = resReg56_hi_2;
    srcReg4 = srcReg6;
  }
}

void aom_filter_block1d8_h4_sse2(const uint8_t *src_ptr,
                                 ptrdiff_t src_pixels_per_line,
                                 uint8_t *output_ptr, ptrdiff_t output_pitch,
                                 uint32_t output_height,
                                 const int16_t *filter) {
  __m128i filtersReg;
  __m128i addFilterReg32;
  __m128i secondFilters, thirdFilters;
  __m128i srcRegFilt32b1_1, srcRegFilt32b1_2;
  __m128i srcReg32b1;
  unsigned int i;
  src_ptr -= 3;
  addFilterReg32 = _mm_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  filtersReg = _mm_srai_epi16(filtersReg, 1);

  // coeffs 0 1 0 1 2 3 2 3
  const __m128i tmp_0 = _mm_unpacklo_epi32(filtersReg, filtersReg);
  // coeffs 4 5 4 5 6 7 6 7
  const __m128i tmp_1 = _mm_unpackhi_epi32(filtersReg, filtersReg);

  secondFilters = _mm_unpackhi_epi64(tmp_0, tmp_0);  // coeffs 2 3 2 3 2 3 2 3
  thirdFilters = _mm_unpacklo_epi64(tmp_1, tmp_1);   // coeffs 4 5 4 5 4 5 4 5

  for (i = output_height; i > 0; i -= 1) {
    srcReg32b1 = _mm_loadu_si128((const __m128i *)src_ptr);

    __m128i ss_2 = _mm_srli_si128(srcReg32b1, 2);
    __m128i ss_4 = _mm_srli_si128(srcReg32b1, 4);
    ss_2 = _mm_unpacklo_epi8(ss_2, _mm_setzero_si128());
    ss_4 = _mm_unpacklo_epi8(ss_4, _mm_setzero_si128());
    __m128i d1 = _mm_madd_epi16(ss_2, secondFilters);
    __m128i d2 = _mm_madd_epi16(ss_4, thirdFilters);
    srcRegFilt32b1_1 = _mm_add_epi32(d1, d2);

    __m128i ss_3 = _mm_srli_si128(srcReg32b1, 3);
    __m128i ss_5 = _mm_srli_si128(srcReg32b1, 5);
    ss_3 = _mm_unpacklo_epi8(ss_3, _mm_setzero_si128());
    ss_5 = _mm_unpacklo_epi8(ss_5, _mm_setzero_si128());
    d1 = _mm_madd_epi16(ss_3, secondFilters);
    d2 = _mm_madd_epi16(ss_5, thirdFilters);
    srcRegFilt32b1_2 = _mm_add_epi32(d1, d2);

    __m128i res_lo = _mm_unpacklo_epi32(srcRegFilt32b1_1, srcRegFilt32b1_2);
    __m128i res_hi = _mm_unpackhi_epi32(srcRegFilt32b1_1, srcRegFilt32b1_2);
    srcRegFilt32b1_1 = _mm_packs_epi32(res_lo, res_hi);

    // shift by 6 bit each 16 bit
    srcRegFilt32b1_1 = _mm_adds_epi16(srcRegFilt32b1_1, addFilterReg32);
    srcRegFilt32b1_1 = _mm_srai_epi16(srcRegFilt32b1_1, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve result
    srcRegFilt32b1_1 = _mm_packus_epi16(srcRegFilt32b1_1, _mm_setzero_si128());

    src_ptr += src_pixels_per_line;

    _mm_storel_epi64((__m128i *)output_ptr, srcRegFilt32b1_1);

    output_ptr += output_pitch;
  }
}

void aom_filter_block1d8_v4_sse2(const uint8_t *src_ptr, ptrdiff_t src_pitch,
                                 uint8_t *output_ptr, ptrdiff_t out_pitch,
                                 uint32_t output_height,
                                 const int16_t *filter) {
  __m128i filtersReg;
  __m128i srcReg2, srcReg3, srcReg4, srcReg5, srcReg6;
  __m128i srcReg23_lo, srcReg34_lo;
  __m128i srcReg45_lo, srcReg56_lo;
  __m128i resReg23_lo, resReg34_lo, resReg45_lo, resReg56_lo;
  __m128i resReg23_45_lo, resReg34_56_lo;
  __m128i resReg23_45, resReg34_56;
  __m128i addFilterReg32, secondFilters, thirdFilters;
  __m128i tmp_0, tmp_1;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;

  addFilterReg32 = _mm_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  filtersReg = _mm_srai_epi16(filtersReg, 1);

  // coeffs 0 1 0 1 2 3 2 3
  const __m128i tmp0 = _mm_unpacklo_epi32(filtersReg, filtersReg);
  // coeffs 4 5 4 5 6 7 6 7
  const __m128i tmp1 = _mm_unpackhi_epi32(filtersReg, filtersReg);

  secondFilters = _mm_unpackhi_epi64(tmp0, tmp0);  // coeffs 2 3 2 3 2 3 2 3
  thirdFilters = _mm_unpacklo_epi64(tmp1, tmp1);   // coeffs 4 5 4 5 4 5 4 5

  // multiply the size of the source and destination stride by two
  src_stride = src_pitch << 1;
  dst_stride = out_pitch << 1;

  srcReg2 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 2));
  srcReg3 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 3));
  srcReg23_lo = _mm_unpacklo_epi8(srcReg2, srcReg3);
  __m128i resReg23_lo_1 = _mm_unpacklo_epi8(srcReg23_lo, _mm_setzero_si128());
  __m128i resReg23_lo_2 = _mm_unpackhi_epi8(srcReg23_lo, _mm_setzero_si128());

  srcReg4 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 4));
  srcReg34_lo = _mm_unpacklo_epi8(srcReg3, srcReg4);
  __m128i resReg34_lo_1 = _mm_unpacklo_epi8(srcReg34_lo, _mm_setzero_si128());
  __m128i resReg34_lo_2 = _mm_unpackhi_epi8(srcReg34_lo, _mm_setzero_si128());

  for (i = output_height; i > 1; i -= 2) {
    srcReg5 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 5));
    srcReg45_lo = _mm_unpacklo_epi8(srcReg4, srcReg5);

    srcReg6 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 6));
    srcReg56_lo = _mm_unpacklo_epi8(srcReg5, srcReg6);

    // multiply 2 adjacent elements with the filter and add the result

    tmp_0 = _mm_madd_epi16(resReg23_lo_1, secondFilters);
    tmp_1 = _mm_madd_epi16(resReg23_lo_2, secondFilters);
    resReg23_lo = _mm_packs_epi32(tmp_0, tmp_1);

    tmp_0 = _mm_madd_epi16(resReg34_lo_1, secondFilters);
    tmp_1 = _mm_madd_epi16(resReg34_lo_2, secondFilters);
    resReg34_lo = _mm_packs_epi32(tmp_0, tmp_1);

    __m128i resReg45_lo_1 = _mm_unpacklo_epi8(srcReg45_lo, _mm_setzero_si128());
    __m128i resReg45_lo_2 = _mm_unpackhi_epi8(srcReg45_lo, _mm_setzero_si128());
    tmp_0 = _mm_madd_epi16(resReg45_lo_1, thirdFilters);
    tmp_1 = _mm_madd_epi16(resReg45_lo_2, thirdFilters);
    resReg45_lo = _mm_packs_epi32(tmp_0, tmp_1);

    __m128i resReg56_lo_1 = _mm_unpacklo_epi8(srcReg56_lo, _mm_setzero_si128());
    __m128i resReg56_lo_2 = _mm_unpackhi_epi8(srcReg56_lo, _mm_setzero_si128());
    tmp_0 = _mm_madd_epi16(resReg56_lo_1, thirdFilters);
    tmp_1 = _mm_madd_epi16(resReg56_lo_2, thirdFilters);
    resReg56_lo = _mm_packs_epi32(tmp_0, tmp_1);

    // add and saturate the results together
    resReg23_45_lo = _mm_adds_epi16(resReg23_lo, resReg45_lo);
    resReg34_56_lo = _mm_adds_epi16(resReg34_lo, resReg56_lo);

    // shift by 6 bit each 16 bit
    resReg23_45_lo = _mm_adds_epi16(resReg23_45_lo, addFilterReg32);
    resReg34_56_lo = _mm_adds_epi16(resReg34_56_lo, addFilterReg32);
    resReg23_45_lo = _mm_srai_epi16(resReg23_45_lo, 6);
    resReg34_56_lo = _mm_srai_epi16(resReg34_56_lo, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve
    // result
    resReg23_45 = _mm_packus_epi16(resReg23_45_lo, _mm_setzero_si128());
    resReg34_56 = _mm_packus_epi16(resReg34_56_lo, _mm_setzero_si128());

    src_ptr += src_stride;

    _mm_storel_epi64((__m128i *)output_ptr, (resReg23_45));
    _mm_storel_epi64((__m128i *)(output_ptr + out_pitch), (resReg34_56));

    output_ptr += dst_stride;

    // save part of the registers for next strides
    resReg23_lo_1 = resReg45_lo_1;
    resReg23_lo_2 = resReg45_lo_2;
    resReg34_lo_1 = resReg56_lo_1;
    resReg34_lo_2 = resReg56_lo_2;
    srcReg4 = srcReg6;
  }
}

void aom_filter_block1d4_h4_sse2(const uint8_t *src_ptr,
                                 ptrdiff_t src_pixels_per_line,
                                 uint8_t *output_ptr, ptrdiff_t output_pitch,
                                 uint32_t output_height,
                                 const int16_t *filter) {
  __m128i filtersReg;
  __m128i addFilterReg32;
  __m128i secondFilters, thirdFilters;
  __m128i srcRegFilt32b1_1;
  __m128i srcReg32b1;
  unsigned int i;
  src_ptr -= 3;
  addFilterReg32 = _mm_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  filtersReg = _mm_srai_epi16(filtersReg, 1);

  // coeffs 0 1 0 1 2 3 2 3
  const __m128i tmp_0 = _mm_unpacklo_epi32(filtersReg, filtersReg);
  // coeffs 4 5 4 5 6 7 6 7
  const __m128i tmp_1 = _mm_unpackhi_epi32(filtersReg, filtersReg);

  secondFilters = _mm_unpackhi_epi64(tmp_0, tmp_0);  // coeffs 2 3 2 3 2 3 2 3
  thirdFilters = _mm_unpacklo_epi64(tmp_1, tmp_1);   // coeffs 4 5 4 5 4 5 4 5

  for (i = output_height; i > 0; i -= 1) {
    srcReg32b1 = _mm_loadu_si128((const __m128i *)src_ptr);

    __m128i ss_2 = _mm_srli_si128(srcReg32b1, 2);
    __m128i ss_3 = _mm_srli_si128(srcReg32b1, 3);
    __m128i ss_4 = _mm_srli_si128(srcReg32b1, 4);
    __m128i ss_5 = _mm_srli_si128(srcReg32b1, 5);

    ss_2 = _mm_unpacklo_epi8(ss_2, _mm_setzero_si128());
    ss_3 = _mm_unpacklo_epi8(ss_3, _mm_setzero_si128());
    ss_4 = _mm_unpacklo_epi8(ss_4, _mm_setzero_si128());
    ss_5 = _mm_unpacklo_epi8(ss_5, _mm_setzero_si128());

    __m128i ss_1_1 = _mm_unpacklo_epi32(ss_2, ss_3);
    __m128i ss_1_2 = _mm_unpacklo_epi32(ss_4, ss_5);

    __m128i d1 = _mm_madd_epi16(ss_1_1, secondFilters);
    __m128i d2 = _mm_madd_epi16(ss_1_2, thirdFilters);
    srcRegFilt32b1_1 = _mm_add_epi32(d1, d2);

    srcRegFilt32b1_1 = _mm_packs_epi32(srcRegFilt32b1_1, _mm_setzero_si128());

    // shift by 6 bit each 16 bit
    srcRegFilt32b1_1 = _mm_adds_epi16(srcRegFilt32b1_1, addFilterReg32);
    srcRegFilt32b1_1 = _mm_srai_epi16(srcRegFilt32b1_1, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve result
    srcRegFilt32b1_1 = _mm_packus_epi16(srcRegFilt32b1_1, _mm_setzero_si128());

    src_ptr += src_pixels_per_line;

    *((int *)(output_ptr)) = _mm_cvtsi128_si32(srcRegFilt32b1_1);

    output_ptr += output_pitch;
  }
}

void aom_filter_block1d4_v4_sse2(const uint8_t *src_ptr, ptrdiff_t src_pitch,
                                 uint8_t *output_ptr, ptrdiff_t out_pitch,
                                 uint32_t output_height,
                                 const int16_t *filter) {
  __m128i filtersReg;
  __m128i srcReg2, srcReg3, srcReg4, srcReg5, srcReg6;
  __m128i srcReg23, srcReg34, srcReg45, srcReg56;
  __m128i resReg23_34, resReg45_56;
  __m128i resReg23_34_45_56;
  __m128i addFilterReg32, secondFilters, thirdFilters;
  __m128i tmp_0, tmp_1;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;

  addFilterReg32 = _mm_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  filtersReg = _mm_srai_epi16(filtersReg, 1);

  // coeffs 0 1 0 1 2 3 2 3
  const __m128i tmp0 = _mm_unpacklo_epi32(filtersReg, filtersReg);
  // coeffs 4 5 4 5 6 7 6 7
  const __m128i tmp1 = _mm_unpackhi_epi32(filtersReg, filtersReg);

  secondFilters = _mm_unpackhi_epi64(tmp0, tmp0);  // coeffs 2 3 2 3 2 3 2 3
  thirdFilters = _mm_unpacklo_epi64(tmp1, tmp1);   // coeffs 4 5 4 5 4 5 4 5

  // multiply the size of the source and destination stride by two
  src_stride = src_pitch << 1;
  dst_stride = out_pitch << 1;

  srcReg2 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 2));
  srcReg3 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 3));
  srcReg23 = _mm_unpacklo_epi8(srcReg2, srcReg3);
  __m128i resReg23 = _mm_unpacklo_epi8(srcReg23, _mm_setzero_si128());

  srcReg4 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 4));
  srcReg34 = _mm_unpacklo_epi8(srcReg3, srcReg4);
  __m128i resReg34 = _mm_unpacklo_epi8(srcReg34, _mm_setzero_si128());

  for (i = output_height; i > 1; i -= 2) {
    srcReg5 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 5));
    srcReg45 = _mm_unpacklo_epi8(srcReg4, srcReg5);
    srcReg6 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 6));
    srcReg56 = _mm_unpacklo_epi8(srcReg5, srcReg6);

    // multiply 2 adjacent elements with the filter and add the result
    tmp_0 = _mm_madd_epi16(resReg23, secondFilters);
    tmp_1 = _mm_madd_epi16(resReg34, secondFilters);
    resReg23_34 = _mm_packs_epi32(tmp_0, tmp_1);

    __m128i resReg45 = _mm_unpacklo_epi8(srcReg45, _mm_setzero_si128());
    __m128i resReg56 = _mm_unpacklo_epi8(srcReg56, _mm_setzero_si128());

    tmp_0 = _mm_madd_epi16(resReg45, thirdFilters);
    tmp_1 = _mm_madd_epi16(resReg56, thirdFilters);
    resReg45_56 = _mm_packs_epi32(tmp_0, tmp_1);

    // add and saturate the results together
    resReg23_34_45_56 = _mm_adds_epi16(resReg23_34, resReg45_56);

    // shift by 6 bit each 16 bit
    resReg23_34_45_56 = _mm_adds_epi16(resReg23_34_45_56, addFilterReg32);
    resReg23_34_45_56 = _mm_srai_epi16(resReg23_34_45_56, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve
    // result
    resReg23_34_45_56 =
        _mm_packus_epi16(resReg23_34_45_56, _mm_setzero_si128());

    src_ptr += src_stride;

    *((int *)(output_ptr)) = _mm_cvtsi128_si32(resReg23_34_45_56);
    *((int *)(output_ptr + out_pitch)) =
        _mm_cvtsi128_si32(_mm_srli_si128(resReg23_34_45_56, 4));

    output_ptr += dst_stride;

    // save part of the registers for next strides
    resReg23 = resReg45;
    resReg34 = resReg56;
    srcReg4 = srcReg6;
  }
}
