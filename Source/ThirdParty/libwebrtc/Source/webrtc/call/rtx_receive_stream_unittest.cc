/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/call/rtx_receive_stream.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_received.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

using ::testing::_;
using ::testing::StrictMock;

class MockRtpPacketSink : public RtpPacketSinkInterface {
 public:
  MOCK_METHOD1(OnRtpPacket, void(const RtpPacketReceived&));
};

constexpr int kMediaPayloadType = 100;
constexpr int kRtxPayloadType = 98;
constexpr uint32_t kMediaSSRC = 0x3333333;
constexpr uint16_t kMediaSeqno = 0x5657;

constexpr uint8_t kRtxPacket[] = {
    0x80,                    // Version 2.
    98,                      // Payload type.
    0x12, 0x34,              // Seqno.
    0x11, 0x11, 0x11, 0x11,  // Timestamp.
    0x22, 0x22, 0x22, 0x22,  // SSRC.
    // RTX header.
    0x56, 0x57,              // Orig seqno.
    // Payload.
    0xee,
};

constexpr uint8_t kRtxPacketWithCVO[] = {
    0x90,                    // Version 2, X set.
    98,                      // Payload type.
    0x12, 0x34,              // Seqno.
    0x11, 0x11, 0x11, 0x11,  // Timestamp.
    0x22, 0x22, 0x22, 0x22,  // SSRC.
    0xbe, 0xde, 0x00, 0x01,  // Extension header.
    0x30, 0x01, 0x00, 0x00,  // 90 degree rotation.
    // RTX header.
    0x56, 0x57,              // Orig seqno.
    // Payload.
    0xee,
};

std::map<int, int> PayloadTypeMapping() {
  std::map<int, int> m;
  m[kRtxPayloadType] = kMediaPayloadType;
  return m;
}

template <typename T>
rtc::ArrayView<T> Truncate(rtc::ArrayView<T> a, size_t drop) {
  return a.subview(0, a.size() - drop);
}

}  // namespace

TEST(RtxReceiveStreamTest, RestoresPacketPayload) {
  StrictMock<MockRtpPacketSink> media_sink;
  RtxReceiveStream rtx_sink(&media_sink, PayloadTypeMapping(), kMediaSSRC);
  RtpPacketReceived rtx_packet;
  EXPECT_TRUE(rtx_packet.Parse(rtc::ArrayView<const uint8_t>(kRtxPacket)));

  EXPECT_CALL(media_sink, OnRtpPacket(_)).WillOnce(testing::Invoke(
      [](const RtpPacketReceived& packet) {
        EXPECT_EQ(packet.SequenceNumber(), kMediaSeqno);
        EXPECT_EQ(packet.Ssrc(), kMediaSSRC);
        EXPECT_EQ(packet.PayloadType(), kMediaPayloadType);
        EXPECT_THAT(packet.payload(), testing::ElementsAre(0xee));
      }));

  rtx_sink.OnRtpPacket(rtx_packet);
}

TEST(RtxReceiveStreamTest, IgnoresUnknownPayloadType) {
  StrictMock<MockRtpPacketSink> media_sink;
  RtxReceiveStream rtx_sink(&media_sink, std::map<int, int>(), kMediaSSRC);
  RtpPacketReceived rtx_packet;
  EXPECT_TRUE(rtx_packet.Parse(rtc::ArrayView<const uint8_t>(kRtxPacket)));
  rtx_sink.OnRtpPacket(rtx_packet);
}

TEST(RtxReceiveStreamTest, IgnoresTruncatedPacket) {
  StrictMock<MockRtpPacketSink> media_sink;
  RtxReceiveStream rtx_sink(&media_sink, PayloadTypeMapping(), kMediaSSRC);
  RtpPacketReceived rtx_packet;
  EXPECT_TRUE(
      rtx_packet.Parse(Truncate(rtc::ArrayView<const uint8_t>(kRtxPacket), 2)));
  rtx_sink.OnRtpPacket(rtx_packet);
}

TEST(RtxReceiveStreamTest, CopiesRtpHeaderExtensions) {
  StrictMock<MockRtpPacketSink> media_sink;
  RtxReceiveStream rtx_sink(&media_sink, PayloadTypeMapping(), kMediaSSRC);
  RtpHeaderExtensionMap extension_map;
  extension_map.RegisterByType(3, kRtpExtensionVideoRotation);
  RtpPacketReceived rtx_packet(&extension_map);
  EXPECT_TRUE(rtx_packet.Parse(
      rtc::ArrayView<const uint8_t>(kRtxPacketWithCVO)));

  VideoRotation rotation = kVideoRotation_0;
  EXPECT_TRUE(rtx_packet.GetExtension<VideoOrientation>(&rotation));
  EXPECT_EQ(kVideoRotation_90, rotation);

  EXPECT_CALL(media_sink, OnRtpPacket(_)).WillOnce(testing::Invoke(
      [](const RtpPacketReceived& packet) {
        EXPECT_EQ(packet.SequenceNumber(), kMediaSeqno);
        EXPECT_EQ(packet.Ssrc(), kMediaSSRC);
        EXPECT_EQ(packet.PayloadType(), kMediaPayloadType);
        EXPECT_THAT(packet.payload(), testing::ElementsAre(0xee));
        VideoRotation rotation = kVideoRotation_0;
        EXPECT_TRUE(packet.GetExtension<VideoOrientation>(&rotation));
        EXPECT_EQ(rotation, kVideoRotation_90);
      }));

  rtx_sink.OnRtpPacket(rtx_packet);
}

}  // namespace webrtc
