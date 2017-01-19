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
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/rand.h>
#include <openssl/stack.h>

#include "internal.h"


enum server_hs_state_t {
  state_process_client_hello = 0,
  state_select_parameters,
  state_send_hello_retry_request,
  state_flush_hello_retry_request,
  state_process_second_client_hello,
  state_send_server_hello,
  state_send_encrypted_extensions,
  state_send_certificate_request,
  state_send_server_certificate,
  state_send_server_certificate_verify,
  state_complete_server_certificate_verify,
  state_send_server_finished,
  state_flush,
  state_process_client_certificate,
  state_process_client_certificate_verify,
  state_process_channel_id,
  state_process_client_finished,
  state_send_new_session_ticket,
  state_flush_new_session_ticket,
  state_done,
};

static const uint8_t kZeroes[EVP_MAX_MD_SIZE] = {0};

static int resolve_ecdhe_secret(SSL *ssl, int *out_need_retry,
                                struct ssl_early_callback_ctx *early_ctx) {
  *out_need_retry = 0;

  /* We only support connections that include an ECDHE key exchange. */
  CBS key_share;
  if (!ssl_early_callback_get_extension(early_ctx, &key_share,
                                        TLSEXT_TYPE_key_share)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_MISSING_KEY_SHARE);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_MISSING_EXTENSION);
    return ssl_hs_error;
  }

  int found_key_share;
  uint8_t *dhe_secret;
  size_t dhe_secret_len;
  uint8_t alert = SSL_AD_DECODE_ERROR;
  if (!ssl_ext_key_share_parse_clienthello(ssl, &found_key_share, &dhe_secret,
                                           &dhe_secret_len, &alert,
                                           &key_share)) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, alert);
    return 0;
  }

  if (!found_key_share) {
    *out_need_retry = 1;
    return 0;
  }

  int ok = tls13_advance_key_schedule(ssl, dhe_secret, dhe_secret_len);
  OPENSSL_free(dhe_secret);
  return ok;
}

static enum ssl_hs_wait_t do_process_client_hello(SSL *ssl, SSL_HANDSHAKE *hs) {
  if (!tls13_check_message_type(ssl, SSL3_MT_CLIENT_HELLO)) {
    return ssl_hs_error;
  }

  struct ssl_early_callback_ctx client_hello;
  if (!ssl_early_callback_init(ssl, &client_hello, ssl->init_msg,
                               ssl->init_num)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CLIENTHELLO_PARSE_FAILED);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return ssl_hs_error;
  }

  assert(ssl->s3->have_version);

  /* Load the client random. */
  if (client_hello.random_len != SSL3_RANDOM_SIZE) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return -1;
  }
  memcpy(ssl->s3->client_random, client_hello.random, client_hello.random_len);

  uint8_t alert = SSL_AD_DECODE_ERROR;
  SSL_SESSION *session = NULL;
  CBS pre_shared_key;
  if (ssl_early_callback_get_extension(&client_hello, &pre_shared_key,
                                       TLSEXT_TYPE_pre_shared_key) &&
      !ssl_ext_pre_shared_key_parse_clienthello(ssl, &session, &alert,
                                                &pre_shared_key)) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, alert);
    return 0;
  }

  if (session != NULL &&
      /* Only resume if the session's version matches. */
      (session->ssl_version != ssl->version ||
       !ssl_client_cipher_list_contains_cipher(
           &client_hello, (uint16_t)SSL_CIPHER_get_id(session->cipher)))) {
    SSL_SESSION_free(session);
    session = NULL;
  }

  if (session == NULL) {
    if (!ssl_get_new_session(ssl, 1 /* server */)) {
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      return ssl_hs_error;
    }
  } else {
    /* Only authentication information carries over in TLS 1.3. */
    ssl->s3->new_session = SSL_SESSION_dup(session, SSL_SESSION_DUP_AUTH_ONLY);
    if (ssl->s3->new_session == NULL) {
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      return ssl_hs_error;
    }
    ssl->s3->session_reused = 1;
    SSL_SESSION_free(session);
  }

  if (ssl->ctx->dos_protection_cb != NULL &&
      ssl->ctx->dos_protection_cb(&client_hello) == 0) {
    /* Connection rejected for DOS reasons. */
    OPENSSL_PUT_ERROR(SSL, SSL_R_CONNECTION_REJECTED);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
    return ssl_hs_error;
  }

  /* TLS 1.3 requires the peer only advertise the null compression. */
  if (client_hello.compression_methods_len != 1 ||
      client_hello.compression_methods[0] != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_COMPRESSION_LIST);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
    return ssl_hs_error;
  }

  /* TLS extensions. */
  if (!ssl_parse_clienthello_tlsext(ssl, &client_hello)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_PARSE_TLSEXT);
    return ssl_hs_error;
  }

  hs->state = state_select_parameters;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_select_parameters(SSL *ssl, SSL_HANDSHAKE *hs) {
  if (!ssl->s3->session_reused) {
    /* Call |cert_cb| to update server certificates if required. */
    if (ssl->cert->cert_cb != NULL) {
      int rv = ssl->cert->cert_cb(ssl, ssl->cert->cert_cb_arg);
      if (rv == 0) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_CERT_CB_ERROR);
        ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
        return ssl_hs_error;
      }
      if (rv < 0) {
        hs->state = state_select_parameters;
        return ssl_hs_x509_lookup;
      }
    }
  }

  struct ssl_early_callback_ctx client_hello;
  if (!ssl_early_callback_init(ssl, &client_hello, ssl->init_msg,
                               ssl->init_num)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CLIENTHELLO_PARSE_FAILED);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return ssl_hs_error;
  }

  if (!ssl->s3->session_reused) {
    const SSL_CIPHER *cipher =
        ssl3_choose_cipher(ssl, &client_hello, ssl_get_cipher_preferences(ssl));
    if (cipher == NULL) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_NO_SHARED_CIPHER);
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
      return ssl_hs_error;
    }

    ssl->s3->new_session->cipher = cipher;
  }

  ssl->s3->tmp.new_cipher = ssl->s3->new_session->cipher;
  ssl->method->received_flight(ssl);


  /* The PRF hash is now known. */
  size_t hash_len =
      EVP_MD_size(ssl_get_handshake_digest(ssl_get_algorithm_prf(ssl)));

  /* Derive resumption material. */
  uint8_t resumption_ctx[EVP_MAX_MD_SIZE] = {0};
  uint8_t psk_secret[EVP_MAX_MD_SIZE] = {0};
  if (ssl->s3->session_reused) {
    if (!tls13_resumption_context(ssl, resumption_ctx, hash_len,
                                  ssl->s3->new_session) ||
        !tls13_resumption_psk(ssl, psk_secret, hash_len,
                              ssl->s3->new_session)) {
      return ssl_hs_error;
    }
  }

  /* Set up the key schedule, hash in the ClientHello, and incorporate the PSK
   * into the running secret. */
  if (!tls13_init_key_schedule(ssl, resumption_ctx, hash_len) ||
      !tls13_advance_key_schedule(ssl, psk_secret, hash_len)) {
    return ssl_hs_error;
  }

  /* Resolve ECDHE and incorporate it into the secret. */
  int need_retry;
  if (!resolve_ecdhe_secret(ssl, &need_retry, &client_hello)) {
    if (need_retry) {
      hs->state = state_send_hello_retry_request;
      return ssl_hs_ok;
    }
    return ssl_hs_error;
  }

  hs->state = state_send_server_hello;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_hello_retry_request(SSL *ssl,
                                                      SSL_HANDSHAKE *hs) {
  CBB cbb, body, extensions;
  uint16_t group_id;
  if (!ssl->method->init_message(ssl, &cbb, &body,
                                 SSL3_MT_HELLO_RETRY_REQUEST) ||
      !CBB_add_u16(&body, ssl->version) ||
      !tls1_get_shared_group(ssl, &group_id) ||
      !CBB_add_u16_length_prefixed(&body, &extensions) ||
      !CBB_add_u16(&extensions, TLSEXT_TYPE_key_share) ||
      !CBB_add_u16(&extensions, 2 /* length */) ||
      !CBB_add_u16(&extensions, group_id) ||
      !ssl->method->finish_message(ssl, &cbb)) {
    CBB_cleanup(&cbb);
    return ssl_hs_error;
  }

  hs->state = state_flush_hello_retry_request;
  return ssl_hs_write_message;
}

static enum ssl_hs_wait_t do_flush_hello_retry_request(SSL *ssl,
                                                       SSL_HANDSHAKE *hs) {
  hs->state = state_process_second_client_hello;
  return ssl_hs_flush_and_read_message;
}

static enum ssl_hs_wait_t do_process_second_client_hello(SSL *ssl,
                                                      SSL_HANDSHAKE *hs) {
  if (!tls13_check_message_type(ssl, SSL3_MT_CLIENT_HELLO)) {
    return ssl_hs_error;
  }

  struct ssl_early_callback_ctx client_hello;
  if (!ssl_early_callback_init(ssl, &client_hello, ssl->init_msg,
                               ssl->init_num)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CLIENTHELLO_PARSE_FAILED);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return ssl_hs_error;
  }

  int need_retry;
  if (!resolve_ecdhe_secret(ssl, &need_retry, &client_hello)) {
    if (need_retry) {
      /* Only send one HelloRetryRequest. */
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
      OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_CURVE);
    }
    return ssl_hs_error;
  }

  if (!ssl->method->hash_current_message(ssl)) {
    return ssl_hs_error;
  }

  ssl->method->received_flight(ssl);
  hs->state = state_send_server_hello;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_server_hello(SSL *ssl, SSL_HANDSHAKE *hs) {
  CBB cbb, body, extensions;
  if (!ssl->method->init_message(ssl, &cbb, &body, SSL3_MT_SERVER_HELLO) ||
      !CBB_add_u16(&body, ssl->version) ||
      !RAND_bytes(ssl->s3->server_random, sizeof(ssl->s3->server_random)) ||
      !CBB_add_bytes(&body, ssl->s3->server_random, SSL3_RANDOM_SIZE) ||
      !CBB_add_u16(&body, ssl_cipher_get_value(ssl->s3->tmp.new_cipher)) ||
      !CBB_add_u16_length_prefixed(&body, &extensions) ||
      !ssl_ext_pre_shared_key_add_serverhello(ssl, &extensions) ||
      !ssl_ext_key_share_add_serverhello(ssl, &extensions)) {
    goto err;
  }

  if (!ssl->s3->session_reused) {
    if (!CBB_add_u16(&extensions, TLSEXT_TYPE_signature_algorithms) ||
        !CBB_add_u16(&extensions, 0)) {
      goto err;
    }
  }

  if (!ssl->method->finish_message(ssl, &cbb)) {
    goto err;
  }

  hs->state = state_send_encrypted_extensions;
  return ssl_hs_write_message;

err:
  CBB_cleanup(&cbb);
  return ssl_hs_error;
}

static enum ssl_hs_wait_t do_send_encrypted_extensions(SSL *ssl,
                                                       SSL_HANDSHAKE *hs) {
  if (!tls13_set_handshake_traffic(ssl)) {
    return ssl_hs_error;
  }

  CBB cbb, body;
  if (!ssl->method->init_message(ssl, &cbb, &body,
                                 SSL3_MT_ENCRYPTED_EXTENSIONS) ||
      !ssl_add_serverhello_tlsext(ssl, &body) ||
      !ssl->method->finish_message(ssl, &cbb)) {
    CBB_cleanup(&cbb);
    return ssl_hs_error;
  }

  hs->state = state_send_certificate_request;
  return ssl_hs_write_message;
}

static enum ssl_hs_wait_t do_send_certificate_request(SSL *ssl,
                                                      SSL_HANDSHAKE *hs) {
  /* Determine whether to request a client certificate. */
  ssl->s3->hs->cert_request = !!(ssl->verify_mode & SSL_VERIFY_PEER);
  /* CertificateRequest may only be sent in non-resumption handshakes. */
  if (ssl->s3->session_reused) {
    ssl->s3->hs->cert_request = 0;
  }

  if (!ssl->s3->hs->cert_request) {
    /* Skip this state. */
    hs->state = state_send_server_certificate;
    return ssl_hs_ok;
  }

  CBB cbb, body, sigalgs_cbb;
  if (!ssl->method->init_message(ssl, &cbb, &body,
                                 SSL3_MT_CERTIFICATE_REQUEST) ||
      !CBB_add_u8(&body, 0 /* no certificate_request_context. */)) {
    goto err;
  }

  const uint16_t *sigalgs;
  size_t num_sigalgs = tls12_get_verify_sigalgs(ssl, &sigalgs);
  if (!CBB_add_u16_length_prefixed(&body, &sigalgs_cbb)) {
    goto err;
  }

  for (size_t i = 0; i < num_sigalgs; i++) {
    if (!CBB_add_u16(&sigalgs_cbb, sigalgs[i])) {
      goto err;
    }
  }

  if (!ssl_add_client_CA_list(ssl, &body) ||
      !CBB_add_u16(&body, 0 /* empty certificate_extensions. */) ||
      !ssl->method->finish_message(ssl, &cbb)) {
    goto err;
  }

  hs->state = state_send_server_certificate;
  return ssl_hs_write_message;

err:
  CBB_cleanup(&cbb);
  return ssl_hs_error;
}

static enum ssl_hs_wait_t do_send_server_certificate(SSL *ssl,
                                                     SSL_HANDSHAKE *hs) {
  if (ssl->s3->session_reused) {
    hs->state = state_send_server_finished;
    return ssl_hs_ok;
  }

  if (!ssl_has_certificate(ssl)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_NO_CERTIFICATE_SET);
    return ssl_hs_error;
  }

  if (!tls13_prepare_certificate(ssl)) {
    return ssl_hs_error;
  }

  hs->state = state_send_server_certificate_verify;
  return ssl_hs_write_message;
}

static enum ssl_hs_wait_t do_send_server_certificate_verify(SSL *ssl,
                                                            SSL_HANDSHAKE *hs,
                                                            int is_first_run) {
  switch (tls13_prepare_certificate_verify(ssl, is_first_run)) {
    case ssl_private_key_success:
      hs->state = state_send_server_finished;
      return ssl_hs_write_message;

    case ssl_private_key_retry:
      hs->state = state_complete_server_certificate_verify;
      return ssl_hs_private_key_operation;

    case ssl_private_key_failure:
      return ssl_hs_error;
  }

  assert(0);
  return ssl_hs_error;
}

static enum ssl_hs_wait_t do_send_server_finished(SSL *ssl, SSL_HANDSHAKE *hs) {
  if (!tls13_prepare_finished(ssl)) {
    return ssl_hs_error;
  }

  hs->state = state_flush;
  return ssl_hs_write_message;
}

static enum ssl_hs_wait_t do_flush(SSL *ssl, SSL_HANDSHAKE *hs) {
  /* Update the secret to the master secret and derive traffic keys. */
  if (!tls13_advance_key_schedule(ssl, kZeroes, hs->hash_len) ||
      !tls13_derive_traffic_secret_0(ssl) ||
      !tls13_set_traffic_key(ssl, type_data, evp_aead_seal,
                             hs->server_traffic_secret_0, hs->hash_len)) {
    return ssl_hs_error;
  }

  hs->state = state_process_client_certificate;
  return ssl_hs_flush_and_read_message;
}

static enum ssl_hs_wait_t do_process_client_certificate(SSL *ssl,
                                                        SSL_HANDSHAKE *hs) {
  if (!ssl->s3->hs->cert_request) {
    /* OpenSSL returns X509_V_OK when no certificates are requested. This is
     * classed by them as a bug, but it's assumed by at least NGINX. */
    ssl->s3->new_session->verify_result = X509_V_OK;

    /* Skip this state. */
    hs->state = state_process_channel_id;
    return ssl_hs_ok;
  }

  const int allow_anonymous =
      (ssl->verify_mode & SSL_VERIFY_FAIL_IF_NO_PEER_CERT) == 0;

  if (!tls13_check_message_type(ssl, SSL3_MT_CERTIFICATE) ||
      !tls13_process_certificate(ssl, allow_anonymous) ||
      !ssl->method->hash_current_message(ssl)) {
    return ssl_hs_error;
  }

  /* For historical reasons, the server's copy of the chain does not include the
   * leaf while the client's does. */
  if (sk_X509_num(ssl->s3->new_session->cert_chain) > 0) {
    X509_free(sk_X509_shift(ssl->s3->new_session->cert_chain));
  }

  hs->state = state_process_client_certificate_verify;
  return ssl_hs_read_message;
}

static enum ssl_hs_wait_t do_process_client_certificate_verify(
    SSL *ssl, SSL_HANDSHAKE *hs) {
  if (ssl->s3->new_session->peer == NULL) {
    /* Skip this state. */
    hs->state = state_process_channel_id;
    return ssl_hs_ok;
  }

  if (!tls13_check_message_type(ssl, SSL3_MT_CERTIFICATE_VERIFY) ||
      !tls13_process_certificate_verify(ssl) ||
      !ssl->method->hash_current_message(ssl)) {
    return 0;
  }

  hs->state = state_process_channel_id;
  return ssl_hs_read_message;
}

static enum ssl_hs_wait_t do_process_channel_id(SSL *ssl, SSL_HANDSHAKE *hs) {
  if (!ssl->s3->tlsext_channel_id_valid) {
    hs->state = state_process_client_finished;
    return ssl_hs_ok;
  }

  if (!tls13_check_message_type(ssl, SSL3_MT_CHANNEL_ID) ||
      !tls1_verify_channel_id(ssl) ||
      !ssl->method->hash_current_message(ssl)) {
    return ssl_hs_error;
  }

  hs->state = state_process_client_finished;
  return ssl_hs_read_message;
}

static enum ssl_hs_wait_t do_process_client_finished(SSL *ssl,
                                                     SSL_HANDSHAKE *hs) {
  if (!tls13_check_message_type(ssl, SSL3_MT_FINISHED) ||
      !tls13_process_finished(ssl) ||
      !ssl->method->hash_current_message(ssl) ||
      /* evp_aead_seal keys have already been switched. */
      !tls13_set_traffic_key(ssl, type_data, evp_aead_open,
                             hs->client_traffic_secret_0, hs->hash_len) ||
      !tls13_finalize_keys(ssl)) {
    return ssl_hs_error;
  }

  ssl->method->received_flight(ssl);
  hs->state = state_send_new_session_ticket;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_new_session_ticket(SSL *ssl,
                                                     SSL_HANDSHAKE *hs) {
  SSL_SESSION *session = ssl->s3->new_session;
  session->tlsext_tick_lifetime_hint = session->timeout;

  /* TODO(svaldez): Add support for sending 0RTT through TicketEarlyDataInfo
   * extension. */

  CBB cbb, body, ke_modes, auth_modes, ticket, extensions;
  if (!ssl->method->init_message(ssl, &cbb, &body,
                                 SSL3_MT_NEW_SESSION_TICKET) ||
      !CBB_add_u32(&body, session->tlsext_tick_lifetime_hint) ||
      !CBB_add_u8_length_prefixed(&body, &ke_modes) ||
      !CBB_add_u8(&ke_modes, SSL_PSK_DHE_KE) ||
      !CBB_add_u8_length_prefixed(&body, &auth_modes) ||
      !CBB_add_u8(&auth_modes, SSL_PSK_AUTH) ||
      !CBB_add_u16_length_prefixed(&body, &ticket) ||
      !ssl_encrypt_ticket(ssl, &ticket, session) ||
      !CBB_add_u16_length_prefixed(&body, &extensions)) {
    goto err;
  }

  /* Add a fake extension. See draft-davidben-tls-grease-01. */
  if (ssl->ctx->grease_enabled) {
    if (!CBB_add_u16(&extensions,
                     ssl_get_grease_value(ssl, ssl_grease_ticket_extension)) ||
        !CBB_add_u16(&extensions, 0 /* empty */)) {
      goto err;
    }
  }

  if (!ssl->method->finish_message(ssl, &cbb)) {
    goto err;
  }

  hs->session_tickets_sent++;

  hs->state = state_flush_new_session_ticket;
  return ssl_hs_write_message;

err:
  CBB_cleanup(&cbb);
  return ssl_hs_error;
}

/* TLS 1.3 recommends single-use tickets, so issue multiple tickets in case the
 * client makes several connections before getting a renewal. */
static const int kNumTickets = 2;

static enum ssl_hs_wait_t do_flush_new_session_ticket(SSL *ssl,
                                                      SSL_HANDSHAKE *hs) {
  if (hs->session_tickets_sent >= kNumTickets) {
    hs->state = state_done;
  } else {
    hs->state = state_send_new_session_ticket;
  }
  return ssl_hs_flush;
}

enum ssl_hs_wait_t tls13_server_handshake(SSL *ssl) {
  SSL_HANDSHAKE *hs = ssl->s3->hs;

  while (hs->state != state_done) {
    enum ssl_hs_wait_t ret = ssl_hs_error;
    enum server_hs_state_t state = hs->state;
    switch (state) {
      case state_process_client_hello:
        ret = do_process_client_hello(ssl, hs);
        break;
      case state_select_parameters:
        ret = do_select_parameters(ssl, hs);
        break;
      case state_send_hello_retry_request:
        ret = do_send_hello_retry_request(ssl, hs);
        break;
      case state_flush_hello_retry_request:
        ret = do_flush_hello_retry_request(ssl, hs);
        break;
      case state_process_second_client_hello:
        ret = do_process_second_client_hello(ssl, hs);
        break;
      case state_send_server_hello:
        ret = do_send_server_hello(ssl, hs);
        break;
      case state_send_encrypted_extensions:
        ret = do_send_encrypted_extensions(ssl, hs);
        break;
      case state_send_certificate_request:
        ret = do_send_certificate_request(ssl, hs);
        break;
      case state_send_server_certificate:
        ret = do_send_server_certificate(ssl, hs);
        break;
      case state_send_server_certificate_verify:
        ret = do_send_server_certificate_verify(ssl, hs, 1 /* first run */);
      break;
      case state_complete_server_certificate_verify:
        ret = do_send_server_certificate_verify(ssl, hs, 0 /* complete */);
      break;
      case state_send_server_finished:
        ret = do_send_server_finished(ssl, hs);
        break;
      case state_flush:
        ret = do_flush(ssl, hs);
        break;
      case state_process_client_certificate:
        ret = do_process_client_certificate(ssl, hs);
        break;
      case state_process_client_certificate_verify:
        ret = do_process_client_certificate_verify(ssl, hs);
        break;
      case state_process_channel_id:
        ret = do_process_channel_id(ssl, hs);
        break;
      case state_process_client_finished:
        ret = do_process_client_finished(ssl, hs);
        break;
      case state_send_new_session_ticket:
        ret = do_send_new_session_ticket(ssl, hs);
        break;
      case state_flush_new_session_ticket:
        ret = do_flush_new_session_ticket(ssl, hs);
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
