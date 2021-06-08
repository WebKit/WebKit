/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_send_stream_impl.h"

#include <algorithm>
#include <memory>
#include <string>

#include "absl/types/optional.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "call/rtp_video_sender.h"
#include "call/test/mock_bitrate_allocator.h"
#include "call/test/mock_rtp_transport_controller_send.h"
#include "modules/rtp_rtcp/source/rtp_sequence_number_map.h"
#include "modules/utility/include/process_thread.h"
#include "modules/video_coding/fec_controller_default.h"
#include "rtc_base/experiments/alr_experiment.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "video/call_stats.h"
#include "video/test/mock_video_stream_encoder.h"

namespace webrtc {

bool operator==(const BitrateAllocationUpdate& a,
                const BitrateAllocationUpdate& b) {
  return a.target_bitrate == b.target_bitrate &&
         a.round_trip_time == b.round_trip_time &&
         a.packet_loss_ratio == b.packet_loss_ratio;
}

namespace internal {
namespace {
using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

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
  MOCK_METHOD(void, RegisterProcessThread, (ProcessThread*), (override));
  MOCK_METHOD(void, DeRegisterProcessThread, (), (override));
  MOCK_METHOD(void, SetActive, (bool), (override));
  MOCK_METHOD(void, SetActiveModules, (const std::vector<bool>), (override));
  MOCK_METHOD(bool, IsActive, (), (override));
  MOCK_METHOD(void, OnNetworkAvailability, (bool), (override));
  MOCK_METHOD((std::map<uint32_t, RtpState>),
              GetRtpStates,
              (),
              (const, override));
  MOCK_METHOD((std::map<uint32_t, RtpPayloadState>),
              GetRtpPayloadStates,
              (),
              (const, override));
  MOCK_METHOD(void, DeliverRtcp, (const uint8_t*, size_t), (override));
  MOCK_METHOD(void,
              OnBitrateAllocationUpdated,
              (const VideoBitrateAllocation&),
              (override));
  MOCK_METHOD(void,
              OnVideoLayersAllocationUpdated,
              (const VideoLayersAllocation&),
              (override));
  MOCK_METHOD(EncodedImageCallback::Result,
              OnEncodedImage,
              (const EncodedImage&, const CodecSpecificInfo*),
              (override));
  MOCK_METHOD(void, OnTransportOverheadChanged, (size_t), (override));
  MOCK_METHOD(void,
              OnBitrateUpdated,
              (BitrateAllocationUpdate, int),
              (override));
  MOCK_METHOD(uint32_t, GetPayloadBitrateBps, (), (const, override));
  MOCK_METHOD(uint32_t, GetProtectionBitrateBps, (), (const, override));
  MOCK_METHOD(void, SetEncodingData, (size_t, size_t, size_t), (override));
  MOCK_METHOD(std::vector<RtpSequenceNumberMap::Info>,
              GetSentRtpPacketInfos,
              (uint32_t ssrc, rtc::ArrayView<const uint16_t> sequence_numbers),
              (const, override));

  MOCK_METHOD(void, SetFecAllowed, (bool fec_allowed), (override));
};

BitrateAllocationUpdate CreateAllocation(int bitrate_bps) {
  BitrateAllocationUpdate update;
  update.target_bitrate = DataRate::BitsPerSec(bitrate_bps);
  update.packet_loss_ratio = 0;
  update.round_trip_time = TimeDelta::Zero();
  return update;
}
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

    EXPECT_CALL(transport_controller_, packet_router())
        .WillRepeatedly(Return(&packet_router_));
    EXPECT_CALL(transport_controller_, CreateRtpVideoSender)
        .WillRepeatedly(Return(&rtp_video_sender_));
    EXPECT_CALL(rtp_video_sender_, SetActive(_))
        .WillRepeatedly(::testing::Invoke(
            [&](bool active) { rtp_video_sender_active_ = active; }));
    EXPECT_CALL(rtp_video_sender_, IsActive())
        .WillRepeatedly(
            ::testing::Invoke([&]() { return rtp_video_sender_active_; }));
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
    return std::make_unique<VideoSendStreamImpl>(
        &clock_, &stats_proxy_, &test_queue_, &call_stats_,
        &transport_controller_, &bitrate_allocator_, &send_delay_stats_,
        &video_stream_encoder_, &event_log_, &config_,
        initial_encoder_max_bitrate, initial_encoder_bitrate_priority,
        suspended_ssrcs, suspended_payload_states, content_type,
        std::make_unique<FecControllerDefault>(&clock_));
  }

 protected:
  NiceMock<MockTransport> transport_;
  NiceMock<MockRtpTransportControllerSend> transport_controller_;
  NiceMock<MockBitrateAllocator> bitrate_allocator_;
  NiceMock<MockVideoStreamEncoder> video_stream_encoder_;
  NiceMock<MockRtpVideoSender> rtp_video_sender_;

  bool rtp_video_sender_active_ = false;
  SimulatedClock clock_;
  RtcEventLogNull event_log_;
  VideoSendStream::Config config_;
  SendDelayStats send_delay_stats_;
  TaskQueueForTest test_queue_;
  std::unique_ptr<ProcessThread> process_thread_;
  // TODO(tommi): Use internal::CallStats
  CallStats call_stats_;
  SendStatisticsProxy stats_proxy_;
  PacketRouter packet_router_;
};

TEST_F(VideoSendStreamImplTest, RegistersAsBitrateObserverOnStart) {
  test_queue_.SendTask(
      [this] {
        const bool kSuspend = false;
        config_.suspend_below_min_bitrate = kSuspend;
        auto vss_impl = CreateVideoSendStreamImpl(
            kDefaultInitialBitrateBps, kDefaultBitratePriority,
            VideoEncoderConfig::ContentType::kRealtimeVideo);
        EXPECT_CALL(bitrate_allocator_, AddObserver(vss_impl.get(), _))
            .WillOnce(Invoke([&](BitrateAllocatorObserver*,
                                 MediaStreamAllocationConfig config) {
              EXPECT_EQ(config.min_bitrate_bps, 0u);
              EXPECT_EQ(config.max_bitrate_bps, kDefaultInitialBitrateBps);
              EXPECT_EQ(config.pad_up_bitrate_bps, 0u);
              EXPECT_EQ(config.enforce_min_bitrate, !kSuspend);
              EXPECT_EQ(config.bitrate_priority, kDefaultBitratePriority);
            }));
        vss_impl->Start();
        EXPECT_CALL(bitrate_allocator_, RemoveObserver(vss_impl.get()))
            .Times(1);
        vss_impl->Stop();
      },
      RTC_FROM_HERE);
}

TEST_F(VideoSendStreamImplTest, UpdatesObserverOnConfigurationChange) {
  test_queue_.SendTask(
      [this] {
        const bool kSuspend = false;
        config_.suspend_below_min_bitrate = kSuspend;
        config_.rtp.extensions.emplace_back(
            RtpExtension::kTransportSequenceNumberUri, 1);
        auto vss_impl = CreateVideoSendStreamImpl(
            kDefaultInitialBitrateBps, kDefaultBitratePriority,
            VideoEncoderConfig::ContentType::kRealtimeVideo);
        vss_impl->Start();

        // QVGA + VGA configuration matching defaults in
        // media/engine/simulcast.cc.
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
            .WillRepeatedly(Invoke([&](BitrateAllocatorObserver*,
                                       MediaStreamAllocationConfig config) {
              EXPECT_EQ(config.min_bitrate_bps,
                        static_cast<uint32_t>(min_transmit_bitrate_bps));
              EXPECT_EQ(config.max_bitrate_bps,
                        static_cast<uint32_t>(qvga_stream.max_bitrate_bps +
                                              vga_stream.max_bitrate_bps));
              if (config.pad_up_bitrate_bps != 0) {
                EXPECT_EQ(config.pad_up_bitrate_bps,
                          static_cast<uint32_t>(qvga_stream.target_bitrate_bps +
                                                vga_stream.min_bitrate_bps));
              }
              EXPECT_EQ(config.enforce_min_bitrate, !kSuspend);
            }));

        static_cast<VideoStreamEncoderInterface::EncoderSink*>(vss_impl.get())
            ->OnEncoderConfigurationChanged(
                std::vector<VideoStream>{qvga_stream, vga_stream}, false,
                VideoEncoderConfig::ContentType::kRealtimeVideo,
                min_transmit_bitrate_bps);
        vss_impl->Stop();
      },
      RTC_FROM_HERE);
}

TEST_F(VideoSendStreamImplTest, UpdatesObserverOnConfigurationChangeWithAlr) {
  test_queue_.SendTask(
      [this] {
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
            .WillRepeatedly(Invoke([&](BitrateAllocatorObserver*,
                                       MediaStreamAllocationConfig config) {
              EXPECT_EQ(config.min_bitrate_bps,
                        static_cast<uint32_t>(low_stream.min_bitrate_bps));
              EXPECT_EQ(config.max_bitrate_bps,
                        static_cast<uint32_t>(low_stream.max_bitrate_bps +
                                              high_stream.max_bitrate_bps));
              if (config.pad_up_bitrate_bps != 0) {
                EXPECT_EQ(config.pad_up_bitrate_bps,
                          static_cast<uint32_t>(min_transmit_bitrate_bps));
              }
              EXPECT_EQ(config.enforce_min_bitrate, !kSuspend);
            }));

        static_cast<VideoStreamEncoderInterface::EncoderSink*>(vss_impl.get())
            ->OnEncoderConfigurationChanged(
                std::vector<VideoStream>{low_stream, high_stream}, false,
                VideoEncoderConfig::ContentType::kScreen,
                min_transmit_bitrate_bps);
        vss_impl->Stop();
      },
      RTC_FROM_HERE);
}

TEST_F(VideoSendStreamImplTest,
       UpdatesObserverOnConfigurationChangeWithSimulcastVideoHysteresis) {
  test::ScopedFieldTrials hysteresis_experiment(
      "WebRTC-VideoRateControl/video_hysteresis:1.25/");

  test_queue_.SendTask(
      [this] {
        auto vss_impl = CreateVideoSendStreamImpl(
            kDefaultInitialBitrateBps, kDefaultBitratePriority,
            VideoEncoderConfig::ContentType::kRealtimeVideo);
        vss_impl->Start();

        // 2-layer video simulcast.
        VideoStream low_stream;
        low_stream.width = 320;
        low_stream.height = 240;
        low_stream.max_framerate = 30;
        low_stream.min_bitrate_bps = 30000;
        low_stream.target_bitrate_bps = 100000;
        low_stream.max_bitrate_bps = 200000;
        low_stream.max_qp = 56;
        low_stream.bitrate_priority = 1;

        VideoStream high_stream;
        high_stream.width = 640;
        high_stream.height = 480;
        high_stream.max_framerate = 30;
        high_stream.min_bitrate_bps = 150000;
        high_stream.target_bitrate_bps = 500000;
        high_stream.max_bitrate_bps = 750000;
        high_stream.max_qp = 56;
        high_stream.bitrate_priority = 1;

        config_.rtp.ssrcs.emplace_back(1);
        config_.rtp.ssrcs.emplace_back(2);

        EXPECT_CALL(bitrate_allocator_, AddObserver(vss_impl.get(), _))
            .WillRepeatedly(Invoke([&](BitrateAllocatorObserver*,
                                       MediaStreamAllocationConfig config) {
              EXPECT_EQ(config.min_bitrate_bps,
                        static_cast<uint32_t>(low_stream.min_bitrate_bps));
              EXPECT_EQ(config.max_bitrate_bps,
                        static_cast<uint32_t>(low_stream.max_bitrate_bps +
                                              high_stream.max_bitrate_bps));
              if (config.pad_up_bitrate_bps != 0) {
                EXPECT_EQ(
                    config.pad_up_bitrate_bps,
                    static_cast<uint32_t>(low_stream.target_bitrate_bps +
                                          1.25 * high_stream.min_bitrate_bps));
              }
            }));

        static_cast<VideoStreamEncoderInterface::EncoderSink*>(vss_impl.get())
            ->OnEncoderConfigurationChanged(
                std::vector<VideoStream>{low_stream, high_stream}, false,
                VideoEncoderConfig::ContentType::kRealtimeVideo,
                /*min_transmit_bitrate_bps=*/0);
        vss_impl->Stop();
      },
      RTC_FROM_HERE);
}

TEST_F(VideoSendStreamImplTest, SetsScreensharePacingFactorWithFeedback) {
  test::ScopedFieldTrials alr_experiment(GetAlrProbingExperimentString());

  test_queue_.SendTask(
      [this] {
        constexpr int kId = 1;
        config_.rtp.extensions.emplace_back(
            RtpExtension::kTransportSequenceNumberUri, kId);
        EXPECT_CALL(transport_controller_,
                    SetPacingFactor(kAlrProbingExperimentPaceMultiplier))
            .Times(1);
        auto vss_impl = CreateVideoSendStreamImpl(
            kDefaultInitialBitrateBps, kDefaultBitratePriority,
            VideoEncoderConfig::ContentType::kScreen);
        vss_impl->Start();
        vss_impl->Stop();
      },
      RTC_FROM_HERE);
}

TEST_F(VideoSendStreamImplTest, DoesNotSetPacingFactorWithoutFeedback) {
  test::ScopedFieldTrials alr_experiment(GetAlrProbingExperimentString());
  test_queue_.SendTask(
      [this] {
        EXPECT_CALL(transport_controller_, SetPacingFactor(_)).Times(0);
        auto vss_impl = CreateVideoSendStreamImpl(
            kDefaultInitialBitrateBps, kDefaultBitratePriority,
            VideoEncoderConfig::ContentType::kScreen);
        vss_impl->Start();
        vss_impl->Stop();
      },
      RTC_FROM_HERE);
}

TEST_F(VideoSendStreamImplTest, ForwardsVideoBitrateAllocationWhenEnabled) {
  test_queue_.SendTask(
      [this] {
        EXPECT_CALL(transport_controller_, SetPacingFactor(_)).Times(0);
        auto vss_impl = CreateVideoSendStreamImpl(
            kDefaultInitialBitrateBps, kDefaultBitratePriority,
            VideoEncoderConfig::ContentType::kScreen);
        VideoStreamEncoderInterface::EncoderSink* const sink =
            static_cast<VideoStreamEncoderInterface::EncoderSink*>(
                vss_impl.get());
        vss_impl->Start();

        // Populate a test instance of video bitrate allocation.
        VideoBitrateAllocation alloc;
        alloc.SetBitrate(0, 0, 10000);
        alloc.SetBitrate(0, 1, 20000);
        alloc.SetBitrate(1, 0, 30000);
        alloc.SetBitrate(1, 1, 40000);

        // Encoder starts out paused, don't forward allocation.
        EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc))
            .Times(0);
        sink->OnBitrateAllocationUpdated(alloc);

        // Unpause encoder, allocation should be passed through.
        const uint32_t kBitrateBps = 100000;
        EXPECT_CALL(rtp_video_sender_, GetPayloadBitrateBps())
            .Times(1)
            .WillOnce(Return(kBitrateBps));
        static_cast<BitrateAllocatorObserver*>(vss_impl.get())
            ->OnBitrateUpdated(CreateAllocation(kBitrateBps));
        EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc))
            .Times(1);
        sink->OnBitrateAllocationUpdated(alloc);

        // Pause encoder again, and block allocations.
        EXPECT_CALL(rtp_video_sender_, GetPayloadBitrateBps())
            .Times(1)
            .WillOnce(Return(0));
        static_cast<BitrateAllocatorObserver*>(vss_impl.get())
            ->OnBitrateUpdated(CreateAllocation(0));
        EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc))
            .Times(0);
        sink->OnBitrateAllocationUpdated(alloc);

        vss_impl->Stop();
      },
      RTC_FROM_HERE);
}

TEST_F(VideoSendStreamImplTest, ThrottlesVideoBitrateAllocationWhenTooSimilar) {
  test_queue_.SendTask(
      [this] {
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
            ->OnBitrateUpdated(CreateAllocation(kBitrateBps));
        VideoStreamEncoderInterface::EncoderSink* const sink =
            static_cast<VideoStreamEncoderInterface::EncoderSink*>(
                vss_impl.get());

        // Populate a test instance of video bitrate allocation.
        VideoBitrateAllocation alloc;
        alloc.SetBitrate(0, 0, 10000);
        alloc.SetBitrate(0, 1, 20000);
        alloc.SetBitrate(1, 0, 30000);
        alloc.SetBitrate(1, 1, 40000);

        // Initial value.
        EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc))
            .Times(1);
        sink->OnBitrateAllocationUpdated(alloc);

        VideoBitrateAllocation updated_alloc = alloc;
        // Needs 10% increase in bitrate to trigger immediate forward.
        const uint32_t base_layer_min_update_bitrate_bps =
            alloc.GetBitrate(0, 0) + alloc.get_sum_bps() / 10;

        // Too small increase, don't forward.
        updated_alloc.SetBitrate(0, 0, base_layer_min_update_bitrate_bps - 1);
        EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(_)).Times(0);
        sink->OnBitrateAllocationUpdated(updated_alloc);

        // Large enough increase, do forward.
        updated_alloc.SetBitrate(0, 0, base_layer_min_update_bitrate_bps);
        EXPECT_CALL(rtp_video_sender_,
                    OnBitrateAllocationUpdated(updated_alloc))
            .Times(1);
        sink->OnBitrateAllocationUpdated(updated_alloc);

        // This is now a decrease compared to last forward allocation, forward
        // immediately.
        updated_alloc.SetBitrate(0, 0, base_layer_min_update_bitrate_bps - 1);
        EXPECT_CALL(rtp_video_sender_,
                    OnBitrateAllocationUpdated(updated_alloc))
            .Times(1);
        sink->OnBitrateAllocationUpdated(updated_alloc);

        vss_impl->Stop();
      },
      RTC_FROM_HERE);
}

TEST_F(VideoSendStreamImplTest, ForwardsVideoBitrateAllocationOnLayerChange) {
  test_queue_.SendTask(
      [this] {
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
            ->OnBitrateUpdated(CreateAllocation(kBitrateBps));
        VideoStreamEncoderInterface::EncoderSink* const sink =
            static_cast<VideoStreamEncoderInterface::EncoderSink*>(
                vss_impl.get());

        // Populate a test instance of video bitrate allocation.
        VideoBitrateAllocation alloc;
        alloc.SetBitrate(0, 0, 10000);
        alloc.SetBitrate(0, 1, 20000);
        alloc.SetBitrate(1, 0, 30000);
        alloc.SetBitrate(1, 1, 40000);

        // Initial value.
        EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc))
            .Times(1);
        sink->OnBitrateAllocationUpdated(alloc);

        // Move some bitrate from one layer to a new one, but keep sum the same.
        // Since layout has changed, immediately trigger forward.
        VideoBitrateAllocation updated_alloc = alloc;
        updated_alloc.SetBitrate(2, 0, 10000);
        updated_alloc.SetBitrate(1, 1, alloc.GetBitrate(1, 1) - 10000);
        EXPECT_EQ(alloc.get_sum_bps(), updated_alloc.get_sum_bps());
        EXPECT_CALL(rtp_video_sender_,
                    OnBitrateAllocationUpdated(updated_alloc))
            .Times(1);
        sink->OnBitrateAllocationUpdated(updated_alloc);

        vss_impl->Stop();
      },
      RTC_FROM_HERE);
}

TEST_F(VideoSendStreamImplTest, ForwardsVideoBitrateAllocationAfterTimeout) {
  test_queue_.SendTask(
      [this] {
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
            ->OnBitrateUpdated(CreateAllocation(kBitrateBps));
        VideoStreamEncoderInterface::EncoderSink* const sink =
            static_cast<VideoStreamEncoderInterface::EncoderSink*>(
                vss_impl.get());

        // Populate a test instance of video bitrate allocation.
        VideoBitrateAllocation alloc;
        alloc.SetBitrate(0, 0, 10000);
        alloc.SetBitrate(0, 1, 20000);
        alloc.SetBitrate(1, 0, 30000);
        alloc.SetBitrate(1, 1, 40000);

        EncodedImage encoded_image;
        CodecSpecificInfo codec_specific;
        EXPECT_CALL(rtp_video_sender_, OnEncodedImage)
            .WillRepeatedly(Return(EncodedImageCallback::Result(
                EncodedImageCallback::Result::OK)));

        // Max time we will throttle similar video bitrate allocations.
        static constexpr int64_t kMaxVbaThrottleTimeMs = 500;

        {
          // Initial value.
          EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc))
              .Times(1);
          sink->OnBitrateAllocationUpdated(alloc);
        }

        {
          // Sending same allocation again, this one should be throttled.
          EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc))
              .Times(0);
          sink->OnBitrateAllocationUpdated(alloc);
        }

        clock_.AdvanceTimeMicroseconds(kMaxVbaThrottleTimeMs * 1000);

        {
          // Sending similar allocation again after timeout, should forward.
          EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc))
              .Times(1);
          sink->OnBitrateAllocationUpdated(alloc);
        }

        {
          // Sending similar allocation again without timeout, throttle.
          EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc))
              .Times(0);
          sink->OnBitrateAllocationUpdated(alloc);
        }

        {
          // Send encoded image, should be a noop.
          EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc))
              .Times(0);
          static_cast<EncodedImageCallback*>(vss_impl.get())
              ->OnEncodedImage(encoded_image, &codec_specific);
        }

        {
          // Advance time and send encoded image, this should wake up and send
          // cached bitrate allocation.
          clock_.AdvanceTimeMicroseconds(kMaxVbaThrottleTimeMs * 1000);
          EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc))
              .Times(1);
          static_cast<EncodedImageCallback*>(vss_impl.get())
              ->OnEncodedImage(encoded_image, &codec_specific);
        }

        {
          // Advance time and send encoded image, there should be no cached
          // allocation to send.
          clock_.AdvanceTimeMicroseconds(kMaxVbaThrottleTimeMs * 1000);
          EXPECT_CALL(rtp_video_sender_, OnBitrateAllocationUpdated(alloc))
              .Times(0);
          static_cast<EncodedImageCallback*>(vss_impl.get())
              ->OnEncodedImage(encoded_image, &codec_specific);
        }

        vss_impl->Stop();
      },
      RTC_FROM_HERE);
}

TEST_F(VideoSendStreamImplTest, CallsVideoStreamEncoderOnBitrateUpdate) {
  test_queue_.SendTask(
      [this] {
        const bool kSuspend = false;
        config_.suspend_below_min_bitrate = kSuspend;
        config_.rtp.extensions.emplace_back(
            RtpExtension::kTransportSequenceNumberUri, 1);
        auto vss_impl = CreateVideoSendStreamImpl(
            kDefaultInitialBitrateBps, kDefaultBitratePriority,
            VideoEncoderConfig::ContentType::kRealtimeVideo);
        vss_impl->Start();

        VideoStream qvga_stream;
        qvga_stream.width = 320;
        qvga_stream.height = 180;
        qvga_stream.max_framerate = 30;
        qvga_stream.min_bitrate_bps = 30000;
        qvga_stream.target_bitrate_bps = 150000;
        qvga_stream.max_bitrate_bps = 200000;
        qvga_stream.max_qp = 56;
        qvga_stream.bitrate_priority = 1;

        int min_transmit_bitrate_bps = 30000;

        config_.rtp.ssrcs.emplace_back(1);

        static_cast<VideoStreamEncoderInterface::EncoderSink*>(vss_impl.get())
            ->OnEncoderConfigurationChanged(
                std::vector<VideoStream>{qvga_stream}, false,
                VideoEncoderConfig::ContentType::kRealtimeVideo,
                min_transmit_bitrate_bps);

        const DataRate network_constrained_rate =
            DataRate::BitsPerSec(qvga_stream.target_bitrate_bps);
        BitrateAllocationUpdate update;
        update.target_bitrate = network_constrained_rate;
        update.stable_target_bitrate = network_constrained_rate;
        update.round_trip_time = TimeDelta::Millis(1);
        EXPECT_CALL(rtp_video_sender_, OnBitrateUpdated(update, _));
        EXPECT_CALL(rtp_video_sender_, GetPayloadBitrateBps())
            .WillOnce(Return(network_constrained_rate.bps()));
        EXPECT_CALL(
            video_stream_encoder_,
            OnBitrateUpdated(network_constrained_rate, network_constrained_rate,
                             network_constrained_rate, 0, _, 0));
        static_cast<BitrateAllocatorObserver*>(vss_impl.get())
            ->OnBitrateUpdated(update);

        // Test allocation where the link allocation is larger than the target,
        // meaning we have some headroom on the link.
        const DataRate qvga_max_bitrate =
            DataRate::BitsPerSec(qvga_stream.max_bitrate_bps);
        const DataRate headroom = DataRate::BitsPerSec(50000);
        const DataRate rate_with_headroom = qvga_max_bitrate + headroom;
        update.target_bitrate = rate_with_headroom;
        update.stable_target_bitrate = rate_with_headroom;
        EXPECT_CALL(rtp_video_sender_, OnBitrateUpdated(update, _));
        EXPECT_CALL(rtp_video_sender_, GetPayloadBitrateBps())
            .WillOnce(Return(rate_with_headroom.bps()));
        EXPECT_CALL(video_stream_encoder_,
                    OnBitrateUpdated(qvga_max_bitrate, qvga_max_bitrate,
                                     rate_with_headroom, 0, _, 0));
        static_cast<BitrateAllocatorObserver*>(vss_impl.get())
            ->OnBitrateUpdated(update);

        // Add protection bitrate to the mix, this should be subtracted from the
        // headroom.
        const uint32_t protection_bitrate_bps = 10000;
        EXPECT_CALL(rtp_video_sender_, GetProtectionBitrateBps())
            .WillOnce(Return(protection_bitrate_bps));

        EXPECT_CALL(rtp_video_sender_, OnBitrateUpdated(update, _));
        EXPECT_CALL(rtp_video_sender_, GetPayloadBitrateBps())
            .WillOnce(Return(rate_with_headroom.bps()));
        const DataRate headroom_minus_protection =
            rate_with_headroom - DataRate::BitsPerSec(protection_bitrate_bps);
        EXPECT_CALL(video_stream_encoder_,
                    OnBitrateUpdated(qvga_max_bitrate, qvga_max_bitrate,
                                     headroom_minus_protection, 0, _, 0));
        static_cast<BitrateAllocatorObserver*>(vss_impl.get())
            ->OnBitrateUpdated(update);

        // Protection bitrate exceeds head room, link allocation should be
        // capped to target bitrate.
        EXPECT_CALL(rtp_video_sender_, GetProtectionBitrateBps())
            .WillOnce(Return(headroom.bps() + 1000));
        EXPECT_CALL(rtp_video_sender_, OnBitrateUpdated(update, _));
        EXPECT_CALL(rtp_video_sender_, GetPayloadBitrateBps())
            .WillOnce(Return(rate_with_headroom.bps()));
        EXPECT_CALL(video_stream_encoder_,
                    OnBitrateUpdated(qvga_max_bitrate, qvga_max_bitrate,
                                     qvga_max_bitrate, 0, _, 0));
        static_cast<BitrateAllocatorObserver*>(vss_impl.get())
            ->OnBitrateUpdated(update);

        // Set rates to zero on stop.
        EXPECT_CALL(video_stream_encoder_,
                    OnBitrateUpdated(DataRate::Zero(), DataRate::Zero(),
                                     DataRate::Zero(), 0, 0, 0));
        vss_impl->Stop();
      },
      RTC_FROM_HERE);
}

TEST_F(VideoSendStreamImplTest, DisablesPaddingOnPausedEncoder) {
  int padding_bitrate = 0;
  std::unique_ptr<VideoSendStreamImpl> vss_impl;

  test_queue_.SendTask(
      [&] {
        vss_impl = CreateVideoSendStreamImpl(
            kDefaultInitialBitrateBps, kDefaultBitratePriority,
            VideoEncoderConfig::ContentType::kRealtimeVideo);

        // Capture padding bitrate for testing.
        EXPECT_CALL(bitrate_allocator_, AddObserver(vss_impl.get(), _))
            .WillRepeatedly(Invoke([&](BitrateAllocatorObserver*,
                                       MediaStreamAllocationConfig config) {
              padding_bitrate = config.pad_up_bitrate_bps;
            }));
        // If observer is removed, no padding will be sent.
        EXPECT_CALL(bitrate_allocator_, RemoveObserver(vss_impl.get()))
            .WillRepeatedly(Invoke(
                [&](BitrateAllocatorObserver*) { padding_bitrate = 0; }));

        EXPECT_CALL(rtp_video_sender_, OnEncodedImage)
            .WillRepeatedly(Return(EncodedImageCallback::Result(
                EncodedImageCallback::Result::OK)));
        const bool kSuspend = false;
        config_.suspend_below_min_bitrate = kSuspend;
        config_.rtp.extensions.emplace_back(
            RtpExtension::kTransportSequenceNumberUri, 1);
        VideoStream qvga_stream;
        qvga_stream.width = 320;
        qvga_stream.height = 180;
        qvga_stream.max_framerate = 30;
        qvga_stream.min_bitrate_bps = 30000;
        qvga_stream.target_bitrate_bps = 150000;
        qvga_stream.max_bitrate_bps = 200000;
        qvga_stream.max_qp = 56;
        qvga_stream.bitrate_priority = 1;

        int min_transmit_bitrate_bps = 30000;

        config_.rtp.ssrcs.emplace_back(1);

        vss_impl->Start();

        // Starts without padding.
        EXPECT_EQ(0, padding_bitrate);

        // Reconfigure e.g. due to a fake frame.
        static_cast<VideoStreamEncoderInterface::EncoderSink*>(vss_impl.get())
            ->OnEncoderConfigurationChanged(
                std::vector<VideoStream>{qvga_stream}, false,
                VideoEncoderConfig::ContentType::kRealtimeVideo,
                min_transmit_bitrate_bps);
        // Still no padding because no actual frames were passed, only
        // reconfiguration happened.
        EXPECT_EQ(0, padding_bitrate);

        // Unpause encoder.
        const uint32_t kBitrateBps = 100000;
        EXPECT_CALL(rtp_video_sender_, GetPayloadBitrateBps())
            .Times(1)
            .WillOnce(Return(kBitrateBps));
        static_cast<BitrateAllocatorObserver*>(vss_impl.get())
            ->OnBitrateUpdated(CreateAllocation(kBitrateBps));

        // A frame is encoded.
        EncodedImage encoded_image;
        CodecSpecificInfo codec_specific;
        static_cast<EncodedImageCallback*>(vss_impl.get())
            ->OnEncodedImage(encoded_image, &codec_specific);
        // Only after actual frame is encoded are we enabling the padding.
        EXPECT_GT(padding_bitrate, 0);
      },
      RTC_FROM_HERE);

  rtc::Event done;
  test_queue_.PostDelayedTask(
      [&] {
        // No padding supposed to be sent for paused observer
        EXPECT_EQ(0, padding_bitrate);
        testing::Mock::VerifyAndClearExpectations(&bitrate_allocator_);
        vss_impl->Stop();
        vss_impl.reset();
        done.Set();
      },
      5000);

  // Pause the test suite so that the last delayed task executes.
  ASSERT_TRUE(done.Wait(10000));
}

TEST_F(VideoSendStreamImplTest, KeepAliveOnDroppedFrame) {
  std::unique_ptr<VideoSendStreamImpl> vss_impl;
  test_queue_.SendTask(
      [&] {
        vss_impl = CreateVideoSendStreamImpl(
            kDefaultInitialBitrateBps, kDefaultBitratePriority,
            VideoEncoderConfig::ContentType::kRealtimeVideo);
        vss_impl->Start();
        const uint32_t kBitrateBps = 100000;
        EXPECT_CALL(rtp_video_sender_, GetPayloadBitrateBps())
            .Times(1)
            .WillOnce(Return(kBitrateBps));
        static_cast<BitrateAllocatorObserver*>(vss_impl.get())
            ->OnBitrateUpdated(CreateAllocation(kBitrateBps));

        // Keep the stream from deallocating by dropping a frame.
        static_cast<EncodedImageCallback*>(vss_impl.get())
            ->OnDroppedFrame(
                EncodedImageCallback::DropReason::kDroppedByEncoder);
        EXPECT_CALL(bitrate_allocator_, RemoveObserver(vss_impl.get()))
            .Times(0);
      },
      RTC_FROM_HERE);

  rtc::Event done;
  test_queue_.PostDelayedTask(
      [&] {
        testing::Mock::VerifyAndClearExpectations(&bitrate_allocator_);
        vss_impl->Stop();
        vss_impl.reset();
        done.Set();
      },
      2000);
  ASSERT_TRUE(done.Wait(5000));
}

TEST_F(VideoSendStreamImplTest, ConfiguresBitratesForSvc) {
  struct TestConfig {
    bool screenshare = false;
    bool alr = false;
    int min_padding_bitrate_bps = 0;
  };

  std::vector<TestConfig> test_variants;
  for (bool screenshare : {false, true}) {
    for (bool alr : {false, true}) {
      for (int min_padding : {0, 400000}) {
        test_variants.push_back({screenshare, alr, min_padding});
      }
    }
  }

  for (const TestConfig& test_config : test_variants) {
    test_queue_.SendTask(
        [this, test_config] {
          const bool kSuspend = false;
          config_.suspend_below_min_bitrate = kSuspend;
          config_.rtp.extensions.emplace_back(
              RtpExtension::kTransportSequenceNumberUri, 1);
          config_.periodic_alr_bandwidth_probing = test_config.alr;
          auto vss_impl = CreateVideoSendStreamImpl(
              kDefaultInitialBitrateBps, kDefaultBitratePriority,
              test_config.screenshare
                  ? VideoEncoderConfig::ContentType::kScreen
                  : VideoEncoderConfig::ContentType::kRealtimeVideo);
          vss_impl->Start();

          // Svc
          VideoStream stream;
          stream.width = 1920;
          stream.height = 1080;
          stream.max_framerate = 30;
          stream.min_bitrate_bps = 60000;
          stream.target_bitrate_bps = 6000000;
          stream.max_bitrate_bps = 1250000;
          stream.num_temporal_layers = 2;
          stream.max_qp = 56;
          stream.bitrate_priority = 1;

          config_.rtp.ssrcs.emplace_back(1);
          config_.rtp.ssrcs.emplace_back(2);

          EXPECT_CALL(
              bitrate_allocator_,
              AddObserver(
                  vss_impl.get(),
                  AllOf(Field(&MediaStreamAllocationConfig::min_bitrate_bps,
                              static_cast<uint32_t>(stream.min_bitrate_bps)),
                        Field(&MediaStreamAllocationConfig::max_bitrate_bps,
                              static_cast<uint32_t>(stream.max_bitrate_bps)),
                        // Stream not yet active - no padding.
                        Field(&MediaStreamAllocationConfig::pad_up_bitrate_bps,
                              0u),
                        Field(&MediaStreamAllocationConfig::enforce_min_bitrate,
                              !kSuspend))));

          static_cast<VideoStreamEncoderInterface::EncoderSink*>(vss_impl.get())
              ->OnEncoderConfigurationChanged(
                  std::vector<VideoStream>{stream}, true,
                  test_config.screenshare
                      ? VideoEncoderConfig::ContentType::kScreen
                      : VideoEncoderConfig::ContentType::kRealtimeVideo,
                  test_config.min_padding_bitrate_bps);
          ::testing::Mock::VerifyAndClearExpectations(&bitrate_allocator_);

          // Simulate an encoded image, this will turn the stream active and
          // enable padding.
          EncodedImage encoded_image;
          CodecSpecificInfo codec_specific;
          EXPECT_CALL(rtp_video_sender_, OnEncodedImage)
              .WillRepeatedly(Return(EncodedImageCallback::Result(
                  EncodedImageCallback::Result::OK)));

          // Screensharing implicitly forces ALR.
          const bool using_alr = test_config.alr || test_config.screenshare;
          // If ALR is used, pads only to min bitrate as rampup is handled by
          // probing. Otherwise target_bitrate contains the padding target.
          const RateControlSettings trials =
              RateControlSettings::ParseFromFieldTrials();
          int expected_padding =
              using_alr
                  ? stream.min_bitrate_bps
                  : static_cast<int>(stream.target_bitrate_bps *
                                     trials.GetSimulcastHysteresisFactor(
                                         test_config.screenshare
                                             ? VideoCodecMode::kScreensharing
                                             : VideoCodecMode::kRealtimeVideo));
          // Min padding bitrate may override padding target.
          expected_padding =
              std::max(expected_padding, test_config.min_padding_bitrate_bps);
          EXPECT_CALL(
              bitrate_allocator_,
              AddObserver(
                  vss_impl.get(),
                  AllOf(Field(&MediaStreamAllocationConfig::min_bitrate_bps,
                              static_cast<uint32_t>(stream.min_bitrate_bps)),
                        Field(&MediaStreamAllocationConfig::max_bitrate_bps,
                              static_cast<uint32_t>(stream.max_bitrate_bps)),
                        // Stream now active - min bitrate use as padding target
                        // when ALR is active.
                        Field(&MediaStreamAllocationConfig::pad_up_bitrate_bps,
                              expected_padding),
                        Field(&MediaStreamAllocationConfig::enforce_min_bitrate,
                              !kSuspend))));
          static_cast<EncodedImageCallback*>(vss_impl.get())
              ->OnEncodedImage(encoded_image, &codec_specific);
          ::testing::Mock::VerifyAndClearExpectations(&bitrate_allocator_);

          vss_impl->Stop();
        },
        RTC_FROM_HERE);
  }
}
}  // namespace internal
}  // namespace webrtc
