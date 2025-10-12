/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/flexfec_receive_stream.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "api/call/transport.h"
#include "api/environment/environment_factory.h"
#include "api/rtp_headers.h"
#include "call/flexfec_receive_stream_impl.h"
#include "call/rtp_stream_receiver_controller.h"
#include "modules/rtp_rtcp/mocks/mock_recovered_packet_receiver.h"
#include "modules/rtp_rtcp/mocks/mock_rtcp_rtt_stats.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"

namespace webrtc {

namespace {

using ::testing::Eq;
using ::testing::Property;

constexpr uint8_t kFlexfecPlType = 118;
constexpr uint8_t kFlexfecSsrc[] = {0x00, 0x00, 0x00, 0x01};
constexpr uint8_t kMediaSsrc[] = {0x00, 0x00, 0x00, 0x02};

FlexfecReceiveStream::Config CreateDefaultConfig(
    Transport* rtcp_send_transport) {
  FlexfecReceiveStream::Config config(rtcp_send_transport);
  config.payload_type = kFlexfecPlType;
  config.rtp.remote_ssrc = ByteReader<uint32_t>::ReadBigEndian(kFlexfecSsrc);
  config.protected_media_ssrcs = {
      ByteReader<uint32_t>::ReadBigEndian(kMediaSsrc)};
  EXPECT_TRUE(config.IsCompleteAndEnabled());
  return config;
}

RtpPacketReceived ParsePacket(ArrayView<const uint8_t> packet) {
  RtpPacketReceived parsed_packet(nullptr);
  EXPECT_TRUE(parsed_packet.Parse(packet));
  return parsed_packet;
}

}  // namespace

TEST(FlexfecReceiveStreamConfigTest, IsCompleteAndEnabled) {
  MockTransport rtcp_send_transport;
  FlexfecReceiveStream::Config config(&rtcp_send_transport);

  config.rtp.local_ssrc = 18374743;
  config.rtcp_mode = RtcpMode::kCompound;
  EXPECT_FALSE(config.IsCompleteAndEnabled());

  config.payload_type = 123;
  EXPECT_FALSE(config.IsCompleteAndEnabled());

  config.rtp.remote_ssrc = 238423838;
  EXPECT_FALSE(config.IsCompleteAndEnabled());

  config.protected_media_ssrcs.push_back(138989393);
  EXPECT_TRUE(config.IsCompleteAndEnabled());

  config.protected_media_ssrcs.push_back(33423423);
  EXPECT_FALSE(config.IsCompleteAndEnabled());
}

class FlexfecReceiveStreamTest : public ::testing::Test {
 protected:
  FlexfecReceiveStreamTest()
      : config_(CreateDefaultConfig(&rtcp_send_transport_)) {
    receive_stream_ = std::make_unique<FlexfecReceiveStreamImpl>(
        CreateEnvironment(), config_, &recovered_packet_receiver_, &rtt_stats_);
    receive_stream_->RegisterWithTransport(&rtp_stream_receiver_controller_);
  }

  ~FlexfecReceiveStreamTest() override {
    receive_stream_->UnregisterFromTransport();
  }

  AutoThread main_thread_;
  MockTransport rtcp_send_transport_;
  FlexfecReceiveStream::Config config_;
  MockRecoveredPacketReceiver recovered_packet_receiver_;
  MockRtcpRttStats rtt_stats_;
  RtpStreamReceiverController rtp_stream_receiver_controller_;
  std::unique_ptr<FlexfecReceiveStreamImpl> receive_stream_;
};

TEST_F(FlexfecReceiveStreamTest, ConstructDestruct) {}

// Create a FlexFEC packet that protects a single media packet and ensure
// that the callback is called. Correctness of recovery is checked in the
// FlexfecReceiver unit tests.
TEST_F(FlexfecReceiveStreamTest, RecoversPacket) {
  constexpr uint8_t kFlexfecSeqNum[] = {0x00, 0x01};
  constexpr uint8_t kFlexfecTs[] = {0x00, 0x11, 0x22, 0x33};
  constexpr uint8_t kMediaPlType = 107;
  constexpr uint8_t kMediaSeqNum[] = {0x00, 0x02};
  constexpr uint8_t kMediaTs[] = {0xaa, 0xbb, 0xcc, 0xdd};

  // This packet mask protects a single media packet, i.e., the FlexFEC payload
  // is a copy of that media packet. When inserted in the FlexFEC pipeline,
  // it will thus trivially recover the lost media packet.
  constexpr uint8_t kKBit0 = 1 << 7;
  constexpr uint8_t kFlexfecPktMask[] = {kKBit0 | 0x00, 0x01};
  constexpr uint8_t kPayloadLength[] = {0x00, 0x04};
  constexpr uint8_t kSsrcCount = 1;
  constexpr uint8_t kReservedBits = 0x00;
  constexpr uint8_t kPayloadBits = 0x00;
  // clang-format off
  constexpr uint8_t kFlexfecPacket[] = {
      // RTP header.
      0x80,            kFlexfecPlType,  kFlexfecSeqNum[0],  kFlexfecSeqNum[1],
      kFlexfecTs[0],   kFlexfecTs[1],   kFlexfecTs[2],      kFlexfecTs[3],
      kFlexfecSsrc[0], kFlexfecSsrc[1], kFlexfecSsrc[2],    kFlexfecSsrc[3],
      // FlexFEC header.
      0x00,            kMediaPlType,    kPayloadLength[0],  kPayloadLength[1],
      kMediaTs[0],     kMediaTs[1],     kMediaTs[2],        kMediaTs[3],
      kSsrcCount,      kReservedBits,   kReservedBits,      kReservedBits,
      kMediaSsrc[0],   kMediaSsrc[1],   kMediaSsrc[2],      kMediaSsrc[3],
      kMediaSeqNum[0], kMediaSeqNum[1], kFlexfecPktMask[0], kFlexfecPktMask[1],
      // FEC payload.
      kPayloadBits,    kPayloadBits,    kPayloadBits,       kPayloadBits};
  // clang-format on

  EXPECT_CALL(recovered_packet_receiver_,
              OnRecoveredPacket(Property(&RtpPacketReceived::payload_size,
                                         Eq(kPayloadLength[1]))));

  receive_stream_->OnRtpPacket(ParsePacket(kFlexfecPacket));

  // Tear-down
  receive_stream_->UnregisterFromTransport();
}

}  // namespace webrtc
