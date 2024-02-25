// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause
// Compares two files and returns whether they are the same once decoded.

#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

#include "aviftest_helpers.h"
#include "avifutil.h"

using avif::ImagePtr;

int main(int argc, char** argv) {
  if (argc != 4 && argc != 5) {
    std::cerr << "Wrong argument: " << argv[0]
              << " file1 file2 ignore_alpha_flag [psnr_threshold]" << std::endl;
    return 2;
  }
  ImagePtr decoded[2] = {ImagePtr(avifImageCreateEmpty()),
                         ImagePtr(avifImageCreateEmpty())};
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
                      /*ignoreColorProfile==*/AVIF_FALSE,
                      /*ignoreExif=*/AVIF_FALSE,
                      /*ignoreXMP=*/AVIF_FALSE, /*allowChangingCicp=*/AVIF_TRUE,
                      // TODO(maryla): also compare gain maps.
                      /*ignoreGainMap=*/AVIF_TRUE,
                      /*imageSizeLimit=*/std::numeric_limits<uint32_t>::max(),
                      decoded[i].get(), &depth[i], nullptr,
                      nullptr) == AVIF_APP_FILE_FORMAT_UNKNOWN) {
      std::cerr << "Image " << argv[i + 1] << " cannot be read." << std::endl;
      return 2;
    }
  }

  if (depth[0] != depth[1]) {
    std::cerr << "Images " << argv[1] << " and " << argv[2]
              << " have different depths." << std::endl;
    return 1;
  }

  bool ignore_alpha = std::stoi(argv[3]) != 0;

  if (argc == 4) {
    if (!avif::testutil::AreImagesEqual(*decoded[0], *decoded[1],
                                        ignore_alpha)) {
      std::cerr << "Images " << argv[1] << " and " << argv[2]
                << " are different." << std::endl;
      return 1;
    }
    std::cout << "Images " << argv[1] << " and " << argv[2] << " are identical."
              << std::endl;
  } else {
    auto psnr = avif::testutil::GetPsnr(*decoded[0], *decoded[1], ignore_alpha);
    if (psnr < std::stod(argv[4])) {
      std::cerr << "PSNR: " << psnr << ", images " << argv[1] << " and "
                << argv[2] << " are not similar." << std::endl;
      return 1;
    }
    std::cout << "PSNR: " << psnr << ", images " << argv[1] << " and "
              << argv[2] << " are similar." << std::endl;
  }

  return 0;
}
