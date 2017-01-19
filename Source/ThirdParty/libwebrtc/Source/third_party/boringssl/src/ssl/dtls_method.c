/*
 * DTLS implementation written by Nagendra Modadugu
 * (nagendra@cs.stanford.edu) for the OpenSSL project 2005. 
 */
/* ====================================================================
 * Copyright (c) 1999-2005 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
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
#include <string.h>

#include <openssl/buf.h>
#include <openssl/err.h>

#include "internal.h"


static int dtls1_version_from_wire(uint16_t *out_version,
                                   uint16_t wire_version) {
  switch (wire_version) {
    case DTLS1_VERSION:
      /* DTLS 1.0 maps to TLS 1.1, not TLS 1.0. */
      *out_version = TLS1_1_VERSION;
      return 1;
    case DTLS1_2_VERSION:
      *out_version = TLS1_2_VERSION;
      return 1;
  }

  return 0;
}

static uint16_t dtls1_version_to_wire(uint16_t version) {
  switch (version) {
    case TLS1_1_VERSION:
      /* DTLS 1.0 maps to TLS 1.1, not TLS 1.0. */
      return DTLS1_VERSION;
    case TLS1_2_VERSION:
      return DTLS1_2_VERSION;
  }

  /* It is an error to use this function with an invalid version. */
  assert(0);
  return 0;
}

static int dtls1_set_read_state(SSL *ssl, SSL_AEAD_CTX *aead_ctx) {
  /* Cipher changes are illegal when there are buffered incoming messages. */
  if (dtls_has_incoming_messages(ssl)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_BUFFERED_MESSAGES_ON_CIPHER_CHANGE);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
    SSL_AEAD_CTX_free(aead_ctx);
    return 0;
  }

  ssl->d1->r_epoch++;
  memset(&ssl->d1->bitmap, 0, sizeof(ssl->d1->bitmap));
  memset(ssl->s3->read_sequence, 0, sizeof(ssl->s3->read_sequence));

  SSL_AEAD_CTX_free(ssl->s3->aead_read_ctx);
  ssl->s3->aead_read_ctx = aead_ctx;
  return 1;
}

static int dtls1_set_write_state(SSL *ssl, SSL_AEAD_CTX *aead_ctx) {
  ssl->d1->w_epoch++;
  memcpy(ssl->d1->last_write_sequence, ssl->s3->write_sequence,
         sizeof(ssl->s3->write_sequence));
  memset(ssl->s3->write_sequence, 0, sizeof(ssl->s3->write_sequence));

  SSL_AEAD_CTX_free(ssl->s3->aead_write_ctx);
  ssl->s3->aead_write_ctx = aead_ctx;
  return 1;
}

static const SSL_PROTOCOL_METHOD kDTLSProtocolMethod = {
    1 /* is_dtls */,
    TLS1_1_VERSION,
    TLS1_2_VERSION,
    dtls1_version_from_wire,
    dtls1_version_to_wire,
    dtls1_new,
    dtls1_free,
    dtls1_get_message,
    dtls1_hash_current_message,
    dtls1_release_current_message,
    dtls1_read_app_data,
    dtls1_read_change_cipher_spec,
    dtls1_read_close_notify,
    dtls1_write_app_data,
    dtls1_dispatch_alert,
    dtls1_supports_cipher,
    dtls1_init_message,
    dtls1_finish_message,
    dtls1_write_message,
    dtls1_send_change_cipher_spec,
    dtls1_expect_flight,
    dtls1_received_flight,
    dtls1_set_read_state,
    dtls1_set_write_state,
};

const SSL_METHOD *DTLS_method(void) {
  static const SSL_METHOD kMethod = {
      0,
      &kDTLSProtocolMethod,
  };
  return &kMethod;
}

/* Legacy version-locked methods. */

const SSL_METHOD *DTLSv1_2_method(void) {
  static const SSL_METHOD kMethod = {
      DTLS1_2_VERSION,
      &kDTLSProtocolMethod,
  };
  return &kMethod;
}

const SSL_METHOD *DTLSv1_method(void) {
  static const SSL_METHOD kMethod = {
      DTLS1_VERSION,
      &kDTLSProtocolMethod,
  };
  return &kMethod;
}

/* Legacy side-specific methods. */

const SSL_METHOD *DTLSv1_2_server_method(void) {
  return DTLSv1_2_method();
}

const SSL_METHOD *DTLSv1_server_method(void) {
  return DTLSv1_method();
}

const SSL_METHOD *DTLSv1_2_client_method(void) {
  return DTLSv1_2_method();
}

const SSL_METHOD *DTLSv1_client_method(void) {
  return DTLSv1_method();
}

const SSL_METHOD *DTLS_server_method(void) {
  return DTLS_method();
}

const SSL_METHOD *DTLS_client_method(void) {
  return DTLS_method();
}
