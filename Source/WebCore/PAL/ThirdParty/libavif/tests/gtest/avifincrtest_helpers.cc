// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "avifincrtest_helpers.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace testutil {
namespace {

//------------------------------------------------------------------------------

// Verifies that the first (top) row_count rows of image1 and image2 are
// identical.
void ComparePartialYuva(const avifImage& image1, const avifImage& image2,
                        uint32_t row_count) {
  if (row_count == 0) {
    return;
  }
  ASSERT_EQ(image1.width, image2.width);
  ASSERT_GE(image1.height, row_count);
  ASSERT_GE(image2.height, row_count);
  ASSERT_EQ(image1.depth, image2.depth);
  ASSERT_EQ(image1.yuvFormat, image2.yuvFormat);
  ASSERT_EQ(image1.yuvRange, image2.yuvRange);

  avifPixelFormatInfo info;
  avifGetPixelFormatInfo(image1.yuvFormat, &info);
  const uint32_t uv_height =
      info.monochrome ? 0
                      : ((row_count + info.chromaShiftY) >> info.chromaShiftY);
  const size_t pixel_byte_count =
      (image1.depth > 8) ? sizeof(uint16_t) : sizeof(uint8_t);

  if (image1.alphaPlane) {
    ASSERT_NE(image2.alphaPlane, nullptr);
    ASSERT_EQ(image1.alphaPremultiplied, image2.alphaPremultiplied);
  }

  const int last_plane = image1.alphaPlane ? AVIF_CHAN_A : AVIF_CHAN_V;
  for (int plane = AVIF_CHAN_Y; plane <= last_plane; ++plane) {
    const size_t width_byte_count =
        avifImagePlaneWidth(&image1, plane) * pixel_byte_count;
    const uint32_t height =
        (plane == AVIF_CHAN_Y || plane == AVIF_CHAN_A) ? row_count : uv_height;
    const uint8_t* row1 = avifImagePlane(&image1, plane);
    const uint8_t* row2 = avifImagePlane(&image2, plane);
    const uint32_t row1_bytes = avifImagePlaneRowBytes(&image1, plane);
    const uint32_t row2_bytes = avifImagePlaneRowBytes(&image2, plane);
    for (uint32_t y = 0; y < height; ++y) {
      ASSERT_EQ(std::memcmp(row1, row2, width_byte_count), 0);
      row1 += row1_bytes;
      row2 += row2_bytes;
    }
  }

#if defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
  if (image1.gainMap != nullptr && image1.gainMap->image != nullptr &&
      image2.gainMap != nullptr && image2.gainMap->image != nullptr) {
    const uint32_t gain_map_row_count = (uint32_t)roundf(
        (float)row_count / image1.height * image1.gainMap->image->height);
    ComparePartialYuva(*image1.gainMap->image, *image2.gainMap->image,
                       gain_map_row_count);
  }
#endif
}

// Returns the expected number of decoded rows when available_byte_count out of
// byte_count were given to the decoder, for an image of height rows, split into
// cells of cell_height rows.
uint32_t GetMinDecodedRowCount(uint32_t height, uint32_t cell_height,
                               bool has_alpha, size_t available_byte_count,
                               size_t byte_count,
                               bool enable_fine_incremental_check) {
  // The whole image should be available when the full input is.
  if (available_byte_count >= byte_count) {
    return height;
  }
  // All but one cell should be decoded if at most 10 bytes are missing.
  if ((available_byte_count + 10) >= byte_count) {
    return height - cell_height;
  }

  // The tests below can be hard to tune for any kind of input, especially
  // fuzzed grids. Early exit in that case.
  if (!enable_fine_incremental_check) return 0;

  // Subtract the header because decoding it does not output any pixel.
  // Most AVIF headers are below 500 bytes.
  if (available_byte_count <= 500) {
    return 0;
  }
  available_byte_count -= 500;
  byte_count -= 500;
  // Alpha, if any, is assumed to be located before the other planes and to
  // represent at most 50% of the payload.
  if (has_alpha) {
    if (available_byte_count <= (byte_count / 2)) {
      return 0;
    }
    available_byte_count -= byte_count / 2;
    byte_count -= byte_count / 2;
  }
  // Linearly map the input availability ratio to the decoded row ratio.
  const uint32_t min_decoded_cell_row_count = static_cast<uint32_t>(
      (height / cell_height) * available_byte_count / byte_count);
  const uint32_t min_decoded_px_row_count =
      min_decoded_cell_row_count * cell_height;
  // One cell is the incremental decoding granularity.
  // It is unlikely that bytes are evenly distributed among cells. Offset two of
  // them.
  if (min_decoded_px_row_count <= (2 * cell_height)) {
    return 0;
  }
  return min_decoded_px_row_count - 2 * cell_height;
}

//------------------------------------------------------------------------------

struct PartialData {
  avifROData available;
  size_t full_size;

  // Only used as nonpersistent input.
  std::unique_ptr<uint8_t[]> nonpersistent_bytes;
  size_t num_nonpersistent_bytes;
};

// Implementation of avifIOReadFunc simulating a stream from an array. See
// avifIOReadFunc documentation. io->data is expected to point to PartialData.
avifResult PartialRead(struct avifIO* io, uint32_t read_flags,
                       uint64_t offset64, size_t size, avifROData* out) {
  PartialData* data = reinterpret_cast<PartialData*>(io->data);
  if ((read_flags != 0) || !data || (data->full_size < offset64)) {
    return AVIF_RESULT_IO_ERROR;
  }
  const size_t offset = static_cast<size_t>(offset64);
  // Use |offset| instead of |offset64| from this point on.
  if (size > (data->full_size - offset)) {
    size = data->full_size - offset;
  }
  if (data->available.size < (offset + size)) {
    return AVIF_RESULT_WAITING_ON_IO;
  }
  if (io->persistent) {
    out->data = data->available.data + offset;
  } else {
    // Dedicated buffer containing just the available bytes and nothing more.
    std::unique_ptr<uint8_t[]> bytes(new uint8_t[size]);
    std::copy(data->available.data + offset,
              data->available.data + offset + size, bytes.get());
    out->data = bytes.get();
    // Flip the previously returned bytes to make sure the values changed.
    for (size_t i = 0; i < data->num_nonpersistent_bytes; ++i) {
      data->nonpersistent_bytes[i] = ~data->nonpersistent_bytes[i];
    }
    // Free the memory to invalidate the old pointer. Only do that after
    // allocating the new bytes to make sure to have a different pointer.
    data->nonpersistent_bytes = std::move(bytes);
    data->num_nonpersistent_bytes = size;
  }
  out->size = size;
  return AVIF_RESULT_OK;
}

//------------------------------------------------------------------------------

// Encodes the image as a grid of at most grid_cols*grid_rows cells.
// The cell count is reduced to fit libavif or AVIF format constraints. If
// impossible, the encoded output is returned empty. The final cell_width and
// cell_height are output.
void EncodeAsGrid(const avifImage& image, uint32_t grid_cols,
                  uint32_t grid_rows, avifRWData* output, uint32_t* cell_width,
                  uint32_t* cell_height) {
  // Chroma subsampling requires even dimensions. See ISO 23000-22 - 7.3.11.4.2
  const bool need_even_widths =
      ((image.yuvFormat == AVIF_PIXEL_FORMAT_YUV420) ||
       (image.yuvFormat == AVIF_PIXEL_FORMAT_YUV422));
  const bool need_even_heights = (image.yuvFormat == AVIF_PIXEL_FORMAT_YUV420);

  ASSERT_GT(grid_cols * grid_rows, 0u);
  *cell_width = image.width / grid_cols;
  *cell_height = image.height / grid_rows;

  // avifEncoderAddImageGrid() only accepts grids that evenly split the image
  // into cells at least 64 pixels wide and tall.
  while ((grid_cols > 1) &&
         (((*cell_width * grid_cols) != image.width) || (*cell_width < 64) ||
          (need_even_widths && ((*cell_width & 1) != 0)))) {
    --grid_cols;
    *cell_width = image.width / grid_cols;
  }
  while ((grid_rows > 1) &&
         (((*cell_height * grid_rows) != image.height) || (*cell_height < 64) ||
          (need_even_heights && ((*cell_height & 1) != 0)))) {
    --grid_rows;
    *cell_height = image.height / grid_rows;
  }

  std::vector<ImagePtr> cell_images;
  cell_images.reserve(grid_cols * grid_rows);
  for (uint32_t row = 0, i_cell = 0; row < grid_rows; ++row) {
    for (uint32_t col = 0; col < grid_cols; ++col, ++i_cell) {
      avifCropRect cell;
      cell.x = col * *cell_width;
      cell.y = row * *cell_height;
      cell.width = ((cell.x + *cell_width) <= image.width)
                       ? *cell_width
                       : (image.width - cell.x);
      cell.height = ((cell.y + *cell_height) <= image.height)
                        ? *cell_height
                        : (image.height - cell.y);
      cell_images.emplace_back(avifImageCreateEmpty());
      ASSERT_NE(cell_images.back(), nullptr);
      ASSERT_EQ(avifImageSetViewRect(cell_images.back().get(), &image, &cell),
                AVIF_RESULT_OK);
    }
  }

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  encoder->speed = AVIF_SPEED_FASTEST;
  // Just here to match libavif API.
  std::vector<avifImage*> cell_image_ptrs(cell_images.size());
  for (size_t i = 0; i < cell_images.size(); ++i) {
    cell_image_ptrs[i] = cell_images[i].get();
  }
  ASSERT_EQ(avifEncoderAddImageGrid(encoder.get(), grid_cols, grid_rows,
                                    cell_image_ptrs.data(),
                                    AVIF_ADD_IMAGE_FLAG_SINGLE),
            AVIF_RESULT_OK);
  ASSERT_EQ(avifEncoderFinish(encoder.get(), output), AVIF_RESULT_OK);
}

// Encodes the image to be decoded incrementally.
void EncodeAsIncremental(const avifImage& image, bool flat_cells,
                         avifRWData* output, uint32_t* cell_width,
                         uint32_t* cell_height) {
  const uint32_t grid_cols = image.width / 64;  // 64px is the min cell width.
  const uint32_t grid_rows = flat_cells ? 1 : (image.height / 64);
  EncodeAsGrid(image, (grid_cols > 1) ? grid_cols : 1,
               (grid_rows > 1) ? grid_rows : 1, output, cell_width,
               cell_height);
}

}  // namespace

void EncodeRectAsIncremental(const avifImage& image, uint32_t width,
                             uint32_t height, bool create_alpha_if_none,
                             bool flat_cells, avifRWData* output,
                             uint32_t* cell_width, uint32_t* cell_height) {
  ImagePtr sub_image(avifImageCreateEmpty());
  ASSERT_NE(sub_image, nullptr);
  ASSERT_LE(width, image.width);
  ASSERT_LE(height, image.height);
  // Encode the centered rect of dimensions width*height from the image.
  avifCropRect rect{/*x=*/(image.width - width) / 2,
                    /*y=*/(image.height - height) / 2, width, height};
  avifPixelFormatInfo info;
  avifGetPixelFormatInfo(image.yuvFormat, &info);
  if (!info.monochrome) {
    // Use even coordinates in subsampled dimensions.
    rect.x &= ~info.chromaShiftX;
    rect.y &= ~info.chromaShiftY;
  }
  ASSERT_EQ(avifImageSetViewRect(sub_image.get(), &image, &rect),
            AVIF_RESULT_OK);
  if (create_alpha_if_none && !sub_image->alphaPlane) {
    ASSERT_NE(image.yuvPlanes[AVIF_CHAN_Y], nullptr)
        << "No luma plane to simulate an alpha plane";
    sub_image->alphaPlane = image.yuvPlanes[AVIF_CHAN_Y];
    sub_image->alphaRowBytes = image.yuvRowBytes[AVIF_CHAN_Y];
    sub_image->alphaPremultiplied = AVIF_FALSE;
    sub_image->imageOwnsAlphaPlane = AVIF_FALSE;
  }
  EncodeAsIncremental(*sub_image, flat_cells, output, cell_width, cell_height);
}

//------------------------------------------------------------------------------

avifResult DecodeIncrementally(const avifRWData& encoded_avif,
                               avifDecoder* decoder, bool is_persistent,
                               bool give_size_hint, bool use_nth_image_api,
                               const avifImage& reference, uint32_t cell_height,
                               bool enable_fine_incremental_check,
                               bool expect_whole_file_read) {
  // AVIF cells are at least 64 pixels tall.
  if (cell_height != reference.height) {
    AVIF_CHECKERR(cell_height >= 64u, AVIF_RESULT_INVALID_ARGUMENT);
  }

  // Emulate a byte-by-byte stream.
  PartialData data = {
      /*available=*/{encoded_avif.data, 0}, /*fullSize=*/encoded_avif.size,
      /*nonpersistent_bytes=*/nullptr, /*num_nonpersistent_bytes=*/0};
  avifIO io = {
      /*destroy=*/nullptr, PartialRead,
      /*write=*/nullptr,   give_size_hint ? encoded_avif.size : 0,
      is_persistent,       &data};
  avifDecoderSetIO(decoder, &io);
  // Reset the decoder's IO to nullptr before 'io' goes out of scope and becomes
  // invalid.
  auto cleanup_io_fn = [](avifDecoder* decoder) {
    avifDecoderSetIO(decoder, nullptr);
  };
  std::unique_ptr<avifDecoder, decltype(cleanup_io_fn)> cleanup_io(
      decoder, cleanup_io_fn);  // Call automatically at end of scope.

  decoder->allowIncremental = AVIF_TRUE;
  const size_t step = std::max<size_t>(1, data.full_size / 10000);

  // Parsing is not incremental.
  avifResult parse_result = avifDecoderParse(decoder);
  while (parse_result == AVIF_RESULT_WAITING_ON_IO) {
    if (data.available.size >= data.full_size) {
      std::cerr << "avifDecoderParse() returned WAITING_ON_IO instead of OK"
                << std::endl;
      return AVIF_RESULT_TRUNCATED_DATA;
    }
    data.available.size = std::min(data.available.size + step, data.full_size);
    parse_result = avifDecoderParse(decoder);
  }
  AVIF_CHECKRES(parse_result);

  // Decoding is incremental.
  uint32_t previously_decoded_row_count = 0;
  avifResult next_image_result = use_nth_image_api
                                     ? avifDecoderNthImage(decoder, 0)
                                     : avifDecoderNextImage(decoder);
  while (next_image_result == AVIF_RESULT_WAITING_ON_IO) {
    if (data.available.size >= data.full_size) {
      std::cerr << (use_nth_image_api ? "avifDecoderNthImage(0)"
                                      : "avifDecoderNextImage()")
                << " returned WAITING_ON_IO instead of OK";
      return AVIF_RESULT_INVALID_ARGUMENT;
    }
    const uint32_t decoded_row_count = avifDecoderDecodedRowCount(decoder);
    AVIF_CHECKERR(decoded_row_count >= previously_decoded_row_count,
                  AVIF_RESULT_INVALID_ARGUMENT);
    const uint32_t min_decoded_row_count = GetMinDecodedRowCount(
        reference.height, cell_height, reference.alphaPlane != nullptr,
        data.available.size, data.full_size, enable_fine_incremental_check);
    AVIF_CHECKERR(decoded_row_count >= min_decoded_row_count,
                  AVIF_RESULT_INVALID_ARGUMENT);
    ComparePartialYuva(reference, *decoder->image, decoded_row_count);

    previously_decoded_row_count = decoded_row_count;
    data.available.size = std::min(data.available.size + step, data.full_size);
    next_image_result = use_nth_image_api ? avifDecoderNthImage(decoder, 0)
                                          : avifDecoderNextImage(decoder);
  }
  AVIF_CHECKRES(next_image_result);
  if (expect_whole_file_read) {
    AVIF_CHECKERR(data.available.size == data.full_size,
                  AVIF_RESULT_INVALID_ARGUMENT);
  }
  AVIF_CHECKERR(avifDecoderDecodedRowCount(decoder) == decoder->image->height,
                AVIF_RESULT_INVALID_ARGUMENT);

  ComparePartialYuva(reference, *decoder->image, reference.height);
  return AVIF_RESULT_OK;
}

avifResult DecodeNonIncrementallyAndIncrementally(
    const avifRWData& encoded_avif, avifDecoder* decoder, bool is_persistent,
    bool give_size_hint, bool use_nth_image_api, uint32_t cell_height,
    bool enable_fine_incremental_check, bool expect_whole_file_read) {
  ImagePtr reference(avifImageCreateEmpty());
  if (reference == nullptr) return AVIF_RESULT_INVALID_ARGUMENT;
  decoder->allowIncremental = AVIF_FALSE;
  if (avifDecoderReadMemory(decoder, reference.get(), encoded_avif.data,
                            encoded_avif.size) != AVIF_RESULT_OK) {
    return AVIF_RESULT_INVALID_ARGUMENT;
  }

  const avifResult result = DecodeIncrementally(
      encoded_avif, decoder, is_persistent, give_size_hint, use_nth_image_api,
      *reference, cell_height, enable_fine_incremental_check,
      expect_whole_file_read);
  return result;
}

//------------------------------------------------------------------------------

}  // namespace testutil
}  // namespace avif
