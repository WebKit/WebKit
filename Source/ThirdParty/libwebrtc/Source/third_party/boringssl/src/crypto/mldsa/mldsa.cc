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

#include <openssl/mldsa.h>

#include "../fipsmodule/bcm_interface.h"

static_assert(sizeof(BCM_mldsa65_private_key) == sizeof(MLDSA65_private_key));
static_assert(alignof(BCM_mldsa65_private_key) == alignof(MLDSA65_private_key));
static_assert(sizeof(BCM_mldsa65_public_key) == sizeof(MLDSA65_public_key));
static_assert(alignof(BCM_mldsa65_public_key) == alignof(MLDSA65_public_key));
static_assert(sizeof(BCM_mldsa65_prehash) == sizeof(MLDSA65_prehash));
static_assert(alignof(BCM_mldsa65_prehash) == alignof(MLDSA65_prehash));
static_assert(sizeof(BCM_mldsa87_private_key) == sizeof(MLDSA87_private_key));
static_assert(alignof(BCM_mldsa87_private_key) == alignof(MLDSA87_private_key));
static_assert(sizeof(BCM_mldsa87_public_key) == sizeof(MLDSA87_public_key));
static_assert(alignof(BCM_mldsa87_public_key) == alignof(MLDSA87_public_key));
static_assert(sizeof(BCM_mldsa87_prehash) == sizeof(MLDSA87_prehash));
static_assert(alignof(BCM_mldsa87_prehash) == alignof(MLDSA87_prehash));
static_assert(sizeof(BCM_mldsa44_private_key) == sizeof(MLDSA44_private_key));
static_assert(alignof(BCM_mldsa44_private_key) == alignof(MLDSA44_private_key));
static_assert(sizeof(BCM_mldsa44_public_key) == sizeof(MLDSA44_public_key));
static_assert(alignof(BCM_mldsa44_public_key) == alignof(MLDSA44_public_key));
static_assert(sizeof(BCM_mldsa44_prehash) == sizeof(MLDSA44_prehash));
static_assert(alignof(BCM_mldsa44_prehash) == alignof(MLDSA44_prehash));
static_assert(MLDSA_SEED_BYTES == BCM_MLDSA_SEED_BYTES);
static_assert(MLDSA_MU_BYTES == BCM_MLDSA_MU_BYTES);
static_assert(MLDSA65_PRIVATE_KEY_BYTES == BCM_MLDSA65_PRIVATE_KEY_BYTES);
static_assert(MLDSA65_PUBLIC_KEY_BYTES == BCM_MLDSA65_PUBLIC_KEY_BYTES);
static_assert(MLDSA65_SIGNATURE_BYTES == BCM_MLDSA65_SIGNATURE_BYTES);
static_assert(MLDSA87_PRIVATE_KEY_BYTES == BCM_MLDSA87_PRIVATE_KEY_BYTES);
static_assert(MLDSA87_PUBLIC_KEY_BYTES == BCM_MLDSA87_PUBLIC_KEY_BYTES);
static_assert(MLDSA87_SIGNATURE_BYTES == BCM_MLDSA87_SIGNATURE_BYTES);
static_assert(MLDSA44_PRIVATE_KEY_BYTES == BCM_MLDSA44_PRIVATE_KEY_BYTES);
static_assert(MLDSA44_PUBLIC_KEY_BYTES == BCM_MLDSA44_PUBLIC_KEY_BYTES);
static_assert(MLDSA44_SIGNATURE_BYTES == BCM_MLDSA44_SIGNATURE_BYTES);

int MLDSA65_generate_key(
    uint8_t out_encoded_public_key[MLDSA65_PUBLIC_KEY_BYTES],
    uint8_t out_seed[MLDSA_SEED_BYTES],
    struct MLDSA65_private_key *out_private_key) {
  return bcm_success(BCM_mldsa65_generate_key(
      out_encoded_public_key, out_seed,
      reinterpret_cast<BCM_mldsa65_private_key *>(out_private_key)));
}

int MLDSA65_private_key_from_seed(struct MLDSA65_private_key *out_private_key,
                                  const uint8_t *seed, size_t seed_len) {
  if (seed_len != BCM_MLDSA_SEED_BYTES) {
    return 0;
  }
  return bcm_success(BCM_mldsa65_private_key_from_seed(
      reinterpret_cast<BCM_mldsa65_private_key *>(out_private_key), seed));
}

int MLDSA65_public_from_private(struct MLDSA65_public_key *out_public_key,
                                const struct MLDSA65_private_key *private_key) {
  return bcm_success(BCM_mldsa65_public_from_private(
      reinterpret_cast<BCM_mldsa65_public_key *>(out_public_key),
      reinterpret_cast<const BCM_mldsa65_private_key *>(private_key)));
}

int MLDSA65_sign(uint8_t out_encoded_signature[MLDSA65_SIGNATURE_BYTES],
                 const struct MLDSA65_private_key *private_key,
                 const uint8_t *msg, size_t msg_len, const uint8_t *context,
                 size_t context_len) {
  if (context_len > 255) {
    return 0;
  }
  return bcm_success(BCM_mldsa65_sign(
      out_encoded_signature,
      reinterpret_cast<const BCM_mldsa65_private_key *>(private_key), msg,
      msg_len, context, context_len));
}

int MLDSA65_verify(const struct MLDSA65_public_key *public_key,
                   const uint8_t *signature, size_t signature_len,
                   const uint8_t *msg, size_t msg_len, const uint8_t *context,
                   size_t context_len) {
  if (context_len > 255 || signature_len != BCM_MLDSA65_SIGNATURE_BYTES) {
    return 0;
  }
  return bcm_success(BCM_mldsa65_verify(
      reinterpret_cast<const BCM_mldsa65_public_key *>(public_key), signature,
      msg, msg_len, context, context_len));
}

int MLDSA65_prehash_init(struct MLDSA65_prehash *out_state,
                         const struct MLDSA65_public_key *public_key,
                         const uint8_t *context, size_t context_len) {
  if (context_len > 255) {
    return 0;
  }
  BCM_mldsa65_prehash_init(
      reinterpret_cast<BCM_mldsa65_prehash *>(out_state),
      reinterpret_cast<const BCM_mldsa65_public_key *>(public_key), context,
      context_len);
  return 1;
}

void MLDSA65_prehash_update(struct MLDSA65_prehash *inout_state,
                            const uint8_t *msg, size_t msg_len) {
  BCM_mldsa65_prehash_update(
      reinterpret_cast<BCM_mldsa65_prehash *>(inout_state), msg, msg_len);
}

void MLDSA65_prehash_finalize(uint8_t out_msg_rep[MLDSA_MU_BYTES],
                              struct MLDSA65_prehash *inout_state) {
  BCM_mldsa65_prehash_finalize(
      out_msg_rep, reinterpret_cast<BCM_mldsa65_prehash *>(inout_state));
}

int MLDSA65_sign_message_representative(
    uint8_t out_encoded_signature[MLDSA65_SIGNATURE_BYTES],
    const struct MLDSA65_private_key *private_key,
    const uint8_t msg_rep[MLDSA_MU_BYTES]) {
  return bcm_success(BCM_mldsa65_sign_message_representative(
      out_encoded_signature,
      reinterpret_cast<const BCM_mldsa65_private_key *>(private_key), msg_rep));
}

int MLDSA65_marshal_public_key(CBB *out,
                               const struct MLDSA65_public_key *public_key) {
  return bcm_success(BCM_mldsa65_marshal_public_key(
      out, reinterpret_cast<const BCM_mldsa65_public_key *>(public_key)));
}

int MLDSA65_parse_public_key(struct MLDSA65_public_key *public_key, CBS *in) {
  return bcm_success(BCM_mldsa65_parse_public_key(
      reinterpret_cast<BCM_mldsa65_public_key *>(public_key), in));
}

int MLDSA87_generate_key(
    uint8_t out_encoded_public_key[MLDSA87_PUBLIC_KEY_BYTES],
    uint8_t out_seed[MLDSA_SEED_BYTES],
    struct MLDSA87_private_key *out_private_key) {
  return bcm_success(BCM_mldsa87_generate_key(
      out_encoded_public_key, out_seed,
      reinterpret_cast<BCM_mldsa87_private_key *>(out_private_key)));
}

int MLDSA87_private_key_from_seed(struct MLDSA87_private_key *out_private_key,
                                  const uint8_t *seed, size_t seed_len) {
  if (seed_len != BCM_MLDSA_SEED_BYTES) {
    return 0;
  }
  return bcm_success(BCM_mldsa87_private_key_from_seed(
      reinterpret_cast<BCM_mldsa87_private_key *>(out_private_key), seed));
}

int MLDSA87_public_from_private(struct MLDSA87_public_key *out_public_key,
                                const struct MLDSA87_private_key *private_key) {
  return bcm_success(BCM_mldsa87_public_from_private(
      reinterpret_cast<BCM_mldsa87_public_key *>(out_public_key),
      reinterpret_cast<const BCM_mldsa87_private_key *>(private_key)));
}

int MLDSA87_sign(uint8_t out_encoded_signature[MLDSA87_SIGNATURE_BYTES],
                 const struct MLDSA87_private_key *private_key,
                 const uint8_t *msg, size_t msg_len, const uint8_t *context,
                 size_t context_len) {
  if (context_len > 255) {
    return 0;
  }
  return bcm_success(BCM_mldsa87_sign(
      out_encoded_signature,
      reinterpret_cast<const BCM_mldsa87_private_key *>(private_key), msg,
      msg_len, context, context_len));
}

int MLDSA87_verify(const struct MLDSA87_public_key *public_key,
                   const uint8_t *signature, size_t signature_len,
                   const uint8_t *msg, size_t msg_len, const uint8_t *context,
                   size_t context_len) {
  if (context_len > 255 || signature_len != BCM_MLDSA87_SIGNATURE_BYTES) {
    return 0;
  }
  return bcm_success(BCM_mldsa87_verify(
      reinterpret_cast<const BCM_mldsa87_public_key *>(public_key), signature,
      msg, msg_len, context, context_len));
}

int MLDSA87_prehash_init(struct MLDSA87_prehash *out_state,
                         const struct MLDSA87_public_key *public_key,
                         const uint8_t *context, size_t context_len) {
  if (context_len > 255) {
    return 0;
  }
  BCM_mldsa87_prehash_init(
      reinterpret_cast<BCM_mldsa87_prehash *>(out_state),
      reinterpret_cast<const BCM_mldsa87_public_key *>(public_key), context,
      context_len);
  return 1;
}

void MLDSA87_prehash_update(struct MLDSA87_prehash *inout_state,
                            const uint8_t *msg, size_t msg_len) {
  BCM_mldsa87_prehash_update(
      reinterpret_cast<BCM_mldsa87_prehash *>(inout_state), msg, msg_len);
}

void MLDSA87_prehash_finalize(uint8_t out_msg_rep[MLDSA_MU_BYTES],
                              struct MLDSA87_prehash *inout_state) {
  BCM_mldsa87_prehash_finalize(
      out_msg_rep, reinterpret_cast<BCM_mldsa87_prehash *>(inout_state));
}

int MLDSA87_sign_message_representative(
    uint8_t out_encoded_signature[MLDSA87_SIGNATURE_BYTES],
    const struct MLDSA87_private_key *private_key,
    const uint8_t msg_rep[MLDSA_MU_BYTES]) {
  return bcm_success(BCM_mldsa87_sign_message_representative(
      out_encoded_signature,
      reinterpret_cast<const BCM_mldsa87_private_key *>(private_key), msg_rep));
}

int MLDSA87_marshal_public_key(CBB *out,
                               const struct MLDSA87_public_key *public_key) {
  return bcm_success(BCM_mldsa87_marshal_public_key(
      out, reinterpret_cast<const BCM_mldsa87_public_key *>(public_key)));
}

int MLDSA87_parse_public_key(struct MLDSA87_public_key *public_key, CBS *in) {
  return bcm_success(BCM_mldsa87_parse_public_key(
      reinterpret_cast<BCM_mldsa87_public_key *>(public_key), in));
}

int MLDSA44_generate_key(
    uint8_t out_encoded_public_key[MLDSA44_PUBLIC_KEY_BYTES],
    uint8_t out_seed[MLDSA_SEED_BYTES],
    struct MLDSA44_private_key *out_private_key) {
  return bcm_success(BCM_mldsa44_generate_key(
      out_encoded_public_key, out_seed,
      reinterpret_cast<BCM_mldsa44_private_key *>(out_private_key)));
}

int MLDSA44_private_key_from_seed(struct MLDSA44_private_key *out_private_key,
                                  const uint8_t *seed, size_t seed_len) {
  if (seed_len != BCM_MLDSA_SEED_BYTES) {
    return 0;
  }
  return bcm_success(BCM_mldsa44_private_key_from_seed(
      reinterpret_cast<BCM_mldsa44_private_key *>(out_private_key), seed));
}

int MLDSA44_public_from_private(struct MLDSA44_public_key *out_public_key,
                                const struct MLDSA44_private_key *private_key) {
  return bcm_success(BCM_mldsa44_public_from_private(
      reinterpret_cast<BCM_mldsa44_public_key *>(out_public_key),
      reinterpret_cast<const BCM_mldsa44_private_key *>(private_key)));
}

int MLDSA44_sign(uint8_t out_encoded_signature[MLDSA44_SIGNATURE_BYTES],
                 const struct MLDSA44_private_key *private_key,
                 const uint8_t *msg, size_t msg_len, const uint8_t *context,
                 size_t context_len) {
  if (context_len > 255) {
    return 0;
  }
  return bcm_success(BCM_mldsa44_sign(
      out_encoded_signature,
      reinterpret_cast<const BCM_mldsa44_private_key *>(private_key), msg,
      msg_len, context, context_len));
}

int MLDSA44_verify(const struct MLDSA44_public_key *public_key,
                   const uint8_t *signature, size_t signature_len,
                   const uint8_t *msg, size_t msg_len, const uint8_t *context,
                   size_t context_len) {
  if (context_len > 255 || signature_len != BCM_MLDSA44_SIGNATURE_BYTES) {
    return 0;
  }
  return bcm_success(BCM_mldsa44_verify(
      reinterpret_cast<const BCM_mldsa44_public_key *>(public_key), signature,
      msg, msg_len, context, context_len));
}

int MLDSA44_prehash_init(struct MLDSA44_prehash *out_state,
                         const struct MLDSA44_public_key *public_key,
                         const uint8_t *context, size_t context_len) {
  if (context_len > 255) {
    return 0;
  }
  BCM_mldsa44_prehash_init(
      reinterpret_cast<BCM_mldsa44_prehash *>(out_state),
      reinterpret_cast<const BCM_mldsa44_public_key *>(public_key), context,
      context_len);
  return 1;
}

void MLDSA44_prehash_update(struct MLDSA44_prehash *inout_state,
                            const uint8_t *msg, size_t msg_len) {
  BCM_mldsa44_prehash_update(
      reinterpret_cast<BCM_mldsa44_prehash *>(inout_state), msg, msg_len);
}

void MLDSA44_prehash_finalize(uint8_t out_msg_rep[MLDSA_MU_BYTES],
                              struct MLDSA44_prehash *inout_state) {
  BCM_mldsa44_prehash_finalize(
      out_msg_rep, reinterpret_cast<BCM_mldsa44_prehash *>(inout_state));
}

int MLDSA44_sign_message_representative(
    uint8_t out_encoded_signature[MLDSA44_SIGNATURE_BYTES],
    const struct MLDSA44_private_key *private_key,
    const uint8_t msg_rep[MLDSA_MU_BYTES]) {
  return bcm_success(BCM_mldsa44_sign_message_representative(
      out_encoded_signature,
      reinterpret_cast<const BCM_mldsa44_private_key *>(private_key), msg_rep));
}

int MLDSA44_marshal_public_key(CBB *out,
                               const struct MLDSA44_public_key *public_key) {
  return bcm_success(BCM_mldsa44_marshal_public_key(
      out, reinterpret_cast<const BCM_mldsa44_public_key *>(public_key)));
}

int MLDSA44_parse_public_key(struct MLDSA44_public_key *public_key, CBS *in) {
  return bcm_success(BCM_mldsa44_parse_public_key(
      reinterpret_cast<BCM_mldsa44_public_key *>(public_key), in));
}
