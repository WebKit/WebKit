/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_stream_encoder.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/test/mock_fec_controller_override.h"
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_bitrate_allocation.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/vp8_temporal_layers.h"
#include "api/video_codecs/vp8_temporal_layers_factory.h"
#include "common_video/h264/h264_common.h"
#include "common_video/include/video_frame_buffer.h"
#include "media/base/video_adapter.h"
#include "modules/video_coding/codecs/vp9/include/vp9_globals.h"
#include "modules/video_coding/utility/default_video_bitrate_allocator.h"
#include "modules/video_coding/utility/simulcast_rate_allocator.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"
#include "system_wrappers/include/sleep.h"
#include "test/encoder_settings.h"
#include "test/fake_encoder.h"
#include "test/field_trial.h"
#include "test/frame_generator.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/video_encoder_proxy_factory.h"
#include "video/send_statistics_proxy.h"

namespace webrtc {

using ScaleReason = AdaptationObserverInterface::AdaptReason;
using ::testing::_;

namespace {
const int kMinPixelsPerFrame = 320 * 180;
const int kMinFramerateFps = 2;
const int kMinBalancedFramerateFps = 7;
const int64_t kFrameTimeoutMs = 100;
const size_t kMaxPayloadLength = 1440;
const uint32_t kTargetBitrateBps = 1000000;
const uint32_t kStartBitrateBps = 600000;
const uint32_t kSimulcastTargetBitrateBps = 3150000;
const uint32_t kLowTargetBitrateBps = kTargetBitrateBps / 10;
const int kMaxInitialFramedrop = 4;
const int kDefaultFramerate = 30;
const int64_t kFrameIntervalMs = rtc::kNumMillisecsPerSec / kDefaultFramerate;

uint8_t optimal_sps[] = {0,    0,    0,    1,    H264::NaluType::kSps,
                         0x00, 0x00, 0x03, 0x03, 0xF4,
                         0x05, 0x03, 0xC7, 0xE0, 0x1B,
                         0x41, 0x10, 0x8D, 0x00};

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

// A fake native buffer that can't be converted to I420.
class FakeNativeBuffer : public webrtc::VideoFrameBuffer {
 public:
  FakeNativeBuffer(rtc::Event* event, int width, int height)
      : event_(event), width_(width), height_(height) {}
  webrtc::VideoFrameBuffer::Type type() const override { return Type::kNative; }
  int width() const override { return width_; }
  int height() const override { return height_; }
  rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override {
    return nullptr;
  }

 private:
  friend class rtc::RefCountedObject<FakeNativeBuffer>;
  ~FakeNativeBuffer() override {
    if (event_)
      event_->Set();
  }
  rtc::Event* const event_;
  const int width_;
  const int height_;
};

class CpuOveruseDetectorProxy : public OveruseFrameDetector {
 public:
  explicit CpuOveruseDetectorProxy(CpuOveruseMetricsObserver* metrics_observer)
      : OveruseFrameDetector(metrics_observer),
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

  CpuOveruseOptions GetOptions() { return options_; }

 private:
  rtc::CriticalSection lock_;
  int last_target_framerate_fps_ RTC_GUARDED_BY(lock_);
};

class VideoStreamEncoderUnderTest : public VideoStreamEncoder {
 public:
  VideoStreamEncoderUnderTest(SendStatisticsProxy* stats_proxy,
                              const VideoStreamEncoderSettings& settings,
                              TaskQueueFactory* task_queue_factory)
      : VideoStreamEncoder(Clock::GetRealTimeClock(),
                           1 /* number_of_cores */,
                           stats_proxy,
                           settings,
                           std::unique_ptr<OveruseFrameDetector>(
                               overuse_detector_proxy_ =
                                   new CpuOveruseDetectorProxy(stats_proxy)),
                           task_queue_factory) {}

  void PostTaskAndWait(bool down, AdaptReason reason) {
    rtc::Event event;
    encoder_queue()->PostTask([this, &event, reason, down] {
      if (down)
        AdaptDown(reason);
      else
        AdaptUp(reason);
      event.Set();
    });
    ASSERT_TRUE(event.Wait(5000));
  }

  // This is used as a synchronisation mechanism, to make sure that the
  // encoder queue is not blocked before we start sending it frames.
  void WaitUntilTaskQueueIsIdle() {
    rtc::Event event;
    encoder_queue()->PostTask([&event] { event.Set(); });
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
      stream.num_temporal_layers = num_temporal_layers_;
      stream.max_framerate = framerate_;
    }
    return streams;
  }

  const size_t num_temporal_layers_;
  const int framerate_;
};

// Simulates simulcast behavior and makes highest stream resolutions divisible
// by 4.
class CroppingVideoStreamFactory
    : public VideoEncoderConfig::VideoStreamFactoryInterface {
 public:
  explicit CroppingVideoStreamFactory(size_t num_temporal_layers, int framerate)
      : num_temporal_layers_(num_temporal_layers), framerate_(framerate) {
    EXPECT_GT(num_temporal_layers, 0u);
    EXPECT_GT(framerate, 0);
  }

 private:
  std::vector<VideoStream> CreateEncoderStreams(
      int width,
      int height,
      const VideoEncoderConfig& encoder_config) override {
    std::vector<VideoStream> streams = test::CreateVideoStreams(
        width - width % 4, height - height % 4, encoder_config);
    for (VideoStream& stream : streams) {
      stream.num_temporal_layers = num_temporal_layers_;
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

  absl::optional<int> last_sent_width() const { return last_width_; }
  absl::optional<int> last_sent_height() const { return last_height_; }

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
        VideoFrame adapted_frame =
            VideoFrame::Builder()
                .set_video_frame_buffer(new rtc::RefCountedObject<TestBuffer>(
                    nullptr, out_width, out_height))
                .set_timestamp_rtp(99)
                .set_timestamp_ms(99)
                .set_rotation(kVideoRotation_0)
                .build();
        adapted_frame.set_ntp_time_ms(video_frame.ntp_time_ms());
        test::FrameForwarder::IncomingCapturedFrame(adapted_frame);
        last_width_.emplace(adapted_frame.width());
        last_height_.emplace(adapted_frame.height());
      } else {
        last_width_ = absl::nullopt;
        last_height_ = absl::nullopt;
      }
    } else {
      test::FrameForwarder::IncomingCapturedFrame(video_frame);
      last_width_.emplace(video_frame.width());
      last_height_.emplace(video_frame.height());
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
  bool adaptation_enabled_ RTC_GUARDED_BY(crit_);
  rtc::VideoSinkWants last_wants_ RTC_GUARDED_BY(crit_);
  absl::optional<int> last_width_;
  absl::optional<int> last_height_;
};

// TODO(nisse): Mock only VideoStreamEncoderObserver.
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

  int GetInputFrameRate() const override {
    rtc::CritScope cs(&lock_);
    if (mock_stats_)
      return mock_stats_->input_frame_rate;
    return SendStatisticsProxy::GetInputFrameRate();
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
  absl::optional<VideoSendStream::Stats> mock_stats_ RTC_GUARDED_BY(lock_);
};

class MockBitrateObserver : public VideoBitrateAllocationObserver {
 public:
  MOCK_METHOD1(OnBitrateAllocationUpdated, void(const VideoBitrateAllocation&));
};

}  // namespace

class VideoStreamEncoderTest : public ::testing::Test {
 public:
  static const int kDefaultTimeoutMs = 30 * 1000;

  VideoStreamEncoderTest()
      : video_send_config_(VideoSendStream::Config(nullptr)),
        codec_width_(320),
        codec_height_(240),
        max_framerate_(kDefaultFramerate),
        task_queue_factory_(CreateDefaultTaskQueueFactory()),
        fake_encoder_(),
        encoder_factory_(&fake_encoder_),
        stats_proxy_(new MockableSendStatisticsProxy(
            Clock::GetRealTimeClock(),
            video_send_config_,
            webrtc::VideoEncoderConfig::ContentType::kRealtimeVideo)),
        sink_(&fake_encoder_) {}

  void SetUp() override {
    metrics::Reset();
    video_send_config_ = VideoSendStream::Config(nullptr);
    video_send_config_.encoder_settings.encoder_factory = &encoder_factory_;
    video_send_config_.encoder_settings.bitrate_allocator_factory =
        &bitrate_allocator_factory_;
    video_send_config_.rtp.payload_name = "FAKE";
    video_send_config_.rtp.payload_type = 125;

    VideoEncoderConfig video_encoder_config;
    test::FillEncoderConfiguration(kVideoCodecVP8, 1, &video_encoder_config);
    video_encoder_config.video_stream_factory =
        new rtc::RefCountedObject<VideoStreamFactory>(1, max_framerate_);
    video_encoder_config_ = video_encoder_config.Copy();

    // Framerate limit is specified by the VideoStreamFactory.
    std::vector<VideoStream> streams =
        video_encoder_config.video_stream_factory->CreateEncoderStreams(
            codec_width_, codec_height_, video_encoder_config);
    max_framerate_ = streams[0].max_framerate;
    fake_clock_.SetTime(Timestamp::us(1234));

    ConfigureEncoder(std::move(video_encoder_config));
  }

  void ConfigureEncoder(VideoEncoderConfig video_encoder_config) {
    if (video_stream_encoder_)
      video_stream_encoder_->Stop();
    video_stream_encoder_.reset(new VideoStreamEncoderUnderTest(
        stats_proxy_.get(), video_send_config_.encoder_settings,
        task_queue_factory_.get()));
    video_stream_encoder_->SetSink(&sink_, false /* rotation_applied */);
    video_stream_encoder_->SetSource(
        &video_source_, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);
    video_stream_encoder_->SetStartBitrate(kTargetBitrateBps);
    video_stream_encoder_->ConfigureEncoder(std::move(video_encoder_config),
                                            kMaxPayloadLength);
    video_stream_encoder_->WaitUntilTaskQueueIsIdle();
  }

  void ResetEncoder(const std::string& payload_name,
                    size_t num_streams,
                    size_t num_temporal_layers,
                    unsigned char num_spatial_layers,
                    bool screenshare) {
    video_send_config_.rtp.payload_name = payload_name;

    VideoEncoderConfig video_encoder_config;
    video_encoder_config.codec_type = PayloadStringToCodecType(payload_name);
    video_encoder_config.number_of_streams = num_streams;
    video_encoder_config.max_bitrate_bps =
        num_streams == 1 ? kTargetBitrateBps : kSimulcastTargetBitrateBps;
    video_encoder_config.video_stream_factory =
        new rtc::RefCountedObject<VideoStreamFactory>(num_temporal_layers,
                                                      kDefaultFramerate);
    video_encoder_config.content_type =
        screenshare ? VideoEncoderConfig::ContentType::kScreen
                    : VideoEncoderConfig::ContentType::kRealtimeVideo;
    if (payload_name == "VP9") {
      VideoCodecVP9 vp9_settings = VideoEncoder::GetDefaultVp9Settings();
      vp9_settings.numberOfSpatialLayers = num_spatial_layers;
      video_encoder_config.encoder_specific_settings =
          new rtc::RefCountedObject<
              VideoEncoderConfig::Vp9EncoderSpecificSettings>(vp9_settings);
    }
    ConfigureEncoder(std::move(video_encoder_config));
  }

  VideoFrame CreateFrame(int64_t ntp_time_ms,
                         rtc::Event* destruction_event) const {
    VideoFrame frame =
        VideoFrame::Builder()
            .set_video_frame_buffer(new rtc::RefCountedObject<TestBuffer>(
                destruction_event, codec_width_, codec_height_))
            .set_timestamp_rtp(99)
            .set_timestamp_ms(99)
            .set_rotation(kVideoRotation_0)
            .build();
    frame.set_ntp_time_ms(ntp_time_ms);
    return frame;
  }

  VideoFrame CreateFrameWithUpdatedPixel(int64_t ntp_time_ms,
                                         rtc::Event* destruction_event,
                                         int offset_x) const {
    VideoFrame frame =
        VideoFrame::Builder()
            .set_video_frame_buffer(new rtc::RefCountedObject<TestBuffer>(
                destruction_event, codec_width_, codec_height_))
            .set_timestamp_rtp(99)
            .set_timestamp_ms(99)
            .set_rotation(kVideoRotation_0)
            .set_update_rect({offset_x, 0, 1, 1})
            .build();
    frame.set_ntp_time_ms(ntp_time_ms);
    return frame;
  }

  VideoFrame CreateFrame(int64_t ntp_time_ms, int width, int height) const {
    VideoFrame frame =
        VideoFrame::Builder()
            .set_video_frame_buffer(
                new rtc::RefCountedObject<TestBuffer>(nullptr, width, height))
            .set_timestamp_rtp(99)
            .set_timestamp_ms(99)
            .set_rotation(kVideoRotation_0)
            .build();
    frame.set_ntp_time_ms(ntp_time_ms);
    frame.set_timestamp_us(ntp_time_ms * 1000);
    return frame;
  }

  VideoFrame CreateFakeNativeFrame(int64_t ntp_time_ms,
                                   rtc::Event* destruction_event,
                                   int width,
                                   int height) const {
    VideoFrame frame =
        VideoFrame::Builder()
            .set_video_frame_buffer(new rtc::RefCountedObject<FakeNativeBuffer>(
                destruction_event, width, height))
            .set_timestamp_rtp(99)
            .set_timestamp_ms(99)
            .set_rotation(kVideoRotation_0)
            .build();
    frame.set_ntp_time_ms(ntp_time_ms);
    return frame;
  }

  VideoFrame CreateFakeNativeFrame(int64_t ntp_time_ms,
                                   rtc::Event* destruction_event) const {
    return CreateFakeNativeFrame(ntp_time_ms, destruction_event, codec_width_,
                                 codec_height_);
  }

  void VerifyAllocatedBitrate(const VideoBitrateAllocation& expected_bitrate) {
    MockBitrateObserver bitrate_observer;
    video_stream_encoder_->SetBitrateAllocationObserver(&bitrate_observer);

    EXPECT_CALL(bitrate_observer, OnBitrateAllocationUpdated(expected_bitrate))
        .Times(1);
    video_stream_encoder_->OnBitrateUpdated(DataRate::bps(kTargetBitrateBps),
                                            DataRate::bps(kTargetBitrateBps), 0,
                                            0);

    video_source_.IncomingCapturedFrame(
        CreateFrame(1, codec_width_, codec_height_));
    WaitForEncodedFrame(1);
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

  void VerifyFpsMaxResolutionMax(const rtc::VideoSinkWants& wants) {
    EXPECT_EQ(kDefaultFramerate, wants.max_framerate_fps);
    EXPECT_EQ(std::numeric_limits<int>::max(), wants.max_pixel_count);
    EXPECT_FALSE(wants.target_pixel_count);
  }

  void VerifyFpsMaxResolutionLt(const rtc::VideoSinkWants& wants1,
                                const rtc::VideoSinkWants& wants2) {
    EXPECT_EQ(kDefaultFramerate, wants1.max_framerate_fps);
    EXPECT_LT(wants1.max_pixel_count, wants2.max_pixel_count);
    EXPECT_GT(wants1.max_pixel_count, 0);
  }

  void VerifyFpsMaxResolutionGt(const rtc::VideoSinkWants& wants1,
                                const rtc::VideoSinkWants& wants2) {
    EXPECT_EQ(kDefaultFramerate, wants1.max_framerate_fps);
    EXPECT_GT(wants1.max_pixel_count, wants2.max_pixel_count);
  }

  void VerifyFpsMaxResolutionEq(const rtc::VideoSinkWants& wants1,
                                const rtc::VideoSinkWants& wants2) {
    EXPECT_EQ(kDefaultFramerate, wants1.max_framerate_fps);
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
    EXPECT_EQ(kDefaultFramerate, wants.max_framerate_fps);
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

  void VerifyBalancedModeFpsRange(const rtc::VideoSinkWants& wants,
                                  int last_frame_pixels) {
    // Balanced mode should always scale FPS to the desired range before
    // attempting to scale resolution.
    int fps_limit = wants.max_framerate_fps;
    if (last_frame_pixels <= 320 * 240) {
      EXPECT_TRUE(7 <= fps_limit && fps_limit <= 10);
    } else if (last_frame_pixels <= 480 * 270) {
      EXPECT_TRUE(10 <= fps_limit && fps_limit <= 15);
    } else if (last_frame_pixels <= 640 * 480) {
      EXPECT_LE(15, fps_limit);
    } else {
      EXPECT_EQ(kDefaultFramerate, fps_limit);
    }
  }

  void WaitForEncodedFrame(int64_t expected_ntp_time) {
    sink_.WaitForEncodedFrame(expected_ntp_time);
    fake_clock_.AdvanceTime(TimeDelta::seconds(1) / max_framerate_);
  }

  bool TimedWaitForEncodedFrame(int64_t expected_ntp_time, int64_t timeout_ms) {
    bool ok = sink_.TimedWaitForEncodedFrame(expected_ntp_time, timeout_ms);
    fake_clock_.AdvanceTime(TimeDelta::seconds(1) / max_framerate_);
    return ok;
  }

  void WaitForEncodedFrame(uint32_t expected_width, uint32_t expected_height) {
    sink_.WaitForEncodedFrame(expected_width, expected_height);
    fake_clock_.AdvanceTime(TimeDelta::seconds(1) / max_framerate_);
  }

  void ExpectDroppedFrame() {
    sink_.ExpectDroppedFrame();
    fake_clock_.AdvanceTime(TimeDelta::seconds(1) / max_framerate_);
  }

  bool WaitForFrame(int64_t timeout_ms) {
    bool ok = sink_.WaitForFrame(timeout_ms);
    fake_clock_.AdvanceTime(TimeDelta::seconds(1) / max_framerate_);
    return ok;
  }

  class TestEncoder : public test::FakeEncoder {
   public:
    TestEncoder() : FakeEncoder(Clock::GetRealTimeClock()) {}

    VideoCodec codec_config() const {
      rtc::CritScope lock(&crit_sect_);
      return config_;
    }

    void BlockNextEncode() {
      rtc::CritScope lock(&local_crit_sect_);
      block_next_encode_ = true;
    }

    VideoEncoder::EncoderInfo GetEncoderInfo() const override {
      rtc::CritScope lock(&local_crit_sect_);
      EncoderInfo info;
      if (initialized_ == EncoderState::kInitialized) {
        if (quality_scaling_) {
          info.scaling_settings =
              VideoEncoder::ScalingSettings(1, 2, kMinPixelsPerFrame);
        }
        info.is_hardware_accelerated = is_hardware_accelerated_;
        for (int i = 0; i < kMaxSpatialLayers; ++i) {
          if (temporal_layers_supported_[i]) {
            int num_layers = temporal_layers_supported_[i].value() ? 2 : 1;
            info.fps_allocation[i].resize(num_layers);
          }
        }
      }

      info.resolution_bitrate_limits = resolution_bitrate_limits_;
      return info;
    }

    int32_t RegisterEncodeCompleteCallback(
        EncodedImageCallback* callback) override {
      rtc::CritScope lock(&local_crit_sect_);
      encoded_image_callback_ = callback;
      return FakeEncoder::RegisterEncodeCompleteCallback(callback);
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

    void SetIsHardwareAccelerated(bool is_hardware_accelerated) {
      rtc::CritScope lock(&local_crit_sect_);
      is_hardware_accelerated_ = is_hardware_accelerated;
    }

    void SetTemporalLayersSupported(size_t spatial_idx, bool supported) {
      RTC_DCHECK_LT(spatial_idx, kMaxSpatialLayers);
      rtc::CritScope lock(&local_crit_sect_);
      temporal_layers_supported_[spatial_idx] = supported;
    }

    void SetResolutionBitrateLimits(
        std::vector<ResolutionBitrateLimits> thresholds) {
      rtc::CritScope cs(&local_crit_sect_);
      resolution_bitrate_limits_ = thresholds;
    }

    void ForceInitEncodeFailure(bool force_failure) {
      rtc::CritScope lock(&local_crit_sect_);
      force_init_encode_failed_ = force_failure;
    }

    void SimulateOvershoot(double rate_factor) {
      rtc::CritScope lock(&local_crit_sect_);
      rate_factor_ = rate_factor;
    }

    uint32_t GetLastFramerate() const {
      rtc::CritScope lock(&local_crit_sect_);
      return last_framerate_;
    }

    VideoFrame::UpdateRect GetLastUpdateRect() const {
      rtc::CritScope lock(&local_crit_sect_);
      return last_update_rect_;
    }

    const std::vector<VideoFrameType>& LastFrameTypes() const {
      rtc::CritScope lock(&local_crit_sect_);
      return last_frame_types_;
    }

    void InjectFrame(const VideoFrame& input_image, bool keyframe) {
      const std::vector<VideoFrameType> frame_type = {
          keyframe ? VideoFrameType::kVideoFrameKey
                   : VideoFrameType::kVideoFrameDelta};
      {
        rtc::CritScope lock(&local_crit_sect_);
        last_frame_types_ = frame_type;
      }
      FakeEncoder::Encode(input_image, &frame_type);
    }

    void InjectEncodedImage(const EncodedImage& image) {
      rtc::CritScope lock(&local_crit_sect_);
      encoded_image_callback_->OnEncodedImage(image, nullptr, nullptr);
    }

    void InjectEncodedImage(const EncodedImage& image,
                            const CodecSpecificInfo* codec_specific_info,
                            const RTPFragmentationHeader* fragmentation) {
      rtc::CritScope lock(&local_crit_sect_);
      encoded_image_callback_->OnEncodedImage(image, codec_specific_info,
                                              fragmentation);
    }

    void ExpectNullFrame() {
      rtc::CritScope lock(&local_crit_sect_);
      expect_null_frame_ = true;
    }

    absl::optional<VideoEncoder::RateControlParameters>
    GetAndResetLastRateControlSettings() {
      auto settings = last_rate_control_settings_;
      last_rate_control_settings_.reset();
      return settings;
    }

    int GetNumEncoderInitializations() const {
      rtc::CritScope lock(&local_crit_sect_);
      return num_encoder_initializations_;
    }

   private:
    int32_t Encode(const VideoFrame& input_image,
                   const std::vector<VideoFrameType>* frame_types) override {
      bool block_encode;
      {
        rtc::CritScope lock(&local_crit_sect_);
        if (expect_null_frame_) {
          EXPECT_EQ(input_image.timestamp(), 0u);
          EXPECT_EQ(input_image.width(), 1);
          last_frame_types_ = *frame_types;
          expect_null_frame_ = false;
        } else {
          EXPECT_GT(input_image.timestamp(), timestamp_);
          EXPECT_GT(input_image.ntp_time_ms(), ntp_time_ms_);
          EXPECT_EQ(input_image.timestamp(), input_image.ntp_time_ms() * 90);
        }

        timestamp_ = input_image.timestamp();
        ntp_time_ms_ = input_image.ntp_time_ms();
        last_input_width_ = input_image.width();
        last_input_height_ = input_image.height();
        block_encode = block_next_encode_;
        block_next_encode_ = false;
        last_update_rect_ = input_image.update_rect();
        last_frame_types_ = *frame_types;
      }
      int32_t result = FakeEncoder::Encode(input_image, frame_types);
      if (block_encode)
        EXPECT_TRUE(continue_encode_event_.Wait(kDefaultTimeoutMs));
      return result;
    }

    int32_t InitEncode(const VideoCodec* config,
                       const Settings& settings) override {
      int res = FakeEncoder::InitEncode(config, settings);

      rtc::CritScope lock(&local_crit_sect_);
      EXPECT_EQ(initialized_, EncoderState::kUninitialized);

      ++num_encoder_initializations_;

      if (config->codecType == kVideoCodecVP8) {
        // Simulate setting up temporal layers, in order to validate the life
        // cycle of these objects.
        Vp8TemporalLayersFactory factory;
        frame_buffer_controller_ =
            factory.Create(*config, settings, &fec_controller_override_);
      }
      if (force_init_encode_failed_) {
        initialized_ = EncoderState::kInitializationFailed;
        return -1;
      }

      initialized_ = EncoderState::kInitialized;
      return res;
    }

    int32_t Release() override {
      rtc::CritScope lock(&local_crit_sect_);
      EXPECT_NE(initialized_, EncoderState::kUninitialized);
      initialized_ = EncoderState::kUninitialized;
      return FakeEncoder::Release();
    }

    void SetRates(const RateControlParameters& parameters) {
      rtc::CritScope lock(&local_crit_sect_);
      VideoBitrateAllocation adjusted_rate_allocation;
      for (size_t si = 0; si < kMaxSpatialLayers; ++si) {
        for (size_t ti = 0; ti < kMaxTemporalStreams; ++ti) {
          if (parameters.bitrate.HasBitrate(si, ti)) {
            adjusted_rate_allocation.SetBitrate(
                si, ti,
                static_cast<uint32_t>(parameters.bitrate.GetBitrate(si, ti) *
                                      rate_factor_));
          }
        }
      }
      last_framerate_ = static_cast<uint32_t>(parameters.framerate_fps + 0.5);
      last_rate_control_settings_ = parameters;
      RateControlParameters adjusted_paramters = parameters;
      adjusted_paramters.bitrate = adjusted_rate_allocation;
      FakeEncoder::SetRates(adjusted_paramters);
    }

    rtc::CriticalSection local_crit_sect_;
    enum class EncoderState {
      kUninitialized,
      kInitializationFailed,
      kInitialized
    } initialized_ RTC_GUARDED_BY(local_crit_sect_) =
        EncoderState::kUninitialized;
    bool block_next_encode_ RTC_GUARDED_BY(local_crit_sect_) = false;
    rtc::Event continue_encode_event_;
    uint32_t timestamp_ RTC_GUARDED_BY(local_crit_sect_) = 0;
    int64_t ntp_time_ms_ RTC_GUARDED_BY(local_crit_sect_) = 0;
    int last_input_width_ RTC_GUARDED_BY(local_crit_sect_) = 0;
    int last_input_height_ RTC_GUARDED_BY(local_crit_sect_) = 0;
    bool quality_scaling_ RTC_GUARDED_BY(local_crit_sect_) = true;
    bool is_hardware_accelerated_ RTC_GUARDED_BY(local_crit_sect_) = false;
    std::unique_ptr<Vp8FrameBufferController> frame_buffer_controller_
        RTC_GUARDED_BY(local_crit_sect_);
    absl::optional<bool>
        temporal_layers_supported_[kMaxSpatialLayers] RTC_GUARDED_BY(
            local_crit_sect_);
    bool force_init_encode_failed_ RTC_GUARDED_BY(local_crit_sect_) = false;
    double rate_factor_ RTC_GUARDED_BY(local_crit_sect_) = 1.0;
    uint32_t last_framerate_ RTC_GUARDED_BY(local_crit_sect_) = 0;
    absl::optional<VideoEncoder::RateControlParameters>
        last_rate_control_settings_;
    VideoFrame::UpdateRect last_update_rect_
        RTC_GUARDED_BY(local_crit_sect_) = {0, 0, 0, 0};
    std::vector<VideoFrameType> last_frame_types_;
    bool expect_null_frame_ = false;
    EncodedImageCallback* encoded_image_callback_
        RTC_GUARDED_BY(local_crit_sect_) = nullptr;
    MockFecControllerOverride fec_controller_override_;
    int num_encoder_initializations_ RTC_GUARDED_BY(local_crit_sect_) = 0;
    std::vector<ResolutionBitrateLimits> resolution_bitrate_limits_
        RTC_GUARDED_BY(local_crit_sect_);
  };

  class TestSink : public VideoStreamEncoder::EncoderSink {
   public:
    explicit TestSink(TestEncoder* test_encoder)
        : test_encoder_(test_encoder) {}

    void WaitForEncodedFrame(int64_t expected_ntp_time) {
      EXPECT_TRUE(
          TimedWaitForEncodedFrame(expected_ntp_time, kDefaultTimeoutMs));
    }

    bool TimedWaitForEncodedFrame(int64_t expected_ntp_time,
                                  int64_t timeout_ms) {
      uint32_t timestamp = 0;
      if (!encoded_frame_event_.Wait(timeout_ms))
        return false;
      {
        rtc::CritScope lock(&crit_);
        timestamp = last_timestamp_;
      }
      test_encoder_->CheckLastTimeStampsMatch(expected_ntp_time, timestamp);
      return true;
    }

    void WaitForEncodedFrame(uint32_t expected_width,
                             uint32_t expected_height) {
      EXPECT_TRUE(encoded_frame_event_.Wait(kDefaultTimeoutMs));
      CheckLastFrameSizeMatches(expected_width, expected_height);
    }

    void CheckLastFrameSizeMatches(uint32_t expected_width,
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

    void CheckLastFrameRotationMatches(VideoRotation expected_rotation) {
      VideoRotation rotation;
      {
        rtc::CritScope lock(&crit_);
        rotation = last_rotation_;
      }
      EXPECT_EQ(expected_rotation, rotation);
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

    void SetNumExpectedLayers(size_t num_layers) {
      rtc::CritScope lock(&crit_);
      num_expected_layers_ = num_layers;
    }

    int64_t GetLastCaptureTimeMs() const {
      rtc::CritScope lock(&crit_);
      return last_capture_time_ms_;
    }

    std::vector<uint8_t> GetLastEncodedImageData() {
      rtc::CritScope lock(&crit_);
      return std::move(last_encoded_image_data_);
    }

    RTPFragmentationHeader GetLastFragmentation() {
      rtc::CritScope lock(&crit_);
      return std::move(last_fragmentation_);
    }

   private:
    Result OnEncodedImage(
        const EncodedImage& encoded_image,
        const CodecSpecificInfo* codec_specific_info,
        const RTPFragmentationHeader* fragmentation) override {
      rtc::CritScope lock(&crit_);
      EXPECT_TRUE(expect_frames_);
      last_encoded_image_data_ = std::vector<uint8_t>(
          encoded_image.data(), encoded_image.data() + encoded_image.size());
      if (fragmentation) {
        last_fragmentation_.CopyFrom(*fragmentation);
      }
      uint32_t timestamp = encoded_image.Timestamp();
      if (last_timestamp_ != timestamp) {
        num_received_layers_ = 1;
      } else {
        ++num_received_layers_;
      }
      last_timestamp_ = timestamp;
      last_capture_time_ms_ = encoded_image.capture_time_ms_;
      last_width_ = encoded_image._encodedWidth;
      last_height_ = encoded_image._encodedHeight;
      last_rotation_ = encoded_image.rotation_;
      if (num_received_layers_ == num_expected_layers_) {
        encoded_frame_event_.Set();
      }
      return Result(Result::OK, last_timestamp_);
    }

    void OnEncoderConfigurationChanged(
        std::vector<VideoStream> streams,
        VideoEncoderConfig::ContentType content_type,
        int min_transmit_bitrate_bps) override {
      rtc::CritScope lock(&crit_);
      ++number_of_reconfigurations_;
      min_transmit_bitrate_bps_ = min_transmit_bitrate_bps;
    }

    rtc::CriticalSection crit_;
    TestEncoder* test_encoder_;
    rtc::Event encoded_frame_event_;
    std::vector<uint8_t> last_encoded_image_data_;
    RTPFragmentationHeader last_fragmentation_;
    uint32_t last_timestamp_ = 0;
    int64_t last_capture_time_ms_ = 0;
    uint32_t last_height_ = 0;
    uint32_t last_width_ = 0;
    VideoRotation last_rotation_ = kVideoRotation_0;
    size_t num_expected_layers_ = 1;
    size_t num_received_layers_ = 0;
    bool expect_frames_ = true;
    int number_of_reconfigurations_ = 0;
    int min_transmit_bitrate_bps_ = 0;
  };

  class VideoBitrateAllocatorProxyFactory
      : public VideoBitrateAllocatorFactory {
   public:
    VideoBitrateAllocatorProxyFactory()
        : bitrate_allocator_factory_(
              CreateBuiltinVideoBitrateAllocatorFactory()) {}

    std::unique_ptr<VideoBitrateAllocator> CreateVideoBitrateAllocator(
        const VideoCodec& codec) override {
      rtc::CritScope lock(&crit_);
      codec_config_ = codec;
      return bitrate_allocator_factory_->CreateVideoBitrateAllocator(codec);
    }

    VideoCodec codec_config() const {
      rtc::CritScope lock(&crit_);
      return codec_config_;
    }

   private:
    std::unique_ptr<VideoBitrateAllocatorFactory> bitrate_allocator_factory_;

    rtc::CriticalSection crit_;
    VideoCodec codec_config_ RTC_GUARDED_BY(crit_);
  };

  VideoSendStream::Config video_send_config_;
  VideoEncoderConfig video_encoder_config_;
  int codec_width_;
  int codec_height_;
  int max_framerate_;
  rtc::ScopedFakeClock fake_clock_;
  const std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  TestEncoder fake_encoder_;
  test::VideoEncoderProxyFactory encoder_factory_;
  VideoBitrateAllocatorProxyFactory bitrate_allocator_factory_;
  std::unique_ptr<MockableSendStatisticsProxy> stats_proxy_;
  TestSink sink_;
  AdaptingFrameForwarder video_source_;
  std::unique_ptr<VideoStreamEncoderUnderTest> video_stream_encoder_;
};

TEST_F(VideoStreamEncoderTest, EncodeOneFrame) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  rtc::Event frame_destroyed_event;
  video_source_.IncomingCapturedFrame(CreateFrame(1, &frame_destroyed_event));
  WaitForEncodedFrame(1);
  EXPECT_TRUE(frame_destroyed_event.Wait(kDefaultTimeoutMs));
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, DropsFramesBeforeFirstOnBitrateUpdated) {
  // Dropped since no target bitrate has been set.
  rtc::Event frame_destroyed_event;
  // The encoder will cache up to one frame for a short duration. Adding two
  // frames means that the first frame will be dropped and the second frame will
  // be sent when the encoder is enabled.
  video_source_.IncomingCapturedFrame(CreateFrame(1, &frame_destroyed_event));
  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));
  EXPECT_TRUE(frame_destroyed_event.Wait(kDefaultTimeoutMs));

  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // The pending frame should be received.
  WaitForEncodedFrame(2);
  video_source_.IncomingCapturedFrame(CreateFrame(3, nullptr));

  WaitForEncodedFrame(3);
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, DropsFramesWhenRateSetToZero) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  WaitForEncodedFrame(1);

  video_stream_encoder_->OnBitrateUpdated(DataRate::bps(0), DataRate::bps(0), 0,
                                          0);
  // The encoder will cache up to one frame for a short duration. Adding two
  // frames means that the first frame will be dropped and the second frame will
  // be sent when the encoder is resumed.
  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));
  video_source_.IncomingCapturedFrame(CreateFrame(3, nullptr));

  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  WaitForEncodedFrame(3);
  video_source_.IncomingCapturedFrame(CreateFrame(4, nullptr));
  WaitForEncodedFrame(4);
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, DropsFramesWithSameOrOldNtpTimestamp) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  WaitForEncodedFrame(1);

  // This frame will be dropped since it has the same ntp timestamp.
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));

  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));
  WaitForEncodedFrame(2);
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, DropsFrameAfterStop) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  WaitForEncodedFrame(1);

  video_stream_encoder_->Stop();
  sink_.SetExpectNoFrames();
  rtc::Event frame_destroyed_event;
  video_source_.IncomingCapturedFrame(CreateFrame(2, &frame_destroyed_event));
  EXPECT_TRUE(frame_destroyed_event.Wait(kDefaultTimeoutMs));
}

TEST_F(VideoStreamEncoderTest, DropsPendingFramesOnSlowEncode) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  fake_encoder_.BlockNextEncode();
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  WaitForEncodedFrame(1);
  // Here, the encoder thread will be blocked in the TestEncoder waiting for a
  // call to ContinueEncode.
  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));
  video_source_.IncomingCapturedFrame(CreateFrame(3, nullptr));
  fake_encoder_.ContinueEncode();
  WaitForEncodedFrame(3);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, DropFrameWithFailedI420Conversion) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  rtc::Event frame_destroyed_event;
  video_source_.IncomingCapturedFrame(
      CreateFakeNativeFrame(1, &frame_destroyed_event));
  ExpectDroppedFrame();
  EXPECT_TRUE(frame_destroyed_event.Wait(kDefaultTimeoutMs));
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, DropFrameWithFailedI420ConversionWithCrop) {
  // Use the cropping factory.
  video_encoder_config_.video_stream_factory =
      new rtc::RefCountedObject<CroppingVideoStreamFactory>(1, 30);
  video_stream_encoder_->ConfigureEncoder(std::move(video_encoder_config_),
                                          kMaxPayloadLength);
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();

  // Capture a frame at codec_width_/codec_height_.
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  WaitForEncodedFrame(1);
  // The encoder will have been configured once.
  EXPECT_EQ(1, sink_.number_of_reconfigurations());
  EXPECT_EQ(codec_width_, fake_encoder_.codec_config().width);
  EXPECT_EQ(codec_height_, fake_encoder_.codec_config().height);

  // Now send in a fake frame that needs to be cropped as the width/height
  // aren't divisible by 4 (see CreateEncoderStreams above).
  rtc::Event frame_destroyed_event;
  video_source_.IncomingCapturedFrame(CreateFakeNativeFrame(
      2, &frame_destroyed_event, codec_width_ + 1, codec_height_ + 1));
  ExpectDroppedFrame();
  EXPECT_TRUE(frame_destroyed_event.Wait(kDefaultTimeoutMs));
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       ConfigureEncoderTriggersOnEncoderConfigurationChanged) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  EXPECT_EQ(0, sink_.number_of_reconfigurations());

  // Capture a frame and wait for it to synchronize with the encoder thread.
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  WaitForEncodedFrame(1);
  // The encoder will have been configured once when the first frame is
  // received.
  EXPECT_EQ(1, sink_.number_of_reconfigurations());

  VideoEncoderConfig video_encoder_config;
  test::FillEncoderConfiguration(kVideoCodecVP8, 1, &video_encoder_config);
  video_encoder_config.min_transmit_bitrate_bps = 9999;
  video_stream_encoder_->ConfigureEncoder(std::move(video_encoder_config),
                                          kMaxPayloadLength);

  // Capture a frame and wait for it to synchronize with the encoder thread.
  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));
  WaitForEncodedFrame(2);
  EXPECT_EQ(2, sink_.number_of_reconfigurations());
  EXPECT_EQ(9999, sink_.last_min_transmit_bitrate());

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, FrameResolutionChangeReconfigureEncoder) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Capture a frame and wait for it to synchronize with the encoder thread.
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  WaitForEncodedFrame(1);
  // The encoder will have been configured once.
  EXPECT_EQ(1, sink_.number_of_reconfigurations());
  EXPECT_EQ(codec_width_, fake_encoder_.codec_config().width);
  EXPECT_EQ(codec_height_, fake_encoder_.codec_config().height);

  codec_width_ *= 2;
  codec_height_ *= 2;
  // Capture a frame with a higher resolution and wait for it to synchronize
  // with the encoder thread.
  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));
  WaitForEncodedFrame(2);
  EXPECT_EQ(codec_width_, fake_encoder_.codec_config().width);
  EXPECT_EQ(codec_height_, fake_encoder_.codec_config().height);
  EXPECT_EQ(2, sink_.number_of_reconfigurations());

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       EncoderInstanceDestroyedBeforeAnotherInstanceCreated) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Capture a frame and wait for it to synchronize with the encoder thread.
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  WaitForEncodedFrame(1);

  VideoEncoderConfig video_encoder_config;
  test::FillEncoderConfiguration(kVideoCodecVP8, 1, &video_encoder_config);
  // Changing the max payload data length recreates encoder.
  video_stream_encoder_->ConfigureEncoder(std::move(video_encoder_config),
                                          kMaxPayloadLength / 2);

  // Capture a frame and wait for it to synchronize with the encoder thread.
  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));
  WaitForEncodedFrame(2);
  EXPECT_EQ(1, encoder_factory_.GetMaxNumberOfSimultaneousEncoderInstances());

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, BitrateLimitsChangeReconfigureRateAllocator) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  VideoEncoderConfig video_encoder_config;
  test::FillEncoderConfiguration(kVideoCodecVP8, 1, &video_encoder_config);
  video_encoder_config.max_bitrate_bps = kTargetBitrateBps;
  video_stream_encoder_->SetStartBitrate(kStartBitrateBps);
  video_stream_encoder_->ConfigureEncoder(video_encoder_config.Copy(),
                                          kMaxPayloadLength);

  // Capture a frame and wait for it to synchronize with the encoder thread.
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  WaitForEncodedFrame(1);
  // The encoder will have been configured once when the first frame is
  // received.
  EXPECT_EQ(1, sink_.number_of_reconfigurations());
  EXPECT_EQ(kTargetBitrateBps,
            bitrate_allocator_factory_.codec_config().maxBitrate * 1000);
  EXPECT_EQ(kStartBitrateBps,
            bitrate_allocator_factory_.codec_config().startBitrate * 1000);

  test::FillEncoderConfiguration(kVideoCodecVP8, 1,
                                 &video_encoder_config);  //???
  video_encoder_config.max_bitrate_bps = kTargetBitrateBps * 2;
  video_stream_encoder_->SetStartBitrate(kStartBitrateBps * 2);
  video_stream_encoder_->ConfigureEncoder(std::move(video_encoder_config),
                                          kMaxPayloadLength);

  // Capture a frame and wait for it to synchronize with the encoder thread.
  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));
  WaitForEncodedFrame(2);
  EXPECT_EQ(2, sink_.number_of_reconfigurations());
  // Bitrate limits have changed - rate allocator should be reconfigured,
  // encoder should not be reconfigured.
  EXPECT_EQ(kTargetBitrateBps * 2,
            bitrate_allocator_factory_.codec_config().maxBitrate * 1000);
  EXPECT_EQ(kStartBitrateBps * 2,
            bitrate_allocator_factory_.codec_config().startBitrate * 1000);
  EXPECT_EQ(1, fake_encoder_.GetNumEncoderInitializations());

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       EncoderRecommendedBitrateLimitsDoNotOverrideAppBitrateLimits) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  VideoEncoderConfig video_encoder_config;
  test::FillEncoderConfiguration(kVideoCodecVP8, 1, &video_encoder_config);
  video_encoder_config.max_bitrate_bps = 0;
  video_encoder_config.simulcast_layers[0].min_bitrate_bps = 0;
  video_stream_encoder_->ConfigureEncoder(video_encoder_config.Copy(),
                                          kMaxPayloadLength);

  video_source_.IncomingCapturedFrame(CreateFrame(1, 360, 180));
  WaitForEncodedFrame(1);

  // Get the default bitrate limits and use them as baseline for custom
  // application and encoder recommended limits.
  const uint32_t kDefaultMinBitrateKbps =
      bitrate_allocator_factory_.codec_config().minBitrate;
  const uint32_t kDefaultMaxBitrateKbps =
      bitrate_allocator_factory_.codec_config().maxBitrate;
  const uint32_t kEncMinBitrateKbps = kDefaultMinBitrateKbps * 2;
  const uint32_t kEncMaxBitrateKbps = kDefaultMaxBitrateKbps * 2;
  const uint32_t kAppMinBitrateKbps = kDefaultMinBitrateKbps * 3;
  const uint32_t kAppMaxBitrateKbps = kDefaultMaxBitrateKbps * 3;

  const VideoEncoder::ResolutionBitrateLimits encoder_bitrate_limits(
      codec_width_ * codec_height_, kEncMinBitrateKbps * 1000,
      kEncMinBitrateKbps * 1000, kEncMaxBitrateKbps * 1000);
  fake_encoder_.SetResolutionBitrateLimits({encoder_bitrate_limits});

  // Change resolution. This will trigger encoder re-configuration and video
  // stream encoder will pick up the bitrate limits recommended by encoder.
  video_source_.IncomingCapturedFrame(CreateFrame(2, 640, 360));
  WaitForEncodedFrame(2);
  video_source_.IncomingCapturedFrame(CreateFrame(3, 360, 180));
  WaitForEncodedFrame(3);

  // App bitrate limits are not set - bitrate limits recommended by encoder
  // should be used.
  EXPECT_EQ(kEncMaxBitrateKbps,
            bitrate_allocator_factory_.codec_config().maxBitrate);
  EXPECT_EQ(kEncMinBitrateKbps,
            bitrate_allocator_factory_.codec_config().minBitrate);

  video_encoder_config.max_bitrate_bps = kAppMaxBitrateKbps * 1000;
  video_encoder_config.simulcast_layers[0].min_bitrate_bps = 0;
  video_stream_encoder_->ConfigureEncoder(video_encoder_config.Copy(),
                                          kMaxPayloadLength);
  video_source_.IncomingCapturedFrame(CreateFrame(4, nullptr));
  WaitForEncodedFrame(4);

  // App limited the max bitrate - bitrate limits recommended by encoder should
  // not be applied.
  EXPECT_EQ(kAppMaxBitrateKbps,
            bitrate_allocator_factory_.codec_config().maxBitrate);
  EXPECT_EQ(kDefaultMinBitrateKbps,
            bitrate_allocator_factory_.codec_config().minBitrate);

  video_encoder_config.max_bitrate_bps = 0;
  video_encoder_config.simulcast_layers[0].min_bitrate_bps =
      kAppMinBitrateKbps * 1000;
  video_stream_encoder_->ConfigureEncoder(video_encoder_config.Copy(),
                                          kMaxPayloadLength);
  video_source_.IncomingCapturedFrame(CreateFrame(5, nullptr));
  WaitForEncodedFrame(5);

  // App limited the min bitrate - bitrate limits recommended by encoder should
  // not be applied.
  EXPECT_EQ(kDefaultMaxBitrateKbps,
            bitrate_allocator_factory_.codec_config().maxBitrate);
  EXPECT_EQ(kAppMinBitrateKbps,
            bitrate_allocator_factory_.codec_config().minBitrate);

  video_encoder_config.max_bitrate_bps = kAppMaxBitrateKbps * 1000;
  video_encoder_config.simulcast_layers[0].min_bitrate_bps =
      kAppMinBitrateKbps * 1000;
  video_stream_encoder_->ConfigureEncoder(std::move(video_encoder_config),
                                          kMaxPayloadLength);
  video_source_.IncomingCapturedFrame(CreateFrame(6, nullptr));
  WaitForEncodedFrame(6);

  // App limited both min and max bitrates - bitrate limits recommended by
  // encoder should not be applied.
  EXPECT_EQ(kAppMaxBitrateKbps,
            bitrate_allocator_factory_.codec_config().maxBitrate);
  EXPECT_EQ(kAppMinBitrateKbps,
            bitrate_allocator_factory_.codec_config().minBitrate);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       EncoderRecommendedMaxAndMinBitratesUsedForGivenResolution) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  const VideoEncoder::ResolutionBitrateLimits encoder_bitrate_limits_270p(
      480 * 270, 34 * 1000, 12 * 1000, 1234 * 1000);
  const VideoEncoder::ResolutionBitrateLimits encoder_bitrate_limits_360p(
      640 * 360, 43 * 1000, 21 * 1000, 2345 * 1000);
  fake_encoder_.SetResolutionBitrateLimits(
      {encoder_bitrate_limits_270p, encoder_bitrate_limits_360p});

  VideoEncoderConfig video_encoder_config;
  test::FillEncoderConfiguration(kVideoCodecVP8, 1, &video_encoder_config);
  video_encoder_config.max_bitrate_bps = 0;
  video_stream_encoder_->ConfigureEncoder(video_encoder_config.Copy(),
                                          kMaxPayloadLength);

  // 270p. The bitrate limits recommended by encoder for 270p should be used.
  video_source_.IncomingCapturedFrame(CreateFrame(1, 480, 270));
  WaitForEncodedFrame(1);
  EXPECT_EQ(static_cast<uint32_t>(encoder_bitrate_limits_270p.min_bitrate_bps),
            bitrate_allocator_factory_.codec_config().minBitrate * 1000);
  EXPECT_EQ(static_cast<uint32_t>(encoder_bitrate_limits_270p.max_bitrate_bps),
            bitrate_allocator_factory_.codec_config().maxBitrate * 1000);

  // 360p. The bitrate limits recommended by encoder for 360p should be used.
  video_source_.IncomingCapturedFrame(CreateFrame(2, 640, 360));
  WaitForEncodedFrame(2);
  EXPECT_EQ(static_cast<uint32_t>(encoder_bitrate_limits_360p.min_bitrate_bps),
            bitrate_allocator_factory_.codec_config().minBitrate * 1000);
  EXPECT_EQ(static_cast<uint32_t>(encoder_bitrate_limits_360p.max_bitrate_bps),
            bitrate_allocator_factory_.codec_config().maxBitrate * 1000);

  // Resolution between 270p and 360p. The bitrate limits recommended by
  // encoder for 360p should be used.
  video_source_.IncomingCapturedFrame(
      CreateFrame(3, (640 + 480) / 2, (360 + 270) / 2));
  WaitForEncodedFrame(3);
  EXPECT_EQ(static_cast<uint32_t>(encoder_bitrate_limits_360p.min_bitrate_bps),
            bitrate_allocator_factory_.codec_config().minBitrate * 1000);
  EXPECT_EQ(static_cast<uint32_t>(encoder_bitrate_limits_360p.max_bitrate_bps),
            bitrate_allocator_factory_.codec_config().maxBitrate * 1000);

  // Resolution higher than 360p. The caps recommended by encoder should be
  // ignored.
  video_source_.IncomingCapturedFrame(CreateFrame(4, 960, 540));
  WaitForEncodedFrame(4);
  EXPECT_NE(static_cast<uint32_t>(encoder_bitrate_limits_270p.min_bitrate_bps),
            bitrate_allocator_factory_.codec_config().minBitrate * 1000);
  EXPECT_NE(static_cast<uint32_t>(encoder_bitrate_limits_270p.max_bitrate_bps),
            bitrate_allocator_factory_.codec_config().maxBitrate * 1000);
  EXPECT_NE(static_cast<uint32_t>(encoder_bitrate_limits_360p.min_bitrate_bps),
            bitrate_allocator_factory_.codec_config().minBitrate * 1000);
  EXPECT_NE(static_cast<uint32_t>(encoder_bitrate_limits_360p.max_bitrate_bps),
            bitrate_allocator_factory_.codec_config().maxBitrate * 1000);

  // Resolution lower than 270p. The max bitrate limit recommended by encoder
  // for 270p should be used.
  video_source_.IncomingCapturedFrame(CreateFrame(5, 320, 180));
  WaitForEncodedFrame(5);
  EXPECT_EQ(static_cast<uint32_t>(encoder_bitrate_limits_270p.min_bitrate_bps),
            bitrate_allocator_factory_.codec_config().minBitrate * 1000);
  EXPECT_EQ(static_cast<uint32_t>(encoder_bitrate_limits_270p.max_bitrate_bps),
            bitrate_allocator_factory_.codec_config().maxBitrate * 1000);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, EncoderRecommendedMaxBitrateCapsTargetBitrate) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  VideoEncoderConfig video_encoder_config;
  test::FillEncoderConfiguration(kVideoCodecVP8, 1, &video_encoder_config);
  video_encoder_config.max_bitrate_bps = 0;
  video_stream_encoder_->ConfigureEncoder(video_encoder_config.Copy(),
                                          kMaxPayloadLength);

  // Encode 720p frame to get the default encoder target bitrate.
  video_source_.IncomingCapturedFrame(CreateFrame(1, 1280, 720));
  WaitForEncodedFrame(1);
  const uint32_t kDefaultTargetBitrateFor720pKbps =
      bitrate_allocator_factory_.codec_config()
          .simulcastStream[0]
          .targetBitrate;

  // Set the max recommended encoder bitrate to something lower than the default
  // target bitrate.
  const VideoEncoder::ResolutionBitrateLimits encoder_bitrate_limits(
      1280 * 720, 10 * 1000, 10 * 1000,
      kDefaultTargetBitrateFor720pKbps / 2 * 1000);
  fake_encoder_.SetResolutionBitrateLimits({encoder_bitrate_limits});

  // Change resolution to trigger encoder reinitialization.
  video_source_.IncomingCapturedFrame(CreateFrame(2, 640, 360));
  WaitForEncodedFrame(2);
  video_source_.IncomingCapturedFrame(CreateFrame(3, 1280, 720));
  WaitForEncodedFrame(3);

  // Ensure the target bitrate is capped by the max bitrate.
  EXPECT_EQ(bitrate_allocator_factory_.codec_config().maxBitrate * 1000,
            static_cast<uint32_t>(encoder_bitrate_limits.max_bitrate_bps));
  EXPECT_EQ(bitrate_allocator_factory_.codec_config()
                    .simulcastStream[0]
                    .targetBitrate *
                1000,
            static_cast<uint32_t>(encoder_bitrate_limits.max_bitrate_bps));

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, SwitchSourceDeregisterEncoderAsSink) {
  EXPECT_TRUE(video_source_.has_sinks());
  test::FrameForwarder new_video_source;
  video_stream_encoder_->SetSource(
      &new_video_source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);
  EXPECT_FALSE(video_source_.has_sinks());
  EXPECT_TRUE(new_video_source.has_sinks());

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, SinkWantsRotationApplied) {
  EXPECT_FALSE(video_source_.sink_wants().rotation_applied);
  video_stream_encoder_->SetSink(&sink_, true /*rotation_applied*/);
  EXPECT_TRUE(video_source_.sink_wants().rotation_applied);
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, TestCpuDowngrades_BalancedMode) {
  const int kFramerateFps = 30;
  const int kWidth = 1280;
  const int kHeight = 720;

  // We rely on the automatic resolution adaptation, but we handle framerate
  // adaptation manually by mocking the stats proxy.
  video_source_.set_adaptation_enabled(true);

  // Enable BALANCED preference, no initial limitation.
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  video_stream_encoder_->SetSource(&video_source_,
                                   webrtc::DegradationPreference::BALANCED);
  VerifyNoLimitation(video_source_.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Adapt down as far as possible.
  rtc::VideoSinkWants last_wants;
  int64_t t = 1;
  int loop_count = 0;
  do {
    ++loop_count;
    last_wants = video_source_.sink_wants();

    // Simulate the framerate we've been asked to adapt to.
    const int fps = std::min(kFramerateFps, last_wants.max_framerate_fps);
    const int frame_interval_ms = rtc::kNumMillisecsPerSec / fps;
    VideoSendStream::Stats mock_stats = stats_proxy_->GetStats();
    mock_stats.input_frame_rate = fps;
    stats_proxy_->SetMockStats(mock_stats);

    video_source_.IncomingCapturedFrame(CreateFrame(t, kWidth, kHeight));
    sink_.WaitForEncodedFrame(t);
    t += frame_interval_ms;

    video_stream_encoder_->TriggerCpuOveruse();
    VerifyBalancedModeFpsRange(
        video_source_.sink_wants(),
        *video_source_.last_sent_width() * *video_source_.last_sent_height());
  } while (video_source_.sink_wants().max_pixel_count <
               last_wants.max_pixel_count ||
           video_source_.sink_wants().max_framerate_fps <
               last_wants.max_framerate_fps);

  // Verify that we've adapted all the way down.
  stats_proxy_->ResetMockStats();
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(loop_count - 1,
            stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(kMinPixelsPerFrame, *video_source_.last_sent_width() *
                                    *video_source_.last_sent_height());
  EXPECT_EQ(kMinBalancedFramerateFps,
            video_source_.sink_wants().max_framerate_fps);

  // Adapt back up the same number of times we adapted down.
  for (int i = 0; i < loop_count - 1; ++i) {
    last_wants = video_source_.sink_wants();

    // Simulate the framerate we've been asked to adapt to.
    const int fps = std::min(kFramerateFps, last_wants.max_framerate_fps);
    const int frame_interval_ms = rtc::kNumMillisecsPerSec / fps;
    VideoSendStream::Stats mock_stats = stats_proxy_->GetStats();
    mock_stats.input_frame_rate = fps;
    stats_proxy_->SetMockStats(mock_stats);

    video_source_.IncomingCapturedFrame(CreateFrame(t, kWidth, kHeight));
    sink_.WaitForEncodedFrame(t);
    t += frame_interval_ms;

    video_stream_encoder_->TriggerCpuNormalUsage();
    VerifyBalancedModeFpsRange(
        video_source_.sink_wants(),
        *video_source_.last_sent_width() * *video_source_.last_sent_height());
    EXPECT_TRUE(video_source_.sink_wants().max_pixel_count >
                    last_wants.max_pixel_count ||
                video_source_.sink_wants().max_framerate_fps >
                    last_wants.max_framerate_fps);
  }

  VerifyFpsMaxResolutionMax(video_source_.sink_wants());
  stats_proxy_->ResetMockStats();
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ((loop_count - 1) * 2,
            stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  video_stream_encoder_->Stop();
}
TEST_F(VideoStreamEncoderTest, SinkWantsStoredByDegradationPreference) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  VerifyNoLimitation(video_source_.sink_wants());

  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;

  int64_t frame_timestamp = 1;

  video_source_.IncomingCapturedFrame(
      CreateFrame(frame_timestamp, kFrameWidth, kFrameHeight));
  WaitForEncodedFrame(frame_timestamp);
  frame_timestamp += kFrameIntervalMs;

  // Trigger CPU overuse.
  video_stream_encoder_->TriggerCpuOveruse();
  video_source_.IncomingCapturedFrame(
      CreateFrame(frame_timestamp, kFrameWidth, kFrameHeight));
  WaitForEncodedFrame(frame_timestamp);
  frame_timestamp += kFrameIntervalMs;

  // Default degradation preference is maintain-framerate, so will lower max
  // wanted resolution.
  EXPECT_FALSE(video_source_.sink_wants().target_pixel_count);
  EXPECT_LT(video_source_.sink_wants().max_pixel_count,
            kFrameWidth * kFrameHeight);
  EXPECT_EQ(kDefaultFramerate, video_source_.sink_wants().max_framerate_fps);

  // Set new source, switch to maintain-resolution.
  test::FrameForwarder new_video_source;
  video_stream_encoder_->SetSource(
      &new_video_source, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);

  // Initially no degradation registered.
  VerifyFpsMaxResolutionMax(new_video_source.sink_wants());

  // Force an input frame rate to be available, or the adaptation call won't
  // know what framerate to adapt form.
  const int kInputFps = 30;
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  stats.input_frame_rate = kInputFps;
  stats_proxy_->SetMockStats(stats);

  video_stream_encoder_->TriggerCpuOveruse();
  new_video_source.IncomingCapturedFrame(
      CreateFrame(frame_timestamp, kFrameWidth, kFrameHeight));
  WaitForEncodedFrame(frame_timestamp);
  frame_timestamp += kFrameIntervalMs;

  // Some framerate constraint should be set.
  EXPECT_FALSE(new_video_source.sink_wants().target_pixel_count);
  EXPECT_EQ(std::numeric_limits<int>::max(),
            new_video_source.sink_wants().max_pixel_count);
  EXPECT_LT(new_video_source.sink_wants().max_framerate_fps, kInputFps);

  // Turn off degradation completely.
  video_stream_encoder_->SetSource(&new_video_source,
                                   webrtc::DegradationPreference::DISABLED);
  VerifyFpsMaxResolutionMax(new_video_source.sink_wants());

  video_stream_encoder_->TriggerCpuOveruse();
  new_video_source.IncomingCapturedFrame(
      CreateFrame(frame_timestamp, kFrameWidth, kFrameHeight));
  WaitForEncodedFrame(frame_timestamp);
  frame_timestamp += kFrameIntervalMs;

  // Still no degradation.
  VerifyFpsMaxResolutionMax(new_video_source.sink_wants());

  // Calling SetSource with resolution scaling enabled apply the old SinkWants.
  video_stream_encoder_->SetSource(
      &new_video_source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);
  EXPECT_LT(new_video_source.sink_wants().max_pixel_count,
            kFrameWidth * kFrameHeight);
  EXPECT_FALSE(new_video_source.sink_wants().target_pixel_count);
  EXPECT_EQ(kDefaultFramerate, new_video_source.sink_wants().max_framerate_fps);

  // Calling SetSource with framerate scaling enabled apply the old SinkWants.
  video_stream_encoder_->SetSource(
      &new_video_source, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);
  EXPECT_FALSE(new_video_source.sink_wants().target_pixel_count);
  EXPECT_EQ(std::numeric_limits<int>::max(),
            new_video_source.sink_wants().max_pixel_count);
  EXPECT_LT(new_video_source.sink_wants().max_framerate_fps, kInputFps);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, StatsTracksQualityAdaptationStats) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  const int kWidth = 1280;
  const int kHeight = 720;
  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  WaitForEncodedFrame(1);
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_EQ(0, stats.number_of_quality_adapt_changes);

  // Trigger adapt down.
  video_stream_encoder_->TriggerQualityLow();
  video_source_.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  WaitForEncodedFrame(2);

  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.bw_limited_resolution);
  EXPECT_EQ(1, stats.number_of_quality_adapt_changes);

  // Trigger adapt up.
  video_stream_encoder_->TriggerQualityHigh();
  video_source_.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  WaitForEncodedFrame(3);

  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_EQ(2, stats.number_of_quality_adapt_changes);
  EXPECT_EQ(0, stats.number_of_cpu_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, StatsTracksCpuAdaptationStats) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  const int kWidth = 1280;
  const int kHeight = 720;
  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  WaitForEncodedFrame(1);
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_EQ(0, stats.number_of_cpu_adapt_changes);

  // Trigger CPU overuse.
  video_stream_encoder_->TriggerCpuOveruse();
  video_source_.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  WaitForEncodedFrame(2);

  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Trigger CPU normal use.
  video_stream_encoder_->TriggerCpuNormalUsage();
  video_source_.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  WaitForEncodedFrame(3);

  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_EQ(2, stats.number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats.number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, SwitchingSourceKeepsCpuAdaptation) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  const int kWidth = 1280;
  const int kHeight = 720;
  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  WaitForEncodedFrame(1);
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_EQ(0, stats.number_of_cpu_adapt_changes);

  // Trigger CPU overuse.
  video_stream_encoder_->TriggerCpuOveruse();
  video_source_.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  WaitForEncodedFrame(2);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Set new source with adaptation still enabled.
  test::FrameForwarder new_video_source;
  video_stream_encoder_->SetSource(
      &new_video_source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);

  new_video_source.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  WaitForEncodedFrame(3);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Set adaptation disabled.
  video_stream_encoder_->SetSource(&new_video_source,
                                   webrtc::DegradationPreference::DISABLED);

  new_video_source.IncomingCapturedFrame(CreateFrame(4, kWidth, kHeight));
  WaitForEncodedFrame(4);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Set adaptation back to enabled.
  video_stream_encoder_->SetSource(
      &new_video_source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);

  new_video_source.IncomingCapturedFrame(CreateFrame(5, kWidth, kHeight));
  WaitForEncodedFrame(5);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Trigger CPU normal use.
  video_stream_encoder_->TriggerCpuNormalUsage();
  new_video_source.IncomingCapturedFrame(CreateFrame(6, kWidth, kHeight));
  WaitForEncodedFrame(6);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_EQ(2, stats.number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats.number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, SwitchingSourceKeepsQualityAdaptation) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  const int kWidth = 1280;
  const int kHeight = 720;
  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  WaitForEncodedFrame(1);
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_FALSE(stats.bw_limited_framerate);
  EXPECT_EQ(0, stats.number_of_quality_adapt_changes);

  // Set new source with adaptation still enabled.
  test::FrameForwarder new_video_source;
  video_stream_encoder_->SetSource(&new_video_source,
                                   webrtc::DegradationPreference::BALANCED);

  new_video_source.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  WaitForEncodedFrame(2);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_FALSE(stats.bw_limited_framerate);
  EXPECT_EQ(0, stats.number_of_quality_adapt_changes);

  // Trigger adapt down.
  video_stream_encoder_->TriggerQualityLow();
  new_video_source.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  WaitForEncodedFrame(3);
  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.bw_limited_resolution);
  EXPECT_FALSE(stats.bw_limited_framerate);
  EXPECT_EQ(1, stats.number_of_quality_adapt_changes);

  // Set new source with adaptation still enabled.
  video_stream_encoder_->SetSource(&new_video_source,
                                   webrtc::DegradationPreference::BALANCED);

  new_video_source.IncomingCapturedFrame(CreateFrame(4, kWidth, kHeight));
  WaitForEncodedFrame(4);
  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.bw_limited_resolution);
  EXPECT_FALSE(stats.bw_limited_framerate);
  EXPECT_EQ(1, stats.number_of_quality_adapt_changes);

  // Disable resolution scaling.
  video_stream_encoder_->SetSource(
      &new_video_source, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);

  new_video_source.IncomingCapturedFrame(CreateFrame(5, kWidth, kHeight));
  WaitForEncodedFrame(5);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.bw_limited_resolution);
  EXPECT_FALSE(stats.bw_limited_framerate);
  EXPECT_EQ(1, stats.number_of_quality_adapt_changes);
  EXPECT_EQ(0, stats.number_of_cpu_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       QualityAdaptationStatsAreResetWhenScalerIsDisabled) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  const int kWidth = 1280;
  const int kHeight = 720;
  int64_t timestamp_ms = kFrameIntervalMs;
  video_source_.set_adaptation_enabled(true);
  video_source_.IncomingCapturedFrame(
      CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt down.
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  video_source_.IncomingCapturedFrame(
      CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger overuse.
  video_stream_encoder_->TriggerCpuOveruse();
  timestamp_ms += kFrameIntervalMs;
  video_source_.IncomingCapturedFrame(
      CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Leave source unchanged, but disable quality scaler.
  fake_encoder_.SetQualityScaling(false);

  VideoEncoderConfig video_encoder_config;
  test::FillEncoderConfiguration(kVideoCodecVP8, 1, &video_encoder_config);
  // Make format different, to force recreation of encoder.
  video_encoder_config.video_format.parameters["foo"] = "foo";
  video_stream_encoder_->ConfigureEncoder(std::move(video_encoder_config),
                                          kMaxPayloadLength);
  timestamp_ms += kFrameIntervalMs;
  video_source_.IncomingCapturedFrame(
      CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       StatsTracksCpuAdaptationStatsWhenSwitchingSource) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  const int kWidth = 1280;
  const int kHeight = 720;
  int sequence = 1;

  video_source_.IncomingCapturedFrame(CreateFrame(sequence, kWidth, kHeight));
  WaitForEncodedFrame(sequence++);
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_framerate);
  EXPECT_EQ(0, stats.number_of_cpu_adapt_changes);

  // Trigger CPU overuse, should now adapt down.
  video_stream_encoder_->TriggerCpuOveruse();
  video_source_.IncomingCapturedFrame(CreateFrame(sequence, kWidth, kHeight));
  WaitForEncodedFrame(sequence++);
  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_framerate);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Set new source with adaptation still enabled.
  test::FrameForwarder new_video_source;
  video_stream_encoder_->SetSource(
      &new_video_source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);

  new_video_source.IncomingCapturedFrame(
      CreateFrame(sequence, kWidth, kHeight));
  WaitForEncodedFrame(sequence++);
  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_framerate);
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Set cpu adaptation by frame dropping.
  video_stream_encoder_->SetSource(
      &new_video_source, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);
  new_video_source.IncomingCapturedFrame(
      CreateFrame(sequence, kWidth, kHeight));
  WaitForEncodedFrame(sequence++);
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
  video_stream_encoder_->TriggerCpuOveruse();
  stats_proxy_->ResetMockStats();

  new_video_source.IncomingCapturedFrame(
      CreateFrame(sequence, kWidth, kHeight));
  WaitForEncodedFrame(sequence++);

  // Framerate now adapted.
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_TRUE(stats.cpu_limited_framerate);
  EXPECT_EQ(2, stats.number_of_cpu_adapt_changes);

  // Disable CPU adaptation.
  video_stream_encoder_->SetSource(&new_video_source,
                                   webrtc::DegradationPreference::DISABLED);
  new_video_source.IncomingCapturedFrame(
      CreateFrame(sequence, kWidth, kHeight));
  WaitForEncodedFrame(sequence++);

  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_framerate);
  EXPECT_EQ(2, stats.number_of_cpu_adapt_changes);

  // Try to trigger overuse. Should not succeed.
  stats_proxy_->SetMockStats(mock_stats);
  video_stream_encoder_->TriggerCpuOveruse();
  stats_proxy_->ResetMockStats();

  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_framerate);
  EXPECT_EQ(2, stats.number_of_cpu_adapt_changes);

  // Switch back the source with resolution adaptation enabled.
  video_stream_encoder_->SetSource(
      &video_source_, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);
  video_source_.IncomingCapturedFrame(CreateFrame(sequence, kWidth, kHeight));
  WaitForEncodedFrame(sequence++);
  stats = stats_proxy_->GetStats();
  EXPECT_TRUE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_framerate);
  EXPECT_EQ(2, stats.number_of_cpu_adapt_changes);

  // Trigger CPU normal usage.
  video_stream_encoder_->TriggerCpuNormalUsage();
  video_source_.IncomingCapturedFrame(CreateFrame(sequence, kWidth, kHeight));
  WaitForEncodedFrame(sequence++);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_framerate);
  EXPECT_EQ(3, stats.number_of_cpu_adapt_changes);

  // Back to the source with adaptation off, set it back to maintain-resolution.
  video_stream_encoder_->SetSource(
      &new_video_source, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);
  new_video_source.IncomingCapturedFrame(
      CreateFrame(sequence, kWidth, kHeight));
  WaitForEncodedFrame(sequence++);
  stats = stats_proxy_->GetStats();
  // Disabled, since we previously switched the source to disabled.
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_TRUE(stats.cpu_limited_framerate);
  EXPECT_EQ(3, stats.number_of_cpu_adapt_changes);

  // Trigger CPU normal usage.
  video_stream_encoder_->TriggerCpuNormalUsage();
  new_video_source.IncomingCapturedFrame(
      CreateFrame(sequence, kWidth, kHeight));
  WaitForEncodedFrame(sequence++);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_framerate);
  EXPECT_EQ(4, stats.number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats.number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       ScalingUpAndDownDoesNothingWithMaintainResolution) {
  const int kWidth = 1280;
  const int kHeight = 720;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Expect no scaling to begin with.
  VerifyNoLimitation(video_source_.sink_wants());

  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  WaitForEncodedFrame(1);

  // Trigger scale down.
  video_stream_encoder_->TriggerQualityLow();

  video_source_.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  WaitForEncodedFrame(2);

  // Expect a scale down.
  EXPECT_TRUE(video_source_.sink_wants().max_pixel_count);
  EXPECT_LT(video_source_.sink_wants().max_pixel_count, kWidth * kHeight);

  // Set resolution scaling disabled.
  test::FrameForwarder new_video_source;
  video_stream_encoder_->SetSource(
      &new_video_source, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);

  // Trigger scale down.
  video_stream_encoder_->TriggerQualityLow();
  new_video_source.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  WaitForEncodedFrame(3);

  // Expect no scaling.
  EXPECT_EQ(std::numeric_limits<int>::max(),
            new_video_source.sink_wants().max_pixel_count);

  // Trigger scale up.
  video_stream_encoder_->TriggerQualityHigh();
  new_video_source.IncomingCapturedFrame(CreateFrame(4, kWidth, kHeight));
  WaitForEncodedFrame(4);

  // Expect nothing to change, still no scaling.
  EXPECT_EQ(std::numeric_limits<int>::max(),
            new_video_source.sink_wants().max_pixel_count);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       SkipsSameAdaptDownRequest_MaintainFramerateMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Enable MAINTAIN_FRAMERATE preference, no initial limitation.
  test::FrameForwarder source;
  video_stream_encoder_->SetSource(
      &source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  WaitForEncodedFrame(1);
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt down, expect scaled down resolution.
  video_stream_encoder_->TriggerCpuOveruse();
  VerifyFpsMaxResolutionLt(source.sink_wants(), kWidth * kHeight);
  const int kLastMaxPixelCount = source.sink_wants().max_pixel_count;
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt down for same input resolution, expect no change.
  video_stream_encoder_->TriggerCpuOveruse();
  EXPECT_EQ(kLastMaxPixelCount, source.sink_wants().max_pixel_count);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, SkipsSameOrLargerAdaptDownRequest_BalancedMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Enable BALANCED preference, no initial limitation.
  test::FrameForwarder source;
  video_stream_encoder_->SetSource(&source,
                                   webrtc::DegradationPreference::BALANCED);
  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(1);
  VerifyFpsMaxResolutionMax(source.sink_wants());

  // Trigger adapt down, expect scaled down resolution.
  video_stream_encoder_->TriggerQualityLow();
  VerifyFpsMaxResolutionLt(source.sink_wants(), kWidth * kHeight);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);
  const int kLastMaxPixelCount = source.sink_wants().max_pixel_count;

  // Trigger adapt down for same input resolution, expect no change.
  source.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  sink_.WaitForEncodedFrame(2);
  video_stream_encoder_->TriggerQualityLow();
  EXPECT_EQ(kLastMaxPixelCount, source.sink_wants().max_pixel_count);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down for larger input resolution, expect no change.
  source.IncomingCapturedFrame(CreateFrame(3, kWidth + 1, kHeight + 1));
  sink_.WaitForEncodedFrame(3);
  video_stream_encoder_->TriggerQualityLow();
  EXPECT_EQ(kLastMaxPixelCount, source.sink_wants().max_pixel_count);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       NoChangeForInitialNormalUsage_MaintainFramerateMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Enable MAINTAIN_FRAMERATE preference, no initial limitation.
  test::FrameForwarder source;
  video_stream_encoder_->SetSource(
      &source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt up, expect no change.
  video_stream_encoder_->TriggerCpuNormalUsage();
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       NoChangeForInitialNormalUsage_MaintainResolutionMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Enable MAINTAIN_RESOLUTION preference, no initial limitation.
  test::FrameForwarder source;
  video_stream_encoder_->SetSource(
      &source, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt up, expect no change.
  video_stream_encoder_->TriggerCpuNormalUsage();
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, NoChangeForInitialNormalUsage_BalancedMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Enable BALANCED preference, no initial limitation.
  test::FrameForwarder source;
  video_stream_encoder_->SetSource(&source,
                                   webrtc::DegradationPreference::BALANCED);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no change.
  video_stream_encoder_->TriggerQualityHigh();
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, NoChangeForInitialNormalUsage_DisabledMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Enable DISABLED preference, no initial limitation.
  test::FrameForwarder source;
  video_stream_encoder_->SetSource(&source,
                                   webrtc::DegradationPreference::DISABLED);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no change.
  video_stream_encoder_->TriggerQualityHigh();
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       AdaptsResolutionForLowQuality_MaintainFramerateMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Enable MAINTAIN_FRAMERATE preference, no initial limitation.
  AdaptingFrameForwarder source;
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(
      &source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  WaitForEncodedFrame(1);
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution.
  video_stream_encoder_->TriggerQualityLow();
  source.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  WaitForEncodedFrame(2);
  VerifyFpsMaxResolutionLt(source.sink_wants(), kWidth * kHeight);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no restriction.
  video_stream_encoder_->TriggerQualityHigh();
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       AdaptsFramerateForLowQuality_MaintainResolutionMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  const int kInputFps = 30;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  stats.input_frame_rate = kInputFps;
  stats_proxy_->SetMockStats(stats);

  // Expect no scaling to begin with (preference: MAINTAIN_FRAMERATE).
  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(1);
  VerifyFpsMaxResolutionMax(video_source_.sink_wants());

  // Trigger adapt down, expect scaled down resolution.
  video_stream_encoder_->TriggerQualityLow();
  video_source_.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  sink_.WaitForEncodedFrame(2);
  VerifyFpsMaxResolutionLt(video_source_.sink_wants(), kWidth * kHeight);

  // Enable MAINTAIN_RESOLUTION preference.
  test::FrameForwarder new_video_source;
  video_stream_encoder_->SetSource(
      &new_video_source, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);
  VerifyFpsMaxResolutionMax(new_video_source.sink_wants());

  // Trigger adapt down, expect reduced framerate.
  video_stream_encoder_->TriggerQualityLow();
  new_video_source.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  sink_.WaitForEncodedFrame(3);
  VerifyFpsLtResolutionMax(new_video_source.sink_wants(), kInputFps);

  // Trigger adapt up, expect no restriction.
  video_stream_encoder_->TriggerQualityHigh();
  VerifyFpsMaxResolutionMax(new_video_source.sink_wants());

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, DoesNotScaleBelowSetResolutionLimit) {
  const int kWidth = 1280;
  const int kHeight = 720;
  const size_t kNumFrames = 10;

  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Enable adapter, expected input resolutions when downscaling:
  // 1280x720 -> 960x540 -> 640x360 -> 480x270 -> 320x180 (kMinPixelsPerFrame)
  video_source_.set_adaptation_enabled(true);

  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  int downscales = 0;
  for (size_t i = 1; i <= kNumFrames; i++) {
    video_source_.IncomingCapturedFrame(
        CreateFrame(i * kFrameIntervalMs, kWidth, kHeight));
    WaitForEncodedFrame(i * kFrameIntervalMs);

    // Trigger scale down.
    rtc::VideoSinkWants last_wants = video_source_.sink_wants();
    video_stream_encoder_->TriggerQualityLow();
    EXPECT_GE(video_source_.sink_wants().max_pixel_count, kMinPixelsPerFrame);

    if (video_source_.sink_wants().max_pixel_count < last_wants.max_pixel_count)
      ++downscales;

    EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
    EXPECT_EQ(downscales,
              stats_proxy_->GetStats().number_of_quality_adapt_changes);
    EXPECT_GT(downscales, 0);
  }
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       AdaptsResolutionUpAndDownTwiceOnOveruse_MaintainFramerateMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Enable MAINTAIN_FRAMERATE preference, no initial limitation.
  AdaptingFrameForwarder source;
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(
      &source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);

  int64_t timestamp_ms = kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt down, expect scaled down resolution.
  video_stream_encoder_->TriggerCpuOveruse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionLt(source.sink_wants(), kWidth * kHeight);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt up, expect no restriction.
  video_stream_encoder_->TriggerCpuNormalUsage();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt down, expect scaled down resolution.
  video_stream_encoder_->TriggerCpuOveruse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionLt(source.sink_wants(), kWidth * kHeight);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt up, expect no restriction.
  video_stream_encoder_->TriggerCpuNormalUsage();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       AdaptsResolutionUpAndDownTwiceForLowQuality_BalancedMode_NoFpsLimit) {
  const int kWidth = 1280;
  const int kHeight = 720;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Enable BALANCED preference, no initial limitation.
  AdaptingFrameForwarder source;
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(&source,
                                   webrtc::DegradationPreference::BALANCED);

  int64_t timestamp_ms = kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution.
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionLt(source.sink_wants(), kWidth * kHeight);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no restriction.
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution.
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionLt(source.sink_wants(), kWidth * kHeight);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no restriction.
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, NoAdaptUpIfBwEstimateIsLessThanMinBitrate) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:57600|129600|230400,fps:7|10|14,kbps:0|0|425/");
  // Reset encoder for field trials to take effect.
  ConfigureEncoder(video_encoder_config_.Copy());

  const int kWidth = 640;  // pixels:640x360=230400
  const int kHeight = 360;
  const int64_t kFrameIntervalMs = 150;
  const int kMinBitrateBps = 425000;
  const int kTooLowMinBitrateBps = 424000;
  video_stream_encoder_->OnBitrateUpdated(DataRate::bps(kTooLowMinBitrateBps),
                                          DataRate::bps(kTooLowMinBitrateBps),
                                          0, 0);

  // Enable BALANCED preference, no initial limitation.
  AdaptingFrameForwarder source;
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(&source,
                                   webrtc::DegradationPreference::BALANCED);

  int64_t timestamp_ms = kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down framerate (640x360@14fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionMax(source.sink_wants(), 14);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution (480x270@14fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionLt(source.sink_wants(), source.last_wants());
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down framerate (480x270@10fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsLtResolutionEq(source.sink_wants(), source.last_wants());
  EXPECT_EQ(source.sink_wants().max_framerate_fps, 10);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no upscale in fps (target bitrate < min bitrate).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect upscaled fps (target bitrate == min bitrate).
  video_stream_encoder_->OnBitrateUpdated(DataRate::bps(kMinBitrateBps),
                                          DataRate::bps(kMinBitrateBps), 0, 0);
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  EXPECT_EQ(source.sink_wants().max_framerate_fps, 14);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       NoAdaptUpInResolutionIfBwEstimateIsLessThanMinBitrate) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:57600|129600|230400,fps:7|10|14,kbps_res:0|0|435/");
  // Reset encoder for field trials to take effect.
  ConfigureEncoder(video_encoder_config_.Copy());

  const int kWidth = 640;  // pixels:640x360=230400
  const int kHeight = 360;
  const int64_t kFrameIntervalMs = 150;
  const int kResolutionMinBitrateBps = 435000;
  const int kTooLowMinResolutionBitrateBps = 434000;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTooLowMinResolutionBitrateBps),
      DataRate::bps(kTooLowMinResolutionBitrateBps), 0, 0);

  // Enable BALANCED preference, no initial limitation.
  AdaptingFrameForwarder source;
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(&source,
                                   webrtc::DegradationPreference::BALANCED);

  int64_t timestamp_ms = kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down framerate (640x360@14fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionMax(source.sink_wants(), 14);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution (480x270@14fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionLt(source.sink_wants(), source.last_wants());
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down framerate (480x270@10fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsLtResolutionEq(source.sink_wants(), source.last_wants());
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect upscaled fps (no bitrate limit) (480x270@14fps).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsGtResolutionEq(source.sink_wants(), source.last_wants());
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no upscale in res (target bitrate < min bitrate).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect upscaled res (target bitrate == min bitrate).
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kResolutionMinBitrateBps),
      DataRate::bps(kResolutionMinBitrateBps), 0, 0);
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionGt(source.sink_wants(), source.last_wants());
  EXPECT_EQ(5, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       NoAdaptUpInFpsAndResolutionIfBwEstimateIsLessThanMinBitrate) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:57600|129600|230400,fps:7|10|14,kbps:0|0|425,kbps_res:0|0|435/");
  // Reset encoder for field trials to take effect.
  ConfigureEncoder(video_encoder_config_.Copy());

  const int kWidth = 640;  // pixels:640x360=230400
  const int kHeight = 360;
  const int64_t kFrameIntervalMs = 150;
  const int kMinBitrateBps = 425000;
  const int kTooLowMinBitrateBps = 424000;
  const int kResolutionMinBitrateBps = 435000;
  const int kTooLowMinResolutionBitrateBps = 434000;
  video_stream_encoder_->OnBitrateUpdated(DataRate::bps(kTooLowMinBitrateBps),
                                          DataRate::bps(kTooLowMinBitrateBps),
                                          0, 0);

  // Enable BALANCED preference, no initial limitation.
  AdaptingFrameForwarder source;
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(&source,
                                   webrtc::DegradationPreference::BALANCED);

  int64_t timestamp_ms = kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down framerate (640x360@14fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionMax(source.sink_wants(), 14);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution (480x270@14fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionLt(source.sink_wants(), source.last_wants());
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down framerate (480x270@10fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsLtResolutionEq(source.sink_wants(), source.last_wants());
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no upscale (target bitrate < min bitrate).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect upscaled fps (target bitrate == min bitrate).
  video_stream_encoder_->OnBitrateUpdated(DataRate::bps(kMinBitrateBps),
                                          DataRate::bps(kMinBitrateBps), 0, 0);
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsGtResolutionEq(source.sink_wants(), source.last_wants());
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no upscale in res (target bitrate < min bitrate).
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTooLowMinResolutionBitrateBps),
      DataRate::bps(kTooLowMinResolutionBitrateBps), 0, 0);
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect upscaled res (target bitrate == min bitrate).
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kResolutionMinBitrateBps),
      DataRate::bps(kResolutionMinBitrateBps), 0, 0);
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionGt(source.sink_wants(), source.last_wants());
  EXPECT_EQ(5, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       AdaptsResolutionOnOveruseAndLowQuality_MaintainFramerateMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Enable MAINTAIN_FRAMERATE preference, no initial limitation.
  AdaptingFrameForwarder source;
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(
      &source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);

  int64_t timestamp_ms = kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt down, expect scaled down resolution (960x540).
  video_stream_encoder_->TriggerCpuOveruse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionLt(source.sink_wants(), kWidth * kHeight);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt down, expect scaled down resolution (640x360).
  video_stream_encoder_->TriggerCpuOveruse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionLt(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt down, expect scaled down resolution (480x270).
  video_stream_encoder_->TriggerCpuOveruse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionLt(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality adapt down, expect scaled down resolution (320x180).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionLt(source.sink_wants(), source.last_wants());
  rtc::VideoSinkWants last_wants = source.sink_wants();
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality adapt down, expect no change (min resolution reached).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionEq(source.sink_wants(), last_wants);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt up, expect upscaled resolution (480x270).
  video_stream_encoder_->TriggerCpuNormalUsage();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionGt(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt up, expect upscaled resolution (640x360).
  video_stream_encoder_->TriggerCpuNormalUsage();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionGt(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(5, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt up, expect upscaled resolution (960x540).
  video_stream_encoder_->TriggerCpuNormalUsage();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionGt(source.sink_wants(), source.last_wants());
  last_wants = source.sink_wants();
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(6, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt up, no cpu downgrades, expect no change (960x540).
  video_stream_encoder_->TriggerCpuNormalUsage();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionEq(source.sink_wants(), last_wants);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(6, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality adapt up, expect no restriction (1280x720).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionGt(source.sink_wants(), source.last_wants());
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(6, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, CpuLimitedHistogramIsReported) {
  const int kWidth = 640;
  const int kHeight = 360;

  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  for (int i = 1; i <= SendStatisticsProxy::kMinRequiredMetricsSamples; ++i) {
    video_source_.IncomingCapturedFrame(CreateFrame(i, kWidth, kHeight));
    WaitForEncodedFrame(i);
  }

  video_stream_encoder_->TriggerCpuOveruse();
  for (int i = 1; i <= SendStatisticsProxy::kMinRequiredMetricsSamples; ++i) {
    video_source_.IncomingCapturedFrame(CreateFrame(
        SendStatisticsProxy::kMinRequiredMetricsSamples + i, kWidth, kHeight));
    WaitForEncodedFrame(SendStatisticsProxy::kMinRequiredMetricsSamples + i);
  }

  video_stream_encoder_->Stop();
  video_stream_encoder_.reset();
  stats_proxy_.reset();

  EXPECT_EQ(1,
            metrics::NumSamples("WebRTC.Video.CpuLimitedResolutionInPercent"));
  EXPECT_EQ(
      1, metrics::NumEvents("WebRTC.Video.CpuLimitedResolutionInPercent", 50));
}

TEST_F(VideoStreamEncoderTest,
       CpuLimitedHistogramIsNotReportedForDisabledDegradation) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  const int kWidth = 640;
  const int kHeight = 360;

  video_stream_encoder_->SetSource(&video_source_,
                                   webrtc::DegradationPreference::DISABLED);

  for (int i = 1; i <= SendStatisticsProxy::kMinRequiredMetricsSamples; ++i) {
    video_source_.IncomingCapturedFrame(CreateFrame(i, kWidth, kHeight));
    WaitForEncodedFrame(i);
  }

  video_stream_encoder_->Stop();
  video_stream_encoder_.reset();
  stats_proxy_.reset();

  EXPECT_EQ(0,
            metrics::NumSamples("WebRTC.Video.CpuLimitedResolutionInPercent"));
}

TEST_F(VideoStreamEncoderTest, CallsBitrateObserver) {
  MockBitrateObserver bitrate_observer;
  video_stream_encoder_->SetBitrateAllocationObserver(&bitrate_observer);

  const int kDefaultFps = 30;
  const VideoBitrateAllocation expected_bitrate =
      DefaultVideoBitrateAllocator(fake_encoder_.codec_config())
          .Allocate(VideoBitrateAllocationParameters(kLowTargetBitrateBps,
                                                     kDefaultFps));

  EXPECT_CALL(bitrate_observer, OnBitrateAllocationUpdated(expected_bitrate))
      .Times(1);
  video_stream_encoder_->OnBitrateUpdated(DataRate::bps(kLowTargetBitrateBps),
                                          DataRate::bps(kLowTargetBitrateBps),
                                          0, 0);

  video_source_.IncomingCapturedFrame(
      CreateFrame(rtc::TimeMillis(), codec_width_, codec_height_));
  WaitForEncodedFrame(rtc::TimeMillis());
  VideoBitrateAllocation bitrate_allocation =
      fake_encoder_.GetAndResetLastRateControlSettings()->bitrate;
  // Check that encoder has been updated too, not just allocation observer.
  EXPECT_EQ(bitrate_allocation.get_sum_bps(), kLowTargetBitrateBps);
  // TODO(srte): The use of millisecs here looks like an error, but the tests
  // fails using seconds, this should be investigated.
  fake_clock_.AdvanceTime(TimeDelta::ms(1) / kDefaultFps);

  // Not called on second frame.
  EXPECT_CALL(bitrate_observer, OnBitrateAllocationUpdated(expected_bitrate))
      .Times(0);
  video_source_.IncomingCapturedFrame(
      CreateFrame(rtc::TimeMillis(), codec_width_, codec_height_));
  WaitForEncodedFrame(rtc::TimeMillis());
  fake_clock_.AdvanceTime(TimeDelta::ms(1) / kDefaultFps);

  // Called after a process interval.
  const int64_t kProcessIntervalMs =
      vcm::VCMProcessTimer::kDefaultProcessIntervalMs;
  EXPECT_CALL(bitrate_observer, OnBitrateAllocationUpdated(expected_bitrate))
      .Times(1);
  const int64_t start_time_ms = rtc::TimeMillis();
  while (rtc::TimeMillis() - start_time_ms < kProcessIntervalMs) {
    video_source_.IncomingCapturedFrame(
        CreateFrame(rtc::TimeMillis(), codec_width_, codec_height_));
    WaitForEncodedFrame(rtc::TimeMillis());
    fake_clock_.AdvanceTime(TimeDelta::ms(1) / kDefaultFps);
  }

  // Since rates are unchanged, encoder should not be reconfigured.
  EXPECT_FALSE(fake_encoder_.GetAndResetLastRateControlSettings().has_value());

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, TemporalLayersNotDisabledIfSupported) {
  // 2 TLs configured, temporal layers supported by encoder.
  const int kNumTemporalLayers = 2;
  ResetEncoder("VP8", 1, kNumTemporalLayers, 1, /*screenshare*/ false);
  fake_encoder_.SetTemporalLayersSupported(0, true);

  // Bitrate allocated across temporal layers.
  const int kTl0Bps = kTargetBitrateBps *
                      webrtc::SimulcastRateAllocator::GetTemporalRateAllocation(
                          kNumTemporalLayers, /*temporal_id*/ 0);
  const int kTl1Bps = kTargetBitrateBps *
                      webrtc::SimulcastRateAllocator::GetTemporalRateAllocation(
                          kNumTemporalLayers, /*temporal_id*/ 1);
  VideoBitrateAllocation expected_bitrate;
  expected_bitrate.SetBitrate(/*si*/ 0, /*ti*/ 0, kTl0Bps);
  expected_bitrate.SetBitrate(/*si*/ 0, /*ti*/ 1, kTl1Bps - kTl0Bps);

  VerifyAllocatedBitrate(expected_bitrate);
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, TemporalLayersDisabledIfNotSupported) {
  // 2 TLs configured, temporal layers not supported by encoder.
  ResetEncoder("VP8", 1, /*num_temporal_layers*/ 2, 1, /*screenshare*/ false);
  fake_encoder_.SetTemporalLayersSupported(0, false);

  // Temporal layers not supported by the encoder.
  // Total bitrate should be at ti:0.
  VideoBitrateAllocation expected_bitrate;
  expected_bitrate.SetBitrate(/*si*/ 0, /*ti*/ 0, kTargetBitrateBps);

  VerifyAllocatedBitrate(expected_bitrate);
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, VerifyBitrateAllocationForTwoStreams) {
  // 2 TLs configured, temporal layers only supported for first stream.
  ResetEncoder("VP8", 2, /*num_temporal_layers*/ 2, 1, /*screenshare*/ false);
  fake_encoder_.SetTemporalLayersSupported(0, true);
  fake_encoder_.SetTemporalLayersSupported(1, false);

  const int kS0Bps = 150000;
  const int kS0Tl0Bps =
      kS0Bps * webrtc::SimulcastRateAllocator::GetTemporalRateAllocation(
                   /*num_layers*/ 2, /*temporal_id*/ 0);
  const int kS0Tl1Bps =
      kS0Bps * webrtc::SimulcastRateAllocator::GetTemporalRateAllocation(
                   /*num_layers*/ 2, /*temporal_id*/ 1);
  const int kS1Bps = kTargetBitrateBps - kS0Tl1Bps;
  // Temporal layers not supported by si:1.
  VideoBitrateAllocation expected_bitrate;
  expected_bitrate.SetBitrate(/*si*/ 0, /*ti*/ 0, kS0Tl0Bps);
  expected_bitrate.SetBitrate(/*si*/ 0, /*ti*/ 1, kS0Tl1Bps - kS0Tl0Bps);
  expected_bitrate.SetBitrate(/*si*/ 1, /*ti*/ 0, kS1Bps);

  VerifyAllocatedBitrate(expected_bitrate);
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, OveruseDetectorUpdatedOnReconfigureAndAdaption) {
  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;
  const int kFramerate = 24;

  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  test::FrameForwarder source;
  video_stream_encoder_->SetSource(
      &source, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);

  // Insert a single frame, triggering initial configuration.
  source.IncomingCapturedFrame(CreateFrame(1, kFrameWidth, kFrameHeight));
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();

  EXPECT_EQ(
      video_stream_encoder_->overuse_detector_proxy_->GetLastTargetFramerate(),
      kDefaultFramerate);

  // Trigger reconfigure encoder (without resetting the entire instance).
  VideoEncoderConfig video_encoder_config;
  video_encoder_config.codec_type = kVideoCodecVP8;
  video_encoder_config.max_bitrate_bps = kTargetBitrateBps;
  video_encoder_config.number_of_streams = 1;
  video_encoder_config.video_stream_factory =
      new rtc::RefCountedObject<VideoStreamFactory>(1, kFramerate);
  video_stream_encoder_->ConfigureEncoder(std::move(video_encoder_config),
                                          kMaxPayloadLength);
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();

  // Detector should be updated with fps limit from codec config.
  EXPECT_EQ(
      video_stream_encoder_->overuse_detector_proxy_->GetLastTargetFramerate(),
      kFramerate);

  // Trigger overuse, max framerate should be reduced.
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  stats.input_frame_rate = kFramerate;
  stats_proxy_->SetMockStats(stats);
  video_stream_encoder_->TriggerCpuOveruse();
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();
  int adapted_framerate =
      video_stream_encoder_->overuse_detector_proxy_->GetLastTargetFramerate();
  EXPECT_LT(adapted_framerate, kFramerate);

  // Trigger underuse, max framerate should go back to codec configured fps.
  // Set extra low fps, to make sure it's actually reset, not just incremented.
  stats = stats_proxy_->GetStats();
  stats.input_frame_rate = adapted_framerate / 2;
  stats_proxy_->SetMockStats(stats);
  video_stream_encoder_->TriggerCpuNormalUsage();
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();
  EXPECT_EQ(
      video_stream_encoder_->overuse_detector_proxy_->GetLastTargetFramerate(),
      kFramerate);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       OveruseDetectorUpdatedRespectsFramerateAfterUnderuse) {
  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;
  const int kLowFramerate = 15;
  const int kHighFramerate = 25;

  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  test::FrameForwarder source;
  video_stream_encoder_->SetSource(
      &source, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);

  // Trigger initial configuration.
  VideoEncoderConfig video_encoder_config;
  video_encoder_config.codec_type = kVideoCodecVP8;
  video_encoder_config.max_bitrate_bps = kTargetBitrateBps;
  video_encoder_config.number_of_streams = 1;
  video_encoder_config.video_stream_factory =
      new rtc::RefCountedObject<VideoStreamFactory>(1, kLowFramerate);
  source.IncomingCapturedFrame(CreateFrame(1, kFrameWidth, kFrameHeight));
  video_stream_encoder_->ConfigureEncoder(std::move(video_encoder_config),
                                          kMaxPayloadLength);
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();

  EXPECT_EQ(
      video_stream_encoder_->overuse_detector_proxy_->GetLastTargetFramerate(),
      kLowFramerate);

  // Trigger overuse, max framerate should be reduced.
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  stats.input_frame_rate = kLowFramerate;
  stats_proxy_->SetMockStats(stats);
  video_stream_encoder_->TriggerCpuOveruse();
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();
  int adapted_framerate =
      video_stream_encoder_->overuse_detector_proxy_->GetLastTargetFramerate();
  EXPECT_LT(adapted_framerate, kLowFramerate);

  // Reconfigure the encoder with a new (higher max framerate), max fps should
  // still respect the adaptation.
  video_encoder_config.video_stream_factory =
      new rtc::RefCountedObject<VideoStreamFactory>(1, kHighFramerate);
  source.IncomingCapturedFrame(CreateFrame(1, kFrameWidth, kFrameHeight));
  video_stream_encoder_->ConfigureEncoder(std::move(video_encoder_config),
                                          kMaxPayloadLength);
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();

  EXPECT_EQ(
      video_stream_encoder_->overuse_detector_proxy_->GetLastTargetFramerate(),
      adapted_framerate);

  // Trigger underuse, max framerate should go back to codec configured fps.
  stats = stats_proxy_->GetStats();
  stats.input_frame_rate = adapted_framerate;
  stats_proxy_->SetMockStats(stats);
  video_stream_encoder_->TriggerCpuNormalUsage();
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();
  EXPECT_EQ(
      video_stream_encoder_->overuse_detector_proxy_->GetLastTargetFramerate(),
      kHighFramerate);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       OveruseDetectorUpdatedOnDegradationPreferenceChange) {
  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;
  const int kFramerate = 24;

  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  test::FrameForwarder source;
  video_stream_encoder_->SetSource(
      &source, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);

  // Trigger initial configuration.
  VideoEncoderConfig video_encoder_config;
  video_encoder_config.codec_type = kVideoCodecVP8;
  video_encoder_config.max_bitrate_bps = kTargetBitrateBps;
  video_encoder_config.number_of_streams = 1;
  video_encoder_config.video_stream_factory =
      new rtc::RefCountedObject<VideoStreamFactory>(1, kFramerate);
  source.IncomingCapturedFrame(CreateFrame(1, kFrameWidth, kFrameHeight));
  video_stream_encoder_->ConfigureEncoder(std::move(video_encoder_config),
                                          kMaxPayloadLength);
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();

  EXPECT_EQ(
      video_stream_encoder_->overuse_detector_proxy_->GetLastTargetFramerate(),
      kFramerate);

  // Trigger overuse, max framerate should be reduced.
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  stats.input_frame_rate = kFramerate;
  stats_proxy_->SetMockStats(stats);
  video_stream_encoder_->TriggerCpuOveruse();
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();
  int adapted_framerate =
      video_stream_encoder_->overuse_detector_proxy_->GetLastTargetFramerate();
  EXPECT_LT(adapted_framerate, kFramerate);

  // Change degradation preference to not enable framerate scaling. Target
  // framerate should be changed to codec defined limit.
  video_stream_encoder_->SetSource(
      &source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();
  EXPECT_EQ(
      video_stream_encoder_->overuse_detector_proxy_->GetLastTargetFramerate(),
      kFramerate);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, DropsFramesAndScalesWhenBitrateIsTooLow) {
  const int kTooLowBitrateForFrameSizeBps = 10000;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTooLowBitrateForFrameSizeBps),
      DataRate::bps(kTooLowBitrateForFrameSizeBps), 0, 0);
  const int kWidth = 640;
  const int kHeight = 360;

  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));

  // Expect to drop this frame, the wait should time out.
  ExpectDroppedFrame();

  // Expect the sink_wants to specify a scaled frame.
  EXPECT_LT(video_source_.sink_wants().max_pixel_count, kWidth * kHeight);

  int last_pixel_count = video_source_.sink_wants().max_pixel_count;

  // Next frame is scaled.
  video_source_.IncomingCapturedFrame(
      CreateFrame(2, kWidth * 3 / 4, kHeight * 3 / 4));

  // Expect to drop this frame, the wait should time out.
  ExpectDroppedFrame();

  EXPECT_LT(video_source_.sink_wants().max_pixel_count, last_pixel_count);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       NumberOfDroppedFramesLimitedWhenBitrateIsTooLow) {
  const int kTooLowBitrateForFrameSizeBps = 10000;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTooLowBitrateForFrameSizeBps),
      DataRate::bps(kTooLowBitrateForFrameSizeBps), 0, 0);
  const int kWidth = 640;
  const int kHeight = 360;

  // We expect the n initial frames to get dropped.
  int i;
  for (i = 1; i <= kMaxInitialFramedrop; ++i) {
    video_source_.IncomingCapturedFrame(CreateFrame(i, kWidth, kHeight));
    ExpectDroppedFrame();
  }
  // The n+1th frame should not be dropped, even though it's size is too large.
  video_source_.IncomingCapturedFrame(CreateFrame(i, kWidth, kHeight));
  WaitForEncodedFrame(i);

  // Expect the sink_wants to specify a scaled frame.
  EXPECT_LT(video_source_.sink_wants().max_pixel_count, kWidth * kHeight);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       InitialFrameDropOffWithMaintainResolutionPreference) {
  const int kWidth = 640;
  const int kHeight = 360;
  video_stream_encoder_->OnBitrateUpdated(DataRate::bps(kLowTargetBitrateBps),
                                          DataRate::bps(kLowTargetBitrateBps),
                                          0, 0);

  // Set degradation preference.
  video_stream_encoder_->SetSource(
      &video_source_, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);

  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  // Frame should not be dropped, even if it's too large.
  WaitForEncodedFrame(1);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, InitialFrameDropOffWhenEncoderDisabledScaling) {
  const int kWidth = 640;
  const int kHeight = 360;
  fake_encoder_.SetQualityScaling(false);

  VideoEncoderConfig video_encoder_config;
  test::FillEncoderConfiguration(kVideoCodecVP8, 1, &video_encoder_config);
  // Make format different, to force recreation of encoder.
  video_encoder_config.video_format.parameters["foo"] = "foo";
  video_stream_encoder_->ConfigureEncoder(std::move(video_encoder_config),
                                          kMaxPayloadLength);
  video_stream_encoder_->OnBitrateUpdated(DataRate::bps(kLowTargetBitrateBps),
                                          DataRate::bps(kLowTargetBitrateBps),
                                          0, 0);

  // Force quality scaler reconfiguration by resetting the source.
  video_stream_encoder_->SetSource(&video_source_,
                                   webrtc::DegradationPreference::BALANCED);

  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  // Frame should not be dropped, even if it's too large.
  WaitForEncodedFrame(1);

  video_stream_encoder_->Stop();
  fake_encoder_.SetQualityScaling(true);
}

TEST_F(VideoStreamEncoderTest, InitialFrameDropActivatesWhenBWEstimateReady) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-InitialFramedrop/Enabled/");
  // Reset encoder for field trials to take effect.
  ConfigureEncoder(video_encoder_config_.Copy());
  const int kTooLowBitrateForFrameSizeBps = 10000;
  const int kWidth = 640;
  const int kHeight = 360;

  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  // Frame should not be dropped.
  WaitForEncodedFrame(1);

  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTooLowBitrateForFrameSizeBps),
      DataRate::bps(kTooLowBitrateForFrameSizeBps), 0, 0);
  video_source_.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  // Expect to drop this frame, the wait should time out.
  ExpectDroppedFrame();

  // Expect the sink_wants to specify a scaled frame.
  EXPECT_LT(video_source_.sink_wants().max_pixel_count, kWidth * kHeight);
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, InitialFrameDropActivatesWhenBweDrops) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScalerSettings/"
      "initial_bitrate_interval_ms:1000,initial_bitrate_factor:0.2/");
  // Reset encoder for field trials to take effect.
  ConfigureEncoder(video_encoder_config_.Copy());
  const int kNotTooLowBitrateForFrameSizeBps = kTargetBitrateBps * 0.2;
  const int kTooLowBitrateForFrameSizeBps = kTargetBitrateBps * 0.19;
  const int kWidth = 640;
  const int kHeight = 360;

  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  // Frame should not be dropped.
  WaitForEncodedFrame(1);

  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kNotTooLowBitrateForFrameSizeBps),
      DataRate::bps(kNotTooLowBitrateForFrameSizeBps), 0, 0);
  video_source_.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  // Frame should not be dropped.
  WaitForEncodedFrame(2);

  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTooLowBitrateForFrameSizeBps),
      DataRate::bps(kTooLowBitrateForFrameSizeBps), 0, 0);
  video_source_.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  // Expect to drop this frame, the wait should time out.
  ExpectDroppedFrame();

  // Expect the sink_wants to specify a scaled frame.
  EXPECT_LT(video_source_.sink_wants().max_pixel_count, kWidth * kHeight);
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       ResolutionNotAdaptedForTooSmallFrame_MaintainFramerateMode) {
  const int kTooSmallWidth = 10;
  const int kTooSmallHeight = 10;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Enable MAINTAIN_FRAMERATE preference, no initial limitation.
  test::FrameForwarder source;
  video_stream_encoder_->SetSource(
      &source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);

  // Trigger adapt down, too small frame, expect no change.
  source.IncomingCapturedFrame(CreateFrame(1, kTooSmallWidth, kTooSmallHeight));
  WaitForEncodedFrame(1);
  video_stream_encoder_->TriggerCpuOveruse();
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       ResolutionNotAdaptedForTooSmallFrame_BalancedMode) {
  const int kTooSmallWidth = 10;
  const int kTooSmallHeight = 10;
  const int kFpsLimit = 7;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Enable BALANCED preference, no initial limitation.
  test::FrameForwarder source;
  video_stream_encoder_->SetSource(&source,
                                   webrtc::DegradationPreference::BALANCED);
  VerifyNoLimitation(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);

  // Trigger adapt down, expect limited framerate.
  source.IncomingCapturedFrame(CreateFrame(1, kTooSmallWidth, kTooSmallHeight));
  WaitForEncodedFrame(1);
  video_stream_encoder_->TriggerQualityLow();
  VerifyFpsEqResolutionMax(source.sink_wants(), kFpsLimit);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, too small frame, expect no change.
  source.IncomingCapturedFrame(CreateFrame(2, kTooSmallWidth, kTooSmallHeight));
  WaitForEncodedFrame(2);
  video_stream_encoder_->TriggerQualityLow();
  VerifyFpsEqResolutionMax(source.sink_wants(), kFpsLimit);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, FailingInitEncodeDoesntCauseCrash) {
  fake_encoder_.ForceInitEncodeFailure(true);
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  ResetEncoder("VP8", 2, 1, 1, false);
  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;
  video_source_.IncomingCapturedFrame(
      CreateFrame(1, kFrameWidth, kFrameHeight));
  ExpectDroppedFrame();
  video_stream_encoder_->Stop();
}

// TODO(sprang): Extend this with fps throttling and any "balanced" extensions.
TEST_F(VideoStreamEncoderTest,
       AdaptsResolutionOnOveruse_MaintainFramerateMode) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;
  // Enabled default VideoAdapter downscaling. First step is 3/4, not 3/5 as
  // requested by
  // VideoStreamEncoder::VideoSourceProxy::RequestResolutionLowerThan().
  video_source_.set_adaptation_enabled(true);

  video_source_.IncomingCapturedFrame(
      CreateFrame(1 * kFrameIntervalMs, kFrameWidth, kFrameHeight));
  WaitForEncodedFrame(kFrameWidth, kFrameHeight);

  // Trigger CPU overuse, downscale by 3/4.
  video_stream_encoder_->TriggerCpuOveruse();
  video_source_.IncomingCapturedFrame(
      CreateFrame(2 * kFrameIntervalMs, kFrameWidth, kFrameHeight));
  WaitForEncodedFrame((kFrameWidth * 3) / 4, (kFrameHeight * 3) / 4);

  // Trigger CPU normal use, return to original resolution.
  video_stream_encoder_->TriggerCpuNormalUsage();
  video_source_.IncomingCapturedFrame(
      CreateFrame(3 * kFrameIntervalMs, kFrameWidth, kFrameHeight));
  WaitForEncodedFrame(kFrameWidth, kFrameHeight);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       AdaptsFramerateOnOveruse_MaintainResolutionMode) {
  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;

  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  video_stream_encoder_->SetSource(
      &video_source_, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);
  video_source_.set_adaptation_enabled(true);

  int64_t timestamp_ms = fake_clock_.TimeNanos() / rtc::kNumNanosecsPerMillisec;

  video_source_.IncomingCapturedFrame(
      CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
  WaitForEncodedFrame(timestamp_ms);

  // Try to trigger overuse. No fps estimate available => no effect.
  video_stream_encoder_->TriggerCpuOveruse();

  // Insert frames for one second to get a stable estimate.
  for (int i = 0; i < max_framerate_; ++i) {
    timestamp_ms += kFrameIntervalMs;
    video_source_.IncomingCapturedFrame(
        CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
    WaitForEncodedFrame(timestamp_ms);
  }

  // Trigger CPU overuse, reduce framerate by 2/3.
  video_stream_encoder_->TriggerCpuOveruse();
  int num_frames_dropped = 0;
  for (int i = 0; i < max_framerate_; ++i) {
    timestamp_ms += kFrameIntervalMs;
    video_source_.IncomingCapturedFrame(
        CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
    if (!WaitForFrame(kFrameTimeoutMs)) {
      ++num_frames_dropped;
    } else {
      sink_.CheckLastFrameSizeMatches(kFrameWidth, kFrameHeight);
    }
  }

  // Add some slack to account for frames dropped by the frame dropper.
  const int kErrorMargin = 1;
  EXPECT_NEAR(num_frames_dropped, max_framerate_ - (max_framerate_ * 2 / 3),
              kErrorMargin);

  // Trigger CPU overuse, reduce framerate by 2/3 again.
  video_stream_encoder_->TriggerCpuOveruse();
  num_frames_dropped = 0;
  for (int i = 0; i <= max_framerate_; ++i) {
    timestamp_ms += kFrameIntervalMs;
    video_source_.IncomingCapturedFrame(
        CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
    if (!WaitForFrame(kFrameTimeoutMs)) {
      ++num_frames_dropped;
    } else {
      sink_.CheckLastFrameSizeMatches(kFrameWidth, kFrameHeight);
    }
  }
  EXPECT_NEAR(num_frames_dropped, max_framerate_ - (max_framerate_ * 4 / 9),
              kErrorMargin);

  // Go back up one step.
  video_stream_encoder_->TriggerCpuNormalUsage();
  num_frames_dropped = 0;
  for (int i = 0; i < max_framerate_; ++i) {
    timestamp_ms += kFrameIntervalMs;
    video_source_.IncomingCapturedFrame(
        CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
    if (!WaitForFrame(kFrameTimeoutMs)) {
      ++num_frames_dropped;
    } else {
      sink_.CheckLastFrameSizeMatches(kFrameWidth, kFrameHeight);
    }
  }
  EXPECT_NEAR(num_frames_dropped, max_framerate_ - (max_framerate_ * 2 / 3),
              kErrorMargin);

  // Go back up to original mode.
  video_stream_encoder_->TriggerCpuNormalUsage();
  num_frames_dropped = 0;
  for (int i = 0; i < max_framerate_; ++i) {
    timestamp_ms += kFrameIntervalMs;
    video_source_.IncomingCapturedFrame(
        CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
    if (!WaitForFrame(kFrameTimeoutMs)) {
      ++num_frames_dropped;
    } else {
      sink_.CheckLastFrameSizeMatches(kFrameWidth, kFrameHeight);
    }
  }
  EXPECT_NEAR(num_frames_dropped, 0, kErrorMargin);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, DoesntAdaptDownPastMinFramerate) {
  const int kFramerateFps = 5;
  const int kFrameIntervalMs = rtc::kNumMillisecsPerSec / kFramerateFps;
  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;

  // Reconfigure encoder with two temporal layers and screensharing, which will
  // disable frame dropping and make testing easier.
  ResetEncoder("VP8", 1, 2, 1, true);

  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  video_stream_encoder_->SetSource(
      &video_source_, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);
  video_source_.set_adaptation_enabled(true);

  int64_t timestamp_ms = fake_clock_.TimeNanos() / rtc::kNumNanosecsPerMillisec;

  // Trigger overuse as much as we can.
  rtc::VideoSinkWants last_wants;
  do {
    last_wants = video_source_.sink_wants();

    // Insert frames to get a new fps estimate...
    for (int j = 0; j < kFramerateFps; ++j) {
      video_source_.IncomingCapturedFrame(
          CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
      if (video_source_.last_sent_width()) {
        sink_.WaitForEncodedFrame(timestamp_ms);
      }
      timestamp_ms += kFrameIntervalMs;
      fake_clock_.AdvanceTime(TimeDelta::ms(kFrameIntervalMs));
    }
    // ...and then try to adapt again.
    video_stream_encoder_->TriggerCpuOveruse();
  } while (video_source_.sink_wants().max_framerate_fps <
           last_wants.max_framerate_fps);

  VerifyFpsEqResolutionMax(video_source_.sink_wants(), kMinFramerateFps);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       AdaptsResolutionAndFramerateForLowQuality_BalancedMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  const int64_t kFrameIntervalMs = 150;
  int64_t timestamp_ms = kFrameIntervalMs;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Enable BALANCED preference, no initial limitation.
  AdaptingFrameForwarder source;
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(&source,
                                   webrtc::DegradationPreference::BALANCED);
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution (960x540@30fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionLt(source.sink_wants(), kWidth * kHeight);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution (640x360@30fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionLt(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect reduced fps (640x360@15fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsLtResolutionEq(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution (480x270@15fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionLt(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Restrict bitrate, trigger adapt down, expect reduced fps (480x270@10fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsLtResolutionEq(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(5, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution (320x180@10fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionLt(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(6, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect reduced fps (320x180@7fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsLtResolutionEq(source.sink_wants(), source.last_wants());
  rtc::VideoSinkWants last_wants = source.sink_wants();
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(7, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, min resolution reached, expect no change.
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionEq(source.sink_wants(), last_wants);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(7, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect expect increased fps (320x180@10fps).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsGtResolutionEq(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(8, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect upscaled resolution (480x270@10fps).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionGt(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(9, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Increase bitrate, trigger adapt up, expect increased fps (480x270@15fps).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsGtResolutionEq(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(10, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect upscaled resolution (640x360@15fps).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionGt(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(11, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect increased fps (640x360@30fps).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionEq(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(12, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect upscaled resolution (960x540@30fps).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionGt(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(13, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no restriction (1280x720fps@30fps).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionGt(source.sink_wants(), source.last_wants());
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(14, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no change.
  video_stream_encoder_->TriggerQualityHigh();
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_EQ(14, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, AdaptWithTwoReasonsAndDifferentOrder_Framerate) {
  const int kWidth = 1280;
  const int kHeight = 720;
  const int64_t kFrameIntervalMs = 150;
  int64_t timestamp_ms = kFrameIntervalMs;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Enable BALANCED preference, no initial limitation.
  AdaptingFrameForwarder source;
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(&source,
                                   webrtc::DegradationPreference::BALANCED);
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt down, expect scaled down resolution (960x540@30fps).
  video_stream_encoder_->TriggerCpuOveruse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionLt(source.sink_wants(), kWidth * kHeight);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt down, expect scaled down resolution (640x360@30fps).
  video_stream_encoder_->TriggerCpuOveruse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionLt(source.sink_wants(), source.last_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality adapt down, expect reduced fps (640x360@15fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsLtResolutionEq(source.sink_wants(), source.last_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt up, expect increased fps (640x360@30fps).
  video_stream_encoder_->TriggerCpuNormalUsage();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionEq(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality adapt up, expect upscaled resolution (960x540@30fps).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionGt(source.sink_wants(), source.last_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt up,  expect no restriction (1280x720fps@30fps).
  video_stream_encoder_->TriggerCpuNormalUsage();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionGt(source.sink_wants(), source.last_wants());
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no change.
  video_stream_encoder_->TriggerQualityHigh();
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       AdaptWithTwoReasonsAndDifferentOrder_Resolution) {
  const int kWidth = 640;
  const int kHeight = 360;
  const int kFpsLimit = 15;
  const int64_t kFrameIntervalMs = 150;
  int64_t timestamp_ms = kFrameIntervalMs;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Enable BALANCED preference, no initial limitation.
  AdaptingFrameForwarder source;
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(&source,
                                   webrtc::DegradationPreference::BALANCED);
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt down, expect scaled down framerate (640x360@15fps).
  video_stream_encoder_->TriggerCpuOveruse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionMax(source.sink_wants(), kFpsLimit);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality adapt down, expect scaled down resolution (480x270@15fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionLt(source.sink_wants(), source.last_wants());
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt up, expect upscaled resolution (640x360@15fps).
  video_stream_encoder_->TriggerCpuNormalUsage();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsEqResolutionGt(source.sink_wants(), source.last_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality adapt up, expect increased fps (640x360@30fps).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no change.
  video_stream_encoder_->TriggerQualityHigh();
  VerifyFpsMaxResolutionMax(source.sink_wants());
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, AcceptsFullHdAdaptedDownSimulcastFrames) {
  const int kFrameWidth = 1920;
  const int kFrameHeight = 1080;
  // 3/4 of 1920.
  const int kAdaptedFrameWidth = 1440;
  // 3/4 of 1080 rounded down to multiple of 4.
  const int kAdaptedFrameHeight = 808;
  const int kFramerate = 24;

  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  // Trigger reconfigure encoder (without resetting the entire instance).
  VideoEncoderConfig video_encoder_config;
  video_encoder_config.codec_type = kVideoCodecVP8;
  video_encoder_config.max_bitrate_bps = kTargetBitrateBps;
  video_encoder_config.number_of_streams = 1;
  video_encoder_config.video_stream_factory =
      new rtc::RefCountedObject<CroppingVideoStreamFactory>(1, kFramerate);
  video_stream_encoder_->ConfigureEncoder(std::move(video_encoder_config),
                                          kMaxPayloadLength);
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();

  video_source_.set_adaptation_enabled(true);

  video_source_.IncomingCapturedFrame(
      CreateFrame(1, kFrameWidth, kFrameHeight));
  WaitForEncodedFrame(kFrameWidth, kFrameHeight);

  // Trigger CPU overuse, downscale by 3/4.
  video_stream_encoder_->TriggerCpuOveruse();
  video_source_.IncomingCapturedFrame(
      CreateFrame(2, kFrameWidth, kFrameHeight));
  WaitForEncodedFrame(kAdaptedFrameWidth, kAdaptedFrameHeight);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, PeriodicallyUpdatesChannelParameters) {
  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;
  const int kLowFps = 2;
  const int kHighFps = 30;

  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  int64_t timestamp_ms = fake_clock_.TimeNanos() / rtc::kNumNanosecsPerMillisec;
  max_framerate_ = kLowFps;

  // Insert 2 seconds of 2fps video.
  for (int i = 0; i < kLowFps * 2; ++i) {
    video_source_.IncomingCapturedFrame(
        CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
    WaitForEncodedFrame(timestamp_ms);
    timestamp_ms += 1000 / kLowFps;
  }

  // Make sure encoder is updated with new target.
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  video_source_.IncomingCapturedFrame(
      CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
  WaitForEncodedFrame(timestamp_ms);
  timestamp_ms += 1000 / kLowFps;

  EXPECT_EQ(kLowFps, fake_encoder_.GetConfiguredInputFramerate());

  // Insert 30fps frames for just a little more than the forced update period.
  const int kVcmTimerIntervalFrames =
      (vcm::VCMProcessTimer::kDefaultProcessIntervalMs * kHighFps) / 1000;
  const int kFrameIntervalMs = 1000 / kHighFps;
  max_framerate_ = kHighFps;
  for (int i = 0; i < kVcmTimerIntervalFrames + 2; ++i) {
    video_source_.IncomingCapturedFrame(
        CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
    // Wait for encoded frame, but skip ahead if it doesn't arrive as it might
    // be dropped if the encoder hans't been updated with the new higher target
    // framerate yet, causing it to overshoot the target bitrate and then
    // suffering the wrath of the media optimizer.
    TimedWaitForEncodedFrame(timestamp_ms, 2 * kFrameIntervalMs);
    timestamp_ms += kFrameIntervalMs;
  }

  // Don expect correct measurement just yet, but it should be higher than
  // before.
  EXPECT_GT(fake_encoder_.GetConfiguredInputFramerate(), kLowFps);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, DoesNotUpdateBitrateAllocationWhenSuspended) {
  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;
  const int kTargetBitrateBps = 1000000;

  MockBitrateObserver bitrate_observer;
  video_stream_encoder_->SetBitrateAllocationObserver(&bitrate_observer);
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();

  // Insert a first video frame, causes another bitrate update.
  int64_t timestamp_ms = fake_clock_.TimeNanos() / rtc::kNumNanosecsPerMillisec;
  EXPECT_CALL(bitrate_observer, OnBitrateAllocationUpdated(_)).Times(1);
  video_source_.IncomingCapturedFrame(
      CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
  WaitForEncodedFrame(timestamp_ms);

  // Next, simulate video suspension due to pacer queue overrun.
  video_stream_encoder_->OnBitrateUpdated(DataRate::bps(0), DataRate::bps(0), 0,
                                          1);

  // Skip ahead until a new periodic parameter update should have occured.
  timestamp_ms += vcm::VCMProcessTimer::kDefaultProcessIntervalMs;
  fake_clock_.AdvanceTime(
      TimeDelta::ms(vcm::VCMProcessTimer::kDefaultProcessIntervalMs));

  // Bitrate observer should not be called.
  EXPECT_CALL(bitrate_observer, OnBitrateAllocationUpdated(_)).Times(0);
  video_source_.IncomingCapturedFrame(
      CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
  ExpectDroppedFrame();

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       DefaultCpuAdaptationThresholdsForSoftwareEncoder) {
  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;
  const CpuOveruseOptions default_options;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  video_source_.IncomingCapturedFrame(
      CreateFrame(1, kFrameWidth, kFrameHeight));
  WaitForEncodedFrame(1);
  EXPECT_EQ(video_stream_encoder_->overuse_detector_proxy_->GetOptions()
                .low_encode_usage_threshold_percent,
            default_options.low_encode_usage_threshold_percent);
  EXPECT_EQ(video_stream_encoder_->overuse_detector_proxy_->GetOptions()
                .high_encode_usage_threshold_percent,
            default_options.high_encode_usage_threshold_percent);
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       HigherCpuAdaptationThresholdsForHardwareEncoder) {
  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;
  CpuOveruseOptions hardware_options;
  hardware_options.low_encode_usage_threshold_percent = 150;
  hardware_options.high_encode_usage_threshold_percent = 200;
  fake_encoder_.SetIsHardwareAccelerated(true);

  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  video_source_.IncomingCapturedFrame(
      CreateFrame(1, kFrameWidth, kFrameHeight));
  WaitForEncodedFrame(1);
  EXPECT_EQ(video_stream_encoder_->overuse_detector_proxy_->GetOptions()
                .low_encode_usage_threshold_percent,
            hardware_options.low_encode_usage_threshold_percent);
  EXPECT_EQ(video_stream_encoder_->overuse_detector_proxy_->GetOptions()
                .high_encode_usage_threshold_percent,
            hardware_options.high_encode_usage_threshold_percent);
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, DropsFramesWhenEncoderOvershoots) {
  const int kFrameWidth = 320;
  const int kFrameHeight = 240;
  const int kFps = 30;
  const int kTargetBitrateBps = 120000;
  const int kNumFramesInRun = kFps * 5;  // Runs of five seconds.

  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  int64_t timestamp_ms = fake_clock_.TimeNanos() / rtc::kNumNanosecsPerMillisec;
  max_framerate_ = kFps;

  // Insert 3 seconds of video, verify number of drops with normal bitrate.
  fake_encoder_.SimulateOvershoot(1.0);
  int num_dropped = 0;
  for (int i = 0; i < kNumFramesInRun; ++i) {
    video_source_.IncomingCapturedFrame(
        CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
    // Wait up to two frame durations for a frame to arrive.
    if (!TimedWaitForEncodedFrame(timestamp_ms, 2 * 1000 / kFps)) {
      ++num_dropped;
    }
    timestamp_ms += 1000 / kFps;
  }

  // Framerate should be measured to be near the expected target rate.
  EXPECT_NEAR(fake_encoder_.GetLastFramerate(), kFps, 1);

  // Frame drops should be within 5% of expected 0%.
  EXPECT_NEAR(num_dropped, 0, 5 * kNumFramesInRun / 100);

  // Make encoder produce frames at double the expected bitrate during 3 seconds
  // of video, verify number of drops. Rate needs to be slightly changed in
  // order to force the rate to be reconfigured.
  double overshoot_factor = 2.0;
  if (RateControlSettings::ParseFromFieldTrials().UseEncoderBitrateAdjuster()) {
    // With bitrate adjuster, when need to overshoot even more to trigger
    // frame dropping.
    overshoot_factor *= 2;
  }
  fake_encoder_.SimulateOvershoot(overshoot_factor);
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps + 1000),
      DataRate::bps(kTargetBitrateBps + 1000), 0, 0);
  num_dropped = 0;
  for (int i = 0; i < kNumFramesInRun; ++i) {
    video_source_.IncomingCapturedFrame(
        CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
    // Wait up to two frame durations for a frame to arrive.
    if (!TimedWaitForEncodedFrame(timestamp_ms, 2 * 1000 / kFps)) {
      ++num_dropped;
    }
    timestamp_ms += 1000 / kFps;
  }

  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Target framerate should be still be near the expected target, despite
  // the frame drops.
  EXPECT_NEAR(fake_encoder_.GetLastFramerate(), kFps, 1);

  // Frame drops should be within 5% of expected 50%.
  EXPECT_NEAR(num_dropped, kNumFramesInRun / 2, 5 * kNumFramesInRun / 100);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, ConfiguresCorrectFrameRate) {
  const int kFrameWidth = 320;
  const int kFrameHeight = 240;
  const int kActualInputFps = 24;
  const int kTargetBitrateBps = 120000;

  ASSERT_GT(max_framerate_, kActualInputFps);

  int64_t timestamp_ms = fake_clock_.TimeNanos() / rtc::kNumNanosecsPerMillisec;
  max_framerate_ = kActualInputFps;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Insert 3 seconds of video, with an input fps lower than configured max.
  for (int i = 0; i < kActualInputFps * 3; ++i) {
    video_source_.IncomingCapturedFrame(
        CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
    // Wait up to two frame durations for a frame to arrive.
    WaitForEncodedFrame(timestamp_ms);
    timestamp_ms += 1000 / kActualInputFps;
  }

  EXPECT_NEAR(kActualInputFps, fake_encoder_.GetLastFramerate(), 1);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, AccumulatesUpdateRectOnDroppedFrames) {
  VideoFrame::UpdateRect rect;
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  fake_encoder_.BlockNextEncode();
  video_source_.IncomingCapturedFrame(
      CreateFrameWithUpdatedPixel(1, nullptr, 0));
  WaitForEncodedFrame(1);
  // On the very first frame full update should be forced.
  rect = fake_encoder_.GetLastUpdateRect();
  EXPECT_EQ(rect.offset_x, 0);
  EXPECT_EQ(rect.offset_y, 0);
  EXPECT_EQ(rect.height, codec_height_);
  EXPECT_EQ(rect.width, codec_width_);
  // Here, the encoder thread will be blocked in the TestEncoder waiting for a
  // call to ContinueEncode.
  video_source_.IncomingCapturedFrame(
      CreateFrameWithUpdatedPixel(2, nullptr, 1));
  ExpectDroppedFrame();
  video_source_.IncomingCapturedFrame(
      CreateFrameWithUpdatedPixel(3, nullptr, 10));
  ExpectDroppedFrame();
  fake_encoder_.ContinueEncode();
  WaitForEncodedFrame(3);
  // Updates to pixels 1 and 10 should be accumulated to one 10x1 rect.
  rect = fake_encoder_.GetLastUpdateRect();
  EXPECT_EQ(rect.offset_x, 1);
  EXPECT_EQ(rect.offset_y, 0);
  EXPECT_EQ(rect.width, 10);
  EXPECT_EQ(rect.height, 1);

  video_source_.IncomingCapturedFrame(
      CreateFrameWithUpdatedPixel(4, nullptr, 0));
  WaitForEncodedFrame(4);
  // Previous frame was encoded, so no accumulation should happen.
  rect = fake_encoder_.GetLastUpdateRect();
  EXPECT_EQ(rect.offset_x, 0);
  EXPECT_EQ(rect.offset_y, 0);
  EXPECT_EQ(rect.width, 1);
  EXPECT_EQ(rect.height, 1);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, SetsFrameTypes) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // First frame is always keyframe.
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  WaitForEncodedFrame(1);
  EXPECT_THAT(
      fake_encoder_.LastFrameTypes(),
      ::testing::ElementsAre(VideoFrameType{VideoFrameType::kVideoFrameKey}));

  // Insert delta frame.
  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));
  WaitForEncodedFrame(2);
  EXPECT_THAT(
      fake_encoder_.LastFrameTypes(),
      ::testing::ElementsAre(VideoFrameType{VideoFrameType::kVideoFrameDelta}));

  // Request next frame be a key-frame.
  video_stream_encoder_->SendKeyFrame();
  video_source_.IncomingCapturedFrame(CreateFrame(3, nullptr));
  WaitForEncodedFrame(3);
  EXPECT_THAT(
      fake_encoder_.LastFrameTypes(),
      ::testing::ElementsAre(VideoFrameType{VideoFrameType::kVideoFrameKey}));

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, SetsFrameTypesSimulcast) {
  // Setup simulcast with three streams.
  ResetEncoder("VP8", 3, 1, 1, false);
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kSimulcastTargetBitrateBps),
      DataRate::bps(kSimulcastTargetBitrateBps), 0, 0);
  // Wait for all three layers before triggering event.
  sink_.SetNumExpectedLayers(3);

  // First frame is always keyframe.
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  WaitForEncodedFrame(1);
  EXPECT_THAT(fake_encoder_.LastFrameTypes(),
              ::testing::ElementsAreArray({VideoFrameType::kVideoFrameKey,
                                           VideoFrameType::kVideoFrameKey,
                                           VideoFrameType::kVideoFrameKey}));

  // Insert delta frame.
  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));
  WaitForEncodedFrame(2);
  EXPECT_THAT(fake_encoder_.LastFrameTypes(),
              ::testing::ElementsAreArray({VideoFrameType::kVideoFrameDelta,
                                           VideoFrameType::kVideoFrameDelta,
                                           VideoFrameType::kVideoFrameDelta}));

  // Request next frame be a key-frame.
  // Only first stream is configured to produce key-frame.
  video_stream_encoder_->SendKeyFrame();
  video_source_.IncomingCapturedFrame(CreateFrame(3, nullptr));
  WaitForEncodedFrame(3);

  // TODO(webrtc:10615): Map keyframe request to spatial layer. Currently
  // keyframe request on any layer triggers keyframe on all layers.
  EXPECT_THAT(fake_encoder_.LastFrameTypes(),
              ::testing::ElementsAreArray({VideoFrameType::kVideoFrameKey,
                                           VideoFrameType::kVideoFrameKey,
                                           VideoFrameType::kVideoFrameKey}));

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, RequestKeyframeInternalSource) {
  // Configure internal source factory and setup test again.
  encoder_factory_.SetHasInternalSource(true);
  ResetEncoder("VP8", 1, 1, 1, false);
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  // Call encoder directly, simulating internal source where encoded frame
  // callback in VideoStreamEncoder is called despite no OnFrame().
  fake_encoder_.InjectFrame(CreateFrame(1, nullptr), true);
  EXPECT_TRUE(WaitForFrame(kDefaultTimeoutMs));
  EXPECT_THAT(
      fake_encoder_.LastFrameTypes(),
      ::testing::ElementsAre(VideoFrameType{VideoFrameType::kVideoFrameKey}));

  const std::vector<VideoFrameType> kDeltaFrame = {
      VideoFrameType::kVideoFrameDelta};
  // Need to set timestamp manually since manually for injected frame.
  VideoFrame frame = CreateFrame(101, nullptr);
  frame.set_timestamp(101);
  fake_encoder_.InjectFrame(frame, false);
  EXPECT_TRUE(WaitForFrame(kDefaultTimeoutMs));
  EXPECT_THAT(
      fake_encoder_.LastFrameTypes(),
      ::testing::ElementsAre(VideoFrameType{VideoFrameType::kVideoFrameDelta}));

  // Request key-frame. The forces a dummy frame down into the encoder.
  fake_encoder_.ExpectNullFrame();
  video_stream_encoder_->SendKeyFrame();
  EXPECT_TRUE(WaitForFrame(kDefaultTimeoutMs));
  EXPECT_THAT(
      fake_encoder_.LastFrameTypes(),
      ::testing::ElementsAre(VideoFrameType{VideoFrameType::kVideoFrameKey}));

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, AdjustsTimestampInternalSource) {
  // Configure internal source factory and setup test again.
  encoder_factory_.SetHasInternalSource(true);
  ResetEncoder("VP8", 1, 1, 1, false);
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);

  int64_t timestamp = 1;
  EncodedImage image;
  image.SetEncodedData(
      EncodedImageBuffer::Create(kTargetBitrateBps / kDefaultFramerate / 8));
  image.capture_time_ms_ = ++timestamp;
  image.SetTimestamp(static_cast<uint32_t>(timestamp * 90));
  const int64_t kEncodeFinishDelayMs = 10;
  image.timing_.encode_start_ms = timestamp;
  image.timing_.encode_finish_ms = timestamp + kEncodeFinishDelayMs;
  fake_encoder_.InjectEncodedImage(image);
  // Wait for frame without incrementing clock.
  EXPECT_TRUE(sink_.WaitForFrame(kDefaultTimeoutMs));
  // Frame is captured kEncodeFinishDelayMs before it's encoded, so restored
  // capture timestamp should be kEncodeFinishDelayMs in the past.
  EXPECT_EQ(sink_.GetLastCaptureTimeMs(),
            fake_clock_.TimeNanos() / rtc::kNumNanosecsPerMillisec -
                kEncodeFinishDelayMs);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, DoesNotRewriteH264BitstreamWithOptimalSps) {
  // Configure internal source factory and setup test again.
  encoder_factory_.SetHasInternalSource(true);
  ResetEncoder("H264", 1, 1, 1, false);

  EncodedImage image(optimal_sps, sizeof(optimal_sps), sizeof(optimal_sps));
  image._frameType = VideoFrameType::kVideoFrameKey;

  CodecSpecificInfo codec_specific_info;
  codec_specific_info.codecType = kVideoCodecH264;

  RTPFragmentationHeader fragmentation;
  fragmentation.VerifyAndAllocateFragmentationHeader(1);
  fragmentation.fragmentationOffset[0] = 4;
  fragmentation.fragmentationLength[0] = sizeof(optimal_sps) - 4;

  fake_encoder_.InjectEncodedImage(image, &codec_specific_info, &fragmentation);
  EXPECT_TRUE(sink_.WaitForFrame(kDefaultTimeoutMs));

  EXPECT_THAT(sink_.GetLastEncodedImageData(),
              testing::ElementsAreArray(optimal_sps));
  RTPFragmentationHeader last_fragmentation = sink_.GetLastFragmentation();
  ASSERT_THAT(last_fragmentation.fragmentationVectorSize, 1U);
  EXPECT_EQ(last_fragmentation.fragmentationOffset[0], 4U);
  EXPECT_EQ(last_fragmentation.fragmentationLength[0], sizeof(optimal_sps) - 4);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, RewritesH264BitstreamWithNonOptimalSps) {
  uint8_t original_sps[] = {0,    0,    0,    1,    H264::NaluType::kSps,
                            0x00, 0x00, 0x03, 0x03, 0xF4,
                            0x05, 0x03, 0xC7, 0xC0};

  // Configure internal source factory and setup test again.
  encoder_factory_.SetHasInternalSource(true);
  ResetEncoder("H264", 1, 1, 1, false);

  EncodedImage image(original_sps, sizeof(original_sps), sizeof(original_sps));
  image._frameType = VideoFrameType::kVideoFrameKey;

  CodecSpecificInfo codec_specific_info;
  codec_specific_info.codecType = kVideoCodecH264;

  RTPFragmentationHeader fragmentation;
  fragmentation.VerifyAndAllocateFragmentationHeader(1);
  fragmentation.fragmentationOffset[0] = 4;
  fragmentation.fragmentationLength[0] = sizeof(original_sps) - 4;

  fake_encoder_.InjectEncodedImage(image, &codec_specific_info, &fragmentation);
  EXPECT_TRUE(sink_.WaitForFrame(kDefaultTimeoutMs));

  EXPECT_THAT(sink_.GetLastEncodedImageData(),
              testing::ElementsAreArray(optimal_sps));
  RTPFragmentationHeader last_fragmentation = sink_.GetLastFragmentation();
  ASSERT_THAT(last_fragmentation.fragmentationVectorSize, 1U);
  EXPECT_EQ(last_fragmentation.fragmentationOffset[0], 4U);
  EXPECT_EQ(last_fragmentation.fragmentationLength[0], sizeof(optimal_sps) - 4);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, CopiesVideoFrameMetadataAfterDownscale) {
  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;
  const int kTargetBitrateBps = 300000;  // To low for HD resolution.

  video_stream_encoder_->OnBitrateUpdated(
      DataRate::bps(kTargetBitrateBps), DataRate::bps(kTargetBitrateBps), 0, 0);
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();

  // Insert a first video frame. It should be dropped because of downscale in
  // resolution.
  int64_t timestamp_ms = fake_clock_.TimeNanos() / rtc::kNumNanosecsPerMillisec;
  VideoFrame frame = CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight);
  frame.set_rotation(kVideoRotation_270);
  video_source_.IncomingCapturedFrame(frame);

  ExpectDroppedFrame();

  // Second frame is downscaled.
  timestamp_ms = fake_clock_.TimeNanos() / rtc::kNumNanosecsPerMillisec;
  frame = CreateFrame(timestamp_ms, kFrameWidth / 2, kFrameHeight / 2);
  frame.set_rotation(kVideoRotation_90);
  video_source_.IncomingCapturedFrame(frame);

  WaitForEncodedFrame(timestamp_ms);
  sink_.CheckLastFrameRotationMatches(kVideoRotation_90);

  // Insert another frame, also downscaled.
  timestamp_ms = fake_clock_.TimeNanos() / rtc::kNumNanosecsPerMillisec;
  frame = CreateFrame(timestamp_ms, kFrameWidth / 2, kFrameHeight / 2);
  frame.set_rotation(kVideoRotation_180);
  video_source_.IncomingCapturedFrame(frame);

  WaitForEncodedFrame(timestamp_ms);
  sink_.CheckLastFrameRotationMatches(kVideoRotation_180);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, BandwidthAllocationLowerBound) {
  const int kFrameWidth = 320;
  const int kFrameHeight = 180;

  // Initial rate.
  video_stream_encoder_->OnBitrateUpdated(
      /*target_bitrate=*/DataRate::kbps(300),
      /*link_allocation=*/DataRate::kbps(300),
      /*fraction_lost=*/0,
      /*rtt_ms=*/0);

  // Insert a first video frame so that encoder gets configured.
  int64_t timestamp_ms = fake_clock_.TimeNanos() / rtc::kNumNanosecsPerMillisec;
  VideoFrame frame = CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight);
  frame.set_rotation(kVideoRotation_270);
  video_source_.IncomingCapturedFrame(frame);
  WaitForEncodedFrame(timestamp_ms);

  // Set a target rate below the minimum allowed by the codec settings.
  VideoCodec codec_config = fake_encoder_.codec_config();
  DataRate min_rate = DataRate::kbps(codec_config.minBitrate);
  DataRate target_rate = min_rate - DataRate::kbps(1);
  video_stream_encoder_->OnBitrateUpdated(
      /*target_bitrate=*/target_rate,
      /*link_allocation=*/target_rate,
      /*fraction_lost=*/0,
      /*rtt_ms=*/0);
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();

  // Target bitrate and bandwidth allocation should both be capped at min_rate.
  auto rate_settings = fake_encoder_.GetAndResetLastRateControlSettings();
  ASSERT_TRUE(rate_settings.has_value());
  DataRate allocation_sum = DataRate::bps(rate_settings->bitrate.get_sum_bps());
  EXPECT_EQ(min_rate, allocation_sum);
  EXPECT_EQ(rate_settings->bandwidth_allocation, min_rate);

  video_stream_encoder_->Stop();
}

}  // namespace webrtc
