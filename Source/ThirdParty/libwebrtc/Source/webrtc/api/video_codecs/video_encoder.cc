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

namespace webrtc {

// TODO(mflodman): Add default complexity for VP9 and VP9.
VideoCodecVP8 VideoEncoder::GetDefaultVp8Settings() {
  VideoCodecVP8 vp8_settings;
  memset(&vp8_settings, 0, sizeof(vp8_settings));

  vp8_settings.resilience = kResilientStream;
  vp8_settings.numberOfTemporalLayers = 1;
  vp8_settings.denoisingOn = true;
  vp8_settings.errorConcealmentOn = false;
  vp8_settings.automaticResizeOn = false;
  vp8_settings.frameDroppingOn = true;
  vp8_settings.keyFrameInterval = 3000;

  return vp8_settings;
}

VideoCodecVP9 VideoEncoder::GetDefaultVp9Settings() {
  VideoCodecVP9 vp9_settings;
  memset(&vp9_settings, 0, sizeof(vp9_settings));

  vp9_settings.resilienceOn = true;
  vp9_settings.numberOfTemporalLayers = 1;
  vp9_settings.denoisingOn = true;
  vp9_settings.frameDroppingOn = true;
  vp9_settings.keyFrameInterval = 3000;
  vp9_settings.adaptiveQpMode = true;
  vp9_settings.automaticResizeOn = true;
  vp9_settings.numberOfSpatialLayers = 1;
  vp9_settings.flexibleMode = false;

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

VideoEncoder::ScalingSettings::ScalingSettings(bool on, int low, int high)
    : enabled(on), thresholds(QpThresholds(low, high)) {}

VideoEncoder::ScalingSettings::ScalingSettings(bool on,
                                               int low,
                                               int high,
                                               int min_pixels)
    : enabled(on),
      thresholds(QpThresholds(low, high)),
      min_pixels_per_frame(min_pixels) {}

VideoEncoder::ScalingSettings::ScalingSettings(bool on, int min_pixels)
    : enabled(on), min_pixels_per_frame(min_pixels) {}

VideoEncoder::ScalingSettings::ScalingSettings(bool on) : enabled(on) {}

VideoEncoder::ScalingSettings::~ScalingSettings() {}


int32_t VideoEncoder::SetRates(uint32_t bitrate, uint32_t framerate) {
  RTC_NOTREACHED() << "SetRate(uint32_t, uint32_t) is deprecated.";
  return -1;
}

int32_t VideoEncoder::SetRateAllocation(
    const BitrateAllocation& allocation,
    uint32_t framerate) {
  return SetRates(allocation.get_sum_kbps(), framerate);
}

VideoEncoder::ScalingSettings VideoEncoder::GetScalingSettings() const {
  return ScalingSettings(false);
}

int32_t VideoEncoder::SetPeriodicKeyFrames(bool enable) {
  return -1;
}

bool VideoEncoder::SupportsNativeHandle() const {
  return false;
}

const char* VideoEncoder::ImplementationName() const {
  return "unknown";
}
}  // namespace webrtc
