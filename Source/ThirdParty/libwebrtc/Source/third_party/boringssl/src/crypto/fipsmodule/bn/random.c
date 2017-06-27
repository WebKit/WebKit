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
 */
/* ====================================================================
 * Copyright (c) 1998-2001 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com). */

#include <openssl/bn.h>

#include <string.h>

#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/type_check.h>

#include "../../internal.h"
#include "../rand/internal.h"


static const uint8_t kDefaultAdditionalData[32] = {0};

static int bn_rand_with_additional_data(BIGNUM *rnd, int bits, int top,
                                        int bottom,
                                        const uint8_t additional_data[32]) {
  uint8_t *buf = NULL;
  int ret = 0, bit, bytes, mask;

  if (rnd == NULL) {
    return 0;
  }

  if (top != BN_RAND_TOP_ANY && top != BN_RAND_TOP_ONE &&
      top != BN_RAND_TOP_TWO) {
    OPENSSL_PUT_ERROR(BN, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  if (bottom != BN_RAND_BOTTOM_ANY && bottom != BN_RAND_BOTTOM_ODD) {
    OPENSSL_PUT_ERROR(BN, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  if (bits == 0) {
    BN_zero(rnd);
    return 1;
  }

  bytes = (bits + 7) / 8;
  bit = (bits - 1) % 8;
  mask = 0xff << (bit + 1);

  buf = OPENSSL_malloc(bytes);
  if (buf == NULL) {
    OPENSSL_PUT_ERROR(BN, ERR_R_MALLOC_FAILURE);
    goto err;
  }

  /* Make a random number and set the top and bottom bits. */
  RAND_bytes_with_additional_data(buf, bytes, additional_data);

  if (top != BN_RAND_TOP_ANY) {
    if (top == BN_RAND_TOP_TWO && bits > 1) {
      if (bit == 0) {
        buf[0] = 1;
        buf[1] |= 0x80;
      } else {
        buf[0] |= (3 << (bit - 1));
      }
    } else {
      buf[0] |= (1 << bit);
    }
  }

  buf[0] &= ~mask;

  /* Set the bottom bit if requested, */
  if (bottom == BN_RAND_BOTTOM_ODD)  {
    buf[bytes - 1] |= 1;
  }

  if (!BN_bin2bn(buf, bytes, rnd)) {
    goto err;
  }

  ret = 1;

err:
  if (buf != NULL) {
    OPENSSL_cleanse(buf, bytes);
    OPENSSL_free(buf);
  }
  return (ret);
}

int BN_rand(BIGNUM *rnd, int bits, int top, int bottom) {
  return bn_rand_with_additional_data(rnd, bits, top, bottom,
                                      kDefaultAdditionalData);
}

int BN_pseudo_rand(BIGNUM *rnd, int bits, int top, int bottom) {
  return BN_rand(rnd, bits, top, bottom);
}

static int bn_rand_range_with_additional_data(
    BIGNUM *r, BN_ULONG min_inclusive, const BIGNUM *max_exclusive,
    const uint8_t additional_data[32]) {
  if (BN_cmp_word(max_exclusive, min_inclusive) <= 0) {
    OPENSSL_PUT_ERROR(BN, BN_R_INVALID_RANGE);
    return 0;
  }

  /* This function is used to implement steps 4 through 7 of FIPS 186-4
   * appendices B.4.2 and B.5.2. When called in those contexts, |max_exclusive|
   * is n and |min_inclusive| is one. */
  unsigned count = 100;
  unsigned n = BN_num_bits(max_exclusive); /* n > 0 */
  do {
    if (!--count) {
      OPENSSL_PUT_ERROR(BN, BN_R_TOO_MANY_ITERATIONS);
      return 0;
    }

    if (/* steps 4 and 5 */
        !bn_rand_with_additional_data(r, n, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY,
                                      additional_data) ||
        /* step 7 */
        !BN_add_word(r, min_inclusive)) {
      return 0;
    }

    /* Step 6. This loops if |r| >= |max_exclusive|. This is identical to
     * checking |r| > |max_exclusive| - 1 or |r| - 1 > |max_exclusive| - 2, the
     * formulation stated in FIPS 186-4. */
  } while (BN_cmp(r, max_exclusive) >= 0);

  return 1;
}

int BN_rand_range_ex(BIGNUM *r, BN_ULONG min_inclusive,
                     const BIGNUM *max_exclusive) {
  return bn_rand_range_with_additional_data(r, min_inclusive, max_exclusive,
                                            kDefaultAdditionalData);
}

int BN_rand_range(BIGNUM *r, const BIGNUM *range) {
  return BN_rand_range_ex(r, 0, range);
}

int BN_pseudo_rand_range(BIGNUM *r, const BIGNUM *range) {
  return BN_rand_range(r, range);
}

int BN_generate_dsa_nonce(BIGNUM *out, const BIGNUM *range, const BIGNUM *priv,
                          const uint8_t *message, size_t message_len,
                          BN_CTX *ctx) {
  /* We copy |priv| into a local buffer to avoid furthur exposing its
   * length. */
  uint8_t private_bytes[96];
  size_t todo = sizeof(priv->d[0]) * priv->top;
  if (todo > sizeof(private_bytes)) {
    /* No reasonable DSA or ECDSA key should have a private key
     * this large and we don't handle this case in order to avoid
     * leaking the length of the private key. */
    OPENSSL_PUT_ERROR(BN, BN_R_PRIVATE_KEY_TOO_LARGE);
    return 0;
  }
  OPENSSL_memcpy(private_bytes, priv->d, todo);
  OPENSSL_memset(private_bytes + todo, 0, sizeof(private_bytes) - todo);

  /* Pass a SHA512 hash of the private key and message as additional data into
   * the RBG. This is a hardening measure against entropy failure. */
  OPENSSL_COMPILE_ASSERT(SHA512_DIGEST_LENGTH >= 32,
                         additional_data_is_too_large_for_sha512);
  SHA512_CTX sha;
  uint8_t digest[SHA512_DIGEST_LENGTH];
  SHA512_Init(&sha);
  SHA512_Update(&sha, private_bytes, sizeof(private_bytes));
  SHA512_Update(&sha, message, message_len);
  SHA512_Final(digest, &sha);

  /* Select a value k from [1, range-1], following FIPS 186-4 appendix B.5.2. */
  return bn_rand_range_with_additional_data(out, 1, range, digest);
}
