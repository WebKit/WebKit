// Copyright 2022 Google LLC. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include <fstream>
#include <iostream>
#include <string>
#include <tuple>

#include "avif/avif.h"
#include "avifincrtest_helpers.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

using testing::Bool;
using testing::Combine;
using testing::Values;

namespace libavif {
namespace {

//------------------------------------------------------------------------------

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

// Reads the file with file_name into bytes and returns them.
testutil::AvifRwData ReadFile(const char* file_name) {
  std::ifstream file(std::string(data_path) + "/" + file_name,
                     std::ios::binary | std::ios::ate);
  testutil::AvifRwData bytes;
  avifRWDataRealloc(&bytes,
                    file.good() ? static_cast<size_t>(file.tellg()) : 0);
  file.seekg(0, std::ios::beg);
  file.read(reinterpret_cast<char*>(bytes.data),
            static_cast<std::streamsize>(bytes.size));
  return bytes;
}

//------------------------------------------------------------------------------

// Check that non-incremental and incremental decodings of a grid AVIF produce
// the same pixels.
TEST(IncrementalTest, Decode) {
  const testutil::AvifRwData encoded_avif = ReadFile("sofa_grid1x5_420.avif");
  ASSERT_NE(encoded_avif.size, 0u);
  testutil::AvifImagePtr reference(avifImageCreateEmpty(), avifImageDestroy);
  ASSERT_NE(reference, nullptr);
  testutil::AvifDecoderPtr decoder(avifDecoderCreate(), avifDecoderDestroy);
  ASSERT_NE(decoder, nullptr);
  ASSERT_EQ(avifDecoderReadMemory(decoder.get(), reference.get(),
                                  encoded_avif.data, encoded_avif.size),
            AVIF_RESULT_OK);

  // Cell height is hardcoded because there is no API to extract it from an
  // encoded payload.
  testutil::DecodeIncrementally(encoded_avif, /*is_persistent=*/true,
                                /*give_size_hint=*/true,
                                /*use_nth_image_api=*/false, *reference,
                                /*cell_height=*/154);
}

//------------------------------------------------------------------------------

class IncrementalTest
    : public testing::TestWithParam<std::tuple<
          /*width=*/uint32_t, /*height=*/uint32_t, /*create_alpha=*/bool,
          /*flat_cells=*/bool, /*encoded_avif_is_persistent=*/bool,
          /*give_size_hint=*/bool, /*use_nth_image_api=*/bool>> {};

// Encodes then decodes a window of width*height pixels at the middle of the
// image. Check that non-incremental and incremental decodings produce the same
// pixels.
TEST_P(IncrementalTest, EncodeDecode) {
  const uint32_t width = std::get<0>(GetParam());
  const uint32_t height = std::get<1>(GetParam());
  const bool create_alpha = std::get<2>(GetParam());
  const bool flat_cells = std::get<3>(GetParam());
  const bool encoded_avif_is_persistent = std::get<4>(GetParam());
  const bool give_size_hint = std::get<5>(GetParam());
  const bool use_nth_image_api = std::get<6>(GetParam());

  // Load an image. It does not matter that it comes from an AVIF file.
  testutil::AvifImagePtr image(avifImageCreateEmpty(), avifImageDestroy);
  ASSERT_NE(image, nullptr);
  const testutil::AvifRwData image_bytes = ReadFile("sofa_grid1x5_420.avif");
  ASSERT_NE(image_bytes.size, 0u);
  testutil::AvifDecoderPtr decoder(avifDecoderCreate(), avifDecoderDestroy);
  ASSERT_NE(decoder, nullptr);
  ASSERT_EQ(avifDecoderReadMemory(decoder.get(), image.get(), image_bytes.data,
                                  image_bytes.size),
            AVIF_RESULT_OK);

  // Encode then decode it.
  testutil::AvifRwData encoded_avif;
  uint32_t cell_width, cell_height;
  testutil::EncodeRectAsIncremental(*image, width, height, create_alpha,
                                    flat_cells, &encoded_avif, &cell_width,
                                    &cell_height);
  testutil::DecodeNonIncrementallyAndIncrementally(
      encoded_avif, encoded_avif_is_persistent, give_size_hint,
      use_nth_image_api, cell_height);
}

INSTANTIATE_TEST_SUITE_P(WholeImage, IncrementalTest,
                         Combine(/*width=*/Values(1024),
                                 /*height=*/Values(770),
                                 /*create_alpha=*/Values(true),
                                 /*flat_cells=*/Bool(),
                                 /*encoded_avif_is_persistent=*/Values(true),
                                 /*give_size_hint=*/Values(true),
                                 /*use_nth_image_api=*/Values(false)));

// avifEncoderAddImageInternal() only accepts grids of one unique cell, or grids
// where width and height are both at least 64.
INSTANTIATE_TEST_SUITE_P(SingleCell, IncrementalTest,
                         Combine(/*width=*/Values(1),
                                 /*height=*/Values(1),
                                 /*create_alpha=*/Bool(),
                                 /*flat_cells=*/Bool(),
                                 /*encoded_avif_is_persistent=*/Bool(),
                                 /*give_size_hint=*/Bool(),
                                 /*use_nth_image_api=*/Bool()));

// Chroma subsampling requires even dimensions. See ISO 23000-22
// section 7.3.11.4.2.
INSTANTIATE_TEST_SUITE_P(SinglePixel, IncrementalTest,
                         Combine(/*width=*/Values(64, 66),
                                 /*height=*/Values(64, 66),
                                 /*create_alpha=*/Bool(),
                                 /*flat_cells=*/Bool(),
                                 /*encoded_avif_is_persistent=*/Bool(),
                                 /*give_size_hint=*/Bool(),
                                 /*use_nth_image_api=*/Bool()));

//------------------------------------------------------------------------------

}  // namespace
}  // namespace libavif

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc < 2) {
    std::cerr
        << "The path to the test data folder must be provided as an argument"
        << std::endl;
    return 1;
  }
  libavif::data_path = argv[1];
  return RUN_ALL_TESTS();
}
