/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <immintrin.h>

#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/x86/convolve.h"
#include "aom_dsp/x86/convolve_avx2.h"
#include "aom_dsp/x86/synonyms_avx2.h"
#include "aom_ports/mem.h"

#if defined(__clang__)
#if (__clang_major__ > 0 && __clang_major__ < 3) ||            \
    (__clang_major__ == 3 && __clang_minor__ <= 3) ||          \
    (defined(__APPLE__) && defined(__apple_build_version__) && \
     ((__clang_major__ == 4 && __clang_minor__ <= 2) ||        \
      (__clang_major__ == 5 && __clang_minor__ == 0)))
#define MM256_BROADCASTSI128_SI256(x) \
  _mm_broadcastsi128_si256((__m128i const *)&(x))
#else  // clang > 3.3, and not 5.0 on macosx.
#define MM256_BROADCASTSI128_SI256(x) _mm256_broadcastsi128_si256(x)
#endif  // clang <= 3.3
#elif defined(__GNUC__)
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ <= 6)
#define MM256_BROADCASTSI128_SI256(x) \
  _mm_broadcastsi128_si256((__m128i const *)&(x))
#elif __GNUC__ == 4 && __GNUC_MINOR__ == 7
#define MM256_BROADCASTSI128_SI256(x) _mm_broadcastsi128_si256(x)
#else  // gcc > 4.7
#define MM256_BROADCASTSI128_SI256(x) _mm256_broadcastsi128_si256(x)
#endif  // gcc <= 4.6
#else   // !(gcc || clang)
#define MM256_BROADCASTSI128_SI256(x) _mm256_broadcastsi128_si256(x)
#endif  // __clang__

static INLINE void xx_storeu2_epi32(const uint8_t *output_ptr,
                                    const ptrdiff_t stride, const __m256i *a) {
  *((int *)(output_ptr)) = _mm_cvtsi128_si32(_mm256_castsi256_si128(*a));
  *((int *)(output_ptr + stride)) =
      _mm_cvtsi128_si32(_mm256_extracti128_si256(*a, 1));
}

static INLINE __m256i xx_loadu2_epi64(const void *hi, const void *lo) {
  __m256i a = _mm256_castsi128_si256(_mm_loadl_epi64((const __m128i *)(lo)));
  a = _mm256_inserti128_si256(a, _mm_loadl_epi64((const __m128i *)(hi)), 1);
  return a;
}

static INLINE void xx_storeu2_epi64(const uint8_t *output_ptr,
                                    const ptrdiff_t stride, const __m256i *a) {
  _mm_storel_epi64((__m128i *)output_ptr, _mm256_castsi256_si128(*a));
  _mm_storel_epi64((__m128i *)(output_ptr + stride),
                   _mm256_extractf128_si256(*a, 1));
}

static INLINE void xx_store2_mi128(const uint8_t *output_ptr,
                                   const ptrdiff_t stride, const __m256i *a) {
  _mm_store_si128((__m128i *)output_ptr, _mm256_castsi256_si128(*a));
  _mm_store_si128((__m128i *)(output_ptr + stride),
                  _mm256_extractf128_si256(*a, 1));
}

static void aom_filter_block1d4_h4_avx2(
    const uint8_t *src_ptr, ptrdiff_t src_pixels_per_line, uint8_t *output_ptr,
    ptrdiff_t output_pitch, uint32_t output_height, const int16_t *filter) {
  __m128i filtersReg;
  __m256i addFilterReg32, filt1Reg, firstFilters, srcReg32b1, srcRegFilt32b1_1;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;
  src_ptr -= 3;
  addFilterReg32 = _mm256_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  filtersReg = _mm_srai_epi16(filtersReg, 1);
  // converting the 16 bit (short) to 8 bit (byte) and have the same data
  // in both lanes of 128 bit register.
  filtersReg = _mm_packs_epi16(filtersReg, filtersReg);
  // have the same data in both lanes of a 256 bit register
  const __m256i filtersReg32 = MM256_BROADCASTSI128_SI256(filtersReg);

  firstFilters =
      _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi32(0x5040302u));
  filt1Reg = _mm256_load_si256((__m256i const *)(filt4_d4_global_avx2));

  // multiple the size of the source and destination stride by two
  src_stride = src_pixels_per_line << 1;
  dst_stride = output_pitch << 1;
  for (i = output_height; i > 1; i -= 2) {
    // load the 2 strides of source
    srcReg32b1 = yy_loadu2_128(src_ptr + src_pixels_per_line, src_ptr);

    // filter the source buffer
    srcRegFilt32b1_1 = _mm256_shuffle_epi8(srcReg32b1, filt1Reg);

    // multiply 4 adjacent elements with the filter and add the result
    srcRegFilt32b1_1 = _mm256_maddubs_epi16(srcRegFilt32b1_1, firstFilters);

    srcRegFilt32b1_1 =
        _mm256_hadds_epi16(srcRegFilt32b1_1, _mm256_setzero_si256());

    // shift by 6 bit each 16 bit
    srcRegFilt32b1_1 = _mm256_adds_epi16(srcRegFilt32b1_1, addFilterReg32);
    srcRegFilt32b1_1 = _mm256_srai_epi16(srcRegFilt32b1_1, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve result
    srcRegFilt32b1_1 =
        _mm256_packus_epi16(srcRegFilt32b1_1, _mm256_setzero_si256());

    src_ptr += src_stride;

    xx_storeu2_epi32(output_ptr, output_pitch, &srcRegFilt32b1_1);
    output_ptr += dst_stride;
  }

  // if the number of strides is odd.
  // process only 4 bytes
  if (i > 0) {
    __m128i srcReg1, srcRegFilt1_1;

    srcReg1 = _mm_loadu_si128((const __m128i *)(src_ptr));

    // filter the source buffer
    srcRegFilt1_1 = _mm_shuffle_epi8(srcReg1, _mm256_castsi256_si128(filt1Reg));

    // multiply 4 adjacent elements with the filter and add the result
    srcRegFilt1_1 =
        _mm_maddubs_epi16(srcRegFilt1_1, _mm256_castsi256_si128(firstFilters));

    srcRegFilt1_1 = _mm_hadds_epi16(srcRegFilt1_1, _mm_setzero_si128());
    // shift by 6 bit each 16 bit
    srcRegFilt1_1 =
        _mm_adds_epi16(srcRegFilt1_1, _mm256_castsi256_si128(addFilterReg32));
    srcRegFilt1_1 = _mm_srai_epi16(srcRegFilt1_1, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve result
    srcRegFilt1_1 = _mm_packus_epi16(srcRegFilt1_1, _mm_setzero_si128());

    // save 4 bytes
    *((int *)(output_ptr)) = _mm_cvtsi128_si32(srcRegFilt1_1);
  }
}

static void aom_filter_block1d4_h8_avx2(
    const uint8_t *src_ptr, ptrdiff_t src_pixels_per_line, uint8_t *output_ptr,
    ptrdiff_t output_pitch, uint32_t output_height, const int16_t *filter) {
  __m128i filtersReg;
  __m256i addFilterReg32, filt1Reg, filt2Reg;
  __m256i firstFilters, secondFilters;
  __m256i srcRegFilt32b1_1, srcRegFilt32b2;
  __m256i srcReg32b1;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;
  src_ptr -= 3;
  addFilterReg32 = _mm256_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  filtersReg = _mm_srai_epi16(filtersReg, 1);
  // converting the 16 bit (short) to 8 bit (byte) and have the same data
  // in both lanes of 128 bit register.
  filtersReg = _mm_packs_epi16(filtersReg, filtersReg);
  // have the same data in both lanes of a 256 bit register
  const __m256i filtersReg32 = MM256_BROADCASTSI128_SI256(filtersReg);

  // duplicate only the first 32 bits
  firstFilters = _mm256_shuffle_epi32(filtersReg32, 0);
  // duplicate only the second 32 bits
  secondFilters = _mm256_shuffle_epi32(filtersReg32, 0x55);

  filt1Reg = _mm256_load_si256((__m256i const *)filt_d4_global_avx2);
  filt2Reg = _mm256_load_si256((__m256i const *)(filt_d4_global_avx2 + 32));

  // multiple the size of the source and destination stride by two
  src_stride = src_pixels_per_line << 1;
  dst_stride = output_pitch << 1;
  for (i = output_height; i > 1; i -= 2) {
    // load the 2 strides of source
    srcReg32b1 = yy_loadu2_128(src_ptr + src_pixels_per_line, src_ptr);

    // filter the source buffer
    srcRegFilt32b1_1 = _mm256_shuffle_epi8(srcReg32b1, filt1Reg);

    // multiply 4 adjacent elements with the filter and add the result
    srcRegFilt32b1_1 = _mm256_maddubs_epi16(srcRegFilt32b1_1, firstFilters);

    // filter the source buffer
    srcRegFilt32b2 = _mm256_shuffle_epi8(srcReg32b1, filt2Reg);

    // multiply 4 adjacent elements with the filter and add the result
    srcRegFilt32b2 = _mm256_maddubs_epi16(srcRegFilt32b2, secondFilters);

    srcRegFilt32b1_1 = _mm256_adds_epi16(srcRegFilt32b1_1, srcRegFilt32b2);

    srcRegFilt32b1_1 =
        _mm256_hadds_epi16(srcRegFilt32b1_1, _mm256_setzero_si256());

    // shift by 6 bit each 16 bit
    srcRegFilt32b1_1 = _mm256_adds_epi16(srcRegFilt32b1_1, addFilterReg32);
    srcRegFilt32b1_1 = _mm256_srai_epi16(srcRegFilt32b1_1, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve result
    srcRegFilt32b1_1 =
        _mm256_packus_epi16(srcRegFilt32b1_1, _mm256_setzero_si256());

    src_ptr += src_stride;

    xx_storeu2_epi32(output_ptr, output_pitch, &srcRegFilt32b1_1);
    output_ptr += dst_stride;
  }

  // if the number of strides is odd.
  // process only 4 bytes
  if (i > 0) {
    __m128i srcReg1, srcRegFilt1_1;
    __m128i srcRegFilt2;

    srcReg1 = _mm_loadu_si128((const __m128i *)(src_ptr));

    // filter the source buffer
    srcRegFilt1_1 = _mm_shuffle_epi8(srcReg1, _mm256_castsi256_si128(filt1Reg));

    // multiply 4 adjacent elements with the filter and add the result
    srcRegFilt1_1 =
        _mm_maddubs_epi16(srcRegFilt1_1, _mm256_castsi256_si128(firstFilters));

    // filter the source buffer
    srcRegFilt2 = _mm_shuffle_epi8(srcReg1, _mm256_castsi256_si128(filt2Reg));

    // multiply 4 adjacent elements with the filter and add the result
    srcRegFilt2 =
        _mm_maddubs_epi16(srcRegFilt2, _mm256_castsi256_si128(secondFilters));

    srcRegFilt1_1 = _mm_adds_epi16(srcRegFilt1_1, srcRegFilt2);
    srcRegFilt1_1 = _mm_hadds_epi16(srcRegFilt1_1, _mm_setzero_si128());
    // shift by 6 bit each 16 bit
    srcRegFilt1_1 =
        _mm_adds_epi16(srcRegFilt1_1, _mm256_castsi256_si128(addFilterReg32));
    srcRegFilt1_1 = _mm_srai_epi16(srcRegFilt1_1, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve result
    srcRegFilt1_1 = _mm_packus_epi16(srcRegFilt1_1, _mm_setzero_si128());

    // save 4 bytes
    *((int *)(output_ptr)) = _mm_cvtsi128_si32(srcRegFilt1_1);
  }
}

static void aom_filter_block1d8_h4_avx2(
    const uint8_t *src_ptr, ptrdiff_t src_pixels_per_line, uint8_t *output_ptr,
    ptrdiff_t output_pitch, uint32_t output_height, const int16_t *filter) {
  __m128i filtersReg;
  __m256i addFilterReg32, filt2Reg, filt3Reg;
  __m256i secondFilters, thirdFilters;
  __m256i srcRegFilt32b1_1, srcRegFilt32b2, srcRegFilt32b3;
  __m256i srcReg32b1, filtersReg32;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;
  src_ptr -= 3;
  addFilterReg32 = _mm256_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  filtersReg = _mm_srai_epi16(filtersReg, 1);
  // converting the 16 bit (short) to 8 bit (byte) and have the same data
  // in both lanes of 128 bit register.
  filtersReg = _mm_packs_epi16(filtersReg, filtersReg);
  // have the same data in both lanes of a 256 bit register
  filtersReg32 = MM256_BROADCASTSI128_SI256(filtersReg);

  // duplicate only the second 16 bits (third and forth byte)
  // across 256 bit register
  secondFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x302u));
  // duplicate only the third 16 bits (fifth and sixth byte)
  // across 256 bit register
  thirdFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x504u));

  filt2Reg = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32));
  filt3Reg = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 2));

  // multiply the size of the source and destination stride by two
  src_stride = src_pixels_per_line << 1;
  dst_stride = output_pitch << 1;
  for (i = output_height; i > 1; i -= 2) {
    // load the 2 strides of source
    srcReg32b1 = yy_loadu2_128(src_ptr + src_pixels_per_line, src_ptr);

    // filter the source buffer
    srcRegFilt32b3 = _mm256_shuffle_epi8(srcReg32b1, filt2Reg);
    srcRegFilt32b2 = _mm256_shuffle_epi8(srcReg32b1, filt3Reg);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt32b3 = _mm256_maddubs_epi16(srcRegFilt32b3, secondFilters);
    srcRegFilt32b2 = _mm256_maddubs_epi16(srcRegFilt32b2, thirdFilters);

    srcRegFilt32b1_1 = _mm256_adds_epi16(srcRegFilt32b3, srcRegFilt32b2);

    // shift by 6 bit each 16 bit
    srcRegFilt32b1_1 = _mm256_adds_epi16(srcRegFilt32b1_1, addFilterReg32);
    srcRegFilt32b1_1 = _mm256_srai_epi16(srcRegFilt32b1_1, 6);

    // shrink to 8 bit each 16 bits
    srcRegFilt32b1_1 = _mm256_packus_epi16(srcRegFilt32b1_1, srcRegFilt32b1_1);

    src_ptr += src_stride;

    xx_storeu2_epi64(output_ptr, output_pitch, &srcRegFilt32b1_1);
    output_ptr += dst_stride;
  }

  // if the number of strides is odd.
  // process only 8 bytes
  if (i > 0) {
    __m128i srcReg1, srcRegFilt1_1;
    __m128i srcRegFilt2, srcRegFilt3;

    srcReg1 = _mm_loadu_si128((const __m128i *)(src_ptr));

    // filter the source buffer
    srcRegFilt2 = _mm_shuffle_epi8(srcReg1, _mm256_castsi256_si128(filt2Reg));
    srcRegFilt3 = _mm_shuffle_epi8(srcReg1, _mm256_castsi256_si128(filt3Reg));

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt2 =
        _mm_maddubs_epi16(srcRegFilt2, _mm256_castsi256_si128(secondFilters));
    srcRegFilt3 =
        _mm_maddubs_epi16(srcRegFilt3, _mm256_castsi256_si128(thirdFilters));

    // add and saturate the results together
    srcRegFilt1_1 = _mm_adds_epi16(srcRegFilt2, srcRegFilt3);

    // shift by 6 bit each 16 bit
    srcRegFilt1_1 =
        _mm_adds_epi16(srcRegFilt1_1, _mm256_castsi256_si128(addFilterReg32));
    srcRegFilt1_1 = _mm_srai_epi16(srcRegFilt1_1, 6);

    // shrink to 8 bit each 16 bits
    srcRegFilt1_1 = _mm_packus_epi16(srcRegFilt1_1, _mm_setzero_si128());

    // save 8 bytes
    _mm_storel_epi64((__m128i *)output_ptr, srcRegFilt1_1);
  }
}

static void aom_filter_block1d8_h8_avx2(
    const uint8_t *src_ptr, ptrdiff_t src_pixels_per_line, uint8_t *output_ptr,
    ptrdiff_t output_pitch, uint32_t output_height, const int16_t *filter) {
  __m128i filtersReg;
  __m256i addFilterReg32, filt1Reg, filt2Reg, filt3Reg, filt4Reg;
  __m256i firstFilters, secondFilters, thirdFilters, forthFilters;
  __m256i srcRegFilt32b1_1, srcRegFilt32b2, srcRegFilt32b3;
  __m256i srcReg32b1;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;
  src_ptr -= 3;
  addFilterReg32 = _mm256_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  filtersReg = _mm_srai_epi16(filtersReg, 1);
  // converting the 16 bit (short) to 8 bit (byte) and have the same data
  // in both lanes of 128 bit register.
  filtersReg = _mm_packs_epi16(filtersReg, filtersReg);
  // have the same data in both lanes of a 256 bit register
  const __m256i filtersReg32 = MM256_BROADCASTSI128_SI256(filtersReg);

  // duplicate only the first 16 bits (first and second byte)
  // across 256 bit register
  firstFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x100u));
  // duplicate only the second 16 bits (third and forth byte)
  // across 256 bit register
  secondFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x302u));
  // duplicate only the third 16 bits (fifth and sixth byte)
  // across 256 bit register
  thirdFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x504u));
  // duplicate only the forth 16 bits (seventh and eighth byte)
  // across 256 bit register
  forthFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x706u));

  filt1Reg = _mm256_load_si256((__m256i const *)filt_global_avx2);
  filt2Reg = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32));
  filt3Reg = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 2));
  filt4Reg = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 3));

  // multiple the size of the source and destination stride by two
  src_stride = src_pixels_per_line << 1;
  dst_stride = output_pitch << 1;
  for (i = output_height; i > 1; i -= 2) {
    // load the 2 strides of source
    srcReg32b1 = yy_loadu2_128(src_ptr + src_pixels_per_line, src_ptr);

    // filter the source buffer
    srcRegFilt32b1_1 = _mm256_shuffle_epi8(srcReg32b1, filt1Reg);
    srcRegFilt32b2 = _mm256_shuffle_epi8(srcReg32b1, filt4Reg);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt32b1_1 = _mm256_maddubs_epi16(srcRegFilt32b1_1, firstFilters);
    srcRegFilt32b2 = _mm256_maddubs_epi16(srcRegFilt32b2, forthFilters);

    // add and saturate the results together
    srcRegFilt32b1_1 = _mm256_adds_epi16(srcRegFilt32b1_1, srcRegFilt32b2);

    // filter the source buffer
    srcRegFilt32b3 = _mm256_shuffle_epi8(srcReg32b1, filt2Reg);
    srcRegFilt32b2 = _mm256_shuffle_epi8(srcReg32b1, filt3Reg);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt32b3 = _mm256_maddubs_epi16(srcRegFilt32b3, secondFilters);
    srcRegFilt32b2 = _mm256_maddubs_epi16(srcRegFilt32b2, thirdFilters);

    __m256i sum23 = _mm256_adds_epi16(srcRegFilt32b3, srcRegFilt32b2);
    srcRegFilt32b1_1 = _mm256_adds_epi16(srcRegFilt32b1_1, sum23);

    // shift by 6 bit each 16 bit
    srcRegFilt32b1_1 = _mm256_adds_epi16(srcRegFilt32b1_1, addFilterReg32);
    srcRegFilt32b1_1 = _mm256_srai_epi16(srcRegFilt32b1_1, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve result
    srcRegFilt32b1_1 =
        _mm256_packus_epi16(srcRegFilt32b1_1, _mm256_setzero_si256());

    src_ptr += src_stride;

    xx_storeu2_epi64(output_ptr, output_pitch, &srcRegFilt32b1_1);
    output_ptr += dst_stride;
  }

  // if the number of strides is odd.
  // process only 8 bytes
  if (i > 0) {
    __m128i srcReg1, srcRegFilt1_1;
    __m128i srcRegFilt2, srcRegFilt3;

    srcReg1 = _mm_loadu_si128((const __m128i *)(src_ptr));

    // filter the source buffer
    srcRegFilt1_1 = _mm_shuffle_epi8(srcReg1, _mm256_castsi256_si128(filt1Reg));
    srcRegFilt2 = _mm_shuffle_epi8(srcReg1, _mm256_castsi256_si128(filt4Reg));

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt1_1 =
        _mm_maddubs_epi16(srcRegFilt1_1, _mm256_castsi256_si128(firstFilters));
    srcRegFilt2 =
        _mm_maddubs_epi16(srcRegFilt2, _mm256_castsi256_si128(forthFilters));

    // add and saturate the results together
    srcRegFilt1_1 = _mm_adds_epi16(srcRegFilt1_1, srcRegFilt2);

    // filter the source buffer
    srcRegFilt3 = _mm_shuffle_epi8(srcReg1, _mm256_castsi256_si128(filt2Reg));
    srcRegFilt2 = _mm_shuffle_epi8(srcReg1, _mm256_castsi256_si128(filt3Reg));

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt3 =
        _mm_maddubs_epi16(srcRegFilt3, _mm256_castsi256_si128(secondFilters));
    srcRegFilt2 =
        _mm_maddubs_epi16(srcRegFilt2, _mm256_castsi256_si128(thirdFilters));

    // add and saturate the results together
    srcRegFilt1_1 =
        _mm_adds_epi16(srcRegFilt1_1, _mm_adds_epi16(srcRegFilt3, srcRegFilt2));

    // shift by 6 bit each 16 bit
    srcRegFilt1_1 =
        _mm_adds_epi16(srcRegFilt1_1, _mm256_castsi256_si128(addFilterReg32));
    srcRegFilt1_1 = _mm_srai_epi16(srcRegFilt1_1, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve
    // result
    srcRegFilt1_1 = _mm_packus_epi16(srcRegFilt1_1, _mm_setzero_si128());

    // save 8 bytes
    _mm_storel_epi64((__m128i *)output_ptr, srcRegFilt1_1);
  }
}

static void aom_filter_block1d16_h4_avx2(
    const uint8_t *src_ptr, ptrdiff_t src_pixels_per_line, uint8_t *output_ptr,
    ptrdiff_t output_pitch, uint32_t output_height, const int16_t *filter) {
  __m128i filtersReg;
  __m256i addFilterReg32, filt2Reg, filt3Reg;
  __m256i secondFilters, thirdFilters;
  __m256i srcRegFilt32b1_1, srcRegFilt32b2_1, srcRegFilt32b2, srcRegFilt32b3;
  __m256i srcReg32b1, srcReg32b2, filtersReg32;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;
  src_ptr -= 3;
  addFilterReg32 = _mm256_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  filtersReg = _mm_srai_epi16(filtersReg, 1);
  // converting the 16 bit (short) to 8 bit (byte) and have the same data
  // in both lanes of 128 bit register.
  filtersReg = _mm_packs_epi16(filtersReg, filtersReg);
  // have the same data in both lanes of a 256 bit register
  filtersReg32 = MM256_BROADCASTSI128_SI256(filtersReg);

  // duplicate only the second 16 bits (third and forth byte)
  // across 256 bit register
  secondFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x302u));
  // duplicate only the third 16 bits (fifth and sixth byte)
  // across 256 bit register
  thirdFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x504u));

  filt2Reg = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32));
  filt3Reg = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 2));

  // multiply the size of the source and destination stride by two
  src_stride = src_pixels_per_line << 1;
  dst_stride = output_pitch << 1;
  for (i = output_height; i > 1; i -= 2) {
    // load the 2 strides of source
    srcReg32b1 = yy_loadu2_128(src_ptr + src_pixels_per_line, src_ptr);

    // filter the source buffer
    srcRegFilt32b3 = _mm256_shuffle_epi8(srcReg32b1, filt2Reg);
    srcRegFilt32b2 = _mm256_shuffle_epi8(srcReg32b1, filt3Reg);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt32b3 = _mm256_maddubs_epi16(srcRegFilt32b3, secondFilters);
    srcRegFilt32b2 = _mm256_maddubs_epi16(srcRegFilt32b2, thirdFilters);

    srcRegFilt32b1_1 = _mm256_adds_epi16(srcRegFilt32b3, srcRegFilt32b2);

    // reading 2 strides of the next 16 bytes
    // (part of it was being read by earlier read)
    srcReg32b2 = yy_loadu2_128(src_ptr + src_pixels_per_line + 8, src_ptr + 8);

    // filter the source buffer
    srcRegFilt32b3 = _mm256_shuffle_epi8(srcReg32b2, filt2Reg);
    srcRegFilt32b2 = _mm256_shuffle_epi8(srcReg32b2, filt3Reg);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt32b3 = _mm256_maddubs_epi16(srcRegFilt32b3, secondFilters);
    srcRegFilt32b2 = _mm256_maddubs_epi16(srcRegFilt32b2, thirdFilters);

    // add and saturate the results together
    srcRegFilt32b2_1 = _mm256_adds_epi16(srcRegFilt32b3, srcRegFilt32b2);

    // shift by 6 bit each 16 bit
    srcRegFilt32b1_1 = _mm256_adds_epi16(srcRegFilt32b1_1, addFilterReg32);
    srcRegFilt32b2_1 = _mm256_adds_epi16(srcRegFilt32b2_1, addFilterReg32);
    srcRegFilt32b1_1 = _mm256_srai_epi16(srcRegFilt32b1_1, 6);
    srcRegFilt32b2_1 = _mm256_srai_epi16(srcRegFilt32b2_1, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve result
    srcRegFilt32b1_1 = _mm256_packus_epi16(srcRegFilt32b1_1, srcRegFilt32b2_1);

    src_ptr += src_stride;

    xx_store2_mi128(output_ptr, output_pitch, &srcRegFilt32b1_1);
    output_ptr += dst_stride;
  }

  // if the number of strides is odd.
  // process only 16 bytes
  if (i > 0) {
    __m256i srcReg1, srcReg12;
    __m256i srcRegFilt2, srcRegFilt3, srcRegFilt1_1;

    srcReg1 = _mm256_loadu_si256((const __m256i *)(src_ptr));
    srcReg12 = _mm256_permute4x64_epi64(srcReg1, 0x94);

    // filter the source buffer
    srcRegFilt2 = _mm256_shuffle_epi8(srcReg12, filt2Reg);
    srcRegFilt3 = _mm256_shuffle_epi8(srcReg12, filt3Reg);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt2 = _mm256_maddubs_epi16(srcRegFilt2, secondFilters);
    srcRegFilt3 = _mm256_maddubs_epi16(srcRegFilt3, thirdFilters);

    // add and saturate the results together
    srcRegFilt1_1 = _mm256_adds_epi16(srcRegFilt2, srcRegFilt3);

    // shift by 6 bit each 16 bit
    srcRegFilt1_1 = _mm256_adds_epi16(srcRegFilt1_1, addFilterReg32);
    srcRegFilt1_1 = _mm256_srai_epi16(srcRegFilt1_1, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve
    // result
    srcRegFilt1_1 = _mm256_packus_epi16(srcRegFilt1_1, srcRegFilt1_1);
    srcRegFilt1_1 = _mm256_permute4x64_epi64(srcRegFilt1_1, 0x8);

    // save 16 bytes
    _mm_store_si128((__m128i *)output_ptr,
                    _mm256_castsi256_si128(srcRegFilt1_1));
  }
}

static void aom_filter_block1d16_h8_avx2(
    const uint8_t *src_ptr, ptrdiff_t src_pixels_per_line, uint8_t *output_ptr,
    ptrdiff_t output_pitch, uint32_t output_height, const int16_t *filter) {
  __m128i filtersReg;
  __m256i addFilterReg32, filt1Reg, filt2Reg, filt3Reg, filt4Reg;
  __m256i firstFilters, secondFilters, thirdFilters, forthFilters;
  __m256i srcRegFilt32b1_1, srcRegFilt32b2_1, srcRegFilt32b2, srcRegFilt32b3;
  __m256i srcReg32b1, srcReg32b2, filtersReg32;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;
  src_ptr -= 3;
  addFilterReg32 = _mm256_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  filtersReg = _mm_srai_epi16(filtersReg, 1);
  // converting the 16 bit (short) to 8 bit (byte) and have the same data
  // in both lanes of 128 bit register.
  filtersReg = _mm_packs_epi16(filtersReg, filtersReg);
  // have the same data in both lanes of a 256 bit register
  filtersReg32 = MM256_BROADCASTSI128_SI256(filtersReg);

  // duplicate only the first 16 bits (first and second byte)
  // across 256 bit register
  firstFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x100u));
  // duplicate only the second 16 bits (third and forth byte)
  // across 256 bit register
  secondFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x302u));
  // duplicate only the third 16 bits (fifth and sixth byte)
  // across 256 bit register
  thirdFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x504u));
  // duplicate only the forth 16 bits (seventh and eighth byte)
  // across 256 bit register
  forthFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x706u));

  filt1Reg = _mm256_load_si256((__m256i const *)filt_global_avx2);
  filt2Reg = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32));
  filt3Reg = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 2));
  filt4Reg = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 3));

  // multiple the size of the source and destination stride by two
  src_stride = src_pixels_per_line << 1;
  dst_stride = output_pitch << 1;
  for (i = output_height; i > 1; i -= 2) {
    // load the 2 strides of source
    srcReg32b1 = yy_loadu2_128(src_ptr + src_pixels_per_line, src_ptr);

    // filter the source buffer
    srcRegFilt32b1_1 = _mm256_shuffle_epi8(srcReg32b1, filt1Reg);
    srcRegFilt32b2 = _mm256_shuffle_epi8(srcReg32b1, filt4Reg);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt32b1_1 = _mm256_maddubs_epi16(srcRegFilt32b1_1, firstFilters);
    srcRegFilt32b2 = _mm256_maddubs_epi16(srcRegFilt32b2, forthFilters);

    // add and saturate the results together
    srcRegFilt32b1_1 = _mm256_adds_epi16(srcRegFilt32b1_1, srcRegFilt32b2);

    // filter the source buffer
    srcRegFilt32b3 = _mm256_shuffle_epi8(srcReg32b1, filt2Reg);
    srcRegFilt32b2 = _mm256_shuffle_epi8(srcReg32b1, filt3Reg);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt32b3 = _mm256_maddubs_epi16(srcRegFilt32b3, secondFilters);
    srcRegFilt32b2 = _mm256_maddubs_epi16(srcRegFilt32b2, thirdFilters);

    __m256i sum23 = _mm256_adds_epi16(srcRegFilt32b3, srcRegFilt32b2);
    srcRegFilt32b1_1 = _mm256_adds_epi16(srcRegFilt32b1_1, sum23);

    // reading 2 strides of the next 16 bytes
    // (part of it was being read by earlier read)
    srcReg32b2 = yy_loadu2_128(src_ptr + src_pixels_per_line + 8, src_ptr + 8);

    // filter the source buffer
    srcRegFilt32b2_1 = _mm256_shuffle_epi8(srcReg32b2, filt1Reg);
    srcRegFilt32b2 = _mm256_shuffle_epi8(srcReg32b2, filt4Reg);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt32b2_1 = _mm256_maddubs_epi16(srcRegFilt32b2_1, firstFilters);
    srcRegFilt32b2 = _mm256_maddubs_epi16(srcRegFilt32b2, forthFilters);

    // add and saturate the results together
    srcRegFilt32b2_1 = _mm256_adds_epi16(srcRegFilt32b2_1, srcRegFilt32b2);

    // filter the source buffer
    srcRegFilt32b3 = _mm256_shuffle_epi8(srcReg32b2, filt2Reg);
    srcRegFilt32b2 = _mm256_shuffle_epi8(srcReg32b2, filt3Reg);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt32b3 = _mm256_maddubs_epi16(srcRegFilt32b3, secondFilters);
    srcRegFilt32b2 = _mm256_maddubs_epi16(srcRegFilt32b2, thirdFilters);

    // add and saturate the results together
    srcRegFilt32b2_1 = _mm256_adds_epi16(
        srcRegFilt32b2_1, _mm256_adds_epi16(srcRegFilt32b3, srcRegFilt32b2));

    // shift by 6 bit each 16 bit
    srcRegFilt32b1_1 = _mm256_adds_epi16(srcRegFilt32b1_1, addFilterReg32);
    srcRegFilt32b2_1 = _mm256_adds_epi16(srcRegFilt32b2_1, addFilterReg32);
    srcRegFilt32b1_1 = _mm256_srai_epi16(srcRegFilt32b1_1, 6);
    srcRegFilt32b2_1 = _mm256_srai_epi16(srcRegFilt32b2_1, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve result
    srcRegFilt32b1_1 = _mm256_packus_epi16(srcRegFilt32b1_1, srcRegFilt32b2_1);

    src_ptr += src_stride;

    xx_store2_mi128(output_ptr, output_pitch, &srcRegFilt32b1_1);
    output_ptr += dst_stride;
  }

  // if the number of strides is odd.
  // process only 16 bytes
  if (i > 0) {
    __m128i srcReg1, srcReg2, srcRegFilt1_1, srcRegFilt2_1;
    __m128i srcRegFilt2, srcRegFilt3;

    srcReg1 = _mm_loadu_si128((const __m128i *)(src_ptr));

    // filter the source buffer
    srcRegFilt1_1 = _mm_shuffle_epi8(srcReg1, _mm256_castsi256_si128(filt1Reg));
    srcRegFilt2 = _mm_shuffle_epi8(srcReg1, _mm256_castsi256_si128(filt4Reg));

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt1_1 =
        _mm_maddubs_epi16(srcRegFilt1_1, _mm256_castsi256_si128(firstFilters));
    srcRegFilt2 =
        _mm_maddubs_epi16(srcRegFilt2, _mm256_castsi256_si128(forthFilters));

    // add and saturate the results together
    srcRegFilt1_1 = _mm_adds_epi16(srcRegFilt1_1, srcRegFilt2);

    // filter the source buffer
    srcRegFilt3 = _mm_shuffle_epi8(srcReg1, _mm256_castsi256_si128(filt2Reg));
    srcRegFilt2 = _mm_shuffle_epi8(srcReg1, _mm256_castsi256_si128(filt3Reg));

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt3 =
        _mm_maddubs_epi16(srcRegFilt3, _mm256_castsi256_si128(secondFilters));
    srcRegFilt2 =
        _mm_maddubs_epi16(srcRegFilt2, _mm256_castsi256_si128(thirdFilters));

    // add and saturate the results together
    srcRegFilt1_1 =
        _mm_adds_epi16(srcRegFilt1_1, _mm_adds_epi16(srcRegFilt3, srcRegFilt2));

    // reading the next 16 bytes
    // (part of it was being read by earlier read)
    srcReg2 = _mm_loadu_si128((const __m128i *)(src_ptr + 8));

    // filter the source buffer
    srcRegFilt2_1 = _mm_shuffle_epi8(srcReg2, _mm256_castsi256_si128(filt1Reg));
    srcRegFilt2 = _mm_shuffle_epi8(srcReg2, _mm256_castsi256_si128(filt4Reg));

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt2_1 =
        _mm_maddubs_epi16(srcRegFilt2_1, _mm256_castsi256_si128(firstFilters));
    srcRegFilt2 =
        _mm_maddubs_epi16(srcRegFilt2, _mm256_castsi256_si128(forthFilters));

    // add and saturate the results together
    srcRegFilt2_1 = _mm_adds_epi16(srcRegFilt2_1, srcRegFilt2);

    // filter the source buffer
    srcRegFilt3 = _mm_shuffle_epi8(srcReg2, _mm256_castsi256_si128(filt2Reg));
    srcRegFilt2 = _mm_shuffle_epi8(srcReg2, _mm256_castsi256_si128(filt3Reg));

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt3 =
        _mm_maddubs_epi16(srcRegFilt3, _mm256_castsi256_si128(secondFilters));
    srcRegFilt2 =
        _mm_maddubs_epi16(srcRegFilt2, _mm256_castsi256_si128(thirdFilters));

    // add and saturate the results together
    srcRegFilt2_1 =
        _mm_adds_epi16(srcRegFilt2_1, _mm_adds_epi16(srcRegFilt3, srcRegFilt2));

    // shift by 6 bit each 16 bit
    srcRegFilt1_1 =
        _mm_adds_epi16(srcRegFilt1_1, _mm256_castsi256_si128(addFilterReg32));
    srcRegFilt1_1 = _mm_srai_epi16(srcRegFilt1_1, 6);

    srcRegFilt2_1 =
        _mm_adds_epi16(srcRegFilt2_1, _mm256_castsi256_si128(addFilterReg32));
    srcRegFilt2_1 = _mm_srai_epi16(srcRegFilt2_1, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve
    // result
    srcRegFilt1_1 = _mm_packus_epi16(srcRegFilt1_1, srcRegFilt2_1);

    // save 16 bytes
    _mm_store_si128((__m128i *)output_ptr, srcRegFilt1_1);
  }
}

static void aom_filter_block1d8_v4_avx2(
    const uint8_t *src_ptr, ptrdiff_t src_pitch, uint8_t *output_ptr,
    ptrdiff_t out_pitch, uint32_t output_height, const int16_t *filter) {
  __m128i filtersReg;
  __m256i filtersReg32, addFilterReg32;
  __m256i srcReg23, srcReg4x, srcReg34, srcReg5x, srcReg45, srcReg6x, srcReg56;
  __m256i srcReg23_34_lo, srcReg45_56_lo;
  __m256i resReg23_34_lo, resReg45_56_lo;
  __m256i resReglo, resReg;
  __m256i secondFilters, thirdFilters;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;

  addFilterReg32 = _mm256_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  // converting the 16 bit (short) to  8 bit (byte) and have the
  // same data in both lanes of 128 bit register.
  filtersReg = _mm_srai_epi16(filtersReg, 1);
  filtersReg = _mm_packs_epi16(filtersReg, filtersReg);
  // have the same data in both lanes of a 256 bit register
  filtersReg32 = MM256_BROADCASTSI128_SI256(filtersReg);

  // duplicate only the second 16 bits (third and forth byte)
  // across 256 bit register
  secondFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x302u));
  // duplicate only the third 16 bits (fifth and sixth byte)
  // across 256 bit register
  thirdFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x504u));

  // multiple the size of the source and destination stride by two
  src_stride = src_pitch << 1;
  dst_stride = out_pitch << 1;

  srcReg23 = xx_loadu2_epi64(src_ptr + src_pitch * 3, src_ptr + src_pitch * 2);
  srcReg4x = _mm256_castsi128_si256(
      _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 4)));

  // have consecutive loads on the same 256 register
  srcReg34 = _mm256_permute2x128_si256(srcReg23, srcReg4x, 0x21);

  srcReg23_34_lo = _mm256_unpacklo_epi8(srcReg23, srcReg34);

  for (i = output_height; i > 1; i -= 2) {
    // load the last 2 loads of 16 bytes and have every two
    // consecutive loads in the same 256 bit register
    srcReg5x = _mm256_castsi128_si256(
        _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 5)));
    srcReg45 =
        _mm256_inserti128_si256(srcReg4x, _mm256_castsi256_si128(srcReg5x), 1);

    srcReg6x = _mm256_castsi128_si256(
        _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 6)));
    srcReg56 =
        _mm256_inserti128_si256(srcReg5x, _mm256_castsi256_si128(srcReg6x), 1);

    // merge every two consecutive registers
    srcReg45_56_lo = _mm256_unpacklo_epi8(srcReg45, srcReg56);

    // multiply 2 adjacent elements with the filter and add the result
    resReg23_34_lo = _mm256_maddubs_epi16(srcReg23_34_lo, secondFilters);
    resReg45_56_lo = _mm256_maddubs_epi16(srcReg45_56_lo, thirdFilters);

    // add and saturate the results together
    resReglo = _mm256_adds_epi16(resReg23_34_lo, resReg45_56_lo);

    // shift by 6 bit each 16 bit
    resReglo = _mm256_adds_epi16(resReglo, addFilterReg32);
    resReglo = _mm256_srai_epi16(resReglo, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve
    // result
    resReg = _mm256_packus_epi16(resReglo, resReglo);

    src_ptr += src_stride;

    xx_storeu2_epi64(output_ptr, out_pitch, &resReg);

    output_ptr += dst_stride;

    // save part of the registers for next strides
    srcReg23_34_lo = srcReg45_56_lo;
    srcReg4x = srcReg6x;
  }
}

static void aom_filter_block1d8_v8_avx2(
    const uint8_t *src_ptr, ptrdiff_t src_pitch, uint8_t *output_ptr,
    ptrdiff_t out_pitch, uint32_t output_height, const int16_t *filter) {
  __m128i filtersReg;
  __m256i addFilterReg32;
  __m256i srcReg32b1, srcReg32b2, srcReg32b3, srcReg32b4, srcReg32b5;
  __m256i srcReg32b6, srcReg32b7, srcReg32b8, srcReg32b9, srcReg32b10;
  __m256i srcReg32b11, srcReg32b12, filtersReg32;
  __m256i firstFilters, secondFilters, thirdFilters, forthFilters;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;

  addFilterReg32 = _mm256_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  // converting the 16 bit (short) to  8 bit (byte) and have the
  // same data in both lanes of 128 bit register.
  filtersReg = _mm_srai_epi16(filtersReg, 1);
  filtersReg = _mm_packs_epi16(filtersReg, filtersReg);
  // have the same data in both lanes of a 256 bit register
  filtersReg32 = MM256_BROADCASTSI128_SI256(filtersReg);

  // duplicate only the first 16 bits (first and second byte)
  // across 256 bit register
  firstFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x100u));
  // duplicate only the second 16 bits (third and forth byte)
  // across 256 bit register
  secondFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x302u));
  // duplicate only the third 16 bits (fifth and sixth byte)
  // across 256 bit register
  thirdFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x504u));
  // duplicate only the forth 16 bits (seventh and eighth byte)
  // across 256 bit register
  forthFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x706u));

  // multiple the size of the source and destination stride by two
  src_stride = src_pitch << 1;
  dst_stride = out_pitch << 1;

  // load 16 bytes 7 times in stride of src_pitch
  srcReg32b1 = xx_loadu2_epi64(src_ptr + src_pitch, src_ptr);
  srcReg32b3 =
      xx_loadu2_epi64(src_ptr + src_pitch * 3, src_ptr + src_pitch * 2);
  srcReg32b5 =
      xx_loadu2_epi64(src_ptr + src_pitch * 5, src_ptr + src_pitch * 4);
  srcReg32b7 = _mm256_castsi128_si256(
      _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 6)));

  // have each consecutive loads on the same 256 register
  srcReg32b2 = _mm256_permute2x128_si256(srcReg32b1, srcReg32b3, 0x21);
  srcReg32b4 = _mm256_permute2x128_si256(srcReg32b3, srcReg32b5, 0x21);
  srcReg32b6 = _mm256_permute2x128_si256(srcReg32b5, srcReg32b7, 0x21);
  // merge every two consecutive registers except the last one
  srcReg32b10 = _mm256_unpacklo_epi8(srcReg32b1, srcReg32b2);
  srcReg32b11 = _mm256_unpacklo_epi8(srcReg32b3, srcReg32b4);
  srcReg32b2 = _mm256_unpacklo_epi8(srcReg32b5, srcReg32b6);

  for (i = output_height; i > 1; i -= 2) {
    // load the last 2 loads of 16 bytes and have every two
    // consecutive loads in the same 256 bit register
    srcReg32b8 = _mm256_castsi128_si256(
        _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 7)));
    srcReg32b7 = _mm256_inserti128_si256(srcReg32b7,
                                         _mm256_castsi256_si128(srcReg32b8), 1);
    srcReg32b9 = _mm256_castsi128_si256(
        _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 8)));
    srcReg32b8 = _mm256_inserti128_si256(srcReg32b8,
                                         _mm256_castsi256_si128(srcReg32b9), 1);

    // merge every two consecutive registers
    // save
    srcReg32b4 = _mm256_unpacklo_epi8(srcReg32b7, srcReg32b8);

    // multiply 2 adjacent elements with the filter and add the result
    srcReg32b10 = _mm256_maddubs_epi16(srcReg32b10, firstFilters);
    srcReg32b6 = _mm256_maddubs_epi16(srcReg32b4, forthFilters);

    // add and saturate the results together
    srcReg32b10 = _mm256_adds_epi16(srcReg32b10, srcReg32b6);

    // multiply 2 adjacent elements with the filter and add the result
    srcReg32b8 = _mm256_maddubs_epi16(srcReg32b11, secondFilters);
    srcReg32b12 = _mm256_maddubs_epi16(srcReg32b2, thirdFilters);

    // add and saturate the results together
    srcReg32b10 = _mm256_adds_epi16(srcReg32b10,
                                    _mm256_adds_epi16(srcReg32b8, srcReg32b12));

    // shift by 6 bit each 16 bit
    srcReg32b10 = _mm256_adds_epi16(srcReg32b10, addFilterReg32);
    srcReg32b10 = _mm256_srai_epi16(srcReg32b10, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve
    // result
    srcReg32b1 = _mm256_packus_epi16(srcReg32b10, _mm256_setzero_si256());

    src_ptr += src_stride;

    xx_storeu2_epi64(output_ptr, out_pitch, &srcReg32b1);

    output_ptr += dst_stride;

    // save part of the registers for next strides
    srcReg32b10 = srcReg32b11;
    srcReg32b11 = srcReg32b2;
    srcReg32b2 = srcReg32b4;
    srcReg32b7 = srcReg32b9;
  }
  if (i > 0) {
    __m128i srcRegFilt1, srcRegFilt4, srcRegFilt6, srcRegFilt8;
    // load the last 16 bytes
    srcRegFilt8 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 7));

    // merge the last 2 results together
    srcRegFilt4 =
        _mm_unpacklo_epi8(_mm256_castsi256_si128(srcReg32b7), srcRegFilt8);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt1 = _mm_maddubs_epi16(_mm256_castsi256_si128(srcReg32b10),
                                    _mm256_castsi256_si128(firstFilters));
    srcRegFilt4 =
        _mm_maddubs_epi16(srcRegFilt4, _mm256_castsi256_si128(forthFilters));

    // add and saturate the results together
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, srcRegFilt4);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt4 = _mm_maddubs_epi16(_mm256_castsi256_si128(srcReg32b11),
                                    _mm256_castsi256_si128(secondFilters));

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt6 = _mm_maddubs_epi16(_mm256_castsi256_si128(srcReg32b2),
                                    _mm256_castsi256_si128(thirdFilters));

    // add and saturate the results together
    srcRegFilt1 =
        _mm_adds_epi16(srcRegFilt1, _mm_adds_epi16(srcRegFilt4, srcRegFilt6));

    // shift by 6 bit each 16 bit
    srcRegFilt1 =
        _mm_adds_epi16(srcRegFilt1, _mm256_castsi256_si128(addFilterReg32));
    srcRegFilt1 = _mm_srai_epi16(srcRegFilt1, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve result
    srcRegFilt1 = _mm_packus_epi16(srcRegFilt1, _mm_setzero_si128());

    // save 8 bytes
    _mm_storel_epi64((__m128i *)output_ptr, srcRegFilt1);
  }
}

static void aom_filter_block1d16_v4_avx2(
    const uint8_t *src_ptr, ptrdiff_t src_pitch, uint8_t *output_ptr,
    ptrdiff_t out_pitch, uint32_t output_height, const int16_t *filter) {
  __m128i filtersReg;
  __m256i filtersReg32, addFilterReg32;
  __m256i srcReg23, srcReg4x, srcReg34, srcReg5x, srcReg45, srcReg6x, srcReg56;
  __m256i srcReg23_34_lo, srcReg23_34_hi, srcReg45_56_lo, srcReg45_56_hi;
  __m256i resReg23_34_lo, resReg23_34_hi, resReg45_56_lo, resReg45_56_hi;
  __m256i resReglo, resReghi, resReg;
  __m256i secondFilters, thirdFilters;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;

  addFilterReg32 = _mm256_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  // converting the 16 bit (short) to  8 bit (byte) and have the
  // same data in both lanes of 128 bit register.
  filtersReg = _mm_srai_epi16(filtersReg, 1);
  filtersReg = _mm_packs_epi16(filtersReg, filtersReg);
  // have the same data in both lanes of a 256 bit register
  filtersReg32 = MM256_BROADCASTSI128_SI256(filtersReg);

  // duplicate only the second 16 bits (third and forth byte)
  // across 256 bit register
  secondFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x302u));
  // duplicate only the third 16 bits (fifth and sixth byte)
  // across 256 bit register
  thirdFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x504u));

  // multiple the size of the source and destination stride by two
  src_stride = src_pitch << 1;
  dst_stride = out_pitch << 1;

  srcReg23 = yy_loadu2_128(src_ptr + src_pitch * 3, src_ptr + src_pitch * 2);
  srcReg4x = _mm256_castsi128_si256(
      _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 4)));

  // have consecutive loads on the same 256 register
  srcReg34 = _mm256_permute2x128_si256(srcReg23, srcReg4x, 0x21);

  srcReg23_34_lo = _mm256_unpacklo_epi8(srcReg23, srcReg34);
  srcReg23_34_hi = _mm256_unpackhi_epi8(srcReg23, srcReg34);

  for (i = output_height; i > 1; i -= 2) {
    // load the last 2 loads of 16 bytes and have every two
    // consecutive loads in the same 256 bit register
    srcReg5x = _mm256_castsi128_si256(
        _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 5)));
    srcReg45 =
        _mm256_inserti128_si256(srcReg4x, _mm256_castsi256_si128(srcReg5x), 1);

    srcReg6x = _mm256_castsi128_si256(
        _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 6)));
    srcReg56 =
        _mm256_inserti128_si256(srcReg5x, _mm256_castsi256_si128(srcReg6x), 1);

    // merge every two consecutive registers
    srcReg45_56_lo = _mm256_unpacklo_epi8(srcReg45, srcReg56);
    srcReg45_56_hi = _mm256_unpackhi_epi8(srcReg45, srcReg56);

    // multiply 2 adjacent elements with the filter and add the result
    resReg23_34_lo = _mm256_maddubs_epi16(srcReg23_34_lo, secondFilters);
    resReg45_56_lo = _mm256_maddubs_epi16(srcReg45_56_lo, thirdFilters);

    // add and saturate the results together
    resReglo = _mm256_adds_epi16(resReg23_34_lo, resReg45_56_lo);

    // multiply 2 adjacent elements with the filter and add the result
    resReg23_34_hi = _mm256_maddubs_epi16(srcReg23_34_hi, secondFilters);
    resReg45_56_hi = _mm256_maddubs_epi16(srcReg45_56_hi, thirdFilters);

    // add and saturate the results together
    resReghi = _mm256_adds_epi16(resReg23_34_hi, resReg45_56_hi);

    // shift by 6 bit each 16 bit
    resReglo = _mm256_adds_epi16(resReglo, addFilterReg32);
    resReghi = _mm256_adds_epi16(resReghi, addFilterReg32);
    resReglo = _mm256_srai_epi16(resReglo, 6);
    resReghi = _mm256_srai_epi16(resReghi, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve
    // result
    resReg = _mm256_packus_epi16(resReglo, resReghi);

    src_ptr += src_stride;

    xx_store2_mi128(output_ptr, out_pitch, &resReg);

    output_ptr += dst_stride;

    // save part of the registers for next strides
    srcReg23_34_lo = srcReg45_56_lo;
    srcReg23_34_hi = srcReg45_56_hi;
    srcReg4x = srcReg6x;
  }
}

static void aom_filter_block1d16_v8_avx2(
    const uint8_t *src_ptr, ptrdiff_t src_pitch, uint8_t *output_ptr,
    ptrdiff_t out_pitch, uint32_t output_height, const int16_t *filter) {
  __m128i filtersReg;
  __m256i addFilterReg32;
  __m256i srcReg32b1, srcReg32b2, srcReg32b3, srcReg32b4, srcReg32b5;
  __m256i srcReg32b6, srcReg32b7, srcReg32b8, srcReg32b9, srcReg32b10;
  __m256i srcReg32b11, srcReg32b12, filtersReg32;
  __m256i firstFilters, secondFilters, thirdFilters, forthFilters;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;

  addFilterReg32 = _mm256_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  // converting the 16 bit (short) to  8 bit (byte) and have the
  // same data in both lanes of 128 bit register.
  filtersReg = _mm_srai_epi16(filtersReg, 1);
  filtersReg = _mm_packs_epi16(filtersReg, filtersReg);
  // have the same data in both lanes of a 256 bit register
  filtersReg32 = MM256_BROADCASTSI128_SI256(filtersReg);

  // duplicate only the first 16 bits (first and second byte)
  // across 256 bit register
  firstFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x100u));
  // duplicate only the second 16 bits (third and forth byte)
  // across 256 bit register
  secondFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x302u));
  // duplicate only the third 16 bits (fifth and sixth byte)
  // across 256 bit register
  thirdFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x504u));
  // duplicate only the forth 16 bits (seventh and eighth byte)
  // across 256 bit register
  forthFilters = _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi16(0x706u));

  // multiple the size of the source and destination stride by two
  src_stride = src_pitch << 1;
  dst_stride = out_pitch << 1;

  // load 16 bytes 7 times in stride of src_pitch
  srcReg32b1 = yy_loadu2_128(src_ptr + src_pitch, src_ptr);
  srcReg32b3 = yy_loadu2_128(src_ptr + src_pitch * 3, src_ptr + src_pitch * 2);
  srcReg32b5 = yy_loadu2_128(src_ptr + src_pitch * 5, src_ptr + src_pitch * 4);
  srcReg32b7 = _mm256_castsi128_si256(
      _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 6)));

  // have each consecutive loads on the same 256 register
  srcReg32b2 = _mm256_permute2x128_si256(srcReg32b1, srcReg32b3, 0x21);
  srcReg32b4 = _mm256_permute2x128_si256(srcReg32b3, srcReg32b5, 0x21);
  srcReg32b6 = _mm256_permute2x128_si256(srcReg32b5, srcReg32b7, 0x21);
  // merge every two consecutive registers except the last one
  srcReg32b10 = _mm256_unpacklo_epi8(srcReg32b1, srcReg32b2);
  srcReg32b1 = _mm256_unpackhi_epi8(srcReg32b1, srcReg32b2);

  // save
  srcReg32b11 = _mm256_unpacklo_epi8(srcReg32b3, srcReg32b4);
  srcReg32b3 = _mm256_unpackhi_epi8(srcReg32b3, srcReg32b4);
  srcReg32b2 = _mm256_unpacklo_epi8(srcReg32b5, srcReg32b6);
  srcReg32b5 = _mm256_unpackhi_epi8(srcReg32b5, srcReg32b6);

  for (i = output_height; i > 1; i -= 2) {
    // load the last 2 loads of 16 bytes and have every two
    // consecutive loads in the same 256 bit register
    srcReg32b8 = _mm256_castsi128_si256(
        _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 7)));
    srcReg32b7 = _mm256_inserti128_si256(srcReg32b7,
                                         _mm256_castsi256_si128(srcReg32b8), 1);
    srcReg32b9 = _mm256_castsi128_si256(
        _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 8)));
    srcReg32b8 = _mm256_inserti128_si256(srcReg32b8,
                                         _mm256_castsi256_si128(srcReg32b9), 1);

    // merge every two consecutive registers
    // save
    srcReg32b4 = _mm256_unpacklo_epi8(srcReg32b7, srcReg32b8);
    srcReg32b7 = _mm256_unpackhi_epi8(srcReg32b7, srcReg32b8);

    // multiply 2 adjacent elements with the filter and add the result
    srcReg32b10 = _mm256_maddubs_epi16(srcReg32b10, firstFilters);
    srcReg32b6 = _mm256_maddubs_epi16(srcReg32b4, forthFilters);

    // add and saturate the results together
    srcReg32b10 = _mm256_adds_epi16(srcReg32b10, srcReg32b6);

    // multiply 2 adjacent elements with the filter and add the result
    srcReg32b8 = _mm256_maddubs_epi16(srcReg32b11, secondFilters);
    srcReg32b12 = _mm256_maddubs_epi16(srcReg32b2, thirdFilters);

    // add and saturate the results together
    srcReg32b10 = _mm256_adds_epi16(srcReg32b10,
                                    _mm256_adds_epi16(srcReg32b8, srcReg32b12));

    // multiply 2 adjacent elements with the filter and add the result
    srcReg32b1 = _mm256_maddubs_epi16(srcReg32b1, firstFilters);
    srcReg32b6 = _mm256_maddubs_epi16(srcReg32b7, forthFilters);

    srcReg32b1 = _mm256_adds_epi16(srcReg32b1, srcReg32b6);

    // multiply 2 adjacent elements with the filter and add the result
    srcReg32b8 = _mm256_maddubs_epi16(srcReg32b3, secondFilters);
    srcReg32b12 = _mm256_maddubs_epi16(srcReg32b5, thirdFilters);

    // add and saturate the results together
    srcReg32b1 = _mm256_adds_epi16(srcReg32b1,
                                   _mm256_adds_epi16(srcReg32b8, srcReg32b12));

    // shift by 6 bit each 16 bit
    srcReg32b10 = _mm256_adds_epi16(srcReg32b10, addFilterReg32);
    srcReg32b1 = _mm256_adds_epi16(srcReg32b1, addFilterReg32);
    srcReg32b10 = _mm256_srai_epi16(srcReg32b10, 6);
    srcReg32b1 = _mm256_srai_epi16(srcReg32b1, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve
    // result
    srcReg32b1 = _mm256_packus_epi16(srcReg32b10, srcReg32b1);

    src_ptr += src_stride;

    xx_store2_mi128(output_ptr, out_pitch, &srcReg32b1);

    output_ptr += dst_stride;

    // save part of the registers for next strides
    srcReg32b10 = srcReg32b11;
    srcReg32b1 = srcReg32b3;
    srcReg32b11 = srcReg32b2;
    srcReg32b3 = srcReg32b5;
    srcReg32b2 = srcReg32b4;
    srcReg32b5 = srcReg32b7;
    srcReg32b7 = srcReg32b9;
  }
  if (i > 0) {
    __m128i srcRegFilt1, srcRegFilt3, srcRegFilt4, srcRegFilt5;
    __m128i srcRegFilt6, srcRegFilt7, srcRegFilt8;
    // load the last 16 bytes
    srcRegFilt8 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 7));

    // merge the last 2 results together
    srcRegFilt4 =
        _mm_unpacklo_epi8(_mm256_castsi256_si128(srcReg32b7), srcRegFilt8);
    srcRegFilt7 =
        _mm_unpackhi_epi8(_mm256_castsi256_si128(srcReg32b7), srcRegFilt8);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt1 = _mm_maddubs_epi16(_mm256_castsi256_si128(srcReg32b10),
                                    _mm256_castsi256_si128(firstFilters));
    srcRegFilt4 =
        _mm_maddubs_epi16(srcRegFilt4, _mm256_castsi256_si128(forthFilters));
    srcRegFilt3 = _mm_maddubs_epi16(_mm256_castsi256_si128(srcReg32b1),
                                    _mm256_castsi256_si128(firstFilters));
    srcRegFilt7 =
        _mm_maddubs_epi16(srcRegFilt7, _mm256_castsi256_si128(forthFilters));

    // add and saturate the results together
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, srcRegFilt4);
    srcRegFilt3 = _mm_adds_epi16(srcRegFilt3, srcRegFilt7);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt4 = _mm_maddubs_epi16(_mm256_castsi256_si128(srcReg32b11),
                                    _mm256_castsi256_si128(secondFilters));
    srcRegFilt5 = _mm_maddubs_epi16(_mm256_castsi256_si128(srcReg32b3),
                                    _mm256_castsi256_si128(secondFilters));

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt6 = _mm_maddubs_epi16(_mm256_castsi256_si128(srcReg32b2),
                                    _mm256_castsi256_si128(thirdFilters));
    srcRegFilt7 = _mm_maddubs_epi16(_mm256_castsi256_si128(srcReg32b5),
                                    _mm256_castsi256_si128(thirdFilters));

    // add and saturate the results together
    srcRegFilt1 =
        _mm_adds_epi16(srcRegFilt1, _mm_adds_epi16(srcRegFilt4, srcRegFilt6));
    srcRegFilt3 =
        _mm_adds_epi16(srcRegFilt3, _mm_adds_epi16(srcRegFilt5, srcRegFilt7));

    // shift by 6 bit each 16 bit
    srcRegFilt1 =
        _mm_adds_epi16(srcRegFilt1, _mm256_castsi256_si128(addFilterReg32));
    srcRegFilt3 =
        _mm_adds_epi16(srcRegFilt3, _mm256_castsi256_si128(addFilterReg32));
    srcRegFilt1 = _mm_srai_epi16(srcRegFilt1, 6);
    srcRegFilt3 = _mm_srai_epi16(srcRegFilt3, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve
    // result
    srcRegFilt1 = _mm_packus_epi16(srcRegFilt1, srcRegFilt3);

    // save 16 bytes
    _mm_store_si128((__m128i *)output_ptr, srcRegFilt1);
  }
}

static void aom_filter_block1d4_v4_avx2(
    const uint8_t *src_ptr, ptrdiff_t src_pitch, uint8_t *output_ptr,
    ptrdiff_t out_pitch, uint32_t output_height, const int16_t *filter) {
  __m128i filtersReg;
  __m256i filtersReg32, addFilterReg32;
  __m256i srcReg23, srcReg4x, srcReg34, srcReg5x, srcReg45, srcReg6x, srcReg56;
  __m256i srcReg23_34_lo, srcReg45_56_lo;
  __m256i srcReg2345_3456_lo;
  __m256i resReglo, resReg;
  __m256i firstFilters;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;

  addFilterReg32 = _mm256_set1_epi16(32);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  // converting the 16 bit (short) to  8 bit (byte) and have the
  // same data in both lanes of 128 bit register.
  filtersReg = _mm_srai_epi16(filtersReg, 1);
  filtersReg = _mm_packs_epi16(filtersReg, filtersReg);
  // have the same data in both lanes of a 256 bit register
  filtersReg32 = MM256_BROADCASTSI128_SI256(filtersReg);

  firstFilters =
      _mm256_shuffle_epi8(filtersReg32, _mm256_set1_epi32(0x5040302u));

  // multiple the size of the source and destination stride by two
  src_stride = src_pitch << 1;
  dst_stride = out_pitch << 1;

  srcReg23 = xx_loadu2_epi64(src_ptr + src_pitch * 3, src_ptr + src_pitch * 2);
  srcReg4x = _mm256_castsi128_si256(
      _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 4)));

  // have consecutive loads on the same 256 register
  srcReg34 = _mm256_permute2x128_si256(srcReg23, srcReg4x, 0x21);

  srcReg23_34_lo = _mm256_unpacklo_epi8(srcReg23, srcReg34);

  for (i = output_height; i > 1; i -= 2) {
    // load the last 2 loads of 16 bytes and have every two
    // consecutive loads in the same 256 bit register
    srcReg5x = _mm256_castsi128_si256(
        _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 5)));
    srcReg45 =
        _mm256_inserti128_si256(srcReg4x, _mm256_castsi256_si128(srcReg5x), 1);

    srcReg6x = _mm256_castsi128_si256(
        _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 6)));
    srcReg56 =
        _mm256_inserti128_si256(srcReg5x, _mm256_castsi256_si128(srcReg6x), 1);

    // merge every two consecutive registers
    srcReg45_56_lo = _mm256_unpacklo_epi8(srcReg45, srcReg56);

    srcReg2345_3456_lo = _mm256_unpacklo_epi16(srcReg23_34_lo, srcReg45_56_lo);

    // multiply 2 adjacent elements with the filter and add the result
    resReglo = _mm256_maddubs_epi16(srcReg2345_3456_lo, firstFilters);

    resReglo = _mm256_hadds_epi16(resReglo, _mm256_setzero_si256());

    // shift by 6 bit each 16 bit
    resReglo = _mm256_adds_epi16(resReglo, addFilterReg32);
    resReglo = _mm256_srai_epi16(resReglo, 6);

    // shrink to 8 bit each 16 bits, the first lane contain the first
    // convolve result and the second lane contain the second convolve
    // result
    resReg = _mm256_packus_epi16(resReglo, resReglo);

    src_ptr += src_stride;

    xx_storeu2_epi32(output_ptr, out_pitch, &resReg);

    output_ptr += dst_stride;

    // save part of the registers for next strides
    srcReg23_34_lo = srcReg45_56_lo;
    srcReg4x = srcReg6x;
  }
}

#if HAVE_AVX2 && HAVE_SSSE3
filter8_1dfunction aom_filter_block1d4_v8_ssse3;
filter8_1dfunction aom_filter_block1d16_v2_ssse3;
filter8_1dfunction aom_filter_block1d16_h2_ssse3;
filter8_1dfunction aom_filter_block1d8_v2_ssse3;
filter8_1dfunction aom_filter_block1d8_h2_ssse3;
filter8_1dfunction aom_filter_block1d4_v2_ssse3;
filter8_1dfunction aom_filter_block1d4_h2_ssse3;
#define aom_filter_block1d4_v8_avx2 aom_filter_block1d4_v8_ssse3
#define aom_filter_block1d16_v2_avx2 aom_filter_block1d16_v2_ssse3
#define aom_filter_block1d16_h2_avx2 aom_filter_block1d16_h2_ssse3
#define aom_filter_block1d8_v2_avx2 aom_filter_block1d8_v2_ssse3
#define aom_filter_block1d8_h2_avx2 aom_filter_block1d8_h2_ssse3
#define aom_filter_block1d4_v2_avx2 aom_filter_block1d4_v2_ssse3
#define aom_filter_block1d4_h2_avx2 aom_filter_block1d4_h2_ssse3
// void aom_convolve8_horiz_avx2(const uint8_t *src, ptrdiff_t src_stride,
//                                uint8_t *dst, ptrdiff_t dst_stride,
//                                const int16_t *filter_x, int x_step_q4,
//                                const int16_t *filter_y, int y_step_q4,
//                                int w, int h);
// void aom_convolve8_vert_avx2(const uint8_t *src, ptrdiff_t src_stride,
//                               uint8_t *dst, ptrdiff_t dst_stride,
//                               const int16_t *filter_x, int x_step_q4,
//                               const int16_t *filter_y, int y_step_q4,
//                               int w, int h);
FUN_CONV_1D(horiz, x_step_q4, filter_x, h, src, , avx2)
FUN_CONV_1D(vert, y_step_q4, filter_y, v, src - src_stride * 3, , avx2)

#endif  // HAVE_AX2 && HAVE_SSSE3
