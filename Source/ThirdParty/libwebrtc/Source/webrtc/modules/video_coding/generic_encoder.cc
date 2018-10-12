/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/generic_encoder.h"

#include <vector>

#include "absl/types/optional.h"
#include "api/video/i420_buffer.h"
#include "modules/include/module_common_types_public.h"
#include "modules/video_coding/encoded_frame.h"
#include "modules/video_coding/media_optimization.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/alr_experiment.h"
#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {
const int kMessagesThrottlingThreshold = 2;
const int kThrottleRatio = 100000;
}  // namespace

VCMEncodedFrameCallback::TimingFramesLayerInfo::TimingFramesLayerInfo() {}
VCMEncodedFrameCallback::TimingFramesLayerInfo::~TimingFramesLayerInfo() {}

VCMGenericEncoder::VCMGenericEncoder(
    VideoEncoder* encoder,
    VCMEncodedFrameCallback* encoded_frame_callback,
    bool internal_source)
    : encoder_(encoder),
      vcm_encoded_frame_callback_(encoded_frame_callback),
      internal_source_(internal_source),
      encoder_params_({VideoBitrateAllocation(), 0, 0, 0}),
      streams_or_svc_num_(0),
      codec_type_(VideoCodecType::kVideoCodecGeneric) {}

VCMGenericEncoder::~VCMGenericEncoder() {}

int32_t VCMGenericEncoder::Release() {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  TRACE_EVENT0("webrtc", "VCMGenericEncoder::Release");
  return encoder_->Release();
}

int32_t VCMGenericEncoder::InitEncode(const VideoCodec* settings,
                                      int32_t number_of_cores,
                                      size_t max_payload_size) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  TRACE_EVENT0("webrtc", "VCMGenericEncoder::InitEncode");
  streams_or_svc_num_ = settings->numberOfSimulcastStreams;
  codec_type_ = settings->codecType;
  if (settings->codecType == kVideoCodecVP9) {
    streams_or_svc_num_ = settings->VP9().numberOfSpatialLayers;
  }
  if (streams_or_svc_num_ == 0)
    streams_or_svc_num_ = 1;

  vcm_encoded_frame_callback_->SetTimingFramesThresholds(
      settings->timing_frame_thresholds);
  vcm_encoded_frame_callback_->OnFrameRateChanged(settings->maxFramerate);

  if (encoder_->InitEncode(settings, number_of_cores, max_payload_size) != 0) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the encoder associated with "
                         "codec type: "
                      << CodecTypeToPayloadString(settings->codecType) << " ("
                      << settings->codecType << ")";
    return -1;
  }
  vcm_encoded_frame_callback_->Reset();
  encoder_->RegisterEncodeCompleteCallback(vcm_encoded_frame_callback_);
  return 0;
}

int32_t VCMGenericEncoder::Encode(const VideoFrame& frame,
                                  const CodecSpecificInfo* codec_specific,
                                  const std::vector<FrameType>& frame_types) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  TRACE_EVENT1("webrtc", "VCMGenericEncoder::Encode", "timestamp",
               frame.timestamp());

  for (FrameType frame_type : frame_types)
    RTC_DCHECK(frame_type == kVideoFrameKey || frame_type == kVideoFrameDelta);

  for (size_t i = 0; i < streams_or_svc_num_; ++i)
    vcm_encoded_frame_callback_->OnEncodeStarted(frame.timestamp(),
                                                 frame.render_time_ms(), i);

  return encoder_->Encode(frame, codec_specific, &frame_types);
}

void VCMGenericEncoder::SetEncoderParameters(const EncoderParameters& params) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  bool channel_parameters_have_changed;
  bool rates_have_changed;
  {
    rtc::CritScope lock(&params_lock_);
    channel_parameters_have_changed =
        params.loss_rate != encoder_params_.loss_rate ||
        params.rtt != encoder_params_.rtt;
    rates_have_changed =
        params.target_bitrate != encoder_params_.target_bitrate ||
        params.input_frame_rate != encoder_params_.input_frame_rate;
    encoder_params_ = params;
  }
  if (channel_parameters_have_changed) {
    int res = encoder_->SetChannelParameters(params.loss_rate, params.rtt);
    if (res != 0) {
      RTC_LOG(LS_WARNING) << "Error set encoder parameters (loss = "
                          << params.loss_rate << ", rtt = " << params.rtt
                          << "): " << res;
    }
  }
  if (rates_have_changed) {
    int res = encoder_->SetRateAllocation(params.target_bitrate,
                                          params.input_frame_rate);
    if (res != 0) {
      RTC_LOG(LS_WARNING) << "Error set encoder rate (total bitrate bps = "
                          << params.target_bitrate.get_sum_bps()
                          << ", framerate = " << params.input_frame_rate
                          << "): " << res;
    }
    vcm_encoded_frame_callback_->OnFrameRateChanged(params.input_frame_rate);
    for (size_t i = 0; i < streams_or_svc_num_; ++i) {
      vcm_encoded_frame_callback_->OnTargetBitrateChanged(
          params.target_bitrate.GetSpatialLayerSum(i) / 8, i);
    }
  }
}

EncoderParameters VCMGenericEncoder::GetEncoderParameters() const {
  rtc::CritScope lock(&params_lock_);
  return encoder_params_;
}

int32_t VCMGenericEncoder::RequestFrame(
    const std::vector<FrameType>& frame_types) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);

  // TODO(nisse): Used only with internal source. Delete as soon as
  // that feature is removed. The only implementation I've been able
  // to find ignores what's in the frame. With one exception: It seems
  // a few test cases, e.g.,
  // VideoSendStreamTest.VideoSendStreamStopSetEncoderRateToZero, set
  // internal_source to true and use FakeEncoder. And the latter will
  // happily encode this 1x1 frame and pass it on down the pipeline.
  return encoder_->Encode(
      VideoFrame(I420Buffer::Create(1, 1), kVideoRotation_0, 0), NULL,
      &frame_types);
  return 0;
}

bool VCMGenericEncoder::InternalSource() const {
  return internal_source_;
}

bool VCMGenericEncoder::SupportsNativeHandle() const {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  return encoder_->SupportsNativeHandle();
}

VCMEncodedFrameCallback::VCMEncodedFrameCallback(
    EncodedImageCallback* post_encode_callback,
    media_optimization::MediaOptimization* media_opt)
    : internal_source_(false),
      post_encode_callback_(post_encode_callback),
      media_opt_(media_opt),
      framerate_(1),
      last_timing_frame_time_ms_(-1),
      timing_frames_thresholds_({-1, 0}),
      incorrect_capture_time_logged_messages_(0),
      reordered_frames_logged_messages_(0),
      stalled_encoder_logged_messages_(0) {
  absl::optional<AlrExperimentSettings> experiment_settings =
      AlrExperimentSettings::CreateFromFieldTrial(
          AlrExperimentSettings::kStrictPacingAndProbingExperimentName);
  if (experiment_settings) {
    experiment_groups_[0] = experiment_settings->group_id + 1;
  } else {
    experiment_groups_[0] = 0;
  }
  experiment_settings = AlrExperimentSettings::CreateFromFieldTrial(
      AlrExperimentSettings::kScreenshareProbingBweExperimentName);
  if (experiment_settings) {
    experiment_groups_[1] = experiment_settings->group_id + 1;
  } else {
    experiment_groups_[1] = 0;
  }
}

VCMEncodedFrameCallback::~VCMEncodedFrameCallback() {}

void VCMEncodedFrameCallback::OnTargetBitrateChanged(
    size_t bitrate_bytes_per_second,
    size_t simulcast_svc_idx) {
  rtc::CritScope crit(&timing_params_lock_);
  if (timing_frames_info_.size() < simulcast_svc_idx + 1)
    timing_frames_info_.resize(simulcast_svc_idx + 1);
  timing_frames_info_[simulcast_svc_idx].target_bitrate_bytes_per_sec =
      bitrate_bytes_per_second;
}

void VCMEncodedFrameCallback::OnFrameRateChanged(size_t framerate) {
  rtc::CritScope crit(&timing_params_lock_);
  framerate_ = framerate;
}

void VCMEncodedFrameCallback::OnEncodeStarted(uint32_t rtp_timestamp,
                                              int64_t capture_time_ms,
                                              size_t simulcast_svc_idx) {
  if (internal_source_) {
    return;
  }
  rtc::CritScope crit(&timing_params_lock_);
  if (timing_frames_info_.size() < simulcast_svc_idx + 1)
    timing_frames_info_.resize(simulcast_svc_idx + 1);
  RTC_DCHECK(
      timing_frames_info_[simulcast_svc_idx].encode_start_list.empty() ||
      rtc::TimeDiff(capture_time_ms, timing_frames_info_[simulcast_svc_idx]
                                         .encode_start_list.back()
                                         .capture_time_ms) >= 0);
  // If stream is disabled due to low bandwidth OnEncodeStarted still will be
  // called and have to be ignored.
  if (timing_frames_info_[simulcast_svc_idx].target_bitrate_bytes_per_sec == 0)
    return;
  if (timing_frames_info_[simulcast_svc_idx].encode_start_list.size() ==
      kMaxEncodeStartTimeListSize) {
    ++stalled_encoder_logged_messages_;
    if (stalled_encoder_logged_messages_ <= kMessagesThrottlingThreshold ||
        stalled_encoder_logged_messages_ % kThrottleRatio == 0) {
      RTC_LOG(LS_WARNING) << "Too many frames in the encode_start_list."
                             " Did encoder stall?";
      if (stalled_encoder_logged_messages_ == kMessagesThrottlingThreshold) {
        RTC_LOG(LS_WARNING) << "Too many log messages. Further stalled encoder"
                               "warnings will be throttled.";
      }
    }
    post_encode_callback_->OnDroppedFrame(DropReason::kDroppedByEncoder);
    timing_frames_info_[simulcast_svc_idx].encode_start_list.pop_front();
  }
  timing_frames_info_[simulcast_svc_idx].encode_start_list.emplace_back(
      rtp_timestamp, capture_time_ms, rtc::TimeMillis());
}

absl::optional<int64_t> VCMEncodedFrameCallback::ExtractEncodeStartTime(
    size_t simulcast_svc_idx,
    EncodedImage* encoded_image) {
  absl::optional<int64_t> result;
  size_t num_simulcast_svc_streams = timing_frames_info_.size();
  if (simulcast_svc_idx < num_simulcast_svc_streams) {
    auto encode_start_list =
        &timing_frames_info_[simulcast_svc_idx].encode_start_list;
    // Skip frames for which there was OnEncodeStarted but no OnEncodedImage
    // call. These are dropped by encoder internally.
    // Because some hardware encoders don't preserve capture timestamp we
    // use RTP timestamps here.
    while (!encode_start_list->empty() &&
           IsNewerTimestamp(encoded_image->Timestamp(),
                            encode_start_list->front().rtp_timestamp)) {
      post_encode_callback_->OnDroppedFrame(DropReason::kDroppedByEncoder);
      encode_start_list->pop_front();
    }
    if (encode_start_list->size() > 0 &&
        encode_start_list->front().rtp_timestamp ==
            encoded_image->Timestamp()) {
      result.emplace(encode_start_list->front().encode_start_time_ms);
      if (encoded_image->capture_time_ms_ !=
          encode_start_list->front().capture_time_ms) {
        // Force correct capture timestamp.
        encoded_image->capture_time_ms_ =
            encode_start_list->front().capture_time_ms;
        ++incorrect_capture_time_logged_messages_;
        if (incorrect_capture_time_logged_messages_ <=
                kMessagesThrottlingThreshold ||
            incorrect_capture_time_logged_messages_ % kThrottleRatio == 0) {
          RTC_LOG(LS_WARNING)
              << "Encoder is not preserving capture timestamps.";
          if (incorrect_capture_time_logged_messages_ ==
              kMessagesThrottlingThreshold) {
            RTC_LOG(LS_WARNING) << "Too many log messages. Further incorrect "
                                   "timestamps warnings will be throttled.";
          }
        }
      }
      encode_start_list->pop_front();
    } else {
      ++reordered_frames_logged_messages_;
      if (reordered_frames_logged_messages_ <= kMessagesThrottlingThreshold ||
          reordered_frames_logged_messages_ % kThrottleRatio == 0) {
        RTC_LOG(LS_WARNING) << "Frame with no encode started time recordings. "
                               "Encoder may be reordering frames "
                               "or not preserving RTP timestamps.";
        if (reordered_frames_logged_messages_ == kMessagesThrottlingThreshold) {
          RTC_LOG(LS_WARNING) << "Too many log messages. Further frames "
                                 "reordering warnings will be throttled.";
        }
      }
    }
  }
  return result;
}

void VCMEncodedFrameCallback::FillTimingInfo(size_t simulcast_svc_idx,
                                             EncodedImage* encoded_image) {
  absl::optional<size_t> outlier_frame_size;
  absl::optional<int64_t> encode_start_ms;
  uint8_t timing_flags = VideoSendTiming::kNotTriggered;
  {
    rtc::CritScope crit(&timing_params_lock_);

    // Encoders with internal sources do not call OnEncodeStarted
    // |timing_frames_info_| may be not filled here.
    if (!internal_source_) {
      encode_start_ms =
          ExtractEncodeStartTime(simulcast_svc_idx, encoded_image);
    }

    if (timing_frames_info_.size() > simulcast_svc_idx) {
      size_t target_bitrate =
          timing_frames_info_[simulcast_svc_idx].target_bitrate_bytes_per_sec;
      if (framerate_ > 0 && target_bitrate > 0) {
        // framerate and target bitrate were reported by encoder.
        size_t average_frame_size = target_bitrate / framerate_;
        outlier_frame_size.emplace(
            average_frame_size *
            timing_frames_thresholds_.outlier_ratio_percent / 100);
      }
    }

    // Outliers trigger timing frames, but do not affect scheduled timing
    // frames.
    if (outlier_frame_size && encoded_image->_length >= *outlier_frame_size) {
      timing_flags |= VideoSendTiming::kTriggeredBySize;
    }

    // Check if it's time to send a timing frame.
    int64_t timing_frame_delay_ms =
        encoded_image->capture_time_ms_ - last_timing_frame_time_ms_;
    // Trigger threshold if it's a first frame, too long passed since the last
    // timing frame, or we already sent timing frame on a different simulcast
    // stream with the same capture time.
    if (last_timing_frame_time_ms_ == -1 ||
        timing_frame_delay_ms >= timing_frames_thresholds_.delay_ms ||
        timing_frame_delay_ms == 0) {
      timing_flags |= VideoSendTiming::kTriggeredByTimer;
      last_timing_frame_time_ms_ = encoded_image->capture_time_ms_;
    }
  }  // rtc::CritScope crit(&timing_params_lock_);

  int64_t now_ms = rtc::TimeMillis();
  // Workaround for chromoting encoder: it passes encode start and finished
  // timestamps in |timing_| field, but they (together with capture timestamp)
  // are not in the WebRTC clock.
  if (internal_source_ && encoded_image->timing_.encode_finish_ms > 0 &&
      encoded_image->timing_.encode_start_ms > 0) {
    int64_t clock_offset_ms = now_ms - encoded_image->timing_.encode_finish_ms;
    // Translate capture timestamp to local WebRTC clock.
    encoded_image->capture_time_ms_ += clock_offset_ms;
    encoded_image->SetTimestamp(
        static_cast<uint32_t>(encoded_image->capture_time_ms_ * 90));
    encode_start_ms.emplace(encoded_image->timing_.encode_start_ms +
                            clock_offset_ms);
  }

  // If encode start is not available that means that encoder uses internal
  // source. In that case capture timestamp may be from a different clock with a
  // drift relative to rtc::TimeMillis(). We can't use it for Timing frames,
  // because to being sent in the network capture time required to be less than
  // all the other timestamps.
  if (encode_start_ms) {
    encoded_image->SetEncodeTime(*encode_start_ms, now_ms);
    encoded_image->timing_.flags = timing_flags;
  } else {
    encoded_image->timing_.flags = VideoSendTiming::kInvalid;
  }
}

EncodedImageCallback::Result VCMEncodedFrameCallback::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific,
    const RTPFragmentationHeader* fragmentation_header) {
  TRACE_EVENT_INSTANT1("webrtc", "VCMEncodedFrameCallback::Encoded",
                       "timestamp", encoded_image.Timestamp());
  const size_t spatial_idx = encoded_image.SpatialIndex().value_or(0);
  EncodedImage image_copy(encoded_image);

  FillTimingInfo(spatial_idx, &image_copy);

  // Piggyback ALR experiment group id and simulcast id into the content type.
  uint8_t experiment_id =
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

  Result result = post_encode_callback_->OnEncodedImage(
      image_copy, codec_specific, fragmentation_header);
  if (result.error != Result::OK)
    return result;

  if (media_opt_) {
    media_opt_->UpdateWithEncodedData(image_copy._length,
                                      image_copy._frameType);
    if (internal_source_) {
      // Signal to encoder to drop next frame.
      result.drop_next_frame = media_opt_->DropFrame();
    }
  }
  return result;
}

}  // namespace webrtc
