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
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/rand.h>
#include <openssl/stack.h>

#include "../crypto/internal.h"
#include "internal.h"


enum server_hs_state_t {
  state_select_parameters = 0,
  state_select_session,
  state_send_hello_retry_request,
  state_process_second_client_hello,
  state_send_server_hello,
  state_send_server_certificate_verify,
  state_complete_server_certificate_verify,
  state_send_server_finished,
  state_read_second_client_flight,
  state_process_end_of_early_data,
  state_process_client_certificate,
  state_process_client_certificate_verify,
  state_process_channel_id,
  state_process_client_finished,
  state_send_new_session_ticket,
  state_done,
};

static const uint8_t kZeroes[EVP_MAX_MD_SIZE] = {0};

static int resolve_ecdhe_secret(SSL_HANDSHAKE *hs, int *out_need_retry,
                                SSL_CLIENT_HELLO *client_hello) {
  SSL *const ssl = hs->ssl;
  *out_need_retry = 0;

  /* We only support connections that include an ECDHE key exchange. */
  CBS key_share;
  if (!ssl_client_hello_get_extension(client_hello, &key_share,
                                      TLSEXT_TYPE_key_share)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_MISSING_KEY_SHARE);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_MISSING_EXTENSION);
    return 0;
  }

  int found_key_share;
  uint8_t *dhe_secret;
  size_t dhe_secret_len;
  uint8_t alert = SSL_AD_DECODE_ERROR;
  if (!ssl_ext_key_share_parse_clienthello(hs, &found_key_share, &dhe_secret,
                                           &dhe_secret_len, &alert,
                                           &key_share)) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, alert);
    return 0;
  }

  if (!found_key_share) {
    *out_need_retry = 1;
    return 0;
  }

  int ok = tls13_advance_key_schedule(hs, dhe_secret, dhe_secret_len);
  OPENSSL_free(dhe_secret);
  return ok;
}

static const SSL_CIPHER *choose_tls13_cipher(
    const SSL *ssl, const SSL_CLIENT_HELLO *client_hello) {
  if (client_hello->cipher_suites_len % 2 != 0) {
    return NULL;
  }

  CBS cipher_suites;
  CBS_init(&cipher_suites, client_hello->cipher_suites,
           client_hello->cipher_suites_len);

  const int aes_is_fine = EVP_has_aes_hardware();
  const uint16_t version = ssl3_protocol_version(ssl);

  const SSL_CIPHER *best = NULL;
  while (CBS_len(&cipher_suites) > 0) {
    uint16_t cipher_suite;
    if (!CBS_get_u16(&cipher_suites, &cipher_suite)) {
      return NULL;
    }

    /* Limit to TLS 1.3 ciphers we know about. */
    const SSL_CIPHER *candidate = SSL_get_cipher_by_value(cipher_suite);
    if (candidate == NULL ||
        SSL_CIPHER_get_min_version(candidate) > version ||
        SSL_CIPHER_get_max_version(candidate) < version) {
      continue;
    }

    /* TLS 1.3 removes legacy ciphers, so honor the client order, but prefer
     * ChaCha20 if we do not have AES hardware. */
    if (aes_is_fine) {
      return candidate;
    }

    if (candidate->algorithm_enc == SSL_CHACHA20POLY1305) {
      return candidate;
    }

    if (best == NULL) {
      best = candidate;
    }
  }

  return best;
}

static int add_new_session_tickets(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  /* TLS 1.3 recommends single-use tickets, so issue multiple tickets in case
   * the client makes several connections before getting a renewal. */
  static const int kNumTickets = 2;

  SSL_SESSION *session = hs->new_session;
  CBB cbb;
  CBB_zero(&cbb);

  /* Rebase the session timestamp so that it is measured from ticket
   * issuance. */
  ssl_session_rebase_time(ssl, session);

  for (int i = 0; i < kNumTickets; i++) {
    if (!RAND_bytes((uint8_t *)&session->ticket_age_add, 4)) {
      goto err;
    }
    session->ticket_age_add_valid = 1;

    CBB body, ticket, extensions;
    if (!ssl->method->init_message(ssl, &cbb, &body,
                                   SSL3_MT_NEW_SESSION_TICKET) ||
        !CBB_add_u32(&body, session->timeout) ||
        !CBB_add_u32(&body, session->ticket_age_add) ||
        !CBB_add_u16_length_prefixed(&body, &ticket) ||
        !ssl_encrypt_ticket(ssl, &ticket, session) ||
        !CBB_add_u16_length_prefixed(&body, &extensions)) {
      goto err;
    }

    if (ssl->cert->enable_early_data) {
      session->ticket_max_early_data = kMaxEarlyDataAccepted;

      CBB early_data_info;
      if (!CBB_add_u16(&extensions, TLSEXT_TYPE_ticket_early_data_info) ||
          !CBB_add_u16_length_prefixed(&extensions, &early_data_info) ||
          !CBB_add_u32(&early_data_info, session->ticket_max_early_data) ||
          !CBB_flush(&extensions)) {
        goto err;
      }
    }

    /* Add a fake extension. See draft-davidben-tls-grease-01. */
    if (!CBB_add_u16(&extensions,
                     ssl_get_grease_value(ssl, ssl_grease_ticket_extension)) ||
        !CBB_add_u16(&extensions, 0 /* empty */)) {
      goto err;
    }

    if (!ssl_add_message_cbb(ssl, &cbb)) {
      goto err;
    }
  }

  return 1;

err:
  CBB_cleanup(&cbb);
  return 0;
}

static enum ssl_hs_wait_t do_select_parameters(SSL_HANDSHAKE *hs) {
  /* At this point, most ClientHello extensions have already been processed by
   * the common handshake logic. Resolve the remaining non-PSK parameters. */
  SSL *const ssl = hs->ssl;

  SSL_CLIENT_HELLO client_hello;
  if (!ssl_client_hello_init(ssl, &client_hello, ssl->init_msg,
                             ssl->init_num)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CLIENTHELLO_PARSE_FAILED);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return ssl_hs_error;
  }

  /* Negotiate the cipher suite. */
  hs->new_cipher = choose_tls13_cipher(ssl, &client_hello);
  if (hs->new_cipher == NULL) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_NO_SHARED_CIPHER);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
    return ssl_hs_error;
  }

  /* HTTP/2 negotiation depends on the cipher suite, so ALPN negotiation was
   * deferred. Complete it now. */
  uint8_t alert = SSL_AD_DECODE_ERROR;
  if (!ssl_negotiate_alpn(hs, &alert, &client_hello)) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, alert);
    return ssl_hs_error;
  }

  /* The PRF hash is now known. Set up the key schedule and hash the
   * ClientHello. */
  if (!tls13_init_key_schedule(hs) ||
      !ssl_hash_current_message(hs)) {
    return ssl_hs_error;
  }

  hs->tls13_state = state_select_session;
  return ssl_hs_ok;
}

static enum ssl_ticket_aead_result_t select_session(
    SSL_HANDSHAKE *hs, uint8_t *out_alert, SSL_SESSION **out_session,
    int32_t *out_ticket_age_skew, const SSL_CLIENT_HELLO *client_hello) {
  SSL *const ssl = hs->ssl;
  *out_session = NULL;

  /* Decode the ticket if we agreed on a PSK key exchange mode. */
  CBS pre_shared_key;
  if (!hs->accept_psk_mode ||
      !ssl_client_hello_get_extension(client_hello, &pre_shared_key,
                                      TLSEXT_TYPE_pre_shared_key)) {
    return ssl_ticket_aead_ignore_ticket;
  }

  /* Verify that the pre_shared_key extension is the last extension in
   * ClientHello. */
  if (CBS_data(&pre_shared_key) + CBS_len(&pre_shared_key) !=
      client_hello->extensions + client_hello->extensions_len) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_PRE_SHARED_KEY_MUST_BE_LAST);
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return ssl_ticket_aead_error;
  }

  CBS ticket, binders;
  uint32_t client_ticket_age;
  if (!ssl_ext_pre_shared_key_parse_clienthello(hs, &ticket, &binders,
                                                &client_ticket_age, out_alert,
                                                &pre_shared_key)) {
    return ssl_ticket_aead_error;
  }

  /* TLS 1.3 session tickets are renewed separately as part of the
   * NewSessionTicket. */
  int unused_renew;
  SSL_SESSION *session = NULL;
  enum ssl_ticket_aead_result_t ret =
      ssl_process_ticket(ssl, &session, &unused_renew, CBS_data(&ticket),
                         CBS_len(&ticket), NULL, 0);
  switch (ret) {
    case ssl_ticket_aead_success:
      break;
    case ssl_ticket_aead_error:
      *out_alert = SSL_AD_INTERNAL_ERROR;
      return ret;
    default:
      return ret;
  }

  if (!ssl_session_is_resumable(hs, session) ||
      /* Historically, some TLS 1.3 tickets were missing ticket_age_add. */
      !session->ticket_age_add_valid) {
    SSL_SESSION_free(session);
    return ssl_ticket_aead_ignore_ticket;
  }

  /* Recover the client ticket age and convert to seconds. */
  client_ticket_age -= session->ticket_age_add;
  client_ticket_age /= 1000;

  struct OPENSSL_timeval now;
  ssl_get_current_time(ssl, &now);

  /* Compute the server ticket age in seconds. */
  assert(now.tv_sec >= session->time);
  uint64_t server_ticket_age = now.tv_sec - session->time;

  /* To avoid overflowing |hs->ticket_age_skew|, we will not resume
   * 68-year-old sessions. */
  if (server_ticket_age > INT32_MAX) {
    SSL_SESSION_free(session);
    return ssl_ticket_aead_ignore_ticket;
  }

  /* TODO(davidben,svaldez): Measure this value to decide on tolerance. For
   * now, accept all values. https://crbug.com/boringssl/113. */
  *out_ticket_age_skew =
      (int32_t)client_ticket_age - (int32_t)server_ticket_age;

  /* Check the PSK binder. */
  if (!tls13_verify_psk_binder(hs, session, &binders)) {
    SSL_SESSION_free(session);
    *out_alert = SSL_AD_DECRYPT_ERROR;
    return ssl_ticket_aead_error;
  }

  *out_session = session;
  return ssl_ticket_aead_success;
}

static enum ssl_hs_wait_t do_select_session(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  SSL_CLIENT_HELLO client_hello;
  if (!ssl_client_hello_init(ssl, &client_hello, ssl->init_msg,
                             ssl->init_num)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CLIENTHELLO_PARSE_FAILED);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return ssl_hs_error;
  }

  uint8_t alert = SSL_AD_DECODE_ERROR;
  SSL_SESSION *session = NULL;
  switch (select_session(hs, &alert, &session, &ssl->s3->ticket_age_skew,
                         &client_hello)) {
    case ssl_ticket_aead_ignore_ticket:
      assert(session == NULL);
      if (!ssl_get_new_session(hs, 1 /* server */)) {
        ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
        return ssl_hs_error;
      }
      break;

    case ssl_ticket_aead_success:
      /* Carry over authentication information from the previous handshake into
       * a fresh session. */
      hs->new_session = SSL_SESSION_dup(session, SSL_SESSION_DUP_AUTH_ONLY);

      if (/* Early data must be acceptable for this ticket. */
          ssl->cert->enable_early_data &&
          session->ticket_max_early_data != 0 &&
          /* The client must have offered early data. */
          hs->early_data_offered &&
          /* Channel ID is incompatible with 0-RTT. */
          !ssl->s3->tlsext_channel_id_valid &&
          /* The negotiated ALPN must match the one in the ticket. */
          ssl->s3->alpn_selected_len == session->early_alpn_len &&
          OPENSSL_memcmp(ssl->s3->alpn_selected, session->early_alpn,
                         ssl->s3->alpn_selected_len) == 0) {
        ssl->early_data_accepted = 1;
      }

      SSL_SESSION_free(session);
      if (hs->new_session == NULL) {
        ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
        return ssl_hs_error;
      }

      ssl->s3->session_reused = 1;

      /* Resumption incorporates fresh key material, so refresh the timeout. */
      ssl_session_renew_timeout(ssl, hs->new_session,
                                ssl->session_ctx->session_psk_dhe_timeout);
      break;

    case ssl_ticket_aead_error:
      ssl3_send_alert(ssl, SSL3_AL_FATAL, alert);
      return ssl_hs_error;

    case ssl_ticket_aead_retry:
      hs->tls13_state = state_select_session;
      return ssl_hs_pending_ticket;
  }

  /* Record connection properties in the new session. */
  hs->new_session->cipher = hs->new_cipher;

  if (hs->hostname != NULL) {
    OPENSSL_free(hs->new_session->tlsext_hostname);
    hs->new_session->tlsext_hostname = BUF_strdup(hs->hostname);
    if (hs->new_session->tlsext_hostname == NULL) {
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      return ssl_hs_error;
    }
  }

  /* Store the initial negotiated ALPN in the session. */
  if (ssl->s3->alpn_selected != NULL) {
    hs->new_session->early_alpn =
        BUF_memdup(ssl->s3->alpn_selected, ssl->s3->alpn_selected_len);
    if (hs->new_session->early_alpn == NULL) {
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      return ssl_hs_error;
    }
    hs->new_session->early_alpn_len = ssl->s3->alpn_selected_len;
  }

  if (ssl->ctx->dos_protection_cb != NULL &&
      ssl->ctx->dos_protection_cb(&client_hello) == 0) {
    /* Connection rejected for DOS reasons. */
    OPENSSL_PUT_ERROR(SSL, SSL_R_CONNECTION_REJECTED);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
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

  if (ssl->early_data_accepted) {
    if (!tls13_derive_early_secrets(hs)) {
      return ssl_hs_error;
    }
  } else if (hs->early_data_offered) {
    ssl->s3->skip_early_data = 1;
  }

  ssl->method->received_flight(ssl);

  /* Resolve ECDHE and incorporate it into the secret. */
  int need_retry;
  if (!resolve_ecdhe_secret(hs, &need_retry, &client_hello)) {
    if (need_retry) {
      ssl->early_data_accepted = 0;
      ssl->s3->skip_early_data = 1;
      hs->tls13_state = state_send_hello_retry_request;
      return ssl_hs_ok;
    }
    return ssl_hs_error;
  }

  hs->tls13_state = state_send_server_hello;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_hello_retry_request(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  CBB cbb, body, extensions;
  uint16_t group_id;
  if (!ssl->method->init_message(ssl, &cbb, &body,
                                 SSL3_MT_HELLO_RETRY_REQUEST) ||
      !CBB_add_u16(&body, ssl->version) ||
      !tls1_get_shared_group(hs, &group_id) ||
      !CBB_add_u16_length_prefixed(&body, &extensions) ||
      !CBB_add_u16(&extensions, TLSEXT_TYPE_key_share) ||
      !CBB_add_u16(&extensions, 2 /* length */) ||
      !CBB_add_u16(&extensions, group_id) ||
      !ssl_add_message_cbb(ssl, &cbb)) {
    CBB_cleanup(&cbb);
    return ssl_hs_error;
  }

  hs->tls13_state = state_process_second_client_hello;
  return ssl_hs_flush_and_read_message;
}

static enum ssl_hs_wait_t do_process_second_client_hello(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (!ssl_check_message_type(ssl, SSL3_MT_CLIENT_HELLO)) {
    return ssl_hs_error;
  }

  SSL_CLIENT_HELLO client_hello;
  if (!ssl_client_hello_init(ssl, &client_hello, ssl->init_msg,
                             ssl->init_num)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CLIENTHELLO_PARSE_FAILED);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return ssl_hs_error;
  }

  int need_retry;
  if (!resolve_ecdhe_secret(hs, &need_retry, &client_hello)) {
    if (need_retry) {
      /* Only send one HelloRetryRequest. */
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
      OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_CURVE);
    }
    return ssl_hs_error;
  }

  if (!ssl_hash_current_message(hs)) {
    return ssl_hs_error;
  }

  ssl->method->received_flight(ssl);
  hs->tls13_state = state_send_server_hello;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_server_hello(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;

  /* Send a ServerHello. */
  CBB cbb, body, extensions;
  if (!ssl->method->init_message(ssl, &cbb, &body, SSL3_MT_SERVER_HELLO) ||
      !CBB_add_u16(&body, ssl->version) ||
      !RAND_bytes(ssl->s3->server_random, sizeof(ssl->s3->server_random)) ||
      !CBB_add_bytes(&body, ssl->s3->server_random, SSL3_RANDOM_SIZE) ||
      !CBB_add_u16(&body, ssl_cipher_get_value(hs->new_cipher)) ||
      !CBB_add_u16_length_prefixed(&body, &extensions) ||
      !ssl_ext_pre_shared_key_add_serverhello(hs, &extensions) ||
      !ssl_ext_key_share_add_serverhello(hs, &extensions) ||
      !ssl_add_message_cbb(ssl, &cbb)) {
    goto err;
  }

  /* Derive and enable the handshake traffic secrets. */
  if (!tls13_derive_handshake_secrets(hs) ||
      !tls13_set_traffic_key(ssl, evp_aead_seal, hs->server_handshake_secret,
                             hs->hash_len)) {
    goto err;
  }

  /* Send EncryptedExtensions. */
  if (!ssl->method->init_message(ssl, &cbb, &body,
                                 SSL3_MT_ENCRYPTED_EXTENSIONS) ||
      !ssl_add_serverhello_tlsext(hs, &body) ||
      !ssl_add_message_cbb(ssl, &cbb)) {
    goto err;
  }

  /* Determine whether to request a client certificate. */
  hs->cert_request = !!(ssl->verify_mode & SSL_VERIFY_PEER);
  /* CertificateRequest may only be sent in non-resumption handshakes. */
  if (ssl->s3->session_reused) {
    hs->cert_request = 0;
  }

  /* Send a CertificateRequest, if necessary. */
  if (hs->cert_request) {
    CBB sigalgs_cbb;
    if (!ssl->method->init_message(ssl, &cbb, &body,
                                   SSL3_MT_CERTIFICATE_REQUEST) ||
        !CBB_add_u8(&body, 0 /* no certificate_request_context. */) ||
        !CBB_add_u16_length_prefixed(&body, &sigalgs_cbb) ||
        !tls12_add_verify_sigalgs(ssl, &sigalgs_cbb) ||
        !ssl_add_client_CA_list(ssl, &body) ||
        !CBB_add_u16(&body, 0 /* empty certificate_extensions. */) ||
        !ssl_add_message_cbb(ssl, &cbb)) {
      goto err;
    }
  }

  /* Send the server Certificate message, if necessary. */
  if (!ssl->s3->session_reused) {
    if (!ssl_has_certificate(ssl)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_NO_CERTIFICATE_SET);
      goto err;
    }

    if (!tls13_add_certificate(hs)) {
      goto err;
    }

    hs->tls13_state = state_send_server_certificate_verify;
    return ssl_hs_ok;
  }

  hs->tls13_state = state_send_server_finished;
  return ssl_hs_ok;

err:
  CBB_cleanup(&cbb);
  return ssl_hs_error;
}

static enum ssl_hs_wait_t do_send_server_certificate_verify(SSL_HANDSHAKE *hs,
                                                            int is_first_run) {
  switch (tls13_add_certificate_verify(hs, is_first_run)) {
    case ssl_private_key_success:
      hs->tls13_state = state_send_server_finished;
      return ssl_hs_ok;

    case ssl_private_key_retry:
      hs->tls13_state = state_complete_server_certificate_verify;
      return ssl_hs_private_key_operation;

    case ssl_private_key_failure:
      return ssl_hs_error;
  }

  assert(0);
  return ssl_hs_error;
}

static enum ssl_hs_wait_t do_send_server_finished(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (!tls13_add_finished(hs) ||
      /* Update the secret to the master secret and derive traffic keys. */
      !tls13_advance_key_schedule(hs, kZeroes, hs->hash_len) ||
      !tls13_derive_application_secrets(hs) ||
      !tls13_set_traffic_key(ssl, evp_aead_seal, hs->server_traffic_secret_0,
                             hs->hash_len)) {
    return ssl_hs_error;
  }

  if (ssl->early_data_accepted) {
    /* If accepting 0-RTT, we send tickets half-RTT. This gets the tickets on
     * the wire sooner and also avoids triggering a write on |SSL_read| when
     * processing the client Finished. This requires computing the client
     * Finished early. See draft-ietf-tls-tls13-18, section 4.5.1. */
    size_t finished_len;
    if (!tls13_finished_mac(hs, hs->expected_client_finished, &finished_len,
                            0 /* client */)) {
      return ssl_hs_error;
    }

    if (finished_len != hs->hash_len) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return ssl_hs_error;
    }

    /* Feed the predicted Finished into the transcript. This allows us to derive
     * the resumption secret early and send half-RTT tickets.
     *
     * TODO(davidben): This will need to be updated for DTLS 1.3. */
    assert(!SSL_is_dtls(hs->ssl));
    uint8_t header[4] = {SSL3_MT_FINISHED, 0, 0, hs->hash_len};
    if (!SSL_TRANSCRIPT_update(&hs->transcript, header, sizeof(header)) ||
        !SSL_TRANSCRIPT_update(&hs->transcript, hs->expected_client_finished,
                               hs->hash_len) ||
        !tls13_derive_resumption_secret(hs) ||
        !add_new_session_tickets(hs)) {
      return ssl_hs_error;
    }
  }

  hs->tls13_state = state_read_second_client_flight;
  return ssl_hs_flush;
}

static enum ssl_hs_wait_t do_read_second_client_flight(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (ssl->early_data_accepted) {
    if (!tls13_set_traffic_key(ssl, evp_aead_open, hs->early_traffic_secret,
                               hs->hash_len)) {
      return ssl_hs_error;
    }
    hs->can_early_write = 1;
    hs->can_early_read = 1;
    hs->tls13_state = state_process_end_of_early_data;
    return ssl_hs_read_end_of_early_data;
  }
  hs->tls13_state = state_process_end_of_early_data;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_process_end_of_early_data(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (!tls13_set_traffic_key(ssl, evp_aead_open, hs->client_handshake_secret,
                             hs->hash_len)) {
    return ssl_hs_error;
  }
  hs->tls13_state = ssl->early_data_accepted ? state_process_client_finished
                                             : state_process_client_certificate;
  return ssl_hs_read_message;
}

static enum ssl_hs_wait_t do_process_client_certificate(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (!hs->cert_request) {
    /* OpenSSL returns X509_V_OK when no certificates are requested. This is
     * classed by them as a bug, but it's assumed by at least NGINX. */
    hs->new_session->verify_result = X509_V_OK;

    /* Skip this state. */
    hs->tls13_state = state_process_channel_id;
    return ssl_hs_ok;
  }

  const int allow_anonymous =
      (ssl->verify_mode & SSL_VERIFY_FAIL_IF_NO_PEER_CERT) == 0;

  if (!ssl_check_message_type(ssl, SSL3_MT_CERTIFICATE) ||
      !tls13_process_certificate(hs, allow_anonymous) ||
      !ssl_hash_current_message(hs)) {
    return ssl_hs_error;
  }

  hs->tls13_state = state_process_client_certificate_verify;
  return ssl_hs_read_message;
}

static enum ssl_hs_wait_t do_process_client_certificate_verify(
    SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (sk_CRYPTO_BUFFER_num(hs->new_session->certs) == 0) {
    /* Skip this state. */
    hs->tls13_state = state_process_channel_id;
    return ssl_hs_ok;
  }

  if (!ssl_check_message_type(ssl, SSL3_MT_CERTIFICATE_VERIFY) ||
      !tls13_process_certificate_verify(hs) ||
      !ssl_hash_current_message(hs)) {
    return ssl_hs_error;
  }

  hs->tls13_state = state_process_channel_id;
  return ssl_hs_read_message;
}

static enum ssl_hs_wait_t do_process_channel_id(SSL_HANDSHAKE *hs) {
  if (!hs->ssl->s3->tlsext_channel_id_valid) {
    hs->tls13_state = state_process_client_finished;
    return ssl_hs_ok;
  }

  if (!ssl_check_message_type(hs->ssl, SSL3_MT_CHANNEL_ID) ||
      !tls1_verify_channel_id(hs) ||
      !ssl_hash_current_message(hs)) {
    return ssl_hs_error;
  }

  hs->tls13_state = state_process_client_finished;
  return ssl_hs_read_message;
}

static enum ssl_hs_wait_t do_process_client_finished(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (!ssl_check_message_type(ssl, SSL3_MT_FINISHED) ||
      /* If early data was accepted, we've already computed the client Finished
       * and derived the resumption secret. */
      !tls13_process_finished(hs, ssl->early_data_accepted) ||
      /* evp_aead_seal keys have already been switched. */
      !tls13_set_traffic_key(ssl, evp_aead_open, hs->client_traffic_secret_0,
                             hs->hash_len)) {
    return ssl_hs_error;
  }

  ssl->method->received_flight(ssl);

  if (!ssl->early_data_accepted) {
    if (!ssl_hash_current_message(hs) ||
        !tls13_derive_resumption_secret(hs)) {
      return ssl_hs_error;
    }

    /* We send post-handshake tickets as part of the handshake in 1-RTT. */
    hs->tls13_state = state_send_new_session_ticket;
    return ssl_hs_ok;
  }

  hs->tls13_state = state_done;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_new_session_ticket(SSL_HANDSHAKE *hs) {
  /* If the client doesn't accept resumption with PSK_DHE_KE, don't send a
   * session ticket. */
  if (!hs->accept_psk_mode) {
    hs->tls13_state = state_done;
    return ssl_hs_ok;
  }

  if (!add_new_session_tickets(hs)) {
    return ssl_hs_error;
  }

  hs->tls13_state = state_done;
  return ssl_hs_flush;
}

enum ssl_hs_wait_t tls13_server_handshake(SSL_HANDSHAKE *hs) {
  while (hs->tls13_state != state_done) {
    enum ssl_hs_wait_t ret = ssl_hs_error;
    enum server_hs_state_t state = hs->tls13_state;
    switch (state) {
      case state_select_parameters:
        ret = do_select_parameters(hs);
        break;
      case state_select_session:
        ret = do_select_session(hs);
        break;
      case state_send_hello_retry_request:
        ret = do_send_hello_retry_request(hs);
        break;
      case state_process_second_client_hello:
        ret = do_process_second_client_hello(hs);
        break;
      case state_send_server_hello:
        ret = do_send_server_hello(hs);
        break;
      case state_send_server_certificate_verify:
        ret = do_send_server_certificate_verify(hs, 1 /* first run */);
      break;
      case state_complete_server_certificate_verify:
        ret = do_send_server_certificate_verify(hs, 0 /* complete */);
        break;
      case state_send_server_finished:
        ret = do_send_server_finished(hs);
        break;
      case state_read_second_client_flight:
        ret = do_read_second_client_flight(hs);
        break;
      case state_process_end_of_early_data:
        ret = do_process_end_of_early_data(hs);
        break;
      case state_process_client_certificate:
        ret = do_process_client_certificate(hs);
        break;
      case state_process_client_certificate_verify:
        ret = do_process_client_certificate_verify(hs);
        break;
      case state_process_channel_id:
        ret = do_process_channel_id(hs);
        break;
      case state_process_client_finished:
        ret = do_process_client_finished(hs);
        break;
      case state_send_new_session_ticket:
        ret = do_send_new_session_ticket(hs);
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
