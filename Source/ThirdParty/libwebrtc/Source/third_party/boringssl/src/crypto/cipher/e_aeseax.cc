// Copyright 2025 The BoringSSL Authors
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
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <openssl/aes.h>
#include <openssl/base.h>
#include <openssl/cipher.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/span.h>

#include "../fipsmodule/cipher/internal.h"
#include "../internal.h"


using namespace bssl;

// Implementation of AES-EAX defined in
// https://www.iacr.org/archive/fse2004/30170391/30170391.pdf.

#define EVP_AEAD_AES_EAX_TAG_LEN AES_BLOCK_SIZE

struct aead_aes_eax_ctx {
  union {
    double align;
    AES_KEY ks;
  } ks;
  uint8_t b[AES_BLOCK_SIZE];
  uint8_t p[AES_BLOCK_SIZE];
};

static void mult_by_X(uint8_t out[AES_BLOCK_SIZE],
                      const uint8_t in[AES_BLOCK_SIZE]) {
  const crypto_word_t in_hi = CRYPTO_load_word_be(in);
  for (size_t i = 0; i < AES_BLOCK_SIZE - 1; ++i) {
    out[i] = (in[i] << 1) | (in[i + 1] >> 7);
  }
  // Carry over 0x87 if msb is 1, 0x00 if msb is 0.
  out[AES_BLOCK_SIZE - 1] = in[AES_BLOCK_SIZE - 1] << 1;
  const uint8_t p = 0x87;
  constant_time_conditional_memxor(out + AES_BLOCK_SIZE - 1, &p, /*n=*/1,
                                   constant_time_msb_w(in_hi));
}

static int aead_aes_eax_init(EVP_AEAD_CTX *ctx, const uint8_t *key,
                             size_t key_len, size_t tag_len) {
  struct aead_aes_eax_ctx *aes_ctx = (struct aead_aes_eax_ctx *)&ctx->state;

  if (key_len != 16 && key_len != 32) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_KEY_LENGTH);
    return 0;
  }

  if (tag_len == EVP_AEAD_DEFAULT_TAG_LENGTH) {
    tag_len = EVP_AEAD_AES_EAX_TAG_LEN;
  }

  if (tag_len != EVP_AEAD_AES_EAX_TAG_LEN) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_TAG_SIZE);
    return 0;
  }

  if (AES_set_encrypt_key(key, /*bits=*/key_len * 8, &aes_ctx->ks.ks) != 0) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_AES_KEY_SETUP_FAILED);
    return 0;
  }
  ctx->tag_len = tag_len;

  // L <- Ek(0^n).
  OPENSSL_memset(aes_ctx->b, 0, sizeof(aes_ctx->b));
  AES_encrypt(aes_ctx->b, aes_ctx->b, &aes_ctx->ks.ks);
  // B <- 2L.
  mult_by_X(aes_ctx->b, aes_ctx->b);
  // P <- 4L = 2B.
  mult_by_X(aes_ctx->p, aes_ctx->b);
  return 1;
}

static void aead_aes_eax_cleanup(EVP_AEAD_CTX *ctx) {}

// Implements the CBK function in the paper.
static void cbk_block(const struct aead_aes_eax_ctx *aes_ctx,
                      const uint8_t in[AES_BLOCK_SIZE],
                      uint8_t out[AES_BLOCK_SIZE]) {
  CRYPTO_xor16(out, in, out);
  AES_encrypt(out, out, &aes_ctx->ks.ks);
}

// Precondition: in_len <= AES_BLOCK_SIZE.
static void pad(const struct aead_aes_eax_ctx *aes_ctx,
                uint8_t out[AES_BLOCK_SIZE], const uint8_t *in, size_t in_len) {
  assert(in_len <= AES_BLOCK_SIZE);
  if (in_len == AES_BLOCK_SIZE) {
    CRYPTO_xor16(out, aes_ctx->b, in);
    return;
  }
  OPENSSL_memset(out, 0, AES_BLOCK_SIZE);
  OPENSSL_memcpy(out, in, in_len);
  out[in_len] = 0x80;
  CRYPTO_xor16(out, aes_ctx->p, out);
}

template <typename IVEC>
static void omac_with_tag(const struct aead_aes_eax_ctx *aes_ctx,
                          uint8_t out[AES_BLOCK_SIZE], Span<const IVEC> ivecs,
                          uint8_t tag) {
  OPENSSL_memset(out, 0, AES_BLOCK_SIZE);
  out[AES_BLOCK_SIZE - 1] = tag;
  size_t in_len = bssl::iovec::TotalLength(ivecs);
  if (in_len == 0) {
    // CBK(pad(M;B,P)) = CBK(B). Avoiding padding to skip a copy.
    cbk_block(aes_ctx, aes_ctx->b, out);
    return;
  }
  // CBK(M1) = Ek(M1 ^ 0^n)
  AES_encrypt(out, out, &aes_ctx->ks.ks);
  bssl::iovec::ForEachBlockRange<AES_BLOCK_SIZE>(
      ivecs,
      [&](const uint8_t *in, size_t len) {
        while (len >= AES_BLOCK_SIZE) {
          // Full blocks, no padding needed.
          cbk_block(aes_ctx, in, out);
          in += AES_BLOCK_SIZE;
          len -= AES_BLOCK_SIZE;
        }
        BSSL_CHECK(len == 0);
        return true;
      },
      [&](const uint8_t *in, size_t len) {
        // Remaining blocks.
        while (len > AES_BLOCK_SIZE) {
          // Full blocks, no padding needed.
          cbk_block(aes_ctx, in, out);
          in += AES_BLOCK_SIZE;
          len -= AES_BLOCK_SIZE;
        }
        // Last partial block.
        uint8_t padded_block[AES_BLOCK_SIZE];
        pad(aes_ctx, padded_block, in, len);
        cbk_block(aes_ctx, padded_block, out);
        return true;
      });
}

static void omac_with_tag_iovec_out(const struct aead_aes_eax_ctx *aes_ctx,
                                    uint8_t out[AES_BLOCK_SIZE],
                                    Span<const CRYPTO_IOVEC> iovecs,
                                    uint8_t tag) {
  OPENSSL_memset(out, 0, AES_BLOCK_SIZE);
  out[AES_BLOCK_SIZE - 1] = tag;
  size_t in_len = bssl::iovec::TotalLength(iovecs);
  if (in_len == 0) {
    // CBK(pad(M;B,P)) = CBK(B). Avoiding padding to skip a copy.
    cbk_block(aes_ctx, aes_ctx->b, out);
    return;
  }
  // CBK(M1) = Ek(M1 ^ 0^n)
  AES_encrypt(out, out, &aes_ctx->ks.ks);
  bssl::iovec::ForEachOutBlockRange<AES_BLOCK_SIZE>(
      iovecs,
      [&](const uint8_t *in, size_t len) {
        while (len >= AES_BLOCK_SIZE) {
          // Full blocks, no padding needed.
          cbk_block(aes_ctx, in, out);
          in += AES_BLOCK_SIZE;
          len -= AES_BLOCK_SIZE;
        }
        BSSL_CHECK(len == 0);
        return true;
      },
      [&](const uint8_t *in, size_t len) {
        while (len > AES_BLOCK_SIZE) {
          // Full blocks, no padding needed.
          cbk_block(aes_ctx, in, out);
          in += AES_BLOCK_SIZE;
          len -= AES_BLOCK_SIZE;
        }
        // Last partial block.
        uint8_t padded_block[AES_BLOCK_SIZE];
        pad(aes_ctx, padded_block, in, len);
        cbk_block(aes_ctx, padded_block, out);
        return true;
      });
}

// Encrypts/decrypts |in_len| bytes from |in| to |out| using AES-CTR with |n| as
// the IV.
static void aes_ctr(const struct aead_aes_eax_ctx *aes_ctx,
                    Span<const CRYPTO_IOVEC> iovecs,
                    const uint8_t n[AES_BLOCK_SIZE]) {
  uint8_t ivec[AES_BLOCK_SIZE];
  OPENSSL_memcpy(ivec, n, AES_BLOCK_SIZE);

  uint8_t ecount_buf[AES_BLOCK_SIZE];
  unsigned int num = 0;

  for (const CRYPTO_IOVEC &iovec : iovecs) {
    AES_ctr128_encrypt(iovec.in, iovec.out, iovec.len, &aes_ctx->ks.ks, ivec,
                       ecount_buf, &num);
  }
}

static int aead_aes_eax_sealv(const EVP_AEAD_CTX *ctx,
                              Span<const CRYPTO_IOVEC> iovecs,
                              Span<uint8_t> out_tag, size_t *out_tag_len,
                              Span<const uint8_t> nonce,
                              Span<const CRYPTO_IVEC> aadvecs) {
  // We use the full 128 bits of the nonce as counter, so no need to check the
  // plaintext size.

  if (out_tag.size() < ctx->tag_len) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BUFFER_TOO_SMALL);
    return 0;
  }

  if (nonce.size() != 12 && nonce.size() != 16) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_NONCE_SIZE);
    return 0;
  }

  const struct aead_aes_eax_ctx *aes_ctx =
      (struct aead_aes_eax_ctx *)&ctx->state;

  // N <- OMAC(0 || nonce)
  uint8_t n[AES_BLOCK_SIZE];
  CRYPTO_IVEC noncevec[1];
  noncevec[0].in = nonce.data();
  noncevec[0].len = nonce.size();
  omac_with_tag(aes_ctx, n, Span<const CRYPTO_IVEC>(noncevec), /*tag=*/0);
  // H <- OMAC(1 || ad)
  uint8_t h[AES_BLOCK_SIZE];
  omac_with_tag(aes_ctx, h, aadvecs, /*tag=*/1);

  // C <- CTR^{N}_{K}(M)
  aes_ctr(aes_ctx, iovecs, n);

  // MAC <- OMAC(2 || C)
  omac_with_tag_iovec_out(aes_ctx, out_tag.data(), iovecs, /*tag=*/2);
  // MAC <- N ^ C ^ H
  CRYPTO_xor16(out_tag.data(), n, out_tag.data());
  CRYPTO_xor16(out_tag.data(), h, out_tag.data());

  *out_tag_len = ctx->tag_len;
  return 1;
}

static int aead_aes_eax_openv_detached(const EVP_AEAD_CTX *ctx,
                                       Span<const CRYPTO_IOVEC> iovecs,
                                       Span<const uint8_t> nonce,
                                       Span<const uint8_t> in_tag,
                                       Span<const CRYPTO_IVEC> aadvecs) {
  const uint64_t ad_len_64 = bssl::iovec::TotalLength(aadvecs);
  if (ad_len_64 >= (UINT64_C(1) << 61)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    return 0;
  }

  const uint64_t in_len_64 = bssl::iovec::TotalLength(iovecs);
  if (in_tag.size() != EVP_AEAD_AES_EAX_TAG_LEN ||
      in_len_64 > (UINT64_C(1) << 36) + AES_BLOCK_SIZE) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
    return 0;
  }

  if (nonce.size() != 12 && nonce.size() != 16) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_NONCE_SIZE);
    return 0;
  }

  const struct aead_aes_eax_ctx *aes_ctx =
      (struct aead_aes_eax_ctx *)&ctx->state;

  // N <- OMAC(0 || nonce)
  uint8_t n[AES_BLOCK_SIZE];
  CRYPTO_IVEC noncevec[1];
  noncevec[0].in = nonce.data();
  noncevec[0].len = nonce.size();
  omac_with_tag(aes_ctx, n, Span<const CRYPTO_IVEC>(noncevec),
                /*tag=*/0);
  // H <- OMAC(1 || ad)
  uint8_t h[AES_BLOCK_SIZE];
  omac_with_tag(aes_ctx, h, aadvecs, /*tag=*/1);

  // MAC <- OMAC(2 || C)
  uint8_t mac[AES_BLOCK_SIZE];
  omac_with_tag(aes_ctx, mac, iovecs, /*tag=*/2);
  // MAC <- N ^ C ^ H
  CRYPTO_xor16(mac, n, mac);
  CRYPTO_xor16(mac, h, mac);

  if (CRYPTO_memcmp(mac, in_tag.data(), in_tag.size()) != 0) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
    return 0;
  }

  // M <- CTR^{N}_{K}(C)
  aes_ctr(aes_ctx, iovecs, n);
  return 1;
}

static const EVP_AEAD aead_aes_128_eax = {
    16,                        // AES key size
    16,                        // nonce length
    EVP_AEAD_AES_EAX_TAG_LEN,  // overhead
    EVP_AEAD_AES_EAX_TAG_LEN,  // max tag length

    aead_aes_eax_init,
    nullptr,  // init_with_direction
    aead_aes_eax_cleanup,
    nullptr,  // openv
    aead_aes_eax_sealv,
    aead_aes_eax_openv_detached,
    nullptr,  // get_iv
    nullptr,  // tag_len
};

static const EVP_AEAD aead_aes_256_eax = {
    32,                        // AES key size
    16,                        // nonce length
    EVP_AEAD_AES_EAX_TAG_LEN,  // overhead
    EVP_AEAD_AES_EAX_TAG_LEN,  // max tag length

    aead_aes_eax_init,
    nullptr,  // init_with_direction
    aead_aes_eax_cleanup,
    nullptr,  // openv
    aead_aes_eax_sealv,
    aead_aes_eax_openv_detached,
    nullptr,  // get_iv
    nullptr,  // tag_len
};

const EVP_AEAD *EVP_aead_aes_128_eax() { return &aead_aes_128_eax; }

const EVP_AEAD *EVP_aead_aes_256_eax() { return &aead_aes_256_eax; }
