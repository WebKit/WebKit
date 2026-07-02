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

#ifndef OPENSSL_HEADER_CRYPTO_FIPSMODULE_SLHDSA_ADDRESS_H
#define OPENSSL_HEADER_CRYPTO_FIPSMODULE_SLHDSA_ADDRESS_H

#include <openssl/mem.h>

#include "../../internal.h"
#include "./params.h"


BSSL_NAMESPACE_BEGIN

#define SLHDSA_ADDR_TYPE_WOTS 0
#define SLHDSA_ADDR_TYPE_WOTSPK 1
#define SLHDSA_ADDR_TYPE_HASHTREE 2
#define SLHDSA_ADDR_TYPE_FORSTREE 3
#define SLHDSA_ADDR_TYPE_FORSPK 4
#define SLHDSA_ADDR_TYPE_WOTSPRF 5
#define SLHDSA_ADDR_TYPE_FORSPRF 6

inline void slhdsa_set_chain_addr(const slh_dsa_config *config,
                                  uint8_t addr[32], uint32_t chain) {
  if (config->compressed_addresses) {
    bssl::CRYPTO_store_u32_be(addr + SLHDSA_ADDR_COMP_OFFSET_CHAIN, chain);
  } else {
    bssl::CRYPTO_store_u32_be(addr + SLHDSA_ADDR_FULL_OFFSET_CHAIN, chain);
  }
}

inline void slhdsa_set_hash_addr(const slh_dsa_config *config, uint8_t addr[32],
                                 uint32_t hash) {
  if (config->compressed_addresses) {
    bssl::CRYPTO_store_u32_be(addr + SLHDSA_ADDR_COMP_OFFSET_HASH, hash);
  } else {
    bssl::CRYPTO_store_u32_be(addr + SLHDSA_ADDR_FULL_OFFSET_HASH, hash);
  }
}

inline void slhdsa_set_keypair_addr(const slh_dsa_config *config,
                                    uint8_t addr[32], uint32_t keypair) {
  if (config->compressed_addresses) {
    bssl::CRYPTO_store_u32_be(addr + SLHDSA_ADDR_COMP_OFFSET_KEYPAIR, keypair);
  } else {
    bssl::CRYPTO_store_u32_be(addr + SLHDSA_ADDR_FULL_OFFSET_KEYPAIR, keypair);
  }
}

inline void slhdsa_copy_keypair_addr(const slh_dsa_config *config,
                                     uint8_t out[32], const uint8_t in[32]) {
  if (config->compressed_addresses) {
    bssl::OPENSSL_memcpy(out, in, SLHDSA_ADDR_COMP_OFFSET_TYPE);
    bssl::OPENSSL_memcpy(out + SLHDSA_ADDR_COMP_OFFSET_KEYPAIR,
                         in + SLHDSA_ADDR_COMP_OFFSET_KEYPAIR, 4);
  } else {
    bssl::OPENSSL_memcpy(out, in, SLHDSA_ADDR_FULL_OFFSET_TYPE);
    bssl::OPENSSL_memcpy(out + SLHDSA_ADDR_FULL_OFFSET_KEYPAIR,
                         in + SLHDSA_ADDR_FULL_OFFSET_KEYPAIR, 4);
  }
}

inline void slhdsa_set_layer_addr(const slh_dsa_config *config,
                                  uint8_t addr[32], uint32_t layer) {
  if (config->compressed_addresses) {
    addr[SLHDSA_ADDR_COMP_OFFSET_LAYER] = (uint8_t)layer;
  } else {
    bssl::CRYPTO_store_u32_be(addr + SLHDSA_ADDR_FULL_OFFSET_LAYER, layer);
  }
}

inline void slhdsa_set_tree_addr(const slh_dsa_config *config, uint8_t addr[32],
                                 uint64_t tree) {
  if (config->compressed_addresses) {
    bssl::CRYPTO_store_u64_be(addr + SLHDSA_ADDR_COMP_OFFSET_TREE, tree);
  } else {
    // The tree address is 12 bytes in this configuration. Just zero the top
    // four bytes.
    bssl::OPENSSL_memset(addr + SLHDSA_ADDR_FULL_OFFSET_TREE, 0, 4);
    bssl::CRYPTO_store_u64_be(addr + SLHDSA_ADDR_FULL_OFFSET_TREE + 4, tree);
  }
}

inline void slhdsa_set_type(const slh_dsa_config *config, uint8_t addr[32],
                            uint32_t type) {
  // FIPS 205 relies on this setting parts of the address to 0, so we do it
  // here to avoid confusion.
  //
  // The behavior here is only correct for the SHA-2 instantiations.
  if (config->compressed_addresses) {
    bssl::OPENSSL_memset(addr + SLHDSA_ADDR_COMP_ZERO_START, 0,
                         SLHDSA_ADDR_COMP_ZERO_LEN);
    addr[SLHDSA_ADDR_COMP_OFFSET_TYPE] = (uint8_t)type;
  } else {
    bssl::OPENSSL_memset(addr + SLHDSA_ADDR_FULL_ZERO_START, 0,
                         SLHDSA_ADDR_FULL_ZERO_LEN);
    bssl::CRYPTO_store_u32_be(addr + SLHDSA_ADDR_FULL_OFFSET_TYPE, type);
  }
}

inline void slhdsa_set_tree_height(const slh_dsa_config *config,
                                   uint8_t addr[32], uint32_t tree_height) {
  if (config->compressed_addresses) {
    bssl::CRYPTO_store_u32_be(addr + SLHDSA_ADDR_COMP_OFFSET_TREE_HEIGHT,
                              tree_height);
  } else {
    bssl::CRYPTO_store_u32_be(addr + SLHDSA_ADDR_FULL_OFFSET_TREE_HEIGHT,
                              tree_height);
  }
}

inline void slhdsa_set_tree_index(const slh_dsa_config *config,
                                  uint8_t addr[32], uint32_t tree_index) {
  if (config->compressed_addresses) {
    bssl::CRYPTO_store_u32_be(addr + SLHDSA_ADDR_COMP_OFFSET_TREE_INDEX,
                              tree_index);
  } else {
    bssl::CRYPTO_store_u32_be(addr + SLHDSA_ADDR_FULL_OFFSET_TREE_INDEX,
                              tree_index);
  }
}

inline uint32_t slhdsa_get_tree_index(const slh_dsa_config *config,
                                      uint8_t addr[32]) {
  if (config->compressed_addresses) {
    return bssl::CRYPTO_load_u32_be(addr + SLHDSA_ADDR_COMP_OFFSET_TREE_INDEX);
  }
  return bssl::CRYPTO_load_u32_be(addr + SLHDSA_ADDR_FULL_OFFSET_TREE_INDEX);
}

BSSL_NAMESPACE_END

#endif  // OPENSSL_HEADER_CRYPTO_FIPSMODULE_SLHDSA_ADDRESS_H
