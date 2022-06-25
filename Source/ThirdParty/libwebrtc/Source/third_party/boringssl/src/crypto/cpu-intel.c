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
 * [including the GNU Public Licence.] */

#include <openssl/cpu.h>


#if !defined(OPENSSL_NO_ASM) && (defined(OPENSSL_X86) || defined(OPENSSL_X86_64))

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
OPENSSL_MSVC_PRAGMA(warning(push, 3))
#include <immintrin.h>
#include <intrin.h>
OPENSSL_MSVC_PRAGMA(warning(pop))
#endif

#include "internal.h"


// OPENSSL_cpuid runs the cpuid instruction. |leaf| is passed in as EAX and ECX
// is set to zero. It writes EAX, EBX, ECX, and EDX to |*out_eax| through
// |*out_edx|.
static void OPENSSL_cpuid(uint32_t *out_eax, uint32_t *out_ebx,
                          uint32_t *out_ecx, uint32_t *out_edx, uint32_t leaf) {
#if defined(_MSC_VER)
  int tmp[4];
  __cpuid(tmp, (int)leaf);
  *out_eax = (uint32_t)tmp[0];
  *out_ebx = (uint32_t)tmp[1];
  *out_ecx = (uint32_t)tmp[2];
  *out_edx = (uint32_t)tmp[3];
#elif defined(__pic__) && defined(OPENSSL_32_BIT)
  // Inline assembly may not clobber the PIC register. For 32-bit, this is EBX.
  // See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=47602.
  __asm__ volatile (
    "xor %%ecx, %%ecx\n"
    "mov %%ebx, %%edi\n"
    "cpuid\n"
    "xchg %%edi, %%ebx\n"
    : "=a"(*out_eax), "=D"(*out_ebx), "=c"(*out_ecx), "=d"(*out_edx)
    : "a"(leaf)
  );
#else
  __asm__ volatile (
    "xor %%ecx, %%ecx\n"
    "cpuid\n"
    : "=a"(*out_eax), "=b"(*out_ebx), "=c"(*out_ecx), "=d"(*out_edx)
    : "a"(leaf)
  );
#endif
}

// OPENSSL_xgetbv returns the value of an Intel Extended Control Register (XCR).
// Currently only XCR0 is defined by Intel so |xcr| should always be zero.
static uint64_t OPENSSL_xgetbv(uint32_t xcr) {
#if defined(_MSC_VER)
  return (uint64_t)_xgetbv(xcr);
#else
  uint32_t eax, edx;
  __asm__ volatile ("xgetbv" : "=a"(eax), "=d"(edx) : "c"(xcr));
  return (((uint64_t)edx) << 32) | eax;
#endif
}

// handle_cpu_env applies the value from |in| to the CPUID values in |out[0]|
// and |out[1]|. See the comment in |OPENSSL_cpuid_setup| about this.
static void handle_cpu_env(uint32_t *out, const char *in) {
  const int invert = in[0] == '~';
  const int or = in[0] == '|';
  const int skip_first_byte = invert || or;
  const int hex = in[skip_first_byte] == '0' && in[skip_first_byte+1] == 'x';

  int sscanf_result;
  uint64_t v;
  if (hex) {
    sscanf_result = sscanf(in + invert + 2, "%" PRIx64, &v);
  } else {
    sscanf_result = sscanf(in + invert, "%" PRIu64, &v);
  }

  if (!sscanf_result) {
    return;
  }

  if (invert) {
    out[0] &= ~v;
    out[1] &= ~(v >> 32);
  } else if (or) {
    out[0] |= v;
    out[1] |= (v >> 32);
  } else {
    out[0] = v;
    out[1] = v >> 32;
  }
}

void OPENSSL_cpuid_setup(void) {
  // Determine the vendor and maximum input value.
  uint32_t eax, ebx, ecx, edx;
  OPENSSL_cpuid(&eax, &ebx, &ecx, &edx, 0);

  uint32_t num_ids = eax;

  int is_intel = ebx == 0x756e6547 /* Genu */ &&
                 edx == 0x49656e69 /* ineI */ &&
                 ecx == 0x6c65746e /* ntel */;
  int is_amd = ebx == 0x68747541 /* Auth */ &&
               edx == 0x69746e65 /* enti */ &&
               ecx == 0x444d4163 /* cAMD */;

  uint32_t extended_features[2] = {0};
  if (num_ids >= 7) {
    OPENSSL_cpuid(&eax, &ebx, &ecx, &edx, 7);
    extended_features[0] = ebx;
    extended_features[1] = ecx;
  }

  OPENSSL_cpuid(&eax, &ebx, &ecx, &edx, 1);

  if (is_amd) {
    // See https://www.amd.com/system/files/TechDocs/25481.pdf, page 10.
    const uint32_t base_family = (eax >> 8) & 15;
    const uint32_t base_model = (eax >> 4) & 15;

    uint32_t family = base_family;
    uint32_t model = base_model;
    if (base_family == 0xf) {
      const uint32_t ext_family = (eax >> 20) & 255;
      family += ext_family;
      const uint32_t ext_model = (eax >> 16) & 15;
      model |= ext_model << 4;
    }

    if (family < 0x17 || (family == 0x17 && 0x70 <= model && model <= 0x7f)) {
      // Disable RDRAND on AMD families before 0x17 (Zen) due to reported
      // failures after suspend.
      // https://bugzilla.redhat.com/show_bug.cgi?id=1150286
      // Also disable for family 0x17, models 0x70–0x7f, due to possible RDRAND
      // failures there too.
      ecx &= ~(1u << 30);
    }
  }

  // Force the hyper-threading bit so that the more conservative path is always
  // chosen.
  edx |= 1u << 28;

  // Reserved bit #20 was historically repurposed to control the in-memory
  // representation of RC4 state. Always set it to zero.
  edx &= ~(1u << 20);

  // Reserved bit #30 is repurposed to signal an Intel CPU.
  if (is_intel) {
    edx |= (1u << 30);

    // Clear the XSAVE bit on Knights Landing to mimic Silvermont. This enables
    // some Silvermont-specific codepaths which perform better. See OpenSSL
    // commit 64d92d74985ebb3d0be58a9718f9e080a14a8e7f.
    if ((eax & 0x0fff0ff0) == 0x00050670 /* Knights Landing */ ||
        (eax & 0x0fff0ff0) == 0x00080650 /* Knights Mill (per SDE) */) {
      ecx &= ~(1u << 26);
    }
  } else {
    edx &= ~(1u << 30);
  }

  // The SDBG bit is repurposed to denote AMD XOP support. Don't ever use AMD
  // XOP code paths.
  ecx &= ~(1u << 11);

  uint64_t xcr0 = 0;
  if (ecx & (1u << 27)) {
    // XCR0 may only be queried if the OSXSAVE bit is set.
    xcr0 = OPENSSL_xgetbv(0);
  }
  // See Intel manual, volume 1, section 14.3.
  if ((xcr0 & 6) != 6) {
    // YMM registers cannot be used.
    ecx &= ~(1u << 28);  // AVX
    ecx &= ~(1u << 12);  // FMA
    ecx &= ~(1u << 11);  // AMD XOP
    // Clear AVX2 and AVX512* bits.
    //
    // TODO(davidben): Should bits 17 and 26-28 also be cleared? Upstream
    // doesn't clear those.
    extended_features[0] &=
        ~((1u << 5) | (1u << 16) | (1u << 21) | (1u << 30) | (1u << 31));
  }
  // See Intel manual, volume 1, section 15.2.
  if ((xcr0 & 0xe6) != 0xe6) {
    // Clear AVX512F. Note we don't touch other AVX512 extensions because they
    // can be used with YMM.
    extended_features[0] &= ~(1u << 16);
  }

  // Disable ADX instructions on Knights Landing. See OpenSSL commit
  // 64d92d74985ebb3d0be58a9718f9e080a14a8e7f.
  if ((ecx & (1u << 26)) == 0) {
    extended_features[0] &= ~(1u << 19);
  }

  OPENSSL_ia32cap_P[0] = edx;
  OPENSSL_ia32cap_P[1] = ecx;
  OPENSSL_ia32cap_P[2] = extended_features[0];
  OPENSSL_ia32cap_P[3] = extended_features[1];

  const char *env1, *env2;
  env1 = getenv("OPENSSL_ia32cap");
  if (env1 == NULL) {
    return;
  }

  // OPENSSL_ia32cap can contain zero, one or two values, separated with a ':'.
  // Each value is a 64-bit, unsigned value which may start with "0x" to
  // indicate a hex value. Prior to the 64-bit value, a '~' or '|' may be given.
  //
  // If the '~' prefix is present:
  //   the value is inverted and ANDed with the probed CPUID result
  // If the '|' prefix is present:
  //   the value is ORed with the probed CPUID result
  // Otherwise:
  //   the value is taken as the result of the CPUID
  //
  // The first value determines OPENSSL_ia32cap_P[0] and [1]. The second [2]
  // and [3].

  handle_cpu_env(&OPENSSL_ia32cap_P[0], env1);
  env2 = strchr(env1, ':');
  if (env2 != NULL) {
    handle_cpu_env(&OPENSSL_ia32cap_P[2], env2 + 1);
  }
}

#endif  // !OPENSSL_NO_ASM && (OPENSSL_X86 || OPENSSL_X86_64)
