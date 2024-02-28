// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <tuple>

#include "avif/avif.h"
#include "avif/avif_cxx.h"
#include "avifjpeg.h"
#include "avifpng.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

using ::testing::Bool;
using ::testing::Combine;
using ::testing::Values;

namespace avif {
namespace {

//------------------------------------------------------------------------------

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

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

  ImagePtr image =
      testutil::CreateImage(/*width=*/12, /*height=*/34, /*depth=*/10,
                            AVIF_PIXEL_FORMAT_YUV444, AVIF_PLANES_ALL);
  ASSERT_NE(image, nullptr);
  testutil::FillImageGradient(image.get());  // The pixels do not matter.
  if (use_icc) {
    ASSERT_EQ(avifImageSetProfileICC(image.get(), testutil::kSampleIcc.data(),
                                     testutil::kSampleIcc.size()),
              AVIF_RESULT_OK);
  }
  if (use_exif) {
    const avifTransformFlags old_transform_flags = image->transformFlags;
    const uint8_t old_irot_angle = image->irot.angle;
    const uint8_t old_imir_axis = image->imir.axis;
    ASSERT_EQ(
        avifImageSetMetadataExif(image.get(), testutil::kSampleExif.data(),
                                 testutil::kSampleExif.size()),
        AVIF_RESULT_OK);
    // testutil::kSampleExif is not a valid Exif payload, just some part of it.
    // These fields should not be modified.
    EXPECT_EQ(image->transformFlags, old_transform_flags);
    EXPECT_EQ(image->irot.angle, old_irot_angle);
    EXPECT_EQ(image->imir.axis, old_imir_axis);
  }
  if (use_xmp) {
    ASSERT_EQ(avifImageSetMetadataXMP(image.get(), testutil::kSampleXmp.data(),
                                      testutil::kSampleXmp.size()),
              AVIF_RESULT_OK);
  }

  // Encode.
  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  encoder->speed = AVIF_SPEED_FASTEST;
  testutil::AvifRwData encoded_avif;
  ASSERT_EQ(avifEncoderWrite(encoder.get(), image.get(), &encoded_avif),
            AVIF_RESULT_OK);

  // Decode.
  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  ASSERT_EQ(avifDecoderReadMemory(decoder.get(), decoded.get(),
                                  encoded_avif.data, encoded_avif.size),
            AVIF_RESULT_OK);

  // Compare input and output metadata.
  EXPECT_TRUE(testutil::AreByteSequencesEqual(
      decoded->icc.data, decoded->icc.size, testutil::kSampleIcc.data(),
      use_icc ? testutil::kSampleIcc.size() : 0u));
  EXPECT_TRUE(testutil::AreByteSequencesEqual(
      decoded->exif.data, decoded->exif.size, testutil::kSampleExif.data(),
      use_exif ? testutil::kSampleExif.size() : 0u));
  EXPECT_TRUE(testutil::AreByteSequencesEqual(
      decoded->xmp.data, decoded->xmp.size, testutil::kSampleXmp.data(),
      use_xmp ? testutil::kSampleXmp.size() : 0u));
}

INSTANTIATE_TEST_SUITE_P(All, AvifMetadataTest,
                         Combine(/*use_icc=*/Bool(), /*use_exif=*/Bool(),
                                 /*use_xmp=*/Bool()));

//------------------------------------------------------------------------------
// Jpeg and PNG metadata tests

ImagePtr WriteAndReadImage(const avifImage& image,
                           const std::string& file_name) {
  const std::string file_path = testing::TempDir() + file_name;
  if (file_name.substr(file_name.size() - 4) == ".png") {
    if (!avifPNGWrite(file_path.c_str(), &image, /*requestedDepth=*/0,
                      AVIF_CHROMA_UPSAMPLING_AUTOMATIC,
                      /*compressionLevel=*/0)) {
      return nullptr;
    }
  } else {
    if (!avifJPEGWrite(file_path.c_str(), &image, /*jpegQuality=*/100,
                       AVIF_CHROMA_UPSAMPLING_AUTOMATIC)) {
      return nullptr;
    }
  }
  return testutil::ReadImage(testing::TempDir().c_str(), file_name.c_str());
}

class MetadataTest : public testing::TestWithParam<
                         std::tuple</*file_name=*/const char*, /*use_icc=*/bool,
                                    /*use_exif=*/bool, /*use_xmp=*/bool>> {};

TEST_P(MetadataTest, ReadWriteReadCompare) {
  const char* file_name = std::get<0>(GetParam());
  const bool use_icc = std::get<1>(GetParam());
  const bool use_exif = std::get<2>(GetParam());
  const bool use_xmp = std::get<3>(GetParam());

  const ImagePtr image = testutil::ReadImage(
      data_path, file_name, AVIF_PIXEL_FORMAT_NONE, 0,
      AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC, !use_icc, !use_exif, !use_xmp);
  ASSERT_NE(image, nullptr);
  EXPECT_NE(image->width * image->height, 0u);

  if (use_icc) {
    EXPECT_NE(image->icc.size, 0u);
    EXPECT_NE(image->icc.data, nullptr);
  } else {
    EXPECT_EQ(image->icc.size, 0u);
    EXPECT_EQ(image->icc.data, nullptr);
  }
  if (use_exif) {
    EXPECT_NE(image->exif.size, 0u);
    EXPECT_NE(image->exif.data, nullptr);
  } else {
    EXPECT_EQ(image->exif.size, 0u);
    EXPECT_EQ(image->exif.data, nullptr);
  }
  if (use_xmp) {
    EXPECT_NE(image->xmp.size, 0u);
    EXPECT_NE(image->xmp.data, nullptr);
  } else {
    EXPECT_EQ(image->xmp.size, 0u);
    EXPECT_EQ(image->xmp.data, nullptr);
  }

  // Writing and reading that same metadata should give the same bytes.
  for (const std::string extension : {".png", ".jpg"}) {
    const ImagePtr temp_image =
        WriteAndReadImage(*image, file_name + extension);
    ASSERT_NE(temp_image, nullptr);
    ASSERT_TRUE(testutil::AreByteSequencesEqual(image->icc, temp_image->icc));
    ASSERT_TRUE(testutil::AreByteSequencesEqual(image->exif, temp_image->exif));
    ASSERT_TRUE(testutil::AreByteSequencesEqual(image->xmp, temp_image->xmp));
  }
}

INSTANTIATE_TEST_SUITE_P(
    PngJpeg, MetadataTest,
    Combine(Values("paris_icc_exif_xmp.png",         // iCCP zTXt zTXt IDAT
                   "paris_icc_exif_xmp_at_end.png",  // iCCP IDAT eXIf tEXt
                   "paris_exif_xmp_icc.jpg"),  // APP1-Exif, APP1-XMP, APP2-ICC
            /*use_icc=*/Bool(), /*use_exif=*/Bool(), /*use_xmp=*/Bool()));

// Verify all parsers lead exactly to the same metadata bytes.
TEST(MetadataTest, Compare) {
  const ImagePtr ref = testutil::ReadImage(data_path, "paris_icc_exif_xmp.png");
  ASSERT_NE(ref, nullptr);
  EXPECT_GT(ref->exif.size, 0u);
  EXPECT_GT(ref->xmp.size, 0u);
  EXPECT_GT(ref->icc.size, 0u);

  for (const char* file_name :
       {"paris_exif_xmp_icc.jpg", "paris_icc_exif_xmp_at_end.png"}) {
    const ImagePtr image = testutil::ReadImage(data_path, file_name);
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
  ASSERT_NE(decoder, nullptr);
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
  const ImagePtr image =
      testutil::ReadImage(data_path, "paris_exif_xmp_icc.jpg");
  ASSERT_NE(image, nullptr);
  // The Exif metadata contains orientation information: 1.
  // It is converted to no irot/imir.
  EXPECT_GT(image->exif.size, 0u);
  EXPECT_EQ(image->transformFlags & (AVIF_TRANSFORM_IROT | AVIF_TRANSFORM_IMIR),
            avifTransformFlags{AVIF_TRANSFORM_NONE});

  const testutil::AvifRwData encoded =
      testutil::Encode(image.get(), AVIF_SPEED_FASTEST);
  const ImagePtr decoded = testutil::Decode(encoded.data, encoded.size);
  ASSERT_NE(decoded, nullptr);

  // No irot/imir after decoding because 1 maps to default no irot/imir.
  EXPECT_TRUE(testutil::AreByteSequencesEqual(image->exif, decoded->exif));
  EXPECT_EQ(
      decoded->transformFlags & (AVIF_TRANSFORM_IROT | AVIF_TRANSFORM_IMIR),
      avifTransformFlags{AVIF_TRANSFORM_NONE});
}

TEST(MetadataTest, ExifOrientation) {
  const ImagePtr image =
      testutil::ReadImage(data_path, "paris_exif_orientation_5.jpg");
  ASSERT_NE(image, nullptr);
  // The Exif metadata contains orientation information: 5.
  EXPECT_GT(image->exif.size, 0u);
  EXPECT_EQ(image->transformFlags & (AVIF_TRANSFORM_IROT | AVIF_TRANSFORM_IMIR),
            avifTransformFlags{AVIF_TRANSFORM_IROT | AVIF_TRANSFORM_IMIR});
  EXPECT_EQ(image->irot.angle, 1u);
  EXPECT_EQ(image->imir.axis, 0u);

  const testutil::AvifRwData encoded =
      testutil::Encode(image.get(), AVIF_SPEED_FASTEST);
  const ImagePtr decoded = testutil::Decode(encoded.data, encoded.size);
  ASSERT_NE(decoded, nullptr);

  // irot/imir are expected.
  EXPECT_TRUE(testutil::AreByteSequencesEqual(image->exif, decoded->exif));
  EXPECT_EQ(
      decoded->transformFlags & (AVIF_TRANSFORM_IROT | AVIF_TRANSFORM_IMIR),
      avifTransformFlags{AVIF_TRANSFORM_IROT | AVIF_TRANSFORM_IMIR});
  EXPECT_EQ(decoded->irot.angle, 1u);
  EXPECT_EQ(decoded->imir.axis, 0u);

  // Exif orientation is kept in JPEG export.
  ImagePtr temp_image =
      WriteAndReadImage(*image, "paris_exif_orientation_5.jpg");
  ASSERT_NE(temp_image, nullptr);
  EXPECT_TRUE(testutil::AreByteSequencesEqual(image->exif, temp_image->exif));
  EXPECT_EQ(image->transformFlags, temp_image->transformFlags);
  EXPECT_EQ(image->irot.angle, temp_image->irot.angle);
  EXPECT_EQ(image->imir.axis, temp_image->imir.axis);
  EXPECT_EQ(image->width, temp_image->width);  // Samples are left untouched.

  // Exif orientation in PNG export should be ignored or discarded.
  temp_image = WriteAndReadImage(*image, "paris_exif_orientation_5.png");
  ASSERT_NE(temp_image, nullptr);
  EXPECT_FALSE(testutil::AreByteSequencesEqual(image->exif, temp_image->exif));
  EXPECT_EQ(
      temp_image->transformFlags & (AVIF_TRANSFORM_IROT | AVIF_TRANSFORM_IMIR),
      avifTransformFlags{0});
  // TODO(yguyon): Fix orientation not being applied to PNG samples.
  EXPECT_EQ(image->width, temp_image->width /* should be height here */);
}

TEST(MetadataTest, ExifOrientationAndForcedImir) {
  const ImagePtr image =
      testutil::ReadImage(data_path, "paris_exif_orientation_5.jpg");
  ASSERT_NE(image, nullptr);
  // The Exif metadata contains orientation information: 5.
  // Force irot/imir to values that have a different meaning than 5.
  // This is not recommended but for testing only.
  EXPECT_GT(image->exif.size, 0u);
  image->transformFlags = AVIF_TRANSFORM_IMIR;
  image->imir.axis = 1;

  const testutil::AvifRwData encoded =
      testutil::Encode(image.get(), AVIF_SPEED_FASTEST);
  const ImagePtr decoded = testutil::Decode(encoded.data, encoded.size);
  ASSERT_NE(decoded, nullptr);

  // Exif orientation is still there but irot/imir do not match it.
  EXPECT_TRUE(testutil::AreByteSequencesEqual(image->exif, decoded->exif));
  EXPECT_EQ(decoded->transformFlags, avifTransformFlags{AVIF_TRANSFORM_IMIR});
  EXPECT_EQ(decoded->irot.angle, 0u);
  EXPECT_EQ(decoded->imir.axis, image->imir.axis);

  // Exif orientation is set equivalent to irot/imir in JPEG export.
  // Existing Exif orientation is overwritten.
  const ImagePtr temp_image =
      WriteAndReadImage(*image, "paris_exif_orientation_2.jpg");
  ASSERT_NE(temp_image, nullptr);
  EXPECT_FALSE(testutil::AreByteSequencesEqual(image->exif, temp_image->exif));
  EXPECT_EQ(image->transformFlags, temp_image->transformFlags);
  EXPECT_EQ(image->imir.axis, temp_image->imir.axis);
  EXPECT_EQ(image->width, temp_image->width);  // Samples are left untouched.
}

TEST(MetadataTest, RotatedJpegBecauseOfIrotImir) {
  const ImagePtr image =
      testutil::ReadImage(data_path, "paris_exif_orientation_5.jpg");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(avifImageSetMetadataExif(image.get(), nullptr, 0),
            AVIF_RESULT_OK);  // Clear Exif.
  // Orientation is kept in irot/imir.
  EXPECT_EQ(image->transformFlags & (AVIF_TRANSFORM_IROT | AVIF_TRANSFORM_IMIR),
            avifTransformFlags{AVIF_TRANSFORM_IROT | AVIF_TRANSFORM_IMIR});
  EXPECT_EQ(image->irot.angle, 1u);
  EXPECT_EQ(image->imir.axis, 0u);

  // No Exif metadata to store the orientation: the samples should be rotated.
  const ImagePtr temp_image =
      WriteAndReadImage(*image, "paris_exif_orientation_5.jpg");
  ASSERT_NE(temp_image, nullptr);
  EXPECT_EQ(temp_image->exif.size, 0u);
  EXPECT_EQ(
      temp_image->transformFlags & (AVIF_TRANSFORM_IROT | AVIF_TRANSFORM_IMIR),
      avifTransformFlags{0});
  // TODO(yguyon): Fix orientation not being applied to JPEG samples.
  EXPECT_EQ(image->width, temp_image->width /* should be height here */);
}

TEST(MetadataTest, ExifIfdOffsetLoopingTo8) {
  const ImagePtr image(avifImageCreateEmpty());
  ASSERT_NE(image, nullptr);
  const uint8_t kBadExifPayload[128] = {
      'M', 'M', 0, 42,                          // TIFF header
      0,   0,   0, 8,                           // Offset to 0th IFD
      0,   1,                                   // fieldCount
      0,   0,   0, 0,  0, 0, 0, 0, 0, 0, 0, 0,  // tag, type, count, valueOffset
      0,   0,   0, 8  // Invalid IFD offset, infinitely looping back to 0th IFD.
  };
  // avifImageSetMetadataExif() calls
  // avifImageExtractExifOrientationToIrotImir() internally.
  // The avifImageExtractExifOrientationToIrotImir() call should not enter an
  // infinite loop.
  ASSERT_EQ(avifImageSetMetadataExif(
                image.get(), kBadExifPayload,
                sizeof(kBadExifPayload) / sizeof(kBadExifPayload[0])),
            AVIF_RESULT_OK);
}

//------------------------------------------------------------------------------

TEST(MetadataTest, ExtendedXMP) {
  const ImagePtr image =
      testutil::ReadImage(data_path, "dog_exif_extended_xmp_icc.jpg");
  ASSERT_NE(image, nullptr);
  ASSERT_NE(image->xmp.size, 0u);
  ASSERT_LT(image->xmp.size,
            size_t{65503});  // Fits in a single JPEG APP1 marker.

  for (const char* temp_file_name : {"dog.png", "dog.jpg"}) {
    const ImagePtr temp_image = WriteAndReadImage(*image, temp_file_name);
    ASSERT_NE(temp_image, nullptr);
    EXPECT_TRUE(testutil::AreByteSequencesEqual(image->xmp, temp_image->xmp));
  }
}

TEST(MetadataTest, MultipleExtendedXMPAndAlternativeGUIDTag) {
  const ImagePtr image =
      testutil::ReadImage(data_path, "paris_extended_xmp.jpg");
  ASSERT_NE(image, nullptr);
  ASSERT_GT(image->xmp.size, size_t{65536 * 2});

  ImagePtr temp_image = WriteAndReadImage(*image, "paris_extended_xmp.png");
  ASSERT_NE(temp_image, nullptr);
  EXPECT_TRUE(testutil::AreByteSequencesEqual(image->xmp, temp_image->xmp));

  // Writing more than 65502 bytes of XMP in a JPEG is not supported.
  temp_image = WriteAndReadImage(*image, "paris_extended_xmp.jpg");
  ASSERT_NE(temp_image, nullptr);
  ASSERT_EQ(temp_image->xmp.size, 0u);  // XMP was dropped.
}

//------------------------------------------------------------------------------

// Regression test for https://github.com/AOMediaCodec/libavif/issues/1333.
// Coverage for avifImageFixXMP().
TEST(MetadataTest, XMPWithTrailingNullCharacter) {
  ImagePtr jpg = testutil::ReadImage(data_path, "paris_xmp_trailing_null.jpg");
  ASSERT_NE(jpg, nullptr);
  ASSERT_NE(jpg->xmp.size, 0u);
  // avifJPEGRead() should strip the trailing null character.
  ASSERT_EQ(std::memchr(jpg->xmp.data, '\0', jpg->xmp.size), nullptr);

  // Append a zero byte to see what happens when encoded with libpng.
  ASSERT_EQ(avifRWDataRealloc(&jpg->xmp, jpg->xmp.size + 1), AVIF_RESULT_OK);
  jpg->xmp.data[jpg->xmp.size - 1] = '\0';
  testutil::WriteImage(jpg.get(),
                       (testing::TempDir() + "xmp_trailing_null.png").c_str());
  const ImagePtr png =
      testutil::ReadImage(testing::TempDir().c_str(), "xmp_trailing_null.png");
  ASSERT_NE(png, nullptr);
  ASSERT_NE(png->xmp.size, 0u);
  // avifPNGRead() should strip the trailing null character, but the libpng
  // export during testutil::WriteImage() probably took care of that anyway.
  ASSERT_EQ(std::memchr(png->xmp.data, '\0', png->xmp.size), nullptr);
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
