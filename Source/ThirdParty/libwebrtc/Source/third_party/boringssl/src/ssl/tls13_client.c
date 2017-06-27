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
#include <limits.h>
#include <string.h>

#include <openssl/bytestring.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/stack.h>

#include "../crypto/internal.h"
#include "internal.h"


enum client_hs_state_t {
  state_process_hello_retry_request = 0,
  state_send_second_client_hello,
  state_process_server_hello,
  state_process_encrypted_extensions,
  state_process_certificate_request,
  state_process_server_certificate,
  state_process_server_certificate_verify,
  state_process_server_finished,
  state_send_end_of_early_data,
  state_send_client_certificate,
  state_send_client_certificate_verify,
  state_complete_client_certificate_verify,
  state_complete_second_flight,
  state_done,
};

static const uint8_t kZeroes[EVP_MAX_MD_SIZE] = {0};

static enum ssl_hs_wait_t do_process_hello_retry_request(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (ssl->s3->tmp.message_type != SSL3_MT_HELLO_RETRY_REQUEST) {
    hs->tls13_state = state_process_server_hello;
    return ssl_hs_ok;
  }

  CBS cbs, extensions;
  uint16_t server_wire_version;
  CBS_init(&cbs, ssl->init_msg, ssl->init_num);
  if (!CBS_get_u16(&cbs, &server_wire_version) ||
      !CBS_get_u16_length_prefixed(&cbs, &extensions) ||
      /* HelloRetryRequest may not be empty. */
      CBS_len(&extensions) == 0 ||
      CBS_len(&cbs) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return ssl_hs_error;
  }

  int have_cookie, have_key_share;
  CBS cookie, key_share;
  const SSL_EXTENSION_TYPE ext_types[] = {
      {TLSEXT_TYPE_key_share, &have_key_share, &key_share},
      {TLSEXT_TYPE_cookie, &have_cookie, &cookie},
  };

  uint8_t alert = SSL_AD_DECODE_ERROR;
  if (!ssl_parse_extensions(&extensions, &alert, ext_types,
                            OPENSSL_ARRAY_SIZE(ext_types),
                            0 /* reject unknown */)) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, alert);
    return ssl_hs_error;
  }

  if (have_cookie) {
    CBS cookie_value;
    if (!CBS_get_u16_length_prefixed(&cookie, &cookie_value) ||
        CBS_len(&cookie_value) == 0 ||
        CBS_len(&cookie) != 0) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
      return ssl_hs_error;
    }

    if (!CBS_stow(&cookie_value, &hs->cookie, &hs->cookie_len)) {
      return ssl_hs_error;
    }
  }

  if (have_key_share) {
    uint16_t group_id;
    if (!CBS_get_u16(&key_share, &group_id) || CBS_len(&key_share) != 0) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
      return ssl_hs_error;
    }

    /* The group must be supported. */
    const uint16_t *groups;
    size_t groups_len;
    tls1_get_grouplist(ssl, &groups, &groups_len);
    int found = 0;
    for (size_t i = 0; i < groups_len; i++) {
      if (groups[i] == group_id) {
        found = 1;
        break;
      }
    }

    if (!found) {
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
      OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_CURVE);
      return ssl_hs_error;
    }

    /* Check that the HelloRetryRequest does not request the key share that
     * was provided in the initial ClientHello. */
    if (SSL_ECDH_CTX_get_id(&hs->ecdh_ctx) == group_id) {
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
      OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_CURVE);
      return ssl_hs_error;
    }

    SSL_ECDH_CTX_cleanup(&hs->ecdh_ctx);
    hs->retry_group = group_id;
  }

  if (!ssl_hash_current_message(hs)) {
    return ssl_hs_error;
  }

  hs->received_hello_retry_request = 1;
  hs->tls13_state = state_send_second_client_hello;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_second_client_hello(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  /* TODO(svaldez): Ensure that we set can_early_write to false since 0-RTT is
   * rejected if we receive a HelloRetryRequest. */
  if (!ssl->method->set_write_state(ssl, NULL) ||
      !ssl_write_client_hello(hs)) {
    return ssl_hs_error;
  }

  hs->tls13_state = state_process_server_hello;
  return ssl_hs_flush_and_read_message;
}

static enum ssl_hs_wait_t do_process_server_hello(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (!ssl_check_message_type(ssl, SSL3_MT_SERVER_HELLO)) {
    return ssl_hs_error;
  }

  CBS cbs, server_random, extensions;
  uint16_t server_wire_version;
  uint16_t cipher_suite;
  CBS_init(&cbs, ssl->init_msg, ssl->init_num);
  if (!CBS_get_u16(&cbs, &server_wire_version) ||
      !CBS_get_bytes(&cbs, &server_random, SSL3_RANDOM_SIZE) ||
      !CBS_get_u16(&cbs, &cipher_suite) ||
      !CBS_get_u16_length_prefixed(&cbs, &extensions) ||
      CBS_len(&cbs) != 0) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return ssl_hs_error;
  }

  if (server_wire_version != ssl->version) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_VERSION_NUMBER);
    return ssl_hs_error;
  }

  assert(ssl->s3->have_version);
  OPENSSL_memcpy(ssl->s3->server_random, CBS_data(&server_random),
                 SSL3_RANDOM_SIZE);

  const SSL_CIPHER *cipher = SSL_get_cipher_by_value(cipher_suite);
  if (cipher == NULL) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNKNOWN_CIPHER_RETURNED);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
    return ssl_hs_error;
  }

  /* Check if the cipher is a TLS 1.3 cipher. */
  if (SSL_CIPHER_get_min_version(cipher) > ssl3_protocol_version(ssl) ||
      SSL_CIPHER_get_max_version(cipher) < ssl3_protocol_version(ssl)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_CIPHER_RETURNED);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
    return ssl_hs_error;
  }

  /* Parse out the extensions. */
  int have_key_share = 0, have_pre_shared_key = 0;
  CBS key_share, pre_shared_key;
  const SSL_EXTENSION_TYPE ext_types[] = {
      {TLSEXT_TYPE_key_share, &have_key_share, &key_share},
      {TLSEXT_TYPE_pre_shared_key, &have_pre_shared_key, &pre_shared_key},
  };

  uint8_t alert = SSL_AD_DECODE_ERROR;
  if (!ssl_parse_extensions(&extensions, &alert, ext_types,
                            OPENSSL_ARRAY_SIZE(ext_types),
                            0 /* reject unknown */)) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, alert);
    return ssl_hs_error;
  }

  alert = SSL_AD_DECODE_ERROR;
  if (have_pre_shared_key) {
    if (ssl->session == NULL) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_EXTENSION);
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNSUPPORTED_EXTENSION);
      return ssl_hs_error;
    }

    if (!ssl_ext_pre_shared_key_parse_serverhello(hs, &alert,
                                                  &pre_shared_key)) {
      ssl3_send_alert(ssl, SSL3_AL_FATAL, alert);
      return ssl_hs_error;
    }

    if (ssl->session->ssl_version != ssl->version) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_OLD_SESSION_VERSION_NOT_RETURNED);
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
      return ssl_hs_error;
    }

    if (ssl->session->cipher->algorithm_prf != cipher->algorithm_prf) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_OLD_SESSION_PRF_HASH_MISMATCH);
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
      return ssl_hs_error;
    }

    if (!ssl_session_is_context_valid(ssl, ssl->session)) {
      /* This is actually a client application bug. */
      OPENSSL_PUT_ERROR(SSL,
                        SSL_R_ATTEMPT_TO_REUSE_SESSION_IN_DIFFERENT_CONTEXT);
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
      return ssl_hs_error;
    }

    ssl->s3->session_reused = 1;
    /* Only authentication information carries over in TLS 1.3. */
    hs->new_session = SSL_SESSION_dup(ssl->session, SSL_SESSION_DUP_AUTH_ONLY);
    if (hs->new_session == NULL) {
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      return ssl_hs_error;
    }

    /* Resumption incorporates fresh key material, so refresh the timeout. */
    ssl_session_renew_timeout(ssl, hs->new_session,
                              ssl->session_ctx->session_psk_dhe_timeout);
  } else if (!ssl_get_new_session(hs, 0)) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
    return ssl_hs_error;
  }

  hs->new_session->cipher = cipher;
  hs->new_cipher = cipher;

  /* The PRF hash is now known. Set up the key schedule. */
  if (!tls13_init_key_schedule(hs)) {
    return ssl_hs_error;
  }

  /* Incorporate the PSK into the running secret. */
  if (ssl->s3->session_reused) {
    if (!tls13_advance_key_schedule(hs, hs->new_session->master_key,
                                    hs->new_session->master_key_length)) {
      return ssl_hs_error;
    }
  } else if (!tls13_advance_key_schedule(hs, kZeroes, hs->hash_len)) {
    return ssl_hs_error;
  }

  if (!have_key_share) {
    /* We do not support psk_ke and thus always require a key share. */
    OPENSSL_PUT_ERROR(SSL, SSL_R_MISSING_KEY_SHARE);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_MISSING_EXTENSION);
    return ssl_hs_error;
  }

  /* Resolve ECDHE and incorporate it into the secret. */
  uint8_t *dhe_secret;
  size_t dhe_secret_len;
  alert = SSL_AD_DECODE_ERROR;
  if (!ssl_ext_key_share_parse_serverhello(hs, &dhe_secret, &dhe_secret_len,
                                           &alert, &key_share)) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, alert);
    return ssl_hs_error;
  }

  if (!tls13_advance_key_schedule(hs, dhe_secret, dhe_secret_len)) {
    OPENSSL_free(dhe_secret);
    return ssl_hs_error;
  }
  OPENSSL_free(dhe_secret);

  if (!ssl_hash_current_message(hs) ||
      !tls13_derive_handshake_secrets(hs) ||
      !tls13_set_traffic_key(ssl, evp_aead_open, hs->server_handshake_secret,
                             hs->hash_len)) {
    return ssl_hs_error;
  }

  /* If not sending early data, set client traffic keys now so that alerts are
   * encrypted. */
  if (!hs->early_data_offered &&
      !tls13_set_traffic_key(ssl, evp_aead_seal, hs->client_handshake_secret,
                             hs->hash_len)) {
    return ssl_hs_error;
  }

  hs->tls13_state = state_process_encrypted_extensions;
  return ssl_hs_read_message;
}

static enum ssl_hs_wait_t do_process_encrypted_extensions(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (!ssl_check_message_type(ssl, SSL3_MT_ENCRYPTED_EXTENSIONS)) {
    return ssl_hs_error;
  }

  CBS cbs;
  CBS_init(&cbs, ssl->init_msg, ssl->init_num);
  if (!ssl_parse_serverhello_tlsext(hs, &cbs)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_PARSE_TLSEXT);
    return ssl_hs_error;
  }
  if (CBS_len(&cbs) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return ssl_hs_error;
  }

  /* Store the negotiated ALPN in the session. */
  if (ssl->s3->alpn_selected != NULL) {
    hs->new_session->early_alpn =
        BUF_memdup(ssl->s3->alpn_selected, ssl->s3->alpn_selected_len);
    if (hs->new_session->early_alpn == NULL) {
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      return ssl_hs_error;
    }
    hs->new_session->early_alpn_len = ssl->s3->alpn_selected_len;
  }

  if (ssl->early_data_accepted) {
    if (ssl->session->cipher != hs->new_session->cipher ||
        ssl->session->early_alpn_len != ssl->s3->alpn_selected_len ||
        OPENSSL_memcmp(ssl->session->early_alpn, ssl->s3->alpn_selected,
                       ssl->s3->alpn_selected_len) != 0) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_ALPN_MISMATCH_ON_EARLY_DATA);
      return ssl_hs_error;
    }
    if (ssl->s3->tlsext_channel_id_valid) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_CHANNEL_ID_ON_EARLY_DATA);
      return ssl_hs_error;
    }
  }

  /* Release offered session now that it is no longer needed. */
  if (ssl->s3->session_reused) {
    ssl_set_session(ssl, NULL);
  }

  if (!ssl_hash_current_message(hs)) {
    return ssl_hs_error;
  }

  hs->tls13_state = state_process_certificate_request;
  return ssl_hs_read_message;
}

static enum ssl_hs_wait_t do_process_certificate_request(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  /* CertificateRequest may only be sent in non-resumption handshakes. */
  if (ssl->s3->session_reused) {
    hs->tls13_state = state_process_server_finished;
    return ssl_hs_ok;
  }

  /* CertificateRequest is optional. */
  if (ssl->s3->tmp.message_type != SSL3_MT_CERTIFICATE_REQUEST) {
    hs->tls13_state = state_process_server_certificate;
    return ssl_hs_ok;
  }

  CBS cbs, context, supported_signature_algorithms;
  CBS_init(&cbs, ssl->init_msg, ssl->init_num);
  if (!CBS_get_u8_length_prefixed(&cbs, &context) ||
      /* The request context is always empty during the handshake. */
      CBS_len(&context) != 0 ||
      !CBS_get_u16_length_prefixed(&cbs, &supported_signature_algorithms) ||
      CBS_len(&supported_signature_algorithms) == 0 ||
      !tls1_parse_peer_sigalgs(hs, &supported_signature_algorithms)) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return ssl_hs_error;
  }

  uint8_t alert = SSL_AD_DECODE_ERROR;
  STACK_OF(CRYPTO_BUFFER) *ca_names =
      ssl_parse_client_CA_list(ssl, &alert, &cbs);
  if (ca_names == NULL) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, alert);
    return ssl_hs_error;
  }

  /* Ignore extensions. */
  CBS extensions;
  if (!CBS_get_u16_length_prefixed(&cbs, &extensions) ||
      CBS_len(&cbs) != 0) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    sk_CRYPTO_BUFFER_pop_free(ca_names, CRYPTO_BUFFER_free);
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return ssl_hs_error;
  }

  hs->cert_request = 1;
  sk_CRYPTO_BUFFER_pop_free(hs->ca_names, CRYPTO_BUFFER_free);
  hs->ca_names = ca_names;
  ssl->ctx->x509_method->hs_flush_cached_ca_names(hs);

  if (!ssl_hash_current_message(hs)) {
    return ssl_hs_error;
  }

  hs->tls13_state = state_process_server_certificate;
  return ssl_hs_read_message;
}

static enum ssl_hs_wait_t do_process_server_certificate(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (!ssl_check_message_type(ssl, SSL3_MT_CERTIFICATE) ||
      !tls13_process_certificate(hs, 0 /* certificate required */) ||
      !ssl_hash_current_message(hs)) {
    return ssl_hs_error;
  }

  hs->tls13_state = state_process_server_certificate_verify;
  return ssl_hs_read_message;
}

static enum ssl_hs_wait_t do_process_server_certificate_verify(
    SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (!ssl_check_message_type(ssl, SSL3_MT_CERTIFICATE_VERIFY) ||
      !tls13_process_certificate_verify(hs) ||
      !ssl_hash_current_message(hs)) {
    return ssl_hs_error;
  }

  hs->tls13_state = state_process_server_finished;
  return ssl_hs_read_message;
}

static enum ssl_hs_wait_t do_process_server_finished(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (!ssl_check_message_type(ssl, SSL3_MT_FINISHED) ||
      !tls13_process_finished(hs, 0 /* don't use saved value */) ||
      !ssl_hash_current_message(hs) ||
      /* Update the secret to the master secret and derive traffic keys. */
      !tls13_advance_key_schedule(hs, kZeroes, hs->hash_len) ||
      !tls13_derive_application_secrets(hs)) {
    return ssl_hs_error;
  }

  ssl->method->received_flight(ssl);
  hs->tls13_state = state_send_end_of_early_data;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_end_of_early_data(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  /* TODO(svaldez): Stop sending early data. */
  if (ssl->early_data_accepted &&
      !ssl->method->add_alert(ssl, SSL3_AL_WARNING,
                              TLS1_AD_END_OF_EARLY_DATA)) {
    return ssl_hs_error;
  }

  if (hs->early_data_offered &&
      !tls13_set_traffic_key(ssl, evp_aead_seal, hs->client_handshake_secret,
                             hs->hash_len)) {
    return ssl_hs_error;
  }

  hs->tls13_state = state_send_client_certificate;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_client_certificate(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;

  /* The peer didn't request a certificate. */
  if (!hs->cert_request) {
    hs->tls13_state = state_complete_second_flight;
    return ssl_hs_ok;
  }

  /* Call cert_cb to update the certificate. */
  if (ssl->cert->cert_cb != NULL) {
    int rv = ssl->cert->cert_cb(ssl, ssl->cert->cert_cb_arg);
    if (rv == 0) {
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      OPENSSL_PUT_ERROR(SSL, SSL_R_CERT_CB_ERROR);
      return ssl_hs_error;
    }
    if (rv < 0) {
      hs->tls13_state = state_send_client_certificate;
      return ssl_hs_x509_lookup;
    }
  }

  if (!ssl_on_certificate_selected(hs) ||
      !tls13_add_certificate(hs)) {
    return ssl_hs_error;
  }

  hs->tls13_state = state_send_client_certificate_verify;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_client_certificate_verify(SSL_HANDSHAKE *hs,
                                                            int is_first_run) {
  SSL *const ssl = hs->ssl;
  /* Don't send CertificateVerify if there is no certificate. */
  if (!ssl_has_certificate(ssl)) {
    hs->tls13_state = state_complete_second_flight;
    return ssl_hs_ok;
  }

  switch (tls13_add_certificate_verify(hs, is_first_run)) {
    case ssl_private_key_success:
      hs->tls13_state = state_complete_second_flight;
      return ssl_hs_ok;

    case ssl_private_key_retry:
      hs->tls13_state = state_complete_client_certificate_verify;
      return ssl_hs_private_key_operation;

    case ssl_private_key_failure:
      return ssl_hs_error;
  }

  assert(0);
  return ssl_hs_error;
}

static enum ssl_hs_wait_t do_complete_second_flight(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;

  /* Send a Channel ID assertion if necessary. */
  if (ssl->s3->tlsext_channel_id_valid) {
    if (!ssl_do_channel_id_callback(ssl)) {
      hs->tls13_state = state_complete_second_flight;
      return ssl_hs_error;
    }

    if (ssl->tlsext_channel_id_private == NULL) {
      return ssl_hs_channel_id_lookup;
    }

    CBB cbb, body;
    if (!ssl->method->init_message(ssl, &cbb, &body, SSL3_MT_CHANNEL_ID) ||
        !tls1_write_channel_id(hs, &body) ||
        !ssl_add_message_cbb(ssl, &cbb)) {
      CBB_cleanup(&cbb);
      return ssl_hs_error;
    }
  }

  /* Send a Finished message. */
  if (!tls13_add_finished(hs)) {
    return ssl_hs_error;
  }

  /* Derive the final keys and enable them. */
  if (!tls13_set_traffic_key(ssl, evp_aead_open, hs->server_traffic_secret_0,
                             hs->hash_len) ||
      !tls13_set_traffic_key(ssl, evp_aead_seal, hs->client_traffic_secret_0,
                             hs->hash_len) ||
      !tls13_derive_resumption_secret(hs)) {
    return ssl_hs_error;
  }

  hs->tls13_state = state_done;
  return ssl_hs_flush;
}

enum ssl_hs_wait_t tls13_client_handshake(SSL_HANDSHAKE *hs) {
  while (hs->tls13_state != state_done) {
    enum ssl_hs_wait_t ret = ssl_hs_error;
    enum client_hs_state_t state = hs->tls13_state;
    switch (state) {
      case state_process_hello_retry_request:
        ret = do_process_hello_retry_request(hs);
        break;
      case state_send_second_client_hello:
        ret = do_send_second_client_hello(hs);
        break;
      case state_process_server_hello:
        ret = do_process_server_hello(hs);
        break;
      case state_process_encrypted_extensions:
        ret = do_process_encrypted_extensions(hs);
        break;
      case state_process_certificate_request:
        ret = do_process_certificate_request(hs);
        break;
      case state_process_server_certificate:
        ret = do_process_server_certificate(hs);
        break;
      case state_process_server_certificate_verify:
        ret = do_process_server_certificate_verify(hs);
        break;
      case state_process_server_finished:
        ret = do_process_server_finished(hs);
        break;
      case state_send_end_of_early_data:
        ret = do_send_end_of_early_data(hs);
        break;
      case state_send_client_certificate:
        ret = do_send_client_certificate(hs);
        break;
      case state_send_client_certificate_verify:
        ret = do_send_client_certificate_verify(hs, 1 /* first run */);
        break;
      case state_complete_client_certificate_verify:
        ret = do_send_client_certificate_verify(hs, 0 /* complete */);
        break;
      case state_complete_second_flight:
        ret = do_complete_second_flight(hs);
        break;
      case state_done:
        ret = ssl_hs_ok;
        break;
    }

    if (ret != ssl_hs_ok) {
      return ret;
    }
  }

  return ssl_hs_ok;
}

int tls13_process_new_session_ticket(SSL *ssl) {
  int ret = 0;
  SSL_SESSION *session = SSL_SESSION_dup(ssl->s3->established_session,
                                         SSL_SESSION_INCLUDE_NONAUTH);
  if (session == NULL) {
    return 0;
  }

  ssl_session_rebase_time(ssl, session);

  uint32_t server_timeout;
  CBS cbs, ticket, extensions;
  CBS_init(&cbs, ssl->init_msg, ssl->init_num);
  if (!CBS_get_u32(&cbs, &server_timeout) ||
      !CBS_get_u32(&cbs, &session->ticket_age_add) ||
      !CBS_get_u16_length_prefixed(&cbs, &ticket) ||
      !CBS_stow(&ticket, &session->tlsext_tick, &session->tlsext_ticklen) ||
      !CBS_get_u16_length_prefixed(&cbs, &extensions) ||
      CBS_len(&cbs) != 0) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    goto err;
  }

  /* Cap the renewable lifetime by the server advertised value. This avoids
   * wasting bandwidth on 0-RTT when we know the server will reject it. */
  if (session->timeout > server_timeout) {
    session->timeout = server_timeout;
  }

  /* Parse out the extensions. */
  int have_early_data_info = 0;
  CBS early_data_info;
  const SSL_EXTENSION_TYPE ext_types[] = {
      {TLSEXT_TYPE_ticket_early_data_info, &have_early_data_info,
       &early_data_info},
  };

  uint8_t alert = SSL_AD_DECODE_ERROR;
  if (!ssl_parse_extensions(&extensions, &alert, ext_types,
                            OPENSSL_ARRAY_SIZE(ext_types),
                            1 /* ignore unknown */)) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, alert);
    goto err;
  }

  if (have_early_data_info && ssl->cert->enable_early_data) {
    if (!CBS_get_u32(&early_data_info, &session->ticket_max_early_data) ||
        CBS_len(&early_data_info) != 0) {
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      goto err;
    }
  }

  session->ticket_age_add_valid = 1;
  session->not_resumable = 0;

  if (ssl->ctx->new_session_cb != NULL &&
      ssl->ctx->new_session_cb(ssl, session)) {
    /* |new_session_cb|'s return value signals that it took ownership. */
    session = NULL;
  }

  ret = 1;

err:
  SSL_SESSION_free(session);
  return ret;
}

void ssl_clear_tls13_state(SSL_HANDSHAKE *hs) {
  SSL_ECDH_CTX_cleanup(&hs->ecdh_ctx);

  OPENSSL_free(hs->key_share_bytes);
  hs->key_share_bytes = NULL;
  hs->key_share_bytes_len = 0;
}
