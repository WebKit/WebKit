/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <stdlib.h>
#include <string.h>
#include <tmmintrin.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/aom_filter.h"
#include "aom_dsp/blend.h"
#include "aom_dsp/x86/masked_variance_intrin_ssse3.h"
#include "aom_dsp/x86/synonyms.h"
#include "aom_ports/mem.h"

// For width a multiple of 16
static void bilinear_filter(const uint8_t *src, int src_stride, int xoffset,
                            int yoffset, uint8_t *dst, int w, int h);

static void bilinear_filter8xh(const uint8_t *src, int src_stride, int xoffset,
                               int yoffset, uint8_t *dst, int h);

static void bilinear_filter4xh(const uint8_t *src, int src_stride, int xoffset,
                               int yoffset, uint8_t *dst, int h);

// For width a multiple of 16
static void masked_variance(const uint8_t *src_ptr, int src_stride,
                            const uint8_t *a_ptr, int a_stride,
                            const uint8_t *b_ptr, int b_stride,
                            const uint8_t *m_ptr, int m_stride, int width,
                            int height, unsigned int *sse, int *sum_);

static void masked_variance8xh(const uint8_t *src_ptr, int src_stride,
                               const uint8_t *a_ptr, const uint8_t *b_ptr,
                               const uint8_t *m_ptr, int m_stride, int height,
                               unsigned int *sse, int *sum_);

static void masked_variance4xh(const uint8_t *src_ptr, int src_stride,
                               const uint8_t *a_ptr, const uint8_t *b_ptr,
                               const uint8_t *m_ptr, int m_stride, int height,
                               unsigned int *sse, int *sum_);

#define MASK_SUBPIX_VAR_SSSE3(W, H)                                   \
  unsigned int aom_masked_sub_pixel_variance##W##x##H##_ssse3(        \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,   \
      const uint8_t *ref, int ref_stride, const uint8_t *second_pred, \
      const uint8_t *msk, int msk_stride, int invert_mask,            \
      unsigned int *sse) {                                            \
    int sum;                                                          \
    uint8_t temp[(H + 1) * W];                                        \
                                                                      \
    bilinear_filter(src, src_stride, xoffset, yoffset, temp, W, H);   \
                                                                      \
    if (!invert_mask)                                                 \
      masked_variance(ref, ref_stride, temp, W, second_pred, W, msk,  \
                      msk_stride, W, H, sse, &sum);                   \
    else                                                              \
      masked_variance(ref, ref_stride, second_pred, W, temp, W, msk,  \
                      msk_stride, W, H, sse, &sum);                   \
    return *sse - (uint32_t)(((int64_t)sum * sum) / (W * H));         \
  }

#define MASK_SUBPIX_VAR8XH_SSSE3(H)                                           \
  unsigned int aom_masked_sub_pixel_variance8x##H##_ssse3(                    \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *ref, int ref_stride, const uint8_t *second_pred,         \
      const uint8_t *msk, int msk_stride, int invert_mask,                    \
      unsigned int *sse) {                                                    \
    int sum;                                                                  \
    uint8_t temp[(H + 1) * 8];                                                \
                                                                              \
    bilinear_filter8xh(src, src_stride, xoffset, yoffset, temp, H);           \
                                                                              \
    if (!invert_mask)                                                         \
      masked_variance8xh(ref, ref_stride, temp, second_pred, msk, msk_stride, \
                         H, sse, &sum);                                       \
    else                                                                      \
      masked_variance8xh(ref, ref_stride, second_pred, temp, msk, msk_stride, \
                         H, sse, &sum);                                       \
    return *sse - (uint32_t)(((int64_t)sum * sum) / (8 * H));                 \
  }

#define MASK_SUBPIX_VAR4XH_SSSE3(H)                                           \
  unsigned int aom_masked_sub_pixel_variance4x##H##_ssse3(                    \
      const uint8_t *src, int src_stride, int xoffset, int yoffset,           \
      const uint8_t *ref, int ref_stride, const uint8_t *second_pred,         \
      const uint8_t *msk, int msk_stride, int invert_mask,                    \
      unsigned int *sse) {                                                    \
    int sum;                                                                  \
    uint8_t temp[(H + 1) * 4];                                                \
                                                                              \
    bilinear_filter4xh(src, src_stride, xoffset, yoffset, temp, H);           \
                                                                              \
    if (!invert_mask)                                                         \
      masked_variance4xh(ref, ref_stride, temp, second_pred, msk, msk_stride, \
                         H, sse, &sum);                                       \
    else                                                                      \
      masked_variance4xh(ref, ref_stride, second_pred, temp, msk, msk_stride, \
                         H, sse, &sum);                                       \
    return *sse - (uint32_t)(((int64_t)sum * sum) / (4 * H));                 \
  }

MASK_SUBPIX_VAR_SSSE3(128, 128)
MASK_SUBPIX_VAR_SSSE3(128, 64)
MASK_SUBPIX_VAR_SSSE3(64, 128)
MASK_SUBPIX_VAR_SSSE3(64, 64)
MASK_SUBPIX_VAR_SSSE3(64, 32)
MASK_SUBPIX_VAR_SSSE3(32, 64)
MASK_SUBPIX_VAR_SSSE3(32, 32)
MASK_SUBPIX_VAR_SSSE3(32, 16)
MASK_SUBPIX_VAR_SSSE3(16, 32)
MASK_SUBPIX_VAR_SSSE3(16, 16)
MASK_SUBPIX_VAR_SSSE3(16, 8)
MASK_SUBPIX_VAR8XH_SSSE3(16)
MASK_SUBPIX_VAR8XH_SSSE3(8)
MASK_SUBPIX_VAR8XH_SSSE3(4)
MASK_SUBPIX_VAR4XH_SSSE3(8)
MASK_SUBPIX_VAR4XH_SSSE3(4)

#if !CONFIG_REALTIME_ONLY
MASK_SUBPIX_VAR4XH_SSSE3(16)
MASK_SUBPIX_VAR_SSSE3(16, 4)
MASK_SUBPIX_VAR8XH_SSSE3(32)
MASK_SUBPIX_VAR_SSSE3(32, 8)
MASK_SUBPIX_VAR_SSSE3(64, 16)
MASK_SUBPIX_VAR_SSSE3(16, 64)
#endif  // !CONFIG_REALTIME_ONLY

static INLINE __m128i filter_block(const __m128i a, const __m128i b,
                                   const __m128i filter) {
  __m128i v0 = _mm_unpacklo_epi8(a, b);
  v0 = _mm_maddubs_epi16(v0, filter);
  v0 = xx_roundn_epu16(v0, FILTER_BITS);

  __m128i v1 = _mm_unpackhi_epi8(a, b);
  v1 = _mm_maddubs_epi16(v1, filter);
  v1 = xx_roundn_epu16(v1, FILTER_BITS);

  return _mm_packus_epi16(v0, v1);
}

static void bilinear_filter(const uint8_t *src, int src_stride, int xoffset,
                            int yoffset, uint8_t *dst, int w, int h) {
  int i, j;
  // Horizontal filter
  if (xoffset == 0) {
    uint8_t *b = dst;
    for (i = 0; i < h + 1; ++i) {
      for (j = 0; j < w; j += 16) {
        __m128i x = _mm_loadu_si128((__m128i *)&src[j]);
        _mm_storeu_si128((__m128i *)&b[j], x);
      }
      src += src_stride;
      b += w;
    }
  } else if (xoffset == 4) {
    uint8_t *b = dst;
    for (i = 0; i < h + 1; ++i) {
      for (j = 0; j < w; j += 16) {
        __m128i x = _mm_loadu_si128((__m128i *)&src[j]);
        __m128i y = _mm_loadu_si128((__m128i *)&src[j + 16]);
        __m128i z = _mm_alignr_epi8(y, x, 1);
        _mm_storeu_si128((__m128i *)&b[j], _mm_avg_epu8(x, z));
      }
      src += src_stride;
      b += w;
    }
  } else {
    uint8_t *b = dst;
    const uint8_t *hfilter = bilinear_filters_2t[xoffset];
    const __m128i hfilter_vec = _mm_set1_epi16(hfilter[0] | (hfilter[1] << 8));
    for (i = 0; i < h + 1; ++i) {
      for (j = 0; j < w; j += 16) {
        const __m128i x = _mm_loadu_si128((__m128i *)&src[j]);
        const __m128i y = _mm_loadu_si128((__m128i *)&src[j + 16]);
        const __m128i z = _mm_alignr_epi8(y, x, 1);
        const __m128i res = filter_block(x, z, hfilter_vec);
        _mm_storeu_si128((__m128i *)&b[j], res);
      }

      src += src_stride;
      b += w;
    }
  }

  // Vertical filter
  if (yoffset == 0) {
    // The data is already in 'dst', so no need to filter
  } else if (yoffset == 4) {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 16) {
        __m128i x = _mm_loadu_si128((__m128i *)&dst[j]);
        __m128i y = _mm_loadu_si128((__m128i *)&dst[j + w]);
        _mm_storeu_si128((__m128i *)&dst[j], _mm_avg_epu8(x, y));
      }
      dst += w;
    }
  } else {
    const uint8_t *vfilter = bilinear_filters_2t[yoffset];
    const __m128i vfilter_vec = _mm_set1_epi16(vfilter[0] | (vfilter[1] << 8));
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 16) {
        const __m128i x = _mm_loadu_si128((__m128i *)&dst[j]);
        const __m128i y = _mm_loadu_si128((__m128i *)&dst[j + w]);
        const __m128i res = filter_block(x, y, vfilter_vec);
        _mm_storeu_si128((__m128i *)&dst[j], res);
      }

      dst += w;
    }
  }
}

static INLINE __m128i filter_block_2rows(const __m128i *a0, const __m128i *b0,
                                         const __m128i *a1, const __m128i *b1,
                                         const __m128i *filter) {
  __m128i v0 = _mm_unpacklo_epi8(*a0, *b0);
  v0 = _mm_maddubs_epi16(v0, *filter);
  v0 = xx_roundn_epu16(v0, FILTER_BITS);

  __m128i v1 = _mm_unpacklo_epi8(*a1, *b1);
  v1 = _mm_maddubs_epi16(v1, *filter);
  v1 = xx_roundn_epu16(v1, FILTER_BITS);

  return _mm_packus_epi16(v0, v1);
}

static void bilinear_filter8xh(const uint8_t *src, int src_stride, int xoffset,
                               int yoffset, uint8_t *dst, int h) {
  int i;
  // Horizontal filter
  if (xoffset == 0) {
    uint8_t *b = dst;
    for (i = 0; i < h + 1; ++i) {
      __m128i x = _mm_loadl_epi64((__m128i *)src);
      _mm_storel_epi64((__m128i *)b, x);
      src += src_stride;
      b += 8;
    }
  } else if (xoffset == 4) {
    uint8_t *b = dst;
    for (i = 0; i < h + 1; ++i) {
      __m128i x = _mm_loadu_si128((__m128i *)src);
      __m128i z = _mm_srli_si128(x, 1);
      _mm_storel_epi64((__m128i *)b, _mm_avg_epu8(x, z));
      src += src_stride;
      b += 8;
    }
  } else {
    uint8_t *b = dst;
    const uint8_t *hfilter = bilinear_filters_2t[xoffset];
    const __m128i hfilter_vec = _mm_set1_epi16(hfilter[0] | (hfilter[1] << 8));
    for (i = 0; i < h; i += 2) {
      const __m128i x0 = _mm_loadu_si128((__m128i *)src);
      const __m128i z0 = _mm_srli_si128(x0, 1);
      const __m128i x1 = _mm_loadu_si128((__m128i *)&src[src_stride]);
      const __m128i z1 = _mm_srli_si128(x1, 1);
      const __m128i res = filter_block_2rows(&x0, &z0, &x1, &z1, &hfilter_vec);
      _mm_storeu_si128((__m128i *)b, res);

      src += src_stride * 2;
      b += 16;
    }
    // Handle i = h separately
    const __m128i x0 = _mm_loadu_si128((__m128i *)src);
    const __m128i z0 = _mm_srli_si128(x0, 1);

    __m128i v0 = _mm_unpacklo_epi8(x0, z0);
    v0 = _mm_maddubs_epi16(v0, hfilter_vec);
    v0 = xx_roundn_epu16(v0, FILTER_BITS);

    _mm_storel_epi64((__m128i *)b, _mm_packus_epi16(v0, v0));
  }

  // Vertical filter
  if (yoffset == 0) {
    // The data is already in 'dst', so no need to filter
  } else if (yoffset == 4) {
    for (i = 0; i < h; ++i) {
      __m128i x = _mm_loadl_epi64((__m128i *)dst);
      __m128i y = _mm_loadl_epi64((__m128i *)&dst[8]);
      _mm_storel_epi64((__m128i *)dst, _mm_avg_epu8(x, y));
      dst += 8;
    }
  } else {
    const uint8_t *vfilter = bilinear_filters_2t[yoffset];
    const __m128i vfilter_vec = _mm_set1_epi16(vfilter[0] | (vfilter[1] << 8));
    for (i = 0; i < h; i += 2) {
      const __m128i x = _mm_loadl_epi64((__m128i *)dst);
      const __m128i y = _mm_loadl_epi64((__m128i *)&dst[8]);
      const __m128i z = _mm_loadl_epi64((__m128i *)&dst[16]);
      const __m128i res = filter_block_2rows(&x, &y, &y, &z, &vfilter_vec);
      _mm_storeu_si128((__m128i *)dst, res);

      dst += 16;
    }
  }
}

static void bilinear_filter4xh(const uint8_t *src, int src_stride, int xoffset,
                               int yoffset, uint8_t *dst, int h) {
  int i;
  // Horizontal filter
  if (xoffset == 0) {
    uint8_t *b = dst;
    for (i = 0; i < h + 1; ++i) {
      __m128i x = xx_loadl_32((__m128i *)src);
      xx_storel_32(b, x);
      src += src_stride;
      b += 4;
    }
  } else if (xoffset == 4) {
    uint8_t *b = dst;
    for (i = 0; i < h + 1; ++i) {
      __m128i x = _mm_loadl_epi64((__m128i *)src);
      __m128i z = _mm_srli_si128(x, 1);
      xx_storel_32(b, _mm_avg_epu8(x, z));
      src += src_stride;
      b += 4;
    }
  } else {
    uint8_t *b = dst;
    const uint8_t *hfilter = bilinear_filters_2t[xoffset];
    const __m128i hfilter_vec = _mm_set1_epi16(hfilter[0] | (hfilter[1] << 8));
    for (i = 0; i < h; i += 4) {
      const __m128i x0 = _mm_loadl_epi64((__m128i *)src);
      const __m128i z0 = _mm_srli_si128(x0, 1);
      const __m128i x1 = _mm_loadl_epi64((__m128i *)&src[src_stride]);
      const __m128i z1 = _mm_srli_si128(x1, 1);
      const __m128i x2 = _mm_loadl_epi64((__m128i *)&src[src_stride * 2]);
      const __m128i z2 = _mm_srli_si128(x2, 1);
      const __m128i x3 = _mm_loadl_epi64((__m128i *)&src[src_stride * 3]);
      const __m128i z3 = _mm_srli_si128(x3, 1);

      const __m128i a0 = _mm_unpacklo_epi32(x0, x1);
      const __m128i b0 = _mm_unpacklo_epi32(z0, z1);
      const __m128i a1 = _mm_unpacklo_epi32(x2, x3);
      const __m128i b1 = _mm_unpacklo_epi32(z2, z3);
      const __m128i res = filter_block_2rows(&a0, &b0, &a1, &b1, &hfilter_vec);
      _mm_storeu_si128((__m128i *)b, res);

      src += src_stride * 4;
      b += 16;
    }
    // Handle i = h separately
    const __m128i x = _mm_loadl_epi64((__m128i *)src);
    const __m128i z = _mm_srli_si128(x, 1);

    __m128i v0 = _mm_unpacklo_epi8(x, z);
    v0 = _mm_maddubs_epi16(v0, hfilter_vec);
    v0 = xx_roundn_epu16(v0, FILTER_BITS);

    xx_storel_32(b, _mm_packus_epi16(v0, v0));
  }

  // Vertical filter
  if (yoffset == 0) {
    // The data is already in 'dst', so no need to filter
  } else if (yoffset == 4) {
    for (i = 0; i < h; ++i) {
      __m128i x = xx_loadl_32((__m128i *)dst);
      __m128i y = xx_loadl_32((__m128i *)&dst[4]);
      xx_storel_32(dst, _mm_avg_epu8(x, y));
      dst += 4;
    }
  } else {
    const uint8_t *vfilter = bilinear_filters_2t[yoffset];
    const __m128i vfilter_vec = _mm_set1_epi16(vfilter[0] | (vfilter[1] << 8));
    for (i = 0; i < h; i += 4) {
      const __m128i a = xx_loadl_32((__m128i *)dst);
      const __m128i b = xx_loadl_32((__m128i *)&dst[4]);
      const __m128i c = xx_loadl_32((__m128i *)&dst[8]);
      const __m128i d = xx_loadl_32((__m128i *)&dst[12]);
      const __m128i e = xx_loadl_32((__m128i *)&dst[16]);

      const __m128i a0 = _mm_unpacklo_epi32(a, b);
      const __m128i b0 = _mm_unpacklo_epi32(b, c);
      const __m128i a1 = _mm_unpacklo_epi32(c, d);
      const __m128i b1 = _mm_unpacklo_epi32(d, e);
      const __m128i res = filter_block_2rows(&a0, &b0, &a1, &b1, &vfilter_vec);
      _mm_storeu_si128((__m128i *)dst, res);

      dst += 16;
    }
  }
}

static INLINE void accumulate_block(const __m128i *src, const __m128i *a,
                                    const __m128i *b, const __m128i *m,
                                    __m128i *sum, __m128i *sum_sq) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i one = _mm_set1_epi16(1);
  const __m128i mask_max = _mm_set1_epi8((1 << AOM_BLEND_A64_ROUND_BITS));
  const __m128i m_inv = _mm_sub_epi8(mask_max, *m);

  // Calculate 16 predicted pixels.
  // Note that the maximum value of any entry of 'pred_l' or 'pred_r'
  // is 64 * 255, so we have plenty of space to add rounding constants.
  const __m128i data_l = _mm_unpacklo_epi8(*a, *b);
  const __m128i mask_l = _mm_unpacklo_epi8(*m, m_inv);
  __m128i pred_l = _mm_maddubs_epi16(data_l, mask_l);
  pred_l = xx_roundn_epu16(pred_l, AOM_BLEND_A64_ROUND_BITS);

  const __m128i data_r = _mm_unpackhi_epi8(*a, *b);
  const __m128i mask_r = _mm_unpackhi_epi8(*m, m_inv);
  __m128i pred_r = _mm_maddubs_epi16(data_r, mask_r);
  pred_r = xx_roundn_epu16(pred_r, AOM_BLEND_A64_ROUND_BITS);

  const __m128i src_l = _mm_unpacklo_epi8(*src, zero);
  const __m128i src_r = _mm_unpackhi_epi8(*src, zero);
  const __m128i diff_l = _mm_sub_epi16(pred_l, src_l);
  const __m128i diff_r = _mm_sub_epi16(pred_r, src_r);

  // Update partial sums and partial sums of squares
  *sum =
      _mm_add_epi32(*sum, _mm_madd_epi16(_mm_add_epi16(diff_l, diff_r), one));
  *sum_sq =
      _mm_add_epi32(*sum_sq, _mm_add_epi32(_mm_madd_epi16(diff_l, diff_l),
                                           _mm_madd_epi16(diff_r, diff_r)));
}

static void masked_variance(const uint8_t *src_ptr, int src_stride,
                            const uint8_t *a_ptr, int a_stride,
                            const uint8_t *b_ptr, int b_stride,
                            const uint8_t *m_ptr, int m_stride, int width,
                            int height, unsigned int *sse, int *sum_) {
  int x, y;
  __m128i sum = _mm_setzero_si128(), sum_sq = _mm_setzero_si128();

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x += 16) {
      const __m128i src = _mm_loadu_si128((const __m128i *)&src_ptr[x]);
      const __m128i a = _mm_loadu_si128((const __m128i *)&a_ptr[x]);
      const __m128i b = _mm_loadu_si128((const __m128i *)&b_ptr[x]);
      const __m128i m = _mm_loadu_si128((const __m128i *)&m_ptr[x]);
      accumulate_block(&src, &a, &b, &m, &sum, &sum_sq);
    }

    src_ptr += src_stride;
    a_ptr += a_stride;
    b_ptr += b_stride;
    m_ptr += m_stride;
  }
  // Reduce down to a single sum and sum of squares
  sum = _mm_hadd_epi32(sum, sum_sq);
  sum = _mm_hadd_epi32(sum, sum);
  *sum_ = _mm_cvtsi128_si32(sum);
  *sse = (unsigned int)_mm_cvtsi128_si32(_mm_srli_si128(sum, 4));
}

static void masked_variance8xh(const uint8_t *src_ptr, int src_stride,
                               const uint8_t *a_ptr, const uint8_t *b_ptr,
                               const uint8_t *m_ptr, int m_stride, int height,
                               unsigned int *sse, int *sum_) {
  int y;
  __m128i sum = _mm_setzero_si128(), sum_sq = _mm_setzero_si128();

  for (y = 0; y < height; y += 2) {
    __m128i src = _mm_unpacklo_epi64(
        _mm_loadl_epi64((const __m128i *)src_ptr),
        _mm_loadl_epi64((const __m128i *)&src_ptr[src_stride]));
    const __m128i a = _mm_loadu_si128((const __m128i *)a_ptr);
    const __m128i b = _mm_loadu_si128((const __m128i *)b_ptr);
    const __m128i m =
        _mm_unpacklo_epi64(_mm_loadl_epi64((const __m128i *)m_ptr),
                           _mm_loadl_epi64((const __m128i *)&m_ptr[m_stride]));
    accumulate_block(&src, &a, &b, &m, &sum, &sum_sq);

    src_ptr += src_stride * 2;
    a_ptr += 16;
    b_ptr += 16;
    m_ptr += m_stride * 2;
  }
  // Reduce down to a single sum and sum of squares
  sum = _mm_hadd_epi32(sum, sum_sq);
  sum = _mm_hadd_epi32(sum, sum);
  *sum_ = _mm_cvtsi128_si32(sum);
  *sse = (unsigned int)_mm_cvtsi128_si32(_mm_srli_si128(sum, 4));
}

static void masked_variance4xh(const uint8_t *src_ptr, int src_stride,
                               const uint8_t *a_ptr, const uint8_t *b_ptr,
                               const uint8_t *m_ptr, int m_stride, int height,
                               unsigned int *sse, int *sum_) {
  int y;
  __m128i sum = _mm_setzero_si128(), sum_sq = _mm_setzero_si128();

  for (y = 0; y < height; y += 4) {
    // Load four rows at a time
    __m128i src = _mm_setr_epi32(*(int *)src_ptr, *(int *)&src_ptr[src_stride],
                                 *(int *)&src_ptr[src_stride * 2],
                                 *(int *)&src_ptr[src_stride * 3]);
    const __m128i a = _mm_loadu_si128((const __m128i *)a_ptr);
    const __m128i b = _mm_loadu_si128((const __m128i *)b_ptr);
    const __m128i m = _mm_setr_epi32(*(int *)m_ptr, *(int *)&m_ptr[m_stride],
                                     *(int *)&m_ptr[m_stride * 2],
                                     *(int *)&m_ptr[m_stride * 3]);
    accumulate_block(&src, &a, &b, &m, &sum, &sum_sq);

    src_ptr += src_stride * 4;
    a_ptr += 16;
    b_ptr += 16;
    m_ptr += m_stride * 4;
  }
  // Reduce down to a single sum and sum of squares
  sum = _mm_hadd_epi32(sum, sum_sq);
  sum = _mm_hadd_epi32(sum, sum);
  *sum_ = _mm_cvtsi128_si32(sum);
  *sse = (unsigned int)_mm_cvtsi128_si32(_mm_srli_si128(sum, 4));
}

#if CONFIG_AV1_HIGHBITDEPTH
// For width a multiple of 8
static void highbd_bilinear_filter(const uint16_t *src, int src_stride,
                                   int xoffset, int yoffset, uint16_t *dst,
                                   int w, int h);

static void highbd_bilinear_filter4xh(const uint16_t *src, int src_stride,
                                      int xoffset, int yoffset, uint16_t *dst,
                                      int h);

// For width a multiple of 8
static void highbd_masked_variance(const uint16_t *src_ptr, int src_stride,
                                   const uint16_t *a_ptr, int a_stride,
                                   const uint16_t *b_ptr, int b_stride,
                                   const uint8_t *m_ptr, int m_stride,
                                   int width, int height, uint64_t *sse,
                                   int *sum_);

static void highbd_masked_variance4xh(const uint16_t *src_ptr, int src_stride,
                                      const uint16_t *a_ptr,
                                      const uint16_t *b_ptr,
                                      const uint8_t *m_ptr, int m_stride,
                                      int height, int *sse, int *sum_);

#define HIGHBD_MASK_SUBPIX_VAR_SSSE3(W, H)                                  \
  unsigned int aom_highbd_8_masked_sub_pixel_variance##W##x##H##_ssse3(     \
      const uint8_t *src8, int src_stride, int xoffset, int yoffset,        \
      const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8,     \
      const uint8_t *msk, int msk_stride, int invert_mask, uint32_t *sse) { \
    uint64_t sse64;                                                         \
    int sum;                                                                \
    uint16_t temp[(H + 1) * W];                                             \
    const uint16_t *src = CONVERT_TO_SHORTPTR(src8);                        \
    const uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);                        \
    const uint16_t *second_pred = CONVERT_TO_SHORTPTR(second_pred8);        \
                                                                            \
    highbd_bilinear_filter(src, src_stride, xoffset, yoffset, temp, W, H);  \
                                                                            \
    if (!invert_mask)                                                       \
      highbd_masked_variance(ref, ref_stride, temp, W, second_pred, W, msk, \
                             msk_stride, W, H, &sse64, &sum);               \
    else                                                                    \
      highbd_masked_variance(ref, ref_stride, second_pred, W, temp, W, msk, \
                             msk_stride, W, H, &sse64, &sum);               \
    *sse = (uint32_t)sse64;                                                 \
    return *sse - (uint32_t)(((int64_t)sum * sum) / (W * H));               \
  }                                                                         \
  unsigned int aom_highbd_10_masked_sub_pixel_variance##W##x##H##_ssse3(    \
      const uint8_t *src8, int src_stride, int xoffset, int yoffset,        \
      const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8,     \
      const uint8_t *msk, int msk_stride, int invert_mask, uint32_t *sse) { \
    uint64_t sse64;                                                         \
    int sum;                                                                \
    int64_t var;                                                            \
    uint16_t temp[(H + 1) * W];                                             \
    const uint16_t *src = CONVERT_TO_SHORTPTR(src8);                        \
    const uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);                        \
    const uint16_t *second_pred = CONVERT_TO_SHORTPTR(second_pred8);        \
                                                                            \
    highbd_bilinear_filter(src, src_stride, xoffset, yoffset, temp, W, H);  \
                                                                            \
    if (!invert_mask)                                                       \
      highbd_masked_variance(ref, ref_stride, temp, W, second_pred, W, msk, \
                             msk_stride, W, H, &sse64, &sum);               \
    else                                                                    \
      highbd_masked_variance(ref, ref_stride, second_pred, W, temp, W, msk, \
                             msk_stride, W, H, &sse64, &sum);               \
    *sse = (uint32_t)ROUND_POWER_OF_TWO(sse64, 4);                          \
    sum = ROUND_POWER_OF_TWO(sum, 2);                                       \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) / (W * H));               \
    return (var >= 0) ? (uint32_t)var : 0;                                  \
  }                                                                         \
  unsigned int aom_highbd_12_masked_sub_pixel_variance##W##x##H##_ssse3(    \
      const uint8_t *src8, int src_stride, int xoffset, int yoffset,        \
      const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8,     \
      const uint8_t *msk, int msk_stride, int invert_mask, uint32_t *sse) { \
    uint64_t sse64;                                                         \
    int sum;                                                                \
    int64_t var;                                                            \
    uint16_t temp[(H + 1) * W];                                             \
    const uint16_t *src = CONVERT_TO_SHORTPTR(src8);                        \
    const uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);                        \
    const uint16_t *second_pred = CONVERT_TO_SHORTPTR(second_pred8);        \
                                                                            \
    highbd_bilinear_filter(src, src_stride, xoffset, yoffset, temp, W, H);  \
                                                                            \
    if (!invert_mask)                                                       \
      highbd_masked_variance(ref, ref_stride, temp, W, second_pred, W, msk, \
                             msk_stride, W, H, &sse64, &sum);               \
    else                                                                    \
      highbd_masked_variance(ref, ref_stride, second_pred, W, temp, W, msk, \
                             msk_stride, W, H, &sse64, &sum);               \
    *sse = (uint32_t)ROUND_POWER_OF_TWO(sse64, 8);                          \
    sum = ROUND_POWER_OF_TWO(sum, 4);                                       \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) / (W * H));               \
    return (var >= 0) ? (uint32_t)var : 0;                                  \
  }

#define HIGHBD_MASK_SUBPIX_VAR4XH_SSSE3(H)                                  \
  unsigned int aom_highbd_8_masked_sub_pixel_variance4x##H##_ssse3(         \
      const uint8_t *src8, int src_stride, int xoffset, int yoffset,        \
      const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8,     \
      const uint8_t *msk, int msk_stride, int invert_mask, uint32_t *sse) { \
    int sse_;                                                               \
    int sum;                                                                \
    uint16_t temp[(H + 1) * 4];                                             \
    const uint16_t *src = CONVERT_TO_SHORTPTR(src8);                        \
    const uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);                        \
    const uint16_t *second_pred = CONVERT_TO_SHORTPTR(second_pred8);        \
                                                                            \
    highbd_bilinear_filter4xh(src, src_stride, xoffset, yoffset, temp, H);  \
                                                                            \
    if (!invert_mask)                                                       \
      highbd_masked_variance4xh(ref, ref_stride, temp, second_pred, msk,    \
                                msk_stride, H, &sse_, &sum);                \
    else                                                                    \
      highbd_masked_variance4xh(ref, ref_stride, second_pred, temp, msk,    \
                                msk_stride, H, &sse_, &sum);                \
    *sse = (uint32_t)sse_;                                                  \
    return *sse - (uint32_t)(((int64_t)sum * sum) / (4 * H));               \
  }                                                                         \
  unsigned int aom_highbd_10_masked_sub_pixel_variance4x##H##_ssse3(        \
      const uint8_t *src8, int src_stride, int xoffset, int yoffset,        \
      const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8,     \
      const uint8_t *msk, int msk_stride, int invert_mask, uint32_t *sse) { \
    int sse_;                                                               \
    int sum;                                                                \
    int64_t var;                                                            \
    uint16_t temp[(H + 1) * 4];                                             \
    const uint16_t *src = CONVERT_TO_SHORTPTR(src8);                        \
    const uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);                        \
    const uint16_t *second_pred = CONVERT_TO_SHORTPTR(second_pred8);        \
                                                                            \
    highbd_bilinear_filter4xh(src, src_stride, xoffset, yoffset, temp, H);  \
                                                                            \
    if (!invert_mask)                                                       \
      highbd_masked_variance4xh(ref, ref_stride, temp, second_pred, msk,    \
                                msk_stride, H, &sse_, &sum);                \
    else                                                                    \
      highbd_masked_variance4xh(ref, ref_stride, second_pred, temp, msk,    \
                                msk_stride, H, &sse_, &sum);                \
    *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_, 4);                           \
    sum = ROUND_POWER_OF_TWO(sum, 2);                                       \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) / (4 * H));               \
    return (var >= 0) ? (uint32_t)var : 0;                                  \
  }                                                                         \
  unsigned int aom_highbd_12_masked_sub_pixel_variance4x##H##_ssse3(        \
      const uint8_t *src8, int src_stride, int xoffset, int yoffset,        \
      const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8,     \
      const uint8_t *msk, int msk_stride, int invert_mask, uint32_t *sse) { \
    int sse_;                                                               \
    int sum;                                                                \
    int64_t var;                                                            \
    uint16_t temp[(H + 1) * 4];                                             \
    const uint16_t *src = CONVERT_TO_SHORTPTR(src8);                        \
    const uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);                        \
    const uint16_t *second_pred = CONVERT_TO_SHORTPTR(second_pred8);        \
                                                                            \
    highbd_bilinear_filter4xh(src, src_stride, xoffset, yoffset, temp, H);  \
                                                                            \
    if (!invert_mask)                                                       \
      highbd_masked_variance4xh(ref, ref_stride, temp, second_pred, msk,    \
                                msk_stride, H, &sse_, &sum);                \
    else                                                                    \
      highbd_masked_variance4xh(ref, ref_stride, second_pred, temp, msk,    \
                                msk_stride, H, &sse_, &sum);                \
    *sse = (uint32_t)ROUND_POWER_OF_TWO(sse_, 8);                           \
    sum = ROUND_POWER_OF_TWO(sum, 4);                                       \
    var = (int64_t)(*sse) - (((int64_t)sum * sum) / (4 * H));               \
    return (var >= 0) ? (uint32_t)var : 0;                                  \
  }

HIGHBD_MASK_SUBPIX_VAR_SSSE3(128, 128)
HIGHBD_MASK_SUBPIX_VAR_SSSE3(128, 64)
HIGHBD_MASK_SUBPIX_VAR_SSSE3(64, 128)
HIGHBD_MASK_SUBPIX_VAR_SSSE3(64, 64)
HIGHBD_MASK_SUBPIX_VAR_SSSE3(64, 32)
HIGHBD_MASK_SUBPIX_VAR_SSSE3(32, 64)
HIGHBD_MASK_SUBPIX_VAR_SSSE3(32, 32)
HIGHBD_MASK_SUBPIX_VAR_SSSE3(32, 16)
HIGHBD_MASK_SUBPIX_VAR_SSSE3(16, 32)
HIGHBD_MASK_SUBPIX_VAR_SSSE3(16, 16)
HIGHBD_MASK_SUBPIX_VAR_SSSE3(16, 8)
HIGHBD_MASK_SUBPIX_VAR_SSSE3(8, 16)
HIGHBD_MASK_SUBPIX_VAR_SSSE3(8, 8)
HIGHBD_MASK_SUBPIX_VAR_SSSE3(8, 4)
HIGHBD_MASK_SUBPIX_VAR4XH_SSSE3(8)
HIGHBD_MASK_SUBPIX_VAR4XH_SSSE3(4)

#if !CONFIG_REALTIME_ONLY
HIGHBD_MASK_SUBPIX_VAR4XH_SSSE3(16)
HIGHBD_MASK_SUBPIX_VAR_SSSE3(16, 4)
HIGHBD_MASK_SUBPIX_VAR_SSSE3(8, 32)
HIGHBD_MASK_SUBPIX_VAR_SSSE3(32, 8)
HIGHBD_MASK_SUBPIX_VAR_SSSE3(16, 64)
HIGHBD_MASK_SUBPIX_VAR_SSSE3(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

static INLINE __m128i highbd_filter_block(const __m128i a, const __m128i b,
                                          const __m128i filter) {
  __m128i v0 = _mm_unpacklo_epi16(a, b);
  v0 = _mm_madd_epi16(v0, filter);
  v0 = xx_roundn_epu32(v0, FILTER_BITS);

  __m128i v1 = _mm_unpackhi_epi16(a, b);
  v1 = _mm_madd_epi16(v1, filter);
  v1 = xx_roundn_epu32(v1, FILTER_BITS);

  return _mm_packs_epi32(v0, v1);
}

static void highbd_bilinear_filter(const uint16_t *src, int src_stride,
                                   int xoffset, int yoffset, uint16_t *dst,
                                   int w, int h) {
  int i, j;
  // Horizontal filter
  if (xoffset == 0) {
    uint16_t *b = dst;
    for (i = 0; i < h + 1; ++i) {
      for (j = 0; j < w; j += 8) {
        __m128i x = _mm_loadu_si128((__m128i *)&src[j]);
        _mm_storeu_si128((__m128i *)&b[j], x);
      }
      src += src_stride;
      b += w;
    }
  } else if (xoffset == 4) {
    uint16_t *b = dst;
    for (i = 0; i < h + 1; ++i) {
      for (j = 0; j < w; j += 8) {
        __m128i x = _mm_loadu_si128((__m128i *)&src[j]);
        __m128i y = _mm_loadu_si128((__m128i *)&src[j + 8]);
        __m128i z = _mm_alignr_epi8(y, x, 2);
        _mm_storeu_si128((__m128i *)&b[j], _mm_avg_epu16(x, z));
      }
      src += src_stride;
      b += w;
    }
  } else {
    uint16_t *b = dst;
    const uint8_t *hfilter = bilinear_filters_2t[xoffset];
    const __m128i hfilter_vec = _mm_set1_epi32(hfilter[0] | (hfilter[1] << 16));
    for (i = 0; i < h + 1; ++i) {
      for (j = 0; j < w; j += 8) {
        const __m128i x = _mm_loadu_si128((__m128i *)&src[j]);
        const __m128i y = _mm_loadu_si128((__m128i *)&src[j + 8]);
        const __m128i z = _mm_alignr_epi8(y, x, 2);
        const __m128i res = highbd_filter_block(x, z, hfilter_vec);
        _mm_storeu_si128((__m128i *)&b[j], res);
      }

      src += src_stride;
      b += w;
    }
  }

  // Vertical filter
  if (yoffset == 0) {
    // The data is already in 'dst', so no need to filter
  } else if (yoffset == 4) {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 8) {
        __m128i x = _mm_loadu_si128((__m128i *)&dst[j]);
        __m128i y = _mm_loadu_si128((__m128i *)&dst[j + w]);
        _mm_storeu_si128((__m128i *)&dst[j], _mm_avg_epu16(x, y));
      }
      dst += w;
    }
  } else {
    const uint8_t *vfilter = bilinear_filters_2t[yoffset];
    const __m128i vfilter_vec = _mm_set1_epi32(vfilter[0] | (vfilter[1] << 16));
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 8) {
        const __m128i x = _mm_loadu_si128((__m128i *)&dst[j]);
        const __m128i y = _mm_loadu_si128((__m128i *)&dst[j + w]);
        const __m128i res = highbd_filter_block(x, y, vfilter_vec);
        _mm_storeu_si128((__m128i *)&dst[j], res);
      }

      dst += w;
    }
  }
}

static INLINE __m128i highbd_filter_block_2rows(const __m128i *a0,
                                                const __m128i *b0,
                                                const __m128i *a1,
                                                const __m128i *b1,
                                                const __m128i *filter) {
  __m128i v0 = _mm_unpacklo_epi16(*a0, *b0);
  v0 = _mm_madd_epi16(v0, *filter);
  v0 = xx_roundn_epu32(v0, FILTER_BITS);

  __m128i v1 = _mm_unpacklo_epi16(*a1, *b1);
  v1 = _mm_madd_epi16(v1, *filter);
  v1 = xx_roundn_epu32(v1, FILTER_BITS);

  return _mm_packs_epi32(v0, v1);
}

static void highbd_bilinear_filter4xh(const uint16_t *src, int src_stride,
                                      int xoffset, int yoffset, uint16_t *dst,
                                      int h) {
  int i;
  // Horizontal filter
  if (xoffset == 0) {
    uint16_t *b = dst;
    for (i = 0; i < h + 1; ++i) {
      __m128i x = _mm_loadl_epi64((__m128i *)src);
      _mm_storel_epi64((__m128i *)b, x);
      src += src_stride;
      b += 4;
    }
  } else if (xoffset == 4) {
    uint16_t *b = dst;
    for (i = 0; i < h + 1; ++i) {
      __m128i x = _mm_loadu_si128((__m128i *)src);
      __m128i z = _mm_srli_si128(x, 2);
      _mm_storel_epi64((__m128i *)b, _mm_avg_epu16(x, z));
      src += src_stride;
      b += 4;
    }
  } else {
    uint16_t *b = dst;
    const uint8_t *hfilter = bilinear_filters_2t[xoffset];
    const __m128i hfilter_vec = _mm_set1_epi32(hfilter[0] | (hfilter[1] << 16));
    for (i = 0; i < h; i += 2) {
      const __m128i x0 = _mm_loadu_si128((__m128i *)src);
      const __m128i z0 = _mm_srli_si128(x0, 2);
      const __m128i x1 = _mm_loadu_si128((__m128i *)&src[src_stride]);
      const __m128i z1 = _mm_srli_si128(x1, 2);
      const __m128i res =
          highbd_filter_block_2rows(&x0, &z0, &x1, &z1, &hfilter_vec);
      _mm_storeu_si128((__m128i *)b, res);

      src += src_stride * 2;
      b += 8;
    }
    // Process i = h separately
    __m128i x = _mm_loadu_si128((__m128i *)src);
    __m128i z = _mm_srli_si128(x, 2);

    __m128i v0 = _mm_unpacklo_epi16(x, z);
    v0 = _mm_madd_epi16(v0, hfilter_vec);
    v0 = xx_roundn_epu32(v0, FILTER_BITS);

    _mm_storel_epi64((__m128i *)b, _mm_packs_epi32(v0, v0));
  }

  // Vertical filter
  if (yoffset == 0) {
    // The data is already in 'dst', so no need to filter
  } else if (yoffset == 4) {
    for (i = 0; i < h; ++i) {
      __m128i x = _mm_loadl_epi64((__m128i *)dst);
      __m128i y = _mm_loadl_epi64((__m128i *)&dst[4]);
      _mm_storel_epi64((__m128i *)dst, _mm_avg_epu16(x, y));
      dst += 4;
    }
  } else {
    const uint8_t *vfilter = bilinear_filters_2t[yoffset];
    const __m128i vfilter_vec = _mm_set1_epi32(vfilter[0] | (vfilter[1] << 16));
    for (i = 0; i < h; i += 2) {
      const __m128i x = _mm_loadl_epi64((__m128i *)dst);
      const __m128i y = _mm_loadl_epi64((__m128i *)&dst[4]);
      const __m128i z = _mm_loadl_epi64((__m128i *)&dst[8]);
      const __m128i res =
          highbd_filter_block_2rows(&x, &y, &y, &z, &vfilter_vec);
      _mm_storeu_si128((__m128i *)dst, res);

      dst += 8;
    }
  }
}

static void highbd_masked_variance(const uint16_t *src_ptr, int src_stride,
                                   const uint16_t *a_ptr, int a_stride,
                                   const uint16_t *b_ptr, int b_stride,
                                   const uint8_t *m_ptr, int m_stride,
                                   int width, int height, uint64_t *sse,
                                   int *sum_) {
  int x, y;
  // Note on bit widths:
  // The maximum value of 'sum' is (2^12 - 1) * 128 * 128 =~ 2^26,
  // so this can be kept as four 32-bit values.
  // But the maximum value of 'sum_sq' is (2^12 - 1)^2 * 128 * 128 =~ 2^38,
  // so this must be stored as two 64-bit values.
  __m128i sum = _mm_setzero_si128(), sum_sq = _mm_setzero_si128();
  const __m128i mask_max = _mm_set1_epi16((1 << AOM_BLEND_A64_ROUND_BITS));
  const __m128i round_const =
      _mm_set1_epi32((1 << AOM_BLEND_A64_ROUND_BITS) >> 1);
  const __m128i zero = _mm_setzero_si128();

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x += 8) {
      const __m128i src = _mm_loadu_si128((const __m128i *)&src_ptr[x]);
      const __m128i a = _mm_loadu_si128((const __m128i *)&a_ptr[x]);
      const __m128i b = _mm_loadu_si128((const __m128i *)&b_ptr[x]);
      const __m128i m =
          _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&m_ptr[x]), zero);
      const __m128i m_inv = _mm_sub_epi16(mask_max, m);

      // Calculate 8 predicted pixels.
      const __m128i data_l = _mm_unpacklo_epi16(a, b);
      const __m128i mask_l = _mm_unpacklo_epi16(m, m_inv);
      __m128i pred_l = _mm_madd_epi16(data_l, mask_l);
      pred_l = _mm_srai_epi32(_mm_add_epi32(pred_l, round_const),
                              AOM_BLEND_A64_ROUND_BITS);

      const __m128i data_r = _mm_unpackhi_epi16(a, b);
      const __m128i mask_r = _mm_unpackhi_epi16(m, m_inv);
      __m128i pred_r = _mm_madd_epi16(data_r, mask_r);
      pred_r = _mm_srai_epi32(_mm_add_epi32(pred_r, round_const),
                              AOM_BLEND_A64_ROUND_BITS);

      const __m128i src_l = _mm_unpacklo_epi16(src, zero);
      const __m128i src_r = _mm_unpackhi_epi16(src, zero);
      __m128i diff_l = _mm_sub_epi32(pred_l, src_l);
      __m128i diff_r = _mm_sub_epi32(pred_r, src_r);

      // Update partial sums and partial sums of squares
      sum = _mm_add_epi32(sum, _mm_add_epi32(diff_l, diff_r));
      // A trick: Now each entry of diff_l and diff_r is stored in a 32-bit
      // field, but the range of values is only [-(2^12 - 1), 2^12 - 1].
      // So we can re-pack into 16-bit fields and use _mm_madd_epi16
      // to calculate the squares and partially sum them.
      const __m128i tmp = _mm_packs_epi32(diff_l, diff_r);
      const __m128i prod = _mm_madd_epi16(tmp, tmp);
      // Then we want to sign-extend to 64 bits and accumulate
      const __m128i sign = _mm_srai_epi32(prod, 31);
      const __m128i tmp_0 = _mm_unpacklo_epi32(prod, sign);
      const __m128i tmp_1 = _mm_unpackhi_epi32(prod, sign);
      sum_sq = _mm_add_epi64(sum_sq, _mm_add_epi64(tmp_0, tmp_1));
    }

    src_ptr += src_stride;
    a_ptr += a_stride;
    b_ptr += b_stride;
    m_ptr += m_stride;
  }
  // Reduce down to a single sum and sum of squares
  sum = _mm_hadd_epi32(sum, zero);
  sum = _mm_hadd_epi32(sum, zero);
  *sum_ = _mm_cvtsi128_si32(sum);
  sum_sq = _mm_add_epi64(sum_sq, _mm_srli_si128(sum_sq, 8));
  _mm_storel_epi64((__m128i *)sse, sum_sq);
}

static void highbd_masked_variance4xh(const uint16_t *src_ptr, int src_stride,
                                      const uint16_t *a_ptr,
                                      const uint16_t *b_ptr,
                                      const uint8_t *m_ptr, int m_stride,
                                      int height, int *sse, int *sum_) {
  int y;
  // Note: For this function, h <= 8 (or maybe 16 if we add 4:1 partitions).
  // So the maximum value of sum is (2^12 - 1) * 4 * 16 =~ 2^18
  // and the maximum value of sum_sq is (2^12 - 1)^2 * 4 * 16 =~ 2^30.
  // So we can safely pack sum_sq into 32-bit fields, which is slightly more
  // convenient.
  __m128i sum = _mm_setzero_si128(), sum_sq = _mm_setzero_si128();
  const __m128i mask_max = _mm_set1_epi16((1 << AOM_BLEND_A64_ROUND_BITS));
  const __m128i round_const =
      _mm_set1_epi32((1 << AOM_BLEND_A64_ROUND_BITS) >> 1);
  const __m128i zero = _mm_setzero_si128();

  for (y = 0; y < height; y += 2) {
    __m128i src = _mm_unpacklo_epi64(
        _mm_loadl_epi64((const __m128i *)src_ptr),
        _mm_loadl_epi64((const __m128i *)&src_ptr[src_stride]));
    const __m128i a = _mm_loadu_si128((const __m128i *)a_ptr);
    const __m128i b = _mm_loadu_si128((const __m128i *)b_ptr);
    const __m128i m = _mm_unpacklo_epi8(
        _mm_unpacklo_epi32(_mm_cvtsi32_si128(*(const int *)m_ptr),
                           _mm_cvtsi32_si128(*(const int *)&m_ptr[m_stride])),
        zero);
    const __m128i m_inv = _mm_sub_epi16(mask_max, m);

    const __m128i data_l = _mm_unpacklo_epi16(a, b);
    const __m128i mask_l = _mm_unpacklo_epi16(m, m_inv);
    __m128i pred_l = _mm_madd_epi16(data_l, mask_l);
    pred_l = _mm_srai_epi32(_mm_add_epi32(pred_l, round_const),
                            AOM_BLEND_A64_ROUND_BITS);

    const __m128i data_r = _mm_unpackhi_epi16(a, b);
    const __m128i mask_r = _mm_unpackhi_epi16(m, m_inv);
    __m128i pred_r = _mm_madd_epi16(data_r, mask_r);
    pred_r = _mm_srai_epi32(_mm_add_epi32(pred_r, round_const),
                            AOM_BLEND_A64_ROUND_BITS);

    const __m128i src_l = _mm_unpacklo_epi16(src, zero);
    const __m128i src_r = _mm_unpackhi_epi16(src, zero);
    __m128i diff_l = _mm_sub_epi32(pred_l, src_l);
    __m128i diff_r = _mm_sub_epi32(pred_r, src_r);

    // Update partial sums and partial sums of squares
    sum = _mm_add_epi32(sum, _mm_add_epi32(diff_l, diff_r));
    const __m128i tmp = _mm_packs_epi32(diff_l, diff_r);
    const __m128i prod = _mm_madd_epi16(tmp, tmp);
    sum_sq = _mm_add_epi32(sum_sq, prod);

    src_ptr += src_stride * 2;
    a_ptr += 8;
    b_ptr += 8;
    m_ptr += m_stride * 2;
  }
  // Reduce down to a single sum and sum of squares
  sum = _mm_hadd_epi32(sum, sum_sq);
  sum = _mm_hadd_epi32(sum, zero);
  *sum_ = _mm_cvtsi128_si32(sum);
  *sse = (unsigned int)_mm_cvtsi128_si32(_mm_srli_si128(sum, 4));
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

void aom_comp_mask_pred_ssse3(uint8_t *comp_pred, const uint8_t *pred,
                              int width, int height, const uint8_t *ref,
                              int ref_stride, const uint8_t *mask,
                              int mask_stride, int invert_mask) {
  const uint8_t *src0 = invert_mask ? pred : ref;
  const uint8_t *src1 = invert_mask ? ref : pred;
  const int stride0 = invert_mask ? width : ref_stride;
  const int stride1 = invert_mask ? ref_stride : width;
  assert(height % 2 == 0);
  int i = 0;
  if (width == 8) {
    comp_mask_pred_8_ssse3(comp_pred, height, src0, stride0, src1, stride1,
                           mask, mask_stride);
  } else if (width == 16) {
    do {
      comp_mask_pred_16_ssse3(src0, src1, mask, comp_pred);
      comp_mask_pred_16_ssse3(src0 + stride0, src1 + stride1,
                              mask + mask_stride, comp_pred + width);
      comp_pred += (width << 1);
      src0 += (stride0 << 1);
      src1 += (stride1 << 1);
      mask += (mask_stride << 1);
      i += 2;
    } while (i < height);
  } else {
    do {
      for (int x = 0; x < width; x += 32) {
        comp_mask_pred_16_ssse3(src0 + x, src1 + x, mask + x, comp_pred);
        comp_mask_pred_16_ssse3(src0 + x + 16, src1 + x + 16, mask + x + 16,
                                comp_pred + 16);
        comp_pred += 32;
      }
      src0 += (stride0);
      src1 += (stride1);
      mask += (mask_stride);
      i += 1;
    } while (i < height);
  }
}
