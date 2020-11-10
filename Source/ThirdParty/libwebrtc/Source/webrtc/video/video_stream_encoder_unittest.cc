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
#include "api/test/mock_video_encoder.h"
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_adaptation_reason.h"
#include "api/video/video_bitrate_allocation.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/vp8_temporal_layers.h"
#include "api/video_codecs/vp8_temporal_layers_factory.h"
#include "call/adaptation/test/fake_adaptation_constraint.h"
#include "call/adaptation/test/fake_resource.h"
#include "common_video/h264/h264_common.h"
#include "common_video/include/video_frame_buffer.h"
#include "media/base/video_adapter.h"
#include "media/engine/webrtc_video_engine.h"
#include "modules/video_coding/codecs/vp9/include/vp9_globals.h"
#include "modules/video_coding/utility/quality_scaler.h"
#include "modules/video_coding/utility/simulcast_rate_allocator.h"
#include "rtc_base/event.h"
#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/synchronization/mutex.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"
#include "system_wrappers/include/sleep.h"
#include "test/encoder_settings.h"
#include "test/fake_encoder.h"
#include "test/field_trial.h"
#include "test/frame_forwarder.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"
#include "test/video_encoder_proxy_factory.h"
#include "video/send_statistics_proxy.h"

namespace webrtc {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::Le;
using ::testing::Lt;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace {
const int kMinPixelsPerFrame = 320 * 180;
const int kQpLow = 1;
const int kQpHigh = 2;
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
const int64_t kProcessIntervalMs = 1000;
const VideoEncoder::ResolutionBitrateLimits
    kEncoderBitrateLimits540p(960 * 540, 100 * 1000, 100 * 1000, 2000 * 1000);
const VideoEncoder::ResolutionBitrateLimits
    kEncoderBitrateLimits720p(1280 * 720, 200 * 1000, 200 * 1000, 4000 * 1000);

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
        last_target_framerate_fps_(-1),
        framerate_updated_event_(true /* manual_reset */,
                                 false /* initially_signaled */) {}
  virtual ~CpuOveruseDetectorProxy() {}

  void OnTargetFramerateUpdated(int framerate_fps) override {
    MutexLock lock(&lock_);
    last_target_framerate_fps_ = framerate_fps;
    OveruseFrameDetector::OnTargetFramerateUpdated(framerate_fps);
    framerate_updated_event_.Set();
  }

  int GetLastTargetFramerate() {
    MutexLock lock(&lock_);
    return last_target_framerate_fps_;
  }

  CpuOveruseOptions GetOptions() { return options_; }

  rtc::Event* framerate_updated_event() { return &framerate_updated_event_; }

 private:
  Mutex lock_;
  int last_target_framerate_fps_ RTC_GUARDED_BY(lock_);
  rtc::Event framerate_updated_event_;
};

class FakeVideoSourceRestrictionsListener
    : public VideoSourceRestrictionsListener {
 public:
  FakeVideoSourceRestrictionsListener()
      : was_restrictions_updated_(false), restrictions_updated_event_() {}
  ~FakeVideoSourceRestrictionsListener() override {
    RTC_DCHECK(was_restrictions_updated_);
  }

  rtc::Event* restrictions_updated_event() {
    return &restrictions_updated_event_;
  }

  // VideoSourceRestrictionsListener implementation.
  void OnVideoSourceRestrictionsUpdated(
      VideoSourceRestrictions restrictions,
      const VideoAdaptationCounters& adaptation_counters,
      rtc::scoped_refptr<Resource> reason,
      const VideoSourceRestrictions& unfiltered_restrictions) override {
    was_restrictions_updated_ = true;
    restrictions_updated_event_.Set();
  }

 private:
  bool was_restrictions_updated_;
  rtc::Event restrictions_updated_event_;
};

auto WantsFps(Matcher<int> fps_matcher) {
  return Field("max_framerate_fps", &rtc::VideoSinkWants::max_framerate_fps,
               fps_matcher);
}

auto WantsMaxPixels(Matcher<int> max_pixel_matcher) {
  return Field("max_pixel_count", &rtc::VideoSinkWants::max_pixel_count,
               AllOf(max_pixel_matcher, Gt(0)));
}

auto ResolutionMax() {
  return AllOf(
      WantsMaxPixels(Eq(std::numeric_limits<int>::max())),
      Field("target_pixel_count", &rtc::VideoSinkWants::target_pixel_count,
            Eq(absl::nullopt)));
}

auto FpsMax() {
  return WantsFps(Eq(kDefaultFramerate));
}

auto FpsUnlimited() {
  return WantsFps(Eq(std::numeric_limits<int>::max()));
}

auto FpsMatchesResolutionMax(Matcher<int> fps_matcher) {
  return AllOf(WantsFps(fps_matcher), ResolutionMax());
}

auto FpsMaxResolutionMatches(Matcher<int> pixel_matcher) {
  return AllOf(FpsMax(), WantsMaxPixels(pixel_matcher));
}

auto FpsMaxResolutionMax() {
  return AllOf(FpsMax(), ResolutionMax());
}

auto UnlimitedSinkWants() {
  return AllOf(FpsUnlimited(), ResolutionMax());
}

auto FpsInRangeForPixelsInBalanced(int last_frame_pixels) {
  Matcher<int> fps_range_matcher;

  if (last_frame_pixels <= 320 * 240) {
    fps_range_matcher = AllOf(Ge(7), Le(10));
  } else if (last_frame_pixels <= 480 * 360) {
    fps_range_matcher = AllOf(Ge(10), Le(15));
  } else if (last_frame_pixels <= 640 * 480) {
    fps_range_matcher = Ge(15);
  } else {
    fps_range_matcher = Eq(kDefaultFramerate);
  }
  return Field("max_framerate_fps", &rtc::VideoSinkWants::max_framerate_fps,
               fps_range_matcher);
}

auto FpsEqResolutionEqTo(const rtc::VideoSinkWants& other_wants) {
  return AllOf(WantsFps(Eq(other_wants.max_framerate_fps)),
               WantsMaxPixels(Eq(other_wants.max_pixel_count)));
}

auto FpsMaxResolutionLt(const rtc::VideoSinkWants& other_wants) {
  return AllOf(FpsMax(), WantsMaxPixels(Lt(other_wants.max_pixel_count)));
}

auto FpsMaxResolutionGt(const rtc::VideoSinkWants& other_wants) {
  return AllOf(FpsMax(), WantsMaxPixels(Gt(other_wants.max_pixel_count)));
}

auto FpsLtResolutionEq(const rtc::VideoSinkWants& other_wants) {
  return AllOf(WantsFps(Lt(other_wants.max_framerate_fps)),
               WantsMaxPixels(Eq(other_wants.max_pixel_count)));
}

auto FpsGtResolutionEq(const rtc::VideoSinkWants& other_wants) {
  return AllOf(WantsFps(Gt(other_wants.max_framerate_fps)),
               WantsMaxPixels(Eq(other_wants.max_pixel_count)));
}

auto FpsEqResolutionLt(const rtc::VideoSinkWants& other_wants) {
  return AllOf(WantsFps(Eq(other_wants.max_framerate_fps)),
               WantsMaxPixels(Lt(other_wants.max_pixel_count)));
}

auto FpsEqResolutionGt(const rtc::VideoSinkWants& other_wants) {
  return AllOf(WantsFps(Eq(other_wants.max_framerate_fps)),
               WantsMaxPixels(Gt(other_wants.max_pixel_count)));
}

class VideoStreamEncoderUnderTest : public VideoStreamEncoder {
 public:
  VideoStreamEncoderUnderTest(TimeController* time_controller,
                              TaskQueueFactory* task_queue_factory,
                              SendStatisticsProxy* stats_proxy,
                              const VideoStreamEncoderSettings& settings)
      : VideoStreamEncoder(time_controller->GetClock(),
                           1 /* number_of_cores */,
                           stats_proxy,
                           settings,
                           std::unique_ptr<OveruseFrameDetector>(
                               overuse_detector_proxy_ =
                                   new CpuOveruseDetectorProxy(stats_proxy)),
                           task_queue_factory),
        time_controller_(time_controller),
        fake_cpu_resource_(FakeResource::Create("FakeResource[CPU]")),
        fake_quality_resource_(FakeResource::Create("FakeResource[QP]")),
        fake_adaptation_constraint_("FakeAdaptationConstraint") {
    InjectAdaptationResource(fake_quality_resource_,
                             VideoAdaptationReason::kQuality);
    InjectAdaptationResource(fake_cpu_resource_, VideoAdaptationReason::kCpu);
    InjectAdaptationConstraint(&fake_adaptation_constraint_);
  }

  void SetSourceAndWaitForRestrictionsUpdated(
      rtc::VideoSourceInterface<VideoFrame>* source,
      const DegradationPreference& degradation_preference) {
    FakeVideoSourceRestrictionsListener listener;
    AddRestrictionsListenerForTesting(&listener);
    SetSource(source, degradation_preference);
    listener.restrictions_updated_event()->Wait(5000);
    RemoveRestrictionsListenerForTesting(&listener);
  }

  void SetSourceAndWaitForFramerateUpdated(
      rtc::VideoSourceInterface<VideoFrame>* source,
      const DegradationPreference& degradation_preference) {
    overuse_detector_proxy_->framerate_updated_event()->Reset();
    SetSource(source, degradation_preference);
    overuse_detector_proxy_->framerate_updated_event()->Wait(5000);
  }

  void OnBitrateUpdatedAndWaitForManagedResources(
      DataRate target_bitrate,
      DataRate stable_target_bitrate,
      DataRate link_allocation,
      uint8_t fraction_lost,
      int64_t round_trip_time_ms,
      double cwnd_reduce_ratio) {
    OnBitrateUpdated(target_bitrate, stable_target_bitrate, link_allocation,
                     fraction_lost, round_trip_time_ms, cwnd_reduce_ratio);
    // Bitrate is updated on the encoder queue.
    WaitUntilTaskQueueIsIdle();
  }

  // This is used as a synchronisation mechanism, to make sure that the
  // encoder queue is not blocked before we start sending it frames.
  void WaitUntilTaskQueueIsIdle() {
    rtc::Event event;
    encoder_queue()->PostTask([&event] { event.Set(); });
    ASSERT_TRUE(event.Wait(5000));
  }

  // Triggers resource usage measurements on the fake CPU resource.
  void TriggerCpuOveruse() {
    rtc::Event event;
    encoder_queue()->PostTask([this, &event] {
      fake_cpu_resource_->SetUsageState(ResourceUsageState::kOveruse);
      event.Set();
    });
    ASSERT_TRUE(event.Wait(5000));
    time_controller_->AdvanceTime(TimeDelta::Millis(0));
  }

  void TriggerCpuUnderuse() {
    rtc::Event event;
    encoder_queue()->PostTask([this, &event] {
      fake_cpu_resource_->SetUsageState(ResourceUsageState::kUnderuse);
      event.Set();
    });
    ASSERT_TRUE(event.Wait(5000));
    time_controller_->AdvanceTime(TimeDelta::Millis(0));
  }

  // Triggers resource usage measurements on the fake quality resource.
  void TriggerQualityLow() {
    rtc::Event event;
    encoder_queue()->PostTask([this, &event] {
      fake_quality_resource_->SetUsageState(ResourceUsageState::kOveruse);
      event.Set();
    });
    ASSERT_TRUE(event.Wait(5000));
    time_controller_->AdvanceTime(TimeDelta::Millis(0));
  }
  void TriggerQualityHigh() {
    rtc::Event event;
    encoder_queue()->PostTask([this, &event] {
      fake_quality_resource_->SetUsageState(ResourceUsageState::kUnderuse);
      event.Set();
    });
    ASSERT_TRUE(event.Wait(5000));
    time_controller_->AdvanceTime(TimeDelta::Millis(0));
  }

  TimeController* const time_controller_;
  CpuOveruseDetectorProxy* overuse_detector_proxy_;
  rtc::scoped_refptr<FakeResource> fake_cpu_resource_;
  rtc::scoped_refptr<FakeResource> fake_quality_resource_;
  FakeAdaptationConstraint fake_adaptation_constraint_;
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
  explicit AdaptingFrameForwarder(TimeController* time_controller)
      : time_controller_(time_controller), adaptation_enabled_(false) {}
  ~AdaptingFrameForwarder() override {}

  void set_adaptation_enabled(bool enabled) {
    MutexLock lock(&mutex_);
    adaptation_enabled_ = enabled;
  }

  bool adaption_enabled() const {
    MutexLock lock(&mutex_);
    return adaptation_enabled_;
  }

  rtc::VideoSinkWants last_wants() const {
    MutexLock lock(&mutex_);
    return last_wants_;
  }

  absl::optional<int> last_sent_width() const { return last_width_; }
  absl::optional<int> last_sent_height() const { return last_height_; }

  void IncomingCapturedFrame(const VideoFrame& video_frame) override {
    RTC_DCHECK(time_controller_->GetMainThread()->IsCurrent());
    time_controller_->AdvanceTime(TimeDelta::Millis(0));

    int cropped_width = 0;
    int cropped_height = 0;
    int out_width = 0;
    int out_height = 0;
    if (adaption_enabled()) {
      RTC_DLOG(INFO) << "IncomingCapturedFrame: AdaptFrameResolution()"
                     << "w=" << video_frame.width()
                     << "h=" << video_frame.height();
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
        if (video_frame.has_update_rect()) {
          adapted_frame.set_update_rect(
              video_frame.update_rect().ScaleWithFrame(
                  video_frame.width(), video_frame.height(), 0, 0,
                  video_frame.width(), video_frame.height(), out_width,
                  out_height));
        }
        test::FrameForwarder::IncomingCapturedFrame(adapted_frame);
        last_width_.emplace(adapted_frame.width());
        last_height_.emplace(adapted_frame.height());
      } else {
        last_width_ = absl::nullopt;
        last_height_ = absl::nullopt;
      }
    } else {
      RTC_DLOG(INFO) << "IncomingCapturedFrame: adaptation not enabled";
      test::FrameForwarder::IncomingCapturedFrame(video_frame);
      last_width_.emplace(video_frame.width());
      last_height_.emplace(video_frame.height());
    }
  }

  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override {
    MutexLock lock(&mutex_);
    last_wants_ = sink_wants_locked();
    adapter_.OnSinkWants(wants);
    test::FrameForwarder::AddOrUpdateSinkLocked(sink, wants);
  }

  TimeController* const time_controller_;
  cricket::VideoAdapter adapter_;
  bool adaptation_enabled_ RTC_GUARDED_BY(mutex_);
  rtc::VideoSinkWants last_wants_ RTC_GUARDED_BY(mutex_);
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
    MutexLock lock(&lock_);
    if (mock_stats_)
      return *mock_stats_;
    return SendStatisticsProxy::GetStats();
  }

  int GetInputFrameRate() const override {
    MutexLock lock(&lock_);
    if (mock_stats_)
      return mock_stats_->input_frame_rate;
    return SendStatisticsProxy::GetInputFrameRate();
  }
  void SetMockStats(const VideoSendStream::Stats& stats) {
    MutexLock lock(&lock_);
    mock_stats_.emplace(stats);
  }

  void ResetMockStats() {
    MutexLock lock(&lock_);
    mock_stats_.reset();
  }

  void SetDroppedFrameCallback(std::function<void(DropReason)> callback) {
    on_frame_dropped_ = std::move(callback);
  }

 private:
  void OnFrameDropped(DropReason reason) override {
    SendStatisticsProxy::OnFrameDropped(reason);
    if (on_frame_dropped_)
      on_frame_dropped_(reason);
  }

  mutable Mutex lock_;
  absl::optional<VideoSendStream::Stats> mock_stats_ RTC_GUARDED_BY(lock_);
  std::function<void(DropReason)> on_frame_dropped_;
};

class MockBitrateObserver : public VideoBitrateAllocationObserver {
 public:
  MOCK_METHOD(void,
              OnBitrateAllocationUpdated,
              (const VideoBitrateAllocation&),
              (override));
};

class MockEncoderSelector
    : public VideoEncoderFactory::EncoderSelectorInterface {
 public:
  MOCK_METHOD(void,
              OnCurrentEncoder,
              (const SdpVideoFormat& format),
              (override));
  MOCK_METHOD(absl::optional<SdpVideoFormat>,
              OnAvailableBitrate,
              (const DataRate& rate),
              (override));
  MOCK_METHOD(absl::optional<SdpVideoFormat>, OnEncoderBroken, (), (override));
};

}  // namespace

class VideoStreamEncoderTest : public ::testing::Test {
 public:
  static const int kDefaultTimeoutMs = 1000;

  VideoStreamEncoderTest()
      : video_send_config_(VideoSendStream::Config(nullptr)),
        codec_width_(320),
        codec_height_(240),
        max_framerate_(kDefaultFramerate),
        fake_encoder_(&time_controller_),
        encoder_factory_(&fake_encoder_),
        stats_proxy_(new MockableSendStatisticsProxy(
            time_controller_.GetClock(),
            video_send_config_,
            webrtc::VideoEncoderConfig::ContentType::kRealtimeVideo)),
        sink_(&time_controller_, &fake_encoder_) {}

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

    ConfigureEncoder(std::move(video_encoder_config));
  }

  void ConfigureEncoder(VideoEncoderConfig video_encoder_config) {
    if (video_stream_encoder_)
      video_stream_encoder_->Stop();
    video_stream_encoder_.reset(new VideoStreamEncoderUnderTest(
        &time_controller_, GetTaskQueueFactory(), stats_proxy_.get(),
        video_send_config_.encoder_settings));
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
      vp9_settings.automaticResizeOn = num_spatial_layers <= 1;
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
            .set_update_rect(VideoFrame::UpdateRect{offset_x, 0, 1, 1})
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
    video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
        DataRate::BitsPerSec(kTargetBitrateBps),
        DataRate::BitsPerSec(kTargetBitrateBps),
        DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

    video_source_.IncomingCapturedFrame(
        CreateFrame(1, codec_width_, codec_height_));
    WaitForEncodedFrame(1);
  }

  void WaitForEncodedFrame(int64_t expected_ntp_time) {
    sink_.WaitForEncodedFrame(expected_ntp_time);
    AdvanceTime(TimeDelta::Seconds(1) / max_framerate_);
  }

  bool TimedWaitForEncodedFrame(int64_t expected_ntp_time, int64_t timeout_ms) {
    bool ok = sink_.TimedWaitForEncodedFrame(expected_ntp_time, timeout_ms);
    AdvanceTime(TimeDelta::Seconds(1) / max_framerate_);
    return ok;
  }

  void WaitForEncodedFrame(uint32_t expected_width, uint32_t expected_height) {
    sink_.WaitForEncodedFrame(expected_width, expected_height);
    AdvanceTime(TimeDelta::Seconds(1) / max_framerate_);
  }

  void ExpectDroppedFrame() {
    sink_.ExpectDroppedFrame();
    AdvanceTime(TimeDelta::Seconds(1) / max_framerate_);
  }

  bool WaitForFrame(int64_t timeout_ms) {
    bool ok = sink_.WaitForFrame(timeout_ms);
    AdvanceTime(TimeDelta::Seconds(1) / max_framerate_);
    return ok;
  }

  class TestEncoder : public test::FakeEncoder {
   public:
    explicit TestEncoder(TimeController* time_controller)
        : FakeEncoder(time_controller->GetClock()),
          time_controller_(time_controller) {
      RTC_DCHECK(time_controller_);
    }

    VideoCodec codec_config() const {
      MutexLock lock(&mutex_);
      return config_;
    }

    void BlockNextEncode() {
      MutexLock lock(&local_mutex_);
      block_next_encode_ = true;
    }

    VideoEncoder::EncoderInfo GetEncoderInfo() const override {
      MutexLock lock(&local_mutex_);
      EncoderInfo info;
      if (initialized_ == EncoderState::kInitialized) {
        if (quality_scaling_) {
          info.scaling_settings = VideoEncoder::ScalingSettings(
              kQpLow, kQpHigh, kMinPixelsPerFrame);
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
      info.requested_resolution_alignment = requested_resolution_alignment_;
      info.apply_alignment_to_all_simulcast_layers =
          apply_alignment_to_all_simulcast_layers_;
      return info;
    }

    int32_t RegisterEncodeCompleteCallback(
        EncodedImageCallback* callback) override {
      MutexLock lock(&local_mutex_);
      encoded_image_callback_ = callback;
      return FakeEncoder::RegisterEncodeCompleteCallback(callback);
    }

    void ContinueEncode() { continue_encode_event_.Set(); }

    void CheckLastTimeStampsMatch(int64_t ntp_time_ms,
                                  uint32_t timestamp) const {
      MutexLock lock(&local_mutex_);
      EXPECT_EQ(timestamp_, timestamp);
      EXPECT_EQ(ntp_time_ms_, ntp_time_ms);
    }

    void SetQualityScaling(bool b) {
      MutexLock lock(&local_mutex_);
      quality_scaling_ = b;
    }

    void SetRequestedResolutionAlignment(int requested_resolution_alignment) {
      MutexLock lock(&local_mutex_);
      requested_resolution_alignment_ = requested_resolution_alignment;
    }

    void SetApplyAlignmentToAllSimulcastLayers(bool b) {
      MutexLock lock(&local_mutex_);
      apply_alignment_to_all_simulcast_layers_ = b;
    }

    void SetIsHardwareAccelerated(bool is_hardware_accelerated) {
      MutexLock lock(&local_mutex_);
      is_hardware_accelerated_ = is_hardware_accelerated;
    }

    void SetTemporalLayersSupported(size_t spatial_idx, bool supported) {
      RTC_DCHECK_LT(spatial_idx, kMaxSpatialLayers);
      MutexLock lock(&local_mutex_);
      temporal_layers_supported_[spatial_idx] = supported;
    }

    void SetResolutionBitrateLimits(
        std::vector<ResolutionBitrateLimits> thresholds) {
      MutexLock lock(&local_mutex_);
      resolution_bitrate_limits_ = thresholds;
    }

    void ForceInitEncodeFailure(bool force_failure) {
      MutexLock lock(&local_mutex_);
      force_init_encode_failed_ = force_failure;
    }

    void SimulateOvershoot(double rate_factor) {
      MutexLock lock(&local_mutex_);
      rate_factor_ = rate_factor;
    }

    uint32_t GetLastFramerate() const {
      MutexLock lock(&local_mutex_);
      return last_framerate_;
    }

    VideoFrame::UpdateRect GetLastUpdateRect() const {
      MutexLock lock(&local_mutex_);
      return last_update_rect_;
    }

    const std::vector<VideoFrameType>& LastFrameTypes() const {
      MutexLock lock(&local_mutex_);
      return last_frame_types_;
    }

    void InjectFrame(const VideoFrame& input_image, bool keyframe) {
      const std::vector<VideoFrameType> frame_type = {
          keyframe ? VideoFrameType::kVideoFrameKey
                   : VideoFrameType::kVideoFrameDelta};
      {
        MutexLock lock(&local_mutex_);
        last_frame_types_ = frame_type;
      }
      FakeEncoder::Encode(input_image, &frame_type);
    }

    void InjectEncodedImage(const EncodedImage& image) {
      MutexLock lock(&local_mutex_);
      encoded_image_callback_->OnEncodedImage(image, nullptr);
    }

    void SetEncodedImageData(
        rtc::scoped_refptr<EncodedImageBufferInterface> encoded_image_data) {
      MutexLock lock(&local_mutex_);
      encoded_image_data_ = encoded_image_data;
    }

    void ExpectNullFrame() {
      MutexLock lock(&local_mutex_);
      expect_null_frame_ = true;
    }

    absl::optional<VideoEncoder::RateControlParameters>
    GetAndResetLastRateControlSettings() {
      auto settings = last_rate_control_settings_;
      last_rate_control_settings_.reset();
      return settings;
    }

    int GetNumEncoderInitializations() const {
      MutexLock lock(&local_mutex_);
      return num_encoder_initializations_;
    }

    int GetNumSetRates() const {
      MutexLock lock(&local_mutex_);
      return num_set_rates_;
    }

    VideoCodec video_codec() const {
      MutexLock lock(&local_mutex_);
      return video_codec_;
    }

   private:
    int32_t Encode(const VideoFrame& input_image,
                   const std::vector<VideoFrameType>* frame_types) override {
      bool block_encode;
      {
        MutexLock lock(&local_mutex_);
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

    CodecSpecificInfo EncodeHook(
        EncodedImage& encoded_image,
        rtc::scoped_refptr<EncodedImageBuffer> buffer) override {
      CodecSpecificInfo codec_specific;
      {
        MutexLock lock(&mutex_);
        codec_specific.codecType = config_.codecType;
      }
      MutexLock lock(&local_mutex_);
      if (encoded_image_data_) {
        encoded_image.SetEncodedData(encoded_image_data_);
      }
      return codec_specific;
    }

    int32_t InitEncode(const VideoCodec* config,
                       const Settings& settings) override {
      int res = FakeEncoder::InitEncode(config, settings);

      MutexLock lock(&local_mutex_);
      EXPECT_EQ(initialized_, EncoderState::kUninitialized);

      ++num_encoder_initializations_;
      video_codec_ = *config;

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
      MutexLock lock(&local_mutex_);
      EXPECT_NE(initialized_, EncoderState::kUninitialized);
      initialized_ = EncoderState::kUninitialized;
      return FakeEncoder::Release();
    }

    void SetRates(const RateControlParameters& parameters) {
      MutexLock lock(&local_mutex_);
      num_set_rates_++;
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

    TimeController* const time_controller_;
    mutable Mutex local_mutex_;
    enum class EncoderState {
      kUninitialized,
      kInitializationFailed,
      kInitialized
    } initialized_ RTC_GUARDED_BY(local_mutex_) = EncoderState::kUninitialized;
    bool block_next_encode_ RTC_GUARDED_BY(local_mutex_) = false;
    rtc::Event continue_encode_event_;
    uint32_t timestamp_ RTC_GUARDED_BY(local_mutex_) = 0;
    int64_t ntp_time_ms_ RTC_GUARDED_BY(local_mutex_) = 0;
    int last_input_width_ RTC_GUARDED_BY(local_mutex_) = 0;
    int last_input_height_ RTC_GUARDED_BY(local_mutex_) = 0;
    bool quality_scaling_ RTC_GUARDED_BY(local_mutex_) = true;
    int requested_resolution_alignment_ RTC_GUARDED_BY(local_mutex_) = 1;
    bool apply_alignment_to_all_simulcast_layers_ RTC_GUARDED_BY(local_mutex_) =
        false;
    bool is_hardware_accelerated_ RTC_GUARDED_BY(local_mutex_) = false;
    rtc::scoped_refptr<EncodedImageBufferInterface> encoded_image_data_
        RTC_GUARDED_BY(local_mutex_);
    std::unique_ptr<Vp8FrameBufferController> frame_buffer_controller_
        RTC_GUARDED_BY(local_mutex_);
    absl::optional<bool>
        temporal_layers_supported_[kMaxSpatialLayers] RTC_GUARDED_BY(
            local_mutex_);
    bool force_init_encode_failed_ RTC_GUARDED_BY(local_mutex_) = false;
    double rate_factor_ RTC_GUARDED_BY(local_mutex_) = 1.0;
    uint32_t last_framerate_ RTC_GUARDED_BY(local_mutex_) = 0;
    absl::optional<VideoEncoder::RateControlParameters>
        last_rate_control_settings_;
    VideoFrame::UpdateRect last_update_rect_ RTC_GUARDED_BY(local_mutex_) = {
        0, 0, 0, 0};
    std::vector<VideoFrameType> last_frame_types_;
    bool expect_null_frame_ = false;
    EncodedImageCallback* encoded_image_callback_ RTC_GUARDED_BY(local_mutex_) =
        nullptr;
    NiceMock<MockFecControllerOverride> fec_controller_override_;
    int num_encoder_initializations_ RTC_GUARDED_BY(local_mutex_) = 0;
    std::vector<ResolutionBitrateLimits> resolution_bitrate_limits_
        RTC_GUARDED_BY(local_mutex_);
    int num_set_rates_ RTC_GUARDED_BY(local_mutex_) = 0;
    VideoCodec video_codec_ RTC_GUARDED_BY(local_mutex_);
  };

  class TestSink : public VideoStreamEncoder::EncoderSink {
   public:
    TestSink(TimeController* time_controller, TestEncoder* test_encoder)
        : time_controller_(time_controller), test_encoder_(test_encoder) {
      RTC_DCHECK(time_controller_);
    }

    void WaitForEncodedFrame(int64_t expected_ntp_time) {
      EXPECT_TRUE(
          TimedWaitForEncodedFrame(expected_ntp_time, kDefaultTimeoutMs));
    }

    bool TimedWaitForEncodedFrame(int64_t expected_ntp_time,
                                  int64_t timeout_ms) {
      uint32_t timestamp = 0;
      if (!WaitForFrame(timeout_ms))
        return false;
      {
        MutexLock lock(&mutex_);
        timestamp = last_timestamp_;
      }
      test_encoder_->CheckLastTimeStampsMatch(expected_ntp_time, timestamp);
      return true;
    }

    void WaitForEncodedFrame(uint32_t expected_width,
                             uint32_t expected_height) {
      EXPECT_TRUE(WaitForFrame(kDefaultTimeoutMs));
      CheckLastFrameSizeMatches(expected_width, expected_height);
    }

    void CheckLastFrameSizeMatches(uint32_t expected_width,
                                   uint32_t expected_height) {
      uint32_t width = 0;
      uint32_t height = 0;
      {
        MutexLock lock(&mutex_);
        width = last_width_;
        height = last_height_;
      }
      EXPECT_EQ(expected_height, height);
      EXPECT_EQ(expected_width, width);
    }

    void CheckLastFrameRotationMatches(VideoRotation expected_rotation) {
      VideoRotation rotation;
      {
        MutexLock lock(&mutex_);
        rotation = last_rotation_;
      }
      EXPECT_EQ(expected_rotation, rotation);
    }

    void ExpectDroppedFrame() { EXPECT_FALSE(WaitForFrame(100)); }

    bool WaitForFrame(int64_t timeout_ms) {
      RTC_DCHECK(time_controller_->GetMainThread()->IsCurrent());
      bool ret = encoded_frame_event_.Wait(timeout_ms);
      time_controller_->AdvanceTime(TimeDelta::Millis(0));
      return ret;
    }

    void SetExpectNoFrames() {
      MutexLock lock(&mutex_);
      expect_frames_ = false;
    }

    int number_of_reconfigurations() const {
      MutexLock lock(&mutex_);
      return number_of_reconfigurations_;
    }

    int last_min_transmit_bitrate() const {
      MutexLock lock(&mutex_);
      return min_transmit_bitrate_bps_;
    }

    void SetNumExpectedLayers(size_t num_layers) {
      MutexLock lock(&mutex_);
      num_expected_layers_ = num_layers;
    }

    int64_t GetLastCaptureTimeMs() const {
      MutexLock lock(&mutex_);
      return last_capture_time_ms_;
    }

    std::vector<uint8_t> GetLastEncodedImageData() {
      MutexLock lock(&mutex_);
      return std::move(last_encoded_image_data_);
    }

   private:
    Result OnEncodedImage(
        const EncodedImage& encoded_image,
        const CodecSpecificInfo* codec_specific_info) override {
      MutexLock lock(&mutex_);
      EXPECT_TRUE(expect_frames_);
      last_encoded_image_data_ = std::vector<uint8_t>(
          encoded_image.data(), encoded_image.data() + encoded_image.size());
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
        bool is_svc,
        VideoEncoderConfig::ContentType content_type,
        int min_transmit_bitrate_bps) override {
      MutexLock lock(&mutex_);
      ++number_of_reconfigurations_;
      min_transmit_bitrate_bps_ = min_transmit_bitrate_bps;
    }

    TimeController* const time_controller_;
    mutable Mutex mutex_;
    TestEncoder* test_encoder_;
    rtc::Event encoded_frame_event_;
    std::vector<uint8_t> last_encoded_image_data_;
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
      MutexLock lock(&mutex_);
      codec_config_ = codec;
      return bitrate_allocator_factory_->CreateVideoBitrateAllocator(codec);
    }

    VideoCodec codec_config() const {
      MutexLock lock(&mutex_);
      return codec_config_;
    }

   private:
    std::unique_ptr<VideoBitrateAllocatorFactory> bitrate_allocator_factory_;

    mutable Mutex mutex_;
    VideoCodec codec_config_ RTC_GUARDED_BY(mutex_);
  };

  Clock* clock() { return time_controller_.GetClock(); }
  void AdvanceTime(TimeDelta duration) {
    time_controller_.AdvanceTime(duration);
  }

  int64_t CurrentTimeMs() { return clock()->CurrentTime().ms(); }

 protected:
  virtual TaskQueueFactory* GetTaskQueueFactory() {
    return time_controller_.GetTaskQueueFactory();
  }

  GlobalSimulatedTimeController time_controller_{Timestamp::Micros(1234)};
  VideoSendStream::Config video_send_config_;
  VideoEncoderConfig video_encoder_config_;
  int codec_width_;
  int codec_height_;
  int max_framerate_;
  TestEncoder fake_encoder_;
  test::VideoEncoderProxyFactory encoder_factory_;
  VideoBitrateAllocatorProxyFactory bitrate_allocator_factory_;
  std::unique_ptr<MockableSendStatisticsProxy> stats_proxy_;
  TestSink sink_;
  AdaptingFrameForwarder video_source_{&time_controller_};
  std::unique_ptr<VideoStreamEncoderUnderTest> video_stream_encoder_;
};

TEST_F(VideoStreamEncoderTest, EncodeOneFrame) {
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
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
  AdvanceTime(TimeDelta::Millis(10));
  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));
  EXPECT_TRUE(frame_destroyed_event.Wait(kDefaultTimeoutMs));

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  // The pending frame should be received.
  WaitForEncodedFrame(2);
  video_source_.IncomingCapturedFrame(CreateFrame(3, nullptr));

  WaitForEncodedFrame(3);
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, DropsFramesWhenRateSetToZero) {
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  WaitForEncodedFrame(1);

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(0), DataRate::BitsPerSec(0), DataRate::BitsPerSec(0),
      0, 0, 0);
  // The encoder will cache up to one frame for a short duration. Adding two
  // frames means that the first frame will be dropped and the second frame will
  // be sent when the encoder is resumed.
  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));
  video_source_.IncomingCapturedFrame(CreateFrame(3, nullptr));

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
  WaitForEncodedFrame(3);
  video_source_.IncomingCapturedFrame(CreateFrame(4, nullptr));
  WaitForEncodedFrame(4);
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, DropsFramesWithSameOrOldNtpTimestamp) {
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  WaitForEncodedFrame(1);

  // This frame will be dropped since it has the same ntp timestamp.
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));

  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));
  WaitForEncodedFrame(2);
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, DropsFrameAfterStop) {
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  WaitForEncodedFrame(1);

  video_stream_encoder_->Stop();
  sink_.SetExpectNoFrames();
  rtc::Event frame_destroyed_event;
  video_source_.IncomingCapturedFrame(CreateFrame(2, &frame_destroyed_event));
  EXPECT_TRUE(frame_destroyed_event.Wait(kDefaultTimeoutMs));
}

class VideoStreamEncoderBlockedTest : public VideoStreamEncoderTest {
 public:
  VideoStreamEncoderBlockedTest() {}

  TaskQueueFactory* GetTaskQueueFactory() override {
    return task_queue_factory_.get();
  }

 private:
  std::unique_ptr<TaskQueueFactory> task_queue_factory_ =
      CreateDefaultTaskQueueFactory();
};

TEST_F(VideoStreamEncoderBlockedTest, DropsPendingFramesOnSlowEncode) {
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  int dropped_count = 0;
  stats_proxy_->SetDroppedFrameCallback(
      [&dropped_count](VideoStreamEncoderObserver::DropReason) {
        ++dropped_count;
      });

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

  EXPECT_EQ(1, dropped_count);
}

TEST_F(VideoStreamEncoderTest, DropFrameWithFailedI420Conversion) {
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
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

TEST_F(VideoStreamEncoderTest, DropsFramesWhenCongestionWindowPushbackSet) {
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  WaitForEncodedFrame(1);

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0.5);
  // The congestion window pushback is set to 0.5, which will drop 1/2 of
  // frames. Adding two frames means that the first frame will be dropped and
  // the second frame will be sent to the encoder.
  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));
  video_source_.IncomingCapturedFrame(CreateFrame(3, nullptr));
  WaitForEncodedFrame(3);
  video_source_.IncomingCapturedFrame(CreateFrame(4, nullptr));
  video_source_.IncomingCapturedFrame(CreateFrame(5, nullptr));
  WaitForEncodedFrame(5);
  EXPECT_EQ(2u, stats_proxy_->GetStats().frames_dropped_by_congestion_window);
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       ConfigureEncoderTriggersOnEncoderConfigurationChanged) {
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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
       IntersectionOfEncoderAndAppBitrateLimitsUsedWhenBothProvided) {
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  const uint32_t kMinEncBitrateKbps = 100;
  const uint32_t kMaxEncBitrateKbps = 1000;
  const VideoEncoder::ResolutionBitrateLimits encoder_bitrate_limits(
      /*frame_size_pixels=*/codec_width_ * codec_height_,
      /*min_start_bitrate_bps=*/0,
      /*min_bitrate_bps=*/kMinEncBitrateKbps * 1000,
      /*max_bitrate_bps=*/kMaxEncBitrateKbps * 1000);
  fake_encoder_.SetResolutionBitrateLimits({encoder_bitrate_limits});

  VideoEncoderConfig video_encoder_config;
  test::FillEncoderConfiguration(kVideoCodecVP8, 1, &video_encoder_config);
  video_encoder_config.max_bitrate_bps = (kMaxEncBitrateKbps + 1) * 1000;
  video_encoder_config.simulcast_layers[0].min_bitrate_bps =
      (kMinEncBitrateKbps + 1) * 1000;
  video_stream_encoder_->ConfigureEncoder(video_encoder_config.Copy(),
                                          kMaxPayloadLength);

  // When both encoder and app provide bitrate limits, the intersection of
  // provided sets should be used.
  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  WaitForEncodedFrame(1);
  EXPECT_EQ(kMaxEncBitrateKbps,
            bitrate_allocator_factory_.codec_config().maxBitrate);
  EXPECT_EQ(kMinEncBitrateKbps + 1,
            bitrate_allocator_factory_.codec_config().minBitrate);

  video_encoder_config.max_bitrate_bps = (kMaxEncBitrateKbps - 1) * 1000;
  video_encoder_config.simulcast_layers[0].min_bitrate_bps =
      (kMinEncBitrateKbps - 1) * 1000;
  video_stream_encoder_->ConfigureEncoder(video_encoder_config.Copy(),
                                          kMaxPayloadLength);
  video_source_.IncomingCapturedFrame(CreateFrame(2, nullptr));
  WaitForEncodedFrame(2);
  EXPECT_EQ(kMaxEncBitrateKbps - 1,
            bitrate_allocator_factory_.codec_config().maxBitrate);
  EXPECT_EQ(kMinEncBitrateKbps,
            bitrate_allocator_factory_.codec_config().minBitrate);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       EncoderAndAppLimitsDontIntersectEncoderLimitsIgnored) {
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  const uint32_t kMinAppBitrateKbps = 100;
  const uint32_t kMaxAppBitrateKbps = 200;
  const uint32_t kMinEncBitrateKbps = kMaxAppBitrateKbps + 1;
  const uint32_t kMaxEncBitrateKbps = kMaxAppBitrateKbps * 2;
  const VideoEncoder::ResolutionBitrateLimits encoder_bitrate_limits(
      /*frame_size_pixels=*/codec_width_ * codec_height_,
      /*min_start_bitrate_bps=*/0,
      /*min_bitrate_bps=*/kMinEncBitrateKbps * 1000,
      /*max_bitrate_bps=*/kMaxEncBitrateKbps * 1000);
  fake_encoder_.SetResolutionBitrateLimits({encoder_bitrate_limits});

  VideoEncoderConfig video_encoder_config;
  test::FillEncoderConfiguration(kVideoCodecVP8, 1, &video_encoder_config);
  video_encoder_config.max_bitrate_bps = kMaxAppBitrateKbps * 1000;
  video_encoder_config.simulcast_layers[0].min_bitrate_bps =
      kMinAppBitrateKbps * 1000;
  video_stream_encoder_->ConfigureEncoder(video_encoder_config.Copy(),
                                          kMaxPayloadLength);

  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  WaitForEncodedFrame(1);
  EXPECT_EQ(kMaxAppBitrateKbps,
            bitrate_allocator_factory_.codec_config().maxBitrate);
  EXPECT_EQ(kMinAppBitrateKbps,
            bitrate_allocator_factory_.codec_config().minBitrate);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       EncoderRecommendedMaxAndMinBitratesUsedForGivenResolution) {
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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

class ResolutionAlignmentTest
    : public VideoStreamEncoderTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<int, std::vector<double>>> {
 public:
  ResolutionAlignmentTest()
      : requested_alignment_(::testing::get<0>(GetParam())),
        scale_factors_(::testing::get<1>(GetParam())) {}

 protected:
  const int requested_alignment_;
  const std::vector<double> scale_factors_;
};

INSTANTIATE_TEST_SUITE_P(
    AlignmentAndScaleFactors,
    ResolutionAlignmentTest,
    ::testing::Combine(
        ::testing::Values(1, 2, 3, 4, 5, 6, 16, 22),  // requested_alignment_
        ::testing::Values(std::vector<double>{-1.0},  // scale_factors_
                          std::vector<double>{-1.0, -1.0},
                          std::vector<double>{-1.0, -1.0, -1.0},
                          std::vector<double>{4.0, 2.0, 1.0},
                          std::vector<double>{9999.0, -1.0, 1.0},
                          std::vector<double>{3.99, 2.01, 1.0},
                          std::vector<double>{4.9, 1.7, 1.25},
                          std::vector<double>{10.0, 4.0, 3.0},
                          std::vector<double>{1.75, 3.5},
                          std::vector<double>{1.5, 2.5},
                          std::vector<double>{1.3, 1.0})));

TEST_P(ResolutionAlignmentTest, SinkWantsAlignmentApplied) {
  // Set requested resolution alignment.
  video_source_.set_adaptation_enabled(true);
  fake_encoder_.SetRequestedResolutionAlignment(requested_alignment_);
  fake_encoder_.SetApplyAlignmentToAllSimulcastLayers(true);

  // Fill config with the scaling factor by which to reduce encoding size.
  const int num_streams = scale_factors_.size();
  VideoEncoderConfig config;
  test::FillEncoderConfiguration(kVideoCodecVP8, num_streams, &config);
  for (int i = 0; i < num_streams; ++i) {
    config.simulcast_layers[i].scale_resolution_down_by = scale_factors_[i];
  }
  config.video_stream_factory =
      new rtc::RefCountedObject<cricket::EncoderStreamFactory>(
          "VP8", /*max qp*/ 56, /*screencast*/ false,
          /*screenshare enabled*/ false);
  video_stream_encoder_->ConfigureEncoder(std::move(config), kMaxPayloadLength);

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kSimulcastTargetBitrateBps),
      DataRate::BitsPerSec(kSimulcastTargetBitrateBps),
      DataRate::BitsPerSec(kSimulcastTargetBitrateBps), 0, 0, 0);
  // Wait for all layers before triggering event.
  sink_.SetNumExpectedLayers(num_streams);

  // On the 1st frame, we should have initialized the encoder and
  // asked for its resolution requirements.
  int64_t timestamp_ms = kFrameIntervalMs;
  video_source_.IncomingCapturedFrame(CreateFrame(timestamp_ms, 1280, 720));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_EQ(1, fake_encoder_.GetNumEncoderInitializations());

  // On the 2nd frame, we should be receiving a correctly aligned resolution.
  // (It's up the to the encoder to potentially drop the previous frame,
  // to avoid coding back-to-back keyframes.)
  timestamp_ms += kFrameIntervalMs;
  video_source_.IncomingCapturedFrame(CreateFrame(timestamp_ms, 1280, 720));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_GE(fake_encoder_.GetNumEncoderInitializations(), 1);

  VideoCodec codec = fake_encoder_.video_codec();
  EXPECT_EQ(codec.numberOfSimulcastStreams, num_streams);
  // Frame size should be a multiple of the requested alignment.
  for (int i = 0; i < codec.numberOfSimulcastStreams; ++i) {
    EXPECT_EQ(codec.simulcastStream[i].width % requested_alignment_, 0);
    EXPECT_EQ(codec.simulcastStream[i].height % requested_alignment_, 0);
    // Aspect ratio should match.
    EXPECT_EQ(codec.width * codec.simulcastStream[i].height,
              codec.height * codec.simulcastStream[i].width);
  }

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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
  video_stream_encoder_->SetSource(&video_source_,
                                   webrtc::DegradationPreference::BALANCED);
  EXPECT_THAT(video_source_.sink_wants(), UnlimitedSinkWants());
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
    EXPECT_THAT(
        video_source_.sink_wants(),
        FpsInRangeForPixelsInBalanced(*video_source_.last_sent_width() *
                                      *video_source_.last_sent_height()));
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

    video_stream_encoder_->TriggerCpuUnderuse();
    EXPECT_THAT(
        video_source_.sink_wants(),
        FpsInRangeForPixelsInBalanced(*video_source_.last_sent_width() *
                                      *video_source_.last_sent_height()));
    EXPECT_TRUE(video_source_.sink_wants().max_pixel_count >
                    last_wants.max_pixel_count ||
                video_source_.sink_wants().max_framerate_fps >
                    last_wants.max_framerate_fps);
  }

  EXPECT_THAT(video_source_.sink_wants(), FpsMaxResolutionMax());
  stats_proxy_->ResetMockStats();
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ((loop_count - 1) * 2,
            stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       SinkWantsNotChangedByResourceLimitedBeforeDegradationPreferenceChange) {
  video_stream_encoder_->OnBitrateUpdated(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
  EXPECT_THAT(video_source_.sink_wants(), UnlimitedSinkWants());

  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;

  int64_t ntp_time = kFrameIntervalMs;

  // Force an input frame rate to be available, or the adaptation call won't
  // know what framerate to adapt form.
  const int kInputFps = 30;
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  stats.input_frame_rate = kInputFps;
  stats_proxy_->SetMockStats(stats);

  video_source_.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(
      &video_source_, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);
  EXPECT_THAT(video_source_.sink_wants(), UnlimitedSinkWants());
  video_source_.IncomingCapturedFrame(
      CreateFrame(ntp_time, kFrameWidth, kFrameHeight));
  sink_.WaitForEncodedFrame(ntp_time);
  ntp_time += kFrameIntervalMs;

  // Trigger CPU overuse.
  video_stream_encoder_->TriggerCpuOveruse();
  video_source_.IncomingCapturedFrame(
      CreateFrame(ntp_time, kFrameWidth, kFrameHeight));
  sink_.WaitForEncodedFrame(ntp_time);
  ntp_time += kFrameIntervalMs;

  EXPECT_FALSE(video_source_.sink_wants().target_pixel_count);
  EXPECT_EQ(std::numeric_limits<int>::max(),
            video_source_.sink_wants().max_pixel_count);
  // Some framerate constraint should be set.
  int restricted_fps = video_source_.sink_wants().max_framerate_fps;
  EXPECT_LT(restricted_fps, kInputFps);
  video_source_.IncomingCapturedFrame(
      CreateFrame(ntp_time, kFrameWidth, kFrameHeight));
  sink_.WaitForEncodedFrame(ntp_time);
  ntp_time += 100;

  video_stream_encoder_->SetSourceAndWaitForRestrictionsUpdated(
      &video_source_, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);
  // Give the encoder queue time to process the change in degradation preference
  // by waiting for an encoded frame.
  video_source_.IncomingCapturedFrame(
      CreateFrame(ntp_time, kFrameWidth, kFrameHeight));
  sink_.WaitForEncodedFrame(ntp_time);
  ntp_time += kFrameIntervalMs;

  video_stream_encoder_->TriggerQualityLow();
  video_source_.IncomingCapturedFrame(
      CreateFrame(ntp_time, kFrameWidth, kFrameHeight));
  sink_.WaitForEncodedFrame(ntp_time);
  ntp_time += kFrameIntervalMs;

  // Some resolution constraint should be set.
  EXPECT_FALSE(video_source_.sink_wants().target_pixel_count);
  EXPECT_LT(video_source_.sink_wants().max_pixel_count,
            kFrameWidth * kFrameHeight);
  EXPECT_EQ(video_source_.sink_wants().max_framerate_fps, kInputFps);

  int pixel_count = video_source_.sink_wants().max_pixel_count;
  // Triggering a CPU underuse should not change the sink wants since it has
  // not been overused for resolution since we changed degradation preference.
  video_stream_encoder_->TriggerCpuUnderuse();
  video_source_.IncomingCapturedFrame(
      CreateFrame(ntp_time, kFrameWidth, kFrameHeight));
  sink_.WaitForEncodedFrame(ntp_time);
  ntp_time += kFrameIntervalMs;
  EXPECT_EQ(video_source_.sink_wants().max_pixel_count, pixel_count);
  EXPECT_EQ(video_source_.sink_wants().max_framerate_fps, kInputFps);

  // Change the degradation preference back. CPU underuse should not adapt since
  // QP is most limited.
  video_stream_encoder_->SetSourceAndWaitForRestrictionsUpdated(
      &video_source_, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);
  video_source_.IncomingCapturedFrame(
      CreateFrame(ntp_time, kFrameWidth, kFrameHeight));
  sink_.WaitForEncodedFrame(ntp_time);
  ntp_time += 100;
  // Resolution adaptations is gone after changing degradation preference.
  EXPECT_FALSE(video_source_.sink_wants().target_pixel_count);
  EXPECT_EQ(std::numeric_limits<int>::max(),
            video_source_.sink_wants().max_pixel_count);
  // The fps adaptation from above is now back.
  EXPECT_EQ(video_source_.sink_wants().max_framerate_fps, restricted_fps);

  // Trigger CPU underuse.
  video_stream_encoder_->TriggerCpuUnderuse();
  video_source_.IncomingCapturedFrame(
      CreateFrame(ntp_time, kFrameWidth, kFrameHeight));
  sink_.WaitForEncodedFrame(ntp_time);
  ntp_time += kFrameIntervalMs;
  EXPECT_EQ(video_source_.sink_wants().max_framerate_fps, restricted_fps);

  // Trigger QP underuse, fps should return to normal.
  video_stream_encoder_->TriggerQualityHigh();
  video_source_.IncomingCapturedFrame(
      CreateFrame(ntp_time, kFrameWidth, kFrameHeight));
  sink_.WaitForEncodedFrame(ntp_time);
  ntp_time += kFrameIntervalMs;
  EXPECT_THAT(video_source_.sink_wants(), FpsMax());

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, SinkWantsStoredByDegradationPreference) {
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
  EXPECT_THAT(video_source_.sink_wants(), UnlimitedSinkWants());

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
  video_stream_encoder_->SetSourceAndWaitForRestrictionsUpdated(
      &new_video_source, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);
  // Give the encoder queue time to process the change in degradation preference
  // by waiting for an encoded frame.
  new_video_source.IncomingCapturedFrame(
      CreateFrame(frame_timestamp, kFrameWidth, kFrameWidth));
  sink_.WaitForEncodedFrame(frame_timestamp);
  frame_timestamp += kFrameIntervalMs;
  // Initially no degradation registered.
  EXPECT_THAT(new_video_source.sink_wants(), FpsMaxResolutionMax());

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
  video_stream_encoder_->SetSourceAndWaitForRestrictionsUpdated(
      &new_video_source, webrtc::DegradationPreference::DISABLED);
  // Give the encoder queue time to process the change in degradation preference
  // by waiting for an encoded frame.
  new_video_source.IncomingCapturedFrame(
      CreateFrame(frame_timestamp, kFrameWidth, kFrameWidth));
  sink_.WaitForEncodedFrame(frame_timestamp);
  frame_timestamp += kFrameIntervalMs;
  EXPECT_THAT(new_video_source.sink_wants(), FpsMaxResolutionMax());

  video_stream_encoder_->TriggerCpuOveruse();
  new_video_source.IncomingCapturedFrame(
      CreateFrame(frame_timestamp, kFrameWidth, kFrameHeight));
  WaitForEncodedFrame(frame_timestamp);
  frame_timestamp += kFrameIntervalMs;

  // Still no degradation.
  EXPECT_THAT(new_video_source.sink_wants(), FpsMaxResolutionMax());

  // Calling SetSource with resolution scaling enabled apply the old SinkWants.
  video_stream_encoder_->SetSourceAndWaitForRestrictionsUpdated(
      &new_video_source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);
  // Give the encoder queue time to process the change in degradation preference
  // by waiting for an encoded frame.
  new_video_source.IncomingCapturedFrame(
      CreateFrame(frame_timestamp, kFrameWidth, kFrameWidth));
  sink_.WaitForEncodedFrame(frame_timestamp);
  frame_timestamp += kFrameIntervalMs;
  EXPECT_LT(new_video_source.sink_wants().max_pixel_count,
            kFrameWidth * kFrameHeight);
  EXPECT_FALSE(new_video_source.sink_wants().target_pixel_count);
  EXPECT_EQ(kDefaultFramerate, new_video_source.sink_wants().max_framerate_fps);

  // Calling SetSource with framerate scaling enabled apply the old SinkWants.
  video_stream_encoder_->SetSourceAndWaitForRestrictionsUpdated(
      &new_video_source, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);
  // Give the encoder queue time to process the change in degradation preference
  // by waiting for an encoded frame.
  new_video_source.IncomingCapturedFrame(
      CreateFrame(frame_timestamp, kFrameWidth, kFrameWidth));
  sink_.WaitForEncodedFrame(frame_timestamp);
  frame_timestamp += kFrameIntervalMs;
  EXPECT_FALSE(new_video_source.sink_wants().target_pixel_count);
  EXPECT_EQ(std::numeric_limits<int>::max(),
            new_video_source.sink_wants().max_pixel_count);
  EXPECT_LT(new_video_source.sink_wants().max_framerate_fps, kInputFps);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, StatsTracksQualityAdaptationStats) {
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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
  video_stream_encoder_->TriggerCpuUnderuse();
  video_source_.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  WaitForEncodedFrame(3);

  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_EQ(2, stats.number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats.number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, SwitchingSourceKeepsCpuAdaptation) {
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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
  video_stream_encoder_->TriggerCpuUnderuse();
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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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
       StatsTracksCpuAdaptationStatsWhenSwitchingSource_Balanced) {
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  const int kWidth = 1280;
  const int kHeight = 720;
  int sequence = 1;

  // Enable BALANCED preference, no initial limitation.
  test::FrameForwarder source;
  video_stream_encoder_->SetSource(&source,
                                   webrtc::DegradationPreference::BALANCED);
  source.IncomingCapturedFrame(CreateFrame(sequence, kWidth, kHeight));
  WaitForEncodedFrame(sequence++);
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_framerate);
  EXPECT_EQ(0, stats.number_of_cpu_adapt_changes);

  // Trigger CPU overuse, should now adapt down.
  video_stream_encoder_->TriggerCpuOveruse();
  source.IncomingCapturedFrame(CreateFrame(sequence, kWidth, kHeight));
  WaitForEncodedFrame(sequence++);
  stats = stats_proxy_->GetStats();
  EXPECT_EQ(1, stats.number_of_cpu_adapt_changes);

  // Set new degradation preference should clear restrictions since we changed
  // from BALANCED.
  video_stream_encoder_->SetSourceAndWaitForRestrictionsUpdated(
      &source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);
  source.IncomingCapturedFrame(CreateFrame(sequence, kWidth, kHeight));
  WaitForEncodedFrame(sequence++);
  stats = stats_proxy_->GetStats();
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
  source.IncomingCapturedFrame(CreateFrame(sequence, kWidth, kHeight));
  WaitForEncodedFrame(sequence++);

  // We have now adapted once.
  stats = stats_proxy_->GetStats();
  EXPECT_EQ(2, stats.number_of_cpu_adapt_changes);

  // Back to BALANCED, should clear the restrictions again.
  video_stream_encoder_->SetSourceAndWaitForRestrictionsUpdated(
      &source, webrtc::DegradationPreference::BALANCED);
  source.IncomingCapturedFrame(CreateFrame(sequence, kWidth, kHeight));
  WaitForEncodedFrame(sequence++);
  stats = stats_proxy_->GetStats();
  EXPECT_FALSE(stats.cpu_limited_resolution);
  EXPECT_FALSE(stats.cpu_limited_framerate);
  EXPECT_EQ(2, stats.number_of_cpu_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       StatsTracksCpuAdaptationStatsWhenSwitchingSource) {
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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
  video_stream_encoder_->TriggerCpuUnderuse();
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
  video_stream_encoder_->TriggerCpuUnderuse();
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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  // Expect no scaling to begin with.
  EXPECT_THAT(video_source_.sink_wants(), UnlimitedSinkWants());

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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  // Enable MAINTAIN_FRAMERATE preference, no initial limitation.
  test::FrameForwarder source;
  video_stream_encoder_->SetSource(
      &source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  WaitForEncodedFrame(1);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt down, expect scaled down resolution.
  video_stream_encoder_->TriggerCpuOveruse();
  EXPECT_THAT(source.sink_wants(),
              FpsMaxResolutionMatches(Lt(kWidth * kHeight)));
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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  // Enable BALANCED preference, no initial limitation.
  test::FrameForwarder source;
  video_stream_encoder_->SetSource(&source,
                                   webrtc::DegradationPreference::BALANCED);
  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(1);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());

  // Trigger adapt down, expect scaled down resolution.
  video_stream_encoder_->TriggerQualityLow();
  EXPECT_THAT(source.sink_wants(),
              FpsMaxResolutionMatches(Lt(kWidth * kHeight)));
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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  // Enable MAINTAIN_FRAMERATE preference, no initial limitation.
  test::FrameForwarder source;
  video_stream_encoder_->SetSource(
      &source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt up, expect no change.
  video_stream_encoder_->TriggerCpuUnderuse();
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       NoChangeForInitialNormalUsage_MaintainResolutionMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  // Enable MAINTAIN_RESOLUTION preference, no initial limitation.
  test::FrameForwarder source;
  video_stream_encoder_->SetSource(
      &source, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt up, expect no change.
  video_stream_encoder_->TriggerCpuUnderuse();
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, NoChangeForInitialNormalUsage_BalancedMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  // Enable BALANCED preference, no initial limitation.
  test::FrameForwarder source;
  video_stream_encoder_->SetSource(&source,
                                   webrtc::DegradationPreference::BALANCED);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no change.
  video_stream_encoder_->TriggerQualityHigh();
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, NoChangeForInitialNormalUsage_DisabledMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  // Enable DISABLED preference, no initial limitation.
  test::FrameForwarder source;
  video_stream_encoder_->SetSource(&source,
                                   webrtc::DegradationPreference::DISABLED);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no change.
  video_stream_encoder_->TriggerQualityHigh();
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       AdaptsResolutionForLowQuality_MaintainFramerateMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  // Enable MAINTAIN_FRAMERATE preference, no initial limitation.
  AdaptingFrameForwarder source(&time_controller_);
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(
      &source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);

  source.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  WaitForEncodedFrame(1);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution.
  video_stream_encoder_->TriggerQualityLow();
  source.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  WaitForEncodedFrame(2);
  EXPECT_THAT(source.sink_wants(),
              FpsMaxResolutionMatches(Lt(kWidth * kHeight)));
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no restriction.
  video_stream_encoder_->TriggerQualityHigh();
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  stats.input_frame_rate = kInputFps;
  stats_proxy_->SetMockStats(stats);

  // Expect no scaling to begin with (preference: MAINTAIN_FRAMERATE).
  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  sink_.WaitForEncodedFrame(1);
  EXPECT_THAT(video_source_.sink_wants(), FpsMaxResolutionMax());

  // Trigger adapt down, expect scaled down resolution.
  video_stream_encoder_->TriggerQualityLow();
  video_source_.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  sink_.WaitForEncodedFrame(2);
  EXPECT_THAT(video_source_.sink_wants(),
              FpsMaxResolutionMatches(Lt(kWidth * kHeight)));

  // Enable MAINTAIN_RESOLUTION preference.
  test::FrameForwarder new_video_source;
  video_stream_encoder_->SetSourceAndWaitForRestrictionsUpdated(
      &new_video_source, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);
  // Give the encoder queue time to process the change in degradation preference
  // by waiting for an encoded frame.
  new_video_source.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  sink_.WaitForEncodedFrame(3);
  EXPECT_THAT(new_video_source.sink_wants(), FpsMaxResolutionMax());

  // Trigger adapt down, expect reduced framerate.
  video_stream_encoder_->TriggerQualityLow();
  new_video_source.IncomingCapturedFrame(CreateFrame(4, kWidth, kHeight));
  sink_.WaitForEncodedFrame(4);
  EXPECT_THAT(new_video_source.sink_wants(),
              FpsMatchesResolutionMax(Lt(kInputFps)));

  // Trigger adapt up, expect no restriction.
  video_stream_encoder_->TriggerQualityHigh();
  EXPECT_THAT(new_video_source.sink_wants(), FpsMaxResolutionMax());

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, DoesNotScaleBelowSetResolutionLimit) {
  const int kWidth = 1280;
  const int kHeight = 720;
  const size_t kNumFrames = 10;

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  // Enable MAINTAIN_FRAMERATE preference, no initial limitation.
  AdaptingFrameForwarder source(&time_controller_);
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(
      &source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);

  int64_t timestamp_ms = kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt down, expect scaled down resolution.
  video_stream_encoder_->TriggerCpuOveruse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(),
              FpsMaxResolutionMatches(Lt(kWidth * kHeight)));
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt up, expect no restriction.
  video_stream_encoder_->TriggerCpuUnderuse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt down, expect scaled down resolution.
  video_stream_encoder_->TriggerCpuOveruse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(),
              FpsMaxResolutionMatches(Lt(kWidth * kHeight)));
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  // Trigger adapt up, expect no restriction.
  video_stream_encoder_->TriggerCpuUnderuse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       AdaptsResolutionUpAndDownTwiceForLowQuality_BalancedMode_NoFpsLimit) {
  const int kWidth = 1280;
  const int kHeight = 720;
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  // Enable BALANCED preference, no initial limitation.
  AdaptingFrameForwarder source(&time_controller_);
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(&source,
                                   webrtc::DegradationPreference::BALANCED);

  int64_t timestamp_ms = kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution.
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(),
              FpsMaxResolutionMatches(Lt(kWidth * kHeight)));
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no restriction.
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution.
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(),
              FpsMaxResolutionMatches(Lt(kWidth * kHeight)));
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no restriction.
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  sink_.WaitForEncodedFrame(kWidth, kHeight);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, AdaptUpIfBwEstimateIsHigherThanMinBitrate) {
  fake_encoder_.SetResolutionBitrateLimits(
      {kEncoderBitrateLimits540p, kEncoderBitrateLimits720p});

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kEncoderBitrateLimits720p.min_start_bitrate_bps),
      DataRate::BitsPerSec(kEncoderBitrateLimits720p.min_start_bitrate_bps),
      DataRate::BitsPerSec(kEncoderBitrateLimits720p.min_start_bitrate_bps), 0,
      0, 0);

  // Enable MAINTAIN_FRAMERATE preference, no initial limitation.
  AdaptingFrameForwarder source(&time_controller_);
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(
      &source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);

  // Insert 720p frame.
  int64_t timestamp_ms = kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, 1280, 720));
  WaitForEncodedFrame(1280, 720);

  // Reduce bitrate and trigger adapt down.
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kEncoderBitrateLimits540p.min_start_bitrate_bps),
      DataRate::BitsPerSec(kEncoderBitrateLimits540p.min_start_bitrate_bps),
      DataRate::BitsPerSec(kEncoderBitrateLimits540p.min_start_bitrate_bps), 0,
      0, 0);
  video_stream_encoder_->TriggerQualityLow();

  // Insert 720p frame. It should be downscaled and encoded.
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, 1280, 720));
  WaitForEncodedFrame(960, 540);

  // Trigger adapt up. Higher resolution should not be requested duo to lack
  // of bitrate.
  video_stream_encoder_->TriggerQualityHigh();
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMatches(Lt(1280 * 720)));

  // Increase bitrate.
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kEncoderBitrateLimits720p.min_start_bitrate_bps),
      DataRate::BitsPerSec(kEncoderBitrateLimits720p.min_start_bitrate_bps),
      DataRate::BitsPerSec(kEncoderBitrateLimits720p.min_start_bitrate_bps), 0,
      0, 0);

  // Trigger adapt up. Higher resolution should be requested.
  video_stream_encoder_->TriggerQualityHigh();
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, DropFirstFramesIfBwEstimateIsTooLow) {
  fake_encoder_.SetResolutionBitrateLimits(
      {kEncoderBitrateLimits540p, kEncoderBitrateLimits720p});

  // Set bitrate equal to min bitrate of 540p.
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kEncoderBitrateLimits540p.min_start_bitrate_bps),
      DataRate::BitsPerSec(kEncoderBitrateLimits540p.min_start_bitrate_bps),
      DataRate::BitsPerSec(kEncoderBitrateLimits540p.min_start_bitrate_bps), 0,
      0, 0);

  // Enable MAINTAIN_FRAMERATE preference, no initial limitation.
  AdaptingFrameForwarder source(&time_controller_);
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(
      &source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);

  // Insert 720p frame. It should be dropped and lower resolution should be
  // requested.
  int64_t timestamp_ms = kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, 1280, 720));
  ExpectDroppedFrame();
  EXPECT_TRUE_WAIT(source.sink_wants().max_pixel_count < 1280 * 720, 5000);

  // Insert 720p frame. It should be downscaled and encoded.
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, 1280, 720));
  WaitForEncodedFrame(960, 540);

  video_stream_encoder_->Stop();
}

class BalancedDegradationTest : public VideoStreamEncoderTest {
 protected:
  void SetupTest() {
    // Reset encoder for field trials to take effect.
    ConfigureEncoder(video_encoder_config_.Copy());
    OnBitrateUpdated(kTargetBitrateBps);

    // Enable BALANCED preference.
    source_.set_adaptation_enabled(true);
    video_stream_encoder_->SetSource(&source_, DegradationPreference::BALANCED);
  }

  void OnBitrateUpdated(int bitrate_bps) {
    video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
        DataRate::BitsPerSec(bitrate_bps), DataRate::BitsPerSec(bitrate_bps),
        DataRate::BitsPerSec(bitrate_bps), 0, 0, 0);
  }

  void InsertFrame() {
    timestamp_ms_ += kFrameIntervalMs;
    source_.IncomingCapturedFrame(CreateFrame(timestamp_ms_, kWidth, kHeight));
  }

  void InsertFrameAndWaitForEncoded() {
    InsertFrame();
    sink_.WaitForEncodedFrame(timestamp_ms_);
  }

  const int kWidth = 640;  // pixels:640x360=230400
  const int kHeight = 360;
  const int64_t kFrameIntervalMs = 150;  // Use low fps to not drop any frame.
  int64_t timestamp_ms_ = 0;
  AdaptingFrameForwarder source_{&time_controller_};
};

TEST_F(BalancedDegradationTest, AdaptDownTwiceIfMinFpsDiffLtThreshold) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:57600|129600|230400,fps:7|10|24,fps_diff:1|1|1/");
  SetupTest();

  // Force input frame rate.
  const int kInputFps = 24;
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  stats.input_frame_rate = kInputFps;
  stats_proxy_->SetMockStats(stats);

  InsertFrameAndWaitForEncoded();
  EXPECT_THAT(source_.sink_wants(), FpsMaxResolutionMax());

  // Trigger adapt down, expect scaled down framerate and resolution,
  // since Fps diff (input-requested:0) < threshold.
  video_stream_encoder_->TriggerQualityLow();
  EXPECT_THAT(source_.sink_wants(),
              AllOf(WantsFps(Eq(24)), WantsMaxPixels(Le(230400))));

  video_stream_encoder_->Stop();
}

TEST_F(BalancedDegradationTest, AdaptDownOnceIfFpsDiffGeThreshold) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:57600|129600|230400,fps:7|10|24,fps_diff:1|1|1/");
  SetupTest();

  // Force input frame rate.
  const int kInputFps = 25;
  VideoSendStream::Stats stats = stats_proxy_->GetStats();
  stats.input_frame_rate = kInputFps;
  stats_proxy_->SetMockStats(stats);

  InsertFrameAndWaitForEncoded();
  EXPECT_THAT(source_.sink_wants(), FpsMaxResolutionMax());

  // Trigger adapt down, expect scaled down framerate only (640x360@24fps).
  // Fps diff (input-requested:1) == threshold.
  video_stream_encoder_->TriggerQualityLow();
  EXPECT_THAT(source_.sink_wants(), FpsMatchesResolutionMax(Eq(24)));

  video_stream_encoder_->Stop();
}

TEST_F(BalancedDegradationTest, AdaptDownUsesCodecSpecificFps) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:57600|129600|230400,fps:7|10|24,vp8_fps:8|11|22/");
  SetupTest();

  EXPECT_EQ(kVideoCodecVP8, video_encoder_config_.codec_type);

  InsertFrameAndWaitForEncoded();
  EXPECT_THAT(source_.sink_wants(), FpsMaxResolutionMax());

  // Trigger adapt down, expect scaled down framerate (640x360@22fps).
  video_stream_encoder_->TriggerQualityLow();
  EXPECT_THAT(source_.sink_wants(), FpsMatchesResolutionMax(Eq(22)));

  video_stream_encoder_->Stop();
}

TEST_F(BalancedDegradationTest, NoAdaptUpIfBwEstimateIsLessThanMinBitrate) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:57600|129600|230400,fps:7|10|14,kbps:0|0|425/");
  SetupTest();

  const int kMinBitrateBps = 425000;
  const int kTooLowMinBitrateBps = 424000;
  OnBitrateUpdated(kTooLowMinBitrateBps);

  InsertFrameAndWaitForEncoded();
  EXPECT_THAT(source_.sink_wants(), FpsMaxResolutionMax());
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down framerate (640x360@14fps).
  video_stream_encoder_->TriggerQualityLow();
  InsertFrameAndWaitForEncoded();
  EXPECT_THAT(source_.sink_wants(), FpsMatchesResolutionMax(Eq(14)));
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution (480x270@14fps).
  video_stream_encoder_->TriggerQualityLow();
  InsertFrameAndWaitForEncoded();
  EXPECT_THAT(source_.sink_wants(), FpsEqResolutionLt(source_.last_wants()));
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down framerate (480x270@10fps).
  video_stream_encoder_->TriggerQualityLow();
  InsertFrameAndWaitForEncoded();
  EXPECT_THAT(source_.sink_wants(), FpsLtResolutionEq(source_.last_wants()));
  EXPECT_EQ(source_.sink_wants().max_framerate_fps, 10);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no upscale in fps (target bitrate < min bitrate).
  video_stream_encoder_->TriggerQualityHigh();
  InsertFrameAndWaitForEncoded();
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect upscaled fps (target bitrate == min bitrate).
  OnBitrateUpdated(kMinBitrateBps);
  video_stream_encoder_->TriggerQualityHigh();
  InsertFrameAndWaitForEncoded();
  EXPECT_EQ(source_.sink_wants().max_framerate_fps, 14);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(BalancedDegradationTest,
       InitialFrameDropAdaptsFpsAndResolutionInOneStep) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:57600|129600|230400,fps:7|24|24/");
  SetupTest();
  OnBitrateUpdated(kLowTargetBitrateBps);

  EXPECT_THAT(source_.sink_wants(), UnlimitedSinkWants());

  // Insert frame, expect scaled down:
  // framerate (640x360@24fps) -> resolution (480x270@24fps).
  InsertFrame();
  EXPECT_FALSE(WaitForFrame(1000));
  EXPECT_LT(source_.sink_wants().max_pixel_count, kWidth * kHeight);
  EXPECT_EQ(source_.sink_wants().max_framerate_fps, 24);

  // Insert frame, expect scaled down:
  // resolution (320x180@24fps).
  InsertFrame();
  EXPECT_FALSE(WaitForFrame(1000));
  EXPECT_LT(source_.sink_wants().max_pixel_count,
            source_.last_wants().max_pixel_count);
  EXPECT_EQ(source_.sink_wants().max_framerate_fps, 24);

  // Frame should not be dropped (min pixels per frame reached).
  InsertFrameAndWaitForEncoded();

  video_stream_encoder_->Stop();
}

TEST_F(BalancedDegradationTest,
       NoAdaptUpInResolutionIfBwEstimateIsLessThanMinBitrate) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:57600|129600|230400,fps:7|10|14,kbps_res:0|0|435/");
  SetupTest();

  const int kResolutionMinBitrateBps = 435000;
  const int kTooLowMinResolutionBitrateBps = 434000;
  OnBitrateUpdated(kTooLowMinResolutionBitrateBps);

  InsertFrameAndWaitForEncoded();
  EXPECT_THAT(source_.sink_wants(), FpsMaxResolutionMax());
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down framerate (640x360@14fps).
  video_stream_encoder_->TriggerQualityLow();
  InsertFrameAndWaitForEncoded();
  EXPECT_THAT(source_.sink_wants(), FpsMatchesResolutionMax(Eq(14)));
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution (480x270@14fps).
  video_stream_encoder_->TriggerQualityLow();
  InsertFrameAndWaitForEncoded();
  EXPECT_THAT(source_.sink_wants(), FpsEqResolutionLt(source_.last_wants()));
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down framerate (480x270@10fps).
  video_stream_encoder_->TriggerQualityLow();
  InsertFrameAndWaitForEncoded();
  EXPECT_THAT(source_.sink_wants(), FpsLtResolutionEq(source_.last_wants()));
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect upscaled fps (no bitrate limit) (480x270@14fps).
  video_stream_encoder_->TriggerQualityHigh();
  InsertFrameAndWaitForEncoded();
  EXPECT_THAT(source_.sink_wants(), FpsGtResolutionEq(source_.last_wants()));
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no upscale in res (target bitrate < min bitrate).
  video_stream_encoder_->TriggerQualityHigh();
  InsertFrameAndWaitForEncoded();
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect upscaled res (target bitrate == min bitrate).
  OnBitrateUpdated(kResolutionMinBitrateBps);
  video_stream_encoder_->TriggerQualityHigh();
  InsertFrameAndWaitForEncoded();
  EXPECT_THAT(source_.sink_wants(), FpsEqResolutionGt(source_.last_wants()));
  EXPECT_EQ(5, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(BalancedDegradationTest,
       NoAdaptUpInFpsAndResolutionIfBwEstimateIsLessThanMinBitrate) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BalancedDegradationSettings/"
      "pixels:57600|129600|230400,fps:7|10|14,kbps:0|0|425,kbps_res:0|0|435/");
  SetupTest();

  const int kMinBitrateBps = 425000;
  const int kTooLowMinBitrateBps = 424000;
  const int kResolutionMinBitrateBps = 435000;
  const int kTooLowMinResolutionBitrateBps = 434000;
  OnBitrateUpdated(kTooLowMinBitrateBps);

  InsertFrameAndWaitForEncoded();
  EXPECT_THAT(source_.sink_wants(), FpsMaxResolutionMax());
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down framerate (640x360@14fps).
  video_stream_encoder_->TriggerQualityLow();
  InsertFrameAndWaitForEncoded();
  EXPECT_THAT(source_.sink_wants(), FpsMatchesResolutionMax(Eq(14)));
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution (480x270@14fps).
  video_stream_encoder_->TriggerQualityLow();
  InsertFrameAndWaitForEncoded();
  EXPECT_THAT(source_.sink_wants(), FpsEqResolutionLt(source_.last_wants()));
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down framerate (480x270@10fps).
  video_stream_encoder_->TriggerQualityLow();
  InsertFrameAndWaitForEncoded();
  EXPECT_THAT(source_.sink_wants(), FpsLtResolutionEq(source_.last_wants()));
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no upscale (target bitrate < min bitrate).
  video_stream_encoder_->TriggerQualityHigh();
  InsertFrameAndWaitForEncoded();
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect upscaled fps (target bitrate == min bitrate).
  OnBitrateUpdated(kMinBitrateBps);
  video_stream_encoder_->TriggerQualityHigh();
  InsertFrameAndWaitForEncoded();
  EXPECT_THAT(source_.sink_wants(), FpsGtResolutionEq(source_.last_wants()));
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no upscale in res (target bitrate < min bitrate).
  OnBitrateUpdated(kTooLowMinResolutionBitrateBps);
  video_stream_encoder_->TriggerQualityHigh();
  InsertFrameAndWaitForEncoded();
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect upscaled res (target bitrate == min bitrate).
  OnBitrateUpdated(kResolutionMinBitrateBps);
  video_stream_encoder_->TriggerQualityHigh();
  InsertFrameAndWaitForEncoded();
  EXPECT_THAT(source_.sink_wants(), FpsEqResolutionGt(source_.last_wants()));
  EXPECT_EQ(5, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       AdaptsResolutionOnOveruseAndLowQuality_MaintainFramerateMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  // Enable MAINTAIN_FRAMERATE preference, no initial limitation.
  AdaptingFrameForwarder source(&time_controller_);
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(
      &source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);

  int64_t timestamp_ms = kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt down, expect scaled down resolution (960x540).
  video_stream_encoder_->TriggerCpuOveruse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(),
              FpsMaxResolutionMatches(Lt(kWidth * kHeight)));
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt down, expect scaled down resolution (640x360).
  video_stream_encoder_->TriggerCpuOveruse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionLt(source.last_wants()));
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt down, expect scaled down resolution (480x270).
  video_stream_encoder_->TriggerCpuOveruse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionLt(source.last_wants()));
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality adapt down, expect scaled down resolution (320x180).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionLt(source.last_wants()));
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
  EXPECT_THAT(source.sink_wants(), FpsMax());
  EXPECT_EQ(source.sink_wants().max_pixel_count, last_wants.max_pixel_count);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality adapt up, expect upscaled resolution (480x270).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionGt(source.last_wants()));
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality and cpu adapt up since both are most limited, expect
  // upscaled resolution (640x360).
  video_stream_encoder_->TriggerCpuUnderuse();
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionGt(source.last_wants()));
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality and cpu adapt up since both are most limited, expect
  // upscaled resolution (960x540).
  video_stream_encoder_->TriggerCpuUnderuse();
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionGt(source.last_wants()));
  last_wants = source.sink_wants();
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(5, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt up, expect no change since not most limited (960x540).
  // However the stats will change since the CPU resource is no longer limited.
  video_stream_encoder_->TriggerCpuUnderuse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsEqResolutionEqTo(last_wants));
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(6, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality adapt up, expect no restriction (1280x720).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionGt(source.last_wants()));
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(6, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(5, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, CpuLimitedHistogramIsReported) {
  const int kWidth = 640;
  const int kHeight = 360;

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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

  EXPECT_METRIC_EQ(
      1, metrics::NumSamples("WebRTC.Video.CpuLimitedResolutionInPercent"));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.CpuLimitedResolutionInPercent", 50));
}

TEST_F(VideoStreamEncoderTest,
       CpuLimitedHistogramIsNotReportedForDisabledDegradation) {
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
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
      SimulcastRateAllocator(fake_encoder_.codec_config())
          .Allocate(VideoBitrateAllocationParameters(kLowTargetBitrateBps,
                                                     kDefaultFps));

  EXPECT_CALL(bitrate_observer, OnBitrateAllocationUpdated(expected_bitrate))
      .Times(1);
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kLowTargetBitrateBps),
      DataRate::BitsPerSec(kLowTargetBitrateBps),
      DataRate::BitsPerSec(kLowTargetBitrateBps), 0, 0, 0);

  video_source_.IncomingCapturedFrame(
      CreateFrame(CurrentTimeMs(), codec_width_, codec_height_));
  WaitForEncodedFrame(CurrentTimeMs());
  VideoBitrateAllocation bitrate_allocation =
      fake_encoder_.GetAndResetLastRateControlSettings()->bitrate;
  // Check that encoder has been updated too, not just allocation observer.
  EXPECT_EQ(bitrate_allocation.get_sum_bps(), kLowTargetBitrateBps);
  AdvanceTime(TimeDelta::Seconds(1) / kDefaultFps);

  // Not called on second frame.
  EXPECT_CALL(bitrate_observer, OnBitrateAllocationUpdated(expected_bitrate))
      .Times(0);
  video_source_.IncomingCapturedFrame(
      CreateFrame(CurrentTimeMs(), codec_width_, codec_height_));
  WaitForEncodedFrame(CurrentTimeMs());
  AdvanceTime(TimeDelta::Millis(1) / kDefaultFps);

  // Called after a process interval.
  EXPECT_CALL(bitrate_observer, OnBitrateAllocationUpdated(expected_bitrate))
      .Times(1);
  const int64_t start_time_ms = CurrentTimeMs();
  while (CurrentTimeMs() - start_time_ms < kProcessIntervalMs) {
    video_source_.IncomingCapturedFrame(
        CreateFrame(CurrentTimeMs(), codec_width_, codec_height_));
    WaitForEncodedFrame(CurrentTimeMs());
    AdvanceTime(TimeDelta::Millis(1) / kDefaultFps);
  }

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
                          kNumTemporalLayers, /*temporal_id*/ 0,
                          /*base_heavy_tl3_alloc*/ false);
  const int kTl1Bps = kTargetBitrateBps *
                      webrtc::SimulcastRateAllocator::GetTemporalRateAllocation(
                          kNumTemporalLayers, /*temporal_id*/ 1,
                          /*base_heavy_tl3_alloc*/ false);
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
      kS0Bps *
      webrtc::SimulcastRateAllocator::GetTemporalRateAllocation(
          /*num_layers*/ 2, /*temporal_id*/ 0, /*base_heavy_tl3_alloc*/ false);
  const int kS0Tl1Bps =
      kS0Bps *
      webrtc::SimulcastRateAllocator::GetTemporalRateAllocation(
          /*num_layers*/ 2, /*temporal_id*/ 1, /*base_heavy_tl3_alloc*/ false);
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

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
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
  video_stream_encoder_->TriggerCpuUnderuse();
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

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
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
  video_stream_encoder_->TriggerCpuUnderuse();
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

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
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
  video_stream_encoder_->SetSourceAndWaitForFramerateUpdated(
      &source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);
  EXPECT_EQ(
      video_stream_encoder_->overuse_detector_proxy_->GetLastTargetFramerate(),
      kFramerate);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, DropsFramesAndScalesWhenBitrateIsTooLow) {
  const int kTooLowBitrateForFrameSizeBps = 10000;
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTooLowBitrateForFrameSizeBps),
      DataRate::BitsPerSec(kTooLowBitrateForFrameSizeBps),
      DataRate::BitsPerSec(kTooLowBitrateForFrameSizeBps), 0, 0, 0);
  const int kWidth = 640;
  const int kHeight = 360;

  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));

  // Expect to drop this frame, the wait should time out.
  ExpectDroppedFrame();

  // Expect the sink_wants to specify a scaled frame.
  EXPECT_TRUE_WAIT(
      video_source_.sink_wants().max_pixel_count < kWidth * kHeight, 5000);

  int last_pixel_count = video_source_.sink_wants().max_pixel_count;

  // Next frame is scaled.
  video_source_.IncomingCapturedFrame(
      CreateFrame(2, kWidth * 3 / 4, kHeight * 3 / 4));

  // Expect to drop this frame, the wait should time out.
  ExpectDroppedFrame();

  EXPECT_TRUE_WAIT(
      video_source_.sink_wants().max_pixel_count < last_pixel_count, 5000);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       NumberOfDroppedFramesLimitedWhenBitrateIsTooLow) {
  const int kTooLowBitrateForFrameSizeBps = 10000;
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTooLowBitrateForFrameSizeBps),
      DataRate::BitsPerSec(kTooLowBitrateForFrameSizeBps),
      DataRate::BitsPerSec(kTooLowBitrateForFrameSizeBps), 0, 0, 0);
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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kLowTargetBitrateBps),
      DataRate::BitsPerSec(kLowTargetBitrateBps),
      DataRate::BitsPerSec(kLowTargetBitrateBps), 0, 0, 0);

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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kLowTargetBitrateBps),
      DataRate::BitsPerSec(kLowTargetBitrateBps),
      DataRate::BitsPerSec(kLowTargetBitrateBps), 0, 0, 0);

  // Force quality scaler reconfiguration by resetting the source.
  video_stream_encoder_->SetSource(&video_source_,
                                   webrtc::DegradationPreference::BALANCED);

  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  // Frame should not be dropped, even if it's too large.
  WaitForEncodedFrame(1);

  video_stream_encoder_->Stop();
  fake_encoder_.SetQualityScaling(true);
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

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  // Frame should not be dropped.
  WaitForEncodedFrame(1);

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kNotTooLowBitrateForFrameSizeBps),
      DataRate::BitsPerSec(kNotTooLowBitrateForFrameSizeBps),
      DataRate::BitsPerSec(kNotTooLowBitrateForFrameSizeBps), 0, 0, 0);
  video_source_.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  // Frame should not be dropped.
  WaitForEncodedFrame(2);

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTooLowBitrateForFrameSizeBps),
      DataRate::BitsPerSec(kTooLowBitrateForFrameSizeBps),
      DataRate::BitsPerSec(kTooLowBitrateForFrameSizeBps), 0, 0, 0);
  video_source_.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  // Expect to drop this frame, the wait should time out.
  ExpectDroppedFrame();

  // Expect the sink_wants to specify a scaled frame.
  EXPECT_TRUE_WAIT(
      video_source_.sink_wants().max_pixel_count < kWidth * kHeight, 5000);
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       InitialFrameDropNotReactivatedWhenBweDropsWhenScalingDisabled) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScalerSettings/"
      "initial_bitrate_interval_ms:1000,initial_bitrate_factor:0.2/");
  fake_encoder_.SetQualityScaling(false);
  ConfigureEncoder(video_encoder_config_.Copy());
  const int kNotTooLowBitrateForFrameSizeBps = kTargetBitrateBps * 0.2;
  const int kTooLowBitrateForFrameSizeBps = kTargetBitrateBps * 0.19;
  const int kWidth = 640;
  const int kHeight = 360;

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
  video_source_.IncomingCapturedFrame(CreateFrame(1, kWidth, kHeight));
  // Frame should not be dropped.
  WaitForEncodedFrame(1);

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kNotTooLowBitrateForFrameSizeBps),
      DataRate::BitsPerSec(kNotTooLowBitrateForFrameSizeBps),
      DataRate::BitsPerSec(kNotTooLowBitrateForFrameSizeBps), 0, 0, 0);
  video_source_.IncomingCapturedFrame(CreateFrame(2, kWidth, kHeight));
  // Frame should not be dropped.
  WaitForEncodedFrame(2);

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTooLowBitrateForFrameSizeBps),
      DataRate::BitsPerSec(kTooLowBitrateForFrameSizeBps),
      DataRate::BitsPerSec(kTooLowBitrateForFrameSizeBps), 0, 0, 0);
  video_source_.IncomingCapturedFrame(CreateFrame(3, kWidth, kHeight));
  // Not dropped since quality scaling is disabled.
  WaitForEncodedFrame(3);

  // Expect the sink_wants to specify a scaled frame.
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();
  EXPECT_THAT(video_source_.sink_wants(), ResolutionMax());

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, RampsUpInQualityWhenBwIsHigh) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityRampupSettings/min_pixels:1,min_duration_ms:2000/");

  // Reset encoder for field trials to take effect.
  VideoEncoderConfig config = video_encoder_config_.Copy();
  config.max_bitrate_bps = kTargetBitrateBps;
  DataRate max_bitrate = DataRate::BitsPerSec(config.max_bitrate_bps);
  ConfigureEncoder(std::move(config));
  fake_encoder_.SetQp(kQpLow);

  // Enable MAINTAIN_FRAMERATE preference.
  AdaptingFrameForwarder source(&time_controller_);
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(&source,
                                   DegradationPreference::MAINTAIN_FRAMERATE);

  // Start at low bitrate.
  const int kLowBitrateBps = 200000;
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kLowBitrateBps),
      DataRate::BitsPerSec(kLowBitrateBps),
      DataRate::BitsPerSec(kLowBitrateBps), 0, 0, 0);

  // Expect first frame to be dropped and resolution to be limited.
  const int kWidth = 1280;
  const int kHeight = 720;
  const int64_t kFrameIntervalMs = 100;
  int64_t timestamp_ms = kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  ExpectDroppedFrame();
  EXPECT_TRUE_WAIT(source.sink_wants().max_pixel_count < kWidth * kHeight,
                   5000);

  // Increase bitrate to encoder max.
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      max_bitrate, max_bitrate, max_bitrate, 0, 0, 0);

  // Insert frames and advance |min_duration_ms|.
  for (size_t i = 1; i <= 10; i++) {
    timestamp_ms += kFrameIntervalMs;
    source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
    WaitForEncodedFrame(timestamp_ms);
  }
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_LT(source.sink_wants().max_pixel_count, kWidth * kHeight);

  AdvanceTime(TimeDelta::Millis(2000));

  // Insert frame should trigger high BW and release quality limitation.
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  // The ramp-up code involves the adaptation queue, give it time to execute.
  // TODO(hbos): Can we await an appropriate event instead?
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());

  // Frame should not be adapted.
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       QualityScalerAdaptationsRemovedWhenQualityScalingDisabled) {
  AdaptingFrameForwarder source(&time_controller_);
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(&source,
                                   DegradationPreference::MAINTAIN_FRAMERATE);
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
  fake_encoder_.SetQp(kQpHigh + 1);
  const int kWidth = 1280;
  const int kHeight = 720;
  const int64_t kFrameIntervalMs = 100;
  int64_t timestamp_ms = kFrameIntervalMs;
  for (size_t i = 1; i <= 100; i++) {
    timestamp_ms += kFrameIntervalMs;
    source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
    WaitForEncodedFrame(timestamp_ms);
  }
  // Wait for QualityScaler, which will wait for 2000*2.5 ms until checking QP
  // for the first time.
  // TODO(eshr): We should avoid these waits by using threads with simulated
  // time.
  EXPECT_TRUE_WAIT(stats_proxy_->GetStats().bw_limited_resolution,
                   2000 * 2.5 * 2);
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();
  EXPECT_THAT(source.sink_wants(), WantsMaxPixels(Lt(kWidth * kHeight)));
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);

  // Disable Quality scaling by turning off scaler on the encoder and
  // reconfiguring.
  fake_encoder_.SetQualityScaling(false);
  video_stream_encoder_->ConfigureEncoder(video_encoder_config_.Copy(),
                                          kMaxPayloadLength);
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();
  AdvanceTime(TimeDelta::Millis(0));
  // Since we turned off the quality scaler, the adaptations made by it are
  // removed.
  EXPECT_THAT(source.sink_wants(), ResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       ResolutionNotAdaptedForTooSmallFrame_MaintainFramerateMode) {
  const int kTooSmallWidth = 10;
  const int kTooSmallHeight = 10;
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  // Enable MAINTAIN_FRAMERATE preference, no initial limitation.
  test::FrameForwarder source;
  video_stream_encoder_->SetSource(
      &source, webrtc::DegradationPreference::MAINTAIN_FRAMERATE);
  EXPECT_THAT(source.sink_wants(), UnlimitedSinkWants());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);

  // Trigger adapt down, too small frame, expect no change.
  source.IncomingCapturedFrame(CreateFrame(1, kTooSmallWidth, kTooSmallHeight));
  WaitForEncodedFrame(1);
  video_stream_encoder_->TriggerCpuOveruse();
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_cpu_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       ResolutionNotAdaptedForTooSmallFrame_BalancedMode) {
  const int kTooSmallWidth = 10;
  const int kTooSmallHeight = 10;
  const int kFpsLimit = 7;
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  // Enable BALANCED preference, no initial limitation.
  test::FrameForwarder source;
  video_stream_encoder_->SetSource(&source,
                                   webrtc::DegradationPreference::BALANCED);
  EXPECT_THAT(source.sink_wants(), UnlimitedSinkWants());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);

  // Trigger adapt down, expect limited framerate.
  source.IncomingCapturedFrame(CreateFrame(1, kTooSmallWidth, kTooSmallHeight));
  WaitForEncodedFrame(1);
  video_stream_encoder_->TriggerQualityLow();
  EXPECT_THAT(source.sink_wants(), FpsMatchesResolutionMax(Eq(kFpsLimit)));
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, too small frame, expect no change.
  source.IncomingCapturedFrame(CreateFrame(2, kTooSmallWidth, kTooSmallHeight));
  WaitForEncodedFrame(2);
  video_stream_encoder_->TriggerQualityLow();
  EXPECT_THAT(source.sink_wants(), FpsMatchesResolutionMax(Eq(kFpsLimit)));
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, FailingInitEncodeDoesntCauseCrash) {
  fake_encoder_.ForceInitEncodeFailure(true);
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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
  video_stream_encoder_->TriggerCpuUnderuse();
  video_source_.IncomingCapturedFrame(
      CreateFrame(3 * kFrameIntervalMs, kFrameWidth, kFrameHeight));
  WaitForEncodedFrame(kFrameWidth, kFrameHeight);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       AdaptsFramerateOnOveruse_MaintainResolutionMode) {
  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
  video_stream_encoder_->SetSource(
      &video_source_, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);
  video_source_.set_adaptation_enabled(true);

  int64_t timestamp_ms = CurrentTimeMs();

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
  video_stream_encoder_->TriggerCpuUnderuse();
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
  video_stream_encoder_->TriggerCpuUnderuse();
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

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
  video_stream_encoder_->SetSource(
      &video_source_, webrtc::DegradationPreference::MAINTAIN_RESOLUTION);
  video_source_.set_adaptation_enabled(true);

  int64_t timestamp_ms = CurrentTimeMs();

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
      AdvanceTime(TimeDelta::Millis(kFrameIntervalMs));
    }
    // ...and then try to adapt again.
    video_stream_encoder_->TriggerCpuOveruse();
  } while (video_source_.sink_wants().max_framerate_fps <
           last_wants.max_framerate_fps);

  EXPECT_THAT(video_source_.sink_wants(),
              FpsMatchesResolutionMax(Eq(kMinFramerateFps)));

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       AdaptsResolutionAndFramerateForLowQuality_BalancedMode) {
  const int kWidth = 1280;
  const int kHeight = 720;
  const int64_t kFrameIntervalMs = 150;
  int64_t timestamp_ms = kFrameIntervalMs;
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  // Enable BALANCED preference, no initial limitation.
  AdaptingFrameForwarder source(&time_controller_);
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(&source,
                                   webrtc::DegradationPreference::BALANCED);
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(0, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution (960x540@30fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(),
              FpsMaxResolutionMatches(Lt(kWidth * kHeight)));
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution (640x360@30fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionLt(source.last_wants()));
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect reduced fps (640x360@15fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsLtResolutionEq(source.last_wants()));
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution (480x270@15fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsEqResolutionLt(source.last_wants()));
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Restrict bitrate, trigger adapt down, expect reduced fps (480x270@10fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsLtResolutionEq(source.last_wants()));
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(5, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect scaled down resolution (320x180@10fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsEqResolutionLt(source.last_wants()));
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(6, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, expect reduced fps (320x180@7fps).
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsLtResolutionEq(source.last_wants()));
  rtc::VideoSinkWants last_wants = source.sink_wants();
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(7, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt down, min resolution reached, expect no change.
  video_stream_encoder_->TriggerQualityLow();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsEqResolutionEqTo(last_wants));
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(7, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect expect increased fps (320x180@10fps).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsGtResolutionEq(source.last_wants()));
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(8, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect upscaled resolution (480x270@10fps).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsEqResolutionGt(source.last_wants()));
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(9, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Increase bitrate, trigger adapt up, expect increased fps (480x270@15fps).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsGtResolutionEq(source.last_wants()));
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(10, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect upscaled resolution (640x360@15fps).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsEqResolutionGt(source.last_wants()));
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(11, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect increased fps (640x360@30fps).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsMax());
  EXPECT_EQ(source.sink_wants().max_pixel_count,
            source.last_wants().max_pixel_count);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(12, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect upscaled resolution (960x540@30fps).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionGt(source.last_wants()));
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(13, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no restriction (1280x720fps@30fps).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionGt(source.last_wants()));
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_EQ(14, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no change.
  video_stream_encoder_->TriggerQualityHigh();
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_EQ(14, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, AdaptWithTwoReasonsAndDifferentOrder_Framerate) {
  const int kWidth = 1280;
  const int kHeight = 720;
  const int64_t kFrameIntervalMs = 150;
  int64_t timestamp_ms = kFrameIntervalMs;
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  // Enable BALANCED preference, no initial limitation.
  AdaptingFrameForwarder source(&time_controller_);
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(&source,
                                   webrtc::DegradationPreference::BALANCED);
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
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
  EXPECT_THAT(source.sink_wants(),
              FpsMaxResolutionMatches(Lt(kWidth * kHeight)));
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
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionLt(source.last_wants()));
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
  EXPECT_THAT(source.sink_wants(), FpsLtResolutionEq(source.last_wants()));
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt up, expect no change since QP is most limited.
  {
    // Store current sink wants since we expect no change and if there is no
    // change then last_wants() is not updated.
    auto previous_sink_wants = source.sink_wants();
    video_stream_encoder_->TriggerCpuUnderuse();
    timestamp_ms += kFrameIntervalMs;
    source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
    WaitForEncodedFrame(timestamp_ms);
    EXPECT_THAT(source.sink_wants(), FpsEqResolutionEqTo(previous_sink_wants));
    EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
    EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);
  }

  // Trigger quality adapt up, expect increased fps (640x360@30fps).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsGtResolutionEq(source.last_wants()));
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality adapt up and Cpu adapt up since both are most limited,
  // expect increased resolution (960x540@30fps).
  video_stream_encoder_->TriggerQualityHigh();
  video_stream_encoder_->TriggerCpuUnderuse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionGt(source.last_wants()));
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality adapt up and Cpu adapt up since both are most limited,
  // expect no restriction (1280x720fps@30fps).
  video_stream_encoder_->TriggerQualityHigh();
  video_stream_encoder_->TriggerCpuUnderuse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionGt(source.last_wants()));
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no change.
  video_stream_encoder_->TriggerQualityHigh();
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(4, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       AdaptWithTwoReasonsAndDifferentOrder_Resolution) {
  const int kWidth = 640;
  const int kHeight = 360;
  const int kFpsLimit = 15;
  const int64_t kFrameIntervalMs = 150;
  int64_t timestamp_ms = kFrameIntervalMs;
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  // Enable BALANCED preference, no initial limitation.
  AdaptingFrameForwarder source(&time_controller_);
  source.set_adaptation_enabled(true);
  video_stream_encoder_->SetSource(&source,
                                   webrtc::DegradationPreference::BALANCED);
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(kWidth, kHeight);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
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
  EXPECT_THAT(source.sink_wants(), FpsMatchesResolutionMax(Eq(kFpsLimit)));
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
  EXPECT_THAT(source.sink_wants(), FpsEqResolutionLt(source.last_wants()));
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger cpu adapt up, expect no change because quality is most limited.
  {
    auto previous_sink_wants = source.sink_wants();
    // Store current sink wants since we expect no change ind if there is no
    // change then last__wants() is not updated.
    video_stream_encoder_->TriggerCpuUnderuse();
    timestamp_ms += kFrameIntervalMs;
    source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
    WaitForEncodedFrame(timestamp_ms);
    EXPECT_THAT(source.sink_wants(), FpsEqResolutionEqTo(previous_sink_wants));
    EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
    EXPECT_EQ(1, stats_proxy_->GetStats().number_of_quality_adapt_changes);
  }

  // Trigger quality adapt up, expect upscaled resolution (640x360@15fps).
  video_stream_encoder_->TriggerQualityHigh();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsEqResolutionGt(source.last_wants()));
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_TRUE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(1, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger quality and cpu adapt up, expect increased fps (640x360@30fps).
  video_stream_encoder_->TriggerQualityHigh();
  video_stream_encoder_->TriggerCpuUnderuse();
  timestamp_ms += kFrameIntervalMs;
  source.IncomingCapturedFrame(CreateFrame(timestamp_ms, kWidth, kHeight));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_FALSE(stats_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_quality_adapt_changes);

  // Trigger adapt up, expect no change.
  video_stream_encoder_->TriggerQualityHigh();
  EXPECT_THAT(source.sink_wants(), FpsMaxResolutionMax());
  EXPECT_EQ(2, stats_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(3, stats_proxy_->GetStats().number_of_quality_adapt_changes);

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

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
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

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  int64_t timestamp_ms = CurrentTimeMs();
  max_framerate_ = kLowFps;

  // Insert 2 seconds of 2fps video.
  for (int i = 0; i < kLowFps * 2; ++i) {
    video_source_.IncomingCapturedFrame(
        CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
    WaitForEncodedFrame(timestamp_ms);
    timestamp_ms += 1000 / kLowFps;
  }

  // Make sure encoder is updated with new target.
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
  video_source_.IncomingCapturedFrame(
      CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
  WaitForEncodedFrame(timestamp_ms);
  timestamp_ms += 1000 / kLowFps;

  EXPECT_EQ(kLowFps, fake_encoder_.GetConfiguredInputFramerate());

  // Insert 30fps frames for just a little more than the forced update period.
  const int kVcmTimerIntervalFrames = (kProcessIntervalMs * kHighFps) / 1000;
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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();

  // Insert a first video frame, causes another bitrate update.
  int64_t timestamp_ms = CurrentTimeMs();
  EXPECT_CALL(bitrate_observer, OnBitrateAllocationUpdated(_)).Times(1);
  video_source_.IncomingCapturedFrame(
      CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight));
  WaitForEncodedFrame(timestamp_ms);

  // Next, simulate video suspension due to pacer queue overrun.
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(0), DataRate::BitsPerSec(0), DataRate::BitsPerSec(0),
      0, 1, 0);

  // Skip ahead until a new periodic parameter update should have occured.
  timestamp_ms += kProcessIntervalMs;
  AdvanceTime(TimeDelta::Millis(kProcessIntervalMs));

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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
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

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
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

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  int64_t timestamp_ms = CurrentTimeMs();
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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps + 1000),
      DataRate::BitsPerSec(kTargetBitrateBps + 1000),
      DataRate::BitsPerSec(kTargetBitrateBps + 1000), 0, 0, 0);
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

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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

  int64_t timestamp_ms = CurrentTimeMs();
  max_framerate_ = kActualInputFps;
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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

TEST_F(VideoStreamEncoderBlockedTest, AccumulatesUpdateRectOnDroppedFrames) {
  VideoFrame::UpdateRect rect;
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kSimulcastTargetBitrateBps),
      DataRate::BitsPerSec(kSimulcastTargetBitrateBps),
      DataRate::BitsPerSec(kSimulcastTargetBitrateBps), 0, 0, 0);
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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

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
            CurrentTimeMs() - kEncodeFinishDelayMs);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, DoesNotRewriteH264BitstreamWithOptimalSps) {
  // SPS contains VUI with restrictions on the maximum number of reordered
  // pictures, there is no need to rewrite the bitstream to enable faster
  // decoding.
  ResetEncoder("H264", 1, 1, 1, false);

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();

  fake_encoder_.SetEncodedImageData(
      EncodedImageBuffer::Create(optimal_sps, sizeof(optimal_sps)));

  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  WaitForEncodedFrame(1);

  EXPECT_THAT(sink_.GetLastEncodedImageData(),
              testing::ElementsAreArray(optimal_sps));

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, RewritesH264BitstreamWithNonOptimalSps) {
  // SPS does not contain VUI, the bitstream is will be rewritten with added
  // VUI with restrictions on the maximum number of reordered pictures to
  // enable faster decoding.
  uint8_t original_sps[] = {0,    0,    0,    1,    H264::NaluType::kSps,
                            0x00, 0x00, 0x03, 0x03, 0xF4,
                            0x05, 0x03, 0xC7, 0xC0};
  ResetEncoder("H264", 1, 1, 1, false);

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();

  fake_encoder_.SetEncodedImageData(
      EncodedImageBuffer::Create(original_sps, sizeof(original_sps)));

  video_source_.IncomingCapturedFrame(CreateFrame(1, nullptr));
  WaitForEncodedFrame(1);

  EXPECT_THAT(sink_.GetLastEncodedImageData(),
              testing::ElementsAreArray(optimal_sps));

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, CopiesVideoFrameMetadataAfterDownscale) {
  const int kFrameWidth = 1280;
  const int kFrameHeight = 720;
  const int kTargetBitrateBps = 300000;  // To low for HD resolution.

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();

  // Insert a first video frame. It should be dropped because of downscale in
  // resolution.
  int64_t timestamp_ms = CurrentTimeMs();
  VideoFrame frame = CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight);
  frame.set_rotation(kVideoRotation_270);
  video_source_.IncomingCapturedFrame(frame);

  ExpectDroppedFrame();

  // Second frame is downscaled.
  timestamp_ms = CurrentTimeMs();
  frame = CreateFrame(timestamp_ms, kFrameWidth / 2, kFrameHeight / 2);
  frame.set_rotation(kVideoRotation_90);
  video_source_.IncomingCapturedFrame(frame);

  WaitForEncodedFrame(timestamp_ms);
  sink_.CheckLastFrameRotationMatches(kVideoRotation_90);

  // Insert another frame, also downscaled.
  timestamp_ms = CurrentTimeMs();
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
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      /*target_bitrate=*/DataRate::KilobitsPerSec(300),
      /*stable_target_bitrate=*/DataRate::KilobitsPerSec(300),
      /*link_allocation=*/DataRate::KilobitsPerSec(300),
      /*fraction_lost=*/0,
      /*rtt_ms=*/0,
      /*cwnd_reduce_ratio=*/0);

  // Insert a first video frame so that encoder gets configured.
  int64_t timestamp_ms = CurrentTimeMs();
  VideoFrame frame = CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight);
  frame.set_rotation(kVideoRotation_270);
  video_source_.IncomingCapturedFrame(frame);
  WaitForEncodedFrame(timestamp_ms);

  // Set a target rate below the minimum allowed by the codec settings.
  VideoCodec codec_config = fake_encoder_.codec_config();
  DataRate min_rate = DataRate::KilobitsPerSec(codec_config.minBitrate);
  DataRate target_rate = min_rate - DataRate::KilobitsPerSec(1);
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      /*target_bitrate=*/target_rate,
      /*stable_target_bitrate=*/target_rate,
      /*link_allocation=*/target_rate,
      /*fraction_lost=*/0,
      /*rtt_ms=*/0,
      /*cwnd_reduce_ratio=*/0);
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();

  // Target bitrate and bandwidth allocation should both be capped at min_rate.
  auto rate_settings = fake_encoder_.GetAndResetLastRateControlSettings();
  ASSERT_TRUE(rate_settings.has_value());
  DataRate allocation_sum =
      DataRate::BitsPerSec(rate_settings->bitrate.get_sum_bps());
  EXPECT_EQ(min_rate, allocation_sum);
  EXPECT_EQ(rate_settings->bandwidth_allocation, min_rate);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, EncoderRatesPropagatedOnReconfigure) {
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
  // Capture a frame and wait for it to synchronize with the encoder thread.
  int64_t timestamp_ms = CurrentTimeMs();
  video_source_.IncomingCapturedFrame(CreateFrame(timestamp_ms, nullptr));
  WaitForEncodedFrame(1);

  auto prev_rate_settings = fake_encoder_.GetAndResetLastRateControlSettings();
  ASSERT_TRUE(prev_rate_settings.has_value());
  EXPECT_EQ(static_cast<int>(prev_rate_settings->framerate_fps),
            kDefaultFramerate);

  // Send 1s of video to ensure the framerate is stable at kDefaultFramerate.
  for (int i = 0; i < 2 * kDefaultFramerate; i++) {
    timestamp_ms += 1000 / kDefaultFramerate;
    video_source_.IncomingCapturedFrame(CreateFrame(timestamp_ms, nullptr));
    WaitForEncodedFrame(timestamp_ms);
  }
  EXPECT_EQ(static_cast<int>(fake_encoder_.GetLastFramerate()),
            kDefaultFramerate);
  // Capture larger frame to trigger a reconfigure.
  codec_height_ *= 2;
  codec_width_ *= 2;
  timestamp_ms += 1000 / kDefaultFramerate;
  video_source_.IncomingCapturedFrame(CreateFrame(timestamp_ms, nullptr));
  WaitForEncodedFrame(timestamp_ms);

  EXPECT_EQ(2, sink_.number_of_reconfigurations());
  auto current_rate_settings =
      fake_encoder_.GetAndResetLastRateControlSettings();
  // Ensure we have actually reconfigured twice
  // The rate settings should have been set again even though
  // they haven't changed.
  ASSERT_TRUE(current_rate_settings.has_value());
  EXPECT_EQ(prev_rate_settings, current_rate_settings);

  video_stream_encoder_->Stop();
}

struct MockEncoderSwitchRequestCallback : public EncoderSwitchRequestCallback {
  MOCK_METHOD(void, RequestEncoderFallback, (), (override));
  MOCK_METHOD(void, RequestEncoderSwitch, (const Config& conf), (override));
  MOCK_METHOD(void,
              RequestEncoderSwitch,
              (const webrtc::SdpVideoFormat& format),
              (override));
};

TEST_F(VideoStreamEncoderTest, BitrateEncoderSwitch) {
  constexpr int kDontCare = 100;

  StrictMock<MockEncoderSwitchRequestCallback> switch_callback;
  video_send_config_.encoder_settings.encoder_switch_request_callback =
      &switch_callback;
  VideoEncoderConfig encoder_config = video_encoder_config_.Copy();
  encoder_config.codec_type = kVideoCodecVP8;
  webrtc::test::ScopedFieldTrials field_trial(
      "WebRTC-NetworkCondition-EncoderSwitch/"
      "codec_thresholds:VP8;100;-1|H264;-1;30000,"
      "to_codec:AV1,to_param:ping,to_value:pong,window:2.0/");

  // Reset encoder for new configuration to take effect.
  ConfigureEncoder(std::move(encoder_config));

  // Send one frame to trigger ReconfigureEncoder.
  video_source_.IncomingCapturedFrame(
      CreateFrame(kDontCare, kDontCare, kDontCare));

  using Config = EncoderSwitchRequestCallback::Config;
  EXPECT_CALL(switch_callback, RequestEncoderSwitch(Matcher<const Config&>(
                                   AllOf(Field(&Config::codec_name, "AV1"),
                                         Field(&Config::param, "ping"),
                                         Field(&Config::value, "pong")))));

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      /*target_bitrate=*/DataRate::KilobitsPerSec(50),
      /*stable_target_bitrate=*/DataRate::KilobitsPerSec(kDontCare),
      /*link_allocation=*/DataRate::KilobitsPerSec(kDontCare),
      /*fraction_lost=*/0,
      /*rtt_ms=*/0,
      /*cwnd_reduce_ratio=*/0);
  AdvanceTime(TimeDelta::Millis(0));

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, VideoSuspendedNoEncoderSwitch) {
  constexpr int kDontCare = 100;

  StrictMock<MockEncoderSwitchRequestCallback> switch_callback;
  video_send_config_.encoder_settings.encoder_switch_request_callback =
      &switch_callback;
  VideoEncoderConfig encoder_config = video_encoder_config_.Copy();
  encoder_config.codec_type = kVideoCodecVP8;
  webrtc::test::ScopedFieldTrials field_trial(
      "WebRTC-NetworkCondition-EncoderSwitch/"
      "codec_thresholds:VP8;100;-1|H264;-1;30000,"
      "to_codec:AV1,to_param:ping,to_value:pong,window:2.0/");

  // Reset encoder for new configuration to take effect.
  ConfigureEncoder(std::move(encoder_config));

  // Send one frame to trigger ReconfigureEncoder.
  video_source_.IncomingCapturedFrame(
      CreateFrame(kDontCare, kDontCare, kDontCare));

  using Config = EncoderSwitchRequestCallback::Config;
  EXPECT_CALL(switch_callback, RequestEncoderSwitch(Matcher<const Config&>(_)))
      .Times(0);

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      /*target_bitrate=*/DataRate::KilobitsPerSec(0),
      /*stable_target_bitrate=*/DataRate::KilobitsPerSec(0),
      /*link_allocation=*/DataRate::KilobitsPerSec(kDontCare),
      /*fraction_lost=*/0,
      /*rtt_ms=*/0,
      /*cwnd_reduce_ratio=*/0);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, ResolutionEncoderSwitch) {
  constexpr int kSufficientBitrateToNotDrop = 1000;
  constexpr int kHighRes = 500;
  constexpr int kLowRes = 100;

  StrictMock<MockEncoderSwitchRequestCallback> switch_callback;
  video_send_config_.encoder_settings.encoder_switch_request_callback =
      &switch_callback;
  webrtc::test::ScopedFieldTrials field_trial(
      "WebRTC-NetworkCondition-EncoderSwitch/"
      "codec_thresholds:VP8;120;-1|H264;-1;30000,"
      "to_codec:AV1,to_param:ping,to_value:pong,window:2.0/");
  VideoEncoderConfig encoder_config = video_encoder_config_.Copy();
  encoder_config.codec_type = kVideoCodecH264;

  // Reset encoder for new configuration to take effect.
  ConfigureEncoder(std::move(encoder_config));

  // The VideoStreamEncoder needs some bitrate before it can start encoding,
  // setting some bitrate so that subsequent calls to WaitForEncodedFrame does
  // not fail.
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      /*target_bitrate=*/DataRate::KilobitsPerSec(kSufficientBitrateToNotDrop),
      /*stable_target_bitrate=*/
      DataRate::KilobitsPerSec(kSufficientBitrateToNotDrop),
      /*link_allocation=*/DataRate::KilobitsPerSec(kSufficientBitrateToNotDrop),
      /*fraction_lost=*/0,
      /*rtt_ms=*/0,
      /*cwnd_reduce_ratio=*/0);

  // Send one frame to trigger ReconfigureEncoder.
  video_source_.IncomingCapturedFrame(CreateFrame(1, kHighRes, kHighRes));
  WaitForEncodedFrame(1);

  using Config = EncoderSwitchRequestCallback::Config;
  EXPECT_CALL(switch_callback, RequestEncoderSwitch(Matcher<const Config&>(
                                   AllOf(Field(&Config::codec_name, "AV1"),
                                         Field(&Config::param, "ping"),
                                         Field(&Config::value, "pong")))));

  video_source_.IncomingCapturedFrame(CreateFrame(2, kLowRes, kLowRes));
  WaitForEncodedFrame(2);

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, EncoderSelectorCurrentEncoderIsSignaled) {
  constexpr int kDontCare = 100;
  StrictMock<MockEncoderSelector> encoder_selector;
  auto encoder_factory = std::make_unique<test::VideoEncoderProxyFactory>(
      &fake_encoder_, &encoder_selector);
  video_send_config_.encoder_settings.encoder_factory = encoder_factory.get();

  // Reset encoder for new configuration to take effect.
  ConfigureEncoder(video_encoder_config_.Copy());

  EXPECT_CALL(encoder_selector, OnCurrentEncoder(_));

  video_source_.IncomingCapturedFrame(
      CreateFrame(kDontCare, kDontCare, kDontCare));
  video_stream_encoder_->Stop();

  // The encoders produces by the VideoEncoderProxyFactory have a pointer back
  // to it's factory, so in order for the encoder instance in the
  // |video_stream_encoder_| to be destroyed before the |encoder_factory| we
  // reset the |video_stream_encoder_| here.
  video_stream_encoder_.reset();
}

TEST_F(VideoStreamEncoderTest, EncoderSelectorBitrateSwitch) {
  constexpr int kDontCare = 100;

  NiceMock<MockEncoderSelector> encoder_selector;
  StrictMock<MockEncoderSwitchRequestCallback> switch_callback;
  video_send_config_.encoder_settings.encoder_switch_request_callback =
      &switch_callback;
  auto encoder_factory = std::make_unique<test::VideoEncoderProxyFactory>(
      &fake_encoder_, &encoder_selector);
  video_send_config_.encoder_settings.encoder_factory = encoder_factory.get();

  // Reset encoder for new configuration to take effect.
  ConfigureEncoder(video_encoder_config_.Copy());

  ON_CALL(encoder_selector, OnAvailableBitrate(_))
      .WillByDefault(Return(SdpVideoFormat("AV1")));
  EXPECT_CALL(switch_callback,
              RequestEncoderSwitch(Matcher<const SdpVideoFormat&>(
                  Field(&SdpVideoFormat::name, "AV1"))));

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      /*target_bitrate=*/DataRate::KilobitsPerSec(50),
      /*stable_target_bitrate=*/DataRate::KilobitsPerSec(kDontCare),
      /*link_allocation=*/DataRate::KilobitsPerSec(kDontCare),
      /*fraction_lost=*/0,
      /*rtt_ms=*/0,
      /*cwnd_reduce_ratio=*/0);
  AdvanceTime(TimeDelta::Millis(0));

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, EncoderSelectorBrokenEncoderSwitch) {
  constexpr int kSufficientBitrateToNotDrop = 1000;
  constexpr int kDontCare = 100;

  NiceMock<MockVideoEncoder> video_encoder;
  NiceMock<MockEncoderSelector> encoder_selector;
  StrictMock<MockEncoderSwitchRequestCallback> switch_callback;
  video_send_config_.encoder_settings.encoder_switch_request_callback =
      &switch_callback;
  auto encoder_factory = std::make_unique<test::VideoEncoderProxyFactory>(
      &video_encoder, &encoder_selector);
  video_send_config_.encoder_settings.encoder_factory = encoder_factory.get();

  // Reset encoder for new configuration to take effect.
  ConfigureEncoder(video_encoder_config_.Copy());

  // The VideoStreamEncoder needs some bitrate before it can start encoding,
  // setting some bitrate so that subsequent calls to WaitForEncodedFrame does
  // not fail.
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      /*target_bitrate=*/DataRate::KilobitsPerSec(kSufficientBitrateToNotDrop),
      /*stable_target_bitrate=*/
      DataRate::KilobitsPerSec(kSufficientBitrateToNotDrop),
      /*link_allocation=*/DataRate::KilobitsPerSec(kSufficientBitrateToNotDrop),
      /*fraction_lost=*/0,
      /*rtt_ms=*/0,
      /*cwnd_reduce_ratio=*/0);

  ON_CALL(video_encoder, Encode(_, _))
      .WillByDefault(Return(WEBRTC_VIDEO_CODEC_ENCODER_FAILURE));
  ON_CALL(encoder_selector, OnEncoderBroken())
      .WillByDefault(Return(SdpVideoFormat("AV2")));

  rtc::Event encode_attempted;
  EXPECT_CALL(switch_callback,
              RequestEncoderSwitch(Matcher<const SdpVideoFormat&>(_)))
      .WillOnce([&encode_attempted](const SdpVideoFormat& format) {
        EXPECT_EQ(format.name, "AV2");
        encode_attempted.Set();
      });

  video_source_.IncomingCapturedFrame(CreateFrame(1, kDontCare, kDontCare));
  encode_attempted.Wait(3000);

  AdvanceTime(TimeDelta::Millis(0));

  video_stream_encoder_->Stop();

  // The encoders produces by the VideoEncoderProxyFactory have a pointer back
  // to it's factory, so in order for the encoder instance in the
  // |video_stream_encoder_| to be destroyed before the |encoder_factory| we
  // reset the |video_stream_encoder_| here.
  video_stream_encoder_.reset();
}

TEST_F(VideoStreamEncoderTest,
       AllocationPropagatedToEncoderWhenTargetRateChanged) {
  const int kFrameWidth = 320;
  const int kFrameHeight = 180;

  // Set initial rate.
  auto rate = DataRate::KilobitsPerSec(100);
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      /*target_bitrate=*/rate,
      /*stable_target_bitrate=*/rate,
      /*link_allocation=*/rate,
      /*fraction_lost=*/0,
      /*rtt_ms=*/0,
      /*cwnd_reduce_ratio=*/0);

  // Insert a first video frame so that encoder gets configured.
  int64_t timestamp_ms = CurrentTimeMs();
  VideoFrame frame = CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight);
  frame.set_rotation(kVideoRotation_270);
  video_source_.IncomingCapturedFrame(frame);
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_EQ(1, fake_encoder_.GetNumSetRates());

  // Change of target bitrate propagates to the encoder.
  auto new_stable_rate = rate - DataRate::KilobitsPerSec(5);
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      /*target_bitrate=*/new_stable_rate,
      /*stable_target_bitrate=*/new_stable_rate,
      /*link_allocation=*/rate,
      /*fraction_lost=*/0,
      /*rtt_ms=*/0,
      /*cwnd_reduce_ratio=*/0);
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();
  EXPECT_EQ(2, fake_encoder_.GetNumSetRates());
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest,
       AllocationNotPropagatedToEncoderWhenTargetRateUnchanged) {
  const int kFrameWidth = 320;
  const int kFrameHeight = 180;

  // Set initial rate.
  auto rate = DataRate::KilobitsPerSec(100);
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      /*target_bitrate=*/rate,
      /*stable_target_bitrate=*/rate,
      /*link_allocation=*/rate,
      /*fraction_lost=*/0,
      /*rtt_ms=*/0,
      /*cwnd_reduce_ratio=*/0);

  // Insert a first video frame so that encoder gets configured.
  int64_t timestamp_ms = CurrentTimeMs();
  VideoFrame frame = CreateFrame(timestamp_ms, kFrameWidth, kFrameHeight);
  frame.set_rotation(kVideoRotation_270);
  video_source_.IncomingCapturedFrame(frame);
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_EQ(1, fake_encoder_.GetNumSetRates());

  // Set a higher target rate without changing the link_allocation. Should not
  // reset encoder's rate.
  auto new_stable_rate = rate - DataRate::KilobitsPerSec(5);
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      /*target_bitrate=*/rate,
      /*stable_target_bitrate=*/new_stable_rate,
      /*link_allocation=*/rate,
      /*fraction_lost=*/0,
      /*rtt_ms=*/0,
      /*cwnd_reduce_ratio=*/0);
  video_stream_encoder_->WaitUntilTaskQueueIsIdle();
  EXPECT_EQ(1, fake_encoder_.GetNumSetRates());
  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, AutomaticAnimationDetection) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-AutomaticAnimationDetectionScreenshare/"
      "enabled:true,min_fps:20,min_duration_ms:1000,min_area_ratio:0.8/");
  const int kFramerateFps = 30;
  const int kWidth = 1920;
  const int kHeight = 1080;
  const int kNumFrames = 2 * kFramerateFps;  // >1 seconds of frames.
  // Works on screenshare mode.
  ResetEncoder("VP8", 1, 1, 1, /*screenshare*/ true);
  // We rely on the automatic resolution adaptation, but we handle framerate
  // adaptation manually by mocking the stats proxy.
  video_source_.set_adaptation_enabled(true);

  // BALANCED degradation preference is required for this feature.
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);
  video_stream_encoder_->SetSource(&video_source_,
                                   webrtc::DegradationPreference::BALANCED);
  EXPECT_THAT(video_source_.sink_wants(), UnlimitedSinkWants());

  VideoFrame frame = CreateFrame(1, kWidth, kHeight);
  frame.set_update_rect(VideoFrame::UpdateRect{0, 0, kWidth, kHeight});

  // Pass enough frames with the full update to trigger animation detection.
  for (int i = 0; i < kNumFrames; ++i) {
    int64_t timestamp_ms = CurrentTimeMs();
    frame.set_ntp_time_ms(timestamp_ms);
    frame.set_timestamp_us(timestamp_ms * 1000);
    video_source_.IncomingCapturedFrame(frame);
    WaitForEncodedFrame(timestamp_ms);
  }

  // Resolution should be limited.
  rtc::VideoSinkWants expected;
  expected.max_framerate_fps = kFramerateFps;
  expected.max_pixel_count = 1280 * 720 + 1;
  EXPECT_THAT(video_source_.sink_wants(), FpsEqResolutionLt(expected));

  // Pass one frame with no known update.
  //  Resolution cap should be removed immediately.
  int64_t timestamp_ms = CurrentTimeMs();
  frame.set_ntp_time_ms(timestamp_ms);
  frame.set_timestamp_us(timestamp_ms * 1000);
  frame.clear_update_rect();

  video_source_.IncomingCapturedFrame(frame);
  WaitForEncodedFrame(timestamp_ms);

  // Resolution should be unlimited now.
  EXPECT_THAT(video_source_.sink_wants(),
              FpsMatchesResolutionMax(Eq(kFramerateFps)));

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, ConfiguresVp9SvcAtOddResolutions) {
  const int kWidth = 720;  // 540p adapted down.
  const int kHeight = 405;
  const int kNumFrames = 3;
  // Works on screenshare mode.
  ResetEncoder("VP9", /*num_streams=*/1, /*num_temporal_layers=*/1,
               /*num_spatial_layers=*/2, /*screenshare=*/true);

  video_source_.set_adaptation_enabled(true);

  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps),
      DataRate::BitsPerSec(kTargetBitrateBps), 0, 0, 0);

  VideoFrame frame = CreateFrame(1, kWidth, kHeight);

  // Pass enough frames with the full update to trigger animation detection.
  for (int i = 0; i < kNumFrames; ++i) {
    int64_t timestamp_ms = CurrentTimeMs();
    frame.set_ntp_time_ms(timestamp_ms);
    frame.set_timestamp_us(timestamp_ms * 1000);
    video_source_.IncomingCapturedFrame(frame);
    WaitForEncodedFrame(timestamp_ms);
  }

  video_stream_encoder_->Stop();
}

TEST_F(VideoStreamEncoderTest, EncoderResetAccordingToParameterChange) {
  const float downscale_factors[] = {4.0, 2.0, 1.0};
  const int number_layers =
      sizeof(downscale_factors) / sizeof(downscale_factors[0]);
  VideoEncoderConfig config;
  test::FillEncoderConfiguration(kVideoCodecVP8, number_layers, &config);
  for (int i = 0; i < number_layers; ++i) {
    config.simulcast_layers[i].scale_resolution_down_by = downscale_factors[i];
    config.simulcast_layers[i].active = true;
  }
  config.video_stream_factory =
      new rtc::RefCountedObject<cricket::EncoderStreamFactory>(
          "VP8", /*max qp*/ 56, /*screencast*/ false,
          /*screenshare enabled*/ false);
  video_stream_encoder_->OnBitrateUpdatedAndWaitForManagedResources(
      DataRate::BitsPerSec(kSimulcastTargetBitrateBps),
      DataRate::BitsPerSec(kSimulcastTargetBitrateBps),
      DataRate::BitsPerSec(kSimulcastTargetBitrateBps), 0, 0, 0);

  // First initialization.
  // Encoder should be initialized. Next frame should be key frame.
  video_stream_encoder_->ConfigureEncoder(config.Copy(), kMaxPayloadLength);
  sink_.SetNumExpectedLayers(number_layers);
  int64_t timestamp_ms = kFrameIntervalMs;
  video_source_.IncomingCapturedFrame(CreateFrame(timestamp_ms, 1280, 720));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_EQ(1, fake_encoder_.GetNumEncoderInitializations());
  EXPECT_THAT(fake_encoder_.LastFrameTypes(),
              ::testing::ElementsAreArray({VideoFrameType::kVideoFrameKey,
                                           VideoFrameType::kVideoFrameKey,
                                           VideoFrameType::kVideoFrameKey}));

  // Disable top layer.
  // Encoder shouldn't be re-initialized. Next frame should be delta frame.
  config.simulcast_layers[number_layers - 1].active = false;
  video_stream_encoder_->ConfigureEncoder(config.Copy(), kMaxPayloadLength);
  sink_.SetNumExpectedLayers(number_layers - 1);
  timestamp_ms += kFrameIntervalMs;
  video_source_.IncomingCapturedFrame(CreateFrame(timestamp_ms, 1280, 720));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_EQ(1, fake_encoder_.GetNumEncoderInitializations());
  EXPECT_THAT(fake_encoder_.LastFrameTypes(),
              ::testing::ElementsAreArray({VideoFrameType::kVideoFrameDelta,
                                           VideoFrameType::kVideoFrameDelta,
                                           VideoFrameType::kVideoFrameDelta}));

  // Re-enable top layer.
  // Encoder should be re-initialized. Next frame should be key frame.
  config.simulcast_layers[number_layers - 1].active = true;
  video_stream_encoder_->ConfigureEncoder(config.Copy(), kMaxPayloadLength);
  sink_.SetNumExpectedLayers(number_layers);
  timestamp_ms += kFrameIntervalMs;
  video_source_.IncomingCapturedFrame(CreateFrame(timestamp_ms, 1280, 720));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_EQ(2, fake_encoder_.GetNumEncoderInitializations());
  EXPECT_THAT(fake_encoder_.LastFrameTypes(),
              ::testing::ElementsAreArray({VideoFrameType::kVideoFrameKey,
                                           VideoFrameType::kVideoFrameKey,
                                           VideoFrameType::kVideoFrameKey}));

  // Top layer max rate change.
  // Encoder shouldn't be re-initialized. Next frame should be delta frame.
  config.simulcast_layers[number_layers - 1].max_bitrate_bps -= 100;
  video_stream_encoder_->ConfigureEncoder(config.Copy(), kMaxPayloadLength);
  sink_.SetNumExpectedLayers(number_layers);
  timestamp_ms += kFrameIntervalMs;
  video_source_.IncomingCapturedFrame(CreateFrame(timestamp_ms, 1280, 720));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_EQ(2, fake_encoder_.GetNumEncoderInitializations());
  EXPECT_THAT(fake_encoder_.LastFrameTypes(),
              ::testing::ElementsAreArray({VideoFrameType::kVideoFrameDelta,
                                           VideoFrameType::kVideoFrameDelta,
                                           VideoFrameType::kVideoFrameDelta}));

  // Top layer resolution change.
  // Encoder should be re-initialized. Next frame should be key frame.
  config.simulcast_layers[number_layers - 1].scale_resolution_down_by += 0.1;
  video_stream_encoder_->ConfigureEncoder(config.Copy(), kMaxPayloadLength);
  sink_.SetNumExpectedLayers(number_layers);
  timestamp_ms += kFrameIntervalMs;
  video_source_.IncomingCapturedFrame(CreateFrame(timestamp_ms, 1280, 720));
  WaitForEncodedFrame(timestamp_ms);
  EXPECT_EQ(3, fake_encoder_.GetNumEncoderInitializations());
  EXPECT_THAT(fake_encoder_.LastFrameTypes(),
              ::testing::ElementsAreArray({VideoFrameType::kVideoFrameKey,
                                           VideoFrameType::kVideoFrameKey,
                                           VideoFrameType::kVideoFrameKey}));
  video_stream_encoder_->Stop();
}

}  // namespace webrtc
