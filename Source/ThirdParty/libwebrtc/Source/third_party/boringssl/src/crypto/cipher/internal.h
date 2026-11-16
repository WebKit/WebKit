// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_CRYPTO_CIPHER_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_CIPHER_INTERNAL_H

#include <assert.h>
#include <stdlib.h>

#include <openssl/aead.h>
#include <openssl/base.h>
#include <openssl/sha.h>
#include <openssl/span.h>

#include "../internal.h"


BSSL_NAMESPACE_BEGIN

// EVP_tls_cbc_get_padding determines the padding from the decrypted, TLS, CBC
// record in `in`. This decrypted record should not include any "decrypted"
// explicit IV. If the record is publicly invalid, it returns zero. Otherwise,
// it returns one and sets `*out_padding_ok` to all ones (0xfff..f) if the
// padding is valid and zero otherwise. It then sets `*out_len` to the length
// with the padding removed or `in_len` if invalid.
//
// If the function returns one, it runs in time independent of the contents of
// `in`. It is also guaranteed that, independent of `*out_padding_ok`, `mac_len`
// <= `*out_len` <= `in_len`, satisfying `EVP_tls_cbc_copy_mac`'s precondition.
int EVP_tls_cbc_remove_padding(crypto_word_t *out_padding_ok, size_t *out_len,
                               const uint8_t *in, size_t in_len,
                               size_t block_size, size_t mac_size);

// EVP_tls_cbc_copy_mac copies `md_size` bytes from the end of the first
// `in_len` bytes of `in` to `out` in constant time (independent of the concrete
// value of `in_len`, which may vary within a 256-byte window). `in` must point
// to a buffer of `orig_len` bytes.
//
// On entry:
//   orig_len >= in_len >= md_size
//   md_size <= EVP_MAX_MD_SIZE
void EVP_tls_cbc_copy_mac(uint8_t *out, size_t md_size, const uint8_t *in,
                          size_t in_len, size_t orig_len);

// EVP_tls_cbc_record_digest_supported returns 1 iff `md` is a hash function
// which EVP_tls_cbc_digest_record supports.
int EVP_tls_cbc_record_digest_supported(const EVP_MD *md);

// EVP_sha1_final_with_secret_suffix computes the result of hashing `len` bytes
// from `in` to `ctx` and writes the resulting hash to `out`. `len` is treated
// as secret and must be at most `max_len`, which is treated as public. `in`
// must point to a buffer of at least `max_len` bytes. It returns one on success
// and zero if inputs are too long.
//
// This function is exported for unit tests.
OPENSSL_EXPORT int EVP_sha1_final_with_secret_suffix(
    SHA_CTX *ctx, uint8_t out[SHA_DIGEST_LENGTH], const uint8_t *in, size_t len,
    size_t max_len);

// EVP_sha256_final_with_secret_suffix acts like
// `EVP_sha1_final_with_secret_suffix`, but for SHA-256.
//
// This function is exported for unit tests.
OPENSSL_EXPORT int EVP_sha256_final_with_secret_suffix(
    SHA256_CTX *ctx, uint8_t out[SHA256_DIGEST_LENGTH], const uint8_t *in,
    size_t len, size_t max_len);

// EVP_tls_cbc_digest_record computes the MAC of a decrypted, padded TLS
// record.
//
//   md: the hash function used in the HMAC.
//     EVP_tls_cbc_record_digest_supported must return true for this hash.
//   md_out: the digest output. At most EVP_MAX_MD_SIZE bytes will be written.
//   md_out_size: the number of output bytes is written here.
//   len_header: the two length bytes of the TLS record header.
//   aadvecs: the 11-byte TLS record header as it was provided by the caller.
//   iovecs_without_trailer: the section of the plaintext that does not include
//     the trailer whose length is secret (typically the entire plaintext with
//     an upper bound of padding and MAC size removed)
//   trailer: a buffer, of public length, containing the remainder of the
//     plaintext as a prefix.
//   data_in_trailer_size: the secret, reported length of the data portion in
//     `trailer` once the padding and MAC have been removed.
//
// On entry: by virtue of having been through one of the remove_padding
// functions, above, we know that data_plus_mac_size is large enough to contain
// a padding byte and MAC. (If the padding was invalid, it might contain the
// padding too. )
int EVP_tls_cbc_digest_record(
    const EVP_MD *md, uint8_t *md_out, size_t *md_out_size,
    const uint8_t len_header[2], bssl::Span<const CRYPTO_IVEC> aadvecs,
    bssl::Span<const CRYPTO_IOVEC> iovecs_without_trailer,
    bssl::Span<const uint8_t> trailer, size_t data_in_trailer_size,
    const uint8_t *mac_secret, unsigned mac_secret_length);


// ChaCha20-Poly1305 Assembly.

#define POLY1305_TAG_LEN 16

// For convenience (the x86_64 calling convention allows only six parameters in
// registers), the final parameter for the assembly functions is both an input
// and output parameter.
union chacha20_poly1305_open_data {
  struct {
    alignas(16) uint8_t key[32];
    uint32_t counter;
    uint8_t nonce[12];
  } in;
  struct {
    uint8_t tag[POLY1305_TAG_LEN];
  } out;
};

union chacha20_poly1305_seal_data {
  struct {
    alignas(16) uint8_t key[32];
    uint32_t counter;
    uint8_t nonce[12];
    const uint8_t *extra_ciphertext;
    size_t extra_ciphertext_len;
  } in;
  struct {
    uint8_t tag[POLY1305_TAG_LEN];
  } out;
};

#if (defined(OPENSSL_X86_64) || defined(OPENSSL_AARCH64)) && \
    !defined(OPENSSL_NO_ASM)

static_assert(sizeof(union chacha20_poly1305_open_data) == 48,
              "wrong chacha20_poly1305_open_data size");
static_assert(sizeof(union chacha20_poly1305_seal_data) == 48 + 8 + 8,
              "wrong chacha20_poly1305_seal_data size");

inline int chacha20_poly1305_asm_capable() {
#if defined(OPENSSL_X86_64)
  return CRYPTO_is_SSE4_1_capable();
#elif defined(OPENSSL_AARCH64)
  return CRYPTO_is_NEON_capable();
#endif
}

// chacha20_poly1305_open is defined in chacha20_poly1305_*.pl. It decrypts
// `plaintext_len` bytes from `ciphertext` and writes them to `out_plaintext`.
// Additional input parameters are passed in `aead_data->in`. On exit, it will
// write calculated tag value to `aead_data->out.tag`, which the caller must
// check.
#if defined(OPENSSL_X86_64)
extern "C" void chacha20_poly1305_open_sse41(
    uint8_t *out_plaintext, const uint8_t *ciphertext, size_t plaintext_len,
    const uint8_t *ad, size_t ad_len, union chacha20_poly1305_open_data *data);
extern "C" void chacha20_poly1305_open_avx2(
    uint8_t *out_plaintext, const uint8_t *ciphertext, size_t plaintext_len,
    const uint8_t *ad, size_t ad_len, union chacha20_poly1305_open_data *data);
inline void chacha20_poly1305_open(uint8_t *out_plaintext,
                                   const uint8_t *ciphertext,
                                   size_t plaintext_len, const uint8_t *ad,
                                   size_t ad_len,
                                   union chacha20_poly1305_open_data *data) {
  if (CRYPTO_is_AVX2_capable() && CRYPTO_is_BMI2_capable()) {
    chacha20_poly1305_open_avx2(out_plaintext, ciphertext, plaintext_len, ad,
                                ad_len, data);
  } else {
    chacha20_poly1305_open_sse41(out_plaintext, ciphertext, plaintext_len, ad,
                                 ad_len, data);
  }
}
#else
extern "C" void chacha20_poly1305_open(uint8_t *out_plaintext,
                                       const uint8_t *ciphertext,
                                       size_t plaintext_len, const uint8_t *ad,
                                       size_t ad_len,
                                       union chacha20_poly1305_open_data *data);
#endif

// chacha20_poly1305_open is defined in chacha20_poly1305_*.pl. It encrypts
// `plaintext_len` bytes from `plaintext` and writes them to `out_ciphertext`.
// Additional input parameters are passed in `aead_data->in`. The calculated tag
// value is over the computed ciphertext concatenated with `extra_ciphertext`
// and written to `aead_data->out.tag`.
#if defined(OPENSSL_X86_64)
extern "C" void chacha20_poly1305_seal_sse41(
    uint8_t *out_ciphertext, const uint8_t *plaintext, size_t plaintext_len,
    const uint8_t *ad, size_t ad_len, union chacha20_poly1305_seal_data *data);
extern "C" void chacha20_poly1305_seal_avx2(
    uint8_t *out_ciphertext, const uint8_t *plaintext, size_t plaintext_len,
    const uint8_t *ad, size_t ad_len, union chacha20_poly1305_seal_data *data);
inline void chacha20_poly1305_seal(uint8_t *out_ciphertext,
                                   const uint8_t *plaintext,
                                   size_t plaintext_len, const uint8_t *ad,
                                   size_t ad_len,
                                   union chacha20_poly1305_seal_data *data) {
  if (CRYPTO_is_AVX2_capable() && CRYPTO_is_BMI2_capable()) {
    chacha20_poly1305_seal_avx2(out_ciphertext, plaintext, plaintext_len, ad,
                                ad_len, data);
  } else {
    chacha20_poly1305_seal_sse41(out_ciphertext, plaintext, plaintext_len, ad,
                                 ad_len, data);
  }
}
#else
extern "C" void chacha20_poly1305_seal(uint8_t *out_ciphertext,
                                       const uint8_t *plaintext,
                                       size_t plaintext_len, const uint8_t *ad,
                                       size_t ad_len,
                                       union chacha20_poly1305_seal_data *data);
#endif

#else

inline int chacha20_poly1305_asm_capable() { return 0; }

inline void chacha20_poly1305_open(uint8_t *out_plaintext,
                                   const uint8_t *ciphertext,
                                   size_t plaintext_len, const uint8_t *ad,
                                   size_t ad_len,
                                   union chacha20_poly1305_open_data *data) {
  abort();
}

inline void chacha20_poly1305_seal(uint8_t *out_ciphertext,
                                   const uint8_t *plaintext,
                                   size_t plaintext_len, const uint8_t *ad,
                                   size_t ad_len,
                                   union chacha20_poly1305_seal_data *data) {
  abort();
}
#endif


// AES-GCM-SIV Assembly.

// TODO(davidben): AES-GCM-SIV assembly is not correct for Windows. It must save
// and restore xmm6 through xmm15.
#if defined(OPENSSL_X86_64) && !defined(OPENSSL_NO_ASM) && \
    !defined(OPENSSL_WINDOWS)
#define AES_GCM_SIV_ASM

struct aead_aes_gcm_siv_asm_ctx {
  alignas(16) uint8_t key[16 * 15];
  int is_128_bit;
};

inline int aes_gcm_siv_asm_capable() {
  return CRYPTO_is_AVX_capable() && CRYPTO_is_AESNI_capable() &&
         CRYPTO_is_PCLMUL_capable();
}

extern "C" {
// aes128gcmsiv_aes_ks writes an AES-128 key schedule for `key` to
// `out_expanded_key`. `out_expanded_key` must be 16-byte aligned.
extern void aes128gcmsiv_aes_ks(const uint8_t key[16],
                                uint8_t out_expanded_key[16 * 15]);

// aes256gcmsiv_aes_ks writes an AES-256 key schedule for `key` to
// `out_expanded_key`. `out_expanded_key` must be 16-byte aligned.
extern void aes256gcmsiv_aes_ks(const uint8_t key[32],
                                uint8_t out_expanded_key[16 * 15]);

// aesgcmsiv_polyval_horner updates the POLYVAL value in `in_out_poly` to
// include a number (`in_blocks`) of 16-byte blocks of data from `in`, given
// the POLYVAL key in `key`. `in_out_poly` and `key` must be 16-byte aligned.
extern void aesgcmsiv_polyval_horner(const uint8_t in_out_poly[16],
                                     const uint8_t key[16], const uint8_t *in,
                                     size_t in_blocks);

// aesgcmsiv_htable_init writes powers 1..8 of `auth_key` to `out_htable`.
// `out_htable` and `auth_key` must be 16-byte aligned.
extern void aesgcmsiv_htable_init(uint8_t out_htable[16 * 8],
                                  const uint8_t auth_key[16]);

// aesgcmsiv_htable6_init writes powers 1..6 of `auth_key` to `out_htable`.
// `out_htable` and `auth_key` must be 16-byte aligned.
extern void aesgcmsiv_htable6_init(uint8_t out_htable[16 * 6],
                                   const uint8_t auth_key[16]);

// aesgcmsiv_htable_polyval updates the POLYVAL value in `in_out_poly` to
// include `in_len` bytes of data from `in`. (Where `in_len` must be a multiple
// of 16.) It uses the precomputed powers of the key given in `htable`.
// `in_out_poly` and `htable` must be 16-byte aligned.
extern void aesgcmsiv_htable_polyval(const uint8_t htable[16 * 8],
                                     const uint8_t *in, size_t in_len,
                                     uint8_t in_out_poly[16]);

// aes128gcmsiv_dec decrypts `in_len` & ~15 bytes from `out` and writes them to
// `in`. `in` and `out` may be equal, but must not otherwise alias.
//
// `in_out_calculated_tag_and_scratch`, on entry, must contain:
//    1. The current value of the calculated tag, which will be updated during
//       decryption and written back to the beginning of this buffer on exit.
//    2. The claimed tag, which is needed to derive counter values.
//
// While decrypting, the whole of `in_out_calculated_tag_and_scratch` may be
// used for other purposes. `in_out_calculated_tag_and_scratch` and `htable`
// must be 16-byte aligned. In order to decrypt and update the POLYVAL value, it
// uses the expanded key from `key` and the table of powers in `htable`.
extern void aes128gcmsiv_dec(const uint8_t *in, uint8_t *out,
                             uint8_t in_out_calculated_tag_and_scratch[16 * 8],
                             const uint8_t htable[16 * 6],
                             const struct aead_aes_gcm_siv_asm_ctx *key,
                             size_t in_len);

// aes256gcmsiv_dec acts like `aes128gcmsiv_dec`, but for AES-256.
// `in_out_calculated_tag_and_scratch` and `htable` must be 16-byte aligned.
extern void aes256gcmsiv_dec(const uint8_t *in, uint8_t *out,
                             uint8_t in_out_calculated_tag_and_scratch[16 * 8],
                             const uint8_t htable[16 * 6],
                             const struct aead_aes_gcm_siv_asm_ctx *key,
                             size_t in_len);

// aes128gcmsiv_kdf performs the AES-GCM-SIV KDF given the expanded key from
// `key_schedule` and the nonce in `nonce`. Note that, while only 12 bytes of
// the nonce are used, 16 bytes are read and so the value must be
// right-padded. `nonce`, `out_key_material`, and `key_schedule` must be
// 16-byte aligned.
extern void aes128gcmsiv_kdf(const uint8_t nonce[16],
                             uint64_t out_key_material[8],
                             const uint8_t *key_schedule);

// aes256gcmsiv_kdf acts like `aes128gcmsiv_kdf`, but for AES-256. `nonce`,
// `out_key_material`, and `key_schedule` must be 16-byte aligned.
extern void aes256gcmsiv_kdf(const uint8_t nonce[16],
                             uint64_t out_key_material[12],
                             const uint8_t *key_schedule);

// aes128gcmsiv_aes_ks_enc_x1 performs a key expansion of the AES-128 key in
// `key`, writes the expanded key to `out_expanded_key` and encrypts a single
// block from `in` to `out`. `in`, `out`, `out_expanded_key`, and `key` must be
// 16-byte aligned.
extern void aes128gcmsiv_aes_ks_enc_x1(const uint8_t in[16], uint8_t out[16],
                                       uint8_t out_expanded_key[16 * 15],
                                       const uint64_t key[2]);

// aes256gcmsiv_aes_ks_enc_x1 acts like `aes128gcmsiv_aes_ks_enc_x1`, but for
// AES-256. `in`, `out`, `out_expanded_key`, and `key` must be 16-byte aligned.
extern void aes256gcmsiv_aes_ks_enc_x1(const uint8_t in[16], uint8_t out[16],
                                       uint8_t out_expanded_key[16 * 15],
                                       const uint64_t key[4]);

// aes128gcmsiv_ecb_enc_block encrypts a single block from `in` to `out` using
// the expanded key in `expanded_key`. `in` and `out` must be 16-byte aligned.
extern void aes128gcmsiv_ecb_enc_block(
    const uint8_t in[16], uint8_t out[16],
    const struct aead_aes_gcm_siv_asm_ctx *expanded_key);

// aes256gcmsiv_ecb_enc_block acts like `aes128gcmsiv_ecb_enc_block`, but for
// AES-256. `in` and `out` must be 16-byte aligned.
extern void aes256gcmsiv_ecb_enc_block(
    const uint8_t in[16], uint8_t out[16],
    const struct aead_aes_gcm_siv_asm_ctx *expanded_key);

// aes128gcmsiv_enc_msg_x4 encrypts `in_len` bytes from `in` to `out` using the
// expanded key from `key`. (The value of `in_len` must be a multiple of 16.)
// The `in` and `out` buffers may be equal but must not otherwise overlap. The
// initial counter is constructed from the given `tag` as required by
// AES-GCM-SIV. `tag` must be 16-byte aligned.
extern void aes128gcmsiv_enc_msg_x4(const uint8_t *in, uint8_t *out,
                                    const uint8_t *tag,
                                    const struct aead_aes_gcm_siv_asm_ctx *key,
                                    size_t in_len);

// aes256gcmsiv_enc_msg_x4 acts like `aes128gcmsiv_enc_msg_x4`, but for
// AES-256. `tag` must be 16-byte aligned.
extern void aes256gcmsiv_enc_msg_x4(const uint8_t *in, uint8_t *out,
                                    const uint8_t *tag,
                                    const struct aead_aes_gcm_siv_asm_ctx *key,
                                    size_t in_len);

// aes128gcmsiv_enc_msg_x8 acts like `aes128gcmsiv_enc_msg_x4`, but is
// optimised for longer messages.
extern void aes128gcmsiv_enc_msg_x8(const uint8_t *in, uint8_t *out,
                                    const uint8_t *tag,
                                    const struct aead_aes_gcm_siv_asm_ctx *key,
                                    size_t in_len);

// aes256gcmsiv_enc_msg_x8 acts like `aes256gcmsiv_enc_msg_x4`, but is
// optimised for longer messages.
extern void aes256gcmsiv_enc_msg_x8(const uint8_t *in, uint8_t *out,
                                    const uint8_t *tag,
                                    const struct aead_aes_gcm_siv_asm_ctx *key,
                                    size_t in_len);
}
#endif  // OPENSSL_X86_64 && !OPENSSL_NO_ASM && !OPENSSL_WINDOWS

BSSL_NAMESPACE_END

#endif  // OPENSSL_HEADER_CRYPTO_CIPHER_INTERNAL_H
