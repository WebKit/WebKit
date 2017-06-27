/* Copyright (c) 2015, Google Inc.
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

#include <stdint.h>
#include <string.h>

#include <gtest/gtest.h>

#include <openssl/curve25519.h>

#include "../internal.h"
#include "../test/file_test.h"
#include "../test/test_util.h"


TEST(Ed25519Test, TestVectors) {
  FileTestGTest("crypto/curve25519/ed25519_tests.txt", [](FileTest *t) {
    std::vector<uint8_t> private_key, public_key, message, expected_signature;
    ASSERT_TRUE(t->GetBytes(&private_key, "PRIV"));
    ASSERT_EQ(64u, private_key.size());
    ASSERT_TRUE(t->GetBytes(&public_key, "PUB"));
    ASSERT_EQ(32u, public_key.size());
    ASSERT_TRUE(t->GetBytes(&message, "MESSAGE"));
    ASSERT_TRUE(t->GetBytes(&expected_signature, "SIG"));
    ASSERT_EQ(64u, expected_signature.size());

    uint8_t signature[64];
    ASSERT_TRUE(ED25519_sign(signature, message.data(), message.size(),
                             private_key.data()));
    EXPECT_EQ(Bytes(expected_signature), Bytes(signature));
    EXPECT_TRUE(ED25519_verify(message.data(), message.size(), signature,
                               public_key.data()));
  });
}

TEST(Ed25519Test, KeypairFromSeed) {
  uint8_t public_key1[32], private_key1[64];
  ED25519_keypair(public_key1, private_key1);

  uint8_t seed[32];
  OPENSSL_memcpy(seed, private_key1, sizeof(seed));

  uint8_t public_key2[32], private_key2[64];
  ED25519_keypair_from_seed(public_key2, private_key2, seed);

  EXPECT_EQ(Bytes(public_key1), Bytes(public_key2));
  EXPECT_EQ(Bytes(private_key1), Bytes(private_key2));
}
