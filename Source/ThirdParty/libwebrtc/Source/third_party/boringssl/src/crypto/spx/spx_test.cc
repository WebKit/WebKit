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

#include <stdlib.h>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <openssl/bytestring.h>
#define OPENSSL_UNSTABLE_EXPERIMENTAL_SPX
#include <openssl/experimental/spx.h>

#include "../test/file_test.h"
#include "../test/test_util.h"

// suppress warnings for experimental spx api
namespace {

TEST(SpxTest, KeyGeneration) {
  const uint8_t seed[3 * SPX_N] = {0};
  const uint8_t expected_pk[] = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0xbe, 0x6b, 0xd7, 0xe8, 0xe1, 0x98,
      0xea, 0xf6, 0x2d, 0x57, 0x2f, 0x13, 0xfc, 0x79, 0xf2, 0x6f,
  };
  const uint8_t expected_sk[] = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0xbe, 0x6b, 0xd7, 0xe8, 0xe1, 0x98, 0xea,
      0xf6, 0x2d, 0x57, 0x2f, 0x13, 0xfc, 0x79, 0xf2, 0x6f,
  };

  uint8_t pk[2 * SPX_N];
  uint8_t sk[4 * SPX_N];
  SPX_generate_key_from_seed(pk, sk, seed);
  EXPECT_EQ(Bytes(pk), Bytes(expected_pk));
  EXPECT_EQ(Bytes(sk), Bytes(expected_sk));
}

TEST(SpxTest, KeyGeneration2) {
  const uint8_t seed[3 * SPX_N] = {
      0x3f, 0x00, 0xff, 0x1c, 0x9c, 0x5e, 0xaa, 0xfe, 0x09, 0xc3, 0x08, 0x0d,
      0xac, 0xc1, 0x83, 0x2b, 0x35, 0x8a, 0x40, 0xd5, 0xf3, 0x8c, 0xcb, 0x97,
      0xe3, 0xa6, 0xc1, 0xb3, 0xb7, 0x5f, 0x42, 0xab, 0x17, 0x34, 0xe6, 0x41,
      0x89, 0xe1, 0x57, 0x93, 0x12, 0x74, 0xdb, 0xbd, 0xb4, 0x28, 0xd0, 0xfb,
  };
  const uint8_t expected_pk[] = {
      0x17, 0x34, 0xe6, 0x41, 0x89, 0xe1, 0x57, 0x93, 0x12, 0x74, 0xdb,
      0xbd, 0xb4, 0x28, 0xd0, 0xfb, 0x59, 0xc8, 0x64, 0xd2, 0x52, 0x96,
      0xa9, 0x22, 0xdc, 0x61, 0xb8, 0xc1, 0x92, 0x15, 0xac, 0x74,
  };
  const uint8_t expected_sk[] = {
      0x3f, 0x00, 0xff, 0x1c, 0x9c, 0x5e, 0xaa, 0xfe, 0x09, 0xc3, 0x08,
      0x0d, 0xac, 0xc1, 0x83, 0x2b, 0x35, 0x8a, 0x40, 0xd5, 0xf3, 0x8c,
      0xcb, 0x97, 0xe3, 0xa6, 0xc1, 0xb3, 0xb7, 0x5f, 0x42, 0xab, 0x17,
      0x34, 0xe6, 0x41, 0x89, 0xe1, 0x57, 0x93, 0x12, 0x74, 0xdb, 0xbd,
      0xb4, 0x28, 0xd0, 0xfb, 0x59, 0xc8, 0x64, 0xd2, 0x52, 0x96, 0xa9,
      0x22, 0xdc, 0x61, 0xb8, 0xc1, 0x92, 0x15, 0xac, 0x74,
  };

  uint8_t pk[2 * SPX_N];
  uint8_t sk[4 * SPX_N];
  SPX_generate_key_from_seed(pk, sk, seed);
  EXPECT_EQ(Bytes(pk), Bytes(expected_pk));
  EXPECT_EQ(Bytes(sk), Bytes(expected_sk));
}

TEST(SpxTest, BasicSignVerify) {
  uint8_t pk[2 * SPX_N];
  uint8_t sk[4 * SPX_N];
  SPX_generate_key(pk, sk);

  uint8_t message[] = {0x42};
  uint8_t signature[SPX_SIGNATURE_BYTES];
  SPX_sign(signature, sk, message, sizeof(message), true);
  EXPECT_EQ(SPX_verify(signature, pk, message, sizeof(message)), 1);
}

static void SpxFileTest(FileTest *t) {
  std::vector<uint8_t> message, public_key, signature;
  t->IgnoreAttribute("count");
  t->IgnoreAttribute("seed");
  t->IgnoreAttribute("mlen");
  ASSERT_TRUE(t->GetBytes(&message, "msg"));
  ASSERT_TRUE(t->GetBytes(&public_key, "pk"));
  t->IgnoreAttribute("sk");
  t->IgnoreAttribute("smlen");
  ASSERT_TRUE(t->GetBytes(&signature, "sm"));

  EXPECT_EQ(SPX_verify(signature.data(), public_key.data(), message.data(),
                       message.size()),
            1);

  message[0] ^= 1;

  EXPECT_EQ(SPX_verify(signature.data(), public_key.data(), message.data(),
                       message.size()),
            0);
}

static void SpxFileDeterministicTest(FileTest *t) {
  std::vector<uint8_t> message, sk, expected_signature;
  t->IgnoreAttribute("count");
  t->IgnoreAttribute("seed");
  t->IgnoreAttribute("mlen");
  ASSERT_TRUE(t->GetBytes(&message, "msg"));
  t->IgnoreAttribute("pk");
  ASSERT_TRUE(t->GetBytes(&sk, "sk"));
  t->IgnoreAttribute("smlen");
  ASSERT_TRUE(t->GetBytes(&expected_signature, "sm"));
  expected_signature.resize(SPX_SIGNATURE_BYTES);

  uint8_t signature[SPX_SIGNATURE_BYTES];
  SPX_sign(signature, sk.data(), message.data(), message.size(), false);

  EXPECT_EQ(Bytes(signature), Bytes(expected_signature));
}

TEST(SpxTest, TestVectors) {
  FileTestGTest("crypto/spx/spx_tests.txt", SpxFileTest);
  FileTestGTest("crypto/spx/spx_tests_deterministic.txt",
                SpxFileDeterministicTest);
}

}  // namespace
