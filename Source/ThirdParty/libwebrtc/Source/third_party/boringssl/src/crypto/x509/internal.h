/* Copyright (c) 2016, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#ifndef OPENSSL_HEADER_X509_INTERNAL_H
#define OPENSSL_HEADER_X509_INTERNAL_H

#include <openssl/base.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* Internal structures. */

struct X509_val_st {
  ASN1_TIME *notBefore;
  ASN1_TIME *notAfter;
} /* X509_VAL */;

struct X509_pubkey_st {
  X509_ALGOR *algor;
  ASN1_BIT_STRING *public_key;
  EVP_PKEY *pkey;
} /* X509_PUBKEY */;

struct x509_attributes_st {
  ASN1_OBJECT *object;
  STACK_OF(ASN1_TYPE) *set;
} /* X509_ATTRIBUTE */;

struct x509_cert_aux_st {
  STACK_OF(ASN1_OBJECT) *trust;   // trusted uses
  STACK_OF(ASN1_OBJECT) *reject;  // rejected uses
  ASN1_UTF8STRING *alias;         // "friendly name"
  ASN1_OCTET_STRING *keyid;       // key id of private key
  STACK_OF(X509_ALGOR) *other;    // other unspecified info
} /* X509_CERT_AUX */;


/* RSA-PSS functions. */

/* x509_rsa_pss_to_ctx configures |ctx| for an RSA-PSS operation based on
 * signature algorithm parameters in |sigalg| (which must have type
 * |NID_rsassaPss|) and key |pkey|. It returns one on success and zero on
 * error. */
int x509_rsa_pss_to_ctx(EVP_MD_CTX *ctx, X509_ALGOR *sigalg, EVP_PKEY *pkey);

/* x509_rsa_pss_to_ctx sets |algor| to the signature algorithm parameters for
 * |ctx|, which must have been configured for an RSA-PSS signing operation. It
 * returns one on success and zero on error. */
int x509_rsa_ctx_to_pss(EVP_MD_CTX *ctx, X509_ALGOR *algor);

/* x509_print_rsa_pss_params prints a human-readable representation of RSA-PSS
 * parameters in |sigalg| to |bp|. It returns one on success and zero on
 * error. */
int x509_print_rsa_pss_params(BIO *bp, const X509_ALGOR *sigalg, int indent,
                              ASN1_PCTX *pctx);


/* Signature algorithm functions. */

/* x509_digest_sign_algorithm encodes the signing parameters of |ctx| as an
 * AlgorithmIdentifer and saves the result in |algor|. It returns one on
 * success, or zero on error. */
int x509_digest_sign_algorithm(EVP_MD_CTX *ctx, X509_ALGOR *algor);

/* x509_digest_verify_init sets up |ctx| for a signature verification operation
 * with public key |pkey| and parameters from |algor|. The |ctx| argument must
 * have been initialised with |EVP_MD_CTX_init|. It returns one on success, or
 * zero on error. */
int x509_digest_verify_init(EVP_MD_CTX *ctx, X509_ALGOR *sigalg,
                            EVP_PKEY *pkey);


#if defined(__cplusplus)
}  /* extern C */
#endif

#endif  /* OPENSSL_HEADER_X509_INTERNAL_H */
