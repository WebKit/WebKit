/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/parameter/zero_checksum_acceptable_chunk_parameter.h"

#include <stdint.h>

#include <type_traits>
#include <vector>

#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace dcsctp {
namespace {
using ::testing::ElementsAre;

TEST(ZeroChecksumAcceptableChunkParameterTest, SerializeAndDeserialize) {
  ZeroChecksumAcceptableChunkParameter parameter(
      ZeroChecksumAlternateErrorDetectionMethod::LowerLayerDtls());

  std::vector<uint8_t> serialized;
  parameter.SerializeTo(serialized);

  EXPECT_THAT(serialized,
              ElementsAre(0x80, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00, 0x01));

  ASSERT_HAS_VALUE_AND_ASSIGN(
      ZeroChecksumAcceptableChunkParameter deserialized,
      ZeroChecksumAcceptableChunkParameter::Parse(serialized));
}

TEST(ZeroChecksumAcceptableChunkParameterTest, FailToDeserializePrevVersion) {
  // This is how the draft described the chunk as, in version 00.
  std::vector<uint8_t> invalid = {0x80, 0x01, 0x00, 0x04};

  EXPECT_FALSE(
      ZeroChecksumAcceptableChunkParameter::Parse(invalid).has_value());
}

TEST(ZeroChecksumAcceptableChunkParameterTest, FailToDeserialize) {
  std::vector<uint8_t> invalid = {0x00, 0x00, 0x00, 0x00};

  EXPECT_FALSE(
      ZeroChecksumAcceptableChunkParameter::Parse(invalid).has_value());
}

TEST(ZeroChecksumAcceptableChunkParameterTest, HasToString) {
  ZeroChecksumAcceptableChunkParameter parameter(
      ZeroChecksumAlternateErrorDetectionMethod::LowerLayerDtls());

  EXPECT_EQ(parameter.ToString(), "Zero Checksum Acceptable (1)");
}

}  // namespace
}  // namespace dcsctp
