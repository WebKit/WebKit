/* Copyright (c) 2016, Google Inc.
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
#include <string.h>

#include <utility>

#include <openssl/aead.h>
#include <openssl/bytestring.h>
#include <openssl/digest.h>
#include <openssl/hkdf.h>
#include <openssl/hmac.h>
#include <openssl/mem.h>

#include "../crypto/internal.h"
#include "internal.h"


BSSL_NAMESPACE_BEGIN

static bool init_key_schedule(SSL_HANDSHAKE *hs, uint16_t version,
                             const SSL_CIPHER *cipher) {
  if (!hs->transcript.InitHash(version, cipher)) {
    return false;
  }

  hs->hash_len = hs->transcript.DigestLen();

  // Initialize the secret to the zero key.
  OPENSSL_memset(hs->secret, 0, hs->hash_len);

  return true;
}

bool tls13_init_key_schedule(SSL_HANDSHAKE *hs, const uint8_t *psk,
                             size_t psk_len) {
  if (!init_key_schedule(hs, ssl_protocol_version(hs->ssl), hs->new_cipher)) {
    return false;
  }

  hs->transcript.FreeBuffer();
  return HKDF_extract(hs->secret, &hs->hash_len, hs->transcript.Digest(), psk,
                      psk_len, hs->secret, hs->hash_len);
}

bool tls13_init_early_key_schedule(SSL_HANDSHAKE *hs, const uint8_t *psk,
                                   size_t psk_len) {
  SSL *const ssl = hs->ssl;
  return init_key_schedule(hs, ssl_session_protocol_version(ssl->session.get()),
                           ssl->session->cipher) &&
         HKDF_extract(hs->secret, &hs->hash_len, hs->transcript.Digest(), psk,
                      psk_len, hs->secret, hs->hash_len);
}

static bool hkdf_expand_label(uint8_t *out, const EVP_MD *digest,
                              const uint8_t *secret, size_t secret_len,
                              const char *label, size_t label_len,
                              const uint8_t *hash, size_t hash_len, size_t len,
                              bool use_quic_label) {
  static const char kTLS13ProtocolLabel[] = "tls13 ";
  static const char kQUICProtocolLabel[] = "quic ";

  const char *protocol_label;
  if (use_quic_label) {
    protocol_label = kQUICProtocolLabel;
  } else {
    protocol_label = kTLS13ProtocolLabel;
  }

  ScopedCBB cbb;
  CBB child;
  Array<uint8_t> hkdf_label;
  if (!CBB_init(cbb.get(),
                2 + 1 + strlen(protocol_label) + label_len + 1 + hash_len) ||
      !CBB_add_u16(cbb.get(), len) ||
      !CBB_add_u8_length_prefixed(cbb.get(), &child) ||
      !CBB_add_bytes(&child, (const uint8_t *)protocol_label,
                     strlen(protocol_label)) ||
      !CBB_add_bytes(&child, (const uint8_t *)label, label_len) ||
      !CBB_add_u8_length_prefixed(cbb.get(), &child) ||
      !CBB_add_bytes(&child, hash, hash_len) ||
      !CBBFinishArray(cbb.get(), &hkdf_label)) {
    return false;
  }

  return HKDF_expand(out, len, digest, secret, secret_len, hkdf_label.data(),
                     hkdf_label.size());
}

static const char kTLS13LabelDerived[] = "derived";

bool tls13_advance_key_schedule(SSL_HANDSHAKE *hs, const uint8_t *in,
                                size_t len) {
  uint8_t derive_context[EVP_MAX_MD_SIZE];
  unsigned derive_context_len;
  if (!EVP_Digest(nullptr, 0, derive_context, &derive_context_len,
                  hs->transcript.Digest(), nullptr)) {
    return false;
  }

  if (!hkdf_expand_label(hs->secret, hs->transcript.Digest(), hs->secret,
                         hs->hash_len, kTLS13LabelDerived,
                         strlen(kTLS13LabelDerived), derive_context,
                         derive_context_len, hs->hash_len,
                         hs->ssl->ctx->quic_method != nullptr)) {
    return false;
  }

  return HKDF_extract(hs->secret, &hs->hash_len, hs->transcript.Digest(), in,
                      len, hs->secret, hs->hash_len);
}

// derive_secret derives a secret of length |len| and writes the result in |out|
// with the given label and the current base secret and most recently-saved
// handshake context. It returns true on success and false on error.
static bool derive_secret(SSL_HANDSHAKE *hs, uint8_t *out, size_t len,
                          const char *label, size_t label_len) {
  uint8_t context_hash[EVP_MAX_MD_SIZE];
  size_t context_hash_len;
  if (!hs->transcript.GetHash(context_hash, &context_hash_len)) {
    return false;
  }

  return hkdf_expand_label(out, hs->transcript.Digest(), hs->secret,
                           hs->hash_len, label, label_len, context_hash,
                           context_hash_len, len,
                           hs->ssl->ctx->quic_method != nullptr);
}

bool tls13_set_traffic_key(SSL *ssl, enum ssl_encryption_level_t level,
                           enum evp_aead_direction_t direction,
                           const uint8_t *traffic_secret,
                           size_t traffic_secret_len) {
  const SSL_SESSION *session = SSL_get_session(ssl);
  uint16_t version = ssl_session_protocol_version(session);

  if (traffic_secret_len > 0xff) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
    return false;
  }

  UniquePtr<SSLAEADContext> traffic_aead;
  if (ssl->ctx->quic_method == nullptr) {
    // Look up cipher suite properties.
    const EVP_AEAD *aead;
    size_t discard;
    if (!ssl_cipher_get_evp_aead(&aead, &discard, &discard, session->cipher,
                                 version, SSL_is_dtls(ssl))) {
      return false;
    }

    const EVP_MD *digest = ssl_session_get_digest(session);

    // Derive the key.
    size_t key_len = EVP_AEAD_key_length(aead);
    uint8_t key[EVP_AEAD_MAX_KEY_LENGTH];
    if (!hkdf_expand_label(key, digest, traffic_secret, traffic_secret_len,
                           "key", 3, NULL, 0, key_len,
                           ssl->ctx->quic_method != nullptr)) {
      return false;
    }

    // Derive the IV.
    size_t iv_len = EVP_AEAD_nonce_length(aead);
    uint8_t iv[EVP_AEAD_MAX_NONCE_LENGTH];
    if (!hkdf_expand_label(iv, digest, traffic_secret, traffic_secret_len, "iv",
                           2, NULL, 0, iv_len,
                           ssl->ctx->quic_method != nullptr)) {
      return false;
    }


    traffic_aead = SSLAEADContext::Create(
        direction, session->ssl_version, SSL_is_dtls(ssl), session->cipher,
        MakeConstSpan(key, key_len), Span<const uint8_t>(),
        MakeConstSpan(iv, iv_len));
  } else {
    // Install a placeholder SSLAEADContext so that SSL accessors work. The
    // encryption itself will be handled by the SSL_QUIC_METHOD.
    traffic_aead =
        SSLAEADContext::CreatePlaceholderForQUIC(version, session->cipher);
  }

  if (!traffic_aead) {
    return false;
  }

  if (direction == evp_aead_open) {
    if (!ssl->method->set_read_state(ssl, std::move(traffic_aead))) {
      return false;
    }
  } else {
    if (!ssl->method->set_write_state(ssl, std::move(traffic_aead))) {
      return false;
    }
  }

  // Save the traffic secret.
  if (direction == evp_aead_open) {
    OPENSSL_memmove(ssl->s3->read_traffic_secret, traffic_secret,
                    traffic_secret_len);
    ssl->s3->read_traffic_secret_len = traffic_secret_len;
    ssl->s3->read_level = level;
  } else {
    OPENSSL_memmove(ssl->s3->write_traffic_secret, traffic_secret,
                    traffic_secret_len);
    ssl->s3->write_traffic_secret_len = traffic_secret_len;
    ssl->s3->write_level = level;
  }

  return true;
}


static const char kTLS13LabelExporter[] = "exp master";
static const char kTLS13LabelEarlyExporter[] = "e exp master";

static const char kTLS13LabelClientEarlyTraffic[] = "c e traffic";
static const char kTLS13LabelClientHandshakeTraffic[] = "c hs traffic";
static const char kTLS13LabelServerHandshakeTraffic[] = "s hs traffic";
static const char kTLS13LabelClientApplicationTraffic[] = "c ap traffic";
static const char kTLS13LabelServerApplicationTraffic[] = "s ap traffic";

bool tls13_derive_early_secrets(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (!derive_secret(hs, hs->early_traffic_secret, hs->hash_len,
                     kTLS13LabelClientEarlyTraffic,
                     strlen(kTLS13LabelClientEarlyTraffic)) ||
      !ssl_log_secret(ssl, "CLIENT_EARLY_TRAFFIC_SECRET",
                      hs->early_traffic_secret, hs->hash_len) ||
      !derive_secret(hs, ssl->s3->early_exporter_secret, hs->hash_len,
                     kTLS13LabelEarlyExporter,
                     strlen(kTLS13LabelEarlyExporter))) {
    return false;
  }
  ssl->s3->early_exporter_secret_len = hs->hash_len;

  if (ssl->ctx->quic_method != nullptr) {
    if (ssl->server) {
      if (!ssl->ctx->quic_method->set_encryption_secrets(
              ssl, ssl_encryption_early_data, nullptr, hs->early_traffic_secret,
              hs->hash_len)) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_QUIC_INTERNAL_ERROR);
        return false;
      }
    } else {
      if (!ssl->ctx->quic_method->set_encryption_secrets(
              ssl, ssl_encryption_early_data, hs->early_traffic_secret, nullptr,
              hs->hash_len)) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_QUIC_INTERNAL_ERROR);
        return false;
      }
    }
  }

  return true;
}

bool tls13_derive_handshake_secrets(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (!derive_secret(hs, hs->client_handshake_secret, hs->hash_len,
                     kTLS13LabelClientHandshakeTraffic,
                     strlen(kTLS13LabelClientHandshakeTraffic)) ||
      !ssl_log_secret(ssl, "CLIENT_HANDSHAKE_TRAFFIC_SECRET",
                      hs->client_handshake_secret, hs->hash_len) ||
      !derive_secret(hs, hs->server_handshake_secret, hs->hash_len,
                     kTLS13LabelServerHandshakeTraffic,
                     strlen(kTLS13LabelServerHandshakeTraffic)) ||
      !ssl_log_secret(ssl, "SERVER_HANDSHAKE_TRAFFIC_SECRET",
                      hs->server_handshake_secret, hs->hash_len)) {
    return false;
  }

  if (ssl->ctx->quic_method != nullptr) {
    if (ssl->server) {
      if (!ssl->ctx->quic_method->set_encryption_secrets(
              ssl, ssl_encryption_handshake, hs->client_handshake_secret,
              hs->server_handshake_secret, hs->hash_len)) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_QUIC_INTERNAL_ERROR);
        return false;
      }
    } else {
      if (!ssl->ctx->quic_method->set_encryption_secrets(
              ssl, ssl_encryption_handshake, hs->server_handshake_secret,
              hs->client_handshake_secret, hs->hash_len)) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_QUIC_INTERNAL_ERROR);
        return false;
      }
    }
  }

  return true;
}

bool tls13_derive_application_secrets(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  ssl->s3->exporter_secret_len = hs->hash_len;
  if (!derive_secret(hs, hs->client_traffic_secret_0, hs->hash_len,
                     kTLS13LabelClientApplicationTraffic,
                     strlen(kTLS13LabelClientApplicationTraffic)) ||
      !ssl_log_secret(ssl, "CLIENT_TRAFFIC_SECRET_0",
                      hs->client_traffic_secret_0, hs->hash_len) ||
      !derive_secret(hs, hs->server_traffic_secret_0, hs->hash_len,
                     kTLS13LabelServerApplicationTraffic,
                     strlen(kTLS13LabelServerApplicationTraffic)) ||
      !ssl_log_secret(ssl, "SERVER_TRAFFIC_SECRET_0",
                      hs->server_traffic_secret_0, hs->hash_len) ||
      !derive_secret(hs, ssl->s3->exporter_secret, hs->hash_len,
                     kTLS13LabelExporter, strlen(kTLS13LabelExporter)) ||
      !ssl_log_secret(ssl, "EXPORTER_SECRET", ssl->s3->exporter_secret,
                      hs->hash_len)) {
    return false;
  }

  if (ssl->ctx->quic_method != nullptr) {
    if (ssl->server) {
      if (!ssl->ctx->quic_method->set_encryption_secrets(
              ssl, ssl_encryption_application, hs->client_traffic_secret_0,
              hs->server_traffic_secret_0, hs->hash_len)) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_QUIC_INTERNAL_ERROR);
        return false;
      }
    } else {
      if (!ssl->ctx->quic_method->set_encryption_secrets(
              ssl, ssl_encryption_application, hs->server_traffic_secret_0,
              hs->client_traffic_secret_0, hs->hash_len)) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_QUIC_INTERNAL_ERROR);
        return false;
      }
    }
  }

  return true;
}

static const char kTLS13LabelApplicationTraffic[] = "traffic upd";

bool tls13_rotate_traffic_key(SSL *ssl, enum evp_aead_direction_t direction) {
  uint8_t *secret;
  size_t secret_len;
  if (direction == evp_aead_open) {
    secret = ssl->s3->read_traffic_secret;
    secret_len = ssl->s3->read_traffic_secret_len;
  } else {
    secret = ssl->s3->write_traffic_secret;
    secret_len = ssl->s3->write_traffic_secret_len;
  }

  const EVP_MD *digest = ssl_session_get_digest(SSL_get_session(ssl));
  if (!hkdf_expand_label(secret, digest, secret, secret_len,
                         kTLS13LabelApplicationTraffic,
                         strlen(kTLS13LabelApplicationTraffic), NULL, 0,
                         secret_len, ssl->ctx->quic_method != nullptr)) {
    return false;
  }

  return tls13_set_traffic_key(ssl, ssl_encryption_application, direction,
                               secret, secret_len);
}

static const char kTLS13LabelResumption[] = "res master";

bool tls13_derive_resumption_secret(SSL_HANDSHAKE *hs) {
  if (hs->hash_len > SSL_MAX_MASTER_KEY_LENGTH) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }
  hs->new_session->master_key_length = hs->hash_len;
  return derive_secret(hs, hs->new_session->master_key,
                       hs->new_session->master_key_length,
                       kTLS13LabelResumption, strlen(kTLS13LabelResumption));
}

static const char kTLS13LabelFinished[] = "finished";

// tls13_verify_data sets |out| to be the HMAC of |context| using a derived
// Finished key for both Finished messages and the PSK binder.
static bool tls13_verify_data(const EVP_MD *digest, uint16_t version,
                              uint8_t *out, size_t *out_len,
                              const uint8_t *secret, size_t hash_len,
                              uint8_t *context, size_t context_len,
                              bool use_quic) {
  uint8_t key[EVP_MAX_MD_SIZE];
  unsigned len;
  if (!hkdf_expand_label(key, digest, secret, hash_len, kTLS13LabelFinished,
                         strlen(kTLS13LabelFinished), NULL, 0, hash_len,
                         use_quic) ||
      HMAC(digest, key, hash_len, context, context_len, out, &len) == NULL) {
    return false;
  }
  *out_len = len;
  return true;
}

bool tls13_finished_mac(SSL_HANDSHAKE *hs, uint8_t *out, size_t *out_len,
                        bool is_server) {
  const uint8_t *traffic_secret;
  if (is_server) {
    traffic_secret = hs->server_handshake_secret;
  } else {
    traffic_secret = hs->client_handshake_secret;
  }

  uint8_t context_hash[EVP_MAX_MD_SIZE];
  size_t context_hash_len;
  if (!hs->transcript.GetHash(context_hash, &context_hash_len) ||
      !tls13_verify_data(hs->transcript.Digest(), hs->ssl->version, out,
                         out_len, traffic_secret, hs->hash_len, context_hash,
                         context_hash_len,
                         hs->ssl->ctx->quic_method != nullptr)) {
    return 0;
  }
  return 1;
}

static const char kTLS13LabelResumptionPSK[] = "resumption";

bool tls13_derive_session_psk(SSL_SESSION *session, Span<const uint8_t> nonce,
                              bool use_quic) {
  const EVP_MD *digest = ssl_session_get_digest(session);
  return hkdf_expand_label(session->master_key, digest, session->master_key,
                           session->master_key_length, kTLS13LabelResumptionPSK,
                           strlen(kTLS13LabelResumptionPSK), nonce.data(),
                           nonce.size(), session->master_key_length, use_quic);
}

static const char kTLS13LabelExportKeying[] = "exporter";

bool tls13_export_keying_material(SSL *ssl, Span<uint8_t> out,
                                  Span<const uint8_t> secret,
                                  Span<const char> label,
                                  Span<const uint8_t> context) {
  if (secret.empty()) {
    assert(0);
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  const EVP_MD *digest = ssl_session_get_digest(SSL_get_session(ssl));

  uint8_t hash[EVP_MAX_MD_SIZE];
  uint8_t export_context[EVP_MAX_MD_SIZE];
  uint8_t derived_secret[EVP_MAX_MD_SIZE];
  unsigned hash_len;
  unsigned export_context_len;
  unsigned derived_secret_len = EVP_MD_size(digest);
  return EVP_Digest(context.data(), context.size(), hash, &hash_len, digest,
                    nullptr) &&
         EVP_Digest(nullptr, 0, export_context, &export_context_len, digest,
                    nullptr) &&
         hkdf_expand_label(derived_secret, digest, secret.data(), secret.size(),
                           label.data(), label.size(), export_context,
                           export_context_len, derived_secret_len,
                           ssl->ctx->quic_method != nullptr) &&
         hkdf_expand_label(out.data(), digest, derived_secret,
                           derived_secret_len, kTLS13LabelExportKeying,
                           strlen(kTLS13LabelExportKeying), hash, hash_len,
                           out.size(), ssl->ctx->quic_method != nullptr);
}

static const char kTLS13LabelPSKBinder[] = "res binder";

static bool tls13_psk_binder(uint8_t *out, uint16_t version,
                             const EVP_MD *digest, uint8_t *psk, size_t psk_len,
                             uint8_t *context, size_t context_len,
                             size_t hash_len, bool use_quic) {
  uint8_t binder_context[EVP_MAX_MD_SIZE];
  unsigned binder_context_len;
  if (!EVP_Digest(NULL, 0, binder_context, &binder_context_len, digest, NULL)) {
    return false;
  }

  uint8_t early_secret[EVP_MAX_MD_SIZE] = {0};
  size_t early_secret_len;
  if (!HKDF_extract(early_secret, &early_secret_len, digest, psk, hash_len,
                    NULL, 0)) {
    return false;
  }

  uint8_t binder_key[EVP_MAX_MD_SIZE] = {0};
  size_t len;
  if (!hkdf_expand_label(binder_key, digest, early_secret, hash_len,
                         kTLS13LabelPSKBinder, strlen(kTLS13LabelPSKBinder),
                         binder_context, binder_context_len, hash_len,
                         use_quic) ||
      !tls13_verify_data(digest, version, out, &len, binder_key, hash_len,
                         context, context_len, use_quic)) {
    return false;
  }

  return true;
}

bool tls13_write_psk_binder(SSL_HANDSHAKE *hs, uint8_t *msg, size_t len) {
  SSL *const ssl = hs->ssl;
  const EVP_MD *digest = ssl_session_get_digest(ssl->session.get());
  size_t hash_len = EVP_MD_size(digest);

  if (len < hash_len + 3) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  ScopedEVP_MD_CTX ctx;
  uint8_t context[EVP_MAX_MD_SIZE];
  unsigned context_len;

  if (!EVP_DigestInit_ex(ctx.get(), digest, NULL) ||
      !EVP_DigestUpdate(ctx.get(), hs->transcript.buffer().data(),
                        hs->transcript.buffer().size()) ||
      !EVP_DigestUpdate(ctx.get(), msg, len - hash_len - 3) ||
      !EVP_DigestFinal_ex(ctx.get(), context, &context_len)) {
    return false;
  }

  uint8_t verify_data[EVP_MAX_MD_SIZE] = {0};
  if (!tls13_psk_binder(verify_data, ssl->session->ssl_version, digest,
                        ssl->session->master_key,
                        ssl->session->master_key_length, context, context_len,
                        hash_len, ssl->ctx->quic_method != nullptr)) {
    return false;
  }

  OPENSSL_memcpy(msg + len - hash_len, verify_data, hash_len);
  return true;
}

bool tls13_verify_psk_binder(SSL_HANDSHAKE *hs, SSL_SESSION *session,
                             const SSLMessage &msg, CBS *binders) {
  size_t hash_len = hs->transcript.DigestLen();

  // The message must be large enough to exclude the binders.
  if (CBS_len(&msg.raw) < CBS_len(binders) + 2) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  // Hash a ClientHello prefix up to the binders. This includes the header. For
  // now, this assumes we only ever verify PSK binders on initial
  // ClientHellos.
  uint8_t context[EVP_MAX_MD_SIZE];
  unsigned context_len;
  if (!EVP_Digest(CBS_data(&msg.raw), CBS_len(&msg.raw) - CBS_len(binders) - 2,
                  context, &context_len, hs->transcript.Digest(), NULL)) {
    return false;
  }

  uint8_t verify_data[EVP_MAX_MD_SIZE] = {0};
  CBS binder;
  if (!tls13_psk_binder(verify_data, hs->ssl->version, hs->transcript.Digest(),
                        session->master_key, session->master_key_length,
                        context, context_len, hash_len,
                        hs->ssl->ctx->quic_method != nullptr) ||
      // We only consider the first PSK, so compare against the first binder.
      !CBS_get_u8_length_prefixed(binders, &binder)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  bool binder_ok = CBS_len(&binder) == hash_len &&
                   CRYPTO_memcmp(CBS_data(&binder), verify_data, hash_len) == 0;
#if defined(BORINGSSL_UNSAFE_FUZZER_MODE)
  binder_ok = true;
#endif
  if (!binder_ok) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DIGEST_CHECK_FAILED);
    return false;
  }

  return true;
}

BSSL_NAMESPACE_END
