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
#include <string>

#include "call/rtp_transport_controller_send.h"
#include "call/rtp_video_sender.h"
#include "modules/video_coding/fec_controller_default.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/rate_limiter.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "video/call_stats.h"
#include "video/send_delay_stats.h"
#include "video/send_statistics_proxy.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Unused;

namespace webrtc {
namespace {
const int8_t kPayloadType = 96;
const uint32_t kSsrc1 = 12345;
const uint32_t kSsrc2 = 23456;
const int16_t kInitialPictureId1 = 222;
const int16_t kInitialPictureId2 = 44;
const int16_t kInitialTl0PicIdx1 = 99;
const int16_t kInitialTl0PicIdx2 = 199;
const int64_t kRetransmitWindowSizeMs = 500;

class MockRtcpIntraFrameObserver : public RtcpIntraFrameObserver {
 public:
  MOCK_METHOD1(OnReceivedIntraFrameRequest, void(uint32_t));
};

class MockCongestionObserver : public NetworkChangedObserver {
 public:
  MOCK_METHOD4(OnNetworkChanged,
               void(uint32_t bitrate_bps,
                    uint8_t fraction_loss,
                    int64_t rtt_ms,
                    int64_t probing_interval_ms));
};

RtpSenderObservers CreateObservers(
    RtcpRttStats* rtcp_rtt_stats,
    RtcpIntraFrameObserver* intra_frame_callback,
    RtcpStatisticsCallback* rtcp_stats,
    StreamDataCountersCallback* rtp_stats,
    BitrateStatisticsObserver* bitrate_observer,
    FrameCountObserver* frame_count_observer,
    RtcpPacketTypeCounterObserver* rtcp_type_observer,
    SendSideDelayObserver* send_delay_observer,
    SendPacketObserver* send_packet_observer) {
  RtpSenderObservers observers;
  observers.rtcp_rtt_stats = rtcp_rtt_stats;
  observers.intra_frame_callback = intra_frame_callback;
  observers.rtcp_stats = rtcp_stats;
  observers.rtp_stats = rtp_stats;
  observers.bitrate_observer = bitrate_observer;
  observers.frame_count_observer = frame_count_observer;
  observers.rtcp_type_observer = rtcp_type_observer;
  observers.send_delay_observer = send_delay_observer;
  observers.send_packet_observer = send_packet_observer;
  return observers;
}

class RtpVideoSenderTestFixture {
 public:
  RtpVideoSenderTestFixture(
      const std::vector<uint32_t>& ssrcs,
      int payload_type,
      const std::map<uint32_t, RtpPayloadState>& suspended_payload_states)
      : clock_(0),
        config_(&transport_),
        send_delay_stats_(&clock_),
        transport_controller_(&clock_, &event_log_, nullptr, bitrate_config_),
        process_thread_(ProcessThread::Create("test_thread")),
        call_stats_(&clock_, process_thread_.get()),
        stats_proxy_(&clock_,
                     config_,
                     VideoEncoderConfig::ContentType::kRealtimeVideo),
        retransmission_rate_limiter_(&clock_, kRetransmitWindowSizeMs) {
    for (uint32_t ssrc : ssrcs) {
      config_.rtp.ssrcs.push_back(ssrc);
    }
    config_.rtp.payload_type = payload_type;
    std::map<uint32_t, RtpState> suspended_ssrcs;
    router_ = absl::make_unique<RtpVideoSender>(
        config_.rtp.ssrcs, suspended_ssrcs, suspended_payload_states,
        config_.rtp, config_.rtcp_report_interval_ms, &transport_,
        CreateObservers(&call_stats_, &encoder_feedback_, &stats_proxy_,
                        &stats_proxy_, &stats_proxy_, &stats_proxy_,
                        &stats_proxy_, &stats_proxy_, &send_delay_stats_),
        &transport_controller_, &event_log_, &retransmission_rate_limiter_,
        absl::make_unique<FecControllerDefault>(&clock_), nullptr,
        CryptoOptions{});
  }

  RtpVideoSender* router() { return router_.get(); }

 private:
  NiceMock<MockTransport> transport_;
  NiceMock<MockCongestionObserver> congestion_observer_;
  NiceMock<MockRtcpIntraFrameObserver> encoder_feedback_;
  SimulatedClock clock_;
  RtcEventLogNullImpl event_log_;
  VideoSendStream::Config config_;
  SendDelayStats send_delay_stats_;
  BitrateConstraints bitrate_config_;
  RtpTransportControllerSend transport_controller_;
  std::unique_ptr<ProcessThread> process_thread_;
  CallStats call_stats_;
  SendStatisticsProxy stats_proxy_;
  RateLimiter retransmission_rate_limiter_;
  std::unique_ptr<RtpVideoSender> router_;
};
}  // namespace

class RtpVideoSenderTest : public ::testing::Test,
                           public ::testing::WithParamInterface<std::string> {
 public:
  RtpVideoSenderTest() : field_trial_(GetParam()) {}

 private:
  test::ScopedFieldTrials field_trial_;
};

INSTANTIATE_TEST_CASE_P(Default, RtpVideoSenderTest, ::testing::Values(""));

INSTANTIATE_TEST_CASE_P(
    TaskQueueTrial,
    RtpVideoSenderTest,
    ::testing::Values("WebRTC-TaskQueueCongestionControl/Enabled/"));

TEST_P(RtpVideoSenderTest, SendOnOneModule) {
  uint8_t payload = 'a';
  EncodedImage encoded_image;
  encoded_image.SetTimestamp(1);
  encoded_image.capture_time_ms_ = 2;
  encoded_image._frameType = kVideoFrameKey;
  encoded_image._buffer = &payload;
  encoded_image._length = 1;

  RtpVideoSenderTestFixture test({kSsrc1}, kPayloadType, {});
  EXPECT_NE(
      EncodedImageCallback::Result::OK,
      test.router()->OnEncodedImage(encoded_image, nullptr, nullptr).error);

  test.router()->SetActive(true);
  EXPECT_EQ(
      EncodedImageCallback::Result::OK,
      test.router()->OnEncodedImage(encoded_image, nullptr, nullptr).error);

  test.router()->SetActive(false);
  EXPECT_NE(
      EncodedImageCallback::Result::OK,
      test.router()->OnEncodedImage(encoded_image, nullptr, nullptr).error);

  test.router()->SetActive(true);
  EXPECT_EQ(
      EncodedImageCallback::Result::OK,
      test.router()->OnEncodedImage(encoded_image, nullptr, nullptr).error);
}

TEST_P(RtpVideoSenderTest, SendSimulcastSetActive) {
  uint8_t payload = 'a';
  EncodedImage encoded_image_1;
  encoded_image_1.SetTimestamp(1);
  encoded_image_1.capture_time_ms_ = 2;
  encoded_image_1._frameType = kVideoFrameKey;
  encoded_image_1._buffer = &payload;
  encoded_image_1._length = 1;

  RtpVideoSenderTestFixture test({kSsrc1, kSsrc2}, kPayloadType, {});

  CodecSpecificInfo codec_info;
  memset(&codec_info, 0, sizeof(CodecSpecificInfo));
  codec_info.codecType = kVideoCodecVP8;

  test.router()->SetActive(true);
  EXPECT_EQ(EncodedImageCallback::Result::OK,
            test.router()
                ->OnEncodedImage(encoded_image_1, &codec_info, nullptr)
                .error);

  EncodedImage encoded_image_2(encoded_image_1);
  encoded_image_2.SetSpatialIndex(1);
  EXPECT_EQ(EncodedImageCallback::Result::OK,
            test.router()
                ->OnEncodedImage(encoded_image_2, &codec_info, nullptr)
                .error);

  // Inactive.
  test.router()->SetActive(false);
  EXPECT_NE(EncodedImageCallback::Result::OK,
            test.router()
                ->OnEncodedImage(encoded_image_1, &codec_info, nullptr)
                .error);
  EXPECT_NE(EncodedImageCallback::Result::OK,
            test.router()
                ->OnEncodedImage(encoded_image_2, &codec_info, nullptr)
                .error);
}

// Tests how setting individual rtp modules to active affects the overall
// behavior of the payload router. First sets one module to active and checks
// that outgoing data can be sent on this module, and checks that no data can
// be sent if both modules are inactive.
TEST_P(RtpVideoSenderTest, SendSimulcastSetActiveModules) {
  uint8_t payload = 'a';
  EncodedImage encoded_image_1;
  encoded_image_1.SetTimestamp(1);
  encoded_image_1.capture_time_ms_ = 2;
  encoded_image_1._frameType = kVideoFrameKey;
  encoded_image_1._buffer = &payload;
  encoded_image_1._length = 1;
  EncodedImage encoded_image_2(encoded_image_1);
  encoded_image_2.SetSpatialIndex(1);

  RtpVideoSenderTestFixture test({kSsrc1, kSsrc2}, kPayloadType, {});
  CodecSpecificInfo codec_info;
  memset(&codec_info, 0, sizeof(CodecSpecificInfo));
  codec_info.codecType = kVideoCodecVP8;

  // Only setting one stream to active will still set the payload router to
  // active and allow sending data on the active stream.
  std::vector<bool> active_modules({true, false});
  test.router()->SetActiveModules(active_modules);
  EXPECT_EQ(EncodedImageCallback::Result::OK,
            test.router()
                ->OnEncodedImage(encoded_image_1, &codec_info, nullptr)
                .error);

  // Setting both streams to inactive will turn the payload router to
  // inactive.
  active_modules = {false, false};
  test.router()->SetActiveModules(active_modules);
  // An incoming encoded image will not ask the module to send outgoing data
  // because the payload router is inactive.
  EXPECT_NE(EncodedImageCallback::Result::OK,
            test.router()
                ->OnEncodedImage(encoded_image_1, &codec_info, nullptr)
                .error);
  EXPECT_NE(EncodedImageCallback::Result::OK,
            test.router()
                ->OnEncodedImage(encoded_image_1, &codec_info, nullptr)
                .error);
}

TEST_P(RtpVideoSenderTest, CreateWithNoPreviousStates) {
  RtpVideoSenderTestFixture test({kSsrc1, kSsrc2}, kPayloadType, {});
  test.router()->SetActive(true);

  std::map<uint32_t, RtpPayloadState> initial_states =
      test.router()->GetRtpPayloadStates();
  EXPECT_EQ(2u, initial_states.size());
  EXPECT_NE(initial_states.find(kSsrc1), initial_states.end());
  EXPECT_NE(initial_states.find(kSsrc2), initial_states.end());
}

TEST_P(RtpVideoSenderTest, CreateWithPreviousStates) {
  const int64_t kState1SharedFrameId = 123;
  const int64_t kState2SharedFrameId = 234;
  RtpPayloadState state1;
  state1.picture_id = kInitialPictureId1;
  state1.tl0_pic_idx = kInitialTl0PicIdx1;
  state1.shared_frame_id = kState1SharedFrameId;
  RtpPayloadState state2;
  state2.picture_id = kInitialPictureId2;
  state2.tl0_pic_idx = kInitialTl0PicIdx2;
  state2.shared_frame_id = kState2SharedFrameId;
  std::map<uint32_t, RtpPayloadState> states = {{kSsrc1, state1},
                                                {kSsrc2, state2}};

  RtpVideoSenderTestFixture test({kSsrc1, kSsrc2}, kPayloadType, states);
  test.router()->SetActive(true);

  std::map<uint32_t, RtpPayloadState> initial_states =
      test.router()->GetRtpPayloadStates();
  EXPECT_EQ(2u, initial_states.size());
  EXPECT_EQ(kInitialPictureId1, initial_states[kSsrc1].picture_id);
  EXPECT_EQ(kInitialTl0PicIdx1, initial_states[kSsrc1].tl0_pic_idx);
  EXPECT_EQ(kInitialPictureId2, initial_states[kSsrc2].picture_id);
  EXPECT_EQ(kInitialTl0PicIdx2, initial_states[kSsrc2].tl0_pic_idx);
  EXPECT_EQ(kState2SharedFrameId, initial_states[kSsrc1].shared_frame_id);
  EXPECT_EQ(kState2SharedFrameId, initial_states[kSsrc2].shared_frame_id);
}
}  // namespace webrtc
