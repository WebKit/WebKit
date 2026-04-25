/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/parameter/ssn_tsn_reset_request_parameter.h"

#include <cstdint>
#include <vector>

#include "net/dcsctp/common/internal_types.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "test/gtest.h"

namespace dcsctp {
namespace {

TEST(SSNTSNResetRequestParameterTest, SerializeAndDeserialize) {
  SSNTSNResetRequestParameter parameter(ReconfigRequestSN(1));

  std::vector<uint8_t> serialized;
  parameter.SerializeTo(serialized);

  ASSERT_HAS_VALUE_AND_ASSIGN(SSNTSNResetRequestParameter deserialized,
                              SSNTSNResetRequestParameter::Parse(serialized));

  EXPECT_EQ(*deserialized.request_sequence_number(), 1u);
}

}  // namespace
}  // namespace dcsctp
