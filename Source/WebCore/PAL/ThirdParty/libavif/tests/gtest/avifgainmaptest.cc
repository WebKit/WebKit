// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "avif/avif_cxx.h"
#include "avif/internal.h"
#include "avifincrtest_helpers.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

using ::testing::Values;

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

void CheckGainMapMetadataMatches(const avifGainMapMetadata& lhs,
                                 const avifGainMapMetadata& rhs) {
  EXPECT_EQ(lhs.backwardDirection, rhs.backwardDirection);
  EXPECT_EQ(lhs.baseHdrHeadroomN, rhs.baseHdrHeadroomN);
  EXPECT_EQ(lhs.baseHdrHeadroomD, rhs.baseHdrHeadroomD);
  EXPECT_EQ(lhs.alternateHdrHeadroomN, rhs.alternateHdrHeadroomN);
  EXPECT_EQ(lhs.alternateHdrHeadroomD, rhs.alternateHdrHeadroomD);
  for (int c = 0; c < 3; ++c) {
    SCOPED_TRACE(c);
    EXPECT_EQ(lhs.baseOffsetN[c], rhs.baseOffsetN[c]);
    EXPECT_EQ(lhs.baseOffsetD[c], rhs.baseOffsetD[c]);
    EXPECT_EQ(lhs.alternateOffsetN[c], rhs.alternateOffsetN[c]);
    EXPECT_EQ(lhs.alternateOffsetD[c], rhs.alternateOffsetD[c]);
    EXPECT_EQ(lhs.gainMapGammaN[c], rhs.gainMapGammaN[c]);
    EXPECT_EQ(lhs.gainMapGammaD[c], rhs.gainMapGammaD[c]);
    EXPECT_EQ(lhs.gainMapMinN[c], rhs.gainMapMinN[c]);
    EXPECT_EQ(lhs.gainMapMinD[c], rhs.gainMapMinD[c]);
    EXPECT_EQ(lhs.gainMapMaxN[c], rhs.gainMapMaxN[c]);
    EXPECT_EQ(lhs.gainMapMaxD[c], rhs.gainMapMaxD[c]);
  }
}

avifGainMapMetadata GetTestGainMapMetadata(bool base_rendition_is_hdr) {
  avifGainMapMetadata metadata = {};
  metadata.backwardDirection = base_rendition_is_hdr;
  metadata.useBaseColorSpace = true;
  metadata.baseHdrHeadroomN = 0;
  metadata.baseHdrHeadroomD = 1;
  metadata.alternateHdrHeadroomN = 6;
  metadata.alternateHdrHeadroomD = 2;
  for (int c = 0; c < 3; ++c) {
    metadata.baseOffsetN[c] = 10 * c;
    metadata.baseOffsetD[c] = 1000;
    metadata.alternateOffsetN[c] = 20 * c;
    metadata.alternateOffsetD[c] = 1000;
    metadata.gainMapGammaN[c] = 1;
    metadata.gainMapGammaD[c] = c + 1;
    metadata.gainMapMinN[c] = -1;
    metadata.gainMapMinD[c] = c + 1;
    metadata.gainMapMaxN[c] = 10 + c + 1;
    metadata.gainMapMaxD[c] = c + 1;
  }
  return metadata;
}

ImagePtr CreateTestImageWithGainMap(bool base_rendition_is_hdr) {
  ImagePtr image =
      testutil::CreateImage(/*width=*/12, /*height=*/34, /*depth=*/10,
                            AVIF_PIXEL_FORMAT_YUV420, AVIF_PLANES_ALL);
  if (image == nullptr) {
    return nullptr;
  }
  image->transferCharacteristics =
      (avifTransferCharacteristics)(base_rendition_is_hdr
                                        ? AVIF_TRANSFER_CHARACTERISTICS_PQ
                                        : AVIF_TRANSFER_CHARACTERISTICS_SRGB);
  testutil::FillImageGradient(image.get());
  ImagePtr gain_map =
      testutil::CreateImage(/*width=*/6, /*height=*/17, /*depth=*/8,
                            AVIF_PIXEL_FORMAT_YUV420, AVIF_PLANES_YUV);
  if (gain_map == nullptr) {
    return nullptr;
  }
  testutil::FillImageGradient(gain_map.get());
  image->gainMap = avifGainMapCreate();
  if (image->gainMap == nullptr) {
    return nullptr;
  }
  image->gainMap->image = gain_map.release();  // 'image' now owns the gain map.
  image->gainMap->metadata = GetTestGainMapMetadata(base_rendition_is_hdr);

  if (base_rendition_is_hdr) {
    image->clli.maxCLL = 10;
    image->clli.maxPALL = 5;
    image->gainMap->altDepth = 8;
    image->gainMap->altPlaneCount = 3;
    image->gainMap->altColorPrimaries = AVIF_COLOR_PRIMARIES_BT601;
    image->gainMap->altTransferCharacteristics =
        AVIF_TRANSFER_CHARACTERISTICS_SRGB;
    image->gainMap->altMatrixCoefficients = AVIF_MATRIX_COEFFICIENTS_SMPTE2085;
  } else {
    image->gainMap->altCLLI.maxCLL = 10;
    image->gainMap->altCLLI.maxPALL = 5;
    image->gainMap->altDepth = 10;
    image->gainMap->altPlaneCount = 3;
    image->gainMap->altColorPrimaries = AVIF_COLOR_PRIMARIES_BT2020;
    image->gainMap->altTransferCharacteristics =
        AVIF_TRANSFER_CHARACTERISTICS_PQ;
    image->gainMap->altMatrixCoefficients = AVIF_MATRIX_COEFFICIENTS_SMPTE2085;
  }

  return image;
}

TEST(GainMapTest, EncodeDecodeBaseImageSdr) {
  ImagePtr image = CreateTestImageWithGainMap(/*base_rendition_is_hdr=*/false);

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  testutil::AvifRwData encoded;
  avifResult result = avifEncoderWrite(encoder.get(), image.get(), &encoded);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << encoder->diag.error;

  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  decoder->enableDecodingGainMap = AVIF_TRUE;
  decoder->enableParsingGainMapMetadata = AVIF_TRUE;

  result = avifDecoderSetIOMemory(decoder.get(), encoded.data, encoded.size);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;

  // Just parse the image first.
  result = avifDecoderParse(decoder.get());
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;
  avifImage* decoded = decoder->image;
  ASSERT_NE(decoded, nullptr);

  // Verify that the gain map is present and matches the input.
  EXPECT_TRUE(decoder->gainMapPresent);
  ASSERT_NE(decoded->gainMap, nullptr);
  ASSERT_NE(decoded->gainMap->image, nullptr);
  EXPECT_EQ(decoded->gainMap->image->matrixCoefficients,
            image->gainMap->image->matrixCoefficients);
  EXPECT_EQ(decoded->gainMap->altCLLI.maxCLL, image->gainMap->altCLLI.maxCLL);
  EXPECT_EQ(decoded->gainMap->altCLLI.maxPALL, image->gainMap->altCLLI.maxPALL);
  EXPECT_EQ(decoded->gainMap->altDepth, 10u);
  EXPECT_EQ(decoded->gainMap->altPlaneCount, 3u);
  EXPECT_EQ(decoded->gainMap->altColorPrimaries, AVIF_COLOR_PRIMARIES_BT2020);
  EXPECT_EQ(decoded->gainMap->altTransferCharacteristics,
            AVIF_TRANSFER_CHARACTERISTICS_PQ);
  EXPECT_EQ(decoded->gainMap->altMatrixCoefficients,
            AVIF_MATRIX_COEFFICIENTS_SMPTE2085);
  EXPECT_EQ(decoded->gainMap->image->width, image->gainMap->image->width);
  EXPECT_EQ(decoded->gainMap->image->height, image->gainMap->image->height);
  EXPECT_EQ(decoded->gainMap->image->depth, image->gainMap->image->depth);
  CheckGainMapMetadataMatches(decoded->gainMap->metadata,
                              image->gainMap->metadata);

  // Decode the image.
  result = avifDecoderNextImage(decoder.get());
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;

  // Verify that the input and decoded images are close.
  EXPECT_GT(testutil::GetPsnr(*image, *decoded), 40.0);
  EXPECT_GT(testutil::GetPsnr(*image->gainMap->image, *decoded->gainMap->image),
            40.0);

  // Uncomment the following to save the encoded image as an AVIF file.
  //  std::ofstream("/tmp/avifgainmaptest_basesdr.avif", std::ios::binary)
  //      .write(reinterpret_cast<char*>(encoded.data), encoded.size);
}

TEST(GainMapTest, EncodeDecodeBaseImageHdr) {
  ImagePtr image = CreateTestImageWithGainMap(/*base_rendition_is_hdr=*/true);

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  testutil::AvifRwData encoded;
  avifResult result = avifEncoderWrite(encoder.get(), image.get(), &encoded);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << encoder->diag.error;

  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  decoder->enableDecodingGainMap = AVIF_TRUE;
  decoder->enableParsingGainMapMetadata = AVIF_TRUE;
  result = avifDecoderReadMemory(decoder.get(), decoded.get(), encoded.data,
                                 encoded.size);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;

  // Verify that the input and decoded images are close.
  EXPECT_GT(testutil::GetPsnr(*image, *decoded), 40.0);
  // Verify that the gain map is present and matches the input.
  EXPECT_TRUE(decoder->gainMapPresent);
  ASSERT_NE(decoded->gainMap, nullptr);
  ASSERT_NE(decoded->gainMap->image, nullptr);
  EXPECT_GT(testutil::GetPsnr(*image->gainMap->image, *decoded->gainMap->image),
            40.0);
  EXPECT_EQ(decoded->clli.maxCLL, image->clli.maxCLL);
  EXPECT_EQ(decoded->clli.maxPALL, image->clli.maxPALL);
  EXPECT_EQ(decoded->gainMap->altCLLI.maxCLL, 0u);
  EXPECT_EQ(decoded->gainMap->altCLLI.maxPALL, 0u);
  EXPECT_EQ(decoded->gainMap->altDepth, 8u);
  EXPECT_EQ(decoded->gainMap->altPlaneCount, 3u);
  EXPECT_EQ(decoded->gainMap->altColorPrimaries, AVIF_COLOR_PRIMARIES_BT601);
  EXPECT_EQ(decoded->gainMap->altTransferCharacteristics,
            AVIF_TRANSFER_CHARACTERISTICS_SRGB);
  EXPECT_EQ(decoded->gainMap->altMatrixCoefficients,
            AVIF_MATRIX_COEFFICIENTS_SMPTE2085);
  EXPECT_EQ(decoded->gainMap->image->width, image->gainMap->image->width);
  EXPECT_EQ(decoded->gainMap->image->height, image->gainMap->image->height);
  EXPECT_EQ(decoded->gainMap->image->depth, image->gainMap->image->depth);
  CheckGainMapMetadataMatches(decoded->gainMap->metadata,
                              image->gainMap->metadata);

  // Uncomment the following to save the encoded image as an AVIF file.
  //  std::ofstream("/tmp/avifgainmaptest_basehdr.avif", std::ios::binary)
  //      .write(reinterpret_cast<char*>(encoded.data), encoded.size);
}

TEST(GainMapTest, EncodeDecodeMetadataSameDenominator) {
  ImagePtr image = CreateTestImageWithGainMap(/*base_rendition_is_hdr=*/true);

  const uint32_t kDenominator = 1000;
  image->gainMap->metadata.baseHdrHeadroomD = kDenominator;
  image->gainMap->metadata.alternateHdrHeadroomD = kDenominator;
  for (int c = 0; c < 3; ++c) {
    image->gainMap->metadata.baseOffsetD[c] = kDenominator;
    image->gainMap->metadata.alternateOffsetD[c] = kDenominator;
    image->gainMap->metadata.gainMapGammaD[c] = kDenominator;
    image->gainMap->metadata.gainMapMinD[c] = kDenominator;
    image->gainMap->metadata.gainMapMaxD[c] = kDenominator;
  }

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  testutil::AvifRwData encoded;
  avifResult result = avifEncoderWrite(encoder.get(), image.get(), &encoded);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << encoder->diag.error;

  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  decoder->enableDecodingGainMap = AVIF_FALSE;
  decoder->enableParsingGainMapMetadata = AVIF_TRUE;
  result = avifDecoderReadMemory(decoder.get(), decoded.get(), encoded.data,
                                 encoded.size);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;

  // Verify that the gain map metadata matches the input.
  CheckGainMapMetadataMatches(decoded->gainMap->metadata,
                              image->gainMap->metadata);
}

TEST(GainMapTest, EncodeDecodeMetadataAllChannelsIdentical) {
  ImagePtr image = CreateTestImageWithGainMap(/*base_rendition_is_hdr=*/true);

  for (int c = 0; c < 3; ++c) {
    image->gainMap->metadata.baseOffsetN[c] = 1;
    image->gainMap->metadata.baseOffsetD[c] = 2;
    image->gainMap->metadata.alternateOffsetN[c] = 3;
    image->gainMap->metadata.alternateOffsetD[c] = 4;
    image->gainMap->metadata.gainMapGammaN[c] = 5;
    image->gainMap->metadata.gainMapGammaD[c] = 6;
    image->gainMap->metadata.gainMapMinN[c] = 7;
    image->gainMap->metadata.gainMapMinD[c] = 8;
    image->gainMap->metadata.gainMapMaxN[c] = 9;
    image->gainMap->metadata.gainMapMaxD[c] = 10;
  }

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  testutil::AvifRwData encoded;
  avifResult result = avifEncoderWrite(encoder.get(), image.get(), &encoded);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << encoder->diag.error;

  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  decoder->enableDecodingGainMap = AVIF_FALSE;
  decoder->enableParsingGainMapMetadata = AVIF_TRUE;
  result = avifDecoderReadMemory(decoder.get(), decoded.get(), encoded.data,
                                 encoded.size);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;

  // Verify that the gain map metadata matches the input.
  CheckGainMapMetadataMatches(decoded->gainMap->metadata,
                              image->gainMap->metadata);
}

TEST(GainMapTest, EncodeDecodeGrid) {
  std::vector<ImagePtr> cells;
  std::vector<const avifImage*> cell_ptrs;
  std::vector<const avifImage*> gain_map_ptrs;
  constexpr int kGridCols = 2;
  constexpr int kGridRows = 2;
  constexpr int kCellWidth = 128;
  constexpr int kCellHeight = 200;

  avifGainMapMetadata gain_map_metadata =
      GetTestGainMapMetadata(/*base_rendition_is_hdr=*/true);

  for (int i = 0; i < kGridCols * kGridRows; ++i) {
    ImagePtr image =
        testutil::CreateImage(kCellWidth, kCellHeight, /*depth=*/10,
                              AVIF_PIXEL_FORMAT_YUV444, AVIF_PLANES_ALL);
    ASSERT_NE(image, nullptr);
    image->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_PQ;
    testutil::FillImageGradient(image.get());
    ImagePtr gain_map =
        testutil::CreateImage(kCellWidth / 2, kCellHeight / 2, /*depth=*/8,
                              AVIF_PIXEL_FORMAT_YUV420, AVIF_PLANES_YUV);
    ASSERT_NE(gain_map, nullptr);
    testutil::FillImageGradient(gain_map.get());
    // 'image' now owns the gain map.
    image->gainMap = avifGainMapCreate();
    ASSERT_NE(image->gainMap, nullptr);
    image->gainMap->image = gain_map.release();
    // all cells must have the same metadata
    image->gainMap->metadata = gain_map_metadata;

    cell_ptrs.push_back(image.get());
    gain_map_ptrs.push_back(image->gainMap->image);
    cells.push_back(std::move(image));
  }

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  testutil::AvifRwData encoded;
  avifResult result =
      avifEncoderAddImageGrid(encoder.get(), kGridCols, kGridRows,
                              cell_ptrs.data(), AVIF_ADD_IMAGE_FLAG_SINGLE);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << encoder->diag.error;
  result = avifEncoderFinish(encoder.get(), &encoded);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << encoder->diag.error;

  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  decoder->enableDecodingGainMap = AVIF_TRUE;
  decoder->enableParsingGainMapMetadata = AVIF_TRUE;
  result = avifDecoderReadMemory(decoder.get(), decoded.get(), encoded.data,
                                 encoded.size);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;

  ImagePtr merged = testutil::CreateImage(
      static_cast<int>(decoded->width), static_cast<int>(decoded->height),
      decoded->depth, decoded->yuvFormat, AVIF_PLANES_ALL);
  ASSERT_EQ(testutil::MergeGrid(kGridCols, kGridRows, cell_ptrs, merged.get()),
            AVIF_RESULT_OK);

  ImagePtr merged_gain_map = testutil::CreateImage(
      static_cast<int>(decoded->gainMap->image->width),
      static_cast<int>(decoded->gainMap->image->height),
      decoded->gainMap->image->depth, decoded->gainMap->image->yuvFormat,
      AVIF_PLANES_YUV);
  ASSERT_EQ(testutil::MergeGrid(kGridCols, kGridRows, gain_map_ptrs,
                                merged_gain_map.get()),
            AVIF_RESULT_OK);

  // Verify that the input and decoded images are close.
  ASSERT_GT(testutil::GetPsnr(*merged, *decoded), 40.0);
  // Verify that the gain map is present and matches the input.
  EXPECT_TRUE(decoder->gainMapPresent);
  ASSERT_NE(decoded->gainMap, nullptr);
  ASSERT_NE(decoded->gainMap->image, nullptr);
  ASSERT_GT(testutil::GetPsnr(*merged_gain_map, *decoded->gainMap->image),
            40.0);
  CheckGainMapMetadataMatches(decoded->gainMap->metadata, gain_map_metadata);

  // Check that non-incremental and incremental decodings of a grid AVIF produce
  // the same pixels.
  ASSERT_EQ(testutil::DecodeNonIncrementallyAndIncrementally(
                encoded, decoder.get(),
                /*is_persistent=*/true, /*give_size_hint=*/true,
                /*use_nth_image_api=*/false, kCellHeight,
                /*enable_fine_incremental_check=*/true),
            AVIF_RESULT_OK);

  // Uncomment the following to save the encoded image as an AVIF file.
  //  std::ofstream("/tmp/avifgainmaptest_grid.avif", std::ios::binary)
  //      .write(reinterpret_cast<char*>(encoded.data), encoded.size);
}

TEST(GainMapTest, InvalidGrid) {
  std::vector<ImagePtr> cells;
  std::vector<const avifImage*> cell_ptrs;
  constexpr int kGridCols = 2;
  constexpr int kGridRows = 2;

  avifGainMapMetadata gain_map_metadata =
      GetTestGainMapMetadata(/*base_rendition_is_hdr=*/true);

  for (int i = 0; i < kGridCols * kGridRows; ++i) {
    ImagePtr image =
        testutil::CreateImage(/*width=*/64, /*height=*/100, /*depth=*/10,
                              AVIF_PIXEL_FORMAT_YUV444, AVIF_PLANES_ALL);
    ASSERT_NE(image, nullptr);
    image->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_PQ;
    testutil::FillImageGradient(image.get());
    ImagePtr gain_map =
        testutil::CreateImage(/*width=*/64, /*height=*/100, /*depth=*/8,
                              AVIF_PIXEL_FORMAT_YUV420, AVIF_PLANES_YUV);
    ASSERT_NE(gain_map, nullptr);
    testutil::FillImageGradient(gain_map.get());
    // 'image' now owns the gain map.
    image->gainMap = avifGainMapCreate();
    ASSERT_NE(image->gainMap, nullptr);
    image->gainMap->image = gain_map.release();
    // all cells must have the same metadata
    image->gainMap->metadata = gain_map_metadata;

    cell_ptrs.push_back(image.get());
    cells.push_back(std::move(image));
  }

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  testutil::AvifRwData encoded;

  avifResult result;

  // Invalid: one cell has the wrong size.
  cells[1]->gainMap->image->height = 90;
  result =
      avifEncoderAddImageGrid(encoder.get(), kGridCols, kGridRows,
                              cell_ptrs.data(), AVIF_ADD_IMAGE_FLAG_SINGLE);
  EXPECT_EQ(result, AVIF_RESULT_INVALID_IMAGE_GRID)
      << avifResultToString(result) << " " << encoder->diag.error;
  cells[1]->gainMap->image->height =
      cells[0]->gainMap->image->height;  // Revert.

  // Invalid: one cell has a different depth.
  cells[1]->gainMap->image->depth = 12;
  result =
      avifEncoderAddImageGrid(encoder.get(), kGridCols, kGridRows,
                              cell_ptrs.data(), AVIF_ADD_IMAGE_FLAG_SINGLE);
  EXPECT_EQ(result, AVIF_RESULT_INVALID_IMAGE_GRID)
      << avifResultToString(result) << " " << encoder->diag.error;
  cells[1]->gainMap->image->depth = cells[0]->gainMap->image->depth;  // Revert.

  // Invalid: one cell has different gain map metadata.
  cells[1]->gainMap->metadata.gainMapGammaN[0] = 42;
  result =
      avifEncoderAddImageGrid(encoder.get(), kGridCols, kGridRows,
                              cell_ptrs.data(), AVIF_ADD_IMAGE_FLAG_SINGLE);
  EXPECT_EQ(result, AVIF_RESULT_INVALID_IMAGE_GRID)
      << avifResultToString(result) << " " << encoder->diag.error;
  cells[1]->gainMap->metadata.gainMapGammaN[0] =
      cells[0]->gainMap->metadata.gainMapGammaN[0];  // Revert.
}

TEST(GainMapTest, SequenceNotSupported) {
  ImagePtr image =
      testutil::CreateImage(/*width=*/64, /*height=*/100, /*depth=*/10,
                            AVIF_PIXEL_FORMAT_YUV444, AVIF_PLANES_ALL);
  ASSERT_NE(image, nullptr);
  image->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_PQ;
  testutil::FillImageGradient(image.get());
  ImagePtr gain_map =
      testutil::CreateImage(/*width=*/64, /*height=*/100, /*depth=*/8,
                            AVIF_PIXEL_FORMAT_YUV420, AVIF_PLANES_YUV);
  ASSERT_NE(gain_map, nullptr);
  testutil::FillImageGradient(gain_map.get());
  // 'image' now owns the gain map.
  image->gainMap = avifGainMapCreate();
  ASSERT_NE(image->gainMap, nullptr);
  image->gainMap->image = gain_map.release();
  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  testutil::AvifRwData encoded;
  // Add a first frame.
  avifResult result =
      avifEncoderAddImage(encoder.get(), image.get(),
                          /*durationInTimescales=*/2, AVIF_ADD_IMAGE_FLAG_NONE);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << encoder->diag.error;
  // Add a second frame.
  result =
      avifEncoderAddImage(encoder.get(), image.get(),
                          /*durationInTimescales=*/2, AVIF_ADD_IMAGE_FLAG_NONE);
  // Image sequences with gain maps are not supported.
  ASSERT_EQ(result, AVIF_RESULT_NOT_IMPLEMENTED)
      << avifResultToString(result) << " " << encoder->diag.error;
}

TEST(GainMapTest, IgnoreGainMap) {
  ImagePtr image = CreateTestImageWithGainMap(/*base_rendition_is_hdr=*/false);
  ASSERT_NE(image, nullptr);

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  testutil::AvifRwData encoded;
  avifResult result = avifEncoderWrite(encoder.get(), image.get(), &encoded);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << encoder->diag.error;

  // Decode image, with enableDecodingGainMap false by default.
  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  result = avifDecoderReadMemory(decoder.get(), decoded.get(), encoded.data,
                                 encoded.size);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;

  // Verify that the input and decoded images are close.
  EXPECT_GT(testutil::GetPsnr(*image, *decoded), 40.0);
  // Verify that the gain map was detected...
  EXPECT_TRUE(decoder->gainMapPresent);
  // ... but not decoded because enableDecodingGainMap is false by default.
  EXPECT_EQ(decoded->gainMap, nullptr);
}

TEST(GainMapTest, IgnoreGainMapButReadMetadata) {
  ImagePtr image = CreateTestImageWithGainMap(/*base_rendition_is_hdr=*/false);
  ASSERT_NE(image, nullptr);

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  testutil::AvifRwData encoded;
  avifResult result = avifEncoderWrite(encoder.get(), image.get(), &encoded);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << encoder->diag.error;

  // Decode image, with enableDecodingGainMap false by default.
  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  decoder->enableParsingGainMapMetadata = AVIF_TRUE;  // Read gain map metadata.
  result = avifDecoderReadMemory(decoder.get(), decoded.get(), encoded.data,
                                 encoded.size);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;

  // Verify that the input and decoded images are close.
  EXPECT_GT(testutil::GetPsnr(*image, *decoded), 40.0);
  // Verify that the gain map was detected...
  EXPECT_TRUE(decoder->gainMapPresent);
  ASSERT_NE(decoded->gainMap, nullptr);
  // ... but not decoded because enableDecodingGainMap is false by default.
  EXPECT_EQ(decoded->gainMap->image, nullptr);
  // Check that the gain map metadata WAS populated.
  CheckGainMapMetadataMatches(decoded->gainMap->metadata,
                              image->gainMap->metadata);
  EXPECT_EQ(decoded->gainMap->altDepth, image->gainMap->altDepth);
  EXPECT_EQ(decoded->gainMap->altPlaneCount, image->gainMap->altPlaneCount);
  EXPECT_EQ(decoded->gainMap->altColorPrimaries,
            image->gainMap->altColorPrimaries);
  EXPECT_EQ(decoded->gainMap->altTransferCharacteristics,
            image->gainMap->altTransferCharacteristics);
  EXPECT_EQ(decoded->gainMap->altMatrixCoefficients,
            image->gainMap->altMatrixCoefficients);
}

TEST(GainMapTest, DecodeGainMapButIgnoreMetadata) {
  ImagePtr image = CreateTestImageWithGainMap(/*base_rendition_is_hdr=*/false);
  ASSERT_NE(image, nullptr);

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  testutil::AvifRwData encoded;
  avifResult result = avifEncoderWrite(encoder.get(), image.get(), &encoded);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << encoder->diag.error;

  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  decoder->enableParsingGainMapMetadata = AVIF_FALSE;
  decoder->enableDecodingGainMap = AVIF_TRUE;
  result = avifDecoderReadMemory(decoder.get(), decoded.get(), encoded.data,
                                 encoded.size);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;

  // Verify that the gain map was detected...
  EXPECT_TRUE(decoder->gainMapPresent);
  ASSERT_NE(decoded->gainMap, nullptr);
  ASSERT_NE(decoded->gainMap->image, nullptr);
  EXPECT_GT(testutil::GetPsnr(*image->gainMap->image, *decoded->gainMap->image),
            40.0);

  // Check that the gain map metadata was not populated.
  CheckGainMapMetadataMatches(decoded->gainMap->metadata,
                              avifGainMapMetadata());
  EXPECT_EQ(decoded->gainMap->altDepth, 0);
  EXPECT_EQ(decoded->gainMap->altPlaneCount, 0);
  EXPECT_EQ(decoded->gainMap->altColorPrimaries,
            AVIF_COLOR_PRIMARIES_UNSPECIFIED);
  EXPECT_EQ(decoded->gainMap->altTransferCharacteristics,
            AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED);
  EXPECT_EQ(decoded->gainMap->altMatrixCoefficients,
            AVIF_MATRIX_COEFFICIENTS_UNSPECIFIED);
}

TEST(GainMapTest, IgnoreColorAndAlpha) {
  ImagePtr image = CreateTestImageWithGainMap(/*base_rendition_is_hdr=*/false);
  ASSERT_NE(image, nullptr);

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  testutil::AvifRwData encoded;
  avifResult result = avifEncoderWrite(encoder.get(), image.get(), &encoded);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << encoder->diag.error;

  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  // Decode just the gain map.
  decoder->ignoreColorAndAlpha = AVIF_TRUE;
  decoder->enableDecodingGainMap = AVIF_TRUE;
  decoder->enableParsingGainMapMetadata = AVIF_TRUE;
  result = avifDecoderReadMemory(decoder.get(), decoded.get(), encoded.data,
                                 encoded.size);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;

  // Main image metadata is available.
  EXPECT_EQ(decoder->image->width, 12u);
  EXPECT_EQ(decoder->image->height, 34u);
  // But pixels are not.
  EXPECT_EQ(decoder->image->yuvRowBytes[0], 0u);
  EXPECT_EQ(decoder->image->yuvRowBytes[1], 0u);
  EXPECT_EQ(decoder->image->yuvRowBytes[2], 0u);
  EXPECT_EQ(decoder->image->alphaRowBytes, 0u);
  // The gain map was decoded.
  EXPECT_TRUE(decoder->gainMapPresent);
  ASSERT_NE(decoded->gainMap, nullptr);
  ASSERT_NE(decoded->gainMap->image, nullptr);
  EXPECT_GT(testutil::GetPsnr(*image->gainMap->image, *decoded->gainMap->image),
            40.0);
  CheckGainMapMetadataMatches(decoded->gainMap->metadata,
                              image->gainMap->metadata);
}

TEST(GainMapTest, IgnoreAll) {
  ImagePtr image = CreateTestImageWithGainMap(/*base_rendition_is_hdr=*/false);
  ASSERT_NE(image, nullptr);

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  testutil::AvifRwData encoded;
  avifResult result = avifEncoderWrite(encoder.get(), image.get(), &encoded);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << encoder->diag.error;

  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  // Ignore both the main image and the gain map.
  decoder->ignoreColorAndAlpha = AVIF_TRUE;
  decoder->enableDecodingGainMap = AVIF_FALSE;
  // But do read the gain map metadata.
  decoder->enableParsingGainMapMetadata = AVIF_TRUE;

  // Parsing just the header should work.
  ASSERT_EQ(avifDecoderSetIOMemory(decoder.get(), encoded.data, encoded.size),
            AVIF_RESULT_OK);
  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);

  EXPECT_TRUE(decoder->gainMapPresent);
  CheckGainMapMetadataMatches(decoder->image->gainMap->metadata,
                              image->gainMap->metadata);
  ASSERT_EQ(decoder->image->gainMap->image, nullptr);

  // But trying to access the next image should give an error because both
  // ignoreColorAndAlpha and enableDecodingGainMap are set.
  ASSERT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_NO_CONTENT);
}

TEST(GainMapTest, NoGainMap) {
  // Create a simple image without a gain map.
  ImagePtr image =
      testutil::CreateImage(/*width=*/12, /*height=*/34, /*depth=*/10,
                            AVIF_PIXEL_FORMAT_YUV420, AVIF_PLANES_ALL);
  ASSERT_NE(image, nullptr);
  image->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_SRGB;
  testutil::FillImageGradient(image.get());
  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  testutil::AvifRwData encoded;
  avifResult result = avifEncoderWrite(encoder.get(), image.get(), &encoded);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << encoder->diag.error;

  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  // Enable gain map decoding.
  decoder->enableDecodingGainMap = AVIF_TRUE;
  decoder->enableParsingGainMapMetadata = AVIF_TRUE;
  result = avifDecoderReadMemory(decoder.get(), decoded.get(), encoded.data,
                                 encoded.size);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;

  // Verify that the input and decoded images are close.
  EXPECT_GT(testutil::GetPsnr(*image, *decoded), 40.0);
  // Verify that no gain map was found.
  EXPECT_FALSE(decoder->gainMapPresent);
  EXPECT_EQ(decoded->gainMap, nullptr);
}

TEST(GainMapTest, DecodeGainMapGrid) {
  const std::string path =
      std::string(data_path) + "color_grid_gainmap_different_grid.avif";
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  decoder->enableDecodingGainMap = true;
  decoder->enableParsingGainMapMetadata = true;

  avifResult result = avifDecoderSetIOFile(decoder.get(), path.c_str());
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;

  // Just parse the image first.
  result = avifDecoderParse(decoder.get());
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;
  avifImage* decoded = decoder->image;
  ASSERT_NE(decoded, nullptr);

  // Verify that the gain map is present and matches the input.
  EXPECT_TRUE(decoder->gainMapPresent);
  // Color+alpha: 4x3 grid of 128x200 tiles.
  EXPECT_EQ(decoded->width, 128u * 4u);
  EXPECT_EQ(decoded->height, 200u * 3u);
  EXPECT_EQ(decoded->depth, 10u);
  ASSERT_NE(decoded->gainMap, nullptr);
  ASSERT_NE(decoded->gainMap->image, nullptr);
  // Gain map: 2x2 grid of 64x80 tiles.
  EXPECT_EQ(decoded->gainMap->image->width, 64u * 2u);
  EXPECT_EQ(decoded->gainMap->image->height, 80u * 2u);
  EXPECT_EQ(decoded->gainMap->image->depth, 8u);
  EXPECT_EQ(decoded->gainMap->metadata.alternateHdrHeadroomN, 6u);
  EXPECT_EQ(decoded->gainMap->metadata.alternateHdrHeadroomD, 2u);

  // Decode the image.
  result = avifDecoderNextImage(decoder.get());
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;
}

TEST(GainMapTest, DecodeColorGridGainMapNoGrid) {
  const std::string path =
      std::string(data_path) + "color_grid_alpha_grid_gainmap_nogrid.avif";
  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  decoder->enableDecodingGainMap = true;
  decoder->enableParsingGainMapMetadata = true;
  ASSERT_EQ(avifDecoderReadFile(decoder.get(), decoded.get(), path.c_str()),
            AVIF_RESULT_OK);

  // Color+alpha: 4x3 grid of 128x200 tiles.
  EXPECT_EQ(decoded->width, 128u * 4u);
  EXPECT_EQ(decoded->height, 200u * 3u);
  ASSERT_NE(decoded->gainMap, nullptr);
  ASSERT_NE(decoded->gainMap->image, nullptr);
  // Gain map: single image of size 64x80.
  EXPECT_EQ(decoded->gainMap->image->width, 64u);
  EXPECT_EQ(decoded->gainMap->image->height, 80u);
  EXPECT_EQ(decoded->gainMap->metadata.alternateHdrHeadroomN, 6u);
  EXPECT_EQ(decoded->gainMap->metadata.alternateHdrHeadroomD, 2u);
}

TEST(GainMapTest, DecodeColorNoGridGainMapGrid) {
  const std::string path =
      std::string(data_path) + "color_nogrid_alpha_nogrid_gainmap_grid.avif";
  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  decoder->enableDecodingGainMap = true;
  decoder->enableParsingGainMapMetadata = true;
  ASSERT_EQ(avifDecoderReadFile(decoder.get(), decoded.get(), path.c_str()),
            AVIF_RESULT_OK);

  // Color+alpha: single image of size 128x200 .
  EXPECT_EQ(decoded->width, 128u);
  EXPECT_EQ(decoded->height, 200u);
  ASSERT_NE(decoded->gainMap, nullptr);
  ASSERT_NE(decoded->gainMap->image, nullptr);
  // Gain map: 2x2 grid of 64x80 tiles.
  EXPECT_EQ(decoded->gainMap->image->width, 64u * 2u);
  EXPECT_EQ(decoded->gainMap->image->height, 80u * 2u);
  EXPECT_EQ(decoded->gainMap->metadata.alternateHdrHeadroomN, 6u);
  EXPECT_EQ(decoded->gainMap->metadata.alternateHdrHeadroomD, 2u);
}

#define EXPECT_FRACTION_NEAR(numerator, denominator, expected)     \
  EXPECT_NEAR(std::abs((double)numerator / denominator), expected, \
              expected * 0.001);

TEST(GainMapTest, ConvertMetadata) {
  avifGainMapMetadataDouble metadata_double = {};
  metadata_double.gainMapMin[0] = 1.0;
  metadata_double.gainMapMin[1] = 1.1;
  metadata_double.gainMapMin[2] = 1.2;
  metadata_double.gainMapMax[0] = 10.0;
  metadata_double.gainMapMax[1] = 10.1;
  metadata_double.gainMapMax[2] = 10.2;
  metadata_double.gainMapGamma[0] = 1.0;
  metadata_double.gainMapGamma[1] = 1.0;
  metadata_double.gainMapGamma[2] = 1.2;
  metadata_double.baseOffset[0] = 1.0 / 32.0;
  metadata_double.baseOffset[1] = 1.0 / 64.0;
  metadata_double.baseOffset[2] = 1.0 / 128.0;
  metadata_double.alternateOffset[0] = 0.004564;
  metadata_double.alternateOffset[1] = 0.0;
  metadata_double.baseHdrHeadroom = 1.0;
  metadata_double.alternateHdrHeadroom = 10.0;
  metadata_double.backwardDirection = AVIF_TRUE;

  // Convert to avifGainMapMetadata.
  avifGainMapMetadata metadata = {};
  ASSERT_TRUE(
      avifGainMapMetadataDoubleToFractions(&metadata, &metadata_double));

  for (int i = 0; i < 3; ++i) {
    EXPECT_FRACTION_NEAR(metadata.gainMapMinN[i], metadata.gainMapMinD[i],
                         metadata_double.gainMapMin[i]);
    EXPECT_FRACTION_NEAR(metadata.gainMapMaxN[i], metadata.gainMapMaxD[i],
                         metadata_double.gainMapMax[i]);
    EXPECT_FRACTION_NEAR(metadata.gainMapGammaN[i], metadata.gainMapGammaD[i],
                         metadata_double.gainMapGamma[i]);
    EXPECT_FRACTION_NEAR(metadata.baseOffsetN[i], metadata.baseOffsetD[i],
                         metadata_double.baseOffset[i]);
    EXPECT_FRACTION_NEAR(metadata.alternateOffsetN[i],
                         metadata.alternateOffsetD[i],
                         metadata_double.alternateOffset[i]);
  }
  EXPECT_FRACTION_NEAR(metadata.baseHdrHeadroomN, metadata.baseHdrHeadroomD,
                       metadata_double.baseHdrHeadroom);
  EXPECT_FRACTION_NEAR(metadata.alternateHdrHeadroomN,
                       metadata.alternateHdrHeadroomD,
                       metadata_double.alternateHdrHeadroom);
  EXPECT_EQ(metadata.backwardDirection, metadata_double.backwardDirection);

  // Convert back to avifGainMapMetadataDouble.
  avifGainMapMetadataDouble metadata_double2 = {};
  ASSERT_TRUE(
      avifGainMapMetadataFractionsToDouble(&metadata_double2, &metadata));

  constexpr double kEpsilon = 0.000001;
  for (int i = 0; i < 3; ++i) {
    EXPECT_NEAR(metadata_double2.gainMapMin[i], metadata_double.gainMapMin[i],
                kEpsilon);
    EXPECT_NEAR(metadata_double2.gainMapMax[i], metadata_double.gainMapMax[i],
                kEpsilon);
    EXPECT_NEAR(metadata_double2.gainMapGamma[i],
                metadata_double.gainMapGamma[i], kEpsilon);
    EXPECT_NEAR(metadata_double2.baseOffset[i], metadata_double.baseOffset[i],
                kEpsilon);
    EXPECT_NEAR(metadata_double2.alternateOffset[i],
                metadata_double.alternateOffset[i], kEpsilon);
  }
  EXPECT_NEAR(metadata_double2.baseHdrHeadroom, metadata_double.baseHdrHeadroom,
              kEpsilon);
  EXPECT_NEAR(metadata_double2.alternateHdrHeadroom,
              metadata_double.alternateHdrHeadroom, kEpsilon);
  EXPECT_EQ(metadata_double2.backwardDirection,
            metadata_double.backwardDirection);
}

TEST(GainMapTest, ConvertMetadataToFractionInvalid) {
  avifGainMapMetadataDouble metadata_double = {};
  metadata_double.gainMapGamma[0] = -42;  // A negative value is invalid!
  avifGainMapMetadata metadata = {};
  ASSERT_FALSE(
      avifGainMapMetadataDoubleToFractions(&metadata, &metadata_double));
}

TEST(GainMapTest, ConvertMetadataToDoubleInvalid) {
  avifGainMapMetadata metadata = {};  // Denominators are zero.
  avifGainMapMetadataDouble metadata_double = {};
  ASSERT_FALSE(
      avifGainMapMetadataFractionsToDouble(&metadata_double, &metadata));
}

static void SwapBaseAndAlternate(const avifImage& new_alternate,
                                 avifGainMap& gain_map) {
  avifGainMapMetadata& metadata = gain_map.metadata;
  metadata.backwardDirection = !metadata.backwardDirection;
  metadata.useBaseColorSpace = !metadata.useBaseColorSpace;
  std::swap(metadata.baseHdrHeadroomN, metadata.alternateHdrHeadroomN);
  std::swap(metadata.baseHdrHeadroomD, metadata.alternateHdrHeadroomD);
  for (int c = 0; c < 3; ++c) {
    std::swap(metadata.baseOffsetN[c], metadata.alternateOffsetN[c]);
    std::swap(metadata.baseOffsetD[c], metadata.alternateOffsetD[c]);
  }
  gain_map.altColorPrimaries = new_alternate.colorPrimaries;
  gain_map.altTransferCharacteristics = new_alternate.transferCharacteristics;
  gain_map.altMatrixCoefficients = new_alternate.matrixCoefficients;
  gain_map.altYUVRange = new_alternate.yuvRange;
  gain_map.altDepth = new_alternate.depth;
  gain_map.altPlaneCount =
      (new_alternate.yuvFormat == AVIF_PIXEL_FORMAT_YUV400) ? 1 : 3;
  gain_map.altCLLI = new_alternate.clli;
}

// Test to generate some test images used by other tests and fuzzers.
// Allows regenerating the images if the gain map format changes.
TEST(GainMapTest, CreateTestImages) {
  // Set to true to update test images.
  constexpr bool kUpdateTestImages = false;

  // Generate seine_sdr_gainmap_big_srgb.jpg
  {
    const std::string path =
        std::string(data_path) + "seine_sdr_gainmap_srgb.avif";
    DecoderPtr decoder(avifDecoderCreate());
    ASSERT_NE(decoder, nullptr);
    decoder->enableDecodingGainMap = true;
    decoder->enableParsingGainMapMetadata = true;

    ImagePtr image(avifImageCreateEmpty());
    ASSERT_NE(image, nullptr);
    avifResult result =
        avifDecoderReadFile(decoder.get(), image.get(), path.c_str());
    ASSERT_EQ(result, AVIF_RESULT_OK)
        << avifResultToString(result) << " " << decoder->diag.error;
    ASSERT_NE(image->gainMap, nullptr);
    ASSERT_NE(image->gainMap->image, nullptr);

    avifDiagnostics diag;
    result =
        avifImageScale(image->gainMap->image, image->gainMap->image->width * 2,
                       image->gainMap->image->height * 2, &diag);
    ASSERT_EQ(result, AVIF_RESULT_OK)
        << avifResultToString(result) << " " << decoder->diag.error;

    const testutil::AvifRwData encoded =
        testutil::Encode(image.get(), /*speed=*/9, /*quality=*/90);
    ASSERT_GT(encoded.size, 0u);
    if (kUpdateTestImages) {
      std::ofstream(std::string(data_path) + "seine_sdr_gainmap_big_srgb.avif",
                    std::ios::binary)
          .write(reinterpret_cast<char*>(encoded.data), encoded.size);
    }
  }

  // Generate seine_hdr_gainmap_srgb.avif and seine_hdr_gainmap_small_srgb.avif
  {
    ImagePtr hdr_image =
        testutil::DecodeFile(std::string(data_path) + "seine_hdr_srgb.avif");
    ASSERT_NE(hdr_image, nullptr);

    const std::string sdr_path =
        std::string(data_path) + "seine_sdr_gainmap_srgb.avif";
    DecoderPtr decoder(avifDecoderCreate());
    ASSERT_NE(decoder, nullptr);
    decoder->enableDecodingGainMap = true;
    decoder->enableParsingGainMapMetadata = true;
    ImagePtr sdr_with_gainmap(avifImageCreateEmpty());
    ASSERT_NE(sdr_with_gainmap, nullptr);
    avifResult result = avifDecoderReadFile(
        decoder.get(), sdr_with_gainmap.get(), sdr_path.c_str());
    ASSERT_EQ(result, AVIF_RESULT_OK)
        << avifResultToString(result) << " " << decoder->diag.error;
    ASSERT_NE(sdr_with_gainmap->gainMap->image, nullptr);

    // Move the gain map from the sdr image to the hdr image.
    hdr_image->gainMap = sdr_with_gainmap->gainMap;
    sdr_with_gainmap->gainMap = nullptr;
    SwapBaseAndAlternate(*sdr_with_gainmap, *hdr_image->gainMap);
    hdr_image->gainMap->altColorPrimaries = sdr_with_gainmap->colorPrimaries;
    hdr_image->gainMap->altTransferCharacteristics =
        sdr_with_gainmap->transferCharacteristics;
    hdr_image->gainMap->altMatrixCoefficients =
        sdr_with_gainmap->matrixCoefficients;
    hdr_image->gainMap->altDepth = sdr_with_gainmap->depth;
    hdr_image->gainMap->altPlaneCount = 3;

    const testutil::AvifRwData encoded =
        testutil::Encode(hdr_image.get(), /*speed=*/9, /*quality=*/90);
    ASSERT_GT(encoded.size, 0u);
    if (kUpdateTestImages) {
      std::ofstream(std::string(data_path) + "seine_hdr_gainmap_srgb.avif",
                    std::ios::binary)
          .write(reinterpret_cast<char*>(encoded.data), encoded.size);
    }

    avifDiagnostics diag;
    result = avifImageScale(hdr_image->gainMap->image,
                            hdr_image->gainMap->image->width / 2,
                            hdr_image->gainMap->image->height / 2, &diag);
    ASSERT_EQ(result, AVIF_RESULT_OK)
        << avifResultToString(result) << " " << decoder->diag.error;

    const testutil::AvifRwData encoded_small_gainmap =
        testutil::Encode(hdr_image.get(), /*speed=*/9, /*quality=*/90);
    ASSERT_GT(encoded.size, 0u);
    if (kUpdateTestImages) {
      std::ofstream(
          std::string(data_path) + "seine_hdr_gainmap_small_srgb.avif",
          std::ios::binary)
          .write(reinterpret_cast<char*>(encoded_small_gainmap.data),
                 encoded_small_gainmap.size);
    }
  }
}

class ToneMapTest
    : public testing::TestWithParam<std::tuple<
          /*source=*/std::string, /*hdr_headroom=*/float,
          /*out_depth=*/int,
          /*out_transfer=*/avifTransferCharacteristics,
          /*out_rgb_format=*/avifRGBFormat,
          /*reference=*/std::string, /*min_psnr=*/float, /*max_psnr=*/float>> {
};

void ToneMapImageAndCompareToReference(
    const avifImage* base_image, const avifGainMap& gain_map,
    float hdr_headroom, int out_depth,
    avifTransferCharacteristics out_transfer_characteristics,
    avifRGBFormat out_rgb_format, const avifImage* reference_image,
    float min_psnr, float max_psnr, float* psnr_out = nullptr) {
  SCOPED_TRACE("hdr_headroom: " + std::to_string(hdr_headroom));

  testutil::AvifRgbImage tone_mapped_rgb(base_image, out_depth, out_rgb_format);
  ImagePtr tone_mapped(
      avifImageCreate(tone_mapped_rgb.width, tone_mapped_rgb.height,
                      tone_mapped_rgb.depth, AVIF_PIXEL_FORMAT_YUV444));
  tone_mapped->transferCharacteristics = out_transfer_characteristics;
  tone_mapped->colorPrimaries = reference_image
                                    ? reference_image->colorPrimaries
                                    : base_image->colorPrimaries;
  tone_mapped->matrixCoefficients = reference_image
                                        ? reference_image->matrixCoefficients
                                        : base_image->matrixCoefficients;

  avifDiagnostics diag;
  avifResult result = avifImageApplyGainMap(
      base_image, &gain_map, hdr_headroom, tone_mapped->colorPrimaries,
      tone_mapped->transferCharacteristics, &tone_mapped_rgb,
      &tone_mapped->clli, &diag);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << diag.error;
  ASSERT_EQ(avifImageRGBToYUV(tone_mapped.get(), &tone_mapped_rgb),
            AVIF_RESULT_OK);
  if (reference_image != nullptr) {
    EXPECT_EQ(out_depth, (int)reference_image->depth);
    const double psnr = testutil::GetPsnr(*reference_image, *tone_mapped);
    std::cout << "PSNR (tone mapped vs reference): " << psnr << "\n";
    EXPECT_GE(psnr, min_psnr);
    EXPECT_LE(psnr, max_psnr);
    if (psnr_out != nullptr) {
      *psnr_out = (float)psnr;
    }
  }

  // Uncomment the following to save the encoded image as an AVIF file.
  // const testutil::AvifRwData encoded =
  //     testutil::Encode(tone_mapped.get(), /*speed=*/9, /*quality=*/90);
  // ASSERT_GT(encoded.size, 0u);
  // std::ofstream("/tmp/tone_mapped_" + std::to_string(hdr_headroom) +
  // ".avif", std::ios::binary)
  //     .write(reinterpret_cast<char*>(encoded.data), encoded.size);
}

TEST_P(ToneMapTest, ToneMapImage) {
  const std::string source = std::get<0>(GetParam());
  const float hdr_headroom = std::get<1>(GetParam());
  // out_depth and out_transfer_characteristics should match the reference image
  // when ther eis one, so that GetPsnr works.
  const int out_depth = std::get<2>(GetParam());
  const avifTransferCharacteristics out_transfer_characteristics =
      std::get<3>(GetParam());
  const avifRGBFormat out_rgb_format = std::get<4>(GetParam());
  const std::string reference = std::get<5>(GetParam());
  const float min_psnr = std::get<6>(GetParam());
  const float max_psnr = std::get<7>(GetParam());

  ImagePtr reference_image = nullptr;
  if (!source.empty()) {
    reference_image = testutil::DecodeFile(std::string(data_path) + reference);
  }

  // Load the source image (that should contain a gain map).
  const std::string path = std::string(data_path) + source;
  ImagePtr image(avifImageCreateEmpty());
  ASSERT_NE(image, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  decoder->enableDecodingGainMap = true;
  decoder->enableParsingGainMapMetadata = true;
  avifResult result =
      avifDecoderReadFile(decoder.get(), image.get(), path.c_str());
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;
  ASSERT_NE(image->gainMap, nullptr);
  ASSERT_NE(image->gainMap->image, nullptr);

  ToneMapImageAndCompareToReference(image.get(), *image->gainMap, hdr_headroom,
                                    out_depth, out_transfer_characteristics,
                                    out_rgb_format, reference_image.get(),
                                    min_psnr, max_psnr);
}

INSTANTIATE_TEST_SUITE_P(
    All, ToneMapTest,
    Values(
        // ------ SDR BASE IMAGE ------

        // hdr_headroom=0, the image should stay SDR (base image untouched).
        // A small loss is expected due to YUV/RGB conversion.
        std::make_tuple(
            /*source=*/"seine_sdr_gainmap_srgb.avif", /*hdr_headroom=*/0.0f,
            /*out_depth=*/8,
            /*out_transfer=*/AVIF_TRANSFER_CHARACTERISTICS_SRGB,
            /*out_rgb_format=*/AVIF_RGB_FORMAT_RGB,
            /*reference=*/"seine_sdr_gainmap_srgb.avif", /*min_psnr=*/60.0f,
            /*max_psnr=*/80.0f),

        // Same as above, outputting to RGBA.
        std::make_tuple(
            /*source=*/"seine_sdr_gainmap_srgb.avif", /*hdr_headroom=*/0.0f,
            /*out_depth=*/8,
            /*out_transfer=*/AVIF_TRANSFER_CHARACTERISTICS_SRGB,
            /*out_rgb_format=*/AVIF_RGB_FORMAT_RGBA,
            /*reference=*/"seine_sdr_gainmap_srgb.avif", /*min_psnr=*/60.0f,
            /*max_psnr=*/80.0f),

        // Same as above, outputting to a different transfer characteristic.
        // As a result we expect a low PSNR (since the PSNR function is not
        // aware of the transfer curve difference).
        std::make_tuple(
            /*source=*/"seine_sdr_gainmap_srgb.avif", /*hdr_headroom=*/0.0f,
            /*out_depth=*/8,
            /*out_transfer=*/AVIF_TRANSFER_CHARACTERISTICS_LOG100,
            /*out_rgb_format=*/AVIF_RGB_FORMAT_RGBA,
            /*reference=*/"seine_sdr_gainmap_srgb.avif", /*min_psnr=*/20.0f,
            /*max_psnr=*/30.0f),

        // hdr_headroom=3, the gain map should be fully applied.
        std::make_tuple(
            /*source=*/"seine_sdr_gainmap_srgb.avif", /*hdr_headroom=*/3.0f,
            /*out_depth=*/10,
            /*out_transfer=*/AVIF_TRANSFER_CHARACTERISTICS_PQ,
            /*out_rgb_format=*/AVIF_RGB_FORMAT_RGB,
            /*reference=*/"seine_hdr_srgb.avif", /*min_psnr=*/40.0f,
            /*max_psnr=*/60.0f),

        // hdr_headroom=3, the gain map should be fully applied.
        // Version with a gain map that is larger than the base image (needs
        // rescaling).
        std::make_tuple(
            /*source=*/"seine_sdr_gainmap_big_srgb.avif", /*hdr_headroom=*/3.0f,
            /*out_depth=*/10,
            /*out_transfer=*/AVIF_TRANSFER_CHARACTERISTICS_PQ,
            /*out_rgb_format=*/AVIF_RGB_FORMAT_RGB,
            /*reference=*/"seine_hdr_srgb.avif", /*min_psnr=*/40.0f,
            /*max_psnr=*/60.0f),

        // hdr_headroom=0.5 No reference image.
        std::make_tuple(
            /*source=*/"seine_sdr_gainmap_srgb.avif", /*hdr_headroom=*/1.5f,
            /*out_depth=*/10,
            /*out_transfer=*/AVIF_TRANSFER_CHARACTERISTICS_PQ,
            /*out_rgb_format=*/AVIF_RGB_FORMAT_RGB,
            /*reference=*/"", /*min_psnr=*/0.0f,
            /*max_psnr=*/0.0f),

        // ------ HDR BASE IMAGE ------

        // hdr_headroom=0, the gain map should be fully applied.
        std::make_tuple(
            /*source=*/"seine_hdr_gainmap_srgb.avif", /*hdr_headroom=*/0.0f,
            /*out_depth=*/8,
            /*out_transfer=*/AVIF_TRANSFER_CHARACTERISTICS_SRGB,
            /*out_rgb_format=*/AVIF_RGB_FORMAT_RGB,
            /*reference=*/"seine_sdr_gainmap_srgb.avif", /*min_psnr=*/38.0f,
            /*max_psnr=*/60.0f),

        // hdr_headroom=0, the gain map should be fully applied.
        // Version with a gain map that is smaller than the base image (needs
        // rescaling). The PSNR is a bit lower than above due to quality loss on
        // the gain map.
        std::make_tuple(
            /*source=*/"seine_hdr_gainmap_small_srgb.avif",
            /*hdr_headroom=*/0.0f,
            /*out_depth=*/8,
            /*out_transfer=*/AVIF_TRANSFER_CHARACTERISTICS_SRGB,
            AVIF_RGB_FORMAT_RGB,
            /*reference=*/"seine_sdr_gainmap_srgb.avif", /*min_psnr=*/36.0f,
            /*max_psnr=*/60.0f),

        // hdr_headroom=3, the image should stay HDR (base image untouched).
        // A small loss is expected due to YUV/RGB conversion.
        std::make_tuple(
            /*source=*/"seine_hdr_gainmap_srgb.avif", /*hdr_headroom=*/3.0f,
            /*out_depth=*/10,
            /*out_transfer=*/AVIF_TRANSFER_CHARACTERISTICS_PQ,
            /*out_rgb_format=*/AVIF_RGB_FORMAT_RGB,
            /*reference=*/"seine_hdr_gainmap_srgb.avif", /*min_psnr=*/60.0f,
            /*max_psnr=*/80.0f),

        // hdr_headroom=0.5 No reference image.
        std::make_tuple(
            /*source=*/"seine_hdr_gainmap_srgb.avif", /*hdr_headroom=*/1.5f,
            /*out_depth=*/10,
            /*out_transfer=*/AVIF_TRANSFER_CHARACTERISTICS_PQ,
            /*out_rgb_format=*/AVIF_RGB_FORMAT_RGB,
            /*reference=*/"", /*min_psnr=*/0.0f, /*max_psnr=*/0.0f)));

TEST(ToneMapTest, ToneMapImageSameHeadroom) {
  const std::string path =
      std::string(data_path) + "seine_sdr_gainmap_srgb.avif";
  ImagePtr image(avifImageCreateEmpty());
  ASSERT_NE(image, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  decoder->enableDecodingGainMap = true;
  decoder->enableParsingGainMapMetadata = true;
  avifResult result =
      avifDecoderReadFile(decoder.get(), image.get(), path.c_str());
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;

  ASSERT_NE(image->gainMap, nullptr);
  ASSERT_NE(image->gainMap->image, nullptr);

  // Force the alternate and base HDR headroom to the same value.
  image->gainMap->metadata.baseHdrHeadroomN =
      image->gainMap->metadata.alternateHdrHeadroomN;
  image->gainMap->metadata.baseHdrHeadroomD =
      image->gainMap->metadata.alternateHdrHeadroomD;
  const float headroom = static_cast<float>(
      static_cast<float>(image->gainMap->metadata.baseHdrHeadroomN) /
      image->gainMap->metadata.baseHdrHeadroomD);

  // Check that when the two headrooms are the same, the gain map is not applied
  // whatever the target headroom is.
  for (const float tonemap_to : {headroom, headroom - 0.5f, headroom + 0.5f}) {
    ToneMapImageAndCompareToReference(
        image.get(), *image->gainMap, /*hdr_headroom=*/tonemap_to,
        /*out_depth=*/image->depth,
        /*out_transfer_characteristics=*/image->transferCharacteristics,
        AVIF_RGB_FORMAT_RGB, /*reference_image=*/image.get(),
        /*min_psnr=*/60, /*max_psnr=*/100);
  }
}

class CreateGainMapTest
    : public testing::TestWithParam<std::tuple<
          /*image1_name=*/std::string, /*image2_name=*/std::string,
          /*downscaling=*/int, /*gain_map_depth=*/int,
          /*gain_map_format=*/avifPixelFormat,
          /*min_psnr=*/float, /*max_psnr=*/float>> {};

// Creates a gain map to go from image1 to image2, and tone maps to check we get
// the correct result. Then does the same thing going from image2 to image1.
TEST_P(CreateGainMapTest, Create) {
  const std::string image1_name = std::get<0>(GetParam());
  const std::string image2_name = std::get<1>(GetParam());
  const int downscaling = std::get<2>(GetParam());
  const int gain_map_depth = std::get<3>(GetParam());
  const avifPixelFormat gain_map_format = std::get<4>(GetParam());
  const float min_psnr = std::get<5>(GetParam());
  const float max_psnr = std::get<6>(GetParam());

  ImagePtr image1 = testutil::DecodeFile(std::string(data_path) + image1_name);
  ASSERT_NE(image1, nullptr);
  ImagePtr image2 = testutil::DecodeFile(std::string(data_path) + image2_name);
  ASSERT_NE(image2, nullptr);

  const uint32_t gain_map_width = std::max<uint32_t>(
      (uint32_t)std::round((float)image1->width / downscaling), 1u);
  const uint32_t gain_map_height = std::max<uint32_t>(
      (uint32_t)std::round((float)image1->height / downscaling), 1u);
  std::unique_ptr<avifGainMap, decltype(&avifGainMapDestroy)> gain_map(
      avifGainMapCreate(), avifGainMapDestroy);
  gain_map->image = avifImageCreate(gain_map_width, gain_map_height,
                                    gain_map_depth, gain_map_format);

  avifDiagnostics diag;
  gain_map->metadata.useBaseColorSpace = true;
  avifResult result = avifImageComputeGainMap(image1.get(), image2.get(),
                                              gain_map.get(), &diag);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << diag.error;

  EXPECT_EQ(gain_map->image->width, gain_map_width);
  EXPECT_EQ(gain_map->image->height, gain_map_height);

  const float image1_headroom = (float)gain_map->metadata.baseHdrHeadroomN /
                                gain_map->metadata.baseHdrHeadroomD;
  const float image2_headroom =
      (float)gain_map->metadata.alternateHdrHeadroomN /
      gain_map->metadata.alternateHdrHeadroomD;

  // Tone map from image1 to image2 by applying the gainmap forward.
  float psnr_image1_to_image2_forward;
  ToneMapImageAndCompareToReference(
      image1.get(), *gain_map, image2_headroom, image2->depth,
      image2->transferCharacteristics, AVIF_RGB_FORMAT_RGB, image2.get(),
      min_psnr, max_psnr, &psnr_image1_to_image2_forward);

  // Tone map from image2 to image1 by applying the gainmap backward.
  SwapBaseAndAlternate(*image1, *gain_map);
  float psnr_image2_to_image1_backward;
  ToneMapImageAndCompareToReference(
      image2.get(), *gain_map, image1_headroom, image1->depth,
      image1->transferCharacteristics, AVIF_RGB_FORMAT_RGB, image1.get(),
      min_psnr, max_psnr, &psnr_image2_to_image1_backward);

  // Uncomment the following to save the gain map as a PNG file.
  // ASSERT_TRUE(testutil::WriteImage(gain_map->image,
  //                                  "/tmp/gain_map_image1_to_image2.png"));

  // Compute the gain map in the other direction (from image2 to image1).
  gain_map->metadata.useBaseColorSpace = false;
  result = avifImageComputeGainMap(image2.get(), image1.get(), gain_map.get(),
                                   &diag);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << diag.error;

  const float image2_headroom2 = (float)gain_map->metadata.baseHdrHeadroomN /
                                 gain_map->metadata.baseHdrHeadroomD;
  EXPECT_NEAR(image2_headroom2, image2_headroom, 0.001);

  // Tone map from image2 to image1 by applying the new gainmap forward.
  float psnr_image2_to_image1_forward;
  ToneMapImageAndCompareToReference(
      image2.get(), *gain_map, image1_headroom, image1->depth,
      image1->transferCharacteristics, AVIF_RGB_FORMAT_RGB, image1.get(),
      min_psnr, max_psnr, &psnr_image2_to_image1_forward);

  // Tone map from image1 to image2 by applying the new gainmap backward.
  SwapBaseAndAlternate(*image2, *gain_map);
  float psnr_image1_to_image2_backward;
  ToneMapImageAndCompareToReference(
      image1.get(), *gain_map, image2_headroom, image2->depth,
      image2->transferCharacteristics, AVIF_RGB_FORMAT_RGB, image2.get(),
      min_psnr, max_psnr, &psnr_image1_to_image2_backward);

  // Uncomment the following to save the gain map as a PNG file.
  // ASSERT_TRUE(testutil::WriteImage(gain_map->image,
  //                                  "/tmp/gain_map_image2_to_image1.png"));

  // Results should be about the same whether the gain map was computed from sdr
  // to hdr or the other way around.
  EXPECT_NEAR(psnr_image1_to_image2_backward, psnr_image1_to_image2_forward,
              min_psnr * 0.1f);
  EXPECT_NEAR(psnr_image2_to_image1_forward, psnr_image2_to_image1_backward,
              min_psnr * 0.1f);
}

INSTANTIATE_TEST_SUITE_P(
    All, CreateGainMapTest,
    Values(
        // Full scale gain map, 3 channels, 10 bit gain map.
        std::make_tuple(/*image1_name=*/"seine_sdr_gainmap_srgb.avif",
                        /*image2_name=*/"seine_hdr_gainmap_srgb.avif",
                        /*downscaling=*/1, /*gain_map_depth=*/10,
                        /*gain_map_format=*/AVIF_PIXEL_FORMAT_YUV444,
                        /*min_psnr=*/55.0f, /*max_psnr=*/80.0f),
        // 8 bit gain map, expect a slightly lower PSNR.
        std::make_tuple(/*image1_name=*/"seine_sdr_gainmap_srgb.avif",
                        /*image2_name=*/"seine_hdr_gainmap_srgb.avif",
                        /*downscaling=*/1, /*gain_map_depth=*/8,
                        /*gain_map_format=*/AVIF_PIXEL_FORMAT_YUV444,
                        /*min_psnr=*/50.0f, /*max_psnr=*/70.0f),
        // 420 gain map, expect a lower PSNR.
        std::make_tuple(/*image1_name=*/"seine_sdr_gainmap_srgb.avif",
                        /*image2_name=*/"seine_hdr_gainmap_srgb.avif",
                        /*downscaling=*/1, /*gain_map_depth=*/8,
                        /*gain_map_format=*/AVIF_PIXEL_FORMAT_YUV420,
                        /*min_psnr=*/40.0f, /*max_psnr=*/60.0f),
        // Downscaled gain map, expect a lower PSNR.
        std::make_tuple(/*image1_name=*/"seine_sdr_gainmap_srgb.avif",
                        /*image2_name=*/"seine_hdr_gainmap_srgb.avif",
                        /*downscaling=*/2, /*gain_map_depth=*/8,
                        /*gain_map_format=*/AVIF_PIXEL_FORMAT_YUV444,
                        /*min_psnr=*/35.0f, /*max_psnr=*/45.0f),
        // Even more downscaled gain map, expect a lower PSNR.
        std::make_tuple(/*image1_name=*/"seine_sdr_gainmap_srgb.avif",
                        /*image2_name=*/"seine_hdr_gainmap_srgb.avif",
                        /*downscaling=*/3, /*gain_map_depth=*/8,
                        /*gain_map_format=*/AVIF_PIXEL_FORMAT_YUV444,
                        /*min_psnr=*/35.0f, /*max_psnr=*/45.0f),
        // Extreme downscaling, just for fun.
        std::make_tuple(/*image1_name=*/"seine_sdr_gainmap_srgb.avif",
                        /*image2_name=*/"seine_hdr_gainmap_srgb.avif",
                        /*downscaling=*/255, /*gain_map_depth=*/8,
                        /*gain_map_format=*/AVIF_PIXEL_FORMAT_YUV444,
                        /*min_psnr=*/20.0f, /*max_psnr=*/35.0f),
        // Grayscale gain map.
        std::make_tuple(/*image1_name=*/"seine_sdr_gainmap_srgb.avif",
                        /*image2_name=*/"seine_hdr_gainmap_srgb.avif",
                        /*downscaling=*/1, /*gain_map_depth=*/8,
                        /*gain_map_format=*/AVIF_PIXEL_FORMAT_YUV400,
                        /*min_psnr=*/40.0f, /*max_psnr=*/60.0f),
        // Downscaled AND grayscale.
        std::make_tuple(/*image1_name=*/"seine_sdr_gainmap_srgb.avif",
                        /*image2_name=*/"seine_hdr_gainmap_srgb.avif",
                        /*downscaling=*/2, /*gain_map_depth=*/8,
                        /*gain_map_format=*/AVIF_PIXEL_FORMAT_YUV400,
                        /*min_psnr=*/35.0f, /*max_psnr=*/45.0f),

        // Color space conversions.
        std::make_tuple(/*image1_name=*/"colors_sdr_srgb.avif",
                        /*image2_name=*/"colors_hdr_rec2020.avif",
                        /*downscaling=*/1, /*gain_map_depth=*/10,
                        /*gain_map_format=*/AVIF_PIXEL_FORMAT_YUV444,
                        /*min_psnr=*/55.0f, /*max_psnr=*/100.0f),
        // The PSNR is very high because there are essentially the same image,
        // simply expresed in different colorspaces.
        std::make_tuple(/*image1_name=*/"colors_hdr_rec2020.avif",
                        /*image2_name=*/"colors_hdr_p3.avif",
                        /*downscaling=*/1, /*gain_map_depth=*/8,
                        /*gain_map_format=*/AVIF_PIXEL_FORMAT_YUV444,
                        /*min_psnr=*/90.0f, /*max_psnr=*/100.0f),
        // Color space conversions with wider color gamut.
        std::make_tuple(/*image1_name=*/"colors_sdr_srgb.avif",
                        /*image2_name=*/"colors_wcg_hdr_rec2020.avif",
                        /*downscaling=*/1, /*gain_map_depth=*/10,
                        /*gain_map_format=*/AVIF_PIXEL_FORMAT_YUV444,
                        /*min_psnr=*/55.0f, /*max_psnr=*/80.0f)));

TEST(FindMinMaxWithoutOutliers, AllSame) {
  constexpr int kNumValues = 10000;

  for (float v : {0.0f, 42.f, -12.f, 1.52f}) {
    std::vector<float> values(kNumValues, v);

    float min, max;
    ASSERT_EQ(
        avifFindMinMaxWithoutOutliers(values.data(), kNumValues, &min, &max),
        AVIF_RESULT_OK);
    EXPECT_EQ(min, v);
    EXPECT_EQ(max, v);
  }
}

TEST(FindMinMaxWithoutOutliers, Test) {
  constexpr int kNumValues = 10000;

  for (const float value_shift : {0.0f, -20.0f, 20.0f}) {
    SCOPED_TRACE("value_shift: " + std::to_string(value_shift));
    std::vector<float> values(kNumValues, value_shift + 2.0f);
    int k = 0;
    for (int i = 0; i < 5; ++i, ++k) {
      values[k] = value_shift + 1.99f;
    }
    for (int i = 0; i < 5; ++i, ++k) {
      values[k] = value_shift + 1.98f;
    }
    for (int i = 0; i < 1; ++i, ++k) {
      values[k] = value_shift + 1.97f;
    }
    for (int i = 0; i < 2; ++i, ++k) {
      values[k] = value_shift + 1.93f;  // Outliers.
    }
    for (int i = 0; i < 3; ++i, ++k) {
      values[k] = value_shift + 10.2f;  // Outliers.
    }

    float min, max;
    ASSERT_EQ(
        avifFindMinMaxWithoutOutliers(values.data(), kNumValues, &min, &max),
        AVIF_RESULT_OK);
    const float kEpsilon = 0.001f;
    EXPECT_NEAR(min, value_shift + 1.97f, kEpsilon);
    const float bucketSize = 0.01f;  // Size of one bucket.
    EXPECT_NEAR(max, value_shift + 2.0f + bucketSize, kEpsilon);
  }
}

}  // namespace
}  // namespace avif

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc != 2) {
    std::cerr << "There must be exactly one argument containing the path to "
                 "the test data folder"
              << std::endl;
    return 1;
  }
  avif::data_path = argv[1];
  return RUN_ALL_TESTS();
}
