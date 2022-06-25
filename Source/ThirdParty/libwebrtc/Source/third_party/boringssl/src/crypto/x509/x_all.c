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
#include <openssl/digest.h>
#include <openssl/dsa.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/rsa.h>
#include <openssl/stack.h>

int X509_verify(X509 *x509, EVP_PKEY *pkey)
{
    if (X509_ALGOR_cmp(x509->sig_alg, x509->cert_info->signature)) {
        OPENSSL_PUT_ERROR(X509, X509_R_SIGNATURE_ALGORITHM_MISMATCH);
        return 0;
    }
    return ASN1_item_verify(ASN1_ITEM_rptr(X509_CINF), x509->sig_alg,
                            x509->signature, x509->cert_info, pkey);
}

int X509_REQ_verify(X509_REQ *req, EVP_PKEY *pkey)
{
    return ASN1_item_verify(ASN1_ITEM_rptr(X509_REQ_INFO),
                            req->sig_alg, req->signature, req->req_info, pkey);
}

int X509_sign(X509 *x, EVP_PKEY *pkey, const EVP_MD *md)
{
    x->cert_info->enc.modified = 1;
    return (ASN1_item_sign(ASN1_ITEM_rptr(X509_CINF), x->cert_info->signature,
                           x->sig_alg, x->signature, x->cert_info, pkey, md));
}

int X509_sign_ctx(X509 *x, EVP_MD_CTX *ctx)
{
    x->cert_info->enc.modified = 1;
    return ASN1_item_sign_ctx(ASN1_ITEM_rptr(X509_CINF),
                              x->cert_info->signature,
                              x->sig_alg, x->signature, x->cert_info, ctx);
}

int X509_REQ_sign(X509_REQ *x, EVP_PKEY *pkey, const EVP_MD *md)
{
    return (ASN1_item_sign(ASN1_ITEM_rptr(X509_REQ_INFO), x->sig_alg, NULL,
                           x->signature, x->req_info, pkey, md));
}

int X509_REQ_sign_ctx(X509_REQ *x, EVP_MD_CTX *ctx)
{
    return ASN1_item_sign_ctx(ASN1_ITEM_rptr(X509_REQ_INFO),
                              x->sig_alg, NULL, x->signature, x->req_info,
                              ctx);
}

int X509_CRL_sign(X509_CRL *x, EVP_PKEY *pkey, const EVP_MD *md)
{
    x->crl->enc.modified = 1;
    return (ASN1_item_sign(ASN1_ITEM_rptr(X509_CRL_INFO), x->crl->sig_alg,
                           x->sig_alg, x->signature, x->crl, pkey, md));
}

int X509_CRL_sign_ctx(X509_CRL *x, EVP_MD_CTX *ctx)
{
    x->crl->enc.modified = 1;
    return ASN1_item_sign_ctx(ASN1_ITEM_rptr(X509_CRL_INFO),
                              x->crl->sig_alg, x->sig_alg, x->signature,
                              x->crl, ctx);
}

int NETSCAPE_SPKI_sign(NETSCAPE_SPKI *x, EVP_PKEY *pkey, const EVP_MD *md)
{
    return (ASN1_item_sign(ASN1_ITEM_rptr(NETSCAPE_SPKAC), x->sig_algor, NULL,
                           x->signature, x->spkac, pkey, md));
}

int NETSCAPE_SPKI_verify(NETSCAPE_SPKI *spki, EVP_PKEY *pkey)
{
    return (ASN1_item_verify(ASN1_ITEM_rptr(NETSCAPE_SPKAC), spki->sig_algor,
                             spki->signature, spki->spkac, pkey));
}

#ifndef OPENSSL_NO_FP_API
X509 *d2i_X509_fp(FILE *fp, X509 **x509)
{
    return ASN1_item_d2i_fp(ASN1_ITEM_rptr(X509), fp, x509);
}

int i2d_X509_fp(FILE *fp, X509 *x509)
{
    return ASN1_item_i2d_fp(ASN1_ITEM_rptr(X509), fp, x509);
}
#endif

X509 *d2i_X509_bio(BIO *bp, X509 **x509)
{
    return ASN1_item_d2i_bio(ASN1_ITEM_rptr(X509), bp, x509);
}

int i2d_X509_bio(BIO *bp, X509 *x509)
{
    return ASN1_item_i2d_bio(ASN1_ITEM_rptr(X509), bp, x509);
}

#ifndef OPENSSL_NO_FP_API
X509_CRL *d2i_X509_CRL_fp(FILE *fp, X509_CRL **crl)
{
    return ASN1_item_d2i_fp(ASN1_ITEM_rptr(X509_CRL), fp, crl);
}

int i2d_X509_CRL_fp(FILE *fp, X509_CRL *crl)
{
    return ASN1_item_i2d_fp(ASN1_ITEM_rptr(X509_CRL), fp, crl);
}
#endif

X509_CRL *d2i_X509_CRL_bio(BIO *bp, X509_CRL **crl)
{
    return ASN1_item_d2i_bio(ASN1_ITEM_rptr(X509_CRL), bp, crl);
}

int i2d_X509_CRL_bio(BIO *bp, X509_CRL *crl)
{
    return ASN1_item_i2d_bio(ASN1_ITEM_rptr(X509_CRL), bp, crl);
}

#ifndef OPENSSL_NO_FP_API
X509_REQ *d2i_X509_REQ_fp(FILE *fp, X509_REQ **req)
{
    return ASN1_item_d2i_fp(ASN1_ITEM_rptr(X509_REQ), fp, req);
}

int i2d_X509_REQ_fp(FILE *fp, X509_REQ *req)
{
    return ASN1_item_i2d_fp(ASN1_ITEM_rptr(X509_REQ), fp, req);
}
#endif

X509_REQ *d2i_X509_REQ_bio(BIO *bp, X509_REQ **req)
{
    return ASN1_item_d2i_bio(ASN1_ITEM_rptr(X509_REQ), bp, req);
}

int i2d_X509_REQ_bio(BIO *bp, X509_REQ *req)
{
    return ASN1_item_i2d_bio(ASN1_ITEM_rptr(X509_REQ), bp, req);
}

#ifndef OPENSSL_NO_FP_API

#define IMPLEMENT_D2I_FP(type, name, bio_func) \
  type *name(FILE *fp, type **obj) {           \
    BIO *bio = BIO_new_fp(fp, BIO_NOCLOSE);    \
    if (bio == NULL) {                         \
      return NULL;                             \
    }                                          \
    type *ret = bio_func(bio, obj);            \
    BIO_free(bio);                             \
    return ret;                                \
  }

#define IMPLEMENT_I2D_FP(type, name, bio_func) \
  int name(FILE *fp, type *obj) {              \
    BIO *bio = BIO_new_fp(fp, BIO_NOCLOSE);    \
    if (bio == NULL) {                         \
      return 0;                                \
    }                                          \
    int ret = bio_func(bio, obj);              \
    BIO_free(bio);                             \
    return ret;                                \
  }

IMPLEMENT_D2I_FP(RSA, d2i_RSAPrivateKey_fp, d2i_RSAPrivateKey_bio)
IMPLEMENT_I2D_FP(RSA, i2d_RSAPrivateKey_fp, i2d_RSAPrivateKey_bio)

IMPLEMENT_D2I_FP(RSA, d2i_RSAPublicKey_fp, d2i_RSAPublicKey_bio)
IMPLEMENT_I2D_FP(RSA, i2d_RSAPublicKey_fp, i2d_RSAPublicKey_bio)

IMPLEMENT_D2I_FP(RSA, d2i_RSA_PUBKEY_fp, d2i_RSA_PUBKEY_bio)
IMPLEMENT_I2D_FP(RSA, i2d_RSA_PUBKEY_fp, i2d_RSA_PUBKEY_bio)
#endif

#define IMPLEMENT_D2I_BIO(type, name, d2i_func)         \
  type *name(BIO *bio, type **obj) {                    \
    uint8_t *data;                                      \
    size_t len;                                         \
    if (!BIO_read_asn1(bio, &data, &len, 100 * 1024)) { \
      return NULL;                                      \
    }                                                   \
    const uint8_t *ptr = data;                          \
    type *ret = d2i_func(obj, &ptr, (long)len);         \
    OPENSSL_free(data);                                 \
    return ret;                                         \
  }

#define IMPLEMENT_I2D_BIO(type, name, i2d_func) \
  int name(BIO *bio, type *obj) {               \
    uint8_t *data = NULL;                       \
    int len = i2d_func(obj, &data);             \
    if (len < 0) {                              \
      return 0;                                 \
    }                                           \
    int ret = BIO_write_all(bio, data, len);    \
    OPENSSL_free(data);                         \
    return ret;                                 \
  }

IMPLEMENT_D2I_BIO(RSA, d2i_RSAPrivateKey_bio, d2i_RSAPrivateKey)
IMPLEMENT_I2D_BIO(RSA, i2d_RSAPrivateKey_bio, i2d_RSAPrivateKey)

IMPLEMENT_D2I_BIO(RSA, d2i_RSAPublicKey_bio, d2i_RSAPublicKey)
IMPLEMENT_I2D_BIO(RSA, i2d_RSAPublicKey_bio, i2d_RSAPublicKey)

IMPLEMENT_D2I_BIO(RSA, d2i_RSA_PUBKEY_bio, d2i_RSA_PUBKEY)
IMPLEMENT_I2D_BIO(RSA, i2d_RSA_PUBKEY_bio, i2d_RSA_PUBKEY)

#ifndef OPENSSL_NO_DSA
# ifndef OPENSSL_NO_FP_API
IMPLEMENT_D2I_FP(DSA, d2i_DSAPrivateKey_fp, d2i_DSAPrivateKey_bio)
IMPLEMENT_I2D_FP(DSA, i2d_DSAPrivateKey_fp, i2d_DSAPrivateKey_bio)

IMPLEMENT_D2I_FP(DSA, d2i_DSA_PUBKEY_fp, d2i_DSA_PUBKEY_bio)
IMPLEMENT_I2D_FP(DSA, i2d_DSA_PUBKEY_fp, i2d_DSA_PUBKEY_bio)
# endif

IMPLEMENT_D2I_BIO(DSA, d2i_DSAPrivateKey_bio, d2i_DSAPrivateKey)
IMPLEMENT_I2D_BIO(DSA, i2d_DSAPrivateKey_bio, i2d_DSAPrivateKey)

IMPLEMENT_D2I_BIO(DSA, d2i_DSA_PUBKEY_bio, d2i_DSA_PUBKEY)
IMPLEMENT_I2D_BIO(DSA, i2d_DSA_PUBKEY_bio, i2d_DSA_PUBKEY)
#endif

#ifndef OPENSSL_NO_FP_API
IMPLEMENT_D2I_FP(EC_KEY, d2i_ECPrivateKey_fp, d2i_ECPrivateKey_bio)
IMPLEMENT_I2D_FP(EC_KEY, i2d_ECPrivateKey_fp, i2d_ECPrivateKey_bio)

IMPLEMENT_D2I_FP(EC_KEY, d2i_EC_PUBKEY_fp, d2i_EC_PUBKEY_bio)
IMPLEMENT_I2D_FP(EC_KEY, i2d_EC_PUBKEY_fp, i2d_EC_PUBKEY_bio)
#endif

IMPLEMENT_D2I_BIO(EC_KEY, d2i_ECPrivateKey_bio, d2i_ECPrivateKey)
IMPLEMENT_I2D_BIO(EC_KEY, i2d_ECPrivateKey_bio, i2d_ECPrivateKey)

IMPLEMENT_D2I_BIO(EC_KEY, d2i_EC_PUBKEY_bio, d2i_EC_PUBKEY)
IMPLEMENT_I2D_BIO(EC_KEY, i2d_EC_PUBKEY_bio, i2d_EC_PUBKEY)

int X509_pubkey_digest(const X509 *data, const EVP_MD *type,
                       unsigned char *md, unsigned int *len)
{
    ASN1_BIT_STRING *key;
    key = X509_get0_pubkey_bitstr(data);
    if (!key)
        return 0;
    return EVP_Digest(key->data, key->length, md, len, type, NULL);
}

int X509_digest(const X509 *data, const EVP_MD *type, unsigned char *md,
                unsigned int *len)
{
    return (ASN1_item_digest
            (ASN1_ITEM_rptr(X509), type, (char *)data, md, len));
}

int X509_CRL_digest(const X509_CRL *data, const EVP_MD *type,
                    unsigned char *md, unsigned int *len)
{
    return (ASN1_item_digest
            (ASN1_ITEM_rptr(X509_CRL), type, (char *)data, md, len));
}

int X509_REQ_digest(const X509_REQ *data, const EVP_MD *type,
                    unsigned char *md, unsigned int *len)
{
    return (ASN1_item_digest
            (ASN1_ITEM_rptr(X509_REQ), type, (char *)data, md, len));
}

int X509_NAME_digest(const X509_NAME *data, const EVP_MD *type,
                     unsigned char *md, unsigned int *len)
{
    return (ASN1_item_digest
            (ASN1_ITEM_rptr(X509_NAME), type, (char *)data, md, len));
}

#ifndef OPENSSL_NO_FP_API
IMPLEMENT_D2I_FP(X509_SIG, d2i_PKCS8_fp, d2i_PKCS8_bio)
IMPLEMENT_I2D_FP(X509_SIG, i2d_PKCS8_fp, i2d_PKCS8_bio)
#endif

IMPLEMENT_D2I_BIO(X509_SIG, d2i_PKCS8_bio, d2i_X509_SIG)
IMPLEMENT_I2D_BIO(X509_SIG, i2d_PKCS8_bio, i2d_X509_SIG)

#ifndef OPENSSL_NO_FP_API
IMPLEMENT_D2I_FP(PKCS8_PRIV_KEY_INFO, d2i_PKCS8_PRIV_KEY_INFO_fp,
                 d2i_PKCS8_PRIV_KEY_INFO_bio)
IMPLEMENT_I2D_FP(PKCS8_PRIV_KEY_INFO, i2d_PKCS8_PRIV_KEY_INFO_fp,
                 i2d_PKCS8_PRIV_KEY_INFO_bio)

int i2d_PKCS8PrivateKeyInfo_fp(FILE *fp, EVP_PKEY *key)
{
    PKCS8_PRIV_KEY_INFO *p8inf;
    int ret;
    p8inf = EVP_PKEY2PKCS8(key);
    if (!p8inf)
        return 0;
    ret = i2d_PKCS8_PRIV_KEY_INFO_fp(fp, p8inf);
    PKCS8_PRIV_KEY_INFO_free(p8inf);
    return ret;
}

IMPLEMENT_D2I_FP(EVP_PKEY, d2i_PrivateKey_fp, d2i_PrivateKey_bio)
IMPLEMENT_I2D_FP(EVP_PKEY, i2d_PrivateKey_fp, i2d_PrivateKey_bio)

IMPLEMENT_D2I_FP(EVP_PKEY, d2i_PUBKEY_fp, d2i_PUBKEY_bio)
IMPLEMENT_I2D_FP(EVP_PKEY, i2d_PUBKEY_fp, i2d_PUBKEY_bio)

IMPLEMENT_D2I_BIO(PKCS8_PRIV_KEY_INFO, d2i_PKCS8_PRIV_KEY_INFO_bio,
                  d2i_PKCS8_PRIV_KEY_INFO)
IMPLEMENT_I2D_BIO(PKCS8_PRIV_KEY_INFO, i2d_PKCS8_PRIV_KEY_INFO_bio,
                  i2d_PKCS8_PRIV_KEY_INFO)

int i2d_PKCS8PrivateKeyInfo_bio(BIO *bp, EVP_PKEY *key)
{
    PKCS8_PRIV_KEY_INFO *p8inf;
    int ret;
    p8inf = EVP_PKEY2PKCS8(key);
    if (!p8inf)
        return 0;
    ret = i2d_PKCS8_PRIV_KEY_INFO_bio(bp, p8inf);
    PKCS8_PRIV_KEY_INFO_free(p8inf);
    return ret;
}
#endif

IMPLEMENT_D2I_BIO(EVP_PKEY, d2i_PrivateKey_bio, d2i_AutoPrivateKey)
IMPLEMENT_I2D_BIO(EVP_PKEY, i2d_PrivateKey_bio, i2d_PrivateKey)

IMPLEMENT_D2I_BIO(EVP_PKEY, d2i_PUBKEY_bio, d2i_PUBKEY)
IMPLEMENT_I2D_BIO(EVP_PKEY, i2d_PUBKEY_bio, i2d_PUBKEY)

IMPLEMENT_D2I_BIO(DH, d2i_DHparams_bio, d2i_DHparams)
IMPLEMENT_I2D_BIO(const DH, i2d_DHparams_bio, i2d_DHparams)
