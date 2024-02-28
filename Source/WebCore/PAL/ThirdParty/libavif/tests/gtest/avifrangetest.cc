// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <algorithm>
#include <cstring>
#include <tuple>

#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

enum class InputType { kStillImage, kGrid, kAnimation };

// Generates a valid AVIF stream with the video range flag set to the same value
// in the "colr" box and in the AV1 OBU payload (in the "mdat" box).
avifResult GenerateEncodedData(avifRange range, InputType input_type,
                               testutil::AvifRwData* encoded) {
  ImagePtr image =
      testutil::CreateImage(/*width=*/64, /*height=*/64, /*depth=*/8,
                            AVIF_PIXEL_FORMAT_YUV420, AVIF_PLANES_ALL, range);
  AVIF_CHECKERR(image != nullptr, AVIF_RESULT_OUT_OF_MEMORY);
  testutil::FillImageGradient(image.get());  // Pixel values do not matter.

  EncoderPtr encoder(avifEncoderCreate());
  AVIF_CHECKERR(encoder != nullptr, AVIF_RESULT_OUT_OF_MEMORY);
  if (input_type == InputType::kStillImage) {
    AVIF_CHECKRES(avifEncoderWrite(encoder.get(), image.get(), encoded));
  } else if (input_type == InputType::kGrid) {
    const avifImage* cellImages[2] = {image.get(), image.get()};
    AVIF_CHECKRES(avifEncoderAddImageGrid(encoder.get(), /*gridCols=*/2,
                                          /*gridRows=*/1, cellImages,
                                          AVIF_ADD_IMAGE_FLAG_SINGLE));
    AVIF_CHECKRES(avifEncoderFinish(encoder.get(), encoded));
  } else {
    assert(input_type == InputType::kAnimation);
    AVIF_CHECKRES(avifEncoderAddImage(encoder.get(), image.get(),
                                      /*durationInTimescales=*/1,
                                      AVIF_ADD_IMAGE_FLAG_NONE));
    AVIF_CHECKRES(avifEncoderAddImage(encoder.get(), image.get(),
                                      /*durationInTimescales=*/1,
                                      AVIF_ADD_IMAGE_FLAG_NONE));
    AVIF_CHECKRES(avifEncoderFinish(encoder.get(), encoded));
  }
  return AVIF_RESULT_OK;
}

class RangeTest
    : public testing::TestWithParam<std::tuple<avifRange, InputType>> {};

TEST_P(RangeTest, DifferentVideoRangeInColrAndMdat) {
  const avifRange obu_range = std::get<0>(GetParam());
  const InputType input_type = std::get<1>(GetParam());
  testutil::AvifRwData encoded;
  ASSERT_EQ(GenerateEncodedData(obu_range, input_type, &encoded),
            AVIF_RESULT_OK);

  // Set full_range_flag in the "colr" box to a different value.
  // This creates an invalid bitstream according to AV1-ISOBMFF v1.2.0 but
  // libavif still allows it.
  const avifRange colr_range =
      (obu_range == AVIF_RANGE_LIMITED) ? AVIF_RANGE_FULL : AVIF_RANGE_LIMITED;
  const uint8_t kColrBoxTag[] = "colr";
  uint8_t* colr_box = std::search(encoded.data, encoded.data + encoded.size,
                                  kColrBoxTag, kColrBoxTag + 4);
  do {
    ASSERT_GT(colr_box, encoded.data + 4);
    ASSERT_LT(colr_box, encoded.data + encoded.size);
    const uint32_t colr_box_size = (colr_box[-4] << 24) | (colr_box[-3] << 16) |
                                   (colr_box[-2] << 8) | colr_box[-1];
    ASSERT_EQ(colr_box_size, 19u);
    ASSERT_LT(colr_box + colr_box_size - 4, encoded.data + encoded.size);
    ASSERT_TRUE(std::equal(colr_box + 4, colr_box + 8,
                           reinterpret_cast<const uint8_t*>("nclx")));
    // full_range_flag(1bit)=colr_range, reserved(7bits)=0
    colr_box[colr_box_size - 5] = static_cast<uint8_t>(colr_range << 7);
    colr_box = std::search(colr_box + 4, encoded.data + encoded.size,
                           kColrBoxTag, kColrBoxTag + 4);
  } while (colr_box != encoded.data + encoded.size);

  // Section 12.1.5.1 of ISO 14496-12 (ISOBMFF) says:
  //   If colour information is supplied in both this [colr] box, and also in
  //   the video bitstream, this box takes precedence, and over-rides the
  //   information in the bitstream.
  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  ASSERT_EQ(avifDecoderReadMemory(decoder.get(), decoded.get(), encoded.data,
                                  encoded.size),
            AVIF_RESULT_OK);
  ASSERT_EQ(decoded->yuvRange, colr_range);
}

TEST_P(RangeTest, MissingColr) {
  const avifRange obu_range = std::get<0>(GetParam());
  const InputType input_type = std::get<1>(GetParam());
  testutil::AvifRwData encoded;
  ASSERT_EQ(GenerateEncodedData(obu_range, input_type, &encoded),
            AVIF_RESULT_OK);

  // Remove the "colr" box (by replacing it with a placeholder).
  // This creates an invalid bitstream according to AV1-ISOBMFF v1.2.0 but
  // libavif still allows it.
  const uint8_t kColrBoxTag[] = "colr";
  uint8_t* colr_box = std::search(encoded.data, encoded.data + encoded.size,
                                  kColrBoxTag, kColrBoxTag + 4);
  do {
    ASSERT_LT(colr_box + 4, encoded.data + encoded.size);
    std::memcpy(colr_box, "free", 4);
    colr_box = std::search(colr_box + 4, encoded.data + encoded.size,
                           kColrBoxTag, kColrBoxTag + 4);
  } while (colr_box != encoded.data + encoded.size);

  // Make sure the AV1 OBU range is kept.
  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  ASSERT_EQ(avifDecoderReadMemory(decoder.get(), decoded.get(), encoded.data,
                                  encoded.size),
            AVIF_RESULT_OK);
  ASSERT_EQ(decoded->yuvRange, obu_range);
}

INSTANTIATE_TEST_SUITE_P(
    All, RangeTest,
    testing::Combine(testing::Values(AVIF_RANGE_LIMITED, AVIF_RANGE_FULL),
                     testing::Values(InputType::kStillImage, InputType::kGrid,
                                     InputType::kAnimation)));

}  // namespace
}  // namespace avif
