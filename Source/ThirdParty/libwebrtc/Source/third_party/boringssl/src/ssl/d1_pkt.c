/* DTLS implementation written by Nagendra Modadugu
 * (nagendra@cs.stanford.edu) for the OpenSSL project 2005. */
/* ====================================================================
 * Copyright (c) 1998-2005 The OpenSSL Project.  All rights reserved.
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
 * [including the GNU Public Licence.] */

#include <openssl/ssl.h>

#include <assert.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/buf.h>
#include <openssl/bytestring.h>
#include <openssl/mem.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include "../crypto/internal.h"
#include "internal.h"


int dtls1_get_record(SSL *ssl) {
again:
  switch (ssl->s3->recv_shutdown) {
    case ssl_shutdown_none:
      break;
    case ssl_shutdown_fatal_alert:
      OPENSSL_PUT_ERROR(SSL, SSL_R_PROTOCOL_IS_SHUTDOWN);
      return -1;
    case ssl_shutdown_close_notify:
      return 0;
  }

  /* Read a new packet if there is no unconsumed one. */
  if (ssl_read_buffer_len(ssl) == 0) {
    int read_ret = ssl_read_buffer_extend_to(ssl, 0 /* unused */);
    if (read_ret < 0 && dtls1_is_timer_expired(ssl)) {
      /* Historically, timeouts were handled implicitly if the caller did not
       * handle them.
       *
       * TODO(davidben): This was to support blocking sockets but affected
       * non-blocking sockets. Can it be removed? */
      int timeout_ret = DTLSv1_handle_timeout(ssl);
      if (timeout_ret <= 0) {
        return timeout_ret;
      }
      goto again;
    }
    if (read_ret <= 0) {
      return read_ret;
    }
  }
  assert(ssl_read_buffer_len(ssl) > 0);

  CBS body;
  uint8_t type, alert;
  size_t consumed;
  enum ssl_open_record_t open_ret =
      dtls_open_record(ssl, &type, &body, &consumed, &alert,
                       ssl_read_buffer(ssl), ssl_read_buffer_len(ssl));
  ssl_read_buffer_consume(ssl, consumed);
  switch (open_ret) {
    case ssl_open_record_partial:
      /* Impossible in DTLS. */
      break;

    case ssl_open_record_success:
      if (CBS_len(&body) > 0xffff) {
        OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
        return -1;
      }

      SSL3_RECORD *rr = &ssl->s3->rrec;
      rr->type = type;
      rr->length = (uint16_t)CBS_len(&body);
      rr->data = (uint8_t *)CBS_data(&body);
      return 1;

    case ssl_open_record_discard:
      goto again;

    case ssl_open_record_close_notify:
      return 0;

    case ssl_open_record_fatal_alert:
      return -1;

    case ssl_open_record_error:
      ssl3_send_alert(ssl, SSL3_AL_FATAL, alert);
      return -1;
  }

  assert(0);
  OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
  return -1;
}

int dtls1_read_app_data(SSL *ssl, int *out_got_handshake, uint8_t *buf, int len,
                        int peek) {
  assert(!SSL_in_init(ssl));

  *out_got_handshake = 0;
  SSL3_RECORD *rr = &ssl->s3->rrec;

again:
  if (rr->length == 0) {
    int ret = dtls1_get_record(ssl);
    if (ret <= 0) {
      return ret;
    }
  }

  if (rr->type == SSL3_RT_HANDSHAKE) {
    /* Parse the first fragment header to determine if this is a pre-CCS or
     * post-CCS handshake record. DTLS resets handshake message numbers on each
     * handshake, so renegotiations and retransmissions are ambiguous. */
    CBS cbs, body;
    struct hm_header_st msg_hdr;
    CBS_init(&cbs, rr->data, rr->length);
    if (!dtls1_parse_fragment(&cbs, &msg_hdr, &body)) {
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
      OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_HANDSHAKE_RECORD);
      return -1;
    }

    if (msg_hdr.type == SSL3_MT_FINISHED &&
        msg_hdr.seq == ssl->d1->handshake_read_seq - 1) {
      if (msg_hdr.frag_off == 0) {
        /* Retransmit our last flight of messages. If the peer sends the second
         * Finished, they may not have received ours. Only do this for the
         * first fragment, in case the Finished was fragmented. */
        if (dtls1_check_timeout_num(ssl) < 0) {
          return -1;
        }

        dtls1_retransmit_outgoing_messages(ssl);
      }

      rr->length = 0;
      goto again;
    }

    /* Otherwise, this is a pre-CCS handshake message from an unsupported
     * renegotiation attempt. Fall through to the error path. */
  }

  if (rr->type != SSL3_RT_APPLICATION_DATA) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_RECORD);
    return -1;
  }

  /* Discard empty records. */
  if (rr->length == 0) {
    goto again;
  }

  if (len <= 0) {
    return len;
  }

  if ((unsigned)len > rr->length) {
    len = rr->length;
  }

  OPENSSL_memcpy(buf, rr->data, len);
  if (!peek) {
    /* TODO(davidben): Should the record be truncated instead? This is a
     * datagram transport. See https://crbug.com/boringssl/65. */
    rr->length -= len;
    rr->data += len;
    if (rr->length == 0) {
      /* The record has been consumed, so we may now clear the buffer. */
      ssl_read_buffer_discard(ssl);
    }
  }

  return len;
}

int dtls1_read_change_cipher_spec(SSL *ssl) {
  SSL3_RECORD *rr = &ssl->s3->rrec;

again:
  if (rr->length == 0) {
    int ret = dtls1_get_record(ssl);
    if (ret <= 0) {
      return ret;
    }
  }

  /* Drop handshake records silently. The epochs match, so this must be a
   * retransmit of a message we already received. */
  if (rr->type == SSL3_RT_HANDSHAKE) {
    rr->length = 0;
    goto again;
  }

  /* Other record types are illegal in this epoch. Note all application data
   * records come in the encrypted epoch. */
  if (rr->type != SSL3_RT_CHANGE_CIPHER_SPEC) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_RECORD);
    return -1;
  }

  if (rr->length != 1 || rr->data[0] != SSL3_MT_CCS) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_CHANGE_CIPHER_SPEC);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
    return -1;
  }

  ssl_do_msg_callback(ssl, 0 /* read */, SSL3_RT_CHANGE_CIPHER_SPEC, rr->data,
                      rr->length);

  rr->length = 0;
  ssl_read_buffer_discard(ssl);
  return 1;
}

void dtls1_read_close_notify(SSL *ssl) {
  /* Bidirectional shutdown doesn't make sense for an unordered transport. DTLS
   * alerts also aren't delivered reliably, so we may even time out because the
   * peer never received our close_notify. Report to the caller that the channel
   * has fully shut down. */
  if (ssl->s3->recv_shutdown == ssl_shutdown_none) {
    ssl->s3->recv_shutdown = ssl_shutdown_close_notify;
  }
}

int dtls1_write_app_data(SSL *ssl, const uint8_t *buf, int len) {
  assert(!SSL_in_init(ssl));

  if (len > SSL3_RT_MAX_PLAIN_LENGTH) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DTLS_MESSAGE_TOO_BIG);
    return -1;
  }

  if (len < 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_LENGTH);
    return -1;
  }

  if (len == 0) {
    return 0;
  }

  int ret = dtls1_write_record(ssl, SSL3_RT_APPLICATION_DATA, buf, (size_t)len,
                               dtls1_use_current_epoch);
  if (ret <= 0) {
    return ret;
  }
  return len;
}

int dtls1_write_record(SSL *ssl, int type, const uint8_t *buf, size_t len,
                       enum dtls1_use_epoch_t use_epoch) {
  assert(len <= SSL3_RT_MAX_PLAIN_LENGTH);
  /* There should never be a pending write buffer in DTLS. One can't write half
   * a datagram, so the write buffer is always dropped in
   * |ssl_write_buffer_flush|. */
  assert(!ssl_write_buffer_is_pending(ssl));

  if (len > SSL3_RT_MAX_PLAIN_LENGTH) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return -1;
  }

  size_t max_out = len + SSL_max_seal_overhead(ssl);
  uint8_t *out;
  size_t ciphertext_len;
  if (!ssl_write_buffer_init(ssl, &out, max_out) ||
      !dtls_seal_record(ssl, out, &ciphertext_len, max_out, type, buf, len,
                        use_epoch)) {
    ssl_write_buffer_clear(ssl);
    return -1;
  }
  ssl_write_buffer_set_len(ssl, ciphertext_len);

  int ret = ssl_write_buffer_flush(ssl);
  if (ret <= 0) {
    return ret;
  }
  return 1;
}

int dtls1_dispatch_alert(SSL *ssl) {
  int ret = dtls1_write_record(ssl, SSL3_RT_ALERT, &ssl->s3->send_alert[0], 2,
                               dtls1_use_current_epoch);
  if (ret <= 0) {
    return ret;
  }
  ssl->s3->alert_dispatch = 0;

  /* If the alert is fatal, flush the BIO now. */
  if (ssl->s3->send_alert[0] == SSL3_AL_FATAL) {
    BIO_flush(ssl->wbio);
  }

  ssl_do_msg_callback(ssl, 1 /* write */, SSL3_RT_ALERT, ssl->s3->send_alert,
                      2);

  int alert = (ssl->s3->send_alert[0] << 8) | ssl->s3->send_alert[1];
  ssl_do_info_callback(ssl, SSL_CB_WRITE_ALERT, alert);

  return 1;
}
