// Copyright 2022 Google LLC. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include <vector>

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

using testing::Combine;
using testing::Values;
using testing::ValuesIn;

namespace libavif {
namespace {

// One AVIF cell in an AVIF grid.
struct Cell {
  int width, height;  // In pixels.
};

avifResult EncodeDecodeGrid(const std::vector<std::vector<Cell>>& cell_rows,
                            avifPixelFormat yuv_format) {
  // Construct a grid.
  std::vector<testutil::AvifImagePtr> cell_images;
  cell_images.reserve(cell_rows.size() * cell_rows.front().size());
  for (const std::vector<Cell>& cell_row : cell_rows) {
    assert(cell_row.size() == cell_rows.front().size());
    for (const Cell& cell : cell_row) {
      cell_images.emplace_back(testutil::CreateImage(
          cell.width, cell.height, /*depth=*/8, yuv_format, AVIF_PLANES_ALL));
      if (!cell_images.back()) {
        return AVIF_RESULT_INVALID_ARGUMENT;
      }
      testutil::FillImageGradient(cell_images.back().get());
    }
  }

  // Encode the grid image (losslessly for easy pixel-by-pixel comparison).
  testutil::AvifEncoderPtr encoder(avifEncoderCreate(), avifEncoderDestroy);
  if (!encoder) {
    return AVIF_RESULT_OUT_OF_MEMORY;
  }
  encoder->speed = AVIF_SPEED_FASTEST;
  encoder->minQuantizer = AVIF_QUANTIZER_LOSSLESS;
  encoder->maxQuantizer = AVIF_QUANTIZER_LOSSLESS;
  encoder->minQuantizerAlpha = AVIF_QUANTIZER_LOSSLESS;
  encoder->minQuantizerAlpha = AVIF_QUANTIZER_LOSSLESS;
  // cell_image_ptrs exists only to match the libavif API.
  std::vector<avifImage*> cell_image_ptrs(cell_images.size());
  for (size_t i = 0; i < cell_images.size(); ++i) {
    cell_image_ptrs[i] = cell_images[i].get();
  }
  avifResult result = avifEncoderAddImageGrid(
      encoder.get(), static_cast<uint32_t>(cell_rows.front().size()),
      static_cast<uint32_t>(cell_rows.size()), cell_image_ptrs.data(),
      AVIF_ADD_IMAGE_FLAG_SINGLE);
  if (result != AVIF_RESULT_OK) {
    return result;
  }

  testutil::AvifRwData encoded_avif;
  result = avifEncoderFinish(encoder.get(), &encoded_avif);
  if (result != AVIF_RESULT_OK) {
    return result;
  }

  // Decode the grid image.
  testutil::AvifImagePtr image(avifImageCreateEmpty(), avifImageDestroy);
  testutil::AvifDecoderPtr decoder(avifDecoderCreate(), avifDecoderDestroy);
  if (!image || !decoder) {
    return AVIF_RESULT_OUT_OF_MEMORY;
  }
  result = avifDecoderReadMemory(decoder.get(), image.get(), encoded_avif.data,
                                 encoded_avif.size);
  if (result != AVIF_RESULT_OK) {
    return result;
  }

  // Reconstruct the input image by merging all cells into a single avifImage.
  testutil::AvifImagePtr grid = testutil::CreateImage(
      static_cast<int>(image->width), static_cast<int>(image->height),
      /*depth=*/8, yuv_format, AVIF_PLANES_ALL);
  testutil::AvifImagePtr view(avifImageCreateEmpty(), avifImageDestroy);
  if (!view) {
    return AVIF_RESULT_OUT_OF_MEMORY;
  }
  avifCropRect rect = {};
  auto it = cell_images.cbegin();
  for (const std::vector<Cell>& cell_row : cell_rows) {
    rect.x = 0;
    for (const Cell& cell : cell_row) {
      rect.width = cell.width;
      rect.height = cell.height;
      result = avifImageSetViewRect(view.get(), grid.get(), &rect);
      if (result != AVIF_RESULT_OK) {
        return result;
      }
      testutil::CopyImageSamples(**it, view.get());
      assert(!view->imageOwnsYUVPlanes);
      ++it;
      rect.x += rect.width;
    }
    rect.y += rect.height;
  }
  if ((rect.x != image->width) || (rect.y != image->height) ||
      !testutil::AreImagesEqual(*image, *image)) {
    return AVIF_RESULT_UNKNOWN_ERROR;
  }
  return AVIF_RESULT_OK;
}

TEST(GridApiTest, SingleCell) {
  for (avifPixelFormat pixel_format :
       {AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV422,
        AVIF_PIXEL_FORMAT_YUV420, AVIF_PIXEL_FORMAT_YUV400}) {
    // Rules on grids do not apply to a single cell.
    EXPECT_EQ(EncodeDecodeGrid({{{1, 1}}}, pixel_format), AVIF_RESULT_OK);
    EXPECT_EQ(EncodeDecodeGrid({{{1, 64}}}, pixel_format), AVIF_RESULT_OK);
    EXPECT_EQ(EncodeDecodeGrid({{{64, 1}}}, pixel_format), AVIF_RESULT_OK);
    EXPECT_EQ(EncodeDecodeGrid({{{64, 64}}}, pixel_format), AVIF_RESULT_OK);
    EXPECT_EQ(EncodeDecodeGrid({{{127, 127}}}, pixel_format), AVIF_RESULT_OK);
  }
}

TEST(GridApiTest, CellsOfSameDimensions) {
  for (avifPixelFormat pixel_format :
       {AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV422,
        AVIF_PIXEL_FORMAT_YUV420, AVIF_PIXEL_FORMAT_YUV400}) {
    // ISO/IEC 23000-22:2019, Section 7.3.11.4.2:
    //   - the tile_width shall be greater than or equal to 64, and should be a
    //     multiple of 64
    //   - the tile_height shall be greater than or equal to 64, and should be a
    //     multiple of 64
    EXPECT_EQ(EncodeDecodeGrid({{{64, 64}, {64, 64}, {64, 64}}}, pixel_format),
              AVIF_RESULT_OK);
    EXPECT_EQ(EncodeDecodeGrid({{{100, 110}},  //
                                {{100, 110}},  //
                                {{100, 110}}},
                               pixel_format),
              AVIF_RESULT_OK);
    EXPECT_EQ(EncodeDecodeGrid({{{64, 64}, {64, 64}, {64, 64}},
                                {{64, 64}, {64, 64}, {64, 64}},
                                {{64, 64}, {64, 64}, {64, 64}}},
                               pixel_format),
              AVIF_RESULT_OK);

    EXPECT_EQ(EncodeDecodeGrid({{{2, 64}, {2, 64}}}, pixel_format),
              AVIF_RESULT_INVALID_IMAGE_GRID);
    EXPECT_EQ(EncodeDecodeGrid({{{64, 62}, {64, 62}}}, pixel_format),
              AVIF_RESULT_INVALID_IMAGE_GRID);
    EXPECT_EQ(EncodeDecodeGrid({{{64, 2}},  //
                                {{64, 2}}},
                               pixel_format),
              AVIF_RESULT_INVALID_IMAGE_GRID);
    EXPECT_EQ(EncodeDecodeGrid({{{2, 64}},  //
                                {{2, 64}}},
                               pixel_format),
              AVIF_RESULT_INVALID_IMAGE_GRID);
  }

  // ISO/IEC 23000-22:2019, Section 7.3.11.4.2:
  //   - when the images are in the 4:2:2 chroma sampling format the horizontal
  //     tile offsets and widths, and the output width, shall be even numbers;
  EXPECT_EQ(EncodeDecodeGrid({{{64, 65}, {64, 65}}}, AVIF_PIXEL_FORMAT_YUV422),
            AVIF_RESULT_OK);
  EXPECT_EQ(EncodeDecodeGrid({{{65, 64}, {65, 64}}}, AVIF_PIXEL_FORMAT_YUV422),
            AVIF_RESULT_INVALID_IMAGE_GRID);
  //   - when the images are in the 4:2:0 chroma sampling format both the
  //     horizontal and vertical tile offsets and widths, and the output width
  //     and height, shall be even numbers.
  EXPECT_EQ(EncodeDecodeGrid({{{64, 65}, {64, 65}}}, AVIF_PIXEL_FORMAT_YUV420),
            AVIF_RESULT_INVALID_IMAGE_GRID);
  EXPECT_EQ(EncodeDecodeGrid({{{65, 64}, {65, 64}}}, AVIF_PIXEL_FORMAT_YUV420),
            AVIF_RESULT_INVALID_IMAGE_GRID);
}

TEST(GridApiTest, CellsOfDifferentDimensions) {
  for (avifPixelFormat pixel_format :
       {AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV422,
        AVIF_PIXEL_FORMAT_YUV420, AVIF_PIXEL_FORMAT_YUV400}) {
    // Right-most cells are narrower.
    EXPECT_EQ(
        EncodeDecodeGrid({{{100, 100}, {100, 100}, {66, 100}}}, pixel_format),
        AVIF_RESULT_OK);
    // Bottom-most cells are shorter.
    EXPECT_EQ(EncodeDecodeGrid({{{100, 100}, {100, 100}},
                                {{100, 100}, {100, 100}},
                                {{100, 66}, {100, 66}}},
                               pixel_format),
              AVIF_RESULT_OK);
    // Right-most cells are narrower and bottom-most cells are shorter.
    EXPECT_EQ(EncodeDecodeGrid({{{100, 100}, {100, 100}, {66, 100}},
                                {{100, 100}, {100, 100}, {66, 100}},
                                {{100, 66}, {100, 66}, {66, 66}}},
                               pixel_format),
              AVIF_RESULT_OK);

    // Right-most cells are wider.
    EXPECT_EQ(EncodeDecodeGrid({{{100, 100}, {100, 100}, {222, 100}},
                                {{100, 100}, {100, 100}, {222, 100}},
                                {{100, 100}, {100, 100}, {222, 100}}},
                               pixel_format),
              AVIF_RESULT_INVALID_IMAGE_GRID);
    // Bottom-most cells are taller.
    EXPECT_EQ(EncodeDecodeGrid({{{100, 100}, {100, 100}, {100, 100}},
                                {{100, 100}, {100, 100}, {100, 100}},
                                {{100, 222}, {100, 222}, {100, 222}}},
                               pixel_format),
              AVIF_RESULT_INVALID_IMAGE_GRID);
    // One cell dimension is off.
    EXPECT_EQ(EncodeDecodeGrid({{{100, 100}, {100, 100}, {100, 100}},
                                {{100, 100}, {66 /* here */, 100}, {100, 100}},
                                {{100, 100}, {100, 100}, {100, 100}}},
                               pixel_format),
              AVIF_RESULT_INVALID_IMAGE_GRID);
    EXPECT_EQ(EncodeDecodeGrid({{{100, 100}, {100, 100}, {66, 100}},
                                {{100, 100}, {100, 100}, {66, 100}},
                                {{100, 66}, {100, 66}, {66, 100 /* here */}}},
                               pixel_format),
              AVIF_RESULT_INVALID_IMAGE_GRID);
  }

  // ISO/IEC 23000-22:2019, Section 7.3.11.4.2:
  //   - when the images are in the 4:2:2 chroma sampling format the horizontal
  //     tile offsets and widths, and the output width, shall be even numbers;
  EXPECT_EQ(EncodeDecodeGrid({{{66, 66}},  //
                              {{66, 65}}},
                             AVIF_PIXEL_FORMAT_YUV422),
            AVIF_RESULT_OK);
  EXPECT_EQ(EncodeDecodeGrid({{{66, 66}, {65, 66}}}, AVIF_PIXEL_FORMAT_YUV422),
            AVIF_RESULT_INVALID_IMAGE_GRID);
  //   - when the images are in the 4:2:0 chroma sampling format both the
  //     horizontal and vertical tile offsets and widths, and the output width
  //     and height, shall be even numbers.
  EXPECT_EQ(EncodeDecodeGrid({{{66, 66}},  //
                              {{66, 65}}},
                             AVIF_PIXEL_FORMAT_YUV420),
            AVIF_RESULT_INVALID_IMAGE_GRID);
  EXPECT_EQ(EncodeDecodeGrid({{{66, 66}, {65, 66}}}, AVIF_PIXEL_FORMAT_YUV420),
            AVIF_RESULT_INVALID_IMAGE_GRID);
}

}  // namespace
}  // namespace libavif
