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


BSSL_NAMESPACE_BEGIN

enum server_hs_state_t {
  state_select_parameters = 0,
  state_select_session,
  state_send_hello_retry_request,
  state_read_second_client_hello,
  state_send_server_hello,
  state_send_server_certificate_verify,
  state_send_server_finished,
  state_read_second_client_flight,
  state_process_end_of_early_data,
  state_read_client_certificate,
  state_read_client_certificate_verify,
  state_read_channel_id,
  state_read_client_finished,
  state_send_new_session_ticket,
  state_done,
};

static const uint8_t kZeroes[EVP_MAX_MD_SIZE] = {0};

static int resolve_ecdhe_secret(SSL_HANDSHAKE *hs, bool *out_need_retry,
                                SSL_CLIENT_HELLO *client_hello) {
  SSL *const ssl = hs->ssl;
  *out_need_retry = false;

  // We only support connections that include an ECDHE key exchange.
  CBS key_share;
  if (!ssl_client_hello_get_extension(client_hello, &key_share,
                                      TLSEXT_TYPE_key_share)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_MISSING_KEY_SHARE);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_MISSING_EXTENSION);
    return 0;
  }

  bool found_key_share;
  Array<uint8_t> dhe_secret;
  uint8_t alert = SSL_AD_DECODE_ERROR;
  if (!ssl_ext_key_share_parse_clienthello(hs, &found_key_share, &dhe_secret,
                                           &alert, &key_share)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
    return 0;
  }

  if (!found_key_share) {
    *out_need_retry = true;
    return 0;
  }

  return tls13_advance_key_schedule(hs, dhe_secret.data(), dhe_secret.size());
}

static int ssl_ext_supported_versions_add_serverhello(SSL_HANDSHAKE *hs,
                                                      CBB *out) {
  CBB contents;
  if (!CBB_add_u16(out, TLSEXT_TYPE_supported_versions) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u16(&contents, hs->ssl->version) ||
      !CBB_flush(out)) {
    return 0;
  }

  return 1;
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
  const uint16_t version = ssl_protocol_version(ssl);

  const SSL_CIPHER *best = NULL;
  while (CBS_len(&cipher_suites) > 0) {
    uint16_t cipher_suite;
    if (!CBS_get_u16(&cipher_suites, &cipher_suite)) {
      return NULL;
    }

    // Limit to TLS 1.3 ciphers we know about.
    const SSL_CIPHER *candidate = SSL_get_cipher_by_value(cipher_suite);
    if (candidate == NULL ||
        SSL_CIPHER_get_min_version(candidate) > version ||
        SSL_CIPHER_get_max_version(candidate) < version) {
      continue;
    }

    // TLS 1.3 removes legacy ciphers, so honor the client order, but prefer
    // ChaCha20 if we do not have AES hardware.
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

static bool add_new_session_tickets(SSL_HANDSHAKE *hs, bool *out_sent_tickets) {
  SSL *const ssl = hs->ssl;
  if (// If the client doesn't accept resumption with PSK_DHE_KE, don't send a
      // session ticket.
      !hs->accept_psk_mode ||
      // We only implement stateless resumption in TLS 1.3, so skip sending
      // tickets if disabled.
      (SSL_get_options(ssl) & SSL_OP_NO_TICKET)) {
    *out_sent_tickets = false;
    return true;
  }

  // TLS 1.3 recommends single-use tickets, so issue multiple tickets in case
  // the client makes several connections before getting a renewal.
  static const int kNumTickets = 2;

  // Rebase the session timestamp so that it is measured from ticket
  // issuance.
  ssl_session_rebase_time(ssl, hs->new_session.get());

  for (int i = 0; i < kNumTickets; i++) {
    UniquePtr<SSL_SESSION> session(
        SSL_SESSION_dup(hs->new_session.get(), SSL_SESSION_INCLUDE_NONAUTH));
    if (!session) {
      return false;
    }

    if (!RAND_bytes((uint8_t *)&session->ticket_age_add, 4)) {
      return false;
    }
    session->ticket_age_add_valid = true;
    if (ssl->enable_early_data) {
      session->ticket_max_early_data = kMaxEarlyDataAccepted;
    }

    static_assert(kNumTickets < 256, "Too many tickets");
    uint8_t nonce[] = {static_cast<uint8_t>(i)};

    ScopedCBB cbb;
    CBB body, nonce_cbb, ticket, extensions;
    if (!ssl->method->init_message(ssl, cbb.get(), &body,
                                   SSL3_MT_NEW_SESSION_TICKET) ||
        !CBB_add_u32(&body, session->timeout) ||
        !CBB_add_u32(&body, session->ticket_age_add) ||
        !CBB_add_u8_length_prefixed(&body, &nonce_cbb) ||
        !CBB_add_bytes(&nonce_cbb, nonce, sizeof(nonce)) ||
        !CBB_add_u16_length_prefixed(&body, &ticket) ||
        !tls13_derive_session_psk(session.get(), nonce,
                                  ssl->ctx->quic_method != nullptr) ||
        !ssl_encrypt_ticket(hs, &ticket, session.get()) ||
        !CBB_add_u16_length_prefixed(&body, &extensions)) {
      return false;
    }

    if (ssl->enable_early_data) {
      CBB early_data_info;
      if (!CBB_add_u16(&extensions, TLSEXT_TYPE_early_data) ||
          !CBB_add_u16_length_prefixed(&extensions, &early_data_info) ||
          !CBB_add_u32(&early_data_info, session->ticket_max_early_data) ||
          !CBB_flush(&extensions)) {
        return false;
      }
    }

    // Add a fake extension. See draft-davidben-tls-grease-01.
    if (!CBB_add_u16(&extensions,
                     ssl_get_grease_value(hs, ssl_grease_ticket_extension)) ||
        !CBB_add_u16(&extensions, 0 /* empty */)) {
      return false;
    }

    if (!ssl_add_message_cbb(ssl, cbb.get())) {
      return false;
    }
  }

  *out_sent_tickets = true;
  return true;
}

static enum ssl_hs_wait_t do_select_parameters(SSL_HANDSHAKE *hs) {
  // At this point, most ClientHello extensions have already been processed by
  // the common handshake logic. Resolve the remaining non-PSK parameters.
  SSL *const ssl = hs->ssl;
  SSLMessage msg;
  if (!ssl->method->get_message(ssl, &msg)) {
    return ssl_hs_read_message;
  }
  SSL_CLIENT_HELLO client_hello;
  if (!ssl_client_hello_init(ssl, &client_hello, msg)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CLIENTHELLO_PARSE_FAILED);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return ssl_hs_error;
  }

  OPENSSL_memcpy(hs->session_id, client_hello.session_id,
                 client_hello.session_id_len);
  hs->session_id_len = client_hello.session_id_len;

  // Negotiate the cipher suite.
  hs->new_cipher = choose_tls13_cipher(ssl, &client_hello);
  if (hs->new_cipher == NULL) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_NO_SHARED_CIPHER);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
    return ssl_hs_error;
  }

  // HTTP/2 negotiation depends on the cipher suite, so ALPN negotiation was
  // deferred. Complete it now.
  uint8_t alert = SSL_AD_DECODE_ERROR;
  if (!ssl_negotiate_alpn(hs, &alert, &client_hello)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
    return ssl_hs_error;
  }

  // The PRF hash is now known. Set up the key schedule and hash the
  // ClientHello.
  if (!hs->transcript.InitHash(ssl_protocol_version(ssl), hs->new_cipher)) {
    return ssl_hs_error;
  }

  if (!ssl_hash_message(hs, msg)) {
    return ssl_hs_error;
  }

  hs->tls13_state = state_select_session;
  return ssl_hs_ok;
}

static enum ssl_ticket_aead_result_t select_session(
    SSL_HANDSHAKE *hs, uint8_t *out_alert, UniquePtr<SSL_SESSION> *out_session,
    int32_t *out_ticket_age_skew, const SSLMessage &msg,
    const SSL_CLIENT_HELLO *client_hello) {
  SSL *const ssl = hs->ssl;
  *out_session = NULL;

  // Decode the ticket if we agreed on a PSK key exchange mode.
  CBS pre_shared_key;
  if (!hs->accept_psk_mode ||
      !ssl_client_hello_get_extension(client_hello, &pre_shared_key,
                                      TLSEXT_TYPE_pre_shared_key)) {
    return ssl_ticket_aead_ignore_ticket;
  }

  // Verify that the pre_shared_key extension is the last extension in
  // ClientHello.
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

  // TLS 1.3 session tickets are renewed separately as part of the
  // NewSessionTicket.
  bool unused_renew;
  UniquePtr<SSL_SESSION> session;
  enum ssl_ticket_aead_result_t ret =
      ssl_process_ticket(hs, &session, &unused_renew, ticket, {});
  switch (ret) {
    case ssl_ticket_aead_success:
      break;
    case ssl_ticket_aead_error:
      *out_alert = SSL_AD_INTERNAL_ERROR;
      return ret;
    default:
      return ret;
  }

  if (!ssl_session_is_resumable(hs, session.get()) ||
      // Historically, some TLS 1.3 tickets were missing ticket_age_add.
      !session->ticket_age_add_valid) {
    return ssl_ticket_aead_ignore_ticket;
  }

  // Recover the client ticket age and convert to seconds.
  client_ticket_age -= session->ticket_age_add;
  client_ticket_age /= 1000;

  struct OPENSSL_timeval now;
  ssl_get_current_time(ssl, &now);

  // Compute the server ticket age in seconds.
  assert(now.tv_sec >= session->time);
  uint64_t server_ticket_age = now.tv_sec - session->time;

  // To avoid overflowing |hs->ticket_age_skew|, we will not resume
  // 68-year-old sessions.
  if (server_ticket_age > INT32_MAX) {
    return ssl_ticket_aead_ignore_ticket;
  }

  // TODO(davidben,svaldez): Measure this value to decide on tolerance. For
  // now, accept all values. https://crbug.com/boringssl/113.
  *out_ticket_age_skew =
      (int32_t)client_ticket_age - (int32_t)server_ticket_age;

  // Check the PSK binder.
  if (!tls13_verify_psk_binder(hs, session.get(), msg, &binders)) {
    *out_alert = SSL_AD_DECRYPT_ERROR;
    return ssl_ticket_aead_error;
  }

  *out_session = std::move(session);
  return ssl_ticket_aead_success;
}

static enum ssl_hs_wait_t do_select_session(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  SSLMessage msg;
  if (!ssl->method->get_message(ssl, &msg)) {
    return ssl_hs_read_message;
  }
  SSL_CLIENT_HELLO client_hello;
  if (!ssl_client_hello_init(ssl, &client_hello, msg)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CLIENTHELLO_PARSE_FAILED);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return ssl_hs_error;
  }

  uint8_t alert = SSL_AD_DECODE_ERROR;
  UniquePtr<SSL_SESSION> session;
  switch (select_session(hs, &alert, &session, &ssl->s3->ticket_age_skew, msg,
                         &client_hello)) {
    case ssl_ticket_aead_ignore_ticket:
      assert(!session);
      if (!ssl_get_new_session(hs, 1 /* server */)) {
        ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
        return ssl_hs_error;
      }
      break;

    case ssl_ticket_aead_success:
      // Carry over authentication information from the previous handshake into
      // a fresh session.
      hs->new_session =
          SSL_SESSION_dup(session.get(), SSL_SESSION_DUP_AUTH_ONLY);

      if (ssl->enable_early_data &&
          // Early data must be acceptable for this ticket.
          session->ticket_max_early_data != 0 &&
          // The client must have offered early data.
          hs->early_data_offered &&
          // Channel ID is incompatible with 0-RTT.
          !ssl->s3->channel_id_valid &&
          // If Token Binding is negotiated, reject 0-RTT.
          !ssl->s3->token_binding_negotiated &&
          // The negotiated ALPN must match the one in the ticket.
          MakeConstSpan(ssl->s3->alpn_selected) == session->early_alpn) {
        ssl->s3->early_data_accepted = true;
      }

      if (hs->new_session == NULL) {
        ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
        return ssl_hs_error;
      }

      ssl->s3->session_reused = true;

      // Resumption incorporates fresh key material, so refresh the timeout.
      ssl_session_renew_timeout(ssl, hs->new_session.get(),
                                ssl->session_ctx->session_psk_dhe_timeout);
      break;

    case ssl_ticket_aead_error:
      ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
      return ssl_hs_error;

    case ssl_ticket_aead_retry:
      hs->tls13_state = state_select_session;
      return ssl_hs_pending_ticket;
  }

  // Record connection properties in the new session.
  hs->new_session->cipher = hs->new_cipher;

  // Store the initial negotiated ALPN in the session.
  if (!hs->new_session->early_alpn.CopyFrom(ssl->s3->alpn_selected)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
    return ssl_hs_error;
  }

  if (ssl->ctx->dos_protection_cb != NULL &&
      ssl->ctx->dos_protection_cb(&client_hello) == 0) {
    // Connection rejected for DOS reasons.
    OPENSSL_PUT_ERROR(SSL, SSL_R_CONNECTION_REJECTED);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
    return ssl_hs_error;
  }

  size_t hash_len = EVP_MD_size(
      ssl_get_handshake_digest(ssl_protocol_version(ssl), hs->new_cipher));

  // Set up the key schedule and incorporate the PSK into the running secret.
  if (ssl->s3->session_reused) {
    if (!tls13_init_key_schedule(hs, hs->new_session->master_key,
                                    hs->new_session->master_key_length)) {
      return ssl_hs_error;
    }
  } else if (!tls13_init_key_schedule(hs, kZeroes, hash_len)) {
    return ssl_hs_error;
  }

  if (ssl->s3->early_data_accepted) {
    if (!tls13_derive_early_secrets(hs)) {
      return ssl_hs_error;
    }
  } else if (hs->early_data_offered) {
    ssl->s3->skip_early_data = true;
  }

  // Resolve ECDHE and incorporate it into the secret.
  bool need_retry;
  if (!resolve_ecdhe_secret(hs, &need_retry, &client_hello)) {
    if (need_retry) {
      ssl->s3->early_data_accepted = false;
      ssl->s3->skip_early_data = true;
      ssl->method->next_message(ssl);
      if (!hs->transcript.UpdateForHelloRetryRequest()) {
        return ssl_hs_error;
      }
      hs->tls13_state = state_send_hello_retry_request;
      return ssl_hs_ok;
    }
    return ssl_hs_error;
  }

  ssl->method->next_message(ssl);
  hs->tls13_state = state_send_server_hello;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_hello_retry_request(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;


  ScopedCBB cbb;
  CBB body, session_id, extensions;
  uint16_t group_id;
  if (!ssl->method->init_message(ssl, cbb.get(), &body, SSL3_MT_SERVER_HELLO) ||
      !CBB_add_u16(&body, TLS1_2_VERSION) ||
      !CBB_add_bytes(&body, kHelloRetryRequest, SSL3_RANDOM_SIZE) ||
      !CBB_add_u8_length_prefixed(&body, &session_id) ||
      !CBB_add_bytes(&session_id, hs->session_id, hs->session_id_len) ||
      !CBB_add_u16(&body, ssl_cipher_get_value(hs->new_cipher)) ||
      !CBB_add_u8(&body, 0 /* no compression */) ||
      !tls1_get_shared_group(hs, &group_id) ||
      !CBB_add_u16_length_prefixed(&body, &extensions) ||
      !CBB_add_u16(&extensions, TLSEXT_TYPE_supported_versions) ||
      !CBB_add_u16(&extensions, 2 /* length */) ||
      !CBB_add_u16(&extensions, ssl->version) ||
      !CBB_add_u16(&extensions, TLSEXT_TYPE_key_share) ||
      !CBB_add_u16(&extensions, 2 /* length */) ||
      !CBB_add_u16(&extensions, group_id) ||
      !ssl_add_message_cbb(ssl, cbb.get())) {
    return ssl_hs_error;
  }

  if (!ssl->method->add_change_cipher_spec(ssl)) {
    return ssl_hs_error;
  }

  hs->sent_hello_retry_request = true;
  hs->tls13_state = state_read_second_client_hello;
  return ssl_hs_flush;
}

static enum ssl_hs_wait_t do_read_second_client_hello(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  SSLMessage msg;
  if (!ssl->method->get_message(ssl, &msg)) {
    return ssl_hs_read_message;
  }
  if (!ssl_check_message_type(ssl, msg, SSL3_MT_CLIENT_HELLO)) {
    return ssl_hs_error;
  }
  SSL_CLIENT_HELLO client_hello;
  if (!ssl_client_hello_init(ssl, &client_hello, msg)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CLIENTHELLO_PARSE_FAILED);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return ssl_hs_error;
  }

  bool need_retry;
  if (!resolve_ecdhe_secret(hs, &need_retry, &client_hello)) {
    if (need_retry) {
      // Only send one HelloRetryRequest.
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
      OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_CURVE);
    }
    return ssl_hs_error;
  }

  if (!ssl_hash_message(hs, msg)) {
    return ssl_hs_error;
  }

  ssl->method->next_message(ssl);
  hs->tls13_state = state_send_server_hello;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_server_hello(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;

  // Send a ServerHello.
  ScopedCBB cbb;
  CBB body, extensions, session_id;
  if (!ssl->method->init_message(ssl, cbb.get(), &body, SSL3_MT_SERVER_HELLO) ||
      !CBB_add_u16(&body, TLS1_2_VERSION) ||
      !RAND_bytes(ssl->s3->server_random, sizeof(ssl->s3->server_random)) ||
      !CBB_add_bytes(&body, ssl->s3->server_random, SSL3_RANDOM_SIZE) ||
      !CBB_add_u8_length_prefixed(&body, &session_id) ||
      !CBB_add_bytes(&session_id, hs->session_id, hs->session_id_len) ||
      !CBB_add_u16(&body, ssl_cipher_get_value(hs->new_cipher)) ||
      !CBB_add_u8(&body, 0) ||
      !CBB_add_u16_length_prefixed(&body, &extensions) ||
      !ssl_ext_pre_shared_key_add_serverhello(hs, &extensions) ||
      !ssl_ext_key_share_add_serverhello(hs, &extensions) ||
      !ssl_ext_supported_versions_add_serverhello(hs, &extensions) ||
      !ssl_add_message_cbb(ssl, cbb.get())) {
    return ssl_hs_error;
  }

  if (!hs->sent_hello_retry_request &&
      !ssl->method->add_change_cipher_spec(ssl)) {
    return ssl_hs_error;
  }

  // Derive and enable the handshake traffic secrets.
  if (!tls13_derive_handshake_secrets(hs) ||
      !tls13_set_traffic_key(ssl, ssl_encryption_handshake, evp_aead_seal,
                             hs->server_handshake_secret, hs->hash_len)) {
    return ssl_hs_error;
  }

  // Send EncryptedExtensions.
  if (!ssl->method->init_message(ssl, cbb.get(), &body,
                                 SSL3_MT_ENCRYPTED_EXTENSIONS) ||
      !ssl_add_serverhello_tlsext(hs, &body) ||
      !ssl_add_message_cbb(ssl, cbb.get())) {
    return ssl_hs_error;
  }

  if (!ssl->s3->session_reused) {
    // Determine whether to request a client certificate.
    hs->cert_request = !!(hs->config->verify_mode & SSL_VERIFY_PEER);
    // Only request a certificate if Channel ID isn't negotiated.
    if ((hs->config->verify_mode & SSL_VERIFY_PEER_IF_NO_OBC) &&
        ssl->s3->channel_id_valid) {
      hs->cert_request = false;
    }
  }

  // Send a CertificateRequest, if necessary.
  if (hs->cert_request) {
    CBB cert_request_extensions, sigalg_contents, sigalgs_cbb;
    if (!ssl->method->init_message(ssl, cbb.get(), &body,
                                   SSL3_MT_CERTIFICATE_REQUEST) ||
        !CBB_add_u8(&body, 0 /* no certificate_request_context. */) ||
        !CBB_add_u16_length_prefixed(&body, &cert_request_extensions) ||
        !CBB_add_u16(&cert_request_extensions,
                     TLSEXT_TYPE_signature_algorithms) ||
        !CBB_add_u16_length_prefixed(&cert_request_extensions,
                                     &sigalg_contents) ||
        !CBB_add_u16_length_prefixed(&sigalg_contents, &sigalgs_cbb) ||
        !tls12_add_verify_sigalgs(ssl, &sigalgs_cbb,
                                  false /* online signature */)) {
      return ssl_hs_error;
    }

    if (tls12_has_different_verify_sigalgs_for_certs(ssl)) {
      if (!CBB_add_u16(&cert_request_extensions,
                       TLSEXT_TYPE_signature_algorithms_cert) ||
          !CBB_add_u16_length_prefixed(&cert_request_extensions,
                                       &sigalg_contents) ||
          !CBB_add_u16_length_prefixed(&sigalg_contents, &sigalgs_cbb) ||
          !tls12_add_verify_sigalgs(ssl, &sigalgs_cbb, true /* certs */)) {
        return ssl_hs_error;
      }
    }

    if (ssl_has_client_CAs(hs->config)) {
      CBB ca_contents;
      if (!CBB_add_u16(&cert_request_extensions,
                       TLSEXT_TYPE_certificate_authorities) ||
          !CBB_add_u16_length_prefixed(&cert_request_extensions,
                                       &ca_contents) ||
          !ssl_add_client_CA_list(hs, &ca_contents) ||
          !CBB_flush(&cert_request_extensions)) {
        return ssl_hs_error;
      }
    }

    if (!ssl_add_message_cbb(ssl, cbb.get())) {
      return ssl_hs_error;
    }
  }

  // Send the server Certificate message, if necessary.
  if (!ssl->s3->session_reused) {
    if (!ssl_has_certificate(hs->config)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_NO_CERTIFICATE_SET);
      return ssl_hs_error;
    }

    if (!tls13_add_certificate(hs)) {
      return ssl_hs_error;
    }

    hs->tls13_state = state_send_server_certificate_verify;
    return ssl_hs_ok;
  }

  hs->tls13_state = state_send_server_finished;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_server_certificate_verify(SSL_HANDSHAKE *hs) {
  switch (tls13_add_certificate_verify(hs)) {
    case ssl_private_key_success:
      hs->tls13_state = state_send_server_finished;
      return ssl_hs_ok;

    case ssl_private_key_retry:
      hs->tls13_state = state_send_server_certificate_verify;
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
      // Update the secret to the master secret and derive traffic keys.
      !tls13_advance_key_schedule(hs, kZeroes, hs->hash_len) ||
      !tls13_derive_application_secrets(hs) ||
      !tls13_set_traffic_key(ssl, ssl_encryption_application, evp_aead_seal,
                             hs->server_traffic_secret_0, hs->hash_len)) {
    return ssl_hs_error;
  }

  if (ssl->s3->early_data_accepted) {
    // If accepting 0-RTT, we send tickets half-RTT. This gets the tickets on
    // the wire sooner and also avoids triggering a write on |SSL_read| when
    // processing the client Finished. This requires computing the client
    // Finished early. See RFC 8446, section 4.6.1.
    static const uint8_t kEndOfEarlyData[4] = {SSL3_MT_END_OF_EARLY_DATA, 0,
                                               0, 0};
    if (!hs->transcript.Update(kEndOfEarlyData)) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return ssl_hs_error;
    }

    size_t finished_len;
    if (!tls13_finished_mac(hs, hs->expected_client_finished, &finished_len,
                            false /* client */)) {
      return ssl_hs_error;
    }

    if (finished_len != hs->hash_len) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return ssl_hs_error;
    }

    // Feed the predicted Finished into the transcript. This allows us to derive
    // the resumption secret early and send half-RTT tickets.
    //
    // TODO(davidben): This will need to be updated for DTLS 1.3.
    assert(!SSL_is_dtls(hs->ssl));
    assert(hs->hash_len <= 0xff);
    uint8_t header[4] = {SSL3_MT_FINISHED, 0, 0,
                         static_cast<uint8_t>(hs->hash_len)};
    bool unused_sent_tickets;
    if (!hs->transcript.Update(header) ||
        !hs->transcript.Update(
            MakeConstSpan(hs->expected_client_finished, hs->hash_len)) ||
        !tls13_derive_resumption_secret(hs) ||
        !add_new_session_tickets(hs, &unused_sent_tickets)) {
      return ssl_hs_error;
    }
  }

  hs->tls13_state = state_read_second_client_flight;
  return ssl_hs_flush;
}

static enum ssl_hs_wait_t do_read_second_client_flight(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (ssl->s3->early_data_accepted) {
    if (!tls13_set_traffic_key(ssl, ssl_encryption_early_data, evp_aead_open,
                               hs->early_traffic_secret, hs->hash_len)) {
      return ssl_hs_error;
    }
    hs->can_early_write = true;
    hs->can_early_read = true;
    hs->in_early_data = true;
  }
  hs->tls13_state = state_process_end_of_early_data;
  return ssl->s3->early_data_accepted ? ssl_hs_read_end_of_early_data
                                      : ssl_hs_ok;
}

static enum ssl_hs_wait_t do_process_end_of_early_data(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (hs->early_data_offered) {
    // If early data was not accepted, the EndOfEarlyData and ChangeCipherSpec
    // message will be in the discarded early data.
    if (hs->ssl->s3->early_data_accepted) {
      SSLMessage msg;
      if (!ssl->method->get_message(ssl, &msg)) {
        return ssl_hs_read_message;
      }

      if (!ssl_check_message_type(ssl, msg, SSL3_MT_END_OF_EARLY_DATA)) {
        return ssl_hs_error;
      }
      if (CBS_len(&msg.body) != 0) {
        ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
        OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
        return ssl_hs_error;
      }
      ssl->method->next_message(ssl);
    }
  }
  if (!tls13_set_traffic_key(ssl, ssl_encryption_handshake, evp_aead_open,
                             hs->client_handshake_secret, hs->hash_len)) {
    return ssl_hs_error;
  }
  hs->tls13_state = ssl->s3->early_data_accepted
                        ? state_read_client_finished
                        : state_read_client_certificate;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_read_client_certificate(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (!hs->cert_request) {
    // OpenSSL returns X509_V_OK when no certificates are requested. This is
    // classed by them as a bug, but it's assumed by at least NGINX.
    hs->new_session->verify_result = X509_V_OK;

    // Skip this state.
    hs->tls13_state = state_read_channel_id;
    return ssl_hs_ok;
  }

  const bool allow_anonymous =
      (hs->config->verify_mode & SSL_VERIFY_FAIL_IF_NO_PEER_CERT) == 0;
  SSLMessage msg;
  if (!ssl->method->get_message(ssl, &msg)) {
    return ssl_hs_read_message;
  }
  if (!ssl_check_message_type(ssl, msg, SSL3_MT_CERTIFICATE) ||
      !tls13_process_certificate(hs, msg, allow_anonymous) ||
      !ssl_hash_message(hs, msg)) {
    return ssl_hs_error;
  }

  ssl->method->next_message(ssl);
  hs->tls13_state = state_read_client_certificate_verify;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_read_client_certificate_verify(
    SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (sk_CRYPTO_BUFFER_num(hs->new_session->certs.get()) == 0) {
    // Skip this state.
    hs->tls13_state = state_read_channel_id;
    return ssl_hs_ok;
  }

  SSLMessage msg;
  if (!ssl->method->get_message(ssl, &msg)) {
    return ssl_hs_read_message;
  }

  switch (ssl_verify_peer_cert(hs)) {
    case ssl_verify_ok:
      break;
    case ssl_verify_invalid:
      return ssl_hs_error;
    case ssl_verify_retry:
      hs->tls13_state = state_read_client_certificate_verify;
      return ssl_hs_certificate_verify;
  }

  if (!ssl_check_message_type(ssl, msg, SSL3_MT_CERTIFICATE_VERIFY) ||
      !tls13_process_certificate_verify(hs, msg) ||
      !ssl_hash_message(hs, msg)) {
    return ssl_hs_error;
  }

  ssl->method->next_message(ssl);
  hs->tls13_state = state_read_channel_id;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_read_channel_id(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (!ssl->s3->channel_id_valid) {
    hs->tls13_state = state_read_client_finished;
    return ssl_hs_ok;
  }

  SSLMessage msg;
  if (!ssl->method->get_message(ssl, &msg)) {
    return ssl_hs_read_message;
  }
  if (!ssl_check_message_type(ssl, msg, SSL3_MT_CHANNEL_ID) ||
      !tls1_verify_channel_id(hs, msg) ||
      !ssl_hash_message(hs, msg)) {
    return ssl_hs_error;
  }

  ssl->method->next_message(ssl);
  hs->tls13_state = state_read_client_finished;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_read_client_finished(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  SSLMessage msg;
  if (!ssl->method->get_message(ssl, &msg)) {
    return ssl_hs_read_message;
  }
  if (!ssl_check_message_type(ssl, msg, SSL3_MT_FINISHED) ||
      // If early data was accepted, we've already computed the client Finished
      // and derived the resumption secret.
      !tls13_process_finished(hs, msg, ssl->s3->early_data_accepted) ||
      // evp_aead_seal keys have already been switched.
      !tls13_set_traffic_key(ssl, ssl_encryption_application, evp_aead_open,
                             hs->client_traffic_secret_0, hs->hash_len)) {
    return ssl_hs_error;
  }

  if (!ssl->s3->early_data_accepted) {
    if (!ssl_hash_message(hs, msg) ||
        !tls13_derive_resumption_secret(hs)) {
      return ssl_hs_error;
    }

    // We send post-handshake tickets as part of the handshake in 1-RTT.
    hs->tls13_state = state_send_new_session_ticket;
  } else {
    // We already sent half-RTT tickets.
    hs->tls13_state = state_done;
  }

  ssl->method->next_message(ssl);
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_new_session_ticket(SSL_HANDSHAKE *hs) {
  bool sent_tickets;
  if (!add_new_session_tickets(hs, &sent_tickets)) {
    return ssl_hs_error;
  }

  hs->tls13_state = state_done;
  return sent_tickets ? ssl_hs_flush : ssl_hs_ok;
}

enum ssl_hs_wait_t tls13_server_handshake(SSL_HANDSHAKE *hs) {
  while (hs->tls13_state != state_done) {
    enum ssl_hs_wait_t ret = ssl_hs_error;
    enum server_hs_state_t state =
        static_cast<enum server_hs_state_t>(hs->tls13_state);
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
      case state_read_second_client_hello:
        ret = do_read_second_client_hello(hs);
        break;
      case state_send_server_hello:
        ret = do_send_server_hello(hs);
        break;
      case state_send_server_certificate_verify:
        ret = do_send_server_certificate_verify(hs);
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
      case state_read_client_certificate:
        ret = do_read_client_certificate(hs);
        break;
      case state_read_client_certificate_verify:
        ret = do_read_client_certificate_verify(hs);
        break;
      case state_read_channel_id:
        ret = do_read_channel_id(hs);
        break;
      case state_read_client_finished:
        ret = do_read_client_finished(hs);
        break;
      case state_send_new_session_ticket:
        ret = do_send_new_session_ticket(hs);
        break;
      case state_done:
        ret = ssl_hs_ok;
        break;
    }

    if (hs->tls13_state != state) {
      ssl_do_info_callback(hs->ssl, SSL_CB_ACCEPT_LOOP, 1);
    }

    if (ret != ssl_hs_ok) {
      return ret;
    }
  }

  return ssl_hs_ok;
}

const char *tls13_server_handshake_state(SSL_HANDSHAKE *hs) {
  enum server_hs_state_t state =
      static_cast<enum server_hs_state_t>(hs->tls13_state);
  switch (state) {
    case state_select_parameters:
      return "TLS 1.3 server select_parameters";
    case state_select_session:
      return "TLS 1.3 server select_session";
    case state_send_hello_retry_request:
      return "TLS 1.3 server send_hello_retry_request";
    case state_read_second_client_hello:
      return "TLS 1.3 server read_second_client_hello";
    case state_send_server_hello:
      return "TLS 1.3 server send_server_hello";
    case state_send_server_certificate_verify:
      return "TLS 1.3 server send_server_certificate_verify";
    case state_send_server_finished:
      return "TLS 1.3 server send_server_finished";
    case state_read_second_client_flight:
      return "TLS 1.3 server read_second_client_flight";
    case state_process_end_of_early_data:
      return "TLS 1.3 server process_end_of_early_data";
    case state_read_client_certificate:
      return "TLS 1.3 server read_client_certificate";
    case state_read_client_certificate_verify:
      return "TLS 1.3 server read_client_certificate_verify";
    case state_read_channel_id:
      return "TLS 1.3 server read_channel_id";
    case state_read_client_finished:
      return "TLS 1.3 server read_client_finished";
    case state_send_new_session_ticket:
      return "TLS 1.3 server send_new_session_ticket";
    case state_done:
      return "TLS 1.3 server done";
  }

  return "TLS 1.3 server unknown";
}

BSSL_NAMESPACE_END
