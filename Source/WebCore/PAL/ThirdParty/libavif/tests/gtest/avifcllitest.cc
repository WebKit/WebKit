// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

TEST(ClliTest, Simple) {
  ImagePtr image =
      testutil::CreateImage(/*width=*/8, /*height=*/8, /*depth=*/8,
                            AVIF_PIXEL_FORMAT_YUV444, AVIF_PLANES_YUV);
  ASSERT_NE(image, nullptr);
  testutil::FillImageGradient(image.get());  // The pixels do not matter.

  for (int max_content_light_level : {0, 1, 65535}) {
    for (int max_pic_average_light_level : {0, 1, 65535}) {
      image->clli.maxCLL = static_cast<uint16_t>(max_content_light_level);
      image->clli.maxPALL = static_cast<uint16_t>(max_pic_average_light_level);

      const testutil::AvifRwData encoded = testutil::Encode(image.get());
      const ImagePtr decoded = testutil::Decode(encoded.data, encoded.size);
      ASSERT_NE(decoded, nullptr);
      ASSERT_EQ(decoded->clli.maxCLL, image->clli.maxCLL);
      ASSERT_EQ(decoded->clli.maxPALL, image->clli.maxPALL);
    }
  }
}

}  // namespace
}  // namespace avif
