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
 * Copyright (c) 1998-2006 The OpenSSL Project.  All rights reserved.
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
 * Hudson (tjh@cryptsoft.com).
 *
 */
/* ====================================================================
 * Copyright 2005 Nokia. All rights reserved.
 *
 * The portions of the attached software ("Contribution") is developed by
 * Nokia Corporation and is licensed pursuant to the OpenSSL open source
 * license.
 *
 * The Contribution, originally written by Mika Kousa and Pasi Eronen of
 * Nokia Corporation, consists of the "PSK" (Pre-Shared Key) ciphersuites
 * support (see RFC 4279) to OpenSSL.
 *
 * No patent licenses or other rights except those expressly stated in
 * the OpenSSL open source license shall be deemed granted or received
 * expressly, by implication, estoppel, or otherwise.
 *
 * No assurances are provided by Nokia that the Contribution does not
 * infringe the patent or other intellectual property rights of any third
 * party or that the license provides you with all the necessary rights
 * to make use of the Contribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. IN
 * ADDITION TO THE DISCLAIMERS INCLUDED IN THE LICENSE, NOKIA
 * SPECIFICALLY DISCLAIMS ANY LIABILITY FOR CLAIMS BROUGHT BY YOU OR ANY
 * OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS OR
 * OTHERWISE. */

#include <openssl/ssl.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <utility>

#include <openssl/err.h>
#include <openssl/hmac.h>
#include <openssl/lhash.h>
#include <openssl/mem.h>
#include <openssl/rand.h>

#include "internal.h"
#include "../crypto/internal.h"


namespace bssl {

// The address of this is a magic value, a pointer to which is returned by
// SSL_magic_pending_session_ptr(). It allows a session callback to indicate
// that it needs to asynchronously fetch session information.
static const char g_pending_session_magic = 0;

static CRYPTO_EX_DATA_CLASS g_ex_data_class =
    CRYPTO_EX_DATA_CLASS_INIT_WITH_APP_DATA;

static void SSL_SESSION_list_remove(SSL_CTX *ctx, SSL_SESSION *session);
static void SSL_SESSION_list_add(SSL_CTX *ctx, SSL_SESSION *session);
static int remove_session_lock(SSL_CTX *ctx, SSL_SESSION *session, int lock);

UniquePtr<SSL_SESSION> ssl_session_new(const SSL_X509_METHOD *x509_method) {
  UniquePtr<SSL_SESSION> session(
      (SSL_SESSION *)OPENSSL_malloc(sizeof(SSL_SESSION)));
  if (!session) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return 0;
  }
  OPENSSL_memset(session.get(), 0, sizeof(SSL_SESSION));

  session->x509_method = x509_method;
  session->verify_result = X509_V_ERR_INVALID_CALL;
  session->references = 1;
  session->timeout = SSL_DEFAULT_SESSION_TIMEOUT;
  session->auth_timeout = SSL_DEFAULT_SESSION_TIMEOUT;
  session->time = time(NULL);
  CRYPTO_new_ex_data(&session->ex_data);
  return session;
}

UniquePtr<SSL_SESSION> SSL_SESSION_dup(SSL_SESSION *session, int dup_flags) {
  UniquePtr<SSL_SESSION> new_session = ssl_session_new(session->x509_method);
  if (!new_session) {
    return nullptr;
  }

  new_session->is_server = session->is_server;
  new_session->ssl_version = session->ssl_version;
  new_session->sid_ctx_length = session->sid_ctx_length;
  OPENSSL_memcpy(new_session->sid_ctx, session->sid_ctx, session->sid_ctx_length);

  // Copy the key material.
  new_session->master_key_length = session->master_key_length;
  OPENSSL_memcpy(new_session->master_key, session->master_key,
         session->master_key_length);
  new_session->cipher = session->cipher;

  // Copy authentication state.
  if (session->psk_identity != NULL) {
    new_session->psk_identity = BUF_strdup(session->psk_identity);
    if (new_session->psk_identity == NULL) {
      return nullptr;
    }
  }
  if (session->certs != NULL) {
    new_session->certs = sk_CRYPTO_BUFFER_new_null();
    if (new_session->certs == NULL) {
      return nullptr;
    }
    for (size_t i = 0; i < sk_CRYPTO_BUFFER_num(session->certs); i++) {
      CRYPTO_BUFFER *buffer = sk_CRYPTO_BUFFER_value(session->certs, i);
      if (!sk_CRYPTO_BUFFER_push(new_session->certs, buffer)) {
        return nullptr;
      }
      CRYPTO_BUFFER_up_ref(buffer);
    }
  }

  if (!session->x509_method->session_dup(new_session.get(), session)) {
    return nullptr;
  }

  new_session->verify_result = session->verify_result;

  if (session->ocsp_response != NULL) {
    new_session->ocsp_response = session->ocsp_response;
    CRYPTO_BUFFER_up_ref(new_session->ocsp_response);
  }

  if (session->signed_cert_timestamp_list != NULL) {
    new_session->signed_cert_timestamp_list =
        session->signed_cert_timestamp_list;
    CRYPTO_BUFFER_up_ref(new_session->signed_cert_timestamp_list);
  }

  OPENSSL_memcpy(new_session->peer_sha256, session->peer_sha256,
                 SHA256_DIGEST_LENGTH);
  new_session->peer_sha256_valid = session->peer_sha256_valid;

  new_session->peer_signature_algorithm = session->peer_signature_algorithm;

  new_session->timeout = session->timeout;
  new_session->auth_timeout = session->auth_timeout;
  new_session->time = session->time;

  // Copy non-authentication connection properties.
  if (dup_flags & SSL_SESSION_INCLUDE_NONAUTH) {
    new_session->session_id_length = session->session_id_length;
    OPENSSL_memcpy(new_session->session_id, session->session_id,
                   session->session_id_length);

    new_session->group_id = session->group_id;

    OPENSSL_memcpy(new_session->original_handshake_hash,
                   session->original_handshake_hash,
                   session->original_handshake_hash_len);
    new_session->original_handshake_hash_len =
        session->original_handshake_hash_len;
    new_session->tlsext_tick_lifetime_hint = session->tlsext_tick_lifetime_hint;
    new_session->ticket_age_add = session->ticket_age_add;
    new_session->ticket_max_early_data = session->ticket_max_early_data;
    new_session->extended_master_secret = session->extended_master_secret;

    if (session->early_alpn != NULL) {
      new_session->early_alpn =
          (uint8_t *)BUF_memdup(session->early_alpn, session->early_alpn_len);
      if (new_session->early_alpn == NULL) {
        return nullptr;
      }
    }
    new_session->early_alpn_len = session->early_alpn_len;
  }

  // Copy the ticket.
  if (dup_flags & SSL_SESSION_INCLUDE_TICKET) {
    if (session->tlsext_tick != NULL) {
      new_session->tlsext_tick =
          (uint8_t *)BUF_memdup(session->tlsext_tick, session->tlsext_ticklen);
      if (new_session->tlsext_tick == NULL) {
        return nullptr;
      }
    }
    new_session->tlsext_ticklen = session->tlsext_ticklen;
  }

  // The new_session does not get a copy of the ex_data.

  new_session->not_resumable = 1;
  return new_session;
}

void ssl_session_rebase_time(SSL *ssl, SSL_SESSION *session) {
  struct OPENSSL_timeval now;
  ssl_get_current_time(ssl, &now);

  // To avoid overflows and underflows, if we've gone back in time, update the
  // time, but mark the session expired.
  if (session->time > now.tv_sec) {
    session->time = now.tv_sec;
    session->timeout = 0;
    session->auth_timeout = 0;
    return;
  }

  // Adjust the session time and timeouts. If the session has already expired,
  // clamp the timeouts at zero.
  uint64_t delta = now.tv_sec - session->time;
  session->time = now.tv_sec;
  if (session->timeout < delta) {
    session->timeout = 0;
  } else {
    session->timeout -= delta;
  }
  if (session->auth_timeout < delta) {
    session->auth_timeout = 0;
  } else {
    session->auth_timeout -= delta;
  }
}

void ssl_session_renew_timeout(SSL *ssl, SSL_SESSION *session,
                               uint32_t timeout) {
  // Rebase the timestamp relative to the current time so |timeout| is measured
  // correctly.
  ssl_session_rebase_time(ssl, session);

  if (session->timeout > timeout) {
    return;
  }

  session->timeout = timeout;
  if (session->timeout > session->auth_timeout) {
    session->timeout = session->auth_timeout;
  }
}

uint16_t ssl_session_protocol_version(const SSL_SESSION *session) {
  uint16_t ret;
  if (!ssl_protocol_version_from_wire(&ret, session->ssl_version)) {
    // An |SSL_SESSION| will never have an invalid version. This is enforced by
    // the parser.
    assert(0);
    return 0;
  }

  return ret;
}

const EVP_MD *ssl_session_get_digest(const SSL_SESSION *session) {
  return ssl_get_handshake_digest(ssl_session_protocol_version(session),
                                  session->cipher);
}

int ssl_get_new_session(SSL_HANDSHAKE *hs, int is_server) {
  SSL *const ssl = hs->ssl;
  if (ssl->mode & SSL_MODE_NO_SESSION_CREATION) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_SESSION_MAY_NOT_BE_CREATED);
    return 0;
  }

  UniquePtr<SSL_SESSION> session = ssl_session_new(ssl->ctx->x509_method);
  if (session == NULL) {
    return 0;
  }

  session->is_server = is_server;
  session->ssl_version = ssl->version;

  // Fill in the time from the |SSL_CTX|'s clock.
  struct OPENSSL_timeval now;
  ssl_get_current_time(ssl, &now);
  session->time = now.tv_sec;

  uint16_t version = ssl_protocol_version(ssl);
  if (version >= TLS1_3_VERSION) {
    // TLS 1.3 uses tickets as authenticators, so we are willing to use them for
    // longer.
    session->timeout = ssl->session_ctx->session_psk_dhe_timeout;
    session->auth_timeout = SSL_DEFAULT_SESSION_AUTH_TIMEOUT;
  } else {
    // TLS 1.2 resumption does not incorporate new key material, so we use a
    // much shorter timeout.
    session->timeout = ssl->session_ctx->session_timeout;
    session->auth_timeout = ssl->session_ctx->session_timeout;
  }

  if (is_server) {
    if (hs->ticket_expected || version >= TLS1_3_VERSION) {
      // Don't set session IDs for sessions resumed with tickets. This will keep
      // them out of the session cache.
      session->session_id_length = 0;
    } else {
      session->session_id_length = SSL3_SSL_SESSION_ID_LENGTH;
      if (!RAND_bytes(session->session_id, session->session_id_length)) {
        return 0;
      }
    }
  } else {
    session->session_id_length = 0;
  }

  if (ssl->cert->sid_ctx_length > sizeof(session->sid_ctx)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }
  OPENSSL_memcpy(session->sid_ctx, ssl->cert->sid_ctx,
                 ssl->cert->sid_ctx_length);
  session->sid_ctx_length = ssl->cert->sid_ctx_length;

  // The session is marked not resumable until it is completely filled in.
  session->not_resumable = 1;
  session->verify_result = X509_V_ERR_INVALID_CALL;

  hs->new_session = std::move(session);
  ssl_set_session(ssl, NULL);
  return 1;
}

int ssl_ctx_rotate_ticket_encryption_key(SSL_CTX *ctx) {
  OPENSSL_timeval now;
  ssl_ctx_get_current_time(ctx, &now);
  {
    // Avoid acquiring a write lock in the common case (i.e. a non-default key
    // is used or the default keys have not expired yet).
    MutexReadLock lock(&ctx->lock);
    if (ctx->tlsext_ticket_key_current &&
        (ctx->tlsext_ticket_key_current->next_rotation_tv_sec == 0 ||
         ctx->tlsext_ticket_key_current->next_rotation_tv_sec > now.tv_sec) &&
        (!ctx->tlsext_ticket_key_prev ||
         ctx->tlsext_ticket_key_prev->next_rotation_tv_sec > now.tv_sec)) {
      return 1;
    }
  }

  MutexWriteLock lock(&ctx->lock);
  if (!ctx->tlsext_ticket_key_current ||
      (ctx->tlsext_ticket_key_current->next_rotation_tv_sec != 0 &&
       ctx->tlsext_ticket_key_current->next_rotation_tv_sec <= now.tv_sec)) {
    // The current key has not been initialized or it is expired.
    auto new_key = bssl::MakeUnique<struct tlsext_ticket_key>();
    if (!new_key) {
      return 0;
    }
    OPENSSL_memset(new_key.get(), 0, sizeof(struct tlsext_ticket_key));
    if (ctx->tlsext_ticket_key_current) {
      // The current key expired. Rotate it to prev and bump up its rotation
      // timestamp. Note that even with the new rotation time it may still be
      // expired and get droppped below.
      ctx->tlsext_ticket_key_current->next_rotation_tv_sec +=
          SSL_DEFAULT_TICKET_KEY_ROTATION_INTERVAL;
      OPENSSL_free(ctx->tlsext_ticket_key_prev);
      ctx->tlsext_ticket_key_prev = ctx->tlsext_ticket_key_current;
    }
    ctx->tlsext_ticket_key_current = new_key.release();
    RAND_bytes(ctx->tlsext_ticket_key_current->name, 16);
    RAND_bytes(ctx->tlsext_ticket_key_current->hmac_key, 16);
    RAND_bytes(ctx->tlsext_ticket_key_current->aes_key, 16);
    ctx->tlsext_ticket_key_current->next_rotation_tv_sec =
        now.tv_sec + SSL_DEFAULT_TICKET_KEY_ROTATION_INTERVAL;
  }

  // Drop an expired prev key.
  if (ctx->tlsext_ticket_key_prev &&
      ctx->tlsext_ticket_key_prev->next_rotation_tv_sec <= now.tv_sec) {
    OPENSSL_free(ctx->tlsext_ticket_key_prev);
    ctx->tlsext_ticket_key_prev = nullptr;
  }

  return 1;
}

static int ssl_encrypt_ticket_with_cipher_ctx(SSL *ssl, CBB *out,
                                              const uint8_t *session_buf,
                                              size_t session_len) {
  ScopedEVP_CIPHER_CTX ctx;
  ScopedHMAC_CTX hctx;

  // If the session is too long, emit a dummy value rather than abort the
  // connection.
  static const size_t kMaxTicketOverhead =
      16 + EVP_MAX_IV_LENGTH + EVP_MAX_BLOCK_LENGTH + EVP_MAX_MD_SIZE;
  if (session_len > 0xffff - kMaxTicketOverhead) {
    static const char kTicketPlaceholder[] = "TICKET TOO LARGE";
    return CBB_add_bytes(out, (const uint8_t *)kTicketPlaceholder,
                         strlen(kTicketPlaceholder));
  }

  // Initialize HMAC and cipher contexts. If callback present it does all the
  // work otherwise use generated values from parent ctx.
  SSL_CTX *tctx = ssl->session_ctx;
  uint8_t iv[EVP_MAX_IV_LENGTH];
  uint8_t key_name[16];
  if (tctx->tlsext_ticket_key_cb != NULL) {
    if (tctx->tlsext_ticket_key_cb(ssl, key_name, iv, ctx.get(), hctx.get(),
                                   1 /* encrypt */) < 0) {
      return 0;
    }
  } else {
    // Rotate ticket key if necessary.
    if (!ssl_ctx_rotate_ticket_encryption_key(tctx)) {
      return 0;
    }
    MutexReadLock lock(&tctx->lock);
    if (!RAND_bytes(iv, 16) ||
        !EVP_EncryptInit_ex(ctx.get(), EVP_aes_128_cbc(), NULL,
                            tctx->tlsext_ticket_key_current->aes_key, iv) ||
        !HMAC_Init_ex(hctx.get(), tctx->tlsext_ticket_key_current->hmac_key, 16,
                      tlsext_tick_md(), NULL)) {
      return 0;
    }
    OPENSSL_memcpy(key_name, tctx->tlsext_ticket_key_current->name, 16);
  }

  uint8_t *ptr;
  if (!CBB_add_bytes(out, key_name, 16) ||
      !CBB_add_bytes(out, iv, EVP_CIPHER_CTX_iv_length(ctx.get())) ||
      !CBB_reserve(out, &ptr, session_len + EVP_MAX_BLOCK_LENGTH)) {
    return 0;
  }

  size_t total = 0;
#if defined(BORINGSSL_UNSAFE_FUZZER_MODE)
  OPENSSL_memcpy(ptr, session_buf, session_len);
  total = session_len;
#else
  int len;
  if (!EVP_EncryptUpdate(ctx.get(), ptr + total, &len, session_buf, session_len)) {
    return 0;
  }
  total += len;
  if (!EVP_EncryptFinal_ex(ctx.get(), ptr + total, &len)) {
    return 0;
  }
  total += len;
#endif
  if (!CBB_did_write(out, total)) {
    return 0;
  }

  unsigned hlen;
  if (!HMAC_Update(hctx.get(), CBB_data(out), CBB_len(out)) ||
      !CBB_reserve(out, &ptr, EVP_MAX_MD_SIZE) ||
      !HMAC_Final(hctx.get(), ptr, &hlen) ||
      !CBB_did_write(out, hlen)) {
    return 0;
  }

  return 1;
}

static int ssl_encrypt_ticket_with_method(SSL *ssl, CBB *out,
                                          const uint8_t *session_buf,
                                          size_t session_len) {
  const SSL_TICKET_AEAD_METHOD *method = ssl->session_ctx->ticket_aead_method;
  const size_t max_overhead = method->max_overhead(ssl);
  const size_t max_out = session_len + max_overhead;
  if (max_out < max_overhead) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
    return 0;
  }

  uint8_t *ptr;
  if (!CBB_reserve(out, &ptr, max_out)) {
    return 0;
  }

  size_t out_len;
  if (!method->seal(ssl, ptr, &out_len, max_out, session_buf, session_len)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_TICKET_ENCRYPTION_FAILED);
    return 0;
  }

  if (!CBB_did_write(out, out_len)) {
    return 0;
  }

  return 1;
}

int ssl_encrypt_ticket(SSL *ssl, CBB *out, const SSL_SESSION *session) {
  // Serialize the SSL_SESSION to be encoded into the ticket.
  uint8_t *session_buf = NULL;
  size_t session_len;
  if (!SSL_SESSION_to_bytes_for_ticket(session, &session_buf, &session_len)) {
    return -1;
  }

  int ret = 0;
  if (ssl->session_ctx->ticket_aead_method) {
    ret = ssl_encrypt_ticket_with_method(ssl, out, session_buf, session_len);
  } else {
    ret =
        ssl_encrypt_ticket_with_cipher_ctx(ssl, out, session_buf, session_len);
  }

  OPENSSL_free(session_buf);
  return ret;
}

int ssl_session_is_context_valid(const SSL *ssl, const SSL_SESSION *session) {
  if (session == NULL) {
    return 0;
  }

  return session->sid_ctx_length == ssl->cert->sid_ctx_length &&
         OPENSSL_memcmp(session->sid_ctx, ssl->cert->sid_ctx,
                        ssl->cert->sid_ctx_length) == 0;
}

int ssl_session_is_time_valid(const SSL *ssl, const SSL_SESSION *session) {
  if (session == NULL) {
    return 0;
  }

  struct OPENSSL_timeval now;
  ssl_get_current_time(ssl, &now);

  // Reject tickets from the future to avoid underflow.
  if (now.tv_sec < session->time) {
    return 0;
  }

  return session->timeout > now.tv_sec - session->time;
}

int ssl_session_is_resumable(const SSL_HANDSHAKE *hs,
                             const SSL_SESSION *session) {
  const SSL *const ssl = hs->ssl;
  return ssl_session_is_context_valid(ssl, session) &&
         // The session must have been created by the same type of end point as
         // we're now using it with.
         ssl->server == session->is_server &&
         // The session must not be expired.
         ssl_session_is_time_valid(ssl, session) &&
         /* Only resume if the session's version matches the negotiated
           * version. */
         ssl->version == session->ssl_version &&
         // Only resume if the session's cipher matches the negotiated one.
         hs->new_cipher == session->cipher &&
         // If the session contains a client certificate (either the full
         // certificate or just the hash) then require that the form of the
         // certificate matches the current configuration.
         ((sk_CRYPTO_BUFFER_num(session->certs) == 0 &&
           !session->peer_sha256_valid) ||
          session->peer_sha256_valid ==
              ssl->retain_only_sha256_of_client_certs);
}

// ssl_lookup_session looks up |session_id| in the session cache and sets
// |*out_session| to an |SSL_SESSION| object if found.
static enum ssl_hs_wait_t ssl_lookup_session(
    SSL *ssl, UniquePtr<SSL_SESSION> *out_session, const uint8_t *session_id,
    size_t session_id_len) {
  out_session->reset();

  if (session_id_len == 0 || session_id_len > SSL_MAX_SSL_SESSION_ID_LENGTH) {
    return ssl_hs_ok;
  }

  UniquePtr<SSL_SESSION> session;
  // Try the internal cache, if it exists.
  if (!(ssl->session_ctx->session_cache_mode &
        SSL_SESS_CACHE_NO_INTERNAL_LOOKUP)) {
    SSL_SESSION data;
    data.ssl_version = ssl->version;
    data.session_id_length = session_id_len;
    OPENSSL_memcpy(data.session_id, session_id, session_id_len);

    MutexReadLock lock(&ssl->session_ctx->lock);
    session.reset(lh_SSL_SESSION_retrieve(ssl->session_ctx->sessions, &data));
    if (session) {
      // |lh_SSL_SESSION_retrieve| returns a non-owning pointer.
      SSL_SESSION_up_ref(session.get());
    }
    // TODO(davidben): This should probably move it to the front of the list.
  }

  // Fall back to the external cache, if it exists.
  if (!session && (ssl->session_ctx->get_session_cb != nullptr ||
                   ssl->session_ctx->get_session_cb_legacy != nullptr)) {
    int copy = 1;
    if (ssl->session_ctx->get_session_cb != nullptr) {
      session.reset(ssl->session_ctx->get_session_cb(ssl, session_id,
                                                     session_id_len, &copy));
    } else {
      session.reset(ssl->session_ctx->get_session_cb_legacy(
          ssl, const_cast<uint8_t *>(session_id), session_id_len, &copy));
    }

    if (!session) {
      return ssl_hs_ok;
    }

    if (session.get() == SSL_magic_pending_session_ptr()) {
      session.release();  // This pointer is not actually owned.
      return ssl_hs_pending_session;
    }

    // Increment reference count now if the session callback asks us to do so
    // (note that if the session structures returned by the callback are shared
    // between threads, it must handle the reference count itself [i.e. copy ==
    // 0], or things won't be thread-safe).
    if (copy) {
      SSL_SESSION_up_ref(session.get());
    }

    // Add the externally cached session to the internal cache if necessary.
    if (!(ssl->session_ctx->session_cache_mode &
          SSL_SESS_CACHE_NO_INTERNAL_STORE)) {
      SSL_CTX_add_session(ssl->session_ctx, session.get());
    }
  }

  if (session && !ssl_session_is_time_valid(ssl, session.get())) {
    // The session was from the cache, so remove it.
    SSL_CTX_remove_session(ssl->session_ctx, session.get());
    session.reset();
  }

  *out_session = std::move(session);
  return ssl_hs_ok;
}

enum ssl_hs_wait_t ssl_get_prev_session(SSL *ssl,
                                        UniquePtr<SSL_SESSION> *out_session,
                                        bool *out_tickets_supported,
                                        bool *out_renew_ticket,
                                        const SSL_CLIENT_HELLO *client_hello) {
  // This is used only by servers.
  assert(ssl->server);
  UniquePtr<SSL_SESSION> session;
  bool renew_ticket = false;

  // If tickets are disabled, always behave as if no tickets are present.
  const uint8_t *ticket = NULL;
  size_t ticket_len = 0;
  const bool tickets_supported =
      !(SSL_get_options(ssl) & SSL_OP_NO_TICKET) &&
      ssl->version > SSL3_VERSION &&
      SSL_early_callback_ctx_extension_get(
          client_hello, TLSEXT_TYPE_session_ticket, &ticket, &ticket_len);
  if (tickets_supported && ticket_len > 0) {
    switch (ssl_process_ticket(ssl, &session, &renew_ticket, ticket, ticket_len,
                               client_hello->session_id,
                               client_hello->session_id_len)) {
      case ssl_ticket_aead_success:
        break;
      case ssl_ticket_aead_ignore_ticket:
        assert(!session);
        break;
      case ssl_ticket_aead_error:
        return ssl_hs_error;
      case ssl_ticket_aead_retry:
        return ssl_hs_pending_ticket;
    }
  } else {
    // The client didn't send a ticket, so the session ID is a real ID.
    enum ssl_hs_wait_t lookup_ret = ssl_lookup_session(
        ssl, &session, client_hello->session_id, client_hello->session_id_len);
    if (lookup_ret != ssl_hs_ok) {
      return lookup_ret;
    }
  }

  *out_session = std::move(session);
  *out_tickets_supported = tickets_supported;
  *out_renew_ticket = renew_ticket;
  return ssl_hs_ok;
}

static int remove_session_lock(SSL_CTX *ctx, SSL_SESSION *session, int lock) {
  int ret = 0;

  if (session != NULL && session->session_id_length != 0) {
    if (lock) {
      CRYPTO_MUTEX_lock_write(&ctx->lock);
    }
    SSL_SESSION *found_session = lh_SSL_SESSION_retrieve(ctx->sessions,
                                                         session);
    if (found_session == session) {
      ret = 1;
      found_session = lh_SSL_SESSION_delete(ctx->sessions, session);
      SSL_SESSION_list_remove(ctx, session);
    }

    if (lock) {
      CRYPTO_MUTEX_unlock_write(&ctx->lock);
    }

    if (ret) {
      if (ctx->remove_session_cb != NULL) {
        ctx->remove_session_cb(ctx, found_session);
      }
      SSL_SESSION_free(found_session);
    }
  }

  return ret;
}

void ssl_set_session(SSL *ssl, SSL_SESSION *session) {
  if (ssl->session == session) {
    return;
  }

  SSL_SESSION_free(ssl->session);
  ssl->session = session;
  if (session != NULL) {
    SSL_SESSION_up_ref(session);
  }
}

// locked by SSL_CTX in the calling function
static void SSL_SESSION_list_remove(SSL_CTX *ctx, SSL_SESSION *session) {
  if (session->next == NULL || session->prev == NULL) {
    return;
  }

  if (session->next == (SSL_SESSION *)&ctx->session_cache_tail) {
    // last element in list
    if (session->prev == (SSL_SESSION *)&ctx->session_cache_head) {
      // only one element in list
      ctx->session_cache_head = NULL;
      ctx->session_cache_tail = NULL;
    } else {
      ctx->session_cache_tail = session->prev;
      session->prev->next = (SSL_SESSION *)&(ctx->session_cache_tail);
    }
  } else {
    if (session->prev == (SSL_SESSION *)&ctx->session_cache_head) {
      // first element in list
      ctx->session_cache_head = session->next;
      session->next->prev = (SSL_SESSION *)&(ctx->session_cache_head);
    } else {  // middle of list
      session->next->prev = session->prev;
      session->prev->next = session->next;
    }
  }
  session->prev = session->next = NULL;
}

static void SSL_SESSION_list_add(SSL_CTX *ctx, SSL_SESSION *session) {
  if (session->next != NULL && session->prev != NULL) {
    SSL_SESSION_list_remove(ctx, session);
  }

  if (ctx->session_cache_head == NULL) {
    ctx->session_cache_head = session;
    ctx->session_cache_tail = session;
    session->prev = (SSL_SESSION *)&(ctx->session_cache_head);
    session->next = (SSL_SESSION *)&(ctx->session_cache_tail);
  } else {
    session->next = ctx->session_cache_head;
    session->next->prev = session;
    session->prev = (SSL_SESSION *)&(ctx->session_cache_head);
    ctx->session_cache_head = session;
  }
}

}  // namespace bssl

using namespace bssl;

SSL_SESSION *SSL_SESSION_new(const SSL_CTX *ctx) {
  return ssl_session_new(ctx->x509_method).release();
}

int SSL_SESSION_up_ref(SSL_SESSION *session) {
  CRYPTO_refcount_inc(&session->references);
  return 1;
}

void SSL_SESSION_free(SSL_SESSION *session) {
  if (session == NULL ||
      !CRYPTO_refcount_dec_and_test_zero(&session->references)) {
    return;
  }

  CRYPTO_free_ex_data(&g_ex_data_class, session, &session->ex_data);

  OPENSSL_cleanse(session->master_key, sizeof(session->master_key));
  OPENSSL_cleanse(session->session_id, sizeof(session->session_id));
  sk_CRYPTO_BUFFER_pop_free(session->certs, CRYPTO_BUFFER_free);
  session->x509_method->session_clear(session);
  OPENSSL_free(session->tlsext_tick);
  CRYPTO_BUFFER_free(session->signed_cert_timestamp_list);
  CRYPTO_BUFFER_free(session->ocsp_response);
  OPENSSL_free(session->psk_identity);
  OPENSSL_free(session->early_alpn);
  OPENSSL_free(session);
}

const uint8_t *SSL_SESSION_get_id(const SSL_SESSION *session,
                                  unsigned *out_len) {
  if (out_len != NULL) {
    *out_len = session->session_id_length;
  }
  return session->session_id;
}

uint32_t SSL_SESSION_get_timeout(const SSL_SESSION *session) {
  return session->timeout;
}

uint64_t SSL_SESSION_get_time(const SSL_SESSION *session) {
  if (session == NULL) {
    // NULL should crash, but silently accept it here for compatibility.
    return 0;
  }
  return session->time;
}

X509 *SSL_SESSION_get0_peer(const SSL_SESSION *session) {
  return session->x509_peer;
}

size_t SSL_SESSION_get_master_key(const SSL_SESSION *session, uint8_t *out,
                                  size_t max_out) {
  // TODO(davidben): Fix master_key_length's type and remove these casts.
  if (max_out == 0) {
    return (size_t)session->master_key_length;
  }
  if (max_out > (size_t)session->master_key_length) {
    max_out = (size_t)session->master_key_length;
  }
  OPENSSL_memcpy(out, session->master_key, max_out);
  return max_out;
}

uint64_t SSL_SESSION_set_time(SSL_SESSION *session, uint64_t time) {
  if (session == NULL) {
    return 0;
  }

  session->time = time;
  return time;
}

uint32_t SSL_SESSION_set_timeout(SSL_SESSION *session, uint32_t timeout) {
  if (session == NULL) {
    return 0;
  }

  session->timeout = timeout;
  session->auth_timeout = timeout;
  return 1;
}

int SSL_SESSION_set1_id_context(SSL_SESSION *session, const uint8_t *sid_ctx,
                                size_t sid_ctx_len) {
  if (sid_ctx_len > sizeof(session->sid_ctx)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_SSL_SESSION_ID_CONTEXT_TOO_LONG);
    return 0;
  }

  static_assert(sizeof(session->sid_ctx) < 256, "sid_ctx_len does not fit");
  session->sid_ctx_length = (uint8_t)sid_ctx_len;
  OPENSSL_memcpy(session->sid_ctx, sid_ctx, sid_ctx_len);

  return 1;
}

int SSL_SESSION_should_be_single_use(const SSL_SESSION *session) {
  return ssl_session_protocol_version(session) >= TLS1_3_VERSION;
}

int SSL_SESSION_is_resumable(const SSL_SESSION *session) {
  return !session->not_resumable;
}

int SSL_SESSION_has_ticket(const SSL_SESSION *session) {
  return session->tlsext_ticklen > 0;
}

void SSL_SESSION_get0_ticket(const SSL_SESSION *session,
                             const uint8_t **out_ticket, size_t *out_len) {
  if (out_ticket != nullptr) {
    *out_ticket = session->tlsext_tick;
  }
  *out_len = session->tlsext_ticklen;
}

uint32_t SSL_SESSION_get_ticket_lifetime_hint(const SSL_SESSION *session) {
  return session->tlsext_tick_lifetime_hint;
}

SSL_SESSION *SSL_magic_pending_session_ptr(void) {
  return (SSL_SESSION *)&g_pending_session_magic;
}

SSL_SESSION *SSL_get_session(const SSL *ssl) {
  // Once the handshake completes we return the established session. Otherwise
  // we return the intermediate session, either |session| (for resumption) or
  // |new_session| if doing a full handshake.
  if (!SSL_in_init(ssl)) {
    return ssl->s3->established_session.get();
  }
  SSL_HANDSHAKE *hs = ssl->s3->hs.get();
  if (hs->early_session) {
    return hs->early_session.get();
  }
  if (hs->new_session) {
    return hs->new_session.get();
  }
  return ssl->session;
}

SSL_SESSION *SSL_get1_session(SSL *ssl) {
  SSL_SESSION *ret = SSL_get_session(ssl);
  if (ret != NULL) {
    SSL_SESSION_up_ref(ret);
  }
  return ret;
}

int SSL_SESSION_get_ex_new_index(long argl, void *argp,
                                 CRYPTO_EX_unused *unused,
                                 CRYPTO_EX_dup *dup_unused,
                                 CRYPTO_EX_free *free_func) {
  int index;
  if (!CRYPTO_get_ex_new_index(&g_ex_data_class, &index, argl, argp,
                               free_func)) {
    return -1;
  }
  return index;
}

int SSL_SESSION_set_ex_data(SSL_SESSION *session, int idx, void *arg) {
  return CRYPTO_set_ex_data(&session->ex_data, idx, arg);
}

void *SSL_SESSION_get_ex_data(const SSL_SESSION *session, int idx) {
  return CRYPTO_get_ex_data(&session->ex_data, idx);
}

int SSL_CTX_add_session(SSL_CTX *ctx, SSL_SESSION *session) {
  // Although |session| is inserted into two structures (a doubly-linked list
  // and the hash table), |ctx| only takes one reference.
  SSL_SESSION_up_ref(session);
  UniquePtr<SSL_SESSION> owned_session(session);

  SSL_SESSION *old_session;
  MutexWriteLock lock(&ctx->lock);
  if (!lh_SSL_SESSION_insert(ctx->sessions, &old_session, session)) {
    return 0;
  }
  // |ctx->sessions| took ownership of |session| and gave us back a reference to
  // |old_session|. (|old_session| may be the same as |session|, in which case
  // we traded identical references with |ctx->sessions|.)
  owned_session.release();
  owned_session.reset(old_session);

  if (old_session != NULL) {
    if (old_session == session) {
      // |session| was already in the cache. There are no linked list pointers
      // to update.
      return 0;
    }

    // There was a session ID collision. |old_session| was replaced with
    // |session| in the hash table, so |old_session| must be removed from the
    // linked list to match.
    SSL_SESSION_list_remove(ctx, old_session);
  }

  SSL_SESSION_list_add(ctx, session);

  // Enforce any cache size limits.
  if (SSL_CTX_sess_get_cache_size(ctx) > 0) {
    while (lh_SSL_SESSION_num_items(ctx->sessions) >
           SSL_CTX_sess_get_cache_size(ctx)) {
      if (!remove_session_lock(ctx, ctx->session_cache_tail, 0)) {
        break;
      }
    }
  }

  return 1;
}

int SSL_CTX_remove_session(SSL_CTX *ctx, SSL_SESSION *session) {
  return remove_session_lock(ctx, session, 1);
}

int SSL_set_session(SSL *ssl, SSL_SESSION *session) {
  // SSL_set_session may only be called before the handshake has started.
  if (ssl->s3->initial_handshake_complete ||
      ssl->s3->hs == NULL ||
      ssl->s3->hs->state != 0) {
    abort();
  }

  ssl_set_session(ssl, session);
  return 1;
}

uint32_t SSL_CTX_set_timeout(SSL_CTX *ctx, uint32_t timeout) {
  if (ctx == NULL) {
    return 0;
  }

  // Historically, zero was treated as |SSL_DEFAULT_SESSION_TIMEOUT|.
  if (timeout == 0) {
    timeout = SSL_DEFAULT_SESSION_TIMEOUT;
  }

  uint32_t old_timeout = ctx->session_timeout;
  ctx->session_timeout = timeout;
  return old_timeout;
}

uint32_t SSL_CTX_get_timeout(const SSL_CTX *ctx) {
  if (ctx == NULL) {
    return 0;
  }

  return ctx->session_timeout;
}

void SSL_CTX_set_session_psk_dhe_timeout(SSL_CTX *ctx, uint32_t timeout) {
  ctx->session_psk_dhe_timeout = timeout;
}

typedef struct timeout_param_st {
  SSL_CTX *ctx;
  uint64_t time;
  LHASH_OF(SSL_SESSION) *cache;
} TIMEOUT_PARAM;

static void timeout_doall_arg(SSL_SESSION *session, void *void_param) {
  TIMEOUT_PARAM *param = reinterpret_cast<TIMEOUT_PARAM *>(void_param);

  if (param->time == 0 ||
      session->time + session->timeout < session->time ||
      param->time > (session->time + session->timeout)) {
    // The reason we don't call SSL_CTX_remove_session() is to
    // save on locking overhead
    (void) lh_SSL_SESSION_delete(param->cache, session);
    SSL_SESSION_list_remove(param->ctx, session);
    if (param->ctx->remove_session_cb != NULL) {
      param->ctx->remove_session_cb(param->ctx, session);
    }
    SSL_SESSION_free(session);
  }
}

void SSL_CTX_flush_sessions(SSL_CTX *ctx, uint64_t time) {
  TIMEOUT_PARAM tp;

  tp.ctx = ctx;
  tp.cache = ctx->sessions;
  if (tp.cache == NULL) {
    return;
  }
  tp.time = time;
  MutexWriteLock lock(&ctx->lock);
  lh_SSL_SESSION_doall_arg(tp.cache, timeout_doall_arg, &tp);
}

void SSL_CTX_sess_set_new_cb(SSL_CTX *ctx,
                             int (*cb)(SSL *ssl, SSL_SESSION *session)) {
  ctx->new_session_cb = cb;
}

int (*SSL_CTX_sess_get_new_cb(SSL_CTX *ctx))(SSL *ssl, SSL_SESSION *session) {
  return ctx->new_session_cb;
}

void SSL_CTX_sess_set_remove_cb(
    SSL_CTX *ctx, void (*cb)(SSL_CTX *ctx, SSL_SESSION *session)) {
  ctx->remove_session_cb = cb;
}

void (*SSL_CTX_sess_get_remove_cb(SSL_CTX *ctx))(SSL_CTX *ctx,
                                                 SSL_SESSION *session) {
  return ctx->remove_session_cb;
}

void SSL_CTX_sess_set_get_cb(SSL_CTX *ctx,
                             SSL_SESSION *(*cb)(SSL *ssl, const uint8_t *id,
                                                int id_len, int *out_copy)) {
  ctx->get_session_cb = cb;
}

void SSL_CTX_sess_set_get_cb(SSL_CTX *ctx,
                             SSL_SESSION *(*cb)(SSL *ssl, uint8_t *id,
                                                int id_len, int *out_copy)) {
  ctx->get_session_cb_legacy = cb;
}

SSL_SESSION *(*SSL_CTX_sess_get_get_cb(SSL_CTX *ctx))(SSL *ssl,
                                                      const uint8_t *id,
                                                      int id_len,
                                                      int *out_copy) {
  return ctx->get_session_cb;
}

void SSL_CTX_set_info_callback(
    SSL_CTX *ctx, void (*cb)(const SSL *ssl, int type, int value)) {
  ctx->info_callback = cb;
}

void (*SSL_CTX_get_info_callback(SSL_CTX *ctx))(const SSL *ssl, int type,
                                                int value) {
  return ctx->info_callback;
}

void SSL_CTX_set_channel_id_cb(SSL_CTX *ctx,
                               void (*cb)(SSL *ssl, EVP_PKEY **pkey)) {
  ctx->channel_id_cb = cb;
}

void (*SSL_CTX_get_channel_id_cb(SSL_CTX *ctx))(SSL *ssl, EVP_PKEY **pkey) {
  return ctx->channel_id_cb;
}
