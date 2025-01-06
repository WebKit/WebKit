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

#include <assert.h>
#include <string.h>

#include "../internal.h"
#include "./address.h"
#include "./fors.h"
#include "./params.h"
#include "./thash.h"

// Compute the base 2^12 representation of `message` (algorithm 4, page 16).
static void fors_base_b(
    uint16_t indices[SLHDSA_SHA2_128S_FORS_TREES],
    const uint8_t message[SLHDSA_SHA2_128S_FORS_MSG_BYTES]) {
  static_assert(SLHDSA_SHA2_128S_FORS_HEIGHT == 12, "");
  static_assert((SLHDSA_SHA2_128S_FORS_TREES & 1) == 0, "");

  const uint8_t *msg = message;
  for (size_t i = 0; i < SLHDSA_SHA2_128S_FORS_TREES; i += 2) {
    uint32_t val = ((uint32_t)msg[0] << 16) | ((uint32_t)msg[1] << 8) | msg[2];
    indices[i] = (val >> 12) & 0xFFF;
    indices[i + 1] = val & 0xFFF;
    msg += 3;
  }
}

// Implements Algorithm 14: fors_skGen function (page 29)
void slhdsa_fors_sk_gen(uint8_t fors_sk[SLHDSA_SHA2_128S_N], uint32_t idx,
                        const uint8_t sk_seed[SLHDSA_SHA2_128S_N],
                        const uint8_t pk_seed[SLHDSA_SHA2_128S_N],
                        uint8_t addr[32]) {
  uint8_t sk_addr[32];
  OPENSSL_memcpy(sk_addr, addr, sizeof(sk_addr));

  slhdsa_set_type(sk_addr, SLHDSA_SHA2_128S_ADDR_TYPE_FORSPRF);
  slhdsa_copy_keypair_addr(sk_addr, addr);
  slhdsa_set_tree_index(sk_addr, idx);
  slhdsa_thash_prf(fors_sk, pk_seed, sk_seed, sk_addr);
}

// Implements Algorithm 15: fors_node function (page 30)
void slhdsa_fors_treehash(uint8_t root_node[SLHDSA_SHA2_128S_N],
                          const uint8_t sk_seed[SLHDSA_SHA2_128S_N],
                          uint32_t i /*target node index*/,
                          uint32_t z /*target node height*/,
                          const uint8_t pk_seed[SLHDSA_SHA2_128S_N],
                          uint8_t addr[32]) {
  BSSL_CHECK(z <= SLHDSA_SHA2_128S_FORS_HEIGHT);
  BSSL_CHECK(i < (uint32_t)(SLHDSA_SHA2_128S_FORS_TREES *
                            (1 << (SLHDSA_SHA2_128S_FORS_HEIGHT - z))));

  if (z == 0) {
    uint8_t sk[SLHDSA_SHA2_128S_N];
    slhdsa_set_tree_height(addr, 0);
    slhdsa_set_tree_index(addr, i);
    slhdsa_fors_sk_gen(sk, i, sk_seed, pk_seed, addr);
    slhdsa_thash_f(root_node, sk, pk_seed, addr);
  } else {
    // Stores left node and right node.
    uint8_t nodes[2 * SLHDSA_SHA2_128S_N];
    slhdsa_fors_treehash(nodes, sk_seed, 2 * i, z - 1, pk_seed, addr);
    slhdsa_fors_treehash(nodes + SLHDSA_SHA2_128S_N, sk_seed, 2 * i + 1, z - 1,
                         pk_seed, addr);
    slhdsa_set_tree_height(addr, z);
    slhdsa_set_tree_index(addr, i);
    slhdsa_thash_h(root_node, nodes, pk_seed, addr);
  }
}

// Implements Algorithm 16: fors_sign function (page 31)
void slhdsa_fors_sign(uint8_t fors_sig[SLHDSA_SHA2_128S_FORS_BYTES],
                      const uint8_t message[SLHDSA_SHA2_128S_FORS_MSG_BYTES],
                      const uint8_t sk_seed[SLHDSA_SHA2_128S_N],
                      const uint8_t pk_seed[SLHDSA_SHA2_128S_N],
                      uint8_t addr[32]) {
  uint16_t indices[SLHDSA_SHA2_128S_FORS_TREES];

  // Derive FORS indices compatible with the NIST changes.
  fors_base_b(indices, message);

  for (size_t i = 0; i < SLHDSA_SHA2_128S_FORS_TREES; ++i) {
    slhdsa_set_tree_height(addr, 0);
    // Write the FORS secret key element to the correct position.
    slhdsa_fors_sk_gen(
        fors_sig + i * SLHDSA_SHA2_128S_N * (SLHDSA_SHA2_128S_FORS_HEIGHT + 1),
        i * (1 << SLHDSA_SHA2_128S_FORS_HEIGHT) + indices[i], sk_seed, pk_seed,
        addr);
    for (size_t j = 0; j < SLHDSA_SHA2_128S_FORS_HEIGHT; ++j) {
      size_t s = (indices[i] / (1 << j)) ^ 1;
      // Write the FORS auth path element to the correct position.
      slhdsa_fors_treehash(
          fors_sig + SLHDSA_SHA2_128S_N *
                         (i * (SLHDSA_SHA2_128S_FORS_HEIGHT + 1) + j + 1),
          sk_seed, i * (1ULL << (SLHDSA_SHA2_128S_FORS_HEIGHT - j)) + s, j,
          pk_seed, addr);
    }
  }
}

// Implements Algorithm 17: fors_pkFromSig function (page 32)
void slhdsa_fors_pk_from_sig(
    uint8_t fors_pk[SLHDSA_SHA2_128S_N],
    const uint8_t fors_sig[SLHDSA_SHA2_128S_FORS_BYTES],
    const uint8_t message[SLHDSA_SHA2_128S_FORS_MSG_BYTES],
    const uint8_t pk_seed[SLHDSA_SHA2_128S_N], uint8_t addr[32]) {
  uint16_t indices[SLHDSA_SHA2_128S_FORS_TREES];
  uint8_t tmp[2 * SLHDSA_SHA2_128S_N];
  uint8_t roots[SLHDSA_SHA2_128S_FORS_TREES * SLHDSA_SHA2_128S_N];

  // Derive FORS indices compatible with the NIST changes.
  fors_base_b(indices, message);

  for (size_t i = 0; i < SLHDSA_SHA2_128S_FORS_TREES; ++i) {
    // Pointer to current sk and authentication path
    const uint8_t *sk =
        fors_sig + i * SLHDSA_SHA2_128S_N * (SLHDSA_SHA2_128S_FORS_HEIGHT + 1);
    const uint8_t *auth =
        fors_sig + i * SLHDSA_SHA2_128S_N * (SLHDSA_SHA2_128S_FORS_HEIGHT + 1) +
        SLHDSA_SHA2_128S_N;
    uint8_t nodes[2 * SLHDSA_SHA2_128S_N];

    slhdsa_set_tree_height(addr, 0);
    slhdsa_set_tree_index(
        addr, (i * (1 << SLHDSA_SHA2_128S_FORS_HEIGHT)) + indices[i]);

    slhdsa_thash_f(nodes, sk, pk_seed, addr);

    for (size_t j = 0; j < SLHDSA_SHA2_128S_FORS_HEIGHT; ++j) {
      slhdsa_set_tree_height(addr, j + 1);

      // Even node
      if (((indices[i] / (1 << j)) % 2) == 0) {
        slhdsa_set_tree_index(addr, slhdsa_get_tree_index(addr) / 2);
        OPENSSL_memcpy(tmp, nodes, SLHDSA_SHA2_128S_N);
        OPENSSL_memcpy(tmp + SLHDSA_SHA2_128S_N, auth + j * SLHDSA_SHA2_128S_N,
                       SLHDSA_SHA2_128S_N);
        slhdsa_thash_h(nodes + SLHDSA_SHA2_128S_N, tmp, pk_seed, addr);
      } else {
        slhdsa_set_tree_index(addr, (slhdsa_get_tree_index(addr) - 1) / 2);
        OPENSSL_memcpy(tmp, auth + j * SLHDSA_SHA2_128S_N, SLHDSA_SHA2_128S_N);
        OPENSSL_memcpy(tmp + SLHDSA_SHA2_128S_N, nodes, SLHDSA_SHA2_128S_N);
        slhdsa_thash_h(nodes + SLHDSA_SHA2_128S_N, tmp, pk_seed, addr);
      }
      OPENSSL_memcpy(nodes, nodes + SLHDSA_SHA2_128S_N, SLHDSA_SHA2_128S_N);
    }
    OPENSSL_memcpy(roots + i * SLHDSA_SHA2_128S_N, nodes, SLHDSA_SHA2_128S_N);
  }

  uint8_t forspk_addr[32];
  OPENSSL_memcpy(forspk_addr, addr, sizeof(forspk_addr));
  slhdsa_set_type(forspk_addr, SLHDSA_SHA2_128S_ADDR_TYPE_FORSPK);
  slhdsa_copy_keypair_addr(forspk_addr, addr);
  slhdsa_thash_tk(fors_pk, roots, pk_seed, forspk_addr);
}
