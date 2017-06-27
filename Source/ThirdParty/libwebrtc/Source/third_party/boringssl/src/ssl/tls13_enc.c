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

#include <openssl/aead.h>
#include <openssl/bytestring.h>
#include <openssl/digest.h>
#include <openssl/hkdf.h>
#include <openssl/hmac.h>
#include <openssl/mem.h>

#include "../crypto/internal.h"
#include "internal.h"


static int init_key_schedule(SSL_HANDSHAKE *hs, uint16_t version,
                              int algorithm_prf) {
  if (!SSL_TRANSCRIPT_init_hash(&hs->transcript, version, algorithm_prf)) {
    return 0;
  }

  hs->hash_len = SSL_TRANSCRIPT_digest_len(&hs->transcript);

  /* Initialize the secret to the zero key. */
  OPENSSL_memset(hs->secret, 0, hs->hash_len);

  return 1;
}

int tls13_init_key_schedule(SSL_HANDSHAKE *hs) {
  if (!init_key_schedule(hs, ssl3_protocol_version(hs->ssl),
                         hs->new_cipher->algorithm_prf)) {
    return 0;
  }

  SSL_TRANSCRIPT_free_buffer(&hs->transcript);
  return 1;
}

int tls13_init_early_key_schedule(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  uint16_t session_version;
  if (!ssl->method->version_from_wire(&session_version,
                                      ssl->session->ssl_version) ||
      !init_key_schedule(hs, session_version,
                         ssl->session->cipher->algorithm_prf)) {
    return 0;
  }

  return 1;
}

int tls13_advance_key_schedule(SSL_HANDSHAKE *hs, const uint8_t *in,
                               size_t len) {
  return HKDF_extract(hs->secret, &hs->hash_len,
                      SSL_TRANSCRIPT_md(&hs->transcript), in, len, hs->secret,
                      hs->hash_len);
}

static int hkdf_expand_label(uint8_t *out, const EVP_MD *digest,
                             const uint8_t *secret, size_t secret_len,
                             const uint8_t *label, size_t label_len,
                             const uint8_t *hash, size_t hash_len, size_t len) {
  static const char kTLS13LabelVersion[] = "TLS 1.3, ";

  CBB cbb, child;
  uint8_t *hkdf_label;
  size_t hkdf_label_len;
  if (!CBB_init(&cbb, 2 + 1 + strlen(kTLS13LabelVersion) + label_len + 1 +
                          hash_len) ||
      !CBB_add_u16(&cbb, len) ||
      !CBB_add_u8_length_prefixed(&cbb, &child) ||
      !CBB_add_bytes(&child, (const uint8_t *)kTLS13LabelVersion,
                     strlen(kTLS13LabelVersion)) ||
      !CBB_add_bytes(&child, label, label_len) ||
      !CBB_add_u8_length_prefixed(&cbb, &child) ||
      !CBB_add_bytes(&child, hash, hash_len) ||
      !CBB_finish(&cbb, &hkdf_label, &hkdf_label_len)) {
    CBB_cleanup(&cbb);
    return 0;
  }

  int ret = HKDF_expand(out, len, digest, secret, secret_len, hkdf_label,
                        hkdf_label_len);
  OPENSSL_free(hkdf_label);
  return ret;
}

/* derive_secret derives a secret of length |len| and writes the result in |out|
 * with the given label and the current base secret and most recently-saved
 * handshake context. It returns one on success and zero on error. */
static int derive_secret(SSL_HANDSHAKE *hs, uint8_t *out, size_t len,
                         const uint8_t *label, size_t label_len) {
  uint8_t context_hash[EVP_MAX_MD_SIZE];
  size_t context_hash_len;
  if (!SSL_TRANSCRIPT_get_hash(&hs->transcript, context_hash,
                               &context_hash_len)) {
    return 0;
  }

  return hkdf_expand_label(out, SSL_TRANSCRIPT_md(&hs->transcript), hs->secret,
                           hs->hash_len, label, label_len, context_hash,
                           context_hash_len, len);
}

int tls13_set_traffic_key(SSL *ssl, enum evp_aead_direction_t direction,
                          const uint8_t *traffic_secret,
                          size_t traffic_secret_len) {
  const SSL_SESSION *session = SSL_get_session(ssl);
  uint16_t version;
  if (!ssl->method->version_from_wire(&version, session->ssl_version)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  if (traffic_secret_len > 0xff) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
    return 0;
  }

  /* Look up cipher suite properties. */
  const EVP_AEAD *aead;
  size_t discard;
  if (!ssl_cipher_get_evp_aead(&aead, &discard, &discard, session->cipher,
                               version, SSL_is_dtls(ssl))) {
    return 0;
  }

  const EVP_MD *digest = ssl_get_handshake_digest(
      session->cipher->algorithm_prf, version);

  /* Derive the key. */
  size_t key_len = EVP_AEAD_key_length(aead);
  uint8_t key[EVP_AEAD_MAX_KEY_LENGTH];
  if (!hkdf_expand_label(key, digest, traffic_secret, traffic_secret_len,
                         (const uint8_t *)"key", 3, NULL, 0, key_len)) {
    return 0;
  }

  /* Derive the IV. */
  size_t iv_len = EVP_AEAD_nonce_length(aead);
  uint8_t iv[EVP_AEAD_MAX_NONCE_LENGTH];
  if (!hkdf_expand_label(iv, digest, traffic_secret, traffic_secret_len,
                         (const uint8_t *)"iv", 2, NULL, 0, iv_len)) {
    return 0;
  }

  SSL_AEAD_CTX *traffic_aead =
      SSL_AEAD_CTX_new(direction, version, SSL_is_dtls(ssl), session->cipher,
                       key, key_len, NULL, 0, iv, iv_len);
  if (traffic_aead == NULL) {
    return 0;
  }

  if (direction == evp_aead_open) {
    if (!ssl->method->set_read_state(ssl, traffic_aead)) {
      return 0;
    }
  } else {
    if (!ssl->method->set_write_state(ssl, traffic_aead)) {
      return 0;
    }
  }

  /* Save the traffic secret. */
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

int tls13_derive_early_secrets(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  return derive_secret(hs, hs->early_traffic_secret, hs->hash_len,
                       (const uint8_t *)kTLS13LabelClientEarlyTraffic,
                       strlen(kTLS13LabelClientEarlyTraffic)) &&
         ssl_log_secret(ssl, "CLIENT_EARLY_TRAFFIC_SECRET",
                        hs->early_traffic_secret, hs->hash_len) &&
         derive_secret(hs, ssl->s3->early_exporter_secret, hs->hash_len,
                       (const uint8_t *)kTLS13LabelEarlyExporter,
                       strlen(kTLS13LabelEarlyExporter));
}

int tls13_derive_handshake_secrets(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  return derive_secret(hs, hs->client_handshake_secret, hs->hash_len,
                       (const uint8_t *)kTLS13LabelClientHandshakeTraffic,
                       strlen(kTLS13LabelClientHandshakeTraffic)) &&
         ssl_log_secret(ssl, "CLIENT_HANDSHAKE_TRAFFIC_SECRET",
                        hs->client_handshake_secret, hs->hash_len) &&
         derive_secret(hs, hs->server_handshake_secret, hs->hash_len,
                       (const uint8_t *)kTLS13LabelServerHandshakeTraffic,
                       strlen(kTLS13LabelServerHandshakeTraffic)) &&
         ssl_log_secret(ssl, "SERVER_HANDSHAKE_TRAFFIC_SECRET",
                        hs->server_handshake_secret, hs->hash_len);
}

int tls13_derive_application_secrets(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  ssl->s3->exporter_secret_len = hs->hash_len;
  return derive_secret(hs, hs->client_traffic_secret_0, hs->hash_len,
                       (const uint8_t *)kTLS13LabelClientApplicationTraffic,
                       strlen(kTLS13LabelClientApplicationTraffic)) &&
         ssl_log_secret(ssl, "CLIENT_TRAFFIC_SECRET_0",
                        hs->client_traffic_secret_0, hs->hash_len) &&
         derive_secret(hs, hs->server_traffic_secret_0, hs->hash_len,
                       (const uint8_t *)kTLS13LabelServerApplicationTraffic,
                       strlen(kTLS13LabelServerApplicationTraffic)) &&
         ssl_log_secret(ssl, "SERVER_TRAFFIC_SECRET_0",
                        hs->server_traffic_secret_0, hs->hash_len) &&
         derive_secret(hs, ssl->s3->exporter_secret, hs->hash_len,
                       (const uint8_t *)kTLS13LabelExporter,
                       strlen(kTLS13LabelExporter));
}

static const char kTLS13LabelApplicationTraffic[] =
    "application traffic secret";

int tls13_rotate_traffic_key(SSL *ssl, enum evp_aead_direction_t direction) {
  const EVP_MD *digest = ssl_get_handshake_digest(
      SSL_get_session(ssl)->cipher->algorithm_prf, ssl3_protocol_version(ssl));

  uint8_t *secret;
  size_t secret_len;
  if (direction == evp_aead_open) {
    secret = ssl->s3->read_traffic_secret;
    secret_len = ssl->s3->read_traffic_secret_len;
  } else {
    secret = ssl->s3->write_traffic_secret;
    secret_len = ssl->s3->write_traffic_secret_len;
  }

  if (!hkdf_expand_label(secret, digest, secret, secret_len,
                         (const uint8_t *)kTLS13LabelApplicationTraffic,
                         strlen(kTLS13LabelApplicationTraffic), NULL, 0,
                         secret_len)) {
    return 0;
  }

  return tls13_set_traffic_key(ssl, direction, secret, secret_len);
}

static const char kTLS13LabelResumption[] = "resumption master secret";

int tls13_derive_resumption_secret(SSL_HANDSHAKE *hs) {
  if (hs->hash_len > SSL_MAX_MASTER_KEY_LENGTH) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  hs->new_session->master_key_length = hs->hash_len;
  return derive_secret(
      hs, hs->new_session->master_key, hs->new_session->master_key_length,
      (const uint8_t *)kTLS13LabelResumption, strlen(kTLS13LabelResumption));
}

static const char kTLS13LabelFinished[] = "finished";

/* tls13_verify_data sets |out| to be the HMAC of |context| using a derived
 * Finished key for both Finished messages and the PSK binder. */
static int tls13_verify_data(const EVP_MD *digest, uint8_t *out,
                             size_t *out_len, const uint8_t *secret,
                             size_t hash_len, uint8_t *context,
                             size_t context_len) {
  uint8_t key[EVP_MAX_MD_SIZE];
  unsigned len;
  if (!hkdf_expand_label(key, digest, secret, hash_len,
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
  if (!SSL_TRANSCRIPT_get_hash(&hs->transcript, context_hash,
                               &context_hash_len) ||
      !tls13_verify_data(SSL_TRANSCRIPT_md(&hs->transcript), out, out_len,
                         traffic_secret, hs->hash_len, context_hash,
                         context_hash_len)) {
    return 0;
  }
  return 1;
}

int tls13_export_keying_material(SSL *ssl, uint8_t *out, size_t out_len,
                                 const char *label, size_t label_len,
                                 const uint8_t *context, size_t context_len,
                                 int use_context) {
  const EVP_MD *digest = ssl_get_handshake_digest(
      SSL_get_session(ssl)->cipher->algorithm_prf, ssl3_protocol_version(ssl));

  const uint8_t *hash = NULL;
  size_t hash_len = 0;
  if (use_context) {
    hash = context;
    hash_len = context_len;
  }
  return hkdf_expand_label(out, digest, ssl->s3->exporter_secret,
                           ssl->s3->exporter_secret_len, (const uint8_t *)label,
                           label_len, hash, hash_len, out_len);
}

static const char kTLS13LabelPSKBinder[] = "resumption psk binder key";

static int tls13_psk_binder(uint8_t *out, const EVP_MD *digest, uint8_t *psk,
                            size_t psk_len, uint8_t *context,
                            size_t context_len, size_t hash_len) {
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

  uint8_t binder_key[EVP_MAX_MD_SIZE] = {0};
  size_t len;
  if (!hkdf_expand_label(binder_key, digest, early_secret, hash_len,
                         (const uint8_t *)kTLS13LabelPSKBinder,
                         strlen(kTLS13LabelPSKBinder), binder_context,
                         binder_context_len, hash_len) ||
      !tls13_verify_data(digest, out, &len, binder_key, hash_len, context,
                         context_len)) {
    return 0;
  }

  return 1;
}

int tls13_write_psk_binder(SSL_HANDSHAKE *hs, uint8_t *msg, size_t len) {
  SSL *const ssl = hs->ssl;
  const EVP_MD *digest = SSL_SESSION_get_digest(ssl->session, ssl);
  if (digest == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }
  size_t hash_len = EVP_MD_size(digest);

  if (len < hash_len + 3) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  EVP_MD_CTX ctx;
  EVP_MD_CTX_init(&ctx);
  uint8_t context[EVP_MAX_MD_SIZE];
  unsigned context_len;
  if (!EVP_DigestInit_ex(&ctx, digest, NULL) ||
      !EVP_DigestUpdate(&ctx, hs->transcript.buffer->data,
                        hs->transcript.buffer->length) ||
      !EVP_DigestUpdate(&ctx, msg, len - hash_len - 3) ||
      !EVP_DigestFinal_ex(&ctx, context, &context_len)) {
    EVP_MD_CTX_cleanup(&ctx);
    return 0;
  }

  EVP_MD_CTX_cleanup(&ctx);

  uint8_t verify_data[EVP_MAX_MD_SIZE] = {0};
  if (!tls13_psk_binder(verify_data, digest, ssl->session->master_key,
                        ssl->session->master_key_length, context, context_len,
                        hash_len)) {
    return 0;
  }

  OPENSSL_memcpy(msg + len - hash_len, verify_data, hash_len);
  return 1;
}

int tls13_verify_psk_binder(SSL_HANDSHAKE *hs, SSL_SESSION *session,
                            CBS *binders) {
  size_t hash_len = SSL_TRANSCRIPT_digest_len(&hs->transcript);

  /* Get the full ClientHello, including message header. It must be large enough
   * to exclude the binders. */
  CBS message;
  hs->ssl->method->get_current_message(hs->ssl, &message);
  if (CBS_len(&message) < CBS_len(binders) + 2) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  /* Hash a ClientHello prefix up to the binders. For now, this assumes we only
   * ever verify PSK binders on initial ClientHellos. */
  uint8_t context[EVP_MAX_MD_SIZE];
  unsigned context_len;
  if (!EVP_Digest(CBS_data(&message), CBS_len(&message) - CBS_len(binders) - 2,
                  context, &context_len, SSL_TRANSCRIPT_md(&hs->transcript),
                  NULL)) {
    return 0;
  }

  uint8_t verify_data[EVP_MAX_MD_SIZE] = {0};
  CBS binder;
  if (!tls13_psk_binder(verify_data, SSL_TRANSCRIPT_md(&hs->transcript),
                        session->master_key, session->master_key_length,
                        context, context_len, hash_len) ||
      /* We only consider the first PSK, so compare against the first binder. */
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
