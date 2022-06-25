/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/error_cause/restart_of_an_association_with_new_address_cause.h"

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

TEST(RestartOfAnAssociationWithNewAddressesCauseTest, SerializeAndDeserialize) {
  uint8_t data[] = {1, 2, 3};
  RestartOfAnAssociationWithNewAddressesCause parameter(data);

  std::vector<uint8_t> serialized;
  parameter.SerializeTo(serialized);

  ASSERT_HAS_VALUE_AND_ASSIGN(
      RestartOfAnAssociationWithNewAddressesCause deserialized,
      RestartOfAnAssociationWithNewAddressesCause::Parse(serialized));

  EXPECT_THAT(deserialized.new_address_tlvs(), ElementsAre(1, 2, 3));
}

}  // namespace
}  // namespace dcsctp
