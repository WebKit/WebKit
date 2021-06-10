/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/parameter/outgoing_ssn_reset_request_parameter.h"

#include <cstdint>
#include <type_traits>
#include <vector>

#include "api/array_view.h"
#include "net/dcsctp/common/internal_types.h"
#include "net/dcsctp/public/types.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::ElementsAre;

TEST(OutgoingSSNResetRequestParameterTest, SerializeAndDeserialize) {
  OutgoingSSNResetRequestParameter parameter(
      ReconfigRequestSN(1), ReconfigRequestSN(2), TSN(3),
      {StreamID(4), StreamID(5), StreamID(6)});

  std::vector<uint8_t> serialized;
  parameter.SerializeTo(serialized);

  ASSERT_HAS_VALUE_AND_ASSIGN(
      OutgoingSSNResetRequestParameter deserialized,
      OutgoingSSNResetRequestParameter::Parse(serialized));

  EXPECT_EQ(*deserialized.request_sequence_number(), 1u);
  EXPECT_EQ(*deserialized.response_sequence_number(), 2u);
  EXPECT_EQ(*deserialized.sender_last_assigned_tsn(), 3u);
  EXPECT_THAT(deserialized.stream_ids(),
              ElementsAre(StreamID(4), StreamID(5), StreamID(6)));
}

}  // namespace
}  // namespace dcsctp
