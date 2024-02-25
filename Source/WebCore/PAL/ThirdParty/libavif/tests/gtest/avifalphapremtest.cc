// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

//------------------------------------------------------------------------------

TEST(AlphaMultiplyTest, OpaqueIsNoOp) {
  for (bool premultiplied_input : {false, true}) {
    if (!premultiplied_input) {
      // TODO(yguyon): Fix the issue and remove this condition.
      //               The issue is that avifPrepareReformatState() will result
      //               in different YUV(A)-to-RGB conversion algorithm choices
      //               (built-in or libyuv) depending on the presence of an
      //               alpha channel, no matter if it is opaque or not, because
      //               it considers that unpremultiplied YUVA samples should be
      //               premultiplied before discarding alpha and converting to
      //               RGB.
      continue;
    }

    // YUVA.
    ImagePtr opaque_alpha = testutil::CreateImage(
        1024, 1024, /*depth=*/8, AVIF_PIXEL_FORMAT_YUV444, AVIF_PLANES_ALL);
    const uint32_t yuva[] = {255, 255, 255, 255};
    testutil::FillImagePlain(opaque_alpha.get(), yuva);
    opaque_alpha->alphaPremultiplied = premultiplied_input;

    // View on YUV (no alpha), to make sure the color values are identical.
    ImagePtr no_alpha(avifImageCreateEmpty());
    ASSERT_NE(no_alpha, nullptr);
    const avifCropRect rect = {0, 0, opaque_alpha->width, opaque_alpha->height};
    ASSERT_EQ(avifImageSetViewRect(no_alpha.get(), opaque_alpha.get(), &rect),
              AVIF_RESULT_OK);
    avifImageFreePlanes(no_alpha.get(), AVIF_PLANES_A);

    // Decorrelate YUV and Alpha.
    testutil::FillImageGradient(no_alpha.get());

    // Convert to RGB and discard any Alpha.
    testutil::AvifRgbImage opaque_rgb(opaque_alpha.get(), opaque_alpha->depth,
                                      AVIF_RGB_FORMAT_RGB);
    ASSERT_EQ(avifImageYUVToRGB(opaque_alpha.get(), &opaque_rgb),
              AVIF_RESULT_OK);
    testutil::AvifRgbImage no_alpha_rgb(no_alpha.get(), no_alpha->depth,
                                        AVIF_RGB_FORMAT_RGB);
    ASSERT_EQ(avifImageYUVToRGB(no_alpha.get(), &no_alpha_rgb), AVIF_RESULT_OK);

    // YUV samples with opaque alpha and with missing alpha should be converted
    // to the same RGB values.
    EXPECT_TRUE(testutil::AreByteSequencesEqual(
        opaque_rgb.pixels, opaque_rgb.height * opaque_rgb.rowBytes,
        no_alpha_rgb.pixels, no_alpha_rgb.height * no_alpha_rgb.rowBytes));

    // TODO(yguyon): Also compare avifImageYUVToRGB() with ignoreAlpha.
  }
}

//------------------------------------------------------------------------------

}  // namespace
}  // namespace avif
