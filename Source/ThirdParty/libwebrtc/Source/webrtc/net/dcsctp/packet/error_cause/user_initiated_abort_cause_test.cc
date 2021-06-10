/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/error_cause/user_initiated_abort_cause.h"

#include <stdint.h>

#include <type_traits>
#include <vector>

#include "api/array_view.h"
#include "net/dcsctp/packet/error_cause/error_cause.h"
#include "net/dcsctp/packet/tlv_trait.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::SizeIs;

TEST(UserInitiatedAbortCauseTest, EmptyReason) {
  Parameters causes =
      Parameters::Builder().Add(UserInitiatedAbortCause("")).Build();

  ASSERT_HAS_VALUE_AND_ASSIGN(Parameters deserialized,
                              Parameters::Parse(causes.data()));
  ASSERT_THAT(deserialized.descriptors(), SizeIs(1));
  EXPECT_EQ(deserialized.descriptors()[0].type, UserInitiatedAbortCause::kType);

  ASSERT_HAS_VALUE_AND_ASSIGN(
      UserInitiatedAbortCause cause,
      UserInitiatedAbortCause::Parse(deserialized.descriptors()[0].data));

  EXPECT_EQ(cause.upper_layer_abort_reason(), "");
}

TEST(UserInitiatedAbortCauseTest, SetReason) {
  Parameters causes = Parameters::Builder()
                          .Add(UserInitiatedAbortCause("User called Close"))
                          .Build();

  ASSERT_HAS_VALUE_AND_ASSIGN(Parameters deserialized,
                              Parameters::Parse(causes.data()));
  ASSERT_THAT(deserialized.descriptors(), SizeIs(1));
  EXPECT_EQ(deserialized.descriptors()[0].type, UserInitiatedAbortCause::kType);

  ASSERT_HAS_VALUE_AND_ASSIGN(
      UserInitiatedAbortCause cause,
      UserInitiatedAbortCause::Parse(deserialized.descriptors()[0].data));

  EXPECT_EQ(cause.upper_layer_abort_reason(), "User called Close");
}

}  // namespace
}  // namespace dcsctp
