/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits>
#include <utility>

#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/base/logging.h"
#include "webrtc/media/base/videoadapter.h"
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
#if defined(WEBRTC_ANDROID)
// TODO(kthelgason): Lower this limit when better testing
// on MediaCodec and fallback implementations are in place.
const int kMinPixelsPerFrame = 320 * 180;
#else
const int kMinPixelsPerFrame = 120 * 90;
#endif
}

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

class ViEEncoderUnderTest : public ViEEncoder {
 public:
  ViEEncoderUnderTest(SendStatisticsProxy* stats_proxy,
                      const VideoSendStream::Config::EncoderSettings& settings)
      : ViEEncoder(1 /* number_of_cores */,
                   stats_proxy,
                   settings,
                   nullptr /* pre_encode_callback */,
                   nullptr /* encoder_timing */) {}

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
};

class VideoStreamFactory
    : public VideoEncoderConfig::VideoStreamFactoryInterface {
 public:
  explicit VideoStreamFactory(size_t num_temporal_layers)
      : num_temporal_layers_(num_temporal_layers) {
    EXPECT_GT(num_temporal_layers, 0u);
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
    }
    return streams;
  }
  const size_t num_temporal_layers_;
};

class AdaptingFrameForwarder : public test::FrameForwarder {
 public:
  AdaptingFrameForwarder() : adaptation_enabled_(false) {}
  virtual ~AdaptingFrameForwarder() {}

  void set_adaptation_enabled(bool enabled) {
    rtc::CritScope cs(&crit_);
    adaptation_enabled_ = enabled;
  }

  bool adaption_enabled() {
    rtc::CritScope cs(&crit_);
    return adaptation_enabled_;
  }

  void IncomingCapturedFrame(const VideoFrame& video_frame) override {
    int cropped_width = 0;
    int cropped_height = 0;
    int out_width = 0;
    int out_height = 0;
    if (adaption_enabled() &&
        adapter_.AdaptFrameResolution(video_frame.width(), video_frame.height(),
                                      video_frame.timestamp_us() * 1000,
                                      &cropped_width, &cropped_height,
                                      &out_width, &out_height)) {
      VideoFrame adapted_frame(
          new rtc::RefCountedObject<TestBuffer>(nullptr, out_width, out_height),
          99, 99, kVideoRotation_0);
      adapted_frame.set_ntp_time_ms(video_frame.ntp_time_ms());
      test::FrameForwarder::IncomingCapturedFrame(adapted_frame);
    } else {
      test::FrameForwarder::IncomingCapturedFrame(video_frame);
    }
  }

  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override {
    rtc::CritScope cs(&crit_);
    adapter_.OnResolutionRequest(wants.target_pixel_count,
                                 wants.max_pixel_count);
    test::FrameForwarder::AddOrUpdateSink(sink, wants);
  }

  cricket::VideoAdapter adapter_;
  bool adaptation_enabled_ GUARDED_BY(crit_);
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
        stats_proxy_(new SendStatisticsProxy(
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
    vie_encoder_->SetSource(&video_source_,
                            VideoSendStream::DegradationPreference::kBalanced);
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
        new rtc::RefCountedObject<VideoStreamFactory>(num_temporal_layers);
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
    return frame;
  }

  class TestEncoder : public test::FakeEncoder {
   public:
    TestEncoder()
        : FakeEncoder(Clock::GetRealTimeClock()),
          continue_encode_event_(false, false) {}

    VideoCodec codec_config() {
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

    rtc::CriticalSection local_crit_sect_;
    bool block_next_encode_ = false;
    rtc::Event continue_encode_event_;
    uint32_t timestamp_ = 0;
    int64_t ntp_time_ms_ = 0;
    int last_input_width_ = 0;
    int last_input_height_ = 0;
    bool quality_scaling_ = true;
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
      uint32_t width = 0;
      uint32_t height = 0;
      EXPECT_TRUE(encoded_frame_event_.Wait(kDefaultTimeoutMs));
      {
        rtc::CritScope lock(&crit_);
        width = last_width_;
        height = last_height_;
      }
      EXPECT_EQ(expected_height, height);
      EXPECT_EQ(expected_width, width);
    }

    void ExpectDroppedFrame() { EXPECT_FALSE(encoded_frame_event_.Wait(100)); }

    void SetExpectNoFrames() {
      rtc::CritScope lock(&crit_);
      expect_frames_ = false;
    }

    int number_of_reconfigurations() {
      rtc::CritScope lock(&crit_);
      return number_of_reconfigurations_;
    }

    int last_min_transmit_bitrate() {
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
  std::unique_ptr<SendStatisticsProxy> stats_proxy_;
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
  vie_encoder_->SetSource(&new_video_source,
                          VideoSendStream::DegradationPreference::kBalanced);
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
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  EXPECT_FALSE(video_source_.sink_wants().target_pixel_count);
  EXPECT_FALSE(video_source_.sink_wants().max_pixel_count);

  int frame_width = 1280;
  int frame_height = 720;

  // Trigger CPU overuse kMaxCpuDowngrades times. Every time, ViEEncoder should
  // request lower resolution.
  for (int i = 1; i <= ViEEncoder::kMaxCpuDowngrades; ++i) {
    video_source_.IncomingCapturedFrame(
        CreateFrame(i, frame_width, frame_height));
    sink_.WaitForEncodedFrame(i);

    vie_encoder_->TriggerCpuOveruse();

    EXPECT_FALSE(video_source_.sink_wants().target_pixel_count);
    EXPECT_LT(video_source_.sink_wants().max_pixel_count.value_or(
                  std::numeric_limits<int>::max()),
              frame_width * frame_height);

    frame_width /= 2;
    frame_height /= 2;
  }

  // Trigger CPU overuse one more time. This should not trigger a request for
  // lower resolution.
  rtc::VideoSinkWants current_wants = video_source_.sink_wants();
  video_source_.IncomingCapturedFrame(CreateFrame(
      ViEEncoder::kMaxCpuDowngrades + 1, frame_width, frame_height));
  sink_.WaitForEncodedFrame(ViEEncoder::kMaxCpuDowngrades + 1);
  vie_encoder_->TriggerCpuOveruse();
  EXPECT_EQ(video_source_.sink_wants().target_pixel_count,
            current_wants.target_pixel_count);
  EXPECT_EQ(video_source_.sink_wants().max_pixel_count,
            current_wants.max_pixel_count);

  // Trigger CPU normal use.
  vie_encoder_->TriggerCpuNormalUsage();
  EXPECT_EQ(frame_width * frame_height * 5 / 3,
            video_source_.sink_wants().target_pixel_count.value_or(0));
  EXPECT_EQ(frame_width * frame_height * 4,
            video_source_.sink_wants().max_pixel_count.value_or(0));

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest,
       ResolutionSinkWantsResetOnSetSourceWithDisabledResolutionScaling) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  EXPECT_FALSE(video_source_.sink_wants().target_pixel_count);
  EXPECT_FALSE(video_source_.sink_wants().max_pixel_count);

  int frame_width = 1280;
  int frame_height = 720;

  video_source_.IncomingCapturedFrame(
      CreateFrame(1, frame_width, frame_height));
  sink_.WaitForEncodedFrame(1);
  // Trigger CPU overuse.
  vie_encoder_->TriggerCpuOveruse();

  video_source_.IncomingCapturedFrame(
      CreateFrame(2, frame_width, frame_height));
  sink_.WaitForEncodedFrame(2);
  EXPECT_FALSE(video_source_.sink_wants().target_pixel_count);
  EXPECT_LT(video_source_.sink_wants().max_pixel_count.value_or(
                std::numeric_limits<int>::max()),
            frame_width * frame_height);

  // Set new source.
  test::FrameForwarder new_video_source;
  vie_encoder_->SetSource(
      &new_video_source,
      VideoSendStream::DegradationPreference::kMaintainResolution);

  EXPECT_FALSE(new_video_source.sink_wants().target_pixel_count);
  EXPECT_FALSE(new_video_source.sink_wants().max_pixel_count);

  new_video_source.IncomingCapturedFrame(
      CreateFrame(3, frame_width, frame_height));
  sink_.WaitForEncodedFrame(3);
  EXPECT_FALSE(new_video_source.sink_wants().target_pixel_count);
  EXPECT_FALSE(new_video_source.sink_wants().max_pixel_count);

  // Calling SetSource with resolution scaling enabled apply the old SinkWants.
  vie_encoder_->SetSource(&new_video_source,
                          VideoSendStream::DegradationPreference::kBalanced);
  EXPECT_LT(new_video_source.sink_wants().max_pixel_count.value_or(
                std::numeric_limits<int>::max()),
            frame_width * frame_height);
  EXPECT_FALSE(new_video_source.sink_wants().target_pixel_count);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, StatsTracksAdaptationStats) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  int frame_width = 1280;
  int frame_height = 720;

  video_source_.IncomingCapturedFrame(
      CreateFrame(1, frame_width, frame_height));
  sink_.WaitForEncodedFrame(1);
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_EQ(0, stats.number_of_cpu_adapt_changes);

  // Trigger CPU overuse.
  vie_encoder_->TriggerCpuOveruse();
  video_source_.IncomingCapturedFrame(
      CreateFrame(2, frame_width, frame_height));
  sink_.WaitForEncodedFrame(2);

  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Trigger CPU normal use.
  vie_encoder_->TriggerCpuNormalUsage();
  video_source_.IncomingCapturedFrame(
      CreateFrame(3, frame_width, frame_height));
  sink_.WaitForEncodedFrame(3);

  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_EQ(2, stats.number_of_cpu_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, SwitchingSourceKeepsCpuAdaptation) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  int frame_width = 1280;
  int frame_height = 720;
  video_source_.IncomingCapturedFrame(
      CreateFrame(1, frame_width, frame_height));
  sink_.WaitForEncodedFrame(1);

  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_EQ(0, stats.number_of_cpu_adapt_changes);

  vie_encoder_->TriggerCpuOveruse();

  video_source_.IncomingCapturedFrame(
      CreateFrame(2, frame_width, frame_height));
  sink_.WaitForEncodedFrame(2);
  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Set new source with adaptation still enabled.
  test::FrameForwarder new_video_source;
  vie_encoder_->SetSource(&new_video_source,
                          VideoSendStream::DegradationPreference::kBalanced);

  new_video_source.IncomingCapturedFrame(
      CreateFrame(3, frame_width, frame_height));
  sink_.WaitForEncodedFrame(3);
  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Set adaptation disabled.
  vie_encoder_->SetSource(
      &new_video_source,
      VideoSendStream::DegradationPreference::kMaintainResolution);

  new_video_source.IncomingCapturedFrame(
      CreateFrame(4, frame_width, frame_height));
  sink_.WaitForEncodedFrame(4);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Set adaptation back to enabled.
  vie_encoder_->SetSource(&new_video_source,
                          VideoSendStream::DegradationPreference::kBalanced);

  new_video_source.IncomingCapturedFrame(
      CreateFrame(5, frame_width, frame_height));
  sink_.WaitForEncodedFrame(5);
  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  vie_encoder_->TriggerCpuNormalUsage();

  new_video_source.IncomingCapturedFrame(
      CreateFrame(6, frame_width, frame_height));
  sink_.WaitForEncodedFrame(6);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_EQ(2, stats.number_of_cpu_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, SwitchingSourceKeepsQualityAdaptation) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  int frame_width = 1280;
  int frame_height = 720;
  video_source_.IncomingCapturedFrame(
      CreateFrame(1, frame_width, frame_height));
  sink_.WaitForEncodedFrame(1);

  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_EQ(0, stats.number_of_cpu_adapt_changes);

  // Set new source with adaptation still enabled.
  test::FrameForwarder new_video_source;
  vie_encoder_->SetSource(&new_video_source,
                          VideoSendStream::DegradationPreference::kBalanced);

  new_video_source.IncomingCapturedFrame(
      CreateFrame(2, frame_width, frame_height));
  sink_.WaitForEncodedFrame(2);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_EQ(0, stats.number_of_cpu_adapt_changes);

  vie_encoder_->TriggerQualityLow();

  new_video_source.IncomingCapturedFrame(
      CreateFrame(3, frame_width, frame_height));
  sink_.WaitForEncodedFrame(3);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_TRUE(stats.bw_limited_resolution);

  vie_encoder_->SetSource(&new_video_source,
                          VideoSendStream::DegradationPreference::kBalanced);

  new_video_source.IncomingCapturedFrame(
      CreateFrame(4, frame_width, frame_height));
  sink_.WaitForEncodedFrame(4);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_TRUE(stats.bw_limited_resolution);

  // Set adaptation disabled.
  vie_encoder_->SetSource(
      &new_video_source,
      VideoSendStream::DegradationPreference::kMaintainResolution);

  new_video_source.IncomingCapturedFrame(
      CreateFrame(5, frame_width, frame_height));
  sink_.WaitForEncodedFrame(5);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.bw_limited_resolution);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, StatsTracksAdaptationStatsWhenSwitchingSource) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  int frame_width = 1280;
  int frame_height = 720;
  int sequence = 1;

  video_source_.IncomingCapturedFrame(
      CreateFrame(sequence, frame_width, frame_height));
  sink_.WaitForEncodedFrame(sequence++);

  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_EQ(0, stats.number_of_cpu_adapt_changes);

  // Trigger CPU overuse again, should now adapt down.
  vie_encoder_->TriggerCpuOveruse();
  video_source_.IncomingCapturedFrame(
      CreateFrame(sequence, frame_width, frame_height));
  sink_.WaitForEncodedFrame(sequence++);

  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Set new source with adaptation still enabled.
  test::FrameForwarder new_video_source;
  vie_encoder_->SetSource(&new_video_source,
                          VideoSendStream::DegradationPreference::kBalanced);

  new_video_source.IncomingCapturedFrame(
      CreateFrame(sequence, frame_width, frame_height));
  sink_.WaitForEncodedFrame(sequence++);
  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Set adaptation disabled.
  vie_encoder_->SetSource(
      &new_video_source,
      VideoSendStream::DegradationPreference::kMaintainResolution);
  new_video_source.IncomingCapturedFrame(
      CreateFrame(sequence, frame_width, frame_height));
  sink_.WaitForEncodedFrame(sequence++);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Switch back the source with adaptation enabled.
  vie_encoder_->SetSource(&video_source_,
                          VideoSendStream::DegradationPreference::kBalanced);
  video_source_.IncomingCapturedFrame(
      CreateFrame(sequence, frame_width, frame_height));
  sink_.WaitForEncodedFrame(sequence++);
  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Trigger CPU normal usage.
  vie_encoder_->TriggerCpuNormalUsage();
  video_source_.IncomingCapturedFrame(
      CreateFrame(sequence, frame_width, frame_height));
  sink_.WaitForEncodedFrame(sequence++);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_EQ(2, stats.number_of_cpu_adapt_changes);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, StatsTracksPreferredBitrate) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  video_source_.IncomingCapturedFrame(CreateFrame(1, 1280, 720));
  sink_.WaitForEncodedFrame(1);

  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  EXPECT_EQ(video_encoder_config_.max_bitrate_bps,
            stats.preferred_media_bitrate_bps);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, ScalingUpAndDownDoesNothingWithMaintainResolution) {
  int frame_width = 1280;
  int frame_height = 720;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  // Expect no scaling to begin with
  EXPECT_FALSE(video_source_.sink_wants().target_pixel_count);
  EXPECT_FALSE(video_source_.sink_wants().max_pixel_count);

  video_source_.IncomingCapturedFrame(
      CreateFrame(1, frame_width, frame_height));
  sink_.WaitForEncodedFrame(1);

  // Trigger scale down
  vie_encoder_->TriggerQualityLow();

  video_source_.IncomingCapturedFrame(
      CreateFrame(2, frame_width, frame_height));
  sink_.WaitForEncodedFrame(2);

  // Expect a scale down.
  EXPECT_TRUE(video_source_.sink_wants().max_pixel_count);
  EXPECT_LT(*video_source_.sink_wants().max_pixel_count,
            frame_width * frame_height);

  // Set adaptation disabled.
  test::FrameForwarder new_video_source;
  vie_encoder_->SetSource(
      &new_video_source,
      VideoSendStream::DegradationPreference::kMaintainResolution);

  // Trigger scale down
  vie_encoder_->TriggerQualityLow();
  new_video_source.IncomingCapturedFrame(
      CreateFrame(3, frame_width, frame_height));
  sink_.WaitForEncodedFrame(3);

  // Expect no scaling
  EXPECT_FALSE(new_video_source.sink_wants().max_pixel_count);

  // Trigger scale up
  vie_encoder_->TriggerQualityHigh();
  new_video_source.IncomingCapturedFrame(
      CreateFrame(4, frame_width, frame_height));
  sink_.WaitForEncodedFrame(4);

  // Expect nothing to change, still no scaling
  EXPECT_FALSE(new_video_source.sink_wants().max_pixel_count);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, DoesNotScaleBelowSetLimit) {
  int frame_width = 1280;
  int frame_height = 720;
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  for (size_t i = 1; i <= 10; i++) {
    video_source_.IncomingCapturedFrame(
        CreateFrame(i, frame_width, frame_height));
    sink_.WaitForEncodedFrame(i);
    // Trigger scale down
    vie_encoder_->TriggerQualityLow();
    EXPECT_GE(*video_source_.sink_wants().max_pixel_count, kMinPixelsPerFrame);
  }

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, UMACpuLimitedResolutionInPercent) {
  vie_encoder_->OnBitrateUpdated(kTargetBitrateBps, 0, 0);

  int frame_width = 640;
  int frame_height = 360;

  for (int i = 1; i <= SendStatisticsProxy::kMinRequiredMetricsSamples; ++i) {
    video_source_.IncomingCapturedFrame(
        CreateFrame(i, frame_width, frame_height));
    sink_.WaitForEncodedFrame(i);
  }

  vie_encoder_->TriggerCpuOveruse();
  for (int i = 1; i <= SendStatisticsProxy::kMinRequiredMetricsSamples; ++i) {
    video_source_.IncomingCapturedFrame(
        CreateFrame(SendStatisticsProxy::kMinRequiredMetricsSamples + i,
                    frame_width, frame_height));
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

TEST_F(ViEEncoderTest, DropsFramesAndScalesWhenBitrateIsTooLow) {
  vie_encoder_->OnBitrateUpdated(kLowTargetBitrateBps, 0, 0);
  int frame_width = 640;
  int frame_height = 360;

  video_source_.IncomingCapturedFrame(
      CreateFrame(1, frame_width, frame_height));

  // Expect to drop this frame, the wait should time out.
  sink_.ExpectDroppedFrame();

  // Expect the sink_wants to specify a scaled frame.
  EXPECT_TRUE(video_source_.sink_wants().max_pixel_count);
  EXPECT_LT(*video_source_.sink_wants().max_pixel_count, 1000 * 1000);

  int last_pixel_count = *video_source_.sink_wants().max_pixel_count;

  // Next frame is scaled
  video_source_.IncomingCapturedFrame(
      CreateFrame(2, frame_width * 3 / 4, frame_height * 3 / 4));

  // Expect to drop this frame, the wait should time out.
  sink_.ExpectDroppedFrame();

  EXPECT_LT(*video_source_.sink_wants().max_pixel_count, last_pixel_count);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, NrOfDroppedFramesLimited) {
  // 1kbps. This can never be achieved.
  vie_encoder_->OnBitrateUpdated(1000, 0, 0);
  int frame_width = 640;
  int frame_height = 360;

  // We expect the n initial frames to get dropped.
  int i;
  for (i = 1; i <= kMaxInitialFramedrop; ++i) {
    video_source_.IncomingCapturedFrame(
        CreateFrame(i, frame_width, frame_height));
    sink_.ExpectDroppedFrame();
  }
  // The n+1th frame should not be dropped, even though it's size is too large.
  video_source_.IncomingCapturedFrame(
      CreateFrame(i, frame_width, frame_height));
  sink_.WaitForEncodedFrame(i);

  // Expect the sink_wants to specify a scaled frame.
  EXPECT_TRUE(video_source_.sink_wants().max_pixel_count);
  EXPECT_LT(*video_source_.sink_wants().max_pixel_count, 1000 * 1000);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, InitialFrameDropOffWithMaintainResolutionPreference) {
  int frame_width = 640;
  int frame_height = 360;
  vie_encoder_->OnBitrateUpdated(kLowTargetBitrateBps, 0, 0);

  // Set degradation preference.
  vie_encoder_->SetSource(
      &video_source_,
      VideoSendStream::DegradationPreference::kMaintainResolution);

  video_source_.IncomingCapturedFrame(
      CreateFrame(1, frame_width, frame_height));
  // Frame should not be dropped, even if it's too large.
  sink_.WaitForEncodedFrame(1);

  vie_encoder_->Stop();
}

TEST_F(ViEEncoderTest, InitialFrameDropOffWhenEncoderDisabledScaling) {
  int frame_width = 640;
  int frame_height = 360;
  fake_encoder_.SetQualityScaling(false);
  vie_encoder_->OnBitrateUpdated(kLowTargetBitrateBps, 0, 0);
  // Force quality scaler reconfiguration by resetting the source.
  vie_encoder_->SetSource(&video_source_,
                          VideoSendStream::DegradationPreference::kBalanced);

  video_source_.IncomingCapturedFrame(
      CreateFrame(1, frame_width, frame_height));
  // Frame should not be dropped, even if it's too large.
  sink_.WaitForEncodedFrame(1);

  vie_encoder_->Stop();
  fake_encoder_.SetQualityScaling(true);
}

// TODO(sprang): Extend this with fps throttling and any "balanced" extensions.
TEST_F(ViEEncoderTest, AdaptsResolutionOnOveruse) {
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

  // Trigger CPU normal use, return to original resoluton;
  vie_encoder_->TriggerCpuNormalUsage();
  video_source_.IncomingCapturedFrame(
      CreateFrame(3, kFrameWidth, kFrameHeight));
  sink_.WaitForEncodedFrame(kFrameWidth, kFrameHeight);

  vie_encoder_->Stop();
}
}  // namespace webrtc
