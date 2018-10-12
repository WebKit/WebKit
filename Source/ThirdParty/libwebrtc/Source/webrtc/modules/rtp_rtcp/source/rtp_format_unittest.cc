/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_format.h"

#include <memory>
#include <numeric>

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::Le;
using ::testing::Gt;
using ::testing::Each;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;

// Calculate difference between largest and smallest packets respecting sizes
// adjustement provided by limits,
// i.e. last packet expected to be smaller than 'average' by reduction_len.
int EffectivePacketsSizeDifference(
    std::vector<int> sizes,
    const RtpPacketizer::PayloadSizeLimits& limits) {
  // Account for larger last packet header.
  sizes.back() += limits.last_packet_reduction_len;

  auto minmax = std::minmax_element(sizes.begin(), sizes.end());
  // MAX-MIN
  return *minmax.second - *minmax.first;
}

int Sum(const std::vector<int>& sizes) {
  return std::accumulate(sizes.begin(), sizes.end(), 0);
}

TEST(RtpPacketizerSplitAboutEqually, AllPacketsAreEqualSumToPayloadLen) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 5;
  limits.last_packet_reduction_len = 2;

  std::vector<int> payload_sizes = RtpPacketizer::SplitAboutEqually(13, limits);

  EXPECT_THAT(Sum(payload_sizes), 13);
}

TEST(RtpPacketizerSplitAboutEqually, AllPacketsAreEqualRespectsMaxPayloadSize) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 5;
  limits.last_packet_reduction_len = 2;

  std::vector<int> payload_sizes = RtpPacketizer::SplitAboutEqually(13, limits);

  EXPECT_THAT(payload_sizes, Each(Le(limits.max_payload_len)));
}

TEST(RtpPacketizerSplitAboutEqually,
     AllPacketsAreEqualRespectsFirstPacketReduction) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 5;
  limits.first_packet_reduction_len = 2;

  std::vector<int> payload_sizes = RtpPacketizer::SplitAboutEqually(13, limits);

  ASSERT_THAT(payload_sizes, Not(IsEmpty()));
  EXPECT_EQ(payload_sizes.front() + limits.first_packet_reduction_len,
            limits.max_payload_len);
}

TEST(RtpPacketizerSplitAboutEqually,
     AllPacketsAreEqualRespectsLastPacketReductionLength) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 5;
  limits.last_packet_reduction_len = 2;

  std::vector<int> payload_sizes = RtpPacketizer::SplitAboutEqually(13, limits);

  ASSERT_THAT(payload_sizes, Not(IsEmpty()));
  EXPECT_LE(payload_sizes.back() + limits.last_packet_reduction_len,
            limits.max_payload_len);
}

TEST(RtpPacketizerSplitAboutEqually, AllPacketsAreEqualInSize) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 5;
  limits.last_packet_reduction_len = 2;

  std::vector<int> payload_sizes = RtpPacketizer::SplitAboutEqually(13, limits);

  EXPECT_EQ(EffectivePacketsSizeDifference(payload_sizes, limits), 0);
}

TEST(RtpPacketizerSplitAboutEqually,
     AllPacketsAreEqualGeneratesMinimumNumberOfPackets) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 5;
  limits.last_packet_reduction_len = 2;

  std::vector<int> payload_sizes = RtpPacketizer::SplitAboutEqually(13, limits);
  // Computed by hand. 3 packets would have exactly capacity 3*5-2=13
  // (max length - for each packet minus last packet reduction).
  EXPECT_THAT(payload_sizes, SizeIs(3));
}

TEST(RtpPacketizerSplitAboutEqually, SomePacketsAreSmallerSumToPayloadLen) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 7;
  limits.last_packet_reduction_len = 5;

  std::vector<int> payload_sizes = RtpPacketizer::SplitAboutEqually(28, limits);

  EXPECT_THAT(Sum(payload_sizes), 28);
}

TEST(RtpPacketizerSplitAboutEqually,
     SomePacketsAreSmallerRespectsMaxPayloadSize) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 7;
  limits.last_packet_reduction_len = 5;

  std::vector<int> payload_sizes = RtpPacketizer::SplitAboutEqually(28, limits);

  EXPECT_THAT(payload_sizes, Each(Le(limits.max_payload_len)));
}

TEST(RtpPacketizerSplitAboutEqually,
     SomePacketsAreSmallerRespectsFirstPacketReduction) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 7;
  limits.first_packet_reduction_len = 5;

  std::vector<int> payload_sizes = RtpPacketizer::SplitAboutEqually(28, limits);

  EXPECT_LE(payload_sizes.front() + limits.first_packet_reduction_len,
            limits.max_payload_len);
}

TEST(RtpPacketizerSplitAboutEqually,
     SomePacketsAreSmallerRespectsLastPacketReductionLength) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 7;
  limits.last_packet_reduction_len = 5;

  std::vector<int> payload_sizes = RtpPacketizer::SplitAboutEqually(28, limits);

  EXPECT_LE(payload_sizes.back(),
            limits.max_payload_len - limits.last_packet_reduction_len);
}

TEST(RtpPacketizerSplitAboutEqually,
     SomePacketsAreSmallerPacketsAlmostEqualInSize) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 7;
  limits.last_packet_reduction_len = 5;

  std::vector<int> payload_sizes = RtpPacketizer::SplitAboutEqually(28, limits);

  EXPECT_LE(EffectivePacketsSizeDifference(payload_sizes, limits), 1);
}

TEST(RtpPacketizerSplitAboutEqually,
     SomePacketsAreSmallerGeneratesMinimumNumberOfPackets) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 7;
  limits.last_packet_reduction_len = 5;

  std::vector<int> payload_sizes = RtpPacketizer::SplitAboutEqually(24, limits);
  // Computed by hand. 4 packets would have capacity 4*7-5=23 (max length -
  // for each packet minus last packet reduction).
  // 5 packets is enough for kPayloadSize.
  EXPECT_THAT(payload_sizes, SizeIs(5));
}

TEST(RtpPacketizerSplitAboutEqually, GivesNonZeroPayloadLengthEachPacket) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 600;
  limits.first_packet_reduction_len = 500;
  limits.last_packet_reduction_len = 550;

  // Naive implementation would split 1450 payload + 1050 reduction bytes into 5
  // packets 500 bytes each, thus leaving first packet zero bytes and even less
  // to last packet.
  std::vector<int> payload_sizes =
      RtpPacketizer::SplitAboutEqually(1450, limits);

  EXPECT_EQ(Sum(payload_sizes), 1450);
  EXPECT_THAT(payload_sizes, Each(Gt(0)));
}

TEST(RtpPacketizerSplitAboutEqually,
     OnePacketWhenExtraSpaceIsEnoughForSumOfFirstAndLastPacketReductions) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 30;
  limits.first_packet_reduction_len = 6;
  limits.last_packet_reduction_len = 4;

  EXPECT_THAT(RtpPacketizer::SplitAboutEqually(20, limits), ElementsAre(20));
}

TEST(RtpPacketizerSplitAboutEqually,
     TwoPacketsWhenExtraSpaceIsTooSmallForSumOfFirstAndLastPacketReductions) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 29;
  limits.first_packet_reduction_len = 6;
  limits.last_packet_reduction_len = 4;

  // First packet needs two more extra bytes compared to last one,
  // so should have two less payload bytes.
  EXPECT_THAT(RtpPacketizer::SplitAboutEqually(20, limits), ElementsAre(9, 11));
}

TEST(RtpPacketizerSplitAboutEqually, RejectsZeroMaxPayloadLen) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 0;

  EXPECT_THAT(RtpPacketizer::SplitAboutEqually(20, limits), IsEmpty());
}

TEST(RtpPacketizerSplitAboutEqually, RejectsZeroFirstPacketLen) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 5;
  limits.first_packet_reduction_len = 5;

  EXPECT_THAT(RtpPacketizer::SplitAboutEqually(20, limits), IsEmpty());
}

TEST(RtpPacketizerSplitAboutEqually, RejectsZeroLastPacketLen) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 5;
  limits.last_packet_reduction_len = 5;

  EXPECT_THAT(RtpPacketizer::SplitAboutEqually(20, limits), IsEmpty());
}

TEST(RtpPacketizerSplitAboutEqually, CantPutSinglePayloadByteInTwoPackets) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 10;
  limits.first_packet_reduction_len = 6;
  limits.last_packet_reduction_len = 4;

  EXPECT_THAT(RtpPacketizer::SplitAboutEqually(1, limits), IsEmpty());
}

TEST(RtpPacketizerSplitAboutEqually, CanPutTwoPayloadBytesInTwoPackets) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 10;
  limits.first_packet_reduction_len = 6;
  limits.last_packet_reduction_len = 4;

  EXPECT_THAT(RtpPacketizer::SplitAboutEqually(2, limits), ElementsAre(1, 1));
}

TEST(RtpPacketizerSplitAboutEqually, CanPutSinglePayloadByteInOnePacket) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 11;
  limits.first_packet_reduction_len = 6;
  limits.last_packet_reduction_len = 4;

  EXPECT_THAT(RtpPacketizer::SplitAboutEqually(1, limits), ElementsAre(1));
}

}  // namespace
}  // namespace webrtc
