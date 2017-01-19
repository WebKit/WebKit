/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/system_wrappers/include/atomic32.h"
#include "webrtc/system_wrappers/include/sleep.h"
#include "webrtc/voice_engine/test/auto_test/fixtures/before_streaming_fixture.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::Field;

class ExtensionVerifyTransport : public webrtc::Transport {
 public:
  ExtensionVerifyTransport()
      : parser_(webrtc::RtpHeaderParser::Create()),
        received_packets_(0),
        bad_packets_(0),
        audio_level_id_(-1),
        absolute_sender_time_id_(-1) {}

  bool SendRtp(const uint8_t* data,
               size_t len,
               const webrtc::PacketOptions& options) override {
    webrtc::RTPHeader header;
    if (parser_->Parse(reinterpret_cast<const uint8_t*>(data), len, &header)) {
      bool ok = true;
      if (audio_level_id_ >= 0 &&
          !header.extension.hasAudioLevel) {
        ok = false;
      }
      if (absolute_sender_time_id_ >= 0 &&
          !header.extension.hasAbsoluteSendTime) {
        ok = false;
      }
      if (!ok) {
        // bad_packets_ count packets we expected to have an extension but
        // didn't have one.
        ++bad_packets_;
      }
    }
    // received_packets_ count all packets we receive.
    ++received_packets_;
    return true;
  }

  bool SendRtcp(const uint8_t* data, size_t len) override {
    return true;
  }

  void SetAudioLevelId(int id) {
    audio_level_id_ = id;
    parser_->RegisterRtpHeaderExtension(webrtc::kRtpExtensionAudioLevel, id);
  }

  void SetAbsoluteSenderTimeId(int id) {
    absolute_sender_time_id_ = id;
    parser_->RegisterRtpHeaderExtension(webrtc::kRtpExtensionAbsoluteSendTime,
                                        id);
  }

  bool Wait() {
    // Wait until we've received to specified number of packets.
    while (received_packets_.Value() < kPacketsExpected) {
      webrtc::SleepMs(kSleepIntervalMs);
    }
    // Check whether any were 'bad' (didn't contain an extension when they
    // where supposed to).
    return bad_packets_.Value() == 0;
  }

 private:
  enum {
    kPacketsExpected = 10,
    kSleepIntervalMs = 10
  };
  std::unique_ptr<webrtc::RtpHeaderParser> parser_;
  webrtc::Atomic32 received_packets_;
  webrtc::Atomic32 bad_packets_;
  int audio_level_id_;
  int absolute_sender_time_id_;
};

class SendRtpRtcpHeaderExtensionsTest : public BeforeStreamingFixture {
 protected:
  void SetUp() override {
    EXPECT_EQ(0, voe_network_->DeRegisterExternalTransport(channel_));
    EXPECT_EQ(0, voe_network_->RegisterExternalTransport(channel_,
                                                         verifying_transport_));
  }
  void TearDown() override { PausePlaying(); }

  ExtensionVerifyTransport verifying_transport_;
};

TEST_F(SendRtpRtcpHeaderExtensionsTest, SentPacketsIncludeNoAudioLevel) {
  verifying_transport_.SetAudioLevelId(0);
  ResumePlaying();
  EXPECT_FALSE(verifying_transport_.Wait());
}

TEST_F(SendRtpRtcpHeaderExtensionsTest, SentPacketsIncludeAudioLevel) {
  EXPECT_EQ(0, voe_rtp_rtcp_->SetSendAudioLevelIndicationStatus(channel_, true,
                                                                9));
  verifying_transport_.SetAudioLevelId(9);
  ResumePlaying();
  EXPECT_TRUE(verifying_transport_.Wait());
}

