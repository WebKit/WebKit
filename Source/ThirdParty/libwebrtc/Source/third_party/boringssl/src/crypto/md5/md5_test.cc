// Copyright 2018 The BoringSSL Authors
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

#include <openssl/md5.h>

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "../test/abi_test.h"
#include "../test/test_util.h"
#include "internal.h"


BSSL_NAMESPACE_BEGIN
namespace {

#if defined(MD5_ASM) && defined(SUPPORTS_ABI_TEST)
TEST(MD5Test, ABI) {
  MD5_CTX ctx;
  MD5_Init(&ctx);

  static const uint8_t kBuf[MD5_CBLOCK * 8] = {0};
  CHECK_ABI(md5_block_asm_data_order, ctx.h, kBuf, 1);
  CHECK_ABI(md5_block_asm_data_order, ctx.h, kBuf, 2);
  CHECK_ABI(md5_block_asm_data_order, ctx.h, kBuf, 4);
  CHECK_ABI(md5_block_asm_data_order, ctx.h, kBuf, 8);
}
#endif  // MD5_ASM && SUPPORTS_ABI_TEST

// Validate that computing MD5 at once or in 2 steps with a context
// serialization in-between provide the same result.
TEST(MD5Test, MarshalAndParseStateRoundTrip) {
  // Prepare an input, pseudo-random content doesn't matter much.
  std::vector<uint8_t> input(200);
  for (size_t i = 0; i < input.size(); i++) {
    input[i] = static_cast<uint8_t>(i * 37 + 13);
  }

  // Compute MD5 on whole input at once.
  uint8_t expected_digest[MD5_DIGEST_LENGTH];
  MD5(input.data(), input.size(), expected_digest);

  // Test split position at every possible place between 0 and input size.
  // The input size of 200 guarantees covering at least 2*MD5_CBLOCK (==2*64)
  // plus some margin.
  for (size_t split = 0; split <= input.size(); split++) {
    SCOPED_TRACE(split);

    // Compute MD5 on first part of the split.
    MD5_CTX ctx;
    ASSERT_EQ(MD5_Init(&ctx), 1);
    ASSERT_EQ(MD5_Update(&ctx, input.data(), split), 1);

    // Serialize the state.
    uint8_t state[MD5_STATE_MAX_BYTES];
    size_t state_len = 0;
    ASSERT_EQ(MD5_marshal_state(&ctx, state, &state_len), 1);
    EXPECT_EQ(state_len, 24 + (split % MD5_CBLOCK));

    // Restore the state and add the second part of the split.
    MD5_CTX restored;
    ASSERT_EQ(MD5_parse_state(&restored, state, state_len), 1);
    ASSERT_EQ(MD5_Update(&restored, input.data() + split, input.size() - split),
              1);

    // Verify that both MD5 computed match.
    uint8_t actual_digest[MD5_DIGEST_LENGTH];
    ASSERT_EQ(MD5_Final(actual_digest, &restored), 1);
    EXPECT_EQ(Bytes(actual_digest, MD5_DIGEST_LENGTH),
              Bytes(expected_digest, MD5_DIGEST_LENGTH));
  }
}

// Verify that the serialization doesn't change.
// Because serialization is used to compute MD5 across job updates, it's
// important that it remains backward and forward compatible.
TEST(MD5Test, MarshalAndParseStateWireFormat) {
  struct TestVector {
    const char *name;
    std::string input;
    size_t input_repeat;
    const char *expected_state_hex;
    const char *expected_digest_hex;
    std::string continuation;
    const char *expected_continuation_digest_hex;
  };
  const TestVector kTestVectors[] = {
      {
          "EmptyInput",
          "",
          1,
          "0123456789abcdeffedcba98765432100000000000000000",
          "d41d8cd98f00b204e9800998ecf8427e",
          "Hello, World!",
          "65a8e27d8879283831b664bd8b7f0ad4",
      },
      {
          "ShortInput_abc",
          "abc",
          1,
          "0123456789abcdeffedcba98765432101800000000000000616263",
          "900150983cd24fb0d6963f7d28e17f72",
          "xyz",
          "70fb874a43097a25234382390c0baeb3",
      },
      {
          // 55 bytes (440 bits) + 1 byte pad (0x80) + 8 bytes length = 64B.
          "Exact_1block_55bytes",
          "1234567890123456789012345678901234567890123456789012345",
          1,
          "0123456789abcdeffedcba9876543210b801000000000000"
          "31323334353637383930313233343536373839303132333435"
          "36373839303132333435363738393031323334353637383930"
          "3132333435",
          "c9ccf168914a1bcfc3229f1948e67da0",
          "xyz",
          "fb463c0c8c6dd9330623d22e764b0c71",
      },
      {
          // 56 bytes (448 bits) + 1 byte pad = 456 bits > 448; forces 2nd block
          // during finalization.
          "PaddingSpill_56bytes",
          "12345678901234567890123456789012345678901234567890123456",
          1,
          "0123456789abcdeffedcba9876543210c001000000000000"
          "31323334353637383930313233343536373839303132333435"
          "36373839303132333435363738393031323334353637383930"
          "313233343536",
          "49f193adce178490e34d1b3a4ec0064c",
          "xyz",
          "f2d4136679b29ddb872955d2aa8e8484",
      },
      {
          "OneByteBeforeBlockBoundary_63bytes",
          std::string(63, 'a'),
          1,
          "0123456789abcdeffedcba9876543210f801000000000000"
          "6161616161616161616161616161616161616161616161616161616161616161"
          "61616161616161616161616161616161616161616161616161616161616161",
          "b06521f39153d618550606be297466d5",
          "xyz",
          "d6ac91844477aed9d6a74c4451ddb443",
      },
      {
          // State transformed, count=512 bits (0x200), buffer empty (0 bytes).
          "ExactBlockBoundary_64bytes",
          std::string(64, 'a'),
          1,
          "56ffd48962d95c1233deca6925e333000002000000000000",
          "014842d480b571495a4a0363793f7367",
          "xyz",
          "d953efb81c869c70ee05f9c2e1bf0123",
      },
      {
          "OneBytePastBlockBoundary_65bytes",
          std::string(65, 'a'),
          1,
          "56ffd48962d95c1233deca6925e33300080200000000000061",
          "c743a45e0d2e6a95cb859adae0248435",
          "xyz",
          "6f722413acec8914bdb1816a4b73f6c7",
      },
      {
          "ExactTwoBlocksBoundary_128bytes",
          std::string(128, 'a'),
          1,
          "d7991be0c609e6a1ea1dd2fe4f6b6bbe0004000000000000",
          "e510683b3f5ffe4093d021808bc6ff70",
          "xyz",
          "514e7a94db9f189e947ec22967f9b6d2",
      },
      {
          // 40 bytes followed by 24 bytes flushes on the 64-byte block
          // boundary.
          "MultiStepUpdateFlushingOnBlockBoundary",
          std::string(40, 'a'),
          1,
          "0123456789abcdeffedcba98765432104001000000000000"
          "6161616161616161616161616161616161616161616161616161616161616161"
          "6161616161616161",
          "4697843037d962f62a5a429e611e0f5f",
          std::string(24, 'a'),
          "014842d480b571495a4a0363793f7367",
      },
      {
          // An input bigger than 512 MiB (536 MiB = 8576 * 64 KiB) requires
          // more than 32 bits for the bit counter, ensuring the full 64-bit
          // counter (Nh and Nl) is marshaled and parsed.
          "MoreThan32bits",
          std::string(64 * 1024, 'a'),
          536 * 16,
          "918ddea3ed0fb037455bbece751ff1ca0000000c01000000",
          "f156cbc99685a811424ff1d18255fd23",
          "xyz",
          "2004333d9e2dc522d21d32ecf4893fc1",
      },
  };

  for (const auto &tv : kTestVectors) {
    SCOPED_TRACE(tv.name);

    // Compute MD5 on `input` (repeated `input_repeat` times).
    MD5_CTX ctx;
    ASSERT_EQ(MD5_Init(&ctx), 1);
    for (size_t i = 0; i < tv.input_repeat; i++) {
      ASSERT_EQ(MD5_Update(&ctx, tv.input.data(), tv.input.size()), 1);
    }

    uint8_t state[MD5_STATE_MAX_BYTES];
    size_t state_len = 0;
    ASSERT_EQ(MD5_marshal_state(&ctx, state, &state_len), 1);

    // Verify that serialized context is as expected (`expected_state_hex`).
    std::vector<uint8_t> expected_state;
    ASSERT_TRUE(DecodeHex(&expected_state, tv.expected_state_hex));
    EXPECT_EQ(Bytes(state, state_len), Bytes(expected_state));

    // Verify that finalizing `ctx` produces `expected_digest_hex`.
    uint8_t digest[MD5_DIGEST_LENGTH];
    ASSERT_EQ(MD5_Final(digest, &ctx), 1);
    std::vector<uint8_t> expected_digest;
    ASSERT_TRUE(DecodeHex(&expected_digest, tv.expected_digest_hex));
    EXPECT_EQ(Bytes(digest, MD5_DIGEST_LENGTH), Bytes(expected_digest));

    // Verify that parsing `expected_state` and finalizing without further
    // updates also produces `expected_digest_hex` and re-marshals identically.
    MD5_CTX restored;
    ASSERT_EQ(MD5_parse_state(&restored, expected_state.data(),
                              expected_state.size()),
              1);
    uint8_t remarshaled_state[MD5_STATE_MAX_BYTES];
    size_t remarshaled_len = 0;
    ASSERT_EQ(MD5_marshal_state(&restored, remarshaled_state, &remarshaled_len),
              1);
    EXPECT_EQ(Bytes(remarshaled_state, remarshaled_len), Bytes(expected_state));
    ASSERT_EQ(MD5_Final(digest, &restored), 1);
    EXPECT_EQ(Bytes(digest, MD5_DIGEST_LENGTH), Bytes(expected_digest));

    // Restore context again and extend computed MD5 with `continuation`.
    ASSERT_EQ(MD5_parse_state(&restored, expected_state.data(),
                              expected_state.size()),
              1);
    ASSERT_EQ(
        MD5_Update(&restored, tv.continuation.data(), tv.continuation.size()),
        1);

    // Verify that the resulting digest is `expected_continuation_digest_hex`.
    ASSERT_EQ(MD5_Final(digest, &restored), 1);
    std::vector<uint8_t> expected_continuation_digest;
    ASSERT_TRUE(DecodeHex(&expected_continuation_digest,
                          tv.expected_continuation_digest_hex));
    EXPECT_EQ(Bytes(digest, MD5_DIGEST_LENGTH),
              Bytes(expected_continuation_digest));
  }
}

TEST(MD5Test, ParseStateInvalid) {
  MD5_CTX ctx;
  ASSERT_EQ(MD5_Init(&ctx), 1);
  ASSERT_EQ(MD5_Update(&ctx, "abc", 3), 1);
  uint8_t initial_state[MD5_STATE_MAX_BYTES];
  size_t initial_len = 0;
  ASSERT_EQ(MD5_marshal_state(&ctx, initial_state, &initial_len), 1);

  auto is_unmodified = [&]() {
    uint8_t cur_state[MD5_STATE_MAX_BYTES];
    size_t cur_len = 0;
    return MD5_marshal_state(&ctx, cur_state, &cur_len) == 1 &&
           Bytes(cur_state, cur_len) == Bytes(initial_state, initial_len);
  };

  MD5_CTX valid_ctx;
  uint8_t state[MD5_STATE_MAX_BYTES + 1] = {0};

  // Too short (< 24 bytes).
  EXPECT_EQ(MD5_parse_state(&ctx, state, 0), 0);
  EXPECT_TRUE(is_unmodified());
  EXPECT_EQ(MD5_parse_state(&ctx, state, 23), 0);
  EXPECT_TRUE(is_unmodified());

  // Too long (> MD5_STATE_MAX_BYTES).
  EXPECT_EQ(MD5_parse_state(&ctx, state, MD5_STATE_MAX_BYTES + 1), 0);
  EXPECT_TRUE(is_unmodified());

  // Valid 24-byte state with Nl = 0, Nh = 0.
  EXPECT_EQ(MD5_parse_state(&valid_ctx, state, 24), 1);

  // Extra trailing bytes when Nl = 0 (expects 24 bytes, given 25).
  EXPECT_EQ(MD5_parse_state(&ctx, state, 25), 0);
  EXPECT_TRUE(is_unmodified());

  // Nl is not a multiple of 8 bits (low 3 bits non-zero).
  state[16] = 1;
  EXPECT_EQ(MD5_parse_state(&ctx, state, 24), 0);
  EXPECT_TRUE(is_unmodified());

  // Nl = 32 bits (4 buffered bytes -> expects 28 bytes total).
  state[16] = 32;
  EXPECT_EQ(MD5_parse_state(&ctx, state, 24), 0);
  EXPECT_TRUE(is_unmodified());
  EXPECT_EQ(MD5_parse_state(&ctx, state, 27), 0);
  EXPECT_TRUE(is_unmodified());
  EXPECT_EQ(MD5_parse_state(&valid_ctx, state, 28), 1);
  EXPECT_EQ(MD5_parse_state(&ctx, state, 29), 0);
  EXPECT_TRUE(is_unmodified());
}

}  // namespace
BSSL_NAMESPACE_END
