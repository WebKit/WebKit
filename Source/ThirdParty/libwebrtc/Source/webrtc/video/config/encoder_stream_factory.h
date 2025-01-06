/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef VIDEO_CONFIG_ENCODER_STREAM_FACTORY_H_
#define VIDEO_CONFIG_ENCODER_STREAM_FACTORY_H_

#include <string>
#include <vector>

#include "api/field_trials_view.h"
#include "api/units/data_rate.h"
#include "api/video_codecs/video_encoder.h"
#include "call/adaptation/video_source_restrictions.h"
#include "video/config/video_encoder_config.h"

namespace cricket {

class EncoderStreamFactory
    : public webrtc::VideoEncoderConfig::VideoStreamFactoryInterface {
 public:
  EncoderStreamFactory(const webrtc::VideoEncoder::EncoderInfo& encoder_info,
                       std::optional<webrtc::VideoSourceRestrictions>
                           restrictions = std::nullopt);

  std::vector<webrtc::VideoStream> CreateEncoderStreams(
      const webrtc::FieldTrialsView& trials,
      int width,
      int height,
      const webrtc::VideoEncoderConfig& encoder_config) override;

 private:
  std::vector<webrtc::VideoStream> CreateDefaultVideoStreams(
      int width,
      int height,
      const webrtc::VideoEncoderConfig& encoder_config,
      const std::optional<webrtc::DataRate>& experimental_min_bitrate) const;

  std::vector<webrtc::VideoStream>
  CreateSimulcastOrConferenceModeScreenshareStreams(
      const webrtc::FieldTrialsView& trials,
      int width,
      int height,
      const webrtc::VideoEncoderConfig& encoder_config,
      const std::optional<webrtc::DataRate>& experimental_min_bitrate) const;

  webrtc::Resolution GetLayerResolutionFromScaleResolutionDownTo(
      int in_frame_width,
      int in_frame_height,
      webrtc::Resolution scale_resolution_down_to) const;

  std::vector<webrtc::Resolution> GetStreamResolutions(
      const webrtc::FieldTrialsView& trials,
      int width,
      int height,
      const webrtc::VideoEncoderConfig& encoder_config) const;

  const int encoder_info_requested_resolution_alignment_;
  const std::optional<webrtc::VideoSourceRestrictions> restrictions_;
};

}  // namespace cricket

#endif  // VIDEO_CONFIG_ENCODER_STREAM_FACTORY_H_
