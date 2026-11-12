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

#include <openssl/curve25519.h>
#include <openssl/span.h>

#include <optional>
#include <utility>
#include <vector>

#include <stdint.h>
#include <string.h>

#include <gtest/gtest.h>

#include "../internal.h"
#include "../test/test_util.h"
#include "./internal.h"


BSSL_NAMESPACE_BEGIN

namespace {

constexpr uint8_t kPassword[] = {'P', 'a', 's', 's', 'w', 'o', 'r', 'd'};
constexpr uint8_t kChannelId[] = {0x0b, 'A', '_', 'i', 'n',  'i', 't', 'i',
                                  'a',  't', 'o', 'r', 0x0b, 'B', '_', 'r',
                                  'e',  's', 'p', 'o', 'n',  'd', 'e', 'r'};
constexpr uint8_t kAda[] = {'A', 'D', 'a'};
constexpr uint8_t kAdb[] = {'A', 'D', 'b'};
constexpr uint8_t kSid[] = {0x7e, 0x4b, 0x47, 0x91, 0xd6, 0xa8, 0xef, 0x01,
                            0x9b, 0x93, 0x6c, 0x79, 0xfb, 0x7f, 0x2c, 0x57};

struct CPACERun {
  bool Run() {
    UniquePtr<CPACE_CTX> alice(CPACE_CTX_new(
        cpace_role_initiator, alice_password.data(), alice_password.size(),
        alice_channel_id.data(), alice_channel_id.size(), alice_aad.data(),
        alice_aad.size(), alice_sid.data(), alice_sid.size()));
    UniquePtr<CPACE_CTX> bob(CPACE_CTX_new(
        cpace_role_responder, bob_password.data(), bob_password.size(),
        bob_channel_id.data(), bob_channel_id.size(), bob_aad.data(),
        bob_aad.size(), bob_sid.data(), bob_sid.size()));

    if (!alice || !bob) {
      return false;
    }

    uint8_t alice_msg[CPACE_MSG_LEN];
    uint8_t bob_msg[CPACE_MSG_LEN];

    if (!CPACE_generate_msg(alice.get(), alice_msg) ||
        !CPACE_generate_msg(bob.get(), bob_msg)) {
      return false;
    }

    if (alice_corrupt_msg_bit >= 0 &&
        static_cast<size_t>(alice_corrupt_msg_bit) < 8 * sizeof(alice_msg)) {
      alice_msg[alice_corrupt_msg_bit / 8] ^= 1 << (alice_corrupt_msg_bit & 7);
    }

    Span<const uint8_t> alice_recv_aad =
        alice_peer_aad.has_value() ? *alice_peer_aad : bob_aad;
    Span<const uint8_t> bob_recv_aad =
        bob_peer_aad.has_value() ? *bob_peer_aad : alice_aad;

    uint8_t alice_key[64], bob_key[64];
    uint8_t alice_sid_out[SHA512_DIGEST_LENGTH],
        bob_sid_out[SHA512_DIGEST_LENGTH];
    if (!CPACE_process_msg(alice.get(), bob_msg, alice_recv_aad.data(),
                           alice_recv_aad.size(), alice_key,
                           pass_sid_out ? alice_sid_out : nullptr) ||
        !CPACE_process_msg(bob.get(), alice_msg, bob_recv_aad.data(),
                           bob_recv_aad.size(), bob_key,
                           pass_sid_out ? bob_sid_out : nullptr)) {
      return false;
    }

    key_matches_ = (CRYPTO_memcmp(alice_key, bob_key, sizeof(alice_key)) == 0);
    sid_matches_ = !pass_sid_out || (CRYPTO_memcmp(alice_sid_out, bob_sid_out,
                                                   sizeof(alice_sid_out)) == 0);
    return true;
  }

  bool key_matches() const { return key_matches_; }
  bool sid_matches() const { return sid_matches_; }

  Span<const uint8_t> alice_password = kPassword;
  Span<const uint8_t> bob_password = kPassword;
  Span<const uint8_t> alice_channel_id = kChannelId;
  Span<const uint8_t> bob_channel_id = kChannelId;
  Span<const uint8_t> alice_aad = kAda;
  Span<const uint8_t> bob_aad = kAdb;
  std::optional<Span<const uint8_t>> alice_peer_aad;
  std::optional<Span<const uint8_t>> bob_peer_aad;
  Span<const uint8_t> alice_sid = kSid;
  Span<const uint8_t> bob_sid = kSid;
  bool pass_sid_out = true;
  int alice_corrupt_msg_bit = -1;

 private:
  bool key_matches_ = false;
  bool sid_matches_ = false;
};

TEST(CPACETest, Handshake) {
  for (int i = 0; i < 10; i++) {
    CPACERun run;
    ASSERT_TRUE(run.Run());
    EXPECT_TRUE(run.key_matches());
    EXPECT_TRUE(run.sid_matches());
  }
}

TEST(CPACETest, HandshakeNullSidOut) {
  CPACERun run;
  run.pass_sid_out = false;
  ASSERT_TRUE(run.Run());
  EXPECT_TRUE(run.key_matches());
}

TEST(CPACETest, HandshakeVariableAad) {
  std::vector<uint8_t> aad127(127, 0x11);
  std::vector<uint8_t> aad128(128, 0x22);
  std::vector<uint8_t> aad255(255, 0x33);
  std::vector<uint8_t> aad256(256, 0x44);
  std::vector<uint8_t> aad300_0(300, 0x00);
  std::vector<uint8_t> aad300_1(300, 0x01);
  static constexpr uint8_t kA[] = {'A'};
  static constexpr uint8_t kB[] = {'B'};
  static constexpr uint8_t kShort[] = {'s', 'h', 'o', 'r', 't'};
  static constexpr uint8_t kLong[] = {'l', 'o', 'n', 'g', 'e', 'r'};

  std::vector<std::pair<Span<const uint8_t>, Span<const uint8_t>>> aad_pairs = {
      {{}, {}},
      {kA, kB},
      {kB, kA},
      {kShort, kLong},
      {aad127, aad128},
      {aad255, aad256},
      {aad300_0, aad300_1},
  };
  for (const auto &[alice_aad, bob_aad] : aad_pairs) {
    CPACERun run;
    run.alice_aad = alice_aad;
    run.bob_aad = bob_aad;
    ASSERT_TRUE(run.Run());
    EXPECT_TRUE(run.key_matches());
    EXPECT_TRUE(run.sid_matches());
  }
}

TEST(CPACETest, EmptySidAndAad) {
  CPACERun run;
  run.alice_sid = {};
  run.bob_sid = {};
  run.alice_aad = {};
  run.bob_aad = {};
  ASSERT_TRUE(run.Run());
  EXPECT_TRUE(run.key_matches());
  EXPECT_TRUE(run.sid_matches());
}

TEST(CPACETest, WrongPassword) {
  static constexpr uint8_t kWrongPassword[] = {'W', 'r', 'o', 'n', 'g'};
  CPACERun run;
  run.bob_password = kWrongPassword;
  ASSERT_TRUE(run.Run());
  EXPECT_FALSE(run.key_matches()) << "Keys matched with different passwords!";
  EXPECT_TRUE(run.sid_matches());
}

TEST(CPACETest, WrongChannelId) {
  static constexpr uint8_t kWrongChannelId[] = {'W', 'r', 'o', 'n',
                                                'g', 'C', 'I'};
  CPACERun run;
  run.bob_channel_id = kWrongChannelId;
  ASSERT_TRUE(run.Run());
  EXPECT_FALSE(run.key_matches()) << "Keys matched with different channel IDs!";
  EXPECT_TRUE(run.sid_matches());
}

TEST(CPACETest, WrongSessionId) {
  static constexpr uint8_t kWrongSessionId[] = {'O', 't', 'h', 'e',
                                                'r', 'S', 'i', 'd'};
  CPACERun run;
  run.bob_sid = kWrongSessionId;
  ASSERT_TRUE(run.Run());
  EXPECT_FALSE(run.key_matches()) << "Keys matched with different session IDs!";
  EXPECT_TRUE(run.sid_matches());
}

TEST(CPACETest, WrongAssociatedData) {
  static constexpr uint8_t kTamperedAad[] = {'B', 'l', 'a', 'h'};
  CPACERun run;
  run.alice_peer_aad = kTamperedAad;
  ASSERT_TRUE(run.Run());
  EXPECT_FALSE(run.key_matches())
      << "Keys matched with tampered associated data!";
  EXPECT_FALSE(run.sid_matches());
}

TEST(CPACETest, CorruptedMessage) {
  for (int i = 0; i < 8 * 32; i += 7) {
    CPACERun run;
    run.alice_corrupt_msg_bit = i;
    // Corrupted message should either fail during X25519 (if point of small
    // order) or produce non-matching keys.
    if (run.Run()) {
      EXPECT_FALSE(run.key_matches())
          << "Key matched after corrupting Alice's message, bit " << i;
      EXPECT_FALSE(run.sid_matches())
          << "SID matched after corrupting Alice's message, bit " << i;
    }
  }
}

TEST(CPACETest, StateMachine) {
  uint8_t msg[32];
  uint8_t key[64];
  uint8_t sid[SHA512_DIGEST_LENGTH];

  UniquePtr<CPACE_CTX> ctx(CPACE_CTX_new(
      cpace_role_initiator, kPassword, sizeof(kPassword), kChannelId,
      sizeof(kChannelId), kAda, sizeof(kAda), nullptr, 0));
  ASSERT_TRUE(ctx);

  // Cannot process before generating message
  uint8_t dummy_peer_msg[32] = {0};
  EXPECT_FALSE(
      CPACE_process_msg(ctx.get(), dummy_peer_msg, nullptr, 0, key, sid));

  // Generate message
  EXPECT_TRUE(CPACE_generate_msg(ctx.get(), msg));

  // Cannot send message again
  EXPECT_FALSE(CPACE_generate_msg(ctx.get(), msg));

  // Process message from peer (valid point)
  uint8_t valid_peer_scalar[32] = {1};
  uint8_t valid_peer_msg[32];
  X25519_public_from_private(valid_peer_msg, valid_peer_scalar);
  EXPECT_TRUE(
      CPACE_process_msg(ctx.get(), valid_peer_msg, nullptr, 0, key, sid));

  // Cannot process message again (single-use)
  EXPECT_FALSE(
      CPACE_process_msg(ctx.get(), valid_peer_msg, nullptr, 0, key, sid));
}

void RunExchange(Span<const uint8_t> ya_priv, Span<const uint8_t> yb_priv,
                 Span<const uint8_t> ya_msg, Span<const uint8_t> yb_msg,
                 Span<const uint8_t> expected_isk,
                 Span<const uint8_t> expected_sid_out,
                 Span<const uint8_t> sid = kSid) {
  UniquePtr<CPACE_CTX> alice(CPACE_CTX_new(
      cpace_role_initiator, kPassword, sizeof(kPassword), kChannelId,
      sizeof(kChannelId), kAda, sizeof(kAda), sid.data(), sid.size()));
  UniquePtr<CPACE_CTX> bob(CPACE_CTX_new(
      cpace_role_responder, kPassword, sizeof(kPassword), kChannelId,
      sizeof(kChannelId), kAdb, sizeof(kAdb), sid.data(), sid.size()));
  ASSERT_TRUE(alice);
  ASSERT_TRUE(bob);

  auto *alice_impl = FromOpaque(alice.get());
  auto *bob_impl = FromOpaque(bob.get());

  if (!ya_priv.empty()) {
    OPENSSL_memcpy(alice_impl->our_private_key, ya_priv.data(), 32);
  }
  OPENSSL_memcpy(alice_impl->our_public_key, ya_msg.data(), 32);
  alice_impl->state = CpaceCtx::cpace_state_msg_generated;

  if (!yb_priv.empty()) {
    OPENSSL_memcpy(bob_impl->our_private_key, yb_priv.data(), 32);
  }
  OPENSSL_memcpy(bob_impl->our_public_key, yb_msg.data(), 32);
  bob_impl->state = CpaceCtx::cpace_state_msg_generated;

  uint8_t alice_isk[64], bob_isk[64];
  uint8_t alice_sid_out[SHA512_DIGEST_LENGTH],
      bob_sid_out[SHA512_DIGEST_LENGTH];
  ASSERT_TRUE(CPACE_process_msg(alice.get(), yb_msg.data(), kAdb, sizeof(kAdb),
                                alice_isk, alice_sid_out));
  ASSERT_TRUE(CPACE_process_msg(bob.get(), ya_msg.data(), kAda, sizeof(kAda),
                                bob_isk, bob_sid_out));

  if (!expected_isk.empty()) {
    EXPECT_EQ(Bytes(alice_isk), Bytes(expected_isk));
    EXPECT_EQ(Bytes(bob_isk), Bytes(expected_isk));
  }
  EXPECT_EQ(Bytes(alice_sid_out), Bytes(bob_sid_out));
  if (!expected_sid_out.empty()) {
    EXPECT_EQ(Bytes(alice_sid_out), Bytes(expected_sid_out));
    EXPECT_EQ(Bytes(bob_sid_out), Bytes(expected_sid_out));
  }
}

// Test vector from draft-irtf-cfrg-cpace Appendix B.1.1 - B.1.5
TEST(CPACETest, RFCVectors) {
  std::vector<uint8_t> ya, yb, ya_msg, yb_msg, expected_isk_ordered;
  ASSERT_TRUE(
      DecodeHex(&ya,
                "21b4f4bd9e64ed355c3eb676a28ebedaf6d8f17bdc365995b3190971"
                "53044080"));
  ASSERT_TRUE(
      DecodeHex(&yb,
                "848b0779ff415f0af4ea14df9dd1d3c29ac41d836c7808896c4eba19"
                "c51ac40a"));
  ASSERT_TRUE(
      DecodeHex(&ya_msg,
                "1d13c89278cdadd826f6d8d7f887701430f8380ddc17611cdd6dc989"
                "ce0c9f32"));
  ASSERT_TRUE(
      DecodeHex(&yb_msg,
                "248cccf6d5cdc3646f0ad593f9e6cef4e69d4945f8372e623512ecea"
                "32185623"));
  ASSERT_TRUE(
      DecodeHex(&expected_isk_ordered,
                "6e19b875f7a561d6b3ca3dbb9ef42ac55de3e717881018204b8922b4"
                "d5e53bb2aa82c300bea7b65d2b671da71922ddf6472301b79bc270ad"
                "fa8bf413285f2263"));

#if !defined(BORINGSSL_SHARED_LIBRARY)
  std::vector<uint8_t> expected_g;
  ASSERT_TRUE(
      DecodeHex(&expected_g,
                "d04bf6d41f6a289632a2e929fa29bebd51092512a7829fdde7d314b6"
                "2f05a73f"));

  // 1. Generator and message derivation (Appendix B.1.1 - B.1.3)
  UniquePtr<CPACE_CTX> ctx(CPACE_CTX_new(
      cpace_role_initiator, kPassword, sizeof(kPassword), kChannelId,
      sizeof(kChannelId), nullptr, 0, kSid, sizeof(kSid)));
  ASSERT_TRUE(ctx);
  auto *impl = FromOpaque(ctx.get());

  uint8_t u[32], g[32], v[32];
  impl->ComputeGeneratorStrHash(u);
  map_to_curve_curve25519_elligator2(g, v, u);
  EXPECT_EQ(Bytes(g), Bytes(expected_g));

  uint8_t computed_ya_msg[32], computed_yb_msg[32];
  ASSERT_TRUE(X25519(computed_ya_msg, ya.data(), g));
  EXPECT_EQ(Bytes(computed_ya_msg), Bytes(ya_msg));
  ASSERT_TRUE(X25519(computed_yb_msg, yb.data(), g));
  EXPECT_EQ(Bytes(computed_yb_msg), Bytes(yb_msg));
#endif

  // 2. Exchange (Appendix B.1.5)
  RunExchange(ya, yb, ya_msg, yb_msg, expected_isk_ordered, {});
}

// Test vector from draft-irtf-cfrg-cpace Appendix B.1.7 (Optional output of
// session id)
TEST(CPACETest, RFCSidOutput) {
  std::vector<uint8_t> Ya, Yb, expected_sid_out_ir;
  ASSERT_TRUE(
      DecodeHex(&Ya,
                "1d13c89278cdadd826f6d8d7f887701430f8380ddc17611cdd6dc989"
                "ce0c9f32"));
  ASSERT_TRUE(
      DecodeHex(&Yb,
                "248cccf6d5cdc3646f0ad593f9e6cef4e69d4945f8372e623512ecea"
                "32185623"));
  ASSERT_TRUE(
      DecodeHex(&expected_sid_out_ir,
                "cbc73f62589bbc96ab6a95ec2363df621e93bc3b0cea83ba6b9571d0"
                "05fa8f5d2d08f7165622777fa484c02a9e6b20a84ee2dbebae8c53be"
                "757dcfc0eebdeb5f"));

  // transcript_ir with empty sid
  RunExchange({}, {}, Ya, Yb, {}, expected_sid_out_ir, {});
}

// Test vector from draft-irtf-cfrg-cpace Appendix B.1.10 (low order points)
TEST(CPACETest, RFCLowOrderPoints) {
  struct LowOrderVector {
    const char *u_hex;
    const char *q_hex;
    bool should_abort;
  };

  const LowOrderVector kVectors[] = {
      {"0000000000000000000000000000000000000000000000000000000000000000",
       "0000000000000000000000000000000000000000000000000000000000000000",
       true},
      {"0100000000000000000000000000000000000000000000000000000000000000",
       "0000000000000000000000000000000000000000000000000000000000000000",
       true},
      {"ecffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
       "0000000000000000000000000000000000000000000000000000000000000000",
       true},
      {"e0eb7a7c3b41b8ae1656e3faf19fc46ada098deb9c32b1fd866205165f49b800",
       "0000000000000000000000000000000000000000000000000000000000000000",
       true},
      {"5f9c95bca3508c24b1d0b1559c83ef5b04445cc4581c8e86d8224eddd09f1157",
       "0000000000000000000000000000000000000000000000000000000000000000",
       true},
      {"edffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
       "0000000000000000000000000000000000000000000000000000000000000000",
       true},
      {"daffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
       "d8e2c776bbacd510d09fd9278b7edcd25fc5ae9adfba3b6e040e8d3b71b21806",
       false},
      {"eeffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
       "0000000000000000000000000000000000000000000000000000000000000000",
       true},
      {"dbffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
       "c85c655ebe8be44ba9c0ffde69f2fe10194458d137f09bbff725ce58803cdb38",
       false},
      {"d9ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
       "db64dafa9b8fdd136914e61461935fe92aa372cb056314e1231bc4ec12417456",
       false},
      {"cdeb7a7c3b41b8ae1656e3faf19fc46ada098deb9c32b1fd866205165f49b880",
       "e062dcd5376d58297be2618c7498f55baa07d7e03184e8aada20bca28888bf7a",
       false},
      {"4c9c95bca3508c24b1d0b1559c83ef5b04445cc4581c8e86d8224eddd09f11d7",
       "993c6ad11c4c29da9a56f7691fd0ff8d732e49de6250b6c2e80003ff4629a175",
       false},
  };

  std::vector<uint8_t> s;
  ASSERT_TRUE(DecodeHex(
      &s, "af46e36bf0527c9d3b16154b82465edd62144c0ac1fc5a18506a2244ba449aff"));

  for (size_t i = 0; i < std::size(kVectors); i++) {
    SCOPED_TRACE(i);
    const auto &vec = kVectors[i];
    std::vector<uint8_t> u, expected_q;
    ASSERT_TRUE(DecodeHex(&u, vec.u_hex));
    ASSERT_TRUE(DecodeHex(&expected_q, vec.q_hex));

    // Protocol abort verification: when included in message from peer,
    // `CPACE_process_msg` must abort if `should_abort` is true.
    UniquePtr<CPACE_CTX> ctx(
        CPACE_CTX_new(cpace_role_initiator, kPassword, sizeof(kPassword),
                      kChannelId, sizeof(kChannelId), nullptr, 0, nullptr, 0));
    ASSERT_TRUE(ctx);

    uint8_t our_msg[CPACE_MSG_LEN];
    ASSERT_TRUE(CPACE_generate_msg(ctx.get(), our_msg));

    uint8_t shared_secret[CPACE_SHARED_SECRET_LEN];
    uint8_t sid_out[SHA512_DIGEST_LENGTH];
    int process_res = CPACE_process_msg(ctx.get(), u.data(), nullptr, 0,
                                        shared_secret, sid_out);
    EXPECT_EQ(process_res, !vec.should_abort);
  }
}

}  // namespace
BSSL_NAMESPACE_END
