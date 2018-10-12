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
#include "rtc_base/fakeclock.h"
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
class MockRtpVideoSender : public RtpVideoSenderInterface {
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
  MOCK_METHOD2(DeliverRtcp, void(const uint8_t*, size_t));
  MOCK_METHOD1(OnBitrateAllocationUpdated, void(const VideoBitrateAllocation&));
  MOCK_METHOD3(OnEncodedImage,
               EncodedImageCallback::Result(const EncodedImage&,
                                            const CodecSpecificInfo*,
                                            const RTPFragmentationHeader*));
  MOCK_METHOD1(OnTransportOverheadChanged, void(size_t));
  MOCK_METHOD1(OnOverheadChanged, void(size_t));
  MOCK_METHOD4(OnBitrateUpdated, void(uint32_t, uint8_t, int64_t, int));
  MOCK_CONST_METHOD0(GetPayloadBitrateBps, uint32_t());
  MOCK_CONST_METHOD0(GetProtectionBitrateBps, uint32_t());
  MOCK_METHOD3(SetEncodingData, void(size_t, size_t, size_t));
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
                CreateRtpVideoSender(_, _, _, _, _, _, _, _, _))
        .WillRepeatedly(Return(&rtp_video_sender_));
    EXPECT_CALL(rtp_video_sender_, SetActive(_))
        .WillRepeatedly(testing::Invoke(
            [&](bool active) { rtp_video_sender_active_ = active; }));
    EXPECT_CALL(rtp_video_sender_, IsActive())
        .WillRepeatedly(
            testing::Invoke([&]() { return rtp_video_sender_active_; }));
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
  NiceMock<MockRtpVideoSender> rtp_video_sender_;

  bool rtp_video_sender_active_ = false;
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

TEST_F(VideoSendStreamImplTest, UpdatesObserverOnConfigurationChange) {
  test_queue_.SendTask([this] {
    config_.track_id = "test";
    const bool kSuspend = false;
    config_.suspend_below_min_bitrate = kSuspend;
    config_.rtp.extensions.emplace_back(
        RtpExtension::kTransportSequenceNumberUri, 1);
    auto vss_impl = CreateVideoSendStreamImpl(
        kDefaultInitialBitrateBps, kDefaultBitratePriority,
        VideoEncoderConfig::ContentType::kRealtimeVideo);
    vss_impl->Start();

    // QVGA + VGA configuration matching defaults in media/engine/simulcast.cc.
    VideoStream qvga_stream;
    qvga_stream.width = 320;
    qvga_stream.height = 180;
    qvga_stream.max_framerate = 30;
    qvga_stream.min_bitrate_bps = 30000;
    qvga_stream.target_bitrate_bps = 150000;
    qvga_stream.max_bitrate_bps = 200000;
    qvga_stream.max_qp = 56;
    qvga_stream.bitrate_priority = 1;

    VideoStream vga_stream;
    vga_stream.width = 640;
    vga_stream.height = 360;
    vga_stream.max_framerate = 30;
    vga_stream.min_bitrate_bps = 150000;
    vga_stream.target_bitrate_bps = 500000;
    vga_stream.max_bitrate_bps = 700000;
    vga_stream.max_qp = 56;
    vga_stream.bitrate_priority = 1;

    int min_transmit_bitrate_bps = 30000;

    config_.rtp.ssrcs.emplace_back(1);
    config_.rtp.ssrcs.emplace_back(2);

    EXPECT_CALL(bitrate_allocator_, AddObserver(vss_impl.get(), _))
        .WillOnce(Invoke(
            [&](BitrateAllocatorObserver*, MediaStreamAllocationConfig config) {
              EXPECT_EQ(config.min_bitrate_bps,
                        static_cast<uint32_t>(min_transmit_bitrate_bps));
              EXPECT_EQ(config.max_bitrate_bps,
                        static_cast<uint32_t>(qvga_stream.max_bitrate_bps +
                                              vga_stream.max_bitrate_bps));
              EXPECT_EQ(config.pad_up_bitrate_bps,
                        static_cast<uint32_t>(qvga_stream.target_bitrate_bps +
                                              vga_stream.min_bitrate_bps));
              EXPECT_EQ(config.enforce_min_bitrate, !kSuspend);
              EXPECT_EQ(config.has_packet_feedback, true);
            }));

    static_cast<VideoStreamEncoderInterface::EncoderSink*>(vss_impl.get())
        ->OnEncoderConfigurationChanged(
            std::vector<VideoStream>{qvga_stream, vga_stream},
            min_transmit_bitrate_bps);
    vss_impl->Stop();
  });
}

TEST_F(VideoSendStreamImplTest, UpdatesObserverOnConfigurationChangeWithAlr) {
  test_queue_.SendTask([this] {
    config_.track_id = "test";
    const bool kSuspend = false;
    config_.suspend_below_min_bitrate = kSuspend;
    config_.rtp.extensions.emplace_back(
        RtpExtension::kTransportSequenceNumberUri, 1);
    config_.periodic_alr_bandwidth_probing = true;
    auto vss_impl = CreateVideoSendStreamImpl(
        kDefaultInitialBitrateBps, kDefaultBitratePriority,
        VideoEncoderConfig::ContentType::kScreen);
    vss_impl->Start();

    // Simulcast screenshare.
    VideoStream low_stream;
    low_stream.width = 1920;
    low_stream.height = 1080;
    low_stream.max_framerate = 5;
    low_stream.min_bitrate_bps = 30000;
    low_stream.target_bitrate_bps = 200000;
    low_stream.max_bitrate_bps = 1000000;
    low_stream.num_temporal_layers = 2;
    low_stream.max_qp = 56;
    low_stream.bitrate_priority = 1;

    VideoStream high_stream;
    high_stream.width = 1920;
    high_stream.height = 1080;
    high_stream.max_framerate = 30;
    high_stream.min_bitrate_bps = 60000;
    high_stream.target_bitrate_bps = 1250000;
    high_stream.max_bitrate_bps = 1250000;
    high_stream.num_temporal_layers = 2;
    high_stream.max_qp = 56;
    high_stream.bitrate_priority = 1;

    // With ALR probing, this will be the padding target instead of
    // low_stream.target_bitrate_bps + high_stream.min_bitrate_bps.
    int min_transmit_bitrate_bps = 400000;

    config_.rtp.ssrcs.emplace_back(1);
    config_.rtp.ssrcs.emplace_back(2);

    EXPECT_CALL(bitrate_allocator_, AddObserver(vss_impl.get(), _))
        .WillOnce(Invoke(
            [&](BitrateAllocatorObserver*, MediaStreamAllocationConfig config) {
              EXPECT_EQ(config.min_bitrate_bps,
                        static_cast<uint32_t>(low_stream.min_bitrate_bps));
              EXPECT_EQ(config.max_bitrate_bps,
                        static_cast<uint32_t>(low_stream.max_bitrate_bps +
                                              high_stream.max_bitrate_bps));
              EXPECT_EQ(config.pad_up_bitrate_bps,
                        static_cast<uint32_t>(min_transmit_bitrate_bps));
              EXPECT_EQ(config.enforce_min_bitrate, !kSuspend);
              EXPECT_EQ(config.has_packet_feedback, true);
            }));

    static_cast<VideoStreamEncoderInterface::EncoderSink*>(vss_impl.get())
        ->OnEncoderConfigurationChanged(
            std::vector<VideoStream>{low_stream, high_stream},
            min_transmit_bitrate_bps);
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

TEST_F(VideoSendStreamImplTest, ForwardsVideoBitrateAllocationWhenEnabled) {
  test_queue_.SendTask([this] {
    EXPECT_CALL(transport_controller_, SetPacingFactor(_)).Times(0);
    auto vss_impl = CreateVideoSendStreamImpl(
        kDefaultInitialBitrateBps, kDefaultBitratePriority,
        VideoEncoderConfig::ContentType::kScreen);
    vss_impl->Start();
    VideoBitrateAllocationObserver* const observer =
        static_cast<VideoBitrateAllocationObserver*>(vss_impl.get());

    // Populate a test instance of video bitrate allocation.
    VideoBitrateAllocation alloc;
    alloc.SetBitrate(0, 0, 10000);
    alloc.SetBitrate(0, 1, 20000);
    alloc.SetBitrate(1, 0, 30000);
    alloc.SetBitrate(1, 1, 40000);

    // Encoder starts out paused, don't forward allocation.
    EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc)).Times(0);
    observer->OnBitrateAllocationUpdated(alloc);

    // Unpause encoder, allocation should be passed through.
    const uint32_t kBitrateBps = 100000;
    EXPECT_CALL(rtp_video_sender_, GetPayloadBitrateBps())
        .Times(1)
        .WillOnce(Return(kBitrateBps));
    static_cast<BitrateAllocatorObserver*>(vss_impl.get())
        ->OnBitrateUpdated(kBitrateBps, 0, 0, 0);
    EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc)).Times(1);
    observer->OnBitrateAllocationUpdated(alloc);

    // Pause encoder again, and block allocations.
    EXPECT_CALL(rtp_video_sender_, GetPayloadBitrateBps())
        .Times(1)
        .WillOnce(Return(0));
    static_cast<BitrateAllocatorObserver*>(vss_impl.get())
        ->OnBitrateUpdated(0, 0, 0, 0);
    EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc)).Times(0);
    observer->OnBitrateAllocationUpdated(alloc);

    vss_impl->Stop();
  });
}

TEST_F(VideoSendStreamImplTest, ThrottlesVideoBitrateAllocationWhenTooSimilar) {
  test_queue_.SendTask([this] {
    auto vss_impl = CreateVideoSendStreamImpl(
        kDefaultInitialBitrateBps, kDefaultBitratePriority,
        VideoEncoderConfig::ContentType::kScreen);
    vss_impl->Start();
    // Unpause encoder, to allows allocations to be passed through.
    const uint32_t kBitrateBps = 100000;
    EXPECT_CALL(rtp_video_sender_, GetPayloadBitrateBps())
        .Times(1)
        .WillOnce(Return(kBitrateBps));
    static_cast<BitrateAllocatorObserver*>(vss_impl.get())
        ->OnBitrateUpdated(kBitrateBps, 0, 0, 0);
    VideoBitrateAllocationObserver* const observer =
        static_cast<VideoBitrateAllocationObserver*>(vss_impl.get());

    // Populate a test instance of video bitrate allocation.
    VideoBitrateAllocation alloc;
    alloc.SetBitrate(0, 0, 10000);
    alloc.SetBitrate(0, 1, 20000);
    alloc.SetBitrate(1, 0, 30000);
    alloc.SetBitrate(1, 1, 40000);

    // Initial value.
    EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc)).Times(1);
    observer->OnBitrateAllocationUpdated(alloc);

    VideoBitrateAllocation updated_alloc = alloc;
    // Needs 10% increase in bitrate to trigger immediate forward.
    const uint32_t base_layer_min_update_bitrate_bps =
        alloc.GetBitrate(0, 0) + alloc.get_sum_bps() / 10;

    // Too small increase, don't forward.
    updated_alloc.SetBitrate(0, 0, base_layer_min_update_bitrate_bps - 1);
    EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(_)).Times(0);
    observer->OnBitrateAllocationUpdated(updated_alloc);

    // Large enough increase, do forward.
    updated_alloc.SetBitrate(0, 0, base_layer_min_update_bitrate_bps);
    EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(updated_alloc))
        .Times(1);
    observer->OnBitrateAllocationUpdated(updated_alloc);

    // This is now a decrease compared to last forward allocation, forward
    // immediately.
    updated_alloc.SetBitrate(0, 0, base_layer_min_update_bitrate_bps - 1);
    EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(updated_alloc))
        .Times(1);
    observer->OnBitrateAllocationUpdated(updated_alloc);

    vss_impl->Stop();
  });
}

TEST_F(VideoSendStreamImplTest, ForwardsVideoBitrateAllocationOnLayerChange) {
  test_queue_.SendTask([this] {
    auto vss_impl = CreateVideoSendStreamImpl(
        kDefaultInitialBitrateBps, kDefaultBitratePriority,
        VideoEncoderConfig::ContentType::kScreen);
    vss_impl->Start();
    // Unpause encoder, to allows allocations to be passed through.
    const uint32_t kBitrateBps = 100000;
    EXPECT_CALL(rtp_video_sender_, GetPayloadBitrateBps())
        .Times(1)
        .WillOnce(Return(kBitrateBps));
    static_cast<BitrateAllocatorObserver*>(vss_impl.get())
        ->OnBitrateUpdated(kBitrateBps, 0, 0, 0);
    VideoBitrateAllocationObserver* const observer =
        static_cast<VideoBitrateAllocationObserver*>(vss_impl.get());

    // Populate a test instance of video bitrate allocation.
    VideoBitrateAllocation alloc;
    alloc.SetBitrate(0, 0, 10000);
    alloc.SetBitrate(0, 1, 20000);
    alloc.SetBitrate(1, 0, 30000);
    alloc.SetBitrate(1, 1, 40000);

    // Initial value.
    EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc)).Times(1);
    observer->OnBitrateAllocationUpdated(alloc);

    // Move some bitrate from one layer to a new one, but keep sum the same.
    // Since layout has changed, immediately trigger forward.
    VideoBitrateAllocation updated_alloc = alloc;
    updated_alloc.SetBitrate(2, 0, 10000);
    updated_alloc.SetBitrate(1, 1, alloc.GetBitrate(1, 1) - 10000);
    EXPECT_EQ(alloc.get_sum_bps(), updated_alloc.get_sum_bps());
    EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(updated_alloc))
        .Times(1);
    observer->OnBitrateAllocationUpdated(updated_alloc);

    vss_impl->Stop();
  });
}

TEST_F(VideoSendStreamImplTest, ForwardsVideoBitrateAllocationAfterTimeout) {
  test_queue_.SendTask([this] {
    rtc::ScopedFakeClock fake_clock;
    fake_clock.SetTimeMicros(clock_.TimeInMicroseconds());

    auto vss_impl = CreateVideoSendStreamImpl(
        kDefaultInitialBitrateBps, kDefaultBitratePriority,
        VideoEncoderConfig::ContentType::kScreen);
    vss_impl->Start();
    const uint32_t kBitrateBps = 100000;
    // Unpause encoder, to allows allocations to be passed through.
    EXPECT_CALL(rtp_video_sender_, GetPayloadBitrateBps())
        .Times(1)
        .WillRepeatedly(Return(kBitrateBps));
    static_cast<BitrateAllocatorObserver*>(vss_impl.get())
        ->OnBitrateUpdated(kBitrateBps, 0, 0, 0);
    VideoBitrateAllocationObserver* const observer =
        static_cast<VideoBitrateAllocationObserver*>(vss_impl.get());

    // Populate a test instance of video bitrate allocation.
    VideoBitrateAllocation alloc;
    alloc.SetBitrate(0, 0, 10000);
    alloc.SetBitrate(0, 1, 20000);
    alloc.SetBitrate(1, 0, 30000);
    alloc.SetBitrate(1, 1, 40000);

    EncodedImage encoded_image;
    CodecSpecificInfo codec_specific;
    EXPECT_CALL(rtp_video_sender_, OnEncodedImage(_, _, _))
        .WillRepeatedly(Return(
            EncodedImageCallback::Result(EncodedImageCallback::Result::OK)));

    // Max time we will throttle similar video bitrate allocations.
    static constexpr int64_t kMaxVbaThrottleTimeMs = 500;

    {
      // Initial value.
      EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc))
          .Times(1);
      observer->OnBitrateAllocationUpdated(alloc);
    }

    {
      // Sending same allocation again, this one should be throttled.
      EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc))
          .Times(0);
      observer->OnBitrateAllocationUpdated(alloc);
    }

    fake_clock.AdvanceTimeMicros(kMaxVbaThrottleTimeMs * 1000);

    {
      // Sending similar allocation again after timeout, should forward.
      EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc))
          .Times(1);
      observer->OnBitrateAllocationUpdated(alloc);
    }

    {
      // Sending similar allocation again without timeout, throttle.
      EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc))
          .Times(0);
      observer->OnBitrateAllocationUpdated(alloc);
    }

    {
      // Send encoded image, should be a noop.
      EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc))
          .Times(0);
      static_cast<EncodedImageCallback*>(vss_impl.get())
          ->OnEncodedImage(encoded_image, &codec_specific, nullptr);
    }

    {
      // Advance time and send encoded image, this should wake up and send
      // cached bitrate allocation.
      fake_clock.AdvanceTimeMicros(kMaxVbaThrottleTimeMs * 1000);
      EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc))
          .Times(1);
      static_cast<EncodedImageCallback*>(vss_impl.get())
          ->OnEncodedImage(encoded_image, &codec_specific, nullptr);
    }

    {
      // Advance time and send encoded image, there should be no cached
      // allocation to send.
      fake_clock.AdvanceTimeMicros(kMaxVbaThrottleTimeMs * 1000);
      EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc))
          .Times(0);
      static_cast<EncodedImageCallback*>(vss_impl.get())
          ->OnEncodedImage(encoded_image, &codec_specific, nullptr);
    }

    vss_impl->Stop();
  });
}

}  // namespace internal
}  // namespace webrtc
