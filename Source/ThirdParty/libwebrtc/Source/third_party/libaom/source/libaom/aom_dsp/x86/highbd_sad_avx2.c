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

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/x86/synonyms_avx2.h"
#include "aom_ports/mem.h"

// SAD
static INLINE unsigned int get_sad_from_mm256_epi32(const __m256i *v) {
  // input 8 32-bit summation
  __m128i lo128, hi128;
  __m256i u = _mm256_srli_si256(*v, 8);
  u = _mm256_add_epi32(u, *v);

  // 4 32-bit summation
  hi128 = _mm256_extracti128_si256(u, 1);
  lo128 = _mm256_castsi256_si128(u);
  lo128 = _mm_add_epi32(hi128, lo128);

  // 2 32-bit summation
  hi128 = _mm_srli_si128(lo128, 4);
  lo128 = _mm_add_epi32(lo128, hi128);

  return (unsigned int)_mm_cvtsi128_si32(lo128);
}

static INLINE void highbd_sad16x4_core_avx2(__m256i *s, __m256i *r,
                                            __m256i *sad_acc) {
  const __m256i zero = _mm256_setzero_si256();
  int i;
  for (i = 0; i < 4; i++) {
    s[i] = _mm256_sub_epi16(s[i], r[i]);
    s[i] = _mm256_abs_epi16(s[i]);
  }

  s[0] = _mm256_add_epi16(s[0], s[1]);
  s[0] = _mm256_add_epi16(s[0], s[2]);
  s[0] = _mm256_add_epi16(s[0], s[3]);

  r[0] = _mm256_unpacklo_epi16(s[0], zero);
  r[1] = _mm256_unpackhi_epi16(s[0], zero);

  r[0] = _mm256_add_epi32(r[0], r[1]);
  *sad_acc = _mm256_add_epi32(*sad_acc, r[0]);
}

// If sec_ptr = 0, calculate regular SAD. Otherwise, calculate average SAD.
static INLINE void sad16x4(const uint16_t *src_ptr, int src_stride,
                           const uint16_t *ref_ptr, int ref_stride,
                           const uint16_t *sec_ptr, __m256i *sad_acc) {
  __m256i s[4], r[4];
  s[0] = _mm256_loadu_si256((const __m256i *)src_ptr);
  s[1] = _mm256_loadu_si256((const __m256i *)(src_ptr + src_stride));
  s[2] = _mm256_loadu_si256((const __m256i *)(src_ptr + 2 * src_stride));
  s[3] = _mm256_loadu_si256((const __m256i *)(src_ptr + 3 * src_stride));

  r[0] = _mm256_loadu_si256((const __m256i *)ref_ptr);
  r[1] = _mm256_loadu_si256((const __m256i *)(ref_ptr + ref_stride));
  r[2] = _mm256_loadu_si256((const __m256i *)(ref_ptr + 2 * ref_stride));
  r[3] = _mm256_loadu_si256((const __m256i *)(ref_ptr + 3 * ref_stride));

  if (sec_ptr) {
    r[0] = _mm256_avg_epu16(r[0], _mm256_loadu_si256((const __m256i *)sec_ptr));
    r[1] = _mm256_avg_epu16(
        r[1], _mm256_loadu_si256((const __m256i *)(sec_ptr + 16)));
    r[2] = _mm256_avg_epu16(
        r[2], _mm256_loadu_si256((const __m256i *)(sec_ptr + 32)));
    r[3] = _mm256_avg_epu16(
        r[3], _mm256_loadu_si256((const __m256i *)(sec_ptr + 48)));
  }
  highbd_sad16x4_core_avx2(s, r, sad_acc);
}

static AOM_FORCE_INLINE unsigned int aom_highbd_sad16xN_avx2(int N,
                                                             const uint8_t *src,
                                                             int src_stride,
                                                             const uint8_t *ref,
                                                             int ref_stride) {
  const uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src);
  const uint16_t *ref_ptr = CONVERT_TO_SHORTPTR(ref);
  int i;
  __m256i sad = _mm256_setzero_si256();
  for (i = 0; i < N; i += 4) {
    sad16x4(src_ptr, src_stride, ref_ptr, ref_stride, NULL, &sad);
    src_ptr += src_stride << 2;
    ref_ptr += ref_stride << 2;
  }
  return (unsigned int)get_sad_from_mm256_epi32(&sad);
}

static void sad32x4(const uint16_t *src_ptr, int src_stride,
                    const uint16_t *ref_ptr, int ref_stride,
                    const uint16_t *sec_ptr, __m256i *sad_acc) {
  __m256i s[4], r[4];
  int row_sections = 0;

  while (row_sections < 2) {
    s[0] = _mm256_loadu_si256((const __m256i *)src_ptr);
    s[1] = _mm256_loadu_si256((const __m256i *)(src_ptr + 16));
    s[2] = _mm256_loadu_si256((const __m256i *)(src_ptr + src_stride));
    s[3] = _mm256_loadu_si256((const __m256i *)(src_ptr + src_stride + 16));

    r[0] = _mm256_loadu_si256((const __m256i *)ref_ptr);
    r[1] = _mm256_loadu_si256((const __m256i *)(ref_ptr + 16));
    r[2] = _mm256_loadu_si256((const __m256i *)(ref_ptr + ref_stride));
    r[3] = _mm256_loadu_si256((const __m256i *)(ref_ptr + ref_stride + 16));

    if (sec_ptr) {
      r[0] =
          _mm256_avg_epu16(r[0], _mm256_loadu_si256((const __m256i *)sec_ptr));
      r[1] = _mm256_avg_epu16(
          r[1], _mm256_loadu_si256((const __m256i *)(sec_ptr + 16)));
      r[2] = _mm256_avg_epu16(
          r[2], _mm256_loadu_si256((const __m256i *)(sec_ptr + 32)));
      r[3] = _mm256_avg_epu16(
          r[3], _mm256_loadu_si256((const __m256i *)(sec_ptr + 48)));
      sec_ptr += 32 << 1;
    }
    highbd_sad16x4_core_avx2(s, r, sad_acc);

    row_sections += 1;
    src_ptr += src_stride << 1;
    ref_ptr += ref_stride << 1;
  }
}

static AOM_FORCE_INLINE unsigned int aom_highbd_sad32xN_avx2(int N,
                                                             const uint8_t *src,
                                                             int src_stride,
                                                             const uint8_t *ref,
                                                             int ref_stride) {
  __m256i sad = _mm256_setzero_si256();
  uint16_t *srcp = CONVERT_TO_SHORTPTR(src);
  uint16_t *refp = CONVERT_TO_SHORTPTR(ref);
  const int left_shift = 2;
  int i;

  for (i = 0; i < N; i += 4) {
    sad32x4(srcp, src_stride, refp, ref_stride, NULL, &sad);
    srcp += src_stride << left_shift;
    refp += ref_stride << left_shift;
  }
  return get_sad_from_mm256_epi32(&sad);
}

static void sad64x2(const uint16_t *src_ptr, int src_stride,
                    const uint16_t *ref_ptr, int ref_stride,
                    const uint16_t *sec_ptr, __m256i *sad_acc) {
  __m256i s[4], r[4];
  int i;
  for (i = 0; i < 2; i++) {
    s[0] = _mm256_loadu_si256((const __m256i *)src_ptr);
    s[1] = _mm256_loadu_si256((const __m256i *)(src_ptr + 16));
    s[2] = _mm256_loadu_si256((const __m256i *)(src_ptr + 32));
    s[3] = _mm256_loadu_si256((const __m256i *)(src_ptr + 48));

    r[0] = _mm256_loadu_si256((const __m256i *)ref_ptr);
    r[1] = _mm256_loadu_si256((const __m256i *)(ref_ptr + 16));
    r[2] = _mm256_loadu_si256((const __m256i *)(ref_ptr + 32));
    r[3] = _mm256_loadu_si256((const __m256i *)(ref_ptr + 48));
    if (sec_ptr) {
      r[0] =
          _mm256_avg_epu16(r[0], _mm256_loadu_si256((const __m256i *)sec_ptr));
      r[1] = _mm256_avg_epu16(
          r[1], _mm256_loadu_si256((const __m256i *)(sec_ptr + 16)));
      r[2] = _mm256_avg_epu16(
          r[2], _mm256_loadu_si256((const __m256i *)(sec_ptr + 32)));
      r[3] = _mm256_avg_epu16(
          r[3], _mm256_loadu_si256((const __m256i *)(sec_ptr + 48)));
      sec_ptr += 64;
    }
    highbd_sad16x4_core_avx2(s, r, sad_acc);
    src_ptr += src_stride;
    ref_ptr += ref_stride;
  }
}

static AOM_FORCE_INLINE unsigned int aom_highbd_sad64xN_avx2(int N,
                                                             const uint8_t *src,
                                                             int src_stride,
                                                             const uint8_t *ref,
                                                             int ref_stride) {
  __m256i sad = _mm256_setzero_si256();
  uint16_t *srcp = CONVERT_TO_SHORTPTR(src);
  uint16_t *refp = CONVERT_TO_SHORTPTR(ref);
  const int left_shift = 1;
  int i;
  for (i = 0; i < N; i += 2) {
    sad64x2(srcp, src_stride, refp, ref_stride, NULL, &sad);
    srcp += src_stride << left_shift;
    refp += ref_stride << left_shift;
  }
  return get_sad_from_mm256_epi32(&sad);
}

static void sad128x1(const uint16_t *src_ptr, const uint16_t *ref_ptr,
                     const uint16_t *sec_ptr, __m256i *sad_acc) {
  __m256i s[4], r[4];
  int i;
  for (i = 0; i < 2; i++) {
    s[0] = _mm256_loadu_si256((const __m256i *)src_ptr);
    s[1] = _mm256_loadu_si256((const __m256i *)(src_ptr + 16));
    s[2] = _mm256_loadu_si256((const __m256i *)(src_ptr + 32));
    s[3] = _mm256_loadu_si256((const __m256i *)(src_ptr + 48));
    r[0] = _mm256_loadu_si256((const __m256i *)ref_ptr);
    r[1] = _mm256_loadu_si256((const __m256i *)(ref_ptr + 16));
    r[2] = _mm256_loadu_si256((const __m256i *)(ref_ptr + 32));
    r[3] = _mm256_loadu_si256((const __m256i *)(ref_ptr + 48));
    if (sec_ptr) {
      r[0] =
          _mm256_avg_epu16(r[0], _mm256_loadu_si256((const __m256i *)sec_ptr));
      r[1] = _mm256_avg_epu16(
          r[1], _mm256_loadu_si256((const __m256i *)(sec_ptr + 16)));
      r[2] = _mm256_avg_epu16(
          r[2], _mm256_loadu_si256((const __m256i *)(sec_ptr + 32)));
      r[3] = _mm256_avg_epu16(
          r[3], _mm256_loadu_si256((const __m256i *)(sec_ptr + 48)));
      sec_ptr += 64;
    }
    highbd_sad16x4_core_avx2(s, r, sad_acc);
    src_ptr += 64;
    ref_ptr += 64;
  }
}

static AOM_FORCE_INLINE unsigned int aom_highbd_sad128xN_avx2(
    int N, const uint8_t *src, int src_stride, const uint8_t *ref,
    int ref_stride) {
  __m256i sad = _mm256_setzero_si256();
  uint16_t *srcp = CONVERT_TO_SHORTPTR(src);
  uint16_t *refp = CONVERT_TO_SHORTPTR(ref);
  int row = 0;
  while (row < N) {
    sad128x1(srcp, refp, NULL, &sad);
    srcp += src_stride;
    refp += ref_stride;
    row++;
  }
  return get_sad_from_mm256_epi32(&sad);
}

#define HIGHBD_SADMXN_AVX2(m, n)                                            \
  unsigned int aom_highbd_sad##m##x##n##_avx2(                              \
      const uint8_t *src, int src_stride, const uint8_t *ref,               \
      int ref_stride) {                                                     \
    return aom_highbd_sad##m##xN_avx2(n, src, src_stride, ref, ref_stride); \
  }

#define HIGHBD_SAD_SKIP_MXN_AVX2(m, n)                                       \
  unsigned int aom_highbd_sad_skip_##m##x##n##_avx2(                         \
      const uint8_t *src, int src_stride, const uint8_t *ref,                \
      int ref_stride) {                                                      \
    return 2 * aom_highbd_sad##m##xN_avx2((n / 2), src, 2 * src_stride, ref, \
                                          2 * ref_stride);                   \
  }

HIGHBD_SADMXN_AVX2(16, 8)
HIGHBD_SADMXN_AVX2(16, 16)
HIGHBD_SADMXN_AVX2(16, 32)

HIGHBD_SADMXN_AVX2(32, 16)
HIGHBD_SADMXN_AVX2(32, 32)
HIGHBD_SADMXN_AVX2(32, 64)

HIGHBD_SADMXN_AVX2(64, 32)
HIGHBD_SADMXN_AVX2(64, 64)
HIGHBD_SADMXN_AVX2(64, 128)

HIGHBD_SADMXN_AVX2(128, 64)
HIGHBD_SADMXN_AVX2(128, 128)

#if !CONFIG_REALTIME_ONLY
HIGHBD_SADMXN_AVX2(16, 4)
HIGHBD_SADMXN_AVX2(16, 64)
HIGHBD_SADMXN_AVX2(32, 8)
HIGHBD_SADMXN_AVX2(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

HIGHBD_SAD_SKIP_MXN_AVX2(16, 8)
HIGHBD_SAD_SKIP_MXN_AVX2(16, 16)
HIGHBD_SAD_SKIP_MXN_AVX2(16, 32)

HIGHBD_SAD_SKIP_MXN_AVX2(32, 16)
HIGHBD_SAD_SKIP_MXN_AVX2(32, 32)
HIGHBD_SAD_SKIP_MXN_AVX2(32, 64)

HIGHBD_SAD_SKIP_MXN_AVX2(64, 32)
HIGHBD_SAD_SKIP_MXN_AVX2(64, 64)
HIGHBD_SAD_SKIP_MXN_AVX2(64, 128)

HIGHBD_SAD_SKIP_MXN_AVX2(128, 64)
HIGHBD_SAD_SKIP_MXN_AVX2(128, 128)

#if !CONFIG_REALTIME_ONLY
HIGHBD_SAD_SKIP_MXN_AVX2(16, 64)
HIGHBD_SAD_SKIP_MXN_AVX2(32, 8)
HIGHBD_SAD_SKIP_MXN_AVX2(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

#if !CONFIG_REALTIME_ONLY
unsigned int aom_highbd_sad16x4_avg_avx2(const uint8_t *src, int src_stride,
                                         const uint8_t *ref, int ref_stride,
                                         const uint8_t *second_pred) {
  __m256i sad = _mm256_setzero_si256();
  uint16_t *srcp = CONVERT_TO_SHORTPTR(src);
  uint16_t *refp = CONVERT_TO_SHORTPTR(ref);
  uint16_t *secp = CONVERT_TO_SHORTPTR(second_pred);
  sad16x4(srcp, src_stride, refp, ref_stride, secp, &sad);

  return get_sad_from_mm256_epi32(&sad);
}
#endif  // !CONFIG_REALTIME_ONLY

unsigned int aom_highbd_sad16x8_avg_avx2(const uint8_t *src, int src_stride,
                                         const uint8_t *ref, int ref_stride,
                                         const uint8_t *second_pred) {
  __m256i sad = _mm256_setzero_si256();
  uint16_t *srcp = CONVERT_TO_SHORTPTR(src);
  uint16_t *refp = CONVERT_TO_SHORTPTR(ref);
  uint16_t *secp = CONVERT_TO_SHORTPTR(second_pred);

  sad16x4(srcp, src_stride, refp, ref_stride, secp, &sad);

  // Next 4 rows
  srcp += src_stride << 2;
  refp += ref_stride << 2;
  secp += 64;
  sad16x4(srcp, src_stride, refp, ref_stride, secp, &sad);
  return get_sad_from_mm256_epi32(&sad);
}

unsigned int aom_highbd_sad16x16_avg_avx2(const uint8_t *src, int src_stride,
                                          const uint8_t *ref, int ref_stride,
                                          const uint8_t *second_pred) {
  const int left_shift = 3;
  uint32_t sum = aom_highbd_sad16x8_avg_avx2(src, src_stride, ref, ref_stride,
                                             second_pred);
  src += src_stride << left_shift;
  ref += ref_stride << left_shift;
  second_pred += 16 << left_shift;
  sum += aom_highbd_sad16x8_avg_avx2(src, src_stride, ref, ref_stride,
                                     second_pred);
  return sum;
}

unsigned int aom_highbd_sad16x32_avg_avx2(const uint8_t *src, int src_stride,
                                          const uint8_t *ref, int ref_stride,
                                          const uint8_t *second_pred) {
  const int left_shift = 4;
  uint32_t sum = aom_highbd_sad16x16_avg_avx2(src, src_stride, ref, ref_stride,
                                              second_pred);
  src += src_stride << left_shift;
  ref += ref_stride << left_shift;
  second_pred += 16 << left_shift;
  sum += aom_highbd_sad16x16_avg_avx2(src, src_stride, ref, ref_stride,
                                      second_pred);
  return sum;
}

#if !CONFIG_REALTIME_ONLY
unsigned int aom_highbd_sad16x64_avg_avx2(const uint8_t *src, int src_stride,
                                          const uint8_t *ref, int ref_stride,
                                          const uint8_t *second_pred) {
  const int left_shift = 5;
  uint32_t sum = aom_highbd_sad16x32_avg_avx2(src, src_stride, ref, ref_stride,
                                              second_pred);
  src += src_stride << left_shift;
  ref += ref_stride << left_shift;
  second_pred += 16 << left_shift;
  sum += aom_highbd_sad16x32_avg_avx2(src, src_stride, ref, ref_stride,
                                      second_pred);
  return sum;
}

unsigned int aom_highbd_sad32x8_avg_avx2(const uint8_t *src, int src_stride,
                                         const uint8_t *ref, int ref_stride,
                                         const uint8_t *second_pred) {
  __m256i sad = _mm256_setzero_si256();
  uint16_t *srcp = CONVERT_TO_SHORTPTR(src);
  uint16_t *refp = CONVERT_TO_SHORTPTR(ref);
  uint16_t *secp = CONVERT_TO_SHORTPTR(second_pred);
  const int left_shift = 2;
  int row_section = 0;

  while (row_section < 2) {
    sad32x4(srcp, src_stride, refp, ref_stride, secp, &sad);
    srcp += src_stride << left_shift;
    refp += ref_stride << left_shift;
    secp += 32 << left_shift;
    row_section += 1;
  }
  return get_sad_from_mm256_epi32(&sad);
}
#endif  // !CONFIG_REALTIME_ONLY

unsigned int aom_highbd_sad32x16_avg_avx2(const uint8_t *src, int src_stride,
                                          const uint8_t *ref, int ref_stride,
                                          const uint8_t *second_pred) {
  __m256i sad = _mm256_setzero_si256();
  uint16_t *srcp = CONVERT_TO_SHORTPTR(src);
  uint16_t *refp = CONVERT_TO_SHORTPTR(ref);
  uint16_t *secp = CONVERT_TO_SHORTPTR(second_pred);
  const int left_shift = 2;
  int row_section = 0;

  while (row_section < 4) {
    sad32x4(srcp, src_stride, refp, ref_stride, secp, &sad);
    srcp += src_stride << left_shift;
    refp += ref_stride << left_shift;
    secp += 32 << left_shift;
    row_section += 1;
  }
  return get_sad_from_mm256_epi32(&sad);
}

unsigned int aom_highbd_sad32x32_avg_avx2(const uint8_t *src, int src_stride,
                                          const uint8_t *ref, int ref_stride,
                                          const uint8_t *second_pred) {
  const int left_shift = 4;
  uint32_t sum = aom_highbd_sad32x16_avg_avx2(src, src_stride, ref, ref_stride,
                                              second_pred);
  src += src_stride << left_shift;
  ref += ref_stride << left_shift;
  second_pred += 32 << left_shift;
  sum += aom_highbd_sad32x16_avg_avx2(src, src_stride, ref, ref_stride,
                                      second_pred);
  return sum;
}

unsigned int aom_highbd_sad32x64_avg_avx2(const uint8_t *src, int src_stride,
                                          const uint8_t *ref, int ref_stride,
                                          const uint8_t *second_pred) {
  const int left_shift = 5;
  uint32_t sum = aom_highbd_sad32x32_avg_avx2(src, src_stride, ref, ref_stride,
                                              second_pred);
  src += src_stride << left_shift;
  ref += ref_stride << left_shift;
  second_pred += 32 << left_shift;
  sum += aom_highbd_sad32x32_avg_avx2(src, src_stride, ref, ref_stride,
                                      second_pred);
  return sum;
}

#if !CONFIG_REALTIME_ONLY
unsigned int aom_highbd_sad64x16_avg_avx2(const uint8_t *src, int src_stride,
                                          const uint8_t *ref, int ref_stride,
                                          const uint8_t *second_pred) {
  __m256i sad = _mm256_setzero_si256();
  uint16_t *srcp = CONVERT_TO_SHORTPTR(src);
  uint16_t *refp = CONVERT_TO_SHORTPTR(ref);
  uint16_t *secp = CONVERT_TO_SHORTPTR(second_pred);
  const int left_shift = 1;
  int row_section = 0;

  while (row_section < 8) {
    sad64x2(srcp, src_stride, refp, ref_stride, secp, &sad);
    srcp += src_stride << left_shift;
    refp += ref_stride << left_shift;
    secp += 64 << left_shift;
    row_section += 1;
  }
  return get_sad_from_mm256_epi32(&sad);
}
#endif  // !CONFIG_REALTIME_ONLY

unsigned int aom_highbd_sad64x32_avg_avx2(const uint8_t *src, int src_stride,
                                          const uint8_t *ref, int ref_stride,
                                          const uint8_t *second_pred) {
  __m256i sad = _mm256_setzero_si256();
  uint16_t *srcp = CONVERT_TO_SHORTPTR(src);
  uint16_t *refp = CONVERT_TO_SHORTPTR(ref);
  uint16_t *secp = CONVERT_TO_SHORTPTR(second_pred);
  const int left_shift = 1;
  int row_section = 0;

  while (row_section < 16) {
    sad64x2(srcp, src_stride, refp, ref_stride, secp, &sad);
    srcp += src_stride << left_shift;
    refp += ref_stride << left_shift;
    secp += 64 << left_shift;
    row_section += 1;
  }
  return get_sad_from_mm256_epi32(&sad);
}

unsigned int aom_highbd_sad64x64_avg_avx2(const uint8_t *src, int src_stride,
                                          const uint8_t *ref, int ref_stride,
                                          const uint8_t *second_pred) {
  const int left_shift = 5;
  uint32_t sum = aom_highbd_sad64x32_avg_avx2(src, src_stride, ref, ref_stride,
                                              second_pred);
  src += src_stride << left_shift;
  ref += ref_stride << left_shift;
  second_pred += 64 << left_shift;
  sum += aom_highbd_sad64x32_avg_avx2(src, src_stride, ref, ref_stride,
                                      second_pred);
  return sum;
}

unsigned int aom_highbd_sad64x128_avg_avx2(const uint8_t *src, int src_stride,
                                           const uint8_t *ref, int ref_stride,
                                           const uint8_t *second_pred) {
  const int left_shift = 6;
  uint32_t sum = aom_highbd_sad64x64_avg_avx2(src, src_stride, ref, ref_stride,
                                              second_pred);
  src += src_stride << left_shift;
  ref += ref_stride << left_shift;
  second_pred += 64 << left_shift;
  sum += aom_highbd_sad64x64_avg_avx2(src, src_stride, ref, ref_stride,
                                      second_pred);
  return sum;
}

unsigned int aom_highbd_sad128x64_avg_avx2(const uint8_t *src, int src_stride,
                                           const uint8_t *ref, int ref_stride,
                                           const uint8_t *second_pred) {
  __m256i sad = _mm256_setzero_si256();
  uint16_t *srcp = CONVERT_TO_SHORTPTR(src);
  uint16_t *refp = CONVERT_TO_SHORTPTR(ref);
  uint16_t *secp = CONVERT_TO_SHORTPTR(second_pred);
  int row = 0;
  while (row < 64) {
    sad128x1(srcp, refp, secp, &sad);
    srcp += src_stride;
    refp += ref_stride;
    secp += 16 << 3;
    row += 1;
  }
  return get_sad_from_mm256_epi32(&sad);
}

unsigned int aom_highbd_sad128x128_avg_avx2(const uint8_t *src, int src_stride,
                                            const uint8_t *ref, int ref_stride,
                                            const uint8_t *second_pred) {
  unsigned int sum;
  const int left_shift = 6;

  sum = aom_highbd_sad128x64_avg_avx2(src, src_stride, ref, ref_stride,
                                      second_pred);
  src += src_stride << left_shift;
  ref += ref_stride << left_shift;
  second_pred += 128 << left_shift;
  sum += aom_highbd_sad128x64_avg_avx2(src, src_stride, ref, ref_stride,
                                       second_pred);
  return sum;
}

// SAD 4D
// Combine 4 __m256i input vectors  v to uint32_t result[4]
static INLINE void get_4d_sad_from_mm256_epi32(const __m256i *v,
                                               uint32_t *res) {
  __m256i u0, u1, u2, u3;
  const __m256i mask = _mm256_set1_epi64x(~0u);
  __m128i sad;

  // 8 32-bit summation
  u0 = _mm256_srli_si256(v[0], 4);
  u1 = _mm256_srli_si256(v[1], 4);
  u2 = _mm256_srli_si256(v[2], 4);
  u3 = _mm256_srli_si256(v[3], 4);

  u0 = _mm256_add_epi32(u0, v[0]);
  u1 = _mm256_add_epi32(u1, v[1]);
  u2 = _mm256_add_epi32(u2, v[2]);
  u3 = _mm256_add_epi32(u3, v[3]);

  u0 = _mm256_and_si256(u0, mask);
  u1 = _mm256_and_si256(u1, mask);
  u2 = _mm256_and_si256(u2, mask);
  u3 = _mm256_and_si256(u3, mask);
  // 4 32-bit summation, evenly positioned

  u1 = _mm256_slli_si256(u1, 4);
  u3 = _mm256_slli_si256(u3, 4);

  u0 = _mm256_or_si256(u0, u1);
  u2 = _mm256_or_si256(u2, u3);
  // 8 32-bit summation, interleaved

  u1 = _mm256_unpacklo_epi64(u0, u2);
  u3 = _mm256_unpackhi_epi64(u0, u2);

  u0 = _mm256_add_epi32(u1, u3);
  sad = _mm_add_epi32(_mm256_extractf128_si256(u0, 1),
                      _mm256_castsi256_si128(u0));
  _mm_storeu_si128((__m128i *)res, sad);
}

static void convert_pointers(const uint8_t *const ref8[],
                             const uint16_t *ref[]) {
  ref[0] = CONVERT_TO_SHORTPTR(ref8[0]);
  ref[1] = CONVERT_TO_SHORTPTR(ref8[1]);
  ref[2] = CONVERT_TO_SHORTPTR(ref8[2]);
  ref[3] = CONVERT_TO_SHORTPTR(ref8[3]);
}

static void init_sad(__m256i *s) {
  s[0] = _mm256_setzero_si256();
  s[1] = _mm256_setzero_si256();
  s[2] = _mm256_setzero_si256();
  s[3] = _mm256_setzero_si256();
}

static AOM_FORCE_INLINE void aom_highbd_sadMxNxD_avx2(
    int M, int N, int D, const uint8_t *src, int src_stride,
    const uint8_t *const ref_array[4], int ref_stride, uint32_t sad_array[4]) {
  __m256i sad_vec[4];
  const uint16_t *refp[4];
  const uint16_t *keep = CONVERT_TO_SHORTPTR(src);
  const uint16_t *srcp;
  const int shift_for_rows = (M < 128) + (M < 64);
  const int row_units = 1 << shift_for_rows;
  int i, r;

  init_sad(sad_vec);
  convert_pointers(ref_array, refp);

  for (i = 0; i < D; ++i) {
    srcp = keep;
    for (r = 0; r < N; r += row_units) {
      if (M == 128) {
        sad128x1(srcp, refp[i], NULL, &sad_vec[i]);
      } else if (M == 64) {
        sad64x2(srcp, src_stride, refp[i], ref_stride, NULL, &sad_vec[i]);
      } else if (M == 32) {
        sad32x4(srcp, src_stride, refp[i], ref_stride, 0, &sad_vec[i]);
      } else if (M == 16) {
        sad16x4(srcp, src_stride, refp[i], ref_stride, 0, &sad_vec[i]);
      } else {
        assert(0);
      }
      srcp += src_stride << shift_for_rows;
      refp[i] += ref_stride << shift_for_rows;
    }
  }
  get_4d_sad_from_mm256_epi32(sad_vec, sad_array);
}

#define HIGHBD_SAD_MXNX4D_AVX2(m, n)                                          \
  void aom_highbd_sad##m##x##n##x4d_avx2(                                     \
      const uint8_t *src, int src_stride, const uint8_t *const ref_array[4],  \
      int ref_stride, uint32_t sad_array[4]) {                                \
    aom_highbd_sadMxNxD_avx2(m, n, 4, src, src_stride, ref_array, ref_stride, \
                             sad_array);                                      \
  }
#define HIGHBD_SAD_SKIP_MXNX4D_AVX2(m, n)                                    \
  void aom_highbd_sad_skip_##m##x##n##x4d_avx2(                              \
      const uint8_t *src, int src_stride, const uint8_t *const ref_array[4], \
      int ref_stride, uint32_t sad_array[4]) {                               \
    aom_highbd_sadMxNxD_avx2(m, (n / 2), 4, src, 2 * src_stride, ref_array,  \
                             2 * ref_stride, sad_array);                     \
    sad_array[0] <<= 1;                                                      \
    sad_array[1] <<= 1;                                                      \
    sad_array[2] <<= 1;                                                      \
    sad_array[3] <<= 1;                                                      \
  }
#define HIGHBD_SAD_MXNX3D_AVX2(m, n)                                          \
  void aom_highbd_sad##m##x##n##x3d_avx2(                                     \
      const uint8_t *src, int src_stride, const uint8_t *const ref_array[4],  \
      int ref_stride, uint32_t sad_array[4]) {                                \
    aom_highbd_sadMxNxD_avx2(m, n, 3, src, src_stride, ref_array, ref_stride, \
                             sad_array);                                      \
  }

HIGHBD_SAD_MXNX4D_AVX2(16, 8)
HIGHBD_SAD_MXNX4D_AVX2(16, 16)
HIGHBD_SAD_MXNX4D_AVX2(16, 32)

HIGHBD_SAD_MXNX4D_AVX2(32, 16)
HIGHBD_SAD_MXNX4D_AVX2(32, 32)
HIGHBD_SAD_MXNX4D_AVX2(32, 64)

HIGHBD_SAD_MXNX4D_AVX2(64, 32)
HIGHBD_SAD_MXNX4D_AVX2(64, 64)
HIGHBD_SAD_MXNX4D_AVX2(64, 128)

HIGHBD_SAD_MXNX4D_AVX2(128, 64)
HIGHBD_SAD_MXNX4D_AVX2(128, 128)

#if !CONFIG_REALTIME_ONLY
HIGHBD_SAD_MXNX4D_AVX2(16, 4)
HIGHBD_SAD_MXNX4D_AVX2(16, 64)
HIGHBD_SAD_MXNX4D_AVX2(32, 8)
HIGHBD_SAD_MXNX4D_AVX2(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

HIGHBD_SAD_SKIP_MXNX4D_AVX2(16, 8)
HIGHBD_SAD_SKIP_MXNX4D_AVX2(16, 16)
HIGHBD_SAD_SKIP_MXNX4D_AVX2(16, 32)

HIGHBD_SAD_SKIP_MXNX4D_AVX2(32, 16)
HIGHBD_SAD_SKIP_MXNX4D_AVX2(32, 32)
HIGHBD_SAD_SKIP_MXNX4D_AVX2(32, 64)

HIGHBD_SAD_SKIP_MXNX4D_AVX2(64, 32)
HIGHBD_SAD_SKIP_MXNX4D_AVX2(64, 64)
HIGHBD_SAD_SKIP_MXNX4D_AVX2(64, 128)

HIGHBD_SAD_SKIP_MXNX4D_AVX2(128, 64)
HIGHBD_SAD_SKIP_MXNX4D_AVX2(128, 128)

#if !CONFIG_REALTIME_ONLY
HIGHBD_SAD_SKIP_MXNX4D_AVX2(16, 64)
HIGHBD_SAD_SKIP_MXNX4D_AVX2(32, 8)
HIGHBD_SAD_SKIP_MXNX4D_AVX2(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

HIGHBD_SAD_MXNX3D_AVX2(16, 8)
HIGHBD_SAD_MXNX3D_AVX2(16, 16)
HIGHBD_SAD_MXNX3D_AVX2(16, 32)

HIGHBD_SAD_MXNX3D_AVX2(32, 16)
HIGHBD_SAD_MXNX3D_AVX2(32, 32)
HIGHBD_SAD_MXNX3D_AVX2(32, 64)

HIGHBD_SAD_MXNX3D_AVX2(64, 32)
HIGHBD_SAD_MXNX3D_AVX2(64, 64)
HIGHBD_SAD_MXNX3D_AVX2(64, 128)

HIGHBD_SAD_MXNX3D_AVX2(128, 64)
HIGHBD_SAD_MXNX3D_AVX2(128, 128)

#if !CONFIG_REALTIME_ONLY
HIGHBD_SAD_MXNX3D_AVX2(16, 4)
HIGHBD_SAD_MXNX3D_AVX2(16, 64)
HIGHBD_SAD_MXNX3D_AVX2(32, 8)
HIGHBD_SAD_MXNX3D_AVX2(64, 16)
#endif  // !CONFIG_REALTIME_ONLY
