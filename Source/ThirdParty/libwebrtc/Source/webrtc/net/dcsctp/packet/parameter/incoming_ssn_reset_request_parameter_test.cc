/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/parameter/incoming_ssn_reset_request_parameter.h"

#include <cstdint>
#include <type_traits>
#include <vector>

#include "api/array_view.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::ElementsAre;

TEST(IncomingSSNResetRequestParameterTest, SerializeAndDeserialize) {
  IncomingSSNResetRequestParameter parameter(
      ReconfigRequestSN(1), {StreamID(2), StreamID(3), StreamID(4)});

  std::vector<uint8_t> serialized;
  parameter.SerializeTo(serialized);

  ASSERT_HAS_VALUE_AND_ASSIGN(
      IncomingSSNResetRequestParameter deserialized,
      IncomingSSNResetRequestParameter::Parse(serialized));

  EXPECT_EQ(*deserialized.request_sequence_number(), 1u);
  EXPECT_THAT(deserialized.stream_ids(),
              ElementsAre(StreamID(2), StreamID(3), StreamID(4)));
}

}  // namespace
}  // namespace dcsctp
