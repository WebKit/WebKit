/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com). */

#ifndef OPENSSL_HEADER_CPU_H
#define OPENSSL_HEADER_CPU_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif


// Runtime CPU feature support


#if defined(OPENSSL_X86) || defined(OPENSSL_X86_64)
// OPENSSL_ia32cap_P contains the Intel CPUID bits when running on an x86 or
// x86-64 system.
//
//   Index 0:
//     EDX for CPUID where EAX = 1
//     Bit 20 is always zero
//     Bit 28 is adjusted to reflect whether the data cache is shared between
//       multiple logical cores
//     Bit 30 is used to indicate an Intel CPU
//   Index 1:
//     ECX for CPUID where EAX = 1
//     Bit 11 is used to indicate AMD XOP support, not SDBG
//   Index 2:
//     EBX for CPUID where EAX = 7
//   Index 3:
//     ECX for CPUID where EAX = 7
//
// Note: the CPUID bits are pre-adjusted for the OSXSAVE bit and the YMM and XMM
// bits in XCR0, so it is not necessary to check those.
extern uint32_t OPENSSL_ia32cap_P[4];

#if defined(BORINGSSL_FIPS) && !defined(BORINGSSL_SHARED_LIBRARY)
const uint32_t *OPENSSL_ia32cap_get(void);
#else
OPENSSL_INLINE const uint32_t *OPENSSL_ia32cap_get(void) {
  return OPENSSL_ia32cap_P;
}
#endif

#endif

#if defined(OPENSSL_ARM) || defined(OPENSSL_AARCH64)

#if defined(OPENSSL_APPLE)
// iOS builds use the static ARM configuration.
#define OPENSSL_STATIC_ARMCAP
#endif

#if !defined(OPENSSL_STATIC_ARMCAP)
// CRYPTO_is_NEON_capable_at_runtime returns true if the current CPU has a NEON
// unit. Note that |OPENSSL_armcap_P| also exists and contains the same
// information in a form that's easier for assembly to use.
OPENSSL_EXPORT int CRYPTO_is_NEON_capable_at_runtime(void);

// CRYPTO_is_ARMv8_AES_capable_at_runtime returns true if the current CPU
// supports the ARMv8 AES instruction.
int CRYPTO_is_ARMv8_AES_capable_at_runtime(void);

// CRYPTO_is_ARMv8_PMULL_capable_at_runtime returns true if the current CPU
// supports the ARMv8 PMULL instruction.
int CRYPTO_is_ARMv8_PMULL_capable_at_runtime(void);

#if defined(OPENSSL_ARM)
// CRYPTO_has_broken_NEON returns one if the current CPU is known to have a
// broken NEON unit. See https://crbug.com/341598.
OPENSSL_EXPORT int CRYPTO_has_broken_NEON(void);

// CRYPTO_needs_hwcap2_workaround returns one if the ARMv8 AArch32 AT_HWCAP2
// workaround was needed. See https://crbug.com/boringssl/46.
OPENSSL_EXPORT int CRYPTO_needs_hwcap2_workaround(void);
#endif
#endif  // !OPENSSL_STATIC_ARMCAP

// CRYPTO_is_NEON_capable returns true if the current CPU has a NEON unit. If
// this is known statically, it is a constant inline function.
OPENSSL_INLINE int CRYPTO_is_NEON_capable(void) {
#if defined(__ARM_NEON__) || defined(__ARM_NEON) || \
    defined(OPENSSL_STATIC_ARMCAP_NEON)
  return 1;
#elif defined(OPENSSL_STATIC_ARMCAP)
  return 0;
#else
  return CRYPTO_is_NEON_capable_at_runtime();
#endif
}

OPENSSL_INLINE int CRYPTO_is_ARMv8_AES_capable(void) {
#if defined(OPENSSL_STATIC_ARMCAP_AES) || defined(__ARM_FEATURE_CRYPTO)
  return 1;
#elif defined(OPENSSL_STATIC_ARMCAP)
  return 0;
#else
  return CRYPTO_is_ARMv8_AES_capable_at_runtime();
#endif
}

OPENSSL_INLINE int CRYPTO_is_ARMv8_PMULL_capable(void) {
#if defined(OPENSSL_STATIC_ARMCAP_PMULL) || defined(__ARM_FEATURE_CRYPTO)
  return 1;
#elif defined(OPENSSL_STATIC_ARMCAP)
  return 0;
#else
  return CRYPTO_is_ARMv8_PMULL_capable_at_runtime();
#endif
}

#endif  // OPENSSL_ARM || OPENSSL_AARCH64

#if defined(OPENSSL_PPC64LE)

// CRYPTO_is_PPC64LE_vcrypto_capable returns true iff the current CPU supports
// the Vector.AES category of instructions.
int CRYPTO_is_PPC64LE_vcrypto_capable(void);

extern unsigned long OPENSSL_ppc64le_hwcap2;

#endif  // OPENSSL_PPC64LE

#if defined(BORINGSSL_DISPATCH_TEST)
// Runtime CPU dispatch testing support

// BORINGSSL_function_hit is an array of flags. The following functions will
// set these flags if BORINGSSL_DISPATCH_TEST is defined.
//   0: aes_hw_ctr32_encrypt_blocks
//   1: aes_hw_encrypt
//   2: aesni_gcm_encrypt
//   3: aes_hw_set_encrypt_key
//   4: vpaes_encrypt
//   5: vpaes_set_encrypt_key
extern uint8_t BORINGSSL_function_hit[7];
#endif  // BORINGSSL_DISPATCH_TEST


#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_CPU_H
