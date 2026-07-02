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

#include "../../internal.h"
#include "./address.h"
#include "./fors.h"
#include "./params.h"
#include "./thash.h"


using namespace bssl;

// Compute the base 2^a representation of `message` (algorithm 4, page 16).
static void fors_base_b(const slh_dsa_config *config, uint32_t *indices,
                        const uint8_t *message) {
  const size_t bits_per_index = config->fors_height;
  BSSL_CHECK(bits_per_index <= SLHDSA_MAX_FORS_HEIGHT);
  const size_t total_bits = bits_per_index * config->fors_trees;
  const size_t msg_bytes = slhdsa_fors_msg_bytes(config);
  BSSL_CHECK(total_bits <= msg_bytes * 8);

  size_t bit_offset = 0;
  for (size_t i = 0; i < config->fors_trees; ++i) {
    uint32_t value = 0;
    for (size_t j = 0; j < bits_per_index; ++j) {
      const size_t bit_index = bit_offset + j;
      const size_t byte_index = bit_index >> 3;
      const size_t bit_in_byte = 7 - (bit_index & 7);
      const uint8_t bit = (message[byte_index] >> bit_in_byte) & 0x01;
      value = (value << 1) | bit;
    }
    indices[i] = value;
    bit_offset += bits_per_index;
  }
}

// Implements Algorithm 14: fors_skGen function (page 29)
void bssl::slhdsa_fors_sk_gen(const slh_dsa_config *config, uint8_t *fors_sk,
                              uint32_t idx, const uint8_t *sk_seed,
                              const uint8_t *pk_seed, uint8_t addr[32]) {
  uint8_t sk_addr[32];
  OPENSSL_memcpy(sk_addr, addr, sizeof(sk_addr));

  slhdsa_set_type(config, sk_addr, SLHDSA_ADDR_TYPE_FORSPRF);
  slhdsa_copy_keypair_addr(config, sk_addr, addr);
  slhdsa_set_tree_index(config, sk_addr, idx);
  slhdsa_thash_prf(config, fors_sk, pk_seed, sk_seed, sk_addr);
}

// Implements Algorithm 15: fors_node function (page 30)
void bssl::slhdsa_fors_treehash(const slh_dsa_config *config,
                                uint8_t *root_node, const uint8_t *sk_seed,
                                uint32_t i /*target node index*/,
                                uint32_t z /*target node height*/,
                                const uint8_t *pk_seed, uint8_t addr[32]) {
  BSSL_CHECK(z <= config->fors_height);
  const uint32_t nodes_in_tree = 1u << (config->fors_height - z);
  BSSL_CHECK(i < (uint32_t)(config->fors_trees * nodes_in_tree));

  if (z == 0) {
    uint8_t sk[SLHDSA_MAX_N];
    slhdsa_set_tree_height(config, addr, 0);
    slhdsa_set_tree_index(config, addr, i);
    slhdsa_fors_sk_gen(config, sk, i, sk_seed, pk_seed, addr);
    slhdsa_thash_f(config, root_node, sk, pk_seed, addr);
  } else {
    // Stores left node and right node.
    uint8_t nodes[2 * SLHDSA_MAX_N];
    slhdsa_fors_treehash(config, nodes, sk_seed, 2 * i, z - 1, pk_seed, addr);
    slhdsa_fors_treehash(config, nodes + config->n, sk_seed, 2 * i + 1, z - 1,
                         pk_seed, addr);
    slhdsa_set_tree_height(config, addr, z);
    slhdsa_set_tree_index(config, addr, i);
    slhdsa_thash_h(config, root_node, nodes, pk_seed, addr);
  }
}

// Implements Algorithm 16: fors_sign function (page 31)
void bssl::slhdsa_fors_sign(const slh_dsa_config *config, uint8_t *fors_sig,
                            const uint8_t *message, const uint8_t *sk_seed,
                            const uint8_t *pk_seed, uint8_t addr[32]) {
  uint32_t indices[SLHDSA_MAX_FORS_TREES];
  BSSL_CHECK(config->fors_trees <= SLHDSA_MAX_FORS_TREES);
  BSSL_CHECK(config->fors_height <= SLHDSA_MAX_FORS_HEIGHT);

  // Derive FORS indices compatible with the NIST changes.
  fors_base_b(config, indices, message);

  const size_t n = config->n;
  const size_t node_stride = n * (config->fors_height + 1);

  for (uint32_t i = 0; i < config->fors_trees; ++i) {
    slhdsa_set_tree_height(config, addr, 0);
    // Write the FORS secret key element to the correct position.
    uint8_t *tree_sig = fors_sig + i * node_stride;
    slhdsa_fors_sk_gen(config, tree_sig,
                       i * (1u << config->fors_height) + indices[i], sk_seed,
                       pk_seed, addr);
    for (uint32_t j = 0; j < config->fors_height; ++j) {
      const size_t sibling = (indices[i] >> j) ^ 1u;
      // Write the FORS auth path element to the correct position.
      slhdsa_fors_treehash(config, tree_sig + (j + 1) * n, sk_seed,
                           i * (1u << (config->fors_height - j)) + sibling, j,
                           pk_seed, addr);
    }
  }
}

// Implements Algorithm 17: fors_pkFromSig function (page 32)
void bssl::slhdsa_fors_pk_from_sig(const slh_dsa_config *config,
                                   uint8_t *fors_pk, const uint8_t *fors_sig,
                                   const uint8_t *message,
                                   const uint8_t *pk_seed, uint8_t addr[32]) {
  uint32_t indices[SLHDSA_MAX_FORS_TREES];
  uint8_t tmp[2 * SLHDSA_MAX_N];
  uint8_t roots[SLHDSA_MAX_FORS_TREES * SLHDSA_MAX_N];
  BSSL_CHECK(config->fors_trees <= SLHDSA_MAX_FORS_TREES);
  BSSL_CHECK(config->fors_height <= SLHDSA_MAX_FORS_HEIGHT);

  // Derive FORS indices compatible with the NIST changes.
  fors_base_b(config, indices, message);

  const size_t n = config->n;
  const size_t node_stride = n * (config->fors_height + 1);

  for (size_t i = 0; i < config->fors_trees; ++i) {
    // Pointer to current sk and authentication path
    const uint8_t *tree_sig = fors_sig + i * node_stride;
    const uint8_t *auth = tree_sig + n;
    uint8_t nodes[2 * SLHDSA_MAX_N];

    slhdsa_set_tree_height(config, addr, 0);
    uint32_t index = i * (1ULL << config->fors_height) + indices[i];
    slhdsa_set_tree_index(config, addr, index);

    slhdsa_thash_f(config, nodes, tree_sig, pk_seed, addr);

    for (uint32_t j = 0; j < config->fors_height; ++j) {
      slhdsa_set_tree_height(config, addr, j + 1);
      index >>= 1;
      slhdsa_set_tree_index(config, addr, index);

      // Even node
      if (((indices[i] >> j) & 1) == 0) {
        OPENSSL_memcpy(tmp, nodes, n);
        OPENSSL_memcpy(tmp + n, auth + j * n, n);
        slhdsa_thash_h(config, nodes + n, tmp, pk_seed, addr);
      } else {
        OPENSSL_memcpy(tmp, auth + j * n, n);
        OPENSSL_memcpy(tmp + n, nodes, n);
        slhdsa_thash_h(config, nodes + n, tmp, pk_seed, addr);
      }
      OPENSSL_memcpy(nodes, nodes + n, n);
    }
    OPENSSL_memcpy(roots + i * n, nodes, n);
  }

  uint8_t forspk_addr[32];
  OPENSSL_memcpy(forspk_addr, addr, sizeof(forspk_addr));
  slhdsa_set_type(config, forspk_addr, SLHDSA_ADDR_TYPE_FORSPK);
  slhdsa_copy_keypair_addr(config, forspk_addr, addr);
  slhdsa_thash_tk(config, fors_pk, roots, pk_seed, forspk_addr);
}
