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

#include <assert.h>
#include <emmintrin.h>  // SSE2

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/blend.h"
#include "aom_dsp/x86/mem_sse2.h"
#include "aom_dsp/x86/synonyms.h"
#include "aom_ports/mem.h"

unsigned int aom_get_mb_ss_sse2(const int16_t *src) {
  __m128i vsum = _mm_setzero_si128();
  int i;

  for (i = 0; i < 32; ++i) {
    const __m128i v = xx_loadu_128(src);
    vsum = _mm_add_epi32(vsum, _mm_madd_epi16(v, v));
    src += 8;
  }

  vsum = _mm_add_epi32(vsum, _mm_srli_si128(vsum, 8));
  vsum = _mm_add_epi32(vsum, _mm_srli_si128(vsum, 4));
  return (unsigned int)_mm_cvtsi128_si32(vsum);
}

static INLINE __m128i load4x2_sse2(const uint8_t *const p, const int stride) {
  const __m128i p0 = _mm_cvtsi32_si128(loadu_int32(p + 0 * stride));
  const __m128i p1 = _mm_cvtsi32_si128(loadu_int32(p + 1 * stride));
  return _mm_unpacklo_epi8(_mm_unpacklo_epi32(p0, p1), _mm_setzero_si128());
}

static INLINE __m128i load8_8to16_sse2(const uint8_t *const p) {
  const __m128i p0 = _mm_loadl_epi64((const __m128i *)p);
  return _mm_unpacklo_epi8(p0, _mm_setzero_si128());
}

static INLINE void load16_8to16_sse2(const uint8_t *const p, __m128i *out) {
  const __m128i p0 = _mm_loadu_si128((const __m128i *)p);
  out[0] = _mm_unpacklo_epi8(p0, _mm_setzero_si128());  // lower 8 values
  out[1] = _mm_unpackhi_epi8(p0, _mm_setzero_si128());  // upper 8 values
}

// Accumulate 4 32bit numbers in val to 1 32bit number
static INLINE unsigned int add32x4_sse2(__m128i val) {
  val = _mm_add_epi32(val, _mm_srli_si128(val, 8));
  val = _mm_add_epi32(val, _mm_srli_si128(val, 4));
  return (unsigned int)_mm_cvtsi128_si32(val);
}

// Accumulate 8 16bit in sum to 4 32bit number
static INLINE __m128i sum_to_32bit_sse2(const __m128i sum) {
  const __m128i sum_lo = _mm_srai_epi32(_mm_unpacklo_epi16(sum, sum), 16);
  const __m128i sum_hi = _mm_srai_epi32(_mm_unpackhi_epi16(sum, sum), 16);
  return _mm_add_epi32(sum_lo, sum_hi);
}

static INLINE void variance_kernel_sse2(const __m128i src, const __m128i ref,
                                        __m128i *const sse,
                                        __m128i *const sum) {
  const __m128i diff = _mm_sub_epi16(src, ref);
  *sse = _mm_add_epi32(*sse, _mm_madd_epi16(diff, diff));
  *sum = _mm_add_epi16(*sum, diff);
}

// Can handle 128 pixels' diff sum (such as 8x16 or 16x8)
// Slightly faster than variance_final_256_pel_sse2()
// diff sum of 128 pixels can still fit in 16bit integer
static INLINE void variance_final_128_pel_sse2(__m128i vsse, __m128i vsum,
                                               unsigned int *const sse,
                                               int *const sum) {
  *sse = add32x4_sse2(vsse);

  vsum = _mm_add_epi16(vsum, _mm_srli_si128(vsum, 8));
  vsum = _mm_add_epi16(vsum, _mm_srli_si128(vsum, 4));
  vsum = _mm_add_epi16(vsum, _mm_srli_si128(vsum, 2));
  *sum = (int16_t)_mm_extract_epi16(vsum, 0);
}

// Can handle 256 pixels' diff sum (such as 16x16)
static INLINE void variance_final_256_pel_sse2(__m128i vsse, __m128i vsum,
                                               unsigned int *const sse,
                                               int *const sum) {
  *sse = add32x4_sse2(vsse);

  vsum = _mm_add_epi16(vsum, _mm_srli_si128(vsum, 8));
  vsum = _mm_add_epi16(vsum, _mm_srli_si128(vsum, 4));
  *sum = (int16_t)_mm_extract_epi16(vsum, 0);
  *sum += (int16_t)_mm_extract_epi16(vsum, 1);
}

// Can handle 512 pixels' diff sum (such as 16x32 or 32x16)
static INLINE void variance_final_512_pel_sse2(__m128i vsse, __m128i vsum,
                                               unsigned int *const sse,
                                               int *const sum) {
  *sse = add32x4_sse2(vsse);

  vsum = _mm_add_epi16(vsum, _mm_srli_si128(vsum, 8));
  vsum = _mm_unpacklo_epi16(vsum, vsum);
  vsum = _mm_srai_epi32(vsum, 16);
  *sum = (int)add32x4_sse2(vsum);
}

// Can handle 1024 pixels' diff sum (such as 32x32)
static INLINE void variance_final_1024_pel_sse2(__m128i vsse, __m128i vsum,
                                                unsigned int *const sse,
                                                int *const sum) {
  *sse = add32x4_sse2(vsse);

  vsum = sum_to_32bit_sse2(vsum);
  *sum = (int)add32x4_sse2(vsum);
}

static INLINE void variance4_sse2(const uint8_t *src, const int src_stride,
                                  const uint8_t *ref, const int ref_stride,
                                  const int h, __m128i *const sse,
                                  __m128i *const sum) {
  assert(h <= 256);  // May overflow for larger height.
  *sum = _mm_setzero_si128();

  for (int i = 0; i < h; i += 2) {
    const __m128i s = load4x2_sse2(src, src_stride);
    const __m128i r = load4x2_sse2(ref, ref_stride);

    variance_kernel_sse2(s, r, sse, sum);
    src += 2 * src_stride;
    ref += 2 * ref_stride;
  }
}

static INLINE void variance8_sse2(const uint8_t *src, const int src_stride,
                                  const uint8_t *ref, const int ref_stride,
                                  const int h, __m128i *const sse,
                                  __m128i *const sum) {
  assert(h <= 128);  // May overflow for larger height.
  *sum = _mm_setzero_si128();
  *sse = _mm_setzero_si128();
  for (int i = 0; i < h; i++) {
    const __m128i s = load8_8to16_sse2(src);
    const __m128i r = load8_8to16_sse2(ref);

    variance_kernel_sse2(s, r, sse, sum);
    src += src_stride;
    ref += ref_stride;
  }
}

static INLINE void variance16_kernel_sse2(const uint8_t *const src,
                                          const uint8_t *const ref,
                                          __m128i *const sse,
                                          __m128i *const sum) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i s = _mm_loadu_si128((const __m128i *)src);
  const __m128i r = _mm_loadu_si128((const __m128i *)ref);
  const __m128i src0 = _mm_unpacklo_epi8(s, zero);
  const __m128i ref0 = _mm_unpacklo_epi8(r, zero);
  const __m128i src1 = _mm_unpackhi_epi8(s, zero);
  const __m128i ref1 = _mm_unpackhi_epi8(r, zero);

  variance_kernel_sse2(src0, ref0, sse, sum);
  variance_kernel_sse2(src1, ref1, sse, sum);
}

static INLINE void variance16_sse2(const uint8_t *src, const int src_stride,
                                   const uint8_t *ref, const int ref_stride,
                                   const int h, __m128i *const sse,
                                   __m128i *const sum) {
  assert(h <= 64);  // May overflow for larger height.
  *sum = _mm_setzero_si128();

  for (int i = 0; i < h; ++i) {
    variance16_kernel_sse2(src, ref, sse, sum);
    src += src_stride;
    ref += ref_stride;
  }
}

static INLINE void variance32_sse2(const uint8_t *src, const int src_stride,
                                   const uint8_t *ref, const int ref_stride,
                                   const int h, __m128i *const sse,
                                   __m128i *const sum) {
  assert(h <= 32);  // May overflow for larger height.
  // Don't initialize sse here since it's an accumulation.
  *sum = _mm_setzero_si128();

  for (int i = 0; i < h; ++i) {
    variance16_kernel_sse2(src + 0, ref + 0, sse, sum);
    variance16_kernel_sse2(src + 16, ref + 16, sse, sum);
    src += src_stride;
    ref += ref_stride;
  }
}

static INLINE void variance64_sse2(const uint8_t *src, const int src_stride,
                                   const uint8_t *ref, const int ref_stride,
                                   const int h, __m128i *const sse,
                                   __m128i *const sum) {
  assert(h <= 16);  // May overflow for larger height.
  *sum = _mm_setzero_si128();

  for (int i = 0; i < h; ++i) {
    variance16_kernel_sse2(src + 0, ref + 0, sse, sum);
    variance16_kernel_sse2(src + 16, ref + 16, sse, sum);
    variance16_kernel_sse2(src + 32, ref + 32, sse, sum);
    variance16_kernel_sse2(src + 48, ref + 48, sse, sum);
    src += src_stride;
    ref += ref_stride;
  }
}

static INLINE void variance128_sse2(const uint8_t *src, const int src_stride,
                                    const uint8_t *ref, const int ref_stride,
                                    const int h, __m128i *const sse,
                                    __m128i *const sum) {
  assert(h <= 8);  // May overflow for larger height.
  *sum = _mm_setzero_si128();

  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < 4; ++j) {
      const int offset0 = j << 5;
      const int offset1 = offset0 + 16;
      variance16_kernel_sse2(src + offset0, ref + offset0, sse, sum);
      variance16_kernel_sse2(src + offset1, ref + offset1, sse, sum);
    }
    src += src_stride;
    ref += ref_stride;
  }
}

void aom_get_var_sse_sum_8x8_quad_sse2(const uint8_t *src_ptr, int src_stride,
                                       const uint8_t *ref_ptr, int ref_stride,
                                       uint32_t *sse8x8, int *sum8x8,
                                       unsigned int *tot_sse, int *tot_sum,
                                       uint32_t *var8x8) {
  // Loop over 4 8x8 blocks. Process one 8x32 block.
  for (int k = 0; k < 4; k++) {
    const uint8_t *src = src_ptr;
    const uint8_t *ref = ref_ptr;
    __m128i vsum = _mm_setzero_si128();
    __m128i vsse = _mm_setzero_si128();
    for (int i = 0; i < 8; i++) {
      const __m128i s = load8_8to16_sse2(src + (k * 8));
      const __m128i r = load8_8to16_sse2(ref + (k * 8));
      const __m128i diff = _mm_sub_epi16(s, r);
      vsse = _mm_add_epi32(vsse, _mm_madd_epi16(diff, diff));
      vsum = _mm_add_epi16(vsum, diff);

      src += src_stride;
      ref += ref_stride;
    }
    variance_final_128_pel_sse2(vsse, vsum, &sse8x8[k], &sum8x8[k]);
  }

  // Calculate variance at 8x8 level and total sse, sum of 8x32 block.
  *tot_sse += sse8x8[0] + sse8x8[1] + sse8x8[2] + sse8x8[3];
  *tot_sum += sum8x8[0] + sum8x8[1] + sum8x8[2] + sum8x8[3];
  for (int i = 0; i < 4; i++)
    var8x8[i] = sse8x8[i] - (uint32_t)(((int64_t)sum8x8[i] * sum8x8[i]) >> 6);
}

void aom_get_var_sse_sum_16x16_dual_sse2(const uint8_t *src_ptr, int src_stride,
                                         const uint8_t *ref_ptr, int ref_stride,
                                         uint32_t *sse16x16,
                                         unsigned int *tot_sse, int *tot_sum,
                                         uint32_t *var16x16) {
  int sum16x16[2] = { 0 };
  // Loop over 2 16x16 blocks. Process one 16x32 block.
  for (int k = 0; k < 2; k++) {
    const uint8_t *src = src_ptr;
    const uint8_t *ref = ref_ptr;
    __m128i vsum = _mm_setzero_si128();
    __m128i vsse = _mm_setzero_si128();
    for (int i = 0; i < 16; i++) {
      __m128i s[2];
      __m128i r[2];
      load16_8to16_sse2(src + (k * 16), s);
      load16_8to16_sse2(ref + (k * 16), r);
      const __m128i diff0 = _mm_sub_epi16(s[0], r[0]);
      const __m128i diff1 = _mm_sub_epi16(s[1], r[1]);
      vsse = _mm_add_epi32(vsse, _mm_madd_epi16(diff0, diff0));
      vsse = _mm_add_epi32(vsse, _mm_madd_epi16(diff1, diff1));
      vsum = _mm_add_epi16(vsum, _mm_add_epi16(diff0, diff1));
      src += src_stride;
      ref += ref_stride;
    }
    variance_final_256_pel_sse2(vsse, vsum, &sse16x16[k], &sum16x16[k]);
  }

  // Calculate variance at 16x16 level and total sse, sum of 16x32 block.
  *tot_sse += sse16x16[0] + sse16x16[1];
  *tot_sum += sum16x16[0] + sum16x16[1];
  for (int i = 0; i < 2; i++)
    var16x16[i] =
        sse16x16[i] - (uint32_t)(((int64_t)sum16x16[i] * sum16x16[i]) >> 8);
}

#define AOM_VAR_NO_LOOP_SSE2(bw, bh, bits, max_pixels)                        \
  unsigned int aom_variance##bw##x##bh##_sse2(                                \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      unsigned int *sse) {                                                    \
    __m128i vsse = _mm_setzero_si128();                                       \
    __m128i vsum;                                                             \
    int sum = 0;                                                              \
    variance##bw##_sse2(src, src_stride, ref, ref_stride, bh, &vsse, &vsum);  \
    variance_final_##max_pixels##_pel_sse2(vsse, vsum, sse, &sum);            \
    assert(sum <= 255 * bw * bh);                                             \
    assert(sum >= -255 * bw * bh);                                            \
    return *sse - (uint32_t)(((int64_t)sum * sum) >> bits);                   \
  }

AOM_VAR_NO_LOOP_SSE2(4, 4, 4, 128)
AOM_VAR_NO_LOOP_SSE2(4, 8, 5, 128)
AOM_VAR_NO_LOOP_SSE2(4, 16, 6, 128)

AOM_VAR_NO_LOOP_SSE2(8, 4, 5, 128)
AOM_VAR_NO_LOOP_SSE2(8, 8, 6, 128)
AOM_VAR_NO_LOOP_SSE2(8, 16, 7, 128)

AOM_VAR_NO_LOOP_SSE2(16, 8, 7, 128)
AOM_VAR_NO_LOOP_SSE2(16, 16, 8, 256)
AOM_VAR_NO_LOOP_SSE2(16, 32, 9, 512)

AOM_VAR_NO_LOOP_SSE2(32, 8, 8, 256)
AOM_VAR_NO_LOOP_SSE2(32, 16, 9, 512)
AOM_VAR_NO_LOOP_SSE2(32, 32, 10, 1024)

#if !CONFIG_REALTIME_ONLY
AOM_VAR_NO_LOOP_SSE2(16, 4, 6, 128)
AOM_VAR_NO_LOOP_SSE2(8, 32, 8, 256)
AOM_VAR_NO_LOOP_SSE2(16, 64, 10, 1024)
#endif

#define AOM_VAR_LOOP_SSE2(bw, bh, bits, uh)                                   \
  unsigned int aom_variance##bw##x##bh##_sse2(                                \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      unsigned int *sse) {                                                    \
    __m128i vsse = _mm_setzero_si128();                                       \
    __m128i vsum = _mm_setzero_si128();                                       \
    for (int i = 0; i < (bh / uh); ++i) {                                     \
      __m128i vsum16;                                                         \
      variance##bw##_sse2(src, src_stride, ref, ref_stride, uh, &vsse,        \
                          &vsum16);                                           \
      vsum = _mm_add_epi32(vsum, sum_to_32bit_sse2(vsum16));                  \
      src += (src_stride * uh);                                               \
      ref += (ref_stride * uh);                                               \
    }                                                                         \
    *sse = add32x4_sse2(vsse);                                                \
    int sum = (int)add32x4_sse2(vsum);                                        \
    assert(sum <= 255 * bw * bh);                                             \
    assert(sum >= -255 * bw * bh);                                            \
    return *sse - (uint32_t)(((int64_t)sum * sum) >> bits);                   \
  }

AOM_VAR_LOOP_SSE2(32, 64, 11, 32)  // 32x32 * ( 64/32 )

AOM_VAR_LOOP_SSE2(64, 32, 11, 16)   // 64x16 * ( 32/16 )
AOM_VAR_LOOP_SSE2(64, 64, 12, 16)   // 64x16 * ( 64/16 )
AOM_VAR_LOOP_SSE2(64, 128, 13, 16)  // 64x16 * ( 128/16 )

AOM_VAR_LOOP_SSE2(128, 64, 13, 8)   // 128x8 * ( 64/8 )
AOM_VAR_LOOP_SSE2(128, 128, 14, 8)  // 128x8 * ( 128/8 )

#if !CONFIG_REALTIME_ONLY
AOM_VAR_NO_LOOP_SSE2(64, 16, 10, 1024)
#endif

unsigned int aom_mse8x8_sse2(const uint8_t *src, int src_stride,
                             const uint8_t *ref, int ref_stride,
                             unsigned int *sse) {
  aom_variance8x8_sse2(src, src_stride, ref, ref_stride, sse);
  return *sse;
}

unsigned int aom_mse8x16_sse2(const uint8_t *src, int src_stride,
                              const uint8_t *ref, int ref_stride,
                              unsigned int *sse) {
  aom_variance8x16_sse2(src, src_stride, ref, ref_stride, sse);
  return *sse;
}

unsigned int aom_mse16x8_sse2(const uint8_t *src, int src_stride,
                              const uint8_t *ref, int ref_stride,
                              unsigned int *sse) {
  aom_variance16x8_sse2(src, src_stride, ref, ref_stride, sse);
  return *sse;
}

unsigned int aom_mse16x16_sse2(const uint8_t *src, int src_stride,
                               const uint8_t *ref, int ref_stride,
                               unsigned int *sse) {
  aom_variance16x16_sse2(src, src_stride, ref, ref_stride, sse);
  return *sse;
}

// The 2 unused parameters are place holders for PIC enabled build.
// These definitions are for functions defined in subpel_variance.asm
#define DECL(w, opt)                                                           \
  int aom_sub_pixel_variance##w##xh_##opt(                                     \
      const uint8_t *src, ptrdiff_t src_stride, int x_offset, int y_offset,    \
      const uint8_t *dst, ptrdiff_t dst_stride, int height, unsigned int *sse, \
      void *unused0, void *unused)
#define DECLS(opt) \
  DECL(4, opt);    \
  DECL(8, opt);    \
  DECL(16, opt)

DECLS(sse2);
DECLS(ssse3);
#undef DECLS
#undef DECL

#define FN(w, h, wf, wlog2, hlog2, opt, cast_prod, cast)                      \
  unsigned int aom_sub_pixel_variance##w##x##h##_##opt(                       \
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
        const int se2 = aom_sub_pixel_variance##wf##xh_##opt(                 \
            src_ptr, src_stride, x_offset, y_offset, dst_ptr, dst_stride, hf, \
            &sse2, NULL, NULL);                                               \
        dst_ptr += hf * dst_stride;                                           \
        src_ptr += hf * src_stride;                                           \
        se += se2;                                                            \
        sse += sse2;                                                          \
      }                                                                       \
      src += wf;                                                              \
      dst += wf;                                                              \
    }                                                                         \
    *sse_ptr = sse;                                                           \
    return sse - (unsigned int)(cast_prod(cast se * se) >> (wlog2 + hlog2));  \
  }

#if !CONFIG_REALTIME_ONLY
#define FNS(opt)                                    \
  FN(128, 128, 16, 7, 7, opt, (int64_t), (int64_t)) \
  FN(128, 64, 16, 7, 6, opt, (int64_t), (int64_t))  \
  FN(64, 128, 16, 6, 7, opt, (int64_t), (int64_t))  \
  FN(64, 64, 16, 6, 6, opt, (int64_t), (int64_t))   \
  FN(64, 32, 16, 6, 5, opt, (int64_t), (int64_t))   \
  FN(32, 64, 16, 5, 6, opt, (int64_t), (int64_t))   \
  FN(32, 32, 16, 5, 5, opt, (int64_t), (int64_t))   \
  FN(32, 16, 16, 5, 4, opt, (int64_t), (int64_t))   \
  FN(16, 32, 16, 4, 5, opt, (int64_t), (int64_t))   \
  FN(16, 16, 16, 4, 4, opt, (uint32_t), (int64_t))  \
  FN(16, 8, 16, 4, 3, opt, (int32_t), (int32_t))    \
  FN(8, 16, 8, 3, 4, opt, (int32_t), (int32_t))     \
  FN(8, 8, 8, 3, 3, opt, (int32_t), (int32_t))      \
  FN(8, 4, 8, 3, 2, opt, (int32_t), (int32_t))      \
  FN(4, 8, 4, 2, 3, opt, (int32_t), (int32_t))      \
  FN(4, 4, 4, 2, 2, opt, (int32_t), (int32_t))      \
  FN(4, 16, 4, 2, 4, opt, (int32_t), (int32_t))     \
  FN(16, 4, 16, 4, 2, opt, (int32_t), (int32_t))    \
  FN(8, 32, 8, 3, 5, opt, (uint32_t), (int64_t))    \
  FN(32, 8, 16, 5, 3, opt, (uint32_t), (int64_t))   \
  FN(16, 64, 16, 4, 6, opt, (int64_t), (int64_t))   \
  FN(64, 16, 16, 6, 4, opt, (int64_t), (int64_t))
#else
#define FNS(opt)                                    \
  FN(128, 128, 16, 7, 7, opt, (int64_t), (int64_t)) \
  FN(128, 64, 16, 7, 6, opt, (int64_t), (int64_t))  \
  FN(64, 128, 16, 6, 7, opt, (int64_t), (int64_t))  \
  FN(64, 64, 16, 6, 6, opt, (int64_t), (int64_t))   \
  FN(64, 32, 16, 6, 5, opt, (int64_t), (int64_t))   \
  FN(32, 64, 16, 5, 6, opt, (int64_t), (int64_t))   \
  FN(32, 32, 16, 5, 5, opt, (int64_t), (int64_t))   \
  FN(32, 16, 16, 5, 4, opt, (int64_t), (int64_t))   \
  FN(16, 32, 16, 4, 5, opt, (int64_t), (int64_t))   \
  FN(16, 16, 16, 4, 4, opt, (uint32_t), (int64_t))  \
  FN(16, 8, 16, 4, 3, opt, (int32_t), (int32_t))    \
  FN(8, 16, 8, 3, 4, opt, (int32_t), (int32_t))     \
  FN(8, 8, 8, 3, 3, opt, (int32_t), (int32_t))      \
  FN(8, 4, 8, 3, 2, opt, (int32_t), (int32_t))      \
  FN(4, 8, 4, 2, 3, opt, (int32_t), (int32_t))      \
  FN(4, 4, 4, 2, 2, opt, (int32_t), (int32_t))
#endif

FNS(sse2)
FNS(ssse3)

#undef FNS
#undef FN

// The 2 unused parameters are place holders for PIC enabled build.
#define DECL(w, opt)                                                        \
  int aom_sub_pixel_avg_variance##w##xh_##opt(                              \
      const uint8_t *src, ptrdiff_t src_stride, int x_offset, int y_offset, \
      const uint8_t *dst, ptrdiff_t dst_stride, const uint8_t *sec,         \
      ptrdiff_t sec_stride, int height, unsigned int *sse, void *unused0,   \
      void *unused)
#define DECLS(opt) \
  DECL(4, opt);    \
  DECL(8, opt);    \
  DECL(16, opt)

DECLS(sse2);
DECLS(ssse3);
#undef DECL
#undef DECLS

#define FN(w, h, wf, wlog2, hlog2, opt, cast_prod, cast)                     \
  unsigned int aom_sub_pixel_avg_variance##w##x##h##_##opt(                  \
      const uint8_t *src, int src_stride, int x_offset, int y_offset,        \
      const uint8_t *dst, int dst_stride, unsigned int *sse_ptr,             \
      const uint8_t *sec) {                                                  \
    /*Avoid overflow in helper by capping height.*/                          \
    const int hf = AOMMIN(h, 64);                                            \
    unsigned int sse = 0;                                                    \
    int se = 0;                                                              \
    for (int i = 0; i < (w / wf); ++i) {                                     \
      const uint8_t *src_ptr = src;                                          \
      const uint8_t *dst_ptr = dst;                                          \
      const uint8_t *sec_ptr = sec;                                          \
      for (int j = 0; j < (h / hf); ++j) {                                   \
        unsigned int sse2;                                                   \
        const int se2 = aom_sub_pixel_avg_variance##wf##xh_##opt(            \
            src_ptr, src_stride, x_offset, y_offset, dst_ptr, dst_stride,    \
            sec_ptr, w, hf, &sse2, NULL, NULL);                              \
        dst_ptr += hf * dst_stride;                                          \
        src_ptr += hf * src_stride;                                          \
        sec_ptr += hf * w;                                                   \
        se += se2;                                                           \
        sse += sse2;                                                         \
      }                                                                      \
      src += wf;                                                             \
      dst += wf;                                                             \
      sec += wf;                                                             \
    }                                                                        \
    *sse_ptr = sse;                                                          \
    return sse - (unsigned int)(cast_prod(cast se * se) >> (wlog2 + hlog2)); \
  }

#if !CONFIG_REALTIME_ONLY
#define FNS(opt)                                    \
  FN(128, 128, 16, 7, 7, opt, (int64_t), (int64_t)) \
  FN(128, 64, 16, 7, 6, opt, (int64_t), (int64_t))  \
  FN(64, 128, 16, 6, 7, opt, (int64_t), (int64_t))  \
  FN(64, 64, 16, 6, 6, opt, (int64_t), (int64_t))   \
  FN(64, 32, 16, 6, 5, opt, (int64_t), (int64_t))   \
  FN(32, 64, 16, 5, 6, opt, (int64_t), (int64_t))   \
  FN(32, 32, 16, 5, 5, opt, (int64_t), (int64_t))   \
  FN(32, 16, 16, 5, 4, opt, (int64_t), (int64_t))   \
  FN(16, 32, 16, 4, 5, opt, (int64_t), (int64_t))   \
  FN(16, 16, 16, 4, 4, opt, (uint32_t), (int64_t))  \
  FN(16, 8, 16, 4, 3, opt, (uint32_t), (int32_t))   \
  FN(8, 16, 8, 3, 4, opt, (uint32_t), (int32_t))    \
  FN(8, 8, 8, 3, 3, opt, (uint32_t), (int32_t))     \
  FN(8, 4, 8, 3, 2, opt, (uint32_t), (int32_t))     \
  FN(4, 8, 4, 2, 3, opt, (uint32_t), (int32_t))     \
  FN(4, 4, 4, 2, 2, opt, (uint32_t), (int32_t))     \
  FN(4, 16, 4, 2, 4, opt, (int32_t), (int32_t))     \
  FN(16, 4, 16, 4, 2, opt, (int32_t), (int32_t))    \
  FN(8, 32, 8, 3, 5, opt, (uint32_t), (int64_t))    \
  FN(32, 8, 16, 5, 3, opt, (uint32_t), (int64_t))   \
  FN(16, 64, 16, 4, 6, opt, (int64_t), (int64_t))   \
  FN(64, 16, 16, 6, 4, opt, (int64_t), (int64_t))
#else
#define FNS(opt)                                    \
  FN(128, 128, 16, 7, 7, opt, (int64_t), (int64_t)) \
  FN(128, 64, 16, 7, 6, opt, (int64_t), (int64_t))  \
  FN(64, 128, 16, 6, 7, opt, (int64_t), (int64_t))  \
  FN(64, 64, 16, 6, 6, opt, (int64_t), (int64_t))   \
  FN(64, 32, 16, 6, 5, opt, (int64_t), (int64_t))   \
  FN(32, 64, 16, 5, 6, opt, (int64_t), (int64_t))   \
  FN(32, 32, 16, 5, 5, opt, (int64_t), (int64_t))   \
  FN(32, 16, 16, 5, 4, opt, (int64_t), (int64_t))   \
  FN(16, 32, 16, 4, 5, opt, (int64_t), (int64_t))   \
  FN(16, 16, 16, 4, 4, opt, (uint32_t), (int64_t))  \
  FN(16, 8, 16, 4, 3, opt, (uint32_t), (int32_t))   \
  FN(8, 16, 8, 3, 4, opt, (uint32_t), (int32_t))    \
  FN(8, 8, 8, 3, 3, opt, (uint32_t), (int32_t))     \
  FN(8, 4, 8, 3, 2, opt, (uint32_t), (int32_t))     \
  FN(4, 8, 4, 2, 3, opt, (uint32_t), (int32_t))     \
  FN(4, 4, 4, 2, 2, opt, (uint32_t), (int32_t))
#endif

FNS(sse2)
FNS(ssse3)

#undef FNS
#undef FN

static INLINE __m128i highbd_comp_mask_pred_line_sse2(const __m128i s0,
                                                      const __m128i s1,
                                                      const __m128i a) {
  const __m128i alpha_max = _mm_set1_epi16((1 << AOM_BLEND_A64_ROUND_BITS));
  const __m128i round_const =
      _mm_set1_epi32((1 << AOM_BLEND_A64_ROUND_BITS) >> 1);
  const __m128i a_inv = _mm_sub_epi16(alpha_max, a);

  const __m128i s_lo = _mm_unpacklo_epi16(s0, s1);
  const __m128i a_lo = _mm_unpacklo_epi16(a, a_inv);
  const __m128i pred_lo = _mm_madd_epi16(s_lo, a_lo);
  const __m128i pred_l = _mm_srai_epi32(_mm_add_epi32(pred_lo, round_const),
                                        AOM_BLEND_A64_ROUND_BITS);

  const __m128i s_hi = _mm_unpackhi_epi16(s0, s1);
  const __m128i a_hi = _mm_unpackhi_epi16(a, a_inv);
  const __m128i pred_hi = _mm_madd_epi16(s_hi, a_hi);
  const __m128i pred_h = _mm_srai_epi32(_mm_add_epi32(pred_hi, round_const),
                                        AOM_BLEND_A64_ROUND_BITS);

  const __m128i comp = _mm_packs_epi32(pred_l, pred_h);

  return comp;
}

void aom_highbd_comp_mask_pred_sse2(uint8_t *comp_pred8, const uint8_t *pred8,
                                    int width, int height, const uint8_t *ref8,
                                    int ref_stride, const uint8_t *mask,
                                    int mask_stride, int invert_mask) {
  int i = 0;
  uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);
  uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  const uint16_t *src0 = invert_mask ? pred : ref;
  const uint16_t *src1 = invert_mask ? ref : pred;
  const int stride0 = invert_mask ? width : ref_stride;
  const int stride1 = invert_mask ? ref_stride : width;
  const __m128i zero = _mm_setzero_si128();

  if (width == 8) {
    do {
      const __m128i s0 = _mm_loadu_si128((const __m128i *)(src0));
      const __m128i s1 = _mm_loadu_si128((const __m128i *)(src1));
      const __m128i m_8 = _mm_loadl_epi64((const __m128i *)mask);
      const __m128i m_16 = _mm_unpacklo_epi8(m_8, zero);

      const __m128i comp = highbd_comp_mask_pred_line_sse2(s0, s1, m_16);

      _mm_storeu_si128((__m128i *)comp_pred, comp);

      src0 += stride0;
      src1 += stride1;
      mask += mask_stride;
      comp_pred += width;
      i += 1;
    } while (i < height);
  } else if (width == 16) {
    do {
      const __m128i s0 = _mm_loadu_si128((const __m128i *)(src0));
      const __m128i s2 = _mm_loadu_si128((const __m128i *)(src0 + 8));
      const __m128i s1 = _mm_loadu_si128((const __m128i *)(src1));
      const __m128i s3 = _mm_loadu_si128((const __m128i *)(src1 + 8));

      const __m128i m_8 = _mm_loadu_si128((const __m128i *)mask);
      const __m128i m01_16 = _mm_unpacklo_epi8(m_8, zero);
      const __m128i m23_16 = _mm_unpackhi_epi8(m_8, zero);

      const __m128i comp = highbd_comp_mask_pred_line_sse2(s0, s1, m01_16);
      const __m128i comp1 = highbd_comp_mask_pred_line_sse2(s2, s3, m23_16);

      _mm_storeu_si128((__m128i *)comp_pred, comp);
      _mm_storeu_si128((__m128i *)(comp_pred + 8), comp1);

      src0 += stride0;
      src1 += stride1;
      mask += mask_stride;
      comp_pred += width;
      i += 1;
    } while (i < height);
  } else {
    do {
      for (int x = 0; x < width; x += 32) {
        for (int j = 0; j < 2; j++) {
          const __m128i s0 =
              _mm_loadu_si128((const __m128i *)(src0 + x + j * 16));
          const __m128i s2 =
              _mm_loadu_si128((const __m128i *)(src0 + x + 8 + j * 16));
          const __m128i s1 =
              _mm_loadu_si128((const __m128i *)(src1 + x + j * 16));
          const __m128i s3 =
              _mm_loadu_si128((const __m128i *)(src1 + x + 8 + j * 16));

          const __m128i m_8 =
              _mm_loadu_si128((const __m128i *)(mask + x + j * 16));
          const __m128i m01_16 = _mm_unpacklo_epi8(m_8, zero);
          const __m128i m23_16 = _mm_unpackhi_epi8(m_8, zero);

          const __m128i comp = highbd_comp_mask_pred_line_sse2(s0, s1, m01_16);
          const __m128i comp1 = highbd_comp_mask_pred_line_sse2(s2, s3, m23_16);

          _mm_storeu_si128((__m128i *)(comp_pred + j * 16), comp);
          _mm_storeu_si128((__m128i *)(comp_pred + 8 + j * 16), comp1);
        }
        comp_pred += 32;
      }
      src0 += stride0;
      src1 += stride1;
      mask += mask_stride;
      i += 1;
    } while (i < height);
  }
}

uint64_t aom_mse_4xh_16bit_sse2(uint8_t *dst, int dstride, uint16_t *src,
                                int sstride, int h) {
  uint64_t sum = 0;
  __m128i dst0_8x8, dst1_8x8, dst_16x8;
  __m128i src0_16x4, src1_16x4, src_16x8;
  __m128i res0_32x4, res0_64x2, res1_64x2;
  __m128i sub_result_16x8;
  const __m128i zeros = _mm_setzero_si128();
  __m128i square_result = _mm_setzero_si128();
  for (int i = 0; i < h; i += 2) {
    dst0_8x8 = _mm_cvtsi32_si128(*(int const *)(&dst[(i + 0) * dstride]));
    dst1_8x8 = _mm_cvtsi32_si128(*(int const *)(&dst[(i + 1) * dstride]));
    dst_16x8 = _mm_unpacklo_epi8(_mm_unpacklo_epi32(dst0_8x8, dst1_8x8), zeros);

    src0_16x4 = _mm_loadl_epi64((__m128i const *)(&src[(i + 0) * sstride]));
    src1_16x4 = _mm_loadl_epi64((__m128i const *)(&src[(i + 1) * sstride]));
    src_16x8 = _mm_unpacklo_epi64(src0_16x4, src1_16x4);

    sub_result_16x8 = _mm_sub_epi16(src_16x8, dst_16x8);

    res0_32x4 = _mm_madd_epi16(sub_result_16x8, sub_result_16x8);

    res0_64x2 = _mm_unpacklo_epi32(res0_32x4, zeros);
    res1_64x2 = _mm_unpackhi_epi32(res0_32x4, zeros);

    square_result =
        _mm_add_epi64(square_result, _mm_add_epi64(res0_64x2, res1_64x2));
  }
  const __m128i sum_64x1 =
      _mm_add_epi64(square_result, _mm_srli_si128(square_result, 8));
  xx_storel_64(&sum, sum_64x1);
  return sum;
}

uint64_t aom_mse_8xh_16bit_sse2(uint8_t *dst, int dstride, uint16_t *src,
                                int sstride, int h) {
  uint64_t sum = 0;
  __m128i dst_8x8, dst_16x8;
  __m128i src_16x8;
  __m128i res0_32x4, res0_64x2, res1_64x2;
  __m128i sub_result_16x8;
  const __m128i zeros = _mm_setzero_si128();
  __m128i square_result = _mm_setzero_si128();

  for (int i = 0; i < h; i++) {
    dst_8x8 = _mm_loadl_epi64((__m128i const *)(&dst[(i + 0) * dstride]));
    dst_16x8 = _mm_unpacklo_epi8(dst_8x8, zeros);

    src_16x8 = _mm_loadu_si128((__m128i *)&src[i * sstride]);

    sub_result_16x8 = _mm_sub_epi16(src_16x8, dst_16x8);

    res0_32x4 = _mm_madd_epi16(sub_result_16x8, sub_result_16x8);

    res0_64x2 = _mm_unpacklo_epi32(res0_32x4, zeros);
    res1_64x2 = _mm_unpackhi_epi32(res0_32x4, zeros);

    square_result =
        _mm_add_epi64(square_result, _mm_add_epi64(res0_64x2, res1_64x2));
  }
  const __m128i sum_64x1 =
      _mm_add_epi64(square_result, _mm_srli_si128(square_result, 8));
  xx_storel_64(&sum, sum_64x1);
  return sum;
}

uint64_t aom_mse_wxh_16bit_sse2(uint8_t *dst, int dstride, uint16_t *src,
                                int sstride, int w, int h) {
  assert((w == 8 || w == 4) && (h == 8 || h == 4) &&
         "w=8/4 and h=8/4 must satisfy");
  switch (w) {
    case 4: return aom_mse_4xh_16bit_sse2(dst, dstride, src, sstride, h);
    case 8: return aom_mse_8xh_16bit_sse2(dst, dstride, src, sstride, h);
    default: assert(0 && "unsupported width"); return -1;
  }
}

uint64_t aom_mse_16xh_16bit_sse2(uint8_t *dst, int dstride, uint16_t *src,
                                 int w, int h) {
  assert((w == 8 || w == 4) && (h == 8 || h == 4) &&
         "w=8/4 and h=8/4 must be satisfied");
  const int num_blks = 16 / w;
  uint64_t sum = 0;
  for (int i = 0; i < num_blks; i++) {
    sum += aom_mse_wxh_16bit_sse2(dst, dstride, src, w, w, h);
    dst += w;
    src += (w * h);
  }
  return sum;
}
