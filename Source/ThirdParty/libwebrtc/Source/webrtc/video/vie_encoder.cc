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

#include "webrtc/base/arraysize.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/location.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/common_video/include/video_bitrate_allocator.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/video_coding/codecs/vp8/temporal_layers.h"
#include "webrtc/modules/video_coding/include/video_codec_initializer.h"
#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"
#include "webrtc/video/overuse_frame_detector.h"
#include "webrtc/video/send_statistics_proxy.h"
#include "webrtc/video_frame.h"
namespace webrtc {

namespace {
using DegradationPreference = VideoSendStream::DegradationPreference;

// Time interval for logging frame counts.
const int64_t kFrameLogIntervalMs = 60000;

// We will never ask for a resolution lower than this.
// TODO(kthelgason): Lower this limit when better testing
// on MediaCodec and fallback implementations are in place.
// See https://bugs.chromium.org/p/webrtc/issues/detail?id=7206
const int kMinPixelsPerFrame = 320 * 180;

// The maximum number of frames to drop at beginning of stream
// to try and achieve desired bitrate.
const int kMaxInitialFramedrop = 4;

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

uint32_t MaximumFrameSizeForBitrate(uint32_t kbps) {
  if (kbps > 0) {
    if (kbps < 300 /* qvga */) {
      return 320 * 240;
    } else if (kbps < 500 /* vga */) {
      return 640 * 480;
    }
  }
  return std::numeric_limits<uint32_t>::max();
}

}  //  namespace

class ViEEncoder::ConfigureEncoderTask : public rtc::QueuedTask {
 public:
  ConfigureEncoderTask(ViEEncoder* vie_encoder,
                       VideoEncoderConfig config,
                       size_t max_data_payload_length,
                       bool nack_enabled)
      : vie_encoder_(vie_encoder),
        config_(std::move(config)),
        max_data_payload_length_(max_data_payload_length),
        nack_enabled_(nack_enabled) {}

 private:
  bool Run() override {
    vie_encoder_->ConfigureEncoderOnTaskQueue(
        std::move(config_), max_data_payload_length_, nack_enabled_);
    return true;
  }

  ViEEncoder* const vie_encoder_;
  VideoEncoderConfig config_;
  size_t max_data_payload_length_;
  bool nack_enabled_;
};

class ViEEncoder::EncodeTask : public rtc::QueuedTask {
 public:
  EncodeTask(const VideoFrame& frame,
             ViEEncoder* vie_encoder,
             int64_t time_when_posted_us,
             bool log_stats)
      : frame_(frame),
        vie_encoder_(vie_encoder),
        time_when_posted_us_(time_when_posted_us),
        log_stats_(log_stats) {
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
      vie_encoder_->EncodeVideoFrame(frame_, time_when_posted_us_);
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
  const int64_t time_when_posted_us_;
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
        degradation_preference_(DegradationPreference::kMaintainResolution),
        source_(nullptr) {}

  void SetSource(rtc::VideoSourceInterface<VideoFrame>* source,
                 const DegradationPreference& degradation_preference) {
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
    const int pixels_wanted = (pixel_count * 3) / 5;
    if (pixels_wanted < kMinPixelsPerFrame)
      return;
    sink_wants_.max_pixel_count = rtc::Optional<int>(pixels_wanted);
    sink_wants_.target_pixel_count = rtc::Optional<int>();
    if (source_)
      source_->AddOrUpdateSink(vie_encoder_, sink_wants_);
  }

  void RequestHigherResolutionThan(int pixel_count) {
    rtc::CritScope lock(&crit_);
    if (!IsResolutionScalingEnabledLocked()) {
      // This can happen since |degradation_preference_| is set on
      // libjingle's worker thread but the adaptation is done on the encoder
      // task queue.
      return;
    }
    // On step down we request at most 3/5 the pixel count of the previous
    // resolution, so in order to take "one step up" we request a resolution as
    // close as possible to 5/3 of the current resolution. The actual pixel
    // count selected depends on the capabilities of the source. In order to not
    // take a too large step up, we cap the requested pixel count to be at most
    // four time the current number of pixels.
    sink_wants_.target_pixel_count = rtc::Optional<int>((pixel_count * 5) / 3);
    sink_wants_.max_pixel_count = rtc::Optional<int>(pixel_count * 4);
    if (source_)
      source_->AddOrUpdateSink(vie_encoder_, sink_wants_);
  }

 private:
  bool IsResolutionScalingEnabledLocked() const
      EXCLUSIVE_LOCKS_REQUIRED(&crit_) {
    return degradation_preference_ !=
           DegradationPreference::kMaintainResolution;
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
  DegradationPreference degradation_preference_ GUARDED_BY(&crit_);
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
      initial_rampup_(0),
      source_proxy_(new VideoSourceProxy(this)),
      sink_(nullptr),
      settings_(settings),
      codec_type_(PayloadNameToCodecType(settings.payload_name)
                      .value_or(VideoCodecType::kVideoCodecUnknown)),
      video_sender_(Clock::GetRealTimeClock(), this, this),
      overuse_detector_(GetCpuOveruseOptions(settings.full_overuse_time),
                        this,
                        encoder_timing,
                        stats_proxy),
      stats_proxy_(stats_proxy),
      pre_encode_callback_(pre_encode_callback),
      module_process_thread_(nullptr),
      pending_encoder_reconfiguration_(false),
      encoder_start_bitrate_bps_(0),
      max_data_payload_length_(0),
      nack_enabled_(false),
      last_observed_bitrate_bps_(0),
      encoder_paused_and_dropped_frame_(false),
      has_received_sli_(false),
      picture_id_sli_(0),
      has_received_rpsi_(false),
      picture_id_rpsi_(0),
      clock_(Clock::GetRealTimeClock()),
      scale_counter_(kScaleReasonSize, 0),
      degradation_preference_(DegradationPreference::kMaintainResolution),
      last_captured_timestamp_(0),
      delta_ntp_internal_ms_(clock_->CurrentNtpInMilliseconds() -
                             clock_->TimeInMilliseconds()),
      last_frame_log_ms_(clock_->TimeInMilliseconds()),
      captured_frame_count_(0),
      dropped_frame_count_(0),
      bitrate_observer_(nullptr),
      encoder_queue_("EncoderQueue") {
  RTC_DCHECK(stats_proxy);
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
  source_proxy_->SetSource(nullptr, DegradationPreference());
  encoder_queue_.PostTask([this] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    overuse_detector_.StopCheckForOveruse();
    rate_allocator_.reset();
    bitrate_observer_ = nullptr;
    video_sender_.RegisterExternalEncoder(nullptr, settings_.payload_type,
                                          false);
    quality_scaler_ = nullptr;
    shutdown_event_.Set();
  });

  shutdown_event_.Wait(rtc::Event::kForever);
}

void ViEEncoder::RegisterProcessThread(ProcessThread* module_process_thread) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(!module_process_thread_);
  module_process_thread_ = module_process_thread;
  module_process_thread_->RegisterModule(&video_sender_, RTC_FROM_HERE);
  module_process_thread_checker_.DetachFromThread();
}

void ViEEncoder::DeRegisterProcessThread() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  module_process_thread_->DeRegisterModule(&video_sender_);
}

void ViEEncoder::SetBitrateObserver(
    VideoBitrateAllocationObserver* bitrate_observer) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  encoder_queue_.PostTask([this, bitrate_observer] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    RTC_DCHECK(!bitrate_observer_);
    bitrate_observer_ = bitrate_observer;
  });
}

void ViEEncoder::SetSource(
    rtc::VideoSourceInterface<VideoFrame>* source,
    const VideoSendStream::DegradationPreference& degradation_preference) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  source_proxy_->SetSource(source, degradation_preference);
  encoder_queue_.PostTask([this, degradation_preference] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);

    degradation_preference_ = degradation_preference;
    initial_rampup_ =
        degradation_preference_ != DegradationPreference::kMaintainResolution
            ? 0
            : kMaxInitialFramedrop;
    ConfigureQualityScaler();
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
                                  size_t max_data_payload_length,
                                  bool nack_enabled) {
  encoder_queue_.PostTask(
      std::unique_ptr<rtc::QueuedTask>(new ConfigureEncoderTask(
          this, std::move(config), max_data_payload_length, nack_enabled)));
}

void ViEEncoder::ConfigureEncoderOnTaskQueue(VideoEncoderConfig config,
                                             size_t max_data_payload_length,
                                             bool nack_enabled) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  RTC_DCHECK(sink_);
  LOG(LS_INFO) << "ConfigureEncoder requested.";

  max_data_payload_length_ = max_data_payload_length;
  nack_enabled_ = nack_enabled;
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

  VideoCodec codec;
  if (!VideoCodecInitializer::SetupCodec(encoder_config_, settings_, streams,
                                         nack_enabled_, &codec,
                                         &rate_allocator_)) {
    LOG(LS_ERROR) << "Failed to create encoder configuration.";
  }

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

  video_sender_.UpdateChannelParemeters(rate_allocator_.get(),
                                        bitrate_observer_);

  int framerate = stats_proxy_->GetSendFrameRate();
  if (framerate == 0)
    framerate = codec.maxFramerate;
  stats_proxy_->OnEncoderReconfigured(
      encoder_config_, rate_allocator_->GetPreferredBitrateBps(framerate));

  pending_encoder_reconfiguration_ = false;

  sink_->OnEncoderConfigurationChanged(
      std::move(streams), encoder_config_.min_transmit_bitrate_bps);

  ConfigureQualityScaler();
}

void ViEEncoder::ConfigureQualityScaler() {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  const auto scaling_settings = settings_.encoder->GetScalingSettings();
  const bool degradation_preference_allows_scaling =
      degradation_preference_ != DegradationPreference::kMaintainResolution;
  if (degradation_preference_allows_scaling && scaling_settings.enabled) {
    // Drop frames and scale down until desired quality is achieved.
    if (scaling_settings.thresholds) {
      quality_scaler_.reset(
          new QualityScaler(this, *(scaling_settings.thresholds)));
    } else {
      quality_scaler_.reset(new QualityScaler(this, codec_type_));
    }
  } else {
    quality_scaler_.reset(nullptr);
    initial_rampup_ = kMaxInitialFramedrop;
  }
  stats_proxy_->SetResolutionRestrictionStats(
      degradation_preference_allows_scaling, scale_counter_[kCpu] > 0,
      scale_counter_[kQuality]);
}

void ViEEncoder::OnFrame(const VideoFrame& video_frame) {
  RTC_DCHECK_RUNS_SERIALIZED(&incoming_frame_race_checker_);
  VideoFrame incoming_frame = video_frame;

  // Local time in webrtc time base.
  int64_t current_time_us = clock_->TimeInMicroseconds();
  int64_t current_time_ms = current_time_us / rtc::kNumMicrosecsPerMillisec;
  // TODO(nisse): This always overrides the incoming timestamp. Don't
  // do that, trust the frame source.
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
    LOG(LS_WARNING) << "Same/old NTP timestamp ("
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
  encoder_queue_.PostTask(std::unique_ptr<rtc::QueuedTask>(new EncodeTask(
      incoming_frame, this, rtc::TimeMicros(), log_stats)));
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
                                  int64_t time_when_posted_us) {
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

  if (initial_rampup_ < kMaxInitialFramedrop &&
      video_frame.size() >
          MaximumFrameSizeForBitrate(encoder_start_bitrate_bps_ / 1000)) {
    LOG(LS_INFO) << "Dropping frame. Too large for target bitrate.";
    AdaptDown(kQuality);
    ++initial_rampup_;
    return;
  }
  initial_rampup_ = kMaxInitialFramedrop;

  int64_t now_ms = clock_->TimeInMilliseconds();
  if (pending_encoder_reconfiguration_) {
    ReconfigureEncoder();
  } else if (!last_parameters_update_ms_ ||
             now_ms - *last_parameters_update_ms_ >=
                 vcm::VCMProcessTimer::kDefaultProcessIntervalMs) {
    video_sender_.UpdateChannelParemeters(rate_allocator_.get(),
                                          bitrate_observer_);
  }
  last_parameters_update_ms_.emplace(now_ms);

  if (EncoderPaused()) {
    TraceFrameDropStart();
    return;
  }
  TraceFrameDropEnd();

  TRACE_EVENT_ASYNC_STEP0("webrtc", "Video", video_frame.render_time_ms(),
                          "Encode");

  overuse_detector_.FrameCaptured(video_frame, time_when_posted_us);

  if (codec_type_ == webrtc::kVideoCodecVP8) {
    webrtc::CodecSpecificInfo codec_specific_info;
    codec_specific_info.codecType = webrtc::kVideoCodecVP8;

    codec_specific_info.codecSpecific.VP8.hasReceivedRPSI = has_received_rpsi_;
    codec_specific_info.codecSpecific.VP8.hasReceivedSLI = has_received_sli_;
    codec_specific_info.codecSpecific.VP8.pictureIdRPSI = picture_id_rpsi_;
    codec_specific_info.codecSpecific.VP8.pictureIdSLI = picture_id_sli_;
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
  stats_proxy_->OnSendEncodedImage(encoded_image, codec_specific_info);

  EncodedImageCallback::Result result =
      sink_->OnEncodedImage(encoded_image, codec_specific_info, fragmentation);

  int64_t time_sent_us = rtc::TimeMicros();
  uint32_t timestamp = encoded_image._timeStamp;
  const int qp = encoded_image.qp_;
  encoder_queue_.PostTask([this, timestamp, time_sent_us, qp] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    overuse_detector_.FrameSent(timestamp, time_sent_us);
    if (quality_scaler_ && qp >= 0)
      quality_scaler_->ReportQP(qp);
  });

  return result;
}

void ViEEncoder::OnDroppedFrame() {
  encoder_queue_.PostTask([this] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    if (quality_scaler_)
      quality_scaler_->ReportDroppedFrame();
  });
}

void ViEEncoder::SendStatistics(uint32_t bit_rate, uint32_t frame_rate) {
  RTC_DCHECK(module_process_thread_checker_.CalledOnValidThread());
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
                                     round_trip_time_ms, rate_allocator_.get(),
                                     bitrate_observer_);

  encoder_start_bitrate_bps_ =
      bitrate_bps != 0 ? bitrate_bps : encoder_start_bitrate_bps_;
  bool video_is_suspended = bitrate_bps == 0;
  bool video_suspension_changed = video_is_suspended != EncoderPaused();
  last_observed_bitrate_bps_ = bitrate_bps;

  if (video_suspension_changed) {
    LOG(LS_INFO) << "Video suspend state changed to: "
                 << (video_is_suspended ? "suspended" : "not suspended");
    stats_proxy_->OnSuspendChange(video_is_suspended);
  }
}

void ViEEncoder::AdaptDown(AdaptReason reason) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  if (degradation_preference_ != DegradationPreference::kBalanced)
    return;
  RTC_DCHECK(static_cast<bool>(last_frame_info_));
  int current_pixel_count = last_frame_info_->pixel_count();
  if (last_adaptation_request_ &&
      last_adaptation_request_->mode_ == AdaptationRequest::Mode::kAdaptDown &&
      current_pixel_count >= last_adaptation_request_->input_pixel_count_) {
    // Don't request lower resolution if the current resolution is not lower
    // than the last time we asked for the resolution to be lowered.
    return;
  }
  last_adaptation_request_.emplace(AdaptationRequest{
      current_pixel_count, AdaptationRequest::Mode::kAdaptDown});

  switch (reason) {
    case kQuality:
      stats_proxy_->OnQualityRestrictedResolutionChanged(
          scale_counter_[reason] + 1);
      break;
    case kCpu:
      if (scale_counter_[reason] >= kMaxCpuDowngrades)
        return;
      // Update stats accordingly.
      stats_proxy_->OnCpuRestrictedResolutionChanged(true);
      break;
  }
  ++scale_counter_[reason];
  source_proxy_->RequestResolutionLowerThan(current_pixel_count);
  LOG(LS_INFO) << "Scaling down resolution.";
  for (size_t i = 0; i < kScaleReasonSize; ++i) {
    LOG(LS_INFO) << "Scaled " << scale_counter_[i]
                 << " times for reason: " << (i ? "cpu" : "quality");
  }
}

void ViEEncoder::AdaptUp(AdaptReason reason) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  if (scale_counter_[reason] == 0 ||
      degradation_preference_ != DegradationPreference::kBalanced) {
    return;
  }
  // Only scale if resolution is higher than last time we requested higher
  // resolution.
  RTC_DCHECK(static_cast<bool>(last_frame_info_));
  int current_pixel_count = last_frame_info_->pixel_count();
  if (last_adaptation_request_ &&
      last_adaptation_request_->mode_ == AdaptationRequest::Mode::kAdaptUp &&
      current_pixel_count <= last_adaptation_request_->input_pixel_count_) {
    // Don't request higher resolution if the current resolution is not higher
    // than the last time we asked for the resolution to be higher.
    return;
  }
  last_adaptation_request_.emplace(AdaptationRequest{
      current_pixel_count, AdaptationRequest::Mode::kAdaptUp});

  switch (reason) {
    case kQuality:
      stats_proxy_->OnQualityRestrictedResolutionChanged(
          scale_counter_[reason] - 1);
      break;
    case kCpu:
      // Update stats accordingly.
      stats_proxy_->OnCpuRestrictedResolutionChanged(scale_counter_[reason] >
                                                     1);
      break;
  }
  --scale_counter_[reason];
  source_proxy_->RequestHigherResolutionThan(current_pixel_count);
  LOG(LS_INFO) << "Scaling up resolution.";
  for (size_t i = 0; i < kScaleReasonSize; ++i) {
    LOG(LS_INFO) << "Scaled " << scale_counter_[i]
                 << " times for reason: " << (i ? "cpu" : "quality");
  }
}

}  // namespace webrtc
