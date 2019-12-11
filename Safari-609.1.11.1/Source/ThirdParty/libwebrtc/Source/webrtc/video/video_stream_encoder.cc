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

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_bitrate_allocator_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/codecs/vp9/svc_rate_allocator.h"
#include "modules/video_coding/include/video_codec_initializer.h"
#include "modules/video_coding/include/video_coding.h"
#include "modules/video_coding/utility/default_video_bitrate_allocator.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/alr_experiment.h"
#include "rtc_base/experiments/quality_scaling_experiment.h"
#include "rtc_base/experiments/rate_control_settings.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/system/fallthrough.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {

// Time interval for logging frame counts.
const int64_t kFrameLogIntervalMs = 60000;
const int kMinFramerateFps = 2;

// Time to keep a single cached pending frame in paused state.
const int64_t kPendingFrameTimeoutMs = 1000;

const char kInitialFramedropFieldTrial[] = "WebRTC-InitialFramedrop";
constexpr char kFrameDropperFieldTrial[] = "WebRTC-FrameDropper";

// The maximum number of frames to drop at beginning of stream
// to try and achieve desired bitrate.
const int kMaxInitialFramedrop = 4;
// When the first change in BWE above this threshold occurs,
// enable DropFrameDueToSize logic.
const float kFramedropThreshold = 0.3;

// Averaging window spanning 90 frames at default 30fps, matching old media
// optimization module defaults.
const int64_t kFrameRateAvergingWindowSizeMs = (1000 / 30) * 90;

const size_t kDefaultPayloadSize = 1440;

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

bool RequiresEncoderReset(const VideoCodec& prev_send_codec,
                          const VideoCodec& new_send_codec,
                          bool was_encode_called_since_last_initialization) {
  // Does not check max/minBitrate or maxFramerate.
  if (new_send_codec.codecType != prev_send_codec.codecType ||
      new_send_codec.width != prev_send_codec.width ||
      new_send_codec.height != prev_send_codec.height ||
      new_send_codec.qpMax != prev_send_codec.qpMax ||
      new_send_codec.numberOfSimulcastStreams !=
          prev_send_codec.numberOfSimulcastStreams ||
      new_send_codec.mode != prev_send_codec.mode) {
    return true;
  }

  if (!was_encode_called_since_last_initialization &&
      (new_send_codec.startBitrate != prev_send_codec.startBitrate)) {
    // If start bitrate has changed reconfigure encoder only if encoding had not
    // yet started.
    return true;
  }

  switch (new_send_codec.codecType) {
    case kVideoCodecVP8:
      if (new_send_codec.VP8() != prev_send_codec.VP8()) {
        return true;
      }
      break;

    case kVideoCodecVP9:
      if (new_send_codec.VP9() != prev_send_codec.VP9()) {
        return true;
      }
      break;

    case kVideoCodecH264:
      if (new_send_codec.H264() != prev_send_codec.H264()) {
        return true;
      }
      break;

    default:
      break;
  }

  for (unsigned char i = 0; i < new_send_codec.numberOfSimulcastStreams; ++i) {
    if (new_send_codec.simulcastStream[i].width !=
            prev_send_codec.simulcastStream[i].width ||
        new_send_codec.simulcastStream[i].height !=
            prev_send_codec.simulcastStream[i].height ||
        new_send_codec.simulcastStream[i].numberOfTemporalLayers !=
            prev_send_codec.simulcastStream[i].numberOfTemporalLayers ||
        new_send_codec.simulcastStream[i].qpMax !=
            prev_send_codec.simulcastStream[i].qpMax ||
        new_send_codec.simulcastStream[i].active !=
            prev_send_codec.simulcastStream[i].active) {
      return true;
    }
  }
  return false;
}

std::array<uint8_t, 2> GetExperimentGroups() {
  std::array<uint8_t, 2> experiment_groups;
  absl::optional<AlrExperimentSettings> experiment_settings =
      AlrExperimentSettings::CreateFromFieldTrial(
          AlrExperimentSettings::kStrictPacingAndProbingExperimentName);
  if (experiment_settings) {
    experiment_groups[0] = experiment_settings->group_id + 1;
  } else {
    experiment_groups[0] = 0;
  }
  experiment_settings = AlrExperimentSettings::CreateFromFieldTrial(
      AlrExperimentSettings::kScreenshareProbingBweExperimentName);
  if (experiment_settings) {
    experiment_groups[1] = experiment_settings->group_id + 1;
  } else {
    experiment_groups[1] = 0;
  }
  return experiment_groups;
}

// Limit allocation across TLs in bitrate allocation according to number of TLs
// in EncoderInfo.
VideoBitrateAllocation UpdateAllocationFromEncoderInfo(
    const VideoBitrateAllocation& allocation,
    const VideoEncoder::EncoderInfo& encoder_info) {
  if (allocation.get_sum_bps() == 0) {
    return allocation;
  }
  VideoBitrateAllocation new_allocation;
  for (int si = 0; si < kMaxSpatialLayers; ++si) {
    if (encoder_info.fps_allocation[si].size() == 1 &&
        allocation.IsSpatialLayerUsed(si)) {
      // One TL is signalled to be used by the encoder. Do not distribute
      // bitrate allocation across TLs (use sum at ti:0).
      new_allocation.SetBitrate(si, 0, allocation.GetSpatialLayerSum(si));
    } else {
      for (int ti = 0; ti < kMaxTemporalStreams; ++ti) {
        if (allocation.HasBitrate(si, ti))
          new_allocation.SetBitrate(si, ti, allocation.GetBitrate(si, ti));
      }
    }
  }
  return new_allocation;
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
    RTC_DCHECK_RUN_ON(&main_checker_);
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
  SequenceChecker main_checker_;
  VideoStreamEncoder* const video_stream_encoder_;
  rtc::VideoSinkWants sink_wants_ RTC_GUARDED_BY(&crit_);
  DegradationPreference degradation_preference_ RTC_GUARDED_BY(&crit_);
  rtc::VideoSourceInterface<VideoFrame>* source_ RTC_GUARDED_BY(&crit_);
  int max_framerate_ RTC_GUARDED_BY(&crit_);

  RTC_DISALLOW_COPY_AND_ASSIGN(VideoSourceProxy);
};

VideoStreamEncoder::EncoderRateSettings::EncoderRateSettings()
    : VideoEncoder::RateControlParameters(), encoder_target(DataRate::Zero()) {}

VideoStreamEncoder::EncoderRateSettings::EncoderRateSettings(
    const VideoBitrateAllocation& bitrate,
    double framerate_fps,
    DataRate bandwidth_allocation,
    DataRate encoder_target)
    : VideoEncoder::RateControlParameters(bitrate,
                                          framerate_fps,
                                          bandwidth_allocation),
      encoder_target(encoder_target) {}

bool VideoStreamEncoder::EncoderRateSettings::operator==(
    const EncoderRateSettings& rhs) const {
  return bitrate == rhs.bitrate && framerate_fps == rhs.framerate_fps &&
         bandwidth_allocation == rhs.bandwidth_allocation &&
         encoder_target == rhs.encoder_target;
}

bool VideoStreamEncoder::EncoderRateSettings::operator!=(
    const EncoderRateSettings& rhs) const {
  return !(*this == rhs);
}

VideoStreamEncoder::VideoStreamEncoder(
    Clock* clock,
    uint32_t number_of_cores,
    VideoStreamEncoderObserver* encoder_stats_observer,
    const VideoStreamEncoderSettings& settings,
    std::unique_ptr<OveruseFrameDetector> overuse_detector,
    TaskQueueFactory* task_queue_factory)
    : shutdown_event_(true /* manual_reset */, false),
      number_of_cores_(number_of_cores),
      initial_framedrop_(0),
      initial_framedrop_on_bwe_enabled_(
          webrtc::field_trial::IsEnabled(kInitialFramedropFieldTrial)),
      quality_scaling_experiment_enabled_(QualityScalingExperiment::Enabled()),
      source_proxy_(new VideoSourceProxy(this)),
      sink_(nullptr),
      settings_(settings),
      rate_control_settings_(RateControlSettings::ParseFromFieldTrials()),
      quality_scaler_settings_(QualityScalerSettings::ParseFromFieldTrials()),
      overuse_detector_(std::move(overuse_detector)),
      encoder_stats_observer_(encoder_stats_observer),
      encoder_initialized_(false),
      max_framerate_(-1),
      pending_encoder_reconfiguration_(false),
      pending_encoder_creation_(false),
      crop_width_(0),
      crop_height_(0),
      encoder_start_bitrate_bps_(0),
      set_start_bitrate_bps_(0),
      set_start_bitrate_time_ms_(0),
      has_seen_first_bwe_drop_(false),
      max_data_payload_length_(0),
      encoder_paused_and_dropped_frame_(false),
      was_encode_called_since_last_initialization_(false),
      encoder_failed_(false),
      clock_(clock),
      degradation_preference_(DegradationPreference::DISABLED),
      posted_frames_waiting_for_encode_(0),
      last_captured_timestamp_(0),
      delta_ntp_internal_ms_(clock_->CurrentNtpInMilliseconds() -
                             clock_->TimeInMilliseconds()),
      last_frame_log_ms_(clock_->TimeInMilliseconds()),
      captured_frame_count_(0),
      dropped_frame_count_(0),
      pending_frame_post_time_us_(0),
      accumulated_update_rect_{0, 0, 0, 0},
      bitrate_observer_(nullptr),
      fec_controller_override_(nullptr),
      force_disable_frame_dropper_(false),
      input_framerate_(kFrameRateAvergingWindowSizeMs, 1000),
      pending_frame_drops_(0),
      next_frame_types_(1, VideoFrameType::kVideoFrameDelta),
      frame_encode_metadata_writer_(this),
      experiment_groups_(GetExperimentGroups()),
      next_frame_id_(0),
      encoder_queue_(task_queue_factory->CreateTaskQueue(
          "EncoderQueue",
          TaskQueueFactory::Priority::NORMAL)) {
  RTC_DCHECK(encoder_stats_observer);
  RTC_DCHECK(overuse_detector_);
  RTC_DCHECK_GE(number_of_cores, 1);

  for (auto& state : encoder_buffer_state_)
    state.fill(std::numeric_limits<int64_t>::max());
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
    rate_allocator_ = nullptr;
    bitrate_observer_ = nullptr;
    ReleaseEncoder();
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

void VideoStreamEncoder::SetFecControllerOverride(
    FecControllerOverride* fec_controller_override) {
  encoder_queue_.PostTask([this, fec_controller_override] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    RTC_DCHECK(!fec_controller_override_);
    fec_controller_override_ = fec_controller_override;
    if (encoder_) {
      encoder_->SetFecControllerOverride(fec_controller_override_);
    }
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
      ConfigureQualityScaler(encoder_->GetEncoderInfo());

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
    set_start_bitrate_bps_ = start_bitrate_bps;
    set_start_bitrate_time_ms_ = clock_->TimeInMilliseconds();
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

  pending_encoder_creation_ =
      (!encoder_ || encoder_config_.video_format != config.video_format ||
       max_data_payload_length_ != max_data_payload_length);
  encoder_config_ = std::move(config);
  max_data_payload_length_ = max_data_payload_length;
  pending_encoder_reconfiguration_ = true;

  // Reconfigure the encoder now if the encoder has an internal source or
  // if the frame resolution is known. Otherwise, the reconfiguration is
  // deferred until the next frame to minimize the number of reconfigurations.
  // The codec configuration depends on incoming video frame size.
  if (last_frame_info_) {
    ReconfigureEncoder();
  } else {
    codec_info_ = settings_.encoder_factory->QueryVideoEncoder(
        encoder_config_.video_format);
    if (HasInternalSource()) {
      last_frame_info_ = VideoFrameInfo(176, 144, false);
      ReconfigureEncoder();
    }
  }
}

static absl::optional<VideoEncoder::ResolutionBitrateLimits>
GetEncoderBitrateLimits(const VideoEncoder::EncoderInfo& encoder_info,
                        int frame_size_pixels) {
  std::vector<VideoEncoder::ResolutionBitrateLimits> bitrate_limits =
      encoder_info.resolution_bitrate_limits;

  // Sort the list of bitrate limits by resolution.
  sort(bitrate_limits.begin(), bitrate_limits.end(),
       [](const VideoEncoder::ResolutionBitrateLimits& lhs,
          const VideoEncoder::ResolutionBitrateLimits& rhs) {
         return lhs.frame_size_pixels < rhs.frame_size_pixels;
       });

  for (size_t i = 0; i < bitrate_limits.size(); ++i) {
    RTC_DCHECK_GT(bitrate_limits[i].min_bitrate_bps, 0);
    RTC_DCHECK_GE(bitrate_limits[i].min_start_bitrate_bps,
                  bitrate_limits[i].min_bitrate_bps);
    RTC_DCHECK_GT(bitrate_limits[i].max_bitrate_bps,
                  bitrate_limits[i].min_start_bitrate_bps);
    if (i > 0) {
      // The bitrate limits aren't expected to decrease with resolution.
      RTC_DCHECK_GE(bitrate_limits[i].min_bitrate_bps,
                    bitrate_limits[i - 1].min_bitrate_bps);
      RTC_DCHECK_GE(bitrate_limits[i].min_start_bitrate_bps,
                    bitrate_limits[i - 1].min_start_bitrate_bps);
      RTC_DCHECK_GE(bitrate_limits[i].max_bitrate_bps,
                    bitrate_limits[i - 1].max_bitrate_bps);
    }

    if (bitrate_limits[i].frame_size_pixels >= frame_size_pixels) {
      return absl::optional<VideoEncoder::ResolutionBitrateLimits>(
          bitrate_limits[i]);
    }
  }

  return absl::nullopt;
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
  auto highest_stream = absl::c_max_element(
      streams, [](const webrtc::VideoStream& a, const webrtc::VideoStream& b) {
        return std::tie(a.width, a.height) < std::tie(b.width, b.height);
      });
  int highest_stream_width = static_cast<int>(highest_stream->width);
  int highest_stream_height = static_cast<int>(highest_stream->height);
  // Dimension may be reduced to be, e.g. divisible by 4.
  RTC_CHECK_GE(last_frame_info_->width, highest_stream_width);
  RTC_CHECK_GE(last_frame_info_->height, highest_stream_height);
  crop_width_ = last_frame_info_->width - highest_stream_width;
  crop_height_ = last_frame_info_->height - highest_stream_height;

  bool encoder_reset_required = false;
  if (pending_encoder_creation_) {
    // Destroy existing encoder instance before creating a new one. Otherwise
    // attempt to create another instance will fail if encoder factory
    // supports only single instance of encoder of given type.
    encoder_.reset();

    encoder_ = settings_.encoder_factory->CreateVideoEncoder(
        encoder_config_.video_format);
    // TODO(nisse): What to do if creating the encoder fails? Crash,
    // or just discard incoming frames?
    RTC_CHECK(encoder_);

    encoder_->SetFecControllerOverride(fec_controller_override_);

    codec_info_ = settings_.encoder_factory->QueryVideoEncoder(
        encoder_config_.video_format);

    encoder_reset_required = true;
  }

  encoder_bitrate_limits_ = GetEncoderBitrateLimits(
      encoder_->GetEncoderInfo(),
      last_frame_info_->width * last_frame_info_->height);

  if (streams.size() == 1 && encoder_bitrate_limits_) {
    // Use bitrate limits recommended by encoder only if app didn't set any of
    // them.
    if (encoder_config_.max_bitrate_bps <= 0 &&
        (encoder_config_.simulcast_layers.empty() ||
         encoder_config_.simulcast_layers[0].min_bitrate_bps <= 0)) {
      streams.back().min_bitrate_bps = encoder_bitrate_limits_->min_bitrate_bps;
      streams.back().max_bitrate_bps = encoder_bitrate_limits_->max_bitrate_bps;
      streams.back().target_bitrate_bps =
          std::min(streams.back().target_bitrate_bps,
                   encoder_bitrate_limits_->max_bitrate_bps);
    }
  }

  VideoCodec codec;
  if (!VideoCodecInitializer::SetupCodec(encoder_config_, streams, &codec)) {
    RTC_LOG(LS_ERROR) << "Failed to create encoder configuration.";
  }

  // Set min_bitrate_bps, max_bitrate_bps, and max padding bit rate for VP9.
  if (encoder_config_.codec_type == kVideoCodecVP9) {
    // Lower max bitrate to the level codec actually can produce.
    streams[0].max_bitrate_bps =
        std::min(streams[0].max_bitrate_bps,
                 SvcRateAllocator::GetMaxBitrate(codec).bps<int>());
    streams[0].min_bitrate_bps = codec.spatialLayers[0].minBitrate * 1000;
    // target_bitrate_bps specifies the maximum padding bitrate.
    streams[0].target_bitrate_bps =
        SvcRateAllocator::GetPaddingBitrate(codec).bps<int>();
  }

  codec.startBitrate =
      std::max(encoder_start_bitrate_bps_ / 1000, codec.minBitrate);
  codec.startBitrate = std::min(codec.startBitrate, codec.maxBitrate);
  codec.expect_encode_from_texture = last_frame_info_->is_texture;
  // Make sure the start bit rate is sane...
  RTC_DCHECK_LE(codec.startBitrate, 1000000);
  max_framerate_ = codec.maxFramerate;

  // Inform source about max configured framerate.
  int max_framerate = 0;
  for (const auto& stream : streams) {
    max_framerate = std::max(stream.max_framerate, max_framerate);
  }
  source_proxy_->SetMaxFramerate(max_framerate);

  if (codec.maxBitrate == 0) {
    // max is one bit per pixel
    codec.maxBitrate =
        (static_cast<int>(codec.height) * static_cast<int>(codec.width) *
         static_cast<int>(codec.maxFramerate)) /
        1000;
    if (codec.startBitrate > codec.maxBitrate) {
      // But if the user tries to set a higher start bit rate we will
      // increase the max accordingly.
      codec.maxBitrate = codec.startBitrate;
    }
  }

  if (codec.startBitrate > codec.maxBitrate) {
    codec.startBitrate = codec.maxBitrate;
  }

  rate_allocator_ =
      settings_.bitrate_allocator_factory->CreateVideoBitrateAllocator(codec);

  // Reset (release existing encoder) if one exists and anything except
  // start bitrate or max framerate has changed.
  if (!encoder_reset_required) {
    encoder_reset_required = RequiresEncoderReset(
        codec, send_codec_, was_encode_called_since_last_initialization_);
  }
  send_codec_ = codec;

  // Keep the same encoder, as long as the video_format is unchanged.
  // Encoder creation block is split in two since EncoderInfo needed to start
  // CPU adaptation with the correct settings should be polled after
  // encoder_->InitEncode().
  bool success = true;
  if (encoder_reset_required) {
    ReleaseEncoder();
    const size_t max_data_payload_length = max_data_payload_length_ > 0
                                               ? max_data_payload_length_
                                               : kDefaultPayloadSize;
    if (encoder_->InitEncode(
            &send_codec_,
            VideoEncoder::Settings(settings_.capabilities, number_of_cores_,
                                   max_data_payload_length)) != 0) {
      RTC_LOG(LS_ERROR) << "Failed to initialize the encoder associated with "
                           "codec type: "
                        << CodecTypeToPayloadString(send_codec_.codecType)
                        << " (" << send_codec_.codecType << ")";
      ReleaseEncoder();
      success = false;
    } else {
      encoder_initialized_ = true;
      encoder_->RegisterEncodeCompleteCallback(this);
      frame_encode_metadata_writer_.OnEncoderInit(send_codec_,
                                                  HasInternalSource());
    }

    frame_encode_metadata_writer_.Reset();
    last_encode_info_ms_ = absl::nullopt;
    was_encode_called_since_last_initialization_ = false;
  }

  if (success) {
    next_frame_types_.clear();
    next_frame_types_.resize(
        std::max(static_cast<int>(codec.numberOfSimulcastStreams), 1),
        VideoFrameType::kVideoFrameKey);
    RTC_LOG(LS_VERBOSE) << " max bitrate " << codec.maxBitrate
                        << " start bitrate " << codec.startBitrate
                        << " max frame rate " << codec.maxFramerate
                        << " max payload size " << max_data_payload_length_;
  } else {
    RTC_LOG(LS_ERROR) << "Failed to configure encoder.";
    rate_allocator_ = nullptr;
  }

  if (pending_encoder_creation_) {
    overuse_detector_->StopCheckForOveruse();
    overuse_detector_->StartCheckForOveruse(
        &encoder_queue_,
        GetCpuOveruseOptions(
            settings_, encoder_->GetEncoderInfo().is_hardware_accelerated),
        this);
    pending_encoder_creation_ = false;
  }

  int num_layers;
  if (codec.codecType == kVideoCodecVP8) {
    num_layers = codec.VP8()->numberOfTemporalLayers;
  } else if (codec.codecType == kVideoCodecVP9) {
    num_layers = codec.VP9()->numberOfTemporalLayers;
  } else if (codec.codecType == kVideoCodecH264) {
    num_layers = codec.H264()->numberOfTemporalLayers;
  } else if (codec.codecType == kVideoCodecGeneric &&
             codec.numberOfSimulcastStreams > 0) {
    // This is mainly for unit testing, disabling frame dropping.
    // TODO(sprang): Add a better way to disable frame dropping.
    num_layers = codec.simulcastStream[0].numberOfTemporalLayers;
  } else {
    num_layers = 1;
  }

  frame_dropper_.Reset();
  frame_dropper_.SetRates(codec.startBitrate, max_framerate_);
  // Force-disable frame dropper if either:
  //  * We have screensharing with layers.
  //  * "WebRTC-FrameDropper" field trial is "Disabled".
  force_disable_frame_dropper_ =
      field_trial::IsDisabled(kFrameDropperFieldTrial) ||
      (num_layers > 1 && codec.mode == VideoCodecMode::kScreensharing);

  VideoEncoder::EncoderInfo info = encoder_->GetEncoderInfo();
  if (rate_control_settings_.UseEncoderBitrateAdjuster()) {
    bitrate_adjuster_ = absl::make_unique<EncoderBitrateAdjuster>(codec);
    bitrate_adjuster_->OnEncoderInfo(info);
  }

  if (rate_allocator_ && last_encoder_rate_settings_) {
    // We have a new rate allocator instance and already configured target
    // bitrate. Update the rate allocation and notify observers.
    last_encoder_rate_settings_->framerate_fps = GetInputFramerateFps();
    SetEncoderRates(
        UpdateBitrateAllocationAndNotifyObserver(*last_encoder_rate_settings_));
  }

  encoder_stats_observer_->OnEncoderReconfigured(encoder_config_, streams);

  pending_encoder_reconfiguration_ = false;

  sink_->OnEncoderConfigurationChanged(
      std::move(streams), encoder_config_.content_type,
      encoder_config_.min_transmit_bitrate_bps);

  // Get the current target framerate, ie the maximum framerate as specified by
  // the current codec configuration, or any limit imposed by cpu adaption in
  // maintain-resolution or balanced mode. This is used to make sure overuse
  // detection doesn't needlessly trigger in low and/or variable framerate
  // scenarios.
  int target_framerate = std::min(
      max_framerate_, source_proxy_->GetActiveSinkWants().max_framerate_fps);
  overuse_detector_->OnTargetFramerateUpdated(target_framerate);

  ConfigureQualityScaler(info);
}

void VideoStreamEncoder::ConfigureQualityScaler(
    const VideoEncoder::EncoderInfo& encoder_info) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  const auto scaling_settings = encoder_info.scaling_settings;
  const bool quality_scaling_allowed =
      IsResolutionScalingEnabled(degradation_preference_) &&
      scaling_settings.thresholds;

  if (quality_scaling_allowed) {
    if (quality_scaler_ == nullptr) {
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
          &encoder_queue_, observer,
          experimental_thresholds ? *experimental_thresholds
                                  : *(scaling_settings.thresholds));
      has_seen_first_significant_bwe_change_ = false;
      initial_framedrop_ = 0;
    }
  } else {
    quality_scaler_.reset(nullptr);
    initial_framedrop_ = kMaxInitialFramedrop;
  }

  if (degradation_preference_ == DegradationPreference::BALANCED &&
      quality_scaler_ && last_frame_info_) {
    absl::optional<VideoEncoder::QpThresholds> thresholds =
        balanced_settings_.GetQpThresholds(encoder_config_.codec_type,
                                           last_frame_info_->pixel_count());
    if (thresholds) {
      quality_scaler_->SetQpThresholds(*thresholds);
    }
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
    encoder_queue_.PostTask([this, incoming_frame]() {
      RTC_DCHECK_RUN_ON(&encoder_queue_);
      accumulated_update_rect_.Union(incoming_frame.update_rect());
    });
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
          accumulated_update_rect_.Union(incoming_frame.update_rect());
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
  // |last_encoder_rate_settings_->encoder_target| will be 0.
  return !last_encoder_rate_settings_ ||
         last_encoder_rate_settings_->encoder_target == DataRate::Zero();
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

VideoStreamEncoder::EncoderRateSettings
VideoStreamEncoder::UpdateBitrateAllocationAndNotifyObserver(
    const EncoderRateSettings& rate_settings) {
  VideoBitrateAllocation new_allocation;
  // Only call allocators if bitrate > 0 (ie, not suspended), otherwise they
  // might cap the bitrate to the min bitrate configured.
  if (rate_allocator_ && rate_settings.encoder_target > DataRate::Zero()) {
    new_allocation = rate_allocator_->Allocate(VideoBitrateAllocationParameters(
        rate_settings.encoder_target.bps(),
        static_cast<uint32_t>(rate_settings.framerate_fps + 0.5)));
  }

  if (bitrate_observer_ && new_allocation.get_sum_bps() > 0) {
    if (encoder_ && encoder_initialized_) {
      // Avoid too old encoder_info_.
      const int64_t kMaxDiffMs = 100;
      const bool updated_recently =
          (last_encode_info_ms_ && ((clock_->TimeInMilliseconds() -
                                     *last_encode_info_ms_) < kMaxDiffMs));
      // Update allocation according to info from encoder.
      bitrate_observer_->OnBitrateAllocationUpdated(
          UpdateAllocationFromEncoderInfo(
              new_allocation,
              updated_recently ? encoder_info_ : encoder_->GetEncoderInfo()));
    } else {
      bitrate_observer_->OnBitrateAllocationUpdated(new_allocation);
    }
  }

  EncoderRateSettings new_rate_settings = rate_settings;
  new_rate_settings.bitrate = new_allocation;
  // VideoBitrateAllocator subclasses may allocate a bitrate higher than the
  // target in order to sustain the min bitrate of the video codec. In this
  // case, make sure the bandwidth allocation is at least equal the allocation
  // as that is part of the document contract for that field.
  new_rate_settings.bandwidth_allocation =
      std::max(new_rate_settings.bandwidth_allocation,
               DataRate::bps(new_rate_settings.bitrate.get_sum_bps()));

  if (bitrate_adjuster_) {
    VideoBitrateAllocation adjusted_allocation =
        bitrate_adjuster_->AdjustRateAllocation(new_rate_settings);
    RTC_LOG(LS_VERBOSE) << "Adjusting allocation, fps = "
                        << rate_settings.framerate_fps << ", from "
                        << new_allocation.ToString() << ", to "
                        << adjusted_allocation.ToString();
    new_rate_settings.bitrate = adjusted_allocation;
  }

  return new_rate_settings;
}

uint32_t VideoStreamEncoder::GetInputFramerateFps() {
  const uint32_t default_fps = max_framerate_ != -1 ? max_framerate_ : 30;
  absl::optional<uint32_t> input_fps =
      input_framerate_.Rate(clock_->TimeInMilliseconds());
  if (!input_fps || *input_fps == 0) {
    return default_fps;
  }
  return *input_fps;
}

void VideoStreamEncoder::SetEncoderRates(
    const EncoderRateSettings& rate_settings) {
  RTC_DCHECK_GT(rate_settings.framerate_fps, 0.0);
  const bool settings_changes = !last_encoder_rate_settings_ ||
                                rate_settings != *last_encoder_rate_settings_;
  if (settings_changes) {
    last_encoder_rate_settings_ = rate_settings;
  }

  if (!encoder_) {
    return;
  }

  // |bitrate_allocation| is 0 it means that the network is down or the send
  // pacer is full. We currently only report this if the encoder has an internal
  // source. If the encoder does not have an internal source, higher levels
  // are expected to not call AddVideoFrame. We do this since its unclear
  // how current encoder implementations behave when given a zero target
  // bitrate.
  // TODO(perkj): Make sure all known encoder implementations handle zero
  // target bitrate and remove this check.
  if (!HasInternalSource() && rate_settings.bitrate.get_sum_bps() == 0) {
    return;
  }

  if (settings_changes) {
    encoder_->SetRates(rate_settings);
    frame_encode_metadata_writer_.OnSetRates(
        rate_settings.bitrate,
        static_cast<uint32_t>(rate_settings.framerate_fps + 0.5));
  }
}

void VideoStreamEncoder::MaybeEncodeVideoFrame(const VideoFrame& video_frame,
                                               int64_t time_when_posted_us) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);

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
    // Force full frame update, since resolution has changed.
    accumulated_update_rect_ =
        VideoFrame::UpdateRect{0, 0, video_frame.width(), video_frame.height()};
  }

  // We have to create then encoder before the frame drop logic,
  // because the latter depends on encoder_->GetScalingSettings.
  // According to the testcase
  // InitialFrameDropOffWhenEncoderDisabledScaling, the return value
  // from GetScalingSettings should enable or disable the frame drop.

  // Update input frame rate before we start using it. If we update it after
  // any potential frame drop we are going to artificially increase frame sizes.
  // Poll the rate before updating, otherwise we risk the rate being estimated
  // a little too high at the start of the call when then window is small.
  uint32_t framerate_fps = GetInputFramerateFps();
  input_framerate_.Update(1u, clock_->TimeInMilliseconds());

  int64_t now_ms = clock_->TimeInMilliseconds();
  if (pending_encoder_reconfiguration_) {
    ReconfigureEncoder();
    last_parameters_update_ms_.emplace(now_ms);
  } else if (!last_parameters_update_ms_ ||
             now_ms - *last_parameters_update_ms_ >=
                 vcm::VCMProcessTimer::kDefaultProcessIntervalMs) {
    if (last_encoder_rate_settings_) {
      // Clone rate settings before update, so that SetEncoderRates() will
      // actually detect the change between the input and
      // |last_encoder_rate_setings_|, triggering the call to SetRate() on the
      // encoder.
      EncoderRateSettings new_rate_settings = *last_encoder_rate_settings_;
      new_rate_settings.framerate_fps = static_cast<double>(framerate_fps);
      SetEncoderRates(
          UpdateBitrateAllocationAndNotifyObserver(new_rate_settings));
    }
    last_parameters_update_ms_.emplace(now_ms);
  }

  // Because pending frame will be dropped in any case, we need to
  // remember its updated region.
  if (pending_frame_) {
    encoder_stats_observer_->OnFrameDropped(
        VideoStreamEncoderObserver::DropReason::kEncoderQueue);
    accumulated_update_rect_.Union(pending_frame_->update_rect());
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
      accumulated_update_rect_.Union(video_frame.update_rect());
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
      accumulated_update_rect_.Union(video_frame.update_rect());
    }
    return;
  }

  pending_frame_.reset();

  frame_dropper_.Leak(framerate_fps);
  // Frame dropping is enabled iff frame dropping is not force-disabled, and
  // rate controller is not trusted.
  const bool frame_dropping_enabled =
      !force_disable_frame_dropper_ &&
      !encoder_info_.has_trusted_rate_controller;
  frame_dropper_.Enable(frame_dropping_enabled);
  if (frame_dropping_enabled && frame_dropper_.DropFrame()) {
    RTC_LOG(LS_VERBOSE)
        << "Drop Frame: "
        << "target bitrate "
        << (last_encoder_rate_settings_
                ? last_encoder_rate_settings_->encoder_target.bps()
                : 0)
        << ", input frame rate " << framerate_fps;
    OnDroppedFrame(
        EncodedImageCallback::DropReason::kDroppedByMediaOptimizations);
    accumulated_update_rect_.Union(video_frame.update_rect());
    return;
  }

  EncodeVideoFrame(video_frame, time_when_posted_us);
}

void VideoStreamEncoder::EncodeVideoFrame(const VideoFrame& video_frame,
                                          int64_t time_when_posted_us) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);

  // If the encoder fail we can't continue to encode frames. When this happens
  // the WebrtcVideoSender is notified and the whole VideoSendStream is
  // recreated.
  if (encoder_failed_)
    return;

  TraceFrameDropEnd();

  VideoFrame out_frame(video_frame);
  // Crop frame if needed.
  if (crop_width_ > 0 || crop_height_ > 0) {
    // If the frame can't be converted to I420, drop it.
    auto i420_buffer = video_frame.video_frame_buffer()->ToI420();
    if (!i420_buffer) {
      RTC_LOG(LS_ERROR) << "Frame conversion for crop failed, dropping frame.";
      return;
    }
    int cropped_width = video_frame.width() - crop_width_;
    int cropped_height = video_frame.height() - crop_height_;
    rtc::scoped_refptr<I420Buffer> cropped_buffer =
        I420Buffer::Create(cropped_width, cropped_height);
    // TODO(ilnik): Remove scaling if cropping is too big, as it should never
    // happen after SinkWants signaled correctly from ReconfigureEncoder.
    VideoFrame::UpdateRect update_rect = video_frame.update_rect();
    if (crop_width_ < 4 && crop_height_ < 4) {
      cropped_buffer->CropAndScaleFrom(*i420_buffer, crop_width_ / 2,
                                       crop_height_ / 2, cropped_width,
                                       cropped_height);
      update_rect.offset_x -= crop_width_ / 2;
      update_rect.offset_y -= crop_height_ / 2;
      update_rect.Intersect(
          VideoFrame::UpdateRect{0, 0, cropped_width, cropped_height});

    } else {
      cropped_buffer->ScaleFrom(*i420_buffer);
      if (!update_rect.IsEmpty()) {
        // Since we can't reason about pixels after scaling, we invalidate whole
        // picture, if anything changed.
        update_rect =
            VideoFrame::UpdateRect{0, 0, cropped_width, cropped_height};
      }
    }
    out_frame.set_video_frame_buffer(cropped_buffer);
    out_frame.set_update_rect(update_rect);
    out_frame.set_ntp_time_ms(video_frame.ntp_time_ms());
    // Since accumulated_update_rect_ is constructed before cropping,
    // we can't trust it. If any changes were pending, we invalidate whole
    // frame here.
    if (!accumulated_update_rect_.IsEmpty()) {
      accumulated_update_rect_ =
          VideoFrame::UpdateRect{0, 0, out_frame.width(), out_frame.height()};
    }
  }

  if (!accumulated_update_rect_.IsEmpty()) {
    accumulated_update_rect_.Union(out_frame.update_rect());
    accumulated_update_rect_.Intersect(
        VideoFrame::UpdateRect{0, 0, out_frame.width(), out_frame.height()});
    out_frame.set_update_rect(accumulated_update_rect_);
    accumulated_update_rect_.MakeEmptyUpdate();
  }

  TRACE_EVENT_ASYNC_STEP0("webrtc", "Video", video_frame.render_time_ms(),
                          "Encode");

  overuse_detector_->FrameCaptured(out_frame, time_when_posted_us);

  // Encoder metadata needs to be updated before encode complete callback.
  VideoEncoder::EncoderInfo info = encoder_->GetEncoderInfo();
  if (info.implementation_name != encoder_info_.implementation_name) {
    encoder_stats_observer_->OnEncoderImplementationChanged(
        info.implementation_name);
    if (bitrate_adjuster_) {
      // Encoder implementation changed, reset overshoot detector states.
      bitrate_adjuster_->Reset();
    }
  }

  if (bitrate_adjuster_) {
    for (size_t si = 0; si < kMaxSpatialLayers; ++si) {
      if (info.fps_allocation[si] != encoder_info_.fps_allocation[si]) {
        bitrate_adjuster_->OnEncoderInfo(info);
        break;
      }
    }
  }

  encoder_info_ = info;
  last_encode_info_ms_ = clock_->TimeInMilliseconds();
  RTC_DCHECK_EQ(send_codec_.width, out_frame.width());
  RTC_DCHECK_EQ(send_codec_.height, out_frame.height());
  const VideoFrameBuffer::Type buffer_type =
      out_frame.video_frame_buffer()->type();
  const bool is_buffer_type_supported =
      buffer_type == VideoFrameBuffer::Type::kI420 ||
      (buffer_type == VideoFrameBuffer::Type::kNative &&
       info.supports_native_handle);

  if (!is_buffer_type_supported) {
    // This module only supports software encoding.
    rtc::scoped_refptr<I420BufferInterface> converted_buffer(
        out_frame.video_frame_buffer()->ToI420());

    if (!converted_buffer) {
      RTC_LOG(LS_ERROR) << "Frame conversion failed, dropping frame.";
      return;
    }

    VideoFrame::UpdateRect update_rect = out_frame.update_rect();
    if (!update_rect.IsEmpty() &&
        out_frame.video_frame_buffer()->GetI420() == nullptr) {
      // UpdatedRect is reset to full update if it's not empty, and buffer was
      // converted, therefore we can't guarantee that pixels outside of
      // UpdateRect didn't change comparing to the previous frame.
      update_rect =
          VideoFrame::UpdateRect{0, 0, out_frame.width(), out_frame.height()};
    }

    out_frame.set_video_frame_buffer(converted_buffer);
    out_frame.set_update_rect(update_rect);
  }

  TRACE_EVENT1("webrtc", "VCMGenericEncoder::Encode", "timestamp",
               out_frame.timestamp());

  frame_encode_metadata_writer_.OnEncodeStarted(out_frame);

  const int32_t encode_status = encoder_->Encode(out_frame, &next_frame_types_);
  was_encode_called_since_last_initialization_ = true;

  if (encode_status < 0) {
    if (encode_status == WEBRTC_VIDEO_CODEC_ENCODER_FAILURE) {
      RTC_LOG(LS_ERROR) << "Encoder failed, failing encoder format: "
                        << encoder_config_.video_format.ToString();
      if (settings_.encoder_failure_callback) {
        encoder_failed_ = true;
        settings_.encoder_failure_callback->OnEncoderFailure();
      } else {
        RTC_LOG(LS_ERROR)
            << "Encoder failed but no encoder fallback callback is registered";
      }
    } else {
      RTC_LOG(LS_ERROR) << "Failed to encode frame. Error code: "
                        << encode_status;
    }

    return;
  }

  for (auto& it : next_frame_types_) {
    it = VideoFrameType::kVideoFrameDelta;
  }
}

void VideoStreamEncoder::SendKeyFrame() {
  if (!encoder_queue_.IsCurrent()) {
    encoder_queue_.PostTask([this] { SendKeyFrame(); });
    return;
  }
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  TRACE_EVENT0("webrtc", "OnKeyFrameRequest");
  RTC_DCHECK(!next_frame_types_.empty());

  // TODO(webrtc:10615): Map keyframe request to spatial layer.
  std::fill(next_frame_types_.begin(), next_frame_types_.end(),
            VideoFrameType::kVideoFrameKey);

  if (HasInternalSource()) {
    // Try to request the frame if we have an external encoder with
    // internal source since AddVideoFrame never will be called.

    // TODO(nisse): Used only with internal source. Delete as soon as
    // that feature is removed. The only implementation I've been able
    // to find ignores what's in the frame. With one exception: It seems
    // a few test cases, e.g.,
    // VideoSendStreamTest.VideoSendStreamStopSetEncoderRateToZero, set
    // internal_source to true and use FakeEncoder. And the latter will
    // happily encode this 1x1 frame and pass it on down the pipeline.
    if (encoder_->Encode(VideoFrame::Builder()
                             .set_video_frame_buffer(I420Buffer::Create(1, 1))
                             .set_rotation(kVideoRotation_0)
                             .set_timestamp_us(0)
                             .build(),
                         &next_frame_types_) == WEBRTC_VIDEO_CODEC_OK) {
      // Try to remove just-performed keyframe request, if stream still exists.
      std::fill(next_frame_types_.begin(), next_frame_types_.end(),
                VideoFrameType::kVideoFrameDelta);
    }
  }
}

void VideoStreamEncoder::OnLossNotification(
    const VideoEncoder::LossNotification& loss_notification) {
  if (!encoder_queue_.IsCurrent()) {
    encoder_queue_.PostTask(
        [this, loss_notification] { OnLossNotification(loss_notification); });
    return;
  }

  RTC_DCHECK_RUN_ON(&encoder_queue_);
  if (encoder_) {
    encoder_->OnLossNotification(loss_notification);
  }
}

EncodedImageCallback::Result VideoStreamEncoder::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific_info,
    const RTPFragmentationHeader* fragmentation) {
  TRACE_EVENT_INSTANT1("webrtc", "VCMEncodedFrameCallback::Encoded",
                       "timestamp", encoded_image.Timestamp());
  const size_t spatial_idx = encoded_image.SpatialIndex().value_or(0);
  EncodedImage image_copy(encoded_image);

  frame_encode_metadata_writer_.FillTimingInfo(spatial_idx, &image_copy);

  std::unique_ptr<RTPFragmentationHeader> fragmentation_copy =
      frame_encode_metadata_writer_.UpdateBitstream(codec_specific_info,
                                                    fragmentation, &image_copy);

  // Piggyback ALR experiment group id and simulcast id into the content type.
  const uint8_t experiment_id =
      experiment_groups_[videocontenttypehelpers::IsScreenshare(
          image_copy.content_type_)];

  // TODO(ilnik): This will force content type extension to be present even
  // for realtime video. At the expense of miniscule overhead we will get
  // sliced receive statistics.
  RTC_CHECK(videocontenttypehelpers::SetExperimentId(&image_copy.content_type_,
                                                     experiment_id));
  // We count simulcast streams from 1 on the wire. That's why we set simulcast
  // id in content type to +1 of that is actual simulcast index. This is because
  // value 0 on the wire is reserved for 'no simulcast stream specified'.
  RTC_CHECK(videocontenttypehelpers::SetSimulcastId(
      &image_copy.content_type_, static_cast<uint8_t>(spatial_idx + 1)));

  // Encoded is called on whatever thread the real encoder implementation run
  // on. In the case of hardware encoders, there might be several encoders
  // running in parallel on different threads.
  encoder_stats_observer_->OnSendEncodedImage(image_copy, codec_specific_info);

  // The simulcast id is signaled in the SpatialIndex. This makes it impossible
  // to do simulcast for codecs that actually support spatial layers since we
  // can't distinguish between an actual spatial layer and a simulcast stream.
  // TODO(bugs.webrtc.org/10520): Signal the simulcast id explicitly.
  int simulcast_id = 0;
  if (codec_specific_info &&
      (codec_specific_info->codecType == kVideoCodecVP8 ||
       codec_specific_info->codecType == kVideoCodecH264 ||
       codec_specific_info->codecType == kVideoCodecGeneric)) {
    simulcast_id = encoded_image.SpatialIndex().value_or(0);
  }

  std::unique_ptr<CodecSpecificInfo> codec_info_copy;
  {
    rtc::CritScope cs(&encoded_image_lock_);

    if (codec_specific_info && codec_specific_info->generic_frame_info) {
      codec_info_copy =
          absl::make_unique<CodecSpecificInfo>(*codec_specific_info);
      GenericFrameInfo& generic_info = *codec_info_copy->generic_frame_info;
      generic_info.frame_id = next_frame_id_++;

      if (encoder_buffer_state_.size() <= static_cast<size_t>(simulcast_id)) {
        RTC_LOG(LS_ERROR) << "At most " << encoder_buffer_state_.size()
                          << " simulcast streams supported.";
      } else {
        std::array<int64_t, kMaxEncoderBuffers>& state =
            encoder_buffer_state_[simulcast_id];
        for (const CodecBufferUsage& buffer : generic_info.encoder_buffers) {
          if (state.size() <= static_cast<size_t>(buffer.id)) {
            RTC_LOG(LS_ERROR)
                << "At most " << state.size() << " encoder buffers supported.";
            break;
          }

          if (buffer.referenced) {
            int64_t diff = generic_info.frame_id - state[buffer.id];
            if (diff <= 0) {
              RTC_LOG(LS_ERROR) << "Invalid frame diff: " << diff << ".";
            } else if (absl::c_find(generic_info.frame_diffs, diff) ==
                       generic_info.frame_diffs.end()) {
              generic_info.frame_diffs.push_back(diff);
            }
          }

          if (buffer.updated)
            state[buffer.id] = generic_info.frame_id;
        }
      }
    }
  }

  EncodedImageCallback::Result result = sink_->OnEncodedImage(
      image_copy, codec_info_copy ? codec_info_copy.get() : codec_specific_info,
      fragmentation_copy ? fragmentation_copy.get() : fragmentation);

  // We are only interested in propagating the meta-data about the image, not
  // encoded data itself, to the post encode function. Since we cannot be sure
  // the pointer will still be valid when run on the task queue, set it to null.
  image_copy.set_buffer(nullptr, 0);

  int temporal_index = 0;
  if (codec_specific_info) {
    if (codec_specific_info->codecType == kVideoCodecVP9) {
      temporal_index = codec_specific_info->codecSpecific.VP9.temporal_idx;
    } else if (codec_specific_info->codecType == kVideoCodecVP8) {
      temporal_index = codec_specific_info->codecSpecific.VP8.temporalIdx;
    }
  }
  if (temporal_index == kNoTemporalIdx) {
    temporal_index = 0;
  }

  RunPostEncode(image_copy, rtc::TimeMicros(), temporal_index);

  if (result.error == Result::OK) {
    // In case of an internal encoder running on a separate thread, the
    // decision to drop a frame might be a frame late and signaled via
    // atomic flag. This is because we can't easily wait for the worker thread
    // without risking deadlocks, eg during shutdown when the worker thread
    // might be waiting for the internal encoder threads to stop.
    if (pending_frame_drops_.load() > 0) {
      int pending_drops = pending_frame_drops_.fetch_sub(1);
      RTC_DCHECK_GT(pending_drops, 0);
      result.drop_next_frame = true;
    }
  }

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

void VideoStreamEncoder::OnBitrateUpdated(DataRate target_bitrate,
                                          DataRate link_allocation,
                                          uint8_t fraction_lost,
                                          int64_t round_trip_time_ms) {
  RTC_DCHECK_GE(link_allocation, target_bitrate);
  if (!encoder_queue_.IsCurrent()) {
    encoder_queue_.PostTask([this, target_bitrate, link_allocation,
                             fraction_lost, round_trip_time_ms] {
      OnBitrateUpdated(target_bitrate, link_allocation, fraction_lost,
                       round_trip_time_ms);
    });
    return;
  }
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  RTC_DCHECK(sink_) << "sink_ must be set before the encoder is active.";

  RTC_LOG(LS_VERBOSE) << "OnBitrateUpdated, bitrate " << target_bitrate.bps()
                      << " link allocation bitrate = " << link_allocation.bps()
                      << " packet loss " << static_cast<int>(fraction_lost)
                      << " rtt " << round_trip_time_ms;

  // On significant changes to BWE at the start of the call,
  // enable frame drops to quickly react to jumps in available bandwidth.
  if (encoder_start_bitrate_bps_ != 0 &&
      !has_seen_first_significant_bwe_change_ && quality_scaler_ &&
      initial_framedrop_on_bwe_enabled_ &&
      abs_diff(target_bitrate.bps(), encoder_start_bitrate_bps_) >=
          kFramedropThreshold * encoder_start_bitrate_bps_) {
    // Reset initial framedrop feature when first real BW estimate arrives.
    // TODO(kthelgason): Update BitrateAllocator to not call OnBitrateUpdated
    // without an actual BW estimate.
    initial_framedrop_ = 0;
    has_seen_first_significant_bwe_change_ = true;
  }
  if (set_start_bitrate_bps_ > 0 && !has_seen_first_bwe_drop_ &&
      quality_scaler_ && quality_scaler_settings_.InitialBitrateIntervalMs() &&
      quality_scaler_settings_.InitialBitrateFactor()) {
    int64_t diff_ms = clock_->TimeInMilliseconds() - set_start_bitrate_time_ms_;
    if (diff_ms < quality_scaler_settings_.InitialBitrateIntervalMs().value() &&
        (target_bitrate.bps() <
         (set_start_bitrate_bps_ *
          quality_scaler_settings_.InitialBitrateFactor().value()))) {
      RTC_LOG(LS_INFO) << "Reset initial_framedrop_. Start bitrate: "
                       << set_start_bitrate_bps_
                       << ", target bitrate: " << target_bitrate.bps();
      initial_framedrop_ = 0;
      has_seen_first_bwe_drop_ = true;
    }
  }

  if (encoder_) {
    encoder_->OnPacketLossRateUpdate(static_cast<float>(fraction_lost) / 256.f);
    encoder_->OnRttUpdate(round_trip_time_ms);
  }

  uint32_t framerate_fps = GetInputFramerateFps();
  frame_dropper_.SetRates((target_bitrate.bps() + 500) / 1000, framerate_fps);
  const bool video_is_suspended = target_bitrate == DataRate::Zero();
  const bool video_suspension_changed = video_is_suspended != EncoderPaused();

  EncoderRateSettings new_rate_settings{VideoBitrateAllocation(),
                                        static_cast<double>(framerate_fps),
                                        link_allocation, target_bitrate};
  SetEncoderRates(UpdateBitrateAllocationAndNotifyObserver(new_rate_settings));

  encoder_start_bitrate_bps_ = target_bitrate.bps() != 0
                                   ? target_bitrate.bps()
                                   : encoder_start_bitrate_bps_;

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

bool VideoStreamEncoder::AdaptDown(AdaptReason reason) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  AdaptationRequest adaptation_request = {
      last_frame_info_->pixel_count(),
      encoder_stats_observer_->GetInputFrameRate(),
      AdaptationRequest::Mode::kAdaptDown};

  bool downgrade_requested =
      last_adaptation_request_ &&
      last_adaptation_request_->mode_ == AdaptationRequest::Mode::kAdaptDown;

  bool did_adapt = true;

  switch (degradation_preference_) {
    case DegradationPreference::BALANCED:
      break;
    case DegradationPreference::MAINTAIN_FRAMERATE:
      if (downgrade_requested &&
          adaptation_request.input_pixel_count_ >=
              last_adaptation_request_->input_pixel_count_) {
        // Don't request lower resolution if the current resolution is not
        // lower than the last time we asked for the resolution to be lowered.
        return true;
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
        return true;
      }
      break;
    case DegradationPreference::DISABLED:
      return true;
  }

  switch (degradation_preference_) {
    case DegradationPreference::BALANCED: {
      // Try scale down framerate, if lower.
      int fps = balanced_settings_.MinFps(encoder_config_.codec_type,
                                          last_frame_info_->pixel_count());
      if (source_proxy_->RestrictFramerate(fps)) {
        GetAdaptCounter().IncrementFramerate(reason);
        // Check if requested fps is higher (or close to) input fps.
        absl::optional<int> min_diff =
            balanced_settings_.MinFpsDiff(last_frame_info_->pixel_count());
        if (min_diff && adaptation_request.framerate_fps_ > 0) {
          int fps_diff = adaptation_request.framerate_fps_ - fps;
          if (fps_diff < min_diff.value()) {
            did_adapt = false;
          }
        }
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
        return true;
      }
      GetAdaptCounter().IncrementResolution(reason);
      break;
    }
    case DegradationPreference::MAINTAIN_RESOLUTION: {
      // Scale down framerate.
      const int requested_framerate = source_proxy_->RequestFramerateLowerThan(
          adaptation_request.framerate_fps_);
      if (requested_framerate == -1)
        return true;
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
  return did_adapt;
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
      // Check if quality should be increased based on bitrate.
      if (reason == kQuality &&
          !balanced_settings_.CanAdaptUp(last_frame_info_->pixel_count(),
                                         encoder_start_bitrate_bps_)) {
        return;
      }
      // Try scale up framerate, if higher.
      int fps = balanced_settings_.MaxFps(encoder_config_.codec_type,
                                          last_frame_info_->pixel_count());
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
      // Check if resolution should be increased based on bitrate.
      if (reason == kQuality &&
          !balanced_settings_.CanAdaptUpResolution(
              last_frame_info_->pixel_count(), encoder_start_bitrate_bps_)) {
        return;
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

void VideoStreamEncoder::RunPostEncode(EncodedImage encoded_image,
                                       int64_t time_sent_us,
                                       int temporal_index) {
  if (!encoder_queue_.IsCurrent()) {
    encoder_queue_.PostTask(
        [this, encoded_image, time_sent_us, temporal_index] {
          RunPostEncode(encoded_image, time_sent_us, temporal_index);
        });
    return;
  }

  RTC_DCHECK_RUN_ON(&encoder_queue_);

  absl::optional<int> encode_duration_us;
  if (encoded_image.timing_.flags != VideoSendTiming::kInvalid) {
    encode_duration_us =
        // TODO(nisse): Maybe use capture_time_ms_ rather than encode_start_ms_?
        rtc::kNumMicrosecsPerMillisec *
        (encoded_image.timing_.encode_finish_ms -
         encoded_image.timing_.encode_start_ms);
  }

  // Run post encode tasks, such as overuse detection and frame rate/drop
  // stats for internal encoders.
  const size_t frame_size = encoded_image.size();
  const bool keyframe =
      encoded_image._frameType == VideoFrameType::kVideoFrameKey;

  if (frame_size > 0) {
    frame_dropper_.Fill(frame_size, !keyframe);
  }

  if (HasInternalSource()) {
    // Update frame dropper after the fact for internal sources.
    input_framerate_.Update(1u, clock_->TimeInMilliseconds());
    frame_dropper_.Leak(GetInputFramerateFps());
    // Signal to encoder to drop next frame.
    if (frame_dropper_.DropFrame()) {
      pending_frame_drops_.fetch_add(1);
    }
  }

  overuse_detector_->FrameSent(
      encoded_image.Timestamp(), time_sent_us,
      encoded_image.capture_time_ms_ * rtc::kNumMicrosecsPerMillisec,
      encode_duration_us);
  if (quality_scaler_ && encoded_image.qp_ >= 0)
    quality_scaler_->ReportQp(encoded_image.qp_, time_sent_us);
  if (bitrate_adjuster_) {
    bitrate_adjuster_->OnEncodedFrame(encoded_image, temporal_index);
  }
}

bool VideoStreamEncoder::HasInternalSource() const {
  // TODO(sprang): Checking both info from encoder and from encoder factory
  // until we have deprecated and removed the encoder factory info.
  return codec_info_.has_internal_source || encoder_info_.has_internal_source;
}

void VideoStreamEncoder::ReleaseEncoder() {
  if (!encoder_ || !encoder_initialized_) {
    return;
  }
  encoder_->Release();
  encoder_initialized_ = false;
  TRACE_EVENT0("webrtc", "VCMGenericEncoder::Release");
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
    absl::c_fill(fps_counters_, 0);
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
  return absl::c_accumulate(counters, 0);
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
