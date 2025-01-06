/* Copyright (c) 2017, Google Inc.
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

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE  // needed for syscall() on Linux.
#endif

#include <openssl/crypto.h>

#include <stdlib.h>
#if defined(BORINGSSL_FIPS)
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <openssl/digest.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

#include "bcm_interface.h"
#include "../bcm_support.h"
#include "../internal.h"

// TODO(crbug.com/362530616): When delocate is removed, build these files as
// separate compilation units again.
#include "aes/aes.c.inc"
#include "aes/aes_nohw.c.inc"
#include "aes/key_wrap.c.inc"
#include "aes/mode_wrappers.c.inc"
#include "bn/add.c.inc"
#include "bn/asm/x86_64-gcc.c.inc"
#include "bn/bn.c.inc"
#include "bn/bytes.c.inc"
#include "bn/cmp.c.inc"
#include "bn/ctx.c.inc"
#include "bn/div.c.inc"
#include "bn/div_extra.c.inc"
#include "bn/exponentiation.c.inc"
#include "bn/gcd.c.inc"
#include "bn/gcd_extra.c.inc"
#include "bn/generic.c.inc"
#include "bn/jacobi.c.inc"
#include "bn/montgomery.c.inc"
#include "bn/montgomery_inv.c.inc"
#include "bn/mul.c.inc"
#include "bn/prime.c.inc"
#include "bn/random.c.inc"
#include "bn/rsaz_exp.c.inc"
#include "bn/shift.c.inc"
#include "bn/sqrt.c.inc"
#include "cipher/aead.c.inc"
#include "cipher/cipher.c.inc"
#include "cipher/e_aes.c.inc"
#include "cipher/e_aesccm.c.inc"
#include "cmac/cmac.c.inc"
#include "dh/check.c.inc"
#include "dh/dh.c.inc"
#include "digest/digest.c.inc"
#include "digest/digests.c.inc"
#include "digestsign/digestsign.c.inc"
#include "ecdh/ecdh.c.inc"
#include "ecdsa/ecdsa.c.inc"
#include "ec/ec.c.inc"
#include "ec/ec_key.c.inc"
#include "ec/ec_montgomery.c.inc"
#include "ec/felem.c.inc"
#include "ec/oct.c.inc"
#include "ec/p224-64.c.inc"
#include "ec/p256.c.inc"
#include "ec/p256-nistz.c.inc"
#include "ec/scalar.c.inc"
#include "ec/simple.c.inc"
#include "ec/simple_mul.c.inc"
#include "ec/util.c.inc"
#include "ec/wnaf.c.inc"
#include "hkdf/hkdf.c.inc"
#include "hmac/hmac.c.inc"
#include "modes/cbc.c.inc"
#include "modes/cfb.c.inc"
#include "modes/ctr.c.inc"
#include "modes/gcm.c.inc"
#include "modes/gcm_nohw.c.inc"
#include "modes/ofb.c.inc"
#include "modes/polyval.c.inc"
#include "rand/ctrdrbg.c.inc"
#include "rand/rand.c.inc"
#include "rsa/blinding.c.inc"
#include "rsa/padding.c.inc"
#include "rsa/rsa.c.inc"
#include "rsa/rsa_impl.c.inc"
#include "self_check/fips.c.inc"
#include "self_check/self_check.c.inc"
#include "service_indicator/service_indicator.c.inc"
#include "sha/sha1.c.inc"
#include "sha/sha256.c.inc"
#include "sha/sha512.c.inc"
#include "tls/kdf.c.inc"


#if defined(BORINGSSL_FIPS)

#if !defined(OPENSSL_ASAN)

// These symbols are filled in by delocate.go (in static builds) or a linker
// script (in shared builds). They point to the start and end of the module, and
// the location of the integrity hash, respectively.
extern const uint8_t BORINGSSL_bcm_text_start[];
extern const uint8_t BORINGSSL_bcm_text_end[];
extern const uint8_t BORINGSSL_bcm_text_hash[];
#if defined(BORINGSSL_SHARED_LIBRARY)
extern const uint8_t BORINGSSL_bcm_rodata_start[];
extern const uint8_t BORINGSSL_bcm_rodata_end[];
#endif

// assert_within is used to sanity check that certain symbols are within the
// bounds of the integrity check. It checks that start <= symbol < end and
// aborts otherwise.
static void assert_within(const void *start, const void *symbol,
                          const void *end) {
  const uintptr_t start_val = (uintptr_t) start;
  const uintptr_t symbol_val = (uintptr_t) symbol;
  const uintptr_t end_val = (uintptr_t) end;

  if (start_val <= symbol_val && symbol_val < end_val) {
    return;
  }

  fprintf(
      CRYPTO_get_stderr(),
      "FIPS module doesn't span expected symbol. Expected %p <= %p < %p\n",
      start, symbol, end);
  BORINGSSL_FIPS_abort();
}

#if defined(OPENSSL_ANDROID) && defined(OPENSSL_AARCH64)
static void BORINGSSL_maybe_set_module_text_permissions(int permission) {
  // Android may be compiled in execute-only-memory mode, in which case the
  // .text segment cannot be read. That conflicts with the need for a FIPS
  // module to hash its own contents, therefore |mprotect| is used to make
  // the module's .text readable for the duration of the hashing process. In
  // other build configurations this is a no-op.
  const uintptr_t page_size = getpagesize();
  const uintptr_t page_start =
      ((uintptr_t)BORINGSSL_bcm_text_start) & ~(page_size - 1);

  if (mprotect((void *)page_start,
               ((uintptr_t)BORINGSSL_bcm_text_end) - page_start,
               permission) != 0) {
    perror("BoringSSL: mprotect");
  }
}
#else
static void BORINGSSL_maybe_set_module_text_permissions(int permission) {}
#endif  // !ANDROID

#endif  // !ASAN

static void __attribute__((constructor))
BORINGSSL_bcm_power_on_self_test(void) {
#if !defined(OPENSSL_ASAN)
  // Integrity tests cannot run under ASAN because it involves reading the full
  // .text section, which triggers the global-buffer overflow detection.
  if (!BORINGSSL_integrity_test()) {
    goto err;
  }
#endif  // OPENSSL_ASAN

  if (!boringssl_self_test_startup()) {
    goto err;
  }

  return;

err:
  BORINGSSL_FIPS_abort();
}

#if !defined(OPENSSL_ASAN)
int BORINGSSL_integrity_test(void) {
  const uint8_t *const start = BORINGSSL_bcm_text_start;
  const uint8_t *const end = BORINGSSL_bcm_text_end;

  assert_within(start, AES_encrypt, end);
  assert_within(start, RSA_sign, end);
  assert_within(start, BCM_rand_bytes, end);
  assert_within(start, EC_GROUP_cmp, end);
  assert_within(start, BCM_sha256_update, end);
  assert_within(start, ecdsa_verify_fixed, end);
  assert_within(start, EVP_AEAD_CTX_seal, end);

#if defined(BORINGSSL_SHARED_LIBRARY)
  const uint8_t *const rodata_start = BORINGSSL_bcm_rodata_start;
  const uint8_t *const rodata_end = BORINGSSL_bcm_rodata_end;
#else
  // In the static build, read-only data is placed within the .text segment.
  const uint8_t *const rodata_start = BORINGSSL_bcm_text_start;
  const uint8_t *const rodata_end = BORINGSSL_bcm_text_end;
#endif

  assert_within(rodata_start, kPrimes, rodata_end);
  assert_within(rodata_start, kP256Field, rodata_end);
  assert_within(rodata_start, kPKCS1SigPrefixes, rodata_end);

  uint8_t result[SHA256_DIGEST_LENGTH];
  const EVP_MD *const kHashFunction = EVP_sha256();
  if (!boringssl_self_test_sha256() ||
      !boringssl_self_test_hmac_sha256()) {
    return 0;
  }

  static const uint8_t kHMACKey[64] = {0};
  unsigned result_len;
  HMAC_CTX hmac_ctx;
  HMAC_CTX_init(&hmac_ctx);
  if (!HMAC_Init_ex(&hmac_ctx, kHMACKey, sizeof(kHMACKey), kHashFunction,
                    NULL /* no ENGINE */)) {
    fprintf(CRYPTO_get_stderr(), "HMAC_Init_ex failed.\n");
    return 0;
  }

  BORINGSSL_maybe_set_module_text_permissions(PROT_READ | PROT_EXEC);
#if defined(BORINGSSL_SHARED_LIBRARY)
  uint64_t length = end - start;
  HMAC_Update(&hmac_ctx, (const uint8_t *) &length, sizeof(length));
  HMAC_Update(&hmac_ctx, start, length);

  length = rodata_end - rodata_start;
  HMAC_Update(&hmac_ctx, (const uint8_t *) &length, sizeof(length));
  HMAC_Update(&hmac_ctx, rodata_start, length);
#else
  HMAC_Update(&hmac_ctx, start, end - start);
#endif
  BORINGSSL_maybe_set_module_text_permissions(PROT_EXEC);

  if (!HMAC_Final(&hmac_ctx, result, &result_len) ||
      result_len != sizeof(result)) {
    fprintf(CRYPTO_get_stderr(), "HMAC failed.\n");
    return 0;
  }
  HMAC_CTX_cleanse(&hmac_ctx); // FIPS 140-3, AS05.10.

  const uint8_t *expected = BORINGSSL_bcm_text_hash;

  if (!check_test(expected, result, sizeof(result), "FIPS integrity test")) {
#if !defined(BORINGSSL_FIPS_BREAK_TESTS)
    return 0;
#endif
  }

  OPENSSL_cleanse(result, sizeof(result)); // FIPS 140-3, AS05.10.
  return 1;
}

const uint8_t* FIPS_module_hash(void) {
  return BORINGSSL_bcm_text_hash;
}

#endif  // OPENSSL_ASAN

void BORINGSSL_FIPS_abort(void) {
  for (;;) {
    abort();
    exit(1);
  }
}

#endif  // BORINGSSL_FIPS
