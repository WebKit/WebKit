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

#include <openssl/bytestring.h>
#include <openssl/err.h>
#include <openssl/hkdf.h>
#include <openssl/mem.h>
#include <openssl/stack.h>
#include <openssl/x509.h>

#include "../crypto/internal.h"
#include "internal.h"


/* kMaxKeyUpdates is the number of consecutive KeyUpdates that will be
 * processed. Without this limit an attacker could force unbounded processing
 * without being able to return application data. */
static const uint8_t kMaxKeyUpdates = 32;

int tls13_handshake(SSL_HANDSHAKE *hs, int *out_early_return) {
  SSL *const ssl = hs->ssl;
  for (;;) {
    /* Resolve the operation the handshake was waiting on. */
    switch (hs->wait) {
      case ssl_hs_error:
        OPENSSL_PUT_ERROR(SSL, SSL_R_SSL_HANDSHAKE_FAILURE);
        return -1;

      case ssl_hs_flush:
      case ssl_hs_flush_and_read_message: {
        int ret = ssl->method->flush_flight(ssl);
        if (ret <= 0) {
          return ret;
        }
        if (hs->wait != ssl_hs_flush_and_read_message) {
          break;
        }
        ssl->method->expect_flight(ssl);
        hs->wait = ssl_hs_read_message;
        /* Fall-through. */
      }

      case ssl_hs_read_message: {
        int ret = ssl->method->ssl_get_message(ssl);
        if (ret <= 0) {
          return ret;
        }
        break;
      }

      case ssl_hs_read_end_of_early_data: {
        if (ssl->s3->hs->can_early_read) {
          /* While we are processing early data, the handshake returns early. */
          *out_early_return = 1;
          return 1;
        }
        hs->wait = ssl_hs_ok;
        break;
      }

      case ssl_hs_x509_lookup:
        ssl->rwstate = SSL_X509_LOOKUP;
        hs->wait = ssl_hs_ok;
        return -1;

      case ssl_hs_channel_id_lookup:
        ssl->rwstate = SSL_CHANNEL_ID_LOOKUP;
        hs->wait = ssl_hs_ok;
        return -1;

      case ssl_hs_private_key_operation:
        ssl->rwstate = SSL_PRIVATE_KEY_OPERATION;
        hs->wait = ssl_hs_ok;
        return -1;

      case ssl_hs_pending_ticket:
        ssl->rwstate = SSL_PENDING_TICKET;
        hs->wait = ssl_hs_ok;
        return -1;

      case ssl_hs_ok:
        break;
    }

    /* Run the state machine again. */
    hs->wait = hs->do_tls13_handshake(hs);
    if (hs->wait == ssl_hs_error) {
      /* Don't loop around to avoid a stray |SSL_R_SSL_HANDSHAKE_FAILURE| the
       * first time around. */
      return -1;
    }
    if (hs->wait == ssl_hs_ok) {
      /* The handshake has completed. */
      return 1;
    }

    /* Otherwise, loop to the beginning and resolve what was blocking the
     * handshake. */
  }
}

int tls13_get_cert_verify_signature_input(
    SSL_HANDSHAKE *hs, uint8_t **out, size_t *out_len,
    enum ssl_cert_verify_context_t cert_verify_context) {
  CBB cbb;
  if (!CBB_init(&cbb, 64 + 33 + 1 + 2 * EVP_MAX_MD_SIZE)) {
    goto err;
  }

  for (size_t i = 0; i < 64; i++) {
    if (!CBB_add_u8(&cbb, 0x20)) {
      goto err;
    }
  }

  const uint8_t *context;
  size_t context_len;
  if (cert_verify_context == ssl_cert_verify_server) {
    /* Include the NUL byte. */
    static const char kContext[] = "TLS 1.3, server CertificateVerify";
    context = (const uint8_t *)kContext;
    context_len = sizeof(kContext);
  } else if (cert_verify_context == ssl_cert_verify_client) {
    static const char kContext[] = "TLS 1.3, client CertificateVerify";
    context = (const uint8_t *)kContext;
    context_len = sizeof(kContext);
  } else if (cert_verify_context == ssl_cert_verify_channel_id) {
    static const char kContext[] = "TLS 1.3, Channel ID";
    context = (const uint8_t *)kContext;
    context_len = sizeof(kContext);
  } else {
    goto err;
  }

  if (!CBB_add_bytes(&cbb, context, context_len)) {
    goto err;
  }

  uint8_t context_hash[EVP_MAX_MD_SIZE];
  size_t context_hash_len;
  if (!SSL_TRANSCRIPT_get_hash(&hs->transcript, context_hash,
                               &context_hash_len) ||
      !CBB_add_bytes(&cbb, context_hash, context_hash_len) ||
      !CBB_finish(&cbb, out, out_len)) {
    goto err;
  }

  return 1;

err:
  OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
  CBB_cleanup(&cbb);
  return 0;
}

int tls13_process_certificate(SSL_HANDSHAKE *hs, int allow_anonymous) {
  SSL *const ssl = hs->ssl;
  CBS cbs, context, certificate_list;
  CBS_init(&cbs, ssl->init_msg, ssl->init_num);
  if (!CBS_get_u8_length_prefixed(&cbs, &context) ||
      CBS_len(&context) != 0) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return 0;
  }

  const int retain_sha256 =
      ssl->server && ssl->retain_only_sha256_of_client_certs;
  int ret = 0;

  EVP_PKEY *pkey = NULL;
  STACK_OF(CRYPTO_BUFFER) *certs = sk_CRYPTO_BUFFER_new_null();
  if (certs == NULL) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    goto err;
  }

  if (!CBS_get_u24_length_prefixed(&cbs, &certificate_list)) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    goto err;
  }

  while (CBS_len(&certificate_list) > 0) {
    CBS certificate, extensions;
    if (!CBS_get_u24_length_prefixed(&certificate_list, &certificate) ||
        !CBS_get_u16_length_prefixed(&certificate_list, &extensions) ||
        CBS_len(&certificate) == 0) {
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
      OPENSSL_PUT_ERROR(SSL, SSL_R_CERT_LENGTH_MISMATCH);
      goto err;
    }

    if (sk_CRYPTO_BUFFER_num(certs) == 0) {
      pkey = ssl_cert_parse_pubkey(&certificate);
      if (pkey == NULL) {
        ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
        OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
        goto err;
      }
      /* TLS 1.3 always uses certificate keys for signing thus the correct
       * keyUsage is enforced. */
      if (!ssl_cert_check_digital_signature_key_usage(&certificate)) {
        ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
        goto err;
      }

      if (retain_sha256) {
        /* Retain the hash of the leaf certificate if requested. */
        SHA256(CBS_data(&certificate), CBS_len(&certificate),
               hs->new_session->peer_sha256);
      }
    }

    CRYPTO_BUFFER *buf =
        CRYPTO_BUFFER_new_from_CBS(&certificate, ssl->ctx->pool);
    if (buf == NULL ||
        !sk_CRYPTO_BUFFER_push(certs, buf)) {
      CRYPTO_BUFFER_free(buf);
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
      goto err;
    }

    /* Parse out the extensions. */
    int have_status_request = 0, have_sct = 0;
    CBS status_request, sct;
    const SSL_EXTENSION_TYPE ext_types[] = {
        {TLSEXT_TYPE_status_request, &have_status_request, &status_request},
        {TLSEXT_TYPE_certificate_timestamp, &have_sct, &sct},
    };

    uint8_t alert = SSL_AD_DECODE_ERROR;
    if (!ssl_parse_extensions(&extensions, &alert, ext_types,
                              OPENSSL_ARRAY_SIZE(ext_types),
                              0 /* reject unknown */)) {
      ssl3_send_alert(ssl, SSL3_AL_FATAL, alert);
      goto err;
    }

    /* All Certificate extensions are parsed, but only the leaf extensions are
     * stored. */
    if (have_status_request) {
      if (ssl->server || !ssl->ocsp_stapling_enabled) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_EXTENSION);
        ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNSUPPORTED_EXTENSION);
        goto err;
      }

      uint8_t status_type;
      CBS ocsp_response;
      if (!CBS_get_u8(&status_request, &status_type) ||
          status_type != TLSEXT_STATUSTYPE_ocsp ||
          !CBS_get_u24_length_prefixed(&status_request, &ocsp_response) ||
          CBS_len(&ocsp_response) == 0 ||
          CBS_len(&status_request) != 0) {
        ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
        goto err;
      }

      if (sk_CRYPTO_BUFFER_num(certs) == 1 &&
          !CBS_stow(&ocsp_response, &hs->new_session->ocsp_response,
                    &hs->new_session->ocsp_response_length)) {
        ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
        goto err;
      }
    }

    if (have_sct) {
      if (ssl->server || !ssl->signed_cert_timestamps_enabled) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_EXTENSION);
        ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNSUPPORTED_EXTENSION);
        goto err;
      }

      if (!ssl_is_sct_list_valid(&sct)) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_ERROR_PARSING_EXTENSION);
        ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
        goto err;
      }

      if (sk_CRYPTO_BUFFER_num(certs) == 1 &&
          !CBS_stow(
              &sct, &hs->new_session->tlsext_signed_cert_timestamp_list,
              &hs->new_session->tlsext_signed_cert_timestamp_list_length)) {
        ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
        goto err;
      }
    }
  }

  if (CBS_len(&cbs) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    goto err;
  }

  EVP_PKEY_free(hs->peer_pubkey);
  hs->peer_pubkey = pkey;
  pkey = NULL;

  sk_CRYPTO_BUFFER_pop_free(hs->new_session->certs, CRYPTO_BUFFER_free);
  hs->new_session->certs = certs;
  certs = NULL;

  if (!ssl->ctx->x509_method->session_cache_objects(hs->new_session)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    goto err;
  }

  if (sk_CRYPTO_BUFFER_num(hs->new_session->certs) == 0) {
    if (!allow_anonymous) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_PEER_DID_NOT_RETURN_A_CERTIFICATE);
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_CERTIFICATE_REQUIRED);
      goto err;
    }

    /* OpenSSL returns X509_V_OK when no certificates are requested. This is
     * classed by them as a bug, but it's assumed by at least NGINX. */
    hs->new_session->verify_result = X509_V_OK;

    /* No certificate, so nothing more to do. */
    ret = 1;
    goto err;
  }

  hs->new_session->peer_sha256_valid = retain_sha256;

  if (!ssl->ctx->x509_method->session_verify_cert_chain(hs->new_session,
                                                        ssl)) {
    goto err;
  }

  ret = 1;

err:
  sk_CRYPTO_BUFFER_pop_free(certs, CRYPTO_BUFFER_free);
  EVP_PKEY_free(pkey);
  return ret;
}

int tls13_process_certificate_verify(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  int ret = 0;
  uint8_t *msg = NULL;
  size_t msg_len;

  if (hs->peer_pubkey == NULL) {
    goto err;
  }

  CBS cbs, signature;
  uint16_t signature_algorithm;
  CBS_init(&cbs, ssl->init_msg, ssl->init_num);
  if (!CBS_get_u16(&cbs, &signature_algorithm) ||
      !CBS_get_u16_length_prefixed(&cbs, &signature) ||
      CBS_len(&cbs) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    goto err;
  }

  int al;
  if (!tls12_check_peer_sigalg(ssl, &al, signature_algorithm)) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, al);
    goto err;
  }
  hs->new_session->peer_signature_algorithm = signature_algorithm;

  if (!tls13_get_cert_verify_signature_input(
          hs, &msg, &msg_len,
          ssl->server ? ssl_cert_verify_client : ssl_cert_verify_server)) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
    goto err;
  }

  int sig_ok =
      ssl_public_key_verify(ssl, CBS_data(&signature), CBS_len(&signature),
                            signature_algorithm, hs->peer_pubkey, msg, msg_len);
#if defined(BORINGSSL_UNSAFE_FUZZER_MODE)
  sig_ok = 1;
  ERR_clear_error();
#endif
  if (!sig_ok) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_SIGNATURE);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECRYPT_ERROR);
    goto err;
  }

  ret = 1;

err:
  OPENSSL_free(msg);
  return ret;
}

int tls13_process_finished(SSL_HANDSHAKE *hs, int use_saved_value) {
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

  int finished_ok =
      ssl->init_num == verify_data_len &&
      CRYPTO_memcmp(verify_data, ssl->init_msg, verify_data_len) == 0;
#if defined(BORINGSSL_UNSAFE_FUZZER_MODE)
  finished_ok = 1;
#endif
  if (!finished_ok) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECRYPT_ERROR);
    OPENSSL_PUT_ERROR(SSL, SSL_R_DIGEST_CHECK_FAILED);
    return 0;
  }

  return 1;
}

int tls13_add_certificate(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  CBB cbb, body, certificate_list;
  if (!ssl->method->init_message(ssl, &cbb, &body, SSL3_MT_CERTIFICATE) ||
      /* The request context is always empty in the handshake. */
      !CBB_add_u8(&body, 0) ||
      !CBB_add_u24_length_prefixed(&body, &certificate_list)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    goto err;
  }

  if (!ssl_has_certificate(ssl)) {
    if (!ssl_add_message_cbb(ssl, &cbb)) {
      goto err;
    }

    return 1;
  }

  CERT *cert = ssl->cert;
  CRYPTO_BUFFER *leaf_buf = sk_CRYPTO_BUFFER_value(cert->chain, 0);
  CBB leaf, extensions;
  if (!CBB_add_u24_length_prefixed(&certificate_list, &leaf) ||
      !CBB_add_bytes(&leaf, CRYPTO_BUFFER_data(leaf_buf),
                     CRYPTO_BUFFER_len(leaf_buf)) ||
      !CBB_add_u16_length_prefixed(&certificate_list, &extensions)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    goto err;
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
      goto err;
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
      goto err;
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
      goto err;
    }
  }

  if (!ssl_add_message_cbb(ssl, &cbb)) {
    goto err;
  }

  return 1;

err:
  CBB_cleanup(&cbb);
  return 0;
}

enum ssl_private_key_result_t tls13_add_certificate_verify(SSL_HANDSHAKE *hs,
                                                           int is_first_run) {
  SSL *const ssl = hs->ssl;
  enum ssl_private_key_result_t ret = ssl_private_key_failure;
  uint8_t *msg = NULL;
  size_t msg_len;
  CBB cbb, body;
  CBB_zero(&cbb);

  uint16_t signature_algorithm;
  if (!tls1_choose_signature_algorithm(hs, &signature_algorithm)) {
    goto err;
  }
  if (!ssl->method->init_message(ssl, &cbb, &body,
                                 SSL3_MT_CERTIFICATE_VERIFY) ||
      !CBB_add_u16(&body, signature_algorithm)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    goto err;
  }

  /* Sign the digest. */
  CBB child;
  const size_t max_sig_len = EVP_PKEY_size(hs->local_pubkey);
  uint8_t *sig;
  size_t sig_len;
  if (!CBB_add_u16_length_prefixed(&body, &child) ||
      !CBB_reserve(&child, &sig, max_sig_len)) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
    goto err;
  }

  enum ssl_private_key_result_t sign_result;
  if (is_first_run) {
    if (!tls13_get_cert_verify_signature_input(
            hs, &msg, &msg_len,
            ssl->server ? ssl_cert_verify_server : ssl_cert_verify_client)) {
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      goto err;
    }
    sign_result = ssl_private_key_sign(ssl, sig, &sig_len, max_sig_len,
                                       signature_algorithm, msg, msg_len);
  } else {
    sign_result = ssl_private_key_complete(ssl, sig, &sig_len, max_sig_len);
  }

  if (sign_result != ssl_private_key_success) {
    ret = sign_result;
    goto err;
  }

  if (!CBB_did_write(&child, sig_len) ||
      !ssl_add_message_cbb(ssl, &cbb)) {
    goto err;
  }

  ret = ssl_private_key_success;

err:
  CBB_cleanup(&cbb);
  OPENSSL_free(msg);
  return ret;
}

int tls13_add_finished(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  size_t verify_data_len;
  uint8_t verify_data[EVP_MAX_MD_SIZE];

  if (!tls13_finished_mac(hs, verify_data, &verify_data_len, ssl->server)) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
    OPENSSL_PUT_ERROR(SSL, SSL_R_DIGEST_CHECK_FAILED);
    return 0;
  }

  CBB cbb, body;
  if (!ssl->method->init_message(ssl, &cbb, &body, SSL3_MT_FINISHED) ||
      !CBB_add_bytes(&body, verify_data, verify_data_len) ||
      !ssl_add_message_cbb(ssl, &cbb)) {
    CBB_cleanup(&cbb);
    return 0;
  }

  return 1;
}

static int tls13_receive_key_update(SSL *ssl) {
  CBS cbs;
  uint8_t key_update_request;
  CBS_init(&cbs, ssl->init_msg, ssl->init_num);
  if (!CBS_get_u8(&cbs, &key_update_request) ||
      CBS_len(&cbs) != 0 ||
      (key_update_request != SSL_KEY_UPDATE_NOT_REQUESTED &&
       key_update_request != SSL_KEY_UPDATE_REQUESTED)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return 0;
  }

  if (!tls13_rotate_traffic_key(ssl, evp_aead_open)) {
    return 0;
  }

  /* Acknowledge the KeyUpdate */
  if (key_update_request == SSL_KEY_UPDATE_REQUESTED &&
      !ssl->s3->key_update_pending) {
    CBB cbb, body;
    if (!ssl->method->init_message(ssl, &cbb, &body, SSL3_MT_KEY_UPDATE) ||
        !CBB_add_u8(&body, SSL_KEY_UPDATE_NOT_REQUESTED) ||
        !ssl_add_message_cbb(ssl, &cbb) ||
        !tls13_rotate_traffic_key(ssl, evp_aead_seal)) {
      CBB_cleanup(&cbb);
      return 0;
    }

    /* Suppress KeyUpdate acknowledgments until this change is written to the
     * wire. This prevents us from accumulating write obligations when read and
     * write progress at different rates. See draft-ietf-tls-tls13-18, section
     * 4.5.3. */
    ssl->s3->key_update_pending = 1;
  }

  return 1;
}

int tls13_post_handshake(SSL *ssl) {
  if (ssl->s3->tmp.message_type == SSL3_MT_KEY_UPDATE) {
    ssl->s3->key_update_count++;
    if (ssl->s3->key_update_count > kMaxKeyUpdates) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_TOO_MANY_KEY_UPDATES);
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
      return 0;
    }

    return tls13_receive_key_update(ssl);
  }

  ssl->s3->key_update_count = 0;

  if (ssl->s3->tmp.message_type == SSL3_MT_NEW_SESSION_TICKET &&
      !ssl->server) {
    return tls13_process_new_session_ticket(ssl);
  }

  ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
  OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_MESSAGE);
  return 0;
}
