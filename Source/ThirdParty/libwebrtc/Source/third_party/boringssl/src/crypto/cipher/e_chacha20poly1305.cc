// Copyright 2014 The BoringSSL Authors
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
#include <string.h>

#include <openssl/chacha.h>
#include <openssl/cipher.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/poly1305.h>
#include <openssl/span.h>

#include "../chacha/internal.h"
#include "../fipsmodule/cipher/internal.h"
#include "../internal.h"
#include "internal.h"

using namespace bssl;

struct aead_chacha20_poly1305_ctx {
  uint8_t key[32];
};

static_assert(sizeof(((EVP_AEAD_CTX *)nullptr)->state) >=
                  sizeof(struct aead_chacha20_poly1305_ctx),
              "AEAD state is too small");
static_assert(alignof(union evp_aead_ctx_st_state) >=
                  alignof(struct aead_chacha20_poly1305_ctx),
              "AEAD state has insufficient alignment");

static int aead_chacha20_poly1305_init(EVP_AEAD_CTX *ctx, const uint8_t *key,
                                       size_t key_len, size_t tag_len) {
  struct aead_chacha20_poly1305_ctx *c20_ctx =
      (struct aead_chacha20_poly1305_ctx *)&ctx->state;

  if (tag_len == 0) {
    tag_len = POLY1305_TAG_LEN;
  }

  if (tag_len > POLY1305_TAG_LEN) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    return 0;
  }

  if (key_len != sizeof(c20_ctx->key)) {
    return 0;  // internal error - EVP_AEAD_CTX_init should catch this.
  }

  OPENSSL_memcpy(c20_ctx->key, key, key_len);
  ctx->tag_len = tag_len;

  return 1;
}

static void aead_chacha20_poly1305_cleanup(EVP_AEAD_CTX *ctx) {}

static void poly1305_update_length(poly1305_state *poly1305, size_t data_len) {
  uint8_t length_bytes[8];

  for (unsigned i = 0; i < sizeof(length_bytes); i++) {
    length_bytes[i] = data_len;
    data_len >>= 8;
  }

  CRYPTO_poly1305_update(poly1305, length_bytes, sizeof(length_bytes));
}

// calc_tag_pre prepares filling |tag| with the authentication tag for the given
// inputs.
static size_t calc_tag_pre(poly1305_state *ctx, const uint8_t key[32],
                           const uint8_t nonce[12],
                           Span<const CRYPTO_IVEC> aadvecs) {
  alignas(16) uint8_t poly1305_key[32];
  OPENSSL_memset(poly1305_key, 0, sizeof(poly1305_key));
  CRYPTO_chacha_20(poly1305_key, poly1305_key, sizeof(poly1305_key), key, nonce,
                   0);

  static const uint8_t padding[16] = {0};  // Padding is all zeros.
  CRYPTO_poly1305_init(ctx, poly1305_key);
  size_t ad_len = 0;
  for (const CRYPTO_IVEC &aadvec : aadvecs) {
    CRYPTO_poly1305_update(ctx, aadvec.in, aadvec.len);
    ad_len += aadvec.len;
  }
  if (ad_len % 16 != 0) {
    CRYPTO_poly1305_update(ctx, padding, sizeof(padding) - (ad_len % 16));
  }
  return ad_len;
}

static void calc_tag_post(poly1305_state *ctx, uint8_t tag[POLY1305_TAG_LEN],
                          size_t ciphertext_total, size_t ad_len) {
  static const uint8_t padding[16] = {0};  // Padding is all zeros.
  if (ciphertext_total % 16 != 0) {
    CRYPTO_poly1305_update(ctx, padding,
                           sizeof(padding) - (ciphertext_total % 16));
  }
  poly1305_update_length(ctx, ad_len);
  poly1305_update_length(ctx, ciphertext_total);
  CRYPTO_poly1305_finish(ctx, tag);
}

static int chacha20_poly1305_sealv(const uint8_t *key,
                                   Span<const CRYPTO_IOVEC> iovecs,
                                   Span<uint8_t> out_tag, size_t *out_tag_len,
                                   Span<const uint8_t> nonce,
                                   Span<const CRYPTO_IVEC> aadvecs,
                                   size_t tag_len) {
  if (out_tag.size() < tag_len) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BUFFER_TOO_SMALL);
    return 0;
  }
  if (nonce.size() != 12) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_NONCE_SIZE);
    return 0;
  }

  // |CRYPTO_chacha_20| uses a 32-bit block counter. Therefore we disallow
  // individual operations that work on more than 256GB at a time.
  // |in_len_64| is needed because, on 32-bit platforms, size_t is only
  // 32-bits and this produces a warning because it's always false.
  // Casting to uint64_t inside the conditional is not sufficient to stop
  // the warning.
  const uint64_t in_len_64 = bssl::iovec::TotalLength(iovecs);
  if (in_len_64 >= (UINT64_C(1) << 32) * 64 - 64) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    return 0;
  }

  union chacha20_poly1305_seal_data data;
  if (chacha20_poly1305_asm_capable() && iovecs.size() <= 2 &&
      aadvecs.size() <= 1) {
    OPENSSL_memcpy(data.in.key, key, 32);
    data.in.counter = 0;
    CopySpan(nonce, data.in.nonce);
    if (iovecs.size() >= 2) {
      // |chacha20_poly1305_seal| only supports one extra input and expects it
      // to have been encrypted ahead of time. (Historically it was only used
      // for very short inputs.)
      constexpr size_t kChaChaBlockSize = 64;
      uint32_t block_counter =
          (uint32_t)(1 + (iovecs[0].len / kChaChaBlockSize));
      size_t offset = iovecs[0].len % kChaChaBlockSize;
      size_t done = 0;
      if (offset != 0) {
        uint8_t block[kChaChaBlockSize];
        memset(block, 0, sizeof(block));
        CRYPTO_chacha_20(block, block, sizeof(block), key, nonce.data(),
                         block_counter);
        for (size_t i = offset; i < sizeof(block) && done < iovecs[1].len;
             i++, done++) {
          iovecs[1].out[done] = iovecs[1].in[done] ^ block[i];
        }
        ++block_counter;
      }
      if (done < iovecs[1].len) {
        CRYPTO_chacha_20(iovecs[1].out + done, iovecs[1].in + done,
                         iovecs[1].len - done, key, nonce.data(),
                         block_counter);
      }
      // TODO(crbug.com/473454967): Support more than 1 extra ciphertext.
      data.in.extra_ciphertext = iovecs[1].out;
      data.in.extra_ciphertext_len = iovecs[1].len;
    } else {
      data.in.extra_ciphertext = nullptr;
      data.in.extra_ciphertext_len = 0;
    }
    chacha20_poly1305_seal(iovecs.size() >= 1 ? iovecs[0].out : nullptr,
                           iovecs.size() >= 1 ? iovecs[0].in : nullptr,
                           iovecs.size() >= 1 ? iovecs[0].len : 0,
                           aadvecs.size() >= 1 ? aadvecs[0].in : nullptr,
                           aadvecs.size() >= 1 ? aadvecs[0].len : 0, &data);
  } else {
    poly1305_state ctx;
    size_t ad_len = calc_tag_pre(&ctx, key, nonce.data(), aadvecs);

    size_t ciphertext_total = 0;
    size_t block = 1;
    bssl::iovec::ForEachBlockRange<64, /*WriteOut=*/true>(
        iovecs,
        [&](const uint8_t *in, uint8_t *out, size_t len) {
          // TODO(crbug.com/473454967): Maybe just provide asm version of this?
          // Here, len is always a multiple of 64.
          CRYPTO_chacha_20(out, in, len, key, nonce.data(), block);
          CRYPTO_poly1305_update(&ctx, out, len);
          ciphertext_total += len;
          block += len / 64;
          return true;
        },
        [&](const uint8_t *in, uint8_t *out, size_t len) {
          // Here, len may be anything. If an asm version can't handle that,
          // it will be worth splitting off multiples of 64 here.
          CRYPTO_chacha_20(out, in, len, key, nonce.data(), block);
          CRYPTO_poly1305_update(&ctx, out, len);
          ciphertext_total += len;
          return true;
        });

    calc_tag_post(&ctx, data.out.tag, ciphertext_total, ad_len);
  }

  CopyToPrefix(Span(data.out.tag).first(tag_len), out_tag);
  *out_tag_len = tag_len;
  return 1;
}

static int aead_chacha20_poly1305_sealv(const EVP_AEAD_CTX *ctx,
                                        Span<const CRYPTO_IOVEC> iovecs,
                                        Span<uint8_t> out_tag,
                                        size_t *out_tag_len,
                                        Span<const uint8_t> nonce,
                                        Span<const CRYPTO_IVEC> aadvecs) {
  const struct aead_chacha20_poly1305_ctx *c20_ctx =
      (struct aead_chacha20_poly1305_ctx *)&ctx->state;

  return chacha20_poly1305_sealv(c20_ctx->key, iovecs, out_tag, out_tag_len,
                                 nonce, aadvecs, ctx->tag_len);
}

static int aead_xchacha20_poly1305_sealv(const EVP_AEAD_CTX *ctx,
                                         Span<const CRYPTO_IOVEC> iovecs,
                                         Span<uint8_t> out_tag,
                                         size_t *out_tag_len,
                                         Span<const uint8_t> nonce,
                                         Span<const CRYPTO_IVEC> aadvecs) {
  const struct aead_chacha20_poly1305_ctx *c20_ctx =
      (struct aead_chacha20_poly1305_ctx *)&ctx->state;

  if (nonce.size() != 24) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_NONCE_SIZE);
    return 0;
  }

  alignas(4) uint8_t derived_key[32];
  alignas(4) uint8_t derived_nonce[12];
  CRYPTO_hchacha20(derived_key, c20_ctx->key, nonce.data());
  OPENSSL_memset(derived_nonce, 0, 4);
  OPENSSL_memcpy(&derived_nonce[4], &nonce[16], 8);

  return chacha20_poly1305_sealv(derived_key, iovecs, out_tag, out_tag_len,
                                 derived_nonce, aadvecs, ctx->tag_len);
}

static int chacha20_poly1305_openv_detached(const uint8_t *key,
                                            Span<const CRYPTO_IOVEC> iovecs,
                                            Span<const uint8_t> nonce,
                                            Span<const uint8_t> in_tag,
                                            Span<const CRYPTO_IVEC> aadvecs,
                                            size_t tag_len) {
  if (nonce.size() != 12) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_NONCE_SIZE);
    return 0;
  }

  if (in_tag.size() != tag_len) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
    return 0;
  }

  // |CRYPTO_chacha_20| uses a 32-bit block counter. Therefore we disallow
  // individual operations that work on more than 256GB at a time.
  // |in_len_64| is needed because, on 32-bit platforms, size_t is only
  // 32-bits and this produces a warning because it's always false.
  // Casting to uint64_t inside the conditional is not sufficient to stop
  // the warning.
  const uint64_t in_len_64 = bssl::iovec::TotalLength(iovecs);
  if (in_len_64 >= (UINT64_C(1) << 32) * 64 - 64) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    return 0;
  }

  union chacha20_poly1305_open_data data;
  if (chacha20_poly1305_asm_capable() && iovecs.size() <= 1 &&
      aadvecs.size() <= 1) {
    // TODO(crbug.com/473454967): Support more than 1 ciphertext segment.
    OPENSSL_memcpy(data.in.key, key, 32);
    data.in.counter = 0;
    CopySpan(nonce, data.in.nonce);
    chacha20_poly1305_open(iovecs.size() >= 1 ? iovecs[0].out : nullptr,
                           iovecs.size() >= 1 ? iovecs[0].in : nullptr,
                           iovecs.size() >= 1 ? iovecs[0].len : 0,
                           aadvecs.size() >= 1 ? aadvecs[0].in : nullptr,
                           aadvecs.size() >= 1 ? aadvecs[0].len : 0, &data);
  } else {
    poly1305_state ctx;
    size_t ad_len = calc_tag_pre(&ctx, key, nonce.data(), aadvecs);

    size_t ciphertext_total = 0;
    size_t block = 1;
    bssl::iovec::ForEachBlockRange<64, /*WriteOut=*/true>(
        iovecs,
        [&](const uint8_t *in, uint8_t *out, size_t len) {
          // TODO(crbug.com/473454967): Maybe just provide asm version of this?
          // Here, len is always a multiple of 64.
          CRYPTO_poly1305_update(&ctx, in, len);
          CRYPTO_chacha_20(out, in, len, key, nonce.data(), block);
          ciphertext_total += len;
          block += len / 64;
          return true;
        },
        [&](const uint8_t *in, uint8_t *out, size_t len) {
          // Here, len may be anything. If an asm version can't handle that,
          // it will be worth splitting off multiples of 64 here.
          CRYPTO_poly1305_update(&ctx, in, len);
          CRYPTO_chacha_20(out, in, len, key, nonce.data(), block);
          ciphertext_total += len;
          return true;
        });

    calc_tag_post(&ctx, data.out.tag, ciphertext_total, ad_len);
  }

  if (CRYPTO_memcmp(data.out.tag, in_tag.data(), tag_len) != 0) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
    return 0;
  }

  return 1;
}

static int aead_chacha20_poly1305_openv_detached(
    const EVP_AEAD_CTX *ctx, Span<const CRYPTO_IOVEC> iovecs,
    Span<const uint8_t> nonce, Span<const uint8_t> in_tag,
    Span<const CRYPTO_IVEC> aadvecs) {
  const struct aead_chacha20_poly1305_ctx *c20_ctx =
      (struct aead_chacha20_poly1305_ctx *)&ctx->state;

  return chacha20_poly1305_openv_detached(c20_ctx->key, iovecs, nonce, in_tag,
                                          aadvecs, ctx->tag_len);
}

static int aead_xchacha20_poly1305_openv_detached(
    const EVP_AEAD_CTX *ctx, Span<const CRYPTO_IOVEC> iovecs,
    Span<const uint8_t> nonce, Span<const uint8_t> in_tag,
    Span<const CRYPTO_IVEC> aadvecs) {
  const struct aead_chacha20_poly1305_ctx *c20_ctx =
      (struct aead_chacha20_poly1305_ctx *)&ctx->state;

  if (nonce.size() != 24) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_NONCE_SIZE);
    return 0;
  }

  alignas(4) uint8_t derived_key[32];
  alignas(4) uint8_t derived_nonce[12];
  CRYPTO_hchacha20(derived_key, c20_ctx->key, nonce.data());
  OPENSSL_memset(derived_nonce, 0, 4);
  OPENSSL_memcpy(&derived_nonce[4], &nonce[16], 8);

  return chacha20_poly1305_openv_detached(derived_key, iovecs, derived_nonce,
                                          in_tag, aadvecs, ctx->tag_len);
}

static const EVP_AEAD aead_chacha20_poly1305 = {
    32,                // key len
    12,                // nonce len
    POLY1305_TAG_LEN,  // overhead
    POLY1305_TAG_LEN,  // max tag length

    aead_chacha20_poly1305_init,
    nullptr,  // init_with_direction
    aead_chacha20_poly1305_cleanup,
    nullptr,  // openv
    aead_chacha20_poly1305_sealv,
    aead_chacha20_poly1305_openv_detached,
    nullptr,  // get_iv
    nullptr,  // tag_len
};

static const EVP_AEAD aead_xchacha20_poly1305 = {
    32,                // key len
    24,                // nonce len
    POLY1305_TAG_LEN,  // overhead
    POLY1305_TAG_LEN,  // max tag length

    aead_chacha20_poly1305_init,
    nullptr,  // init_with_direction
    aead_chacha20_poly1305_cleanup,
    nullptr,  // openv
    aead_xchacha20_poly1305_sealv,
    aead_xchacha20_poly1305_openv_detached,
    nullptr,  // get_iv
    nullptr,  // tag_len
};

const EVP_AEAD *EVP_aead_chacha20_poly1305() { return &aead_chacha20_poly1305; }

const EVP_AEAD *EVP_aead_xchacha20_poly1305() {
  return &aead_xchacha20_poly1305;
}
