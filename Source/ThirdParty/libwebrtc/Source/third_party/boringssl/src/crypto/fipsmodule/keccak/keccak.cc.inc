// Copyright 2023 The BoringSSL Authors
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
#include <stdlib.h>

#include "../../internal.h"
#include "./internal.h"


using namespace bssl;

// keccak_f implements the Keccak-1600 permutation as described at
// https://keccak.team/keccak_specs_summary.html. Each lane is represented as a
// 64-bit value and the 5×5 lanes are stored as an array in row-major order.
//
// To support vectorization, U64 shall either be uint64_t, or an uint64_t-based
// vector type.
template <typename U64, U64 (*rotl)(U64 value, int shift) = CRYPTO_rotl_u64>
static void keccak_f(U64 state[25]) {
  static const int kNumRounds = 24;
  for (int round = 0; round < kNumRounds; round++) {
    // θ step
    U64 c[5];
    for (int x = 0; x < 5; x++) {
      c[x] = state[x] ^ state[x + 5] ^ state[x + 10] ^ state[x + 15] ^
             state[x + 20];
    }

    for (int x = 0; x < 5; x++) {
      const U64 d = c[(x + 4) % 5] ^ rotl(c[(x + 1) % 5], 1);
      for (int y = 0; y < 5; y++) {
        state[y * 5 + x] ^= d;
      }
    }

    // ρ and π steps.
    //
    // These steps involve a mapping of the state matrix. Each input point,
    // (x,y), is rotated and written to the point (y, 2x + 3y). In the Keccak
    // pseudo-code a separate array is used because an in-place operation would
    // overwrite some values that are subsequently needed. However, the mapping
    // forms a trail through 24 of the 25 values so we can do it in place with
    // only a single temporary variable.
    //
    // Start with (1, 0). The value here will be mapped and end up at (0, 2).
    // That value will end up at (2, 1), then (1, 2), and so on. After 24
    // steps, 24 of the 25 values have been hit (as this mapping is injective)
    // and the sequence will repeat. All that remains is to handle the element
    // at (0, 0), but the rotation for that element is zero, and it goes to (0,
    // 0), so we can ignore it.
    U64 prev_value = state[1];
#define PI_RHO_STEP(index, rotation)              \
  do {                                            \
    const U64 value = rotl(prev_value, rotation); \
    prev_value = state[index];                    \
    state[index] = value;                         \
  } while (0)

    PI_RHO_STEP(10, 1);
    PI_RHO_STEP(7, 3);
    PI_RHO_STEP(11, 6);
    PI_RHO_STEP(17, 10);
    PI_RHO_STEP(18, 15);
    PI_RHO_STEP(3, 21);
    PI_RHO_STEP(5, 28);
    PI_RHO_STEP(16, 36);
    PI_RHO_STEP(8, 45);
    PI_RHO_STEP(21, 55);
    PI_RHO_STEP(24, 2);
    PI_RHO_STEP(4, 14);
    PI_RHO_STEP(15, 27);
    PI_RHO_STEP(23, 41);
    PI_RHO_STEP(19, 56);
    PI_RHO_STEP(13, 8);
    PI_RHO_STEP(12, 25);
    PI_RHO_STEP(2, 43);
    PI_RHO_STEP(20, 62);
    PI_RHO_STEP(14, 18);
    PI_RHO_STEP(22, 39);
    PI_RHO_STEP(9, 61);
    PI_RHO_STEP(6, 20);
    PI_RHO_STEP(1, 44);

#undef PI_RHO_STEP

    // χ step
    for (int y = 0; y < 5; y++) {
      const int row_index = 5 * y;
      const U64 orig_x0 = state[row_index];
      const U64 orig_x1 = state[row_index + 1];
      state[row_index] ^= ~orig_x1 & state[row_index + 2];
      state[row_index + 1] ^= ~state[row_index + 2] & state[row_index + 3];
      state[row_index + 2] ^= ~state[row_index + 3] & state[row_index + 4];
      state[row_index + 3] ^= ~state[row_index + 4] & orig_x0;
      state[row_index + 4] ^= ~orig_x0 & orig_x1;
    }

    // ι step
    //
    // From https://keccak.team/files/Keccak-reference-3.0.pdf, section
    // 1.2, the round constants are based on the output of a LFSR. Thus, as
    // suggested in the appendix of of
    // https://keccak.team/keccak_specs_summary.html, the values are
    // simply encoded here.
    static const uint64_t kRoundConstants[24] = {
        0x0000000000000001, 0x0000000000008082, 0x800000000000808a,
        0x8000000080008000, 0x000000000000808b, 0x0000000080000001,
        0x8000000080008081, 0x8000000000008009, 0x000000000000008a,
        0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
        0x000000008000808b, 0x800000000000008b, 0x8000000000008089,
        0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
        0x000000000000800a, 0x800000008000000a, 0x8000000080008081,
        0x8000000000008080, 0x0000000080000001, 0x8000000080008008,
    };

    state[0] ^= kRoundConstants[round];
  }
}

#if defined(HAVE_KECCAK_X2)
typedef uint64_t v2u64 __attribute__((vector_size(16)));

static inline v2u64 v_rotl(v2u64 x, int shift) {
  return (x << shift) | (x >> ((-shift) & 63));
}

static void keccak_f_x2(uint64_t state0[25], uint64_t state1[25]) {
  v2u64 s[25];
  for (int i = 0; i < 25; i++) {
    s[i] = (v2u64){state0[i], state1[i]};
  }
  keccak_f<v2u64, v_rotl>(s);
  for (int i = 0; i < 25; i++) {
    state0[i] = s[i][0];
    state1[i] = s[i][1];
  }
}
#endif

static void keccak_init(struct BORINGSSL_keccak_st *ctx,
                        enum boringssl_keccak_config_t config) {
  size_t required_out_len;
  size_t capacity_bytes;
  switch (config) {
    case boringssl_sha3_256:
      capacity_bytes = 512 / 8;
      required_out_len = 32;
      break;
    case boringssl_sha3_512:
      capacity_bytes = 1024 / 8;
      required_out_len = 64;
      break;
    case boringssl_shake128:
      capacity_bytes = 256 / 8;
      required_out_len = 0;
      break;
    case boringssl_shake256:
      capacity_bytes = 512 / 8;
      required_out_len = 0;
      break;
    default:
      abort();
  }

  OPENSSL_memset(ctx, 0, sizeof(*ctx));
  ctx->config = config;
  ctx->phase = boringssl_keccak_phase_absorb;
  ctx->required_out_len = required_out_len;
  ctx->rate_bytes = 200 - capacity_bytes;
  assert(ctx->rate_bytes % 8 == 0);
}

void bssl::BORINGSSL_keccak(uint8_t *out, size_t out_len, const uint8_t *in,
                            size_t in_len,
                            enum boringssl_keccak_config_t config) {
  struct BORINGSSL_keccak_st ctx;
  BORINGSSL_keccak_init(&ctx, config);
  if (ctx.required_out_len != 0 && out_len != ctx.required_out_len) {
    abort();
  }
  BORINGSSL_keccak_absorb(&ctx, in, in_len);
  BORINGSSL_keccak_squeeze(&ctx, out, out_len);
}

#if defined(HAVE_KECCAK_X2)
void bssl::BORINGSSL_keccak_short_x2(uint8_t *outs[2], size_t out_len,
                                     const uint8_t *ins[2], size_t in_len,
                                     enum boringssl_keccak_config_t config) {
  struct BORINGSSL_keccak_st ctx[2];
  for (size_t i = 0; i < 2; ++i) {
    BORINGSSL_keccak_init(&ctx[i], config);
    if (ctx[i].required_out_len != 0 && out_len != ctx[i].required_out_len) {
      abort();
    }

    // NOTE: this implementation is only efficient if in_len < ctx->rate_bytes,
    // as right now only keccak_f calls in BORINGSSL_keccak_squeeze and
    // keccak_finalize are vectorized. So just fail in every other case for
    // now.
    BSSL_CHECK(in_len < ctx[i].rate_bytes);

    BORINGSSL_keccak_absorb(&ctx[i], ins[i], in_len);
  }
  BORINGSSL_keccak_squeeze_x2(ctx, outs, out_len);
}
#endif

void bssl::BORINGSSL_keccak_init(struct BORINGSSL_keccak_st *ctx,
                                 enum boringssl_keccak_config_t config) {
  keccak_init(ctx, config);
}

void bssl::BORINGSSL_keccak_absorb(struct BORINGSSL_keccak_st *ctx,
                                   const uint8_t *in, size_t in_len) {
  if (ctx->phase == boringssl_keccak_phase_squeeze) {
    // It's illegal to call absorb() again after calling squeeze().
    abort();
  }

  const size_t rate_words = ctx->rate_bytes / 8;
  // XOR the input. Accessing |ctx->state| as a |uint8_t*| is allowed by strict
  // aliasing because we require |uint8_t| to be a character type.
  uint8_t *state_bytes = (uint8_t *)ctx->state;

  // Absorb partial block.
  if (ctx->absorb_offset != 0) {
    assert(ctx->absorb_offset < ctx->rate_bytes);
    size_t first_block_len = ctx->rate_bytes - ctx->absorb_offset;
    for (size_t i = 0; i < first_block_len && i < in_len; i++) {
      state_bytes[ctx->absorb_offset + i] ^= in[i];
    }

    // This input didn't fill the block.
    if (first_block_len > in_len) {
      ctx->absorb_offset += in_len;
      return;
    }

    keccak_f(ctx->state);
    in += first_block_len;
    in_len -= first_block_len;
  }

  // Absorb full blocks.
  while (in_len >= ctx->rate_bytes) {
    for (size_t i = 0; i < rate_words; i++) {
      ctx->state[i] ^= CRYPTO_load_u64_le(in + 8 * i);
    }
    keccak_f(ctx->state);
    in += ctx->rate_bytes;
    in_len -= ctx->rate_bytes;
  }

  // Absorb partial block.
  assert(in_len < ctx->rate_bytes);
  for (size_t i = 0; i < in_len; i++) {
    state_bytes[i] ^= in[i];
  }
  ctx->absorb_offset = in_len;
}

static uint8_t keccak_terminator(struct BORINGSSL_keccak_st *ctx) {
  switch (ctx->config) {
    case boringssl_sha3_256:
    case boringssl_sha3_512:
      return 0x06;
    case boringssl_shake128:
    case boringssl_shake256:
      return 0x1f;
    default:
      abort();
  }
}

static void keccak_finalize(struct BORINGSSL_keccak_st *ctx) {
  // XOR the terminator. Accessing |ctx->state| as a |uint8_t*| is allowed by
  // strict aliasing because we require |uint8_t| to be a character type.
  uint8_t *state_bytes = (uint8_t *)ctx->state;
  state_bytes[ctx->absorb_offset] ^= keccak_terminator(ctx);
  state_bytes[ctx->rate_bytes - 1] ^= 0x80;
  keccak_f(ctx->state);
}

#if defined(HAVE_KECCAK_X2)
static void keccak_finalize_x2(struct BORINGSSL_keccak_st ctx[2]) {
  for (size_t i = 0; i < 2; ++i) {
    // XOR the terminator. Accessing |ctx->state| as a |uint8_t*| is allowed by
    // strict aliasing because we require |uint8_t| to be a character type.
    uint8_t *state_bytes = (uint8_t *)ctx[i].state;
    state_bytes[ctx[i].absorb_offset] ^= keccak_terminator(&ctx[i]);
    state_bytes[ctx[i].rate_bytes - 1] ^= 0x80;
  }
  keccak_f_x2(ctx[0].state, ctx[1].state);
}
#endif

void bssl::BORINGSSL_keccak_squeeze(struct BORINGSSL_keccak_st *ctx,
                                    uint8_t *out, size_t out_len) {
  if (ctx->required_out_len != 0 &&
      (ctx->phase == boringssl_keccak_phase_squeeze ||
       out_len != ctx->required_out_len)) {
    // The SHA-3 variants must be squeezed in a single call, to confirm that the
    // output length is correct.
    abort();
  }

  if (ctx->phase == boringssl_keccak_phase_absorb) {
    keccak_finalize(ctx);
    ctx->phase = boringssl_keccak_phase_squeeze;
  }

  // Accessing |ctx->state| as a |uint8_t*| is allowed by strict aliasing
  // because we require |uint8_t| to be a character type.
  const uint8_t *state_bytes = (const uint8_t *)ctx->state;
  while (out_len) {
    if (ctx->squeeze_offset == ctx->rate_bytes) {
      keccak_f(ctx->state);
      ctx->squeeze_offset = 0;
    }

    size_t remaining = ctx->rate_bytes - ctx->squeeze_offset;
    size_t todo = out_len;
    if (todo > remaining) {
      todo = remaining;
    }
    OPENSSL_memcpy(out, &state_bytes[ctx->squeeze_offset], todo);
    out += todo;
    out_len -= todo;
    ctx->squeeze_offset += todo;
  }
}

#if defined(HAVE_KECCAK_X2)
void bssl::BORINGSSL_keccak_squeeze_x2(struct BORINGSSL_keccak_st ctx[2],
                                       uint8_t *outs[2], size_t out_len) {
  for (size_t i = 0; i < 2; ++i) {
    if (ctx[i].required_out_len != 0 &&
        (ctx[i].phase == boringssl_keccak_phase_squeeze ||
         out_len != ctx[i].required_out_len)) {
      // The SHA-3 variants must be squeezed in a single call, to confirm that
      // the output length is correct.
      abort();
    }
  }

  // These fields are processed in parallel. Everything here uses ctx[0]; at the
  // end changes are mirrored back to ctx[1] just in case.
#define FOR_COMMON_FIELDS(MACRO) \
  MACRO(phase)                   \
  MACRO(config)                  \
  MACRO(absorb_offset)           \
  MACRO(squeeze_offset)          \
  MACRO(rate_bytes)

#define MUST_BE_EQUAL(field) BSSL_CHECK(ctx[0].field == ctx[1].field);
  FOR_COMMON_FIELDS(MUST_BE_EQUAL)
#undef MUST_BE_EQUAL

  if (ctx->phase == boringssl_keccak_phase_absorb) {
    keccak_finalize_x2(ctx);
    ctx->phase = boringssl_keccak_phase_squeeze;
  }

  // Accessing |ctx->state| as a |uint8_t*| is allowed by strict aliasing
  // because we require |uint8_t| to be a character type.
  uint8_t *optr[2] = {outs[0], outs[1]};
  while (out_len) {
    if (ctx->squeeze_offset == ctx->rate_bytes) {
      keccak_f_x2(ctx[0].state, ctx[1].state);
      ctx->squeeze_offset = 0;
    }

    size_t remaining = ctx->rate_bytes - ctx->squeeze_offset;
    size_t todo = out_len;
    if (todo > remaining) {
      todo = remaining;
    }
    for (size_t i = 0; i < 2; ++i) {
      const uint8_t *state_bytes = (const uint8_t *)ctx[i].state;
      OPENSSL_memcpy(optr[i], &state_bytes[ctx->squeeze_offset], todo);
      optr[i] += todo;
    }
    out_len -= todo;
    ctx->squeeze_offset += todo;
  }

#define COPY_FIELD_VALUE(field) ctx[1].field = ctx[0].field;
  FOR_COMMON_FIELDS(COPY_FIELD_VALUE)
#undef COPY_FIELD_VALUE

#undef FOR_COMMON_FIELDS
}
#endif  // HAVE_KECCAK_X2
