/*
 *  Copyright 2012 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/row.h"

#include <string.h>  // For memset.

#include "libyuv/basic_types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// memset for temp is meant to clear the source buffer (not dest) so that
// SIMD that reads full multiple of 16 bytes will not trigger msan errors.
// memset is not needed for production, as the garbage values are processed but
// not used, although there may be edge cases for subsampling.
// The size of the buffer is based on the largest read, which can be inferred
// by the source type (e.g. ARGB) and the mask (last parameter), or by examining
// the source code for how much the source pointers are advanced.

// Subsampled source needs to be increase by 1 of not even.
#define SS(width, shift) (((width) + (1 << (shift)) - 1) >> (shift))

// Any 4 planes to 1
#define ANY41(NAMEANY, ANY_SIMD, UVSHIFT, DUVSHIFT, BPP, MASK)               \
  void NAMEANY(const uint8_t* y_buf, const uint8_t* u_buf,                   \
               const uint8_t* v_buf, const uint8_t* a_buf, uint8_t* dst_ptr, \
               int width) {                                                  \
    SIMD_ALIGNED(uint8_t temp[64 * 5]);                                      \
    memset(temp, 0, 64 * 4); /* for msan */                                  \
    int r = width & MASK;                                                    \
    int n = width & ~MASK;                                                   \
    if (n > 0) {                                                             \
      ANY_SIMD(y_buf, u_buf, v_buf, a_buf, dst_ptr, n);                      \
    }                                                                        \
    memcpy(temp, y_buf + n, r);                                              \
    memcpy(temp + 64, u_buf + (n >> UVSHIFT), SS(r, UVSHIFT));               \
    memcpy(temp + 128, v_buf + (n >> UVSHIFT), SS(r, UVSHIFT));              \
    memcpy(temp + 192, a_buf + n, r);                                        \
    ANY_SIMD(temp, temp + 64, temp + 128, temp + 192, temp + 256, MASK + 1); \
    memcpy(dst_ptr + (n >> DUVSHIFT) * BPP, temp + 256,                      \
           SS(r, DUVSHIFT) * BPP);                                           \
  }

#ifdef HAS_MERGEARGBROW_SSE2
ANY41(MergeARGBRow_Any_SSE2, MergeARGBRow_SSE2, 0, 0, 4, 7)
#endif
#ifdef HAS_MERGEARGBROW_AVX2
ANY41(MergeARGBRow_Any_AVX2, MergeARGBRow_AVX2, 0, 0, 4, 15)
#endif
#ifdef HAS_MERGEARGBROW_NEON
ANY41(MergeARGBRow_Any_NEON, MergeARGBRow_NEON, 0, 0, 4, 15)
#endif

// Note that odd width replication includes 444 due to implementation
// on arm that subsamples 444 to 422 internally.
// Any 4 planes to 1 with yuvconstants
#define ANY41C(NAMEANY, ANY_SIMD, UVSHIFT, DUVSHIFT, BPP, MASK)              \
  void NAMEANY(const uint8_t* y_buf, const uint8_t* u_buf,                   \
               const uint8_t* v_buf, const uint8_t* a_buf, uint8_t* dst_ptr, \
               const struct YuvConstants* yuvconstants, int width) {         \
    SIMD_ALIGNED(uint8_t temp[64 * 5]);                                      \
    memset(temp, 0, 64 * 4); /* for msan */                                  \
    int r = width & MASK;                                                    \
    int n = width & ~MASK;                                                   \
    if (n > 0) {                                                             \
      ANY_SIMD(y_buf, u_buf, v_buf, a_buf, dst_ptr, yuvconstants, n);        \
    }                                                                        \
    memcpy(temp, y_buf + n, r);                                              \
    memcpy(temp + 64, u_buf + (n >> UVSHIFT), SS(r, UVSHIFT));               \
    memcpy(temp + 128, v_buf + (n >> UVSHIFT), SS(r, UVSHIFT));              \
    memcpy(temp + 192, a_buf + n, r);                                        \
    if (width & 1) {                                                         \
      temp[64 + SS(r, UVSHIFT)] = temp[64 + SS(r, UVSHIFT) - 1];             \
      temp[128 + SS(r, UVSHIFT)] = temp[128 + SS(r, UVSHIFT) - 1];           \
    }                                                                        \
    ANY_SIMD(temp, temp + 64, temp + 128, temp + 192, temp + 256,            \
             yuvconstants, MASK + 1);                                        \
    memcpy(dst_ptr + (n >> DUVSHIFT) * BPP, temp + 256,                      \
           SS(r, DUVSHIFT) * BPP);                                           \
  }

#ifdef HAS_I444ALPHATOARGBROW_SSSE3
ANY41C(I444AlphaToARGBRow_Any_SSSE3, I444AlphaToARGBRow_SSSE3, 0, 0, 4, 7)
#endif
#ifdef HAS_I444ALPHATOARGBROW_AVX2
ANY41C(I444AlphaToARGBRow_Any_AVX2, I444AlphaToARGBRow_AVX2, 0, 0, 4, 15)
#endif
#ifdef HAS_I422ALPHATOARGBROW_SSSE3
ANY41C(I422AlphaToARGBRow_Any_SSSE3, I422AlphaToARGBRow_SSSE3, 1, 0, 4, 7)
#endif
#ifdef HAS_I422ALPHATOARGBROW_AVX2
ANY41C(I422AlphaToARGBRow_Any_AVX2, I422AlphaToARGBRow_AVX2, 1, 0, 4, 15)
#endif
#ifdef HAS_I444ALPHATOARGBROW_NEON
ANY41C(I444AlphaToARGBRow_Any_NEON, I444AlphaToARGBRow_NEON, 0, 0, 4, 7)
#endif
#ifdef HAS_I422ALPHATOARGBROW_NEON
ANY41C(I422AlphaToARGBRow_Any_NEON, I422AlphaToARGBRow_NEON, 1, 0, 4, 7)
#endif
#ifdef HAS_I444ALPHATOARGBROW_MSA
ANY41C(I444AlphaToARGBRow_Any_MSA, I444AlphaToARGBRow_MSA, 0, 0, 4, 7)
#endif
#ifdef HAS_I422ALPHATOARGBROW_MSA
ANY41C(I422AlphaToARGBRow_Any_MSA, I422AlphaToARGBRow_MSA, 1, 0, 4, 7)
#endif
#ifdef HAS_I444ALPHATOARGBROW_MMI
ANY41C(I444AlphaToARGBRow_Any_MMI, I444AlphaToARGBRow_MMI, 0, 0, 4, 7)
#endif
#ifdef HAS_I422ALPHATOARGBROW_MMI
ANY41C(I422AlphaToARGBRow_Any_MMI, I422AlphaToARGBRow_MMI, 1, 0, 4, 7)
#endif
#undef ANY41C

// Any 4 planes to 1 plane of 8 bit with yuvconstants
#define ANY41CT(NAMEANY, ANY_SIMD, UVSHIFT, DUVSHIFT, T, SBPP, BPP, MASK)      \
  void NAMEANY(const T* y_buf, const T* u_buf, const T* v_buf, const T* a_buf, \
               uint8_t* dst_ptr, const struct YuvConstants* yuvconstants,      \
               int width) {                                                    \
    SIMD_ALIGNED(T temp[16 * 4]);                                              \
    SIMD_ALIGNED(uint8_t out[64]);                                             \
    memset(temp, 0, 16 * 4 * SBPP); /* for YUY2 and msan */                    \
    int r = width & MASK;                                                      \
    int n = width & ~MASK;                                                     \
    if (n > 0) {                                                               \
      ANY_SIMD(y_buf, u_buf, v_buf, a_buf, dst_ptr, yuvconstants, n);          \
    }                                                                          \
    memcpy(temp, y_buf + n, r * SBPP);                                         \
    memcpy(temp + 16, u_buf + (n >> UVSHIFT), SS(r, UVSHIFT) * SBPP);          \
    memcpy(temp + 32, v_buf + (n >> UVSHIFT), SS(r, UVSHIFT) * SBPP);          \
    memcpy(temp + 48, a_buf + n, r * SBPP);                                    \
    ANY_SIMD(temp, temp + 16, temp + 32, temp + 48, out, yuvconstants,         \
             MASK + 1);                                                        \
    memcpy(dst_ptr + (n >> DUVSHIFT) * BPP, out, SS(r, DUVSHIFT) * BPP);       \
  }

#ifdef HAS_I210ALPHATOARGBROW_SSSE3
ANY41CT(I210AlphaToARGBRow_Any_SSSE3,
        I210AlphaToARGBRow_SSSE3,
        1,
        0,
        uint16_t,
        2,
        4,
        7)
#endif

#ifdef HAS_I210ALPHATOARGBROW_AVX2
ANY41CT(I210AlphaToARGBRow_Any_AVX2,
        I210AlphaToARGBRow_AVX2,
        1,
        0,
        uint16_t,
        2,
        4,
        15)
#endif

#ifdef HAS_I410ALPHATOARGBROW_SSSE3
ANY41CT(I410AlphaToARGBRow_Any_SSSE3,
        I410AlphaToARGBRow_SSSE3,
        0,
        0,
        uint16_t,
        2,
        4,
        7)
#endif

#ifdef HAS_I410ALPHATOARGBROW_AVX2
ANY41CT(I410AlphaToARGBRow_Any_AVX2,
        I410AlphaToARGBRow_AVX2,
        0,
        0,
        uint16_t,
        2,
        4,
        15)
#endif

#undef ANY41CT

// Any 4 planes to 1 plane with parameter
#define ANY41PT(NAMEANY, ANY_SIMD, STYPE, SBPP, DTYPE, BPP, MASK)          \
  void NAMEANY(const STYPE* r_buf, const STYPE* g_buf, const STYPE* b_buf, \
               const STYPE* a_buf, DTYPE* dst_ptr, int depth, int width) { \
    SIMD_ALIGNED(STYPE temp[16 * 4]);                                      \
    SIMD_ALIGNED(DTYPE out[64]);                                           \
    memset(temp, 0, 16 * 4 * SBPP); /* for YUY2 and msan */                \
    int r = width & MASK;                                                  \
    int n = width & ~MASK;                                                 \
    if (n > 0) {                                                           \
      ANY_SIMD(r_buf, g_buf, b_buf, a_buf, dst_ptr, depth, n);             \
    }                                                                      \
    memcpy(temp, r_buf + n, r * SBPP);                                     \
    memcpy(temp + 16, g_buf + n, r * SBPP);                                \
    memcpy(temp + 32, b_buf + n, r * SBPP);                                \
    memcpy(temp + 48, a_buf + n, r * SBPP);                                \
    ANY_SIMD(temp, temp + 16, temp + 32, temp + 48, out, depth, MASK + 1); \
    memcpy((uint8_t*)dst_ptr + n * BPP, out, r * BPP);                     \
  }

#ifdef HAS_MERGEAR64ROW_AVX2
ANY41PT(MergeAR64Row_Any_AVX2, MergeAR64Row_AVX2, uint16_t, 2, uint16_t, 8, 15)
#endif

#ifdef HAS_MERGEAR64ROW_NEON
ANY41PT(MergeAR64Row_Any_NEON, MergeAR64Row_NEON, uint16_t, 2, uint16_t, 8, 7)
#endif

#ifdef HAS_MERGEARGB16TO8ROW_AVX2
ANY41PT(MergeARGB16To8Row_Any_AVX2,
        MergeARGB16To8Row_AVX2,
        uint16_t,
        2,
        uint8_t,
        4,
        15)
#endif

#ifdef HAS_MERGEARGB16TO8ROW_NEON
ANY41PT(MergeARGB16To8Row_Any_NEON,
        MergeARGB16To8Row_NEON,
        uint16_t,
        2,
        uint8_t,
        4,
        7)
#endif

#undef ANY41PT

// Any 3 planes to 1.
#define ANY31(NAMEANY, ANY_SIMD, UVSHIFT, DUVSHIFT, BPP, MASK)      \
  void NAMEANY(const uint8_t* y_buf, const uint8_t* u_buf,          \
               const uint8_t* v_buf, uint8_t* dst_ptr, int width) { \
    SIMD_ALIGNED(uint8_t temp[64 * 4]);                             \
    memset(temp, 0, 64 * 3); /* for YUY2 and msan */                \
    int r = width & MASK;                                           \
    int n = width & ~MASK;                                          \
    if (n > 0) {                                                    \
      ANY_SIMD(y_buf, u_buf, v_buf, dst_ptr, n);                    \
    }                                                               \
    memcpy(temp, y_buf + n, r);                                     \
    memcpy(temp + 64, u_buf + (n >> UVSHIFT), SS(r, UVSHIFT));      \
    memcpy(temp + 128, v_buf + (n >> UVSHIFT), SS(r, UVSHIFT));     \
    ANY_SIMD(temp, temp + 64, temp + 128, temp + 192, MASK + 1);    \
    memcpy(dst_ptr + (n >> DUVSHIFT) * BPP, temp + 192,             \
           SS(r, DUVSHIFT) * BPP);                                  \
  }

// Merge functions.
#ifdef HAS_MERGERGBROW_SSSE3
ANY31(MergeRGBRow_Any_SSSE3, MergeRGBRow_SSSE3, 0, 0, 3, 15)
#endif
#ifdef HAS_MERGERGBROW_NEON
ANY31(MergeRGBRow_Any_NEON, MergeRGBRow_NEON, 0, 0, 3, 15)
#endif
#ifdef HAS_MERGERGBROW_MMI
ANY31(MergeRGBRow_Any_MMI, MergeRGBRow_MMI, 0, 0, 3, 7)
#endif
#ifdef HAS_MERGEXRGBROW_SSE2
ANY31(MergeXRGBRow_Any_SSE2, MergeXRGBRow_SSE2, 0, 0, 4, 7)
#endif
#ifdef HAS_MERGEXRGBROW_AVX2
ANY31(MergeXRGBRow_Any_AVX2, MergeXRGBRow_AVX2, 0, 0, 4, 15)
#endif
#ifdef HAS_MERGEXRGBROW_NEON
ANY31(MergeXRGBRow_Any_NEON, MergeXRGBRow_NEON, 0, 0, 4, 15)
#endif
#ifdef HAS_I422TOYUY2ROW_SSE2
ANY31(I422ToYUY2Row_Any_SSE2, I422ToYUY2Row_SSE2, 1, 1, 4, 15)
ANY31(I422ToUYVYRow_Any_SSE2, I422ToUYVYRow_SSE2, 1, 1, 4, 15)
#endif
#ifdef HAS_I422TOYUY2ROW_AVX2
ANY31(I422ToYUY2Row_Any_AVX2, I422ToYUY2Row_AVX2, 1, 1, 4, 31)
ANY31(I422ToUYVYRow_Any_AVX2, I422ToUYVYRow_AVX2, 1, 1, 4, 31)
#endif
#ifdef HAS_I422TOYUY2ROW_NEON
ANY31(I422ToYUY2Row_Any_NEON, I422ToYUY2Row_NEON, 1, 1, 4, 15)
#endif
#ifdef HAS_I422TOYUY2ROW_MSA
ANY31(I422ToYUY2Row_Any_MSA, I422ToYUY2Row_MSA, 1, 1, 4, 31)
#endif
#ifdef HAS_I422TOYUY2ROW_MMI
ANY31(I422ToYUY2Row_Any_MMI, I422ToYUY2Row_MMI, 1, 1, 4, 7)
#endif
#ifdef HAS_I422TOUYVYROW_NEON
ANY31(I422ToUYVYRow_Any_NEON, I422ToUYVYRow_NEON, 1, 1, 4, 15)
#endif
#ifdef HAS_I422TOUYVYROW_MSA
ANY31(I422ToUYVYRow_Any_MSA, I422ToUYVYRow_MSA, 1, 1, 4, 31)
#endif
#ifdef HAS_I422TOUYVYROW_MMI
ANY31(I422ToUYVYRow_Any_MMI, I422ToUYVYRow_MMI, 1, 1, 4, 7)
#endif
#ifdef HAS_BLENDPLANEROW_AVX2
ANY31(BlendPlaneRow_Any_AVX2, BlendPlaneRow_AVX2, 0, 0, 1, 31)
#endif
#ifdef HAS_BLENDPLANEROW_SSSE3
ANY31(BlendPlaneRow_Any_SSSE3, BlendPlaneRow_SSSE3, 0, 0, 1, 7)
#endif
#ifdef HAS_BLENDPLANEROW_MMI
ANY31(BlendPlaneRow_Any_MMI, BlendPlaneRow_MMI, 0, 0, 1, 7)
#endif
#undef ANY31

// Note that odd width replication includes 444 due to implementation
// on arm that subsamples 444 to 422 internally.
// Any 3 planes to 1 with yuvconstants
#define ANY31C(NAMEANY, ANY_SIMD, UVSHIFT, DUVSHIFT, BPP, MASK)      \
  void NAMEANY(const uint8_t* y_buf, const uint8_t* u_buf,           \
               const uint8_t* v_buf, uint8_t* dst_ptr,               \
               const struct YuvConstants* yuvconstants, int width) { \
    SIMD_ALIGNED(uint8_t temp[128 * 4]);                             \
    memset(temp, 0, 128 * 3); /* for YUY2 and msan */                \
    int r = width & MASK;                                            \
    int n = width & ~MASK;                                           \
    if (n > 0) {                                                     \
      ANY_SIMD(y_buf, u_buf, v_buf, dst_ptr, yuvconstants, n);       \
    }                                                                \
    memcpy(temp, y_buf + n, r);                                      \
    memcpy(temp + 128, u_buf + (n >> UVSHIFT), SS(r, UVSHIFT));      \
    memcpy(temp + 256, v_buf + (n >> UVSHIFT), SS(r, UVSHIFT));      \
    if (width & 1) {                                                 \
      temp[128 + SS(r, UVSHIFT)] = temp[128 + SS(r, UVSHIFT) - 1];   \
      temp[256 + SS(r, UVSHIFT)] = temp[256 + SS(r, UVSHIFT) - 1];   \
    }                                                                \
    ANY_SIMD(temp, temp + 128, temp + 256, temp + 384, yuvconstants, \
             MASK + 1);                                              \
    memcpy(dst_ptr + (n >> DUVSHIFT) * BPP, temp + 384,              \
           SS(r, DUVSHIFT) * BPP);                                   \
  }

#ifdef HAS_I422TOARGBROW_SSSE3
ANY31C(I422ToARGBRow_Any_SSSE3, I422ToARGBRow_SSSE3, 1, 0, 4, 7)
#endif
#ifdef HAS_I422TORGBAROW_SSSE3
ANY31C(I422ToRGBARow_Any_SSSE3, I422ToRGBARow_SSSE3, 1, 0, 4, 7)
#endif
#ifdef HAS_I422TOARGB4444ROW_SSSE3
ANY31C(I422ToARGB4444Row_Any_SSSE3, I422ToARGB4444Row_SSSE3, 1, 0, 2, 7)
#endif
#ifdef HAS_I422TOARGB1555ROW_SSSE3
ANY31C(I422ToARGB1555Row_Any_SSSE3, I422ToARGB1555Row_SSSE3, 1, 0, 2, 7)
#endif
#ifdef HAS_I422TORGB565ROW_SSSE3
ANY31C(I422ToRGB565Row_Any_SSSE3, I422ToRGB565Row_SSSE3, 1, 0, 2, 7)
#endif
#ifdef HAS_I422TORGB24ROW_SSSE3
ANY31C(I422ToRGB24Row_Any_SSSE3, I422ToRGB24Row_SSSE3, 1, 0, 3, 15)
#endif
#ifdef HAS_I422TOAR30ROW_SSSE3
ANY31C(I422ToAR30Row_Any_SSSE3, I422ToAR30Row_SSSE3, 1, 0, 4, 7)
#endif
#ifdef HAS_I422TOAR30ROW_AVX2
ANY31C(I422ToAR30Row_Any_AVX2, I422ToAR30Row_AVX2, 1, 0, 4, 15)
#endif
#ifdef HAS_I444TOARGBROW_SSSE3
ANY31C(I444ToARGBRow_Any_SSSE3, I444ToARGBRow_SSSE3, 0, 0, 4, 7)
#endif
#ifdef HAS_I422TORGB24ROW_AVX2
ANY31C(I422ToRGB24Row_Any_AVX2, I422ToRGB24Row_AVX2, 1, 0, 3, 31)
#endif
#ifdef HAS_I422TOARGBROW_AVX2
ANY31C(I422ToARGBRow_Any_AVX2, I422ToARGBRow_AVX2, 1, 0, 4, 15)
#endif
#ifdef HAS_I422TORGBAROW_AVX2
ANY31C(I422ToRGBARow_Any_AVX2, I422ToRGBARow_AVX2, 1, 0, 4, 15)
#endif
#ifdef HAS_I444TOARGBROW_AVX2
ANY31C(I444ToARGBRow_Any_AVX2, I444ToARGBRow_AVX2, 0, 0, 4, 15)
#endif
#ifdef HAS_I422TOARGB4444ROW_AVX2
ANY31C(I422ToARGB4444Row_Any_AVX2, I422ToARGB4444Row_AVX2, 1, 0, 2, 15)
#endif
#ifdef HAS_I422TOARGB1555ROW_AVX2
ANY31C(I422ToARGB1555Row_Any_AVX2, I422ToARGB1555Row_AVX2, 1, 0, 2, 15)
#endif
#ifdef HAS_I422TORGB565ROW_AVX2
ANY31C(I422ToRGB565Row_Any_AVX2, I422ToRGB565Row_AVX2, 1, 0, 2, 15)
#endif
#ifdef HAS_I422TOARGBROW_NEON
ANY31C(I444ToARGBRow_Any_NEON, I444ToARGBRow_NEON, 0, 0, 4, 7)
ANY31C(I422ToARGBRow_Any_NEON, I422ToARGBRow_NEON, 1, 0, 4, 7)
ANY31C(I422ToRGBARow_Any_NEON, I422ToRGBARow_NEON, 1, 0, 4, 7)
ANY31C(I422ToRGB24Row_Any_NEON, I422ToRGB24Row_NEON, 1, 0, 3, 7)
ANY31C(I422ToARGB4444Row_Any_NEON, I422ToARGB4444Row_NEON, 1, 0, 2, 7)
ANY31C(I422ToARGB1555Row_Any_NEON, I422ToARGB1555Row_NEON, 1, 0, 2, 7)
ANY31C(I422ToRGB565Row_Any_NEON, I422ToRGB565Row_NEON, 1, 0, 2, 7)
#endif
#ifdef HAS_I422TOARGBROW_MSA
ANY31C(I444ToARGBRow_Any_MSA, I444ToARGBRow_MSA, 0, 0, 4, 7)
ANY31C(I422ToARGBRow_Any_MSA, I422ToARGBRow_MSA, 1, 0, 4, 7)
ANY31C(I422ToRGBARow_Any_MSA, I422ToRGBARow_MSA, 1, 0, 4, 7)
ANY31C(I422ToRGB24Row_Any_MSA, I422ToRGB24Row_MSA, 1, 0, 3, 15)
ANY31C(I422ToARGB4444Row_Any_MSA, I422ToARGB4444Row_MSA, 1, 0, 2, 7)
ANY31C(I422ToARGB1555Row_Any_MSA, I422ToARGB1555Row_MSA, 1, 0, 2, 7)
ANY31C(I422ToRGB565Row_Any_MSA, I422ToRGB565Row_MSA, 1, 0, 2, 7)
#endif
#ifdef HAS_I422TOARGBROW_MMI
ANY31C(I444ToARGBRow_Any_MMI, I444ToARGBRow_MMI, 0, 0, 4, 7)
ANY31C(I422ToARGBRow_Any_MMI, I422ToARGBRow_MMI, 1, 0, 4, 7)
ANY31C(I422ToRGB24Row_Any_MMI, I422ToRGB24Row_MMI, 1, 0, 3, 15)
ANY31C(I422ToARGB4444Row_Any_MMI, I422ToARGB4444Row_MMI, 1, 0, 2, 7)
ANY31C(I422ToARGB1555Row_Any_MMI, I422ToARGB1555Row_MMI, 1, 0, 2, 7)
ANY31C(I422ToRGB565Row_Any_MMI, I422ToRGB565Row_MMI, 1, 0, 2, 7)
ANY31C(I422ToRGBARow_Any_MMI, I422ToRGBARow_MMI, 1, 0, 4, 7)
#endif
#undef ANY31C

// Any 3 planes of 16 bit to 1 with yuvconstants
// TODO(fbarchard): consider sharing this code with ANY31C
#define ANY31CT(NAMEANY, ANY_SIMD, UVSHIFT, DUVSHIFT, T, SBPP, BPP, MASK) \
  void NAMEANY(const T* y_buf, const T* u_buf, const T* v_buf,            \
               uint8_t* dst_ptr, const struct YuvConstants* yuvconstants, \
               int width) {                                               \
    SIMD_ALIGNED(T temp[16 * 3]);                                         \
    SIMD_ALIGNED(uint8_t out[64]);                                        \
    memset(temp, 0, 16 * 3 * SBPP); /* for YUY2 and msan */               \
    int r = width & MASK;                                                 \
    int n = width & ~MASK;                                                \
    if (n > 0) {                                                          \
      ANY_SIMD(y_buf, u_buf, v_buf, dst_ptr, yuvconstants, n);            \
    }                                                                     \
    memcpy(temp, y_buf + n, r * SBPP);                                    \
    memcpy(temp + 16, u_buf + (n >> UVSHIFT), SS(r, UVSHIFT) * SBPP);     \
    memcpy(temp + 32, v_buf + (n >> UVSHIFT), SS(r, UVSHIFT) * SBPP);     \
    ANY_SIMD(temp, temp + 16, temp + 32, out, yuvconstants, MASK + 1);    \
    memcpy(dst_ptr + (n >> DUVSHIFT) * BPP, out, SS(r, DUVSHIFT) * BPP);  \
  }

#ifdef HAS_I210TOAR30ROW_SSSE3
ANY31CT(I210ToAR30Row_Any_SSSE3, I210ToAR30Row_SSSE3, 1, 0, uint16_t, 2, 4, 7)
#endif
#ifdef HAS_I210TOARGBROW_SSSE3
ANY31CT(I210ToARGBRow_Any_SSSE3, I210ToARGBRow_SSSE3, 1, 0, uint16_t, 2, 4, 7)
#endif
#ifdef HAS_I210TOARGBROW_AVX2
ANY31CT(I210ToARGBRow_Any_AVX2, I210ToARGBRow_AVX2, 1, 0, uint16_t, 2, 4, 15)
#endif
#ifdef HAS_I210TOAR30ROW_AVX2
ANY31CT(I210ToAR30Row_Any_AVX2, I210ToAR30Row_AVX2, 1, 0, uint16_t, 2, 4, 15)
#endif
#ifdef HAS_I410TOAR30ROW_SSSE3
ANY31CT(I410ToAR30Row_Any_SSSE3, I410ToAR30Row_SSSE3, 0, 0, uint16_t, 2, 4, 7)
#endif
#ifdef HAS_I410TOARGBROW_SSSE3
ANY31CT(I410ToARGBRow_Any_SSSE3, I410ToARGBRow_SSSE3, 0, 0, uint16_t, 2, 4, 7)
#endif
#ifdef HAS_I410TOARGBROW_AVX2
ANY31CT(I410ToARGBRow_Any_AVX2, I410ToARGBRow_AVX2, 0, 0, uint16_t, 2, 4, 15)
#endif
#ifdef HAS_I410TOAR30ROW_AVX2
ANY31CT(I410ToAR30Row_Any_AVX2, I410ToAR30Row_AVX2, 0, 0, uint16_t, 2, 4, 15)
#endif
#ifdef HAS_I210TOARGBROW_MMI
ANY31CT(I210ToARGBRow_Any_MMI, I210ToARGBRow_MMI, 1, 0, uint16_t, 2, 4, 7)
#endif
#ifdef HAS_I212TOAR30ROW_SSSE3
ANY31CT(I212ToAR30Row_Any_SSSE3, I212ToAR30Row_SSSE3, 1, 0, uint16_t, 2, 4, 7)
#endif
#ifdef HAS_I212TOARGBROW_SSSE3
ANY31CT(I212ToARGBRow_Any_SSSE3, I212ToARGBRow_SSSE3, 1, 0, uint16_t, 2, 4, 7)
#endif
#ifdef HAS_I212TOARGBROW_AVX2
ANY31CT(I212ToARGBRow_Any_AVX2, I212ToARGBRow_AVX2, 1, 0, uint16_t, 2, 4, 15)
#endif
#ifdef HAS_I212TOAR30ROW_AVX2
ANY31CT(I212ToAR30Row_Any_AVX2, I212ToAR30Row_AVX2, 1, 0, uint16_t, 2, 4, 15)
#endif
#undef ANY31CT

// Any 3 planes to 1 plane with parameter
#define ANY31PT(NAMEANY, ANY_SIMD, STYPE, SBPP, DTYPE, BPP, MASK)          \
  void NAMEANY(const STYPE* r_buf, const STYPE* g_buf, const STYPE* b_buf, \
               DTYPE* dst_ptr, int depth, int width) {                     \
    SIMD_ALIGNED(STYPE temp[16 * 3]);                                      \
    SIMD_ALIGNED(DTYPE out[64]);                                           \
    memset(temp, 0, 16 * 3 * SBPP); /* for YUY2 and msan */                \
    int r = width & MASK;                                                  \
    int n = width & ~MASK;                                                 \
    if (n > 0) {                                                           \
      ANY_SIMD(r_buf, g_buf, b_buf, dst_ptr, depth, n);                    \
    }                                                                      \
    memcpy(temp, r_buf + n, r * SBPP);                                     \
    memcpy(temp + 16, g_buf + n, r * SBPP);                                \
    memcpy(temp + 32, b_buf + n, r * SBPP);                                \
    ANY_SIMD(temp, temp + 16, temp + 32, out, depth, MASK + 1);            \
    memcpy((uint8_t*)dst_ptr + n * BPP, out, r * BPP);                     \
  }

#ifdef HAS_MERGEXR30ROW_AVX2
ANY31PT(MergeXR30Row_Any_AVX2, MergeXR30Row_AVX2, uint16_t, 2, uint8_t, 4, 15)
#endif

#ifdef HAS_MERGEXR30ROW_NEON
ANY31PT(MergeXR30Row_Any_NEON, MergeXR30Row_NEON, uint16_t, 2, uint8_t, 4, 3)
ANY31PT(MergeXR30Row_10_Any_NEON,
        MergeXR30Row_10_NEON,
        uint16_t,
        2,
        uint8_t,
        4,
        3)
#endif

#ifdef HAS_MERGEXR64ROW_AVX2
ANY31PT(MergeXR64Row_Any_AVX2, MergeXR64Row_AVX2, uint16_t, 2, uint16_t, 8, 15)
#endif

#ifdef HAS_MERGEXR64ROW_NEON
ANY31PT(MergeXR64Row_Any_NEON, MergeXR64Row_NEON, uint16_t, 2, uint16_t, 8, 7)
#endif

#ifdef HAS_MERGEXRGB16TO8ROW_AVX2
ANY31PT(MergeXRGB16To8Row_Any_AVX2,
        MergeXRGB16To8Row_AVX2,
        uint16_t,
        2,
        uint8_t,
        4,
        15)
#endif

#ifdef HAS_MERGEXRGB16TO8ROW_NEON
ANY31PT(MergeXRGB16To8Row_Any_NEON,
        MergeXRGB16To8Row_NEON,
        uint16_t,
        2,
        uint8_t,
        4,
        7)
#endif

#undef ANY31PT

// Any 2 planes to 1.
#define ANY21(NAMEANY, ANY_SIMD, UVSHIFT, SBPP, SBPP2, BPP, MASK)             \
  void NAMEANY(const uint8_t* y_buf, const uint8_t* uv_buf, uint8_t* dst_ptr, \
               int width) {                                                   \
    SIMD_ALIGNED(uint8_t temp[64 * 3]);                                       \
    memset(temp, 0, 64 * 2); /* for msan */                                   \
    int r = width & MASK;                                                     \
    int n = width & ~MASK;                                                    \
    if (n > 0) {                                                              \
      ANY_SIMD(y_buf, uv_buf, dst_ptr, n);                                    \
    }                                                                         \
    memcpy(temp, y_buf + n * SBPP, r * SBPP);                                 \
    memcpy(temp + 64, uv_buf + (n >> UVSHIFT) * SBPP2,                        \
           SS(r, UVSHIFT) * SBPP2);                                           \
    ANY_SIMD(temp, temp + 64, temp + 128, MASK + 1);                          \
    memcpy(dst_ptr + n * BPP, temp + 128, r * BPP);                           \
  }

// Merge functions.
#ifdef HAS_MERGEUVROW_SSE2
ANY21(MergeUVRow_Any_SSE2, MergeUVRow_SSE2, 0, 1, 1, 2, 15)
#endif
#ifdef HAS_MERGEUVROW_AVX2
ANY21(MergeUVRow_Any_AVX2, MergeUVRow_AVX2, 0, 1, 1, 2, 31)
#endif
#ifdef HAS_MERGEUVROW_NEON
ANY21(MergeUVRow_Any_NEON, MergeUVRow_NEON, 0, 1, 1, 2, 15)
#endif
#ifdef HAS_MERGEUVROW_MSA
ANY21(MergeUVRow_Any_MSA, MergeUVRow_MSA, 0, 1, 1, 2, 15)
#endif
#ifdef HAS_MERGEUVROW_MMI
ANY21(MergeUVRow_Any_MMI, MergeUVRow_MMI, 0, 1, 1, 2, 7)
#endif
#ifdef HAS_NV21TOYUV24ROW_NEON
ANY21(NV21ToYUV24Row_Any_NEON, NV21ToYUV24Row_NEON, 1, 1, 2, 3, 15)
#endif
#ifdef HAS_NV21TOYUV24ROW_AVX2
ANY21(NV21ToYUV24Row_Any_AVX2, NV21ToYUV24Row_AVX2, 1, 1, 2, 3, 31)
#endif
// Math functions.
#ifdef HAS_ARGBMULTIPLYROW_SSE2
ANY21(ARGBMultiplyRow_Any_SSE2, ARGBMultiplyRow_SSE2, 0, 4, 4, 4, 3)
#endif
#ifdef HAS_ARGBADDROW_SSE2
ANY21(ARGBAddRow_Any_SSE2, ARGBAddRow_SSE2, 0, 4, 4, 4, 3)
#endif
#ifdef HAS_ARGBSUBTRACTROW_SSE2
ANY21(ARGBSubtractRow_Any_SSE2, ARGBSubtractRow_SSE2, 0, 4, 4, 4, 3)
#endif
#ifdef HAS_ARGBMULTIPLYROW_AVX2
ANY21(ARGBMultiplyRow_Any_AVX2, ARGBMultiplyRow_AVX2, 0, 4, 4, 4, 7)
#endif
#ifdef HAS_ARGBADDROW_AVX2
ANY21(ARGBAddRow_Any_AVX2, ARGBAddRow_AVX2, 0, 4, 4, 4, 7)
#endif
#ifdef HAS_ARGBSUBTRACTROW_AVX2
ANY21(ARGBSubtractRow_Any_AVX2, ARGBSubtractRow_AVX2, 0, 4, 4, 4, 7)
#endif
#ifdef HAS_ARGBMULTIPLYROW_NEON
ANY21(ARGBMultiplyRow_Any_NEON, ARGBMultiplyRow_NEON, 0, 4, 4, 4, 7)
#endif
#ifdef HAS_ARGBADDROW_NEON
ANY21(ARGBAddRow_Any_NEON, ARGBAddRow_NEON, 0, 4, 4, 4, 7)
#endif
#ifdef HAS_ARGBSUBTRACTROW_NEON
ANY21(ARGBSubtractRow_Any_NEON, ARGBSubtractRow_NEON, 0, 4, 4, 4, 7)
#endif
#ifdef HAS_ARGBMULTIPLYROW_MSA
ANY21(ARGBMultiplyRow_Any_MSA, ARGBMultiplyRow_MSA, 0, 4, 4, 4, 3)
#endif
#ifdef HAS_ARGBMULTIPLYROW_MMI
ANY21(ARGBMultiplyRow_Any_MMI, ARGBMultiplyRow_MMI, 0, 4, 4, 4, 1)
#endif
#ifdef HAS_ARGBADDROW_MSA
ANY21(ARGBAddRow_Any_MSA, ARGBAddRow_MSA, 0, 4, 4, 4, 7)
#endif
#ifdef HAS_ARGBADDROW_MMI
ANY21(ARGBAddRow_Any_MMI, ARGBAddRow_MMI, 0, 4, 4, 4, 1)
#endif
#ifdef HAS_ARGBSUBTRACTROW_MSA
ANY21(ARGBSubtractRow_Any_MSA, ARGBSubtractRow_MSA, 0, 4, 4, 4, 7)
#endif
#ifdef HAS_ARGBSUBTRACTROW_MMI
ANY21(ARGBSubtractRow_Any_MMI, ARGBSubtractRow_MMI, 0, 4, 4, 4, 1)
#endif
#ifdef HAS_SOBELROW_SSE2
ANY21(SobelRow_Any_SSE2, SobelRow_SSE2, 0, 1, 1, 4, 15)
#endif
#ifdef HAS_SOBELROW_NEON
ANY21(SobelRow_Any_NEON, SobelRow_NEON, 0, 1, 1, 4, 7)
#endif
#ifdef HAS_SOBELROW_MSA
ANY21(SobelRow_Any_MSA, SobelRow_MSA, 0, 1, 1, 4, 15)
#endif
#ifdef HAS_SOBELROW_MMI
ANY21(SobelRow_Any_MMI, SobelRow_MMI, 0, 1, 1, 4, 7)
#endif
#ifdef HAS_SOBELTOPLANEROW_SSE2
ANY21(SobelToPlaneRow_Any_SSE2, SobelToPlaneRow_SSE2, 0, 1, 1, 1, 15)
#endif
#ifdef HAS_SOBELTOPLANEROW_NEON
ANY21(SobelToPlaneRow_Any_NEON, SobelToPlaneRow_NEON, 0, 1, 1, 1, 15)
#endif
#ifdef HAS_SOBELTOPLANEROW_MSA
ANY21(SobelToPlaneRow_Any_MSA, SobelToPlaneRow_MSA, 0, 1, 1, 1, 31)
#endif
#ifdef HAS_SOBELTOPLANEROW_MMI
ANY21(SobelToPlaneRow_Any_MMI, SobelToPlaneRow_MMI, 0, 1, 1, 1, 7)
#endif
#ifdef HAS_SOBELXYROW_SSE2
ANY21(SobelXYRow_Any_SSE2, SobelXYRow_SSE2, 0, 1, 1, 4, 15)
#endif
#ifdef HAS_SOBELXYROW_NEON
ANY21(SobelXYRow_Any_NEON, SobelXYRow_NEON, 0, 1, 1, 4, 7)
#endif
#ifdef HAS_SOBELXYROW_MSA
ANY21(SobelXYRow_Any_MSA, SobelXYRow_MSA, 0, 1, 1, 4, 15)
#endif
#ifdef HAS_SOBELXYROW_MMI
ANY21(SobelXYRow_Any_MMI, SobelXYRow_MMI, 0, 1, 1, 4, 7)
#endif
#undef ANY21

// Any 2 planes to 1 with yuvconstants
#define ANY21C(NAMEANY, ANY_SIMD, UVSHIFT, SBPP, SBPP2, BPP, MASK)            \
  void NAMEANY(const uint8_t* y_buf, const uint8_t* uv_buf, uint8_t* dst_ptr, \
               const struct YuvConstants* yuvconstants, int width) {          \
    SIMD_ALIGNED(uint8_t temp[128 * 3]);                                      \
    memset(temp, 0, 128 * 2); /* for msan */                                  \
    int r = width & MASK;                                                     \
    int n = width & ~MASK;                                                    \
    if (n > 0) {                                                              \
      ANY_SIMD(y_buf, uv_buf, dst_ptr, yuvconstants, n);                      \
    }                                                                         \
    memcpy(temp, y_buf + n * SBPP, r * SBPP);                                 \
    memcpy(temp + 128, uv_buf + (n >> UVSHIFT) * SBPP2,                       \
           SS(r, UVSHIFT) * SBPP2);                                           \
    ANY_SIMD(temp, temp + 128, temp + 256, yuvconstants, MASK + 1);           \
    memcpy(dst_ptr + n * BPP, temp + 256, r * BPP);                           \
  }

// Biplanar to RGB.
#ifdef HAS_NV12TOARGBROW_SSSE3
ANY21C(NV12ToARGBRow_Any_SSSE3, NV12ToARGBRow_SSSE3, 1, 1, 2, 4, 7)
#endif
#ifdef HAS_NV12TOARGBROW_AVX2
ANY21C(NV12ToARGBRow_Any_AVX2, NV12ToARGBRow_AVX2, 1, 1, 2, 4, 15)
#endif
#ifdef HAS_NV12TOARGBROW_NEON
ANY21C(NV12ToARGBRow_Any_NEON, NV12ToARGBRow_NEON, 1, 1, 2, 4, 7)
#endif
#ifdef HAS_NV12TOARGBROW_MSA
ANY21C(NV12ToARGBRow_Any_MSA, NV12ToARGBRow_MSA, 1, 1, 2, 4, 7)
#endif
#ifdef HAS_NV12TOARGBROW_MMI
ANY21C(NV12ToARGBRow_Any_MMI, NV12ToARGBRow_MMI, 1, 1, 2, 4, 7)
#endif
#ifdef HAS_NV21TOARGBROW_SSSE3
ANY21C(NV21ToARGBRow_Any_SSSE3, NV21ToARGBRow_SSSE3, 1, 1, 2, 4, 7)
#endif
#ifdef HAS_NV21TOARGBROW_AVX2
ANY21C(NV21ToARGBRow_Any_AVX2, NV21ToARGBRow_AVX2, 1, 1, 2, 4, 15)
#endif
#ifdef HAS_NV21TOARGBROW_NEON
ANY21C(NV21ToARGBRow_Any_NEON, NV21ToARGBRow_NEON, 1, 1, 2, 4, 7)
#endif
#ifdef HAS_NV21TOARGBROW_MSA
ANY21C(NV21ToARGBRow_Any_MSA, NV21ToARGBRow_MSA, 1, 1, 2, 4, 7)
#endif
#ifdef HAS_NV21TOARGBROW_MMI
ANY21C(NV21ToARGBRow_Any_MMI, NV21ToARGBRow_MMI, 1, 1, 2, 4, 7)
#endif
#ifdef HAS_NV12TORGB24ROW_NEON
ANY21C(NV12ToRGB24Row_Any_NEON, NV12ToRGB24Row_NEON, 1, 1, 2, 3, 7)
#endif
#ifdef HAS_NV21TORGB24ROW_NEON
ANY21C(NV21ToRGB24Row_Any_NEON, NV21ToRGB24Row_NEON, 1, 1, 2, 3, 7)
#endif
#ifdef HAS_NV12TORGB24ROW_SSSE3
ANY21C(NV12ToRGB24Row_Any_SSSE3, NV12ToRGB24Row_SSSE3, 1, 1, 2, 3, 15)
#endif
#ifdef HAS_NV12TORGB24ROW_MMI
ANY21C(NV12ToRGB24Row_Any_MMI, NV12ToRGB24Row_MMI, 1, 1, 2, 3, 7)
#endif
#ifdef HAS_NV21TORGB24ROW_SSSE3
ANY21C(NV21ToRGB24Row_Any_SSSE3, NV21ToRGB24Row_SSSE3, 1, 1, 2, 3, 15)
#endif
#ifdef HAS_NV12TORGB24ROW_AVX2
ANY21C(NV12ToRGB24Row_Any_AVX2, NV12ToRGB24Row_AVX2, 1, 1, 2, 3, 31)
#endif
#ifdef HAS_NV21TORGB24ROW_AVX2
ANY21C(NV21ToRGB24Row_Any_AVX2, NV21ToRGB24Row_AVX2, 1, 1, 2, 3, 31)
#endif
#ifdef HAS_NV21TORGB24ROW_MMI
ANY21C(NV21ToRGB24Row_Any_MMI, NV21ToRGB24Row_MMI, 1, 1, 2, 3, 7)
#endif
#ifdef HAS_NV12TORGB565ROW_SSSE3
ANY21C(NV12ToRGB565Row_Any_SSSE3, NV12ToRGB565Row_SSSE3, 1, 1, 2, 2, 7)
#endif
#ifdef HAS_NV12TORGB565ROW_AVX2
ANY21C(NV12ToRGB565Row_Any_AVX2, NV12ToRGB565Row_AVX2, 1, 1, 2, 2, 15)
#endif
#ifdef HAS_NV12TORGB565ROW_NEON
ANY21C(NV12ToRGB565Row_Any_NEON, NV12ToRGB565Row_NEON, 1, 1, 2, 2, 7)
#endif
#ifdef HAS_NV12TORGB565ROW_MSA
ANY21C(NV12ToRGB565Row_Any_MSA, NV12ToRGB565Row_MSA, 1, 1, 2, 2, 7)
#endif
#ifdef HAS_NV12TORGB565ROW_MMI
ANY21C(NV12ToRGB565Row_Any_MMI, NV12ToRGB565Row_MMI, 1, 1, 2, 2, 7)
#endif
#undef ANY21C

// Any 2 planes of 16 bit to 1 with yuvconstants
#define ANY21CT(NAMEANY, ANY_SIMD, UVSHIFT, DUVSHIFT, T, SBPP, BPP, MASK)      \
  void NAMEANY(const T* y_buf, const T* uv_buf, uint8_t* dst_ptr,              \
               const struct YuvConstants* yuvconstants, int width) {           \
    SIMD_ALIGNED(T temp[16 * 3]);                                              \
    SIMD_ALIGNED(uint8_t out[64]);                                             \
    memset(temp, 0, 16 * 3 * SBPP); /* for YUY2 and msan */                    \
    int r = width & MASK;                                                      \
    int n = width & ~MASK;                                                     \
    if (n > 0) {                                                               \
      ANY_SIMD(y_buf, uv_buf, dst_ptr, yuvconstants, n);                       \
    }                                                                          \
    memcpy(temp, y_buf + n, r * SBPP);                                         \
    memcpy(temp + 16, uv_buf + 2 * (n >> UVSHIFT), SS(r, UVSHIFT) * SBPP * 2); \
    ANY_SIMD(temp, temp + 16, out, yuvconstants, MASK + 1);                    \
    memcpy(dst_ptr + (n >> DUVSHIFT) * BPP, out, SS(r, DUVSHIFT) * BPP);       \
  }

#ifdef HAS_P210TOAR30ROW_SSSE3
ANY21CT(P210ToAR30Row_Any_SSSE3, P210ToAR30Row_SSSE3, 1, 0, uint16_t, 2, 4, 7)
#endif
#ifdef HAS_P210TOARGBROW_SSSE3
ANY21CT(P210ToARGBRow_Any_SSSE3, P210ToARGBRow_SSSE3, 1, 0, uint16_t, 2, 4, 7)
#endif
#ifdef HAS_P210TOARGBROW_AVX2
ANY21CT(P210ToARGBRow_Any_AVX2, P210ToARGBRow_AVX2, 1, 0, uint16_t, 2, 4, 15)
#endif
#ifdef HAS_P210TOAR30ROW_AVX2
ANY21CT(P210ToAR30Row_Any_AVX2, P210ToAR30Row_AVX2, 1, 0, uint16_t, 2, 4, 15)
#endif
#ifdef HAS_P410TOAR30ROW_SSSE3
ANY21CT(P410ToAR30Row_Any_SSSE3, P410ToAR30Row_SSSE3, 0, 0, uint16_t, 2, 4, 7)
#endif
#ifdef HAS_P410TOARGBROW_SSSE3
ANY21CT(P410ToARGBRow_Any_SSSE3, P410ToARGBRow_SSSE3, 0, 0, uint16_t, 2, 4, 7)
#endif
#ifdef HAS_P410TOARGBROW_AVX2
ANY21CT(P410ToARGBRow_Any_AVX2, P410ToARGBRow_AVX2, 0, 0, uint16_t, 2, 4, 15)
#endif
#ifdef HAS_P410TOAR30ROW_AVX2
ANY21CT(P410ToAR30Row_Any_AVX2, P410ToAR30Row_AVX2, 0, 0, uint16_t, 2, 4, 15)
#endif

#undef ANY21CT

// Any 2 16 bit planes with parameter to 1
#define ANY21PT(NAMEANY, ANY_SIMD, T, BPP, MASK)                     \
  void NAMEANY(const T* src_u, const T* src_v, T* dst_uv, int depth, \
               int width) {                                          \
    SIMD_ALIGNED(T temp[16 * 4]);                                    \
    memset(temp, 0, 16 * 4 * BPP); /* for msan */                    \
    int r = width & MASK;                                            \
    int n = width & ~MASK;                                           \
    if (n > 0) {                                                     \
      ANY_SIMD(src_u, src_v, dst_uv, depth, n);                      \
    }                                                                \
    memcpy(temp, src_u + n, r * BPP);                                \
    memcpy(temp + 16, src_v + n, r * BPP);                           \
    ANY_SIMD(temp, temp + 16, temp + 32, depth, MASK + 1);           \
    memcpy(dst_uv + n * 2, temp + 32, r * BPP * 2);                  \
  }

#ifdef HAS_MERGEUVROW_16_AVX2
ANY21PT(MergeUVRow_16_Any_AVX2, MergeUVRow_16_AVX2, uint16_t, 2, 15)
#endif
#ifdef HAS_MERGEUVROW_16_NEON
ANY21PT(MergeUVRow_16_Any_NEON, MergeUVRow_16_NEON, uint16_t, 2, 7)
#endif

#undef ANY21CT

// Any 1 to 1.
#define ANY11(NAMEANY, ANY_SIMD, UVSHIFT, SBPP, BPP, MASK)                \
  void NAMEANY(const uint8_t* src_ptr, uint8_t* dst_ptr, int width) {     \
    SIMD_ALIGNED(uint8_t temp[128 * 2]);                                  \
    memset(temp, 0, 128); /* for YUY2 and msan */                         \
    int r = width & MASK;                                                 \
    int n = width & ~MASK;                                                \
    if (n > 0) {                                                          \
      ANY_SIMD(src_ptr, dst_ptr, n);                                      \
    }                                                                     \
    memcpy(temp, src_ptr + (n >> UVSHIFT) * SBPP, SS(r, UVSHIFT) * SBPP); \
    ANY_SIMD(temp, temp + 128, MASK + 1);                                 \
    memcpy(dst_ptr + n * BPP, temp + 128, r * BPP);                       \
  }

#ifdef HAS_COPYROW_AVX
ANY11(CopyRow_Any_AVX, CopyRow_AVX, 0, 1, 1, 63)
#endif
#ifdef HAS_COPYROW_SSE2
ANY11(CopyRow_Any_SSE2, CopyRow_SSE2, 0, 1, 1, 31)
#endif
#ifdef HAS_COPYROW_NEON
ANY11(CopyRow_Any_NEON, CopyRow_NEON, 0, 1, 1, 31)
#endif
#if defined(HAS_ARGBTORGB24ROW_SSSE3)
ANY11(ARGBToRGB24Row_Any_SSSE3, ARGBToRGB24Row_SSSE3, 0, 4, 3, 15)
ANY11(ARGBToRAWRow_Any_SSSE3, ARGBToRAWRow_SSSE3, 0, 4, 3, 15)
ANY11(ARGBToRGB565Row_Any_SSE2, ARGBToRGB565Row_SSE2, 0, 4, 2, 3)
ANY11(ARGBToARGB1555Row_Any_SSE2, ARGBToARGB1555Row_SSE2, 0, 4, 2, 3)
ANY11(ARGBToARGB4444Row_Any_SSE2, ARGBToARGB4444Row_SSE2, 0, 4, 2, 3)
#endif
#if defined(HAS_ARGBTORGB24ROW_AVX2)
ANY11(ARGBToRGB24Row_Any_AVX2, ARGBToRGB24Row_AVX2, 0, 4, 3, 31)
#endif
#if defined(HAS_ARGBTORGB24ROW_AVX512VBMI)
ANY11(ARGBToRGB24Row_Any_AVX512VBMI, ARGBToRGB24Row_AVX512VBMI, 0, 4, 3, 31)
#endif
#if defined(HAS_ARGBTORAWROW_AVX2)
ANY11(ARGBToRAWRow_Any_AVX2, ARGBToRAWRow_AVX2, 0, 4, 3, 31)
#endif
#if defined(HAS_ARGBTORGB565ROW_AVX2)
ANY11(ARGBToRGB565Row_Any_AVX2, ARGBToRGB565Row_AVX2, 0, 4, 2, 7)
#endif
#if defined(HAS_ARGBTOARGB4444ROW_AVX2)
ANY11(ARGBToARGB1555Row_Any_AVX2, ARGBToARGB1555Row_AVX2, 0, 4, 2, 7)
ANY11(ARGBToARGB4444Row_Any_AVX2, ARGBToARGB4444Row_AVX2, 0, 4, 2, 7)
#endif
#if defined(HAS_ABGRTOAR30ROW_SSSE3)
ANY11(ABGRToAR30Row_Any_SSSE3, ABGRToAR30Row_SSSE3, 0, 4, 4, 3)
#endif
#if defined(HAS_ARGBTOAR30ROW_SSSE3)
ANY11(ARGBToAR30Row_Any_SSSE3, ARGBToAR30Row_SSSE3, 0, 4, 4, 3)
#endif
#if defined(HAS_ABGRTOAR30ROW_AVX2)
ANY11(ABGRToAR30Row_Any_AVX2, ABGRToAR30Row_AVX2, 0, 4, 4, 7)
#endif
#if defined(HAS_ARGBTOAR30ROW_AVX2)
ANY11(ARGBToAR30Row_Any_AVX2, ARGBToAR30Row_AVX2, 0, 4, 4, 7)
#endif
#if defined(HAS_J400TOARGBROW_SSE2)
ANY11(J400ToARGBRow_Any_SSE2, J400ToARGBRow_SSE2, 0, 1, 4, 7)
#endif
#if defined(HAS_J400TOARGBROW_AVX2)
ANY11(J400ToARGBRow_Any_AVX2, J400ToARGBRow_AVX2, 0, 1, 4, 15)
#endif
#if defined(HAS_RGB24TOARGBROW_SSSE3)
ANY11(RGB24ToARGBRow_Any_SSSE3, RGB24ToARGBRow_SSSE3, 0, 3, 4, 15)
ANY11(RAWToARGBRow_Any_SSSE3, RAWToARGBRow_SSSE3, 0, 3, 4, 15)
ANY11(RGB565ToARGBRow_Any_SSE2, RGB565ToARGBRow_SSE2, 0, 2, 4, 7)
ANY11(ARGB1555ToARGBRow_Any_SSE2, ARGB1555ToARGBRow_SSE2, 0, 2, 4, 7)
ANY11(ARGB4444ToARGBRow_Any_SSE2, ARGB4444ToARGBRow_SSE2, 0, 2, 4, 7)
#endif
#if defined(HAS_RAWTORGBAROW_SSSE3)
ANY11(RAWToRGBARow_Any_SSSE3, RAWToRGBARow_SSSE3, 0, 3, 4, 15)
#endif
#if defined(HAS_RAWTORGB24ROW_SSSE3)
ANY11(RAWToRGB24Row_Any_SSSE3, RAWToRGB24Row_SSSE3, 0, 3, 3, 7)
#endif
#if defined(HAS_RGB565TOARGBROW_AVX2)
ANY11(RGB565ToARGBRow_Any_AVX2, RGB565ToARGBRow_AVX2, 0, 2, 4, 15)
#endif
#if defined(HAS_ARGB1555TOARGBROW_AVX2)
ANY11(ARGB1555ToARGBRow_Any_AVX2, ARGB1555ToARGBRow_AVX2, 0, 2, 4, 15)
#endif
#if defined(HAS_ARGB4444TOARGBROW_AVX2)
ANY11(ARGB4444ToARGBRow_Any_AVX2, ARGB4444ToARGBRow_AVX2, 0, 2, 4, 15)
#endif
#if defined(HAS_ARGBTORGB24ROW_NEON)
ANY11(ARGBToRGB24Row_Any_NEON, ARGBToRGB24Row_NEON, 0, 4, 3, 7)
ANY11(ARGBToRAWRow_Any_NEON, ARGBToRAWRow_NEON, 0, 4, 3, 7)
ANY11(ARGBToRGB565Row_Any_NEON, ARGBToRGB565Row_NEON, 0, 4, 2, 7)
ANY11(ARGBToARGB1555Row_Any_NEON, ARGBToARGB1555Row_NEON, 0, 4, 2, 7)
ANY11(ARGBToARGB4444Row_Any_NEON, ARGBToARGB4444Row_NEON, 0, 4, 2, 7)
ANY11(J400ToARGBRow_Any_NEON, J400ToARGBRow_NEON, 0, 1, 4, 7)
#endif
#if defined(HAS_ARGBTORGB24ROW_MSA)
ANY11(ARGBToRGB24Row_Any_MSA, ARGBToRGB24Row_MSA, 0, 4, 3, 15)
ANY11(ARGBToRAWRow_Any_MSA, ARGBToRAWRow_MSA, 0, 4, 3, 15)
ANY11(ARGBToRGB565Row_Any_MSA, ARGBToRGB565Row_MSA, 0, 4, 2, 7)
ANY11(ARGBToARGB1555Row_Any_MSA, ARGBToARGB1555Row_MSA, 0, 4, 2, 7)
ANY11(ARGBToARGB4444Row_Any_MSA, ARGBToARGB4444Row_MSA, 0, 4, 2, 7)
ANY11(J400ToARGBRow_Any_MSA, J400ToARGBRow_MSA, 0, 1, 4, 15)
#endif
#if defined(HAS_ARGBTORGB24ROW_MMI)
ANY11(ARGBToRGB24Row_Any_MMI, ARGBToRGB24Row_MMI, 0, 4, 3, 3)
ANY11(ARGBToRAWRow_Any_MMI, ARGBToRAWRow_MMI, 0, 4, 3, 3)
ANY11(ARGBToRGB565Row_Any_MMI, ARGBToRGB565Row_MMI, 0, 4, 2, 3)
ANY11(ARGBToARGB1555Row_Any_MMI, ARGBToARGB1555Row_MMI, 0, 4, 2, 3)
ANY11(ARGBToARGB4444Row_Any_MMI, ARGBToARGB4444Row_MMI, 0, 4, 2, 3)
ANY11(J400ToARGBRow_Any_MMI, J400ToARGBRow_MMI, 0, 1, 4, 3)
#endif
#if defined(HAS_RAWTORGB24ROW_NEON)
ANY11(RAWToRGB24Row_Any_NEON, RAWToRGB24Row_NEON, 0, 3, 3, 7)
#endif
#if defined(HAS_RAWTORGB24ROW_MSA)
ANY11(RAWToRGB24Row_Any_MSA, RAWToRGB24Row_MSA, 0, 3, 3, 15)
#endif
#if defined(HAS_RAWTORGB24ROW_MMI)
ANY11(RAWToRGB24Row_Any_MMI, RAWToRGB24Row_MMI, 0, 3, 3, 3)
#endif
#ifdef HAS_ARGBTOYROW_AVX2
ANY11(ARGBToYRow_Any_AVX2, ARGBToYRow_AVX2, 0, 4, 1, 31)
#endif
#ifdef HAS_ABGRTOYROW_AVX2
ANY11(ABGRToYRow_Any_AVX2, ABGRToYRow_AVX2, 0, 4, 1, 31)
#endif
#ifdef HAS_ARGBTOYJROW_AVX2
ANY11(ARGBToYJRow_Any_AVX2, ARGBToYJRow_AVX2, 0, 4, 1, 31)
#endif
#ifdef HAS_RGBATOYJROW_AVX2
ANY11(RGBAToYJRow_Any_AVX2, RGBAToYJRow_AVX2, 0, 4, 1, 31)
#endif
#ifdef HAS_UYVYTOYROW_AVX2
ANY11(UYVYToYRow_Any_AVX2, UYVYToYRow_AVX2, 0, 2, 1, 31)
#endif
#ifdef HAS_YUY2TOYROW_AVX2
ANY11(YUY2ToYRow_Any_AVX2, YUY2ToYRow_AVX2, 1, 4, 1, 31)
#endif
#ifdef HAS_ARGBTOYROW_SSSE3
ANY11(ARGBToYRow_Any_SSSE3, ARGBToYRow_SSSE3, 0, 4, 1, 15)
#endif
#ifdef HAS_BGRATOYROW_SSSE3
ANY11(BGRAToYRow_Any_SSSE3, BGRAToYRow_SSSE3, 0, 4, 1, 15)
ANY11(ABGRToYRow_Any_SSSE3, ABGRToYRow_SSSE3, 0, 4, 1, 15)
ANY11(RGBAToYRow_Any_SSSE3, RGBAToYRow_SSSE3, 0, 4, 1, 15)
ANY11(YUY2ToYRow_Any_SSE2, YUY2ToYRow_SSE2, 1, 4, 1, 15)
ANY11(UYVYToYRow_Any_SSE2, UYVYToYRow_SSE2, 1, 4, 1, 15)
#endif
#ifdef HAS_ARGBTOYJROW_SSSE3
ANY11(ARGBToYJRow_Any_SSSE3, ARGBToYJRow_SSSE3, 0, 4, 1, 15)
#endif
#ifdef HAS_RGBATOYJROW_SSSE3
ANY11(RGBAToYJRow_Any_SSSE3, RGBAToYJRow_SSSE3, 0, 4, 1, 15)
#endif
#ifdef HAS_ARGBTOYROW_NEON
ANY11(ARGBToYRow_Any_NEON, ARGBToYRow_NEON, 0, 4, 1, 7)
#endif
#ifdef HAS_ARGBTOYROW_MSA
ANY11(ARGBToYRow_Any_MSA, ARGBToYRow_MSA, 0, 4, 1, 15)
#endif
#ifdef HAS_ARGBTOYROW_MMI
ANY11(ARGBToYRow_Any_MMI, ARGBToYRow_MMI, 0, 4, 1, 7)
#endif
#ifdef HAS_ARGBTOYJROW_NEON
ANY11(ARGBToYJRow_Any_NEON, ARGBToYJRow_NEON, 0, 4, 1, 7)
#endif
#ifdef HAS_RGBATOYJROW_NEON
ANY11(RGBAToYJRow_Any_NEON, RGBAToYJRow_NEON, 0, 4, 1, 7)
#endif
#ifdef HAS_ARGBTOYJROW_MSA
ANY11(ARGBToYJRow_Any_MSA, ARGBToYJRow_MSA, 0, 4, 1, 15)
#endif
#ifdef HAS_ARGBTOYJROW_MMI
ANY11(ARGBToYJRow_Any_MMI, ARGBToYJRow_MMI, 0, 4, 1, 7)
#endif
#ifdef HAS_BGRATOYROW_NEON
ANY11(BGRAToYRow_Any_NEON, BGRAToYRow_NEON, 0, 4, 1, 7)
#endif
#ifdef HAS_BGRATOYROW_MSA
ANY11(BGRAToYRow_Any_MSA, BGRAToYRow_MSA, 0, 4, 1, 15)
#endif
#ifdef HAS_BGRATOYROW_MMI
ANY11(BGRAToYRow_Any_MMI, BGRAToYRow_MMI, 0, 4, 1, 7)
#endif
#ifdef HAS_ABGRTOYROW_NEON
ANY11(ABGRToYRow_Any_NEON, ABGRToYRow_NEON, 0, 4, 1, 7)
#endif
#ifdef HAS_ABGRTOYROW_MSA
ANY11(ABGRToYRow_Any_MSA, ABGRToYRow_MSA, 0, 4, 1, 7)
#endif
#ifdef HAS_ABGRTOYROW_MMI
ANY11(ABGRToYRow_Any_MMI, ABGRToYRow_MMI, 0, 4, 1, 7)
#endif
#ifdef HAS_RGBATOYROW_NEON
ANY11(RGBAToYRow_Any_NEON, RGBAToYRow_NEON, 0, 4, 1, 7)
#endif
#ifdef HAS_RGBATOYROW_MSA
ANY11(RGBAToYRow_Any_MSA, RGBAToYRow_MSA, 0, 4, 1, 15)
#endif
#ifdef HAS_RGBATOYROW_MMI
ANY11(RGBAToYRow_Any_MMI, RGBAToYRow_MMI, 0, 4, 1, 7)
#endif
#ifdef HAS_RGB24TOYROW_NEON
ANY11(RGB24ToYRow_Any_NEON, RGB24ToYRow_NEON, 0, 3, 1, 7)
#endif
#ifdef HAS_RGB24TOYJROW_AVX2
ANY11(RGB24ToYJRow_Any_AVX2, RGB24ToYJRow_AVX2, 0, 3, 1, 31)
#endif
#ifdef HAS_RGB24TOYJROW_SSSE3
ANY11(RGB24ToYJRow_Any_SSSE3, RGB24ToYJRow_SSSE3, 0, 3, 1, 15)
#endif
#ifdef HAS_RGB24TOYJROW_NEON
ANY11(RGB24ToYJRow_Any_NEON, RGB24ToYJRow_NEON, 0, 3, 1, 7)
#endif
#ifdef HAS_RGB24TOYROW_MSA
ANY11(RGB24ToYRow_Any_MSA, RGB24ToYRow_MSA, 0, 3, 1, 15)
#endif
#ifdef HAS_RGB24TOYROW_MMI
ANY11(RGB24ToYRow_Any_MMI, RGB24ToYRow_MMI, 0, 3, 1, 7)
#endif
#ifdef HAS_RAWTOYROW_NEON
ANY11(RAWToYRow_Any_NEON, RAWToYRow_NEON, 0, 3, 1, 7)
#endif
#ifdef HAS_RAWTOYJROW_AVX2
ANY11(RAWToYJRow_Any_AVX2, RAWToYJRow_AVX2, 0, 3, 1, 31)
#endif
#ifdef HAS_RAWTOYJROW_SSSE3
ANY11(RAWToYJRow_Any_SSSE3, RAWToYJRow_SSSE3, 0, 3, 1, 15)
#endif
#ifdef HAS_RAWTOYJROW_NEON
ANY11(RAWToYJRow_Any_NEON, RAWToYJRow_NEON, 0, 3, 1, 7)
#endif
#ifdef HAS_RAWTOYROW_MSA
ANY11(RAWToYRow_Any_MSA, RAWToYRow_MSA, 0, 3, 1, 15)
#endif
#ifdef HAS_RAWTOYROW_MMI
ANY11(RAWToYRow_Any_MMI, RAWToYRow_MMI, 0, 3, 1, 7)
#endif
#ifdef HAS_RGB565TOYROW_NEON
ANY11(RGB565ToYRow_Any_NEON, RGB565ToYRow_NEON, 0, 2, 1, 7)
#endif
#ifdef HAS_RGB565TOYROW_MSA
ANY11(RGB565ToYRow_Any_MSA, RGB565ToYRow_MSA, 0, 2, 1, 15)
#endif
#ifdef HAS_RGB565TOYROW_MMI
ANY11(RGB565ToYRow_Any_MMI, RGB565ToYRow_MMI, 0, 2, 1, 7)
#endif
#ifdef HAS_ARGB1555TOYROW_NEON
ANY11(ARGB1555ToYRow_Any_NEON, ARGB1555ToYRow_NEON, 0, 2, 1, 7)
#endif
#ifdef HAS_ARGB1555TOYROW_MSA
ANY11(ARGB1555ToYRow_Any_MSA, ARGB1555ToYRow_MSA, 0, 2, 1, 15)
#endif
#ifdef HAS_ARGB1555TOYROW_MMI
ANY11(ARGB1555ToYRow_Any_MMI, ARGB1555ToYRow_MMI, 0, 2, 1, 7)
#endif
#ifdef HAS_ARGB4444TOYROW_NEON
ANY11(ARGB4444ToYRow_Any_NEON, ARGB4444ToYRow_NEON, 0, 2, 1, 7)
#endif
#ifdef HAS_ARGB4444TOYROW_MMI
ANY11(ARGB4444ToYRow_Any_MMI, ARGB4444ToYRow_MMI, 0, 2, 1, 7)
#endif
#ifdef HAS_YUY2TOYROW_NEON
ANY11(YUY2ToYRow_Any_NEON, YUY2ToYRow_NEON, 1, 4, 1, 15)
#endif
#ifdef HAS_UYVYTOYROW_NEON
ANY11(UYVYToYRow_Any_NEON, UYVYToYRow_NEON, 1, 4, 1, 15)
#endif
#ifdef HAS_YUY2TOYROW_MSA
ANY11(YUY2ToYRow_Any_MSA, YUY2ToYRow_MSA, 1, 4, 1, 31)
#endif
#ifdef HAS_YUY2TOYROW_MMI
ANY11(YUY2ToYRow_Any_MMI, YUY2ToYRow_MMI, 1, 4, 1, 7)
#endif
#ifdef HAS_UYVYTOYROW_MSA
ANY11(UYVYToYRow_Any_MSA, UYVYToYRow_MSA, 1, 4, 1, 31)
#endif
#ifdef HAS_UYVYTOYROW_MMI
ANY11(UYVYToYRow_Any_MMI, UYVYToYRow_MMI, 1, 4, 1, 15)
#endif
#ifdef HAS_AYUVTOYROW_NEON
ANY11(AYUVToYRow_Any_NEON, AYUVToYRow_NEON, 0, 4, 1, 15)
#endif
#ifdef HAS_SWAPUVROW_SSSE3
ANY11(SwapUVRow_Any_SSSE3, SwapUVRow_SSSE3, 0, 2, 2, 15)
#endif
#ifdef HAS_SWAPUVROW_AVX2
ANY11(SwapUVRow_Any_AVX2, SwapUVRow_AVX2, 0, 2, 2, 31)
#endif
#ifdef HAS_SWAPUVROW_NEON
ANY11(SwapUVRow_Any_NEON, SwapUVRow_NEON, 0, 2, 2, 15)
#endif
#ifdef HAS_RGB24TOARGBROW_NEON
ANY11(RGB24ToARGBRow_Any_NEON, RGB24ToARGBRow_NEON, 0, 3, 4, 7)
#endif
#ifdef HAS_RGB24TOARGBROW_MSA
ANY11(RGB24ToARGBRow_Any_MSA, RGB24ToARGBRow_MSA, 0, 3, 4, 15)
#endif
#ifdef HAS_RGB24TOARGBROW_MMI
ANY11(RGB24ToARGBRow_Any_MMI, RGB24ToARGBRow_MMI, 0, 3, 4, 3)
#endif
#ifdef HAS_RAWTOARGBROW_NEON
ANY11(RAWToARGBRow_Any_NEON, RAWToARGBRow_NEON, 0, 3, 4, 7)
#endif
#ifdef HAS_RAWTORGBAROW_NEON
ANY11(RAWToRGBARow_Any_NEON, RAWToRGBARow_NEON, 0, 3, 4, 7)
#endif
#ifdef HAS_RAWTOARGBROW_MSA
ANY11(RAWToARGBRow_Any_MSA, RAWToARGBRow_MSA, 0, 3, 4, 15)
#endif
#ifdef HAS_RAWTOARGBROW_MMI
ANY11(RAWToARGBRow_Any_MMI, RAWToARGBRow_MMI, 0, 3, 4, 3)
#endif
#ifdef HAS_RGB565TOARGBROW_NEON
ANY11(RGB565ToARGBRow_Any_NEON, RGB565ToARGBRow_NEON, 0, 2, 4, 7)
#endif
#ifdef HAS_RGB565TOARGBROW_MSA
ANY11(RGB565ToARGBRow_Any_MSA, RGB565ToARGBRow_MSA, 0, 2, 4, 15)
#endif
#ifdef HAS_RGB565TOARGBROW_MMI
ANY11(RGB565ToARGBRow_Any_MMI, RGB565ToARGBRow_MMI, 0, 2, 4, 3)
#endif
#ifdef HAS_ARGB1555TOARGBROW_NEON
ANY11(ARGB1555ToARGBRow_Any_NEON, ARGB1555ToARGBRow_NEON, 0, 2, 4, 7)
#endif
#ifdef HAS_ARGB1555TOARGBROW_MSA
ANY11(ARGB1555ToARGBRow_Any_MSA, ARGB1555ToARGBRow_MSA, 0, 2, 4, 15)
#endif
#ifdef HAS_ARGB1555TOARGBROW_MMI
ANY11(ARGB1555ToARGBRow_Any_MMI, ARGB1555ToARGBRow_MMI, 0, 2, 4, 3)
#endif
#ifdef HAS_ARGB4444TOARGBROW_NEON
ANY11(ARGB4444ToARGBRow_Any_NEON, ARGB4444ToARGBRow_NEON, 0, 2, 4, 7)
#endif
#ifdef HAS_ARGB4444TOARGBROW_MSA
ANY11(ARGB4444ToARGBRow_Any_MSA, ARGB4444ToARGBRow_MSA, 0, 2, 4, 15)
#endif
#ifdef HAS_ARGB4444TOARGBROW_MMI
ANY11(ARGB4444ToARGBRow_Any_MMI, ARGB4444ToARGBRow_MMI, 0, 2, 4, 3)
#endif
#ifdef HAS_ARGBATTENUATEROW_SSSE3
ANY11(ARGBAttenuateRow_Any_SSSE3, ARGBAttenuateRow_SSSE3, 0, 4, 4, 3)
#endif
#ifdef HAS_ARGBUNATTENUATEROW_SSE2
ANY11(ARGBUnattenuateRow_Any_SSE2, ARGBUnattenuateRow_SSE2, 0, 4, 4, 3)
#endif
#ifdef HAS_ARGBATTENUATEROW_AVX2
ANY11(ARGBAttenuateRow_Any_AVX2, ARGBAttenuateRow_AVX2, 0, 4, 4, 7)
#endif
#ifdef HAS_ARGBUNATTENUATEROW_AVX2
ANY11(ARGBUnattenuateRow_Any_AVX2, ARGBUnattenuateRow_AVX2, 0, 4, 4, 7)
#endif
#ifdef HAS_ARGBATTENUATEROW_NEON
ANY11(ARGBAttenuateRow_Any_NEON, ARGBAttenuateRow_NEON, 0, 4, 4, 7)
#endif
#ifdef HAS_ARGBATTENUATEROW_MSA
ANY11(ARGBAttenuateRow_Any_MSA, ARGBAttenuateRow_MSA, 0, 4, 4, 7)
#endif
#ifdef HAS_ARGBATTENUATEROW_MMI
ANY11(ARGBAttenuateRow_Any_MMI, ARGBAttenuateRow_MMI, 0, 4, 4, 1)
#endif
#ifdef HAS_ARGBEXTRACTALPHAROW_SSE2
ANY11(ARGBExtractAlphaRow_Any_SSE2, ARGBExtractAlphaRow_SSE2, 0, 4, 1, 7)
#endif
#ifdef HAS_ARGBEXTRACTALPHAROW_AVX2
ANY11(ARGBExtractAlphaRow_Any_AVX2, ARGBExtractAlphaRow_AVX2, 0, 4, 1, 31)
#endif
#ifdef HAS_ARGBEXTRACTALPHAROW_NEON
ANY11(ARGBExtractAlphaRow_Any_NEON, ARGBExtractAlphaRow_NEON, 0, 4, 1, 15)
#endif
#ifdef HAS_ARGBEXTRACTALPHAROW_MSA
ANY11(ARGBExtractAlphaRow_Any_MSA, ARGBExtractAlphaRow_MSA, 0, 4, 1, 15)
#endif
#ifdef HAS_ARGBEXTRACTALPHAROW_MMI
ANY11(ARGBExtractAlphaRow_Any_MMI, ARGBExtractAlphaRow_MMI, 0, 4, 1, 7)
#endif
#undef ANY11

// Any 1 to 1 blended.  Destination is read, modify, write.
#define ANY11B(NAMEANY, ANY_SIMD, UVSHIFT, SBPP, BPP, MASK)               \
  void NAMEANY(const uint8_t* src_ptr, uint8_t* dst_ptr, int width) {     \
    SIMD_ALIGNED(uint8_t temp[64 * 2]);                                   \
    memset(temp, 0, 64 * 2); /* for msan */                               \
    int r = width & MASK;                                                 \
    int n = width & ~MASK;                                                \
    if (n > 0) {                                                          \
      ANY_SIMD(src_ptr, dst_ptr, n);                                      \
    }                                                                     \
    memcpy(temp, src_ptr + (n >> UVSHIFT) * SBPP, SS(r, UVSHIFT) * SBPP); \
    memcpy(temp + 64, dst_ptr + n * BPP, r * BPP);                        \
    ANY_SIMD(temp, temp + 64, MASK + 1);                                  \
    memcpy(dst_ptr + n * BPP, temp + 64, r * BPP);                        \
  }

#ifdef HAS_ARGBCOPYALPHAROW_AVX2
ANY11B(ARGBCopyAlphaRow_Any_AVX2, ARGBCopyAlphaRow_AVX2, 0, 4, 4, 15)
#endif
#ifdef HAS_ARGBCOPYALPHAROW_SSE2
ANY11B(ARGBCopyAlphaRow_Any_SSE2, ARGBCopyAlphaRow_SSE2, 0, 4, 4, 7)
#endif
#ifdef HAS_ARGBCOPYALPHAROW_MMI
ANY11B(ARGBCopyAlphaRow_Any_MMI, ARGBCopyAlphaRow_MMI, 0, 4, 4, 1)
#endif
#ifdef HAS_ARGBCOPYYTOALPHAROW_AVX2
ANY11B(ARGBCopyYToAlphaRow_Any_AVX2, ARGBCopyYToAlphaRow_AVX2, 0, 1, 4, 15)
#endif
#ifdef HAS_ARGBCOPYYTOALPHAROW_SSE2
ANY11B(ARGBCopyYToAlphaRow_Any_SSE2, ARGBCopyYToAlphaRow_SSE2, 0, 1, 4, 7)
#endif
#ifdef HAS_ARGBCOPYYTOALPHAROW_MMI
ANY11B(ARGBCopyYToAlphaRow_Any_MMI, ARGBCopyYToAlphaRow_MMI, 0, 1, 4, 7)
#endif
#undef ANY11B

// Any 1 to 1 with parameter.
#define ANY11P(NAMEANY, ANY_SIMD, T, SBPP, BPP, MASK)                          \
  void NAMEANY(const uint8_t* src_ptr, uint8_t* dst_ptr, T param, int width) { \
    SIMD_ALIGNED(uint8_t temp[64 * 2]);                                        \
    memset(temp, 0, 64); /* for msan */                                        \
    int r = width & MASK;                                                      \
    int n = width & ~MASK;                                                     \
    if (n > 0) {                                                               \
      ANY_SIMD(src_ptr, dst_ptr, param, n);                                    \
    }                                                                          \
    memcpy(temp, src_ptr + n * SBPP, r * SBPP);                                \
    ANY_SIMD(temp, temp + 64, param, MASK + 1);                                \
    memcpy(dst_ptr + n * BPP, temp + 64, r * BPP);                             \
  }

#if defined(HAS_I400TOARGBROW_SSE2)
ANY11P(I400ToARGBRow_Any_SSE2,
       I400ToARGBRow_SSE2,
       const struct YuvConstants*,
       1,
       4,
       7)
#endif
#if defined(HAS_I400TOARGBROW_AVX2)
ANY11P(I400ToARGBRow_Any_AVX2,
       I400ToARGBRow_AVX2,
       const struct YuvConstants*,
       1,
       4,
       15)
#endif
#if defined(HAS_I400TOARGBROW_NEON)
ANY11P(I400ToARGBRow_Any_NEON,
       I400ToARGBRow_NEON,
       const struct YuvConstants*,
       1,
       4,
       7)
#endif
#if defined(HAS_I400TOARGBROW_MSA)
ANY11P(I400ToARGBRow_Any_MSA,
       I400ToARGBRow_MSA,
       const struct YuvConstants*,
       1,
       4,
       15)
#endif
#if defined(HAS_I400TOARGBROW_MMI)
ANY11P(I400ToARGBRow_Any_MMI,
       I400ToARGBRow_MMI,
       const struct YuvConstants*,
       1,
       4,
       7)
#endif

#if defined(HAS_ARGBTORGB565DITHERROW_SSE2)
ANY11P(ARGBToRGB565DitherRow_Any_SSE2,
       ARGBToRGB565DitherRow_SSE2,
       const uint32_t,
       4,
       2,
       3)
#endif
#if defined(HAS_ARGBTORGB565DITHERROW_AVX2)
ANY11P(ARGBToRGB565DitherRow_Any_AVX2,
       ARGBToRGB565DitherRow_AVX2,
       const uint32_t,
       4,
       2,
       7)
#endif
#if defined(HAS_ARGBTORGB565DITHERROW_NEON)
ANY11P(ARGBToRGB565DitherRow_Any_NEON,
       ARGBToRGB565DitherRow_NEON,
       const uint32_t,
       4,
       2,
       7)
#endif
#if defined(HAS_ARGBTORGB565DITHERROW_MSA)
ANY11P(ARGBToRGB565DitherRow_Any_MSA,
       ARGBToRGB565DitherRow_MSA,
       const uint32_t,
       4,
       2,
       7)
#endif
#if defined(HAS_ARGBTORGB565DITHERROW_MMI)
ANY11P(ARGBToRGB565DitherRow_Any_MMI,
       ARGBToRGB565DitherRow_MMI,
       const uint32_t,
       4,
       2,
       3)
#endif
#ifdef HAS_ARGBSHUFFLEROW_SSSE3
ANY11P(ARGBShuffleRow_Any_SSSE3, ARGBShuffleRow_SSSE3, const uint8_t*, 4, 4, 7)
#endif
#ifdef HAS_ARGBSHUFFLEROW_AVX2
ANY11P(ARGBShuffleRow_Any_AVX2, ARGBShuffleRow_AVX2, const uint8_t*, 4, 4, 15)
#endif
#ifdef HAS_ARGBSHUFFLEROW_NEON
ANY11P(ARGBShuffleRow_Any_NEON, ARGBShuffleRow_NEON, const uint8_t*, 4, 4, 3)
#endif
#ifdef HAS_ARGBSHUFFLEROW_MSA
ANY11P(ARGBShuffleRow_Any_MSA, ARGBShuffleRow_MSA, const uint8_t*, 4, 4, 7)
#endif
#ifdef HAS_ARGBSHUFFLEROW_MMI
ANY11P(ARGBShuffleRow_Any_MMI, ARGBShuffleRow_MMI, const uint8_t*, 4, 4, 1)
#endif
#undef ANY11P
#undef ANY11P

// Any 1 to 1 with type
#define ANY11T(NAMEANY, ANY_SIMD, SBPP, BPP, STYPE, DTYPE, MASK)  \
  void NAMEANY(const STYPE* src_ptr, DTYPE* dst_ptr, int width) { \
    SIMD_ALIGNED(uint8_t temp[(MASK + 1) * SBPP]);                \
    SIMD_ALIGNED(uint8_t out[(MASK + 1) * BPP]);                  \
    memset(temp, 0, (MASK + 1) * SBPP); /* for msan */            \
    int r = width & MASK;                                         \
    int n = width & ~MASK;                                        \
    if (n > 0) {                                                  \
      ANY_SIMD(src_ptr, dst_ptr, n);                              \
    }                                                             \
    memcpy(temp, (uint8_t*)(src_ptr) + n * SBPP, r * SBPP);       \
    ANY_SIMD((STYPE*)temp, (DTYPE*)out, MASK + 1);                \
    memcpy((uint8_t*)(dst_ptr) + n * BPP, out, r * BPP);          \
  }

#ifdef HAS_ARGBTOAR64ROW_SSSE3
ANY11T(ARGBToAR64Row_Any_SSSE3, ARGBToAR64Row_SSSE3, 4, 8, uint8_t, uint16_t, 3)
#endif

#ifdef HAS_ARGBTOAB64ROW_SSSE3
ANY11T(ARGBToAB64Row_Any_SSSE3, ARGBToAB64Row_SSSE3, 4, 8, uint8_t, uint16_t, 3)
#endif

#ifdef HAS_AR64TOARGBROW_SSSE3
ANY11T(AR64ToARGBRow_Any_SSSE3, AR64ToARGBRow_SSSE3, 8, 4, uint16_t, uint8_t, 3)
#endif

#ifdef HAS_ARGBTOAR64ROW_SSSE3
ANY11T(AB64ToARGBRow_Any_SSSE3, AB64ToARGBRow_SSSE3, 8, 4, uint16_t, uint8_t, 3)
#endif

#ifdef HAS_ARGBTOAR64ROW_AVX2
ANY11T(ARGBToAR64Row_Any_AVX2, ARGBToAR64Row_AVX2, 4, 8, uint8_t, uint16_t, 7)
#endif

#ifdef HAS_ARGBTOAB64ROW_AVX2
ANY11T(ARGBToAB64Row_Any_AVX2, ARGBToAB64Row_AVX2, 4, 8, uint8_t, uint16_t, 7)
#endif

#ifdef HAS_AR64TOARGBROW_AVX2
ANY11T(AR64ToARGBRow_Any_AVX2, AR64ToARGBRow_AVX2, 8, 4, uint16_t, uint8_t, 7)
#endif

#ifdef HAS_ARGBTOAR64ROW_AVX2
ANY11T(AB64ToARGBRow_Any_AVX2, AB64ToARGBRow_AVX2, 8, 4, uint16_t, uint8_t, 7)
#endif

#ifdef HAS_ARGBTOAR64ROW_NEON
ANY11T(ARGBToAR64Row_Any_NEON, ARGBToAR64Row_NEON, 4, 8, uint8_t, uint16_t, 7)
#endif

#ifdef HAS_ARGBTOAB64ROW_NEON
ANY11T(ARGBToAB64Row_Any_NEON, ARGBToAB64Row_NEON, 4, 8, uint8_t, uint16_t, 7)
#endif

#ifdef HAS_AR64TOARGBROW_NEON
ANY11T(AR64ToARGBRow_Any_NEON, AR64ToARGBRow_NEON, 8, 4, uint16_t, uint8_t, 7)
#endif

#ifdef HAS_ARGBTOAR64ROW_NEON
ANY11T(AB64ToARGBRow_Any_NEON, AB64ToARGBRow_NEON, 8, 4, uint16_t, uint8_t, 7)
#endif

#undef ANY11T

// Any 1 to 1 with parameter and shorts.  BPP measures in shorts.
#define ANY11C(NAMEANY, ANY_SIMD, SBPP, BPP, STYPE, DTYPE, MASK)             \
  void NAMEANY(const STYPE* src_ptr, DTYPE* dst_ptr, int scale, int width) { \
    SIMD_ALIGNED(STYPE temp[32]);                                            \
    SIMD_ALIGNED(DTYPE out[32]);                                             \
    memset(temp, 0, 32 * SBPP); /* for msan */                               \
    int r = width & MASK;                                                    \
    int n = width & ~MASK;                                                   \
    if (n > 0) {                                                             \
      ANY_SIMD(src_ptr, dst_ptr, scale, n);                                  \
    }                                                                        \
    memcpy(temp, src_ptr + n, r * SBPP);                                     \
    ANY_SIMD(temp, out, scale, MASK + 1);                                    \
    memcpy(dst_ptr + n, out, r * BPP);                                       \
  }

#ifdef HAS_CONVERT16TO8ROW_SSSE3
ANY11C(Convert16To8Row_Any_SSSE3,
       Convert16To8Row_SSSE3,
       2,
       1,
       uint16_t,
       uint8_t,
       15)
#endif
#ifdef HAS_CONVERT16TO8ROW_AVX2
ANY11C(Convert16To8Row_Any_AVX2,
       Convert16To8Row_AVX2,
       2,
       1,
       uint16_t,
       uint8_t,
       31)
#endif
#ifdef HAS_CONVERT8TO16ROW_SSE2
ANY11C(Convert8To16Row_Any_SSE2,
       Convert8To16Row_SSE2,
       1,
       2,
       uint8_t,
       uint16_t,
       15)
#endif
#ifdef HAS_CONVERT8TO16ROW_AVX2
ANY11C(Convert8To16Row_Any_AVX2,
       Convert8To16Row_AVX2,
       1,
       2,
       uint8_t,
       uint16_t,
       31)
#endif
#ifdef HAS_MULTIPLYROW_16_AVX2
ANY11C(MultiplyRow_16_Any_AVX2,
       MultiplyRow_16_AVX2,
       2,
       2,
       uint16_t,
       uint16_t,
       31)
#endif
#ifdef HAS_MULTIPLYROW_16_NEON
ANY11C(MultiplyRow_16_Any_NEON,
       MultiplyRow_16_NEON,
       2,
       2,
       uint16_t,
       uint16_t,
       15)
#endif
#ifdef HAS_DIVIDEROW_16_AVX2
ANY11C(DivideRow_16_Any_AVX2, DivideRow_16_AVX2, 2, 2, uint16_t, uint16_t, 31)
#endif
#ifdef HAS_DIVIDEROW_16_NEON
ANY11C(DivideRow_16_Any_NEON, DivideRow_16_NEON, 2, 2, uint16_t, uint16_t, 15)
#endif
#undef ANY11C

// Any 1 to 1 with parameter and shorts to byte.  BPP measures in shorts.
#define ANY11P16(NAMEANY, ANY_SIMD, ST, T, SBPP, BPP, MASK)             \
  void NAMEANY(const ST* src_ptr, T* dst_ptr, float param, int width) { \
    SIMD_ALIGNED(ST temp[32]);                                          \
    SIMD_ALIGNED(T out[32]);                                            \
    memset(temp, 0, SBPP * 32); /* for msan */                          \
    int r = width & MASK;                                               \
    int n = width & ~MASK;                                              \
    if (n > 0) {                                                        \
      ANY_SIMD(src_ptr, dst_ptr, param, n);                             \
    }                                                                   \
    memcpy(temp, src_ptr + n, r * SBPP);                                \
    ANY_SIMD(temp, out, param, MASK + 1);                               \
    memcpy(dst_ptr + n, out, r * BPP);                                  \
  }

#ifdef HAS_HALFFLOATROW_SSE2
ANY11P16(HalfFloatRow_Any_SSE2, HalfFloatRow_SSE2, uint16_t, uint16_t, 2, 2, 7)
#endif
#ifdef HAS_HALFFLOATROW_AVX2
ANY11P16(HalfFloatRow_Any_AVX2, HalfFloatRow_AVX2, uint16_t, uint16_t, 2, 2, 15)
#endif
#ifdef HAS_HALFFLOATROW_F16C
ANY11P16(HalfFloatRow_Any_F16C, HalfFloatRow_F16C, uint16_t, uint16_t, 2, 2, 15)
ANY11P16(HalfFloat1Row_Any_F16C,
         HalfFloat1Row_F16C,
         uint16_t,
         uint16_t,
         2,
         2,
         15)
#endif
#ifdef HAS_HALFFLOATROW_NEON
ANY11P16(HalfFloatRow_Any_NEON, HalfFloatRow_NEON, uint16_t, uint16_t, 2, 2, 7)
ANY11P16(HalfFloat1Row_Any_NEON,
         HalfFloat1Row_NEON,
         uint16_t,
         uint16_t,
         2,
         2,
         7)
#endif
#ifdef HAS_HALFFLOATROW_MSA
ANY11P16(HalfFloatRow_Any_MSA, HalfFloatRow_MSA, uint16_t, uint16_t, 2, 2, 31)
#endif
#ifdef HAS_BYTETOFLOATROW_NEON
ANY11P16(ByteToFloatRow_Any_NEON, ByteToFloatRow_NEON, uint8_t, float, 1, 3, 7)
#endif
#undef ANY11P16

// Any 1 to 1 with yuvconstants
#define ANY11C(NAMEANY, ANY_SIMD, UVSHIFT, SBPP, BPP, MASK)               \
  void NAMEANY(const uint8_t* src_ptr, uint8_t* dst_ptr,                  \
               const struct YuvConstants* yuvconstants, int width) {      \
    SIMD_ALIGNED(uint8_t temp[128 * 2]);                                  \
    memset(temp, 0, 128); /* for YUY2 and msan */                         \
    int r = width & MASK;                                                 \
    int n = width & ~MASK;                                                \
    if (n > 0) {                                                          \
      ANY_SIMD(src_ptr, dst_ptr, yuvconstants, n);                        \
    }                                                                     \
    memcpy(temp, src_ptr + (n >> UVSHIFT) * SBPP, SS(r, UVSHIFT) * SBPP); \
    ANY_SIMD(temp, temp + 128, yuvconstants, MASK + 1);                   \
    memcpy(dst_ptr + n * BPP, temp + 128, r * BPP);                       \
  }
#if defined(HAS_YUY2TOARGBROW_SSSE3)
ANY11C(YUY2ToARGBRow_Any_SSSE3, YUY2ToARGBRow_SSSE3, 1, 4, 4, 15)
ANY11C(UYVYToARGBRow_Any_SSSE3, UYVYToARGBRow_SSSE3, 1, 4, 4, 15)
#endif
#if defined(HAS_YUY2TOARGBROW_AVX2)
ANY11C(YUY2ToARGBRow_Any_AVX2, YUY2ToARGBRow_AVX2, 1, 4, 4, 31)
ANY11C(UYVYToARGBRow_Any_AVX2, UYVYToARGBRow_AVX2, 1, 4, 4, 31)
#endif
#if defined(HAS_YUY2TOARGBROW_NEON)
ANY11C(YUY2ToARGBRow_Any_NEON, YUY2ToARGBRow_NEON, 1, 4, 4, 7)
ANY11C(UYVYToARGBRow_Any_NEON, UYVYToARGBRow_NEON, 1, 4, 4, 7)
#endif
#if defined(HAS_YUY2TOARGBROW_MSA)
ANY11C(YUY2ToARGBRow_Any_MSA, YUY2ToARGBRow_MSA, 1, 4, 4, 7)
ANY11C(UYVYToARGBRow_Any_MSA, UYVYToARGBRow_MSA, 1, 4, 4, 7)
#endif
#if defined(HAS_YUY2TOARGBROW_MMI)
ANY11C(YUY2ToARGBRow_Any_MMI, YUY2ToARGBRow_MMI, 1, 4, 4, 7)
ANY11C(UYVYToARGBRow_Any_MMI, UYVYToARGBRow_MMI, 1, 4, 4, 7)
#endif
#undef ANY11C

// Any 1 to 1 interpolate.  Takes 2 rows of source via stride.
#define ANY11I(NAMEANY, ANY_SIMD, SBPP, BPP, MASK)                             \
  void NAMEANY(uint8_t* dst_ptr, const uint8_t* src_ptr, ptrdiff_t src_stride, \
               int width, int source_y_fraction) {                             \
    SIMD_ALIGNED(uint8_t temp[64 * 3]);                                        \
    memset(temp, 0, 64 * 2); /* for msan */                                    \
    int r = width & MASK;                                                      \
    int n = width & ~MASK;                                                     \
    if (n > 0) {                                                               \
      ANY_SIMD(dst_ptr, src_ptr, src_stride, n, source_y_fraction);            \
    }                                                                          \
    memcpy(temp, src_ptr + n * SBPP, r * SBPP);                                \
    memcpy(temp + 64, src_ptr + src_stride + n * SBPP, r * SBPP);              \
    ANY_SIMD(temp + 128, temp, 64, MASK + 1, source_y_fraction);               \
    memcpy(dst_ptr + n * BPP, temp + 128, r * BPP);                            \
  }

#ifdef HAS_INTERPOLATEROW_AVX2
ANY11I(InterpolateRow_Any_AVX2, InterpolateRow_AVX2, 1, 1, 31)
#endif
#ifdef HAS_INTERPOLATEROW_SSSE3
ANY11I(InterpolateRow_Any_SSSE3, InterpolateRow_SSSE3, 1, 1, 15)
#endif
#ifdef HAS_INTERPOLATEROW_NEON
ANY11I(InterpolateRow_Any_NEON, InterpolateRow_NEON, 1, 1, 15)
#endif
#ifdef HAS_INTERPOLATEROW_MSA
ANY11I(InterpolateRow_Any_MSA, InterpolateRow_MSA, 1, 1, 31)
#endif
#ifdef HAS_INTERPOLATEROW_MMI
ANY11I(InterpolateRow_Any_MMI, InterpolateRow_MMI, 1, 1, 7)
#endif
#undef ANY11I

// Any 1 to 1 mirror.
#define ANY11M(NAMEANY, ANY_SIMD, BPP, MASK)                              \
  void NAMEANY(const uint8_t* src_ptr, uint8_t* dst_ptr, int width) {     \
    SIMD_ALIGNED(uint8_t temp[64 * 2]);                                   \
    memset(temp, 0, 64); /* for msan */                                   \
    int r = width & MASK;                                                 \
    int n = width & ~MASK;                                                \
    if (n > 0) {                                                          \
      ANY_SIMD(src_ptr + r * BPP, dst_ptr, n);                            \
    }                                                                     \
    memcpy(temp, src_ptr, r* BPP);                                        \
    ANY_SIMD(temp, temp + 64, MASK + 1);                                  \
    memcpy(dst_ptr + n * BPP, temp + 64 + (MASK + 1 - r) * BPP, r * BPP); \
  }

#ifdef HAS_MIRRORROW_AVX2
ANY11M(MirrorRow_Any_AVX2, MirrorRow_AVX2, 1, 31)
#endif
#ifdef HAS_MIRRORROW_SSSE3
ANY11M(MirrorRow_Any_SSSE3, MirrorRow_SSSE3, 1, 15)
#endif
#ifdef HAS_MIRRORROW_NEON
ANY11M(MirrorRow_Any_NEON, MirrorRow_NEON, 1, 31)
#endif
#ifdef HAS_MIRRORROW_MSA
ANY11M(MirrorRow_Any_MSA, MirrorRow_MSA, 1, 63)
#endif
#ifdef HAS_MIRRORROW_MMI
ANY11M(MirrorRow_Any_MMI, MirrorRow_MMI, 1, 7)
#endif
#ifdef HAS_MIRRORUVROW_AVX2
ANY11M(MirrorUVRow_Any_AVX2, MirrorUVRow_AVX2, 2, 15)
#endif
#ifdef HAS_MIRRORUVROW_SSSE3
ANY11M(MirrorUVRow_Any_SSSE3, MirrorUVRow_SSSE3, 2, 7)
#endif
#ifdef HAS_MIRRORUVROW_NEON
ANY11M(MirrorUVRow_Any_NEON, MirrorUVRow_NEON, 2, 31)
#endif
#ifdef HAS_MIRRORUVROW_MSA
ANY11M(MirrorUVRow_Any_MSA, MirrorUVRow_MSA, 2, 7)
#endif
#ifdef HAS_ARGBMIRRORROW_AVX2
ANY11M(ARGBMirrorRow_Any_AVX2, ARGBMirrorRow_AVX2, 4, 7)
#endif
#ifdef HAS_ARGBMIRRORROW_SSE2
ANY11M(ARGBMirrorRow_Any_SSE2, ARGBMirrorRow_SSE2, 4, 3)
#endif
#ifdef HAS_ARGBMIRRORROW_NEON
ANY11M(ARGBMirrorRow_Any_NEON, ARGBMirrorRow_NEON, 4, 7)
#endif
#ifdef HAS_ARGBMIRRORROW_MSA
ANY11M(ARGBMirrorRow_Any_MSA, ARGBMirrorRow_MSA, 4, 15)
#endif
#ifdef HAS_ARGBMIRRORROW_MMI
ANY11M(ARGBMirrorRow_Any_MMI, ARGBMirrorRow_MMI, 4, 1)
#endif
#ifdef HAS_RGB24MIRRORROW_SSSE3
ANY11M(RGB24MirrorRow_Any_SSSE3, RGB24MirrorRow_SSSE3, 3, 15)
#endif
#ifdef HAS_RGB24MIRRORROW_NEON
ANY11M(RGB24MirrorRow_Any_NEON, RGB24MirrorRow_NEON, 3, 15)
#endif
#undef ANY11M

// Any 1 plane. (memset)
#define ANY1(NAMEANY, ANY_SIMD, T, BPP, MASK)        \
  void NAMEANY(uint8_t* dst_ptr, T v32, int width) { \
    SIMD_ALIGNED(uint8_t temp[64]);                  \
    memset(temp, 0, 64); /* for msan */              \
    int r = width & MASK;                            \
    int n = width & ~MASK;                           \
    if (n > 0) {                                     \
      ANY_SIMD(dst_ptr, v32, n);                     \
    }                                                \
    ANY_SIMD(temp, v32, MASK + 1);                   \
    memcpy(dst_ptr + n * BPP, temp, r * BPP);        \
  }

#ifdef HAS_SETROW_X86
ANY1(SetRow_Any_X86, SetRow_X86, uint8_t, 1, 3)
#endif
#ifdef HAS_SETROW_NEON
ANY1(SetRow_Any_NEON, SetRow_NEON, uint8_t, 1, 15)
#endif
#ifdef HAS_ARGBSETROW_NEON
ANY1(ARGBSetRow_Any_NEON, ARGBSetRow_NEON, uint32_t, 4, 3)
#endif
#ifdef HAS_ARGBSETROW_MSA
ANY1(ARGBSetRow_Any_MSA, ARGBSetRow_MSA, uint32_t, 4, 3)
#endif
#ifdef HAS_ARGBSETROW_MMI
ANY1(ARGBSetRow_Any_MMI, ARGBSetRow_MMI, uint32_t, 4, 3)
#endif
#undef ANY1

// Any 1 to 2.  Outputs UV planes.
#define ANY12(NAMEANY, ANY_SIMD, UVSHIFT, BPP, DUVSHIFT, MASK)          \
  void NAMEANY(const uint8_t* src_ptr, uint8_t* dst_u, uint8_t* dst_v,  \
               int width) {                                             \
    SIMD_ALIGNED(uint8_t temp[128 * 3]);                                \
    memset(temp, 0, 128); /* for msan */                                \
    int r = width & MASK;                                               \
    int n = width & ~MASK;                                              \
    if (n > 0) {                                                        \
      ANY_SIMD(src_ptr, dst_u, dst_v, n);                               \
    }                                                                   \
    memcpy(temp, src_ptr + (n >> UVSHIFT) * BPP, SS(r, UVSHIFT) * BPP); \
    ANY_SIMD(temp, temp + 128, temp + 256, MASK + 1);                   \
    memcpy(dst_u + (n >> DUVSHIFT), temp + 128, SS(r, DUVSHIFT));       \
    memcpy(dst_v + (n >> DUVSHIFT), temp + 256, SS(r, DUVSHIFT));       \
  }

#ifdef HAS_SPLITUVROW_SSE2
ANY12(SplitUVRow_Any_SSE2, SplitUVRow_SSE2, 0, 2, 0, 15)
#endif
#ifdef HAS_SPLITUVROW_AVX2
ANY12(SplitUVRow_Any_AVX2, SplitUVRow_AVX2, 0, 2, 0, 31)
#endif
#ifdef HAS_SPLITUVROW_NEON
ANY12(SplitUVRow_Any_NEON, SplitUVRow_NEON, 0, 2, 0, 15)
#endif
#ifdef HAS_SPLITUVROW_MSA
ANY12(SplitUVRow_Any_MSA, SplitUVRow_MSA, 0, 2, 0, 31)
#endif
#ifdef HAS_SPLITUVROW_MMI
ANY12(SplitUVRow_Any_MMI, SplitUVRow_MMI, 0, 2, 0, 7)
#endif
#ifdef HAS_ARGBTOUV444ROW_SSSE3
ANY12(ARGBToUV444Row_Any_SSSE3, ARGBToUV444Row_SSSE3, 0, 4, 0, 15)
#endif
#ifdef HAS_YUY2TOUV422ROW_AVX2
ANY12(YUY2ToUV422Row_Any_AVX2, YUY2ToUV422Row_AVX2, 1, 4, 1, 31)
ANY12(UYVYToUV422Row_Any_AVX2, UYVYToUV422Row_AVX2, 1, 4, 1, 31)
#endif
#ifdef HAS_YUY2TOUV422ROW_SSE2
ANY12(YUY2ToUV422Row_Any_SSE2, YUY2ToUV422Row_SSE2, 1, 4, 1, 15)
ANY12(UYVYToUV422Row_Any_SSE2, UYVYToUV422Row_SSE2, 1, 4, 1, 15)
#endif
#ifdef HAS_YUY2TOUV422ROW_NEON
ANY12(ARGBToUV444Row_Any_NEON, ARGBToUV444Row_NEON, 0, 4, 0, 7)
ANY12(YUY2ToUV422Row_Any_NEON, YUY2ToUV422Row_NEON, 1, 4, 1, 15)
ANY12(UYVYToUV422Row_Any_NEON, UYVYToUV422Row_NEON, 1, 4, 1, 15)
#endif
#ifdef HAS_YUY2TOUV422ROW_MSA
ANY12(ARGBToUV444Row_Any_MSA, ARGBToUV444Row_MSA, 0, 4, 0, 15)
ANY12(YUY2ToUV422Row_Any_MSA, YUY2ToUV422Row_MSA, 1, 4, 1, 31)
ANY12(UYVYToUV422Row_Any_MSA, UYVYToUV422Row_MSA, 1, 4, 1, 31)
#endif
#ifdef HAS_YUY2TOUV422ROW_MMI
ANY12(ARGBToUV444Row_Any_MMI, ARGBToUV444Row_MMI, 0, 4, 0, 7)
ANY12(UYVYToUV422Row_Any_MMI, UYVYToUV422Row_MMI, 1, 4, 1, 15)
ANY12(YUY2ToUV422Row_Any_MMI, YUY2ToUV422Row_MMI, 1, 4, 1, 15)
#endif
#undef ANY12

// Any 2 16 bit planes with parameter to 1
#define ANY12PT(NAMEANY, ANY_SIMD, T, BPP, MASK)                            \
  void NAMEANY(const T* src_uv, T* dst_u, T* dst_v, int depth, int width) { \
    SIMD_ALIGNED(T temp[16 * 4]);                                           \
    memset(temp, 0, 16 * 4 * BPP); /* for msan */                           \
    int r = width & MASK;                                                   \
    int n = width & ~MASK;                                                  \
    if (n > 0) {                                                            \
      ANY_SIMD(src_uv, dst_u, dst_v, depth, n);                             \
    }                                                                       \
    memcpy(temp, src_uv + n * 2, r * BPP * 2);                              \
    ANY_SIMD(temp, temp + 32, temp + 48, depth, MASK + 1);                  \
    memcpy(dst_u + n, temp + 32, r * BPP);                                  \
    memcpy(dst_v + n, temp + 48, r * BPP);                                  \
  }

#ifdef HAS_SPLITUVROW_16_AVX2
ANY12PT(SplitUVRow_16_Any_AVX2, SplitUVRow_16_AVX2, uint16_t, 2, 15)
#endif

#ifdef HAS_SPLITUVROW_16_NEON
ANY12PT(SplitUVRow_16_Any_NEON, SplitUVRow_16_NEON, uint16_t, 2, 7)
#endif

#undef ANY21CT

// Any 1 to 3.  Outputs RGB planes.
#define ANY13(NAMEANY, ANY_SIMD, BPP, MASK)                                \
  void NAMEANY(const uint8_t* src_ptr, uint8_t* dst_r, uint8_t* dst_g,     \
               uint8_t* dst_b, int width) {                                \
    SIMD_ALIGNED(uint8_t temp[16 * 6]);                                    \
    memset(temp, 0, 16 * 3); /* for msan */                                \
    int r = width & MASK;                                                  \
    int n = width & ~MASK;                                                 \
    if (n > 0) {                                                           \
      ANY_SIMD(src_ptr, dst_r, dst_g, dst_b, n);                           \
    }                                                                      \
    memcpy(temp, src_ptr + n * BPP, r * BPP);                              \
    ANY_SIMD(temp, temp + 16 * 3, temp + 16 * 4, temp + 16 * 5, MASK + 1); \
    memcpy(dst_r + n, temp + 16 * 3, r);                                   \
    memcpy(dst_g + n, temp + 16 * 4, r);                                   \
    memcpy(dst_b + n, temp + 16 * 5, r);                                   \
  }

#ifdef HAS_SPLITRGBROW_SSSE3
ANY13(SplitRGBRow_Any_SSSE3, SplitRGBRow_SSSE3, 3, 15)
#endif
#ifdef HAS_SPLITRGBROW_NEON
ANY13(SplitRGBRow_Any_NEON, SplitRGBRow_NEON, 3, 15)
#endif
#ifdef HAS_SPLITRGBROW_MMI
ANY13(SplitRGBRow_Any_MMI, SplitRGBRow_MMI, 3, 3)
#endif
#ifdef HAS_SPLITXRGBROW_SSE2
ANY13(SplitXRGBRow_Any_SSE2, SplitXRGBRow_SSE2, 4, 7)
#endif
#ifdef HAS_SPLITXRGBROW_SSSE3
ANY13(SplitXRGBRow_Any_SSSE3, SplitXRGBRow_SSSE3, 4, 7)
#endif
#ifdef HAS_SPLITXRGBROW_AVX2
ANY13(SplitXRGBRow_Any_AVX2, SplitXRGBRow_AVX2, 4, 15)
#endif
#ifdef HAS_SPLITXRGBROW_NEON
ANY13(SplitXRGBRow_Any_NEON, SplitXRGBRow_NEON, 4, 15)
#endif

// Any 1 to 4.  Outputs ARGB planes.
#define ANY14(NAMEANY, ANY_SIMD, BPP, MASK)                                    \
  void NAMEANY(const uint8_t* src_ptr, uint8_t* dst_r, uint8_t* dst_g,         \
               uint8_t* dst_b, uint8_t* dst_a, int width) {                    \
    SIMD_ALIGNED(uint8_t temp[16 * 8]);                                        \
    memset(temp, 0, 16 * 4); /* for msan */                                    \
    int r = width & MASK;                                                      \
    int n = width & ~MASK;                                                     \
    if (n > 0) {                                                               \
      ANY_SIMD(src_ptr, dst_r, dst_g, dst_b, dst_a, n);                        \
    }                                                                          \
    memcpy(temp, src_ptr + n * BPP, r * BPP);                                  \
    ANY_SIMD(temp, temp + 16 * 4, temp + 16 * 5, temp + 16 * 6, temp + 16 * 7, \
             MASK + 1);                                                        \
    memcpy(dst_r + n, temp + 16 * 4, r);                                       \
    memcpy(dst_g + n, temp + 16 * 5, r);                                       \
    memcpy(dst_b + n, temp + 16 * 6, r);                                       \
    memcpy(dst_a + n, temp + 16 * 7, r);                                       \
  }

#ifdef HAS_SPLITARGBROW_SSE2
ANY14(SplitARGBRow_Any_SSE2, SplitARGBRow_SSE2, 4, 7)
#endif
#ifdef HAS_SPLITARGBROW_SSSE3
ANY14(SplitARGBRow_Any_SSSE3, SplitARGBRow_SSSE3, 4, 7)
#endif
#ifdef HAS_SPLITARGBROW_AVX2
ANY14(SplitARGBRow_Any_AVX2, SplitARGBRow_AVX2, 4, 15)
#endif
#ifdef HAS_SPLITARGBROW_NEON
ANY14(SplitARGBRow_Any_NEON, SplitARGBRow_NEON, 4, 15)
#endif

// Any 1 to 2 with source stride (2 rows of source).  Outputs UV planes.
// 128 byte row allows for 32 avx ARGB pixels.
#define ANY12S(NAMEANY, ANY_SIMD, UVSHIFT, BPP, MASK)                        \
  void NAMEANY(const uint8_t* src_ptr, int src_stride, uint8_t* dst_u,       \
               uint8_t* dst_v, int width) {                                  \
    SIMD_ALIGNED(uint8_t temp[128 * 4]);                                     \
    memset(temp, 0, 128 * 2); /* for msan */                                 \
    int r = width & MASK;                                                    \
    int n = width & ~MASK;                                                   \
    if (n > 0) {                                                             \
      ANY_SIMD(src_ptr, src_stride, dst_u, dst_v, n);                        \
    }                                                                        \
    memcpy(temp, src_ptr + (n >> UVSHIFT) * BPP, SS(r, UVSHIFT) * BPP);      \
    memcpy(temp + 128, src_ptr + src_stride + (n >> UVSHIFT) * BPP,          \
           SS(r, UVSHIFT) * BPP);                                            \
    if ((width & 1) && UVSHIFT == 0) { /* repeat last pixel for subsample */ \
      memcpy(temp + SS(r, UVSHIFT) * BPP, temp + SS(r, UVSHIFT) * BPP - BPP, \
             BPP);                                                           \
      memcpy(temp + 128 + SS(r, UVSHIFT) * BPP,                              \
             temp + 128 + SS(r, UVSHIFT) * BPP - BPP, BPP);                  \
    }                                                                        \
    ANY_SIMD(temp, 128, temp + 256, temp + 384, MASK + 1);                   \
    memcpy(dst_u + (n >> 1), temp + 256, SS(r, 1));                          \
    memcpy(dst_v + (n >> 1), temp + 384, SS(r, 1));                          \
  }

#ifdef HAS_ARGBTOUVROW_AVX2
ANY12S(ARGBToUVRow_Any_AVX2, ARGBToUVRow_AVX2, 0, 4, 31)
#endif
#ifdef HAS_ABGRTOUVROW_AVX2
ANY12S(ABGRToUVRow_Any_AVX2, ABGRToUVRow_AVX2, 0, 4, 31)
#endif
#ifdef HAS_ARGBTOUVJROW_AVX2
ANY12S(ARGBToUVJRow_Any_AVX2, ARGBToUVJRow_AVX2, 0, 4, 31)
#endif
#ifdef HAS_ARGBTOUVROW_SSSE3
ANY12S(ARGBToUVRow_Any_SSSE3, ARGBToUVRow_SSSE3, 0, 4, 15)
ANY12S(ARGBToUVJRow_Any_SSSE3, ARGBToUVJRow_SSSE3, 0, 4, 15)
ANY12S(BGRAToUVRow_Any_SSSE3, BGRAToUVRow_SSSE3, 0, 4, 15)
ANY12S(ABGRToUVRow_Any_SSSE3, ABGRToUVRow_SSSE3, 0, 4, 15)
ANY12S(RGBAToUVRow_Any_SSSE3, RGBAToUVRow_SSSE3, 0, 4, 15)
#endif
#ifdef HAS_YUY2TOUVROW_AVX2
ANY12S(YUY2ToUVRow_Any_AVX2, YUY2ToUVRow_AVX2, 1, 4, 31)
ANY12S(UYVYToUVRow_Any_AVX2, UYVYToUVRow_AVX2, 1, 4, 31)
#endif
#ifdef HAS_YUY2TOUVROW_SSE2
ANY12S(YUY2ToUVRow_Any_SSE2, YUY2ToUVRow_SSE2, 1, 4, 15)
ANY12S(UYVYToUVRow_Any_SSE2, UYVYToUVRow_SSE2, 1, 4, 15)
#endif
#ifdef HAS_ARGBTOUVROW_NEON
ANY12S(ARGBToUVRow_Any_NEON, ARGBToUVRow_NEON, 0, 4, 15)
#endif
#ifdef HAS_ARGBTOUVROW_MSA
ANY12S(ARGBToUVRow_Any_MSA, ARGBToUVRow_MSA, 0, 4, 31)
#endif
#ifdef HAS_ARGBTOUVROW_MMI
ANY12S(ARGBToUVRow_Any_MMI, ARGBToUVRow_MMI, 0, 4, 15)
#endif
#ifdef HAS_ARGBTOUVJROW_NEON
ANY12S(ARGBToUVJRow_Any_NEON, ARGBToUVJRow_NEON, 0, 4, 15)
#endif
#ifdef HAS_ARGBTOUVJROW_MSA
ANY12S(ARGBToUVJRow_Any_MSA, ARGBToUVJRow_MSA, 0, 4, 31)
#endif
#ifdef HAS_ARGBTOUVJROW_MMI
ANY12S(ARGBToUVJRow_Any_MMI, ARGBToUVJRow_MMI, 0, 4, 15)
#endif
#ifdef HAS_BGRATOUVROW_NEON
ANY12S(BGRAToUVRow_Any_NEON, BGRAToUVRow_NEON, 0, 4, 15)
#endif
#ifdef HAS_BGRATOUVROW_MSA
ANY12S(BGRAToUVRow_Any_MSA, BGRAToUVRow_MSA, 0, 4, 15)
#endif
#ifdef HAS_BGRATOUVROW_MMI
ANY12S(BGRAToUVRow_Any_MMI, BGRAToUVRow_MMI, 0, 4, 15)
#endif
#ifdef HAS_ABGRTOUVROW_NEON
ANY12S(ABGRToUVRow_Any_NEON, ABGRToUVRow_NEON, 0, 4, 15)
#endif
#ifdef HAS_ABGRTOUVROW_MSA
ANY12S(ABGRToUVRow_Any_MSA, ABGRToUVRow_MSA, 0, 4, 15)
#endif
#ifdef HAS_ABGRTOUVROW_MMI
ANY12S(ABGRToUVRow_Any_MMI, ABGRToUVRow_MMI, 0, 4, 15)
#endif
#ifdef HAS_RGBATOUVROW_NEON
ANY12S(RGBAToUVRow_Any_NEON, RGBAToUVRow_NEON, 0, 4, 15)
#endif
#ifdef HAS_RGBATOUVROW_MSA
ANY12S(RGBAToUVRow_Any_MSA, RGBAToUVRow_MSA, 0, 4, 15)
#endif
#ifdef HAS_RGBATOUVROW_MMI
ANY12S(RGBAToUVRow_Any_MMI, RGBAToUVRow_MMI, 0, 4, 15)
#endif
#ifdef HAS_RGB24TOUVROW_NEON
ANY12S(RGB24ToUVRow_Any_NEON, RGB24ToUVRow_NEON, 0, 3, 15)
#endif
#ifdef HAS_RGB24TOUVROW_MSA
ANY12S(RGB24ToUVRow_Any_MSA, RGB24ToUVRow_MSA, 0, 3, 15)
#endif
#ifdef HAS_RGB24TOUVROW_MMI
ANY12S(RGB24ToUVRow_Any_MMI, RGB24ToUVRow_MMI, 0, 3, 15)
#endif
#ifdef HAS_RAWTOUVROW_NEON
ANY12S(RAWToUVRow_Any_NEON, RAWToUVRow_NEON, 0, 3, 15)
#endif
#ifdef HAS_RAWTOUVROW_MSA
ANY12S(RAWToUVRow_Any_MSA, RAWToUVRow_MSA, 0, 3, 15)
#endif
#ifdef HAS_RAWTOUVROW_MMI
ANY12S(RAWToUVRow_Any_MMI, RAWToUVRow_MMI, 0, 3, 15)
#endif
#ifdef HAS_RGB565TOUVROW_NEON
ANY12S(RGB565ToUVRow_Any_NEON, RGB565ToUVRow_NEON, 0, 2, 15)
#endif
#ifdef HAS_RGB565TOUVROW_MSA
ANY12S(RGB565ToUVRow_Any_MSA, RGB565ToUVRow_MSA, 0, 2, 15)
#endif
#ifdef HAS_RGB565TOUVROW_MMI
ANY12S(RGB565ToUVRow_Any_MMI, RGB565ToUVRow_MMI, 0, 2, 15)
#endif
#ifdef HAS_ARGB1555TOUVROW_NEON
ANY12S(ARGB1555ToUVRow_Any_NEON, ARGB1555ToUVRow_NEON, 0, 2, 15)
#endif
#ifdef HAS_ARGB1555TOUVROW_MSA
ANY12S(ARGB1555ToUVRow_Any_MSA, ARGB1555ToUVRow_MSA, 0, 2, 15)
#endif
#ifdef HAS_ARGB1555TOUVROW_MMI
ANY12S(ARGB1555ToUVRow_Any_MMI, ARGB1555ToUVRow_MMI, 0, 2, 15)
#endif
#ifdef HAS_ARGB4444TOUVROW_NEON
ANY12S(ARGB4444ToUVRow_Any_NEON, ARGB4444ToUVRow_NEON, 0, 2, 15)
#endif
#ifdef HAS_ARGB4444TOUVROW_MMI
ANY12S(ARGB4444ToUVRow_Any_MMI, ARGB4444ToUVRow_MMI, 0, 2, 15)
#endif
#ifdef HAS_YUY2TOUVROW_NEON
ANY12S(YUY2ToUVRow_Any_NEON, YUY2ToUVRow_NEON, 1, 4, 15)
#endif
#ifdef HAS_UYVYTOUVROW_NEON
ANY12S(UYVYToUVRow_Any_NEON, UYVYToUVRow_NEON, 1, 4, 15)
#endif
#ifdef HAS_YUY2TOUVROW_MSA
ANY12S(YUY2ToUVRow_Any_MSA, YUY2ToUVRow_MSA, 1, 4, 31)
#endif
#ifdef HAS_YUY2TOUVROW_MMI
ANY12S(YUY2ToUVRow_Any_MMI, YUY2ToUVRow_MMI, 1, 4, 15)
#endif
#ifdef HAS_UYVYTOUVROW_MSA
ANY12S(UYVYToUVRow_Any_MSA, UYVYToUVRow_MSA, 1, 4, 31)
#endif
#ifdef HAS_UYVYTOUVROW_MMI
ANY12S(UYVYToUVRow_Any_MMI, UYVYToUVRow_MMI, 1, 4, 15)
#endif
#undef ANY12S

// Any 1 to 1 with source stride (2 rows of source).  Outputs UV plane.
// 128 byte row allows for 32 avx ARGB pixels.
#define ANY11S(NAMEANY, ANY_SIMD, UVSHIFT, BPP, MASK)                        \
  void NAMEANY(const uint8_t* src_ptr, int src_stride, uint8_t* dst_vu,      \
               int width) {                                                  \
    SIMD_ALIGNED(uint8_t temp[128 * 3]);                                     \
    memset(temp, 0, 128 * 2); /* for msan */                                 \
    int r = width & MASK;                                                    \
    int n = width & ~MASK;                                                   \
    if (n > 0) {                                                             \
      ANY_SIMD(src_ptr, src_stride, dst_vu, n);                              \
    }                                                                        \
    memcpy(temp, src_ptr + (n >> UVSHIFT) * BPP, SS(r, UVSHIFT) * BPP);      \
    memcpy(temp + 128, src_ptr + src_stride + (n >> UVSHIFT) * BPP,          \
           SS(r, UVSHIFT) * BPP);                                            \
    if ((width & 1) && UVSHIFT == 0) { /* repeat last pixel for subsample */ \
      memcpy(temp + SS(r, UVSHIFT) * BPP, temp + SS(r, UVSHIFT) * BPP - BPP, \
             BPP);                                                           \
      memcpy(temp + 128 + SS(r, UVSHIFT) * BPP,                              \
             temp + 128 + SS(r, UVSHIFT) * BPP - BPP, BPP);                  \
    }                                                                        \
    ANY_SIMD(temp, 128, temp + 256, MASK + 1);                               \
    memcpy(dst_vu + (n >> 1) * 2, temp + 256, SS(r, 1) * 2);                 \
  }

#ifdef HAS_AYUVTOVUROW_NEON
ANY11S(AYUVToUVRow_Any_NEON, AYUVToUVRow_NEON, 0, 4, 15)
ANY11S(AYUVToVURow_Any_NEON, AYUVToVURow_NEON, 0, 4, 15)
#endif
#undef ANY11S

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
