/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/parameter/reconfiguration_response_parameter.h"

#include <stdint.h>

#include <type_traits>
#include <vector>

#include "absl/types/optional.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {

TEST(ReconfigurationResponseParameterTest, SerializeAndDeserializeFirstForm) {
  ReconfigurationResponseParameter parameter(
      ReconfigRequestSN(1),
      ReconfigurationResponseParameter::Result::kSuccessPerformed);

  std::vector<uint8_t> serialized;
  parameter.SerializeTo(serialized);

  ASSERT_HAS_VALUE_AND_ASSIGN(
      ReconfigurationResponseParameter deserialized,
      ReconfigurationResponseParameter::Parse(serialized));

  EXPECT_EQ(*deserialized.response_sequence_number(), 1u);
  EXPECT_EQ(deserialized.result(),
            ReconfigurationResponseParameter::Result::kSuccessPerformed);
  EXPECT_EQ(deserialized.sender_next_tsn(), absl::nullopt);
  EXPECT_EQ(deserialized.receiver_next_tsn(), absl::nullopt);
}

TEST(ReconfigurationResponseParameterTest,
     SerializeAndDeserializeFirstFormSecondForm) {
  ReconfigurationResponseParameter parameter(
      ReconfigRequestSN(1),
      ReconfigurationResponseParameter::Result::kSuccessPerformed, TSN(2),
      TSN(3));

  std::vector<uint8_t> serialized;
  parameter.SerializeTo(serialized);

  ASSERT_HAS_VALUE_AND_ASSIGN(
      ReconfigurationResponseParameter deserialized,
      ReconfigurationResponseParameter::Parse(serialized));

  EXPECT_EQ(*deserialized.response_sequence_number(), 1u);
  EXPECT_EQ(deserialized.result(),
            ReconfigurationResponseParameter::Result::kSuccessPerformed);
  EXPECT_TRUE(deserialized.sender_next_tsn().has_value());
  EXPECT_EQ(**deserialized.sender_next_tsn(), 2u);
  EXPECT_TRUE(deserialized.receiver_next_tsn().has_value());
  EXPECT_EQ(**deserialized.receiver_next_tsn(), 3u);
}

}  // namespace
}  // namespace dcsctp
