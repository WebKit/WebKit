/* ====================================================================
 * Copyright (c) 1998-2005 The OpenSSL Project.  All rights reserved.
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
 *    openssl-core@OpenSSL.org.
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

#include <openssl/ecdsa.h>

#include <assert.h>
#include <string.h>

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/mem.h>

#include "../bn/internal.h"
#include "../ec/internal.h"
#include "../../internal.h"


/* digest_to_bn interprets |digest_len| bytes from |digest| as a big-endian
 * number and sets |out| to that value. It then truncates |out| so that it's,
 * at most, as long as |order|. It returns one on success and zero otherwise. */
static int digest_to_bn(BIGNUM *out, const uint8_t *digest, size_t digest_len,
                        const BIGNUM *order) {
  size_t num_bits;

  num_bits = BN_num_bits(order);
  /* Need to truncate digest if it is too long: first truncate whole
   * bytes. */
  if (8 * digest_len > num_bits) {
    digest_len = (num_bits + 7) / 8;
  }
  if (!BN_bin2bn(digest, digest_len, out)) {
    OPENSSL_PUT_ERROR(ECDSA, ERR_R_BN_LIB);
    return 0;
  }

  /* If still too long truncate remaining bits with a shift */
  if ((8 * digest_len > num_bits) &&
      !BN_rshift(out, out, 8 - (num_bits & 0x7))) {
    OPENSSL_PUT_ERROR(ECDSA, ERR_R_BN_LIB);
    return 0;
  }

  return 1;
}

ECDSA_SIG *ECDSA_SIG_new(void) {
  ECDSA_SIG *sig = OPENSSL_malloc(sizeof(ECDSA_SIG));
  if (sig == NULL) {
    return NULL;
  }
  sig->r = BN_new();
  sig->s = BN_new();
  if (sig->r == NULL || sig->s == NULL) {
    ECDSA_SIG_free(sig);
    return NULL;
  }
  return sig;
}

void ECDSA_SIG_free(ECDSA_SIG *sig) {
  if (sig == NULL) {
    return;
  }

  BN_free(sig->r);
  BN_free(sig->s);
  OPENSSL_free(sig);
}

ECDSA_SIG *ECDSA_do_sign(const uint8_t *digest, size_t digest_len,
                         const EC_KEY *key) {
  return ECDSA_do_sign_ex(digest, digest_len, NULL, NULL, key);
}

int ECDSA_do_verify(const uint8_t *digest, size_t digest_len,
                    const ECDSA_SIG *sig, const EC_KEY *eckey) {
  int ret = 0;
  BN_CTX *ctx;
  BIGNUM *u1, *u2, *m, *X;
  EC_POINT *point = NULL;
  const EC_GROUP *group;
  const EC_POINT *pub_key;

  /* check input values */
  if ((group = EC_KEY_get0_group(eckey)) == NULL ||
      (pub_key = EC_KEY_get0_public_key(eckey)) == NULL ||
      sig == NULL) {
    OPENSSL_PUT_ERROR(ECDSA, ECDSA_R_MISSING_PARAMETERS);
    return 0;
  }

  ctx = BN_CTX_new();
  if (!ctx) {
    OPENSSL_PUT_ERROR(ECDSA, ERR_R_MALLOC_FAILURE);
    return 0;
  }
  BN_CTX_start(ctx);
  u1 = BN_CTX_get(ctx);
  u2 = BN_CTX_get(ctx);
  m = BN_CTX_get(ctx);
  X = BN_CTX_get(ctx);
  if (u1 == NULL || u2 == NULL || m == NULL || X == NULL) {
    OPENSSL_PUT_ERROR(ECDSA, ERR_R_BN_LIB);
    goto err;
  }

  const BIGNUM *order = EC_GROUP_get0_order(group);
  if (BN_is_zero(sig->r) || BN_is_negative(sig->r) ||
      BN_ucmp(sig->r, order) >= 0 || BN_is_zero(sig->s) ||
      BN_is_negative(sig->s) || BN_ucmp(sig->s, order) >= 0) {
    OPENSSL_PUT_ERROR(ECDSA, ECDSA_R_BAD_SIGNATURE);
    goto err;
  }
  /* calculate tmp1 = inv(S) mod order */
  int no_inverse;
  if (!BN_mod_inverse_odd(u2, &no_inverse, sig->s, order, ctx)) {
    OPENSSL_PUT_ERROR(ECDSA, ERR_R_BN_LIB);
    goto err;
  }
  if (!digest_to_bn(m, digest, digest_len, order)) {
    goto err;
  }
  /* u1 = m * tmp mod order */
  if (!BN_mod_mul(u1, m, u2, order, ctx)) {
    OPENSSL_PUT_ERROR(ECDSA, ERR_R_BN_LIB);
    goto err;
  }
  /* u2 = r * w mod q */
  if (!BN_mod_mul(u2, sig->r, u2, order, ctx)) {
    OPENSSL_PUT_ERROR(ECDSA, ERR_R_BN_LIB);
    goto err;
  }

  point = EC_POINT_new(group);
  if (point == NULL) {
    OPENSSL_PUT_ERROR(ECDSA, ERR_R_MALLOC_FAILURE);
    goto err;
  }
  if (!EC_POINT_mul(group, point, u1, pub_key, u2, ctx)) {
    OPENSSL_PUT_ERROR(ECDSA, ERR_R_EC_LIB);
    goto err;
  }
  if (!EC_POINT_get_affine_coordinates_GFp(group, point, X, NULL, ctx)) {
    OPENSSL_PUT_ERROR(ECDSA, ERR_R_EC_LIB);
    goto err;
  }
  if (!BN_nnmod(u1, X, order, ctx)) {
    OPENSSL_PUT_ERROR(ECDSA, ERR_R_BN_LIB);
    goto err;
  }
  /* if the signature is correct u1 is equal to sig->r */
  if (BN_ucmp(u1, sig->r) != 0) {
    OPENSSL_PUT_ERROR(ECDSA, ECDSA_R_BAD_SIGNATURE);
    goto err;
  }

  ret = 1;

err:
  BN_CTX_end(ctx);
  BN_CTX_free(ctx);
  EC_POINT_free(point);
  return ret;
}

static int ecdsa_sign_setup(const EC_KEY *eckey, BN_CTX *ctx_in, BIGNUM **kinvp,
                            BIGNUM **rp, const uint8_t *digest,
                            size_t digest_len) {
  BN_CTX *ctx = NULL;
  BIGNUM *k = NULL, *kinv = NULL, *r = NULL, *tmp = NULL;
  EC_POINT *tmp_point = NULL;
  const EC_GROUP *group;
  int ret = 0;

  if (eckey == NULL || (group = EC_KEY_get0_group(eckey)) == NULL) {
    OPENSSL_PUT_ERROR(ECDSA, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  if (ctx_in == NULL) {
    if ((ctx = BN_CTX_new()) == NULL) {
      OPENSSL_PUT_ERROR(ECDSA, ERR_R_MALLOC_FAILURE);
      return 0;
    }
  } else {
    ctx = ctx_in;
  }

  k = BN_new();
  kinv = BN_new(); /* this value is later returned in *kinvp */
  r = BN_new(); /* this value is later returned in *rp    */
  tmp = BN_new();
  if (k == NULL || kinv == NULL || r == NULL || tmp == NULL) {
    OPENSSL_PUT_ERROR(ECDSA, ERR_R_MALLOC_FAILURE);
    goto err;
  }
  tmp_point = EC_POINT_new(group);
  if (tmp_point == NULL) {
    OPENSSL_PUT_ERROR(ECDSA, ERR_R_EC_LIB);
    goto err;
  }

  const BIGNUM *order = EC_GROUP_get0_order(group);

  /* Check that the size of the group order is FIPS compliant (FIPS 186-4
   * B.5.2). */
  if (BN_num_bits(order) < 160) {
    OPENSSL_PUT_ERROR(ECDSA, EC_R_INVALID_GROUP_ORDER);
    goto err;
  }

  do {
    /* If possible, we'll include the private key and message digest in the k
     * generation. The |digest| argument is only empty if |ECDSA_sign_setup| is
     * being used. */
    if (eckey->fixed_k != NULL) {
      if (!BN_copy(k, eckey->fixed_k)) {
        goto err;
      }
    } else if (digest_len > 0) {
      do {
        if (!BN_generate_dsa_nonce(k, order, EC_KEY_get0_private_key(eckey),
                                   digest, digest_len, ctx)) {
          OPENSSL_PUT_ERROR(ECDSA, ECDSA_R_RANDOM_NUMBER_GENERATION_FAILED);
          goto err;
        }
      } while (BN_is_zero(k));
    } else if (!BN_rand_range_ex(k, 1, order)) {
      OPENSSL_PUT_ERROR(ECDSA, ECDSA_R_RANDOM_NUMBER_GENERATION_FAILED);
      goto err;
    }

    /* Compute the inverse of k. The order is a prime, so use Fermat's Little
     * Theorem. Note |ec_group_get_mont_data| may return NULL but
     * |bn_mod_inverse_prime| allows this. */
    if (!bn_mod_inverse_prime(kinv, k, order, ctx,
                              ec_group_get_mont_data(group))) {
      OPENSSL_PUT_ERROR(ECDSA, ERR_R_BN_LIB);
      goto err;
    }

    /* We do not want timing information to leak the length of k,
     * so we compute G*k using an equivalent scalar of fixed
     * bit-length. */

    if (!BN_add(k, k, order)) {
      goto err;
    }
    if (BN_num_bits(k) <= BN_num_bits(order)) {
      if (!BN_add(k, k, order)) {
        goto err;
      }
    }

    /* compute r the x-coordinate of generator * k */
    if (!EC_POINT_mul(group, tmp_point, k, NULL, NULL, ctx)) {
      OPENSSL_PUT_ERROR(ECDSA, ERR_R_EC_LIB);
      goto err;
    }
    if (!EC_POINT_get_affine_coordinates_GFp(group, tmp_point, tmp, NULL,
                                             ctx)) {
      OPENSSL_PUT_ERROR(ECDSA, ERR_R_EC_LIB);
      goto err;
    }

    if (!BN_nnmod(r, tmp, order, ctx)) {
      OPENSSL_PUT_ERROR(ECDSA, ERR_R_BN_LIB);
      goto err;
    }
  } while (BN_is_zero(r));

  /* clear old values if necessary */
  BN_clear_free(*rp);
  BN_clear_free(*kinvp);

  /* save the pre-computed values  */
  *rp = r;
  *kinvp = kinv;
  ret = 1;

err:
  BN_clear_free(k);
  if (!ret) {
    BN_clear_free(kinv);
    BN_clear_free(r);
  }
  if (ctx_in == NULL) {
    BN_CTX_free(ctx);
  }
  EC_POINT_free(tmp_point);
  BN_clear_free(tmp);
  return ret;
}

int ECDSA_sign_setup(const EC_KEY *eckey, BN_CTX *ctx, BIGNUM **kinv,
                     BIGNUM **rp) {
  return ecdsa_sign_setup(eckey, ctx, kinv, rp, NULL, 0);
}

ECDSA_SIG *ECDSA_do_sign_ex(const uint8_t *digest, size_t digest_len,
                            const BIGNUM *in_kinv, const BIGNUM *in_r,
                            const EC_KEY *eckey) {
  int ok = 0;
  BIGNUM *kinv = NULL, *s, *m = NULL, *tmp = NULL;
  const BIGNUM *ckinv;
  BN_CTX *ctx = NULL;
  const EC_GROUP *group;
  ECDSA_SIG *ret;
  const BIGNUM *priv_key;

  if (eckey->ecdsa_meth && eckey->ecdsa_meth->sign) {
    OPENSSL_PUT_ERROR(ECDSA, ECDSA_R_NOT_IMPLEMENTED);
    return NULL;
  }

  group = EC_KEY_get0_group(eckey);
  priv_key = EC_KEY_get0_private_key(eckey);

  if (group == NULL || priv_key == NULL) {
    OPENSSL_PUT_ERROR(ECDSA, ERR_R_PASSED_NULL_PARAMETER);
    return NULL;
  }

  ret = ECDSA_SIG_new();
  if (!ret) {
    OPENSSL_PUT_ERROR(ECDSA, ERR_R_MALLOC_FAILURE);
    return NULL;
  }
  s = ret->s;

  if ((ctx = BN_CTX_new()) == NULL ||
      (tmp = BN_new()) == NULL ||
      (m = BN_new()) == NULL) {
    OPENSSL_PUT_ERROR(ECDSA, ERR_R_MALLOC_FAILURE);
    goto err;
  }

  const BIGNUM *order = EC_GROUP_get0_order(group);

  if (!digest_to_bn(m, digest, digest_len, order)) {
    goto err;
  }
  for (;;) {
    if (in_kinv == NULL || in_r == NULL) {
      if (!ecdsa_sign_setup(eckey, ctx, &kinv, &ret->r, digest, digest_len)) {
        OPENSSL_PUT_ERROR(ECDSA, ERR_R_ECDSA_LIB);
        goto err;
      }
      ckinv = kinv;
    } else {
      ckinv = in_kinv;
      if (BN_copy(ret->r, in_r) == NULL) {
        OPENSSL_PUT_ERROR(ECDSA, ERR_R_MALLOC_FAILURE);
        goto err;
      }
    }

    if (!BN_mod_mul(tmp, priv_key, ret->r, order, ctx)) {
      OPENSSL_PUT_ERROR(ECDSA, ERR_R_BN_LIB);
      goto err;
    }
    if (!BN_mod_add_quick(s, tmp, m, order)) {
      OPENSSL_PUT_ERROR(ECDSA, ERR_R_BN_LIB);
      goto err;
    }
    if (!BN_mod_mul(s, s, ckinv, order, ctx)) {
      OPENSSL_PUT_ERROR(ECDSA, ERR_R_BN_LIB);
      goto err;
    }
    if (BN_is_zero(s)) {
      /* if kinv and r have been supplied by the caller
       * don't to generate new kinv and r values */
      if (in_kinv != NULL && in_r != NULL) {
        OPENSSL_PUT_ERROR(ECDSA, ECDSA_R_NEED_NEW_SETUP_VALUES);
        goto err;
      }
    } else {
      /* s != 0 => we have a valid signature */
      break;
    }
  }

  ok = 1;

err:
  if (!ok) {
    ECDSA_SIG_free(ret);
    ret = NULL;
  }
  BN_CTX_free(ctx);
  BN_clear_free(m);
  BN_clear_free(tmp);
  BN_clear_free(kinv);
  return ret;
}
