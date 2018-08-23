/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>

#include <algorithm>
#include <memory>

#include "api/video_codecs/video_codec.h"
#include "modules/rtp_rtcp/include/rtp_payload_registry.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_receiver_video.h"
#include "modules/rtp_rtcp/test/testAPI/test_api.h"
#include "rtc_base/rate_limiter.h"
#include "test/gtest.h"

namespace {
const uint32_t kSsrc = 3456;
const unsigned char kPayloadType = 100;
};

namespace webrtc {

class RtpRtcpVideoTest : public ::testing::Test {
 protected:
  RtpRtcpVideoTest()
      : fake_clock_(123456), retransmission_rate_limiter_(&fake_clock_, 1000) {}
  ~RtpRtcpVideoTest() override = default;

  void SetUp() override {
    transport_ = new LoopBackTransport();
    receiver_ = new TestRtpReceiver();
    receive_statistics_.reset(ReceiveStatistics::Create(&fake_clock_));
    RtpRtcp::Configuration configuration;
    configuration.audio = false;
    configuration.clock = &fake_clock_;
    configuration.outgoing_transport = transport_;
    configuration.retransmission_rate_limiter = &retransmission_rate_limiter_;

    video_module_ = RtpRtcp::CreateRtpRtcp(configuration);
    rtp_receiver_.reset(RtpReceiver::CreateVideoReceiver(
        &fake_clock_, receiver_, &rtp_payload_registry_));

    video_module_->SetRTCPStatus(RtcpMode::kCompound);
    video_module_->SetSSRC(kSsrc);
    video_module_->SetStorePacketsStatus(true, 600);
    EXPECT_EQ(0, video_module_->SetSendingStatus(true));

    transport_->SetSendModule(video_module_, &rtp_payload_registry_,
                              rtp_receiver_.get(), receive_statistics_.get());

    VideoCodec video_codec;
    memset(&video_codec, 0, sizeof(video_codec));
    video_codec.plType = 123;
    video_codec.codecType = kVideoCodecI420;

    video_module_->RegisterVideoSendPayload(123, "I420");
    EXPECT_EQ(0, rtp_payload_registry_.RegisterReceivePayload(video_codec));

    payload_data_length_ = sizeof(video_frame_);

    for (size_t n = 0; n < payload_data_length_; n++) {
      video_frame_[n] = n % 10;
    }
  }

  size_t BuildRTPheader(uint8_t* buffer,
                        uint32_t timestamp,
                        uint32_t sequence_number) {
    buffer[0] = static_cast<uint8_t>(0x80);  // version 2
    buffer[1] = static_cast<uint8_t>(kPayloadType);
    ByteWriter<uint16_t>::WriteBigEndian(buffer + 2, sequence_number);
    ByteWriter<uint32_t>::WriteBigEndian(buffer + 4, timestamp);
    ByteWriter<uint32_t>::WriteBigEndian(buffer + 8, 0x1234);  // SSRC.
    size_t rtpHeaderLength = 12;
    return rtpHeaderLength;
  }

  size_t PaddingPacket(uint8_t* buffer,
                       uint32_t timestamp,
                       uint32_t sequence_number,
                       size_t bytes) {
    // Max in the RFC 3550 is 255 bytes, we limit it to be modulus 32 for SRTP.
    size_t max_length = 224;

    size_t padding_bytes_in_packet = max_length;
    if (bytes < max_length) {
      padding_bytes_in_packet = (bytes + 16) & 0xffe0;  // Keep our modulus 32.
    }
    // Correct seq num, timestamp and payload type.
    size_t header_length = BuildRTPheader(buffer, timestamp, sequence_number);
    buffer[0] |= 0x20;  // Set padding bit.
    int32_t* data = reinterpret_cast<int32_t*>(&(buffer[header_length]));

    // Fill data buffer with random data.
    for (size_t j = 0; j < (padding_bytes_in_packet >> 2); j++) {
      data[j] = rand();  // NOLINT
    }
    // Set number of padding bytes in the last byte of the packet.
    buffer[header_length + padding_bytes_in_packet - 1] =
        padding_bytes_in_packet;
    return padding_bytes_in_packet + header_length;
  }

  void TearDown() override {
    delete video_module_;
    delete transport_;
    delete receiver_;
  }

  std::unique_ptr<ReceiveStatistics> receive_statistics_;
  RTPPayloadRegistry rtp_payload_registry_;
  std::unique_ptr<RtpReceiver> rtp_receiver_;
  RtpRtcp* video_module_;
  LoopBackTransport* transport_;
  TestRtpReceiver* receiver_;
  uint8_t video_frame_[65000];
  size_t payload_data_length_;
  SimulatedClock fake_clock_;
  RateLimiter retransmission_rate_limiter_;
};

TEST_F(RtpRtcpVideoTest, BasicVideo) {
  uint32_t timestamp = 3000;
  RTPVideoHeader video_header;
  EXPECT_TRUE(video_module_->SendOutgoingData(
      kVideoFrameDelta, 123, timestamp, timestamp / 90, video_frame_,
      payload_data_length_, nullptr, &video_header, nullptr));
}

TEST_F(RtpRtcpVideoTest, PaddingOnlyFrames) {
  const size_t kPadSize = 255;
  uint8_t padding_packet[kPadSize];
  uint32_t seq_num = 0;
  uint32_t timestamp = 3000;
  VideoCodec codec;
  codec.codecType = kVideoCodecVP8;
  codec.plType = kPayloadType;
  EXPECT_EQ(0, rtp_payload_registry_.RegisterReceivePayload(codec));
  for (int frame_idx = 0; frame_idx < 10; ++frame_idx) {
    for (int packet_idx = 0; packet_idx < 5; ++packet_idx) {
      size_t packet_size =
          PaddingPacket(padding_packet, timestamp, seq_num, kPadSize);
      ++seq_num;
      RTPHeader header;
      std::unique_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
      EXPECT_TRUE(parser->Parse(padding_packet, packet_size, &header));
      const auto pl =
          rtp_payload_registry_.PayloadTypeToPayload(header.payloadType);
      EXPECT_TRUE(pl);
      const uint8_t* payload = padding_packet + header.headerLength;
      const size_t payload_length = packet_size - header.headerLength;
      EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
          header, payload, payload_length, pl->typeSpecific));
      EXPECT_EQ(0u, receiver_->payload_size());
      EXPECT_EQ(payload_length, receiver_->rtp_header().header.paddingLength);
    }
    timestamp += 3000;
    fake_clock_.AdvanceTimeMilliseconds(33);
  }
}

}  // namespace webrtc
