// Copyright 2014 The BoringSSL Authors
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

#include <openssl/bytestring.h>
#include <openssl/obj.h>
#include <openssl/rand.h>

#include "../../internal.h"
#include "../bcm_interface.h"
#include "address.h"
#include "fors.h"
#include "merkle.h"
#include "params.h"
#include "thash.h"


using namespace bssl;

#if defined(BORINGSSL_FIPS)

DEFINE_STATIC_ONCE(g_slhdsa_keygen_self_test_once)
DEFINE_STATIC_ONCE(g_slhdsa_sign_self_test_once)
DEFINE_STATIC_ONCE(g_slhdsa_verify_self_test_once)

#endif

namespace {

namespace fips {
void ensure_keygen_self_test();
void ensure_sign_self_test();
void ensure_verify_self_test();
}  // namespace fips

// The OBJECT IDENTIFIER header is also included in these values, per the spec.
const uint8_t kSHA256OID[] = {0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
                              0x65, 0x03, 0x04, 0x02, 0x01};
const uint8_t kSHA384OID[] = {0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
                              0x65, 0x03, 0x04, 0x02, 0x02};
#define MAX_OID_LENGTH 11
#define MAX_CONTEXT_LENGTH 255

bcm_infallible generate_key_from_seed_no_self_test(const slh_dsa_config *config,
                                                   uint8_t *out_public_key,
                                                   uint8_t *out_secret_key,
                                                   const uint8_t *seed) {
  // Initialize SK.seed || SK.prf || PK.seed from seed.
  OPENSSL_memcpy(out_secret_key, seed, 3 * config->n);

  // Initialize PK.seed from seed.
  OPENSSL_memcpy(out_public_key, seed + 2 * config->n, config->n);

  uint8_t addr[32] = {0};
  slhdsa_set_layer_addr(config, addr, config->d - 1);

  // Set PK.root
  slhdsa_treehash(config, out_public_key + config->n, out_secret_key, 0,
                  config->tree_height, out_public_key, addr);
  OPENSSL_memcpy(out_secret_key + 3 * config->n, out_public_key + config->n,
                 config->n);

  // FIPS 140-3 IG 10.3.A comment 1 says of the pair-wise consistency test for
  // SLH-DSA:
  //
  // "For key pairs generated for use with approved algorithms in SP 800-208 and
  // FIPS 205, the PCT (described by the tester in TE10.35.02) may be limited to
  // confirming the same key identifier (I in the case of LMS, SEED in the case
  // of XMSS and PK.SEED for SLH-DSA) is shared by the resulting public and
  // private key following generation."
  //
  // Since this is cheap, we always do this.

  if (boringssl_fips_break_test("SLHDSA_PWCT")) {
    out_public_key[0] ^= 1;
  }
  if (OPENSSL_memcmp(out_public_key, out_secret_key + 2 * config->n,
                     config->n) != 0) {
    abort();
  }

  return bcm_infallible::not_approved;
}

uint64_t load_tree_index(const slh_dsa_config *config, const uint8_t *in) {
  const size_t tree_bits = slhdsa_tree_bits(config);
  const size_t tree_bytes = slhdsa_tree_bytes(config);
  BSSL_CHECK(tree_bits <= 64);
  BSSL_CHECK(tree_bytes <= 8);

  uint8_t buf[8] = {0};
  OPENSSL_memcpy(buf + (sizeof(buf) - tree_bytes), in, tree_bytes);
  uint64_t index = CRYPTO_load_u64_be(buf);
  if (tree_bits < 64) {
    index &= (~(uint64_t)0) >> (64 - tree_bits);
  }
  return index;
}

// Implements Algorithm 22: slh_sign function (Section 10.2.1, page 39)
bcm_infallible sign_internal_no_self_test(
    const slh_dsa_config *config, uint8_t *out_signature,
    const uint8_t *secret_key,
    const uint8_t header[BCM_SLHDSA_M_PRIME_HEADER_LEN], const uint8_t *context,
    size_t context_len, const uint8_t *msg, size_t msg_len,
    const uint8_t *entropy) {
  const size_t n = config->n;
  const uint8_t *sk_seed = secret_key;
  const uint8_t *sk_prf = secret_key + n;
  const uint8_t *pk_seed = secret_key + 2 * n;
  const uint8_t *pk_root = secret_key + 3 * n;

  // Derive randomizer R and copy it to signature
  uint8_t R[SLHDSA_MAX_N];
  slhdsa_thash_prfmsg(config, R, sk_prf, entropy, header, context, context_len,
                      msg, msg_len);
  OPENSSL_memcpy(out_signature, R, n);

  // Compute message digest
  uint8_t digest[SLHDSA_MAX_DIGEST_SIZE];
  slhdsa_thash_hmsg(config, digest, R, pk_seed, pk_root, header, context,
                    context_len, msg, msg_len);

  uint8_t fors_digest[SLHDSA_MAX_FORS_MSG_BYTES];
  const size_t fors_msg_bytes = slhdsa_fors_msg_bytes(config);
  OPENSSL_memcpy(fors_digest, digest, fors_msg_bytes);

  size_t digest_offset = fors_msg_bytes;
  const uint64_t idx_tree = load_tree_index(config, digest + digest_offset);
  digest_offset += slhdsa_tree_bytes(config);
  uint32_t idx_leaf = 0;
  const size_t leaf_bytes = slhdsa_leaf_bytes(config);
  for (size_t i = 0; i < leaf_bytes; ++i) {
    idx_leaf = (idx_leaf << 8) | digest[digest_offset + i];
  }
  const size_t leaf_bits = slhdsa_leaf_bits(config);
  if (leaf_bits < 32) {
    idx_leaf &= (~(uint32_t)0) >> (32 - leaf_bits);
  }

  uint8_t addr[32] = {0};
  slhdsa_set_tree_addr(config, addr, idx_tree);
  slhdsa_set_type(config, addr, SLHDSA_ADDR_TYPE_FORSTREE);
  slhdsa_set_keypair_addr(config, addr, idx_leaf);

  slhdsa_fors_sign(config, out_signature + n, fors_digest, sk_seed, pk_seed,
                   addr);

  uint8_t pk_fors[SLHDSA_MAX_N];
  slhdsa_fors_pk_from_sig(config, pk_fors, out_signature + n, fors_digest,
                          pk_seed, addr);

  slhdsa_ht_sign(config, out_signature + n + slhdsa_fors_bytes(config), pk_fors,
                 idx_tree, idx_leaf, sk_seed, pk_seed);
  return bcm_infallible::approved;
}

bcm_status verify_internal(const slh_dsa_config *config,
                           const uint8_t *signature, size_t signature_len,
                           const uint8_t *public_key,
                           const uint8_t header[BCM_SLHDSA_M_PRIME_HEADER_LEN],
                           const uint8_t *context, size_t context_len,
                           const uint8_t *msg, size_t msg_len) {
  const size_t n = config->n;
  if (signature_len != config->signature_bytes) {
    return bcm_status::failure;
  }
  const uint8_t *pk_seed = public_key;
  const uint8_t *pk_root = public_key + n;

  const uint8_t *r = signature;
  const uint8_t *sig_fors = signature + n;
  const uint8_t *sig_ht = sig_fors + slhdsa_fors_bytes(config);

  uint8_t digest[SLHDSA_MAX_DIGEST_SIZE];
  slhdsa_thash_hmsg(config, digest, r, pk_seed, pk_root, header, context,
                    context_len, msg, msg_len);

  uint8_t fors_digest[SLHDSA_MAX_FORS_MSG_BYTES];
  const size_t fors_msg_bytes = slhdsa_fors_msg_bytes(config);
  OPENSSL_memcpy(fors_digest, digest, fors_msg_bytes);

  size_t digest_offset = fors_msg_bytes;
  const uint64_t idx_tree = load_tree_index(config, digest + digest_offset);
  digest_offset += slhdsa_tree_bytes(config);
  uint32_t idx_leaf = 0;
  const size_t leaf_bytes = slhdsa_leaf_bytes(config);
  for (size_t i = 0; i < leaf_bytes; ++i) {
    idx_leaf = (idx_leaf << 8) | digest[digest_offset + i];
  }
  const size_t leaf_bits = slhdsa_leaf_bits(config);
  if (leaf_bits < 32) {
    idx_leaf &= (~(uint32_t)0) >> (32 - leaf_bits);
  }

  uint8_t addr[32] = {0};
  slhdsa_set_tree_addr(config, addr, idx_tree);
  slhdsa_set_type(config, addr, SLHDSA_ADDR_TYPE_FORSTREE);
  slhdsa_set_keypair_addr(config, addr, idx_leaf);

  uint8_t pk_fors[SLHDSA_MAX_N];
  slhdsa_fors_pk_from_sig(config, pk_fors, sig_fors, fors_digest, pk_seed,
                          addr);

  if (!slhdsa_ht_verify(config, sig_ht, pk_fors, idx_tree, idx_leaf, pk_root,
                        pk_seed)) {
    return bcm_status::failure;
  }

  return bcm_status::approved;
}

namespace fips {

#include "fips_known_values.inc"

static int keygen_self_test() {
  uint8_t seed[3 * BCM_SLHDSA_SHA2_128S_N] = {0};
  uint8_t pub[BCM_SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES];
  uint8_t priv[BCM_SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES];
  generate_key_from_seed_no_self_test(&kSLHDSAConfigSHA2_128s, pub, priv, seed);

  if (!BORINGSSL_check_test(kExpectedPublicKey, pub, "SLH-DSA public key") ||
      !BORINGSSL_check_test(kExpectedPrivateKey, priv, "SLH-DSA private key")) {
    return 0;
  }

  return 1;
}

static int sign_self_test() {
  uint8_t header[BCM_SLHDSA_M_PRIME_HEADER_LEN] = {0};
  uint8_t entropy[BCM_SLHDSA_SHA2_128S_N] = {0};
  uint8_t sig[BCM_SLHDSA_SHA2_128S_SIGNATURE_BYTES];
  sign_internal_no_self_test(&kSLHDSAConfigSHA2_128s, sig, kExpectedPrivateKey,
                             header, nullptr, 0, nullptr, 0, entropy);
  uint8_t digest[32];
  SHA256(sig, sizeof(sig), digest);

  if (!BORINGSSL_check_test(kExpectedSignatureSHA256, digest,
                            "SLH-DSA signature")) {
    return 0;
  }

  return 1;
}

static int verify_self_test() {
  uint8_t header[BCM_SLHDSA_M_PRIME_HEADER_LEN] = {0};
  return verify_internal(&kSLHDSAConfigSHA2_128s, kExpectedSignature,
                         sizeof(kExpectedSignature), kExpectedPublicKey, header,
                         nullptr, 0, nullptr, 0) == bcm_status::approved;
}

#if defined(BORINGSSL_FIPS)

void ensure_keygen_self_test() {
  CRYPTO_once(g_slhdsa_keygen_self_test_once_bss_get(), []() {
    if (!keygen_self_test()) {
      BORINGSSL_FIPS_abort();
    }
  });
}

void ensure_sign_self_test() {
  CRYPTO_once(g_slhdsa_sign_self_test_once_bss_get(), []() {
    if (!sign_self_test()) {
      BORINGSSL_FIPS_abort();
    }
  });
}

void ensure_verify_self_test() {
  CRYPTO_once(g_slhdsa_verify_self_test_once_bss_get(), []() {
    if (!verify_self_test()) {
      BORINGSSL_FIPS_abort();
    }
  });
}

#else

void ensure_keygen_self_test() {}
void ensure_sign_self_test() {}
void ensure_verify_self_test() {}

#endif

}  // namespace fips

}  // namespace

bcm_infallible bssl::BCM_slhdsa_sha2_128s_generate_key_from_seed(
    uint8_t out_public_key[BCM_SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
    uint8_t out_secret_key[BCM_SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES],
    const uint8_t seed[3 * BCM_SLHDSA_SHA2_128S_N]) {
  fips::ensure_keygen_self_test();
  return generate_key_from_seed_no_self_test(
      &kSLHDSAConfigSHA2_128s, out_public_key, out_secret_key, seed);
}

bcm_infallible bssl::BCM_slhdsa_shake_256f_generate_key_from_seed(
    uint8_t out_public_key[BCM_SLHDSA_SHAKE_256F_PUBLIC_KEY_BYTES],
    uint8_t out_secret_key[BCM_SLHDSA_SHAKE_256F_PRIVATE_KEY_BYTES],
    const uint8_t seed[3 * BCM_SLHDSA_SHAKE_256F_N]) {
  fips::ensure_keygen_self_test();
  return generate_key_from_seed_no_self_test(
      &kSLHDSAConfigSHAKE_256f, out_public_key, out_secret_key, seed);
}

bcm_status bssl::BCM_slhdsa_sha2_128s_generate_key_from_seed_fips(
    uint8_t out_public_key[BCM_SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
    uint8_t out_secret_key[BCM_SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES],
    const uint8_t seed[3 * BCM_SLHDSA_SHA2_128S_N]) {
  if (out_public_key == nullptr || out_secret_key == nullptr) {
    return bcm_status::failure;
  }
  BCM_slhdsa_sha2_128s_generate_key_from_seed(out_public_key, out_secret_key,
                                              seed);
  return bcm_status::approved;
}

bcm_status bssl::BCM_slhdsa_shake_256f_generate_key_from_seed_fips(
    uint8_t out_public_key[BCM_SLHDSA_SHAKE_256F_PUBLIC_KEY_BYTES],
    uint8_t out_secret_key[BCM_SLHDSA_SHAKE_256F_PRIVATE_KEY_BYTES],
    const uint8_t seed[3 * BCM_SLHDSA_SHAKE_256F_N]) {
  if (out_public_key == nullptr || out_secret_key == nullptr) {
    return bcm_status::failure;
  }
  BCM_slhdsa_shake_256f_generate_key_from_seed(out_public_key, out_secret_key,
                                               seed);
  return bcm_status::approved;
}

bcm_infallible bssl::BCM_slhdsa_sha2_128s_generate_key(
    uint8_t out_public_key[BCM_SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
    uint8_t out_private_key[BCM_SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES]) {
  uint8_t seed[3 * BCM_SLHDSA_SHA2_128S_N];
  RAND_bytes(seed, 3 * BCM_SLHDSA_SHA2_128S_N);
  return BCM_slhdsa_sha2_128s_generate_key_from_seed(out_public_key,
                                                     out_private_key, seed);
}

bcm_infallible bssl::BCM_slhdsa_shake_256f_generate_key(
    uint8_t out_public_key[BCM_SLHDSA_SHAKE_256F_PUBLIC_KEY_BYTES],
    uint8_t out_private_key[BCM_SLHDSA_SHAKE_256F_PRIVATE_KEY_BYTES]) {
  uint8_t seed[3 * BCM_SLHDSA_SHAKE_256F_N];
  RAND_bytes(seed, 3 * BCM_SLHDSA_SHAKE_256F_N);
  return BCM_slhdsa_shake_256f_generate_key_from_seed(out_public_key,
                                                      out_private_key, seed);
}

bcm_status bssl::BCM_slhdsa_sha2_128s_generate_key_fips(
    uint8_t out_public_key[BCM_SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
    uint8_t out_private_key[BCM_SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES]) {
  if (out_public_key == nullptr || out_private_key == nullptr) {
    return bcm_status::failure;
  }
  BCM_slhdsa_sha2_128s_generate_key(out_public_key, out_private_key);
  return bcm_status::approved;
}

bcm_status bssl::BCM_slhdsa_shake_256f_generate_key_fips(
    uint8_t out_public_key[BCM_SLHDSA_SHAKE_256F_PUBLIC_KEY_BYTES],
    uint8_t out_private_key[BCM_SLHDSA_SHAKE_256F_PRIVATE_KEY_BYTES]) {
  if (out_public_key == nullptr || out_private_key == nullptr) {
    return bcm_status::failure;
  }
  BCM_slhdsa_shake_256f_generate_key(out_public_key, out_private_key);
  return bcm_status::approved;
}

bcm_infallible bssl::BCM_slhdsa_sha2_128s_public_from_private(
    uint8_t out_public_key[BCM_SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
    const uint8_t private_key[BCM_SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES]) {
  OPENSSL_memcpy(out_public_key, private_key + 2 * BCM_SLHDSA_SHA2_128S_N,
                 BCM_SLHDSA_SHA2_128S_N * 2);
  return bcm_infallible::approved;
}

bcm_infallible bssl::BCM_slhdsa_shake_256f_public_from_private(
    uint8_t out_public_key[BCM_SLHDSA_SHAKE_256F_PUBLIC_KEY_BYTES],
    const uint8_t private_key[BCM_SLHDSA_SHAKE_256F_PRIVATE_KEY_BYTES]) {
  OPENSSL_memcpy(out_public_key, private_key + 2 * BCM_SLHDSA_SHAKE_256F_N,
                 BCM_SLHDSA_SHAKE_256F_N * 2);
  return bcm_infallible::approved;
}

bcm_status bssl::BCM_slhdsa_sha2_128s_sign(
    uint8_t out_signature[BCM_SLHDSA_SHA2_128S_SIGNATURE_BYTES],
    const uint8_t private_key[BCM_SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES],
    const uint8_t *msg, size_t msg_len, const uint8_t *context,
    size_t context_len) {
  if (context_len > MAX_CONTEXT_LENGTH) {
    return bcm_status::failure;
  }

  // Construct header for M' as specified in Algorithm 22
  uint8_t M_prime_header[2];
  M_prime_header[0] = 0;  // domain separator for pure signing
  M_prime_header[1] = (uint8_t)context_len;

  uint8_t entropy[BCM_SLHDSA_SHA2_128S_N];
  RAND_bytes(entropy, sizeof(entropy));
  BCM_slhdsa_sha2_128s_sign_internal(out_signature, private_key, M_prime_header,
                                     context, context_len, msg, msg_len,
                                     entropy);
  return bcm_status::approved;
}

bcm_status bssl::BCM_slhdsa_shake_256f_sign(
    uint8_t out_signature[BCM_SLHDSA_SHAKE_256F_SIGNATURE_BYTES],
    const uint8_t private_key[BCM_SLHDSA_SHAKE_256F_PRIVATE_KEY_BYTES],
    const uint8_t *msg, size_t msg_len, const uint8_t *context,
    size_t context_len) {
  if (context_len > MAX_CONTEXT_LENGTH) {
    return bcm_status::failure;
  }

  uint8_t M_prime_header[2];
  M_prime_header[0] = 0;
  M_prime_header[1] = (uint8_t)context_len;

  uint8_t entropy[BCM_SLHDSA_SHAKE_256F_N];
  RAND_bytes(entropy, sizeof(entropy));
  BCM_slhdsa_shake_256f_sign_internal(out_signature, private_key,
                                      M_prime_header, context, context_len, msg,
                                      msg_len, entropy);
  return bcm_status::approved;
}

static int slhdsa_get_context_and_oid(uint8_t *out_context_and_oid,
                                      size_t *out_context_and_oid_len,
                                      size_t max_out_context_and_oid,
                                      const uint8_t *context,
                                      size_t context_len, int hash_nid,
                                      size_t hashed_msg_len) {
  const uint8_t *oid;
  size_t oid_len;
  size_t expected_hash_len;
  switch (hash_nid) {
    case NID_sha256:
      oid = kSHA256OID;
      oid_len = sizeof(kSHA256OID);
      static_assert(sizeof(kSHA256OID) <= MAX_OID_LENGTH);
      expected_hash_len = 32;
      break;

    // The SLH-DSA spec only lists SHA-256 and SHA-512. This function also
    // supports SHA-384, which is non-standard.
    case NID_sha384:
      oid = kSHA384OID;
      oid_len = sizeof(kSHA384OID);
      static_assert(sizeof(kSHA384OID) <= MAX_OID_LENGTH);
      expected_hash_len = 48;
      break;

    // If adding a hash function with a larger `oid_len`, update the size of
    // `context_and_oid` in the callers.
    default:
      return 0;
  }

  if (hashed_msg_len != expected_hash_len) {
    return 0;
  }

  *out_context_and_oid_len = context_len + oid_len;
  if (*out_context_and_oid_len > max_out_context_and_oid) {
    return 0;
  }

  OPENSSL_memcpy(out_context_and_oid, context, context_len);
  OPENSSL_memcpy(out_context_and_oid + context_len, oid, oid_len);

  return 1;
}

bcm_infallible bssl::BCM_slhdsa_sha2_128s_sign_internal(
    uint8_t out_signature[BCM_SLHDSA_SHA2_128S_SIGNATURE_BYTES],
    const uint8_t secret_key[BCM_SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES],
    const uint8_t header[BCM_SLHDSA_M_PRIME_HEADER_LEN], const uint8_t *context,
    size_t context_len, const uint8_t *msg, size_t msg_len,
    const uint8_t entropy[BCM_SLHDSA_SHA2_128S_N]) {
  fips::ensure_sign_self_test();
  return sign_internal_no_self_test(&kSLHDSAConfigSHA2_128s, out_signature,
                                    secret_key, header, context, context_len,
                                    msg, msg_len, entropy);
}

bcm_infallible bssl::BCM_slhdsa_shake_256f_sign_internal(
    uint8_t out_signature[BCM_SLHDSA_SHAKE_256F_SIGNATURE_BYTES],
    const uint8_t secret_key[BCM_SLHDSA_SHAKE_256F_PRIVATE_KEY_BYTES],
    const uint8_t header[BCM_SLHDSA_M_PRIME_HEADER_LEN], const uint8_t *context,
    size_t context_len, const uint8_t *msg, size_t msg_len,
    const uint8_t entropy[BCM_SLHDSA_SHAKE_256F_N]) {
  fips::ensure_sign_self_test();
  return sign_internal_no_self_test(&kSLHDSAConfigSHAKE_256f, out_signature,
                                    secret_key, header, context, context_len,
                                    msg, msg_len, entropy);
}

bcm_status bssl::BCM_slhdsa_sha2_128s_prehash_sign(
    uint8_t out_signature[BCM_SLHDSA_SHA2_128S_SIGNATURE_BYTES],
    const uint8_t private_key[BCM_SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES],
    const uint8_t *hashed_msg, size_t hashed_msg_len, int hash_nid,
    const uint8_t *context, size_t context_len) {
  if (context_len > MAX_CONTEXT_LENGTH) {
    return bcm_status::failure;
  }

  uint8_t M_prime_header[2];
  M_prime_header[0] = 1;  // domain separator for prehashed signing
  M_prime_header[1] = (uint8_t)context_len;

  uint8_t context_and_oid[MAX_CONTEXT_LENGTH + MAX_OID_LENGTH];
  size_t context_and_oid_len;
  if (!slhdsa_get_context_and_oid(context_and_oid, &context_and_oid_len,
                                  sizeof(context_and_oid), context, context_len,
                                  hash_nid, hashed_msg_len)) {
    return bcm_status::failure;
  }

  uint8_t entropy[BCM_SLHDSA_SHA2_128S_N];
  RAND_bytes(entropy, sizeof(entropy));
  BCM_slhdsa_sha2_128s_sign_internal(out_signature, private_key, M_prime_header,
                                     context_and_oid, context_and_oid_len,
                                     hashed_msg, hashed_msg_len, entropy);
  return bcm_status::approved;
}

bcm_status bssl::BCM_slhdsa_shake_256f_prehash_sign(
    uint8_t out_signature[BCM_SLHDSA_SHAKE_256F_SIGNATURE_BYTES],
    const uint8_t private_key[BCM_SLHDSA_SHAKE_256F_PRIVATE_KEY_BYTES],
    const uint8_t *hashed_msg, size_t hashed_msg_len, int hash_nid,
    const uint8_t *context, size_t context_len) {
  if (context_len > MAX_CONTEXT_LENGTH) {
    return bcm_status::failure;
  }

  uint8_t M_prime_header[2];
  M_prime_header[0] = 1;
  M_prime_header[1] = (uint8_t)context_len;

  uint8_t context_and_oid[MAX_CONTEXT_LENGTH + MAX_OID_LENGTH];
  size_t context_and_oid_len;
  if (!slhdsa_get_context_and_oid(context_and_oid, &context_and_oid_len,
                                  sizeof(context_and_oid), context, context_len,
                                  hash_nid, hashed_msg_len)) {
    return bcm_status::failure;
  }

  uint8_t entropy[BCM_SLHDSA_SHAKE_256F_N];
  RAND_bytes(entropy, sizeof(entropy));
  BCM_slhdsa_shake_256f_sign_internal(
      out_signature, private_key, M_prime_header, context_and_oid,
      context_and_oid_len, hashed_msg, hashed_msg_len, entropy);
  return bcm_status::approved;
}

// Implements Algorithm 24: slh_verify function (Section 10.3, page 41)
bcm_status bssl::BCM_slhdsa_sha2_128s_verify(
    const uint8_t *signature, size_t signature_len,
    const uint8_t public_key[BCM_SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
    const uint8_t *msg, size_t msg_len, const uint8_t *context,
    size_t context_len) {
  if (context_len > MAX_CONTEXT_LENGTH) {
    return bcm_status::failure;
  }

  // Construct header for M' as specified in Algorithm 24
  uint8_t M_prime_header[2];
  M_prime_header[0] = 0;  // domain separator for pure verification
  M_prime_header[1] = (uint8_t)context_len;

  return BCM_slhdsa_sha2_128s_verify_internal(
      signature, signature_len, public_key, M_prime_header, context,
      context_len, msg, msg_len);
}

bcm_status bssl::BCM_slhdsa_shake_256f_verify(
    const uint8_t *signature, size_t signature_len,
    const uint8_t public_key[BCM_SLHDSA_SHAKE_256F_PUBLIC_KEY_BYTES],
    const uint8_t *msg, size_t msg_len, const uint8_t *context,
    size_t context_len) {
  if (context_len > MAX_CONTEXT_LENGTH) {
    return bcm_status::failure;
  }

  uint8_t M_prime_header[2];
  M_prime_header[0] = 0;
  M_prime_header[1] = (uint8_t)context_len;

  return BCM_slhdsa_shake_256f_verify_internal(
      signature, signature_len, public_key, M_prime_header, context,
      context_len, msg, msg_len);
}

bcm_status bssl::BCM_slhdsa_sha2_128s_prehash_verify(
    const uint8_t *signature, size_t signature_len,
    const uint8_t public_key[BCM_SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
    const uint8_t *hashed_msg, size_t hashed_msg_len, int hash_nid,
    const uint8_t *context, size_t context_len) {
  if (context_len > MAX_CONTEXT_LENGTH) {
    return bcm_status::failure;
  }

  uint8_t M_prime_header[2];
  M_prime_header[0] = 1;  // domain separator for prehashed verification
  M_prime_header[1] = (uint8_t)context_len;

  uint8_t context_and_oid[MAX_CONTEXT_LENGTH + MAX_OID_LENGTH];
  size_t context_and_oid_len;
  if (!slhdsa_get_context_and_oid(context_and_oid, &context_and_oid_len,
                                  sizeof(context_and_oid), context, context_len,
                                  hash_nid, hashed_msg_len)) {
    return bcm_status::failure;
  }

  return BCM_slhdsa_sha2_128s_verify_internal(
      signature, signature_len, public_key, M_prime_header, context_and_oid,
      context_and_oid_len, hashed_msg, hashed_msg_len);
}

bcm_status bssl::BCM_slhdsa_shake_256f_prehash_verify(
    const uint8_t *signature, size_t signature_len,
    const uint8_t public_key[BCM_SLHDSA_SHAKE_256F_PUBLIC_KEY_BYTES],
    const uint8_t *hashed_msg, size_t hashed_msg_len, int hash_nid,
    const uint8_t *context, size_t context_len) {
  if (context_len > MAX_CONTEXT_LENGTH) {
    return bcm_status::failure;
  }

  uint8_t M_prime_header[2];
  M_prime_header[0] = 1;
  M_prime_header[1] = (uint8_t)context_len;

  uint8_t context_and_oid[MAX_CONTEXT_LENGTH + MAX_OID_LENGTH];
  size_t context_and_oid_len;
  if (!slhdsa_get_context_and_oid(context_and_oid, &context_and_oid_len,
                                  sizeof(context_and_oid), context, context_len,
                                  hash_nid, hashed_msg_len)) {
    return bcm_status::failure;
  }

  return BCM_slhdsa_shake_256f_verify_internal(
      signature, signature_len, public_key, M_prime_header, context_and_oid,
      context_and_oid_len, hashed_msg, hashed_msg_len);
}

bcm_status bssl::BCM_slhdsa_sha2_128s_verify_internal(
    const uint8_t *signature, size_t signature_len,
    const uint8_t public_key[BCM_SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
    const uint8_t header[BCM_SLHDSA_M_PRIME_HEADER_LEN], const uint8_t *context,
    size_t context_len, const uint8_t *msg, size_t msg_len) {
  fips::ensure_verify_self_test();
  return verify_internal(&kSLHDSAConfigSHA2_128s, signature, signature_len,
                         public_key, header, context, context_len, msg,
                         msg_len);
}

bcm_status bssl::BCM_slhdsa_shake_256f_verify_internal(
    const uint8_t *signature, size_t signature_len,
    const uint8_t public_key[BCM_SLHDSA_SHAKE_256F_PUBLIC_KEY_BYTES],
    const uint8_t header[BCM_SLHDSA_M_PRIME_HEADER_LEN], const uint8_t *context,
    size_t context_len, const uint8_t *msg, size_t msg_len) {
  fips::ensure_verify_self_test();
  return verify_internal(&kSLHDSAConfigSHAKE_256f, signature, signature_len,
                         public_key, header, context, context_len, msg,
                         msg_len);
}

int bssl::boringssl_self_test_slhdsa() {
  return fips::keygen_self_test() && fips::sign_self_test() &&
         fips::verify_self_test();
}
