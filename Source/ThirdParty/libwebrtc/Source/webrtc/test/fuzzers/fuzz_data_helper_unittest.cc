/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/fuzzers/fuzz_data_helper.h"

#include <cstdint>

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc::test {
namespace {

using ::testing::Each;
using ::testing::ElementsAre;

TEST(FuzzDataHelperTest, ReadsIntegers) {
  const uint8_t kData[] = {0xFE, 0x01, 0x02, 0x03, 0x04, 0x05};
  FuzzDataHelper fuzz_data(kData);
  ASSERT_EQ(fuzz_data.BytesLeft(), 6u);

  EXPECT_EQ(fuzz_data.Read<int8_t>(), -2);
  EXPECT_EQ(fuzz_data.BytesLeft(), 5u);

  EXPECT_EQ(fuzz_data.Read<uint8_t>(), 0x01);
  EXPECT_EQ(fuzz_data.BytesLeft(), 4u);

  EXPECT_EQ(fuzz_data.Read<uint16_t>(), 0x0302);
  EXPECT_EQ(fuzz_data.BytesLeft(), 2u);

  // Read 4 bytes while only two left in `kData` - expect that succeed and
  // returns predictable results (fills missing data with zeros).
  EXPECT_EQ(fuzz_data.Read<uint32_t>(), 0x0000'0504u);
  EXPECT_EQ(fuzz_data.BytesLeft(), 0u);
}

struct PlainOldData {
  uint32_t value;
  uint8_t array[3];
};

TEST(FuzzDataHelperTest, ReadsAnyType) {
  const uint8_t kData[] = {0x01, 0x01, 0x02, 0x03, 0x04, 0x05};
  FuzzDataHelper fuzz_data(kData);

  PlainOldData pod = fuzz_data.Read<PlainOldData>();
  EXPECT_EQ(pod.value, 0x03020101u);
  EXPECT_THAT(pod.array, ElementsAre(0x04, 0x05, 0));
}

TEST(FuzzDataHelperTest, ReadsZerosWhenNoDataLeft) {
  const uint8_t kData[] = {0xFE};
  FuzzDataHelper fuzz_data(kData);
  ASSERT_EQ(fuzz_data.BytesLeft(), 1u);
  fuzz_data.Read<uint8_t>();

  ASSERT_EQ(fuzz_data.BytesLeft(), 0u);

  EXPECT_EQ(fuzz_data.Read<int8_t>(), 0);
  EXPECT_EQ(fuzz_data.Read<uint8_t>(), 0u);
  EXPECT_EQ(fuzz_data.Read<uint16_t>(), 0u);
  PlainOldData pod = fuzz_data.Read<PlainOldData>();
  EXPECT_EQ(pod.value, 0u);
  EXPECT_THAT(pod.array, Each(0u));
}

}  // namespace
}  // namespace webrtc::test
