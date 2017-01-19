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
#include <openssl/dh.h>
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


static int ssl3_get_client_hello(SSL *ssl);
static int ssl3_send_server_hello(SSL *ssl);
static int ssl3_send_server_certificate(SSL *ssl);
static int ssl3_send_certificate_status(SSL *ssl);
static int ssl3_send_server_key_exchange(SSL *ssl);
static int ssl3_send_certificate_request(SSL *ssl);
static int ssl3_send_server_hello_done(SSL *ssl);
static int ssl3_get_client_certificate(SSL *ssl);
static int ssl3_get_client_key_exchange(SSL *ssl);
static int ssl3_get_cert_verify(SSL *ssl);
static int ssl3_get_next_proto(SSL *ssl);
static int ssl3_get_channel_id(SSL *ssl);
static int ssl3_send_new_session_ticket(SSL *ssl);

int ssl3_accept(SSL *ssl) {
  uint32_t alg_a;
  int ret = -1;
  int state, skip = 0;

  assert(ssl->handshake_func == ssl3_accept);
  assert(ssl->server);

  for (;;) {
    state = ssl->state;

    switch (ssl->state) {
      case SSL_ST_INIT:
        ssl->state = SSL_ST_ACCEPT;
        skip = 1;
        break;

      case SSL_ST_ACCEPT:
        ssl_do_info_callback(ssl, SSL_CB_HANDSHAKE_START, 1);

        ssl->s3->hs = ssl_handshake_new(tls13_server_handshake);
        if (ssl->s3->hs == NULL) {
          ret = -1;
          goto end;
        }

        /* Enable a write buffer. This groups handshake messages within a flight
         * into a single write. */
        if (!ssl_init_wbio_buffer(ssl)) {
          ret = -1;
          goto end;
        }

        if (!ssl3_init_handshake_buffer(ssl)) {
          OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
          ret = -1;
          goto end;
        }

        ssl->state = SSL3_ST_SR_CLNT_HELLO_A;
        break;

      case SSL3_ST_SR_CLNT_HELLO_A:
      case SSL3_ST_SR_CLNT_HELLO_B:
      case SSL3_ST_SR_CLNT_HELLO_C:
      case SSL3_ST_SR_CLNT_HELLO_D:
        ret = ssl3_get_client_hello(ssl);
        if (ssl->state == SSL_ST_TLS13) {
          break;
        }
        if (ret <= 0) {
          goto end;
        }
        ssl->method->received_flight(ssl);
        ssl->state = SSL3_ST_SW_SRVR_HELLO_A;
        break;

      case SSL3_ST_SW_SRVR_HELLO_A:
      case SSL3_ST_SW_SRVR_HELLO_B:
        ret = ssl3_send_server_hello(ssl);
        if (ret <= 0) {
          goto end;
        }
        if (ssl->session != NULL) {
          ssl->state = SSL3_ST_SW_SESSION_TICKET_A;
        } else {
          ssl->state = SSL3_ST_SW_CERT_A;
        }
        break;

      case SSL3_ST_SW_CERT_A:
      case SSL3_ST_SW_CERT_B:
        if (ssl_cipher_uses_certificate_auth(ssl->s3->tmp.new_cipher)) {
          ret = ssl3_send_server_certificate(ssl);
          if (ret <= 0) {
            goto end;
          }
        } else {
          skip = 1;
        }
        ssl->state = SSL3_ST_SW_CERT_STATUS_A;
        break;

      case SSL3_ST_SW_CERT_STATUS_A:
      case SSL3_ST_SW_CERT_STATUS_B:
        if (ssl->s3->hs->certificate_status_expected) {
          ret = ssl3_send_certificate_status(ssl);
          if (ret <= 0) {
            goto end;
          }
        } else {
          skip = 1;
        }
        ssl->state = SSL3_ST_SW_KEY_EXCH_A;
        break;

      case SSL3_ST_SW_KEY_EXCH_A:
      case SSL3_ST_SW_KEY_EXCH_B:
      case SSL3_ST_SW_KEY_EXCH_C:
        alg_a = ssl->s3->tmp.new_cipher->algorithm_auth;

        /* PSK ciphers send ServerKeyExchange if there is an identity hint. */
        if (ssl_cipher_requires_server_key_exchange(ssl->s3->tmp.new_cipher) ||
            ((alg_a & SSL_aPSK) && ssl->psk_identity_hint)) {
          ret = ssl3_send_server_key_exchange(ssl);
          if (ret <= 0) {
            goto end;
          }
        } else {
          skip = 1;
        }

        ssl->state = SSL3_ST_SW_CERT_REQ_A;
        break;

      case SSL3_ST_SW_CERT_REQ_A:
      case SSL3_ST_SW_CERT_REQ_B:
        if (ssl->s3->hs->cert_request) {
          ret = ssl3_send_certificate_request(ssl);
          if (ret <= 0) {
            goto end;
          }
        } else {
          skip = 1;
        }
        ssl->state = SSL3_ST_SW_SRVR_DONE_A;
        break;

      case SSL3_ST_SW_SRVR_DONE_A:
      case SSL3_ST_SW_SRVR_DONE_B:
        ret = ssl3_send_server_hello_done(ssl);
        if (ret <= 0) {
          goto end;
        }
        ssl->s3->tmp.next_state = SSL3_ST_SR_CERT_A;
        ssl->state = SSL3_ST_SW_FLUSH;
        break;

      case SSL3_ST_SR_CERT_A:
        if (ssl->s3->hs->cert_request) {
          ret = ssl3_get_client_certificate(ssl);
          if (ret <= 0) {
            goto end;
          }
        }
        ssl->state = SSL3_ST_SR_KEY_EXCH_A;
        break;

      case SSL3_ST_SR_KEY_EXCH_A:
      case SSL3_ST_SR_KEY_EXCH_B:
        ret = ssl3_get_client_key_exchange(ssl);
        if (ret <= 0) {
          goto end;
        }
        ssl->state = SSL3_ST_SR_CERT_VRFY_A;
        break;

      case SSL3_ST_SR_CERT_VRFY_A:
        ret = ssl3_get_cert_verify(ssl);
        if (ret <= 0) {
          goto end;
        }

        ssl->state = SSL3_ST_SR_CHANGE;
        break;

      case SSL3_ST_SR_CHANGE:
        ret = ssl->method->read_change_cipher_spec(ssl);
        if (ret <= 0) {
          goto end;
        }

        if (!tls1_change_cipher_state(ssl, SSL3_CHANGE_CIPHER_SERVER_READ)) {
          ret = -1;
          goto end;
        }

        ssl->state = SSL3_ST_SR_NEXT_PROTO_A;
        break;

      case SSL3_ST_SR_NEXT_PROTO_A:
        if (ssl->s3->hs->next_proto_neg_seen) {
          ret = ssl3_get_next_proto(ssl);
          if (ret <= 0) {
            goto end;
          }
        } else {
          skip = 1;
        }
        ssl->state = SSL3_ST_SR_CHANNEL_ID_A;
        break;

      case SSL3_ST_SR_CHANNEL_ID_A:
        if (ssl->s3->tlsext_channel_id_valid) {
          ret = ssl3_get_channel_id(ssl);
          if (ret <= 0) {
            goto end;
          }
        } else {
          skip = 1;
        }
        ssl->state = SSL3_ST_SR_FINISHED_A;
        break;

      case SSL3_ST_SR_FINISHED_A:
        ret = ssl3_get_finished(ssl);
        if (ret <= 0) {
          goto end;
        }

        ssl->method->received_flight(ssl);
        if (ssl->session != NULL) {
          ssl->state = SSL_ST_OK;
        } else {
          ssl->state = SSL3_ST_SW_SESSION_TICKET_A;
        }

        /* If this is a full handshake with ChannelID then record the handshake
         * hashes in |ssl->s3->new_session| in case we need them to verify a
         * ChannelID signature on a resumption of this session in the future. */
        if (ssl->session == NULL && ssl->s3->tlsext_channel_id_valid) {
          ret = tls1_record_handshake_hashes_for_channel_id(ssl);
          if (ret <= 0) {
            goto end;
          }
        }
        break;

      case SSL3_ST_SW_SESSION_TICKET_A:
      case SSL3_ST_SW_SESSION_TICKET_B:
        if (ssl->tlsext_ticket_expected) {
          ret = ssl3_send_new_session_ticket(ssl);
          if (ret <= 0) {
            goto end;
          }
        } else {
          skip = 1;
        }
        ssl->state = SSL3_ST_SW_CHANGE;
        break;

      case SSL3_ST_SW_CHANGE:
        ret = ssl->method->send_change_cipher_spec(ssl);
        if (ret <= 0) {
          goto end;
        }
        ssl->state = SSL3_ST_SW_FINISHED_A;

        if (!tls1_change_cipher_state(ssl, SSL3_CHANGE_CIPHER_SERVER_WRITE)) {
          ret = -1;
          goto end;
        }
        break;

      case SSL3_ST_SW_FINISHED_A:
      case SSL3_ST_SW_FINISHED_B:
        ret = ssl3_send_finished(ssl, SSL3_ST_SW_FINISHED_A,
                                 SSL3_ST_SW_FINISHED_B);
        if (ret <= 0) {
          goto end;
        }
        ssl->state = SSL3_ST_SW_FLUSH;
        if (ssl->session != NULL) {
          ssl->s3->tmp.next_state = SSL3_ST_SR_CHANGE;
        } else {
          ssl->s3->tmp.next_state = SSL_ST_OK;
        }
        break;

      case SSL3_ST_SW_FLUSH:
        if (BIO_flush(ssl->wbio) <= 0) {
          ssl->rwstate = SSL_WRITING;
          ret = -1;
          goto end;
        }

        ssl->state = ssl->s3->tmp.next_state;
        if (ssl->state != SSL_ST_OK) {
          ssl->method->expect_flight(ssl);
        }
        break;

      case SSL_ST_TLS13:
        ret = tls13_handshake(ssl);
        if (ret <= 0) {
          goto end;
        }
        ssl->state = SSL_ST_OK;
        break;

      case SSL_ST_OK:
        /* Clean a few things up. */
        ssl3_cleanup_key_block(ssl);
        ssl->method->release_current_message(ssl, 1 /* free_buffer */);

        /* If we aren't retaining peer certificates then we can discard it
         * now. */
        if (ssl->s3->new_session != NULL &&
            ssl->ctx->retain_only_sha256_of_client_certs) {
          X509_free(ssl->s3->new_session->peer);
          ssl->s3->new_session->peer = NULL;
          sk_X509_pop_free(ssl->s3->new_session->cert_chain, X509_free);
          ssl->s3->new_session->cert_chain = NULL;
        }

        SSL_SESSION_free(ssl->s3->established_session);
        if (ssl->session != NULL) {
          SSL_SESSION_up_ref(ssl->session);
          ssl->s3->established_session = ssl->session;
        } else {
          ssl->s3->established_session = ssl->s3->new_session;
          ssl->s3->established_session->not_resumable = 0;
          ssl->s3->new_session = NULL;
        }

        /* remove buffering on output */
        ssl_free_wbio_buffer(ssl);

        ssl_handshake_free(ssl->s3->hs);
        ssl->s3->hs = NULL;

        ssl->s3->initial_handshake_complete = 1;

        ssl_update_cache(ssl, SSL_SESS_CACHE_SERVER);

        ssl_do_info_callback(ssl, SSL_CB_HANDSHAKE_DONE, 1);

        ret = 1;
        goto end;

      default:
        OPENSSL_PUT_ERROR(SSL, SSL_R_UNKNOWN_STATE);
        ret = -1;
        goto end;
    }

    if (!ssl->s3->tmp.reuse_message && !skip && ssl->state != state) {
      int new_state = ssl->state;
      ssl->state = state;
      ssl_do_info_callback(ssl, SSL_CB_ACCEPT_LOOP, 1);
      ssl->state = new_state;
    }
    skip = 0;
  }

end:
  ssl_do_info_callback(ssl, SSL_CB_ACCEPT_EXIT, ret);
  return ret;
}

int ssl_client_cipher_list_contains_cipher(
    const struct ssl_early_callback_ctx *client_hello, uint16_t id) {
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

static int negotiate_version(
    SSL *ssl, int *out_alert,
    const struct ssl_early_callback_ctx *client_hello) {
  uint16_t min_version, max_version;
  if (!ssl_get_version_range(ssl, &min_version, &max_version)) {
    *out_alert = SSL_AD_PROTOCOL_VERSION;
    return 0;
  }

  uint16_t version = 0;
  /* Check supported_versions extension if it is present. */
  CBS supported_versions;
  if (ssl_early_callback_get_extension(client_hello, &supported_versions,
                                       TLSEXT_TYPE_supported_versions)) {
    CBS versions;
    if (!CBS_get_u8_length_prefixed(&supported_versions, &versions) ||
        CBS_len(&supported_versions) != 0 ||
        CBS_len(&versions) == 0) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      *out_alert = SSL_AD_DECODE_ERROR;
      return 0;
    }

    int found_version = 0;
    while (CBS_len(&versions) != 0) {
      uint16_t ext_version;
      if (!CBS_get_u16(&versions, &ext_version)) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
        *out_alert = SSL_AD_DECODE_ERROR;
        return 0;
      }
      if (!ssl->method->version_from_wire(&ext_version, ext_version)) {
        continue;
      }
      if (min_version <= ext_version &&
          ext_version <= max_version) {
        version = ext_version;
        found_version = 1;
        break;
      }
    }

    if (!found_version) {
      goto unsupported_protocol;
    }
  } else {
    /* Process ClientHello.version instead. Note that versions beyond (D)TLS 1.2
     * do not use this mechanism. */
    if (SSL_is_dtls(ssl)) {
      if (client_hello->version <= DTLS1_2_VERSION) {
        version = TLS1_2_VERSION;
      } else if (client_hello->version <= DTLS1_VERSION) {
        version = TLS1_1_VERSION;
      } else {
        goto unsupported_protocol;
      }
    } else {
      if (client_hello->version >= TLS1_2_VERSION) {
        version = TLS1_2_VERSION;
      } else if (client_hello->version >= TLS1_1_VERSION) {
        version = TLS1_1_VERSION;
      } else if (client_hello->version >= TLS1_VERSION) {
        version = TLS1_VERSION;
      } else if (client_hello->version >= SSL3_VERSION) {
        version = SSL3_VERSION;
      } else {
        goto unsupported_protocol;
      }
    }

    /* Apply our minimum and maximum version. */
    if (version > max_version) {
      version = max_version;
    }

    if (version < min_version) {
      goto unsupported_protocol;
    }
  }

  /* Handle FALLBACK_SCSV. */
  if (ssl_client_cipher_list_contains_cipher(client_hello,
                                             SSL3_CK_FALLBACK_SCSV & 0xffff) &&
      version < max_version) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_INAPPROPRIATE_FALLBACK);
    *out_alert = SSL3_AD_INAPPROPRIATE_FALLBACK;
    return 0;
  }

  ssl->client_version = client_hello->version;
  ssl->version = ssl->method->version_to_wire(version);
  ssl->s3->enc_method = ssl3_get_enc_method(version);
  assert(ssl->s3->enc_method != NULL);

  /* At this point, the connection's version is known and |ssl->version| is
   * fixed. Begin enforcing the record-layer version. */
  ssl->s3->have_version = 1;

  return 1;

unsupported_protocol:
  OPENSSL_PUT_ERROR(SSL, SSL_R_UNSUPPORTED_PROTOCOL);
  *out_alert = SSL_AD_PROTOCOL_VERSION;
  return 0;
}

static int ssl3_get_client_hello(SSL *ssl) {
  int al = SSL_AD_INTERNAL_ERROR, ret = -1;
  SSL_SESSION *session = NULL;

  if (ssl->state == SSL3_ST_SR_CLNT_HELLO_A) {
    /* The first time around, read the ClientHello. */
    int msg_ret = ssl->method->ssl_get_message(ssl, SSL3_MT_CLIENT_HELLO,
                                               ssl_hash_message);
    if (msg_ret <= 0) {
      return msg_ret;
    }

    ssl->state = SSL3_ST_SR_CLNT_HELLO_B;
  }

  struct ssl_early_callback_ctx client_hello;
  if (!ssl_early_callback_init(ssl, &client_hello, ssl->init_msg,
                               ssl->init_num)) {
    al = SSL_AD_DECODE_ERROR;
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    goto f_err;
  }

  if (ssl->state == SSL3_ST_SR_CLNT_HELLO_B) {
    /* Unlike other callbacks, the early callback is not run a second time if
     * paused. */
    ssl->state = SSL3_ST_SR_CLNT_HELLO_C;

    /* Run the early callback. */
    if (ssl->ctx->select_certificate_cb != NULL) {
      switch (ssl->ctx->select_certificate_cb(&client_hello)) {
        case 0:
          ssl->rwstate = SSL_CERTIFICATE_SELECTION_PENDING;
          goto err;

        case -1:
          /* Connection rejected. */
          al = SSL_AD_HANDSHAKE_FAILURE;
          OPENSSL_PUT_ERROR(SSL, SSL_R_CONNECTION_REJECTED);
          goto f_err;

        default:
          /* fallthrough */;
      }
    }
  }

  /* Negotiate the protocol version if we have not done so yet. */
  if (!ssl->s3->have_version) {
    if (!negotiate_version(ssl, &al, &client_hello)) {
      goto f_err;
    }

    if (ssl3_protocol_version(ssl) >= TLS1_3_VERSION) {
      ssl->state = SSL_ST_TLS13;
      return 1;
    }
  }

  if (ssl->state == SSL3_ST_SR_CLNT_HELLO_C) {
    /* Load the client random. */
    if (client_hello.random_len != SSL3_RANDOM_SIZE) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return -1;
    }
    memcpy(ssl->s3->client_random, client_hello.random,
           client_hello.random_len);

    /* Determine whether we are doing session resumption. */
    int send_new_ticket = 0;
    switch (
        ssl_get_prev_session(ssl, &session, &send_new_ticket, &client_hello)) {
      case ssl_session_success:
        break;
      case ssl_session_error:
        goto err;
      case ssl_session_retry:
        ssl->rwstate = SSL_PENDING_SESSION;
        goto err;
    }
    ssl->tlsext_ticket_expected = send_new_ticket;

    /* The EMS state is needed when making the resumption decision, but
     * extensions are not normally parsed until later. This detects the EMS
     * extension for the resumption decision and it's checked against the result
     * of the normal parse later in this function. */
    CBS ems;
    int have_extended_master_secret =
        ssl->version != SSL3_VERSION &&
        ssl_early_callback_get_extension(&client_hello, &ems,
                                         TLSEXT_TYPE_extended_master_secret) &&
        CBS_len(&ems) == 0;

    int has_session = 0;
    if (session != NULL) {
      if (session->extended_master_secret &&
          !have_extended_master_secret) {
        /* A ClientHello without EMS that attempts to resume a session with EMS
         * is fatal to the connection. */
        al = SSL_AD_HANDSHAKE_FAILURE;
        OPENSSL_PUT_ERROR(SSL, SSL_R_RESUMED_EMS_SESSION_WITHOUT_EMS_EXTENSION);
        goto f_err;
      }

      has_session =
          /* Only resume if the session's version matches the negotiated
           * version: most clients do not accept a mismatch. */
          ssl->version == session->ssl_version &&
          /* If the client offers the EMS extension, but the previous session
           * didn't use it, then negotiate a new session. */
          have_extended_master_secret == session->extended_master_secret;
    }

    if (has_session) {
      /* Use the old session. */
      ssl->session = session;
      session = NULL;
      ssl->s3->session_reused = 1;
    } else {
      ssl_set_session(ssl, NULL);
      if (!ssl_get_new_session(ssl, 1 /* server */)) {
        goto err;
      }

      /* Clear the session ID if we want the session to be single-use. */
      if (!(ssl->ctx->session_cache_mode & SSL_SESS_CACHE_SERVER)) {
        ssl->s3->new_session->session_id_length = 0;
      }
    }

    if (ssl->ctx->dos_protection_cb != NULL &&
        ssl->ctx->dos_protection_cb(&client_hello) == 0) {
      /* Connection rejected for DOS reasons. */
      al = SSL_AD_INTERNAL_ERROR;
      OPENSSL_PUT_ERROR(SSL, SSL_R_CONNECTION_REJECTED);
      goto f_err;
    }

    /* Only null compression is supported. */
    if (memchr(client_hello.compression_methods, 0,
               client_hello.compression_methods_len) == NULL) {
      al = SSL_AD_ILLEGAL_PARAMETER;
      OPENSSL_PUT_ERROR(SSL, SSL_R_NO_COMPRESSION_SPECIFIED);
      goto f_err;
    }

    /* TLS extensions. */
    if (!ssl_parse_clienthello_tlsext(ssl, &client_hello)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_PARSE_TLSEXT);
      goto err;
    }

    if (have_extended_master_secret != ssl->s3->tmp.extended_master_secret) {
      al = SSL_AD_INTERNAL_ERROR;
      OPENSSL_PUT_ERROR(SSL, SSL_R_EMS_STATE_INCONSISTENT);
      goto f_err;
    }

    ssl->state = SSL3_ST_SR_CLNT_HELLO_D;
  }

  /* Determine the remaining connection parameters. This is a separate state so
   * |cert_cb| does not cause earlier logic to run multiple times. */
  assert(ssl->state == SSL3_ST_SR_CLNT_HELLO_D);

  if (ssl->session != NULL) {
    /* Check that the cipher is in the list. */
    if (!ssl_client_cipher_list_contains_cipher(
            &client_hello, (uint16_t)ssl->session->cipher->id)) {
      al = SSL_AD_ILLEGAL_PARAMETER;
      OPENSSL_PUT_ERROR(SSL, SSL_R_REQUIRED_CIPHER_MISSING);
      goto f_err;
    }

    ssl->s3->tmp.new_cipher = ssl->session->cipher;
  } else {
    /* Call |cert_cb| to update server certificates if required. */
    if (ssl->cert->cert_cb != NULL) {
      int rv = ssl->cert->cert_cb(ssl, ssl->cert->cert_cb_arg);
      if (rv == 0) {
        al = SSL_AD_INTERNAL_ERROR;
        OPENSSL_PUT_ERROR(SSL, SSL_R_CERT_CB_ERROR);
        goto f_err;
      }
      if (rv < 0) {
        ssl->rwstate = SSL_X509_LOOKUP;
        goto err;
      }
    }

    const SSL_CIPHER *c =
        ssl3_choose_cipher(ssl, &client_hello, ssl_get_cipher_preferences(ssl));
    if (c == NULL) {
      al = SSL_AD_HANDSHAKE_FAILURE;
      OPENSSL_PUT_ERROR(SSL, SSL_R_NO_SHARED_CIPHER);
      goto f_err;
    }

    ssl->s3->new_session->cipher = c;
    ssl->s3->tmp.new_cipher = c;

    /* Determine whether to request a client certificate. */
    ssl->s3->hs->cert_request = !!(ssl->verify_mode & SSL_VERIFY_PEER);
    /* Only request a certificate if Channel ID isn't negotiated. */
    if ((ssl->verify_mode & SSL_VERIFY_PEER_IF_NO_OBC) &&
        ssl->s3->tlsext_channel_id_valid) {
      ssl->s3->hs->cert_request = 0;
    }
    /* CertificateRequest may only be sent in certificate-based ciphers. */
    if (!ssl_cipher_uses_certificate_auth(ssl->s3->tmp.new_cipher)) {
      ssl->s3->hs->cert_request = 0;
    }

    if (!ssl->s3->hs->cert_request) {
      /* OpenSSL returns X509_V_OK when no certificates are requested. This is
       * classed by them as a bug, but it's assumed by at least NGINX. */
      ssl->s3->new_session->verify_result = X509_V_OK;
    }
  }

  /* Now that the cipher is known, initialize the handshake hash. */
  if (!ssl3_init_handshake_hash(ssl)) {
    goto f_err;
  }

  /* Release the handshake buffer if client authentication isn't required. */
  if (!ssl->s3->hs->cert_request) {
    ssl3_free_handshake_buffer(ssl);
  }

  ret = 1;

  if (0) {
  f_err:
    ssl3_send_alert(ssl, SSL3_AL_FATAL, al);
  }

err:
  SSL_SESSION_free(session);
  return ret;
}

static int ssl3_send_server_hello(SSL *ssl) {
  if (ssl->state == SSL3_ST_SW_SRVR_HELLO_B) {
    return ssl->method->write_message(ssl);
  }

  assert(ssl->state == SSL3_ST_SW_SRVR_HELLO_A);

  /* We only accept ChannelIDs on connections with ECDHE in order to avoid a
   * known attack while we fix ChannelID itself. */
  if (ssl->s3->tlsext_channel_id_valid &&
      (ssl->s3->tmp.new_cipher->algorithm_mkey & SSL_kECDHE) == 0) {
    ssl->s3->tlsext_channel_id_valid = 0;
  }

  /* If this is a resumption and the original handshake didn't support
   * ChannelID then we didn't record the original handshake hashes in the
   * session and so cannot resume with ChannelIDs. */
  if (ssl->session != NULL &&
      ssl->session->original_handshake_hash_len == 0) {
    ssl->s3->tlsext_channel_id_valid = 0;
  }

  struct timeval now;
  ssl_get_current_time(ssl, &now);
  ssl->s3->server_random[0] = now.tv_sec >> 24;
  ssl->s3->server_random[1] = now.tv_sec >> 16;
  ssl->s3->server_random[2] = now.tv_sec >> 8;
  ssl->s3->server_random[3] = now.tv_sec;
  if (!RAND_bytes(ssl->s3->server_random + 4, SSL3_RANDOM_SIZE - 4)) {
    return -1;
  }

  /* TODO(davidben): Implement the TLS 1.1 and 1.2 downgrade sentinels once TLS
   * 1.3 is finalized and we are not implementing a draft version. */

  const SSL_SESSION *session = ssl->s3->new_session;
  if (ssl->session != NULL) {
    session = ssl->session;
  }

  CBB cbb, body, session_id;
  if (!ssl->method->init_message(ssl, &cbb, &body, SSL3_MT_SERVER_HELLO) ||
      !CBB_add_u16(&body, ssl->version) ||
      !CBB_add_bytes(&body, ssl->s3->server_random, SSL3_RANDOM_SIZE) ||
      !CBB_add_u8_length_prefixed(&body, &session_id) ||
      !CBB_add_bytes(&session_id, session->session_id,
                     session->session_id_length) ||
      !CBB_add_u16(&body, ssl_cipher_get_value(ssl->s3->tmp.new_cipher)) ||
      !CBB_add_u8(&body, 0 /* no compression */) ||
      !ssl_add_serverhello_tlsext(ssl, &body) ||
      !ssl->method->finish_message(ssl, &cbb)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    CBB_cleanup(&cbb);
    return -1;
  }

  ssl->state = SSL3_ST_SW_SRVR_HELLO_B;
  return ssl->method->write_message(ssl);
}

static int ssl3_send_server_certificate(SSL *ssl) {
  if (ssl->state == SSL3_ST_SW_CERT_B) {
    return ssl->method->write_message(ssl);
  }

  if (!ssl_has_certificate(ssl)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_NO_CERTIFICATE_SET);
    return 0;
  }

  if (!ssl3_output_cert_chain(ssl)) {
    return 0;
  }
  ssl->state = SSL3_ST_SW_CERT_B;
  return ssl->method->write_message(ssl);
}

static int ssl3_send_certificate_status(SSL *ssl) {
  if (ssl->state == SSL3_ST_SW_CERT_STATUS_B) {
    return ssl->method->write_message(ssl);
  }

  CBB cbb, body, ocsp_response;
  if (!ssl->method->init_message(ssl, &cbb, &body,
                                 SSL3_MT_CERTIFICATE_STATUS) ||
      !CBB_add_u8(&body, TLSEXT_STATUSTYPE_ocsp) ||
      !CBB_add_u24_length_prefixed(&body, &ocsp_response) ||
      !CBB_add_bytes(&ocsp_response, ssl->ctx->ocsp_response,
                     ssl->ctx->ocsp_response_length) ||
      !ssl->method->finish_message(ssl, &cbb)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    CBB_cleanup(&cbb);
    return -1;
  }

  ssl->state = SSL3_ST_SW_CERT_STATUS_B;
  return ssl->method->write_message(ssl);
}

static int ssl3_send_server_key_exchange(SSL *ssl) {
  if (ssl->state == SSL3_ST_SW_KEY_EXCH_C) {
    return ssl->method->write_message(ssl);
  }

  CBB cbb, child;
  CBB_zero(&cbb);

  /* Put together the parameters. */
  if (ssl->state == SSL3_ST_SW_KEY_EXCH_A) {
    uint32_t alg_k = ssl->s3->tmp.new_cipher->algorithm_mkey;
    uint32_t alg_a = ssl->s3->tmp.new_cipher->algorithm_auth;

    /* Pre-allocate enough room to comfortably fit an ECDHE public key. */
    if (!CBB_init(&cbb, 128)) {
      goto err;
    }

    /* PSK ciphers begin with an identity hint. */
    if (alg_a & SSL_aPSK) {
      size_t len =
          (ssl->psk_identity_hint == NULL) ? 0 : strlen(ssl->psk_identity_hint);
      if (!CBB_add_u16_length_prefixed(&cbb, &child) ||
          !CBB_add_bytes(&child, (const uint8_t *)ssl->psk_identity_hint,
                         len)) {
        goto err;
      }
    }

    if (alg_k & SSL_kDHE) {
      /* Determine the group to use. */
      DH *params = ssl->cert->dh_tmp;
      if (params == NULL && ssl->cert->dh_tmp_cb != NULL) {
        params = ssl->cert->dh_tmp_cb(ssl, 0, 1024);
      }
      if (params == NULL) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_MISSING_TMP_DH_KEY);
        ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
        goto err;
      }
      ssl->s3->new_session->key_exchange_info = DH_num_bits(params);

      /* Set up DH, generate a key, and emit the public half. */
      DH *dh = DHparams_dup(params);
      if (dh == NULL) {
        goto err;
      }

      SSL_ECDH_CTX_init_for_dhe(&ssl->s3->hs->ecdh_ctx, dh);
      if (!CBB_add_u16_length_prefixed(&cbb, &child) ||
          !BN_bn2cbb_padded(&child, BN_num_bytes(params->p), params->p) ||
          !CBB_add_u16_length_prefixed(&cbb, &child) ||
          !BN_bn2cbb_padded(&child, BN_num_bytes(params->g), params->g) ||
          !CBB_add_u16_length_prefixed(&cbb, &child) ||
          !SSL_ECDH_CTX_offer(&ssl->s3->hs->ecdh_ctx, &child)) {
        goto err;
      }
    } else if (alg_k & SSL_kECDHE) {
      /* Determine the group to use. */
      uint16_t group_id;
      if (!tls1_get_shared_group(ssl, &group_id)) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_MISSING_TMP_ECDH_KEY);
        ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
        goto err;
      }
      ssl->s3->new_session->key_exchange_info = group_id;

      /* Set up ECDH, generate a key, and emit the public half. */
      if (!SSL_ECDH_CTX_init(&ssl->s3->hs->ecdh_ctx, group_id) ||
          !CBB_add_u8(&cbb, NAMED_CURVE_TYPE) ||
          !CBB_add_u16(&cbb, group_id) ||
          !CBB_add_u8_length_prefixed(&cbb, &child) ||
          !SSL_ECDH_CTX_offer(&ssl->s3->hs->ecdh_ctx, &child)) {
        goto err;
      }
    } else if (alg_k & SSL_kCECPQ1) {
      SSL_ECDH_CTX_init_for_cecpq1(&ssl->s3->hs->ecdh_ctx);
      if (!CBB_add_u16_length_prefixed(&cbb, &child) ||
          !SSL_ECDH_CTX_offer(&ssl->s3->hs->ecdh_ctx, &child)) {
        goto err;
      }
    } else {
      assert(alg_k & SSL_kPSK);
    }

    if (!CBB_finish(&cbb, &ssl->s3->hs->server_params,
                    &ssl->s3->hs->server_params_len)) {
      goto err;
    }
  }

  /* Assemble the message. */
  CBB body;
  if (!ssl->method->init_message(ssl, &cbb, &body,
                                 SSL3_MT_SERVER_KEY_EXCHANGE) ||
      !CBB_add_bytes(&body, ssl->s3->hs->server_params,
                     ssl->s3->hs->server_params_len)) {
    goto err;
  }

  /* Add a signature. */
  if (ssl_cipher_uses_certificate_auth(ssl->s3->tmp.new_cipher)) {
    if (!ssl_has_private_key(ssl)) {
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      goto err;
    }

    /* Determine the signature algorithm. */
    uint16_t signature_algorithm;
    if (!tls1_choose_signature_algorithm(ssl, &signature_algorithm)) {
      goto err;
    }
    if (ssl3_protocol_version(ssl) >= TLS1_2_VERSION) {
      if (!CBB_add_u16(&body, signature_algorithm)) {
        OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
        ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
        goto err;
      }
    }

    /* Add space for the signature. */
    const size_t max_sig_len = ssl_private_key_max_signature_len(ssl);
    uint8_t *ptr;
    if (!CBB_add_u16_length_prefixed(&body, &child) ||
        !CBB_reserve(&child, &ptr, max_sig_len)) {
      goto err;
    }

    size_t sig_len;
    enum ssl_private_key_result_t sign_result;
    if (ssl->state == SSL3_ST_SW_KEY_EXCH_A) {
      CBB transcript;
      uint8_t *transcript_data;
      size_t transcript_len;
      if (!CBB_init(&transcript,
                    2*SSL3_RANDOM_SIZE + ssl->s3->hs->server_params_len) ||
          !CBB_add_bytes(&transcript, ssl->s3->client_random, SSL3_RANDOM_SIZE) ||
          !CBB_add_bytes(&transcript, ssl->s3->server_random, SSL3_RANDOM_SIZE) ||
          !CBB_add_bytes(&transcript, ssl->s3->hs->server_params,
                         ssl->s3->hs->server_params_len) ||
          !CBB_finish(&transcript, &transcript_data, &transcript_len)) {
        CBB_cleanup(&transcript);
        OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
        ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
        goto err;
      }

      sign_result = ssl_private_key_sign(ssl, ptr, &sig_len, max_sig_len,
                                         signature_algorithm, transcript_data,
                                         transcript_len);
      OPENSSL_free(transcript_data);
    } else {
      assert(ssl->state == SSL3_ST_SW_KEY_EXCH_B);
      sign_result = ssl_private_key_complete(ssl, ptr, &sig_len, max_sig_len);
    }

    switch (sign_result) {
      case ssl_private_key_success:
        if (!CBB_did_write(&child, sig_len)) {
          goto err;
        }
        break;
      case ssl_private_key_failure:
        goto err;
      case ssl_private_key_retry:
        ssl->rwstate = SSL_PRIVATE_KEY_OPERATION;
        ssl->state = SSL3_ST_SW_KEY_EXCH_B;
        goto err;
    }
  }

  if (!ssl->method->finish_message(ssl, &cbb)) {
    goto err;
  }

  OPENSSL_free(ssl->s3->hs->server_params);
  ssl->s3->hs->server_params = NULL;
  ssl->s3->hs->server_params_len = 0;

  ssl->state = SSL3_ST_SW_KEY_EXCH_C;
  return ssl->method->write_message(ssl);

err:
  CBB_cleanup(&cbb);
  return -1;
}

static int add_cert_types(SSL *ssl, CBB *cbb) {
  /* Get configured signature algorithms. */
  int have_rsa_sign = 0;
  int have_ecdsa_sign = 0;
  const uint16_t *sig_algs;
  size_t num_sig_algs = tls12_get_verify_sigalgs(ssl, &sig_algs);
  for (size_t i = 0; i < num_sig_algs; i++) {
    switch (sig_algs[i]) {
      case SSL_SIGN_RSA_PKCS1_SHA512:
      case SSL_SIGN_RSA_PKCS1_SHA384:
      case SSL_SIGN_RSA_PKCS1_SHA256:
      case SSL_SIGN_RSA_PKCS1_SHA1:
        have_rsa_sign = 1;
        break;

      case SSL_SIGN_ECDSA_SECP521R1_SHA512:
      case SSL_SIGN_ECDSA_SECP384R1_SHA384:
      case SSL_SIGN_ECDSA_SECP256R1_SHA256:
      case SSL_SIGN_ECDSA_SHA1:
        have_ecdsa_sign = 1;
        break;
    }
  }

  if (have_rsa_sign && !CBB_add_u8(cbb, SSL3_CT_RSA_SIGN)) {
    return 0;
  }

  /* ECDSA certs can be used with RSA cipher suites as well so we don't need to
   * check for SSL_kECDH or SSL_kECDHE. */
  if (ssl->version >= TLS1_VERSION && have_ecdsa_sign &&
      !CBB_add_u8(cbb, TLS_CT_ECDSA_SIGN)) {
    return 0;
  }

  return 1;
}

static int ssl3_send_certificate_request(SSL *ssl) {
  if (ssl->state == SSL3_ST_SW_CERT_REQ_B) {
    return ssl->method->write_message(ssl);
  }

  CBB cbb, body, cert_types, sigalgs_cbb;
  if (!ssl->method->init_message(ssl, &cbb, &body,
                                 SSL3_MT_CERTIFICATE_REQUEST) ||
      !CBB_add_u8_length_prefixed(&body, &cert_types) ||
      !add_cert_types(ssl, &cert_types)) {
    goto err;
  }

  if (ssl3_protocol_version(ssl) >= TLS1_2_VERSION) {
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
  }

  if (!ssl_add_client_CA_list(ssl, &body) ||
      !ssl->method->finish_message(ssl, &cbb)) {
    goto err;
  }

  ssl->state = SSL3_ST_SW_CERT_REQ_B;
  return ssl->method->write_message(ssl);

err:
  OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
  CBB_cleanup(&cbb);
  return -1;
}

static int ssl3_send_server_hello_done(SSL *ssl) {
  if (ssl->state == SSL3_ST_SW_SRVR_DONE_B) {
    return ssl->method->write_message(ssl);
  }

  CBB cbb, body;
  if (!ssl->method->init_message(ssl, &cbb, &body, SSL3_MT_SERVER_HELLO_DONE) ||
      !ssl->method->finish_message(ssl, &cbb)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    CBB_cleanup(&cbb);
    return -1;
  }

  ssl->state = SSL3_ST_SW_SRVR_DONE_B;
  return ssl->method->write_message(ssl);
}

static int ssl3_get_client_certificate(SSL *ssl) {
  assert(ssl->s3->hs->cert_request);

  int msg_ret = ssl->method->ssl_get_message(ssl, -1, ssl_hash_message);
  if (msg_ret <= 0) {
    return msg_ret;
  }

  if (ssl->s3->tmp.message_type != SSL3_MT_CERTIFICATE) {
    if (ssl->version == SSL3_VERSION &&
        ssl->s3->tmp.message_type == SSL3_MT_CLIENT_KEY_EXCHANGE) {
      /* In SSL 3.0, the Certificate message is omitted to signal no
       * certificate. */
      if (ssl->verify_mode & SSL_VERIFY_FAIL_IF_NO_PEER_CERT) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_PEER_DID_NOT_RETURN_A_CERTIFICATE);
        ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
        return -1;
      }

      /* OpenSSL returns X509_V_OK when no certificates are received. This is
       * classed by them as a bug, but it's assumed by at least NGINX. */
      ssl->s3->new_session->verify_result = X509_V_OK;
      ssl->s3->tmp.reuse_message = 1;
      return 1;
    }

    OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_MESSAGE);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
    return -1;
  }

  CBS certificate_msg;
  CBS_init(&certificate_msg, ssl->init_msg, ssl->init_num);
  uint8_t alert;
  STACK_OF(X509) *chain = ssl_parse_cert_chain(
      ssl, &alert, ssl->ctx->retain_only_sha256_of_client_certs
                       ? ssl->s3->new_session->peer_sha256
                       : NULL,
      &certificate_msg);
  if (chain == NULL) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, alert);
    goto err;
  }

  if (CBS_len(&certificate_msg) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    goto err;
  }

  if (sk_X509_num(chain) == 0) {
    /* No client certificate so the handshake buffer may be discarded. */
    ssl3_free_handshake_buffer(ssl);

    /* In SSL 3.0, sending no certificate is signaled by omitting the
     * Certificate message. */
    if (ssl->version == SSL3_VERSION) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_NO_CERTIFICATES_RETURNED);
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
      goto err;
    }

    if (ssl->verify_mode & SSL_VERIFY_FAIL_IF_NO_PEER_CERT) {
      /* Fail for TLS only if we required a certificate */
      OPENSSL_PUT_ERROR(SSL, SSL_R_PEER_DID_NOT_RETURN_A_CERTIFICATE);
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_HANDSHAKE_FAILURE);
      goto err;
    }

    /* OpenSSL returns X509_V_OK when no certificates are received. This is
     * classed by them as a bug, but it's assumed by at least NGINX. */
    ssl->s3->new_session->verify_result = X509_V_OK;
  } else {
    /* The hash would have been filled in. */
    if (ssl->ctx->retain_only_sha256_of_client_certs) {
      ssl->s3->new_session->peer_sha256_valid = 1;
    }

    if (!ssl_verify_cert_chain(ssl, &ssl->s3->new_session->verify_result,
                               chain)) {
      goto err;
    }
  }

  X509_free(ssl->s3->new_session->peer);
  ssl->s3->new_session->peer = sk_X509_shift(chain);

  sk_X509_pop_free(ssl->s3->new_session->cert_chain, X509_free);
  ssl->s3->new_session->cert_chain = chain;
  /* Inconsistency alert: cert_chain does *not* include the peer's own
   * certificate, while we do include it in s3_clnt.c */

  return 1;

err:
  sk_X509_pop_free(chain, X509_free);
  return -1;
}

static int ssl3_get_client_key_exchange(SSL *ssl) {
  int al;
  CBS client_key_exchange;
  uint32_t alg_k;
  uint32_t alg_a;
  uint8_t *premaster_secret = NULL;
  size_t premaster_secret_len = 0;
  uint8_t *decrypt_buf = NULL;

  unsigned psk_len = 0;
  uint8_t psk[PSK_MAX_PSK_LEN];

  if (ssl->state == SSL3_ST_SR_KEY_EXCH_A) {
    int ret = ssl->method->ssl_get_message(ssl, SSL3_MT_CLIENT_KEY_EXCHANGE,
                                           ssl_hash_message);
    if (ret <= 0) {
      return ret;
    }
  }

  CBS_init(&client_key_exchange, ssl->init_msg, ssl->init_num);
  alg_k = ssl->s3->tmp.new_cipher->algorithm_mkey;
  alg_a = ssl->s3->tmp.new_cipher->algorithm_auth;

  /* If using a PSK key exchange, prepare the pre-shared key. */
  if (alg_a & SSL_aPSK) {
    CBS psk_identity;

    /* If using PSK, the ClientKeyExchange contains a psk_identity. If PSK,
     * then this is the only field in the message. */
    if (!CBS_get_u16_length_prefixed(&client_key_exchange, &psk_identity) ||
        ((alg_k & SSL_kPSK) && CBS_len(&client_key_exchange) != 0)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      al = SSL_AD_DECODE_ERROR;
      goto f_err;
    }

    if (ssl->psk_server_callback == NULL) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_PSK_NO_SERVER_CB);
      al = SSL_AD_INTERNAL_ERROR;
      goto f_err;
    }

    if (CBS_len(&psk_identity) > PSK_MAX_IDENTITY_LEN ||
        CBS_contains_zero_byte(&psk_identity)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DATA_LENGTH_TOO_LONG);
      al = SSL_AD_ILLEGAL_PARAMETER;
      goto f_err;
    }

    if (!CBS_strdup(&psk_identity, &ssl->s3->new_session->psk_identity)) {
      al = SSL_AD_INTERNAL_ERROR;
      OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
      goto f_err;
    }

    /* Look up the key for the identity. */
    psk_len = ssl->psk_server_callback(ssl, ssl->s3->new_session->psk_identity,
                                       psk, sizeof(psk));
    if (psk_len > PSK_MAX_PSK_LEN) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      al = SSL_AD_INTERNAL_ERROR;
      goto f_err;
    } else if (psk_len == 0) {
      /* PSK related to the given identity not found */
      OPENSSL_PUT_ERROR(SSL, SSL_R_PSK_IDENTITY_NOT_FOUND);
      al = SSL_AD_UNKNOWN_PSK_IDENTITY;
      goto f_err;
    }
  }

  /* Depending on the key exchange method, compute |premaster_secret| and
   * |premaster_secret_len|. */
  if (alg_k & SSL_kRSA) {
    /* Allocate a buffer large enough for an RSA decryption. */
    const size_t rsa_size = ssl_private_key_max_signature_len(ssl);
    decrypt_buf = OPENSSL_malloc(rsa_size);
    if (decrypt_buf == NULL) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
      goto err;
    }

    enum ssl_private_key_result_t decrypt_result;
    size_t decrypt_len;
    if (ssl->state == SSL3_ST_SR_KEY_EXCH_A) {
      if (!ssl_has_private_key(ssl) ||
          ssl_private_key_type(ssl) != NID_rsaEncryption) {
        al = SSL_AD_HANDSHAKE_FAILURE;
        OPENSSL_PUT_ERROR(SSL, SSL_R_MISSING_RSA_CERTIFICATE);
        goto f_err;
      }
      CBS encrypted_premaster_secret;
      if (ssl->version > SSL3_VERSION) {
        if (!CBS_get_u16_length_prefixed(&client_key_exchange,
                                         &encrypted_premaster_secret) ||
            CBS_len(&client_key_exchange) != 0) {
          al = SSL_AD_DECODE_ERROR;
          OPENSSL_PUT_ERROR(SSL,
                            SSL_R_TLS_RSA_ENCRYPTED_VALUE_LENGTH_IS_WRONG);
          goto f_err;
        }
      } else {
        encrypted_premaster_secret = client_key_exchange;
      }

      /* Decrypt with no padding. PKCS#1 padding will be removed as part of the
       * timing-sensitive code below. */
      decrypt_result = ssl_private_key_decrypt(
          ssl, decrypt_buf, &decrypt_len, rsa_size,
          CBS_data(&encrypted_premaster_secret),
          CBS_len(&encrypted_premaster_secret));
    } else {
      assert(ssl->state == SSL3_ST_SR_KEY_EXCH_B);
      /* Complete async decrypt. */
      decrypt_result =
          ssl_private_key_complete(ssl, decrypt_buf, &decrypt_len, rsa_size);
    }

    switch (decrypt_result) {
      case ssl_private_key_success:
        break;
      case ssl_private_key_failure:
        goto err;
      case ssl_private_key_retry:
        ssl->rwstate = SSL_PRIVATE_KEY_OPERATION;
        ssl->state = SSL3_ST_SR_KEY_EXCH_B;
        goto err;
    }

    if (decrypt_len != rsa_size) {
      al = SSL_AD_DECRYPT_ERROR;
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECRYPTION_FAILED);
      goto f_err;
    }

    /* Prepare a random premaster, to be used on invalid padding. See RFC 5246,
     * section 7.4.7.1. */
    premaster_secret_len = SSL_MAX_MASTER_KEY_LENGTH;
    premaster_secret = OPENSSL_malloc(premaster_secret_len);
    if (premaster_secret == NULL) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
      goto err;
    }
    if (!RAND_bytes(premaster_secret, premaster_secret_len)) {
      goto err;
    }

    /* The smallest padded premaster is 11 bytes of overhead. Small keys are
     * publicly invalid. */
    if (decrypt_len < 11 + premaster_secret_len) {
      al = SSL_AD_DECRYPT_ERROR;
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECRYPTION_FAILED);
      goto f_err;
    }

    /* Check the padding. See RFC 3447, section 7.2.2. */
    size_t padding_len = decrypt_len - premaster_secret_len;
    uint8_t good = constant_time_eq_int_8(decrypt_buf[0], 0) &
                   constant_time_eq_int_8(decrypt_buf[1], 2);
    for (size_t i = 2; i < padding_len - 1; i++) {
      good &= ~constant_time_is_zero_8(decrypt_buf[i]);
    }
    good &= constant_time_is_zero_8(decrypt_buf[padding_len - 1]);

    /* The premaster secret must begin with |client_version|. This too must be
     * checked in constant time (http://eprint.iacr.org/2003/052/). */
    good &= constant_time_eq_8(decrypt_buf[padding_len],
                               (unsigned)(ssl->client_version >> 8));
    good &= constant_time_eq_8(decrypt_buf[padding_len + 1],
                               (unsigned)(ssl->client_version & 0xff));

    /* Select, in constant time, either the decrypted premaster or the random
     * premaster based on |good|. */
    for (size_t i = 0; i < premaster_secret_len; i++) {
      premaster_secret[i] = constant_time_select_8(
          good, decrypt_buf[padding_len + i], premaster_secret[i]);
    }

    OPENSSL_free(decrypt_buf);
    decrypt_buf = NULL;
  } else if (alg_k & (SSL_kECDHE|SSL_kDHE|SSL_kCECPQ1)) {
    /* Parse the ClientKeyExchange. */
    CBS peer_key;
    if (!SSL_ECDH_CTX_get_key(&ssl->s3->hs->ecdh_ctx, &client_key_exchange,
                              &peer_key) ||
        CBS_len(&client_key_exchange) != 0) {
      al = SSL_AD_DECODE_ERROR;
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      goto f_err;
    }

    /* Compute the premaster. */
    uint8_t alert;
    if (!SSL_ECDH_CTX_finish(&ssl->s3->hs->ecdh_ctx, &premaster_secret,
                             &premaster_secret_len, &alert, CBS_data(&peer_key),
                             CBS_len(&peer_key))) {
      al = alert;
      goto f_err;
    }

    /* The key exchange state may now be discarded. */
    SSL_ECDH_CTX_cleanup(&ssl->s3->hs->ecdh_ctx);
  } else if (alg_k & SSL_kPSK) {
    /* For plain PSK, other_secret is a block of 0s with the same length as the
     * pre-shared key. */
    premaster_secret_len = psk_len;
    premaster_secret = OPENSSL_malloc(premaster_secret_len);
    if (premaster_secret == NULL) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
      goto err;
    }
    memset(premaster_secret, 0, premaster_secret_len);
  } else {
    al = SSL_AD_HANDSHAKE_FAILURE;
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNKNOWN_CIPHER_TYPE);
    goto f_err;
  }

  /* For a PSK cipher suite, the actual pre-master secret is combined with the
   * pre-shared key. */
  if (alg_a & SSL_aPSK) {
    CBB new_premaster, child;
    uint8_t *new_data;
    size_t new_len;

    CBB_zero(&new_premaster);
    if (!CBB_init(&new_premaster, 2 + psk_len + 2 + premaster_secret_len) ||
        !CBB_add_u16_length_prefixed(&new_premaster, &child) ||
        !CBB_add_bytes(&child, premaster_secret, premaster_secret_len) ||
        !CBB_add_u16_length_prefixed(&new_premaster, &child) ||
        !CBB_add_bytes(&child, psk, psk_len) ||
        !CBB_finish(&new_premaster, &new_data, &new_len)) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
      CBB_cleanup(&new_premaster);
      goto err;
    }

    OPENSSL_cleanse(premaster_secret, premaster_secret_len);
    OPENSSL_free(premaster_secret);
    premaster_secret = new_data;
    premaster_secret_len = new_len;
  }

  /* Compute the master secret */
  ssl->s3->new_session->master_key_length = tls1_generate_master_secret(
      ssl, ssl->s3->new_session->master_key, premaster_secret,
      premaster_secret_len);
  if (ssl->s3->new_session->master_key_length == 0) {
    goto err;
  }
  ssl->s3->new_session->extended_master_secret =
      ssl->s3->tmp.extended_master_secret;

  OPENSSL_cleanse(premaster_secret, premaster_secret_len);
  OPENSSL_free(premaster_secret);
  return 1;

f_err:
  ssl3_send_alert(ssl, SSL3_AL_FATAL, al);
err:
  if (premaster_secret != NULL) {
    OPENSSL_cleanse(premaster_secret, premaster_secret_len);
    OPENSSL_free(premaster_secret);
  }
  OPENSSL_free(decrypt_buf);

  return -1;
}

static int ssl3_get_cert_verify(SSL *ssl) {
  int al, ret = 0;
  CBS certificate_verify, signature;
  X509 *peer = ssl->s3->new_session->peer;
  EVP_PKEY *pkey = NULL;

  /* Only RSA and ECDSA client certificates are supported, so a
   * CertificateVerify is required if and only if there's a client certificate.
   * */
  if (peer == NULL) {
    ssl3_free_handshake_buffer(ssl);
    return 1;
  }

  int msg_ret = ssl->method->ssl_get_message(ssl, SSL3_MT_CERTIFICATE_VERIFY,
                                             ssl_dont_hash_message);
  if (msg_ret <= 0) {
    return msg_ret;
  }

  /* Filter out unsupported certificate types. */
  pkey = X509_get_pubkey(peer);
  if (pkey == NULL) {
    goto err;
  }

  CBS_init(&certificate_verify, ssl->init_msg, ssl->init_num);

  /* Determine the digest type if needbe. */
  uint16_t signature_algorithm = 0;
  if (ssl3_protocol_version(ssl) >= TLS1_2_VERSION) {
    if (!CBS_get_u16(&certificate_verify, &signature_algorithm)) {
      al = SSL_AD_DECODE_ERROR;
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      goto f_err;
    }
    if (!tls12_check_peer_sigalg(ssl, &al, signature_algorithm)) {
      goto f_err;
    }
    ssl->s3->tmp.peer_signature_algorithm = signature_algorithm;
  } else if (pkey->type == EVP_PKEY_RSA) {
    signature_algorithm = SSL_SIGN_RSA_PKCS1_MD5_SHA1;
  } else if (pkey->type == EVP_PKEY_EC) {
    signature_algorithm = SSL_SIGN_ECDSA_SHA1;
  } else {
    al = SSL_AD_UNSUPPORTED_CERTIFICATE;
    OPENSSL_PUT_ERROR(SSL, SSL_R_PEER_ERROR_UNSUPPORTED_CERTIFICATE_TYPE);
    goto f_err;
  }

  /* Parse and verify the signature. */
  if (!CBS_get_u16_length_prefixed(&certificate_verify, &signature) ||
      CBS_len(&certificate_verify) != 0) {
    al = SSL_AD_DECODE_ERROR;
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    goto f_err;
  }

  int sig_ok;
  /* The SSL3 construction for CertificateVerify does not decompose into a
   * single final digest and signature, and must be special-cased. */
  if (ssl3_protocol_version(ssl) == SSL3_VERSION) {
    const EVP_MD *md;
    uint8_t digest[EVP_MAX_MD_SIZE];
    size_t digest_len;
    if (!ssl3_cert_verify_hash(ssl, &md, digest, &digest_len,
                               signature_algorithm)) {
      goto err;
    }

    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new(pkey, NULL);
    sig_ok = pctx != NULL &&
             EVP_PKEY_verify_init(pctx) &&
             EVP_PKEY_CTX_set_signature_md(pctx, md) &&
             EVP_PKEY_verify(pctx, CBS_data(&signature), CBS_len(&signature),
                             digest, digest_len);
    EVP_PKEY_CTX_free(pctx);
  } else {
    sig_ok = ssl_public_key_verify(
        ssl, CBS_data(&signature), CBS_len(&signature), signature_algorithm,
        pkey, (const uint8_t *)ssl->s3->handshake_buffer->data,
        ssl->s3->handshake_buffer->length);
  }

#if defined(BORINGSSL_UNSAFE_FUZZER_MODE)
  sig_ok = 1;
  ERR_clear_error();
#endif
  if (!sig_ok) {
    al = SSL_AD_DECRYPT_ERROR;
    OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_SIGNATURE);
    goto f_err;
  }

  /* The handshake buffer is no longer necessary, and we may hash the current
   * message.*/
  ssl3_free_handshake_buffer(ssl);
  if (!ssl->method->hash_current_message(ssl)) {
    goto err;
  }

  ret = 1;

  if (0) {
  f_err:
    ssl3_send_alert(ssl, SSL3_AL_FATAL, al);
  }

err:
  EVP_PKEY_free(pkey);

  return ret;
}

/* ssl3_get_next_proto reads a Next Protocol Negotiation handshake message. It
 * sets the next_proto member in s if found */
static int ssl3_get_next_proto(SSL *ssl) {
  int ret =
      ssl->method->ssl_get_message(ssl, SSL3_MT_NEXT_PROTO, ssl_hash_message);
  if (ret <= 0) {
    return ret;
  }

  CBS next_protocol, selected_protocol, padding;
  CBS_init(&next_protocol, ssl->init_msg, ssl->init_num);
  if (!CBS_get_u8_length_prefixed(&next_protocol, &selected_protocol) ||
      !CBS_get_u8_length_prefixed(&next_protocol, &padding) ||
      CBS_len(&next_protocol) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return 0;
  }

  if (!CBS_stow(&selected_protocol, &ssl->s3->next_proto_negotiated,
                &ssl->s3->next_proto_negotiated_len)) {
    return 0;
  }

  return 1;
}

/* ssl3_get_channel_id reads and verifies a ClientID handshake message. */
static int ssl3_get_channel_id(SSL *ssl) {
  int msg_ret = ssl->method->ssl_get_message(ssl, SSL3_MT_CHANNEL_ID,
                                             ssl_dont_hash_message);
  if (msg_ret <= 0) {
    return msg_ret;
  }

  if (!tls1_verify_channel_id(ssl) ||
      !ssl->method->hash_current_message(ssl)) {
    return -1;
  }
  return 1;
}

static int ssl3_send_new_session_ticket(SSL *ssl) {
  if (ssl->state == SSL3_ST_SW_SESSION_TICKET_B) {
    return ssl->method->write_message(ssl);
  }

  CBB cbb, body, ticket;
  if (!ssl->method->init_message(ssl, &cbb, &body,
                                 SSL3_MT_NEW_SESSION_TICKET) ||
      /* Ticket lifetime hint (advisory only): We leave this unspecified for
       * resumed session (for simplicity), and guess that tickets for new
       * sessions will live as long as their sessions. */
      !CBB_add_u32(&body,
                   ssl->session != NULL ? 0 : ssl->s3->new_session->timeout) ||
      !CBB_add_u16_length_prefixed(&body, &ticket) ||
      !ssl_encrypt_ticket(ssl, &ticket, ssl->session != NULL
                                            ? ssl->session
                                            : ssl->s3->new_session) ||
      !ssl->method->finish_message(ssl, &cbb)) {
    return 0;
  }

  ssl->state = SSL3_ST_SW_SESSION_TICKET_B;
  return ssl->method->write_message(ssl);
}
