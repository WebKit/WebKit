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

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/md5.h>
#include <openssl/nid.h>

#include "internal.h"

static int ssl3_prf(const SSL *ssl, uint8_t *out, size_t out_len,
                    const uint8_t *secret, size_t secret_len, const char *label,
                    size_t label_len, const uint8_t *seed1, size_t seed1_len,
                    const uint8_t *seed2, size_t seed2_len) {
  EVP_MD_CTX md5;
  EVP_MD_CTX sha1;
  uint8_t buf[16], smd[SHA_DIGEST_LENGTH];
  uint8_t c = 'A';
  size_t i, j, k;

  k = 0;
  EVP_MD_CTX_init(&md5);
  EVP_MD_CTX_init(&sha1);
  for (i = 0; i < out_len; i += MD5_DIGEST_LENGTH) {
    k++;
    if (k > sizeof(buf)) {
      /* bug: 'buf' is too small for this ciphersuite */
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return 0;
    }

    for (j = 0; j < k; j++) {
      buf[j] = c;
    }
    c++;
    if (!EVP_DigestInit_ex(&sha1, EVP_sha1(), NULL)) {
      OPENSSL_PUT_ERROR(SSL, ERR_LIB_EVP);
      return 0;
    }
    EVP_DigestUpdate(&sha1, buf, k);
    EVP_DigestUpdate(&sha1, secret, secret_len);
    /* |label| is ignored for SSLv3. */
    if (seed1_len) {
      EVP_DigestUpdate(&sha1, seed1, seed1_len);
    }
    if (seed2_len) {
      EVP_DigestUpdate(&sha1, seed2, seed2_len);
    }
    EVP_DigestFinal_ex(&sha1, smd, NULL);

    if (!EVP_DigestInit_ex(&md5, EVP_md5(), NULL)) {
      OPENSSL_PUT_ERROR(SSL, ERR_LIB_EVP);
      return 0;
    }
    EVP_DigestUpdate(&md5, secret, secret_len);
    EVP_DigestUpdate(&md5, smd, SHA_DIGEST_LENGTH);
    if (i + MD5_DIGEST_LENGTH > out_len) {
      EVP_DigestFinal_ex(&md5, smd, NULL);
      memcpy(out, smd, out_len - i);
    } else {
      EVP_DigestFinal_ex(&md5, out, NULL);
    }

    out += MD5_DIGEST_LENGTH;
  }

  OPENSSL_cleanse(smd, SHA_DIGEST_LENGTH);
  EVP_MD_CTX_cleanup(&md5);
  EVP_MD_CTX_cleanup(&sha1);

  return 1;
}

void ssl3_cleanup_key_block(SSL *ssl) {
  if (ssl->s3->tmp.key_block != NULL) {
    OPENSSL_cleanse(ssl->s3->tmp.key_block, ssl->s3->tmp.key_block_length);
    OPENSSL_free(ssl->s3->tmp.key_block);
    ssl->s3->tmp.key_block = NULL;
  }
  ssl->s3->tmp.key_block_length = 0;
}

int ssl3_init_handshake_buffer(SSL *ssl) {
  ssl3_free_handshake_buffer(ssl);
  ssl3_free_handshake_hash(ssl);
  ssl->s3->handshake_buffer = BUF_MEM_new();
  return ssl->s3->handshake_buffer != NULL;
}

/* init_digest_with_data calls |EVP_DigestInit_ex| on |ctx| with |md| and then
 * writes the data in |buf| to it. */
static int init_digest_with_data(EVP_MD_CTX *ctx, const EVP_MD *md,
                                 const BUF_MEM *buf) {
  if (!EVP_DigestInit_ex(ctx, md, NULL)) {
    return 0;
  }
  EVP_DigestUpdate(ctx, buf->data, buf->length);
  return 1;
}

int ssl3_init_handshake_hash(SSL *ssl) {
  ssl3_free_handshake_hash(ssl);

  uint32_t algorithm_prf = ssl_get_algorithm_prf(ssl);
  if (!init_digest_with_data(&ssl->s3->handshake_hash,
                             ssl_get_handshake_digest(algorithm_prf),
                             ssl->s3->handshake_buffer)) {
    return 0;
  }

  if (algorithm_prf == SSL_HANDSHAKE_MAC_DEFAULT &&
      !init_digest_with_data(&ssl->s3->handshake_md5, EVP_md5(),
                             ssl->s3->handshake_buffer)) {
    return 0;
  }

  return 1;
}

void ssl3_free_handshake_hash(SSL *ssl) {
  EVP_MD_CTX_cleanup(&ssl->s3->handshake_hash);
  EVP_MD_CTX_cleanup(&ssl->s3->handshake_md5);
}

void ssl3_free_handshake_buffer(SSL *ssl) {
  BUF_MEM_free(ssl->s3->handshake_buffer);
  ssl->s3->handshake_buffer = NULL;
}

int ssl3_update_handshake_hash(SSL *ssl, const uint8_t *in, size_t in_len) {
  /* Depending on the state of the handshake, either the handshake buffer may be
   * active, the rolling hash, or both. */

  if (ssl->s3->handshake_buffer != NULL) {
    size_t new_len = ssl->s3->handshake_buffer->length + in_len;
    if (new_len < in_len) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
      return 0;
    }
    if (!BUF_MEM_grow(ssl->s3->handshake_buffer, new_len)) {
      return 0;
    }
    memcpy(ssl->s3->handshake_buffer->data + new_len - in_len, in, in_len);
  }

  if (EVP_MD_CTX_md(&ssl->s3->handshake_hash) != NULL) {
    EVP_DigestUpdate(&ssl->s3->handshake_hash, in, in_len);
  }
  if (EVP_MD_CTX_md(&ssl->s3->handshake_md5) != NULL) {
    EVP_DigestUpdate(&ssl->s3->handshake_md5, in, in_len);
  }
  return 1;
}

static int ssl3_handshake_mac(SSL *ssl, int md_nid, const char *sender,
                              size_t sender_len, uint8_t *p) {
  unsigned int ret;
  size_t npad, n;
  unsigned int i;
  uint8_t md_buf[EVP_MAX_MD_SIZE];
  EVP_MD_CTX ctx;
  const EVP_MD_CTX *ctx_template;

  if (md_nid == NID_md5) {
    ctx_template = &ssl->s3->handshake_md5;
  } else if (md_nid == EVP_MD_CTX_type(&ssl->s3->handshake_hash)) {
    ctx_template = &ssl->s3->handshake_hash;
  } else {
    OPENSSL_PUT_ERROR(SSL, SSL_R_NO_REQUIRED_DIGEST);
    return 0;
  }

  EVP_MD_CTX_init(&ctx);
  if (!EVP_MD_CTX_copy_ex(&ctx, ctx_template)) {
    EVP_MD_CTX_cleanup(&ctx);
    OPENSSL_PUT_ERROR(SSL, ERR_LIB_EVP);
    return 0;
  }

  static const uint8_t kPad1[48] = {
      0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
      0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
      0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
      0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
  };

  static const uint8_t kPad2[48] = {
      0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
      0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
      0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
      0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
  };

  n = EVP_MD_CTX_size(&ctx);

  SSL_SESSION *session = ssl->session;
  if (ssl->s3->new_session != NULL) {
    session = ssl->s3->new_session;
  }

  npad = (48 / n) * n;
  if (sender != NULL) {
    EVP_DigestUpdate(&ctx, sender, sender_len);
  }
  EVP_DigestUpdate(&ctx, session->master_key, session->master_key_length);
  EVP_DigestUpdate(&ctx, kPad1, npad);
  EVP_DigestFinal_ex(&ctx, md_buf, &i);

  if (!EVP_DigestInit_ex(&ctx, EVP_MD_CTX_md(&ctx), NULL)) {
    EVP_MD_CTX_cleanup(&ctx);
    OPENSSL_PUT_ERROR(SSL, ERR_LIB_EVP);
    return 0;
  }
  EVP_DigestUpdate(&ctx, session->master_key, session->master_key_length);
  EVP_DigestUpdate(&ctx, kPad2, npad);
  EVP_DigestUpdate(&ctx, md_buf, i);
  EVP_DigestFinal_ex(&ctx, p, &ret);

  EVP_MD_CTX_cleanup(&ctx);

  return ret;
}

static int ssl3_final_finish_mac(SSL *ssl, int from_server, uint8_t *out) {
  const char *sender = from_server ? SSL3_MD_SERVER_FINISHED_CONST
                                   : SSL3_MD_CLIENT_FINISHED_CONST;
  const size_t sender_len = 4;
  int ret, sha1len;
  ret = ssl3_handshake_mac(ssl, NID_md5, sender, sender_len, out);
  if (ret == 0) {
    return 0;
  }

  out += ret;

  sha1len = ssl3_handshake_mac(ssl, NID_sha1, sender, sender_len, out);
  if (sha1len == 0) {
    return 0;
  }

  ret += sha1len;
  return ret;
}

int ssl3_cert_verify_hash(SSL *ssl, const EVP_MD **out_md, uint8_t *out,
                          size_t *out_len, uint16_t signature_algorithm) {
  assert(ssl3_protocol_version(ssl) == SSL3_VERSION);

  if (signature_algorithm == SSL_SIGN_RSA_PKCS1_MD5_SHA1) {
    if (ssl3_handshake_mac(ssl, NID_md5, NULL, 0, out) == 0 ||
        ssl3_handshake_mac(ssl, NID_sha1, NULL, 0,
                           out + MD5_DIGEST_LENGTH) == 0) {
      return 0;
    }
    *out_md = EVP_md5_sha1();
    *out_len = MD5_DIGEST_LENGTH + SHA_DIGEST_LENGTH;
  } else if (signature_algorithm == SSL_SIGN_ECDSA_SHA1) {
    if (ssl3_handshake_mac(ssl, NID_sha1, NULL, 0, out) == 0) {
      return 0;
    }
    *out_md = EVP_sha1();
    *out_len = SHA_DIGEST_LENGTH;
  } else {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  return 1;
}

const SSL3_ENC_METHOD SSLv3_enc_data = {
    ssl3_prf,
    ssl3_final_finish_mac,
};
