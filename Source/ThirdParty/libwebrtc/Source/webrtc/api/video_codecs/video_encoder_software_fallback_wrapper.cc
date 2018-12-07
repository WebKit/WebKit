/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_encoder_software_fallback_wrapper.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "media/base/codec.h"
#include "media/base/h264_profile_level_id.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {

const char kVp8ForceFallbackEncoderFieldTrial[] =
    "WebRTC-VP8-Forced-Fallback-Encoder-v2";

bool EnableForcedFallback() {
  return field_trial::IsEnabled(kVp8ForceFallbackEncoderFieldTrial);
}

bool IsForcedFallbackPossible(const VideoCodec& codec_settings) {
  return codec_settings.codecType == kVideoCodecVP8 &&
         codec_settings.numberOfSimulcastStreams <= 1 &&
         codec_settings.VP8().numberOfTemporalLayers == 1;
}

void GetForcedFallbackParamsFromFieldTrialGroup(int* param_min_pixels,
                                                int* param_max_pixels,
                                                int minimum_max_pixels) {
  RTC_DCHECK(param_min_pixels);
  RTC_DCHECK(param_max_pixels);
  std::string group =
      webrtc::field_trial::FindFullName(kVp8ForceFallbackEncoderFieldTrial);
  if (group.empty())
    return;

  int min_pixels;
  int max_pixels;
  int min_bps;
  if (sscanf(group.c_str(), "Enabled-%d,%d,%d", &min_pixels, &max_pixels,
             &min_bps) != 3) {
    RTC_LOG(LS_WARNING)
        << "Invalid number of forced fallback parameters provided.";
    return;
  }
  if (min_pixels <= 0 || max_pixels < minimum_max_pixels ||
      max_pixels < min_pixels || min_bps <= 0) {
    RTC_LOG(LS_WARNING) << "Invalid forced fallback parameter value provided.";
    return;
  }
  *param_min_pixels = min_pixels;
  *param_max_pixels = max_pixels;
}

class VideoEncoderSoftwareFallbackWrapper final : public VideoEncoder {
 public:
  VideoEncoderSoftwareFallbackWrapper(
      std::unique_ptr<webrtc::VideoEncoder> sw_encoder,
      std::unique_ptr<webrtc::VideoEncoder> hw_encoder);
  ~VideoEncoderSoftwareFallbackWrapper() override;

  int32_t InitEncode(const VideoCodec* codec_settings,
                     int32_t number_of_cores,
                     size_t max_payload_size) override;

  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override;

  int32_t Release() override;
  int32_t Encode(const VideoFrame& frame,
                 const CodecSpecificInfo* codec_specific_info,
                 const std::vector<FrameType>* frame_types) override;
  int32_t SetRateAllocation(const VideoBitrateAllocation& bitrate_allocation,
                            uint32_t framerate) override;
  EncoderInfo GetEncoderInfo() const override;

 private:
  bool InitFallbackEncoder();

  // If |forced_fallback_possible_| is true:
  // The forced fallback is requested if the resolution is less than or equal to
  // |max_pixels_|. The resolution is allowed to be scaled down to
  // |min_pixels_|.
  class ForcedFallbackParams {
   public:
    bool IsValid(const VideoCodec& codec) const {
      return codec.width * codec.height <= max_pixels_;
    }

    bool active_ = false;
    int min_pixels_ = 320 * 180;
    int max_pixels_ = 320 * 240;
  };

  bool TryInitForcedFallbackEncoder();
  bool TryReInitForcedFallbackEncoder();
  void ValidateSettingsForForcedFallback();
  bool IsForcedFallbackActive() const;
  void MaybeModifyCodecForFallback();

  // Settings used in the last InitEncode call and used if a dynamic fallback to
  // software is required.
  VideoCodec codec_settings_;
  int32_t number_of_cores_;
  size_t max_payload_size_;

  // The last bitrate/framerate set, and a flag for noting they are set.
  bool rates_set_;
  VideoBitrateAllocation bitrate_allocation_;
  uint32_t framerate_;

  // The last channel parameters set, and a flag for noting they are set.
  bool channel_parameters_set_;
  uint32_t packet_loss_;
  int64_t rtt_;

  bool use_fallback_encoder_;
  const std::unique_ptr<webrtc::VideoEncoder> encoder_;

  const std::unique_ptr<webrtc::VideoEncoder> fallback_encoder_;
  EncodedImageCallback* callback_;

  bool forced_fallback_possible_;
  ForcedFallbackParams forced_fallback_;
};

VideoEncoderSoftwareFallbackWrapper::VideoEncoderSoftwareFallbackWrapper(
    std::unique_ptr<webrtc::VideoEncoder> sw_encoder,
    std::unique_ptr<webrtc::VideoEncoder> hw_encoder)
    : number_of_cores_(0),
      max_payload_size_(0),
      rates_set_(false),
      framerate_(0),
      channel_parameters_set_(false),
      packet_loss_(0),
      rtt_(0),
      use_fallback_encoder_(false),
      encoder_(std::move(hw_encoder)),
      fallback_encoder_(std::move(sw_encoder)),
      callback_(nullptr),
      forced_fallback_possible_(EnableForcedFallback()) {
  if (forced_fallback_possible_) {
    GetForcedFallbackParamsFromFieldTrialGroup(
        &forced_fallback_.min_pixels_, &forced_fallback_.max_pixels_,
        encoder_->GetEncoderInfo().scaling_settings.min_pixels_per_frame -
            1);  // No HW below.
  }
}
VideoEncoderSoftwareFallbackWrapper::~VideoEncoderSoftwareFallbackWrapper() =
    default;

bool VideoEncoderSoftwareFallbackWrapper::InitFallbackEncoder() {
  RTC_LOG(LS_WARNING) << "Encoder falling back to software encoding.";

  const int ret = fallback_encoder_->InitEncode(
      &codec_settings_, number_of_cores_, max_payload_size_);
  use_fallback_encoder_ = (ret == WEBRTC_VIDEO_CODEC_OK);
  if (!use_fallback_encoder_) {
    RTC_LOG(LS_ERROR) << "Failed to initialize software-encoder fallback.";
    fallback_encoder_->Release();
    return false;
  }
  // Replay callback, rates, and channel parameters.
  if (callback_)
    fallback_encoder_->RegisterEncodeCompleteCallback(callback_);
  if (rates_set_)
    fallback_encoder_->SetRateAllocation(bitrate_allocation_, framerate_);

  // Since we're switching to the fallback encoder, Release the real encoder. It
  // may be re-initialized via InitEncode later, and it will continue to get
  // Set calls for rates and channel parameters in the meantime.
  encoder_->Release();
  return true;
}

int32_t VideoEncoderSoftwareFallbackWrapper::InitEncode(
    const VideoCodec* codec_settings,
    int32_t number_of_cores,
    size_t max_payload_size) {
  // Store settings, in case we need to dynamically switch to the fallback
  // encoder after a failed Encode call.
  codec_settings_ = *codec_settings;
  number_of_cores_ = number_of_cores;
  max_payload_size_ = max_payload_size;
  // Clear stored rate/channel parameters.
  rates_set_ = false;
  ValidateSettingsForForcedFallback();

  // Try to reinit forced software codec if it is in use.
  if (TryReInitForcedFallbackEncoder()) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  // Try to init forced software codec if it should be used.
  if (TryInitForcedFallbackEncoder()) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  forced_fallback_.active_ = false;

  int32_t ret =
      encoder_->InitEncode(codec_settings, number_of_cores, max_payload_size);
  if (ret == WEBRTC_VIDEO_CODEC_OK) {
    if (use_fallback_encoder_) {
      RTC_LOG(LS_WARNING)
          << "InitEncode OK, no longer using the software fallback encoder.";
      fallback_encoder_->Release();
      use_fallback_encoder_ = false;
    }
    if (callback_)
      encoder_->RegisterEncodeCompleteCallback(callback_);
    return ret;
  }
  // Try to instantiate software codec.
  if (InitFallbackEncoder()) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  // Software encoder failed, use original return code.
  return ret;
}

int32_t VideoEncoderSoftwareFallbackWrapper::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  callback_ = callback;
  int32_t ret = encoder_->RegisterEncodeCompleteCallback(callback);
  if (use_fallback_encoder_)
    return fallback_encoder_->RegisterEncodeCompleteCallback(callback);
  return ret;
}

int32_t VideoEncoderSoftwareFallbackWrapper::Release() {
  return use_fallback_encoder_ ? fallback_encoder_->Release()
                               : encoder_->Release();
}

int32_t VideoEncoderSoftwareFallbackWrapper::Encode(
    const VideoFrame& frame,
    const CodecSpecificInfo* codec_specific_info,
    const std::vector<FrameType>* frame_types) {
  if (use_fallback_encoder_)
    return fallback_encoder_->Encode(frame, codec_specific_info, frame_types);
  int32_t ret = encoder_->Encode(frame, codec_specific_info, frame_types);
  // If requested, try a software fallback.
  bool fallback_requested = (ret == WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE);
  if (fallback_requested && InitFallbackEncoder()) {
    // Start using the fallback with this frame.
    return fallback_encoder_->Encode(frame, codec_specific_info, frame_types);
  }
  return ret;
}

int32_t VideoEncoderSoftwareFallbackWrapper::SetRateAllocation(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  rates_set_ = true;
  bitrate_allocation_ = bitrate_allocation;
  framerate_ = framerate;
  int32_t ret = encoder_->SetRateAllocation(bitrate_allocation_, framerate);
  if (use_fallback_encoder_)
    return fallback_encoder_->SetRateAllocation(bitrate_allocation_, framerate);
  return ret;
}

VideoEncoder::EncoderInfo VideoEncoderSoftwareFallbackWrapper::GetEncoderInfo()
    const {
  EncoderInfo fallback_encoder_info = fallback_encoder_->GetEncoderInfo();
  EncoderInfo default_encoder_info = encoder_->GetEncoderInfo();

  EncoderInfo info =
      use_fallback_encoder_ ? fallback_encoder_info : default_encoder_info;

  if (forced_fallback_possible_) {
    const auto settings = forced_fallback_.active_
                              ? fallback_encoder_info.scaling_settings
                              : default_encoder_info.scaling_settings;
    info.scaling_settings =
        settings.thresholds
            ? VideoEncoder::ScalingSettings(settings.thresholds->low,
                                            settings.thresholds->high,
                                            forced_fallback_.min_pixels_)
            : VideoEncoder::ScalingSettings::kOff;
  } else {
    info.scaling_settings = default_encoder_info.scaling_settings;
  }

  return info;
}

bool VideoEncoderSoftwareFallbackWrapper::IsForcedFallbackActive() const {
  return (forced_fallback_possible_ && use_fallback_encoder_ &&
          forced_fallback_.active_);
}

bool VideoEncoderSoftwareFallbackWrapper::TryInitForcedFallbackEncoder() {
  if (!forced_fallback_possible_ || use_fallback_encoder_) {
    return false;
  }
  // Fallback not active.
  if (!forced_fallback_.IsValid(codec_settings_)) {
    return false;
  }
  // Settings valid, try to instantiate software codec.
  RTC_LOG(LS_INFO) << "Request forced SW encoder fallback: "
                   << codec_settings_.width << "x" << codec_settings_.height;
  if (!InitFallbackEncoder()) {
    return false;
  }
  forced_fallback_.active_ = true;
  return true;
}

bool VideoEncoderSoftwareFallbackWrapper::TryReInitForcedFallbackEncoder() {
  if (!IsForcedFallbackActive()) {
    return false;
  }
  // Forced fallback active.
  if (!forced_fallback_.IsValid(codec_settings_)) {
    RTC_LOG(LS_INFO) << "Stop forced SW encoder fallback, max pixels exceeded.";
    return false;
  }
  // Settings valid, reinitialize the forced fallback encoder.
  if (fallback_encoder_->InitEncode(&codec_settings_, number_of_cores_,
                                    max_payload_size_) !=
      WEBRTC_VIDEO_CODEC_OK) {
    RTC_LOG(LS_ERROR) << "Failed to init forced SW encoder fallback.";
    return false;
  }
  return true;
}

void VideoEncoderSoftwareFallbackWrapper::ValidateSettingsForForcedFallback() {
  if (!forced_fallback_possible_)
    return;

  if (!IsForcedFallbackPossible(codec_settings_)) {
    if (IsForcedFallbackActive()) {
      fallback_encoder_->Release();
      use_fallback_encoder_ = false;
    }
    RTC_LOG(LS_INFO) << "Disable forced_fallback_possible_ due to settings.";
    forced_fallback_possible_ = false;
  }
}

}  // namespace

std::unique_ptr<VideoEncoder> CreateVideoEncoderSoftwareFallbackWrapper(
    std::unique_ptr<VideoEncoder> sw_fallback_encoder,
    std::unique_ptr<VideoEncoder> hw_encoder) {
  return absl::make_unique<VideoEncoderSoftwareFallbackWrapper>(
      std::move(sw_fallback_encoder), std::move(hw_encoder));
}

}  // namespace webrtc
