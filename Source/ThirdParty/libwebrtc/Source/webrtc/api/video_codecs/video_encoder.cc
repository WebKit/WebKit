/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_encoder.h"

#include <string.h>

#include "rtc_base/checks.h"

namespace webrtc {

// TODO(mflodman): Add default complexity for VP9 and VP9.
VideoCodecVP8 VideoEncoder::GetDefaultVp8Settings() {
  VideoCodecVP8 vp8_settings;
  memset(&vp8_settings, 0, sizeof(vp8_settings));

  vp8_settings.numberOfTemporalLayers = 1;
  vp8_settings.denoisingOn = true;
  vp8_settings.automaticResizeOn = false;
  vp8_settings.frameDroppingOn = true;
  vp8_settings.keyFrameInterval = 3000;

  return vp8_settings;
}

VideoCodecVP9 VideoEncoder::GetDefaultVp9Settings() {
  VideoCodecVP9 vp9_settings;
  memset(&vp9_settings, 0, sizeof(vp9_settings));

  vp9_settings.numberOfTemporalLayers = 1;
  vp9_settings.denoisingOn = true;
  vp9_settings.frameDroppingOn = true;
  vp9_settings.keyFrameInterval = 3000;
  vp9_settings.adaptiveQpMode = true;
  vp9_settings.automaticResizeOn = true;
  vp9_settings.numberOfSpatialLayers = 1;
  vp9_settings.flexibleMode = false;
  vp9_settings.interLayerPred = InterLayerPredMode::kOn;

  return vp9_settings;
}

VideoCodecH264 VideoEncoder::GetDefaultH264Settings() {
  VideoCodecH264 h264_settings;
  memset(&h264_settings, 0, sizeof(h264_settings));

  h264_settings.frameDroppingOn = true;
  h264_settings.keyFrameInterval = 3000;
  h264_settings.spsData = nullptr;
  h264_settings.spsLen = 0;
  h264_settings.ppsData = nullptr;
  h264_settings.ppsLen = 0;
  h264_settings.profile = H264::kProfileConstrainedBaseline;

  return h264_settings;
}

VideoEncoder::ScalingSettings::ScalingSettings() = default;

VideoEncoder::ScalingSettings::ScalingSettings(KOff) : ScalingSettings() {}

VideoEncoder::ScalingSettings::ScalingSettings(int low, int high)
    : thresholds(QpThresholds(low, high)) {}

VideoEncoder::ScalingSettings::ScalingSettings(int low,
                                               int high,
                                               int min_pixels)
    : thresholds(QpThresholds(low, high)), min_pixels_per_frame(min_pixels) {}

VideoEncoder::ScalingSettings::ScalingSettings(const ScalingSettings&) =
    default;

VideoEncoder::ScalingSettings::~ScalingSettings() {}

// static
constexpr VideoEncoder::ScalingSettings::KOff
    VideoEncoder::ScalingSettings::kOff;

VideoEncoder::EncoderInfo::EncoderInfo()
    : scaling_settings(VideoEncoder::ScalingSettings::kOff),
      supports_native_handle(false),
      implementation_name("unknown"),
      has_trusted_rate_controller(false) {}

VideoEncoder::EncoderInfo::~EncoderInfo() = default;

int32_t VideoEncoder::SetRates(uint32_t bitrate, uint32_t framerate) {
  RTC_NOTREACHED() << "SetRate(uint32_t, uint32_t) is deprecated.";
  return -1;
}

int32_t VideoEncoder::SetRateAllocation(
    const VideoBitrateAllocation& allocation,
    uint32_t framerate) {
  return SetRates(allocation.get_sum_kbps(), framerate);
}

VideoEncoder::ScalingSettings VideoEncoder::GetScalingSettings() const {
  return ScalingSettings::kOff;
}

bool VideoEncoder::SupportsNativeHandle() const {
  return false;
}

const char* VideoEncoder::ImplementationName() const {
  return "unknown";
}

// TODO(webrtc:9722): Remove and make pure virtual when the three legacy
// methods called here are gone.
VideoEncoder::EncoderInfo VideoEncoder::GetEncoderInfo() const {
  EncoderInfo info;
  info.scaling_settings = GetScalingSettings();
  info.supports_native_handle = SupportsNativeHandle();
  info.implementation_name = ImplementationName();
  return info;
}
}  // namespace webrtc
