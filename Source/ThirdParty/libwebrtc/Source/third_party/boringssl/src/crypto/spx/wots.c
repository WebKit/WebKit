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

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "./address.h"
#include "./params.h"
#include "./spx_util.h"
#include "./thash.h"
#include "./wots.h"

// Chaining function used in WOTS+.
static void chain(uint8_t *output, const uint8_t *input, uint32_t start,
                  uint32_t steps, const uint8_t *pub_seed, uint8_t addr[32]) {
  memcpy(output, input, SPX_N);

  for (size_t i = start; i < (start + steps) && i < SPX_WOTS_W; ++i) {
    spx_set_hash_addr(addr, i);
    spx_thash_f(output, output, pub_seed, addr);
  }
}

void spx_wots_pk_from_sig(uint8_t *pk, const uint8_t *sig, const uint8_t *msg,
                          const uint8_t pub_seed[SPX_N], uint8_t addr[32]) {
  uint8_t tmp[SPX_WOTS_BYTES];
  uint8_t wots_pk_addr[32];
  memcpy(wots_pk_addr, addr, sizeof(wots_pk_addr));

  // Convert message to base w
  uint32_t base_w_msg[SPX_WOTS_LEN];
  spx_base_b(base_w_msg, SPX_WOTS_LEN1, msg, /*log2_b=*/SPX_WOTS_LOG_W);

  // Compute checksum
  uint64_t csum = 0;
  for (size_t i = 0; i < SPX_WOTS_LEN1; ++i) {
    csum += SPX_WOTS_W - 1 - base_w_msg[i];
  }

  // Convert csum to base w as in Algorithm 7, Line 9
  uint8_t csum_bytes[(SPX_WOTS_LEN2 * SPX_WOTS_LOG_W + 7) / 8];
  csum = csum << ((8 - ((SPX_WOTS_LEN2 * SPX_WOTS_LOG_W)) % 8) % 8);
  spx_uint64_to_len_bytes(csum_bytes, sizeof(csum_bytes), csum);

  // Write the base w representation of csum to the end of the message.
  spx_base_b(base_w_msg + SPX_WOTS_LEN1, SPX_WOTS_LEN2, csum_bytes,
             /*log2_b=*/SPX_WOTS_LOG_W);

  // Compute chains
  for (size_t i = 0; i < SPX_WOTS_LEN; ++i) {
    spx_set_chain_addr(addr, i);
    chain(tmp + i * SPX_N, sig + i * SPX_N, base_w_msg[i],
          SPX_WOTS_W - 1 - base_w_msg[i], pub_seed, addr);
  }

  // Compress pk
  spx_set_type(wots_pk_addr, SPX_ADDR_TYPE_WOTSPK);
  spx_copy_keypair_addr(wots_pk_addr, addr);
  spx_thash_tl(pk, tmp, pub_seed, wots_pk_addr);
}

void spx_wots_pk_gen(uint8_t *pk, const uint8_t sk_seed[SPX_N],
                     const uint8_t pub_seed[SPX_N], uint8_t addr[32]) {
  uint8_t tmp[SPX_WOTS_BYTES];
  uint8_t tmp_sk[SPX_N];
  uint8_t wots_pk_addr[32], sk_addr[32];
  memcpy(wots_pk_addr, addr, sizeof(wots_pk_addr));
  memcpy(sk_addr, addr, sizeof(sk_addr));

  spx_set_type(sk_addr, SPX_ADDR_TYPE_WOTSPRF);
  spx_copy_keypair_addr(sk_addr, addr);

  for (size_t i = 0; i < SPX_WOTS_LEN; ++i) {
    spx_set_chain_addr(sk_addr, i);
    spx_thash_prf(tmp_sk, pub_seed, sk_seed, sk_addr);
    spx_set_chain_addr(addr, i);
    chain(tmp + i * SPX_N, tmp_sk, 0, SPX_WOTS_W - 1, pub_seed, addr);
  }

  // Compress pk
  spx_set_type(wots_pk_addr, SPX_ADDR_TYPE_WOTSPK);
  spx_copy_keypair_addr(wots_pk_addr, addr);
  spx_thash_tl(pk, tmp, pub_seed, wots_pk_addr);
}

void spx_wots_sign(uint8_t *sig, const uint8_t msg[SPX_N],
                   const uint8_t sk_seed[SPX_N], const uint8_t pub_seed[SPX_N],
                   uint8_t addr[32]) {
  // Convert message to base w
  uint32_t base_w_msg[SPX_WOTS_LEN];
  spx_base_b(base_w_msg, SPX_WOTS_LEN1, msg, /*log2_b=*/SPX_WOTS_LOG_W);

  // Compute checksum
  uint64_t csum = 0;
  for (size_t i = 0; i < SPX_WOTS_LEN1; ++i) {
    csum += SPX_WOTS_W - 1 - base_w_msg[i];
  }

  // Convert csum to base w as in Algorithm 6, Line 9
  uint8_t csum_bytes[(SPX_WOTS_LEN2 * SPX_WOTS_LOG_W + 7) / 8];
  csum = csum << ((8 - ((SPX_WOTS_LEN2 * SPX_WOTS_LOG_W)) % 8) % 8);
  spx_uint64_to_len_bytes(csum_bytes, sizeof(csum_bytes), csum);

  // Write the base w representation of csum to the end of the message.
  spx_base_b(base_w_msg + SPX_WOTS_LEN1, SPX_WOTS_LEN2, csum_bytes,
             /*log2_b=*/SPX_WOTS_LOG_W);

  // Compute chains
  uint8_t tmp_sk[SPX_N];
  uint8_t sk_addr[32];
  memcpy(sk_addr, addr, sizeof(sk_addr));
  spx_set_type(sk_addr, SPX_ADDR_TYPE_WOTSPRF);
  spx_copy_keypair_addr(sk_addr, addr);

  for (size_t i = 0; i < SPX_WOTS_LEN; ++i) {
    spx_set_chain_addr(sk_addr, i);
    spx_thash_prf(tmp_sk, pub_seed, sk_seed, sk_addr);
    spx_set_chain_addr(addr, i);
    chain(sig + i * SPX_N, tmp_sk, 0, base_w_msg[i], pub_seed, addr);
  }
}
