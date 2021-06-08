/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/tx/retransmission_error_counter.h"

#include "net/dcsctp/public/dcsctp_options.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {

TEST(RetransmissionErrorCounterTest, HasInitialValue) {
  DcSctpOptions options;
  RetransmissionErrorCounter counter("log: ", options);
  EXPECT_EQ(counter.value(), 0);
}

TEST(RetransmissionErrorCounterTest, ReturnsFalseAtMaximumValue) {
  DcSctpOptions options;
  options.max_retransmissions = 5;
  RetransmissionErrorCounter counter("log: ", options);
  EXPECT_TRUE(counter.Increment("test"));   // 1
  EXPECT_TRUE(counter.Increment("test"));   // 2
  EXPECT_TRUE(counter.Increment("test"));   // 3
  EXPECT_TRUE(counter.Increment("test"));   // 4
  EXPECT_TRUE(counter.Increment("test"));   // 5
  EXPECT_FALSE(counter.Increment("test"));  // Too many retransmissions
}

TEST(RetransmissionErrorCounterTest, CanHandleZeroRetransmission) {
  DcSctpOptions options;
  options.max_retransmissions = 0;
  RetransmissionErrorCounter counter("log: ", options);
  EXPECT_FALSE(counter.Increment("test"));  // One is too many.
}

TEST(RetransmissionErrorCounterTest, IsExhaustedAtMaximum) {
  DcSctpOptions options;
  options.max_retransmissions = 3;
  RetransmissionErrorCounter counter("log: ", options);
  EXPECT_TRUE(counter.Increment("test"));  // 1
  EXPECT_FALSE(counter.IsExhausted());
  EXPECT_TRUE(counter.Increment("test"));  // 2
  EXPECT_FALSE(counter.IsExhausted());
  EXPECT_TRUE(counter.Increment("test"));  // 3
  EXPECT_FALSE(counter.IsExhausted());
  EXPECT_FALSE(counter.Increment("test"));  // Too many retransmissions
  EXPECT_TRUE(counter.IsExhausted());
  EXPECT_FALSE(counter.Increment("test"));  // One after too many
  EXPECT_TRUE(counter.IsExhausted());
}

TEST(RetransmissionErrorCounterTest, ClearingCounter) {
  DcSctpOptions options;
  options.max_retransmissions = 3;
  RetransmissionErrorCounter counter("log: ", options);
  EXPECT_TRUE(counter.Increment("test"));  // 1
  EXPECT_TRUE(counter.Increment("test"));  // 2
  counter.Clear();
  EXPECT_TRUE(counter.Increment("test"));  // 1
  EXPECT_TRUE(counter.Increment("test"));  // 2
  EXPECT_TRUE(counter.Increment("test"));  // 3
  EXPECT_FALSE(counter.IsExhausted());
  EXPECT_FALSE(counter.Increment("test"));  // Too many retransmissions
  EXPECT_TRUE(counter.IsExhausted());
}

}  // namespace
}  // namespace dcsctp
