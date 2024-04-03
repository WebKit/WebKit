// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#ifndef LIBAVIF_TESTS_AVIFTEST_HELPERS_H_
#define LIBAVIF_TESTS_AVIFTEST_HELPERS_H_

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "avif/avif.h"
#include "avif/avif_cxx.h"

//------------------------------------------------------------------------------
// Duplicated from internal.h
// Used for debugging. Define AVIF_BREAK_ON_ERROR to catch the earliest failure
// during encoding or decoding.
#if defined(AVIF_BREAK_ON_ERROR)
static inline void avifBreakOnError() {
  // Same mechanism as OpenCV's error() function, or replace by a breakpoint.
  int* p = NULL;
  *p = 0;
}
#else
#define avifBreakOnError()
#endif

// Used by stream related things.
#define AVIF_CHECK(A)     \
  do {                    \
    if (!(A)) {           \
      avifBreakOnError(); \
      return AVIF_FALSE;  \
    }                     \
  } while (0)

// Used instead of CHECK if needing to return a specific error on failure,
// instead of AVIF_FALSE
#define AVIF_CHECKERR(A, ERR) \
  do {                        \
    if (!(A)) {               \
      avifBreakOnError();     \
      return ERR;             \
    }                         \
  } while (0)

// Forward any error to the caller now or continue execution.
#define AVIF_CHECKRES(A)              \
  do {                                \
    const avifResult result__ = (A);  \
    if (result__ != AVIF_RESULT_OK) { \
      avifBreakOnError();             \
      return result__;                \
    }                                 \
  } while (0)
//------------------------------------------------------------------------------

namespace avif {
namespace testutil {

//------------------------------------------------------------------------------

// ICC color profiles are not checked by libavif so the content does not matter.
// This is a truncated widespread ICC color profile.
static const std::array<uint8_t, 24> kSampleIcc = {
    0x00, 0x00, 0x02, 0x0c, 0x6c, 0x63, 0x6d, 0x73, 0x02, 0x10, 0x00, 0x00,
    0x6d, 0x6e, 0x74, 0x72, 0x52, 0x47, 0x42, 0x20, 0x58, 0x59, 0x5a, 0x20};

// Exif bytes are partially checked by libavif.
// This is a truncated widespread Exif metadata chunk.
static const std::array<uint8_t, 24> kSampleExif = {
    0xff, 0x1,  0x45, 0x78, 0x69, 0x76, 0x32, 0xff, 0xe1, 0x12, 0x5a, 0x45,
    0x78, 0x69, 0x66, 0x0,  0x0,  0x49, 0x49, 0x2a, 0x0,  0x8,  0x0,  0x0};

// XMP bytes are not checked by libavif so the content does not matter.
// This is a truncated widespread XMP metadata chunk.
static const std::array<uint8_t, 24> kSampleXmp = {
    0x3c, 0x3f, 0x78, 0x70, 0x61, 0x63, 0x6b, 0x65, 0x74, 0x20, 0x62, 0x65,
    0x67, 0x69, 0x6e, 0x3d, 0x22, 0xef, 0xbb, 0xbf, 0x22, 0x20, 0x69, 0x64};

//------------------------------------------------------------------------------
// Memory management

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
ImagePtr CreateImage(int width, int height, int depth,
                     avifPixelFormat yuv_format, avifPlanesFlags planes,
                     avifRange yuv_range = AVIF_RANGE_FULL);

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

// Returns true if both images have the same features and pixel values.
bool AreImagesEqual(const avifRGBImage& image1, const avifRGBImage& image2);

// Returns the Peak Signal-to-Noise Ratio of image1 compared to image2.
// A value of 99dB means all samples are exactly the same.
// A negative value means that the input images cannot be compared.
double GetPsnr(const avifImage& image1, const avifImage& image2,
               bool ignore_alpha = false);

// Merges the given image grid cells into a single image.
avifResult MergeGrid(int grid_cols, int grid_rows,
                     const std::vector<ImagePtr>& cells, avifImage* merged);
avifResult MergeGrid(int grid_cols, int grid_rows,
                     const std::vector<const avifImage*>& cells,
                     avifImage* merged);

//------------------------------------------------------------------------------
// Shorter versions of libavif functions

// Reads the image named file_name located in directory at folder_path.
// Returns nullptr in case of error.
ImagePtr ReadImage(const char* folder_path, const char* file_name,
                   avifPixelFormat requested_format = AVIF_PIXEL_FORMAT_NONE,
                   int requested_depth = 0,
                   avifChromaDownsampling chroma_downsampling =
                       AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC,
                   avifBool ignore_icc = false, avifBool ignore_exif = false,
                   avifBool ignore_xmp = false,
                   avifBool allow_changing_cicp = true,
                   avifBool ignore_gain_map = false);
// Convenient wrapper around avifPNGWrite() for debugging purposes.
// Do not remove.
bool WriteImage(const avifImage* image, const char* file_path);

// Encodes the image with default parameters.
// Returns an empty payload in case of error.
AvifRwData Encode(const avifImage* image, int speed = AVIF_SPEED_DEFAULT,
                  int quality = AVIF_QUALITY_DEFAULT);

// Decodes the bytes to an image with default parameters.
// Returns nullptr in case of error.
ImagePtr Decode(const uint8_t* bytes, size_t num_bytes);

// Decodes the file to an image with default parameters.
// Returns nullptr in case of error.
ImagePtr DecodeFile(const std::string& path);

// Returns true if an AV1 encoder is available.
bool Av1EncoderAvailable();

// Returns true if an AV1 decoder is available.
bool Av1DecoderAvailable();

//------------------------------------------------------------------------------
// avifIO overlay

struct AvifIOLimitedReader {
  static constexpr uint64_t kNoClamp = std::numeric_limits<uint64_t>::max();

  avifIO io;
  avifIO* underlyingIO;
  uint64_t clamp;
};

avifIO* AvifIOCreateLimitedReader(avifIO* underlyingIO, uint64_t clamp);

//------------------------------------------------------------------------------

// Splits the input image into grid_cols*grid_rows views to be encoded as a
// grid. Returns an empty vector if the input image cannot be split that way.
std::vector<ImagePtr> ImageToGrid(const avifImage* image, uint32_t grid_cols,
                                  uint32_t grid_rows);

// Converts a unique_ptr array to a raw pointer array as needed by libavif API.
std::vector<const avifImage*> UniquePtrToRawPtr(
    const std::vector<ImagePtr>& unique_ptrs);

//------------------------------------------------------------------------------

}  // namespace testutil
}  // namespace avif

#endif  // LIBAVIF_TESTS_AVIFTEST_HELPERS_H_
