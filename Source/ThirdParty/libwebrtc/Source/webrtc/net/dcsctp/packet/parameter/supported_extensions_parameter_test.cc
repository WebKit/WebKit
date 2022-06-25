/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/parameter/supported_extensions_parameter.h"

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

TEST(SupportedExtensionsParameterTest, SerializeAndDeserialize) {
  SupportedExtensionsParameter parameter({1, 2, 3});

  std::vector<uint8_t> serialized;
  parameter.SerializeTo(serialized);

  ASSERT_HAS_VALUE_AND_ASSIGN(SupportedExtensionsParameter deserialized,
                              SupportedExtensionsParameter::Parse(serialized));

  EXPECT_THAT(deserialized.chunk_types(), ElementsAre(1, 2, 3));
  EXPECT_TRUE(deserialized.supports(1));
  EXPECT_TRUE(deserialized.supports(2));
  EXPECT_TRUE(deserialized.supports(3));
  EXPECT_FALSE(deserialized.supports(4));
}

}  // namespace
}  // namespace dcsctp
