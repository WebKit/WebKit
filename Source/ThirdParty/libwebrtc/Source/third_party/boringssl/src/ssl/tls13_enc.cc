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


namespace bssl {

static int init_key_schedule(SSL_HANDSHAKE *hs, uint16_t version,
                             const SSL_CIPHER *cipher) {
  if (!hs->transcript.InitHash(version, cipher)) {
    return 0;
  }

  hs->hash_len = hs->transcript.DigestLen();

  // Initialize the secret to the zero key.
  OPENSSL_memset(hs->secret, 0, hs->hash_len);

  return 1;
}

int tls13_init_key_schedule(SSL_HANDSHAKE *hs, const uint8_t *psk,
                            size_t psk_len) {
  if (!init_key_schedule(hs, ssl_protocol_version(hs->ssl), hs->new_cipher)) {
    return 0;
  }

  hs->transcript.FreeBuffer();
  return HKDF_extract(hs->secret, &hs->hash_len, hs->transcript.Digest(), psk,
                      psk_len, hs->secret, hs->hash_len);
}

int tls13_init_early_key_schedule(SSL_HANDSHAKE *hs, const uint8_t *psk,
                                  size_t psk_len) {
  SSL *const ssl = hs->ssl;
  return init_key_schedule(hs, ssl_session_protocol_version(ssl->session),
                           ssl->session->cipher) &&
         HKDF_extract(hs->secret, &hs->hash_len, hs->transcript.Digest(), psk,
                      psk_len, hs->secret, hs->hash_len);
}

static int hkdf_expand_label(uint8_t *out, uint16_t version,
                             const EVP_MD *digest, const uint8_t *secret,
                             size_t secret_len, const uint8_t *label,
                             size_t label_len, const uint8_t *hash,
                             size_t hash_len, size_t len) {
  const char *kTLS13LabelVersion =
      ssl_is_draft21(version) ? "tls13 " : "TLS 1.3, ";

  ScopedCBB cbb;
  CBB child;
  uint8_t *hkdf_label;
  size_t hkdf_label_len;
  if (!CBB_init(cbb.get(), 2 + 1 + strlen(kTLS13LabelVersion) + label_len + 1 +
                               hash_len) ||
      !CBB_add_u16(cbb.get(), len) ||
      !CBB_add_u8_length_prefixed(cbb.get(), &child) ||
      !CBB_add_bytes(&child, (const uint8_t *)kTLS13LabelVersion,
                     strlen(kTLS13LabelVersion)) ||
      !CBB_add_bytes(&child, label, label_len) ||
      !CBB_add_u8_length_prefixed(cbb.get(), &child) ||
      !CBB_add_bytes(&child, hash, hash_len) ||
      !CBB_finish(cbb.get(), &hkdf_label, &hkdf_label_len)) {
    return 0;
  }

  int ret = HKDF_expand(out, len, digest, secret, secret_len, hkdf_label,
                        hkdf_label_len);
  OPENSSL_free(hkdf_label);
  return ret;
}

static const char kTLS13LabelDerived[] = "derived";

int tls13_advance_key_schedule(SSL_HANDSHAKE *hs, const uint8_t *in,
                               size_t len) {
  SSL *const ssl = hs->ssl;

  // Draft 18 does not include the extra Derive-Secret step.
  if (ssl_is_draft21(ssl->version)) {
    uint8_t derive_context[EVP_MAX_MD_SIZE];
    unsigned derive_context_len;
    if (!EVP_Digest(nullptr, 0, derive_context, &derive_context_len,
                    hs->transcript.Digest(), nullptr)) {
      return 0;
    }

    if (!hkdf_expand_label(hs->secret, ssl->version, hs->transcript.Digest(),
                           hs->secret, hs->hash_len,
                           (const uint8_t *)kTLS13LabelDerived,
                           strlen(kTLS13LabelDerived), derive_context,
                           derive_context_len, hs->hash_len)) {
      return 0;
    }
  }

  return HKDF_extract(hs->secret, &hs->hash_len, hs->transcript.Digest(), in,
                      len, hs->secret, hs->hash_len);
}

// derive_secret derives a secret of length |len| and writes the result in |out|
// with the given label and the current base secret and most recently-saved
// handshake context. It returns one on success and zero on error.
static int derive_secret(SSL_HANDSHAKE *hs, uint8_t *out, size_t len,
                         const uint8_t *label, size_t label_len) {
  uint8_t context_hash[EVP_MAX_MD_SIZE];
  size_t context_hash_len;
  if (!hs->transcript.GetHash(context_hash, &context_hash_len)) {
    return 0;
  }

  return hkdf_expand_label(out, SSL_get_session(hs->ssl)->ssl_version,
                           hs->transcript.Digest(), hs->secret, hs->hash_len,
                           label, label_len, context_hash, context_hash_len,
                           len);
}

int tls13_set_traffic_key(SSL *ssl, enum evp_aead_direction_t direction,
                          const uint8_t *traffic_secret,
                          size_t traffic_secret_len) {
  const SSL_SESSION *session = SSL_get_session(ssl);
  uint16_t version = ssl_session_protocol_version(session);

  if (traffic_secret_len > 0xff) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
    return 0;
  }

  // Look up cipher suite properties.
  const EVP_AEAD *aead;
  size_t discard;
  if (!ssl_cipher_get_evp_aead(&aead, &discard, &discard, session->cipher,
                               version, SSL_is_dtls(ssl))) {
    return 0;
  }

  const EVP_MD *digest = ssl_session_get_digest(session);

  // Derive the key.
  size_t key_len = EVP_AEAD_key_length(aead);
  uint8_t key[EVP_AEAD_MAX_KEY_LENGTH];
  if (!hkdf_expand_label(key, session->ssl_version, digest, traffic_secret,
                         traffic_secret_len, (const uint8_t *)"key", 3, NULL, 0,
                         key_len)) {
    return 0;
  }

  // Derive the IV.
  size_t iv_len = EVP_AEAD_nonce_length(aead);
  uint8_t iv[EVP_AEAD_MAX_NONCE_LENGTH];
  if (!hkdf_expand_label(iv, session->ssl_version, digest, traffic_secret,
                         traffic_secret_len, (const uint8_t *)"iv", 2, NULL, 0,
                         iv_len)) {
    return 0;
  }

  UniquePtr<SSLAEADContext> traffic_aead =
      SSLAEADContext::Create(direction, session->ssl_version, SSL_is_dtls(ssl),
                             session->cipher, MakeConstSpan(key, key_len),
                             Span<const uint8_t>(), MakeConstSpan(iv, iv_len));
  if (!traffic_aead) {
    return 0;
  }

  if (direction == evp_aead_open) {
    if (!ssl->method->set_read_state(ssl, std::move(traffic_aead))) {
      return 0;
    }
  } else {
    if (!ssl->method->set_write_state(ssl, std::move(traffic_aead))) {
      return 0;
    }
  }

  // Save the traffic secret.
  if (direction == evp_aead_open) {
    OPENSSL_memmove(ssl->s3->read_traffic_secret, traffic_secret,
                    traffic_secret_len);
    ssl->s3->read_traffic_secret_len = traffic_secret_len;
  } else {
    OPENSSL_memmove(ssl->s3->write_traffic_secret, traffic_secret,
                    traffic_secret_len);
    ssl->s3->write_traffic_secret_len = traffic_secret_len;
  }

  return 1;
}

static const char kTLS13LabelExporter[] = "exporter master secret";
static const char kTLS13LabelEarlyExporter[] = "early exporter master secret";

static const char kTLS13LabelClientEarlyTraffic[] =
    "client early traffic secret";
static const char kTLS13LabelClientHandshakeTraffic[] =
    "client handshake traffic secret";
static const char kTLS13LabelServerHandshakeTraffic[] =
    "server handshake traffic secret";
static const char kTLS13LabelClientApplicationTraffic[] =
    "client application traffic secret";
static const char kTLS13LabelServerApplicationTraffic[] =
    "server application traffic secret";

static const char kTLS13Draft21LabelExporter[] = "exp master";
static const char kTLS13Draft21LabelEarlyExporter[] = "e exp master";

static const char kTLS13Draft21LabelClientEarlyTraffic[] = "c e traffic";
static const char kTLS13Draft21LabelClientHandshakeTraffic[] = "c hs traffic";
static const char kTLS13Draft21LabelServerHandshakeTraffic[] = "s hs traffic";
static const char kTLS13Draft21LabelClientApplicationTraffic[] = "c ap traffic";
static const char kTLS13Draft21LabelServerApplicationTraffic[] = "s ap traffic";

int tls13_derive_early_secrets(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  uint16_t version = SSL_get_session(ssl)->ssl_version;

  const char *early_traffic_label = ssl_is_draft21(version)
                                        ? kTLS13Draft21LabelClientEarlyTraffic
                                        : kTLS13LabelClientEarlyTraffic;
  const char *early_exporter_label = ssl_is_draft21(version)
                                         ? kTLS13Draft21LabelEarlyExporter
                                         : kTLS13LabelEarlyExporter;
  return derive_secret(hs, hs->early_traffic_secret, hs->hash_len,
                       (const uint8_t *)early_traffic_label,
                       strlen(early_traffic_label)) &&
         ssl_log_secret(ssl, "CLIENT_EARLY_TRAFFIC_SECRET",
                        hs->early_traffic_secret, hs->hash_len) &&
         derive_secret(hs, ssl->s3->early_exporter_secret, hs->hash_len,
                       (const uint8_t *)early_exporter_label,
                       strlen(early_exporter_label));
}

int tls13_derive_handshake_secrets(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  const char *client_label = ssl_is_draft21(ssl->version)
                                 ? kTLS13Draft21LabelClientHandshakeTraffic
                                 : kTLS13LabelClientHandshakeTraffic;
  const char *server_label = ssl_is_draft21(ssl->version)
                                 ? kTLS13Draft21LabelServerHandshakeTraffic
                                 : kTLS13LabelServerHandshakeTraffic;
  return derive_secret(hs, hs->client_handshake_secret, hs->hash_len,
                       (const uint8_t *)client_label, strlen(client_label)) &&
         ssl_log_secret(ssl, "CLIENT_HANDSHAKE_TRAFFIC_SECRET",
                        hs->client_handshake_secret, hs->hash_len) &&
         derive_secret(hs, hs->server_handshake_secret, hs->hash_len,
                       (const uint8_t *)server_label, strlen(server_label)) &&
         ssl_log_secret(ssl, "SERVER_HANDSHAKE_TRAFFIC_SECRET",
                        hs->server_handshake_secret, hs->hash_len);
}

int tls13_derive_application_secrets(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  ssl->s3->exporter_secret_len = hs->hash_len;
  const char *client_label = ssl_is_draft21(ssl->version)
                                 ? kTLS13Draft21LabelClientApplicationTraffic
                                 : kTLS13LabelClientApplicationTraffic;
  const char *server_label = ssl_is_draft21(ssl->version)
                                 ? kTLS13Draft21LabelServerApplicationTraffic
                                 : kTLS13LabelServerApplicationTraffic;
  const char *exporter_label = ssl_is_draft21(ssl->version)
                                   ? kTLS13Draft21LabelExporter
                                   : kTLS13LabelExporter;
  return derive_secret(hs, hs->client_traffic_secret_0, hs->hash_len,
                       (const uint8_t *)client_label, strlen(client_label)) &&
         ssl_log_secret(ssl, "CLIENT_TRAFFIC_SECRET_0",
                        hs->client_traffic_secret_0, hs->hash_len) &&
         derive_secret(hs, hs->server_traffic_secret_0, hs->hash_len,
                       (const uint8_t *)server_label, strlen(server_label)) &&
         ssl_log_secret(ssl, "SERVER_TRAFFIC_SECRET_0",
                        hs->server_traffic_secret_0, hs->hash_len) &&
         derive_secret(hs, ssl->s3->exporter_secret, hs->hash_len,
                       (const uint8_t *)exporter_label,
                       strlen(exporter_label)) &&
         ssl_log_secret(ssl, "EXPORTER_SECRET", ssl->s3->exporter_secret,
                        hs->hash_len);
}

static const char kTLS13LabelApplicationTraffic[] =
    "application traffic secret";
static const char kTLS13Draft21LabelApplicationTraffic[] = "traffic upd";

int tls13_rotate_traffic_key(SSL *ssl, enum evp_aead_direction_t direction) {
  uint8_t *secret;
  size_t secret_len;
  if (direction == evp_aead_open) {
    secret = ssl->s3->read_traffic_secret;
    secret_len = ssl->s3->read_traffic_secret_len;
  } else {
    secret = ssl->s3->write_traffic_secret;
    secret_len = ssl->s3->write_traffic_secret_len;
  }

  const char *traffic_label = ssl_is_draft21(ssl->version)
                                  ? kTLS13Draft21LabelApplicationTraffic
                                  : kTLS13LabelApplicationTraffic;

  const EVP_MD *digest = ssl_session_get_digest(SSL_get_session(ssl));
  if (!hkdf_expand_label(secret, ssl->version, digest, secret, secret_len,
                         (const uint8_t *)traffic_label, strlen(traffic_label),
                         NULL, 0, secret_len)) {
    return 0;
  }

  return tls13_set_traffic_key(ssl, direction, secret, secret_len);
}

static const char kTLS13LabelResumption[] = "resumption master secret";
static const char kTLS13Draft21LabelResumption[] = "res master";

int tls13_derive_resumption_secret(SSL_HANDSHAKE *hs) {
  if (hs->hash_len > SSL_MAX_MASTER_KEY_LENGTH) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }
  const char *resumption_label = ssl_is_draft21(hs->ssl->version)
                                     ? kTLS13Draft21LabelResumption
                                     : kTLS13LabelResumption;
  hs->new_session->master_key_length = hs->hash_len;
  return derive_secret(
      hs, hs->new_session->master_key, hs->new_session->master_key_length,
      (const uint8_t *)resumption_label, strlen(resumption_label));
}

static const char kTLS13LabelFinished[] = "finished";

// tls13_verify_data sets |out| to be the HMAC of |context| using a derived
// Finished key for both Finished messages and the PSK binder.
static int tls13_verify_data(const EVP_MD *digest, uint16_t version,
                             uint8_t *out, size_t *out_len,
                             const uint8_t *secret, size_t hash_len,
                             uint8_t *context, size_t context_len) {
  uint8_t key[EVP_MAX_MD_SIZE];
  unsigned len;
  if (!hkdf_expand_label(key, version, digest, secret, hash_len,
                         (const uint8_t *)kTLS13LabelFinished,
                         strlen(kTLS13LabelFinished), NULL, 0, hash_len) ||
      HMAC(digest, key, hash_len, context, context_len, out, &len) == NULL) {
    return 0;
  }
  *out_len = len;
  return 1;
}

int tls13_finished_mac(SSL_HANDSHAKE *hs, uint8_t *out, size_t *out_len,
                       int is_server) {
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
                         context_hash_len)) {
    return 0;
  }
  return 1;
}

static const char kTLS13LabelResumptionPSK[] = "resumption";

bool tls13_derive_session_psk(SSL_SESSION *session, Span<const uint8_t> nonce) {
  if (!ssl_is_draft21(session->ssl_version)) {
    return true;
  }

  const EVP_MD *digest = ssl_session_get_digest(session);
  return hkdf_expand_label(session->master_key, session->ssl_version, digest,
                           session->master_key, session->master_key_length,
                           (const uint8_t *)kTLS13LabelResumptionPSK,
                           strlen(kTLS13LabelResumptionPSK), nonce.data(),
                           nonce.size(), session->master_key_length);
}

static const char kTLS13LabelExportKeying[] = "exporter";

int tls13_export_keying_material(SSL *ssl, uint8_t *out, size_t out_len,
                                 const char *label, size_t label_len,
                                 const uint8_t *context_in,
                                 size_t context_in_len, int use_context) {
  const uint8_t *context = NULL;
  size_t context_len = 0;
  if (use_context) {
    context = context_in;
    context_len = context_in_len;
  }

  if (!ssl_is_draft21(ssl->version)) {
    const EVP_MD *digest = ssl_session_get_digest(SSL_get_session(ssl));
    return hkdf_expand_label(
        out, ssl->version, digest, ssl->s3->exporter_secret,
        ssl->s3->exporter_secret_len, (const uint8_t *)label, label_len,
        context, context_len, out_len);
  }

  const EVP_MD *digest = ssl_session_get_digest(SSL_get_session(ssl));

  uint8_t hash[EVP_MAX_MD_SIZE];
  uint8_t export_context[EVP_MAX_MD_SIZE];
  uint8_t derived_secret[EVP_MAX_MD_SIZE];
  unsigned hash_len;
  unsigned export_context_len;
  unsigned derived_secret_len = EVP_MD_size(digest);
  if (!EVP_Digest(context, context_len, hash, &hash_len, digest, NULL) ||
      !EVP_Digest(NULL, 0, export_context, &export_context_len, digest, NULL)) {
    return 0;
  }
  return hkdf_expand_label(
             derived_secret, ssl->version, digest, ssl->s3->exporter_secret,
             ssl->s3->exporter_secret_len, (const uint8_t *)label, label_len,
             export_context, export_context_len, derived_secret_len) &&
         hkdf_expand_label(
             out, ssl->version, digest, derived_secret, derived_secret_len,
             (const uint8_t *)kTLS13LabelExportKeying,
             strlen(kTLS13LabelExportKeying), hash, hash_len, out_len);
}

static const char kTLS13LabelPSKBinder[] = "resumption psk binder key";
static const char kTLS13Draft21LabelPSKBinder[] = "res binder";

static int tls13_psk_binder(uint8_t *out, uint16_t version,
                            const EVP_MD *digest, uint8_t *psk, size_t psk_len,
                            uint8_t *context, size_t context_len,
                            size_t hash_len) {
  uint8_t binder_context[EVP_MAX_MD_SIZE];
  unsigned binder_context_len;
  if (!EVP_Digest(NULL, 0, binder_context, &binder_context_len, digest, NULL)) {
    return 0;
  }

  uint8_t early_secret[EVP_MAX_MD_SIZE] = {0};
  size_t early_secret_len;
  if (!HKDF_extract(early_secret, &early_secret_len, digest, psk, hash_len,
                    NULL, 0)) {
    return 0;
  }
  const char *binder_label = ssl_is_draft21(version)
                                 ? kTLS13Draft21LabelPSKBinder
                                 : kTLS13LabelPSKBinder;

  uint8_t binder_key[EVP_MAX_MD_SIZE] = {0};
  size_t len;
  if (!hkdf_expand_label(binder_key, version, digest, early_secret, hash_len,
                         (const uint8_t *)binder_label, strlen(binder_label),
                         binder_context, binder_context_len, hash_len) ||
      !tls13_verify_data(digest, version, out, &len, binder_key, hash_len,
                         context, context_len)) {
    return 0;
  }

  return 1;
}

int tls13_write_psk_binder(SSL_HANDSHAKE *hs, uint8_t *msg, size_t len) {
  SSL *const ssl = hs->ssl;
  const EVP_MD *digest = ssl_session_get_digest(ssl->session);
  size_t hash_len = EVP_MD_size(digest);

  if (len < hash_len + 3) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  ScopedEVP_MD_CTX ctx;
  uint8_t context[EVP_MAX_MD_SIZE];
  unsigned context_len;

  if (!EVP_DigestInit_ex(ctx.get(), digest, NULL) ||
      !EVP_DigestUpdate(ctx.get(), hs->transcript.buffer().data(),
                        hs->transcript.buffer().size()) ||
      !EVP_DigestUpdate(ctx.get(), msg, len - hash_len - 3) ||
      !EVP_DigestFinal_ex(ctx.get(), context, &context_len)) {
    return 0;
  }

  uint8_t verify_data[EVP_MAX_MD_SIZE] = {0};
  if (!tls13_psk_binder(verify_data, ssl->session->ssl_version, digest,
                        ssl->session->master_key,
                        ssl->session->master_key_length, context, context_len,
                        hash_len)) {
    return 0;
  }

  OPENSSL_memcpy(msg + len - hash_len, verify_data, hash_len);
  return 1;
}

int tls13_verify_psk_binder(SSL_HANDSHAKE *hs, SSL_SESSION *session,
                            const SSLMessage &msg, CBS *binders) {
  size_t hash_len = hs->transcript.DigestLen();

  // The message must be large enough to exclude the binders.
  if (CBS_len(&msg.raw) < CBS_len(binders) + 2) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  // Hash a ClientHello prefix up to the binders. This includes the header. For
  // now, this assumes we only ever verify PSK binders on initial
  // ClientHellos.
  uint8_t context[EVP_MAX_MD_SIZE];
  unsigned context_len;
  if (!EVP_Digest(CBS_data(&msg.raw), CBS_len(&msg.raw) - CBS_len(binders) - 2,
                  context, &context_len, hs->transcript.Digest(), NULL)) {
    return 0;
  }

  uint8_t verify_data[EVP_MAX_MD_SIZE] = {0};
  CBS binder;
  if (!tls13_psk_binder(verify_data, hs->ssl->version, hs->transcript.Digest(),
                        session->master_key, session->master_key_length,
                        context, context_len, hash_len) ||
      // We only consider the first PSK, so compare against the first binder.
      !CBS_get_u8_length_prefixed(binders, &binder)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  int binder_ok =
      CBS_len(&binder) == hash_len &&
      CRYPTO_memcmp(CBS_data(&binder), verify_data, hash_len) == 0;
#if defined(BORINGSSL_UNSAFE_FUZZER_MODE)
  binder_ok = 1;
#endif
  if (!binder_ok) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DIGEST_CHECK_FAILED);
    return 0;
  }

  return 1;
}

}  // namespace bssl
