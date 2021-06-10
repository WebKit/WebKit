/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/parameter/parameter.h"

#include <cstdint>
#include <type_traits>
#include <vector>

#include "api/array_view.h"
#include "net/dcsctp/packet/parameter/outgoing_ssn_reset_request_parameter.h"
#include "net/dcsctp/packet/tlv_trait.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::ElementsAre;
using ::testing::SizeIs;

TEST(ParameterTest, SerializeDeserializeParameter) {
  Parameters parameters =
      Parameters::Builder()
          .Add(OutgoingSSNResetRequestParameter(ReconfigRequestSN(123),
                                                ReconfigRequestSN(456),
                                                TSN(789), {StreamID(42)}))
          .Build();

  rtc::ArrayView<const uint8_t> serialized = parameters.data();

  ASSERT_HAS_VALUE_AND_ASSIGN(Parameters parsed, Parameters::Parse(serialized));
  auto descriptors = parsed.descriptors();
  ASSERT_THAT(descriptors, SizeIs(1));
  EXPECT_THAT(descriptors[0].type, OutgoingSSNResetRequestParameter::kType);

  ASSERT_HAS_VALUE_AND_ASSIGN(
      OutgoingSSNResetRequestParameter parsed_param,
      OutgoingSSNResetRequestParameter::Parse(descriptors[0].data));
  EXPECT_EQ(*parsed_param.request_sequence_number(), 123u);
  EXPECT_EQ(*parsed_param.response_sequence_number(), 456u);
  EXPECT_EQ(*parsed_param.sender_last_assigned_tsn(), 789u);
  EXPECT_THAT(parsed_param.stream_ids(), ElementsAre(StreamID(42)));
}

}  // namespace
}  // namespace dcsctp
