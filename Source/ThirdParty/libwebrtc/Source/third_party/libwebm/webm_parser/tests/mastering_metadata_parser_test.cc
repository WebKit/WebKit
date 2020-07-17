// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/mastering_metadata_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"

using webm::ElementParserTest;
using webm::Id;
using webm::MasteringMetadata;
using webm::MasteringMetadataParser;

namespace {

class MasteringMetadataParserTest
    : public ElementParserTest<MasteringMetadataParser,
                               Id::kMasteringMetadata> {};

TEST_F(MasteringMetadataParserTest, DefaultParse) {
  ParseAndVerify();

  const MasteringMetadata mastering_metadata = parser_.value();

  EXPECT_FALSE(mastering_metadata.primary_r_chromaticity_x.is_present());
  EXPECT_EQ(0, mastering_metadata.primary_r_chromaticity_x.value());

  EXPECT_FALSE(mastering_metadata.primary_r_chromaticity_y.is_present());
  EXPECT_EQ(0, mastering_metadata.primary_r_chromaticity_y.value());

  EXPECT_FALSE(mastering_metadata.primary_g_chromaticity_x.is_present());
  EXPECT_EQ(0, mastering_metadata.primary_g_chromaticity_x.value());

  EXPECT_FALSE(mastering_metadata.primary_g_chromaticity_y.is_present());
  EXPECT_EQ(0, mastering_metadata.primary_g_chromaticity_y.value());

  EXPECT_FALSE(mastering_metadata.primary_b_chromaticity_x.is_present());
  EXPECT_EQ(0, mastering_metadata.primary_b_chromaticity_x.value());

  EXPECT_FALSE(mastering_metadata.primary_b_chromaticity_y.is_present());
  EXPECT_EQ(0, mastering_metadata.primary_b_chromaticity_y.value());

  EXPECT_FALSE(mastering_metadata.white_point_chromaticity_x.is_present());
  EXPECT_EQ(0, mastering_metadata.white_point_chromaticity_x.value());

  EXPECT_FALSE(mastering_metadata.white_point_chromaticity_y.is_present());
  EXPECT_EQ(0, mastering_metadata.white_point_chromaticity_y.value());

  EXPECT_FALSE(mastering_metadata.luminance_max.is_present());
  EXPECT_EQ(0, mastering_metadata.luminance_max.value());

  EXPECT_FALSE(mastering_metadata.luminance_min.is_present());
  EXPECT_EQ(0, mastering_metadata.luminance_min.value());
}

TEST_F(MasteringMetadataParserTest, DefaultValues) {
  SetReaderData({
      0x55, 0xD1,  // ID = 0x55D1 (PrimaryRChromaticityX).
      0x80,  // Size = 0.

      0x55, 0xD2,  // ID = 0x55D2 (PrimaryRChromaticityY).
      0x80,  // Size = 0.

      0x55, 0xD3,  // ID = 0x55D3 (PrimaryGChromaticityX).
      0x80,  // Size = 0.

      0x55, 0xD4,  // ID = 0x55D4 (PrimaryGChromaticityY).
      0x80,  // Size = 0.

      0x55, 0xD5,  // ID = 0x55D5 (PrimaryBChromaticityX).
      0x80,  // Size = 0.

      0x55, 0xD6,  // ID = 0x55D6 (PrimaryBChromaticityY).
      0x80,  // Size = 0.

      0x55, 0xD7,  // ID = 0x55D7 (WhitePointChromaticityX).
      0x80,  // Size = 0.

      0x55, 0xD8,  // ID = 0x55D8 (WhitePointChromaticityY).
      0x80,  // Size = 0.

      0x55, 0xD9,  // ID = 0x55D9 (LuminanceMax).
      0x80,  // Size = 0.

      0x55, 0xDA,  // ID = 0x55DA (LuminanceMin).
      0x80,  // Size = 0.
  });

  ParseAndVerify();

  const MasteringMetadata mastering_metadata = parser_.value();

  EXPECT_TRUE(mastering_metadata.primary_r_chromaticity_x.is_present());
  EXPECT_EQ(0, mastering_metadata.primary_r_chromaticity_x.value());

  EXPECT_TRUE(mastering_metadata.primary_r_chromaticity_y.is_present());
  EXPECT_EQ(0, mastering_metadata.primary_r_chromaticity_y.value());

  EXPECT_TRUE(mastering_metadata.primary_g_chromaticity_x.is_present());
  EXPECT_EQ(0, mastering_metadata.primary_g_chromaticity_x.value());

  EXPECT_TRUE(mastering_metadata.primary_g_chromaticity_y.is_present());
  EXPECT_EQ(0, mastering_metadata.primary_g_chromaticity_y.value());

  EXPECT_TRUE(mastering_metadata.primary_b_chromaticity_x.is_present());
  EXPECT_EQ(0, mastering_metadata.primary_b_chromaticity_x.value());

  EXPECT_TRUE(mastering_metadata.primary_b_chromaticity_y.is_present());
  EXPECT_EQ(0, mastering_metadata.primary_b_chromaticity_y.value());

  EXPECT_TRUE(mastering_metadata.white_point_chromaticity_x.is_present());
  EXPECT_EQ(0, mastering_metadata.white_point_chromaticity_x.value());

  EXPECT_TRUE(mastering_metadata.white_point_chromaticity_y.is_present());
  EXPECT_EQ(0, mastering_metadata.white_point_chromaticity_y.value());

  EXPECT_TRUE(mastering_metadata.luminance_max.is_present());
  EXPECT_EQ(0, mastering_metadata.luminance_max.value());

  EXPECT_TRUE(mastering_metadata.luminance_min.is_present());
  EXPECT_EQ(0, mastering_metadata.luminance_min.value());
}

TEST_F(MasteringMetadataParserTest, CustomValues) {
  SetReaderData({
      0x55, 0xD1,  // ID = 0x55D1 (PrimaryRChromaticityX).
      0x84,  // Size = 4.
      0x3E, 0x00, 0x00, 0x00,  // Body (value = 0.125).

      0x55, 0xD2,  // ID = 0x55D2 (PrimaryRChromaticityY).
      0x84,  // Size = 4.
      0x3E, 0x80, 0x00, 0x00,  // Body (value = 0.25).

      0x55, 0xD3,  // ID = 0x55D3 (PrimaryGChromaticityX).
      0x84,  // Size = 4.
      0x3E, 0xC0, 0x00, 0x00,  // Body (value = 0.375).

      0x55, 0xD4,  // ID = 0x55D4 (PrimaryGChromaticityY).
      0x84,  // Size = 4.
      0x3F, 0x00, 0x00, 0x00,  // Body (value = 0.5).

      0x55, 0xD5,  // ID = 0x55D5 (PrimaryBChromaticityX).
      0x84,  // Size = 4.
      0x3F, 0x20, 0x00, 0x00,  // Body (value = 0.625).

      0x55, 0xD6,  // ID = 0x55D6 (PrimaryBChromaticityY).
      0x84,  // Size = 4.
      0x3F, 0x40, 0x00, 0x00,  // Body (value = 0.75).

      0x55, 0xD7,  // ID = 0x55D7 (WhitePointChromaticityX).
      0x84,  // Size = 4.
      0x3F, 0x60, 0x00, 0x00,  // Body (value = 0.875).

      0x55, 0xD8,  // ID = 0x55D8 (WhitePointChromaticityY).
      0x84,  // Size = 4.
      0x3F, 0x80, 0x00, 0x00,  // Body (value = 1).

      0x55, 0xD9,  // ID = 0x55D9 (LuminanceMax).
      0x84,  // Size = 4.
      0x40, 0x00, 0x00, 0x00,  // Body (value = 2).

      0x55, 0xDA,  // ID = 0x55DA (LuminanceMin).
      0x84,  // Size = 4.
      0x40, 0x40, 0x00, 0x00,  // Body (value = 3).
  });

  ParseAndVerify();

  const MasteringMetadata mastering_metadata = parser_.value();

  EXPECT_TRUE(mastering_metadata.primary_r_chromaticity_x.is_present());
  EXPECT_EQ(0.125, mastering_metadata.primary_r_chromaticity_x.value());

  EXPECT_TRUE(mastering_metadata.primary_r_chromaticity_y.is_present());
  EXPECT_EQ(0.25, mastering_metadata.primary_r_chromaticity_y.value());

  EXPECT_TRUE(mastering_metadata.primary_g_chromaticity_x.is_present());
  EXPECT_EQ(0.375, mastering_metadata.primary_g_chromaticity_x.value());

  EXPECT_TRUE(mastering_metadata.primary_g_chromaticity_y.is_present());
  EXPECT_EQ(0.5, mastering_metadata.primary_g_chromaticity_y.value());

  EXPECT_TRUE(mastering_metadata.primary_b_chromaticity_x.is_present());
  EXPECT_EQ(0.625, mastering_metadata.primary_b_chromaticity_x.value());

  EXPECT_TRUE(mastering_metadata.primary_b_chromaticity_y.is_present());
  EXPECT_EQ(0.75, mastering_metadata.primary_b_chromaticity_y.value());

  EXPECT_TRUE(mastering_metadata.white_point_chromaticity_x.is_present());
  EXPECT_EQ(0.875, mastering_metadata.white_point_chromaticity_x.value());

  EXPECT_TRUE(mastering_metadata.white_point_chromaticity_y.is_present());
  EXPECT_EQ(1.0, mastering_metadata.white_point_chromaticity_y.value());

  EXPECT_TRUE(mastering_metadata.luminance_max.is_present());
  EXPECT_EQ(2.0, mastering_metadata.luminance_max.value());

  EXPECT_TRUE(mastering_metadata.luminance_min.is_present());
  EXPECT_EQ(3.0, mastering_metadata.luminance_min.value());
}

}  // namespace
