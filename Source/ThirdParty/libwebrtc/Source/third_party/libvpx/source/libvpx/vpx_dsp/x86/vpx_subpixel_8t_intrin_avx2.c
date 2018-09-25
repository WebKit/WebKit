/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <immintrin.h>

#include "./vpx_dsp_rtcd.h"
#include "vpx_dsp/x86/convolve.h"
#include "vpx_dsp/x86/convolve_avx2.h"
#include "vpx_ports/mem.h"

// filters for 16_h8
DECLARE_ALIGNED(32, static const uint8_t, filt1_global_avx2[32]) = {
  0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8,
  0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8
};

DECLARE_ALIGNED(32, static const uint8_t, filt2_global_avx2[32]) = {
  2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10,
  2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10
};

DECLARE_ALIGNED(32, static const uint8_t, filt3_global_avx2[32]) = {
  4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12,
  4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12
};

DECLARE_ALIGNED(32, static const uint8_t, filt4_global_avx2[32]) = {
  6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14,
  6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14
};

static INLINE void vpx_filter_block1d16_h8_x_avx2(
    const uint8_t *src_ptr, ptrdiff_t src_pixels_per_line, uint8_t *output_ptr,
    ptrdiff_t output_pitch, uint32_t output_height, const int16_t *filter,
    const int avg) {
  __m128i outReg1, outReg2;
  __m256i outReg32b1, outReg32b2;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;
  __m256i f[4], filt[4], s[4];

  shuffle_filter_avx2(filter, f);
  filt[0] = _mm256_load_si256((__m256i const *)filt1_global_avx2);
  filt[1] = _mm256_load_si256((__m256i const *)filt2_global_avx2);
  filt[2] = _mm256_load_si256((__m256i const *)filt3_global_avx2);
  filt[3] = _mm256_load_si256((__m256i const *)filt4_global_avx2);

  // multiple the size of the source and destination stride by two
  src_stride = src_pixels_per_line << 1;
  dst_stride = output_pitch << 1;
  for (i = output_height; i > 1; i -= 2) {
    __m256i srcReg;

    // load the 2 strides of source
    srcReg =
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i *)(src_ptr - 3)));
    srcReg = _mm256_inserti128_si256(
        srcReg,
        _mm_loadu_si128((const __m128i *)(src_ptr + src_pixels_per_line - 3)),
        1);

    // filter the source buffer
    s[0] = _mm256_shuffle_epi8(srcReg, filt[0]);
    s[1] = _mm256_shuffle_epi8(srcReg, filt[1]);
    s[2] = _mm256_shuffle_epi8(srcReg, filt[2]);
    s[3] = _mm256_shuffle_epi8(srcReg, filt[3]);
    outReg32b1 = convolve8_16_avx2(s, f);

    // reading 2 strides of the next 16 bytes
    // (part of it was being read by earlier read)
    srcReg =
        _mm256_castsi128_si256(_mm_loadu_si128((const __m128i *)(src_ptr + 5)));
    srcReg = _mm256_inserti128_si256(
        srcReg,
        _mm_loadu_si128((const __m128i *)(src_ptr + src_pixels_per_line + 5)),
        1);

    // filter the source buffer
    s[0] = _mm256_shuffle_epi8(srcReg, filt[0]);
    s[1] = _mm256_shuffle_epi8(srcReg, filt[1]);
    s[2] = _mm256_shuffle_epi8(srcReg, filt[2]);
    s[3] = _mm256_shuffle_epi8(srcReg, filt[3]);
    outReg32b2 = convolve8_16_avx2(s, f);

    // shrink to 8 bit each 16 bits, the low and high 64-bits of each lane
    // contain the first and second convolve result respectively
    outReg32b1 = _mm256_packus_epi16(outReg32b1, outReg32b2);

    src_ptr += src_stride;

    // average if necessary
    outReg1 = _mm256_castsi256_si128(outReg32b1);
    outReg2 = _mm256_extractf128_si256(outReg32b1, 1);
    if (avg) {
      outReg1 = _mm_avg_epu8(outReg1, _mm_load_si128((__m128i *)output_ptr));
      outReg2 = _mm_avg_epu8(
          outReg2, _mm_load_si128((__m128i *)(output_ptr + output_pitch)));
    }

    // save 16 bytes
    _mm_store_si128((__m128i *)output_ptr, outReg1);

    // save the next 16 bits
    _mm_store_si128((__m128i *)(output_ptr + output_pitch), outReg2);

    output_ptr += dst_stride;
  }

  // if the number of strides is odd.
  // process only 16 bytes
  if (i > 0) {
    __m128i srcReg;

    // load the first 16 bytes of the last row
    srcReg = _mm_loadu_si128((const __m128i *)(src_ptr - 3));

    // filter the source buffer
    s[0] = _mm256_castsi128_si256(
        _mm_shuffle_epi8(srcReg, _mm256_castsi256_si128(filt[0])));
    s[1] = _mm256_castsi128_si256(
        _mm_shuffle_epi8(srcReg, _mm256_castsi256_si128(filt[1])));
    s[2] = _mm256_castsi128_si256(
        _mm_shuffle_epi8(srcReg, _mm256_castsi256_si128(filt[2])));
    s[3] = _mm256_castsi128_si256(
        _mm_shuffle_epi8(srcReg, _mm256_castsi256_si128(filt[3])));
    outReg1 = convolve8_8_avx2(s, f);

    // reading the next 16 bytes
    // (part of it was being read by earlier read)
    srcReg = _mm_loadu_si128((const __m128i *)(src_ptr + 5));

    // filter the source buffer
    s[0] = _mm256_castsi128_si256(
        _mm_shuffle_epi8(srcReg, _mm256_castsi256_si128(filt[0])));
    s[1] = _mm256_castsi128_si256(
        _mm_shuffle_epi8(srcReg, _mm256_castsi256_si128(filt[1])));
    s[2] = _mm256_castsi128_si256(
        _mm_shuffle_epi8(srcReg, _mm256_castsi256_si128(filt[2])));
    s[3] = _mm256_castsi128_si256(
        _mm_shuffle_epi8(srcReg, _mm256_castsi256_si128(filt[3])));
    outReg2 = convolve8_8_avx2(s, f);

    // shrink to 8 bit each 16 bits, the low and high 64-bits of each lane
    // contain the first and second convolve result respectively
    outReg1 = _mm_packus_epi16(outReg1, outReg2);

    // average if necessary
    if (avg) {
      outReg1 = _mm_avg_epu8(outReg1, _mm_load_si128((__m128i *)output_ptr));
    }

    // save 16 bytes
    _mm_store_si128((__m128i *)output_ptr, outReg1);
  }
}

static void vpx_filter_block1d16_h8_avx2(
    const uint8_t *src_ptr, ptrdiff_t src_stride, uint8_t *output_ptr,
    ptrdiff_t dst_stride, uint32_t output_height, const int16_t *filter) {
  vpx_filter_block1d16_h8_x_avx2(src_ptr, src_stride, output_ptr, dst_stride,
                                 output_height, filter, 0);
}

static void vpx_filter_block1d16_h8_avg_avx2(
    const uint8_t *src_ptr, ptrdiff_t src_stride, uint8_t *output_ptr,
    ptrdiff_t dst_stride, uint32_t output_height, const int16_t *filter) {
  vpx_filter_block1d16_h8_x_avx2(src_ptr, src_stride, output_ptr, dst_stride,
                                 output_height, filter, 1);
}

static INLINE void vpx_filter_block1d16_v8_x_avx2(
    const uint8_t *src_ptr, ptrdiff_t src_pitch, uint8_t *output_ptr,
    ptrdiff_t out_pitch, uint32_t output_height, const int16_t *filter,
    const int avg) {
  __m128i outReg1, outReg2;
  __m256i srcRegHead1;
  unsigned int i;
  ptrdiff_t src_stride, dst_stride;
  __m256i f[4], s1[4], s2[4];

  shuffle_filter_avx2(filter, f);

  // multiple the size of the source and destination stride by two
  src_stride = src_pitch << 1;
  dst_stride = out_pitch << 1;

  {
    __m128i s[6];
    __m256i s32b[6];

    // load 16 bytes 7 times in stride of src_pitch
    s[0] = _mm_loadu_si128((const __m128i *)(src_ptr + 0 * src_pitch));
    s[1] = _mm_loadu_si128((const __m128i *)(src_ptr + 1 * src_pitch));
    s[2] = _mm_loadu_si128((const __m128i *)(src_ptr + 2 * src_pitch));
    s[3] = _mm_loadu_si128((const __m128i *)(src_ptr + 3 * src_pitch));
    s[4] = _mm_loadu_si128((const __m128i *)(src_ptr + 4 * src_pitch));
    s[5] = _mm_loadu_si128((const __m128i *)(src_ptr + 5 * src_pitch));
    srcRegHead1 = _mm256_castsi128_si256(
        _mm_loadu_si128((const __m128i *)(src_ptr + 6 * src_pitch)));

    // have each consecutive loads on the same 256 register
    s32b[0] = _mm256_inserti128_si256(_mm256_castsi128_si256(s[0]), s[1], 1);
    s32b[1] = _mm256_inserti128_si256(_mm256_castsi128_si256(s[1]), s[2], 1);
    s32b[2] = _mm256_inserti128_si256(_mm256_castsi128_si256(s[2]), s[3], 1);
    s32b[3] = _mm256_inserti128_si256(_mm256_castsi128_si256(s[3]), s[4], 1);
    s32b[4] = _mm256_inserti128_si256(_mm256_castsi128_si256(s[4]), s[5], 1);
    s32b[5] = _mm256_inserti128_si256(_mm256_castsi128_si256(s[5]),
                                      _mm256_castsi256_si128(srcRegHead1), 1);

    // merge every two consecutive registers except the last one
    // the first lanes contain values for filtering odd rows (1,3,5...) and
    // the second lanes contain values for filtering even rows (2,4,6...)
    s1[0] = _mm256_unpacklo_epi8(s32b[0], s32b[1]);
    s2[0] = _mm256_unpackhi_epi8(s32b[0], s32b[1]);
    s1[1] = _mm256_unpacklo_epi8(s32b[2], s32b[3]);
    s2[1] = _mm256_unpackhi_epi8(s32b[2], s32b[3]);
    s1[2] = _mm256_unpacklo_epi8(s32b[4], s32b[5]);
    s2[2] = _mm256_unpackhi_epi8(s32b[4], s32b[5]);
  }

  for (i = output_height; i > 1; i -= 2) {
    __m256i srcRegHead2, srcRegHead3;

    // load the next 2 loads of 16 bytes and have every two
    // consecutive loads in the same 256 bit register
    srcRegHead2 = _mm256_castsi128_si256(
        _mm_loadu_si128((const __m128i *)(src_ptr + 7 * src_pitch)));
    srcRegHead1 = _mm256_inserti128_si256(
        srcRegHead1, _mm256_castsi256_si128(srcRegHead2), 1);
    srcRegHead3 = _mm256_castsi128_si256(
        _mm_loadu_si128((const __m128i *)(src_ptr + 8 * src_pitch)));
    srcRegHead2 = _mm256_inserti128_si256(
        srcRegHead2, _mm256_castsi256_si128(srcRegHead3), 1);

    // merge the two new consecutive registers
    // the first lane contain values for filtering odd rows (1,3,5...) and
    // the second lane contain values for filtering even rows (2,4,6...)
    s1[3] = _mm256_unpacklo_epi8(srcRegHead1, srcRegHead2);
    s2[3] = _mm256_unpackhi_epi8(srcRegHead1, srcRegHead2);

    s1[0] = convolve8_16_avx2(s1, f);
    s2[0] = convolve8_16_avx2(s2, f);

    // shrink to 8 bit each 16 bits, the low and high 64-bits of each lane
    // contain the first and second convolve result respectively
    s1[0] = _mm256_packus_epi16(s1[0], s2[0]);

    src_ptr += src_stride;

    // average if necessary
    outReg1 = _mm256_castsi256_si128(s1[0]);
    outReg2 = _mm256_extractf128_si256(s1[0], 1);
    if (avg) {
      outReg1 = _mm_avg_epu8(outReg1, _mm_load_si128((__m128i *)output_ptr));
      outReg2 = _mm_avg_epu8(
          outReg2, _mm_load_si128((__m128i *)(output_ptr + out_pitch)));
    }

    // save 16 bytes
    _mm_store_si128((__m128i *)output_ptr, outReg1);

    // save the next 16 bits
    _mm_store_si128((__m128i *)(output_ptr + out_pitch), outReg2);

    output_ptr += dst_stride;

    // shift down by two rows
    s1[0] = s1[1];
    s2[0] = s2[1];
    s1[1] = s1[2];
    s2[1] = s2[2];
    s1[2] = s1[3];
    s2[2] = s2[3];
    srcRegHead1 = srcRegHead3;
  }

  // if the number of strides is odd.
  // process only 16 bytes
  if (i > 0) {
    // load the last 16 bytes
    const __m128i srcRegHead2 =
        _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 7));

    // merge the last 2 results together
    s1[0] = _mm256_castsi128_si256(
        _mm_unpacklo_epi8(_mm256_castsi256_si128(srcRegHead1), srcRegHead2));
    s2[0] = _mm256_castsi128_si256(
        _mm_unpackhi_epi8(_mm256_castsi256_si128(srcRegHead1), srcRegHead2));

    outReg1 = convolve8_8_avx2(s1, f);
    outReg2 = convolve8_8_avx2(s2, f);

    // shrink to 8 bit each 16 bits, the low and high 64-bits of each lane
    // contain the first and second convolve result respectively
    outReg1 = _mm_packus_epi16(outReg1, outReg2);

    // average if necessary
    if (avg) {
      outReg1 = _mm_avg_epu8(outReg1, _mm_load_si128((__m128i *)output_ptr));
    }

    // save 16 bytes
    _mm_store_si128((__m128i *)output_ptr, outReg1);
  }
}

static void vpx_filter_block1d16_v8_avx2(const uint8_t *src_ptr,
                                         ptrdiff_t src_stride, uint8_t *dst_ptr,
                                         ptrdiff_t dst_stride, uint32_t height,
                                         const int16_t *filter) {
  vpx_filter_block1d16_v8_x_avx2(src_ptr, src_stride, dst_ptr, dst_stride,
                                 height, filter, 0);
}

static void vpx_filter_block1d16_v8_avg_avx2(
    const uint8_t *src_ptr, ptrdiff_t src_stride, uint8_t *dst_ptr,
    ptrdiff_t dst_stride, uint32_t height, const int16_t *filter) {
  vpx_filter_block1d16_v8_x_avx2(src_ptr, src_stride, dst_ptr, dst_stride,
                                 height, filter, 1);
}

#if HAVE_AVX2 && HAVE_SSSE3
filter8_1dfunction vpx_filter_block1d4_v8_ssse3;
#if ARCH_X86_64
filter8_1dfunction vpx_filter_block1d8_v8_intrin_ssse3;
filter8_1dfunction vpx_filter_block1d8_h8_intrin_ssse3;
filter8_1dfunction vpx_filter_block1d4_h8_intrin_ssse3;
#define vpx_filter_block1d8_v8_avx2 vpx_filter_block1d8_v8_intrin_ssse3
#define vpx_filter_block1d8_h8_avx2 vpx_filter_block1d8_h8_intrin_ssse3
#define vpx_filter_block1d4_h8_avx2 vpx_filter_block1d4_h8_intrin_ssse3
#else  // ARCH_X86
filter8_1dfunction vpx_filter_block1d8_v8_ssse3;
filter8_1dfunction vpx_filter_block1d8_h8_ssse3;
filter8_1dfunction vpx_filter_block1d4_h8_ssse3;
#define vpx_filter_block1d8_v8_avx2 vpx_filter_block1d8_v8_ssse3
#define vpx_filter_block1d8_h8_avx2 vpx_filter_block1d8_h8_ssse3
#define vpx_filter_block1d4_h8_avx2 vpx_filter_block1d4_h8_ssse3
#endif  // ARCH_X86_64
filter8_1dfunction vpx_filter_block1d8_v8_avg_ssse3;
filter8_1dfunction vpx_filter_block1d8_h8_avg_ssse3;
filter8_1dfunction vpx_filter_block1d4_v8_avg_ssse3;
filter8_1dfunction vpx_filter_block1d4_h8_avg_ssse3;
#define vpx_filter_block1d8_v8_avg_avx2 vpx_filter_block1d8_v8_avg_ssse3
#define vpx_filter_block1d8_h8_avg_avx2 vpx_filter_block1d8_h8_avg_ssse3
#define vpx_filter_block1d4_v8_avg_avx2 vpx_filter_block1d4_v8_avg_ssse3
#define vpx_filter_block1d4_h8_avg_avx2 vpx_filter_block1d4_h8_avg_ssse3
filter8_1dfunction vpx_filter_block1d16_v2_ssse3;
filter8_1dfunction vpx_filter_block1d16_h2_ssse3;
filter8_1dfunction vpx_filter_block1d8_v2_ssse3;
filter8_1dfunction vpx_filter_block1d8_h2_ssse3;
filter8_1dfunction vpx_filter_block1d4_v2_ssse3;
filter8_1dfunction vpx_filter_block1d4_h2_ssse3;
#define vpx_filter_block1d4_v8_avx2 vpx_filter_block1d4_v8_ssse3
#define vpx_filter_block1d16_v2_avx2 vpx_filter_block1d16_v2_ssse3
#define vpx_filter_block1d16_h2_avx2 vpx_filter_block1d16_h2_ssse3
#define vpx_filter_block1d8_v2_avx2 vpx_filter_block1d8_v2_ssse3
#define vpx_filter_block1d8_h2_avx2 vpx_filter_block1d8_h2_ssse3
#define vpx_filter_block1d4_v2_avx2 vpx_filter_block1d4_v2_ssse3
#define vpx_filter_block1d4_h2_avx2 vpx_filter_block1d4_h2_ssse3
filter8_1dfunction vpx_filter_block1d16_v2_avg_ssse3;
filter8_1dfunction vpx_filter_block1d16_h2_avg_ssse3;
filter8_1dfunction vpx_filter_block1d8_v2_avg_ssse3;
filter8_1dfunction vpx_filter_block1d8_h2_avg_ssse3;
filter8_1dfunction vpx_filter_block1d4_v2_avg_ssse3;
filter8_1dfunction vpx_filter_block1d4_h2_avg_ssse3;
#define vpx_filter_block1d16_v2_avg_avx2 vpx_filter_block1d16_v2_avg_ssse3
#define vpx_filter_block1d16_h2_avg_avx2 vpx_filter_block1d16_h2_avg_ssse3
#define vpx_filter_block1d8_v2_avg_avx2 vpx_filter_block1d8_v2_avg_ssse3
#define vpx_filter_block1d8_h2_avg_avx2 vpx_filter_block1d8_h2_avg_ssse3
#define vpx_filter_block1d4_v2_avg_avx2 vpx_filter_block1d4_v2_avg_ssse3
#define vpx_filter_block1d4_h2_avg_avx2 vpx_filter_block1d4_h2_avg_ssse3
// void vpx_convolve8_horiz_avx2(const uint8_t *src, ptrdiff_t src_stride,
//                                uint8_t *dst, ptrdiff_t dst_stride,
//                                const InterpKernel *filter, int x0_q4,
//                                int32_t x_step_q4, int y0_q4, int y_step_q4,
//                                int w, int h);
// void vpx_convolve8_vert_avx2(const uint8_t *src, ptrdiff_t src_stride,
//                               uint8_t *dst, ptrdiff_t dst_stride,
//                               const InterpKernel *filter, int x0_q4,
//                               int32_t x_step_q4, int y0_q4, int y_step_q4,
//                               int w, int h);
// void vpx_convolve8_avg_horiz_avx2(const uint8_t *src, ptrdiff_t src_stride,
//                                    uint8_t *dst, ptrdiff_t dst_stride,
//                                    const InterpKernel *filter, int x0_q4,
//                                    int32_t x_step_q4, int y0_q4,
//                                    int y_step_q4, int w, int h);
// void vpx_convolve8_avg_vert_avx2(const uint8_t *src, ptrdiff_t src_stride,
//                                   uint8_t *dst, ptrdiff_t dst_stride,
//                                   const InterpKernel *filter, int x0_q4,
//                                   int32_t x_step_q4, int y0_q4,
//                                   int y_step_q4, int w, int h);
FUN_CONV_1D(horiz, x0_q4, x_step_q4, h, src, , avx2);
FUN_CONV_1D(vert, y0_q4, y_step_q4, v, src - src_stride * 3, , avx2);
FUN_CONV_1D(avg_horiz, x0_q4, x_step_q4, h, src, avg_, avx2);
FUN_CONV_1D(avg_vert, y0_q4, y_step_q4, v, src - src_stride * 3, avg_, avx2);

// void vpx_convolve8_avx2(const uint8_t *src, ptrdiff_t src_stride,
//                          uint8_t *dst, ptrdiff_t dst_stride,
//                          const InterpKernel *filter, int x0_q4,
//                          int32_t x_step_q4, int y0_q4, int y_step_q4,
//                          int w, int h);
// void vpx_convolve8_avg_avx2(const uint8_t *src, ptrdiff_t src_stride,
//                              uint8_t *dst, ptrdiff_t dst_stride,
//                              const InterpKernel *filter, int x0_q4,
//                              int32_t x_step_q4, int y0_q4, int y_step_q4,
//                              int w, int h);
FUN_CONV_2D(, avx2);
FUN_CONV_2D(avg_, avx2);
#endif  // HAVE_AX2 && HAVE_SSSE3
