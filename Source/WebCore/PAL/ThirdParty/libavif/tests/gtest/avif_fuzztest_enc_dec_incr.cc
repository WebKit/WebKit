// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause
// Encode a fuzzed image split into a grid and decode it incrementally.
// Compare the output with a non-incremental decode.

#include <cassert>
#include <cstdint>
#include <vector>

#include "avif/internal.h"
#include "avif_fuzztest_helpers.h"
#include "avifincrtest_helpers.h"
#include "aviftest_helpers.h"
#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

using ::fuzztest::Arbitrary;
using ::fuzztest::InRange;

namespace avif {
namespace testutil {
namespace {

::testing::Environment* const kStackLimitEnv = SetStackLimitTo512x1024Bytes();

// Encodes an image into an AVIF grid then decodes it.
void EncodeDecodeGridValid(ImagePtr image, EncoderPtr encoder,
                           DecoderPtr decoder, uint32_t grid_cols,
                           uint32_t grid_rows, bool is_encoded_data_persistent,
                           bool give_size_hint_to_decoder) {
  ASSERT_NE(image, nullptr);
  ASSERT_NE(encoder, nullptr);

  const std::vector<ImagePtr> cells =
      ImageToGrid(image.get(), grid_cols, grid_rows);
  if (cells.empty()) return;
  const uint32_t cell_width = cells.front()->width;
  const uint32_t cell_height = cells.front()->height;
  const uint32_t encoded_width = std::min(image->width, grid_cols * cell_width);
  const uint32_t encoded_height =
      std::min(image->height, grid_rows * cell_height);

  AvifRwData encoded_data;
  const avifResult encoder_result = avifEncoderAddImageGrid(
      encoder.get(), grid_cols, grid_rows, UniquePtrToRawPtr(cells).data(),
      AVIF_ADD_IMAGE_FLAG_SINGLE);
  if (((grid_cols > 1 || grid_rows > 1) &&
       !avifAreGridDimensionsValid(image->yuvFormat, encoded_width,
                                   encoded_height, cell_width, cell_height,
                                   nullptr))) {
    ASSERT_TRUE(encoder_result == AVIF_RESULT_INVALID_IMAGE_GRID)
        << avifResultToString(encoder_result);
    return;
  }
  ASSERT_EQ(encoder_result, AVIF_RESULT_OK)
      << avifResultToString(encoder_result);

  const avifResult finish_result =
      avifEncoderFinish(encoder.get(), &encoded_data);
  ASSERT_EQ(finish_result, AVIF_RESULT_OK) << avifResultToString(finish_result);

  const avifResult decode_result = DecodeNonIncrementallyAndIncrementally(
      encoded_data, decoder.get(), is_encoded_data_persistent,
      give_size_hint_to_decoder, /*use_nth_image_api=*/true, cell_height);
  ASSERT_EQ(decode_result, AVIF_RESULT_OK) << avifResultToString(decode_result);
}

FUZZ_TEST(EncodeDecodeAvifFuzzTest, EncodeDecodeGridValid)
    .WithDomains(ArbitraryAvifImage(), ArbitraryAvifEncoder(),
                 ArbitraryAvifDecoder(),
                 /*grid_cols=*/InRange<uint32_t>(1, 32),
                 /*grid_rows=*/InRange<uint32_t>(1, 32),
                 /*is_encoded_data_persistent=*/Arbitrary<bool>(),
                 /*give_size_hint_to_decoder=*/Arbitrary<bool>());

}  // namespace
}  // namespace testutil
}  // namespace avif
