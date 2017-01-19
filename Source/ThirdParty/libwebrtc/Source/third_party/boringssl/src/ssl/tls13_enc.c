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
#include <openssl/hmac.h>
#include <openssl/hkdf.h>
#include <openssl/mem.h>

#include "internal.h"


int tls13_init_key_schedule(SSL *ssl, const uint8_t *resumption_ctx,
                            size_t resumption_ctx_len) {
  SSL_HANDSHAKE *hs = ssl->s3->hs;
  const EVP_MD *digest = ssl_get_handshake_digest(ssl_get_algorithm_prf(ssl));

  hs->hash_len = EVP_MD_size(digest);

  /* Save the hash of the resumption context. */
  unsigned resumption_hash_len;
  if (!EVP_Digest(resumption_ctx, resumption_ctx_len, hs->resumption_hash,
                  &resumption_hash_len, digest, NULL)) {
    return 0;
  }

  /* Initialize the secret to the zero key. */
  memset(hs->secret, 0, hs->hash_len);

  /* Initialize the rolling hashes and release the handshake buffer. */
  if (!ssl3_init_handshake_hash(ssl)) {
    return 0;
  }
  ssl3_free_handshake_buffer(ssl);
  return 1;
}

int tls13_advance_key_schedule(SSL *ssl, const uint8_t *in, size_t len) {
  SSL_HANDSHAKE *hs = ssl->s3->hs;
  const EVP_MD *digest = ssl_get_handshake_digest(ssl_get_algorithm_prf(ssl));

  return HKDF_extract(hs->secret, &hs->hash_len, digest, in, len, hs->secret,
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

int tls13_get_context_hashes(SSL *ssl, uint8_t *out, size_t *out_len) {
  SSL_HANDSHAKE *hs = ssl->s3->hs;

  EVP_MD_CTX ctx;
  EVP_MD_CTX_init(&ctx);
  unsigned handshake_len = 0;
  int ok = EVP_MD_CTX_copy_ex(&ctx, &ssl->s3->handshake_hash) &&
           EVP_DigestFinal_ex(&ctx, out, &handshake_len);
  EVP_MD_CTX_cleanup(&ctx);
  if (!ok) {
    return 0;
  }

  memcpy(out + handshake_len, hs->resumption_hash, hs->hash_len);
  *out_len = handshake_len + hs->hash_len;
  return 1;
}

/* derive_secret derives a secret of length |len| and writes the result in |out|
 * with the given label and the current base secret and most recently-saved
 * handshake context. It returns one on success and zero on error. */
static int derive_secret(SSL *ssl, uint8_t *out, size_t len,
                         const uint8_t *label, size_t label_len) {
  SSL_HANDSHAKE *hs = ssl->s3->hs;
  const EVP_MD *digest = ssl_get_handshake_digest(ssl_get_algorithm_prf(ssl));

  uint8_t context_hashes[2 * EVP_MAX_MD_SIZE];
  size_t context_hashes_len;
  if (!tls13_get_context_hashes(ssl, context_hashes, &context_hashes_len)) {
    return 0;
  }

  return hkdf_expand_label(out, digest, hs->secret, hs->hash_len, label,
                           label_len, context_hashes, context_hashes_len, len);
}

int tls13_set_traffic_key(SSL *ssl, enum tls_record_type_t type,
                          enum evp_aead_direction_t direction,
                          const uint8_t *traffic_secret,
                          size_t traffic_secret_len) {
  if (traffic_secret_len > 0xff) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
    return 0;
  }

  const char *key_label, *iv_label;
  switch (type) {
    case type_early_handshake:
      key_label = "early handshake key expansion, key";
      iv_label = "early handshake key expansion, iv";
      break;
    case type_early_data:
      key_label = "early application data key expansion, key";
      iv_label = "early application data key expansion, iv";
      break;
    case type_handshake:
      key_label = "handshake key expansion, key";
      iv_label = "handshake key expansion, iv";
      break;
    case type_data:
      key_label = "application data key expansion, key";
      iv_label = "application data key expansion, iv";
      break;
    default:
      return 0;
  }

  /* Look up cipher suite properties. */
  const EVP_AEAD *aead;
  const EVP_MD *digest = ssl_get_handshake_digest(ssl_get_algorithm_prf(ssl));
  size_t mac_secret_len, fixed_iv_len;
  if (!ssl_cipher_get_evp_aead(&aead, &mac_secret_len, &fixed_iv_len,
                               SSL_get_session(ssl)->cipher,
                               ssl3_protocol_version(ssl))) {
    return 0;
  }

  /* Derive the key. */
  size_t key_len = EVP_AEAD_key_length(aead);
  uint8_t key[EVP_AEAD_MAX_KEY_LENGTH];
  if (!hkdf_expand_label(key, digest, traffic_secret, traffic_secret_len,
                         (const uint8_t *)key_label, strlen(key_label), NULL, 0,
                         key_len)) {
    return 0;
  }

  /* Derive the IV. */
  size_t iv_len = EVP_AEAD_nonce_length(aead);
  uint8_t iv[EVP_AEAD_MAX_NONCE_LENGTH];
  if (!hkdf_expand_label(iv, digest, traffic_secret, traffic_secret_len,
                         (const uint8_t *)iv_label, strlen(iv_label), NULL, 0,
                         iv_len)) {
    return 0;
  }

  SSL_AEAD_CTX *traffic_aead = SSL_AEAD_CTX_new(
      direction, ssl3_protocol_version(ssl), SSL_get_session(ssl)->cipher, key,
      key_len, NULL, 0, iv, iv_len);
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
    memmove(ssl->s3->read_traffic_secret, traffic_secret, traffic_secret_len);
    ssl->s3->read_traffic_secret_len = traffic_secret_len;
  } else {
    memmove(ssl->s3->write_traffic_secret, traffic_secret, traffic_secret_len);
    ssl->s3->write_traffic_secret_len = traffic_secret_len;
  }

  return 1;
}

static const char kTLS13LabelClientHandshakeTraffic[] =
    "client handshake traffic secret";
static const char kTLS13LabelServerHandshakeTraffic[] =
    "server handshake traffic secret";
static const char kTLS13LabelClientApplicationTraffic[] =
    "client application traffic secret";
static const char kTLS13LabelServerApplicationTraffic[] =
    "server application traffic secret";

int tls13_set_handshake_traffic(SSL *ssl) {
  SSL_HANDSHAKE *hs = ssl->s3->hs;

  uint8_t client_traffic_secret[EVP_MAX_MD_SIZE];
  uint8_t server_traffic_secret[EVP_MAX_MD_SIZE];
  if (!derive_secret(ssl, client_traffic_secret, hs->hash_len,
                     (const uint8_t *)kTLS13LabelClientHandshakeTraffic,
                     strlen(kTLS13LabelClientHandshakeTraffic)) ||
      !ssl_log_secret(ssl, "CLIENT_HANDSHAKE_TRAFFIC_SECRET",
                      client_traffic_secret, hs->hash_len) ||
      !derive_secret(ssl, server_traffic_secret, hs->hash_len,
                     (const uint8_t *)kTLS13LabelServerHandshakeTraffic,
                     strlen(kTLS13LabelServerHandshakeTraffic)) ||
      !ssl_log_secret(ssl, "SERVER_HANDSHAKE_TRAFFIC_SECRET",
                      server_traffic_secret, hs->hash_len)) {
    return 0;
  }

  if (ssl->server) {
    if (!tls13_set_traffic_key(ssl, type_handshake, evp_aead_open,
                               client_traffic_secret, hs->hash_len) ||
        !tls13_set_traffic_key(ssl, type_handshake, evp_aead_seal,
                               server_traffic_secret, hs->hash_len)) {
      return 0;
    }
  } else {
    if (!tls13_set_traffic_key(ssl, type_handshake, evp_aead_open,
                               server_traffic_secret, hs->hash_len) ||
        !tls13_set_traffic_key(ssl, type_handshake, evp_aead_seal,
                               client_traffic_secret, hs->hash_len)) {
      return 0;
    }
  }
  return 1;
}

int tls13_derive_traffic_secret_0(SSL *ssl) {
  SSL_HANDSHAKE *hs = ssl->s3->hs;

  return derive_secret(ssl, hs->client_traffic_secret_0, hs->hash_len,
                       (const uint8_t *)kTLS13LabelClientApplicationTraffic,
                       strlen(kTLS13LabelClientApplicationTraffic)) &&
         ssl_log_secret(ssl, "CLIENT_TRAFFIC_SECRET_0",
                        hs->client_traffic_secret_0, hs->hash_len) &&
         derive_secret(ssl, hs->server_traffic_secret_0, hs->hash_len,
                       (const uint8_t *)kTLS13LabelServerApplicationTraffic,
                       strlen(kTLS13LabelServerApplicationTraffic)) &&
         ssl_log_secret(ssl, "SERVER_TRAFFIC_SECRET_0",
                        hs->server_traffic_secret_0, hs->hash_len);
}

static const char kTLS13LabelApplicationTraffic[] =
    "application traffic secret";

int tls13_rotate_traffic_key(SSL *ssl, enum evp_aead_direction_t direction) {
  const EVP_MD *digest = ssl_get_handshake_digest(ssl_get_algorithm_prf(ssl));

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

  return tls13_set_traffic_key(ssl, type_data, direction, secret, secret_len);
}

static const char kTLS13LabelExporter[] = "exporter master secret";
static const char kTLS13LabelResumption[] = "resumption master secret";

int tls13_finalize_keys(SSL *ssl) {
  SSL_HANDSHAKE *hs = ssl->s3->hs;

  ssl->s3->exporter_secret_len = hs->hash_len;
  ssl->s3->new_session->master_key_length = hs->hash_len;
  if (!derive_secret(
          ssl, ssl->s3->exporter_secret, ssl->s3->exporter_secret_len,
          (const uint8_t *)kTLS13LabelExporter, strlen(kTLS13LabelExporter)) ||
      !derive_secret(ssl, ssl->s3->new_session->master_key,
                     ssl->s3->new_session->master_key_length,
                     (const uint8_t *)kTLS13LabelResumption,
                     strlen(kTLS13LabelResumption))) {
    return 0;
  }

  return 1;
}

int tls13_finished_mac(SSL *ssl, uint8_t *out, size_t *out_len, int is_server) {
  SSL_HANDSHAKE *hs = ssl->s3->hs;
  const EVP_MD *digest = ssl_get_handshake_digest(ssl_get_algorithm_prf(ssl));

  uint8_t key[EVP_MAX_MD_SIZE];
  size_t key_len = EVP_MD_size(digest);

  const uint8_t *traffic_secret;
  const char *label = "finished";
  if (is_server == ssl->server) {
    traffic_secret = ssl->s3->write_traffic_secret;
  } else {
    traffic_secret = ssl->s3->read_traffic_secret;
  }

  uint8_t context_hashes[2 * EVP_MAX_MD_SIZE];
  size_t context_hashes_len;
  unsigned len;
  if (!hkdf_expand_label(key, digest, traffic_secret, hs->hash_len,
                         (const uint8_t *)label, strlen(label), NULL, 0,
                         hs->hash_len) ||
      !tls13_get_context_hashes(ssl, context_hashes, &context_hashes_len) ||
      HMAC(digest, key, key_len, context_hashes, context_hashes_len, out,
           &len) == NULL) {
    return 0;
  }
  *out_len = len;
  return 1;
}

static const char kTLS13LabelResumptionPSK[] = "resumption psk";
static const char kTLS13LabelResumptionContext[] = "resumption context";

int tls13_resumption_psk(SSL *ssl, uint8_t *out, size_t out_len,
                         const SSL_SESSION *session) {
  const EVP_MD *digest = ssl_get_handshake_digest(ssl_get_algorithm_prf(ssl));
  return hkdf_expand_label(out, digest, session->master_key,
                           session->master_key_length,
                           (const uint8_t *)kTLS13LabelResumptionPSK,
                           strlen(kTLS13LabelResumptionPSK), NULL, 0, out_len);
}

int tls13_resumption_context(SSL *ssl, uint8_t *out, size_t out_len,
                             const SSL_SESSION *session) {
  const EVP_MD *digest = ssl_get_handshake_digest(ssl_get_algorithm_prf(ssl));
  return hkdf_expand_label(out, digest, session->master_key,
                           session->master_key_length,
                           (const uint8_t *)kTLS13LabelResumptionContext,
                           strlen(kTLS13LabelResumptionContext), NULL, 0,
                           out_len);
}

int tls13_export_keying_material(SSL *ssl, uint8_t *out, size_t out_len,
                                 const char *label, size_t label_len,
                                 const uint8_t *context, size_t context_len,
                                 int use_context) {
  const EVP_MD *digest = ssl_get_handshake_digest(ssl_get_algorithm_prf(ssl));

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
