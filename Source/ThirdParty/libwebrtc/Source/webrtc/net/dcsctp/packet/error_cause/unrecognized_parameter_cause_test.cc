/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/error_cause/unrecognized_parameter_cause.h"

#include <stdint.h>

#include <type_traits>
#include <vector>

#include "api/array_view.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::ElementsAre;

TEST(UnrecognizedParametersCauseTest, SerializeAndDeserialize) {
  uint8_t unrecognized_parameters[] = {1, 2, 3};
  UnrecognizedParametersCause parameter(unrecognized_parameters);

  std::vector<uint8_t> serialized;
  parameter.SerializeTo(serialized);

  ASSERT_HAS_VALUE_AND_ASSIGN(UnrecognizedParametersCause deserialized,
                              UnrecognizedParametersCause::Parse(serialized));

  EXPECT_THAT(deserialized.unrecognized_parameters(), ElementsAre(1, 2, 3));
}
}  // namespace
}  // namespace dcsctp
