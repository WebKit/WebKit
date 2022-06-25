/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/rtx_receive_stream.h"

#include "call/test/mock_rtp_packet_sink_interface.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

using ::testing::_;
using ::testing::Property;
using ::testing::StrictMock;

constexpr int kMediaPayloadType = 100;
constexpr int kRtxPayloadType = 98;
constexpr int kUnknownPayloadType = 90;
constexpr uint32_t kMediaSSRC = 0x3333333;
constexpr uint16_t kMediaSeqno = 0x5657;

constexpr uint8_t kRtxPacket[] = {
    0x80,  // Version 2.
    98,    // Payload type.
    0x12,
    0x34,  // Seqno.
    0x11,
    0x11,
    0x11,
    0x11,  // Timestamp.
    0x22,
    0x22,
    0x22,
    0x22,  // SSRC.
    // RTX header.
    0x56,
    0x57,  // Orig seqno.
    // Payload.
    0xee,
};

constexpr uint8_t kRtxPacketWithPadding[] = {
    0xa0,  // Version 2, P set
    98,    // Payload type.
    0x12,
    0x34,  // Seqno.
    0x11,
    0x11,
    0x11,
    0x11,  // Timestamp.
    0x22,
    0x22,
    0x22,
    0x22,  // SSRC.
    // RTX header.
    0x56,
    0x57,  // Orig seqno.
    // Padding
    0x1,
};

constexpr uint8_t kRtxPacketWithCVO[] = {
    0x90,  // Version 2, X set.
    98,    // Payload type.
    0x12,
    0x34,  // Seqno.
    0x11,
    0x11,
    0x11,
    0x11,  // Timestamp.
    0x22,
    0x22,
    0x22,
    0x22,  // SSRC.
    0xbe,
    0xde,
    0x00,
    0x01,  // Extension header.
    0x30,
    0x01,
    0x00,
    0x00,  // 90 degree rotation.
    // RTX header.
    0x56,
    0x57,  // Orig seqno.
    // Payload.
    0xee,
};

std::map<int, int> PayloadTypeMapping() {
  const std::map<int, int> m = {{kRtxPayloadType, kMediaPayloadType}};
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

  EXPECT_CALL(media_sink, OnRtpPacket)
      .WillOnce([](const RtpPacketReceived& packet) {
        EXPECT_EQ(packet.SequenceNumber(), kMediaSeqno);
        EXPECT_EQ(packet.Ssrc(), kMediaSSRC);
        EXPECT_EQ(packet.PayloadType(), kMediaPayloadType);
        EXPECT_THAT(packet.payload(), ::testing::ElementsAre(0xee));
      });

  rtx_sink.OnRtpPacket(rtx_packet);
}

TEST(RtxReceiveStreamTest, SetsRecoveredFlag) {
  StrictMock<MockRtpPacketSink> media_sink;
  RtxReceiveStream rtx_sink(&media_sink, PayloadTypeMapping(), kMediaSSRC);
  RtpPacketReceived rtx_packet;
  EXPECT_TRUE(rtx_packet.Parse(rtc::ArrayView<const uint8_t>(kRtxPacket)));
  EXPECT_FALSE(rtx_packet.recovered());
  EXPECT_CALL(media_sink, OnRtpPacket)
      .WillOnce([](const RtpPacketReceived& packet) {
        EXPECT_TRUE(packet.recovered());
      });

  rtx_sink.OnRtpPacket(rtx_packet);
}

TEST(RtxReceiveStreamTest, IgnoresUnknownPayloadType) {
  StrictMock<MockRtpPacketSink> media_sink;
  const std::map<int, int> payload_type_mapping = {
      {kUnknownPayloadType, kMediaPayloadType}};

  RtxReceiveStream rtx_sink(&media_sink, payload_type_mapping, kMediaSSRC);
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
  EXPECT_TRUE(
      rtx_packet.Parse(rtc::ArrayView<const uint8_t>(kRtxPacketWithCVO)));

  VideoRotation rotation = kVideoRotation_0;
  EXPECT_TRUE(rtx_packet.GetExtension<VideoOrientation>(&rotation));
  EXPECT_EQ(kVideoRotation_90, rotation);

  EXPECT_CALL(media_sink, OnRtpPacket)
      .WillOnce([](const RtpPacketReceived& packet) {
        EXPECT_EQ(packet.SequenceNumber(), kMediaSeqno);
        EXPECT_EQ(packet.Ssrc(), kMediaSSRC);
        EXPECT_EQ(packet.PayloadType(), kMediaPayloadType);
        EXPECT_THAT(packet.payload(), ::testing::ElementsAre(0xee));
        VideoRotation rotation = kVideoRotation_0;
        EXPECT_TRUE(packet.GetExtension<VideoOrientation>(&rotation));
        EXPECT_EQ(rotation, kVideoRotation_90);
      });

  rtx_sink.OnRtpPacket(rtx_packet);
}

TEST(RtxReceiveStreamTest, PropagatesArrivalTime) {
  StrictMock<MockRtpPacketSink> media_sink;
  RtxReceiveStream rtx_sink(&media_sink, PayloadTypeMapping(), kMediaSSRC);
  RtpPacketReceived rtx_packet(nullptr);
  EXPECT_TRUE(rtx_packet.Parse(rtc::ArrayView<const uint8_t>(kRtxPacket)));
  rtx_packet.set_arrival_time(Timestamp::Millis(123));
  EXPECT_CALL(media_sink, OnRtpPacket(Property(&RtpPacketReceived::arrival_time,
                                               Timestamp::Millis(123))));
  rtx_sink.OnRtpPacket(rtx_packet);
}

TEST(RtxReceiveStreamTest, SupportsLargePacket) {
  StrictMock<MockRtpPacketSink> media_sink;
  RtxReceiveStream rtx_sink(&media_sink, PayloadTypeMapping(), kMediaSSRC);
  RtpPacketReceived rtx_packet;
  constexpr int kRtxPacketSize = 2000;
  constexpr int kRtxPayloadOffset = 14;
  uint8_t large_rtx_packet[kRtxPacketSize];
  memcpy(large_rtx_packet, kRtxPacket, sizeof(kRtxPacket));
  rtc::ArrayView<uint8_t> payload(large_rtx_packet + kRtxPayloadOffset,
                                  kRtxPacketSize - kRtxPayloadOffset);

  // Fill payload.
  for (size_t i = 0; i < payload.size(); i++) {
    payload[i] = i;
  }
  EXPECT_TRUE(
      rtx_packet.Parse(rtc::ArrayView<const uint8_t>(large_rtx_packet)));

  EXPECT_CALL(media_sink, OnRtpPacket)
      .WillOnce([&](const RtpPacketReceived& packet) {
        EXPECT_EQ(packet.SequenceNumber(), kMediaSeqno);
        EXPECT_EQ(packet.Ssrc(), kMediaSSRC);
        EXPECT_EQ(packet.PayloadType(), kMediaPayloadType);
        EXPECT_THAT(packet.payload(), ::testing::ElementsAreArray(payload));
      });

  rtx_sink.OnRtpPacket(rtx_packet);
}

TEST(RtxReceiveStreamTest, SupportsLargePacketWithPadding) {
  StrictMock<MockRtpPacketSink> media_sink;
  RtxReceiveStream rtx_sink(&media_sink, PayloadTypeMapping(), kMediaSSRC);
  RtpPacketReceived rtx_packet;
  constexpr int kRtxPacketSize = 2000;
  constexpr int kRtxPayloadOffset = 14;
  constexpr int kRtxPaddingSize = 50;
  uint8_t large_rtx_packet[kRtxPacketSize];
  memcpy(large_rtx_packet, kRtxPacketWithPadding,
         sizeof(kRtxPacketWithPadding));
  rtc::ArrayView<uint8_t> payload(
      large_rtx_packet + kRtxPayloadOffset,
      kRtxPacketSize - kRtxPayloadOffset - kRtxPaddingSize);
  rtc::ArrayView<uint8_t> padding(
      large_rtx_packet + kRtxPacketSize - kRtxPaddingSize, kRtxPaddingSize);

  // Fill payload.
  for (size_t i = 0; i < payload.size(); i++) {
    payload[i] = i;
  }
  // Fill padding. Only value of last padding byte matters.
  for (size_t i = 0; i < padding.size(); i++) {
    padding[i] = kRtxPaddingSize;
  }

  EXPECT_TRUE(
      rtx_packet.Parse(rtc::ArrayView<const uint8_t>(large_rtx_packet)));

  EXPECT_CALL(media_sink, OnRtpPacket)
      .WillOnce([&](const RtpPacketReceived& packet) {
        EXPECT_EQ(packet.SequenceNumber(), kMediaSeqno);
        EXPECT_EQ(packet.Ssrc(), kMediaSSRC);
        EXPECT_EQ(packet.PayloadType(), kMediaPayloadType);
        EXPECT_THAT(packet.payload(), ::testing::ElementsAreArray(payload));
      });

  rtx_sink.OnRtpPacket(rtx_packet);
}

}  // namespace webrtc
