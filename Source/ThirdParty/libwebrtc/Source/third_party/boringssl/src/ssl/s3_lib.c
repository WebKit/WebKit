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

#include <openssl/buf.h>
#include <openssl/dh.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/md5.h>
#include <openssl/mem.h>
#include <openssl/nid.h>

#include "internal.h"


int ssl3_supports_cipher(const SSL_CIPHER *cipher) {
  return 1;
}

void ssl3_expect_flight(SSL *ssl) {}

void ssl3_received_flight(SSL *ssl) {}

int ssl3_new(SSL *ssl) {
  SSL3_STATE *s3;

  s3 = OPENSSL_malloc(sizeof *s3);
  if (s3 == NULL) {
    goto err;
  }
  memset(s3, 0, sizeof *s3);

  EVP_MD_CTX_init(&s3->handshake_hash);
  EVP_MD_CTX_init(&s3->handshake_md5);

  ssl->s3 = s3;

  /* Set the version to the highest supported version.
   *
   * TODO(davidben): Move this field into |s3|, have it store the normalized
   * protocol version, and implement this pre-negotiation quirk in |SSL_version|
   * at the API boundary rather than in internal state. */
  ssl->version = TLS1_2_VERSION;
  return 1;
err:
  return 0;
}

void ssl3_free(SSL *ssl) {
  if (ssl == NULL || ssl->s3 == NULL) {
    return;
  }

  ssl3_cleanup_key_block(ssl);
  ssl_read_buffer_clear(ssl);
  ssl_write_buffer_clear(ssl);

  SSL_SESSION_free(ssl->s3->new_session);
  SSL_SESSION_free(ssl->s3->established_session);
  ssl3_free_handshake_buffer(ssl);
  ssl3_free_handshake_hash(ssl);
  ssl_handshake_free(ssl->s3->hs);
  OPENSSL_free(ssl->s3->next_proto_negotiated);
  OPENSSL_free(ssl->s3->alpn_selected);
  SSL_AEAD_CTX_free(ssl->s3->aead_read_ctx);
  SSL_AEAD_CTX_free(ssl->s3->aead_write_ctx);
  OPENSSL_free(ssl->s3->pending_message);

  OPENSSL_cleanse(ssl->s3, sizeof *ssl->s3);
  OPENSSL_free(ssl->s3);
  ssl->s3 = NULL;
}

struct ssl_cipher_preference_list_st *ssl_get_cipher_preferences(SSL *ssl) {
  if (ssl->cipher_list != NULL) {
    return ssl->cipher_list;
  }

  if (ssl->version >= TLS1_1_VERSION && ssl->ctx->cipher_list_tls11 != NULL) {
    return ssl->ctx->cipher_list_tls11;
  }

  if (ssl->version >= TLS1_VERSION && ssl->ctx->cipher_list_tls10 != NULL) {
    return ssl->ctx->cipher_list_tls10;
  }

  if (ssl->ctx->cipher_list != NULL) {
    return ssl->ctx->cipher_list;
  }

  return NULL;
}

const SSL_CIPHER *ssl3_choose_cipher(
    SSL *ssl, const struct ssl_early_callback_ctx *client_hello,
    const struct ssl_cipher_preference_list_st *server_pref) {
  const SSL_CIPHER *c, *ret = NULL;
  STACK_OF(SSL_CIPHER) *srvr = server_pref->ciphers, *prio, *allow;
  int ok;
  size_t cipher_index;
  uint32_t alg_k, alg_a, mask_k, mask_a;
  /* in_group_flags will either be NULL, or will point to an array of bytes
   * which indicate equal-preference groups in the |prio| stack. See the
   * comment about |in_group_flags| in the |ssl_cipher_preference_list_st|
   * struct. */
  const uint8_t *in_group_flags;
  /* group_min contains the minimal index so far found in a group, or -1 if no
   * such value exists yet. */
  int group_min = -1;

  STACK_OF(SSL_CIPHER) *clnt = ssl_parse_client_cipher_list(client_hello);
  if (clnt == NULL) {
    return NULL;
  }

  if (ssl->options & SSL_OP_CIPHER_SERVER_PREFERENCE) {
    prio = srvr;
    in_group_flags = server_pref->in_group_flags;
    allow = clnt;
  } else {
    prio = clnt;
    in_group_flags = NULL;
    allow = srvr;
  }

  ssl_get_compatible_server_ciphers(ssl, &mask_k, &mask_a);

  for (size_t i = 0; i < sk_SSL_CIPHER_num(prio); i++) {
    c = sk_SSL_CIPHER_value(prio, i);

    ok = 1;

    /* Check the TLS version. */
    if (SSL_CIPHER_get_min_version(c) > ssl3_protocol_version(ssl) ||
        SSL_CIPHER_get_max_version(c) < ssl3_protocol_version(ssl)) {
      ok = 0;
    }

    alg_k = c->algorithm_mkey;
    alg_a = c->algorithm_auth;

    ok = ok && (alg_k & mask_k) && (alg_a & mask_a);

    if (ok && sk_SSL_CIPHER_find(allow, &cipher_index, c)) {
      if (in_group_flags != NULL && in_group_flags[i] == 1) {
        /* This element of |prio| is in a group. Update the minimum index found
         * so far and continue looking. */
        if (group_min == -1 || (size_t)group_min > cipher_index) {
          group_min = cipher_index;
        }
      } else {
        if (group_min != -1 && (size_t)group_min < cipher_index) {
          cipher_index = group_min;
        }
        ret = sk_SSL_CIPHER_value(allow, cipher_index);
        break;
      }
    }

    if (in_group_flags != NULL && in_group_flags[i] == 0 && group_min != -1) {
      /* We are about to leave a group, but we found a match in it, so that's
       * our answer. */
      ret = sk_SSL_CIPHER_value(allow, group_min);
      break;
    }
  }

  sk_SSL_CIPHER_free(clnt);
  return ret;
}

/* If we are using default SHA1+MD5 algorithms switch to new SHA256 PRF and
 * handshake macs if required. */
uint32_t ssl_get_algorithm_prf(const SSL *ssl) {
  uint32_t algorithm_prf = ssl->s3->tmp.new_cipher->algorithm_prf;
  if (algorithm_prf == SSL_HANDSHAKE_MAC_DEFAULT &&
      ssl3_protocol_version(ssl) >= TLS1_2_VERSION) {
    return SSL_HANDSHAKE_MAC_SHA256;
  }
  return algorithm_prf;
}
