// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/content_enc_aes_settings_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"

using webm::AesSettingsCipherMode;
using webm::ContentEncAesSettings;
using webm::ContentEncAesSettingsParser;
using webm::ElementParserTest;
using webm::Id;

namespace {

class ContentEncAesSettingsParserTest
    : public ElementParserTest<ContentEncAesSettingsParser,
                               Id::kContentEncAesSettings> {};

TEST_F(ContentEncAesSettingsParserTest, DefaultParse) {
  ParseAndVerify();

  const ContentEncAesSettings content_enc_aes_settings = parser_.value();

  EXPECT_FALSE(content_enc_aes_settings.aes_settings_cipher_mode.is_present());
  EXPECT_EQ(AesSettingsCipherMode::kCtr,
            content_enc_aes_settings.aes_settings_cipher_mode.value());
}

TEST_F(ContentEncAesSettingsParserTest, DefaultValues) {
  SetReaderData({
      0x47, 0xE8,  // ID = 0x47E8 (AESSettingsCipherMode).
      0x80,  // Size = 0.
  });

  ParseAndVerify();

  const ContentEncAesSettings content_enc_aes_settings = parser_.value();

  EXPECT_TRUE(content_enc_aes_settings.aes_settings_cipher_mode.is_present());
  EXPECT_EQ(AesSettingsCipherMode::kCtr,
            content_enc_aes_settings.aes_settings_cipher_mode.value());
}

TEST_F(ContentEncAesSettingsParserTest, CustomValues) {
  SetReaderData({
      0x47, 0xE8,  // ID = 0x47E8 (AESSettingsCipherMode).
      0x81,  // Size = 1.
      0x00,  // Body (value = 0).
  });

  ParseAndVerify();

  const ContentEncAesSettings content_enc_aes_settings = parser_.value();

  EXPECT_TRUE(content_enc_aes_settings.aes_settings_cipher_mode.is_present());
  EXPECT_EQ(static_cast<AesSettingsCipherMode>(0),
            content_enc_aes_settings.aes_settings_cipher_mode.value());
}

}  // namespace
