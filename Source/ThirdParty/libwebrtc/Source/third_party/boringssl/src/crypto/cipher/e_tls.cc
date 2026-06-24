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

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <openssl/aead.h>
#include <openssl/cipher.h>
#include <openssl/err.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <openssl/mem.h>
#include <openssl/sha.h>
#include <openssl/span.h>

#include "../fipsmodule/cipher/internal.h"
#include "../internal.h"
#include "../mem_internal.h"
#include "internal.h"


using namespace bssl;

typedef struct {
  EVP_CIPHER_CTX cipher_ctx;
  HMAC_CTX *hmac_ctx;
  // mac_key is the portion of the key used for the MAC. It is retained
  // separately for the constant-time CBC code.
  uint8_t mac_key[EVP_MAX_MD_SIZE];
  uint8_t mac_key_len;
  // implicit_iv is one iff this is a pre-TLS-1.1 CBC cipher without an explicit
  // IV.
  char implicit_iv;
} AEAD_TLS_CTX;

static_assert(EVP_MAX_MD_SIZE < 256, "mac_key_len does not fit in uint8_t");

static_assert(sizeof(((EVP_AEAD_CTX *)nullptr)->state) >= sizeof(AEAD_TLS_CTX),
              "AEAD state is too small");
static_assert(alignof(union evp_aead_ctx_st_state) >= alignof(AEAD_TLS_CTX),
              "AEAD state has insufficient alignment");

static void aead_tls_cleanup(EVP_AEAD_CTX *ctx) {
  AEAD_TLS_CTX *tls_ctx = (AEAD_TLS_CTX *)&ctx->state;
  EVP_CIPHER_CTX_cleanup(&tls_ctx->cipher_ctx);
  HMAC_CTX_free(tls_ctx->hmac_ctx);
}

static int aead_tls_init(EVP_AEAD_CTX *ctx, const uint8_t *key, size_t key_len,
                         size_t tag_len, enum evp_aead_direction_t dir,
                         const EVP_CIPHER *cipher, const EVP_MD *md,
                         char implicit_iv) {
  if (tag_len != EVP_AEAD_DEFAULT_TAG_LENGTH && tag_len != EVP_MD_size(md)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_TAG_SIZE);
    return 0;
  }

  if (key_len != EVP_AEAD_key_length(ctx->aead)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_KEY_LENGTH);
    return 0;
  }

  size_t mac_key_len = EVP_MD_size(md);
  size_t enc_key_len = EVP_CIPHER_key_length(cipher);
  assert(mac_key_len + enc_key_len +
             (implicit_iv ? EVP_CIPHER_iv_length(cipher) : 0) ==
         key_len);

  AEAD_TLS_CTX *tls_ctx = (AEAD_TLS_CTX *)&ctx->state;
  tls_ctx->hmac_ctx = HMAC_CTX_new();
  if (!tls_ctx->hmac_ctx) {
    return 0;
  }
  EVP_CIPHER_CTX_init(&tls_ctx->cipher_ctx);
  assert(mac_key_len <= EVP_MAX_MD_SIZE);
  OPENSSL_memcpy(tls_ctx->mac_key, key, mac_key_len);
  tls_ctx->mac_key_len = (uint8_t)mac_key_len;
  tls_ctx->implicit_iv = implicit_iv;

  if (!EVP_CipherInit_ex(
          &tls_ctx->cipher_ctx, cipher, nullptr, &key[mac_key_len],
          implicit_iv ? &key[mac_key_len + enc_key_len] : nullptr,
          dir == evp_aead_seal) ||
      !HMAC_Init_ex(tls_ctx->hmac_ctx, key, mac_key_len, md, nullptr)) {
    aead_tls_cleanup(ctx);
    return 0;
  }
  EVP_CIPHER_CTX_set_padding(&tls_ctx->cipher_ctx, 0);

  return 1;
}

static size_t aead_tls_tag_len(const EVP_AEAD_CTX *ctx, const size_t in_len) {
  const AEAD_TLS_CTX *tls_ctx = (AEAD_TLS_CTX *)&ctx->state;
  assert(EVP_CIPHER_CTX_mode(&tls_ctx->cipher_ctx) == EVP_CIPH_CBC_MODE);

  const size_t hmac_len = HMAC_size(tls_ctx->hmac_ctx);
  const size_t block_size = EVP_CIPHER_CTX_block_size(&tls_ctx->cipher_ctx);
  // An overflow of |in_len + hmac_len| doesn't affect the result mod
  // |block_size|, provided that |block_size| is a smaller power of two.
  assert(block_size == 8 /*3DES*/ || block_size == 16 /*AES*/);
  const size_t pad_len = block_size - ((in_len + hmac_len) & (block_size - 1));
  return hmac_len + pad_len;
}

static int aead_tls_sealv(const EVP_AEAD_CTX *ctx,
                          Span<const CRYPTO_IOVEC> iovecs,
                          Span<uint8_t> out_tag, size_t *out_tag_len,
                          Span<const uint8_t> nonce,
                          Span<const CRYPTO_IVEC> aadvecs) {
  AEAD_TLS_CTX *tls_ctx = (AEAD_TLS_CTX *)&ctx->state;

  if (!tls_ctx->cipher_ctx.encrypt) {
    // Unlike a normal AEAD, a TLS AEAD may only be used in one direction.
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_INVALID_OPERATION);
    return 0;
  }

  size_t in_len = bssl::iovec::TotalLength(iovecs);
  if (out_tag.size() < aead_tls_tag_len(ctx, in_len)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BUFFER_TOO_SMALL);
    return 0;
  }

  if (nonce.size() != EVP_AEAD_nonce_length(ctx->aead)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_INVALID_NONCE_SIZE);
    return 0;
  }

  size_t ad_len = bssl::iovec::TotalLength(aadvecs);
  if (ad_len != 13 - 2 /* length bytes */) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_INVALID_AD_SIZE);
    return 0;
  }

  // To allow for CBC mode which changes cipher length, |ad| doesn't include the
  // length for legacy ciphers.
  uint8_t ad_extra[2];
  CRYPTO_store_u16_be(ad_extra, static_cast<uint16_t>(in_len));

  // Compute the MAC. This must be first in case the operation is being done
  // in-place.
  uint8_t mac[EVP_MAX_MD_SIZE];
  if (!HMAC_Init_ex(tls_ctx->hmac_ctx, nullptr, 0, nullptr, nullptr)) {
    return 0;
  }
  for (const CRYPTO_IVEC &aadvec : aadvecs) {
    if (!HMAC_Update(tls_ctx->hmac_ctx, aadvec.in, aadvec.len)) {
      return 0;
    }
  }
  if (!HMAC_Update(tls_ctx->hmac_ctx, ad_extra, sizeof(ad_extra))) {
    return 0;
  }
  for (const CRYPTO_IOVEC &iovec : iovecs) {
    if (!HMAC_Update(tls_ctx->hmac_ctx, iovec.in, iovec.len)) {
      return 0;
    }
  }
  unsigned mac_len;
  if (!HMAC_Final(tls_ctx->hmac_ctx, mac, &mac_len)) {
    return 0;
  }

  // Configure the explicit IV.
  assert(EVP_CIPHER_CTX_mode(&tls_ctx->cipher_ctx) == EVP_CIPH_CBC_MODE);
  if (!tls_ctx->implicit_iv &&
      !EVP_EncryptInit_ex(&tls_ctx->cipher_ctx, nullptr, nullptr, nullptr,
                          nonce.data())) {
    return 0;
  }

  size_t block_size = EVP_CIPHER_CTX_block_size(&tls_ctx->cipher_ctx);
  assert(block_size == 8 /*3DES*/ || block_size == 16 /*AES*/);

  // Encrypt the input.
  size_t len = 0;
  size_t tag_len = 0;
  if (!bssl::iovec::ForEachBlockRange_Dynamic</*WriteOut=*/true>(
          block_size, iovecs,
          [&](const uint8_t *in, uint8_t *out, size_t chunk_len) {
            // Complete block(s).
            size_t out_len;
            if (!EVP_EncryptUpdate_ex(&tls_ctx->cipher_ctx, out, &out_len,
                                      chunk_len, in, chunk_len)) {
              return false;
            }
            assert(out_len == chunk_len);
            len += out_len;
            return true;
          },
          [&](const uint8_t *in, uint8_t *out, size_t chunk_len) {
            // Final chunk, possibly with a partial block.
            size_t out_len;
            if (!EVP_EncryptUpdate_ex(&tls_ctx->cipher_ctx, out, &out_len,
                                      chunk_len, in, chunk_len)) {
              return false;
            }
            len += out_len;
            size_t remaining = chunk_len - out_len;
            assert(remaining < block_size);
            if (remaining == 0) {
              return true;
            }

            // Feed the MAC into the cipher in two steps. First complete the
            // final partial block from encrypting the input and split the
            // result between |out| and |out_tag|. Then feed the rest.
            const size_t early_mac_len = block_size - remaining;
            assert(early_mac_len < block_size);
            assert(len + block_size - early_mac_len == in_len);
            uint8_t buf[EVP_MAX_BLOCK_LENGTH];
            size_t buf_len;
            if (!EVP_EncryptUpdate_ex(&tls_ctx->cipher_ctx, buf, &buf_len,
                                      sizeof(buf), mac, early_mac_len)) {
              return false;
            }
            assert(buf_len == block_size);
            OPENSSL_memcpy(out + out_len, buf, remaining);
            OPENSSL_memcpy(out_tag.data(), buf + remaining, early_mac_len);
            tag_len = early_mac_len;
            return true;
          })) {
    return 0;
  }

  if (!EVP_EncryptUpdate_ex(&tls_ctx->cipher_ctx, out_tag.data() + tag_len,
                            &len, out_tag.size() - tag_len, mac + tag_len,
                            mac_len - tag_len)) {
    return 0;
  }
  tag_len += len;

  // Compute padding and feed that into the cipher.
  uint8_t padding[256];
  unsigned padding_len = block_size - ((in_len + mac_len) & (block_size - 1));
  OPENSSL_memset(padding, padding_len - 1, padding_len);
  if (!EVP_EncryptUpdate_ex(&tls_ctx->cipher_ctx, out_tag.data() + tag_len,
                            &len, out_tag.size() - tag_len, padding,
                            padding_len)) {
    return 0;
  }
  tag_len += len;

  if (!EVP_EncryptFinal_ex2(&tls_ctx->cipher_ctx, out_tag.data() + tag_len,
                            &len, out_tag.size() - tag_len)) {
    return 0;
  }
  assert(len == 0);  // Padding is explicit.
  assert(tag_len == aead_tls_tag_len(ctx, in_len));

  *out_tag_len = tag_len;
  return 1;
}

static int aead_tls_openv(const EVP_AEAD_CTX *ctx,
                          Span<const CRYPTO_IOVEC> iovecs,
                          size_t *out_total_bytes, Span<const uint8_t> nonce,
                          Span<const CRYPTO_IVEC> aadvecs) {
  AEAD_TLS_CTX *tls_ctx = (AEAD_TLS_CTX *)&ctx->state;

  if (tls_ctx->cipher_ctx.encrypt) {
    // Unlike a normal AEAD, a TLS AEAD may only be used in one direction.
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_INVALID_OPERATION);
    return 0;
  }

  size_t in_len = bssl::iovec::TotalLength(iovecs);
  if (in_len < HMAC_size(tls_ctx->hmac_ctx)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
    return 0;
  }

  if (nonce.size() != EVP_AEAD_nonce_length(ctx->aead)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_INVALID_NONCE_SIZE);
    return 0;
  }

  size_t ad_len = bssl::iovec::TotalLength(aadvecs);
  if (ad_len != 13 - 2 /* length bytes */) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_INVALID_AD_SIZE);
    return 0;
  }

  // Configure the explicit IV.
  assert(EVP_CIPHER_CTX_mode(&tls_ctx->cipher_ctx) == EVP_CIPH_CBC_MODE);
  if (!tls_ctx->implicit_iv &&
      !EVP_DecryptInit_ex(&tls_ctx->cipher_ctx, nullptr, nullptr, nullptr,
                          nonce.data())) {
    return 0;
  }

  // Decrypt to get the plaintext + MAC + padding.
  size_t total = 0;
  size_t block_size = EVP_CIPHER_CTX_block_size(&tls_ctx->cipher_ctx);
  auto decrypt_update = [&](const uint8_t *in, uint8_t *out, size_t len) {
    size_t out_len;
    if (!EVP_DecryptUpdate_ex(&tls_ctx->cipher_ctx, out, &out_len, len, in,
                              len)) {
      return false;
    }
    CONSTTIME_SECRET(out, out_len);
    if (out_len != len) {
      // A byte sequence that was not a multiple of the block size was provided
      // as ciphertext. This is generally invalid and thus should be rejected.
      OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
      return false;
    }
    total += len;
    return true;
  };
  if (!bssl::iovec::ForEachBlockRange_Dynamic</*WriteOut=*/true>(
          block_size, iovecs, decrypt_update, decrypt_update)) {
    return false;
  }
  assert(total == in_len);

  const size_t mac_len = HMAC_size(tls_ctx->hmac_ctx);

  // Split the decrypted record into |iovecs_without_trailer| and |trailer|,
  // based on the public lower bound of where the plaintext ends. The plaintext
  // is followed by |mac_len| and then at most 256 bytes of padding.
  InplaceVector<CRYPTO_IOVEC, CRYPTO_IOVEC_MAX> iovecs_without_trailer;
  iovecs_without_trailer.CopyFrom(iovecs);
  uint8_t trailer_buf[EVP_MAX_MD_SIZE + 256];
  const size_t trailer_len = std::min(in_len, mac_len + 256);
  std::optional<Span<const uint8_t>> trailer = bssl::iovec::GetAndRemoveOutSuffix(
      Span(trailer_buf).first(trailer_len), Span(iovecs_without_trailer));
  BSSL_CHECK(trailer.has_value());

  // Remove CBC padding. Code from here on is timing-sensitive with respect to
  // |padding_ok|, |trailer_minus_padding|, and derived values.
  crypto_word_t padding_ok;
  size_t trailer_minus_padding;
  if (!EVP_tls_cbc_remove_padding(&padding_ok, &trailer_minus_padding,
                                  trailer->data(), trailer->size(), block_size,
                                  mac_len)) {
    // Publicly invalid. This can be rejected in non-constant time.
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
    return 0;
  }

  // If the padding is valid, |trailer->first(trailer_minus_padding)| is the
  // last bytes of plaintext and the MAC. Otherwise, it is still large enough to
  // extract a MAC, but it will be irrelevant. Note that |trailer_minus_padding|
  // is secret.
  declassify_assert(trailer_minus_padding >= mac_len);
  size_t data_in_trailer_len = trailer_minus_padding - mac_len;
  size_t max_data_in_trailer_len = trailer->size() - mac_len;
  size_t data_len = total - trailer->size() + data_in_trailer_len;

  // To allow for CBC mode which changes cipher length, |ad_len| doesn't
  // include the length for legacy ciphers.
  uint8_t ad_extra[2];
  CRYPTO_store_u16_be(ad_extra, static_cast<uint16_t>(data_len));

  // Compute the MAC and extract the one in the record.
  uint8_t mac[EVP_MAX_MD_SIZE];
  size_t got_mac_len;
  assert(EVP_tls_cbc_record_digest_supported(tls_ctx->hmac_ctx->md));
  if (!EVP_tls_cbc_digest_record(
          tls_ctx->hmac_ctx->md, mac, &got_mac_len, ad_extra, aadvecs,
          iovecs_without_trailer, trailer->first(max_data_in_trailer_len),
          data_in_trailer_len, tls_ctx->mac_key, tls_ctx->mac_key_len)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
    return 0;
  }
  assert(got_mac_len == mac_len);

  uint8_t record_mac[EVP_MAX_MD_SIZE];
  EVP_tls_cbc_copy_mac(record_mac, mac_len, trailer->data(),
                       trailer_minus_padding, trailer->size());

  // Perform the MAC check and the padding check in constant-time. It should be
  // safe to simply perform the padding check first, but it would not be under a
  // different choice of MAC location on padding failure. See
  // EVP_tls_cbc_remove_padding. The value barrier seems to be necessary to
  // prevent a branch in Clang.
  crypto_word_t good = value_barrier_w(
      constant_time_eq_int(CRYPTO_memcmp(record_mac, mac, mac_len), 0));
  good &= padding_ok;
  if (!constant_time_declassify_w(good)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
    return 0;
  }

  // End of timing-sensitive code.
  CONSTTIME_DECLASSIFY(&data_len, sizeof(data_len));
  for (const CRYPTO_IOVEC &iovec : iovecs) {
    CONSTTIME_DECLASSIFY(iovec.out, iovec.len);
  }

  *out_total_bytes = data_len;
  return 1;
}

static int aead_aes_128_cbc_sha1_tls_init(EVP_AEAD_CTX *ctx, const uint8_t *key,
                                          size_t key_len, size_t tag_len,
                                          enum evp_aead_direction_t dir) {
  return aead_tls_init(ctx, key, key_len, tag_len, dir, EVP_aes_128_cbc(),
                       EVP_sha1(), 0);
}

static int aead_aes_128_cbc_sha1_tls_implicit_iv_init(
    EVP_AEAD_CTX *ctx, const uint8_t *key, size_t key_len, size_t tag_len,
    enum evp_aead_direction_t dir) {
  return aead_tls_init(ctx, key, key_len, tag_len, dir, EVP_aes_128_cbc(),
                       EVP_sha1(), 1);
}

static int aead_aes_128_cbc_sha256_tls_init(EVP_AEAD_CTX *ctx,
                                            const uint8_t *key, size_t key_len,
                                            size_t tag_len,
                                            enum evp_aead_direction_t dir) {
  return aead_tls_init(ctx, key, key_len, tag_len, dir, EVP_aes_128_cbc(),
                       EVP_sha256(), 0);
}

static int aead_aes_256_cbc_sha1_tls_init(EVP_AEAD_CTX *ctx, const uint8_t *key,
                                          size_t key_len, size_t tag_len,
                                          enum evp_aead_direction_t dir) {
  return aead_tls_init(ctx, key, key_len, tag_len, dir, EVP_aes_256_cbc(),
                       EVP_sha1(), 0);
}

static int aead_aes_256_cbc_sha1_tls_implicit_iv_init(
    EVP_AEAD_CTX *ctx, const uint8_t *key, size_t key_len, size_t tag_len,
    enum evp_aead_direction_t dir) {
  return aead_tls_init(ctx, key, key_len, tag_len, dir, EVP_aes_256_cbc(),
                       EVP_sha1(), 1);
}

static int aead_des_ede3_cbc_sha1_tls_init(EVP_AEAD_CTX *ctx,
                                           const uint8_t *key, size_t key_len,
                                           size_t tag_len,
                                           enum evp_aead_direction_t dir) {
  return aead_tls_init(ctx, key, key_len, tag_len, dir, EVP_des_ede3_cbc(),
                       EVP_sha1(), 0);
}

static int aead_des_ede3_cbc_sha1_tls_implicit_iv_init(
    EVP_AEAD_CTX *ctx, const uint8_t *key, size_t key_len, size_t tag_len,
    enum evp_aead_direction_t dir) {
  return aead_tls_init(ctx, key, key_len, tag_len, dir, EVP_des_ede3_cbc(),
                       EVP_sha1(), 1);
}

static int aead_tls_get_iv(const EVP_AEAD_CTX *ctx, const uint8_t **out_iv,
                           size_t *out_iv_len) {
  const AEAD_TLS_CTX *tls_ctx = (AEAD_TLS_CTX *)&ctx->state;
  const size_t iv_len = EVP_CIPHER_CTX_iv_length(&tls_ctx->cipher_ctx);
  if (iv_len <= 1) {
    OPENSSL_PUT_ERROR(CIPHER, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  *out_iv = tls_ctx->cipher_ctx.iv;
  *out_iv_len = iv_len;
  return 1;
}

static const EVP_AEAD aead_aes_128_cbc_sha1_tls = {
    SHA_DIGEST_LENGTH + 16,  // key len (SHA1 + AES128)
    16,                      // nonce len (IV)
    16 + SHA_DIGEST_LENGTH,  // overhead (padding + SHA1)
    SHA_DIGEST_LENGTH,       // max tag length

    nullptr,  // init
    aead_aes_128_cbc_sha1_tls_init,
    aead_tls_cleanup,
    aead_tls_openv,
    aead_tls_sealv,
    nullptr,  // openv_detached
    nullptr,  // get_iv
    aead_tls_tag_len,
};

static const EVP_AEAD aead_aes_128_cbc_sha1_tls_implicit_iv = {
    SHA_DIGEST_LENGTH + 16 + 16,  // key len (SHA1 + AES128 + IV)
    0,                            // nonce len
    16 + SHA_DIGEST_LENGTH,       // overhead (padding + SHA1)
    SHA_DIGEST_LENGTH,            // max tag length

    nullptr,  // init
    aead_aes_128_cbc_sha1_tls_implicit_iv_init,
    aead_tls_cleanup,
    aead_tls_openv,
    aead_tls_sealv,
    nullptr,          // openv_detached
    aead_tls_get_iv,  // get_iv
    aead_tls_tag_len,
};

static const EVP_AEAD aead_aes_128_cbc_sha256_tls = {
    SHA256_DIGEST_LENGTH + 16,  // key len (SHA256 + AES128)
    16,                         // nonce len (IV)
    16 + SHA256_DIGEST_LENGTH,  // overhead (padding + SHA256)
    SHA256_DIGEST_LENGTH,       // max tag length

    nullptr,  // init
    aead_aes_128_cbc_sha256_tls_init,
    aead_tls_cleanup,
    aead_tls_openv,
    aead_tls_sealv,
    nullptr,  // openv_detached
    nullptr,  // get_iv
    aead_tls_tag_len,
};

static const EVP_AEAD aead_aes_256_cbc_sha1_tls = {
    SHA_DIGEST_LENGTH + 32,  // key len (SHA1 + AES256)
    16,                      // nonce len (IV)
    16 + SHA_DIGEST_LENGTH,  // overhead (padding + SHA1)
    SHA_DIGEST_LENGTH,       // max tag length

    nullptr,  // init
    aead_aes_256_cbc_sha1_tls_init,
    aead_tls_cleanup,
    aead_tls_openv,
    aead_tls_sealv,
    nullptr,  // openv_detached
    nullptr,  // get_iv
    aead_tls_tag_len,
};

static const EVP_AEAD aead_aes_256_cbc_sha1_tls_implicit_iv = {
    SHA_DIGEST_LENGTH + 32 + 16,  // key len (SHA1 + AES256 + IV)
    0,                            // nonce len
    16 + SHA_DIGEST_LENGTH,       // overhead (padding + SHA1)
    SHA_DIGEST_LENGTH,            // max tag length

    nullptr,  // init
    aead_aes_256_cbc_sha1_tls_implicit_iv_init,
    aead_tls_cleanup,
    aead_tls_openv,
    aead_tls_sealv,
    nullptr,          // openv_detached
    aead_tls_get_iv,  // get_iv
    aead_tls_tag_len,
};

static const EVP_AEAD aead_des_ede3_cbc_sha1_tls = {
    SHA_DIGEST_LENGTH + 24,  // key len (SHA1 + 3DES)
    8,                       // nonce len (IV)
    8 + SHA_DIGEST_LENGTH,   // overhead (padding + SHA1)
    SHA_DIGEST_LENGTH,       // max tag length

    nullptr,  // init
    aead_des_ede3_cbc_sha1_tls_init,
    aead_tls_cleanup,
    aead_tls_openv,
    aead_tls_sealv,
    nullptr,  // openv_detached
    nullptr,  // get_iv
    aead_tls_tag_len,
};

static const EVP_AEAD aead_des_ede3_cbc_sha1_tls_implicit_iv = {
    SHA_DIGEST_LENGTH + 24 + 8,  // key len (SHA1 + 3DES + IV)
    0,                           // nonce len
    8 + SHA_DIGEST_LENGTH,       // overhead (padding + SHA1)
    SHA_DIGEST_LENGTH,           // max tag length

    nullptr,  // init
    aead_des_ede3_cbc_sha1_tls_implicit_iv_init,
    aead_tls_cleanup,
    aead_tls_openv,
    aead_tls_sealv,
    nullptr,          // openv_detached
    aead_tls_get_iv,  // get_iv
    aead_tls_tag_len,
};

const EVP_AEAD *EVP_aead_aes_128_cbc_sha1_tls() {
  return &aead_aes_128_cbc_sha1_tls;
}

const EVP_AEAD *EVP_aead_aes_128_cbc_sha1_tls_implicit_iv() {
  return &aead_aes_128_cbc_sha1_tls_implicit_iv;
}

const EVP_AEAD *EVP_aead_aes_128_cbc_sha256_tls() {
  return &aead_aes_128_cbc_sha256_tls;
}

const EVP_AEAD *EVP_aead_aes_256_cbc_sha1_tls() {
  return &aead_aes_256_cbc_sha1_tls;
}

const EVP_AEAD *EVP_aead_aes_256_cbc_sha1_tls_implicit_iv() {
  return &aead_aes_256_cbc_sha1_tls_implicit_iv;
}

const EVP_AEAD *EVP_aead_des_ede3_cbc_sha1_tls() {
  return &aead_des_ede3_cbc_sha1_tls;
}

const EVP_AEAD *EVP_aead_des_ede3_cbc_sha1_tls_implicit_iv() {
  return &aead_des_ede3_cbc_sha1_tls_implicit_iv;
}
