/*
 * DTLS implementation written by Nagendra Modadugu
 * (nagendra@cs.stanford.edu) for the OpenSSL project 2005. 
 */
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
#include <limits.h>
#include <string.h>

#include <openssl/buf.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/rand.h>
#include <openssl/type_check.h>

#include "../crypto/internal.h"
#include "internal.h"


/* TODO(davidben): 28 comes from the size of IP + UDP header. Is this reasonable
 * for these values? Notably, why is kMinMTU a function of the transport
 * protocol's overhead rather than, say, what's needed to hold a minimally-sized
 * handshake fragment plus protocol overhead. */

/* kMinMTU is the minimum acceptable MTU value. */
static const unsigned int kMinMTU = 256 - 28;

/* kDefaultMTU is the default MTU value to use if neither the user nor
 * the underlying BIO supplies one. */
static const unsigned int kDefaultMTU = 1500 - 28;


/* Receiving handshake messages. */

static void dtls1_hm_fragment_free(hm_fragment *frag) {
  if (frag == NULL) {
    return;
  }
  OPENSSL_free(frag->data);
  OPENSSL_free(frag->reassembly);
  OPENSSL_free(frag);
}

static hm_fragment *dtls1_hm_fragment_new(const struct hm_header_st *msg_hdr) {
  hm_fragment *frag = OPENSSL_malloc(sizeof(hm_fragment));
  if (frag == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return NULL;
  }
  OPENSSL_memset(frag, 0, sizeof(hm_fragment));
  frag->type = msg_hdr->type;
  frag->seq = msg_hdr->seq;
  frag->msg_len = msg_hdr->msg_len;

  /* Allocate space for the reassembled message and fill in the header. */
  frag->data = OPENSSL_malloc(DTLS1_HM_HEADER_LENGTH + msg_hdr->msg_len);
  if (frag->data == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    goto err;
  }

  CBB cbb;
  if (!CBB_init_fixed(&cbb, frag->data, DTLS1_HM_HEADER_LENGTH) ||
      !CBB_add_u8(&cbb, msg_hdr->type) ||
      !CBB_add_u24(&cbb, msg_hdr->msg_len) ||
      !CBB_add_u16(&cbb, msg_hdr->seq) ||
      !CBB_add_u24(&cbb, 0 /* frag_off */) ||
      !CBB_add_u24(&cbb, msg_hdr->msg_len) ||
      !CBB_finish(&cbb, NULL, NULL)) {
    CBB_cleanup(&cbb);
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    goto err;
  }

  /* If the handshake message is empty, |frag->reassembly| is NULL. */
  if (msg_hdr->msg_len > 0) {
    /* Initialize reassembly bitmask. */
    if (msg_hdr->msg_len + 7 < msg_hdr->msg_len) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
      goto err;
    }
    size_t bitmask_len = (msg_hdr->msg_len + 7) / 8;
    frag->reassembly = OPENSSL_malloc(bitmask_len);
    if (frag->reassembly == NULL) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
      goto err;
    }
    OPENSSL_memset(frag->reassembly, 0, bitmask_len);
  }

  return frag;

err:
  dtls1_hm_fragment_free(frag);
  return NULL;
}

/* bit_range returns a |uint8_t| with bits |start|, inclusive, to |end|,
 * exclusive, set. */
static uint8_t bit_range(size_t start, size_t end) {
  return (uint8_t)(~((1u << start) - 1) & ((1u << end) - 1));
}

/* dtls1_hm_fragment_mark marks bytes |start|, inclusive, to |end|, exclusive,
 * as received in |frag|. If |frag| becomes complete, it clears
 * |frag->reassembly|. The range must be within the bounds of |frag|'s message
 * and |frag->reassembly| must not be NULL. */
static void dtls1_hm_fragment_mark(hm_fragment *frag, size_t start,
                                   size_t end) {
  size_t msg_len = frag->msg_len;

  if (frag->reassembly == NULL || start > end || end > msg_len) {
    assert(0);
    return;
  }
  /* A zero-length message will never have a pending reassembly. */
  assert(msg_len > 0);

  if ((start >> 3) == (end >> 3)) {
    frag->reassembly[start >> 3] |= bit_range(start & 7, end & 7);
  } else {
    frag->reassembly[start >> 3] |= bit_range(start & 7, 8);
    for (size_t i = (start >> 3) + 1; i < (end >> 3); i++) {
      frag->reassembly[i] = 0xff;
    }
    if ((end & 7) != 0) {
      frag->reassembly[end >> 3] |= bit_range(0, end & 7);
    }
  }

  /* Check if the fragment is complete. */
  for (size_t i = 0; i < (msg_len >> 3); i++) {
    if (frag->reassembly[i] != 0xff) {
      return;
    }
  }
  if ((msg_len & 7) != 0 &&
      frag->reassembly[msg_len >> 3] != bit_range(0, msg_len & 7)) {
    return;
  }

  OPENSSL_free(frag->reassembly);
  frag->reassembly = NULL;
}

/* dtls1_is_current_message_complete returns one if the current handshake
 * message is complete and zero otherwise. */
static int dtls1_is_current_message_complete(const SSL *ssl) {
  hm_fragment *frag = ssl->d1->incoming_messages[ssl->d1->handshake_read_seq %
                                                 SSL_MAX_HANDSHAKE_FLIGHT];
  return frag != NULL && frag->reassembly == NULL;
}

/* dtls1_get_incoming_message returns the incoming message corresponding to
 * |msg_hdr|. If none exists, it creates a new one and inserts it in the
 * queue. Otherwise, it checks |msg_hdr| is consistent with the existing one. It
 * returns NULL on failure. The caller does not take ownership of the result. */
static hm_fragment *dtls1_get_incoming_message(
    SSL *ssl, const struct hm_header_st *msg_hdr) {
  if (msg_hdr->seq < ssl->d1->handshake_read_seq ||
      msg_hdr->seq - ssl->d1->handshake_read_seq >= SSL_MAX_HANDSHAKE_FLIGHT) {
    return NULL;
  }

  size_t idx = msg_hdr->seq % SSL_MAX_HANDSHAKE_FLIGHT;
  hm_fragment *frag = ssl->d1->incoming_messages[idx];
  if (frag != NULL) {
    assert(frag->seq == msg_hdr->seq);
    /* The new fragment must be compatible with the previous fragments from this
     * message. */
    if (frag->type != msg_hdr->type ||
        frag->msg_len != msg_hdr->msg_len) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_FRAGMENT_MISMATCH);
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
      return NULL;
    }
    return frag;
  }

  /* This is the first fragment from this message. */
  frag = dtls1_hm_fragment_new(msg_hdr);
  if (frag == NULL) {
    return NULL;
  }
  ssl->d1->incoming_messages[idx] = frag;
  return frag;
}

/* dtls1_process_handshake_record reads a handshake record and processes it. It
 * returns one if the record was successfully processed and 0 or -1 on error. */
static int dtls1_process_handshake_record(SSL *ssl) {
  SSL3_RECORD *rr = &ssl->s3->rrec;

start:
  if (rr->length == 0) {
    int ret = dtls1_get_record(ssl);
    if (ret <= 0) {
      return ret;
    }
  }

  /* Cross-epoch records are discarded, but we may receive out-of-order
   * application data between ChangeCipherSpec and Finished or a
   * ChangeCipherSpec before the appropriate point in the handshake. Those must
   * be silently discarded.
   *
   * However, only allow the out-of-order records in the correct epoch.
   * Application data must come in the encrypted epoch, and ChangeCipherSpec in
   * the unencrypted epoch (we never renegotiate). Other cases fall through and
   * fail with a fatal error. */
  if ((rr->type == SSL3_RT_APPLICATION_DATA &&
       ssl->s3->aead_read_ctx != NULL) ||
      (rr->type == SSL3_RT_CHANGE_CIPHER_SPEC &&
       ssl->s3->aead_read_ctx == NULL)) {
    rr->length = 0;
    goto start;
  }

  if (rr->type != SSL3_RT_HANDSHAKE) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_RECORD);
    return -1;
  }

  CBS cbs;
  CBS_init(&cbs, rr->data, rr->length);

  while (CBS_len(&cbs) > 0) {
    /* Read a handshake fragment. */
    struct hm_header_st msg_hdr;
    CBS body;
    if (!dtls1_parse_fragment(&cbs, &msg_hdr, &body)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_HANDSHAKE_RECORD);
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
      return -1;
    }

    const size_t frag_off = msg_hdr.frag_off;
    const size_t frag_len = msg_hdr.frag_len;
    const size_t msg_len = msg_hdr.msg_len;
    if (frag_off > msg_len || frag_off + frag_len < frag_off ||
        frag_off + frag_len > msg_len ||
        msg_len > ssl_max_handshake_message_len(ssl)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_EXCESSIVE_MESSAGE_SIZE);
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
      return -1;
    }

    /* The encrypted epoch in DTLS has only one handshake message. */
    if (ssl->d1->r_epoch == 1 && msg_hdr.seq != ssl->d1->handshake_read_seq) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_RECORD);
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
      return -1;
    }

    if (msg_hdr.seq < ssl->d1->handshake_read_seq ||
        msg_hdr.seq >
            (unsigned)ssl->d1->handshake_read_seq + SSL_MAX_HANDSHAKE_FLIGHT) {
      /* Ignore fragments from the past, or ones too far in the future. */
      continue;
    }

    hm_fragment *frag = dtls1_get_incoming_message(ssl, &msg_hdr);
    if (frag == NULL) {
      return -1;
    }
    assert(frag->msg_len == msg_len);

    if (frag->reassembly == NULL) {
      /* The message is already assembled. */
      continue;
    }
    assert(msg_len > 0);

    /* Copy the body into the fragment. */
    OPENSSL_memcpy(frag->data + DTLS1_HM_HEADER_LENGTH + frag_off,
                   CBS_data(&body), CBS_len(&body));
    dtls1_hm_fragment_mark(frag, frag_off, frag_off + frag_len);
  }

  rr->length = 0;
  ssl_read_buffer_discard(ssl);
  return 1;
}

int dtls1_get_message(SSL *ssl) {
  if (ssl->s3->tmp.reuse_message) {
    /* There must be a current message. */
    assert(ssl->init_msg != NULL);
    ssl->s3->tmp.reuse_message = 0;
  } else {
    dtls1_release_current_message(ssl, 0 /* don't free buffer */);
  }

  /* Process handshake records until the current message is ready. */
  while (!dtls1_is_current_message_complete(ssl)) {
    int ret = dtls1_process_handshake_record(ssl);
    if (ret <= 0) {
      return ret;
    }
  }

  hm_fragment *frag = ssl->d1->incoming_messages[ssl->d1->handshake_read_seq %
                                                 SSL_MAX_HANDSHAKE_FLIGHT];
  assert(frag != NULL);
  assert(frag->reassembly == NULL);
  assert(ssl->d1->handshake_read_seq == frag->seq);

  /* TODO(davidben): This function has a lot of implicit outputs. Simplify the
   * |ssl_get_message| API. */
  ssl->s3->tmp.message_type = frag->type;
  ssl->init_msg = frag->data + DTLS1_HM_HEADER_LENGTH;
  ssl->init_num = frag->msg_len;

  ssl_do_msg_callback(ssl, 0 /* read */, SSL3_RT_HANDSHAKE, frag->data,
                      ssl->init_num + DTLS1_HM_HEADER_LENGTH);
  return 1;
}

void dtls1_get_current_message(const SSL *ssl, CBS *out) {
  assert(dtls1_is_current_message_complete(ssl));

  hm_fragment *frag = ssl->d1->incoming_messages[ssl->d1->handshake_read_seq %
                                                 SSL_MAX_HANDSHAKE_FLIGHT];
  CBS_init(out, frag->data, DTLS1_HM_HEADER_LENGTH + frag->msg_len);
}

void dtls1_release_current_message(SSL *ssl, int free_buffer) {
  if (ssl->init_msg == NULL) {
    return;
  }

  assert(dtls1_is_current_message_complete(ssl));
  size_t index = ssl->d1->handshake_read_seq % SSL_MAX_HANDSHAKE_FLIGHT;
  dtls1_hm_fragment_free(ssl->d1->incoming_messages[index]);
  ssl->d1->incoming_messages[index] = NULL;
  ssl->d1->handshake_read_seq++;

  ssl->init_msg = NULL;
  ssl->init_num = 0;
}

void dtls_clear_incoming_messages(SSL *ssl) {
  for (size_t i = 0; i < SSL_MAX_HANDSHAKE_FLIGHT; i++) {
    dtls1_hm_fragment_free(ssl->d1->incoming_messages[i]);
    ssl->d1->incoming_messages[i] = NULL;
  }
}

int dtls_has_incoming_messages(const SSL *ssl) {
  size_t current = ssl->d1->handshake_read_seq % SSL_MAX_HANDSHAKE_FLIGHT;
  for (size_t i = 0; i < SSL_MAX_HANDSHAKE_FLIGHT; i++) {
    /* Skip the current message. */
    if (ssl->init_msg != NULL && i == current) {
      assert(dtls1_is_current_message_complete(ssl));
      continue;
    }
    if (ssl->d1->incoming_messages[i] != NULL) {
      return 1;
    }
  }
  return 0;
}

int dtls1_parse_fragment(CBS *cbs, struct hm_header_st *out_hdr,
                         CBS *out_body) {
  OPENSSL_memset(out_hdr, 0x00, sizeof(struct hm_header_st));

  if (!CBS_get_u8(cbs, &out_hdr->type) ||
      !CBS_get_u24(cbs, &out_hdr->msg_len) ||
      !CBS_get_u16(cbs, &out_hdr->seq) ||
      !CBS_get_u24(cbs, &out_hdr->frag_off) ||
      !CBS_get_u24(cbs, &out_hdr->frag_len) ||
      !CBS_get_bytes(cbs, out_body, out_hdr->frag_len)) {
    return 0;
  }

  return 1;
}


/* Sending handshake messages. */

void dtls_clear_outgoing_messages(SSL *ssl) {
  for (size_t i = 0; i < ssl->d1->outgoing_messages_len; i++) {
    OPENSSL_free(ssl->d1->outgoing_messages[i].data);
    ssl->d1->outgoing_messages[i].data = NULL;
  }
  ssl->d1->outgoing_messages_len = 0;
  ssl->d1->outgoing_written = 0;
  ssl->d1->outgoing_offset = 0;
}

int dtls1_init_message(SSL *ssl, CBB *cbb, CBB *body, uint8_t type) {
  /* Pick a modest size hint to save most of the |realloc| calls. */
  if (!CBB_init(cbb, 64) ||
      !CBB_add_u8(cbb, type) ||
      !CBB_add_u24(cbb, 0 /* length (filled in later) */) ||
      !CBB_add_u16(cbb, ssl->d1->handshake_write_seq) ||
      !CBB_add_u24(cbb, 0 /* offset */) ||
      !CBB_add_u24_length_prefixed(cbb, body)) {
    return 0;
  }

  return 1;
}

int dtls1_finish_message(SSL *ssl, CBB *cbb, uint8_t **out_msg,
                         size_t *out_len) {
  *out_msg = NULL;
  if (!CBB_finish(cbb, out_msg, out_len) ||
      *out_len < DTLS1_HM_HEADER_LENGTH) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    OPENSSL_free(*out_msg);
    return 0;
  }

  /* Fix up the header. Copy the fragment length into the total message
   * length. */
  OPENSSL_memcpy(*out_msg + 1, *out_msg + DTLS1_HM_HEADER_LENGTH - 3, 3);
  return 1;
}

/* add_outgoing adds a new handshake message or ChangeCipherSpec to the current
 * outgoing flight. It returns one on success and zero on error. In both cases,
 * it takes ownership of |data| and releases it with |OPENSSL_free| when
 * done. */
static int add_outgoing(SSL *ssl, int is_ccs, uint8_t *data, size_t len) {
  OPENSSL_COMPILE_ASSERT(SSL_MAX_HANDSHAKE_FLIGHT <
                             (1 << 8 * sizeof(ssl->d1->outgoing_messages_len)),
                         outgoing_messages_len_is_too_small);
  if (ssl->d1->outgoing_messages_len >= SSL_MAX_HANDSHAKE_FLIGHT) {
    assert(0);
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    OPENSSL_free(data);
    return 0;
  }

  if (!is_ccs) {
    /* TODO(svaldez): Move this up a layer to fix abstraction for SSL_TRANSCRIPT
     * on hs. */
    if (ssl->s3->hs != NULL &&
        !SSL_TRANSCRIPT_update(&ssl->s3->hs->transcript, data, len)) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      OPENSSL_free(data);
      return 0;
    }
    ssl->d1->handshake_write_seq++;
  }

  DTLS_OUTGOING_MESSAGE *msg =
      &ssl->d1->outgoing_messages[ssl->d1->outgoing_messages_len];
  msg->data = data;
  msg->len = len;
  msg->epoch = ssl->d1->w_epoch;
  msg->is_ccs = is_ccs;

  ssl->d1->outgoing_messages_len++;
  return 1;
}

int dtls1_add_message(SSL *ssl, uint8_t *data, size_t len) {
  return add_outgoing(ssl, 0 /* handshake */, data, len);
}

int dtls1_add_change_cipher_spec(SSL *ssl) {
  return add_outgoing(ssl, 1 /* ChangeCipherSpec */, NULL, 0);
}

int dtls1_add_alert(SSL *ssl, uint8_t level, uint8_t desc) {
  /* The |add_alert| path is only used for warning alerts for now, which DTLS
   * never sends. This will be implemented later once closure alerts are
   * converted. */
  assert(0);
  OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
  return 0;
}

/* dtls1_update_mtu updates the current MTU from the BIO, ensuring it is above
 * the minimum. */
static void dtls1_update_mtu(SSL *ssl) {
  /* TODO(davidben): No consumer implements |BIO_CTRL_DGRAM_SET_MTU| and the
   * only |BIO_CTRL_DGRAM_QUERY_MTU| implementation could use
   * |SSL_set_mtu|. Does this need to be so complex?  */
  if (ssl->d1->mtu < dtls1_min_mtu() &&
      !(SSL_get_options(ssl) & SSL_OP_NO_QUERY_MTU)) {
    long mtu = BIO_ctrl(ssl->wbio, BIO_CTRL_DGRAM_QUERY_MTU, 0, NULL);
    if (mtu >= 0 && mtu <= (1 << 30) && (unsigned)mtu >= dtls1_min_mtu()) {
      ssl->d1->mtu = (unsigned)mtu;
    } else {
      ssl->d1->mtu = kDefaultMTU;
      BIO_ctrl(ssl->wbio, BIO_CTRL_DGRAM_SET_MTU, ssl->d1->mtu, NULL);
    }
  }

  /* The MTU should be above the minimum now. */
  assert(ssl->d1->mtu >= dtls1_min_mtu());
}

enum seal_result_t {
  seal_error,
  seal_no_progress,
  seal_partial,
  seal_success,
};

/* seal_next_message seals |msg|, which must be the next message, to |out|. If
 * progress was made, it returns |seal_partial| or |seal_success| and sets
 * |*out_len| to the number of bytes written. */
static enum seal_result_t seal_next_message(SSL *ssl, uint8_t *out,
                                            size_t *out_len, size_t max_out,
                                            const DTLS_OUTGOING_MESSAGE *msg) {
  assert(ssl->d1->outgoing_written < ssl->d1->outgoing_messages_len);
  assert(msg == &ssl->d1->outgoing_messages[ssl->d1->outgoing_written]);

  /* DTLS renegotiation is unsupported, so only epochs 0 (NULL cipher) and 1
   * (negotiated cipher) exist. */
  assert(ssl->d1->w_epoch == 0 || ssl->d1->w_epoch == 1);
  assert(msg->epoch <= ssl->d1->w_epoch);
  enum dtls1_use_epoch_t use_epoch = dtls1_use_current_epoch;
  if (ssl->d1->w_epoch == 1 && msg->epoch == 0) {
    use_epoch = dtls1_use_previous_epoch;
  }
  size_t overhead = dtls_max_seal_overhead(ssl, use_epoch);
  size_t prefix = dtls_seal_prefix_len(ssl, use_epoch);

  if (msg->is_ccs) {
    /* Check there is room for the ChangeCipherSpec. */
    static const uint8_t kChangeCipherSpec[1] = {SSL3_MT_CCS};
    if (max_out < sizeof(kChangeCipherSpec) + overhead) {
      return seal_no_progress;
    }

    if (!dtls_seal_record(ssl, out, out_len, max_out,
                          SSL3_RT_CHANGE_CIPHER_SPEC, kChangeCipherSpec,
                          sizeof(kChangeCipherSpec), use_epoch)) {
      return seal_error;
    }

    ssl_do_msg_callback(ssl, 1 /* write */, SSL3_RT_CHANGE_CIPHER_SPEC,
                        kChangeCipherSpec, sizeof(kChangeCipherSpec));
    return seal_success;
  }

  /* DTLS messages are serialized as a single fragment in |msg|. */
  CBS cbs, body;
  struct hm_header_st hdr;
  CBS_init(&cbs, msg->data, msg->len);
  if (!dtls1_parse_fragment(&cbs, &hdr, &body) ||
      hdr.frag_off != 0 ||
      hdr.frag_len != CBS_len(&body) ||
      hdr.msg_len != CBS_len(&body) ||
      !CBS_skip(&body, ssl->d1->outgoing_offset) ||
      CBS_len(&cbs) != 0) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return seal_error;
  }

  /* Determine how much progress can be made. */
  if (max_out < DTLS1_HM_HEADER_LENGTH + 1 + overhead || max_out < prefix) {
    return seal_no_progress;
  }
  size_t todo = CBS_len(&body);
  if (todo > max_out - DTLS1_HM_HEADER_LENGTH - overhead) {
    todo = max_out - DTLS1_HM_HEADER_LENGTH - overhead;
  }

  /* Assemble a fragment, to be sealed in-place. */
  CBB cbb;
  uint8_t *frag = out + prefix;
  size_t max_frag = max_out - prefix, frag_len;
  if (!CBB_init_fixed(&cbb, frag, max_frag) ||
      !CBB_add_u8(&cbb, hdr.type) ||
      !CBB_add_u24(&cbb, hdr.msg_len) ||
      !CBB_add_u16(&cbb, hdr.seq) ||
      !CBB_add_u24(&cbb, ssl->d1->outgoing_offset) ||
      !CBB_add_u24(&cbb, todo) ||
      !CBB_add_bytes(&cbb, CBS_data(&body), todo) ||
      !CBB_finish(&cbb, NULL, &frag_len)) {
    CBB_cleanup(&cbb);
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return seal_error;
  }

  ssl_do_msg_callback(ssl, 1 /* write */, SSL3_RT_HANDSHAKE, frag, frag_len);

  if (!dtls_seal_record(ssl, out, out_len, max_out, SSL3_RT_HANDSHAKE,
                        out + prefix, frag_len, use_epoch)) {
    return seal_error;
  }

  if (todo == CBS_len(&body)) {
    /* The next message is complete. */
    ssl->d1->outgoing_offset = 0;
    return seal_success;
  }

  ssl->d1->outgoing_offset += todo;
  return seal_partial;
}

/* seal_next_packet writes as much of the next flight as possible to |out| and
 * advances |ssl->d1->outgoing_written| and |ssl->d1->outgoing_offset| as
 * appropriate. */
static int seal_next_packet(SSL *ssl, uint8_t *out, size_t *out_len,
                            size_t max_out) {
  int made_progress = 0;
  size_t total = 0;
  assert(ssl->d1->outgoing_written < ssl->d1->outgoing_messages_len);
  for (; ssl->d1->outgoing_written < ssl->d1->outgoing_messages_len;
       ssl->d1->outgoing_written++) {
    const DTLS_OUTGOING_MESSAGE *msg =
        &ssl->d1->outgoing_messages[ssl->d1->outgoing_written];
    size_t len;
    enum seal_result_t ret = seal_next_message(ssl, out, &len, max_out, msg);
    switch (ret) {
      case seal_error:
        return 0;

      case seal_no_progress:
        goto packet_full;

      case seal_partial:
      case seal_success:
        out += len;
        max_out -= len;
        total += len;
        made_progress = 1;

        if (ret == seal_partial) {
          goto packet_full;
        }
        break;
    }
  }

packet_full:
  /* The MTU was too small to make any progress. */
  if (!made_progress) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_MTU_TOO_SMALL);
    return 0;
  }

  *out_len = total;
  return 1;
}

int dtls1_flush_flight(SSL *ssl) {
  dtls1_update_mtu(ssl);

  int ret = -1;
  uint8_t *packet = OPENSSL_malloc(ssl->d1->mtu);
  if (packet == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    goto err;
  }

  while (ssl->d1->outgoing_written < ssl->d1->outgoing_messages_len) {
    uint8_t old_written = ssl->d1->outgoing_written;
    uint32_t old_offset = ssl->d1->outgoing_offset;

    size_t packet_len;
    if (!seal_next_packet(ssl, packet, &packet_len, ssl->d1->mtu)) {
      goto err;
    }

    int bio_ret = BIO_write(ssl->wbio, packet, packet_len);
    if (bio_ret <= 0) {
      /* Retry this packet the next time around. */
      ssl->d1->outgoing_written = old_written;
      ssl->d1->outgoing_offset = old_offset;
      ssl->rwstate = SSL_WRITING;
      ret = bio_ret;
      goto err;
    }
  }

  if (BIO_flush(ssl->wbio) <= 0) {
    ssl->rwstate = SSL_WRITING;
    goto err;
  }

  ret = 1;

err:
  OPENSSL_free(packet);
  return ret;
}

int dtls1_retransmit_outgoing_messages(SSL *ssl) {
  /* Rewind to the start of the flight and write it again.
   *
   * TODO(davidben): This does not allow retransmits to be resumed on
   * non-blocking write. */
  ssl->d1->outgoing_written = 0;
  ssl->d1->outgoing_offset = 0;

  return dtls1_flush_flight(ssl);
}

unsigned int dtls1_min_mtu(void) {
  return kMinMTU;
}
