// Copyright 2026 The BoringSSL Authors
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


#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <openssl/base.h>
#include <openssl/bytestring.h>
#include <openssl/curve25519.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/rand.h>
#include <openssl/sha2.h>
#include <openssl/span.h>

#include "../internal.h"
#include "../mem_internal.h"
#include "./internal.h"


CPACE_CTX *CPACE_CTX_new(enum cpace_role_t role, const uint8_t *password,
                         size_t password_len, const uint8_t *channel_id,
                         size_t channel_id_len, const uint8_t *assoc_data,
                         size_t assoc_data_len, const uint8_t *session_id,
                         size_t session_id_len) {
  bssl::UniquePtr<bssl::CpaceCtx> ctx = bssl::MakeUnique<bssl::CpaceCtx>();
  if (ctx == nullptr) {
    return nullptr;
  }

  ctx->our_role = role;
  if (!ctx->our_password.CopyFrom(bssl::Span(password, password_len)) ||
      !ctx->channel_id.CopyFrom(bssl::Span(channel_id, channel_id_len)) ||
      !ctx->our_aad.CopyFrom(bssl::Span(assoc_data, assoc_data_len)) ||
      !ctx->session_id.CopyFrom(bssl::Span(session_id, session_id_len))) {
    return nullptr;
  }

  return ctx.release();
}

void CPACE_CTX_free(CPACE_CTX *ctx) { Delete(bssl::FromOpaque(ctx)); }

namespace {
constexpr uint64_t leb128_len(uint64_t len) {
  if (len < 128) {
    return 1;
  }
  return (bssl::CRYPTO_bit_width(len) + 6) / 7;
}

static constexpr size_t kMaxLenTagSize = leb128_len(UINT64_MAX);
static constexpr std::string_view kDSI = "CPace255";

constexpr size_t leb128_tag(uint64_t len, uint8_t tag[kMaxLenTagSize]) {
  size_t tag_len = leb128_len(len);
  for (size_t i = 0; i < tag_len; ++i) {
    if (len < 128) {
      tag[i] = len;
    } else {
      tag[i] = (len & 0x7f) | 0x80;
      len >>= 7;
    }
  }
  return tag_len;
}

void push_lv_to_hasher(SHA512_CTX &hasher, bssl::Span<const uint8_t> data) {
  uint8_t lv_tag[kMaxLenTagSize];
  size_t lv_tag_len = leb128_tag(data.size(), lv_tag);
  SHA512_Update(&hasher, lv_tag, lv_tag_len);
  SHA512_Update(&hasher, data.data(), data.size());
}
}  // namespace


// This implements the first two steps of G.calculate_generator in Section 8.2
// of draft-irtf-cfrg-cpace-21.
void bssl::CpaceCtx::ComputeGeneratorStrHash(
    uint8_t out_gen_str_hash[32]) const {
  // len_zpad = max(0,s_in_bytes - 1 - len(prepend_len(PRS)) -
  //                len(prepend_len(DSI)))
  size_t digest_len = leb128_len(our_password.size()) + our_password.size()  //
                      + leb128_len(kDSI.length()) + kDSI.length();
  size_t len_zpad = 0;
  if (digest_len < SHA512_CBLOCK - 1) {
    len_zpad = SHA512_CBLOCK - 1 - digest_len;
  }

  constexpr uint8_t kZeros[SHA512_CBLOCK] = {};
  // gen_str = generator_string(G.DSI, PRS, CI, sid, H.s_in_bytes)
  SHA512_CTX hasher;
  SHA512_Init(&hasher);
  push_lv_to_hasher(hasher, StringAsBytes(kDSI));
  push_lv_to_hasher(hasher, our_password);
  push_lv_to_hasher(hasher, Span(kZeros, len_zpad));
  push_lv_to_hasher(hasher, channel_id);
  push_lv_to_hasher(hasher, session_id);
  uint8_t out[SHA512_DIGEST_LENGTH];
  // gen_str_hash = H.hash(gen_str, G.field_size_bytes).
  SHA512_Final(out, &hasher);
  OPENSSL_memcpy(out_gen_str_hash, out, 32);
}

bool bssl::CpaceCtx::ComputeMessage(uint8_t msg[CPACE_MSG_LEN]) {
  if (state != cpace_state_init) {
    OPENSSL_PUT_ERROR(CRYPTO, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return false;
  }

  uint8_t gen_str_hash[32];
  // We now compute the first two steps of G.calculate_generator(H, PRS, sid,
  // CI).
  ComputeGeneratorStrHash(gen_str_hash);
  // This result is then considered as a field coordinate using the u =
  // decodeUCoordinate(gen_str_hash, G.field_size_bits) function from RFC7748.
  uint8_t g[32], v[32];
  map_to_curve_curve25519_elligator2(g, v, gen_str_hash);
  // sample_scalar() = sample_random_bytes(G.field_size_bytes)
  RAND_bytes(our_private_key, 32);
  auto rc = X25519(our_public_key, our_private_key, g);
  OPENSSL_memcpy(msg, our_public_key, 32);
  if (!rc) {
    // This will happen with negligible probability.
    OPENSSL_PUT_ERROR(CRYPTO, ERR_R_INTERNAL_ERROR);
    return false;
  }
  state = cpace_state_msg_generated;
  return true;
}

int CPACE_generate_msg(CPACE_CTX *ctx, uint8_t out_msg[CPACE_MSG_LEN]) {
  auto *impl = bssl::FromOpaque(ctx);
  return impl->ComputeMessage(out_msg);
}

bool bssl::CpaceCtx::ComputeISK(
    const uint8_t peer_msg[CPACE_MSG_LEN], Span<const uint8_t> peer_assoc_data,
    uint8_t out_shared_secret[CPACE_SHARED_SECRET_LEN],
    uint8_t out_sid[SHA512_DIGEST_LENGTH]) {
  if (state != cpace_state_msg_generated) {
    OPENSSL_PUT_ERROR(CRYPTO, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return false;
  }

  state = cpace_state_key_generated;

  uint8_t k[32];
  if (X25519(k, our_private_key, peer_msg) != 1) {
    // TODO(crbug.com/42290066): This is another instance where unifying the
    // reason code and library code into one error code makes most sense.
    OPENSSL_PUT_ERROR(EC, EC_R_DECODE_ERROR);
    return false;
  }
  // ISK = H.hash(lv_cat(G.DSI || b"_ISK", sid, K)||transcript(Ya,ADa,Yb,ADb))
  static constexpr std::string_view kDsiIsk = "CPace255_ISK";
  static constexpr std::string_view kDsiSidOut = "CPaceSidOutput";
  SHA512_CTX hasher, sid_hasher;
  SHA512_Init(&hasher);
  const bool compute_sid = out_sid != nullptr;
  if (compute_sid) {
    SHA512_Init(&sid_hasher);
    SHA512_Update(&sid_hasher, kDsiSidOut.data(), kDsiSidOut.size());
  }
  push_lv_to_hasher(hasher, StringAsBytes(kDsiIsk));
  push_lv_to_hasher(hasher, session_id);
  push_lv_to_hasher(hasher, Span(k, 32));
  Span peer_public_key(peer_msg, 32);
  auto push_our_transcript = [&] {
    push_lv_to_hasher(hasher, our_public_key);
    push_lv_to_hasher(hasher, our_aad);
    if (compute_sid) {
      push_lv_to_hasher(sid_hasher, our_public_key);
      push_lv_to_hasher(sid_hasher, our_aad);
    }
  };
  auto push_peer_transcript = [&] {
    push_lv_to_hasher(hasher, peer_public_key);
    push_lv_to_hasher(hasher, peer_assoc_data);
    if (compute_sid) {
      push_lv_to_hasher(sid_hasher, peer_public_key);
      push_lv_to_hasher(sid_hasher, peer_assoc_data);
    }
  };

  switch (our_role) {
    case cpace_role_initiator: {
      push_our_transcript();
      push_peer_transcript();
      break;
    }
    case cpace_role_responder: {
      push_peer_transcript();
      push_our_transcript();
      break;
    }
  }
  if (compute_sid) {
    SHA512_Final(out_sid, &sid_hasher);
  }
  return SHA512_Final(out_shared_secret, &hasher);
}

// CPACE_process_msg processes a received message and finalize
int CPACE_process_msg(CPACE_CTX *ctx, const uint8_t msg[CPACE_MSG_LEN],
                      const uint8_t *peer_assoc_data,
                      size_t peer_assoc_data_len,
                      uint8_t out_shared_secret[CPACE_SHARED_SECRET_LEN],
                      uint8_t out_sid[SHA512_DIGEST_LENGTH]) {
  auto *impl = bssl::FromOpaque(ctx);
  return impl->ComputeISK(msg, bssl::Span(peer_assoc_data, peer_assoc_data_len),
                          out_shared_secret, out_sid);
}
