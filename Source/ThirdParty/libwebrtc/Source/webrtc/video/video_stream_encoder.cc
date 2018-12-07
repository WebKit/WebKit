/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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
#include <numeric>
#include <utility>

#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_bitrate_allocator_factory.h"
#include "modules/video_coding/include/video_codec_initializer.h"
#include "modules/video_coding/include/video_coding.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/quality_scaling_experiment.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/system/fallthrough.h"
#include "rtc_base/timeutils.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/field_trial.h"
#include "video/overuse_frame_detector.h"

namespace webrtc {

namespace {

// Time interval for logging frame counts.
const int64_t kFrameLogIntervalMs = 60000;
const int kMinFramerateFps = 2;

// Time to keep a single cached pending frame in paused state.
const int64_t kPendingFrameTimeoutMs = 1000;

const char kInitialFramedropFieldTrial[] = "WebRTC-InitialFramedrop";

// The maximum number of frames to drop at beginning of stream
// to try and achieve desired bitrate.
const int kMaxInitialFramedrop = 4;
// When the first change in BWE above this threshold occurs,
// enable DropFrameDueToSize logic.
const float kFramedropThreshold = 0.3;

// Initial limits for BALANCED degradation preference.
int MinFps(int pixels) {
  if (pixels <= 320 * 240) {
    return 7;
  } else if (pixels <= 480 * 270) {
    return 10;
  } else if (pixels <= 640 * 480) {
    return 15;
  } else {
    return std::numeric_limits<int>::max();
  }
}

int MaxFps(int pixels) {
  if (pixels <= 320 * 240) {
    return 10;
  } else if (pixels <= 480 * 270) {
    return 15;
  } else {
    return std::numeric_limits<int>::max();
  }
}

uint32_t abs_diff(uint32_t a, uint32_t b) {
  return (a < b) ? b - a : a - b;
}

bool IsResolutionScalingEnabled(DegradationPreference degradation_preference) {
  return degradation_preference == DegradationPreference::MAINTAIN_FRAMERATE ||
         degradation_preference == DegradationPreference::BALANCED;
}

bool IsFramerateScalingEnabled(DegradationPreference degradation_preference) {
  return degradation_preference == DegradationPreference::MAINTAIN_RESOLUTION ||
         degradation_preference == DegradationPreference::BALANCED;
}

// TODO(pbos): Lower these thresholds (to closer to 100%) when we handle
// pipelining encoders better (multiple input frames before something comes
// out). This should effectively turn off CPU adaptations for systems that
// remotely cope with the load right now.
CpuOveruseOptions GetCpuOveruseOptions(
    const VideoStreamEncoderSettings& settings,
    bool full_overuse_time) {
  CpuOveruseOptions options;

  if (full_overuse_time) {
    options.low_encode_usage_threshold_percent = 150;
    options.high_encode_usage_threshold_percent = 200;
  }
  if (settings.experiment_cpu_load_estimator) {
    options.filter_time_ms = 5 * rtc::kNumMillisecsPerSec;
  }

  return options;
}

}  //  namespace

// VideoSourceProxy is responsible ensuring thread safety between calls to
// VideoStreamEncoder::SetSource that will happen on libjingle's worker thread
// when a video capturer is connected to the encoder and the encoder task queue
// (encoder_queue_) where the encoder reports its VideoSinkWants.
class VideoStreamEncoder::VideoSourceProxy {
 public:
  explicit VideoSourceProxy(VideoStreamEncoder* video_stream_encoder)
      : video_stream_encoder_(video_stream_encoder),
        degradation_preference_(DegradationPreference::DISABLED),
        source_(nullptr),
        max_framerate_(std::numeric_limits<int>::max()) {}

  void SetSource(rtc::VideoSourceInterface<VideoFrame>* source,
                 const DegradationPreference& degradation_preference) {
    // Called on libjingle's worker thread.
    RTC_DCHECK_CALLED_SEQUENTIALLY(&main_checker_);
    rtc::VideoSourceInterface<VideoFrame>* old_source = nullptr;
    rtc::VideoSinkWants wants;
    {
      rtc::CritScope lock(&crit_);
      degradation_preference_ = degradation_preference;
      old_source = source_;
      source_ = source;
      wants = GetActiveSinkWantsInternal();
    }

    if (old_source != source && old_source != nullptr) {
      old_source->RemoveSink(video_stream_encoder_);
    }

    if (!source) {
      return;
    }

    source->AddOrUpdateSink(video_stream_encoder_, wants);
  }

  void SetMaxFramerate(int max_framerate) {
    RTC_DCHECK_GT(max_framerate, 0);
    rtc::CritScope lock(&crit_);
    if (max_framerate == max_framerate_)
      return;

    RTC_LOG(LS_INFO) << "Set max framerate: " << max_framerate;
    max_framerate_ = max_framerate;
    if (source_) {
      source_->AddOrUpdateSink(video_stream_encoder_,
                               GetActiveSinkWantsInternal());
    }
  }

  void SetWantsRotationApplied(bool rotation_applied) {
    rtc::CritScope lock(&crit_);
    sink_wants_.rotation_applied = rotation_applied;
    if (source_) {
      source_->AddOrUpdateSink(video_stream_encoder_,
                               GetActiveSinkWantsInternal());
    }
  }

  rtc::VideoSinkWants GetActiveSinkWants() {
    rtc::CritScope lock(&crit_);
    return GetActiveSinkWantsInternal();
  }

  void ResetPixelFpsCount() {
    rtc::CritScope lock(&crit_);
    sink_wants_.max_pixel_count = std::numeric_limits<int>::max();
    sink_wants_.target_pixel_count.reset();
    sink_wants_.max_framerate_fps = std::numeric_limits<int>::max();
    if (source_)
      source_->AddOrUpdateSink(video_stream_encoder_,
                               GetActiveSinkWantsInternal());
  }

  bool RequestResolutionLowerThan(int pixel_count,
                                  int min_pixels_per_frame,
                                  bool* min_pixels_reached) {
    // Called on the encoder task queue.
    rtc::CritScope lock(&crit_);
    if (!source_ || !IsResolutionScalingEnabled(degradation_preference_)) {
      // This can happen since |degradation_preference_| is set on libjingle's
      // worker thread but the adaptation is done on the encoder task queue.
      return false;
    }
    // The input video frame size will have a resolution less than or equal to
    // |max_pixel_count| depending on how the source can scale the frame size.
    const int pixels_wanted = (pixel_count * 3) / 5;
    if (pixels_wanted >= sink_wants_.max_pixel_count) {
      return false;
    }
    if (pixels_wanted < min_pixels_per_frame) {
      *min_pixels_reached = true;
      return false;
    }
    RTC_LOG(LS_INFO) << "Scaling down resolution, max pixels: "
                     << pixels_wanted;
    sink_wants_.max_pixel_count = pixels_wanted;
    sink_wants_.target_pixel_count = absl::nullopt;
    source_->AddOrUpdateSink(video_stream_encoder_,
                             GetActiveSinkWantsInternal());
    return true;
  }

  int RequestFramerateLowerThan(int fps) {
    // Called on the encoder task queue.
    // The input video frame rate will be scaled down to 2/3, rounding down.
    int framerate_wanted = (fps * 2) / 3;
    return RestrictFramerate(framerate_wanted) ? framerate_wanted : -1;
  }

  bool RequestHigherResolutionThan(int pixel_count) {
    // Called on the encoder task queue.
    rtc::CritScope lock(&crit_);
    if (!source_ || !IsResolutionScalingEnabled(degradation_preference_)) {
      // This can happen since |degradation_preference_| is set on libjingle's
      // worker thread but the adaptation is done on the encoder task queue.
      return false;
    }
    int max_pixels_wanted = pixel_count;
    if (max_pixels_wanted != std::numeric_limits<int>::max())
      max_pixels_wanted = pixel_count * 4;

    if (max_pixels_wanted <= sink_wants_.max_pixel_count)
      return false;

    sink_wants_.max_pixel_count = max_pixels_wanted;
    if (max_pixels_wanted == std::numeric_limits<int>::max()) {
      // Remove any constraints.
      sink_wants_.target_pixel_count.reset();
    } else {
      // On step down we request at most 3/5 the pixel count of the previous
      // resolution, so in order to take "one step up" we request a resolution
      // as close as possible to 5/3 of the current resolution. The actual pixel
      // count selected depends on the capabilities of the source. In order to
      // not take a too large step up, we cap the requested pixel count to be at
      // most four time the current number of pixels.
      sink_wants_.target_pixel_count = (pixel_count * 5) / 3;
    }
    RTC_LOG(LS_INFO) << "Scaling up resolution, max pixels: "
                     << max_pixels_wanted;
    source_->AddOrUpdateSink(video_stream_encoder_,
                             GetActiveSinkWantsInternal());
    return true;
  }

  // Request upgrade in framerate. Returns the new requested frame, or -1 if
  // no change requested. Note that maxint may be returned if limits due to
  // adaptation requests are removed completely. In that case, consider
  // |max_framerate_| to be the current limit (assuming the capturer complies).
  int RequestHigherFramerateThan(int fps) {
    // Called on the encoder task queue.
    // The input frame rate will be scaled up to the last step, with rounding.
    int framerate_wanted = fps;
    if (fps != std::numeric_limits<int>::max())
      framerate_wanted = (fps * 3) / 2;

    return IncreaseFramerate(framerate_wanted) ? framerate_wanted : -1;
  }

  bool RestrictFramerate(int fps) {
    // Called on the encoder task queue.
    rtc::CritScope lock(&crit_);
    if (!source_ || !IsFramerateScalingEnabled(degradation_preference_))
      return false;

    const int fps_wanted = std::max(kMinFramerateFps, fps);
    if (fps_wanted >= sink_wants_.max_framerate_fps)
      return false;

    RTC_LOG(LS_INFO) << "Scaling down framerate: " << fps_wanted;
    sink_wants_.max_framerate_fps = fps_wanted;
    source_->AddOrUpdateSink(video_stream_encoder_,
                             GetActiveSinkWantsInternal());
    return true;
  }

  bool IncreaseFramerate(int fps) {
    // Called on the encoder task queue.
    rtc::CritScope lock(&crit_);
    if (!source_ || !IsFramerateScalingEnabled(degradation_preference_))
      return false;

    const int fps_wanted = std::max(kMinFramerateFps, fps);
    if (fps_wanted <= sink_wants_.max_framerate_fps)
      return false;

    RTC_LOG(LS_INFO) << "Scaling up framerate: " << fps_wanted;
    sink_wants_.max_framerate_fps = fps_wanted;
    source_->AddOrUpdateSink(video_stream_encoder_,
                             GetActiveSinkWantsInternal());
    return true;
  }

 private:
  rtc::VideoSinkWants GetActiveSinkWantsInternal()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(&crit_) {
    rtc::VideoSinkWants wants = sink_wants_;
    // Clear any constraints from the current sink wants that don't apply to
    // the used degradation_preference.
    switch (degradation_preference_) {
      case DegradationPreference::BALANCED:
        break;
      case DegradationPreference::MAINTAIN_FRAMERATE:
        wants.max_framerate_fps = std::numeric_limits<int>::max();
        break;
      case DegradationPreference::MAINTAIN_RESOLUTION:
        wants.max_pixel_count = std::numeric_limits<int>::max();
        wants.target_pixel_count.reset();
        break;
      case DegradationPreference::DISABLED:
        wants.max_pixel_count = std::numeric_limits<int>::max();
        wants.target_pixel_count.reset();
        wants.max_framerate_fps = std::numeric_limits<int>::max();
    }
    // Limit to configured max framerate.
    wants.max_framerate_fps = std::min(max_framerate_, wants.max_framerate_fps);
    return wants;
  }

  rtc::CriticalSection crit_;
  rtc::SequencedTaskChecker main_checker_;
  VideoStreamEncoder* const video_stream_encoder_;
  rtc::VideoSinkWants sink_wants_ RTC_GUARDED_BY(&crit_);
  DegradationPreference degradation_preference_ RTC_GUARDED_BY(&crit_);
  rtc::VideoSourceInterface<VideoFrame>* source_ RTC_GUARDED_BY(&crit_);
  int max_framerate_ RTC_GUARDED_BY(&crit_);

  RTC_DISALLOW_COPY_AND_ASSIGN(VideoSourceProxy);
};

VideoStreamEncoder::VideoStreamEncoder(
    uint32_t number_of_cores,
    VideoStreamEncoderObserver* encoder_stats_observer,
    const VideoStreamEncoderSettings& settings,
    rtc::VideoSinkInterface<VideoFrame>* pre_encode_callback,
    std::unique_ptr<OveruseFrameDetector> overuse_detector)
    : shutdown_event_(true /* manual_reset */, false),
      number_of_cores_(number_of_cores),
      initial_framedrop_(0),
      initial_framedrop_on_bwe_enabled_(
          webrtc::field_trial::IsEnabled(kInitialFramedropFieldTrial)),
      quality_scaling_experiment_enabled_(QualityScalingExperiment::Enabled()),
      source_proxy_(new VideoSourceProxy(this)),
      sink_(nullptr),
      settings_(settings),
      video_sender_(Clock::GetRealTimeClock(), this),
      overuse_detector_(std::move(overuse_detector)),
      encoder_stats_observer_(encoder_stats_observer),
      pre_encode_callback_(pre_encode_callback),
      max_framerate_(-1),
      pending_encoder_reconfiguration_(false),
      pending_encoder_creation_(false),
      crop_width_(0),
      crop_height_(0),
      encoder_start_bitrate_bps_(0),
      max_data_payload_length_(0),
      last_observed_bitrate_bps_(0),
      encoder_paused_and_dropped_frame_(false),
      clock_(Clock::GetRealTimeClock()),
      degradation_preference_(DegradationPreference::DISABLED),
      posted_frames_waiting_for_encode_(0),
      last_captured_timestamp_(0),
      delta_ntp_internal_ms_(clock_->CurrentNtpInMilliseconds() -
                             clock_->TimeInMilliseconds()),
      last_frame_log_ms_(clock_->TimeInMilliseconds()),
      captured_frame_count_(0),
      dropped_frame_count_(0),
      pending_frame_post_time_us_(0),
      bitrate_observer_(nullptr),
      encoder_queue_("EncoderQueue") {
  RTC_DCHECK(encoder_stats_observer);
  RTC_DCHECK(overuse_detector_);
}

VideoStreamEncoder::~VideoStreamEncoder() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(shutdown_event_.Wait(0))
      << "Must call ::Stop() before destruction.";
}

void VideoStreamEncoder::Stop() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  source_proxy_->SetSource(nullptr, DegradationPreference());
  encoder_queue_.PostTask([this] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    overuse_detector_->StopCheckForOveruse();
    rate_allocator_.reset();
    bitrate_observer_ = nullptr;
    video_sender_.RegisterExternalEncoder(nullptr, false);
    quality_scaler_ = nullptr;
    shutdown_event_.Set();
  });

  shutdown_event_.Wait(rtc::Event::kForever);
}

void VideoStreamEncoder::SetBitrateAllocationObserver(
    VideoBitrateAllocationObserver* bitrate_observer) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  encoder_queue_.PostTask([this, bitrate_observer] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    RTC_DCHECK(!bitrate_observer_);
    bitrate_observer_ = bitrate_observer;
  });
}

void VideoStreamEncoder::SetSource(
    rtc::VideoSourceInterface<VideoFrame>* source,
    const DegradationPreference& degradation_preference) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  source_proxy_->SetSource(source, degradation_preference);
  encoder_queue_.PostTask([this, degradation_preference] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    if (degradation_preference_ != degradation_preference) {
      // Reset adaptation state, so that we're not tricked into thinking there's
      // an already pending request of the same type.
      last_adaptation_request_.reset();
      if (degradation_preference == DegradationPreference::BALANCED ||
          degradation_preference_ == DegradationPreference::BALANCED) {
        // TODO(asapersson): Consider removing |adapt_counters_| map and use one
        // AdaptCounter for all modes.
        source_proxy_->ResetPixelFpsCount();
        adapt_counters_.clear();
      }
    }
    degradation_preference_ = degradation_preference;

    if (encoder_)
      ConfigureQualityScaler();

    if (!IsFramerateScalingEnabled(degradation_preference) &&
        max_framerate_ != -1) {
      // If frame rate scaling is no longer allowed, remove any potential
      // allowance for longer frame intervals.
      overuse_detector_->OnTargetFramerateUpdated(max_framerate_);
    }
  });
}

void VideoStreamEncoder::SetSink(EncoderSink* sink, bool rotation_applied) {
  source_proxy_->SetWantsRotationApplied(rotation_applied);
  encoder_queue_.PostTask([this, sink] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    sink_ = sink;
  });
}

void VideoStreamEncoder::SetStartBitrate(int start_bitrate_bps) {
  encoder_queue_.PostTask([this, start_bitrate_bps] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    encoder_start_bitrate_bps_ = start_bitrate_bps;
  });
}

void VideoStreamEncoder::ConfigureEncoder(VideoEncoderConfig config,
                                          size_t max_data_payload_length) {
  // TODO(srte): This struct should be replaced by a lambda with move capture
  // when C++14 lambda is allowed.
  struct ConfigureEncoderTask {
    void operator()() {
      encoder->ConfigureEncoderOnTaskQueue(std::move(config),
                                           max_data_payload_length);
    }
    VideoStreamEncoder* encoder;
    VideoEncoderConfig config;
    size_t max_data_payload_length;
  };
  encoder_queue_.PostTask(
      ConfigureEncoderTask{this, std::move(config), max_data_payload_length});
}

void VideoStreamEncoder::ConfigureEncoderOnTaskQueue(
    VideoEncoderConfig config,
    size_t max_data_payload_length) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  RTC_DCHECK(sink_);
  RTC_LOG(LS_INFO) << "ConfigureEncoder requested.";

  max_data_payload_length_ = max_data_payload_length;
  pending_encoder_creation_ =
      (!encoder_ || encoder_config_.video_format != config.video_format);
  encoder_config_ = std::move(config);
  pending_encoder_reconfiguration_ = true;

  // Reconfigure the encoder now if the encoder has an internal source or
  // if the frame resolution is known. Otherwise, the reconfiguration is
  // deferred until the next frame to minimize the number of reconfigurations.
  // The codec configuration depends on incoming video frame size.
  if (last_frame_info_) {
    ReconfigureEncoder();
  } else if (settings_.encoder_factory
                 ->QueryVideoEncoder(encoder_config_.video_format)
                 .has_internal_source) {
    last_frame_info_ = VideoFrameInfo(176, 144, false);
    ReconfigureEncoder();
  }
}

// TODO(bugs.webrtc.org/8807): Currently this always does a hard
// reconfiguration, but this isn't always necessary. Add in logic to only update
// the VideoBitrateAllocator and call OnEncoderConfigurationChanged with a
// "soft" reconfiguration.
void VideoStreamEncoder::ReconfigureEncoder() {
  RTC_DCHECK(pending_encoder_reconfiguration_);
  std::vector<VideoStream> streams =
      encoder_config_.video_stream_factory->CreateEncoderStreams(
          last_frame_info_->width, last_frame_info_->height, encoder_config_);

  // TODO(ilnik): If configured resolution is significantly less than provided,
  // e.g. because there are not enough SSRCs for all simulcast streams,
  // signal new resolutions via SinkWants to video source.

  // Stream dimensions may be not equal to given because of a simulcast
  // restrictions.
  int highest_stream_width = static_cast<int>(streams.back().width);
  int highest_stream_height = static_cast<int>(streams.back().height);
  // Dimension may be reduced to be, e.g. divisible by 4.
  RTC_CHECK_GE(last_frame_info_->width, highest_stream_width);
  RTC_CHECK_GE(last_frame_info_->height, highest_stream_height);
  crop_width_ = last_frame_info_->width - highest_stream_width;
  crop_height_ = last_frame_info_->height - highest_stream_height;

  VideoCodec codec;
  if (!VideoCodecInitializer::SetupCodec(encoder_config_, streams, &codec)) {
    RTC_LOG(LS_ERROR) << "Failed to create encoder configuration.";
  }

  rate_allocator_ =
      settings_.bitrate_allocator_factory->CreateVideoBitrateAllocator(codec);
  RTC_CHECK(rate_allocator_) << "Failed to create bitrate allocator.";

  // Set min_bitrate_bps, max_bitrate_bps, and max padding bit rate for VP9.
  if (encoder_config_.codec_type == kVideoCodecVP9) {
    RTC_DCHECK_EQ(1U, streams.size());
    int max_encoder_bitrate_kbps = 0;
    for (int i = 0; i < codec.VP9()->numberOfSpatialLayers; ++i) {
      max_encoder_bitrate_kbps += codec.spatialLayers[i].maxBitrate;
    }
    // Lower max bitrate to the level codec actually can produce.
    streams[0].max_bitrate_bps =
        std::min(streams[0].max_bitrate_bps, max_encoder_bitrate_kbps * 1000);
    streams[0].min_bitrate_bps = codec.spatialLayers[0].minBitrate * 1000;
    // Pass along the value of maximum padding bit rate from
    // spatialLayers[].targetBitrate to streams[0].target_bitrate_bps.
    // TODO(ssilkin): There should be some margin between max padding bitrate
    // and max encoder bitrate. With the current logic they can be equal.
    streams[0].target_bitrate_bps =
        std::min(static_cast<unsigned int>(streams[0].max_bitrate_bps),
                 codec.spatialLayers[codec.VP9()->numberOfSpatialLayers - 1]
                         .targetBitrate *
                     1000);
  }

  codec.startBitrate =
      std::max(encoder_start_bitrate_bps_ / 1000, codec.minBitrate);
  codec.startBitrate = std::min(codec.startBitrate, codec.maxBitrate);
  codec.expect_encode_from_texture = last_frame_info_->is_texture;
  max_framerate_ = codec.maxFramerate;

  // Inform source about max configured framerate.
  int max_framerate = 0;
  for (const auto& stream : streams) {
    max_framerate = std::max(stream.max_framerate, max_framerate);
  }
  source_proxy_->SetMaxFramerate(max_framerate);

  // Keep the same encoder, as long as the video_format is unchanged.
  if (pending_encoder_creation_) {
    pending_encoder_creation_ = false;
    if (encoder_) {
      video_sender_.RegisterExternalEncoder(nullptr, false);
    }

    encoder_ = settings_.encoder_factory->CreateVideoEncoder(
        encoder_config_.video_format);
    // TODO(nisse): What to do if creating the encoder fails? Crash,
    // or just discard incoming frames?
    RTC_CHECK(encoder_);

    const webrtc::VideoEncoderFactory::CodecInfo info =
        settings_.encoder_factory->QueryVideoEncoder(
            encoder_config_.video_format);

    overuse_detector_->StopCheckForOveruse();
    overuse_detector_->StartCheckForOveruse(
        GetCpuOveruseOptions(settings_, info.is_hardware_accelerated), this);

    video_sender_.RegisterExternalEncoder(encoder_.get(),
                                          info.has_internal_source);
  }
  // RegisterSendCodec implies an unconditional call to
  // encoder_->InitEncode().
  bool success = video_sender_.RegisterSendCodec(
                     &codec, number_of_cores_,
                     static_cast<uint32_t>(max_data_payload_length_)) == VCM_OK;
  if (!success) {
    RTC_LOG(LS_ERROR) << "Failed to configure encoder.";
    rate_allocator_.reset();
  }

  video_sender_.UpdateChannelParameters(rate_allocator_.get(),
                                        bitrate_observer_);

  encoder_stats_observer_->OnEncoderReconfigured(encoder_config_, streams);

  pending_encoder_reconfiguration_ = false;

  sink_->OnEncoderConfigurationChanged(
      std::move(streams), encoder_config_.min_transmit_bitrate_bps);

  // Get the current target framerate, ie the maximum framerate as specified by
  // the current codec configuration, or any limit imposed by cpu adaption in
  // maintain-resolution or balanced mode. This is used to make sure overuse
  // detection doesn't needlessly trigger in low and/or variable framerate
  // scenarios.
  int target_framerate = std::min(
      max_framerate_, source_proxy_->GetActiveSinkWants().max_framerate_fps);
  overuse_detector_->OnTargetFramerateUpdated(target_framerate);

  ConfigureQualityScaler();
}

void VideoStreamEncoder::ConfigureQualityScaler() {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  const auto scaling_settings = encoder_->GetEncoderInfo().scaling_settings;
  const bool quality_scaling_allowed =
      IsResolutionScalingEnabled(degradation_preference_) &&
      scaling_settings.thresholds;

  if (quality_scaling_allowed) {
    if (quality_scaler_.get() == nullptr) {
      // Quality scaler has not already been configured.

      // Use experimental thresholds if available.
      absl::optional<VideoEncoder::QpThresholds> experimental_thresholds;
      if (quality_scaling_experiment_enabled_) {
        experimental_thresholds = QualityScalingExperiment::GetQpThresholds(
            encoder_config_.codec_type);
      }
      // Since the interface is non-public, absl::make_unique can't do this
      // upcast.
      AdaptationObserverInterface* observer = this;
      quality_scaler_ = absl::make_unique<QualityScaler>(
          observer, experimental_thresholds ? *experimental_thresholds
                                            : *(scaling_settings.thresholds));
      has_seen_first_significant_bwe_change_ = false;
      initial_framedrop_ = 0;
    }
  } else {
    quality_scaler_.reset(nullptr);
    initial_framedrop_ = kMaxInitialFramedrop;
  }

  encoder_stats_observer_->OnAdaptationChanged(
      VideoStreamEncoderObserver::AdaptationReason::kNone,
      GetActiveCounts(kCpu), GetActiveCounts(kQuality));
}

void VideoStreamEncoder::OnFrame(const VideoFrame& video_frame) {
  RTC_DCHECK_RUNS_SERIALIZED(&incoming_frame_race_checker_);
  VideoFrame incoming_frame = video_frame;

  // Local time in webrtc time base.
  int64_t current_time_us = clock_->TimeInMicroseconds();
  int64_t current_time_ms = current_time_us / rtc::kNumMicrosecsPerMillisec;
  // In some cases, e.g., when the frame from decoder is fed to encoder,
  // the timestamp may be set to the future. As the encoding pipeline assumes
  // capture time to be less than present time, we should reset the capture
  // timestamps here. Otherwise there may be issues with RTP send stream.
  if (incoming_frame.timestamp_us() > current_time_us)
    incoming_frame.set_timestamp_us(current_time_us);

  // Capture time may come from clock with an offset and drift from clock_.
  int64_t capture_ntp_time_ms;
  if (video_frame.ntp_time_ms() > 0) {
    capture_ntp_time_ms = video_frame.ntp_time_ms();
  } else if (video_frame.render_time_ms() != 0) {
    capture_ntp_time_ms = video_frame.render_time_ms() + delta_ntp_internal_ms_;
  } else {
    capture_ntp_time_ms = current_time_ms + delta_ntp_internal_ms_;
  }
  incoming_frame.set_ntp_time_ms(capture_ntp_time_ms);

  // Convert NTP time, in ms, to RTP timestamp.
  const int kMsToRtpTimestamp = 90;
  incoming_frame.set_timestamp(
      kMsToRtpTimestamp * static_cast<uint32_t>(incoming_frame.ntp_time_ms()));

  if (incoming_frame.ntp_time_ms() <= last_captured_timestamp_) {
    // We don't allow the same capture time for two frames, drop this one.
    RTC_LOG(LS_WARNING) << "Same/old NTP timestamp ("
                        << incoming_frame.ntp_time_ms()
                        << " <= " << last_captured_timestamp_
                        << ") for incoming frame. Dropping.";
    return;
  }

  bool log_stats = false;
  if (current_time_ms - last_frame_log_ms_ > kFrameLogIntervalMs) {
    last_frame_log_ms_ = current_time_ms;
    log_stats = true;
  }

  last_captured_timestamp_ = incoming_frame.ntp_time_ms();

  int64_t post_time_us = rtc::TimeMicros();
  ++posted_frames_waiting_for_encode_;

  encoder_queue_.PostTask(
      [this, incoming_frame, post_time_us, log_stats]() {
        RTC_DCHECK_RUN_ON(&encoder_queue_);
        encoder_stats_observer_->OnIncomingFrame(incoming_frame.width(),
                                                 incoming_frame.height());
        ++captured_frame_count_;
        const int posted_frames_waiting_for_encode =
            posted_frames_waiting_for_encode_.fetch_sub(1);
        RTC_DCHECK_GT(posted_frames_waiting_for_encode, 0);
        if (posted_frames_waiting_for_encode == 1) {
          MaybeEncodeVideoFrame(incoming_frame, post_time_us);
        } else {
          // There is a newer frame in flight. Do not encode this frame.
          RTC_LOG(LS_VERBOSE)
              << "Incoming frame dropped due to that the encoder is blocked.";
          ++dropped_frame_count_;
          encoder_stats_observer_->OnFrameDropped(
              VideoStreamEncoderObserver::DropReason::kEncoderQueue);
        }
        if (log_stats) {
          RTC_LOG(LS_INFO) << "Number of frames: captured "
                           << captured_frame_count_
                           << ", dropped (due to encoder blocked) "
                           << dropped_frame_count_ << ", interval_ms "
                           << kFrameLogIntervalMs;
          captured_frame_count_ = 0;
          dropped_frame_count_ = 0;
        }
      });
}

void VideoStreamEncoder::OnDiscardedFrame() {
  encoder_stats_observer_->OnFrameDropped(
      VideoStreamEncoderObserver::DropReason::kSource);
}

bool VideoStreamEncoder::EncoderPaused() const {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  // Pause video if paused by caller or as long as the network is down or the
  // pacer queue has grown too large in buffered mode.
  // If the pacer queue has grown too large or the network is down,
  // last_observed_bitrate_bps_ will be 0.
  return last_observed_bitrate_bps_ == 0;
}

void VideoStreamEncoder::TraceFrameDropStart() {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  // Start trace event only on the first frame after encoder is paused.
  if (!encoder_paused_and_dropped_frame_) {
    TRACE_EVENT_ASYNC_BEGIN0("webrtc", "EncoderPaused", this);
  }
  encoder_paused_and_dropped_frame_ = true;
}

void VideoStreamEncoder::TraceFrameDropEnd() {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  // End trace event on first frame after encoder resumes, if frame was dropped.
  if (encoder_paused_and_dropped_frame_) {
    TRACE_EVENT_ASYNC_END0("webrtc", "EncoderPaused", this);
  }
  encoder_paused_and_dropped_frame_ = false;
}

void VideoStreamEncoder::MaybeEncodeVideoFrame(const VideoFrame& video_frame,
                                               int64_t time_when_posted_us) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);

  if (pre_encode_callback_)
    pre_encode_callback_->OnFrame(video_frame);

  if (!last_frame_info_ || video_frame.width() != last_frame_info_->width ||
      video_frame.height() != last_frame_info_->height ||
      video_frame.is_texture() != last_frame_info_->is_texture) {
    pending_encoder_reconfiguration_ = true;
    last_frame_info_ = VideoFrameInfo(video_frame.width(), video_frame.height(),
                                      video_frame.is_texture());
    RTC_LOG(LS_INFO) << "Video frame parameters changed: dimensions="
                     << last_frame_info_->width << "x"
                     << last_frame_info_->height
                     << ", texture=" << last_frame_info_->is_texture << ".";
  }

  // We have to create then encoder before the frame drop logic,
  // because the latter depends on encoder_->GetScalingSettings.
  // According to the testcase
  // InitialFrameDropOffWhenEncoderDisabledScaling, the return value
  // from GetScalingSettings should enable or disable the frame drop.

  int64_t now_ms = clock_->TimeInMilliseconds();
  if (pending_encoder_reconfiguration_) {
    ReconfigureEncoder();
    last_parameters_update_ms_.emplace(now_ms);
  } else if (!last_parameters_update_ms_ ||
             now_ms - *last_parameters_update_ms_ >=
                 vcm::VCMProcessTimer::kDefaultProcessIntervalMs) {
    video_sender_.UpdateChannelParameters(rate_allocator_.get(),
                                          bitrate_observer_);
    last_parameters_update_ms_.emplace(now_ms);
  }

  if (DropDueToSize(video_frame.size())) {
    RTC_LOG(LS_INFO) << "Dropping frame. Too large for target bitrate.";
    int count = GetConstAdaptCounter().ResolutionCount(kQuality);
    AdaptDown(kQuality);
    if (GetConstAdaptCounter().ResolutionCount(kQuality) > count) {
      encoder_stats_observer_->OnInitialQualityResolutionAdaptDown();
    }
    ++initial_framedrop_;
    // Storing references to a native buffer risks blocking frame capture.
    if (video_frame.video_frame_buffer()->type() !=
        VideoFrameBuffer::Type::kNative) {
      pending_frame_ = video_frame;
      pending_frame_post_time_us_ = time_when_posted_us;
    } else {
      // Ensure that any previously stored frame is dropped.
      pending_frame_.reset();
    }
    return;
  }
  initial_framedrop_ = kMaxInitialFramedrop;

  if (EncoderPaused()) {
    // Storing references to a native buffer risks blocking frame capture.
    if (video_frame.video_frame_buffer()->type() !=
        VideoFrameBuffer::Type::kNative) {
      if (pending_frame_)
        TraceFrameDropStart();
      pending_frame_ = video_frame;
      pending_frame_post_time_us_ = time_when_posted_us;
    } else {
      // Ensure that any previously stored frame is dropped.
      pending_frame_.reset();
      TraceFrameDropStart();
    }
    return;
  }

  pending_frame_.reset();
  EncodeVideoFrame(video_frame, time_when_posted_us);
}

void VideoStreamEncoder::EncodeVideoFrame(const VideoFrame& video_frame,
                                          int64_t time_when_posted_us) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  TraceFrameDropEnd();

  VideoFrame out_frame(video_frame);
  // Crop frame if needed.
  if (crop_width_ > 0 || crop_height_ > 0) {
    int cropped_width = video_frame.width() - crop_width_;
    int cropped_height = video_frame.height() - crop_height_;
    rtc::scoped_refptr<I420Buffer> cropped_buffer =
        I420Buffer::Create(cropped_width, cropped_height);
    // TODO(ilnik): Remove scaling if cropping is too big, as it should never
    // happen after SinkWants signaled correctly from ReconfigureEncoder.
    if (crop_width_ < 4 && crop_height_ < 4) {
      cropped_buffer->CropAndScaleFrom(
          *video_frame.video_frame_buffer()->ToI420(), crop_width_ / 2,
          crop_height_ / 2, cropped_width, cropped_height);
    } else {
      cropped_buffer->ScaleFrom(
          *video_frame.video_frame_buffer()->ToI420().get());
    }
    out_frame =
        VideoFrame(cropped_buffer, video_frame.timestamp(),
                   video_frame.render_time_ms(), video_frame.rotation());
    out_frame.set_ntp_time_ms(video_frame.ntp_time_ms());
  }

  TRACE_EVENT_ASYNC_STEP0("webrtc", "Video", video_frame.render_time_ms(),
                          "Encode");

  overuse_detector_->FrameCaptured(out_frame, time_when_posted_us);

  // Encoder metadata needs to be updated before encode complete callback.
  VideoEncoder::EncoderInfo info = encoder_->GetEncoderInfo();
  if (info.implementation_name != encoder_info_.implementation_name) {
    encoder_stats_observer_->OnEncoderImplementationChanged(
        info.implementation_name);
  }
  encoder_info_ = info;

  video_sender_.AddVideoFrame(out_frame, nullptr, encoder_info_);
}

void VideoStreamEncoder::SendKeyFrame() {
  if (!encoder_queue_.IsCurrent()) {
    encoder_queue_.PostTask([this] { SendKeyFrame(); });
    return;
  }
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  TRACE_EVENT0("webrtc", "OnKeyFrameRequest");
  video_sender_.IntraFrameRequest(0);
}

EncodedImageCallback::Result VideoStreamEncoder::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific_info,
    const RTPFragmentationHeader* fragmentation) {
  // Encoded is called on whatever thread the real encoder implementation run
  // on. In the case of hardware encoders, there might be several encoders
  // running in parallel on different threads.
  encoder_stats_observer_->OnSendEncodedImage(encoded_image,
                                              codec_specific_info);

  EncodedImageCallback::Result result =
      sink_->OnEncodedImage(encoded_image, codec_specific_info, fragmentation);

  int64_t time_sent_us = rtc::TimeMicros();
  uint32_t timestamp = encoded_image.Timestamp();
  const int qp = encoded_image.qp_;
  int64_t capture_time_us =
      encoded_image.capture_time_ms_ * rtc::kNumMicrosecsPerMillisec;

  absl::optional<int> encode_duration_us;
  if (encoded_image.timing_.flags != VideoSendTiming::kInvalid) {
    encode_duration_us.emplace(
        // TODO(nisse): Maybe use capture_time_ms_ rather than encode_start_ms_?
        rtc::kNumMicrosecsPerMillisec *
        (encoded_image.timing_.encode_finish_ms -
         encoded_image.timing_.encode_start_ms));
  }

  encoder_queue_.PostTask(
      [this, timestamp, time_sent_us, qp, capture_time_us, encode_duration_us] {
        RTC_DCHECK_RUN_ON(&encoder_queue_);
        overuse_detector_->FrameSent(timestamp, time_sent_us, capture_time_us,
                                     encode_duration_us);
        if (quality_scaler_ && qp >= 0)
          quality_scaler_->ReportQp(qp);
      });

  return result;
}

void VideoStreamEncoder::OnDroppedFrame(DropReason reason) {
  switch (reason) {
    case DropReason::kDroppedByMediaOptimizations:
      encoder_stats_observer_->OnFrameDropped(
          VideoStreamEncoderObserver::DropReason::kMediaOptimization);
      encoder_queue_.PostTask([this] {
        RTC_DCHECK_RUN_ON(&encoder_queue_);
        if (quality_scaler_)
          quality_scaler_->ReportDroppedFrameByMediaOpt();
      });
      break;
    case DropReason::kDroppedByEncoder:
      encoder_stats_observer_->OnFrameDropped(
          VideoStreamEncoderObserver::DropReason::kEncoder);
      encoder_queue_.PostTask([this] {
        RTC_DCHECK_RUN_ON(&encoder_queue_);
        if (quality_scaler_)
          quality_scaler_->ReportDroppedFrameByEncoder();
      });
      break;
  }
}

void VideoStreamEncoder::OnBitrateUpdated(uint32_t bitrate_bps,
                                          uint8_t fraction_lost,
                                          int64_t round_trip_time_ms) {
  if (!encoder_queue_.IsCurrent()) {
    encoder_queue_.PostTask(
        [this, bitrate_bps, fraction_lost, round_trip_time_ms] {
          OnBitrateUpdated(bitrate_bps, fraction_lost, round_trip_time_ms);
        });
    return;
  }
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  RTC_DCHECK(sink_) << "sink_ must be set before the encoder is active.";

  RTC_LOG(LS_VERBOSE) << "OnBitrateUpdated, bitrate " << bitrate_bps
                      << " packet loss " << static_cast<int>(fraction_lost)
                      << " rtt " << round_trip_time_ms;
  // On significant changes to BWE at the start of the call,
  // enable frame drops to quickly react to jumps in available bandwidth.
  if (encoder_start_bitrate_bps_ != 0 &&
      !has_seen_first_significant_bwe_change_ && quality_scaler_ &&
      initial_framedrop_on_bwe_enabled_ &&
      abs_diff(bitrate_bps, encoder_start_bitrate_bps_) >=
          kFramedropThreshold * encoder_start_bitrate_bps_) {
    // Reset initial framedrop feature when first real BW estimate arrives.
    // TODO(kthelgason): Update BitrateAllocator to not call OnBitrateUpdated
    // without an actual BW estimate.
    initial_framedrop_ = 0;
    has_seen_first_significant_bwe_change_ = true;
  }

  video_sender_.SetChannelParameters(bitrate_bps, fraction_lost,
                                     round_trip_time_ms, rate_allocator_.get(),
                                     bitrate_observer_);

  encoder_start_bitrate_bps_ =
      bitrate_bps != 0 ? bitrate_bps : encoder_start_bitrate_bps_;
  bool video_is_suspended = bitrate_bps == 0;
  bool video_suspension_changed = video_is_suspended != EncoderPaused();
  last_observed_bitrate_bps_ = bitrate_bps;

  if (video_suspension_changed) {
    RTC_LOG(LS_INFO) << "Video suspend state changed to: "
                     << (video_is_suspended ? "suspended" : "not suspended");
    encoder_stats_observer_->OnSuspendChange(video_is_suspended);
  }
  if (video_suspension_changed && !video_is_suspended && pending_frame_ &&
      !DropDueToSize(pending_frame_->size())) {
    int64_t pending_time_us = rtc::TimeMicros() - pending_frame_post_time_us_;
    if (pending_time_us < kPendingFrameTimeoutMs * 1000)
      EncodeVideoFrame(*pending_frame_, pending_frame_post_time_us_);
    pending_frame_.reset();
  }
}

bool VideoStreamEncoder::DropDueToSize(uint32_t pixel_count) const {
  if (initial_framedrop_ < kMaxInitialFramedrop &&
      encoder_start_bitrate_bps_ > 0) {
    if (encoder_start_bitrate_bps_ < 300000 /* qvga */) {
      return pixel_count > 320 * 240;
    } else if (encoder_start_bitrate_bps_ < 500000 /* vga */) {
      return pixel_count > 640 * 480;
    }
  }
  return false;
}

void VideoStreamEncoder::AdaptDown(AdaptReason reason) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  AdaptationRequest adaptation_request = {
      last_frame_info_->pixel_count(),
      encoder_stats_observer_->GetInputFrameRate(),
      AdaptationRequest::Mode::kAdaptDown};

  bool downgrade_requested =
      last_adaptation_request_ &&
      last_adaptation_request_->mode_ == AdaptationRequest::Mode::kAdaptDown;

  switch (degradation_preference_) {
    case DegradationPreference::BALANCED:
      break;
    case DegradationPreference::MAINTAIN_FRAMERATE:
      if (downgrade_requested &&
          adaptation_request.input_pixel_count_ >=
              last_adaptation_request_->input_pixel_count_) {
        // Don't request lower resolution if the current resolution is not
        // lower than the last time we asked for the resolution to be lowered.
        return;
      }
      break;
    case DegradationPreference::MAINTAIN_RESOLUTION:
      if (adaptation_request.framerate_fps_ <= 0 ||
          (downgrade_requested &&
           adaptation_request.framerate_fps_ < kMinFramerateFps)) {
        // If no input fps estimate available, can't determine how to scale down
        // framerate. Otherwise, don't request lower framerate if we don't have
        // a valid frame rate. Since framerate, unlike resolution, is a measure
        // we have to estimate, and can fluctuate naturally over time, don't
        // make the same kind of limitations as for resolution, but trust the
        // overuse detector to not trigger too often.
        return;
      }
      break;
    case DegradationPreference::DISABLED:
      return;
  }

  switch (degradation_preference_) {
    case DegradationPreference::BALANCED: {
      // Try scale down framerate, if lower.
      int fps = MinFps(last_frame_info_->pixel_count());
      if (source_proxy_->RestrictFramerate(fps)) {
        GetAdaptCounter().IncrementFramerate(reason);
        break;
      }
      // Scale down resolution.
      RTC_FALLTHROUGH();
    }
    case DegradationPreference::MAINTAIN_FRAMERATE: {
      // Scale down resolution.
      bool min_pixels_reached = false;
      if (!source_proxy_->RequestResolutionLowerThan(
              adaptation_request.input_pixel_count_,
              encoder_->GetEncoderInfo().scaling_settings.min_pixels_per_frame,
              &min_pixels_reached)) {
        if (min_pixels_reached)
          encoder_stats_observer_->OnMinPixelLimitReached();
        return;
      }
      GetAdaptCounter().IncrementResolution(reason);
      break;
    }
    case DegradationPreference::MAINTAIN_RESOLUTION: {
      // Scale down framerate.
      const int requested_framerate = source_proxy_->RequestFramerateLowerThan(
          adaptation_request.framerate_fps_);
      if (requested_framerate == -1)
        return;
      RTC_DCHECK_NE(max_framerate_, -1);
      overuse_detector_->OnTargetFramerateUpdated(
          std::min(max_framerate_, requested_framerate));
      GetAdaptCounter().IncrementFramerate(reason);
      break;
    }
    case DegradationPreference::DISABLED:
      RTC_NOTREACHED();
  }

  last_adaptation_request_.emplace(adaptation_request);

  UpdateAdaptationStats(reason);

  RTC_LOG(LS_INFO) << GetConstAdaptCounter().ToString();
}

void VideoStreamEncoder::AdaptUp(AdaptReason reason) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);

  const AdaptCounter& adapt_counter = GetConstAdaptCounter();
  int num_downgrades = adapt_counter.TotalCount(reason);
  if (num_downgrades == 0)
    return;
  RTC_DCHECK_GT(num_downgrades, 0);

  AdaptationRequest adaptation_request = {
      last_frame_info_->pixel_count(),
      encoder_stats_observer_->GetInputFrameRate(),
      AdaptationRequest::Mode::kAdaptUp};

  bool adapt_up_requested =
      last_adaptation_request_ &&
      last_adaptation_request_->mode_ == AdaptationRequest::Mode::kAdaptUp;

  if (degradation_preference_ == DegradationPreference::MAINTAIN_FRAMERATE) {
    if (adapt_up_requested &&
        adaptation_request.input_pixel_count_ <=
            last_adaptation_request_->input_pixel_count_) {
      // Don't request higher resolution if the current resolution is not
      // higher than the last time we asked for the resolution to be higher.
      return;
    }
  }

  switch (degradation_preference_) {
    case DegradationPreference::BALANCED: {
      // Try scale up framerate, if higher.
      int fps = MaxFps(last_frame_info_->pixel_count());
      if (source_proxy_->IncreaseFramerate(fps)) {
        GetAdaptCounter().DecrementFramerate(reason, fps);
        // Reset framerate in case of fewer fps steps down than up.
        if (adapt_counter.FramerateCount() == 0 &&
            fps != std::numeric_limits<int>::max()) {
          RTC_LOG(LS_INFO) << "Removing framerate down-scaling setting.";
          source_proxy_->IncreaseFramerate(std::numeric_limits<int>::max());
        }
        break;
      }
      // Scale up resolution.
      RTC_FALLTHROUGH();
    }
    case DegradationPreference::MAINTAIN_FRAMERATE: {
      // Scale up resolution.
      int pixel_count = adaptation_request.input_pixel_count_;
      if (adapt_counter.ResolutionCount() == 1) {
        RTC_LOG(LS_INFO) << "Removing resolution down-scaling setting.";
        pixel_count = std::numeric_limits<int>::max();
      }
      if (!source_proxy_->RequestHigherResolutionThan(pixel_count))
        return;
      GetAdaptCounter().DecrementResolution(reason);
      break;
    }
    case DegradationPreference::MAINTAIN_RESOLUTION: {
      // Scale up framerate.
      int fps = adaptation_request.framerate_fps_;
      if (adapt_counter.FramerateCount() == 1) {
        RTC_LOG(LS_INFO) << "Removing framerate down-scaling setting.";
        fps = std::numeric_limits<int>::max();
      }

      const int requested_framerate =
          source_proxy_->RequestHigherFramerateThan(fps);
      if (requested_framerate == -1) {
        overuse_detector_->OnTargetFramerateUpdated(max_framerate_);
        return;
      }
      overuse_detector_->OnTargetFramerateUpdated(
          std::min(max_framerate_, requested_framerate));
      GetAdaptCounter().DecrementFramerate(reason);
      break;
    }
    case DegradationPreference::DISABLED:
      return;
  }

  last_adaptation_request_.emplace(adaptation_request);

  UpdateAdaptationStats(reason);

  RTC_LOG(LS_INFO) << adapt_counter.ToString();
}

// TODO(nisse): Delete, once AdaptReason and AdaptationReason are merged.
void VideoStreamEncoder::UpdateAdaptationStats(AdaptReason reason) {
  switch (reason) {
    case kCpu:
      encoder_stats_observer_->OnAdaptationChanged(
          VideoStreamEncoderObserver::AdaptationReason::kCpu,
          GetActiveCounts(kCpu), GetActiveCounts(kQuality));
      break;
    case kQuality:
      encoder_stats_observer_->OnAdaptationChanged(
          VideoStreamEncoderObserver::AdaptationReason::kQuality,
          GetActiveCounts(kCpu), GetActiveCounts(kQuality));
      break;
  }
}

VideoStreamEncoderObserver::AdaptationSteps VideoStreamEncoder::GetActiveCounts(
    AdaptReason reason) {
  VideoStreamEncoderObserver::AdaptationSteps counts =
      GetConstAdaptCounter().Counts(reason);
  switch (reason) {
    case kCpu:
      if (!IsFramerateScalingEnabled(degradation_preference_))
        counts.num_framerate_reductions = absl::nullopt;
      if (!IsResolutionScalingEnabled(degradation_preference_))
        counts.num_resolution_reductions = absl::nullopt;
      break;
    case kQuality:
      if (!IsFramerateScalingEnabled(degradation_preference_) ||
          !quality_scaler_) {
        counts.num_framerate_reductions = absl::nullopt;
      }
      if (!IsResolutionScalingEnabled(degradation_preference_) ||
          !quality_scaler_) {
        counts.num_resolution_reductions = absl::nullopt;
      }
      break;
  }
  return counts;
}

VideoStreamEncoder::AdaptCounter& VideoStreamEncoder::GetAdaptCounter() {
  return adapt_counters_[degradation_preference_];
}

const VideoStreamEncoder::AdaptCounter&
VideoStreamEncoder::GetConstAdaptCounter() {
  return adapt_counters_[degradation_preference_];
}

// Class holding adaptation information.
VideoStreamEncoder::AdaptCounter::AdaptCounter() {
  fps_counters_.resize(kScaleReasonSize);
  resolution_counters_.resize(kScaleReasonSize);
  static_assert(kScaleReasonSize == 2, "Update MoveCount.");
}

VideoStreamEncoder::AdaptCounter::~AdaptCounter() {}

std::string VideoStreamEncoder::AdaptCounter::ToString() const {
  rtc::StringBuilder ss;
  ss << "Downgrade counts: fps: {" << ToString(fps_counters_);
  ss << "}, resolution: {" << ToString(resolution_counters_) << "}";
  return ss.Release();
}

VideoStreamEncoderObserver::AdaptationSteps
VideoStreamEncoder::AdaptCounter::Counts(int reason) const {
  VideoStreamEncoderObserver::AdaptationSteps counts;
  counts.num_framerate_reductions = fps_counters_[reason];
  counts.num_resolution_reductions = resolution_counters_[reason];
  return counts;
}

void VideoStreamEncoder::AdaptCounter::IncrementFramerate(int reason) {
  ++(fps_counters_[reason]);
}

void VideoStreamEncoder::AdaptCounter::IncrementResolution(int reason) {
  ++(resolution_counters_[reason]);
}

void VideoStreamEncoder::AdaptCounter::DecrementFramerate(int reason) {
  if (fps_counters_[reason] == 0) {
    // Balanced mode: Adapt up is in a different order, switch reason.
    // E.g. framerate adapt down: quality (2), framerate adapt up: cpu (3).
    // 1. Down resolution (cpu):   res={quality:0,cpu:1}, fps={quality:0,cpu:0}
    // 2. Down fps (quality):      res={quality:0,cpu:1}, fps={quality:1,cpu:0}
    // 3. Up fps (cpu):            res={quality:1,cpu:0}, fps={quality:0,cpu:0}
    // 4. Up resolution (quality): res={quality:0,cpu:0}, fps={quality:0,cpu:0}
    RTC_DCHECK_GT(TotalCount(reason), 0) << "No downgrade for reason.";
    RTC_DCHECK_GT(FramerateCount(), 0) << "Framerate not downgraded.";
    MoveCount(&resolution_counters_, reason);
    MoveCount(&fps_counters_, (reason + 1) % kScaleReasonSize);
  }
  --(fps_counters_[reason]);
  RTC_DCHECK_GE(fps_counters_[reason], 0);
}

void VideoStreamEncoder::AdaptCounter::DecrementResolution(int reason) {
  if (resolution_counters_[reason] == 0) {
    // Balanced mode: Adapt up is in a different order, switch reason.
    RTC_DCHECK_GT(TotalCount(reason), 0) << "No downgrade for reason.";
    RTC_DCHECK_GT(ResolutionCount(), 0) << "Resolution not downgraded.";
    MoveCount(&fps_counters_, reason);
    MoveCount(&resolution_counters_, (reason + 1) % kScaleReasonSize);
  }
  --(resolution_counters_[reason]);
  RTC_DCHECK_GE(resolution_counters_[reason], 0);
}

void VideoStreamEncoder::AdaptCounter::DecrementFramerate(int reason,
                                                          int cur_fps) {
  DecrementFramerate(reason);
  // Reset if at max fps (i.e. in case of fewer steps up than down).
  if (cur_fps == std::numeric_limits<int>::max())
    std::fill(fps_counters_.begin(), fps_counters_.end(), 0);
}

int VideoStreamEncoder::AdaptCounter::FramerateCount() const {
  return Count(fps_counters_);
}

int VideoStreamEncoder::AdaptCounter::ResolutionCount() const {
  return Count(resolution_counters_);
}

int VideoStreamEncoder::AdaptCounter::FramerateCount(int reason) const {
  return fps_counters_[reason];
}

int VideoStreamEncoder::AdaptCounter::ResolutionCount(int reason) const {
  return resolution_counters_[reason];
}

int VideoStreamEncoder::AdaptCounter::TotalCount(int reason) const {
  return FramerateCount(reason) + ResolutionCount(reason);
}

int VideoStreamEncoder::AdaptCounter::Count(
    const std::vector<int>& counters) const {
  return std::accumulate(counters.begin(), counters.end(), 0);
}

void VideoStreamEncoder::AdaptCounter::MoveCount(std::vector<int>* counters,
                                                 int from_reason) {
  int to_reason = (from_reason + 1) % kScaleReasonSize;
  ++((*counters)[to_reason]);
  --((*counters)[from_reason]);
}

std::string VideoStreamEncoder::AdaptCounter::ToString(
    const std::vector<int>& counters) const {
  rtc::StringBuilder ss;
  for (size_t reason = 0; reason < kScaleReasonSize; ++reason) {
    ss << (reason ? " cpu" : "quality") << ":" << counters[reason];
  }
  return ss.Release();
}

}  // namespace webrtc
