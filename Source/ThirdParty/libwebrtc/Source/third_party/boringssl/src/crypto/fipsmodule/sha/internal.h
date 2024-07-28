/* Copyright (c) 2018, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#ifndef OPENSSL_HEADER_SHA_INTERNAL_H
#define OPENSSL_HEADER_SHA_INTERNAL_H

#include <openssl/base.h>

#include "../../internal.h"

#if defined(__cplusplus)
extern "C" {
#endif

// Define SHA{n}[_{variant}]_ASM if sha{n}_block_data_order[_{variant}] is
// defined in assembly.

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_ARM)

#define SHA1_ASM_NOHW
#define SHA256_ASM_NOHW
#define SHA512_ASM_NOHW

#define SHA1_ASM_HW
OPENSSL_INLINE int sha1_hw_capable(void) {
  return CRYPTO_is_ARMv8_SHA1_capable();
}

#define SHA1_ASM_NEON
void sha1_block_data_order_neon(uint32_t state[5], const uint8_t *data,
                                size_t num);

#define SHA256_ASM_HW
OPENSSL_INLINE int sha256_hw_capable(void) {
  return CRYPTO_is_ARMv8_SHA256_capable();
}

#define SHA256_ASM_NEON
void sha256_block_data_order_neon(uint32_t state[8], const uint8_t *data,
                                  size_t num);

// Armv8.2 SHA-512 instructions are not available in 32-bit.
#define SHA512_ASM_NEON
void sha512_block_data_order_neon(uint64_t state[8], const uint8_t *data,
                                  size_t num);

#elif !defined(OPENSSL_NO_ASM) && defined(OPENSSL_AARCH64)

#define SHA1_ASM_NOHW
#define SHA256_ASM_NOHW
#define SHA512_ASM_NOHW

#define SHA1_ASM_HW
OPENSSL_INLINE int sha1_hw_capable(void) {
  return CRYPTO_is_ARMv8_SHA1_capable();
}

#define SHA256_ASM_HW
OPENSSL_INLINE int sha256_hw_capable(void) {
  return CRYPTO_is_ARMv8_SHA256_capable();
}

#define SHA512_ASM_HW
OPENSSL_INLINE int sha512_hw_capable(void) {
  return CRYPTO_is_ARMv8_SHA512_capable();
}

#elif !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86)

#define SHA1_ASM_NOHW
#define SHA256_ASM_NOHW
#define SHA512_ASM_NOHW

#define SHA1_ASM_SSSE3
OPENSSL_INLINE int sha1_ssse3_capable(void) {
  // TODO(davidben): Do we need to check the FXSR bit? The Intel manual does not
  // say to.
  return CRYPTO_is_SSSE3_capable() && CRYPTO_is_FXSR_capable();
}
void sha1_block_data_order_ssse3(uint32_t state[5], const uint8_t *data,
                                 size_t num);

#define SHA1_ASM_AVX
OPENSSL_INLINE int sha1_avx_capable(void) {
  // Pre-Zen AMD CPUs had slow SHLD/SHRD; Zen added the SHA extension; see the
  // discussion in sha1-586.pl.
  //
  // TODO(davidben): Should we enable SHAEXT on 32-bit x86?
  // TODO(davidben): Do we need to check the FXSR bit? The Intel manual does not
  // say to.
  return CRYPTO_is_AVX_capable() && CRYPTO_is_intel_cpu() &&
         CRYPTO_is_FXSR_capable();
}
void sha1_block_data_order_avx(uint32_t state[5], const uint8_t *data,
                               size_t num);

#define SHA256_ASM_SSSE3
OPENSSL_INLINE int sha256_ssse3_capable(void) {
  // TODO(davidben): Do we need to check the FXSR bit? The Intel manual does not
  // say to.
  return CRYPTO_is_SSSE3_capable() && CRYPTO_is_FXSR_capable();
}
void sha256_block_data_order_ssse3(uint32_t state[8], const uint8_t *data,
                                   size_t num);

#define SHA256_ASM_AVX
OPENSSL_INLINE int sha256_avx_capable(void) {
  // Pre-Zen AMD CPUs had slow SHLD/SHRD; Zen added the SHA extension; see the
  // discussion in sha1-586.pl.
  //
  // TODO(davidben): Should we enable SHAEXT on 32-bit x86?
  // TODO(davidben): Do we need to check the FXSR bit? The Intel manual does not
  // say to.
  return CRYPTO_is_AVX_capable() && CRYPTO_is_intel_cpu() &&
         CRYPTO_is_FXSR_capable();
}
void sha256_block_data_order_avx(uint32_t state[8], const uint8_t *data,
                                 size_t num);

#define SHA512_ASM_SSSE3
OPENSSL_INLINE int sha512_ssse3_capable(void) {
  // TODO(davidben): Do we need to check the FXSR bit? The Intel manual does not
  // say to.
  return CRYPTO_is_SSSE3_capable() && CRYPTO_is_FXSR_capable();
}
void sha512_block_data_order_ssse3(uint64_t state[8], const uint8_t *data,
                                   size_t num);

#elif !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64)

#define SHA1_ASM_NOHW
#define SHA256_ASM_NOHW
#define SHA512_ASM_NOHW

#define SHA1_ASM_HW
OPENSSL_INLINE int sha1_hw_capable(void) {
  return CRYPTO_is_x86_SHA_capable() && CRYPTO_is_SSSE3_capable();
}

#define SHA1_ASM_AVX2
OPENSSL_INLINE int sha1_avx2_capable(void) {
  return CRYPTO_is_AVX2_capable() && CRYPTO_is_BMI2_capable() &&
         CRYPTO_is_BMI1_capable();
}
void sha1_block_data_order_avx2(uint32_t state[5], const uint8_t *data,
                                size_t num);

#define SHA1_ASM_AVX
OPENSSL_INLINE int sha1_avx_capable(void) {
  // Pre-Zen AMD CPUs had slow SHLD/SHRD; Zen added the SHA extension; see the
  // discussion in sha1-586.pl.
  return CRYPTO_is_AVX_capable() && CRYPTO_is_intel_cpu();
}
void sha1_block_data_order_avx(uint32_t state[5], const uint8_t *data,
                               size_t num);

#define SHA1_ASM_SSSE3
OPENSSL_INLINE int sha1_ssse3_capable(void) {
  return CRYPTO_is_SSSE3_capable();
}
void sha1_block_data_order_ssse3(uint32_t state[5], const uint8_t *data,
                                 size_t num);

#define SHA256_ASM_HW
OPENSSL_INLINE int sha256_hw_capable(void) {
  // Note that the original assembly did not check SSSE3.
  return CRYPTO_is_x86_SHA_capable() && CRYPTO_is_SSSE3_capable();
}

#define SHA256_ASM_AVX
OPENSSL_INLINE int sha256_avx_capable(void) {
  // Pre-Zen AMD CPUs had slow SHLD/SHRD; Zen added the SHA extension; see the
  // discussion in sha1-586.pl.
  return CRYPTO_is_AVX_capable() && CRYPTO_is_intel_cpu();
}
void sha256_block_data_order_avx(uint32_t state[8], const uint8_t *data,
                                 size_t num);

#define SHA256_ASM_SSSE3
OPENSSL_INLINE int sha256_ssse3_capable(void) {
  return CRYPTO_is_SSSE3_capable();
}
void sha256_block_data_order_ssse3(uint32_t state[8], const uint8_t *data,
                                   size_t num);

#define SHA512_ASM_AVX
OPENSSL_INLINE int sha512_avx_capable(void) {
  // Pre-Zen AMD CPUs had slow SHLD/SHRD; Zen added the SHA extension; see the
  // discussion in sha1-586.pl.
  return CRYPTO_is_AVX_capable() && CRYPTO_is_intel_cpu();
}
void sha512_block_data_order_avx(uint64_t state[8], const uint8_t *data,
                                 size_t num);

#endif

#if defined(SHA1_ASM_HW)
void sha1_block_data_order_hw(uint32_t state[5], const uint8_t *data,
                              size_t num);
#endif
#if defined(SHA1_ASM_NOHW)
void sha1_block_data_order_nohw(uint32_t state[5], const uint8_t *data,
                                size_t num);
#endif

#if defined(SHA256_ASM_HW)
void sha256_block_data_order_hw(uint32_t state[8], const uint8_t *data,
                                size_t num);
#endif
#if defined(SHA256_ASM_NOHW)
void sha256_block_data_order_nohw(uint32_t state[8], const uint8_t *data,
                                  size_t num);
#endif

#if defined(SHA512_ASM_HW)
void sha512_block_data_order_hw(uint64_t state[8], const uint8_t *data,
                                size_t num);
#endif

#if defined(SHA512_ASM_NOHW)
void sha512_block_data_order_nohw(uint64_t state[8], const uint8_t *data,
                                  size_t num);
#endif

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif  // OPENSSL_HEADER_SHA_INTERNAL_H
