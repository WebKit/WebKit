/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/convert_from_argb.h"  // For ArgbConstants
#include "libyuv/row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Note: every register written by an inline asm block (vector, general
// purpose, mask/predicate, "cc" for flags and "memory" for stores) must be
// listed in its clobber list, even when the ABI treats it as caller-saved.
// With LTO/LTCG the compiler may keep values live in any register across the
// asm statement, and other OS ABIs (e.g. Windows) differ in which registers
// are callee-saved. List "memory" first, then "cc", then registers.

// This module is for GCC x86 and x64.
#if !defined(LIBYUV_DISABLE_X86) &&               \
    (defined(__x86_64__) || defined(__i386__)) && \
    !defined(LIBYUV_ENABLE_ROWWIN)

// Note: for avx and avx512 declare clobber as xmm registers due to
// clang for windows needing to preserve xmm registers but not saving
// them if declared as ymm or zmm.

#if defined(HAS_ARGBTOYMATRIXROW_SSSE3) || defined(HAS_ARGBGRAYROW_SSSE3)

// Constants for ARGB

// JPeg full range.
static const uvec8 kARGBToYJ = {29u, 150u, 77u, 0u, 29u, 150u, 77u, 0u,
                                29u, 150u, 77u, 0u, 29u, 150u, 77u, 0u};

#endif  // defined(HAS_ARGBTOYMATRIXROW_SSSE3) || defined(HAS_ARGBGRAYROW_SSSE3)

#if defined(HAS_ARGBTOYMATRIXROW_SSSE3) || defined(HAS_I422TOARGBROW_SSSE3)
// Constants for BGRA

// Constants for ABGR

// Constants for RGBA.
// 126 (7e) - (-109..110) = 16..235

static const uvec16 kSub128 = {0x8080u, 0x8080u, 0x8080u, 0x8080u,
                               0x8080u, 0x8080u, 0x8080u, 0x8080u};

#endif  // defined(HAS_ARGBTOYMATRIXROW_SSSE3) ||
        // defined(HAS_I422TOARGBROW_SSSE3)

#if defined(HAS_RGB24TOARGBROW_SSSE3) || defined(HAS_RGB24TOARGBROW_AVX2) || \
    defined(HAS_RGB24TOARGBROW_AVX512BW) ||                                  \
    defined(HAS_RGBTOYMATRIXROW_AVX512BW)
// Shuffle table for converting RGB24 to ARGB.
static const uvec8 kShuffleMaskRGB24ToARGB[2] = {
    {0u, 1u, 2u, 128u, 3u, 4u, 5u, 128u, 6u, 7u, 8u, 128u, 9u, 10u, 11u, 128u},
    {4u, 5u, 6u, 128u, 7u, 8u, 9u, 128u, 10u, 11u, 12u, 128u, 13u, 14u, 15u,
     128u}};
#endif  // defined(HAS_RGB24TOARGBROW_SSSE3) ||
        // defined(HAS_RGB24TOARGBROW_AVX2) ||
        // defined(HAS_RGB24TOARGBROW_AVX512BW) ||
        // defined(HAS_RGBTOYMATRIXROW_AVX512BW)

#if defined(HAS_RAWTOARGBROW_SSSE3) || defined(HAS_RAWTOARGBROW_AVX2)
// Shuffle table for converting RAW to ARGB.
static const uvec8 kShuffleMaskRAWToARGB = {
    2u, 1u, 0u, 128u, 5u, 4u, 3u, 128u, 8u, 7u, 6u, 128u, 11u, 10u, 9u, 128u};
// Shuffle table for converting RAW to ARGB.  Last 12
static const uvec8 kShuffleMaskRAWToARGB_0 = {6u,  5u,   4u,  128u, 9u,  8u,
                                              7u,  128u, 12u, 11u,  10u, 128u,
                                              15u, 14u,  13u, 128u};
#endif  // defined(HAS_RAWTOARGBROW_SSSE3) || defined(HAS_RAWTOARGBROW_AVX2)
#if defined(HAS_RAWTOARGBROW_AVX512BW)
static const uvec8 kShuffleMaskRAWToARGB_AVX512BW[2] = {
    {2u, 1u, 0u, 128u, 5u, 4u, 3u, 128u, 8u, 7u, 6u, 128u, 11u, 10u, 9u, 128u},
    {6u, 5u, 4u, 128u, 9u, 8u, 7u, 128u, 12u, 11u, 10u, 128u, 15u, 14u, 13u,
     128u}};
#endif  // defined(HAS_RAWTOARGBROW_AVX512BW)
#if defined(HAS_RAWTORGB24ROW_AVX2) || defined(HAS_RAWTORGB24ROW_AVX512BW)
static const uvec8 kShuffleMaskRAWToRGB24_AVX2[2] = {
    {2u, 1u, 0u, 5u, 4u, 3u, 8u, 7u, 6u, 11u, 10u, 9u, 128u, 128u, 128u, 128u},
    {6u, 5u, 4u, 9u, 8u, 7u, 12u, 11u, 10u, 15u, 14u, 13u, 128u, 128u, 128u,
     128u}};
#endif  // defined(HAS_RAWTORGB24ROW_AVX2) ||
        // defined(HAS_RAWTORGB24ROW_AVX512BW)
#if defined(HAS_RAWTORGB24ROW_AVX2)
static const uint32_t kPermd0_AVX2[8] = {0, 1, 2, 4, 5, 6, 3, 7};
static const uint32_t kPermd1_AVX2[8] = {2, 4, 5, 6, 3, 7, 0, 1};
static const uint32_t kPermd2_AVX2[8] = {5, 6, 3, 7, 0, 1, 2, 4};
#endif  // defined(HAS_RAWTORGB24ROW_AVX2)
#if defined(HAS_RAWTORGB24ROW_AVX512BW) ||   \
    defined(HAS_ARGBTORGB24ROW_AVX512BW) || \
    defined(HAS_ARGBTORAWROW_AVX512BW)
static const uint32_t kPermdRGB24_0_AVX512BW[16] = {
    0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 16, 17, 18, 20};
static const uint32_t kPermdRGB24_1_AVX512BW[16] = {
    5, 6, 8, 9, 10, 12, 13, 14, 16, 17, 18, 20, 21, 22, 24, 25};
static const uint32_t kPermdRGB24_2_AVX512BW[16] = {
    10, 12, 13, 14, 16, 17, 18, 20, 21, 22, 24, 25, 26, 28, 29, 30};
#endif  // defined(HAS_RAWTORGB24ROW_AVX512BW) ||
        // defined(HAS_ARGBTORGB24ROW_AVX512BW) ||
        // defined(HAS_ARGBTORAWROW_AVX512BW)
#if defined(HAS_RAWTORGB24ROW_AVX512VBMI)
static const uint8_t kPermRAWToRGB24_0[64] = {
    0x02, 0x01, 0x00, 0x05, 0x04, 0x03, 0x08, 0x07, 0x06, 0x0b, 0x0a,
    0x09, 0x0e, 0x0d, 0x0c, 0x11, 0x10, 0x0f, 0x14, 0x13, 0x12, 0x17,
    0x16, 0x15, 0x1a, 0x19, 0x18, 0x1d, 0x1c, 0x1b, 0x20, 0x1f, 0x1e,
    0x23, 0x22, 0x21, 0x26, 0x25, 0x24, 0x29, 0x28, 0x27, 0x2c, 0x2b,
    0x2a, 0x2f, 0x2e, 0x2d, 0x32, 0x31, 0x30, 0x35, 0x34, 0x33, 0x38,
    0x37, 0x36, 0x3b, 0x3a, 0x39, 0x3e, 0x3d, 0x3c, 0x41};
static const uint8_t kPermRAWToRGB24_1[64] = {
    0x00, 0x7f, 0x04, 0x03, 0x02, 0x07, 0x06, 0x05, 0x0a, 0x09, 0x08,
    0x0d, 0x0c, 0x0b, 0x10, 0x0f, 0x0e, 0x13, 0x12, 0x11, 0x16, 0x15,
    0x14, 0x19, 0x18, 0x17, 0x1c, 0x1b, 0x1a, 0x1f, 0x1e, 0x1d, 0x22,
    0x21, 0x20, 0x25, 0x24, 0x23, 0x28, 0x27, 0x26, 0x2b, 0x2a, 0x29,
    0x2e, 0x2d, 0x2c, 0x31, 0x30, 0x2f, 0x34, 0x33, 0x32, 0x37, 0x36,
    0x35, 0x3a, 0x39, 0x38, 0x3d, 0x3c, 0x3b, 0x00, 0x3f};
static const uint8_t kPermRAWToRGB24_2[64] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
    0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
    0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
    0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x40, 0x3f};
static const uint8_t kPermRAWToRGB24_3[64] = {
    0x3e, 0x43, 0x42, 0x41, 0x46, 0x45, 0x44, 0x49, 0x48, 0x47, 0x4c,
    0x4b, 0x4a, 0x4f, 0x4e, 0x4d, 0x52, 0x51, 0x50, 0x55, 0x54, 0x53,
    0x58, 0x57, 0x56, 0x5b, 0x5a, 0x59, 0x5e, 0x5d, 0x5c, 0x61, 0x60,
    0x5f, 0x64, 0x63, 0x62, 0x67, 0x66, 0x65, 0x6a, 0x69, 0x68, 0x6d,
    0x6c, 0x6b, 0x70, 0x6f, 0x6e, 0x73, 0x72, 0x71, 0x76, 0x75, 0x74,
    0x79, 0x78, 0x77, 0x7c, 0x7b, 0x7a, 0x7f, 0x7e, 0x7d};
#endif  // defined(HAS_RAWTORGB24ROW_AVX512VBMI)

#if defined(HAS_RAWTORGBAROW_SSSE3)
// Shuffle table for converting RAW to RGBA.
static const uvec8 kShuffleMaskRAWToRGBA = {
    128u, 2u, 1u, 0u, 128u, 5u, 4u, 3u, 128u, 8u, 7u, 6u, 128u, 11u, 10u, 9u};
#endif  // defined(HAS_RAWTORGBAROW_SSSE3)

#if defined(HAS_RAWTORGB24ROW_SSSE3)
// Shuffle table for converting RAW to RGB24.  First 8.
static const uvec8 kShuffleMaskRAWToRGB24_0 = {
    2u,   1u,   0u,   5u,   4u,   3u,   8u,   7u,
    128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u};

// Shuffle table for converting RAW to RGB24.  Middle 8.
static const uvec8 kShuffleMaskRAWToRGB24_1 = {
    2u,   7u,   6u,   5u,   10u,  9u,   8u,   13u,
    128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u};

// Shuffle table for converting RAW to RGB24.  Last 8.
static const uvec8 kShuffleMaskRAWToRGB24_2 = {
    8u,   7u,   12u,  11u,  10u,  15u,  14u,  13u,
    128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u};
#endif  // defined(HAS_RAWTORGB24ROW_SSSE3)

// Shuffle table for converting ARGB to RGB24.
// and for I422ToRGB24.  First 8 + next 4
static const uvec8 kShuffleMaskARGBToRGB24[2] = {
    {0u, 1u, 2u, 4u, 5u, 6u, 8u, 9u, 10u, 12u, 13u, 14u, 128u, 128u, 128u,
     128u},
    {0u, 1u, 2u, 4u, 5u, 6u, 8u, 9u, 128u, 128u, 128u, 128u, 10u, 12u, 13u,
     14u}};

// Shuffle table for converting ARGB to RAW.
static const uvec8 kShuffleMaskARGBToRAW = {
    2u, 1u, 0u, 6u, 5u, 4u, 10u, 9u, 8u, 14u, 13u, 12u, 128u, 128u, 128u, 128u};

// YUY2 shuf 16 Y to 32 Y.
static const vec8 kShuffleYUY2Y = {0, 0, 2,  2,  4,  4,  6,  6,
                                   8, 8, 10, 10, 12, 12, 14, 14};

// YUY2 shuf 8 UV to 16 UV.
static const vec8 kShuffleYUY2UV = {1, 3,  1, 3,  5,  7,  5,  7,
                                    9, 11, 9, 11, 13, 15, 13, 15};

// UYVY shuf 16 Y to 32 Y.
static const vec8 kShuffleUYVYY = {1, 1, 3,  3,  5,  5,  7,  7,
                                   9, 9, 11, 11, 13, 13, 15, 15};

// UYVY shuf 8 UV to 16 UV.
static const vec8 kShuffleUYVYUV = {0, 2,  0, 2,  4,  6,  4,  6,
                                    8, 10, 8, 10, 12, 14, 12, 14};

// NV21 shuf 8 VU to 16 UV.
static const lvec8 kShuffleNV21 = {
    1, 0, 1, 0, 3, 2, 3, 2, 5, 4, 5, 4, 7, 6, 7, 6,
    1, 0, 1, 0, 3, 2, 3, 2, 5, 4, 5, 4, 7, 6, 7, 6,
};

#if defined(HAS_J400TOARGBROW_AVX2) || defined(HAS_J400TOARGBROW_AVX512BW)
static const uint8_t kShuffleMaskJ400ToARGB[64] = {
    0u,  0u,   0u,  128u, 1u,  1u,   1u,  128u, 2u,  2u,   2u,  128u, 3u,  3u,
    3u,  128u, 4u,  4u,   4u,  128u, 5u,  5u,   5u,  128u, 6u,  6u,   6u,  128u,
    7u,  7u,   7u,  128u, 8u,  8u,   8u,  128u, 9u,  9u,   9u,  128u, 10u, 10u,
    10u, 128u, 11u, 11u,  11u, 128u, 12u, 12u,  12u, 128u, 13u, 13u,  13u, 128u,
    14u, 14u,  14u, 128u, 15u, 15u,  15u, 128u};
#endif  // defined(HAS_J400TOARGBROW_AVX2) ||
        // defined(HAS_J400TOARGBROW_AVX512BW)

#ifdef HAS_J400TOARGBROW_AVX2
void J400ToARGBRow_AVX2(const uint8_t* src_y, uint8_t* dst_argb, int width) {
  asm volatile(
      "vpcmpeqb    %%ymm7,%%ymm7,%%ymm7          \n"
      "vpslld      $0x18,%%ymm7,%%ymm7           \n"
      "vmovdqu     (%3),%%ymm5                   \n"
      "vmovdqu     0x20(%3),%%ymm6               \n"

      LABELALIGN
      "1:          \n"
      "vbroadcasti128 (%0),%%ymm0                \n"
      "vpshufb     %%ymm5,%%ymm0,%%ymm1          \n"
      "vpshufb     %%ymm6,%%ymm0,%%ymm2          \n"
      "vpor        %%ymm7,%%ymm1,%%ymm1          \n"
      "vpor        %%ymm7,%%ymm2,%%ymm2          \n"
      "vmovdqu     %%ymm1,(%1)                   \n"
      "vmovdqu     %%ymm2,0x20(%1)               \n"
      "lea         0x10(%0),%0                   \n"
      "lea         0x40(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_y),                 // %0
        "+r"(dst_argb),              // %1
        "+r"(width)                  // %2
      : "r"(kShuffleMaskJ400ToARGB)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm5", "xmm6", "xmm7");
}
#endif  // HAS_J400TOARGBROW_AVX2

#ifdef HAS_J400TOARGBROW_AVX512BW
void J400ToARGBRow_AVX512BW(const uint8_t* src_y,
                            uint8_t* dst_argb,
                            int width) {
  asm volatile(
      "vpternlogd  $0xff,%%zmm16,%%zmm16,%%zmm16 \n"  // 0xffffffff
      "vpslld      $0x18,%%zmm16,%%zmm16         \n"  // 0xff000000
      "vmovdqu64   %3,%%zmm5                     \n"

      LABELALIGN
      "1:          \n"
      "vbroadcasti32x4 (%0),%%zmm0               \n"
      "vbroadcasti32x4 0x10(%0),%%zmm1           \n"
      "vpshufb     %%zmm5,%%zmm0,%%zmm0          \n"
      "vpshufb     %%zmm5,%%zmm1,%%zmm1          \n"
      "vpord       %%zmm16,%%zmm0,%%zmm0         \n"
      "vpord       %%zmm16,%%zmm1,%%zmm1         \n"
      "vmovdqu64   %%zmm0,(%1)                   \n"
      "vmovdqu64   %%zmm1,0x40(%1)               \n"
      "lea         0x20(%0),%0                   \n"
      "lea         0x80(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_y),                 // %0
        "+r"(dst_argb),              // %1
        "+r"(width)                  // %2
      : "m"(kShuffleMaskJ400ToARGB)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm5", "xmm16");
}
#endif  // HAS_J400TOARGBROW_AVX512BW

#ifdef HAS_RGB24TOARGBROW_SSSE3
void RGB24ToARGBRow_SSSE3(const uint8_t* src_rgb24,
                          uint8_t* dst_argb,
                          int width) {
  asm volatile(
      "pcmpeqb     %%xmm5,%%xmm5                 \n"  // 0xff000000
      "pslld       $0x18,%%xmm5                  \n"
      "movdqa      %3,%%xmm4                     \n"

      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "movdqu      0x20(%0),%%xmm3               \n"
      "lea         0x30(%0),%0                   \n"
      "movdqa      %%xmm3,%%xmm2                 \n"
      "palignr     $0x8,%%xmm1,%%xmm2            \n"
      "pshufb      %%xmm4,%%xmm2                 \n"
      "por         %%xmm5,%%xmm2                 \n"
      "palignr     $0xc,%%xmm0,%%xmm1            \n"
      "pshufb      %%xmm4,%%xmm0                 \n"
      "movdqu      %%xmm2,0x20(%1)               \n"
      "por         %%xmm5,%%xmm0                 \n"
      "pshufb      %%xmm4,%%xmm1                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "por         %%xmm5,%%xmm1                 \n"
      "palignr     $0x4,%%xmm3,%%xmm3            \n"
      "pshufb      %%xmm4,%%xmm3                 \n"
      "movdqu      %%xmm1,0x10(%1)               \n"
      "por         %%xmm5,%%xmm3                 \n"
      "movdqu      %%xmm3,0x30(%1)               \n"
      "lea         0x40(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_rgb24),                 // %0
        "+r"(dst_argb),                  // %1
        "+r"(width)                      // %2
      : "m"(kShuffleMaskRGB24ToARGB[0])  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_RGB24TOARGBROW_SSSE3

#ifdef HAS_RGB24TOARGBROW_AVX2
void RGB24ToARGBRow_AVX2(const uint8_t* src_rgb24,
                         uint8_t* dst_argb,
                         int width) {
  asm volatile(
      "vpcmpeqb    %%ymm6,%%ymm6,%%ymm6          \n"  // 0xff000000
      "vpslld      $0x18,%%ymm6,%%ymm6           \n"
      "vbroadcasti128 %3,%%ymm4                  \n"
      "vbroadcasti128 %4,%%ymm5                  \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%xmm0                   \n"  // first 12
      "vinserti128 $1,12(%0),%%ymm0,%%ymm0       \n"  // second 12
      "vmovdqu     24(%0),%%xmm1                 \n"  // third 12
      "vinserti128 $1,36(%0),%%ymm1,%%ymm1       \n"  // forth 12
      "vmovdqu     48(%0),%%xmm2                 \n"  // fifth 12
      "vinserti128 $1,60(%0),%%ymm2,%%ymm2       \n"  // sixth 12
      "vmovdqu     68(%0),%%xmm3                 \n"  // seventh 12
      "vinserti128 $1,80(%0),%%ymm3,%%ymm3       \n"  // eighth 12
      "lea         96(%0),%0                     \n"
      "vpshufb     %%ymm4,%%ymm0,%%ymm0          \n"
      "vpshufb     %%ymm4,%%ymm1,%%ymm1          \n"
      "vpshufb     %%ymm4,%%ymm2,%%ymm2          \n"
      "vpshufb     %%ymm5,%%ymm3,%%ymm3          \n"
      "vpor        %%ymm6,%%ymm0,%%ymm0          \n"
      "vpor        %%ymm6,%%ymm1,%%ymm1          \n"
      "vpor        %%ymm6,%%ymm2,%%ymm2          \n"
      "vpor        %%ymm6,%%ymm3,%%ymm3          \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "vmovdqu     %%ymm1,0x20(%1)               \n"
      "vmovdqu     %%ymm2,0x40(%1)               \n"
      "vmovdqu     %%ymm3,0x60(%1)               \n"
      "lea         0x80(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_rgb24),                  // %0
        "+r"(dst_argb),                   // %1
        "+r"(width)                       // %2
      : "m"(kShuffleMaskRGB24ToARGB[0]),  // %3
        "m"(kShuffleMaskRGB24ToARGB[1])   // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}
#endif  // HAS_RGB24TOARGBROW_AVX2

#ifdef HAS_RAWTOARGBROW_SSSE3
void RAWToARGBRow_SSSE3(const uint8_t* src_raw, uint8_t* dst_argb, int width) {
  asm volatile(
      "pcmpeqb     %%xmm6,%%xmm6                 \n"  // 0xff000000
      "pslld       $0x18,%%xmm6                  \n"
      "movdqa      %3,%%xmm4                     \n"
      "movdqa      %4,%%xmm5                     \n"

      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      12(%0),%%xmm1                 \n"
      "movdqu      24(%0),%%xmm2                 \n"
      "movdqu      32(%0),%%xmm3                 \n"
      "lea         0x30(%0),%0                   \n"
      "pshufb      %%xmm4,%%xmm0                 \n"
      "pshufb      %%xmm4,%%xmm1                 \n"
      "pshufb      %%xmm4,%%xmm2                 \n"
      "pshufb      %%xmm5,%%xmm3                 \n"
      "por         %%xmm6,%%xmm0                 \n"
      "por         %%xmm6,%%xmm1                 \n"
      "por         %%xmm6,%%xmm2                 \n"
      "por         %%xmm6,%%xmm3                 \n"
      "movdqu      %%xmm0,0x00(%1)               \n"
      "movdqu      %%xmm1,0x10(%1)               \n"
      "movdqu      %%xmm2,0x20(%1)               \n"
      "movdqu      %%xmm3,0x30(%1)               \n"
      "lea         0x40(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_raw),                // %0
        "+r"(dst_argb),               // %1
        "+r"(width)                   // %2
      : "m"(kShuffleMaskRAWToARGB),   // %3
        "m"(kShuffleMaskRAWToARGB_0)  // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}
#endif  // HAS_RAWTOARGBROW_SSSE3

#ifdef HAS_RAWTOARGBROW_AVX2
void RAWToARGBRow_AVX2(const uint8_t* src_raw, uint8_t* dst_argb, int width) {
  asm volatile(
      "vpcmpeqb    %%ymm6,%%ymm6,%%ymm6          \n"  // 0xff000000
      "vpslld      $0x18,%%ymm6,%%ymm6           \n"
      "vbroadcasti128 %3,%%ymm4                  \n"  //
      "vbroadcasti128 %4,%%ymm5                  \n"  //

      LABELALIGN  //
      "1:          \n"
      "vmovdqu     (%0),%%xmm0                   \n"  // first 12
      "vinserti128 $1,12(%0),%%ymm0,%%ymm0       \n"  // second 12
      "vmovdqu     24(%0),%%xmm1                 \n"  // third 12
      "vinserti128 $1,36(%0),%%ymm1,%%ymm1       \n"  // forth 12
      "vmovdqu     48(%0),%%xmm2                 \n"  // fifth 12
      "vinserti128 $1,60(%0),%%ymm2,%%ymm2       \n"  // sixth 12
      "vmovdqu     68(%0),%%xmm3                 \n"  // seventh 12
      "vinserti128 $1,80(%0),%%ymm3,%%ymm3       \n"  // eighth 12
      "lea         96(%0),%0                     \n"
      "vpshufb     %%ymm4,%%ymm0,%%ymm0          \n"
      "vpshufb     %%ymm4,%%ymm1,%%ymm1          \n"
      "vpshufb     %%ymm4,%%ymm2,%%ymm2          \n"
      "vpshufb     %%ymm5,%%ymm3,%%ymm3          \n"
      "vpor        %%ymm6,%%ymm0,%%ymm0          \n"
      "vpor        %%ymm6,%%ymm1,%%ymm1          \n"
      "vpor        %%ymm6,%%ymm2,%%ymm2          \n"
      "vpor        %%ymm6,%%ymm3,%%ymm3          \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "vmovdqu     %%ymm1,0x20(%1)               \n"
      "vmovdqu     %%ymm2,0x40(%1)               \n"
      "vmovdqu     %%ymm3,0x60(%1)               \n"
      "lea         0x80(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_raw),                // %0
        "+r"(dst_argb),               // %1
        "+r"(width)                   // %2
      : "m"(kShuffleMaskRAWToARGB),   // %3
        "m"(kShuffleMaskRAWToARGB_0)  // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}
#endif  // HAS_RAWTOARGBROW_AVX2

#if defined(HAS_RAWTOARGBROW_AVX512BW) ||   \
    defined(HAS_RGB24TOARGBROW_AVX512BW) || \
    defined(HAS_RAWTORGB24ROW_AVX512BW) ||  \
    defined(HAS_RGBTOYMATRIXROW_AVX512BW)
static const uint64_t kPermqRGB24ToARGB0_AVX512BW[8] = {0, 1, 1, 2, 3, 4, 4, 5};
static const uint64_t kPermqRGB24ToARGB1_AVX512BW[8] = {6, 7,  7,  8,
                                                        9, 10, 10, 11};
static const uint64_t kPermqRGB24ToARGB2_AVX512BW[8] = {4, 5, 5, 6, 7, 8, 8, 9};
static const uint64_t kPermqRGB24ToARGB3_AVX512BW[8] = {2, 3, 3, 4, 5, 6, 6, 7};
#endif  // defined(HAS_RAWTOARGBROW_AVX512BW) ||
        // defined(HAS_RGB24TOARGBROW_AVX512BW) ||
        // defined(HAS_RAWTORGB24ROW_AVX512BW) ||
        // defined(HAS_RGBTOYMATRIXROW_AVX512BW)

#ifdef HAS_RAWTOARGBROW_AVX512BW
void RGBToARGBRow_AVX512BW(const uint8_t* src_raw,
                           uint8_t* dst_argb,
                           const uint32_t* shuffler,
                           int width) {
  asm volatile(
      "vmovdqu64   %4,%%zmm20                    \n"  // kPermqRGB24ToARGB0_AVX512BW
      "vmovdqu64   %5,%%zmm21                    \n"  // kPermqRGB24ToARGB1_AVX512BW
      "vmovdqu64   %6,%%zmm22                    \n"  // kPermqRGB24ToARGB2_AVX512BW
      "vmovdqu64   %7,%%zmm23                    \n"  // kPermqRGB24ToARGB3_AVX512BW
      "vbroadcasti64x4 %3,%%zmm4                 \n"  // shuffler (32 bytes)
      "vpternlogd  $0xff,%%zmm16,%%zmm16,%%zmm16 \n"  // 0xffffffff
      "vpslld      $0x18,%%zmm16,%%zmm16         \n"  // 0xff000000

      LABELALIGN
      "1:          \n"
      "vmovdqu8    (%0),%%zmm0                   \n"
      "vmovdqu8    64(%0),%%zmm1                 \n"
      "vmovdqu8    128(%0),%%zmm2                \n"
      "lea         192(%0),%0                    \n"
      "vpermq      %%zmm2,%%zmm23,%%zmm3         \n"
      "vmovdqa64   %%zmm22,%%zmm18               \n"
      "vpermi2q    %%zmm2,%%zmm1,%%zmm18         \n"
      "vmovdqa64   %%zmm21,%%zmm19               \n"
      "vpermi2q    %%zmm1,%%zmm0,%%zmm19         \n"
      "vpermq      %%zmm0,%%zmm20,%%zmm0         \n"
      "vmovdqa64   %%zmm19,%%zmm1                \n"
      "vmovdqa64   %%zmm18,%%zmm2                \n"
      "vpshufb     %%zmm4,%%zmm0,%%zmm0          \n"
      "vpshufb     %%zmm4,%%zmm1,%%zmm1          \n"
      "vpshufb     %%zmm4,%%zmm2,%%zmm2          \n"
      "vpshufb     %%zmm4,%%zmm3,%%zmm3          \n"
      "vpord       %%zmm16,%%zmm0,%%zmm0         \n"
      "vpord       %%zmm16,%%zmm1,%%zmm1         \n"
      "vpord       %%zmm16,%%zmm2,%%zmm2         \n"
      "vpord       %%zmm16,%%zmm3,%%zmm3         \n"
      "vmovdqu32   %%zmm0,(%1)                   \n"
      "vmovdqu32   %%zmm1,0x40(%1)               \n"
      "vmovdqu32   %%zmm2,0x80(%1)               \n"
      "vmovdqu32   %%zmm3,0xc0(%1)               \n"
      "lea         0x100(%1),%1                  \n"
      "sub         $0x40,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_raw),                     // %0
        "+r"(dst_argb),                    // %1
        "+r"(width)                        // %2
      : "m"(*shuffler),                    // %3
        "m"(kPermqRGB24ToARGB0_AVX512BW),  // %4
        "m"(kPermqRGB24ToARGB1_AVX512BW),  // %5
        "m"(kPermqRGB24ToARGB2_AVX512BW),  // %6
        "m"(kPermqRGB24ToARGB3_AVX512BW)   // %7
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm16",
        "xmm18", "xmm19", "xmm20", "xmm21", "xmm22", "xmm23");
}

void RAWToARGBRow_AVX512BW(const uint8_t* src_raw,
                           uint8_t* dst_argb,
                           int width) {
  RGBToARGBRow_AVX512BW(src_raw, dst_argb,
                        (const uint32_t*)&kShuffleMaskRAWToARGB_AVX512BW[0],
                        width);
}

void RGB24ToARGBRow_AVX512BW(const uint8_t* src_rgb24,
                             uint8_t* dst_argb,
                             int width) {
  RGBToARGBRow_AVX512BW(src_rgb24, dst_argb,
                        (const uint32_t*)&kShuffleMaskRGB24ToARGB[0], width);
}
#endif  // HAS_RAWTOARGBROW_AVX512BW

#ifdef HAS_RAWTORGBAROW_SSSE3
// Same code as RAWToARGB with different shuffler and A in low bits
void RAWToRGBARow_SSSE3(const uint8_t* src_raw, uint8_t* dst_rgba, int width) {
  asm volatile(
      "pcmpeqb     %%xmm5,%%xmm5                 \n"  // 0x000000ff
      "psrld       $24,%%xmm5                    \n"
      "movdqa      %3,%%xmm4                     \n"

      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "movdqu      0x20(%0),%%xmm3               \n"
      "lea         0x30(%0),%0                   \n"
      "movdqa      %%xmm3,%%xmm2                 \n"
      "palignr     $0x8,%%xmm1,%%xmm2            \n"
      "pshufb      %%xmm4,%%xmm2                 \n"
      "por         %%xmm5,%%xmm2                 \n"
      "palignr     $0xc,%%xmm0,%%xmm1            \n"
      "pshufb      %%xmm4,%%xmm0                 \n"
      "movdqu      %%xmm2,0x20(%1)               \n"
      "por         %%xmm5,%%xmm0                 \n"
      "pshufb      %%xmm4,%%xmm1                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "por         %%xmm5,%%xmm1                 \n"
      "palignr     $0x4,%%xmm3,%%xmm3            \n"
      "pshufb      %%xmm4,%%xmm3                 \n"
      "movdqu      %%xmm1,0x10(%1)               \n"
      "por         %%xmm5,%%xmm3                 \n"
      "movdqu      %%xmm3,0x30(%1)               \n"
      "lea         0x40(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_raw),              // %0
        "+r"(dst_rgba),             // %1
        "+r"(width)                 // %2
      : "m"(kShuffleMaskRAWToRGBA)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_RAWTORGBAROW_SSSE3

#ifdef HAS_RAWTORGB24ROW_SSSE3
void RAWToRGB24Row_SSSE3(const uint8_t* src_raw,
                         uint8_t* dst_rgb24,
                         int width) {
  asm volatile(
      "movdqa      %3,%%xmm3                     \n"
      "movdqa      %4,%%xmm4                     \n"
      "movdqa      %5,%%xmm5                     \n"

      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x4(%0),%%xmm1                \n"
      "movdqu      0x8(%0),%%xmm2                \n"
      "lea         0x18(%0),%0                   \n"
      "pshufb      %%xmm3,%%xmm0                 \n"
      "pshufb      %%xmm4,%%xmm1                 \n"
      "pshufb      %%xmm5,%%xmm2                 \n"
      "movq        %%xmm0,(%1)                   \n"
      "movq        %%xmm1,0x8(%1)                \n"
      "movq        %%xmm2,0x10(%1)               \n"
      "lea         0x18(%1),%1                   \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src_raw),                  // %0
        "+r"(dst_rgb24),                // %1
        "+r"(width)                     // %2
      : "m"(kShuffleMaskRAWToRGB24_0),  // %3
        "m"(kShuffleMaskRAWToRGB24_1),  // %4
        "m"(kShuffleMaskRAWToRGB24_2)   // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_RAWTORGB24ROW_SSSE3

#ifdef HAS_RAWTORGB24ROW_AVX2
void RAWToRGB24Row_AVX2(const uint8_t* src_raw, uint8_t* dst_rgb24, int width) {
  asm volatile(
      "vmovdqu     %3,%%ymm4                     \n"  // shuffler (32 bytes)
      "vmovdqu     %4,%%ymm5                     \n"  // kPermd0_AVX2
      "vmovdqu     %5,%%ymm6                     \n"  // kPermd1_AVX2
      "vmovdqu     %6,%%ymm7                     \n"  // kPermd2_AVX2

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"  // raw0
      "vmovdqu     32(%0),%%ymm1                 \n"  // raw1
      "vmovdqu     64(%0),%%ymm2                 \n"  // raw2
      "lea         96(%0),%0                     \n"

      "vperm2i128  $0x21,%%ymm1,%%ymm0,%%ymm3    \n"  // m01
      "vpermq      $0x94,%%ymm0,%%ymm0           \n"  // b0
      "vperm2i128  $0x21,%%ymm2,%%ymm1,%%ymm1    \n"  // m12
      "vpermq      $0xe9,%%ymm2,%%ymm2           \n"  // b3
      "vpermq      $0xe9,%%ymm3,%%ymm3           \n"  // b1
      "vpermq      $0x94,%%ymm1,%%ymm1           \n"  // b2

      "vpshufb     %%ymm4,%%ymm0,%%ymm0          \n"
      "vpshufb     %%ymm4,%%ymm3,%%ymm3          \n"
      "vpshufb     %%ymm4,%%ymm1,%%ymm1          \n"
      "vpshufb     %%ymm4,%%ymm2,%%ymm2          \n"

      "vpermd      %%ymm0,%%ymm5,%%ymm0          \n"
      "vpermd      %%ymm3,%%ymm6,%%ymm3          \n"
      "vpermd      %%ymm1,%%ymm7,%%ymm1          \n"
      "vpermd      %%ymm2,%%ymm5,%%ymm2          \n"

      // Dst0
      "vpblendd    $0xc0,%%ymm3,%%ymm0,%%ymm0    \n"
      "vmovdqu     %%ymm0,(%1)                   \n"

      // Dst1
      "vpblendd    $0xf0,%%ymm1,%%ymm3,%%ymm3    \n"
      "vpermq      $0x90,%%ymm2,%%ymm2           \n"
      "vmovdqu     %%ymm3,32(%1)                 \n"

      // Dst2
      "vpblendd    $0xfc,%%ymm2,%%ymm1,%%ymm1    \n"
      "vmovdqu     %%ymm1,64(%1)                 \n"

      "lea         96(%1),%1                     \n"
      "sub         $32,%2                        \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_raw),                        // %0
        "+r"(dst_rgb24),                      // %1
        "+r"(width)                           // %2
      : "m"(kShuffleMaskRAWToRGB24_AVX2[0]),  // %3
        "m"(kPermd0_AVX2),                    // %4
        "m"(kPermd1_AVX2),                    // %5
        "m"(kPermd2_AVX2)                     // %6
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_RAWTORGB24ROW_AVX2

#ifdef HAS_RAWTORGB24ROW_AVX512BW
void RAWToRGB24Row_AVX512BW(const uint8_t* src_raw,
                            uint8_t* dst_rgb24,
                            int width) {
  asm volatile(
      "vmovdqu64   %3,%%zmm20                    \n"  // kPermqRGB24ToARGB0_AVX512BW
      "vmovdqu64   %4,%%zmm21                    \n"  // kPermqRGB24ToARGB1_AVX512BW
      "vmovdqu64   %5,%%zmm22                    \n"  // kPermqRGB24ToARGB2_AVX512BW
      "vmovdqu64   %6,%%zmm23                    \n"  // kPermqRGB24ToARGB3_AVX512BW
      "vbroadcasti64x4 %7,%%zmm4                 \n"  // shuffler (32 bytes)
      "vmovdqu32   %8,%%zmm24                    \n"  // kPermdRGB24_0_AVX512BW
      "vmovdqu32   %9,%%zmm25                    \n"  // kPermdRGB24_1_AVX512BW
      "vmovdqu32   %10,%%zmm26                   \n"  // kPermdRGB24_2_AVX512BW

      LABELALIGN
      "1:          \n"
      "vmovdqu8    (%0),%%zmm0                   \n"
      "vmovdqu8    64(%0),%%zmm1                 \n"
      "vmovdqu8    128(%0),%%zmm2                \n"
      "lea         192(%0),%0                    \n"
      "vpermq      %%zmm2,%%zmm23,%%zmm3         \n"
      "vmovdqa64   %%zmm0,%%zmm19                \n"
      "vpermt2q    %%zmm1,%%zmm21,%%zmm19        \n"
      "vpermt2q    %%zmm2,%%zmm22,%%zmm1         \n"
      "vpermq      %%zmm0,%%zmm20,%%zmm0         \n"
      "vpshufb     %%zmm4,%%zmm0,%%zmm0          \n"
      "vpshufb     %%zmm4,%%zmm19,%%zmm19        \n"
      "vpshufb     %%zmm4,%%zmm1,%%zmm2          \n"
      "vpshufb     %%zmm4,%%zmm3,%%zmm3          \n"
      "vpermt2d    %%zmm19,%%zmm24,%%zmm0        \n"
      "vpermt2d    %%zmm2,%%zmm25,%%zmm19        \n"
      "vpermt2d    %%zmm3,%%zmm26,%%zmm2         \n"
      "vmovdqu8    %%zmm0,(%1)                   \n"
      "vmovdqu8    %%zmm19,0x40(%1)              \n"
      "vmovdqu8    %%zmm2,0x80(%1)               \n"
      "lea         0xc0(%1),%1                   \n"
      "sub         $0x40,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_raw),                        // %0
        "+r"(dst_rgb24),                      // %1
        "+r"(width)                           // %2
      : "m"(kPermqRGB24ToARGB0_AVX512BW),     // %3
        "m"(kPermqRGB24ToARGB1_AVX512BW),     // %4
        "m"(kPermqRGB24ToARGB2_AVX512BW),     // %5
        "m"(kPermqRGB24ToARGB3_AVX512BW),     // %6
        "m"(kShuffleMaskRAWToRGB24_AVX2[0]),  // %7
        "m"(kPermdRGB24_0_AVX512BW),          // %8
        "m"(kPermdRGB24_1_AVX512BW),          // %9
        "m"(kPermdRGB24_2_AVX512BW)           // %10
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm19",
        "xmm20", "xmm21", "xmm22", "xmm23", "xmm24", "xmm25", "xmm26");
}
#endif  // HAS_RAWTORGB24ROW_AVX512BW

#ifdef HAS_RAWTORGB24ROW_AVX512VBMI
void RAWToRGB24Row_AVX512VBMI(const uint8_t* src_raw,
                              uint8_t* dst_rgb24,
                              int width) {
  asm volatile(
      "vmovdqu32   %[kPermRAWToRGB24_0],%%zmm20  \n"
      "vmovdqu32   %[kPermRAWToRGB24_1],%%zmm21  \n"
      "vmovdqu32   %[kPermRAWToRGB24_2],%%zmm22  \n"
      "vmovdqu32   %[kPermRAWToRGB24_3],%%zmm23  \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu8    (%0),%%zmm0                   \n"
      "vmovdqu8    64(%0),%%zmm1                 \n"
      "vmovdqu8    128(%0),%%zmm2                \n"
      "lea         192(%0),%0                    \n"
      "vmovdqa64   %%zmm1,%%zmm3                 \n"
      "vpermt2b    %%zmm0,%%zmm21,%%zmm1         \n"
      "vpermt2b    %%zmm3,%%zmm20,%%zmm0         \n"
      "vpermt2b    %%zmm2,%%zmm23,%%zmm3         \n"
      "vpermt2b    %%zmm2,%%zmm22,%%zmm1         \n"
      "vmovdqu8    %%zmm0,(%1)                   \n"
      "vmovdqu8    %%zmm1,0x40(%1)               \n"
      "vmovdqu8    %%zmm3,0x80(%1)               \n"
      "lea         0xc0(%1),%1                   \n"
      "sub         $0x40,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_raw),    // %0
        "+r"(dst_rgb24),  // %1
        "+r"(width)       // %2
      : [kPermRAWToRGB24_0] "m"(kPermRAWToRGB24_0),
        [kPermRAWToRGB24_1] "m"(kPermRAWToRGB24_1),
        [kPermRAWToRGB24_2] "m"(kPermRAWToRGB24_2),
        [kPermRAWToRGB24_3] "m"(kPermRAWToRGB24_3)
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm20", "xmm21",
        "xmm22", "xmm23");
}
#endif  // HAS_RAWTORGB24ROW_AVX512VBMI

#ifdef HAS_RGB565TOARGBROW_AVX2
void RGB565ToARGBRow_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile(
      "mov         $0x1080108,%%eax              \n"
      "vmovd       %%eax,%%xmm5                  \n"
      "vpbroadcastd %%xmm5,%%ymm5                \n"
      "mov         $0x20802080,%%eax             \n"
      "vmovd       %%eax,%%xmm6                  \n"
      "vpbroadcastd %%xmm6,%%ymm6                \n"
      "vpcmpeqb    %%ymm3,%%ymm3,%%ymm3          \n"
      "vpsllw      $0xb,%%ymm3,%%ymm3            \n"
      "vpcmpeqb    %%ymm4,%%ymm4,%%ymm4          \n"
      "vpsllw      $10,%%ymm4,%%ymm4             \n"
      "vpsrlw      $5,%%ymm4,%%ymm4              \n"
      "vpcmpeqb    %%ymm7,%%ymm7,%%ymm7          \n"
      "vpsllw      $0x8,%%ymm7,%%ymm7            \n"
      "sub         %0,%1                         \n"
      "sub         %0,%1                         \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vpand       %%ymm3,%%ymm0,%%ymm1          \n"
      "vpsllw      $0xb,%%ymm0,%%ymm2            \n"
      "vpmulhuw    %%ymm5,%%ymm1,%%ymm1          \n"
      "vpmulhuw    %%ymm5,%%ymm2,%%ymm2          \n"
      "vpsllw      $0x8,%%ymm1,%%ymm1            \n"
      "vpor        %%ymm2,%%ymm1,%%ymm1          \n"
      "vpand       %%ymm4,%%ymm0,%%ymm0          \n"
      "vpmulhuw    %%ymm6,%%ymm0,%%ymm0          \n"
      "vpor        %%ymm7,%%ymm0,%%ymm0          \n"
      "vpunpcklbw  %%ymm0,%%ymm1,%%ymm2          \n"
      "vpunpckhbw  %%ymm0,%%ymm1,%%ymm1          \n"
      "vperm2i128  $0x20,%%ymm1,%%ymm2,%%ymm0    \n"
      "vperm2i128  $0x31,%%ymm1,%%ymm2,%%ymm1    \n"
      "vmovdqu     %%ymm0,(%1,%0,2)              \n"
      "vmovdqu     %%ymm1,0x20(%1,%0,2)          \n"
      "lea         0x20(%0),%0                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src),   // %0
        "+r"(dst),   // %1
        "+r"(width)  // %2
      :
      : "memory", "cc", "eax", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5",
        "xmm6", "xmm7");
}
#endif  // HAS_RGB565TOARGBROW_AVX2

#ifdef HAS_ARGB1555TOARGBROW_AVX2
void ARGB1555ToARGBRow_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile(
      "mov         $0x1080108,%%eax              \n"
      "vmovd       %%eax,%%xmm5                  \n"
      "vpbroadcastd %%xmm5,%%ymm5                \n"
      "mov         $0x42004200,%%eax             \n"
      "vmovd       %%eax,%%xmm6                  \n"
      "vpbroadcastd %%xmm6,%%ymm6                \n"
      "vpcmpeqb    %%ymm3,%%ymm3,%%ymm3          \n"
      "vpsllw      $0xb,%%ymm3,%%ymm3            \n"
      "vpsrlw      $0x6,%%ymm3,%%ymm4            \n"
      "vpcmpeqb    %%ymm7,%%ymm7,%%ymm7          \n"
      "vpsllw      $0x8,%%ymm7,%%ymm7            \n"
      "sub         %0,%1                         \n"
      "sub         %0,%1                         \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vpsllw      $0x1,%%ymm0,%%ymm1            \n"
      "vpsllw      $0xb,%%ymm0,%%ymm2            \n"
      "vpand       %%ymm3,%%ymm1,%%ymm1          \n"
      "vpmulhuw    %%ymm5,%%ymm2,%%ymm2          \n"
      "vpmulhuw    %%ymm5,%%ymm1,%%ymm1          \n"
      "vpsllw      $0x8,%%ymm1,%%ymm1            \n"
      "vpor        %%ymm2,%%ymm1,%%ymm1          \n"
      "vpsraw      $0x8,%%ymm0,%%ymm2            \n"
      "vpand       %%ymm4,%%ymm0,%%ymm0          \n"
      "vpmulhuw    %%ymm6,%%ymm0,%%ymm0          \n"
      "vpand       %%ymm7,%%ymm2,%%ymm2          \n"
      "vpor        %%ymm2,%%ymm0,%%ymm0          \n"
      "vpunpcklbw  %%ymm0,%%ymm1,%%ymm2          \n"
      "vpunpckhbw  %%ymm0,%%ymm1,%%ymm1          \n"
      "vperm2i128  $0x20,%%ymm1,%%ymm2,%%ymm0    \n"
      "vperm2i128  $0x31,%%ymm1,%%ymm2,%%ymm1    \n"
      "vmovdqu     %%ymm0,(%1,%0,2)              \n"
      "vmovdqu     %%ymm1,0x20(%1,%0,2)          \n"
      "lea         0x20(%0),%0                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src),   // %0
        "+r"(dst),   // %1
        "+r"(width)  // %2
      :
      : "memory", "cc", "eax", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5",
        "xmm6", "xmm7");
}
#endif  // HAS_ARGB1555TOARGBROW_AVX2

#ifdef HAS_ARGB4444TOARGBROW_AVX2
void ARGB4444ToARGBRow_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile(
      "mov         $0x0f0f0f0f,%%eax             \n"
      "vmovd       %%eax,%%xmm4                  \n"
      "vpbroadcastd %%xmm4,%%ymm4                \n"
      "vpslld      $0x4,%%ymm4,%%ymm5            \n"
      "sub         %0,%1                         \n"
      "sub         %0,%1                         \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vpand       %%ymm5,%%ymm0,%%ymm2          \n"
      "vpand       %%ymm4,%%ymm0,%%ymm0          \n"
      "vpsllw      $0x4,%%ymm0,%%ymm1            \n"
      "vpsrlw      $0x4,%%ymm2,%%ymm3            \n"
      "vpor        %%ymm1,%%ymm0,%%ymm0          \n"
      "vpor        %%ymm3,%%ymm2,%%ymm2          \n"
      "vpunpckhbw  %%ymm2,%%ymm0,%%ymm1          \n"
      "vpunpcklbw  %%ymm2,%%ymm0,%%ymm0          \n"
      "vperm2i128  $0x20,%%ymm1,%%ymm0,%%ymm2    \n"
      "vperm2i128  $0x31,%%ymm1,%%ymm0,%%ymm1    \n"
      "vmovdqu     %%ymm2,(%1,%0,2)              \n"
      "vmovdqu     %%ymm1,0x20(%1,%0,2)          \n"
      "lea         0x20(%0),%0                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src),   // %0
        "+r"(dst),   // %1
        "+r"(width)  // %2
      :
      : "memory", "cc", "eax", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_ARGB4444TOARGBROW_AVX2

#ifdef HAS_ARGBTORGB24ROW_SSSE3
void ARGBToRGB24Row_SSSE3(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile("movdqa      %3,%%xmm6                     \n"

               LABELALIGN
               "1:          \n"
               "movdqu      (%0),%%xmm0                   \n"
               "movdqu      0x10(%0),%%xmm1               \n"
               "movdqu      0x20(%0),%%xmm2               \n"
               "movdqu      0x30(%0),%%xmm3               \n"
               "lea         0x40(%0),%0                   \n"
               "pshufb      %%xmm6,%%xmm0                 \n"
               "pshufb      %%xmm6,%%xmm1                 \n"
               "pshufb      %%xmm6,%%xmm2                 \n"
               "pshufb      %%xmm6,%%xmm3                 \n"
               "movdqa      %%xmm1,%%xmm4                 \n"
               "psrldq      $0x4,%%xmm1                   \n"
               "pslldq      $0xc,%%xmm4                   \n"
               "movdqa      %%xmm2,%%xmm5                 \n"
               "por         %%xmm4,%%xmm0                 \n"
               "pslldq      $0x8,%%xmm5                   \n"
               "movdqu      %%xmm0,(%1)                   \n"
               "por         %%xmm5,%%xmm1                 \n"
               "psrldq      $0x8,%%xmm2                   \n"
               "pslldq      $0x4,%%xmm3                   \n"
               "por         %%xmm3,%%xmm2                 \n"
               "movdqu      %%xmm1,0x10(%1)               \n"
               "movdqu      %%xmm2,0x20(%1)               \n"
               "lea         0x30(%1),%1                   \n"
               "sub         $0x10,%2                      \n"
               "jg          1b                            \n"
               : "+r"(src),                       // %0
                 "+r"(dst),                       // %1
                 "+r"(width)                      // %2
               : "m"(kShuffleMaskARGBToRGB24[0])  // %3
               : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5",
                 "xmm6");
}
#endif  // HAS_ARGBTORGB24ROW_SSSE3

#ifdef HAS_ARGBTORAWROW_SSSE3
void ARGBToRAWRow_SSSE3(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile("movdqa      %3,%%xmm6                     \n"

               LABELALIGN
               "1:          \n"
               "movdqu      (%0),%%xmm0                   \n"
               "movdqu      0x10(%0),%%xmm1               \n"
               "movdqu      0x20(%0),%%xmm2               \n"
               "movdqu      0x30(%0),%%xmm3               \n"
               "lea         0x40(%0),%0                   \n"
               "pshufb      %%xmm6,%%xmm0                 \n"
               "pshufb      %%xmm6,%%xmm1                 \n"
               "pshufb      %%xmm6,%%xmm2                 \n"
               "pshufb      %%xmm6,%%xmm3                 \n"
               "movdqa      %%xmm1,%%xmm4                 \n"
               "psrldq      $0x4,%%xmm1                   \n"
               "pslldq      $0xc,%%xmm4                   \n"
               "movdqa      %%xmm2,%%xmm5                 \n"
               "por         %%xmm4,%%xmm0                 \n"
               "pslldq      $0x8,%%xmm5                   \n"
               "movdqu      %%xmm0,(%1)                   \n"
               "por         %%xmm5,%%xmm1                 \n"
               "psrldq      $0x8,%%xmm2                   \n"
               "pslldq      $0x4,%%xmm3                   \n"
               "por         %%xmm3,%%xmm2                 \n"
               "movdqu      %%xmm1,0x10(%1)               \n"
               "movdqu      %%xmm2,0x20(%1)               \n"
               "lea         0x30(%1),%1                   \n"
               "sub         $0x10,%2                      \n"
               "jg          1b                            \n"
               : "+r"(src),                  // %0
                 "+r"(dst),                  // %1
                 "+r"(width)                 // %2
               : "m"(kShuffleMaskARGBToRAW)  // %3
               : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5",
                 "xmm6");
}
#endif  // HAS_ARGBTORAWROW_SSSE3

#ifdef HAS_ARGBTORGB24ROW_AVX2
// vpermd for 12+12 to 24
static const lvec32 kPermdRGB24_AVX = {0, 1, 2, 4, 5, 6, 3, 7};

void ARGBToRGB24Row_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile(
      "vbroadcasti128 %3,%%ymm6                  \n"
      "vmovdqa     %4,%%ymm7                     \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "vmovdqu     0x40(%0),%%ymm2               \n"
      "vmovdqu     0x60(%0),%%ymm3               \n"
      "lea         0x80(%0),%0                   \n"
      "vpshufb     %%ymm6,%%ymm0,%%ymm0          \n"  // xxx0yyy0
      "vpshufb     %%ymm6,%%ymm1,%%ymm1          \n"
      "vpshufb     %%ymm6,%%ymm2,%%ymm2          \n"
      "vpshufb     %%ymm6,%%ymm3,%%ymm3          \n"
      "vpermd      %%ymm0,%%ymm7,%%ymm0          \n"  // pack to 24 bytes
      "vpermd      %%ymm1,%%ymm7,%%ymm1          \n"
      "vpermd      %%ymm2,%%ymm7,%%ymm2          \n"
      "vpermd      %%ymm3,%%ymm7,%%ymm3          \n"
      "vpermq      $0x3f,%%ymm1,%%ymm4           \n"  // combine 24 + 8
      "vpor        %%ymm4,%%ymm0,%%ymm0          \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "vpermq      $0xf9,%%ymm1,%%ymm1           \n"  // combine 16 + 16
      "vpermq      $0x4f,%%ymm2,%%ymm4           \n"
      "vpor        %%ymm4,%%ymm1,%%ymm1          \n"
      "vmovdqu     %%ymm1,0x20(%1)               \n"
      "vpermq      $0xfe,%%ymm2,%%ymm2           \n"  // combine 8 + 24
      "vpermq      $0x93,%%ymm3,%%ymm3           \n"
      "vpor        %%ymm3,%%ymm2,%%ymm2          \n"
      "vmovdqu     %%ymm2,0x40(%1)               \n"
      "lea         0x60(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src),                        // %0
        "+r"(dst),                        // %1
        "+r"(width)                       // %2
      : "m"(kShuffleMaskARGBToRGB24[0]),  // %3
        "m"(kPermdRGB24_AVX)              // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_ARGBTORGB24ROW_AVX2

#if defined(HAS_ARGBTORGB24ROW_AVX512BW) || defined(HAS_ARGBTORAWROW_AVX512BW)
static inline void ARGBToRGBRow_AVX512BW(const uint8_t* src,
                                         uint8_t* dst,
                                         int width,
                                         const uvec8& shuffler) {
  uintptr_t temp;
  asm volatile(
      "vbroadcasti32x4 %[shuffler],%%zmm4        \n"
      "vmovdqu32   %[kPermdRGB24_0],%%zmm5       \n"
      "vmovdqu32   %[kPermdRGB24_1],%%zmm16      \n"
      "vmovdqu32   %[kPermdRGB24_2],%%zmm17      \n"
      "sub         $0x40,%[width]                \n"
      "jl          2f                            \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu8    (%[src]),%%zmm0               \n"
      "vmovdqu8    0x40(%[src]),%%zmm1           \n"
      "vmovdqu8    0x80(%[src]),%%zmm2           \n"
      "vmovdqu8    0xc0(%[src]),%%zmm3           \n"
      "lea         0x100(%[src]),%[src]          \n"
      "vpshufb     %%zmm4,%%zmm0,%%zmm0          \n"
      "vpshufb     %%zmm4,%%zmm1,%%zmm1          \n"
      "vpshufb     %%zmm4,%%zmm2,%%zmm2          \n"
      "vpshufb     %%zmm4,%%zmm3,%%zmm3          \n"
      "vpermt2d    %%zmm1,%%zmm5,%%zmm0          \n"
      "vpermt2d    %%zmm2,%%zmm16,%%zmm1         \n"
      "vpermt2d    %%zmm3,%%zmm17,%%zmm2         \n"
      "vmovdqu8    %%zmm0,(%[dst])               \n"
      "vmovdqu8    %%zmm1,0x40(%[dst])           \n"
      "vmovdqu8    %%zmm2,0x80(%[dst])           \n"
      "lea         0xc0(%[dst]),%[dst]           \n"
      "sub         $0x40,%[width]                \n"
      "jge         1b                            \n"

      "2:          \n"
      "add         $0x40,%[width]                \n"
      "je          99f                           \n"

      "mov         $-1,%[temp]                   \n"
      "bzhi        %q[width],%[temp],%[temp]     \n"
      "kmovw       %k[temp],%%k1                 \n"
      "shr         $0x10,%[temp]                 \n"
      "kmovw       %k[temp],%%k2                 \n"
      "shr         $0x10,%[temp]                 \n"
      "kmovw       %k[temp],%%k3                 \n"
      "shr         $0x10,%[temp]                 \n"
      "kmovw       %k[temp],%%k4                 \n"
      "vmovdqu32   (%[src]),%%zmm0%{%%k1%}%{z%}     \n"
      "vmovdqu32   0x40(%[src]),%%zmm1%{%%k2%}%{z%} \n"
      "vmovdqu32   0x80(%[src]),%%zmm2%{%%k3%}%{z%} \n"
      "vmovdqu32   0xc0(%[src]),%%zmm3%{%%k4%}%{z%} \n"

      "mov         $-1,%[temp]                   \n"
      "lea         (%q[width],%q[width],2),%k[width] \n"
      "bzhi        %q[width],%[temp],%[src]      \n"
      "kmovq       %[src],%%k1                   \n"
      "xor         %k[src],%k[src]               \n"
      "sub         $0x40,%k[width]               \n"
      "cmovg       %[temp],%[src]                \n"
      "bzhi        %q[width],%[src],%[src]       \n"
      "kmovq       %[src],%%k2                   \n"
      "xor         %k[src],%k[src]               \n"
      "sub         $0x40,%k[width]               \n"
      "cmovg       %[temp],%[src]                \n"
      "bzhi        %q[width],%[src],%[src]       \n"
      "kmovq       %[src],%%k3                   \n"

      "vpshufb     %%zmm4,%%zmm0,%%zmm0          \n"
      "vpshufb     %%zmm4,%%zmm1,%%zmm1          \n"
      "vpshufb     %%zmm4,%%zmm2,%%zmm2          \n"
      "vpshufb     %%zmm4,%%zmm3,%%zmm3          \n"
      "vpermt2d    %%zmm1,%%zmm5,%%zmm0          \n"
      "vpermt2d    %%zmm2,%%zmm16,%%zmm1         \n"
      "vpermt2d    %%zmm3,%%zmm17,%%zmm2         \n"
      "vmovdqu8    %%zmm0,(%[dst])%{%%k1%}       \n"
      "vmovdqu8    %%zmm1,0x40(%[dst])%{%%k2%}   \n"
      "vmovdqu8    %%zmm2,0x80(%[dst])%{%%k3%}   \n"

      "99:         \n"
      "vzeroupper  \n"
      : [src] "+r"(src),      // %[src]
        [dst] "+r"(dst),      // %[dst]
        [width] "+r"(width),  // %[width]
        [temp] "=&r"(temp)    // %[temp]
      : [shuffler] "m"(shuffler),
        [kPermdRGB24_0] "m"(kPermdRGB24_0_AVX512BW),
        [kPermdRGB24_1] "m"(kPermdRGB24_1_AVX512BW),
        [kPermdRGB24_2] "m"(kPermdRGB24_2_AVX512BW)
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm16",
        "xmm17", "k1", "k2", "k3", "k4");
}
#endif  // defined(HAS_ARGBTORGB24ROW_AVX512BW) ||
        // defined(HAS_ARGBTORAWROW_AVX512BW)

#ifdef HAS_ARGBTORGB24ROW_AVX512BW
void ARGBToRGB24Row_AVX512BW(const uint8_t* src, uint8_t* dst, int width) {
  ARGBToRGBRow_AVX512BW(src, dst, width, kShuffleMaskARGBToRGB24[0]);
}
#endif  // HAS_ARGBTORGB24ROW_AVX512BW

#ifdef HAS_ARGBTORAWROW_AVX2
void ARGBToRAWRow_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile(
      "vbroadcasti128 %3,%%ymm6                  \n"
      "vmovdqa     %4,%%ymm7                     \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "vmovdqu     0x40(%0),%%ymm2               \n"
      "vmovdqu     0x60(%0),%%ymm3               \n"
      "lea         0x80(%0),%0                   \n"
      "vpshufb     %%ymm6,%%ymm0,%%ymm0          \n"  // xxx0yyy0
      "vpshufb     %%ymm6,%%ymm1,%%ymm1          \n"
      "vpshufb     %%ymm6,%%ymm2,%%ymm2          \n"
      "vpshufb     %%ymm6,%%ymm3,%%ymm3          \n"
      "vpermd      %%ymm0,%%ymm7,%%ymm0          \n"  // pack to 24 bytes
      "vpermd      %%ymm1,%%ymm7,%%ymm1          \n"
      "vpermd      %%ymm2,%%ymm7,%%ymm2          \n"
      "vpermd      %%ymm3,%%ymm7,%%ymm3          \n"
      "vpermq      $0x3f,%%ymm1,%%ymm4           \n"  // combine 24 + 8
      "vpor        %%ymm4,%%ymm0,%%ymm0          \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "vpermq      $0xf9,%%ymm1,%%ymm1           \n"  // combine 16 + 16
      "vpermq      $0x4f,%%ymm2,%%ymm4           \n"
      "vpor        %%ymm4,%%ymm1,%%ymm1          \n"
      "vmovdqu     %%ymm1,0x20(%1)               \n"
      "vpermq      $0xfe,%%ymm2,%%ymm2           \n"  // combine 8 + 24
      "vpermq      $0x93,%%ymm3,%%ymm3           \n"
      "vpor        %%ymm3,%%ymm2,%%ymm2          \n"
      "vmovdqu     %%ymm2,0x40(%1)               \n"
      "lea         0x60(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src),                   // %0
        "+r"(dst),                   // %1
        "+r"(width)                  // %2
      : "m"(kShuffleMaskARGBToRAW),  // %3
        "m"(kPermdRGB24_AVX)         // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_ARGBTORAWROW_AVX2

#ifdef HAS_ARGBTORAWROW_AVX512BW
void ARGBToRAWRow_AVX512BW(const uint8_t* src, uint8_t* dst, int width) {
  ARGBToRGBRow_AVX512BW(src, dst, width, kShuffleMaskARGBToRAW);
}
#endif  // HAS_ARGBTORAWROW_AVX512BW

#ifdef HAS_ARGBTORGB565DITHERROW_AVX2
void ARGBToRGB565DitherRow_AVX2(const uint8_t* src,
                                uint8_t* dst,
                                uint32_t dither4,
                                int width) {
  asm volatile(
      "vbroadcastss %3,%%xmm6                    \n"
      "vpunpcklbw  %%xmm6,%%xmm6,%%xmm6          \n"
      "vpermq      $0xd8,%%ymm6,%%ymm6           \n"
      "vpunpcklwd  %%ymm6,%%ymm6,%%ymm6          \n"
      "vpcmpeqb    %%ymm3,%%ymm3,%%ymm3          \n"
      "vpsrld      $0x1b,%%ymm3,%%ymm3           \n"
      "vpcmpeqb    %%ymm4,%%ymm4,%%ymm4          \n"
      "vpsrld      $0x1a,%%ymm4,%%ymm4           \n"
      "vpslld      $0x5,%%ymm4,%%ymm4            \n"
      "vpslld      $0xb,%%ymm3,%%ymm5            \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vpaddusb    %%ymm6,%%ymm0,%%ymm0          \n"
      "vpsrld      $0x5,%%ymm0,%%ymm2            \n"
      "vpsrld      $0x3,%%ymm0,%%ymm1            \n"
      "vpsrld      $0x8,%%ymm0,%%ymm0            \n"
      "vpand       %%ymm4,%%ymm2,%%ymm2          \n"
      "vpand       %%ymm3,%%ymm1,%%ymm1          \n"
      "vpand       %%ymm5,%%ymm0,%%ymm0          \n"
      "vpor        %%ymm2,%%ymm1,%%ymm1          \n"
      "vpor        %%ymm1,%%ymm0,%%ymm0          \n"
      "vpackusdw   %%ymm0,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "lea         0x20(%0),%0                   \n"
      "vmovdqu     %%xmm0,(%1)                   \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src),    // %0
        "+r"(dst),    // %1
        "+r"(width)   // %2
      : "m"(dither4)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_ARGBTORGB565DITHERROW_AVX2

#ifdef HAS_ARGBTORGB565ROW_AVX2
void ARGBToRGB565Row_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile(
      "vpcmpeqb    %%ymm3,%%ymm3,%%ymm3          \n"
      "vpsrld      $0x1b,%%ymm3,%%ymm3           \n"
      "vpcmpeqb    %%ymm4,%%ymm4,%%ymm4          \n"
      "vpsrld      $0x1a,%%ymm4,%%ymm4           \n"
      "vpslld      $0x5,%%ymm4,%%ymm4            \n"
      "vpslld      $0xb,%%ymm3,%%ymm5            \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vpsrld      $0x5,%%ymm0,%%ymm2            \n"
      "vpsrld      $0x3,%%ymm0,%%ymm1            \n"
      "vpsrld      $0x8,%%ymm0,%%ymm0            \n"
      "vpand       %%ymm4,%%ymm2,%%ymm2          \n"
      "vpand       %%ymm3,%%ymm1,%%ymm1          \n"
      "vpand       %%ymm5,%%ymm0,%%ymm0          \n"
      "vpor        %%ymm2,%%ymm1,%%ymm1          \n"
      "vpor        %%ymm1,%%ymm0,%%ymm0          \n"
      "vpackusdw   %%ymm0,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "lea         0x20(%0),%0                   \n"
      "vmovdqu     %%xmm0,(%1)                   \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src),   // %0
        "+r"(dst),   // %1
        "+r"(width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_ARGBTORGB565ROW_AVX2

#ifdef HAS_ARGBTOARGB1555ROW_AVX2
void ARGBToARGB1555Row_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile(
      "vpcmpeqb    %%ymm3,%%ymm3,%%ymm3          \n"
      "vpslld      $0x1f,%%ymm3,%%ymm6           \n"
      "vpsrld      $0x10,%%ymm6,%%ymm6           \n"
      "vpsrld      $0x1b,%%ymm3,%%ymm3           \n"
      "vpslld      $0x5,%%ymm3,%%ymm4            \n"
      "vpslld      $0xa,%%ymm3,%%ymm5            \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vpsrld      $0x6,%%ymm0,%%ymm2            \n"
      "vpsrld      $0x3,%%ymm0,%%ymm1            \n"
      "vpsrld      $0x10,%%ymm0,%%ymm7           \n"
      "vpsrld      $0x9,%%ymm0,%%ymm0            \n"
      "vpand       %%ymm4,%%ymm2,%%ymm2          \n"
      "vpand       %%ymm3,%%ymm1,%%ymm1          \n"
      "vpand       %%ymm6,%%ymm7,%%ymm7          \n"
      "vpand       %%ymm5,%%ymm0,%%ymm0          \n"
      "vpor        %%ymm2,%%ymm1,%%ymm1          \n"
      "vpor        %%ymm7,%%ymm0,%%ymm0          \n"
      "vpor        %%ymm1,%%ymm0,%%ymm0          \n"
      "vpackusdw   %%ymm0,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "lea         0x20(%0),%0                   \n"
      "vmovdqu     %%xmm0,(%1)                   \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src),   // %0
        "+r"(dst),   // %1
        "+r"(width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_ARGBTOARGB1555ROW_AVX2

#ifdef HAS_ARGBTOARGB4444ROW_AVX2
void ARGBToARGB4444Row_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile(
      "vpcmpeqb    %%ymm3,%%ymm3,%%ymm3          \n"
      "vpsrld      $0x1c,%%ymm3,%%ymm3           \n"
      "vpslld      $0x4,%%ymm3,%%ymm4            \n"
      "vpslld      $0x8,%%ymm3,%%ymm5            \n"
      "vpslld      $0xc,%%ymm3,%%ymm6            \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vpsrld      $0x8,%%ymm0,%%ymm2            \n"
      "vpsrld      $0x4,%%ymm0,%%ymm1            \n"
      "vpsrld      $0x10,%%ymm0,%%ymm7           \n"
      "vpsrld      $0xc,%%ymm0,%%ymm0            \n"
      "vpand       %%ymm4,%%ymm2,%%ymm2          \n"
      "vpand       %%ymm3,%%ymm1,%%ymm1          \n"
      "vpand       %%ymm6,%%ymm7,%%ymm7          \n"
      "vpand       %%ymm5,%%ymm0,%%ymm0          \n"
      "vpor        %%ymm2,%%ymm1,%%ymm1          \n"
      "vpor        %%ymm7,%%ymm0,%%ymm0          \n"
      "vpor        %%ymm1,%%ymm0,%%ymm0          \n"
      "vpackusdw   %%ymm0,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "lea         0x20(%0),%0                   \n"
      "vmovdqu     %%xmm0,(%1)                   \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src),   // %0
        "+r"(dst),   // %1
        "+r"(width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_ARGBTOARGB4444ROW_AVX2

/*

ARGBToAR30Row:

Red Blue
With the 8 bit value in the upper bits of a short, vpmulhuw by (1024+4) will
produce a 10 bit value in the low 10 bits of each 16 bit value. This is whats
wanted for the blue channel. The red needs to be shifted 4 left, so multiply by
(1024+4)*16 for red.

Alpha Green
Alpha and Green are already in the high bits so vpand can zero out the other
bits, keeping just 2 upper bits of alpha and 8 bit green. The same multiplier
could be used for Green - (1024+4) putting the 10 bit green in the lsb.  Alpha
would be a simple multiplier to shift it into position.  It wants a gap of 10
above the green.  Green is 10 bits, so there are 6 bits in the low short.  4
more are needed, so a multiplier of 4 gets the 2 bits into the upper 16 bits,
and then a shift of 4 is a multiply of 16, so (4*16) = 64.  Then shift the
result left 10 to position the A and G channels.
*/

// Shuffle table for converting RAW to RGB24.  Last 8.
static const uvec8 kShuffleRB30 = {128u, 0u, 128u, 2u,  128u, 4u,  128u, 6u,
                                   128u, 8u, 128u, 10u, 128u, 12u, 128u, 14u};

static const uvec8 kShuffleBR30 = {128u, 2u,  128u, 0u, 128u, 6u,  128u, 4u,
                                   128u, 10u, 128u, 8u, 128u, 14u, 128u, 12u};

static const uint32_t kMulRB10 = 1028 * 16 * 65536 + 1028;
static const uint32_t kMaskRB10 = 0x3ff003ff;
static const uint32_t kMaskAG10 = 0xc000ff00;
static const uint32_t kMulAG10 = 64 * 65536 + 1028;

void ARGBToAR30Row_SSSE3(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile(
      "movdqa      %3,%%xmm2                     \n"  // shuffler for RB
      "movd        %4,%%xmm3                     \n"  // multipler for RB
      "movd        %5,%%xmm4                     \n"  // mask for R10 B10
      "movd        %6,%%xmm5                     \n"  // mask for AG
      "movd        %7,%%xmm6                     \n"  // multipler for AG
      "pshufd      $0x0,%%xmm3,%%xmm3            \n"
      "pshufd      $0x0,%%xmm4,%%xmm4            \n"
      "pshufd      $0x0,%%xmm5,%%xmm5            \n"
      "pshufd      $0x0,%%xmm6,%%xmm6            \n"
      "sub         %0,%1                         \n"

      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"  // fetch 4 ARGB pixels
      "movdqa      %%xmm0,%%xmm1                 \n"
      "pshufb      %%xmm2,%%xmm1                 \n"  // R0B0
      "pand        %%xmm5,%%xmm0                 \n"  // A0G0
      "pmulhuw     %%xmm3,%%xmm1                 \n"  // X2 R16 X4  B10
      "pmulhuw     %%xmm6,%%xmm0                 \n"  // X10 A2 X10 G10
      "pand        %%xmm4,%%xmm1                 \n"  // X2 R10 X10 B10
      "pslld       $10,%%xmm0                    \n"  // A2 x10 G10 x10
      "por         %%xmm1,%%xmm0                 \n"  // A2 R10 G10 B10
      "movdqu      %%xmm0,(%1,%0)                \n"  // store 4 AR30 pixels
      "add         $0x10,%0                      \n"
      "sub         $0x4,%2                       \n"
      "jg          1b                            \n"

      : "+r"(src),          // %0
        "+r"(dst),          // %1
        "+r"(width)         // %2
      : "m"(kShuffleRB30),  // %3
        "m"(kMulRB10),      // %4
        "m"(kMaskRB10),     // %5
        "m"(kMaskAG10),     // %6
        "m"(kMulAG10)       // %7
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}

void ABGRToAR30Row_SSSE3(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile(
      "movdqa      %3,%%xmm2                     \n"  // shuffler for RB
      "movd        %4,%%xmm3                     \n"  // multipler for RB
      "movd        %5,%%xmm4                     \n"  // mask for R10 B10
      "movd        %6,%%xmm5                     \n"  // mask for AG
      "movd        %7,%%xmm6                     \n"  // multipler for AG
      "pshufd      $0x0,%%xmm3,%%xmm3            \n"
      "pshufd      $0x0,%%xmm4,%%xmm4            \n"
      "pshufd      $0x0,%%xmm5,%%xmm5            \n"
      "pshufd      $0x0,%%xmm6,%%xmm6            \n"
      "sub         %0,%1                         \n"

      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"  // fetch 4 ABGR pixels
      "movdqa      %%xmm0,%%xmm1                 \n"
      "pshufb      %%xmm2,%%xmm1                 \n"  // R0B0
      "pand        %%xmm5,%%xmm0                 \n"  // A0G0
      "pmulhuw     %%xmm3,%%xmm1                 \n"  // X2 R16 X4  B10
      "pmulhuw     %%xmm6,%%xmm0                 \n"  // X10 A2 X10 G10
      "pand        %%xmm4,%%xmm1                 \n"  // X2 R10 X10 B10
      "pslld       $10,%%xmm0                    \n"  // A2 x10 G10 x10
      "por         %%xmm1,%%xmm0                 \n"  // A2 R10 G10 B10
      "movdqu      %%xmm0,(%1,%0)                \n"  // store 4 AR30 pixels
      "add         $0x10,%0                      \n"
      "sub         $0x4,%2                       \n"
      "jg          1b                            \n"

      : "+r"(src),          // %0
        "+r"(dst),          // %1
        "+r"(width)         // %2
      : "m"(kShuffleBR30),  // %3  reversed shuffler
        "m"(kMulRB10),      // %4
        "m"(kMaskRB10),     // %5
        "m"(kMaskAG10),     // %6
        "m"(kMulAG10)       // %7
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}

#ifdef HAS_ARGBTOAR30ROW_AVX2
void ARGBToAR30Row_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile(
      "vbroadcasti128 %3,%%ymm2                  \n"  // shuffler for RB
      "vbroadcastss %4,%%ymm3                    \n"  // multipler for RB
      "vbroadcastss %5,%%ymm4                    \n"  // mask for R10 B10
      "vbroadcastss %6,%%ymm5                    \n"  // mask for AG
      "vbroadcastss %7,%%ymm6                    \n"  // multipler for AG
      "sub         %0,%1                         \n"

      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"  // fetch 8 ARGB pixels
      "vpshufb     %%ymm2,%%ymm0,%%ymm1          \n"  // R0B0
      "vpand       %%ymm5,%%ymm0,%%ymm0          \n"  // A0G0
      "vpmulhuw    %%ymm3,%%ymm1,%%ymm1          \n"  // X2 R16 X4  B10
      "vpmulhuw    %%ymm6,%%ymm0,%%ymm0          \n"  // X10 A2 X10 G10
      "vpand       %%ymm4,%%ymm1,%%ymm1          \n"  // X2 R10 X10 B10
      "vpslld      $10,%%ymm0,%%ymm0             \n"  // A2 x10 G10 x10
      "vpor        %%ymm1,%%ymm0,%%ymm0          \n"  // A2 R10 G10 B10
      "vmovdqu     %%ymm0,(%1,%0)                \n"  // store 8 AR30 pixels
      "add         $0x20,%0                      \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"

      : "+r"(src),          // %0
        "+r"(dst),          // %1
        "+r"(width)         // %2
      : "m"(kShuffleRB30),  // %3
        "m"(kMulRB10),      // %4
        "m"(kMaskRB10),     // %5
        "m"(kMaskAG10),     // %6
        "m"(kMulAG10)       // %7
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}
#endif  // HAS_ARGBTOAR30ROW_AVX2

#ifdef HAS_ABGRTOAR30ROW_AVX2
void ABGRToAR30Row_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile(
      "vbroadcasti128 %3,%%ymm2                  \n"  // shuffler for RB
      "vbroadcastss %4,%%ymm3                    \n"  // multipler for RB
      "vbroadcastss %5,%%ymm4                    \n"  // mask for R10 B10
      "vbroadcastss %6,%%ymm5                    \n"  // mask for AG
      "vbroadcastss %7,%%ymm6                    \n"  // multipler for AG
      "sub         %0,%1                         \n"

      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"  // fetch 8 ABGR pixels
      "vpshufb     %%ymm2,%%ymm0,%%ymm1          \n"  // R0B0
      "vpand       %%ymm5,%%ymm0,%%ymm0          \n"  // A0G0
      "vpmulhuw    %%ymm3,%%ymm1,%%ymm1          \n"  // X2 R16 X4  B10
      "vpmulhuw    %%ymm6,%%ymm0,%%ymm0          \n"  // X10 A2 X10 G10
      "vpand       %%ymm4,%%ymm1,%%ymm1          \n"  // X2 R10 X10 B10
      "vpslld      $10,%%ymm0,%%ymm0             \n"  // A2 x10 G10 x10
      "vpor        %%ymm1,%%ymm0,%%ymm0          \n"  // A2 R10 G10 B10
      "vmovdqu     %%ymm0,(%1,%0)                \n"  // store 8 AR30 pixels
      "add         $0x20,%0                      \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"

      : "+r"(src),          // %0
        "+r"(dst),          // %1
        "+r"(width)         // %2
      : "m"(kShuffleBR30),  // %3  reversed shuffler
        "m"(kMulRB10),      // %4
        "m"(kMaskRB10),     // %5
        "m"(kMaskAG10),     // %6
        "m"(kMulAG10)       // %7
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}
#endif  // HAS_ABGRTOAR30ROW_AVX2

static const uvec8 kShuffleARGBToABGR = {2,  1, 0, 3,  6,  5,  4,  7,
                                         10, 9, 8, 11, 14, 13, 12, 15};

static const uvec8 kShuffleARGBToAB64Lo = {2, 2, 1, 1, 0, 0, 3, 3,
                                           6, 6, 5, 5, 4, 4, 7, 7};
static const uvec8 kShuffleARGBToAB64Hi = {10, 10, 9,  9,  8,  8,  11, 11,
                                           14, 14, 13, 13, 12, 12, 15, 15};

void ARGBToAR64Row_SSSE3(const uint8_t* src_argb,
                         uint16_t* dst_ar64,
                         int width) {
  asm volatile(
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqa      %%xmm0,%%xmm1                 \n"
      "punpcklbw   %%xmm0,%%xmm0                 \n"
      "punpckhbw   %%xmm1,%%xmm1                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "movdqu      %%xmm1,0x10(%1)               \n"
      "lea         0x10(%0),%0                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x4,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src_argb),  // %0
        "+r"(dst_ar64),  // %1
        "+r"(width)      // %2
        ::"memory",
        "cc", "xmm0", "xmm1");
}

void ARGBToAB64Row_SSSE3(const uint8_t* src_argb,
                         uint16_t* dst_ab64,
                         int width) {
  asm volatile(
      "movdqa      %3,%%xmm2                     \n"
      "movdqa      %4,%%xmm3                     \n" LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqa      %%xmm0,%%xmm1                 \n"
      "pshufb      %%xmm2,%%xmm0                 \n"
      "pshufb      %%xmm3,%%xmm1                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "movdqu      %%xmm1,0x10(%1)               \n"
      "lea         0x10(%0),%0                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x4,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src_argb),             // %0
        "+r"(dst_ab64),             // %1
        "+r"(width)                 // %2
      : "m"(kShuffleARGBToAB64Lo),  // %3
        "m"(kShuffleARGBToAB64Hi)   // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3");
}

void AR64ToARGBRow_SSSE3(const uint16_t* src_ar64,
                         uint8_t* dst_argb,
                         int width) {
  asm volatile(
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "psrlw       $8,%%xmm0                     \n"
      "psrlw       $8,%%xmm1                     \n"
      "packuswb    %%xmm1,%%xmm0                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "lea         0x20(%0),%0                   \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x4,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src_ar64),  // %0
        "+r"(dst_argb),  // %1
        "+r"(width)      // %2
        ::"memory",
        "cc", "xmm0", "xmm1");
}

void AB64ToARGBRow_SSSE3(const uint16_t* src_ab64,
                         uint8_t* dst_argb,
                         int width) {
  asm volatile("movdqa      %3,%%xmm2                     \n"

               LABELALIGN
               "1:          \n"
               "movdqu      (%0),%%xmm0                   \n"
               "movdqu      0x10(%0),%%xmm1               \n"
               "psrlw       $8,%%xmm0                     \n"
               "psrlw       $8,%%xmm1                     \n"
               "packuswb    %%xmm1,%%xmm0                 \n"
               "pshufb      %%xmm2,%%xmm0                 \n"
               "movdqu      %%xmm0,(%1)                   \n"
               "lea         0x20(%0),%0                   \n"
               "lea         0x10(%1),%1                   \n"
               "sub         $0x4,%2                       \n"
               "jg          1b                            \n"
               : "+r"(src_ab64),          // %0
                 "+r"(dst_argb),          // %1
                 "+r"(width)              // %2
               : "m"(kShuffleARGBToABGR)  // %3
               : "memory", "cc", "xmm0", "xmm1", "xmm2");
}

#ifdef HAS_ARGBTOAR64ROW_AVX2
void ARGBToAR64Row_AVX2(const uint8_t* src_argb,
                        uint16_t* dst_ar64,
                        int width) {
  asm volatile(
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vpunpckhbw  %%ymm0,%%ymm0,%%ymm1          \n"
      "vpunpcklbw  %%ymm0,%%ymm0,%%ymm0          \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "vmovdqu     %%ymm1,0x20(%1)               \n"
      "lea         0x20(%0),%0                   \n"
      "lea         0x40(%1),%1                   \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_argb),  // %0
        "+r"(dst_ar64),  // %1
        "+r"(width)      // %2
        ::"memory",
        "cc", "xmm0", "xmm1");
}
#endif  // HAS_ARGBTOAR64ROW_AVX2

#ifdef HAS_ARGBTOAB64ROW_AVX2
void ARGBToAB64Row_AVX2(const uint8_t* src_argb,
                        uint16_t* dst_ab64,
                        int width) {
  asm volatile(
      "vbroadcasti128 %3,%%ymm2                  \n"
      "vbroadcasti128 %4,%%ymm3                  \n" LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vpshufb     %%ymm3,%%ymm0,%%ymm1          \n"
      "vpshufb     %%ymm2,%%ymm0,%%ymm0          \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "vmovdqu     %%ymm1,0x20(%1)               \n"
      "lea         0x20(%0),%0                   \n"
      "lea         0x40(%1),%1                   \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_argb),             // %0
        "+r"(dst_ab64),             // %1
        "+r"(width)                 // %2
      : "m"(kShuffleARGBToAB64Lo),  // %3
        "m"(kShuffleARGBToAB64Hi)   // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3");
}
#endif  // HAS_ARGBTOAB64ROW_AVX2

#ifdef HAS_AR64TOARGBROW_AVX2
void AR64ToARGBRow_AVX2(const uint16_t* src_ar64,
                        uint8_t* dst_argb,
                        int width) {
  asm volatile(
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "vpsrlw      $8,%%ymm0,%%ymm0              \n"
      "vpsrlw      $8,%%ymm1,%%ymm1              \n"
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "lea         0x40(%0),%0                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_ar64),  // %0
        "+r"(dst_argb),  // %1
        "+r"(width)      // %2
        ::"memory",
        "cc", "xmm0", "xmm1");
}
#endif  // HAS_AR64TOARGBROW_AVX2

#ifdef HAS_AB64TOARGBROW_AVX2
void AB64ToARGBRow_AVX2(const uint16_t* src_ab64,
                        uint8_t* dst_argb,
                        int width) {
  asm volatile("vbroadcasti128 %3,%%ymm2                  \n" LABELALIGN
               "1:          \n"
               "vmovdqu     (%0),%%ymm0                   \n"
               "vmovdqu     0x20(%0),%%ymm1               \n"
               "vpsrlw      $8,%%ymm0,%%ymm0              \n"
               "vpsrlw      $8,%%ymm1,%%ymm1              \n"
               "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
               "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
               "vpshufb     %%ymm2,%%ymm0,%%ymm0          \n"
               "vmovdqu     %%ymm0,(%1)                   \n"
               "lea         0x40(%0),%0                   \n"
               "lea         0x20(%1),%1                   \n"
               "sub         $0x8,%2                       \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : "+r"(src_ab64),          // %0
                 "+r"(dst_argb),          // %1
                 "+r"(width)              // %2
               : "m"(kShuffleARGBToABGR)  // %3
               : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_AB64TOARGBROW_AVX2

// clang-format off

// TODO(mraptis): Consider passing R, G, B multipliers as parameter.
// round parameter is register containing value to add before shift.
#define RGBTOY(round)                            \
  "1:                                        \n" \
  "movdqu    (%0),%%xmm0                     \n" \
  "movdqu    0x10(%0),%%xmm1                 \n" \
  "movdqu    0x20(%0),%%xmm2                 \n" \
  "movdqu    0x30(%0),%%xmm3                 \n" \
  "psubb     %%xmm5,%%xmm0                   \n" \
  "psubb     %%xmm5,%%xmm1                   \n" \
  "psubb     %%xmm5,%%xmm2                   \n" \
  "psubb     %%xmm5,%%xmm3                   \n" \
  "movdqu    %%xmm4,%%xmm6                   \n" \
  "pmaddubsw %%xmm0,%%xmm6                   \n" \
  "movdqu    %%xmm4,%%xmm0                   \n" \
  "pmaddubsw %%xmm1,%%xmm0                   \n" \
  "movdqu    %%xmm4,%%xmm1                   \n" \
  "pmaddubsw %%xmm2,%%xmm1                   \n" \
  "movdqu    %%xmm4,%%xmm2                   \n" \
  "pmaddubsw %%xmm3,%%xmm2                   \n" \
  "lea       0x40(%0),%0                     \n" \
  "phaddw    %%xmm0,%%xmm6                   \n" \
  "phaddw    %%xmm2,%%xmm1                   \n" \
  "prefetcht0 1280(%0)                       \n" \
  "paddw     %%" #round ",%%xmm6             \n" \
  "paddw     %%" #round ",%%xmm1             \n" \
  "psrlw     $0x8,%%xmm6                     \n" \
  "psrlw     $0x8,%%xmm1                     \n" \
  "packuswb  %%xmm1,%%xmm6                   \n" \
  "movdqu    %%xmm6,(%1)                     \n" \
  "lea       0x10(%1),%1                     \n" \
  "sub       $0x10,%2                        \n" \
  "jg        1b                              \n"

#define RGBTOY_AVX2(round)                                       \
  "1:                                        \n"                 \
  "vmovdqu    (%0),%%ymm0                    \n"                 \
  "vmovdqu    0x20(%0),%%ymm1                \n"                 \
  "vmovdqu    0x40(%0),%%ymm2                \n"                 \
  "vmovdqu    0x60(%0),%%ymm3                \n"                 \
  "vpsubb     %%ymm5, %%ymm0, %%ymm0         \n"                 \
  "vpsubb     %%ymm5, %%ymm1, %%ymm1         \n"                 \
  "vpsubb     %%ymm5, %%ymm2, %%ymm2         \n"                 \
  "vpsubb     %%ymm5, %%ymm3, %%ymm3         \n"                 \
  "vpmaddubsw %%ymm0,%%ymm4,%%ymm0           \n"                 \
  "vpmaddubsw %%ymm1,%%ymm4,%%ymm1           \n"                 \
  "vpmaddubsw %%ymm2,%%ymm4,%%ymm2           \n"                 \
  "vpmaddubsw %%ymm3,%%ymm4,%%ymm3           \n"                 \
  "lea       0x80(%0),%0                     \n"                 \
  "vphaddw    %%ymm1,%%ymm0,%%ymm0           \n" /* mutates. */  \
  "vphaddw    %%ymm3,%%ymm2,%%ymm2           \n"                 \
  "prefetcht0 1280(%0)                       \n"                 \
  "vpaddw     %%" #round ",%%ymm0,%%ymm0     \n" /* Add 16 */    \
  "vpaddw     %%" #round ",%%ymm2,%%ymm2     \n"                 \
  "vpsrlw     $0x8,%%ymm0,%%ymm0             \n"                 \
  "vpsrlw     $0x8,%%ymm2,%%ymm2             \n"                 \
  "vpackuswb  %%ymm2,%%ymm0,%%ymm0           \n" /* mutates. */  \
  "vpermd     %%ymm0,%%ymm6,%%ymm0           \n" /* unmutate. */ \
  "vmovdqu    %%ymm0,(%1)                    \n"                 \
  "lea       0x20(%1),%1                     \n"                 \
  "sub       $0x20,%2                        \n"                 \
  "jg        1b                              \n"

// clang-format on

#if defined(HAS_ARGBTOYMATRIXROW_AVX2) ||     \
    defined(HAS_ARGBTOUV444MATRIXROW_AVX2) || \
    defined(HAS_ARGBEXTRACTALPHAROW_AVX2)
// vpermd for vphaddw + vpackuswb vpermd.
static const lvec32 kPermdARGBToY_AVX = {0, 4, 1, 5, 2, 6, 3, 7};
#endif  // defined(HAS_ARGBTOYMATRIXROW_AVX2) ||
        // defined(HAS_ARGBTOUV444MATRIXROW_AVX2) ||
        // defined(HAS_ARGBEXTRACTALPHAROW_AVX2)

#ifdef HAS_ARGBTOYMATRIXROW_SSSE3
void ARGBToYMatrixRow_SSSE3(const uint8_t* src_argb,
                            uint8_t* dst_y,
                            int width,
                            const struct ArgbConstants* c) {
  asm volatile(
      "pcmpeqb     %%xmm5,%%xmm5                 \n"
      "psllw       $15,%%xmm5                    \n"
      "packsswb    %%xmm5,%%xmm5                 \n"
      "movdqa      0(%3),%%xmm4                  \n"
      "movdqa      0x60(%3),%%xmm7               \n"
      "movdqa      %%xmm4,%%xmm6                 \n"
      "pmaddubsw   %%xmm5,%%xmm6                 \n"
      "phaddw      %%xmm6,%%xmm6                 \n"
      "psubw       %%xmm6,%%xmm7                 \n" LABELALIGN "" RGBTOY(xmm7)
      : "+r"(src_argb),  // %0
        "+r"(dst_y),     // %1
        "+r"(width)      // %2
      : "r"(c)           // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_ARGBTOYMATRIXROW_SSSE3

#ifdef HAS_ARGBTOYMATRIXROW_AVX2
void ARGBToYMatrixRow_AVX2(const uint8_t* src_argb,
                           uint8_t* dst_y,
                           int width,
                           const struct ArgbConstants* c) {
  asm volatile(
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"
      "vpsllw      $15,%%ymm5,%%ymm5             \n"
      "vpacksswb   %%ymm5,%%ymm5,%%ymm5          \n"
      "vbroadcasti128 0(%3),%%ymm4               \n"
      "vbroadcasti128 0x60(%3),%%ymm7            \n"
      "vpmaddubsw  %%ymm5,%%ymm4,%%ymm6          \n"
      "vphaddw     %%ymm6,%%ymm6,%%ymm6          \n"
      "vpsubw      %%ymm6,%%ymm7,%%ymm7          \n"
      "vmovdqa     %4,%%ymm6                     \n" LABELALIGN
      "" RGBTOY_AVX2(ymm7) "vzeroupper  \n"
      : "+r"(src_argb),         // %0
        "+r"(dst_y),            // %1
        "+r"(width)             // %2
      : "r"(c),                 // %3
        "m"(kPermdARGBToY_AVX)  // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_ARGBTOYMATRIXROW_AVX2

#if defined(HAS_ARGBTOYMATRIXROW_AVX512BW) ||     \
    defined(HAS_ARGBTOUV444MATRIXROW_AVX512BW) || \
    defined(HAS_ARGBTOUVMATRIXROW_AVX512BW) ||    \
    defined(HAS_RGBTOYMATRIXROW_AVX512BW)
static const uint32_t kPermdARGBToY_AVX512BW[16] = {0, 4, 8,  12, 1, 5, 9,  13,
                                                    2, 6, 10, 14, 3, 7, 11, 15};
#endif  // defined(HAS_ARGBTOYMATRIXROW_AVX512BW) ||
        // defined(HAS_ARGBTOUV444MATRIXROW_AVX512BW) ||
        // defined(HAS_ARGBTOUVMATRIXROW_AVX512BW) ||
        // defined(HAS_RGBTOYMATRIXROW_AVX512BW)

#if defined(HAS_ARGBTOUVMATRIXROW_AVX512BW)
static const uint32_t kPermdARGBToUV_AVX512BW[16] = {
    0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15};
#endif  // defined(HAS_ARGBTOUVMATRIXROW_AVX512BW)

#ifdef HAS_ARGBTOYMATRIXROW_AVX512BW
void ARGBToYMatrixRow_AVX512BW(const uint8_t* src_argb,
                               uint8_t* dst_y,
                               int width,
                               const struct ArgbConstants* c) {
  asm volatile(
      "vpternlogd  $0xff,%%zmm16,%%zmm16,%%zmm16 \n"
      "vpsllw      $15,%%zmm16,%%zmm5            \n"
      "vpacksswb   %%zmm5,%%zmm5,%%zmm5          \n"  // zmm5 = 0x80
      "vpabsw      %%zmm16,%%zmm16               \n"  // zmm16 = 1
      "vbroadcasti32x4 0(%3),%%zmm4              \n"
      "vbroadcasti32x4 0x60(%3),%%zmm17          \n"
      "vpmaddubsw  %%zmm5,%%zmm4,%%zmm18         \n"
      "vpmaddwd    %%zmm16,%%zmm18,%%zmm18       \n"
      "vpackssdw   %%zmm18,%%zmm18,%%zmm18       \n"
      "vpsubw      %%zmm18,%%zmm17,%%zmm17       \n"
      "vmovups     %4,%%zmm18                    \n" LABELALIGN
      "1:          \n"
      "vmovups     (%0),%%zmm0                   \n"
      "vmovups     0x40(%0),%%zmm1               \n"
      "vmovups     0x80(%0),%%zmm2               \n"
      "vmovups     0xc0(%0),%%zmm3               \n"
      "vpsubb      %%zmm5,%%zmm0,%%zmm0          \n"
      "vpsubb      %%zmm5,%%zmm1,%%zmm1          \n"
      "vpsubb      %%zmm5,%%zmm2,%%zmm2          \n"
      "vpsubb      %%zmm5,%%zmm3,%%zmm3          \n"
      "vpmaddubsw  %%zmm0,%%zmm4,%%zmm0          \n"
      "vpmaddubsw  %%zmm1,%%zmm4,%%zmm1          \n"
      "vpmaddubsw  %%zmm2,%%zmm4,%%zmm2          \n"
      "vpmaddubsw  %%zmm3,%%zmm4,%%zmm3          \n"
      "lea         0x100(%0),%0                  \n"
      "vpmaddwd    %%zmm16,%%zmm0,%%zmm0         \n"
      "vpmaddwd    %%zmm16,%%zmm1,%%zmm1         \n"
      "vpackssdw   %%zmm1,%%zmm0,%%zmm0          \n"
      "vpmaddwd    %%zmm16,%%zmm2,%%zmm2         \n"
      "vpmaddwd    %%zmm16,%%zmm3,%%zmm3         \n"
      "vpackssdw   %%zmm3,%%zmm2,%%zmm2          \n"
      "vpaddw      %%zmm17,%%zmm0,%%zmm0         \n"
      "vpaddw      %%zmm17,%%zmm2,%%zmm2         \n"
      "vpsrlw      $0x8,%%zmm0,%%zmm0            \n"
      "vpsrlw      $0x8,%%zmm2,%%zmm2            \n"
      "vpackuswb   %%zmm2,%%zmm0,%%zmm0          \n"
      "vpermd      %%zmm0,%%zmm18,%%zmm0         \n"
      "vmovups     %%zmm0,(%1)                   \n"
      "lea         0x40(%1),%1                   \n"
      "sub         $0x40,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_argb),              // %0
        "+r"(dst_y),                 // %1
        "+r"(width)                  // %2
      : "r"(c),                      // %3
        "m"(kPermdARGBToY_AVX512BW)  // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm16",
        "xmm17", "xmm18");
}
#endif  // HAS_ARGBTOYMATRIXROW_AVX512BW

#ifdef HAS_RGBTOYMATRIXROW_AVX512BW
void RGBToYMatrixRow_AVX512BW(const uint8_t* src_rgb,
                              uint8_t* dst_y,
                              int width,
                              const struct ArgbConstants* c) {
  asm volatile(
      "vmovdqu64   %5,%%zmm20                    \n"  // kPermqRGB24ToARGB0_AVX512BW
      "vmovdqu64   %6,%%zmm21                    \n"  // kPermqRGB24ToARGB1_AVX512BW
      "vmovdqu64   %7,%%zmm22                    \n"  // kPermqRGB24ToARGB2_AVX512BW
      "vmovdqu64   %8,%%zmm23                    \n"  // kPermqRGB24ToARGB3_AVX512BW
      "vbroadcasti64x4 %9,%%zmm24                \n"  // kShuffleMaskRGB24ToARGB
      "vpternlogd  $0xff,%%zmm16,%%zmm16,%%zmm16 \n"
      "vpsllw      $15,%%zmm16,%%zmm5            \n"
      "vpacksswb   %%zmm5,%%zmm5,%%zmm5          \n"  // zmm5 = 0x80
      "vpabsw      %%zmm16,%%zmm16               \n"  // zmm16 = 1
      "vbroadcasti32x4 0(%3),%%zmm4              \n"  // zmm4 = kRGBToY
      "vbroadcasti32x4 0x60(%3),%%zmm17          \n"  // zmm17 = kAddY
      "vpmaddubsw  %%zmm5,%%zmm4,%%zmm18         \n"
      "vpmaddwd    %%zmm16,%%zmm18,%%zmm18       \n"
      "vpackssdw   %%zmm18,%%zmm18,%%zmm18       \n"
      "vpsubw      %%zmm18,%%zmm17,%%zmm17       \n"  // zmm17 = adjusted bias
      "vmovups     %4,%%zmm25                    \n"  // zmm25 =
                                                      // kPermdARGBToY_AVX512BW
      LABELALIGN
      "1:          \n"
      "vmovdqu8    (%0),%%zmm0                   \n"
      "vmovdqu8    64(%0),%%zmm1                 \n"
      "vmovdqu8    128(%0),%%zmm2                \n"
      "lea         192(%0),%0                    \n"
      "vpermq      %%zmm2,%%zmm23,%%zmm3         \n"
      "vmovdqa64   %%zmm22,%%zmm18               \n"
      "vpermi2q    %%zmm2,%%zmm1,%%zmm18         \n"
      "vmovdqa64   %%zmm21,%%zmm19               \n"
      "vpermi2q    %%zmm1,%%zmm0,%%zmm19         \n"
      "vpermq      %%zmm0,%%zmm20,%%zmm0         \n"
      "vmovdqa64   %%zmm19,%%zmm1                \n"
      "vmovdqa64   %%zmm18,%%zmm2                \n"
      "vpshufb     %%zmm24,%%zmm0,%%zmm0         \n"
      "vpshufb     %%zmm24,%%zmm1,%%zmm1         \n"
      "vpshufb     %%zmm24,%%zmm2,%%zmm2         \n"
      "vpshufb     %%zmm24,%%zmm3,%%zmm3         \n"
      "vpsubb      %%zmm5,%%zmm0,%%zmm0          \n"
      "vpsubb      %%zmm5,%%zmm1,%%zmm1          \n"
      "vpsubb      %%zmm5,%%zmm2,%%zmm2          \n"
      "vpsubb      %%zmm5,%%zmm3,%%zmm3          \n"
      "vpmaddubsw  %%zmm0,%%zmm4,%%zmm0          \n"
      "vpmaddubsw  %%zmm1,%%zmm4,%%zmm1          \n"
      "vpmaddubsw  %%zmm2,%%zmm4,%%zmm2          \n"
      "vpmaddubsw  %%zmm3,%%zmm4,%%zmm3          \n"
      "vpmaddwd    %%zmm16,%%zmm0,%%zmm0         \n"
      "vpmaddwd    %%zmm16,%%zmm1,%%zmm1         \n"
      "vpackssdw   %%zmm1,%%zmm0,%%zmm0          \n"
      "vpmaddwd    %%zmm16,%%zmm2,%%zmm2         \n"
      "vpmaddwd    %%zmm16,%%zmm3,%%zmm3         \n"
      "vpackssdw   %%zmm3,%%zmm2,%%zmm2          \n"
      "vpaddw      %%zmm17,%%zmm0,%%zmm0         \n"
      "vpaddw      %%zmm17,%%zmm2,%%zmm2         \n"
      "vpsrlw      $0x8,%%zmm0,%%zmm0            \n"
      "vpsrlw      $0x8,%%zmm2,%%zmm2            \n"
      "vpackuswb   %%zmm2,%%zmm0,%%zmm0          \n"
      "vpermd      %%zmm0,%%zmm25,%%zmm0         \n"
      "vmovups     %%zmm0,(%1)                   \n"
      "lea         0x40(%1),%1                   \n"
      "sub         $0x40,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_rgb),                     // %0
        "+r"(dst_y),                       // %1
        "+r"(width)                        // %2
      : "r"(c),                            // %3
        "m"(kPermdARGBToY_AVX512BW),       // %4
        "m"(kPermqRGB24ToARGB0_AVX512BW),  // %5
        "m"(kPermqRGB24ToARGB1_AVX512BW),  // %6
        "m"(kPermqRGB24ToARGB2_AVX512BW),  // %7
        "m"(kPermqRGB24ToARGB3_AVX512BW),  // %8
        "m"(kShuffleMaskRGB24ToARGB)       // %9
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm16",
        "xmm17", "xmm18", "xmm19", "xmm20", "xmm21", "xmm22", "xmm23", "xmm24",
        "xmm25");
}
#endif  // HAS_RGBTOYMATRIXROW_AVX512BW

#ifdef HAS_ARGBTOUV444MATRIXROW_SSSE3
void ARGBToUV444MatrixRow_SSSE3(const uint8_t* src_argb,
                                uint8_t* dst_u,
                                uint8_t* dst_v,
                                int width,
                                const struct ArgbConstants* c) {
  asm volatile(
      "pcmpeqb     %%xmm5,%%xmm5                 \n"  // 0x8000
      "psllw       $15,%%xmm5                    \n"
      "movdqa      0x20(%4),%%xmm3               \n"  // kRGBToU
      "movdqa      0x40(%4),%%xmm4               \n"  // kRGBToV
      "sub         %1,%2                         \n"

      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "movdqu      0x20(%0),%%xmm2               \n"
      "movdqu      0x30(%0),%%xmm6               \n"
      "pmaddubsw   %%xmm3,%%xmm0                 \n"
      "pmaddubsw   %%xmm3,%%xmm1                 \n"
      "pmaddubsw   %%xmm3,%%xmm2                 \n"
      "pmaddubsw   %%xmm3,%%xmm6                 \n"
      "phaddw      %%xmm1,%%xmm0                 \n"
      "phaddw      %%xmm6,%%xmm2                 \n"
      "movdqa      %%xmm5,%%xmm1                 \n"
      "movdqa      %%xmm5,%%xmm6                 \n"
      "psubw       %%xmm0,%%xmm1                 \n"
      "psubw       %%xmm2,%%xmm6                 \n"
      "psrlw       $0x8,%%xmm1                   \n"
      "psrlw       $0x8,%%xmm6                   \n"
      "packuswb    %%xmm6,%%xmm1                 \n"
      "movdqu      %%xmm1,(%1)                   \n"

      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "movdqu      0x20(%0),%%xmm2               \n"
      "movdqu      0x30(%0),%%xmm6               \n"
      "pmaddubsw   %%xmm4,%%xmm0                 \n"
      "pmaddubsw   %%xmm4,%%xmm1                 \n"
      "pmaddubsw   %%xmm4,%%xmm2                 \n"
      "pmaddubsw   %%xmm4,%%xmm6                 \n"
      "phaddw      %%xmm1,%%xmm0                 \n"
      "phaddw      %%xmm6,%%xmm2                 \n"
      "movdqa      %%xmm5,%%xmm1                 \n"
      "movdqa      %%xmm5,%%xmm6                 \n"
      "psubw       %%xmm0,%%xmm1                 \n"
      "psubw       %%xmm2,%%xmm6                 \n"
      "psrlw       $0x8,%%xmm1                   \n"
      "psrlw       $0x8,%%xmm6                   \n"
      "packuswb    %%xmm6,%%xmm1                 \n"
      "movdqu      %%xmm1,0x00(%1,%2,1)          \n"

      "lea         0x40(%0),%0                   \n"
      "lea         0x10(%1),%1                   \n"
      "subl        $0x10,%3                      \n"
      "jg          1b                            \n"
      : "+r"(src_argb),  // %0
        "+r"(dst_u),     // %1
        "+r"(dst_v),     // %2
#if defined(__i386__)
        "+m"(width)  // %3
#else
        "+rm"(width)  // %3
#endif
      : "r"(c)  // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}
#endif  // HAS_ARGBTOUV444MATRIXROW_SSSE3

#ifdef HAS_ARGBTOUV444MATRIXROW_AVX2

void ARGBToUV444MatrixRow_AVX2(const uint8_t* src_argb,
                               uint8_t* dst_u,
                               uint8_t* dst_v,
                               int width,
                               const struct ArgbConstants* c) {
  asm volatile(
      "vmovdqa     0x20(%4),%%ymm3               \n"  // kRGBToU
      "vmovdqa     0x40(%4),%%ymm4               \n"  // kRGBToV
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"  // 0x8000
      "vpsllw      $15,%%ymm5,%%ymm5             \n"
      "vmovdqa     %5,%%ymm7                     \n"
      "sub         %1,%2                         \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "vmovdqu     0x40(%0),%%ymm2               \n"
      "vmovdqu     0x60(%0),%%ymm6               \n"
      "vpmaddubsw  %%ymm3,%%ymm0,%%ymm0          \n"
      "vpmaddubsw  %%ymm3,%%ymm1,%%ymm1          \n"
      "vpmaddubsw  %%ymm3,%%ymm2,%%ymm2          \n"
      "vpmaddubsw  %%ymm3,%%ymm6,%%ymm6          \n"
      "vphaddw     %%ymm1,%%ymm0,%%ymm0          \n"  // mutates
      "vphaddw     %%ymm6,%%ymm2,%%ymm2          \n"
      "vpsubw      %%ymm0,%%ymm5,%%ymm0          \n"
      "vpsubw      %%ymm2,%%ymm5,%%ymm2          \n"
      "vpsrlw      $0x8,%%ymm0,%%ymm0            \n"
      "vpsrlw      $0x8,%%ymm2,%%ymm2            \n"
      "vpackuswb   %%ymm2,%%ymm0,%%ymm0          \n"  // mutates
      "vpermd      %%ymm0,%%ymm7,%%ymm0          \n"  // unmutate.
      "vmovdqu     %%ymm0,(%1)                   \n"

      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "vmovdqu     0x40(%0),%%ymm2               \n"
      "vmovdqu     0x60(%0),%%ymm6               \n"
      "vpmaddubsw  %%ymm4,%%ymm0,%%ymm0          \n"
      "vpmaddubsw  %%ymm4,%%ymm1,%%ymm1          \n"
      "vpmaddubsw  %%ymm4,%%ymm2,%%ymm2          \n"
      "vpmaddubsw  %%ymm4,%%ymm6,%%ymm6          \n"
      "vphaddw     %%ymm1,%%ymm0,%%ymm0          \n"  // mutates
      "vphaddw     %%ymm6,%%ymm2,%%ymm2          \n"
      "vpsubw      %%ymm0,%%ymm5,%%ymm0          \n"
      "vpsubw      %%ymm2,%%ymm5,%%ymm2          \n"
      "vpsrlw      $0x8,%%ymm0,%%ymm0            \n"
      "vpsrlw      $0x8,%%ymm2,%%ymm2            \n"
      "vpackuswb   %%ymm2,%%ymm0,%%ymm0          \n"  // mutates
      "vpermd      %%ymm0,%%ymm7,%%ymm0          \n"  // unmutate.
      "vmovdqu     %%ymm0,(%1,%2,1)              \n"
      "lea         0x80(%0),%0                   \n"
      "lea         0x20(%1),%1                   \n"
      "subl        $0x20,%3                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_argb),  // %0
        "+r"(dst_u),     // %1
        "+r"(dst_v),     // %2
#if defined(__i386__)
        "+m"(width)  // %3
#else
        "+rm"(width)  // %3
#endif
      : "r"(c),                 // %4
        "m"(kPermdARGBToY_AVX)  // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_ARGBTOUV444MATRIXROW_AVX2

#ifdef HAS_ARGBTOUV444MATRIXROW_AVX512BW

void ARGBToUV444MatrixRow_AVX512BW(const uint8_t* src_argb,
                                   uint8_t* dst_u,
                                   uint8_t* dst_v,
                                   int width,
                                   const struct ArgbConstants* c) {
  asm volatile(
      "vbroadcasti32x4 0x20(%4),%%zmm3           \n"  // kRGBToU
      "vbroadcasti32x4 0x40(%4),%%zmm4           \n"  // kRGBToV
      "vpternlogd  $0xff,%%zmm16,%%zmm16,%%zmm16 \n"  // -1
      "vpsllw      $15,%%zmm16,%%zmm5            \n"  // 0x8000
      "vmovups     %5,%%zmm17                    \n"
      "sub         %1,%2                         \n"

      LABELALIGN
      "1:          \n"
      "vmovups     (%0),%%zmm0                   \n"
      "vmovups     0x40(%0),%%zmm1               \n"
      "vmovups     0x80(%0),%%zmm2               \n"
      "vmovups     0xc0(%0),%%zmm18              \n"
      "vpmaddubsw  %%zmm3,%%zmm0,%%zmm0          \n"
      "vpmaddubsw  %%zmm3,%%zmm1,%%zmm1          \n"
      "vpmaddubsw  %%zmm3,%%zmm2,%%zmm2          \n"
      "vpmaddubsw  %%zmm3,%%zmm18,%%zmm18        \n"
      "vpmaddwd    %%zmm16,%%zmm0,%%zmm0         \n"
      "vpmaddwd    %%zmm16,%%zmm1,%%zmm1         \n"
      "vpmaddwd    %%zmm16,%%zmm2,%%zmm2         \n"
      "vpmaddwd    %%zmm16,%%zmm18,%%zmm18       \n"
      "vpackssdw   %%zmm1,%%zmm0,%%zmm0          \n"  // mutates
      "vpackssdw   %%zmm18,%%zmm2,%%zmm2         \n"
      "vpsubw      %%zmm5,%%zmm0,%%zmm0          \n"
      "vpsubw      %%zmm5,%%zmm2,%%zmm2          \n"
      "vpsrlw      $0x8,%%zmm0,%%zmm0            \n"
      "vpsrlw      $0x8,%%zmm2,%%zmm2            \n"
      "vpackuswb   %%zmm2,%%zmm0,%%zmm0          \n"  // mutates
      "vpermd      %%zmm0,%%zmm17,%%zmm0         \n"  // unmutate.
      "vmovups     %%zmm0,(%1)                   \n"

      "vmovups     (%0),%%zmm0                   \n"
      "vmovups     0x40(%0),%%zmm1               \n"
      "vmovups     0x80(%0),%%zmm2               \n"
      "vmovups     0xc0(%0),%%zmm18              \n"
      "vpmaddubsw  %%zmm4,%%zmm0,%%zmm0          \n"
      "vpmaddubsw  %%zmm4,%%zmm1,%%zmm1          \n"
      "vpmaddubsw  %%zmm4,%%zmm2,%%zmm2          \n"
      "vpmaddubsw  %%zmm4,%%zmm18,%%zmm18        \n"
      "vpmaddwd    %%zmm16,%%zmm0,%%zmm0         \n"
      "vpmaddwd    %%zmm16,%%zmm1,%%zmm1         \n"
      "vpmaddwd    %%zmm16,%%zmm2,%%zmm2         \n"
      "vpmaddwd    %%zmm16,%%zmm18,%%zmm18       \n"
      "vpackssdw   %%zmm1,%%zmm0,%%zmm0          \n"  // mutates
      "vpackssdw   %%zmm18,%%zmm2,%%zmm2         \n"
      "vpsubw      %%zmm5,%%zmm0,%%zmm0          \n"
      "vpsubw      %%zmm5,%%zmm2,%%zmm2          \n"
      "vpsrlw      $0x8,%%zmm0,%%zmm0            \n"
      "vpsrlw      $0x8,%%zmm2,%%zmm2            \n"
      "vpackuswb   %%zmm2,%%zmm0,%%zmm0          \n"  // mutates
      "vpermd      %%zmm0,%%zmm17,%%zmm0         \n"  // unmutate.
      "vmovups     %%zmm0,(%1,%2,1)              \n"
      "lea         0x100(%0),%0                  \n"
      "lea         0x40(%1),%1                   \n"
      "subl        $0x40,%3                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_argb),  // %0
        "+r"(dst_u),     // %1
        "+r"(dst_v),     // %2
#if defined(__i386__)
        "+m"(width)  // %3
#else
        "+rm"(width)  // %3
#endif
      : "r"(c),                      // %4
        "m"(kPermdARGBToY_AVX512BW)  // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm16",
        "xmm17", "xmm18");
}
#endif  // HAS_ARGBTOUV444MATRIXROW_AVX512BW

#ifdef HAS_ARGBTOUVMATRIXROW_SSSE3

// ARGBARGB to AARRGGBB shuffle
static const lvec8 kShuffleAARRGGBB = {
    0, 4, 1, 5, 2, 6, 3, 7, 8, 12, 9, 13, 10, 14, 11, 15,
    0, 4, 1, 5, 2, 6, 3, 7, 8, 12, 9, 13, 10, 14, 11, 15,
};

// 8x2 -> 4x1 ARGB pixels converted to 4 U and 4 V
// ARGBToUV does rounding average of 4 ARGB pixels
void ARGBToUVMatrixRow_SSSE3(const uint8_t* src_argb,
                             int src_stride_argb,
                             uint8_t* dst_u,
                             uint8_t* dst_v,
                             int width,
                             const struct ArgbConstants* c) {
  asm volatile(
      "movdqa      0x20(%5),%%xmm4               \n"  // RGBToU
      "movdqa      0x40(%5),%%xmm5               \n"  // RGBToV
      "pcmpeqb     %%xmm6,%%xmm6                 \n"  // 0x0101
      "pabsb       %%xmm6,%%xmm6                 \n"
      "movdqa      %6,%%xmm7                     \n"  // kShuffleAARRGGBB
      "sub         %1,%2                         \n"

      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"  // Read 8x2 ARGB Pixels
      "movdqu      0x10(%0),%%xmm1               \n"
      "movdqu      0x00(%0,%4,1),%%xmm2          \n"
      "movdqu      0x10(%0,%4,1),%%xmm3          \n"
      "pshufb      %%xmm7,%%xmm0                 \n"  // aarrggbb
      "pshufb      %%xmm7,%%xmm1                 \n"
      "pshufb      %%xmm7,%%xmm2                 \n"
      "pshufb      %%xmm7,%%xmm3                 \n"
      "pmaddubsw   %%xmm6,%%xmm0                 \n"  // 8x2 -> 4x2
      "pmaddubsw   %%xmm6,%%xmm1                 \n"
      "pmaddubsw   %%xmm6,%%xmm2                 \n"
      "pmaddubsw   %%xmm6,%%xmm3                 \n"
      "paddw       %%xmm2,%%xmm0                 \n"  // 4x2 -> 4x1
      "paddw       %%xmm3,%%xmm1                 \n"
      "pxor        %%xmm2,%%xmm2                 \n"  // 0 for vpavgw
      "psrlw       $1,%%xmm0                     \n"
      "psrlw       $1,%%xmm1                     \n"
      "pavgw       %%xmm2,%%xmm0                 \n"
      "pavgw       %%xmm2,%%xmm1                 \n"
      "packuswb    %%xmm1,%%xmm0                 \n"  // mutates

      "movdqa      %%xmm6,%%xmm2                 \n"
      "psllw       $15,%%xmm2                    \n"  // 0x8000
      "movdqa      %%xmm0,%%xmm1                 \n"
      "pmaddubsw   %%xmm5,%%xmm1                 \n"  // 4 V
      "pmaddubsw   %%xmm4,%%xmm0                 \n"  // 4 U
      "phaddw      %%xmm1,%%xmm0                 \n"  // uuuuvvvv
      "psubw       %%xmm0,%%xmm2                 \n"
      "psrlw       $0x8,%%xmm2                   \n"
      "packuswb    %%xmm2,%%xmm2                 \n"
      "movd        %%xmm2,(%1)                   \n"  // Write 4 U's
      "pshufd      $0x55,%%xmm2,%%xmm2           \n"  // Copy V to low 4 bytes
      "movd        %%xmm2,0x00(%1,%2,1)          \n"  // Write 4 V's

      "lea         0x20(%0),%0                  \n"
      "lea         0x4(%1),%1                   \n"
      "subl        $0x8,%3                      \n"
      "jg          1b                           \n"
      : "+r"(src_argb),  // %0
        "+r"(dst_u),     // %1
        "+r"(dst_v),     // %2
#if defined(__i386__)
        "+m"(width)  // %3
#else
        "+rm"(width)  // %3
#endif
      : "r"((ptrdiff_t)(src_stride_argb)),  // %4
        "r"(c),                             // %5
        "m"(kShuffleAARRGGBB)               // %6
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}

#endif  // HAS_ARGBTOUVMATRIXROW_SSSE3

#ifdef HAS_ARGBTOUVMATRIXROW_AVX2

// 16x2 -> 8x1 ARGB pixels converted to 8 U and 8 V
// ARGBToUV does rounding average of 4 ARGB pixels
void ARGBToUVMatrixRow_AVX2(const uint8_t* src_argb,
                            int src_stride_argb,
                            uint8_t* dst_u,
                            uint8_t* dst_v,
                            int width,
                            const struct ArgbConstants* c) {
  asm volatile(
      "vbroadcasti128 0x20(%5),%%ymm4           \n"   // RGBToU
      "vbroadcasti128 0x40(%5),%%ymm5           \n"   // RGBToV
      "vpcmpeqb    %%ymm6,%%ymm6,%%ymm6          \n"  // 0x0101
      "vpabsb      %%ymm6,%%ymm6                 \n"
      "vmovdqa     %6,%%ymm7                     \n"  // kShuffleAARRGGBB
      "sub         %1,%2                         \n"

      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"  // Read 16x2 ARGB Pixels
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "vmovdqu     0x00(%0,%4,1),%%ymm2          \n"
      "vmovdqu     0x20(%0,%4,1),%%ymm3          \n"
      "vpshufb     %%ymm7,%%ymm0,%%ymm0          \n"  // aarrggbb
      "vpshufb     %%ymm7,%%ymm1,%%ymm1          \n"
      "vpshufb     %%ymm7,%%ymm2,%%ymm2          \n"
      "vpshufb     %%ymm7,%%ymm3,%%ymm3          \n"
      "vpmaddubsw  %%ymm6,%%ymm0,%%ymm0          \n"  // 16x2 -> 8x2
      "vpmaddubsw  %%ymm6,%%ymm1,%%ymm1          \n"
      "vpmaddubsw  %%ymm6,%%ymm2,%%ymm2          \n"
      "vpmaddubsw  %%ymm6,%%ymm3,%%ymm3          \n"
      "vpaddw      %%ymm0,%%ymm2,%%ymm0          \n"  // 8x2 -> 8x1
      "vpaddw      %%ymm1,%%ymm3,%%ymm1          \n"
      "vpxor       %%ymm2,%%ymm2,%%ymm2          \n"  // 0 for vpavgw
      "vpsrlw      $1,%%ymm0,%%ymm0              \n"
      "vpsrlw      $1,%%ymm1,%%ymm1              \n"
      "vpavgw      %%ymm2,%%ymm0,%%ymm0          \n"
      "vpavgw      %%ymm2,%%ymm1,%%ymm1          \n"
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"  // mutates
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"  // 8 ARGB Pixels

      "vpsllw      $15,%%ymm6,%%ymm2             \n"  // 0x8000
      "vpmaddubsw  %%ymm5,%%ymm0,%%ymm1          \n"  // 8 V
      "vpmaddubsw  %%ymm4,%%ymm0,%%ymm0          \n"  // 8 U
      "vphaddw     %%ymm1,%%ymm0,%%ymm0          \n"  // uuuuvvvv uuuuvvvv
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"  // uuuuuuuu vvvvvvvv
      "vpsubw      %%ymm0,%%ymm2,%%ymm0          \n"
      "vpsrlw      $0x8,%%ymm0,%%ymm0            \n"
      "vpackuswb   %%ymm0,%%ymm0,%%ymm0          \n"  // mutates 8U8u- 8V8v
      "vmovq       %%xmm0,(%1)                   \n"  // Write 8 U's
      "vextractf128 $0x1,%%ymm0,%%xmm0           \n"  // Copy V to low 8 bytes
      "vmovq       %%xmm0,0x00(%1,%2,1)          \n"  // Write 8 V's

      "lea         0x40(%0),%0                   \n"
      "lea         0x8(%1),%1                    \n"
      "subl        $0x10,%3                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_argb),  // %0
        "+r"(dst_u),     // %1
        "+r"(dst_v),     // %2
#if defined(__i386__)
        "+m"(width)  // %3
#else
        "+rm"(width)  // %3
#endif
      : "r"((ptrdiff_t)(src_stride_argb)),  // %4
        "r"(c),                             // %5
        "m"(kShuffleAARRGGBB)               // %6
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_ARGBTOUVMATRIXROW_AVX2

// RGB to BT601 coefficients
// UB   0.875 coefficient = 112
// UG -0.5781 coefficient = -74
// UR -0.2969 coefficient = -38
// VB -0.1406 coefficient = -18
// VG -0.7344 coefficient = -94
// VR   0.875 coefficient = 112

#ifdef HAS_ARGBTOUVMATRIXROW_AVX512BW

// 32x2 -> 16x1 ARGB pixels converted to 16 U and 16 V
// ARGBToUV does rounding average of 4 ARGB pixels

void ARGBToUVMatrixRow_AVX512BW(const uint8_t* src_argb,
                                int src_stride_argb,
                                uint8_t* dst_u,
                                uint8_t* dst_v,
                                int width,
                                const struct ArgbConstants* c) {
  asm volatile(
      "vbroadcasti32x4 0x20(%5),%%zmm4           \n"  // RGBToU
      "vbroadcasti32x4 0x40(%5),%%zmm5           \n"  // RGBToV
      "vpternlogd  $0xff,%%zmm16,%%zmm16,%%zmm16 \n"
      "vpabsb      %%zmm16,%%zmm20               \n"  // 0x0101
      "vpsllw      $15,%%zmm16,%%zmm17           \n"  // 0x8000
      "vbroadcasti32x4 %6,%%zmm21                \n"  // kShuffleAARRGGBB
      "vmovups     %7,%%zmm18                    \n"  // kPermdARGBToY_AVX512BW
      "vmovups     %8,%%zmm19                    \n"  // kPermdARGBToUV_AVX512BW
      "sub         %1,%2                         \n"

      LABELALIGN
      "1:          \n"
      "vmovups     (%0),%%zmm0                   \n"  // Read 32x2 ARGB Pixels
      "vmovups     0x40(%0),%%zmm1               \n"
      "vmovups     0x00(%0,%4,1),%%zmm2          \n"
      "vmovups     0x40(%0,%4,1),%%zmm3          \n"
      "vpshufb     %%zmm21,%%zmm0,%%zmm0         \n"  // aarrggbb
      "vpshufb     %%zmm21,%%zmm1,%%zmm1         \n"
      "vpshufb     %%zmm21,%%zmm2,%%zmm2         \n"
      "vpshufb     %%zmm21,%%zmm3,%%zmm3         \n"
      "vpmaddubsw  %%zmm20,%%zmm0,%%zmm0         \n"  // 32x2 -> 16x2
      "vpmaddubsw  %%zmm20,%%zmm1,%%zmm1         \n"
      "vpmaddubsw  %%zmm20,%%zmm2,%%zmm2         \n"
      "vpmaddubsw  %%zmm20,%%zmm3,%%zmm3         \n"
      "vpaddw      %%zmm0,%%zmm2,%%zmm0          \n"  // 16x2 -> 16x1
      "vpaddw      %%zmm1,%%zmm3,%%zmm1          \n"
      "vpxorq      %%zmm2,%%zmm2,%%zmm2          \n"  // 0 for vpavgw
      "vpsrlw      $1,%%zmm0,%%zmm0              \n"
      "vpsrlw      $1,%%zmm1,%%zmm1              \n"
      "vpavgw      %%zmm2,%%zmm0,%%zmm0          \n"
      "vpavgw      %%zmm2,%%zmm1,%%zmm1          \n"
      "vpackuswb   %%zmm1,%%zmm0,%%zmm0          \n"  // mutates
      "vpermd      %%zmm0,%%zmm19,%%zmm0         \n"  // unscramble pixels

      "vpmaddubsw  %%zmm4,%%zmm0,%%zmm1          \n"  // 16 U
      "vpmaddubsw  %%zmm5,%%zmm0,%%zmm0          \n"  // 16 V
      "vpmaddwd    %%zmm16,%%zmm1,%%zmm1         \n"
      "vpmaddwd    %%zmm16,%%zmm0,%%zmm0         \n"
      "vpackssdw   %%zmm0,%%zmm1,%%zmm0          \n"  // mutates (U in lower, V
                                                      // in upper)
      "vpaddw      %%zmm17,%%zmm0,%%zmm0         \n"
      "vpsrlw      $0x8,%%zmm0,%%zmm0            \n"
      "vpackuswb   %%zmm0,%%zmm0,%%zmm0          \n"  // mutates
      "vpermd      %%zmm0,%%zmm18,%%zmm0         \n"  // unmutate

      "vmovdqu     %%xmm0,(%1)                   \n"  // Write 16 U's
      "vextracti32x4 $0x1,%%zmm0,%%xmm0          \n"
      "vmovdqu     %%xmm0,0x00(%1,%2,1)          \n"  // Write 16 V's

      "lea         0x80(%0),%0                   \n"
      "lea         0x10(%1),%1                   \n"
      "subl        $0x20,%3                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_argb),  // %0
        "+r"(dst_u),     // %1
        "+r"(dst_v),     // %2
#if defined(__i386__)
        "+m"(width)  // %3
#else
        "+rm"(width)  // %3
#endif
      : "r"((ptrdiff_t)(src_stride_argb)),  // %4
        "r"(c),                             // %5
        "m"(kShuffleAARRGGBB),              // %6
        "m"(kPermdARGBToY_AVX512BW),        // %7
        "m"(kPermdARGBToUV_AVX512BW)        // %8
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm16",
        "xmm17", "xmm18", "xmm19", "xmm20", "xmm21");
}
#endif  // HAS_ARGBTOUVMATRIXROW_AVX512BW

#if defined(HAS_I422TOARGBROW_SSSE3) || defined(HAS_I422TOARGBROW_AVX2)

// Read 8 UV from 444
#define READYUV444                                                \
  "movq       (%[u_buf]),%%xmm3                               \n" \
  "movq       0x00(%[u_buf],%[v_buf],1),%%xmm1                \n" \
  "lea        0x8(%[u_buf]),%[u_buf]                          \n" \
  "punpcklbw  %%xmm1,%%xmm3                                   \n" \
  "movq       (%[y_buf]),%%xmm4                               \n" \
  "punpcklbw  %%xmm4,%%xmm4                                   \n" \
  "lea        0x8(%[y_buf]),%[y_buf]                          \n"

// Read 4 UV from 422, upsample to 8 UV
#define READYUV422                                                \
  "movd       (%[u_buf]),%%xmm3                               \n" \
  "movd       0x00(%[u_buf],%[v_buf],1),%%xmm1                \n" \
  "lea        0x4(%[u_buf]),%[u_buf]                          \n" \
  "punpcklbw  %%xmm1,%%xmm3                                   \n" \
  "punpcklwd  %%xmm3,%%xmm3                                   \n" \
  "movq       (%[y_buf]),%%xmm4                               \n" \
  "punpcklbw  %%xmm4,%%xmm4                                   \n" \
  "lea        0x8(%[y_buf]),%[y_buf]                          \n"

// Read 4 UV from 422 10 bit, upsample to 8 UV
#define READYUV210                                                \
  "movq       (%[u_buf]),%%xmm3                               \n" \
  "movq       0x00(%[u_buf],%[v_buf],1),%%xmm1                \n" \
  "lea        0x8(%[u_buf]),%[u_buf]                          \n" \
  "punpcklwd  %%xmm1,%%xmm3                                   \n" \
  "psraw      $2,%%xmm3                                       \n" \
  "packuswb   %%xmm3,%%xmm3                                   \n" \
  "punpcklwd  %%xmm3,%%xmm3                                   \n" \
  "movdqu     (%[y_buf]),%%xmm4                               \n" \
  "movdqa     %%xmm4,%%xmm2                                   \n" \
  "psllw      $6,%%xmm4                                       \n" \
  "psrlw      $4,%%xmm2                                       \n" \
  "paddw      %%xmm2,%%xmm4                                   \n" \
  "lea        0x10(%[y_buf]),%[y_buf]                         \n"

#define READYUVA210                                               \
  "movq       (%[u_buf]),%%xmm3                               \n" \
  "movq       0x00(%[u_buf],%[v_buf],1),%%xmm1                \n" \
  "lea        0x8(%[u_buf]),%[u_buf]                          \n" \
  "punpcklwd  %%xmm1,%%xmm3                                   \n" \
  "psraw      $2,%%xmm3                                       \n" \
  "packuswb   %%xmm3,%%xmm3                                   \n" \
  "punpcklwd  %%xmm3,%%xmm3                                   \n" \
  "movdqu     (%[y_buf]),%%xmm4                               \n" \
  "movdqa     %%xmm4,%%xmm2                                   \n" \
  "psllw      $6,%%xmm4                                       \n" \
  "psrlw      $4,%%xmm2                                       \n" \
  "paddw      %%xmm2,%%xmm4                                   \n" \
  "lea        0x10(%[y_buf]),%[y_buf]                         \n" \
  "movdqu     (%[a_buf]),%%xmm5                               \n" \
  "psraw      $2,%%xmm5                                       \n" \
  "packuswb   %%xmm5,%%xmm5                                   \n" \
  "lea        0x10(%[a_buf]),%[a_buf]                         \n"

// Read 8 UV from 444 10 bit
#define READYUV410                                                \
  "movdqu     (%[u_buf]),%%xmm3                               \n" \
  "movdqu     0x00(%[u_buf],%[v_buf],1),%%xmm2                \n" \
  "lea        0x10(%[u_buf]),%[u_buf]                         \n" \
  "psraw      $2,%%xmm3                                       \n" \
  "psraw      $2,%%xmm2                                       \n" \
  "movdqa     %%xmm3,%%xmm1                                   \n" \
  "punpcklwd  %%xmm2,%%xmm3                                   \n" \
  "punpckhwd  %%xmm2,%%xmm1                                   \n" \
  "packuswb   %%xmm1,%%xmm3                                   \n" \
  "movdqu     (%[y_buf]),%%xmm4                               \n" \
  "movdqa     %%xmm4,%%xmm2                                   \n" \
  "psllw      $6,%%xmm4                                       \n" \
  "psrlw      $4,%%xmm2                                       \n" \
  "paddw      %%xmm2,%%xmm4                                   \n" \
  "lea        0x10(%[y_buf]),%[y_buf]                         \n"

// Read 8 UV from 444 10 bit.  With 8 Alpha.
#define READYUVA410                                               \
  "movdqu     (%[u_buf]),%%xmm3                               \n" \
  "movdqu     0x00(%[u_buf],%[v_buf],1),%%xmm2                \n" \
  "lea        0x10(%[u_buf]),%[u_buf]                         \n" \
  "psraw      $2,%%xmm3                                       \n" \
  "psraw      $2,%%xmm2                                       \n" \
  "movdqa     %%xmm3,%%xmm1                                   \n" \
  "punpcklwd  %%xmm2,%%xmm3                                   \n" \
  "punpckhwd  %%xmm2,%%xmm1                                   \n" \
  "packuswb   %%xmm1,%%xmm3                                   \n" \
  "movdqu     (%[y_buf]),%%xmm4                               \n" \
  "movdqa     %%xmm4,%%xmm2                                   \n" \
  "psllw      $6,%%xmm4                                       \n" \
  "psrlw      $4,%%xmm2                                       \n" \
  "paddw      %%xmm2,%%xmm4                                   \n" \
  "lea        0x10(%[y_buf]),%[y_buf]                         \n" \
  "movdqu     (%[a_buf]),%%xmm5                               \n" \
  "psraw      $2,%%xmm5                                       \n" \
  "packuswb   %%xmm5,%%xmm5                                   \n" \
  "lea        0x10(%[a_buf]),%[a_buf]                         \n"

// Read 4 UV from 422 12 bit, upsample to 8 UV
#define READYUV212                                                \
  "movq       (%[u_buf]),%%xmm3                               \n" \
  "movq       0x00(%[u_buf],%[v_buf],1),%%xmm1                \n" \
  "lea        0x8(%[u_buf]),%[u_buf]                          \n" \
  "punpcklwd  %%xmm1,%%xmm3                                   \n" \
  "psraw      $0x4,%%xmm3                                     \n" \
  "packuswb   %%xmm3,%%xmm3                                   \n" \
  "punpcklwd  %%xmm3,%%xmm3                                   \n" \
  "movdqu     (%[y_buf]),%%xmm4                               \n" \
  "movdqa     %%xmm4,%%xmm2                                   \n" \
  "psllw      $4,%%xmm4                                       \n" \
  "psrlw      $8,%%xmm2                                       \n" \
  "paddw      %%xmm2,%%xmm4                                   \n" \
  "lea        0x10(%[y_buf]),%[y_buf]                         \n"

// Read 4 UV from 422, upsample to 8 UV.  With 8 Alpha.
#define READYUVA422                                               \
  "movd       (%[u_buf]),%%xmm3                               \n" \
  "movd       0x00(%[u_buf],%[v_buf],1),%%xmm1                \n" \
  "lea        0x4(%[u_buf]),%[u_buf]                          \n" \
  "punpcklbw  %%xmm1,%%xmm3                                   \n" \
  "punpcklwd  %%xmm3,%%xmm3                                   \n" \
  "movq       (%[y_buf]),%%xmm4                               \n" \
  "punpcklbw  %%xmm4,%%xmm4                                   \n" \
  "lea        0x8(%[y_buf]),%[y_buf]                          \n" \
  "movq       (%[a_buf]),%%xmm5                               \n" \
  "lea        0x8(%[a_buf]),%[a_buf]                          \n"

// Read 8 UV from 444.  With 8 Alpha.
#define READYUVA444                                               \
  "movq       (%[u_buf]),%%xmm3                               \n" \
  "movq       0x00(%[u_buf],%[v_buf],1),%%xmm1                \n" \
  "lea        0x8(%[u_buf]),%[u_buf]                          \n" \
  "punpcklbw  %%xmm1,%%xmm3                                   \n" \
  "movq       (%[y_buf]),%%xmm4                               \n" \
  "punpcklbw  %%xmm4,%%xmm4                                   \n" \
  "lea        0x8(%[y_buf]),%[y_buf]                          \n" \
  "movq       (%[a_buf]),%%xmm5                               \n" \
  "lea        0x8(%[a_buf]),%[a_buf]                          \n"

// Read 4 UV from NV12, upsample to 8 UV
#define READNV12                                                  \
  "movq       (%[uv_buf]),%%xmm3                              \n" \
  "lea        0x8(%[uv_buf]),%[uv_buf]                        \n" \
  "punpcklwd  %%xmm3,%%xmm3                                   \n" \
  "movq       (%[y_buf]),%%xmm4                               \n" \
  "punpcklbw  %%xmm4,%%xmm4                                   \n" \
  "lea        0x8(%[y_buf]),%[y_buf]                          \n"

// Read 4 VU from NV21, upsample to 8 UV
#define READNV21                                                  \
  "movq       (%[vu_buf]),%%xmm3                              \n" \
  "lea        0x8(%[vu_buf]),%[vu_buf]                        \n" \
  "pshufb     %[kShuffleNV21], %%xmm3                         \n" \
  "movq       (%[y_buf]),%%xmm4                               \n" \
  "punpcklbw  %%xmm4,%%xmm4                                   \n" \
  "lea        0x8(%[y_buf]),%[y_buf]                          \n"

// Read 4 YUY2 with 8 Y and upsample 4 UV to 8 UV.
// xmm6 kShuffleYUY2Y,
// xmm7 kShuffleYUY2UV
#define READYUY2                                                  \
  "movdqu     (%[yuy2_buf]),%%xmm4                            \n" \
  "lea        0x10(%[yuy2_buf]),%[yuy2_buf]                   \n" \
  "movdqa     %%xmm4,%%xmm3                                   \n" \
  "pshufb     %%xmm6,%%xmm4                                   \n" \
  "pshufb     %%xmm7,%%xmm3                                   \n"

// Read 4 UYVY with 8 Y and upsample 4 UV to 8 UV.
// xmm6 kShuffleUYVYY,
// xmm7 kShuffleUYVYUV
#define READUYVY                                                  \
  "movdqu     (%[uyvy_buf]),%%xmm4                            \n" \
  "lea        0x10(%[uyvy_buf]),%[uyvy_buf]                   \n" \
  "movdqa     %%xmm4,%%xmm3                                   \n" \
  "pshufb     %%xmm6,%%xmm4                                   \n" \
  "pshufb     %%xmm7,%%xmm3                                   \n"

// Read 4 UV from P210, upsample to 8 UV
#define READP210                                                  \
  "movdqu     (%[uv_buf]),%%xmm3                              \n" \
  "lea        0x10(%[uv_buf]),%[uv_buf]                       \n" \
  "psrlw      $0x8,%%xmm3                                     \n" \
  "packuswb   %%xmm3,%%xmm3                                   \n" \
  "punpcklwd  %%xmm3,%%xmm3                                   \n" \
  "movdqu     (%[y_buf]),%%xmm4                               \n" \
  "lea        0x10(%[y_buf]),%[y_buf]                         \n"

// Read 8 UV from P410
#define READP410                                                  \
  "movdqu     (%[uv_buf]),%%xmm3                              \n" \
  "movdqu     0x10(%[uv_buf]),%%xmm1                          \n" \
  "lea        0x20(%[uv_buf]),%[uv_buf]                       \n" \
  "psrlw      $0x8,%%xmm3                                     \n" \
  "psrlw      $0x8,%%xmm1                                     \n" \
  "packuswb   %%xmm1,%%xmm3                                   \n" \
  "movdqu     (%[y_buf]),%%xmm4                               \n" \
  "lea        0x10(%[y_buf]),%[y_buf]                         \n"

#if defined(__x86_64__)
#define YUVTORGB_SETUP(yuvconstants)                              \
  "pcmpeqb    %%xmm13,%%xmm13                                 \n" \
  "movdqa     (%[yuvconstants]),%%xmm8                        \n" \
  "pxor       %%xmm12,%%xmm12                                 \n" \
  "movdqa     32(%[yuvconstants]),%%xmm9                      \n" \
  "pavgb      %%xmm12,%%xmm13                                 \n" \
  "movdqa     64(%[yuvconstants]),%%xmm10                     \n" \
  "movdqa     96(%[yuvconstants]),%%xmm11                     \n" \
  "movdqa     128(%[yuvconstants]),%%xmm12                    \n"

#define YUVTORGB_SETUP_AR30(yuvconstants)                         \
  YUVTORGB_SETUP(yuvconstants)                                    \
  "pcmpeqb     %%xmm0,%%xmm0                                  \n" \
  "psrlw       $14,%%xmm0                                     \n" \
  "psllw       $3,%%xmm0                                      \n" \
  "psubw       %%xmm0,%%xmm12                                 \n"

// Convert 8 pixels: 8 UV and 8 Y
#define YUVTORGB16(yuvconstants)                                  \
  "psubb      %%xmm13,%%xmm3                                  \n" \
  "pmulhuw    %%xmm11,%%xmm4                                  \n" \
  "movdqa     %%xmm8,%%xmm0                                   \n" \
  "movdqa     %%xmm9,%%xmm1                                   \n" \
  "movdqa     %%xmm10,%%xmm2                                  \n" \
  "paddw      %%xmm12,%%xmm4                                  \n" \
  "pmaddubsw  %%xmm3,%%xmm0                                   \n" \
  "pmaddubsw  %%xmm3,%%xmm1                                   \n" \
  "pmaddubsw  %%xmm3,%%xmm2                                   \n" \
  "paddsw     %%xmm4,%%xmm0                                   \n" \
  "paddsw     %%xmm4,%%xmm2                                   \n" \
  "psubsw     %%xmm1,%%xmm4                                   \n" \
  "movdqa     %%xmm4,%%xmm1                                   \n"

#define YUVTORGB_REGS "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13",

#else
#define YUVTORGB_SETUP(yuvconstants)
#define YUVTORGB_SETUP_AR30(yuvconstants)

// Convert 8 pixels: 8 UV and 8 Y
#define YUVTORGB16(yuvconstants)                                  \
  "pcmpeqb    %%xmm0,%%xmm0                                   \n" \
  "pabsb      %%xmm0,%%xmm0                                   \n" \
  "psllw      $7,%%xmm0                                       \n" \
  "psubb      %%xmm0,%%xmm3                                   \n" \
  "pmulhuw    96(%[yuvconstants]),%%xmm4                      \n" \
  "movdqa     (%[yuvconstants]),%%xmm0                        \n" \
  "movdqa     32(%[yuvconstants]),%%xmm1                      \n" \
  "movdqa     64(%[yuvconstants]),%%xmm2                      \n" \
  "pmaddubsw  %%xmm3,%%xmm0                                   \n" \
  "pmaddubsw  %%xmm3,%%xmm1                                   \n" \
  "pmaddubsw  %%xmm3,%%xmm2                                   \n" \
  "movdqa     128(%[yuvconstants]),%%xmm3                     \n" \
  "paddw      %%xmm3,%%xmm4                                   \n" \
  "paddsw     %%xmm4,%%xmm0                                   \n" \
  "paddsw     %%xmm4,%%xmm2                                   \n" \
  "psubsw     %%xmm1,%%xmm4                                   \n" \
  "movdqa     %%xmm4,%%xmm1                                   \n"

#define YUVTORGB_REGS
#endif  // defined(__x86_64__)

#define YUVTORGB(yuvconstants)                                    \
  YUVTORGB16(yuvconstants)                                        \
  "psraw      $0x6,%%xmm0                                     \n" \
  "psraw      $0x6,%%xmm1                                     \n" \
  "psraw      $0x6,%%xmm2                                     \n" \
  "packuswb   %%xmm0,%%xmm0                                   \n" \
  "packuswb   %%xmm1,%%xmm1                                   \n" \
  "packuswb   %%xmm2,%%xmm2                                   \n"

// Store 8 ARGB values.
#define STOREARGB                                                  \
  "punpcklbw  %%xmm1,%%xmm0                                    \n" \
  "punpcklbw  %%xmm5,%%xmm2                                    \n" \
  "movdqa     %%xmm0,%%xmm1                                    \n" \
  "punpcklwd  %%xmm2,%%xmm0                                    \n" \
  "punpckhwd  %%xmm2,%%xmm1                                    \n" \
  "movdqu     %%xmm0,(%[dst_argb])                             \n" \
  "movdqu     %%xmm1,0x10(%[dst_argb])                         \n" \
  "lea        0x20(%[dst_argb]), %[dst_argb]                   \n"

// Store 8 RGBA values.
#define STORERGBA                                                  \
  "pcmpeqb   %%xmm5,%%xmm5                                     \n" \
  "punpcklbw %%xmm2,%%xmm1                                     \n" \
  "punpcklbw %%xmm0,%%xmm5                                     \n" \
  "movdqa    %%xmm5,%%xmm0                                     \n" \
  "punpcklwd %%xmm1,%%xmm5                                     \n" \
  "punpckhwd %%xmm1,%%xmm0                                     \n" \
  "movdqu    %%xmm5,(%[dst_rgba])                              \n" \
  "movdqu    %%xmm0,0x10(%[dst_rgba])                          \n" \
  "lea       0x20(%[dst_rgba]),%[dst_rgba]                     \n"

// Store 8 RGB24 values.
#define STORERGB24                                                      \
  "punpcklbw   %%xmm1,%%xmm0                                        \n" \
  "punpcklbw   %%xmm2,%%xmm2                                        \n" \
  "movdqa      %%xmm0,%%xmm1                                        \n" \
  "punpcklwd   %%xmm2,%%xmm0                                        \n" \
  "punpckhwd   %%xmm2,%%xmm1                                        \n" \
  "pshufb      %%xmm5,%%xmm0                                        \n" \
  "pshufb      %%xmm6,%%xmm1                                        \n" \
  "palignr     $0xc,%%xmm0,%%xmm1                                   \n" \
  "movq        %%xmm0,(%[dst_rgb24])                                \n" \
  "movdqu      %%xmm1,0x8(%[dst_rgb24])                             \n" \
  "lea         0x18(%[dst_rgb24]),%[dst_rgb24]                      \n"

// Store 8 AR30 values.
#define STOREAR30                                                  \
  "pminsw     %%xmm7,%%xmm0                                    \n" \
  "pminsw     %%xmm7,%%xmm1                                    \n" \
  "pminsw     %%xmm7,%%xmm2                                    \n" \
  "pmaxsw     %%xmm6,%%xmm0                                    \n" \
  "pmaxsw     %%xmm6,%%xmm1                                    \n" \
  "pmaxsw     %%xmm6,%%xmm2                                    \n" \
  "movdqa     %%xmm1,%%xmm3                                    \n" \
  "pand       %%xmm7,%%xmm3                                    \n" \
  "pand       %%xmm7,%%xmm2                                    \n" \
  "psrlw      $4,%%xmm0                                        \n" \
  "psrlw      $10,%%xmm1                                       \n" \
  "psllw      $6,%%xmm3                                        \n" \
  "por        %%xmm3,%%xmm0                                    \n" \
  "por        %%xmm5,%%xmm2                                    \n" \
  "por        %%xmm1,%%xmm2                                    \n" \
  "movdqa     %%xmm0,%%xmm3                                    \n" \
  "punpcklwd  %%xmm2,%%xmm0                                    \n" \
  "punpckhwd  %%xmm2,%%xmm3                                    \n" \
  "movdqu     %%xmm0,(%[dst_ar30])                             \n" \
  "movdqu     %%xmm3,0x10(%[dst_ar30])                         \n" \
  "lea        0x20(%[dst_ar30]), %[dst_ar30]                   \n"

void OMITFP I444ToARGBRow_SSSE3(const uint8_t* y_buf,
                                const uint8_t* u_buf,
                                const uint8_t* v_buf,
                                uint8_t* dst_argb,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  asm volatile (
    YUVTORGB_SETUP(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "pcmpeqb     %%xmm5,%%xmm5                 \n"

    LABELALIGN
      "1:          \n"
    READYUV444
    YUVTORGB(yuvconstants)
    STOREARGB
      "sub         $0x8,%[width]                 \n"
      "jg          1b                            \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
  );
}

#ifdef HAS_I444ALPHATOARGBROW_SSSE3
void OMITFP I444AlphaToARGBRow_SSSE3(const uint8_t* y_buf,
                                     const uint8_t* u_buf,
                                     const uint8_t* v_buf,
                                     const uint8_t* a_buf,
                                     uint8_t* dst_argb,
                                     const struct YuvConstants* yuvconstants,
                                     int width) {
  asm volatile(YUVTORGB_SETUP(
                   yuvconstants) "sub         %[u_buf],%[v_buf]             \n"

               LABELALIGN "1:          \n" READYUVA444 YUVTORGB(yuvconstants)
                   STOREARGB
               "subl        $0x8,%[width]                 \n"
               "jg          1b                            \n"
               : [y_buf] "+r"(y_buf),        // %[y_buf]
                 [u_buf] "+r"(u_buf),        // %[u_buf]
                 [v_buf] "+r"(v_buf),        // %[v_buf]
                 [a_buf] "+r"(a_buf),        // %[a_buf]
                 [dst_argb] "+r"(dst_argb),  // %[dst_argb]
#if defined(__i386__)
                 [width] "+m"(width)  // %[width]
#else
                 [width] "+rm"(width)  // %[width]
#endif
               : [yuvconstants] "r"(yuvconstants)  // %[yuvconstants]
               : "memory", "cc", YUVTORGB_REGS "xmm0", "xmm1", "xmm2", "xmm3",
                 "xmm4", "xmm5");
}
#endif  // HAS_I444ALPHATOARGBROW_SSSE3

void OMITFP I422ToRGB24Row_SSSE3(const uint8_t* y_buf,
                                 const uint8_t* u_buf,
                                 const uint8_t* v_buf,
                                 uint8_t* dst_rgb24,
                                 const struct YuvConstants* yuvconstants,
                                 int width) {
  asm volatile (
    YUVTORGB_SETUP(yuvconstants)
      "movdqa      %[kShuffleMaskARGBToRGB24_1],%%xmm5 \n"
      "movdqa      %[kShuffleMaskARGBToRGB24_0],%%xmm6 \n"
      "sub         %[u_buf],%[v_buf]             \n"

    LABELALIGN
      "1:          \n"
    READYUV422
    YUVTORGB(yuvconstants)
    STORERGB24
      "subl        $0x8,%[width]                 \n"
      "jg          1b                            \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_rgb24]"+r"(dst_rgb24),  // %[dst_rgb24]
#if defined(__i386__)
    [width]"+m"(width)     // %[width]
#else
    [width]"+rm"(width)    // %[width]
#endif
  : [yuvconstants]"r"(yuvconstants),  // %[yuvconstants]
    [kShuffleMaskARGBToRGB24_0]"m"(kShuffleMaskARGBToRGB24[0]),
    [kShuffleMaskARGBToRGB24_1]"m"(kShuffleMaskARGBToRGB24[1])
  : "memory", "cc", YUVTORGB_REGS
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6"
  );
}

void OMITFP I444ToRGB24Row_SSSE3(const uint8_t* y_buf,
                                 const uint8_t* u_buf,
                                 const uint8_t* v_buf,
                                 uint8_t* dst_rgb24,
                                 const struct YuvConstants* yuvconstants,
                                 int width) {
  asm volatile (
    YUVTORGB_SETUP(yuvconstants)
      "movdqa      %[kShuffleMaskARGBToRGB24_1],%%xmm5 \n"
      "movdqa      %[kShuffleMaskARGBToRGB24_0],%%xmm6 \n"
      "sub         %[u_buf],%[v_buf]             \n"

    LABELALIGN
      "1:          \n"
    READYUV444
    YUVTORGB(yuvconstants)
    STORERGB24
      "subl        $0x8,%[width]                 \n"
      "jg          1b                            \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_rgb24]"+r"(dst_rgb24),  // %[dst_rgb24]
#if defined(__i386__)
    [width]"+m"(width)     // %[width]
#else
    [width]"+rm"(width)    // %[width]
#endif
  : [yuvconstants]"r"(yuvconstants),  // %[yuvconstants]
    [kShuffleMaskARGBToRGB24_0]"m"(kShuffleMaskARGBToRGB24[0]),
    [kShuffleMaskARGBToRGB24_1]"m"(kShuffleMaskARGBToRGB24[1])
  : "memory", "cc", YUVTORGB_REGS
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6"
  );
}

void OMITFP I422ToARGBRow_SSSE3(const uint8_t* y_buf,
                                const uint8_t* u_buf,
                                const uint8_t* v_buf,
                                uint8_t* dst_argb,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  asm volatile (
    YUVTORGB_SETUP(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "pcmpeqb     %%xmm5,%%xmm5                 \n"

    LABELALIGN
      "1:          \n"
    READYUV422
    YUVTORGB(yuvconstants)
    STOREARGB
      "sub         $0x8,%[width]                 \n"
      "jg          1b                            \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
  );
}

void OMITFP I422ToAR30Row_SSSE3(const uint8_t* y_buf,
                                const uint8_t* u_buf,
                                const uint8_t* v_buf,
                                uint8_t* dst_ar30,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  asm volatile (
    YUVTORGB_SETUP_AR30(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "pcmpeqb     %%xmm5,%%xmm5                 \n"  // all 1s
      "movdqa      %%xmm5,%%xmm7                 \n"
      "psrlw       $6,%%xmm7                     \n"
      "psllw       $4,%%xmm7                     \n"  // 1023 * 16 for max
      "psllw       $14,%%xmm5                    \n"  // 0xc000 for 2 alpha bits
      "pxor        %%xmm6,%%xmm6                 \n"  // 0 for min

    LABELALIGN
      "1:          \n"
    READYUV422
    YUVTORGB16(yuvconstants)
    STOREAR30
      "sub         $0x8,%[width]                 \n"
      "jg          1b                            \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_ar30]"+r"(dst_ar30),  // %[dst_ar30]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
  );
}

// 10 bit YUV to ARGB
void OMITFP I210ToARGBRow_SSSE3(const uint16_t* y_buf,
                                const uint16_t* u_buf,
                                const uint16_t* v_buf,
                                uint8_t* dst_argb,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  asm volatile (
    YUVTORGB_SETUP(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "pcmpeqb     %%xmm5,%%xmm5                 \n"

    LABELALIGN
      "1:          \n"
    READYUV210
    YUVTORGB(yuvconstants)
    STOREARGB
      "sub         $0x8,%[width]                 \n"
      "jg          1b                            \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
  );
}

// 12 bit YUV to ARGB
void OMITFP I212ToARGBRow_SSSE3(const uint16_t* y_buf,
                                const uint16_t* u_buf,
                                const uint16_t* v_buf,
                                uint8_t* dst_argb,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  asm volatile (
    YUVTORGB_SETUP(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "pcmpeqb     %%xmm5,%%xmm5                 \n"

    LABELALIGN
      "1:          \n"
    READYUV212
    YUVTORGB(yuvconstants)
    STOREARGB
      "sub         $0x8,%[width]                 \n"
      "jg          1b                            \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
  );
}

// 10 bit YUV to AR30
void OMITFP I210ToAR30Row_SSSE3(const uint16_t* y_buf,
                                const uint16_t* u_buf,
                                const uint16_t* v_buf,
                                uint8_t* dst_ar30,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  asm volatile (
    YUVTORGB_SETUP(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "pcmpeqb     %%xmm5,%%xmm5                 \n"  // all 1s
      "movdqa      %%xmm5,%%xmm7                 \n"
      "psrlw       $6,%%xmm7                     \n"
      "psllw       $4,%%xmm7                     \n"  // 1023 * 16 for max
      "psllw       $14,%%xmm5                    \n"  // 0xc000 for 2 alpha bits
      "pxor        %%xmm6,%%xmm6                 \n"  // 0 for min

    LABELALIGN
      "1:          \n"
    READYUV210
    YUVTORGB16(yuvconstants)
    STOREAR30
      "sub         $0x8,%[width]                 \n"
      "jg          1b                            \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_ar30]"+r"(dst_ar30),  // %[dst_ar30]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
  );
}

// 12 bit YUV to AR30
void OMITFP I212ToAR30Row_SSSE3(const uint16_t* y_buf,
                                const uint16_t* u_buf,
                                const uint16_t* v_buf,
                                uint8_t* dst_ar30,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  asm volatile (
    YUVTORGB_SETUP(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "pcmpeqb     %%xmm5,%%xmm5                 \n"  // all 1s
      "movdqa      %%xmm5,%%xmm7                 \n"
      "psrlw       $6,%%xmm7                     \n"
      "psllw       $4,%%xmm7                     \n"  // 1023 * 16 for max
      "psllw       $14,%%xmm5                    \n"  // 0xc000 for 2 alpha bits
      "pxor        %%xmm6,%%xmm6                 \n"  // 0 for min

    LABELALIGN
      "1:          \n"
    READYUV212
    YUVTORGB16(yuvconstants)
    STOREAR30
      "sub         $0x8,%[width]                 \n"
      "jg          1b                            \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_ar30]"+r"(dst_ar30),  // %[dst_ar30]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
  );
}

// 10 bit YUV to ARGB
void OMITFP I410ToARGBRow_SSSE3(const uint16_t* y_buf,
                                const uint16_t* u_buf,
                                const uint16_t* v_buf,
                                uint8_t* dst_argb,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  asm volatile (
    YUVTORGB_SETUP(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "pcmpeqb     %%xmm5,%%xmm5                 \n"

    LABELALIGN
      "1:          \n"
    READYUV410
    YUVTORGB(yuvconstants)
    STOREARGB
      "sub         $0x8,%[width]                 \n"
      "jg          1b                            \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
  );
}

#ifdef HAS_I210ALPHATOARGBROW_SSSE3
// 10 bit YUVA to ARGB
void OMITFP I210AlphaToARGBRow_SSSE3(const uint16_t* y_buf,
                                     const uint16_t* u_buf,
                                     const uint16_t* v_buf,
                                     const uint16_t* a_buf,
                                     uint8_t* dst_argb,
                                     const struct YuvConstants* yuvconstants,
                                     int width) {
  asm volatile(YUVTORGB_SETUP(
                   yuvconstants) "sub         %[u_buf],%[v_buf]             \n"

               LABELALIGN "1:          \n" READYUVA210 YUVTORGB(yuvconstants)
                   STOREARGB
               "subl        $0x8,%[width]                 \n"
               "jg          1b                            \n"
               : [y_buf] "+r"(y_buf),  // %[y_buf]
                 [u_buf] "+r"(u_buf),  // %[u_buf]
                 [v_buf] "+r"(v_buf),  // %[v_buf]
                 [a_buf] "+r"(a_buf),
                 [dst_argb] "+r"(dst_argb),  // %[dst_argb]
#if defined(__i386__)
                 [width] "+m"(width)  // %[width]
#else
                 [width] "+rm"(width)  // %[width]
#endif
               : [yuvconstants] "r"(yuvconstants)  // %[yuvconstants]
               : "memory", "cc", YUVTORGB_REGS "xmm0", "xmm1", "xmm2", "xmm3",
                 "xmm4", "xmm5");
}
#endif  // HAS_I210ALPHATOARGBROW_SSSE3

#ifdef HAS_I410ALPHATOARGBROW_SSSE3
// 10 bit YUVA to ARGB
void OMITFP I410AlphaToARGBRow_SSSE3(const uint16_t* y_buf,
                                     const uint16_t* u_buf,
                                     const uint16_t* v_buf,
                                     const uint16_t* a_buf,
                                     uint8_t* dst_argb,
                                     const struct YuvConstants* yuvconstants,
                                     int width) {
  asm volatile(YUVTORGB_SETUP(
                   yuvconstants) "sub         %[u_buf],%[v_buf]             \n"

               LABELALIGN "1:          \n" READYUVA410 YUVTORGB(yuvconstants)
                   STOREARGB
               "subl        $0x8,%[width]                 \n"
               "jg          1b                            \n"
               : [y_buf] "+r"(y_buf),  // %[y_buf]
                 [u_buf] "+r"(u_buf),  // %[u_buf]
                 [v_buf] "+r"(v_buf),  // %[v_buf]
                 [a_buf] "+r"(a_buf),
                 [dst_argb] "+r"(dst_argb),  // %[dst_argb]
#if defined(__i386__)
                 [width] "+m"(width)  // %[width]
#else
                 [width] "+rm"(width)  // %[width]
#endif
               : [yuvconstants] "r"(yuvconstants)  // %[yuvconstants]
               : "memory", "cc", YUVTORGB_REGS "xmm0", "xmm1", "xmm2", "xmm3",
                 "xmm4", "xmm5");
}
#endif  // HAS_I410ALPHATOARGBROW_SSSE3

// 10 bit YUV to AR30
void OMITFP I410ToAR30Row_SSSE3(const uint16_t* y_buf,
                                const uint16_t* u_buf,
                                const uint16_t* v_buf,
                                uint8_t* dst_ar30,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  asm volatile (
    YUVTORGB_SETUP(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "pcmpeqb     %%xmm5,%%xmm5                 \n"  // all 1s
      "movdqa      %%xmm5,%%xmm7                 \n"
      "psrlw       $6,%%xmm7                     \n"
      "psllw       $4,%%xmm7                     \n"  // 1023 * 16 for max
      "psllw       $14,%%xmm5                    \n"  // 0xc000 for 2 alpha bits
      "pxor        %%xmm6,%%xmm6                 \n"  // 0 for min

    LABELALIGN
      "1:          \n"
    READYUV410
    YUVTORGB16(yuvconstants)
    STOREAR30
      "sub         $0x8,%[width]                 \n"
      "jg          1b                            \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_ar30]"+r"(dst_ar30),  // %[dst_ar30]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)   // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS
      "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
  );
}

#ifdef HAS_I422ALPHATOARGBROW_SSSE3
void OMITFP I422AlphaToARGBRow_SSSE3(const uint8_t* y_buf,
                                     const uint8_t* u_buf,
                                     const uint8_t* v_buf,
                                     const uint8_t* a_buf,
                                     uint8_t* dst_argb,
                                     const struct YuvConstants* yuvconstants,
                                     int width) {
  asm volatile(YUVTORGB_SETUP(
                   yuvconstants) "sub         %[u_buf],%[v_buf]             \n"

               LABELALIGN "1:          \n" READYUVA422 YUVTORGB(yuvconstants)
                   STOREARGB
               "subl        $0x8,%[width]                 \n"
               "jg          1b                            \n"
               : [y_buf] "+r"(y_buf),        // %[y_buf]
                 [u_buf] "+r"(u_buf),        // %[u_buf]
                 [v_buf] "+r"(v_buf),        // %[v_buf]
                 [a_buf] "+r"(a_buf),        // %[a_buf]
                 [dst_argb] "+r"(dst_argb),  // %[dst_argb]
#if defined(__i386__)
                 [width] "+m"(width)  // %[width]
#else
                 [width] "+rm"(width)  // %[width]
#endif
               : [yuvconstants] "r"(yuvconstants)  // %[yuvconstants]
               : "memory", "cc", YUVTORGB_REGS "xmm0", "xmm1", "xmm2", "xmm3",
                 "xmm4", "xmm5");
}
#endif  // HAS_I422ALPHATOARGBROW_SSSE3

void OMITFP NV12ToARGBRow_SSSE3(const uint8_t* y_buf,
                                const uint8_t* uv_buf,
                                uint8_t* dst_argb,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  asm volatile(YUVTORGB_SETUP(
                   yuvconstants) "pcmpeqb     %%xmm5,%%xmm5                 \n"

               LABELALIGN "1:          \n" READNV12 YUVTORGB(yuvconstants)
                   STOREARGB
               "sub         $0x8,%[width]                 \n"
               "jg          1b                            \n"
               : [y_buf] "+r"(y_buf),              // %[y_buf]
                 [uv_buf] "+r"(uv_buf),            // %[uv_buf]
                 [dst_argb] "+r"(dst_argb),        // %[dst_argb]
                 [width] "+rm"(width)              // %[width]
               : [yuvconstants] "r"(yuvconstants)  // %[yuvconstants]
               : "memory", "cc", YUVTORGB_REGS "xmm0", "xmm1", "xmm2", "xmm3",
                 "xmm4", "xmm5");
}

void OMITFP NV21ToARGBRow_SSSE3(const uint8_t* y_buf,
                                const uint8_t* vu_buf,
                                uint8_t* dst_argb,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  asm volatile(YUVTORGB_SETUP(
                   yuvconstants) "pcmpeqb     %%xmm5,%%xmm5                 \n"

               LABELALIGN "1:          \n" READNV21 YUVTORGB(yuvconstants)
                   STOREARGB
               "sub         $0x8,%[width]                 \n"
               "jg          1b                            \n"
               : [y_buf] "+r"(y_buf),               // %[y_buf]
                 [vu_buf] "+r"(vu_buf),             // %[vu_buf]
                 [dst_argb] "+r"(dst_argb),         // %[dst_argb]
                 [width] "+rm"(width)               // %[width]
               : [yuvconstants] "r"(yuvconstants),  // %[yuvconstants]
                 [kShuffleNV21] "m"(kShuffleNV21)
               : "memory", "cc", YUVTORGB_REGS "xmm0", "xmm1", "xmm2", "xmm3",
                 "xmm4", "xmm5");
}

void OMITFP YUY2ToARGBRow_SSSE3(const uint8_t* yuy2_buf,
                                uint8_t* dst_argb,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  asm volatile(
      "movdqa      %[kShuffleYUY2Y],%%xmm6       \n"
      "movdqa      %[kShuffleYUY2UV],%%xmm7      \n" YUVTORGB_SETUP(
          yuvconstants) "pcmpeqb     %%xmm5,%%xmm5                 \n"

      LABELALIGN "1:          \n" READYUY2 YUVTORGB(yuvconstants) STOREARGB
      "sub         $0x8,%[width]                 \n"
      "jg          1b                            \n"
      : [yuy2_buf] "+r"(yuy2_buf),         // %[yuy2_buf]
        [dst_argb] "+r"(dst_argb),         // %[dst_argb]
        [width] "+rm"(width)               // %[width]
      : [yuvconstants] "r"(yuvconstants),  // %[yuvconstants]
        [kShuffleYUY2Y] "m"(kShuffleYUY2Y), [kShuffleYUY2UV] "m"(kShuffleYUY2UV)
      : "memory", "cc", YUVTORGB_REGS "xmm0", "xmm1", "xmm2", "xmm3", "xmm4",
        "xmm5", "xmm6", "xmm7");
}

void OMITFP UYVYToARGBRow_SSSE3(const uint8_t* uyvy_buf,
                                uint8_t* dst_argb,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  asm volatile(
      "movdqa      %[kShuffleUYVYY],%%xmm6       \n"
      "movdqa      %[kShuffleUYVYUV],%%xmm7      \n" YUVTORGB_SETUP(
          yuvconstants) "pcmpeqb     %%xmm5,%%xmm5                 \n"

      LABELALIGN "1:          \n" READUYVY YUVTORGB(yuvconstants) STOREARGB
      "sub         $0x8,%[width]                 \n"
      "jg          1b                            \n"
      : [uyvy_buf] "+r"(uyvy_buf),         // %[uyvy_buf]
        [dst_argb] "+r"(dst_argb),         // %[dst_argb]
        [width] "+rm"(width)               // %[width]
      : [yuvconstants] "r"(yuvconstants),  // %[yuvconstants]
        [kShuffleUYVYY] "m"(kShuffleUYVYY), [kShuffleUYVYUV] "m"(kShuffleUYVYUV)
      : "memory", "cc", YUVTORGB_REGS "xmm0", "xmm1", "xmm2", "xmm3", "xmm4",
        "xmm5", "xmm6", "xmm7");
}

void OMITFP P210ToARGBRow_SSSE3(const uint16_t* y_buf,
                                const uint16_t* uv_buf,
                                uint8_t* dst_argb,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  asm volatile(YUVTORGB_SETUP(
                   yuvconstants) "pcmpeqb     %%xmm5,%%xmm5                 \n"

               LABELALIGN "1:          \n" READP210 YUVTORGB(yuvconstants)
                   STOREARGB
               "sub         $0x8,%[width]                 \n"
               "jg          1b                            \n"
               : [y_buf] "+r"(y_buf),              // %[y_buf]
                 [uv_buf] "+r"(uv_buf),            // %[u_buf]
                 [dst_argb] "+r"(dst_argb),        // %[dst_argb]
                 [width] "+rm"(width)              // %[width]
               : [yuvconstants] "r"(yuvconstants)  // %[yuvconstants]
               : "memory", "cc", YUVTORGB_REGS "xmm0", "xmm1", "xmm2", "xmm3",
                 "xmm4", "xmm5");
}

void OMITFP P410ToARGBRow_SSSE3(const uint16_t* y_buf,
                                const uint16_t* uv_buf,
                                uint8_t* dst_argb,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  asm volatile(YUVTORGB_SETUP(
                   yuvconstants) "pcmpeqb     %%xmm5,%%xmm5                 \n"

               LABELALIGN "1:          \n" READP410 YUVTORGB(yuvconstants)
                   STOREARGB
               "sub         $0x8,%[width]                 \n"
               "jg          1b                            \n"
               : [y_buf] "+r"(y_buf),              // %[y_buf]
                 [uv_buf] "+r"(uv_buf),            // %[u_buf]
                 [dst_argb] "+r"(dst_argb),        // %[dst_argb]
                 [width] "+rm"(width)              // %[width]
               : [yuvconstants] "r"(yuvconstants)  // %[yuvconstants]
               : "memory", "cc", YUVTORGB_REGS "xmm0", "xmm1", "xmm2", "xmm3",
                 "xmm4", "xmm5");
}

void OMITFP P210ToAR30Row_SSSE3(const uint16_t* y_buf,
                                const uint16_t* uv_buf,
                                uint8_t* dst_ar30,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  asm volatile (
    YUVTORGB_SETUP(yuvconstants)
      "pcmpeqb     %%xmm5,%%xmm5                 \n"  // all 1s
      "movdqa      %%xmm5,%%xmm7                 \n"
      "psrlw       $6,%%xmm7                     \n"
      "psllw       $4,%%xmm7                     \n"  // 1023 * 16 for max
      "psllw       $14,%%xmm5                    \n"  // 0xc000 for 2 alpha bits
      "pxor        %%xmm6,%%xmm6                 \n"  // 0 for min

    LABELALIGN
      "1:          \n"
    READP210
    YUVTORGB16(yuvconstants)
    STOREAR30
      "sub         $0x8,%[width]                 \n"
      "jg          1b                            \n"
  : [y_buf]"+r"(y_buf),              // %[y_buf]
    [uv_buf]"+r"(uv_buf),            // %[uv_buf]
    [dst_ar30]"+r"(dst_ar30),        // %[dst_ar30]
    [width]"+rm"(width)              // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS
      "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
  );
}

void OMITFP P410ToAR30Row_SSSE3(const uint16_t* y_buf,
                                const uint16_t* uv_buf,
                                uint8_t* dst_ar30,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  asm volatile (
    YUVTORGB_SETUP(yuvconstants)
      "pcmpeqb     %%xmm5,%%xmm5                 \n"  // all 1s
      "movdqa      %%xmm5,%%xmm7                 \n"
      "psrlw       $6,%%xmm7                     \n"
      "psllw       $4,%%xmm7                     \n"  // 1023 * 16 for max
      "psllw       $14,%%xmm5                    \n"  // 0xc000 for 2 alpha bits
      "pxor        %%xmm6,%%xmm6                 \n"  // 0 for min

    LABELALIGN
      "1:          \n"
    READP410
    YUVTORGB16(yuvconstants)
    STOREAR30
      "sub         $0x8,%[width]                 \n"
      "jg          1b                            \n"
  : [y_buf]"+r"(y_buf),              // %[y_buf]
    [uv_buf]"+r"(uv_buf),            // %[uv_buf]
    [dst_ar30]"+r"(dst_ar30),        // %[dst_ar30]
    [width]"+rm"(width)              // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS
      "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
  );
}

void OMITFP I422ToRGBARow_SSSE3(const uint8_t* y_buf,
                                const uint8_t* u_buf,
                                const uint8_t* v_buf,
                                uint8_t* dst_rgba,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  asm volatile (
    YUVTORGB_SETUP(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "pcmpeqb     %%xmm5,%%xmm5                 \n"

    LABELALIGN
      "1:          \n"
    READYUV422
    YUVTORGB(yuvconstants)
    STORERGBA
      "sub         $0x8,%[width]                 \n"
      "jg          1b                            \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_rgba]"+r"(dst_rgba),  // %[dst_rgba]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
  );
}

#endif  // defined(HAS_I422TOARGBROW_SSSE3) || defined(HAS_I422TOARGBROW_AVX2)

// Read 16 UV from 444
#define READYUV444_AVX2                                               \
  "vmovdqu    (%[u_buf]),%%xmm3                                   \n" \
  "vmovdqu    0x00(%[u_buf],%[v_buf],1),%%xmm1                    \n" \
  "lea        0x10(%[u_buf]),%[u_buf]                             \n" \
  "vpermq     $0xd8,%%ymm3,%%ymm3                                 \n" \
  "vpermq     $0xd8,%%ymm1,%%ymm1                                 \n" \
  "vpunpcklbw %%ymm1,%%ymm3,%%ymm3                                \n" \
  "vmovdqu    (%[y_buf]),%%xmm4                                   \n" \
  "vpermq     $0xd8,%%ymm4,%%ymm4                                 \n" \
  "vpunpcklbw %%ymm4,%%ymm4,%%ymm4                                \n" \
  "lea        0x10(%[y_buf]),%[y_buf]                             \n"

// Read 8 UV from 422, upsample to 16 UV.
#define READYUV422_AVX2                                               \
  "vmovq      (%[u_buf]),%%xmm3                                   \n" \
  "vmovq      0x00(%[u_buf],%[v_buf],1),%%xmm1                    \n" \
  "lea        0x8(%[u_buf]),%[u_buf]                              \n" \
  "vpunpcklbw %%ymm1,%%ymm3,%%ymm3                                \n" \
  "vpermq     $0xd8,%%ymm3,%%ymm3                                 \n" \
  "vpunpcklwd %%ymm3,%%ymm3,%%ymm3                                \n" \
  "vmovdqu    (%[y_buf]),%%xmm4                                   \n" \
  "vpermq     $0xd8,%%ymm4,%%ymm4                                 \n" \
  "vpunpcklbw %%ymm4,%%ymm4,%%ymm4                                \n" \
  "lea        0x10(%[y_buf]),%[y_buf]                             \n"

#define READYUV422_AVX512BW                                           \
  "vmovdqu    (%[u_buf]),%%xmm3                                   \n" \
  "vmovdqu    0x00(%[u_buf],%[v_buf],1),%%xmm1                    \n" \
  "lea        0x10(%[u_buf]),%[u_buf]                             \n" \
  "vpunpcklbw %%xmm1,%%xmm3,%%xmm2                                \n" \
  "vpunpckhbw %%xmm1,%%xmm3,%%xmm3                                \n" \
  "vinserti64x2 $1,%%xmm3,%%ymm2,%%ymm3                           \n" \
  "vmovdqa64  %%zmm16,%%zmm1                                      \n" \
  "vpermi2w   %%zmm3,%%zmm3,%%zmm1                                \n" \
  "vpmovzxbw  (%[y_buf]),%%zmm4                                   \n" \
  "lea        0x20(%[y_buf]),%[y_buf]                             \n" \
  "vpsllw     $8,%%zmm4,%%zmm3                                    \n" \
  "vpord      %%zmm3,%%zmm4,%%zmm4                                \n" \
  "vmovdqa64  %%zmm1,%%zmm3                                       \n"

#define READYUV422_MASK_AVX512BW                                      \
  "mov         $-1,%[temp]                                        \n" \
  "bzhi        %q[width],%[temp],%[temp]                          \n" \
  "kmovd       %k[temp],%%k1                                      \n" \
  "vpmovzxbw   (%[y_buf]),%%zmm4%{%%k1%}%{z%}                     \n" \
  "mov         $-1,%[temp]                                        \n" \
  "lea         1(%q[width]),%k[y_buf]                             \n" \
  "shr         $1,%k[y_buf]                                       \n" \
  "bzhi        %q[y_buf],%[temp],%[y_buf]                         \n" \
  "kmovw       %k[y_buf],%%k2                                     \n" \
  "vmovdqu8    (%[u_buf]),%%xmm3%{%%k2%}%{z%}                     \n" \
  "vmovdqu8    0x00(%[u_buf],%[v_buf],1),%%xmm1%{%%k2%}%{z%}      \n" \
  "vpunpcklbw  %%xmm1,%%xmm3,%%xmm2                               \n" \
  "vpunpckhbw  %%xmm1,%%xmm3,%%xmm3                               \n" \
  "vinserti64x2 $1,%%xmm3,%%ymm2,%%ymm3                           \n" \
  "vmovdqa64   %%zmm16,%%zmm1                                     \n" \
  "vpermi2w    %%zmm3,%%zmm3,%%zmm1                               \n" \
  "vpsllw      $8,%%zmm4,%%zmm3                                   \n" \
  "vpord       %%zmm3,%%zmm4,%%zmm4                               \n" \
  "vmovdqa64   %%zmm1,%%zmm3                                      \n"

// Read 8 UV from 210, upsample to 16 UV
// TODO(fbarchard): Consider vpshufb to replace pack/unpack
// TODO(fbarchard): Consider vunpcklpd to combine the 2 registers into 1.
#define READYUV210_AVX2                                            \
  "vmovdqu    (%[u_buf]),%%xmm3                                \n" \
  "vmovdqu    0x00(%[u_buf],%[v_buf],1),%%xmm1                 \n" \
  "lea        0x10(%[u_buf]),%[u_buf]                          \n" \
  "vpermq     $0xd8,%%ymm3,%%ymm3                              \n" \
  "vpermq     $0xd8,%%ymm1,%%ymm1                              \n" \
  "vpunpcklwd %%ymm1,%%ymm3,%%ymm3                             \n" \
  "vpsraw     $2,%%ymm3,%%ymm3                                 \n" \
  "vpackuswb  %%ymm3,%%ymm3,%%ymm3                             \n" \
  "vpunpcklwd %%ymm3,%%ymm3,%%ymm3                             \n" \
  "vmovdqu    (%[y_buf]),%%ymm4                                \n" \
  "vpsllw     $6,%%ymm4,%%ymm2                                 \n" \
  "vpsrlw     $4,%%ymm4,%%ymm4                                 \n" \
  "vpaddw     %%ymm2,%%ymm4,%%ymm4                             \n" \
  "lea        0x20(%[y_buf]),%[y_buf]                          \n"

// Read 8 UV from 210, upsample to 16 UV. With 16 Alpha.
#define READYUVA210_AVX2                                           \
  "vmovdqu    (%[u_buf]),%%xmm3                                \n" \
  "vmovdqu    0x00(%[u_buf],%[v_buf],1),%%xmm1                 \n" \
  "lea        0x10(%[u_buf]),%[u_buf]                          \n" \
  "vpermq     $0xd8,%%ymm3,%%ymm3                              \n" \
  "vpermq     $0xd8,%%ymm1,%%ymm1                              \n" \
  "vpunpcklwd %%ymm1,%%ymm3,%%ymm3                             \n" \
  "vpsraw     $2,%%ymm3,%%ymm3                                 \n" \
  "vpackuswb  %%ymm3,%%ymm3,%%ymm3                             \n" \
  "vpunpcklwd %%ymm3,%%ymm3,%%ymm3                             \n" \
  "vmovdqu    (%[y_buf]),%%ymm4                                \n" \
  "vpsllw     $6,%%ymm4,%%ymm2                                 \n" \
  "vpsrlw     $4,%%ymm4,%%ymm4                                 \n" \
  "vpaddw     %%ymm2,%%ymm4,%%ymm4                             \n" \
  "lea        0x20(%[y_buf]),%[y_buf]                          \n" \
  "vmovdqu    (%[a_buf]),%%ymm5                                \n" \
  "vpsraw     $2,%%ymm5,%%ymm5                                 \n" \
  "vpackuswb  %%ymm5,%%ymm5,%%ymm5                             \n" \
  "lea        0x20(%[a_buf]),%[a_buf]                          \n"

// Read 16 UV from 410
#define READYUV410_AVX2                                            \
  "vmovdqu    (%[u_buf]),%%ymm3                                \n" \
  "vmovdqu    0x00(%[u_buf],%[v_buf],1),%%ymm2                 \n" \
  "lea        0x20(%[u_buf]),%[u_buf]                          \n" \
  "vpsraw     $2,%%ymm3,%%ymm3                                 \n" \
  "vpsraw     $2,%%ymm2,%%ymm2                                 \n" \
  "vpunpckhwd %%ymm2,%%ymm3,%%ymm1                             \n" \
  "vpunpcklwd %%ymm2,%%ymm3,%%ymm3                             \n" \
  "vpackuswb  %%ymm1,%%ymm3,%%ymm3                             \n" \
  "vmovdqu    (%[y_buf]),%%ymm4                                \n" \
  "vpsllw     $6,%%ymm4,%%ymm2                                 \n" \
  "vpsrlw     $4,%%ymm4,%%ymm4                                 \n" \
  "vpaddw     %%ymm2,%%ymm4,%%ymm4                             \n" \
  "lea        0x20(%[y_buf]),%[y_buf]                          \n"

// Read 8 UV from 212 12 bit, upsample to 16 UV
#define READYUV212_AVX2                                            \
  "vmovdqu    (%[u_buf]),%%xmm3                                \n" \
  "vmovdqu    0x00(%[u_buf],%[v_buf],1),%%xmm1                 \n" \
  "lea        0x10(%[u_buf]),%[u_buf]                          \n" \
  "vpermq     $0xd8,%%ymm3,%%ymm3                              \n" \
  "vpermq     $0xd8,%%ymm1,%%ymm1                              \n" \
  "vpunpcklwd %%ymm1,%%ymm3,%%ymm3                             \n" \
  "vpsraw     $0x4,%%ymm3,%%ymm3                               \n" \
  "vpackuswb  %%ymm3,%%ymm3,%%ymm3                             \n" \
  "vpunpcklwd %%ymm3,%%ymm3,%%ymm3                             \n" \
  "vmovdqu    (%[y_buf]),%%ymm4                                \n" \
  "vpsllw     $4,%%ymm4,%%ymm2                                 \n" \
  "vpsrlw     $8,%%ymm4,%%ymm4                                 \n" \
  "vpaddw     %%ymm2,%%ymm4,%%ymm4                             \n" \
  "lea        0x20(%[y_buf]),%[y_buf]                          \n"

// Read 16 UV from 410. With 16 Alpha.
#define READYUVA410_AVX2                                           \
  "vmovdqu    (%[u_buf]),%%ymm3                                \n" \
  "vmovdqu    0x00(%[u_buf],%[v_buf],1),%%ymm2                 \n" \
  "lea        0x20(%[u_buf]),%[u_buf]                          \n" \
  "vpsraw     $2,%%ymm3,%%ymm3                                 \n" \
  "vpsraw     $2,%%ymm2,%%ymm2                                 \n" \
  "vpunpckhwd %%ymm2,%%ymm3,%%ymm1                             \n" \
  "vpunpcklwd %%ymm2,%%ymm3,%%ymm3                             \n" \
  "vpackuswb  %%ymm1,%%ymm3,%%ymm3                             \n" \
  "vmovdqu    (%[y_buf]),%%ymm4                                \n" \
  "vpsllw     $6,%%ymm4,%%ymm2                                 \n" \
  "vpsrlw     $4,%%ymm4,%%ymm4                                 \n" \
  "vpaddw     %%ymm2,%%ymm4,%%ymm4                             \n" \
  "lea        0x20(%[y_buf]),%[y_buf]                          \n" \
  "vmovdqu    (%[a_buf]),%%ymm5                                \n" \
  "vpsraw     $2,%%ymm5,%%ymm5                                 \n" \
  "vpackuswb  %%ymm5,%%ymm5,%%ymm5                             \n" \
  "lea        0x20(%[a_buf]),%[a_buf]                          \n"

// Read 16 UV from 444.  With 16 Alpha.
#define READYUVA444_AVX2                                              \
  "vmovdqu    (%[u_buf]),%%xmm3                                   \n" \
  "vmovdqu    0x00(%[u_buf],%[v_buf],1),%%xmm1                    \n" \
  "lea        0x10(%[u_buf]),%[u_buf]                             \n" \
  "vpermq     $0xd8,%%ymm3,%%ymm3                                 \n" \
  "vpermq     $0xd8,%%ymm1,%%ymm1                                 \n" \
  "vpunpcklbw %%ymm1,%%ymm3,%%ymm3                                \n" \
  "vmovdqu    (%[y_buf]),%%xmm4                                   \n" \
  "vpermq     $0xd8,%%ymm4,%%ymm4                                 \n" \
  "vpunpcklbw %%ymm4,%%ymm4,%%ymm4                                \n" \
  "lea        0x10(%[y_buf]),%[y_buf]                             \n" \
  "vmovdqu    (%[a_buf]),%%xmm5                                   \n" \
  "vpermq     $0xd8,%%ymm5,%%ymm5                                 \n" \
  "lea        0x10(%[a_buf]),%[a_buf]                             \n"

// Read 8 UV from 422, upsample to 16 UV.  With 16 Alpha.
#define READYUVA422_AVX2                                              \
  "vmovq      (%[u_buf]),%%xmm3                                   \n" \
  "vmovq      0x00(%[u_buf],%[v_buf],1),%%xmm1                    \n" \
  "lea        0x8(%[u_buf]),%[u_buf]                              \n" \
  "vpunpcklbw %%ymm1,%%ymm3,%%ymm3                                \n" \
  "vpermq     $0xd8,%%ymm3,%%ymm3                                 \n" \
  "vpunpcklwd %%ymm3,%%ymm3,%%ymm3                                \n" \
  "vmovdqu    (%[y_buf]),%%xmm4                                   \n" \
  "vpermq     $0xd8,%%ymm4,%%ymm4                                 \n" \
  "vpunpcklbw %%ymm4,%%ymm4,%%ymm4                                \n" \
  "lea        0x10(%[y_buf]),%[y_buf]                             \n" \
  "vmovdqu    (%[a_buf]),%%xmm5                                   \n" \
  "vpermq     $0xd8,%%ymm5,%%ymm5                                 \n" \
  "lea        0x10(%[a_buf]),%[a_buf]                             \n"

// Read 8 UV from NV12, upsample to 16 UV.
#define READNV12_AVX2                                                 \
  "vmovdqu    (%[uv_buf]),%%xmm3                                  \n" \
  "lea        0x10(%[uv_buf]),%[uv_buf]                           \n" \
  "vpermq     $0xd8,%%ymm3,%%ymm3                                 \n" \
  "vpunpcklwd %%ymm3,%%ymm3,%%ymm3                                \n" \
  "vmovdqu    (%[y_buf]),%%xmm4                                   \n" \
  "vpermq     $0xd8,%%ymm4,%%ymm4                                 \n" \
  "vpunpcklbw %%ymm4,%%ymm4,%%ymm4                                \n" \
  "lea        0x10(%[y_buf]),%[y_buf]                             \n"

// Read 8 VU from NV21, upsample to 16 UV.
#define READNV21_AVX2                                                 \
  "vmovdqu    (%[vu_buf]),%%xmm3                                  \n" \
  "lea        0x10(%[vu_buf]),%[vu_buf]                           \n" \
  "vpermq     $0xd8,%%ymm3,%%ymm3                                 \n" \
  "vpshufb     %[kShuffleNV21], %%ymm3, %%ymm3                    \n" \
  "vmovdqu    (%[y_buf]),%%xmm4                                   \n" \
  "vpermq     $0xd8,%%ymm4,%%ymm4                                 \n" \
  "vpunpcklbw %%ymm4,%%ymm4,%%ymm4                                \n" \
  "lea        0x10(%[y_buf]),%[y_buf]                             \n"

// Read 4 UV from P210, upsample to 8 UV
#define READP210_AVX2                                                 \
  "vmovdqu    (%[uv_buf]),%%ymm3                                  \n" \
  "lea        0x20(%[uv_buf]),%[uv_buf]                           \n" \
  "vpsrlw     $0x8,%%ymm3,%%ymm3                                  \n" \
  "vpackuswb  %%ymm3,%%ymm3,%%ymm3                                \n" \
  "vpunpcklwd %%ymm3,%%ymm3,%%ymm3                                \n" \
  "vmovdqu    (%[y_buf]),%%ymm4                                   \n" \
  "lea        0x20(%[y_buf]),%[y_buf]                             \n"

// Read 8 UV from P410
#define READP410_AVX2                                                 \
  "vmovdqu    (%[uv_buf]),%%ymm3                                  \n" \
  "vmovdqu    0x20(%[uv_buf]),%%ymm1                              \n" \
  "lea        0x40(%[uv_buf]),%[uv_buf]                           \n" \
  "vpsrlw     $0x8,%%ymm3,%%ymm3                                  \n" \
  "vpsrlw     $0x8,%%ymm1,%%ymm1                                  \n" \
  "vpackuswb  %%ymm1,%%ymm3,%%ymm3                                \n" \
  "vpermq     $0xd8,%%ymm3,%%ymm3                                 \n" \
  "vmovdqu    (%[y_buf]),%%ymm4                                   \n" \
  "lea        0x20(%[y_buf]),%[y_buf]                             \n"

// Read 8 YUY2 with 16 Y and upsample 8 UV to 16 UV.
// ymm6 kShuffleYUY2Y,
// ymm7 kShuffleYUY2UV
#define READYUY2_AVX2                                                 \
  "vmovdqu    (%[yuy2_buf]),%%ymm1                                \n" \
  "vpshufb    %%ymm6,%%ymm1,%%ymm4                                \n" \
  "vpshufb    %%ymm7,%%ymm1,%%ymm3                                \n" \
  "lea        0x20(%[yuy2_buf]),%[yuy2_buf]                       \n"

// Read 8 UYVY with 16 Y and upsample 8 UV to 16 UV.
// ymm6 kShuffleUYVYY,
// ymm7 kShuffleUYVYUV
#define READUYVY_AVX2                                                 \
  "vmovdqu    (%[uyvy_buf]),%%ymm1                                \n" \
  "vpshufb    %%ymm6,%%ymm1,%%ymm4                                \n" \
  "vpshufb    %%ymm7,%%ymm1,%%ymm3                                \n" \
  "lea        0x20(%[uyvy_buf]),%[uyvy_buf]                       \n"

#if defined(__x86_64__)
#define YUVTORGB_SETUP_AVX2(yuvconstants)                             \
  "vpcmpeqb    %%ymm13,%%ymm13,%%ymm13                            \n" \
  "vmovdqa     (%[yuvconstants]),%%ymm8                           \n" \
  "vpabsb      %%ymm13,%%ymm13                                    \n" \
  "vmovdqa     32(%[yuvconstants]),%%ymm9                         \n" \
  "vpsllw      $7,%%ymm13,%%ymm13                                 \n" \
  "vmovdqa     64(%[yuvconstants]),%%ymm10                        \n" \
  "vmovdqa     96(%[yuvconstants]),%%ymm11                        \n" \
  "vmovdqa     128(%[yuvconstants]),%%ymm12                       \n"

#define YUVTORGB_SETUP_AVX512BW(yuvconstants)                         \
  "vpternlogd $0xff,%%zmm13,%%zmm13,%%zmm13                       \n" \
  "vpbroadcastq (%[yuvconstants]),%%zmm8                          \n" \
  "vpabsb     %%zmm13,%%zmm13                                     \n" \
  "vpsllw     $7,%%zmm13,%%zmm13                                  \n" \
  "vpbroadcastq 32(%[yuvconstants]),%%zmm9                        \n" \
  "vpbroadcastq 64(%[yuvconstants]),%%zmm10                       \n" \
  "vpbroadcastq 96(%[yuvconstants]),%%zmm11                       \n" \
  "vpbroadcastq 128(%[yuvconstants]),%%zmm12                      \n" \
  "vmovups    (%[quadsplitperm]),%%zmm16                          \n" \
  "vmovups    (%[dquadsplitperm]),%%zmm17                         \n" \
  "vmovups    (%[unperm]),%%zmm18                                 \n"

#define YUVTORGB_SETUP_AR30_AVX2(yuvconstants)                    \
  YUVTORGB_SETUP_AVX2(yuvconstants)                               \
  "vpcmpeqb    %%ymm0,%%ymm0,%%ymm0                           \n" \
  "vpsrlw      $14,%%ymm0,%%ymm0                              \n" \
  "vpsllw      $3,%%ymm0,%%ymm0                               \n" \
  "vpsubw      %%ymm0,%%ymm12,%%ymm12                         \n"

#define YUVTORGB_SETUP_AR30_AVX512BW(yuvconstants)                    \
  YUVTORGB_SETUP_AVX512BW(yuvconstants)                               \
  "vpternlogd  $0xff,%%zmm0,%%zmm0,%%zmm0                         \n" \
  "vpsrlw      $14,%%zmm0,%%zmm0                                  \n" \
  "vpsllw      $3,%%zmm0,%%zmm0                                   \n" \
  "vpsubw      %%zmm0,%%zmm12,%%zmm12                             \n"

#define YUVTORGB16_AVX2(yuvconstants)                                 \
  "vpsubb      %%ymm13,%%ymm3,%%ymm3                              \n" \
  "vpmulhuw    %%ymm11,%%ymm4,%%ymm4                              \n" \
  "vpmaddubsw  %%ymm3,%%ymm8,%%ymm0                               \n" \
  "vpmaddubsw  %%ymm3,%%ymm9,%%ymm1                               \n" \
  "vpmaddubsw  %%ymm3,%%ymm10,%%ymm2                              \n" \
  "vpaddw      %%ymm4,%%ymm12,%%ymm4                              \n" \
  "vpaddsw     %%ymm4,%%ymm0,%%ymm0                               \n" \
  "vpsubsw     %%ymm1,%%ymm4,%%ymm1                               \n" \
  "vpaddsw     %%ymm4,%%ymm2,%%ymm2                               \n"

#define YUVTORGB16_AVX512BW(yuvconstants)                             \
  "vpsubb      %%zmm13,%%zmm3,%%zmm3                              \n" \
  "vpmulhuw    %%zmm11,%%zmm4,%%zmm4                              \n" \
  "vpmaddubsw  %%zmm3,%%zmm8,%%zmm0                               \n" \
  "vpmaddubsw  %%zmm3,%%zmm9,%%zmm1                               \n" \
  "vpmaddubsw  %%zmm3,%%zmm10,%%zmm2                              \n" \
  "vpaddw      %%zmm4,%%zmm12,%%zmm4                              \n" \
  "vpaddsw     %%zmm4,%%zmm0,%%zmm0                               \n" \
  "vpsubsw     %%zmm1,%%zmm4,%%zmm1                               \n" \
  "vpaddsw     %%zmm4,%%zmm2,%%zmm2                               \n"

#define YUVTORGB_REGS_AVX2 "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13",
#define YUVTORGB_REGS_AVX512BW \
  "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm16", "xmm17", "xmm18",

#else  // Convert 16 pixels: 16 UV and 16 Y.

#define YUVTORGB_SETUP_AVX2(yuvconstants)
#define YUVTORGB_SETUP_AR30_AVX2(yuvconstants)
#define YUVTORGB_SETUP_AR30_AVX512BW(yuvconstants)
#define YUVTORGB16_AVX2(yuvconstants)                                 \
  "vpcmpeqb    %%ymm0,%%ymm0,%%ymm0                               \n" \
  "vpabsb      %%ymm0,%%ymm0                                      \n" \
  "vpsllw      $7,%%ymm0,%%ymm0                                   \n" \
  "vpsubb      %%ymm0,%%ymm3,%%ymm3                               \n" \
  "vpmulhuw    96(%[yuvconstants]),%%ymm4,%%ymm4                  \n" \
  "vmovdqa     (%[yuvconstants]),%%ymm0                           \n" \
  "vmovdqa     32(%[yuvconstants]),%%ymm1                         \n" \
  "vmovdqa     64(%[yuvconstants]),%%ymm2                         \n" \
  "vpmaddubsw  %%ymm3,%%ymm0,%%ymm0                               \n" \
  "vpmaddubsw  %%ymm3,%%ymm1,%%ymm1                               \n" \
  "vpmaddubsw  %%ymm3,%%ymm2,%%ymm2                               \n" \
  "vmovdqa     128(%[yuvconstants]),%%ymm3                        \n" \
  "vpaddw      %%ymm4,%%ymm3,%%ymm4                               \n" \
  "vpaddsw     %%ymm4,%%ymm0,%%ymm0                               \n" \
  "vpsubsw     %%ymm1,%%ymm4,%%ymm1                               \n" \
  "vpaddsw     %%ymm4,%%ymm2,%%ymm2                               \n"

#define YUVTORGB_REGS_AVX2
#endif  // defined(__x86_64__)

#define YUVTORGB_AVX2(yuvconstants)                                   \
  YUVTORGB16_AVX2(yuvconstants)                                       \
  "vpsraw      $0x6,%%ymm0,%%ymm0                                 \n" \
  "vpsraw      $0x6,%%ymm1,%%ymm1                                 \n" \
  "vpsraw      $0x6,%%ymm2,%%ymm2                                 \n" \
  "vpackuswb   %%ymm0,%%ymm0,%%ymm0                               \n" \
  "vpackuswb   %%ymm1,%%ymm1,%%ymm1                               \n" \
  "vpackuswb   %%ymm2,%%ymm2,%%ymm2                               \n"

#define YUVTORGB_AVX512BW(yuvconstants)                               \
  YUVTORGB16_AVX512BW(yuvconstants)                                   \
  "vpsraw     $0x6,%%zmm0,%%zmm0                                  \n" \
  "vpsraw     $0x6,%%zmm1,%%zmm1                                  \n" \
  "vpsraw     $0x6,%%zmm2,%%zmm2                                  \n" \
  "vpackuswb  %%zmm0,%%zmm0,%%zmm0                                \n" \
  "vpackuswb  %%zmm1,%%zmm1,%%zmm1                                \n" \
  "vpackuswb  %%zmm2,%%zmm2,%%zmm2                                \n"

// Store 16 ARGB values.
#define STOREARGB_AVX2                                                \
  "vpunpcklbw %%ymm1,%%ymm0,%%ymm0                                \n" \
  "vpermq     $0xd8,%%ymm0,%%ymm0                                 \n" \
  "vpunpcklbw %%ymm5,%%ymm2,%%ymm2                                \n" \
  "vpermq     $0xd8,%%ymm2,%%ymm2                                 \n" \
  "vpunpcklwd %%ymm2,%%ymm0,%%ymm1                                \n" \
  "vpunpckhwd %%ymm2,%%ymm0,%%ymm0                                \n" \
  "vmovdqu    %%ymm1,(%[dst_argb])                                \n" \
  "vmovdqu    %%ymm0,0x20(%[dst_argb])                            \n" \
  "lea        0x40(%[dst_argb]), %[dst_argb]                      \n"

// Store 32 ARGB values.
#define STOREARGB_AVX512BW                                            \
  "vpunpcklbw %%zmm1,%%zmm0,%%zmm0                                \n" \
  "vpermq     %%zmm0,%%zmm18,%%zmm0                               \n" \
  "vpunpcklbw %%zmm5,%%zmm2,%%zmm2                                \n" \
  "vpermq     %%zmm2,%%zmm18,%%zmm2                               \n" \
  "vpunpcklwd %%zmm2,%%zmm0,%%zmm1                                \n" \
  "vpunpckhwd %%zmm2,%%zmm0,%%zmm0                                \n" \
  "vmovups    %%zmm1,(%[dst_argb])                                \n" \
  "vmovups    %%zmm0,0x40(%[dst_argb])                            \n" \
  "lea        0x80(%[dst_argb]), %[dst_argb]                      \n"

// Store 16 RGB24 values.
#define STORERGB24_AVX2                                                 \
  "vpunpcklbw   %%ymm1,%%ymm0,%%ymm0                                \n" \
  "vpunpcklbw   %%ymm2,%%ymm2,%%ymm2                                \n" \
  "vmovdqa      %%ymm0,%%ymm1                                       \n" \
  "vpunpcklwd   %%ymm2,%%ymm0,%%ymm0                                \n" \
  "vpunpckhwd   %%ymm2,%%ymm1,%%ymm1                                \n" \
  "vpshufb      %%ymm5,%%ymm0,%%ymm0                                \n" \
  "vpshufb      %%ymm6,%%ymm1,%%ymm1                                \n" \
  "vpalignr     $0xc,%%ymm0,%%ymm1,%%ymm1                           \n" \
  "vextracti128 $1,%%ymm0,%%xmm2                                    \n" \
  "vextracti128 $1,%%ymm1,%%xmm3                                    \n" \
  "vpunpcklqdq  %%xmm1,%%xmm0,%%xmm0                                \n" \
  "vpalignr     $8,%%xmm1,%%xmm2,%%xmm2                             \n" \
  "vmovdqu      %%xmm0,(%[dst_rgb24])                               \n" \
  "vmovdqu      %%xmm2,0x10(%[dst_rgb24])                           \n" \
  "vmovdqu      %%xmm3,0x20(%[dst_rgb24])                           \n" \
  "lea          0x30(%[dst_rgb24]),%[dst_rgb24]                     \n"

// Store 32 RGB24 values with VBMI.
#define STORERGB24_AVX512VBMI                                         \
  "vpermt2b    %%zmm1,%%zmm20,%%zmm0                              \n" \
  "vmovdqa64   %%zmm0,%%zmm3                                      \n" \
  "vpermt2b    %%zmm2,%%zmm21,%%zmm3                              \n" \
  "vpermt2b    %%zmm2,%%zmm22,%%zmm0                              \n" \
  "vmovdqu8    %%zmm3,(%[dst_rgb24])                              \n" \
  "vmovdqu8    %%ymm0,0x40(%[dst_rgb24])                          \n" \
  "lea         0x60(%[dst_rgb24]),%[dst_rgb24]                    \n"

// Store masked RGB24 values with VBMI.
#define STORERGB24_MASK_AVX512VBMI                                    \
  "lea         (%q[width],%q[width],2),%k[width]                  \n" \
  "bzhi        %q[width],%[temp],%[y_buf]                         \n" \
  "kmovq       %[y_buf],%%k3                                      \n" \
  "xor         %k[y_buf],%k[y_buf]                                \n" \
  "sub         $0x40,%k[width]                                    \n" \
  "cmovg       %[temp],%[y_buf]                                   \n" \
  "bzhi        %q[width],%[y_buf],%[y_buf]                        \n" \
  "kmovd       %k[y_buf],%%k4                                     \n" \
  "vpermt2b    %%zmm1,%%zmm20,%%zmm0                              \n" \
  "vmovdqa64   %%zmm0,%%zmm3                                      \n" \
  "vpermt2b    %%zmm2,%%zmm21,%%zmm3                              \n" \
  "vpermt2b    %%zmm2,%%zmm22,%%zmm0                              \n" \
  "vmovdqu8    %%zmm3,(%[dst_rgb24])%{%%k3%}                      \n" \
  "vmovdqu8    %%ymm0,0x40(%[dst_rgb24])%{%%k4%}                  \n"

// Store 32 RGB24 values with AVX512BW.
#define STORERGB24_AVX512BW                                           \
  "vpunpcklbw  %%zmm1,%%zmm0,%%zmm0                               \n" \
  "vpunpcklbw  %%zmm2,%%zmm2,%%zmm2                               \n" \
  "vmovdqa64   %%zmm0,%%zmm1                                      \n" \
  "vpunpcklwd  %%zmm2,%%zmm0,%%zmm0                               \n" \
  "vpunpckhwd  %%zmm2,%%zmm1,%%zmm1                               \n" \
  "vpshufb     %%zmm5,%%zmm0,%%zmm0                               \n" \
  "vpshufb     %%zmm6,%%zmm1,%%zmm1                               \n" \
  "vpalignr    $0xc,%%zmm0,%%zmm1,%%zmm1                          \n" \
  "vmovdqa64   %%zmm20,%%zmm3                                     \n" \
  "vpermi2q    %%zmm1,%%zmm0,%%zmm3                               \n" \
  "vmovdqa64   %%zmm21,%%zmm4                                     \n" \
  "vpermi2q    %%zmm1,%%zmm0,%%zmm4                               \n" \
  "vmovdqu8    %%zmm3,(%[dst_rgb24])                              \n" \
  "vmovdqu8    %%ymm4,0x40(%[dst_rgb24])                          \n" \
  "lea         0x60(%[dst_rgb24]),%[dst_rgb24]                    \n"

// Store masked RGB24 values with AVX512BW.
#define STORERGB24_MASK_AVX512BW                                      \
  "lea         (%q[width],%q[width],2),%k[width]                  \n" \
  "bzhi        %q[width],%[temp],%[y_buf]                         \n" \
  "kmovq       %[y_buf],%%k3                                      \n" \
  "xor         %k[y_buf],%k[y_buf]                                \n" \
  "sub         $0x40,%k[width]                                    \n" \
  "cmovg       %[temp],%[y_buf]                                   \n" \
  "bzhi        %q[width],%[y_buf],%[y_buf]                        \n" \
  "kmovd       %k[y_buf],%%k4                                     \n" \
  "vpunpcklbw  %%zmm1,%%zmm0,%%zmm0                               \n" \
  "vpunpcklbw  %%zmm2,%%zmm2,%%zmm2                               \n" \
  "vmovdqa64   %%zmm0,%%zmm1                                      \n" \
  "vpunpcklwd  %%zmm2,%%zmm0,%%zmm0                               \n" \
  "vpunpckhwd  %%zmm2,%%zmm1,%%zmm1                               \n" \
  "vpshufb     %%zmm5,%%zmm0,%%zmm0                               \n" \
  "vpshufb     %%zmm6,%%zmm1,%%zmm1                               \n" \
  "vpalignr    $0xc,%%zmm0,%%zmm1,%%zmm1                          \n" \
  "vmovdqa64   %%zmm20,%%zmm3                                     \n" \
  "vpermi2q    %%zmm1,%%zmm0,%%zmm3                               \n" \
  "vmovdqa64   %%zmm21,%%zmm4                                     \n" \
  "vpermi2q    %%zmm1,%%zmm0,%%zmm4                               \n" \
  "vmovdqu8    %%zmm3,(%[dst_rgb24])%{%%k3%}                      \n" \
  "vmovdqu8    %%ymm4,0x40(%[dst_rgb24])%{%%k4%}                  \n"

// Store 32 AR30 values.
#define STOREAR30_AVX512BW                                            \
  "vpminsw     %%zmm7,%%zmm0,%%zmm0                               \n" \
  "vpminsw     %%zmm7,%%zmm1,%%zmm1                               \n" \
  "vpminsw     %%zmm7,%%zmm2,%%zmm2                               \n" \
  "vpmaxsw     %%zmm6,%%zmm0,%%zmm0                               \n" \
  "vpmaxsw     %%zmm6,%%zmm1,%%zmm1                               \n" \
  "vpmaxsw     %%zmm6,%%zmm2,%%zmm2                               \n" \
  "vpsrlw      $4,%%zmm0,%%zmm0                                   \n" \
  "vpsrlw      $10,%%zmm1,%%zmm3                                  \n" \
  "vpsllw      $6,%%zmm1,%%zmm1                                   \n" \
  "vpternlogd  $0xf8,%%zmm19,%%zmm1,%%zmm0                        \n" \
  "vpternlogd  $0xe4,%%zmm7,%%zmm3,%%zmm2                         \n" \
  "vpord       %%zmm5,%%zmm2,%%zmm2                               \n" \
  "vmovdqa64   %%zmm0,%%zmm3                                      \n" \
  "vpermt2w    %%zmm2,%%zmm20,%%zmm0                              \n" \
  "vpermt2w    %%zmm2,%%zmm21,%%zmm3                              \n" \
  "vmovdqu32   %%zmm0,(%[dst_ar30])                               \n" \
  "vmovdqu32   %%zmm3,0x40(%[dst_ar30])                           \n" \
  "lea         0x80(%[dst_ar30]), %[dst_ar30]                     \n"

// Store 16 AR30 values.
#define STOREAR30_AVX2                                                \
  "vpminsw     %%ymm7,%%ymm0,%%ymm0                               \n" \
  "vpminsw     %%ymm7,%%ymm1,%%ymm1                               \n" \
  "vpminsw     %%ymm7,%%ymm2,%%ymm2                               \n" \
  "vpmaxsw     %%ymm6,%%ymm0,%%ymm0                               \n" \
  "vpmaxsw     %%ymm6,%%ymm1,%%ymm1                               \n" \
  "vpmaxsw     %%ymm6,%%ymm2,%%ymm2                               \n" \
  "vpand       %%ymm7,%%ymm1,%%ymm3                               \n" \
  "vpand       %%ymm7,%%ymm2,%%ymm2                               \n" \
  "vpsrlw      $4,%%ymm0,%%ymm0                                   \n" \
  "vpsrlw      $10,%%ymm1,%%ymm1                                  \n" \
  "vpsllw      $6,%%ymm3,%%ymm3                                   \n" \
  "vpor        %%ymm3,%%ymm0,%%ymm0                               \n" \
  "vpor        %%ymm5,%%ymm2,%%ymm2                               \n" \
  "vpor        %%ymm1,%%ymm2,%%ymm2                               \n" \
  "vpermq      $0xd8,%%ymm2,%%ymm2                                \n" \
  "vpermq      $0xd8,%%ymm0,%%ymm0                                \n" \
  "vpunpcklwd  %%ymm2,%%ymm0,%%ymm1                               \n" \
  "vpunpckhwd  %%ymm2,%%ymm0,%%ymm3                               \n" \
  "vmovdqu     %%ymm1,(%[dst_ar30])                               \n" \
  "vmovdqu     %%ymm3,0x20(%[dst_ar30])                           \n" \
  "lea         0x40(%[dst_ar30]), %[dst_ar30]                     \n"

#ifdef HAS_I444TOARGBROW_AVX2
// 16 pixels
// 16 UV values with 16 Y producing 16 ARGB (64 bytes).
void OMITFP I444ToARGBRow_AVX2(const uint8_t* y_buf,
                               const uint8_t* u_buf,
                               const uint8_t* v_buf,
                               uint8_t* dst_argb,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  asm volatile (
    YUVTORGB_SETUP_AVX2(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"

    LABELALIGN
      "1:          \n"
    READYUV444_AVX2
    YUVTORGB_AVX2(yuvconstants)
    STOREARGB_AVX2
      "sub         $0x10,%[width]                \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS_AVX2
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
  );
}
#endif  // HAS_I444TOARGBROW_AVX2

#if defined(HAS_I422TOARGBROW_AVX2)
// 16 pixels
// 8 UV values upsampled to 16 UV, mixed with 16 Y producing 16 ARGB (64 bytes).
void OMITFP I422ToARGBRow_AVX2(const uint8_t* y_buf,
                               const uint8_t* u_buf,
                               const uint8_t* v_buf,
                               uint8_t* dst_argb,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  asm volatile (
    YUVTORGB_SETUP_AVX2(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"

    LABELALIGN
      "1:          \n"
    READYUV422_AVX2
    YUVTORGB_AVX2(yuvconstants)
    STOREARGB_AVX2
      "sub         $0x10,%[width]                \n"
      "jg          1b                            \n"

      "vzeroupper  \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS_AVX2
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
  );
}
#endif  // defined(HAS_I422TOARGBROW_AVX2)

#if defined(HAS_I422TORGB24ROW_AVX2)
// 16 pixels
// 8 UV values upsampled to 16 UV, mixed with 16 Y producing 16 RGB24 (48
// bytes).
void OMITFP I422ToRGB24Row_AVX2(const uint8_t* y_buf,
                                const uint8_t* u_buf,
                                const uint8_t* v_buf,
                                uint8_t* dst_rgb24,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  asm volatile (
    YUVTORGB_SETUP_AVX2(yuvconstants)
      "vbroadcasti128 %[kShuffleMaskARGBToRGB24_1],%%ymm5 \n"
      "vbroadcasti128 %[kShuffleMaskARGBToRGB24_0],%%ymm6 \n"
      "sub         %[u_buf],%[v_buf]             \n"

    LABELALIGN
      "1:          \n"
    READYUV422_AVX2
    YUVTORGB_AVX2(yuvconstants)
    STORERGB24_AVX2
      "subl        $0x10,%[width]                \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_rgb24]"+r"(dst_rgb24),  // %[dst_rgb24]
#if defined(__i386__)
    [width]"+m"(width)     // %[width]
#else
    [width]"+rm"(width)    // %[width]
#endif
  : [yuvconstants]"r"(yuvconstants),  // %[yuvconstants]
    [kShuffleMaskARGBToRGB24_0]"m"(kShuffleMaskARGBToRGB24[0]),
    [kShuffleMaskARGBToRGB24_1]"m"(kShuffleMaskARGBToRGB24[1])
  : "memory", "cc", YUVTORGB_REGS_AVX2
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6"
  );
}
#endif  // defined(HAS_I422TORGB24ROW_AVX2)

#if defined(HAS_I422TOARGBROW_AVX512BW) ||    \
    defined(HAS_I422TORGB24ROW_AVX512VBMI) || \
    defined(HAS_I422TORGB24ROW_AVX512BW) ||   \
    defined(HAS_I422TOAR30ROW_AVX512BW)
static const uint16_t kSplitQuadWords[32] = {
    0, 0, 1, 1, 2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,
    8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15};
static const uint64_t kSplitDoubleQuadWords[8] = {0, 1, 4, 4, 2, 3, 4, 4};
static const uint64_t kUnpermuteAVX512[8] = {0, 4, 1, 5, 2, 6, 3, 7};
#endif  // defined(HAS_I422TOARGBROW_AVX512BW) ||
        // defined(HAS_I422TORGB24ROW_AVX512VBMI) ||
        // defined(HAS_I422TORGB24ROW_AVX512BW) ||
        // defined(HAS_I422TOAR30ROW_AVX512BW)

#if defined(HAS_I422TOAR30ROW_AVX512BW)
static const uint16_t kPermAR30_0[32] = {
    0, 32, 1, 33, 2,  34, 3,  35, 4,  36, 5,  37, 6,  38, 7,  39,
    8, 40, 9, 41, 10, 42, 11, 43, 12, 44, 13, 45, 14, 46, 15, 47};
static const uint16_t kPermAR30_1[32] = {
    16, 48, 17, 49, 18, 50, 19, 51, 20, 52, 21, 53, 22, 54, 23, 55,
    24, 56, 25, 57, 26, 58, 27, 59, 28, 60, 29, 61, 30, 62, 31, 63};
#endif  // defined(HAS_I422TOAR30ROW_AVX512BW)

#if defined(HAS_I422TOARGBROW_AVX512BW)
// 32 pixels
// 16 UV values upsampled to 32 UV, mixed with 32 Y producing 32 ARGB (128
// bytes).
void OMITFP I422ToARGBRow_AVX512BW(const uint8_t* y_buf,
                                   const uint8_t* u_buf,
                                   const uint8_t* v_buf,
                                   uint8_t* dst_argb,
                                   const struct YuvConstants* yuvconstants,
                                   int width) {
  asm volatile (
    YUVTORGB_SETUP_AVX512BW(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "vpternlogd  $0xff,%%zmm5,%%zmm5,%%zmm5    \n"

    LABELALIGN
      "1:          \n"
    READYUV422_AVX512BW
    YUVTORGB_AVX512BW(yuvconstants)
    STOREARGB_AVX512BW
      "sub         $0x20,%[width]                \n"
      "jg          1b                            \n"

      "vzeroupper  \n"
  : [y_buf]"+r"(y_buf),                         // %[y_buf]
    [u_buf]"+r"(u_buf),                         // %[u_buf]
    [v_buf]"+r"(v_buf),                         // %[v_buf]
    [dst_argb]"+r"(dst_argb),                   // %[dst_argb]
    [width]"+rm"(width)                         // %[width]
  : [yuvconstants]"r"(yuvconstants),            // %[yuvconstants]
    [quadsplitperm]"r"(kSplitQuadWords),        // %[quadsplitperm]
    [dquadsplitperm]"r"(kSplitDoubleQuadWords), // %[dquadsplitperm]
    [unperm]"r"(kUnpermuteAVX512)               // %[unperm]
  : "memory", "cc", YUVTORGB_REGS_AVX512BW
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
  );
}
#endif  // defined(HAS_I422TOARGBROW_AVX512BW)

#if defined(HAS_I422TORGB24ROW_AVX512VBMI)
static const uint8_t kMaskBG[64] = {
    0x00, 0x40, 0x01, 0x41, 0x02, 0x42, 0x03, 0x43, 0x04, 0x44, 0x05,
    0x45, 0x06, 0x46, 0x07, 0x47, 0x10, 0x50, 0x11, 0x51, 0x12, 0x52,
    0x13, 0x53, 0x14, 0x54, 0x15, 0x55, 0x16, 0x56, 0x17, 0x57, 0x20,
    0x60, 0x21, 0x61, 0x22, 0x62, 0x23, 0x63, 0x24, 0x64, 0x25, 0x65,
    0x26, 0x66, 0x27, 0x67, 0x30, 0x70, 0x31, 0x71, 0x32, 0x72, 0x33,
    0x73, 0x34, 0x74, 0x35, 0x75, 0x36, 0x76, 0x37, 0x77};
static const uint8_t kMaskDST0[64] = {
    0x00, 0x01, 0x40, 0x02, 0x03, 0x41, 0x04, 0x05, 0x42, 0x06, 0x07,
    0x43, 0x08, 0x09, 0x44, 0x0a, 0x0b, 0x45, 0x0c, 0x0d, 0x46, 0x0e,
    0x0f, 0x47, 0x10, 0x11, 0x50, 0x12, 0x13, 0x51, 0x14, 0x15, 0x52,
    0x16, 0x17, 0x53, 0x18, 0x19, 0x54, 0x1a, 0x1b, 0x55, 0x1c, 0x1d,
    0x56, 0x1e, 0x1f, 0x57, 0x20, 0x21, 0x60, 0x22, 0x23, 0x61, 0x24,
    0x25, 0x62, 0x26, 0x27, 0x63, 0x28, 0x29, 0x64, 0x2a};
static const uint8_t kMaskDST1[64] = {
    0x2b, 0x65, 0x2c, 0x2d, 0x66, 0x2e, 0x2f, 0x67, 0x30, 0x31, 0x70,
    0x32, 0x33, 0x71, 0x34, 0x35, 0x72, 0x36, 0x37, 0x73, 0x38, 0x39,
    0x74, 0x3a, 0x3b, 0x75, 0x3c, 0x3d, 0x76, 0x3e, 0x3f, 0x77, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// 32 pixels
// 16 UV values upsampled to 32 UV, mixed with 32 Y producing 32 RGB24 (96
// bytes).
void OMITFP I422ToRGB24Row_AVX512VBMI(const uint8_t* y_buf,
                                      const uint8_t* u_buf,
                                      const uint8_t* v_buf,
                                      uint8_t* dst_rgb24,
                                      const struct YuvConstants* yuvconstants,
                                      int width) {
  uintptr_t temp;
  asm volatile (
    YUVTORGB_SETUP_AVX512BW(yuvconstants)
      "vmovdqu32   %[kMaskBG],%%zmm20            \n"
      "vmovdqu32   %[kMaskDST0],%%zmm21          \n"
      "vmovdqu32   %[kMaskDST1],%%zmm22          \n"
      "sub         %[u_buf],%[v_buf]             \n"
      "sub         $0x20,%[width]                \n"
      "jl          2f                            \n"

    LABELALIGN
      "1:          \n"
    READYUV422_AVX512BW
    YUVTORGB_AVX512BW(yuvconstants)
    STORERGB24_AVX512VBMI
      "sub         $0x20,%[width]                \n"
      "jge         1b                            \n"

      "2:          \n"
      "add         $0x20,%[width]                \n"
      "je          99f                           \n"

    READYUV422_MASK_AVX512BW
    YUVTORGB_AVX512BW(yuvconstants)
    STORERGB24_MASK_AVX512VBMI

      "99:         \n"
      "vzeroupper  \n"
  : [y_buf]"+r"(y_buf),                         // %[y_buf]
    [u_buf]"+r"(u_buf),                         // %[u_buf]
    [v_buf]"+r"(v_buf),                         // %[v_buf]
    [dst_rgb24]"+r"(dst_rgb24),                 // %[dst_rgb24]
    [width]"+r"(width),                         // %[width]
    [temp]"=&r"(temp)                           // %[temp]
  : [yuvconstants]"r"(yuvconstants),            // %[yuvconstants]
    [quadsplitperm]"r"(kSplitQuadWords),        // %[quadsplitperm]
    [dquadsplitperm]"r"(kSplitDoubleQuadWords), // %[dquadsplitperm]
    [unperm]"r"(kUnpermuteAVX512),              // %[unperm]
    [kMaskBG]"m"(kMaskBG),                      // %[kMaskBG]
    [kMaskDST0]"m"(kMaskDST0),                  // %[kMaskDST0]
    [kMaskDST1]"m"(kMaskDST1)                   // %[kMaskDST1]
  : "memory", "cc", YUVTORGB_REGS_AVX512BW
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5",
    "xmm20", "xmm21", "xmm22",
    "k1", "k2", "k3", "k4"
  );
}
#endif  // defined(HAS_I422TORGB24ROW_AVX512VBMI)

#if defined(HAS_I422TORGB24ROW_AVX512BW)
static const uint64_t kStitchRGB24_0[8] = {0, 8, 9, 2, 10, 11, 4, 12};
static const uint64_t kStitchRGB24_1[8] = {13, 6, 14, 15, 0, 0, 0, 0};

// 32 pixels
// 16 UV values upsampled to 32 UV, mixed with 32 Y producing 32 RGB24 (96
// bytes).
void OMITFP I422ToRGB24Row_AVX512BW(const uint8_t* y_buf,
                                    const uint8_t* u_buf,
                                    const uint8_t* v_buf,
                                    uint8_t* dst_rgb24,
                                    const struct YuvConstants* yuvconstants,
                                    int width) {
  uintptr_t temp;
  asm volatile (
    YUVTORGB_SETUP_AVX512BW(yuvconstants)
      "vbroadcasti32x4 %[kShuffleMaskARGBToRGB24_1],%%zmm5 \n"
      "vbroadcasti32x4 %[kShuffleMaskARGBToRGB24_0],%%zmm6 \n"
      "vmovdqu64   %[kStitchRGB24_0],%%zmm20     \n"
      "vmovdqu64   %[kStitchRGB24_1],%%zmm21     \n"
      "sub         %[u_buf],%[v_buf]             \n"
      "sub         $0x20,%[width]                \n"
      "jl          2f                            \n"

    LABELALIGN
      "1:          \n"
    READYUV422_AVX512BW
    YUVTORGB_AVX512BW(yuvconstants)
    STORERGB24_AVX512BW
      "sub         $0x20,%[width]                \n"
      "jge         1b                            \n"

      "2:          \n"
      "add         $0x20,%[width]                \n"
      "je          99f                           \n"

    READYUV422_MASK_AVX512BW
    YUVTORGB_AVX512BW(yuvconstants)
    STORERGB24_MASK_AVX512BW

      "99:         \n"
      "vzeroupper  \n"
  : [y_buf]"+r"(y_buf),                         // %[y_buf]
    [u_buf]"+r"(u_buf),                         // %[u_buf]
    [v_buf]"+r"(v_buf),                         // %[v_buf]
    [dst_rgb24]"+r"(dst_rgb24),                 // %[dst_rgb24]
    [width]"+r"(width),                         // %[width]
    [temp]"=&r"(temp)                           // %[temp]
  : [yuvconstants]"r"(yuvconstants),            // %[yuvconstants]
    [quadsplitperm]"r"(kSplitQuadWords),        // %[quadsplitperm]
    [dquadsplitperm]"r"(kSplitDoubleQuadWords), // %[dquadsplitperm]
    [unperm]"r"(kUnpermuteAVX512),              // %[unperm]
    [kShuffleMaskARGBToRGB24_0]"m"(kShuffleMaskARGBToRGB24[0]),
    [kShuffleMaskARGBToRGB24_1]"m"(kShuffleMaskARGBToRGB24[1]),
    [kStitchRGB24_0]"m"(kStitchRGB24_0),        // %[kStitchRGB24_0]
    [kStitchRGB24_1]"m"(kStitchRGB24_1)         // %[kStitchRGB24_1]
  : "memory", "cc", YUVTORGB_REGS_AVX512BW
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
    "xmm20", "xmm21",
    "k1", "k2", "k3", "k4"
  );
}
#endif  // defined(HAS_I422TORGB24ROW_AVX512BW)

#if defined(HAS_I422TOAR30ROW_AVX512BW)
// 32 pixels
// 16 UV values upsampled to 32 UV, mixed with 32 Y producing 32 AR30 (128
// bytes).
void OMITFP I422ToAR30Row_AVX512BW(const uint8_t* y_buf,
                                   const uint8_t* u_buf,
                                   const uint8_t* v_buf,
                                   uint8_t* dst_ar30,
                                   const struct YuvConstants* yuvconstants,
                                   int width) {
  asm volatile (
    YUVTORGB_SETUP_AR30_AVX512BW(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "vpternlogd  $0xff,%%zmm5,%%zmm5,%%zmm5    \n"  // all 1s
      "vpsrlw      $6,%%zmm5,%%zmm7              \n"
      "vpsllw      $4,%%zmm7,%%zmm7              \n"  // 1023 * 16 for max
      "vpsllw      $10,%%zmm5,%%zmm19            \n"  // 0xfc00 for G mask
      "vpsllw      $14,%%zmm5,%%zmm5             \n"  // 0xc000 for 2 alpha bits
      "vpxord      %%zmm6,%%zmm6,%%zmm6          \n"  // 0 for min
      "vmovdqu16   (%[permar30_0]),%%zmm20       \n"
      "vmovdqu16   (%[permar30_1]),%%zmm21       \n"

    LABELALIGN
      "1:          \n"
    READYUV422_AVX512BW
    YUVTORGB16_AVX512BW(yuvconstants)
    STOREAR30_AVX512BW
      "sub         $0x20,%[width]                \n"
      "jg          1b                            \n"

      "vzeroupper  \n"
  : [y_buf]"+r"(y_buf),                         // %[y_buf]
    [u_buf]"+r"(u_buf),                         // %[u_buf]
    [v_buf]"+r"(v_buf),                         // %[v_buf]
    [dst_ar30]"+r"(dst_ar30),                   // %[dst_ar30]
    [width]"+rm"(width)                         // %[width]
  : [yuvconstants]"r"(yuvconstants),            // %[yuvconstants]
    [quadsplitperm]"r"(kSplitQuadWords),        // %[quadsplitperm]
    [dquadsplitperm]"r"(kSplitDoubleQuadWords), // %[dquadsplitperm]
    [unperm]"r"(kUnpermuteAVX512),              // %[unperm]
    [permar30_0]"r"(kPermAR30_0),               // %[permar30_0]
    [permar30_1]"r"(kPermAR30_1)                // %[permar30_1]
  : "memory", "cc", YUVTORGB_REGS_AVX512BW
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
    "xmm19", "xmm20", "xmm21"
  );
}
#endif  // defined(HAS_I422TOAR30ROW_AVX512BW)

#if defined(HAS_I422TOAR30ROW_AVX2)
// 16 pixels
// 8 UV values upsampled to 16 UV, mixed with 16 Y producing 16 AR30 (64 bytes).
void OMITFP I422ToAR30Row_AVX2(const uint8_t* y_buf,
                               const uint8_t* u_buf,
                               const uint8_t* v_buf,
                               uint8_t* dst_ar30,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  asm volatile (
    YUVTORGB_SETUP_AR30_AVX2(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"  // all 1s
      "vpsrlw      $6,%%ymm5,%%ymm7              \n"
      "vpsllw      $4,%%ymm7,%%ymm7              \n"  // 1023 * 16 for max
      "vpsllw      $14,%%ymm5,%%ymm5             \n"  // 0xc000 for 2 alpha bits
      "vpxor       %%ymm6,%%ymm6,%%ymm6          \n"  // 0 for min

    LABELALIGN
      "1:          \n"
    READYUV422_AVX2
    YUVTORGB16_AVX2(yuvconstants)
    STOREAR30_AVX2
      "sub         $0x10,%[width]                \n"
      "jg          1b                            \n"

      "vzeroupper  \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_ar30]"+r"(dst_ar30),  // %[dst_ar30]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS_AVX2
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
  );
}
#endif  // defined(HAS_I422TOAR30ROW_AVX2)

#if defined(HAS_I210TOARGBROW_AVX2)
// 16 pixels
// 8 UV values upsampled to 16 UV, mixed with 16 Y producing 16 ARGB (64 bytes).
void OMITFP I210ToARGBRow_AVX2(const uint16_t* y_buf,
                               const uint16_t* u_buf,
                               const uint16_t* v_buf,
                               uint8_t* dst_argb,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  asm volatile (
    YUVTORGB_SETUP_AVX2(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"

    LABELALIGN
      "1:          \n"
    READYUV210_AVX2
    YUVTORGB_AVX2(yuvconstants)
    STOREARGB_AVX2
      "sub         $0x10,%[width]                \n"
      "jg          1b                            \n"

      "vzeroupper  \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS_AVX2
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
  );
}
#endif  // defined(HAS_I210TOARGBROW_AVX2)

#if defined(HAS_I212TOARGBROW_AVX2)
// 16 pixels
// 8 UV values upsampled to 16 UV, mixed with 16 Y producing 16 ARGB (64 bytes).
void OMITFP I212ToARGBRow_AVX2(const uint16_t* y_buf,
                               const uint16_t* u_buf,
                               const uint16_t* v_buf,
                               uint8_t* dst_argb,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  asm volatile (
    YUVTORGB_SETUP_AVX2(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"

    LABELALIGN
      "1:          \n"
    READYUV212_AVX2
    YUVTORGB_AVX2(yuvconstants)
    STOREARGB_AVX2
      "sub         $0x10,%[width]                \n"
      "jg          1b                            \n"

      "vzeroupper  \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS_AVX2
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
  );
}
#endif  // defined(HAS_I212TOARGBROW_AVX2)

#if defined(HAS_I210TOAR30ROW_AVX2)
// 16 pixels
// 8 UV values upsampled to 16 UV, mixed with 16 Y producing 16 AR30 (64 bytes).
void OMITFP I210ToAR30Row_AVX2(const uint16_t* y_buf,
                               const uint16_t* u_buf,
                               const uint16_t* v_buf,
                               uint8_t* dst_ar30,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  asm volatile (
    YUVTORGB_SETUP_AVX2(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"  // all 1s
      "vpsrlw      $6,%%ymm5,%%ymm7              \n"
      "vpsllw      $4,%%ymm7,%%ymm7              \n"  // 1023 * 16 for max
      "vpsllw      $14,%%ymm5,%%ymm5             \n"  // 0xc000 for 2 alpha bits
      "vpxor       %%ymm6,%%ymm6,%%ymm6          \n"  // 0 for min

    LABELALIGN
      "1:          \n"
    READYUV210_AVX2
    YUVTORGB16_AVX2(yuvconstants)
    STOREAR30_AVX2
      "sub         $0x10,%[width]                \n"
      "jg          1b                            \n"

      "vzeroupper  \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_ar30]"+r"(dst_ar30),  // %[dst_ar30]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS_AVX2
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
  );
}
#endif  // defined(HAS_I210TOAR30ROW_AVX2)

#if defined(HAS_I212TOAR30ROW_AVX2)
// 16 pixels
// 8 UV values upsampled to 16 UV, mixed with 16 Y producing 16 AR30 (64 bytes).
void OMITFP I212ToAR30Row_AVX2(const uint16_t* y_buf,
                               const uint16_t* u_buf,
                               const uint16_t* v_buf,
                               uint8_t* dst_ar30,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  asm volatile (
    YUVTORGB_SETUP_AVX2(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"  // all 1s
      "vpsrlw      $6,%%ymm5,%%ymm7              \n"
      "vpsllw      $4,%%ymm7,%%ymm7              \n"  // 1023 * 16 for max
      "vpsllw      $14,%%ymm5,%%ymm5             \n"  // 0xc000 for 2 alpha bits
      "vpxor       %%ymm6,%%ymm6,%%ymm6          \n"  // 0 for min

    LABELALIGN
      "1:          \n"
    READYUV212_AVX2
    YUVTORGB16_AVX2(yuvconstants)
    STOREAR30_AVX2
      "sub         $0x10,%[width]                \n"
      "jg          1b                            \n"

      "vzeroupper  \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_ar30]"+r"(dst_ar30),  // %[dst_ar30]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS_AVX2
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
  );
}
#endif  // defined(HAS_I212TOAR30ROW_AVX2)

#if defined(HAS_I410TOARGBROW_AVX2)
// 16 pixels
// 16 UV values with 16 Y producing 16 ARGB (64 bytes).
void OMITFP I410ToARGBRow_AVX2(const uint16_t* y_buf,
                               const uint16_t* u_buf,
                               const uint16_t* v_buf,
                               uint8_t* dst_argb,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  asm volatile (
    YUVTORGB_SETUP_AVX2(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"

    LABELALIGN
      "1:          \n"
    READYUV410_AVX2
    YUVTORGB_AVX2(yuvconstants)
    STOREARGB_AVX2
      "sub         $0x10,%[width]                \n"
      "jg          1b                            \n"
      "vzeroupper  \n"

  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS_AVX2
      "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
  );
}
#endif  // defined(HAS_I410TOARGBROW_AVX2)

#if defined(HAS_I210ALPHATOARGBROW_AVX2)
// 16 pixels
// 8 UV, 16 Y and 16 A producing 16 ARGB (64 bytes).
void OMITFP I210AlphaToARGBRow_AVX2(const uint16_t* y_buf,
                                    const uint16_t* u_buf,
                                    const uint16_t* v_buf,
                                    const uint16_t* a_buf,
                                    uint8_t* dst_argb,
                                    const struct YuvConstants* yuvconstants,
                                    int width) {
  asm volatile(YUVTORGB_SETUP_AVX2(
                   yuvconstants) "sub         %[u_buf],%[v_buf]             \n"

               LABELALIGN "1:          \n" READYUVA210_AVX2 YUVTORGB_AVX2(
                   yuvconstants) STOREARGB_AVX2
               "subl        $0x10,%[width]                \n"
               "jg          1b                            \n"
               "vzeroupper  \n"

               : [y_buf] "+r"(y_buf),        // %[y_buf]
                 [u_buf] "+r"(u_buf),        // %[u_buf]
                 [v_buf] "+r"(v_buf),        // %[v_buf]
                 [a_buf] "+r"(a_buf),        // %[a_buf]
                 [dst_argb] "+r"(dst_argb),  // %[dst_argb]
#if defined(__i386__)
                 [width] "+m"(width)  // %[width]
#else
                 [width] "+rm"(width)  // %[width]
#endif
               : [yuvconstants] "r"(yuvconstants)  // %[yuvconstants]
               : "memory", "cc", YUVTORGB_REGS_AVX2 "xmm0", "xmm1", "xmm2",
                 "xmm3", "xmm4", "xmm5");
}
#endif  // defined(HAS_I210ALPHATOARGBROW_AVX2)

#if defined(HAS_I410ALPHATOARGBROW_AVX2)
// 16 pixels
// 16 UV, 16 Y and 16 A producing 16 ARGB (64 bytes).
void OMITFP I410AlphaToARGBRow_AVX2(const uint16_t* y_buf,
                                    const uint16_t* u_buf,
                                    const uint16_t* v_buf,
                                    const uint16_t* a_buf,
                                    uint8_t* dst_argb,
                                    const struct YuvConstants* yuvconstants,
                                    int width) {
  asm volatile(YUVTORGB_SETUP_AVX2(
                   yuvconstants) "sub         %[u_buf],%[v_buf]             \n"

               LABELALIGN "1:          \n" READYUVA410_AVX2 YUVTORGB_AVX2(
                   yuvconstants) STOREARGB_AVX2
               "subl        $0x10,%[width]                \n"
               "jg          1b                            \n"
               "vzeroupper  \n"

               : [y_buf] "+r"(y_buf),        // %[y_buf]
                 [u_buf] "+r"(u_buf),        // %[u_buf]
                 [v_buf] "+r"(v_buf),        // %[v_buf]
                 [a_buf] "+r"(a_buf),        // %[a_buf]
                 [dst_argb] "+r"(dst_argb),  // %[dst_argb]
#if defined(__i386__)
                 [width] "+m"(width)  // %[width]
#else
                 [width] "+rm"(width)  // %[width]
#endif
               : [yuvconstants] "r"(yuvconstants)  // %[yuvconstants]
               : "memory", "cc", YUVTORGB_REGS_AVX2 "xmm0", "xmm1", "xmm2",
                 "xmm3", "xmm4", "xmm5");
}
#endif  // defined(HAS_I410ALPHATOARGBROW_AVX2)

#if defined(HAS_I410TOAR30ROW_AVX2)
// 16 pixels
// 16 UV values with 16 Y producing 16 AR30 (64 bytes).
void OMITFP I410ToAR30Row_AVX2(const uint16_t* y_buf,
                               const uint16_t* u_buf,
                               const uint16_t* v_buf,
                               uint8_t* dst_ar30,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  asm volatile (
    YUVTORGB_SETUP_AVX2(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"  // all 1s
      "vpsrlw      $6,%%ymm5,%%ymm7              \n"
      "vpsllw      $4,%%ymm7,%%ymm7              \n"  // 1023 * 16 for max
      "vpsllw      $14,%%ymm5,%%ymm5             \n"  // 0xc000 for 2 alpha bits
      "vpxor       %%ymm6,%%ymm6,%%ymm6          \n"  // 0 for min

    LABELALIGN
      "1:          \n"
    READYUV410_AVX2
    YUVTORGB16_AVX2(yuvconstants)
    STOREAR30_AVX2
      "sub         $0x10,%[width]                \n"
      "jg          1b                            \n"

      "vzeroupper  \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_ar30]"+r"(dst_ar30),  // %[dst_ar30]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS_AVX2
      "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
  );
}
#endif  // defined(HAS_I410TOAR30ROW_AVX2)

#if defined(HAS_I444ALPHATOARGBROW_AVX2)
// 16 pixels
// 16 UV values with 16 Y and 16 A producing 16 ARGB.
void OMITFP I444AlphaToARGBRow_AVX2(const uint8_t* y_buf,
                                    const uint8_t* u_buf,
                                    const uint8_t* v_buf,
                                    const uint8_t* a_buf,
                                    uint8_t* dst_argb,
                                    const struct YuvConstants* yuvconstants,
                                    int width) {
  asm volatile(YUVTORGB_SETUP_AVX2(
                   yuvconstants) "sub         %[u_buf],%[v_buf]             \n"

               LABELALIGN "1:          \n" READYUVA444_AVX2 YUVTORGB_AVX2(
                   yuvconstants) STOREARGB_AVX2
               "subl        $0x10,%[width]                \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : [y_buf] "+r"(y_buf),        // %[y_buf]
                 [u_buf] "+r"(u_buf),        // %[u_buf]
                 [v_buf] "+r"(v_buf),        // %[v_buf]
                 [a_buf] "+r"(a_buf),        // %[a_buf]
                 [dst_argb] "+r"(dst_argb),  // %[dst_argb]
#if defined(__i386__)
                 [width] "+m"(width)  // %[width]
#else
                 [width] "+rm"(width)  // %[width]
#endif
               : [yuvconstants] "r"(yuvconstants)  // %[yuvconstants]
               : "memory", "cc", YUVTORGB_REGS_AVX2 "xmm0", "xmm1", "xmm2",
                 "xmm3", "xmm4", "xmm5");
}
#endif  // defined(HAS_I444ALPHATOARGBROW_AVX2)

#if defined(HAS_I422ALPHATOARGBROW_AVX2)
// 16 pixels
// 8 UV values upsampled to 16 UV, mixed with 16 Y and 16 A producing 16 ARGB.
void OMITFP I422AlphaToARGBRow_AVX2(const uint8_t* y_buf,
                                    const uint8_t* u_buf,
                                    const uint8_t* v_buf,
                                    const uint8_t* a_buf,
                                    uint8_t* dst_argb,
                                    const struct YuvConstants* yuvconstants,
                                    int width) {
  asm volatile(YUVTORGB_SETUP_AVX2(
                   yuvconstants) "sub         %[u_buf],%[v_buf]             \n"

               LABELALIGN "1:          \n" READYUVA422_AVX2 YUVTORGB_AVX2(
                   yuvconstants) STOREARGB_AVX2
               "subl        $0x10,%[width]                \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : [y_buf] "+r"(y_buf),        // %[y_buf]
                 [u_buf] "+r"(u_buf),        // %[u_buf]
                 [v_buf] "+r"(v_buf),        // %[v_buf]
                 [a_buf] "+r"(a_buf),        // %[a_buf]
                 [dst_argb] "+r"(dst_argb),  // %[dst_argb]
#if defined(__i386__)
                 [width] "+m"(width)  // %[width]
#else
                 [width] "+rm"(width)  // %[width]
#endif
               : [yuvconstants] "r"(yuvconstants)  // %[yuvconstants]
               : "memory", "cc", YUVTORGB_REGS_AVX2 "xmm0", "xmm1", "xmm2",
                 "xmm3", "xmm4", "xmm5");
}
#endif  // defined(HAS_I422ALPHATOARGBROW_AVX2)

#if defined(HAS_I422TORGBAROW_AVX2)
// 16 pixels
// 8 UV values upsampled to 16 UV, mixed with 16 Y producing 16 RGBA (64 bytes).
void OMITFP I422ToRGBARow_AVX2(const uint8_t* y_buf,
                               const uint8_t* u_buf,
                               const uint8_t* v_buf,
                               uint8_t* dst_argb,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  asm volatile (
    YUVTORGB_SETUP_AVX2(yuvconstants)
      "sub         %[u_buf],%[v_buf]             \n"
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"

    LABELALIGN
      "1:          \n"
    READYUV422_AVX2
    YUVTORGB_AVX2(yuvconstants)

    // Step 3: Weave into RGBA
    "vpunpcklbw %%ymm2,%%ymm1,%%ymm1           \n"
    "vpermq     $0xd8,%%ymm1,%%ymm1            \n"
    "vpunpcklbw %%ymm0,%%ymm5,%%ymm2           \n"
    "vpermq     $0xd8,%%ymm2,%%ymm2            \n"
    "vpunpcklwd %%ymm1,%%ymm2,%%ymm0           \n"
    "vpunpckhwd %%ymm1,%%ymm2,%%ymm1           \n"
    "vmovdqu    %%ymm0,(%[dst_argb])           \n"
    "vmovdqu    %%ymm1,0x20(%[dst_argb])       \n"
    "lea        0x40(%[dst_argb]),%[dst_argb]  \n"
    "sub        $0x10,%[width]                 \n"
    "jg         1b                             \n"
    "vzeroupper                                \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [u_buf]"+r"(u_buf),    // %[u_buf]
    [v_buf]"+r"(v_buf),    // %[v_buf]
    [dst_argb]"+r"(dst_argb),  // %[dst_argb]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS_AVX2
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
  );
}
#endif  // defined(HAS_I422TORGBAROW_AVX2)

#if defined(HAS_NV12TOARGBROW_AVX2)
// 16 pixels.
// 8 UV values upsampled to 16 UV, mixed with 16 Y producing 16 ARGB (64 bytes).
void OMITFP NV12ToARGBRow_AVX2(const uint8_t* y_buf,
                               const uint8_t* uv_buf,
                               uint8_t* dst_argb,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  asm volatile(YUVTORGB_SETUP_AVX2(
                   yuvconstants) "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"

               LABELALIGN "1:          \n" READNV12_AVX2 YUVTORGB_AVX2(
                   yuvconstants) STOREARGB_AVX2
               "sub         $0x10,%[width]                \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : [y_buf] "+r"(y_buf),              // %[y_buf]
                 [uv_buf] "+r"(uv_buf),            // %[uv_buf]
                 [dst_argb] "+r"(dst_argb),        // %[dst_argb]
                 [width] "+rm"(width)              // %[width]
               : [yuvconstants] "r"(yuvconstants)  // %[yuvconstants]
               : "memory", "cc", YUVTORGB_REGS_AVX2 "xmm0", "xmm0", "xmm1",
                 "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // defined(HAS_NV12TOARGBROW_AVX2)

#if defined(HAS_NV21TOARGBROW_AVX2)
// 16 pixels.
// 8 VU values upsampled to 16 UV, mixed with 16 Y producing 16 ARGB (64 bytes).
void OMITFP NV21ToARGBRow_AVX2(const uint8_t* y_buf,
                               const uint8_t* vu_buf,
                               uint8_t* dst_argb,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  asm volatile(YUVTORGB_SETUP_AVX2(
                   yuvconstants) "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"

               LABELALIGN "1:          \n" READNV21_AVX2 YUVTORGB_AVX2(
                   yuvconstants) STOREARGB_AVX2
               "sub         $0x10,%[width]                \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : [y_buf] "+r"(y_buf),               // %[y_buf]
                 [vu_buf] "+r"(vu_buf),             // %[vu_buf]
                 [dst_argb] "+r"(dst_argb),         // %[dst_argb]
                 [width] "+rm"(width)               // %[width]
               : [yuvconstants] "r"(yuvconstants),  // %[yuvconstants]
                 [kShuffleNV21] "m"(kShuffleNV21)
               : "memory", "cc", YUVTORGB_REGS_AVX2 "xmm0", "xmm0", "xmm1",
                 "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // defined(HAS_NV21TOARGBROW_AVX2)

#if defined(HAS_YUY2TOARGBROW_AVX2)
// 16 pixels.
// 8 YUY2 values with 16 Y and 8 UV producing 16 ARGB (64 bytes).
void OMITFP YUY2ToARGBRow_AVX2(const uint8_t* yuy2_buf,
                               uint8_t* dst_argb,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  asm volatile(
      "vbroadcasti128 %[kShuffleYUY2Y],%%ymm6    \n"
      "vbroadcasti128 %[kShuffleYUY2UV],%%ymm7   \n" YUVTORGB_SETUP_AVX2(
          yuvconstants) "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"

      LABELALIGN "1:          \n" READYUY2_AVX2 YUVTORGB_AVX2(yuvconstants)
          STOREARGB_AVX2
      "sub         $0x10,%[width]                \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : [yuy2_buf] "+r"(yuy2_buf),         // %[yuy2_buf]
        [dst_argb] "+r"(dst_argb),         // %[dst_argb]
        [width] "+rm"(width)               // %[width]
      : [yuvconstants] "r"(yuvconstants),  // %[yuvconstants]
        [kShuffleYUY2Y] "m"(kShuffleYUY2Y), [kShuffleYUY2UV] "m"(kShuffleYUY2UV)
      : "memory", "cc", YUVTORGB_REGS_AVX2 "xmm0", "xmm0", "xmm1", "xmm2",
        "xmm3", "xmm4", "xmm5", "xmm6", "xmm7");
}
#endif  // defined(HAS_YUY2TOARGBROW_AVX2)

#if defined(HAS_UYVYTOARGBROW_AVX2)
// 16 pixels.
// 8 UYVY values with 16 Y and 8 UV producing 16 ARGB (64 bytes).
void OMITFP UYVYToARGBRow_AVX2(const uint8_t* uyvy_buf,
                               uint8_t* dst_argb,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  asm volatile(
      "vbroadcasti128 %[kShuffleUYVYY],%%ymm6    \n"
      "vbroadcasti128 %[kShuffleUYVYUV],%%ymm7   \n" YUVTORGB_SETUP_AVX2(
          yuvconstants) "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"

      LABELALIGN "1:          \n" READUYVY_AVX2 YUVTORGB_AVX2(yuvconstants)
          STOREARGB_AVX2
      "sub         $0x10,%[width]                \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : [uyvy_buf] "+r"(uyvy_buf),         // %[uyvy_buf]
        [dst_argb] "+r"(dst_argb),         // %[dst_argb]
        [width] "+rm"(width)               // %[width]
      : [yuvconstants] "r"(yuvconstants),  // %[yuvconstants]
        [kShuffleUYVYY] "m"(kShuffleUYVYY), [kShuffleUYVYUV] "m"(kShuffleUYVYUV)
      : "memory", "cc", YUVTORGB_REGS_AVX2 "xmm0", "xmm1", "xmm2", "xmm3",
        "xmm4", "xmm5", "xmm6", "xmm7");
}
#endif  // defined(HAS_UYVYTOARGBROW_AVX2)

#if defined(HAS_P210TOARGBROW_AVX2)
// 16 pixels.
// 8 UV values upsampled to 16 UV, mixed with 16 Y producing 16 ARGB (64 bytes).
void OMITFP P210ToARGBRow_AVX2(const uint16_t* y_buf,
                               const uint16_t* uv_buf,
                               uint8_t* dst_argb,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  asm volatile(YUVTORGB_SETUP_AVX2(
                   yuvconstants) "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"

               LABELALIGN "1:          \n" READP210_AVX2 YUVTORGB_AVX2(
                   yuvconstants) STOREARGB_AVX2
               "sub         $0x10,%[width]                \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : [y_buf] "+r"(y_buf),              // %[y_buf]
                 [uv_buf] "+r"(uv_buf),            // %[uv_buf]
                 [dst_argb] "+r"(dst_argb),        // %[dst_argb]
                 [width] "+rm"(width)              // %[width]
               : [yuvconstants] "r"(yuvconstants)  // %[yuvconstants]
               : "memory", "cc", YUVTORGB_REGS_AVX2 "xmm0", "xmm0", "xmm1",
                 "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // defined(HAS_P210TOARGBROW_AVX2)

#if defined(HAS_P410TOARGBROW_AVX2)
// 16 pixels.
// 8 UV values upsampled to 16 UV, mixed with 16 Y producing 16 ARGB (64 bytes).
void OMITFP P410ToARGBRow_AVX2(const uint16_t* y_buf,
                               const uint16_t* uv_buf,
                               uint8_t* dst_argb,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  asm volatile(YUVTORGB_SETUP_AVX2(
                   yuvconstants) "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"

               LABELALIGN "1:          \n" READP410_AVX2 YUVTORGB_AVX2(
                   yuvconstants) STOREARGB_AVX2
               "sub         $0x10,%[width]                \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : [y_buf] "+r"(y_buf),              // %[y_buf]
                 [uv_buf] "+r"(uv_buf),            // %[uv_buf]
                 [dst_argb] "+r"(dst_argb),        // %[dst_argb]
                 [width] "+rm"(width)              // %[width]
               : [yuvconstants] "r"(yuvconstants)  // %[yuvconstants]
               : "memory", "cc", YUVTORGB_REGS_AVX2 "xmm0", "xmm0", "xmm1",
                 "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // defined(HAS_P410TOARGBROW_AVX2)

#if defined(HAS_P210TOAR30ROW_AVX2)
// 16 pixels
// 16 UV values with 16 Y producing 16 AR30 (64 bytes).
void OMITFP P210ToAR30Row_AVX2(const uint16_t* y_buf,
                               const uint16_t* uv_buf,
                               uint8_t* dst_ar30,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  asm volatile (
    YUVTORGB_SETUP_AVX2(yuvconstants)
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"  // all 1s
      "vpsrlw      $6,%%ymm5,%%ymm7              \n"
      "vpsllw      $4,%%ymm7,%%ymm7              \n"  // 1023 * 16 for max
      "vpsllw      $14,%%ymm5,%%ymm5             \n"  // 0xc000 for 2 alpha bits
      "vpxor       %%ymm6,%%ymm6,%%ymm6          \n"  // 0 for min

    LABELALIGN
      "1:          \n"
    READP210_AVX2
    YUVTORGB16_AVX2(yuvconstants)
    STOREAR30_AVX2
      "sub         $0x10,%[width]                \n"
      "jg          1b                            \n"

      "vzeroupper  \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [uv_buf]"+r"(uv_buf),    // %[uv_buf]
    [dst_ar30]"+r"(dst_ar30),  // %[dst_ar30]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS_AVX2
      "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
  );
}
#endif  // defined(HAS_P210TOAR30ROW_AVX2)

#if defined(HAS_P410TOAR30ROW_AVX2)
// 16 pixels
// 16 UV values with 16 Y producing 16 AR30 (64 bytes).
void OMITFP P410ToAR30Row_AVX2(const uint16_t* y_buf,
                               const uint16_t* uv_buf,
                               uint8_t* dst_ar30,
                               const struct YuvConstants* yuvconstants,
                               int width) {
  asm volatile (
    YUVTORGB_SETUP_AVX2(yuvconstants)
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"  // all 1s
      "vpsrlw      $6,%%ymm5,%%ymm7              \n"
      "vpsllw      $4,%%ymm7,%%ymm7              \n"  // 1023 * 16 for max
      "vpsllw      $14,%%ymm5,%%ymm5             \n"  // 0xc000 for 2 alpha bits
      "vpxor       %%ymm6,%%ymm6,%%ymm6          \n"  // 0 for min

    LABELALIGN
      "1:          \n"
    READP410_AVX2
    YUVTORGB16_AVX2(yuvconstants)
    STOREAR30_AVX2
      "sub         $0x10,%[width]                \n"
      "jg          1b                            \n"

      "vzeroupper  \n"
  : [y_buf]"+r"(y_buf),    // %[y_buf]
    [uv_buf]"+r"(uv_buf),    // %[uv_buf]
    [dst_ar30]"+r"(dst_ar30),  // %[dst_ar30]
    [width]"+rm"(width)    // %[width]
  : [yuvconstants]"r"(yuvconstants)  // %[yuvconstants]
  : "memory", "cc", YUVTORGB_REGS_AVX2
      "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
  );
}
#endif  // defined(HAS_P410TOAR30ROW_AVX2)

#ifdef HAS_I400TOARGBROW_SSE2
void I400ToARGBRow_SSE2(const uint8_t* y_buf,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  asm volatile(
      "movdqa      96(%3),%%xmm2                 \n"  // yg = 18997 = 1.164
      "movdqa      128(%3),%%xmm3                \n"  // ygb = 1160 = 1.164 * 16
      "pcmpeqb     %%xmm4,%%xmm4                 \n"  // 0xff000000
      "pslld       $0x18,%%xmm4                  \n"

      LABELALIGN
      "1:          \n"
      // Step 1: Scale Y contribution to 8 G values. G = (y - 16) * 1.164
      "movq      (%0),%%xmm0                     \n"
      "lea       0x8(%0),%0                      \n"
      "punpcklbw %%xmm0,%%xmm0                   \n"
      "pmulhuw   %%xmm2,%%xmm0                   \n"
      "paddsw    %%xmm3,%%xmm0                   \n"
      "psraw     $6, %%xmm0                      \n"
      "packuswb  %%xmm0,%%xmm0                   \n"

      // Step 2: Weave into ARGB
      "punpcklbw %%xmm0,%%xmm0                   \n"
      "movdqa    %%xmm0,%%xmm1                   \n"
      "punpcklwd %%xmm0,%%xmm0                   \n"
      "punpckhwd %%xmm1,%%xmm1                   \n"
      "por       %%xmm4,%%xmm0                   \n"
      "por       %%xmm4,%%xmm1                   \n"
      "movdqu    %%xmm0,(%1)                     \n"
      "movdqu    %%xmm1,0x10(%1)                 \n"
      "lea       0x20(%1),%1                     \n"

      "sub       $0x8,%2                         \n"
      "jg        1b                              \n"
      : "+r"(y_buf),       // %0
        "+r"(dst_argb),    // %1
        "+rm"(width)       // %2
      : "r"(yuvconstants)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4");
}
#endif  // HAS_I400TOARGBROW_SSE2

#ifdef HAS_I400TOARGBROW_AVX2
// 16 pixels of Y converted to 16 pixels of ARGB (64 bytes).
// note: vpunpcklbw mutates and vpackuswb unmutates.
void I400ToARGBRow_AVX2(const uint8_t* y_buf,
                        uint8_t* dst_argb,
                        const struct YuvConstants* yuvconstants,
                        int width) {
  asm volatile(
      "vmovdqa     96(%3),%%ymm2                 \n"  // yg = 18997 = 1.164
      "vmovdqa     128(%3),%%ymm3                \n"  // ygb = -1160 = 1.164*16
      "vpcmpeqb    %%ymm4,%%ymm4,%%ymm4          \n"  // 0xff000000
      "vpslld      $0x18,%%ymm4,%%ymm4           \n"

      LABELALIGN
      "1:          \n"
      // Step 1: Scale Y contribution to 16 G values. G = (y - 16) * 1.164
      "vmovdqu    (%0),%%xmm0                    \n"
      "lea        0x10(%0),%0                    \n"
      "vpermq     $0xd8,%%ymm0,%%ymm0            \n"
      "vpunpcklbw %%ymm0,%%ymm0,%%ymm0           \n"
      "vpmulhuw   %%ymm2,%%ymm0,%%ymm0           \n"
      "vpaddsw    %%ymm3,%%ymm0,%%ymm0           \n"
      "vpsraw     $0x6,%%ymm0,%%ymm0             \n"
      "vpackuswb  %%ymm0,%%ymm0,%%ymm0           \n"
      "vpunpcklbw %%ymm0,%%ymm0,%%ymm1           \n"
      "vpermq     $0xd8,%%ymm1,%%ymm1            \n"
      "vpunpcklwd %%ymm1,%%ymm1,%%ymm0           \n"
      "vpunpckhwd %%ymm1,%%ymm1,%%ymm1           \n"
      "vpor       %%ymm4,%%ymm0,%%ymm0           \n"
      "vpor       %%ymm4,%%ymm1,%%ymm1           \n"
      "vmovdqu    %%ymm0,(%1)                    \n"
      "vmovdqu    %%ymm1,0x20(%1)                \n"
      "lea        0x40(%1),%1                     \n"
      "sub        $0x10,%2                       \n"
      "jg        1b                              \n"
      "vzeroupper                                \n"
      : "+r"(y_buf),       // %0
        "+r"(dst_argb),    // %1
        "+rm"(width)       // %2
      : "r"(yuvconstants)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4");
}
#endif  // HAS_I400TOARGBROW_AVX2

#ifdef HAS_MIRRORROW_SSSE3
// Shuffle table for reversing the bytes.
static const uvec8 kShuffleMirror = {15u, 14u, 13u, 12u, 11u, 10u, 9u, 8u,
                                     7u,  6u,  5u,  4u,  3u,  2u,  1u, 0u};

void MirrorRow_SSSE3(const uint8_t* src, uint8_t* dst, int width) {
  ptrdiff_t temp_width = (ptrdiff_t)(width);
  asm volatile("movdqa      %3,%%xmm5                     \n"

               LABELALIGN
               "1:          \n"
               "movdqu      -0x10(%0,%2,1),%%xmm0         \n"
               "pshufb      %%xmm5,%%xmm0                 \n"
               "movdqu      %%xmm0,(%1)                   \n"
               "lea         0x10(%1),%1                   \n"
               "sub         $0x10,%2                      \n"
               "jg          1b                            \n"
               : "+r"(src),           // %0
                 "+r"(dst),           // %1
                 "+r"(temp_width)     // %2
               : "m"(kShuffleMirror)  // %3
               : "memory", "cc", "xmm0", "xmm5");
}
#endif  // HAS_MIRRORROW_SSSE3

#ifdef HAS_MIRRORROW_AVX512BW
void MirrorRow_AVX512BW(const uint8_t* src, uint8_t* dst, int width) {
  ptrdiff_t temp_width = (ptrdiff_t)(width);
  asm volatile("vbroadcasti32x4 %3,%%zmm5                 \n"

               LABELALIGN
               "1:          \n"
               "vmovdqu8    -0x40(%0,%2,1),%%zmm0         \n"
               "vpshufb     %%zmm5,%%zmm0,%%zmm0          \n"
               "vshufi64x2  $0x1b,%%zmm0,%%zmm0,%%zmm0    \n"
               "vmovdqu8    %%zmm0,(%1)                   \n"
               "lea         0x40(%1),%1                   \n"
               "sub         $0x40,%2                      \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : "+r"(src),           // %0
                 "+r"(dst),           // %1
                 "+r"(temp_width)     // %2
               : "m"(kShuffleMirror)  // %3
               : "memory", "cc", "xmm0", "xmm5");
}
#endif  // HAS_MIRRORROW_AVX512BW

#ifdef HAS_MIRRORROW_AVX2
void MirrorRow_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  ptrdiff_t temp_width = (ptrdiff_t)(width);
  asm volatile("vbroadcasti128 %3,%%ymm5                  \n"

               LABELALIGN
               "1:          \n"
               "vmovdqu     -0x20(%0,%2,1),%%ymm0         \n"
               "vpshufb     %%ymm5,%%ymm0,%%ymm0          \n"
               "vpermq      $0x4e,%%ymm0,%%ymm0           \n"
               "vmovdqu     %%ymm0,(%1)                   \n"
               "lea         0x20(%1),%1                   \n"
               "sub         $0x20,%2                      \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : "+r"(src),           // %0
                 "+r"(dst),           // %1
                 "+r"(temp_width)     // %2
               : "m"(kShuffleMirror)  // %3
               : "memory", "cc", "xmm0", "xmm5");
}
#endif  // HAS_MIRRORROW_AVX2

#if defined(HAS_MIRRORSPLITUVROW_AVX2) || defined(HAS_MIRRORSPLITUVROW_AVX512BW)
// Shuffle table for reversing the bytes of UV channels.
static const uvec8 kShuffleMirrorSplitUV = {14u, 12u, 10u, 8u, 6u, 4u, 2u, 0u,
                                            15u, 13u, 11u, 9u, 7u, 5u, 3u, 1u};
#endif  // defined(HAS_MIRRORSPLITUVROW_AVX2) ||
        // defined(HAS_MIRRORSPLITUVROW_AVX512BW)

#ifdef HAS_MIRRORSPLITUVROW_AVX512BW
static const uint64_t kMirrorSplitUVPermute[8] = {6, 4, 2, 0, 7, 5, 3, 1};

void MirrorSplitUVRow_AVX512BW(const uint8_t* src,
                               uint8_t* dst_u,
                               uint8_t* dst_v,
                               int width) {
  ptrdiff_t temp_width = (ptrdiff_t)(width);
  asm volatile(
      "vbroadcasti32x4 %4,%%zmm1                 \n"
      "lea         -0x40(%0,%3,2),%0             \n"
      "sub         %1,%2                         \n"
      "vmovdqu64   %5,%%zmm3                     \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu8    (%0),%%zmm0                   \n"
      "lea         -0x40(%0),%0                  \n"
      "vpshufb     %%zmm1,%%zmm0,%%zmm0          \n"
      "vpermq      %%zmm0,%%zmm3,%%zmm0          \n"
      "vextracti64x4 $0x1,%%zmm0,%%ymm2          \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "vmovdqu     %%ymm2,0x00(%1,%2,1)          \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x20,%3                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src),                   // %0
        "+r"(dst_u),                 // %1
        "+r"(dst_v),                 // %2
        "+r"(temp_width)             // %3
      : "m"(kShuffleMirrorSplitUV),  // %4
        "m"(kMirrorSplitUVPermute)   // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3");
}
#endif  // HAS_MIRRORSPLITUVROW_AVX512BW

#ifdef HAS_MIRRORSPLITUVROW_AVX2
void MirrorSplitUVRow_AVX2(const uint8_t* src,
                           uint8_t* dst_u,
                           uint8_t* dst_v,
                           int width) {
  ptrdiff_t temp_width = (ptrdiff_t)(width);
  asm volatile(
      "vbroadcasti128 %4,%%ymm1                  \n"
      "lea         -0x20(%0,%3,2),%0             \n"
      "sub         %1,%2                         \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "lea         -0x20(%0),%0                  \n"
      "vpshufb     %%ymm1,%%ymm0,%%ymm0          \n"
      "vpermq      $0x72,%%ymm0,%%ymm0           \n"
      "vextracti128 $0x1,%%ymm0,%%xmm2           \n"
      "vmovdqu     %%xmm0,(%1)                   \n"
      "vmovdqu     %%xmm2,0x00(%1,%2,1)          \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x10,%3                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src),                  // %0
        "+r"(dst_u),                // %1
        "+r"(dst_v),                // %2
        "+r"(temp_width)            // %3
      : "m"(kShuffleMirrorSplitUV)  // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_MIRRORSPLITUVROW_AVX2

#ifdef HAS_MIRRORUVROW_SSSE3
// Shuffle table for reversing the UV.
static const uvec8 kShuffleMirrorUV = {14u, 15u, 12u, 13u, 10u, 11u, 8u, 9u,
                                       6u,  7u,  4u,  5u,  2u,  3u,  0u, 1u};

void MirrorUVRow_SSSE3(const uint8_t* src_uv, uint8_t* dst_uv, int width) {
  ptrdiff_t temp_width = (ptrdiff_t)(width);
  asm volatile("movdqa      %3,%%xmm5                     \n"

               LABELALIGN
               "1:          \n"
               "movdqu      -0x10(%0,%2,2),%%xmm0         \n"
               "pshufb      %%xmm5,%%xmm0                 \n"
               "movdqu      %%xmm0,(%1)                   \n"
               "lea         0x10(%1),%1                   \n"
               "sub         $0x8,%2                       \n"
               "jg          1b                            \n"
               : "+r"(src_uv),          // %0
                 "+r"(dst_uv),          // %1
                 "+r"(temp_width)       // %2
               : "m"(kShuffleMirrorUV)  // %3
               : "memory", "cc", "xmm0", "xmm5");
}
#endif  // HAS_MIRRORUVROW_SSSE3

#ifdef HAS_MIRRORUVROW_AVX2
void MirrorUVRow_AVX2(const uint8_t* src_uv, uint8_t* dst_uv, int width) {
  ptrdiff_t temp_width = (ptrdiff_t)(width);
  asm volatile("vbroadcasti128 %3,%%ymm5                  \n"

               LABELALIGN
               "1:          \n"
               "vmovdqu     -0x20(%0,%2,2),%%ymm0         \n"
               "vpshufb     %%ymm5,%%ymm0,%%ymm0          \n"
               "vpermq      $0x4e,%%ymm0,%%ymm0           \n"
               "vmovdqu     %%ymm0,(%1)                   \n"
               "lea         0x20(%1),%1                   \n"
               "sub         $0x10,%2                      \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : "+r"(src_uv),          // %0
                 "+r"(dst_uv),          // %1
                 "+r"(temp_width)       // %2
               : "m"(kShuffleMirrorUV)  // %3
               : "memory", "cc", "xmm0", "xmm5");
}
#endif  // HAS_MIRRORUVROW_AVX2

#ifdef HAS_MIRRORUVROW_AVX512BW
static const uint16_t kMirrorUVPermute_AVX512[32] = {
    31u, 30u, 29u, 28u, 27u, 26u, 25u, 24u, 23u, 22u, 21u,
    20u, 19u, 18u, 17u, 16u, 15u, 14u, 13u, 12u, 11u, 10u,
    9u,  8u,  7u,  6u,  5u,  4u,  3u,  2u,  1u,  0u};
void MirrorUVRow_AVX512BW(const uint8_t* src_uv, uint8_t* dst_uv, int width) {
  ptrdiff_t temp_width = (ptrdiff_t)(width);
  asm volatile("vmovdqu16   %3,%%zmm5                     \n"

               LABELALIGN
               "1:          \n"
               "vpermw      -0x40(%0,%2,2),%%zmm5,%%zmm0  \n"
               "vmovdqu16   %%zmm0,(%1)                   \n"
               "lea         0x40(%1),%1                   \n"
               "sub         $0x20,%2                      \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : "+r"(src_uv),                 // %0
                 "+r"(dst_uv),                 // %1
                 "+r"(temp_width)              // %2
               : "m"(kMirrorUVPermute_AVX512)  // %3
               : "memory", "cc", "xmm0", "xmm5");
}
#endif  // HAS_MIRRORUVROW_AVX512BW

#ifdef HAS_RGB24MIRRORROW_SSSE3

// Shuffle first 5 pixels to last 5 mirrored.  first byte zero
static const uvec8 kShuffleMirrorRGB0 = {128u, 12u, 13u, 14u, 9u, 10u, 11u, 6u,
                                         7u,   8u,  3u,  4u,  5u, 0u,  1u,  2u};

// Shuffle last 5 pixels to first 5 mirrored.  last byte zero
static const uvec8 kShuffleMirrorRGB1 = {
    13u, 14u, 15u, 10u, 11u, 12u, 7u, 8u, 9u, 4u, 5u, 6u, 1u, 2u, 3u, 128u};

// Shuffle 5 pixels at a time (15 bytes)
void RGB24MirrorRow_SSSE3(const uint8_t* src_rgb24,
                          uint8_t* dst_rgb24,
                          int width) {
  ptrdiff_t temp_width = (ptrdiff_t)(width);
  src_rgb24 += width * 3 - 48;
  asm volatile(
      "movdqa      %3,%%xmm4                     \n"
      "movdqa      %4,%%xmm5                     \n"

      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"  // first 5
      "movdqu      15(%0),%%xmm1                 \n"  // next 5
      "movdqu      30(%0),%%xmm2                 \n"  // next 5
      "movdqu      32(%0),%%xmm3                 \n"  // last 1 special
      "pshufb      %%xmm4,%%xmm0                 \n"
      "pshufb      %%xmm4,%%xmm1                 \n"
      "pshufb      %%xmm4,%%xmm2                 \n"
      "pshufb      %%xmm5,%%xmm3                 \n"
      "lea         -0x30(%0),%0                  \n"
      "movdqu      %%xmm0,32(%1)                 \n"  // last 5
      "movdqu      %%xmm1,17(%1)                 \n"  // next 5
      "movdqu      %%xmm2,2(%1)                  \n"  // next 5
      "movlpd      %%xmm3,0(%1)                  \n"  // first 1
      "lea         0x30(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_rgb24),          // %0
        "+r"(dst_rgb24),          // %1
        "+r"(temp_width)          // %2
      : "m"(kShuffleMirrorRGB0),  // %3
        "m"(kShuffleMirrorRGB1)   // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_RGB24MIRRORROW_SSSE3

#ifdef HAS_RGB24MIRRORROW_AVX2
// Shuffle first 10 pixels to last 10 mirrored.  first byte zero
static const uvec8 kShuffleMirrorRGB0_AVX = {
    128u, 12u, 13u, 14u, 9u, 10u, 11u, 6u, 7u, 8u, 3u, 4u, 5u, 0u, 1u, 2u};

// Shuffle last 2 pixels to first 2 mirrored.  last byte zero
static const uvec8 kShuffleMirrorRGB1_AVX = {
    13u, 14u, 15u, 10u, 11u, 12u, 7u, 8u, 9u, 4u, 5u, 6u, 1u, 2u, 3u, 128u};

void RGB24MirrorRow_AVX2(const uint8_t* src_rgb24,
                         uint8_t* dst_rgb24,
                         int width) {
  ptrdiff_t temp_width = (ptrdiff_t)(width);
  src_rgb24 += width * 3 - 96;
  asm volatile(
      "vbroadcasti128 %3,%%ymm4                  \n"
      "vmovdqa     %4,%%xmm5                     \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%xmm0                   \n"  // first 10
      "vinserti128 $1,15(%0),%%ymm0,%%ymm0       \n"
      "vmovdqu     30(%0),%%xmm1                 \n"  // next 10
      "vinserti128 $1,45(%0),%%ymm1,%%ymm1       \n"
      "vmovdqu     60(%0),%%xmm2                 \n"  // next 10
      "vinserti128 $1,75(%0),%%ymm2,%%ymm2       \n"
      "vmovdqu     80(%0),%%xmm3                 \n"  // last 2 special
      "vpshufb     %%ymm4,%%ymm0,%%ymm0          \n"
      "vpshufb     %%ymm4,%%ymm1,%%ymm1          \n"
      "vpshufb     %%ymm4,%%ymm2,%%ymm2          \n"
      "vpshufb     %%xmm5,%%xmm3,%%xmm3          \n"
      "lea         -0x60(%0),%0                  \n"
      "vmovdqu     %%xmm0,80(%1)                 \n"
      "vextracti128 $1,%%ymm0,65(%1)             \n"
      "vmovdqu     %%xmm1,50(%1)                 \n"
      "vextracti128 $1,%%ymm1,35(%1)             \n"
      "vmovdqu     %%xmm2,20(%1)                 \n"
      "vextracti128 $1,%%ymm2,5(%1)              \n"
      "vmovq       %%xmm3,0(%1)                  \n"
      "lea         0x60(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_rgb24),              // %0
        "+r"(dst_rgb24),              // %1
        "+r"(temp_width)              // %2
      : "m"(kShuffleMirrorRGB0_AVX),  // %3
        "m"(kShuffleMirrorRGB1_AVX)   // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_RGB24MIRRORROW_AVX2

#ifdef HAS_RGB24MIRRORROW_AVX512VBMI
// Mirror 64 RGB24 pixels (192 bytes = 3 zmm) per loop.  Loads and stores are
// 3 full contiguous zmm vectors.  Output vector 0 is a 2 source byte permute
// of input vectors 1 and 2, and output vectors 1 and 2 are 2 source byte
// permutes of input vectors 0 and 1.  Output byte 65 (byte 1 of output
// vector 1) is the only byte that comes from a third vector: byte 0 of input
// vector 2, which is merged in with a masked broadcast.
static const uint8_t kPermMirrorRGB24_0[64] = {
    125u, 126u, 127u, 122u, 123u, 124u, 119u, 120u, 121u, 116u, 117u,
    118u, 113u, 114u, 115u, 110u, 111u, 112u, 107u, 108u, 109u, 104u,
    105u, 106u, 101u, 102u, 103u, 98u,  99u,  100u, 95u,  96u,  97u,
    92u,  93u,  94u,  89u,  90u,  91u,  86u,  87u,  88u,  83u,  84u,
    85u,  80u,  81u,  82u,  77u,  78u,  79u,  74u,  75u,  76u,  71u,
    72u,  73u,  68u,  69u,  70u,  65u,  66u,  67u,  62u};
static const uint8_t kPermMirrorRGB24_1[64] = {
    127u, 128u, 123u, 124u, 125u, 120u, 121u, 122u, 117u, 118u, 119u,
    114u, 115u, 116u, 111u, 112u, 113u, 108u, 109u, 110u, 105u, 106u,
    107u, 102u, 103u, 104u, 99u,  100u, 101u, 96u,  97u,  98u,  93u,
    94u,  95u,  90u,  91u,  92u,  87u,  88u,  89u,  84u,  85u,  86u,
    81u,  82u,  83u,  78u,  79u,  80u,  75u,  76u,  77u,  72u,  73u,
    74u,  69u,  70u,  71u,  66u,  67u,  68u,  63u,  64u};
static const uint8_t kPermMirrorRGB24_2[64] = {
    65u, 60u, 61u, 62u, 57u, 58u, 59u, 54u, 55u, 56u, 51u, 52u, 53u,
    48u, 49u, 50u, 45u, 46u, 47u, 42u, 43u, 44u, 39u, 40u, 41u, 36u,
    37u, 38u, 33u, 34u, 35u, 30u, 31u, 32u, 27u, 28u, 29u, 24u, 25u,
    26u, 21u, 22u, 23u, 18u, 19u, 20u, 15u, 16u, 17u, 12u, 13u, 14u,
    9u,  10u, 11u, 6u,  7u,  8u,  3u,  4u,  5u,  0u,  1u,  2u};

// Byte mask with the low n bits set, clamped to [0, 64].
static inline uint64_t RGB24MirrorMask(int n) {
  return n <= 0 ? 0ull : n >= 64 ? ~0ull : (1ull << n) - 1ull;
}

void RGB24MirrorRow_AVX512VBMI(const uint8_t* src_rgb24,
                               uint8_t* dst_rgb24,
                               int width) {
  int r = width & 63;
  ptrdiff_t temp_width = (ptrdiff_t)(width - r);
  if (temp_width > 0) {
    const uint8_t* src = src_rgb24 + (ptrdiff_t)width * 3 - 192;
    asm volatile(
        "vmovdqu8    %3,%%zmm20                  \n"
        "vmovdqu8    %4,%%zmm21                  \n"
        "vmovdqu8    %5,%%zmm22                  \n"
        "kmovq       %6,%%k1                     \n"

        LABELALIGN
        "1:          \n"
        "vmovdqu8    (%0),%%zmm0                 \n"  // last 64 pixels
        "vmovdqu8    0x40(%0),%%zmm1             \n"
        "vmovdqu8    0x80(%0),%%zmm2             \n"
        "lea         -0xc0(%0),%0                \n"
        "vmovdqa64   %%zmm20,%%zmm3              \n"
        "vmovdqa64   %%zmm21,%%zmm4              \n"
        "vmovdqa64   %%zmm22,%%zmm5              \n"
        "vpermi2b    %%zmm2,%%zmm1,%%zmm3        \n"  // out0 from in1:in2
        "vpermi2b    %%zmm1,%%zmm0,%%zmm4        \n"  // out1 from in0:in1
        "vpermi2b    %%zmm1,%%zmm0,%%zmm5        \n"  // out2 from in0:in1
        "vpbroadcastb %%xmm2,%%zmm4%{%%k1%}      \n"  // out1 byte 1 = in2[0]
        "vmovdqu8    %%zmm3,(%1)                 \n"
        "vmovdqu8    %%zmm4,0x40(%1)             \n"
        "vmovdqu8    %%zmm5,0x80(%1)             \n"
        "lea         0xc0(%1),%1                 \n"
        "sub         $0x40,%2                    \n"
        "jg          1b                          \n"
        "vzeroupper  \n"
        : "+r"(src),                // %0
          "+r"(dst_rgb24),          // %1
          "+r"(temp_width)          // %2
        : "m"(kPermMirrorRGB24_0),  // %3
          "m"(kPermMirrorRGB24_1),  // %4
          "m"(kPermMirrorRGB24_2),  // %5
          "r"((uint64_t)2)          // %6
        : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5",
          "xmm20", "xmm21", "xmm22", "k1");
  }
  // Remaining 1 to 63 pixels are the first r pixels of the source.  Masked
  // loads and stores access only the 3 * r valid bytes.  The tables, as
  // global byte offsets into a 192 byte block, are adjusted by the 192 - 3 * r
  // missing bytes.  Bytes from the third vector (index >= 128) are selected
  // with a mask from the sign bit of the index.
  if (r) {
    int nb = r * 3;
    int off = 192 - nb;
    asm volatile(
        "kmovq       %2,%%k1                     \n"
        "kmovq       %3,%%k2                     \n"
        "kmovq       %4,%%k3                     \n"
        "vmovdqu8    (%0),%%zmm0%{%%k1%}%{z%}    \n"
        "vmovdqu8    0x40(%0),%%zmm1%{%%k2%}%{z%}\n"
        "vmovdqu8    0x80(%0),%%zmm2%{%%k3%}%{z%}\n"
        "vpbroadcastb %5,%%zmm6                  \n"  // off
        "vpbroadcastb %6,%%zmm7                  \n"  // off - 64
        "vmovdqu8    %7,%%zmm3                   \n"
        "vmovdqu8    %8,%%zmm4                   \n"
        "vmovdqu8    %9,%%zmm5                   \n"
        "vpsubb      %%zmm7,%%zmm3,%%zmm3        \n"
        "vpsubb      %%zmm6,%%zmm4,%%zmm4        \n"
        "vpsubb      %%zmm6,%%zmm5,%%zmm5        \n"

        "vpmovb2m    %%zmm3,%%k4                 \n"  // out0
        "vmovdqa64   %%zmm3,%%zmm6               \n"
        "vpermi2b    %%zmm1,%%zmm0,%%zmm6        \n"
        "vpermb      %%zmm2,%%zmm3,%%zmm6%{%%k4%}\n"
        "vmovdqu8    %%zmm6,(%1)%{%%k1%}         \n"
        "vpmovb2m    %%zmm4,%%k4                 \n"  // out1
        "vmovdqa64   %%zmm4,%%zmm6               \n"
        "vpermi2b    %%zmm1,%%zmm0,%%zmm6        \n"
        "vpermb      %%zmm2,%%zmm4,%%zmm6%{%%k4%}\n"
        "vmovdqu8    %%zmm6,0x40(%1)%{%%k2%}     \n"
        "vpmovb2m    %%zmm5,%%k4                 \n"  // out2
        "vmovdqa64   %%zmm5,%%zmm6               \n"
        "vpermi2b    %%zmm1,%%zmm0,%%zmm6        \n"
        "vpermb      %%zmm2,%%zmm5,%%zmm6%{%%k4%}\n"
        "vmovdqu8    %%zmm6,0x80(%1)%{%%k3%}     \n"
        "vzeroupper  \n"
        :
        : "r"(src_rgb24),                  // %0
          "r"(dst_rgb24),                  // %1
          "r"(RGB24MirrorMask(nb)),        // %2
          "r"(RGB24MirrorMask(nb - 64)),   // %3
          "r"(RGB24MirrorMask(nb - 128)),  // %4
          "r"(off),                        // %5
          "r"(off - 64),                   // %6
          "m"(kPermMirrorRGB24_0),         // %7
          "m"(kPermMirrorRGB24_1),         // %8
          "m"(kPermMirrorRGB24_2)          // %9
        : "memory", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
          "xmm7", "k1", "k2", "k3", "k4");
  }
}
#endif  // HAS_RGB24MIRRORROW_AVX512VBMI

#ifdef HAS_ARGBMIRRORROW_SSE2

void ARGBMirrorRow_SSE2(const uint8_t* src, uint8_t* dst, int width) {
  ptrdiff_t temp_width = (ptrdiff_t)(width);
  asm volatile("lea         -0x10(%0,%2,4),%0             \n"

               LABELALIGN
               "1:          \n"
               "movdqu      (%0),%%xmm0                   \n"
               "pshufd      $0x1b,%%xmm0,%%xmm0           \n"
               "lea         -0x10(%0),%0                  \n"
               "movdqu      %%xmm0,(%1)                   \n"
               "lea         0x10(%1),%1                   \n"
               "sub         $0x4,%2                       \n"
               "jg          1b                            \n"
               : "+r"(src),        // %0
                 "+r"(dst),        // %1
                 "+r"(temp_width)  // %2
               :
               : "memory", "cc", "xmm0");
}
#endif  // HAS_ARGBMIRRORROW_SSE2

#ifdef HAS_ARGBMIRRORROW_AVX2
// Shuffle table for reversing the bytes.
static const ulvec32 kARGBShuffleMirror_AVX2 = {7u, 6u, 5u, 4u, 3u, 2u, 1u, 0u};
void ARGBMirrorRow_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  ptrdiff_t temp_width = (ptrdiff_t)(width);
  asm volatile("vmovdqu     %3,%%ymm5                     \n"

               LABELALIGN
               "1:          \n"
               "vpermd      -0x20(%0,%2,4),%%ymm5,%%ymm0  \n"
               "vmovdqu     %%ymm0,(%1)                   \n"
               "lea         0x20(%1),%1                   \n"
               "sub         $0x8,%2                       \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : "+r"(src),                    // %0
                 "+r"(dst),                    // %1
                 "+r"(temp_width)              // %2
               : "m"(kARGBShuffleMirror_AVX2)  // %3
               : "memory", "cc", "xmm0", "xmm5");
}
#endif  // HAS_ARGBMIRRORROW_AVX2

#ifdef HAS_ARGBMIRRORROW_AVX512BW
static const uint32_t kARGBShuffleMirror_AVX512[16] = {
    15u, 14u, 13u, 12u, 11u, 10u, 9u, 8u, 7u, 6u, 5u, 4u, 3u, 2u, 1u, 0u};

// Mirror 16 ARGB pixels per loop.  The remaining 1 to 15 pixels are at the
// start of src.  They are loaded with a k mask, permuted with the reverse
// index minus (16 - remaining), and stored with the same k mask.
void ARGBMirrorRow_AVX512BW(const uint8_t* src, uint8_t* dst, int width) {
  ptrdiff_t temp_width = (ptrdiff_t)(width);
  uintptr_t temp;
  asm volatile(
      "vmovdqu32   %4,%%zmm5                     \n"
      "sub         $0x10,%2                      \n"
      "jl          2f                            \n"

      LABELALIGN
      "1:          \n"
      "vpermd      (%0,%2,4),%%zmm5,%%zmm0       \n"
      "vmovdqu32   %%zmm0,(%1)                   \n"
      "lea         0x40(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jge         1b                            \n"

      "2:          \n"
      "add         $0x10,%2                      \n"
      "jle         99f                           \n"
      "mov         $-1,%k3                       \n"
      "bzhi        %k2,%k3,%k3                   \n"
      "kmovw       %k3,%%k1                      \n"
      "mov         $0x10,%k3                     \n"
      "sub         %k2,%k3                       \n"
      "vpbroadcastd %k3,%%zmm1                   \n"
      "vpsubd      %%zmm1,%%zmm5,%%zmm1          \n"
      "vmovdqu32   (%0),%%zmm0%{%%k1%}%{z%}      \n"
      "vpermd      %%zmm0,%%zmm1,%%zmm0          \n"
      "vmovdqu32   %%zmm0,(%1)%{%%k1%}           \n"

      "99:         \n"
      "vzeroupper  \n"
      : "+r"(src),                      // %0
        "+r"(dst),                      // %1
        "+r"(temp_width),               // %2
        "=&r"(temp)                     // %3
      : "m"(kARGBShuffleMirror_AVX512)  // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm5", "k1");
}
#endif  // HAS_ARGBMIRRORROW_AVX512BW

#ifdef HAS_SPLITUVROW_AVX2
void SplitUVRow_AVX2(const uint8_t* src_uv,
                     uint8_t* dst_u,
                     uint8_t* dst_v,
                     int width) {
  asm volatile(
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"
      "vpsrlw      $0x8,%%ymm5,%%ymm5            \n"
      "sub         %1,%2                         \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "lea         0x40(%0),%0                   \n"
      "vpsrlw      $0x8,%%ymm0,%%ymm2            \n"
      "vpsrlw      $0x8,%%ymm1,%%ymm3            \n"
      "vpand       %%ymm5,%%ymm0,%%ymm0          \n"
      "vpand       %%ymm5,%%ymm1,%%ymm1          \n"
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpackuswb   %%ymm3,%%ymm2,%%ymm2          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vpermq      $0xd8,%%ymm2,%%ymm2           \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "vmovdqu     %%ymm2,0x00(%1,%2,1)          \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x20,%3                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_uv),  // %0
        "+r"(dst_u),   // %1
        "+r"(dst_v),   // %2
        "+r"(width)    // %3
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm5");
}
#endif  // HAS_SPLITUVROW_AVX2

#ifdef HAS_SPLITUVROW_AVX512BW
static const uint64_t kSplitUVPermute[8] = {0, 2, 4, 6, 1, 3, 5, 7};

void SplitUVRow_AVX512BW(const uint8_t* src_uv,
                         uint8_t* dst_u,
                         uint8_t* dst_v,
                         int width) {
  asm volatile(
      "vpternlogd  $0xff,%%zmm5,%%zmm5,%%zmm5    \n"
      "vpsrlw      $0x8,%%zmm5,%%zmm5            \n"
      "vmovdqu64   %4,%%zmm4                     \n"
      "sub         %1,%2                         \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu8    (%0),%%zmm0                   \n"
      "vmovdqu8    0x40(%0),%%zmm1               \n"
      "lea         0x80(%0),%0                   \n"
      "vpsrlw      $0x8,%%zmm0,%%zmm2            \n"
      "vpsrlw      $0x8,%%zmm1,%%zmm3            \n"
      "vpandd      %%zmm5,%%zmm0,%%zmm0          \n"
      "vpandd      %%zmm5,%%zmm1,%%zmm1          \n"
      "vpackuswb   %%zmm1,%%zmm0,%%zmm0          \n"
      "vpackuswb   %%zmm3,%%zmm2,%%zmm2          \n"
      "vpermq      %%zmm0,%%zmm4,%%zmm0          \n"
      "vpermq      %%zmm2,%%zmm4,%%zmm2          \n"
      "vmovdqu8    %%zmm0,(%1)                   \n"
      "vmovdqu8    %%zmm2,0x00(%1,%2,1)          \n"
      "lea         0x40(%1),%1                   \n"
      "sub         $0x40,%3                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_uv),         // %0
        "+r"(dst_u),          // %1
        "+r"(dst_v),          // %2
        "+r"(width)           // %3
      : "m"(kSplitUVPermute)  // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_SPLITUVROW_AVX512BW

#ifdef HAS_SPLITUVROW_SSE2
void SplitUVRow_SSE2(const uint8_t* src_uv,
                     uint8_t* dst_u,
                     uint8_t* dst_v,
                     int width) {
  asm volatile(
      "pcmpeqb     %%xmm5,%%xmm5                 \n"
      "psrlw       $0x8,%%xmm5                   \n"
      "sub         %1,%2                         \n"

      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "lea         0x20(%0),%0                   \n"
      "movdqa      %%xmm0,%%xmm2                 \n"
      "movdqa      %%xmm1,%%xmm3                 \n"
      "pand        %%xmm5,%%xmm0                 \n"
      "pand        %%xmm5,%%xmm1                 \n"
      "packuswb    %%xmm1,%%xmm0                 \n"
      "psrlw       $0x8,%%xmm2                   \n"
      "psrlw       $0x8,%%xmm3                   \n"
      "packuswb    %%xmm3,%%xmm2                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "movdqu      %%xmm2,0x00(%1,%2,1)          \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x10,%3                      \n"
      "jg          1b                            \n"
      : "+r"(src_uv),  // %0
        "+r"(dst_u),   // %1
        "+r"(dst_v),   // %2
        "+r"(width)    // %3
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm5");
}
#endif  // HAS_SPLITUVROW_SSE2

#ifdef HAS_DETILEROW_SSE2
void DetileRow_SSE2(const uint8_t* src,
                    ptrdiff_t src_tile_stride,
                    uint8_t* dst,
                    int width) {
  asm volatile(
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "sub         $0x10,%2                      \n"
      "lea         (%0,%3),%0                    \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "lea         0x10(%1),%1                   \n"
      "jg          1b                            \n"
      : "+r"(src),            // %0
        "+r"(dst),            // %1
        "+r"(width)           // %2
      : "r"(src_tile_stride)  // %3
      : "cc", "memory", "xmm0");
}
#endif  // HAS_DETILEROW_SSE2

#ifdef HAS_DETILEROW_16_SSE2
void DetileRow_16_SSE2(const uint16_t* src,
                       ptrdiff_t src_tile_stride,
                       uint16_t* dst,
                       int width) {
  asm volatile(
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "lea         (%0,%3,2),%0                  \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "movdqu      %%xmm1,0x10(%1)               \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src),            // %0
        "+r"(dst),            // %1
        "+r"(width)           // %2
      : "r"(src_tile_stride)  // %3
      : "cc", "memory", "xmm0", "xmm1");
}
#endif  // HAS_DETILEROW_16_SSE2

#ifdef HAS_DETILEROW_16_AVX
void DetileRow_16_AVX(const uint16_t* src,
                      ptrdiff_t src_tile_stride,
                      uint16_t* dst,
                      int width) {
  asm volatile(
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "lea         (%0,%3,2),%0                  \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src),            // %0
        "+r"(dst),            // %1
        "+r"(width)           // %2
      : "r"(src_tile_stride)  // %3
      : "cc", "memory", "xmm0");
}
#endif  // HAS_DETILEROW_16_AVX

#ifdef HAS_DETILETOYUY2_SSE2
// Read 16 Y, 8 UV, and write 8 YUYV.
void DetileToYUY2_SSE2(const uint8_t* src_y,
                       ptrdiff_t src_y_tile_stride,
                       const uint8_t* src_uv,
                       ptrdiff_t src_uv_tile_stride,
                       uint8_t* dst_yuy2,
                       int width) {
  asm volatile(
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"  // Load 16 Y
      "sub         $0x10,%3                      \n"
      "lea         (%0,%4),%0                    \n"
      "movdqu      (%1),%%xmm1                   \n"  // Load 8 UV
      "lea         (%1,%5),%1                    \n"
      "movdqu      %%xmm0,%%xmm2                 \n"
      "punpcklbw   %%xmm1,%%xmm0                 \n"
      "punpckhbw   %%xmm1,%%xmm2                 \n"
      "movdqu      %%xmm0,(%2)                   \n"
      "movdqu      %%xmm2,0x10(%2)               \n"
      "lea         0x20(%2),%2                   \n"
      "jg          1b                            \n"
      : "+r"(src_y),                            // %0
        "+r"(src_uv),                           // %1
        "+r"(dst_yuy2),                         // %2
        "+r"(width)                             // %3
      : "r"(src_y_tile_stride),                 // %4
        "r"(src_uv_tile_stride)                 // %5
      : "cc", "memory", "xmm0", "xmm1", "xmm2"  // Clobber list
  );
}
#endif  // HAS_DETILETOYUY2_SSE2

#ifdef HAS_DETILESPLITUVROW_SSSE3
// TODO(greenjustin): Look into generating these constants instead of loading
// them since this can cause branch mispredicts for fPIC code on 32-bit
// machines.
static const uvec8 kDeinterlaceUV = {0, 2, 4, 6, 8, 10, 12, 14,
                                     1, 3, 5, 7, 9, 11, 13, 15};

// TODO(greenjustin): Research alternatives to pshufb, since pshufb can be very
// slow on older SSE2 processors.
void DetileSplitUVRow_SSSE3(const uint8_t* src_uv,
                            ptrdiff_t src_tile_stride,
                            uint8_t* dst_u,
                            uint8_t* dst_v,
                            int width) {
  asm volatile(
      "movdqu      %4,%%xmm1                     \n"
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "lea         (%0, %5),%0                   \n"
      "pshufb      %%xmm1,%%xmm0                 \n"
      "movq        %%xmm0,(%1)                   \n"
      "lea         0x8(%1),%1                    \n"
      "movhps      %%xmm0,(%2)                   \n"
      "lea         0x8(%2),%2                    \n"
      "sub         $0x10,%3                      \n"
      "jg          1b                            \n"
      : "+r"(src_uv),         // %0
        "+r"(dst_u),          // %1
        "+r"(dst_v),          // %2
        "+r"(width)           // %3
      : "m"(kDeinterlaceUV),  // %4
        "r"(src_tile_stride)  // %5
      : "cc", "memory", "xmm0", "xmm1");
}
#endif  // HAS_DETILESPLITUVROW_SSSE3

#ifdef HAS_MERGEUVROW_AVX512BW
void MergeUVRow_AVX512BW(const uint8_t* src_u,
                         const uint8_t* src_v,
                         uint8_t* dst_uv,
                         int width) {
  asm volatile("sub         %0,%1                         \n"

               LABELALIGN
               "1:          \n"
               "vpmovzxbw   (%0),%%zmm0                   \n"
               "vpmovzxbw   0x00(%0,%1,1),%%zmm1          \n"
               "lea         0x20(%0),%0                   \n"
               "vpsllw      $0x8,%%zmm1,%%zmm1            \n"
               "vporq       %%zmm0,%%zmm1,%%zmm2          \n"
               "vmovdqu64   %%zmm2,(%2)                   \n"
               "lea         0x40(%2),%2                   \n"
               "sub         $0x20,%3                      \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : "+r"(src_u),   // %0
                 "+r"(src_v),   // %1
                 "+r"(dst_uv),  // %2
                 "+r"(width)    // %3
               :
               : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_MERGEUVROW_AVX512BW

#ifdef HAS_MERGEUVROW_AVX2
void MergeUVRow_AVX2(const uint8_t* src_u,
                     const uint8_t* src_v,
                     uint8_t* dst_uv,
                     int width) {
  asm volatile("sub         %0,%1                         \n"

               LABELALIGN
               "1:          \n"
               "vpmovzxbw   (%0),%%ymm0                   \n"
               "vpmovzxbw   0x00(%0,%1,1),%%ymm1          \n"
               "lea         0x10(%0),%0                   \n"
               "vpsllw      $0x8,%%ymm1,%%ymm1            \n"
               "vpor        %%ymm0,%%ymm1,%%ymm2          \n"
               "vmovdqu     %%ymm2,(%2)                   \n"
               "lea         0x20(%2),%2                   \n"
               "sub         $0x10,%3                      \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : "+r"(src_u),   // %0
                 "+r"(src_v),   // %1
                 "+r"(dst_uv),  // %2
                 "+r"(width)    // %3
               :
               : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_MERGEUVROW_AVX2

#ifdef HAS_MERGEUVROW_SSE2
void MergeUVRow_SSE2(const uint8_t* src_u,
                     const uint8_t* src_v,
                     uint8_t* dst_uv,
                     int width) {
  asm volatile("sub         %0,%1                         \n"

               LABELALIGN
               "1:          \n"
               "movdqu      (%0),%%xmm0                   \n"
               "movdqu      0x00(%0,%1,1),%%xmm1          \n"
               "lea         0x10(%0),%0                   \n"
               "movdqa      %%xmm0,%%xmm2                 \n"
               "punpcklbw   %%xmm1,%%xmm0                 \n"
               "punpckhbw   %%xmm1,%%xmm2                 \n"
               "movdqu      %%xmm0,(%2)                   \n"
               "movdqu      %%xmm2,0x10(%2)               \n"
               "lea         0x20(%2),%2                   \n"
               "sub         $0x10,%3                      \n"
               "jg          1b                            \n"
               : "+r"(src_u),   // %0
                 "+r"(src_v),   // %1
                 "+r"(dst_uv),  // %2
                 "+r"(width)    // %3
               :
               : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_MERGEUVROW_SSE2

#ifdef HAS_MERGEUVROW_16_AVX2
void MergeUVRow_16_AVX2(const uint16_t* src_u,
                        const uint16_t* src_v,
                        uint16_t* dst_uv,
                        int depth,
                        int width) {
  asm volatile(
      "vmovd       %4,%%xmm3                     \n"
      "vmovd       %5,%%xmm4                     \n"

      "sub         %0,%1                         \n"
      // 8 pixels per loop.

      LABELALIGN
      "1:          \n"
      "vpmovzxwd   (%0),%%ymm0                   \n"
      "vpmovzxwd   0x00(%0,%1,1),%%ymm1          \n"
      "lea         0x10(%0),%0                   \n"
      "vpsllw      %%xmm3,%%ymm0,%%ymm0          \n"
      "vpslld      %%xmm4,%%ymm1,%%ymm1          \n"
      "vpor        %%ymm0,%%ymm1,%%ymm2          \n"
      "vmovdqu     %%ymm2,(%2)                   \n"
      "lea         0x20(%2),%2                   \n"
      "sub         $0x8,%3                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_u),      // %0
        "+r"(src_v),      // %1
        "+r"(dst_uv),     // %2
        "+r"(width)       // %3
      : "r"(16 - depth),  // %4
        "r"(32 - depth)   // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4");
}
#endif  // HAS_MERGEUVROW_16_AVX2

#ifdef HAS_SPLITUVROW_16_AVX2
const uvec8 kSplitUVShuffle16 = {0, 1, 4, 5, 8,  9,  12, 13,
                                 2, 3, 6, 7, 10, 11, 14, 15};
void SplitUVRow_16_AVX2(const uint16_t* src_uv,
                        uint16_t* dst_u,
                        uint16_t* dst_v,
                        int depth,
                        int width) {
  depth = 16 - depth;
  asm volatile(
      "vmovd       %4,%%xmm3                     \n"
      "vbroadcasti128 %5,%%ymm4                  \n"
      "sub         %1,%2                         \n"

      // 16 pixels per loop.
      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "add         $0x40,%0                      \n"

      "vpsrlw      %%xmm3,%%ymm0,%%ymm0          \n"
      "vpsrlw      %%xmm3,%%ymm1,%%ymm1          \n"
      "vpshufb     %%ymm4,%%ymm0,%%ymm0          \n"
      "vpshufb     %%ymm4,%%ymm1,%%ymm1          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vpermq      $0xd8,%%ymm1,%%ymm1           \n"
      "vextractf128 $0x0,%%ymm0,(%1)             \n"
      "vextractf128 $0x0,%%ymm1,0x10(%1)         \n"
      "vextractf128 $0x1,%%ymm0,(%1,%2)          \n"
      "vextractf128 $0x1,%%ymm1,0x10(%1,%2)      \n"
      "add         $0x20,%1                      \n"
      "sub         $0x10,%3                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_uv),           // %0
        "+r"(dst_u),            // %1
        "+r"(dst_v),            // %2
        "+r"(width)             // %3
      : "r"(depth),             // %4
        "m"(kSplitUVShuffle16)  // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4");
}
#endif  // HAS_SPLITUVROW_16_AVX2

// Use scale to convert lsb formats to msb, depending how many bits there are:
// 128 = 9 bits
// 64 = 10 bits
// 16 = 12 bits
// 1 = 16 bits
#ifdef HAS_MULTIPLYROW_16_AVX2
void MultiplyRow_16_AVX2(const uint16_t* src_y,
                         uint16_t* dst_y,
                         int scale,
                         int width) {
  asm volatile(
      "vmovd       %3,%%xmm3                     \n"
      "vpbroadcastw %%xmm3,%%ymm3                \n"
      "sub         %0,%1                         \n"

      // 32 pixels per loop.
      LABELALIGN
      "1:          \n"
      "vpmullw     (%0),%%ymm3,%%ymm0            \n"
      "vpmullw     0x20(%0),%%ymm3,%%ymm1        \n"
      "vmovdqu     %%ymm0,(%0,%1)                \n"
      "vmovdqu     %%ymm1,0x20(%0,%1)            \n"
      "add         $0x40,%0                      \n"
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_y),  // %0
        "+r"(dst_y),  // %1
        "+r"(width)   // %2
      : "r"(scale)    // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm3");
}
#endif  // HAS_MULTIPLYROW_16_AVX2

#ifdef HAS_MULTIPLYROW_16_AVX512BW
void MultiplyRow_16_AVX512BW(const uint16_t* src_y,
                             uint16_t* dst_y,
                             int scale,
                             int width) {
  asm volatile("vpbroadcastw %3,%%zmm2                    \n"

               // 64 pixels per loop.
               LABELALIGN
               "1:          \n"
               "vpmullw     (%0),%%zmm2,%%zmm0            \n"
               "vpmullw     0x40(%0),%%zmm2,%%zmm1        \n"
               "add         $0x80,%0                      \n"
               "vmovdqu16   %%zmm0,(%1)                   \n"
               "vmovdqu16   %%zmm1,0x40(%1)               \n"
               "add         $0x80,%1                      \n"
               "sub         $0x40,%2                      \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : "+r"(src_y),  // %0
                 "+r"(dst_y),  // %1
                 "+r"(width)   // %2
               : "r"(scale)    // %3
               : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_MULTIPLYROW_16_AVX512BW

// Use scale to convert msb formats to lsb, depending how many bits there are:
// 512 = 9 bits
// 1024 = 10 bits
// 4096 = 12 bits
// 65536 = 16 bits
#ifdef HAS_DIVIDEROW_16_AVX2
void DivideRow_16_AVX2(const uint16_t* src_y,
                       uint16_t* dst_y,
                       int scale,
                       int width) {
  asm volatile(
      "vmovd       %3,%%xmm3                     \n"
      "vpbroadcastw %%xmm3,%%ymm3                \n"
      "sub         %0,%1                         \n"

      // 32 pixels per loop.
      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "vpmulhuw    %%ymm3,%%ymm0,%%ymm0          \n"
      "vpmulhuw    %%ymm3,%%ymm1,%%ymm1          \n"
      "vmovdqu     %%ymm0,(%0,%1)                \n"
      "vmovdqu     %%ymm1,0x20(%0,%1)            \n"
      "add         $0x40,%0                      \n"
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_y),  // %0
        "+r"(dst_y),  // %1
        "+r"(width),  // %2
        "+r"(scale)   // %3
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm3");
}
#endif  // HAS_DIVIDEROW_16_AVX2

// Use scale to convert lsb formats to msb, depending how many bits there are:
// 32768 = 9 bits
// 16384 = 10 bits
// 4096 = 12 bits
// 256 = 16 bits
void Convert16To8Row_SSSE3(const uint16_t* src_y,
                           uint8_t* dst_y,
                           int scale,
                           int width) {
  asm volatile(
      "movd        %3,%%xmm2                     \n"
      "punpcklwd   %%xmm2,%%xmm2                 \n"
      "pshufd      $0x0,%%xmm2,%%xmm2            \n"

      // 32 pixels per loop.
      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "add         $0x20,%0                      \n"
      "pmulhuw     %%xmm2,%%xmm0                 \n"
      "pmulhuw     %%xmm2,%%xmm1                 \n"
      "packuswb    %%xmm1,%%xmm0                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "add         $0x10,%1                      \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_y),  // %0
        "+r"(dst_y),  // %1
        "+r"(width)   // %2
      : "r"(scale)    // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2");
}

#ifdef HAS_CONVERT16TO8ROW_AVX2
void Convert16To8Row_AVX2(const uint16_t* src_y,
                          uint8_t* dst_y,
                          int scale,
                          int width) {
  asm volatile(
      "vmovd       %3,%%xmm2                     \n"
      "vpbroadcastw %%xmm2,%%ymm2                \n"

      // 32 pixels per loop.
      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "add         $0x40,%0                      \n"
      "vpmulhuw    %%ymm2,%%ymm0,%%ymm0          \n"
      "vpmulhuw    %%ymm2,%%ymm1,%%ymm1          \n"
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"  // mutates
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "add         $0x20,%1                      \n"
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_y),  // %0
        "+r"(dst_y),  // %1
        "+r"(width)   // %2
      : "r"(scale)    // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_CONVERT16TO8ROW_AVX2

#ifdef HAS_CONVERT16TO8ROW_AVX512BW
void Convert16To8Row_AVX512BW(const uint16_t* src_y,
                              uint8_t* dst_y,
                              int scale,
                              int width) {
  asm volatile("vpbroadcastw %3,%%zmm2                    \n"

               // 64 pixels per loop.
               LABELALIGN
               "1:          \n"
               "vmovups     (%0),%%zmm0                   \n"
               "vmovups     0x40(%0),%%zmm1               \n"
               "add         $0x80,%0                      \n"
               "vpmulhuw    %%zmm2,%%zmm0,%%zmm0          \n"
               "vpmulhuw    %%zmm2,%%zmm1,%%zmm1          \n"
               "vpmovuswb   %%zmm0,%%ymm0                 \n"
               "vpmovuswb   %%zmm1,%%ymm1                 \n"
               "vmovups     %%ymm0,(%1)                   \n"
               "vmovups     %%ymm1,0x20(%1)               \n"
               "add         $0x40,%1                      \n"
               "sub         $0x40,%2                      \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : "+r"(src_y),  // %0
                 "+r"(dst_y),  // %1
                 "+r"(width)   // %2
               : "r"(scale)    // %3
               : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_CONVERT16TO8ROW_AVX512BW

#ifdef HAS_HALFROW_16TO8_SSSE3
void HalfRow_16To8_SSSE3(const uint16_t* src_uv,
                         ptrdiff_t src_uv_stride,
                         uint8_t* dst_uv,
                         int scale,
                         int width) {
  asm volatile(
      "movd        %4,%%xmm2                     \n"
      "punpcklwd   %%xmm2,%%xmm2                 \n"
      "pshufd      $0x0,%%xmm2,%%xmm2            \n"

      // 16 pixels per loop.
      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "movdqu      (%0,%3,2),%%xmm3              \n"
      "movdqu      0x10(%0,%3,2),%%xmm4          \n"
      "add         $0x20,%0                      \n"
      "pavgw       %%xmm3,%%xmm0                 \n"
      "pavgw       %%xmm4,%%xmm1                 \n"
      "pmulhuw     %%xmm2,%%xmm0                 \n"
      "pmulhuw     %%xmm2,%%xmm1                 \n"
      "packuswb    %%xmm1,%%xmm0                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "add         $0x10,%1                      \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_uv),        // %0
        "+r"(dst_uv),        // %1
        "+r"(width)          // %2
      : "r"(src_uv_stride),  // %3
        "r"(scale)           // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4");
}
#endif  // HAS_HALFROW_16TO8_SSSE3

#ifdef HAS_HALFWIDTHROW_16TO8_SSSE3
static const uvec8 kShuffleMaskHalfWidth16To8 = {
    0u,   1u,   4u,   5u,   8u,   9u,   12u,  13u,
    128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u};

void HalfWidthRow_16To8_SSSE3(const uint16_t* src_uv,
                              ptrdiff_t src_uv_stride,
                              uint8_t* dst_uv,
                              int scale,
                              int width) {
  asm volatile(
      "movd        %4,%%xmm2                     \n"
      "punpcklwd   %%xmm2,%%xmm2                 \n"
      "pshufd      $0x0,%%xmm2,%%xmm2            \n"
      "pcmpeqw     %%xmm5,%%xmm5                 \n"
      "pcmpeqw     %%xmm6,%%xmm6                 \n"
      "psrlw       $15,%%xmm5                    \n"
      "pmaddwd     %%xmm6,%%xmm6                 \n"
      "movdqa      %5,%%xmm7                     \n"

      // 8 pixels per loop.
      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "movdqu      (%0,%3,2),%%xmm3              \n"
      "movdqu      0x10(%0,%3,2),%%xmm4          \n"
      "add         $0x20,%0                      \n"
      "pmaddwd     %%xmm5,%%xmm0                 \n"
      "pmaddwd     %%xmm5,%%xmm1                 \n"
      "pmaddwd     %%xmm5,%%xmm3                 \n"
      "pmaddwd     %%xmm5,%%xmm4                 \n"
      "paddd       %%xmm3,%%xmm0                 \n"
      "paddd       %%xmm4,%%xmm1                 \n"
      "paddd       %%xmm6,%%xmm0                 \n"
      "paddd       %%xmm6,%%xmm1                 \n"
      "psrld       $2,%%xmm0                     \n"
      "psrld       $2,%%xmm1                     \n"
      "pshufb      %%xmm7,%%xmm0                 \n"
      "pshufb      %%xmm7,%%xmm1                 \n"
      "punpcklqdq  %%xmm1,%%xmm0                 \n"
      "pmulhuw     %%xmm2,%%xmm0                 \n"
      "packuswb    %%xmm0,%%xmm0                 \n"
      "movq        %%xmm0,(%1)                   \n"
      "add         $0x08,%1                      \n"
      "sub         $0x08,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_uv),                    // %0
        "+r"(dst_uv),                    // %1
        "+r"(width)                      // %2
      : "r"(src_uv_stride),              // %3
        "rm"(scale),                     // %4
        "m"(kShuffleMaskHalfWidth16To8)  // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_HALFWIDTHROW_16TO8_SSSE3

#ifdef HAS_HALFROW_16TO8_AVX2
void HalfRow_16To8_AVX2(const uint16_t* src_uv,
                        ptrdiff_t src_uv_stride,
                        uint8_t* dst_uv,
                        int scale,
                        int width) {
  asm volatile(
      "vmovd       %4,%%xmm2                     \n"
      "vpbroadcastw %%xmm2,%%ymm2                \n"

      // 32 pixels per loop.
      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "vpavgw      (%0,%3,2),%%ymm0,%%ymm0       \n"
      "vpavgw      0x20(%0,%3,2),%%ymm1,%%ymm1   \n"
      "add         $0x40,%0                      \n"
      "vpmulhuw    %%ymm2,%%ymm0,%%ymm0          \n"
      "vpmulhuw    %%ymm2,%%ymm1,%%ymm1          \n"
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "add         $0x20,%1                      \n"
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_uv),        // %0
        "+r"(dst_uv),        // %1
        "+r"(width)          // %2
      : "r"(src_uv_stride),  // %3
        "rm"(scale)          // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_HALFROW_16TO8_AVX2

#ifdef HAS_HALFWIDTHROW_16TO8_AVX2
void HalfWidthRow_16To8_AVX2(const uint16_t* src_uv,
                             ptrdiff_t src_uv_stride,
                             uint8_t* dst_uv,
                             int scale,
                             int width) {
  asm volatile(
      "vmovd       %4,%%xmm2                     \n"
      "vpbroadcastw %%xmm2,%%ymm2                \n"
      "vpcmpeqw    %%ymm5,%%ymm5,%%ymm5          \n"
      "vpmaddwd    %%ymm5,%%ymm5,%%ymm6          \n"
      "vpsrlw      $15,%%ymm5,%%ymm5             \n"

      // 16 pixels per loop.
      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "vmovdqu     (%0,%3,2),%%ymm3              \n"
      "vmovdqu     0x20(%0,%3,2),%%ymm4          \n"
      "add         $0x40,%0                      \n"
      "vpmaddwd    %%ymm5,%%ymm0,%%ymm0          \n"
      "vpmaddwd    %%ymm5,%%ymm1,%%ymm1          \n"
      "vpmaddwd    %%ymm5,%%ymm3,%%ymm3          \n"
      "vpmaddwd    %%ymm5,%%ymm4,%%ymm4          \n"
      "vpaddd      %%ymm3,%%ymm0,%%ymm0          \n"
      "vpaddd      %%ymm4,%%ymm1,%%ymm1          \n"
      "vpaddd      %%ymm6,%%ymm0,%%ymm0          \n"
      "vpaddd      %%ymm6,%%ymm1,%%ymm1          \n"
      "vpsrld      $2,%%ymm0,%%ymm0              \n"
      "vpsrld      $2,%%ymm1,%%ymm1              \n"
      "vpackusdw   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vpmulhuw    %%ymm2,%%ymm0,%%ymm0          \n"
      "vpackuswb   %%ymm0,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%xmm0,(%1)                   \n"
      "add         $0x10,%1                      \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_uv),        // %0
        "+r"(dst_uv),        // %1
        "+r"(width)          // %2
      : "r"(src_uv_stride),  // %3
        "rm"(scale)          // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}
#endif  // HAS_HALFWIDTHROW_16TO8_AVX2

#ifdef HAS_HALFROW_16TO8_AVX512BW
void HalfRow_16To8_AVX512BW(const uint16_t* src_uv,
                            ptrdiff_t src_uv_stride,
                            uint8_t* dst_uv,
                            int scale,
                            int width) {
  const uint16_t* src_uv1 = src_uv + src_uv_stride;
  const int shift = 16 - __builtin_ctz((uint32_t)scale);
  asm volatile("vpbroadcastw %4,%%zmm2                    \n"

               // 64 pixels per loop.
               LABELALIGN
               "1:          \n"
               "vmovdqu64   (%0),%%zmm0                   \n"
               "vmovdqu64   0x40(%0),%%zmm1               \n"
               "vpavgw      (%1),%%zmm0,%%zmm0            \n"
               "vpavgw      0x40(%1),%%zmm1,%%zmm1        \n"
               "add         $0x80,%0                      \n"
               "add         $0x80,%1                      \n"
               "vpsrlvw     %%zmm2,%%zmm0,%%zmm0          \n"
               "vpsrlvw     %%zmm2,%%zmm1,%%zmm1          \n"
               "vpmovuswb   %%zmm0,(%2)                   \n"
               "vpmovuswb   %%zmm1,0x20(%2)               \n"
               "add         $0x40,%2                      \n"
               "sub         $0x40,%3                      \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : "+r"(src_uv),   // %0
                 "+r"(src_uv1),  // %1
                 "+r"(dst_uv),   // %2
                 "+r"(width)     // %3
               : "rm"(shift)     // %4
               : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_HALFROW_16TO8_AVX512BW

#ifdef HAS_HALFWIDTHROW_16TO8_AVX512BW
void HalfWidthRow_16To8_AVX512BW(const uint16_t* src_uv,
                                 ptrdiff_t src_uv_stride,
                                 uint8_t* dst_uv,
                                 int scale,
                                 int width) {
  const uint16_t* src_uv1 = src_uv + src_uv_stride;
  asm volatile(
      "vpbroadcastw %4,%%zmm2                    \n"
      "vpternlogd  $0xff,%%zmm5,%%zmm5,%%zmm5    \n"
      "vpmaddwd    %%zmm5,%%zmm5,%%zmm6          \n"
      "vpabsw      %%zmm5,%%zmm5                 \n"

      // 32 pixels per loop.
      LABELALIGN
      "1:          \n"
      "vmovdqu64   (%0),%%zmm0                   \n"
      "vmovdqu64   0x40(%0),%%zmm1               \n"
      "vmovdqu64   (%1),%%zmm3                   \n"
      "vmovdqu64   0x40(%1),%%zmm4               \n"
      "add         $0x80,%0                      \n"
      "add         $0x80,%1                      \n"
      "vpmaddwd    %%zmm5,%%zmm0,%%zmm0          \n"
      "vpmaddwd    %%zmm5,%%zmm1,%%zmm1          \n"
      "vpmaddwd    %%zmm5,%%zmm3,%%zmm3          \n"
      "vpmaddwd    %%zmm5,%%zmm4,%%zmm4          \n"
      "vpaddd      %%zmm3,%%zmm0,%%zmm0          \n"
      "vpaddd      %%zmm4,%%zmm1,%%zmm1          \n"
      "vpaddd      %%zmm6,%%zmm0,%%zmm0          \n"
      "vpaddd      %%zmm6,%%zmm1,%%zmm1          \n"
      "vpsrld      $2,%%zmm0,%%zmm0              \n"
      "vpsrld      $2,%%zmm1,%%zmm1              \n"
      "vpmovusdw   %%zmm0,%%ymm0                 \n"
      "vpmovusdw   %%zmm1,%%ymm1                 \n"
      "vinserti64x4 $1,%%ymm1,%%zmm0,%%zmm0      \n"
      "vpmulhuw    %%zmm2,%%zmm0,%%zmm0          \n"
      "vpmovuswb   %%zmm0,(%2)                   \n"
      "add         $0x20,%2                      \n"
      "sub         $0x20,%3                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_uv),   // %0
        "+r"(src_uv1),  // %1
        "+r"(dst_uv),   // %2
        "+r"(width)     // %3
      : "rm"(scale)     // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}
#endif  // HAS_HALFWIDTHROW_16TO8_AVX512BW

// Use scale to convert to lsb formats depending how many bits there are:
// 512 = 9 bits
// 1024 = 10 bits
// 4096 = 12 bits
// 65536 = 16 bits
void Convert8To16Row_SSE2(const uint8_t* src_y,
                          uint16_t* dst_y,
                          int bits,
                          int width) {
  const int shift = 16 - bits;
  asm volatile("movd        %3,%%xmm2                     \n"

               // 16 pixels per loop.
               LABELALIGN
               "1:          \n"
               "movdqu      (%0),%%xmm0                   \n"
               "movdqa      %%xmm0,%%xmm1                 \n"
               "punpcklbw   %%xmm0,%%xmm0                 \n"
               "punpckhbw   %%xmm1,%%xmm1                 \n"
               "add         $0x10,%0                      \n"
               "psrlw       %%xmm2,%%xmm0                 \n"
               "psrlw       %%xmm2,%%xmm1                 \n"
               "movdqu      %%xmm0,(%1)                   \n"
               "movdqu      %%xmm1,0x10(%1)               \n"
               "add         $0x20,%1                      \n"
               "sub         $0x10,%2                      \n"
               "jg          1b                            \n"
               : "+r"(src_y),  // %0
                 "+r"(dst_y),  // %1
                 "+r"(width)   // %2
               : "r"(shift)    // %3
               : "memory", "cc", "xmm0", "xmm1", "xmm2");
}

#ifdef HAS_CONVERT8TO16ROW_AVX2
void Convert8To16Row_AVX2(const uint8_t* src_y,
                          uint16_t* dst_y,
                          int bits,
                          int width) {
  const int shift = 16 - bits;
  asm volatile("vmovd       %3,%%xmm2                     \n"

               // 32 pixels per loop.
               LABELALIGN
               "1:          \n"
               "vmovdqu     (%0),%%ymm0                   \n"
               "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
               "add         $0x20,%0                      \n"
               "vpunpckhbw  %%ymm0,%%ymm0,%%ymm1          \n"
               "vpunpcklbw  %%ymm0,%%ymm0,%%ymm0          \n"
               "vpsrlw      %%xmm2,%%ymm0,%%ymm0          \n"
               "vpsrlw      %%xmm2,%%ymm1,%%ymm1          \n"
               "vmovdqu     %%ymm0,(%1)                   \n"
               "vmovdqu     %%ymm1,0x20(%1)               \n"
               "add         $0x40,%1                      \n"
               "sub         $0x20,%2                      \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : "+r"(src_y),  // %0
                 "+r"(dst_y),  // %1
                 "+r"(width)   // %2
               : "r"(shift)    // %3
               : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_CONVERT8TO16ROW_AVX2

#ifdef HAS_CONVERT8TO16ROW_AVX512BW
void Convert8To16Row_AVX512BW(const uint8_t* src_y,
                              uint16_t* dst_y,
                              int bits,
                              int width) {
  const int shift = 16 - bits;
  asm volatile(
      "vpbroadcastw %4,%%zmm2                    \n"
      "vmovd       %3,%%xmm3                    \n"

      // 64 pixels per loop.
      LABELALIGN
      "1:          \n"
      "vpmovzxbw   (%0),%%zmm0                  \n"
      "vpmovzxbw   0x20(%0),%%zmm1              \n"
      "add         $0x40,%0                     \n"
      "vpmullw     %%zmm2,%%zmm0,%%zmm0         \n"
      "vpmullw     %%zmm2,%%zmm1,%%zmm1         \n"
      "vpsrlw      %%xmm3,%%zmm0,%%zmm0         \n"
      "vpsrlw      %%xmm3,%%zmm1,%%zmm1         \n"
      "vmovdqu16   %%zmm0,(%1)                  \n"
      "vmovdqu16   %%zmm1,0x40(%1)              \n"
      "add         $0x80,%1                     \n"
      "sub         $0x40,%2                     \n"
      "jg          1b                           \n"
      "vzeroupper  \n"
      : "+r"(src_y),  // %0
        "+r"(dst_y),  // %1
        "+r"(width)   // %2
      : "r"(shift),   // %3
        "r"(0x0101)   // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3");
}
#endif  // HAS_CONVERT8TO16ROW_AVX512BW

#ifdef HAS_SPLITRGBROW_SSSE3
// Shuffle table for converting RGB to Planar.
static const uvec8 kSplitRGBShuffle[9] = {
    {0u, 3u, 6u, 9u, 12u, 15u, 128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u,
     128u, 128u},
    {128u, 128u, 128u, 128u, 128u, 128u, 2u, 5u, 8u, 11u, 14u, 128u, 128u, 128u,
     128u, 128u},
    {128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u, 1u, 4u,
     7u, 10u, 13u},
    {1u, 4u, 7u, 10u, 13u, 128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u,
     128u, 128u},
    {128u, 128u, 128u, 128u, 128u, 0u, 3u, 6u, 9u, 12u, 15u, 128u, 128u, 128u,
     128u, 128u},
    {128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u, 2u, 5u,
     8u, 11u, 14u},
    {2u, 5u, 8u, 11u, 14u, 128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u,
     128u, 128u},
    {128u, 128u, 128u, 128u, 128u, 1u, 4u, 7u, 10u, 13u, 128u, 128u, 128u, 128u,
     128u, 128u},
    {128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u, 0u, 3u, 6u, 9u,
     12u, 15u}};

void SplitRGBRow_SSSE3(const uint8_t* src_rgb,
                       uint8_t* dst_r,
                       uint8_t* dst_g,
                       uint8_t* dst_b,
                       int width) {
  asm volatile(
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "movdqu      0x20(%0),%%xmm2               \n"
      "pshufb      0(%5), %%xmm0                 \n"
      "pshufb      16(%5), %%xmm1                \n"
      "pshufb      32(%5), %%xmm2                \n"
      "por         %%xmm1,%%xmm0                 \n"
      "por         %%xmm2,%%xmm0                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "lea         0x10(%1),%1                   \n"

      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "movdqu      0x20(%0),%%xmm2               \n"
      "pshufb      48(%5),%%xmm0                 \n"
      "pshufb      64(%5),%%xmm1                 \n"
      "pshufb      80(%5), %%xmm2                \n"
      "por         %%xmm1,%%xmm0                 \n"
      "por         %%xmm2,%%xmm0                 \n"
      "movdqu      %%xmm0,(%2)                   \n"
      "lea         0x10(%2),%2                   \n"

      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "movdqu      0x20(%0),%%xmm2               \n"
      "pshufb      96(%5), %%xmm0                \n"
      "pshufb      112(%5), %%xmm1               \n"
      "pshufb      128(%5), %%xmm2               \n"
      "por         %%xmm1,%%xmm0                 \n"
      "por         %%xmm2,%%xmm0                 \n"
      "movdqu      %%xmm0,(%3)                   \n"
      "lea         0x10(%3),%3                   \n"
      "lea         0x30(%0),%0                   \n"
      "sub         $0x10,%4                      \n"
      "jg          1b                            \n"
      : "+r"(src_rgb),             // %0
        "+r"(dst_r),               // %1
        "+r"(dst_g),               // %2
        "+r"(dst_b),               // %3
        "+r"(width)                // %4
      : "r"(&kSplitRGBShuffle[0])  // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_SPLITRGBROW_SSSE3

#ifdef HAS_SPLITRGBROW_SSE41
// Shuffle table for converting RGB to Planar, SSE4.1. Note: these are used for
// the AVX2 implementation as well.
static const uvec8 kSplitRGBShuffleSSE41[5] = {
    {0u, 3u, 6u, 9u, 12u, 15u, 2u, 5u, 8u, 11u, 14u, 1u, 4u, 7u, 10u, 13u},
    {1u, 4u, 7u, 10u, 13u, 0u, 3u, 6u, 9u, 12u, 15u, 2u, 5u, 8u, 11u, 14u},
    {2u, 5u, 8u, 11u, 14u, 1u, 4u, 7u, 10u, 13u, 0u, 3u, 6u, 9u, 12u, 15u},
    {0u, 128u, 0u, 0u, 128u, 0u, 0u, 128u, 0u, 0u, 128u, 0u, 0u, 128u, 0u, 0u},
    {0u, 0u, 128u, 0u, 0u, 128u, 0u, 0u, 128u, 0u, 0u, 128u, 0u, 0u, 128u, 0u},
};

void SplitRGBRow_SSE41(const uint8_t* src_rgb,
                       uint8_t* dst_r,
                       uint8_t* dst_g,
                       uint8_t* dst_b,
                       int width) {
  asm volatile(
      "movdqa      48(%5), %%xmm0                \n"
      "1:          \n"
      "movdqu      (%0),%%xmm1                   \n"
      "movdqu      0x10(%0),%%xmm2               \n"
      "movdqu      0x20(%0),%%xmm3               \n"
      "lea         0x30(%0),%0                   \n"
      "movdqa      %%xmm1, %%xmm4                \n"
      "pblendvb    %%xmm3, %%xmm1                \n"
      "pblendvb    %%xmm2, %%xmm3                \n"
      "pblendvb    %%xmm4, %%xmm2                \n"
      "palignr     $0xF, %%xmm0, %%xmm0          \n"
      "pblendvb    %%xmm2, %%xmm1                \n"
      "pblendvb    %%xmm3, %%xmm2                \n"
      "pblendvb    %%xmm4, %%xmm3                \n"
      "palignr     $0x1, %%xmm0, %%xmm0          \n"
      "pshufb      0(%5), %%xmm1                 \n"
      "pshufb      16(%5), %%xmm2                \n"
      "pshufb      32(%5), %%xmm3                \n"
      "movdqu      %%xmm1,(%1)                   \n"
      "lea         0x10(%1),%1                   \n"
      "movdqu      %%xmm2,(%2)                   \n"
      "lea         0x10(%2),%2                   \n"
      "movdqu      %%xmm3,(%3)                   \n"
      "lea         0x10(%3),%3                   \n"
      "sub         $0x10,%4                      \n"
      "jg          1b                            \n"
      : "+r"(src_rgb),                  // %0
        "+r"(dst_r),                    // %1
        "+r"(dst_g),                    // %2
        "+r"(dst_b),                    // %3
        "+r"(width)                     // %4
      : "r"(&kSplitRGBShuffleSSE41[0])  // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4");
}
#endif  // HAS_SPLITRGBROW_SSE41

#ifdef HAS_SPLITRGBROW_AVX2
void SplitRGBRow_AVX2(const uint8_t* src_rgb,
                      uint8_t* dst_r,
                      uint8_t* dst_g,
                      uint8_t* dst_b,
                      int width) {
  asm volatile(
      "vbroadcasti128 48(%5), %%ymm0             \n"
      "vbroadcasti128 64(%5), %%ymm7             \n"
#if defined(__x86_64__)
      "vbroadcasti128 0(%5), %%ymm8              \n"
      "vbroadcasti128 16(%5), %%ymm9             \n"
      "vbroadcasti128 32(%5), %%ymm10            \n"
#endif
      "1:          \n"
      "vmovdqu     (%0),%%ymm4                   \n"
      "vmovdqu     0x20(%0),%%ymm5               \n"
      "vmovdqu     0x40(%0),%%ymm6               \n"
      "lea         0x60(%0),%0                   \n"
      "vpblendd    $240, %%ymm5, %%ymm4, %%ymm1  \n"
      "vperm2i128  $33, %%ymm6, %%ymm4, %%ymm2   \n"
      "vpblendd    $240, %%ymm6, %%ymm5, %%ymm3  \n"
      "vpblendvb   %%ymm0, %%ymm3, %%ymm1, %%ymm4 \n"
      "vpblendvb   %%ymm0, %%ymm1, %%ymm2, %%ymm5 \n"
      "vpblendvb   %%ymm0, %%ymm2, %%ymm3, %%ymm6 \n"
      "vpblendvb   %%ymm7, %%ymm5, %%ymm4, %%ymm1 \n"
      "vpblendvb   %%ymm7, %%ymm6, %%ymm5, %%ymm2 \n"
      "vpblendvb   %%ymm7, %%ymm4, %%ymm6, %%ymm3 \n"
#if defined(__x86_64__)
      "vpshufb     %%ymm8, %%ymm1, %%ymm1        \n"
      "vpshufb     %%ymm9, %%ymm2, %%ymm2        \n"
      "vpshufb     %%ymm10, %%ymm3, %%ymm3       \n"
#else
      "vbroadcasti128 0(%5), %%ymm4              \n"
      "vbroadcasti128 16(%5), %%ymm5             \n"
      "vbroadcasti128 32(%5), %%ymm6             \n"
      "vpshufb     %%ymm4, %%ymm1, %%ymm1        \n"
      "vpshufb     %%ymm5, %%ymm2, %%ymm2        \n"
      "vpshufb     %%ymm6, %%ymm3, %%ymm3        \n"
#endif
      "vmovdqu     %%ymm1,(%1)                   \n"
      "lea         0x20(%1),%1                   \n"
      "vmovdqu     %%ymm2,(%2)                   \n"
      "lea         0x20(%2),%2                   \n"
      "vmovdqu     %%ymm3,(%3)                   \n"
      "lea         0x20(%3),%3                   \n"
      "sub         $0x20,%4                      \n"
      "jg          1b                            \n"
      : "+r"(src_rgb),                  // %0
        "+r"(dst_r),                    // %1
        "+r"(dst_g),                    // %2
        "+r"(dst_b),                    // %3
        "+r"(width)                     // %4
      : "r"(&kSplitRGBShuffleSSE41[0])  // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7"
#if defined(__x86_64__)
        ,
        "xmm8", "xmm9", "xmm10"
#endif
  );
}
#endif  // HAS_SPLITRGBROW_AVX2

#ifdef HAS_MERGERGBROW_SSSE3
// Shuffle table for converting Planar to RGB.
static const uvec8 kMergeRGBShuffle[9] = {
    {0u, 128u, 128u, 1u, 128u, 128u, 2u, 128u, 128u, 3u, 128u, 128u, 4u, 128u,
     128u, 5u},
    {128u, 0u, 128u, 128u, 1u, 128u, 128u, 2u, 128u, 128u, 3u, 128u, 128u, 4u,
     128u, 128u},
    {128u, 128u, 0u, 128u, 128u, 1u, 128u, 128u, 2u, 128u, 128u, 3u, 128u, 128u,
     4u, 128u},
    {128u, 128u, 6u, 128u, 128u, 7u, 128u, 128u, 8u, 128u, 128u, 9u, 128u, 128u,
     10u, 128u},
    {5u, 128u, 128u, 6u, 128u, 128u, 7u, 128u, 128u, 8u, 128u, 128u, 9u, 128u,
     128u, 10u},
    {128u, 5u, 128u, 128u, 6u, 128u, 128u, 7u, 128u, 128u, 8u, 128u, 128u, 9u,
     128u, 128u},
    {128u, 11u, 128u, 128u, 12u, 128u, 128u, 13u, 128u, 128u, 14u, 128u, 128u,
     15u, 128u, 128u},
    {128u, 128u, 11u, 128u, 128u, 12u, 128u, 128u, 13u, 128u, 128u, 14u, 128u,
     128u, 15u, 128u},
    {10u, 128u, 128u, 11u, 128u, 128u, 12u, 128u, 128u, 13u, 128u, 128u, 14u,
     128u, 128u, 15u}};

void MergeRGBRow_SSSE3(const uint8_t* src_r,
                       const uint8_t* src_g,
                       const uint8_t* src_b,
                       uint8_t* dst_rgb,
                       int width) {
  asm volatile(
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      (%1),%%xmm1                   \n"
      "movdqu      (%2),%%xmm2                   \n"
      "pshufb      (%5), %%xmm0                  \n"
      "pshufb      16(%5), %%xmm1                \n"
      "pshufb      32(%5), %%xmm2                \n"
      "por         %%xmm1,%%xmm0                 \n"
      "por         %%xmm2,%%xmm0                 \n"
      "movdqu      %%xmm0,(%3)                   \n"

      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      (%1),%%xmm1                   \n"
      "movdqu      (%2),%%xmm2                   \n"
      "pshufb      48(%5), %%xmm0                \n"
      "pshufb      64(%5), %%xmm1                \n"
      "pshufb      80(%5), %%xmm2                \n"
      "por         %%xmm1,%%xmm0                 \n"
      "por         %%xmm2,%%xmm0                 \n"
      "movdqu      %%xmm0,16(%3)                 \n"

      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      (%1),%%xmm1                   \n"
      "movdqu      (%2),%%xmm2                   \n"
      "pshufb      96(%5), %%xmm0                \n"
      "pshufb      112(%5), %%xmm1               \n"
      "pshufb      128(%5), %%xmm2               \n"
      "por         %%xmm1,%%xmm0                 \n"
      "por         %%xmm2,%%xmm0                 \n"
      "movdqu      %%xmm0,32(%3)                 \n"

      "lea         0x10(%0),%0                   \n"
      "lea         0x10(%1),%1                   \n"
      "lea         0x10(%2),%2                   \n"
      "lea         0x30(%3),%3                   \n"
      "sub         $0x10,%4                      \n"
      "jg          1b                            \n"
      : "+r"(src_r),               // %0
        "+r"(src_g),               // %1
        "+r"(src_b),               // %2
        "+r"(dst_rgb),             // %3
        "+r"(width)                // %4
      : "r"(&kMergeRGBShuffle[0])  // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_MERGERGBROW_SSSE3

#ifdef HAS_MERGEARGBROW_SSE2
void MergeARGBRow_SSE2(const uint8_t* src_r,
                       const uint8_t* src_g,
                       const uint8_t* src_b,
                       const uint8_t* src_a,
                       uint8_t* dst_argb,
                       int width) {
  asm volatile(
      "sub         %0,%1                         \n"
      "sub         %0,%2                         \n"
      "sub         %0,%3                         \n"

      LABELALIGN
      "1:          \n"

      "movq        (%0,%2),%%xmm0                \n"  // B
      "movq        (%0),%%xmm1                   \n"  // R
      "movq        (%0,%1),%%xmm2                \n"  // G
      "punpcklbw   %%xmm1,%%xmm0                 \n"  // BR
      "movq        (%0,%3),%%xmm1                \n"  // A
      "punpcklbw   %%xmm1,%%xmm2                 \n"  // GA
      "movdqa      %%xmm0,%%xmm1                 \n"  // BR
      "punpckhbw   %%xmm2,%%xmm1                 \n"  // BGRA (hi)
      "punpcklbw   %%xmm2,%%xmm0                 \n"  // BGRA (lo)
      "movdqu      %%xmm0,(%4)                   \n"
      "movdqu      %%xmm1,16(%4)                 \n"

      "lea         8(%0),%0                      \n"
      "lea         32(%4),%4                     \n"
      "sub         $0x8,%5                       \n"
      "jg          1b                            \n"
      : "+r"(src_r),     // %0
        "+r"(src_g),     // %1
        "+r"(src_b),     // %2
        "+r"(src_a),     // %3
        "+r"(dst_argb),  // %4
        "+r"(width)      // %5
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_MERGEARGBROW_SSE2

#ifdef HAS_MERGEXRGBROW_SSE2
void MergeXRGBRow_SSE2(const uint8_t* src_r,
                       const uint8_t* src_g,
                       const uint8_t* src_b,
                       uint8_t* dst_argb,
                       int width) {
  asm volatile(
      "1:          \n"

      "movq        (%2),%%xmm0                   \n"  // B
      "movq        (%0),%%xmm1                   \n"  // R
      "movq        (%1),%%xmm2                   \n"  // G
      "punpcklbw   %%xmm1,%%xmm0                 \n"  // BR
      "pcmpeqd     %%xmm1,%%xmm1                 \n"  // A(255)
      "punpcklbw   %%xmm1,%%xmm2                 \n"  // GA
      "movdqa      %%xmm0,%%xmm1                 \n"  // BR
      "punpckhbw   %%xmm2,%%xmm1                 \n"  // BGRA (hi)
      "punpcklbw   %%xmm2,%%xmm0                 \n"  // BGRA (lo)
      "movdqu      %%xmm0,(%3)                   \n"
      "movdqu      %%xmm1,16(%3)                 \n"

      "lea         8(%0),%0                      \n"
      "lea         8(%1),%1                      \n"
      "lea         8(%2),%2                      \n"
      "lea         32(%3),%3                     \n"
      "sub         $0x8,%4                       \n"
      "jg          1b                            \n"
      : "+r"(src_r),     // %0
        "+r"(src_g),     // %1
        "+r"(src_b),     // %2
        "+r"(dst_argb),  // %3
        "+r"(width)      // %4
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_MERGEXRGBROW_SSE2

#ifdef HAS_MERGEARGBROW_AVX2
void MergeARGBRow_AVX2(const uint8_t* src_r,
                       const uint8_t* src_g,
                       const uint8_t* src_b,
                       const uint8_t* src_a,
                       uint8_t* dst_argb,
                       int width) {
  asm volatile(
      "sub         %0,%1                         \n"
      "sub         %0,%2                         \n"
      "sub         %0,%3                         \n"

      LABELALIGN
      "1:          \n"

      "vmovdqu     (%0,%2),%%xmm0                \n"  // B
      "vmovdqu     (%0,%1),%%xmm1                \n"  // R
      "vinserti128 $1,(%0),%%ymm0,%%ymm0         \n"  // G
      "vinserti128 $1,(%0,%3),%%ymm1,%%ymm1      \n"  // A
      "vpunpckhbw  %%ymm1,%%ymm0,%%ymm2          \n"
      "vpunpcklbw  %%ymm1,%%ymm0,%%ymm0          \n"
      "vperm2i128  $0x31,%%ymm2,%%ymm0,%%ymm1    \n"
      "vperm2i128  $0x20,%%ymm2,%%ymm0,%%ymm0    \n"
      "vpunpckhwd  %%ymm1,%%ymm0,%%ymm2          \n"
      "vpunpcklwd  %%ymm1,%%ymm0,%%ymm0          \n"
      "vperm2i128  $0x31,%%ymm2,%%ymm0,%%ymm1    \n"
      "vperm2i128  $0x20,%%ymm2,%%ymm0,%%ymm0    \n"
      "vmovdqu     %%ymm0,(%4)                   \n"  // First 8
      "vmovdqu     %%ymm1,32(%4)                 \n"  // Next 8

      "lea         16(%0),%0                     \n"
      "lea         64(%4),%4                     \n"
      "sub         $0x10,%5                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_r),     // %0
        "+r"(src_g),     // %1
        "+r"(src_b),     // %2
        "+r"(src_a),     // %3
        "+r"(dst_argb),  // %4
        "+r"(width)      // %5
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_MERGEARGBROW_AVX2

#ifdef HAS_MERGEXRGBROW_AVX2
void MergeXRGBRow_AVX2(const uint8_t* src_r,
                       const uint8_t* src_g,
                       const uint8_t* src_b,
                       uint8_t* dst_argb,
                       int width) {
  asm volatile(
      "1:          \n"

      "vmovdqu     (%2),%%xmm0                   \n"  // B
      "vpcmpeqb    %%ymm1,%%ymm1,%%ymm1          \n"  // A(255)
      "vinserti128 $0,(%1),%%ymm1,%%ymm1         \n"  // R
      "vinserti128 $1,(%0),%%ymm0,%%ymm0         \n"  // G
      "vpunpckhbw  %%ymm1,%%ymm0,%%ymm2          \n"
      "vpunpcklbw  %%ymm1,%%ymm0,%%ymm0          \n"
      "vperm2i128  $0x31,%%ymm2,%%ymm0,%%ymm1    \n"
      "vperm2i128  $0x20,%%ymm2,%%ymm0,%%ymm0    \n"
      "vpunpckhwd  %%ymm1,%%ymm0,%%ymm2          \n"
      "vpunpcklwd  %%ymm1,%%ymm0,%%ymm0          \n"
      "vperm2i128  $0x31,%%ymm2,%%ymm0,%%ymm1    \n"
      "vperm2i128  $0x20,%%ymm2,%%ymm0,%%ymm0    \n"
      "vmovdqu     %%ymm0,(%3)                   \n"  // First 8
      "vmovdqu     %%ymm1,32(%3)                 \n"  // Next 8

      "lea         16(%0),%0                     \n"
      "lea         16(%1),%1                     \n"
      "lea         16(%2),%2                     \n"
      "lea         64(%3),%3                     \n"
      "sub         $0x10,%4                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_r),     // %0
        "+r"(src_g),     // %1
        "+r"(src_b),     // %2
        "+r"(dst_argb),  // %3
        "+rm"(width)     // %4
        ::"memory",
        "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_MERGEXRGBROW_AVX2

#ifdef HAS_SPLITARGBROW_SSE2
void SplitARGBRow_SSE2(const uint8_t* src_argb,
                       uint8_t* dst_r,
                       uint8_t* dst_g,
                       uint8_t* dst_b,
                       uint8_t* dst_a,
                       int width) {
  asm volatile(
      "sub         %1,%2                         \n"
      "sub         %1,%3                         \n"
      "sub         %1,%4                         \n"

      LABELALIGN
      "1:          \n"

      "movdqu      (%0),%%xmm0                   \n"  // 00-0F
      "movdqu      16(%0),%%xmm1                 \n"  // 10-1F
      "movdqa      %%xmm0,%%xmm2                 \n"
      "punpcklqdq  %%xmm1,%%xmm0                 \n"  // 00-07 10-17
      "punpckhqdq  %%xmm1,%%xmm2                 \n"  // 08-0F 18-1F
      "movdqa      %%xmm0,%%xmm1                 \n"
      "punpcklbw   %%xmm2,%%xmm0                 \n"  // 08192A3B4C5D6E7F (lo)
      "punpckhbw   %%xmm2,%%xmm1                 \n"  // 08192A3B4C5D6E7F (hi)
      "movdqa      %%xmm0,%%xmm2                 \n"
      "punpcklqdq  %%xmm1,%%xmm0                 \n"  // 08192A3B08192A3B
      "punpckhqdq  %%xmm1,%%xmm2                 \n"  // 4C5D6E7F4C5D6E7F
      "movdqa      %%xmm0,%%xmm1                 \n"
      "punpcklbw   %%xmm2,%%xmm0                 \n"  // 048C159D26AE37BF (lo)
      "punpckhbw   %%xmm2,%%xmm1                 \n"  // 048C159D26AE37BF (hi)
      "movdqa      %%xmm0,%%xmm2                 \n"
      "punpckldq   %%xmm1,%%xmm0                 \n"  // 048C048C159D159D (BG)
      "punpckhdq   %%xmm1,%%xmm2                 \n"  // 26AE26AE37BF37BF (RA)
      "movlps      %%xmm0,(%1,%3)                \n"  // B
      "movhps      %%xmm0,(%1,%2)                \n"  // G
      "movlps      %%xmm2,(%1)                   \n"  // R
      "movhps      %%xmm2,(%1,%4)                \n"  // A

      "lea         32(%0),%0                     \n"
      "lea         8(%1),%1                      \n"
      "sub         $0x8,%5                       \n"
      "jg          1b                            \n"
      : "+r"(src_argb),  // %0
        "+r"(dst_r),     // %1
        "+r"(dst_g),     // %2
        "+r"(dst_b),     // %3
        "+r"(dst_a),     // %4
        "+rm"(width)     // %5
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_SPLITARGBROW_SSE2

#ifdef HAS_SPLITXRGBROW_SSE2
void SplitXRGBRow_SSE2(const uint8_t* src_argb,
                       uint8_t* dst_r,
                       uint8_t* dst_g,
                       uint8_t* dst_b,
                       int width) {
  asm volatile(
      "1:          \n"

      "movdqu      (%0),%%xmm0                   \n"  // 00-0F
      "movdqu      16(%0),%%xmm1                 \n"  // 10-1F
      "movdqa      %%xmm0,%%xmm2                 \n"
      "punpcklqdq  %%xmm1,%%xmm0                 \n"  // 00-07 10-17
      "punpckhqdq  %%xmm1,%%xmm2                 \n"  // 08-0F 18-1F
      "movdqa      %%xmm0,%%xmm1                 \n"
      "punpcklbw   %%xmm2,%%xmm0                 \n"  // 08192A3B4C5D6E7F (lo)
      "punpckhbw   %%xmm2,%%xmm1                 \n"  // 08192A3B4C5D6E7F (hi)
      "movdqa      %%xmm0,%%xmm2                 \n"
      "punpcklqdq  %%xmm1,%%xmm0                 \n"  // 08192A3B08192A3B
      "punpckhqdq  %%xmm1,%%xmm2                 \n"  // 4C5D6E7F4C5D6E7F
      "movdqa      %%xmm0,%%xmm1                 \n"
      "punpcklbw   %%xmm2,%%xmm0                 \n"  // 048C159D26AE37BF (lo)
      "punpckhbw   %%xmm2,%%xmm1                 \n"  // 048C159D26AE37BF (hi)
      "movdqa      %%xmm0,%%xmm2                 \n"
      "punpckldq   %%xmm1,%%xmm0                 \n"  // 048C048C159D159D (BG)
      "punpckhdq   %%xmm1,%%xmm2                 \n"  // 26AE26AE37BF37BF (RA)
      "movlps      %%xmm0,(%3)                   \n"  // B
      "movhps      %%xmm0,(%2)                   \n"  // G
      "movlps      %%xmm2,(%1)                   \n"  // R

      "lea         32(%0),%0                     \n"
      "lea         8(%1),%1                      \n"
      "lea         8(%2),%2                      \n"
      "lea         8(%3),%3                      \n"
      "sub         $0x8,%4                       \n"
      "jg          1b                            \n"
      : "+r"(src_argb),  // %0
        "+r"(dst_r),     // %1
        "+r"(dst_g),     // %2
        "+r"(dst_b),     // %3
        "+rm"(width)     // %4
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_SPLITXRGBROW_SSE2

static const uvec8 kShuffleMaskARGBSplit = {0, 4, 8,  12, 1, 5, 9,  13,
                                            2, 6, 10, 14, 3, 7, 11, 15};
#ifdef HAS_SPLITARGBROW_SSSE3
void SplitARGBRow_SSSE3(const uint8_t* src_argb,
                        uint8_t* dst_r,
                        uint8_t* dst_g,
                        uint8_t* dst_b,
                        uint8_t* dst_a,
                        int width) {
  asm volatile(
      "movdqa      %6,%%xmm3                     \n"
      "sub         %1,%2                         \n"
      "sub         %1,%3                         \n"
      "sub         %1,%4                         \n"

      LABELALIGN
      "1:          \n"

      "movdqu      (%0),%%xmm0                   \n"  // 00-0F
      "movdqu      16(%0),%%xmm1                 \n"  // 10-1F
      "pshufb      %%xmm3,%%xmm0                 \n"  // 048C159D26AE37BF (lo)
      "pshufb      %%xmm3,%%xmm1                 \n"  // 048C159D26AE37BF (hi)
      "movdqa      %%xmm0,%%xmm2                 \n"
      "punpckldq   %%xmm1,%%xmm0                 \n"  // 048C048C159D159D (BG)
      "punpckhdq   %%xmm1,%%xmm2                 \n"  // 26AE26AE37BF37BF (RA)
      "movlps      %%xmm0,(%1,%3)                \n"  // B
      "movhps      %%xmm0,(%1,%2)                \n"  // G
      "movlps      %%xmm2,(%1)                   \n"  // R
      "movhps      %%xmm2,(%1,%4)                \n"  // A

      "lea         32(%0),%0                     \n"
      "lea         8(%1),%1                      \n"
      "subl        $0x8,%5                       \n"
      "jg          1b                            \n"
      : "+r"(src_argb),  // %0
        "+r"(dst_r),     // %1
        "+r"(dst_g),     // %2
        "+r"(dst_b),     // %3
        "+r"(dst_a),     // %4
#if defined(__i386__)
        "+m"(width)  // %5
#else
        "+rm"(width)  // %5
#endif
      : "m"(kShuffleMaskARGBSplit)  // %6
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3");
}
#endif  // HAS_SPLITARGBROW_SSSE3

#ifdef HAS_SPLITXRGBROW_SSSE3
void SplitXRGBRow_SSSE3(const uint8_t* src_argb,
                        uint8_t* dst_r,
                        uint8_t* dst_g,
                        uint8_t* dst_b,
                        int width) {
  asm volatile(
      "movdqa      %5,%%xmm3                     \n"

      LABELALIGN
      "1:          \n"

      "movdqu      (%0),%%xmm0                   \n"  // 00-0F
      "movdqu      16(%0),%%xmm1                 \n"  // 10-1F
      "pshufb      %%xmm3,%%xmm0                 \n"  // 048C159D26AE37BF (lo)
      "pshufb      %%xmm3,%%xmm1                 \n"  // 048C159D26AE37BF (hi)
      "movdqa      %%xmm0,%%xmm2                 \n"
      "punpckldq   %%xmm1,%%xmm0                 \n"  // 048C048C159D159D (BG)
      "punpckhdq   %%xmm1,%%xmm2                 \n"  // 26AE26AE37BF37BF (RA)
      "movlps      %%xmm0,(%3)                   \n"  // B
      "movhps      %%xmm0,(%2)                   \n"  // G
      "movlps      %%xmm2,(%1)                   \n"  // R

      "lea         32(%0),%0                     \n"
      "lea         8(%1),%1                      \n"
      "lea         8(%2),%2                      \n"
      "lea         8(%3),%3                      \n"
      "sub         $0x8,%4                       \n"
      "jg          1b                            \n"
      : "+r"(src_argb),             // %0
        "+r"(dst_r),                // %1
        "+r"(dst_g),                // %2
        "+r"(dst_b),                // %3
        "+r"(width)                 // %4
      : "m"(kShuffleMaskARGBSplit)  // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3");
}
#endif  // HAS_SPLITXRGBROW_SSSE3

#ifdef HAS_SPLITARGBROW_AVX2
static const ulvec32 kShuffleMaskARGBPermute = {0, 4, 1, 5, 2, 6, 3, 7};
void SplitARGBRow_AVX2(const uint8_t* src_argb,
                       uint8_t* dst_r,
                       uint8_t* dst_g,
                       uint8_t* dst_b,
                       uint8_t* dst_a,
                       int width) {
  asm volatile(
      "sub         %1,%2                         \n"
      "sub         %1,%3                         \n"
      "sub         %1,%4                         \n"
      "vmovdqa     %7,%%ymm3                     \n"
      "vbroadcasti128 %6,%%ymm4                  \n"

      LABELALIGN
      "1:          \n"

      "vmovdqu     (%0),%%xmm0                   \n"  // 00-0F
      "vmovdqu     16(%0),%%xmm1                 \n"  // 10-1F
      "vinserti128 $1,32(%0),%%ymm0,%%ymm0       \n"  // 00-0F 20-2F
      "vinserti128 $1,48(%0),%%ymm1,%%ymm1       \n"  // 10-1F 30-3F
      "vpshufb     %%ymm4,%%ymm0,%%ymm0          \n"
      "vpshufb     %%ymm4,%%ymm1,%%ymm1          \n"
      "vpermd      %%ymm0,%%ymm3,%%ymm0          \n"
      "vpermd      %%ymm1,%%ymm3,%%ymm1          \n"
      "vpunpckhdq  %%ymm1,%%ymm0,%%ymm2          \n"  // GA
      "vpunpckldq  %%ymm1,%%ymm0,%%ymm0          \n"  // BR
      "vmovdqu     %%xmm0,(%1,%3)                \n"  // B
      "vextracti128 $1,%%ymm0,(%1)               \n"  // R
      "vmovdqu     %%xmm2,(%1,%2)                \n"  // G
      "vextracti128 $1,%%ymm2,(%1,%4)            \n"  // A
      "lea         64(%0),%0                     \n"
      "lea         16(%1),%1                     \n"
      "subl        $0x10,%5                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_argb),  // %0
        "+r"(dst_r),     // %1
        "+r"(dst_g),     // %2
        "+r"(dst_b),     // %3
        "+r"(dst_a),     // %4
#if defined(__i386__)
        "+m"(width)  // %5
#else
        "+rm"(width)  // %5
#endif
      : "m"(kShuffleMaskARGBSplit),   // %6
        "m"(kShuffleMaskARGBPermute)  // %7
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4");
}
#endif  // HAS_SPLITARGBROW_AVX2

#ifdef HAS_SPLITXRGBROW_AVX2
void SplitXRGBRow_AVX2(const uint8_t* src_argb,
                       uint8_t* dst_r,
                       uint8_t* dst_g,
                       uint8_t* dst_b,
                       int width) {
  asm volatile(
      "vmovdqa     %6,%%ymm3                     \n"
      "vbroadcasti128 %5,%%ymm4                  \n"

      LABELALIGN
      "1:          \n"

      "vmovdqu     (%0),%%xmm0                   \n"  // 00-0F
      "vmovdqu     16(%0),%%xmm1                 \n"  // 10-1F
      "vinserti128 $1,32(%0),%%ymm0,%%ymm0       \n"  // 00-0F 20-2F
      "vinserti128 $1,48(%0),%%ymm1,%%ymm1       \n"  // 10-1F 30-3F
      "vpshufb     %%ymm4,%%ymm0,%%ymm0          \n"
      "vpshufb     %%ymm4,%%ymm1,%%ymm1          \n"
      "vpermd      %%ymm0,%%ymm3,%%ymm0          \n"
      "vpermd      %%ymm1,%%ymm3,%%ymm1          \n"
      "vpunpckhdq  %%ymm1,%%ymm0,%%ymm2          \n"  // GA
      "vpunpckldq  %%ymm1,%%ymm0,%%ymm0          \n"  // BR
      "vmovdqu     %%xmm0,(%3)                   \n"  // B
      "vextracti128 $1,%%ymm0,(%1)               \n"  // R
      "vmovdqu     %%xmm2,(%2)                   \n"  // G

      "lea         64(%0),%0                     \n"
      "lea         16(%1),%1                     \n"
      "lea         16(%2),%2                     \n"
      "lea         16(%3),%3                     \n"
      "sub         $0x10,%4                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_argb),               // %0
        "+r"(dst_r),                  // %1
        "+r"(dst_g),                  // %2
        "+r"(dst_b),                  // %3
        "+r"(width)                   // %4
      : "m"(kShuffleMaskARGBSplit),   // %5
        "m"(kShuffleMaskARGBPermute)  // %6
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4");
}
#endif  // HAS_SPLITXRGBROW_AVX2

#ifdef HAS_MERGEXR30ROW_AVX2
void MergeXR30Row_AVX2(const uint16_t* src_r,
                       const uint16_t* src_g,
                       const uint16_t* src_b,
                       uint8_t* dst_ar30,
                       int depth,
                       int width) {
  int shift = depth - 10;
  asm volatile(
      "sub         %0,%1                         \n"
      "sub         %0,%2                         \n"
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"  // AR30 constants
      "vpsrlw      $14,%%ymm5,%%ymm5             \n"
      "vpsllw      $4,%%ymm5,%%ymm5              \n"  // 2 alpha bits
      "vpcmpeqb    %%ymm6,%%ymm6,%%ymm6          \n"
      "vpsrlw      $6,%%ymm6,%%ymm6              \n"
      "vmovd       %5,%%xmm4                     \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     (%0,%1),%%ymm1                \n"
      "vmovdqu     (%0,%2),%%ymm2                \n"
      "vpsrlw      %%xmm4,%%ymm0,%%ymm0          \n"
      "vpsrlw      %%xmm4,%%ymm1,%%ymm1          \n"
      "vpsrlw      %%xmm4,%%ymm2,%%ymm2          \n"
      "vpminuw     %%ymm0,%%ymm6,%%ymm0          \n"
      "vpminuw     %%ymm1,%%ymm6,%%ymm1          \n"
      "vpminuw     %%ymm2,%%ymm6,%%ymm2          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vpermq      $0xd8,%%ymm1,%%ymm1           \n"
      "vpermq      $0xd8,%%ymm2,%%ymm2           \n"
      "vpsllw      $0x4,%%ymm0,%%ymm0            \n"  // Shift R to target bit
      "vpunpckhwd  %%ymm0,%%ymm2,%%ymm3          \n"  // RB
      "vpunpcklwd  %%ymm0,%%ymm2,%%ymm0          \n"
      "vpunpckhwd  %%ymm5,%%ymm1,%%ymm2          \n"  // AG
      "vpunpcklwd  %%ymm5,%%ymm1,%%ymm1          \n"
      "vpslld      $0xa,%%ymm1,%%ymm1            \n"  // Shift AG to target bit
      "vpslld      $0xa,%%ymm2,%%ymm2            \n"
      "vpor        %%ymm1,%%ymm0,%%ymm0          \n"  // Combine
      "vpor        %%ymm2,%%ymm3,%%ymm3          \n"
      "vmovdqu     %%ymm0,(%3)                   \n"
      "vmovdqu     %%ymm3,0x20(%3)               \n"
      "lea         0x20(%0),%0                   \n"
      "lea         0x40(%3),%3                   \n"
      "sub         $0x10,%4                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_r),     // %0
        "+r"(src_g),     // %1
        "+r"(src_b),     // %2
        "+r"(dst_ar30),  // %3
        "+r"(width)      // %4
#if defined(__i386__)
      : "m"(shift)  // %5
#else
      : "rm"(shift)  // %5
#endif
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}
#endif  // HAS_MERGEXR30ROW_AVX2

#ifdef HAS_MERGEAR64ROW_AVX2
static const lvec32 MergeAR64Permute = {0, 4, 2, 6, 1, 5, 3, 7};
void MergeAR64Row_AVX2(const uint16_t* src_r,
                       const uint16_t* src_g,
                       const uint16_t* src_b,
                       const uint16_t* src_a,
                       uint16_t* dst_ar64,
                       int depth,
                       int width) {
  int shift = 16 - depth;
  int mask = (1 << depth) - 1;
  mask = (mask << 16) + mask;
  asm volatile(
      "sub         %0,%1                         \n"
      "sub         %0,%2                         \n"
      "sub         %0,%3                         \n"
      "vmovdqa     %8,%%ymm5                     \n"
      "vmovd       %6,%%xmm6                     \n"
      "vbroadcastss %7,%%ymm7                    \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"  // R
      "vmovdqu     (%0,%1),%%ymm1                \n"  // G
      "vmovdqu     (%0,%2),%%ymm2                \n"  // B
      "vmovdqu     (%0,%3),%%ymm3                \n"  // A
      "vpminuw     %%ymm0,%%ymm7,%%ymm0          \n"
      "vpminuw     %%ymm1,%%ymm7,%%ymm1          \n"
      "vpminuw     %%ymm2,%%ymm7,%%ymm2          \n"
      "vpminuw     %%ymm3,%%ymm7,%%ymm3          \n"
      "vpsllw      %%xmm6,%%ymm0,%%ymm0          \n"
      "vpsllw      %%xmm6,%%ymm1,%%ymm1          \n"
      "vpsllw      %%xmm6,%%ymm2,%%ymm2          \n"
      "vpsllw      %%xmm6,%%ymm3,%%ymm3          \n"
      "vpermd      %%ymm0,%%ymm5,%%ymm0          \n"
      "vpermd      %%ymm1,%%ymm5,%%ymm1          \n"
      "vpermd      %%ymm2,%%ymm5,%%ymm2          \n"
      "vpermd      %%ymm3,%%ymm5,%%ymm3          \n"
      "vpunpcklwd  %%ymm1,%%ymm2,%%ymm4          \n"  // BG(low)
      "vpunpckhwd  %%ymm1,%%ymm2,%%ymm1          \n"  // BG(hi)
      "vpunpcklwd  %%ymm3,%%ymm0,%%ymm2          \n"  // RA(low)
      "vpunpckhwd  %%ymm3,%%ymm0,%%ymm0          \n"  // RA(hi)
      "vpunpckldq  %%ymm2,%%ymm4,%%ymm3          \n"  // BGRA(1)
      "vpunpckhdq  %%ymm2,%%ymm4,%%ymm4          \n"  // BGRA(3)
      "vpunpckldq  %%ymm0,%%ymm1,%%ymm2          \n"  // BGRA(2)
      "vpunpckhdq  %%ymm0,%%ymm1,%%ymm1          \n"  // BGRA(4)
      "vmovdqu     %%ymm3,(%4)                   \n"
      "vmovdqu     %%ymm2,0x20(%4)               \n"
      "vmovdqu     %%ymm4,0x40(%4)               \n"
      "vmovdqu     %%ymm1,0x60(%4)               \n"
      "lea         0x20(%0),%0                   \n"
      "lea         0x80(%4),%4                   \n"
      "subl        $0x10,%5                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_r),     // %0
        "+r"(src_g),     // %1
        "+r"(src_b),     // %2
        "+r"(src_a),     // %3
        "+r"(dst_ar64),  // %4
#if defined(__i386__)
        "+m"(width)  // %5
#else
        "+rm"(width)  // %5
#endif
      : "m"(shift),            // %6
        "m"(mask),             // %7
        "m"(MergeAR64Permute)  // %8
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_MERGEAR64ROW_AVX2

#ifdef HAS_MERGEXR64ROW_AVX2
void MergeXR64Row_AVX2(const uint16_t* src_r,
                       const uint16_t* src_g,
                       const uint16_t* src_b,
                       uint16_t* dst_ar64,
                       int depth,
                       int width) {
  int shift = 16 - depth;
  int mask = (1 << depth) - 1;
  mask = (mask << 16) + mask;
  asm volatile(
      "sub         %0,%1                         \n"
      "sub         %0,%2                         \n"
      "vmovdqa     %7,%%ymm5                     \n"
      "vmovd       %5,%%xmm6                     \n"
      "vbroadcastss %6,%%ymm7                    \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"  // R
      "vmovdqu     (%0,%1),%%ymm1                \n"  // G
      "vmovdqu     (%0,%2),%%ymm2                \n"  // B
      "vpminuw     %%ymm0,%%ymm7,%%ymm0          \n"
      "vpminuw     %%ymm1,%%ymm7,%%ymm1          \n"
      "vpminuw     %%ymm2,%%ymm7,%%ymm2          \n"
      "vpsllw      %%xmm6,%%ymm0,%%ymm0          \n"
      "vpsllw      %%xmm6,%%ymm1,%%ymm1          \n"
      "vpsllw      %%xmm6,%%ymm2,%%ymm2          \n"
      "vpermd      %%ymm0,%%ymm5,%%ymm0          \n"
      "vpermd      %%ymm1,%%ymm5,%%ymm1          \n"
      "vpermd      %%ymm2,%%ymm5,%%ymm2          \n"
      "vpcmpeqb    %%ymm3,%%ymm3,%%ymm3          \n"  // A (0xffff)
      "vpunpcklwd  %%ymm1,%%ymm2,%%ymm4          \n"  // BG(low)
      "vpunpckhwd  %%ymm1,%%ymm2,%%ymm1          \n"  // BG(hi)
      "vpunpcklwd  %%ymm3,%%ymm0,%%ymm2          \n"  // RA(low)
      "vpunpckhwd  %%ymm3,%%ymm0,%%ymm0          \n"  // RA(hi)
      "vpunpckldq  %%ymm2,%%ymm4,%%ymm3          \n"  // BGRA(1)
      "vpunpckhdq  %%ymm2,%%ymm4,%%ymm4          \n"  // BGRA(3)
      "vpunpckldq  %%ymm0,%%ymm1,%%ymm2          \n"  // BGRA(2)
      "vpunpckhdq  %%ymm0,%%ymm1,%%ymm1          \n"  // BGRA(4)
      "vmovdqu     %%ymm3,(%3)                   \n"
      "vmovdqu     %%ymm2,0x20(%3)               \n"
      "vmovdqu     %%ymm4,0x40(%3)               \n"
      "vmovdqu     %%ymm1,0x60(%3)               \n"
      "lea         0x20(%0),%0                   \n"
      "lea         0x80(%3),%3                   \n"
      "subl        $0x10,%4                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_r),           // %0
        "+r"(src_g),           // %1
        "+r"(src_b),           // %2
        "+r"(dst_ar64),        // %3
        "+r"(width)            // %4
      : "m"(shift),            // %5
        "m"(mask),             // %6
        "m"(MergeAR64Permute)  // %7
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_MERGEXR64ROW_AVX2

#ifdef HAS_MERGEARGB16TO8ROW_AVX2
static const uvec8 MergeARGB16To8Shuffle = {0, 8,  1, 9,  2, 10, 3, 11,
                                            4, 12, 5, 13, 6, 14, 7, 15};
void MergeARGB16To8Row_AVX2(const uint16_t* src_r,
                            const uint16_t* src_g,
                            const uint16_t* src_b,
                            const uint16_t* src_a,
                            uint8_t* dst_argb,
                            int depth,
                            int width) {
  int shift = depth - 8;
  asm volatile(
      "sub         %0,%1                         \n"
      "sub         %0,%2                         \n"
      "sub         %0,%3                         \n"
      "vbroadcasti128 %7,%%ymm5                  \n"
      "vmovd       %6,%%xmm6                     \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"  // R
      "vmovdqu     (%0,%1),%%ymm1                \n"  // G
      "vmovdqu     (%0,%2),%%ymm2                \n"  // B
      "vmovdqu     (%0,%3),%%ymm3                \n"  // A
      "vpsrlw      %%xmm6,%%ymm0,%%ymm0          \n"
      "vpsrlw      %%xmm6,%%ymm1,%%ymm1          \n"
      "vpsrlw      %%xmm6,%%ymm2,%%ymm2          \n"
      "vpsrlw      %%xmm6,%%ymm3,%%ymm3          \n"
      "vpackuswb   %%ymm1,%%ymm2,%%ymm1          \n"  // BG (planar)
      "vpackuswb   %%ymm3,%%ymm0,%%ymm0          \n"  // RA (planar)
      "vpshufb     %%ymm5,%%ymm1,%%ymm1          \n"  // BG (interleave)
      "vpshufb     %%ymm5,%%ymm0,%%ymm0          \n"  // RA (interleave)
      "vpermq      $0xd8,%%ymm1,%%ymm1           \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vpunpcklwd  %%ymm0,%%ymm1,%%ymm2          \n"  // BGRA (low)
      "vpunpckhwd  %%ymm0,%%ymm1,%%ymm0          \n"  // BGRA (hi)
      "vmovdqu     %%ymm2,(%4)                   \n"
      "vmovdqu     %%ymm0,0x20(%4)               \n"
      "lea         0x20(%0),%0                   \n"
      "lea         0x40(%4),%4                   \n"
      "subl        $0x10,%5                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_r),     // %0
        "+r"(src_g),     // %1
        "+r"(src_b),     // %2
        "+r"(src_a),     // %3
        "+r"(dst_argb),  // %4
#if defined(__i386__)
        "+m"(width)  // %5
#else
        "+rm"(width)  // %5
#endif
      : "m"(shift),                 // %6
        "m"(MergeARGB16To8Shuffle)  // %7
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}
#endif  // HAS_MERGEARGB16TO8ROW_AVX2

#ifdef HAS_MERGEXRGB16TO8ROW_AVX2
void MergeXRGB16To8Row_AVX2(const uint16_t* src_r,
                            const uint16_t* src_g,
                            const uint16_t* src_b,
                            uint8_t* dst_argb,
                            int depth,
                            int width) {
  int shift = depth - 8;
  asm volatile(
      "sub         %0,%1                         \n"
      "sub         %0,%2                         \n"
      "vbroadcasti128 %6,%%ymm5                  \n"
      "vmovd       %5,%%xmm6                     \n"
      "vpcmpeqb    %%ymm3,%%ymm3,%%ymm3          \n"
      "vpsrlw      $8,%%ymm3,%%ymm3              \n"  // A (0xff)

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"  // R
      "vmovdqu     (%0,%1),%%ymm1                \n"  // G
      "vmovdqu     (%0,%2),%%ymm2                \n"  // B
      "vpsrlw      %%xmm6,%%ymm0,%%ymm0          \n"
      "vpsrlw      %%xmm6,%%ymm1,%%ymm1          \n"
      "vpsrlw      %%xmm6,%%ymm2,%%ymm2          \n"
      "vpackuswb   %%ymm1,%%ymm2,%%ymm1          \n"  // BG (planar)
      "vpackuswb   %%ymm3,%%ymm0,%%ymm0          \n"  // RA (planar)
      "vpshufb     %%ymm5,%%ymm1,%%ymm1          \n"  // BG (interleave)
      "vpshufb     %%ymm5,%%ymm0,%%ymm0          \n"  // RA (interleave)
      "vpermq      $0xd8,%%ymm1,%%ymm1           \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vpunpcklwd  %%ymm0,%%ymm1,%%ymm2          \n"  // BGRA (low)
      "vpunpckhwd  %%ymm0,%%ymm1,%%ymm0          \n"  // BGRA (hi)
      "vmovdqu     %%ymm2,(%3)                   \n"
      "vmovdqu     %%ymm0,0x20(%3)               \n"
      "lea         0x20(%0),%0                   \n"
      "lea         0x40(%3),%3                   \n"
      "subl        $0x10,%4                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_r),                // %0
        "+r"(src_g),                // %1
        "+r"(src_b),                // %2
        "+r"(dst_argb),             // %3
        "+r"(width)                 // %4
      : "m"(shift),                 // %5
        "m"(MergeARGB16To8Shuffle)  // %6
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}
#endif  // HAS_MERGEXRGB16TO8ROW_AVX2

#ifdef HAS_COPYROW_SSE2
void CopyRow_SSE2(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile(
      "test        $0xf,%0                       \n"
      "jne         2f                            \n"
      "test        $0xf,%1                       \n"
      "jne         2f                            \n"

      LABELALIGN
      "1:          \n"
      "movdqa      (%0),%%xmm0                   \n"
      "movdqa      0x10(%0),%%xmm1               \n"
      "lea         0x20(%0),%0                   \n"
      "movdqa      %%xmm0,(%1)                   \n"
      "movdqa      %%xmm1,0x10(%1)               \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "jmp         9f                            \n"

      LABELALIGN
      "2:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "lea         0x20(%0),%0                   \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "movdqu      %%xmm1,0x10(%1)               \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jg          2b                            \n"

      LABELALIGN "9:          \n"
      : "+r"(src),   // %0
        "+r"(dst),   // %1
        "+r"(width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1");
}
#endif  // HAS_COPYROW_SSE2

#ifdef HAS_COPYROW_AVX
void CopyRow_AVX(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile(
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "lea         0x40(%0),%0                   \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "vmovdqu     %%ymm1,0x20(%1)               \n"
      "lea         0x40(%1),%1                   \n"
      "sub         $0x40,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src),   // %0
        "+r"(dst),   // %1
        "+r"(width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1");
}
#endif  // HAS_COPYROW_AVX

#ifdef HAS_COPYROW_AVX512BW
void CopyRow_AVX512BW(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile(
      "1:          \n"
      "vmovups     (%0),%%zmm0                   \n"
      "vmovups     0x40(%0),%%zmm1               \n"
      "lea         0x80(%0),%0                   \n"
      "vmovups     %%zmm0,(%1)                   \n"
      "vmovups     %%zmm1,0x40(%1)               \n"
      "lea         0x80(%1),%1                   \n"
      "sub         $0x80,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src),   // %0
        "+r"(dst),   // %1
        "+r"(width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1");
}
#endif  // HAS_COPYROW_AVX512BW

#ifdef HAS_COPYROW_ERMS
// Multiple of 1.
void CopyRow_ERMS(const uint8_t* src, uint8_t* dst, int width) {
  size_t width_tmp = (size_t)(width);
  asm volatile("rep         movsb                         \n"
               : "+S"(src),       // %0
                 "+D"(dst),       // %1
                 "+c"(width_tmp)  // %2
               :
               : "memory", "cc");
}
#endif  // HAS_COPYROW_ERMS

#ifdef HAS_ARGBCOPYALPHAROW_SSE2
// width in pixels
void ARGBCopyAlphaRow_SSE2(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile(
      "pcmpeqb     %%xmm0,%%xmm0                 \n"
      "pslld       $0x18,%%xmm0                  \n"
      "pcmpeqb     %%xmm1,%%xmm1                 \n"
      "psrld       $0x8,%%xmm1                   \n"

      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm2                   \n"
      "movdqu      0x10(%0),%%xmm3               \n"
      "lea         0x20(%0),%0                   \n"
      "movdqu      (%1),%%xmm4                   \n"
      "movdqu      0x10(%1),%%xmm5               \n"
      "pand        %%xmm0,%%xmm2                 \n"
      "pand        %%xmm0,%%xmm3                 \n"
      "pand        %%xmm1,%%xmm4                 \n"
      "pand        %%xmm1,%%xmm5                 \n"
      "por         %%xmm4,%%xmm2                 \n"
      "por         %%xmm5,%%xmm3                 \n"
      "movdqu      %%xmm2,(%1)                   \n"
      "movdqu      %%xmm3,0x10(%1)               \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src),   // %0
        "+r"(dst),   // %1
        "+r"(width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_ARGBCOPYALPHAROW_SSE2

#ifdef HAS_ARGBCOPYALPHAROW_AVX2
// width in pixels
void ARGBCopyAlphaRow_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile(
      "vpcmpeqb    %%ymm0,%%ymm0,%%ymm0          \n"
      "vpsrld      $0x8,%%ymm0,%%ymm0            \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm1                   \n"
      "vmovdqu     0x20(%0),%%ymm2               \n"
      "lea         0x40(%0),%0                   \n"
      "vpblendvb   %%ymm0,(%1),%%ymm1,%%ymm1     \n"
      "vpblendvb   %%ymm0,0x20(%1),%%ymm2,%%ymm2 \n"
      "vmovdqu     %%ymm1,(%1)                   \n"
      "vmovdqu     %%ymm2,0x20(%1)               \n"
      "lea         0x40(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src),   // %0
        "+r"(dst),   // %1
        "+r"(width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_ARGBCOPYALPHAROW_AVX2

#ifdef HAS_ARGBEXTRACTALPHAROW_SSE2
// width in pixels
void ARGBExtractAlphaRow_SSE2(const uint8_t* src_argb,
                              uint8_t* dst_a,
                              int width) {
  asm volatile(
      "1:          \n"
      "movdqu      (%0), %%xmm0                  \n"
      "movdqu      0x10(%0), %%xmm1              \n"
      "lea         0x20(%0), %0                  \n"
      "psrld       $0x18, %%xmm0                 \n"
      "psrld       $0x18, %%xmm1                 \n"
      "packssdw    %%xmm1, %%xmm0                \n"
      "packuswb    %%xmm0, %%xmm0                \n"
      "movq        %%xmm0,(%1)                   \n"
      "lea         0x8(%1), %1                   \n"
      "sub         $0x8, %2                      \n"
      "jg          1b                            \n"
      : "+r"(src_argb),  // %0
        "+r"(dst_a),     // %1
        "+rm"(width)     // %2
      :
      : "memory", "cc", "xmm0", "xmm1");
}
#endif  // HAS_ARGBEXTRACTALPHAROW_SSE2

#ifdef HAS_ARGBEXTRACTALPHAROW_AVX2
static const uvec8 kShuffleAlphaShort_AVX2 = {
    3u,  128u, 128u, 128u, 7u,  128u, 128u, 128u,
    11u, 128u, 128u, 128u, 15u, 128u, 128u, 128u};

void ARGBExtractAlphaRow_AVX2(const uint8_t* src_argb,
                              uint8_t* dst_a,
                              int width) {
  asm volatile(
      "vmovdqa     %3,%%ymm4                     \n"
      "vbroadcasti128 %4,%%ymm5                  \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0), %%ymm0                  \n"
      "vmovdqu     0x20(%0), %%ymm1              \n"
      "vpshufb     %%ymm5,%%ymm0,%%ymm0          \n"  // vpsrld $0x18, %%ymm0
      "vpshufb     %%ymm5,%%ymm1,%%ymm1          \n"
      "vmovdqu     0x40(%0), %%ymm2              \n"
      "vmovdqu     0x60(%0), %%ymm3              \n"
      "lea         0x80(%0), %0                  \n"
      "vpackssdw   %%ymm1, %%ymm0, %%ymm0        \n"  // mutates
      "vpshufb     %%ymm5,%%ymm2,%%ymm2          \n"
      "vpshufb     %%ymm5,%%ymm3,%%ymm3          \n"
      "vpackssdw   %%ymm3, %%ymm2, %%ymm2        \n"  // mutates
      "vpackuswb   %%ymm2,%%ymm0,%%ymm0          \n"  // mutates.
      "vpermd      %%ymm0,%%ymm4,%%ymm0          \n"  // unmutate.
      "vmovdqu     %%ymm0,(%1)                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x20, %2                     \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_argb),               // %0
        "+r"(dst_a),                  // %1
        "+rm"(width)                  // %2
      : "m"(kPermdARGBToY_AVX),       // %3
        "m"(kShuffleAlphaShort_AVX2)  // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_ARGBEXTRACTALPHAROW_AVX2

#ifdef HAS_ARGBEXTRACTALPHAROW_AVX512BW
void ARGBExtractAlphaRow_AVX512BW(const uint8_t* src_argb,
                                  uint8_t* dst_a,
                                  int width) {
  uintptr_t temp;
  asm volatile(
      "sub         $0x20,%2                      \n"
      "jl          2f                            \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu32   (%0),%%zmm0                   \n"
      "vmovdqu32   0x40(%0),%%zmm1               \n"
      "lea         0x80(%0),%0                   \n"
      "vpsrld      $0x18,%%zmm0,%%zmm0           \n"
      "vpsrld      $0x18,%%zmm1,%%zmm1           \n"
      "vpmovdb     %%zmm0,(%1)                   \n"
      "vpmovdb     %%zmm1,0x10(%1)               \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jge         1b                            \n"

      "2:          \n"
      "add         $0x20,%2                      \n"
      "jle         99f                           \n"
      "cmp         $0x10,%2                      \n"
      "jl          3f                            \n"
      "vmovdqu32   (%0),%%zmm0                   \n"
      "lea         0x40(%0),%0                   \n"
      "vpsrld      $0x18,%%zmm0,%%zmm0           \n"
      "vpmovdb     %%zmm0,(%1)                   \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jz          99f                           \n"

      "3:          \n"
      "mov         $-1,%k3                       \n"
      "bzhi        %k2,%k3,%k3                   \n"
      "kmovw       %k3,%%k1                      \n"
      "vmovdqu32   (%0),%%zmm0%{%%k1%}%{z%}      \n"
      "vpsrld      $0x18,%%zmm0,%%zmm0           \n"
      "vpmovdb     %%zmm0,(%1)%{%%k1%}           \n"

      "99:         \n"
      "vzeroupper  \n"
      : "+r"(src_argb),  // %0
        "+r"(dst_a),     // %1
        "+r"(width),     // %2
        "=&r"(temp)      // %3
      :
      : "memory", "cc", "xmm0", "xmm1", "k1");
}
#endif  // HAS_ARGBEXTRACTALPHAROW_AVX512BW

#ifdef HAS_ARGBCOPYYTOALPHAROW_SSE2
// width in pixels
void ARGBCopyYToAlphaRow_SSE2(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile(
      "pcmpeqb     %%xmm0,%%xmm0                 \n"
      "pslld       $0x18,%%xmm0                  \n"
      "pcmpeqb     %%xmm1,%%xmm1                 \n"
      "psrld       $0x8,%%xmm1                   \n"

      LABELALIGN
      "1:          \n"
      "movq        (%0),%%xmm2                   \n"
      "lea         0x8(%0),%0                    \n"
      "punpcklbw   %%xmm2,%%xmm2                 \n"
      "punpckhwd   %%xmm2,%%xmm3                 \n"
      "punpcklwd   %%xmm2,%%xmm2                 \n"
      "movdqu      (%1),%%xmm4                   \n"
      "movdqu      0x10(%1),%%xmm5               \n"
      "pand        %%xmm0,%%xmm2                 \n"
      "pand        %%xmm0,%%xmm3                 \n"
      "pand        %%xmm1,%%xmm4                 \n"
      "pand        %%xmm1,%%xmm5                 \n"
      "por         %%xmm4,%%xmm2                 \n"
      "por         %%xmm5,%%xmm3                 \n"
      "movdqu      %%xmm2,(%1)                   \n"
      "movdqu      %%xmm3,0x10(%1)               \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src),   // %0
        "+r"(dst),   // %1
        "+r"(width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_ARGBCOPYYTOALPHAROW_SSE2

#ifdef HAS_ARGBCOPYYTOALPHAROW_AVX2
// width in pixels
void ARGBCopyYToAlphaRow_AVX2(const uint8_t* src, uint8_t* dst, int width) {
  asm volatile(
      "vpcmpeqb    %%ymm0,%%ymm0,%%ymm0          \n"
      "vpsrld      $0x8,%%ymm0,%%ymm0            \n"

      LABELALIGN
      "1:          \n"
      "vpmovzxbd   (%0),%%ymm1                   \n"
      "vpmovzxbd   0x8(%0),%%ymm2                \n"
      "lea         0x10(%0),%0                   \n"
      "vpslld      $0x18,%%ymm1,%%ymm1           \n"
      "vpslld      $0x18,%%ymm2,%%ymm2           \n"
      "vpblendvb   %%ymm0,(%1),%%ymm1,%%ymm1     \n"
      "vpblendvb   %%ymm0,0x20(%1),%%ymm2,%%ymm2 \n"
      "vmovdqu     %%ymm1,(%1)                   \n"
      "vmovdqu     %%ymm2,0x20(%1)               \n"
      "lea         0x40(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src),   // %0
        "+r"(dst),   // %1
        "+r"(width)  // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_ARGBCOPYYTOALPHAROW_AVX2

#ifdef HAS_ARGBCOPYYTOALPHAROW_AVX512BW
void ARGBCopyYToAlphaRow_AVX512BW(const uint8_t* src, uint8_t* dst, int width) {
  uintptr_t temp;
  asm volatile(
      "vpternlogd  $0xff,%%zmm0,%%zmm0,%%zmm0    \n"
      "vpsrld      $0x8,%%zmm0,%%zmm0            \n"
      "sub         $0x20,%2                      \n"
      "jl          2f                            \n"

      LABELALIGN
      "1:          \n"
      "vpmovzxbd   (%0),%%zmm1                   \n"
      "vpmovzxbd   0x10(%0),%%zmm2               \n"
      "lea         0x20(%0),%0                   \n"
      "vpslld      $0x18,%%zmm1,%%zmm1           \n"
      "vpslld      $0x18,%%zmm2,%%zmm2           \n"
      "vpternlogd  $0xf8,(%1),%%zmm0,%%zmm1      \n"
      "vpternlogd  $0xf8,0x40(%1),%%zmm0,%%zmm2 \n"
      "vmovdqu32   %%zmm1,(%1)                   \n"
      "vmovdqu32   %%zmm2,0x40(%1)               \n"
      "lea         0x80(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jge         1b                            \n"

      "2:          \n"
      "add         $0x20,%2                      \n"
      "jle         99f                           \n"
      "cmp         $0x10,%2                      \n"
      "jl          3f                            \n"
      "vpmovzxbd   (%0),%%zmm1                   \n"
      "lea         0x10(%0),%0                   \n"
      "vpslld      $0x18,%%zmm1,%%zmm1           \n"
      "vpternlogd  $0xf8,(%1),%%zmm0,%%zmm1      \n"
      "vmovdqu32   %%zmm1,(%1)                   \n"
      "lea         0x40(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jz          99f                           \n"

      "3:          \n"
      "mov         $-1,%k3                       \n"
      "bzhi        %k2,%k3,%k3                   \n"
      "kmovw       %k3,%%k1                      \n"
      "vpmovzxbd   (%0),%%zmm1%{%%k1%}%{z%}      \n"
      "vpslld      $0x18,%%zmm1,%%zmm1           \n"
      "vpternlogd  $0xf8,(%1),%%zmm0,%%zmm1%{%%k1%}%{z%} \n"
      "vmovdqu32   %%zmm1,(%1)%{%%k1%}           \n"

      "99:         \n"
      "vzeroupper  \n"
      : "+r"(src),    // %0
        "+r"(dst),    // %1
        "+r"(width),  // %2
        "=&r"(temp)   // %3
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "k1");
}
#endif  // HAS_ARGBCOPYYTOALPHAROW_AVX512BW

#ifdef HAS_SETROW_X86
void SetRow_X86(uint8_t* dst, uint8_t v8, int width) {
  size_t width_tmp = (size_t)(width >> 2);
  const uint32_t v32 = v8 * 0x01010101u;  // Duplicate byte to all bytes.
  asm volatile("rep         stosl                         \n"
               : "+D"(dst),       // %0
                 "+c"(width_tmp)  // %1
               : "a"(v32)         // %2
               : "memory", "cc");
}

void SetRow_ERMS(uint8_t* dst, uint8_t v8, int width) {
  size_t width_tmp = (size_t)(width);
  asm volatile("rep         stosb                         \n"
               : "+D"(dst),       // %0
                 "+c"(width_tmp)  // %1
               : "a"(v8)          // %2
               : "memory", "cc");
}

void ARGBSetRow_X86(uint8_t* dst_argb, uint32_t v32, int width) {
  size_t width_tmp = (size_t)(width);
  asm volatile("rep         stosl                         \n"
               : "+D"(dst_argb),  // %0
                 "+c"(width_tmp)  // %1
               : "a"(v32)         // %2
               : "memory", "cc");
}
#endif  // HAS_SETROW_X86

#ifdef HAS_YUY2TOYROW_SSE2
void YUY2ToYRow_SSE2(const uint8_t* src_yuy2, uint8_t* dst_y, int width) {
  asm volatile(
      "pcmpeqb     %%xmm5,%%xmm5                 \n"
      "psrlw       $0x8,%%xmm5                   \n"

      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "lea         0x20(%0),%0                   \n"
      "pand        %%xmm5,%%xmm0                 \n"
      "pand        %%xmm5,%%xmm1                 \n"
      "packuswb    %%xmm1,%%xmm0                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_yuy2),  // %0
        "+r"(dst_y),     // %1
        "+r"(width)      // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm5");
}

void YUY2ToNVUVRow_SSE2(const uint8_t* src_yuy2,
                        int stride_yuy2,
                        uint8_t* dst_uv,
                        int width) {
  asm volatile(
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "movdqu      0x00(%0,%3,1),%%xmm2          \n"
      "movdqu      0x10(%0,%3,1),%%xmm3          \n"
      "lea         0x20(%0),%0                   \n"
      "pavgb       %%xmm2,%%xmm0                 \n"
      "pavgb       %%xmm3,%%xmm1                 \n"
      "psrlw       $0x8,%%xmm0                   \n"
      "psrlw       $0x8,%%xmm1                   \n"
      "packuswb    %%xmm1,%%xmm0                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_yuy2),                // %0
        "+r"(dst_uv),                  // %1
        "+r"(width)                    // %2
      : "r"((ptrdiff_t)(stride_yuy2))  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3");
}

void YUY2ToUVRow_SSE2(const uint8_t* src_yuy2,
                      int stride_yuy2,
                      uint8_t* dst_u,
                      uint8_t* dst_v,
                      int width) {
  asm volatile(
      "pcmpeqb     %%xmm5,%%xmm5                 \n"
      "psrlw       $0x8,%%xmm5                   \n"
      "sub         %1,%2                         \n"

      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "movdqu      0x00(%0,%4,1),%%xmm2          \n"
      "movdqu      0x10(%0,%4,1),%%xmm3          \n"
      "lea         0x20(%0),%0                   \n"
      "pavgb       %%xmm2,%%xmm0                 \n"
      "pavgb       %%xmm3,%%xmm1                 \n"
      "psrlw       $0x8,%%xmm0                   \n"
      "psrlw       $0x8,%%xmm1                   \n"
      "packuswb    %%xmm1,%%xmm0                 \n"
      "movdqa      %%xmm0,%%xmm1                 \n"
      "pand        %%xmm5,%%xmm0                 \n"
      "packuswb    %%xmm0,%%xmm0                 \n"
      "psrlw       $0x8,%%xmm1                   \n"
      "packuswb    %%xmm1,%%xmm1                 \n"
      "movq        %%xmm0,(%1)                   \n"
      "movq        %%xmm1,0x00(%1,%2,1)          \n"
      "lea         0x8(%1),%1                    \n"
      "sub         $0x10,%3                      \n"
      "jg          1b                            \n"
      : "+r"(src_yuy2),                // %0
        "+r"(dst_u),                   // %1
        "+r"(dst_v),                   // %2
        "+r"(width)                    // %3
      : "r"((ptrdiff_t)(stride_yuy2))  // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm5");
}

void YUY2ToUV422Row_SSE2(const uint8_t* src_yuy2,
                         uint8_t* dst_u,
                         uint8_t* dst_v,
                         int width) {
  asm volatile(
      "pcmpeqb     %%xmm5,%%xmm5                 \n"
      "psrlw       $0x8,%%xmm5                   \n"
      "sub         %1,%2                         \n"

      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "lea         0x20(%0),%0                   \n"
      "psrlw       $0x8,%%xmm0                   \n"
      "psrlw       $0x8,%%xmm1                   \n"
      "packuswb    %%xmm1,%%xmm0                 \n"
      "movdqa      %%xmm0,%%xmm1                 \n"
      "pand        %%xmm5,%%xmm0                 \n"
      "packuswb    %%xmm0,%%xmm0                 \n"
      "psrlw       $0x8,%%xmm1                   \n"
      "packuswb    %%xmm1,%%xmm1                 \n"
      "movq        %%xmm0,(%1)                   \n"
      "movq        %%xmm1,0x00(%1,%2,1)          \n"
      "lea         0x8(%1),%1                    \n"
      "sub         $0x10,%3                      \n"
      "jg          1b                            \n"
      : "+r"(src_yuy2),  // %0
        "+r"(dst_u),     // %1
        "+r"(dst_v),     // %2
        "+r"(width)      // %3
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm5");
}

void UYVYToYRow_SSE2(const uint8_t* src_uyvy, uint8_t* dst_y, int width) {
  asm volatile(
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "lea         0x20(%0),%0                   \n"
      "psrlw       $0x8,%%xmm0                   \n"
      "psrlw       $0x8,%%xmm1                   \n"
      "packuswb    %%xmm1,%%xmm0                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      : "+r"(src_uyvy),  // %0
        "+r"(dst_y),     // %1
        "+r"(width)      // %2
      :
      : "memory", "cc", "xmm0", "xmm1");
}

void UYVYToUVRow_SSE2(const uint8_t* src_uyvy,
                      int stride_uyvy,
                      uint8_t* dst_u,
                      uint8_t* dst_v,
                      int width) {
  asm volatile(
      "pcmpeqb     %%xmm5,%%xmm5                 \n"
      "psrlw       $0x8,%%xmm5                   \n"
      "sub         %1,%2                         \n"

      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "movdqu      0x00(%0,%4,1),%%xmm2          \n"
      "movdqu      0x10(%0,%4,1),%%xmm3          \n"
      "lea         0x20(%0),%0                   \n"
      "pavgb       %%xmm2,%%xmm0                 \n"
      "pavgb       %%xmm3,%%xmm1                 \n"
      "pand        %%xmm5,%%xmm0                 \n"
      "pand        %%xmm5,%%xmm1                 \n"
      "packuswb    %%xmm1,%%xmm0                 \n"
      "movdqa      %%xmm0,%%xmm1                 \n"
      "pand        %%xmm5,%%xmm0                 \n"
      "packuswb    %%xmm0,%%xmm0                 \n"
      "psrlw       $0x8,%%xmm1                   \n"
      "packuswb    %%xmm1,%%xmm1                 \n"
      "movq        %%xmm0,(%1)                   \n"
      "movq        %%xmm1,0x00(%1,%2,1)          \n"
      "lea         0x8(%1),%1                    \n"
      "sub         $0x10,%3                      \n"
      "jg          1b                            \n"
      : "+r"(src_uyvy),                // %0
        "+r"(dst_u),                   // %1
        "+r"(dst_v),                   // %2
        "+r"(width)                    // %3
      : "r"((ptrdiff_t)(stride_uyvy))  // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm5");
}

void UYVYToUV422Row_SSE2(const uint8_t* src_uyvy,
                         uint8_t* dst_u,
                         uint8_t* dst_v,
                         int width) {
  asm volatile(
      "pcmpeqb     %%xmm5,%%xmm5                 \n"
      "psrlw       $0x8,%%xmm5                   \n"
      "sub         %1,%2                         \n"

      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "lea         0x20(%0),%0                   \n"
      "pand        %%xmm5,%%xmm0                 \n"
      "pand        %%xmm5,%%xmm1                 \n"
      "packuswb    %%xmm1,%%xmm0                 \n"
      "movdqa      %%xmm0,%%xmm1                 \n"
      "pand        %%xmm5,%%xmm0                 \n"
      "packuswb    %%xmm0,%%xmm0                 \n"
      "psrlw       $0x8,%%xmm1                   \n"
      "packuswb    %%xmm1,%%xmm1                 \n"
      "movq        %%xmm0,(%1)                   \n"
      "movq        %%xmm1,0x00(%1,%2,1)          \n"
      "lea         0x8(%1),%1                    \n"
      "sub         $0x10,%3                      \n"
      "jg          1b                            \n"
      : "+r"(src_uyvy),  // %0
        "+r"(dst_u),     // %1
        "+r"(dst_v),     // %2
        "+r"(width)      // %3
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm5");
}
#endif  // HAS_YUY2TOYROW_SSE2

#ifdef HAS_YUY2TOYROW_AVX2
void YUY2ToYRow_AVX2(const uint8_t* src_yuy2, uint8_t* dst_y, int width) {
  asm volatile(
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"
      "vpsrlw      $0x8,%%ymm5,%%ymm5            \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "lea         0x40(%0),%0                   \n"
      "vpand       %%ymm5,%%ymm0,%%ymm0          \n"
      "vpand       %%ymm5,%%ymm1,%%ymm1          \n"
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_yuy2),  // %0
        "+r"(dst_y),     // %1
        "+r"(width)      // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm5");
}

void YUY2ToNVUVRow_AVX2(const uint8_t* src_yuy2,
                        int stride_yuy2,
                        uint8_t* dst_uv,
                        int width) {
  asm volatile(
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "vpavgb      0x00(%0,%3,1),%%ymm0,%%ymm0   \n"
      "vpavgb      0x20(%0,%3,1),%%ymm1,%%ymm1   \n"
      "lea         0x40(%0),%0                   \n"
      "vpsrlw      $0x8,%%ymm0,%%ymm0            \n"
      "vpsrlw      $0x8,%%ymm1,%%ymm1            \n"
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_yuy2),                // %0
        "+r"(dst_uv),                  // %1
        "+r"(width)                    // %2
      : "r"((ptrdiff_t)(stride_yuy2))  // %3
      : "memory", "cc", "xmm0", "xmm1");
}

void YUY2ToUVRow_AVX2(const uint8_t* src_yuy2,
                      int stride_yuy2,
                      uint8_t* dst_u,
                      uint8_t* dst_v,
                      int width) {
  asm volatile(
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"
      "vpsrlw      $0x8,%%ymm5,%%ymm5            \n"
      "sub         %1,%2                         \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "vpavgb      0x00(%0,%4,1),%%ymm0,%%ymm0   \n"
      "vpavgb      0x20(%0,%4,1),%%ymm1,%%ymm1   \n"
      "lea         0x40(%0),%0                   \n"
      "vpsrlw      $0x8,%%ymm0,%%ymm0            \n"
      "vpsrlw      $0x8,%%ymm1,%%ymm1            \n"
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vpand       %%ymm5,%%ymm0,%%ymm1          \n"
      "vpsrlw      $0x8,%%ymm0,%%ymm0            \n"
      "vpackuswb   %%ymm1,%%ymm1,%%ymm1          \n"
      "vpackuswb   %%ymm0,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm1,%%ymm1           \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vextractf128 $0x0,%%ymm1,(%1)             \n"
      "vextractf128 $0x0,%%ymm0,0x00(%1,%2,1)    \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x20,%3                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_yuy2),                // %0
        "+r"(dst_u),                   // %1
        "+r"(dst_v),                   // %2
        "+r"(width)                    // %3
      : "r"((ptrdiff_t)(stride_yuy2))  // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm5");
}

void YUY2ToUV422Row_AVX2(const uint8_t* src_yuy2,
                         uint8_t* dst_u,
                         uint8_t* dst_v,
                         int width) {
  asm volatile(
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"
      "vpsrlw      $0x8,%%ymm5,%%ymm5            \n"
      "sub         %1,%2                         \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "lea         0x40(%0),%0                   \n"
      "vpsrlw      $0x8,%%ymm0,%%ymm0            \n"
      "vpsrlw      $0x8,%%ymm1,%%ymm1            \n"
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vpand       %%ymm5,%%ymm0,%%ymm1          \n"
      "vpsrlw      $0x8,%%ymm0,%%ymm0            \n"
      "vpackuswb   %%ymm1,%%ymm1,%%ymm1          \n"
      "vpackuswb   %%ymm0,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm1,%%ymm1           \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vextractf128 $0x0,%%ymm1,(%1)             \n"
      "vextractf128 $0x0,%%ymm0,0x00(%1,%2,1)    \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x20,%3                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_yuy2),  // %0
        "+r"(dst_u),     // %1
        "+r"(dst_v),     // %2
        "+r"(width)      // %3
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm5");
}

void UYVYToYRow_AVX2(const uint8_t* src_uyvy, uint8_t* dst_y, int width) {
  asm volatile(
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "lea         0x40(%0),%0                   \n"
      "vpsrlw      $0x8,%%ymm0,%%ymm0            \n"
      "vpsrlw      $0x8,%%ymm1,%%ymm1            \n"
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vmovdqu     %%ymm0,(%1)                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_uyvy),  // %0
        "+r"(dst_y),     // %1
        "+r"(width)      // %2
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm5");
}
void UYVYToUVRow_AVX2(const uint8_t* src_uyvy,
                      int stride_uyvy,
                      uint8_t* dst_u,
                      uint8_t* dst_v,
                      int width) {
  asm volatile(
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"
      "vpsrlw      $0x8,%%ymm5,%%ymm5            \n"
      "sub         %1,%2                         \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "vpavgb      0x00(%0,%4,1),%%ymm0,%%ymm0   \n"
      "vpavgb      0x20(%0,%4,1),%%ymm1,%%ymm1   \n"
      "lea         0x40(%0),%0                   \n"
      "vpand       %%ymm5,%%ymm0,%%ymm0          \n"
      "vpand       %%ymm5,%%ymm1,%%ymm1          \n"
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vpand       %%ymm5,%%ymm0,%%ymm1          \n"
      "vpsrlw      $0x8,%%ymm0,%%ymm0            \n"
      "vpackuswb   %%ymm1,%%ymm1,%%ymm1          \n"
      "vpackuswb   %%ymm0,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm1,%%ymm1           \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vextractf128 $0x0,%%ymm1,(%1)             \n"
      "vextractf128 $0x0,%%ymm0,0x00(%1,%2,1)    \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x20,%3                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_uyvy),                // %0
        "+r"(dst_u),                   // %1
        "+r"(dst_v),                   // %2
        "+r"(width)                    // %3
      : "r"((ptrdiff_t)(stride_uyvy))  // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm5");
}

void UYVYToUV422Row_AVX2(const uint8_t* src_uyvy,
                         uint8_t* dst_u,
                         uint8_t* dst_v,
                         int width) {
  asm volatile(
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"
      "vpsrlw      $0x8,%%ymm5,%%ymm5            \n"
      "sub         %1,%2                         \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vmovdqu     0x20(%0),%%ymm1               \n"
      "lea         0x40(%0),%0                   \n"
      "vpand       %%ymm5,%%ymm0,%%ymm0          \n"
      "vpand       %%ymm5,%%ymm1,%%ymm1          \n"
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vpand       %%ymm5,%%ymm0,%%ymm1          \n"
      "vpsrlw      $0x8,%%ymm0,%%ymm0            \n"
      "vpackuswb   %%ymm1,%%ymm1,%%ymm1          \n"
      "vpackuswb   %%ymm0,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm1,%%ymm1           \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vextractf128 $0x0,%%ymm1,(%1)             \n"
      "vextractf128 $0x0,%%ymm0,0x00(%1,%2,1)    \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x20,%3                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_uyvy),  // %0
        "+r"(dst_u),     // %1
        "+r"(dst_v),     // %2
        "+r"(width)      // %3
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm5");
}
#endif  // HAS_YUY2TOYROW_AVX2

#ifdef HAS_ARGBBLENDROW_SSSE3
// Shuffle table for isolating alpha.
static const uvec8 kShuffleAlpha = {3u,  0x80, 3u,  0x80, 7u,  0x80, 7u,  0x80,
                                    11u, 0x80, 11u, 0x80, 15u, 0x80, 15u, 0x80};

// Blend 8 pixels at a time
void ARGBBlendRow_SSSE3(const uint8_t* src_argb,
                        const uint8_t* src_argb1,
                        uint8_t* dst_argb,
                        int width) {
  asm volatile(
      "pcmpeqb     %%xmm7,%%xmm7                 \n"
      "psrlw       $0xf,%%xmm7                   \n"
      "pcmpeqb     %%xmm6,%%xmm6                 \n"
      "psrlw       $0x8,%%xmm6                   \n"
      "pcmpeqb     %%xmm5,%%xmm5                 \n"
      "psllw       $0x8,%%xmm5                   \n"
      "pcmpeqb     %%xmm4,%%xmm4                 \n"
      "pslld       $0x18,%%xmm4                  \n"
      "sub         $0x4,%3                       \n"
      "jl          49f                           \n"

      // 4 pixel loop.
      LABELALIGN
      "40:         \n"
      "movdqu      (%0),%%xmm3                   \n"
      "lea         0x10(%0),%0                   \n"
      "movdqa      %%xmm3,%%xmm0                 \n"
      "pxor        %%xmm4,%%xmm3                 \n"
      "movdqu      (%1),%%xmm2                   \n"
      "pshufb      %4,%%xmm3                     \n"
      "pand        %%xmm6,%%xmm2                 \n"
      "paddw       %%xmm7,%%xmm3                 \n"
      "pmullw      %%xmm3,%%xmm2                 \n"
      "movdqu      (%1),%%xmm1                   \n"
      "lea         0x10(%1),%1                   \n"
      "psrlw       $0x8,%%xmm1                   \n"
      "por         %%xmm4,%%xmm0                 \n"
      "pmullw      %%xmm3,%%xmm1                 \n"
      "psrlw       $0x8,%%xmm2                   \n"
      "paddusb     %%xmm2,%%xmm0                 \n"
      "pand        %%xmm5,%%xmm1                 \n"
      "paddusb     %%xmm1,%%xmm0                 \n"
      "movdqu      %%xmm0,(%2)                   \n"
      "lea         0x10(%2),%2                   \n"
      "sub         $0x4,%3                       \n"
      "jge         40b                           \n"

      "49:         \n"
      "add         $0x3,%3                       \n"
      "jl          99f                           \n"

      // 1 pixel loop.
      "91:         \n"
      "movd        (%0),%%xmm3                   \n"
      "lea         0x4(%0),%0                    \n"
      "movdqa      %%xmm3,%%xmm0                 \n"
      "pxor        %%xmm4,%%xmm3                 \n"
      "movd        (%1),%%xmm2                   \n"
      "pshufb      %4,%%xmm3                     \n"
      "pand        %%xmm6,%%xmm2                 \n"
      "paddw       %%xmm7,%%xmm3                 \n"
      "pmullw      %%xmm3,%%xmm2                 \n"
      "movd        (%1),%%xmm1                   \n"
      "lea         0x4(%1),%1                    \n"
      "psrlw       $0x8,%%xmm1                   \n"
      "por         %%xmm4,%%xmm0                 \n"
      "pmullw      %%xmm3,%%xmm1                 \n"
      "psrlw       $0x8,%%xmm2                   \n"
      "paddusb     %%xmm2,%%xmm0                 \n"
      "pand        %%xmm5,%%xmm1                 \n"
      "paddusb     %%xmm1,%%xmm0                 \n"
      "movd        %%xmm0,(%2)                   \n"
      "lea         0x4(%2),%2                    \n"
      "sub         $0x1,%3                       \n"
      "jge         91b                           \n"
      "99:         \n"
      : "+r"(src_argb),     // %0
        "+r"(src_argb1),    // %1
        "+r"(dst_argb),     // %2
        "+r"(width)         // %3
      : "m"(kShuffleAlpha)  // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_ARGBBLENDROW_SSSE3

#ifdef HAS_BLENDPLANEROW_SSSE3
// Blend 8 pixels at a time.
// unsigned version of math
// =((A2*C2)+(B2*(255-C2))+255)/256
// signed version of math
// =(((A2-128)*C2)+((B2-128)*(255-C2))+32768+127)/256
void BlendPlaneRow_SSSE3(const uint8_t* src0,
                         const uint8_t* src1,
                         const uint8_t* alpha,
                         uint8_t* dst,
                         int width) {
  asm volatile(
      "pcmpeqb     %%xmm5,%%xmm5                 \n"
      "psllw       $0x8,%%xmm5                   \n"
      "pcmpeqb     %%xmm6,%%xmm6                 \n"
      "pabsb       %%xmm6,%%xmm6                 \n"
      "psllw       $7,%%xmm6                     \n"
      "mov         $0x807f807f,%%eax             \n"
      "movd        %%eax,%%xmm7                  \n"
      "pshufd      $0x0,%%xmm7,%%xmm7            \n"
      "sub         %2,%0                         \n"
      "sub         %2,%1                         \n"
      "sub         %2,%3                         \n"

      // 8 pixel loop.
      LABELALIGN
      "1:          \n"
      "movq        (%2),%%xmm0                   \n"
      "punpcklbw   %%xmm0,%%xmm0                 \n"
      "pxor        %%xmm5,%%xmm0                 \n"
      "movq        (%0,%2,1),%%xmm1              \n"
      "movq        (%1,%2,1),%%xmm2              \n"
      "punpcklbw   %%xmm2,%%xmm1                 \n"
      "psubb       %%xmm6,%%xmm1                 \n"
      "pmaddubsw   %%xmm1,%%xmm0                 \n"
      "paddw       %%xmm7,%%xmm0                 \n"
      "psrlw       $0x8,%%xmm0                   \n"
      "packuswb    %%xmm0,%%xmm0                 \n"
      "movq        %%xmm0,(%3,%2,1)              \n"
      "lea         0x8(%2),%2                    \n"
      "sub         $0x8,%4                       \n"
      "jg          1b                            \n"
      : "+r"(src0),   // %0
        "+r"(src1),   // %1
        "+r"(alpha),  // %2
        "+r"(dst),    // %3
        "+rm"(width)  // %4
        ::"memory",
        "cc", "eax", "xmm0", "xmm1", "xmm2", "xmm5", "xmm6", "xmm7");
}
#endif  // HAS_BLENDPLANEROW_SSSE3

#ifdef HAS_BLENDPLANEROW_AVX2
// Blend 32 pixels at a time.
// unsigned version of math
// =((A2*C2)+(B2*(255-C2))+255)/256
// signed version of math
// =(((A2-128)*C2)+((B2-128)*(255-C2))+32768+127)/256
void BlendPlaneRow_AVX2(const uint8_t* src0,
                        const uint8_t* src1,
                        const uint8_t* alpha,
                        uint8_t* dst,
                        int width) {
  asm volatile(
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"
      "vpsllw      $0x8,%%ymm5,%%ymm5            \n"
      "vpcmpeqb    %%ymm6,%%ymm6,%%ymm6          \n"
      "vpabsb      %%ymm6,%%ymm6                 \n"
      "vpsllw      $7,%%ymm6,%%ymm6              \n"
      "mov         $0x807f807f,%%eax             \n"
      "vmovd       %%eax,%%xmm7                  \n"
      "vbroadcastss %%xmm7,%%ymm7                \n"
      "sub         %2,%0                         \n"
      "sub         %2,%1                         \n"
      "sub         %2,%3                         \n"

      // 32 pixel loop.
      LABELALIGN
      "1:          \n"
      "vmovdqu     (%2),%%ymm0                   \n"
      "vpunpckhbw  %%ymm0,%%ymm0,%%ymm3          \n"
      "vpunpcklbw  %%ymm0,%%ymm0,%%ymm0          \n"
      "vpxor       %%ymm5,%%ymm3,%%ymm3          \n"
      "vpxor       %%ymm5,%%ymm0,%%ymm0          \n"
      "vmovdqu     (%0,%2,1),%%ymm1              \n"
      "vmovdqu     (%1,%2,1),%%ymm2              \n"
      "vpunpckhbw  %%ymm2,%%ymm1,%%ymm4          \n"
      "vpunpcklbw  %%ymm2,%%ymm1,%%ymm1          \n"
      "vpsubb      %%ymm6,%%ymm4,%%ymm4          \n"
      "vpsubb      %%ymm6,%%ymm1,%%ymm1          \n"
      "vpmaddubsw  %%ymm4,%%ymm3,%%ymm3          \n"
      "vpmaddubsw  %%ymm1,%%ymm0,%%ymm0          \n"
      "vpaddw      %%ymm7,%%ymm3,%%ymm3          \n"
      "vpaddw      %%ymm7,%%ymm0,%%ymm0          \n"
      "vpsrlw      $0x8,%%ymm3,%%ymm3            \n"
      "vpsrlw      $0x8,%%ymm0,%%ymm0            \n"
      "vpackuswb   %%ymm3,%%ymm0,%%ymm0          \n"
      "vmovdqu     %%ymm0,(%3,%2,1)              \n"
      "lea         0x20(%2),%2                   \n"
      "sub         $0x20,%4                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src0),   // %0
        "+r"(src1),   // %1
        "+r"(alpha),  // %2
        "+r"(dst),    // %3
        "+rm"(width)  // %4
        ::"memory",
        "cc", "eax", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_BLENDPLANEROW_AVX2

#ifdef HAS_BLENDPLANEROW_AVX512BW
// Blend 64 pixels at a time.
// unsigned version of math
// =((A2*C2)+(B2*(255-C2))+255)/256
// signed version of math
// =(((A2-128)*C2)+((B2-128)*(255-C2))+32768+127)/256
void BlendPlaneRow_AVX512BW(const uint8_t* src0,
                            const uint8_t* src1,
                            const uint8_t* alpha,
                            uint8_t* dst,
                            int width) {
  asm volatile(
      "vpternlogd  $0xff,%%zmm17,%%zmm17,%%zmm17 \n"
      "vpsllw      $0x8,%%zmm17,%%zmm16          \n"  // 0xff00
      "vpabsb      %%zmm17,%%zmm17               \n"  // 0x0101
      "vpsllw      $7,%%zmm17,%%zmm17            \n"  // 0x8080
      "mov         $0x807f807f,%%eax             \n"
      "vmovd       %%eax,%%xmm18                 \n"
      "vbroadcastss %%xmm18,%%zmm18              \n"  // 0x807f
      "sub         %2,%0                         \n"
      "sub         %2,%1                         \n"
      "sub         %2,%3                         \n"

      // 64 pixel loop.
      LABELALIGN
      "1:          \n"
      "vmovdqu8    (%2),%%zmm0                   \n"
      "vpunpckhbw  %%zmm0,%%zmm0,%%zmm3          \n"
      "vpunpcklbw  %%zmm0,%%zmm0,%%zmm0          \n"
      "vpxorq      %%zmm16,%%zmm3,%%zmm3         \n"
      "vpxorq      %%zmm16,%%zmm0,%%zmm0         \n"
      "vmovdqu8    (%0,%2,1),%%zmm1              \n"
      "vmovdqu8    (%1,%2,1),%%zmm2              \n"
      "vpunpckhbw  %%zmm2,%%zmm1,%%zmm4          \n"
      "vpunpcklbw  %%zmm2,%%zmm1,%%zmm1          \n"
      "vpsubb      %%zmm17,%%zmm4,%%zmm4         \n"
      "vpsubb      %%zmm17,%%zmm1,%%zmm1         \n"
      "vpmaddubsw  %%zmm4,%%zmm3,%%zmm3          \n"
      "vpmaddubsw  %%zmm1,%%zmm0,%%zmm0          \n"
      "vpaddw      %%zmm18,%%zmm3,%%zmm3         \n"
      "vpaddw      %%zmm18,%%zmm0,%%zmm0         \n"
      "vpsrlw      $0x8,%%zmm3,%%zmm3            \n"
      "vpsrlw      $0x8,%%zmm0,%%zmm0            \n"
      "vpackuswb   %%zmm3,%%zmm0,%%zmm0          \n"
      "vmovdqu8    %%zmm0,(%3,%2,1)              \n"
      "add         $64,%2                        \n"
      "sub         $64,%4                        \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src0),   // %0
        "+r"(src1),   // %1
        "+r"(alpha),  // %2
        "+r"(dst),    // %3
        "+rm"(width)  // %4
        ::"memory",
        "cc", "eax", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm16", "xmm17",
        "xmm18");
}
#endif  // HAS_BLENDPLANEROW_AVX512BW

#ifdef HAS_ARGBATTENUATEROW_SSSE3
// Shuffle table duplicating alpha.
static const vec8 kAttenuateShuffle = {6,    -128, 6,    -128, 6,  -128,
                                       -128, -128, 14,   -128, 14, -128,
                                       14,   -128, -128, -128};

// Attenuate 4 pixels at a time.
void ARGBAttenuateRow_SSSE3(const uint8_t* src_argb,
                            uint8_t* dst_argb,
                            int width) {
  asm volatile(
      "movdqa      %3,%%xmm4                     \n"
      "pcmpeqb     %%xmm5,%%xmm5                 \n"
      "pslld       $0x18,%%xmm5                  \n"
      "pxor        %%xmm6,%%xmm6                 \n"
      "pcmpeqb     %%xmm7,%%xmm7                 \n"
      "punpcklbw   %%xmm6,%%xmm7                 \n"
      "sub         %0,%1                         \n"

      // 4 pixel loop.
      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm6                   \n"
      "movdqa      %%xmm6,%%xmm0                 \n"
      "movdqa      %%xmm6,%%xmm1                 \n"
      "punpcklbw   %%xmm5,%%xmm0                 \n"
      "punpckhbw   %%xmm5,%%xmm1                 \n"
      "movdqa      %%xmm0,%%xmm2                 \n"
      "movdqa      %%xmm1,%%xmm3                 \n"
      "pshufb      %%xmm4,%%xmm2                 \n"  // a,a,a,0
      "pshufb      %%xmm4,%%xmm3                 \n"
      "pmullw      %%xmm2,%%xmm0                 \n"  // rgb * alpha
      "pmullw      %%xmm3,%%xmm1                 \n"
      "paddw       %%xmm7,%%xmm0                 \n"  // + 255
      "paddw       %%xmm7,%%xmm1                 \n"
      "psrlw       $0x8,%%xmm0                   \n"
      "psrlw       $0x8,%%xmm1                   \n"
      "packuswb    %%xmm1,%%xmm0                 \n"
      "pand        %%xmm5,%%xmm6                 \n"
      "por         %%xmm6,%%xmm0                 \n"
      "movdqu      %%xmm0,(%0,%1)                \n"
      "lea         0x10(%0),%0                   \n"
      "sub         $0x4,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src_argb),         // %0
        "+r"(dst_argb),         // %1
        "+r"(width)             // %2
      : "m"(kAttenuateShuffle)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_ARGBATTENUATEROW_SSSE3

#ifdef HAS_ARGBATTENUATEROW_AVX2

// Shuffle table duplicating alpha.
static const lvec8 kAttenuateShuffle_AVX2 = {
    6,    -128, 6,    -128, 6,    -128, -128, -128, 14,   -128, 14,
    -128, 14,   -128, -128, -128, 22,   -128, 22,   -128, 22,   -128,
    -128, -128, 30,   -128, 30,   -128, 30,   -128, -128, -128};

// Attenuate 8 pixels at a time.
void ARGBAttenuateRow_AVX2(const uint8_t* src_argb,
                           uint8_t* dst_argb,
                           int width) {
  asm volatile(
      "vmovdqa     %3,%%ymm4                     \n"
      "vpcmpeqb    %%ymm5,%%ymm5,%%ymm5          \n"
      "vpslld      $0x18,%%ymm5,%%ymm5           \n"
      "vpxor       %%ymm6,%%ymm6,%%ymm6          \n"
      "vpcmpeqb    %%ymm7,%%ymm7,%%ymm7          \n"
      "vpunpcklbw  %%ymm6,%%ymm7,%%ymm7          \n"
      "sub         %0,%1                         \n"

      // 8 pixel loop.
      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm6                   \n"
      "vpunpcklbw  %%ymm5,%%ymm6,%%ymm0          \n"
      "vpunpckhbw  %%ymm5,%%ymm6,%%ymm1          \n"
      "vpshufb     %%ymm4,%%ymm0,%%ymm2          \n"
      "vpshufb     %%ymm4,%%ymm1,%%ymm3          \n"
      "vpmullw     %%ymm2,%%ymm0,%%ymm0          \n"
      "vpmullw     %%ymm3,%%ymm1,%%ymm1          \n"
      "vpaddw      %%ymm7,%%ymm0,%%ymm0          \n"
      "vpaddw      %%ymm7,%%ymm1,%%ymm1          \n"
      "vpsrlw      $0x8,%%ymm0,%%ymm0            \n"
      "vpsrlw      $0x8,%%ymm1,%%ymm1            \n"
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
      "vpand       %%ymm5,%%ymm6,%%ymm1          \n"
      "vpor        %%ymm1,%%ymm0,%%ymm0          \n"
      "vmovdqu     %%ymm0,0x00(%0,%1,1)          \n"
      "lea         0x20(%0),%0                   \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_argb),              // %0
        "+r"(dst_argb),              // %1
        "+r"(width)                  // %2
      : "m"(kAttenuateShuffle_AVX2)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_ARGBATTENUATEROW_AVX2

#ifdef HAS_ARGBUNATTENUATEROW_SSE2
// Unattenuate 4 pixels at a time.
void ARGBUnattenuateRow_SSE2(const uint8_t* src_argb,
                             uint8_t* dst_argb,
                             int width) {
  uintptr_t alpha;
  asm volatile(
      // 4 pixel loop.
      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movzb       0x03(%0),%3                   \n"
      "punpcklbw   %%xmm0,%%xmm0                 \n"
      "movd        0x00(%4,%3,4),%%xmm2          \n"
      "movzb       0x07(%0),%3                   \n"
      "movd        0x00(%4,%3,4),%%xmm3          \n"
      "pshuflw     $0x40,%%xmm2,%%xmm2           \n"
      "pshuflw     $0x40,%%xmm3,%%xmm3           \n"
      "movlhps     %%xmm3,%%xmm2                 \n"
      "pmulhuw     %%xmm2,%%xmm0                 \n"
      "movdqu      (%0),%%xmm1                   \n"
      "movzb       0x0b(%0),%3                   \n"
      "punpckhbw   %%xmm1,%%xmm1                 \n"
      "movd        0x00(%4,%3,4),%%xmm2          \n"
      "movzb       0x0f(%0),%3                   \n"
      "movd        0x00(%4,%3,4),%%xmm3          \n"
      "pshuflw     $0x40,%%xmm2,%%xmm2           \n"
      "pshuflw     $0x40,%%xmm3,%%xmm3           \n"
      "movlhps     %%xmm3,%%xmm2                 \n"
      "pmulhuw     %%xmm2,%%xmm1                 \n"
      "lea         0x10(%0),%0                   \n"
      "packuswb    %%xmm1,%%xmm0                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x4,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src_argb),     // %0
        "+r"(dst_argb),     // %1
        "+r"(width),        // %2
        "=&r"(alpha)        // %3
      : "r"(fixed_invtbl8)  // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_ARGBUNATTENUATEROW_SSE2

#ifdef HAS_ARGBUNATTENUATEROW_AVX2
// Shuffle table duplicating alpha.
static const uvec8 kUnattenShuffleAlpha_AVX2 = {
    0u, 1u, 0u, 1u, 0u, 1u, 6u, 7u, 8u, 9u, 8u, 9u, 8u, 9u, 14u, 15u};
// Unattenuate 8 pixels at a time.
void ARGBUnattenuateRow_AVX2(const uint8_t* src_argb,
                             uint8_t* dst_argb,
                             int width) {
  uintptr_t alpha;
  asm volatile(
      "sub         %0,%1                         \n"
      "vbroadcasti128 %5,%%ymm5                  \n"

      // 8 pixel loop.
      LABELALIGN
      "1:          \n"
      // replace VPGATHER
      "movzb       0x03(%0),%3                   \n"
      "vmovd       0x00(%4,%3,4),%%xmm0          \n"
      "movzb       0x07(%0),%3                   \n"
      "vmovd       0x00(%4,%3,4),%%xmm1          \n"
      "movzb       0x0b(%0),%3                   \n"
      "vpunpckldq  %%xmm1,%%xmm0,%%xmm6          \n"
      "vmovd       0x00(%4,%3,4),%%xmm2          \n"
      "movzb       0x0f(%0),%3                   \n"
      "vmovd       0x00(%4,%3,4),%%xmm3          \n"
      "movzb       0x13(%0),%3                   \n"
      "vpunpckldq  %%xmm3,%%xmm2,%%xmm7          \n"
      "vmovd       0x00(%4,%3,4),%%xmm0          \n"
      "movzb       0x17(%0),%3                   \n"
      "vmovd       0x00(%4,%3,4),%%xmm1          \n"
      "movzb       0x1b(%0),%3                   \n"
      "vpunpckldq  %%xmm1,%%xmm0,%%xmm0          \n"
      "vmovd       0x00(%4,%3,4),%%xmm2          \n"
      "movzb       0x1f(%0),%3                   \n"
      "vmovd       0x00(%4,%3,4),%%xmm3          \n"
      "vpunpckldq  %%xmm3,%%xmm2,%%xmm2          \n"
      "vpunpcklqdq %%xmm7,%%xmm6,%%xmm3          \n"
      "vpunpcklqdq %%xmm2,%%xmm0,%%xmm0          \n"
      "vinserti128 $0x1,%%xmm0,%%ymm3,%%ymm3     \n"
      // end of VPGATHER

      "vmovdqu     (%0),%%ymm6                   \n"
      "vpunpcklbw  %%ymm6,%%ymm6,%%ymm0          \n"
      "vpunpckhbw  %%ymm6,%%ymm6,%%ymm1          \n"
      "vpunpcklwd  %%ymm3,%%ymm3,%%ymm2          \n"
      "vpunpckhwd  %%ymm3,%%ymm3,%%ymm3          \n"
      "vpshufb     %%ymm5,%%ymm2,%%ymm2          \n"
      "vpshufb     %%ymm5,%%ymm3,%%ymm3          \n"
      "vpmulhuw    %%ymm2,%%ymm0,%%ymm0          \n"
      "vpmulhuw    %%ymm3,%%ymm1,%%ymm1          \n"
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
      "vmovdqu     %%ymm0,0x00(%0,%1,1)          \n"
      "lea         0x20(%0),%0                   \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_argb),                 // %0
        "+r"(dst_argb),                 // %1
        "+r"(width),                    // %2
        "=&r"(alpha)                    // %3
      : "r"(fixed_invtbl8),             // %4
        "m"(kUnattenShuffleAlpha_AVX2)  // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_ARGBUNATTENUATEROW_AVX2

#ifdef HAS_ARGBGRAYROW_SSSE3
// Convert 8 ARGB pixels (64 bytes) to 8 Gray ARGB pixels
void ARGBGrayRow_SSSE3(const uint8_t* src_argb, uint8_t* dst_argb, int width) {
  asm volatile(
      "movdqa      %3,%%xmm4                     \n"
      "movdqa      %4,%%xmm5                     \n"

      // 8 pixel loop.
      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "psubb       %%xmm5,%%xmm0                 \n"
      "psubb       %%xmm5,%%xmm1                 \n"
      "movdqu      %%xmm4,%%xmm6                 \n"
      "pmaddubsw   %%xmm0,%%xmm6                 \n"
      "movdqu      %%xmm4,%%xmm0                 \n"
      "pmaddubsw   %%xmm1,%%xmm0                 \n"
      "phaddw      %%xmm0,%%xmm6                 \n"
      "paddw       %%xmm5,%%xmm6                 \n"
      "psrlw       $0x8,%%xmm6                   \n"
      "packuswb    %%xmm6,%%xmm6                 \n"
      "movdqu      (%0),%%xmm2                   \n"
      "movdqu      0x10(%0),%%xmm3               \n"
      "lea         0x20(%0),%0                   \n"
      "psrld       $0x18,%%xmm2                  \n"
      "psrld       $0x18,%%xmm3                  \n"
      "packuswb    %%xmm3,%%xmm2                 \n"
      "packuswb    %%xmm2,%%xmm2                 \n"
      "movdqa      %%xmm6,%%xmm3                 \n"
      "punpcklbw   %%xmm6,%%xmm6                 \n"
      "punpcklbw   %%xmm2,%%xmm3                 \n"
      "movdqa      %%xmm6,%%xmm1                 \n"
      "punpcklwd   %%xmm3,%%xmm6                 \n"
      "punpckhwd   %%xmm3,%%xmm1                 \n"
      "movdqu      %%xmm6,(%1)                   \n"
      "movdqu      %%xmm1,0x10(%1)               \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src_argb),  // %0
        "+r"(dst_argb),  // %1
        "+r"(width)      // %2
      : "m"(kARGBToYJ),  // %3
        "m"(kSub128)     // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}
#endif  // HAS_ARGBGRAYROW_SSSE3

#ifdef HAS_ARGBSEPIAROW_SSSE3
//    b = (r * 35 + g * 68 + b * 17) >> 7
//    g = (r * 45 + g * 88 + b * 22) >> 7
//    r = (r * 50 + g * 98 + b * 24) >> 7
// Constant for ARGB color to sepia tone
static const vec8 kARGBToSepiaB = {17, 68, 35, 0, 17, 68, 35, 0,
                                   17, 68, 35, 0, 17, 68, 35, 0};

static const vec8 kARGBToSepiaG = {22, 88, 45, 0, 22, 88, 45, 0,
                                   22, 88, 45, 0, 22, 88, 45, 0};

static const vec8 kARGBToSepiaR = {24, 98, 50, 0, 24, 98, 50, 0,
                                   24, 98, 50, 0, 24, 98, 50, 0};

// Convert 8 ARGB pixels (32 bytes) to 8 Sepia ARGB pixels.
void ARGBSepiaRow_SSSE3(uint8_t* dst_argb, int width) {
  asm volatile(
      "movdqa      %2,%%xmm2                     \n"
      "movdqa      %3,%%xmm3                     \n"
      "movdqa      %4,%%xmm4                     \n"

      // 8 pixel loop.
      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm6               \n"
      "pmaddubsw   %%xmm2,%%xmm0                 \n"
      "pmaddubsw   %%xmm2,%%xmm6                 \n"
      "phaddw      %%xmm6,%%xmm0                 \n"
      "psrlw       $0x7,%%xmm0                   \n"
      "packuswb    %%xmm0,%%xmm0                 \n"
      "movdqu      (%0),%%xmm5                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "pmaddubsw   %%xmm3,%%xmm5                 \n"
      "pmaddubsw   %%xmm3,%%xmm1                 \n"
      "phaddw      %%xmm1,%%xmm5                 \n"
      "psrlw       $0x7,%%xmm5                   \n"
      "packuswb    %%xmm5,%%xmm5                 \n"
      "punpcklbw   %%xmm5,%%xmm0                 \n"
      "movdqu      (%0),%%xmm5                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "pmaddubsw   %%xmm4,%%xmm5                 \n"
      "pmaddubsw   %%xmm4,%%xmm1                 \n"
      "phaddw      %%xmm1,%%xmm5                 \n"
      "psrlw       $0x7,%%xmm5                   \n"
      "packuswb    %%xmm5,%%xmm5                 \n"
      "movdqu      (%0),%%xmm6                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "psrld       $0x18,%%xmm6                  \n"
      "psrld       $0x18,%%xmm1                  \n"
      "packuswb    %%xmm1,%%xmm6                 \n"
      "packuswb    %%xmm6,%%xmm6                 \n"
      "punpcklbw   %%xmm6,%%xmm5                 \n"
      "movdqa      %%xmm0,%%xmm1                 \n"
      "punpcklwd   %%xmm5,%%xmm0                 \n"
      "punpckhwd   %%xmm5,%%xmm1                 \n"
      "movdqu      %%xmm0,(%0)                   \n"
      "movdqu      %%xmm1,0x10(%0)               \n"
      "lea         0x20(%0),%0                   \n"
      "sub         $0x8,%1                       \n"
      "jg          1b                            \n"
      : "+r"(dst_argb),      // %0
        "+r"(width)          // %1
      : "m"(kARGBToSepiaB),  // %2
        "m"(kARGBToSepiaG),  // %3
        "m"(kARGBToSepiaR)   // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}
#endif  // HAS_ARGBSEPIAROW_SSSE3

#ifdef HAS_ARGBCOLORMATRIXROW_SSSE3
// Tranform 8 ARGB pixels (32 bytes) with color matrix.
// Same as Sepia except matrix is provided.
void ARGBColorMatrixRow_SSSE3(const uint8_t* src_argb,
                              uint8_t* dst_argb,
                              const int8_t* matrix_argb,
                              int width) {
  asm volatile(
      "movdqu      (%3),%%xmm5                   \n"
      "pshufd      $0x00,%%xmm5,%%xmm2           \n"
      "pshufd      $0x55,%%xmm5,%%xmm3           \n"
      "pshufd      $0xaa,%%xmm5,%%xmm4           \n"
      "pshufd      $0xff,%%xmm5,%%xmm5           \n"

      // 8 pixel loop.
      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm7               \n"
      "pmaddubsw   %%xmm2,%%xmm0                 \n"
      "pmaddubsw   %%xmm2,%%xmm7                 \n"
      "movdqu      (%0),%%xmm6                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "pmaddubsw   %%xmm3,%%xmm6                 \n"
      "pmaddubsw   %%xmm3,%%xmm1                 \n"
      "phaddsw     %%xmm7,%%xmm0                 \n"
      "phaddsw     %%xmm1,%%xmm6                 \n"
      "psraw       $0x6,%%xmm0                   \n"
      "psraw       $0x6,%%xmm6                   \n"
      "packuswb    %%xmm0,%%xmm0                 \n"
      "packuswb    %%xmm6,%%xmm6                 \n"
      "punpcklbw   %%xmm6,%%xmm0                 \n"
      "movdqu      (%0),%%xmm1                   \n"
      "movdqu      0x10(%0),%%xmm7               \n"
      "pmaddubsw   %%xmm4,%%xmm1                 \n"
      "pmaddubsw   %%xmm4,%%xmm7                 \n"
      "phaddsw     %%xmm7,%%xmm1                 \n"
      "movdqu      (%0),%%xmm6                   \n"
      "movdqu      0x10(%0),%%xmm7               \n"
      "pmaddubsw   %%xmm5,%%xmm6                 \n"
      "pmaddubsw   %%xmm5,%%xmm7                 \n"
      "phaddsw     %%xmm7,%%xmm6                 \n"
      "psraw       $0x6,%%xmm1                   \n"
      "psraw       $0x6,%%xmm6                   \n"
      "packuswb    %%xmm1,%%xmm1                 \n"
      "packuswb    %%xmm6,%%xmm6                 \n"
      "punpcklbw   %%xmm6,%%xmm1                 \n"
      "movdqa      %%xmm0,%%xmm6                 \n"
      "punpcklwd   %%xmm1,%%xmm0                 \n"
      "punpckhwd   %%xmm1,%%xmm6                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "movdqu      %%xmm6,0x10(%1)               \n"
      "lea         0x20(%0),%0                   \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x8,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src_argb),   // %0
        "+r"(dst_argb),   // %1
        "+r"(width)       // %2
      : "r"(matrix_argb)  // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_ARGBCOLORMATRIXROW_SSSE3

#ifdef HAS_ARGBQUANTIZEROW_SSE2
// Quantize 4 ARGB pixels (16 bytes).
void ARGBQuantizeRow_SSE2(uint8_t* dst_argb,
                          int scale,
                          int interval_size,
                          int interval_offset,
                          int width) {
  asm volatile(
      "movd        %2,%%xmm2                     \n"
      "movd        %3,%%xmm3                     \n"
      "movd        %4,%%xmm4                     \n"
      "pshuflw     $0x40,%%xmm2,%%xmm2           \n"
      "pshufd      $0x44,%%xmm2,%%xmm2           \n"
      "pshuflw     $0x40,%%xmm3,%%xmm3           \n"
      "pshufd      $0x44,%%xmm3,%%xmm3           \n"
      "pshuflw     $0x40,%%xmm4,%%xmm4           \n"
      "pshufd      $0x44,%%xmm4,%%xmm4           \n"
      "pxor        %%xmm5,%%xmm5                 \n"
      "pcmpeqb     %%xmm6,%%xmm6                 \n"
      "pslld       $0x18,%%xmm6                  \n"

      // 4 pixel loop.
      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "punpcklbw   %%xmm5,%%xmm0                 \n"
      "pmulhuw     %%xmm2,%%xmm0                 \n"
      "movdqu      (%0),%%xmm1                   \n"
      "punpckhbw   %%xmm5,%%xmm1                 \n"
      "pmulhuw     %%xmm2,%%xmm1                 \n"
      "pmullw      %%xmm3,%%xmm0                 \n"
      "movdqu      (%0),%%xmm7                   \n"
      "pmullw      %%xmm3,%%xmm1                 \n"
      "pand        %%xmm6,%%xmm7                 \n"
      "paddw       %%xmm4,%%xmm0                 \n"
      "paddw       %%xmm4,%%xmm1                 \n"
      "packuswb    %%xmm1,%%xmm0                 \n"
      "por         %%xmm7,%%xmm0                 \n"
      "movdqu      %%xmm0,(%0)                   \n"
      "lea         0x10(%0),%0                   \n"
      "sub         $0x4,%1                       \n"
      "jg          1b                            \n"
      : "+r"(dst_argb),       // %0
        "+r"(width)           // %1
      : "r"(scale),           // %2
        "r"(interval_size),   // %3
        "r"(interval_offset)  // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_ARGBQUANTIZEROW_SSE2

#ifdef HAS_ARGBSHADEROW_SSE2
// Shade 4 pixels at a time by specified value.
void ARGBShadeRow_SSE2(const uint8_t* src_argb,
                       uint8_t* dst_argb,
                       int width,
                       uint32_t value) {
  asm volatile(
      "movd        %3,%%xmm2                     \n"
      "punpcklbw   %%xmm2,%%xmm2                 \n"
      "punpcklqdq  %%xmm2,%%xmm2                 \n"

      // 4 pixel loop.
      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "lea         0x10(%0),%0                   \n"
      "movdqa      %%xmm0,%%xmm1                 \n"
      "punpcklbw   %%xmm0,%%xmm0                 \n"
      "punpckhbw   %%xmm1,%%xmm1                 \n"
      "pmulhuw     %%xmm2,%%xmm0                 \n"
      "pmulhuw     %%xmm2,%%xmm1                 \n"
      "psrlw       $0x8,%%xmm0                   \n"
      "psrlw       $0x8,%%xmm1                   \n"
      "packuswb    %%xmm1,%%xmm0                 \n"
      "movdqu      %%xmm0,(%1)                   \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x4,%2                       \n"
      "jg          1b                            \n"
      : "+r"(src_argb),  // %0
        "+r"(dst_argb),  // %1
        "+r"(width)      // %2
      : "r"(value)       // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_ARGBSHADEROW_SSE2

#ifdef HAS_ARGBMULTIPLYROW_SSE2
// Multiply 2 rows of ARGB pixels together, 4 pixels at a time.
void ARGBMultiplyRow_SSE2(const uint8_t* src_argb,
                          const uint8_t* src_argb1,
                          uint8_t* dst_argb,
                          int width) {
  asm volatile(
      "pxor        %%xmm5,%%xmm5                 \n"
      "pcmpeqw     %%xmm4,%%xmm4                 \n"
      "psrlw       $0xf,%%xmm4                   \n"
      "psllw       $0x7,%%xmm4                   \n"

      // 4 pixel loop.
      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "lea         0x10(%0),%0                   \n"
      "movdqu      (%1),%%xmm2                   \n"
      "lea         0x10(%1),%1                   \n"
      "movdqa      %%xmm0,%%xmm1                 \n"
      "movdqa      %%xmm2,%%xmm3                 \n"
      "punpcklbw   %%xmm5,%%xmm0                 \n"
      "punpckhbw   %%xmm5,%%xmm1                 \n"
      "punpcklbw   %%xmm5,%%xmm2                 \n"
      "punpckhbw   %%xmm5,%%xmm3                 \n"
      "pmullw      %%xmm2,%%xmm0                 \n"
      "pmullw      %%xmm3,%%xmm1                 \n"
      "paddw       %%xmm4,%%xmm0                 \n"
      "paddw       %%xmm4,%%xmm1                 \n"
      "psrlw       $0x8,%%xmm0                   \n"
      "psrlw       $0x8,%%xmm1                   \n"
      "packuswb    %%xmm1,%%xmm0                 \n"
      "movdqu      %%xmm0,(%2)                   \n"
      "lea         0x10(%2),%2                   \n"
      "sub         $0x4,%3                       \n"
      "jg          1b                            \n"
      : "+r"(src_argb),   // %0
        "+r"(src_argb1),  // %1
        "+r"(dst_argb),   // %2
        "+r"(width)       // %3
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_ARGBMULTIPLYROW_SSE2

#ifdef HAS_ARGBMULTIPLYROW_AVX2
// Multiply 2 rows of ARGB pixels together, 8 pixels at a time.
void ARGBMultiplyRow_AVX2(const uint8_t* src_argb,
                          const uint8_t* src_argb1,
                          uint8_t* dst_argb,
                          int width) {
  asm volatile("vpxor       %%ymm5,%%ymm5,%%ymm5          \n"

               // 8 pixel loop.
               LABELALIGN
               "1:          \n"
               "vmovdqu     (%0),%%ymm1                   \n"
               "lea         0x20(%0),%0                   \n"
               "vmovdqu     (%1),%%ymm3                   \n"
               "lea         0x20(%1),%1                   \n"
               "vpunpcklbw  %%ymm1,%%ymm5,%%ymm0          \n"
               "vpunpckhbw  %%ymm1,%%ymm5,%%ymm1          \n"
               "vpsrlw      $0x1,%%ymm0,%%ymm0            \n"
               "vpsrlw      $0x1,%%ymm1,%%ymm1            \n"
               "vpunpcklbw  %%ymm5,%%ymm3,%%ymm2          \n"
               "vpunpckhbw  %%ymm5,%%ymm3,%%ymm3          \n"
               "vpmulhrsw   %%ymm2,%%ymm0,%%ymm0          \n"
               "vpmulhrsw   %%ymm3,%%ymm1,%%ymm1          \n"
               "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
               "vmovdqu     %%ymm0,(%2)                   \n"
               "lea         0x20(%2),%2                   \n"
               "sub         $0x8,%3                       \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : "+r"(src_argb),   // %0
                 "+r"(src_argb1),  // %1
                 "+r"(dst_argb),   // %2
                 "+r"(width)       // %3
               :
               : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm5");
}
#endif  // HAS_ARGBMULTIPLYROW_AVX2

#ifdef HAS_ARGBADDROW_SSE2
// Add 2 rows of ARGB pixels together, 4 pixels at a time.
void ARGBAddRow_SSE2(const uint8_t* src_argb,
                     const uint8_t* src_argb1,
                     uint8_t* dst_argb,
                     int width) {
  asm volatile(
      // 4 pixel loop.
      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "lea         0x10(%0),%0                   \n"
      "movdqu      (%1),%%xmm1                   \n"
      "lea         0x10(%1),%1                   \n"
      "paddusb     %%xmm1,%%xmm0                 \n"
      "movdqu      %%xmm0,(%2)                   \n"
      "lea         0x10(%2),%2                   \n"
      "sub         $0x4,%3                       \n"
      "jg          1b                            \n"
      : "+r"(src_argb),   // %0
        "+r"(src_argb1),  // %1
        "+r"(dst_argb),   // %2
        "+r"(width)       // %3
      :
      : "memory", "cc", "xmm0", "xmm1");
}
#endif  // HAS_ARGBADDROW_SSE2

#ifdef HAS_ARGBADDROW_AVX2
// Add 2 rows of ARGB pixels together, 4 pixels at a time.
void ARGBAddRow_AVX2(const uint8_t* src_argb,
                     const uint8_t* src_argb1,
                     uint8_t* dst_argb,
                     int width) {
  asm volatile(
      // 4 pixel loop.
      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "lea         0x20(%0),%0                   \n"
      "vpaddusb    (%1),%%ymm0,%%ymm0            \n"
      "lea         0x20(%1),%1                   \n"
      "vmovdqu     %%ymm0,(%2)                   \n"
      "lea         0x20(%2),%2                   \n"
      "sub         $0x8,%3                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_argb),   // %0
        "+r"(src_argb1),  // %1
        "+r"(dst_argb),   // %2
        "+r"(width)       // %3
      :
      : "memory", "cc", "xmm0");
}
#endif  // HAS_ARGBADDROW_AVX2

#ifdef HAS_ARGBSUBTRACTROW_SSE2
// Subtract 2 rows of ARGB pixels, 4 pixels at a time.
void ARGBSubtractRow_SSE2(const uint8_t* src_argb,
                          const uint8_t* src_argb1,
                          uint8_t* dst_argb,
                          int width) {
  asm volatile(
      // 4 pixel loop.
      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "lea         0x10(%0),%0                   \n"
      "movdqu      (%1),%%xmm1                   \n"
      "lea         0x10(%1),%1                   \n"
      "psubusb     %%xmm1,%%xmm0                 \n"
      "movdqu      %%xmm0,(%2)                   \n"
      "lea         0x10(%2),%2                   \n"
      "sub         $0x4,%3                       \n"
      "jg          1b                            \n"
      : "+r"(src_argb),   // %0
        "+r"(src_argb1),  // %1
        "+r"(dst_argb),   // %2
        "+r"(width)       // %3
      :
      : "memory", "cc", "xmm0", "xmm1");
}
#endif  // HAS_ARGBSUBTRACTROW_SSE2

#ifdef HAS_ARGBSUBTRACTROW_AVX2
// Subtract 2 rows of ARGB pixels, 8 pixels at a time.
void ARGBSubtractRow_AVX2(const uint8_t* src_argb,
                          const uint8_t* src_argb1,
                          uint8_t* dst_argb,
                          int width) {
  asm volatile(
      // 4 pixel loop.
      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "lea         0x20(%0),%0                   \n"
      "vpsubusb    (%1),%%ymm0,%%ymm0            \n"
      "lea         0x20(%1),%1                   \n"
      "vmovdqu     %%ymm0,(%2)                   \n"
      "lea         0x20(%2),%2                   \n"
      "sub         $0x8,%3                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_argb),   // %0
        "+r"(src_argb1),  // %1
        "+r"(dst_argb),   // %2
        "+r"(width)       // %3
      :
      : "memory", "cc", "xmm0");
}
#endif  // HAS_ARGBSUBTRACTROW_AVX2

#ifdef HAS_SOBELXROW_SSE2
// SobelX as a matrix is
// -1  0  1
// -2  0  2
// -1  0  1
void SobelXRow_SSE2(const uint8_t* src_y0,
                    const uint8_t* src_y1,
                    const uint8_t* src_y2,
                    uint8_t* dst_sobelx,
                    int width) {
  asm volatile(
      "sub         %0,%1                         \n"
      "sub         %0,%2                         \n"
      "sub         %0,%3                         \n"
      "pxor        %%xmm5,%%xmm5                 \n"

      // 8 pixel loop.
      LABELALIGN
      "1:          \n"
      "movq        (%0),%%xmm0                   \n"
      "movq        0x2(%0),%%xmm1                \n"
      "punpcklbw   %%xmm5,%%xmm0                 \n"
      "punpcklbw   %%xmm5,%%xmm1                 \n"
      "psubw       %%xmm1,%%xmm0                 \n"
      "movq        0x00(%0,%1,1),%%xmm1          \n"
      "movq        0x02(%0,%1,1),%%xmm2          \n"
      "punpcklbw   %%xmm5,%%xmm1                 \n"
      "punpcklbw   %%xmm5,%%xmm2                 \n"
      "psubw       %%xmm2,%%xmm1                 \n"
      "movq        0x00(%0,%2,1),%%xmm2          \n"
      "movq        0x02(%0,%2,1),%%xmm3          \n"
      "punpcklbw   %%xmm5,%%xmm2                 \n"
      "punpcklbw   %%xmm5,%%xmm3                 \n"
      "psubw       %%xmm3,%%xmm2                 \n"
      "paddw       %%xmm2,%%xmm0                 \n"
      "paddw       %%xmm1,%%xmm0                 \n"
      "paddw       %%xmm1,%%xmm0                 \n"
      "pxor        %%xmm1,%%xmm1                 \n"
      "psubw       %%xmm0,%%xmm1                 \n"
      "pmaxsw      %%xmm1,%%xmm0                 \n"
      "packuswb    %%xmm0,%%xmm0                 \n"
      "movq        %%xmm0,0x00(%0,%3,1)          \n"
      "lea         0x8(%0),%0                    \n"
      "sub         $0x8,%4                       \n"
      "jg          1b                            \n"
      : "+r"(src_y0),      // %0
        "+r"(src_y1),      // %1
        "+r"(src_y2),      // %2
        "+r"(dst_sobelx),  // %3
        "+r"(width)        // %4
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm5");
}
#endif  // HAS_SOBELXROW_SSE2

#ifdef HAS_SOBELYROW_SSE2
// SobelY as a matrix is
// -1 -2 -1
//  0  0  0
//  1  2  1
void SobelYRow_SSE2(const uint8_t* src_y0,
                    const uint8_t* src_y1,
                    uint8_t* dst_sobely,
                    int width) {
  asm volatile(
      "sub         %0,%1                         \n"
      "sub         %0,%2                         \n"
      "pxor        %%xmm5,%%xmm5                 \n"

      // 8 pixel loop.
      LABELALIGN
      "1:          \n"
      "movq        (%0),%%xmm0                   \n"
      "movq        0x00(%0,%1,1),%%xmm1          \n"
      "punpcklbw   %%xmm5,%%xmm0                 \n"
      "punpcklbw   %%xmm5,%%xmm1                 \n"
      "psubw       %%xmm1,%%xmm0                 \n"
      "movq        0x1(%0),%%xmm1                \n"
      "movq        0x01(%0,%1,1),%%xmm2          \n"
      "punpcklbw   %%xmm5,%%xmm1                 \n"
      "punpcklbw   %%xmm5,%%xmm2                 \n"
      "psubw       %%xmm2,%%xmm1                 \n"
      "movq        0x2(%0),%%xmm2                \n"
      "movq        0x02(%0,%1,1),%%xmm3          \n"
      "punpcklbw   %%xmm5,%%xmm2                 \n"
      "punpcklbw   %%xmm5,%%xmm3                 \n"
      "psubw       %%xmm3,%%xmm2                 \n"
      "paddw       %%xmm2,%%xmm0                 \n"
      "paddw       %%xmm1,%%xmm0                 \n"
      "paddw       %%xmm1,%%xmm0                 \n"
      "pxor        %%xmm1,%%xmm1                 \n"
      "psubw       %%xmm0,%%xmm1                 \n"
      "pmaxsw      %%xmm1,%%xmm0                 \n"
      "packuswb    %%xmm0,%%xmm0                 \n"
      "movq        %%xmm0,0x00(%0,%2,1)          \n"
      "lea         0x8(%0),%0                    \n"
      "sub         $0x8,%3                       \n"
      "jg          1b                            \n"
      : "+r"(src_y0),      // %0
        "+r"(src_y1),      // %1
        "+r"(dst_sobely),  // %2
        "+r"(width)        // %3
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm5");
}
#endif  // HAS_SOBELYROW_SSE2

#ifdef HAS_SOBELROW_SSE2
// Adds Sobel X and Sobel Y and stores Sobel into ARGB.
// A = 255
// R = Sobel
// G = Sobel
// B = Sobel
void SobelRow_SSE2(const uint8_t* src_sobelx,
                   const uint8_t* src_sobely,
                   uint8_t* dst_argb,
                   int width) {
  asm volatile(
      "sub         %0,%1                         \n"
      "pcmpeqb     %%xmm5,%%xmm5                 \n"
      "pslld       $0x18,%%xmm5                  \n"

      // 8 pixel loop.
      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x00(%0,%1,1),%%xmm1          \n"
      "lea         0x10(%0),%0                   \n"
      "paddusb     %%xmm1,%%xmm0                 \n"
      "movdqa      %%xmm0,%%xmm2                 \n"
      "punpcklbw   %%xmm0,%%xmm2                 \n"
      "punpckhbw   %%xmm0,%%xmm0                 \n"
      "movdqa      %%xmm2,%%xmm1                 \n"
      "punpcklwd   %%xmm2,%%xmm1                 \n"
      "punpckhwd   %%xmm2,%%xmm2                 \n"
      "por         %%xmm5,%%xmm1                 \n"
      "por         %%xmm5,%%xmm2                 \n"
      "movdqa      %%xmm0,%%xmm3                 \n"
      "punpcklwd   %%xmm0,%%xmm3                 \n"
      "punpckhwd   %%xmm0,%%xmm0                 \n"
      "por         %%xmm5,%%xmm3                 \n"
      "por         %%xmm5,%%xmm0                 \n"
      "movdqu      %%xmm1,(%2)                   \n"
      "movdqu      %%xmm2,0x10(%2)               \n"
      "movdqu      %%xmm3,0x20(%2)               \n"
      "movdqu      %%xmm0,0x30(%2)               \n"
      "lea         0x40(%2),%2                   \n"
      "sub         $0x10,%3                      \n"
      "jg          1b                            \n"
      : "+r"(src_sobelx),  // %0
        "+r"(src_sobely),  // %1
        "+r"(dst_argb),    // %2
        "+r"(width)        // %3
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm5");
}
#endif  // HAS_SOBELROW_SSE2

#ifdef HAS_SOBELTOPLANEROW_SSE2
// Adds Sobel X and Sobel Y and stores Sobel into a plane.
void SobelToPlaneRow_SSE2(const uint8_t* src_sobelx,
                          const uint8_t* src_sobely,
                          uint8_t* dst_y,
                          int width) {
  asm volatile("sub         %0,%1                         \n"

               // 8 pixel loop.
               LABELALIGN
               "1:          \n"
               "movdqu      (%0),%%xmm0                   \n"
               "movdqu      0x00(%0,%1,1),%%xmm1          \n"
               "lea         0x10(%0),%0                   \n"
               "paddusb     %%xmm1,%%xmm0                 \n"
               "movdqu      %%xmm0,(%2)                   \n"
               "lea         0x10(%2),%2                   \n"
               "sub         $0x10,%3                      \n"
               "jg          1b                            \n"
               : "+r"(src_sobelx),  // %0
                 "+r"(src_sobely),  // %1
                 "+r"(dst_y),       // %2
                 "+r"(width)        // %3
               :
               : "memory", "cc", "xmm0", "xmm1");
}
#endif  // HAS_SOBELTOPLANEROW_SSE2

#ifdef HAS_SOBELXYROW_SSE2
// Mixes Sobel X, Sobel Y and Sobel into ARGB.
// A = 255
// R = Sobel X
// G = Sobel
// B = Sobel Y
void SobelXYRow_SSE2(const uint8_t* src_sobelx,
                     const uint8_t* src_sobely,
                     uint8_t* dst_argb,
                     int width) {
  asm volatile(
      "sub         %0,%1                         \n"
      "pcmpeqb     %%xmm5,%%xmm5                 \n"

      // 8 pixel loop.
      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x00(%0,%1,1),%%xmm1          \n"
      "lea         0x10(%0),%0                   \n"
      "movdqa      %%xmm0,%%xmm2                 \n"
      "paddusb     %%xmm1,%%xmm2                 \n"
      "movdqa      %%xmm0,%%xmm3                 \n"
      "punpcklbw   %%xmm5,%%xmm3                 \n"
      "punpckhbw   %%xmm5,%%xmm0                 \n"
      "movdqa      %%xmm1,%%xmm4                 \n"
      "punpcklbw   %%xmm2,%%xmm4                 \n"
      "punpckhbw   %%xmm2,%%xmm1                 \n"
      "movdqa      %%xmm4,%%xmm6                 \n"
      "punpcklwd   %%xmm3,%%xmm6                 \n"
      "punpckhwd   %%xmm3,%%xmm4                 \n"
      "movdqa      %%xmm1,%%xmm7                 \n"
      "punpcklwd   %%xmm0,%%xmm7                 \n"
      "punpckhwd   %%xmm0,%%xmm1                 \n"
      "movdqu      %%xmm6,(%2)                   \n"
      "movdqu      %%xmm4,0x10(%2)               \n"
      "movdqu      %%xmm7,0x20(%2)               \n"
      "movdqu      %%xmm1,0x30(%2)               \n"
      "lea         0x40(%2),%2                   \n"
      "sub         $0x10,%3                      \n"
      "jg          1b                            \n"
      : "+r"(src_sobelx),  // %0
        "+r"(src_sobely),  // %1
        "+r"(dst_argb),    // %2
        "+r"(width)        // %3
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_SOBELXYROW_SSE2

#ifdef HAS_COMPUTECUMULATIVESUMROW_SSE2
// Creates a table of cumulative sums where each value is a sum of all values
// above and to the left of the value, inclusive of the value.
void ComputeCumulativeSumRow_SSE2(const uint8_t* row,
                                  int32_t* cumsum,
                                  const int32_t* previous_cumsum,
                                  int width) {
  asm volatile(
      "pxor        %%xmm0,%%xmm0                 \n"
      "pxor        %%xmm1,%%xmm1                 \n"
      "sub         $0x4,%3                       \n"
      "jl          49f                           \n"
      "test        $0xf,%1                       \n"
      "jne         49f                           \n"

      // 4 pixel loop.
      LABELALIGN
      "40:         \n"
      "movdqu      (%0),%%xmm2                   \n"
      "lea         0x10(%0),%0                   \n"
      "movdqa      %%xmm2,%%xmm4                 \n"
      "punpcklbw   %%xmm1,%%xmm2                 \n"
      "movdqa      %%xmm2,%%xmm3                 \n"
      "punpcklwd   %%xmm1,%%xmm2                 \n"
      "punpckhwd   %%xmm1,%%xmm3                 \n"
      "punpckhbw   %%xmm1,%%xmm4                 \n"
      "movdqa      %%xmm4,%%xmm5                 \n"
      "punpcklwd   %%xmm1,%%xmm4                 \n"
      "punpckhwd   %%xmm1,%%xmm5                 \n"
      "paddd       %%xmm2,%%xmm0                 \n"
      "movdqu      (%2),%%xmm2                   \n"
      "paddd       %%xmm0,%%xmm2                 \n"
      "paddd       %%xmm3,%%xmm0                 \n"
      "movdqu      0x10(%2),%%xmm3               \n"
      "paddd       %%xmm0,%%xmm3                 \n"
      "paddd       %%xmm4,%%xmm0                 \n"
      "movdqu      0x20(%2),%%xmm4               \n"
      "paddd       %%xmm0,%%xmm4                 \n"
      "paddd       %%xmm5,%%xmm0                 \n"
      "movdqu      0x30(%2),%%xmm5               \n"
      "lea         0x40(%2),%2                   \n"
      "paddd       %%xmm0,%%xmm5                 \n"
      "movdqu      %%xmm2,(%1)                   \n"
      "movdqu      %%xmm3,0x10(%1)               \n"
      "movdqu      %%xmm4,0x20(%1)               \n"
      "movdqu      %%xmm5,0x30(%1)               \n"
      "lea         0x40(%1),%1                   \n"
      "sub         $0x4,%3                       \n"
      "jge         40b                           \n"

      "49:         \n"
      "add         $0x3,%3                       \n"
      "jl          19f                           \n"

      // 1 pixel loop.
      LABELALIGN
      "10:         \n"
      "movd        (%0),%%xmm2                   \n"
      "lea         0x4(%0),%0                    \n"
      "punpcklbw   %%xmm1,%%xmm2                 \n"
      "punpcklwd   %%xmm1,%%xmm2                 \n"
      "paddd       %%xmm2,%%xmm0                 \n"
      "movdqu      (%2),%%xmm2                   \n"
      "lea         0x10(%2),%2                   \n"
      "paddd       %%xmm0,%%xmm2                 \n"
      "movdqu      %%xmm2,(%1)                   \n"
      "lea         0x10(%1),%1                   \n"
      "sub         $0x1,%3                       \n"
      "jge         10b                           \n"

      "19:         \n"
      : "+r"(row),              // %0
        "+r"(cumsum),           // %1
        "+r"(previous_cumsum),  // %2
        "+r"(width)             // %3
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_COMPUTECUMULATIVESUMROW_SSE2

#ifdef HAS_CUMULATIVESUMTOAVERAGEROW_SSE2
void CumulativeSumToAverageRow_SSE2(const int32_t* topleft,
                                    const int32_t* botleft,
                                    int width,
                                    int area,
                                    uint8_t* dst,
                                    int count) {
  asm volatile(
      "movd        %5,%%xmm5                     \n"
      "cvtdq2ps    %%xmm5,%%xmm5                 \n"
      "rcpss       %%xmm5,%%xmm4                 \n"
      "pshufd      $0x0,%%xmm4,%%xmm4            \n"
      "sub         $0x4,%3                       \n"
      "jl          49f                           \n"
      "cmpl        $0x80,%5                      \n"
      "ja          40f                           \n"

      "pshufd      $0x0,%%xmm5,%%xmm5            \n"
      "pcmpeqb     %%xmm6,%%xmm6                 \n"
      "psrld       $0x10,%%xmm6                  \n"
      "cvtdq2ps    %%xmm6,%%xmm6                 \n"
      "addps       %%xmm6,%%xmm5                 \n"
      "mulps       %%xmm4,%%xmm5                 \n"
      "cvtps2dq    %%xmm5,%%xmm5                 \n"
      "packssdw    %%xmm5,%%xmm5                 \n"

      // 4 pixel small loop.
      LABELALIGN
      "4:          \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "movdqu      0x20(%0),%%xmm2               \n"
      "movdqu      0x30(%0),%%xmm3               \n"
      "psubd       0x00(%0,%4,4),%%xmm0          \n"
      "psubd       0x10(%0,%4,4),%%xmm1          \n"
      "psubd       0x20(%0,%4,4),%%xmm2          \n"
      "psubd       0x30(%0,%4,4),%%xmm3          \n"
      "lea         0x40(%0),%0                   \n"
      "psubd       (%1),%%xmm0                   \n"
      "psubd       0x10(%1),%%xmm1               \n"
      "psubd       0x20(%1),%%xmm2               \n"
      "psubd       0x30(%1),%%xmm3               \n"
      "paddd       0x00(%1,%4,4),%%xmm0          \n"
      "paddd       0x10(%1,%4,4),%%xmm1          \n"
      "paddd       0x20(%1,%4,4),%%xmm2          \n"
      "paddd       0x30(%1,%4,4),%%xmm3          \n"
      "lea         0x40(%1),%1                   \n"
      "packssdw    %%xmm1,%%xmm0                 \n"
      "packssdw    %%xmm3,%%xmm2                 \n"
      "pmulhuw     %%xmm5,%%xmm0                 \n"
      "pmulhuw     %%xmm5,%%xmm2                 \n"
      "packuswb    %%xmm2,%%xmm0                 \n"
      "movdqu      %%xmm0,(%2)                   \n"
      "lea         0x10(%2),%2                   \n"
      "sub         $0x4,%3                       \n"
      "jge         4b                            \n"
      "jmp         49f                           \n"

      // 4 pixel loop
      LABELALIGN
      "40:         \n"
      "movdqu      (%0),%%xmm0                   \n"
      "movdqu      0x10(%0),%%xmm1               \n"
      "movdqu      0x20(%0),%%xmm2               \n"
      "movdqu      0x30(%0),%%xmm3               \n"
      "psubd       0x00(%0,%4,4),%%xmm0          \n"
      "psubd       0x10(%0,%4,4),%%xmm1          \n"
      "psubd       0x20(%0,%4,4),%%xmm2          \n"
      "psubd       0x30(%0,%4,4),%%xmm3          \n"
      "lea         0x40(%0),%0                   \n"
      "psubd       (%1),%%xmm0                   \n"
      "psubd       0x10(%1),%%xmm1               \n"
      "psubd       0x20(%1),%%xmm2               \n"
      "psubd       0x30(%1),%%xmm3               \n"
      "paddd       0x00(%1,%4,4),%%xmm0          \n"
      "paddd       0x10(%1,%4,4),%%xmm1          \n"
      "paddd       0x20(%1,%4,4),%%xmm2          \n"
      "paddd       0x30(%1,%4,4),%%xmm3          \n"
      "lea         0x40(%1),%1                   \n"
      "cvtdq2ps    %%xmm0,%%xmm0                 \n"
      "cvtdq2ps    %%xmm1,%%xmm1                 \n"
      "mulps       %%xmm4,%%xmm0                 \n"
      "mulps       %%xmm4,%%xmm1                 \n"
      "cvtdq2ps    %%xmm2,%%xmm2                 \n"
      "cvtdq2ps    %%xmm3,%%xmm3                 \n"
      "mulps       %%xmm4,%%xmm2                 \n"
      "mulps       %%xmm4,%%xmm3                 \n"
      "cvtps2dq    %%xmm0,%%xmm0                 \n"
      "cvtps2dq    %%xmm1,%%xmm1                 \n"
      "cvtps2dq    %%xmm2,%%xmm2                 \n"
      "cvtps2dq    %%xmm3,%%xmm3                 \n"
      "packssdw    %%xmm1,%%xmm0                 \n"
      "packssdw    %%xmm3,%%xmm2                 \n"
      "packuswb    %%xmm2,%%xmm0                 \n"
      "movdqu      %%xmm0,(%2)                   \n"
      "lea         0x10(%2),%2                   \n"
      "sub         $0x4,%3                       \n"
      "jge         40b                           \n"

      "49:         \n"
      "add         $0x3,%3                       \n"
      "jl          19f                           \n"

      // 1 pixel loop
      LABELALIGN
      "10:         \n"
      "movdqu      (%0),%%xmm0                   \n"
      "psubd       0x00(%0,%4,4),%%xmm0          \n"
      "lea         0x10(%0),%0                   \n"
      "psubd       (%1),%%xmm0                   \n"
      "paddd       0x00(%1,%4,4),%%xmm0          \n"
      "lea         0x10(%1),%1                   \n"
      "cvtdq2ps    %%xmm0,%%xmm0                 \n"
      "mulps       %%xmm4,%%xmm0                 \n"
      "cvtps2dq    %%xmm0,%%xmm0                 \n"
      "packssdw    %%xmm0,%%xmm0                 \n"
      "packuswb    %%xmm0,%%xmm0                 \n"
      "movd        %%xmm0,(%2)                   \n"
      "lea         0x4(%2),%2                    \n"
      "sub         $0x1,%3                       \n"
      "jge         10b                           \n"
      "19:         \n"
      : "+r"(topleft),            // %0
        "+r"(botleft),            // %1
        "+r"(dst),                // %2
        "+rm"(count)              // %3
      : "r"((ptrdiff_t)(width)),  // %4
        "rm"(area)                // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}
#endif  // HAS_CUMULATIVESUMTOAVERAGEROW_SSE2

#ifdef HAS_COMPUTECUMULATIVESUMROW_AVX2
static const uvec8 kCumSumTranspose = {0, 4, 8,  12, 1, 5, 9,  13,
                                       2, 6, 10, 14, 3, 7, 11, 15};
static const vec8 kCumSumMaddLo[2] = {
    {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0},
    {1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0}};
static const vec8 kCumSumMaddHi[2] = {
    {1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};

void ComputeCumulativeSumRow_AVX2(const uint8_t* row,
                                  int32_t* cumsum,
                                  const int32_t* previous_cumsum,
                                  int width) {
  asm volatile(
      "vpxor       %%xmm0,%%xmm0,%%xmm0       \n"
      "vbroadcasti128 %4,%%ymm5                  \n"
      "vmovdqu     %5,%%ymm6                  \n"
      "vmovdqu     %6,%%ymm7                  \n"
      "vpcmpeqd    %%ymm8,%%ymm8,%%ymm8       \n"
      "vpsrlw      $0xf,%%ymm8,%%ymm8         \n"
      "sub         $0x4,%3                    \n"
      "jl          49f                        \n"

      // 4 pixel loop.
      LABELALIGN
      "40:         \n"
      "vbroadcasti128 (%0),%%ymm2                \n"
      "lea         0x10(%0),%0                \n"
      "vpshufb     %%ymm5,%%ymm2,%%ymm2       \n"
      "vpmaddubsw  %%ymm6,%%ymm2,%%ymm3       \n"
      "vpmaddubsw  %%ymm7,%%ymm2,%%ymm4       \n"
      "vpmaddwd    %%ymm8,%%ymm3,%%ymm3       \n"
      "vpmaddwd    %%ymm8,%%ymm4,%%ymm4       \n"
      "vperm2i128  $0x11,%%ymm4,%%ymm4,%%ymm2 \n"
      "vpaddd      (%2),%%ymm3,%%ymm3         \n"
      "vpaddd      0x20(%2),%%ymm4,%%ymm4     \n"
      "lea         0x40(%2),%2                \n"
      "vpaddd      %%ymm0,%%ymm3,%%ymm3       \n"
      "vpaddd      %%ymm0,%%ymm4,%%ymm4       \n"
      "vpaddd      %%ymm2,%%ymm0,%%ymm0       \n"
      "vmovdqu     %%ymm3,(%1)                \n"
      "vmovdqu     %%ymm4,0x20(%1)            \n"
      "lea         0x40(%1),%1                \n"
      "sub         $0x4,%3                    \n"
      "jge         40b                        \n"

      "49:         \n"
      "add         $0x3,%3                    \n"
      "jl          19f                        \n"

      // 1 pixel loop.
      LABELALIGN
      "10:         \n"
      "vpmovzxbd   (%0),%%xmm2                \n"
      "lea         0x4(%0),%0                 \n"
      "vpaddd      %%xmm2,%%xmm0,%%xmm0       \n"
      "vpaddd      (%2),%%xmm0,%%xmm2         \n"
      "lea         0x10(%2),%2                \n"
      "vmovdqu     %%xmm2,(%1)                \n"
      "lea         0x10(%1),%1                \n"
      "sub         $0x1,%3                    \n"
      "jge         10b                        \n"

      "19:         \n"
      "vzeroupper  \n"
      : "+r"(row),              // %0
        "+r"(cumsum),           // %1
        "+r"(previous_cumsum),  // %2
        "+r"(width)             // %3
      : "m"(kCumSumTranspose),  // %4
        "m"(kCumSumMaddLo),     // %5
        "m"(kCumSumMaddHi)      // %6
      : "memory", "cc", "xmm0", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
        "xmm8");
}
#endif  // HAS_COMPUTECUMULATIVESUMROW_AVX2

#ifdef HAS_CUMULATIVESUMTOAVERAGEROW_AVX2
void CumulativeSumToAverageRow_AVX2(const int32_t* topleft,
                                    const int32_t* botleft,
                                    int width,
                                    int area,
                                    uint8_t* dst,
                                    int count) {
  asm volatile(
      "vpxor       %%xmm5,%%xmm5,%%xmm5       \n"
      "vcvtsi2ssl  %5,%%xmm5,%%xmm5           \n"
      "vrcpss      %%xmm5,%%xmm5,%%xmm4       \n"
      "vpshufd     $0x0,%%xmm4,%%xmm4         \n"
      "sub         $0x4,%3                    \n"
      "jl          19f                        \n"
      "vinserti128 $0x1,%%xmm4,%%ymm4,%%ymm4  \n"
      "cmpl        $0x80,%5                   \n"
      "ja          10f                        \n"

      "vpcmpeqb    %%xmm6,%%xmm6,%%xmm6       \n"
      "vpsrld      $0x10,%%xmm6,%%xmm6        \n"
      "vcvtdq2ps   %%xmm6,%%xmm6              \n"
      "vaddss      %%xmm6,%%xmm5,%%xmm6       \n"
      "vmulss      %%xmm4,%%xmm6,%%xmm6       \n"
      "vcvtps2dq   %%xmm6,%%xmm6              \n"
      "vpackssdw   %%xmm6,%%xmm6,%%xmm6       \n"
      "vpbroadcastw %%xmm6,%%ymm6              \n"

      // 4 pixel small loop.
      LABELALIGN
      "4:          \n"
      "vmovdqu     (%0),%%ymm0                \n"
      "vmovdqu     0x20(%0),%%ymm1            \n"
      "vpsubd      0x00(%0,%4,4),%%ymm0,%%ymm0\n"
      "vpsubd      0x20(%0,%4,4),%%ymm1,%%ymm1\n"
      "lea         0x40(%0),%0                \n"
      "vpsubd      (%1),%%ymm0,%%ymm0         \n"
      "vpsubd      0x20(%1),%%ymm1,%%ymm1     \n"
      "vpaddd      0x00(%1,%4,4),%%ymm0,%%ymm0\n"
      "vpaddd      0x20(%1,%4,4),%%ymm1,%%ymm1\n"
      "lea         0x40(%1),%1                \n"
      "vpackssdw   %%ymm1,%%ymm0,%%ymm0       \n"
      "vpmulhuw    %%ymm6,%%ymm0,%%ymm0       \n"
      "vextracti128 $0x1,%%ymm0,%%xmm1         \n"
      "vpackuswb   %%xmm1,%%xmm0,%%xmm0       \n"
      "vpshufd     $0xd8,%%xmm0,%%xmm0        \n"
      "vmovdqu     %%xmm0,(%2)                \n"
      "lea         0x10(%2),%2                \n"
      "sub         $0x4,%3                    \n"
      "jge         4b                         \n"
      "vzeroupper  \n"
      "jmp         19f                        \n"

      // 4 pixel loop.
      LABELALIGN
      "10:         \n"
      "vmovdqu     (%0),%%ymm0                \n"
      "vmovdqu     0x20(%0),%%ymm1            \n"
      "vpsubd      0x00(%0,%4,4),%%ymm0,%%ymm0\n"
      "vpsubd      0x20(%0,%4,4),%%ymm1,%%ymm1\n"
      "lea         0x40(%0),%0                \n"
      "vpsubd      (%1),%%ymm0,%%ymm0         \n"
      "vpsubd      0x20(%1),%%ymm1,%%ymm1     \n"
      "vpaddd      0x00(%1,%4,4),%%ymm0,%%ymm0\n"
      "vpaddd      0x20(%1,%4,4),%%ymm1,%%ymm1\n"
      "lea         0x40(%1),%1                \n"
      "vcvtdq2ps   %%ymm0,%%ymm0              \n"
      "vcvtdq2ps   %%ymm1,%%ymm1              \n"
      "vmulps      %%ymm4,%%ymm0,%%ymm0       \n"
      "vmulps      %%ymm4,%%ymm1,%%ymm1       \n"
      "vcvtps2dq   %%ymm0,%%ymm0              \n"
      "vcvtps2dq   %%ymm1,%%ymm1              \n"
      "vpackssdw   %%ymm1,%%ymm0,%%ymm0       \n"
      "vextracti128 $0x1,%%ymm0,%%xmm1         \n"
      "vpackuswb   %%xmm1,%%xmm0,%%xmm0       \n"
      "vpshufd     $0xd8,%%xmm0,%%xmm0        \n"
      "vmovdqu     %%xmm0,(%2)                \n"
      "lea         0x10(%2),%2                \n"
      "sub         $0x4,%3                    \n"
      "jge         10b                        \n"
      "vzeroupper  \n"

      "19:         \n"
      "add         $0x3,%3                    \n"
      "jl          99f                        \n"

      // 1 pixel loop.
      LABELALIGN
      "20:         \n"
      "vmovdqu     (%0),%%xmm0                \n"
      "vpsubd      0x00(%0,%4,4),%%xmm0,%%xmm0\n"
      "lea         0x10(%0),%0                \n"
      "vmovdqu     0x00(%1,%4,4),%%xmm1       \n"
      "vpsubd      (%1),%%xmm1,%%xmm1         \n"
      "lea         0x10(%1),%1                \n"
      "vpaddd      %%xmm1,%%xmm0,%%xmm0       \n"
      "vcvtdq2ps   %%xmm0,%%xmm0              \n"
      "vmulps      %%xmm4,%%xmm0,%%xmm0       \n"
      "vcvtps2dq   %%xmm0,%%xmm0              \n"
      "vpackssdw   %%xmm0,%%xmm0,%%xmm0       \n"
      "vpackuswb   %%xmm0,%%xmm0,%%xmm0       \n"
      "vmovd       %%xmm0,(%2)                \n"
      "lea         0x4(%2),%2                 \n"
      "sub         $0x1,%3                    \n"
      "jge         20b                        \n"
      "99:         \n"
      : "+r"(topleft),            // %0
        "+r"(botleft),            // %1
        "+r"(dst),                // %2
        "+r"(count)               // %3
      : "r"((ptrdiff_t)(width)),  // %4
        "r"(area)                 // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm4", "xmm5", "xmm6");
}
#endif  // HAS_CUMULATIVESUMTOAVERAGEROW_AVX2

#ifdef HAS_ARGBAFFINEROW_SSE2
// Copy ARGB pixels from source image with slope to a row of destination.
LIBYUV_API
void ARGBAffineRow_SSE2(const uint8_t* src_argb,
                        int src_argb_stride,
                        uint8_t* dst_argb,
                        const float* src_dudv,
                        int width) {
  ptrdiff_t src_argb_stride_temp = src_argb_stride;
  intptr_t temp;
  asm volatile(
      "movq        (%3),%%xmm2                   \n"
      "movq        0x08(%3),%%xmm7               \n"
      "shl         $0x10,%1                      \n"
      "add         $0x4,%1                       \n"
      "movd        %1,%%xmm5                     \n"
      "sub         $0x4,%4                       \n"
      "jl          49f                           \n"

      "pshufd      $0x44,%%xmm7,%%xmm7           \n"
      "pshufd      $0x0,%%xmm5,%%xmm5            \n"
      "movdqa      %%xmm2,%%xmm0                 \n"
      "addps       %%xmm7,%%xmm0                 \n"
      "movlhps     %%xmm0,%%xmm2                 \n"
      "movdqa      %%xmm7,%%xmm4                 \n"
      "addps       %%xmm4,%%xmm4                 \n"
      "movdqa      %%xmm2,%%xmm3                 \n"
      "addps       %%xmm4,%%xmm3                 \n"
      "addps       %%xmm4,%%xmm4                 \n"

      // 4 pixel loop
      LABELALIGN
      "40:         \n"
      "cvttps2dq   %%xmm2,%%xmm0                 \n"  // x,y float->int first 2
      "cvttps2dq   %%xmm3,%%xmm1                 \n"  // x,y float->int next 2
      "packssdw    %%xmm1,%%xmm0                 \n"  // x, y as 8 shorts
      "pmaddwd     %%xmm5,%%xmm0                 \n"  // off = x*4 + y*stride
      "movd        %%xmm0,%k1                    \n"
      "pshufd      $0x39,%%xmm0,%%xmm0           \n"
      "movd        %%xmm0,%k5                    \n"
      "pshufd      $0x39,%%xmm0,%%xmm0           \n"
      "movd        0x00(%0,%1,1),%%xmm1          \n"
      "movd        0x00(%0,%5,1),%%xmm6          \n"
      "punpckldq   %%xmm6,%%xmm1                 \n"
      "addps       %%xmm4,%%xmm2                 \n"
      "movq        %%xmm1,(%2)                   \n"
      "movd        %%xmm0,%k1                    \n"
      "pshufd      $0x39,%%xmm0,%%xmm0           \n"
      "movd        %%xmm0,%k5                    \n"
      "movd        0x00(%0,%1,1),%%xmm0          \n"
      "movd        0x00(%0,%5,1),%%xmm6          \n"
      "punpckldq   %%xmm6,%%xmm0                 \n"
      "addps       %%xmm4,%%xmm3                 \n"
      "movq        %%xmm0,0x08(%2)               \n"
      "lea         0x10(%2),%2                   \n"
      "sub         $0x4,%4                       \n"
      "jge         40b                           \n"

      "49:         \n"
      "add         $0x3,%4                       \n"
      "jl          19f                           \n"

      // 1 pixel loop
      LABELALIGN
      "10:         \n"
      "cvttps2dq   %%xmm2,%%xmm0                 \n"
      "packssdw    %%xmm0,%%xmm0                 \n"
      "pmaddwd     %%xmm5,%%xmm0                 \n"
      "addps       %%xmm7,%%xmm2                 \n"
      "movd        %%xmm0,%k1                    \n"
      "movd        0x00(%0,%1,1),%%xmm0          \n"
      "movd        %%xmm0,(%2)                   \n"
      "lea         0x04(%2),%2                   \n"
      "sub         $0x1,%4                       \n"
      "jge         10b                           \n"
      "19:         \n"
      : "+r"(src_argb),              // %0
        "+r"(src_argb_stride_temp),  // %1
        "+r"(dst_argb),              // %2
        "+r"(src_dudv),              // %3
        "+rm"(width),                // %4
        "=&r"(temp)                  // %5
      :
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_ARGBAFFINEROW_SSE2

#ifdef HAS_INTERPOLATEROW_AVX2
// Bilinear filter 32x2 -> 32x1
void InterpolateRow_AVX2(uint8_t* dst_ptr,
                         const uint8_t* src_ptr,
                         ptrdiff_t src_stride,
                         int width,
                         int source_y_fraction) {
  asm volatile(
      "sub         %1,%0                         \n"
      "cmp         $0x0,%3                       \n"
      "je          100f                          \n"
      "cmp         $0x80,%3                      \n"
      "je          50f                           \n"

      "vmovd       %3,%%xmm0                     \n"
      "neg         %3                            \n"
      "add         $0x100,%3                     \n"
      "vmovd       %3,%%xmm5                     \n"
      "vpunpcklbw  %%xmm0,%%xmm5,%%xmm5          \n"
      "vpunpcklwd  %%xmm5,%%xmm5,%%xmm5          \n"
      "vbroadcastss %%xmm5,%%ymm5                \n"
      "vpcmpeqb    %%ymm4,%%ymm4,%%ymm4          \n"
      "vpabsb      %%ymm4,%%ymm4                 \n"
      "vpsllw      $7,%%ymm4,%%ymm4              \n"

      // General purpose row blend.
      LABELALIGN
      "1:          \n"
      "vmovdqu     (%1),%%ymm0                   \n"
      "vmovdqu     0x00(%1,%4,1),%%ymm2          \n"
      "vpunpckhbw  %%ymm2,%%ymm0,%%ymm1          \n"
      "vpunpcklbw  %%ymm2,%%ymm0,%%ymm0          \n"
      "vpsubb      %%ymm4,%%ymm1,%%ymm1          \n"
      "vpsubb      %%ymm4,%%ymm0,%%ymm0          \n"
      "vpmaddubsw  %%ymm1,%%ymm5,%%ymm1          \n"
      "vpmaddubsw  %%ymm0,%%ymm5,%%ymm0          \n"
      "vpaddw      %%ymm4,%%ymm1,%%ymm1          \n"
      "vpaddw      %%ymm4,%%ymm0,%%ymm0          \n"
      "vpsrlw      $0x8,%%ymm1,%%ymm1            \n"
      "vpsrlw      $0x8,%%ymm0,%%ymm0            \n"
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"
      "vmovdqu     %%ymm0,0x00(%1,%0,1)          \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "jmp         99f                           \n"

      // Blend 50 / 50.
      LABELALIGN
      "50:         \n"
      "vmovdqu     (%1),%%ymm0                   \n"
      "vpavgb      0x00(%1,%4,1),%%ymm0,%%ymm0   \n"
      "vmovdqu     %%ymm0,0x00(%1,%0,1)          \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jg          50b                           \n"
      "jmp         99f                           \n"

      // Blend 100 / 0 - Copy row unchanged.
      LABELALIGN
      "100:        \n"
      "vmovdqu     (%1),%%ymm0                   \n"
      "vmovdqu     %%ymm0,0x00(%1,%0,1)          \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x20,%2                      \n"
      "jg          100b                          \n"

      "99:         \n"
      "vzeroupper  \n"
      : "+r"(dst_ptr),           // %0
        "+r"(src_ptr),           // %1
        "+r"(width),             // %2
        "+r"(source_y_fraction)  // %3
      : "r"(src_stride)          // %4
      : "memory", "cc", "eax", "xmm0", "xmm1", "xmm2", "xmm4", "xmm5");
}
#endif  // HAS_INTERPOLATEROW_AVX2

#ifdef HAS_INTERPOLATEROW_AVX512BW
// Bilinear filter 64x2 -> 64x1 with masked tail for any width.
void InterpolateRow_AVX512BW(uint8_t* dst_ptr,
                             const uint8_t* src_ptr,
                             ptrdiff_t src_stride,
                             int width,
                             int source_y_fraction) {
  asm volatile(
      "sub         %1,%0                         \n"
      "cmp         $0x0,%3                       \n"
      "je          100f                          \n"
      "cmp         $0x80,%3                      \n"
      "je          50f                           \n"

      "imul        $0xff,%3,%3                   \n"
      "add         $0x100,%3                     \n"
      "vpbroadcastw %3,%%zmm5                    \n"
      "vpternlogd  $0xff,%%zmm4,%%zmm4,%%zmm4    \n"
      "vpabsb      %%zmm4,%%zmm4                 \n"
      "vpsllw      $7,%%zmm4,%%zmm4              \n"

      "sub         $0x40,%2                      \n"
      "jl          2f                            \n"

      // General purpose row blend.
      LABELALIGN
      "1:          \n"
      "vmovdqu64   (%1),%%zmm0                   \n"
      "vmovdqu64   0x00(%1,%4,1),%%zmm2          \n"
      "vpunpckhbw  %%zmm2,%%zmm0,%%zmm1          \n"
      "vpunpcklbw  %%zmm2,%%zmm0,%%zmm0          \n"
      "vpsubb      %%zmm4,%%zmm1,%%zmm1          \n"
      "vpsubb      %%zmm4,%%zmm0,%%zmm0          \n"
      "vpmaddubsw  %%zmm1,%%zmm5,%%zmm1          \n"
      "vpmaddubsw  %%zmm0,%%zmm5,%%zmm0          \n"
      "vpaddw      %%zmm4,%%zmm1,%%zmm1          \n"
      "vpaddw      %%zmm4,%%zmm0,%%zmm0          \n"
      "vpsrlw      $0x8,%%zmm1,%%zmm1            \n"
      "vpsrlw      $0x8,%%zmm0,%%zmm0            \n"
      "vpackuswb   %%zmm1,%%zmm0,%%zmm0          \n"
      "vmovdqu64   %%zmm0,0x00(%1,%0,1)          \n"
      "lea         0x40(%1),%1                   \n"
      "sub         $0x40,%2                      \n"
      "jge         1b                            \n"

      "2:          \n"
      "add         $0x40,%2                      \n"
      "jle         99f                           \n"
      "mov         $-1,%q3                       \n"
      "bzhi        %q2,%q3,%q3                   \n"
      "kmovq       %q3,%%k1                      \n"
      "vmovdqu8    (%1),%%zmm0%{%%k1%}%{z%}      \n"
      "vmovdqu8    0x00(%1,%4,1),%%zmm2%{%%k1%}%{z%}\n"
      "vpunpckhbw  %%zmm2,%%zmm0,%%zmm1          \n"
      "vpunpcklbw  %%zmm2,%%zmm0,%%zmm0          \n"
      "vpsubb      %%zmm4,%%zmm1,%%zmm1          \n"
      "vpsubb      %%zmm4,%%zmm0,%%zmm0          \n"
      "vpmaddubsw  %%zmm1,%%zmm5,%%zmm1          \n"
      "vpmaddubsw  %%zmm0,%%zmm5,%%zmm0          \n"
      "vpaddw      %%zmm4,%%zmm1,%%zmm1          \n"
      "vpaddw      %%zmm4,%%zmm0,%%zmm0          \n"
      "vpsrlw      $0x8,%%zmm1,%%zmm1            \n"
      "vpsrlw      $0x8,%%zmm0,%%zmm0            \n"
      "vpackuswb   %%zmm1,%%zmm0,%%zmm0          \n"
      "vmovdqu8    %%zmm0,0x00(%1,%0,1)%{%%k1%}  \n"
      "jmp         99f                           \n"

      // Blend 50 / 50.
      LABELALIGN
      "50:         \n"
      "sub         $0x40,%2                      \n"
      "jl          52f                           \n"
      LABELALIGN
      "51:         \n"
      "vmovdqu64   (%1),%%zmm0                   \n"
      "vpavgb      0x00(%1,%4,1),%%zmm0,%%zmm0   \n"
      "vmovdqu64   %%zmm0,0x00(%1,%0,1)          \n"
      "lea         0x40(%1),%1                   \n"
      "sub         $0x40,%2                      \n"
      "jge         51b                           \n"

      "52:         \n"
      "add         $0x40,%2                      \n"
      "jle         99f                           \n"
      "mov         $-1,%q3                       \n"
      "bzhi        %q2,%q3,%q3                   \n"
      "kmovq       %q3,%%k1                      \n"
      "vmovdqu8    (%1),%%zmm0%{%%k1%}%{z%}      \n"
      "vmovdqu8    0x00(%1,%4,1),%%zmm2%{%%k1%}%{z%}\n"
      "vpavgb      %%zmm2,%%zmm0,%%zmm0          \n"
      "vmovdqu8    %%zmm0,0x00(%1,%0,1)%{%%k1%}  \n"
      "jmp         99f                           \n"

      // Blend 100 / 0 - Copy row unchanged.
      LABELALIGN
      "100:        \n"
      "sub         $0x40,%2                      \n"
      "jl          102f                          \n"
      LABELALIGN
      "101:        \n"
      "vmovdqu64   (%1),%%zmm0                   \n"
      "vmovdqu64   %%zmm0,0x00(%1,%0,1)          \n"
      "lea         0x40(%1),%1                   \n"
      "sub         $0x40,%2                      \n"
      "jge         101b                          \n"

      "102:        \n"
      "add         $0x40,%2                      \n"
      "jle         99f                           \n"
      "mov         $-1,%q3                       \n"
      "bzhi        %q2,%q3,%q3                   \n"
      "kmovq       %q3,%%k1                      \n"
      "vmovdqu8    (%1),%%zmm0%{%%k1%}%{z%}      \n"
      "vmovdqu8    %%zmm0,0x00(%1,%0,1)%{%%k1%}  \n"

      "99:         \n"
      "vzeroupper  \n"
      : "+r"(dst_ptr),           // %0
        "+r"(src_ptr),           // %1
        "+r"(width),             // %2
        "+r"(source_y_fraction)  // %3
      : "r"(src_stride)          // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm4", "xmm5", "k1");
}
#endif  // HAS_INTERPOLATEROW_AVX512BW

#ifdef HAS_INTERPOLATEROW_16_AVX2
// Bilinear filter 16x2 -> 16x1
void InterpolateRow_16_AVX2(uint16_t* dst_ptr,
                            const uint16_t* src_ptr,
                            ptrdiff_t src_stride,
                            int width,
                            int source_y_fraction) {
  asm volatile(
      "sub         %1,%0                         \n"
      "cmp         $0x0,%3                       \n"
      "je          100f                          \n"
      "cmp         $0x80,%3                      \n"
      "je          50f                           \n"

      "vmovd       %3,%%xmm0                     \n"
      "neg         %3                            \n"
      "add         $0x100,%3                     \n"
      "vmovd       %3,%%xmm5                     \n"
      "vpunpcklwd  %%xmm0,%%xmm5,%%xmm5          \n"
      "vpbroadcastd %%xmm5,%%ymm5                \n"
      "mov         $0x80008000,%%eax             \n"  // 0x80008000 used to bias
                                                      // unsigned words to
                                                      // signed range for
                                                      // vpmaddwd.
      "vmovd       %%eax,%%xmm4                  \n"
      "vbroadcastss %%xmm4,%%ymm4                \n"
      "mov         $8388736,%%eax                \n"  // 32768 * 256 + 128
                                                      // rounding constant.
      "vmovd       %%eax,%%xmm3                  \n"
      "vbroadcastss %%xmm3,%%ymm3                \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%1),%%ymm0                   \n"
      "vmovdqu     (%1,%4,2),%%ymm1              \n"
      "vpunpckhwd  %%ymm1,%%ymm0,%%ymm2          \n"
      "vpunpcklwd  %%ymm1,%%ymm0,%%ymm0          \n"
      "vpsubw      %%ymm4,%%ymm2,%%ymm2          \n"
      "vpsubw      %%ymm4,%%ymm0,%%ymm0          \n"
      "vpmaddwd    %%ymm5,%%ymm2,%%ymm2          \n"
      "vpmaddwd    %%ymm5,%%ymm0,%%ymm0          \n"
      "vpaddd      %%ymm3,%%ymm2,%%ymm2          \n"
      "vpaddd      %%ymm3,%%ymm0,%%ymm0          \n"
      "vpsrad      $0x8,%%ymm2,%%ymm2            \n"
      "vpsrad      $0x8,%%ymm0,%%ymm0            \n"
      "vpackusdw   %%ymm2,%%ymm0,%%ymm0          \n"
      "vmovdqu     %%ymm0,0x00(%1,%0,1)          \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "jmp         99f                           \n"

      "50:         \n" LABELALIGN
      "2:          \n"
      "vmovdqu     (%1),%%ymm0                   \n"
      "vpavgw      (%1,%4,2),%%ymm0,%%ymm0       \n"
      "vmovdqu     %%ymm0,0x00(%1,%0,1)          \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          2b                            \n"
      "jmp         99f                           \n"

      "100:        \n" LABELALIGN
      "3:          \n"
      "vmovdqu     (%1),%%ymm0                   \n"
      "vmovdqu     %%ymm0,0x00(%1,%0,1)          \n"
      "lea         0x20(%1),%1                   \n"
      "sub         $0x10,%2                      \n"
      "jg          3b                            \n"

      "99:         \n"
      "vzeroupper  \n"
      : "+r"(dst_ptr),           // %0
        "+r"(src_ptr),           // %1
        "+r"(width),             // %2
        "+r"(source_y_fraction)  // %3
      : "r"(src_stride)          // %4
      : "memory", "cc", "eax", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_INTERPOLATEROW_16_AVX2

#ifdef HAS_ARGBSHUFFLEROW_SSSE3
// For BGRAToARGB, ABGRToARGB, RGBAToARGB, and ARGBToRGBA.
void ARGBShuffleRow_SSSE3(const uint8_t* src_argb,
                          uint8_t* dst_argb,
                          const uint8_t* shuffler,
                          int width) {
  asm volatile("movdqu      (%3),%%xmm5                   \n"

               LABELALIGN
               "1:          \n"
               "movdqu      (%0),%%xmm0                   \n"
               "movdqu      0x10(%0),%%xmm1               \n"
               "lea         0x20(%0),%0                   \n"
               "pshufb      %%xmm5,%%xmm0                 \n"
               "pshufb      %%xmm5,%%xmm1                 \n"
               "movdqu      %%xmm0,(%1)                   \n"
               "movdqu      %%xmm1,0x10(%1)               \n"
               "lea         0x20(%1),%1                   \n"
               "sub         $0x8,%2                       \n"
               "jg          1b                            \n"
               : "+r"(src_argb),  // %0
                 "+r"(dst_argb),  // %1
                 "+r"(width)      // %2
               : "r"(shuffler)    // %3
               : "memory", "cc", "xmm0", "xmm1", "xmm5");
}
#endif  // HAS_ARGBSHUFFLEROW_SSSE3

#ifdef HAS_ARGBSHUFFLEROW_AVX2
// For BGRAToARGB, ABGRToARGB, RGBAToARGB, and ARGBToRGBA.
void ARGBShuffleRow_AVX2(const uint8_t* src_argb,
                         uint8_t* dst_argb,
                         const uint8_t* shuffler,
                         int width) {
  asm volatile("vbroadcasti128 (%3),%%ymm5                \n"

               LABELALIGN
               "1:          \n"
               "vmovdqu     (%0),%%ymm0                   \n"
               "vmovdqu     0x20(%0),%%ymm1               \n"
               "lea         0x40(%0),%0                   \n"
               "vpshufb     %%ymm5,%%ymm0,%%ymm0          \n"
               "vpshufb     %%ymm5,%%ymm1,%%ymm1          \n"
               "vmovdqu     %%ymm0,(%1)                   \n"
               "vmovdqu     %%ymm1,0x20(%1)               \n"
               "lea         0x40(%1),%1                   \n"
               "sub         $0x10,%2                      \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : "+r"(src_argb),  // %0
                 "+r"(dst_argb),  // %1
                 "+r"(width)      // %2
               : "r"(shuffler)    // %3
               : "memory", "cc", "xmm0", "xmm1", "xmm5");
}
#endif  // HAS_ARGBSHUFFLEROW_AVX2

#ifdef HAS_ARGBSHUFFLEROW_AVX512BW
// For BGRAToARGB, ABGRToARGB, RGBAToARGB, and ARGBToRGBA.
void ARGBShuffleRow_AVX512BW(const uint8_t* src_argb,
                             uint8_t* dst_argb,
                             const uint8_t* shuffler,
                             int width) {
  asm volatile("vbroadcasti32x4 (%3),%%zmm5               \n"

               LABELALIGN
               "1:          \n"
               "vmovdqu8    (%0),%%zmm0                   \n"
               "vmovdqu8    0x40(%0),%%zmm1               \n"
               "lea         0x80(%0),%0                   \n"
               "vpshufb     %%zmm5,%%zmm0,%%zmm0          \n"
               "vpshufb     %%zmm5,%%zmm1,%%zmm1          \n"
               "vmovdqu8    %%zmm0,(%1)                   \n"
               "vmovdqu8    %%zmm1,0x40(%1)               \n"
               "lea         0x80(%1),%1                   \n"
               "sub         $0x20,%2                      \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : "+r"(src_argb),  // %0
                 "+r"(dst_argb),  // %1
                 "+r"(width)      // %2
               : "r"(shuffler)    // %3
               : "memory", "cc", "xmm0", "xmm1", "xmm5");
}
#endif  // HAS_ARGBSHUFFLEROW_AVX512BW

#ifdef HAS_I422TOYUY2ROW_SSE2
void I422ToYUY2Row_SSE2(const uint8_t* src_y,
                        const uint8_t* src_u,
                        const uint8_t* src_v,
                        uint8_t* dst_yuy2,
                        int width) {
  asm volatile("sub         %1,%2                         \n"

               LABELALIGN
               "1:          \n"
               "movq        (%1),%%xmm2                   \n"
               "movq        0x00(%1,%2,1),%%xmm1          \n"
               "add         $0x8,%1                       \n"
               "punpcklbw   %%xmm1,%%xmm2                 \n"
               "movdqu      (%0),%%xmm0                   \n"
               "add         $0x10,%0                      \n"
               "movdqa      %%xmm0,%%xmm1                 \n"
               "punpcklbw   %%xmm2,%%xmm0                 \n"
               "punpckhbw   %%xmm2,%%xmm1                 \n"
               "movdqu      %%xmm0,(%3)                   \n"
               "movdqu      %%xmm1,0x10(%3)               \n"
               "lea         0x20(%3),%3                   \n"
               "sub         $0x10,%4                      \n"
               "jg          1b                            \n"
               : "+r"(src_y),     // %0
                 "+r"(src_u),     // %1
                 "+r"(src_v),     // %2
                 "+r"(dst_yuy2),  // %3
                 "+rm"(width)     // %4
               :
               : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_I422TOYUY2ROW_SSE2

#ifdef HAS_I422TOUYVYROW_SSE2
void I422ToUYVYRow_SSE2(const uint8_t* src_y,
                        const uint8_t* src_u,
                        const uint8_t* src_v,
                        uint8_t* dst_uyvy,
                        int width) {
  asm volatile("sub         %1,%2                         \n"

               LABELALIGN
               "1:          \n"
               "movq        (%1),%%xmm2                   \n"
               "movq        0x00(%1,%2,1),%%xmm1          \n"
               "add         $0x8,%1                       \n"
               "punpcklbw   %%xmm1,%%xmm2                 \n"
               "movdqu      (%0),%%xmm0                   \n"
               "movdqa      %%xmm2,%%xmm1                 \n"
               "add         $0x10,%0                      \n"
               "punpcklbw   %%xmm0,%%xmm1                 \n"
               "punpckhbw   %%xmm0,%%xmm2                 \n"
               "movdqu      %%xmm1,(%3)                   \n"
               "movdqu      %%xmm2,0x10(%3)               \n"
               "lea         0x20(%3),%3                   \n"
               "sub         $0x10,%4                      \n"
               "jg          1b                            \n"
               : "+r"(src_y),     // %0
                 "+r"(src_u),     // %1
                 "+r"(src_v),     // %2
                 "+r"(dst_uyvy),  // %3
                 "+rm"(width)     // %4
               :
               : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_I422TOUYVYROW_SSE2

#ifdef HAS_I422TOYUY2ROW_AVX2
void I422ToYUY2Row_AVX2(const uint8_t* src_y,
                        const uint8_t* src_u,
                        const uint8_t* src_v,
                        uint8_t* dst_yuy2,
                        int width) {
  asm volatile("sub         %1,%2                         \n"

               LABELALIGN
               "1:          \n"
               "vpmovzxbw   (%1),%%ymm1                   \n"
               "vpmovzxbw   0x00(%1,%2,1),%%ymm2          \n"
               "add         $0x10,%1                      \n"
               "vpsllw      $0x8,%%ymm2,%%ymm2            \n"
               "vpor        %%ymm1,%%ymm2,%%ymm2          \n"
               "vmovdqu     (%0),%%ymm0                   \n"
               "add         $0x20,%0                      \n"
               "vpunpcklbw  %%ymm2,%%ymm0,%%ymm1          \n"
               "vpunpckhbw  %%ymm2,%%ymm0,%%ymm2          \n"
               "vextractf128 $0x0,%%ymm1,(%3)             \n"
               "vextractf128 $0x0,%%ymm2,0x10(%3)         \n"
               "vextractf128 $0x1,%%ymm1,0x20(%3)         \n"
               "vextractf128 $0x1,%%ymm2,0x30(%3)         \n"
               "lea         0x40(%3),%3                   \n"
               "sub         $0x20,%4                      \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : "+r"(src_y),     // %0
                 "+r"(src_u),     // %1
                 "+r"(src_v),     // %2
                 "+r"(dst_yuy2),  // %3
                 "+rm"(width)     // %4
               :
               : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_I422TOYUY2ROW_AVX2

#ifdef HAS_I422TOUYVYROW_AVX2
void I422ToUYVYRow_AVX2(const uint8_t* src_y,
                        const uint8_t* src_u,
                        const uint8_t* src_v,
                        uint8_t* dst_uyvy,
                        int width) {
  asm volatile("sub         %1,%2                         \n"

               LABELALIGN
               "1:          \n"
               "vpmovzxbw   (%1),%%ymm1                   \n"
               "vpmovzxbw   0x00(%1,%2,1),%%ymm2          \n"
               "add         $0x10,%1                      \n"
               "vpsllw      $0x8,%%ymm2,%%ymm2            \n"
               "vpor        %%ymm1,%%ymm2,%%ymm2          \n"
               "vmovdqu     (%0),%%ymm0                   \n"
               "add         $0x20,%0                      \n"
               "vpunpcklbw  %%ymm0,%%ymm2,%%ymm1          \n"
               "vpunpckhbw  %%ymm0,%%ymm2,%%ymm2          \n"
               "vextractf128 $0x0,%%ymm1,(%3)             \n"
               "vextractf128 $0x0,%%ymm2,0x10(%3)         \n"
               "vextractf128 $0x1,%%ymm1,0x20(%3)         \n"
               "vextractf128 $0x1,%%ymm2,0x30(%3)         \n"
               "lea         0x40(%3),%3                   \n"
               "sub         $0x20,%4                      \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : "+r"(src_y),     // %0
                 "+r"(src_u),     // %1
                 "+r"(src_v),     // %2
                 "+r"(dst_uyvy),  // %3
                 "+rm"(width)     // %4
               :
               : "memory", "cc", "xmm0", "xmm1", "xmm2");
}
#endif  // HAS_I422TOUYVYROW_AVX2

#ifdef HAS_ARGBPOLYNOMIALROW_SSE2
void ARGBPolynomialRow_SSE2(const uint8_t* src_argb,
                            uint8_t* dst_argb,
                            const float* poly,
                            int width) {
  asm volatile("pxor        %%xmm3,%%xmm3                 \n"

               // 2 pixel loop.
               LABELALIGN
               "1:          \n"
               "movq        (%0),%%xmm0                   \n"
               "lea         0x8(%0),%0                    \n"
               "punpcklbw   %%xmm3,%%xmm0                 \n"
               "movdqa      %%xmm0,%%xmm4                 \n"
               "punpcklwd   %%xmm3,%%xmm0                 \n"
               "punpckhwd   %%xmm3,%%xmm4                 \n"
               "cvtdq2ps    %%xmm0,%%xmm0                 \n"
               "cvtdq2ps    %%xmm4,%%xmm4                 \n"
               "movdqa      %%xmm0,%%xmm1                 \n"
               "movdqa      %%xmm4,%%xmm5                 \n"
               "mulps       0x10(%3),%%xmm0               \n"
               "mulps       0x10(%3),%%xmm4               \n"
               "addps       (%3),%%xmm0                   \n"
               "addps       (%3),%%xmm4                   \n"
               "movdqa      %%xmm1,%%xmm2                 \n"
               "movdqa      %%xmm5,%%xmm6                 \n"
               "mulps       %%xmm1,%%xmm2                 \n"
               "mulps       %%xmm5,%%xmm6                 \n"
               "mulps       %%xmm2,%%xmm1                 \n"
               "mulps       %%xmm6,%%xmm5                 \n"
               "mulps       0x20(%3),%%xmm2               \n"
               "mulps       0x20(%3),%%xmm6               \n"
               "mulps       0x30(%3),%%xmm1               \n"
               "mulps       0x30(%3),%%xmm5               \n"
               "addps       %%xmm2,%%xmm0                 \n"
               "addps       %%xmm6,%%xmm4                 \n"
               "addps       %%xmm1,%%xmm0                 \n"
               "addps       %%xmm5,%%xmm4                 \n"
               "cvttps2dq   %%xmm0,%%xmm0                 \n"
               "cvttps2dq   %%xmm4,%%xmm4                 \n"
               "packuswb    %%xmm4,%%xmm0                 \n"
               "packuswb    %%xmm0,%%xmm0                 \n"
               "movq        %%xmm0,(%1)                   \n"
               "lea         0x8(%1),%1                    \n"
               "sub         $0x2,%2                       \n"
               "jg          1b                            \n"
               : "+r"(src_argb),  // %0
                 "+r"(dst_argb),  // %1
                 "+r"(width)      // %2
               : "r"(poly)        // %3
               : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5",
                 "xmm6");
}
#endif  // HAS_ARGBPOLYNOMIALROW_SSE2

#ifdef HAS_ARGBPOLYNOMIALROW_AVX2
void ARGBPolynomialRow_AVX2(const uint8_t* src_argb,
                            uint8_t* dst_argb,
                            const float* poly,
                            int width) {
  asm volatile(
      "vbroadcasti128 (%3),%%ymm4                \n"
      "vbroadcasti128 0x10(%3),%%ymm5            \n"
      "vbroadcasti128 0x20(%3),%%ymm6            \n"
      "vbroadcasti128 0x30(%3),%%ymm7            \n"

      // 2 pixel loop.
      LABELALIGN
      "1:          \n"
      "vpmovzxbd   (%0),%%ymm0                   \n"  // 2 ARGB pixels
      "lea         0x8(%0),%0                    \n"
      "vcvtdq2ps   %%ymm0,%%ymm0                 \n"  // X 8 floats
      "vmulps      %%ymm0,%%ymm0,%%ymm2          \n"  // X * X
      "vmulps      %%ymm7,%%ymm0,%%ymm3          \n"  // C3 * X
      "vfmadd132ps %%ymm5,%%ymm4,%%ymm0          \n"  // result = C0 + C1 * X
      "vfmadd231ps %%ymm6,%%ymm2,%%ymm0          \n"  // result += C2 * X * X
      "vfmadd231ps %%ymm3,%%ymm2,%%ymm0          \n"  // result += C3 * X * X *
                                                      // X
      "vcvttps2dq  %%ymm0,%%ymm0                 \n"
      "vpackusdw   %%ymm0,%%ymm0,%%ymm0          \n"
      "vpermq      $0xd8,%%ymm0,%%ymm0           \n"
      "vpackuswb   %%xmm0,%%xmm0,%%xmm0          \n"
      "vmovq       %%xmm0,(%1)                   \n"
      "lea         0x8(%1),%1                    \n"
      "sub         $0x2,%2                       \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_argb),  // %0
        "+r"(dst_argb),  // %1
        "+r"(width)      // %2
      : "r"(poly)        // %3
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6",
        "xmm7");
}
#endif  // HAS_ARGBPOLYNOMIALROW_AVX2

#ifdef HAS_HALFFLOATROW_AVX2
static float kScaleBias = 1.9259299444e-34f;
void HalfFloatRow_AVX2(const uint16_t* src,
                       uint16_t* dst,
                       float scale,
                       int width) {
  scale *= kScaleBias;
  asm volatile(
      "vbroadcastss %3, %%ymm4                   \n"
      "vpxor       %%ymm5,%%ymm5,%%ymm5          \n"
      "sub         %0,%1                         \n"

      // 16 pixel loop.
      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm2                   \n"  // 16 shorts
      "add         $0x20,%0                      \n"
      "vpunpckhwd  %%ymm5,%%ymm2,%%ymm3          \n"  // mutates
      "vpunpcklwd  %%ymm5,%%ymm2,%%ymm2          \n"
      "vcvtdq2ps   %%ymm3,%%ymm3                 \n"
      "vcvtdq2ps   %%ymm2,%%ymm2                 \n"
      "vmulps      %%ymm3,%%ymm4,%%ymm3          \n"
      "vmulps      %%ymm2,%%ymm4,%%ymm2          \n"
      "vpsrld      $0xd,%%ymm3,%%ymm3            \n"
      "vpsrld      $0xd,%%ymm2,%%ymm2            \n"
      "vpackssdw   %%ymm3, %%ymm2, %%ymm2        \n"  // unmutates
      "vmovdqu     %%ymm2,-0x20(%0,%1,1)         \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"

      "vzeroupper  \n"
      : "+r"(src),   // %0
        "+r"(dst),   // %1
        "+r"(width)  // %2
#if defined(__x86_64__)
      : "x"(scale)  // %3
#else
      : "m"(scale)  // %3
#endif
      : "memory", "cc", "xmm2", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_HALFFLOATROW_AVX2

#ifdef HAS_HALFFLOATROW_F16C
void HalfFloatRow_F16C(const uint16_t* src,
                       uint16_t* dst,
                       float scale,
                       int width) {
  asm volatile(
      "vbroadcastss %3, %%ymm4                   \n"
      "sub         %0,%1                         \n"

      // 16 pixel loop.
      LABELALIGN
      "1:          \n"
      "vpmovzxwd   (%0),%%ymm2                   \n"  // 16 shorts -> 16 ints
      "vpmovzxwd   0x10(%0),%%ymm3               \n"
      "vcvtdq2ps   %%ymm2,%%ymm2                 \n"
      "vcvtdq2ps   %%ymm3,%%ymm3                 \n"
      "vmulps      %%ymm2,%%ymm4,%%ymm2          \n"
      "vmulps      %%ymm3,%%ymm4,%%ymm3          \n"
      "vcvtps2ph   $3, %%ymm2, %%xmm2            \n"
      "vcvtps2ph   $3, %%ymm3, %%xmm3            \n"
      "vmovdqu     %%xmm2,0x00(%0,%1,1)          \n"
      "vmovdqu     %%xmm3,0x10(%0,%1,1)          \n"
      "add         $0x20,%0                      \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src),   // %0
        "+r"(dst),   // %1
        "+r"(width)  // %2
#if defined(__x86_64__)
      : "x"(scale)  // %3
#else
      : "m"(scale)  // %3
#endif
      : "memory", "cc", "xmm2", "xmm3", "xmm4");
}
#endif  // HAS_HALFFLOATROW_F16C

#ifdef HAS_HALFFLOATROW_F16C
void HalfFloat1Row_F16C(const uint16_t* src, uint16_t* dst, float, int width) {
  asm volatile(
      "sub         %0,%1                         \n"
      // 16 pixel loop.
      LABELALIGN
      "1:          \n"
      "vpmovzxwd   (%0),%%ymm2                   \n"  // 16 shorts -> 16 ints
      "vpmovzxwd   0x10(%0),%%ymm3               \n"
      "vcvtdq2ps   %%ymm2,%%ymm2                 \n"
      "vcvtdq2ps   %%ymm3,%%ymm3                 \n"
      "vcvtps2ph   $3, %%ymm2, %%xmm2            \n"
      "vcvtps2ph   $3, %%ymm3, %%xmm3            \n"
      "vmovdqu     %%xmm2,0x00(%0,%1,1)          \n"
      "vmovdqu     %%xmm3,0x10(%0,%1,1)          \n"
      "add         $0x20,%0                      \n"
      "sub         $0x10,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src),   // %0
        "+r"(dst),   // %1
        "+r"(width)  // %2
      :
      : "memory", "cc", "xmm2", "xmm3");
}
#endif  // HAS_HALFFLOATROW_F16C

#ifdef HAS_ARGBCOLORTABLEROW_X86
// Tranform ARGB pixels with color table.
void ARGBColorTableRow_X86(uint8_t* dst_argb,
                           const uint8_t* table_argb,
                           int width) {
  uintptr_t pixel_temp;
  asm volatile(
      // 1 pixel loop.
      LABELALIGN
      "1:          \n"
      "movzb       (%0),%1                       \n"
      "lea         0x4(%0),%0                    \n"
      "movzb       0x00(%3,%1,4),%1              \n"
      "mov         %b1,-0x4(%0)                  \n"
      "movzb       -0x3(%0),%1                   \n"
      "movzb       0x01(%3,%1,4),%1              \n"
      "mov         %b1,-0x3(%0)                  \n"
      "movzb       -0x2(%0),%1                   \n"
      "movzb       0x02(%3,%1,4),%1              \n"
      "mov         %b1,-0x2(%0)                  \n"
      "movzb       -0x1(%0),%1                   \n"
      "movzb       0x03(%3,%1,4),%1              \n"
      "mov         %b1,-0x1(%0)                  \n"
      "dec         %2                            \n"
      "jg          1b                            \n"
      : "+r"(dst_argb),     // %0
        "=&d"(pixel_temp),  // %1
        "+r"(width)         // %2
      : "r"(table_argb)     // %3
      : "memory", "cc");
}
#endif  // HAS_ARGBCOLORTABLEROW_X86

#ifdef HAS_RGBCOLORTABLEROW_X86
// Tranform RGB pixels with color table.
void RGBColorTableRow_X86(uint8_t* dst_argb,
                          const uint8_t* table_argb,
                          int width) {
  uintptr_t pixel_temp;
  asm volatile(
      // 1 pixel loop.
      LABELALIGN
      "1:          \n"
      "movzb       (%0),%1                       \n"
      "lea         0x4(%0),%0                    \n"
      "movzb       0x00(%3,%1,4),%1              \n"
      "mov         %b1,-0x4(%0)                  \n"
      "movzb       -0x3(%0),%1                   \n"
      "movzb       0x01(%3,%1,4),%1              \n"
      "mov         %b1,-0x3(%0)                  \n"
      "movzb       -0x2(%0),%1                   \n"
      "movzb       0x02(%3,%1,4),%1              \n"
      "mov         %b1,-0x2(%0)                  \n"
      "dec         %2                            \n"
      "jg          1b                            \n"
      : "+r"(dst_argb),     // %0
        "=&d"(pixel_temp),  // %1
        "+r"(width)         // %2
      : "r"(table_argb)     // %3
      : "memory", "cc");
}
#endif  // HAS_RGBCOLORTABLEROW_X86

#ifdef HAS_ARGBLUMACOLORTABLEROW_SSSE3
// Tranform RGB pixels with luma table.
void ARGBLumaColorTableRow_SSSE3(const uint8_t* src_argb,
                                 uint8_t* dst_argb,
                                 int width,
                                 const uint8_t* luma,
                                 uint32_t lumacoeff) {
  uintptr_t pixel_temp;
  uintptr_t table_temp;
  asm volatile(
      "movd        %6,%%xmm3                     \n"
      "pshufd      $0x0,%%xmm3,%%xmm3            \n"
      "pcmpeqb     %%xmm4,%%xmm4                 \n"
      "psllw       $0x8,%%xmm4                   \n"
      "pxor        %%xmm5,%%xmm5                 \n"

      // 4 pixel loop.
      LABELALIGN
      "1:          \n"
      "movdqu      (%2),%%xmm0                   \n"
      "pmaddubsw   %%xmm3,%%xmm0                 \n"
      "phaddw      %%xmm0,%%xmm0                 \n"
      "pand        %%xmm4,%%xmm0                 \n"
      "punpcklwd   %%xmm5,%%xmm0                 \n"
      "movd        %%xmm0,%k1                    \n"  // 32 bit offset
      "add         %5,%1                         \n"
      "pshufd      $0x39,%%xmm0,%%xmm0           \n"

      "movzb       (%2),%0                       \n"
      "movzb       0x00(%1,%0,1),%0              \n"
      "mov         %b0,(%3)                      \n"
      "movzb       0x1(%2),%0                    \n"
      "movzb       0x00(%1,%0,1),%0              \n"
      "mov         %b0,0x1(%3)                   \n"
      "movzb       0x2(%2),%0                    \n"
      "movzb       0x00(%1,%0,1),%0              \n"
      "mov         %b0,0x2(%3)                   \n"
      "movzb       0x3(%2),%0                    \n"
      "mov         %b0,0x3(%3)                   \n"

      "movd        %%xmm0,%k1                    \n"  // 32 bit offset
      "add         %5,%1                         \n"
      "pshufd      $0x39,%%xmm0,%%xmm0           \n"

      "movzb       0x4(%2),%0                    \n"
      "movzb       0x00(%1,%0,1),%0              \n"
      "mov         %b0,0x4(%3)                   \n"
      "movzb       0x5(%2),%0                    \n"
      "movzb       0x00(%1,%0,1),%0              \n"
      "mov         %b0,0x5(%3)                   \n"
      "movzb       0x6(%2),%0                    \n"
      "movzb       0x00(%1,%0,1),%0              \n"
      "mov         %b0,0x6(%3)                   \n"
      "movzb       0x7(%2),%0                    \n"
      "mov         %b0,0x7(%3)                   \n"

      "movd        %%xmm0,%k1                    \n"  // 32 bit offset
      "add         %5,%1                         \n"
      "pshufd      $0x39,%%xmm0,%%xmm0           \n"

      "movzb       0x8(%2),%0                    \n"
      "movzb       0x00(%1,%0,1),%0              \n"
      "mov         %b0,0x8(%3)                   \n"
      "movzb       0x9(%2),%0                    \n"
      "movzb       0x00(%1,%0,1),%0              \n"
      "mov         %b0,0x9(%3)                   \n"
      "movzb       0xa(%2),%0                    \n"
      "movzb       0x00(%1,%0,1),%0              \n"
      "mov         %b0,0xa(%3)                   \n"
      "movzb       0xb(%2),%0                    \n"
      "mov         %b0,0xb(%3)                   \n"

      "movd        %%xmm0,%k1                    \n"  // 32 bit offset
      "add         %5,%1                         \n"

      "movzb       0xc(%2),%0                    \n"
      "movzb       0x00(%1,%0,1),%0              \n"
      "mov         %b0,0xc(%3)                   \n"
      "movzb       0xd(%2),%0                    \n"
      "movzb       0x00(%1,%0,1),%0              \n"
      "mov         %b0,0xd(%3)                   \n"
      "movzb       0xe(%2),%0                    \n"
      "movzb       0x00(%1,%0,1),%0              \n"
      "mov         %b0,0xe(%3)                   \n"
      "movzb       0xf(%2),%0                    \n"
      "mov         %b0,0xf(%3)                   \n"
      "lea         0x10(%2),%2                   \n"
      "lea         0x10(%3),%3                   \n"
      "sub         $0x4,%4                       \n"
      "jg          1b                            \n"
      : "=&d"(pixel_temp),  // %0
        "=&a"(table_temp),  // %1
        "+r"(src_argb),     // %2
        "+r"(dst_argb),     // %3
        "+rm"(width)        // %4
      : "r"(luma),          // %5
        "rm"(lumacoeff)     // %6
      : "memory", "cc", "xmm0", "xmm3", "xmm4", "xmm5");
}
#endif  // HAS_ARGBLUMACOLORTABLEROW_SSSE3

static const uvec8 kYUV24Shuffle[3] = {
    {8, 9, 0, 8, 9, 1, 10, 11, 2, 10, 11, 3, 12, 13, 4, 12},
    {9, 1, 10, 11, 2, 10, 11, 3, 12, 13, 4, 12, 13, 5, 14, 15},
    {2, 10, 11, 3, 12, 13, 4, 12, 13, 5, 14, 15, 6, 14, 15, 7}};

// Convert biplanar NV21 to packed YUV24
// NV21 has VU in memory for chroma.
// YUV24 is VUY in memory
void NV21ToYUV24Row_SSSE3(const uint8_t* src_y,
                          const uint8_t* src_vu,
                          uint8_t* dst_yuv24,
                          int width) {
  asm volatile(
      "sub         %0,%1                         \n"
      "movdqa      (%4),%%xmm4                   \n"  // 3 shuffler constants
      "movdqa      16(%4),%%xmm5                 \n"
      "movdqa      32(%4),%%xmm6                 \n"
      "1:          \n"
      "movdqu      (%0),%%xmm2                   \n"  // load 16 Y values
      "movdqu      (%0,%1),%%xmm3                \n"  // load 8 VU values
      "lea         16(%0),%0                     \n"
      "movdqa      %%xmm2,%%xmm0                 \n"
      "movdqa      %%xmm2,%%xmm1                 \n"
      "shufps      $0x44,%%xmm3,%%xmm0           \n"  // Y 0..7,  UV 0..3
      "shufps      $0x99,%%xmm3,%%xmm1           \n"  // Y 4..11, UV 2..5
      "shufps      $0xee,%%xmm3,%%xmm2           \n"  // Y 8..15, UV 4..7
      "pshufb      %%xmm4, %%xmm0                \n"  // weave into YUV24
      "pshufb      %%xmm5, %%xmm1                \n"
      "pshufb      %%xmm6, %%xmm2                \n"
      "movdqu      %%xmm0,(%2)                   \n"
      "movdqu      %%xmm1,16(%2)                 \n"
      "movdqu      %%xmm2,32(%2)                 \n"
      "lea         48(%2),%2                     \n"
      "sub         $16,%3                        \n"  // 16 pixels per loop
      "jg          1b                            \n"
      : "+r"(src_y),            // %0
        "+r"(src_vu),           // %1
        "+r"(dst_yuv24),        // %2
        "+r"(width)             // %3
      : "r"(&kYUV24Shuffle[0])  // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}

// Convert biplanar NV21 to packed YUV24
// NV21 has VU in memory for chroma.
// YUV24 is VUY in memory
void NV21ToYUV24Row_AVX2(const uint8_t* src_y,
                         const uint8_t* src_vu,
                         uint8_t* dst_yuv24,
                         int width) {
  asm volatile(
      "sub         %0,%1                         \n"
      "vbroadcasti128 (%4),%%ymm4                \n"  // 3 shuffler constants
      "vbroadcasti128 16(%4),%%ymm5              \n"
      "vbroadcasti128 32(%4),%%ymm6              \n"

      "1:          \n"
      "vmovdqu     (%0),%%ymm2                   \n"  // load 32 Y values
      "vmovdqu     (%0,%1),%%ymm3                \n"  // load 16 VU values
      "lea         32(%0),%0                     \n"
      "vshufps     $0x44,%%ymm3,%%ymm2,%%ymm0    \n"  // Y 0..7,  UV 0..3
      "vshufps     $0x99,%%ymm3,%%ymm2,%%ymm1    \n"  // Y 4..11, UV 2..5
      "vshufps     $0xee,%%ymm3,%%ymm2,%%ymm2    \n"  // Y 8..15, UV 4..7
      "vpshufb     %%ymm4,%%ymm0,%%ymm0          \n"  // weave into YUV24
      "vpshufb     %%ymm5,%%ymm1,%%ymm1          \n"
      "vpshufb     %%ymm6,%%ymm2,%%ymm2          \n"
      "vperm2i128  $0x20,%%ymm1,%%ymm0,%%ymm3    \n"
      "vperm2i128  $0x30,%%ymm0,%%ymm2,%%ymm0    \n"
      "vperm2i128  $0x31,%%ymm2,%%ymm1,%%ymm1    \n"
      "vmovdqu     %%ymm3,(%2)                   \n"
      "vmovdqu     %%ymm0,32(%2)                 \n"
      "vmovdqu     %%ymm1,64(%2)                 \n"
      "lea         96(%2),%2                     \n"
      "sub         $32,%3                        \n"  // 32 pixels per loop
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_y),            // %0
        "+r"(src_vu),           // %1
        "+r"(dst_yuv24),        // %2
        "+r"(width)             // %3
      : "r"(&kYUV24Shuffle[0])  // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6");
}

#ifdef HAS_NV21ToYUV24ROW_AVX512
// The following VMBI VEX256 code tests okay with the intelsde emulator.
static const lvec8 kYUV24Perm[3] = {
    {32, 33, 0,  32, 33, 1,  34, 35, 2,  34, 35, 3,  36, 37, 4,  36,
     37, 5,  38, 39, 6,  38, 39, 7,  40, 41, 8,  40, 41, 9,  42, 43},
    {10, 42, 43, 11, 44, 45, 12, 44, 45, 13, 46, 47, 14, 46, 47, 15,
     48, 49, 16, 48, 49, 17, 50, 51, 18, 50, 51, 19, 52, 53, 20, 52},
    {53, 21, 54, 55, 22, 54, 55, 23, 56, 57, 24, 56, 57, 25, 58, 59,
     26, 58, 59, 27, 60, 61, 28, 60, 61, 29, 62, 63, 30, 62, 63, 31}};

void NV21ToYUV24Row_AVX512(const uint8_t* src_y,
                           const uint8_t* src_vu,
                           uint8_t* dst_yuv24,
                           int width) {
  asm volatile(
      "sub         %0,%1                         \n"
      "vmovdqa32   (%4),%%ymm4                   \n"  // 3 shuffler constants
      "vmovdqa32   32(%4),%%ymm5                 \n"
      "vmovdqa32   64(%4),%%ymm16                \n" LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm2                   \n"  // load 32 Y values
      "vmovdqu     (%0,%1),%%ymm3                \n"  // load 16 VU values
      "lea         32(%0),%0                     \n"
      "vmovdqa     %%ymm2, %%ymm0                \n"
      "vmovdqa     %%ymm2, %%ymm1                \n"
      "vpermt2b    %%ymm3,%%ymm4,%%ymm0          \n"
      "vpermt2b    %%ymm3,%%ymm5,%%ymm1          \n"
      "vpermt2b    %%ymm3,%%ymm16,%%ymm2         \n"
      "vmovdqu     %%ymm0,(%2)                   \n"
      "vmovdqu     %%ymm1,32(%2)                 \n"
      "vmovdqu     %%ymm2,64(%2)                 \n"
      "lea         96(%2),%2                     \n"
      "sub         $32,%3                        \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_y),         // %0
        "+r"(src_vu),        // %1
        "+r"(dst_yuv24),     // %2
        "+r"(width)          // %3
      : "r"(&kYUV24Perm[0])  // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5",
        "xmm16");
}

#endif  // HAS_NV21ToYUV24ROW_AVX512

#if defined(HAS_SWAPUVROW_SSSE3) || defined(HAS_SWAPUVROW_AVX2)

// Shuffle table for reversing the bytes.
static const uvec8 kShuffleUVToVU = {1u, 0u, 3u,  2u,  5u,  4u,  7u,  6u,
                                     9u, 8u, 11u, 10u, 13u, 12u, 15u, 14u};
#endif

#ifdef HAS_SWAPUVROW_SSSE3
// Convert UV plane of NV12 to VU of NV21.
void SwapUVRow_SSSE3(const uint8_t* src_uv, uint8_t* dst_vu, int width) {
  asm volatile("movdqu      %3,%%xmm5                     \n"

               LABELALIGN
               "1:          \n"
               "movdqu      (%0),%%xmm0                   \n"
               "movdqu      0x10(%0),%%xmm1               \n"
               "lea         0x20(%0),%0                   \n"
               "pshufb      %%xmm5,%%xmm0                 \n"
               "pshufb      %%xmm5,%%xmm1                 \n"
               "movdqu      %%xmm0,(%1)                   \n"
               "movdqu      %%xmm1,0x10(%1)               \n"
               "lea         0x20(%1),%1                   \n"
               "sub         $0x10,%2                      \n"
               "jg          1b                            \n"
               : "+r"(src_uv),        // %0
                 "+r"(dst_vu),        // %1
                 "+r"(width)          // %2
               : "m"(kShuffleUVToVU)  // %3
               : "memory", "cc", "xmm0", "xmm1", "xmm5");
}
#endif  // HAS_SWAPUVROW_SSSE3

#ifdef HAS_SWAPUVROW_AVX2
void SwapUVRow_AVX2(const uint8_t* src_uv, uint8_t* dst_vu, int width) {
  asm volatile("vbroadcasti128 %3,%%ymm5                  \n"

               LABELALIGN
               "1:          \n"
               "vmovdqu     (%0),%%ymm0                   \n"
               "vmovdqu     0x20(%0),%%ymm1               \n"
               "lea         0x40(%0),%0                   \n"
               "vpshufb     %%ymm5,%%ymm0,%%ymm0          \n"
               "vpshufb     %%ymm5,%%ymm1,%%ymm1          \n"
               "vmovdqu     %%ymm0,(%1)                   \n"
               "vmovdqu     %%ymm1,0x20(%1)               \n"
               "lea         0x40(%1),%1                   \n"
               "sub         $0x20,%2                      \n"
               "jg          1b                            \n"
               "vzeroupper  \n"
               : "+r"(src_uv),        // %0
                 "+r"(dst_vu),        // %1
                 "+r"(width)          // %2
               : "m"(kShuffleUVToVU)  // %3
               : "memory", "cc", "xmm0", "xmm1", "xmm5");
}
#endif  // HAS_SWAPUVROW_AVX2

void HalfMergeUVRow_SSSE3(const uint8_t* src_u,
                          int src_stride_u,
                          const uint8_t* src_v,
                          int src_stride_v,
                          uint8_t* dst_uv,
                          int width) {
  asm volatile(
      "pcmpeqb     %%xmm4,%%xmm4                 \n"  // 0x0101
      "pabsb       %%xmm4,%%xmm4                 \n"
      "pxor        %%xmm5,%%xmm5                 \n"

      LABELALIGN
      "1:          \n"
      "movdqu      (%0),%%xmm0                   \n"  // load 16 U values
      "movdqu      (%1),%%xmm1                   \n"  // load 16 V values
      "movdqu      0(%0,%4,1),%%xmm2             \n"  // 16 from next row
      "movdqu      0(%1,%5,1),%%xmm3             \n"
      "lea         0x10(%0),%0                   \n"
      "pmaddubsw   %%xmm4,%%xmm0                 \n"  // half size
      "pmaddubsw   %%xmm4,%%xmm1                 \n"
      "pmaddubsw   %%xmm4,%%xmm2                 \n"
      "pmaddubsw   %%xmm4,%%xmm3                 \n"
      "lea         0x10(%1),%1                   \n"
      "paddw       %%xmm2,%%xmm0                 \n"
      "paddw       %%xmm3,%%xmm1                 \n"
      "psrlw       $0x1,%%xmm0                   \n"
      "psrlw       $0x1,%%xmm1                   \n"
      "pavgw       %%xmm5,%%xmm0                 \n"
      "pavgw       %%xmm5,%%xmm1                 \n"
      "packuswb    %%xmm0,%%xmm0                 \n"
      "packuswb    %%xmm1,%%xmm1                 \n"
      "punpcklbw   %%xmm1,%%xmm0                 \n"
      "movdqu      %%xmm0,(%2)                   \n"  // store 8 UV pixels
      "lea         0x10(%2),%2                   \n"
      "sub         $0x10,%3                      \n"  // 16 src pixels per loop
      "jg          1b                            \n"
      : "+r"(src_u),                     // %0
        "+r"(src_v),                     // %1
        "+r"(dst_uv),                    // %2
        "+r"(width)                      // %3
      : "r"((ptrdiff_t)(src_stride_u)),  // %4
        "r"((ptrdiff_t)(src_stride_v))   // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}

void HalfMergeUVRow_AVX2(const uint8_t* src_u,
                         int src_stride_u,
                         const uint8_t* src_v,
                         int src_stride_v,
                         uint8_t* dst_uv,
                         int width) {
  asm volatile(
      "vpcmpeqb    %%ymm4,%%ymm4,%%ymm4          \n"
      "vpabsb      %%ymm4,%%ymm4                 \n"
      "vpxor       %%ymm5,%%ymm5,%%ymm5          \n"

      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"  // load 32 U values
      "vmovdqu     (%1),%%ymm1                   \n"  // load 32 V values
      "vmovdqu     0(%0,%4,1),%%ymm2             \n"  // 32 from next row
      "vmovdqu     0(%1,%5,1),%%ymm3             \n"
      "lea         0x20(%0),%0                   \n"
      "vpmaddubsw  %%ymm4,%%ymm0,%%ymm0          \n"  // half size
      "vpmaddubsw  %%ymm4,%%ymm1,%%ymm1          \n"
      "vpmaddubsw  %%ymm4,%%ymm2,%%ymm2          \n"
      "vpmaddubsw  %%ymm4,%%ymm3,%%ymm3          \n"
      "lea         0x20(%1),%1                   \n"
      "vpaddw      %%ymm2,%%ymm0,%%ymm0          \n"
      "vpaddw      %%ymm3,%%ymm1,%%ymm1          \n"
      "vpsrlw      $0x1,%%ymm0,%%ymm0            \n"
      "vpsrlw      $0x1,%%ymm1,%%ymm1            \n"
      "vpavgw      %%ymm5,%%ymm0,%%ymm0          \n"
      "vpavgw      %%ymm5,%%ymm1,%%ymm1          \n"
      "vpackuswb   %%ymm0,%%ymm0,%%ymm0          \n"
      "vpackuswb   %%ymm1,%%ymm1,%%ymm1          \n"
      "vpunpcklbw  %%ymm1,%%ymm0,%%ymm0          \n"
      "vmovdqu     %%ymm0,(%2)                   \n"  // store 16 UV pixels
      "lea         0x20(%2),%2                   \n"
      "sub         $0x20,%3                      \n"  // 32 src pixels per loop
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_u),                     // %0
        "+r"(src_v),                     // %1
        "+r"(dst_uv),                    // %2
        "+r"(width)                      // %3
      : "r"((ptrdiff_t)(src_stride_u)),  // %4
        "r"((ptrdiff_t)(src_stride_v))   // %5
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}

void ClampFloatToZero_SSE2(const float* src_x, float* dst_y, int width) {
  asm volatile(
      "pxor        %%xmm1,%%xmm1                 \n"

      LABELALIGN
      "1:          \n"
      "movd        (%0),%%xmm0                   \n"  // load float
      "maxss       %%xmm1, %%xmm0                \n"  // clamp to zero
      "add         4, %0                         \n"
      "movd        %%xmm0, (%1)                  \n"  // store float
      "add         4, %1                         \n"
      "sub         $0x4,%2                       \n"  // 1 float per loop
      "jg          1b                            \n"
      : "+r"(src_x),  // %0
        "+r"(dst_y),  // %1
        "+r"(width)   // %2
      :
      : "memory", "cc", "xmm0", "xmm1");
}

#ifdef HAS_CONVERT16TO8ROW_AVX2
void Convert8To8Row_AVX2(const uint8_t* src_y,
                         uint8_t* dst_y,
                         int scale,
                         int bias,
                         int width) {
  asm volatile(
      "sub         %0,%1                         \n"
      "vmovd       %3,%%xmm2                     \n"
      "vmovd       %4,%%xmm3                     \n"
      "vpbroadcastw %%xmm2,%%ymm2                \n"
      "vpbroadcastb %%xmm3,%%ymm3                \n"
      "vpxor       %%ymm4,%%ymm4,%%ymm4          \n"
      "vpsllw      $8,%%ymm2,%%ymm2              \n"

      // 32 pixels per loop.
      LABELALIGN
      "1:          \n"
      "vmovdqu     (%0),%%ymm0                   \n"
      "vpunpckhbw  %%ymm4,%%ymm0,%%ymm1          \n"  // mutates
      "vpunpcklbw  %%ymm4,%%ymm0,%%ymm0          \n"
      "vpmulhuw    %%ymm2,%%ymm0,%%ymm0          \n"
      "vpmulhuw    %%ymm2,%%ymm1,%%ymm1          \n"
      "vpackuswb   %%ymm1,%%ymm0,%%ymm0          \n"  // unmutates
      "vpaddb      %%ymm3,%%ymm0,%%ymm0          \n"
      "vmovdqu     %%ymm0,(%0,%1)                \n"
      "add         $0x20,%0                      \n"
      "sub         $0x20,%2                      \n"
      "jg          1b                            \n"
      "vzeroupper  \n"
      : "+r"(src_y),  // %0
        "+r"(dst_y),  // %1
        "+r"(width)   // %2
      : "r"(scale),   // %3
        "r"(bias)     // %4
      : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4");
}
#endif  // HAS_CONVERT16TO8ROW_AVX2

#endif  // !defined(LIBYUV_DISABLE_X86) &&
        // (defined(__x86_64__) || defined(__i386__)) &&
        // !defined(LIBYUV_ENABLE_ROWWIN)

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
