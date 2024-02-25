// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "avif_fuzztest_helpers.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <utility>
#include <vector>

#include "avif/avif.h"
#include "avifutil.h"
#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

namespace avif {
namespace testutil {
namespace {

//------------------------------------------------------------------------------

ImagePtr CreateAvifImage(size_t width, size_t height, int depth,
                         avifPixelFormat pixel_format, bool has_alpha,
                         const uint8_t* samples) {
  ImagePtr image(avifImageCreate(static_cast<uint32_t>(width),
                                 static_cast<uint32_t>(height), depth,
                                 pixel_format));
  if (image.get() == nullptr) {
    return image;
  }
  if (avifImageAllocatePlanes(image.get(),
                              has_alpha ? AVIF_PLANES_ALL : AVIF_PLANES_YUV) !=
      AVIF_RESULT_OK) {
    return nullptr;
  }

  for (avifChannelIndex c :
       {AVIF_CHAN_Y, AVIF_CHAN_U, AVIF_CHAN_V, AVIF_CHAN_A}) {
    const size_t plane_height = avifImagePlaneHeight(image.get(), c);
    uint8_t* row = avifImagePlane(image.get(), c);
    const uint32_t row_bytes = avifImagePlaneRowBytes(image.get(), c);
    assert(row_bytes == avifImagePlaneWidth(image.get(), c) *
                            (avifImageUsesU16(image.get()) ? 2 : 1));
    for (size_t y = 0; y < plane_height; ++y) {
      std::copy(samples, samples + row_bytes, row);
      row += row_bytes;
      samples += row_bytes;
    }
  }
  return image;
}

}  // namespace

ImagePtr CreateAvifImage8b(size_t width, size_t height,
                           avifPixelFormat pixel_format, bool has_alpha,
                           const std::vector<uint8_t>& samples) {
  return CreateAvifImage(width, height, 8, pixel_format, has_alpha,
                         samples.data());
}

ImagePtr CreateAvifImage16b(size_t width, size_t height, int depth,
                            avifPixelFormat pixel_format, bool has_alpha,
                            const std::vector<uint16_t>& samples) {
  return CreateAvifImage(width, height, depth, pixel_format, has_alpha,
                         reinterpret_cast<const uint8_t*>(samples.data()));
}

EncoderPtr CreateAvifEncoder(avifCodecChoice codec_choice, int max_threads,
                             int min_quantizer, int max_quantizer,
                             int min_quantizer_alpha, int max_quantizer_alpha,
                             int tile_rows_log2, int tile_cols_log2,
                             int speed) {
  EncoderPtr encoder(avifEncoderCreate());
  if (encoder.get() == nullptr) {
    return encoder;
  }
  encoder->codecChoice = codec_choice;
  encoder->maxThreads = max_threads;
  // minQuantizer must be at most maxQuantizer.
  encoder->minQuantizer = std::min(min_quantizer, max_quantizer);
  encoder->maxQuantizer = std::max(min_quantizer, max_quantizer);
  encoder->minQuantizerAlpha =
      std::min(min_quantizer_alpha, max_quantizer_alpha);
  encoder->maxQuantizerAlpha =
      std::max(min_quantizer_alpha, max_quantizer_alpha);
  encoder->tileRowsLog2 = tile_rows_log2;
  encoder->tileColsLog2 = tile_cols_log2;
  encoder->speed = speed;
  return encoder;
}

DecoderPtr CreateAvifDecoder(avifCodecChoice codec_choice, int max_threads,
                             avifDecoderSource requested_source,
                             bool allow_progressive, bool allow_incremental,
                             bool ignore_exif, bool ignore_xmp,
                             uint32_t image_size_limit,
                             uint32_t image_dimension_limit,
                             uint32_t image_count_limit,
                             avifStrictFlags strict_flags) {
  DecoderPtr decoder(avifDecoderCreate());
  if (decoder.get() == nullptr) {
    return decoder;
  }
  decoder->codecChoice = codec_choice;
  decoder->maxThreads = max_threads;
  decoder->requestedSource = requested_source;
  decoder->allowProgressive = allow_progressive;
  decoder->allowIncremental = allow_incremental;
  decoder->ignoreExif = ignore_exif;
  decoder->ignoreXMP = ignore_xmp;
  decoder->imageSizeLimit = image_size_limit;
  decoder->imageDimensionLimit = image_dimension_limit;
  decoder->imageCountLimit = image_count_limit;
  decoder->strictFlags = strict_flags;
  return decoder;
}

ImagePtr AvifImageToUniquePtr(avifImage* image) { return ImagePtr(image); }

#if defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
DecoderPtr AddGainMapOptionsToDecoder(DecoderPtr decoder,
                                      bool enable_parsing_gain_map_metadata,
                                      bool enable_decoding_gain_map) {
  decoder->enableParsingGainMapMetadata = enable_parsing_gain_map_metadata;
  decoder->enableDecodingGainMap = enable_decoding_gain_map;
  // Do not fuzz 'ignoreColorAndAlpha' since most tests assume that if the
  // file/buffer is successfully decoded, then the main image was decoded, which
  // is no longer the case when this option is on.
  return decoder;
}
#endif

//------------------------------------------------------------------------------

size_t GetNumSamples(size_t width, size_t height, avifPixelFormat pixel_format,
                     bool has_alpha) {
  const size_t num_luma_samples = width * height;

  avifPixelFormatInfo pixel_format_info;
  avifGetPixelFormatInfo(pixel_format, &pixel_format_info);
  size_t num_chroma_samples = 0;
  if (!pixel_format_info.monochrome) {
    num_chroma_samples = 2 *
                         ((width + pixel_format_info.chromaShiftX) >>
                          pixel_format_info.chromaShiftX) *
                         ((height + pixel_format_info.chromaShiftY) >>
                          pixel_format_info.chromaShiftY);
  }

  size_t num_alpha_samples = 0;
  if (has_alpha) {
    num_alpha_samples = width * height;
  }

  return num_luma_samples + num_chroma_samples + num_alpha_samples;
}

//------------------------------------------------------------------------------

std::vector<uint8_t> GetWhiteSinglePixelAvif() {
  return {
      0x0,  0x0,  0x0,  0x20, 0x66, 0x74, 0x79, 0x70, 0x61, 0x76, 0x69, 0x66,
      0x0,  0x0,  0x0,  0x0,  0x61, 0x76, 0x69, 0x66, 0x6d, 0x69, 0x66, 0x31,
      0x6d, 0x69, 0x61, 0x66, 0x4d, 0x41, 0x31, 0x41, 0x0,  0x0,  0x0,  0xf2,
      0x6d, 0x65, 0x74, 0x61, 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x28,
      0x68, 0x64, 0x6c, 0x72, 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,
      0x70, 0x69, 0x63, 0x74, 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,
      0x0,  0x0,  0x0,  0x0,  0x6c, 0x69, 0x62, 0x61, 0x76, 0x69, 0x66, 0x0,
      0x0,  0x0,  0x0,  0xe,  0x70, 0x69, 0x74, 0x6d, 0x0,  0x0,  0x0,  0x0,
      0x0,  0x1,  0x0,  0x0,  0x0,  0x1e, 0x69, 0x6c, 0x6f, 0x63, 0x0,  0x0,
      0x0,  0x0,  0x44, 0x0,  0x0,  0x1,  0x0,  0x1,  0x0,  0x0,  0x0,  0x1,
      0x0,  0x0,  0x1,  0x1a, 0x0,  0x0,  0x0,  0x17, 0x0,  0x0,  0x0,  0x28,
      0x69, 0x69, 0x6e, 0x66, 0x0,  0x0,  0x0,  0x0,  0x0,  0x1,  0x0,  0x0,
      0x0,  0x1a, 0x69, 0x6e, 0x66, 0x65, 0x2,  0x0,  0x0,  0x0,  0x0,  0x1,
      0x0,  0x0,  0x61, 0x76, 0x30, 0x31, 0x43, 0x6f, 0x6c, 0x6f, 0x72, 0x0,
      0x0,  0x0,  0x0,  0x6a, 0x69, 0x70, 0x72, 0x70, 0x0,  0x0,  0x0,  0x4b,
      0x69, 0x70, 0x63, 0x6f, 0x0,  0x0,  0x0,  0x14, 0x69, 0x73, 0x70, 0x65,
      0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x1,  0x0,  0x0,  0x0,  0x1,
      0x0,  0x0,  0x0,  0x10, 0x70, 0x69, 0x78, 0x69, 0x0,  0x0,  0x0,  0x0,
      0x3,  0x8,  0x8,  0x8,  0x0,  0x0,  0x0,  0xc,  0x61, 0x76, 0x31, 0x43,
      0x81, 0x20, 0x0,  0x0,  0x0,  0x0,  0x0,  0x13, 0x63, 0x6f, 0x6c, 0x72,
      0x6e, 0x63, 0x6c, 0x78, 0x0,  0x1,  0x0,  0xd,  0x0,  0x6,  0x80, 0x0,
      0x0,  0x0,  0x17, 0x69, 0x70, 0x6d, 0x61, 0x0,  0x0,  0x0,  0x0,  0x0,
      0x0,  0x0,  0x1,  0x0,  0x1,  0x4,  0x1,  0x2,  0x83, 0x4,  0x0,  0x0,
      0x0,  0x1f, 0x6d, 0x64, 0x61, 0x74, 0x12, 0x0,  0xa,  0x7,  0x38, 0x0,
      0x6,  0x10, 0x10, 0xd0, 0x69, 0x32, 0xa,  0x1f, 0xf0, 0x3f, 0xff, 0xff,
      0xc4, 0x0,  0x0,  0xaf, 0x70};
}

//------------------------------------------------------------------------------
// Environment setup

namespace {
class Environment : public ::testing::Environment {
 public:
  Environment(const char* name, const char* value)
      : name_(name), value_(value) {}
  void SetUp() override { setenv(name_, value_, 1); }

 private:
  const char* name_;
  const char* value_;
};
}  // namespace

::testing::Environment* SetEnv(const char* name, const char* value) {
  return ::testing::AddGlobalTestEnvironment(new Environment(name, value));
}

//------------------------------------------------------------------------------

std::vector<std::string> GetSeedDataDirs() {
  const char* var = std::getenv("TEST_DATA_DIRS");
  std::vector<std::string> res;
  if (var == nullptr || *var == 0) return res;
  const char* var_start = var;
  while (true) {
    if (*var == 0 || *var == ';') {
      res.push_back(std::string(var_start, var - var_start));
      if (*var == 0) break;
      var_start = var + 1;
    }
    ++var;
  }
  return res;
}

std::vector<std::string> GetTestImagesContents(
    size_t max_file_size, const std::vector<avifAppFileFormat>& image_formats) {
  // Use an environment variable to get the test data directory because
  // fuzztest seeds are created before the main() function is called, so the
  // test has no chance to parse command line arguments.
  const std::vector<std::string> test_data_dirs = GetSeedDataDirs();
  if (test_data_dirs.empty()) {
    // Only a warning because this can happen when running the binary with
    // --list_fuzz_tests (such as with gtest_discover_tests() in cmake).
    std::cerr << "WARNING: TEST_DATA_DIRS env variable not set, unable to read "
                 "seed files";
    return {};
  }

  std::vector<std::string> seeds;
  for (const std::string& test_data_dir : test_data_dirs) {
    std::cout << "Reading seeds from " << test_data_dir
              << " (non recursively)\n";
    auto tuple_vector = fuzztest::ReadFilesFromDirectory(test_data_dir);
    seeds.reserve(tuple_vector.size());
    for (auto& [file_content] : tuple_vector) {
      if (file_content.size() > max_file_size) continue;
      if (!image_formats.empty()) {
        const avifAppFileFormat format = avifGuessBufferFileFormat(
            reinterpret_cast<const uint8_t*>(file_content.data()),
            file_content.size());
        if (std::find(image_formats.begin(), image_formats.end(), format) ==
            image_formats.end()) {
          continue;
        }
      }

      seeds.push_back(std::move(file_content));
    }
  }
  if (seeds.empty()) {
    std::cerr << "ERROR: no files found that match the given file size and "
                 "format criteria\n";
    std::abort();
  }
  std::cout << "Returning " << seeds.size() << " seed images\n";
  return seeds;
}

//------------------------------------------------------------------------------

}  // namespace testutil
}  // namespace avif
