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

#include <openssl/buf.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/md5.h>
#include <openssl/nid.h>
#include <openssl/sha.h>

#include "../crypto/internal.h"
#include "internal.h"


int SSL_TRANSCRIPT_init(SSL_TRANSCRIPT *transcript) {
  SSL_TRANSCRIPT_cleanup(transcript);
  transcript->buffer = BUF_MEM_new();
  return transcript->buffer != NULL;
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

int SSL_TRANSCRIPT_init_hash(SSL_TRANSCRIPT *transcript, uint16_t version,
                             int algorithm_prf) {
  const EVP_MD *md = ssl_get_handshake_digest(algorithm_prf, version);

  /* To support SSL 3.0's Finished and CertificateVerify constructions,
   * EVP_md5_sha1() is split into MD5 and SHA-1 halves. When SSL 3.0 is removed,
   * we can simplify this. */
  if (md == EVP_md5_sha1()) {
    if (!init_digest_with_data(&transcript->md5, EVP_md5(),
                               transcript->buffer)) {
      return 0;
    }
    md = EVP_sha1();
  }

  if (!init_digest_with_data(&transcript->hash, md, transcript->buffer)) {
    return 0;
  }

  return 1;
}

void SSL_TRANSCRIPT_cleanup(SSL_TRANSCRIPT *transcript) {
  SSL_TRANSCRIPT_free_buffer(transcript);
  EVP_MD_CTX_cleanup(&transcript->hash);
  EVP_MD_CTX_cleanup(&transcript->md5);
}

void SSL_TRANSCRIPT_free_buffer(SSL_TRANSCRIPT *transcript) {
  BUF_MEM_free(transcript->buffer);
  transcript->buffer = NULL;
}

size_t SSL_TRANSCRIPT_digest_len(const SSL_TRANSCRIPT *transcript) {
  return EVP_MD_size(SSL_TRANSCRIPT_md(transcript));
}

const EVP_MD *SSL_TRANSCRIPT_md(const SSL_TRANSCRIPT *transcript) {
  if (EVP_MD_CTX_md(&transcript->md5) != NULL) {
    return EVP_md5_sha1();
  }
  return EVP_MD_CTX_md(&transcript->hash);
}

int SSL_TRANSCRIPT_update(SSL_TRANSCRIPT *transcript, const uint8_t *in,
                          size_t in_len) {
  /* Depending on the state of the handshake, either the handshake buffer may be
   * active, the rolling hash, or both. */
  if (transcript->buffer != NULL) {
    size_t new_len = transcript->buffer->length + in_len;
    if (new_len < in_len) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
      return 0;
    }
    if (!BUF_MEM_grow(transcript->buffer, new_len)) {
      return 0;
    }
    OPENSSL_memcpy(transcript->buffer->data + new_len - in_len, in, in_len);
  }

  if (EVP_MD_CTX_md(&transcript->hash) != NULL) {
    EVP_DigestUpdate(&transcript->hash, in, in_len);
  }
  if (EVP_MD_CTX_md(&transcript->md5) != NULL) {
    EVP_DigestUpdate(&transcript->md5, in, in_len);
  }

  return 1;
}

int SSL_TRANSCRIPT_get_hash(const SSL_TRANSCRIPT *transcript, uint8_t *out,
                            size_t *out_len) {
  int ret = 0;
  EVP_MD_CTX ctx;
  EVP_MD_CTX_init(&ctx);
  unsigned md5_len = 0;
  if (EVP_MD_CTX_md(&transcript->md5) != NULL) {
    if (!EVP_MD_CTX_copy_ex(&ctx, &transcript->md5) ||
        !EVP_DigestFinal_ex(&ctx, out, &md5_len)) {
      goto err;
    }
  }

  unsigned len;
  if (!EVP_MD_CTX_copy_ex(&ctx, &transcript->hash) ||
      !EVP_DigestFinal_ex(&ctx, out + md5_len, &len)) {
    goto err;
  }

  *out_len = md5_len + len;
  ret = 1;

err:
  EVP_MD_CTX_cleanup(&ctx);
  return ret;
}

static int ssl3_handshake_mac(SSL_TRANSCRIPT *transcript,
                              const SSL_SESSION *session,
                              const EVP_MD_CTX *ctx_template,
                              const char *sender, size_t sender_len,
                              uint8_t *p, size_t *out_len) {
  unsigned int len;
  size_t npad, n;
  unsigned int i;
  uint8_t md_buf[EVP_MAX_MD_SIZE];
  EVP_MD_CTX ctx;

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
  EVP_DigestFinal_ex(&ctx, p, &len);

  EVP_MD_CTX_cleanup(&ctx);

  *out_len = len;
  return 1;
}

int SSL_TRANSCRIPT_ssl3_cert_verify_hash(SSL_TRANSCRIPT *transcript,
                                         uint8_t *out, size_t *out_len,
                                         const SSL_SESSION *session,
                                         int signature_algorithm) {
  if (SSL_TRANSCRIPT_md(transcript) != EVP_md5_sha1()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  if (signature_algorithm == SSL_SIGN_RSA_PKCS1_MD5_SHA1) {
    size_t md5_len, len;
    if (!ssl3_handshake_mac(transcript, session, &transcript->md5, NULL, 0, out,
                            &md5_len) ||
        !ssl3_handshake_mac(transcript, session, &transcript->hash, NULL, 0,
                            out + md5_len, &len)) {
      return 0;
    }
    *out_len = md5_len + len;
    return 1;
  }

  if (signature_algorithm == SSL_SIGN_ECDSA_SHA1) {
    return ssl3_handshake_mac(transcript, session, &transcript->hash, NULL, 0,
                              out, out_len);
  }

  OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
  return 0;
}

int SSL_TRANSCRIPT_finish_mac(SSL_TRANSCRIPT *transcript, uint8_t *out,
                              size_t *out_len, const SSL_SESSION *session,
                              int from_server, uint16_t version) {
  if (version == SSL3_VERSION) {
    if (SSL_TRANSCRIPT_md(transcript) != EVP_md5_sha1()) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return 0;
    }

    const char *sender = from_server ? SSL3_MD_SERVER_FINISHED_CONST
                                     : SSL3_MD_CLIENT_FINISHED_CONST;
    const size_t sender_len = 4;
    size_t md5_len, len;
    if (!ssl3_handshake_mac(transcript, session, &transcript->md5, sender,
                            sender_len, out, &md5_len) ||
        !ssl3_handshake_mac(transcript, session, &transcript->hash, sender,
                            sender_len, out + md5_len, &len)) {
      return 0;
    }

    *out_len = md5_len + len;
    return 1;
  }

  /* At this point, the handshake should have released the handshake buffer on
   * its own. */
  assert(transcript->buffer == NULL);

  const char *label = TLS_MD_CLIENT_FINISH_CONST;
  size_t label_len = TLS_MD_SERVER_FINISH_CONST_SIZE;
  if (from_server) {
    label = TLS_MD_SERVER_FINISH_CONST;
    label_len = TLS_MD_SERVER_FINISH_CONST_SIZE;
  }

  uint8_t digests[EVP_MAX_MD_SIZE];
  size_t digests_len;
  if (!SSL_TRANSCRIPT_get_hash(transcript, digests, &digests_len)) {
    return 0;
  }

  static const size_t kFinishedLen = 12;
  if (!tls1_prf(SSL_TRANSCRIPT_md(transcript), out, kFinishedLen,
                session->master_key, session->master_key_length, label,
                label_len, digests, digests_len, NULL, 0)) {
    return 0;
  }

  *out_len = kFinishedLen;
  return 1;
}
