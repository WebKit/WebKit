/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <immintrin.h>  // AVX2

#include "./vpx_dsp_rtcd.h"

/* clang-format off */
DECLARE_ALIGNED(32, static const uint8_t, bilinear_filters_avx2[512]) = {
  16, 0,  16, 0,  16, 0,  16, 0,  16, 0,  16, 0,  16, 0,  16, 0,
  16, 0,  16, 0,  16, 0,  16, 0,  16, 0,  16, 0,  16, 0,  16, 0,
  14, 2,  14, 2,  14, 2,  14, 2,  14, 2,  14, 2,  14, 2,  14, 2,
  14, 2,  14, 2,  14, 2,  14, 2,  14, 2,  14, 2,  14, 2,  14, 2,
  12, 4,  12, 4,  12, 4,  12, 4,  12, 4,  12, 4,  12, 4,  12, 4,
  12, 4,  12, 4,  12, 4,  12, 4,  12, 4,  12, 4,  12, 4,  12, 4,
  10, 6,  10, 6,  10, 6,  10, 6,  10, 6,  10, 6,  10, 6,  10, 6,
  10, 6,  10, 6,  10, 6,  10, 6,  10, 6,  10, 6,  10, 6,  10, 6,
  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
  6,  10, 6,  10, 6,  10, 6,  10, 6,  10, 6,  10, 6,  10, 6,  10,
  6,  10, 6,  10, 6,  10, 6,  10, 6,  10, 6,  10, 6,  10, 6,  10,
  4,  12, 4,  12, 4,  12, 4,  12, 4,  12, 4,  12, 4,  12, 4,  12,
  4,  12, 4,  12, 4,  12, 4,  12, 4,  12, 4,  12, 4,  12, 4,  12,
  2,  14, 2,  14, 2,  14, 2,  14, 2,  14, 2,  14, 2,  14, 2,  14,
  2,  14, 2,  14, 2,  14, 2,  14, 2,  14, 2,  14, 2,  14, 2,  14,
};

DECLARE_ALIGNED(32, static const int8_t, adjacent_sub_avx2[32]) = {
  1, -1,  1, -1,  1, -1,  1, -1,  1, -1,  1, -1,  1, -1,  1, -1,
  1, -1,  1, -1,  1, -1,  1, -1,  1, -1,  1, -1,  1, -1,  1, -1
};
/* clang-format on */

void vpx_get16x16var_avx2(const unsigned char *src_ptr, int source_stride,
                          const unsigned char *ref_ptr, int recon_stride,
                          unsigned int *sse, int *sum) {
  unsigned int i, src_2strides, ref_2strides;
  __m256i sum_reg = _mm256_setzero_si256();
  __m256i sse_reg = _mm256_setzero_si256();
  // process two 16 byte locations in a 256 bit register
  src_2strides = source_stride << 1;
  ref_2strides = recon_stride << 1;
  for (i = 0; i < 8; ++i) {
    // convert up values in 128 bit registers across lanes
    const __m256i src0 =
        _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i const *)(src_ptr)));
    const __m256i src1 = _mm256_cvtepu8_epi16(
        _mm_loadu_si128((__m128i const *)(src_ptr + source_stride)));
    const __m256i ref0 =
        _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i const *)(ref_ptr)));
    const __m256i ref1 = _mm256_cvtepu8_epi16(
        _mm_loadu_si128((__m128i const *)(ref_ptr + recon_stride)));
    const __m256i diff0 = _mm256_sub_epi16(src0, ref0);
    const __m256i diff1 = _mm256_sub_epi16(src1, ref1);
    const __m256i madd0 = _mm256_madd_epi16(diff0, diff0);
    const __m256i madd1 = _mm256_madd_epi16(diff1, diff1);

    // add to the running totals
    sum_reg = _mm256_add_epi16(sum_reg, _mm256_add_epi16(diff0, diff1));
    sse_reg = _mm256_add_epi32(sse_reg, _mm256_add_epi32(madd0, madd1));

    src_ptr += src_2strides;
    ref_ptr += ref_2strides;
  }
  {
    // extract the low lane and add it to the high lane
    const __m128i sum_reg_128 = _mm_add_epi16(
        _mm256_castsi256_si128(sum_reg), _mm256_extractf128_si256(sum_reg, 1));
    const __m128i sse_reg_128 = _mm_add_epi32(
        _mm256_castsi256_si128(sse_reg), _mm256_extractf128_si256(sse_reg, 1));

    // sum upper and lower 64 bits together and convert up to 32 bit values
    const __m128i sum_reg_64 =
        _mm_add_epi16(sum_reg_128, _mm_srli_si128(sum_reg_128, 8));
    const __m128i sum_int32 = _mm_cvtepi16_epi32(sum_reg_64);

    // unpack sse and sum registers and add
    const __m128i sse_sum_lo = _mm_unpacklo_epi32(sse_reg_128, sum_int32);
    const __m128i sse_sum_hi = _mm_unpackhi_epi32(sse_reg_128, sum_int32);
    const __m128i sse_sum = _mm_add_epi32(sse_sum_lo, sse_sum_hi);

    // perform the final summation and extract the results
    const __m128i res = _mm_add_epi32(sse_sum, _mm_srli_si128(sse_sum, 8));
    *((int *)sse) = _mm_cvtsi128_si32(res);
    *((int *)sum) = _mm_extract_epi32(res, 1);
  }
}

static void get32x16var_avx2(const unsigned char *src_ptr, int source_stride,
                             const unsigned char *ref_ptr, int recon_stride,
                             unsigned int *sse, int *sum) {
  unsigned int i, src_2strides, ref_2strides;
  const __m256i adj_sub = _mm256_load_si256((__m256i const *)adjacent_sub_avx2);
  __m256i sum_reg = _mm256_setzero_si256();
  __m256i sse_reg = _mm256_setzero_si256();

  // process 64 elements in an iteration
  src_2strides = source_stride << 1;
  ref_2strides = recon_stride << 1;
  for (i = 0; i < 8; i++) {
    const __m256i src0 = _mm256_loadu_si256((__m256i const *)(src_ptr));
    const __m256i src1 =
        _mm256_loadu_si256((__m256i const *)(src_ptr + source_stride));
    const __m256i ref0 = _mm256_loadu_si256((__m256i const *)(ref_ptr));
    const __m256i ref1 =
        _mm256_loadu_si256((__m256i const *)(ref_ptr + recon_stride));

    // unpack into pairs of source and reference values
    const __m256i src_ref0 = _mm256_unpacklo_epi8(src0, ref0);
    const __m256i src_ref1 = _mm256_unpackhi_epi8(src0, ref0);
    const __m256i src_ref2 = _mm256_unpacklo_epi8(src1, ref1);
    const __m256i src_ref3 = _mm256_unpackhi_epi8(src1, ref1);

    // subtract adjacent elements using src*1 + ref*-1
    const __m256i diff0 = _mm256_maddubs_epi16(src_ref0, adj_sub);
    const __m256i diff1 = _mm256_maddubs_epi16(src_ref1, adj_sub);
    const __m256i diff2 = _mm256_maddubs_epi16(src_ref2, adj_sub);
    const __m256i diff3 = _mm256_maddubs_epi16(src_ref3, adj_sub);
    const __m256i madd0 = _mm256_madd_epi16(diff0, diff0);
    const __m256i madd1 = _mm256_madd_epi16(diff1, diff1);
    const __m256i madd2 = _mm256_madd_epi16(diff2, diff2);
    const __m256i madd3 = _mm256_madd_epi16(diff3, diff3);

    // add to the running totals
    sum_reg = _mm256_add_epi16(sum_reg, _mm256_add_epi16(diff0, diff1));
    sum_reg = _mm256_add_epi16(sum_reg, _mm256_add_epi16(diff2, diff3));
    sse_reg = _mm256_add_epi32(sse_reg, _mm256_add_epi32(madd0, madd1));
    sse_reg = _mm256_add_epi32(sse_reg, _mm256_add_epi32(madd2, madd3));

    src_ptr += src_2strides;
    ref_ptr += ref_2strides;
  }

  {
    // extract the low lane and add it to the high lane
    const __m128i sum_reg_128 = _mm_add_epi16(
        _mm256_castsi256_si128(sum_reg), _mm256_extractf128_si256(sum_reg, 1));
    const __m128i sse_reg_128 = _mm_add_epi32(
        _mm256_castsi256_si128(sse_reg), _mm256_extractf128_si256(sse_reg, 1));

    // sum upper and lower 64 bits together and convert up to 32 bit values
    const __m128i sum_reg_64 =
        _mm_add_epi16(sum_reg_128, _mm_srli_si128(sum_reg_128, 8));
    const __m128i sum_int32 = _mm_cvtepi16_epi32(sum_reg_64);

    // unpack sse and sum registers and add
    const __m128i sse_sum_lo = _mm_unpacklo_epi32(sse_reg_128, sum_int32);
    const __m128i sse_sum_hi = _mm_unpackhi_epi32(sse_reg_128, sum_int32);
    const __m128i sse_sum = _mm_add_epi32(sse_sum_lo, sse_sum_hi);

    // perform the final summation and extract the results
    const __m128i res = _mm_add_epi32(sse_sum, _mm_srli_si128(sse_sum, 8));
    *((int *)sse) = _mm_cvtsi128_si32(res);
    *((int *)sum) = _mm_extract_epi32(res, 1);
  }
}

#define FILTER_SRC(filter)                               \
  /* filter the source */                                \
  exp_src_lo = _mm256_maddubs_epi16(exp_src_lo, filter); \
  exp_src_hi = _mm256_maddubs_epi16(exp_src_hi, filter); \
                                                         \
  /* add 8 to source */                                  \
  exp_src_lo = _mm256_add_epi16(exp_src_lo, pw8);        \
  exp_src_hi = _mm256_add_epi16(exp_src_hi, pw8);        \
                                                         \
  /* divide source by 16 */                              \
  exp_src_lo = _mm256_srai_epi16(exp_src_lo, 4);         \
  exp_src_hi = _mm256_srai_epi16(exp_src_hi, 4);

#define CALC_SUM_SSE_INSIDE_LOOP                          \
  /* expand each byte to 2 bytes */                       \
  exp_dst_lo = _mm256_unpacklo_epi8(dst_reg, zero_reg);   \
  exp_dst_hi = _mm256_unpackhi_epi8(dst_reg, zero_reg);   \
  /* source - dest */                                     \
  exp_src_lo = _mm256_sub_epi16(exp_src_lo, exp_dst_lo);  \
  exp_src_hi = _mm256_sub_epi16(exp_src_hi, exp_dst_hi);  \
  /* caculate sum */                                      \
  *sum_reg = _mm256_add_epi16(*sum_reg, exp_src_lo);      \
  exp_src_lo = _mm256_madd_epi16(exp_src_lo, exp_src_lo); \
  *sum_reg = _mm256_add_epi16(*sum_reg, exp_src_hi);      \
  exp_src_hi = _mm256_madd_epi16(exp_src_hi, exp_src_hi); \
  /* calculate sse */                                     \
  *sse_reg = _mm256_add_epi32(*sse_reg, exp_src_lo);      \
  *sse_reg = _mm256_add_epi32(*sse_reg, exp_src_hi);

// final calculation to sum and sse
#define CALC_SUM_AND_SSE                                                   \
  res_cmp = _mm256_cmpgt_epi16(zero_reg, sum_reg);                         \
  sse_reg_hi = _mm256_srli_si256(sse_reg, 8);                              \
  sum_reg_lo = _mm256_unpacklo_epi16(sum_reg, res_cmp);                    \
  sum_reg_hi = _mm256_unpackhi_epi16(sum_reg, res_cmp);                    \
  sse_reg = _mm256_add_epi32(sse_reg, sse_reg_hi);                         \
  sum_reg = _mm256_add_epi32(sum_reg_lo, sum_reg_hi);                      \
                                                                           \
  sse_reg_hi = _mm256_srli_si256(sse_reg, 4);                              \
  sum_reg_hi = _mm256_srli_si256(sum_reg, 8);                              \
                                                                           \
  sse_reg = _mm256_add_epi32(sse_reg, sse_reg_hi);                         \
  sum_reg = _mm256_add_epi32(sum_reg, sum_reg_hi);                         \
  *((int *)sse) = _mm_cvtsi128_si32(_mm256_castsi256_si128(sse_reg)) +     \
                  _mm_cvtsi128_si32(_mm256_extractf128_si256(sse_reg, 1)); \
  sum_reg_hi = _mm256_srli_si256(sum_reg, 4);                              \
  sum_reg = _mm256_add_epi32(sum_reg, sum_reg_hi);                         \
  sum = _mm_cvtsi128_si32(_mm256_castsi256_si128(sum_reg)) +               \
        _mm_cvtsi128_si32(_mm256_extractf128_si256(sum_reg, 1));

static INLINE void spv32_x0_y0(const uint8_t *src, int src_stride,
                               const uint8_t *dst, int dst_stride,
                               const uint8_t *sec, int sec_stride, int do_sec,
                               int height, __m256i *sum_reg, __m256i *sse_reg) {
  const __m256i zero_reg = _mm256_setzero_si256();
  __m256i exp_src_lo, exp_src_hi, exp_dst_lo, exp_dst_hi;
  int i;
  for (i = 0; i < height; i++) {
    const __m256i dst_reg = _mm256_loadu_si256((__m256i const *)dst);
    const __m256i src_reg = _mm256_loadu_si256((__m256i const *)src);
    if (do_sec) {
      const __m256i sec_reg = _mm256_loadu_si256((__m256i const *)sec);
      const __m256i avg_reg = _mm256_avg_epu8(src_reg, sec_reg);
      exp_src_lo = _mm256_unpacklo_epi8(avg_reg, zero_reg);
      exp_src_hi = _mm256_unpackhi_epi8(avg_reg, zero_reg);
      sec += sec_stride;
    } else {
      exp_src_lo = _mm256_unpacklo_epi8(src_reg, zero_reg);
      exp_src_hi = _mm256_unpackhi_epi8(src_reg, zero_reg);
    }
    CALC_SUM_SSE_INSIDE_LOOP
    src += src_stride;
    dst += dst_stride;
  }
}

// (x == 0, y == 4) or (x == 4, y == 0).  sstep determines the direction.
static INLINE void spv32_half_zero(const uint8_t *src, int src_stride,
                                   const uint8_t *dst, int dst_stride,
                                   const uint8_t *sec, int sec_stride,
                                   int do_sec, int height, __m256i *sum_reg,
                                   __m256i *sse_reg, int sstep) {
  const __m256i zero_reg = _mm256_setzero_si256();
  __m256i exp_src_lo, exp_src_hi, exp_dst_lo, exp_dst_hi;
  int i;
  for (i = 0; i < height; i++) {
    const __m256i dst_reg = _mm256_loadu_si256((__m256i const *)dst);
    const __m256i src_0 = _mm256_loadu_si256((__m256i const *)src);
    const __m256i src_1 = _mm256_loadu_si256((__m256i const *)(src + sstep));
    const __m256i src_avg = _mm256_avg_epu8(src_0, src_1);
    if (do_sec) {
      const __m256i sec_reg = _mm256_loadu_si256((__m256i const *)sec);
      const __m256i avg_reg = _mm256_avg_epu8(src_avg, sec_reg);
      exp_src_lo = _mm256_unpacklo_epi8(avg_reg, zero_reg);
      exp_src_hi = _mm256_unpackhi_epi8(avg_reg, zero_reg);
      sec += sec_stride;
    } else {
      exp_src_lo = _mm256_unpacklo_epi8(src_avg, zero_reg);
      exp_src_hi = _mm256_unpackhi_epi8(src_avg, zero_reg);
    }
    CALC_SUM_SSE_INSIDE_LOOP
    src += src_stride;
    dst += dst_stride;
  }
}

static INLINE void spv32_x0_y4(const uint8_t *src, int src_stride,
                               const uint8_t *dst, int dst_stride,
                               const uint8_t *sec, int sec_stride, int do_sec,
                               int height, __m256i *sum_reg, __m256i *sse_reg) {
  spv32_half_zero(src, src_stride, dst, dst_stride, sec, sec_stride, do_sec,
                  height, sum_reg, sse_reg, src_stride);
}

static INLINE void spv32_x4_y0(const uint8_t *src, int src_stride,
                               const uint8_t *dst, int dst_stride,
                               const uint8_t *sec, int sec_stride, int do_sec,
                               int height, __m256i *sum_reg, __m256i *sse_reg) {
  spv32_half_zero(src, src_stride, dst, dst_stride, sec, sec_stride, do_sec,
                  height, sum_reg, sse_reg, 1);
}

static INLINE void spv32_x4_y4(const uint8_t *src, int src_stride,
                               const uint8_t *dst, int dst_stride,
                               const uint8_t *sec, int sec_stride, int do_sec,
                               int height, __m256i *sum_reg, __m256i *sse_reg) {
  const __m256i zero_reg = _mm256_setzero_si256();
  const __m256i src_a = _mm256_loadu_si256((__m256i const *)src);
  const __m256i src_b = _mm256_loadu_si256((__m256i const *)(src + 1));
  __m256i prev_src_avg = _mm256_avg_epu8(src_a, src_b);
  __m256i exp_src_lo, exp_src_hi, exp_dst_lo, exp_dst_hi;
  int i;
  src += src_stride;
  for (i = 0; i < height; i++) {
    const __m256i dst_reg = _mm256_loadu_si256((__m256i const *)dst);
    const __m256i src_0 = _mm256_loadu_si256((__m256i const *)(src));
    const __m256i src_1 = _mm256_loadu_si256((__m256i const *)(src + 1));
    const __m256i src_avg = _mm256_avg_epu8(src_0, src_1);
    const __m256i current_avg = _mm256_avg_epu8(prev_src_avg, src_avg);
    prev_src_avg = src_avg;

    if (do_sec) {
      const __m256i sec_reg = _mm256_loadu_si256((__m256i const *)sec);
      const __m256i avg_reg = _mm256_avg_epu8(current_avg, sec_reg);
      exp_src_lo = _mm256_unpacklo_epi8(avg_reg, zero_reg);
      exp_src_hi = _mm256_unpackhi_epi8(avg_reg, zero_reg);
      sec += sec_stride;
    } else {
      exp_src_lo = _mm256_unpacklo_epi8(current_avg, zero_reg);
      exp_src_hi = _mm256_unpackhi_epi8(current_avg, zero_reg);
    }
    // save current source average
    CALC_SUM_SSE_INSIDE_LOOP
    dst += dst_stride;
    src += src_stride;
  }
}

// (x == 0, y == bil) or (x == 4, y == bil).  sstep determines the direction.
static INLINE void spv32_bilin_zero(const uint8_t *src, int src_stride,
                                    const uint8_t *dst, int dst_stride,
                                    const uint8_t *sec, int sec_stride,
                                    int do_sec, int height, __m256i *sum_reg,
                                    __m256i *sse_reg, int offset, int sstep) {
  const __m256i zero_reg = _mm256_setzero_si256();
  const __m256i pw8 = _mm256_set1_epi16(8);
  const __m256i filter = _mm256_load_si256(
      (__m256i const *)(bilinear_filters_avx2 + (offset << 5)));
  __m256i exp_src_lo, exp_src_hi, exp_dst_lo, exp_dst_hi;
  int i;
  for (i = 0; i < height; i++) {
    const __m256i dst_reg = _mm256_loadu_si256((__m256i const *)dst);
    const __m256i src_0 = _mm256_loadu_si256((__m256i const *)src);
    const __m256i src_1 = _mm256_loadu_si256((__m256i const *)(src + sstep));
    exp_src_lo = _mm256_unpacklo_epi8(src_0, src_1);
    exp_src_hi = _mm256_unpackhi_epi8(src_0, src_1);

    FILTER_SRC(filter)
    if (do_sec) {
      const __m256i sec_reg = _mm256_loadu_si256((__m256i const *)sec);
      const __m256i exp_src = _mm256_packus_epi16(exp_src_lo, exp_src_hi);
      const __m256i avg_reg = _mm256_avg_epu8(exp_src, sec_reg);
      sec += sec_stride;
      exp_src_lo = _mm256_unpacklo_epi8(avg_reg, zero_reg);
      exp_src_hi = _mm256_unpackhi_epi8(avg_reg, zero_reg);
    }
    CALC_SUM_SSE_INSIDE_LOOP
    src += src_stride;
    dst += dst_stride;
  }
}

static INLINE void spv32_x0_yb(const uint8_t *src, int src_stride,
                               const uint8_t *dst, int dst_stride,
                               const uint8_t *sec, int sec_stride, int do_sec,
                               int height, __m256i *sum_reg, __m256i *sse_reg,
                               int y_offset) {
  spv32_bilin_zero(src, src_stride, dst, dst_stride, sec, sec_stride, do_sec,
                   height, sum_reg, sse_reg, y_offset, src_stride);
}

static INLINE void spv32_xb_y0(const uint8_t *src, int src_stride,
                               const uint8_t *dst, int dst_stride,
                               const uint8_t *sec, int sec_stride, int do_sec,
                               int height, __m256i *sum_reg, __m256i *sse_reg,
                               int x_offset) {
  spv32_bilin_zero(src, src_stride, dst, dst_stride, sec, sec_stride, do_sec,
                   height, sum_reg, sse_reg, x_offset, 1);
}

static INLINE void spv32_x4_yb(const uint8_t *src, int src_stride,
                               const uint8_t *dst, int dst_stride,
                               const uint8_t *sec, int sec_stride, int do_sec,
                               int height, __m256i *sum_reg, __m256i *sse_reg,
                               int y_offset) {
  const __m256i zero_reg = _mm256_setzero_si256();
  const __m256i pw8 = _mm256_set1_epi16(8);
  const __m256i filter = _mm256_load_si256(
      (__m256i const *)(bilinear_filters_avx2 + (y_offset << 5)));
  const __m256i src_a = _mm256_loadu_si256((__m256i const *)src);
  const __m256i src_b = _mm256_loadu_si256((__m256i const *)(src + 1));
  __m256i prev_src_avg = _mm256_avg_epu8(src_a, src_b);
  __m256i exp_src_lo, exp_src_hi, exp_dst_lo, exp_dst_hi;
  int i;
  src += src_stride;
  for (i = 0; i < height; i++) {
    const __m256i dst_reg = _mm256_loadu_si256((__m256i const *)dst);
    const __m256i src_0 = _mm256_loadu_si256((__m256i const *)src);
    const __m256i src_1 = _mm256_loadu_si256((__m256i const *)(src + 1));
    const __m256i src_avg = _mm256_avg_epu8(src_0, src_1);
    exp_src_lo = _mm256_unpacklo_epi8(prev_src_avg, src_avg);
    exp_src_hi = _mm256_unpackhi_epi8(prev_src_avg, src_avg);
    prev_src_avg = src_avg;

    FILTER_SRC(filter)
    if (do_sec) {
      const __m256i sec_reg = _mm256_loadu_si256((__m256i const *)sec);
      const __m256i exp_src_avg = _mm256_packus_epi16(exp_src_lo, exp_src_hi);
      const __m256i avg_reg = _mm256_avg_epu8(exp_src_avg, sec_reg);
      exp_src_lo = _mm256_unpacklo_epi8(avg_reg, zero_reg);
      exp_src_hi = _mm256_unpackhi_epi8(avg_reg, zero_reg);
      sec += sec_stride;
    }
    CALC_SUM_SSE_INSIDE_LOOP
    dst += dst_stride;
    src += src_stride;
  }
}

static INLINE void spv32_xb_y4(const uint8_t *src, int src_stride,
                               const uint8_t *dst, int dst_stride,
                               const uint8_t *sec, int sec_stride, int do_sec,
                               int height, __m256i *sum_reg, __m256i *sse_reg,
                               int x_offset) {
  const __m256i zero_reg = _mm256_setzero_si256();
  const __m256i pw8 = _mm256_set1_epi16(8);
  const __m256i filter = _mm256_load_si256(
      (__m256i const *)(bilinear_filters_avx2 + (x_offset << 5)));
  const __m256i src_a = _mm256_loadu_si256((__m256i const *)src);
  const __m256i src_b = _mm256_loadu_si256((__m256i const *)(src + 1));
  __m256i exp_src_lo, exp_src_hi, exp_dst_lo, exp_dst_hi;
  __m256i src_reg, src_pack;
  int i;
  exp_src_lo = _mm256_unpacklo_epi8(src_a, src_b);
  exp_src_hi = _mm256_unpackhi_epi8(src_a, src_b);
  FILTER_SRC(filter)
  // convert each 16 bit to 8 bit to each low and high lane source
  src_pack = _mm256_packus_epi16(exp_src_lo, exp_src_hi);

  src += src_stride;
  for (i = 0; i < height; i++) {
    const __m256i dst_reg = _mm256_loadu_si256((__m256i const *)dst);
    const __m256i src_0 = _mm256_loadu_si256((__m256i const *)src);
    const __m256i src_1 = _mm256_loadu_si256((__m256i const *)(src + 1));
    exp_src_lo = _mm256_unpacklo_epi8(src_0, src_1);
    exp_src_hi = _mm256_unpackhi_epi8(src_0, src_1);

    FILTER_SRC(filter)

    src_reg = _mm256_packus_epi16(exp_src_lo, exp_src_hi);
    // average between previous pack to the current
    src_pack = _mm256_avg_epu8(src_pack, src_reg);

    if (do_sec) {
      const __m256i sec_reg = _mm256_loadu_si256((__m256i const *)sec);
      const __m256i avg_pack = _mm256_avg_epu8(src_pack, sec_reg);
      exp_src_lo = _mm256_unpacklo_epi8(avg_pack, zero_reg);
      exp_src_hi = _mm256_unpackhi_epi8(avg_pack, zero_reg);
      sec += sec_stride;
    } else {
      exp_src_lo = _mm256_unpacklo_epi8(src_pack, zero_reg);
      exp_src_hi = _mm256_unpackhi_epi8(src_pack, zero_reg);
    }
    CALC_SUM_SSE_INSIDE_LOOP
    src_pack = src_reg;
    dst += dst_stride;
    src += src_stride;
  }
}

static INLINE void spv32_xb_yb(const uint8_t *src, int src_stride,
                               const uint8_t *dst, int dst_stride,
                               const uint8_t *sec, int sec_stride, int do_sec,
                               int height, __m256i *sum_reg, __m256i *sse_reg,
                               int x_offset, int y_offset) {
  const __m256i zero_reg = _mm256_setzero_si256();
  const __m256i pw8 = _mm256_set1_epi16(8);
  const __m256i xfilter = _mm256_load_si256(
      (__m256i const *)(bilinear_filters_avx2 + (x_offset << 5)));
  const __m256i yfilter = _mm256_load_si256(
      (__m256i const *)(bilinear_filters_avx2 + (y_offset << 5)));
  const __m256i src_a = _mm256_loadu_si256((__m256i const *)src);
  const __m256i src_b = _mm256_loadu_si256((__m256i const *)(src + 1));
  __m256i exp_src_lo, exp_src_hi, exp_dst_lo, exp_dst_hi;
  __m256i prev_src_pack, src_pack;
  int i;
  exp_src_lo = _mm256_unpacklo_epi8(src_a, src_b);
  exp_src_hi = _mm256_unpackhi_epi8(src_a, src_b);
  FILTER_SRC(xfilter)
  // convert each 16 bit to 8 bit to each low and high lane source
  prev_src_pack = _mm256_packus_epi16(exp_src_lo, exp_src_hi);
  src += src_stride;

  for (i = 0; i < height; i++) {
    const __m256i dst_reg = _mm256_loadu_si256((__m256i const *)dst);
    const __m256i src_0 = _mm256_loadu_si256((__m256i const *)src);
    const __m256i src_1 = _mm256_loadu_si256((__m256i const *)(src + 1));
    exp_src_lo = _mm256_unpacklo_epi8(src_0, src_1);
    exp_src_hi = _mm256_unpackhi_epi8(src_0, src_1);

    FILTER_SRC(xfilter)
    src_pack = _mm256_packus_epi16(exp_src_lo, exp_src_hi);

    // merge previous pack to current pack source
    exp_src_lo = _mm256_unpacklo_epi8(prev_src_pack, src_pack);
    exp_src_hi = _mm256_unpackhi_epi8(prev_src_pack, src_pack);

    FILTER_SRC(yfilter)
    if (do_sec) {
      const __m256i sec_reg = _mm256_loadu_si256((__m256i const *)sec);
      const __m256i exp_src = _mm256_packus_epi16(exp_src_lo, exp_src_hi);
      const __m256i avg_reg = _mm256_avg_epu8(exp_src, sec_reg);
      exp_src_lo = _mm256_unpacklo_epi8(avg_reg, zero_reg);
      exp_src_hi = _mm256_unpackhi_epi8(avg_reg, zero_reg);
      sec += sec_stride;
    }

    prev_src_pack = src_pack;

    CALC_SUM_SSE_INSIDE_LOOP
    dst += dst_stride;
    src += src_stride;
  }
}

static INLINE int sub_pix_var32xh(const uint8_t *src, int src_stride,
                                  int x_offset, int y_offset,
                                  const uint8_t *dst, int dst_stride,
                                  const uint8_t *sec, int sec_stride,
                                  int do_sec, int height, unsigned int *sse) {
  const __m256i zero_reg = _mm256_setzero_si256();
  __m256i sum_reg = _mm256_setzero_si256();
  __m256i sse_reg = _mm256_setzero_si256();
  __m256i sse_reg_hi, res_cmp, sum_reg_lo, sum_reg_hi;
  int sum;
  // x_offset = 0 and y_offset = 0
  if (x_offset == 0) {
    if (y_offset == 0) {
      spv32_x0_y0(src, src_stride, dst, dst_stride, sec, sec_stride, do_sec,
                  height, &sum_reg, &sse_reg);
      // x_offset = 0 and y_offset = 4
    } else if (y_offset == 4) {
      spv32_x0_y4(src, src_stride, dst, dst_stride, sec, sec_stride, do_sec,
                  height, &sum_reg, &sse_reg);
      // x_offset = 0 and y_offset = bilin interpolation
    } else {
      spv32_x0_yb(src, src_stride, dst, dst_stride, sec, sec_stride, do_sec,
                  height, &sum_reg, &sse_reg, y_offset);
    }
    // x_offset = 4  and y_offset = 0
  } else if (x_offset == 4) {
    if (y_offset == 0) {
      spv32_x4_y0(src, src_stride, dst, dst_stride, sec, sec_stride, do_sec,
                  height, &sum_reg, &sse_reg);
      // x_offset = 4  and y_offset = 4
    } else if (y_offset == 4) {
      spv32_x4_y4(src, src_stride, dst, dst_stride, sec, sec_stride, do_sec,
                  height, &sum_reg, &sse_reg);
      // x_offset = 4  and y_offset = bilin interpolation
    } else {
      spv32_x4_yb(src, src_stride, dst, dst_stride, sec, sec_stride, do_sec,
                  height, &sum_reg, &sse_reg, y_offset);
    }
    // x_offset = bilin interpolation and y_offset = 0
  } else {
    if (y_offset == 0) {
      spv32_xb_y0(src, src_stride, dst, dst_stride, sec, sec_stride, do_sec,
                  height, &sum_reg, &sse_reg, x_offset);
      // x_offset = bilin interpolation and y_offset = 4
    } else if (y_offset == 4) {
      spv32_xb_y4(src, src_stride, dst, dst_stride, sec, sec_stride, do_sec,
                  height, &sum_reg, &sse_reg, x_offset);
      // x_offset = bilin interpolation and y_offset = bilin interpolation
    } else {
      spv32_xb_yb(src, src_stride, dst, dst_stride, sec, sec_stride, do_sec,
                  height, &sum_reg, &sse_reg, x_offset, y_offset);
    }
  }
  CALC_SUM_AND_SSE
  return sum;
}

static unsigned int sub_pixel_variance32xh_avx2(
    const uint8_t *src, int src_stride, int x_offset, int y_offset,
    const uint8_t *dst, int dst_stride, int height, unsigned int *sse) {
  return sub_pix_var32xh(src, src_stride, x_offset, y_offset, dst, dst_stride,
                         NULL, 0, 0, height, sse);
}

static unsigned int sub_pixel_avg_variance32xh_avx2(
    const uint8_t *src, int src_stride, int x_offset, int y_offset,
    const uint8_t *dst, int dst_stride, const uint8_t *sec, int sec_stride,
    int height, unsigned int *sse) {
  return sub_pix_var32xh(src, src_stride, x_offset, y_offset, dst, dst_stride,
                         sec, sec_stride, 1, height, sse);
}

typedef void (*get_var_avx2)(const uint8_t *src, int src_stride,
                             const uint8_t *ref, int ref_stride,
                             unsigned int *sse, int *sum);

static void variance_avx2(const uint8_t *src, int src_stride,
                          const uint8_t *ref, int ref_stride, int w, int h,
                          unsigned int *sse, int *sum, get_var_avx2 var_fn,
                          int block_size) {
  int i, j;

  *sse = 0;
  *sum = 0;

  for (i = 0; i < h; i += 16) {
    for (j = 0; j < w; j += block_size) {
      unsigned int sse0;
      int sum0;
      var_fn(&src[src_stride * i + j], src_stride, &ref[ref_stride * i + j],
             ref_stride, &sse0, &sum0);
      *sse += sse0;
      *sum += sum0;
    }
  }
}

unsigned int vpx_variance16x16_avx2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_avx2(src, src_stride, ref, ref_stride, 16, 16, sse, &sum,
                vpx_get16x16var_avx2, 16);
  return *sse - (uint32_t)(((int64_t)sum * sum) >> 8);
}

unsigned int vpx_mse16x16_avx2(const uint8_t *src, int src_stride,
                               const uint8_t *ref, int ref_stride,
                               unsigned int *sse) {
  int sum;
  vpx_get16x16var_avx2(src, src_stride, ref, ref_stride, sse, &sum);
  return *sse;
}

unsigned int vpx_variance32x16_avx2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_avx2(src, src_stride, ref, ref_stride, 32, 16, sse, &sum,
                get32x16var_avx2, 32);
  return *sse - (uint32_t)(((int64_t)sum * sum) >> 9);
}

unsigned int vpx_variance32x32_avx2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_avx2(src, src_stride, ref, ref_stride, 32, 32, sse, &sum,
                get32x16var_avx2, 32);
  return *sse - (uint32_t)(((int64_t)sum * sum) >> 10);
}

unsigned int vpx_variance64x64_avx2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_avx2(src, src_stride, ref, ref_stride, 64, 64, sse, &sum,
                get32x16var_avx2, 32);
  return *sse - (uint32_t)(((int64_t)sum * sum) >> 12);
}

unsigned int vpx_variance64x32_avx2(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    unsigned int *sse) {
  int sum;
  variance_avx2(src, src_stride, ref, ref_stride, 64, 32, sse, &sum,
                get32x16var_avx2, 32);
  return *sse - (uint32_t)(((int64_t)sum * sum) >> 11);
}

unsigned int vpx_sub_pixel_variance64x64_avx2(const uint8_t *src,
                                              int src_stride, int x_offset,
                                              int y_offset, const uint8_t *dst,
                                              int dst_stride,
                                              unsigned int *sse) {
  unsigned int sse1;
  const int se1 = sub_pixel_variance32xh_avx2(
      src, src_stride, x_offset, y_offset, dst, dst_stride, 64, &sse1);
  unsigned int sse2;
  const int se2 =
      sub_pixel_variance32xh_avx2(src + 32, src_stride, x_offset, y_offset,
                                  dst + 32, dst_stride, 64, &sse2);
  const int se = se1 + se2;
  *sse = sse1 + sse2;
  return *sse - (uint32_t)(((int64_t)se * se) >> 12);
}

unsigned int vpx_sub_pixel_variance32x32_avx2(const uint8_t *src,
                                              int src_stride, int x_offset,
                                              int y_offset, const uint8_t *dst,
                                              int dst_stride,
                                              unsigned int *sse) {
  const int se = sub_pixel_variance32xh_avx2(
      src, src_stride, x_offset, y_offset, dst, dst_stride, 32, sse);
  return *sse - (uint32_t)(((int64_t)se * se) >> 10);
}

unsigned int vpx_sub_pixel_avg_variance64x64_avx2(
    const uint8_t *src, int src_stride, int x_offset, int y_offset,
    const uint8_t *dst, int dst_stride, unsigned int *sse, const uint8_t *sec) {
  unsigned int sse1;
  const int se1 = sub_pixel_avg_variance32xh_avx2(
      src, src_stride, x_offset, y_offset, dst, dst_stride, sec, 64, 64, &sse1);
  unsigned int sse2;
  const int se2 = sub_pixel_avg_variance32xh_avx2(
      src + 32, src_stride, x_offset, y_offset, dst + 32, dst_stride, sec + 32,
      64, 64, &sse2);
  const int se = se1 + se2;

  *sse = sse1 + sse2;

  return *sse - (uint32_t)(((int64_t)se * se) >> 12);
}

unsigned int vpx_sub_pixel_avg_variance32x32_avx2(
    const uint8_t *src, int src_stride, int x_offset, int y_offset,
    const uint8_t *dst, int dst_stride, unsigned int *sse, const uint8_t *sec) {
  // Process 32 elements in parallel.
  const int se = sub_pixel_avg_variance32xh_avx2(
      src, src_stride, x_offset, y_offset, dst, dst_stride, sec, 32, 32, sse);
  return *sse - (uint32_t)(((int64_t)se * se) >> 10);
}
