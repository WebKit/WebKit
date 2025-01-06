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

int SHA1_Init(SHA_CTX *sha) {
  BCM_sha1_init(sha);
  return 1;
}

int SHA1_Update(SHA_CTX *sha, const void *data, size_t len) {
  BCM_sha1_update(sha, data, len);
  return 1;
}

int SHA1_Final(uint8_t out[SHA_DIGEST_LENGTH], SHA_CTX *sha) {
  BCM_sha1_final(out, sha);
  return 1;
}

uint8_t *SHA1(const uint8_t *data, size_t len, uint8_t out[SHA_DIGEST_LENGTH]) {
  SHA_CTX ctx;
  BCM_sha1_init(&ctx);
  BCM_sha1_update(&ctx, data, len);
  BCM_sha1_final(out, &ctx);
  OPENSSL_cleanse(&ctx, sizeof(ctx));
  return out;
}

void SHA1_Transform(SHA_CTX *sha, const uint8_t block[SHA_CBLOCK]) {
  BCM_sha1_transform(sha, block);
}

void CRYPTO_fips_186_2_prf(uint8_t *out, size_t out_len,
                           const uint8_t xkey[SHA_DIGEST_LENGTH]) {
  BCM_fips_186_2_prf(out, out_len, xkey);
}
