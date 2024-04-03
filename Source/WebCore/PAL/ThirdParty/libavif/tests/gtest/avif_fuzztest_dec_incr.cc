// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause
// Compare non-incremental and incremental decode results of an arbitrary byte
// sequence.

#include <cstdint>
#include <string>

#include "avif/avif.h"
#include "avif_fuzztest_helpers.h"
#include "avifincrtest_helpers.h"
#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

using ::fuzztest::Arbitrary;

namespace avif {
namespace testutil {
namespace {

::testing::Environment* const kStackLimitEnv = SetStackLimitTo512x1024Bytes();

//------------------------------------------------------------------------------

struct DecoderInput {
  const uint8_t* available_bytes;
  size_t available_size;
  size_t read_size;
};

// A custom reader is necessary to get the number of bytes read by libavif.
// See avifIOReadFunc() documentation.
avifResult AvifIoRead(struct avifIO* io, uint32_t read_flags, uint64_t offset,
                      size_t size, avifROData* out) {
  DecoderInput* data = reinterpret_cast<DecoderInput*>(io->data);
  if (read_flags != 0 || !data || data->available_size < offset) {
    return AVIF_RESULT_IO_ERROR;
  }
  out->data = data->available_bytes + offset;
  out->size = std::min(size, data->available_size - offset);
  data->read_size = std::max(data->read_size, offset + out->size);
  return AVIF_RESULT_OK;
}

//------------------------------------------------------------------------------

void DecodeIncr(const std::string& arbitrary_bytes, bool is_persistent,
                bool give_size_hint, bool use_nth_image_api) {
  ASSERT_FALSE(GetSeedDataDirs().empty());  // Make sure seeds are available.

  ImagePtr reference(avifImageCreateEmpty());
  ASSERT_NE(reference.get(), nullptr);

  DecoderInput data = {reinterpret_cast<const uint8_t*>(arbitrary_bytes.data()),
                       arbitrary_bytes.size(), 0};
  avifIO io = {.read = AvifIoRead,
               .sizeHint = arbitrary_bytes.size(),
               .persistent = AVIF_TRUE,
               .data = &data};

  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder.get(), nullptr);
  avifDecoderSetIO(decoder.get(), &io);
  // OSS-Fuzz limits the allocated memory to 2560 MB.
  // avifDecoderParse returns AVIF_RESULT_NOT_IMPLEMENTED if kImageSizeLimit is
  // bigger than AVIF_DEFAULT_IMAGE_SIZE_LIMIT.
  constexpr uint32_t kImageSizeLimit =
      2560u * 512 * 512 / AVIF_MAX_AV1_LAYER_COUNT / sizeof(uint16_t);
  static_assert(kImageSizeLimit <= AVIF_DEFAULT_IMAGE_SIZE_LIMIT,
                "Too big an image size limit");
  decoder->imageSizeLimit = kImageSizeLimit;

  if (avifDecoderRead(decoder.get(), reference.get()) == AVIF_RESULT_OK) {
    // Avoid timeouts by discarding big images decoded many times.
    // TODO(yguyon): Increase this arbitrary threshold but decode incrementally
    //               fewer times than as many bytes.
    if (reference->width * reference->height * data.read_size >
        8 * 1024 * 1024) {
      return;
    }
    // decodeIncrementally() will fail if there are leftover bytes.
    const avifRWData encoded_data = {const_cast<uint8_t*>(data.available_bytes),
                                     data.read_size};
    // No clue on whether encoded_data is tiled so use a lower bound of a single
    // tile for the whole image.
    // Note that an AVIF tile is at most as high as an AV1 frame
    // (aomediacodec.github.io/av1-spec says max_frame_height_minus_1 < 65536)
    // but libavif successfully decodes AVIF files with dimensions unrelated to
    // the underlying AV1 frame (for example a 1x1000000 AVIF for a 1x1 AV1).
    // Otherwise we could use the minimum of reference->height and 65536u below.
    const uint32_t max_cell_height = reference->height;
    const avifResult result = DecodeIncrementally(
        encoded_data, decoder.get(), is_persistent, give_size_hint,
        use_nth_image_api, *reference, max_cell_height);
    // The result does not matter, as long as we do not crash.
    (void)result;
  }
}

FUZZ_TEST(DecodeAvifFuzzTest, DecodeIncr)
    .WithDomains(ArbitraryImageWithSeeds({AVIF_APP_FILE_FORMAT_AVIF}),
                 Arbitrary<bool>(), Arbitrary<bool>(), Arbitrary<bool>());

//------------------------------------------------------------------------------

}  // namespace
}  // namespace testutil
}  // namespace avif
