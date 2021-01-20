/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_format_video_generic.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAreArray;
using ::testing::Le;

constexpr RtpPacketizer::PayloadSizeLimits kNoSizeLimits;

std::vector<int> NextPacketFillPayloadSizes(RtpPacketizerGeneric* packetizer) {
  RtpPacketToSend packet(nullptr);
  std::vector<int> result;
  while (packetizer->NextPacket(&packet)) {
    result.push_back(packet.payload_size());
  }
  return result;
}

TEST(RtpPacketizerVideoGeneric, RespectsMaxPayloadSize) {
  const size_t kPayloadSize = 50;
  const uint8_t kPayload[kPayloadSize] = {};

  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 6;
  RtpPacketizerGeneric packetizer(kPayload, limits, RTPVideoHeader());

  std::vector<int> payload_sizes = NextPacketFillPayloadSizes(&packetizer);

  EXPECT_THAT(payload_sizes, Each(Le(limits.max_payload_len)));
}

TEST(RtpPacketizerVideoGeneric, UsesMaxPayloadSize) {
  const size_t kPayloadSize = 50;
  const uint8_t kPayload[kPayloadSize] = {};

  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 6;
  RtpPacketizerGeneric packetizer(kPayload, limits, RTPVideoHeader());

  std::vector<int> payload_sizes = NextPacketFillPayloadSizes(&packetizer);

  // With kPayloadSize > max_payload_len^2, there should be packets that use
  // all the payload, otherwise it is possible to use less packets.
  EXPECT_THAT(payload_sizes, Contains(limits.max_payload_len));
}

TEST(RtpPacketizerVideoGeneric, WritesExtendedHeaderWhenPictureIdIsSet) {
  const size_t kPayloadSize = 13;
  const uint8_t kPayload[kPayloadSize] = {};

  RTPVideoHeader rtp_video_header;
  rtp_video_header.video_type_header.emplace<RTPVideoHeaderLegacyGeneric>()
      .picture_id = 37;
  rtp_video_header.frame_type = VideoFrameType::kVideoFrameKey;
  RtpPacketizerGeneric packetizer(kPayload, kNoSizeLimits, rtp_video_header);

  RtpPacketToSend packet(nullptr);
  ASSERT_TRUE(packetizer.NextPacket(&packet));

  rtc::ArrayView<const uint8_t> payload = packet.payload();
  EXPECT_EQ(payload.size(), 3 + kPayloadSize);
  EXPECT_TRUE(payload[0] & 0x04);  // Extended header bit is set.
  // Frame id is 37.
  EXPECT_EQ(0u, payload[1]);
  EXPECT_EQ(37u, payload[2]);
}

TEST(RtpPacketizerVideoGeneric, RespectsMaxPayloadSizeWithExtendedHeader) {
  const int kPayloadSize = 50;
  const uint8_t kPayload[kPayloadSize] = {};

  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 6;
  RTPVideoHeader rtp_video_header;
  rtp_video_header.video_type_header.emplace<RTPVideoHeaderLegacyGeneric>()
      .picture_id = 37;
  RtpPacketizerGeneric packetizer(kPayload, limits, rtp_video_header);

  std::vector<int> payload_sizes = NextPacketFillPayloadSizes(&packetizer);

  EXPECT_THAT(payload_sizes, Each(Le(limits.max_payload_len)));
}

TEST(RtpPacketizerVideoGeneric, UsesMaxPayloadSizeWithExtendedHeader) {
  const int kPayloadSize = 50;
  const uint8_t kPayload[kPayloadSize] = {};

  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 6;
  RTPVideoHeader rtp_video_header;
  rtp_video_header.video_type_header.emplace<RTPVideoHeaderLegacyGeneric>()
      .picture_id = 37;
  RtpPacketizerGeneric packetizer(kPayload, limits, rtp_video_header);
  std::vector<int> payload_sizes = NextPacketFillPayloadSizes(&packetizer);

  // With kPayloadSize > max_payload_len^2, there should be packets that use
  // all the payload, otherwise it is possible to use less packets.
  EXPECT_THAT(payload_sizes, Contains(limits.max_payload_len));
}

TEST(RtpPacketizerVideoGeneric, FrameIdOver15bitsWrapsAround) {
  const int kPayloadSize = 13;
  const uint8_t kPayload[kPayloadSize] = {};

  RTPVideoHeader rtp_video_header;
  rtp_video_header.video_type_header.emplace<RTPVideoHeaderLegacyGeneric>()
      .picture_id = 0x8137;
  rtp_video_header.frame_type = VideoFrameType::kVideoFrameKey;
  RtpPacketizerGeneric packetizer(kPayload, kNoSizeLimits, rtp_video_header);

  RtpPacketToSend packet(nullptr);
  ASSERT_TRUE(packetizer.NextPacket(&packet));

  rtc::ArrayView<const uint8_t> payload = packet.payload();
  EXPECT_TRUE(payload[0] & 0x04);  // Extended header bit is set.
  // Frame id is 0x137.
  EXPECT_EQ(0x01u, payload[1]);
  EXPECT_EQ(0x37u, payload[2]);
}

TEST(RtpPacketizerVideoGeneric, NoFrameIdDoesNotWriteExtendedHeader) {
  const int kPayloadSize = 13;
  const uint8_t kPayload[kPayloadSize] = {};

  RtpPacketizerGeneric packetizer(kPayload, kNoSizeLimits, RTPVideoHeader());

  RtpPacketToSend packet(nullptr);
  ASSERT_TRUE(packetizer.NextPacket(&packet));

  rtc::ArrayView<const uint8_t> payload = packet.payload();
  EXPECT_FALSE(payload[0] & 0x04);
}

TEST(RtpPacketizerVideoGeneric, DoesNotWriteHeaderForRawPayload) {
  const uint8_t kPayload[] = {0x05, 0x25, 0x52};

  RtpPacketizerGeneric packetizer(kPayload, kNoSizeLimits);

  RtpPacketToSend packet(nullptr);
  ASSERT_TRUE(packetizer.NextPacket(&packet));

  rtc::ArrayView<const uint8_t> payload = packet.payload();
  EXPECT_THAT(payload, ElementsAreArray(kPayload));
}

}  // namespace
}  // namespace webrtc
