// Copyright 2022 Google LLC. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include <array>
#include <tuple>

#include "avif/internal.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

using ::testing::Bool;
using ::testing::Combine;
using ::testing::Values;

namespace libavif {
namespace {

//------------------------------------------------------------------------------

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

// ICC color profiles are not checked by libavif so the content does not matter.
// This is a truncated widespread ICC color profile.
const std::array<uint8_t, 24> kSampleIcc = {
    0x00, 0x00, 0x02, 0x0c, 0x6c, 0x63, 0x6d, 0x73, 0x02, 0x10, 0x00, 0x00,
    0x6d, 0x6e, 0x74, 0x72, 0x52, 0x47, 0x42, 0x20, 0x58, 0x59, 0x5a, 0x20};

// Exif bytes are partially checked by libavif. This is a truncated widespread
// Exif metadata chunk.
const std::array<uint8_t, 24> kSampleExif = {
    0xff, 0x1,  0x45, 0x78, 0x69, 0x76, 0x32, 0xff, 0xe1, 0x12, 0x5a, 0x45,
    0x78, 0x69, 0x66, 0x0,  0x0,  0x49, 0x49, 0x2a, 0x0,  0x8,  0x0,  0x0};

// XMP bytes are not checked by libavif so the content does not matter.
// This is a truncated widespread XMP metadata chunk.
const std::array<uint8_t, 24> kSampleXmp = {
    0x3c, 0x3f, 0x78, 0x70, 0x61, 0x63, 0x6b, 0x65, 0x74, 0x20, 0x62, 0x65,
    0x67, 0x69, 0x6e, 0x3d, 0x22, 0xef, 0xbb, 0xbf, 0x22, 0x20, 0x69, 0x64};

//------------------------------------------------------------------------------
// AVIF encode/decode metadata tests

class AvifMetadataTest
    : public testing::TestWithParam<
          std::tuple</*use_icc=*/bool, /*use_exif=*/bool, /*use_xmp=*/bool>> {};

// Encodes, decodes then verifies that the output metadata matches the input
// metadata defined by the parameters.
TEST_P(AvifMetadataTest, EncodeDecode) {
  const bool use_icc = std::get<0>(GetParam());
  const bool use_exif = std::get<1>(GetParam());
  const bool use_xmp = std::get<2>(GetParam());

  testutil::AvifImagePtr image =
      testutil::CreateImage(/*width=*/12, /*height=*/34, /*depth=*/10,
                            AVIF_PIXEL_FORMAT_YUV444, AVIF_PLANES_ALL);
  ASSERT_NE(image, nullptr);
  testutil::FillImageGradient(image.get());  // The pixels do not matter.
  if (use_icc) {
    avifImageSetProfileICC(image.get(), kSampleIcc.data(), kSampleIcc.size());
  }
  if (use_exif) {
    const avifTransformFlags old_transform_flags = image->transformFlags;
    const uint8_t old_irot_angle = image->irot.angle;
    const uint8_t old_imir_mode = image->imir.mode;
    avifImageSetMetadataExif(image.get(), kSampleExif.data(),
                             kSampleExif.size());
    // kSampleExif is not a valid Exif payload, just some part of it. These
    // fields should not be modified.
    EXPECT_EQ(image->transformFlags, old_transform_flags);
    EXPECT_EQ(image->irot.angle, old_irot_angle);
    EXPECT_EQ(image->imir.mode, old_imir_mode);
  }
  if (use_xmp) {
    avifImageSetMetadataXMP(image.get(), kSampleXmp.data(), kSampleXmp.size());
  }

  // Encode.
  testutil::AvifEncoderPtr encoder(avifEncoderCreate(), avifEncoderDestroy);
  ASSERT_NE(encoder, nullptr);
  encoder->speed = AVIF_SPEED_FASTEST;
  testutil::AvifRwData encoded_avif;
  ASSERT_EQ(avifEncoderWrite(encoder.get(), image.get(), &encoded_avif),
            AVIF_RESULT_OK);

  // Decode.
  testutil::AvifImagePtr decoded(avifImageCreateEmpty(), avifImageDestroy);
  ASSERT_NE(decoded, nullptr);
  testutil::AvifDecoderPtr decoder(avifDecoderCreate(), avifDecoderDestroy);
  ASSERT_NE(decoder, nullptr);
  ASSERT_EQ(avifDecoderReadMemory(decoder.get(), decoded.get(),
                                  encoded_avif.data, encoded_avif.size),
            AVIF_RESULT_OK);

  // Compare input and output metadata.
  EXPECT_TRUE(testutil::AreByteSequencesEqual(
      decoded->icc.data, decoded->icc.size, kSampleIcc.data(),
      use_icc ? kSampleIcc.size() : 0u));
  EXPECT_TRUE(testutil::AreByteSequencesEqual(
      decoded->exif.data, decoded->exif.size, kSampleExif.data(),
      use_exif ? kSampleExif.size() : 0u));
  EXPECT_TRUE(testutil::AreByteSequencesEqual(
      decoded->xmp.data, decoded->xmp.size, kSampleXmp.data(),
      use_xmp ? kSampleXmp.size() : 0u));
}

INSTANTIATE_TEST_SUITE_P(All, AvifMetadataTest,
                         Combine(/*use_icc=*/Bool(), /*use_exif=*/Bool(),
                                 /*use_xmp=*/Bool()));

//------------------------------------------------------------------------------
// Jpeg and PNG metadata tests

class MetadataTest
    : public testing::TestWithParam<
          std::tuple</*file_name=*/const char*, /*use_icc=*/bool,
                     /*use_exif=*/bool, /*use_xmp=*/bool, /*expect_icc=*/bool,
                     /*expect_exif=*/bool, /*expect_xmp=*/bool>> {};

// zTXt "Raw profile type exif" at the beginning of a PNG file.
TEST_P(MetadataTest, Read) {
  const char* file_name = std::get<0>(GetParam());
  const bool use_icc = std::get<1>(GetParam());
  const bool use_exif = std::get<2>(GetParam());
  const bool use_xmp = std::get<3>(GetParam());
  const bool expect_icc = std::get<4>(GetParam());
  const bool expect_exif = std::get<5>(GetParam());
  const bool expect_xmp = std::get<6>(GetParam());

  const testutil::AvifImagePtr image = testutil::ReadImage(
      data_path, file_name, AVIF_PIXEL_FORMAT_NONE, 0,
      AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC, !use_icc, !use_exif, !use_xmp);
  ASSERT_NE(image, nullptr);
  EXPECT_NE(image->width * image->height, 0u);

  if (expect_icc) {
    EXPECT_NE(image->icc.size, 0u);
    EXPECT_NE(image->icc.data, nullptr);
  } else {
    EXPECT_EQ(image->icc.size, 0u);
    EXPECT_EQ(image->icc.data, nullptr);
  }
  if (expect_exif) {
    EXPECT_NE(image->exif.size, 0u);
    EXPECT_NE(image->exif.data, nullptr);
  } else {
    EXPECT_EQ(image->exif.size, 0u);
    EXPECT_EQ(image->exif.data, nullptr);
  }
  if (expect_xmp) {
    EXPECT_NE(image->xmp.size, 0u);
    EXPECT_NE(image->xmp.data, nullptr);
  } else {
    EXPECT_EQ(image->xmp.size, 0u);
    EXPECT_EQ(image->xmp.data, nullptr);
  }
}

INSTANTIATE_TEST_SUITE_P(
    PngNone, MetadataTest,
    Combine(Values("paris_icc_exif_xmp.png"),  // iCCP zTXt zTXt IDAT
            /*use_icc=*/Values(false), /*use_exif=*/Values(false),
            /*use_xmp=*/Values(false), /*expected_icc=*/Values(false),
            /*expected_exif=*/Values(false), /*expected_xmp=*/Values(false)));
INSTANTIATE_TEST_SUITE_P(
    PngAll, MetadataTest,
    Combine(Values("paris_icc_exif_xmp.png"),  // iCCP zTXt zTXt IDAT
            /*use_icc=*/Values(true), /*use_exif=*/Values(true),
            /*use_xmp=*/Values(true), /*expected_icc=*/Values(true),
            /*expected_exif=*/Values(true), /*expected_xmp=*/Values(true)));

INSTANTIATE_TEST_SUITE_P(
    PngExifAtEnd, MetadataTest,
    Combine(Values("paris_icc_exif_xmp_at_end.png"),  // iCCP IDAT eXIf tEXt
            /*use_icc=*/Values(true), /*use_exif=*/Values(true),
            /*use_xmp=*/Values(true), /*expected_icc=*/Values(true),
            /*expected_exif=*/Values(true), /*expected_xmp=*/Values(true)));

INSTANTIATE_TEST_SUITE_P(
    Jpeg, MetadataTest,
    Combine(Values("paris_exif_xmp_icc.jpg"),  // APP1-Exif, APP1-XMP, APP2-ICC
            /*use_icc=*/Values(true), /*use_exif=*/Values(true),
            /*use_xmp=*/Values(true), /*expected_icc=*/Values(true),
            /*expected_exif=*/Values(true), /*expected_xmp=*/Values(true)));

// Verify all parsers lead exactly to the same metadata bytes.
TEST(MetadataTest, Compare) {
  const testutil::AvifImagePtr ref =
      testutil::ReadImage(data_path, "paris_icc_exif_xmp.png");
  ASSERT_NE(ref, nullptr);
  EXPECT_GT(ref->exif.size, 0u);
  EXPECT_GT(ref->xmp.size, 0u);
  EXPECT_GT(ref->icc.size, 0u);

  for (const char* file_name :
       {"paris_exif_xmp_icc.jpg", "paris_icc_exif_xmp_at_end.png"}) {
    const testutil::AvifImagePtr image =
        testutil::ReadImage(data_path, file_name);
    ASSERT_NE(image, nullptr);
    EXPECT_TRUE(testutil::AreByteSequencesEqual(image->exif, ref->exif));
    EXPECT_TRUE(testutil::AreByteSequencesEqual(image->xmp, ref->xmp));
    EXPECT_TRUE(testutil::AreByteSequencesEqual(image->icc, ref->icc));
  }
}

// A test for https://github.com/AOMediaCodec/libavif/issues/1086 to prevent
// regression.
TEST(MetadataTest, DecoderParseICC) {
  std::string file_path = std::string(data_path) + "paris_icc_exif_xmp.avif";
  avifDecoder* decoder = avifDecoderCreate();
  EXPECT_EQ(avifDecoderSetIOFile(decoder, file_path.c_str()), AVIF_RESULT_OK);
  EXPECT_EQ(avifDecoderParse(decoder), AVIF_RESULT_OK);
  // Check the first four bytes of the ICC profile.
  ASSERT_GE(decoder->image->icc.size, 4u);
  EXPECT_EQ(decoder->image->icc.data[0], 0);
  EXPECT_EQ(decoder->image->icc.data[1], 0);
  EXPECT_EQ(decoder->image->icc.data[2], 2);
  EXPECT_EQ(decoder->image->icc.data[3], 84);
  avifDecoderDestroy(decoder);
}

//------------------------------------------------------------------------------

TEST(MetadataTest, ExifButDefaultIrotImir) {
  const testutil::AvifImagePtr image =
      testutil::ReadImage(data_path, "paris_exif_xmp_icc.jpg");
  ASSERT_NE(image, nullptr);
  // The Exif metadata contains orientation information: 1.
  // It is converted to no irot/imir.
  EXPECT_GT(image->exif.size, 0u);
  EXPECT_EQ(image->transformFlags & (AVIF_TRANSFORM_IROT | AVIF_TRANSFORM_IMIR),
            avifTransformFlags{AVIF_TRANSFORM_NONE});

  const testutil::AvifRwData encoded =
      testutil::Encode(image.get(), AVIF_SPEED_FASTEST);
  const testutil::AvifImagePtr decoded =
      testutil::Decode(encoded.data, encoded.size);
  ASSERT_NE(decoded, nullptr);

  // No irot/imir after decoding because 1 maps to default no irot/imir.
  EXPECT_TRUE(testutil::AreByteSequencesEqual(image->exif, decoded->exif));
  EXPECT_EQ(
      decoded->transformFlags & (AVIF_TRANSFORM_IROT | AVIF_TRANSFORM_IMIR),
      avifTransformFlags{AVIF_TRANSFORM_NONE});
}

TEST(MetadataTest, ExifOrientation) {
  const testutil::AvifImagePtr image =
      testutil::ReadImage(data_path, "paris_exif_orientation_5.jpg");
  ASSERT_NE(image, nullptr);
  // The Exif metadata contains orientation information: 5.
  EXPECT_GT(image->exif.size, 0u);
  EXPECT_EQ(image->transformFlags & (AVIF_TRANSFORM_IROT | AVIF_TRANSFORM_IMIR),
            avifTransformFlags{AVIF_TRANSFORM_IROT | AVIF_TRANSFORM_IMIR});
  EXPECT_EQ(image->irot.angle, 1u);
  EXPECT_EQ(image->imir.mode, 0u);

  const testutil::AvifRwData encoded =
      testutil::Encode(image.get(), AVIF_SPEED_FASTEST);
  const testutil::AvifImagePtr decoded =
      testutil::Decode(encoded.data, encoded.size);
  ASSERT_NE(decoded, nullptr);

  // irot/imir are expected.
  EXPECT_TRUE(testutil::AreByteSequencesEqual(image->exif, decoded->exif));
  EXPECT_EQ(
      decoded->transformFlags & (AVIF_TRANSFORM_IROT | AVIF_TRANSFORM_IMIR),
      avifTransformFlags{AVIF_TRANSFORM_IROT | AVIF_TRANSFORM_IMIR});
  EXPECT_EQ(decoded->irot.angle, 1u);
  EXPECT_EQ(decoded->imir.mode, 0u);
}

TEST(MetadataTest, ExifOrientationAndForcedImir) {
  const testutil::AvifImagePtr image =
      testutil::ReadImage(data_path, "paris_exif_orientation_5.jpg");
  ASSERT_NE(image, nullptr);
  // The Exif metadata contains orientation information: 5.
  // Force irot/imir to values that have a different meaning than 5.
  // This is not recommended but for testing only.
  EXPECT_GT(image->exif.size, 0u);
  image->transformFlags = AVIF_TRANSFORM_IMIR;
  image->imir.mode = 1;

  const testutil::AvifRwData encoded =
      testutil::Encode(image.get(), AVIF_SPEED_FASTEST);
  const testutil::AvifImagePtr decoded =
      testutil::Decode(encoded.data, encoded.size);
  ASSERT_NE(decoded, nullptr);

  // Exif orientation is still there but irot/imir do not match it.
  EXPECT_TRUE(testutil::AreByteSequencesEqual(image->exif, decoded->exif));
  EXPECT_EQ(decoded->transformFlags, avifTransformFlags{AVIF_TRANSFORM_IMIR});
  EXPECT_EQ(decoded->irot.angle, 0u);
  EXPECT_EQ(decoded->imir.mode, image->imir.mode);
}

TEST(MetadataTest, ExifIfdOffsetLoopingTo8) {
  const testutil::AvifImagePtr image(avifImageCreateEmpty(), avifImageDestroy);
  ASSERT_NE(image, nullptr);
  const uint8_t kBadExifPayload[128] = {
      'M', 'M', 0, 42,                          // TIFF header
      0,   0,   0, 8,                           // Offset to 0th IFD
      0,   1,                                   // fieldCount
      0,   0,   0, 0,  0, 0, 0, 0, 0, 0, 0, 0,  // tag, type, count, valueOffset
      0,   0,   0, 8  // Invalid IFD offset, infinitely looping back to 0th IFD.
  };
  avifRWDataSet(&image->exif, kBadExifPayload,
                sizeof(kBadExifPayload) / sizeof(kBadExifPayload[0]));
  // avifImageExtractExifOrientationToIrotImir() does not verify  the whole
  // payload, only the parts necessary to extract Exif orientation.
  ASSERT_EQ(avifImageExtractExifOrientationToIrotImir(image.get()),
            AVIF_RESULT_OK);
}

//------------------------------------------------------------------------------

TEST(MetadataTest, ExtendedXMP) {
  const testutil::AvifImagePtr image =
      testutil::ReadImage(data_path, "dog_exif_extended_xmp_icc.jpg");
  ASSERT_NE(image, nullptr);
  ASSERT_NE(image->xmp.size, 0u);
}

TEST(MetadataTest, MultipleExtendedXMPAndAlternativeGUIDTag) {
  const testutil::AvifImagePtr image =
      testutil::ReadImage(data_path, "paris_extended_xmp.jpg");
  ASSERT_NE(image, nullptr);
  ASSERT_GT(image->xmp.size, size_t{65536 * 2});
}

//------------------------------------------------------------------------------

}  // namespace
}  // namespace libavif

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc != 2) {
    std::cerr << "There must be exactly one argument containing the path to "
                 "the test data folder"
              << std::endl;
    return 1;
  }
  libavif::data_path = argv[1];
  return RUN_ALL_TESTS();
}
