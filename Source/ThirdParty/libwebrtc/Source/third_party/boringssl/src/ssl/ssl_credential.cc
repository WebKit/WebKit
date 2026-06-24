// Copyright 2024 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <openssl/base.h>
#include <openssl/ssl.h>

#include <assert.h>

#include <openssl/hkdf.h>
#include <openssl/span.h>

#include "../crypto/internal.h"
#include "../crypto/mem_internal.h"
#include "../crypto/spake2plus/internal.h"
#include "internal.h"


BSSL_NAMESPACE_BEGIN

// new_leafless_chain returns a fresh stack of buffers set to {nullptr}.
static UniquePtr<STACK_OF(CRYPTO_BUFFER)> new_leafless_chain() {
  UniquePtr<STACK_OF(CRYPTO_BUFFER)> chain(sk_CRYPTO_BUFFER_new_null());
  if (!chain || !sk_CRYPTO_BUFFER_push(chain.get(), nullptr)) {
    return nullptr;
  }

  return chain;
}

bool ssl_get_full_credential_list(SSL_HANDSHAKE *hs,
                                  Array<SSLCredential *> *out) {
  CERT *cert = hs->config->cert.get();
  // Finish filling in the legacy credential if needed.
  if (!cert->x509_method->ssl_auto_chain_if_needed(hs)) {
    return false;
  }

  size_t num_creds = cert->credentials.size();
  bool include_legacy = cert->legacy_credential->IsComplete();
  if (include_legacy) {
    num_creds++;
  }

  if (!out->InitForOverwrite(num_creds)) {
    return false;
  }

  for (size_t i = 0; i < cert->credentials.size(); i++) {
    (*out)[i] = cert->credentials[i].get();
  }
  if (include_legacy) {
    (*out)[num_creds - 1] = cert->legacy_credential.get();
  }
  return true;
}

bool ssl_credential_matches_requested_issuers(SSL_HANDSHAKE *hs,
                                              const SSLCredential *cred) {
  if (!cred->must_match_issuer) {
    // This credential does not need to match a requested issuer, so
    // it is good to use without a match.
    return true;
  }

  // If we have names sent by the CA extension, and this
  // credential matches it, it is good.
  if (hs->ca_names != nullptr) {
    for (const CRYPTO_BUFFER *ca_name : hs->ca_names.get()) {
      if (cred->ChainContainsIssuer(
              Span(CRYPTO_BUFFER_data(ca_name), CRYPTO_BUFFER_len(ca_name)))) {
        return true;
      }
    }
  }
  // If the credential has a trust anchor ID and it matches one sent by the
  // peer, it is good.
  if (!cred->trust_anchor_id.empty() && hs->peer_requested_trust_anchors) {
    CBS cbs = CBS(*hs->peer_requested_trust_anchors), candidate;
    while (CBS_len(&cbs) > 0) {
      if (!CBS_get_u8_length_prefixed(&cbs, &candidate) ||
          CBS_len(&candidate) == 0) {
        OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
        return false;
      }
      if (candidate == Span(cred->trust_anchor_id)) {
        hs->matched_peer_trust_anchor = true;
        return true;
      }
    }
  }

  OPENSSL_PUT_ERROR(SSL, SSL_R_NO_MATCHING_ISSUER);
  return false;
}

std::optional<uint8_t> ssl_credential_type_to_cert_type(
    SSLCredentialType cred_type) {
  switch (cred_type) {
    case SSLCredentialType::kX509:
    case SSLCredentialType::kDelegated:
      return TLSEXT_cert_type_x509;
    case SSLCredentialType::kRawPublicKey:
      return TLSEXT_cert_type_rpk;
    default:
      return std::nullopt;
  }
}

static ExDataClass g_ex_data_class;

SSLCredential::SSLCredential(SSLCredentialType type_arg)
    : RefCounted(CheckSubClass()), type(type_arg) {
  CRYPTO_new_ex_data(&ex_data);
}

SSLCredential::~SSLCredential() {
  CRYPTO_free_ex_data(&g_ex_data_class, &ex_data);
}

UniquePtr<SSLCredential> SSLCredential::Dup() const {
  assert(type == SSLCredentialType::kX509);
  UniquePtr<SSLCredential> ret = MakeUnique<SSLCredential>(type);
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
    ret->chain.reset(sk_CRYPTO_BUFFER_deep_copy(
        chain.get(), CRYPTO_BUFFER_dup_ref, CRYPTO_BUFFER_free));
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

void SSLCredential::ClearCertAndKey() {
  pubkey = nullptr;
  privkey = nullptr;
  key_method = nullptr;
  chain = nullptr;
}

bool SSLCredential::UsesX509() const {
  switch (type) {
    case SSLCredentialType::kX509:
    case SSLCredentialType::kDelegated:
      return true;
    case SSLCredentialType::kSPAKE2PlusV1Client:
    case SSLCredentialType::kSPAKE2PlusV1Server:
    case SSLCredentialType::kPreSharedKey:
    case SSLCredentialType::kRawPublicKey:
      return false;
  }
  abort();
}

bool SSLCredential::UsesPrivateKey() const {
  switch (type) {
    case SSLCredentialType::kX509:
    case SSLCredentialType::kDelegated:
    case SSLCredentialType::kRawPublicKey:
      return true;
    case SSLCredentialType::kSPAKE2PlusV1Client:
    case SSLCredentialType::kSPAKE2PlusV1Server:
    case SSLCredentialType::kPreSharedKey:
      return false;
  }
  abort();
}

bool SSLCredential::IsComplete() const {
  // APIs like |SSL_use_certificate| and |SSL_set1_chain| configure the leaf and
  // other certificates separately. It is possible for |chain| have a null leaf.
  if (UsesX509() && (sk_CRYPTO_BUFFER_num(chain.get()) == 0 ||
                     sk_CRYPTO_BUFFER_value(chain.get(), 0) == nullptr)) {
    return false;
  }
  // We must have been configured with a public key, or successfully extracted a
  // public key from the certificate, delegated credential, etc.
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

bool SSLCredential::SetLeafCert(UniquePtr<CRYPTO_BUFFER> leaf,
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

void SSLCredential::ClearIntermediateCerts() {
  if (chain == nullptr) {
    return;
  }

  while (sk_CRYPTO_BUFFER_num(chain.get()) > 1) {
    CRYPTO_BUFFER_free(sk_CRYPTO_BUFFER_pop(chain.get()));
  }
}

bool SSLCredential::ChainContainsIssuer(Span<const uint8_t> dn) const {
  if (UsesX509()) {
    // TODO(bbe) This is used for matching a chain by CA name for the CA
    // extension. If we require a chain to be present, we could remove any
    // remaining parts of the chain after the found issuer, on the assumption
    // that the peer sending the CA extension has the issuer in their trust
    // store and does not need us to waste bytes on the wire.
    CBS dn_cbs;
    CBS_init(&dn_cbs, dn.data(), dn.size());
    for (size_t i = 0; i < sk_CRYPTO_BUFFER_num(chain.get()); i++) {
      const CRYPTO_BUFFER *cert = sk_CRYPTO_BUFFER_value(chain.get(), i);
      CBS cert_cbs;
      CRYPTO_BUFFER_init_CBS(cert, &cert_cbs);
      if (ssl_cert_matches_issuer(&cert_cbs, &dn_cbs)) {
        return true;
      }
    }
  }
  return false;
}

bool SSLCredential::HasPAKEAttempts() const { return pake_limit.load() != 0; }

bool SSLCredential::ClaimPAKEAttempt() const {
  uint32_t current = pake_limit.load(std::memory_order_relaxed);
  for (;;) {
    if (current == 0) {
      return false;
    }
    if (pake_limit.compare_exchange_weak(current, current - 1)) {
      break;
    }
  }

  return true;
}

void SSLCredential::RestorePAKEAttempt() const {
  // This should not overflow because it will only be paired with
  // ClaimPAKEAttempt.
  pake_limit.fetch_add(1);
}

bool SSLCredential::AppendIntermediateCert(UniquePtr<CRYPTO_BUFFER> cert) {
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

BSSL_NAMESPACE_END

using namespace bssl;

SSL_CREDENTIAL *SSL_CREDENTIAL_new_x509() {
  return New<SSLCredential>(SSLCredentialType::kX509);
}

SSL_CREDENTIAL *SSL_CREDENTIAL_new_pre_shared_key(
    const uint8_t *key, size_t key_len, const uint8_t *id, size_t id_len,
    const EVP_MD *md, const uint8_t *context, size_t context_len) {
  if (id_len == 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_PSK_IDENTITY_NOT_FOUND);
    return nullptr;
  }

  auto cred = MakeUnique<SSLCredential>(SSLCredentialType::kPreSharedKey);
  size_t epskx_len;
  if (cred == nullptr ||
      // Precompute epskx, to avoid recomputing it on every use of the
      // credential.
      !cred->epskx.InitForOverwrite(EVP_MD_size(md)) ||
      !HKDF_extract(cred->epskx.data(), &epskx_len, md, key, key_len,
                    /*salt=*/nullptr, /*salt_len=*/0) ||
      !cred->epsk_id.CopyFrom(Span(id, id_len)) ||
      !cred->epsk_context.CopyFrom(Span(context, context_len))) {
    return nullptr;
  }
  BSSL_CHECK(epskx_len == cred->epskx.size());
  cred->epsk_md = md;
  return cred.release();
}

SSL_CREDENTIAL *SSL_CREDENTIAL_new_delegated() {
  return New<SSLCredential>(SSLCredentialType::kDelegated);
}

SSL_CREDENTIAL *SSL_CREDENTIAL_new_raw_public_key(EVP_PKEY *pkey) {
  if (!EVP_PKEY_has_public(pkey) || !EVP_PKEY_has_private(pkey)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_MISSING_KEY);
    return nullptr;
  }
  UniquePtr<SSLCredential> cred =
      MakeUnique<SSLCredential>(SSLCredentialType::kRawPublicKey);
  if (cred == nullptr) {
    return nullptr;
  }
  cred->pubkey = UpRef(pkey);
  cred->privkey = UpRef(pkey);
  return cred.release();
}

SSL_CREDENTIAL *SSL_CREDENTIAL_new_raw_public_key_custom(
    EVP_PKEY *pubkey, const SSL_PRIVATE_KEY_METHOD *method) {
  if (!EVP_PKEY_has_public(pubkey)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_MISSING_KEY);
    return nullptr;
  }
  UniquePtr<SSLCredential> cred =
      MakeUnique<SSLCredential>(SSLCredentialType::kRawPublicKey);
  if (cred == nullptr) {
    return nullptr;
  }
  cred->pubkey = UpRef(pubkey);
  BSSL_CHECK(SSL_CREDENTIAL_set_private_key_method(cred.get(), method));
  return cred.release();
}

void SSL_CREDENTIAL_up_ref(SSL_CREDENTIAL *cred) {
  FromOpaque(cred)->UpRefInternal();
}

SSL_CREDENTIAL *SSL_CREDENTIAL_dup_ref(const SSL_CREDENTIAL *cred) {
  // Safety: we do not mutate the internal state of |cred| other than the
  // ref-count atomic variable.
  auto *cred_impl = FromOpaque(const_cast<SSL_CREDENTIAL *>(cred));
  cred_impl->UpRefInternal();
  return const_cast<SSL_CREDENTIAL *>(cred);
}

void SSL_CREDENTIAL_free(SSL_CREDENTIAL *cred) {
  if (cred != nullptr) {
    FromOpaque(cred)->DecRefInternal();
  }
}

int SSL_CREDENTIAL_is_complete(const SSL_CREDENTIAL *cred) {
  return FromOpaque(cred)->IsComplete();
}

int SSL_CREDENTIAL_set1_private_key(SSL_CREDENTIAL *cred, EVP_PKEY *key) {
  auto *cred_impl = FromOpaque(cred);
  if (!cred_impl->UsesPrivateKey()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  // If the public half has been configured, check |key| matches. |pubkey| will
  // have been extracted from the certificate, delegated credential, etc.
  if (cred_impl->pubkey != nullptr &&
      !ssl_compare_public_and_private_key(cred_impl->pubkey.get(), key)) {
    return 0;
  }

  cred_impl->privkey = UpRef(key);
  cred_impl->key_method = nullptr;
  return 1;
}

int SSL_CREDENTIAL_set_private_key_method(
    SSL_CREDENTIAL *cred, const SSL_PRIVATE_KEY_METHOD *key_method) {
  auto *cred_impl = FromOpaque(cred);
  if (!cred_impl->UsesPrivateKey()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  cred_impl->privkey = nullptr;
  cred_impl->key_method = key_method;
  return 1;
}

int SSL_CREDENTIAL_set1_cert_chain(SSL_CREDENTIAL *cred,
                                   CRYPTO_BUFFER *const *certs,
                                   size_t num_certs) {
  auto *cred_impl = FromOpaque(cred);
  if (!cred_impl->UsesX509() || num_certs == 0) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  if (!cred_impl->SetLeafCert(UpRef(certs[0]),
                              /*discard_key_on_mismatch=*/false)) {
    return 0;
  }

  cred_impl->ClearIntermediateCerts();
  for (size_t i = 1; i < num_certs; i++) {
    if (!cred_impl->AppendIntermediateCert(UpRef(certs[i]))) {
      return 0;
    }
  }

  return 1;
}

int SSL_CREDENTIAL_set1_delegated_credential(SSL_CREDENTIAL *cred,
                                             CRYPTO_BUFFER *dc) {
  auto *cred_impl = FromOpaque(cred);
  if (cred_impl->type != SSLCredentialType::kDelegated) {
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

  UniquePtr<EVP_PKEY> pubkey = ssl_parse_peer_subject_public_key_info(spki);
  if (pubkey == nullptr) {
    return 0;
  }

  if (!cred_impl->sigalgs.CopyFrom(Span(&dc_cert_verify_algorithm, 1))) {
    return 0;
  }

  if (cred_impl->privkey != nullptr &&
      !ssl_compare_public_and_private_key(pubkey.get(),
                                          cred_impl->privkey.get())) {
    return 0;
  }

  cred_impl->dc = UpRef(dc);
  cred_impl->pubkey = std::move(pubkey);
  cred_impl->dc_algorithm = algorithm;
  return 1;
}

int SSL_CREDENTIAL_set1_ocsp_response(SSL_CREDENTIAL *cred,
                                      CRYPTO_BUFFER *ocsp) {
  auto *cred_impl = FromOpaque(cred);
  if (!cred_impl->UsesX509()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  cred_impl->ocsp_response = UpRef(ocsp);
  return 1;
}

int SSL_CREDENTIAL_set1_signed_cert_timestamp_list(SSL_CREDENTIAL *cred,
                                                   CRYPTO_BUFFER *sct_list) {
  auto *cred_impl = FromOpaque(cred);
  if (!cred_impl->UsesX509()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  CBS cbs;
  CRYPTO_BUFFER_init_CBS(sct_list, &cbs);
  if (!ssl_is_sct_list_valid(&cbs)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_SCT_LIST);
    return 0;
  }

  cred_impl->signed_cert_timestamp_list = UpRef(sct_list);
  return 1;
}

int SSL_spake2plusv1_register(uint8_t out_w0[32], uint8_t out_w1[32],
                              uint8_t out_registration_record[65],
                              const uint8_t *password, size_t password_len,
                              const uint8_t *client_identity,
                              size_t client_identity_len,
                              const uint8_t *server_identity,
                              size_t server_identity_len) {
  return spake2plus::Register(
      Span(out_w0, 32), Span(out_w1, 32), Span(out_registration_record, 65),
      Span(password, password_len), Span(client_identity, client_identity_len),
      Span(server_identity, server_identity_len));
}

static UniquePtr<SSLCredential> ssl_credential_new_spake2plusv1(
    SSLCredentialType type, Span<const uint8_t> context,
    Span<const uint8_t> client_identity, Span<const uint8_t> server_identity,
    uint32_t limit) {
  assert(type == SSLCredentialType::kSPAKE2PlusV1Client ||
         type == SSLCredentialType::kSPAKE2PlusV1Server);
  auto cred = MakeUnique<SSLCredential>(type);
  if (cred == nullptr) {
    return nullptr;
  }

  if (!cred->pake_context.CopyFrom(context) ||
      !cred->client_identity.CopyFrom(client_identity) ||
      !cred->server_identity.CopyFrom(server_identity)) {
    return nullptr;
  }

  cred->pake_limit.store(limit);
  return cred;
}

SSL_CREDENTIAL *SSL_CREDENTIAL_new_spake2plusv1_client(
    const uint8_t *context, size_t context_len, const uint8_t *client_identity,
    size_t client_identity_len, const uint8_t *server_identity,
    size_t server_identity_len, uint32_t error_limit, const uint8_t *w0,
    size_t w0_len, const uint8_t *w1, size_t w1_len) {
  if (w0_len != spake2plus::kVerifierSize ||
      w1_len != spake2plus::kVerifierSize ||
      (context == nullptr && context_len != 0)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_SPAKE2PLUSV1_VALUE);
    return nullptr;
  }

  UniquePtr<SSLCredential> cred = ssl_credential_new_spake2plusv1(
      SSLCredentialType::kSPAKE2PlusV1Client, Span(context, context_len),
      Span(client_identity, client_identity_len),
      Span(server_identity, server_identity_len), error_limit);
  if (!cred) {
    return nullptr;
  }

  if (!cred->password_verifier_w0.CopyFrom(Span(w0, w0_len)) ||
      !cred->password_verifier_w1.CopyFrom(Span(w1, w1_len))) {
    return nullptr;
  }

  return cred.release();
}

SSL_CREDENTIAL *SSL_CREDENTIAL_new_spake2plusv1_server(
    const uint8_t *context, size_t context_len, const uint8_t *client_identity,
    size_t client_identity_len, const uint8_t *server_identity,
    size_t server_identity_len, uint32_t rate_limit, const uint8_t *w0,
    size_t w0_len, const uint8_t *registration_record,
    size_t registration_record_len) {
  if (w0_len != spake2plus::kVerifierSize ||
      registration_record_len != spake2plus::kRegistrationRecordSize ||
      (context == nullptr && context_len != 0)) {
    return nullptr;
  }

  UniquePtr<SSLCredential> cred = ssl_credential_new_spake2plusv1(
      SSLCredentialType::kSPAKE2PlusV1Server, Span(context, context_len),
      Span(client_identity, client_identity_len),
      Span(server_identity, server_identity_len), rate_limit);
  if (!cred) {
    return nullptr;
  }

  if (!cred->password_verifier_w0.CopyFrom(Span(w0, w0_len)) ||
      !cred->registration_record.CopyFrom(
          Span(registration_record, registration_record_len))) {
    return nullptr;
  }

  return cred.release();
}

int SSL_CTX_add1_credential(SSL_CTX *ctx, const SSL_CREDENTIAL *cred) {
  auto *cred_impl = FromOpaque(cred);
  if (!cred_impl->IsComplete()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }
  return FromOpaque(ctx)->cert->credentials.Push(UpRef(cred_impl));
}

int SSL_add1_credential(SSL *ssl, const SSL_CREDENTIAL *cred) {
  auto *cred_impl = FromOpaque(cred);
  if (ssl->config == nullptr) {
    return 0;
  }

  if (!cred_impl->IsComplete()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }
  return ssl->config->cert->credentials.Push(UpRef(cred_impl));
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
  return CRYPTO_set_ex_data(&FromOpaque(cred)->ex_data, idx, arg);
}

void *SSL_CREDENTIAL_get_ex_data(const SSL_CREDENTIAL *cred, int idx) {
  return CRYPTO_get_ex_data(&FromOpaque(cred)->ex_data, idx);
}

void SSL_CREDENTIAL_set_must_match_issuer(SSL_CREDENTIAL *cred, int match) {
  FromOpaque(cred)->must_match_issuer = !!match;
}

int SSL_CREDENTIAL_set1_trust_anchor_id(SSL_CREDENTIAL *cred, const uint8_t *id,
                                        size_t id_len) {
  auto *cred_impl = FromOpaque(cred);
  // For now, this is only valid for X.509.
  if (!cred_impl->UsesX509()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  if (!cred_impl->trust_anchor_id.CopyFrom(Span(id, id_len))) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return 0;
  }

  return 1;
}

int SSL_CREDENTIAL_set1_certificate_properties(
    SSL_CREDENTIAL *cred, CRYPTO_BUFFER *cert_property_list) {
  auto *cred_impl = FromOpaque(cred);
  // For now, this is only valid for X.509.
  if (!cred_impl->UsesX509()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }
  std::optional<CBS> trust_anchor;
  CBS cbs, cpl;
  CRYPTO_BUFFER_init_CBS(cert_property_list, &cbs);

  if (!CBS_get_u16_length_prefixed(&cbs, &cpl)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_CERTIFICATE_PROPERTY_LIST);
    return 0;
  }
  while (CBS_len(&cpl) != 0) {
    uint16_t cp_type;
    CBS cp_data;
    if (!CBS_get_u16(&cpl, &cp_type) ||
        !CBS_get_u16_length_prefixed(&cpl, &cp_data)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_CERTIFICATE_PROPERTY_LIST);
      return 0;
    }
    switch (cp_type) {
      case 0:  // trust anchor identifier.
        if (trust_anchor.has_value()) {
          OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_CERTIFICATE_PROPERTY_LIST);
          return 0;
        }
        trust_anchor = cp_data;
        break;
      default:
        break;
    }
  }
  if (CBS_len(&cbs) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_CERTIFICATE_PROPERTY_LIST);
    return 0;
  }
  // Certificate property list has parsed correctly.

  // We do not currently retain |cert_property_list|, but if we define another
  // property with larger fields (e.g. stapled SCTs), it may make sense for
  // those fields to retain |cert_property_list| and alias into it.
  if (trust_anchor.has_value()) {
    if (!CBS_len(&trust_anchor.value())) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_TRUST_ANCHOR_LIST);
      return 0;
    }
    if (!SSL_CREDENTIAL_set1_trust_anchor_id(cred_impl,
                                             CBS_data(&trust_anchor.value()),
                                             CBS_len(&trust_anchor.value()))) {
      return 0;
    }
  }
  return 1;
}
