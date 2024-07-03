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

#include <openssl/x509.h>

#include <limits.h>

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/bytestring.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/obj.h>

#include "../internal.h"
#include "internal.h"


static void x509_pubkey_changed(X509_PUBKEY *pub) {
  EVP_PKEY_free(pub->pkey);
  pub->pkey = NULL;

  // Re-encode the |X509_PUBKEY| to DER and parse it with EVP's APIs.
  uint8_t *spki = NULL;
  int spki_len = i2d_X509_PUBKEY(pub, &spki);
  if (spki_len < 0) {
    goto err;
  }

  CBS cbs;
  CBS_init(&cbs, spki, (size_t)spki_len);
  EVP_PKEY *pkey = EVP_parse_public_key(&cbs);
  if (pkey == NULL || CBS_len(&cbs) != 0) {
    EVP_PKEY_free(pkey);
    goto err;
  }

  pub->pkey = pkey;

err:
  OPENSSL_free(spki);
  // If the operation failed, clear errors. An |X509_PUBKEY| whose key we cannot
  // parse is still a valid SPKI. It just cannot be converted to an |EVP_PKEY|.
  ERR_clear_error();
}

static int pubkey_cb(int operation, ASN1_VALUE **pval, const ASN1_ITEM *it,
                     void *exarg) {
  X509_PUBKEY *pubkey = (X509_PUBKEY *)*pval;
  if (operation == ASN1_OP_FREE_POST) {
    EVP_PKEY_free(pubkey->pkey);
  } else if (operation == ASN1_OP_D2I_POST) {
    x509_pubkey_changed(pubkey);
  }
  return 1;
}

ASN1_SEQUENCE_cb(X509_PUBKEY, pubkey_cb) = {
    ASN1_SIMPLE(X509_PUBKEY, algor, X509_ALGOR),
    ASN1_SIMPLE(X509_PUBKEY, public_key, ASN1_BIT_STRING),
} ASN1_SEQUENCE_END_cb(X509_PUBKEY, X509_PUBKEY)

IMPLEMENT_ASN1_FUNCTIONS_const(X509_PUBKEY)

int X509_PUBKEY_set(X509_PUBKEY **x, EVP_PKEY *pkey) {
  X509_PUBKEY *pk = NULL;
  uint8_t *spki = NULL;
  size_t spki_len;

  if (x == NULL) {
    return 0;
  }

  CBB cbb;
  if (!CBB_init(&cbb, 0) ||  //
      !EVP_marshal_public_key(&cbb, pkey) ||
      !CBB_finish(&cbb, &spki, &spki_len) ||  //
      spki_len > LONG_MAX) {
    CBB_cleanup(&cbb);
    OPENSSL_PUT_ERROR(X509, X509_R_PUBLIC_KEY_ENCODE_ERROR);
    goto error;
  }

  const uint8_t *p = spki;
  pk = d2i_X509_PUBKEY(NULL, &p, (long)spki_len);
  if (pk == NULL || p != spki + spki_len) {
    OPENSSL_PUT_ERROR(X509, X509_R_PUBLIC_KEY_DECODE_ERROR);
    goto error;
  }

  OPENSSL_free(spki);
  X509_PUBKEY_free(*x);
  *x = pk;

  return 1;
error:
  X509_PUBKEY_free(pk);
  OPENSSL_free(spki);
  return 0;
}

EVP_PKEY *X509_PUBKEY_get0(const X509_PUBKEY *key) {
  if (key == NULL) {
    return NULL;
  }

  if (key->pkey == NULL) {
    OPENSSL_PUT_ERROR(X509, X509_R_PUBLIC_KEY_DECODE_ERROR);
    return NULL;
  }

  return key->pkey;
}

EVP_PKEY *X509_PUBKEY_get(const X509_PUBKEY *key) {
  EVP_PKEY *pkey = X509_PUBKEY_get0(key);
  if (pkey != NULL) {
    EVP_PKEY_up_ref(pkey);
  }
  return pkey;
}

int X509_PUBKEY_set0_param(X509_PUBKEY *pub, ASN1_OBJECT *obj, int param_type,
                           void *param_value, uint8_t *key, int key_len) {
  if (!X509_ALGOR_set0(pub->algor, obj, param_type, param_value)) {
    return 0;
  }

  ASN1_STRING_set0(pub->public_key, key, key_len);
  // Set the number of unused bits to zero.
  pub->public_key->flags &= ~(ASN1_STRING_FLAG_BITS_LEFT | 0x07);
  pub->public_key->flags |= ASN1_STRING_FLAG_BITS_LEFT;

  x509_pubkey_changed(pub);
  return 1;
}

int X509_PUBKEY_get0_param(ASN1_OBJECT **out_obj, const uint8_t **out_key,
                           int *out_key_len, X509_ALGOR **out_alg,
                           X509_PUBKEY *pub) {
  if (out_obj != NULL) {
    *out_obj = pub->algor->algorithm;
  }
  if (out_key != NULL) {
    *out_key = pub->public_key->data;
    *out_key_len = pub->public_key->length;
  }
  if (out_alg != NULL) {
    *out_alg = pub->algor;
  }
  return 1;
}

const ASN1_BIT_STRING *X509_PUBKEY_get0_public_key(const X509_PUBKEY *pub) {
  return pub->public_key;
}
