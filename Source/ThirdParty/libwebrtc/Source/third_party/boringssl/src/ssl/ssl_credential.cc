/* Copyright (c) 2024, Google Inc.
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

#include <openssl/ssl.h>

#include <assert.h>

#include <openssl/span.h>

#include "internal.h"
#include "../crypto/internal.h"


BSSL_NAMESPACE_BEGIN

// new_leafless_chain returns a fresh stack of buffers set to {nullptr}.
static UniquePtr<STACK_OF(CRYPTO_BUFFER)> new_leafless_chain(void) {
  UniquePtr<STACK_OF(CRYPTO_BUFFER)> chain(sk_CRYPTO_BUFFER_new_null());
  if (!chain ||
      !sk_CRYPTO_BUFFER_push(chain.get(), nullptr)) {
    return nullptr;
  }

  return chain;
}

bool ssl_get_credential_list(SSL_HANDSHAKE *hs, Array<SSL_CREDENTIAL *> *out) {
  CERT *cert = hs->config->cert.get();
  // Finish filling in the default credential if needed.
  if (!cert->x509_method->ssl_auto_chain_if_needed(hs)) {
    return false;
  }

  size_t num_creds = cert->credentials.size();
  bool include_default = cert->default_credential->IsComplete();
  if (include_default) {
    num_creds++;
  }

  if (!out->Init(num_creds)) {
    return false;
  }

  for (size_t i = 0; i < cert->credentials.size(); i++) {
    (*out)[i] = cert->credentials[i].get();
  }
  if (include_default) {
    (*out)[num_creds - 1] = cert->default_credential.get();
  }
  return true;
}

BSSL_NAMESPACE_END

using namespace bssl;

static CRYPTO_EX_DATA_CLASS g_ex_data_class = CRYPTO_EX_DATA_CLASS_INIT;

ssl_credential_st::ssl_credential_st(SSLCredentialType type_arg)
    : RefCounted(CheckSubClass()), type(type_arg) {
  CRYPTO_new_ex_data(&ex_data);
}

ssl_credential_st::~ssl_credential_st() {
  CRYPTO_free_ex_data(&g_ex_data_class, this, &ex_data);
}

static CRYPTO_BUFFER *buffer_up_ref(const CRYPTO_BUFFER *buffer) {
  CRYPTO_BUFFER_up_ref(const_cast<CRYPTO_BUFFER *>(buffer));
  return const_cast<CRYPTO_BUFFER *>(buffer);
}

UniquePtr<SSL_CREDENTIAL> ssl_credential_st::Dup() const {
  assert(type == SSLCredentialType::kX509);
  UniquePtr<SSL_CREDENTIAL> ret = MakeUnique<SSL_CREDENTIAL>(type);
  if (ret == nullptr) {
    return nullptr;
  }

  ret->pubkey = UpRef(pubkey);
  ret->privkey = UpRef(privkey);
  ret->key_method = key_method;
  if (!ret->sigalgs.CopyFrom(sigalgs)) {
    return nullptr;
  }

  if (chain) {
    ret->chain.reset(sk_CRYPTO_BUFFER_deep_copy(chain.get(), buffer_up_ref,
                                                CRYPTO_BUFFER_free));
    if (!ret->chain) {
      return nullptr;
    }
  }

  ret->dc = UpRef(dc);
  ret->signed_cert_timestamp_list = UpRef(signed_cert_timestamp_list);
  ret->ocsp_response = UpRef(ocsp_response);
  ret->dc_algorithm = dc_algorithm;
  return ret;
}

void ssl_credential_st::ClearCertAndKey() {
  pubkey = nullptr;
  privkey = nullptr;
  key_method = nullptr;
  chain = nullptr;
}

bool ssl_credential_st::UsesX509() const {
  // Currently, all credential types use X.509. However, we may add other
  // certificate types in the future. Add the checks in the setters now, so we
  // don't forget.
  return true;
}

bool ssl_credential_st::UsesPrivateKey() const {
  // Currently, all credential types use private keys. However, we may add PSK
  return true;
}

bool ssl_credential_st::IsComplete() const {
  // APIs like |SSL_use_certificate| and |SSL_set1_chain| configure the leaf and
  // other certificates separately. It is possible for |chain| have a null leaf.
  if (UsesX509() && (sk_CRYPTO_BUFFER_num(chain.get()) == 0 ||
                     sk_CRYPTO_BUFFER_value(chain.get(), 0) == nullptr)) {
    return false;
  }
  // We must have successfully extracted a public key from the certificate,
  // delegated credential, etc.
  if (UsesPrivateKey() && pubkey == nullptr) {
    return false;
  }
  if (UsesPrivateKey() && privkey == nullptr && key_method == nullptr) {
    return false;
  }
  if (type == SSLCredentialType::kDelegated && dc == nullptr) {
    return false;
  }
  return true;
}

bool ssl_credential_st::SetLeafCert(UniquePtr<CRYPTO_BUFFER> leaf,
                                    bool discard_key_on_mismatch) {
  if (!UsesX509()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return false;
  }

  const bool private_key_matches_leaf = type != SSLCredentialType::kDelegated;

  CBS cbs;
  CRYPTO_BUFFER_init_CBS(leaf.get(), &cbs);
  UniquePtr<EVP_PKEY> new_pubkey = ssl_cert_parse_pubkey(&cbs);
  if (new_pubkey == nullptr) {
    return false;
  }

  if (!ssl_is_key_type_supported(EVP_PKEY_id(new_pubkey.get()))) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNKNOWN_CERTIFICATE_TYPE);
    return false;
  }

  // An ECC certificate may be usable for ECDH or ECDSA. We only support ECDSA
  // certificates, so sanity-check the key usage extension.
  if (EVP_PKEY_id(new_pubkey.get()) == EVP_PKEY_EC &&
      !ssl_cert_check_key_usage(&cbs, key_usage_digital_signature)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNKNOWN_CERTIFICATE_TYPE);
    return false;
  }

  if (private_key_matches_leaf && privkey != nullptr &&
      !ssl_compare_public_and_private_key(new_pubkey.get(), privkey.get())) {
    if (!discard_key_on_mismatch) {
      return false;
    }
    ERR_clear_error();
    privkey = nullptr;
  }

  if (chain == nullptr) {
    chain = new_leafless_chain();
    if (chain == nullptr) {
      return false;
    }
  }

  CRYPTO_BUFFER_free(sk_CRYPTO_BUFFER_value(chain.get(), 0));
  sk_CRYPTO_BUFFER_set(chain.get(), 0, leaf.release());
  if (private_key_matches_leaf) {
    pubkey = std::move(new_pubkey);
  }
  return true;
}

void ssl_credential_st::ClearIntermediateCerts() {
  if (chain == nullptr) {
    return;
  }

  while (sk_CRYPTO_BUFFER_num(chain.get()) > 1) {
    CRYPTO_BUFFER_free(sk_CRYPTO_BUFFER_pop(chain.get()));
  }
}

bool ssl_credential_st::AppendIntermediateCert(UniquePtr<CRYPTO_BUFFER> cert) {
  if (!UsesX509()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return false;
  }

  if (chain == nullptr) {
    chain = new_leafless_chain();
    if (chain == nullptr) {
      return false;
    }
  }

  return PushToStack(chain.get(), std::move(cert));
}

SSL_CREDENTIAL *SSL_CREDENTIAL_new_x509(void) {
  return New<SSL_CREDENTIAL>(SSLCredentialType::kX509);
}

SSL_CREDENTIAL *SSL_CREDENTIAL_new_delegated(void) {
  return New<SSL_CREDENTIAL>(SSLCredentialType::kDelegated);
}

void SSL_CREDENTIAL_up_ref(SSL_CREDENTIAL *cred) { cred->UpRefInternal(); }

void SSL_CREDENTIAL_free(SSL_CREDENTIAL *cred) {
  if (cred != nullptr) {
    cred->DecRefInternal();
  }
}

int SSL_CREDENTIAL_set1_private_key(SSL_CREDENTIAL *cred, EVP_PKEY *key) {
  if (!cred->UsesPrivateKey()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  // If the public half has been configured, check |key| matches. |pubkey| will
  // have been extracted from the certificate, delegated credential, etc.
  if (cred->pubkey != nullptr &&
      !ssl_compare_public_and_private_key(cred->pubkey.get(), key)) {
    return false;
  }

  cred->privkey = UpRef(key);
  cred->key_method = nullptr;
  return 1;
}

int SSL_CREDENTIAL_set_private_key_method(
    SSL_CREDENTIAL *cred, const SSL_PRIVATE_KEY_METHOD *key_method) {
  if (!cred->UsesPrivateKey()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  cred->privkey = nullptr;
  cred->key_method = key_method;
  return 1;
}

int SSL_CREDENTIAL_set1_cert_chain(SSL_CREDENTIAL *cred,
                                   CRYPTO_BUFFER *const *certs,
                                   size_t num_certs) {
  if (!cred->UsesX509() || num_certs == 0) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  if (!cred->SetLeafCert(UpRef(certs[0]), /*discard_key_on_mismatch=*/false)) {
    return 0;
  }

  cred->ClearIntermediateCerts();
  for (size_t i = 1; i < num_certs; i++) {
    if (!cred->AppendIntermediateCert(UpRef(certs[i]))) {
      return 0;
    }
  }

  return 1;
}

int SSL_CREDENTIAL_set1_delegated_credential(
    SSL_CREDENTIAL *cred, CRYPTO_BUFFER *dc) {
  if (cred->type != SSLCredentialType::kDelegated) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  // Parse the delegated credential to check for validity, and extract a few
  // fields from it. See RFC 9345, section 4.
  CBS cbs, spki, sig;
  uint32_t valid_time;
  uint16_t dc_cert_verify_algorithm, algorithm;
  CRYPTO_BUFFER_init_CBS(dc, &cbs);
  if (!CBS_get_u32(&cbs, &valid_time) ||
      !CBS_get_u16(&cbs, &dc_cert_verify_algorithm) ||
      !CBS_get_u24_length_prefixed(&cbs, &spki) ||
      !CBS_get_u16(&cbs, &algorithm) ||
      !CBS_get_u16_length_prefixed(&cbs, &sig) ||  //
      CBS_len(&sig) == 0 ||                        //
      CBS_len(&cbs) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return 0;
  }

  // RFC 9345 forbids algorithms that use the rsaEncryption OID. As the
  // RSASSA-PSS OID is unusably complicated, this effectively means we will not
  // support RSA delegated credentials.
  if (SSL_get_signature_algorithm_key_type(dc_cert_verify_algorithm) ==
      EVP_PKEY_RSA) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_SIGNATURE_ALGORITHM);
    return 0;
  }

  UniquePtr<EVP_PKEY> pubkey(EVP_parse_public_key(&spki));
  if (pubkey == nullptr || CBS_len(&spki) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return 0;
  }

  if (!cred->sigalgs.CopyFrom(MakeConstSpan(&dc_cert_verify_algorithm, 1))) {
    return 0;
  }

  if (cred->privkey != nullptr &&
      !ssl_compare_public_and_private_key(pubkey.get(), cred->privkey.get())) {
    return 0;
  }

  cred->dc = UpRef(dc);
  cred->pubkey = std::move(pubkey);
  cred->dc_algorithm = algorithm;
  return 1;
}

int SSL_CREDENTIAL_set1_ocsp_response(SSL_CREDENTIAL *cred,
                                      CRYPTO_BUFFER *ocsp) {
  if (!cred->UsesX509()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  cred->ocsp_response = UpRef(ocsp);
  return 1;
}

int SSL_CREDENTIAL_set1_signed_cert_timestamp_list(SSL_CREDENTIAL *cred,
                                                   CRYPTO_BUFFER *sct_list) {
  if (!cred->UsesX509()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  CBS cbs;
  CRYPTO_BUFFER_init_CBS(sct_list, &cbs);
  if (!ssl_is_sct_list_valid(&cbs)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_SCT_LIST);
    return 0;
  }

  cred->signed_cert_timestamp_list = UpRef(sct_list);
  return 1;
}

int SSL_CTX_add1_credential(SSL_CTX *ctx, SSL_CREDENTIAL *cred) {
  if (!cred->IsComplete()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }
  return ctx->cert->credentials.Push(UpRef(cred));
}

int SSL_add1_credential(SSL *ssl, SSL_CREDENTIAL *cred) {
  if (ssl->config == nullptr) {
    return 0;
  }

  if (!cred->IsComplete()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }
  return ssl->config->cert->credentials.Push(UpRef(cred));
}

const SSL_CREDENTIAL *SSL_get0_selected_credential(const SSL *ssl) {
  if (ssl->s3->hs == nullptr) {
    return nullptr;
  }
  return ssl->s3->hs->credential.get();
}

int SSL_CREDENTIAL_get_ex_new_index(long argl, void *argp,
                                    CRYPTO_EX_unused *unused,
                                    CRYPTO_EX_dup *dup_unused,
                                    CRYPTO_EX_free *free_func) {
  return CRYPTO_get_ex_new_index_ex(&g_ex_data_class, argl, argp, free_func);
}

int SSL_CREDENTIAL_set_ex_data(SSL_CREDENTIAL *cred, int idx, void *arg) {
  return CRYPTO_set_ex_data(&cred->ex_data, idx, arg);
}

void *SSL_CREDENTIAL_get_ex_data(const SSL_CREDENTIAL *cred, int idx) {
  return CRYPTO_get_ex_data(&cred->ex_data, idx);
}
