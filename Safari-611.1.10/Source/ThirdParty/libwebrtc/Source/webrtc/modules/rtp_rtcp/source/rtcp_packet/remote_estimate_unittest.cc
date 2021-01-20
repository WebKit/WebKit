/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/rtp_rtcp/source/rtcp_packet/remote_estimate.h"

#include "test/gtest.h"

namespace webrtc {
namespace rtcp {
TEST(RemoteEstimateTest, EncodesCapacityBounds) {
  NetworkStateEstimate src;
  src.link_capacity_lower = DataRate::KilobitsPerSec(10);
  src.link_capacity_upper = DataRate::KilobitsPerSec(1000000);
  rtc::Buffer data = GetRemoteEstimateSerializer()->Serialize(src);
  NetworkStateEstimate dst;
  EXPECT_TRUE(GetRemoteEstimateSerializer()->Parse(data, &dst));
  EXPECT_EQ(src.link_capacity_lower, dst.link_capacity_lower);
  EXPECT_EQ(src.link_capacity_upper, dst.link_capacity_upper);
}

TEST(RemoteEstimateTest, ExpandsToPlusInfinity) {
  NetworkStateEstimate src;
  // White box testing: We know that the value is stored in an unsigned 24 int
  // with kbps resolution. We expected it be represented as plus infinity.
  src.link_capacity_lower = DataRate::KilobitsPerSec(2 << 24);
  src.link_capacity_upper = DataRate::PlusInfinity();
  rtc::Buffer data = GetRemoteEstimateSerializer()->Serialize(src);

  NetworkStateEstimate dst;
  EXPECT_TRUE(GetRemoteEstimateSerializer()->Parse(data, &dst));
  EXPECT_TRUE(dst.link_capacity_lower.IsPlusInfinity());
  EXPECT_TRUE(dst.link_capacity_upper.IsPlusInfinity());
}

TEST(RemoteEstimateTest, DoesNotEncodeNegative) {
  NetworkStateEstimate src;
  src.link_capacity_lower = DataRate::MinusInfinity();
  src.link_capacity_upper = DataRate::MinusInfinity();
  rtc::Buffer data = GetRemoteEstimateSerializer()->Serialize(src);
  // Since MinusInfinity can't be represented, the buffer should be empty.
  EXPECT_EQ(data.size(), 0u);
  NetworkStateEstimate dst;
  dst.link_capacity_lower = DataRate::KilobitsPerSec(300);
  EXPECT_TRUE(GetRemoteEstimateSerializer()->Parse(data, &dst));
  // The fields will be left unchanged by the parser as they were not encoded.
  EXPECT_EQ(dst.link_capacity_lower, DataRate::KilobitsPerSec(300));
  EXPECT_TRUE(dst.link_capacity_upper.IsMinusInfinity());
}
}  // namespace rtcp
}  // namespace webrtc
