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
#include <limits.h>
#include <string.h>

#include <openssl/buf.h>
#include <openssl/bytestring.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/md5.h>
#include <openssl/nid.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

#include "internal.h"


SSL_HANDSHAKE *ssl_handshake_new(enum ssl_hs_wait_t (*do_handshake)(SSL *ssl)) {
  SSL_HANDSHAKE *hs = OPENSSL_malloc(sizeof(SSL_HANDSHAKE));
  if (hs == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return NULL;
  }
  memset(hs, 0, sizeof(SSL_HANDSHAKE));
  hs->do_handshake = do_handshake;
  hs->wait = ssl_hs_ok;
  return hs;
}

void ssl_handshake_free(SSL_HANDSHAKE *hs) {
  if (hs == NULL) {
    return;
  }

  OPENSSL_cleanse(hs->secret, sizeof(hs->secret));
  OPENSSL_cleanse(hs->client_traffic_secret_0,
                  sizeof(hs->client_traffic_secret_0));
  OPENSSL_cleanse(hs->server_traffic_secret_0,
                  sizeof(hs->server_traffic_secret_0));
  SSL_ECDH_CTX_cleanup(&hs->ecdh_ctx);
  OPENSSL_free(hs->cookie);
  OPENSSL_free(hs->key_share_bytes);
  OPENSSL_free(hs->public_key);
  OPENSSL_free(hs->peer_sigalgs);
  OPENSSL_free(hs->peer_supported_group_list);
  OPENSSL_free(hs->peer_key);
  OPENSSL_free(hs->server_params);
  OPENSSL_free(hs->peer_psk_identity_hint);
  sk_X509_NAME_pop_free(hs->ca_names, X509_NAME_free);
  OPENSSL_free(hs->certificate_types);
  OPENSSL_free(hs);
}

/* ssl3_do_write sends |ssl->init_buf| in records of type 'type'
 * (SSL3_RT_HANDSHAKE or SSL3_RT_CHANGE_CIPHER_SPEC). It returns 1 on success
 * and <= 0 on error. */
static int ssl3_do_write(SSL *ssl, int type, const uint8_t *data, size_t len) {
  int ret = ssl3_write_bytes(ssl, type, data, len);
  if (ret <= 0) {
    return ret;
  }

  /* ssl3_write_bytes writes the data in its entirety. */
  assert((size_t)ret == len);
  ssl_do_msg_callback(ssl, 1 /* write */, type, data, len);
  return 1;
}

int ssl3_init_message(SSL *ssl, CBB *cbb, CBB *body, uint8_t type) {
  CBB_zero(cbb);
  if (ssl->s3->pending_message != NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  /* Pick a modest size hint to save most of the |realloc| calls. */
  if (!CBB_init(cbb, 64) ||
      !CBB_add_u8(cbb, type) ||
      !CBB_add_u24_length_prefixed(cbb, body)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  return 1;
}

int ssl3_finish_message(SSL *ssl, CBB *cbb) {
  if (ssl->s3->pending_message != NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  uint8_t *msg = NULL;
  size_t len;
  if (!CBB_finish(cbb, &msg, &len) ||
      len > 0xffffffffu) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    OPENSSL_free(msg);
    return 0;
  }

  ssl3_update_handshake_hash(ssl, msg, len);

  ssl->s3->pending_message = msg;
  ssl->s3->pending_message_len = (uint32_t)len;
  return 1;
}

int ssl3_write_message(SSL *ssl) {
  if (ssl->s3->pending_message == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  int ret = ssl3_do_write(ssl, SSL3_RT_HANDSHAKE, ssl->s3->pending_message,
                          ssl->s3->pending_message_len);
  if (ret <= 0) {
    return ret;
  }

  OPENSSL_free(ssl->s3->pending_message);
  ssl->s3->pending_message = NULL;
  ssl->s3->pending_message_len = 0;
  return 1;
}

int ssl3_send_finished(SSL *ssl, int a, int b) {
  if (ssl->state == b) {
    return ssl->method->write_message(ssl);
  }

  uint8_t finished[EVP_MAX_MD_SIZE];
  size_t finished_len =
      ssl->s3->enc_method->final_finish_mac(ssl, ssl->server, finished);
  if (finished_len == 0) {
    return 0;
  }

  /* Log the master secret, if logging is enabled. */
  if (!ssl_log_secret(ssl, "CLIENT_RANDOM",
                      SSL_get_session(ssl)->master_key,
                      SSL_get_session(ssl)->master_key_length)) {
    return 0;
  }

  /* Copy the Finished so we can use it for renegotiation checks. */
  if (ssl->version != SSL3_VERSION) {
    if (finished_len > sizeof(ssl->s3->previous_client_finished) ||
        finished_len > sizeof(ssl->s3->previous_server_finished)) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return -1;
    }

    if (ssl->server) {
      memcpy(ssl->s3->previous_server_finished, finished, finished_len);
      ssl->s3->previous_server_finished_len = finished_len;
    } else {
      memcpy(ssl->s3->previous_client_finished, finished, finished_len);
      ssl->s3->previous_client_finished_len = finished_len;
    }
  }

  CBB cbb, body;
  if (!ssl->method->init_message(ssl, &cbb, &body, SSL3_MT_FINISHED) ||
      !CBB_add_bytes(&body, finished, finished_len) ||
      !ssl->method->finish_message(ssl, &cbb)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    CBB_cleanup(&cbb);
    return -1;
  }

  ssl->state = b;
  return ssl->method->write_message(ssl);
}

int ssl3_get_finished(SSL *ssl) {
  int ret = ssl->method->ssl_get_message(ssl, SSL3_MT_FINISHED,
                                         ssl_dont_hash_message);
  if (ret <= 0) {
    return ret;
  }

  /* Snapshot the finished hash before incorporating the new message. */
  uint8_t finished[EVP_MAX_MD_SIZE];
  size_t finished_len =
      ssl->s3->enc_method->final_finish_mac(ssl, !ssl->server, finished);
  if (finished_len == 0 ||
      !ssl->method->hash_current_message(ssl)) {
    return -1;
  }

  int finished_ok = ssl->init_num == finished_len &&
                    CRYPTO_memcmp(ssl->init_msg, finished, finished_len) == 0;
#if defined(BORINGSSL_UNSAFE_FUZZER_MODE)
  finished_ok = 1;
#endif
  if (!finished_ok) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECRYPT_ERROR);
    OPENSSL_PUT_ERROR(SSL, SSL_R_DIGEST_CHECK_FAILED);
    return -1;
  }

  /* Copy the Finished so we can use it for renegotiation checks. */
  if (ssl->version != SSL3_VERSION) {
    if (finished_len > sizeof(ssl->s3->previous_client_finished) ||
        finished_len > sizeof(ssl->s3->previous_server_finished)) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return -1;
    }

    if (ssl->server) {
      memcpy(ssl->s3->previous_client_finished, finished, finished_len);
      ssl->s3->previous_client_finished_len = finished_len;
    } else {
      memcpy(ssl->s3->previous_server_finished, finished, finished_len);
      ssl->s3->previous_server_finished_len = finished_len;
    }
  }

  return 1;
}

int ssl3_send_change_cipher_spec(SSL *ssl) {
  static const uint8_t kChangeCipherSpec[1] = {SSL3_MT_CCS};

  return ssl3_do_write(ssl, SSL3_RT_CHANGE_CIPHER_SPEC, kChangeCipherSpec,
                       sizeof(kChangeCipherSpec));
}

int ssl3_output_cert_chain(SSL *ssl) {
  CBB cbb, body;
  if (!ssl->method->init_message(ssl, &cbb, &body, SSL3_MT_CERTIFICATE) ||
      !ssl_add_cert_chain(ssl, &body) ||
      !ssl->method->finish_message(ssl, &cbb)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    CBB_cleanup(&cbb);
    return 0;
  }

  return 1;
}

size_t ssl_max_handshake_message_len(const SSL *ssl) {
  /* kMaxMessageLen is the default maximum message size for handshakes which do
   * not accept peer certificate chains. */
  static const size_t kMaxMessageLen = 16384;

  if (SSL_in_init(ssl)) {
    if ((!ssl->server || (ssl->verify_mode & SSL_VERIFY_PEER)) &&
        kMaxMessageLen < ssl->max_cert_list) {
      return ssl->max_cert_list;
    }
    return kMaxMessageLen;
  }

  if (ssl3_protocol_version(ssl) < TLS1_3_VERSION) {
    /* In TLS 1.2 and below, the largest acceptable post-handshake message is
     * a HelloRequest. */
    return 0;
  }

  if (ssl->server) {
    /* The largest acceptable post-handshake message for a server is a
     * KeyUpdate. We will never initiate post-handshake auth. */
    return 0;
  }

  /* Clients must accept NewSessionTicket and CertificateRequest, so allow the
   * default size. */
  return kMaxMessageLen;
}

static int extend_handshake_buffer(SSL *ssl, size_t length) {
  if (!BUF_MEM_reserve(ssl->init_buf, length)) {
    return -1;
  }
  while (ssl->init_buf->length < length) {
    int ret = ssl3_read_handshake_bytes(
        ssl, (uint8_t *)ssl->init_buf->data + ssl->init_buf->length,
        length - ssl->init_buf->length);
    if (ret <= 0) {
      return ret;
    }
    ssl->init_buf->length += (size_t)ret;
  }
  return 1;
}

static int read_v2_client_hello(SSL *ssl, int *out_is_v2_client_hello) {
  /* Read the first 5 bytes, the size of the TLS record header. This is
   * sufficient to detect a V2ClientHello and ensures that we never read beyond
   * the first record. */
  int ret = ssl_read_buffer_extend_to(ssl, SSL3_RT_HEADER_LENGTH);
  if (ret <= 0) {
    return ret;
  }
  const uint8_t *p = ssl_read_buffer(ssl);

  /* Some dedicated error codes for protocol mixups should the application wish
   * to interpret them differently. (These do not overlap with ClientHello or
   * V2ClientHello.) */
  if (strncmp("GET ", (const char *)p, 4) == 0 ||
      strncmp("POST ", (const char *)p, 5) == 0 ||
      strncmp("HEAD ", (const char *)p, 5) == 0 ||
      strncmp("PUT ", (const char *)p, 4) == 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_HTTP_REQUEST);
    return -1;
  }
  if (strncmp("CONNE", (const char *)p, 5) == 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_HTTPS_PROXY_REQUEST);
    return -1;
  }

  if ((p[0] & 0x80) == 0 || p[2] != SSL2_MT_CLIENT_HELLO ||
      p[3] != SSL3_VERSION_MAJOR) {
    /* Not a V2ClientHello. */
    *out_is_v2_client_hello = 0;
    return 1;
  }

  /* Determine the length of the V2ClientHello. */
  size_t msg_length = ((p[0] & 0x7f) << 8) | p[1];
  if (msg_length > (1024 * 4)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_RECORD_TOO_LARGE);
    return -1;
  }
  if (msg_length < SSL3_RT_HEADER_LENGTH - 2) {
    /* Reject lengths that are too short early. We have already read
     * |SSL3_RT_HEADER_LENGTH| bytes, so we should not attempt to process an
     * (invalid) V2ClientHello which would be shorter than that. */
    OPENSSL_PUT_ERROR(SSL, SSL_R_RECORD_LENGTH_MISMATCH);
    return -1;
  }

  /* Read the remainder of the V2ClientHello. */
  ret = ssl_read_buffer_extend_to(ssl, 2 + msg_length);
  if (ret <= 0) {
    return ret;
  }

  CBS v2_client_hello;
  CBS_init(&v2_client_hello, ssl_read_buffer(ssl) + 2, msg_length);

  /* The V2ClientHello without the length is incorporated into the handshake
   * hash. */
  if (!ssl3_update_handshake_hash(ssl, CBS_data(&v2_client_hello),
                                  CBS_len(&v2_client_hello))) {
    return -1;
  }

  ssl_do_msg_callback(ssl, 0 /* read */, 0 /* V2ClientHello */,
                      CBS_data(&v2_client_hello), CBS_len(&v2_client_hello));

  uint8_t msg_type;
  uint16_t version, cipher_spec_length, session_id_length, challenge_length;
  CBS cipher_specs, session_id, challenge;
  if (!CBS_get_u8(&v2_client_hello, &msg_type) ||
      !CBS_get_u16(&v2_client_hello, &version) ||
      !CBS_get_u16(&v2_client_hello, &cipher_spec_length) ||
      !CBS_get_u16(&v2_client_hello, &session_id_length) ||
      !CBS_get_u16(&v2_client_hello, &challenge_length) ||
      !CBS_get_bytes(&v2_client_hello, &cipher_specs, cipher_spec_length) ||
      !CBS_get_bytes(&v2_client_hello, &session_id, session_id_length) ||
      !CBS_get_bytes(&v2_client_hello, &challenge, challenge_length) ||
      CBS_len(&v2_client_hello) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return -1;
  }

  /* msg_type has already been checked. */
  assert(msg_type == SSL2_MT_CLIENT_HELLO);

  /* The client_random is the V2ClientHello challenge. Truncate or
   * left-pad with zeros as needed. */
  size_t rand_len = CBS_len(&challenge);
  if (rand_len > SSL3_RANDOM_SIZE) {
    rand_len = SSL3_RANDOM_SIZE;
  }
  uint8_t random[SSL3_RANDOM_SIZE];
  memset(random, 0, SSL3_RANDOM_SIZE);
  memcpy(random + (SSL3_RANDOM_SIZE - rand_len), CBS_data(&challenge),
         rand_len);

  /* Write out an equivalent SSLv3 ClientHello. */
  size_t max_v3_client_hello = SSL3_HM_HEADER_LENGTH + 2 /* version */ +
                               SSL3_RANDOM_SIZE + 1 /* session ID length */ +
                               2 /* cipher list length */ +
                               CBS_len(&cipher_specs) / 3 * 2 +
                               1 /* compression length */ + 1 /* compression */;
  CBB client_hello, hello_body, cipher_suites;
  CBB_zero(&client_hello);
  if (!BUF_MEM_reserve(ssl->init_buf, max_v3_client_hello) ||
      !CBB_init_fixed(&client_hello, (uint8_t *)ssl->init_buf->data,
                      ssl->init_buf->max) ||
      !CBB_add_u8(&client_hello, SSL3_MT_CLIENT_HELLO) ||
      !CBB_add_u24_length_prefixed(&client_hello, &hello_body) ||
      !CBB_add_u16(&hello_body, version) ||
      !CBB_add_bytes(&hello_body, random, SSL3_RANDOM_SIZE) ||
      /* No session id. */
      !CBB_add_u8(&hello_body, 0) ||
      !CBB_add_u16_length_prefixed(&hello_body, &cipher_suites)) {
    CBB_cleanup(&client_hello);
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return -1;
  }

  /* Copy the cipher suites. */
  while (CBS_len(&cipher_specs) > 0) {
    uint32_t cipher_spec;
    if (!CBS_get_u24(&cipher_specs, &cipher_spec)) {
      CBB_cleanup(&client_hello);
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      return -1;
    }

    /* Skip SSLv2 ciphers. */
    if ((cipher_spec & 0xff0000) != 0) {
      continue;
    }
    if (!CBB_add_u16(&cipher_suites, cipher_spec)) {
      CBB_cleanup(&client_hello);
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return -1;
    }
  }

  /* Add the null compression scheme and finish. */
  if (!CBB_add_u8(&hello_body, 1) || !CBB_add_u8(&hello_body, 0) ||
      !CBB_finish(&client_hello, NULL, &ssl->init_buf->length)) {
    CBB_cleanup(&client_hello);
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return -1;
  }

  /* Consume and discard the V2ClientHello. */
  ssl_read_buffer_consume(ssl, 2 + msg_length);
  ssl_read_buffer_discard(ssl);

  *out_is_v2_client_hello = 1;
  return 1;
}

int ssl3_get_message(SSL *ssl, int msg_type,
                     enum ssl_hash_message_t hash_message) {
again:
  /* Re-create the handshake buffer if needed. */
  if (ssl->init_buf == NULL) {
    ssl->init_buf = BUF_MEM_new();
    if (ssl->init_buf == NULL) {
      return -1;
    }
  }

  if (ssl->server && !ssl->s3->v2_hello_done) {
    /* Bypass the record layer for the first message to handle V2ClientHello. */
    assert(hash_message == ssl_hash_message);
    int is_v2_client_hello = 0;
    int ret = read_v2_client_hello(ssl, &is_v2_client_hello);
    if (ret <= 0) {
      return ret;
    }
    if (is_v2_client_hello) {
      /* V2ClientHello is hashed separately. */
      hash_message = ssl_dont_hash_message;
    }
    ssl->s3->v2_hello_done = 1;
  }

  if (ssl->s3->tmp.reuse_message) {
    /* A ssl_dont_hash_message call cannot be combined with reuse_message; the
     * ssl_dont_hash_message would have to have been applied to the previous
     * call. */
    assert(hash_message == ssl_hash_message);
    assert(ssl->init_msg != NULL);

    ssl->s3->tmp.reuse_message = 0;
    hash_message = ssl_dont_hash_message;
  } else {
    ssl3_release_current_message(ssl, 0 /* don't free buffer */);
  }

  /* Read the message header, if we haven't yet. */
  int ret = extend_handshake_buffer(ssl, SSL3_HM_HEADER_LENGTH);
  if (ret <= 0) {
    return ret;
  }

  /* Parse out the length. Cap it so the peer cannot force us to buffer up to
   * 2^24 bytes. */
  const uint8_t *p = (uint8_t *)ssl->init_buf->data;
  size_t msg_len = (((uint32_t)p[1]) << 16) | (((uint32_t)p[2]) << 8) | p[3];
  if (msg_len > ssl_max_handshake_message_len(ssl)) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
    OPENSSL_PUT_ERROR(SSL, SSL_R_EXCESSIVE_MESSAGE_SIZE);
    return -1;
  }

  /* Read the message body, if we haven't yet. */
  ret = extend_handshake_buffer(ssl, SSL3_HM_HEADER_LENGTH + msg_len);
  if (ret <= 0) {
    return ret;
  }

  /* We have now received a complete message. */
  ssl_do_msg_callback(ssl, 0 /* read */, SSL3_RT_HANDSHAKE, ssl->init_buf->data,
                      ssl->init_buf->length);

  ssl->s3->tmp.message_type = ((const uint8_t *)ssl->init_buf->data)[0];
  ssl->init_msg = (uint8_t*)ssl->init_buf->data + SSL3_HM_HEADER_LENGTH;
  ssl->init_num = ssl->init_buf->length - SSL3_HM_HEADER_LENGTH;

  /* Ignore stray HelloRequest messages in the handshake before TLS 1.3. Per RFC
   * 5246, section 7.4.1.1, the server may send HelloRequest at any time. */
  if (!ssl->server && SSL_in_init(ssl) &&
      (!ssl->s3->have_version || ssl3_protocol_version(ssl) < TLS1_3_VERSION) &&
      ssl->s3->tmp.message_type == SSL3_MT_HELLO_REQUEST &&
      ssl->init_num == 0) {
    goto again;
  }

  if (msg_type >= 0 && ssl->s3->tmp.message_type != msg_type) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_MESSAGE);
    return -1;
  }

  /* Feed this message into MAC computation. */
  if (hash_message == ssl_hash_message && !ssl3_hash_current_message(ssl)) {
    return -1;
  }

  return 1;
}

int ssl3_hash_current_message(SSL *ssl) {
  return ssl3_update_handshake_hash(ssl, (uint8_t *)ssl->init_buf->data,
                                    ssl->init_buf->length);
}

void ssl3_release_current_message(SSL *ssl, int free_buffer) {
  if (ssl->init_msg != NULL) {
    /* |init_buf| never contains data beyond the current message. */
    assert(SSL3_HM_HEADER_LENGTH + ssl->init_num == ssl->init_buf->length);

    /* Clear the current message. */
    ssl->init_msg = NULL;
    ssl->init_num = 0;
    ssl->init_buf->length = 0;
  }

  if (free_buffer) {
    BUF_MEM_free(ssl->init_buf);
    ssl->init_buf = NULL;
  }
}

int ssl_verify_alarm_type(long type) {
  int al;

  switch (type) {
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
    case X509_V_ERR_UNABLE_TO_GET_CRL:
    case X509_V_ERR_UNABLE_TO_GET_CRL_ISSUER:
      al = SSL_AD_UNKNOWN_CA;
      break;

    case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:
    case X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE:
    case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY:
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
    case X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD:
    case X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD:
    case X509_V_ERR_CERT_NOT_YET_VALID:
    case X509_V_ERR_CRL_NOT_YET_VALID:
    case X509_V_ERR_CERT_UNTRUSTED:
    case X509_V_ERR_CERT_REJECTED:
    case X509_V_ERR_HOSTNAME_MISMATCH:
    case X509_V_ERR_EMAIL_MISMATCH:
    case X509_V_ERR_IP_ADDRESS_MISMATCH:
      al = SSL_AD_BAD_CERTIFICATE;
      break;

    case X509_V_ERR_CERT_SIGNATURE_FAILURE:
    case X509_V_ERR_CRL_SIGNATURE_FAILURE:
      al = SSL_AD_DECRYPT_ERROR;
      break;

    case X509_V_ERR_CERT_HAS_EXPIRED:
    case X509_V_ERR_CRL_HAS_EXPIRED:
      al = SSL_AD_CERTIFICATE_EXPIRED;
      break;

    case X509_V_ERR_CERT_REVOKED:
      al = SSL_AD_CERTIFICATE_REVOKED;
      break;

    case X509_V_ERR_UNSPECIFIED:
    case X509_V_ERR_OUT_OF_MEM:
    case X509_V_ERR_INVALID_CALL:
    case X509_V_ERR_STORE_LOOKUP:
      al = SSL_AD_INTERNAL_ERROR;
      break;

    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
    case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
    case X509_V_ERR_CERT_CHAIN_TOO_LONG:
    case X509_V_ERR_PATH_LENGTH_EXCEEDED:
    case X509_V_ERR_INVALID_CA:
      al = SSL_AD_UNKNOWN_CA;
      break;

    case X509_V_ERR_APPLICATION_VERIFICATION:
      al = SSL_AD_HANDSHAKE_FAILURE;
      break;

    case X509_V_ERR_INVALID_PURPOSE:
      al = SSL_AD_UNSUPPORTED_CERTIFICATE;
      break;

    default:
      al = SSL_AD_CERTIFICATE_UNKNOWN;
      break;
  }

  return al;
}
