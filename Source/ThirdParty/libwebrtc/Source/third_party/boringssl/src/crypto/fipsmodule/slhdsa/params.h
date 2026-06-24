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

#ifndef OPENSSL_HEADER_CRYPTO_FIPSMODULE_SLHDSA_PARAMS_H
#define OPENSSL_HEADER_CRYPTO_FIPSMODULE_SLHDSA_PARAMS_H

#include <openssl/base.h>

#include <stdbool.h>

#include "../bcm_interface.h"


BSSL_NAMESPACE_BEGIN

enum slh_dsa_hash_type {
  SLH_DSA_HASH_SHA2_256,
  SLH_DSA_HASH_SHAKE_256,
};

// Upper bounds for stack allocations across all SLH-DSA parameter sets in
// FIPS 205. These keep the code simple and avoid dynamic allocation while
// still covering larger future parameter sets such as SLH-DSA-SHAKE-256f.
#define SLHDSA_MAX_N 32
#define SLHDSA_MAX_WOTS_LEN 67
#define SLHDSA_MAX_WOTS_BYTES (SLHDSA_MAX_N * SLHDSA_MAX_WOTS_LEN)
#define SLHDSA_MAX_FORS_HEIGHT 17
#define SLHDSA_MAX_FORS_TREES 35
#define SLHDSA_MAX_FORS_BYTES \
  ((SLHDSA_MAX_FORS_HEIGHT + 1) * SLHDSA_MAX_FORS_TREES * SLHDSA_MAX_N)
#define SLHDSA_MAX_FORS_MSG_BYTES \
  ((SLHDSA_MAX_FORS_HEIGHT * SLHDSA_MAX_FORS_TREES + 7) / 8)
#define SLHDSA_MAX_TREE_HEIGHT 18
#define SLHDSA_MAX_D 18
#define SLHDSA_MAX_DIGEST_SIZE 64
#define SLHDSA_MAX_HASH_BLOCK_BYTES 168

// Values bound by these limits are assumed to be valid shifts within a
// uint32_t.
static_assert(SLHDSA_MAX_TREE_HEIGHT < 32);
static_assert(SLHDSA_MAX_FORS_HEIGHT < 32);

#define SLHDSA_ADDR_BYTES 32
#define SLHDSA_ADDR_COMPRESSED_BYTES 22
#define SLHDSA_ADDR_COMP_OFFSET_LAYER 0
#define SLHDSA_ADDR_COMP_OFFSET_TREE 1
#define SLHDSA_ADDR_COMP_OFFSET_TYPE 9
#define SLHDSA_ADDR_COMP_OFFSET_KEYPAIR 10
#define SLHDSA_ADDR_COMP_OFFSET_CHAIN 14
#define SLHDSA_ADDR_COMP_OFFSET_TREE_HEIGHT 14
#define SLHDSA_ADDR_COMP_OFFSET_HASH 18
#define SLHDSA_ADDR_COMP_OFFSET_TREE_INDEX 18
#define SLHDSA_ADDR_COMP_ZERO_START 10
#define SLHDSA_ADDR_COMP_ZERO_LEN 12
#define SLHDSA_ADDR_FULL_OFFSET_LAYER 0
#define SLHDSA_ADDR_FULL_OFFSET_TREE 4
#define SLHDSA_ADDR_FULL_OFFSET_TYPE 16
#define SLHDSA_ADDR_FULL_OFFSET_KEYPAIR 20
#define SLHDSA_ADDR_FULL_OFFSET_CHAIN 24
#define SLHDSA_ADDR_FULL_OFFSET_TREE_HEIGHT 24
#define SLHDSA_ADDR_FULL_OFFSET_HASH 28
#define SLHDSA_ADDR_FULL_OFFSET_TREE_INDEX 28
#define SLHDSA_ADDR_FULL_ZERO_START 20
#define SLHDSA_ADDR_FULL_ZERO_LEN 12

typedef struct slh_dsa_config {
  uint32_t n;
  uint32_t full_height;
  uint32_t d;
  uint32_t tree_height;
  uint32_t fors_height;
  uint32_t fors_trees;
  uint32_t wots_w;
  uint32_t wots_log_w;
  uint32_t wots_len1;
  uint32_t wots_len2;
  uint32_t digest_size;
  uint32_t hash_block_bytes;
  uint32_t hash_output_bytes;
  uint32_t public_key_bytes;
  uint32_t private_key_bytes;
  uint32_t signature_bytes;
  enum slh_dsa_hash_type hash_type;
  bool compressed_addresses;
} slh_dsa_config;

inline uint32_t slhdsa_wots_len(const slh_dsa_config *config) {
  return config->wots_len1 + config->wots_len2;
}

inline uint32_t slhdsa_wots_bytes(const slh_dsa_config *config) {
  return config->n * slhdsa_wots_len(config);
}

inline uint32_t slhdsa_xmss_bytes(const slh_dsa_config *config) {
  return slhdsa_wots_bytes(config) + config->n * config->tree_height;
}

inline uint32_t slhdsa_fors_msg_bytes(const slh_dsa_config *config) {
  return (config->fors_height * config->fors_trees + 7) / 8;
}

inline uint32_t slhdsa_fors_bytes(const slh_dsa_config *config) {
  return (config->fors_height + 1) * config->fors_trees * config->n;
}

inline uint32_t slhdsa_tree_bits(const slh_dsa_config *config) {
  return config->tree_height * (config->d - 1);
}

inline uint32_t slhdsa_tree_bytes(const slh_dsa_config *config) {
  return (slhdsa_tree_bits(config) + 7) / 8;
}

inline uint32_t slhdsa_leaf_bits(const slh_dsa_config *config) {
  return config->tree_height;
}

inline uint32_t slhdsa_leaf_bytes(const slh_dsa_config *config) {
  return (slhdsa_leaf_bits(config) + 7) / 8;
}

static const slh_dsa_config kSLHDSAConfigSHA2_128s = {
    /*n=*/BCM_SLHDSA_SHA2_128S_N,
    /*full_height=*/63,
    /*d=*/7,
    /*tree_height=*/9,
    /*fors_height=*/12,
    /*fors_trees=*/14,
    /*wots_w=*/16,
    /*wots_log_w=*/4,
    /*wots_len1=*/32,
    /*wots_len2=*/3,
    /*digest_size=*/30,
    /*hash_block_bytes=*/64,
    /*hash_output_bytes=*/32,
    /*public_key_bytes=*/BCM_SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES,
    /*private_key_bytes=*/BCM_SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES,
    /*signature_bytes=*/BCM_SLHDSA_SHA2_128S_SIGNATURE_BYTES,
    /*hash_type=*/SLH_DSA_HASH_SHA2_256,
    /*compressed_addresses=*/true,
};

static const slh_dsa_config kSLHDSAConfigSHAKE_256f = {
    /*n=*/BCM_SLHDSA_SHAKE_256F_N,
    /*full_height=*/68,
    /*d=*/17,
    /*tree_height=*/4,
    /*fors_height=*/9,
    /*fors_trees=*/35,
    /*wots_w=*/16,
    /*wots_log_w=*/4,
    /*wots_len1=*/64,
    /*wots_len2=*/3,
    /*digest_size=*/49,
    /*hash_block_bytes=*/136,
    /*hash_output_bytes=*/32,
    /*public_key_bytes=*/BCM_SLHDSA_SHAKE_256F_PUBLIC_KEY_BYTES,
    /*private_key_bytes=*/BCM_SLHDSA_SHAKE_256F_PRIVATE_KEY_BYTES,
    /*signature_bytes=*/BCM_SLHDSA_SHAKE_256F_SIGNATURE_BYTES,
    /*hash_type=*/SLH_DSA_HASH_SHAKE_256,
    /*compressed_addresses=*/false,
};

BSSL_NAMESPACE_END

#endif  // OPENSSL_HEADER_CRYPTO_FIPSMODULE_SLHDSA_PARAMS_H
