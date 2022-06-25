/* ====================================================================
 * Copyright (c) 2011 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
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

#include <openssl/dh.h>

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/mem.h>

#include "../fipsmodule/bn/internal.h"


BIGNUM *BN_get_rfc3526_prime_1536(BIGNUM *ret) {
  static const BN_ULONG kPrime1536Data[] = {
      TOBN(0xffffffff, 0xffffffff), TOBN(0xf1746c08, 0xca237327),
      TOBN(0x670c354e, 0x4abc9804), TOBN(0x9ed52907, 0x7096966d),
      TOBN(0x1c62f356, 0x208552bb), TOBN(0x83655d23, 0xdca3ad96),
      TOBN(0x69163fa8, 0xfd24cf5f), TOBN(0x98da4836, 0x1c55d39a),
      TOBN(0xc2007cb8, 0xa163bf05), TOBN(0x49286651, 0xece45b3d),
      TOBN(0xae9f2411, 0x7c4b1fe6), TOBN(0xee386bfb, 0x5a899fa5),
      TOBN(0x0bff5cb6, 0xf406b7ed), TOBN(0xf44c42e9, 0xa637ed6b),
      TOBN(0xe485b576, 0x625e7ec6), TOBN(0x4fe1356d, 0x6d51c245),
      TOBN(0x302b0a6d, 0xf25f1437), TOBN(0xef9519b3, 0xcd3a431b),
      TOBN(0x514a0879, 0x8e3404dd), TOBN(0x020bbea6, 0x3b139b22),
      TOBN(0x29024e08, 0x8a67cc74), TOBN(0xc4c6628b, 0x80dc1cd1),
      TOBN(0xc90fdaa2, 0x2168c234), TOBN(0xffffffff, 0xffffffff),
  };

  static const BIGNUM kPrime1536BN = STATIC_BIGNUM(kPrime1536Data);

  BIGNUM *alloc = NULL;
  if (ret == NULL) {
    alloc = BN_new();
    if (alloc == NULL) {
      return NULL;
    }
    ret = alloc;
  }

  if (!BN_copy(ret, &kPrime1536BN)) {
    BN_free(alloc);
    return NULL;
  }

  return ret;
}

int DH_generate_parameters_ex(DH *dh, int prime_bits, int generator,
                              BN_GENCB *cb) {
  // We generate DH parameters as follows
  // find a prime q which is prime_bits/2 bits long.
  // p=(2*q)+1 or (p-1)/2 = q
  // For this case, g is a generator if
  // g^((p-1)/q) mod p != 1 for values of q which are the factors of p-1.
  // Since the factors of p-1 are q and 2, we just need to check
  // g^2 mod p != 1 and g^q mod p != 1.
  //
  // Having said all that,
  // there is another special case method for the generators 2, 3 and 5.
  // for 2, p mod 24 == 11
  // for 3, p mod 12 == 5  <<<<< does not work for safe primes.
  // for 5, p mod 10 == 3 or 7
  //
  // Thanks to Phil Karn <karn@qualcomm.com> for the pointers about the
  // special generators and for answering some of my questions.
  //
  // I've implemented the second simple method :-).
  // Since DH should be using a safe prime (both p and q are prime),
  // this generator function can take a very very long time to run.

  // Actually there is no reason to insist that 'generator' be a generator.
  // It's just as OK (and in some sense better) to use a generator of the
  // order-q subgroup.

  BIGNUM *t1, *t2;
  int g, ok = 0;
  BN_CTX *ctx = NULL;

  ctx = BN_CTX_new();
  if (ctx == NULL) {
    goto err;
  }
  BN_CTX_start(ctx);
  t1 = BN_CTX_get(ctx);
  t2 = BN_CTX_get(ctx);
  if (t1 == NULL || t2 == NULL) {
    goto err;
  }

  // Make sure |dh| has the necessary elements
  if (dh->p == NULL) {
    dh->p = BN_new();
    if (dh->p == NULL) {
      goto err;
    }
  }
  if (dh->g == NULL) {
    dh->g = BN_new();
    if (dh->g == NULL) {
      goto err;
    }
  }

  if (generator <= 1) {
    OPENSSL_PUT_ERROR(DH, DH_R_BAD_GENERATOR);
    goto err;
  }
  if (generator == DH_GENERATOR_2) {
    if (!BN_set_word(t1, 24)) {
      goto err;
    }
    if (!BN_set_word(t2, 11)) {
      goto err;
    }
    g = 2;
  } else if (generator == DH_GENERATOR_5) {
    if (!BN_set_word(t1, 10)) {
      goto err;
    }
    if (!BN_set_word(t2, 3)) {
      goto err;
    }
    // BN_set_word(t3,7); just have to miss
    // out on these ones :-(
    g = 5;
  } else {
    // in the general case, don't worry if 'generator' is a
    // generator or not: since we are using safe primes,
    // it will generate either an order-q or an order-2q group,
    // which both is OK
    if (!BN_set_word(t1, 2)) {
      goto err;
    }
    if (!BN_set_word(t2, 1)) {
      goto err;
    }
    g = generator;
  }

  if (!BN_generate_prime_ex(dh->p, prime_bits, 1, t1, t2, cb)) {
    goto err;
  }
  if (!BN_GENCB_call(cb, 3, 0)) {
    goto err;
  }
  if (!BN_set_word(dh->g, g)) {
    goto err;
  }
  ok = 1;

err:
  if (!ok) {
    OPENSSL_PUT_ERROR(DH, ERR_R_BN_LIB);
  }

  if (ctx != NULL) {
    BN_CTX_end(ctx);
    BN_CTX_free(ctx);
  }
  return ok;
}

static int int_dh_bn_cpy(BIGNUM **dst, const BIGNUM *src) {
  BIGNUM *a = NULL;

  if (src) {
    a = BN_dup(src);
    if (!a) {
      return 0;
    }
  }

  BN_free(*dst);
  *dst = a;
  return 1;
}

static int int_dh_param_copy(DH *to, const DH *from, int is_x942) {
  if (is_x942 == -1) {
    is_x942 = !!from->q;
  }
  if (!int_dh_bn_cpy(&to->p, from->p) ||
      !int_dh_bn_cpy(&to->g, from->g)) {
    return 0;
  }

  if (!is_x942) {
    return 1;
  }

  if (!int_dh_bn_cpy(&to->q, from->q) ||
      !int_dh_bn_cpy(&to->j, from->j)) {
    return 0;
  }

  OPENSSL_free(to->seed);
  to->seed = NULL;
  to->seedlen = 0;

  if (from->seed) {
    to->seed = OPENSSL_memdup(from->seed, from->seedlen);
    if (!to->seed) {
      return 0;
    }
    to->seedlen = from->seedlen;
  }

  return 1;
}

DH *DHparams_dup(const DH *dh) {
  DH *ret = DH_new();
  if (!ret) {
    return NULL;
  }

  if (!int_dh_param_copy(ret, dh, -1)) {
    DH_free(ret);
    return NULL;
  }

  return ret;
}
