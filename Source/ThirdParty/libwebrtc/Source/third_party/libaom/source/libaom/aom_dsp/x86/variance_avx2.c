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

unsigned int aom_sub_pixel_variance32xh_avx2(const uint8_t *src, int src_stride,
                                             int x_offset, int y_offset,
                                             const uint8_t *dst, int dst_stride,
                                             int height, unsigned int *sse);
unsigned int aom_sub_pixel_variance16xh_avx2(const uint8_t *src, int src_stride,
                                             int x_offset, int y_offset,
                                             const uint8_t *dst, int dst_stride,
                                             int height, unsigned int *sse);

unsigned int aom_sub_pixel_avg_variance32xh_avx2(
    const uint8_t *src, int src_stride, int x_offset, int y_offset,
    const uint8_t *dst, int dst_stride, const uint8_t *sec, int sec_stride,
    int height, unsigned int *sseptr);

#define AOM_SUB_PIXEL_VAR_AVX2(w, h, wf, wlog2, hlog2)                        \
  unsigned int aom_sub_pixel_variance##w##x##h##_avx2(                        \
      const uint8_t *src, int src_stride, int x_offset, int y_offset,         \
      const uint8_t *dst, int dst_stride, unsigned int *sse_ptr) {            \
    /*Avoid overflow in helper by capping height.*/                           \
    const int hf = AOMMIN(h, 64);                                             \
    unsigned int sse = 0;                                                     \
    int se = 0;                                                               \
    for (int i = 0; i < (w / wf); ++i) {                                      \
      const uint8_t *src_ptr = src;                                           \
      const uint8_t *dst_ptr = dst;                                           \
      for (int j = 0; j < (h / hf); ++j) {                                    \
        unsigned int sse2;                                                    \
        const int se2 = aom_sub_pixel_variance##wf##xh_avx2(                  \
            src_ptr, src_stride, x_offset, y_offset, dst_ptr, dst_stride, hf, \
            &sse2);                                                           \
        dst_ptr += hf * dst_stride;                                           \
        src_ptr += hf * src_stride;                                           \
        se += se2;                                                            \
        sse += sse2;                                                          \
      }                                                                       \
      src += wf;                                                              \
      dst += wf;                                                              \
    }                                                                         \
    *sse_ptr = sse;                                                           \
    return sse - (unsigned int)(((int64_t)se * se) >> (wlog2 + hlog2));       \
  }

AOM_SUB_PIXEL_VAR_AVX2(128, 128, 32, 7, 7)
AOM_SUB_PIXEL_VAR_AVX2(128, 64, 32, 7, 6)
AOM_SUB_PIXEL_VAR_AVX2(64, 128, 32, 6, 7)
AOM_SUB_PIXEL_VAR_AVX2(64, 64, 32, 6, 6)
AOM_SUB_PIXEL_VAR_AVX2(64, 32, 32, 6, 5)
AOM_SUB_PIXEL_VAR_AVX2(32, 64, 32, 5, 6)
AOM_SUB_PIXEL_VAR_AVX2(32, 32, 32, 5, 5)
AOM_SUB_PIXEL_VAR_AVX2(32, 16, 32, 5, 4)
AOM_SUB_PIXEL_VAR_AVX2(16, 32, 16, 4, 5)
AOM_SUB_PIXEL_VAR_AVX2(16, 16, 16, 4, 4)
AOM_SUB_PIXEL_VAR_AVX2(16, 8, 16, 4, 3)
#if !CONFIG_REALTIME_ONLY
AOM_SUB_PIXEL_VAR_AVX2(16, 64, 16, 4, 6)
AOM_SUB_PIXEL_VAR_AVX2(16, 4, 16, 4, 2)
#endif

#define AOM_SUB_PIXEL_AVG_VAR_AVX2(w, h, wf, wlog2, hlog2)                \
  unsigned int aom_sub_pixel_avg_variance##w##x##h##_avx2(                \
      const uint8_t *src, int src_stride, int x_offset, int y_offset,     \
      const uint8_t *dst, int dst_stride, unsigned int *sse_ptr,          \
      const uint8_t *sec) {                                               \
    /*Avoid overflow in helper by capping height.*/                       \
    const int hf = AOMMIN(h, 64);                                         \
    unsigned int sse = 0;                                                 \
    int se = 0;                                                           \
    for (int i = 0; i < (w / wf); ++i) {                                  \
      const uint8_t *src_ptr = src;                                       \
      const uint8_t *dst_ptr = dst;                                       \
      const uint8_t *sec_ptr = sec;                                       \
      for (int j = 0; j < (h / hf); ++j) {                                \
        unsigned int sse2;                                                \
        const int se2 = aom_sub_pixel_avg_variance##wf##xh_avx2(          \
            src_ptr, src_stride, x_offset, y_offset, dst_ptr, dst_stride, \
            sec_ptr, w, hf, &sse2);                                       \
        dst_ptr += hf * dst_stride;                                       \
        src_ptr += hf * src_stride;                                       \
        sec_ptr += hf * w;                                                \
        se += se2;                                                        \
        sse += sse2;                                                      \
      }                                                                   \
      src += wf;                                                          \
      dst += wf;                                                          \
      sec += wf;                                                          \
    }                                                                     \
    *sse_ptr = sse;                                                       \
    return sse - (unsigned int)(((int64_t)se * se) >> (wlog2 + hlog2));   \
  }

AOM_SUB_PIXEL_AVG_VAR_AVX2(128, 128, 32, 7, 7)
AOM_SUB_PIXEL_AVG_VAR_AVX2(128, 64, 32, 7, 6)
AOM_SUB_PIXEL_AVG_VAR_AVX2(64, 128, 32, 6, 7)
AOM_SUB_PIXEL_AVG_VAR_AVX2(64, 64, 32, 6, 6)
AOM_SUB_PIXEL_AVG_VAR_AVX2(64, 32, 32, 6, 5)
AOM_SUB_PIXEL_AVG_VAR_AVX2(32, 64, 32, 5, 6)
AOM_SUB_PIXEL_AVG_VAR_AVX2(32, 32, 32, 5, 5)
AOM_SUB_PIXEL_AVG_VAR_AVX2(32, 16, 32, 5, 4)

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

uint64_t aom_mse_4xh_16bit_avx2(uint8_t *dst, int dstride, uint16_t *src,
                                int sstride, int h) {
  uint64_t sum = 0;
  __m128i dst0_4x8, dst1_4x8, dst2_4x8, dst3_4x8, dst_16x8;
  __m128i src0_4x16, src1_4x16, src2_4x16, src3_4x16;
  __m256i src0_8x16, src1_8x16, dst_16x16, src_16x16;
  __m256i res0_4x64, res1_4x64, res2_4x64, res3_4x64;
  __m256i sub_result;
  const __m256i zeros = _mm256_broadcastsi128_si256(_mm_setzero_si128());
  __m256i square_result = _mm256_broadcastsi128_si256(_mm_setzero_si128());
  for (int i = 0; i < h; i += 4) {
    dst0_4x8 = _mm_cvtsi32_si128(*(uint32_t const *)(&dst[(i + 0) * dstride]));
    dst1_4x8 = _mm_cvtsi32_si128(*(uint32_t const *)(&dst[(i + 1) * dstride]));
    dst2_4x8 = _mm_cvtsi32_si128(*(uint32_t const *)(&dst[(i + 2) * dstride]));
    dst3_4x8 = _mm_cvtsi32_si128(*(uint32_t const *)(&dst[(i + 3) * dstride]));
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

    sub_result = _mm256_abs_epi16(_mm256_sub_epi16(src_16x16, dst_16x16));

    src_16x16 = _mm256_unpacklo_epi16(sub_result, zeros);
    dst_16x16 = _mm256_unpackhi_epi16(sub_result, zeros);

    src_16x16 = _mm256_madd_epi16(src_16x16, src_16x16);  // 32bit store
    dst_16x16 = _mm256_madd_epi16(dst_16x16, dst_16x16);  // 32bit store

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

uint64_t aom_mse_8xh_16bit_avx2(uint8_t *dst, int dstride, uint16_t *src,
                                int sstride, int h) {
  uint64_t sum = 0;
  __m128i dst0_8x8, dst1_8x8, dst3_16x8;
  __m256i src0_8x16, src1_8x16, src_16x16, dst_16x16;
  __m256i res0_4x64, res1_4x64, res2_4x64, res3_4x64;
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

uint64_t aom_mse_wxh_16bit_avx2(uint8_t *dst, int dstride, uint16_t *src,
                                int sstride, int w, int h) {
  assert((w == 8 || w == 4) && (h == 8 || h == 4) &&
         "w=8/4 and h=8/4 must satisfy");
  switch (w) {
    case 4: return aom_mse_4xh_16bit_avx2(dst, dstride, src, sstride, h);
    case 8: return aom_mse_8xh_16bit_avx2(dst, dstride, src, sstride, h);
    default: assert(0 && "unsupported width"); return -1;
  }
}

static INLINE void sum_final_256bit_avx2(__m256i sum_8x16[2], int *const sum) {
  const __m256i sum_result_0 = _mm256_hadd_epi16(sum_8x16[0], sum_8x16[1]);
  const __m256i sum_result_1 =
      _mm256_add_epi16(_mm256_srli_si256(sum_result_0, 4), sum_result_0);
  const __m256i sum_result_2 =
      _mm256_add_epi16(_mm256_srli_si256(sum_result_1, 2), sum_result_1);
  const __m128i sum_128_high = _mm256_extractf128_si256(sum_result_2, 1);
  const __m128i sum_result_3 =
      _mm_unpacklo_epi16(_mm256_castsi256_si128(sum_result_2), sum_128_high);
  const __m128i sum_result_4 =
      _mm_unpackhi_epi16(_mm256_castsi256_si128(sum_result_2), sum_128_high);
  const __m128i sum_result_5 = _mm_unpacklo_epi32(sum_result_3, sum_result_4);

  _mm_storeu_si128((__m128i *)sum, _mm_cvtepi16_epi32(sum_result_5));
}

static INLINE void calc_sum_sse_for_8x32_block_avx2(const uint8_t *src,
                                                    const uint8_t *ref,
                                                    __m256i sse_8x16[2],
                                                    __m256i sum_8x16[2]) {
  const __m256i s0_256 =
      _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)src));
  const __m256i r0_256 =
      _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)ref));
  const __m256i s1_256 =
      _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)(src + 16)));
  const __m256i r1_256 =
      _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)(ref + 16)));
  const __m256i diff0 = _mm256_sub_epi16(s0_256, r0_256);
  const __m256i diff1 = _mm256_sub_epi16(s1_256, r1_256);

  sse_8x16[0] = _mm256_add_epi32(sse_8x16[0], _mm256_madd_epi16(diff0, diff0));
  sse_8x16[1] = _mm256_add_epi32(sse_8x16[1], _mm256_madd_epi16(diff1, diff1));
  sum_8x16[0] = _mm256_add_epi16(sum_8x16[0], diff0);
  sum_8x16[1] = _mm256_add_epi16(sum_8x16[1], diff1);
}

static INLINE void get_sse_sum_8x8_quad_avx2(const uint8_t *src,
                                             const int src_stride,
                                             const uint8_t *ref,
                                             const int ref_stride, const int h,
                                             unsigned int *const sse,
                                             int *const sum) {
  assert(h <= 128);  // May overflow for larger height.
  __m256i sse_8x16[2], sum_8x16[2];
  sum_8x16[0] = _mm256_setzero_si256();
  sse_8x16[0] = _mm256_setzero_si256();
  sum_8x16[1] = sum_8x16[0];
  sse_8x16[1] = sse_8x16[0];

  for (int i = 0; i < h; i += 2) {
    // Process 8x32 block of first row.
    calc_sum_sse_for_8x32_block_avx2(src, ref, sse_8x16, sum_8x16);

    // Process 8x32 block of second row.
    calc_sum_sse_for_8x32_block_avx2(src + src_stride, ref + ref_stride,
                                     sse_8x16, sum_8x16);

    src += src_stride << 1;
    ref += ref_stride << 1;
  }

  // Add sse registers appropriately to get each 8x8 block sse separately.
  const __m256i sse_result_1 = _mm256_hadd_epi32(sse_8x16[0], sse_8x16[1]);
  const __m256i sse_result_2 =
      _mm256_hadd_epi32(sse_result_1, _mm256_setzero_si256());
  const __m256i sse_result_3 = _mm256_permute4x64_epi64(sse_result_2, 0xd8);

  _mm_storeu_si128(
      (__m128i *)sse,
      _mm_shuffle_epi32(_mm256_castsi256_si128(sse_result_3), 0xd8));

  // Add sum registers appropriately to get each 8x8 block sum separately.
  sum_final_256bit_avx2(sum_8x16, sum);
}

void aom_get_sse_sum_8x8_quad_avx2(const uint8_t *src_ptr, int src_stride,
                                   const uint8_t *ref_ptr, int ref_stride,
                                   unsigned int *sse, int *sum) {
  get_sse_sum_8x8_quad_avx2(src_ptr, src_stride, ref_ptr, ref_stride, 8, sse,
                            sum);
}
