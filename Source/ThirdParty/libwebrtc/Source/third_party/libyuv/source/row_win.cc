/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/row.h"
#include "libyuv/convert_from_argb.h"  // For ArgbConstants

// This module is for Visual C 32/64 bit
#if !defined(LIBYUV_DISABLE_X86) && \
    (defined(__x86_64__) || defined(__i386__) || \
     defined(_M_X64) || defined(_M_X86)) && \
    ((defined(_MSC_VER) && !defined(__clang__)) || \
     defined(LIBYUV_ENABLE_ROWWIN))

#include <emmintrin.h>
#include <tmmintrin.h>  // For _mm_maddubs_epi16
#include <immintrin.h>  // For AVX2 intrinsics

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Read 8 UV from 444
#define READYUV444                                    \
  xmm3 = _mm_loadl_epi64((__m128i*)u_buf);            \
  xmm1 = _mm_loadl_epi64((__m128i*)(u_buf + offset)); \
  xmm3 = _mm_unpacklo_epi8(xmm3, xmm1);               \
  u_buf += 8;                                         \
  xmm4 = _mm_loadl_epi64((__m128i*)y_buf);            \
  xmm4 = _mm_unpacklo_epi8(xmm4, xmm4);               \
  y_buf += 8;

// Read 8 UV from 444, With 8 Alpha.
#define READYUVA444                                   \
  xmm3 = _mm_loadl_epi64((__m128i*)u_buf);            \
  xmm1 = _mm_loadl_epi64((__m128i*)(u_buf + offset)); \
  xmm3 = _mm_unpacklo_epi8(xmm3, xmm1);               \
  u_buf += 8;                                         \
  xmm4 = _mm_loadl_epi64((__m128i*)y_buf);            \
  xmm4 = _mm_unpacklo_epi8(xmm4, xmm4);               \
  y_buf += 8;                                         \
  xmm5 = _mm_loadl_epi64((__m128i*)a_buf);            \
  a_buf += 8;

// Read 4 UV from 422, upsample to 8 UV.
#define READYUV422                                        \
  xmm3 = _mm_cvtsi32_si128(*(uint32_t*)u_buf);            \
  xmm1 = _mm_cvtsi32_si128(*(uint32_t*)(u_buf + offset)); \
  xmm3 = _mm_unpacklo_epi8(xmm3, xmm1);                   \
  xmm3 = _mm_unpacklo_epi16(xmm3, xmm3);                  \
  u_buf += 4;                                             \
  xmm4 = _mm_loadl_epi64((__m128i*)y_buf);                \
  xmm4 = _mm_unpacklo_epi8(xmm4, xmm4);                   \
  y_buf += 8;

// Read 4 UV from 422, upsample to 8 UV.  With 8 Alpha.
#define READYUVA422                                       \
  xmm3 = _mm_cvtsi32_si128(*(uint32_t*)u_buf);            \
  xmm1 = _mm_cvtsi32_si128(*(uint32_t*)(u_buf + offset)); \
  xmm3 = _mm_unpacklo_epi8(xmm3, xmm1);                   \
  xmm3 = _mm_unpacklo_epi16(xmm3, xmm3);                  \
  u_buf += 4;                                             \
  xmm4 = _mm_loadl_epi64((__m128i*)y_buf);                \
  xmm4 = _mm_unpacklo_epi8(xmm4, xmm4);                   \
  y_buf += 8;                                             \
  xmm5 = _mm_loadl_epi64((__m128i*)a_buf);                \
  a_buf += 8;

// Convert 8 pixels: 8 UV and 8 Y.
#define YUVTORGB(yuvconstants)                                      \
  xmm3 = _mm_sub_epi8(xmm3, _mm_set1_epi8((char)0x80));             \
  xmm4 = _mm_mulhi_epu16(xmm4, *(__m128i*)yuvconstants->kYToRgb);   \
  xmm4 = _mm_add_epi16(xmm4, *(__m128i*)yuvconstants->kYBiasToRgb); \
  xmm0 = _mm_maddubs_epi16(*(__m128i*)yuvconstants->kUVToB, xmm3);  \
  xmm1 = _mm_maddubs_epi16(*(__m128i*)yuvconstants->kUVToG, xmm3);  \
  xmm2 = _mm_maddubs_epi16(*(__m128i*)yuvconstants->kUVToR, xmm3);  \
  xmm0 = _mm_adds_epi16(xmm4, xmm0);                                \
  xmm1 = _mm_subs_epi16(xmm4, xmm1);                                \
  xmm2 = _mm_adds_epi16(xmm4, xmm2);                                \
  xmm0 = _mm_srai_epi16(xmm0, 6);                                   \
  xmm1 = _mm_srai_epi16(xmm1, 6);                                   \
  xmm2 = _mm_srai_epi16(xmm2, 6);                                   \
  xmm0 = _mm_packus_epi16(xmm0, xmm0);                              \
  xmm1 = _mm_packus_epi16(xmm1, xmm1);                              \
  xmm2 = _mm_packus_epi16(xmm2, xmm2);

// Store 8 ARGB values.
#define STOREARGB                                    \
  xmm0 = _mm_unpacklo_epi8(xmm0, xmm1);              \
  xmm2 = _mm_unpacklo_epi8(xmm2, xmm5);              \
  xmm1 = _mm_loadu_si128(&xmm0);                     \
  xmm0 = _mm_unpacklo_epi16(xmm0, xmm2);             \
  xmm1 = _mm_unpackhi_epi16(xmm1, xmm2);             \
  _mm_storeu_si128((__m128i*)dst_argb, xmm0);        \
  _mm_storeu_si128((__m128i*)(dst_argb + 16), xmm1); \
  dst_argb += 32;

#if defined(HAS_ARGBTOYMATRIXROW_AVX2)

#if defined(__clang__) || defined(__GNUC__)
#define LIBYUV_TARGET_AVX2 __attribute__((target("avx2")))
#define LIBYUV_TARGET_AVX512BW \
  __attribute__((target("avx512bw,avx512vl,avx512f")))
#else
#define LIBYUV_TARGET_AVX2
#define LIBYUV_TARGET_AVX512BW
#endif

// Convert 32 ARGB pixels (128 bytes) to 32 UV444 values.
#if defined(HAS_ARGBTOYMATRIXROW_AVX2) || defined(HAS_ARGBTOUV444MATRIXROW_AVX2)
LIBYUV_TARGET_AVX2
void ARGBToUV444MatrixRow_AVX2(const uint8_t* src_argb,
                               uint8_t* dst_u,
                               uint8_t* dst_v,
                               int width,
                               const struct ArgbConstants* c) {
  __m256i ymm_u =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)c->kRGBToU));
  __m256i ymm_v =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)c->kRGBToV));
  __m256i ymm5 = _mm256_set1_epi16((short)0x8000);
  __m256i perm_mask = _mm256_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7);

  while (width > 0) {
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src_argb);
    __m256i ymm1 = _mm256_loadu_si256((const __m256i*)(src_argb + 32));
    __m256i ymm2 = _mm256_loadu_si256((const __m256i*)(src_argb + 64));
    __m256i ymm3 = _mm256_loadu_si256((const __m256i*)(src_argb + 96));
    src_argb += 128;

    __m256i ymm0_u = _mm256_maddubs_epi16(ymm0, ymm_u);
    __m256i ymm1_u = _mm256_maddubs_epi16(ymm1, ymm_u);
    __m256i ymm2_u = _mm256_maddubs_epi16(ymm2, ymm_u);
    __m256i ymm3_u = _mm256_maddubs_epi16(ymm3, ymm_u);

    __m256i ymm0_v = _mm256_maddubs_epi16(ymm0, ymm_v);
    __m256i ymm1_v = _mm256_maddubs_epi16(ymm1, ymm_v);
    __m256i ymm2_v = _mm256_maddubs_epi16(ymm2, ymm_v);
    __m256i ymm3_v = _mm256_maddubs_epi16(ymm3, ymm_v);

    ymm0_u = _mm256_hadd_epi16(ymm0_u, ymm1_u);
    ymm2_u = _mm256_hadd_epi16(ymm2_u, ymm3_u);

    ymm0_v = _mm256_hadd_epi16(ymm0_v, ymm1_v);
    ymm2_v = _mm256_hadd_epi16(ymm2_v, ymm3_v);

    ymm0_u = _mm256_sub_epi16(ymm5, ymm0_u);
    ymm2_u = _mm256_sub_epi16(ymm5, ymm2_u);

    ymm0_v = _mm256_sub_epi16(ymm5, ymm0_v);
    ymm2_v = _mm256_sub_epi16(ymm5, ymm2_v);

    ymm0_u = _mm256_srli_epi16(ymm0_u, 8);
    ymm2_u = _mm256_srli_epi16(ymm2_u, 8);

    ymm0_v = _mm256_srli_epi16(ymm0_v, 8);
    ymm2_v = _mm256_srli_epi16(ymm2_v, 8);

    ymm0_u = _mm256_packus_epi16(ymm0_u, ymm2_u);
    ymm0_u = _mm256_permutevar8x32_epi32(ymm0_u, perm_mask);

    ymm0_v = _mm256_packus_epi16(ymm0_v, ymm2_v);
    ymm0_v = _mm256_permutevar8x32_epi32(ymm0_v, perm_mask);

    _mm256_storeu_si256((__m256i*)dst_u, ymm0_u);
    _mm256_storeu_si256((__m256i*)dst_v, ymm0_v);
    dst_u += 32;
    dst_v += 32;
    width -= 32;
  }
}
#endif
LIBYUV_TARGET_AVX2
void ARGBToYMatrixRow_AVX2(const uint8_t* src_argb,
                           uint8_t* dst_y,
                           int width,
                           const struct ArgbConstants* c) {
  __m256i ymm5 = _mm256_set1_epi8((char)0x80);
  __m256i ymm4 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)c->kRGBToY));
  __m256i ymm7 =
      _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)c->kAddY));
  __m256i ymm6 = _mm256_maddubs_epi16(ymm4, ymm5);
  ymm6 = _mm256_hadd_epi16(ymm6, ymm6);
  ymm7 = _mm256_sub_epi16(ymm7, ymm6);
  __m256i perm_mask = _mm256_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7);

  while (width > 0) {
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src_argb);
    __m256i ymm1 = _mm256_loadu_si256((const __m256i*)(src_argb + 32));
    __m256i ymm2 = _mm256_loadu_si256((const __m256i*)(src_argb + 64));
    __m256i ymm3 = _mm256_loadu_si256((const __m256i*)(src_argb + 96));
    src_argb += 128;

    ymm0 = _mm256_sub_epi8(ymm0, ymm5);
    ymm1 = _mm256_sub_epi8(ymm1, ymm5);
    ymm2 = _mm256_sub_epi8(ymm2, ymm5);
    ymm3 = _mm256_sub_epi8(ymm3, ymm5);

    ymm0 = _mm256_maddubs_epi16(ymm4, ymm0);
    ymm1 = _mm256_maddubs_epi16(ymm4, ymm1);
    ymm2 = _mm256_maddubs_epi16(ymm4, ymm2);
    ymm3 = _mm256_maddubs_epi16(ymm4, ymm3);

    ymm0 = _mm256_hadd_epi16(ymm0, ymm1);
    ymm2 = _mm256_hadd_epi16(ymm2, ymm3);

    ymm0 = _mm256_add_epi16(ymm0, ymm7);
    ymm2 = _mm256_add_epi16(ymm2, ymm7);

    ymm0 = _mm256_srli_epi16(ymm0, 8);
    ymm2 = _mm256_srli_epi16(ymm2, 8);

    ymm0 = _mm256_packus_epi16(ymm0, ymm2);
    ymm0 = _mm256_permutevar8x32_epi32(ymm0, perm_mask);

    _mm256_storeu_si256((__m256i*)dst_y, ymm0);
    dst_y += 32;
    width -= 32;
  }
}

LIBYUV_TARGET_AVX2
void ARGBToYRow_AVX2(const uint8_t* src_argb, uint8_t* dst_y, int width) {
  ARGBToYMatrixRow_AVX2(src_argb, dst_y, width, &kArgbI601Constants);
}

LIBYUV_TARGET_AVX2
void ABGRToYRow_AVX2(const uint8_t* src_abgr, uint8_t* dst_y, int width) {
  ARGBToYMatrixRow_AVX2(src_abgr, dst_y, width, &kAbgrI601Constants);
}

LIBYUV_TARGET_AVX2
void ARGBToYJRow_AVX2(const uint8_t* src_argb, uint8_t* dst_y, int width) {
  ARGBToYMatrixRow_AVX2(src_argb, dst_y, width, &kArgbJPEGConstants);
}

LIBYUV_TARGET_AVX2
void ABGRToYJRow_AVX2(const uint8_t* src_abgr, uint8_t* dst_y, int width) {
  ARGBToYMatrixRow_AVX2(src_abgr, dst_y, width, &kAbgrJPEGConstants);
}

LIBYUV_TARGET_AVX2
void RGBAToYJRow_AVX2(const uint8_t* src_rgba, uint8_t* dst_y, int width) {
  ARGBToYMatrixRow_AVX2(src_rgba, dst_y, width, &kRgbaJPEGConstants);
}

LIBYUV_TARGET_AVX2
void RGBAToYRow_AVX2(const uint8_t* src_rgba, uint8_t* dst_y, int width) {
  ARGBToYMatrixRow_AVX2(src_rgba, dst_y, width, &kRgbaI601Constants);
}

LIBYUV_TARGET_AVX2
void BGRAToYRow_AVX2(const uint8_t* src_bgra, uint8_t* dst_y, int width) {
  ARGBToYMatrixRow_AVX2(src_bgra, dst_y, width, &kBgraI601Constants);
}

#ifdef HAS_RAWTOARGBROW_AVX2
LIBYUV_TARGET_AVX2
void RAWToARGBRow_AVX2(const uint8_t* src_raw, uint8_t* dst_argb, int width) {
  __m256i ymm_alpha = _mm256_set1_epi32(0xff000000);
  __m128i shuf_low = _mm_set_epi8(-1, 9, 10, 11, -1, 6, 7, 8, -1, 3, 4, 5, -1, 0, 1, 2);
  __m128i shuf_high = _mm_set_epi8(-1, 13, 14, 15, -1, 10, 11, 12, -1, 7, 8, 9, -1, 4, 5, 6);
  __m256i ymm_shuf = _mm256_broadcastsi128_si256(shuf_low);
  __m256i ymm_shuf2 = _mm256_broadcastsi128_si256(shuf_high);

  while (width > 0) {
    __m128i xmm0 = _mm_loadu_si128((const __m128i*)src_raw);
    __m256i ymm0 = _mm256_castsi128_si256(xmm0);
    ymm0 = _mm256_inserti128_si256(ymm0, _mm_loadu_si128((const __m128i*)(src_raw + 12)), 1);

    __m128i xmm1 = _mm_loadu_si128((const __m128i*)(src_raw + 24));
    __m256i ymm1 = _mm256_castsi128_si256(xmm1);
    ymm1 = _mm256_inserti128_si256(ymm1, _mm_loadu_si128((const __m128i*)(src_raw + 36)), 1);

    __m128i xmm2 = _mm_loadu_si128((const __m128i*)(src_raw + 48));
    __m256i ymm2 = _mm256_castsi128_si256(xmm2);
    ymm2 = _mm256_inserti128_si256(ymm2, _mm_loadu_si128((const __m128i*)(src_raw + 60)), 1);

    __m128i xmm3 = _mm_loadu_si128((const __m128i*)(src_raw + 68));
    __m256i ymm3 = _mm256_castsi128_si256(xmm3);
    ymm3 = _mm256_inserti128_si256(ymm3, _mm_loadu_si128((const __m128i*)(src_raw + 80)), 1);

    ymm0 = _mm256_shuffle_epi8(ymm0, ymm_shuf);
    ymm1 = _mm256_shuffle_epi8(ymm1, ymm_shuf);
    ymm2 = _mm256_shuffle_epi8(ymm2, ymm_shuf);
    ymm3 = _mm256_shuffle_epi8(ymm3, ymm_shuf2);

    ymm0 = _mm256_or_si256(ymm0, ymm_alpha);
    ymm1 = _mm256_or_si256(ymm1, ymm_alpha);
    ymm2 = _mm256_or_si256(ymm2, ymm_alpha);
    ymm3 = _mm256_or_si256(ymm3, ymm_alpha);

    _mm256_storeu_si256((__m256i*)dst_argb, ymm0);
    _mm256_storeu_si256((__m256i*)(dst_argb + 32), ymm1);
    _mm256_storeu_si256((__m256i*)(dst_argb + 64), ymm2);
    _mm256_storeu_si256((__m256i*)(dst_argb + 96), ymm3);

    src_raw += 96;
    dst_argb += 128;
    width -= 32;
  }
}
#endif

#ifdef HAS_RAWTOARGBROW_AVX512BW
LIBYUV_TARGET_AVX512BW
void RGBToARGBRow_AVX512BW(const uint8_t* src_raw, uint8_t* dst_argb, const __m128i* shuffler, int width) {
  __m512i zmm_alpha = _mm512_set1_epi32(0xff000000);
  __m512i zmm_perm = _mm512_set_epi32(
      12, 11, 10, 9, 9, 8, 7, 6, 6, 5, 4, 3, 3, 2, 1, 0);
  __m512i zmm_shuf = _mm512_broadcast_i32x4(_mm_loadu_si128(shuffler));

  while (width > 0) {
    __m512i zmm0 = _mm512_maskz_loadu_epi8(0xffffffffffffull, src_raw);
    __m512i zmm1 = _mm512_maskz_loadu_epi8(0xffffffffffffull, src_raw + 48);
    __m512i zmm2 = _mm512_maskz_loadu_epi8(0xffffffffffffull, src_raw + 96);
    __m512i zmm3 = _mm512_maskz_loadu_epi8(0xffffffffffffull, src_raw + 144);

    zmm0 = _mm512_permutexvar_epi32(zmm_perm, zmm0);
    zmm1 = _mm512_permutexvar_epi32(zmm_perm, zmm1);
    zmm2 = _mm512_permutexvar_epi32(zmm_perm, zmm2);
    zmm3 = _mm512_permutexvar_epi32(zmm_perm, zmm3);

    zmm0 = _mm512_shuffle_epi8(zmm0, zmm_shuf);
    zmm1 = _mm512_shuffle_epi8(zmm1, zmm_shuf);
    zmm2 = _mm512_shuffle_epi8(zmm2, zmm_shuf);
    zmm3 = _mm512_shuffle_epi8(zmm3, zmm_shuf);

    zmm0 = _mm512_or_si512(zmm0, zmm_alpha);
    zmm1 = _mm512_or_si512(zmm1, zmm_alpha);
    zmm2 = _mm512_or_si512(zmm2, zmm_alpha);
    zmm3 = _mm512_or_si512(zmm3, zmm_alpha);

    _mm512_storeu_si512(dst_argb, zmm0);
    _mm512_storeu_si512(dst_argb + 64, zmm1);
    _mm512_storeu_si512(dst_argb + 128, zmm2);
    _mm512_storeu_si512(dst_argb + 192, zmm3);

    src_raw += 192;
    dst_argb += 256;
    width -= 64;
  }
}

LIBYUV_TARGET_AVX512BW
void RAWToARGBRow_AVX512BW(const uint8_t* src_raw, uint8_t* dst_argb, int width) {
  __m128i shuf = _mm_set_epi8(-1, 9, 10, 11, -1, 6, 7, 8, -1, 3, 4, 5, -1, 0, 1, 2);
  RGBToARGBRow_AVX512BW(src_raw, dst_argb, &shuf, width);
}

LIBYUV_TARGET_AVX512BW
void RGB24ToARGBRow_AVX512BW(const uint8_t* src_rgb24, uint8_t* dst_argb, int width) {
  __m128i shuf = _mm_set_epi8(-1, 11, 10, 9, -1, 8, 7, 6, -1, 5, 4, 3, -1, 2, 1, 0);
  RGBToARGBRow_AVX512BW(src_rgb24, dst_argb, &shuf, width);
}
#endif

#ifdef HAS_ARGBTOUVMATRIXROW_AVX2
LIBYUV_TARGET_AVX2 __attribute__((no_sanitize("cfi-icall")))
void ARGBToUVMatrixRow_AVX2(const uint8_t* src_argb,
                            int src_stride_argb,
                            uint8_t* dst_u,
                            uint8_t* dst_v,
                            int width,
                            const struct ArgbConstants* c) {
  __m256i ymm_u = _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)c->kRGBToU));
  __m256i ymm_v = _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)c->kRGBToV));
  __m256i ymm_0101 = _mm256_set1_epi16(0x0101);
  __m256i ymm_shuf = _mm256_setr_epi8(0, 4, 1, 5, 2, 6, 3, 7, 8, 12, 9, 13, 10, 14, 11, 15,
                                      0, 4, 1, 5, 2, 6, 3, 7, 8, 12, 9, 13, 10, 14, 11, 15);
  __m256i ymm_8000 = _mm256_set1_epi16((short)0x8000);
  __m256i ymm_zero = _mm256_setzero_si256();

  while (width > 0) {
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src_argb);
    __m256i ymm1 = _mm256_loadu_si256((const __m256i*)(src_argb + 32));
    __m256i ymm2 = _mm256_loadu_si256((const __m256i*)(src_argb + src_stride_argb));
    __m256i ymm3 = _mm256_loadu_si256((const __m256i*)(src_argb + src_stride_argb + 32));

    ymm0 = _mm256_shuffle_epi8(ymm0, ymm_shuf);
    ymm1 = _mm256_shuffle_epi8(ymm1, ymm_shuf);
    ymm2 = _mm256_shuffle_epi8(ymm2, ymm_shuf);
    ymm3 = _mm256_shuffle_epi8(ymm3, ymm_shuf);

    ymm0 = _mm256_maddubs_epi16(ymm0, ymm_0101);
    ymm1 = _mm256_maddubs_epi16(ymm1, ymm_0101);
    ymm2 = _mm256_maddubs_epi16(ymm2, ymm_0101);
    ymm3 = _mm256_maddubs_epi16(ymm3, ymm_0101);

    ymm0 = _mm256_add_epi16(ymm0, ymm2);
    ymm1 = _mm256_add_epi16(ymm1, ymm3);

    ymm0 = _mm256_srli_epi16(ymm0, 1);
    ymm1 = _mm256_srli_epi16(ymm1, 1);
    ymm0 = _mm256_avg_epu16(ymm0, ymm_zero);
    ymm1 = _mm256_avg_epu16(ymm1, ymm_zero);

    ymm0 = _mm256_packus_epi16(ymm0, ymm1);
    ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);

    ymm1 = _mm256_maddubs_epi16(ymm0, ymm_v);
    ymm0 = _mm256_maddubs_epi16(ymm0, ymm_u);

    ymm0 = _mm256_hadd_epi16(ymm0, ymm1);
    ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);
    ymm0 = _mm256_sub_epi16(ymm_8000, ymm0);
    ymm0 = _mm256_srli_epi16(ymm0, 8);
    ymm0 = _mm256_packus_epi16(ymm0, ymm0);

    __m128i xmm_u = _mm256_castsi256_si128(ymm0);
    __m128i xmm_v = _mm256_extracti128_si256(ymm0, 1);

    _mm_storel_epi64((__m128i*)dst_u, xmm_u);
    _mm_storel_epi64((__m128i*)dst_v, xmm_v);

    src_argb += 64;
    dst_u += 8;
    dst_v += 8;
    width -= 16;
  }
}
#endif

#ifdef HAS_MERGEUVROW_AVX2
LIBYUV_TARGET_AVX2
void MergeUVRow_AVX2(const uint8_t* src_u,
                     const uint8_t* src_v,
                     uint8_t* dst_uv,
                     int width) {
  while (width > 0) {
    __m256i ymm0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)src_u));
    __m256i ymm1 = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)src_v));

    ymm1 = _mm256_slli_epi16(ymm1, 8);
    ymm0 = _mm256_or_si256(ymm0, ymm1);

    _mm256_storeu_si256((__m256i*)dst_uv, ymm0);

    src_u += 16;
    src_v += 16;
    dst_uv += 32;
    width -= 16;
  }
}
#endif

#ifdef HAS_MIRRORROW_AVX2
LIBYUV_TARGET_AVX2
void MirrorRow_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  __m256i ymm_shuf =
      _mm256_broadcastsi128_si256(_mm_setr_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0));
  src += width;
  while (width > 0) {
    src -= 32;
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src);
    ymm0 = _mm256_shuffle_epi8(ymm0, ymm_shuf);
    ymm0 = _mm256_permute4x64_epi64(ymm0, 0x4e);
    _mm256_storeu_si256((__m256i*)dst, ymm0);
    dst += 32;
    width -= 32;
  }
}
#endif

#ifdef HAS_MIRRORUVROW_AVX2
LIBYUV_TARGET_AVX2
void MirrorUVRow_AVX2(const uint8_t* src_uv, uint8_t* dst_uv, int width) {
  __m256i ymm_shuf =
      _mm256_broadcastsi128_si256(_mm_setr_epi8(14, 15, 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1));
  src_uv += width * 2;
  while (width > 0) {
    src_uv -= 32;
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src_uv);
    ymm0 = _mm256_shuffle_epi8(ymm0, ymm_shuf);
    ymm0 = _mm256_permute4x64_epi64(ymm0, 0x4e);
    _mm256_storeu_si256((__m256i*)dst_uv, ymm0);
    dst_uv += 32;
    width -= 16;
  }
}
#endif

#ifdef HAS_MIRRORSPLITUVROW_AVX2
LIBYUV_TARGET_AVX2
void MirrorSplitUVRow_AVX2(const uint8_t* src_uv,
                           uint8_t* dst_u,
                           uint8_t* dst_v,
                           int width) {
  __m256i ymm_shuf =
      _mm256_broadcastsi128_si256(_mm_setr_epi8(14, 12, 10, 8, 6, 4, 2, 0, 15, 13, 11, 9, 7, 5, 3, 1));
  src_uv += width * 2;
  while (width > 0) {
    src_uv -= 32;
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src_uv);
    ymm0 = _mm256_shuffle_epi8(ymm0, ymm_shuf);
    ymm0 = _mm256_permute4x64_epi64(ymm0, 0x72);
    _mm_storeu_si128((__m128i*)dst_u, _mm256_castsi256_si128(ymm0));
    _mm_storeu_si128((__m128i*)dst_v, _mm256_extracti128_si256(ymm0, 1));
    dst_u += 16;
    dst_v += 16;
    width -= 16;
  }
}
#endif

#ifdef HAS_RGB24MIRRORROW_AVX2
LIBYUV_TARGET_AVX2
void RGB24MirrorRow_AVX2(const uint8_t* src_rgb24,
                         uint8_t* dst_rgb24,
                         int width) {
  __m256i shuf0 = _mm256_setr_epi8(
      -1, 12, 13, 14, 9, 10, 11, 6, 7, 8, 3, 4, 5, 0, 1, 2,
      -1, 12, 13, 14, 9, 10, 11, 6, 7, 8, 3, 4, 5, 0, 1, 2);
  __m128i shuf1 = _mm_setr_epi8(
      13, 14, 15, 10, 11, 12, 7, 8, 9, 4, 5, 6, 1, 2, 3, -1);

  src_rgb24 += width * 3 - 96;
  while (width > 0) {
    __m128i v0_lo = _mm_loadu_si128((const __m128i*)(src_rgb24 + 0));
    __m128i v0_hi = _mm_loadu_si128((const __m128i*)(src_rgb24 + 15));
    __m256i v0 = _mm256_inserti128_si256(_mm256_castsi128_si256(v0_lo), v0_hi, 1);

    __m128i v1_lo = _mm_loadu_si128((const __m128i*)(src_rgb24 + 30));
    __m128i v1_hi = _mm_loadu_si128((const __m128i*)(src_rgb24 + 45));
    __m256i v1 = _mm256_inserti128_si256(_mm256_castsi128_si256(v1_lo), v1_hi, 1);

    __m128i v2_lo = _mm_loadu_si128((const __m128i*)(src_rgb24 + 60));
    __m128i v2_hi = _mm_loadu_si128((const __m128i*)(src_rgb24 + 75));
    __m256i v2 = _mm256_inserti128_si256(_mm256_castsi128_si256(v2_lo), v2_hi, 1);

    __m128i v3 = _mm_loadu_si128((const __m128i*)(src_rgb24 + 80));

    v0 = _mm256_shuffle_epi8(v0, shuf0);
    v1 = _mm256_shuffle_epi8(v1, shuf0);
    v2 = _mm256_shuffle_epi8(v2, shuf0);
    v3 = _mm_shuffle_epi8(v3, shuf1);

    _mm_storeu_si128((__m128i*)(dst_rgb24 + 80), _mm256_castsi256_si128(v0));
    _mm_storeu_si128((__m128i*)(dst_rgb24 + 65), _mm256_extracti128_si256(v0, 1));
    _mm_storeu_si128((__m128i*)(dst_rgb24 + 50), _mm256_castsi256_si128(v1));
    _mm_storeu_si128((__m128i*)(dst_rgb24 + 35), _mm256_extracti128_si256(v1, 1));
    _mm_storeu_si128((__m128i*)(dst_rgb24 + 20), _mm256_castsi256_si128(v2));
    _mm_storeu_si128((__m128i*)(dst_rgb24 + 5), _mm256_extracti128_si256(v2, 1));
    _mm_storel_epi64((__m128i*)(dst_rgb24 + 0), v3);

    src_rgb24 -= 96;
    dst_rgb24 += 96;
    width -= 32;
  }
}
#endif

#ifdef HAS_INTERPOLATEROW_AVX2
LIBYUV_TARGET_AVX2
void InterpolateRow_AVX2(uint8_t* dst_ptr,
                         const uint8_t* src_ptr,
                         ptrdiff_t src_stride,
                         int width,
                         int source_y_fraction) {
  int y1 = source_y_fraction;
  int y0 = 256 - y1;
  const uint8_t* src_ptr1 = src_ptr + src_stride;
  __m256i ymm_y = _mm256_set1_epi16((y1 << 8) | y0);
  __m256i ymm_8080 = _mm256_set1_epi16(0x8080);
  int i;

  if (y1 == 0) {
    for (i = 0; i < width; i += 32) {
      _mm256_storeu_si256((__m256i*)(dst_ptr + i),
                          _mm256_loadu_si256((const __m256i*)(src_ptr + i)));
    }
  } else if (y1 == 128) {
    for (i = 0; i < width; i += 32) {
      __m256i row0 = _mm256_loadu_si256((const __m256i*)(src_ptr + i));
      __m256i row1 = _mm256_loadu_si256((const __m256i*)(src_ptr1 + i));
      _mm256_storeu_si256((__m256i*)(dst_ptr + i), _mm256_avg_epu8(row0, row1));
    }
  } else {
    for (i = 0; i < width; i += 32) {
      __m256i row0 = _mm256_loadu_si256((const __m256i*)(src_ptr + i));
      __m256i row1 = _mm256_loadu_si256((const __m256i*)(src_ptr1 + i));
      __m256i low = _mm256_unpacklo_epi8(row0, row1);
      __m256i high = _mm256_unpackhi_epi8(row0, row1);
      low = _mm256_sub_epi8(low, ymm_8080);
      high = _mm256_sub_epi8(high, ymm_8080);
      low = _mm256_maddubs_epi16(ymm_y, low);
      high = _mm256_maddubs_epi16(ymm_y, high);
      low = _mm256_add_epi16(low, ymm_8080);
      high = _mm256_add_epi16(high, ymm_8080);
      low = _mm256_srli_epi16(low, 8);
      high = _mm256_srli_epi16(high, 8);
      _mm256_storeu_si256((__m256i*)(dst_ptr + i),
                          _mm256_packus_epi16(low, high));
    }
  }
  _mm256_zeroupper();
}
#endif

#ifdef HAS_INTERPOLATEROW_16_AVX2
LIBYUV_TARGET_AVX2
void InterpolateRow_16_AVX2(uint16_t* dst_ptr,
                            const uint16_t* src_ptr,
                            ptrdiff_t src_stride,
                            int width,
                            int source_y_fraction) {
  int y1 = source_y_fraction;
  int y0 = 256 - y1;
  const uint16_t* src_ptr1 = src_ptr + src_stride;
  __m256i ymm_y = _mm256_set1_epi32((y1 << 16) | y0);
  __m256i ymm_8000 = _mm256_set1_epi16((short)0x8000);
  __m256i ymm_round = _mm256_set1_epi32(8388736);  // 0x800000 + 128
  int i;

  if (y1 == 0) {
    for (i = 0; i < width; i += 16) {
      _mm256_storeu_si256((__m256i*)(dst_ptr + i),
                          _mm256_loadu_si256((const __m256i*)(src_ptr + i)));
    }
  } else if (y1 == 128) {
    for (i = 0; i < width; i += 16) {
      __m256i row0 = _mm256_loadu_si256((const __m256i*)(src_ptr + i));
      __m256i row1 = _mm256_loadu_si256((const __m256i*)(src_ptr1 + i));
      _mm256_storeu_si256((__m256i*)(dst_ptr + i), _mm256_avg_epu16(row0, row1));
    }
  } else {
    for (i = 0; i < width; i += 16) {
      __m256i row0 = _mm256_loadu_si256((const __m256i*)(src_ptr + i));
      __m256i row1 = _mm256_loadu_si256((const __m256i*)(src_ptr1 + i));
      __m256i row0l = _mm256_unpacklo_epi16(row0, row1);
      __m256i row0h = _mm256_unpackhi_epi16(row0, row1);
      row0l = _mm256_sub_epi16(row0l, ymm_8000);
      row0h = _mm256_sub_epi16(row0h, ymm_8000);
      __m256i resl = _mm256_madd_epi16(row0l, ymm_y);
      __m256i resh = _mm256_madd_epi16(row0h, ymm_y);
      resl = _mm256_add_epi32(resl, ymm_round);
      resh = _mm256_add_epi32(resh, ymm_round);
      resl = _mm256_srai_epi32(resl, 8);
      resh = _mm256_srai_epi32(resh, 8);
      _mm256_storeu_si256((__m256i*)(dst_ptr + i),
                          _mm256_packus_epi32(resl, resh));
    }
  }
  _mm256_zeroupper();
}
#endif

#ifdef HAS_ARGBMIRRORROW_AVX2
LIBYUV_TARGET_AVX2
void ARGBMirrorRow_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  __m256i ymm_shuf = _mm256_setr_epi32(7, 6, 5, 4, 3, 2, 1, 0);
  src += width * 4;
  while (width > 0) {
    src -= 32;
    __m256i ymm0 = _mm256_loadu_si256((const __m256i*)src);
    ymm0 = _mm256_permutevar8x32_epi32(ymm0, ymm_shuf);
    _mm256_storeu_si256((__m256i*)dst, ymm0);
    dst += 32;
    width -= 8;
  }
}
#endif

#ifdef HAS_J400TOARGBROW_AVX2
alignas(32) static const uint8_t kShuffleMaskJ400ToARGB_0[32] = {
    0u, 0u, 0u, 128u, 1u, 1u, 1u, 128u, 2u, 2u, 2u, 128u, 3u, 3u, 3u, 128u,
    4u, 4u, 4u, 128u, 5u, 5u, 5u, 128u, 6u, 6u, 6u, 128u, 7u, 7u, 7u, 128u
};
alignas(32) static const uint8_t kShuffleMaskJ400ToARGB_1[32] = {
    8u, 8u, 8u, 128u, 9u, 9u, 9u, 128u, 10u, 10u, 10u, 128u, 11u, 11u, 11u, 128u,
    12u, 12u, 12u, 128u, 13u, 13u, 13u, 128u, 14u, 14u, 14u, 128u, 15u, 15u, 15u, 128u
};

LIBYUV_TARGET_AVX2
void J400ToARGBRow_AVX2(const uint8_t* src_y, uint8_t* dst_argb, int width) {
  __m256i ymm_mask0 = _mm256_load_si256((const __m256i*)kShuffleMaskJ400ToARGB_0);
  __m256i ymm_mask1 = _mm256_load_si256((const __m256i*)kShuffleMaskJ400ToARGB_1);
  __m256i ymm_alpha = _mm256_set1_epi32((int)0xff000000u);

  while (width > 0) {
    __m256i ymm0 = _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)src_y));

    __m256i ymm1 = _mm256_shuffle_epi8(ymm0, ymm_mask0);
    __m256i ymm2 = _mm256_shuffle_epi8(ymm0, ymm_mask1);

    ymm1 = _mm256_or_si256(ymm1, ymm_alpha);
    ymm2 = _mm256_or_si256(ymm2, ymm_alpha);

    _mm256_storeu_si256((__m256i*)dst_argb, ymm1);
    _mm256_storeu_si256((__m256i*)(dst_argb + 32), ymm2);

    src_y += 16;
    dst_argb += 64;
    width -= 16;
  }
}
#endif  // HAS_J400TOARGBROW_AVX2

#endif

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_X86) && (defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_X86)) && ((defined(_MSC_VER) && !defined(__clang__)) || defined(LIBYUV_ENABLE_ROWWIN))
