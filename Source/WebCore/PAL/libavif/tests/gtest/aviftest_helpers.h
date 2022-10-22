// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#ifndef LIBAVIF_TESTS_AVIFTEST_HELPERS_H_
#define LIBAVIF_TESTS_AVIFTEST_HELPERS_H_

#include <limits>
#include <memory>

#include "avif/avif.h"

namespace libavif {
namespace testutil {

//------------------------------------------------------------------------------
// Memory management

using AvifImagePtr = std::unique_ptr<avifImage, decltype(&avifImageDestroy)>;
using AvifEncoderPtr =
    std::unique_ptr<avifEncoder, decltype(&avifEncoderDestroy)>;
using AvifDecoderPtr =
    std::unique_ptr<avifDecoder, decltype(&avifDecoderDestroy)>;

class AvifRwData : public avifRWData {
 public:
  AvifRwData() : avifRWData{nullptr, 0} {}
  AvifRwData(const AvifRwData&) = delete;
  AvifRwData(AvifRwData&& other);
  ~AvifRwData() { avifRWDataFree(this); }
};

class AvifRgbImage : public avifRGBImage {
 public:
  AvifRgbImage(const avifImage* yuv, int rgbDepth, avifRGBFormat rgbFormat);
  ~AvifRgbImage() { avifRGBImageFreePixels(this); }
};

//------------------------------------------------------------------------------
// Samples and images

// Contains the sample position of each channel for a given avifRGBFormat.
// The alpha sample position is set to 0 for layouts having no alpha channel.
struct RgbChannelOffsets {
  uint8_t r, g, b, a;
};
RgbChannelOffsets GetRgbChannelOffsets(avifRGBFormat format);

// Creates an image. Returns null in case of memory failure.
AvifImagePtr CreateImage(int width, int height, int depth,
                         avifPixelFormat yuv_format, avifPlanesFlags planes,
                         avifRange yuv_range = AVIF_RANGE_FULL);

// Copy the pixel values from an image to another. They must share the same
// features (dimensions, depth etc.).
void CopyImageSamples(const avifImage& from, avifImage* to);

// Set all pixels of each plane of an image.
void FillImagePlain(avifImage* image, const uint32_t yuva[4]);
void FillImageGradient(avifImage* image);
void FillImageChannel(avifRGBImage* image, uint32_t channel_offset,
                      uint32_t value);

// Returns true if both arrays are empty or have the same length and bytes.
// data1 may be null only when data1_length is 0.
// data2 may be null only when data2_length is 0.
bool AreByteSequencesEqual(const uint8_t data1[], size_t data1_length,
                           const uint8_t data2[], size_t data2_length);
bool AreByteSequencesEqual(const avifRWData& data1, const avifRWData& data2);

// Returns true if both images have the same features, pixel values and
// metadata. If ignore_alpha is true, the alpha channel is not taken into
// account in the comparison.
bool AreImagesEqual(const avifImage& image1, const avifImage& image2,
                    bool ignore_alpha = false);

//------------------------------------------------------------------------------
// Shorter versions of libavif functions

// Reads the image named file_name located in directory at folder_path.
// Returns nullptr in case of error.
AvifImagePtr ReadImage(
    const char* folder_path, const char* file_name,
    avifPixelFormat requested_format = AVIF_PIXEL_FORMAT_NONE,
    int requested_depth = 0,
    avifChromaDownsampling chromaDownsampling =
        AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC,
    avifBool ignore_icc = false, avifBool ignore_exif = false,
    avifBool ignore_xmp = false);

// Encodes the image with default parameters.
// Returns an empty payload in case of error.
AvifRwData Encode(const avifImage* image, int speed = AVIF_SPEED_DEFAULT);

// Decodes the bytes to an image with default parameters.
// Returns nullptr in case of error.
AvifImagePtr Decode(const uint8_t* bytes, size_t num_bytes);

//------------------------------------------------------------------------------
// avifIO overlay

struct AvifIOLimitedReader {
  static constexpr uint64_t kNoClamp = std::numeric_limits<uint64_t>::max();

  avifIO io;
  avifIO* underlyingIO;
  uint64_t clamp;
};

avifIO* AvifIOCreateLimitedReader(avifIO* underlyingIO, uint64_t clamp);

}  // namespace testutil
}  // namespace libavif

//------------------------------------------------------------------------------

#endif  // LIBAVIF_TESTS_AVIFTEST_HELPERS_H_
