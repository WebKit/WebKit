/* crypto/x509/x509_req.c */
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

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

X509_REQ *X509_to_X509_REQ(X509 *x, EVP_PKEY *pkey, const EVP_MD *md)
{
    X509_REQ *ret;
    X509_REQ_INFO *ri;
    int i;
    EVP_PKEY *pktmp;

    ret = X509_REQ_new();
    if (ret == NULL) {
        OPENSSL_PUT_ERROR(X509, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    ri = ret->req_info;

    ri->version->length = 1;
    ri->version->data = (unsigned char *)OPENSSL_malloc(1);
    if (ri->version->data == NULL)
        goto err;
    ri->version->data[0] = 0;   /* version == 0 */

    if (!X509_REQ_set_subject_name(ret, X509_get_subject_name(x)))
        goto err;

    pktmp = X509_get_pubkey(x);
    if (pktmp == NULL)
        goto err;
    i = X509_REQ_set_pubkey(ret, pktmp);
    EVP_PKEY_free(pktmp);
    if (!i)
        goto err;

    if (pkey != NULL) {
        if (!X509_REQ_sign(ret, pkey, md))
            goto err;
    }
    return (ret);
 err:
    X509_REQ_free(ret);
    return (NULL);
}

long X509_REQ_get_version(const X509_REQ *req)
{
    return ASN1_INTEGER_get(req->req_info->version);
}

X509_NAME *X509_REQ_get_subject_name(const X509_REQ *req)
{
    return req->req_info->subject;
}

EVP_PKEY *X509_REQ_get_pubkey(X509_REQ *req)
{
    if ((req == NULL) || (req->req_info == NULL))
        return (NULL);
    return (X509_PUBKEY_get(req->req_info->pubkey));
}

int X509_REQ_check_private_key(X509_REQ *x, EVP_PKEY *k)
{
    EVP_PKEY *xk = NULL;
    int ok = 0;

    xk = X509_REQ_get_pubkey(x);
    switch (EVP_PKEY_cmp(xk, k)) {
    case 1:
        ok = 1;
        break;
    case 0:
        OPENSSL_PUT_ERROR(X509, X509_R_KEY_VALUES_MISMATCH);
        break;
    case -1:
        OPENSSL_PUT_ERROR(X509, X509_R_KEY_TYPE_MISMATCH);
        break;
    case -2:
        if (k->type == EVP_PKEY_EC) {
            OPENSSL_PUT_ERROR(X509, ERR_R_EC_LIB);
            break;
        }
        if (k->type == EVP_PKEY_DH) {
            /* No idea */
            OPENSSL_PUT_ERROR(X509, X509_R_CANT_CHECK_DH_KEY);
            break;
        }
        OPENSSL_PUT_ERROR(X509, X509_R_UNKNOWN_KEY_TYPE);
    }

    EVP_PKEY_free(xk);
    return (ok);
}

int X509_REQ_extension_nid(int req_nid)
{
    return req_nid == NID_ext_req || req_nid == NID_ms_ext_req;
}

STACK_OF(X509_EXTENSION) *X509_REQ_get_extensions(X509_REQ *req)
{
    if (req == NULL || req->req_info == NULL) {
        return NULL;
    }

    int idx = X509_REQ_get_attr_by_NID(req, NID_ext_req, -1);
    if (idx == -1) {
        idx = X509_REQ_get_attr_by_NID(req, NID_ms_ext_req, -1);
    }
    if (idx == -1) {
        return NULL;
    }

    X509_ATTRIBUTE *attr = X509_REQ_get_attr(req, idx);
    ASN1_TYPE *ext = X509_ATTRIBUTE_get0_type(attr, 0);
    if (!ext || ext->type != V_ASN1_SEQUENCE) {
        return NULL;
    }
    const unsigned char *p = ext->value.sequence->data;
    return (STACK_OF(X509_EXTENSION) *)
        ASN1_item_d2i(NULL, &p, ext->value.sequence->length,
                      ASN1_ITEM_rptr(X509_EXTENSIONS));
}

/*
 * Add a STACK_OF extensions to a certificate request: allow alternative OIDs
 * in case we want to create a non standard one.
 */

int X509_REQ_add_extensions_nid(X509_REQ *req,
                                const STACK_OF(X509_EXTENSION) *exts, int nid)
{
    /* Generate encoding of extensions */
    unsigned char *ext = NULL;
    int ext_len = ASN1_item_i2d((ASN1_VALUE *)exts, &ext,
                                ASN1_ITEM_rptr(X509_EXTENSIONS));
    if (ext_len <= 0) {
        return 0;
    }
    int ret = X509_REQ_add1_attr_by_NID(req, nid, V_ASN1_SEQUENCE, ext,
                                        ext_len);
    OPENSSL_free(ext);
    return ret;
}

/* This is the normal usage: use the "official" OID */
int X509_REQ_add_extensions(X509_REQ *req,
                            const STACK_OF(X509_EXTENSION) *exts)
{
    return X509_REQ_add_extensions_nid(req, exts, NID_ext_req);
}

/* Request attribute functions */

int X509_REQ_get_attr_count(const X509_REQ *req)
{
    return X509at_get_attr_count(req->req_info->attributes);
}

int X509_REQ_get_attr_by_NID(const X509_REQ *req, int nid, int lastpos)
{
    return X509at_get_attr_by_NID(req->req_info->attributes, nid, lastpos);
}

int X509_REQ_get_attr_by_OBJ(const X509_REQ *req, const ASN1_OBJECT *obj,
                             int lastpos)
{
    return X509at_get_attr_by_OBJ(req->req_info->attributes, obj, lastpos);
}

X509_ATTRIBUTE *X509_REQ_get_attr(const X509_REQ *req, int loc)
{
    return X509at_get_attr(req->req_info->attributes, loc);
}

X509_ATTRIBUTE *X509_REQ_delete_attr(X509_REQ *req, int loc)
{
    return X509at_delete_attr(req->req_info->attributes, loc);
}

int X509_REQ_add1_attr(X509_REQ *req, X509_ATTRIBUTE *attr)
{
    if (X509at_add1_attr(&req->req_info->attributes, attr))
        return 1;
    return 0;
}

int X509_REQ_add1_attr_by_OBJ(X509_REQ *req,
                              const ASN1_OBJECT *obj, int attrtype,
                              const unsigned char *data, int len)
{
    if (X509at_add1_attr_by_OBJ(&req->req_info->attributes, obj,
                                attrtype, data, len))
        return 1;
    return 0;
}

int X509_REQ_add1_attr_by_NID(X509_REQ *req,
                              int nid, int attrtype,
                              const unsigned char *data, int len)
{
    if (X509at_add1_attr_by_NID(&req->req_info->attributes, nid,
                                attrtype, data, len))
        return 1;
    return 0;
}

int X509_REQ_add1_attr_by_txt(X509_REQ *req,
                              const char *attrname, int attrtype,
                              const unsigned char *data, int len)
{
    if (X509at_add1_attr_by_txt(&req->req_info->attributes, attrname,
                                attrtype, data, len))
        return 1;
    return 0;
}

void X509_REQ_get0_signature(const X509_REQ *req, const ASN1_BIT_STRING **psig,
                             const X509_ALGOR **palg)
{
    if (psig != NULL)
        *psig = req->signature;
    if (palg != NULL)
        *palg = req->sig_alg;
}

int X509_REQ_get_signature_nid(const X509_REQ *req)
{
    return OBJ_obj2nid(req->sig_alg->algorithm);
}

int i2d_re_X509_REQ_tbs(X509_REQ *req, unsigned char **pp)
{
    req->req_info->enc.modified = 1;
    return i2d_X509_REQ_INFO(req->req_info, pp);
}
