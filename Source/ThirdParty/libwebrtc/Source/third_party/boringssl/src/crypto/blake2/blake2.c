/* Copyright (c) 2021, Google Inc.
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

#include <openssl/blake2.h>

#include <openssl/type_check.h>

#include "../internal.h"

// https://tools.ietf.org/html/rfc7693#section-2.6
static const uint64_t kIV[8] = {
    UINT64_C(0x6a09e667f3bcc908), UINT64_C(0xbb67ae8584caa73b),
    UINT64_C(0x3c6ef372fe94f82b), UINT64_C(0xa54ff53a5f1d36f1),
    UINT64_C(0x510e527fade682d1), UINT64_C(0x9b05688c2b3e6c1f),
    UINT64_C(0x1f83d9abfb41bd6b), UINT64_C(0x5be0cd19137e2179),
};

// https://tools.ietf.org/html/rfc7693#section-2.7
static const uint8_t kSigma[10 * 16] = {
    // clang-format off
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3,
    11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4,
    7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8,
    9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13,
    2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9,
    12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11,
    13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10,
    6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5,
    10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0,
    // clang-format on
};

#define RIGHT_ROTATE(v, n) (((v) >> (n)) | ((v) << (64 - (n))))

// https://tools.ietf.org/html/rfc7693#section-3.1
static void blake2b_mix(uint64_t v[16], int a, int b, int c, int d, uint64_t x,
                        uint64_t y) {
  v[a] = v[a] + v[b] + x;
  v[d] = RIGHT_ROTATE(v[d] ^ v[a], 32);
  v[c] = v[c] + v[d];
  v[b] = RIGHT_ROTATE(v[b] ^ v[c], 24);
  v[a] = v[a] + v[b] + y;
  v[d] = RIGHT_ROTATE(v[d] ^ v[a], 16);
  v[c] = v[c] + v[d];
  v[b] = RIGHT_ROTATE(v[b] ^ v[c], 63);
}

static void blake2b_transform(
    BLAKE2B_CTX *b2b,
    const uint64_t block_words[BLAKE2B_CBLOCK / sizeof(uint64_t)],
    size_t num_bytes, int is_final_block) {
  // https://tools.ietf.org/html/rfc7693#section-3.2
  uint64_t v[16];
  OPENSSL_STATIC_ASSERT(sizeof(v) == sizeof(b2b->h) + sizeof(kIV), "");
  OPENSSL_memcpy(v, b2b->h, sizeof(b2b->h));
  OPENSSL_memcpy(&v[8], kIV, sizeof(kIV));

  b2b->t_low += num_bytes;
  if (b2b->t_low < num_bytes) {
    b2b->t_high++;
  }
  v[12] ^= b2b->t_low;
  v[13] ^= b2b->t_high;

  if (is_final_block) {
    v[14] = ~v[14];
  }

  for (int round = 0; round < 12; round++) {
    const uint8_t *const s = &kSigma[16 * (round % 10)];
    blake2b_mix(v, 0, 4, 8, 12, block_words[s[0]], block_words[s[1]]);
    blake2b_mix(v, 1, 5, 9, 13, block_words[s[2]], block_words[s[3]]);
    blake2b_mix(v, 2, 6, 10, 14, block_words[s[4]], block_words[s[5]]);
    blake2b_mix(v, 3, 7, 11, 15, block_words[s[6]], block_words[s[7]]);
    blake2b_mix(v, 0, 5, 10, 15, block_words[s[8]], block_words[s[9]]);
    blake2b_mix(v, 1, 6, 11, 12, block_words[s[10]], block_words[s[11]]);
    blake2b_mix(v, 2, 7, 8, 13, block_words[s[12]], block_words[s[13]]);
    blake2b_mix(v, 3, 4, 9, 14, block_words[s[14]], block_words[s[15]]);
  }

  for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(b2b->h); i++) {
    b2b->h[i] ^= v[i];
    b2b->h[i] ^= v[i + 8];
  }
}

void BLAKE2B256_Init(BLAKE2B_CTX *b2b) {
  OPENSSL_memset(b2b, 0, sizeof(BLAKE2B_CTX));

  OPENSSL_STATIC_ASSERT(sizeof(kIV) == sizeof(b2b->h), "");
  OPENSSL_memcpy(&b2b->h, kIV, sizeof(kIV));

  // https://tools.ietf.org/html/rfc7693#section-2.5
  b2b->h[0] ^= 0x01010000 | BLAKE2B256_DIGEST_LENGTH;
}

void BLAKE2B256_Update(BLAKE2B_CTX *b2b, const void *in_data, size_t len) {
  const uint8_t *data = (const uint8_t *)in_data;

  size_t todo = sizeof(b2b->block.bytes) - b2b->block_used;
  if (todo > len) {
    todo = len;
  }
  OPENSSL_memcpy(&b2b->block.bytes[b2b->block_used], data, todo);
  b2b->block_used += todo;
  data += todo;
  len -= todo;

  if (!len) {
    return;
  }

  // More input remains therefore we must have filled |b2b->block|.
  assert(b2b->block_used == BLAKE2B_CBLOCK);
  blake2b_transform(b2b, b2b->block.words, BLAKE2B_CBLOCK,
                    /*is_final_block=*/0);
  b2b->block_used = 0;

  while (len > BLAKE2B_CBLOCK) {
    uint64_t block_words[BLAKE2B_CBLOCK / sizeof(uint64_t)];
    OPENSSL_memcpy(block_words, data, sizeof(block_words));
    blake2b_transform(b2b, block_words, BLAKE2B_CBLOCK, /*is_final_block=*/0);
    data += BLAKE2B_CBLOCK;
    len -= BLAKE2B_CBLOCK;
  }

  OPENSSL_memcpy(b2b->block.bytes, data, len);
  b2b->block_used = len;
}

void BLAKE2B256_Final(uint8_t out[BLAKE2B256_DIGEST_LENGTH], BLAKE2B_CTX *b2b) {
  OPENSSL_memset(&b2b->block.bytes[b2b->block_used], 0,
                 sizeof(b2b->block.bytes) - b2b->block_used);
  blake2b_transform(b2b, b2b->block.words, b2b->block_used,
                    /*is_final_block=*/1);
  OPENSSL_STATIC_ASSERT(BLAKE2B256_DIGEST_LENGTH <= sizeof(b2b->h), "");
  memcpy(out, b2b->h, BLAKE2B256_DIGEST_LENGTH);
}

void BLAKE2B256(const uint8_t *data, size_t len,
                uint8_t out[BLAKE2B256_DIGEST_LENGTH]) {
  BLAKE2B_CTX ctx;
  BLAKE2B256_Init(&ctx);
  BLAKE2B256_Update(&ctx, data, len);
  BLAKE2B256_Final(out, &ctx);
}
