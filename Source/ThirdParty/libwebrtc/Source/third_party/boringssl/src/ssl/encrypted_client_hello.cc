/* Copyright (c) 2021, Google Inc.
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

#include <openssl/bytestring.h>
#include <openssl/curve25519.h>
#include <openssl/err.h>
#include <openssl/hkdf.h>
#include <openssl/hpke.h>

#include "internal.h"


#if defined(OPENSSL_MSAN)
#define NO_SANITIZE_MEMORY __attribute__((no_sanitize("memory")))
#else
#define NO_SANITIZE_MEMORY
#endif

BSSL_NAMESPACE_BEGIN

static const decltype(&EVP_hpke_aes_128_gcm) kSupportedAEADs[] = {
    &EVP_hpke_aes_128_gcm,
    &EVP_hpke_aes_256_gcm,
    &EVP_hpke_chacha20_poly1305,
};

static const EVP_HPKE_AEAD *get_ech_aead(uint16_t aead_id) {
  for (const auto aead_func : kSupportedAEADs) {
    const EVP_HPKE_AEAD *aead = aead_func();
    if (aead_id == EVP_HPKE_AEAD_id(aead)) {
      return aead;
    }
  }
  return nullptr;
}

// ssl_client_hello_write_without_extensions serializes |client_hello| into
// |out|, omitting the length-prefixed extensions. It serializes individual
// fields, starting with |client_hello->version|, and ignores the
// |client_hello->client_hello| field. It returns true on success and false on
// failure.
static bool ssl_client_hello_write_without_extensions(
    const SSL_CLIENT_HELLO *client_hello, CBB *out) {
  CBB cbb;
  if (!CBB_add_u16(out, client_hello->version) ||
      !CBB_add_bytes(out, client_hello->random, client_hello->random_len) ||
      !CBB_add_u8_length_prefixed(out, &cbb) ||
      !CBB_add_bytes(&cbb, client_hello->session_id,
                     client_hello->session_id_len) ||
      !CBB_add_u16_length_prefixed(out, &cbb) ||
      !CBB_add_bytes(&cbb, client_hello->cipher_suites,
                     client_hello->cipher_suites_len) ||
      !CBB_add_u8_length_prefixed(out, &cbb) ||
      !CBB_add_bytes(&cbb, client_hello->compression_methods,
                     client_hello->compression_methods_len) ||
      !CBB_flush(out)) {
    return false;
  }
  return true;
}

bool ssl_decode_client_hello_inner(
    SSL *ssl, uint8_t *out_alert, Array<uint8_t> *out_client_hello_inner,
    Span<const uint8_t> encoded_client_hello_inner,
    const SSL_CLIENT_HELLO *client_hello_outer) {
  SSL_CLIENT_HELLO client_hello_inner;
  if (!ssl_client_hello_init(ssl, &client_hello_inner,
                             encoded_client_hello_inner)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return false;
  }
  // TLS 1.3 ClientHellos must have extensions, and EncodedClientHelloInners use
  // ClientHelloOuter's session_id.
  if (client_hello_inner.extensions_len == 0 ||
      client_hello_inner.session_id_len != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return false;
  }
  client_hello_inner.session_id = client_hello_outer->session_id;
  client_hello_inner.session_id_len = client_hello_outer->session_id_len;

  // Begin serializing a message containing the ClientHelloInner in |cbb|.
  ScopedCBB cbb;
  CBB body, extensions;
  if (!ssl->method->init_message(ssl, cbb.get(), &body, SSL3_MT_CLIENT_HELLO) ||
      !ssl_client_hello_write_without_extensions(&client_hello_inner, &body) ||
      !CBB_add_u16_length_prefixed(&body, &extensions)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  // Sort the extensions in ClientHelloOuter, so ech_outer_extensions may be
  // processed in O(n*log(n)) time, rather than O(n^2).
  struct Extension {
    uint16_t extension = 0;
    Span<const uint8_t> body;
    bool copied = false;
  };

  // MSan's libc interceptors do not handle |bsearch|. See b/182583130.
  auto compare_extension = [](const void *a, const void *b)
                               NO_SANITIZE_MEMORY -> int {
    const Extension *extension_a = reinterpret_cast<const Extension *>(a);
    const Extension *extension_b = reinterpret_cast<const Extension *>(b);
    if (extension_a->extension < extension_b->extension) {
      return -1;
    } else if (extension_a->extension > extension_b->extension) {
      return 1;
    }
    return 0;
  };
  GrowableArray<Extension> sorted_extensions;
  CBS unsorted_extensions(MakeConstSpan(client_hello_outer->extensions,
                                        client_hello_outer->extensions_len));
  while (CBS_len(&unsorted_extensions) > 0) {
    Extension extension;
    CBS extension_body;
    if (!CBS_get_u16(&unsorted_extensions, &extension.extension) ||
        !CBS_get_u16_length_prefixed(&unsorted_extensions, &extension_body)) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return false;
    }
    extension.body = extension_body;
    if (!sorted_extensions.Push(extension)) {
      return false;
    }
  }
  qsort(sorted_extensions.data(), sorted_extensions.size(), sizeof(Extension),
        compare_extension);

  // Copy extensions from |client_hello_inner|, expanding ech_outer_extensions.
  CBS inner_extensions(MakeConstSpan(client_hello_inner.extensions,
                                     client_hello_inner.extensions_len));
  while (CBS_len(&inner_extensions) > 0) {
    uint16_t extension_id;
    CBS extension_body;
    if (!CBS_get_u16(&inner_extensions, &extension_id) ||
        !CBS_get_u16_length_prefixed(&inner_extensions, &extension_body)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      return false;
    }
    if (extension_id != TLSEXT_TYPE_ech_outer_extensions) {
      if (!CBB_add_u16(&extensions, extension_id) ||
          !CBB_add_u16(&extensions, CBS_len(&extension_body)) ||
          !CBB_add_bytes(&extensions, CBS_data(&extension_body),
                         CBS_len(&extension_body))) {
        OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
        return false;
      }
      continue;
    }

    // Replace ech_outer_extensions with the corresponding outer extensions.
    CBS outer_extensions;
    if (!CBS_get_u8_length_prefixed(&extension_body, &outer_extensions) ||
        CBS_len(&extension_body) != 0) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      return false;
    }
    while (CBS_len(&outer_extensions) > 0) {
      uint16_t extension_needed;
      if (!CBS_get_u16(&outer_extensions, &extension_needed)) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
        return false;
      }
      if (extension_needed == TLSEXT_TYPE_encrypted_client_hello) {
        *out_alert = SSL_AD_ILLEGAL_PARAMETER;
        OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
        return false;
      }
      // Find the referenced extension.
      Extension key;
      key.extension = extension_needed;
      Extension *result = reinterpret_cast<Extension *>(
          bsearch(&key, sorted_extensions.data(), sorted_extensions.size(),
                  sizeof(Extension), compare_extension));
      if (result == nullptr) {
        *out_alert = SSL_AD_ILLEGAL_PARAMETER;
        OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
        return false;
      }

      // Extensions may be referenced at most once, to bound the result size.
      if (result->copied) {
        *out_alert = SSL_AD_ILLEGAL_PARAMETER;
        OPENSSL_PUT_ERROR(SSL, SSL_R_DUPLICATE_EXTENSION);
        return false;
      }
      result->copied = true;

      if (!CBB_add_u16(&extensions, extension_needed) ||
          !CBB_add_u16(&extensions, result->body.size()) ||
          !CBB_add_bytes(&extensions, result->body.data(),
                         result->body.size())) {
        OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
        return false;
      }
    }
  }
  if (!CBB_flush(&body)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  // See https://github.com/tlswg/draft-ietf-tls-esni/pull/411
  CBS extension;
  if (!ssl_client_hello_init(ssl, &client_hello_inner,
                             MakeConstSpan(CBB_data(&body), CBB_len(&body))) ||
      !ssl_client_hello_get_extension(&client_hello_inner, &extension,
                                      TLSEXT_TYPE_ech_is_inner) ||
      CBS_len(&extension) != 0 ||
      ssl_client_hello_get_extension(&client_hello_inner, &extension,
                                     TLSEXT_TYPE_encrypted_client_hello) ||
      !ssl_client_hello_get_extension(&client_hello_inner, &extension,
                                      TLSEXT_TYPE_supported_versions)) {
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_CLIENT_HELLO_INNER);
    return false;
  }
  // Parse supported_versions and reject TLS versions prior to TLS 1.3. Older
  // versions are incompatible with ECH.
  CBS versions;
  if (!CBS_get_u8_length_prefixed(&extension, &versions) ||
      CBS_len(&extension) != 0 ||  //
      CBS_len(&versions) == 0) {
    *out_alert = SSL_AD_DECODE_ERROR;
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return false;
  }
  while (CBS_len(&versions) != 0) {
    uint16_t version;
    if (!CBS_get_u16(&versions, &version)) {
      *out_alert = SSL_AD_DECODE_ERROR;
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      return false;
    }
    if (version == SSL3_VERSION || version == TLS1_VERSION ||
        version == TLS1_1_VERSION || version == TLS1_2_VERSION ||
        version == DTLS1_VERSION || version == DTLS1_2_VERSION) {
      *out_alert = SSL_AD_ILLEGAL_PARAMETER;
      OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_CLIENT_HELLO_INNER);
      return false;
    }
  }

  if (!ssl->method->finish_message(ssl, cbb.get(), out_client_hello_inner)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }
  return true;
}

bool ssl_client_hello_decrypt(
    EVP_HPKE_CTX *hpke_ctx, Array<uint8_t> *out_encoded_client_hello_inner,
    bool *out_is_decrypt_error, const SSL_CLIENT_HELLO *client_hello_outer,
    uint16_t kdf_id, uint16_t aead_id, const uint8_t config_id,
    Span<const uint8_t> enc, Span<const uint8_t> payload) {
  *out_is_decrypt_error = false;

  // Compute the ClientHello portion of the ClientHelloOuterAAD value. See
  // draft-ietf-tls-esni-09, section 5.2.
  ScopedCBB ch_outer_aad_cbb;
  CBB enc_cbb, outer_hello_cbb, extensions_cbb;
  if (!CBB_init(ch_outer_aad_cbb.get(), 0) ||
      !CBB_add_u16(ch_outer_aad_cbb.get(), kdf_id) ||
      !CBB_add_u16(ch_outer_aad_cbb.get(), aead_id) ||
      !CBB_add_u8(ch_outer_aad_cbb.get(), config_id) ||
      !CBB_add_u16_length_prefixed(ch_outer_aad_cbb.get(), &enc_cbb) ||
      !CBB_add_bytes(&enc_cbb, enc.data(), enc.size()) ||
      !CBB_add_u24_length_prefixed(ch_outer_aad_cbb.get(), &outer_hello_cbb) ||
      !ssl_client_hello_write_without_extensions(client_hello_outer,
                                                 &outer_hello_cbb) ||
      !CBB_add_u16_length_prefixed(&outer_hello_cbb, &extensions_cbb)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return false;
  }

  CBS extensions(MakeConstSpan(client_hello_outer->extensions,
                               client_hello_outer->extensions_len));
  while (CBS_len(&extensions) > 0) {
    uint16_t extension_id;
    CBS extension_body;
    if (!CBS_get_u16(&extensions, &extension_id) ||
        !CBS_get_u16_length_prefixed(&extensions, &extension_body)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      return false;
    }
    if (extension_id == TLSEXT_TYPE_encrypted_client_hello) {
      continue;
    }
    if (!CBB_add_u16(&extensions_cbb, extension_id) ||
        !CBB_add_u16(&extensions_cbb, CBS_len(&extension_body)) ||
        !CBB_add_bytes(&extensions_cbb, CBS_data(&extension_body),
                       CBS_len(&extension_body))) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
      return false;
    }
  }
  if (!CBB_flush(ch_outer_aad_cbb.get())) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return false;
  }

  // Attempt to decrypt into |out_encoded_client_hello_inner|.
  if (!out_encoded_client_hello_inner->Init(payload.size())) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return false;
  }
  size_t encoded_client_hello_inner_len;
  if (!EVP_HPKE_CTX_open(hpke_ctx, out_encoded_client_hello_inner->data(),
                         &encoded_client_hello_inner_len,
                         out_encoded_client_hello_inner->size(), payload.data(),
                         payload.size(), CBB_data(ch_outer_aad_cbb.get()),
                         CBB_len(ch_outer_aad_cbb.get()))) {
    *out_is_decrypt_error = true;
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECRYPTION_FAILED);
    return false;
  }
  out_encoded_client_hello_inner->Shrink(encoded_client_hello_inner_len);
  return true;
}


bool ECHServerConfig::Init(Span<const uint8_t> ech_config,
                           Span<const uint8_t> private_key,
                           bool is_retry_config) {
  assert(!initialized_);
  is_retry_config_ = is_retry_config;

  if (!ech_config_.CopyFrom(ech_config)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return false;
  }
  // Read from |ech_config_| so we can save Spans with the same lifetime as |this|.
  CBS reader(ech_config_);

  uint16_t version;
  if (!CBS_get_u16(&reader, &version)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return false;
  }
  // Parse the ECHConfig, rejecting all unsupported parameters and extensions.
  // Unlike most server options, ECH's server configuration is serialized and
  // configured in both the server and DNS. If the caller configures an
  // unsupported parameter, this is a deployment error. To catch these errors,
  // we fail early.
  if (version != TLSEXT_TYPE_encrypted_client_hello) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNSUPPORTED_ECH_SERVER_CONFIG);
    return false;
  }

  CBS ech_config_contents, public_name, public_key, cipher_suites, extensions;
  uint16_t kem_id, max_name_len;
  if (!CBS_get_u16_length_prefixed(&reader, &ech_config_contents) ||
      !CBS_get_u8(&ech_config_contents, &config_id_) ||
      !CBS_get_u16(&ech_config_contents, &kem_id) ||
      !CBS_get_u16_length_prefixed(&ech_config_contents, &public_key) ||
      CBS_len(&public_key) == 0 ||
      !CBS_get_u16_length_prefixed(&ech_config_contents, &cipher_suites) ||
      CBS_len(&cipher_suites) == 0 ||
      !CBS_get_u16(&ech_config_contents, &max_name_len) ||
      !CBS_get_u16_length_prefixed(&ech_config_contents, &public_name) ||
      CBS_len(&public_name) == 0 ||
      !CBS_get_u16_length_prefixed(&ech_config_contents, &extensions) ||
      CBS_len(&ech_config_contents) != 0 ||  //
      CBS_len(&reader) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return false;
  }

  // We do not support any ECHConfig extensions, so |extensions| must be empty.
  if (CBS_len(&extensions) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_ECH_SERVER_CONFIG_UNSUPPORTED_EXTENSION);
    return false;
  }

  cipher_suites_ = cipher_suites;
  while (CBS_len(&cipher_suites) > 0) {
    uint16_t kdf_id, aead_id;
    if (!CBS_get_u16(&cipher_suites, &kdf_id) ||
        !CBS_get_u16(&cipher_suites, &aead_id)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      return false;
    }
    // The server promises to support every option in the ECHConfig, so reject
    // any unsupported cipher suites.
    if (kdf_id != EVP_HPKE_HKDF_SHA256 || get_ech_aead(aead_id) == nullptr) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_UNSUPPORTED_ECH_SERVER_CONFIG);
      return false;
    }
  }

  // We only support one KEM.
  if (kem_id != EVP_HPKE_DHKEM_X25519_HKDF_SHA256) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNSUPPORTED_ECH_SERVER_CONFIG);
    return false;
  }
  if (!EVP_HPKE_KEY_init(key_.get(), EVP_hpke_x25519_hkdf_sha256(),
                         private_key.data(), private_key.size())) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return false;
  }
  // Check the public key in the ECHConfig matches the private key.
  uint8_t expected_public_key[EVP_HPKE_MAX_PUBLIC_KEY_LENGTH];
  size_t expected_public_key_len;
  if (!EVP_HPKE_KEY_public_key(key_.get(), expected_public_key,
                               &expected_public_key_len,
                               sizeof(expected_public_key))) {
    return false;
  }
  if (MakeConstSpan(expected_public_key, expected_public_key_len) !=
      public_key) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_ECH_SERVER_CONFIG_AND_PRIVATE_KEY_MISMATCH);
    return false;
  }

  initialized_ = true;
  return true;
}

bool ECHServerConfig::SetupContext(EVP_HPKE_CTX *ctx, uint16_t kdf_id,
                                   uint16_t aead_id,
                                   Span<const uint8_t> enc) const {
  assert(initialized_);

  // Check the cipher suite is supported by this ECHServerConfig.
  CBS cbs(cipher_suites_);
  bool cipher_ok = false;
  while (CBS_len(&cbs) != 0) {
    uint16_t supported_kdf_id, supported_aead_id;
    if (!CBS_get_u16(&cbs, &supported_kdf_id) ||
        !CBS_get_u16(&cbs, &supported_aead_id)) {
      return false;
    }
    if (kdf_id == supported_kdf_id && aead_id == supported_aead_id) {
      cipher_ok = true;
      break;
    }
  }
  if (!cipher_ok) {
    return false;
  }

  static const uint8_t kInfoLabel[] = "tls ech";
  ScopedCBB info_cbb;
  if (!CBB_init(info_cbb.get(), sizeof(kInfoLabel) + ech_config_.size()) ||
      !CBB_add_bytes(info_cbb.get(), kInfoLabel,
                     sizeof(kInfoLabel) /* includes trailing NUL */) ||
      !CBB_add_bytes(info_cbb.get(), ech_config_.data(), ech_config_.size())) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return false;
  }

  assert(kdf_id == EVP_HPKE_HKDF_SHA256);
  assert(get_ech_aead(aead_id) != NULL);
  return EVP_HPKE_CTX_setup_recipient(
      ctx, key_.get(), EVP_hpke_hkdf_sha256(), get_ech_aead(aead_id), enc.data(),
      enc.size(), CBB_data(info_cbb.get()), CBB_len(info_cbb.get()));
}

BSSL_NAMESPACE_END
