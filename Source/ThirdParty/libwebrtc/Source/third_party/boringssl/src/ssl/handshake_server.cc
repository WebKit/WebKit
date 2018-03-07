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
 * Copyright (c) 1998-2007 The OpenSSL Project.  All rights reserved.
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
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the OpenSSL open source
 * license provided above.
 *
 * ECC cipher suite support in OpenSSL originally written by
 * Vipul Gupta and Sumit Gupta of Sun Microsystems Laboratories.
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
#include <string.h>

#include <openssl/bn.h>
#include <openssl/buf.h>
#include <openssl/bytestring.h>
#include <openssl/cipher.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <openssl/mem.h>
#include <openssl/nid.h>
#include <openssl/rand.h>
#include <openssl/x509.h>

#include "internal.h"
#include "../crypto/internal.h"


namespace bssl {

enum ssl_server_hs_state_t {
  state_start_accept = 0,
  state_read_client_hello,
  state_select_certificate,
  state_tls13,
  state_select_parameters,
  state_send_server_hello,
  state_send_server_certificate,
  state_send_server_key_exchange,
  state_send_server_hello_done,
  state_read_client_certificate,
  state_verify_client_certificate,
  state_read_client_key_exchange,
  state_read_client_certificate_verify,
  state_read_change_cipher_spec,
  state_process_change_cipher_spec,
  state_read_next_proto,
  state_read_channel_id,
  state_read_client_finished,
  state_send_server_finished,
  state_finish_server_handshake,
  state_done,
};

int ssl_client_cipher_list_contains_cipher(const SSL_CLIENT_HELLO *client_hello,
                                           uint16_t id) {
  CBS cipher_suites;
  CBS_init(&cipher_suites, client_hello->cipher_suites,
           client_hello->cipher_suites_len);

  while (CBS_len(&cipher_suites) > 0) {
    uint16_t got_id;
    if (!CBS_get_u16(&cipher_suites, &got_id)) {
      return 0;
    }

    if (got_id == id) {
      return 1;
    }
  }

  return 0;
}

static int negotiate_version(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                             const SSL_CLIENT_HELLO *client_hello) {
  SSL *const ssl = hs->ssl;
  assert(!ssl->s3->have_version);
  CBS supported_versions, versions;
  if (ssl_client_hello_get_extension(client_hello, &supported_versions,
                                     TLSEXT_TYPE_supported_versions)) {
    if (!CBS_get_u8_length_prefixed(&supported_versions, &versions) ||
        CBS_len(&supported_versions) != 0 ||
        CBS_len(&versions) == 0) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      *out_alert = SSL_AD_DECODE_ERROR;
      return 0;
    }
  } else {
    // Convert the ClientHello version to an equivalent supported_versions
    // extension.
    static const uint8_t kTLSVersions[] = {
        0x03, 0x03,  // TLS 1.2
        0x03, 0x02,  // TLS 1.1
        0x03, 0x01,  // TLS 1
        0x03, 0x00,  // SSL 3
    };

    static const uint8_t kDTLSVersions[] = {
        0xfe, 0xfd,  // DTLS 1.2
        0xfe, 0xff,  // DTLS 1.0
    };

    size_t versions_len = 0;
    if (SSL_is_dtls(ssl)) {
      if (client_hello->version <= DTLS1_2_VERSION) {
        versions_len = 4;
      } else if (client_hello->version <= DTLS1_VERSION) {
        versions_len = 2;
      }
      CBS_init(&versions, kDTLSVersions + sizeof(kDTLSVersions) - versions_len,
               versions_len);
    } else {
      if (client_hello->version >= TLS1_2_VERSION) {
        versions_len = 8;
      } else if (client_hello->version >= TLS1_1_VERSION) {
        versions_len = 6;
      } else if (client_hello->version >= TLS1_VERSION) {
        versions_len = 4;
      } else if (client_hello->version >= SSL3_VERSION) {
        versions_len = 2;
      }
      CBS_init(&versions, kTLSVersions + sizeof(kTLSVersions) - versions_len,
               versions_len);
    }
  }

  if (!ssl_negotiate_version(hs, out_alert, &ssl->version, &versions)) {
    return 0;
  }

  // At this point, the connection's version is known and |ssl->version| is
  // fixed. Begin enforcing the record-layer version.
  ssl->s3->have_version = true;
  ssl->s3->aead_write_ctx->SetVersionIfNullCipher(ssl->version);

  // Handle FALLBACK_SCSV.
  if (ssl_client_cipher_list_contains_cipher(client_hello,
                                             SSL3_CK_FALLBACK_SCSV & 0xffff) &&
      ssl_protocol_version(ssl) < hs->max_version) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_INAPPROPRIATE_FALLBACK);
    *out_alert = SSL3_AD_INAPPROPRIATE_FALLBACK;
    return 0;
  }

  return 1;
}

static UniquePtr<STACK_OF(SSL_CIPHER)> ssl_parse_client_cipher_list(
    const SSL_CLIENT_HELLO *client_hello) {
  CBS cipher_suites;
  CBS_init(&cipher_suites, client_hello->cipher_suites,
           client_hello->cipher_suites_len);

  UniquePtr<STACK_OF(SSL_CIPHER)> sk(sk_SSL_CIPHER_new_null());
  if (!sk) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return nullptr;
  }

  while (CBS_len(&cipher_suites) > 0) {
    uint16_t cipher_suite;

    if (!CBS_get_u16(&cipher_suites, &cipher_suite)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_ERROR_IN_RECEIVED_CIPHER_LIST);
      return nullptr;
    }

    const SSL_CIPHER *c = SSL_get_cipher_by_value(cipher_suite);
    if (c != NULL && !sk_SSL_CIPHER_push(sk.get(), c)) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
      return nullptr;
    }
  }

  return sk;
}

// ssl_get_compatible_server_ciphers determines the key exchange and
// authentication cipher suite masks compatible with the server configuration
// and current ClientHello parameters of |hs|. It sets |*out_mask_k| to the key
// exchange mask and |*out_mask_a| to the authentication mask.
static void ssl_get_compatible_server_ciphers(SSL_HANDSHAKE *hs,
                                              uint32_t *out_mask_k,
                                              uint32_t *out_mask_a) {
  SSL *const ssl = hs->ssl;
  uint32_t mask_k = 0;
  uint32_t mask_a = 0;

  if (ssl_has_certificate(ssl)) {
    mask_a |= ssl_cipher_auth_mask_for_key(hs->local_pubkey.get());
    if (EVP_PKEY_id(hs->local_pubkey.get()) == EVP_PKEY_RSA) {
      mask_k |= SSL_kRSA;
    }
  }

  // Check for a shared group to consider ECDHE ciphers.
  uint16_t unused;
  if (tls1_get_shared_group(hs, &unused)) {
    mask_k |= SSL_kECDHE;
  }

  // PSK requires a server callback.
  if (ssl->psk_server_callback != NULL) {
    mask_k |= SSL_kPSK;
    mask_a |= SSL_aPSK;
  }

  *out_mask_k = mask_k;
  *out_mask_a = mask_a;
}

static const SSL_CIPHER *ssl3_choose_cipher(
    SSL_HANDSHAKE *hs, const SSL_CLIENT_HELLO *client_hello,
    const struct ssl_cipher_preference_list_st *server_pref) {
  SSL *const ssl = hs->ssl;
  STACK_OF(SSL_CIPHER) *prio, *allow;
  // in_group_flags will either be NULL, or will point to an array of bytes
  // which indicate equal-preference groups in the |prio| stack. See the
  // comment about |in_group_flags| in the |ssl_cipher_preference_list_st|
  // struct.
  const uint8_t *in_group_flags;
  // group_min contains the minimal index so far found in a group, or -1 if no
  // such value exists yet.
  int group_min = -1;

  UniquePtr<STACK_OF(SSL_CIPHER)> client_pref =
      ssl_parse_client_cipher_list(client_hello);
  if (!client_pref) {
    return nullptr;
  }

  if (ssl->options & SSL_OP_CIPHER_SERVER_PREFERENCE) {
    prio = server_pref->ciphers;
    in_group_flags = server_pref->in_group_flags;
    allow = client_pref.get();
  } else {
    prio = client_pref.get();
    in_group_flags = NULL;
    allow = server_pref->ciphers;
  }

  uint32_t mask_k, mask_a;
  ssl_get_compatible_server_ciphers(hs, &mask_k, &mask_a);

  for (size_t i = 0; i < sk_SSL_CIPHER_num(prio); i++) {
    const SSL_CIPHER *c = sk_SSL_CIPHER_value(prio, i);

    size_t cipher_index;
    if (// Check if the cipher is supported for the current version.
        SSL_CIPHER_get_min_version(c) <= ssl_protocol_version(ssl) &&
        ssl_protocol_version(ssl) <= SSL_CIPHER_get_max_version(c) &&
        // Check the cipher is supported for the server configuration.
        (c->algorithm_mkey & mask_k) &&
        (c->algorithm_auth & mask_a) &&
        // Check the cipher is in the |allow| list.
        sk_SSL_CIPHER_find(allow, &cipher_index, c)) {
      if (in_group_flags != NULL && in_group_flags[i] == 1) {
        // This element of |prio| is in a group. Update the minimum index found
        // so far and continue looking.
        if (group_min == -1 || (size_t)group_min > cipher_index) {
          group_min = cipher_index;
        }
      } else {
        if (group_min != -1 && (size_t)group_min < cipher_index) {
          cipher_index = group_min;
        }
        return sk_SSL_CIPHER_value(allow, cipher_index);
      }
    }

    if (in_group_flags != NULL && in_group_flags[i] == 0 && group_min != -1) {
      // We are about to leave a group, but we found a match in it, so that's
      // our answer.
      return sk_SSL_CIPHER_value(allow, group_min);
    }
  }

  return nullptr;
}

static enum ssl_hs_wait_t do_start_accept(SSL_HANDSHAKE *hs) {
  ssl_do_info_callback(hs->ssl, SSL_CB_HANDSHAKE_START, 1);
  hs->state = state_read_client_hello;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_read_client_hello(SSL_HANDSHAKE *hs) {
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
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return ssl_hs_error;
  }

  // Run the early callback.
  if (ssl->ctx->select_certificate_cb != NULL) {
    switch (ssl->ctx->select_certificate_cb(&client_hello)) {
      case ssl_select_cert_retry:
        return ssl_hs_certificate_selection_pending;

      case ssl_select_cert_error:
        // Connection rejected.
        OPENSSL_PUT_ERROR(SSL, SSL_R_CONNECTION_REJECTED);
        ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
        return ssl_hs_error;

      default:
        /* fallthrough */;
    }
  }

  // Freeze the version range after the early callback.
  if (!ssl_get_version_range(ssl, &hs->min_version, &hs->max_version)) {
    return ssl_hs_error;
  }

  uint8_t alert = SSL_AD_DECODE_ERROR;
  if (!negotiate_version(hs, &alert, &client_hello)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
    return ssl_hs_error;
  }

  hs->client_version = client_hello.version;
  if (client_hello.random_len != SSL3_RANDOM_SIZE) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return ssl_hs_error;
  }
  OPENSSL_memcpy(ssl->s3->client_random, client_hello.random,
                 client_hello.random_len);

  // Only null compression is supported. TLS 1.3 further requires the peer
  // advertise no other compression.
  if (OPENSSL_memchr(client_hello.compression_methods, 0,
                     client_hello.compression_methods_len) == NULL ||
      (ssl_protocol_version(ssl) >= TLS1_3_VERSION &&
       client_hello.compression_methods_len != 1)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_COMPRESSION_LIST);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
    return ssl_hs_error;
  }

  // TLS extensions.
  if (!ssl_parse_clienthello_tlsext(hs, &client_hello)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_PARSE_TLSEXT);
    return ssl_hs_error;
  }

  hs->state = state_select_certificate;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_select_certificate(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;

  SSLMessage msg;
  if (!ssl->method->get_message(ssl, &msg)) {
    return ssl_hs_read_message;
  }

  // Call |cert_cb| to update server certificates if required.
  if (ssl->cert->cert_cb != NULL) {
    int rv = ssl->cert->cert_cb(ssl, ssl->cert->cert_cb_arg);
    if (rv == 0) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_CERT_CB_ERROR);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      return ssl_hs_error;
    }
    if (rv < 0) {
      return ssl_hs_x509_lookup;
    }
  }

  if (!ssl_on_certificate_selected(hs)) {
    return ssl_hs_error;
  }

  if (ssl_protocol_version(ssl) >= TLS1_3_VERSION) {
    // Jump to the TLS 1.3 state machine.
    hs->state = state_tls13;
    return ssl_hs_ok;
  }

  SSL_CLIENT_HELLO client_hello;
  if (!ssl_client_hello_init(ssl, &client_hello, msg)) {
    return ssl_hs_error;
  }

  // Negotiate the cipher suite. This must be done after |cert_cb| so the
  // certificate is finalized.
  hs->new_cipher =
      ssl3_choose_cipher(hs, &client_hello, ssl_get_cipher_preferences(ssl));
  if (hs->new_cipher == NULL) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_NO_SHARED_CIPHER);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
    return ssl_hs_error;
  }

  hs->state = state_select_parameters;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_tls13(SSL_HANDSHAKE *hs) {
  enum ssl_hs_wait_t wait = tls13_server_handshake(hs);
  if (wait == ssl_hs_ok) {
    hs->state = state_finish_server_handshake;
    return ssl_hs_ok;
  }

  return wait;
}

static enum ssl_hs_wait_t do_select_parameters(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;

  SSLMessage msg;
  if (!ssl->method->get_message(ssl, &msg)) {
    return ssl_hs_read_message;
  }

  SSL_CLIENT_HELLO client_hello;
  if (!ssl_client_hello_init(ssl, &client_hello, msg)) {
    return ssl_hs_error;
  }

  // Determine whether we are doing session resumption.
  UniquePtr<SSL_SESSION> session;
  bool tickets_supported = false, renew_ticket = false;
  enum ssl_hs_wait_t wait = ssl_get_prev_session(
      ssl, &session, &tickets_supported, &renew_ticket, &client_hello);
  if (wait != ssl_hs_ok) {
    return wait;
  }

  if (session) {
    if (session->extended_master_secret && !hs->extended_master_secret) {
      // A ClientHello without EMS that attempts to resume a session with EMS
      // is fatal to the connection.
      OPENSSL_PUT_ERROR(SSL, SSL_R_RESUMED_EMS_SESSION_WITHOUT_EMS_EXTENSION);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
      return ssl_hs_error;
    }

    if (!ssl_session_is_resumable(hs, session.get()) ||
        // If the client offers the EMS extension, but the previous session
        // didn't use it, then negotiate a new session.
        hs->extended_master_secret != session->extended_master_secret) {
      session.reset();
    }
  }

  if (session) {
    // Use the old session.
    hs->ticket_expected = renew_ticket;
    ssl->session = session.release();
    ssl->s3->session_reused = true;
  } else {
    hs->ticket_expected = tickets_supported;
    ssl_set_session(ssl, NULL);
    if (!ssl_get_new_session(hs, 1 /* server */)) {
      return ssl_hs_error;
    }

    // Clear the session ID if we want the session to be single-use.
    if (!(ssl->ctx->session_cache_mode & SSL_SESS_CACHE_SERVER)) {
      hs->new_session->session_id_length = 0;
    }
  }

  if (ssl->ctx->dos_protection_cb != NULL &&
      ssl->ctx->dos_protection_cb(&client_hello) == 0) {
    // Connection rejected for DOS reasons.
    OPENSSL_PUT_ERROR(SSL, SSL_R_CONNECTION_REJECTED);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
    return ssl_hs_error;
  }

  if (ssl->session == NULL) {
    hs->new_session->cipher = hs->new_cipher;

    // Determine whether to request a client certificate.
    hs->cert_request = !!(ssl->verify_mode & SSL_VERIFY_PEER);
    // Only request a certificate if Channel ID isn't negotiated.
    if ((ssl->verify_mode & SSL_VERIFY_PEER_IF_NO_OBC) &&
        ssl->s3->tlsext_channel_id_valid) {
      hs->cert_request = false;
    }
    // CertificateRequest may only be sent in certificate-based ciphers.
    if (!ssl_cipher_uses_certificate_auth(hs->new_cipher)) {
      hs->cert_request = false;
    }

    if (!hs->cert_request) {
      // OpenSSL returns X509_V_OK when no certificates are requested. This is
      // classed by them as a bug, but it's assumed by at least NGINX.
      hs->new_session->verify_result = X509_V_OK;
    }
  }

  // HTTP/2 negotiation depends on the cipher suite, so ALPN negotiation was
  // deferred. Complete it now.
  uint8_t alert = SSL_AD_DECODE_ERROR;
  if (!ssl_negotiate_alpn(hs, &alert, &client_hello)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
    return ssl_hs_error;
  }

  // Now that all parameters are known, initialize the handshake hash and hash
  // the ClientHello.
  if (!hs->transcript.InitHash(ssl_protocol_version(ssl), hs->new_cipher) ||
      !ssl_hash_message(hs, msg)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
    return ssl_hs_error;
  }

  // Release the handshake buffer if client authentication isn't required.
  if (!hs->cert_request) {
    hs->transcript.FreeBuffer();
  }

  ssl->method->next_message(ssl);

  hs->state = state_send_server_hello;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_server_hello(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;

  // We only accept ChannelIDs on connections with ECDHE in order to avoid a
  // known attack while we fix ChannelID itself.
  if (ssl->s3->tlsext_channel_id_valid &&
      (hs->new_cipher->algorithm_mkey & SSL_kECDHE) == 0) {
    ssl->s3->tlsext_channel_id_valid = false;
  }

  // If this is a resumption and the original handshake didn't support
  // ChannelID then we didn't record the original handshake hashes in the
  // session and so cannot resume with ChannelIDs.
  if (ssl->session != NULL &&
      ssl->session->original_handshake_hash_len == 0) {
    ssl->s3->tlsext_channel_id_valid = false;
  }

  struct OPENSSL_timeval now;
  ssl_get_current_time(ssl, &now);
  ssl->s3->server_random[0] = now.tv_sec >> 24;
  ssl->s3->server_random[1] = now.tv_sec >> 16;
  ssl->s3->server_random[2] = now.tv_sec >> 8;
  ssl->s3->server_random[3] = now.tv_sec;
  if (!RAND_bytes(ssl->s3->server_random + 4, SSL3_RANDOM_SIZE - 4)) {
    return ssl_hs_error;
  }

  // TODO(davidben): Implement the TLS 1.1 and 1.2 downgrade sentinels once TLS
  // 1.3 is finalized and we are not implementing a draft version.

  const SSL_SESSION *session = hs->new_session.get();
  if (ssl->session != NULL) {
    session = ssl->session;
  }

  ScopedCBB cbb;
  CBB body, session_id;
  if (!ssl->method->init_message(ssl, cbb.get(), &body, SSL3_MT_SERVER_HELLO) ||
      !CBB_add_u16(&body, ssl->version) ||
      !CBB_add_bytes(&body, ssl->s3->server_random, SSL3_RANDOM_SIZE) ||
      !CBB_add_u8_length_prefixed(&body, &session_id) ||
      !CBB_add_bytes(&session_id, session->session_id,
                     session->session_id_length) ||
      !CBB_add_u16(&body, ssl_cipher_get_value(hs->new_cipher)) ||
      !CBB_add_u8(&body, 0 /* no compression */) ||
      !ssl_add_serverhello_tlsext(hs, &body) ||
      !ssl_add_message_cbb(ssl, cbb.get())) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return ssl_hs_error;
  }

  if (ssl->session != NULL) {
    hs->state = state_send_server_finished;
  } else {
    hs->state = state_send_server_certificate;
  }
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_server_certificate(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  ScopedCBB cbb;

  if (ssl_cipher_uses_certificate_auth(hs->new_cipher)) {
    if (!ssl_has_certificate(ssl)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_NO_CERTIFICATE_SET);
      return ssl_hs_error;
    }

    if (!ssl_output_cert_chain(ssl)) {
      return ssl_hs_error;
    }

    if (hs->certificate_status_expected) {
      CBB body, ocsp_response;
      if (!ssl->method->init_message(ssl, cbb.get(), &body,
                                     SSL3_MT_CERTIFICATE_STATUS) ||
          !CBB_add_u8(&body, TLSEXT_STATUSTYPE_ocsp) ||
          !CBB_add_u24_length_prefixed(&body, &ocsp_response) ||
          !CBB_add_bytes(&ocsp_response,
                         CRYPTO_BUFFER_data(ssl->cert->ocsp_response),
                         CRYPTO_BUFFER_len(ssl->cert->ocsp_response)) ||
          !ssl_add_message_cbb(ssl, cbb.get())) {
        OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
        return ssl_hs_error;
      }
    }
  }

  // Assemble ServerKeyExchange parameters if needed.
  uint32_t alg_k = hs->new_cipher->algorithm_mkey;
  uint32_t alg_a = hs->new_cipher->algorithm_auth;
  if (ssl_cipher_requires_server_key_exchange(hs->new_cipher) ||
      ((alg_a & SSL_aPSK) && ssl->psk_identity_hint)) {

    // Pre-allocate enough room to comfortably fit an ECDHE public key. Prepend
    // the client and server randoms for the signing transcript.
    CBB child;
    if (!CBB_init(cbb.get(), SSL3_RANDOM_SIZE * 2 + 128) ||
        !CBB_add_bytes(cbb.get(), ssl->s3->client_random, SSL3_RANDOM_SIZE) ||
        !CBB_add_bytes(cbb.get(), ssl->s3->server_random, SSL3_RANDOM_SIZE)) {
      return ssl_hs_error;
    }

    // PSK ciphers begin with an identity hint.
    if (alg_a & SSL_aPSK) {
      size_t len =
          (ssl->psk_identity_hint == NULL) ? 0 : strlen(ssl->psk_identity_hint);
      if (!CBB_add_u16_length_prefixed(cbb.get(), &child) ||
          !CBB_add_bytes(&child, (const uint8_t *)ssl->psk_identity_hint,
                         len)) {
        return ssl_hs_error;
      }
    }

    if (alg_k & SSL_kECDHE) {
      // Determine the group to use.
      uint16_t group_id;
      if (!tls1_get_shared_group(hs, &group_id)) {
        OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
        ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
        return ssl_hs_error;
       }
      hs->new_session->group_id = group_id;

      // Set up ECDH, generate a key, and emit the public half.
      hs->key_share = SSLKeyShare::Create(group_id);
      if (!hs->key_share ||
          !CBB_add_u8(cbb.get(), NAMED_CURVE_TYPE) ||
          !CBB_add_u16(cbb.get(), group_id) ||
          !CBB_add_u8_length_prefixed(cbb.get(), &child) ||
          !hs->key_share->Offer(&child)) {
        return ssl_hs_error;
      }
    } else {
      assert(alg_k & SSL_kPSK);
    }

    if (!CBBFinishArray(cbb.get(), &hs->server_params)) {
      return ssl_hs_error;
    }
  }

  hs->state = state_send_server_key_exchange;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_server_key_exchange(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;

  if (hs->server_params.size() == 0) {
    hs->state = state_send_server_hello_done;
    return ssl_hs_ok;
  }

  ScopedCBB cbb;
  CBB body, child;
  if (!ssl->method->init_message(ssl, cbb.get(), &body,
                                 SSL3_MT_SERVER_KEY_EXCHANGE) ||
      // |hs->server_params| contains a prefix for signing.
      hs->server_params.size() < 2 * SSL3_RANDOM_SIZE ||
      !CBB_add_bytes(&body, hs->server_params.data() + 2 * SSL3_RANDOM_SIZE,
                     hs->server_params.size() - 2 * SSL3_RANDOM_SIZE)) {
    return ssl_hs_error;
  }

  // Add a signature.
  if (ssl_cipher_uses_certificate_auth(hs->new_cipher)) {
    if (!ssl_has_private_key(ssl)) {
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      return ssl_hs_error;
    }

    // Determine the signature algorithm.
    uint16_t signature_algorithm;
    if (!tls1_choose_signature_algorithm(hs, &signature_algorithm)) {
      return ssl_hs_error;
    }
    if (ssl_protocol_version(ssl) >= TLS1_2_VERSION) {
      if (!CBB_add_u16(&body, signature_algorithm)) {
        OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
        ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
        return ssl_hs_error;
      }
    }

    // Add space for the signature.
    const size_t max_sig_len = EVP_PKEY_size(hs->local_pubkey.get());
    uint8_t *ptr;
    if (!CBB_add_u16_length_prefixed(&body, &child) ||
        !CBB_reserve(&child, &ptr, max_sig_len)) {
      return ssl_hs_error;
    }

    size_t sig_len;
    switch (ssl_private_key_sign(hs, ptr, &sig_len, max_sig_len,
                                 signature_algorithm, hs->server_params)) {
      case ssl_private_key_success:
        if (!CBB_did_write(&child, sig_len)) {
          return ssl_hs_error;
        }
        break;
      case ssl_private_key_failure:
        return ssl_hs_error;
      case ssl_private_key_retry:
        return ssl_hs_private_key_operation;
    }
  }

  if (!ssl_add_message_cbb(ssl, cbb.get())) {
    return ssl_hs_error;
  }

  hs->server_params.Reset();

  hs->state = state_send_server_hello_done;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_server_hello_done(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;

  ScopedCBB cbb;
  CBB body;

  if (hs->cert_request) {
    CBB cert_types, sigalgs_cbb;
    if (!ssl->method->init_message(ssl, cbb.get(), &body,
                                   SSL3_MT_CERTIFICATE_REQUEST) ||
        !CBB_add_u8_length_prefixed(&body, &cert_types) ||
        !CBB_add_u8(&cert_types, SSL3_CT_RSA_SIGN) ||
        (ssl_protocol_version(ssl) >= TLS1_VERSION &&
         !CBB_add_u8(&cert_types, TLS_CT_ECDSA_SIGN)) ||
        (ssl_protocol_version(ssl) >= TLS1_2_VERSION &&
         (!CBB_add_u16_length_prefixed(&body, &sigalgs_cbb) ||
          !tls12_add_verify_sigalgs(ssl, &sigalgs_cbb))) ||
        !ssl_add_client_CA_list(ssl, &body) ||
        !ssl_add_message_cbb(ssl, cbb.get())) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return ssl_hs_error;
    }
  }

  if (!ssl->method->init_message(ssl, cbb.get(), &body,
                                 SSL3_MT_SERVER_HELLO_DONE) ||
      !ssl_add_message_cbb(ssl, cbb.get())) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return ssl_hs_error;
  }

  hs->state = state_read_client_certificate;
  return ssl_hs_flush;
}

static enum ssl_hs_wait_t do_read_client_certificate(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;

  if (!hs->cert_request) {
    hs->state = state_verify_client_certificate;
    return ssl_hs_ok;
  }

  SSLMessage msg;
  if (!ssl->method->get_message(ssl, &msg)) {
    return ssl_hs_read_message;
  }

  if (msg.type != SSL3_MT_CERTIFICATE) {
    if (ssl->version == SSL3_VERSION &&
        msg.type == SSL3_MT_CLIENT_KEY_EXCHANGE) {
      // In SSL 3.0, the Certificate message is omitted to signal no
      // certificate.
      if (ssl->verify_mode & SSL_VERIFY_FAIL_IF_NO_PEER_CERT) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_PEER_DID_NOT_RETURN_A_CERTIFICATE);
        ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
        return ssl_hs_error;
      }

      // OpenSSL returns X509_V_OK when no certificates are received. This is
      // classed by them as a bug, but it's assumed by at least NGINX.
      hs->new_session->verify_result = X509_V_OK;
      hs->state = state_verify_client_certificate;
      return ssl_hs_ok;
    }

    OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_MESSAGE);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
    return ssl_hs_error;
  }

  if (!ssl_hash_message(hs, msg)) {
    return ssl_hs_error;
  }

  CBS certificate_msg = msg.body;
  uint8_t alert = SSL_AD_DECODE_ERROR;
  UniquePtr<STACK_OF(CRYPTO_BUFFER)> chain;
  if (!ssl_parse_cert_chain(&alert, &chain, &hs->peer_pubkey,
                            ssl->retain_only_sha256_of_client_certs
                                ? hs->new_session->peer_sha256
                                : NULL,
                            &certificate_msg, ssl->ctx->pool)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
    return ssl_hs_error;
  }
  sk_CRYPTO_BUFFER_pop_free(hs->new_session->certs, CRYPTO_BUFFER_free);
  hs->new_session->certs = chain.release();

  if (CBS_len(&certificate_msg) != 0 ||
      !ssl->ctx->x509_method->session_cache_objects(hs->new_session.get())) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return ssl_hs_error;
  }

  if (sk_CRYPTO_BUFFER_num(hs->new_session->certs) == 0) {
    // No client certificate so the handshake buffer may be discarded.
    hs->transcript.FreeBuffer();

    // In SSL 3.0, sending no certificate is signaled by omitting the
    // Certificate message.
    if (ssl->version == SSL3_VERSION) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_NO_CERTIFICATES_RETURNED);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
      return ssl_hs_error;
    }

    if (ssl->verify_mode & SSL_VERIFY_FAIL_IF_NO_PEER_CERT) {
      // Fail for TLS only if we required a certificate
      OPENSSL_PUT_ERROR(SSL, SSL_R_PEER_DID_NOT_RETURN_A_CERTIFICATE);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
      return ssl_hs_error;
    }

    // OpenSSL returns X509_V_OK when no certificates are received. This is
    // classed by them as a bug, but it's assumed by at least NGINX.
    hs->new_session->verify_result = X509_V_OK;
  } else if (ssl->retain_only_sha256_of_client_certs) {
    // The hash will have been filled in.
    hs->new_session->peer_sha256_valid = 1;
  }

  ssl->method->next_message(ssl);
  hs->state = state_verify_client_certificate;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_verify_client_certificate(SSL_HANDSHAKE *hs) {
  if (sk_CRYPTO_BUFFER_num(hs->new_session->certs) > 0) {
    switch (ssl_verify_peer_cert(hs)) {
      case ssl_verify_ok:
        break;
      case ssl_verify_invalid:
        return ssl_hs_error;
      case ssl_verify_retry:
        return ssl_hs_certificate_verify;
    }
  }

  hs->state = state_read_client_key_exchange;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_read_client_key_exchange(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  SSLMessage msg;
  if (!ssl->method->get_message(ssl, &msg)) {
    return ssl_hs_read_message;
  }

  if (!ssl_check_message_type(ssl, msg, SSL3_MT_CLIENT_KEY_EXCHANGE)) {
    return ssl_hs_error;
  }

  CBS client_key_exchange = msg.body;
  uint32_t alg_k = hs->new_cipher->algorithm_mkey;
  uint32_t alg_a = hs->new_cipher->algorithm_auth;

  // If using a PSK key exchange, parse the PSK identity.
  if (alg_a & SSL_aPSK) {
    CBS psk_identity;

    // If using PSK, the ClientKeyExchange contains a psk_identity. If PSK,
    // then this is the only field in the message.
    if (!CBS_get_u16_length_prefixed(&client_key_exchange, &psk_identity) ||
        ((alg_k & SSL_kPSK) && CBS_len(&client_key_exchange) != 0)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
      return ssl_hs_error;
    }

    if (CBS_len(&psk_identity) > PSK_MAX_IDENTITY_LEN ||
        CBS_contains_zero_byte(&psk_identity)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DATA_LENGTH_TOO_LONG);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
      return ssl_hs_error;
    }

    if (!CBS_strdup(&psk_identity, &hs->new_session->psk_identity)) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      return ssl_hs_error;
    }
  }

  // Depending on the key exchange method, compute |premaster_secret|.
  Array<uint8_t> premaster_secret;
  if (alg_k & SSL_kRSA) {
    CBS encrypted_premaster_secret;
    if (ssl->version > SSL3_VERSION) {
      if (!CBS_get_u16_length_prefixed(&client_key_exchange,
                                       &encrypted_premaster_secret) ||
          CBS_len(&client_key_exchange) != 0) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
        ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
        return ssl_hs_error;
      }
    } else {
      encrypted_premaster_secret = client_key_exchange;
    }

    // Allocate a buffer large enough for an RSA decryption.
    Array<uint8_t> decrypt_buf;
    if (!decrypt_buf.Init(EVP_PKEY_size(hs->local_pubkey.get()))) {
      return ssl_hs_error;
    }

    // Decrypt with no padding. PKCS#1 padding will be removed as part of the
    // timing-sensitive code below.
    size_t decrypt_len;
    switch (ssl_private_key_decrypt(hs, decrypt_buf.data(), &decrypt_len,
                                    decrypt_buf.size(),
                                    encrypted_premaster_secret)) {
      case ssl_private_key_success:
        break;
      case ssl_private_key_failure:
        return ssl_hs_error;
      case ssl_private_key_retry:
        return ssl_hs_private_key_operation;
    }

    if (decrypt_len != decrypt_buf.size()) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECRYPTION_FAILED);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECRYPT_ERROR);
      return ssl_hs_error;
    }

    // Prepare a random premaster, to be used on invalid padding. See RFC 5246,
    // section 7.4.7.1.
    if (!premaster_secret.Init(SSL_MAX_MASTER_KEY_LENGTH) ||
        !RAND_bytes(premaster_secret.data(), premaster_secret.size())) {
      return ssl_hs_error;
    }

    // The smallest padded premaster is 11 bytes of overhead. Small keys are
    // publicly invalid.
    if (decrypt_len < 11 + premaster_secret.size()) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECRYPTION_FAILED);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECRYPT_ERROR);
      return ssl_hs_error;
    }

    // Check the padding. See RFC 3447, section 7.2.2.
    size_t padding_len = decrypt_len - premaster_secret.size();
    uint8_t good = constant_time_eq_int_8(decrypt_buf[0], 0) &
                   constant_time_eq_int_8(decrypt_buf[1], 2);
    for (size_t i = 2; i < padding_len - 1; i++) {
      good &= ~constant_time_is_zero_8(decrypt_buf[i]);
    }
    good &= constant_time_is_zero_8(decrypt_buf[padding_len - 1]);

    // The premaster secret must begin with |client_version|. This too must be
    // checked in constant time (http://eprint.iacr.org/2003/052/).
    good &= constant_time_eq_8(decrypt_buf[padding_len],
                               (unsigned)(hs->client_version >> 8));
    good &= constant_time_eq_8(decrypt_buf[padding_len + 1],
                               (unsigned)(hs->client_version & 0xff));

    // Select, in constant time, either the decrypted premaster or the random
    // premaster based on |good|.
    for (size_t i = 0; i < premaster_secret.size(); i++) {
      premaster_secret[i] = constant_time_select_8(
          good, decrypt_buf[padding_len + i], premaster_secret[i]);
    }
  } else if (alg_k & SSL_kECDHE) {
    // Parse the ClientKeyExchange.
    CBS peer_key;
    if (!CBS_get_u8_length_prefixed(&client_key_exchange, &peer_key) ||
        CBS_len(&client_key_exchange) != 0) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
      return ssl_hs_error;
    }

    // Compute the premaster.
    uint8_t alert = SSL_AD_DECODE_ERROR;
    if (!hs->key_share->Finish(&premaster_secret, &alert, peer_key)) {
      ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
      return ssl_hs_error;
    }

    // The key exchange state may now be discarded.
    hs->key_share.reset();
  } else if (!(alg_k & SSL_kPSK)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
    return ssl_hs_error;
  }

  // For a PSK cipher suite, the actual pre-master secret is combined with the
  // pre-shared key.
  if (alg_a & SSL_aPSK) {
    if (ssl->psk_server_callback == NULL) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      return ssl_hs_error;
    }

    // Look up the key for the identity.
    uint8_t psk[PSK_MAX_PSK_LEN];
    unsigned psk_len = ssl->psk_server_callback(
        ssl, hs->new_session->psk_identity, psk, sizeof(psk));
    if (psk_len > PSK_MAX_PSK_LEN) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      return ssl_hs_error;
    } else if (psk_len == 0) {
      // PSK related to the given identity not found.
      OPENSSL_PUT_ERROR(SSL, SSL_R_PSK_IDENTITY_NOT_FOUND);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNKNOWN_PSK_IDENTITY);
      return ssl_hs_error;
    }

    if (alg_k & SSL_kPSK) {
      // In plain PSK, other_secret is a block of 0s with the same length as the
      // pre-shared key.
      if (!premaster_secret.Init(psk_len)) {
        return ssl_hs_error;
      }
      OPENSSL_memset(premaster_secret.data(), 0, premaster_secret.size());
    }

    ScopedCBB new_premaster;
    CBB child;
    if (!CBB_init(new_premaster.get(),
                  2 + psk_len + 2 + premaster_secret.size()) ||
        !CBB_add_u16_length_prefixed(new_premaster.get(), &child) ||
        !CBB_add_bytes(&child, premaster_secret.data(),
                       premaster_secret.size()) ||
        !CBB_add_u16_length_prefixed(new_premaster.get(), &child) ||
        !CBB_add_bytes(&child, psk, psk_len) ||
        !CBBFinishArray(new_premaster.get(), &premaster_secret)) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
      return ssl_hs_error;
    }
  }

  if (!ssl_hash_message(hs, msg)) {
    return ssl_hs_error;
  }

  // Compute the master secret.
  hs->new_session->master_key_length = tls1_generate_master_secret(
      hs, hs->new_session->master_key, premaster_secret);
  if (hs->new_session->master_key_length == 0) {
    return ssl_hs_error;
  }
  hs->new_session->extended_master_secret = hs->extended_master_secret;

  ssl->method->next_message(ssl);
  hs->state = state_read_client_certificate_verify;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_read_client_certificate_verify(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;

  // Only RSA and ECDSA client certificates are supported, so a
  // CertificateVerify is required if and only if there's a client certificate.
  if (!hs->peer_pubkey) {
    hs->transcript.FreeBuffer();
    hs->state = state_read_change_cipher_spec;
    return ssl_hs_ok;
  }

  SSLMessage msg;
  if (!ssl->method->get_message(ssl, &msg)) {
    return ssl_hs_read_message;
  }

  if (!ssl_check_message_type(ssl, msg, SSL3_MT_CERTIFICATE_VERIFY)) {
    return ssl_hs_error;
  }

  CBS certificate_verify = msg.body, signature;

  // Determine the signature algorithm.
  uint16_t signature_algorithm = 0;
  if (ssl_protocol_version(ssl) >= TLS1_2_VERSION) {
    if (!CBS_get_u16(&certificate_verify, &signature_algorithm)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
      return ssl_hs_error;
    }
    uint8_t alert = SSL_AD_DECODE_ERROR;
    if (!tls12_check_peer_sigalg(ssl, &alert, signature_algorithm)) {
      ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
      return ssl_hs_error;
    }
    hs->new_session->peer_signature_algorithm = signature_algorithm;
  } else if (!tls1_get_legacy_signature_algorithm(&signature_algorithm,
                                                  hs->peer_pubkey.get())) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_PEER_ERROR_UNSUPPORTED_CERTIFICATE_TYPE);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNSUPPORTED_CERTIFICATE);
    return ssl_hs_error;
  }

  // Parse and verify the signature.
  if (!CBS_get_u16_length_prefixed(&certificate_verify, &signature) ||
      CBS_len(&certificate_verify) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return ssl_hs_error;
  }

  bool sig_ok;
  // The SSL3 construction for CertificateVerify does not decompose into a
  // single final digest and signature, and must be special-cased.
  if (ssl_protocol_version(ssl) == SSL3_VERSION) {
    uint8_t digest[EVP_MAX_MD_SIZE];
    size_t digest_len;
    if (!hs->transcript.GetSSL3CertVerifyHash(
            digest, &digest_len, hs->new_session.get(), signature_algorithm)) {
      return ssl_hs_error;
    }

    UniquePtr<EVP_PKEY_CTX> pctx(
        EVP_PKEY_CTX_new(hs->peer_pubkey.get(), nullptr));
    sig_ok = pctx &&
             EVP_PKEY_verify_init(pctx.get()) &&
             EVP_PKEY_verify(pctx.get(), CBS_data(&signature),
                             CBS_len(&signature), digest, digest_len);
  } else {
    sig_ok =
        ssl_public_key_verify(ssl, signature, signature_algorithm,
                              hs->peer_pubkey.get(), hs->transcript.buffer());
  }

#if defined(BORINGSSL_UNSAFE_FUZZER_MODE)
  sig_ok = true;
  ERR_clear_error();
#endif
  if (!sig_ok) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_SIGNATURE);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECRYPT_ERROR);
    return ssl_hs_error;
  }

  // The handshake buffer is no longer necessary, and we may hash the current
  // message.
  hs->transcript.FreeBuffer();
  if (!ssl_hash_message(hs, msg)) {
    return ssl_hs_error;
  }

  ssl->method->next_message(ssl);
  hs->state = state_read_change_cipher_spec;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_read_change_cipher_spec(SSL_HANDSHAKE *hs) {
  hs->state = state_process_change_cipher_spec;
  return ssl_hs_read_change_cipher_spec;
}

static enum ssl_hs_wait_t do_process_change_cipher_spec(SSL_HANDSHAKE *hs) {
  if (!tls1_change_cipher_state(hs, evp_aead_open)) {
    return ssl_hs_error;
  }

  hs->state = state_read_next_proto;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_read_next_proto(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;

  if (!hs->next_proto_neg_seen) {
    hs->state = state_read_channel_id;
    return ssl_hs_ok;
  }

  SSLMessage msg;
  if (!ssl->method->get_message(ssl, &msg)) {
    return ssl_hs_read_message;
  }

  if (!ssl_check_message_type(ssl, msg, SSL3_MT_NEXT_PROTO) ||
      !ssl_hash_message(hs, msg)) {
    return ssl_hs_error;
  }

  CBS next_protocol = msg.body, selected_protocol, padding;
  if (!CBS_get_u8_length_prefixed(&next_protocol, &selected_protocol) ||
      !CBS_get_u8_length_prefixed(&next_protocol, &padding) ||
      CBS_len(&next_protocol) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return ssl_hs_error;
  }

  if (!ssl->s3->next_proto_negotiated.CopyFrom(selected_protocol)) {
    return ssl_hs_error;
  }

  ssl->method->next_message(ssl);
  hs->state = state_read_channel_id;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_read_channel_id(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;

  if (!ssl->s3->tlsext_channel_id_valid) {
    hs->state = state_read_client_finished;
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
  hs->state = state_read_client_finished;
  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_read_client_finished(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  enum ssl_hs_wait_t wait = ssl_get_finished(hs);
  if (wait != ssl_hs_ok) {
    return wait;
  }

  if (ssl->session != NULL) {
    hs->state = state_finish_server_handshake;
  } else {
    hs->state = state_send_server_finished;
  }

  // If this is a full handshake with ChannelID then record the handshake
  // hashes in |hs->new_session| in case we need them to verify a
  // ChannelID signature on a resumption of this session in the future.
  if (ssl->session == NULL && ssl->s3->tlsext_channel_id_valid &&
      !tls1_record_handshake_hashes_for_channel_id(hs)) {
    return ssl_hs_error;
  }

  return ssl_hs_ok;
}

static enum ssl_hs_wait_t do_send_server_finished(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;

  if (hs->ticket_expected) {
    const SSL_SESSION *session;
    UniquePtr<SSL_SESSION> session_copy;
    if (ssl->session == NULL) {
      // Fix the timeout to measure from the ticket issuance time.
      ssl_session_rebase_time(ssl, hs->new_session.get());
      session = hs->new_session.get();
    } else {
      // We are renewing an existing session. Duplicate the session to adjust
      // the timeout.
      session_copy = SSL_SESSION_dup(ssl->session, SSL_SESSION_INCLUDE_NONAUTH);
      if (!session_copy) {
        return ssl_hs_error;
      }

      ssl_session_rebase_time(ssl, session_copy.get());
      session = session_copy.get();
    }

    ScopedCBB cbb;
    CBB body, ticket;
    if (!ssl->method->init_message(ssl, cbb.get(), &body,
                                   SSL3_MT_NEW_SESSION_TICKET) ||
        !CBB_add_u32(&body, session->timeout) ||
        !CBB_add_u16_length_prefixed(&body, &ticket) ||
        !ssl_encrypt_ticket(ssl, &ticket, session) ||
        !ssl_add_message_cbb(ssl, cbb.get())) {
      return ssl_hs_error;
    }
  }

  if (!ssl->method->add_change_cipher_spec(ssl) ||
      !tls1_change_cipher_state(hs, evp_aead_seal) ||
      !ssl_send_finished(hs)) {
    return ssl_hs_error;
  }

  if (ssl->session != NULL) {
    hs->state = state_read_change_cipher_spec;
  } else {
    hs->state = state_finish_server_handshake;
  }
  return ssl_hs_flush;
}

static enum ssl_hs_wait_t do_finish_server_handshake(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;

  ssl->method->on_handshake_complete(ssl);

  // If we aren't retaining peer certificates then we can discard it now.
  if (hs->new_session != NULL && ssl->retain_only_sha256_of_client_certs) {
    sk_CRYPTO_BUFFER_pop_free(hs->new_session->certs, CRYPTO_BUFFER_free);
    hs->new_session->certs = NULL;
    ssl->ctx->x509_method->session_clear(hs->new_session.get());
  }

  if (ssl->session != NULL) {
    SSL_SESSION_up_ref(ssl->session);
    ssl->s3->established_session.reset(ssl->session);
  } else {
    ssl->s3->established_session = std::move(hs->new_session);
    ssl->s3->established_session->not_resumable = 0;
  }

  hs->handshake_finalized = true;
  ssl->s3->initial_handshake_complete = true;
  ssl_update_cache(hs, SSL_SESS_CACHE_SERVER);

  hs->state = state_done;
  return ssl_hs_ok;
}

enum ssl_hs_wait_t ssl_server_handshake(SSL_HANDSHAKE *hs) {
  while (hs->state != state_done) {
    enum ssl_hs_wait_t ret = ssl_hs_error;
    enum ssl_server_hs_state_t state =
        static_cast<enum ssl_server_hs_state_t>(hs->state);
    switch (state) {
      case state_start_accept:
        ret = do_start_accept(hs);
        break;
      case state_read_client_hello:
        ret = do_read_client_hello(hs);
        break;
      case state_select_certificate:
        ret = do_select_certificate(hs);
        break;
      case state_tls13:
        ret = do_tls13(hs);
        break;
      case state_select_parameters:
        ret = do_select_parameters(hs);
        break;
      case state_send_server_hello:
        ret = do_send_server_hello(hs);
        break;
      case state_send_server_certificate:
        ret = do_send_server_certificate(hs);
        break;
      case state_send_server_key_exchange:
        ret = do_send_server_key_exchange(hs);
        break;
      case state_send_server_hello_done:
        ret = do_send_server_hello_done(hs);
        break;
      case state_read_client_certificate:
        ret = do_read_client_certificate(hs);
        break;
      case state_verify_client_certificate:
        ret = do_verify_client_certificate(hs);
        break;
      case state_read_client_key_exchange:
        ret = do_read_client_key_exchange(hs);
        break;
      case state_read_client_certificate_verify:
        ret = do_read_client_certificate_verify(hs);
        break;
      case state_read_change_cipher_spec:
        ret = do_read_change_cipher_spec(hs);
        break;
      case state_process_change_cipher_spec:
        ret = do_process_change_cipher_spec(hs);
        break;
      case state_read_next_proto:
        ret = do_read_next_proto(hs);
        break;
      case state_read_channel_id:
        ret = do_read_channel_id(hs);
        break;
      case state_read_client_finished:
        ret = do_read_client_finished(hs);
        break;
      case state_send_server_finished:
        ret = do_send_server_finished(hs);
        break;
      case state_finish_server_handshake:
        ret = do_finish_server_handshake(hs);
        break;
      case state_done:
        ret = ssl_hs_ok;
        break;
    }

    if (hs->state != state) {
      ssl_do_info_callback(hs->ssl, SSL_CB_ACCEPT_LOOP, 1);
    }

    if (ret != ssl_hs_ok) {
      return ret;
    }
  }

  ssl_do_info_callback(hs->ssl, SSL_CB_HANDSHAKE_DONE, 1);
  return ssl_hs_ok;
}

const char *ssl_server_handshake_state(SSL_HANDSHAKE *hs) {
  enum ssl_server_hs_state_t state =
      static_cast<enum ssl_server_hs_state_t>(hs->state);
  switch (state) {
    case state_start_accept:
      return "TLS server start_accept";
    case state_read_client_hello:
      return "TLS server read_client_hello";
    case state_select_certificate:
      return "TLS server select_certificate";
    case state_tls13:
      return tls13_server_handshake_state(hs);
    case state_select_parameters:
      return "TLS server select_parameters";
    case state_send_server_hello:
      return "TLS server send_server_hello";
    case state_send_server_certificate:
      return "TLS server send_server_certificate";
    case state_send_server_key_exchange:
      return "TLS server send_server_key_exchange";
    case state_send_server_hello_done:
      return "TLS server send_server_hello_done";
    case state_read_client_certificate:
      return "TLS server read_client_certificate";
    case state_verify_client_certificate:
      return "TLS server verify_client_certificate";
    case state_read_client_key_exchange:
      return "TLS server read_client_key_exchange";
    case state_read_client_certificate_verify:
      return "TLS server read_client_certificate_verify";
    case state_read_change_cipher_spec:
      return "TLS server read_change_cipher_spec";
    case state_process_change_cipher_spec:
      return "TLS server process_change_cipher_spec";
    case state_read_next_proto:
      return "TLS server read_next_proto";
    case state_read_channel_id:
      return "TLS server read_channel_id";
    case state_read_client_finished:
      return "TLS server read_client_finished";
    case state_send_server_finished:
      return "TLS server send_server_finished";
    case state_finish_server_handshake:
      return "TLS server finish_server_handshake";
    case state_done:
      return "TLS server done";
  }

  return "TLS server unknown";
}

}
