/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2002 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com). */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 * ECC cipher suite support in OpenSSL originally developed by
 * SUN MICROSYSTEMS, INC., and contributed to the OpenSSL project. */

#include <openssl/ssl.h>

#include <assert.h>

#include <utility>

#include <openssl/rand.h>

#include "../crypto/internal.h"
#include "internal.h"


BSSL_NAMESPACE_BEGIN

SSL_HANDSHAKE::SSL_HANDSHAKE(SSL *ssl_arg)
    : ssl(ssl_arg),
      ech_accept(false),
      ech_present(false),
      ech_is_inner_present(false),
      scts_requested(false),
      needs_psk_binder(false),
      handshake_finalized(false),
      accept_psk_mode(false),
      cert_request(false),
      certificate_status_expected(false),
      ocsp_stapling_requested(false),
      delegated_credential_requested(false),
      should_ack_sni(false),
      in_false_start(false),
      in_early_data(false),
      early_data_offered(false),
      can_early_read(false),
      can_early_write(false),
      next_proto_neg_seen(false),
      ticket_expected(false),
      extended_master_secret(false),
      pending_private_key_op(false),
      grease_seeded(false),
      handback(false),
      hints_requested(false),
      cert_compression_negotiated(false),
      apply_jdk11_workaround(false),
      can_release_private_key(false) {
  assert(ssl);
}

SSL_HANDSHAKE::~SSL_HANDSHAKE() {
  ssl->ctx->x509_method->hs_flush_cached_ca_names(this);
}

void SSL_HANDSHAKE::ResizeSecrets(size_t hash_len) {
  if (hash_len > SSL_MAX_MD_SIZE) {
    abort();
  }
  hash_len_ = hash_len;
}

bool SSL_HANDSHAKE::GetClientHello(SSLMessage *out_msg,
                                   SSL_CLIENT_HELLO *out_client_hello) {
  if (!ech_client_hello_buf.empty()) {
    // If the backing buffer is non-empty, the ClientHelloInner has been set.
    out_msg->is_v2_hello = false;
    out_msg->type = SSL3_MT_CLIENT_HELLO;
    out_msg->raw = CBS(ech_client_hello_buf);
    out_msg->body = MakeConstSpan(ech_client_hello_buf).subspan(4);
  } else if (!ssl->method->get_message(ssl, out_msg)) {
    // The message has already been read, so this cannot fail.
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  if (!ssl_client_hello_init(ssl, out_client_hello, out_msg->body)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CLIENTHELLO_PARSE_FAILED);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return false;
  }
  return true;
}

UniquePtr<SSL_HANDSHAKE> ssl_handshake_new(SSL *ssl) {
  UniquePtr<SSL_HANDSHAKE> hs = MakeUnique<SSL_HANDSHAKE>(ssl);
  if (!hs || !hs->transcript.Init()) {
    return nullptr;
  }
  hs->config = ssl->config.get();
  if (!hs->config) {
    assert(hs->config);
    return nullptr;
  }
  return hs;
}

bool ssl_check_message_type(SSL *ssl, const SSLMessage &msg, int type) {
  if (msg.type != type) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_MESSAGE);
    ERR_add_error_dataf("got type %d, wanted type %d", msg.type, type);
    return false;
  }

  return true;
}

bool ssl_add_message_cbb(SSL *ssl, CBB *cbb) {
  Array<uint8_t> msg;
  if (!ssl->method->finish_message(ssl, cbb, &msg) ||
      !ssl->method->add_message(ssl, std::move(msg))) {
    return false;
  }

  return true;
}

size_t ssl_max_handshake_message_len(const SSL *ssl) {
  // kMaxMessageLen is the default maximum message size for handshakes which do
  // not accept peer certificate chains.
  static const size_t kMaxMessageLen = 16384;

  if (SSL_in_init(ssl)) {
    SSL_CONFIG *config = ssl->config.get();  // SSL_in_init() implies not NULL.
    if ((!ssl->server || (config->verify_mode & SSL_VERIFY_PEER)) &&
        kMaxMessageLen < ssl->max_cert_list) {
      return ssl->max_cert_list;
    }
    return kMaxMessageLen;
  }

  if (ssl_protocol_version(ssl) < TLS1_3_VERSION) {
    // In TLS 1.2 and below, the largest acceptable post-handshake message is
    // a HelloRequest.
    return 0;
  }

  if (ssl->server) {
    // The largest acceptable post-handshake message for a server is a
    // KeyUpdate. We will never initiate post-handshake auth.
    return 1;
  }

  // Clients must accept NewSessionTicket, so allow the default size.
  return kMaxMessageLen;
}

bool ssl_hash_message(SSL_HANDSHAKE *hs, const SSLMessage &msg) {
  // V2ClientHello messages are pre-hashed.
  if (msg.is_v2_hello) {
    return true;
  }

  return hs->transcript.Update(msg.raw);
}

bool ssl_parse_extensions(const CBS *cbs, uint8_t *out_alert,
                          Span<const SSL_EXTENSION_TYPE> ext_types,
                          bool ignore_unknown) {
  // Reset everything.
  for (const SSL_EXTENSION_TYPE &ext_type : ext_types) {
    *ext_type.out_present = false;
    CBS_init(ext_type.out_data, nullptr, 0);
  }

  CBS copy = *cbs;
  while (CBS_len(&copy) != 0) {
    uint16_t type;
    CBS data;
    if (!CBS_get_u16(&copy, &type) ||
        !CBS_get_u16_length_prefixed(&copy, &data)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_PARSE_TLSEXT);
      *out_alert = SSL_AD_DECODE_ERROR;
      return false;
    }

    const SSL_EXTENSION_TYPE *found = nullptr;
    for (const SSL_EXTENSION_TYPE &ext_type : ext_types) {
      if (type == ext_type.type) {
        found = &ext_type;
        break;
      }
    }

    if (found == nullptr) {
      if (ignore_unknown) {
        continue;
      }
      OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_EXTENSION);
      *out_alert = SSL_AD_UNSUPPORTED_EXTENSION;
      return false;
    }

    // Duplicate ext_types are forbidden.
    if (*found->out_present) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DUPLICATE_EXTENSION);
      *out_alert = SSL_AD_ILLEGAL_PARAMETER;
      return false;
    }

    *found->out_present = 1;
    *found->out_data = data;
  }

  return true;
}

enum ssl_verify_result_t ssl_verify_peer_cert(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  const SSL_SESSION *prev_session = ssl->s3->established_session.get();
  if (prev_session != NULL) {
    // If renegotiating, the server must not change the server certificate. See
    // https://mitls.org/pages/attacks/3SHAKE. We never resume on renegotiation,
    // so this check is sufficient to ensure the reported peer certificate never
    // changes on renegotiation.
    assert(!ssl->server);
    if (sk_CRYPTO_BUFFER_num(prev_session->certs.get()) !=
        sk_CRYPTO_BUFFER_num(hs->new_session->certs.get())) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_SERVER_CERT_CHANGED);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
      return ssl_verify_invalid;
    }

    for (size_t i = 0; i < sk_CRYPTO_BUFFER_num(hs->new_session->certs.get());
         i++) {
      const CRYPTO_BUFFER *old_cert =
          sk_CRYPTO_BUFFER_value(prev_session->certs.get(), i);
      const CRYPTO_BUFFER *new_cert =
          sk_CRYPTO_BUFFER_value(hs->new_session->certs.get(), i);
      if (CRYPTO_BUFFER_len(old_cert) != CRYPTO_BUFFER_len(new_cert) ||
          OPENSSL_memcmp(CRYPTO_BUFFER_data(old_cert),
                         CRYPTO_BUFFER_data(new_cert),
                         CRYPTO_BUFFER_len(old_cert)) != 0) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_SERVER_CERT_CHANGED);
        ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
        return ssl_verify_invalid;
      }
    }

    // The certificate is identical, so we may skip re-verifying the
    // certificate. Since we only authenticated the previous one, copy other
    // authentication from the established session and ignore what was newly
    // received.
    hs->new_session->ocsp_response = UpRef(prev_session->ocsp_response);
    hs->new_session->signed_cert_timestamp_list =
        UpRef(prev_session->signed_cert_timestamp_list);
    hs->new_session->verify_result = prev_session->verify_result;
    return ssl_verify_ok;
  }

  uint8_t alert = SSL_AD_CERTIFICATE_UNKNOWN;
  enum ssl_verify_result_t ret;
  if (hs->config->custom_verify_callback != nullptr) {
    ret = hs->config->custom_verify_callback(ssl, &alert);
    switch (ret) {
      case ssl_verify_ok:
        hs->new_session->verify_result = X509_V_OK;
        break;
      case ssl_verify_invalid:
        // If |SSL_VERIFY_NONE|, the error is non-fatal, but we keep the result.
        if (hs->config->verify_mode == SSL_VERIFY_NONE) {
          ERR_clear_error();
          ret = ssl_verify_ok;
        }
        hs->new_session->verify_result = X509_V_ERR_APPLICATION_VERIFICATION;
        break;
      case ssl_verify_retry:
        break;
    }
  } else {
    ret = ssl->ctx->x509_method->session_verify_cert_chain(
              hs->new_session.get(), hs, &alert)
              ? ssl_verify_ok
              : ssl_verify_invalid;
  }

  if (ret == ssl_verify_invalid) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CERTIFICATE_VERIFY_FAILED);
    ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
  }

  // Emulate OpenSSL's client OCSP callback. OpenSSL verifies certificates
  // before it receives the OCSP, so it needs a second callback for OCSP.
  if (ret == ssl_verify_ok && !ssl->server &&
      hs->config->ocsp_stapling_enabled &&
      ssl->ctx->legacy_ocsp_callback != nullptr) {
    int cb_ret =
        ssl->ctx->legacy_ocsp_callback(ssl, ssl->ctx->legacy_ocsp_callback_arg);
    if (cb_ret <= 0) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_OCSP_CB_ERROR);
      ssl_send_alert(ssl, SSL3_AL_FATAL,
                     cb_ret == 0 ? SSL_AD_BAD_CERTIFICATE_STATUS_RESPONSE
                                 : SSL_AD_INTERNAL_ERROR);
      ret = ssl_verify_invalid;
    }
  }

  return ret;
}

// Verifies a stored certificate when resuming a session. A few things are
// different from verify_peer_cert:
// 1. We can't be renegotiating if we're resuming a session.
// 2. The session is immutable, so we don't support verify_mode ==
// SSL_VERIFY_NONE
// 3. We don't call the OCSP callback.
// 4. We only support custom verify callbacks.
enum ssl_verify_result_t ssl_reverify_peer_cert(SSL_HANDSHAKE *hs,
                                                bool send_alert) {
  SSL *const ssl = hs->ssl;
  assert(ssl->s3->established_session == nullptr);
  assert(hs->config->verify_mode != SSL_VERIFY_NONE);

  uint8_t alert = SSL_AD_CERTIFICATE_UNKNOWN;
  enum ssl_verify_result_t ret = ssl_verify_invalid;
  if (hs->config->custom_verify_callback != nullptr) {
    ret = hs->config->custom_verify_callback(ssl, &alert);
  }

  if (ret == ssl_verify_invalid) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CERTIFICATE_VERIFY_FAILED);
    if (send_alert) {
      ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
    }
  }

  return ret;
}

uint16_t ssl_get_grease_value(SSL_HANDSHAKE *hs,
                              enum ssl_grease_index_t index) {
  // Draw entropy for all GREASE values at once. This avoids calling
  // |RAND_bytes| repeatedly and makes the values consistent within a
  // connection. The latter is so the second ClientHello matches after
  // HelloRetryRequest and so supported_groups and key_shares are consistent.
  if (!hs->grease_seeded) {
    RAND_bytes(hs->grease_seed, sizeof(hs->grease_seed));
    hs->grease_seeded = true;
  }

  // This generates a random value of the form 0xωaωa, for all 0 ≤ ω < 16.
  uint16_t ret = hs->grease_seed[index];
  ret = (ret & 0xf0) | 0x0a;
  ret |= ret << 8;
  return ret;
}

enum ssl_hs_wait_t ssl_get_finished(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  SSLMessage msg;
  if (!ssl->method->get_message(ssl, &msg)) {
    return ssl_hs_read_message;
  }

  if (!ssl_check_message_type(ssl, msg, SSL3_MT_FINISHED)) {
    return ssl_hs_error;
  }

  // Snapshot the finished hash before incorporating the new message.
  uint8_t finished[EVP_MAX_MD_SIZE];
  size_t finished_len;
  if (!hs->transcript.GetFinishedMAC(finished, &finished_len,
                                     ssl_handshake_session(hs), !ssl->server) ||
      !ssl_hash_message(hs, msg)) {
    return ssl_hs_error;
  }

  int finished_ok = CBS_mem_equal(&msg.body, finished, finished_len);
#if defined(BORINGSSL_UNSAFE_FUZZER_MODE)
  finished_ok = 1;
#endif
  if (!finished_ok) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECRYPT_ERROR);
    OPENSSL_PUT_ERROR(SSL, SSL_R_DIGEST_CHECK_FAILED);
    return ssl_hs_error;
  }

  // Copy the Finished so we can use it for renegotiation checks.
  if (finished_len > sizeof(ssl->s3->previous_client_finished) ||
      finished_len > sizeof(ssl->s3->previous_server_finished)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return ssl_hs_error;
  }

  if (ssl->server) {
    OPENSSL_memcpy(ssl->s3->previous_client_finished, finished, finished_len);
    ssl->s3->previous_client_finished_len = finished_len;
  } else {
    OPENSSL_memcpy(ssl->s3->previous_server_finished, finished, finished_len);
    ssl->s3->previous_server_finished_len = finished_len;
  }

  // The Finished message should be the end of a flight.
  if (ssl->method->has_unprocessed_handshake_data(ssl)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
    OPENSSL_PUT_ERROR(SSL, SSL_R_EXCESS_HANDSHAKE_DATA);
    return ssl_hs_error;
  }

  ssl->method->next_message(ssl);
  return ssl_hs_ok;
}

bool ssl_send_finished(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  const SSL_SESSION *session = ssl_handshake_session(hs);

  uint8_t finished[EVP_MAX_MD_SIZE];
  size_t finished_len;
  if (!hs->transcript.GetFinishedMAC(finished, &finished_len, session,
                                     ssl->server)) {
    return 0;
  }

  // Log the master secret, if logging is enabled.
  if (!ssl_log_secret(ssl, "CLIENT_RANDOM",
                      MakeConstSpan(session->secret, session->secret_length))) {
    return 0;
  }

  // Copy the Finished so we can use it for renegotiation checks.
  if (finished_len > sizeof(ssl->s3->previous_client_finished) ||
      finished_len > sizeof(ssl->s3->previous_server_finished)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  if (ssl->server) {
    OPENSSL_memcpy(ssl->s3->previous_server_finished, finished, finished_len);
    ssl->s3->previous_server_finished_len = finished_len;
  } else {
    OPENSSL_memcpy(ssl->s3->previous_client_finished, finished, finished_len);
    ssl->s3->previous_client_finished_len = finished_len;
  }

  ScopedCBB cbb;
  CBB body;
  if (!ssl->method->init_message(ssl, cbb.get(), &body, SSL3_MT_FINISHED) ||
      !CBB_add_bytes(&body, finished, finished_len) ||
      !ssl_add_message_cbb(ssl, cbb.get())) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  return 1;
}

bool ssl_output_cert_chain(SSL_HANDSHAKE *hs) {
  ScopedCBB cbb;
  CBB body;
  if (!hs->ssl->method->init_message(hs->ssl, cbb.get(), &body,
                                     SSL3_MT_CERTIFICATE) ||
      !ssl_add_cert_chain(hs, &body) ||
      !ssl_add_message_cbb(hs->ssl, cbb.get())) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  return true;
}

const SSL_SESSION *ssl_handshake_session(const SSL_HANDSHAKE *hs) {
  if (hs->new_session) {
    return hs->new_session.get();
  }
  return hs->ssl->session.get();
}

int ssl_run_handshake(SSL_HANDSHAKE *hs, bool *out_early_return) {
  SSL *const ssl = hs->ssl;
  for (;;) {
    // Resolve the operation the handshake was waiting on. Each condition may
    // halt the handshake by returning, or continue executing if the handshake
    // may immediately proceed. Cases which halt the handshake can clear
    // |hs->wait| to re-enter the state machine on the next iteration, or leave
    // it set to keep the condition sticky.
    switch (hs->wait) {
      case ssl_hs_error:
        ERR_restore_state(hs->error.get());
        return -1;

      case ssl_hs_flush: {
        int ret = ssl->method->flush_flight(ssl);
        if (ret <= 0) {
          return ret;
        }
        break;
      }

      case ssl_hs_read_server_hello:
      case ssl_hs_read_message:
      case ssl_hs_read_change_cipher_spec: {
        if (ssl->quic_method) {
          // QUIC has no ChangeCipherSpec messages.
          assert(hs->wait != ssl_hs_read_change_cipher_spec);
          // The caller should call |SSL_provide_quic_data|. Clear |hs->wait| so
          // the handshake can check if there is sufficient data next iteration.
          ssl->s3->rwstate = SSL_ERROR_WANT_READ;
          hs->wait = ssl_hs_ok;
          return -1;
        }

        uint8_t alert = SSL_AD_DECODE_ERROR;
        size_t consumed = 0;
        ssl_open_record_t ret;
        if (hs->wait == ssl_hs_read_change_cipher_spec) {
          ret = ssl_open_change_cipher_spec(ssl, &consumed, &alert,
                                            ssl->s3->read_buffer.span());
        } else {
          ret = ssl_open_handshake(ssl, &consumed, &alert,
                                   ssl->s3->read_buffer.span());
        }
        if (ret == ssl_open_record_error &&
            hs->wait == ssl_hs_read_server_hello) {
          uint32_t err = ERR_peek_error();
          if (ERR_GET_LIB(err) == ERR_LIB_SSL &&
              ERR_GET_REASON(err) == SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE) {
            // Add a dedicated error code to the queue for a handshake_failure
            // alert in response to ClientHello. This matches NSS's client
            // behavior and gives a better error on a (probable) failure to
            // negotiate initial parameters. Note: this error code comes after
            // the original one.
            //
            // See https://crbug.com/446505.
            OPENSSL_PUT_ERROR(SSL, SSL_R_HANDSHAKE_FAILURE_ON_CLIENT_HELLO);
          }
        }
        bool retry;
        int bio_ret = ssl_handle_open_record(ssl, &retry, ret, consumed, alert);
        if (bio_ret <= 0) {
          return bio_ret;
        }
        if (retry) {
          continue;
        }
        ssl->s3->read_buffer.DiscardConsumed();
        break;
      }

      case ssl_hs_read_end_of_early_data: {
        if (ssl->s3->hs->can_early_read) {
          // While we are processing early data, the handshake returns early.
          *out_early_return = true;
          return 1;
        }
        hs->wait = ssl_hs_ok;
        break;
      }

      case ssl_hs_certificate_selection_pending:
        ssl->s3->rwstate = SSL_ERROR_PENDING_CERTIFICATE;
        hs->wait = ssl_hs_ok;
        return -1;

      case ssl_hs_handoff:
        ssl->s3->rwstate = SSL_ERROR_HANDOFF;
        hs->wait = ssl_hs_ok;
        return -1;

      case ssl_hs_handback: {
        int ret = ssl->method->flush_flight(ssl);
        if (ret <= 0) {
          return ret;
        }
        ssl->s3->rwstate = SSL_ERROR_HANDBACK;
        hs->wait = ssl_hs_handback;
        return -1;
      }

        // The following cases are associated with callback APIs which expect to
        // be called each time the state machine runs. Thus they set |hs->wait|
        // to |ssl_hs_ok| so that, next time, we re-enter the state machine and
        // call the callback again.
      case ssl_hs_x509_lookup:
        ssl->s3->rwstate = SSL_ERROR_WANT_X509_LOOKUP;
        hs->wait = ssl_hs_ok;
        return -1;
      case ssl_hs_channel_id_lookup:
        ssl->s3->rwstate = SSL_ERROR_WANT_CHANNEL_ID_LOOKUP;
        hs->wait = ssl_hs_ok;
        return -1;
      case ssl_hs_private_key_operation:
        ssl->s3->rwstate = SSL_ERROR_WANT_PRIVATE_KEY_OPERATION;
        hs->wait = ssl_hs_ok;
        return -1;
      case ssl_hs_pending_session:
        ssl->s3->rwstate = SSL_ERROR_PENDING_SESSION;
        hs->wait = ssl_hs_ok;
        return -1;
      case ssl_hs_pending_ticket:
        ssl->s3->rwstate = SSL_ERROR_PENDING_TICKET;
        hs->wait = ssl_hs_ok;
        return -1;
      case ssl_hs_certificate_verify:
        ssl->s3->rwstate = SSL_ERROR_WANT_CERTIFICATE_VERIFY;
        hs->wait = ssl_hs_ok;
        return -1;

      case ssl_hs_early_data_rejected:
        assert(ssl->s3->early_data_reason != ssl_early_data_unknown);
        assert(!hs->can_early_write);
        ssl->s3->rwstate = SSL_ERROR_EARLY_DATA_REJECTED;
        return -1;

      case ssl_hs_early_return:
        *out_early_return = true;
        hs->wait = ssl_hs_ok;
        return 1;

      case ssl_hs_hints_ready:
        ssl->s3->rwstate = SSL_ERROR_HANDSHAKE_HINTS_READY;
        return -1;

      case ssl_hs_ok:
        break;
    }

    // Run the state machine again.
    hs->wait = ssl->do_handshake(hs);
    if (hs->wait == ssl_hs_error) {
      hs->error.reset(ERR_save_state());
      return -1;
    }
    if (hs->wait == ssl_hs_ok) {
      // The handshake has completed.
      *out_early_return = false;
      return 1;
    }

    // Otherwise, loop to the beginning and resolve what was blocking the
    // handshake.
  }
}

BSSL_NAMESPACE_END
