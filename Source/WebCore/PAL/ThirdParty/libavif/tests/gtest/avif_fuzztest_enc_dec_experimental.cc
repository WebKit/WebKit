// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "avif/avif.h"
#include "avif_fuzztest_helpers.h"
#include "aviftest_helpers.h"
#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

namespace avif {
namespace testutil {
namespace {

::testing::Environment* const kStackLimitEnv = SetStackLimitTo512x1024Bytes();

void CheckGainMapMetadataMatches(const avifGainMapMetadata& actual,
                                 const avifGainMapMetadata& expected) {
  // 'expecteed' is the source struct which has arbitrary data and booleans
  // values can contain any value, but the decoded ('actual') struct should
  // be 0 or 1.
  EXPECT_EQ(actual.backwardDirection, expected.backwardDirection ? 1 : 0);
  EXPECT_EQ(actual.baseHdrHeadroomN, expected.baseHdrHeadroomN);
  EXPECT_EQ(actual.baseHdrHeadroomD, expected.baseHdrHeadroomD);
  EXPECT_EQ(actual.alternateHdrHeadroomN, expected.alternateHdrHeadroomN);
  EXPECT_EQ(actual.alternateHdrHeadroomD, expected.alternateHdrHeadroomD);
  for (int c = 0; c < 3; ++c) {
    SCOPED_TRACE(c);
    EXPECT_EQ(actual.baseOffsetN[c], expected.baseOffsetN[c]);
    EXPECT_EQ(actual.baseOffsetD[c], expected.baseOffsetD[c]);
    EXPECT_EQ(actual.alternateOffsetN[c], expected.alternateOffsetN[c]);
    EXPECT_EQ(actual.alternateOffsetD[c], expected.alternateOffsetD[c]);
    EXPECT_EQ(actual.gainMapGammaN[c], expected.gainMapGammaN[c]);
    EXPECT_EQ(actual.gainMapGammaD[c], expected.gainMapGammaD[c]);
    EXPECT_EQ(actual.gainMapMinN[c], expected.gainMapMinN[c]);
    EXPECT_EQ(actual.gainMapMinD[c], expected.gainMapMinD[c]);
    EXPECT_EQ(actual.gainMapMaxN[c], expected.gainMapMaxN[c]);
    EXPECT_EQ(actual.gainMapMaxD[c], expected.gainMapMaxD[c]);
  }
}

void EncodeDecodeValid(ImagePtr image, EncoderPtr encoder, DecoderPtr decoder) {
  ImagePtr decoded_image(avifImageCreateEmpty());
  ASSERT_NE(image.get(), nullptr);
  ASSERT_NE(encoder.get(), nullptr);
  ASSERT_NE(decoder.get(), nullptr);
  ASSERT_NE(decoded_image.get(), nullptr);

  AvifRwData encoded_data;
  const avifResult encoder_result =
      avifEncoderWrite(encoder.get(), image.get(), &encoded_data);
  ASSERT_EQ(encoder_result, AVIF_RESULT_OK)
      << avifResultToString(encoder_result);

  const avifResult decoder_result = avifDecoderReadMemory(
      decoder.get(), decoded_image.get(), encoded_data.data, encoded_data.size);
  ASSERT_EQ(decoder_result, AVIF_RESULT_OK)
      << avifResultToString(decoder_result);

  EXPECT_EQ(decoded_image->width, image->width);
  EXPECT_EQ(decoded_image->height, image->height);
  EXPECT_EQ(decoded_image->depth, image->depth);
  EXPECT_EQ(decoded_image->yuvFormat, image->yuvFormat);

  EXPECT_EQ(decoder->gainMapPresent,
            image->gainMap != nullptr && image->gainMap->image != nullptr);
  ASSERT_EQ(decoded_image->gainMap != nullptr,
            decoder->gainMapPresent && (decoder->enableDecodingGainMap ||
                                        decoder->enableParsingGainMapMetadata));
  ASSERT_EQ(decoded_image->gainMap != nullptr &&
                decoded_image->gainMap->image != nullptr,
            decoder->gainMapPresent && decoder->enableDecodingGainMap);
  if (decoder->gainMapPresent && decoder->enableDecodingGainMap) {
    ASSERT_NE(decoded_image->gainMap, nullptr);
    ASSERT_NE(decoded_image->gainMap->image, nullptr);
    EXPECT_EQ(decoded_image->gainMap->image->width,
              image->gainMap->image->width);
    EXPECT_EQ(decoded_image->gainMap->image->height,
              image->gainMap->image->height);
    EXPECT_EQ(decoded_image->gainMap->image->depth,
              image->gainMap->image->depth);
    EXPECT_EQ(decoded_image->gainMap->image->yuvFormat,
              image->gainMap->image->yuvFormat);
    EXPECT_EQ(image->gainMap->image->gainMap, nullptr);
    EXPECT_EQ(decoded_image->gainMap->image->alphaPlane, nullptr);

    if (decoder->enableParsingGainMapMetadata) {
      CheckGainMapMetadataMatches(decoded_image->gainMap->metadata,
                                  image->gainMap->metadata);
    }
  }

  // Verify that an opaque input leads to an opaque output.
  if (avifImageIsOpaque(image.get())) {
    EXPECT_TRUE(avifImageIsOpaque(decoded_image.get()));
  }
  // A transparent image may be heavily compressed to an opaque image. This is
  // hard to verify so do not check it.
}

// Note that avifGainMapMetadata is passed as a byte array
// because the C array fields in the struct seem to prevent fuzztest from
// handling it natively.
ImagePtr AddGainMapToImage(
    ImagePtr image, ImagePtr gain_map,
    const std::array<uint8_t, sizeof(avifGainMapMetadata)>& metadata) {
  image->gainMap = avifGainMapCreate();
  image->gainMap->image = gain_map.release();
  std::memcpy(&image->gainMap->metadata, metadata.data(), metadata.size());
  return image;
}

inline auto ArbitraryAvifImageWithGainMap() {
  return fuzztest::Map(
      AddGainMapToImage, ArbitraryAvifImage(), ArbitraryAvifImage(),
      fuzztest::Arbitrary<std::array<uint8_t, sizeof(avifGainMapMetadata)>>());
}

FUZZ_TEST(EncodeDecodeAvifFuzzTest, EncodeDecodeValid)
    .WithDomains(fuzztest::OneOf(ArbitraryAvifImage(),
                                 ArbitraryAvifImageWithGainMap()),
                 ArbitraryAvifEncoder(), ArbitraryAvifDecoder());

}  // namespace
}  // namespace testutil
}  // namespace avif
