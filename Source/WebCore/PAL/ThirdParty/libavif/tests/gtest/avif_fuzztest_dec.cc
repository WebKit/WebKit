// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause
// Decodes an arbitrary sequence of bytes.

#include <cstdint>

#include "avif/avif.h"
#include "avif_fuzztest_helpers.h"
#include "aviftest_helpers.h"
#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

using ::fuzztest::Arbitrary;

namespace avif {
namespace testutil {
namespace {

::testing::Environment* const kStackLimitEnv = SetStackLimitTo512x1024Bytes();

//------------------------------------------------------------------------------

void Decode(const std::string& arbitrary_bytes, DecoderPtr decoder) {
  ASSERT_FALSE(GetSeedDataDirs().empty());  // Make sure seeds are available.

  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  const avifResult result = avifDecoderReadMemory(
      decoder.get(), decoded.get(),
      reinterpret_cast<const uint8_t*>(arbitrary_bytes.data()),
      arbitrary_bytes.size());
  if (result == AVIF_RESULT_OK) {
    EXPECT_GT(decoded->width, 0u);
    EXPECT_GT(decoded->height, 0u);
  }
}

FUZZ_TEST(DecodeAvifTest, Decode)
    .WithDomains(ArbitraryImageWithSeeds({AVIF_APP_FILE_FORMAT_AVIF}),
                 ArbitraryAvifDecoder());

//------------------------------------------------------------------------------

}  // namespace
}  // namespace testutil
}  // namespace avif
