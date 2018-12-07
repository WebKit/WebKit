/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_codec.h"

#include <string.h>
#include <string>

#include "absl/strings/match.h"
#include "rtc_base/checks.h"
#include "rtc_base/stringutils.h"

namespace webrtc {

bool VideoCodecVP8::operator==(const VideoCodecVP8& other) const {
  return (complexity == other.complexity &&
          numberOfTemporalLayers == other.numberOfTemporalLayers &&
          denoisingOn == other.denoisingOn &&
          automaticResizeOn == other.automaticResizeOn &&
          frameDroppingOn == other.frameDroppingOn &&
          keyFrameInterval == other.keyFrameInterval);
}

bool VideoCodecVP9::operator==(const VideoCodecVP9& other) const {
  return (complexity == other.complexity &&
          numberOfTemporalLayers == other.numberOfTemporalLayers &&
          denoisingOn == other.denoisingOn &&
          frameDroppingOn == other.frameDroppingOn &&
          keyFrameInterval == other.keyFrameInterval &&
          adaptiveQpMode == other.adaptiveQpMode &&
          automaticResizeOn == other.automaticResizeOn &&
          numberOfSpatialLayers == other.numberOfSpatialLayers &&
          flexibleMode == other.flexibleMode);
}

bool VideoCodecH264::operator==(const VideoCodecH264& other) const {
  return (frameDroppingOn == other.frameDroppingOn &&
          keyFrameInterval == other.keyFrameInterval &&
          spsLen == other.spsLen && ppsLen == other.ppsLen &&
          profile == other.profile &&
          (spsLen == 0 || memcmp(spsData, other.spsData, spsLen) == 0) &&
          (ppsLen == 0 || memcmp(ppsData, other.ppsData, ppsLen) == 0));
}

bool SpatialLayer::operator==(const SpatialLayer& other) const {
  return (width == other.width && height == other.height &&
          numberOfTemporalLayers == other.numberOfTemporalLayers &&
          maxBitrate == other.maxBitrate &&
          targetBitrate == other.targetBitrate &&
          minBitrate == other.minBitrate && qpMax == other.qpMax &&
          active == other.active);
}

VideoCodec::VideoCodec()
    : codecType(kVideoCodecGeneric),
      plType(0),
      width(0),
      height(0),
      startBitrate(0),
      maxBitrate(0),
      minBitrate(0),
      targetBitrate(0),
      maxFramerate(0),
      active(true),
      qpMax(0),
      numberOfSimulcastStreams(0),
      simulcastStream(),
      spatialLayers(),
      mode(VideoCodecMode::kRealtimeVideo),
      expect_encode_from_texture(false),
      timing_frame_thresholds({0, 0}),
      codec_specific_() {}

VideoCodecVP8* VideoCodec::VP8() {
  RTC_DCHECK_EQ(codecType, kVideoCodecVP8);
  return &codec_specific_.VP8;
}

const VideoCodecVP8& VideoCodec::VP8() const {
  RTC_DCHECK_EQ(codecType, kVideoCodecVP8);
  return codec_specific_.VP8;
}

VideoCodecVP9* VideoCodec::VP9() {
  RTC_DCHECK_EQ(codecType, kVideoCodecVP9);
  return &codec_specific_.VP9;
}

const VideoCodecVP9& VideoCodec::VP9() const {
  RTC_DCHECK_EQ(codecType, kVideoCodecVP9);
  return codec_specific_.VP9;
}

VideoCodecH264* VideoCodec::H264() {
  RTC_DCHECK_EQ(codecType, kVideoCodecH264);
  return &codec_specific_.H264;
}

const VideoCodecH264& VideoCodec::H264() const {
  RTC_DCHECK_EQ(codecType, kVideoCodecH264);
  return codec_specific_.H264;
}

static const char* kPayloadNameVp8 = "VP8";
static const char* kPayloadNameVp9 = "VP9";
static const char* kPayloadNameH264 = "H264";
static const char* kPayloadNameI420 = "I420";
static const char* kPayloadNameGeneric = "Generic";
static const char* kPayloadNameMultiplex = "Multiplex";

const char* CodecTypeToPayloadString(VideoCodecType type) {
  switch (type) {
    case kVideoCodecVP8:
      return kPayloadNameVp8;
    case kVideoCodecVP9:
      return kPayloadNameVp9;
    case kVideoCodecH264:
      return kPayloadNameH264;
    case kVideoCodecI420:
      return kPayloadNameI420;
    // Other codecs default to generic.
    default:
      return kPayloadNameGeneric;
  }
}

VideoCodecType PayloadStringToCodecType(const std::string& name) {
  if (absl::EqualsIgnoreCase(name, kPayloadNameVp8))
    return kVideoCodecVP8;
  if (absl::EqualsIgnoreCase(name, kPayloadNameVp9))
    return kVideoCodecVP9;
  if (absl::EqualsIgnoreCase(name, kPayloadNameH264))
    return kVideoCodecH264;
  if (absl::EqualsIgnoreCase(name, kPayloadNameI420))
    return kVideoCodecI420;
  if (absl::EqualsIgnoreCase(name, kPayloadNameMultiplex))
    return kVideoCodecMultiplex;
  return kVideoCodecGeneric;
}

}  // namespace webrtc
