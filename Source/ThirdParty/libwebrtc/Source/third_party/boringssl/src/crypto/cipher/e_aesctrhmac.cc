// Copyright 2017 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <openssl/aead.h>

#include <assert.h>

#include <openssl/cipher.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/sha2.h>
#include <openssl/span.h>

#include "../fipsmodule/aes/internal.h"
#include "../fipsmodule/cipher/internal.h"


using namespace bssl;

#define EVP_AEAD_AES_CTR_HMAC_SHA256_TAG_LEN SHA256_DIGEST_LENGTH
#define EVP_AEAD_AES_CTR_HMAC_SHA256_NONCE_LEN 12

struct aead_aes_ctr_hmac_sha256_ctx {
  union {
    double align;
    AES_KEY ks;
  } ks;
  ctr128_f ctr;
  block128_f block;
  SHA256_CTX inner_init_state;
  SHA256_CTX outer_init_state;
};

static_assert(sizeof(((EVP_AEAD_CTX *)nullptr)->state) >=
                  sizeof(struct aead_aes_ctr_hmac_sha256_ctx),
              "AEAD state is too small");
static_assert(alignof(union evp_aead_ctx_st_state) >=
                  alignof(struct aead_aes_ctr_hmac_sha256_ctx),
              "AEAD state has insufficient alignment");

static void hmac_init(SHA256_CTX *out_inner, SHA256_CTX *out_outer,
                      const uint8_t hmac_key[32]) {
  static const size_t hmac_key_len = 32;
  uint8_t block[SHA256_CBLOCK];
  OPENSSL_memcpy(block, hmac_key, hmac_key_len);
  OPENSSL_memset(block + hmac_key_len, 0x36, sizeof(block) - hmac_key_len);

  unsigned i;
  for (i = 0; i < hmac_key_len; i++) {
    block[i] ^= 0x36;
  }

  SHA256_Init(out_inner);
  SHA256_Update(out_inner, block, sizeof(block));

  OPENSSL_memset(block + hmac_key_len, 0x5c, sizeof(block) - hmac_key_len);
  for (i = 0; i < hmac_key_len; i++) {
    block[i] ^= (0x36 ^ 0x5c);
  }

  SHA256_Init(out_outer);
  SHA256_Update(out_outer, block, sizeof(block));
}

static int aead_aes_ctr_hmac_sha256_init(EVP_AEAD_CTX *ctx, const uint8_t *key,
                                         size_t key_len, size_t tag_len) {
  struct aead_aes_ctr_hmac_sha256_ctx *aes_ctx =
      (struct aead_aes_ctr_hmac_sha256_ctx *)&ctx->state;
  static const size_t hmac_key_len = 32;

  if (key_len < hmac_key_len) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_KEY_LENGTH);
    return 0;  // EVP_AEAD_CTX_init should catch this.
  }

  const size_t aes_key_len = key_len - hmac_key_len;
  if (aes_key_len != 16 && aes_key_len != 32) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_KEY_LENGTH);
    return 0;  // EVP_AEAD_CTX_init should catch this.
  }

  if (tag_len == EVP_AEAD_DEFAULT_TAG_LENGTH) {
    tag_len = EVP_AEAD_AES_CTR_HMAC_SHA256_TAG_LEN;
  }

  if (tag_len > EVP_AEAD_AES_CTR_HMAC_SHA256_TAG_LEN) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TAG_TOO_LARGE);
    return 0;
  }

  aes_ctx->ctr = aes_ctr_set_key(&aes_ctx->ks.ks, nullptr, &aes_ctx->block, key,
                                 aes_key_len);
  ctx->tag_len = tag_len;
  hmac_init(&aes_ctx->inner_init_state, &aes_ctx->outer_init_state,
            key + aes_key_len);

  return 1;
}

static void aead_aes_ctr_hmac_sha256_cleanup(EVP_AEAD_CTX *ctx) {}

static void hmac_update_uint64(SHA256_CTX *sha256, uint64_t value) {
  unsigned i;
  uint8_t bytes[8];

  for (i = 0; i < sizeof(bytes); i++) {
    bytes[i] = value & 0xff;
    value >>= 8;
  }
  SHA256_Update(sha256, bytes, sizeof(bytes));
}

static void hmac_calculate(uint8_t out[SHA256_DIGEST_LENGTH],
                           const SHA256_CTX *inner_init_state,
                           const SHA256_CTX *outer_init_state,
                           Span<const CRYPTO_IVEC> aadvecs,
                           const uint8_t *nonce,
                           Span<const CRYPTO_IOVEC> iovecs, bool encrypt) {
  size_t ad_len = bssl::iovec::TotalLength(aadvecs);
  SHA256_CTX sha256;
  OPENSSL_memcpy(&sha256, inner_init_state, sizeof(sha256));
  hmac_update_uint64(&sha256, ad_len);
  hmac_update_uint64(&sha256, bssl::iovec::TotalLength(iovecs));
  SHA256_Update(&sha256, nonce, EVP_AEAD_AES_CTR_HMAC_SHA256_NONCE_LEN);
  for (const CRYPTO_IVEC &aadvec : aadvecs) {
    SHA256_Update(&sha256, aadvec.in, aadvec.len);
  }

  // Pad with zeros to the end of the SHA-256 block.
  const unsigned num_padding =
      (SHA256_CBLOCK - ((sizeof(uint64_t) * 2 +
                         EVP_AEAD_AES_CTR_HMAC_SHA256_NONCE_LEN + ad_len) %
                        SHA256_CBLOCK)) %
      SHA256_CBLOCK;
  uint8_t padding[SHA256_CBLOCK];
  OPENSSL_memset(padding, 0, num_padding);
  SHA256_Update(&sha256, padding, num_padding);

  for (const CRYPTO_IOVEC &iovec : iovecs) {
    SHA256_Update(&sha256, encrypt ? iovec.out : iovec.in, iovec.len);
  }

  uint8_t inner_digest[SHA256_DIGEST_LENGTH];
  SHA256_Final(inner_digest, &sha256);

  OPENSSL_memcpy(&sha256, outer_init_state, sizeof(sha256));
  SHA256_Update(&sha256, inner_digest, sizeof(inner_digest));
  SHA256_Final(out, &sha256);
}

static void aead_aes_ctr_hmac_sha256_crypt(
    const struct aead_aes_ctr_hmac_sha256_ctx *aes_ctx,
    Span<const CRYPTO_IOVEC> iovecs, const uint8_t *nonce) {
  uint8_t partial_block_buffer[AES_BLOCK_SIZE];
  unsigned partial_block_offset = 0;
  OPENSSL_memset(partial_block_buffer, 0, sizeof(partial_block_buffer));

  uint8_t counter[AES_BLOCK_SIZE];
  OPENSSL_memcpy(counter, nonce, EVP_AEAD_AES_CTR_HMAC_SHA256_NONCE_LEN);
  OPENSSL_memset(counter + EVP_AEAD_AES_CTR_HMAC_SHA256_NONCE_LEN, 0, 4);

  for (const CRYPTO_IOVEC &iovec : iovecs) {
    CRYPTO_ctr128_encrypt_ctr32(iovec.in, iovec.out, iovec.len, &aes_ctx->ks.ks,
                                counter, partial_block_buffer,
                                &partial_block_offset, aes_ctx->ctr);
  }
}

static int aead_aes_ctr_hmac_sha256_sealv(const EVP_AEAD_CTX *ctx,
                                          Span<const CRYPTO_IOVEC> iovecs,
                                          Span<uint8_t> out_tag,
                                          size_t *out_tag_len,
                                          Span<const uint8_t> nonce,
                                          Span<const CRYPTO_IVEC> aadvecs) {
  const struct aead_aes_ctr_hmac_sha256_ctx *aes_ctx =
      (struct aead_aes_ctr_hmac_sha256_ctx *)&ctx->state;
  const uint64_t in_len_64 = bssl::iovec::TotalLength(iovecs);

  if (in_len_64 >= (UINT64_C(1) << 32) * AES_BLOCK_SIZE) {
    // This input is so large it would overflow the 32-bit block counter.
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    return 0;
  }

  if (out_tag.size() < ctx->tag_len) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BUFFER_TOO_SMALL);
    return 0;
  }

  if (nonce.size() != EVP_AEAD_AES_CTR_HMAC_SHA256_NONCE_LEN) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_NONCE_SIZE);
    return 0;
  }

  aead_aes_ctr_hmac_sha256_crypt(aes_ctx, iovecs, nonce.data());

  uint8_t hmac_result[SHA256_DIGEST_LENGTH];
  hmac_calculate(hmac_result, &aes_ctx->inner_init_state,
                 &aes_ctx->outer_init_state, aadvecs, nonce.data(), iovecs,
                 /*encrypt=*/true);
  CopyToPrefix(Span(hmac_result).first(ctx->tag_len), out_tag);
  *out_tag_len = ctx->tag_len;

  return 1;
}

static int aead_aes_ctr_hmac_sha256_openv_detached(
    const EVP_AEAD_CTX *ctx, Span<const CRYPTO_IOVEC> iovecs,
    Span<const uint8_t> nonce, Span<const uint8_t> in_tag,
    Span<const CRYPTO_IVEC> aadvecs) {
  const struct aead_aes_ctr_hmac_sha256_ctx *aes_ctx =
      (struct aead_aes_ctr_hmac_sha256_ctx *)&ctx->state;
  const uint64_t in_len_64 = bssl::iovec::TotalLength(iovecs);

  if (in_len_64 >= (UINT64_C(1) << 32) * AES_BLOCK_SIZE) {
    // This input is so large it would overflow the 32-bit block counter.
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
    return 0;
  }

  if (in_tag.size() != ctx->tag_len) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
    return 0;
  }

  if (nonce.size() != EVP_AEAD_AES_CTR_HMAC_SHA256_NONCE_LEN) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_NONCE_SIZE);
    return 0;
  }

  uint8_t hmac_result[SHA256_DIGEST_LENGTH];
  hmac_calculate(hmac_result, &aes_ctx->inner_init_state,
                 &aes_ctx->outer_init_state, aadvecs, nonce.data(), iovecs,
                 /*encrypt=*/false);
  if (CRYPTO_memcmp(hmac_result, in_tag.data(), ctx->tag_len) != 0) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
    return 0;
  }

  aead_aes_ctr_hmac_sha256_crypt(aes_ctx, iovecs, nonce.data());

  return 1;
}

static const EVP_AEAD aead_aes_128_ctr_hmac_sha256 = {
    16 /* AES key */ + 32 /* HMAC key */,
    12,                                    // nonce length
    EVP_AEAD_AES_CTR_HMAC_SHA256_TAG_LEN,  // overhead
    EVP_AEAD_AES_CTR_HMAC_SHA256_TAG_LEN,  // max tag length

    aead_aes_ctr_hmac_sha256_init,
    nullptr /* init_with_direction */,
    aead_aes_ctr_hmac_sha256_cleanup,
    nullptr /* openv */,
    aead_aes_ctr_hmac_sha256_sealv,
    aead_aes_ctr_hmac_sha256_openv_detached,
    nullptr /* get_iv */,
    nullptr /* tag_len */,
};

static const EVP_AEAD aead_aes_256_ctr_hmac_sha256 = {
    32 /* AES key */ + 32 /* HMAC key */,
    12,                                    // nonce length
    EVP_AEAD_AES_CTR_HMAC_SHA256_TAG_LEN,  // overhead
    EVP_AEAD_AES_CTR_HMAC_SHA256_TAG_LEN,  // max tag length

    aead_aes_ctr_hmac_sha256_init,
    nullptr /* init_with_direction */,
    aead_aes_ctr_hmac_sha256_cleanup,
    nullptr /* openv */,
    aead_aes_ctr_hmac_sha256_sealv,
    aead_aes_ctr_hmac_sha256_openv_detached,
    nullptr /* get_iv */,
    nullptr /* tag_len */,
};

const EVP_AEAD *EVP_aead_aes_128_ctr_hmac_sha256() {
  return &aead_aes_128_ctr_hmac_sha256;
}

const EVP_AEAD *EVP_aead_aes_256_ctr_hmac_sha256() {
  return &aead_aes_256_ctr_hmac_sha256;
}
