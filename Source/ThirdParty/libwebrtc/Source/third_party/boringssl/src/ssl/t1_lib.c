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
 * Hudson (tjh@cryptsoft.com). */

#include <openssl/ssl.h>

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/bytestring.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/mem.h>
#include <openssl/nid.h>
#include <openssl/rand.h>
#include <openssl/type_check.h>

#include "internal.h"
#include "../crypto/internal.h"


static int ssl_check_clienthello_tlsext(SSL *ssl);
static int ssl_check_serverhello_tlsext(SSL *ssl);

static int compare_uint16_t(const void *p1, const void *p2) {
  uint16_t u1 = *((const uint16_t *)p1);
  uint16_t u2 = *((const uint16_t *)p2);
  if (u1 < u2) {
    return -1;
  } else if (u1 > u2) {
    return 1;
  } else {
    return 0;
  }
}

/* Per http://tools.ietf.org/html/rfc5246#section-7.4.1.4, there may not be
 * more than one extension of the same type in a ClientHello or ServerHello.
 * This function does an initial scan over the extensions block to filter those
 * out. */
static int tls1_check_duplicate_extensions(const CBS *cbs) {
  CBS extensions = *cbs;
  size_t num_extensions = 0, i = 0;
  uint16_t *extension_types = NULL;
  int ret = 0;

  /* First pass: count the extensions. */
  while (CBS_len(&extensions) > 0) {
    uint16_t type;
    CBS extension;

    if (!CBS_get_u16(&extensions, &type) ||
        !CBS_get_u16_length_prefixed(&extensions, &extension)) {
      goto done;
    }

    num_extensions++;
  }

  if (num_extensions == 0) {
    return 1;
  }

  extension_types = OPENSSL_malloc(sizeof(uint16_t) * num_extensions);
  if (extension_types == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    goto done;
  }

  /* Second pass: gather the extension types. */
  extensions = *cbs;
  for (i = 0; i < num_extensions; i++) {
    CBS extension;

    if (!CBS_get_u16(&extensions, &extension_types[i]) ||
        !CBS_get_u16_length_prefixed(&extensions, &extension)) {
      /* This should not happen. */
      goto done;
    }
  }
  assert(CBS_len(&extensions) == 0);

  /* Sort the extensions and make sure there are no duplicates. */
  qsort(extension_types, num_extensions, sizeof(uint16_t), compare_uint16_t);
  for (i = 1; i < num_extensions; i++) {
    if (extension_types[i - 1] == extension_types[i]) {
      goto done;
    }
  }

  ret = 1;

done:
  OPENSSL_free(extension_types);
  return ret;
}

int ssl_early_callback_init(SSL *ssl, struct ssl_early_callback_ctx *ctx,
                            const uint8_t *in, size_t in_len) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->ssl = ssl;
  ctx->client_hello = in;
  ctx->client_hello_len = in_len;

  CBS client_hello, random, session_id;
  CBS_init(&client_hello, ctx->client_hello, ctx->client_hello_len);
  if (!CBS_get_u16(&client_hello, &ctx->version) ||
      !CBS_get_bytes(&client_hello, &random, SSL3_RANDOM_SIZE) ||
      !CBS_get_u8_length_prefixed(&client_hello, &session_id) ||
      CBS_len(&session_id) > SSL_MAX_SSL_SESSION_ID_LENGTH) {
    return 0;
  }

  ctx->random = CBS_data(&random);
  ctx->random_len = CBS_len(&random);
  ctx->session_id = CBS_data(&session_id);
  ctx->session_id_len = CBS_len(&session_id);

  /* Skip past DTLS cookie */
  if (SSL_is_dtls(ctx->ssl)) {
    CBS cookie;
    if (!CBS_get_u8_length_prefixed(&client_hello, &cookie) ||
        CBS_len(&cookie) > DTLS1_COOKIE_LENGTH) {
      return 0;
    }
  }

  CBS cipher_suites, compression_methods;
  if (!CBS_get_u16_length_prefixed(&client_hello, &cipher_suites) ||
      CBS_len(&cipher_suites) < 2 || (CBS_len(&cipher_suites) & 1) != 0 ||
      !CBS_get_u8_length_prefixed(&client_hello, &compression_methods) ||
      CBS_len(&compression_methods) < 1) {
    return 0;
  }

  ctx->cipher_suites = CBS_data(&cipher_suites);
  ctx->cipher_suites_len = CBS_len(&cipher_suites);
  ctx->compression_methods = CBS_data(&compression_methods);
  ctx->compression_methods_len = CBS_len(&compression_methods);

  /* If the ClientHello ends here then it's valid, but doesn't have any
   * extensions. (E.g. SSLv3.) */
  if (CBS_len(&client_hello) == 0) {
    ctx->extensions = NULL;
    ctx->extensions_len = 0;
    return 1;
  }

  /* Extract extensions and check it is valid. */
  CBS extensions;
  if (!CBS_get_u16_length_prefixed(&client_hello, &extensions) ||
      !tls1_check_duplicate_extensions(&extensions) ||
      CBS_len(&client_hello) != 0) {
    return 0;
  }

  ctx->extensions = CBS_data(&extensions);
  ctx->extensions_len = CBS_len(&extensions);

  return 1;
}

int ssl_early_callback_get_extension(const struct ssl_early_callback_ctx *ctx,
                                     CBS *out, uint16_t extension_type) {
  CBS extensions;
  CBS_init(&extensions, ctx->extensions, ctx->extensions_len);
  while (CBS_len(&extensions) != 0) {
    /* Decode the next extension. */
    uint16_t type;
    CBS extension;
    if (!CBS_get_u16(&extensions, &type) ||
        !CBS_get_u16_length_prefixed(&extensions, &extension)) {
      return 0;
    }

    if (type == extension_type) {
      *out = extension;
      return 1;
    }
  }

  return 0;
}

int SSL_early_callback_ctx_extension_get(
    const struct ssl_early_callback_ctx *ctx, uint16_t extension_type,
    const uint8_t **out_data, size_t *out_len) {
  CBS cbs;
  if (!ssl_early_callback_get_extension(ctx, &cbs, extension_type)) {
    return 0;
  }

  *out_data = CBS_data(&cbs);
  *out_len = CBS_len(&cbs);
  return 1;
}

static const uint16_t kDefaultGroups[] = {
    SSL_CURVE_X25519,
    SSL_CURVE_SECP256R1,
    SSL_CURVE_SECP384R1,
#if defined(BORINGSSL_ANDROID_SYSTEM)
    SSL_CURVE_SECP521R1,
#endif
};

void tls1_get_grouplist(SSL *ssl, const uint16_t **out_group_ids,
                        size_t *out_group_ids_len) {
  *out_group_ids = ssl->supported_group_list;
  *out_group_ids_len = ssl->supported_group_list_len;
  if (!*out_group_ids) {
    *out_group_ids = kDefaultGroups;
    *out_group_ids_len = OPENSSL_ARRAY_SIZE(kDefaultGroups);
  }
}

int tls1_get_shared_group(SSL *ssl, uint16_t *out_group_id) {
  assert(ssl->server);

  const uint16_t *groups, *pref, *supp;
  size_t groups_len, pref_len, supp_len;
  tls1_get_grouplist(ssl, &groups, &groups_len);

  /* Clients are not required to send a supported_groups extension. In this
   * case, the server is free to pick any group it likes. See RFC 4492,
   * section 4, paragraph 3.
   *
   * However, in the interests of compatibility, we will skip ECDH if the
   * client didn't send an extension because we can't be sure that they'll
   * support our favoured group. Thus we do not special-case an emtpy
   * |peer_supported_group_list|. */

  if (ssl->options & SSL_OP_CIPHER_SERVER_PREFERENCE) {
    pref = groups;
    pref_len = groups_len;
    supp = ssl->s3->hs->peer_supported_group_list;
    supp_len = ssl->s3->hs->peer_supported_group_list_len;
  } else {
    pref = ssl->s3->hs->peer_supported_group_list;
    pref_len = ssl->s3->hs->peer_supported_group_list_len;
    supp = groups;
    supp_len = groups_len;
  }

  for (size_t i = 0; i < pref_len; i++) {
    for (size_t j = 0; j < supp_len; j++) {
      if (pref[i] == supp[j]) {
        *out_group_id = pref[i];
        return 1;
      }
    }
  }

  return 0;
}

int tls1_set_curves(uint16_t **out_group_ids, size_t *out_group_ids_len,
                    const int *curves, size_t ncurves) {
  uint16_t *group_ids;

  group_ids = OPENSSL_malloc(ncurves * sizeof(uint16_t));
  if (group_ids == NULL) {
    return 0;
  }

  for (size_t i = 0; i < ncurves; i++) {
    if (!ssl_nid_to_group_id(&group_ids[i], curves[i])) {
      OPENSSL_free(group_ids);
      return 0;
    }
  }

  OPENSSL_free(*out_group_ids);
  *out_group_ids = group_ids;
  *out_group_ids_len = ncurves;

  return 1;
}

int tls1_set_curves_list(uint16_t **out_group_ids, size_t *out_group_ids_len,
                         const char *curves) {
  uint16_t *group_ids = NULL;
  size_t ncurves = 0;

  const char *col;
  const char *ptr = curves;

  do {
    col = strchr(ptr, ':');

    uint16_t group_id;
    if (!ssl_name_to_group_id(&group_id, ptr,
                              col ? (size_t)(col - ptr) : strlen(ptr))) {
      goto err;
    }

    uint16_t *new_group_ids = OPENSSL_realloc(group_ids,
                                              (ncurves + 1) * sizeof(uint16_t));
    if (new_group_ids == NULL) {
      goto err;
    }
    group_ids = new_group_ids;

    group_ids[ncurves] = group_id;
    ncurves++;

    if (col) {
      ptr = col + 1;
    }
  } while (col);

  OPENSSL_free(*out_group_ids);
  *out_group_ids = group_ids;
  *out_group_ids_len = ncurves;

  return 1;

err:
  OPENSSL_free(group_ids);
  return 0;
}

int tls1_check_group_id(SSL *ssl, uint16_t group_id) {
  const uint16_t *groups;
  size_t groups_len;
  tls1_get_grouplist(ssl, &groups, &groups_len);
  for (size_t i = 0; i < groups_len; i++) {
    if (groups[i] == group_id) {
      return 1;
    }
  }

  return 0;
}

/* kVerifySignatureAlgorithms is the default list of accepted signature
 * algorithms for verifying. */
static const uint16_t kVerifySignatureAlgorithms[] = {
    /* For now, do not enable RSA-PSS signature algorithms on Android's system
     * BoringSSL. Once TLS 1.3 is finalized and the change in Chrome has stuck,
     * restore them. */
#if !defined(BORINGSSL_ANDROID_SYSTEM)
    SSL_SIGN_RSA_PSS_SHA512,
#endif
    SSL_SIGN_RSA_PKCS1_SHA512,
    /* TODO(davidben): Remove this entry and SSL_CURVE_SECP521R1 from
     * kDefaultGroups. */
#if defined(BORINGSSL_ANDROID_SYSTEM)
    SSL_SIGN_ECDSA_SECP521R1_SHA512,
#endif

#if !defined(BORINGSSL_ANDROID_SYSTEM)
    SSL_SIGN_RSA_PSS_SHA384,
#endif
    SSL_SIGN_RSA_PKCS1_SHA384,
    SSL_SIGN_ECDSA_SECP384R1_SHA384,

#if !defined(BORINGSSL_ANDROID_SYSTEM)
    SSL_SIGN_RSA_PSS_SHA256,
#endif
    SSL_SIGN_RSA_PKCS1_SHA256,
    SSL_SIGN_ECDSA_SECP256R1_SHA256,

    SSL_SIGN_RSA_PKCS1_SHA1,
};

/* kSignSignatureAlgorithms is the default list of supported signature
 * algorithms for signing. */
static const uint16_t kSignSignatureAlgorithms[] = {
    /* For now, do not enable RSA-PSS signature algorithms on Android's system
     * BoringSSL. Once TLS 1.3 is finalized and the change in Chrome has stuck,
     * restore them. */
#if !defined(BORINGSSL_ANDROID_SYSTEM)
    SSL_SIGN_RSA_PSS_SHA512,
#endif
    SSL_SIGN_RSA_PKCS1_SHA512,
    SSL_SIGN_ECDSA_SECP521R1_SHA512,

#if !defined(BORINGSSL_ANDROID_SYSTEM)
    SSL_SIGN_RSA_PSS_SHA384,
#endif
    SSL_SIGN_RSA_PKCS1_SHA384,
    SSL_SIGN_ECDSA_SECP384R1_SHA384,

#if !defined(BORINGSSL_ANDROID_SYSTEM)
    SSL_SIGN_RSA_PSS_SHA256,
#endif
    SSL_SIGN_RSA_PKCS1_SHA256,
    SSL_SIGN_ECDSA_SECP256R1_SHA256,

    SSL_SIGN_RSA_PKCS1_SHA1,
    SSL_SIGN_ECDSA_SHA1,
};

size_t tls12_get_verify_sigalgs(const SSL *ssl, const uint16_t **out) {
  *out = kVerifySignatureAlgorithms;
  return OPENSSL_ARRAY_SIZE(kVerifySignatureAlgorithms);
}

int tls12_check_peer_sigalg(SSL *ssl, int *out_alert, uint16_t sigalg) {
  const uint16_t *verify_sigalgs;
  size_t num_verify_sigalgs = tls12_get_verify_sigalgs(ssl, &verify_sigalgs);
  for (size_t i = 0; i < num_verify_sigalgs; i++) {
    if (sigalg == verify_sigalgs[i]) {
      return 1;
    }
  }

  OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_SIGNATURE_TYPE);
  *out_alert = SSL_AD_ILLEGAL_PARAMETER;
  return 0;
}

/* Get a mask of disabled algorithms: an algorithm is disabled if it isn't
 * supported or doesn't appear in supported signature algorithms. Unlike
 * ssl_cipher_get_disabled this applies to a specific session and not global
 * settings. */
void ssl_set_client_disabled(SSL *ssl) {
  CERT *c = ssl->cert;
  int have_rsa = 0, have_ecdsa = 0;
  c->mask_a = 0;
  c->mask_k = 0;

  /* Now go through all signature algorithms seeing if we support any for RSA or
   * ECDSA. Do this for all versions not just TLS 1.2. */
  const uint16_t *sigalgs;
  size_t num_sigalgs = tls12_get_verify_sigalgs(ssl, &sigalgs);
  for (size_t i = 0; i < num_sigalgs; i++) {
    switch (sigalgs[i]) {
      case SSL_SIGN_RSA_PSS_SHA512:
      case SSL_SIGN_RSA_PSS_SHA384:
      case SSL_SIGN_RSA_PSS_SHA256:
      case SSL_SIGN_RSA_PKCS1_SHA512:
      case SSL_SIGN_RSA_PKCS1_SHA384:
      case SSL_SIGN_RSA_PKCS1_SHA256:
      case SSL_SIGN_RSA_PKCS1_SHA1:
        have_rsa = 1;
        break;

      case SSL_SIGN_ECDSA_SECP521R1_SHA512:
      case SSL_SIGN_ECDSA_SECP384R1_SHA384:
      case SSL_SIGN_ECDSA_SECP256R1_SHA256:
      case SSL_SIGN_ECDSA_SHA1:
        have_ecdsa = 1;
        break;
    }
  }

  /* Disable auth if we don't include any appropriate signature algorithms. */
  if (!have_rsa) {
    c->mask_a |= SSL_aRSA;
  }
  if (!have_ecdsa) {
    c->mask_a |= SSL_aECDSA;
  }

  /* with PSK there must be client callback set */
  if (!ssl->psk_client_callback) {
    c->mask_a |= SSL_aPSK;
    c->mask_k |= SSL_kPSK;
  }
}

/* tls_extension represents a TLS extension that is handled internally. The
 * |init| function is called for each handshake, before any other functions of
 * the extension. Then the add and parse callbacks are called as needed.
 *
 * The parse callbacks receive a |CBS| that contains the contents of the
 * extension (i.e. not including the type and length bytes). If an extension is
 * not received then the parse callbacks will be called with a NULL CBS so that
 * they can do any processing needed to handle the absence of an extension.
 *
 * The add callbacks receive a |CBB| to which the extension can be appended but
 * the function is responsible for appending the type and length bytes too.
 *
 * All callbacks return one for success and zero for error. If a parse function
 * returns zero then a fatal alert with value |*out_alert| will be sent. If
 * |*out_alert| isn't set, then a |decode_error| alert will be sent. */
struct tls_extension {
  uint16_t value;
  void (*init)(SSL *ssl);

  int (*add_clienthello)(SSL *ssl, CBB *out);
  int (*parse_serverhello)(SSL *ssl, uint8_t *out_alert, CBS *contents);

  int (*parse_clienthello)(SSL *ssl, uint8_t *out_alert, CBS *contents);
  int (*add_serverhello)(SSL *ssl, CBB *out);
};

static int forbid_parse_serverhello(SSL *ssl, uint8_t *out_alert, CBS *contents) {
  if (contents != NULL) {
    /* Servers MUST NOT send this extension. */
    *out_alert = SSL_AD_UNSUPPORTED_EXTENSION;
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_EXTENSION);
    return 0;
  }

  return 1;
}

static int ignore_parse_clienthello(SSL *ssl, uint8_t *out_alert, CBS *contents) {
  /* This extension from the client is handled elsewhere. */
  return 1;
}

static int dont_add_serverhello(SSL *ssl, CBB *out) {
  return 1;
}

/* Server name indication (SNI).
 *
 * https://tools.ietf.org/html/rfc6066#section-3. */

static int ext_sni_add_clienthello(SSL *ssl, CBB *out) {
  if (ssl->tlsext_hostname == NULL) {
    return 1;
  }

  CBB contents, server_name_list, name;
  if (!CBB_add_u16(out, TLSEXT_TYPE_server_name) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &server_name_list) ||
      !CBB_add_u8(&server_name_list, TLSEXT_NAMETYPE_host_name) ||
      !CBB_add_u16_length_prefixed(&server_name_list, &name) ||
      !CBB_add_bytes(&name, (const uint8_t *)ssl->tlsext_hostname,
                     strlen(ssl->tlsext_hostname)) ||
      !CBB_flush(out)) {
    return 0;
  }

  return 1;
}

static int ext_sni_parse_serverhello(SSL *ssl, uint8_t *out_alert,
                                     CBS *contents) {
  if (contents == NULL) {
    return 1;
  }

  if (CBS_len(contents) != 0) {
    return 0;
  }

  assert(ssl->tlsext_hostname != NULL);

  if (ssl->session == NULL) {
    assert(ssl->s3->new_session->tlsext_hostname == NULL);
    ssl->s3->new_session->tlsext_hostname = BUF_strdup(ssl->tlsext_hostname);
    if (!ssl->s3->new_session->tlsext_hostname) {
      *out_alert = SSL_AD_INTERNAL_ERROR;
      return 0;
    }
  }

  return 1;
}

static int ext_sni_parse_clienthello(SSL *ssl, uint8_t *out_alert,
                                     CBS *contents) {
  if (contents == NULL) {
    return 1;
  }

  CBS server_name_list, host_name;
  uint8_t name_type;
  if (!CBS_get_u16_length_prefixed(contents, &server_name_list) ||
      !CBS_get_u8(&server_name_list, &name_type) ||
      /* Although the server_name extension was intended to be extensible to
       * new name types and multiple names, OpenSSL 1.0.x had a bug which meant
       * different name types will cause an error. Further, RFC 4366 originally
       * defined syntax inextensibly. RFC 6066 corrected this mistake, but
       * adding new name types is no longer feasible.
       *
       * Act as if the extensibility does not exist to simplify parsing. */
      !CBS_get_u16_length_prefixed(&server_name_list, &host_name) ||
      CBS_len(&server_name_list) != 0 ||
      CBS_len(contents) != 0) {
    return 0;
  }

  if (name_type != TLSEXT_NAMETYPE_host_name ||
      CBS_len(&host_name) == 0 ||
      CBS_len(&host_name) > TLSEXT_MAXLEN_host_name ||
      CBS_contains_zero_byte(&host_name)) {
    *out_alert = SSL_AD_UNRECOGNIZED_NAME;
    return 0;
  }

  /* TODO(davidben): SNI should be resolved before resumption. We have the
   * early callback as a replacement, but we should fix the current callback
   * and avoid the need for |SSL_CTX_set_session_id_context|. */
  if (ssl->session == NULL) {
    assert(ssl->s3->new_session->tlsext_hostname == NULL);

    /* Copy the hostname as a string. */
    if (!CBS_strdup(&host_name, &ssl->s3->new_session->tlsext_hostname)) {
      *out_alert = SSL_AD_INTERNAL_ERROR;
      return 0;
    }

    ssl->s3->hs->should_ack_sni = 1;
  }

  return 1;
}

static int ext_sni_add_serverhello(SSL *ssl, CBB *out) {
  if (ssl->session != NULL ||
      !ssl->s3->hs->should_ack_sni ||
      ssl->s3->new_session->tlsext_hostname == NULL) {
    return 1;
  }

  if (!CBB_add_u16(out, TLSEXT_TYPE_server_name) ||
      !CBB_add_u16(out, 0 /* length */)) {
    return 0;
  }

  return 1;
}


/* Renegotiation indication.
 *
 * https://tools.ietf.org/html/rfc5746 */

static int ext_ri_add_clienthello(SSL *ssl, CBB *out) {
  uint16_t min_version, max_version;
  if (!ssl_get_version_range(ssl, &min_version, &max_version)) {
    return 0;
  }

  /* Renegotiation indication is not necessary in TLS 1.3. */
  if (min_version >= TLS1_3_VERSION) {
    return 1;
  }

  assert(ssl->s3->initial_handshake_complete ==
         (ssl->s3->previous_client_finished_len != 0));

  CBB contents, prev_finished;
  if (!CBB_add_u16(out, TLSEXT_TYPE_renegotiate) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u8_length_prefixed(&contents, &prev_finished) ||
      !CBB_add_bytes(&prev_finished, ssl->s3->previous_client_finished,
                     ssl->s3->previous_client_finished_len) ||
      !CBB_flush(out)) {
    return 0;
  }

  return 1;
}

static int ext_ri_parse_serverhello(SSL *ssl, uint8_t *out_alert,
                                    CBS *contents) {
  if (contents != NULL && ssl3_protocol_version(ssl) >= TLS1_3_VERSION) {
    return 0;
  }

  /* Servers may not switch between omitting the extension and supporting it.
   * See RFC 5746, sections 3.5 and 4.2. */
  if (ssl->s3->initial_handshake_complete &&
      (contents != NULL) != ssl->s3->send_connection_binding) {
    *out_alert = SSL_AD_HANDSHAKE_FAILURE;
    OPENSSL_PUT_ERROR(SSL, SSL_R_RENEGOTIATION_MISMATCH);
    return 0;
  }

  if (contents == NULL) {
    /* Strictly speaking, if we want to avoid an attack we should *always* see
     * RI even on initial ServerHello because the client doesn't see any
     * renegotiation during an attack. However this would mean we could not
     * connect to any server which doesn't support RI.
     *
     * OpenSSL has |SSL_OP_LEGACY_SERVER_CONNECT| to control this, but in
     * practical terms every client sets it so it's just assumed here. */
    return 1;
  }

  const size_t expected_len = ssl->s3->previous_client_finished_len +
                              ssl->s3->previous_server_finished_len;

  /* Check for logic errors */
  assert(!expected_len || ssl->s3->previous_client_finished_len);
  assert(!expected_len || ssl->s3->previous_server_finished_len);
  assert(ssl->s3->initial_handshake_complete ==
         (ssl->s3->previous_client_finished_len != 0));
  assert(ssl->s3->initial_handshake_complete ==
         (ssl->s3->previous_server_finished_len != 0));

  /* Parse out the extension contents. */
  CBS renegotiated_connection;
  if (!CBS_get_u8_length_prefixed(contents, &renegotiated_connection) ||
      CBS_len(contents) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_RENEGOTIATION_ENCODING_ERR);
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return 0;
  }

  /* Check that the extension matches. */
  if (CBS_len(&renegotiated_connection) != expected_len) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_RENEGOTIATION_MISMATCH);
    *out_alert = SSL_AD_HANDSHAKE_FAILURE;
    return 0;
  }

  const uint8_t *d = CBS_data(&renegotiated_connection);
  if (CRYPTO_memcmp(d, ssl->s3->previous_client_finished,
        ssl->s3->previous_client_finished_len)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_RENEGOTIATION_MISMATCH);
    *out_alert = SSL_AD_HANDSHAKE_FAILURE;
    return 0;
  }
  d += ssl->s3->previous_client_finished_len;

  if (CRYPTO_memcmp(d, ssl->s3->previous_server_finished,
        ssl->s3->previous_server_finished_len)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_RENEGOTIATION_MISMATCH);
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return 0;
  }
  ssl->s3->send_connection_binding = 1;

  return 1;
}

static int ext_ri_parse_clienthello(SSL *ssl, uint8_t *out_alert,
                                    CBS *contents) {
  /* Renegotiation isn't supported as a server so this function should never be
   * called after the initial handshake. */
  assert(!ssl->s3->initial_handshake_complete);

  if (ssl3_protocol_version(ssl) >= TLS1_3_VERSION) {
    return 1;
  }

  if (contents == NULL) {
    return 1;
  }

  CBS renegotiated_connection;
  if (!CBS_get_u8_length_prefixed(contents, &renegotiated_connection) ||
      CBS_len(contents) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_RENEGOTIATION_ENCODING_ERR);
    return 0;
  }

  /* Check that the extension matches. We do not support renegotiation as a
   * server, so this must be empty. */
  if (CBS_len(&renegotiated_connection) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_RENEGOTIATION_MISMATCH);
    *out_alert = SSL_AD_HANDSHAKE_FAILURE;
    return 0;
  }

  ssl->s3->send_connection_binding = 1;

  return 1;
}

static int ext_ri_add_serverhello(SSL *ssl, CBB *out) {
  /* Renegotiation isn't supported as a server so this function should never be
   * called after the initial handshake. */
  assert(!ssl->s3->initial_handshake_complete);

  if (ssl3_protocol_version(ssl) >= TLS1_3_VERSION) {
    return 1;
  }

  if (!CBB_add_u16(out, TLSEXT_TYPE_renegotiate) ||
      !CBB_add_u16(out, 1 /* length */) ||
      !CBB_add_u8(out, 0 /* empty renegotiation info */)) {
    return 0;
  }

  return 1;
}


/* Extended Master Secret.
 *
 * https://tools.ietf.org/html/rfc7627 */

static int ext_ems_add_clienthello(SSL *ssl, CBB *out) {
  uint16_t min_version, max_version;
  if (!ssl_get_version_range(ssl, &min_version, &max_version)) {
    return 0;
  }

  /* Extended master secret is not necessary in TLS 1.3. */
  if (min_version >= TLS1_3_VERSION || max_version <= SSL3_VERSION) {
    return 1;
  }

  if (!CBB_add_u16(out, TLSEXT_TYPE_extended_master_secret) ||
      !CBB_add_u16(out, 0 /* length */)) {
    return 0;
  }

  return 1;
}

static int ext_ems_parse_serverhello(SSL *ssl, uint8_t *out_alert,
                                     CBS *contents) {
  /* Whether EMS is negotiated may not change on renegotation. */
  if (ssl->s3->initial_handshake_complete) {
    if ((contents != NULL) != ssl->s3->tmp.extended_master_secret) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_RENEGOTIATION_EMS_MISMATCH);
      *out_alert = SSL_AD_ILLEGAL_PARAMETER;
      return 0;
    }

    return 1;
  }

  if (contents == NULL) {
    return 1;
  }

  if (ssl3_protocol_version(ssl) >= TLS1_3_VERSION ||
      ssl->version == SSL3_VERSION) {
    return 0;
  }

  if (CBS_len(contents) != 0) {
    return 0;
  }

  ssl->s3->tmp.extended_master_secret = 1;
  return 1;
}

static int ext_ems_parse_clienthello(SSL *ssl, uint8_t *out_alert,
                                     CBS *contents) {
  if (ssl3_protocol_version(ssl) >= TLS1_3_VERSION ||
      ssl->version == SSL3_VERSION) {
    return 1;
  }

  if (contents == NULL) {
    return 1;
  }

  if (CBS_len(contents) != 0) {
    return 0;
  }

  ssl->s3->tmp.extended_master_secret = 1;
  return 1;
}

static int ext_ems_add_serverhello(SSL *ssl, CBB *out) {
  if (!ssl->s3->tmp.extended_master_secret) {
    return 1;
  }

  if (!CBB_add_u16(out, TLSEXT_TYPE_extended_master_secret) ||
      !CBB_add_u16(out, 0 /* length */)) {
    return 0;
  }

  return 1;
}


/* Session tickets.
 *
 * https://tools.ietf.org/html/rfc5077 */

static int ext_ticket_add_clienthello(SSL *ssl, CBB *out) {
  uint16_t min_version, max_version;
  if (!ssl_get_version_range(ssl, &min_version, &max_version)) {
    return 0;
  }

  /* TLS 1.3 uses a different ticket extension. */
  if (min_version >= TLS1_3_VERSION ||
      SSL_get_options(ssl) & SSL_OP_NO_TICKET) {
    return 1;
  }

  const uint8_t *ticket_data = NULL;
  int ticket_len = 0;

  /* Renegotiation does not participate in session resumption. However, still
   * advertise the extension to avoid potentially breaking servers which carry
   * over the state from the previous handshake, such as OpenSSL servers
   * without upstream's 3c3f0259238594d77264a78944d409f2127642c4. */
  uint16_t session_version;
  if (!ssl->s3->initial_handshake_complete &&
      ssl->session != NULL &&
      ssl->session->tlsext_tick != NULL &&
      /* Don't send TLS 1.3 session tickets in the ticket extension. */
      ssl->method->version_from_wire(&session_version,
                                     ssl->session->ssl_version) &&
      session_version < TLS1_3_VERSION) {
    ticket_data = ssl->session->tlsext_tick;
    ticket_len = ssl->session->tlsext_ticklen;
  }

  CBB ticket;
  if (!CBB_add_u16(out, TLSEXT_TYPE_session_ticket) ||
      !CBB_add_u16_length_prefixed(out, &ticket) ||
      !CBB_add_bytes(&ticket, ticket_data, ticket_len) ||
      !CBB_flush(out)) {
    return 0;
  }

  return 1;
}

static int ext_ticket_parse_serverhello(SSL *ssl, uint8_t *out_alert,
                                        CBS *contents) {
  ssl->tlsext_ticket_expected = 0;

  if (contents == NULL) {
    return 1;
  }

  if (ssl3_protocol_version(ssl) >= TLS1_3_VERSION) {
    return 0;
  }

  /* If |SSL_OP_NO_TICKET| is set then no extension will have been sent and
   * this function should never be called, even if the server tries to send the
   * extension. */
  assert((SSL_get_options(ssl) & SSL_OP_NO_TICKET) == 0);

  if (CBS_len(contents) != 0) {
    return 0;
  }

  ssl->tlsext_ticket_expected = 1;
  return 1;
}

static int ext_ticket_add_serverhello(SSL *ssl, CBB *out) {
  if (!ssl->tlsext_ticket_expected) {
    return 1;
  }

  /* If |SSL_OP_NO_TICKET| is set, |tlsext_ticket_expected| should never be
   * true. */
  assert((SSL_get_options(ssl) & SSL_OP_NO_TICKET) == 0);

  if (!CBB_add_u16(out, TLSEXT_TYPE_session_ticket) ||
      !CBB_add_u16(out, 0 /* length */)) {
    return 0;
  }

  return 1;
}


/* Signature Algorithms.
 *
 * https://tools.ietf.org/html/rfc5246#section-7.4.1.4.1 */

static int ext_sigalgs_add_clienthello(SSL *ssl, CBB *out) {
  uint16_t min_version, max_version;
  if (!ssl_get_version_range(ssl, &min_version, &max_version)) {
    return 0;
  }

  if (max_version < TLS1_2_VERSION) {
    return 1;
  }

  const uint16_t *sigalgs;
  const size_t num_sigalgs = tls12_get_verify_sigalgs(ssl, &sigalgs);

  CBB contents, sigalgs_cbb;
  if (!CBB_add_u16(out, TLSEXT_TYPE_signature_algorithms) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &sigalgs_cbb)) {
    return 0;
  }

  for (size_t i = 0; i < num_sigalgs; i++) {
    if (!CBB_add_u16(&sigalgs_cbb, sigalgs[i])) {
      return 0;
    }
  }

  if (!CBB_flush(out)) {
    return 0;
  }

  return 1;
}

static int ext_sigalgs_parse_clienthello(SSL *ssl, uint8_t *out_alert,
                                         CBS *contents) {
  OPENSSL_free(ssl->s3->hs->peer_sigalgs);
  ssl->s3->hs->peer_sigalgs = NULL;
  ssl->s3->hs->num_peer_sigalgs = 0;

  if (contents == NULL) {
    return 1;
  }

  CBS supported_signature_algorithms;
  if (!CBS_get_u16_length_prefixed(contents, &supported_signature_algorithms) ||
      CBS_len(contents) != 0 ||
      CBS_len(&supported_signature_algorithms) == 0 ||
      !tls1_parse_peer_sigalgs(ssl, &supported_signature_algorithms)) {
    return 0;
  }

  return 1;
}


/* OCSP Stapling.
 *
 * https://tools.ietf.org/html/rfc6066#section-8 */

static void ext_ocsp_init(SSL *ssl) {
  ssl->tlsext_status_type = -1;
}

static int ext_ocsp_add_clienthello(SSL *ssl, CBB *out) {
  if (!ssl->ocsp_stapling_enabled) {
    return 1;
  }

  CBB contents;
  if (!CBB_add_u16(out, TLSEXT_TYPE_status_request) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u8(&contents, TLSEXT_STATUSTYPE_ocsp) ||
      !CBB_add_u16(&contents, 0 /* empty responder ID list */) ||
      !CBB_add_u16(&contents, 0 /* empty request extensions */) ||
      !CBB_flush(out)) {
    return 0;
  }

  ssl->tlsext_status_type = TLSEXT_STATUSTYPE_ocsp;
  return 1;
}

static int ext_ocsp_parse_serverhello(SSL *ssl, uint8_t *out_alert,
                                      CBS *contents) {
  if (contents == NULL) {
    return 1;
  }

  if (ssl3_protocol_version(ssl) < TLS1_3_VERSION) {
    /* OCSP stapling is forbidden on non-certificate ciphers. */
    if (CBS_len(contents) != 0 ||
        !ssl_cipher_uses_certificate_auth(ssl->s3->tmp.new_cipher)) {
      return 0;
    }

    /* Note this does not check for resumption in TLS 1.2. Sending
     * status_request here does not make sense, but OpenSSL does so and the
     * specification does not say anything. Tolerate it but ignore it. */

    ssl->s3->hs->certificate_status_expected = 1;
    return 1;
  }

  /* In TLS 1.3, OCSP stapling is forbidden on resumption. */
  if (ssl->s3->session_reused) {
    return 0;
  }

  uint8_t status_type;
  CBS ocsp_response;
  if (!CBS_get_u8(contents, &status_type) ||
      status_type != TLSEXT_STATUSTYPE_ocsp ||
      !CBS_get_u24_length_prefixed(contents, &ocsp_response) ||
      CBS_len(&ocsp_response) == 0 ||
      CBS_len(contents) != 0) {
    return 0;
  }

  if (!CBS_stow(&ocsp_response, &ssl->s3->new_session->ocsp_response,
                &ssl->s3->new_session->ocsp_response_length)) {
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return 0;
  }

  return 1;
}

static int ext_ocsp_parse_clienthello(SSL *ssl, uint8_t *out_alert,
                                      CBS *contents) {
  if (contents == NULL) {
    return 1;
  }

  uint8_t status_type;
  if (!CBS_get_u8(contents, &status_type)) {
    return 0;
  }

  /* We cannot decide whether OCSP stapling will occur yet because the correct
   * SSL_CTX might not have been selected. */
  ssl->s3->hs->ocsp_stapling_requested = status_type == TLSEXT_STATUSTYPE_ocsp;

  return 1;
}

static int ext_ocsp_add_serverhello(SSL *ssl, CBB *out) {
  if (!ssl->s3->hs->ocsp_stapling_requested ||
      ssl->ctx->ocsp_response_length == 0 ||
      ssl->s3->session_reused ||
      (ssl3_protocol_version(ssl) < TLS1_3_VERSION &&
       !ssl_cipher_uses_certificate_auth(ssl->s3->tmp.new_cipher))) {
    return 1;
  }

  if (ssl3_protocol_version(ssl) < TLS1_3_VERSION) {
    /* The extension shouldn't be sent when resuming sessions. */
    if (ssl->session != NULL) {
      return 1;
    }

    ssl->s3->hs->certificate_status_expected = 1;

    return CBB_add_u16(out, TLSEXT_TYPE_status_request) &&
           CBB_add_u16(out, 0 /* length */);
  }

  CBB body, ocsp_response;
  return CBB_add_u16(out, TLSEXT_TYPE_status_request) &&
         CBB_add_u16_length_prefixed(out, &body) &&
         CBB_add_u8(&body, TLSEXT_STATUSTYPE_ocsp) &&
         CBB_add_u24_length_prefixed(&body, &ocsp_response) &&
         CBB_add_bytes(&ocsp_response, ssl->ctx->ocsp_response,
                       ssl->ctx->ocsp_response_length) &&
         CBB_flush(out);
}


/* Next protocol negotiation.
 *
 * https://htmlpreview.github.io/?https://github.com/agl/technotes/blob/master/nextprotoneg.html */

static int ext_npn_add_clienthello(SSL *ssl, CBB *out) {
  if (ssl->s3->initial_handshake_complete ||
      ssl->ctx->next_proto_select_cb == NULL ||
      (ssl->options & SSL_OP_DISABLE_NPN) ||
      SSL_is_dtls(ssl)) {
    return 1;
  }

  if (!CBB_add_u16(out, TLSEXT_TYPE_next_proto_neg) ||
      !CBB_add_u16(out, 0 /* length */)) {
    return 0;
  }

  return 1;
}

static int ext_npn_parse_serverhello(SSL *ssl, uint8_t *out_alert,
                                     CBS *contents) {
  if (contents == NULL) {
    return 1;
  }

  if (ssl3_protocol_version(ssl) >= TLS1_3_VERSION) {
    return 0;
  }

  /* If any of these are false then we should never have sent the NPN
   * extension in the ClientHello and thus this function should never have been
   * called. */
  assert(!ssl->s3->initial_handshake_complete);
  assert(!SSL_is_dtls(ssl));
  assert(ssl->ctx->next_proto_select_cb != NULL);
  assert(!(ssl->options & SSL_OP_DISABLE_NPN));

  if (ssl->s3->alpn_selected != NULL) {
    /* NPN and ALPN may not be negotiated in the same connection. */
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    OPENSSL_PUT_ERROR(SSL, SSL_R_NEGOTIATED_BOTH_NPN_AND_ALPN);
    return 0;
  }

  const uint8_t *const orig_contents = CBS_data(contents);
  const size_t orig_len = CBS_len(contents);

  while (CBS_len(contents) != 0) {
    CBS proto;
    if (!CBS_get_u8_length_prefixed(contents, &proto) ||
        CBS_len(&proto) == 0) {
      return 0;
    }
  }

  uint8_t *selected;
  uint8_t selected_len;
  if (ssl->ctx->next_proto_select_cb(
          ssl, &selected, &selected_len, orig_contents, orig_len,
          ssl->ctx->next_proto_select_cb_arg) != SSL_TLSEXT_ERR_OK) {
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return 0;
  }

  OPENSSL_free(ssl->s3->next_proto_negotiated);
  ssl->s3->next_proto_negotiated = BUF_memdup(selected, selected_len);
  if (ssl->s3->next_proto_negotiated == NULL) {
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return 0;
  }

  ssl->s3->next_proto_negotiated_len = selected_len;
  ssl->s3->hs->next_proto_neg_seen = 1;

  return 1;
}

static int ext_npn_parse_clienthello(SSL *ssl, uint8_t *out_alert,
                                     CBS *contents) {
  if (ssl3_protocol_version(ssl) >= TLS1_3_VERSION) {
    return 1;
  }

  if (contents != NULL && CBS_len(contents) != 0) {
    return 0;
  }

  if (contents == NULL ||
      ssl->s3->initial_handshake_complete ||
      /* If the ALPN extension is seen before NPN, ignore it. (If ALPN is seen
       * afterwards, parsing the ALPN extension will clear
       * |next_proto_neg_seen|. */
      ssl->s3->alpn_selected != NULL ||
      ssl->ctx->next_protos_advertised_cb == NULL ||
      SSL_is_dtls(ssl)) {
    return 1;
  }

  ssl->s3->hs->next_proto_neg_seen = 1;
  return 1;
}

static int ext_npn_add_serverhello(SSL *ssl, CBB *out) {
  /* |next_proto_neg_seen| might have been cleared when an ALPN extension was
   * parsed. */
  if (!ssl->s3->hs->next_proto_neg_seen) {
    return 1;
  }

  const uint8_t *npa;
  unsigned npa_len;

  if (ssl->ctx->next_protos_advertised_cb(
          ssl, &npa, &npa_len, ssl->ctx->next_protos_advertised_cb_arg) !=
      SSL_TLSEXT_ERR_OK) {
    ssl->s3->hs->next_proto_neg_seen = 0;
    return 1;
  }

  CBB contents;
  if (!CBB_add_u16(out, TLSEXT_TYPE_next_proto_neg) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_bytes(&contents, npa, npa_len) ||
      !CBB_flush(out)) {
    return 0;
  }

  return 1;
}


/* Signed certificate timestamps.
 *
 * https://tools.ietf.org/html/rfc6962#section-3.3.1 */

static int ext_sct_add_clienthello(SSL *ssl, CBB *out) {
  if (!ssl->signed_cert_timestamps_enabled) {
    return 1;
  }

  if (!CBB_add_u16(out, TLSEXT_TYPE_certificate_timestamp) ||
      !CBB_add_u16(out, 0 /* length */)) {
    return 0;
  }

  return 1;
}

static int ext_sct_parse_serverhello(SSL *ssl, uint8_t *out_alert,
                                     CBS *contents) {
  if (contents == NULL) {
    return 1;
  }

  /* If this is false then we should never have sent the SCT extension in the
   * ClientHello and thus this function should never have been called. */
  assert(ssl->signed_cert_timestamps_enabled);

  if (CBS_len(contents) == 0) {
    *out_alert = SSL_AD_DECODE_ERROR;
    return 0;
  }

  /* Session resumption uses the original session information. The extension
   * should not be sent on resumption, but RFC 6962 did not make it a
   * requirement, so tolerate this.
   *
   * TODO(davidben): Enforce this anyway. */
  if (!ssl->s3->session_reused &&
      !CBS_stow(
          contents,
          &ssl->s3->new_session->tlsext_signed_cert_timestamp_list,
          &ssl->s3->new_session->tlsext_signed_cert_timestamp_list_length)) {
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return 0;
  }

  return 1;
}

static int ext_sct_parse_clienthello(SSL *ssl, uint8_t *out_alert,
                                     CBS *contents) {
  return contents == NULL || CBS_len(contents) == 0;
}

static int ext_sct_add_serverhello(SSL *ssl, CBB *out) {
  /* The extension shouldn't be sent when resuming sessions. */
  if (ssl->s3->session_reused ||
      ssl->ctx->signed_cert_timestamp_list_length == 0) {
    return 1;
  }

  CBB contents;
  return CBB_add_u16(out, TLSEXT_TYPE_certificate_timestamp) &&
         CBB_add_u16_length_prefixed(out, &contents) &&
         CBB_add_bytes(&contents, ssl->ctx->signed_cert_timestamp_list,
                       ssl->ctx->signed_cert_timestamp_list_length) &&
         CBB_flush(out);
}


/* Application-level Protocol Negotiation.
 *
 * https://tools.ietf.org/html/rfc7301 */

static void ext_alpn_init(SSL *ssl) {
  OPENSSL_free(ssl->s3->alpn_selected);
  ssl->s3->alpn_selected = NULL;
}

static int ext_alpn_add_clienthello(SSL *ssl, CBB *out) {
  if (ssl->alpn_client_proto_list == NULL ||
      ssl->s3->initial_handshake_complete) {
    return 1;
  }

  CBB contents, proto_list;
  if (!CBB_add_u16(out, TLSEXT_TYPE_application_layer_protocol_negotiation) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &proto_list) ||
      !CBB_add_bytes(&proto_list, ssl->alpn_client_proto_list,
                     ssl->alpn_client_proto_list_len) ||
      !CBB_flush(out)) {
    return 0;
  }

  return 1;
}

static int ext_alpn_parse_serverhello(SSL *ssl, uint8_t *out_alert,
                                      CBS *contents) {
  if (contents == NULL) {
    return 1;
  }

  assert(!ssl->s3->initial_handshake_complete);
  assert(ssl->alpn_client_proto_list != NULL);

  if (ssl->s3->hs->next_proto_neg_seen) {
    /* NPN and ALPN may not be negotiated in the same connection. */
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    OPENSSL_PUT_ERROR(SSL, SSL_R_NEGOTIATED_BOTH_NPN_AND_ALPN);
    return 0;
  }

  /* The extension data consists of a ProtocolNameList which must have
   * exactly one ProtocolName. Each of these is length-prefixed. */
  CBS protocol_name_list, protocol_name;
  if (!CBS_get_u16_length_prefixed(contents, &protocol_name_list) ||
      CBS_len(contents) != 0 ||
      !CBS_get_u8_length_prefixed(&protocol_name_list, &protocol_name) ||
      /* Empty protocol names are forbidden. */
      CBS_len(&protocol_name) == 0 ||
      CBS_len(&protocol_name_list) != 0) {
    return 0;
  }

  /* Check that the protcol name is one of the ones we advertised. */
  int protocol_ok = 0;
  CBS client_protocol_name_list, client_protocol_name;
  CBS_init(&client_protocol_name_list, ssl->alpn_client_proto_list,
           ssl->alpn_client_proto_list_len);
  while (CBS_len(&client_protocol_name_list) > 0) {
    if (!CBS_get_u8_length_prefixed(&client_protocol_name_list,
                                    &client_protocol_name)) {
      *out_alert = SSL_AD_INTERNAL_ERROR;
      return 0;
    }

    if (CBS_len(&client_protocol_name) == CBS_len(&protocol_name) &&
        memcmp(CBS_data(&client_protocol_name), CBS_data(&protocol_name),
               CBS_len(&protocol_name)) == 0) {
      protocol_ok = 1;
      break;
    }
  }

  if (!protocol_ok) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_ALPN_PROTOCOL);
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return 0;
  }

  if (!CBS_stow(&protocol_name, &ssl->s3->alpn_selected,
                &ssl->s3->alpn_selected_len)) {
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return 0;
  }

  return 1;
}

static int ext_alpn_parse_clienthello(SSL *ssl, uint8_t *out_alert,
                                      CBS *contents) {
  if (contents == NULL) {
    return 1;
  }

  if (ssl->ctx->alpn_select_cb == NULL ||
      ssl->s3->initial_handshake_complete) {
    return 1;
  }

  /* ALPN takes precedence over NPN. */
  ssl->s3->hs->next_proto_neg_seen = 0;

  CBS protocol_name_list;
  if (!CBS_get_u16_length_prefixed(contents, &protocol_name_list) ||
      CBS_len(contents) != 0 ||
      CBS_len(&protocol_name_list) < 2) {
    return 0;
  }

  /* Validate the protocol list. */
  CBS protocol_name_list_copy = protocol_name_list;
  while (CBS_len(&protocol_name_list_copy) > 0) {
    CBS protocol_name;

    if (!CBS_get_u8_length_prefixed(&protocol_name_list_copy, &protocol_name) ||
        /* Empty protocol names are forbidden. */
        CBS_len(&protocol_name) == 0) {
      return 0;
    }
  }

  const uint8_t *selected;
  uint8_t selected_len;
  if (ssl->ctx->alpn_select_cb(
          ssl, &selected, &selected_len, CBS_data(&protocol_name_list),
          CBS_len(&protocol_name_list),
          ssl->ctx->alpn_select_cb_arg) == SSL_TLSEXT_ERR_OK) {
    OPENSSL_free(ssl->s3->alpn_selected);
    ssl->s3->alpn_selected = BUF_memdup(selected, selected_len);
    if (ssl->s3->alpn_selected == NULL) {
      *out_alert = SSL_AD_INTERNAL_ERROR;
      return 0;
    }
    ssl->s3->alpn_selected_len = selected_len;
  }

  return 1;
}

static int ext_alpn_add_serverhello(SSL *ssl, CBB *out) {
  if (ssl->s3->alpn_selected == NULL) {
    return 1;
  }

  CBB contents, proto_list, proto;
  if (!CBB_add_u16(out, TLSEXT_TYPE_application_layer_protocol_negotiation) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &proto_list) ||
      !CBB_add_u8_length_prefixed(&proto_list, &proto) ||
      !CBB_add_bytes(&proto, ssl->s3->alpn_selected,
                     ssl->s3->alpn_selected_len) ||
      !CBB_flush(out)) {
    return 0;
  }

  return 1;
}


/* Channel ID.
 *
 * https://tools.ietf.org/html/draft-balfanz-tls-channelid-01 */

static void ext_channel_id_init(SSL *ssl) {
  ssl->s3->tlsext_channel_id_valid = 0;
}

static int ext_channel_id_add_clienthello(SSL *ssl, CBB *out) {
  if (!ssl->tlsext_channel_id_enabled ||
      SSL_is_dtls(ssl)) {
    return 1;
  }

  if (!CBB_add_u16(out, TLSEXT_TYPE_channel_id) ||
      !CBB_add_u16(out, 0 /* length */)) {
    return 0;
  }

  return 1;
}

static int ext_channel_id_parse_serverhello(SSL *ssl, uint8_t *out_alert,
                                            CBS *contents) {
  if (contents == NULL) {
    return 1;
  }

  assert(!SSL_is_dtls(ssl));
  assert(ssl->tlsext_channel_id_enabled);

  if (CBS_len(contents) != 0) {
    return 0;
  }

  ssl->s3->tlsext_channel_id_valid = 1;
  return 1;
}

static int ext_channel_id_parse_clienthello(SSL *ssl, uint8_t *out_alert,
                                            CBS *contents) {
  if (contents == NULL ||
      !ssl->tlsext_channel_id_enabled ||
      SSL_is_dtls(ssl)) {
    return 1;
  }

  if (CBS_len(contents) != 0) {
    return 0;
  }

  ssl->s3->tlsext_channel_id_valid = 1;
  return 1;
}

static int ext_channel_id_add_serverhello(SSL *ssl, CBB *out) {
  if (!ssl->s3->tlsext_channel_id_valid) {
    return 1;
  }

  if (!CBB_add_u16(out, TLSEXT_TYPE_channel_id) ||
      !CBB_add_u16(out, 0 /* length */)) {
    return 0;
  }

  return 1;
}


/* Secure Real-time Transport Protocol (SRTP) extension.
 *
 * https://tools.ietf.org/html/rfc5764 */


static void ext_srtp_init(SSL *ssl) {
  ssl->srtp_profile = NULL;
}

static int ext_srtp_add_clienthello(SSL *ssl, CBB *out) {
  STACK_OF(SRTP_PROTECTION_PROFILE) *profiles = SSL_get_srtp_profiles(ssl);
  if (profiles == NULL) {
    return 1;
  }
  const size_t num_profiles = sk_SRTP_PROTECTION_PROFILE_num(profiles);
  if (num_profiles == 0) {
    return 1;
  }

  CBB contents, profile_ids;
  if (!CBB_add_u16(out, TLSEXT_TYPE_srtp) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &profile_ids)) {
    return 0;
  }

  for (size_t i = 0; i < num_profiles; i++) {
    if (!CBB_add_u16(&profile_ids,
                     sk_SRTP_PROTECTION_PROFILE_value(profiles, i)->id)) {
      return 0;
    }
  }

  if (!CBB_add_u8(&contents, 0 /* empty use_mki value */) ||
      !CBB_flush(out)) {
    return 0;
  }

  return 1;
}

static int ext_srtp_parse_serverhello(SSL *ssl, uint8_t *out_alert,
                                      CBS *contents) {
  if (contents == NULL) {
    return 1;
  }

  /* The extension consists of a u16-prefixed profile ID list containing a
   * single uint16_t profile ID, then followed by a u8-prefixed srtp_mki field.
   *
   * See https://tools.ietf.org/html/rfc5764#section-4.1.1 */
  CBS profile_ids, srtp_mki;
  uint16_t profile_id;
  if (!CBS_get_u16_length_prefixed(contents, &profile_ids) ||
      !CBS_get_u16(&profile_ids, &profile_id) ||
      CBS_len(&profile_ids) != 0 ||
      !CBS_get_u8_length_prefixed(contents, &srtp_mki) ||
      CBS_len(contents) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST);
    return 0;
  }

  if (CBS_len(&srtp_mki) != 0) {
    /* Must be no MKI, since we never offer one. */
    OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_SRTP_MKI_VALUE);
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return 0;
  }

  STACK_OF(SRTP_PROTECTION_PROFILE) *profiles = SSL_get_srtp_profiles(ssl);

  /* Check to see if the server gave us something we support (and presumably
   * offered). */
  for (size_t i = 0; i < sk_SRTP_PROTECTION_PROFILE_num(profiles); i++) {
    const SRTP_PROTECTION_PROFILE *profile =
        sk_SRTP_PROTECTION_PROFILE_value(profiles, i);

    if (profile->id == profile_id) {
      ssl->srtp_profile = profile;
      return 1;
    }
  }

  OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST);
  *out_alert = SSL_AD_ILLEGAL_PARAMETER;
  return 0;
}

static int ext_srtp_parse_clienthello(SSL *ssl, uint8_t *out_alert,
                                      CBS *contents) {
  if (contents == NULL) {
    return 1;
  }

  CBS profile_ids, srtp_mki;
  if (!CBS_get_u16_length_prefixed(contents, &profile_ids) ||
      CBS_len(&profile_ids) < 2 ||
      !CBS_get_u8_length_prefixed(contents, &srtp_mki) ||
      CBS_len(contents) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST);
    return 0;
  }
  /* Discard the MKI value for now. */

  const STACK_OF(SRTP_PROTECTION_PROFILE) *server_profiles =
      SSL_get_srtp_profiles(ssl);

  /* Pick the server's most preferred profile. */
  for (size_t i = 0; i < sk_SRTP_PROTECTION_PROFILE_num(server_profiles); i++) {
    const SRTP_PROTECTION_PROFILE *server_profile =
        sk_SRTP_PROTECTION_PROFILE_value(server_profiles, i);

    CBS profile_ids_tmp;
    CBS_init(&profile_ids_tmp, CBS_data(&profile_ids), CBS_len(&profile_ids));

    while (CBS_len(&profile_ids_tmp) > 0) {
      uint16_t profile_id;
      if (!CBS_get_u16(&profile_ids_tmp, &profile_id)) {
        return 0;
      }

      if (server_profile->id == profile_id) {
        ssl->srtp_profile = server_profile;
        return 1;
      }
    }
  }

  return 1;
}

static int ext_srtp_add_serverhello(SSL *ssl, CBB *out) {
  if (ssl->srtp_profile == NULL) {
    return 1;
  }

  CBB contents, profile_ids;
  if (!CBB_add_u16(out, TLSEXT_TYPE_srtp) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &profile_ids) ||
      !CBB_add_u16(&profile_ids, ssl->srtp_profile->id) ||
      !CBB_add_u8(&contents, 0 /* empty MKI */) ||
      !CBB_flush(out)) {
    return 0;
  }

  return 1;
}


/* EC point formats.
 *
 * https://tools.ietf.org/html/rfc4492#section-5.1.2 */

static int ssl_any_ec_cipher_suites_enabled(const SSL *ssl) {
  if (ssl->version < TLS1_VERSION && !SSL_is_dtls(ssl)) {
    return 0;
  }

  const STACK_OF(SSL_CIPHER) *cipher_stack = SSL_get_ciphers(ssl);

  for (size_t i = 0; i < sk_SSL_CIPHER_num(cipher_stack); i++) {
    const SSL_CIPHER *cipher = sk_SSL_CIPHER_value(cipher_stack, i);

    const uint32_t alg_k = cipher->algorithm_mkey;
    const uint32_t alg_a = cipher->algorithm_auth;
    if ((alg_k & SSL_kECDHE) || (alg_a & SSL_aECDSA)) {
      return 1;
    }
  }

  return 0;
}

static int ext_ec_point_add_extension(SSL *ssl, CBB *out) {
  CBB contents, formats;
  if (!CBB_add_u16(out, TLSEXT_TYPE_ec_point_formats) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u8_length_prefixed(&contents, &formats) ||
      !CBB_add_u8(&formats, TLSEXT_ECPOINTFORMAT_uncompressed) ||
      !CBB_flush(out)) {
    return 0;
  }

  return 1;
}

static int ext_ec_point_add_clienthello(SSL *ssl, CBB *out) {
  if (!ssl_any_ec_cipher_suites_enabled(ssl)) {
    return 1;
  }

  return ext_ec_point_add_extension(ssl, out);
}

static int ext_ec_point_parse_serverhello(SSL *ssl, uint8_t *out_alert,
                                          CBS *contents) {
  if (contents == NULL) {
    return 1;
  }

  if (ssl3_protocol_version(ssl) >= TLS1_3_VERSION) {
    return 0;
  }

  CBS ec_point_format_list;
  if (!CBS_get_u8_length_prefixed(contents, &ec_point_format_list) ||
      CBS_len(contents) != 0) {
    return 0;
  }

  /* Per RFC 4492, section 5.1.2, implementations MUST support the uncompressed
   * point format. */
  if (memchr(CBS_data(&ec_point_format_list), TLSEXT_ECPOINTFORMAT_uncompressed,
             CBS_len(&ec_point_format_list)) == NULL) {
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return 0;
  }

  return 1;
}

static int ext_ec_point_parse_clienthello(SSL *ssl, uint8_t *out_alert,
                                          CBS *contents) {
  if (ssl3_protocol_version(ssl) >= TLS1_3_VERSION) {
    return 1;
  }

  return ext_ec_point_parse_serverhello(ssl, out_alert, contents);
}

static int ext_ec_point_add_serverhello(SSL *ssl, CBB *out) {
  if (ssl3_protocol_version(ssl) >= TLS1_3_VERSION) {
    return 1;
  }

  const uint32_t alg_k = ssl->s3->tmp.new_cipher->algorithm_mkey;
  const uint32_t alg_a = ssl->s3->tmp.new_cipher->algorithm_auth;
  const int using_ecc = (alg_k & SSL_kECDHE) || (alg_a & SSL_aECDSA);

  if (!using_ecc) {
    return 1;
  }

  return ext_ec_point_add_extension(ssl, out);
}

/* Pre Shared Key
 *
 * https://tools.ietf.org/html/draft-ietf-tls-tls13-16#section-4.2.6 */

static int ext_pre_shared_key_add_clienthello(SSL *ssl, CBB *out) {
  uint16_t min_version, max_version;
  if (!ssl_get_version_range(ssl, &min_version, &max_version)) {
    return 0;
  }

  uint16_t session_version;
  if (max_version < TLS1_3_VERSION || ssl->session == NULL ||
      !ssl->method->version_from_wire(&session_version,
                                      ssl->session->ssl_version) ||
      session_version < TLS1_3_VERSION) {
    return 1;
  }

  CBB contents, identity, ke_modes, auth_modes, ticket;
  if (!CBB_add_u16(out, TLSEXT_TYPE_pre_shared_key) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &identity) ||
      !CBB_add_u8_length_prefixed(&identity, &ke_modes) ||
      !CBB_add_u8(&ke_modes, SSL_PSK_DHE_KE) ||
      !CBB_add_u8_length_prefixed(&identity, &auth_modes) ||
      !CBB_add_u8(&auth_modes, SSL_PSK_AUTH) ||
      !CBB_add_u16_length_prefixed(&identity, &ticket) ||
      !CBB_add_bytes(&ticket, ssl->session->tlsext_tick,
                     ssl->session->tlsext_ticklen)) {
    return 0;
  }

  return CBB_flush(out);
}

int ssl_ext_pre_shared_key_parse_serverhello(SSL *ssl, uint8_t *out_alert,
                                             CBS *contents) {
  uint16_t psk_id;
  if (!CBS_get_u16(contents, &psk_id) ||
      CBS_len(contents) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    *out_alert = SSL_AD_DECODE_ERROR;
    return 0;
  }

  if (psk_id != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_PSK_IDENTITY_NOT_FOUND);
    *out_alert = SSL_AD_UNKNOWN_PSK_IDENTITY;
    return 0;
  }

  return 1;
}

int ssl_ext_pre_shared_key_parse_clienthello(SSL *ssl,
                                             SSL_SESSION **out_session,
                                             uint8_t *out_alert,
                                             CBS *contents) {
  /* We only process the first PSK identity since we don't support pure PSK. */
  CBS identity, ke_modes, auth_modes, ticket;
  if (!CBS_get_u16_length_prefixed(contents, &identity) ||
      !CBS_get_u8_length_prefixed(&identity, &ke_modes) ||
      !CBS_get_u8_length_prefixed(&identity, &auth_modes) ||
      !CBS_get_u16_length_prefixed(&identity, &ticket) ||
      CBS_len(&identity) != 0) {
    *out_alert = SSL_AD_DECODE_ERROR;
    return 0;
  }

  /* We only support tickets with PSK_DHE_KE and PSK_AUTH. */
  if (memchr(CBS_data(&ke_modes), SSL_PSK_DHE_KE, CBS_len(&ke_modes)) == NULL ||
      memchr(CBS_data(&auth_modes), SSL_PSK_AUTH, CBS_len(&auth_modes)) ==
          NULL) {
    *out_session = NULL;
    return 1;
  }

  /* TLS 1.3 session tickets are renewed separately as part of the
   * NewSessionTicket. */
  int renew;
  return tls_process_ticket(ssl, out_session, &renew, CBS_data(&ticket),
                            CBS_len(&ticket), NULL, 0);
}

int ssl_ext_pre_shared_key_add_serverhello(SSL *ssl, CBB *out) {
  if (!ssl->s3->session_reused) {
    return 1;
  }

  CBB contents;
  if (!CBB_add_u16(out, TLSEXT_TYPE_pre_shared_key) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      /* We only consider the first identity for resumption */
      !CBB_add_u16(&contents, 0) ||
      !CBB_flush(out)) {
    return 0;
  }

  return 1;
}


/* Key Share
 *
 * https://tools.ietf.org/html/draft-ietf-tls-tls13-16#section-4.2.5 */

static int ext_key_share_add_clienthello(SSL *ssl, CBB *out) {
  uint16_t min_version, max_version;
  if (!ssl_get_version_range(ssl, &min_version, &max_version)) {
    return 0;
  }

  if (max_version < TLS1_3_VERSION) {
    return 1;
  }

  CBB contents, kse_bytes;
  if (!CBB_add_u16(out, TLSEXT_TYPE_key_share) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &kse_bytes)) {
    return 0;
  }

  uint16_t group_id;
  if (ssl->s3->hs->received_hello_retry_request) {
    /* Replay the old key shares. */
    if (!CBB_add_bytes(&kse_bytes, ssl->s3->hs->key_share_bytes,
                       ssl->s3->hs->key_share_bytes_len)) {
      return 0;
    }
    OPENSSL_free(ssl->s3->hs->key_share_bytes);
    ssl->s3->hs->key_share_bytes = NULL;
    ssl->s3->hs->key_share_bytes_len = 0;

    group_id = ssl->s3->hs->retry_group;

    /* We received a HelloRetryRequest without a new curve, so there is no new
     * share to append. Leave |ecdh_ctx| as-is. */
    if (group_id == 0) {
      return CBB_flush(out);
    }
  } else {
    /* Add a fake group. See draft-davidben-tls-grease-01. */
    if (ssl->ctx->grease_enabled &&
        (!CBB_add_u16(&kse_bytes,
                      ssl_get_grease_value(ssl, ssl_grease_group)) ||
         !CBB_add_u16(&kse_bytes, 1 /* length */) ||
         !CBB_add_u8(&kse_bytes, 0 /* one byte key share */))) {
      return 0;
    }

    /* Predict the most preferred group. */
    const uint16_t *groups;
    size_t groups_len;
    tls1_get_grouplist(ssl, &groups, &groups_len);
    if (groups_len == 0) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_NO_GROUPS_SPECIFIED);
      return 0;
    }

    group_id = groups[0];
  }

  CBB key_exchange;
  if (!CBB_add_u16(&kse_bytes, group_id) ||
      !CBB_add_u16_length_prefixed(&kse_bytes, &key_exchange) ||
      !SSL_ECDH_CTX_init(&ssl->s3->hs->ecdh_ctx, group_id) ||
      !SSL_ECDH_CTX_offer(&ssl->s3->hs->ecdh_ctx, &key_exchange) ||
      !CBB_flush(&kse_bytes)) {
    return 0;
  }

  if (!ssl->s3->hs->received_hello_retry_request) {
    /* Save the contents of the extension to repeat it in the second
     * ClientHello. */
    ssl->s3->hs->key_share_bytes_len = CBB_len(&kse_bytes);
    ssl->s3->hs->key_share_bytes = BUF_memdup(CBB_data(&kse_bytes),
                                              CBB_len(&kse_bytes));
    if (ssl->s3->hs->key_share_bytes == NULL) {
      return 0;
    }
  }

  return CBB_flush(out);
}

int ssl_ext_key_share_parse_serverhello(SSL *ssl, uint8_t **out_secret,
                                        size_t *out_secret_len,
                                        uint8_t *out_alert, CBS *contents) {
  CBS peer_key;
  uint16_t group_id;
  if (!CBS_get_u16(contents, &group_id) ||
      !CBS_get_u16_length_prefixed(contents, &peer_key) ||
      CBS_len(contents) != 0) {
    *out_alert = SSL_AD_DECODE_ERROR;
    return 0;
  }

  if (SSL_ECDH_CTX_get_id(&ssl->s3->hs->ecdh_ctx) != group_id) {
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_CURVE);
    return 0;
  }

  if (!SSL_ECDH_CTX_finish(&ssl->s3->hs->ecdh_ctx, out_secret, out_secret_len,
                           out_alert, CBS_data(&peer_key),
                           CBS_len(&peer_key))) {
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return 0;
  }

  ssl->s3->new_session->key_exchange_info = group_id;
  SSL_ECDH_CTX_cleanup(&ssl->s3->hs->ecdh_ctx);
  return 1;
}

int ssl_ext_key_share_parse_clienthello(SSL *ssl, int *out_found,
                                        uint8_t **out_secret,
                                        size_t *out_secret_len,
                                        uint8_t *out_alert, CBS *contents) {
  uint16_t group_id;
  CBS key_shares;
  if (!tls1_get_shared_group(ssl, &group_id)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_NO_SHARED_GROUP);
    *out_alert = SSL_AD_HANDSHAKE_FAILURE;
    return 0;
  }

  if (!CBS_get_u16_length_prefixed(contents, &key_shares) ||
      CBS_len(contents) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return 0;
  }

  /* Find the corresponding key share. */
  int found = 0;
  CBS peer_key;
  while (CBS_len(&key_shares) > 0) {
    uint16_t id;
    CBS peer_key_tmp;
    if (!CBS_get_u16(&key_shares, &id) ||
        !CBS_get_u16_length_prefixed(&key_shares, &peer_key_tmp)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      return 0;
    }

    if (id == group_id) {
      if (found) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_DUPLICATE_KEY_SHARE);
        *out_alert = SSL_AD_ILLEGAL_PARAMETER;
        return 0;
      }

      found = 1;
      peer_key = peer_key_tmp;
      /* Continue parsing the structure to keep peers honest. */
    }
  }

  if (!found) {
    *out_found = 0;
    *out_secret = NULL;
    *out_secret_len = 0;
    return 1;
  }

  /* Compute the DH secret. */
  uint8_t *secret = NULL;
  size_t secret_len;
  SSL_ECDH_CTX group;
  memset(&group, 0, sizeof(SSL_ECDH_CTX));
  CBB public_key;
  if (!CBB_init(&public_key, 32) ||
      !SSL_ECDH_CTX_init(&group, group_id) ||
      !SSL_ECDH_CTX_accept(&group, &public_key, &secret, &secret_len,
                           out_alert, CBS_data(&peer_key),
                           CBS_len(&peer_key)) ||
      !CBB_finish(&public_key, &ssl->s3->hs->public_key,
                  &ssl->s3->hs->public_key_len)) {
    OPENSSL_free(secret);
    SSL_ECDH_CTX_cleanup(&group);
    CBB_cleanup(&public_key);
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return 0;
  }

  SSL_ECDH_CTX_cleanup(&group);

  *out_secret = secret;
  *out_secret_len = secret_len;
  *out_found = 1;
  return 1;
}

int ssl_ext_key_share_add_serverhello(SSL *ssl, CBB *out) {
  uint16_t group_id;
  CBB kse_bytes, public_key;
  if (!tls1_get_shared_group(ssl, &group_id) ||
      !CBB_add_u16(out, TLSEXT_TYPE_key_share) ||
      !CBB_add_u16_length_prefixed(out, &kse_bytes) ||
      !CBB_add_u16(&kse_bytes, group_id) ||
      !CBB_add_u16_length_prefixed(&kse_bytes, &public_key) ||
      !CBB_add_bytes(&public_key, ssl->s3->hs->public_key,
                     ssl->s3->hs->public_key_len) ||
      !CBB_flush(out)) {
    return 0;
  }

  OPENSSL_free(ssl->s3->hs->public_key);
  ssl->s3->hs->public_key = NULL;
  ssl->s3->hs->public_key_len = 0;

  ssl->s3->new_session->key_exchange_info = group_id;
  return 1;
}


/* Supported Versions
 *
 * https://tools.ietf.org/html/draft-ietf-tls-tls13-16#section-4.2.1 */

static int ext_supported_versions_add_clienthello(SSL *ssl, CBB *out) {
  uint16_t min_version, max_version;
  if (!ssl_get_version_range(ssl, &min_version, &max_version)) {
    return 0;
  }

  if (max_version <= TLS1_2_VERSION) {
    return 1;
  }

  CBB contents, versions;
  if (!CBB_add_u16(out, TLSEXT_TYPE_supported_versions) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u8_length_prefixed(&contents, &versions)) {
    return 0;
  }

  /* Add a fake version. See draft-davidben-tls-grease-01. */
  if (ssl->ctx->grease_enabled &&
      !CBB_add_u16(&versions, ssl_get_grease_value(ssl, ssl_grease_version))) {
    return 0;
  }

  for (uint16_t version = max_version; version >= min_version; version--) {
    if (!CBB_add_u16(&versions, ssl->method->version_to_wire(version))) {
      return 0;
    }
  }

  if (!CBB_flush(out)) {
    return 0;
  }

  return 1;
}


/* Cookie
 *
 * https://tools.ietf.org/html/draft-ietf-tls-tls13-16#section-4.2.2 */

static int ext_cookie_add_clienthello(SSL *ssl, CBB *out) {
  if (ssl->s3->hs->cookie == NULL) {
    return 1;
  }

  CBB contents, cookie;
  if (!CBB_add_u16(out, TLSEXT_TYPE_cookie) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &cookie) ||
      !CBB_add_bytes(&cookie, ssl->s3->hs->cookie, ssl->s3->hs->cookie_len) ||
      !CBB_flush(out)) {
    return 0;
  }

  /* The cookie is no longer needed in memory. */
  OPENSSL_free(ssl->s3->hs->cookie);
  ssl->s3->hs->cookie = NULL;
  ssl->s3->hs->cookie_len = 0;
  return 1;
}


/* Negotiated Groups
 *
 * https://tools.ietf.org/html/rfc4492#section-5.1.2
 * https://tools.ietf.org/html/draft-ietf-tls-tls13-16#section-4.2.4 */

static int ext_supported_groups_add_clienthello(SSL *ssl, CBB *out) {
  if (!ssl_any_ec_cipher_suites_enabled(ssl)) {
    return 1;
  }

  CBB contents, groups_bytes;
  if (!CBB_add_u16(out, TLSEXT_TYPE_supported_groups) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &groups_bytes)) {
    return 0;
  }

  /* Add a fake group. See draft-davidben-tls-grease-01. */
  if (ssl->ctx->grease_enabled &&
      !CBB_add_u16(&groups_bytes,
                   ssl_get_grease_value(ssl, ssl_grease_group))) {
    return 0;
  }

  const uint16_t *groups;
  size_t groups_len;
  tls1_get_grouplist(ssl, &groups, &groups_len);

  for (size_t i = 0; i < groups_len; i++) {
    if (!CBB_add_u16(&groups_bytes, groups[i])) {
      return 0;
    }
  }

  return CBB_flush(out);
}

static int ext_supported_groups_parse_serverhello(SSL *ssl, uint8_t *out_alert,
                                                  CBS *contents) {
  /* This extension is not expected to be echoed by servers in TLS 1.2, but some
   * BigIP servers send it nonetheless, so do not enforce this. */
  return 1;
}

static int ext_supported_groups_parse_clienthello(SSL *ssl, uint8_t *out_alert,
                                                  CBS *contents) {
  if (contents == NULL) {
    return 1;
  }

  CBS supported_group_list;
  if (!CBS_get_u16_length_prefixed(contents, &supported_group_list) ||
      CBS_len(&supported_group_list) == 0 ||
      (CBS_len(&supported_group_list) & 1) != 0 ||
      CBS_len(contents) != 0) {
    return 0;
  }

  ssl->s3->hs->peer_supported_group_list = OPENSSL_malloc(
      CBS_len(&supported_group_list));
  if (ssl->s3->hs->peer_supported_group_list == NULL) {
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return 0;
  }

  const size_t num_groups = CBS_len(&supported_group_list) / 2;
  for (size_t i = 0; i < num_groups; i++) {
    if (!CBS_get_u16(&supported_group_list,
                     &ssl->s3->hs->peer_supported_group_list[i])) {
      goto err;
    }
  }

  assert(CBS_len(&supported_group_list) == 0);
  ssl->s3->hs->peer_supported_group_list_len = num_groups;

  return 1;

err:
  OPENSSL_free(ssl->s3->hs->peer_supported_group_list);
  ssl->s3->hs->peer_supported_group_list = NULL;
  *out_alert = SSL_AD_INTERNAL_ERROR;
  return 0;
}

static int ext_supported_groups_add_serverhello(SSL *ssl, CBB *out) {
  /* Servers don't echo this extension. */
  return 1;
}


/* kExtensions contains all the supported extensions. */
static const struct tls_extension kExtensions[] = {
  {
    TLSEXT_TYPE_renegotiate,
    NULL,
    ext_ri_add_clienthello,
    ext_ri_parse_serverhello,
    ext_ri_parse_clienthello,
    ext_ri_add_serverhello,
  },
  {
    TLSEXT_TYPE_server_name,
    NULL,
    ext_sni_add_clienthello,
    ext_sni_parse_serverhello,
    ext_sni_parse_clienthello,
    ext_sni_add_serverhello,
  },
  {
    TLSEXT_TYPE_extended_master_secret,
    NULL,
    ext_ems_add_clienthello,
    ext_ems_parse_serverhello,
    ext_ems_parse_clienthello,
    ext_ems_add_serverhello,
  },
  {
    TLSEXT_TYPE_session_ticket,
    NULL,
    ext_ticket_add_clienthello,
    ext_ticket_parse_serverhello,
    /* Ticket extension client parsing is handled in ssl_session.c */
    ignore_parse_clienthello,
    ext_ticket_add_serverhello,
  },
  {
    TLSEXT_TYPE_signature_algorithms,
    NULL,
    ext_sigalgs_add_clienthello,
    forbid_parse_serverhello,
    ext_sigalgs_parse_clienthello,
    dont_add_serverhello,
  },
  {
    TLSEXT_TYPE_status_request,
    ext_ocsp_init,
    ext_ocsp_add_clienthello,
    ext_ocsp_parse_serverhello,
    ext_ocsp_parse_clienthello,
    ext_ocsp_add_serverhello,
  },
  {
    TLSEXT_TYPE_next_proto_neg,
    NULL,
    ext_npn_add_clienthello,
    ext_npn_parse_serverhello,
    ext_npn_parse_clienthello,
    ext_npn_add_serverhello,
  },
  {
    TLSEXT_TYPE_certificate_timestamp,
    NULL,
    ext_sct_add_clienthello,
    ext_sct_parse_serverhello,
    ext_sct_parse_clienthello,
    ext_sct_add_serverhello,
  },
  {
    TLSEXT_TYPE_application_layer_protocol_negotiation,
    ext_alpn_init,
    ext_alpn_add_clienthello,
    ext_alpn_parse_serverhello,
    ext_alpn_parse_clienthello,
    ext_alpn_add_serverhello,
  },
  {
    TLSEXT_TYPE_channel_id,
    ext_channel_id_init,
    ext_channel_id_add_clienthello,
    ext_channel_id_parse_serverhello,
    ext_channel_id_parse_clienthello,
    ext_channel_id_add_serverhello,
  },
  {
    TLSEXT_TYPE_srtp,
    ext_srtp_init,
    ext_srtp_add_clienthello,
    ext_srtp_parse_serverhello,
    ext_srtp_parse_clienthello,
    ext_srtp_add_serverhello,
  },
  {
    TLSEXT_TYPE_ec_point_formats,
    NULL,
    ext_ec_point_add_clienthello,
    ext_ec_point_parse_serverhello,
    ext_ec_point_parse_clienthello,
    ext_ec_point_add_serverhello,
  },
  {
    TLSEXT_TYPE_key_share,
    NULL,
    ext_key_share_add_clienthello,
    forbid_parse_serverhello,
    ignore_parse_clienthello,
    dont_add_serverhello,
  },
  {
    TLSEXT_TYPE_pre_shared_key,
    NULL,
    ext_pre_shared_key_add_clienthello,
    forbid_parse_serverhello,
    ignore_parse_clienthello,
    dont_add_serverhello,
  },
  {
    TLSEXT_TYPE_supported_versions,
    NULL,
    ext_supported_versions_add_clienthello,
    forbid_parse_serverhello,
    ignore_parse_clienthello,
    dont_add_serverhello,
  },
  {
    TLSEXT_TYPE_cookie,
    NULL,
    ext_cookie_add_clienthello,
    forbid_parse_serverhello,
    ignore_parse_clienthello,
    dont_add_serverhello,
  },
  /* The final extension must be non-empty. WebSphere Application Server 7.0 is
   * intolerant to the last extension being zero-length. See
   * https://crbug.com/363583. */
  {
    TLSEXT_TYPE_supported_groups,
    NULL,
    ext_supported_groups_add_clienthello,
    ext_supported_groups_parse_serverhello,
    ext_supported_groups_parse_clienthello,
    ext_supported_groups_add_serverhello,
  },
};

#define kNumExtensions (sizeof(kExtensions) / sizeof(struct tls_extension))

OPENSSL_COMPILE_ASSERT(kNumExtensions <=
                           sizeof(((SSL_HANDSHAKE *)NULL)->extensions.sent) * 8,
                       too_many_extensions_for_sent_bitset);
OPENSSL_COMPILE_ASSERT(
    kNumExtensions <= sizeof(((SSL_HANDSHAKE *)NULL)->extensions.received) * 8,
    too_many_extensions_for_received_bitset);

static const struct tls_extension *tls_extension_find(uint32_t *out_index,
                                                      uint16_t value) {
  unsigned i;
  for (i = 0; i < kNumExtensions; i++) {
    if (kExtensions[i].value == value) {
      *out_index = i;
      return &kExtensions[i];
    }
  }

  return NULL;
}

int SSL_extension_supported(unsigned extension_value) {
  uint32_t index;
  return extension_value == TLSEXT_TYPE_padding ||
         tls_extension_find(&index, extension_value) != NULL;
}

int ssl_add_clienthello_tlsext(SSL *ssl, CBB *out, size_t header_len) {
  /* don't add extensions for SSLv3 unless doing secure renegotiation */
  if (ssl->client_version == SSL3_VERSION &&
      !ssl->s3->send_connection_binding) {
    return 1;
  }

  CBB extensions;
  if (!CBB_add_u16_length_prefixed(out, &extensions)) {
    goto err;
  }

  ssl->s3->hs->extensions.sent = 0;
  ssl->s3->hs->custom_extensions.sent = 0;

  for (size_t i = 0; i < kNumExtensions; i++) {
    if (kExtensions[i].init != NULL) {
      kExtensions[i].init(ssl);
    }
  }

  uint16_t grease_ext1 = 0;
  if (ssl->ctx->grease_enabled) {
    /* Add a fake empty extension. See draft-davidben-tls-grease-01. */
    grease_ext1 = ssl_get_grease_value(ssl, ssl_grease_extension1);
    if (!CBB_add_u16(&extensions, grease_ext1) ||
        !CBB_add_u16(&extensions, 0 /* zero length */)) {
      goto err;
    }
  }

  for (size_t i = 0; i < kNumExtensions; i++) {
    const size_t len_before = CBB_len(&extensions);
    if (!kExtensions[i].add_clienthello(ssl, &extensions)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_ERROR_ADDING_EXTENSION);
      ERR_add_error_dataf("extension: %u", (unsigned)kExtensions[i].value);
      goto err;
    }

    if (CBB_len(&extensions) != len_before) {
      ssl->s3->hs->extensions.sent |= (1u << i);
    }
  }

  if (!custom_ext_add_clienthello(ssl, &extensions)) {
    goto err;
  }

  if (ssl->ctx->grease_enabled) {
    /* Add a fake non-empty extension. See draft-davidben-tls-grease-01. */
    uint16_t grease_ext2 = ssl_get_grease_value(ssl, ssl_grease_extension2);

    /* The two fake extensions must not have the same value. GREASE values are
     * of the form 0x1a1a, 0x2a2a, 0x3a3a, etc., so XOR to generate a different
     * one. */
    if (grease_ext1 == grease_ext2) {
      grease_ext2 ^= 0x1010;
    }

    if (!CBB_add_u16(&extensions, grease_ext2) ||
        !CBB_add_u16(&extensions, 1 /* one byte length */) ||
        !CBB_add_u8(&extensions, 0 /* single zero byte as contents */)) {
      goto err;
    }
  }

  if (!SSL_is_dtls(ssl)) {
    header_len += 2 + CBB_len(&extensions);
    if (header_len > 0xff && header_len < 0x200) {
      /* Add padding to workaround bugs in F5 terminators. See RFC 7685.
       *
       * NB: because this code works out the length of all existing extensions
       * it MUST always appear last. */
      size_t padding_len = 0x200 - header_len;
      /* Extensions take at least four bytes to encode. Always include at least
       * one byte of data if including the extension. WebSphere Application
       * Server 7.0 is intolerant to the last extension being zero-length. See
       * https://crbug.com/363583. */
      if (padding_len >= 4 + 1) {
        padding_len -= 4;
      } else {
        padding_len = 1;
      }

      uint8_t *padding_bytes;
      if (!CBB_add_u16(&extensions, TLSEXT_TYPE_padding) ||
          !CBB_add_u16(&extensions, padding_len) ||
          !CBB_add_space(&extensions, &padding_bytes, padding_len)) {
        goto err;
      }

      memset(padding_bytes, 0, padding_len);
    }
  }

  /* Discard empty extensions blocks. */
  if (CBB_len(&extensions) == 0) {
    CBB_discard_child(out);
  }

  return CBB_flush(out);

err:
  OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
  return 0;
}

int ssl_add_serverhello_tlsext(SSL *ssl, CBB *out) {
  CBB extensions;
  if (!CBB_add_u16_length_prefixed(out, &extensions)) {
    goto err;
  }

  unsigned i;
  for (i = 0; i < kNumExtensions; i++) {
    if (!(ssl->s3->hs->extensions.received & (1u << i))) {
      /* Don't send extensions that were not received. */
      continue;
    }

    if (!kExtensions[i].add_serverhello(ssl, &extensions)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_ERROR_ADDING_EXTENSION);
      ERR_add_error_dataf("extension: %u", (unsigned)kExtensions[i].value);
      goto err;
    }
  }

  if (!custom_ext_add_serverhello(ssl, &extensions)) {
    goto err;
  }

  /* Discard empty extensions blocks before TLS 1.3. */
  if (ssl3_protocol_version(ssl) < TLS1_3_VERSION &&
      CBB_len(&extensions) == 0) {
    CBB_discard_child(out);
  }

  return CBB_flush(out);

err:
  OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
  return 0;
}

static int ssl_scan_clienthello_tlsext(
    SSL *ssl, const struct ssl_early_callback_ctx *client_hello,
    int *out_alert) {
  for (size_t i = 0; i < kNumExtensions; i++) {
    if (kExtensions[i].init != NULL) {
      kExtensions[i].init(ssl);
    }
  }

  ssl->s3->hs->extensions.received = 0;
  ssl->s3->hs->custom_extensions.received = 0;

  CBS extensions;
  CBS_init(&extensions, client_hello->extensions, client_hello->extensions_len);
  while (CBS_len(&extensions) != 0) {
    uint16_t type;
    CBS extension;

    /* Decode the next extension. */
    if (!CBS_get_u16(&extensions, &type) ||
        !CBS_get_u16_length_prefixed(&extensions, &extension)) {
      *out_alert = SSL_AD_DECODE_ERROR;
      return 0;
    }

    /* RFC 5746 made the existence of extensions in SSL 3.0 somewhat
     * ambiguous. Ignore all but the renegotiation_info extension. */
    if (ssl->version == SSL3_VERSION && type != TLSEXT_TYPE_renegotiate) {
      continue;
    }

    unsigned ext_index;
    const struct tls_extension *const ext =
        tls_extension_find(&ext_index, type);

    if (ext == NULL) {
      if (!custom_ext_parse_clienthello(ssl, out_alert, type, &extension)) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_ERROR_PARSING_EXTENSION);
        return 0;
      }
      continue;
    }

    ssl->s3->hs->extensions.received |= (1u << ext_index);
    uint8_t alert = SSL_AD_DECODE_ERROR;
    if (!ext->parse_clienthello(ssl, &alert, &extension)) {
      *out_alert = alert;
      OPENSSL_PUT_ERROR(SSL, SSL_R_ERROR_PARSING_EXTENSION);
      ERR_add_error_dataf("extension: %u", (unsigned)type);
      return 0;
    }
  }

  for (size_t i = 0; i < kNumExtensions; i++) {
    if (ssl->s3->hs->extensions.received & (1u << i)) {
      continue;
    }

    CBS *contents = NULL, fake_contents;
    static const uint8_t kFakeRenegotiateExtension[] = {0};
    if (kExtensions[i].value == TLSEXT_TYPE_renegotiate &&
        ssl_client_cipher_list_contains_cipher(client_hello,
                                               SSL3_CK_SCSV & 0xffff)) {
      /* The renegotiation SCSV was received so pretend that we received a
       * renegotiation extension. */
      CBS_init(&fake_contents, kFakeRenegotiateExtension,
               sizeof(kFakeRenegotiateExtension));
      contents = &fake_contents;
      ssl->s3->hs->extensions.received |= (1u << i);
    }

    /* Extension wasn't observed so call the callback with a NULL
     * parameter. */
    uint8_t alert = SSL_AD_DECODE_ERROR;
    if (!kExtensions[i].parse_clienthello(ssl, &alert, contents)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_MISSING_EXTENSION);
      ERR_add_error_dataf("extension: %u", (unsigned)kExtensions[i].value);
      *out_alert = alert;
      return 0;
    }
  }

  return 1;
}

int ssl_parse_clienthello_tlsext(
    SSL *ssl, const struct ssl_early_callback_ctx *client_hello) {
  int alert = -1;
  if (ssl_scan_clienthello_tlsext(ssl, client_hello, &alert) <= 0) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, alert);
    return 0;
  }

  if (ssl_check_clienthello_tlsext(ssl) <= 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CLIENTHELLO_TLSEXT);
    return 0;
  }

  return 1;
}

OPENSSL_COMPILE_ASSERT(kNumExtensions <= sizeof(uint32_t) * 8, too_many_bits);

static int ssl_scan_serverhello_tlsext(SSL *ssl, CBS *cbs, int *out_alert) {
  /* Before TLS 1.3, ServerHello extensions blocks may be omitted if empty. */
  if (CBS_len(cbs) == 0 && ssl3_protocol_version(ssl) < TLS1_3_VERSION) {
    return 1;
  }

  /* Decode the extensions block and check it is valid. */
  CBS extensions;
  if (!CBS_get_u16_length_prefixed(cbs, &extensions) ||
      !tls1_check_duplicate_extensions(&extensions)) {
    *out_alert = SSL_AD_DECODE_ERROR;
    return 0;
  }

  uint32_t received = 0;
  while (CBS_len(&extensions) != 0) {
    uint16_t type;
    CBS extension;

    /* Decode the next extension. */
    if (!CBS_get_u16(&extensions, &type) ||
        !CBS_get_u16_length_prefixed(&extensions, &extension)) {
      *out_alert = SSL_AD_DECODE_ERROR;
      return 0;
    }

    unsigned ext_index;
    const struct tls_extension *const ext =
        tls_extension_find(&ext_index, type);

    if (ext == NULL) {
      if (!custom_ext_parse_serverhello(ssl, out_alert, type, &extension)) {
        return 0;
      }
      continue;
    }

    if (!(ssl->s3->hs->extensions.sent & (1u << ext_index)) &&
        type != TLSEXT_TYPE_renegotiate) {
      /* If the extension was never sent then it is illegal, except for the
       * renegotiation extension which, in SSL 3.0, is signaled via SCSV. */
      OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_EXTENSION);
      ERR_add_error_dataf("extension :%u", (unsigned)type);
      *out_alert = SSL_AD_UNSUPPORTED_EXTENSION;
      return 0;
    }

    received |= (1u << ext_index);

    uint8_t alert = SSL_AD_DECODE_ERROR;
    if (!ext->parse_serverhello(ssl, &alert, &extension)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_ERROR_PARSING_EXTENSION);
      ERR_add_error_dataf("extension: %u", (unsigned)type);
      *out_alert = alert;
      return 0;
    }
  }

  for (size_t i = 0; i < kNumExtensions; i++) {
    if (!(received & (1u << i))) {
      /* Extension wasn't observed so call the callback with a NULL
       * parameter. */
      uint8_t alert = SSL_AD_DECODE_ERROR;
      if (!kExtensions[i].parse_serverhello(ssl, &alert, NULL)) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_MISSING_EXTENSION);
        ERR_add_error_dataf("extension: %u", (unsigned)kExtensions[i].value);
        *out_alert = alert;
        return 0;
      }
    }
  }

  return 1;
}

static int ssl_check_clienthello_tlsext(SSL *ssl) {
  int ret = SSL_TLSEXT_ERR_NOACK;
  int al = SSL_AD_UNRECOGNIZED_NAME;

  if (ssl->ctx->tlsext_servername_callback != 0) {
    ret = ssl->ctx->tlsext_servername_callback(ssl, &al,
                                               ssl->ctx->tlsext_servername_arg);
  } else if (ssl->initial_ctx->tlsext_servername_callback != 0) {
    ret = ssl->initial_ctx->tlsext_servername_callback(
        ssl, &al, ssl->initial_ctx->tlsext_servername_arg);
  }

  switch (ret) {
    case SSL_TLSEXT_ERR_ALERT_FATAL:
      ssl3_send_alert(ssl, SSL3_AL_FATAL, al);
      return -1;

    case SSL_TLSEXT_ERR_ALERT_WARNING:
      ssl3_send_alert(ssl, SSL3_AL_WARNING, al);
      return 1;

    case SSL_TLSEXT_ERR_NOACK:
      ssl->s3->hs->should_ack_sni = 0;
      return 1;

    default:
      return 1;
  }
}

static int ssl_check_serverhello_tlsext(SSL *ssl) {
  int ret = SSL_TLSEXT_ERR_OK;
  int al = SSL_AD_UNRECOGNIZED_NAME;

  if (ssl->ctx->tlsext_servername_callback != 0) {
    ret = ssl->ctx->tlsext_servername_callback(ssl, &al,
                                               ssl->ctx->tlsext_servername_arg);
  } else if (ssl->initial_ctx->tlsext_servername_callback != 0) {
    ret = ssl->initial_ctx->tlsext_servername_callback(
        ssl, &al, ssl->initial_ctx->tlsext_servername_arg);
  }

  switch (ret) {
    case SSL_TLSEXT_ERR_ALERT_FATAL:
      ssl3_send_alert(ssl, SSL3_AL_FATAL, al);
      return -1;

    case SSL_TLSEXT_ERR_ALERT_WARNING:
      ssl3_send_alert(ssl, SSL3_AL_WARNING, al);
      return 1;

    default:
      return 1;
  }
}

int ssl_parse_serverhello_tlsext(SSL *ssl, CBS *cbs) {
  int alert = -1;
  if (ssl_scan_serverhello_tlsext(ssl, cbs, &alert) <= 0) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, alert);
    return 0;
  }

  if (ssl_check_serverhello_tlsext(ssl) <= 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_SERVERHELLO_TLSEXT);
    return 0;
  }

  return 1;
}

int tls_process_ticket(SSL *ssl, SSL_SESSION **out_session,
                       int *out_renew_ticket, const uint8_t *ticket,
                       size_t ticket_len, const uint8_t *session_id,
                       size_t session_id_len) {
  int ret = 1; /* Most errors are non-fatal. */
  SSL_CTX *ssl_ctx = ssl->initial_ctx;
  uint8_t *plaintext = NULL;

  HMAC_CTX hmac_ctx;
  HMAC_CTX_init(&hmac_ctx);
  EVP_CIPHER_CTX cipher_ctx;
  EVP_CIPHER_CTX_init(&cipher_ctx);

  *out_renew_ticket = 0;
  *out_session = NULL;

  if (SSL_get_options(ssl) & SSL_OP_NO_TICKET) {
    goto done;
  }

  if (session_id_len > SSL_MAX_SSL_SESSION_ID_LENGTH) {
    goto done;
  }

  /* Ensure there is room for the key name and the largest IV
   * |tlsext_ticket_key_cb| may try to consume. The real limit may be lower, but
   * the maximum IV length should be well under the minimum size for the
   * session material and HMAC. */
  if (ticket_len < SSL_TICKET_KEY_NAME_LEN + EVP_MAX_IV_LENGTH) {
    goto done;
  }
  const uint8_t *iv = ticket + SSL_TICKET_KEY_NAME_LEN;

  if (ssl_ctx->tlsext_ticket_key_cb != NULL) {
    int cb_ret = ssl_ctx->tlsext_ticket_key_cb(
        ssl, (uint8_t *)ticket /* name */, (uint8_t *)iv, &cipher_ctx,
        &hmac_ctx, 0 /* decrypt */);
    if (cb_ret < 0) {
      ret = 0;
      goto done;
    }
    if (cb_ret == 0) {
      goto done;
    }
    if (cb_ret == 2) {
      *out_renew_ticket = 1;
    }
  } else {
    /* Check the key name matches. */
    if (memcmp(ticket, ssl_ctx->tlsext_tick_key_name,
               SSL_TICKET_KEY_NAME_LEN) != 0) {
      goto done;
    }
    if (!HMAC_Init_ex(&hmac_ctx, ssl_ctx->tlsext_tick_hmac_key,
                      sizeof(ssl_ctx->tlsext_tick_hmac_key), tlsext_tick_md(),
                      NULL) ||
        !EVP_DecryptInit_ex(&cipher_ctx, EVP_aes_128_cbc(), NULL,
                            ssl_ctx->tlsext_tick_aes_key, iv)) {
      ret = 0;
      goto done;
    }
  }
  size_t iv_len = EVP_CIPHER_CTX_iv_length(&cipher_ctx);

  /* Check the MAC at the end of the ticket. */
  uint8_t mac[EVP_MAX_MD_SIZE];
  size_t mac_len = HMAC_size(&hmac_ctx);
  if (ticket_len < SSL_TICKET_KEY_NAME_LEN + iv_len + 1 + mac_len) {
    /* The ticket must be large enough for key name, IV, data, and MAC. */
    goto done;
  }
  HMAC_Update(&hmac_ctx, ticket, ticket_len - mac_len);
  HMAC_Final(&hmac_ctx, mac, NULL);
  int mac_ok =
      CRYPTO_memcmp(mac, ticket + (ticket_len - mac_len), mac_len) == 0;
#if defined(BORINGSSL_UNSAFE_FUZZER_MODE)
  mac_ok = 1;
#endif
  if (!mac_ok) {
    goto done;
  }

  /* Decrypt the session data. */
  const uint8_t *ciphertext = ticket + SSL_TICKET_KEY_NAME_LEN + iv_len;
  size_t ciphertext_len = ticket_len - SSL_TICKET_KEY_NAME_LEN - iv_len -
                          mac_len;
  plaintext = OPENSSL_malloc(ciphertext_len);
  if (plaintext == NULL) {
    ret = 0;
    goto done;
  }
  size_t plaintext_len;
#if defined(BORINGSSL_UNSAFE_FUZZER_MODE)
  memcpy(plaintext, ciphertext, ciphertext_len);
  plaintext_len = ciphertext_len;
#else
  if (ciphertext_len >= INT_MAX) {
    goto done;
  }
  int len1, len2;
  if (!EVP_DecryptUpdate(&cipher_ctx, plaintext, &len1, ciphertext,
                         (int)ciphertext_len) ||
      !EVP_DecryptFinal_ex(&cipher_ctx, plaintext + len1, &len2)) {
    ERR_clear_error(); /* Don't leave an error on the queue. */
    goto done;
  }
  plaintext_len = (size_t)(len1 + len2);
#endif

  /* Decode the session. */
  SSL_SESSION *session = SSL_SESSION_from_bytes(plaintext, plaintext_len);
  if (session == NULL) {
    ERR_clear_error(); /* Don't leave an error on the queue. */
    goto done;
  }

  /* Copy the client's session ID into the new session, to denote the ticket has
   * been accepted. */
  memcpy(session->session_id, session_id, session_id_len);
  session->session_id_length = session_id_len;

  if (!ssl_session_is_context_valid(ssl, session) ||
      !ssl_session_is_time_valid(ssl, session)) {
    SSL_SESSION_free(session);
    session = NULL;
  }

  *out_session = session;

done:
  OPENSSL_free(plaintext);
  HMAC_CTX_cleanup(&hmac_ctx);
  EVP_CIPHER_CTX_cleanup(&cipher_ctx);
  return ret;
}

int tls1_parse_peer_sigalgs(SSL *ssl, const CBS *in_sigalgs) {
  /* Extension ignored for inappropriate versions */
  if (ssl3_protocol_version(ssl) < TLS1_2_VERSION) {
    return 1;
  }

  SSL_HANDSHAKE *hs = ssl->s3->hs;
  OPENSSL_free(hs->peer_sigalgs);
  hs->peer_sigalgs = NULL;
  hs->num_peer_sigalgs = 0;

  size_t num_sigalgs = CBS_len(in_sigalgs);
  if (num_sigalgs % 2 != 0) {
    return 0;
  }
  num_sigalgs /= 2;

  /* supported_signature_algorithms in the certificate request is
   * allowed to be empty. */
  if (num_sigalgs == 0) {
    return 1;
  }

  /* This multiplication doesn't overflow because sizeof(uint16_t) is two
   * and we just divided |num_sigalgs| by two. */
  hs->peer_sigalgs = OPENSSL_malloc(num_sigalgs * sizeof(uint16_t));
  if (hs->peer_sigalgs == NULL) {
    return 0;
  }
  hs->num_peer_sigalgs = num_sigalgs;

  CBS sigalgs;
  CBS_init(&sigalgs, CBS_data(in_sigalgs), CBS_len(in_sigalgs));
  for (size_t i = 0; i < num_sigalgs; i++) {
    if (!CBS_get_u16(&sigalgs, &hs->peer_sigalgs[i])) {
      return 0;
    }
  }

  return 1;
}

int tls1_choose_signature_algorithm(SSL *ssl, uint16_t *out) {
  CERT *cert = ssl->cert;
  SSL_HANDSHAKE *hs = ssl->s3->hs;

  /* Before TLS 1.2, the signature algorithm isn't negotiated as part of the
   * handshake. It is fixed at MD5-SHA1 for RSA and SHA1 for ECDSA. */
  if (ssl3_protocol_version(ssl) < TLS1_2_VERSION) {
    int type = ssl_private_key_type(ssl);
    if (type == NID_rsaEncryption) {
      *out = SSL_SIGN_RSA_PKCS1_MD5_SHA1;
      return 1;
    }
    if (ssl_is_ecdsa_key_type(type)) {
      *out = SSL_SIGN_ECDSA_SHA1;
      return 1;
    }
    OPENSSL_PUT_ERROR(SSL, SSL_R_NO_COMMON_SIGNATURE_ALGORITHMS);
    return 0;
  }

  const uint16_t *sigalgs = cert->sigalgs;
  size_t num_sigalgs = cert->num_sigalgs;
  if (sigalgs == NULL) {
    sigalgs = kSignSignatureAlgorithms;
    num_sigalgs = OPENSSL_ARRAY_SIZE(kSignSignatureAlgorithms);
  }

  const uint16_t *peer_sigalgs = hs->peer_sigalgs;
  size_t num_peer_sigalgs = hs->num_peer_sigalgs;
  if (num_peer_sigalgs == 0 && ssl3_protocol_version(ssl) < TLS1_3_VERSION) {
    /* If the client didn't specify any signature_algorithms extension then
     * we can assume that it supports SHA1. See
     * http://tools.ietf.org/html/rfc5246#section-7.4.1.4.1 */
    static const uint16_t kDefaultPeerAlgorithms[] = {SSL_SIGN_RSA_PKCS1_SHA1,
                                                      SSL_SIGN_ECDSA_SHA1};
    peer_sigalgs = kDefaultPeerAlgorithms;
    num_peer_sigalgs = OPENSSL_ARRAY_SIZE(kDefaultPeerAlgorithms);
  }

  for (size_t i = 0; i < num_sigalgs; i++) {
    uint16_t sigalg = sigalgs[i];
    /* SSL_SIGN_RSA_PKCS1_MD5_SHA1 is an internal value and should never be
     * negotiated. */
    if (sigalg == SSL_SIGN_RSA_PKCS1_MD5_SHA1 ||
        !ssl_private_key_supports_signature_algorithm(ssl, sigalgs[i])) {
      continue;
    }

    for (size_t j = 0; j < num_peer_sigalgs; j++) {
      if (sigalg == peer_sigalgs[j]) {
        *out = sigalg;
        return 1;
      }
    }
  }

  OPENSSL_PUT_ERROR(SSL, SSL_R_NO_COMMON_SIGNATURE_ALGORITHMS);
  return 0;
}

int tls1_verify_channel_id(SSL *ssl) {
  int ret = 0;
  uint16_t extension_type;
  CBS extension, channel_id;

  /* A Channel ID handshake message is structured to contain multiple
   * extensions, but the only one that can be present is Channel ID. */
  CBS_init(&channel_id, ssl->init_msg, ssl->init_num);
  if (!CBS_get_u16(&channel_id, &extension_type) ||
      !CBS_get_u16_length_prefixed(&channel_id, &extension) ||
      CBS_len(&channel_id) != 0 ||
      extension_type != TLSEXT_TYPE_channel_id ||
      CBS_len(&extension) != TLSEXT_CHANNEL_ID_SIZE) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return 0;
  }

  EC_GROUP *p256 = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1);
  if (!p256) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_NO_P256_SUPPORT);
    return 0;
  }

  EC_KEY *key = NULL;
  EC_POINT *point = NULL;
  BIGNUM x, y;
  ECDSA_SIG sig;
  BN_init(&x);
  BN_init(&y);
  sig.r = BN_new();
  sig.s = BN_new();
  if (sig.r == NULL || sig.s == NULL) {
    goto err;
  }

  const uint8_t *p = CBS_data(&extension);
  if (BN_bin2bn(p + 0, 32, &x) == NULL ||
      BN_bin2bn(p + 32, 32, &y) == NULL ||
      BN_bin2bn(p + 64, 32, sig.r) == NULL ||
      BN_bin2bn(p + 96, 32, sig.s) == NULL) {
    goto err;
  }

  point = EC_POINT_new(p256);
  if (point == NULL ||
      !EC_POINT_set_affine_coordinates_GFp(p256, point, &x, &y, NULL)) {
    goto err;
  }

  key = EC_KEY_new();
  if (key == NULL ||
      !EC_KEY_set_group(key, p256) ||
      !EC_KEY_set_public_key(key, point)) {
    goto err;
  }

  uint8_t digest[EVP_MAX_MD_SIZE];
  size_t digest_len;
  if (!tls1_channel_id_hash(ssl, digest, &digest_len)) {
    goto err;
  }

  int sig_ok = ECDSA_do_verify(digest, digest_len, &sig, key);
#if defined(BORINGSSL_UNSAFE_FUZZER_MODE)
  sig_ok = 1;
#endif
  if (!sig_ok) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CHANNEL_ID_SIGNATURE_INVALID);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECRYPT_ERROR);
    ssl->s3->tlsext_channel_id_valid = 0;
    goto err;
  }

  memcpy(ssl->s3->tlsext_channel_id, p, 64);
  ret = 1;

err:
  BN_free(&x);
  BN_free(&y);
  BN_free(sig.r);
  BN_free(sig.s);
  EC_KEY_free(key);
  EC_POINT_free(point);
  EC_GROUP_free(p256);
  return ret;
}

int tls1_write_channel_id(SSL *ssl, CBB *cbb) {
  uint8_t digest[EVP_MAX_MD_SIZE];
  size_t digest_len;
  if (!tls1_channel_id_hash(ssl, digest, &digest_len)) {
    return 0;
  }

  EC_KEY *ec_key = EVP_PKEY_get0_EC_KEY(ssl->tlsext_channel_id_private);
  if (ec_key == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  int ret = 0;
  BIGNUM *x = BN_new();
  BIGNUM *y = BN_new();
  ECDSA_SIG *sig = NULL;
  if (x == NULL || y == NULL ||
      !EC_POINT_get_affine_coordinates_GFp(EC_KEY_get0_group(ec_key),
                                           EC_KEY_get0_public_key(ec_key),
                                           x, y, NULL)) {
    goto err;
  }

  sig = ECDSA_do_sign(digest, digest_len, ec_key);
  if (sig == NULL) {
    goto err;
  }

  CBB child;
  if (!CBB_add_u16(cbb, TLSEXT_TYPE_channel_id) ||
      !CBB_add_u16_length_prefixed(cbb, &child) ||
      !BN_bn2cbb_padded(&child, 32, x) ||
      !BN_bn2cbb_padded(&child, 32, y) ||
      !BN_bn2cbb_padded(&child, 32, sig->r) ||
      !BN_bn2cbb_padded(&child, 32, sig->s) ||
      !CBB_flush(cbb)) {
    goto err;
  }

  ret = 1;

err:
  BN_free(x);
  BN_free(y);
  ECDSA_SIG_free(sig);
  return ret;
}

int tls1_channel_id_hash(SSL *ssl, uint8_t *out, size_t *out_len) {
  if (ssl3_protocol_version(ssl) >= TLS1_3_VERSION) {
    uint8_t *msg;
    size_t msg_len;
    if (!tls13_get_cert_verify_signature_input(ssl, &msg, &msg_len,
                                               ssl_cert_verify_channel_id)) {
      return 0;
    }
    SHA256(msg, msg_len, out);
    *out_len = SHA256_DIGEST_LENGTH;
    OPENSSL_free(msg);
    return 1;
  }

  SHA256_CTX ctx;

  SHA256_Init(&ctx);
  static const char kClientIDMagic[] = "TLS Channel ID signature";
  SHA256_Update(&ctx, kClientIDMagic, sizeof(kClientIDMagic));

  if (ssl->session != NULL) {
    static const char kResumptionMagic[] = "Resumption";
    SHA256_Update(&ctx, kResumptionMagic, sizeof(kResumptionMagic));
    if (ssl->session->original_handshake_hash_len == 0) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return 0;
    }
    SHA256_Update(&ctx, ssl->session->original_handshake_hash,
                  ssl->session->original_handshake_hash_len);
  }

  uint8_t handshake_hash[EVP_MAX_MD_SIZE];
  int handshake_hash_len = tls1_handshake_digest(ssl, handshake_hash,
                                                 sizeof(handshake_hash));
  if (handshake_hash_len < 0) {
    return 0;
  }
  SHA256_Update(&ctx, handshake_hash, (size_t)handshake_hash_len);
  SHA256_Final(out, &ctx);
  *out_len = SHA256_DIGEST_LENGTH;
  return 1;
}

/* tls1_record_handshake_hashes_for_channel_id records the current handshake
 * hashes in |ssl->s3->new_session| so that Channel ID resumptions can sign that
 * data. */
int tls1_record_handshake_hashes_for_channel_id(SSL *ssl) {
  int digest_len;
  /* This function should never be called for a resumed session because the
   * handshake hashes that we wish to record are for the original, full
   * handshake. */
  if (ssl->session != NULL) {
    return -1;
  }

  digest_len =
      tls1_handshake_digest(
          ssl, ssl->s3->new_session->original_handshake_hash,
          sizeof(ssl->s3->new_session->original_handshake_hash));
  if (digest_len < 0) {
    return -1;
  }

  ssl->s3->new_session->original_handshake_hash_len = digest_len;

  return 1;
}

int ssl_do_channel_id_callback(SSL *ssl) {
  if (ssl->tlsext_channel_id_private != NULL ||
      ssl->ctx->channel_id_cb == NULL) {
    return 1;
  }

  EVP_PKEY *key = NULL;
  ssl->ctx->channel_id_cb(ssl, &key);
  if (key == NULL) {
    /* The caller should try again later. */
    return 1;
  }

  int ret = SSL_set1_tls_channel_id(ssl, key);
  EVP_PKEY_free(key);
  return ret;
}
