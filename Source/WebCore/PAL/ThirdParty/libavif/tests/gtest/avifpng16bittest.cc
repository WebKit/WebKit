// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <cstdint>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "avif/avif.h"
#include "avif/avif_cxx.h"
#include "avifpng.h"
#include "aviftest_helpers.h"
#include "avifutil.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

//------------------------------------------------------------------------------

// Reads samples with an expected bit depth.
ImagePtr ReadImageLosslessBitDepth(const std::string& path,
                                   uint32_t bit_depth) {
  ImagePtr image(avifImageCreateEmpty());
  if (!image) return nullptr;
  // Lossless.
  image->yuvRange = AVIF_RANGE_FULL;
  image->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_IDENTITY;

  uint32_t output_depth;
  if (avifReadImage(path.c_str(), AVIF_PIXEL_FORMAT_YUV444, bit_depth,
                    AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC,
                    /*ignoreColorProfile=*/true,
                    /*ignoreExif=*/true, /*ignoreXMP=*/true,
                    /*allowChangingCicp=*/true, /*ignoreGainMap=*/true,
                    AVIF_DEFAULT_IMAGE_SIZE_LIMIT, image.get(), &output_depth,
                    /*sourceTiming=*/nullptr,
                    /*frameIter=*/nullptr) == AVIF_APP_FILE_FORMAT_UNKNOWN) {
    return nullptr;
  }
  EXPECT_EQ(output_depth, bit_depth);
  EXPECT_EQ(image->depth, bit_depth);
  return image;
}

// Returns the ratio of sample values present in the channel over all possible
// values for the sample bit depth.
double GetSampleValueSpread(const avifImage& image, avifChannelIndex channel) {
  std::vector<uint32_t> is_value_used(size_t{1} << image.depth, 0);
  const uint8_t* row = image.yuvPlanes[channel];
  for (uint32_t y = 0; y < avifImagePlaneHeight(&image, channel); ++y) {
    for (uint32_t x = 0; x < avifImagePlaneWidth(&image, channel); ++x) {
      const uint32_t value = avifImageUsesU16(&image)
                                 ? reinterpret_cast<const uint16_t*>(row)[x]
                                 : row[x];
      is_value_used[value] = 1;
    }
    row += avifImagePlaneRowBytes(&image, channel);
  }
  const double num_used_values =
      std::accumulate(is_value_used.begin(), is_value_used.end(), 0u);
  return num_used_values / is_value_used.size();
}

//------------------------------------------------------------------------------

TEST(BitDepthTest, Png16b) {
  // Read 16-bit image.
  ImagePtr original =
      ReadImageLosslessBitDepth(std::string(data_path) + "weld_16bit.png", 16);
  ASSERT_NE(original, nullptr);
  // More than 75% of possible sample values are present in each channel.
  // This is a "true" 16-bit image, not 8-bit samples shifted left by 8 bits.
  EXPECT_GT(GetSampleValueSpread(*original, AVIF_CHAN_Y), 0.75);
  EXPECT_GT(GetSampleValueSpread(*original, AVIF_CHAN_U), 0.75);
  EXPECT_GT(GetSampleValueSpread(*original, AVIF_CHAN_V), 0.75);

  // Write and read it as a 16-bit PNG.
  const std::string file_path =
      testing::TempDir() + "avifpng16bittest_weld_16bit.png";
  ASSERT_TRUE(avifPNGWrite(file_path.c_str(), original.get(), original->depth,
                           AVIF_CHROMA_UPSAMPLING_AUTOMATIC,
                           /*compressionLevel=*/0));
  ImagePtr image = ReadImageLosslessBitDepth(file_path, original->depth);
  ASSERT_NE(image, nullptr);

  // They should match exactly.
  EXPECT_TRUE(testutil::AreImagesEqual(*original, *image));
}

//------------------------------------------------------------------------------

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
