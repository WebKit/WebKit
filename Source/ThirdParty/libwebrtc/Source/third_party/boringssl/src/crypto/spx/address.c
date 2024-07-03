/* Copyright (c) 2023, Google LLC
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
#include "./spx_util.h"


// Offsets of various fields in the address structure for SPHINCS+-SHA2-128s.

// The byte used to specify the Merkle tree layer.
#define SPX_OFFSET_LAYER 0

// The start of the 8 byte field used to specify the tree.
#define SPX_OFFSET_TREE 1

// The byte used to specify the hash type (reason).
#define SPX_OFFSET_TYPE 9

// The high byte used to specify the key pair (which one-time signature).
#define SPX_OFFSET_KP_ADDR2 12

// The low byte used to specific the key pair.
#define SPX_OFFSET_KP_ADDR1 13

// The byte used to specify the chain address (which Winternitz chain).
#define SPX_OFFSET_CHAIN_ADDR 17

// The byte used to specify the hash address (where in the Winternitz chain).
#define SPX_OFFSET_HASH_ADDR 21

// The byte used to specify the height of this node in the FORS or Merkle tree.
#define SPX_OFFSET_TREE_HGT 17

// The start of the 4 byte field used to specify the node in the FORS or Merkle
// tree.
#define SPX_OFFSET_TREE_INDEX 18


void spx_set_chain_addr(uint8_t addr[32], uint32_t chain) {
  addr[SPX_OFFSET_CHAIN_ADDR] = (uint8_t)chain;
}

void spx_set_hash_addr(uint8_t addr[32], uint32_t hash) {
  addr[SPX_OFFSET_HASH_ADDR] = (uint8_t)hash;
}

void spx_set_keypair_addr(uint8_t addr[32], uint32_t keypair) {
  addr[SPX_OFFSET_KP_ADDR2] = (uint8_t)(keypair >> 8);
  addr[SPX_OFFSET_KP_ADDR1] = (uint8_t)keypair;
}

void spx_copy_keypair_addr(uint8_t out[32], const uint8_t in[32]) {
  memcpy(out, in, SPX_OFFSET_TREE + 8);
  out[SPX_OFFSET_KP_ADDR2] = in[SPX_OFFSET_KP_ADDR2];
  out[SPX_OFFSET_KP_ADDR1] = in[SPX_OFFSET_KP_ADDR1];
}

void spx_set_layer_addr(uint8_t addr[32], uint32_t layer) {
  addr[SPX_OFFSET_LAYER] = (uint8_t)layer;
}

void spx_set_tree_addr(uint8_t addr[32], uint64_t tree) {
  spx_uint64_to_len_bytes(&addr[SPX_OFFSET_TREE], 8, tree);
}

void spx_set_type(uint8_t addr[32], uint32_t type) {
  // NIST draft relies on this setting parts of the address to 0, so we do it
  // here to avoid confusion.
  //
  // The behavior here is only correct for the SHA2 instantiations.
  memset(addr + 10, 0, 12);
  addr[SPX_OFFSET_TYPE] = (uint8_t)type;
}

void spx_set_tree_height(uint8_t addr[32], uint32_t tree_height) {
  addr[SPX_OFFSET_TREE_HGT] = (uint8_t)tree_height;
}

void spx_set_tree_index(uint8_t addr[32], uint32_t tree_index) {
  CRYPTO_store_u32_be(&addr[SPX_OFFSET_TREE_INDEX], tree_index);
}

uint32_t spx_get_tree_index(uint8_t addr[32]) {
  return CRYPTO_load_u32_be(addr + SPX_OFFSET_TREE_INDEX);
}
