/* Copyright (c) 2024, Google Inc.
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

#include <openssl/sha.h>

#include <openssl/mem.h>

#include "../fipsmodule/bcm_interface.h"


int SHA384_Init(SHA512_CTX *sha) {
  BCM_sha384_init(sha);
  return 1;
}

int SHA384_Update(SHA512_CTX *sha, const void *data, size_t len) {
  BCM_sha384_update(sha, data, len);
  return 1;
}

int SHA384_Final(uint8_t out[SHA384_DIGEST_LENGTH], SHA512_CTX *sha) {
  BCM_sha384_final(out, sha);
  return 1;
}

uint8_t *SHA384(const uint8_t *data, size_t len,
                uint8_t out[SHA384_DIGEST_LENGTH]) {
  SHA512_CTX ctx;
  BCM_sha384_init(&ctx);
  BCM_sha384_update(&ctx, data, len);
  BCM_sha384_final(out, &ctx);
  OPENSSL_cleanse(&ctx, sizeof(ctx));
  return out;
}

int SHA512_256_Init(SHA512_CTX *sha) {
  BCM_sha512_256_init(sha);
  return 1;
}

int SHA512_256_Update(SHA512_CTX *sha, const void *data, size_t len) {
  BCM_sha512_256_update(sha, data, len);
  return 1;
}

int SHA512_256_Final(uint8_t out[SHA512_256_DIGEST_LENGTH], SHA512_CTX *sha) {
  BCM_sha512_256_final(out, sha);
  return 1;
}

uint8_t *SHA512_256(const uint8_t *data, size_t len,
                uint8_t out[SHA512_256_DIGEST_LENGTH]) {
  SHA512_CTX ctx;
  BCM_sha512_256_init(&ctx);
  BCM_sha512_256_update(&ctx, data, len);
  BCM_sha512_256_final(out, &ctx);
  OPENSSL_cleanse(&ctx, sizeof(ctx));
  return out;
}

int SHA512_Init(SHA512_CTX *sha) {
  BCM_sha512_init(sha);
  return 1;
}

int SHA512_Update(SHA512_CTX *sha, const void *data, size_t len) {
  BCM_sha512_update(sha, data, len);
  return 1;
}

int SHA512_Final(uint8_t out[SHA512_DIGEST_LENGTH], SHA512_CTX *sha) {
  // Historically this function retured failure if passed NULL, even
  // though other final functions do not.
  if (out == NULL) {
    return 0;
  }
  BCM_sha512_final(out, sha);
  return 1;
}

uint8_t *SHA512(const uint8_t *data, size_t len,
                uint8_t out[SHA512_DIGEST_LENGTH]) {
  SHA512_CTX ctx;
  BCM_sha512_init(&ctx);
  BCM_sha512_update(&ctx, data, len);
  BCM_sha512_final(out, &ctx);
  OPENSSL_cleanse(&ctx, sizeof(ctx));
  return out;
}

void SHA512_Transform(SHA512_CTX *sha, const uint8_t block[SHA512_CBLOCK]) {
  BCM_sha512_transform(sha, block);
}
