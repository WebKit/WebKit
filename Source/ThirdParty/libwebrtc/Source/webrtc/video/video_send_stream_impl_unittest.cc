/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "call/rtp_video_sender.h"
#include "call/test/mock_bitrate_allocator.h"
#include "call/test/mock_rtp_transport_controller_send.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/utility/include/process_thread.h"
#include "modules/video_coding/fec_controller_default.h"
#include "rtc_base/experiments/alr_experiment.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "video/test/mock_video_stream_encoder.h"
#include "video/video_send_stream_impl.h"

namespace webrtc {
namespace internal {
namespace {
using testing::NiceMock;
using testing::StrictMock;
using testing::ReturnRef;
using testing::Return;
using testing::Invoke;
using testing::_;

constexpr int64_t kDefaultInitialBitrateBps = 333000;
const double kDefaultBitratePriority = 0.5;

const float kAlrProbingExperimentPaceMultiplier = 1.0f;
std::string GetAlrProbingExperimentString() {
  return std::string(
             AlrExperimentSettings::kScreenshareProbingBweExperimentName) +
         "/1.0,2875,80,40,-60,3/";
}
class MockPayloadRouter : public RtpVideoSenderInterface {
 public:
  MOCK_METHOD1(RegisterProcessThread, void(ProcessThread*));
  MOCK_METHOD0(DeRegisterProcessThread, void());
  MOCK_METHOD1(SetActive, void(bool));
  MOCK_METHOD1(SetActiveModules, void(const std::vector<bool>));
  MOCK_METHOD0(IsActive, bool());
  MOCK_METHOD1(OnNetworkAvailability, void(bool));
  MOCK_CONST_METHOD0(GetRtpStates, std::map<uint32_t, RtpState>());
  MOCK_CONST_METHOD0(GetRtpPayloadStates,
                     std::map<uint32_t, RtpPayloadState>());
  MOCK_CONST_METHOD0(FecEnabled, bool());
  MOCK_CONST_METHOD0(NackEnabled, bool());
  MOCK_METHOD2(DeliverRtcp, void(const uint8_t*, size_t));
  MOCK_METHOD5(ProtectionRequest,
               void(const FecProtectionParams*,
                    const FecProtectionParams*,
                    uint32_t*,
                    uint32_t*,
                    uint32_t*));
  MOCK_METHOD1(SetMaxRtpPacketSize, void(size_t));
  MOCK_METHOD1(OnBitrateAllocationUpdated, void(const VideoBitrateAllocation&));
  MOCK_METHOD3(OnEncodedImage,
               EncodedImageCallback::Result(const EncodedImage&,
                                            const CodecSpecificInfo*,
                                            const RTPFragmentationHeader*));
};
}  // namespace

class VideoSendStreamImplTest : public ::testing::Test {
 protected:
  VideoSendStreamImplTest()
      : clock_(1000 * 1000 * 1000),
        config_(&transport_),
        send_delay_stats_(&clock_),
        test_queue_("test_queue"),
        process_thread_(ProcessThread::Create("test_thread")),
        call_stats_(&clock_, process_thread_.get()),
        stats_proxy_(&clock_,
                     config_,
                     VideoEncoderConfig::ContentType::kRealtimeVideo) {
    config_.rtp.ssrcs.push_back(8080);
    config_.rtp.payload_type = 1;

    EXPECT_CALL(transport_controller_, keepalive_config())
        .WillRepeatedly(ReturnRef(keepalive_config_));
    EXPECT_CALL(transport_controller_, packet_router())
        .WillRepeatedly(Return(&packet_router_));
    EXPECT_CALL(transport_controller_,
                CreateRtpVideoSender(_, _, _, _, _, _, _, _))
        .WillRepeatedly(Return(&payload_router_));
    EXPECT_CALL(payload_router_, SetActive(_))
        .WillRepeatedly(testing::Invoke(
            [&](bool active) { payload_router_active_ = active; }));
    EXPECT_CALL(payload_router_, IsActive())
        .WillRepeatedly(
            testing::Invoke([&]() { return payload_router_active_; }));
  }
  ~VideoSendStreamImplTest() {}

  std::unique_ptr<VideoSendStreamImpl> CreateVideoSendStreamImpl(
      int initial_encoder_max_bitrate,
      double initial_encoder_bitrate_priority,
      VideoEncoderConfig::ContentType content_type) {
    EXPECT_CALL(bitrate_allocator_, GetStartBitrate(_))
        .WillOnce(Return(123000));
    std::map<uint32_t, RtpState> suspended_ssrcs;
    std::map<uint32_t, RtpPayloadState> suspended_payload_states;
    return absl::make_unique<VideoSendStreamImpl>(
        &stats_proxy_, &test_queue_, &call_stats_, &transport_controller_,
        &bitrate_allocator_, &send_delay_stats_, &video_stream_encoder_,
        &event_log_, &config_, initial_encoder_max_bitrate,
        initial_encoder_bitrate_priority, suspended_ssrcs,
        suspended_payload_states, content_type,
        absl::make_unique<FecControllerDefault>(&clock_));
  }

 protected:
  NiceMock<MockTransport> transport_;
  NiceMock<MockRtpTransportControllerSend> transport_controller_;
  NiceMock<MockBitrateAllocator> bitrate_allocator_;
  NiceMock<MockVideoStreamEncoder> video_stream_encoder_;
  NiceMock<MockPayloadRouter> payload_router_;

  bool payload_router_active_ = false;
  SimulatedClock clock_;
  RtcEventLogNullImpl event_log_;
  VideoSendStream::Config config_;
  SendDelayStats send_delay_stats_;
  rtc::test::TaskQueueForTest test_queue_;
  std::unique_ptr<ProcessThread> process_thread_;
  CallStats call_stats_;
  SendStatisticsProxy stats_proxy_;
  PacketRouter packet_router_;
  RtpKeepAliveConfig keepalive_config_;
};

TEST_F(VideoSendStreamImplTest, RegistersAsBitrateObserverOnStart) {
  test_queue_.SendTask([this] {
    config_.track_id = "test";
    const bool kSuspend = false;
    config_.suspend_below_min_bitrate = kSuspend;
    auto vss_impl = CreateVideoSendStreamImpl(
        kDefaultInitialBitrateBps, kDefaultBitratePriority,
        VideoEncoderConfig::ContentType::kRealtimeVideo);
    EXPECT_CALL(bitrate_allocator_, AddObserver(vss_impl.get(), _))
        .WillOnce(Invoke(
            [&](BitrateAllocatorObserver*, MediaStreamAllocationConfig config) {
              EXPECT_EQ(config.min_bitrate_bps, 0u);
              EXPECT_EQ(config.max_bitrate_bps, kDefaultInitialBitrateBps);
              EXPECT_EQ(config.pad_up_bitrate_bps, 0u);
              EXPECT_EQ(config.enforce_min_bitrate, !kSuspend);
              EXPECT_EQ(config.track_id, "test");
              EXPECT_EQ(config.bitrate_priority, kDefaultBitratePriority);
              EXPECT_EQ(config.has_packet_feedback, false);
            }));
    vss_impl->Start();
    EXPECT_CALL(bitrate_allocator_, RemoveObserver(vss_impl.get())).Times(1);
    vss_impl->Stop();
  });
}

TEST_F(VideoSendStreamImplTest, ReportFeedbackAvailability) {
  test_queue_.SendTask([this] {
    config_.rtp.extensions.emplace_back(
        RtpExtension::kTransportSequenceNumberUri,
        RtpExtension::kTransportSequenceNumberDefaultId);

    auto vss_impl = CreateVideoSendStreamImpl(
        kDefaultInitialBitrateBps, kDefaultBitratePriority,
        VideoEncoderConfig::ContentType::kRealtimeVideo);
    EXPECT_CALL(bitrate_allocator_, AddObserver(vss_impl.get(), _))
        .WillOnce(Invoke(
            [&](BitrateAllocatorObserver*, MediaStreamAllocationConfig config) {
              EXPECT_EQ(config.has_packet_feedback, true);
            }));
    vss_impl->Start();
    EXPECT_CALL(bitrate_allocator_, RemoveObserver(vss_impl.get())).Times(1);
    vss_impl->Stop();
  });
}

TEST_F(VideoSendStreamImplTest, SetsScreensharePacingFactorWithFeedback) {
  test::ScopedFieldTrials alr_experiment(GetAlrProbingExperimentString());

  test_queue_.SendTask([this] {
    config_.rtp.extensions.emplace_back(
        RtpExtension::kTransportSequenceNumberUri,
        RtpExtension::kTransportSequenceNumberDefaultId);
    EXPECT_CALL(transport_controller_,
                SetPacingFactor(kAlrProbingExperimentPaceMultiplier))
        .Times(1);
    auto vss_impl = CreateVideoSendStreamImpl(
        kDefaultInitialBitrateBps, kDefaultBitratePriority,
        VideoEncoderConfig::ContentType::kScreen);
    vss_impl->Start();
    vss_impl->Stop();
  });
}

TEST_F(VideoSendStreamImplTest, DoesNotSetPacingFactorWithoutFeedback) {
  test::ScopedFieldTrials alr_experiment(GetAlrProbingExperimentString());
  test_queue_.SendTask([this] {
    EXPECT_CALL(transport_controller_, SetPacingFactor(_)).Times(0);
    auto vss_impl = CreateVideoSendStreamImpl(
        kDefaultInitialBitrateBps, kDefaultBitratePriority,
        VideoEncoderConfig::ContentType::kScreen);
    vss_impl->Start();
    vss_impl->Stop();
  });
}
}  // namespace internal
}  // namespace webrtc
