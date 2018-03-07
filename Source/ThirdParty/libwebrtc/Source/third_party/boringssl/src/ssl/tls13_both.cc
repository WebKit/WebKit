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

#include <openssl/bytestring.h>
#include <openssl/err.h>
#include <openssl/hkdf.h>
#include <openssl/mem.h>
#include <openssl/stack.h>
#include <openssl/x509.h>

#include "../crypto/internal.h"
#include "internal.h"


namespace bssl {

// kMaxKeyUpdates is the number of consecutive KeyUpdates that will be
// processed. Without this limit an attacker could force unbounded processing
// without being able to return application data.
static const uint8_t kMaxKeyUpdates = 32;

const uint8_t kHelloRetryRequest[SSL3_RANDOM_SIZE] = {
    0xcf, 0x21, 0xad, 0x74, 0xe5, 0x9a, 0x61, 0x11, 0xbe, 0x1d, 0x8c,
    0x02, 0x1e, 0x65, 0xb8, 0x91, 0xc2, 0xa2, 0x11, 0x16, 0x7a, 0xbb,
    0x8c, 0x5e, 0x07, 0x9e, 0x09, 0xe2, 0xc8, 0xa8, 0x33, 0x9c,
};

bool tls13_get_cert_verify_signature_input(
    SSL_HANDSHAKE *hs, Array<uint8_t> *out,
    enum ssl_cert_verify_context_t cert_verify_context) {
  ScopedCBB cbb;
  if (!CBB_init(cbb.get(), 64 + 33 + 1 + 2 * EVP_MAX_MD_SIZE)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return false;
  }

  for (size_t i = 0; i < 64; i++) {
    if (!CBB_add_u8(cbb.get(), 0x20)) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
      return false;
    }
  }

  Span<const char> context;
  if (cert_verify_context == ssl_cert_verify_server) {
    static const char kContext[] = "TLS 1.3, server CertificateVerify";
    context = kContext;
  } else if (cert_verify_context == ssl_cert_verify_client) {
    static const char kContext[] = "TLS 1.3, client CertificateVerify";
    context = kContext;
  } else if (cert_verify_context == ssl_cert_verify_channel_id) {
    static const char kContext[] = "TLS 1.3, Channel ID";
    context = kContext;
  } else {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return false;
  }

  // Note |context| includes the NUL byte separator.
  if (!CBB_add_bytes(cbb.get(),
                     reinterpret_cast<const uint8_t *>(context.data()),
                     context.size())) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return false;
  }

  uint8_t context_hash[EVP_MAX_MD_SIZE];
  size_t context_hash_len;
  if (!hs->transcript.GetHash(context_hash, &context_hash_len) ||
      !CBB_add_bytes(cbb.get(), context_hash, context_hash_len) ||
      !CBBFinishArray(cbb.get(), out)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return false;
  }

  return true;
}

int tls13_process_certificate(SSL_HANDSHAKE *hs, const SSLMessage &msg,
                              int allow_anonymous) {
  SSL *const ssl = hs->ssl;
  CBS body = msg.body, context, certificate_list;
  if (!CBS_get_u8_length_prefixed(&body, &context) ||
      CBS_len(&context) != 0 ||
      !CBS_get_u24_length_prefixed(&body, &certificate_list) ||
      CBS_len(&body) != 0) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return 0;
  }

  UniquePtr<STACK_OF(CRYPTO_BUFFER)> certs(sk_CRYPTO_BUFFER_new_null());
  if (!certs) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return 0;
  }

  const bool retain_sha256 =
      ssl->server && ssl->retain_only_sha256_of_client_certs;
  UniquePtr<EVP_PKEY> pkey;
  while (CBS_len(&certificate_list) > 0) {
    CBS certificate, extensions;
    if (!CBS_get_u24_length_prefixed(&certificate_list, &certificate) ||
        !CBS_get_u16_length_prefixed(&certificate_list, &extensions) ||
        CBS_len(&certificate) == 0) {
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
      OPENSSL_PUT_ERROR(SSL, SSL_R_CERT_LENGTH_MISMATCH);
      return 0;
    }

    if (sk_CRYPTO_BUFFER_num(certs.get()) == 0) {
      pkey = ssl_cert_parse_pubkey(&certificate);
      if (!pkey) {
        ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
        OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
        return 0;
      }
      // TLS 1.3 always uses certificate keys for signing thus the correct
      // keyUsage is enforced.
      if (!ssl_cert_check_digital_signature_key_usage(&certificate)) {
        ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
        return 0;
      }

      if (retain_sha256) {
        // Retain the hash of the leaf certificate if requested.
        SHA256(CBS_data(&certificate), CBS_len(&certificate),
               hs->new_session->peer_sha256);
      }
    }

    UniquePtr<CRYPTO_BUFFER> buf(
        CRYPTO_BUFFER_new_from_CBS(&certificate, ssl->ctx->pool));
    if (!buf ||
        !PushToStack(certs.get(), std::move(buf))) {
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
      return 0;
    }

    // Parse out the extensions.
    bool have_status_request = false, have_sct = false;
    CBS status_request, sct;
    const SSL_EXTENSION_TYPE ext_types[] = {
        {TLSEXT_TYPE_status_request, &have_status_request, &status_request},
        {TLSEXT_TYPE_certificate_timestamp, &have_sct, &sct},
    };

    uint8_t alert = SSL_AD_DECODE_ERROR;
    if (!ssl_parse_extensions(&extensions, &alert, ext_types,
                              OPENSSL_ARRAY_SIZE(ext_types),
                              0 /* reject unknown */)) {
      ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
      return 0;
    }

    // All Certificate extensions are parsed, but only the leaf extensions are
    // stored.
    if (have_status_request) {
      if (ssl->server || !ssl->ocsp_stapling_enabled) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_EXTENSION);
        ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNSUPPORTED_EXTENSION);
        return 0;
      }

      uint8_t status_type;
      CBS ocsp_response;
      if (!CBS_get_u8(&status_request, &status_type) ||
          status_type != TLSEXT_STATUSTYPE_ocsp ||
          !CBS_get_u24_length_prefixed(&status_request, &ocsp_response) ||
          CBS_len(&ocsp_response) == 0 ||
          CBS_len(&status_request) != 0) {
        ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
        return 0;
      }

      if (sk_CRYPTO_BUFFER_num(certs.get()) == 1) {
        CRYPTO_BUFFER_free(hs->new_session->ocsp_response);
        hs->new_session->ocsp_response =
            CRYPTO_BUFFER_new_from_CBS(&ocsp_response, ssl->ctx->pool);
        if (hs->new_session->ocsp_response == nullptr) {
          ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
          return 0;
        }
      }
    }

    if (have_sct) {
      if (ssl->server || !ssl->signed_cert_timestamps_enabled) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_EXTENSION);
        ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNSUPPORTED_EXTENSION);
        return 0;
      }

      if (!ssl_is_sct_list_valid(&sct)) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_ERROR_PARSING_EXTENSION);
        ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
        return 0;
      }

      if (sk_CRYPTO_BUFFER_num(certs.get()) == 1) {
        CRYPTO_BUFFER_free(hs->new_session->signed_cert_timestamp_list);
        hs->new_session->signed_cert_timestamp_list =
            CRYPTO_BUFFER_new_from_CBS(&sct, ssl->ctx->pool);
        if (hs->new_session->signed_cert_timestamp_list == nullptr) {
          ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
          return 0;
        }
      }
    }
  }

  // Store a null certificate list rather than an empty one if the peer didn't
  // send certificates.
  if (sk_CRYPTO_BUFFER_num(certs.get()) == 0) {
    certs.reset();
  }

  hs->peer_pubkey = std::move(pkey);

  sk_CRYPTO_BUFFER_pop_free(hs->new_session->certs, CRYPTO_BUFFER_free);
  hs->new_session->certs = certs.release();

  if (!ssl->ctx->x509_method->session_cache_objects(hs->new_session.get())) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return 0;
  }

  if (sk_CRYPTO_BUFFER_num(hs->new_session->certs) == 0) {
    if (!allow_anonymous) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_PEER_DID_NOT_RETURN_A_CERTIFICATE);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_CERTIFICATE_REQUIRED);
      return 0;
    }

    // OpenSSL returns X509_V_OK when no certificates are requested. This is
    // classed by them as a bug, but it's assumed by at least NGINX.
    hs->new_session->verify_result = X509_V_OK;

    // No certificate, so nothing more to do.
    return 1;
  }

  hs->new_session->peer_sha256_valid = retain_sha256;
  return 1;
}

int tls13_process_certificate_verify(SSL_HANDSHAKE *hs, const SSLMessage &msg) {
  SSL *const ssl = hs->ssl;
  if (hs->peer_pubkey == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  CBS body = msg.body, signature;
  uint16_t signature_algorithm;
  if (!CBS_get_u16(&body, &signature_algorithm) ||
      !CBS_get_u16_length_prefixed(&body, &signature) ||
      CBS_len(&body) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return 0;
  }

  uint8_t alert = SSL_AD_DECODE_ERROR;
  if (!tls12_check_peer_sigalg(ssl, &alert, signature_algorithm)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
    return 0;
  }
  hs->new_session->peer_signature_algorithm = signature_algorithm;

  Array<uint8_t> input;
  if (!tls13_get_cert_verify_signature_input(
          hs, &input,
          ssl->server ? ssl_cert_verify_client : ssl_cert_verify_server)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
    return 0;
  }

  bool sig_ok = ssl_public_key_verify(ssl, signature, signature_algorithm,
                                      hs->peer_pubkey.get(), input);
#if defined(BORINGSSL_UNSAFE_FUZZER_MODE)
  sig_ok = true;
  ERR_clear_error();
#endif
  if (!sig_ok) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_SIGNATURE);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECRYPT_ERROR);
    return 0;
  }

  return 1;
}

int tls13_process_finished(SSL_HANDSHAKE *hs, const SSLMessage &msg,
                           int use_saved_value) {
  SSL *const ssl = hs->ssl;
  uint8_t verify_data_buf[EVP_MAX_MD_SIZE];
  const uint8_t *verify_data;
  size_t verify_data_len;
  if (use_saved_value) {
    assert(ssl->server);
    verify_data = hs->expected_client_finished;
    verify_data_len = hs->hash_len;
  } else {
    if (!tls13_finished_mac(hs, verify_data_buf, &verify_data_len,
                            !ssl->server)) {
      return 0;
    }
    verify_data = verify_data_buf;
  }

  int finished_ok = CBS_mem_equal(&msg.body, verify_data, verify_data_len);
#if defined(BORINGSSL_UNSAFE_FUZZER_MODE)
  finished_ok = 1;
#endif
  if (!finished_ok) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECRYPT_ERROR);
    OPENSSL_PUT_ERROR(SSL, SSL_R_DIGEST_CHECK_FAILED);
    return 0;
  }

  return 1;
}

int tls13_add_certificate(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  ScopedCBB cbb;
  CBB body, certificate_list;
  if (!ssl->method->init_message(ssl, cbb.get(), &body, SSL3_MT_CERTIFICATE) ||
      // The request context is always empty in the handshake.
      !CBB_add_u8(&body, 0) ||
      !CBB_add_u24_length_prefixed(&body, &certificate_list)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  if (!ssl_has_certificate(ssl)) {
    return ssl_add_message_cbb(ssl, cbb.get());
  }

  CERT *cert = ssl->cert;
  CRYPTO_BUFFER *leaf_buf = sk_CRYPTO_BUFFER_value(cert->chain, 0);
  CBB leaf, extensions;
  if (!CBB_add_u24_length_prefixed(&certificate_list, &leaf) ||
      !CBB_add_bytes(&leaf, CRYPTO_BUFFER_data(leaf_buf),
                     CRYPTO_BUFFER_len(leaf_buf)) ||
      !CBB_add_u16_length_prefixed(&certificate_list, &extensions)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  if (hs->scts_requested && ssl->cert->signed_cert_timestamp_list != NULL) {
    CBB contents;
    if (!CBB_add_u16(&extensions, TLSEXT_TYPE_certificate_timestamp) ||
        !CBB_add_u16_length_prefixed(&extensions, &contents) ||
        !CBB_add_bytes(
            &contents,
            CRYPTO_BUFFER_data(ssl->cert->signed_cert_timestamp_list),
            CRYPTO_BUFFER_len(ssl->cert->signed_cert_timestamp_list)) ||
        !CBB_flush(&extensions)) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return 0;
    }
  }

  if (hs->ocsp_stapling_requested &&
      ssl->cert->ocsp_response != NULL) {
    CBB contents, ocsp_response;
    if (!CBB_add_u16(&extensions, TLSEXT_TYPE_status_request) ||
        !CBB_add_u16_length_prefixed(&extensions, &contents) ||
        !CBB_add_u8(&contents, TLSEXT_STATUSTYPE_ocsp) ||
        !CBB_add_u24_length_prefixed(&contents, &ocsp_response) ||
        !CBB_add_bytes(&ocsp_response,
                       CRYPTO_BUFFER_data(ssl->cert->ocsp_response),
                       CRYPTO_BUFFER_len(ssl->cert->ocsp_response)) ||
        !CBB_flush(&extensions)) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return 0;
    }
  }

  for (size_t i = 1; i < sk_CRYPTO_BUFFER_num(cert->chain); i++) {
    CRYPTO_BUFFER *cert_buf = sk_CRYPTO_BUFFER_value(cert->chain, i);
    CBB child;
    if (!CBB_add_u24_length_prefixed(&certificate_list, &child) ||
        !CBB_add_bytes(&child, CRYPTO_BUFFER_data(cert_buf),
                       CRYPTO_BUFFER_len(cert_buf)) ||
        !CBB_add_u16(&certificate_list, 0 /* no extensions */)) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return 0;
    }
  }

  return ssl_add_message_cbb(ssl, cbb.get());
}

enum ssl_private_key_result_t tls13_add_certificate_verify(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  uint16_t signature_algorithm;
  if (!tls1_choose_signature_algorithm(hs, &signature_algorithm)) {
    return ssl_private_key_failure;
  }

  ScopedCBB cbb;
  CBB body;
  if (!ssl->method->init_message(ssl, cbb.get(), &body,
                                 SSL3_MT_CERTIFICATE_VERIFY) ||
      !CBB_add_u16(&body, signature_algorithm)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return ssl_private_key_failure;
  }

  // Sign the digest.
  CBB child;
  const size_t max_sig_len = EVP_PKEY_size(hs->local_pubkey.get());
  uint8_t *sig;
  size_t sig_len;
  if (!CBB_add_u16_length_prefixed(&body, &child) ||
      !CBB_reserve(&child, &sig, max_sig_len)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
    return ssl_private_key_failure;
  }

  Array<uint8_t> msg;
  if (!tls13_get_cert_verify_signature_input(
          hs, &msg,
          ssl->server ? ssl_cert_verify_server : ssl_cert_verify_client)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
    return ssl_private_key_failure;
  }

  enum ssl_private_key_result_t sign_result = ssl_private_key_sign(
      hs, sig, &sig_len, max_sig_len, signature_algorithm, msg);
  if (sign_result != ssl_private_key_success) {
    return sign_result;
  }

  if (!CBB_did_write(&child, sig_len) ||
      !ssl_add_message_cbb(ssl, cbb.get())) {
    return ssl_private_key_failure;
  }

  return ssl_private_key_success;
}

int tls13_add_finished(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  size_t verify_data_len;
  uint8_t verify_data[EVP_MAX_MD_SIZE];

  if (!tls13_finished_mac(hs, verify_data, &verify_data_len, ssl->server)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
    OPENSSL_PUT_ERROR(SSL, SSL_R_DIGEST_CHECK_FAILED);
    return 0;
  }

  ScopedCBB cbb;
  CBB body;
  if (!ssl->method->init_message(ssl, cbb.get(), &body, SSL3_MT_FINISHED) ||
      !CBB_add_bytes(&body, verify_data, verify_data_len) ||
      !ssl_add_message_cbb(ssl, cbb.get())) {
    return 0;
  }

  return 1;
}

static int tls13_receive_key_update(SSL *ssl, const SSLMessage &msg) {
  CBS body = msg.body;
  uint8_t key_update_request;
  if (!CBS_get_u8(&body, &key_update_request) ||
      CBS_len(&body) != 0 ||
      (key_update_request != SSL_KEY_UPDATE_NOT_REQUESTED &&
       key_update_request != SSL_KEY_UPDATE_REQUESTED)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return 0;
  }

  if (!tls13_rotate_traffic_key(ssl, evp_aead_open)) {
    return 0;
  }

  // Acknowledge the KeyUpdate
  if (key_update_request == SSL_KEY_UPDATE_REQUESTED &&
      !ssl->s3->key_update_pending) {
    ScopedCBB cbb;
    CBB body_cbb;
    if (!ssl->method->init_message(ssl, cbb.get(), &body_cbb,
                                   SSL3_MT_KEY_UPDATE) ||
        !CBB_add_u8(&body_cbb, SSL_KEY_UPDATE_NOT_REQUESTED) ||
        !ssl_add_message_cbb(ssl, cbb.get()) ||
        !tls13_rotate_traffic_key(ssl, evp_aead_seal)) {
      return 0;
    }

    // Suppress KeyUpdate acknowledgments until this change is written to the
    // wire. This prevents us from accumulating write obligations when read and
    // write progress at different rates. See draft-ietf-tls-tls13-18, section
    // 4.5.3.
    ssl->s3->key_update_pending = true;
  }

  return 1;
}

int tls13_post_handshake(SSL *ssl, const SSLMessage &msg) {
  if (msg.type == SSL3_MT_KEY_UPDATE) {
    ssl->s3->key_update_count++;
    if (ssl->s3->key_update_count > kMaxKeyUpdates) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_TOO_MANY_KEY_UPDATES);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
      return 0;
    }

    return tls13_receive_key_update(ssl, msg);
  }

  ssl->s3->key_update_count = 0;

  if (msg.type == SSL3_MT_NEW_SESSION_TICKET && !ssl->server) {
    return tls13_process_new_session_ticket(ssl, msg);
  }

  ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
  OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_MESSAGE);
  return 0;
}

}  // namespace bssl
