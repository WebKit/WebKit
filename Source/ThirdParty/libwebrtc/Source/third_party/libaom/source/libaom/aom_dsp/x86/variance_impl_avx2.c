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

#include <immintrin.h>  // AVX2

#include "config/aom_dsp_rtcd.h"

#include "aom_ports/mem.h"

/* clang-format off */
DECLARE_ALIGNED(32, static const uint8_t, bilinear_filters_avx2[512]) = {
  16,  0, 16,  0, 16,  0, 16,  0, 16,  0, 16,  0, 16,  0, 16,  0,
  16,  0, 16,  0, 16,  0, 16,  0, 16,  0, 16,  0, 16,  0, 16,  0,
  14,  2, 14,  2, 14,  2, 14,  2, 14,  2, 14,  2, 14,  2, 14,  2,
  14,  2, 14,  2, 14,  2, 14,  2, 14,  2, 14,  2, 14,  2, 14,  2,
  12,  4, 12,  4, 12,  4, 12,  4, 12,  4, 12,  4, 12,  4, 12,  4,
  12,  4, 12,  4, 12,  4, 12,  4, 12,  4, 12,  4, 12,  4, 12,  4,
  10,  6, 10,  6, 10,  6, 10,  6, 10,  6, 10,  6, 10,  6, 10,  6,
  10,  6, 10,  6, 10,  6, 10,  6, 10,  6, 10,  6, 10,  6, 10,  6,
   8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
   8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
   6, 10,  6, 10,  6, 10,  6, 10,  6, 10,  6, 10,  6, 10,  6, 10,
   6, 10,  6, 10,  6, 10,  6, 10,  6, 10,  6, 10,  6, 10,  6, 10,
   4, 12,  4, 12,  4, 12,  4, 12,  4, 12,  4, 12,  4, 12,  4, 12,
   4, 12,  4, 12,  4, 12,  4, 12,  4, 12,  4, 12,  4, 12,  4, 12,
   2, 14,  2, 14,  2, 14,  2, 14,  2, 14,  2, 14,  2, 14,  2, 14,
   2, 14,  2, 14,  2, 14,  2, 14,  2, 14,  2, 14,  2, 14,  2, 14,
};
/* clang-format on */

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

#define MERGE_WITH_SRC(src_reg, reg)               \
  exp_src_lo = _mm256_unpacklo_epi8(src_reg, reg); \
  exp_src_hi = _mm256_unpackhi_epi8(src_reg, reg);

#define LOAD_SRC_DST                                    \
  /* load source and destination */                     \
  src_reg = _mm256_loadu_si256((__m256i const *)(src)); \
  dst_reg = _mm256_loadu_si256((__m256i const *)(dst));

#define AVG_NEXT_SRC(src_reg, size_stride)                                 \
  src_next_reg = _mm256_loadu_si256((__m256i const *)(src + size_stride)); \
  /* average between current and next stride source */                     \
  src_reg = _mm256_avg_epu8(src_reg, src_next_reg);

#define MERGE_NEXT_SRC(src_reg, size_stride)                               \
  src_next_reg = _mm256_loadu_si256((__m256i const *)(src + size_stride)); \
  MERGE_WITH_SRC(src_reg, src_next_reg)

#define CALC_SUM_SSE_INSIDE_LOOP                          \
  /* expand each byte to 2 bytes */                       \
  exp_dst_lo = _mm256_unpacklo_epi8(dst_reg, zero_reg);   \
  exp_dst_hi = _mm256_unpackhi_epi8(dst_reg, zero_reg);   \
  /* source - dest */                                     \
  exp_src_lo = _mm256_sub_epi16(exp_src_lo, exp_dst_lo);  \
  exp_src_hi = _mm256_sub_epi16(exp_src_hi, exp_dst_hi);  \
  /* caculate sum */                                      \
  sum_reg = _mm256_add_epi16(sum_reg, exp_src_lo);        \
  exp_src_lo = _mm256_madd_epi16(exp_src_lo, exp_src_lo); \
  sum_reg = _mm256_add_epi16(sum_reg, exp_src_hi);        \
  exp_src_hi = _mm256_madd_epi16(exp_src_hi, exp_src_hi); \
  /* calculate sse */                                     \
  sse_reg = _mm256_add_epi32(sse_reg, exp_src_lo);        \
  sse_reg = _mm256_add_epi32(sse_reg, exp_src_hi);

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

// Functions related to sub pixel variance width 16
#define LOAD_SRC_DST_INSERT(src_stride, dst_stride)              \
  /* load source and destination of 2 rows and insert*/          \
  src_reg = _mm256_inserti128_si256(                             \
      _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)(src))), \
      _mm_loadu_si128((__m128i *)(src + src_stride)), 1);        \
  dst_reg = _mm256_inserti128_si256(                             \
      _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)(dst))), \
      _mm_loadu_si128((__m128i *)(dst + dst_stride)), 1);

#define AVG_NEXT_SRC_INSERT(src_reg, size_stride)                              \
  src_next_reg = _mm256_inserti128_si256(                                      \
      _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)(src + size_stride))), \
      _mm_loadu_si128((__m128i *)(src + (size_stride << 1))), 1);              \
  /* average between current and next stride source */                         \
  src_reg = _mm256_avg_epu8(src_reg, src_next_reg);

#define MERGE_NEXT_SRC_INSERT(src_reg, size_stride)                            \
  src_next_reg = _mm256_inserti128_si256(                                      \
      _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)(src + size_stride))), \
      _mm_loadu_si128((__m128i *)(src + (src_stride + size_stride))), 1);      \
  MERGE_WITH_SRC(src_reg, src_next_reg)

#define LOAD_SRC_NEXT_BYTE_INSERT                                    \
  /* load source and another source from next row   */               \
  src_reg = _mm256_inserti128_si256(                                 \
      _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)(src))),     \
      _mm_loadu_si128((__m128i *)(src + src_stride)), 1);            \
  /* load source and next row source from 1 byte onwards   */        \
  src_next_reg = _mm256_inserti128_si256(                            \
      _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)(src + 1))), \
      _mm_loadu_si128((__m128i *)(src + src_stride + 1)), 1);

#define LOAD_DST_INSERT                                          \
  dst_reg = _mm256_inserti128_si256(                             \
      _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)(dst))), \
      _mm_loadu_si128((__m128i *)(dst + dst_stride)), 1);

#define LOAD_SRC_MERGE_128BIT(filter)                        \
  __m128i src_reg_0 = _mm_loadu_si128((__m128i *)(src));     \
  __m128i src_reg_1 = _mm_loadu_si128((__m128i *)(src + 1)); \
  __m128i src_lo = _mm_unpacklo_epi8(src_reg_0, src_reg_1);  \
  __m128i src_hi = _mm_unpackhi_epi8(src_reg_0, src_reg_1);  \
  __m128i filter_128bit = _mm256_castsi256_si128(filter);    \
  __m128i pw8_128bit = _mm256_castsi256_si128(pw8);

#define FILTER_SRC_128BIT(filter)             \
  /* filter the source */                     \
  src_lo = _mm_maddubs_epi16(src_lo, filter); \
  src_hi = _mm_maddubs_epi16(src_hi, filter); \
                                              \
  /* add 8 to source */                       \
  src_lo = _mm_add_epi16(src_lo, pw8_128bit); \
  src_hi = _mm_add_epi16(src_hi, pw8_128bit); \
                                              \
  /* divide source by 16 */                   \
  src_lo = _mm_srai_epi16(src_lo, 4);         \
  src_hi = _mm_srai_epi16(src_hi, 4);

// TODO(chiyotsai@google.com): These variance functions are macro-fied so we
// don't have to manually optimize the individual for-loops. We could save some
// binary size by optimizing the loops more carefully without duplicating the
// codes with a macro.
#define MAKE_SUB_PIXEL_VAR_32XH(height, log2height)                           \
  static AOM_INLINE int aom_sub_pixel_variance32x##height##_imp_avx2(         \
      const uint8_t *src, int src_stride, int x_offset, int y_offset,         \
      const uint8_t *dst, int dst_stride, unsigned int *sse) {                \
    __m256i src_reg, dst_reg, exp_src_lo, exp_src_hi, exp_dst_lo, exp_dst_hi; \
    __m256i sse_reg, sum_reg, sse_reg_hi, res_cmp, sum_reg_lo, sum_reg_hi;    \
    __m256i zero_reg;                                                         \
    int i, sum;                                                               \
    sum_reg = _mm256_setzero_si256();                                         \
    sse_reg = _mm256_setzero_si256();                                         \
    zero_reg = _mm256_setzero_si256();                                        \
                                                                              \
    /* x_offset = 0 and y_offset = 0 */                                       \
    if (x_offset == 0) {                                                      \
      if (y_offset == 0) {                                                    \
        for (i = 0; i < height; i++) {                                        \
          LOAD_SRC_DST                                                        \
          /* expend each byte to 2 bytes */                                   \
          MERGE_WITH_SRC(src_reg, zero_reg)                                   \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          src += src_stride;                                                  \
          dst += dst_stride;                                                  \
        }                                                                     \
        /* x_offset = 0 and y_offset = 4 */                                   \
      } else if (y_offset == 4) {                                             \
        __m256i src_next_reg;                                                 \
        for (i = 0; i < height; i++) {                                        \
          LOAD_SRC_DST                                                        \
          AVG_NEXT_SRC(src_reg, src_stride)                                   \
          /* expend each byte to 2 bytes */                                   \
          MERGE_WITH_SRC(src_reg, zero_reg)                                   \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          src += src_stride;                                                  \
          dst += dst_stride;                                                  \
        }                                                                     \
        /* x_offset = 0 and y_offset = bilin interpolation */                 \
      } else {                                                                \
        __m256i filter, pw8, src_next_reg;                                    \
                                                                              \
        y_offset <<= 5;                                                       \
        filter = _mm256_load_si256(                                           \
            (__m256i const *)(bilinear_filters_avx2 + y_offset));             \
        pw8 = _mm256_set1_epi16(8);                                           \
        for (i = 0; i < height; i++) {                                        \
          LOAD_SRC_DST                                                        \
          MERGE_NEXT_SRC(src_reg, src_stride)                                 \
          FILTER_SRC(filter)                                                  \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          src += src_stride;                                                  \
          dst += dst_stride;                                                  \
        }                                                                     \
      }                                                                       \
      /* x_offset = 4  and y_offset = 0 */                                    \
    } else if (x_offset == 4) {                                               \
      if (y_offset == 0) {                                                    \
        __m256i src_next_reg;                                                 \
        for (i = 0; i < height; i++) {                                        \
          LOAD_SRC_DST                                                        \
          AVG_NEXT_SRC(src_reg, 1)                                            \
          /* expand each byte to 2 bytes */                                   \
          MERGE_WITH_SRC(src_reg, zero_reg)                                   \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          src += src_stride;                                                  \
          dst += dst_stride;                                                  \
        }                                                                     \
        /* x_offset = 4  and y_offset = 4 */                                  \
      } else if (y_offset == 4) {                                             \
        __m256i src_next_reg, src_avg;                                        \
        /* load source and another source starting from the next */           \
        /* following byte */                                                  \
        src_reg = _mm256_loadu_si256((__m256i const *)(src));                 \
        AVG_NEXT_SRC(src_reg, 1)                                              \
        for (i = 0; i < height; i++) {                                        \
          src_avg = src_reg;                                                  \
          src += src_stride;                                                  \
          LOAD_SRC_DST                                                        \
          AVG_NEXT_SRC(src_reg, 1)                                            \
          /* average between previous average to current average */           \
          src_avg = _mm256_avg_epu8(src_avg, src_reg);                        \
          /* expand each byte to 2 bytes */                                   \
          MERGE_WITH_SRC(src_avg, zero_reg)                                   \
          /* save current source average */                                   \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          dst += dst_stride;                                                  \
        }                                                                     \
        /* x_offset = 4  and y_offset = bilin interpolation */                \
      } else {                                                                \
        __m256i filter, pw8, src_next_reg, src_avg;                           \
        y_offset <<= 5;                                                       \
        filter = _mm256_load_si256(                                           \
            (__m256i const *)(bilinear_filters_avx2 + y_offset));             \
        pw8 = _mm256_set1_epi16(8);                                           \
        /* load source and another source starting from the next */           \
        /* following byte */                                                  \
        src_reg = _mm256_loadu_si256((__m256i const *)(src));                 \
        AVG_NEXT_SRC(src_reg, 1)                                              \
        for (i = 0; i < height; i++) {                                        \
          /* save current source average */                                   \
          src_avg = src_reg;                                                  \
          src += src_stride;                                                  \
          LOAD_SRC_DST                                                        \
          AVG_NEXT_SRC(src_reg, 1)                                            \
          MERGE_WITH_SRC(src_avg, src_reg)                                    \
          FILTER_SRC(filter)                                                  \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          dst += dst_stride;                                                  \
        }                                                                     \
      }                                                                       \
      /* x_offset = bilin interpolation and y_offset = 0 */                   \
    } else {                                                                  \
      if (y_offset == 0) {                                                    \
        __m256i filter, pw8, src_next_reg;                                    \
        x_offset <<= 5;                                                       \
        filter = _mm256_load_si256(                                           \
            (__m256i const *)(bilinear_filters_avx2 + x_offset));             \
        pw8 = _mm256_set1_epi16(8);                                           \
        for (i = 0; i < height; i++) {                                        \
          LOAD_SRC_DST                                                        \
          MERGE_NEXT_SRC(src_reg, 1)                                          \
          FILTER_SRC(filter)                                                  \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          src += src_stride;                                                  \
          dst += dst_stride;                                                  \
        }                                                                     \
        /* x_offset = bilin interpolation and y_offset = 4 */                 \
      } else if (y_offset == 4) {                                             \
        __m256i filter, pw8, src_next_reg, src_pack;                          \
        x_offset <<= 5;                                                       \
        filter = _mm256_load_si256(                                           \
            (__m256i const *)(bilinear_filters_avx2 + x_offset));             \
        pw8 = _mm256_set1_epi16(8);                                           \
        src_reg = _mm256_loadu_si256((__m256i const *)(src));                 \
        MERGE_NEXT_SRC(src_reg, 1)                                            \
        FILTER_SRC(filter)                                                    \
        /* convert each 16 bit to 8 bit to each low and high lane source */   \
        src_pack = _mm256_packus_epi16(exp_src_lo, exp_src_hi);               \
        for (i = 0; i < height; i++) {                                        \
          src += src_stride;                                                  \
          LOAD_SRC_DST                                                        \
          MERGE_NEXT_SRC(src_reg, 1)                                          \
          FILTER_SRC(filter)                                                  \
          src_reg = _mm256_packus_epi16(exp_src_lo, exp_src_hi);              \
          /* average between previous pack to the current */                  \
          src_pack = _mm256_avg_epu8(src_pack, src_reg);                      \
          MERGE_WITH_SRC(src_pack, zero_reg)                                  \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          src_pack = src_reg;                                                 \
          dst += dst_stride;                                                  \
        }                                                                     \
        /* x_offset = bilin interpolation and y_offset = bilin interpolation  \
         */                                                                   \
      } else {                                                                \
        __m256i xfilter, yfilter, pw8, src_next_reg, src_pack;                \
        x_offset <<= 5;                                                       \
        xfilter = _mm256_load_si256(                                          \
            (__m256i const *)(bilinear_filters_avx2 + x_offset));             \
        y_offset <<= 5;                                                       \
        yfilter = _mm256_load_si256(                                          \
            (__m256i const *)(bilinear_filters_avx2 + y_offset));             \
        pw8 = _mm256_set1_epi16(8);                                           \
        /* load source and another source starting from the next */           \
        /* following byte */                                                  \
        src_reg = _mm256_loadu_si256((__m256i const *)(src));                 \
        MERGE_NEXT_SRC(src_reg, 1)                                            \
                                                                              \
        FILTER_SRC(xfilter)                                                   \
        /* convert each 16 bit to 8 bit to each low and high lane source */   \
        src_pack = _mm256_packus_epi16(exp_src_lo, exp_src_hi);               \
        for (i = 0; i < height; i++) {                                        \
          src += src_stride;                                                  \
          LOAD_SRC_DST                                                        \
          MERGE_NEXT_SRC(src_reg, 1)                                          \
          FILTER_SRC(xfilter)                                                 \
          src_reg = _mm256_packus_epi16(exp_src_lo, exp_src_hi);              \
          /* merge previous pack to current pack source */                    \
          MERGE_WITH_SRC(src_pack, src_reg)                                   \
          /* filter the source */                                             \
          FILTER_SRC(yfilter)                                                 \
          src_pack = src_reg;                                                 \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          dst += dst_stride;                                                  \
        }                                                                     \
      }                                                                       \
    }                                                                         \
    CALC_SUM_AND_SSE                                                          \
    _mm256_zeroupper();                                                       \
    return sum;                                                               \
  }                                                                           \
  unsigned int aom_sub_pixel_variance32x##height##_avx2(                      \
      const uint8_t *src, int src_stride, int x_offset, int y_offset,         \
      const uint8_t *dst, int dst_stride, unsigned int *sse) {                \
    const int sum = aom_sub_pixel_variance32x##height##_imp_avx2(             \
        src, src_stride, x_offset, y_offset, dst, dst_stride, sse);           \
    return *sse - (unsigned int)(((int64_t)sum * sum) >> (5 + log2height));   \
  }

MAKE_SUB_PIXEL_VAR_32XH(64, 6)
MAKE_SUB_PIXEL_VAR_32XH(32, 5)
MAKE_SUB_PIXEL_VAR_32XH(16, 4)

#define AOM_SUB_PIXEL_VAR_AVX2(w, h, wf, hf, wlog2, hlog2)                \
  unsigned int aom_sub_pixel_variance##w##x##h##_avx2(                    \
      const uint8_t *src, int src_stride, int x_offset, int y_offset,     \
      const uint8_t *dst, int dst_stride, unsigned int *sse_ptr) {        \
    unsigned int sse = 0;                                                 \
    int se = 0;                                                           \
    for (int i = 0; i < (w / wf); ++i) {                                  \
      const uint8_t *src_ptr = src;                                       \
      const uint8_t *dst_ptr = dst;                                       \
      for (int j = 0; j < (h / hf); ++j) {                                \
        unsigned int sse2;                                                \
        const int se2 = aom_sub_pixel_variance##wf##x##hf##_imp_avx2(     \
            src_ptr, src_stride, x_offset, y_offset, dst_ptr, dst_stride, \
            &sse2);                                                       \
        dst_ptr += hf * dst_stride;                                       \
        src_ptr += hf * src_stride;                                       \
        se += se2;                                                        \
        sse += sse2;                                                      \
      }                                                                   \
      src += wf;                                                          \
      dst += wf;                                                          \
    }                                                                     \
    *sse_ptr = sse;                                                       \
    return sse - (unsigned int)(((int64_t)se * se) >> (wlog2 + hlog2));   \
  }

// Note: hf = AOMMIN(h, 64) to avoid overflow in helper by capping height.
AOM_SUB_PIXEL_VAR_AVX2(128, 128, 32, 64, 7, 7)
AOM_SUB_PIXEL_VAR_AVX2(128, 64, 32, 64, 7, 6)
AOM_SUB_PIXEL_VAR_AVX2(64, 128, 32, 64, 6, 7)
AOM_SUB_PIXEL_VAR_AVX2(64, 64, 32, 64, 6, 6)
AOM_SUB_PIXEL_VAR_AVX2(64, 32, 32, 32, 6, 5)

#define MAKE_SUB_PIXEL_VAR_16XH(height, log2height)                           \
  unsigned int aom_sub_pixel_variance16x##height##_avx2(                      \
      const uint8_t *src, int src_stride, int x_offset, int y_offset,         \
      const uint8_t *dst, int dst_stride, unsigned int *sse) {                \
    __m256i src_reg, dst_reg, exp_src_lo, exp_src_hi, exp_dst_lo, exp_dst_hi; \
    __m256i sse_reg, sum_reg, sse_reg_hi, res_cmp, sum_reg_lo, sum_reg_hi;    \
    __m256i zero_reg;                                                         \
    int i, sum;                                                               \
    sum_reg = _mm256_setzero_si256();                                         \
    sse_reg = _mm256_setzero_si256();                                         \
    zero_reg = _mm256_setzero_si256();                                        \
                                                                              \
    /* x_offset = 0 and y_offset = 0 */                                       \
    if (x_offset == 0) {                                                      \
      if (y_offset == 0) {                                                    \
        for (i = 0; i < height; i += 2) {                                     \
          LOAD_SRC_DST_INSERT(src_stride, dst_stride)                         \
          /* expend each byte to 2 bytes */                                   \
          MERGE_WITH_SRC(src_reg, zero_reg)                                   \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          src += (src_stride << 1);                                           \
          dst += (dst_stride << 1);                                           \
        }                                                                     \
        /* x_offset = 0 and y_offset = 4 */                                   \
      } else if (y_offset == 4) {                                             \
        __m256i src_next_reg;                                                 \
        for (i = 0; i < height; i += 2) {                                     \
          LOAD_SRC_DST_INSERT(src_stride, dst_stride)                         \
          AVG_NEXT_SRC_INSERT(src_reg, src_stride)                            \
          /* expend each byte to 2 bytes */                                   \
          MERGE_WITH_SRC(src_reg, zero_reg)                                   \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          src += (src_stride << 1);                                           \
          dst += (dst_stride << 1);                                           \
        }                                                                     \
        /* x_offset = 0 and y_offset = bilin interpolation */                 \
      } else {                                                                \
        __m256i filter, pw8, src_next_reg;                                    \
        y_offset <<= 5;                                                       \
        filter = _mm256_load_si256(                                           \
            (__m256i const *)(bilinear_filters_avx2 + y_offset));             \
        pw8 = _mm256_set1_epi16(8);                                           \
        for (i = 0; i < height; i += 2) {                                     \
          LOAD_SRC_DST_INSERT(src_stride, dst_stride)                         \
          MERGE_NEXT_SRC_INSERT(src_reg, src_stride)                          \
          FILTER_SRC(filter)                                                  \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          src += (src_stride << 1);                                           \
          dst += (dst_stride << 1);                                           \
        }                                                                     \
      }                                                                       \
      /* x_offset = 4  and y_offset = 0 */                                    \
    } else if (x_offset == 4) {                                               \
      if (y_offset == 0) {                                                    \
        __m256i src_next_reg;                                                 \
        for (i = 0; i < height; i += 2) {                                     \
          LOAD_SRC_NEXT_BYTE_INSERT                                           \
          LOAD_DST_INSERT                                                     \
          /* average between current and next stride source */                \
          src_reg = _mm256_avg_epu8(src_reg, src_next_reg);                   \
          /* expand each byte to 2 bytes */                                   \
          MERGE_WITH_SRC(src_reg, zero_reg)                                   \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          src += (src_stride << 1);                                           \
          dst += (dst_stride << 1);                                           \
        }                                                                     \
        /* x_offset = 4  and y_offset = 4 */                                  \
      } else if (y_offset == 4) {                                             \
        __m256i src_next_reg, src_avg, src_temp;                              \
        /* load and insert source and next row source */                      \
        LOAD_SRC_NEXT_BYTE_INSERT                                             \
        src_avg = _mm256_avg_epu8(src_reg, src_next_reg);                     \
        src += src_stride << 1;                                               \
        for (i = 0; i < height - 2; i += 2) {                                 \
          LOAD_SRC_NEXT_BYTE_INSERT                                           \
          src_next_reg = _mm256_avg_epu8(src_reg, src_next_reg);              \
          src_temp = _mm256_permute2x128_si256(src_avg, src_next_reg, 0x21);  \
          src_temp = _mm256_avg_epu8(src_avg, src_temp);                      \
          LOAD_DST_INSERT                                                     \
          /* expand each byte to 2 bytes */                                   \
          MERGE_WITH_SRC(src_temp, zero_reg)                                  \
          /* save current source average */                                   \
          src_avg = src_next_reg;                                             \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          dst += dst_stride << 1;                                             \
          src += src_stride << 1;                                             \
        }                                                                     \
        /* last 2 rows processing happens here */                             \
        __m128i src_reg_0 = _mm_loadu_si128((__m128i *)(src));                \
        __m128i src_reg_1 = _mm_loadu_si128((__m128i *)(src + 1));            \
        src_reg_0 = _mm_avg_epu8(src_reg_0, src_reg_1);                       \
        src_next_reg = _mm256_permute2x128_si256(                             \
            src_avg, _mm256_castsi128_si256(src_reg_0), 0x21);                \
        LOAD_DST_INSERT                                                       \
        src_avg = _mm256_avg_epu8(src_avg, src_next_reg);                     \
        MERGE_WITH_SRC(src_avg, zero_reg)                                     \
        CALC_SUM_SSE_INSIDE_LOOP                                              \
      } else {                                                                \
        /* x_offset = 4  and y_offset = bilin interpolation */                \
        __m256i filter, pw8, src_next_reg, src_avg, src_temp;                 \
        y_offset <<= 5;                                                       \
        filter = _mm256_load_si256(                                           \
            (__m256i const *)(bilinear_filters_avx2 + y_offset));             \
        pw8 = _mm256_set1_epi16(8);                                           \
        /* load and insert source and next row source */                      \
        LOAD_SRC_NEXT_BYTE_INSERT                                             \
        src_avg = _mm256_avg_epu8(src_reg, src_next_reg);                     \
        src += src_stride << 1;                                               \
        for (i = 0; i < height - 2; i += 2) {                                 \
          LOAD_SRC_NEXT_BYTE_INSERT                                           \
          src_next_reg = _mm256_avg_epu8(src_reg, src_next_reg);              \
          src_temp = _mm256_permute2x128_si256(src_avg, src_next_reg, 0x21);  \
          LOAD_DST_INSERT                                                     \
          MERGE_WITH_SRC(src_avg, src_temp)                                   \
          /* save current source average */                                   \
          src_avg = src_next_reg;                                             \
          FILTER_SRC(filter)                                                  \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          dst += dst_stride << 1;                                             \
          src += src_stride << 1;                                             \
        }                                                                     \
        /* last 2 rows processing happens here */                             \
        __m128i src_reg_0 = _mm_loadu_si128((__m128i *)(src));                \
        __m128i src_reg_1 = _mm_loadu_si128((__m128i *)(src + 1));            \
        src_reg_0 = _mm_avg_epu8(src_reg_0, src_reg_1);                       \
        src_next_reg = _mm256_permute2x128_si256(                             \
            src_avg, _mm256_castsi128_si256(src_reg_0), 0x21);                \
        LOAD_DST_INSERT                                                       \
        MERGE_WITH_SRC(src_avg, src_next_reg)                                 \
        FILTER_SRC(filter)                                                    \
        CALC_SUM_SSE_INSIDE_LOOP                                              \
      }                                                                       \
      /* x_offset = bilin interpolation and y_offset = 0 */                   \
    } else {                                                                  \
      if (y_offset == 0) {                                                    \
        __m256i filter, pw8, src_next_reg;                                    \
        x_offset <<= 5;                                                       \
        filter = _mm256_load_si256(                                           \
            (__m256i const *)(bilinear_filters_avx2 + x_offset));             \
        pw8 = _mm256_set1_epi16(8);                                           \
        for (i = 0; i < height; i += 2) {                                     \
          LOAD_SRC_DST_INSERT(src_stride, dst_stride)                         \
          MERGE_NEXT_SRC_INSERT(src_reg, 1)                                   \
          FILTER_SRC(filter)                                                  \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          src += (src_stride << 1);                                           \
          dst += (dst_stride << 1);                                           \
        }                                                                     \
        /* x_offset = bilin interpolation and y_offset = 4 */                 \
      } else if (y_offset == 4) {                                             \
        __m256i filter, pw8, src_next_reg, src_pack;                          \
        x_offset <<= 5;                                                       \
        filter = _mm256_load_si256(                                           \
            (__m256i const *)(bilinear_filters_avx2 + x_offset));             \
        pw8 = _mm256_set1_epi16(8);                                           \
        /* load and insert source and next row source */                      \
        LOAD_SRC_NEXT_BYTE_INSERT                                             \
        MERGE_WITH_SRC(src_reg, src_next_reg)                                 \
        FILTER_SRC(filter)                                                    \
        /* convert each 16 bit to 8 bit to each low and high lane source */   \
        src_pack = _mm256_packus_epi16(exp_src_lo, exp_src_hi);               \
        src += src_stride << 1;                                               \
        for (i = 0; i < height - 2; i += 2) {                                 \
          LOAD_SRC_NEXT_BYTE_INSERT                                           \
          LOAD_DST_INSERT                                                     \
          MERGE_WITH_SRC(src_reg, src_next_reg)                               \
          FILTER_SRC(filter)                                                  \
          src_reg = _mm256_packus_epi16(exp_src_lo, exp_src_hi);              \
          src_next_reg = _mm256_permute2x128_si256(src_pack, src_reg, 0x21);  \
          /* average between previous pack to the current */                  \
          src_pack = _mm256_avg_epu8(src_pack, src_next_reg);                 \
          MERGE_WITH_SRC(src_pack, zero_reg)                                  \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          src_pack = src_reg;                                                 \
          src += src_stride << 1;                                             \
          dst += dst_stride << 1;                                             \
        }                                                                     \
        /* last 2 rows processing happens here */                             \
        LOAD_SRC_MERGE_128BIT(filter)                                         \
        LOAD_DST_INSERT                                                       \
        FILTER_SRC_128BIT(filter_128bit)                                      \
        src_reg_0 = _mm_packus_epi16(src_lo, src_hi);                         \
        src_next_reg = _mm256_permute2x128_si256(                             \
            src_pack, _mm256_castsi128_si256(src_reg_0), 0x21);               \
        /* average between previous pack to the current */                    \
        src_pack = _mm256_avg_epu8(src_pack, src_next_reg);                   \
        MERGE_WITH_SRC(src_pack, zero_reg)                                    \
        CALC_SUM_SSE_INSIDE_LOOP                                              \
      } else {                                                                \
        /* x_offset = bilin interpolation and y_offset = bilin interpolation  \
         */                                                                   \
        __m256i xfilter, yfilter, pw8, src_next_reg, src_pack;                \
        x_offset <<= 5;                                                       \
        xfilter = _mm256_load_si256(                                          \
            (__m256i const *)(bilinear_filters_avx2 + x_offset));             \
        y_offset <<= 5;                                                       \
        yfilter = _mm256_load_si256(                                          \
            (__m256i const *)(bilinear_filters_avx2 + y_offset));             \
        pw8 = _mm256_set1_epi16(8);                                           \
        /* load and insert source and next row source */                      \
        LOAD_SRC_NEXT_BYTE_INSERT                                             \
        MERGE_WITH_SRC(src_reg, src_next_reg)                                 \
        FILTER_SRC(xfilter)                                                   \
        /* convert each 16 bit to 8 bit to each low and high lane source */   \
        src_pack = _mm256_packus_epi16(exp_src_lo, exp_src_hi);               \
        src += src_stride << 1;                                               \
        for (i = 0; i < height - 2; i += 2) {                                 \
          LOAD_SRC_NEXT_BYTE_INSERT                                           \
          LOAD_DST_INSERT                                                     \
          MERGE_WITH_SRC(src_reg, src_next_reg)                               \
          FILTER_SRC(xfilter)                                                 \
          src_reg = _mm256_packus_epi16(exp_src_lo, exp_src_hi);              \
          src_next_reg = _mm256_permute2x128_si256(src_pack, src_reg, 0x21);  \
          /* average between previous pack to the current */                  \
          MERGE_WITH_SRC(src_pack, src_next_reg)                              \
          /* filter the source */                                             \
          FILTER_SRC(yfilter)                                                 \
          src_pack = src_reg;                                                 \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          src += src_stride << 1;                                             \
          dst += dst_stride << 1;                                             \
        }                                                                     \
        /* last 2 rows processing happens here */                             \
        LOAD_SRC_MERGE_128BIT(xfilter)                                        \
        LOAD_DST_INSERT                                                       \
        FILTER_SRC_128BIT(filter_128bit)                                      \
        src_reg_0 = _mm_packus_epi16(src_lo, src_hi);                         \
        src_next_reg = _mm256_permute2x128_si256(                             \
            src_pack, _mm256_castsi128_si256(src_reg_0), 0x21);               \
        MERGE_WITH_SRC(src_pack, src_next_reg)                                \
        FILTER_SRC(yfilter)                                                   \
        CALC_SUM_SSE_INSIDE_LOOP                                              \
      }                                                                       \
    }                                                                         \
    CALC_SUM_AND_SSE                                                          \
    _mm256_zeroupper();                                                       \
    return *sse - (unsigned int)(((int64_t)sum * sum) >> (4 + log2height));   \
  }

MAKE_SUB_PIXEL_VAR_16XH(32, 5)
MAKE_SUB_PIXEL_VAR_16XH(16, 4)
MAKE_SUB_PIXEL_VAR_16XH(8, 3)
#if !CONFIG_REALTIME_ONLY
MAKE_SUB_PIXEL_VAR_16XH(64, 6)
MAKE_SUB_PIXEL_VAR_16XH(4, 2)
#endif

#define MAKE_SUB_PIXEL_AVG_VAR_32XH(height, log2height)                       \
  static int sub_pixel_avg_variance32x##height##_imp_avx2(                    \
      const uint8_t *src, int src_stride, int x_offset, int y_offset,         \
      const uint8_t *dst, int dst_stride, const uint8_t *sec, int sec_stride, \
      unsigned int *sse) {                                                    \
    __m256i sec_reg;                                                          \
    __m256i src_reg, dst_reg, exp_src_lo, exp_src_hi, exp_dst_lo, exp_dst_hi; \
    __m256i sse_reg, sum_reg, sse_reg_hi, res_cmp, sum_reg_lo, sum_reg_hi;    \
    __m256i zero_reg;                                                         \
    int i, sum;                                                               \
    sum_reg = _mm256_setzero_si256();                                         \
    sse_reg = _mm256_setzero_si256();                                         \
    zero_reg = _mm256_setzero_si256();                                        \
                                                                              \
    /* x_offset = 0 and y_offset = 0 */                                       \
    if (x_offset == 0) {                                                      \
      if (y_offset == 0) {                                                    \
        for (i = 0; i < height; i++) {                                        \
          LOAD_SRC_DST                                                        \
          sec_reg = _mm256_loadu_si256((__m256i const *)(sec));               \
          src_reg = _mm256_avg_epu8(src_reg, sec_reg);                        \
          sec += sec_stride;                                                  \
          /* expend each byte to 2 bytes */                                   \
          MERGE_WITH_SRC(src_reg, zero_reg)                                   \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          src += src_stride;                                                  \
          dst += dst_stride;                                                  \
        }                                                                     \
      } else if (y_offset == 4) {                                             \
        __m256i src_next_reg;                                                 \
        for (i = 0; i < height; i++) {                                        \
          LOAD_SRC_DST                                                        \
          AVG_NEXT_SRC(src_reg, src_stride)                                   \
          sec_reg = _mm256_loadu_si256((__m256i const *)(sec));               \
          src_reg = _mm256_avg_epu8(src_reg, sec_reg);                        \
          sec += sec_stride;                                                  \
          /* expend each byte to 2 bytes */                                   \
          MERGE_WITH_SRC(src_reg, zero_reg)                                   \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          src += src_stride;                                                  \
          dst += dst_stride;                                                  \
        }                                                                     \
        /* x_offset = 0 and y_offset = bilin interpolation */                 \
      } else {                                                                \
        __m256i filter, pw8, src_next_reg;                                    \
                                                                              \
        y_offset <<= 5;                                                       \
        filter = _mm256_load_si256(                                           \
            (__m256i const *)(bilinear_filters_avx2 + y_offset));             \
        pw8 = _mm256_set1_epi16(8);                                           \
        for (i = 0; i < height; i++) {                                        \
          LOAD_SRC_DST                                                        \
          MERGE_NEXT_SRC(src_reg, src_stride)                                 \
          FILTER_SRC(filter)                                                  \
          src_reg = _mm256_packus_epi16(exp_src_lo, exp_src_hi);              \
          sec_reg = _mm256_loadu_si256((__m256i const *)(sec));               \
          src_reg = _mm256_avg_epu8(src_reg, sec_reg);                        \
          sec += sec_stride;                                                  \
          MERGE_WITH_SRC(src_reg, zero_reg)                                   \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          src += src_stride;                                                  \
          dst += dst_stride;                                                  \
        }                                                                     \
      }                                                                       \
      /* x_offset = 4  and y_offset = 0 */                                    \
    } else if (x_offset == 4) {                                               \
      if (y_offset == 0) {                                                    \
        __m256i src_next_reg;                                                 \
        for (i = 0; i < height; i++) {                                        \
          LOAD_SRC_DST                                                        \
          AVG_NEXT_SRC(src_reg, 1)                                            \
          sec_reg = _mm256_loadu_si256((__m256i const *)(sec));               \
          src_reg = _mm256_avg_epu8(src_reg, sec_reg);                        \
          sec += sec_stride;                                                  \
          /* expand each byte to 2 bytes */                                   \
          MERGE_WITH_SRC(src_reg, zero_reg)                                   \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          src += src_stride;                                                  \
          dst += dst_stride;                                                  \
        }                                                                     \
        /* x_offset = 4  and y_offset = 4 */                                  \
      } else if (y_offset == 4) {                                             \
        __m256i src_next_reg, src_avg;                                        \
        /* load source and another source starting from the next */           \
        /* following byte */                                                  \
        src_reg = _mm256_loadu_si256((__m256i const *)(src));                 \
        AVG_NEXT_SRC(src_reg, 1)                                              \
        for (i = 0; i < height; i++) {                                        \
          /* save current source average */                                   \
          src_avg = src_reg;                                                  \
          src += src_stride;                                                  \
          LOAD_SRC_DST                                                        \
          AVG_NEXT_SRC(src_reg, 1)                                            \
          /* average between previous average to current average */           \
          src_avg = _mm256_avg_epu8(src_avg, src_reg);                        \
          sec_reg = _mm256_loadu_si256((__m256i const *)(sec));               \
          src_avg = _mm256_avg_epu8(src_avg, sec_reg);                        \
          sec += sec_stride;                                                  \
          /* expand each byte to 2 bytes */                                   \
          MERGE_WITH_SRC(src_avg, zero_reg)                                   \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          dst += dst_stride;                                                  \
        }                                                                     \
        /* x_offset = 4  and y_offset = bilin interpolation */                \
      } else {                                                                \
        __m256i filter, pw8, src_next_reg, src_avg;                           \
        y_offset <<= 5;                                                       \
        filter = _mm256_load_si256(                                           \
            (__m256i const *)(bilinear_filters_avx2 + y_offset));             \
        pw8 = _mm256_set1_epi16(8);                                           \
        /* load source and another source starting from the next */           \
        /* following byte */                                                  \
        src_reg = _mm256_loadu_si256((__m256i const *)(src));                 \
        AVG_NEXT_SRC(src_reg, 1)                                              \
        for (i = 0; i < height; i++) {                                        \
          /* save current source average */                                   \
          src_avg = src_reg;                                                  \
          src += src_stride;                                                  \
          LOAD_SRC_DST                                                        \
          AVG_NEXT_SRC(src_reg, 1)                                            \
          MERGE_WITH_SRC(src_avg, src_reg)                                    \
          FILTER_SRC(filter)                                                  \
          src_avg = _mm256_packus_epi16(exp_src_lo, exp_src_hi);              \
          sec_reg = _mm256_loadu_si256((__m256i const *)(sec));               \
          src_avg = _mm256_avg_epu8(src_avg, sec_reg);                        \
          /* expand each byte to 2 bytes */                                   \
          MERGE_WITH_SRC(src_avg, zero_reg)                                   \
          sec += sec_stride;                                                  \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          dst += dst_stride;                                                  \
        }                                                                     \
      }                                                                       \
      /* x_offset = bilin interpolation and y_offset = 0 */                   \
    } else {                                                                  \
      if (y_offset == 0) {                                                    \
        __m256i filter, pw8, src_next_reg;                                    \
        x_offset <<= 5;                                                       \
        filter = _mm256_load_si256(                                           \
            (__m256i const *)(bilinear_filters_avx2 + x_offset));             \
        pw8 = _mm256_set1_epi16(8);                                           \
        for (i = 0; i < height; i++) {                                        \
          LOAD_SRC_DST                                                        \
          MERGE_NEXT_SRC(src_reg, 1)                                          \
          FILTER_SRC(filter)                                                  \
          src_reg = _mm256_packus_epi16(exp_src_lo, exp_src_hi);              \
          sec_reg = _mm256_loadu_si256((__m256i const *)(sec));               \
          src_reg = _mm256_avg_epu8(src_reg, sec_reg);                        \
          MERGE_WITH_SRC(src_reg, zero_reg)                                   \
          sec += sec_stride;                                                  \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          src += src_stride;                                                  \
          dst += dst_stride;                                                  \
        }                                                                     \
        /* x_offset = bilin interpolation and y_offset = 4 */                 \
      } else if (y_offset == 4) {                                             \
        __m256i filter, pw8, src_next_reg, src_pack;                          \
        x_offset <<= 5;                                                       \
        filter = _mm256_load_si256(                                           \
            (__m256i const *)(bilinear_filters_avx2 + x_offset));             \
        pw8 = _mm256_set1_epi16(8);                                           \
        src_reg = _mm256_loadu_si256((__m256i const *)(src));                 \
        MERGE_NEXT_SRC(src_reg, 1)                                            \
        FILTER_SRC(filter)                                                    \
        /* convert each 16 bit to 8 bit to each low and high lane source */   \
        src_pack = _mm256_packus_epi16(exp_src_lo, exp_src_hi);               \
        for (i = 0; i < height; i++) {                                        \
          src += src_stride;                                                  \
          LOAD_SRC_DST                                                        \
          MERGE_NEXT_SRC(src_reg, 1)                                          \
          FILTER_SRC(filter)                                                  \
          src_reg = _mm256_packus_epi16(exp_src_lo, exp_src_hi);              \
          /* average between previous pack to the current */                  \
          src_pack = _mm256_avg_epu8(src_pack, src_reg);                      \
          sec_reg = _mm256_loadu_si256((__m256i const *)(sec));               \
          src_pack = _mm256_avg_epu8(src_pack, sec_reg);                      \
          sec += sec_stride;                                                  \
          MERGE_WITH_SRC(src_pack, zero_reg)                                  \
          src_pack = src_reg;                                                 \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          dst += dst_stride;                                                  \
        }                                                                     \
        /* x_offset = bilin interpolation and y_offset = bilin interpolation  \
         */                                                                   \
      } else {                                                                \
        __m256i xfilter, yfilter, pw8, src_next_reg, src_pack;                \
        x_offset <<= 5;                                                       \
        xfilter = _mm256_load_si256(                                          \
            (__m256i const *)(bilinear_filters_avx2 + x_offset));             \
        y_offset <<= 5;                                                       \
        yfilter = _mm256_load_si256(                                          \
            (__m256i const *)(bilinear_filters_avx2 + y_offset));             \
        pw8 = _mm256_set1_epi16(8);                                           \
        /* load source and another source starting from the next */           \
        /* following byte */                                                  \
        src_reg = _mm256_loadu_si256((__m256i const *)(src));                 \
        MERGE_NEXT_SRC(src_reg, 1)                                            \
                                                                              \
        FILTER_SRC(xfilter)                                                   \
        /* convert each 16 bit to 8 bit to each low and high lane source */   \
        src_pack = _mm256_packus_epi16(exp_src_lo, exp_src_hi);               \
        for (i = 0; i < height; i++) {                                        \
          src += src_stride;                                                  \
          LOAD_SRC_DST                                                        \
          MERGE_NEXT_SRC(src_reg, 1)                                          \
          FILTER_SRC(xfilter)                                                 \
          src_reg = _mm256_packus_epi16(exp_src_lo, exp_src_hi);              \
          /* merge previous pack to current pack source */                    \
          MERGE_WITH_SRC(src_pack, src_reg)                                   \
          /* filter the source */                                             \
          FILTER_SRC(yfilter)                                                 \
          src_pack = _mm256_packus_epi16(exp_src_lo, exp_src_hi);             \
          sec_reg = _mm256_loadu_si256((__m256i const *)(sec));               \
          src_pack = _mm256_avg_epu8(src_pack, sec_reg);                      \
          MERGE_WITH_SRC(src_pack, zero_reg)                                  \
          src_pack = src_reg;                                                 \
          sec += sec_stride;                                                  \
          CALC_SUM_SSE_INSIDE_LOOP                                            \
          dst += dst_stride;                                                  \
        }                                                                     \
      }                                                                       \
    }                                                                         \
    CALC_SUM_AND_SSE                                                          \
    _mm256_zeroupper();                                                       \
    return sum;                                                               \
  }                                                                           \
  unsigned int aom_sub_pixel_avg_variance32x##height##_avx2(                  \
      const uint8_t *src, int src_stride, int x_offset, int y_offset,         \
      const uint8_t *dst, int dst_stride, unsigned int *sse,                  \
      const uint8_t *sec_ptr) {                                               \
    const int sum = sub_pixel_avg_variance32x##height##_imp_avx2(             \
        src, src_stride, x_offset, y_offset, dst, dst_stride, sec_ptr, 32,    \
        sse);                                                                 \
    return *sse - (unsigned int)(((int64_t)sum * sum) >> (5 + log2height));   \
  }

MAKE_SUB_PIXEL_AVG_VAR_32XH(64, 6)
MAKE_SUB_PIXEL_AVG_VAR_32XH(32, 5)
MAKE_SUB_PIXEL_AVG_VAR_32XH(16, 4)

#define AOM_SUB_PIXEL_AVG_VAR_AVX2(w, h, wf, hf, wlog2, hlog2)            \
  unsigned int aom_sub_pixel_avg_variance##w##x##h##_avx2(                \
      const uint8_t *src, int src_stride, int x_offset, int y_offset,     \
      const uint8_t *dst, int dst_stride, unsigned int *sse_ptr,          \
      const uint8_t *sec) {                                               \
    unsigned int sse = 0;                                                 \
    int se = 0;                                                           \
    for (int i = 0; i < (w / wf); ++i) {                                  \
      const uint8_t *src_ptr = src;                                       \
      const uint8_t *dst_ptr = dst;                                       \
      const uint8_t *sec_ptr = sec;                                       \
      for (int j = 0; j < (h / hf); ++j) {                                \
        unsigned int sse2;                                                \
        const int se2 = sub_pixel_avg_variance##wf##x##hf##_imp_avx2(     \
            src_ptr, src_stride, x_offset, y_offset, dst_ptr, dst_stride, \
            sec_ptr, w, &sse2);                                           \
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

// Note: hf = AOMMIN(h, 64) to avoid overflow in helper by capping height.
AOM_SUB_PIXEL_AVG_VAR_AVX2(128, 128, 32, 64, 7, 7)
AOM_SUB_PIXEL_AVG_VAR_AVX2(128, 64, 32, 64, 7, 6)
AOM_SUB_PIXEL_AVG_VAR_AVX2(64, 128, 32, 64, 6, 7)
AOM_SUB_PIXEL_AVG_VAR_AVX2(64, 64, 32, 64, 6, 6)
AOM_SUB_PIXEL_AVG_VAR_AVX2(64, 32, 32, 32, 6, 5)
