/* Copyright (c) 2014, Google Inc.
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

#include <string.h>

#include <openssl/chacha.h>
#include <openssl/cipher.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/poly1305.h>

#include "internal.h"
#include "../internal.h"


#define POLY1305_TAG_LEN 16

struct aead_chacha20_poly1305_ctx {
  unsigned char key[32];
  unsigned char tag_len;
};

static int aead_chacha20_poly1305_init(EVP_AEAD_CTX *ctx, const uint8_t *key,
                                       size_t key_len, size_t tag_len) {
  struct aead_chacha20_poly1305_ctx *c20_ctx;

  if (tag_len == 0) {
    tag_len = POLY1305_TAG_LEN;
  }

  if (tag_len > POLY1305_TAG_LEN) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    return 0;
  }

  if (key_len != sizeof(c20_ctx->key)) {
    return 0; /* internal error - EVP_AEAD_CTX_init should catch this. */
  }

  c20_ctx = OPENSSL_malloc(sizeof(struct aead_chacha20_poly1305_ctx));
  if (c20_ctx == NULL) {
    return 0;
  }

  memcpy(c20_ctx->key, key, key_len);
  c20_ctx->tag_len = tag_len;
  ctx->aead_state = c20_ctx;

  return 1;
}

static void aead_chacha20_poly1305_cleanup(EVP_AEAD_CTX *ctx) {
  struct aead_chacha20_poly1305_ctx *c20_ctx = ctx->aead_state;
  OPENSSL_cleanse(c20_ctx->key, sizeof(c20_ctx->key));
  OPENSSL_free(c20_ctx);
}

static void poly1305_update_length(poly1305_state *poly1305, size_t data_len) {
  uint8_t length_bytes[8];
  unsigned i;

  for (i = 0; i < sizeof(length_bytes); i++) {
    length_bytes[i] = data_len;
    data_len >>= 8;
  }

  CRYPTO_poly1305_update(poly1305, length_bytes, sizeof(length_bytes));
}

typedef void (*aead_poly1305_update)(poly1305_state *ctx, const uint8_t *ad,
                                     size_t ad_len, const uint8_t *ciphertext,
                                     size_t ciphertext_len);

/* aead_poly1305 fills |tag| with the authentication tag for the given
 * inputs, using |update| to control the order and format that the inputs are
 * signed/authenticated. */
static void aead_poly1305(aead_poly1305_update update,
                          uint8_t tag[POLY1305_TAG_LEN],
                          const struct aead_chacha20_poly1305_ctx *c20_ctx,
                          const uint8_t nonce[12], const uint8_t *ad,
                          size_t ad_len, const uint8_t *ciphertext,
                          size_t ciphertext_len) {
  alignas(16) uint8_t poly1305_key[32];
  memset(poly1305_key, 0, sizeof(poly1305_key));
  CRYPTO_chacha_20(poly1305_key, poly1305_key, sizeof(poly1305_key),
                   c20_ctx->key, nonce, 0);
  poly1305_state ctx;
  CRYPTO_poly1305_init(&ctx, poly1305_key);
  update(&ctx, ad, ad_len, ciphertext, ciphertext_len);
  CRYPTO_poly1305_finish(&ctx, tag);
}

static int seal_impl(aead_poly1305_update poly1305_update,
                     const EVP_AEAD_CTX *ctx, uint8_t *out, size_t *out_len,
                     size_t max_out_len, const uint8_t nonce[12],
                     const uint8_t *in, size_t in_len, const uint8_t *ad,
                     size_t ad_len) {
  const struct aead_chacha20_poly1305_ctx *c20_ctx = ctx->aead_state;
  const uint64_t in_len_64 = in_len;

  /* |CRYPTO_chacha_20| uses a 32-bit block counter. Therefore we disallow
   * individual operations that work on more than 256GB at a time.
   * |in_len_64| is needed because, on 32-bit platforms, size_t is only
   * 32-bits and this produces a warning because it's always false.
   * Casting to uint64_t inside the conditional is not sufficient to stop
   * the warning. */
  if (in_len_64 >= (UINT64_C(1) << 32) * 64 - 64) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    return 0;
  }

  if (in_len + c20_ctx->tag_len < in_len) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    return 0;
  }

  if (max_out_len < in_len + c20_ctx->tag_len) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BUFFER_TOO_SMALL);
    return 0;
  }

  CRYPTO_chacha_20(out, in, in_len, c20_ctx->key, nonce, 1);

  alignas(16) uint8_t tag[POLY1305_TAG_LEN];
  aead_poly1305(poly1305_update, tag, c20_ctx, nonce, ad, ad_len, out, in_len);

  memcpy(out + in_len, tag, c20_ctx->tag_len);
  *out_len = in_len + c20_ctx->tag_len;
  return 1;
}

static int open_impl(aead_poly1305_update poly1305_update,
                     const EVP_AEAD_CTX *ctx, uint8_t *out, size_t *out_len,
                     size_t max_out_len, const uint8_t nonce[12],
                     const uint8_t *in, size_t in_len, const uint8_t *ad,
                     size_t ad_len) {
  const struct aead_chacha20_poly1305_ctx *c20_ctx = ctx->aead_state;
  size_t plaintext_len;
  const uint64_t in_len_64 = in_len;

  if (in_len < c20_ctx->tag_len) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
    return 0;
  }

  /* |CRYPTO_chacha_20| uses a 32-bit block counter. Therefore we disallow
   * individual operations that work on more than 256GB at a time.
   * |in_len_64| is needed because, on 32-bit platforms, size_t is only
   * 32-bits and this produces a warning because it's always false.
   * Casting to uint64_t inside the conditional is not sufficient to stop
   * the warning. */
  if (in_len_64 >= (UINT64_C(1) << 32) * 64 - 64) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    return 0;
  }

  plaintext_len = in_len - c20_ctx->tag_len;
  alignas(16) uint8_t tag[POLY1305_TAG_LEN];
  aead_poly1305(poly1305_update, tag, c20_ctx, nonce, ad, ad_len, in,
                plaintext_len);
  if (CRYPTO_memcmp(tag, in + plaintext_len, c20_ctx->tag_len) != 0) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
    return 0;
  }

  CRYPTO_chacha_20(out, in, plaintext_len, c20_ctx->key, nonce, 1);
  *out_len = plaintext_len;
  return 1;
}

static void poly1305_update_padded_16(poly1305_state *poly1305,
                                      const uint8_t *data, size_t data_len) {
  static const uint8_t padding[16] = { 0 }; /* Padding is all zeros. */

  CRYPTO_poly1305_update(poly1305, data, data_len);
  if (data_len % 16 != 0) {
    CRYPTO_poly1305_update(poly1305, padding, sizeof(padding) - (data_len % 16));
  }
}

static void poly1305_update(poly1305_state *ctx, const uint8_t *ad,
                            size_t ad_len, const uint8_t *ciphertext,
                            size_t ciphertext_len) {
  poly1305_update_padded_16(ctx, ad, ad_len);
  poly1305_update_padded_16(ctx, ciphertext, ciphertext_len);
  poly1305_update_length(ctx, ad_len);
  poly1305_update_length(ctx, ciphertext_len);
}

static int aead_chacha20_poly1305_seal(const EVP_AEAD_CTX *ctx, uint8_t *out,
                                       size_t *out_len, size_t max_out_len,
                                       const uint8_t *nonce, size_t nonce_len,
                                       const uint8_t *in, size_t in_len,
                                       const uint8_t *ad, size_t ad_len) {
  if (nonce_len != 12) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_NONCE_SIZE);
    return 0;
  }
  return seal_impl(poly1305_update, ctx, out, out_len, max_out_len, nonce, in,
                   in_len, ad, ad_len);
}

static int aead_chacha20_poly1305_open(const EVP_AEAD_CTX *ctx, uint8_t *out,
                                       size_t *out_len, size_t max_out_len,
                                       const uint8_t *nonce, size_t nonce_len,
                                       const uint8_t *in, size_t in_len,
                                       const uint8_t *ad, size_t ad_len) {
  if (nonce_len != 12) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_NONCE_SIZE);
    return 0;
  }
  return open_impl(poly1305_update, ctx, out, out_len, max_out_len, nonce, in,
                   in_len, ad, ad_len);
}

static const EVP_AEAD aead_chacha20_poly1305 = {
    32,                 /* key len */
    12,                 /* nonce len */
    POLY1305_TAG_LEN,   /* overhead */
    POLY1305_TAG_LEN,   /* max tag length */
    aead_chacha20_poly1305_init,
    NULL, /* init_with_direction */
    aead_chacha20_poly1305_cleanup,
    aead_chacha20_poly1305_seal,
    aead_chacha20_poly1305_open,
    NULL,               /* get_iv */
};

const EVP_AEAD *EVP_aead_chacha20_poly1305(void) {
  return &aead_chacha20_poly1305;
}

static void poly1305_update_old(poly1305_state *ctx, const uint8_t *ad,
                                size_t ad_len, const uint8_t *ciphertext,
                                size_t ciphertext_len) {
  CRYPTO_poly1305_update(ctx, ad, ad_len);
  poly1305_update_length(ctx, ad_len);
  CRYPTO_poly1305_update(ctx, ciphertext, ciphertext_len);
  poly1305_update_length(ctx, ciphertext_len);
}

static int aead_chacha20_poly1305_old_seal(
    const EVP_AEAD_CTX *ctx, uint8_t *out, size_t *out_len, size_t max_out_len,
    const uint8_t *nonce, size_t nonce_len, const uint8_t *in, size_t in_len,
    const uint8_t *ad, size_t ad_len) {
  if (nonce_len != 8) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_NONCE_SIZE);
    return 0;
  }
  uint8_t nonce_96[12];
  memset(nonce_96, 0, 4);
  memcpy(nonce_96 + 4, nonce, 8);
  return seal_impl(poly1305_update_old, ctx, out, out_len, max_out_len,
                   nonce_96, in, in_len, ad, ad_len);
}

static int aead_chacha20_poly1305_old_open(
    const EVP_AEAD_CTX *ctx, uint8_t *out, size_t *out_len, size_t max_out_len,
    const uint8_t *nonce, size_t nonce_len, const uint8_t *in, size_t in_len,
    const uint8_t *ad, size_t ad_len) {
  if (nonce_len != 8) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_NONCE_SIZE);
    return 0;
  }
  uint8_t nonce_96[12];
  memset(nonce_96, 0, 4);
  memcpy(nonce_96 + 4, nonce, 8);
  return open_impl(poly1305_update_old, ctx, out, out_len, max_out_len,
                   nonce_96, in, in_len, ad, ad_len);
}

static const EVP_AEAD aead_chacha20_poly1305_old = {
    32,                 /* key len */
    8,                  /* nonce len */
    POLY1305_TAG_LEN,   /* overhead */
    POLY1305_TAG_LEN,   /* max tag length */
    aead_chacha20_poly1305_init,
    NULL, /* init_with_direction */
    aead_chacha20_poly1305_cleanup,
    aead_chacha20_poly1305_old_seal,
    aead_chacha20_poly1305_old_open,
    NULL,               /* get_iv */
};

const EVP_AEAD *EVP_aead_chacha20_poly1305_old(void) {
  return &aead_chacha20_poly1305_old;
}
