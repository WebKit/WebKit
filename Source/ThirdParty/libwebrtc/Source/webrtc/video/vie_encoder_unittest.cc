/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <limits>
#include <utility>

#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/base/fakeclock.h"
#include "webrtc/base/logging.h"
#include "webrtc/media/base/videoadapter.h"
#include "webrtc/modules/video_coding/codecs/vp8/temporal_layers.h"
#include "webrtc/modules/video_coding/utility/default_video_bitrate_allocator.h"
#include "webrtc/system_wrappers/include/metrics_default.h"
#include "webrtc/system_wrappers/include/sleep.h"
#include "webrtc/test/encoder_settings.h"
#include "webrtc/test/fake_encoder.h"
#include "webrtc/test/frame_generator.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/video/send_statistics_proxy.h"
#include "webrtc/video/vie_encoder.h"

namespace {
// TODO(kthelgason): Lower this limit when better testing
// on MediaCodec and fallback implementations are in place.
const int kMinPixelsPerFrame = 320 * 180;
const int kMinFramerateFps = 2;
const int64_t kFrameTimeoutMs = 100;
}  // namespace

namespace webrtc {

using DegredationPreference = VideoSendStream::DegradationPreference;
using ScaleReason = AdaptationObserverInterface::AdaptReason;
using ::testing::_;
using ::testing::Return;

namespace {
const size_t kMaxPayloadLength = 1440;
const int kTargetBitrateBps = 1000000;
const int kLowTargetBitrateBps = kTargetBitrateBps / 10;
const int kMaxInitialFramedrop = 4;
const int kDefaultFramerate = 30;

class TestBuffer : public webrtc::I420Buffer {
 public:
  TestBuffer(rtc::Event* event, int width, int height)
      : I420Buffer(width, height), event_(event) {}

 private:
  friend class rtc::RefCountedObject<TestBuffer>;
  ~TestBuffer() override {
    if (event_)
      event_->Set();
  }
  rtc::Event* const event_;
};

class CpuOveruseDetectorProxy : public OveruseFrameDetector {
 public:
  CpuOveruseDetectorProxy(const CpuOveruseOptions& options,
                          AdaptationObserverInterface* overuse_observer,
                          EncodedFrameObserver* encoder_timing_,
                          CpuOveruseMetricsObserver* metrics_observer)
      : OveruseFrameDetector(options,
                             overuse_observer,
                             encoder_timing_,
                             metrics_observer),
        last_target_framerate_fps_(-1) {}
  virtual ~CpuOveruseDetectorProxy() {}

  void OnTargetFramerateUpdated(int framerate_fps) override {
    rtc::CritScope cs(&lock_);
    last_target_framerate_fps_ = framerate_fps;
    OveruseFrameDetector::OnTargetFramerateUpdated(framerate_fps);
  }

  int GetLastTargetFramerate() {
    rtc::CritScope cs(&lock_);
    return last_target_framerate_fps_;
  }

 private:
  rtc::CriticalSection lock_;
  int last_target_framerate_fps_ GUARDED_BY(lock_);
};

class ViEEncoderUnderTest : public ViEEncoder {
 public:
  ViEEncoderUnderTest(SendStatisticsProxy* stats_proxy,
                      const VideoSendStream::Config::EncoderSettings& settings)
      : ViEEncoder(1 /* number_of_cores */,
                   stats_proxy,
                   settings,
                   nullptr /* pre_encode_callback */,
                   nullptr /* encoder_timing */,
                   std::unique_ptr<OveruseFrameDetector>(
                       overuse_detector_proxy_ = new CpuOveruseDetectorProxy(
                           GetCpuOveruseOptions(settings.full_overuse_time),
                           this,
                           nullptr,
                           stats_proxy))) {}

  void PostTaskAndWait(bool down, AdaptReason reason) {
    rtc::Event event(false, false);
    encoder_queue()->PostTask([this, &event, reason, down] {
      down ? AdaptDown(reason) : AdaptUp(reason);
      event.Set();
    });
    ASSERT_TRUE(event.Wait(5000));
  }

  // This is used as a synchronisation mechanism, to make sure that the
  // encoder queue is not blocked before we start sending it frames.
  void WaitUntilTaskQueueIsIdle() {
    rtc::Event event(false, false);
    encoder_queue()->PostTask([&event] {
      event.Set();
    });
    ASSERT_TRUE(event.Wait(5000));
  }

  void TriggerCpuOveruse() { PostTaskAndWait(true, AdaptReason::kCpu); }

  void TriggerCpuNormalUsage() { PostTaskAndWait(false, AdaptReason::kCpu); }

  void TriggerQualityLow() { PostTaskAndWait(true, AdaptReason::kQuality); }

  void TriggerQualityHigh() { PostTaskAndWait(false, AdaptReason::kQuality); }

  CpuOveruseDetectorProxy* overuse_detector_proxy_;
};

class VideoStreamFactory
    : public VideoEncoderConfig::VideoStreamFactoryInterface {
 public:
  explicit VideoStreamFactory(size_t num_temporal_layers, int framerate)
      : num_temporal_layers_(num_temporal_layers), framerate_(framerate) {
    EXPECT_GT(num_temporal_layers, 0u);
    EXPECT_GT(framerate, 0);
  }

 private:
  std::vector<VideoStream> CreateEncoderStreams(
      int width,
      int height,
      const VideoEncoderConfig& encoder_config) override {
    std::vector<VideoStream> streams =
        test::CreateVideoStreams(width, height, encoder_config);
    for (VideoStream& stream : streams) {
      stream.temporal_layer_thresholds_bps.resize(num_temporal_layers_ - 1);
      stream.max_framerate = framerate_;
    }
    return streams;
  }

  const size_t num_temporal_layers_;
  const int framerate_;
};


class AdaptingFrameForwarder : public test::FrameForwarder {
 public:
  AdaptingFrameForwarder() : adaptation_enabled_(false) {}
  ~AdaptingFrameForwarder() override {}

  void set_adaptation_enabled(bool enabled) {
    rtc::CritScope cs(&crit_);
    adaptation_enabled_ = enabled;
  }

  bool adaption_enabled() const {
    rtc::CritScope cs(&crit_);
    return adaptation_enabled_;
  }

  rtc::VideoSinkWants last_wants() const {
    rtc::CritScope cs(&crit_);
    return last_wants_;
  }

  void IncomingCapturedFrame(const VideoFrame& video_frame) override {
    int cropped_width = 0;
    int cropped_height = 0;
    int out_width = 0;
    int out_height = 0;
    if (adaption_enabled()) {
      if (adapter_.AdaptFrameResolution(
              video_frame.width(), video_frame.height(),
              video_frame.timestamp_us() * 1000, &cropped_width,
              &cropped_height, &out_width, &out_height)) {
        VideoFrame adapted_frame(new rtc::RefCountedObject<TestBuffer>(
                                     nullptr, out_width, out_height),
                                 99, 99, kVideoRotation_0);
        adapted_frame.set_ntp_time_ms(video_frame.ntp_time_ms());
        test::FrameForwarder::IncomingCapturedFrame(adapted_frame);
      }
    } else {
      test::FrameForwarder::IncomingCapturedFrame(video_frame);
    }
  }

  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override {
    rtc::CritScope cs(&crit_);
    last_wants_ = sink_wants();
    adapter_.OnResolutionFramerateRequest(wants.target_pixel_count,
                                          wants.max_pixel_count,
                                          wants.max_framerate_fps);
    test::FrameForwarder::AddOrUpdateSink(sink, wants);
  }
  cricket::VideoAdapter adapter_;
  bool adaptation_enabled_ GUARDED_BY(crit_);
  rtc::VideoSinkWants last_wants_ GUARDED_BY(crit_);
};

class MockableSendStatisticsProxy : public SendStatisticsProxy {
 public:
  MockableSendStatisticsProxy(Clock* clock,
                              const VideoSendStream::Config& config,
                              VideoEncoderConfig::ContentType content_type)
      : SendStatisticsProxy(clock, config, content_type) {}

  VideoSendStream::Stats GetStats() override {
    rtc::CritScope cs(&lock_);
    if (mock_stats_)
      return *mock_stats_;
    return SendStatisticsProxy::GetStats();
  }

  void SetMockStats(const VideoSendStream::Stats& stats) {
    rtc::CritScope cs(&lock_);
    mock_stats_.emplace(stats);
  }

  void ResetMockStats() {
    rtc::CritScope cs(&lock_);
    mock_stats_.reset();
  }

 private:
  rtc::CriticalSection lock_;
  rtc::Optional<VideoSendStream::Stats> mock_stats_ GUARDED_BY(lock_);
};

}  // namespace

class ViEEncoderTest : public ::testing::Test {
 public:
  static const int kDefaultTimeoutMs = 30 * 1000;

  ViEEncoderTest()
      : video_send_config_(VideoSendStream::Config(nullptr)),
        codec_width_(320),
        codec_height_(240),
        fake_encoder_(),
        stats_proxy_(new MockableSendStatisticsProxy(
            Clock::GetRealTimeClock(),
            video_send_config_,
            webrtc::VideoEncoderConfig::ContentType::kRealtimeVideo)),
        sink_(&fake_encoder_) {}

  void SetUp() override {
    metrics::Reset();
    video_send_config_ = VideoSendStream::Config(nullptr);
    video_send_config_.encoder_settings.encoder = &fake_encoder_;
    video_send_config_.encoder_settings.payload_name = "FAKE";
    video_send_config_.encoder_settings.payload_type = 125;

    VideoEncoderConfig video_encoder_config;
    test::FillEncoderConfiguration(1, &video_encoder_config);
    video_encoder_config_ = video_encoder_config.Copy();
    ConfigureEncoder(std::move(video_encoder_config), true /* nack_enabled */);
  }

  void ConfigureEncoder(VideoEncoderConfig video_encoder_config,
                        bool nack_enabled) {
    if (vie_encoder_)
      vie_encoder_->Stop();
    vie_encoder_.reset(new ViEEncoderUnderTest(
        stats_proxy_.get(), video_send_config_.encoder_settings));
    vie_encoder_->SetSink(&sink_, false /* rotation_applied */);
    vie_encoder_->SetSource(
        &video_source_,
        VideoSendStream::DegradationPreference::kMaintainFramerate);
    vie_encoder_->SetStartBitrate(kTargetBitrateBps);
    vie_encoder_->ConfigureEncoder(std::move(video_encoder_config),
                                   kMaxPayloadLength, nack_enabled);
    vie_encoder_->WaitUntilTaskQueueIsIdle();
  }

  void ResetEncoder(const std::string& payload_name,
                    size_t num_streams,
                    size_t num_temporal_layers,
                    bool nack_enabled) {
    video_send_config_.encoder_settings.payload_name = payload_name;

    VideoEncoderConfig video_encoder_config;
    video_encoder_config.number_of_streams = num_streams;
    video_encoder_config.max_bitrate_bps = kTargetBitrateBps;
    video_encoder_config.video_stream_factory =
        new rtc::RefCountedObject<VideoStreamFactory>(num_temporal_layers,
                                                      kDefaultFramerate);
    ConfigureEncoder(std::move(video_encoder_config), nack_enabled);
  }

  VideoFrame CreateFrame(int64_t ntp_time_ms,
                         rtc::Event* destruction_event) const {
    VideoFrame frame(new rtc::RefCountedObject<TestBuffer>(
                         destruction_event, codec_width_, codec_height_),
                     99, 99, kVideoRotation_0);
    frame.set_ntp_time_ms(ntp_time_ms);
    return frame;
  }

  VideoFrame CreateFrame(int64_t ntp_time_ms, int width, int height) const {
    VideoFrame frame(
        new rtc::RefCountedObject<TestBuffer>(nullptr, width, height), 99, 99,
        kVideoRotation_0);
    frame.set_ntp_time_ms(ntp_time_ms);
    frame.set_timestamp_us(ntp_time_ms * 1000);
    return frame;
  }

  void VerifyNoLimitation(const rtc::VideoSinkWants& wants) {
    EXPECT_EQ(std::numeric_limits<int>::max(), wants.max_framerate_fps);
    EXPECT_EQ(std::numeric_limits<int>::max(), wants.max_pixel_count);
    EXPECT_FALSE(wants.target_pixel_count);
  }

  void VerifyFpsEqResolutionEq(const rtc::VideoSinkWants& wants1,
                               const rtc::VideoSinkWants& wants2) {
    EXPECT_EQ(wants1.max_framerate_fps, wants2.max_framerate_fps);
    EXPECT_EQ(wants1.max_pixel_count, wants2.max_pixel_count);
  }

  void VerifyFpsMaxResolutionLt(const rtc::VideoSinkWants& wants1,
                                const rtc::VideoSinkWants& wants2) {
    EXPECT_EQ(std::numeric_limits<int>::max(), wants1.max_framerate_fps);
    EXPECT_LT(wants1.max_pixel_count, wants2.max_pixel_count);
    EXPECT_GT(wants1.max_pixel_count, 0);
  }

  void VerifyFpsMaxResolutionGt(const rtc::VideoSinkWants& wants1,
                                const rtc::VideoSinkWants& wants2) {
    EXPECT_EQ(std::numeric_limits<int>::max(), wants1.max_framerate_fps);
    EXPECT_GT(wants1.max_pixel_count, wants2.max_pixel_count);
  }

  void VerifyFpsMaxResolutionEq(const rtc::VideoSinkWants& wants1,
                                const rtc::VideoSinkWants& wants2) {
    EXPECT_EQ(std::numeric_limits<int>::max(), wants1.max_framerate_fps);
    EXPECT_EQ(wants1.max_pixel_count, wants2.max_pixel_count);
  }

  void VerifyFpsLtResolutionEq(const rtc::VideoSinkWants& wants1,
                               const rtc::VideoSinkWants& wants2) {
    EXPECT_LT(wants1.max_framerate_fps, wants2.max_framerate_fps);
    EXPECT_EQ(wants1.max_pixel_count, wants2.max_pixel_count);
  }

  void VerifyFpsGtResolutionEq(const rtc::VideoSinkWants& wants1,
                               const rtc::VideoSinkWants& wants2) {
    EXPECT_GT(wants1.max_framerate_fps, wants2.max_framerate_fps);
    EXPECT_EQ(wants1.max_pixel_count, wants2.max_pixel_count);
  }

  void VerifyFpsEqResolutionLt(const rtc::VideoSinkWants& wants1,
                               const rtc::VideoSinkWants& wants2) {
    EXPECT_EQ(wants1.max_framerate_fps, wants2.max_framerate_fps);
    EXPECT_LT(wants1.max_pixel_count, wants2.max_pixel_count);
    EXPECT_GT(wants1.max_pixel_count, 0);
  }

  void VerifyFpsEqResolutionGt(const rtc::VideoSinkWants& wants1,
                               const rtc::VideoSinkWants& wants2) {
    EXPECT_EQ(wants1.max_framerate_fps, wants2.max_framerate_fps);
    EXPECT_GT(wants1.max_pixel_count, wants2.max_pixel_count);
  }

  void VerifyFpsMaxResolutionLt(const rtc::VideoSinkWants& wants,
                                int pixel_count) {
    EXPECT_EQ(std::numeric_limits<int>::max(), wants.max_framerate_fps);
    EXPECT_LT(wants.max_pixel_count, pixel_count);
    EXPECT_GT(wants.max_pixel_count, 0);
  }

  void VerifyFpsLtResolutionMax(const rtc::VideoSinkWants& wants, int fps) {
    EXPECT_LT(wants.max_framerate_fps, fps);
    EXPECT_EQ(std::numeric_limits<int>::max(), wants.max_pixel_count);
    EXPECT_FALSE(wants.target_pixel_count);
  }

  void VerifyFpsEqResolutionMax(const rtc::VideoSinkWants& wants,
                                int expected_fps) {
    EXPECT_EQ(expected_fps, wants.max_framerate_fps);
    EXPECT_EQ(std::numeric_limits<int>::max(), wants.max_pixel_count);
    EXPECT_FALSE(wants.target_pixel_count);
  }

  class TestEncoder : public test::FakeEncoder {
   public:
    TestEncoder()
        : FakeEncoder(Clock::GetRealTimeClock()),
          continue_encode_event_(false, false) {}

    VideoCodec codec_config() const {
      rtc::CritScope lock(&crit_sect_);
      return config_;
    }

    void BlockNextEncode() {
      rtc::CritScope lock(&local_crit_sect_);
      block_next_encode_ = true;
    }

    VideoEncoder::ScalingSettings GetScalingSettings() const override {
      rtc::CritScope lock(&local_crit_sect_);
      if (quality_scaling_)
        return VideoEncoder::ScalingSettings(true, 1, 2);
      return VideoEncoder::ScalingSettings(false);
    }

    void ContinueEncode() { continue_encode_event_.Set(); }

    void CheckLastTimeStampsMatch(int64_t ntp_time_ms,
                                  uint32_t timestamp) const {
      rtc::CritScope lock(&local_crit_sect_);
      EXPECT_EQ(timestamp_, timestamp);
      EXPECT_EQ(ntp_time_ms_, ntp_time_ms);
    }

    void SetQualityScaling(bool b) {
      rtc::CritScope lock(&local_crit_sect_);
      quality_scaling_ = b;
    }

    void ForceInitEncodeFailure(bool force_failure) {
      rtc::CritScope lock(&local_crit_sect_);
      force_init_encode_failed_ = force_failure;
    }

   private:
    int32_t Encode(const VideoFrame& input_image,
                   const CodecSpecificInfo* codec_specific_info,
                   const std::vector<FrameType>* frame_types) override {
      bool block_encode;
      {
        rtc::CritScope lock(&local_crit_sect_);
        EXPECT_GT(input_image.timestamp(), timestamp_);
        EXPECT_GT(input_image.ntp_time_ms(), ntp_time_ms_);
        EXPECT_EQ(input_image.timestamp(), input_image.ntp_time_ms() * 90);

        timestamp_ = input_image.timestamp();
        ntp_time_ms_ = input_image.ntp_time_ms();
        last_input_width_ = input_image.width();
        last_input_height_ = input_image.height();
        block_encode = block_next_encode_;
        block_next_encode_ = false;
      }
      int32_t result =
          FakeEncoder::Encode(input_image, codec_specific_info, frame_types);
      if (block_encode)
        EXPECT_TRUE(continue_encode_event_.Wait(kDefaultTimeoutMs));
      return result;
    }

    int32_t InitEncode(const VideoCodec* config,
                       int32_t number_of_cores,
                       size_t max_payload_size) override {
      int res =
          FakeEncoder::InitEncode(config, number_of_cores, max_payload_size);
      rtc::CritScope lock(&local_crit_sect_);
      if (config->codecType == kVideoCodecVP8 && config->VP8().tl_factory) {
        // Simulate setting up temporal layers, in order to validate the life
        // cycle of these objects.
        int num_streams = std::max<int>(1, config->numberOfSimulcastStreams);
        int num_temporal_layers =
            std::max<int>(1, config->VP8().numberOfTemporalLayers);
        for (int i = 0; i < num_streams; ++i) {
          allocated_temporal_layers_.emplace_back(
              config->VP8().tl_factory->Create(i, num_temporal_layers, 42));
        }
      }
      if (force_init_encode_failed_)
        return -1;
      return res;
    }

    rtc::CriticalSection local_crit_sect_;
    bool block_next_encode_ GUARDED_BY(local_crit_sect_) = false;
    rtc::Event continue_encode_event_;
    uint32_t timestamp_ GUARDED_BY(local_crit_sect_) = 0;
    int64_t ntp_time_ms_ GUARDED_BY(local_crit_sect_) = 0;
    int last_input_width_ GUARDED_BY(local_crit_sect_) = 0;
    int last_input_height_ GUARDED_BY(local_crit_sect_) = 0;
    bool quality_scaling_ GUARDED_BY(local_crit_sect_) = true;
    std::vector<std::unique_ptr<TemporalLayers>> allocated_temporal_layers_
        GUARDED_BY(local_crit_sect_);
    bool force_init_encode_failed_ GUARDED_BY(local_crit_sect_) = false;
  };

  class TestSink : public ViEEncoder::EncoderSink {
   public:
    explicit TestSink(TestEncoder* test_encoder)
        : test_encoder_(test_encoder), encoded_frame_event_(false, false) {}

    void WaitForEncodedFrame(int64_t expected_ntp_time) {
      uint32_t timestamp = 0;
      EXPECT_TRUE(encoded_frame_event_.Wait(kDefaultTimeoutMs));
      {
        rtc::CritScope lock(&crit_);
        timestamp = last_timestamp_;
      }
      test_encoder_->CheckLastTimeStampsMatch(expected_ntp_time, timestamp);
    }

    void WaitForEncodedFrame(uint32_t expected_width,
                             uint32_t expected_height) {
      EXPECT_TRUE(encoded_frame_event_.Wait(kDefaultTimeoutMs));
      CheckLastFrameSizeMathces(expected_width, expected_height);
    }

    void CheckLastFrameSizeMathces(uint32_t expected_width,
                                   uint32_t expected_height) {
      uint32_t width = 0;
      uint32_t height = 0;
      {
        rtc::CritScope lock(&crit_);
        width = last_width_;
        height = last_height_;
      }
      EXPECT_EQ(expected_height, height);
      EXPECT_EQ(expected_width, width);
    }

    void ExpectDroppedFrame() { EXPECT_FALSE(encoded_frame_event_.Wait(100)); }

    bool WaitForFrame(int64_t timeout_ms) {
      return encoded_frame_event_.Wait(timeout_ms);
    }

    void SetExpectNoFrames() {
      rtc::CritScope lock(&crit_);
      expect_frames_ = false;
    }

    int number_of_reconfigurations() const {
      rtc::CritScope lock(&crit_);
      return number_of_reconfigurations_;
    }

    int last_min_transmit_bitrate() const {
      rtc::CritScope lock(&crit_);
      return min_transmit_bitrate_bps_;
    }

   private:
    Result OnEncodedImage(
        const EncodedImage& encoded_image,
        const CodecSpecificInfo* codec_specific_info,
        const RTPFragmentationHeader* fragmentation) override {
      rtc::CritScope lock(&crit_);
      EXPECT_TRUE(expect_frames_);
      last_timestamp_ = encoded_image._timeStamp;
      last_width_ = encoded_image._encodedWidth;
      last_height_ = encoded_image._encodedHeight;
      encoded_frame_event_.Set();
      return Result(Result::OK, last_timestamp_);
    }

    void OnEncoderConfigurationChanged(std::vector<VideoStream> streams,
                                       int min_transmit_bitrate_bps) override {
      rtc::CriticalSection crit_;
      ++number_of_reconfigurations_;
      min_transmit_bitrate_bps_ = min_transmit_bitrate_bps;
    }

    rtc::CriticalSection crit_;
    TestEncoder* test_encoder_;
    rtc::Event encoded_frame_event_;
    uint32_t last_timestamp_ = 0;
    uint32_t last_height_ = 0;
    uint32_t last_width_ = 0;
    bool expect_frames_ = true;
    int number_of_reconfigurations_ = 0;
    int min_transmit_bitrate_bps_ = 0;
  };

  VideoSendStream::Config video_send_config_;
  VideoEncoderConfig video_encoder_config_;
  int codec_width_;
  int codec_height_;
  TestEncoder fake_encoder_;
  std::unique_ptr<MockableSendStatisticsProxy> stats_proxy_;
  TestSink sink_;
  AdaptingFrameForwarder video_source_;
  std::unique_ptr<ViEEncoderUnderTest> vie_encoder_;
};

TEST_F(ViEEncoderTest, EncodeOneFrame) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);
  rtc::Event frame_destroyed_event(false, false);
  video_source_.IncomingCapturedFrame(CreateFrame(1, &frame_destroyed_event));
  sink_.WaitForEncodedFrame(1);
  EXPECT_TRUE(frame_destroyed_event.Wait(kDefaultTimeoutMs));
  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, DropsFramesBeforeFirstOnBitrateUpdated) {
  // Dropped since no target bitrate has been set.
  rtc::Event frame_destroyed_event(false, false);
  video_source_.IncomingCapturedFrame(CreateFrame(1, &frame_destroyed_event));
  EXPECT_TRUE(frame_destroyed_event.Wait(kDefaultTimeoutMs));

  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));
  sink_.WaitForEncodedFrame(2);
  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, DropsFramesWhenRateSetToZero) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  sink_.WaitForEncodedFrame(1);

  vie_encoder_->OnBitrateUpdated(0, 0, 0);
  // Dropped since bitrate is zero.
  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));

  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);
  video_source_.IncomingCapturedFrame(CreateFrame(3, nullptr));
  sink_.WaitForEncodedFrame(3);
  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, DropsFramesWithSameOrOldNtpTimestamp) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  sink_.WaitForEncodedFrame(1);

  // This frame will be dropped since it has the same ntp timestamp.
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));

  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));
  sink_.WaitForEncodedFrame(2);
  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, DropsFrameAfterStop) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  sink_.WaitForEncodedFrame(1);

  vie_encoder_->Stop();
  sink_.SetExpectNoFrames();
  rtc::Event frame_destroyed_event(false, false);
  video_source_.IncomingCapturedFrame(CreateFrame(2, &frame_destroyed_event));
  EXPECT_TRUE(frame_destroyed_event.Wait(kDefaultTimeoutMs));
}

TEST_F(ViEEncoderTest, DropsPendingFramesOnSlowEncode) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  fake_encoder_.BlockNextEncode();
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  sink_.WaitForEncodedFrame(1);
  // Here, the encoder thread will be blocked in the TestEncoder waiting for a
  // call to ContinueEncode.
  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));
  video_source_.IncomingCapturedFrame(CreateFrame(3, nullptr));
  fake_encoder_.ContinueEncode();
  sink_.WaitForEncodedFrame(3);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, ConfigureEncoderTriggersOnEncoderConfigurationChanged) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);
  EXPECT_EQ(0, sink_.number_of_reconfigurations());

  // Capture a frame and wait for it to synchronize with the encoder thread.
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  sink_.WaitForEncodedFrame(1);
  // The encoder will have been configured once when the first frame is
  // received.
  EXPECT_EQ(1, sink_.number_of_reconfigurations());

  VideoEncoderConfig video_encoder_config;
  test::FillEncoderConfiguration(1, &video_encoder_config);
  video_encoder_config.min_transmit_bitrate_bps = 9999;
  vie_encoder_->ConfigureEncoder(std::move(video_encoder_config),
                                 kMaxPayloadLength, true /* nack_enabled */);

  // Capture a frame and wait for it to synchronize with the encoder thread.
  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));
  sink_.WaitForEncodedFrame(2);
  EXPECT_EQ(2, sink_.number_of_reconfigurations());
  EXPECT_EQ(9999, sink_.last_min_transmit_bitrate());

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, FrameResolutionChangeReconfigureEncoder) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Capture a frame and wait for it to synchronize with the encoder thread.
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  sink_.WaitForEncodedFrame(1);
  // The encoder will have been configured once.
  EXPECT_EQ(1, sink_.number_of_reconfigurations());
  EXPECT_EQ(codec_width_, fake_encoder_.codec_config().width);
  EXPECT_EQ(codec_height_, fake_encoder_.codec_config().height);

  codec_width_ *= 2;
  codec_height_ *= 2;
  // Capture a frame with a higher resolution and wait for it to synchronize
  // with the encoder thread.
  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));
  sink_.WaitForEncodedFrame(2);
  EXPECT_EQ(codec_width_, fake_encoder_.codec_config().width);
  EXPECT_EQ(codec_height_, fake_encoder_.codec_config().height);
  EXPECT_EQ(2, sink_.number_of_reconfigurations());

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, Vp8ResilienceIsOffFor1S1TLWithNackEnabled) {
  const bool kNackEnabled = true;
  const size_t kNumStreams = 1;
  const size_t kNumTl = 1;
  ResetEncoder("VP8", kNumStreams, kNumTl, kNackEnabled);
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Capture a frame and wait for it to synchronize with the encoder thread.
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  sink_.WaitForEncodedFrame(1);
  // The encoder have been configured once when the first frame is received.
  EXPECT_EQ(1, sink_.number_of_reconfigurations());
  EXPECT_EQ(kVideoCodecVP8, fake_encoder_.codec_config().codecType);
  EXPECT_EQ(kNumStreams, fake_encoder_.codec_config().numberOfSimulcastStreams);
  EXPECT_EQ(kNumTl, fake_encoder_.codec_config().VP8()->numberOfTemporalLayers);
  // Resilience is off for no temporal layers with nack on.
  EXPECT_EQ(kResilienceOff, fake_encoder_.codec_config().VP8()->resilience);
  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, Vp8ResilienceIsOffFor2S1TlWithNackEnabled) {
  const bool kNackEnabled = true;
  const size_t kNumStreams = 2;
  const size_t kNumTl = 1;
  ResetEncoder("VP8", kNumStreams, kNumTl, kNackEnabled);
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Capture a frame and wait for it to synchronize with the encoder thread.
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  sink_.WaitForEncodedFrame(1);
  // The encoder have been configured once when the first frame is received.
  EXPECT_EQ(1, sink_.number_of_reconfigurations());
  EXPECT_EQ(kVideoCodecVP8, fake_encoder_.codec_config().codecType);
  EXPECT_EQ(kNumStreams, fake_encoder_.codec_config().numberOfSimulcastStreams);
  EXPECT_EQ(kNumTl, fake_encoder_.codec_config().VP8()->numberOfTemporalLayers);
  // Resilience is off for no temporal layers and >1 streams with nack on.
  EXPECT_EQ(kResilienceOff, fake_encoder_.codec_config().VP8()->resilience);
  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, Vp8ResilienceIsOnFor1S1TLWithNackDisabled) {
  const bool kNackEnabled = false;
  const size_t kNumStreams = 1;
  const size_t kNumTl = 1;
  ResetEncoder("VP8", kNumStreams, kNumTl, kNackEnabled);
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Capture a frame and wait for it to synchronize with the encoder thread.
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  sink_.WaitForEncodedFrame(1);
  // The encoder have been configured once when the first frame is received.
  EXPECT_EQ(1, sink_.number_of_reconfigurations());
  EXPECT_EQ(kVideoCodecVP8, fake_encoder_.codec_config().codecType);
  EXPECT_EQ(kNumStreams, fake_encoder_.codec_config().numberOfSimulcastStreams);
  EXPECT_EQ(kNumTl, fake_encoder_.codec_config().VP8()->numberOfTemporalLayers);
  // Resilience is on for no temporal layers with nack off.
  EXPECT_EQ(kResilientStream, fake_encoder_.codec_config().VP8()->resilience);
  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, Vp8ResilienceIsOnFor1S2TlWithNackEnabled) {
  const bool kNackEnabled = true;
  const size_t kNumStreams = 1;
  const size_t kNumTl = 2;
  ResetEncoder("VP8", kNumStreams, kNumTl, kNackEnabled);
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Capture a frame and wait for it to synchronize with the encoder thread.
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  sink_.WaitForEncodedFrame(1);
  // The encoder have been configured once when the first frame is received.
  EXPECT_EQ(1, sink_.number_of_reconfigurations());
  EXPECT_EQ(kVideoCodecVP8, fake_encoder_.codec_config().codecType);
  EXPECT_EQ(kNumStreams, fake_encoder_.codec_config().numberOfSimulcastStreams);
  EXPECT_EQ(kNumTl, fake_encoder_.codec_config().VP8()->numberOfTemporalLayers);
  // Resilience is on for temporal layers.
  EXPECT_EQ(kResilientStream, fake_encoder_.codec_config().VP8()->resilience);
  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, SwitchSourceDeregisterEncoderAsSink) {
  EXPECT_TRUE(video_source_.has_sinks());
  test::FrameForwarder new_video_source;
  vie_encoder_->SetSource(
      &new_video_source,
      VideoSendStream::DegradationPreference::kMaintainFramerate);
  EXPECT_FALSE(video_source_.has_sinks());
  EXPECT_TRUE(new_video_source.has_sinks());

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, SinkWantsRotationApplied) {
  EXPECT_FALSE(video_source_.sink_wants().rotation_applied);
  vie_encoder_->SetSink(&sink_, true /*rotation_applied*/);
  EXPECT_TRUE(video_source_.sink_wants().rotation_applied);
  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, SinkWantsFromOveruseDetector) {
  const int kMaxDowngrades = ViEEncoder::kMaxCpuResolutionDowngrades;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  VerifyNoLimitation(video_source_.sink_wants());

  int frame_width = 1280;
  int frame_height = 720;

  // Trigger CPU overuse kMaxCpuDowngrades times. Every time, ViEEncoder should
  // request lower resolution.
  for (int i = 1; i <= kMaxDowngrades; ++i) {
    video_source_.IncomingCapturedFrame(
        CreateFrame(i, frame_width, frame_height));
    sink_.WaitForEncodedFrame(i);

    vie_encoder_->TriggerCpuOveruse();

    EXPECT_FALSE(video_source_.sink_wants().target_pixel_count);
    EXPECT_LT(video_source_.sink_wants().max_pixel_count,
              frame_width * frame_height);
    EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
    EXPECT_EQ(i, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

    frame_width /= 2;
    frame_height /= 2;
  }

  // Trigger CPU overuse one more time. This should not trigger a request for
  // lower resolution.
  rtc::VideoSinkWants current_wants = video_source_.sink_wants();
  video_source_.IncomingCapturedFrame(
      CreateFrame(kMaxDowngrades + 1, frame_width, frame_height));
  sink_.WaitForEncodedFrame(kMaxDowngrades + 1);
  vie_encoder_->TriggerCpuOveruse();
  EXPECT_EQ(video_source_.sink_wants().target_pixel_count,
            current_wants.target_pixel_count);
  EXPECT_EQ(video_source_.sink_wants().max_pixel_count,
            current_wants.max_pixel_count);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(kMaxDowngrades,
            stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger CPU normal use.
  vie_encoder_->TriggerCpuNormalUsage();
  EXPECT_EQ(frame_width * frame_height * 5 / 3,
            video_source_.sink_wants().target_pixel_count.value_or(0));
  EXPECT_EQ(frame_width * frame_height * 4,
            video_source_.sink_wants().max_pixel_count);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(kMaxDowngrades + 1,
            stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, TestMaxCpuResolutionDowngrades_BalancedMode_NoFpsLimit) {
  const int kMaxDowngrades = ViEEncoder::kMaxCpuResolutionDowngrades;
  const int kWidth = 1280;
  const int kHeight = 720;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Enable kBalanced preference, no initial limitation.
  AdaptingFrameForwarder source;
  source.set_adaptation_enabled(true);
  vie_encoder_->SetSource(&source,
                          VideoSendStream::DegradationPreference::kBalanced);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt down kMaxCpuDowngrades times.
  int t = 1;
  for (int i = 1; i <= kMaxDowngrades; ++i) {
    source.IncomingCapturedFrame(CreateFrame(t, kWidth, kHeight));
    sink_.WaitForEncodedFrame(t++);
    vie_encoder_->TriggerCpuOveruse();
    VerifyFpsMaxResolutionLt(source.sink_wants(), source.last_wants());
    EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
    EXPECT_EQ(i, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  }

  // Trigger adapt down, max cpu downgrades reach, expect no change.
  rtc::VideoSinkWants last_wants = source.sink_wants();
  source.IncomingCapturedFrame(CreateFrame(t, kWidth, kHeight));
  sink_.WaitForEncodedFrame(t++);
  vie_encoder_->TriggerCpuOveruse();
  VerifyFpsEqResolutionEq(source.sink_wants(), last_wants);
  EXPECT_EQ(last_wants.max_pixel_count, source.sink_wants().max_pixel_count);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(kMaxDowngrades,
            stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt up kMaxCpuDowngrades times.
  for (int i = 1; i <= kMaxDowngrades; ++i) {
    source.IncomingCapturedFrame(CreateFrame(t, kWidth, kHeight));
    sink_.WaitForEncodedFrame(t++);
    vie_encoder_->TriggerCpuNormalUsage();
    VerifyFpsMaxResolutionGt(source.sink_wants(), source.last_wants());
    EXPECT_GT(source.sink_wants().max_pixel_count, last_wants.max_pixel_count);
    EXPECT_EQ(kMaxDowngrades + i,
              stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  }

  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);

  vie_encoder_->Stop();
}
TEST_F(ViEEncoderTest, SinkWantsStoredByDegradationPreference) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);
  VerifyNoLimitation(video_source_.sink_wants());

  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;
  const int kFrameIntervalMs = 1000 / 30;

  int frame_timestamp = 1;

  video_source_.IncomingCapturedFrame(
      CreateFrame(frame_timestamp, kFrameWidth, kFrameHeight));
  sink_.WaitForEncodedFrame(frame_timestamp);
  frame_timestamp += kFrameIntervalMs;

  // Trigger CPU overuse.
  vie_encoder_->TriggerCpuOveruse();
  video_source_.IncomingCapturedFrame(
      CreateFrame(frame_timestamp, kFrameWidth, kFrameHeight));
  sink_.WaitForEncodedFrame(frame_timestamp);
  frame_timestamp += kFrameIntervalMs;

  // Default degradation preference is maintain-framerate, so will lower max
  // wanted resolution.
  EXPECT_FALSE(video_source_.sink_wants().target_pixel_count);
  EXPECT_LT(video_source_.sink_wants().max_pixel_count,
            kFrameWidth * kFrameHeight);
  EXPECT_EQ(std::numeric_limits<int>::max(),
            video_source_.sink_wants().max_framerate_fps);

  // Set new source, switch to maintain-resolution.
  test::FrameForwarder new_video_source;
  vie_encoder_->SetSource(
      &new_video_source,
      VideoSendStream::DegradationPreference::kMaintainResolution);

  // Initially no degradation registered.
  VerifyNoLimitation(new_video_source.sink_wants());

  // Force an input frame rate to be available, or the adaptation call won't
  // know what framerate to adapt form.
  const int kInputFps = 30;
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  stats.input_frame_rate = kInputFps;
  stats_proxy_->SetMockStats(stats);

  vie_encoder_->TriggerCpuOveruse();
  new_video_source.IncomingCapturedFrame(
      CreateFrame(frame_timestamp, kFrameWidth, kFrameHeight));
  sink_.WaitForEncodedFrame(frame_timestamp);
  frame_timestamp += kFrameIntervalMs;

  // Some framerate constraint should be set.
  EXPECT_FALSE(new_video_source.sink_wants().target_pixel_count);
  EXPECT_EQ(std::numeric_limits<int>::max(),
            new_video_source.sink_wants().max_pixel_count);
  EXPECT_LT(new_video_source.sink_wants().max_framerate_fps, kInputFps);

  // Turn off degradation completely.
  vie_encoder_->SetSource(
      &new_video_source,
      VideoSendStream::DegradationPreference::kDegradationDisabled);
  VerifyNoLimitation(new_video_source.sink_wants());

  vie_encoder_->TriggerCpuOveruse();
  new_video_source.IncomingCapturedFrame(
      CreateFrame(frame_timestamp, kFrameWidth, kFrameHeight));
  sink_.WaitForEncodedFrame(frame_timestamp);
  frame_timestamp += kFrameIntervalMs;

  // Still no degradation.
  VerifyNoLimitation(new_video_source.sink_wants());

  // Calling SetSource with resolution scaling enabled apply the old SinkWants.
  vie_encoder_->SetSource(
      &new_video_source,
      VideoSendStream::DegradationPreference::kMaintainFramerate);
  EXPECT_LT(new_video_source.sink_wants().max_pixel_count,
            kFrameWidth * kFrameHeight);
  EXPECT_FALSE(new_video_source.sink_wants().target_pixel_count);
  EXPECT_EQ(std::numeric_limits<int>::max(),
            new_video_source.sink_wants().max_framerate_fps);

  // Calling SetSource with framerate scaling enabled apply the old SinkWants.
  vie_encoder_->SetSource(
      &new_video_source,
      VideoSendStream::DegradationPreference::kMaintainResolution);
  EXPECT_FALSE(new_video_source.sink_wants().target_pixel_count);
  EXPECT_EQ(std::numeric_limits<int>::max(),
            new_video_source.sink_wants().max_pixel_count);
  EXPECT_LT(new_video_source.sink_wants().max_framerate_fps, kInputFps);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, StatsTracksQualityAdaptationStats) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  const int kWidth = 1280;
  const int kHeight = 720;
  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(1);
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_EQ(0, stats.number_of_quality_adapt_changes);

  // Trigger adapt down.
  vie_encoder_->TriggerQualityLow();
  video_source_.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  sink_.WaitForEncodedFrame(2);

  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.bw_limited_resolution);
  EXPECT_EQ(1, stats.number_of_quality_adapt_changes);

  // Trigger adapt up.
  vie_encoder_->TriggerQualityHigh();
  video_source_.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  sink_.WaitForEncodedFrame(3);

  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_EQ(2, stats.number_of_quality_adapt_changes);
  EXPECT_EQ(0, stats.number_of_cpu_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, StatsTracksCpuAdaptationStats) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  const int kWidth = 1280;
  const int kHeight = 720;
  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(1);
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_EQ(0, stats.number_of_cpu_adapt_changes);

  // Trigger CPU overuse.
  vie_encoder_->TriggerCpuOveruse();
  video_source_.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  sink_.WaitForEncodedFrame(2);

  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Trigger CPU normal use.
  vie_encoder_->TriggerCpuNormalUsage();
  video_source_.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  sink_.WaitForEncodedFrame(3);

  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_EQ(2, stats.number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats.number_of_quality_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, SwitchingSourceKeepsCpuAdaptation) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  const int kWidth = 1280;
  const int kHeight = 720;
  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(1);
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_EQ(0, stats.number_of_cpu_adapt_changes);

  // Trigger CPU overuse.
  vie_encoder_->TriggerCpuOveruse();
  video_source_.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  sink_.WaitForEncodedFrame(2);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Set new source with adaptation still enabled.
  test::FrameForwarder new_video_source;
  vie_encoder_->SetSource(
      &new_video_source,
      VideoSendStream::DegradationPreference::kMaintainFramerate);

  new_video_source.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  sink_.WaitForEncodedFrame(3);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Set adaptation disabled.
  vie_encoder_->SetSource(
      &new_video_source,
      VideoSendStream::DegradationPreference::kDegradationDisabled);

  new_video_source.IncomingCapturedFrame(CreateFrame(4, kWidth, kHeight));
  sink_.WaitForEncodedFrame(4);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Set adaptation back to enabled.
  vie_encoder_->SetSource(
      &new_video_source,
      VideoSendStream::DegradationPreference::kMaintainFramerate);

  new_video_source.IncomingCapturedFrame(CreateFrame(5, kWidth, kHeight));
  sink_.WaitForEncodedFrame(5);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Trigger CPU normal use.
  vie_encoder_->TriggerCpuNormalUsage();
  new_video_source.IncomingCapturedFrame(CreateFrame(6, kWidth, kHeight));
  sink_.WaitForEncodedFrame(6);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_EQ(2, stats.number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats.number_of_quality_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, SwitchingSourceKeepsQualityAdaptation) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  const int kWidth = 1280;
  const int kHeight = 720;
  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(1);
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_FALSE(stats.bw_limited_framerate);
  EXPECT_EQ(0, stats.number_of_quality_adapt_changes);

  // Set new source with adaptation still enabled.
  test::FrameForwarder new_video_source;
  vie_encoder_->SetSource(&new_video_source,
                          VideoSendStream::DegradationPreference::kBalanced);

  new_video_source.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  sink_.WaitForEncodedFrame(2);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_FALSE(stats.bw_limited_framerate);
  EXPECT_EQ(0, stats.number_of_quality_adapt_changes);

  // Trigger adapt down.
  vie_encoder_->TriggerQualityLow();
  new_video_source.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  sink_.WaitForEncodedFrame(3);
  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.bw_limited_resolution);
  EXPECT_FALSE(stats.bw_limited_framerate);
  EXPECT_EQ(1, stats.number_of_quality_adapt_changes);

  // Set new source with adaptation still enabled.
  vie_encoder_->SetSource(&new_video_source,
                          VideoSendStream::DegradationPreference::kBalanced);

  new_video_source.IncomingCapturedFrame(CreateFrame(4, kWidth, kHeight));
  sink_.WaitForEncodedFrame(4);
  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.bw_limited_resolution);
  EXPECT_FALSE(stats.bw_limited_framerate);
  EXPECT_EQ(1, stats.number_of_quality_adapt_changes);

  // Disable resolution scaling.
  vie_encoder_->SetSource(
      &new_video_source,
      VideoSendStream::DegradationPreference::kMaintainResolution);

  new_video_source.IncomingCapturedFrame(CreateFrame(5, kWidth, kHeight));
  sink_.WaitForEncodedFrame(5);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_FALSE(stats.bw_limited_framerate);
  EXPECT_EQ(1, stats.number_of_quality_adapt_changes);
  EXPECT_EQ(0, stats.number_of_cpu_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, QualityAdaptationStatsAreResetWhenScalerIsDisabled) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  const int kWidth = 1280;
  const int kHeight = 720;
  video_source_.set_adaptation_enabled(true);
  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(1);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt down.
  vie_encoder_->TriggerQualityLow();
  video_source_.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  sink_.WaitForEncodedFrame(2);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger overuse.
  vie_encoder_->TriggerCpuOveruse();
  video_source_.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  sink_.WaitForEncodedFrame(3);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Set source with adaptation still enabled but quality scaler is off.
  fake_encoder_.SetQualityScaling(false);
  vie_encoder_->SetSource(
      &video_source_,
      VideoSendStream::DegradationPreference::kMaintainFramerate);

  video_source_.IncomingCapturedFrame(CreateFrame(4, kWidth, kHeight));
  sink_.WaitForEncodedFrame(4);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, StatsTracksCpuAdaptationStatsWhenSwitchingSource) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  const int kWidth = 1280;
  const int kHeight = 720;
  int sequence = 1;

  video_source_.IncomingCapturedFrame(CreateFrame(sequence, kWidth, kHeight));
  sink_.WaitForEncodedFrame(sequence++);
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_framerate);
  EXPECT_EQ(0, stats.number_of_cpu_adapt_changes);

  // Trigger CPU overuse, should now adapt down.
  vie_encoder_->TriggerCpuOveruse();
  video_source_.IncomingCapturedFrame(CreateFrame(sequence, kWidth, kHeight));
  sink_.WaitForEncodedFrame(sequence++);
  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_framerate);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Set new source with adaptation still enabled.
  test::FrameForwarder new_video_source;
  vie_encoder_->SetSource(
      &new_video_source,
      VideoSendStream::DegradationPreference::kMaintainFramerate);

  new_video_source.IncomingCapturedFrame(
      CreateFrame(sequence, kWidth, kHeight));
  sink_.WaitForEncodedFrame(sequence++);
  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_framerate);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Set cpu adaptation by frame dropping.
  vie_encoder_->SetSource(
      &new_video_source,
      VideoSendStream::DegradationPreference::kMaintainResolution);
  new_video_source.IncomingCapturedFrame(
      CreateFrame(sequence, kWidth, kHeight));
  sink_.WaitForEncodedFrame(sequence++);
  stats = stats_proxy_->GetStats();
  // Not adapted at first.
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_framerate);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Force an input frame rate to be available, or the adaptation call won't
  // know what framerate to adapt from.
  VideoSendStream::Stats mock_stats = stats_proxy_->GetStats();
  mock_stats.input_frame_rate = 30;
  stats_proxy_->SetMockStats(mock_stats);
  vie_encoder_->TriggerCpuOveruse();
  stats_proxy_->ResetMockStats();

  new_video_source.IncomingCapturedFrame(
      CreateFrame(sequence, kWidth, kHeight));
  sink_.WaitForEncodedFrame(sequence++);

  // Framerate now adapted.
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_TRUE(stats.cpu_limited_framerate);
  EXPECT_EQ(2, stats.number_of_cpu_adapt_changes);

  // Disable CPU adaptation.
  vie_encoder_->SetSource(
      &new_video_source,
      VideoSendStream::DegradationPreference::kDegradationDisabled);
  new_video_source.IncomingCapturedFrame(
      CreateFrame(sequence, kWidth, kHeight));
  sink_.WaitForEncodedFrame(sequence++);

  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_framerate);
  EXPECT_EQ(2, stats.number_of_cpu_adapt_changes);

  // Try to trigger overuse. Should not succeed.
  stats_proxy_->SetMockStats(mock_stats);
  vie_encoder_->TriggerCpuOveruse();
  stats_proxy_->ResetMockStats();

  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_framerate);
  EXPECT_EQ(2, stats.number_of_cpu_adapt_changes);

  // Switch back the source with resolution adaptation enabled.
  vie_encoder_->SetSource(
      &video_source_,
      VideoSendStream::DegradationPreference::kMaintainFramerate);
  video_source_.IncomingCapturedFrame(CreateFrame(sequence, kWidth, kHeight));
  sink_.WaitForEncodedFrame(sequence++);
  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_framerate);
  EXPECT_EQ(2, stats.number_of_cpu_adapt_changes);

  // Trigger CPU normal usage.
  vie_encoder_->TriggerCpuNormalUsage();
  video_source_.IncomingCapturedFrame(CreateFrame(sequence, kWidth, kHeight));
  sink_.WaitForEncodedFrame(sequence++);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_framerate);
  EXPECT_EQ(3, stats.number_of_cpu_adapt_changes);

  // Back to the source with adaptation off, set it back to maintain-resolution.
  vie_encoder_->SetSource(
      &new_video_source,
      VideoSendStream::DegradationPreference::kMaintainResolution);
  new_video_source.IncomingCapturedFrame(
      CreateFrame(sequence, kWidth, kHeight));
  sink_.WaitForEncodedFrame(sequence++);
  stats = stats_proxy_->GetStats();
  // Disabled, since we previously switched the source to disabled.
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_TRUE(stats.cpu_limited_framerate);
  EXPECT_EQ(3, stats.number_of_cpu_adapt_changes);

  // Trigger CPU normal usage.
  vie_encoder_->TriggerCpuNormalUsage();
  new_video_source.IncomingCapturedFrame(
      CreateFrame(sequence, kWidth, kHeight));
  sink_.WaitForEncodedFrame(sequence++);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_framerate);
  EXPECT_EQ(4, stats.number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats.number_of_quality_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, StatsTracksPreferredBitrate) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  const int kWidth = 1280;
  const int kHeight = 720;
  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(1);

  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  EXPECT_EQ(video_encoder_config_.max_bitrate_bps,
            stats.preferred_media_bitrate_bps);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, ScalingUpAndDownDoesNothingWithMaintainResolution) {
  const int kWidth = 1280;
  const int kHeight = 720;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Expect no scaling to begin with.
  VerifyNoLimitation(video_source_.sink_wants());

  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(1);

  // Trigger scale down.
  vie_encoder_->TriggerQualityLow();

  video_source_.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  sink_.WaitForEncodedFrame(2);

  // Expect a scale down.
  EXPECT_TRUE(video_source_.sink_wants().max_pixel_count);
  EXPECT_LT(video_source_.sink_wants().max_pixel_count, kWidth * kHeight);

  // Set resolution scaling disabled.
  test::FrameForwarder new_video_source;
  vie_encoder_->SetSource(
      &new_video_source,
      VideoSendStream::DegradationPreference::kMaintainResolution);

  // Trigger scale down.
  vie_encoder_->TriggerQualityLow();
  new_video_source.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  sink_.WaitForEncodedFrame(3);

  // Expect no scaling.
  EXPECT_EQ(std::numeric_limits<int>::max(),
            new_video_source.sink_wants().max_pixel_count);

  // Trigger scale up.
  vie_encoder_->TriggerQualityHigh();
  new_video_source.IncomingCapturedFrame(CreateFrame(4, kWidth, kHeight));
  sink_.WaitForEncodedFrame(4);

  // Expect nothing to change, still no scaling.
  EXPECT_EQ(std::numeric_limits<int>::max(),
            new_video_source.sink_wants().max_pixel_count);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, SkipsSameAdaptDownRequest_MaintainFramerateMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Enable kMaintainFramerate preference, no initial limitation.
  test::FrameForwarder source;
  vie_encoder_->SetSource(
      &source, VideoSendStream::DegradationPreference::kMaintainFramerate);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(1);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt down, expect scaled down resolution.
  vie_encoder_->TriggerCpuOveruse();
  VerifyFpsMaxResolutionLt(source.sink_wants(), kWidth * kHeight);
  const int kLastMaxPixelCount = source.sink_wants().max_pixel_count;
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt down for same input resolution, expect no change.
  vie_encoder_->TriggerCpuOveruse();
  EXPECT_EQ(kLastMaxPixelCount, source.sink_wants().max_pixel_count);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, SkipsSameOrLargerAdaptDownRequest_BalancedMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Enable kBalanced preference, no initial limitation.
  test::FrameForwarder source;
  vie_encoder_->SetSource(&source,
                          VideoSendStream::DegradationPreference::kBalanced);
  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(1);
  VerifyNoLimitation(source.sink_wants());

  // Trigger adapt down, expect scaled down resolution.
  vie_encoder_->TriggerQualityLow();
  VerifyFpsMaxResolutionLt(source.sink_wants(), kWidth * kHeight);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);
  const int kLastMaxPixelCount = source.sink_wants().max_pixel_count;

  // Trigger adapt down for same input resolution, expect no change.
  source.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  sink_.WaitForEncodedFrame(2);
  vie_encoder_->TriggerQualityLow();
  EXPECT_EQ(kLastMaxPixelCount, source.sink_wants().max_pixel_count);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down for larger input resolution, expect no change.
  source.IncomingCapturedFrame(CreateFrame(3, kWidth + 1, kHeight + 1));
  sink_.WaitForEncodedFrame(3);
  vie_encoder_->TriggerQualityLow();
  EXPECT_EQ(kLastMaxPixelCount, source.sink_wants().max_pixel_count);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, NoChangeForInitialNormalUsage_MaintainFramerateMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Enable kMaintainFramerate preference, no initial limitation.
  test::FrameForwarder source;
  vie_encoder_->SetSource(
      &source, VideoSendStream::DegradationPreference::kMaintainFramerate);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt up, expect no change.
  vie_encoder_->TriggerCpuNormalUsage();
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, NoChangeForInitialNormalUsage_MaintainResolutionMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Enable kMaintainResolution preference, no initial limitation.
  test::FrameForwarder source;
  vie_encoder_->SetSource(
      &source, VideoSendStream::DegradationPreference::kMaintainResolution);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt up, expect no change.
  vie_encoder_->TriggerCpuNormalUsage();
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, NoChangeForInitialNormalUsage_BalancedMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Enable kBalanced preference, no initial limitation.
  test::FrameForwarder source;
  vie_encoder_->SetSource(&source,
                          VideoSendStream::DegradationPreference::kBalanced);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no change.
  vie_encoder_->TriggerQualityHigh();
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, NoChangeForInitialNormalUsage_DisabledMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Enable kDegradationDisabled preference, no initial limitation.
  test::FrameForwarder source;
  vie_encoder_->SetSource(
      &source, VideoSendStream::DegradationPreference::kDegradationDisabled);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no change.
  vie_encoder_->TriggerQualityHigh();
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, AdaptsResolutionForLowQuality_MaintainFramerateMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Enable kMaintainFramerate preference, no initial limitation.
  AdaptingFrameForwarder source;
  source.set_adaptation_enabled(true);
  vie_encoder_->SetSource(
      &source, VideoSendStream::DegradationPreference::kMaintainFramerate);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(1);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution.
  vie_encoder_->TriggerQualityLow();
  source.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  sink_.WaitForEncodedFrame(2);
  VerifyFpsMaxResolutionLt(source.sink_wants(), kWidth * kHeight);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no restriction.
  vie_encoder_->TriggerQualityHigh();
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, AdaptsFramerateForLowQuality_MaintainResolutionMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  const int kInputFps = 30;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  stats.input_frame_rate = kInputFps;
  stats_proxy_->SetMockStats(stats);

  // Expect no scaling to begin with (preference: kMaintainFramerate).
  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(1);
  VerifyNoLimitation(video_source_.sink_wants());

  // Trigger adapt down, expect scaled down resolution.
  vie_encoder_->TriggerQualityLow();
  video_source_.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  sink_.WaitForEncodedFrame(2);
  VerifyFpsMaxResolutionLt(video_source_.sink_wants(), kWidth * kHeight);

  // Enable kMaintainResolution preference.
  test::FrameForwarder new_video_source;
  vie_encoder_->SetSource(
      &new_video_source,
      VideoSendStream::DegradationPreference::kMaintainResolution);
  VerifyNoLimitation(new_video_source.sink_wants());

  // Trigger adapt down, expect reduced framerate.
  vie_encoder_->TriggerQualityLow();
  new_video_source.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  sink_.WaitForEncodedFrame(3);
  VerifyFpsLtResolutionMax(new_video_source.sink_wants(), kInputFps);

  // Trigger adapt up, expect no restriction.
  vie_encoder_->TriggerQualityHigh();
  VerifyNoLimitation(new_video_source.sink_wants());

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, DoesNotScaleBelowSetResolutionLimit) {
  const int kWidth = 1280;
  const int kHeight = 720;
  const size_t kNumFrames = 10;

  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Enable adapter, expected input resolutions when downscaling:
  // 1280x720 -> 960x540 -> 640x360 -> 480x270 -> 320x180 (min resolution limit)
  video_source_.set_adaptation_enabled(true);

  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  int downscales = 0;
  for (size_t i = 1; i <= kNumFrames; i++) {
    video_source_.IncomingCapturedFrame(CreateFrame(i, kWidth, kHeight));
    sink_.WaitForEncodedFrame(i);

    // Trigger scale down.
    rtc::VideoSinkWants last_wants = video_source_.sink_wants();
    vie_encoder_->TriggerQualityLow();
    EXPECT_GE(video_source_.sink_wants().max_pixel_count, kMinPixelsPerFrame);

    if (video_source_.sink_wants().max_pixel_count < last_wants.max_pixel_count)
      ++downscales;

    EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
    EXPECT_EQ(downscales,
              stats_proxy_->GetStats().number_of_quality_adapt_changes);
    EXPECT_GT(downscales, 0);
  }
  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest,
       AdaptsResolutionUpAndDownTwiceOnOveruse_MaintainFramerateMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Enable kMaintainFramerate preference, no initial limitation.
  AdaptingFrameForwarder source;
  source.set_adaptation_enabled(true);
  vie_encoder_->SetSource(
      &source, VideoSendStream::DegradationPreference::kMaintainFramerate);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt down, expect scaled down resolution.
  vie_encoder_->TriggerCpuOveruse();
  source.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  sink_.WaitForEncodedFrame(2);
  VerifyFpsMaxResolutionLt(source.sink_wants(), kWidth * kHeight);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt up, expect no restriction.
  vie_encoder_->TriggerCpuNormalUsage();
  source.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt down, expect scaled down resolution.
  vie_encoder_->TriggerCpuOveruse();
  source.IncomingCapturedFrame(CreateFrame(4, kWidth, kHeight));
  sink_.WaitForEncodedFrame(4);
  VerifyFpsMaxResolutionLt(source.sink_wants(), kWidth * kHeight);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt up, expect no restriction.
  vie_encoder_->TriggerCpuNormalUsage();
  source.IncomingCapturedFrame(CreateFrame(5, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest,
       AdaptsResolutionUpAndDownTwiceForLowQuality_BalancedMode_NoFpsLimit) {
  const int kWidth = 1280;
  const int kHeight = 720;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Enable kBalanced preference, no initial limitation.
  AdaptingFrameForwarder source;
  source.set_adaptation_enabled(true);
  vie_encoder_->SetSource(&source,
                          VideoSendStream::DegradationPreference::kBalanced);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution.
  vie_encoder_->TriggerQualityLow();
  source.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  sink_.WaitForEncodedFrame(2);
  VerifyFpsMaxResolutionLt(source.sink_wants(), kWidth * kHeight);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no restriction.
  vie_encoder_->TriggerQualityHigh();
  source.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution.
  vie_encoder_->TriggerQualityLow();
  source.IncomingCapturedFrame(CreateFrame(4, kWidth, kHeight));
  sink_.WaitForEncodedFrame(4);
  VerifyFpsMaxResolutionLt(source.sink_wants(), kWidth * kHeight);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no restriction.
  vie_encoder_->TriggerQualityHigh();
  source.IncomingCapturedFrame(CreateFrame(5, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest,
       AdaptsResolutionOnOveruseAndLowQuality_MaintainFramerateMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Enable kMaintainFramerate preference, no initial limitation.
  AdaptingFrameForwarder source;
  source.set_adaptation_enabled(true);
  vie_encoder_->SetSource(
      &source, VideoSendStream::DegradationPreference::kMaintainFramerate);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt down, expect scaled down resolution (960x540).
  vie_encoder_->TriggerCpuOveruse();
  source.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  sink_.WaitForEncodedFrame(2);
  VerifyFpsMaxResolutionLt(source.sink_wants(), kWidth * kHeight);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt down, expect scaled down resolution (640x360).
  vie_encoder_->TriggerCpuOveruse();
  source.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  sink_.WaitForEncodedFrame(3);
  VerifyFpsMaxResolutionLt(source.sink_wants(), source.last_wants());
  rtc::VideoSinkWants last_wants = source.sink_wants();
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt down, max cpu downgrades reached, expect no change.
  vie_encoder_->TriggerCpuOveruse();
  source.IncomingCapturedFrame(CreateFrame(4, kWidth, kHeight));
  sink_.WaitForEncodedFrame(4);
  VerifyFpsEqResolutionEq(source.sink_wants(), last_wants);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality adapt down, expect scaled down resolution (480x270).
  vie_encoder_->TriggerQualityLow();
  source.IncomingCapturedFrame(CreateFrame(5, kWidth, kHeight));
  sink_.WaitForEncodedFrame(5);
  VerifyFpsMaxResolutionLt(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt up, expect upscaled resolution (640x360).
  vie_encoder_->TriggerCpuNormalUsage();
  source.IncomingCapturedFrame(CreateFrame(6, kWidth, kHeight));
  sink_.WaitForEncodedFrame(6);
  VerifyFpsMaxResolutionGt(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt up, expect upscaled resolution (960x540).
  vie_encoder_->TriggerCpuNormalUsage();
  source.IncomingCapturedFrame(CreateFrame(7, kWidth, kHeight));
  sink_.WaitForEncodedFrame(7);
  VerifyFpsMaxResolutionGt(source.sink_wants(), source.last_wants());
  last_wants = source.sink_wants();
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt up, no cpu downgrades, expect no change (960x540).
  vie_encoder_->TriggerCpuNormalUsage();
  source.IncomingCapturedFrame(CreateFrame(8, kWidth, kHeight));
  sink_.WaitForEncodedFrame(8);
  VerifyFpsEqResolutionEq(source.sink_wants(), last_wants);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality adapt up, expect no restriction (1280x720).
  vie_encoder_->TriggerQualityHigh();
  source.IncomingCapturedFrame(CreateFrame(9, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionGt(source.sink_wants(), source.last_wants());
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, CpuLimitedHistogramIsReported) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);
  const int kWidth = 640;
  const int kHeight = 360;

  for (int i = 1; i <= SendStatisticsProxy::kMinRequiredMetricsSamples; ++i) {
    video_source_.IncomingCapturedFrame(CreateFrame(i, kWidth, kHeight));
    sink_.WaitForEncodedFrame(i);
  }

  vie_encoder_->TriggerCpuOveruse();
  for (int i = 1; i <= SendStatisticsProxy::kMinRequiredMetricsSamples; ++i) {
    video_source_.IncomingCapturedFrame(CreateFrame(
        SendStatisticsProxy::kMinRequiredMetricsSamples + i, kWidth, kHeight));
    sink_.WaitForEncodedFrame(SendStatisticsProxy::kMinRequiredMetricsSamples +
                              i);
  }

  vie_encoder_->Stop();
  vie_encoder_.reset();
  stats_proxy_.reset();

  EXPECT_EQ(1,
            metrics::NumSamples("WebRTC.Video.CpuLimitedResolutionInPercent"));
  EXPECT_EQ(
      1, metrics::NumEvents("WebRTC.Video.CpuLimitedResolutionInPercent", 50));
}

TEST_F(ViEEncoderTest, CpuLimitedHistogramIsNotReportedForDisabledDegradation) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);
  const int kWidth = 640;
  const int kHeight = 360;

  vie_encoder_->SetSource(
      &video_source_,
      VideoSendStream::DegradationPreference::kDegradationDisabled);

  for (int i = 1; i <= SendStatisticsProxy::kMinRequiredMetricsSamples; ++i) {
    video_source_.IncomingCapturedFrame(CreateFrame(i, kWidth, kHeight));
    sink_.WaitForEncodedFrame(i);
  }

  vie_encoder_->Stop();
  vie_encoder_.reset();
  stats_proxy_.reset();

  EXPECT_EQ(0,
            metrics::NumSamples("WebRTC.Video.CpuLimitedResolutionInPercent"));
}

TEST_F(ViEEncoderTest, CallsBitrateObserver) {
  class MockBitrateObserver : public VideoBitrateAllocationObserver {
   public:
    MOCK_METHOD1(OnBitrateAllocationUpdated, void(const BitrateAllocation&));
  } bitrate_observer;
  vie_encoder_->SetBitrateObserver(&bitrate_observer);

  const int kDefaultFps = 30;
  const BitrateAllocation expected_bitrate =
      DefaultVideoBitrateAllocator(fake_encoder_.codec_config())
          .GetAllocation(kLowTargetBitrateBps, kDefaultFps);

  // First called on bitrate updated, then again on first frame.
  EXPECT_CALL(bitrate_observer, OnBitrateAllocationUpdated(expected_bitrate))
      .Times(2);
  vie_encoder_->OnBitrateUpdated(kLowTargetBitrateBps, 0, 0);

  const int64_t kStartTimeMs = 1;
  video_source_.IncomingCapturedFrame(
      CreateFrame(kStartTimeMs, codec_width_, codec_height_));
  sink_.WaitForEncodedFrame(kStartTimeMs);

  // Not called on second frame.
  EXPECT_CALL(bitrate_observer, OnBitrateAllocationUpdated(expected_bitrate))
      .Times(0);
  video_source_.IncomingCapturedFrame(
      CreateFrame(kStartTimeMs + 1, codec_width_, codec_height_));
  sink_.WaitForEncodedFrame(kStartTimeMs + 1);

  // Called after a process interval.
  const int64_t kProcessIntervalMs =
      vcm::VCMProcessTimer::kDefaultProcessIntervalMs;
  // TODO(sprang): ViEEncoder should die and/or get injectable clock.
  // Sleep for one processing interval plus one frame to avoid flakiness.
  SleepMs(kProcessIntervalMs + 1000 / kDefaultFps);
  EXPECT_CALL(bitrate_observer, OnBitrateAllocationUpdated(expected_bitrate))
      .Times(1);
  video_source_.IncomingCapturedFrame(CreateFrame(
      kStartTimeMs + kProcessIntervalMs, codec_width_, codec_height_));
  sink_.WaitForEncodedFrame(kStartTimeMs + kProcessIntervalMs);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, OveruseDetectorUpdatedOnReconfigureAndAdaption) {
  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;
  const int kFramerate = 24;

  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);
  test::FrameForwarder source;
  vie_encoder_->SetSource(
      &source, VideoSendStream::DegradationPreference::kMaintainResolution);

  // Insert a single frame, triggering initial configuration.
  source.IncomingCapturedFrame(CreateFrame(1, kFrameWidth, kFrameHeight));
  vie_encoder_->WaitUntilTaskQueueIsIdle();

  EXPECT_EQ(vie_encoder_->overuse_detector_proxy_->GetLastTargetFramerate(),
            kDefaultFramerate);

  // Trigger reconfigure encoder (without resetting the entire instance).
  VideoEncoderConfig video_encoder_config;
  video_encoder_config.max_bitrate_bps = kTargetBitrateBps;
  video_encoder_config.number_of_streams = 1;
  video_encoder_config.video_stream_factory =
      new rtc::RefCountedObject<VideoStreamFactory>(1, kFramerate);
  vie_encoder_->ConfigureEncoder(std::move(video_encoder_config),
                                 kMaxPayloadLength, false);
  vie_encoder_->WaitUntilTaskQueueIsIdle();

  // Detector should be updated with fps limit from codec config.
  EXPECT_EQ(vie_encoder_->overuse_detector_proxy_->GetLastTargetFramerate(),
            kFramerate);

  // Trigger overuse, max framerate should be reduced.
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  stats.input_frame_rate = kFramerate;
  stats_proxy_->SetMockStats(stats);
  vie_encoder_->TriggerCpuOveruse();
  vie_encoder_->WaitUntilTaskQueueIsIdle();
  int adapted_framerate =
      vie_encoder_->overuse_detector_proxy_->GetLastTargetFramerate();
  EXPECT_LT(adapted_framerate, kFramerate);

  // Trigger underuse, max framerate should go back to codec configured fps.
  // Set extra low fps, to make sure it's actually reset, not just incremented.
  stats = stats_proxy_->GetStats();
  stats.input_frame_rate = adapted_framerate / 2;
  stats_proxy_->SetMockStats(stats);
  vie_encoder_->TriggerCpuNormalUsage();
  vie_encoder_->WaitUntilTaskQueueIsIdle();
  EXPECT_EQ(vie_encoder_->overuse_detector_proxy_->GetLastTargetFramerate(),
            kFramerate);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, OveruseDetectorUpdatedRespectsFramerateAfterUnderuse) {
  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;
  const int kLowFramerate = 15;
  const int kHighFramerate = 25;

  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);
  test::FrameForwarder source;
  vie_encoder_->SetSource(
      &source, VideoSendStream::DegradationPreference::kMaintainResolution);

  // Trigger initial configuration.
  VideoEncoderConfig video_encoder_config;
  video_encoder_config.max_bitrate_bps = kTargetBitrateBps;
  video_encoder_config.number_of_streams = 1;
  video_encoder_config.video_stream_factory =
      new rtc::RefCountedObject<VideoStreamFactory>(1, kLowFramerate);
  source.IncomingCapturedFrame(CreateFrame(1, kFrameWidth, kFrameHeight));
  vie_encoder_->ConfigureEncoder(std::move(video_encoder_config),
                                 kMaxPayloadLength, false);
  vie_encoder_->WaitUntilTaskQueueIsIdle();

  EXPECT_EQ(vie_encoder_->overuse_detector_proxy_->GetLastTargetFramerate(),
            kLowFramerate);

  // Trigger overuse, max framerate should be reduced.
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  stats.input_frame_rate = kLowFramerate;
  stats_proxy_->SetMockStats(stats);
  vie_encoder_->TriggerCpuOveruse();
  vie_encoder_->WaitUntilTaskQueueIsIdle();
  int adapted_framerate =
      vie_encoder_->overuse_detector_proxy_->GetLastTargetFramerate();
  EXPECT_LT(adapted_framerate, kLowFramerate);

  // Reconfigure the encoder with a new (higher max framerate), max fps should
  // still respect the adaptation.
  video_encoder_config.video_stream_factory =
      new rtc::RefCountedObject<VideoStreamFactory>(1, kHighFramerate);
  source.IncomingCapturedFrame(CreateFrame(1, kFrameWidth, kFrameHeight));
  vie_encoder_->ConfigureEncoder(std::move(video_encoder_config),
                                 kMaxPayloadLength, false);
  vie_encoder_->WaitUntilTaskQueueIsIdle();

  EXPECT_EQ(vie_encoder_->overuse_detector_proxy_->GetLastTargetFramerate(),
            adapted_framerate);

  // Trigger underuse, max framerate should go back to codec configured fps.
  stats = stats_proxy_->GetStats();
  stats.input_frame_rate = adapted_framerate;
  stats_proxy_->SetMockStats(stats);
  vie_encoder_->TriggerCpuNormalUsage();
  vie_encoder_->WaitUntilTaskQueueIsIdle();
  EXPECT_EQ(vie_encoder_->overuse_detector_proxy_->GetLastTargetFramerate(),
            kHighFramerate);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, OveruseDetectorUpdatedOnDegradationPreferenceChange) {
  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;
  const int kFramerate = 24;

  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);
  test::FrameForwarder source;
  vie_encoder_->SetSource(
      &source, VideoSendStream::DegradationPreference::kMaintainResolution);

  // Trigger initial configuration.
  VideoEncoderConfig video_encoder_config;
  video_encoder_config.max_bitrate_bps = kTargetBitrateBps;
  video_encoder_config.number_of_streams = 1;
  video_encoder_config.video_stream_factory =
      new rtc::RefCountedObject<VideoStreamFactory>(1, kFramerate);
  source.IncomingCapturedFrame(CreateFrame(1, kFrameWidth, kFrameHeight));
  vie_encoder_->ConfigureEncoder(std::move(video_encoder_config),
                                 kMaxPayloadLength, false);
  vie_encoder_->WaitUntilTaskQueueIsIdle();

  EXPECT_EQ(vie_encoder_->overuse_detector_proxy_->GetLastTargetFramerate(),
            kFramerate);

  // Trigger overuse, max framerate should be reduced.
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  stats.input_frame_rate = kFramerate;
  stats_proxy_->SetMockStats(stats);
  vie_encoder_->TriggerCpuOveruse();
  vie_encoder_->WaitUntilTaskQueueIsIdle();
  int adapted_framerate =
      vie_encoder_->overuse_detector_proxy_->GetLastTargetFramerate();
  EXPECT_LT(adapted_framerate, kFramerate);

  // Change degradation preference to not enable framerate scaling. Target
  // framerate should be changed to codec defined limit.
  vie_encoder_->SetSource(
      &source, VideoSendStream::DegradationPreference::kMaintainFramerate);
  vie_encoder_->WaitUntilTaskQueueIsIdle();
  EXPECT_EQ(vie_encoder_->overuse_detector_proxy_->GetLastTargetFramerate(),
            kFramerate);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, DropsFramesAndScalesWhenBitrateIsTooLow) {
  const int kTooLowBitrateForFrameSizeBps = 10000;
  vie_encoder_->OnBitrateUpdated(kTooLowBitrateForFrameSizeBps, 0, 0);
  const int kWidth = 640;
  const int kHeight = 360;

  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));

  // Expect to drop this frame, the wait should time out.
  sink_.ExpectDroppedFrame();

  // Expect the sink_wants to specify a scaled frame.
  EXPECT_LT(video_source_.sink_wants().max_pixel_count, kWidth * kHeight);

  int last_pixel_count = video_source_.sink_wants().max_pixel_count;

  // Next frame is scaled.
  video_source_.IncomingCapturedFrame(
      CreateFrame(2, kWidth * 3 / 4, kHeight * 3 / 4));

  // Expect to drop this frame, the wait should time out.
  sink_.ExpectDroppedFrame();

  EXPECT_LT(video_source_.sink_wants().max_pixel_count, last_pixel_count);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, NumberOfDroppedFramesLimitedWhenBitrateIsTooLow) {
  const int kTooLowBitrateForFrameSizeBps = 10000;
  vie_encoder_->OnBitrateUpdated(kTooLowBitrateForFrameSizeBps, 0, 0);
  const int kWidth = 640;
  const int kHeight = 360;

  // We expect the n initial frames to get dropped.
  int i;
  for (i = 1; i <= kMaxInitialFramedrop; ++i) {
    video_source_.IncomingCapturedFrame(CreateFrame(i, kWidth, kHeight));
    sink_.ExpectDroppedFrame();
  }
  // The n+1th frame should not be dropped, even though it's size is too large.
  video_source_.IncomingCapturedFrame(CreateFrame(i, kWidth, kHeight));
  sink_.WaitForEncodedFrame(i);

  // Expect the sink_wants to specify a scaled frame.
  EXPECT_LT(video_source_.sink_wants().max_pixel_count, kWidth * kHeight);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, InitialFrameDropOffWithMaintainResolutionPreference) {
  const int kWidth = 640;
  const int kHeight = 360;
  vie_encoder_->OnBitrateUpdated(kLowTargetBitrateBps, 0, 0);

  // Set degradation preference.
  vie_encoder_->SetSource(
      &video_source_,
      VideoSendStream::DegradationPreference::kMaintainResolution);

  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  // Frame should not be dropped, even if it's too large.
  sink_.WaitForEncodedFrame(1);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, InitialFrameDropOffWhenEncoderDisabledScaling) {
  const int kWidth = 640;
  const int kHeight = 360;
  fake_encoder_.SetQualityScaling(false);
  vie_encoder_->OnBitrateUpdated(kLowTargetBitrateBps, 0, 0);

  // Force quality scaler reconfiguration by resetting the source.
  vie_encoder_->SetSource(&video_source_,
                          VideoSendStream::DegradationPreference::kBalanced);

  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  // Frame should not be dropped, even if it's too large.
  sink_.WaitForEncodedFrame(1);

  vie_encoder_->Stop();
  fake_encoder_.SetQualityScaling(true);
}

TEST_F(ViEEncoderTest,
       ResolutionNotAdaptedForTooSmallFrame_MaintainFramerateMode) {
  const int kTooSmallWidth = 10;
  const int kTooSmallHeight = 10;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Enable kMaintainFramerate preference, no initial limitation.
  test::FrameForwarder source;
  vie_encoder_->SetSource(
      &source, VideoSendStream::DegradationPreference::kMaintainFramerate);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);

  // Trigger adapt down, too small frame, expect no change.
  source.IncomingCapturedFrame(CreateFrame(1, kTooSmallWidth, kTooSmallHeight));
  sink_.WaitForEncodedFrame(1);
  vie_encoder_->TriggerCpuOveruse();
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, ResolutionNotAdaptedForTooSmallFrame_BalancedMode) {
  const int kTooSmallWidth = 10;
  const int kTooSmallHeight = 10;
  const int kFpsLimit = 7;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Enable kBalanced preference, no initial limitation.
  test::FrameForwarder source;
  vie_encoder_->SetSource(&source,
                          VideoSendStream::DegradationPreference::kBalanced);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);

  // Trigger adapt down, expect limited framerate.
  source.IncomingCapturedFrame(CreateFrame(1, kTooSmallWidth, kTooSmallHeight));
  sink_.WaitForEncodedFrame(1);
  vie_encoder_->TriggerQualityLow();
  VerifyFpsEqResolutionMax(source.sink_wants(), kFpsLimit);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, too small frame, expect no change.
  source.IncomingCapturedFrame(CreateFrame(2, kTooSmallWidth, kTooSmallHeight));
  sink_.WaitForEncodedFrame(2);
  vie_encoder_->TriggerQualityLow();
  VerifyFpsEqResolutionMax(source.sink_wants(), kFpsLimit);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, FailingInitEncodeDoesntCauseCrash) {
  fake_encoder_.ForceInitEncodeFailure(true);
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);
  ResetEncoder("VP8", 2, 1, true);
  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;
  video_source_.IncomingCapturedFrame(
      CreateFrame(1, kFrameWidth, kFrameHeight));
  sink_.ExpectDroppedFrame();
  vie_encoder_->Stop();
}

// TODO(sprang): Extend this with fps throttling and any "balanced" extensions.
TEST_F(ViEEncoderTest, AdaptsResolutionOnOveruse_MaintainFramerateMode) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;
  // Enabled default VideoAdapter downscaling. First step is 3/4, not 3/5 as
  // requested by ViEEncoder::VideoSourceProxy::RequestResolutionLowerThan().
  video_source_.set_adaptation_enabled(true);

  video_source_.IncomingCapturedFrame(
      CreateFrame(1, kFrameWidth, kFrameHeight));
  sink_.WaitForEncodedFrame(kFrameWidth, kFrameHeight);

  // Trigger CPU overuse, downscale by 3/4.
  vie_encoder_->TriggerCpuOveruse();
  video_source_.IncomingCapturedFrame(
      CreateFrame(2, kFrameWidth, kFrameHeight));
  sink_.WaitForEncodedFrame((kFrameWidth * 3) / 4, (kFrameHeight * 3) / 4);

  // Trigger CPU normal use, return to original resolution.
  vie_encoder_->TriggerCpuNormalUsage();
  video_source_.IncomingCapturedFrame(
      CreateFrame(3, kFrameWidth, kFrameHeight));
  sink_.WaitForEncodedFrame(kFrameWidth, kFrameHeight);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, AdaptsFramerateOnOveruse_MaintainResolutionMode) {
  const int kDefaultFramerateFps = 30;
  const int kFrameIntervalMs = rtc::kNumMillisecsPerSec / kDefaultFramerateFps;
  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;
  rtc::ScopedFakeClock fake_clock;

  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);
  vie_encoder_->SetSource(
      &video_source_,
      VideoSendStream::DegradationPreference::kMaintainResolution);
  video_source_.set_adaptation_enabled(true);

  fake_clock.SetTimeMicros(kFrameIntervalMs * 1000);
  int64_t timestamp_ms = kFrameIntervalMs;

  video_source_.IncomingCapturedFrame(
      CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);

  // Try to trigger overuse. No fps estimate available => no effect.
  vie_encoder_->TriggerCpuOveruse();

  // Insert frames for one second to get a stable estimate.
  for (int i = 0; i < kDefaultFramerateFps; ++i) {
    timestamp_ms += kFrameIntervalMs;
    fake_clock.AdvanceTimeMicros(kFrameIntervalMs * 1000);
    video_source_.IncomingCapturedFrame(
        CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
    sink_.WaitForEncodedFrame(timestamp_ms);
  }

  // Trigger CPU overuse, reduce framerate by 2/3.
  vie_encoder_->TriggerCpuOveruse();
  int num_frames_dropped = 0;
  for (int i = 0; i < kDefaultFramerateFps; ++i) {
    timestamp_ms += kFrameIntervalMs;
    fake_clock.AdvanceTimeMicros(kFrameIntervalMs * 1000);
    video_source_.IncomingCapturedFrame(
        CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
    if (!sink_.WaitForFrame(kFrameTimeoutMs)) {
      ++num_frames_dropped;
    } else {
      sink_.CheckLastFrameSizeMathces(kFrameWidth, kFrameHeight);
    }
  }

  // TODO(sprang): Find where there's rounding errors or stuff causing the
  // margin here to be a little larger than we'd like (input fps estimate is
  // off) and the frame dropping is a little too aggressive.
  const int kErrorMargin = 5;
  EXPECT_NEAR(num_frames_dropped,
              kDefaultFramerateFps - (kDefaultFramerateFps * 2 / 3),
              kErrorMargin);

  // Trigger CPU overuse, reduce framerate by 2/3 again.
  vie_encoder_->TriggerCpuOveruse();
  num_frames_dropped = 0;
  for (int i = 0; i < kDefaultFramerateFps; ++i) {
    timestamp_ms += kFrameIntervalMs;
    fake_clock.AdvanceTimeMicros(kFrameIntervalMs * 1000);
    video_source_.IncomingCapturedFrame(
        CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
    if (!sink_.WaitForFrame(kFrameTimeoutMs)) {
      ++num_frames_dropped;
    } else {
      sink_.CheckLastFrameSizeMathces(kFrameWidth, kFrameHeight);
    }
  }
  EXPECT_NEAR(num_frames_dropped,
              kDefaultFramerateFps - (kDefaultFramerateFps * 4 / 9),
              kErrorMargin);

  // Go back up one step.
  vie_encoder_->TriggerCpuNormalUsage();
  num_frames_dropped = 0;
  for (int i = 0; i < kDefaultFramerateFps; ++i) {
    timestamp_ms += kFrameIntervalMs;
    fake_clock.AdvanceTimeMicros(kFrameIntervalMs * 1000);
    video_source_.IncomingCapturedFrame(
        CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
    if (!sink_.WaitForFrame(kFrameTimeoutMs)) {
      ++num_frames_dropped;
    } else {
      sink_.CheckLastFrameSizeMathces(kFrameWidth, kFrameHeight);
    }
  }
  EXPECT_NEAR(num_frames_dropped,
              kDefaultFramerateFps - (kDefaultFramerateFps * 2 / 3),
              kErrorMargin);

  // Go back up to original mode.
  vie_encoder_->TriggerCpuNormalUsage();
  num_frames_dropped = 0;
  for (int i = 0; i < kDefaultFramerateFps; ++i) {
    timestamp_ms += kFrameIntervalMs;
    fake_clock.AdvanceTimeMicros(kFrameIntervalMs * 1000);
    video_source_.IncomingCapturedFrame(
        CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
    if (!sink_.WaitForFrame(kFrameTimeoutMs)) {
      ++num_frames_dropped;
    } else {
      sink_.CheckLastFrameSizeMathces(kFrameWidth, kFrameHeight);
    }
  }
  EXPECT_NEAR(num_frames_dropped, 0, kErrorMargin);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, DoesntAdaptDownPastMinFramerate) {
  const int kFramerateFps = 5;
  const int kFrameIntervalMs = rtc::kNumMillisecsPerSec / kFramerateFps;
  const int kMinFpsFrameInterval = rtc::kNumMillisecsPerSec / kMinFramerateFps;
  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;

  rtc::ScopedFakeClock fake_clock;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);
  vie_encoder_->SetSource(
      &video_source_,
      VideoSendStream::DegradationPreference::kMaintainResolution);
  video_source_.set_adaptation_enabled(true);

  fake_clock.SetTimeMicros(kFrameIntervalMs * 1000);
  int64_t timestamp_ms = kFrameIntervalMs;

  // Trigger overuse as much as we can.
  for (int i = 0; i < ViEEncoder::kMaxCpuResolutionDowngrades; ++i) {
    // Insert frames to get a new fps estimate...
    for (int j = 0; j < kFramerateFps; ++j) {
      video_source_.IncomingCapturedFrame(
          CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
      timestamp_ms += kFrameIntervalMs;
      fake_clock.AdvanceTimeMicros(kFrameIntervalMs * 1000);
    }
    // ...and then try to adapt again.
    vie_encoder_->TriggerCpuOveruse();
  }

  // Drain any frame in the pipeline.
  sink_.WaitForFrame(kDefaultTimeoutMs);

  // Insert frames at min fps, all should go through.
  for (int i = 0; i < 10; ++i) {
    timestamp_ms += kMinFpsFrameInterval;
    fake_clock.AdvanceTimeMicros(kMinFpsFrameInterval * 1000);
    video_source_.IncomingCapturedFrame(
        CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
    sink_.WaitForEncodedFrame(timestamp_ms);
  }

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, AdaptsResolutionAndFramerateForLowQuality_BalancedMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  const int64_t kFrameIntervalMs = 150;
  int64_t timestamp_ms = kFrameIntervalMs;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Enable kBalanced preference, no initial limitation.
  AdaptingFrameForwarder source;
  source.set_adaptation_enabled(true);
  vie_encoder_->SetSource(&source,
                          VideoSendStream::DegradationPreference::kBalanced);
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution (960x540@30fps).
  vie_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionLt(source.sink_wants(), kWidth * kHeight);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution (640x360@30fps).
  vie_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionLt(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect reduced fps (640x360@15fps).
  vie_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsLtResolutionEq(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution (480x270@15fps).
  vie_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionLt(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Restrict bitrate, trigger adapt down, expect reduced fps (480x270@10fps).
  vie_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsLtResolutionEq(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(5, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution (320x180@10fps).
  vie_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionLt(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(6, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect reduced fps (320x180@7fps).
  vie_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsLtResolutionEq(source.sink_wants(), source.last_wants());
  rtc::VideoSinkWants last_wants = source.sink_wants();
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(7, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, min resolution reached, expect no change.
  vie_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionEq(source.sink_wants(), last_wants);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(7, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect expect increased fps (320x180@10fps).
  vie_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsGtResolutionEq(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(8, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect upscaled resolution (480x270@10fps).
  vie_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionGt(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(9, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Increase bitrate, trigger adapt up, expect increased fps (480x270@15fps).
  vie_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsGtResolutionEq(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(10, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect upscaled resolution (640x360@15fps).
  vie_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionGt(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(11, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect increased fps (640x360@30fps).
  vie_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionEq(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(12, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect upscaled resolution (960x540@30fps).
  vie_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionGt(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(13, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up,  expect no restriction (1280x720fps@30fps).
  vie_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionGt(source.sink_wants(), source.last_wants());
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(14, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no change.
  vie_encoder_->TriggerQualityHigh();
  VerifyNoLimitation(source.sink_wants());
  EXPECT_EQ(14, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, AdaptWithTwoReasonsAndDifferentOrder_Framerate) {
  const int kWidth = 1280;
  const int kHeight = 720;
  const int64_t kFrameIntervalMs = 150;
  int64_t timestamp_ms = kFrameIntervalMs;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Enable kBalanced preference, no initial limitation.
  AdaptingFrameForwarder source;
  source.set_adaptation_enabled(true);
  vie_encoder_->SetSource(&source,
                          VideoSendStream::DegradationPreference::kBalanced);
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt down, expect scaled down resolution (960x540@30fps).
  vie_encoder_->TriggerCpuOveruse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionLt(source.sink_wants(), kWidth * kHeight);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt down, expect scaled down resolution (640x360@30fps).
  vie_encoder_->TriggerCpuOveruse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionLt(source.sink_wants(), source.last_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality adapt down, expect reduced fps (640x360@15fps).
  vie_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsLtResolutionEq(source.sink_wants(), source.last_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt up, expect increased fps (640x360@30fps).
  vie_encoder_->TriggerCpuNormalUsage();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionEq(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality adapt up, expect upscaled resolution (960x540@30fps).
  vie_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionGt(source.sink_wants(), source.last_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt up,  expect no restriction (1280x720fps@30fps).
  vie_encoder_->TriggerCpuNormalUsage();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionGt(source.sink_wants(), source.last_wants());
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no change.
  vie_encoder_->TriggerQualityHigh();
  VerifyNoLimitation(source.sink_wants());
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, AdaptWithTwoReasonsAndDifferentOrder_Resolution) {
  const int kWidth = 640;
  const int kHeight = 360;
  const int kFpsLimit = 15;
  const int64_t kFrameIntervalMs = 150;
  int64_t timestamp_ms = kFrameIntervalMs;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Enable kBalanced preference, no initial limitation.
  AdaptingFrameForwarder source;
  source.set_adaptation_enabled(true);
  vie_encoder_->SetSource(&source,
                          VideoSendStream::DegradationPreference::kBalanced);
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt down, expect scaled down framerate (640x360@15fps).
  vie_encoder_->TriggerCpuOveruse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionMax(source.sink_wants(), kFpsLimit);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality adapt down, expect scaled down resolution (480x270@15fps).
  vie_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionLt(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt up, expect upscaled resolution (640x360@15fps).
  vie_encoder_->TriggerCpuNormalUsage();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionGt(source.sink_wants(), source.last_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality adapt up, expect increased fps (640x360@30fps).
  vie_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no change.
  vie_encoder_->TriggerQualityHigh();
  VerifyNoLimitation(source.sink_wants());
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, AcceptsFullHdAdaptedDownSimulcastFrames) {
  // Simulates simulcast behavior and makes highest stream resolutions divisible
  // by 4.
  class CroppingVideoStreamFactory
      : public VideoEncoderConfig::VideoStreamFactoryInterface {
   public:
    explicit CroppingVideoStreamFactory(size_t num_temporal_layers,
                                        int framerate)
        : num_temporal_layers_(num_temporal_layers), framerate_(framerate) {
      EXPECT_GT(num_temporal_layers, 0u);
      EXPECT_GT(framerate, 0);
    }

   private:
    std::vector<VideoStream> CreateEncoderStreams(
        int width,
        int height,
        const VideoEncoderConfig& encoder_config) override {
      std::vector<VideoStream> streams =
          test::CreateVideoStreams(width - width % 4, height - height % 4,
                                   encoder_config);
      for (VideoStream& stream : streams) {
        stream.temporal_layer_thresholds_bps.resize(num_temporal_layers_ - 1);
        stream.max_framerate = framerate_;
      }
      return streams;
    }

    const size_t num_temporal_layers_;
    const int framerate_;
  };

  const int kFrameWidth = 1920;
  const int kFrameHeight = 1080;
  // 3/4 of 1920.
  const int kAdaptedFrameWidth = 1440;
  // 3/4 of 1080 rounded down to multiple of 4.
  const int kAdaptedFrameHeight = 808;
  const int kFramerate = 24;

  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);
  // Trigger reconfigure encoder (without resetting the entire instance).
  VideoEncoderConfig video_encoder_config;
  video_encoder_config.max_bitrate_bps = kTargetBitrateBps;
  video_encoder_config.number_of_streams = 1;
  video_encoder_config.video_stream_factory =
      new rtc::RefCountedObject<CroppingVideoStreamFactory>(1, kFramerate);
  vie_encoder_->ConfigureEncoder(std::move(video_encoder_config),
                                   kMaxPayloadLength, false);
  vie_encoder_->WaitUntilTaskQueueIsIdle();

  video_source_.set_adaptation_enabled(true);

  video_source_.IncomingCapturedFrame(
      CreateFrame(1, kFrameWidth, kFrameHeight));
  sink_.WaitForEncodedFrame(kFrameWidth, kFrameHeight);

  // Trigger CPU overuse, downscale by 3/4.
  vie_encoder_->TriggerCpuOveruse();
  video_source_.IncomingCapturedFrame(
      CreateFrame(2, kFrameWidth, kFrameHeight));
  sink_.WaitForEncodedFrame(kAdaptedFrameWidth, kAdaptedFrameHeight);

  vie_encoder_->Stop();
}


}  // namespace webrtc
