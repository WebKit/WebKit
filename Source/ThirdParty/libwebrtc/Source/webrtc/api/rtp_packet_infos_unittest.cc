/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtp_packet_infos.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::SizeIs;

template <typename Iterator>
RtpPacketInfos::vector_type ToVector(Iterator begin, Iterator end) {
  return RtpPacketInfos::vector_type(begin, end);
}

}  // namespace

TEST(RtpPacketInfosTest, BasicFunctionality) {
  RtpPacketInfo p0(123, {1, 2}, 89, 5, AbsoluteCaptureTime{45, 78},
                   Timestamp::Millis(7));
  RtpPacketInfo p1(456, {3, 4}, 89, 4, AbsoluteCaptureTime{13, 21},
                   Timestamp::Millis(1));
  RtpPacketInfo p2(789, {5, 6}, 88, 1, AbsoluteCaptureTime{99, 78},
                   Timestamp::Millis(7));

  RtpPacketInfos x({p0, p1, p2});

  ASSERT_THAT(x, SizeIs(3));

  EXPECT_EQ(x[0], p0);
  EXPECT_EQ(x[1], p1);
  EXPECT_EQ(x[2], p2);

  EXPECT_EQ(x.front(), p0);
  EXPECT_EQ(x.back(), p2);

  EXPECT_THAT(ToVector(x.begin(), x.end()), ElementsAre(p0, p1, p2));
  EXPECT_THAT(ToVector(x.rbegin(), x.rend()), ElementsAre(p2, p1, p0));

  EXPECT_THAT(ToVector(x.cbegin(), x.cend()), ElementsAre(p0, p1, p2));
  EXPECT_THAT(ToVector(x.crbegin(), x.crend()), ElementsAre(p2, p1, p0));

  EXPECT_FALSE(x.empty());
}

TEST(RtpPacketInfosTest, CopyShareData) {
  RtpPacketInfo p0(123, {1, 2}, 89, 5, AbsoluteCaptureTime{45, 78},
                   Timestamp::Millis(7));
  RtpPacketInfo p1(456, {3, 4}, 89, 4, AbsoluteCaptureTime{13, 21},
                   Timestamp::Millis(1));
  RtpPacketInfo p2(789, {5, 6}, 88, 1, AbsoluteCaptureTime{99, 78},
                   Timestamp::Millis(7));

  RtpPacketInfos lhs({p0, p1, p2});
  RtpPacketInfos rhs = lhs;

  ASSERT_THAT(lhs, SizeIs(3));
  ASSERT_THAT(rhs, SizeIs(3));

  for (size_t i = 0; i < lhs.size(); ++i) {
    EXPECT_EQ(lhs[i], rhs[i]);
  }

  EXPECT_EQ(lhs.front(), rhs.front());
  EXPECT_EQ(lhs.back(), rhs.back());

  EXPECT_EQ(lhs.begin(), rhs.begin());
  EXPECT_EQ(lhs.end(), rhs.end());
  EXPECT_EQ(lhs.rbegin(), rhs.rbegin());
  EXPECT_EQ(lhs.rend(), rhs.rend());

  EXPECT_EQ(lhs.cbegin(), rhs.cbegin());
  EXPECT_EQ(lhs.cend(), rhs.cend());
  EXPECT_EQ(lhs.crbegin(), rhs.crbegin());
  EXPECT_EQ(lhs.crend(), rhs.crend());

  EXPECT_EQ(lhs.empty(), rhs.empty());
}

}  // namespace webrtc
