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

#include "aom_dsp/x86/masked_variance_intrin_ssse3.h"
#include "aom_dsp/x86/synonyms.h"

static INLINE __m128i mm256_add_hi_lo_epi16(const __m256i val) {
  return _mm_add_epi16(_mm256_castsi256_si128(val),
                       _mm256_extractf128_si256(val, 1));
}

static INLINE __m128i mm256_add_hi_lo_epi32(const __m256i val) {
  return _mm_add_epi32(_mm256_castsi256_si128(val),
                       _mm256_extractf128_si256(val, 1));
}

static INLINE void variance_kernel_avx2(const __m256i src, const __m256i ref,
                                        __m256i *const sse,
                                        __m256i *const sum) {
  const __m256i adj_sub = _mm256_set1_epi16((short)0xff01);  // (1,-1)

  // unpack into pairs of source and reference values
  const __m256i src_ref0 = _mm256_unpacklo_epi8(src, ref);
  const __m256i src_ref1 = _mm256_unpackhi_epi8(src, ref);

  // subtract adjacent elements using src*1 + ref*-1
  const __m256i diff0 = _mm256_maddubs_epi16(src_ref0, adj_sub);
  const __m256i diff1 = _mm256_maddubs_epi16(src_ref1, adj_sub);
  const __m256i madd0 = _mm256_madd_epi16(diff0, diff0);
  const __m256i madd1 = _mm256_madd_epi16(diff1, diff1);

  // add to the running totals
  *sum = _mm256_add_epi16(*sum, _mm256_add_epi16(diff0, diff1));
  *sse = _mm256_add_epi32(*sse, _mm256_add_epi32(madd0, madd1));
}

static INLINE int variance_final_from_32bit_sum_avx2(__m256i vsse, __m128i vsum,
                                                     unsigned int *const sse) {
  // extract the low lane and add it to the high lane
  const __m128i sse_reg_128 = mm256_add_hi_lo_epi32(vsse);

  // unpack sse and sum registers and add
  const __m128i sse_sum_lo = _mm_unpacklo_epi32(sse_reg_128, vsum);
  const __m128i sse_sum_hi = _mm_unpackhi_epi32(sse_reg_128, vsum);
  const __m128i sse_sum = _mm_add_epi32(sse_sum_lo, sse_sum_hi);

  // perform the final summation and extract the results
  const __m128i res = _mm_add_epi32(sse_sum, _mm_srli_si128(sse_sum, 8));
  *((int *)sse) = _mm_cvtsi128_si32(res);
  return _mm_extract_epi32(res, 1);
}

// handle pixels (<= 512)
static INLINE int variance_final_512_avx2(__m256i vsse, __m256i vsum,
                                          unsigned int *const sse) {
  // extract the low lane and add it to the high lane
  const __m128i vsum_128 = mm256_add_hi_lo_epi16(vsum);
  const __m128i vsum_64 = _mm_add_epi16(vsum_128, _mm_srli_si128(vsum_128, 8));
  const __m128i sum_int32 = _mm_cvtepi16_epi32(vsum_64);
  return variance_final_from_32bit_sum_avx2(vsse, sum_int32, sse);
}

// handle 1024 pixels (32x32, 16x64, 64x16)
static INLINE int variance_final_1024_avx2(__m256i vsse, __m256i vsum,
                                           unsigned int *const sse) {
  // extract the low lane and add it to the high lane
  const __m128i vsum_128 = mm256_add_hi_lo_epi16(vsum);
  const __m128i vsum_64 =
      _mm_add_epi32(_mm_cvtepi16_epi32(vsum_128),
                    _mm_cvtepi16_epi32(_mm_srli_si128(vsum_128, 8)));
  return variance_final_from_32bit_sum_avx2(vsse, vsum_64, sse);
}

static INLINE __m256i sum_to_32bit_avx2(const __m256i sum) {
  const __m256i sum_lo = _mm256_cvtepi16_epi32(_mm256_castsi256_si128(sum));
  const __m256i sum_hi =
      _mm256_cvtepi16_epi32(_mm256_extractf128_si256(sum, 1));
  return _mm256_add_epi32(sum_lo, sum_hi);
}

// handle 2048 pixels (32x64, 64x32)
static INLINE int variance_final_2048_avx2(__m256i vsse, __m256i vsum,
                                           unsigned int *const sse) {
  vsum = sum_to_32bit_avx2(vsum);
  const __m128i vsum_128 = mm256_add_hi_lo_epi32(vsum);
  return variance_final_from_32bit_sum_avx2(vsse, vsum_128, sse);
}

static INLINE void variance16_kernel_avx2(
    const uint8_t *const src, const int src_stride, const uint8_t *const ref,
    const int ref_stride, __m256i *const sse, __m256i *const sum) {
  const __m128i s0 = _mm_loadu_si128((__m128i const *)(src + 0 * src_stride));
  const __m128i s1 = _mm_loadu_si128((__m128i const *)(src + 1 * src_stride));
  const __m128i r0 = _mm_loadu_si128((__m128i const *)(ref + 0 * ref_stride));
  const __m128i r1 = _mm_loadu_si128((__m128i const *)(ref + 1 * ref_stride));
  const __m256i s = _mm256_inserti128_si256(_mm256_castsi128_si256(s0), s1, 1);
  const __m256i r = _mm256_inserti128_si256(_mm256_castsi128_si256(r0), r1, 1);
  variance_kernel_avx2(s, r, sse, sum);
}

static INLINE void variance32_kernel_avx2(const uint8_t *const src,
                                          const uint8_t *const ref,
                                          __m256i *const sse,
                                          __m256i *const sum) {
  const __m256i s = _mm256_loadu_si256((__m256i const *)(src));
  const __m256i r = _mm256_loadu_si256((__m256i const *)(ref));
  variance_kernel_avx2(s, r, sse, sum);
}

static INLINE void variance16_avx2(const uint8_t *src, const int src_stride,
                                   const uint8_t *ref, const int ref_stride,
                                   const int h, __m256i *const vsse,
                                   __m256i *const vsum) {
  *vsum = _mm256_setzero_si256();

  for (int i = 0; i < h; i += 2) {
    variance16_kernel_avx2(src, src_stride, ref, ref_stride, vsse, vsum);
    src += 2 * src_stride;
    ref += 2 * ref_stride;
  }
}

static INLINE void variance32_avx2(const uint8_t *src, const int src_stride,
                                   const uint8_t *ref, const int ref_stride,
                                   const int h, __m256i *const vsse,
                                   __m256i *const vsum) {
  *vsum = _mm256_setzero_si256();

  for (int i = 0; i < h; i++) {
    variance32_kernel_avx2(src, ref, vsse, vsum);
    src += src_stride;
    ref += ref_stride;
  }
}

static INLINE void variance64_avx2(const uint8_t *src, const int src_stride,
                                   const uint8_t *ref, const int ref_stride,
                                   const int h, __m256i *const vsse,
                                   __m256i *const vsum) {
  *vsum = _mm256_setzero_si256();

  for (int i = 0; i < h; i++) {
    variance32_kernel_avx2(src + 0, ref + 0, vsse, vsum);
    variance32_kernel_avx2(src + 32, ref + 32, vsse, vsum);
    src += src_stride;
    ref += ref_stride;
  }
}

static INLINE void variance128_avx2(const uint8_t *src, const int src_stride,
                                    const uint8_t *ref, const int ref_stride,
                                    const int h, __m256i *const vsse,
                                    __m256i *const vsum) {
  *vsum = _mm256_setzero_si256();

  for (int i = 0; i < h; i++) {
    variance32_kernel_avx2(src + 0, ref + 0, vsse, vsum);
    variance32_kernel_avx2(src + 32, ref + 32, vsse, vsum);
    variance32_kernel_avx2(src + 64, ref + 64, vsse, vsum);
    variance32_kernel_avx2(src + 96, ref + 96, vsse, vsum);
    src += src_stride;
    ref += ref_stride;
  }
}

#define AOM_VAR_NO_LOOP_AVX2(bw, bh, bits, max_pixel)                         \
  unsigned int aom_variance##bw##x##bh##_avx2(                                \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      unsigned int *sse) {                                                    \
    __m256i vsse = _mm256_setzero_si256();                                    \
    __m256i vsum;                                                             \
    variance##bw##_avx2(src, src_stride, ref, ref_stride, bh, &vsse, &vsum);  \
    const int sum = variance_final_##max_pixel##_avx2(vsse, vsum, sse);       \
    return *sse - (uint32_t)(((int64_t)sum * sum) >> bits);                   \
  }

AOM_VAR_NO_LOOP_AVX2(16, 8, 7, 512)
AOM_VAR_NO_LOOP_AVX2(16, 16, 8, 512)
AOM_VAR_NO_LOOP_AVX2(16, 32, 9, 512)

AOM_VAR_NO_LOOP_AVX2(32, 16, 9, 512)
AOM_VAR_NO_LOOP_AVX2(32, 32, 10, 1024)
AOM_VAR_NO_LOOP_AVX2(32, 64, 11, 2048)

AOM_VAR_NO_LOOP_AVX2(64, 32, 11, 2048)

#if !CONFIG_REALTIME_ONLY
AOM_VAR_NO_LOOP_AVX2(64, 16, 10, 1024)
AOM_VAR_NO_LOOP_AVX2(32, 8, 8, 512)
AOM_VAR_NO_LOOP_AVX2(16, 64, 10, 1024)
AOM_VAR_NO_LOOP_AVX2(16, 4, 6, 512)
#endif

#define AOM_VAR_LOOP_AVX2(bw, bh, bits, uh)                                   \
  unsigned int aom_variance##bw##x##bh##_avx2(                                \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      unsigned int *sse) {                                                    \
    __m256i vsse = _mm256_setzero_si256();                                    \
    __m256i vsum = _mm256_setzero_si256();                                    \
    for (int i = 0; i < (bh / uh); i++) {                                     \
      __m256i vsum16;                                                         \
      variance##bw##_avx2(src, src_stride, ref, ref_stride, uh, &vsse,        \
                          &vsum16);                                           \
      vsum = _mm256_add_epi32(vsum, sum_to_32bit_avx2(vsum16));               \
      src += uh * src_stride;                                                 \
      ref += uh * ref_stride;                                                 \
    }                                                                         \
    const __m128i vsum_128 = mm256_add_hi_lo_epi32(vsum);                     \
    const int sum = variance_final_from_32bit_sum_avx2(vsse, vsum_128, sse);  \
    return *sse - (unsigned int)(((int64_t)sum * sum) >> bits);               \
  }

AOM_VAR_LOOP_AVX2(64, 64, 12, 32)    // 64x32 * ( 64/32)
AOM_VAR_LOOP_AVX2(64, 128, 13, 32)   // 64x32 * (128/32)
AOM_VAR_LOOP_AVX2(128, 64, 13, 16)   // 128x16 * ( 64/16)
AOM_VAR_LOOP_AVX2(128, 128, 14, 16)  // 128x16 * (128/16)

unsigned int aom_mse16x16_avx2(const uint8_t *src, int src_stride,
                               const uint8_t *ref, int ref_stride,
                               unsigned int *sse) {
  aom_variance16x16_avx2(src, src_stride, ref, ref_stride, sse);
  return *sse;
}

static INLINE __m256i mm256_loadu2(const uint8_t *p0, const uint8_t *p1) {
  const __m256i d =
      _mm256_castsi128_si256(_mm_loadu_si128((const __m128i *)p1));
  return _mm256_insertf128_si256(d, _mm_loadu_si128((const __m128i *)p0), 1);
}

static INLINE __m256i mm256_loadu2_16(const uint16_t *p0, const uint16_t *p1) {
  const __m256i d =
      _mm256_castsi128_si256(_mm_loadu_si128((const __m128i *)p1));
  return _mm256_insertf128_si256(d, _mm_loadu_si128((const __m128i *)p0), 1);
}

static INLINE void comp_mask_pred_line_avx2(const __m256i s0, const __m256i s1,
                                            const __m256i a,
                                            uint8_t *comp_pred) {
  const __m256i alpha_max = _mm256_set1_epi8(AOM_BLEND_A64_MAX_ALPHA);
  const int16_t round_bits = 15 - AOM_BLEND_A64_ROUND_BITS;
  const __m256i round_offset = _mm256_set1_epi16(1 << (round_bits));

  const __m256i ma = _mm256_sub_epi8(alpha_max, a);

  const __m256i ssAL = _mm256_unpacklo_epi8(s0, s1);
  const __m256i aaAL = _mm256_unpacklo_epi8(a, ma);
  const __m256i ssAH = _mm256_unpackhi_epi8(s0, s1);
  const __m256i aaAH = _mm256_unpackhi_epi8(a, ma);

  const __m256i blendAL = _mm256_maddubs_epi16(ssAL, aaAL);
  const __m256i blendAH = _mm256_maddubs_epi16(ssAH, aaAH);
  const __m256i roundAL = _mm256_mulhrs_epi16(blendAL, round_offset);
  const __m256i roundAH = _mm256_mulhrs_epi16(blendAH, round_offset);

  const __m256i roundA = _mm256_packus_epi16(roundAL, roundAH);
  _mm256_storeu_si256((__m256i *)(comp_pred), roundA);
}

void aom_comp_avg_pred_avx2(uint8_t *comp_pred, const uint8_t *pred, int width,
                            int height, const uint8_t *ref, int ref_stride) {
  int row = 0;
  if (width == 8) {
    do {
      const __m256i pred_0123 = _mm256_loadu_si256((const __m256i *)(pred));
      const __m128i ref_0 = _mm_loadl_epi64((const __m128i *)(ref));
      const __m128i ref_1 =
          _mm_loadl_epi64((const __m128i *)(ref + ref_stride));
      const __m128i ref_2 =
          _mm_loadl_epi64((const __m128i *)(ref + 2 * ref_stride));
      const __m128i ref_3 =
          _mm_loadl_epi64((const __m128i *)(ref + 3 * ref_stride));
      const __m128i ref_01 = _mm_unpacklo_epi64(ref_0, ref_1);
      const __m128i ref_23 = _mm_unpacklo_epi64(ref_2, ref_3);

      const __m256i ref_0123 =
          _mm256_inserti128_si256(_mm256_castsi128_si256(ref_01), ref_23, 1);
      const __m256i average = _mm256_avg_epu8(pred_0123, ref_0123);
      _mm256_storeu_si256((__m256i *)(comp_pred), average);

      row += 4;
      pred += 32;
      comp_pred += 32;
      ref += 4 * ref_stride;
    } while (row < height);
  } else if (width == 16) {
    do {
      const __m256i pred_0 = _mm256_loadu_si256((const __m256i *)(pred));
      const __m256i pred_1 = _mm256_loadu_si256((const __m256i *)(pred + 32));
      const __m256i tmp0 =
          _mm256_castsi128_si256(_mm_loadu_si128((const __m128i *)(ref)));
      const __m256i ref_0 = _mm256_inserti128_si256(
          tmp0, _mm_loadu_si128((const __m128i *)(ref + ref_stride)), 1);
      const __m256i tmp1 = _mm256_castsi128_si256(
          _mm_loadu_si128((const __m128i *)(ref + 2 * ref_stride)));
      const __m256i ref_1 = _mm256_inserti128_si256(
          tmp1, _mm_loadu_si128((const __m128i *)(ref + 3 * ref_stride)), 1);
      const __m256i average_0 = _mm256_avg_epu8(pred_0, ref_0);
      const __m256i average_1 = _mm256_avg_epu8(pred_1, ref_1);
      _mm256_storeu_si256((__m256i *)(comp_pred), average_0);
      _mm256_storeu_si256((__m256i *)(comp_pred + 32), average_1);

      row += 4;
      pred += 64;
      comp_pred += 64;
      ref += 4 * ref_stride;
    } while (row < height);
  } else if (width == 32) {
    do {
      const __m256i pred_0 = _mm256_loadu_si256((const __m256i *)(pred));
      const __m256i pred_1 = _mm256_loadu_si256((const __m256i *)(pred + 32));
      const __m256i ref_0 = _mm256_loadu_si256((const __m256i *)(ref));
      const __m256i ref_1 =
          _mm256_loadu_si256((const __m256i *)(ref + ref_stride));
      const __m256i average_0 = _mm256_avg_epu8(pred_0, ref_0);
      const __m256i average_1 = _mm256_avg_epu8(pred_1, ref_1);
      _mm256_storeu_si256((__m256i *)(comp_pred), average_0);
      _mm256_storeu_si256((__m256i *)(comp_pred + 32), average_1);

      row += 2;
      pred += 64;
      comp_pred += 64;
      ref += 2 * ref_stride;
    } while (row < height);
  } else if (width % 64 == 0) {
    do {
      for (int x = 0; x < width; x += 64) {
        const __m256i pred_0 = _mm256_loadu_si256((const __m256i *)(pred + x));
        const __m256i pred_1 =
            _mm256_loadu_si256((const __m256i *)(pred + x + 32));
        const __m256i ref_0 = _mm256_loadu_si256((const __m256i *)(ref + x));
        const __m256i ref_1 =
            _mm256_loadu_si256((const __m256i *)(ref + x + 32));
        const __m256i average_0 = _mm256_avg_epu8(pred_0, ref_0);
        const __m256i average_1 = _mm256_avg_epu8(pred_1, ref_1);
        _mm256_storeu_si256((__m256i *)(comp_pred + x), average_0);
        _mm256_storeu_si256((__m256i *)(comp_pred + x + 32), average_1);
      }
      row++;
      pred += width;
      comp_pred += width;
      ref += ref_stride;
    } while (row < height);
  } else {
    aom_comp_avg_pred_c(comp_pred, pred, width, height, ref, ref_stride);
  }
}

void aom_comp_mask_pred_avx2(uint8_t *comp_pred, const uint8_t *pred, int width,
                             int height, const uint8_t *ref, int ref_stride,
                             const uint8_t *mask, int mask_stride,
                             int invert_mask) {
  int i = 0;
  const uint8_t *src0 = invert_mask ? pred : ref;
  const uint8_t *src1 = invert_mask ? ref : pred;
  const int stride0 = invert_mask ? width : ref_stride;
  const int stride1 = invert_mask ? ref_stride : width;
  if (width == 8) {
    comp_mask_pred_8_ssse3(comp_pred, height, src0, stride0, src1, stride1,
                           mask, mask_stride);
  } else if (width == 16) {
    do {
      const __m256i sA0 = mm256_loadu2(src0 + stride0, src0);
      const __m256i sA1 = mm256_loadu2(src1 + stride1, src1);
      const __m256i aA = mm256_loadu2(mask + mask_stride, mask);
      src0 += (stride0 << 1);
      src1 += (stride1 << 1);
      mask += (mask_stride << 1);
      const __m256i sB0 = mm256_loadu2(src0 + stride0, src0);
      const __m256i sB1 = mm256_loadu2(src1 + stride1, src1);
      const __m256i aB = mm256_loadu2(mask + mask_stride, mask);
      src0 += (stride0 << 1);
      src1 += (stride1 << 1);
      mask += (mask_stride << 1);
      // comp_pred's stride == width == 16
      comp_mask_pred_line_avx2(sA0, sA1, aA, comp_pred);
      comp_mask_pred_line_avx2(sB0, sB1, aB, comp_pred + 32);
      comp_pred += (16 << 2);
      i += 4;
    } while (i < height);
  } else {
    do {
      for (int x = 0; x < width; x += 32) {
        const __m256i sA0 = _mm256_lddqu_si256((const __m256i *)(src0 + x));
        const __m256i sA1 = _mm256_lddqu_si256((const __m256i *)(src1 + x));
        const __m256i aA = _mm256_lddqu_si256((const __m256i *)(mask + x));

        comp_mask_pred_line_avx2(sA0, sA1, aA, comp_pred);
        comp_pred += 32;
      }
      src0 += stride0;
      src1 += stride1;
      mask += mask_stride;
      i++;
    } while (i < height);
  }
}

static INLINE __m256i highbd_comp_mask_pred_line_avx2(const __m256i s0,
                                                      const __m256i s1,
                                                      const __m256i a) {
  const __m256i alpha_max = _mm256_set1_epi16((1 << AOM_BLEND_A64_ROUND_BITS));
  const __m256i round_const =
      _mm256_set1_epi32((1 << AOM_BLEND_A64_ROUND_BITS) >> 1);
  const __m256i a_inv = _mm256_sub_epi16(alpha_max, a);

  const __m256i s_lo = _mm256_unpacklo_epi16(s0, s1);
  const __m256i a_lo = _mm256_unpacklo_epi16(a, a_inv);
  const __m256i pred_lo = _mm256_madd_epi16(s_lo, a_lo);
  const __m256i pred_l = _mm256_srai_epi32(
      _mm256_add_epi32(pred_lo, round_const), AOM_BLEND_A64_ROUND_BITS);

  const __m256i s_hi = _mm256_unpackhi_epi16(s0, s1);
  const __m256i a_hi = _mm256_unpackhi_epi16(a, a_inv);
  const __m256i pred_hi = _mm256_madd_epi16(s_hi, a_hi);
  const __m256i pred_h = _mm256_srai_epi32(
      _mm256_add_epi32(pred_hi, round_const), AOM_BLEND_A64_ROUND_BITS);

  const __m256i comp = _mm256_packs_epi32(pred_l, pred_h);

  return comp;
}

void aom_highbd_comp_mask_pred_avx2(uint8_t *comp_pred8, const uint8_t *pred8,
                                    int width, int height, const uint8_t *ref8,
                                    int ref_stride, const uint8_t *mask,
                                    int mask_stride, int invert_mask) {
  int i = 0;
  uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);
  const uint16_t *src0 = invert_mask ? pred : ref;
  const uint16_t *src1 = invert_mask ? ref : pred;
  const int stride0 = invert_mask ? width : ref_stride;
  const int stride1 = invert_mask ? ref_stride : width;
  const __m256i zero = _mm256_setzero_si256();

  if (width == 8) {
    do {
      const __m256i s0 = mm256_loadu2_16(src0 + stride0, src0);
      const __m256i s1 = mm256_loadu2_16(src1 + stride1, src1);

      const __m128i m_l = _mm_loadl_epi64((const __m128i *)mask);
      const __m128i m_h = _mm_loadl_epi64((const __m128i *)(mask + 8));

      __m256i m = _mm256_castsi128_si256(m_l);
      m = _mm256_insertf128_si256(m, m_h, 1);
      const __m256i m_16 = _mm256_unpacklo_epi8(m, zero);

      const __m256i comp = highbd_comp_mask_pred_line_avx2(s0, s1, m_16);

      _mm_storeu_si128((__m128i *)(comp_pred), _mm256_castsi256_si128(comp));

      _mm_storeu_si128((__m128i *)(comp_pred + width),
                       _mm256_extractf128_si256(comp, 1));

      src0 += (stride0 << 1);
      src1 += (stride1 << 1);
      mask += (mask_stride << 1);
      comp_pred += (width << 1);
      i += 2;
    } while (i < height);
  } else if (width == 16) {
    do {
      const __m256i s0 = _mm256_loadu_si256((const __m256i *)(src0));
      const __m256i s1 = _mm256_loadu_si256((const __m256i *)(src1));
      const __m256i m_16 =
          _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)mask));

      const __m256i comp = highbd_comp_mask_pred_line_avx2(s0, s1, m_16);

      _mm256_storeu_si256((__m256i *)comp_pred, comp);

      src0 += stride0;
      src1 += stride1;
      mask += mask_stride;
      comp_pred += width;
      i += 1;
    } while (i < height);
  } else {
    do {
      for (int x = 0; x < width; x += 32) {
        const __m256i s0 = _mm256_loadu_si256((const __m256i *)(src0 + x));
        const __m256i s2 = _mm256_loadu_si256((const __m256i *)(src0 + x + 16));
        const __m256i s1 = _mm256_loadu_si256((const __m256i *)(src1 + x));
        const __m256i s3 = _mm256_loadu_si256((const __m256i *)(src1 + x + 16));

        const __m256i m01_16 =
            _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)(mask + x)));
        const __m256i m23_16 = _mm256_cvtepu8_epi16(
            _mm_loadu_si128((const __m128i *)(mask + x + 16)));

        const __m256i comp = highbd_comp_mask_pred_line_avx2(s0, s1, m01_16);
        const __m256i comp1 = highbd_comp_mask_pred_line_avx2(s2, s3, m23_16);

        _mm256_storeu_si256((__m256i *)comp_pred, comp);
        _mm256_storeu_si256((__m256i *)(comp_pred + 16), comp1);

        comp_pred += 32;
      }
      src0 += stride0;
      src1 += stride1;
      mask += mask_stride;
      i += 1;
    } while (i < height);
  }
}

static uint64_t mse_4xh_16bit_avx2(uint8_t *dst, int dstride, uint16_t *src,
                                   int sstride, int h) {
  uint64_t sum = 0;
  __m128i dst0_4x8, dst1_4x8, dst2_4x8, dst3_4x8, dst_16x8;
  __m128i src0_4x16, src1_4x16, src2_4x16, src3_4x16;
  __m256i src0_8x16, src1_8x16, dst_16x16, src_16x16;
  __m256i res0_4x64, res1_4x64;
  __m256i sub_result;
  const __m256i zeros = _mm256_broadcastsi128_si256(_mm_setzero_si128());
  __m256i square_result = _mm256_broadcastsi128_si256(_mm_setzero_si128());
  for (int i = 0; i < h; i += 4) {
    dst0_4x8 = _mm_cvtsi32_si128(*(int const *)(&dst[(i + 0) * dstride]));
    dst1_4x8 = _mm_cvtsi32_si128(*(int const *)(&dst[(i + 1) * dstride]));
    dst2_4x8 = _mm_cvtsi32_si128(*(int const *)(&dst[(i + 2) * dstride]));
    dst3_4x8 = _mm_cvtsi32_si128(*(int const *)(&dst[(i + 3) * dstride]));
    dst_16x8 = _mm_unpacklo_epi64(_mm_unpacklo_epi32(dst0_4x8, dst1_4x8),
                                  _mm_unpacklo_epi32(dst2_4x8, dst3_4x8));
    dst_16x16 = _mm256_cvtepu8_epi16(dst_16x8);

    src0_4x16 = _mm_loadl_epi64((__m128i const *)(&src[(i + 0) * sstride]));
    src1_4x16 = _mm_loadl_epi64((__m128i const *)(&src[(i + 1) * sstride]));
    src2_4x16 = _mm_loadl_epi64((__m128i const *)(&src[(i + 2) * sstride]));
    src3_4x16 = _mm_loadl_epi64((__m128i const *)(&src[(i + 3) * sstride]));
    src0_8x16 =
        _mm256_castsi128_si256(_mm_unpacklo_epi64(src0_4x16, src1_4x16));
    src1_8x16 =
        _mm256_castsi128_si256(_mm_unpacklo_epi64(src2_4x16, src3_4x16));
    src_16x16 = _mm256_permute2x128_si256(src0_8x16, src1_8x16, 0x20);

    // r15 r14 r13------------r1 r0  - 16 bit
    sub_result = _mm256_abs_epi16(_mm256_sub_epi16(src_16x16, dst_16x16));

    // s7 s6 s5 s4 s3 s2 s1 s0 - 32bit
    src_16x16 = _mm256_madd_epi16(sub_result, sub_result);

    // accumulation of result
    square_result = _mm256_add_epi32(square_result, src_16x16);
  }

  // s5 s4 s1 s0  - 64bit
  res0_4x64 = _mm256_unpacklo_epi32(square_result, zeros);
  // s7 s6 s3 s2 - 64bit
  res1_4x64 = _mm256_unpackhi_epi32(square_result, zeros);
  // r3 r2 r1 r0 - 64bit
  res0_4x64 = _mm256_add_epi64(res0_4x64, res1_4x64);
  // r1+r3 r2+r0 - 64bit
  const __m128i sum_1x64 =
      _mm_add_epi64(_mm256_castsi256_si128(res0_4x64),
                    _mm256_extracti128_si256(res0_4x64, 1));
  xx_storel_64(&sum, _mm_add_epi64(sum_1x64, _mm_srli_si128(sum_1x64, 8)));
  return sum;
}

// Compute mse of four consecutive 4x4 blocks.
// In src buffer, each 4x4 block in a 32x32 filter block is stored sequentially.
// Hence src_blk_stride is same as block width. Whereas dst buffer is a frame
// buffer, thus dstride is a frame level stride.
static uint64_t mse_4xh_quad_16bit_avx2(uint8_t *dst, int dstride,
                                        uint16_t *src, int src_blk_stride,
                                        int h) {
  uint64_t sum = 0;
  __m128i dst0_16x8, dst1_16x8, dst2_16x8, dst3_16x8;
  __m256i dst0_16x16, dst1_16x16, dst2_16x16, dst3_16x16;
  __m256i res0_4x64, res1_4x64;
  __m256i sub_result_0, sub_result_1, sub_result_2, sub_result_3;
  const __m256i zeros = _mm256_broadcastsi128_si256(_mm_setzero_si128());
  __m256i square_result = zeros;
  uint16_t *src_temp = src;

  for (int i = 0; i < h; i += 4) {
    dst0_16x8 = _mm_loadu_si128((__m128i *)(&dst[(i + 0) * dstride]));
    dst1_16x8 = _mm_loadu_si128((__m128i *)(&dst[(i + 1) * dstride]));
    dst2_16x8 = _mm_loadu_si128((__m128i *)(&dst[(i + 2) * dstride]));
    dst3_16x8 = _mm_loadu_si128((__m128i *)(&dst[(i + 3) * dstride]));

    // row0 of 1st,2nd, 3rd and 4th 4x4 blocks- d00 d10 d20 d30
    dst0_16x16 = _mm256_cvtepu8_epi16(dst0_16x8);
    // row1 of 1st,2nd, 3rd and 4th 4x4 blocks - d01 d11 d21 d31
    dst1_16x16 = _mm256_cvtepu8_epi16(dst1_16x8);
    // row2 of 1st,2nd, 3rd and 4th 4x4 blocks - d02 d12 d22 d32
    dst2_16x16 = _mm256_cvtepu8_epi16(dst2_16x8);
    // row3 of 1st,2nd, 3rd and 4th 4x4 blocks - d03 d13 d23 d33
    dst3_16x16 = _mm256_cvtepu8_epi16(dst3_16x8);

    // All rows of 1st 4x4 block - r00 r01 r02 r03
    __m256i src0_16x16 = _mm256_loadu_si256((__m256i const *)(&src_temp[0]));
    // All rows of 2nd 4x4 block - r10 r11 r12 r13
    __m256i src1_16x16 =
        _mm256_loadu_si256((__m256i const *)(&src_temp[src_blk_stride]));
    // All rows of 3rd 4x4 block - r20 r21 r22 r23
    __m256i src2_16x16 =
        _mm256_loadu_si256((__m256i const *)(&src_temp[2 * src_blk_stride]));
    // All rows of 4th 4x4 block - r30 r31 r32 r33
    __m256i src3_16x16 =
        _mm256_loadu_si256((__m256i const *)(&src_temp[3 * src_blk_stride]));

    // r00 r10 r02 r12
    __m256i tmp0_16x16 = _mm256_unpacklo_epi64(src0_16x16, src1_16x16);
    // r01 r11 r03 r13
    __m256i tmp1_16x16 = _mm256_unpackhi_epi64(src0_16x16, src1_16x16);
    // r20 r30 r22 r32
    __m256i tmp2_16x16 = _mm256_unpacklo_epi64(src2_16x16, src3_16x16);
    // r21 r31 r23 r33
    __m256i tmp3_16x16 = _mm256_unpackhi_epi64(src2_16x16, src3_16x16);

    // r00 r10 r20 r30
    src0_16x16 = _mm256_permute2f128_si256(tmp0_16x16, tmp2_16x16, 0x20);
    // r01 r11 r21 r31
    src1_16x16 = _mm256_permute2f128_si256(tmp1_16x16, tmp3_16x16, 0x20);
    // r02 r12 r22 r32
    src2_16x16 = _mm256_permute2f128_si256(tmp0_16x16, tmp2_16x16, 0x31);
    // r03 r13 r23 r33
    src3_16x16 = _mm256_permute2f128_si256(tmp1_16x16, tmp3_16x16, 0x31);

    // r15 r14 r13------------r1 r0  - 16 bit
    sub_result_0 = _mm256_abs_epi16(_mm256_sub_epi16(src0_16x16, dst0_16x16));
    sub_result_1 = _mm256_abs_epi16(_mm256_sub_epi16(src1_16x16, dst1_16x16));
    sub_result_2 = _mm256_abs_epi16(_mm256_sub_epi16(src2_16x16, dst2_16x16));
    sub_result_3 = _mm256_abs_epi16(_mm256_sub_epi16(src3_16x16, dst3_16x16));

    // s7 s6 s5 s4 s3 s2 s1 s0    - 32bit
    src0_16x16 = _mm256_madd_epi16(sub_result_0, sub_result_0);
    src1_16x16 = _mm256_madd_epi16(sub_result_1, sub_result_1);
    src2_16x16 = _mm256_madd_epi16(sub_result_2, sub_result_2);
    src3_16x16 = _mm256_madd_epi16(sub_result_3, sub_result_3);

    // accumulation of result
    src0_16x16 = _mm256_add_epi32(src0_16x16, src1_16x16);
    src2_16x16 = _mm256_add_epi32(src2_16x16, src3_16x16);
    const __m256i square_result_0 = _mm256_add_epi32(src0_16x16, src2_16x16);
    square_result = _mm256_add_epi32(square_result, square_result_0);
    src_temp += 16;
  }

  // s5 s4 s1 s0  - 64bit
  res0_4x64 = _mm256_unpacklo_epi32(square_result, zeros);
  // s7  s6  s3  s2 - 64bit
  res1_4x64 = _mm256_unpackhi_epi32(square_result, zeros);
  // r3 r2 r1 r0 - 64bit
  res0_4x64 = _mm256_add_epi64(res0_4x64, res1_4x64);
  // r1+r3 r2+r0 - 64bit
  const __m128i sum_1x64 =
      _mm_add_epi64(_mm256_castsi256_si128(res0_4x64),
                    _mm256_extracti128_si256(res0_4x64, 1));
  xx_storel_64(&sum, _mm_add_epi64(sum_1x64, _mm_srli_si128(sum_1x64, 8)));
  return sum;
}

static uint64_t mse_8xh_16bit_avx2(uint8_t *dst, int dstride, uint16_t *src,
                                   int sstride, int h) {
  uint64_t sum = 0;
  __m128i dst0_8x8, dst1_8x8, dst3_16x8;
  __m256i src0_8x16, src1_8x16, src_16x16, dst_16x16;
  __m256i res0_4x64, res1_4x64;
  __m256i sub_result;
  const __m256i zeros = _mm256_broadcastsi128_si256(_mm_setzero_si128());
  __m256i square_result = _mm256_broadcastsi128_si256(_mm_setzero_si128());

  for (int i = 0; i < h; i += 2) {
    dst0_8x8 = _mm_loadl_epi64((__m128i const *)(&dst[(i + 0) * dstride]));
    dst1_8x8 = _mm_loadl_epi64((__m128i const *)(&dst[(i + 1) * dstride]));
    dst3_16x8 = _mm_unpacklo_epi64(dst0_8x8, dst1_8x8);
    dst_16x16 = _mm256_cvtepu8_epi16(dst3_16x8);

    src0_8x16 =
        _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)&src[i * sstride]));
    src1_8x16 = _mm256_castsi128_si256(
        _mm_loadu_si128((__m128i *)&src[(i + 1) * sstride]));
    src_16x16 = _mm256_permute2x128_si256(src0_8x16, src1_8x16, 0x20);

    // r15 r14 r13 - - - r1 r0 - 16 bit
    sub_result = _mm256_abs_epi16(_mm256_sub_epi16(src_16x16, dst_16x16));

    // s7 s6 s5 s4 s3 s2 s1 s0 - 32bit
    src_16x16 = _mm256_madd_epi16(sub_result, sub_result);

    // accumulation of result
    square_result = _mm256_add_epi32(square_result, src_16x16);
  }

  // s5 s4 s1 s0  - 64bit
  res0_4x64 = _mm256_unpacklo_epi32(square_result, zeros);
  // s7 s6 s3 s2 - 64bit
  res1_4x64 = _mm256_unpackhi_epi32(square_result, zeros);
  // r3 r2 r1 r0 - 64bit
  res0_4x64 = _mm256_add_epi64(res0_4x64, res1_4x64);
  // r1+r3 r2+r0 - 64bit
  const __m128i sum_1x64 =
      _mm_add_epi64(_mm256_castsi256_si128(res0_4x64),
                    _mm256_extracti128_si256(res0_4x64, 1));
  xx_storel_64(&sum, _mm_add_epi64(sum_1x64, _mm_srli_si128(sum_1x64, 8)));
  return sum;
}

// Compute mse of two consecutive 8x8 blocks.
// In src buffer, each 8x8 block in a 64x64 filter block is stored sequentially.
// Hence src_blk_stride is same as block width. Whereas dst buffer is a frame
// buffer, thus dstride is a frame level stride.
static uint64_t mse_8xh_dual_16bit_avx2(uint8_t *dst, int dstride,
                                        uint16_t *src, int src_blk_stride,
                                        int h) {
  uint64_t sum = 0;
  __m128i dst0_16x8, dst1_16x8;
  __m256i dst0_16x16, dst1_16x16;
  __m256i res0_4x64, res1_4x64;
  __m256i sub_result_0, sub_result_1;
  const __m256i zeros = _mm256_broadcastsi128_si256(_mm_setzero_si128());
  __m256i square_result = zeros;
  uint16_t *src_temp = src;

  for (int i = 0; i < h; i += 2) {
    dst0_16x8 = _mm_loadu_si128((__m128i *)(&dst[(i + 0) * dstride]));
    dst1_16x8 = _mm_loadu_si128((__m128i *)(&dst[(i + 1) * dstride]));

    // row0 of 1st and 2nd 8x8 block - d00 d10
    dst0_16x16 = _mm256_cvtepu8_epi16(dst0_16x8);
    // row1 of 1st and 2nd 8x8 block - d01 d11
    dst1_16x16 = _mm256_cvtepu8_epi16(dst1_16x8);

    // 2 rows of 1st 8x8 block - r00 r01
    __m256i src0_16x16 = _mm256_loadu_si256((__m256i const *)(&src_temp[0]));
    // 2 rows of 2nd 8x8 block - r10 r11
    __m256i src1_16x16 =
        _mm256_loadu_si256((__m256i const *)(&src_temp[src_blk_stride]));
    // r00 r10 - 128bit
    __m256i tmp0_16x16 =
        _mm256_permute2f128_si256(src0_16x16, src1_16x16, 0x20);
    // r01 r11 - 128bit
    __m256i tmp1_16x16 =
        _mm256_permute2f128_si256(src0_16x16, src1_16x16, 0x31);

    // r15 r14 r13------------r1 r0 - 16 bit
    sub_result_0 = _mm256_abs_epi16(_mm256_sub_epi16(tmp0_16x16, dst0_16x16));
    sub_result_1 = _mm256_abs_epi16(_mm256_sub_epi16(tmp1_16x16, dst1_16x16));

    // s7 s6 s5 s4 s3 s2 s1 s0 - 32bit each
    src0_16x16 = _mm256_madd_epi16(sub_result_0, sub_result_0);
    src1_16x16 = _mm256_madd_epi16(sub_result_1, sub_result_1);

    // accumulation of result
    src0_16x16 = _mm256_add_epi32(src0_16x16, src1_16x16);
    square_result = _mm256_add_epi32(square_result, src0_16x16);
    src_temp += 16;
  }

  // s5 s4 s1 s0  - 64bit
  res0_4x64 = _mm256_unpacklo_epi32(square_result, zeros);
  // s7 s6 s3 s2 - 64bit
  res1_4x64 = _mm256_unpackhi_epi32(square_result, zeros);
  // r3 r2 r1 r0 - 64bit
  res0_4x64 = _mm256_add_epi64(res0_4x64, res1_4x64);
  // r1+r3 r2+r0 - 64bit
  const __m128i sum_1x64 =
      _mm_add_epi64(_mm256_castsi256_si128(res0_4x64),
                    _mm256_extracti128_si256(res0_4x64, 1));
  xx_storel_64(&sum, _mm_add_epi64(sum_1x64, _mm_srli_si128(sum_1x64, 8)));
  return sum;
}

uint64_t aom_mse_wxh_16bit_avx2(uint8_t *dst, int dstride, uint16_t *src,
                                int sstride, int w, int h) {
  assert((w == 8 || w == 4) && (h == 8 || h == 4) &&
         "w=8/4 and h=8/4 must be satisfied");
  switch (w) {
    case 4: return mse_4xh_16bit_avx2(dst, dstride, src, sstride, h);
    case 8: return mse_8xh_16bit_avx2(dst, dstride, src, sstride, h);
    default: assert(0 && "unsupported width"); return -1;
  }
}

// Computes mse of two 8x8 or four 4x4 consecutive blocks. Luma plane uses 8x8
// block and Chroma uses 4x4 block. In src buffer, each block in a filter block
// is stored sequentially. Hence src_blk_stride is same as block width. Whereas
// dst buffer is a frame buffer, thus dstride is a frame level stride.
uint64_t aom_mse_16xh_16bit_avx2(uint8_t *dst, int dstride, uint16_t *src,
                                 int w, int h) {
  assert((w == 8 || w == 4) && (h == 8 || h == 4) &&
         "w=8/4 and h=8/4 must be satisfied");
  switch (w) {
    case 4: return mse_4xh_quad_16bit_avx2(dst, dstride, src, w * h, h);
    case 8: return mse_8xh_dual_16bit_avx2(dst, dstride, src, w * h, h);
    default: assert(0 && "unsupported width"); return -1;
  }
}

static INLINE void calc_sum_sse_wd32_avx2(const uint8_t *src,
                                          const uint8_t *ref,
                                          __m256i set_one_minusone,
                                          __m256i sse_8x16[2],
                                          __m256i sum_8x16[2]) {
  const __m256i s00_256 = _mm256_loadu_si256((__m256i const *)(src));
  const __m256i r00_256 = _mm256_loadu_si256((__m256i const *)(ref));

  const __m256i u_low_256 = _mm256_unpacklo_epi8(s00_256, r00_256);
  const __m256i u_high_256 = _mm256_unpackhi_epi8(s00_256, r00_256);

  const __m256i diff0 = _mm256_maddubs_epi16(u_low_256, set_one_minusone);
  const __m256i diff1 = _mm256_maddubs_epi16(u_high_256, set_one_minusone);

  sse_8x16[0] = _mm256_add_epi32(sse_8x16[0], _mm256_madd_epi16(diff0, diff0));
  sse_8x16[1] = _mm256_add_epi32(sse_8x16[1], _mm256_madd_epi16(diff1, diff1));
  sum_8x16[0] = _mm256_add_epi16(sum_8x16[0], diff0);
  sum_8x16[1] = _mm256_add_epi16(sum_8x16[1], diff1);
}

static INLINE __m256i calc_sum_sse_order(__m256i *sse_hx16, __m256i *sum_hx16,
                                         unsigned int *tot_sse, int *tot_sum) {
  // s00 s01 s10 s11 s20 s21 s30 s31
  const __m256i sse_results = _mm256_hadd_epi32(sse_hx16[0], sse_hx16[1]);
  // d00 d01 d02 d03 | d10 d11 d12 d13 | d20 d21 d22 d23 | d30 d31 d32 d33
  const __m256i sum_result_r0 = _mm256_hadd_epi16(sum_hx16[0], sum_hx16[1]);
  // d00 d01 d10 d11 | d00 d02 d10 d11 | d20 d21 d30 d31 | d20 d21 d30 d31
  const __m256i sum_result_1 = _mm256_hadd_epi16(sum_result_r0, sum_result_r0);
  // d00 d01 d10 d11 d20 d21 d30 d31 | X
  const __m256i sum_result_3 = _mm256_permute4x64_epi64(sum_result_1, 0x08);
  // d00 d01 d10 d11 d20 d21 d30 d31
  const __m256i sum_results =
      _mm256_cvtepi16_epi32(_mm256_castsi256_si128(sum_result_3));

  // Add sum & sse registers appropriately to get total sum & sse separately.
  // s0 s1 d0 d1 s2 s3 d2 d3
  const __m256i sum_sse_add = _mm256_hadd_epi32(sse_results, sum_results);
  // s0 s1 s2 s3 d0 d1 d2 d3
  const __m256i sum_sse_order_add = _mm256_permute4x64_epi64(sum_sse_add, 0xd8);
  // s0+s1 s2+s3 s0+s1 s2+s3 d0+d1 d2+d3 d0+d1 d2+d3
  const __m256i sum_sse_order_add_1 =
      _mm256_hadd_epi32(sum_sse_order_add, sum_sse_order_add);
  // s0 x x x | d0 x x x
  const __m256i sum_sse_order_add_final =
      _mm256_hadd_epi32(sum_sse_order_add_1, sum_sse_order_add_1);
  // s0
  const uint32_t first_value =
      (uint32_t)_mm256_extract_epi32(sum_sse_order_add_final, 0);
  *tot_sse += first_value;
  // d0
  const int second_value = _mm256_extract_epi32(sum_sse_order_add_final, 4);
  *tot_sum += second_value;
  return sum_sse_order_add;
}

static INLINE void get_var_sse_sum_8x8_quad_avx2(
    const uint8_t *src, int src_stride, const uint8_t *ref,
    const int ref_stride, const int h, uint32_t *sse8x8, int *sum8x8,
    unsigned int *tot_sse, int *tot_sum, uint32_t *var8x8) {
  assert(h <= 128);  // May overflow for larger height.
  __m256i sse_8x16[2], sum_8x16[2];
  sum_8x16[0] = _mm256_setzero_si256();
  sse_8x16[0] = _mm256_setzero_si256();
  sum_8x16[1] = sum_8x16[0];
  sse_8x16[1] = sse_8x16[0];
  const __m256i set_one_minusone = _mm256_set1_epi16((short)0xff01);

  for (int i = 0; i < h; i++) {
    // Process 8x32 block of one row.
    calc_sum_sse_wd32_avx2(src, ref, set_one_minusone, sse_8x16, sum_8x16);
    src += src_stride;
    ref += ref_stride;
  }

  const __m256i sum_sse_order_add =
      calc_sum_sse_order(sse_8x16, sum_8x16, tot_sse, tot_sum);

  // s0 s1 s2 s3
  _mm_storeu_si128((__m128i *)sse8x8,
                   _mm256_castsi256_si128(sum_sse_order_add));
  // d0 d1 d2 d3
  const __m128i sum_temp8x8 = _mm256_extractf128_si256(sum_sse_order_add, 1);
  _mm_storeu_si128((__m128i *)sum8x8, sum_temp8x8);

  // (d0xd0 >> 6)=f0 (d1xd1 >> 6)=f1 (d2xd2 >> 6)=f2 (d3xd3 >> 6)=f3
  const __m128i mull_results =
      _mm_srli_epi32(_mm_mullo_epi32(sum_temp8x8, sum_temp8x8), 6);
  // s0-f0=v0 s1-f1=v1 s2-f2=v2 s3-f3=v3
  const __m128i variance_8x8 =
      _mm_sub_epi32(_mm256_castsi256_si128(sum_sse_order_add), mull_results);
  // v0 v1 v2 v3
  _mm_storeu_si128((__m128i *)var8x8, variance_8x8);
}

static INLINE void get_var_sse_sum_16x16_dual_avx2(
    const uint8_t *src, int src_stride, const uint8_t *ref,
    const int ref_stride, const int h, uint32_t *sse16x16,
    unsigned int *tot_sse, int *tot_sum, uint32_t *var16x16) {
  assert(h <= 128);  // May overflow for larger height.
  __m256i sse_16x16[2], sum_16x16[2];
  sum_16x16[0] = _mm256_setzero_si256();
  sse_16x16[0] = _mm256_setzero_si256();
  sum_16x16[1] = sum_16x16[0];
  sse_16x16[1] = sse_16x16[0];
  const __m256i set_one_minusone = _mm256_set1_epi16((short)0xff01);

  for (int i = 0; i < h; i++) {
    // Process 16x32 block of one row.
    calc_sum_sse_wd32_avx2(src, ref, set_one_minusone, sse_16x16, sum_16x16);
    src += src_stride;
    ref += ref_stride;
  }

  const __m256i sum_sse_order_add =
      calc_sum_sse_order(sse_16x16, sum_16x16, tot_sse, tot_sum);

  const __m256i sum_sse_order_add_1 =
      _mm256_hadd_epi32(sum_sse_order_add, sum_sse_order_add);

  // s0+s1 s2+s3 x x
  _mm_storel_epi64((__m128i *)sse16x16,
                   _mm256_castsi256_si128(sum_sse_order_add_1));

  // d0+d1 d2+d3 x x
  const __m128i sum_temp16x16 =
      _mm256_extractf128_si256(sum_sse_order_add_1, 1);

  // (d0xd0 >> 6)=f0 (d1xd1 >> 6)=f1 (d2xd2 >> 6)=f2 (d3xd3 >> 6)=f3
  const __m128i mull_results =
      _mm_srli_epi32(_mm_mullo_epi32(sum_temp16x16, sum_temp16x16), 8);

  // s0-f0=v0 s1-f1=v1 s2-f2=v2 s3-f3=v3
  const __m128i variance_16x16 =
      _mm_sub_epi32(_mm256_castsi256_si128(sum_sse_order_add_1), mull_results);

  // v0 v1 v2 v3
  _mm_storel_epi64((__m128i *)var16x16, variance_16x16);
}

void aom_get_var_sse_sum_8x8_quad_avx2(const uint8_t *src_ptr,
                                       int source_stride,
                                       const uint8_t *ref_ptr, int ref_stride,
                                       uint32_t *sse8x8, int *sum8x8,
                                       unsigned int *tot_sse, int *tot_sum,
                                       uint32_t *var8x8) {
  get_var_sse_sum_8x8_quad_avx2(src_ptr, source_stride, ref_ptr, ref_stride, 8,
                                sse8x8, sum8x8, tot_sse, tot_sum, var8x8);
}

void aom_get_var_sse_sum_16x16_dual_avx2(const uint8_t *src_ptr,
                                         int source_stride,
                                         const uint8_t *ref_ptr, int ref_stride,
                                         uint32_t *sse16x16,
                                         unsigned int *tot_sse, int *tot_sum,
                                         uint32_t *var16x16) {
  get_var_sse_sum_16x16_dual_avx2(src_ptr, source_stride, ref_ptr, ref_stride,
                                  16, sse16x16, tot_sse, tot_sum, var16x16);
}
