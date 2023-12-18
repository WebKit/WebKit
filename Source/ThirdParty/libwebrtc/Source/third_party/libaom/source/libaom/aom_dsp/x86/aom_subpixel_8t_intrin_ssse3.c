/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <tmmintrin.h>

#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/aom_filter.h"
#include "aom_dsp/x86/convolve.h"
#include "aom_dsp/x86/convolve_sse2.h"
#include "aom_dsp/x86/convolve_ssse3.h"
#include "aom_dsp/x86/mem_sse2.h"
#include "aom_dsp/x86/transpose_sse2.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "aom_ports/emmintrin_compat.h"

DECLARE_ALIGNED(32, static const uint8_t, filt_h4[]) = {
  0,  1,  1,  2,  2, 3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  0,  1,  1,
  2,  2,  3,  3,  4, 4,  5,  5,  6,  6,  7,  7,  8,  2,  3,  3,  4,  4,  5,
  5,  6,  6,  7,  7, 8,  8,  9,  9,  10, 2,  3,  3,  4,  4,  5,  5,  6,  6,
  7,  7,  8,  8,  9, 9,  10, 4,  5,  5,  6,  6,  7,  7,  8,  8,  9,  9,  10,
  10, 11, 11, 12, 4, 5,  5,  6,  6,  7,  7,  8,  8,  9,  9,  10, 10, 11, 11,
  12, 6,  7,  7,  8, 8,  9,  9,  10, 10, 11, 11, 12, 12, 13, 13, 14, 6,  7,
  7,  8,  8,  9,  9, 10, 10, 11, 11, 12, 12, 13, 13, 14
};

DECLARE_ALIGNED(32, static const uint8_t, filtd4[]) = {
  2, 3, 4, 5, 3, 4, 5, 6, 4, 5, 6, 7, 5, 6, 7, 8,
  2, 3, 4, 5, 3, 4, 5, 6, 4, 5, 6, 7, 5, 6, 7, 8,
};

static void aom_filter_block1d4_h4_ssse3(
    const uint8_t *src_ptr, ptrdiff_t src_pixels_per_line, uint8_t *output_ptr,
    ptrdiff_t output_pitch, uint32_t output_height, const int16_t *filter) {
  __m128i filtersReg;
  __m128i addFilterReg32, filt1Reg, firstFilters, srcReg32b1, srcRegFilt32b1_1;
  unsigned int i;
  src_ptr -= 3;
  addFilterReg32 = _mm_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  filtersReg = _mm_srai_epi16(filtersReg, 1);
  // converting the 16 bit (short) to 8 bit (byte) and have the same data
  // in both lanes of 128 bit register.
  filtersReg = _mm_packs_epi16(filtersReg, filtersReg);

  firstFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi32(0x5040302u));
  filt1Reg = _mm_load_si128((__m128i const *)(filtd4));

  for (i = output_height; i > 0; i -= 1) {
    // load the 2 strides of source
    srcReg32b1 = _mm_loadu_si128((const __m128i *)src_ptr);

    // filter the source buffer
    srcRegFilt32b1_1 = _mm_shuffle_epi8(srcReg32b1, filt1Reg);

    // multiply 4 adjacent elements with the filter and add the result
    srcRegFilt32b1_1 = _mm_maddubs_epi16(srcRegFilt32b1_1, firstFilters);

    srcRegFilt32b1_1 = _mm_hadds_epi16(srcRegFilt32b1_1, _mm_setzero_si128());

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

static void aom_filter_block1d4_v4_ssse3(
    const uint8_t *src_ptr, ptrdiff_t src_pitch, uint8_t *output_ptr,
    ptrdiff_t out_pitch, uint32_t output_height, const int16_t *filter) {
  __m128i filtersReg;
  __m128i addFilterReg32;
  __m128i srcReg2, srcReg3, srcReg23, srcReg4, srcReg34, srcReg5, srcReg45,
      srcReg6, srcReg56;
  __m128i srcReg23_34_lo, srcReg45_56_lo;
  __m128i srcReg2345_3456_lo, srcReg2345_3456_hi;
  __m128i resReglo, resReghi;
  __m128i firstFilters;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;

  addFilterReg32 = _mm_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  // converting the 16 bit (short) to  8 bit (byte) and have the
  // same data in both lanes of 128 bit register.
  filtersReg = _mm_srai_epi16(filtersReg, 1);
  filtersReg = _mm_packs_epi16(filtersReg, filtersReg);

  firstFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi32(0x5040302u));

  // multiple the size of the source and destination stride by two
  src_stride = src_pitch << 1;
  dst_stride = out_pitch << 1;

  srcReg2 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 2));
  srcReg3 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 3));
  srcReg23 = _mm_unpacklo_epi32(srcReg2, srcReg3);

  srcReg4 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 4));

  // have consecutive loads on the same 256 register
  srcReg34 = _mm_unpacklo_epi32(srcReg3, srcReg4);

  srcReg23_34_lo = _mm_unpacklo_epi8(srcReg23, srcReg34);

  for (i = output_height; i > 1; i -= 2) {
    srcReg5 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 5));
    srcReg45 = _mm_unpacklo_epi32(srcReg4, srcReg5);

    srcReg6 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 6));
    srcReg56 = _mm_unpacklo_epi32(srcReg5, srcReg6);

    // merge every two consecutive registers
    srcReg45_56_lo = _mm_unpacklo_epi8(srcReg45, srcReg56);

    srcReg2345_3456_lo = _mm_unpacklo_epi16(srcReg23_34_lo, srcReg45_56_lo);
    srcReg2345_3456_hi = _mm_unpackhi_epi16(srcReg23_34_lo, srcReg45_56_lo);

    // multiply 2 adjacent elements with the filter and add the result
    resReglo = _mm_maddubs_epi16(srcReg2345_3456_lo, firstFilters);
    resReghi = _mm_maddubs_epi16(srcReg2345_3456_hi, firstFilters);

    resReglo = _mm_hadds_epi16(resReglo, _mm_setzero_si128());
    resReghi = _mm_hadds_epi16(resReghi, _mm_setzero_si128());

    // shift by 6 bit each 16 bit
    resReglo = _mm_adds_epi16(resReglo, addFilterReg32);
    resReghi = _mm_adds_epi16(resReghi, addFilterReg32);
    resReglo = _mm_srai_epi16(resReglo, 6);
    resReghi = _mm_srai_epi16(resReghi, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve
    // result
    resReglo = _mm_packus_epi16(resReglo, resReglo);
    resReghi = _mm_packus_epi16(resReghi, resReghi);

    src_ptr += src_stride;

    *((int *)(output_ptr)) = _mm_cvtsi128_si32(resReglo);
    *((int *)(output_ptr + out_pitch)) = _mm_cvtsi128_si32(resReghi);

    output_ptr += dst_stride;

    // save part of the registers for next strides
    srcReg23_34_lo = srcReg45_56_lo;
    srcReg4 = srcReg6;
  }
}

static void aom_filter_block1d8_h4_ssse3(
    const uint8_t *src_ptr, ptrdiff_t src_pixels_per_line, uint8_t *output_ptr,
    ptrdiff_t output_pitch, uint32_t output_height, const int16_t *filter) {
  __m128i filtersReg;
  __m128i addFilterReg32, filt2Reg, filt3Reg;
  __m128i secondFilters, thirdFilters;
  __m128i srcRegFilt32b1_1, srcRegFilt32b2, srcRegFilt32b3;
  __m128i srcReg32b1;
  unsigned int i;
  src_ptr -= 3;
  addFilterReg32 = _mm_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  filtersReg = _mm_srai_epi16(filtersReg, 1);
  // converting the 16 bit (short) to 8 bit (byte) and have the same data
  // in both lanes of 128 bit register.
  filtersReg = _mm_packs_epi16(filtersReg, filtersReg);

  // duplicate only the second 16 bits (third and forth byte)
  // across 256 bit register
  secondFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x302u));
  // duplicate only the third 16 bits (fifth and sixth byte)
  // across 256 bit register
  thirdFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x504u));

  filt2Reg = _mm_load_si128((__m128i const *)(filt_h4 + 32));
  filt3Reg = _mm_load_si128((__m128i const *)(filt_h4 + 32 * 2));

  for (i = output_height; i > 0; i -= 1) {
    srcReg32b1 = _mm_loadu_si128((const __m128i *)src_ptr);

    // filter the source buffer
    srcRegFilt32b3 = _mm_shuffle_epi8(srcReg32b1, filt2Reg);
    srcRegFilt32b2 = _mm_shuffle_epi8(srcReg32b1, filt3Reg);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt32b3 = _mm_maddubs_epi16(srcRegFilt32b3, secondFilters);
    srcRegFilt32b2 = _mm_maddubs_epi16(srcRegFilt32b2, thirdFilters);

    srcRegFilt32b1_1 = _mm_adds_epi16(srcRegFilt32b3, srcRegFilt32b2);

    // shift by 6 bit each 16 bit
    srcRegFilt32b1_1 = _mm_adds_epi16(srcRegFilt32b1_1, addFilterReg32);
    srcRegFilt32b1_1 = _mm_srai_epi16(srcRegFilt32b1_1, 6);

    // shrink to 8 bit each 16 bits
    srcRegFilt32b1_1 = _mm_packus_epi16(srcRegFilt32b1_1, _mm_setzero_si128());

    src_ptr += src_pixels_per_line;

    _mm_storel_epi64((__m128i *)output_ptr, srcRegFilt32b1_1);

    output_ptr += output_pitch;
  }
}

static void aom_filter_block1d8_v4_ssse3(
    const uint8_t *src_ptr, ptrdiff_t src_pitch, uint8_t *output_ptr,
    ptrdiff_t out_pitch, uint32_t output_height, const int16_t *filter) {
  __m128i filtersReg;
  __m128i srcReg2, srcReg3, srcReg4, srcReg5, srcReg6;
  __m128i srcReg23, srcReg34, srcReg45, srcReg56;
  __m128i resReg23, resReg34, resReg45, resReg56;
  __m128i resReg23_45, resReg34_56;
  __m128i addFilterReg32, secondFilters, thirdFilters;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;

  addFilterReg32 = _mm_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  // converting the 16 bit (short) to  8 bit (byte) and have the
  // same data in both lanes of 128 bit register.
  filtersReg = _mm_srai_epi16(filtersReg, 1);
  filtersReg = _mm_packs_epi16(filtersReg, filtersReg);

  // duplicate only the second 16 bits (third and forth byte)
  // across 128 bit register
  secondFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x302u));
  // duplicate only the third 16 bits (fifth and sixth byte)
  // across 128 bit register
  thirdFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x504u));

  // multiple the size of the source and destination stride by two
  src_stride = src_pitch << 1;
  dst_stride = out_pitch << 1;

  srcReg2 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 2));
  srcReg3 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 3));
  srcReg23 = _mm_unpacklo_epi8(srcReg2, srcReg3);

  srcReg4 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 4));

  // have consecutive loads on the same 256 register
  srcReg34 = _mm_unpacklo_epi8(srcReg3, srcReg4);

  for (i = output_height; i > 1; i -= 2) {
    srcReg5 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 5));

    srcReg45 = _mm_unpacklo_epi8(srcReg4, srcReg5);

    srcReg6 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 6));

    srcReg56 = _mm_unpacklo_epi8(srcReg5, srcReg6);

    // multiply 2 adjacent elements with the filter and add the result
    resReg23 = _mm_maddubs_epi16(srcReg23, secondFilters);
    resReg34 = _mm_maddubs_epi16(srcReg34, secondFilters);
    resReg45 = _mm_maddubs_epi16(srcReg45, thirdFilters);
    resReg56 = _mm_maddubs_epi16(srcReg56, thirdFilters);

    // add and saturate the results together
    resReg23_45 = _mm_adds_epi16(resReg23, resReg45);
    resReg34_56 = _mm_adds_epi16(resReg34, resReg56);

    // shift by 6 bit each 16 bit
    resReg23_45 = _mm_adds_epi16(resReg23_45, addFilterReg32);
    resReg34_56 = _mm_adds_epi16(resReg34_56, addFilterReg32);
    resReg23_45 = _mm_srai_epi16(resReg23_45, 6);
    resReg34_56 = _mm_srai_epi16(resReg34_56, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve
    // result
    resReg23_45 = _mm_packus_epi16(resReg23_45, _mm_setzero_si128());
    resReg34_56 = _mm_packus_epi16(resReg34_56, _mm_setzero_si128());

    src_ptr += src_stride;

    _mm_storel_epi64((__m128i *)output_ptr, (resReg23_45));
    _mm_storel_epi64((__m128i *)(output_ptr + out_pitch), (resReg34_56));

    output_ptr += dst_stride;

    // save part of the registers for next strides
    srcReg23 = srcReg45;
    srcReg34 = srcReg56;
    srcReg4 = srcReg6;
  }
}

static void aom_filter_block1d16_h4_ssse3(
    const uint8_t *src_ptr, ptrdiff_t src_pixels_per_line, uint8_t *output_ptr,
    ptrdiff_t output_pitch, uint32_t output_height, const int16_t *filter) {
  __m128i filtersReg;
  __m128i addFilterReg32, filt2Reg, filt3Reg;
  __m128i secondFilters, thirdFilters;
  __m128i srcRegFilt32b1_1, srcRegFilt32b2_1, srcRegFilt32b2, srcRegFilt32b3;
  __m128i srcReg32b1, srcReg32b2;
  unsigned int i;
  src_ptr -= 3;
  addFilterReg32 = _mm_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  filtersReg = _mm_srai_epi16(filtersReg, 1);
  // converting the 16 bit (short) to 8 bit (byte) and have the same data
  // in both lanes of 128 bit register.
  filtersReg = _mm_packs_epi16(filtersReg, filtersReg);

  // duplicate only the second 16 bits (third and forth byte)
  // across 256 bit register
  secondFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x302u));
  // duplicate only the third 16 bits (fifth and sixth byte)
  // across 256 bit register
  thirdFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x504u));

  filt2Reg = _mm_load_si128((__m128i const *)(filt_h4 + 32));
  filt3Reg = _mm_load_si128((__m128i const *)(filt_h4 + 32 * 2));

  for (i = output_height; i > 0; i -= 1) {
    srcReg32b1 = _mm_loadu_si128((const __m128i *)src_ptr);

    // filter the source buffer
    srcRegFilt32b3 = _mm_shuffle_epi8(srcReg32b1, filt2Reg);
    srcRegFilt32b2 = _mm_shuffle_epi8(srcReg32b1, filt3Reg);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt32b3 = _mm_maddubs_epi16(srcRegFilt32b3, secondFilters);
    srcRegFilt32b2 = _mm_maddubs_epi16(srcRegFilt32b2, thirdFilters);

    srcRegFilt32b1_1 = _mm_adds_epi16(srcRegFilt32b3, srcRegFilt32b2);

    // reading stride of the next 16 bytes
    // (part of it was being read by earlier read)
    srcReg32b2 = _mm_loadu_si128((const __m128i *)(src_ptr + 8));

    // filter the source buffer
    srcRegFilt32b3 = _mm_shuffle_epi8(srcReg32b2, filt2Reg);
    srcRegFilt32b2 = _mm_shuffle_epi8(srcReg32b2, filt3Reg);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt32b3 = _mm_maddubs_epi16(srcRegFilt32b3, secondFilters);
    srcRegFilt32b2 = _mm_maddubs_epi16(srcRegFilt32b2, thirdFilters);

    // add and saturate the results together
    srcRegFilt32b2_1 = _mm_adds_epi16(srcRegFilt32b3, srcRegFilt32b2);

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

static void aom_filter_block1d16_v4_ssse3(
    const uint8_t *src_ptr, ptrdiff_t src_pitch, uint8_t *output_ptr,
    ptrdiff_t out_pitch, uint32_t output_height, const int16_t *filter) {
  __m128i filtersReg;
  __m128i srcReg2, srcReg3, srcReg4, srcReg5, srcReg6;
  __m128i srcReg23_lo, srcReg23_hi, srcReg34_lo, srcReg34_hi;
  __m128i srcReg45_lo, srcReg45_hi, srcReg56_lo, srcReg56_hi;
  __m128i resReg23_lo, resReg34_lo, resReg45_lo, resReg56_lo;
  __m128i resReg23_hi, resReg34_hi, resReg45_hi, resReg56_hi;
  __m128i resReg23_45_lo, resReg34_56_lo, resReg23_45_hi, resReg34_56_hi;
  __m128i resReg23_45, resReg34_56;
  __m128i addFilterReg32, secondFilters, thirdFilters;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;

  addFilterReg32 = _mm_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  // converting the 16 bit (short) to  8 bit (byte) and have the
  // same data in both lanes of 128 bit register.
  filtersReg = _mm_srai_epi16(filtersReg, 1);
  filtersReg = _mm_packs_epi16(filtersReg, filtersReg);

  // duplicate only the second 16 bits (third and forth byte)
  // across 128 bit register
  secondFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x302u));
  // duplicate only the third 16 bits (fifth and sixth byte)
  // across 128 bit register
  thirdFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x504u));

  // multiple the size of the source and destination stride by two
  src_stride = src_pitch << 1;
  dst_stride = out_pitch << 1;

  srcReg2 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 2));
  srcReg3 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 3));
  srcReg23_lo = _mm_unpacklo_epi8(srcReg2, srcReg3);
  srcReg23_hi = _mm_unpackhi_epi8(srcReg2, srcReg3);

  srcReg4 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 4));

  // have consecutive loads on the same 256 register
  srcReg34_lo = _mm_unpacklo_epi8(srcReg3, srcReg4);
  srcReg34_hi = _mm_unpackhi_epi8(srcReg3, srcReg4);

  for (i = output_height; i > 1; i -= 2) {
    srcReg5 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 5));

    srcReg45_lo = _mm_unpacklo_epi8(srcReg4, srcReg5);
    srcReg45_hi = _mm_unpackhi_epi8(srcReg4, srcReg5);

    srcReg6 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 6));

    srcReg56_lo = _mm_unpacklo_epi8(srcReg5, srcReg6);
    srcReg56_hi = _mm_unpackhi_epi8(srcReg5, srcReg6);

    // multiply 2 adjacent elements with the filter and add the result
    resReg23_lo = _mm_maddubs_epi16(srcReg23_lo, secondFilters);
    resReg34_lo = _mm_maddubs_epi16(srcReg34_lo, secondFilters);
    resReg45_lo = _mm_maddubs_epi16(srcReg45_lo, thirdFilters);
    resReg56_lo = _mm_maddubs_epi16(srcReg56_lo, thirdFilters);

    // add and saturate the results together
    resReg23_45_lo = _mm_adds_epi16(resReg23_lo, resReg45_lo);
    resReg34_56_lo = _mm_adds_epi16(resReg34_lo, resReg56_lo);

    // multiply 2 adjacent elements with the filter and add the result

    resReg23_hi = _mm_maddubs_epi16(srcReg23_hi, secondFilters);
    resReg34_hi = _mm_maddubs_epi16(srcReg34_hi, secondFilters);
    resReg45_hi = _mm_maddubs_epi16(srcReg45_hi, thirdFilters);
    resReg56_hi = _mm_maddubs_epi16(srcReg56_hi, thirdFilters);

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
    srcReg23_lo = srcReg45_lo;
    srcReg34_lo = srcReg56_lo;
    srcReg23_hi = srcReg45_hi;
    srcReg34_hi = srcReg56_hi;
    srcReg4 = srcReg6;
  }
}

static INLINE __m128i shuffle_filter_convolve8_8_ssse3(
    const __m128i *const s, const int16_t *const filter) {
  __m128i f[4];
  shuffle_filter_ssse3(filter, f);
  return convolve8_8_ssse3(s, f);
}

static void filter_horiz_w8_ssse3(const uint8_t *const src,
                                  const ptrdiff_t src_stride,
                                  uint8_t *const dst,
                                  const int16_t *const x_filter) {
  __m128i s[8], ss[4], temp;

  load_8bit_8x8(src, src_stride, s);
  // 00 01 10 11 20 21 30 31  40 41 50 51 60 61 70 71
  // 02 03 12 13 22 23 32 33  42 43 52 53 62 63 72 73
  // 04 05 14 15 24 25 34 35  44 45 54 55 64 65 74 75
  // 06 07 16 17 26 27 36 37  46 47 56 57 66 67 76 77
  transpose_16bit_4x8(s, ss);
  temp = shuffle_filter_convolve8_8_ssse3(ss, x_filter);
  // shrink to 8 bit each 16 bits
  temp = _mm_packus_epi16(temp, temp);
  // save only 8 bytes convolve result
  _mm_storel_epi64((__m128i *)dst, temp);
}

static void transpose8x8_to_dst(const uint8_t *const src,
                                const ptrdiff_t src_stride, uint8_t *const dst,
                                const ptrdiff_t dst_stride) {
  __m128i s[8];

  load_8bit_8x8(src, src_stride, s);
  transpose_8bit_8x8(s, s);
  store_8bit_8x8(s, dst, dst_stride);
}

static void scaledconvolve_horiz_w8(const uint8_t *src,
                                    const ptrdiff_t src_stride, uint8_t *dst,
                                    const ptrdiff_t dst_stride,
                                    const InterpKernel *const x_filters,
                                    const int x0_q4, const int x_step_q4,
                                    const int w, const int h) {
  DECLARE_ALIGNED(16, uint8_t, temp[8 * 8]);
  int x, y, z;
  src -= SUBPEL_TAPS / 2 - 1;

  // This function processes 8x8 areas. The intermediate height is not always
  // a multiple of 8, so force it to be a multiple of 8 here.
  y = h + (8 - (h & 0x7));

  do {
    int x_q4 = x0_q4;
    for (x = 0; x < w; x += 8) {
      // process 8 src_x steps
      for (z = 0; z < 8; ++z) {
        const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
        const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
        if (x_q4 & SUBPEL_MASK) {
          filter_horiz_w8_ssse3(src_x, src_stride, temp + (z * 8), x_filter);
        } else {
          int i;
          for (i = 0; i < 8; ++i) {
            temp[z * 8 + i] = src_x[i * src_stride + 3];
          }
        }
        x_q4 += x_step_q4;
      }

      // transpose the 8x8 filters values back to dst
      transpose8x8_to_dst(temp, 8, dst + x, dst_stride);
    }

    src += src_stride * 8;
    dst += dst_stride * 8;
  } while (y -= 8);
}

static void filter_horiz_w4_ssse3(const uint8_t *const src,
                                  const ptrdiff_t src_stride,
                                  uint8_t *const dst,
                                  const int16_t *const filter) {
  __m128i s[4];
  __m128i temp;

  load_8bit_8x4(src, src_stride, s);
  transpose_16bit_4x4(s, s);

  temp = shuffle_filter_convolve8_8_ssse3(s, filter);
  // shrink to 8 bit each 16 bits
  temp = _mm_packus_epi16(temp, temp);
  // save only 4 bytes
  *(int *)dst = _mm_cvtsi128_si32(temp);
}

static void transpose4x4_to_dst(const uint8_t *const src,
                                const ptrdiff_t src_stride, uint8_t *const dst,
                                const ptrdiff_t dst_stride) {
  __m128i s[4];

  load_8bit_4x4(src, src_stride, s);
  s[0] = transpose_8bit_4x4(s);
  s[1] = _mm_srli_si128(s[0], 4);
  s[2] = _mm_srli_si128(s[0], 8);
  s[3] = _mm_srli_si128(s[0], 12);
  store_8bit_4x4(s, dst, dst_stride);
}

static void scaledconvolve_horiz_w4(const uint8_t *src,
                                    const ptrdiff_t src_stride, uint8_t *dst,
                                    const ptrdiff_t dst_stride,
                                    const InterpKernel *const x_filters,
                                    const int x0_q4, const int x_step_q4,
                                    const int w, const int h) {
  DECLARE_ALIGNED(16, uint8_t, temp[4 * 4]);
  int x, y, z;
  src -= SUBPEL_TAPS / 2 - 1;

  for (y = 0; y < h; y += 4) {
    int x_q4 = x0_q4;
    for (x = 0; x < w; x += 4) {
      // process 4 src_x steps
      for (z = 0; z < 4; ++z) {
        const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
        const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
        if (x_q4 & SUBPEL_MASK) {
          filter_horiz_w4_ssse3(src_x, src_stride, temp + (z * 4), x_filter);
        } else {
          int i;
          for (i = 0; i < 4; ++i) {
            temp[z * 4 + i] = src_x[i * src_stride + 3];
          }
        }
        x_q4 += x_step_q4;
      }

      // transpose the 4x4 filters values back to dst
      transpose4x4_to_dst(temp, 4, dst + x, dst_stride);
    }

    src += src_stride * 4;
    dst += dst_stride * 4;
  }
}

static __m128i filter_vert_kernel(const __m128i *const s,
                                  const int16_t *const filter) {
  __m128i ss[4];
  __m128i temp;

  // 00 10 01 11 02 12 03 13
  ss[0] = _mm_unpacklo_epi8(s[0], s[1]);
  // 20 30 21 31 22 32 23 33
  ss[1] = _mm_unpacklo_epi8(s[2], s[3]);
  // 40 50 41 51 42 52 43 53
  ss[2] = _mm_unpacklo_epi8(s[4], s[5]);
  // 60 70 61 71 62 72 63 73
  ss[3] = _mm_unpacklo_epi8(s[6], s[7]);

  temp = shuffle_filter_convolve8_8_ssse3(ss, filter);
  // shrink to 8 bit each 16 bits
  return _mm_packus_epi16(temp, temp);
}

static void filter_vert_w4_ssse3(const uint8_t *const src,
                                 const ptrdiff_t src_stride, uint8_t *const dst,
                                 const int16_t *const filter) {
  __m128i s[8];
  __m128i temp;

  load_8bit_4x8(src, src_stride, s);
  temp = filter_vert_kernel(s, filter);
  // save only 4 bytes
  *(int *)dst = _mm_cvtsi128_si32(temp);
}

static void scaledconvolve_vert_w4(
    const uint8_t *src, const ptrdiff_t src_stride, uint8_t *const dst,
    const ptrdiff_t dst_stride, const InterpKernel *const y_filters,
    const int y0_q4, const int y_step_q4, const int w, const int h) {
  int y;
  int y_q4 = y0_q4;

  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  for (y = 0; y < h; ++y) {
    const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
    const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];

    if (y_q4 & SUBPEL_MASK) {
      filter_vert_w4_ssse3(src_y, src_stride, &dst[y * dst_stride], y_filter);
    } else {
      memcpy(&dst[y * dst_stride], &src_y[3 * src_stride], w);
    }

    y_q4 += y_step_q4;
  }
}

static void filter_vert_w8_ssse3(const uint8_t *const src,
                                 const ptrdiff_t src_stride, uint8_t *const dst,
                                 const int16_t *const filter) {
  __m128i s[8], temp;

  load_8bit_8x8(src, src_stride, s);
  temp = filter_vert_kernel(s, filter);
  // save only 8 bytes convolve result
  _mm_storel_epi64((__m128i *)dst, temp);
}

static void scaledconvolve_vert_w8(
    const uint8_t *src, const ptrdiff_t src_stride, uint8_t *const dst,
    const ptrdiff_t dst_stride, const InterpKernel *const y_filters,
    const int y0_q4, const int y_step_q4, const int w, const int h) {
  int y;
  int y_q4 = y0_q4;

  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  for (y = 0; y < h; ++y) {
    const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
    const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
    if (y_q4 & SUBPEL_MASK) {
      filter_vert_w8_ssse3(src_y, src_stride, &dst[y * dst_stride], y_filter);
    } else {
      memcpy(&dst[y * dst_stride], &src_y[3 * src_stride], w);
    }
    y_q4 += y_step_q4;
  }
}

static void filter_vert_w16_ssse3(const uint8_t *src,
                                  const ptrdiff_t src_stride,
                                  uint8_t *const dst,
                                  const int16_t *const filter, const int w) {
  int i;
  __m128i f[4];
  shuffle_filter_ssse3(filter, f);

  for (i = 0; i < w; i += 16) {
    __m128i s[8], s_lo[4], s_hi[4], temp_lo, temp_hi;

    loadu_8bit_16x8(src, src_stride, s);

    // merge the result together
    s_lo[0] = _mm_unpacklo_epi8(s[0], s[1]);
    s_hi[0] = _mm_unpackhi_epi8(s[0], s[1]);
    s_lo[1] = _mm_unpacklo_epi8(s[2], s[3]);
    s_hi[1] = _mm_unpackhi_epi8(s[2], s[3]);
    s_lo[2] = _mm_unpacklo_epi8(s[4], s[5]);
    s_hi[2] = _mm_unpackhi_epi8(s[4], s[5]);
    s_lo[3] = _mm_unpacklo_epi8(s[6], s[7]);
    s_hi[3] = _mm_unpackhi_epi8(s[6], s[7]);
    temp_lo = convolve8_8_ssse3(s_lo, f);
    temp_hi = convolve8_8_ssse3(s_hi, f);

    // shrink to 8 bit each 16 bits, the first lane contain the first convolve
    // result and the second lane contain the second convolve result
    temp_hi = _mm_packus_epi16(temp_lo, temp_hi);
    src += 16;
    // save 16 bytes convolve result
    _mm_store_si128((__m128i *)&dst[i], temp_hi);
  }
}

static void scaledconvolve_vert_w16(
    const uint8_t *src, const ptrdiff_t src_stride, uint8_t *const dst,
    const ptrdiff_t dst_stride, const InterpKernel *const y_filters,
    const int y0_q4, const int y_step_q4, const int w, const int h) {
  int y;
  int y_q4 = y0_q4;

  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  for (y = 0; y < h; ++y) {
    const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
    const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
    if (y_q4 & SUBPEL_MASK) {
      filter_vert_w16_ssse3(src_y, src_stride, &dst[y * dst_stride], y_filter,
                            w);
    } else {
      memcpy(&dst[y * dst_stride], &src_y[3 * src_stride], w);
    }
    y_q4 += y_step_q4;
  }
}

void aom_scaled_2d_ssse3(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
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

filter8_1dfunction aom_filter_block1d16_v8_ssse3;
filter8_1dfunction aom_filter_block1d16_h8_ssse3;
filter8_1dfunction aom_filter_block1d8_v8_ssse3;
filter8_1dfunction aom_filter_block1d8_h8_ssse3;
filter8_1dfunction aom_filter_block1d4_v8_ssse3;
filter8_1dfunction aom_filter_block1d4_h8_ssse3;

filter8_1dfunction aom_filter_block1d16_v2_ssse3;
filter8_1dfunction aom_filter_block1d16_h2_ssse3;
filter8_1dfunction aom_filter_block1d8_v2_ssse3;
filter8_1dfunction aom_filter_block1d8_h2_ssse3;
filter8_1dfunction aom_filter_block1d4_v2_ssse3;
filter8_1dfunction aom_filter_block1d4_h2_ssse3;

// void aom_convolve8_horiz_ssse3(const uint8_t *src, ptrdiff_t src_stride,
//                                uint8_t *dst, ptrdiff_t dst_stride,
//                                const int16_t *filter_x, int x_step_q4,
//                                const int16_t *filter_y, int y_step_q4,
//                                int w, int h);
// void aom_convolve8_vert_ssse3(const uint8_t *src, ptrdiff_t src_stride,
//                               uint8_t *dst, ptrdiff_t dst_stride,
//                               const int16_t *filter_x, int x_step_q4,
//                               const int16_t *filter_y, int y_step_q4,
//                               int w, int h);
FUN_CONV_1D(horiz, x_step_q4, filter_x, h, src, , ssse3)
FUN_CONV_1D(vert, y_step_q4, filter_y, v, src - src_stride * 3, , ssse3)
