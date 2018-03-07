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

#include <assert.h>
#include <limits.h>

#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>

#include "internal.h"
#include "../crypto/internal.h"


namespace bssl {

int ssl_is_key_type_supported(int key_type) {
  return key_type == EVP_PKEY_RSA || key_type == EVP_PKEY_EC ||
         key_type == EVP_PKEY_ED25519;
}

static int ssl_set_pkey(CERT *cert, EVP_PKEY *pkey) {
  if (!ssl_is_key_type_supported(pkey->type)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNKNOWN_CERTIFICATE_TYPE);
    return 0;
  }

  if (cert->chain != NULL &&
      sk_CRYPTO_BUFFER_value(cert->chain, 0) != NULL &&
      // Sanity-check that the private key and the certificate match.
      !ssl_cert_check_private_key(cert, pkey)) {
    return 0;
  }

  EVP_PKEY_free(cert->privatekey);
  EVP_PKEY_up_ref(pkey);
  cert->privatekey = pkey;

  return 1;
}

typedef struct {
  uint16_t sigalg;
  int pkey_type;
  int curve;
  const EVP_MD *(*digest_func)(void);
  char is_rsa_pss;
} SSL_SIGNATURE_ALGORITHM;

static const SSL_SIGNATURE_ALGORITHM kSignatureAlgorithms[] = {
    {SSL_SIGN_RSA_PKCS1_MD5_SHA1, EVP_PKEY_RSA, NID_undef, &EVP_md5_sha1, 0},
    {SSL_SIGN_RSA_PKCS1_SHA1, EVP_PKEY_RSA, NID_undef, &EVP_sha1, 0},
    {SSL_SIGN_RSA_PKCS1_SHA256, EVP_PKEY_RSA, NID_undef, &EVP_sha256, 0},
    {SSL_SIGN_RSA_PKCS1_SHA384, EVP_PKEY_RSA, NID_undef, &EVP_sha384, 0},
    {SSL_SIGN_RSA_PKCS1_SHA512, EVP_PKEY_RSA, NID_undef, &EVP_sha512, 0},

    {SSL_SIGN_RSA_PSS_SHA256, EVP_PKEY_RSA, NID_undef, &EVP_sha256, 1},
    {SSL_SIGN_RSA_PSS_SHA384, EVP_PKEY_RSA, NID_undef, &EVP_sha384, 1},
    {SSL_SIGN_RSA_PSS_SHA512, EVP_PKEY_RSA, NID_undef, &EVP_sha512, 1},

    {SSL_SIGN_ECDSA_SHA1, EVP_PKEY_EC, NID_undef, &EVP_sha1, 0},
    {SSL_SIGN_ECDSA_SECP256R1_SHA256, EVP_PKEY_EC, NID_X9_62_prime256v1,
     &EVP_sha256, 0},
    {SSL_SIGN_ECDSA_SECP384R1_SHA384, EVP_PKEY_EC, NID_secp384r1, &EVP_sha384,
     0},
    {SSL_SIGN_ECDSA_SECP521R1_SHA512, EVP_PKEY_EC, NID_secp521r1, &EVP_sha512,
     0},

    {SSL_SIGN_ED25519, EVP_PKEY_ED25519, NID_undef, NULL, 0},
};

static const SSL_SIGNATURE_ALGORITHM *get_signature_algorithm(uint16_t sigalg) {
  for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(kSignatureAlgorithms); i++) {
    if (kSignatureAlgorithms[i].sigalg == sigalg) {
      return &kSignatureAlgorithms[i];
    }
  }
  return NULL;
}

int ssl_has_private_key(const SSL *ssl) {
  return ssl->cert->privatekey != NULL || ssl->cert->key_method != NULL;
}

static int pkey_supports_algorithm(const SSL *ssl, EVP_PKEY *pkey,
                                   uint16_t sigalg) {
  const SSL_SIGNATURE_ALGORITHM *alg = get_signature_algorithm(sigalg);
  if (alg == NULL ||
      EVP_PKEY_id(pkey) != alg->pkey_type) {
    return 0;
  }

  if (ssl_protocol_version(ssl) >= TLS1_3_VERSION) {
    // RSA keys may only be used with RSA-PSS.
    if (alg->pkey_type == EVP_PKEY_RSA && !alg->is_rsa_pss) {
      return 0;
    }

    // EC keys have a curve requirement.
    if (alg->pkey_type == EVP_PKEY_EC &&
        (alg->curve == NID_undef ||
         EC_GROUP_get_curve_name(
             EC_KEY_get0_group(EVP_PKEY_get0_EC_KEY(pkey))) != alg->curve)) {
      return 0;
    }
  }

  return 1;
}

static int setup_ctx(SSL *ssl, EVP_MD_CTX *ctx, EVP_PKEY *pkey, uint16_t sigalg,
                     int is_verify) {
  if (!pkey_supports_algorithm(ssl, pkey, sigalg)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_SIGNATURE_TYPE);
    return 0;
  }

  const SSL_SIGNATURE_ALGORITHM *alg = get_signature_algorithm(sigalg);
  const EVP_MD *digest = alg->digest_func != NULL ? alg->digest_func() : NULL;
  EVP_PKEY_CTX *pctx;
  if (is_verify) {
    if (!EVP_DigestVerifyInit(ctx, &pctx, digest, NULL, pkey)) {
      return 0;
    }
  } else if (!EVP_DigestSignInit(ctx, &pctx, digest, NULL, pkey)) {
    return 0;
  }

  if (alg->is_rsa_pss) {
    if (!EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING) ||
        !EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, -1 /* salt len = hash len */)) {
      return 0;
    }
  }

  return 1;
}

enum ssl_private_key_result_t ssl_private_key_sign(
    SSL_HANDSHAKE *hs, uint8_t *out, size_t *out_len, size_t max_out,
    uint16_t sigalg, Span<const uint8_t> in) {
  SSL *const ssl = hs->ssl;
  if (ssl->cert->key_method != NULL) {
    enum ssl_private_key_result_t ret;
    if (hs->pending_private_key_op) {
      ret = ssl->cert->key_method->complete(ssl, out, out_len, max_out);
    } else {
      ret = ssl->cert->key_method->sign(ssl, out, out_len, max_out, sigalg,
                                        in.data(), in.size());
    }
    hs->pending_private_key_op = ret == ssl_private_key_retry;
    return ret;
  }

  *out_len = max_out;
  ScopedEVP_MD_CTX ctx;
  if (!setup_ctx(ssl, ctx.get(), ssl->cert->privatekey, sigalg, 0 /* sign */) ||
      !EVP_DigestSign(ctx.get(), out, out_len, in.data(), in.size())) {
    return ssl_private_key_failure;
  }
  return ssl_private_key_success;
}

bool ssl_public_key_verify(SSL *ssl, Span<const uint8_t> signature,
                           uint16_t sigalg, EVP_PKEY *pkey,
                           Span<const uint8_t> in) {
  ScopedEVP_MD_CTX ctx;
  return setup_ctx(ssl, ctx.get(), pkey, sigalg, 1 /* verify */) &&
         EVP_DigestVerify(ctx.get(), signature.data(), signature.size(),
                          in.data(), in.size());
}

enum ssl_private_key_result_t ssl_private_key_decrypt(SSL_HANDSHAKE *hs,
                                                      uint8_t *out,
                                                      size_t *out_len,
                                                      size_t max_out,
                                                      Span<const uint8_t> in) {
  SSL *const ssl = hs->ssl;
  if (ssl->cert->key_method != NULL) {
    enum ssl_private_key_result_t ret;
    if (hs->pending_private_key_op) {
      ret = ssl->cert->key_method->complete(ssl, out, out_len, max_out);
    } else {
      ret = ssl->cert->key_method->decrypt(ssl, out, out_len, max_out,
                                           in.data(), in.size());
    }
    hs->pending_private_key_op = ret == ssl_private_key_retry;
    return ret;
  }

  RSA *rsa = EVP_PKEY_get0_RSA(ssl->cert->privatekey);
  if (rsa == NULL) {
    // Decrypt operations are only supported for RSA keys.
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return ssl_private_key_failure;
  }

  // Decrypt with no padding. PKCS#1 padding will be removed as part of the
  // timing-sensitive code by the caller.
  if (!RSA_decrypt(rsa, out_len, out, max_out, in.data(), in.size(),
                   RSA_NO_PADDING)) {
    return ssl_private_key_failure;
  }
  return ssl_private_key_success;
}

bool ssl_private_key_supports_signature_algorithm(SSL_HANDSHAKE *hs,
                                                  uint16_t sigalg) {
  SSL *const ssl = hs->ssl;
  if (!pkey_supports_algorithm(ssl, hs->local_pubkey.get(), sigalg)) {
    return false;
  }

  // Ensure the RSA key is large enough for the hash. RSASSA-PSS requires that
  // emLen be at least hLen + sLen + 2. Both hLen and sLen are the size of the
  // hash in TLS. Reasonable RSA key sizes are large enough for the largest
  // defined RSASSA-PSS algorithm, but 1024-bit RSA is slightly too small for
  // SHA-512. 1024-bit RSA is sometimes used for test credentials, so check the
  // size so that we can fall back to another algorithm in that case.
  const SSL_SIGNATURE_ALGORITHM *alg = get_signature_algorithm(sigalg);
  if (alg->is_rsa_pss && (size_t)EVP_PKEY_size(hs->local_pubkey.get()) <
                             2 * EVP_MD_size(alg->digest_func()) + 2) {
    return false;
  }

  return true;
}

}  // namespace bssl

using namespace bssl;

int SSL_use_RSAPrivateKey(SSL *ssl, RSA *rsa) {
  if (rsa == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  if (!pkey ||
      !EVP_PKEY_set1_RSA(pkey.get(), rsa)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_EVP_LIB);
    return 0;
  }

  return ssl_set_pkey(ssl->cert, pkey.get());
}

int SSL_use_RSAPrivateKey_ASN1(SSL *ssl, const uint8_t *der, size_t der_len) {
  UniquePtr<RSA> rsa(RSA_private_key_from_bytes(der, der_len));
  if (!rsa) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_ASN1_LIB);
    return 0;
  }

  return SSL_use_RSAPrivateKey(ssl, rsa.get());
}

int SSL_use_PrivateKey(SSL *ssl, EVP_PKEY *pkey) {
  if (pkey == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  return ssl_set_pkey(ssl->cert, pkey);
}

int SSL_use_PrivateKey_ASN1(int type, SSL *ssl, const uint8_t *der,
                            size_t der_len) {
  if (der_len > LONG_MAX) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
    return 0;
  }

  const uint8_t *p = der;
  UniquePtr<EVP_PKEY> pkey(d2i_PrivateKey(type, NULL, &p, (long)der_len));
  if (!pkey || p != der + der_len) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_ASN1_LIB);
    return 0;
  }

  return SSL_use_PrivateKey(ssl, pkey.get());
}

int SSL_CTX_use_RSAPrivateKey(SSL_CTX *ctx, RSA *rsa) {
  if (rsa == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  if (!pkey ||
      !EVP_PKEY_set1_RSA(pkey.get(), rsa)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_EVP_LIB);
    return 0;
  }

  return ssl_set_pkey(ctx->cert, pkey.get());
}

int SSL_CTX_use_RSAPrivateKey_ASN1(SSL_CTX *ctx, const uint8_t *der,
                                   size_t der_len) {
  UniquePtr<RSA> rsa(RSA_private_key_from_bytes(der, der_len));
  if (!rsa) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_ASN1_LIB);
    return 0;
  }

  return SSL_CTX_use_RSAPrivateKey(ctx, rsa.get());
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
  UniquePtr<EVP_PKEY> pkey(d2i_PrivateKey(type, NULL, &p, (long)der_len));
  if (!pkey || p != der + der_len) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_ASN1_LIB);
    return 0;
  }

  return SSL_CTX_use_PrivateKey(ctx, pkey.get());
}

void SSL_set_private_key_method(SSL *ssl,
                                const SSL_PRIVATE_KEY_METHOD *key_method) {
  ssl->cert->key_method = key_method;
}

void SSL_CTX_set_private_key_method(SSL_CTX *ctx,
                                    const SSL_PRIVATE_KEY_METHOD *key_method) {
  ctx->cert->key_method = key_method;
}

const char *SSL_get_signature_algorithm_name(uint16_t sigalg,
                                             int include_curve) {
  switch (sigalg) {
    case SSL_SIGN_RSA_PKCS1_MD5_SHA1:
      return "rsa_pkcs1_md5_sha1";
    case SSL_SIGN_RSA_PKCS1_SHA1:
      return "rsa_pkcs1_sha1";
    case SSL_SIGN_RSA_PKCS1_SHA256:
      return "rsa_pkcs1_sha256";
    case SSL_SIGN_RSA_PKCS1_SHA384:
      return "rsa_pkcs1_sha384";
    case SSL_SIGN_RSA_PKCS1_SHA512:
      return "rsa_pkcs1_sha512";
    case SSL_SIGN_ECDSA_SHA1:
      return "ecdsa_sha1";
    case SSL_SIGN_ECDSA_SECP256R1_SHA256:
      return include_curve ? "ecdsa_secp256r1_sha256" : "ecdsa_sha256";
    case SSL_SIGN_ECDSA_SECP384R1_SHA384:
      return include_curve ? "ecdsa_secp384r1_sha384" : "ecdsa_sha384";
    case SSL_SIGN_ECDSA_SECP521R1_SHA512:
      return include_curve ? "ecdsa_secp521r1_sha512" : "ecdsa_sha512";
    case SSL_SIGN_RSA_PSS_SHA256:
      return "rsa_pss_sha256";
    case SSL_SIGN_RSA_PSS_SHA384:
      return "rsa_pss_sha384";
    case SSL_SIGN_RSA_PSS_SHA512:
      return "rsa_pss_sha512";
    case SSL_SIGN_ED25519:
      return "ed25519";
    default:
      return NULL;
  }
}

int SSL_get_signature_algorithm_key_type(uint16_t sigalg) {
  const SSL_SIGNATURE_ALGORITHM *alg = get_signature_algorithm(sigalg);
  return alg != nullptr ? alg->pkey_type : EVP_PKEY_NONE;
}

const EVP_MD *SSL_get_signature_algorithm_digest(uint16_t sigalg) {
  const SSL_SIGNATURE_ALGORITHM *alg = get_signature_algorithm(sigalg);
  if (alg == nullptr || alg->digest_func == nullptr) {
    return nullptr;
  }
  return alg->digest_func();
}

int SSL_is_signature_algorithm_rsa_pss(uint16_t sigalg) {
  const SSL_SIGNATURE_ALGORITHM *alg = get_signature_algorithm(sigalg);
  return alg != nullptr && alg->is_rsa_pss;
}

static int set_algorithm_prefs(uint16_t **out_prefs, size_t *out_num_prefs,
                               const uint16_t *prefs, size_t num_prefs) {
  OPENSSL_free(*out_prefs);

  *out_num_prefs = 0;
  *out_prefs = (uint16_t *)BUF_memdup(prefs, num_prefs * sizeof(prefs[0]));
  if (*out_prefs == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return 0;
  }
  *out_num_prefs = num_prefs;

  return 1;
}

int SSL_CTX_set_signing_algorithm_prefs(SSL_CTX *ctx, const uint16_t *prefs,
                                        size_t num_prefs) {
  return set_algorithm_prefs(&ctx->cert->sigalgs, &ctx->cert->num_sigalgs,
                             prefs, num_prefs);
}

int SSL_set_signing_algorithm_prefs(SSL *ssl, const uint16_t *prefs,
                                    size_t num_prefs) {
  return set_algorithm_prefs(&ssl->cert->sigalgs, &ssl->cert->num_sigalgs,
                             prefs, num_prefs);
}

int SSL_CTX_set_verify_algorithm_prefs(SSL_CTX *ctx, const uint16_t *prefs,
                                       size_t num_prefs) {
  return set_algorithm_prefs(&ctx->verify_sigalgs, &ctx->num_verify_sigalgs,
                             prefs, num_prefs);
}
