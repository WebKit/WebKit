/* Copyright (c) 2018, Google Inc.
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

#include <openssl/aead.h>

#include <assert.h>

#include <openssl/cipher.h>
#include <openssl/err.h>
#include <openssl/mem.h>

#include "../fipsmodule/cipher/internal.h"


#define EVP_AEAD_AES_CCM_MAX_TAG_LEN 16

struct aead_aes_ccm_ctx {
  union {
    double align;
    AES_KEY ks;
  } ks;
  CCM128_CONTEXT ccm;
};

OPENSSL_STATIC_ASSERT(sizeof(((EVP_AEAD_CTX *)NULL)->state) >=
                          sizeof(struct aead_aes_ccm_ctx),
                      "AEAD state is too small");
#if defined(__GNUC__) || defined(__clang__)
OPENSSL_STATIC_ASSERT(alignof(union evp_aead_ctx_st_state) >=
                          alignof(struct aead_aes_ccm_ctx),
                      "AEAD state has insufficient alignment");
#endif

static int aead_aes_ccm_init(EVP_AEAD_CTX *ctx, const uint8_t *key,
                             size_t key_len, size_t tag_len, unsigned M,
                             unsigned L) {
  assert(M == EVP_AEAD_max_overhead(ctx->aead));
  assert(M == EVP_AEAD_max_tag_len(ctx->aead));
  assert(15 - L == EVP_AEAD_nonce_length(ctx->aead));

  if (key_len != EVP_AEAD_key_length(ctx->aead)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_KEY_LENGTH);
    return 0;  // EVP_AEAD_CTX_init should catch this.
  }

  if (tag_len == EVP_AEAD_DEFAULT_TAG_LENGTH) {
    tag_len = M;
  }

  if (tag_len != M) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TAG_TOO_LARGE);
    return 0;
  }

  struct aead_aes_ccm_ctx *ccm_ctx = (struct aead_aes_ccm_ctx *)&ctx->state;

  block128_f block;
  ctr128_f ctr = aes_ctr_set_key(&ccm_ctx->ks.ks, NULL, &block, key, key_len);
  ctx->tag_len = tag_len;
  if (!CRYPTO_ccm128_init(&ccm_ctx->ccm, &ccm_ctx->ks.ks, block, ctr, M, L)) {
    OPENSSL_PUT_ERROR(CIPHER, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  return 1;
}

static void aead_aes_ccm_cleanup(EVP_AEAD_CTX *ctx) {}

static int aead_aes_ccm_seal_scatter(
    const EVP_AEAD_CTX *ctx, uint8_t *out, uint8_t *out_tag,
    size_t *out_tag_len, size_t max_out_tag_len, const uint8_t *nonce,
    size_t nonce_len, const uint8_t *in, size_t in_len, const uint8_t *extra_in,
    size_t extra_in_len, const uint8_t *ad, size_t ad_len) {
  const struct aead_aes_ccm_ctx *ccm_ctx =
      (struct aead_aes_ccm_ctx *)&ctx->state;

  if (in_len > CRYPTO_ccm128_max_input(&ccm_ctx->ccm)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    return 0;
  }

  if (max_out_tag_len < ctx->tag_len) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BUFFER_TOO_SMALL);
    return 0;
  }

  if (nonce_len != EVP_AEAD_nonce_length(ctx->aead)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_INVALID_NONCE_SIZE);
    return 0;
  }

  if (!CRYPTO_ccm128_encrypt(&ccm_ctx->ccm, &ccm_ctx->ks.ks, out, out_tag,
                             ctx->tag_len, nonce, nonce_len, in, in_len, ad,
                             ad_len)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    return 0;
  }

  *out_tag_len = ctx->tag_len;
  return 1;
}

static int aead_aes_ccm_open_gather(const EVP_AEAD_CTX *ctx, uint8_t *out,
                                    const uint8_t *nonce, size_t nonce_len,
                                    const uint8_t *in, size_t in_len,
                                    const uint8_t *in_tag, size_t in_tag_len,
                                    const uint8_t *ad, size_t ad_len) {
  const struct aead_aes_ccm_ctx *ccm_ctx =
      (struct aead_aes_ccm_ctx *)&ctx->state;

  if (in_len > CRYPTO_ccm128_max_input(&ccm_ctx->ccm)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    return 0;
  }

  if (nonce_len != EVP_AEAD_nonce_length(ctx->aead)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_INVALID_NONCE_SIZE);
    return 0;
  }

  if (in_tag_len != ctx->tag_len) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
    return 0;
  }

  uint8_t tag[EVP_AEAD_AES_CCM_MAX_TAG_LEN];
  assert(ctx->tag_len <= EVP_AEAD_AES_CCM_MAX_TAG_LEN);
  if (!CRYPTO_ccm128_decrypt(&ccm_ctx->ccm, &ccm_ctx->ks.ks, out, tag,
                             ctx->tag_len, nonce, nonce_len, in, in_len, ad,
                             ad_len)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    return 0;
  }

  if (CRYPTO_memcmp(tag, in_tag, ctx->tag_len) != 0) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
    return 0;
  }

  return 1;
}

static int aead_aes_ccm_bluetooth_init(EVP_AEAD_CTX *ctx, const uint8_t *key,
                                       size_t key_len, size_t tag_len) {
  return aead_aes_ccm_init(ctx, key, key_len, tag_len, 4, 2);
}

static const EVP_AEAD aead_aes_128_ccm_bluetooth = {
    16,  // key length (AES-128)
    13,  // nonce length
    4,   // overhead
    4,   // max tag length
    0,   // seal_scatter_supports_extra_in

    aead_aes_ccm_bluetooth_init,
    NULL /* init_with_direction */,
    aead_aes_ccm_cleanup,
    NULL /* open */,
    aead_aes_ccm_seal_scatter,
    aead_aes_ccm_open_gather,
    NULL /* get_iv */,
    NULL /* tag_len */,
};

const EVP_AEAD *EVP_aead_aes_128_ccm_bluetooth(void) {
  return &aead_aes_128_ccm_bluetooth;
}

static int aead_aes_ccm_bluetooth_8_init(EVP_AEAD_CTX *ctx, const uint8_t *key,
                                         size_t key_len, size_t tag_len) {
  return aead_aes_ccm_init(ctx, key, key_len, tag_len, 8, 2);
}

static const EVP_AEAD aead_aes_128_ccm_bluetooth_8 = {
    16,  // key length (AES-128)
    13,  // nonce length
    8,   // overhead
    8,   // max tag length
    0,   // seal_scatter_supports_extra_in

    aead_aes_ccm_bluetooth_8_init,
    NULL /* init_with_direction */,
    aead_aes_ccm_cleanup,
    NULL /* open */,
    aead_aes_ccm_seal_scatter,
    aead_aes_ccm_open_gather,
    NULL /* get_iv */,
    NULL /* tag_len */,
};

const EVP_AEAD *EVP_aead_aes_128_ccm_bluetooth_8(void) {
  return &aead_aes_128_ccm_bluetooth_8;
}
