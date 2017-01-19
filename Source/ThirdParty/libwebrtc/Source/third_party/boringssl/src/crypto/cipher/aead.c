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

#include <openssl/cipher.h>
#include <openssl/err.h>

#include "internal.h"
#include "../internal.h"


size_t EVP_AEAD_key_length(const EVP_AEAD *aead) { return aead->key_len; }

size_t EVP_AEAD_nonce_length(const EVP_AEAD *aead) { return aead->nonce_len; }

size_t EVP_AEAD_max_overhead(const EVP_AEAD *aead) { return aead->overhead; }

size_t EVP_AEAD_max_tag_len(const EVP_AEAD *aead) { return aead->max_tag_len; }

void EVP_AEAD_CTX_zero(EVP_AEAD_CTX *ctx) {
  memset(ctx, 0, sizeof(EVP_AEAD_CTX));
}

int EVP_AEAD_CTX_init(EVP_AEAD_CTX *ctx, const EVP_AEAD *aead,
                      const uint8_t *key, size_t key_len, size_t tag_len,
                      ENGINE *impl) {
  if (!aead->init) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_NO_DIRECTION_SET);
    ctx->aead = NULL;
    return 0;
  }
  return EVP_AEAD_CTX_init_with_direction(ctx, aead, key, key_len, tag_len,
                                          evp_aead_open);
}

int EVP_AEAD_CTX_init_with_direction(EVP_AEAD_CTX *ctx, const EVP_AEAD *aead,
                                     const uint8_t *key, size_t key_len,
                                     size_t tag_len,
                                     enum evp_aead_direction_t dir) {
  if (key_len != aead->key_len) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_KEY_SIZE);
    ctx->aead = NULL;
    return 0;
  }

  ctx->aead = aead;

  int ok;
  if (aead->init) {
    ok = aead->init(ctx, key, key_len, tag_len);
  } else {
    ok = aead->init_with_direction(ctx, key, key_len, tag_len, dir);
  }

  if (!ok) {
    ctx->aead = NULL;
  }

  return ok;
}

void EVP_AEAD_CTX_cleanup(EVP_AEAD_CTX *ctx) {
  if (ctx->aead == NULL) {
    return;
  }
  ctx->aead->cleanup(ctx);
  ctx->aead = NULL;
}

/* check_alias returns 1 if |out| is compatible with |in| and 0 otherwise. If
 * |in| and |out| alias, we require that |in| == |out|. */
static int check_alias(const uint8_t *in, size_t in_len, const uint8_t *out,
                       size_t out_len) {
  if (!buffers_alias(in, in_len, out, out_len)) {
    return 1;
  }

  return in == out;
}

int EVP_AEAD_CTX_seal(const EVP_AEAD_CTX *ctx, uint8_t *out, size_t *out_len,
                      size_t max_out_len, const uint8_t *nonce,
                      size_t nonce_len, const uint8_t *in, size_t in_len,
                      const uint8_t *ad, size_t ad_len) {
  size_t possible_out_len = in_len + ctx->aead->overhead;

  if (possible_out_len < in_len /* overflow */) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    goto error;
  }

  if (!check_alias(in, in_len, out, max_out_len)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_OUTPUT_ALIASES_INPUT);
    goto error;
  }

  if (ctx->aead->seal(ctx, out, out_len, max_out_len, nonce, nonce_len, in,
                      in_len, ad, ad_len)) {
    return 1;
  }

error:
  /* In the event of an error, clear the output buffer so that a caller
   * that doesn't check the return value doesn't send raw data. */
  memset(out, 0, max_out_len);
  *out_len = 0;
  return 0;
}

int EVP_AEAD_CTX_open(const EVP_AEAD_CTX *ctx, uint8_t *out, size_t *out_len,
                      size_t max_out_len, const uint8_t *nonce,
                      size_t nonce_len, const uint8_t *in, size_t in_len,
                      const uint8_t *ad, size_t ad_len) {
  if (!check_alias(in, in_len, out, max_out_len)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_OUTPUT_ALIASES_INPUT);
    goto error;
  }

  if (ctx->aead->open(ctx, out, out_len, max_out_len, nonce, nonce_len, in,
                      in_len, ad, ad_len)) {
    return 1;
  }

error:
  /* In the event of an error, clear the output buffer so that a caller
   * that doesn't check the return value doesn't try and process bad
   * data. */
  memset(out, 0, max_out_len);
  *out_len = 0;
  return 0;
}

const EVP_AEAD *EVP_AEAD_CTX_aead(const EVP_AEAD_CTX *ctx) { return ctx->aead; }

int EVP_AEAD_CTX_get_iv(const EVP_AEAD_CTX *ctx, const uint8_t **out_iv,
                        size_t *out_len) {
  if (ctx->aead->get_iv == NULL) {
    return 0;
  }

  return ctx->aead->get_iv(ctx, out_iv, out_len);
}
