// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <array>
#include <iostream>
#include <ostream>

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

//------------------------------------------------------------------------------

bool GetIoStatsFromEncode(avifIOStats& encoder_io_stats, int quality,
                          int qualityAlpha, bool translucent,
                          int num_frames = 1, bool encode_as_grid = false) {
  ImagePtr image = testutil::ReadImage(data_path, "paris_exif_xmp_icc.jpg");
  AVIF_CHECK(image != nullptr);
  if (translucent) {
    // Make up some alpha from luma.
    image->alphaPlane = image->yuvPlanes[AVIF_CHAN_Y];
    image->alphaRowBytes = image->yuvRowBytes[AVIF_CHAN_Y];
    AVIF_CHECK(!image->imageOwnsAlphaPlane);
  }

  EncoderPtr encoder(avifEncoderCreate());
  AVIF_CHECK(encoder != nullptr);
  encoder->speed = AVIF_SPEED_FASTEST;
  encoder->quality = quality;
  encoder->qualityAlpha = qualityAlpha;
  for (int frame = 0; frame < num_frames; ++frame) {
    if (encode_as_grid) {
      const avifCropRect left_rect = {0, 0, image->width / 2, image->height};
      ImagePtr left_cell(avifImageCreateEmpty());
      AVIF_CHECK(avifImageSetViewRect(left_cell.get(), image.get(),
                                      &left_rect) == AVIF_RESULT_OK);
      const avifCropRect right_rect = {image->width / 2, 0, image->width / 2,
                                       image->height};
      ImagePtr right_cell(avifImageCreateEmpty());
      AVIF_CHECK(avifImageSetViewRect(right_cell.get(), image.get(),
                                      &right_rect) == AVIF_RESULT_OK);
      const std::array<avifImage*, 2> pointers = {left_cell.get(),
                                                  right_cell.get()};
      AVIF_CHECK(avifEncoderAddImageGrid(encoder.get(), /*gridCols=*/2,
                                         /*gridRows=*/1, pointers.data(),
                                         AVIF_ADD_IMAGE_FLAG_NONE) ==
                 AVIF_RESULT_OK);
    } else {
      AVIF_CHECK(avifEncoderAddImage(encoder.get(), image.get(),
                                     /*durationInTimescales=*/1,
                                     AVIF_ADD_IMAGE_FLAG_NONE) ==
                 AVIF_RESULT_OK);
    }

    // Watermark each frame.
    image->yuvPlanes[AVIF_CHAN_Y][0] =
        (image->yuvPlanes[AVIF_CHAN_Y][0] + 1u) % 256u;
  }

  testutil::AvifRwData encoded;
  AVIF_CHECK(avifEncoderFinish(encoder.get(), &encoded) == AVIF_RESULT_OK);
  encoder_io_stats = encoder->ioStats;

  // Make sure decoding gives the same stats.
  DecoderPtr decoder(avifDecoderCreate());
  AVIF_CHECK(decoder != nullptr);
  AVIF_CHECK(avifDecoderSetIOMemory(decoder.get(), encoded.data,
                                    encoded.size) == AVIF_RESULT_OK);
  AVIF_CHECK(avifDecoderParse(decoder.get()) == AVIF_RESULT_OK);
  EXPECT_EQ(decoder->ioStats.colorOBUSize, encoder_io_stats.colorOBUSize);
  EXPECT_EQ(decoder->ioStats.alphaOBUSize, encoder_io_stats.alphaOBUSize);
  return true;
}

//------------------------------------------------------------------------------

TEST(IoStatsTest, ColorObuSize) {
  avifIOStats io_stats;
  ASSERT_TRUE(GetIoStatsFromEncode(io_stats, AVIF_QUALITY_DEFAULT,
                                   AVIF_QUALITY_DEFAULT,
                                   /*translucent=*/false));
  EXPECT_GT(io_stats.colorOBUSize, 0u);
  EXPECT_EQ(io_stats.alphaOBUSize, 0u);
}

TEST(IoStatsTest, AlphaObuSize) {
  avifIOStats io_stats;
  ASSERT_TRUE(GetIoStatsFromEncode(io_stats, AVIF_QUALITY_WORST,
                                   AVIF_QUALITY_WORST, /*translucent=*/true));
  EXPECT_GT(io_stats.colorOBUSize, 0u);
  EXPECT_GT(io_stats.alphaOBUSize, 0u);
}

// Disabled because segfault happens with some libaom versions (such as 3.5.0)
// but not others (such as 3.6.0). TODO(yguyon): Find the commit that fixed it.
TEST(DISABLED_IoStatsTest, AnimationObuSize) {
  avifIOStats io_stats;
  ASSERT_TRUE(GetIoStatsFromEncode(io_stats, AVIF_QUALITY_LOSSLESS,
                                   AVIF_QUALITY_LOSSLESS,
                                   /*translucent=*/true));

  avifIOStats io_stats_grid;
  ASSERT_TRUE(GetIoStatsFromEncode(io_stats_grid, AVIF_QUALITY_LOSSLESS,
                                   AVIF_QUALITY_LOSSLESS, /*translucent=*/true,
                                   /*num_frames=*/3));
  EXPECT_GT(io_stats_grid.colorOBUSize, io_stats.colorOBUSize);
  EXPECT_GT(io_stats_grid.alphaOBUSize, io_stats.alphaOBUSize);
}

TEST(IoStatsTest, GridObuSize) {
  avifIOStats io_stats;
  ASSERT_TRUE(GetIoStatsFromEncode(io_stats, AVIF_QUALITY_BEST,
                                   AVIF_QUALITY_BEST, /*translucent=*/true));

  avifIOStats io_stats_grid;
  ASSERT_TRUE(GetIoStatsFromEncode(io_stats_grid, AVIF_QUALITY_BEST,
                                   AVIF_QUALITY_BEST, /*translucent=*/true,
                                   /*num_frames=*/1, /*encode_as_grid=*/true));
  EXPECT_GT(io_stats_grid.colorOBUSize, io_stats.colorOBUSize / 2);
  EXPECT_LT(io_stats_grid.colorOBUSize, io_stats.colorOBUSize * 3 / 2);
  EXPECT_GT(io_stats_grid.alphaOBUSize, io_stats.alphaOBUSize / 2);
  EXPECT_LT(io_stats_grid.alphaOBUSize, io_stats.alphaOBUSize * 3 / 2);
}

TEST(IoStatsTest, GridAnimation) {
  avifIOStats io_stats;
  // Animated grids are not allowed.
  ASSERT_FALSE(GetIoStatsFromEncode(io_stats, AVIF_QUALITY_BEST,
                                    AVIF_QUALITY_BEST, /*translucent=*/false,
                                    /*num_frames=*/3, /*encode_as_grid=*/true));
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
