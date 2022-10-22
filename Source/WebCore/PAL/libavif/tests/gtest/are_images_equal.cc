// Copyright 2022 Google LLC. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause
// Compares two files and returns whether they are the same once decoded.

#include <iostream>
#include <string>

#include "aviftest_helpers.h"
#include "avifutil.h"

using libavif::testutil::AvifImagePtr;

int main(int argc, char** argv) {
  if (argc != 4) {
    std::cerr << "Wrong argument: " << argv[0]
              << " file1 file2 ignore_alpha_flag" << std::endl;
    return 2;
  }
  AvifImagePtr decoded[2] = {
      AvifImagePtr(avifImageCreateEmpty(), avifImageDestroy),
      AvifImagePtr(avifImageCreateEmpty(), avifImageDestroy)};
  if (!decoded[0] || !decoded[1]) {
    std::cerr << "Cannot create AVIF images." << std::endl;
    return 2;
  }
  uint32_t depth[2];
  // Request the bit depth closest to the bit depth of the input file.
  constexpr int kRequestedDepth = 0;
  constexpr avifPixelFormat requestedFormat = AVIF_PIXEL_FORMAT_NONE;
  for (int i : {0, 1}) {
    // Make sure no color conversion happens.
    decoded[i]->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_IDENTITY;
    if (avifReadImage(argv[i + 1], requestedFormat, kRequestedDepth,
                      AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC,
                      /*ignoreICC=*/AVIF_FALSE, /*ignoreExif=*/AVIF_FALSE,
                      /*ignoreXMP=*/AVIF_FALSE, decoded[i].get(), &depth[i],
                      nullptr, nullptr) == AVIF_APP_FILE_FORMAT_UNKNOWN) {
      std::cerr << "Image " << argv[i + 1] << " cannot be read." << std::endl;
      return 2;
    }
  }

  if (depth[0] != depth[1]) {
    std::cerr << "Images " << argv[1] << " and " << argv[2]
              << " have different depths." << std::endl;
    return 1;
  }
  if (!libavif::testutil::AreImagesEqual(*decoded[0], *decoded[1],
                                         std::stoi(argv[3]))) {
    std::cerr << "Images " << argv[1] << " and " << argv[2] << " are different."
              << std::endl;
    return 1;
  }
  std::cout << "Images " << argv[1] << " and " << argv[2] << " are identical."
            << std::endl;
  return 0;
}
