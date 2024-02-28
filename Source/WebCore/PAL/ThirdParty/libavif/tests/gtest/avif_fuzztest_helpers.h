// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#ifndef LIBAVIF_TESTS_OSS_FUZZ_AVIF_FUZZTEST_HELPERS_H_
#define LIBAVIF_TESTS_OSS_FUZZ_AVIF_FUZZTEST_HELPERS_H_

#include <cstdint>
#include <cstdlib>
#include <vector>

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "avifutil.h"
#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

namespace avif {
namespace testutil {

//------------------------------------------------------------------------------
// C++ wrapper for scoped memory management of C API objects.

// Exposed for convenient fuzztest reproducer output.
ImagePtr CreateAvifImage8b(size_t width, size_t height,
                           avifPixelFormat pixel_format, bool has_alpha,
                           const std::vector<uint8_t>& samples);
ImagePtr CreateAvifImage16b(size_t width, size_t height, int depth,
                            avifPixelFormat pixel_format, bool has_alpha,
                            const std::vector<uint16_t>& samples);
EncoderPtr CreateAvifEncoder(avifCodecChoice codec_choice, int max_threads,
                             int min_quantizer, int max_quantizer,
                             int min_quantizer_alpha, int max_quantizer_alpha,
                             int tile_rows_log2, int tile_cols_log2, int speed);
DecoderPtr CreateAvifDecoder(avifCodecChoice codec_choice, int max_threads,
                             avifDecoderSource requested_source,
                             bool allow_progressive, bool allow_incremental,
                             bool ignore_exif, bool ignore_xmp,
                             uint32_t image_size_limit,
                             uint32_t image_dimension_limit,
                             uint32_t image_count_limit,
                             avifStrictFlags strict_flags);
#if defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
DecoderPtr AddGainMapOptionsToDecoder(DecoderPtr decoder,
                                      bool enable_parsing_gain_map_metadata,
                                      bool enable_decoding_gain_map);
#endif

//------------------------------------------------------------------------------
// Custom fuzztest generators.
// See https://github.com/google/fuzztest/blob/main/doc/domains-reference.md.

// Do not generate images wider or taller than this.
inline constexpr size_t kMaxDimension = 512;  // In pixels.

size_t GetNumSamples(size_t width, size_t height, avifPixelFormat pixel_format,
                     bool has_alpha);

// To avoid using fuzztest::internal, the return type of the functions below is
// auto.

inline auto ArbitraryPixelFormat() {
  return fuzztest::ElementOf<avifPixelFormat>(
      {AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV422,
       AVIF_PIXEL_FORMAT_YUV420, AVIF_PIXEL_FORMAT_YUV400});
}

// avifImage generator type: Width, height, pixel format and 8-bit samples.
inline auto ArbitraryAvifImage8b() {
  return fuzztest::FlatMap(
      [](size_t width, size_t height, avifPixelFormat pixel_format,
         bool has_alpha) {
        return fuzztest::Map(
            CreateAvifImage8b, fuzztest::Just(width), fuzztest::Just(height),
            fuzztest::Just(pixel_format), fuzztest::Just(has_alpha),
            fuzztest::Arbitrary<std::vector<uint8_t>>().WithSize(
                GetNumSamples(width, height, pixel_format, has_alpha)));
      },
      fuzztest::InRange<uint16_t>(1, kMaxDimension),
      fuzztest::InRange<uint16_t>(1, kMaxDimension), ArbitraryPixelFormat(),
      fuzztest::Arbitrary<bool>());
}

// avifImage generator type: Width, height, depth, pixel format and 16-bit
// samples.
inline auto ArbitraryAvifImage16b() {
  return fuzztest::FlatMap(
      [](size_t width, size_t height, int depth, avifPixelFormat pixel_format,
         bool has_alpha) {
        return fuzztest::Map(
            CreateAvifImage16b, fuzztest::Just(width), fuzztest::Just(height),
            fuzztest::Just(depth), fuzztest::Just(pixel_format),
            fuzztest::Just(has_alpha),
            fuzztest::ContainerOf<std::vector<uint16_t>>(
                fuzztest::InRange<uint16_t>(0, (1 << depth) - 1))
                .WithSize(
                    GetNumSamples(width, height, pixel_format, has_alpha)));
      },
      fuzztest::InRange<uint16_t>(1, kMaxDimension),
      fuzztest::InRange<uint16_t>(1, kMaxDimension),
      fuzztest::ElementOf({10, 12}), ArbitraryPixelFormat(),
      fuzztest::Arbitrary<bool>());
}

// Generator for an arbitrary ImagePtr.
inline auto ArbitraryAvifImage() {
  return fuzztest::OneOf(ArbitraryAvifImage8b(), ArbitraryAvifImage16b());
}

// Generator for an arbitrary EncoderPtr.
inline auto ArbitraryAvifEncoder() {
  const auto codec_choice = fuzztest::ElementOf<avifCodecChoice>(
      {AVIF_CODEC_CHOICE_AUTO, AVIF_CODEC_CHOICE_AOM});
  // MAX_NUM_THREADS from libaom/aom_util/aom_thread.h
  const auto max_threads = fuzztest::InRange(0, 64);
  const auto min_quantizer = fuzztest::InRange(AVIF_QUANTIZER_BEST_QUALITY,
                                               AVIF_QUANTIZER_WORST_QUALITY);
  const auto max_quantizer = fuzztest::InRange(AVIF_QUANTIZER_BEST_QUALITY,
                                               AVIF_QUANTIZER_WORST_QUALITY);
  const auto min_quantizer_alpha = fuzztest::InRange(
      AVIF_QUANTIZER_BEST_QUALITY, AVIF_QUANTIZER_WORST_QUALITY);
  const auto max_quantizer_alpha = fuzztest::InRange(
      AVIF_QUANTIZER_BEST_QUALITY, AVIF_QUANTIZER_WORST_QUALITY);
  const auto tile_rows_log2 = fuzztest::InRange(0, 6);
  const auto tile_cols_log2 = fuzztest::InRange(0, 6);
  // Fuzz only a small range of 'speed' values to avoid slowing down the fuzzer
  // too much. The main goal is to fuzz libavif, not the underlying AV1 encoder.
  const auto speed = fuzztest::InRange(6, AVIF_SPEED_FASTEST);
  return fuzztest::Map(CreateAvifEncoder, codec_choice, max_threads,
                       min_quantizer, max_quantizer, min_quantizer_alpha,
                       max_quantizer_alpha, tile_rows_log2, tile_cols_log2,
                       speed);
}

// Generator for an arbitrary EncoderPtr with base options fuzzed (i.e.
// without "experimental" options hidden behind compile flags).
inline auto ArbitraryBaseAvifDecoder() {
  // MAX_NUM_THREADS from libaom/aom_util/aom_thread.h
  const auto max_threads = fuzztest::InRange(0, 64);
  return fuzztest::Map(
      CreateAvifDecoder,
      fuzztest::ElementOf<avifCodecChoice>({AVIF_CODEC_CHOICE_AUTO,
                                            AVIF_CODEC_CHOICE_AOM,
                                            AVIF_CODEC_CHOICE_DAV1D}),
      max_threads,
      /*requested_source=*/
      fuzztest::ElementOf(
          {AVIF_DECODER_SOURCE_AUTO, AVIF_DECODER_SOURCE_PRIMARY_ITEM}),
      /*allow_progressive=*/fuzztest::Arbitrary<bool>(),
      /*allow_incremental=*/fuzztest::Arbitrary<bool>(),
      /*ignore_exif=*/fuzztest::Arbitrary<bool>(),
      /*ignore_xmp=*/fuzztest::Arbitrary<bool>(),
      /*image_size_limit=*/fuzztest::Just(kMaxDimension * kMaxDimension),
      /*image_dimension_limit=*/fuzztest::Just(kMaxDimension),
      /*image_count_limit=*/fuzztest::Just(10),
      /*strict_flags=*/
      fuzztest::BitFlagCombinationOf({AVIF_STRICT_PIXI_REQUIRED,
                                      AVIF_STRICT_CLAP_VALID,
                                      AVIF_STRICT_ALPHA_ISPE_REQUIRED}));
}

#if defined(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
// Generator for an arbitrary EncoderPtr with base options and gain map
// options fuzzed, with the exception of 'ignoreColorAndAlpha' (because it would
// break most tests' assumptions).
inline auto ArbitraryAvifDecoderWithGainMapOptions() {
  return fuzztest::Map(
      AddGainMapOptionsToDecoder, ArbitraryBaseAvifDecoder(),
      /*enable_parsing_gain_map_metadata=*/fuzztest::Arbitrary<bool>(),
      /*enable_decoding_gain_map=*/fuzztest::Arbitrary<bool>());
}

// Generator for an arbitrary DecoderPtr.
inline auto ArbitraryAvifDecoder() {
  return ArbitraryAvifDecoderWithGainMapOptions();
}
#else
// Generator for an arbitrary DecoderPtr.
inline auto ArbitraryAvifDecoder() { return ArbitraryBaseAvifDecoder(); }
#endif  // AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP

//------------------------------------------------------------------------------

// Returns a white pixel compressed with AVIF.
std::vector<uint8_t> GetWhiteSinglePixelAvif();

//------------------------------------------------------------------------------
// Environment setup

// Sets the environment variable 'name' to the 'value' during the setup step.
::testing::Environment* SetEnv(const char* name, const char* value);

inline ::testing::Environment* SetStackLimitTo512x1024Bytes() {
  return SetEnv("FUZZTEST_STACK_LIMIT", "524288");
}

//------------------------------------------------------------------------------

// Returns the paths contained in the 'TEST_DATA_DIRS' environment variable.
// Several paths can be set in the variable, separated by ';'.
// Returns nullptr if not set.
// Tests that use ArbitraryImageWithSeeds() should
// ASSERT_FALSE(GetSeedDataDirs().empty()) if they want to make sure that seeds
// are actually used.
std::vector<std::string> GetSeedDataDirs();

// Returns a list of test images contents (not paths) from the directory set in
// the 'TEST_DATA_DIRS' environment variable, that are smaller than
// 'max_file_size' and have one of the formats in 'image_formats' (or any format
// if 'image_formats' is empty).
// If TEST_DATA_DIRS is not set, returns an empty set.
// Tests that use this should ASSERT_FALSE(GetSeedDataDirs().empty())
// if they want to make sure that seeds are actually used.
// Terminates the program with abort() if TEST_DATA_DIRS is set but doesn't
// contain any matching images.
std::vector<std::string> GetTestImagesContents(
    size_t max_file_size, const std::vector<avifAppFileFormat>& image_formats);

// Generator for an arbitrary ImagePtr that uses test image files as seeds.
// Uses the 'TEST_DATA_DIRS' environment variable to load the seeds.
// If TEST_DATA_DIRS is not set, no seeds are used.
// Tests that use this should ASSERT_FALSE(GetSeedDataDirs().empty())
// if they want to make sure that seeds are actually used.
// Terminates the program with abort() if TEST_DATA_DIRS is set but doesn't
// contain any matching images.
inline auto ArbitraryImageWithSeeds(
    const std::vector<avifAppFileFormat>& image_formats) {
  constexpr uint32_t kMaxSeedFileSize = 1024 * 1024;  // 1MB.
  return fuzztest::Arbitrary<std::string>()
      .WithMaxSize(kMaxSeedFileSize)
      .WithSeeds(GetTestImagesContents(kMaxSeedFileSize, image_formats));
}

//------------------------------------------------------------------------------

}  // namespace testutil
}  // namespace avif

#endif  // LIBAVIF_TESTS_OSS_FUZZ_AVIF_FUZZTEST_HELPERS_H_
