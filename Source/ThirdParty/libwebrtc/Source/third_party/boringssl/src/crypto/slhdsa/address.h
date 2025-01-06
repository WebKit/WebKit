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

#ifndef OPENSSL_HEADER_CRYPTO_SLHDSA_ADDRESS_H
#define OPENSSL_HEADER_CRYPTO_SLHDSA_ADDRESS_H

#include <openssl/mem.h>

#include "../internal.h"

#if defined(__cplusplus)
extern "C" {
#endif


// Offsets of various fields in the address structure for SLH-DSA-SHA2-128s.

// The byte used to specify the Merkle tree layer.
#define SLHDSA_SHA2_128S_OFFSET_LAYER 0

// The start of the 8 byte field used to specify the tree.
#define SLHDSA_SHA2_128S_OFFSET_TREE 1

// The byte used to specify the hash type (reason).
#define SLHDSA_SHA2_128S_OFFSET_TYPE 9

// The high byte used to specify the key pair (which one-time signature).
#define SLHDSA_SHA2_128S_OFFSET_KP_ADDR2 12

// The low byte used to specific the key pair.
#define SLHDSA_SHA2_128S_OFFSET_KP_ADDR1 13

// The byte used to specify the chain address (which Winternitz chain).
#define SLHDSA_SHA2_128S_OFFSET_CHAIN_ADDR 17

// The byte used to specify the hash address (where in the Winternitz chain).
#define SLHDSA_SHA2_128S_OFFSET_HASH_ADDR 21

// The byte used to specify the height of this node in the FORS or Merkle tree.
#define SLHDSA_SHA2_128S_OFFSET_TREE_HGT 17

// The start of the 4 byte field used to specify the node in the FORS or Merkle
// tree.
#define SLHDSA_SHA2_128S_OFFSET_TREE_INDEX 18


OPENSSL_INLINE void slhdsa_set_chain_addr(uint8_t addr[32], uint32_t chain) {
  addr[SLHDSA_SHA2_128S_OFFSET_CHAIN_ADDR] = (uint8_t)chain;
}

OPENSSL_INLINE void slhdsa_set_hash_addr(uint8_t addr[32], uint32_t hash) {
  addr[SLHDSA_SHA2_128S_OFFSET_HASH_ADDR] = (uint8_t)hash;
}

OPENSSL_INLINE void slhdsa_set_keypair_addr(uint8_t addr[32],
                                            uint32_t keypair) {
  addr[SLHDSA_SHA2_128S_OFFSET_KP_ADDR2] = (uint8_t)(keypair >> 8);
  addr[SLHDSA_SHA2_128S_OFFSET_KP_ADDR1] = (uint8_t)keypair;
}

OPENSSL_INLINE void slhdsa_copy_keypair_addr(uint8_t out[32],
                                             const uint8_t in[32]) {
  OPENSSL_memcpy(out, in, SLHDSA_SHA2_128S_OFFSET_TREE + 8);
  out[SLHDSA_SHA2_128S_OFFSET_KP_ADDR2] = in[SLHDSA_SHA2_128S_OFFSET_KP_ADDR2];
  out[SLHDSA_SHA2_128S_OFFSET_KP_ADDR1] = in[SLHDSA_SHA2_128S_OFFSET_KP_ADDR1];
}

OPENSSL_INLINE void slhdsa_set_layer_addr(uint8_t addr[32], uint32_t layer) {
  addr[SLHDSA_SHA2_128S_OFFSET_LAYER] = (uint8_t)layer;
}

OPENSSL_INLINE void slhdsa_set_tree_addr(uint8_t addr[32], uint64_t tree) {
  CRYPTO_store_u64_be(&addr[SLHDSA_SHA2_128S_OFFSET_TREE], tree);
}

#define SLHDSA_SHA2_128S_ADDR_TYPE_WOTS 0
#define SLHDSA_SHA2_128S_ADDR_TYPE_WOTSPK 1
#define SLHDSA_SHA2_128S_ADDR_TYPE_HASHTREE 2
#define SLHDSA_SHA2_128S_ADDR_TYPE_FORSTREE 3
#define SLHDSA_SHA2_128S_ADDR_TYPE_FORSPK 4
#define SLHDSA_SHA2_128S_ADDR_TYPE_WOTSPRF 5
#define SLHDSA_SHA2_128S_ADDR_TYPE_FORSPRF 6

OPENSSL_INLINE void slhdsa_set_type(uint8_t addr[32], uint32_t type) {
  // FIPS 205 relies on this setting parts of the address to 0, so we do it
  // here to avoid confusion.
  //
  // The behavior here is only correct for the SHA-2 instantiations.
  OPENSSL_memset(addr + 10, 0, 12);
  addr[SLHDSA_SHA2_128S_OFFSET_TYPE] = (uint8_t)type;
}

OPENSSL_INLINE void slhdsa_set_tree_height(uint8_t addr[32],
                                           uint32_t tree_height) {
  addr[SLHDSA_SHA2_128S_OFFSET_TREE_HGT] = (uint8_t)tree_height;
}

OPENSSL_INLINE void slhdsa_set_tree_index(uint8_t addr[32],
                                          uint32_t tree_index) {
  CRYPTO_store_u32_be(&addr[SLHDSA_SHA2_128S_OFFSET_TREE_INDEX], tree_index);
}

OPENSSL_INLINE uint32_t slhdsa_get_tree_index(uint8_t addr[32]) {
  return CRYPTO_load_u32_be(addr + SLHDSA_SHA2_128S_OFFSET_TREE_INDEX);
}


#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_CRYPTO_SLHDSA_ADDRESS_H
