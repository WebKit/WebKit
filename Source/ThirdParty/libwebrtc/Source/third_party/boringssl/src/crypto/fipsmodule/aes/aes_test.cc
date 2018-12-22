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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <openssl/aes.h>

#include "../../internal.h"
#include "../../test/file_test.h"
#include "../../test/test_util.h"
#include "../../test/wycheproof_util.h"


static void TestRaw(FileTest *t) {
  std::vector<uint8_t> key, plaintext, ciphertext;
  ASSERT_TRUE(t->GetBytes(&key, "Key"));
  ASSERT_TRUE(t->GetBytes(&plaintext, "Plaintext"));
  ASSERT_TRUE(t->GetBytes(&ciphertext, "Ciphertext"));

  ASSERT_EQ(static_cast<unsigned>(AES_BLOCK_SIZE), plaintext.size());
  ASSERT_EQ(static_cast<unsigned>(AES_BLOCK_SIZE), ciphertext.size());

  AES_KEY aes_key;
  ASSERT_EQ(0, AES_set_encrypt_key(key.data(), 8 * key.size(), &aes_key));

  // Test encryption.
  uint8_t block[AES_BLOCK_SIZE];
  AES_encrypt(plaintext.data(), block, &aes_key);
  EXPECT_EQ(Bytes(ciphertext), Bytes(block));

  // Test in-place encryption.
  OPENSSL_memcpy(block, plaintext.data(), AES_BLOCK_SIZE);
  AES_encrypt(block, block, &aes_key);
  EXPECT_EQ(Bytes(ciphertext), Bytes(block));

  ASSERT_EQ(0, AES_set_decrypt_key(key.data(), 8 * key.size(), &aes_key));

  // Test decryption.
  AES_decrypt(ciphertext.data(), block, &aes_key);
  EXPECT_EQ(Bytes(plaintext), Bytes(block));

  // Test in-place decryption.
  OPENSSL_memcpy(block, ciphertext.data(), AES_BLOCK_SIZE);
  AES_decrypt(block, block, &aes_key);
  EXPECT_EQ(Bytes(plaintext), Bytes(block));
}

static void TestKeyWrap(FileTest *t) {
  // All test vectors use the default IV, so test both with implicit and
  // explicit IV.
  //
  // TODO(davidben): Find test vectors that use a different IV.
  static const uint8_t kDefaultIV[] = {
      0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6,
  };

  std::vector<uint8_t> key, plaintext, ciphertext;
  ASSERT_TRUE(t->GetBytes(&key, "Key"));
  ASSERT_TRUE(t->GetBytes(&plaintext, "Plaintext"));
  ASSERT_TRUE(t->GetBytes(&ciphertext, "Ciphertext"));

  ASSERT_EQ(plaintext.size() + 8, ciphertext.size())
      << "Invalid Plaintext and Ciphertext lengths.";

  // Test encryption.
  AES_KEY aes_key;
  ASSERT_EQ(0, AES_set_encrypt_key(key.data(), 8 * key.size(), &aes_key));

  // Test with implicit IV.
  std::unique_ptr<uint8_t[]> buf(new uint8_t[ciphertext.size()]);
  int len = AES_wrap_key(&aes_key, nullptr /* iv */, buf.get(),
                         plaintext.data(), plaintext.size());
  ASSERT_GE(len, 0);
  EXPECT_EQ(Bytes(ciphertext), Bytes(buf.get(), static_cast<size_t>(len)));

  // Test with explicit IV.
  OPENSSL_memset(buf.get(), 0, ciphertext.size());
  len = AES_wrap_key(&aes_key, kDefaultIV, buf.get(), plaintext.data(),
                     plaintext.size());
  ASSERT_GE(len, 0);
  EXPECT_EQ(Bytes(ciphertext), Bytes(buf.get(), static_cast<size_t>(len)));

  // Test decryption.
  ASSERT_EQ(0, AES_set_decrypt_key(key.data(), 8 * key.size(), &aes_key));

  // Test with implicit IV.
  buf.reset(new uint8_t[plaintext.size()]);
  len = AES_unwrap_key(&aes_key, nullptr /* iv */, buf.get(), ciphertext.data(),
                       ciphertext.size());
  ASSERT_GE(len, 0);
  EXPECT_EQ(Bytes(plaintext), Bytes(buf.get(), static_cast<size_t>(len)));

  // Test with explicit IV.
  OPENSSL_memset(buf.get(), 0, plaintext.size());
  len = AES_unwrap_key(&aes_key, kDefaultIV, buf.get(), ciphertext.data(),
                       ciphertext.size());
  ASSERT_GE(len, 0);

  // Test corrupted ciphertext.
  ciphertext[0] ^= 1;
  EXPECT_EQ(-1, AES_unwrap_key(&aes_key, nullptr /* iv */, buf.get(),
                               ciphertext.data(), ciphertext.size()));
}

TEST(AESTest, TestVectors) {
  FileTestGTest("crypto/fipsmodule/aes/aes_tests.txt", [](FileTest *t) {
    if (t->GetParameter() == "Raw") {
      TestRaw(t);
    } else if (t->GetParameter() == "KeyWrap") {
      TestKeyWrap(t);
    } else {
      ADD_FAILURE() << "Unknown mode " << t->GetParameter();
    }
  });
}

TEST(AESTest, WycheproofKeyWrap) {
  FileTestGTest("third_party/wycheproof_testvectors/kw_test.txt",
                [](FileTest *t) {
    std::string key_size;
    ASSERT_TRUE(t->GetInstruction(&key_size, "keySize"));
    std::vector<uint8_t> ct, key, msg;
    ASSERT_TRUE(t->GetBytes(&ct, "ct"));
    ASSERT_TRUE(t->GetBytes(&key, "key"));
    ASSERT_TRUE(t->GetBytes(&msg, "msg"));
    ASSERT_EQ(static_cast<unsigned>(atoi(key_size.c_str())), key.size() * 8);
    WycheproofResult result;
    ASSERT_TRUE(GetWycheproofResult(t, &result));

    if (result != WycheproofResult::kInvalid) {
      ASSERT_GE(ct.size(), 8u);

      AES_KEY aes;
      ASSERT_EQ(0, AES_set_decrypt_key(key.data(), 8 * key.size(), &aes));
      std::vector<uint8_t> out(ct.size() - 8);
      int len = AES_unwrap_key(&aes, nullptr, out.data(), ct.data(), ct.size());
      ASSERT_EQ(static_cast<int>(out.size()), len);
      EXPECT_EQ(Bytes(msg), Bytes(out));

      out.resize(msg.size() + 8);
      ASSERT_EQ(0, AES_set_encrypt_key(key.data(), 8 * key.size(), &aes));
      len = AES_wrap_key(&aes, nullptr, out.data(), msg.data(), msg.size());
      ASSERT_EQ(static_cast<int>(out.size()), len);
      EXPECT_EQ(Bytes(ct), Bytes(out));
    } else {
      AES_KEY aes;
      ASSERT_EQ(0, AES_set_decrypt_key(key.data(), 8 * key.size(), &aes));
      std::vector<uint8_t> out(ct.size() < 8 ? 0 : ct.size() - 8);
      int len = AES_unwrap_key(&aes, nullptr, out.data(), ct.data(), ct.size());
      EXPECT_EQ(-1, len);
    }
  });
}

TEST(AESTest, WrapBadLengths) {
  uint8_t key[128/8] = {0};
  AES_KEY aes;
  ASSERT_EQ(0, AES_set_encrypt_key(key, 128, &aes));

  // Input lengths to |AES_wrap_key| must be a multiple of 8 and at least 16.
  static const size_t kLengths[] = {0, 1,  2,  3,  4,  5,  6,  7, 8,
                                    9, 10, 11, 12, 13, 14, 15, 20};
  for (size_t len : kLengths) {
    SCOPED_TRACE(len);
    std::vector<uint8_t> in(len);
    std::vector<uint8_t> out(len + 8);
    EXPECT_EQ(-1,
              AES_wrap_key(&aes, nullptr, out.data(), in.data(), in.size()));
  }
}
