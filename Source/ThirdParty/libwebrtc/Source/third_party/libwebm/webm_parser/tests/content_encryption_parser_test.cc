// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/content_encryption_parser.h"

#include <cstdint>

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"

using webm::AesSettingsCipherMode;
using webm::ContentEncAesSettings;
using webm::ContentEncAlgo;
using webm::ContentEncryption;
using webm::ContentEncryptionParser;
using webm::ElementParserTest;
using webm::Id;

namespace {

class ContentEncryptionParserTest
    : public ElementParserTest<ContentEncryptionParser,
                               Id::kContentEncryption> {};

TEST_F(ContentEncryptionParserTest, DefaultParse) {
  ParseAndVerify();

  const ContentEncryption content_encryption = parser_.value();

  EXPECT_FALSE(content_encryption.algorithm.is_present());
  EXPECT_EQ(ContentEncAlgo::kOnlySigned, content_encryption.algorithm.value());

  EXPECT_FALSE(content_encryption.key_id.is_present());
  EXPECT_EQ(std::vector<std::uint8_t>{}, content_encryption.key_id.value());

  EXPECT_FALSE(content_encryption.aes_settings.is_present());
  EXPECT_EQ(ContentEncAesSettings{}, content_encryption.aes_settings.value());
}

TEST_F(ContentEncryptionParserTest, DefaultValues) {
  SetReaderData({
      0x47, 0xE1,  // ID = 0x47E1 (ContentEncAlgo).
      0x80,  // Size = 0.

      0x47, 0xE2,  // ID = 0x47E2 (ContentEncKeyID).
      0x80,  // Size = 0.

      0x47, 0xE7,  // ID = 0x47E7 (ContentEncAESSettings).
      0x80,  // Size = 0.
  });

  ParseAndVerify();

  const ContentEncryption content_encryption = parser_.value();

  EXPECT_TRUE(content_encryption.algorithm.is_present());
  EXPECT_EQ(ContentEncAlgo::kOnlySigned, content_encryption.algorithm.value());

  EXPECT_TRUE(content_encryption.key_id.is_present());
  EXPECT_EQ(std::vector<std::uint8_t>{}, content_encryption.key_id.value());

  EXPECT_TRUE(content_encryption.aes_settings.is_present());
  EXPECT_EQ(ContentEncAesSettings{}, content_encryption.aes_settings.value());
}

TEST_F(ContentEncryptionParserTest, CustomValues) {
  SetReaderData({
      0x47, 0xE1,  // ID = 0x47E1 (ContentEncAlgo).
      0x81,  // Size = 1.
      0x05,  // Body (value = AES).

      0x47, 0xE2,  // ID = 0x47E2 (ContentEncKeyID).
      0x81,  // Size = 1.
      0x00,  // Body.

      0x47, 0xE7,  // ID = 0x47E7 (ContentEncAESSettings).
      0x84,  // Size = 4.

      0x47, 0xE8,  //   ID = 0x47E8 (AESSettingsCipherMode).
      0x81,  //   Size = 1.
      0x00,  //   Body (value = 0).
  });

  ParseAndVerify();

  const ContentEncryption content_encryption = parser_.value();

  EXPECT_TRUE(content_encryption.algorithm.is_present());
  EXPECT_EQ(ContentEncAlgo::kAes, content_encryption.algorithm.value());

  EXPECT_TRUE(content_encryption.key_id.is_present());
  EXPECT_EQ(std::vector<std::uint8_t>{0x00}, content_encryption.key_id.value());

  ContentEncAesSettings expected;
  expected.aes_settings_cipher_mode.Set(static_cast<AesSettingsCipherMode>(0),
                                        true);
  EXPECT_TRUE(content_encryption.aes_settings.is_present());
  EXPECT_EQ(expected, content_encryption.aes_settings.value());
}

}  // namespace
