// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/colour_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"

using webm::Colour;
using webm::ColourParser;
using webm::ElementParserTest;
using webm::Id;
using webm::MasteringMetadata;
using webm::MatrixCoefficients;
using webm::Primaries;
using webm::Range;
using webm::TransferCharacteristics;

namespace {

class ColourParserTest : public ElementParserTest<ColourParser, Id::kColour> {};

TEST_F(ColourParserTest, DefaultParse) {
  ParseAndVerify();

  const Colour colour = parser_.value();

  EXPECT_FALSE(colour.matrix_coefficients.is_present());
  EXPECT_EQ(MatrixCoefficients::kUnspecified,
            colour.matrix_coefficients.value());

  EXPECT_FALSE(colour.bits_per_channel.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), colour.bits_per_channel.value());

  EXPECT_FALSE(colour.chroma_subsampling_x.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), colour.chroma_subsampling_x.value());

  EXPECT_FALSE(colour.chroma_subsampling_y.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), colour.chroma_subsampling_y.value());

  EXPECT_FALSE(colour.cb_subsampling_x.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), colour.cb_subsampling_x.value());

  EXPECT_FALSE(colour.cb_subsampling_y.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), colour.cb_subsampling_y.value());

  EXPECT_FALSE(colour.chroma_siting_x.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), colour.chroma_siting_x.value());

  EXPECT_FALSE(colour.chroma_siting_y.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), colour.chroma_siting_y.value());

  EXPECT_FALSE(colour.range.is_present());
  EXPECT_EQ(Range::kUnspecified, colour.range.value());

  EXPECT_FALSE(colour.transfer_characteristics.is_present());
  EXPECT_EQ(TransferCharacteristics::kUnspecified,
            colour.transfer_characteristics.value());

  EXPECT_FALSE(colour.primaries.is_present());
  EXPECT_EQ(Primaries::kUnspecified, colour.primaries.value());

  EXPECT_FALSE(colour.max_cll.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), colour.max_cll.value());

  EXPECT_FALSE(colour.max_fall.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), colour.max_fall.value());

  EXPECT_FALSE(colour.mastering_metadata.is_present());
  EXPECT_EQ(MasteringMetadata{}, colour.mastering_metadata.value());
}

TEST_F(ColourParserTest, DefaultValues) {
  SetReaderData({
      0x55, 0xB1,  // ID = 0x55B1 (MatrixCoefficients).
      0x80,  // Size = 0.

      0x55, 0xB2,  // ID = 0x55B2 (BitsPerChannel).
      0x80,  // Size = 0.

      0x55, 0xB3,  // ID = 0x55B3 (ChromaSubsamplingHorz).
      0x80,  // Size = 0.

      0x55, 0xB4,  // ID = 0x55B4 (ChromaSubsamplingVert).
      0x80,  // Size = 0.

      0x55, 0xB5,  // ID = 0x55B5 (CbSubsamplingHorz).
      0x80,  // Size = 0.

      0x55, 0xB6,  // ID = 0x55B6 (CbSubsamplingVert).
      0x80,  // Size = 0.

      0x55, 0xB7,  // ID = 0x55B7 (ChromaSitingHorz).
      0x80,  // Size = 0.

      0x55, 0xB8,  // ID = 0x55B8 (ChromaSitingVert).
      0x80,  // Size = 0.

      0x55, 0xB9,  // ID = 0x55B9 (Range).
      0x80,  // Size = 0.

      0x55, 0xBA,  // ID = 0x55BA (TransferCharacteristics).
      0x80,  // Size = 0.

      0x55, 0xBB,  // ID = 0x55BB (Primaries).
      0x80,  // Size = 0.

      0x55, 0xBC,  // ID = 0x55BC (MaxCLL).
      0x80,  // Size = 0.

      0x55, 0xBD,  // ID = 0x55BD (MaxFALL).
      0x80,  // Size = 0.

      0x55, 0xD0,  // ID = 0x55D0 (MasteringMetadata).
      0x80,  // Size = 0.
  });

  ParseAndVerify();

  const Colour colour = parser_.value();

  EXPECT_TRUE(colour.matrix_coefficients.is_present());
  EXPECT_EQ(MatrixCoefficients::kUnspecified,
            colour.matrix_coefficients.value());

  EXPECT_TRUE(colour.bits_per_channel.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), colour.bits_per_channel.value());

  EXPECT_TRUE(colour.chroma_subsampling_x.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), colour.chroma_subsampling_x.value());

  EXPECT_TRUE(colour.chroma_subsampling_y.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), colour.chroma_subsampling_y.value());

  EXPECT_TRUE(colour.cb_subsampling_x.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), colour.cb_subsampling_x.value());

  EXPECT_TRUE(colour.cb_subsampling_y.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), colour.cb_subsampling_y.value());

  EXPECT_TRUE(colour.chroma_siting_x.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), colour.chroma_siting_x.value());

  EXPECT_TRUE(colour.chroma_siting_y.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), colour.chroma_siting_y.value());

  EXPECT_TRUE(colour.range.is_present());
  EXPECT_EQ(Range::kUnspecified, colour.range.value());

  EXPECT_TRUE(colour.transfer_characteristics.is_present());
  EXPECT_EQ(TransferCharacteristics::kUnspecified,
            colour.transfer_characteristics.value());

  EXPECT_TRUE(colour.primaries.is_present());
  EXPECT_EQ(Primaries::kUnspecified, colour.primaries.value());

  EXPECT_TRUE(colour.max_cll.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), colour.max_cll.value());

  EXPECT_TRUE(colour.max_fall.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), colour.max_fall.value());

  EXPECT_TRUE(colour.mastering_metadata.is_present());
  EXPECT_EQ(MasteringMetadata{}, colour.mastering_metadata.value());
}

TEST_F(ColourParserTest, CustomValues) {
  SetReaderData({
      0x55, 0xB1,  // ID = 0x55B1 (MatrixCoefficients).
      0x81,  // Size = 1.
      0x01,  // Body (value = BT.709).

      0x55, 0xB2,  // ID = 0x55B2 (BitsPerChannel).
      0x81,  // Size = 1.
      0x02,  // Body (value = 2).

      0x55, 0xB3,  // ID = 0x55B3 (ChromaSubsamplingHorz).
      0x81,  // Size = 1.
      0x03,  // Body (value = 3).

      0x55, 0xB4,  // ID = 0x55B4 (ChromaSubsamplingVert).
      0x81,  // Size = 1.
      0x04,  // Body (value = 4).

      0x55, 0xB5,  // ID = 0x55B5 (CbSubsamplingHorz).
      0x81,  // Size = 1.
      0x05,  // Body (value = 5).

      0x55, 0xB6,  // ID = 0x55B6 (CbSubsamplingVert).
      0x81,  // Size = 1.
      0x06,  // Body (value = 6).

      0x55, 0xB7,  // ID = 0x55B7 (ChromaSitingHorz).
      0x81,  // Size = 1.
      0x01,  // Body (value = 1).

      0x55, 0xB8,  // ID = 0x55B8 (ChromaSitingVert).
      0x81,  // Size = 1.
      0x02,  // Body (value = 2).

      0x55, 0xB9,  // ID = 0x55B9 (Range).
      0x81,  // Size = 1.
      0x03,  // Body (value = 3 (derived)).

      0x55, 0xBA,  // ID = 0x55BA (TransferCharacteristics).
      0x81,  // Size = 1.
      0x04,  // Body (value = BT.470‑6 System M with display gamma 2.2).

      0x55, 0xBB,  // ID = 0x55BB (Primaries).
      0x81,  // Size = 1.
      0x05,  // Body (value =  BT.470‑6 System B, G).

      0x55, 0xBC,  // ID = 0x55BC (MaxCLL).
      0x81,  // Size = 1.
      0x06,  // Body (value = 6).

      0x55, 0xBD,  // ID = 0x55BD (MaxFALL).
      0x81,  // Size = 1.
      0x07,  // Body (value = 7).

      0x55, 0xD0,  // ID = 0x55D0 (MasteringMetadata).
      0x87,  // Size = 7.

      0x55, 0xD1,  //   ID = 0x55D1 (PrimaryRChromaticityX).
      0x84,  //   Size = 4.
      0x3F, 0x80, 0x00, 0x00,  //   Body (value = 1).
  });

  ParseAndVerify();

  const Colour colour = parser_.value();

  EXPECT_TRUE(colour.matrix_coefficients.is_present());
  EXPECT_EQ(MatrixCoefficients::kBt709, colour.matrix_coefficients.value());

  EXPECT_TRUE(colour.bits_per_channel.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(2), colour.bits_per_channel.value());

  EXPECT_TRUE(colour.chroma_subsampling_x.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(3), colour.chroma_subsampling_x.value());

  EXPECT_TRUE(colour.chroma_subsampling_y.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(4), colour.chroma_subsampling_y.value());

  EXPECT_TRUE(colour.cb_subsampling_x.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(5), colour.cb_subsampling_x.value());

  EXPECT_TRUE(colour.cb_subsampling_y.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(6), colour.cb_subsampling_y.value());

  EXPECT_TRUE(colour.chroma_siting_x.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), colour.chroma_siting_x.value());

  EXPECT_TRUE(colour.chroma_siting_y.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(2), colour.chroma_siting_y.value());

  EXPECT_TRUE(colour.range.is_present());
  EXPECT_EQ(Range::kDerived, colour.range.value());

  EXPECT_TRUE(colour.transfer_characteristics.is_present());
  EXPECT_EQ(TransferCharacteristics::kGamma22curve,
            colour.transfer_characteristics.value());

  EXPECT_TRUE(colour.primaries.is_present());
  EXPECT_EQ(Primaries::kBt470Bg, colour.primaries.value());

  EXPECT_TRUE(colour.max_cll.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(6), colour.max_cll.value());

  EXPECT_TRUE(colour.max_fall.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(7), colour.max_fall.value());

  MasteringMetadata mastering_metadata{};
  mastering_metadata.primary_r_chromaticity_x.Set(1.0, true);
  EXPECT_TRUE(colour.mastering_metadata.is_present());
  EXPECT_EQ(mastering_metadata, colour.mastering_metadata.value());
}

}  // namespace
