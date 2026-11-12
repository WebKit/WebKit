// Copyright 2018 The BoringSSL Authors
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

#include <openssl/ssl.h>

#include <openssl/bytestring.h>
#include <openssl/err.h>

#include <algorithm>
#include <optional>

#include "../crypto/internal.h"
#include "internal.h"


BSSL_NAMESPACE_BEGIN

static const CBS_ASN1_TAG kFeatureTagALPS = CBS_ASN1_CONTEXT_SPECIFIC | 0;

// early_data_t represents the state of early data in a more compact way than
// the 3 bits used by the implementation.
enum early_data_t {
  early_data_not_offered = 0,
  early_data_accepted = 1,
  early_data_rejected_hrr = 2,
  early_data_skipped = 3,

  early_data_max_value = early_data_skipped,
};

// serialize_features adds a description of features supported by this binary to
// `out`.  Returns true on success and false on error.
static bool serialize_features(CBB *out) {
  CBB ciphers;
  if (!CBB_add_asn1(out, &ciphers, CBS_ASN1_OCTETSTRING)) {
    return false;
  }
  Span<const SSL_CIPHER> all_ciphers = AllCiphers();
  for (const SSL_CIPHER &cipher : all_ciphers) {
    if (!CBB_add_u16(&ciphers, cipher.protocol_id)) {
      return false;
    }
  }
  CBB groups;
  if (!CBB_add_asn1(out, &groups, CBS_ASN1_OCTETSTRING)) {
    return false;
  }
  for (const NamedGroup &g : NamedGroups()) {
    if (!CBB_add_u16(&groups, g.group_id)) {
      return false;
    }
  }
  // ALPS is a draft protocol and may change over time. The features structure
  // contains a [0] IMPLICIT OCTET STRING OPTIONAL, containing a list of u16
  // ALPS versions that the binary supports. For now we name them by codepoint.
  // Once ALPS is finalized and past the support horizon, this field can be
  // removed.
  CBB alps;
  if (!CBB_add_asn1(out, &alps, kFeatureTagALPS) ||
      !CBB_add_u16(&alps, TLSEXT_TYPE_application_settings_old) ||
      !CBB_add_u16(&alps, TLSEXT_TYPE_application_settings)) {
    return false;
  }
  return CBB_flush(out);
}

// apply_remote_features reads a list of supported features from `in` and
// (possibly) reconfigures `ssl` to disallow the negotiation of features whose
// support has not been indicated. (This prevents the hints from being generated
// based on features that are not supported on the actual server.)
static bool apply_remote_features(SSLImpl *ssl, CBS *in) {
  CBS ciphers;
  if (!CBS_get_asn1(in, &ciphers, CBS_ASN1_OCTETSTRING)) {
    return false;
  }
  bssl::UniquePtr<STACK_OF(SSL_CIPHER)> supported(sk_SSL_CIPHER_new_null());
  if (!supported) {
    return false;
  }
  while (CBS_len(&ciphers)) {
    uint16_t id;
    if (!CBS_get_u16(&ciphers, &id)) {
      return false;
    }
    const SSL_CIPHER *cipher = SSL_get_cipher_by_value(id);
    if (!cipher) {
      continue;
    }
    if (!sk_SSL_CIPHER_push(supported.get(), cipher)) {
      return false;
    }
  }
  const STACK_OF(SSL_CIPHER) *configured =
      ssl->config->cipher_list ? ssl->config->cipher_list->ciphers()
                               : ssl->ctx->cipher_list->ciphers();
  bssl::UniquePtr<STACK_OF(SSL_CIPHER)> unsupported(sk_SSL_CIPHER_new_null());
  if (!unsupported) {
    return false;
  }
  for (const SSL_CIPHER *configured_cipher : configured) {
    if (sk_SSL_CIPHER_find(supported.get(), nullptr, configured_cipher)) {
      continue;
    }
    if (!sk_SSL_CIPHER_push(unsupported.get(), configured_cipher)) {
      return false;
    }
  }
  if (sk_SSL_CIPHER_num(unsupported.get()) && !ssl->config->cipher_list) {
    ssl->config->cipher_list = bssl::MakeUnique<SSLCipherPreferenceList>();
    if (!ssl->config->cipher_list ||
        !ssl->config->cipher_list->CopyFrom(*ssl->ctx->cipher_list)) {
      return false;
    }
  }
  for (const SSL_CIPHER *unsupported_cipher : unsupported.get()) {
    ssl->config->cipher_list->Remove(unsupported_cipher);
  }
  if (sk_SSL_CIPHER_num(SSL_get_ciphers(ssl)) == 0) {
    return false;
  }

  CBS groups;
  if (!CBS_get_asn1(in, &groups, CBS_ASN1_OCTETSTRING)) {
    return false;
  }
  Array<uint16_t> supported_groups;
  if (!supported_groups.InitForOverwrite(CBS_len(&groups) / 2)) {
    return false;
  }
  size_t idx = 0;
  while (CBS_len(&groups)) {
    uint16_t group;
    if (!CBS_get_u16(&groups, &group)) {
      return false;
    }
    supported_groups[idx++] = group;
  }
  Span<const uint16_t> configured_groups =
      ssl->s3->hs->config->supported_group_list;
  Span<const uint32_t> configured_groups_flags =
      ssl->s3->hs->config->supported_group_list_flags;
  Array<uint16_t> new_configured_groups;
  // Temporary scratch space to keep track of consecutive runs of groups of
  // equal preference, so they can be relabeled after filtering for groups
  // present in the remote features and copying to the new configuration.
  Array<size_t> new_groups_preference_ranks;
  Array<uint32_t> new_groups_flags;
  if (!new_configured_groups.InitForOverwrite(configured_groups.size()) ||
      !new_groups_preference_ranks.Init(configured_groups.size()) ||
      !new_groups_flags.Init(configured_groups.size())) {
    return false;
  }
  // Populate temporary array containing "preference rank" of each group.
  for (size_t i = 1; i < configured_groups.size(); ++i) {
    new_groups_preference_ranks[i] = new_groups_preference_ranks[i - 1];
    if ((configured_groups_flags[i - 1] &
         SSL_GROUP_FLAG_EQUAL_PREFERENCE_WITH_NEXT) == 0) {
      ++new_groups_preference_ranks[i];
    }
  }
  idx = 0;
  std::optional<size_t> last_rank;
  for (size_t i = 0u; i < configured_groups.size(); ++i) {
    uint16_t configured_group = configured_groups[i];
    if (std::find(supported_groups.begin(), supported_groups.end(),
                  configured_group) == supported_groups.end()) {
      continue;
    }
    new_configured_groups[idx] = configured_group;
    // Set the "equal preference with next" flag on the previous group, if
    // appropriate.
    if (last_rank.has_value() && new_groups_preference_ranks[i] == *last_rank) {
      new_groups_flags[idx - 1] |= SSL_GROUP_FLAG_EQUAL_PREFERENCE_WITH_NEXT;
    }
    last_rank = new_groups_preference_ranks[i];
    ++idx;
  }
  if (idx == 0) {
    return false;
  }
  new_configured_groups.Shrink(idx);
  new_groups_flags.Shrink(idx);
  ssl->config->supported_group_list = std::move(new_configured_groups);
  ssl->config->supported_group_list_flags = std::move(new_groups_flags);

  CBS alps;
  CBS_init(&alps, nullptr, 0);
  if (!CBS_get_optional_asn1(in, &alps, /*out_present=*/nullptr,
                             kFeatureTagALPS)) {
    return false;
  }
  bool supports_alps = false;
  while (CBS_len(&alps) != 0) {
    uint16_t id;
    if (!CBS_get_u16(&alps, &id)) {
      return false;
    }
    // For now, we support two ALPS codepoints, so we need to extract both
    // codepoints, and then filter what the handshaker might try to send.
    if ((id == TLSEXT_TYPE_application_settings &&
         ssl->config->alps_use_new_codepoint) ||
        (id == TLSEXT_TYPE_application_settings_old &&
         !ssl->config->alps_use_new_codepoint)) {
      supports_alps = true;
      break;
    }
  }
  if (!supports_alps) {
    ssl->config->alps_configs.clear();
  }

  return true;
}

BSSL_NAMESPACE_END

using namespace bssl;

int SSL_serialize_capabilities(const SSL *ssl, CBB *out) {
  CBB seq;
  if (!CBB_add_asn1(out, &seq, CBS_ASN1_SEQUENCE) ||
      !serialize_features(&seq) ||  //
      !CBB_flush(out)) {
    return 0;
  }

  return 1;
}

int SSL_request_handshake_hints(SSL *ssl, const uint8_t *client_hello,
                                size_t client_hello_len,
                                const uint8_t *capabilities,
                                size_t capabilities_len) {
  auto *ssl_impl = FromOpaque(ssl);
  if (SSL_is_dtls(ssl_impl) || ssl_impl->s3->hs == nullptr) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  CBS cbs, seq;
  CBS_init(&cbs, capabilities, capabilities_len);
  UniquePtr<SSL_HANDSHAKE_HINTS> pending_hints =
      MakeUnique<SSL_HANDSHAKE_HINTS>();
  if (pending_hints == nullptr ||
      !CBS_get_asn1(&cbs, &seq, CBS_ASN1_SEQUENCE) ||
      !apply_remote_features(ssl_impl, &seq)) {
    return 0;
  }

  SSL3_STATE *const s3 = ssl_impl->s3;
  s3->v2_hello_done = true;
  s3->has_message = true;

  Array<uint8_t> client_hello_msg;
  ScopedCBB client_hello_cbb;
  CBB client_hello_body;
  if (!ssl_impl->method->init_message(ssl_impl, client_hello_cbb.get(),
                                      &client_hello_body,
                                      SSL3_MT_CLIENT_HELLO) ||
      !CBB_add_bytes(&client_hello_body, client_hello, client_hello_len) ||
      !ssl_impl->method->finish_message(ssl_impl, client_hello_cbb.get(),
                                        &client_hello_msg)) {
    return 0;
  }

  s3->hs_buf.reset(BUF_MEM_new());
  if (!s3->hs_buf || !BUF_MEM_append(s3->hs_buf.get(), client_hello_msg.data(),
                                     client_hello_msg.size())) {
    return 0;
  }

  s3->hs->pending_hints = std::move(pending_hints);
  return 1;
}

// `SSL_HANDSHAKE_HINTS` is serialized as the following ASN.1 structure. We use
// implicit tagging to make it a little more compact.
//
// HandshakeHints ::= SEQUENCE {
//     serverRandomTLS13       [0] IMPLICIT OCTET STRING OPTIONAL,
//     keyShareHint            [1] IMPLICIT KeyShareHint OPTIONAL,
//     signatureHint           [2] IMPLICIT SignatureHint OPTIONAL,
//     -- At most one of decryptedPSKHint or ignorePSKHint may be present. It
//     -- corresponds to the first entry in pre_shared_keys. TLS 1.2 session
//     -- tickets use a separate hint, to ensure the caller does not apply the
//     -- hint to the wrong field.
//     decryptedPSKHint        [3] IMPLICIT OCTET STRING OPTIONAL,
//     ignorePSKHint           [4] IMPLICIT NULL OPTIONAL,
//     -- Due to a historical typo, compressCertificateHint's tag is primitive
//     -- instead of constructed.
//     compressCertificateHint [5 PRIMITIVE] IMPLICIT CompressCertificateHint
//                                 OPTIONAL,
//     -- TLS 1.2 and 1.3 use different server random hints because one contains
//     -- a timestamp while the other doesn't. If the hint was generated
//     -- assuming TLS 1.3 but we actually negotiate TLS 1.2, mixing the two
//     -- will break this.
//     serverRandomTLS12       [6] IMPLICIT OCTET STRING OPTIONAL,
//     -- Due to a historical typo, ecdheHint tag has universal class instead of
//     -- context-specific.
//     ecdheHint               [UNIVERSAL 7] IMPLICIT ECDHEHint OPTIONAL
//     -- At most one of decryptedTicketHint or ignoreTicketHint may be present.
//     -- renewTicketHint requires decryptedTicketHint.
//     decryptedTicketHint     [8] IMPLICIT OCTET STRING OPTIONAL,
//     renewTicketHint         [9] IMPLICIT NULL OPTIONAL,
//     ignoreTicketHint       [10] IMPLICIT NULL OPTIONAL,
//     -- Unlike a usual ASN.1 structure, trailing data is ignored.
// }
//
// KeyShareHint ::= SEQUENCE {
//     groupId                 INTEGER,
//     ciphertext              OCTET STRING,
//     secret                  OCTET STRING,
// }
//
// SignatureHint ::= SEQUENCE {
//     algorithm               INTEGER,
//     input                   OCTET STRING,
//     subjectPublicKeyInfo    OCTET STRING,
//     signature               OCTET STRING,
// }
//
// CompressCertificateHint ::= SEQUENCE {
//     algorithm               INTEGER,
//     input                   OCTET STRING,
//     compressed              OCTET STRING,
// }
//
// ECDHEHint ::= SEQUENCE {
//     groupId                 INTEGER,
//     publicKey               OCTET STRING,
//     privateKey              OCTET STRING,
// }

// HandshakeHints tags.
static const CBS_ASN1_TAG kServerRandomTLS13Tag = CBS_ASN1_CONTEXT_SPECIFIC | 0;
static const CBS_ASN1_TAG kKeyShareHintTag =
    CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC | 1;
static const CBS_ASN1_TAG kSignatureHintTag =
    CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC | 2;
static const CBS_ASN1_TAG kDecryptedPSKTag = CBS_ASN1_CONTEXT_SPECIFIC | 3;
static const CBS_ASN1_TAG kIgnorePSKTag = CBS_ASN1_CONTEXT_SPECIFIC | 4;
static const CBS_ASN1_TAG kCompressCertificateTag =
    CBS_ASN1_CONTEXT_SPECIFIC | 5;
static const CBS_ASN1_TAG kServerRandomTLS12Tag = CBS_ASN1_CONTEXT_SPECIFIC | 6;
static const CBS_ASN1_TAG kECDHEHintTag = CBS_ASN1_CONSTRUCTED | 7;
static const CBS_ASN1_TAG kDecryptedTicketTag = CBS_ASN1_CONTEXT_SPECIFIC | 8;
static const CBS_ASN1_TAG kRenewTicketTag = CBS_ASN1_CONTEXT_SPECIFIC | 9;
static const CBS_ASN1_TAG kIgnoreTicketTag = CBS_ASN1_CONTEXT_SPECIFIC | 10;

int SSL_serialize_handshake_hints(const SSL *ssl, CBB *out) {
  const auto *ssl_impl = FromOpaque(ssl);
  const SSL_HANDSHAKE *hs = ssl_impl->s3->hs.get();
  if (!ssl_impl->server || hs == nullptr || hs->pending_hints == nullptr) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  const SSL_HANDSHAKE_HINTS *const hints = hs->pending_hints.get();
  CBB seq, child;
  if (!CBB_add_asn1(out, &seq, CBS_ASN1_SEQUENCE)) {
    return 0;
  }

  if (!hints->server_random_tls13.empty()) {
    if (!CBB_add_asn1_element(&seq, kServerRandomTLS13Tag,
                              hints->server_random_tls13.data(),
                              hints->server_random_tls13.size())) {
      return 0;
    }
  }

  if (hints->key_share_group_id != 0 && !hints->key_share_ciphertext.empty() &&
      !hints->key_share_secret.empty()) {
    if (!CBB_add_asn1(&seq, &child, kKeyShareHintTag) ||
        !CBB_add_asn1_uint64(&child, hints->key_share_group_id) ||
        !CBB_add_asn1_octet_string(&child, hints->key_share_ciphertext.data(),
                                   hints->key_share_ciphertext.size()) ||
        !CBB_add_asn1_octet_string(&child, hints->key_share_secret.data(),
                                   hints->key_share_secret.size())) {
      return 0;
    }
  }

  if (hints->signature_algorithm != 0 && !hints->signature_input.empty() &&
      !hints->signature.empty()) {
    if (!CBB_add_asn1(&seq, &child, kSignatureHintTag) ||
        !CBB_add_asn1_uint64(&child, hints->signature_algorithm) ||
        !CBB_add_asn1_octet_string(&child, hints->signature_input.data(),
                                   hints->signature_input.size()) ||
        !CBB_add_asn1_octet_string(&child, hints->signature_spki.data(),
                                   hints->signature_spki.size()) ||
        !CBB_add_asn1_octet_string(&child, hints->signature.data(),
                                   hints->signature.size())) {
      return 0;
    }
  }

  if (!hints->decrypted_psk.empty()) {
    if (!CBB_add_asn1_element(&seq, kDecryptedPSKTag,
                              hints->decrypted_psk.data(),
                              hints->decrypted_psk.size())) {
      return 0;
    }
  }

  if (hints->ignore_psk &&  //
      !CBB_add_asn1(&seq, &child, kIgnorePSKTag)) {
    return 0;
  }

  if (hints->cert_compression_alg_id != 0 &&
      !hints->cert_compression_input.empty() &&
      !hints->cert_compression_output.empty()) {
    if (!CBB_add_asn1(&seq, &child, kCompressCertificateTag) ||
        !CBB_add_asn1_uint64(&child, hints->cert_compression_alg_id) ||
        !CBB_add_asn1_octet_string(&child, hints->cert_compression_input.data(),
                                   hints->cert_compression_input.size()) ||
        !CBB_add_asn1_octet_string(&child,
                                   hints->cert_compression_output.data(),
                                   hints->cert_compression_output.size())) {
      return 0;
    }
  }

  if (!hints->server_random_tls12.empty()) {
    if (!CBB_add_asn1_element(&seq, kServerRandomTLS12Tag,
                              hints->server_random_tls12.data(),
                              hints->server_random_tls12.size())) {
      return 0;
    }
  }

  if (hints->ecdhe_group_id != 0 && !hints->ecdhe_public_key.empty() &&
      !hints->ecdhe_private_key.empty()) {
    if (!CBB_add_asn1(&seq, &child, kECDHEHintTag) ||
        !CBB_add_asn1_uint64(&child, hints->ecdhe_group_id) ||
        !CBB_add_asn1_octet_string(&child, hints->ecdhe_public_key.data(),
                                   hints->ecdhe_public_key.size()) ||
        !CBB_add_asn1_octet_string(&child, hints->ecdhe_private_key.data(),
                                   hints->ecdhe_private_key.size())) {
      return 0;
    }
  }


  if (!hints->decrypted_ticket.empty()) {
    if (!CBB_add_asn1_element(&seq, kDecryptedTicketTag,
                              hints->decrypted_ticket.data(),
                              hints->decrypted_ticket.size())) {
      return 0;
    }
  }

  if (hints->renew_ticket &&  //
      !CBB_add_asn1(&seq, &child, kRenewTicketTag)) {
    return 0;
  }

  if (hints->ignore_ticket &&  //
      !CBB_add_asn1(&seq, &child, kIgnoreTicketTag)) {
    return 0;
  }

  return CBB_flush(out);
}

static bool get_optional_implicit_null(CBS *cbs, bool *out_present,
                                       CBS_ASN1_TAG tag) {
  CBS value;
  int present;
  if (!CBS_get_optional_asn1(cbs, &value, &present, tag) ||
      (present && CBS_len(&value) != 0)) {
    return false;
  }
  *out_present = present;
  return true;
}

int SSL_set_handshake_hints(SSL *ssl, const uint8_t *hints, size_t hints_len) {
  auto *ssl_impl = FromOpaque(ssl);
  if (SSL_is_dtls(ssl_impl) || ssl_impl->s3->hs == nullptr) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  UniquePtr<SSL_HANDSHAKE_HINTS> hints_obj = MakeUnique<SSL_HANDSHAKE_HINTS>();
  if (hints_obj == nullptr) {
    return 0;
  }

  CBS cbs, seq, server_random_tls13, key_share, signature_hint, psk,
      cert_compression, server_random_tls12, ecdhe, ticket;
  int has_server_random_tls13, has_key_share, has_signature_hint, has_psk,
      has_cert_compression, has_server_random_tls12, has_ecdhe, has_ticket;
  CBS_init(&cbs, hints, hints_len);
  if (!CBS_get_asn1(&cbs, &seq, CBS_ASN1_SEQUENCE) ||
      !CBS_get_optional_asn1(&seq, &server_random_tls13,
                             &has_server_random_tls13, kServerRandomTLS13Tag) ||
      !CBS_get_optional_asn1(&seq, &key_share, &has_key_share,
                             kKeyShareHintTag) ||
      !CBS_get_optional_asn1(&seq, &signature_hint, &has_signature_hint,
                             kSignatureHintTag) ||
      !CBS_get_optional_asn1(&seq, &psk, &has_psk, kDecryptedPSKTag) ||
      !get_optional_implicit_null(&seq, &hints_obj->ignore_psk,
                                  kIgnorePSKTag) ||
      !CBS_get_optional_asn1(&seq, &cert_compression, &has_cert_compression,
                             kCompressCertificateTag) ||
      !CBS_get_optional_asn1(&seq, &server_random_tls12,
                             &has_server_random_tls12, kServerRandomTLS12Tag) ||
      !CBS_get_optional_asn1(&seq, &ecdhe, &has_ecdhe, kECDHEHintTag) ||
      !CBS_get_optional_asn1(&seq, &ticket, &has_ticket, kDecryptedTicketTag) ||
      !get_optional_implicit_null(&seq, &hints_obj->renew_ticket,
                                  kRenewTicketTag) ||
      !get_optional_implicit_null(&seq, &hints_obj->ignore_ticket,
                                  kIgnoreTicketTag)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_COULD_NOT_PARSE_HINTS);
    return 0;
  }

  if (has_server_random_tls13 &&
      !hints_obj->server_random_tls13.CopyFrom(server_random_tls13)) {
    return 0;
  }

  if (has_key_share) {
    uint64_t group_id;
    CBS ciphertext, secret;
    if (!CBS_get_asn1_uint64(&key_share, &group_id) ||  //
        group_id == 0 || group_id > 0xffff ||
        !CBS_get_asn1(&key_share, &ciphertext, CBS_ASN1_OCTETSTRING) ||
        !hints_obj->key_share_ciphertext.CopyFrom(ciphertext) ||
        !CBS_get_asn1(&key_share, &secret, CBS_ASN1_OCTETSTRING) ||
        !hints_obj->key_share_secret.CopyFrom(secret)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_COULD_NOT_PARSE_HINTS);
      return 0;
    }
    hints_obj->key_share_group_id = static_cast<uint16_t>(group_id);
  }

  if (has_signature_hint) {
    uint64_t sig_alg;
    CBS input, spki, signature;
    if (!CBS_get_asn1_uint64(&signature_hint, &sig_alg) ||  //
        sig_alg == 0 || sig_alg > 0xffff ||
        !CBS_get_asn1(&signature_hint, &input, CBS_ASN1_OCTETSTRING) ||
        !hints_obj->signature_input.CopyFrom(input) ||
        !CBS_get_asn1(&signature_hint, &spki, CBS_ASN1_OCTETSTRING) ||
        !hints_obj->signature_spki.CopyFrom(spki) ||
        !CBS_get_asn1(&signature_hint, &signature, CBS_ASN1_OCTETSTRING) ||
        !hints_obj->signature.CopyFrom(signature)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_COULD_NOT_PARSE_HINTS);
      return 0;
    }
    hints_obj->signature_algorithm = static_cast<uint16_t>(sig_alg);
  }

  if (has_psk && !hints_obj->decrypted_psk.CopyFrom(psk)) {
    return 0;
  }
  if (has_psk && hints_obj->ignore_psk) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_COULD_NOT_PARSE_HINTS);
    return 0;
  }

  if (has_cert_compression) {
    uint64_t alg;
    CBS input, output;
    if (!CBS_get_asn1_uint64(&cert_compression, &alg) ||  //
        alg == 0 || alg > 0xffff ||
        !CBS_get_asn1(&cert_compression, &input, CBS_ASN1_OCTETSTRING) ||
        !hints_obj->cert_compression_input.CopyFrom(input) ||
        !CBS_get_asn1(&cert_compression, &output, CBS_ASN1_OCTETSTRING) ||
        !hints_obj->cert_compression_output.CopyFrom(output)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_COULD_NOT_PARSE_HINTS);
      return 0;
    }
    hints_obj->cert_compression_alg_id = static_cast<uint16_t>(alg);
  }

  if (has_server_random_tls12 &&
      !hints_obj->server_random_tls12.CopyFrom(server_random_tls12)) {
    return 0;
  }

  if (has_ecdhe) {
    uint64_t group_id;
    CBS public_key, private_key;
    if (!CBS_get_asn1_uint64(&ecdhe, &group_id) ||  //
        group_id == 0 || group_id > 0xffff ||
        !CBS_get_asn1(&ecdhe, &public_key, CBS_ASN1_OCTETSTRING) ||
        !hints_obj->ecdhe_public_key.CopyFrom(public_key) ||
        !CBS_get_asn1(&ecdhe, &private_key, CBS_ASN1_OCTETSTRING) ||
        !hints_obj->ecdhe_private_key.CopyFrom(private_key)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_COULD_NOT_PARSE_HINTS);
      return 0;
    }
    hints_obj->ecdhe_group_id = static_cast<uint16_t>(group_id);
  }

  if (has_ticket && !hints_obj->decrypted_ticket.CopyFrom(ticket)) {
    return 0;
  }
  if (has_ticket && hints_obj->ignore_ticket) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_COULD_NOT_PARSE_HINTS);
    return 0;
  }
  if (!has_ticket && hints_obj->renew_ticket) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_COULD_NOT_PARSE_HINTS);
    return 0;
  }

  ssl_impl->s3->hs->provided_hints = std::move(hints_obj);
  return 1;
}
