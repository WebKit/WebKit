/* crypto/pem/pem_info.c */
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

#include <openssl/pem.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <openssl/dsa.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#ifndef OPENSSL_NO_FP_API
STACK_OF(X509_INFO) *PEM_X509_INFO_read(FILE *fp, STACK_OF(X509_INFO) *sk,
                                        pem_password_cb *cb, void *u)
{
    BIO *b = BIO_new_fp(fp, BIO_NOCLOSE);
    if (b == NULL) {
        OPENSSL_PUT_ERROR(PEM, ERR_R_BUF_LIB);
        return 0;
    }
    STACK_OF(X509_INFO) *ret = PEM_X509_INFO_read_bio(b, sk, cb, u);
    BIO_free(b);
    return ret;
}
#endif

enum parse_result_t {
    parse_ok,
    parse_error,
    parse_new_entry,
};

static enum parse_result_t parse_x509(X509_INFO *info, const uint8_t *data,
                                      size_t len, int key_type)
{
    if (info->x509 != NULL) {
        return parse_new_entry;
    }
    info->x509 = d2i_X509(NULL, &data, len);
    return info->x509 != NULL ? parse_ok : parse_error;
}

static enum parse_result_t parse_x509_aux(X509_INFO *info, const uint8_t *data,
                                          size_t len, int key_type)
{
    if (info->x509 != NULL) {
        return parse_new_entry;
    }
    info->x509 = d2i_X509_AUX(NULL, &data, len);
    return info->x509 != NULL ? parse_ok : parse_error;
}

static enum parse_result_t parse_crl(X509_INFO *info, const uint8_t *data,
                                     size_t len, int key_type)
{
    if (info->crl != NULL) {
        return parse_new_entry;
    }
    info->crl = d2i_X509_CRL(NULL, &data, len);
    return info->crl != NULL ? parse_ok : parse_error;
}

static enum parse_result_t parse_key(X509_INFO *info, const uint8_t *data,
                                     size_t len, int key_type)
{
    if (info->x_pkey != NULL) {
        return parse_new_entry;
    }
    info->x_pkey = X509_PKEY_new();
    if (info->x_pkey == NULL) {
        return parse_error;
    }
    info->x_pkey->dec_pkey = d2i_PrivateKey(key_type, NULL, &data, len);
    return info->x_pkey->dec_pkey != NULL ? parse_ok : parse_error;
}

STACK_OF(X509_INFO) *PEM_X509_INFO_read_bio(BIO *bp, STACK_OF(X509_INFO) *sk,
                                            pem_password_cb *cb, void *u)
{
    X509_INFO *info = NULL;
    char *name = NULL, *header = NULL;
    unsigned char *data = NULL;
    long len;
    int ok = 0;
    STACK_OF(X509_INFO) *ret = NULL;

    if (sk == NULL) {
        ret = sk_X509_INFO_new_null();
        if (ret == NULL) {
            OPENSSL_PUT_ERROR(PEM, ERR_R_MALLOC_FAILURE);
            return NULL;
        }
    } else {
        ret = sk;
    }
    size_t orig_num = sk_X509_INFO_num(ret);

    info = X509_INFO_new();
    if (info == NULL) {
        goto err;
    }

    for (;;) {
        if (!PEM_read_bio(bp, &name, &header, &data, &len)) {
            uint32_t error = ERR_peek_last_error();
            if (ERR_GET_LIB(error) == ERR_LIB_PEM &&
                ERR_GET_REASON(error) == PEM_R_NO_START_LINE) {
                ERR_clear_error();
                break;
            }
            goto err;
        }

        enum parse_result_t (*parse_function)(X509_INFO *, const uint8_t *,
                                              size_t, int) = NULL;
        int key_type = EVP_PKEY_NONE;
        if (strcmp(name, PEM_STRING_X509) == 0 ||
            strcmp(name, PEM_STRING_X509_OLD) == 0) {
            parse_function = parse_x509;
        } else if (strcmp(name, PEM_STRING_X509_TRUSTED) == 0) {
            parse_function = parse_x509_aux;
        } else if (strcmp(name, PEM_STRING_X509_CRL) == 0) {
            parse_function = parse_crl;
        } else if (strcmp(name, PEM_STRING_RSA) == 0) {
            parse_function = parse_key;
            key_type = EVP_PKEY_RSA;
        } else if (strcmp(name, PEM_STRING_DSA) == 0) {
            parse_function = parse_key;
            key_type = EVP_PKEY_DSA;
        } else if (strcmp(name, PEM_STRING_ECPRIVATEKEY) == 0) {
            parse_function = parse_key;
            key_type = EVP_PKEY_EC;
        }

        /* If a private key has a header, assume it is encrypted. */
        if (key_type != EVP_PKEY_NONE && strlen(header) > 10) {
            if (info->x_pkey != NULL) {
                if (!sk_X509_INFO_push(ret, info)) {
                    goto err;
                }
                info = X509_INFO_new();
                if (info == NULL) {
                    goto err;
                }
            }
            /* Historically, raw entries pushed an empty key. */
            info->x_pkey = X509_PKEY_new();
            if (info->x_pkey == NULL ||
                !PEM_get_EVP_CIPHER_INFO(header, &info->enc_cipher)) {
                goto err;
            }
            info->enc_data = (char *)data;
            info->enc_len = (int)len;
            data = NULL;
        } else if (parse_function != NULL) {
            EVP_CIPHER_INFO cipher;
            if (!PEM_get_EVP_CIPHER_INFO(header, &cipher) ||
                !PEM_do_header(&cipher, data, &len, cb, u)) {
                goto err;
            }
            enum parse_result_t result =
                parse_function(info, data, len, key_type);
            if (result == parse_new_entry) {
                if (!sk_X509_INFO_push(ret, info)) {
                    goto err;
                }
                info = X509_INFO_new();
                if (info == NULL) {
                    goto err;
                }
                result = parse_function(info, data, len, key_type);
            }
            if (result != parse_ok) {
                OPENSSL_PUT_ERROR(PEM, ERR_R_ASN1_LIB);
                goto err;
            }
        }
        OPENSSL_free(name);
        OPENSSL_free(header);
        OPENSSL_free(data);
        name = NULL;
        header = NULL;
        data = NULL;
    }

    /* Push the last entry on the stack if not empty. */
    if (info->x509 != NULL || info->crl != NULL ||
        info->x_pkey != NULL || info->enc_data != NULL) {
        if (!sk_X509_INFO_push(ret, info)) {
            goto err;
        }
        info = NULL;
    }

    ok = 1;

 err:
    X509_INFO_free(info);
    if (!ok) {
        while (sk_X509_INFO_num(ret) > orig_num) {
            X509_INFO_free(sk_X509_INFO_pop(ret));
        }
        if (ret != sk) {
            sk_X509_INFO_free(ret);
        }
        ret = NULL;
    }

    OPENSSL_free(name);
    OPENSSL_free(header);
    OPENSSL_free(data);
    return ret;
}

/* A TJH addition */
int PEM_X509_INFO_write_bio(BIO *bp, X509_INFO *xi, EVP_CIPHER *enc,
                            unsigned char *kstr, int klen,
                            pem_password_cb *cb, void *u)
{
    int i, ret = 0;
    unsigned char *data = NULL;
    const char *objstr = NULL;
    char buf[PEM_BUFSIZE];
    unsigned char *iv = NULL;
    unsigned iv_len = 0;

    if (enc != NULL) {
        iv_len = EVP_CIPHER_iv_length(enc);
        objstr = OBJ_nid2sn(EVP_CIPHER_nid(enc));
        if (objstr == NULL) {
            OPENSSL_PUT_ERROR(PEM, PEM_R_UNSUPPORTED_CIPHER);
            goto err;
        }
    }

    /*
     * now for the fun part ... if we have a private key then we have to be
     * able to handle a not-yet-decrypted key being written out correctly ...
     * if it is decrypted or it is non-encrypted then we use the base code
     */
    if (xi->x_pkey != NULL) {
        if ((xi->enc_data != NULL) && (xi->enc_len > 0)) {
            if (enc == NULL) {
                OPENSSL_PUT_ERROR(PEM, PEM_R_CIPHER_IS_NULL);
                goto err;
            }

            /* copy from weirdo names into more normal things */
            iv = xi->enc_cipher.iv;
            data = (unsigned char *)xi->enc_data;
            i = xi->enc_len;

            /*
             * we take the encryption data from the internal stuff rather
             * than what the user has passed us ... as we have to match
             * exactly for some strange reason
             */
            objstr = OBJ_nid2sn(EVP_CIPHER_nid(xi->enc_cipher.cipher));
            if (objstr == NULL) {
                OPENSSL_PUT_ERROR(PEM, PEM_R_UNSUPPORTED_CIPHER);
                goto err;
            }

            /* create the right magic header stuff */
            assert(strlen(objstr) + 23 + 2 * iv_len + 13 <= sizeof buf);
            buf[0] = '\0';
            PEM_proc_type(buf, PEM_TYPE_ENCRYPTED);
            PEM_dek_info(buf, objstr, iv_len, (char *)iv);

            /* use the normal code to write things out */
            i = PEM_write_bio(bp, PEM_STRING_RSA, buf, data, i);
            if (i <= 0)
                goto err;
        } else {
            /* Add DSA/DH */
            /* normal optionally encrypted stuff */
            if (PEM_write_bio_RSAPrivateKey(bp,
                                            xi->x_pkey->dec_pkey->pkey.rsa,
                                            enc, kstr, klen, cb, u) <= 0)
                goto err;
        }
    }

    /* if we have a certificate then write it out now */
    if ((xi->x509 != NULL) && (PEM_write_bio_X509(bp, xi->x509) <= 0))
        goto err;

    /*
     * we are ignoring anything else that is loaded into the X509_INFO
     * structure for the moment ... as I don't need it so I'm not coding it
     * here and Eric can do it when this makes it into the base library --tjh
     */

    ret = 1;

err:
  OPENSSL_cleanse(buf, PEM_BUFSIZE);
  return ret;
}
