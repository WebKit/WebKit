/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/vie_encoder.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"
#include "webrtc/video/overuse_frame_detector.h"
#include "webrtc/video/send_statistics_proxy.h"
#include "webrtc/video_frame.h"

namespace webrtc {

namespace {
// Time interval for logging frame counts.
const int64_t kFrameLogIntervalMs = 60000;

VideoCodecType PayloadNameToCodecType(const std::string& payload_name) {
  if (payload_name == "VP8")
    return kVideoCodecVP8;
  if (payload_name == "VP9")
    return kVideoCodecVP9;
  if (payload_name == "H264")
    return kVideoCodecH264;
  return kVideoCodecGeneric;
}

VideoCodec VideoEncoderConfigToVideoCodec(
    const VideoEncoderConfig& config,
    const std::vector<VideoStream>& streams,
    const std::string& payload_name,
    int payload_type) {
  static const int kEncoderMinBitrateKbps = 30;
  RTC_DCHECK(!streams.empty());
  RTC_DCHECK_GE(config.min_transmit_bitrate_bps, 0);

  VideoCodec video_codec;
  memset(&video_codec, 0, sizeof(video_codec));
  video_codec.codecType = PayloadNameToCodecType(payload_name);

  switch (config.content_type) {
    case VideoEncoderConfig::ContentType::kRealtimeVideo:
      video_codec.mode = kRealtimeVideo;
      break;
    case VideoEncoderConfig::ContentType::kScreen:
      video_codec.mode = kScreensharing;
      if (streams.size() == 1 &&
          streams[0].temporal_layer_thresholds_bps.size() == 1) {
        video_codec.targetBitrate =
            streams[0].temporal_layer_thresholds_bps[0] / 1000;
      }
      break;
  }

  if (config.encoder_specific_settings)
    config.encoder_specific_settings->FillEncoderSpecificSettings(&video_codec);

  switch (video_codec.codecType) {
    case kVideoCodecVP8: {
      if (!config.encoder_specific_settings)
        video_codec.codecSpecific.VP8 = VideoEncoder::GetDefaultVp8Settings();
      video_codec.codecSpecific.VP8.numberOfTemporalLayers =
          static_cast<unsigned char>(
              streams.back().temporal_layer_thresholds_bps.size() + 1);
      break;
    }
    case kVideoCodecVP9: {
      if (!config.encoder_specific_settings)
        video_codec.codecSpecific.VP9 = VideoEncoder::GetDefaultVp9Settings();
      if (video_codec.mode == kScreensharing &&
          config.encoder_specific_settings) {
        video_codec.codecSpecific.VP9.flexibleMode = true;
        // For now VP9 screensharing use 1 temporal and 2 spatial layers.
        RTC_DCHECK_EQ(1, video_codec.codecSpecific.VP9.numberOfTemporalLayers);
        RTC_DCHECK_EQ(2, video_codec.codecSpecific.VP9.numberOfSpatialLayers);
      }
      video_codec.codecSpecific.VP9.numberOfTemporalLayers =
          static_cast<unsigned char>(
              streams.back().temporal_layer_thresholds_bps.size() + 1);
      break;
    }
    case kVideoCodecH264: {
      if (!config.encoder_specific_settings)
        video_codec.codecSpecific.H264 = VideoEncoder::GetDefaultH264Settings();
      break;
    }
    default:
      // TODO(pbos): Support encoder_settings codec-agnostically.
      RTC_DCHECK(!config.encoder_specific_settings)
          << "Encoder-specific settings for codec type not wired up.";
      break;
  }

  strncpy(video_codec.plName, payload_name.c_str(), kPayloadNameSize - 1);
  video_codec.plName[kPayloadNameSize - 1] = '\0';
  video_codec.plType = payload_type;
  video_codec.numberOfSimulcastStreams =
      static_cast<unsigned char>(streams.size());
  video_codec.minBitrate = streams[0].min_bitrate_bps / 1000;
  if (video_codec.minBitrate < kEncoderMinBitrateKbps)
    video_codec.minBitrate = kEncoderMinBitrateKbps;
  RTC_DCHECK_LE(streams.size(), static_cast<size_t>(kMaxSimulcastStreams));
  if (video_codec.codecType == kVideoCodecVP9) {
    // If the vector is empty, bitrates will be configured automatically.
    RTC_DCHECK(config.spatial_layers.empty() ||
               config.spatial_layers.size() ==
                   video_codec.codecSpecific.VP9.numberOfSpatialLayers);
    RTC_DCHECK_LE(video_codec.codecSpecific.VP9.numberOfSpatialLayers,
                  kMaxSimulcastStreams);
    for (size_t i = 0; i < config.spatial_layers.size(); ++i)
      video_codec.spatialLayers[i] = config.spatial_layers[i];
  }
  for (size_t i = 0; i < streams.size(); ++i) {
    SimulcastStream* sim_stream = &video_codec.simulcastStream[i];
    RTC_DCHECK_GT(streams[i].width, 0u);
    RTC_DCHECK_GT(streams[i].height, 0u);
    RTC_DCHECK_GT(streams[i].max_framerate, 0);
    // Different framerates not supported per stream at the moment.
    RTC_DCHECK_EQ(streams[i].max_framerate, streams[0].max_framerate);
    RTC_DCHECK_GE(streams[i].min_bitrate_bps, 0);
    RTC_DCHECK_GE(streams[i].target_bitrate_bps, streams[i].min_bitrate_bps);
    RTC_DCHECK_GE(streams[i].max_bitrate_bps, streams[i].target_bitrate_bps);
    RTC_DCHECK_GE(streams[i].max_qp, 0);

    sim_stream->width = static_cast<uint16_t>(streams[i].width);
    sim_stream->height = static_cast<uint16_t>(streams[i].height);
    sim_stream->minBitrate = streams[i].min_bitrate_bps / 1000;
    sim_stream->targetBitrate = streams[i].target_bitrate_bps / 1000;
    sim_stream->maxBitrate = streams[i].max_bitrate_bps / 1000;
    sim_stream->qpMax = streams[i].max_qp;
    sim_stream->numberOfTemporalLayers = static_cast<unsigned char>(
        streams[i].temporal_layer_thresholds_bps.size() + 1);

    video_codec.width = std::max(video_codec.width,
                                 static_cast<uint16_t>(streams[i].width));
    video_codec.height = std::max(
        video_codec.height, static_cast<uint16_t>(streams[i].height));
    video_codec.minBitrate =
        std::min(static_cast<uint16_t>(video_codec.minBitrate),
                 static_cast<uint16_t>(streams[i].min_bitrate_bps / 1000));
    video_codec.maxBitrate += streams[i].max_bitrate_bps / 1000;
    video_codec.qpMax = std::max(video_codec.qpMax,
                                 static_cast<unsigned int>(streams[i].max_qp));
  }

  if (video_codec.maxBitrate == 0) {
    // Unset max bitrate -> cap to one bit per pixel.
    video_codec.maxBitrate =
        (video_codec.width * video_codec.height * video_codec.maxFramerate) /
        1000;
  }
  if (video_codec.maxBitrate < kEncoderMinBitrateKbps)
    video_codec.maxBitrate = kEncoderMinBitrateKbps;

  RTC_DCHECK_GT(streams[0].max_framerate, 0);
  video_codec.maxFramerate = streams[0].max_framerate;
  return video_codec;
}

// TODO(pbos): Lower these thresholds (to closer to 100%) when we handle
// pipelining encoders better (multiple input frames before something comes
// out). This should effectively turn off CPU adaptations for systems that
// remotely cope with the load right now.
CpuOveruseOptions GetCpuOveruseOptions(bool full_overuse_time) {
  CpuOveruseOptions options;
  if (full_overuse_time) {
    options.low_encode_usage_threshold_percent = 150;
    options.high_encode_usage_threshold_percent = 200;
  }
  return options;
}

}  //  namespace

class ViEEncoder::ConfigureEncoderTask : public rtc::QueuedTask {
 public:
  ConfigureEncoderTask(ViEEncoder* vie_encoder,
                       VideoEncoderConfig config,
                       size_t max_data_payload_length)
      : vie_encoder_(vie_encoder),
        config_(std::move(config)),
        max_data_payload_length_(max_data_payload_length) {}

 private:
  bool Run() override {
    vie_encoder_->ConfigureEncoderOnTaskQueue(std::move(config_),
                                              max_data_payload_length_);
    return true;
  }

  ViEEncoder* const vie_encoder_;
  VideoEncoderConfig config_;
  size_t max_data_payload_length_;
};

class ViEEncoder::EncodeTask : public rtc::QueuedTask {
 public:
  EncodeTask(const VideoFrame& frame,
             ViEEncoder* vie_encoder,
             int64_t time_when_posted_in_ms,
             bool log_stats)
      : vie_encoder_(vie_encoder),
        time_when_posted_ms_(time_when_posted_in_ms),
        log_stats_(log_stats) {
    frame_ = frame;
    ++vie_encoder_->posted_frames_waiting_for_encode_;
  }

 private:
  bool Run() override {
    RTC_DCHECK_RUN_ON(&vie_encoder_->encoder_queue_);
    RTC_DCHECK_GT(vie_encoder_->posted_frames_waiting_for_encode_.Value(), 0);
    vie_encoder_->stats_proxy_->OnIncomingFrame(frame_.width(),
                                                frame_.height());
    ++vie_encoder_->captured_frame_count_;
    if (--vie_encoder_->posted_frames_waiting_for_encode_ == 0) {
      vie_encoder_->EncodeVideoFrame(frame_, time_when_posted_ms_);
    } else {
      // There is a newer frame in flight. Do not encode this frame.
      LOG(LS_VERBOSE)
          << "Incoming frame dropped due to that the encoder is blocked.";
      ++vie_encoder_->dropped_frame_count_;
    }
    if (log_stats_) {
      LOG(LS_INFO) << "Number of frames: captured "
                   << vie_encoder_->captured_frame_count_
                   << ", dropped (due to encoder blocked) "
                   << vie_encoder_->dropped_frame_count_ << ", interval_ms "
                   << kFrameLogIntervalMs;
      vie_encoder_->captured_frame_count_ = 0;
      vie_encoder_->dropped_frame_count_ = 0;
    }
    return true;
  }
  VideoFrame frame_;
  ViEEncoder* const vie_encoder_;
  const int64_t time_when_posted_ms_;
  const bool log_stats_;
};

// VideoSourceProxy is responsible ensuring thread safety between calls to
// ViEEncoder::SetSource that will happen on libjingle's worker thread when a
// video capturer is connected to the encoder and the encoder task queue
// (encoder_queue_) where the encoder reports its VideoSinkWants.
class ViEEncoder::VideoSourceProxy {
 public:
  explicit VideoSourceProxy(ViEEncoder* vie_encoder)
      : vie_encoder_(vie_encoder),
        degradation_preference_(
            VideoSendStream::DegradationPreference::kMaintainResolution),
        source_(nullptr) {}

  void SetSource(
      rtc::VideoSourceInterface<VideoFrame>* source,
      const VideoSendStream::DegradationPreference& degradation_preference) {
    // Called on libjingle's worker thread.
    RTC_DCHECK_CALLED_SEQUENTIALLY(&main_checker_);
    rtc::VideoSourceInterface<VideoFrame>* old_source = nullptr;
    rtc::VideoSinkWants wants;
    {
      rtc::CritScope lock(&crit_);
      old_source = source_;
      source_ = source;
      degradation_preference_ = degradation_preference;
      wants = current_wants();
    }

    if (old_source != source && old_source != nullptr) {
      old_source->RemoveSink(vie_encoder_);
    }

    if (!source) {
      return;
    }

    source->AddOrUpdateSink(vie_encoder_, wants);
  }

  void SetWantsRotationApplied(bool rotation_applied) {
    rtc::CritScope lock(&crit_);
    sink_wants_.rotation_applied = rotation_applied;
    disabled_scaling_sink_wants_.rotation_applied = rotation_applied;
    if (source_) {
      source_->AddOrUpdateSink(vie_encoder_, current_wants());
    }
  }

  void RequestResolutionLowerThan(int pixel_count) {
    // Called on the encoder task queue.
    rtc::CritScope lock(&crit_);
    if (!IsResolutionScalingEnabledLocked()) {
      // This can happen since |degradation_preference_| is set on
      // libjingle's worker thread but the adaptation is done on the encoder
      // task queue.
      return;
    }
    // The input video frame size will have a resolution with less than or
    // equal to |max_pixel_count| depending on how the source can scale the
    // input frame size.
    sink_wants_.max_pixel_count = rtc::Optional<int>((pixel_count * 3) / 5);
    sink_wants_.max_pixel_count_step_up = rtc::Optional<int>();
    if (source_)
      source_->AddOrUpdateSink(vie_encoder_, sink_wants_);
  }

  void RequestHigherResolutionThan(int pixel_count) {
    rtc::CritScope lock(&crit_);
    if (!IsResolutionScalingEnabledLocked()) {
      // This can happen since |degradation_preference_| is set on
      // libjingle's worker thread but the adaptation is done on the encoder
      // task
      // queue.
      return;
    }
    // The input video frame size will have a resolution with "one step up"
    // pixels than |max_pixel_count_step_up| where "one step up" depends on
    // how the source can scale the input frame size.
    sink_wants_.max_pixel_count = rtc::Optional<int>();
    sink_wants_.max_pixel_count_step_up = rtc::Optional<int>(pixel_count);
    if (source_)
      source_->AddOrUpdateSink(vie_encoder_, sink_wants_);
  }

 private:
  bool IsResolutionScalingEnabledLocked() const
      EXCLUSIVE_LOCKS_REQUIRED(&crit_) {
    return degradation_preference_ !=
           VideoSendStream::DegradationPreference::kMaintainResolution;
  }

  const rtc::VideoSinkWants& current_wants() const
      EXCLUSIVE_LOCKS_REQUIRED(&crit_) {
    return IsResolutionScalingEnabledLocked() ? sink_wants_
                                              : disabled_scaling_sink_wants_;
  }

  rtc::CriticalSection crit_;
  rtc::SequencedTaskChecker main_checker_;
  ViEEncoder* const vie_encoder_;
  rtc::VideoSinkWants sink_wants_ GUARDED_BY(&crit_);
  rtc::VideoSinkWants disabled_scaling_sink_wants_ GUARDED_BY(&crit_);
  VideoSendStream::DegradationPreference degradation_preference_
      GUARDED_BY(&crit_);
  rtc::VideoSourceInterface<VideoFrame>* source_ GUARDED_BY(&crit_);

  RTC_DISALLOW_COPY_AND_ASSIGN(VideoSourceProxy);
};

ViEEncoder::ViEEncoder(uint32_t number_of_cores,
                       SendStatisticsProxy* stats_proxy,
                       const VideoSendStream::Config::EncoderSettings& settings,
                       rtc::VideoSinkInterface<VideoFrame>* pre_encode_callback,
                       EncodedFrameObserver* encoder_timing)
    : shutdown_event_(true /* manual_reset */, false),
      number_of_cores_(number_of_cores),
      source_proxy_(new VideoSourceProxy(this)),
      sink_(nullptr),
      settings_(settings),
      codec_type_(PayloadNameToCodecType(settings.payload_name)),
      video_sender_(Clock::GetRealTimeClock(), this, this),
      overuse_detector_(Clock::GetRealTimeClock(),
                        GetCpuOveruseOptions(settings.full_overuse_time),
                        this,
                        encoder_timing,
                        stats_proxy),
      stats_proxy_(stats_proxy),
      pre_encode_callback_(pre_encode_callback),
      module_process_thread_(nullptr),
      pending_encoder_reconfiguration_(false),
      encoder_start_bitrate_bps_(0),
      max_data_payload_length_(0),
      last_observed_bitrate_bps_(0),
      encoder_paused_and_dropped_frame_(false),
      has_received_sli_(false),
      picture_id_sli_(0),
      has_received_rpsi_(false),
      picture_id_rpsi_(0),
      clock_(Clock::GetRealTimeClock()),
      degradation_preference_(
          VideoSendStream::DegradationPreference::kBalanced),
      cpu_restricted_counter_(0),
      last_frame_width_(0),
      last_frame_height_(0),
      last_captured_timestamp_(0),
      delta_ntp_internal_ms_(clock_->CurrentNtpInMilliseconds() -
                             clock_->TimeInMilliseconds()),
      last_frame_log_ms_(clock_->TimeInMilliseconds()),
      captured_frame_count_(0),
      dropped_frame_count_(0),
      encoder_queue_("EncoderQueue") {
  encoder_queue_.PostTask([this] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    overuse_detector_.StartCheckForOveruse();
    video_sender_.RegisterExternalEncoder(
        settings_.encoder, settings_.payload_type, settings_.internal_source);
  });
}

ViEEncoder::~ViEEncoder() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(shutdown_event_.Wait(0))
      << "Must call ::Stop() before destruction.";
}

void ViEEncoder::Stop() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  source_proxy_->SetSource(nullptr, VideoSendStream::DegradationPreference());
  encoder_queue_.PostTask([this] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    overuse_detector_.StopCheckForOveruse();
    video_sender_.RegisterExternalEncoder(nullptr, settings_.payload_type,
                                          false);
    shutdown_event_.Set();
  });

  shutdown_event_.Wait(rtc::Event::kForever);
}

void ViEEncoder::RegisterProcessThread(ProcessThread* module_process_thread) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(!module_process_thread_);
  module_process_thread_ = module_process_thread;
  module_process_thread_->RegisterModule(&video_sender_);
  module_process_thread_checker_.DetachFromThread();
}

void ViEEncoder::DeRegisterProcessThread() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  module_process_thread_->DeRegisterModule(&video_sender_);
}

void ViEEncoder::SetSource(
    rtc::VideoSourceInterface<VideoFrame>* source,
    const VideoSendStream::DegradationPreference& degradation_preference) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  source_proxy_->SetSource(source, degradation_preference);
  encoder_queue_.PostTask([this, degradation_preference] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    degradation_preference_ = degradation_preference;
    // Set the stats for if we are currently CPU restricted. We are CPU
    // restricted depending on degradation preference and
    // if the overusedetector has currently detected overuse which is counted in
    // |cpu_restricted_counter_|
    // We do this on the encoder task queue to avoid a race with the stats set
    // in ViEEncoder::NormalUsage and ViEEncoder::OveruseDetected.
    stats_proxy_->SetCpuRestrictedResolution(
        degradation_preference_ !=
            VideoSendStream::DegradationPreference::kMaintainResolution &&
        cpu_restricted_counter_ != 0);
  });
}

void ViEEncoder::SetSink(EncoderSink* sink, bool rotation_applied) {
  source_proxy_->SetWantsRotationApplied(rotation_applied);
  encoder_queue_.PostTask([this, sink] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    sink_ = sink;
  });
}

void ViEEncoder::SetStartBitrate(int start_bitrate_bps) {
  encoder_queue_.PostTask([this, start_bitrate_bps] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    encoder_start_bitrate_bps_ = start_bitrate_bps;
  });
}

void ViEEncoder::ConfigureEncoder(VideoEncoderConfig config,
                                  size_t max_data_payload_length) {
  encoder_queue_.PostTask(
      std::unique_ptr<rtc::QueuedTask>(new ConfigureEncoderTask(
          this, std::move(config), max_data_payload_length)));
}

void ViEEncoder::ConfigureEncoderOnTaskQueue(VideoEncoderConfig config,
                                             size_t max_data_payload_length) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  RTC_DCHECK(sink_);
  LOG(LS_INFO) << "ConfigureEncoder requested.";

  max_data_payload_length_ = max_data_payload_length;
  encoder_config_ = std::move(config);
  pending_encoder_reconfiguration_ = true;

  // Reconfigure the encoder now if the encoder has an internal source or
  // if the frame resolution is known. Otherwise, the reconfiguration is
  // deferred until the next frame to minimize the number of reconfigurations.
  // The codec configuration depends on incoming video frame size.
  if (last_frame_info_) {
    ReconfigureEncoder();
  } else if (settings_.internal_source) {
    last_frame_info_ = rtc::Optional<VideoFrameInfo>(
        VideoFrameInfo(176, 144, kVideoRotation_0, false));
    ReconfigureEncoder();
  }
}

void ViEEncoder::ReconfigureEncoder() {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  RTC_DCHECK(pending_encoder_reconfiguration_);
  std::vector<VideoStream> streams =
      encoder_config_.video_stream_factory->CreateEncoderStreams(
          last_frame_info_->width, last_frame_info_->height, encoder_config_);

  VideoCodec codec = VideoEncoderConfigToVideoCodec(
      encoder_config_, streams, settings_.payload_name, settings_.payload_type);

  codec.startBitrate =
      std::max(encoder_start_bitrate_bps_ / 1000, codec.minBitrate);
  codec.startBitrate = std::min(codec.startBitrate, codec.maxBitrate);
  codec.expect_encode_from_texture = last_frame_info_->is_texture;

  bool success = video_sender_.RegisterSendCodec(
                     &codec, number_of_cores_,
                     static_cast<uint32_t>(max_data_payload_length_)) == VCM_OK;
  if (!success) {
    LOG(LS_ERROR) << "Failed to configure encoder.";
    RTC_DCHECK(success);
  }

  rate_allocator_.reset(new SimulcastRateAllocator(codec));
  if (stats_proxy_) {
    stats_proxy_->OnEncoderReconfigured(encoder_config_,
                                        rate_allocator_->GetPreferedBitrate());
  }

  pending_encoder_reconfiguration_ = false;
  if (stats_proxy_) {
    stats_proxy_->OnEncoderReconfigured(encoder_config_,
                                        rate_allocator_->GetPreferedBitrate());
  }
  sink_->OnEncoderConfigurationChanged(
      std::move(streams), encoder_config_.min_transmit_bitrate_bps);
}

void ViEEncoder::OnFrame(const VideoFrame& video_frame) {
  RTC_DCHECK_RUNS_SERIALIZED(&incoming_frame_race_checker_);
  VideoFrame incoming_frame = video_frame;

  // Local time in webrtc time base.
  int64_t current_time = clock_->TimeInMilliseconds();
  incoming_frame.set_render_time_ms(current_time);

  // Capture time may come from clock with an offset and drift from clock_.
  int64_t capture_ntp_time_ms;
  if (video_frame.ntp_time_ms() != 0) {
    capture_ntp_time_ms = video_frame.ntp_time_ms();
  } else if (video_frame.render_time_ms() != 0) {
    capture_ntp_time_ms = video_frame.render_time_ms() + delta_ntp_internal_ms_;
  } else {
    capture_ntp_time_ms = current_time + delta_ntp_internal_ms_;
  }
  incoming_frame.set_ntp_time_ms(capture_ntp_time_ms);

  // Convert NTP time, in ms, to RTP timestamp.
  const int kMsToRtpTimestamp = 90;
  incoming_frame.set_timestamp(
      kMsToRtpTimestamp * static_cast<uint32_t>(incoming_frame.ntp_time_ms()));

  if (incoming_frame.ntp_time_ms() <= last_captured_timestamp_) {
    // We don't allow the same capture time for two frames, drop this one.
    LOG(LS_WARNING) << "Same/old NTP timestamp ("
                    << incoming_frame.ntp_time_ms()
                    << " <= " << last_captured_timestamp_
                    << ") for incoming frame. Dropping.";
    return;
  }

  bool log_stats = false;
  if (current_time - last_frame_log_ms_ > kFrameLogIntervalMs) {
    last_frame_log_ms_ = current_time;
    log_stats = true;
  }

  last_captured_timestamp_ = incoming_frame.ntp_time_ms();
  encoder_queue_.PostTask(std::unique_ptr<rtc::QueuedTask>(new EncodeTask(
      incoming_frame, this, clock_->TimeInMilliseconds(), log_stats)));
}

bool ViEEncoder::EncoderPaused() const {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  // Pause video if paused by caller or as long as the network is down or the
  // pacer queue has grown too large in buffered mode.
  // If the pacer queue has grown too large or the network is down,
  // last_observed_bitrate_bps_ will be 0.
  return last_observed_bitrate_bps_ == 0;
}

void ViEEncoder::TraceFrameDropStart() {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  // Start trace event only on the first frame after encoder is paused.
  if (!encoder_paused_and_dropped_frame_) {
    TRACE_EVENT_ASYNC_BEGIN0("webrtc", "EncoderPaused", this);
  }
  encoder_paused_and_dropped_frame_ = true;
  return;
}

void ViEEncoder::TraceFrameDropEnd() {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  // End trace event on first frame after encoder resumes, if frame was dropped.
  if (encoder_paused_and_dropped_frame_) {
    TRACE_EVENT_ASYNC_END0("webrtc", "EncoderPaused", this);
  }
  encoder_paused_and_dropped_frame_ = false;
}

void ViEEncoder::EncodeVideoFrame(const VideoFrame& video_frame,
                                  int64_t time_when_posted_in_ms) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  if (pre_encode_callback_)
    pre_encode_callback_->OnFrame(video_frame);

  if (!last_frame_info_ || video_frame.width() != last_frame_info_->width ||
      video_frame.height() != last_frame_info_->height ||
      video_frame.rotation() != last_frame_info_->rotation ||
      video_frame.is_texture() != last_frame_info_->is_texture) {
    pending_encoder_reconfiguration_ = true;
    last_frame_info_ = rtc::Optional<VideoFrameInfo>(
        VideoFrameInfo(video_frame.width(), video_frame.height(),
                       video_frame.rotation(), video_frame.is_texture()));
    LOG(LS_INFO) << "Video frame parameters changed: dimensions="
                 << last_frame_info_->width << "x" << last_frame_info_->height
                 << ", rotation=" << last_frame_info_->rotation
                 << ", texture=" << last_frame_info_->is_texture;
  }

  if (pending_encoder_reconfiguration_) {
    ReconfigureEncoder();
  }

  if (EncoderPaused()) {
    TraceFrameDropStart();
    return;
  }
  TraceFrameDropEnd();

  last_frame_height_ = video_frame.height();
  last_frame_width_ = video_frame.width();

  TRACE_EVENT_ASYNC_STEP0("webrtc", "Video", video_frame.render_time_ms(),
                          "Encode");

  overuse_detector_.FrameCaptured(video_frame, time_when_posted_in_ms);

  if (codec_type_ == webrtc::kVideoCodecVP8) {
    webrtc::CodecSpecificInfo codec_specific_info;
    codec_specific_info.codecType = webrtc::kVideoCodecVP8;

      codec_specific_info.codecSpecific.VP8.hasReceivedRPSI =
          has_received_rpsi_;
      codec_specific_info.codecSpecific.VP8.hasReceivedSLI =
          has_received_sli_;
      codec_specific_info.codecSpecific.VP8.pictureIdRPSI =
          picture_id_rpsi_;
      codec_specific_info.codecSpecific.VP8.pictureIdSLI  =
          picture_id_sli_;
      has_received_sli_ = false;
      has_received_rpsi_ = false;

      video_sender_.AddVideoFrame(video_frame, &codec_specific_info);
      return;
  }
  video_sender_.AddVideoFrame(video_frame, nullptr);
}

void ViEEncoder::SendKeyFrame() {
  if (!encoder_queue_.IsCurrent()) {
    encoder_queue_.PostTask([this] { SendKeyFrame(); });
    return;
  }
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  video_sender_.IntraFrameRequest(0);
}

EncodedImageCallback::Result ViEEncoder::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific_info,
    const RTPFragmentationHeader* fragmentation) {
  // Encoded is called on whatever thread the real encoder implementation run
  // on. In the case of hardware encoders, there might be several encoders
  // running in parallel on different threads.
  if (stats_proxy_) {
    stats_proxy_->OnSendEncodedImage(encoded_image, codec_specific_info);
  }

  EncodedImageCallback::Result result =
      sink_->OnEncodedImage(encoded_image, codec_specific_info, fragmentation);

  int64_t time_sent = clock_->TimeInMilliseconds();
  uint32_t timestamp = encoded_image._timeStamp;

  encoder_queue_.PostTask([this, timestamp, time_sent] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    overuse_detector_.FrameSent(timestamp, time_sent);
  });

  return result;
}

void ViEEncoder::SendStatistics(uint32_t bit_rate, uint32_t frame_rate) {
  RTC_DCHECK(module_process_thread_checker_.CalledOnValidThread());
  if (stats_proxy_)
    stats_proxy_->OnEncoderStatsUpdate(frame_rate, bit_rate);
}

void ViEEncoder::OnReceivedSLI(uint8_t picture_id) {
  if (!encoder_queue_.IsCurrent()) {
    encoder_queue_.PostTask([this, picture_id] { OnReceivedSLI(picture_id); });
    return;
  }
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  picture_id_sli_ = picture_id;
  has_received_sli_ = true;
}

void ViEEncoder::OnReceivedRPSI(uint64_t picture_id) {
  if (!encoder_queue_.IsCurrent()) {
    encoder_queue_.PostTask([this, picture_id] { OnReceivedRPSI(picture_id); });
    return;
  }
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  picture_id_rpsi_ = picture_id;
  has_received_rpsi_ = true;
}

void ViEEncoder::OnReceivedIntraFrameRequest(size_t stream_index) {
  if (!encoder_queue_.IsCurrent()) {
    encoder_queue_.PostTask(
        [this, stream_index] { OnReceivedIntraFrameRequest(stream_index); });
    return;
  }
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  // Key frame request from remote side, signal to VCM.
  TRACE_EVENT0("webrtc", "OnKeyFrameRequest");
  video_sender_.IntraFrameRequest(stream_index);
}

void ViEEncoder::OnBitrateUpdated(uint32_t bitrate_bps,
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

  LOG(LS_VERBOSE) << "OnBitrateUpdated, bitrate " << bitrate_bps
                  << " packet loss " << static_cast<int>(fraction_lost)
                  << " rtt " << round_trip_time_ms;

  video_sender_.SetChannelParameters(bitrate_bps, fraction_lost,
                                     round_trip_time_ms);

  encoder_start_bitrate_bps_ =
      bitrate_bps != 0 ? bitrate_bps : encoder_start_bitrate_bps_;
  bool video_is_suspended = bitrate_bps == 0;
  bool video_suspension_changed =
      video_is_suspended != (last_observed_bitrate_bps_ == 0);
  last_observed_bitrate_bps_ = bitrate_bps;

  if (stats_proxy_ && video_suspension_changed) {
    LOG(LS_INFO) << "Video suspend state changed to: "
                 << (video_is_suspended ? "suspended" : "not suspended");
    stats_proxy_->OnSuspendChange(video_is_suspended);
  }
}

void ViEEncoder::OveruseDetected() {
#if defined(WEBRTC_WEBKIT_BUILD)
  // WEBKIT Change: We disable OveruseDetected for now.
  // FIXME: Investigate using it. See https://bugs.webkit.org/show_bug.cgi?id=168990.
#else
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  if (degradation_preference_ ==
          VideoSendStream::DegradationPreference::kMaintainResolution ||
      cpu_restricted_counter_ >= kMaxCpuDowngrades) {
    return;
  }
  LOG(LS_INFO) << "CPU overuse detected. Requesting lower resolution.";
  // Request lower resolution if the current resolution is lower than last time
  // we asked for the resolution to be lowered.
  // Update stats accordingly.
  int current_pixel_count = last_frame_height_ * last_frame_width_;
  if (!max_pixel_count_ || current_pixel_count < *max_pixel_count_) {
    max_pixel_count_ = rtc::Optional<int>(current_pixel_count);
    max_pixel_count_step_up_ = rtc::Optional<int>();
    stats_proxy_->OnCpuRestrictedResolutionChanged(true);
    ++cpu_restricted_counter_;
    source_proxy_->RequestResolutionLowerThan(current_pixel_count);
  }
#endif
}

void ViEEncoder::NormalUsage() {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  if (degradation_preference_ ==
          VideoSendStream::DegradationPreference::kMaintainResolution ||
      cpu_restricted_counter_ == 0) {
    return;
  }

  LOG(LS_INFO) << "CPU underuse detected. Requesting higher resolution.";
  int current_pixel_count = last_frame_height_ * last_frame_width_;
  // Request higher resolution if we are CPU restricted and the the current
  // resolution is higher than last time we requested higher resolution.
  // Update stats accordingly.
  if (!max_pixel_count_step_up_ ||
      current_pixel_count > *max_pixel_count_step_up_) {
    max_pixel_count_ = rtc::Optional<int>();
    max_pixel_count_step_up_ = rtc::Optional<int>(current_pixel_count);
    --cpu_restricted_counter_;
    stats_proxy_->OnCpuRestrictedResolutionChanged(cpu_restricted_counter_ > 0);
    source_proxy_->RequestHigherResolutionThan(current_pixel_count);
  }
}

}  // namespace webrtc
