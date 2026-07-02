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
#include <stdint.h>
#include <string.h>

#include "../../internal.h"
#include "./address.h"
#include "./params.h"
#include "./thash.h"
#include "./wots.h"


using namespace bssl;

// Implements Algorithm 5: chain function, page 18
static void chain(const slh_dsa_config *config, uint8_t *output,
                  const uint8_t *input, uint32_t start, uint32_t steps,
                  const uint8_t *pub_seed, uint8_t addr[32]) {
  assert(start < config->wots_w);
  assert(steps < config->wots_w);

  OPENSSL_memcpy(output, input, config->n);

  for (uint32_t i = start; i < (start + steps) && i < config->wots_w; ++i) {
    slhdsa_set_hash_addr(config, addr, i);
    slhdsa_thash_f(config, output, output, pub_seed, addr);
  }
}

static void slhdsa_wots_do_chain(const slh_dsa_config *config, uint8_t *out,
                                 uint8_t sk_addr[32], uint8_t addr[32],
                                 uint8_t value, const uint8_t *sk_seed,
                                 const uint8_t *pub_seed,
                                 uint32_t chain_index) {
  uint8_t tmp_sk[SLHDSA_MAX_N];
  slhdsa_set_chain_addr(config, sk_addr, chain_index);
  slhdsa_thash_prf(config, tmp_sk, pub_seed, sk_seed, sk_addr);
  slhdsa_set_chain_addr(config, addr, chain_index);
  chain(config, out, tmp_sk, 0, value, pub_seed, addr);
}

// Implements Algorithm 6: wots_pkGen function, page 18
void bssl::slhdsa_wots_pk_gen(const slh_dsa_config *config, uint8_t *pk,
                              const uint8_t *sk_seed, const uint8_t *pub_seed,
                              uint8_t addr[32]) {
  uint8_t wots_pk_addr[32], sk_addr[32];
  OPENSSL_memcpy(wots_pk_addr, addr, sizeof(wots_pk_addr));
  OPENSSL_memcpy(sk_addr, addr, sizeof(sk_addr));
  slhdsa_set_type(config, sk_addr, SLHDSA_ADDR_TYPE_WOTSPRF);
  slhdsa_copy_keypair_addr(config, sk_addr, addr);

  uint8_t tmp[SLHDSA_MAX_WOTS_BYTES];
  const uint32_t n = config->n;
  const uint32_t wots_len = slhdsa_wots_len(config);
  const uint32_t max_chain = config->wots_w - 1;
  for (uint32_t i = 0; i < wots_len; ++i) {
    slhdsa_wots_do_chain(config, tmp + i * n, sk_addr, addr, max_chain, sk_seed,
                         pub_seed, i);
  }

  // Compress pk
  slhdsa_set_type(config, wots_pk_addr, SLHDSA_ADDR_TYPE_WOTSPK);
  slhdsa_copy_keypair_addr(config, wots_pk_addr, addr);
  slhdsa_thash_tl(config, pk, tmp, pub_seed, wots_pk_addr);
}

// Implements Algorithm 7: wots_sign function, page 20
void bssl::slhdsa_wots_sign(const slh_dsa_config *config, uint8_t *sig,
                            const uint8_t *msg, const uint8_t *sk_seed,
                            const uint8_t *pub_seed, uint8_t addr[32]) {
  BSSL_CHECK(config->wots_w == 16);
  const size_t n = config->n;
  const uint32_t max_chain = config->wots_w - 1;

  uint16_t csum = 0;
  for (size_t i = 0; i < n; ++i) {
    csum += max_chain - (msg[i] >> 4);
    csum += max_chain - (msg[i] & 15);
  }

  // Compute chains
  uint8_t sk_addr[32];
  OPENSSL_memcpy(sk_addr, addr, sizeof(sk_addr));
  slhdsa_set_type(config, sk_addr, SLHDSA_ADDR_TYPE_WOTSPRF);
  slhdsa_copy_keypair_addr(config, sk_addr, addr);

  uint32_t chain_index = 0;
  for (size_t i = 0; i < n; ++i) {
    slhdsa_wots_do_chain(config, sig, sk_addr, addr, msg[i] >> 4, sk_seed,
                         pub_seed, chain_index++);
    sig += n;

    slhdsa_wots_do_chain(config, sig, sk_addr, addr, msg[i] & 15, sk_seed,
                         pub_seed, chain_index++);
    sig += n;
  }

  // Include the WOTS checksum values (len2 = 3 for the parameter sets we
  // support).
  slhdsa_wots_do_chain(config, sig, sk_addr, addr, (csum >> 8) & 15, sk_seed,
                       pub_seed, chain_index++);
  sig += n;
  slhdsa_wots_do_chain(config, sig, sk_addr, addr, (csum >> 4) & 15, sk_seed,
                       pub_seed, chain_index++);
  sig += n;
  slhdsa_wots_do_chain(config, sig, sk_addr, addr, csum & 15, sk_seed, pub_seed,
                       chain_index++);
}

static void slhdsa_wots_pk_from_sig_do_chain(const slh_dsa_config *config,
                                             uint8_t *out, uint8_t addr[32],
                                             const uint8_t *in, uint8_t value,
                                             const uint8_t *pub_seed,
                                             uint32_t chain_index) {
  slhdsa_set_chain_addr(config, addr, chain_index);
  chain(config, out + chain_index * config->n, in + chain_index * config->n,
        value, (config->wots_w - 1) - value, pub_seed, addr);
}

// Implements Algorithm 8: wots_pkFromSig function, page 21
void bssl::slhdsa_wots_pk_from_sig(const slh_dsa_config *config, uint8_t *pk,
                                   const uint8_t *sig, const uint8_t *msg,
                                   const uint8_t *pub_seed, uint8_t addr[32]) {
  BSSL_CHECK(config->wots_w == 16);
  const size_t n = config->n;
  const uint32_t max_chain = config->wots_w - 1;

  uint16_t csum = 0;
  for (size_t i = 0; i < n; ++i) {
    csum += max_chain - (msg[i] >> 4);
    csum += max_chain - (msg[i] & 15);
  }

  uint8_t tmp[SLHDSA_MAX_WOTS_BYTES];
  uint8_t wots_pk_addr[32];
  OPENSSL_memcpy(wots_pk_addr, addr, sizeof(wots_pk_addr));

  uint32_t chain_index = 0;
  for (size_t i = 0; i < n; ++i) {
    slhdsa_wots_pk_from_sig_do_chain(config, tmp, addr, sig, msg[i] >> 4,
                                     pub_seed, chain_index++);
    slhdsa_wots_pk_from_sig_do_chain(config, tmp, addr, sig, msg[i] & 15,
                                     pub_seed, chain_index++);
  }

  slhdsa_wots_pk_from_sig_do_chain(config, tmp, addr, sig, csum >> 8, pub_seed,
                                   chain_index++);
  slhdsa_wots_pk_from_sig_do_chain(config, tmp, addr, sig, (csum >> 4) & 15,
                                   pub_seed, chain_index++);
  slhdsa_wots_pk_from_sig_do_chain(config, tmp, addr, sig, csum & 15, pub_seed,
                                   chain_index++);

  // Compress pk
  slhdsa_set_type(config, wots_pk_addr, SLHDSA_ADDR_TYPE_WOTSPK);
  slhdsa_copy_keypair_addr(config, wots_pk_addr, addr);
  slhdsa_thash_tl(config, pk, tmp, pub_seed, wots_pk_addr);
}
