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
 * Copyright (c) 1998-2007 The OpenSSL Project.  All rights reserved.
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
 * Hudson (tjh@cryptsoft.com).
 *
 */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 * ECC cipher suite support in OpenSSL originally developed by
 * SUN MICROSYSTEMS, INC., and contributed to the OpenSSL project. */

#include <openssl/ssl.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <utility>

#include <openssl/bn.h>
#include <openssl/bytestring.h>
#include <openssl/ec_key.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

#include "../crypto/internal.h"
#include "internal.h"


BSSL_NAMESPACE_BEGIN

CERT::CERT(const SSL_X509_METHOD *x509_method_arg)
    : default_credential(MakeUnique<SSL_CREDENTIAL>(SSLCredentialType::kX509)),
      x509_method(x509_method_arg) {}

CERT::~CERT() { x509_method->cert_free(this); }

UniquePtr<CERT> ssl_cert_dup(CERT *cert) {
  UniquePtr<CERT> ret = MakeUnique<CERT>(cert->x509_method);
  if (!ret) {
    return nullptr;
  }

  // TODO(crbug.com/boringssl/431): This should just be |CopyFrom|.
  for (const auto &cred : cert->credentials) {
    if (!ret->credentials.Push(UpRef(cred))) {
      return nullptr;
    }
  }

  // |default_credential| is mutable, so it must be copied. We cannot simply
  // bump the reference count.
  ret->default_credential = cert->default_credential->Dup();
  if (ret->default_credential == nullptr) {
    return nullptr;
  }

  ret->cert_cb = cert->cert_cb;
  ret->cert_cb_arg = cert->cert_cb_arg;

  ret->x509_method->cert_dup(ret.get(), cert);

  ret->sid_ctx_length = cert->sid_ctx_length;
  OPENSSL_memcpy(ret->sid_ctx, cert->sid_ctx, sizeof(ret->sid_ctx));

  return ret;
}

static void ssl_cert_set_cert_cb(CERT *cert, int (*cb)(SSL *ssl, void *arg),
                                 void *arg) {
  cert->cert_cb = cb;
  cert->cert_cb_arg = arg;
}

static int cert_set_chain_and_key(
    CERT *cert, CRYPTO_BUFFER *const *certs, size_t num_certs,
    EVP_PKEY *privkey, const SSL_PRIVATE_KEY_METHOD *privkey_method) {
  if (num_certs == 0 ||
      (privkey == NULL && privkey_method == NULL)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  if (privkey != NULL && privkey_method != NULL) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CANNOT_HAVE_BOTH_PRIVKEY_AND_METHOD);
    return 0;
  }

  cert->default_credential->ClearCertAndKey();
  if (!SSL_CREDENTIAL_set1_cert_chain(cert->default_credential.get(), certs,
                                      num_certs)) {
    return 0;
  }

  cert->x509_method->cert_flush_cached_leaf(cert);
  cert->x509_method->cert_flush_cached_chain(cert);

  return privkey != nullptr
             ? SSL_CREDENTIAL_set1_private_key(cert->default_credential.get(),
                                               privkey)
             : SSL_CREDENTIAL_set_private_key_method(
                   cert->default_credential.get(), privkey_method);
}

bool ssl_set_cert(CERT *cert, UniquePtr<CRYPTO_BUFFER> buffer) {
  // Don't fail for a cert/key mismatch, just free the current private key.
  // (When switching to a different keypair, the caller should switch the
  // certificate, then the key.)
  if (!cert->default_credential->SetLeafCert(
          std::move(buffer), /*discard_key_on_mismatch=*/true)) {
    return false;
  }

  cert->x509_method->cert_flush_cached_leaf(cert);
  return true;
}

bool ssl_parse_cert_chain(uint8_t *out_alert,
                          UniquePtr<STACK_OF(CRYPTO_BUFFER)> *out_chain,
                          UniquePtr<EVP_PKEY> *out_pubkey,
                          uint8_t *out_leaf_sha256, CBS *cbs,
                          CRYPTO_BUFFER_POOL *pool) {
  out_chain->reset();
  out_pubkey->reset();

  CBS certificate_list;
  if (!CBS_get_u24_length_prefixed(cbs, &certificate_list)) {
    *out_alert = SSL_AD_DECODE_ERROR;
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return false;
  }

  if (CBS_len(&certificate_list) == 0) {
    return true;
  }

  UniquePtr<STACK_OF(CRYPTO_BUFFER)> chain(sk_CRYPTO_BUFFER_new_null());
  if (!chain) {
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return false;
  }

  UniquePtr<EVP_PKEY> pubkey;
  while (CBS_len(&certificate_list) > 0) {
    CBS certificate;
    if (!CBS_get_u24_length_prefixed(&certificate_list, &certificate) ||
        CBS_len(&certificate) == 0) {
      *out_alert = SSL_AD_DECODE_ERROR;
      OPENSSL_PUT_ERROR(SSL, SSL_R_CERT_LENGTH_MISMATCH);
      return false;
    }

    if (sk_CRYPTO_BUFFER_num(chain.get()) == 0) {
      pubkey = ssl_cert_parse_pubkey(&certificate);
      if (!pubkey) {
        *out_alert = SSL_AD_DECODE_ERROR;
        return false;
      }

      // Retain the hash of the leaf certificate if requested.
      if (out_leaf_sha256 != NULL) {
        SHA256(CBS_data(&certificate), CBS_len(&certificate), out_leaf_sha256);
      }
    }

    UniquePtr<CRYPTO_BUFFER> buf(
        CRYPTO_BUFFER_new_from_CBS(&certificate, pool));
    if (!buf ||
        !PushToStack(chain.get(), std::move(buf))) {
      *out_alert = SSL_AD_INTERNAL_ERROR;
      return false;
    }
  }

  *out_chain = std::move(chain);
  *out_pubkey = std::move(pubkey);
  return true;
}

// ssl_cert_skip_to_spki parses a DER-encoded, X.509 certificate from |in| and
// positions |*out_tbs_cert| to cover the TBSCertificate, starting at the
// subjectPublicKeyInfo.
static bool ssl_cert_skip_to_spki(const CBS *in, CBS *out_tbs_cert) {
  /* From RFC 5280, section 4.1
   *    Certificate  ::=  SEQUENCE  {
   *      tbsCertificate       TBSCertificate,
   *      signatureAlgorithm   AlgorithmIdentifier,
   *      signatureValue       BIT STRING  }

   * TBSCertificate  ::=  SEQUENCE  {
   *      version         [0]  EXPLICIT Version DEFAULT v1,
   *      serialNumber         CertificateSerialNumber,
   *      signature            AlgorithmIdentifier,
   *      issuer               Name,
   *      validity             Validity,
   *      subject              Name,
   *      subjectPublicKeyInfo SubjectPublicKeyInfo,
   *      ... } */
  CBS buf = *in;

  CBS toplevel;
  if (!CBS_get_asn1(&buf, &toplevel, CBS_ASN1_SEQUENCE) ||
      CBS_len(&buf) != 0 ||
      !CBS_get_asn1(&toplevel, out_tbs_cert, CBS_ASN1_SEQUENCE) ||
      // version
      !CBS_get_optional_asn1(
          out_tbs_cert, NULL, NULL,
          CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC | 0) ||
      // serialNumber
      !CBS_get_asn1(out_tbs_cert, NULL, CBS_ASN1_INTEGER) ||
      // signature algorithm
      !CBS_get_asn1(out_tbs_cert, NULL, CBS_ASN1_SEQUENCE) ||
      // issuer
      !CBS_get_asn1(out_tbs_cert, NULL, CBS_ASN1_SEQUENCE) ||
      // validity
      !CBS_get_asn1(out_tbs_cert, NULL, CBS_ASN1_SEQUENCE) ||
      // subject
      !CBS_get_asn1(out_tbs_cert, NULL, CBS_ASN1_SEQUENCE)) {
    return false;
  }

  return true;
}

UniquePtr<EVP_PKEY> ssl_cert_parse_pubkey(const CBS *in) {
  CBS buf = *in, tbs_cert;
  if (!ssl_cert_skip_to_spki(&buf, &tbs_cert)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CANNOT_PARSE_LEAF_CERT);
    return nullptr;
  }

  return UniquePtr<EVP_PKEY>(EVP_parse_public_key(&tbs_cert));
}

bool ssl_compare_public_and_private_key(const EVP_PKEY *pubkey,
                                        const EVP_PKEY *privkey) {
  if (EVP_PKEY_is_opaque(privkey)) {
    // We cannot check an opaque private key and have to trust that it
    // matches.
    return true;
  }

  switch (EVP_PKEY_cmp(pubkey, privkey)) {
    case 1:
      return true;
    case 0:
      OPENSSL_PUT_ERROR(X509, X509_R_KEY_VALUES_MISMATCH);
      return false;
    case -1:
      OPENSSL_PUT_ERROR(X509, X509_R_KEY_TYPE_MISMATCH);
      return false;
    case -2:
      OPENSSL_PUT_ERROR(X509, X509_R_UNKNOWN_KEY_TYPE);
      return false;
  }

  assert(0);
  return false;
}

bool ssl_cert_check_key_usage(const CBS *in, enum ssl_key_usage_t bit) {
  CBS buf = *in;

  CBS tbs_cert, outer_extensions;
  int has_extensions;
  if (!ssl_cert_skip_to_spki(&buf, &tbs_cert) ||
      // subjectPublicKeyInfo
      !CBS_get_asn1(&tbs_cert, NULL, CBS_ASN1_SEQUENCE) ||
      // issuerUniqueID
      !CBS_get_optional_asn1(&tbs_cert, NULL, NULL,
                             CBS_ASN1_CONTEXT_SPECIFIC | 1) ||
      // subjectUniqueID
      !CBS_get_optional_asn1(&tbs_cert, NULL, NULL,
                             CBS_ASN1_CONTEXT_SPECIFIC | 2) ||
      !CBS_get_optional_asn1(
          &tbs_cert, &outer_extensions, &has_extensions,
          CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC | 3)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CANNOT_PARSE_LEAF_CERT);
    return false;
  }

  if (!has_extensions) {
    return true;
  }

  CBS extensions;
  if (!CBS_get_asn1(&outer_extensions, &extensions, CBS_ASN1_SEQUENCE)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CANNOT_PARSE_LEAF_CERT);
    return false;
  }

  while (CBS_len(&extensions) > 0) {
    CBS extension, oid, contents;
    if (!CBS_get_asn1(&extensions, &extension, CBS_ASN1_SEQUENCE) ||
        !CBS_get_asn1(&extension, &oid, CBS_ASN1_OBJECT) ||
        (CBS_peek_asn1_tag(&extension, CBS_ASN1_BOOLEAN) &&
         !CBS_get_asn1(&extension, NULL, CBS_ASN1_BOOLEAN)) ||
        !CBS_get_asn1(&extension, &contents, CBS_ASN1_OCTETSTRING) ||
        CBS_len(&extension) != 0) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_CANNOT_PARSE_LEAF_CERT);
      return false;
    }

    static const uint8_t kKeyUsageOID[3] = {0x55, 0x1d, 0x0f};
    if (CBS_len(&oid) != sizeof(kKeyUsageOID) ||
        OPENSSL_memcmp(CBS_data(&oid), kKeyUsageOID, sizeof(kKeyUsageOID)) !=
            0) {
      continue;
    }

    CBS bit_string;
    if (!CBS_get_asn1(&contents, &bit_string, CBS_ASN1_BITSTRING) ||
        CBS_len(&contents) != 0) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_CANNOT_PARSE_LEAF_CERT);
      return false;
    }

    // This is the KeyUsage extension. See
    // https://tools.ietf.org/html/rfc5280#section-4.2.1.3
    if (!CBS_is_valid_asn1_bitstring(&bit_string)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_CANNOT_PARSE_LEAF_CERT);
      return false;
    }

    if (!CBS_asn1_bitstring_has_bit(&bit_string, bit)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_KEY_USAGE_BIT_INCORRECT);
      return false;
    }

    return true;
  }

  // No KeyUsage extension found.
  return true;
}

UniquePtr<STACK_OF(CRYPTO_BUFFER)> ssl_parse_client_CA_list(SSL *ssl,
                                                            uint8_t *out_alert,
                                                            CBS *cbs) {
  CRYPTO_BUFFER_POOL *const pool = ssl->ctx->pool;

  UniquePtr<STACK_OF(CRYPTO_BUFFER)> ret(sk_CRYPTO_BUFFER_new_null());
  if (!ret) {
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return nullptr;
  }

  CBS child;
  if (!CBS_get_u16_length_prefixed(cbs, &child)) {
    *out_alert = SSL_AD_DECODE_ERROR;
    OPENSSL_PUT_ERROR(SSL, SSL_R_LENGTH_MISMATCH);
    return nullptr;
  }

  while (CBS_len(&child) > 0) {
    CBS distinguished_name;
    if (!CBS_get_u16_length_prefixed(&child, &distinguished_name)) {
      *out_alert = SSL_AD_DECODE_ERROR;
      OPENSSL_PUT_ERROR(SSL, SSL_R_CA_DN_TOO_LONG);
      return nullptr;
    }

    UniquePtr<CRYPTO_BUFFER> buffer(
        CRYPTO_BUFFER_new_from_CBS(&distinguished_name, pool));
    if (!buffer ||
        !PushToStack(ret.get(), std::move(buffer))) {
      *out_alert = SSL_AD_INTERNAL_ERROR;
      return nullptr;
    }
  }

  if (!ssl->ctx->x509_method->check_client_CA_list(ret.get())) {
    *out_alert = SSL_AD_DECODE_ERROR;
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return nullptr;
  }

  return ret;
}

bool ssl_has_client_CAs(const SSL_CONFIG *cfg) {
  const STACK_OF(CRYPTO_BUFFER) *names = cfg->client_CA.get();
  if (names == nullptr) {
    names = cfg->ssl->ctx->client_CA.get();
  }
  if (names == nullptr) {
    return false;
  }
  return sk_CRYPTO_BUFFER_num(names) > 0;
}

bool ssl_add_client_CA_list(SSL_HANDSHAKE *hs, CBB *cbb) {
  CBB child, name_cbb;
  if (!CBB_add_u16_length_prefixed(cbb, &child)) {
    return false;
  }

  const STACK_OF(CRYPTO_BUFFER) *names = hs->config->client_CA.get();
  if (names == NULL) {
    names = hs->ssl->ctx->client_CA.get();
  }
  if (names == NULL) {
    return CBB_flush(cbb);
  }

  for (const CRYPTO_BUFFER *name : names) {
    if (!CBB_add_u16_length_prefixed(&child, &name_cbb) ||
        !CBB_add_bytes(&name_cbb, CRYPTO_BUFFER_data(name),
                       CRYPTO_BUFFER_len(name))) {
      return false;
    }
  }

  return CBB_flush(cbb);
}

bool ssl_check_leaf_certificate(SSL_HANDSHAKE *hs, EVP_PKEY *pkey,
                                const CRYPTO_BUFFER *leaf) {
  assert(ssl_protocol_version(hs->ssl) < TLS1_3_VERSION);

  // Check the certificate's type matches the cipher. This does not check key
  // usage restrictions, which are handled separately.
  //
  // TODO(davidben): Put the key type and key usage checks in one place.
  if (!(hs->new_cipher->algorithm_auth &
        ssl_cipher_auth_mask_for_key(pkey, /*sign_ok=*/true))) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_CERTIFICATE_TYPE);
    return false;
  }

  if (EVP_PKEY_id(pkey) == EVP_PKEY_EC) {
    // Check the key's group and point format are acceptable.
    EC_KEY *ec_key = EVP_PKEY_get0_EC_KEY(pkey);
    uint16_t group_id;
    if (!ssl_nid_to_group_id(
            &group_id, EC_GROUP_get_curve_name(EC_KEY_get0_group(ec_key))) ||
        !tls1_check_group_id(hs, group_id) ||
        EC_KEY_get_conv_form(ec_key) != POINT_CONVERSION_UNCOMPRESSED) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_ECC_CERT);
      return false;
    }
  }

  return true;
}

BSSL_NAMESPACE_END

using namespace bssl;

int SSL_set_chain_and_key(SSL *ssl, CRYPTO_BUFFER *const *certs,
                          size_t num_certs, EVP_PKEY *privkey,
                          const SSL_PRIVATE_KEY_METHOD *privkey_method) {
  if (!ssl->config) {
    return 0;
  }
  return cert_set_chain_and_key(ssl->config->cert.get(), certs, num_certs,
                                privkey, privkey_method);
}

int SSL_CTX_set_chain_and_key(SSL_CTX *ctx, CRYPTO_BUFFER *const *certs,
                              size_t num_certs, EVP_PKEY *privkey,
                              const SSL_PRIVATE_KEY_METHOD *privkey_method) {
  return cert_set_chain_and_key(ctx->cert.get(), certs, num_certs, privkey,
                                privkey_method);
}

void SSL_certs_clear(SSL *ssl) {
  if (!ssl->config) {
    return;
  }

  CERT *cert = ssl->config->cert.get();
  cert->x509_method->cert_clear(cert);
  cert->credentials.clear();
  cert->default_credential->ClearCertAndKey();
}

const STACK_OF(CRYPTO_BUFFER) *SSL_CTX_get0_chain(const SSL_CTX *ctx) {
  return ctx->cert->default_credential->chain.get();
}

const STACK_OF(CRYPTO_BUFFER) *SSL_get0_chain(const SSL *ssl) {
  if (!ssl->config) {
    return nullptr;
  }
  return ssl->config->cert->default_credential->chain.get();
}

int SSL_CTX_use_certificate_ASN1(SSL_CTX *ctx, size_t der_len,
                                 const uint8_t *der) {
  UniquePtr<CRYPTO_BUFFER> buffer(CRYPTO_BUFFER_new(der, der_len, NULL));
  if (!buffer) {
    return 0;
  }

  return ssl_set_cert(ctx->cert.get(), std::move(buffer));
}

int SSL_use_certificate_ASN1(SSL *ssl, const uint8_t *der, size_t der_len) {
  UniquePtr<CRYPTO_BUFFER> buffer(CRYPTO_BUFFER_new(der, der_len, NULL));
  if (!buffer || !ssl->config) {
    return 0;
  }

  return ssl_set_cert(ssl->config->cert.get(), std::move(buffer));
}

void SSL_CTX_set_cert_cb(SSL_CTX *ctx, int (*cb)(SSL *ssl, void *arg),
                         void *arg) {
  ssl_cert_set_cert_cb(ctx->cert.get(), cb, arg);
}

void SSL_set_cert_cb(SSL *ssl, int (*cb)(SSL *ssl, void *arg), void *arg) {
  if (!ssl->config) {
    return;
  }
  ssl_cert_set_cert_cb(ssl->config->cert.get(), cb, arg);
}

const STACK_OF(CRYPTO_BUFFER) *SSL_get0_peer_certificates(const SSL *ssl) {
  SSL_SESSION *session = SSL_get_session(ssl);
  if (session == NULL) {
    return NULL;
  }

  return session->certs.get();
}

const STACK_OF(CRYPTO_BUFFER) *SSL_get0_server_requested_CAs(const SSL *ssl) {
  if (ssl->s3->hs == NULL) {
    return NULL;
  }
  return ssl->s3->hs->ca_names.get();
}

int SSL_CTX_set_signed_cert_timestamp_list(SSL_CTX *ctx, const uint8_t *list,
                                           size_t list_len) {
  UniquePtr<CRYPTO_BUFFER> buf(CRYPTO_BUFFER_new(list, list_len, nullptr));
  return buf != nullptr && SSL_CREDENTIAL_set1_signed_cert_timestamp_list(
                               ctx->cert->default_credential.get(), buf.get());
}

int SSL_set_signed_cert_timestamp_list(SSL *ssl, const uint8_t *list,
                                       size_t list_len) {
  if (!ssl->config) {
    return 0;
  }
  UniquePtr<CRYPTO_BUFFER> buf(CRYPTO_BUFFER_new(list, list_len, nullptr));
  return buf != nullptr &&
         SSL_CREDENTIAL_set1_signed_cert_timestamp_list(
             ssl->config->cert->default_credential.get(), buf.get());
}

int SSL_CTX_set_ocsp_response(SSL_CTX *ctx, const uint8_t *response,
                              size_t response_len) {
  UniquePtr<CRYPTO_BUFFER> buf(
      CRYPTO_BUFFER_new(response, response_len, nullptr));
  return buf != nullptr && SSL_CREDENTIAL_set1_ocsp_response(
                               ctx->cert->default_credential.get(), buf.get());
}

int SSL_set_ocsp_response(SSL *ssl, const uint8_t *response,
                          size_t response_len) {
  if (!ssl->config) {
    return 0;
  }
  UniquePtr<CRYPTO_BUFFER> buf(
      CRYPTO_BUFFER_new(response, response_len, nullptr));
  return buf != nullptr &&
         SSL_CREDENTIAL_set1_ocsp_response(
             ssl->config->cert->default_credential.get(), buf.get());
}

void SSL_CTX_set0_client_CAs(SSL_CTX *ctx, STACK_OF(CRYPTO_BUFFER) *name_list) {
  ctx->x509_method->ssl_ctx_flush_cached_client_CA(ctx);
  ctx->client_CA.reset(name_list);
}

void SSL_set0_client_CAs(SSL *ssl, STACK_OF(CRYPTO_BUFFER) *name_list) {
  if (!ssl->config) {
    return;
  }
  ssl->ctx->x509_method->ssl_flush_cached_client_CA(ssl->config.get());
  ssl->config->client_CA.reset(name_list);
}
