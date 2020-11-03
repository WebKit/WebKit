/* Copyright (c) 2020, Google Inc.
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

#ifndef OPENSSL_HEADER_CRYPTO_HPKE_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_HPKE_INTERNAL_H

#include <openssl/aead.h>
#include <openssl/base.h>
#include <openssl/curve25519.h>

#if defined(__cplusplus)
extern "C" {
#endif


// Hybrid Public Key Encryption.
//
// Hybrid Public Key Encryption (HPKE) enables a sender to encrypt messages to a
// receiver with a public key. Optionally, the sender may authenticate its
// possession of a pre-shared key to the recipient.
//
// See https://tools.ietf.org/html/draft-irtf-cfrg-hpke-05.

// EVP_HPKE_AEAD_* are AEAD identifiers.
#define EVP_HPKE_AEAD_AES_GCM_128 0x0001
#define EVP_HPKE_AEAD_AES_GCM_256 0x0002
#define EVP_HPKE_AEAD_CHACHA20POLY1305 0x0003

// EVP_HPKE_HKDF_* are HKDF identifiers.
#define EVP_HPKE_HKDF_SHA256 0x0001
#define EVP_HPKE_HKDF_SHA384 0x0002
#define EVP_HPKE_HKDF_SHA512 0x0003

// EVP_HPKE_MAX_OVERHEAD contains the largest value that
// |EVP_HPKE_CTX_max_overhead| would ever return for any context.
#define EVP_HPKE_MAX_OVERHEAD EVP_AEAD_MAX_OVERHEAD


// Encryption contexts.

// An |EVP_HPKE_CTX| is an HPKE encryption context.
typedef struct evp_hpke_ctx_st {
  const EVP_MD *hkdf_md;
  EVP_AEAD_CTX aead_ctx;
  uint16_t kdf_id;
  uint16_t aead_id;
  uint8_t nonce[EVP_AEAD_MAX_NONCE_LENGTH];
  uint8_t exporter_secret[EVP_MAX_MD_SIZE];
  uint64_t seq;
  int is_sender;
} EVP_HPKE_CTX;

// EVP_HPKE_CTX_init initializes an already-allocated |EVP_HPKE_CTX|. The caller
// should then use one of the |EVP_HPKE_CTX_setup_*| functions.
//
// It is safe, but not necessary to call |EVP_HPKE_CTX_cleanup| in this state.
OPENSSL_EXPORT void EVP_HPKE_CTX_init(EVP_HPKE_CTX *ctx);

// EVP_HPKE_CTX_cleanup releases memory referenced by |ctx|. |ctx| must have
// been initialized with |EVP_HPKE_CTX_init|.
OPENSSL_EXPORT void EVP_HPKE_CTX_cleanup(EVP_HPKE_CTX *ctx);


// Setting up HPKE contexts.
//
// In each of the following functions, |hpke| must have been initialized with
// |EVP_HPKE_CTX_init|. |kdf_id| selects the KDF for non-KEM HPKE operations and
// must be one of the |EVP_HPKE_HKDF_*| constants. |aead_id| selects the AEAD
// for the "open" and "seal" operations and must be one of the |EVP_HPKE_AEAD_*"
// constants."

// EVP_HPKE_CTX_setup_base_s_x25519 sets up |hpke| as a sender context that can
// encrypt for the private key corresponding to |peer_public_value| (the
// recipient's public key). It returns one on success, and zero otherwise. Note
// that this function will fail if |peer_public_value| is invalid.
//
// This function writes the encapsulated shared secret to |out_enc|.
OPENSSL_EXPORT int EVP_HPKE_CTX_setup_base_s_x25519(
    EVP_HPKE_CTX *hpke, uint8_t out_enc[X25519_PUBLIC_VALUE_LEN],
    uint16_t kdf_id, uint16_t aead_id,
    const uint8_t peer_public_value[X25519_PUBLIC_VALUE_LEN],
    const uint8_t *info, size_t info_len);

// EVP_HPKE_CTX_setup_base_s_x25519_for_test behaves like
// |EVP_HPKE_CTX_setup_base_s_x25519|, but takes a pre-generated ephemeral
// sender key.
OPENSSL_EXPORT int EVP_HPKE_CTX_setup_base_s_x25519_for_test(
    EVP_HPKE_CTX *hpke, uint16_t kdf_id, uint16_t aead_id,
    const uint8_t peer_public_value[X25519_PUBLIC_VALUE_LEN],
    const uint8_t *info, size_t info_len,
    const uint8_t ephemeral_private[X25519_PRIVATE_KEY_LEN],
    const uint8_t ephemeral_public[X25519_PUBLIC_VALUE_LEN]);

// EVP_HPKE_CTX_setup_base_r_x25519 sets up |hpke| as a recipient context that
// can decrypt messages. |private_key| is the recipient's private key, and |enc|
// is the encapsulated shared secret from the sender. Note that this function
// will fail if |enc| is invalid.
OPENSSL_EXPORT int EVP_HPKE_CTX_setup_base_r_x25519(
    EVP_HPKE_CTX *hpke, uint16_t kdf_id, uint16_t aead_id,
    const uint8_t enc[X25519_PUBLIC_VALUE_LEN],
    const uint8_t public_key[X25519_PUBLIC_VALUE_LEN],
    const uint8_t private_key[X25519_PRIVATE_KEY_LEN], const uint8_t *info,
    size_t info_len);

// EVP_HPKE_CTX_setup_psk_s_x25519 sets up |hpke| as a sender context that can
// encrypt for the private key corresponding to |peer_public_value| (the
// recipient's public key) and authenticate its possession of a PSK. It returns
// one on success, and zero otherwise. Note that this function will fail if
// |peer_public_value| is invalid.
//
// The PSK and its ID must be provided in |psk| and |psk_id|, respectively. Both
// must be nonempty (|psk_len| and |psk_id_len| must be non-zero), or this
// function will fail.
//
// This function writes the encapsulated shared secret to |out_enc|.
OPENSSL_EXPORT int EVP_HPKE_CTX_setup_psk_s_x25519(
    EVP_HPKE_CTX *hpke, uint8_t out_enc[X25519_PUBLIC_VALUE_LEN],
    uint16_t kdf_id, uint16_t aead_id,
    const uint8_t peer_public_value[X25519_PUBLIC_VALUE_LEN],
    const uint8_t *info, size_t info_len, const uint8_t *psk, size_t psk_len,
    const uint8_t *psk_id, size_t psk_id_len);

// EVP_HPKE_CTX_setup_psk_s_x25519_for_test behaves like
// |EVP_HPKE_CTX_setup_psk_s_x25519|, but takes a pre-generated ephemeral sender
// key.
OPENSSL_EXPORT int EVP_HPKE_CTX_setup_psk_s_x25519_for_test(
    EVP_HPKE_CTX *hpke, uint16_t kdf_id, uint16_t aead_id,
    const uint8_t peer_public_value[X25519_PUBLIC_VALUE_LEN],
    const uint8_t *info, size_t info_len, const uint8_t *psk, size_t psk_len,
    const uint8_t *psk_id, size_t psk_id_len,
    const uint8_t ephemeral_private[X25519_PRIVATE_KEY_LEN],
    const uint8_t ephemeral_public[X25519_PUBLIC_VALUE_LEN]);

// EVP_HPKE_CTX_setup_psk_r_x25519 sets up |hpke| as a recipient context that
// can decrypt messages. Future open (decrypt) operations will fail if the
// sender does not possess the PSK indicated by |psk| and |psk_id|.
// |private_key| is the recipient's private key, and |enc| is the encapsulated
// shared secret from the sender. If |enc| is invalid, this function will fail.
//
// The PSK and its ID must be provided in |psk| and |psk_id|, respectively. Both
// must be nonempty (|psk_len| and |psk_id_len| must be non-zero), or this
// function will fail.
OPENSSL_EXPORT int EVP_HPKE_CTX_setup_psk_r_x25519(
    EVP_HPKE_CTX *hpke, uint16_t kdf_id, uint16_t aead_id,
    const uint8_t enc[X25519_PUBLIC_VALUE_LEN],
    const uint8_t public_key[X25519_PUBLIC_VALUE_LEN],
    const uint8_t private_key[X25519_PRIVATE_KEY_LEN], const uint8_t *info,
    size_t info_len, const uint8_t *psk, size_t psk_len, const uint8_t *psk_id,
    size_t psk_id_len);


// Using an HPKE context.

// EVP_HPKE_CTX_open uses the HPKE context |hpke| to authenticate |in_len| bytes
// from |in| and |ad_len| bytes from |ad| and to decrypt at most |in_len| bytes
// into |out|. It returns one on success, and zero otherwise.
//
// This operation will fail if the |hpke| context is not set up as a receiver.
//
// Note that HPKE encryption is stateful and ordered. The sender's first call to
// |EVP_HPKE_CTX_seal| must correspond to the recipient's first call to
// |EVP_HPKE_CTX_open|, etc.
//
// At most |in_len| bytes are written to |out|. In order to ensure success,
// |max_out_len| should be at least |in_len|. On successful return, |*out_len|
// is set to the actual number of bytes written.
OPENSSL_EXPORT int EVP_HPKE_CTX_open(EVP_HPKE_CTX *hpke, uint8_t *out,
                                     size_t *out_len, size_t max_out_len,
                                     const uint8_t *in, size_t in_len,
                                     const uint8_t *ad, size_t ad_len);

// EVP_HPKE_CTX_seal uses the HPKE context |hpke| to encrypt and authenticate
// |in_len| bytes of ciphertext |in| and authenticate |ad_len| bytes from |ad|,
// writing the result to |out|. It returns one on success and zero otherwise.
//
// This operation will fail if the |hpke| context is not set up as a sender.
//
// Note that HPKE encryption is stateful and ordered. The sender's first call to
// |EVP_HPKE_CTX_seal| must correspond to the recipient's first call to
// |EVP_HPKE_CTX_open|, etc.
//
// At most, |max_out_len| encrypted bytes are written to |out|. On successful
// return, |*out_len| is set to the actual number of bytes written.
//
// To ensure success, |max_out_len| should be |in_len| plus the result of
// |EVP_HPKE_CTX_max_overhead| or |EVP_HPKE_MAX_OVERHEAD|.
OPENSSL_EXPORT int EVP_HPKE_CTX_seal(EVP_HPKE_CTX *hpke, uint8_t *out,
                                     size_t *out_len, size_t max_out_len,
                                     const uint8_t *in, size_t in_len,
                                     const uint8_t *ad, size_t ad_len);

// EVP_HPKE_CTX_export uses the HPKE context |hpke| to export a secret of
// |secret_len| bytes into |out|. This function uses |context_len| bytes from
// |context| as a context string for the secret. This is necessary to separate
// different uses of exported secrets and bind relevant caller-specific context
// into the output. It returns one on success and zero otherwise.
OPENSSL_EXPORT int EVP_HPKE_CTX_export(const EVP_HPKE_CTX *hpke, uint8_t *out,
                                       size_t secret_len,
                                       const uint8_t *context,
                                       size_t context_len);

// EVP_HPKE_CTX_max_overhead returns the maximum number of additional bytes
// added by sealing data with |EVP_HPKE_CTX_seal|. The |hpke| context must be
// set up as a sender.
OPENSSL_EXPORT size_t EVP_HPKE_CTX_max_overhead(const EVP_HPKE_CTX *hpke);


#if defined(__cplusplus)
}  // extern C
#endif

#if !defined(BORINGSSL_NO_CXX)
extern "C++" {

BSSL_NAMESPACE_BEGIN

using ScopedEVP_HPKE_CTX =
    internal::StackAllocated<EVP_HPKE_CTX, void, EVP_HPKE_CTX_init,
                             EVP_HPKE_CTX_cleanup>;

BSSL_NAMESPACE_END

}  // extern C++
#endif

#endif  // OPENSSL_HEADER_CRYPTO_HPKE_INTERNAL_H
