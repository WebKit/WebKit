/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/video_codecs/video_encoder_config.h"

#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
VideoStream::VideoStream()
    : width(0),
      height(0),
      max_framerate(-1),
      min_bitrate_bps(-1),
      target_bitrate_bps(-1),
      max_bitrate_bps(-1),
      scale_resolution_down_by(-1.),
      max_qp(-1),
      num_temporal_layers(absl::nullopt),
      active(true) {}
VideoStream::VideoStream(const VideoStream& other) = default;

VideoStream::~VideoStream() = default;

std::string VideoStream::ToString() const {
  char buf[1024];
  rtc::SimpleStringBuilder ss(buf);
  ss << "{width: " << width;
  ss << ", height: " << height;
  ss << ", max_framerate: " << max_framerate;
  ss << ", min_bitrate_bps:" << min_bitrate_bps;
  ss << ", target_bitrate_bps:" << target_bitrate_bps;
  ss << ", max_bitrate_bps:" << max_bitrate_bps;
  ss << ", max_qp: " << max_qp;
  ss << ", num_temporal_layers: " << num_temporal_layers.value_or(1);
  ss << ", bitrate_priority: " << bitrate_priority.value_or(0);
  ss << ", active: " << active;
  ss << ", scale_down_by: " << scale_resolution_down_by;

  return ss.str();
}

VideoEncoderConfig::VideoEncoderConfig()
    : codec_type(kVideoCodecGeneric),
      video_format("Unset"),
      content_type(ContentType::kRealtimeVideo),
      encoder_specific_settings(nullptr),
      min_transmit_bitrate_bps(0),
      max_bitrate_bps(0),
      bitrate_priority(1.0),
      number_of_streams(0),
      legacy_conference_mode(false),
      is_quality_scaling_allowed(false) {}

VideoEncoderConfig::VideoEncoderConfig(VideoEncoderConfig&&) = default;

VideoEncoderConfig::~VideoEncoderConfig() = default;

std::string VideoEncoderConfig::ToString() const {
  char buf[1024];
  rtc::SimpleStringBuilder ss(buf);
  ss << "{codec_type: ";
  ss << CodecTypeToPayloadString(codec_type);
  ss << ", content_type: ";
  switch (content_type) {
    case ContentType::kRealtimeVideo:
      ss << "kRealtimeVideo";
      break;
    case ContentType::kScreen:
      ss << "kScreenshare";
      break;
  }
  ss << ", encoder_specific_settings: ";
  ss << (encoder_specific_settings != NULL ? "(ptr)" : "NULL");

  ss << ", min_transmit_bitrate_bps: " << min_transmit_bitrate_bps;
  ss << '}';
  return ss.str();
}

VideoEncoderConfig::VideoEncoderConfig(const VideoEncoderConfig&) = default;

void VideoEncoderConfig::EncoderSpecificSettings::FillEncoderSpecificSettings(
    VideoCodec* codec) const {
  if (codec->codecType == kVideoCodecH264) {
    FillVideoCodecH264(codec->H264());
  } else if (codec->codecType == kVideoCodecVP8) {
    FillVideoCodecVp8(codec->VP8());
  } else if (codec->codecType == kVideoCodecVP9) {
    FillVideoCodecVp9(codec->VP9());
  } else {
    RTC_NOTREACHED() << "Encoder specifics set/used for unknown codec type.";
  }
}

void VideoEncoderConfig::EncoderSpecificSettings::FillVideoCodecH264(
    VideoCodecH264* h264_settings) const {
  RTC_NOTREACHED();
}

void VideoEncoderConfig::EncoderSpecificSettings::FillVideoCodecVp8(
    VideoCodecVP8* vp8_settings) const {
  RTC_NOTREACHED();
}

void VideoEncoderConfig::EncoderSpecificSettings::FillVideoCodecVp9(
    VideoCodecVP9* vp9_settings) const {
  RTC_NOTREACHED();
}

VideoEncoderConfig::H264EncoderSpecificSettings::H264EncoderSpecificSettings(
    const VideoCodecH264& specifics)
    : specifics_(specifics) {}

void VideoEncoderConfig::H264EncoderSpecificSettings::FillVideoCodecH264(
    VideoCodecH264* h264_settings) const {
  *h264_settings = specifics_;
}

VideoEncoderConfig::Vp8EncoderSpecificSettings::Vp8EncoderSpecificSettings(
    const VideoCodecVP8& specifics)
    : specifics_(specifics) {}

void VideoEncoderConfig::Vp8EncoderSpecificSettings::FillVideoCodecVp8(
    VideoCodecVP8* vp8_settings) const {
  *vp8_settings = specifics_;
}

VideoEncoderConfig::Vp9EncoderSpecificSettings::Vp9EncoderSpecificSettings(
    const VideoCodecVP9& specifics)
    : specifics_(specifics) {}

void VideoEncoderConfig::Vp9EncoderSpecificSettings::FillVideoCodecVp9(
    VideoCodecVP9* vp9_settings) const {
  *vp9_settings = specifics_;
}

}  // namespace webrtc
