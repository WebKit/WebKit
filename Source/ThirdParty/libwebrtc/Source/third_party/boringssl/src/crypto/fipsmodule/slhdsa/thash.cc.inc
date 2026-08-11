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

#include <openssl/base.h>

#include <assert.h>
#include <string.h>

#include <openssl/sha2.h>

#include "../../internal.h"
#include "../keccak/internal.h"
#include "./thash.h"


using namespace bssl;

// Internal thash function used by F, H, and T_l for SHA-256 based parameter
// sets (Section 11.2, pages 44-46)
static void slhdsa_thash_sha256(const slh_dsa_config *config, uint8_t *output,
                                const uint8_t *input, size_t input_blocks,
                                const uint8_t *pk_seed, uint8_t addr[32]) {
  BSSL_CHECK(config->hash_type == SLH_DSA_HASH_SHA2_256);
  SHA256_CTX sha256;
  SHA256_Init(&sha256);

  uint8_t zeros[SLHDSA_MAX_HASH_BLOCK_BYTES] = {0};
  BSSL_CHECK(config->hash_block_bytes <= sizeof(zeros));
  BSSL_CHECK(config->hash_block_bytes >= config->n);
  SHA256_Update(&sha256, pk_seed, config->n);
  SHA256_Update(&sha256, zeros, config->hash_block_bytes - config->n);
  SHA256_Update(&sha256, addr, SLHDSA_ADDR_COMPRESSED_BYTES);
  SHA256_Update(&sha256, input, input_blocks * config->n);

  uint8_t hash[SLHDSA_MAX_DIGEST_SIZE];
  SHA256_Final(hash, &sha256);
  OPENSSL_memcpy(output, hash, config->n);
}

// Internal thash function used by F, H, and T_l for SHAKE-256 based parameter
// sets (Section 11.1, pages 43-45)
static void slhdsa_thash_shake(const slh_dsa_config *config, uint8_t *output,
                               const uint8_t *input, size_t input_blocks,
                               const uint8_t *pk_seed, uint8_t addr[32]) {
  BSSL_CHECK(config->hash_type == SLH_DSA_HASH_SHAKE_256);
  BSSL_CHECK(!config->compressed_addresses);

  struct BORINGSSL_keccak_st ctx;
  BORINGSSL_keccak_init(&ctx, boringssl_shake256);
  BORINGSSL_keccak_absorb(&ctx, pk_seed, config->n);
  BORINGSSL_keccak_absorb(&ctx, addr, SLHDSA_ADDR_BYTES);
  BORINGSSL_keccak_absorb(&ctx, input, input_blocks * config->n);
  BORINGSSL_keccak_squeeze(&ctx, output, config->n);
}

static void slhdsa_thash_dispatch(const slh_dsa_config *config, uint8_t *output,
                                  const uint8_t *input, size_t input_blocks,
                                  const uint8_t *pk_seed, uint8_t addr[32]) {
  switch (config->hash_type) {
    case SLH_DSA_HASH_SHA2_256:
      slhdsa_thash_sha256(config, output, input, input_blocks, pk_seed, addr);
      return;
    case SLH_DSA_HASH_SHAKE_256:
      slhdsa_thash_shake(config, output, input, input_blocks, pk_seed, addr);
      return;
  }
  BSSL_CHECK(false);
}

// Implements PRF_msg function (Section 4.1, page 11 and Section 11.2, pages
// 44-46)
static void slhdsa_thash_prfmsg_sha256(const slh_dsa_config *config,
                                       uint8_t *output, const uint8_t *sk_prf,
                                       const uint8_t *entropy,
                                       const uint8_t *header,
                                       const uint8_t *ctx, size_t ctx_len,
                                       const uint8_t *msg, size_t msg_len) {
  BSSL_CHECK(config->hash_type == SLH_DSA_HASH_SHA2_256);
  // Compute HMAC-SHA256(sk_prf, entropy || header || ctx || msg). We inline
  // HMAC to avoid an allocation.
  uint8_t hmac_key[SHA256_CBLOCK];
  BSSL_CHECK(config->n <= sizeof(hmac_key));
  OPENSSL_memcpy(hmac_key, sk_prf, config->n);
  for (size_t i = 0; i < config->n; i++) {
    hmac_key[i] ^= 0x36;
  }
  OPENSSL_memset(hmac_key + config->n, 0x36, sizeof(hmac_key) - config->n);

  SHA256_CTX sha_ctx;
  SHA256_Init(&sha_ctx);
  SHA256_Update(&sha_ctx, hmac_key, sizeof(hmac_key));
  SHA256_Update(&sha_ctx, entropy, config->n);
  if (header) {
    SHA256_Update(&sha_ctx, header, BCM_SLHDSA_M_PRIME_HEADER_LEN);
  }
  if (ctx_len != 0) {
    SHA256_Update(&sha_ctx, ctx, ctx_len);
  }
  if (msg_len != 0) {
    SHA256_Update(&sha_ctx, msg, msg_len);
  }
  uint8_t hash[SHA256_DIGEST_LENGTH];
  SHA256_Final(hash, &sha_ctx);

  for (size_t i = 0; i < config->n; i++) {
    hmac_key[i] ^= 0x36 ^ 0x5c;
  }
  OPENSSL_memset(hmac_key + config->n, 0x5c, sizeof(hmac_key) - config->n);

  SHA256_Init(&sha_ctx);
  SHA256_Update(&sha_ctx, hmac_key, sizeof(hmac_key));
  SHA256_Update(&sha_ctx, hash, sizeof(hash));
  SHA256_Final(hash, &sha_ctx);

  // Truncate to n bytes
  OPENSSL_memcpy(output, hash, config->n);
}

static void slhdsa_thash_prfmsg_shake(const slh_dsa_config *config,
                                      uint8_t *output, const uint8_t *sk_prf,
                                      const uint8_t *entropy,
                                      const uint8_t *header,
                                      const uint8_t *ctx, size_t ctx_len,
                                      const uint8_t *msg, size_t msg_len) {
  BSSL_CHECK(config->hash_type == SLH_DSA_HASH_SHAKE_256);

  struct BORINGSSL_keccak_st keccak;
  BORINGSSL_keccak_init(&keccak, boringssl_shake256);
  BORINGSSL_keccak_absorb(&keccak, sk_prf, config->n);
  BORINGSSL_keccak_absorb(&keccak, entropy, config->n);
  if (header) {
    BORINGSSL_keccak_absorb(&keccak, header, BCM_SLHDSA_M_PRIME_HEADER_LEN);
  }
  if (ctx_len != 0) {
    BORINGSSL_keccak_absorb(&keccak, ctx, ctx_len);
  }
  if (msg_len != 0) {
    BORINGSSL_keccak_absorb(&keccak, msg, msg_len);
  }
  BORINGSSL_keccak_squeeze(&keccak, output, config->n);
}

void bssl::slhdsa_thash_prfmsg(const slh_dsa_config *config, uint8_t *output,
                               const uint8_t *sk_prf, const uint8_t *entropy,
                               const uint8_t *header, const uint8_t *ctx,
                               size_t ctx_len, const uint8_t *msg,
                               size_t msg_len) {
  switch (config->hash_type) {
    case SLH_DSA_HASH_SHA2_256:
      slhdsa_thash_prfmsg_sha256(config, output, sk_prf, entropy, header, ctx,
                                 ctx_len, msg, msg_len);
      return;
    case SLH_DSA_HASH_SHAKE_256:
      slhdsa_thash_prfmsg_shake(config, output, sk_prf, entropy, header, ctx,
                                ctx_len, msg, msg_len);
      return;
  }
  BSSL_CHECK(false);
}

// Implements H_msg function (Section 4.1, page 11 and Section 11.2, pages
// 44-46)
static void slhdsa_thash_hmsg_sha256(const slh_dsa_config *config,
                                     uint8_t *output, const uint8_t *r,
                                     const uint8_t *pk_seed,
                                     const uint8_t *pk_root,
                                     const uint8_t *header,
                                     const uint8_t *ctx, size_t ctx_len,
                                     const uint8_t *msg, size_t msg_len) {
  BSSL_CHECK(config->hash_type == SLH_DSA_HASH_SHA2_256);
  // MGF1-SHA-256(R || PK.seed || SHA-256(R || PK.seed || PK.root || header ||
  // ctx || M), m) input_buffer stores R || PK_SEED || SHA256(..) || 4-byte
  // index
  uint8_t input_buffer[2 * SLHDSA_MAX_N + SLHDSA_MAX_DIGEST_SIZE + 4] = {0};
  OPENSSL_memcpy(input_buffer, r, config->n);
  OPENSSL_memcpy(input_buffer + config->n, pk_seed, config->n);

  // Inner hash
  SHA256_CTX sha_ctx;
  SHA256_Init(&sha_ctx);
  SHA256_Update(&sha_ctx, r, config->n);
  SHA256_Update(&sha_ctx, pk_seed, config->n);
  SHA256_Update(&sha_ctx, pk_root, config->n);
  if (header) {
    SHA256_Update(&sha_ctx, header, BCM_SLHDSA_M_PRIME_HEADER_LEN);
  }
  if (ctx_len != 0) {
    SHA256_Update(&sha_ctx, ctx, ctx_len);
  }
  if (msg_len != 0) {
    SHA256_Update(&sha_ctx, msg, msg_len);
  }
  // Write directly into the input buffer
  SHA256_Final(input_buffer + 2 * config->n, &sha_ctx);

  // MGF1-SHA-256
  uint8_t hash[32];
  BSSL_CHECK(config->digest_size <= sizeof(hash));
  BSSL_CHECK(config->hash_output_bytes == sizeof(hash));
  const size_t mgf_input_len =
      2 * config->n + config->hash_output_bytes + 4;
  SHA256(input_buffer, mgf_input_len, hash);
  OPENSSL_memcpy(output, hash, config->digest_size);
}

static void slhdsa_thash_hmsg_shake(const slh_dsa_config *config,
                                    uint8_t *output, const uint8_t *r,
                                    const uint8_t *pk_seed,
                                    const uint8_t *pk_root,
                                    const uint8_t *header,
                                    const uint8_t *ctx, size_t ctx_len,
                                    const uint8_t *msg, size_t msg_len) {
  BSSL_CHECK(config->hash_type == SLH_DSA_HASH_SHAKE_256);

  struct BORINGSSL_keccak_st keccak;
  BORINGSSL_keccak_init(&keccak, boringssl_shake256);
  BORINGSSL_keccak_absorb(&keccak, r, config->n);
  BORINGSSL_keccak_absorb(&keccak, pk_seed, config->n);
  BORINGSSL_keccak_absorb(&keccak, pk_root, config->n);
  if (header) {
    BORINGSSL_keccak_absorb(&keccak, header, BCM_SLHDSA_M_PRIME_HEADER_LEN);
  }
  if (ctx_len != 0) {
    BORINGSSL_keccak_absorb(&keccak, ctx, ctx_len);
  }
  if (msg_len != 0) {
    BORINGSSL_keccak_absorb(&keccak, msg, msg_len);
  }
  BORINGSSL_keccak_squeeze(&keccak, output, config->digest_size);
}

void bssl::slhdsa_thash_hmsg(const slh_dsa_config *config, uint8_t *output,
                             const uint8_t *r, const uint8_t *pk_seed,
                             const uint8_t *pk_root, const uint8_t *header,
                             const uint8_t *ctx, size_t ctx_len,
                             const uint8_t *msg, size_t msg_len) {
  switch (config->hash_type) {
    case SLH_DSA_HASH_SHA2_256:
      slhdsa_thash_hmsg_sha256(config, output, r, pk_seed, pk_root, header, ctx,
                               ctx_len, msg, msg_len);
      return;
    case SLH_DSA_HASH_SHAKE_256:
      slhdsa_thash_hmsg_shake(config, output, r, pk_seed, pk_root, header, ctx,
                              ctx_len, msg, msg_len);
      return;
  }
  BSSL_CHECK(false);
}

// Implements PRF function (Section 4.1, page 11 and Section 11.2, pages 44-46)
static void slhdsa_thash_prf_sha256(const slh_dsa_config *config,
                                    uint8_t *output, const uint8_t *pk_seed,
                                    const uint8_t *sk_seed, uint8_t addr[32]) {
  slhdsa_thash_sha256(config, output, sk_seed, 1, pk_seed, addr);
}

static void slhdsa_thash_prf_shake(const slh_dsa_config *config,
                                   uint8_t *output, const uint8_t *pk_seed,
                                   const uint8_t *sk_seed, uint8_t addr[32]) {
  BSSL_CHECK(config->hash_type == SLH_DSA_HASH_SHAKE_256);
  BSSL_CHECK(!config->compressed_addresses);

  struct BORINGSSL_keccak_st keccak;
  BORINGSSL_keccak_init(&keccak, boringssl_shake256);
  BORINGSSL_keccak_absorb(&keccak, pk_seed, config->n);
  BORINGSSL_keccak_absorb(&keccak, addr, SLHDSA_ADDR_BYTES);
  BORINGSSL_keccak_absorb(&keccak, sk_seed, config->n);
  BORINGSSL_keccak_squeeze(&keccak, output, config->n);
}

void bssl::slhdsa_thash_prf(const slh_dsa_config *config, uint8_t *output,
                            const uint8_t *pk_seed, const uint8_t *sk_seed,
                            uint8_t addr[32]) {
  switch (config->hash_type) {
    case SLH_DSA_HASH_SHA2_256:
      slhdsa_thash_prf_sha256(config, output, pk_seed, sk_seed, addr);
      return;
    case SLH_DSA_HASH_SHAKE_256:
      slhdsa_thash_prf_shake(config, output, pk_seed, sk_seed, addr);
      return;
  }
  BSSL_CHECK(false);
}

// Implements T_l function for WOTS+ public key compression (Section 4.1, page
// 11 and Section 11.2, pages 44-46)
void bssl::slhdsa_thash_tl(const slh_dsa_config *config, uint8_t *output,
                           const uint8_t *input, const uint8_t *pk_seed,
                           uint8_t addr[32]) {
  slhdsa_thash_dispatch(config, output, input, slhdsa_wots_len(config), pk_seed,
                        addr);
}

// Implements H function (Section 4.1, page 11 and Section 11.2, pages 44-46)
void bssl::slhdsa_thash_h(const slh_dsa_config *config, uint8_t *output,
                          const uint8_t *input, const uint8_t *pk_seed,
                          uint8_t addr[32]) {
  slhdsa_thash_dispatch(config, output, input, 2, pk_seed, addr);
}

// Implements F function (Section 4.1, page 11 and Section 11.2, pages 44-46)
void bssl::slhdsa_thash_f(const slh_dsa_config *config, uint8_t *output,
                          const uint8_t *input, const uint8_t *pk_seed,
                          uint8_t addr[32]) {
  slhdsa_thash_dispatch(config, output, input, 1, pk_seed, addr);
}

// Implements T_k function for FORS public key compression (Section 4.1, page 11
// and Section 11.2, pages 44-46)
void bssl::slhdsa_thash_tk(const slh_dsa_config *config, uint8_t *output,
                           const uint8_t *input, const uint8_t *pk_seed,
                           uint8_t addr[32]) {
  slhdsa_thash_dispatch(config, output, input, config->fors_trees, pk_seed,
                        addr);
}
