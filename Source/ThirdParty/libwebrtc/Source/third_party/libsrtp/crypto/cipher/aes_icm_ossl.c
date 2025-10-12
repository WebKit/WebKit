/*
 * aes_icm_ossl.c
 *
 * AES Integer Counter Mode
 *
 * John A. Foley
 * Cisco Systems, Inc.
 *
 * 2/24/2012:  This module was modified to use CiscoSSL for AES counter
 *             mode.  Eddy Lem contributed the code to allow this.
 *
 * 12/20/2012: Added support for AES-192 and AES-256.
 */

/*
 *
 * Copyright (c) 2013-2017, Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 *   Neither the name of the Cisco Systems, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <openssl/evp.h>
#include "aes_icm_ext.h"
#include "crypto_types.h"
#include "err.h" /* for srtp_debug */
#include "alloc.h"
#include "cipher_types.h"
#include "cipher_test_cases.h"

srtp_debug_module_t srtp_mod_aes_icm = {
    0,             /* debugging is off by default */
    "aes icm ossl" /* printable module name       */
};

/*
 * integer counter mode works as follows:
 *
 * 16 bits
 * <----->
 * +------+------+------+------+------+------+------+------+
 * |           nonce           |    packet index    |  ctr |---+
 * +------+------+------+------+------+------+------+------+   |
 *                                                             |
 * +------+------+------+------+------+------+------+------+   v
 * |                      salt                      |000000|->(+)
 * +------+------+------+------+------+------+------+------+   |
 *                                                             |
 *                                                        +---------+
 *                                                        | encrypt |
 *                                                        +---------+
 *                                                             |
 * +------+------+------+------+------+------+------+------+   |
 * |                    keystream block                    |<--+
 * +------+------+------+------+------+------+------+------+
 *
 * All fields are big-endian
 *
 * ctr is the block counter, which increments from zero for
 * each packet (16 bits wide)
 *
 * packet index is distinct for each packet (48 bits wide)
 *
 * nonce can be distinct across many uses of the same key, or
 * can be a fixed value per key, or can be per-packet randomness
 * (64 bits)
 *
 */

/*
 * This function allocates a new instance of this crypto engine.
 * The key_len parameter should be one of 30, 38, or 46 for
 * AES-128, AES-192, and AES-256 respectively.  Note, this key_len
 * value is inflated, as it also accounts for the 112 bit salt
 * value.  The tlen argument is for the AEAD tag length, which
 * isn't used in counter mode.
 */
static srtp_err_status_t srtp_aes_icm_openssl_alloc(srtp_cipher_t **c,
                                                    int key_len,
                                                    int tlen)
{
    srtp_aes_icm_ctx_t *icm;
    (void)tlen;

    debug_print(srtp_mod_aes_icm, "allocating cipher with key length %d",
                key_len);

    /*
     * Verify the key_len is valid for one of: AES-128/192/256
     */
    if (key_len != SRTP_AES_ICM_128_KEY_LEN_WSALT &&
        key_len != SRTP_AES_ICM_192_KEY_LEN_WSALT &&
        key_len != SRTP_AES_ICM_256_KEY_LEN_WSALT) {
        return srtp_err_status_bad_param;
    }

    /* allocate memory a cipher of type aes_icm */
    *c = (srtp_cipher_t *)srtp_crypto_alloc(sizeof(srtp_cipher_t));
    if (*c == NULL) {
        return srtp_err_status_alloc_fail;
    }

    icm = (srtp_aes_icm_ctx_t *)srtp_crypto_alloc(sizeof(srtp_aes_icm_ctx_t));
    if (icm == NULL) {
        srtp_crypto_free(*c);
        *c = NULL;
        return srtp_err_status_alloc_fail;
    }

    icm->ctx = EVP_CIPHER_CTX_new();
    if (icm->ctx == NULL) {
        srtp_crypto_free(icm);
        srtp_crypto_free(*c);
        *c = NULL;
        return srtp_err_status_alloc_fail;
    }

    /* set pointers */
    (*c)->state = icm;

    /* setup cipher parameters */
    switch (key_len) {
    case SRTP_AES_ICM_128_KEY_LEN_WSALT:
        (*c)->algorithm = SRTP_AES_ICM_128;
        (*c)->type = &srtp_aes_icm_128;
        icm->key_size = SRTP_AES_128_KEY_LEN;
        break;
    case SRTP_AES_ICM_192_KEY_LEN_WSALT:
        (*c)->algorithm = SRTP_AES_ICM_192;
        (*c)->type = &srtp_aes_icm_192;
        icm->key_size = SRTP_AES_192_KEY_LEN;
        break;
    case SRTP_AES_ICM_256_KEY_LEN_WSALT:
        (*c)->algorithm = SRTP_AES_ICM_256;
        (*c)->type = &srtp_aes_icm_256;
        icm->key_size = SRTP_AES_256_KEY_LEN;
        break;
    }

    /* set key size */
    (*c)->key_len = key_len;

    return srtp_err_status_ok;
}

/*
 * This function deallocates an instance of this engine
 */
static srtp_err_status_t srtp_aes_icm_openssl_dealloc(srtp_cipher_t *c)
{
    srtp_aes_icm_ctx_t *ctx;

    if (c == NULL) {
        return srtp_err_status_bad_param;
    }

    /*
     * Free the EVP context
     */
    ctx = (srtp_aes_icm_ctx_t *)c->state;
    if (ctx != NULL) {
        EVP_CIPHER_CTX_free(ctx->ctx);
        /* zeroize the key material */
        octet_string_set_to_zero(ctx, sizeof(srtp_aes_icm_ctx_t));
        srtp_crypto_free(ctx);
    }

    /* free memory */
    srtp_crypto_free(c);

    return srtp_err_status_ok;
}

/*
 * aes_icm_openssl_context_init(...) initializes the aes_icm_context
 * using the value in key[].
 *
 * the key is the secret key
 *
 * the salt is unpredictable (but not necessarily secret) data which
 * randomizes the starting point in the keystream
 */
static srtp_err_status_t srtp_aes_icm_openssl_context_init(void *cv,
                                                           const uint8_t *key)
{
    srtp_aes_icm_ctx_t *c = (srtp_aes_icm_ctx_t *)cv;
    const EVP_CIPHER *evp;

    /*
     * set counter and initial values to 'offset' value, being careful not to
     * go past the end of the key buffer
     */
    v128_set_to_zero(&c->counter);
    v128_set_to_zero(&c->offset);
    memcpy(&c->counter, key + c->key_size, SRTP_SALT_LEN);
    memcpy(&c->offset, key + c->key_size, SRTP_SALT_LEN);

    /* force last two octets of the offset to zero (for srtp compatibility) */
    c->offset.v8[SRTP_SALT_LEN] = c->offset.v8[SRTP_SALT_LEN + 1] = 0;
    c->counter.v8[SRTP_SALT_LEN] = c->counter.v8[SRTP_SALT_LEN + 1] = 0;

    debug_print(srtp_mod_aes_icm, "key:  %s",
                srtp_octet_string_hex_string(key, c->key_size));
    debug_print(srtp_mod_aes_icm, "offset: %s", v128_hex_string(&c->offset));

    switch (c->key_size) {
    case SRTP_AES_256_KEY_LEN:
        evp = EVP_aes_256_ctr();
        break;
    case SRTP_AES_192_KEY_LEN:
        evp = EVP_aes_192_ctr();
        break;
    case SRTP_AES_128_KEY_LEN:
        evp = EVP_aes_128_ctr();
        break;
    default:
        return srtp_err_status_bad_param;
        break;
    }

    EVP_CIPHER_CTX_reset(c->ctx);

    if (!EVP_EncryptInit_ex(c->ctx, evp, NULL, key, NULL)) {
        return srtp_err_status_fail;
    }

    return srtp_err_status_ok;
}

/*
 * aes_icm_set_iv(c, iv) sets the counter value to the exor of iv with
 * the offset
 */
static srtp_err_status_t srtp_aes_icm_openssl_set_iv(
    void *cv,
    uint8_t *iv,
    srtp_cipher_direction_t dir)
{
    srtp_aes_icm_ctx_t *c = (srtp_aes_icm_ctx_t *)cv;
    v128_t nonce;
    (void)dir;

    /* set nonce (for alignment) */
    v128_copy_octet_string(&nonce, iv);

    debug_print(srtp_mod_aes_icm, "setting iv: %s", v128_hex_string(&nonce));

    v128_xor(&c->counter, &c->offset, &nonce);

    debug_print(srtp_mod_aes_icm, "set_counter: %s",
                v128_hex_string(&c->counter));

    if (!EVP_EncryptInit_ex(c->ctx, NULL, NULL, NULL, c->counter.v8)) {
        return srtp_err_status_fail;
    }

    return srtp_err_status_ok;
}

/*
 * This function encrypts a buffer using AES CTR mode
 *
 * Parameters:
 *	c	Crypto context
 *	buf	data to encrypt
 *	enc_len	length of encrypt buffer
 */
static srtp_err_status_t srtp_aes_icm_openssl_encrypt(void *cv,
                                                      unsigned char *buf,
                                                      unsigned int *enc_len)
{
    srtp_aes_icm_ctx_t *c = (srtp_aes_icm_ctx_t *)cv;
    int len = 0;

    debug_print(srtp_mod_aes_icm, "rs0: %s", v128_hex_string(&c->counter));

    if (!EVP_EncryptUpdate(c->ctx, buf, &len, buf, *enc_len)) {
        return srtp_err_status_cipher_fail;
    }
    *enc_len = len;

    if (!EVP_EncryptFinal_ex(c->ctx, buf + len, &len)) {
        return srtp_err_status_cipher_fail;
    }
    *enc_len += len;

    return srtp_err_status_ok;
}

/*
 * Name of this crypto engine
 */
static const char srtp_aes_icm_128_openssl_description[] =
    "AES-128 counter mode using openssl";
static const char srtp_aes_icm_192_openssl_description[] =
    "AES-192 counter mode using openssl";
static const char srtp_aes_icm_256_openssl_description[] =
    "AES-256 counter mode using openssl";

/*
 * This is the function table for this crypto engine.
 * note: the encrypt function is identical to the decrypt function
 */
const srtp_cipher_type_t srtp_aes_icm_128 = {
    srtp_aes_icm_openssl_alloc,           /* */
    srtp_aes_icm_openssl_dealloc,         /* */
    srtp_aes_icm_openssl_context_init,    /* */
    0,                                    /* set_aad */
    srtp_aes_icm_openssl_encrypt,         /* */
    srtp_aes_icm_openssl_encrypt,         /* */
    srtp_aes_icm_openssl_set_iv,          /* */
    0,                                    /* get_tag */
    srtp_aes_icm_128_openssl_description, /* */
    &srtp_aes_icm_128_test_case_0,        /* */
    SRTP_AES_ICM_128                      /* */
};

/*
 * This is the function table for this crypto engine.
 * note: the encrypt function is identical to the decrypt function
 */
const srtp_cipher_type_t srtp_aes_icm_192 = {
    srtp_aes_icm_openssl_alloc,           /* */
    srtp_aes_icm_openssl_dealloc,         /* */
    srtp_aes_icm_openssl_context_init,    /* */
    0,                                    /* set_aad */
    srtp_aes_icm_openssl_encrypt,         /* */
    srtp_aes_icm_openssl_encrypt,         /* */
    srtp_aes_icm_openssl_set_iv,          /* */
    0,                                    /* get_tag */
    srtp_aes_icm_192_openssl_description, /* */
    &srtp_aes_icm_192_test_case_0,        /* */
    SRTP_AES_ICM_192                      /* */
};

/*
 * This is the function table for this crypto engine.
 * note: the encrypt function is identical to the decrypt function
 */
const srtp_cipher_type_t srtp_aes_icm_256 = {
    srtp_aes_icm_openssl_alloc,           /* */
    srtp_aes_icm_openssl_dealloc,         /* */
    srtp_aes_icm_openssl_context_init,    /* */
    0,                                    /* set_aad */
    srtp_aes_icm_openssl_encrypt,         /* */
    srtp_aes_icm_openssl_encrypt,         /* */
    srtp_aes_icm_openssl_set_iv,          /* */
    0,                                    /* get_tag */
    srtp_aes_icm_256_openssl_description, /* */
    &srtp_aes_icm_256_test_case_0,        /* */
    SRTP_AES_ICM_256                      /* */
};
