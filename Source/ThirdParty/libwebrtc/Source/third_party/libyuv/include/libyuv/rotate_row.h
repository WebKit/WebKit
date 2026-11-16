/*
 *  Copyright 2013 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef INCLUDE_LIBYUV_ROTATE_ROW_H_
#define INCLUDE_LIBYUV_ROTATE_ROW_H_

#include <stddef.h>
#include <stdint.h>

#include "libyuv/cpu_support.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// The following are available for 32 bit x86, Visual C 64 bit, and for clang
// with LIBYUV_ENABLE_ROWWIN, which is also enabled for MemorySanitizer. These
// are intrinsics in rotate_win.cc.
#if !defined(LIBYUV_DISABLE_X86) &&                  \
    (defined(__i386__) || defined(_M_IX86) ||        \
     ((defined(__x86_64__) || defined(_M_X64)) &&    \
      ((defined(_MSC_VER) && !defined(__clang__)) || \
       defined(LIBYUV_ENABLE_ROWWIN))))
#define HAS_TRANSPOSEWX8_SSSE3
#define HAS_TRANSPOSEUVWX8_SSE2
#define HAS_TRANSPOSEWX16_16_AVX2
#define HAS_TRANSPOSEWX4_32_SSE2
#define HAS_TRANSPOSEWX8_32_AVX2
#define HAS_TRANSPOSEWX16_AVX2
#define HAS_TRANSPOSEUVWX16_AVX2
#if defined(__x86_64__) || defined(_M_X64)
#define HAS_TRANSPOSEWX16_AVX512BW
#define HAS_TRANSPOSEUVWX16_AVX512BW
#define HAS_TRANSPOSEWX32_16_AVX512BW
#define HAS_TRANSPOSEWX16_32_AVX512BW
#endif
#endif

// The following are available for 64 bit GCC and clang, as inline assembly in
// rotate_gcc.cc:
#if !defined(LIBYUV_DISABLE_X86) && defined(__x86_64__) && \
    !defined(LIBYUV_ENABLE_ROWWIN)
#define HAS_TRANSPOSEWX8_SSSE3
#define HAS_TRANSPOSEUVWX8_SSE2
#define HAS_TRANSPOSEWX16_16_AVX2
#define HAS_TRANSPOSEWX4_32_SSE2
#define HAS_TRANSPOSEWX8_32_AVX2
#define HAS_TRANSPOSEWX16_AVX2
#define HAS_TRANSPOSEUVWX16_AVX2
#endif

// The following are available for AVX512 clang x64 platforms:
#if !defined(LIBYUV_DISABLE_X86) && defined(__x86_64__) && \
    defined(CLANG_HAS_AVX512) && !defined(LIBYUV_ENABLE_ROWWIN)
#define HAS_TRANSPOSEWX16_AVX512BW
#define HAS_TRANSPOSEUVWX16_AVX512BW
#define HAS_TRANSPOSEWX32_16_AVX512BW
#define HAS_TRANSPOSEWX16_32_AVX512BW
#endif

#if !defined(LIBYUV_DISABLE_NEON) && \
    (defined(__ARM_NEON__) || defined(LIBYUV_NEON) || defined(__aarch64__))
#if defined(__aarch64__)
#define HAS_TRANSPOSEWX16_NEON
#else
#define HAS_TRANSPOSEWX8_NEON
#endif
#define HAS_TRANSPOSEUVWX8_NEON
#define HAS_TRANSPOSEWX8_16_NEON
#define HAS_TRANSPOSEWX4_32_NEON
#define HAS_TRANSPOSEWXH_32_NEON
#endif

#if !defined(LIBYUV_DISABLE_SVE) && defined(__aarch64__)
#define HAS_TRANSPOSEWX8_16_SVE2
#define HAS_TRANSPOSEWX4_32_SVE2
#endif

#if !defined(LIBYUV_DISABLE_SME) && defined(CLANG_HAS_SME) && \
    defined(__aarch64__)
#define HAS_TRANSPOSEWXH_SME
#define HAS_TRANSPOSEUVWXH_SME
#define HAS_TRANSPOSEWXH_16_SME
#define HAS_TRANSPOSEWXH_32_SME
#endif

#if !defined(LIBYUV_DISABLE_LSX) && defined(__loongarch_sx)
#define HAS_TRANSPOSEWX16_LSX
#define HAS_TRANSPOSEUVWX16_LSX
#define HAS_TRANSPOSEWX8_16_LSX
#define HAS_TRANSPOSEWX4_32_LSX
#endif

#if !defined(LIBYUV_DISABLE_LASX) && defined(__loongarch_asx)
#define HAS_TRANSPOSEWX16_16_LASX
#define HAS_TRANSPOSEWX4_32_LASX
#endif

#if !defined(LIBYUV_DISABLE_RVV) && defined(__riscv_vector)
#define HAS_TRANSPOSEWXH_16_RVV
#define HAS_TRANSPOSEWX4_32_RVV
#define HAS_TRANSPOSEWXH_32_RVV
#endif

#if !defined(LIBYUV_DISABLE_WASM) && defined(__wasm_simd128__)
#define HAS_TRANSPOSEWX8_WASMSIMD
#define HAS_TRANSPOSEUVWX8_WASMSIMD
#define HAS_TRANSPOSEWX16_WASMSIMD
#define HAS_TRANSPOSEUVWX16_WASMSIMD
#define HAS_TRANSPOSEWX8_16_WASMSIMD
#define HAS_TRANSPOSEWX4_32_WASMSIMD
#endif

void TransposeWxH_C(const uint8_t* src,
                    int src_stride,
                    uint8_t* dst,
                    int dst_stride,
                    int width,
                    int height);

void TransposeWx8_C(const uint8_t* src,
                    int src_stride,
                    uint8_t* dst,
                    int dst_stride,
                    int width);
void TransposeWx16_C(const uint8_t* src,
                     int src_stride,
                     uint8_t* dst,
                     int dst_stride,
                     int width);
void TransposeWx8_NEON(const uint8_t* src,
                       int src_stride,
                       uint8_t* dst,
                       int dst_stride,
                       int width);
void TransposeWx16_NEON(const uint8_t* src,
                        int src_stride,
                        uint8_t* dst,
                        int dst_stride,
                        int width);
void TransposeWxH_SME(const uint8_t* src,
                      int src_stride,
                      uint8_t* dst,
                      int dst_stride,
                      int width,
                      int height);
void TransposeWx8_SSSE3(const uint8_t* src,
                        int src_stride,
                        uint8_t* dst,
                        int dst_stride,
                        int width);
void TransposeWx16_AVX2(const uint8_t* src,
                        int src_stride,
                        uint8_t* dst,
                        int dst_stride,
                        int width);
void TransposeWx16_AVX512BW(const uint8_t* src,
                            int src_stride,
                            uint8_t* dst,
                            int dst_stride,
                            int width);
void TransposeWx16_LSX(const uint8_t* src,
                       int src_stride,
                       uint8_t* dst,
                       int dst_stride,
                       int width);
void TransposeWx8_WASMSIMD(const uint8_t* src,
                           int src_stride,
                           uint8_t* dst,
                           int dst_stride,
                           int width);
void TransposeWx16_WASMSIMD(const uint8_t* src,
                            int src_stride,
                            uint8_t* dst,
                            int dst_stride,
                            int width);

void TransposeWx8_Any_NEON(const uint8_t* src,
                           int src_stride,
                           uint8_t* dst,
                           int dst_stride,
                           int width);
void TransposeWx16_Any_NEON(const uint8_t* src,
                            int src_stride,
                            uint8_t* dst,
                            int dst_stride,
                            int width);
void TransposeWx8_Any_SSSE3(const uint8_t* src,
                            int src_stride,
                            uint8_t* dst,
                            int dst_stride,
                            int width);
void TransposeWx16_Any_AVX2(const uint8_t* src,
                            int src_stride,
                            uint8_t* dst,
                            int dst_stride,
                            int width);
void TransposeWx16_Any_LSX(const uint8_t* src,
                           int src_stride,
                           uint8_t* dst,
                           int dst_stride,
                           int width);
void TransposeWx8_Any_WASMSIMD(const uint8_t* src,
                               int src_stride,
                               uint8_t* dst,
                               int dst_stride,
                               int width);
void TransposeWx16_Any_WASMSIMD(const uint8_t* src,
                                int src_stride,
                                uint8_t* dst,
                                int dst_stride,
                                int width);

void TransposeUVWxH_C(const uint8_t* src,
                      int src_stride,
                      uint8_t* dst_a,
                      int dst_stride_a,
                      uint8_t* dst_b,
                      int dst_stride_b,
                      int width,
                      int height);

void TransposeUVWx8_C(const uint8_t* src,
                      int src_stride,
                      uint8_t* dst_a,
                      int dst_stride_a,
                      uint8_t* dst_b,
                      int dst_stride_b,
                      int width);
void TransposeUVWx16_C(const uint8_t* src,
                       int src_stride,
                       uint8_t* dst_a,
                       int dst_stride_a,
                       uint8_t* dst_b,
                       int dst_stride_b,
                       int width);
void TransposeUVWx8_SSE2(const uint8_t* src,
                         int src_stride,
                         uint8_t* dst_a,
                         int dst_stride_a,
                         uint8_t* dst_b,
                         int dst_stride_b,
                         int width);
void TransposeUVWx16_AVX2(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst_a,
                          int dst_stride_a,
                          uint8_t* dst_b,
                          int dst_stride_b,
                          int width);
void TransposeUVWx16_AVX512BW(const uint8_t* src,
                              int src_stride,
                              uint8_t* dst_a,
                              int dst_stride_a,
                              uint8_t* dst_b,
                              int dst_stride_b,
                              int width);
void TransposeUVWx8_NEON(const uint8_t* src,
                         int src_stride,
                         uint8_t* dst_a,
                         int dst_stride_a,
                         uint8_t* dst_b,
                         int dst_stride_b,
                         int width);
void TransposeUVWxH_SME(const uint8_t* src,
                        int src_stride,
                        uint8_t* dst_a,
                        int dst_stride_a,
                        uint8_t* dst_b,
                        int dst_stride_b,
                        int width,
                        int height);
void TransposeUVWx16_LSX(const uint8_t* src,
                         int src_stride,
                         uint8_t* dst_a,
                         int dst_stride_a,
                         uint8_t* dst_b,
                         int dst_stride_b,
                         int width);
void TransposeUVWx8_WASMSIMD(const uint8_t* src,
                             int src_stride,
                             uint8_t* dst_a,
                             int dst_stride_a,
                             uint8_t* dst_b,
                             int dst_stride_b,
                             int width);
void TransposeUVWx16_WASMSIMD(const uint8_t* src,
                              int src_stride,
                              uint8_t* dst_a,
                              int dst_stride_a,
                              uint8_t* dst_b,
                              int dst_stride_b,
                              int width);

void TransposeUVWx8_Any_SSE2(const uint8_t* src,
                             int src_stride,
                             uint8_t* dst_a,
                             int dst_stride_a,
                             uint8_t* dst_b,
                             int dst_stride_b,
                             int width);
void TransposeUVWx8_Any_NEON(const uint8_t* src,
                             int src_stride,
                             uint8_t* dst_a,
                             int dst_stride_a,
                             uint8_t* dst_b,
                             int dst_stride_b,
                             int width);
void TransposeUVWx16_Any_AVX2(const uint8_t* src,
                              int src_stride,
                              uint8_t* dst_a,
                              int dst_stride_a,
                              uint8_t* dst_b,
                              int dst_stride_b,
                              int width);
void TransposeUVWx16_Any_LSX(const uint8_t* src,
                             int src_stride,
                             uint8_t* dst_a,
                             int dst_stride_a,
                             uint8_t* dst_b,
                             int dst_stride_b,
                             int width);
void TransposeUVWx8_Any_WASMSIMD(const uint8_t* src,
                                 int src_stride,
                                 uint8_t* dst_a,
                                 int dst_stride_a,
                                 uint8_t* dst_b,
                                 int dst_stride_b,
                                 int width);
void TransposeUVWx16_Any_WASMSIMD(const uint8_t* src,
                                  int src_stride,
                                  uint8_t* dst_a,
                                  int dst_stride_a,
                                  uint8_t* dst_b,
                                  int dst_stride_b,
                                  int width);
void TransposeWxH_16_C(const uint16_t* src,
                       int src_stride,
                       uint16_t* dst,
                       int dst_stride,
                       int width,
                       int height);

void TransposeWx8_16_C(const uint16_t* src,
                       int src_stride,
                       uint16_t* dst,
                       int dst_stride,
                       int width);
void TransposeWx16_16_AVX2(const uint16_t* src,
                           int src_stride,
                           uint16_t* dst,
                           int dst_stride,
                           int width);
void TransposeWx32_16_AVX512BW(const uint16_t* src,
                               int src_stride,
                               uint16_t* dst,
                               int dst_stride,
                               int width);
void TransposeWx8_16_NEON(const uint16_t* src,
                          int src_stride,
                          uint16_t* dst,
                          int dst_stride,
                          int width);
void TransposeWx8_16_SVE2(const uint16_t* src,
                          int src_stride,
                          uint16_t* dst,
                          int dst_stride,
                          int width);
void TransposeWxH_16_SME(const uint16_t* src,
                         int src_stride,
                         uint16_t* dst,
                         int dst_stride,
                         int width,
                         int height);
void TransposeWxH_16_RVV(const uint16_t* src,
                         int src_stride,
                         uint16_t* dst,
                         int dst_stride,
                         int width,
                         int height);
void TransposeWx8_16_LSX(const uint16_t* src,
                         int src_stride,
                         uint16_t* dst,
                         int dst_stride,
                         int width);
void TransposeWx16_16_LASX(const uint16_t* src,
                           int src_stride,
                           uint16_t* dst,
                           int dst_stride,
                           int width);
void TransposeWx8_16_WASMSIMD(const uint16_t* src,
                              int src_stride,
                              uint16_t* dst,
                              int dst_stride,
                              int width);
void TransposeWx16_16_Any_AVX2(const uint16_t* src,
                               int src_stride,
                               uint16_t* dst,
                               int dst_stride,
                               int width);
void TransposeWx8_16_Any_NEON(const uint16_t* src,
                              int src_stride,
                              uint16_t* dst,
                              int dst_stride,
                              int width);
void TransposeWx8_16_Any_LSX(const uint16_t* src,
                             int src_stride,
                             uint16_t* dst,
                             int dst_stride,
                             int width);
void TransposeWx16_16_Any_LASX(const uint16_t* src,
                               int src_stride,
                               uint16_t* dst,
                               int dst_stride,
                               int width);
void TransposeWx8_16_Any_WASMSIMD(const uint16_t* src,
                                  int src_stride,
                                  uint16_t* dst,
                                  int dst_stride,
                                  int width);

void TransposeWxH_32_C(const uint8_t* src,
                       int src_stride,
                       uint8_t* dst,
                       int dst_stride,
                       int width,
                       int height);

void TransposeWxH_32_SME(const uint8_t* src,
                         int src_stride,
                         uint8_t* dst,
                         int dst_stride,
                         int width,
                         int height);

void TransposeWx4_32_NEON(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst,
                          int dst_stride,
                          int width);

void TransposeWxH_32_NEON(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst,
                          int dst_stride,
                          int width,
                          int height);

void TransposeWx4_32_SVE2(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst,
                          int dst_stride,
                          int width);

void TransposeWx4_32_SSE2(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst,
                          int dst_stride,
                          int width);

void TransposeWx8_32_AVX2(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst,
                          int dst_stride,
                          int width);

void TransposeWx16_32_AVX512BW(const uint8_t* src,
                               int src_stride,
                               uint8_t* dst,
                               int dst_stride,
                               int width);

void TransposeWx4_32_RVV(const uint8_t* src,
                         int src_stride,
                         uint8_t* dst,
                         int dst_stride,
                         int width);

void TransposeWxH_32_RVV(const uint8_t* src,
                         int src_stride,
                         uint8_t* dst,
                         int dst_stride,
                         int width,
                         int height);

void TransposeWx4_32_LSX(const uint8_t* src,
                         int src_stride,
                         uint8_t* dst,
                         int dst_stride,
                         int width);

void TransposeWx4_32_LASX(const uint8_t* src,
                          int src_stride,
                          uint8_t* dst,
                          int dst_stride,
                          int width);

void TransposeWx4_32_WASMSIMD(const uint8_t* src,
                              int src_stride,
                              uint8_t* dst,
                              int dst_stride,
                              int width);

void TransposeWx4_32_C(const uint8_t* src,
                       int src_stride,
                       uint8_t* dst,
                       int dst_stride,
                       int width);

void TransposeWx4_32_Any_SSE2(const uint8_t* src,
                              int src_stride,
                              uint8_t* dst,
                              int dst_stride,
                              int width);

void TransposeWx8_32_Any_AVX2(const uint8_t* src,
                              int src_stride,
                              uint8_t* dst,
                              int dst_stride,
                              int width);

void TransposeWx4_32_Any_LSX(const uint8_t* src,
                             int src_stride,
                             uint8_t* dst,
                             int dst_stride,
                             int width);

void TransposeWx4_32_Any_LASX(const uint8_t* src,
                              int src_stride,
                              uint8_t* dst,
                              int dst_stride,
                              int width);

void TransposeWx4_32_Any_WASMSIMD(const uint8_t* src,
                                  int src_stride,
                                  uint8_t* dst,
                                  int dst_stride,
                                  int width);

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // INCLUDE_LIBYUV_ROTATE_ROW_H_
