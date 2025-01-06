/* Copyright (c) 2024, Google LLC
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

#include <openssl/base.h>

#include <string.h>

#include "../internal.h"
#include "./address.h"
#include "./merkle.h"
#include "./params.h"
#include "./thash.h"
#include "./wots.h"


// Implements Algorithm 9: xmss_node function (page 23)
void slhdsa_treehash(uint8_t out_pk[SLHDSA_SHA2_128S_N],
                     const uint8_t sk_seed[SLHDSA_SHA2_128S_N],
                     uint32_t i /*target node index*/,
                     uint32_t z /*target node height*/,
                     const uint8_t pk_seed[SLHDSA_SHA2_128S_N],
                     uint8_t addr[32]) {
  BSSL_CHECK(z <= SLHDSA_SHA2_128S_TREE_HEIGHT);
  BSSL_CHECK(i < (uint32_t)(1 << (SLHDSA_SHA2_128S_TREE_HEIGHT - z)));

  if (z == 0) {
    slhdsa_set_type(addr, SLHDSA_SHA2_128S_ADDR_TYPE_WOTS);
    slhdsa_set_keypair_addr(addr, i);
    slhdsa_wots_pk_gen(out_pk, sk_seed, pk_seed, addr);
  } else {
    // Stores left node and right node.
    uint8_t nodes[2 * SLHDSA_SHA2_128S_N];
    slhdsa_treehash(nodes, sk_seed, 2 * i, z - 1, pk_seed, addr);
    slhdsa_treehash(nodes + SLHDSA_SHA2_128S_N, sk_seed, 2 * i + 1, z - 1,
                    pk_seed, addr);
    slhdsa_set_type(addr, SLHDSA_SHA2_128S_ADDR_TYPE_HASHTREE);
    slhdsa_set_tree_height(addr, z);
    slhdsa_set_tree_index(addr, i);
    slhdsa_thash_h(out_pk, nodes, pk_seed, addr);
  }
}

// Implements Algorithm 10: xmss_sign function (page 24)
void slhdsa_xmss_sign(uint8_t sig[SLHDSA_SHA2_128S_XMSS_BYTES],
                      const uint8_t msg[SLHDSA_SHA2_128S_N], unsigned int idx,
                      const uint8_t sk_seed[SLHDSA_SHA2_128S_N],
                      const uint8_t pk_seed[SLHDSA_SHA2_128S_N],
                      uint8_t addr[32]) {
  // Build authentication path
  for (size_t j = 0; j < SLHDSA_SHA2_128S_TREE_HEIGHT; ++j) {
    unsigned int k = (idx >> j) ^ 1;
    slhdsa_treehash(sig + SLHDSA_SHA2_128S_WOTS_BYTES + j * SLHDSA_SHA2_128S_N,
                    sk_seed, k, j, pk_seed, addr);
  }

  // Compute WOTS+ signature
  slhdsa_set_type(addr, SLHDSA_SHA2_128S_ADDR_TYPE_WOTS);
  slhdsa_set_keypair_addr(addr, idx);
  slhdsa_wots_sign(sig, msg, sk_seed, pk_seed, addr);
}

// Implements Algorithm 11: xmss_pkFromSig function (page 25)
void slhdsa_xmss_pk_from_sig(
    uint8_t root[SLHDSA_SHA2_128S_N],
    const uint8_t xmss_sig[SLHDSA_SHA2_128S_XMSS_BYTES], unsigned int idx,
    const uint8_t msg[SLHDSA_SHA2_128S_N],
    const uint8_t pk_seed[SLHDSA_SHA2_128S_N], uint8_t addr[32]) {
  // Stores node[0] and node[1] from Algorithm 11
  slhdsa_set_type(addr, SLHDSA_SHA2_128S_ADDR_TYPE_WOTS);
  slhdsa_set_keypair_addr(addr, idx);
  uint8_t node[2 * SLHDSA_SHA2_128S_N];
  slhdsa_wots_pk_from_sig(node, xmss_sig, msg, pk_seed, addr);

  slhdsa_set_type(addr, SLHDSA_SHA2_128S_ADDR_TYPE_HASHTREE);
  slhdsa_set_tree_index(addr, idx);

  uint8_t tmp[2 * SLHDSA_SHA2_128S_N];
  const uint8_t *const auth = xmss_sig + SLHDSA_SHA2_128S_WOTS_BYTES;
  for (size_t k = 0; k < SLHDSA_SHA2_128S_TREE_HEIGHT; ++k) {
    slhdsa_set_tree_height(addr, k + 1);
    if (((idx >> k) & 1) == 0) {
      slhdsa_set_tree_index(addr, slhdsa_get_tree_index(addr) >> 1);
      OPENSSL_memcpy(tmp, node, SLHDSA_SHA2_128S_N);
      OPENSSL_memcpy(tmp + SLHDSA_SHA2_128S_N, auth + k * SLHDSA_SHA2_128S_N,
                     SLHDSA_SHA2_128S_N);
      slhdsa_thash_h(node + SLHDSA_SHA2_128S_N, tmp, pk_seed, addr);
    } else {
      slhdsa_set_tree_index(addr, (slhdsa_get_tree_index(addr) - 1) >> 1);
      OPENSSL_memcpy(tmp, auth + k * SLHDSA_SHA2_128S_N, SLHDSA_SHA2_128S_N);
      OPENSSL_memcpy(tmp + SLHDSA_SHA2_128S_N, node, SLHDSA_SHA2_128S_N);
      slhdsa_thash_h(node + SLHDSA_SHA2_128S_N, tmp, pk_seed, addr);
    }
    OPENSSL_memcpy(node, node + SLHDSA_SHA2_128S_N, SLHDSA_SHA2_128S_N);
  }
  OPENSSL_memcpy(root, node, SLHDSA_SHA2_128S_N);
}

// Implements Algorithm 12: ht_sign function (page 27)
void slhdsa_ht_sign(
    uint8_t sig[SLHDSA_SHA2_128S_XMSS_BYTES * SLHDSA_SHA2_128S_D],
    const uint8_t message[SLHDSA_SHA2_128S_N], uint64_t idx_tree,
    uint32_t idx_leaf, const uint8_t sk_seed[SLHDSA_SHA2_128S_N],
    const uint8_t pk_seed[SLHDSA_SHA2_128S_N]) {
  uint8_t addr[32] = {0};
  slhdsa_set_tree_addr(addr, idx_tree);

  // Layer 0
  slhdsa_xmss_sign(sig, message, idx_leaf, sk_seed, pk_seed, addr);
  uint8_t root[SLHDSA_SHA2_128S_N];
  slhdsa_xmss_pk_from_sig(root, sig, idx_leaf, message, pk_seed, addr);
  sig += SLHDSA_SHA2_128S_XMSS_BYTES;

  // All other layers
  for (size_t j = 1; j < SLHDSA_SHA2_128S_D; ++j) {
    idx_leaf = idx_tree % (1 << SLHDSA_SHA2_128S_TREE_HEIGHT);
    idx_tree = idx_tree >> SLHDSA_SHA2_128S_TREE_HEIGHT;
    slhdsa_set_layer_addr(addr, j);
    slhdsa_set_tree_addr(addr, idx_tree);
    slhdsa_xmss_sign(sig, root, idx_leaf, sk_seed, pk_seed, addr);
    if (j < (SLHDSA_SHA2_128S_D - 1)) {
      slhdsa_xmss_pk_from_sig(root, sig, idx_leaf, root, pk_seed, addr);
    }

    sig += SLHDSA_SHA2_128S_XMSS_BYTES;
  }
}

// Implements Algorithm 13: ht_verify function (page 28)
int slhdsa_ht_verify(
    const uint8_t sig[SLHDSA_SHA2_128S_D * SLHDSA_SHA2_128S_XMSS_BYTES],
    const uint8_t message[SLHDSA_SHA2_128S_N], uint64_t idx_tree,
    uint32_t idx_leaf, const uint8_t pk_root[SLHDSA_SHA2_128S_N],
    const uint8_t pk_seed[SLHDSA_SHA2_128S_N]) {
  uint8_t addr[32] = {0};
  slhdsa_set_tree_addr(addr, idx_tree);

  uint8_t node[SLHDSA_SHA2_128S_N];
  slhdsa_xmss_pk_from_sig(node, sig, idx_leaf, message, pk_seed, addr);

  for (size_t j = 1; j < SLHDSA_SHA2_128S_D; ++j) {
    idx_leaf = idx_tree % (1 << SLHDSA_SHA2_128S_TREE_HEIGHT);
    idx_tree = idx_tree >> SLHDSA_SHA2_128S_TREE_HEIGHT;
    slhdsa_set_layer_addr(addr, j);
    slhdsa_set_tree_addr(addr, idx_tree);

    slhdsa_xmss_pk_from_sig(node, sig + j * SLHDSA_SHA2_128S_XMSS_BYTES,
                            idx_leaf, node, pk_seed, addr);
  }
  return memcmp(node, pk_root, SLHDSA_SHA2_128S_N) == 0;
}
