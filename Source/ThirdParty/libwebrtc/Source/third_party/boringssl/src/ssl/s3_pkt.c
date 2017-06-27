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

#include <openssl/ssl.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <openssl/buf.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/rand.h>

#include "../crypto/internal.h"
#include "internal.h"


static int do_ssl3_write(SSL *ssl, int type, const uint8_t *buf, unsigned len);

/* ssl3_get_record reads a new input record. On success, it places it in
 * |ssl->s3->rrec| and returns one. Otherwise it returns <= 0 on error or if
 * more data is needed. */
static int ssl3_get_record(SSL *ssl) {
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

  CBS body;
  uint8_t type, alert = SSL_AD_DECODE_ERROR;
  size_t consumed;
  enum ssl_open_record_t open_ret =
      tls_open_record(ssl, &type, &body, &consumed, &alert,
                      ssl_read_buffer(ssl), ssl_read_buffer_len(ssl));
  if (open_ret != ssl_open_record_partial) {
    ssl_read_buffer_consume(ssl, consumed);
  }
  switch (open_ret) {
    case ssl_open_record_partial: {
      int read_ret = ssl_read_buffer_extend_to(ssl, consumed);
      if (read_ret <= 0) {
        return read_ret;
      }
      goto again;
    }

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

int ssl3_write_app_data(SSL *ssl, const uint8_t *buf, int len) {
  assert(ssl_can_write(ssl));
  assert(ssl->s3->aead_write_ctx != NULL);

  unsigned tot, n, nw;

  assert(ssl->s3->wnum <= INT_MAX);
  tot = ssl->s3->wnum;
  ssl->s3->wnum = 0;

  /* Ensure that if we end up with a smaller value of data to write out than
   * the the original len from a write which didn't complete for non-blocking
   * I/O and also somehow ended up avoiding the check for this in
   * ssl3_write_pending/SSL_R_BAD_WRITE_RETRY as it must never be possible to
   * end up with (len-tot) as a large number that will then promptly send
   * beyond the end of the users buffer ... so we trap and report the error in
   * a way the user will notice. */
  if (len < 0 || (size_t)len < tot) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_LENGTH);
    return -1;
  }

  n = len - tot;
  for (;;) {
    /* max contains the maximum number of bytes that we can put into a
     * record. */
    unsigned max = ssl->max_send_fragment;
    if (n > max) {
      nw = max;
    } else {
      nw = n;
    }

    int ret = do_ssl3_write(ssl, SSL3_RT_APPLICATION_DATA, &buf[tot], nw);
    if (ret <= 0) {
      ssl->s3->wnum = tot;
      return ret;
    }

    if (ret == (int)n || (ssl->mode & SSL_MODE_ENABLE_PARTIAL_WRITE)) {
      return tot + ret;
    }

    n -= ret;
    tot += ret;
  }
}

static int ssl3_write_pending(SSL *ssl, int type, const uint8_t *buf,
                              unsigned int len) {
  if (ssl->s3->wpend_tot > (int)len ||
      (!(ssl->mode & SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER) &&
       ssl->s3->wpend_buf != buf) ||
      ssl->s3->wpend_type != type) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_WRITE_RETRY);
    return -1;
  }

  int ret = ssl_write_buffer_flush(ssl);
  if (ret <= 0) {
    return ret;
  }
  return ssl->s3->wpend_ret;
}

/* do_ssl3_write writes an SSL record of the given type. */
static int do_ssl3_write(SSL *ssl, int type, const uint8_t *buf, unsigned len) {
  /* If there is still data from the previous record, flush it. */
  if (ssl_write_buffer_is_pending(ssl)) {
    return ssl3_write_pending(ssl, type, buf, len);
  }

  if (len > SSL3_RT_MAX_PLAIN_LENGTH) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return -1;
  }

  if (len == 0) {
    return 0;
  }

  size_t flight_len = 0;
  if (ssl->s3->pending_flight != NULL) {
    flight_len =
        ssl->s3->pending_flight->length - ssl->s3->pending_flight_offset;
  }

  size_t max_out = len + SSL_max_seal_overhead(ssl);
  if (max_out < len || max_out + flight_len < max_out) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
    return -1;
  }
  max_out += flight_len;

  uint8_t *out;
  size_t ciphertext_len;
  if (!ssl_write_buffer_init(ssl, &out, max_out)) {
    return -1;
  }

  /* Add any unflushed handshake data as a prefix. This may be a KeyUpdate
   * acknowledgment or 0-RTT key change messages. |pending_flight| must be clear
   * when data is added to |write_buffer| or it will be written in the wrong
   * order. */
  if (ssl->s3->pending_flight != NULL) {
    OPENSSL_memcpy(
        out, ssl->s3->pending_flight->data + ssl->s3->pending_flight_offset,
        flight_len);
    BUF_MEM_free(ssl->s3->pending_flight);
    ssl->s3->pending_flight = NULL;
    ssl->s3->pending_flight_offset = 0;
  }

  if (!tls_seal_record(ssl, out + flight_len, &ciphertext_len,
                       max_out - flight_len, type, buf, len)) {
    return -1;
  }
  ssl_write_buffer_set_len(ssl, flight_len + ciphertext_len);

  /* Now that we've made progress on the connection, uncork KeyUpdate
   * acknowledgments. */
  ssl->s3->key_update_pending = 0;

  /* memorize arguments so that ssl3_write_pending can detect bad write retries
   * later */
  ssl->s3->wpend_tot = len;
  ssl->s3->wpend_buf = buf;
  ssl->s3->wpend_type = type;
  ssl->s3->wpend_ret = len;

  /* we now just need to write the buffer */
  return ssl3_write_pending(ssl, type, buf, len);
}

static int consume_record(SSL *ssl, uint8_t *out, int len, int peek) {
  SSL3_RECORD *rr = &ssl->s3->rrec;

  if (len <= 0) {
    return len;
  }

  if (len > (int)rr->length) {
    len = (int)rr->length;
  }

  OPENSSL_memcpy(out, rr->data, len);
  if (!peek) {
    rr->length -= len;
    rr->data += len;
    if (rr->length == 0) {
      /* The record has been consumed, so we may now clear the buffer. */
      ssl_read_buffer_discard(ssl);
    }
  }
  return len;
}

int ssl3_read_app_data(SSL *ssl, int *out_got_handshake, uint8_t *buf, int len,
                       int peek) {
  assert(ssl_can_read(ssl));
  assert(ssl->s3->aead_read_ctx != NULL);
  *out_got_handshake = 0;

  ssl->method->release_current_message(ssl, 0 /* don't free buffer */);

  SSL3_RECORD *rr = &ssl->s3->rrec;

  for (;;) {
    /* A previous iteration may have read a partial handshake message. Do not
     * allow more app data in that case. */
    int has_hs_data = ssl->init_buf != NULL && ssl->init_buf->length > 0;

    /* Get new packet if necessary. */
    if (rr->length == 0 && !has_hs_data) {
      int ret = ssl3_get_record(ssl);
      if (ret <= 0) {
        return ret;
      }
    }

    if (has_hs_data || rr->type == SSL3_RT_HANDSHAKE) {
      /* If reading 0-RTT data, reject handshake data. 0-RTT data is terminated
       * by an alert. */
      if (SSL_in_init(ssl)) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_RECORD);
        ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
        return -1;
      }

      /* Post-handshake data prior to TLS 1.3 is always renegotiation, which we
       * never accept as a server. Otherwise |ssl3_get_message| will send
       * |SSL_R_EXCESSIVE_MESSAGE_SIZE|. */
      if (ssl->server && ssl3_protocol_version(ssl) < TLS1_3_VERSION) {
        ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_NO_RENEGOTIATION);
        OPENSSL_PUT_ERROR(SSL, SSL_R_NO_RENEGOTIATION);
        return -1;
      }

      /* Parse post-handshake handshake messages. */
      int ret = ssl3_get_message(ssl);
      if (ret <= 0) {
        return ret;
      }
      *out_got_handshake = 1;
      return -1;
    }

    const int is_early_data_read = ssl->server &&
                                   ssl->s3->hs != NULL &&
                                   ssl->s3->hs->can_early_read &&
                                   ssl3_protocol_version(ssl) >= TLS1_3_VERSION;

    /* Handle the end_of_early_data alert. */
    if (rr->type == SSL3_RT_ALERT &&
        rr->length == 2 &&
        rr->data[0] == SSL3_AL_WARNING &&
        rr->data[1] == TLS1_AD_END_OF_EARLY_DATA &&
        is_early_data_read) {
      /* Consume the record. */
      rr->length = 0;
      ssl_read_buffer_discard(ssl);
      /* Stop accepting early data. */
      ssl->s3->hs->can_early_read = 0;
      *out_got_handshake = 1;
      return -1;
    }

    if (rr->type != SSL3_RT_APPLICATION_DATA) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_RECORD);
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
      return -1;
    }

    if (is_early_data_read) {
      if (rr->length > kMaxEarlyDataAccepted - ssl->s3->hs->early_data_read) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_TOO_MUCH_READ_EARLY_DATA);
        ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL3_AD_UNEXPECTED_MESSAGE);
        return -1;
      }

      ssl->s3->hs->early_data_read += rr->length;
    }

    if (rr->length != 0) {
      return consume_record(ssl, buf, len, peek);
    }

    /* Discard empty records and loop again. */
  }
}

int ssl3_read_change_cipher_spec(SSL *ssl) {
  SSL3_RECORD *rr = &ssl->s3->rrec;

  if (rr->length == 0) {
    int ret = ssl3_get_record(ssl);
    if (ret <= 0) {
      return ret;
    }
  }

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

void ssl3_read_close_notify(SSL *ssl) {
  /* Read records until an error or close_notify. */
  while (ssl3_get_record(ssl) > 0) {
    ;
  }
}

int ssl3_read_handshake_bytes(SSL *ssl, uint8_t *buf, int len) {
  SSL3_RECORD *rr = &ssl->s3->rrec;

  for (;;) {
    /* Get new packet if necessary. */
    if (rr->length == 0) {
      int ret = ssl3_get_record(ssl);
      if (ret <= 0) {
        return ret;
      }
    }

    if (rr->type != SSL3_RT_HANDSHAKE) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_RECORD);
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
      return -1;
    }

    if (rr->length != 0) {
      return consume_record(ssl, buf, len, 0 /* consume data */);
    }

    /* Discard empty records and loop again. */
  }
}

int ssl3_send_alert(SSL *ssl, int level, int desc) {
  /* It is illegal to send an alert when we've already sent a closing one. */
  if (ssl->s3->send_shutdown != ssl_shutdown_none) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_PROTOCOL_IS_SHUTDOWN);
    return -1;
  }

  if (level == SSL3_AL_WARNING && desc == SSL_AD_CLOSE_NOTIFY) {
    ssl->s3->send_shutdown = ssl_shutdown_close_notify;
  } else {
    assert(level == SSL3_AL_FATAL);
    ssl->s3->send_shutdown = ssl_shutdown_fatal_alert;
  }

  ssl->s3->alert_dispatch = 1;
  ssl->s3->send_alert[0] = level;
  ssl->s3->send_alert[1] = desc;
  if (!ssl_write_buffer_is_pending(ssl)) {
    /* Nothing is being written out, so the alert may be dispatched
     * immediately. */
    return ssl->method->dispatch_alert(ssl);
  }

  /* The alert will be dispatched later. */
  return -1;
}

int ssl3_dispatch_alert(SSL *ssl) {
  int ret = do_ssl3_write(ssl, SSL3_RT_ALERT, &ssl->s3->send_alert[0], 2);
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
