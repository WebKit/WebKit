/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Each;
using ::testing::ElementsAreArray;
using ::testing::Le;
using ::testing::SizeIs;

const size_t kMaxPayloadSize = 1200;

uint8_t kTestPayload[kMaxPayloadSize];

std::vector<size_t> NextPacketFillPayloadSizes(
    RtpPacketizerGeneric* packetizer) {
  RtpPacketToSend packet(nullptr);
  std::vector<size_t> result;
  while (packetizer->NextPacket(&packet)) {
    result.push_back(packet.payload_size());
  }
  return result;
}

size_t GetEffectivePacketsSizeDifference(std::vector<size_t>* payload_sizes,
                                         size_t last_packet_reduction_len) {
  // Account for larger last packet header.
  payload_sizes->back() += last_packet_reduction_len;
  auto minmax =
      std::minmax_element(payload_sizes->begin(), payload_sizes->end());
  // MAX-MIN
  size_t difference = *minmax.second - *minmax.first;
  // Revert temporary changes.
  payload_sizes->back() -= last_packet_reduction_len;
  return difference;
}

}  // namespace

TEST(RtpPacketizerVideoGeneric, AllPacketsMayBeEqualAndRespectMaxPayloadSize) {
  const size_t kMaxPayloadLen = 6;
  const size_t kLastPacketReductionLen = 2;
  const size_t kPayloadSize = 13;
  RtpPacketizerGeneric packetizer(kVideoFrameKey, kMaxPayloadLen,
                                  kLastPacketReductionLen);
  size_t num_packets =
      packetizer.SetPayloadData(kTestPayload, kPayloadSize, nullptr);
  std::vector<size_t> payload_sizes = NextPacketFillPayloadSizes(&packetizer);
  EXPECT_THAT(payload_sizes, SizeIs(num_packets));

  EXPECT_THAT(payload_sizes, Each(Le(kMaxPayloadLen)));
}

TEST(RtpPacketizerVideoGeneric,
     AllPacketsMayBeEqual_RespectsLastPacketReductionLength) {
  const size_t kMaxPayloadLen = 6;
  const size_t kLastPacketReductionLen = 2;
  const size_t kPayloadSize = 13;
  RtpPacketizerGeneric packetizer(kVideoFrameKey, kMaxPayloadLen,
                                  kLastPacketReductionLen);
  size_t num_packets =
      packetizer.SetPayloadData(kTestPayload, kPayloadSize, nullptr);
  std::vector<size_t> payload_sizes = NextPacketFillPayloadSizes(&packetizer);
  EXPECT_THAT(payload_sizes, SizeIs(num_packets));

  EXPECT_LE(payload_sizes.back(), kMaxPayloadLen - kLastPacketReductionLen);
}

TEST(RtpPacketizerVideoGeneric,
     AllPacketsMayBeEqual_MakesPacketsAlmostEqualInSize) {
  const size_t kMaxPayloadLen = 6;
  const size_t kLastPacketReductionLen = 2;
  const size_t kPayloadSize = 13;
  RtpPacketizerGeneric packetizer(kVideoFrameKey, kMaxPayloadLen,
                                  kLastPacketReductionLen);
  size_t num_packets =
      packetizer.SetPayloadData(kTestPayload, kPayloadSize, nullptr);
  std::vector<size_t> payload_sizes = NextPacketFillPayloadSizes(&packetizer);
  EXPECT_THAT(payload_sizes, SizeIs(num_packets));

  size_t sizes_difference = GetEffectivePacketsSizeDifference(
      &payload_sizes, kLastPacketReductionLen);
  EXPECT_LE(sizes_difference, 1u);
}

TEST(RtpPacketizerVideoGeneric,
     AllPacketsMayBeEqual_GeneratesMinimumNumberOfPackets) {
  const size_t kMaxPayloadLen = 6;
  const size_t kLastPacketReductionLen = 3;
  const size_t kPayloadSize = 13;
  // Computed by hand. 3 packets would have capacity 3*(6-1)-3=12 (max length -
  // generic header lengh for each packet minus last packet reduction).
  // 4 packets is enough for kPayloadSize.
  const size_t kMinNumPackets = 4;
  RtpPacketizerGeneric packetizer(kVideoFrameKey, kMaxPayloadLen,
                                  kLastPacketReductionLen);
  size_t num_packets =
      packetizer.SetPayloadData(kTestPayload, kPayloadSize, nullptr);
  std::vector<size_t> payload_sizes = NextPacketFillPayloadSizes(&packetizer);
  EXPECT_THAT(payload_sizes, SizeIs(num_packets));

  EXPECT_EQ(num_packets, kMinNumPackets);
}

TEST(RtpPacketizerVideoGeneric, SomePacketsAreSmaller_RespectsMaxPayloadSize) {
  const size_t kMaxPayloadLen = 8;
  const size_t kLastPacketReductionLen = 5;
  const size_t kPayloadSize = 28;
  RtpPacketizerGeneric packetizer(kVideoFrameKey, kMaxPayloadLen,
                                  kLastPacketReductionLen);
  size_t num_packets =
      packetizer.SetPayloadData(kTestPayload, kPayloadSize, nullptr);
  std::vector<size_t> payload_sizes = NextPacketFillPayloadSizes(&packetizer);
  EXPECT_THAT(payload_sizes, SizeIs(num_packets));

  EXPECT_THAT(payload_sizes, Each(Le(kMaxPayloadLen)));
}

TEST(RtpPacketizerVideoGeneric,
     SomePacketsAreSmaller_RespectsLastPacketReductionLength) {
  const size_t kMaxPayloadLen = 8;
  const size_t kLastPacketReductionLen = 5;
  const size_t kPayloadSize = 28;
  RtpPacketizerGeneric packetizer(kVideoFrameKey, kMaxPayloadLen,
                                  kLastPacketReductionLen);
  size_t num_packets =
      packetizer.SetPayloadData(kTestPayload, kPayloadSize, nullptr);
  std::vector<size_t> payload_sizes = NextPacketFillPayloadSizes(&packetizer);
  EXPECT_THAT(payload_sizes, SizeIs(num_packets));

  EXPECT_LE(payload_sizes.back(), kMaxPayloadLen - kLastPacketReductionLen);
}

TEST(RtpPacketizerVideoGeneric,
     SomePacketsAreSmaller_MakesPacketsAlmostEqualInSize) {
  const size_t kMaxPayloadLen = 8;
  const size_t kLastPacketReductionLen = 5;
  const size_t kPayloadSize = 28;
  RtpPacketizerGeneric packetizer(kVideoFrameKey, kMaxPayloadLen,
                                  kLastPacketReductionLen);
  size_t num_packets =
      packetizer.SetPayloadData(kTestPayload, kPayloadSize, nullptr);
  std::vector<size_t> payload_sizes = NextPacketFillPayloadSizes(&packetizer);
  EXPECT_THAT(payload_sizes, SizeIs(num_packets));

  size_t sizes_difference = GetEffectivePacketsSizeDifference(
      &payload_sizes, kLastPacketReductionLen);
  EXPECT_LE(sizes_difference, 1u);
}

TEST(RtpPacketizerVideoGeneric,
     SomePacketsAreSmaller_GeneratesMinimumNumberOfPackets) {
  const size_t kMaxPayloadLen = 8;
  const size_t kLastPacketReductionLen = 5;
  const size_t kPayloadSize = 28;
  // Computed by hand. 4 packets would have capacity 4*(8-1)-5=23 (max length -
  // generic header lengh for each packet minus last packet reduction).
  // 5 packets is enough for kPayloadSize.
  const size_t kMinNumPackets = 5;
  RtpPacketizerGeneric packetizer(kVideoFrameKey, kMaxPayloadLen,
                                  kLastPacketReductionLen);
  size_t num_packets =
      packetizer.SetPayloadData(kTestPayload, kPayloadSize, nullptr);
  std::vector<size_t> payload_sizes = NextPacketFillPayloadSizes(&packetizer);
  EXPECT_THAT(payload_sizes, SizeIs(num_packets));

  EXPECT_EQ(num_packets, kMinNumPackets);
}

}  // namespace webrtc
