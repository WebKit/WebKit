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

#include <string.h>

#include "../../internal.h"
#include "./address.h"
#include "./merkle.h"
#include "./params.h"
#include "./thash.h"
#include "./wots.h"


using namespace bssl;

// Implements Algorithm 9: xmss_node function (page 23)
void bssl::slhdsa_treehash(const slh_dsa_config *config, uint8_t *out_pk,
                           const uint8_t *sk_seed, uint32_t i, uint32_t z,
                           const uint8_t *pk_seed, uint8_t addr[32]) {
  BSSL_CHECK(z <= config->tree_height);
  BSSL_CHECK(i < (uint32_t)(1u << (config->tree_height - z)));

  if (z == 0) {
    slhdsa_set_type(config, addr, SLHDSA_ADDR_TYPE_WOTS);
    slhdsa_set_keypair_addr(config, addr, i);
    slhdsa_wots_pk_gen(config, out_pk, sk_seed, pk_seed, addr);
  } else {
    // Stores left node and right node.
    uint8_t nodes[2 * SLHDSA_MAX_N];
    slhdsa_treehash(config, nodes, sk_seed, 2 * i, z - 1, pk_seed, addr);
    slhdsa_treehash(config, nodes + config->n, sk_seed, 2 * i + 1, z - 1,
                    pk_seed, addr);
    slhdsa_set_type(config, addr, SLHDSA_ADDR_TYPE_HASHTREE);
    slhdsa_set_tree_height(config, addr, z);
    slhdsa_set_tree_index(config, addr, i);
    slhdsa_thash_h(config, out_pk, nodes, pk_seed, addr);
  }
}

// Implements Algorithm 10: xmss_sign function (page 24)
void bssl::slhdsa_xmss_sign(const slh_dsa_config *config, uint8_t *sig,
                            const uint8_t *msg, unsigned int idx,
                            const uint8_t *sk_seed, const uint8_t *pk_seed,
                            uint8_t addr[32]) {
  // Build authentication path
  const size_t wots_bytes = slhdsa_wots_bytes(config);
  const size_t n = config->n;
  for (size_t j = 0; j < config->tree_height; ++j) {
    unsigned int k = (idx >> j) ^ 1;
    slhdsa_treehash(config, sig + wots_bytes + j * n, sk_seed, k, j, pk_seed,
                    addr);
  }

  // Compute WOTS+ signature
  slhdsa_set_type(config, addr, SLHDSA_ADDR_TYPE_WOTS);
  slhdsa_set_keypair_addr(config, addr, idx);
  slhdsa_wots_sign(config, sig, msg, sk_seed, pk_seed, addr);
}

// Implements Algorithm 11: xmss_pkFromSig function (page 25)
void bssl::slhdsa_xmss_pk_from_sig(const slh_dsa_config *config, uint8_t *root,
                                   const uint8_t *xmss_sig, unsigned int idx,
                                   const uint8_t *msg, const uint8_t *pk_seed,
                                   uint8_t addr[32]) {
  // Stores node[0] and node[1] from Algorithm 11
  slhdsa_set_type(config, addr, SLHDSA_ADDR_TYPE_WOTS);
  slhdsa_set_keypair_addr(config, addr, idx);
  uint8_t node[2 * SLHDSA_MAX_N];
  const size_t n = config->n;
  slhdsa_wots_pk_from_sig(config, node, xmss_sig, msg, pk_seed, addr);

  slhdsa_set_type(config, addr, SLHDSA_ADDR_TYPE_HASHTREE);
  slhdsa_set_tree_index(config, addr, idx);

  uint8_t tmp[2 * SLHDSA_MAX_N];
  const uint8_t *const auth = xmss_sig + slhdsa_wots_bytes(config);
  for (uint32_t k = 0; k < config->tree_height; ++k) {
    slhdsa_set_tree_height(config, addr, k + 1);
    if (((idx >> k) & 1) == 0) {
      slhdsa_set_tree_index(config, addr,
                            slhdsa_get_tree_index(config, addr) >> 1);
      OPENSSL_memcpy(tmp, node, n);
      OPENSSL_memcpy(tmp + n, auth + k * n, n);
      slhdsa_thash_h(config, node + n, tmp, pk_seed, addr);
    } else {
      slhdsa_set_tree_index(config, addr,
                            (slhdsa_get_tree_index(config, addr) - 1) >> 1);
      OPENSSL_memcpy(tmp, auth + k * n, n);
      OPENSSL_memcpy(tmp + n, node, n);
      slhdsa_thash_h(config, node + n, tmp, pk_seed, addr);
    }
    OPENSSL_memcpy(node, node + n, n);
  }
  OPENSSL_memcpy(root, node, n);
}

// Implements Algorithm 12: ht_sign function (page 27)
void bssl::slhdsa_ht_sign(const slh_dsa_config *config, uint8_t *sig,
                          const uint8_t *message, uint64_t idx_tree,
                          uint32_t idx_leaf, const uint8_t *sk_seed,
                          const uint8_t *pk_seed) {
  uint8_t addr[32] = {0};
  slhdsa_set_tree_addr(config, addr, idx_tree);

  // Layer 0
  slhdsa_xmss_sign(config, sig, message, idx_leaf, sk_seed, pk_seed, addr);
  uint8_t root[SLHDSA_MAX_N];
  slhdsa_xmss_pk_from_sig(config, root, sig, idx_leaf, message, pk_seed, addr);
  sig += slhdsa_xmss_bytes(config);

  // All other layers
  BSSL_CHECK(config->tree_height <= SLHDSA_MAX_TREE_HEIGHT);
  const uint32_t leaf_mask = (1u << config->tree_height) - 1;
  for (uint32_t j = 1; j < config->d; ++j) {
    idx_leaf = idx_tree & leaf_mask;
    idx_tree = idx_tree >> config->tree_height;
    slhdsa_set_layer_addr(config, addr, j);
    slhdsa_set_tree_addr(config, addr, idx_tree);
    slhdsa_xmss_sign(config, sig, root, idx_leaf, sk_seed, pk_seed, addr);
    if (j < (config->d - 1)) {
      slhdsa_xmss_pk_from_sig(config, root, sig, idx_leaf, root, pk_seed, addr);
    }

    sig += slhdsa_xmss_bytes(config);
  }
}

// Implements Algorithm 13: ht_verify function (page 28)
int bssl::slhdsa_ht_verify(const slh_dsa_config *config, const uint8_t *sig,
                           const uint8_t *message, uint64_t idx_tree,
                           uint32_t idx_leaf, const uint8_t *pk_root,
                           const uint8_t *pk_seed) {
  uint8_t addr[32] = {0};
  slhdsa_set_tree_addr(config, addr, idx_tree);

  uint8_t node[SLHDSA_MAX_N];
  slhdsa_xmss_pk_from_sig(config, node, sig, idx_leaf, message, pk_seed, addr);

  BSSL_CHECK(config->tree_height <= SLHDSA_MAX_TREE_HEIGHT);
  const uint32_t leaf_mask = (1u << config->tree_height) - 1;
  for (uint32_t j = 1; j < config->d; ++j) {
    idx_leaf = idx_tree & leaf_mask;
    idx_tree = idx_tree >> config->tree_height;
    slhdsa_set_layer_addr(config, addr, j);
    slhdsa_set_tree_addr(config, addr, idx_tree);

    slhdsa_xmss_pk_from_sig(config, node, sig + j * slhdsa_xmss_bytes(config),
                            idx_leaf, node, pk_seed, addr);
  }
  return memcmp(node, pk_root, config->n) == 0;
}
