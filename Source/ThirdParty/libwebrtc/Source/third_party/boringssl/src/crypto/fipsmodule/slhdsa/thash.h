// Copyright 2024 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CRYPTO_FIPSMODULE_SLHDSA_THASH_H
#define OPENSSL_HEADER_CRYPTO_FIPSMODULE_SLHDSA_THASH_H

#include <openssl/sha2.h>

#include "./params.h"


BSSL_NAMESPACE_BEGIN

// slhdsa_hash_ctx holds the public seed, plus precomputed hash state, that is
// shared by every F, H, T_l, and PRF call made under a single key.
struct slhdsa_hash_ctx {
  // pk_seed is PK.seed. It must outlive the context.
  const uint8_t *pk_seed;
  // sha256_seeded is only used for SHA-256 parameter sets. It holds the
  // SHA-256 state after absorbing PK.seed || toByte(0, 64 - n), the first
  // block of every F, H, T_l, and PRF input (Section 11.2, pages 44-46).
  // Computing it once per key saves one compression function call per
  // tweakable hash call.
  SHA256_CTX sha256_seeded;
};

// slhdsa_hash_ctx_init initializes `hash_ctx` for computing tweakable hashes
// with public seed `pk_seed`, which must outlive `hash_ctx`.
void slhdsa_hash_ctx_init(const slh_dsa_config *config,
                          slhdsa_hash_ctx *hash_ctx, const uint8_t *pk_seed);

// Implements PRF_msg: a pseudo-random function that is used to generate the
// randomizer r for the randomized hashing of the message to be signed.
// (Section 4.1, page 11)
void slhdsa_thash_prfmsg(const slh_dsa_config *config, uint8_t *output,
                         const uint8_t *sk_prf, const uint8_t *opt_rand,
                         const uint8_t *header, const uint8_t *ctx,
                         size_t ctx_len, const uint8_t *msg, size_t msg_len);

// Implements H_msg: a hash function used to generate the digest of the message
// to be signed. (Section 4.1, page 11)
void slhdsa_thash_hmsg(const slh_dsa_config *config, uint8_t *output,
                       const uint8_t *r, const uint8_t *pk_seed,
                       const uint8_t *pk_root, const uint8_t *header,
                       const uint8_t *ctx, size_t ctx_len, const uint8_t *msg,
                       size_t msg_len);

// Implements PRF: a pseudo-random function that is used to generate the secret
// values in WOTS+ and FORS private keys. (Section 4.1, page 11)
void slhdsa_thash_prf(const slh_dsa_config *config, uint8_t *output,
                      const slhdsa_hash_ctx *hash_ctx, const uint8_t *sk_seed,
                      uint8_t addr[32]);

// Implements T_l: a hash function that maps an l*n-byte message to an n-byte
// message. Used for WOTS+ public key compression. (Section 4.1, page 11)
void slhdsa_thash_tl(const slh_dsa_config *config, uint8_t *output,
                     const uint8_t *input, const slhdsa_hash_ctx *hash_ctx,
                     uint8_t addr[32]);

// Implements H: a hash function that takes a 2*n-byte message as input and
// produces an n-byte output. (Section 4.1, page 11)
void slhdsa_thash_h(const slh_dsa_config *config, uint8_t *output,
                    const uint8_t *input, const slhdsa_hash_ctx *hash_ctx,
                    uint8_t addr[32]);

// Implements F: a hash function that takes an n-byte message as input and
// produces an n-byte output. (Section 4.1, page 11)
void slhdsa_thash_f(const slh_dsa_config *config, uint8_t *output,
                    const uint8_t *input, const slhdsa_hash_ctx *hash_ctx,
                    uint8_t addr[32]);

// Implements T_k: a hash function that maps a k*n-byte message to an n-byte
// message. Used for FORS public key compression. (Section 4.1, page 11)
void slhdsa_thash_tk(const slh_dsa_config *config, uint8_t *output,
                     const uint8_t *input, const slhdsa_hash_ctx *hash_ctx,
                     uint8_t addr[32]);

BSSL_NAMESPACE_END

#endif  // OPENSSL_HEADER_CRYPTO_FIPSMODULE_SLHDSA_THASH_H
