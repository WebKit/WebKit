/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "api/rtp_header_extension_id.h"
#include "api/rtp_headers.h"
#include "api/rtp_parameters.h"
#include "api/test/simulated_network.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "modules/rtp_rtcp/source/rtcp_packet/target_bitrate.h"
#include "rtc_base/event.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "test/call_test.h"
#include "test/gtest.h"
#include "test/rtcp_packet_parser.h"
#include "test/rtp_rtcp_observer.h"
#include "test/video_test_constants.h"
#include "video/config/video_encoder_config.h"

namespace webrtc {
namespace {
constexpr RtpHeaderExtensionId kTransportSequenceNumberExtensionId(2);

class ExtendedReportsEndToEndTest : public test::CallTest {
 public:
  ExtendedReportsEndToEndTest() {
    RegisterRtpExtension(RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                                      kTransportSequenceNumberExtensionId));
  }
};

class RtcpXrObserver : public test::EndToEndTest {
 public:
  RtcpXrObserver(bool enable_rrtr,
                 VideoEncoderConfig::ContentType content_type,
                 TimeDelta timeout = test::VideoTestConstants::kDefaultTimeout)
      : EndToEndTest(timeout),
        enable_rrtr_(enable_rrtr),
        content_type_(content_type),
        sent_rtcp_sr_(0),
        sent_rtcp_rr_(0),
        sent_rtcp_rrtr_(0),
        sent_rtcp_target_bitrate_(false),
        sent_zero_rtcp_target_bitrate_(false),
        sent_rtcp_dlrr_(0),
        send_simulated_network_(nullptr) {
    forward_transport_config_.link_capacity = DataRate::KilobitsPerSec(500);
    forward_transport_config_.queue_delay_ms = 0;
    forward_transport_config_.loss_percent = 0;
  }

 private:
  // Receive stream should send RR packets (and RRTR packets if enabled).
  Action OnReceiveRtcp(std::span<const uint8_t> packet) override {
    MutexLock lock(&mutex_);
    test::RtcpPacketParser parser;
    EXPECT_TRUE(parser.Parse(packet));

    sent_rtcp_rr_ += parser.receiver_report()->num_packets();
    EXPECT_EQ(0, parser.sender_report()->num_packets());
    EXPECT_GE(1, parser.xr()->num_packets());
    if (parser.xr()->num_packets() > 0) {
      if (parser.xr()->rrtr())
        ++sent_rtcp_rrtr_;
      EXPECT_FALSE(parser.xr()->dlrr());
    }

    return SEND_PACKET;
  }
  // Send stream should send SR packets (and DLRR packets if enabled).
  Action OnSendRtcp(std::span<const uint8_t> packet) override {
    MutexLock lock(&mutex_);
    test::RtcpPacketParser parser;
    EXPECT_TRUE(parser.Parse(packet));

    sent_rtcp_sr_ += parser.sender_report()->num_packets();
    EXPECT_LE(parser.xr()->num_packets(), 1);
    if (parser.xr()->num_packets() > 0) {
      EXPECT_FALSE(parser.xr()->rrtr());
      if (parser.xr()->dlrr())
        ++sent_rtcp_dlrr_;
      if (parser.xr()->target_bitrate()) {
        sent_rtcp_target_bitrate_ = true;
        auto target_bitrates =
            parser.xr()->target_bitrate()->GetTargetBitrates();
        if (target_bitrates.empty()) {
          sent_zero_rtcp_target_bitrate_ = true;
        }
        for (const rtcp::TargetBitrate::BitrateItem& item : target_bitrates) {
          if (item.target_bitrate_kbps == 0) {
            sent_zero_rtcp_target_bitrate_ = true;
            break;
          }
        }
      }
    }

    if (sent_rtcp_sr_ > kNumRtcpReportPacketsToObserve &&
        sent_rtcp_rr_ > kNumRtcpReportPacketsToObserve) {
      if (enable_rrtr_) {
        EXPECT_GT(sent_rtcp_rrtr_, 0);
        EXPECT_GT(sent_rtcp_dlrr_, 0);
      } else {
        EXPECT_EQ(sent_rtcp_rrtr_, 0);
        EXPECT_EQ(sent_rtcp_dlrr_, 0);
      }
      EXPECT_FALSE(sent_rtcp_target_bitrate_);
      EXPECT_FALSE(sent_zero_rtcp_target_bitrate_);
      observation_complete_.Set();
    }
    return SEND_PACKET;
  }

  size_t GetNumVideoStreams() const override { return 1; }

  BuiltInNetworkBehaviorConfig GetSendTransportConfig() const override {
    return forward_transport_config_;
  }

  void OnTransportCreated(
      test::PacketTransport* to_receiver,
      SimulatedNetworkInterface* sender_network,
      test::PacketTransport* to_sender,
      SimulatedNetworkInterface* receiver_network) override {
    send_simulated_network_ = sender_network;
  }

  void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStreamInterface::Config>* receive_configs,
      VideoEncoderConfig* encoder_config) override {
    encoder_config->content_type = content_type_;
    (*receive_configs)[0].rtp.rtcp_mode = RtcpMode::kReducedSize;
    (*receive_configs)[0].rtp.rtcp_xr.receiver_reference_time_report =
        enable_rrtr_;
  }

  void PerformTest() override {
    EXPECT_TRUE(Wait())
        << "Timed out while waiting for RTCP SR/RR packets to be sent.";
  }

  static const int kNumRtcpReportPacketsToObserve = 5;

  Mutex mutex_;
  const bool enable_rrtr_;
  const VideoEncoderConfig::ContentType content_type_;
  int sent_rtcp_sr_;
  int sent_rtcp_rr_ RTC_GUARDED_BY(&mutex_);
  int sent_rtcp_rrtr_ RTC_GUARDED_BY(&mutex_);
  bool sent_rtcp_target_bitrate_ RTC_GUARDED_BY(&mutex_);
  bool sent_zero_rtcp_target_bitrate_ RTC_GUARDED_BY(&mutex_);
  int sent_rtcp_dlrr_;
  BuiltInNetworkBehaviorConfig forward_transport_config_;
  SimulatedNetworkInterface* send_simulated_network_ = nullptr;
};

TEST_F(ExtendedReportsEndToEndTest, TestExtendedReportsWithRrtr) {
  RtcpXrObserver test(/*enable_rrtr=*/true,
                      VideoEncoderConfig::ContentType::kRealtimeVideo);
  RunBaseTest(&test);
}

TEST_F(ExtendedReportsEndToEndTest, TestExtendedReportsWithoutRrtr) {
  RtcpXrObserver test(/*enable_rrtr=*/false,
                      VideoEncoderConfig::ContentType::kRealtimeVideo);
  RunBaseTest(&test);
}

}  // namespace
}  // namespace webrtc
