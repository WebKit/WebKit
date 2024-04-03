// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

using ::testing::Combine;
using ::testing::Values;

namespace avif {
namespace {

//------------------------------------------------------------------------------

class AvifCondensedImageBoxTest
    : public testing::TestWithParam<std::tuple<
          /*width=*/int, /*height=*/int, /*depth=*/int, avifPixelFormat,
          avifPlanesFlags, avifRange, /*create_icc=*/bool, /*create_exif=*/bool,
          /*create_xmp=*/bool, /*create_clli=*/bool, avifTransformFlags>> {};

TEST_P(AvifCondensedImageBoxTest, SimpleOpaque) {
  const int width = std::get<0>(GetParam());
  const int height = std::get<1>(GetParam());
  const int depth = std::get<2>(GetParam());
  const avifPixelFormat format = std::get<3>(GetParam());
  const avifPlanesFlags planes = std::get<4>(GetParam());
  const avifRange range = std::get<5>(GetParam());
  const bool create_icc = std::get<6>(GetParam());
  const bool create_exif = std::get<7>(GetParam());
  const bool create_xmp = std::get<8>(GetParam());
  const bool create_clli = std::get<9>(GetParam());
  const avifTransformFlags create_transform_flags = std::get<10>(GetParam());

  ImagePtr image =
      testutil::CreateImage(width, height, depth, format, planes, range);
  ASSERT_NE(image, nullptr);
  testutil::FillImageGradient(image.get());  // The pixels do not matter.
  if (create_icc) {
    ASSERT_EQ(avifImageSetProfileICC(image.get(), testutil::kSampleIcc.data(),
                                     testutil::kSampleIcc.size()),
              AVIF_RESULT_OK);
  }
  if (create_exif) {
    size_t exif_tiff_header_offset;  // Must be 0 for 'avir' brand.
    ASSERT_EQ(avifGetExifTiffHeaderOffset(testutil::kSampleExif.data(),
                                          testutil::kSampleExif.size(),
                                          &exif_tiff_header_offset),
              AVIF_RESULT_OK);
    ASSERT_EQ(
        avifImageSetMetadataExif(
            image.get(), testutil::kSampleExif.data() + exif_tiff_header_offset,
            testutil::kSampleExif.size() - exif_tiff_header_offset),
        AVIF_RESULT_OK);
  }
  if (create_xmp) {
    ASSERT_EQ(avifImageSetMetadataXMP(image.get(), testutil::kSampleXmp.data(),
                                      testutil::kSampleXmp.size()),
              AVIF_RESULT_OK);
  }
  if (create_clli) {
    image->clli.maxCLL = 1;
    image->clli.maxPALL = 2;
  }
  image->transformFlags = create_transform_flags;
  if (create_transform_flags & AVIF_TRANSFORM_PASP) {
    image->pasp.hSpacing = 1;
    image->pasp.vSpacing = 2;
  }
  if (create_transform_flags & AVIF_TRANSFORM_CLAP) {
    const avifCropRect rect{image->width / 4, image->height / 4,
                            std::min(1u, image->width / 2),
                            std::min(1u, image->height / 2)};
    avifDiagnostics diag;
    ASSERT_TRUE(avifCleanApertureBoxConvertCropRect(&image->clap, &rect,
                                                    image->width, image->height,
                                                    image->yuvFormat, &diag));
  }
  if (create_transform_flags & AVIF_TRANSFORM_IROT) {
    image->irot.angle = 2;
  }
  if (create_transform_flags & AVIF_TRANSFORM_IMIR) {
    image->imir.axis = 1;
  }

  // Encode.
  testutil::AvifRwData encoded_mini;
  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  encoder->speed = AVIF_SPEED_FASTEST;
  encoder->headerFormat = AVIF_HEADER_REDUCED;
  ASSERT_EQ(avifEncoderWrite(encoder.get(), image.get(), &encoded_mini),
            AVIF_RESULT_OK);

  // Decode.
  const ImagePtr decoded_mini =
      testutil::Decode(encoded_mini.data, encoded_mini.size);
  ASSERT_NE(decoded_mini, nullptr);

  // Compare.
  testutil::AvifRwData encoded_meta =
      testutil::Encode(image.get(), encoder->speed);
  ASSERT_NE(encoded_meta.data, nullptr);
  // At least 200 bytes should be saved.
  EXPECT_LT(encoded_mini.size, encoded_meta.size - 200);

  const ImagePtr decoded_meta =
      testutil::Decode(encoded_meta.data, encoded_meta.size);
  ASSERT_NE(decoded_meta, nullptr);

  // Only the container changed. The pixels, features and metadata should be
  // identical.
  EXPECT_TRUE(
      testutil::AreImagesEqual(*decoded_meta.get(), *decoded_mini.get()));
}

INSTANTIATE_TEST_SUITE_P(OnePixel, AvifCondensedImageBoxTest,
                         Combine(/*width=*/Values(1), /*height=*/Values(1),
                                 /*depth=*/Values(8),
                                 Values(AVIF_PIXEL_FORMAT_YUV444),
                                 Values(AVIF_PLANES_YUV, AVIF_PLANES_ALL),
                                 Values(AVIF_RANGE_LIMITED, AVIF_RANGE_FULL),
                                 /*create_icc=*/Values(false, true),
                                 /*create_exif=*/Values(false, true),
                                 /*create_xmp=*/Values(false, true),
                                 /*create_clli=*/Values(false),
                                 Values(AVIF_TRANSFORM_NONE)));

INSTANTIATE_TEST_SUITE_P(
    DepthsSubsamplings, AvifCondensedImageBoxTest,
    Combine(/*width=*/Values(12), /*height=*/Values(34),
            /*depth=*/Values(8, 10, 12),
            Values(AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV422,
                   AVIF_PIXEL_FORMAT_YUV420, AVIF_PIXEL_FORMAT_YUV400),
            Values(AVIF_PLANES_ALL), Values(AVIF_RANGE_FULL),
            /*create_icc=*/Values(false), /*create_exif=*/Values(false),
            /*create_xmp=*/Values(false), /*create_clli=*/Values(false),
            Values(AVIF_TRANSFORM_NONE)));

INSTANTIATE_TEST_SUITE_P(
    Dimensions, AvifCondensedImageBoxTest,
    Combine(/*width=*/Values(127), /*height=*/Values(200), /*depth=*/Values(8),
            Values(AVIF_PIXEL_FORMAT_YUV444), Values(AVIF_PLANES_ALL),
            Values(AVIF_RANGE_FULL), /*create_icc=*/Values(true),
            /*create_exif=*/Values(true), /*create_xmp=*/Values(true),
            /*create_clli=*/Values(false), Values(AVIF_TRANSFORM_NONE)));

// Use CLLI and transform properties that are not supported by a plain
// CondensedImageBox to force the use of an ExtendedMetaBox.
INSTANTIATE_TEST_SUITE_P(
    ExtendedMetaBox, AvifCondensedImageBoxTest,
    Combine(/*width=*/Values(16), /*height=*/Values(24), /*depth=*/Values(8),
            Values(AVIF_PIXEL_FORMAT_YUV444), Values(AVIF_PLANES_ALL),
            Values(AVIF_RANGE_FULL), /*create_icc=*/Values(true),
            /*create_exif=*/Values(true), /*create_xmp=*/Values(true),
            /*create_clli=*/Values(true),
            Values(AVIF_TRANSFORM_NONE,
                   AVIF_TRANSFORM_PASP | AVIF_TRANSFORM_CLAP |
                       AVIF_TRANSFORM_IROT | AVIF_TRANSFORM_IMIR)));

//------------------------------------------------------------------------------

}  // namespace
}  // namespace avif
