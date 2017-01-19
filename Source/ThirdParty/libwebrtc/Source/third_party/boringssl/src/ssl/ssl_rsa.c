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

#include <openssl/ssl.h>

#include <limits.h>

#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/type_check.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "internal.h"


static int ssl_set_cert(CERT *c, X509 *x509);
static int ssl_set_pkey(CERT *c, EVP_PKEY *pkey);

static int is_key_type_supported(int key_type) {
  return key_type == EVP_PKEY_RSA || key_type == EVP_PKEY_EC;
}

int SSL_use_certificate(SSL *ssl, X509 *x) {
  if (x == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }
  return ssl_set_cert(ssl->cert, x);
}

int SSL_use_certificate_ASN1(SSL *ssl, const uint8_t *der, size_t der_len) {
  if (der_len > LONG_MAX) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
    return 0;
  }

  const uint8_t *p = der;
  X509 *x509 = d2i_X509(NULL, &p, (long)der_len);
  if (x509 == NULL || p != der + der_len) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_ASN1_LIB);
    X509_free(x509);
    return 0;
  }

  int ret = SSL_use_certificate(ssl, x509);
  X509_free(x509);
  return ret;
}

int SSL_use_RSAPrivateKey(SSL *ssl, RSA *rsa) {
  EVP_PKEY *pkey;
  int ret;

  if (rsa == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  pkey = EVP_PKEY_new();
  if (pkey == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_EVP_LIB);
    return 0;
  }

  RSA_up_ref(rsa);
  EVP_PKEY_assign_RSA(pkey, rsa);

  ret = ssl_set_pkey(ssl->cert, pkey);
  EVP_PKEY_free(pkey);

  return ret;
}

static int ssl_set_pkey(CERT *c, EVP_PKEY *pkey) {
  if (!is_key_type_supported(pkey->type)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNKNOWN_CERTIFICATE_TYPE);
    return 0;
  }

  if (c->x509 != NULL) {
    /* Sanity-check that the private key and the certificate match, unless the
     * key is opaque (in case of, say, a smartcard). */
    if (!EVP_PKEY_is_opaque(pkey) &&
        !X509_check_private_key(c->x509, pkey)) {
      X509_free(c->x509);
      c->x509 = NULL;
      return 0;
    }
  }

  EVP_PKEY_free(c->privatekey);
  EVP_PKEY_up_ref(pkey);
  c->privatekey = pkey;

  return 1;
}

int SSL_use_RSAPrivateKey_ASN1(SSL *ssl, const uint8_t *der, size_t der_len) {
  RSA *rsa = RSA_private_key_from_bytes(der, der_len);
  if (rsa == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_ASN1_LIB);
    return 0;
  }

  int ret = SSL_use_RSAPrivateKey(ssl, rsa);
  RSA_free(rsa);
  return ret;
}

int SSL_use_PrivateKey(SSL *ssl, EVP_PKEY *pkey) {
  int ret;

  if (pkey == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  ret = ssl_set_pkey(ssl->cert, pkey);
  return ret;
}

int SSL_use_PrivateKey_ASN1(int type, SSL *ssl, const uint8_t *der,
                            size_t der_len) {
  if (der_len > LONG_MAX) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
    return 0;
  }

  const uint8_t *p = der;
  EVP_PKEY *pkey = d2i_PrivateKey(type, NULL, &p, (long)der_len);
  if (pkey == NULL || p != der + der_len) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_ASN1_LIB);
    EVP_PKEY_free(pkey);
    return 0;
  }

  int ret = SSL_use_PrivateKey(ssl, pkey);
  EVP_PKEY_free(pkey);
  return ret;
}

int SSL_CTX_use_certificate(SSL_CTX *ctx, X509 *x) {
  if (x == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  return ssl_set_cert(ctx->cert, x);
}

static int ssl_set_cert(CERT *c, X509 *x) {
  EVP_PKEY *pkey = X509_get_pubkey(x);
  if (pkey == NULL) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_X509_LIB);
    return 0;
  }

  if (!is_key_type_supported(pkey->type)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNKNOWN_CERTIFICATE_TYPE);
    EVP_PKEY_free(pkey);
    return 0;
  }

  /* An ECC certificate may be usable for ECDH or ECDSA. We only support ECDSA
   * certificates, so sanity-check the key usage extension. */
  if (pkey->type == EVP_PKEY_EC) {
    /* This call populates extension flags (ex_flags). */
    X509_check_purpose(x, -1, 0);
    if ((x->ex_flags & EXFLAG_KUSAGE) &&
        !(x->ex_kusage & X509v3_KU_DIGITAL_SIGNATURE)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_UNKNOWN_CERTIFICATE_TYPE);
      EVP_PKEY_free(pkey);
      return 0;
    }
  }

  if (c->privatekey != NULL) {
    /* Sanity-check that the private key and the certificate match, unless the
     * key is opaque (in case of, say, a smartcard). */
    if (!EVP_PKEY_is_opaque(c->privatekey) &&
        !X509_check_private_key(x, c->privatekey)) {
      /* don't fail for a cert/key mismatch, just free current private key
       * (when switching to a different cert & key, first this function should
       * be used, then ssl_set_pkey */
      EVP_PKEY_free(c->privatekey);
      c->privatekey = NULL;
      /* clear error queue */
      ERR_clear_error();
    }
  }

  EVP_PKEY_free(pkey);

  X509_free(c->x509);
  X509_up_ref(x);
  c->x509 = x;

  return 1;
}

int SSL_CTX_use_certificate_ASN1(SSL_CTX *ctx, size_t der_len,
                                 const uint8_t *der) {
  if (der_len > LONG_MAX) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
    return 0;
  }

  const uint8_t *p = der;
  X509 *x509 = d2i_X509(NULL, &p, (long)der_len);
  if (x509 == NULL || p != der + der_len) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_ASN1_LIB);
    X509_free(x509);
    return 0;
  }

  int ret = SSL_CTX_use_certificate(ctx, x509);
  X509_free(x509);
  return ret;
}

int SSL_CTX_use_RSAPrivateKey(SSL_CTX *ctx, RSA *rsa) {
  int ret;
  EVP_PKEY *pkey;

  if (rsa == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  pkey = EVP_PKEY_new();
  if (pkey == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_EVP_LIB);
    return 0;
  }

  RSA_up_ref(rsa);
  EVP_PKEY_assign_RSA(pkey, rsa);

  ret = ssl_set_pkey(ctx->cert, pkey);
  EVP_PKEY_free(pkey);
  return ret;
}

int SSL_CTX_use_RSAPrivateKey_ASN1(SSL_CTX *ctx, const uint8_t *der,
                                   size_t der_len) {
  RSA *rsa = RSA_private_key_from_bytes(der, der_len);
  if (rsa == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_ASN1_LIB);
    return 0;
  }

  int ret = SSL_CTX_use_RSAPrivateKey(ctx, rsa);
  RSA_free(rsa);
  return ret;
}

int SSL_CTX_use_PrivateKey(SSL_CTX *ctx, EVP_PKEY *pkey) {
  if (pkey == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  return ssl_set_pkey(ctx->cert, pkey);
}

int SSL_CTX_use_PrivateKey_ASN1(int type, SSL_CTX *ctx, const uint8_t *der,
                                size_t der_len) {
  if (der_len > LONG_MAX) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
    return 0;
  }

  const uint8_t *p = der;
  EVP_PKEY *pkey = d2i_PrivateKey(type, NULL, &p, (long)der_len);
  if (pkey == NULL || p != der + der_len) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_ASN1_LIB);
    EVP_PKEY_free(pkey);
    return 0;
  }

  int ret = SSL_CTX_use_PrivateKey(ctx, pkey);
  EVP_PKEY_free(pkey);
  return ret;
}

void SSL_set_private_key_method(SSL *ssl,
                                const SSL_PRIVATE_KEY_METHOD *key_method) {
  ssl->cert->key_method = key_method;
}

void SSL_CTX_set_private_key_method(SSL_CTX *ctx,
                                    const SSL_PRIVATE_KEY_METHOD *key_method) {
  ctx->cert->key_method = key_method;
}

static int set_signing_algorithm_prefs(CERT *cert, const uint16_t *prefs,
                                       size_t num_prefs) {
  OPENSSL_free(cert->sigalgs);

  cert->num_sigalgs = 0;
  cert->sigalgs = BUF_memdup(prefs, num_prefs * sizeof(prefs[0]));
  if (cert->sigalgs == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return 0;
  }
  cert->num_sigalgs = num_prefs;

  return 1;
}

int SSL_CTX_set_signing_algorithm_prefs(SSL_CTX *ctx, const uint16_t *prefs,
                                        size_t num_prefs) {
  return set_signing_algorithm_prefs(ctx->cert, prefs, num_prefs);
}


int SSL_set_signing_algorithm_prefs(SSL *ssl, const uint16_t *prefs,
                                    size_t num_prefs) {
  return set_signing_algorithm_prefs(ssl->cert, prefs, num_prefs);
}

OPENSSL_COMPILE_ASSERT(sizeof(int) >= 2 * sizeof(uint16_t),
                       digest_list_conversion_cannot_overflow);

int SSL_set_private_key_digest_prefs(SSL *ssl, const int *digest_nids,
                                     size_t num_digests) {
  OPENSSL_free(ssl->cert->sigalgs);

  ssl->cert->num_sigalgs = 0;
  ssl->cert->sigalgs = OPENSSL_malloc(sizeof(uint16_t) * 2 * num_digests);
  if (ssl->cert->sigalgs == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return 0;
  }

  /* Convert the digest list to a signature algorithms list.
   *
   * TODO(davidben): Replace this API with one that can express RSA-PSS, etc. */
  for (size_t i = 0; i < num_digests; i++) {
    switch (digest_nids[i]) {
      case NID_sha1:
        ssl->cert->sigalgs[ssl->cert->num_sigalgs] = SSL_SIGN_RSA_PKCS1_SHA1;
        ssl->cert->sigalgs[ssl->cert->num_sigalgs + 1] = SSL_SIGN_ECDSA_SHA1;
        ssl->cert->num_sigalgs += 2;
        break;
      case NID_sha256:
        ssl->cert->sigalgs[ssl->cert->num_sigalgs] = SSL_SIGN_RSA_PKCS1_SHA256;
        ssl->cert->sigalgs[ssl->cert->num_sigalgs + 1] =
            SSL_SIGN_ECDSA_SECP256R1_SHA256;
        ssl->cert->num_sigalgs += 2;
        break;
      case NID_sha384:
        ssl->cert->sigalgs[ssl->cert->num_sigalgs] = SSL_SIGN_RSA_PKCS1_SHA384;
        ssl->cert->sigalgs[ssl->cert->num_sigalgs + 1] =
            SSL_SIGN_ECDSA_SECP384R1_SHA384;
        ssl->cert->num_sigalgs += 2;
        break;
      case NID_sha512:
        ssl->cert->sigalgs[ssl->cert->num_sigalgs] = SSL_SIGN_RSA_PKCS1_SHA512;
        ssl->cert->sigalgs[ssl->cert->num_sigalgs + 1] =
            SSL_SIGN_ECDSA_SECP521R1_SHA512;
        ssl->cert->num_sigalgs += 2;
        break;
    }
  }

  return 1;
}

int ssl_has_private_key(const SSL *ssl) {
  return ssl->cert->privatekey != NULL || ssl->cert->key_method != NULL;
}

int ssl_is_ecdsa_key_type(int type) {
  switch (type) {
    /* TODO(davidben): Remove support for |EVP_PKEY_EC| key types. */
    case EVP_PKEY_EC:
    case NID_X9_62_prime256v1:
    case NID_secp384r1:
    case NID_secp521r1:
      return 1;
    default:
      return 0;
  }
}

int ssl_private_key_type(SSL *ssl) {
  if (ssl->cert->key_method != NULL) {
    return ssl->cert->key_method->type(ssl);
  }
  switch (EVP_PKEY_id(ssl->cert->privatekey)) {
    case EVP_PKEY_RSA:
      return NID_rsaEncryption;
    case EVP_PKEY_EC:
      return EC_GROUP_get_curve_name(
          EC_KEY_get0_group(EVP_PKEY_get0_EC_KEY(ssl->cert->privatekey)));
    default:
      return NID_undef;
  }
}

size_t ssl_private_key_max_signature_len(SSL *ssl) {
  if (ssl->cert->key_method != NULL) {
    return ssl->cert->key_method->max_signature_len(ssl);
  }
  return EVP_PKEY_size(ssl->cert->privatekey);
}

/* TODO(davidben): Forbid RSA-PKCS1 in TLS 1.3. For now we allow it because NSS
 * has yet to start doing RSA-PSS, so enforcing it would complicate interop
 * testing. */
static int is_rsa_pkcs1(const EVP_MD **out_md, uint16_t sigalg) {
  switch (sigalg) {
    case SSL_SIGN_RSA_PKCS1_MD5_SHA1:
      *out_md = EVP_md5_sha1();
      return 1;
    case SSL_SIGN_RSA_PKCS1_SHA1:
      *out_md = EVP_sha1();
      return 1;
    case SSL_SIGN_RSA_PKCS1_SHA256:
      *out_md = EVP_sha256();
      return 1;
    case SSL_SIGN_RSA_PKCS1_SHA384:
      *out_md = EVP_sha384();
      return 1;
    case SSL_SIGN_RSA_PKCS1_SHA512:
      *out_md = EVP_sha512();
      return 1;
    default:
      return 0;
  }
}

static int ssl_sign_rsa_pkcs1(SSL *ssl, uint8_t *out, size_t *out_len,
                              size_t max_out, const EVP_MD *md,
                              const uint8_t *in, size_t in_len) {
  EVP_MD_CTX ctx;
  EVP_MD_CTX_init(&ctx);
  *out_len = max_out;
  int ret = EVP_DigestSignInit(&ctx, NULL, md, NULL, ssl->cert->privatekey) &&
            EVP_DigestSignUpdate(&ctx, in, in_len) &&
            EVP_DigestSignFinal(&ctx, out, out_len);
  EVP_MD_CTX_cleanup(&ctx);
  return ret;
}

static int ssl_verify_rsa_pkcs1(SSL *ssl, const uint8_t *signature,
                                size_t signature_len, const EVP_MD *md,
                                EVP_PKEY *pkey, const uint8_t *in,
                                size_t in_len) {
  if (pkey->type != EVP_PKEY_RSA) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_SIGNATURE_TYPE);
    return 0;
  }

  EVP_MD_CTX md_ctx;
  EVP_MD_CTX_init(&md_ctx);
  int ret = EVP_DigestVerifyInit(&md_ctx, NULL, md, NULL, pkey) &&
            EVP_DigestVerifyUpdate(&md_ctx, in, in_len) &&
            EVP_DigestVerifyFinal(&md_ctx, signature, signature_len);
  EVP_MD_CTX_cleanup(&md_ctx);
  return ret;
}

static int is_ecdsa(int *out_curve, const EVP_MD **out_md, uint16_t sigalg) {
  switch (sigalg) {
    case SSL_SIGN_ECDSA_SHA1:
      *out_curve = NID_undef;
      *out_md = EVP_sha1();
      return 1;
    case SSL_SIGN_ECDSA_SECP256R1_SHA256:
      *out_curve = NID_X9_62_prime256v1;
      *out_md = EVP_sha256();
      return 1;
    case SSL_SIGN_ECDSA_SECP384R1_SHA384:
      *out_curve = NID_secp384r1;
      *out_md = EVP_sha384();
      return 1;
    case SSL_SIGN_ECDSA_SECP521R1_SHA512:
      *out_curve = NID_secp521r1;
      *out_md = EVP_sha512();
      return 1;
    default:
      return 0;
  }
}

static int ssl_sign_ecdsa(SSL *ssl, uint8_t *out, size_t *out_len,
                          size_t max_out, int curve, const EVP_MD *md,
                          const uint8_t *in, size_t in_len) {
  EC_KEY *ec_key = EVP_PKEY_get0_EC_KEY(ssl->cert->privatekey);
  if (ec_key == NULL) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_SIGNATURE_TYPE);
    return 0;
  }

  /* In TLS 1.3, the curve is also specified by the signature algorithm. */
  if (ssl3_protocol_version(ssl) >= TLS1_3_VERSION &&
      (curve == NID_undef ||
       EC_GROUP_get_curve_name(EC_KEY_get0_group(ec_key)) != curve)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_SIGNATURE_TYPE);
    return 0;
  }

  EVP_MD_CTX ctx;
  EVP_MD_CTX_init(&ctx);
  *out_len = max_out;
  int ret = EVP_DigestSignInit(&ctx, NULL, md, NULL, ssl->cert->privatekey) &&
            EVP_DigestSignUpdate(&ctx, in, in_len) &&
            EVP_DigestSignFinal(&ctx, out, out_len);
  EVP_MD_CTX_cleanup(&ctx);
  return ret;
}

static int ssl_verify_ecdsa(SSL *ssl, const uint8_t *signature,
                            size_t signature_len, int curve, const EVP_MD *md,
                            EVP_PKEY *pkey, const uint8_t *in, size_t in_len) {
  EC_KEY *ec_key = EVP_PKEY_get0_EC_KEY(pkey);
  if (ec_key == NULL) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_SIGNATURE_TYPE);
    return 0;
  }

  /* In TLS 1.3, the curve is also specified by the signature algorithm. */
  if (ssl3_protocol_version(ssl) >= TLS1_3_VERSION &&
      (curve == NID_undef ||
       EC_GROUP_get_curve_name(EC_KEY_get0_group(ec_key)) != curve)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_SIGNATURE_TYPE);
    return 0;
  }

  EVP_MD_CTX md_ctx;
  EVP_MD_CTX_init(&md_ctx);
  int ret = EVP_DigestVerifyInit(&md_ctx, NULL, md, NULL, pkey) &&
            EVP_DigestVerifyUpdate(&md_ctx, in, in_len) &&
            EVP_DigestVerifyFinal(&md_ctx, signature, signature_len);
  EVP_MD_CTX_cleanup(&md_ctx);
  return ret;
}

static int is_rsa_pss(const EVP_MD **out_md, uint16_t sigalg) {
  switch (sigalg) {
    case SSL_SIGN_RSA_PSS_SHA256:
      *out_md = EVP_sha256();
      return 1;
    case SSL_SIGN_RSA_PSS_SHA384:
      *out_md = EVP_sha384();
      return 1;
    case SSL_SIGN_RSA_PSS_SHA512:
      *out_md = EVP_sha512();
      return 1;
    default:
      return 0;
  }
}

static int ssl_sign_rsa_pss(SSL *ssl, uint8_t *out, size_t *out_len,
                              size_t max_out, const EVP_MD *md,
                              const uint8_t *in, size_t in_len) {
  EVP_MD_CTX ctx;
  EVP_MD_CTX_init(&ctx);
  *out_len = max_out;
  EVP_PKEY_CTX *pctx;
  int ret =
      EVP_DigestSignInit(&ctx, &pctx, md, NULL, ssl->cert->privatekey) &&
      EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING) &&
      EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, -1 /* salt len = hash len */) &&
      EVP_DigestSignUpdate(&ctx, in, in_len) &&
      EVP_DigestSignFinal(&ctx, out, out_len);
  EVP_MD_CTX_cleanup(&ctx);
  return ret;
}

static int ssl_verify_rsa_pss(SSL *ssl, const uint8_t *signature,
                                size_t signature_len, const EVP_MD *md,
                                EVP_PKEY *pkey, const uint8_t *in,
                                size_t in_len) {
  if (pkey->type != EVP_PKEY_RSA) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_SIGNATURE_TYPE);
    return 0;
  }

  EVP_MD_CTX md_ctx;
  EVP_MD_CTX_init(&md_ctx);
  EVP_PKEY_CTX *pctx;
  int ret =
      EVP_DigestVerifyInit(&md_ctx, &pctx, md, NULL, pkey) &&
      EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING) &&
      EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, -1 /* salt len = hash len */) &&
      EVP_DigestVerifyUpdate(&md_ctx, in, in_len) &&
      EVP_DigestVerifyFinal(&md_ctx, signature, signature_len);
  EVP_MD_CTX_cleanup(&md_ctx);
  return ret;
}

enum ssl_private_key_result_t ssl_private_key_sign(
    SSL *ssl, uint8_t *out, size_t *out_len, size_t max_out,
    uint16_t signature_algorithm, const uint8_t *in, size_t in_len) {
  if (ssl->cert->key_method != NULL) {
    if (ssl->cert->key_method->sign != NULL) {
      return ssl->cert->key_method->sign(ssl, out, out_len, max_out,
                                         signature_algorithm, in, in_len);
    }

    /* TODO(davidben): Remove support for |sign_digest|-only
     * |SSL_PRIVATE_KEY_METHOD|s. */
    const EVP_MD *md;
    int curve;
    if (!is_rsa_pkcs1(&md, signature_algorithm) &&
        !is_ecdsa(&curve, &md, signature_algorithm)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_UNSUPPORTED_PROTOCOL_FOR_CUSTOM_KEY);
      return ssl_private_key_failure;
    }

    uint8_t hash[EVP_MAX_MD_SIZE];
    unsigned hash_len;
    if (!EVP_Digest(in, in_len, hash, &hash_len, md, NULL)) {
      return ssl_private_key_failure;
    }

    return ssl->cert->key_method->sign_digest(ssl, out, out_len, max_out, md,
                                              hash, hash_len);
  }

  const EVP_MD *md;
  if (is_rsa_pkcs1(&md, signature_algorithm) &&
      ssl3_protocol_version(ssl) < TLS1_3_VERSION) {
    return ssl_sign_rsa_pkcs1(ssl, out, out_len, max_out, md, in, in_len)
               ? ssl_private_key_success
               : ssl_private_key_failure;
  }

  int curve;
  if (is_ecdsa(&curve, &md, signature_algorithm)) {
    return ssl_sign_ecdsa(ssl, out, out_len, max_out, curve, md, in, in_len)
               ? ssl_private_key_success
               : ssl_private_key_failure;
  }

  if (is_rsa_pss(&md, signature_algorithm)) {
    return ssl_sign_rsa_pss(ssl, out, out_len, max_out, md, in, in_len)
               ? ssl_private_key_success
               : ssl_private_key_failure;
  }

  OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_SIGNATURE_TYPE);
  return ssl_private_key_failure;
}

int ssl_public_key_verify(SSL *ssl, const uint8_t *signature,
                          size_t signature_len, uint16_t signature_algorithm,
                          EVP_PKEY *pkey, const uint8_t *in, size_t in_len) {
  const EVP_MD *md;
  if (is_rsa_pkcs1(&md, signature_algorithm) &&
      ssl3_protocol_version(ssl) < TLS1_3_VERSION) {
    return ssl_verify_rsa_pkcs1(ssl, signature, signature_len, md, pkey, in,
                                in_len);
  }

  int curve;
  if (is_ecdsa(&curve, &md, signature_algorithm)) {
    return ssl_verify_ecdsa(ssl, signature, signature_len, curve, md, pkey, in,
                            in_len);
  }

  if (is_rsa_pss(&md, signature_algorithm)) {
    return ssl_verify_rsa_pss(ssl, signature, signature_len, md, pkey, in,
                              in_len);
  }

  OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_SIGNATURE_TYPE);
  return 0;
}

enum ssl_private_key_result_t ssl_private_key_decrypt(
    SSL *ssl, uint8_t *out, size_t *out_len, size_t max_out,
    const uint8_t *in, size_t in_len) {
  if (ssl->cert->key_method != NULL) {
    return ssl->cert->key_method->decrypt(ssl, out, out_len, max_out, in,
                                          in_len);
  }

  RSA *rsa = EVP_PKEY_get0_RSA(ssl->cert->privatekey);
  if (rsa == NULL) {
    /* Decrypt operations are only supported for RSA keys. */
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return ssl_private_key_failure;
  }

  /* Decrypt with no padding. PKCS#1 padding will be removed as part
   * of the timing-sensitive code by the caller. */
  if (!RSA_decrypt(rsa, out_len, out, max_out, in, in_len, RSA_NO_PADDING)) {
    return ssl_private_key_failure;
  }
  return ssl_private_key_success;
}

enum ssl_private_key_result_t ssl_private_key_complete(SSL *ssl, uint8_t *out,
                                                       size_t *out_len,
                                                       size_t max_out) {
  /* Only custom keys may be asynchronous. */
  return ssl->cert->key_method->complete(ssl, out, out_len, max_out);
}

int ssl_private_key_supports_signature_algorithm(SSL *ssl,
                                                 uint16_t signature_algorithm) {
  const EVP_MD *md;
  if (is_rsa_pkcs1(&md, signature_algorithm) &&
      ssl3_protocol_version(ssl) < TLS1_3_VERSION) {
    return ssl_private_key_type(ssl) == NID_rsaEncryption;
  }

  int curve;
  if (is_ecdsa(&curve, &md, signature_algorithm)) {
    int type = ssl_private_key_type(ssl);
    if (!ssl_is_ecdsa_key_type(type)) {
      return 0;
    }

    /* Prior to TLS 1.3, ECDSA curves did not match the signature algorithm. */
    if (ssl3_protocol_version(ssl) < TLS1_3_VERSION) {
      return 1;
    }

    return curve != NID_undef && type == curve;
  }

  if (is_rsa_pss(&md, signature_algorithm)) {
    if (ssl_private_key_type(ssl) != NID_rsaEncryption) {
      return 0;
    }

    /* Ensure the RSA key is large enough for the hash. RSASSA-PSS requires that
     * emLen be at least hLen + sLen + 2. Both hLen and sLen are the size of the
     * hash in TLS. Reasonable RSA key sizes are large enough for the largest
     * defined RSASSA-PSS algorithm, but 1024-bit RSA is slightly too large for
     * SHA-512. 1024-bit RSA is sometimes used for test credentials, so check
     * the size to fall back to another algorithm. */
    if (ssl_private_key_max_signature_len(ssl) < 2 * EVP_MD_size(md) + 2) {
      return 0;
    }

    /* RSA-PSS is only supported by message-based private keys. */
    if (ssl->cert->key_method != NULL && ssl->cert->key_method->sign == NULL) {
      return 0;
    }

    return 1;
  }

  return 0;
}
