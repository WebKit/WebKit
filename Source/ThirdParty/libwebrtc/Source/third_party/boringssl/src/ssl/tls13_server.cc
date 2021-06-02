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

#include <tuple>

#include <openssl/aead.h>
#include <openssl/bytestring.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/hpke.h>
#include <openssl/mem.h>
#include <openssl/rand.h>
#include <openssl/stack.h>

#include "../crypto/internal.h"
#include "internal.h"


BSSL_NAMESPACE_BEGIN

static const uint8_t kZeroes[EVP_MAX_MD_SIZE] = {0};

// Allow a minute of ticket age skew in either direction. This covers
// transmission delays in ClientHello and NewSessionTicket, as well as
// drift between client and server clock rate since the ticket was issued.
// See RFC 8446, section 8.3.
static const int32_t kMaxTicketAgeSkewSeconds = 60;

static bool resolve_ecdhe_secret(SSL_HANDSHAKE *hs,
                                 const SSL_CLIENT_HELLO *client_hello) {
  SSL *const ssl = hs->ssl;
  const uint16_t group_id = hs->new_session->group_id;

  bool found_key_share;
  Span<const uint8_t> peer_key;
  uint8_t alert = SSL_AD_DECODE_ERROR;
  if (!ssl_ext_key_share_parse_clienthello(hs, &found_key_share, &peer_key,
                                           &alert, client_hello)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
    return false;
  }

  if (!found_key_share) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
    OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_CURVE);
    return false;
  }

  Array<uint8_t> secret;
  SSL_HANDSHAKE_HINTS *const hints = hs->hints.get();
  if (hints && !hs->hints_requested && hints->key_share_group_id == group_id &&
      !hints->key_share_secret.empty()) {
    // Copy DH secret from hints.
    if (!hs->ecdh_public_key.CopyFrom(hints->key_share_public_key) ||
        !secret.CopyFrom(hints->key_share_secret)) {
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      return false;
    }
  } else {
    ScopedCBB public_key;
    UniquePtr<SSLKeyShare> key_share = SSLKeyShare::Create(group_id);
    if (!key_share ||  //
        !CBB_init(public_key.get(), 32) ||
        !key_share->Accept(public_key.get(), &secret, &alert, peer_key) ||
        !CBBFinishArray(public_key.get(), &hs->ecdh_public_key)) {
      ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
      return false;
    }
    if (hints && hs->hints_requested) {
      hints->key_share_group_id = group_id;
      if (!hints->key_share_public_key.CopyFrom(hs->ecdh_public_key) ||
          !hints->key_share_secret.CopyFrom(secret)) {
        ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
        return false;
      }
    }
  }

  return tls13_advance_key_schedule(hs, secret);
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
    const SSL *ssl, const SSL_CLIENT_HELLO *client_hello, uint16_t group_id) {
  CBS cipher_suites;
  CBS_init(&cipher_suites, client_hello->cipher_suites,
           client_hello->cipher_suites_len);

  const uint16_t version = ssl_protocol_version(ssl);

  return ssl_choose_tls13_cipher(cipher_suites, version, group_id);
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
    bool enable_early_data =
        ssl->enable_early_data &&
        (!ssl->quic_method || !ssl->config->quic_early_data_context.empty());
    if (enable_early_data) {
      // QUIC does not use the max_early_data_size parameter and always sets it
      // to a fixed value. See draft-ietf-quic-tls-22, section 4.5.
      session->ticket_max_early_data =
          ssl->quic_method != nullptr ? 0xffffffff : kMaxEarlyDataAccepted;
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
        !tls13_derive_session_psk(session.get(), nonce) ||
        !ssl_encrypt_ticket(hs, &ticket, session.get()) ||
        !CBB_add_u16_length_prefixed(&body, &extensions)) {
      return false;
    }

    if (enable_early_data) {
      CBB early_data;
      if (!CBB_add_u16(&extensions, TLSEXT_TYPE_early_data) ||
          !CBB_add_u16_length_prefixed(&extensions, &early_data) ||
          !CBB_add_u32(&early_data, session->ticket_max_early_data) ||
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
  SSL_CLIENT_HELLO client_hello;
  if (!hs->GetClientHello(&msg, &client_hello)) {
    return ssl_hs_error;
  }

  if (ssl->quic_method != nullptr && client_hello.session_id_len > 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_COMPATIBILITY_MODE);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
    return ssl_hs_error;
  }
  OPENSSL_memcpy(hs->session_id, client_hello.session_id,
                 client_hello.session_id_len);
  hs->session_id_len = client_hello.session_id_len;

  uint16_t group_id;
  if (!tls1_get_shared_group(hs, &group_id)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_NO_SHARED_GROUP);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
    return ssl_hs_error;
  }

  // Negotiate the cipher suite.
  hs->new_cipher = choose_tls13_cipher(ssl, &client_hello, group_id);
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

  hs->tls13_state = state13_select_session;
  return ssl_hs_ok;
}

static enum ssl_ticket_aead_result_t select_session(
    SSL_HANDSHAKE *hs, uint8_t *out_alert, UniquePtr<SSL_SESSION> *out_session,
    int32_t *out_ticket_age_skew, bool *out_offered_ticket,
    const SSLMessage &msg, const SSL_CLIENT_HELLO *client_hello) {
  SSL *const ssl = hs->ssl;
  *out_session = nullptr;

  CBS pre_shared_key;
  *out_offered_ticket = ssl_client_hello_get_extension(
      client_hello, &pre_shared_key, TLSEXT_TYPE_pre_shared_key);
  if (!*out_offered_ticket) {
    return ssl_ticket_aead_ignore_ticket;
  }

  // Per RFC8446, section 4.2.9, servers MUST abort the handshake if the client
  // sends pre_shared_key without psk_key_exchange_modes.
  CBS unused;
  if (!ssl_client_hello_get_extension(client_hello, &unused,
                                      TLSEXT_TYPE_psk_key_exchange_modes)) {
    *out_alert = SSL_AD_MISSING_EXTENSION;
    OPENSSL_PUT_ERROR(SSL, SSL_R_MISSING_EXTENSION);
    return ssl_ticket_aead_error;
  }

  CBS ticket, binders;
  uint32_t client_ticket_age;
  if (!ssl_ext_pre_shared_key_parse_clienthello(
          hs, &ticket, &binders, &client_ticket_age, out_alert, client_hello,
          &pre_shared_key)) {
    return ssl_ticket_aead_error;
  }

  // If the peer did not offer psk_dhe, ignore the resumption.
  if (!hs->accept_psk_mode) {
    return ssl_ticket_aead_ignore_ticket;
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

  *out_ticket_age_skew = static_cast<int32_t>(client_ticket_age) -
                         static_cast<int32_t>(server_ticket_age);

  // Check the PSK binder.
  if (!tls13_verify_psk_binder(hs, session.get(), msg, &binders)) {
    *out_alert = SSL_AD_DECRYPT_ERROR;
    return ssl_ticket_aead_error;
  }

  *out_session = std::move(session);
  return ssl_ticket_aead_success;
}

static bool quic_ticket_compatible(const SSL_SESSION *session,
                                   const SSL_CONFIG *config) {
  if (!session->is_quic) {
    return true;
  }

  if (session->quic_early_data_context.empty() ||
      config->quic_early_data_context.size() !=
          session->quic_early_data_context.size() ||
      CRYPTO_memcmp(config->quic_early_data_context.data(),
                    session->quic_early_data_context.data(),
                    session->quic_early_data_context.size()) != 0) {
    return false;
  }
  return true;
}

static enum ssl_hs_wait_t do_select_session(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  SSLMessage msg;
  SSL_CLIENT_HELLO client_hello;
  if (!hs->GetClientHello(&msg, &client_hello)) {
    return ssl_hs_error;
  }

  uint8_t alert = SSL_AD_DECODE_ERROR;
  UniquePtr<SSL_SESSION> session;
  bool offered_ticket = false;
  switch (select_session(hs, &alert, &session, &ssl->s3->ticket_age_skew,
                         &offered_ticket, msg, &client_hello)) {
    case ssl_ticket_aead_ignore_ticket:
      assert(!session);
      if (!ssl_get_new_session(hs)) {
        ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
        return ssl_hs_error;
      }
      break;

    case ssl_ticket_aead_success:
      // Carry over authentication information from the previous handshake into
      // a fresh session.
      hs->new_session =
          SSL_SESSION_dup(session.get(), SSL_SESSION_DUP_AUTH_ONLY);
      if (hs->new_session == nullptr) {
        ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
        return ssl_hs_error;
      }

      ssl->s3->session_reused = true;
      hs->can_release_private_key = true;

      // Resumption incorporates fresh key material, so refresh the timeout.
      ssl_session_renew_timeout(ssl, hs->new_session.get(),
                                ssl->session_ctx->session_psk_dhe_timeout);
      break;

    case ssl_ticket_aead_error:
      ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
      return ssl_hs_error;

    case ssl_ticket_aead_retry:
      hs->tls13_state = state13_select_session;
      return ssl_hs_pending_ticket;
  }

  // Negotiate ALPS now, after ALPN is negotiated and |hs->new_session| is
  // initialized.
  if (!ssl_negotiate_alps(hs, &alert, &client_hello)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
    return ssl_hs_error;
  }

  // Record connection properties in the new session.
  hs->new_session->cipher = hs->new_cipher;
  if (!tls1_get_shared_group(hs, &hs->new_session->group_id)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_NO_SHARED_GROUP);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
    return ssl_hs_error;
  }

  // Determine if we need HelloRetryRequest.
  bool found_key_share;
  if (!ssl_ext_key_share_parse_clienthello(hs, &found_key_share,
                                           /*out_key_share=*/nullptr, &alert,
                                           &client_hello)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
    return ssl_hs_error;
  }

  // Determine if we're negotiating 0-RTT.
  if (!ssl->enable_early_data) {
    ssl->s3->early_data_reason = ssl_early_data_disabled;
  } else if (!offered_ticket) {
    ssl->s3->early_data_reason = ssl_early_data_no_session_offered;
  } else if (!session) {
    ssl->s3->early_data_reason = ssl_early_data_session_not_resumed;
  } else if (session->ticket_max_early_data == 0) {
    ssl->s3->early_data_reason = ssl_early_data_unsupported_for_session;
  } else if (!hs->early_data_offered) {
    ssl->s3->early_data_reason = ssl_early_data_peer_declined;
  } else if (ssl->s3->channel_id_valid) {
    // Channel ID is incompatible with 0-RTT.
    ssl->s3->early_data_reason = ssl_early_data_channel_id;
  } else if (ssl->s3->token_binding_negotiated) {
    // Token Binding is incompatible with 0-RTT.
    ssl->s3->early_data_reason = ssl_early_data_token_binding;
  } else if (MakeConstSpan(ssl->s3->alpn_selected) != session->early_alpn) {
    // The negotiated ALPN must match the one in the ticket.
    ssl->s3->early_data_reason = ssl_early_data_alpn_mismatch;
  } else if (hs->new_session->has_application_settings !=
                 session->has_application_settings ||
             MakeConstSpan(hs->new_session->local_application_settings) !=
                 session->local_application_settings) {
    ssl->s3->early_data_reason = ssl_early_data_alps_mismatch;
  } else if (ssl->s3->ticket_age_skew < -kMaxTicketAgeSkewSeconds ||
             kMaxTicketAgeSkewSeconds < ssl->s3->ticket_age_skew) {
    ssl->s3->early_data_reason = ssl_early_data_ticket_age_skew;
  } else if (!quic_ticket_compatible(session.get(), hs->config)) {
    ssl->s3->early_data_reason = ssl_early_data_quic_parameter_mismatch;
  } else if (!found_key_share) {
    ssl->s3->early_data_reason = ssl_early_data_hello_retry_request;
  } else {
    // |ssl_session_is_resumable| forbids cross-cipher resumptions even if the
    // PRF hashes match.
    assert(hs->new_cipher == session->cipher);

    ssl->s3->early_data_reason = ssl_early_data_accepted;
    ssl->s3->early_data_accepted = true;
  }

  // Store the ALPN and ALPS values in the session for 0-RTT. Note the peer
  // applications settings are not generally known until client
  // EncryptedExtensions.
  if (!hs->new_session->early_alpn.CopyFrom(ssl->s3->alpn_selected)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
    return ssl_hs_error;
  }

  // The peer applications settings are usually received later, in
  // EncryptedExtensions. But, in 0-RTT handshakes, we carry over the
  // values from |session|. Do this now, before |session| is discarded.
  if (ssl->s3->early_data_accepted &&
      hs->new_session->has_application_settings &&
      !hs->new_session->peer_application_settings.CopyFrom(
          session->peer_application_settings)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
    return ssl_hs_error;
  }

  // Copy the QUIC early data context to the session.
  if (ssl->enable_early_data && ssl->quic_method) {
    if (!hs->new_session->quic_early_data_context.CopyFrom(
            hs->config->quic_early_data_context)) {
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      return ssl_hs_error;
    }
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
    if (!tls13_init_key_schedule(
            hs, MakeConstSpan(hs->new_session->secret,
                              hs->new_session->secret_length))) {
      return ssl_hs_error;
    }
  } else if (!tls13_init_key_schedule(hs, MakeConstSpan(kZeroes, hash_len))) {
    return ssl_hs_error;
  }

  if (!ssl_hash_message(hs, msg)) {
    return ssl_hs_error;
  }

  if (ssl->s3->early_data_accepted) {
    if (!tls13_derive_early_secret(hs)) {
      return ssl_hs_error;
    }
  } else if (hs->early_data_offered) {
    ssl->s3->skip_early_data = true;
  }

  if (!found_key_share) {
    ssl->method->next_message(ssl);
    if (!hs->transcript.UpdateForHelloRetryRequest()) {
      return ssl_hs_error;
    }
    hs->tls13_state = state13_send_hello_retry_request;
    return ssl_hs_ok;
  }

  if (!resolve_ecdhe_secret(hs, &client_hello)) {
    return ssl_hs_error;
  }

  ssl->method->next_message(ssl);
  hs->ech_client_hello_buf.Reset();
  hs->tls13_state = state13_send_server_hello;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_hello_retry_request(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (hs->hints_requested) {
    return ssl_hs_hints_ready;
  }

  ScopedCBB cbb;
  CBB body, session_id, extensions;
  uint16_t group_id;
  if (!ssl->method->init_message(ssl, cbb.get(), &body, SSL3_MT_SERVER_HELLO) ||
      !CBB_add_u16(&body, TLS1_2_VERSION) ||
      !CBB_add_bytes(&body, kHelloRetryRequest, SSL3_RANDOM_SIZE) ||
      !CBB_add_u8_length_prefixed(&body, &session_id) ||
      !CBB_add_bytes(&session_id, hs->session_id, hs->session_id_len) ||
      !CBB_add_u16(&body, SSL_CIPHER_get_protocol_id(hs->new_cipher)) ||
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

  ssl->s3->used_hello_retry_request = true;
  hs->tls13_state = state13_read_second_client_hello;
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
  if (!ssl_client_hello_init(ssl, &client_hello, msg.body)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CLIENTHELLO_PARSE_FAILED);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return ssl_hs_error;
  }

  if (hs->ech_accept) {
    // If we previously accepted the ClientHelloInner, check that the second
    // ClientHello contains an encrypted_client_hello extension.
    CBS ech_body;
    if (!ssl_client_hello_get_extension(&client_hello, &ech_body,
                                        TLSEXT_TYPE_encrypted_client_hello)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_MISSING_EXTENSION);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_MISSING_EXTENSION);
      return ssl_hs_error;
    }

    // Parse a ClientECH out of the extension body.
    uint16_t kdf_id, aead_id;
    uint8_t config_id;
    CBS enc, payload;
    if (!CBS_get_u16(&ech_body, &kdf_id) ||  //
        !CBS_get_u16(&ech_body, &aead_id) ||
        !CBS_get_u8(&ech_body, &config_id) ||
        !CBS_get_u16_length_prefixed(&ech_body, &enc) ||
        !CBS_get_u16_length_prefixed(&ech_body, &payload) ||
        CBS_len(&ech_body) != 0) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
      return ssl_hs_error;
    }

    // Check that ClientECH.cipher_suite is unchanged and that
    // ClientECH.enc is empty.
    if (kdf_id != EVP_HPKE_KDF_id(EVP_HPKE_CTX_kdf(hs->ech_hpke_ctx.get())) ||
        aead_id !=
            EVP_HPKE_AEAD_id(EVP_HPKE_CTX_aead(hs->ech_hpke_ctx.get())) ||
        config_id != hs->ech_config_id || CBS_len(&enc) > 0) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
      return ssl_hs_error;
    }

    // Decrypt the payload with the HPKE context from the first ClientHello.
    Array<uint8_t> encoded_client_hello_inner;
    bool unused;
    if (!ssl_client_hello_decrypt(
            hs->ech_hpke_ctx.get(), &encoded_client_hello_inner, &unused,
            &client_hello, kdf_id, aead_id, config_id, enc, payload)) {
      // Decryption failure is fatal in the second ClientHello.
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECRYPTION_FAILED);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECRYPT_ERROR);
      return ssl_hs_error;
    }

    // Recover the ClientHelloInner from the EncodedClientHelloInner.
    uint8_t alert = SSL_AD_DECODE_ERROR;
    bssl::Array<uint8_t> client_hello_inner;
    if (!ssl_decode_client_hello_inner(ssl, &alert, &client_hello_inner,
                                       encoded_client_hello_inner,
                                       &client_hello)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
      return ssl_hs_error;
    }
    hs->ech_client_hello_buf = std::move(client_hello_inner);

    // Reparse |client_hello| from the buffer owned by |hs|.
    if (!hs->GetClientHello(&msg, &client_hello)) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return ssl_hs_error;
    }
  }

  // We perform all our negotiation based on the first ClientHello (for
  // consistency with what |select_certificate_cb| observed), which is in the
  // transcript, so we can ignore most of this second one.
  //
  // We do, however, check the second PSK binder. This covers the client key
  // share, in case we ever send half-RTT data (we currently do not). It is also
  // a tricky computation, so we enforce the peer handled it correctly.
  if (ssl->s3->session_reused) {
    CBS pre_shared_key;
    if (!ssl_client_hello_get_extension(&client_hello, &pre_shared_key,
                                        TLSEXT_TYPE_pre_shared_key)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_INCONSISTENT_CLIENT_HELLO);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
      return ssl_hs_error;
    }

    CBS ticket, binders;
    uint32_t client_ticket_age;
    uint8_t alert = SSL_AD_DECODE_ERROR;
    if (!ssl_ext_pre_shared_key_parse_clienthello(
            hs, &ticket, &binders, &client_ticket_age, &alert, &client_hello,
            &pre_shared_key)) {
      ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
      return ssl_hs_error;
    }

    // Note it is important that we do not obtain a new |SSL_SESSION| from
    // |ticket|. We have already selected parameters based on the first
    // ClientHello (in the transcript) and must not switch partway through.
    if (!tls13_verify_psk_binder(hs, hs->new_session.get(), msg, &binders)) {
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECRYPT_ERROR);
      return ssl_hs_error;
    }
  }

  if (!resolve_ecdhe_secret(hs, &client_hello)) {
    return ssl_hs_error;
  }

  if (!ssl_hash_message(hs, msg)) {
    return ssl_hs_error;
  }

  // ClientHello should be the end of the flight.
  if (ssl->method->has_unprocessed_handshake_data(ssl)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
    OPENSSL_PUT_ERROR(SSL, SSL_R_EXCESS_HANDSHAKE_DATA);
    return ssl_hs_error;
  }

  ssl->method->next_message(ssl);
  hs->ech_client_hello_buf.Reset();
  hs->tls13_state = state13_send_server_hello;
  return ssl_hs_ok;
}

static bool make_server_hello(SSL_HANDSHAKE *hs, Array<uint8_t> *out) {
  SSL *const ssl = hs->ssl;
  ScopedCBB cbb;
  CBB body, extensions, session_id;
  if (!ssl->method->init_message(ssl, cbb.get(), &body, SSL3_MT_SERVER_HELLO) ||
      !CBB_add_u16(&body, TLS1_2_VERSION) ||
      !CBB_add_bytes(&body, ssl->s3->server_random,
                     sizeof(ssl->s3->server_random)) ||
      !CBB_add_u8_length_prefixed(&body, &session_id) ||
      !CBB_add_bytes(&session_id, hs->session_id, hs->session_id_len) ||
      !CBB_add_u16(&body, SSL_CIPHER_get_protocol_id(hs->new_cipher)) ||
      !CBB_add_u8(&body, 0) ||
      !CBB_add_u16_length_prefixed(&body, &extensions) ||
      !ssl_ext_pre_shared_key_add_serverhello(hs, &extensions) ||
      !ssl_ext_key_share_add_serverhello(hs, &extensions) ||
      !ssl_ext_supported_versions_add_serverhello(hs, &extensions) ||
      !ssl->method->finish_message(ssl, cbb.get(), out)) {
    return false;
  }
  return true;
}

static enum ssl_hs_wait_t do_send_server_hello(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;

  Span<uint8_t> random(ssl->s3->server_random);

  SSL_HANDSHAKE_HINTS *const hints = hs->hints.get();
  if (hints && !hs->hints_requested &&
      hints->server_random.size() == random.size()) {
    OPENSSL_memcpy(random.data(), hints->server_random.data(), random.size());
  } else {
    RAND_bytes(random.data(), random.size());
    if (hints && hs->hints_requested &&
        !hints->server_random.CopyFrom(random)) {
      return ssl_hs_error;
    }
  }

  assert(!hs->ech_accept || hs->ech_is_inner_present);

  if (hs->ech_is_inner_present) {
    // Construct the ServerHelloECHConf message, which is the same as
    // ServerHello, except the last 8 bytes of its random field are zeroed out.
    Span<uint8_t> random_suffix = random.subspan(24);
    OPENSSL_memset(random_suffix.data(), 0, random_suffix.size());

    Array<uint8_t> server_hello_ech_conf;
    if (!make_server_hello(hs, &server_hello_ech_conf) ||
        !tls13_ech_accept_confirmation(hs, random_suffix,
                                       server_hello_ech_conf)) {
      return ssl_hs_error;
    }
  }

  Array<uint8_t> server_hello;
  if (!make_server_hello(hs, &server_hello) ||
      !ssl->method->add_message(ssl, std::move(server_hello))) {
    return ssl_hs_error;
  }

  hs->ecdh_public_key.Reset();  // No longer needed.
  if (!ssl->s3->used_hello_retry_request &&
      !ssl->method->add_change_cipher_spec(ssl)) {
    return ssl_hs_error;
  }

  // Derive and enable the handshake traffic secrets.
  if (!tls13_derive_handshake_secrets(hs) ||
      !tls13_set_traffic_key(ssl, ssl_encryption_handshake, evp_aead_seal,
                             hs->new_session.get(),
                             hs->server_handshake_secret())) {
    return ssl_hs_error;
  }

  // Send EncryptedExtensions.
  ScopedCBB cbb;
  CBB body;
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
        !tls12_add_verify_sigalgs(hs, &sigalgs_cbb)) {
      return ssl_hs_error;
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
    if (!ssl_has_certificate(hs)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_NO_CERTIFICATE_SET);
      return ssl_hs_error;
    }

    if (!tls13_add_certificate(hs)) {
      return ssl_hs_error;
    }

    hs->tls13_state = state13_send_server_certificate_verify;
    return ssl_hs_ok;
  }

  hs->tls13_state = state13_send_server_finished;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_server_certificate_verify(SSL_HANDSHAKE *hs) {
  switch (tls13_add_certificate_verify(hs)) {
    case ssl_private_key_success:
      hs->tls13_state = state13_send_server_finished;
      return ssl_hs_ok;

    case ssl_private_key_retry:
      hs->tls13_state = state13_send_server_certificate_verify;
      return ssl_hs_private_key_operation;

    case ssl_private_key_failure:
      return ssl_hs_error;
  }

  assert(0);
  return ssl_hs_error;
}

static enum ssl_hs_wait_t do_send_server_finished(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (hs->hints_requested) {
    return ssl_hs_hints_ready;
  }

  hs->can_release_private_key = true;
  if (!tls13_add_finished(hs) ||
      // Update the secret to the master secret and derive traffic keys.
      !tls13_advance_key_schedule(
          hs, MakeConstSpan(kZeroes, hs->transcript.DigestLen())) ||
      !tls13_derive_application_secrets(hs) ||
      !tls13_set_traffic_key(ssl, ssl_encryption_application, evp_aead_seal,
                             hs->new_session.get(),
                             hs->server_traffic_secret_0())) {
    return ssl_hs_error;
  }

  hs->tls13_state = state13_send_half_rtt_ticket;
  return hs->handback ? ssl_hs_handback : ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_half_rtt_ticket(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;

  if (ssl->s3->early_data_accepted) {
    // If accepting 0-RTT, we send tickets half-RTT. This gets the tickets on
    // the wire sooner and also avoids triggering a write on |SSL_read| when
    // processing the client Finished. This requires computing the client
    // Finished early. See RFC 8446, section 4.6.1.
    static const uint8_t kEndOfEarlyData[4] = {SSL3_MT_END_OF_EARLY_DATA, 0,
                                               0, 0};
    if (ssl->quic_method == nullptr &&
        !hs->transcript.Update(kEndOfEarlyData)) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return ssl_hs_error;
    }

    size_t finished_len;
    if (!tls13_finished_mac(hs, hs->expected_client_finished().data(),
                            &finished_len, false /* client */)) {
      return ssl_hs_error;
    }

    if (finished_len != hs->expected_client_finished().size()) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return ssl_hs_error;
    }

    // Feed the predicted Finished into the transcript. This allows us to derive
    // the resumption secret early and send half-RTT tickets.
    //
    // TODO(davidben): This will need to be updated for DTLS 1.3.
    assert(!SSL_is_dtls(hs->ssl));
    assert(hs->expected_client_finished().size() <= 0xff);
    uint8_t header[4] = {
        SSL3_MT_FINISHED, 0, 0,
        static_cast<uint8_t>(hs->expected_client_finished().size())};
    bool unused_sent_tickets;
    if (!hs->transcript.Update(header) ||
        !hs->transcript.Update(hs->expected_client_finished()) ||
        !tls13_derive_resumption_secret(hs) ||
        !add_new_session_tickets(hs, &unused_sent_tickets)) {
      return ssl_hs_error;
    }
  }

  hs->tls13_state = state13_read_second_client_flight;
  return ssl_hs_flush;
}

static enum ssl_hs_wait_t do_read_second_client_flight(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (ssl->s3->early_data_accepted) {
    if (!tls13_set_traffic_key(ssl, ssl_encryption_early_data, evp_aead_open,
                               hs->new_session.get(),
                               hs->early_traffic_secret())) {
      return ssl_hs_error;
    }
    hs->can_early_write = true;
    hs->can_early_read = true;
    hs->in_early_data = true;
  }

  // QUIC doesn't use an EndOfEarlyData message (draft-ietf-quic-tls-22,
  // section 8.3), so we switch to client_handshake_secret before the early
  // return.
  if (ssl->quic_method != nullptr) {
    if (!tls13_set_traffic_key(ssl, ssl_encryption_handshake, evp_aead_open,
                               hs->new_session.get(),
                               hs->client_handshake_secret())) {
      return ssl_hs_error;
    }
    hs->tls13_state = state13_process_end_of_early_data;
    return ssl->s3->early_data_accepted ? ssl_hs_early_return : ssl_hs_ok;
  }

  hs->tls13_state = state13_process_end_of_early_data;
  return ssl->s3->early_data_accepted ? ssl_hs_read_end_of_early_data
                                      : ssl_hs_ok;
}

static enum ssl_hs_wait_t do_process_end_of_early_data(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  // In protocols that use EndOfEarlyData, we must consume the extra message and
  // switch to client_handshake_secret after the early return.
  if (ssl->quic_method == nullptr) {
    // If early data was not accepted, the EndOfEarlyData will be in the
    // discarded early data.
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
    if (!tls13_set_traffic_key(ssl, ssl_encryption_handshake, evp_aead_open,
                               hs->new_session.get(),
                               hs->client_handshake_secret())) {
      return ssl_hs_error;
    }
  }
  hs->tls13_state = state13_read_client_encrypted_extensions;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_read_client_encrypted_extensions(
    SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  // For now, only one extension uses client EncryptedExtensions. This function
  // may be generalized if others use it in the future.
  if (hs->new_session->has_application_settings &&
      !ssl->s3->early_data_accepted) {
    SSLMessage msg;
    if (!ssl->method->get_message(ssl, &msg)) {
      return ssl_hs_read_message;
    }
    if (!ssl_check_message_type(ssl, msg, SSL3_MT_ENCRYPTED_EXTENSIONS)) {
      return ssl_hs_error;
    }

    CBS body = msg.body, extensions;
    if (!CBS_get_u16_length_prefixed(&body, &extensions) ||
        CBS_len(&body) != 0) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
      return ssl_hs_error;
    }

    // Parse out the extensions.
    bool have_application_settings = false;
    CBS application_settings;
    SSL_EXTENSION_TYPE ext_types[] = {{TLSEXT_TYPE_application_settings,
                                       &have_application_settings,
                                       &application_settings}};
    uint8_t alert = SSL_AD_DECODE_ERROR;
    if (!ssl_parse_extensions(&extensions, &alert, ext_types,
                              /*ignore_unknown=*/false)) {
      ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
      return ssl_hs_error;
    }

    if (!have_application_settings) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_MISSING_EXTENSION);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_MISSING_EXTENSION);
      return ssl_hs_error;
    }

    // Note that, if 0-RTT was accepted, these values will already have been
    // initialized earlier.
    if (!hs->new_session->peer_application_settings.CopyFrom(
            application_settings) ||
        !ssl_hash_message(hs, msg)) {
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      return ssl_hs_error;
    }

    ssl->method->next_message(ssl);
  }

  hs->tls13_state = state13_read_client_certificate;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_read_client_certificate(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (!hs->cert_request) {
    if (!ssl->s3->session_reused) {
      // OpenSSL returns X509_V_OK when no certificates are requested. This is
      // classed by them as a bug, but it's assumed by at least NGINX. (Only do
      // this in full handshakes as resumptions should carry over the previous
      // |verify_result|, though this is a no-op because servers do not
      // implement the client's odd soft-fail mode.)
      hs->new_session->verify_result = X509_V_OK;
    }

    // Skip this state.
    hs->tls13_state = state13_read_channel_id;
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
  hs->tls13_state = state13_read_client_certificate_verify;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_read_client_certificate_verify(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (sk_CRYPTO_BUFFER_num(hs->new_session->certs.get()) == 0) {
    // Skip this state.
    hs->tls13_state = state13_read_channel_id;
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
      hs->tls13_state = state13_read_client_certificate_verify;
      return ssl_hs_certificate_verify;
  }

  if (!ssl_check_message_type(ssl, msg, SSL3_MT_CERTIFICATE_VERIFY) ||
      !tls13_process_certificate_verify(hs, msg) ||
      !ssl_hash_message(hs, msg)) {
    return ssl_hs_error;
  }

  ssl->method->next_message(ssl);
  hs->tls13_state = state13_read_channel_id;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_read_channel_id(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (!ssl->s3->channel_id_valid) {
    hs->tls13_state = state13_read_client_finished;
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
  hs->tls13_state = state13_read_client_finished;
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
                             hs->new_session.get(),
                             hs->client_traffic_secret_0())) {
    return ssl_hs_error;
  }

  if (!ssl->s3->early_data_accepted) {
    if (!ssl_hash_message(hs, msg) ||
        !tls13_derive_resumption_secret(hs)) {
      return ssl_hs_error;
    }

    // We send post-handshake tickets as part of the handshake in 1-RTT.
    hs->tls13_state = state13_send_new_session_ticket;
  } else {
    // We already sent half-RTT tickets.
    hs->tls13_state = state13_done;
  }

  ssl->method->next_message(ssl);
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_new_session_ticket(SSL_HANDSHAKE *hs) {
  bool sent_tickets;
  if (!add_new_session_tickets(hs, &sent_tickets)) {
    return ssl_hs_error;
  }

  hs->tls13_state = state13_done;
  // In TLS 1.3, the NewSessionTicket isn't flushed until the server performs a
  // write, to prevent a non-reading client from causing the server to hang in
  // the case of a small server write buffer. Consumers which don't write data
  // to the client will need to do a zero-byte write if they wish to flush the
  // tickets.
  if (hs->ssl->quic_method != nullptr && sent_tickets) {
    return ssl_hs_flush;
  }
  return ssl_hs_ok;
}

enum ssl_hs_wait_t tls13_server_handshake(SSL_HANDSHAKE *hs) {
  while (hs->tls13_state != state13_done) {
    enum ssl_hs_wait_t ret = ssl_hs_error;
    enum tls13_server_hs_state_t state =
        static_cast<enum tls13_server_hs_state_t>(hs->tls13_state);
    switch (state) {
      case state13_select_parameters:
        ret = do_select_parameters(hs);
        break;
      case state13_select_session:
        ret = do_select_session(hs);
        break;
      case state13_send_hello_retry_request:
        ret = do_send_hello_retry_request(hs);
        break;
      case state13_read_second_client_hello:
        ret = do_read_second_client_hello(hs);
        break;
      case state13_send_server_hello:
        ret = do_send_server_hello(hs);
        break;
      case state13_send_server_certificate_verify:
        ret = do_send_server_certificate_verify(hs);
        break;
      case state13_send_server_finished:
        ret = do_send_server_finished(hs);
        break;
      case state13_send_half_rtt_ticket:
        ret = do_send_half_rtt_ticket(hs);
        break;
      case state13_read_second_client_flight:
        ret = do_read_second_client_flight(hs);
        break;
      case state13_process_end_of_early_data:
        ret = do_process_end_of_early_data(hs);
        break;
      case state13_read_client_encrypted_extensions:
        ret = do_read_client_encrypted_extensions(hs);
        break;
      case state13_read_client_certificate:
        ret = do_read_client_certificate(hs);
        break;
      case state13_read_client_certificate_verify:
        ret = do_read_client_certificate_verify(hs);
        break;
      case state13_read_channel_id:
        ret = do_read_channel_id(hs);
        break;
      case state13_read_client_finished:
        ret = do_read_client_finished(hs);
        break;
      case state13_send_new_session_ticket:
        ret = do_send_new_session_ticket(hs);
        break;
      case state13_done:
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
  enum tls13_server_hs_state_t state =
      static_cast<enum tls13_server_hs_state_t>(hs->tls13_state);
  switch (state) {
    case state13_select_parameters:
      return "TLS 1.3 server select_parameters";
    case state13_select_session:
      return "TLS 1.3 server select_session";
    case state13_send_hello_retry_request:
      return "TLS 1.3 server send_hello_retry_request";
    case state13_read_second_client_hello:
      return "TLS 1.3 server read_second_client_hello";
    case state13_send_server_hello:
      return "TLS 1.3 server send_server_hello";
    case state13_send_server_certificate_verify:
      return "TLS 1.3 server send_server_certificate_verify";
    case state13_send_half_rtt_ticket:
      return "TLS 1.3 server send_half_rtt_ticket";
    case state13_send_server_finished:
      return "TLS 1.3 server send_server_finished";
    case state13_read_second_client_flight:
      return "TLS 1.3 server read_second_client_flight";
    case state13_process_end_of_early_data:
      return "TLS 1.3 server process_end_of_early_data";
    case state13_read_client_encrypted_extensions:
      return "TLS 1.3 server read_client_encrypted_extensions";
    case state13_read_client_certificate:
      return "TLS 1.3 server read_client_certificate";
    case state13_read_client_certificate_verify:
      return "TLS 1.3 server read_client_certificate_verify";
    case state13_read_channel_id:
      return "TLS 1.3 server read_channel_id";
    case state13_read_client_finished:
      return "TLS 1.3 server read_client_finished";
    case state13_send_new_session_ticket:
      return "TLS 1.3 server send_new_session_ticket";
    case state13_done:
      return "TLS 1.3 server done";
  }

  return "TLS 1.3 server unknown";
}

BSSL_NAMESPACE_END
