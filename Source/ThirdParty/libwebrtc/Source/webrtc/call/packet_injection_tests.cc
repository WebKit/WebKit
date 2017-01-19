/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/test/call_test.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/null_transport.h"

namespace webrtc {

class PacketInjectionTest : public test::CallTest {
 protected:
  enum class CodecType {
    kVp8,
    kH264,
  };

  PacketInjectionTest() : rtp_header_parser_(RtpHeaderParser::Create()) {}

  void InjectIncorrectPacket(CodecType codec_type,
                             uint8_t packet_type,
                             const uint8_t* packet,
                             size_t length);

  std::unique_ptr<RtpHeaderParser> rtp_header_parser_;
};

void PacketInjectionTest::InjectIncorrectPacket(CodecType codec_type,
                                                uint8_t payload_type,
                                                const uint8_t* packet,
                                                size_t length) {
  CreateSenderCall(Call::Config(&event_log_));
  CreateReceiverCall(Call::Config(&event_log_));

  test::NullTransport null_transport;
  CreateSendConfig(1, 0, 0, &null_transport);
  CreateMatchingReceiveConfigs(&null_transport);
  video_receive_configs_[0].decoders[0].payload_type = payload_type;
  switch (codec_type) {
    case CodecType::kVp8:
      video_receive_configs_[0].decoders[0].payload_name = "VP8";
      break;
    case CodecType::kH264:
      video_receive_configs_[0].decoders[0].payload_name = "H264";
      break;
  }
  CreateVideoStreams();

  RTPHeader header;
  EXPECT_TRUE(rtp_header_parser_->Parse(packet, length, &header));
  EXPECT_EQ(kVideoSendSsrcs[0], header.ssrc)
      << "Packet should have configured SSRC to not be dropped early.";
  EXPECT_EQ(payload_type, header.payloadType);
  Start();
  EXPECT_EQ(PacketReceiver::DELIVERY_PACKET_ERROR,
            receiver_call_->Receiver()->DeliverPacket(MediaType::VIDEO, packet,
                                                      length, PacketTime()));
  Stop();

  DestroyStreams();
}

TEST_F(PacketInjectionTest, StapAPacketWithTruncatedNalUnits) {
  const uint8_t kPacket[] = {0x80,
                             0xE5,
                             0xE6,
                             0x0,
                             0x0,
                             0xED,
                             0x23,
                             0x4,
                             0x00,
                             0xC0,
                             0xFF,
                             0xED,
                             0x58,
                             0xCB,
                             0xED,
                             0xDF};

  InjectIncorrectPacket(CodecType::kH264, 101, kPacket, sizeof(kPacket));
}

}  // namespace webrtc
