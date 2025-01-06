/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/svc/simulcast_to_svc_converter.h"

#include "modules/video_coding/svc/create_scalability_structure.h"
#include "modules/video_coding/svc/scalability_mode_util.h"
#include "modules/video_coding/utility/simulcast_utility.h"
#include "rtc_base/checks.h"

namespace webrtc {

SimulcastToSvcConverter::SimulcastToSvcConverter(const VideoCodec& codec) {
  config_ = codec;
  int num_temporal_layers =
      config_.simulcastStream[0].GetNumberOfTemporalLayers();
  int num_spatial_layers = config_.numberOfSimulcastStreams;
  ScalabilityMode scalability_mode;
  switch (num_temporal_layers) {
    case 1:
      scalability_mode = ScalabilityMode::kL1T1;
      break;
    case 2:
      scalability_mode = ScalabilityMode::kL1T2;
      break;
    case 3:
      scalability_mode = ScalabilityMode::kL1T3;
      break;
    default:
      RTC_DCHECK_NOTREACHED();
  }

  for (int i = 0; i < num_spatial_layers; ++i) {
    config_.spatialLayers[i] = config_.simulcastStream[i];
  }
  config_.simulcastStream[0] =
      config_.simulcastStream[config_.numberOfSimulcastStreams - 1];
  config_.VP9()->numberOfSpatialLayers = config_.numberOfSimulcastStreams;
  config_.VP9()->numberOfTemporalLayers =
      config_.spatialLayers[0].numberOfTemporalLayers;
  config_.VP9()->interLayerPred = InterLayerPredMode::kOff;
  config_.numberOfSimulcastStreams = 1;
  config_.UnsetScalabilityMode();

  for (int i = 0; i < num_spatial_layers; ++i) {
    layers_.emplace_back(scalability_mode, num_temporal_layers);
  }
}

VideoCodec SimulcastToSvcConverter::GetConfig() const {
  return config_;
}

void SimulcastToSvcConverter::EncodeStarted(bool force_keyframe) {
  // Check if at least one layer was encoded successfully.
  bool some_layers_has_completed = false;
  for (size_t i = 0; i < layers_.size(); ++i) {
    some_layers_has_completed |= !layers_[i].awaiting_frame;
  }
  for (size_t i = 0; i < layers_.size(); ++i) {
    if (layers_[i].awaiting_frame && some_layers_has_completed) {
      // Simulcast SVC controller updates pattern on all layers, even
      // if some layers has dropped the frame.
      // Simulate that behavior for all controllers, not updated
      // while rewriting frame descriptors.
      layers_[i].video_controller->OnEncodeDone(layers_[i].layer_config);
    }
    layers_[i].awaiting_frame = true;
    auto configs = layers_[i].video_controller->NextFrameConfig(force_keyframe);
    RTC_CHECK_EQ(configs.size(), 1u);
    layers_[i].layer_config = configs[0];
  }
}

bool SimulcastToSvcConverter::ConvertFrame(EncodedImage& encoded_image,
                                           CodecSpecificInfo& codec_specific) {
  int sid = encoded_image.SpatialIndex().value_or(0);
  encoded_image.SetSimulcastIndex(sid);
  encoded_image.SetSpatialIndex(std::nullopt);
  codec_specific.end_of_picture = true;
  if (codec_specific.scalability_mode) {
    int num_temporal_layers =
        ScalabilityModeToNumTemporalLayers(*codec_specific.scalability_mode);
    RTC_DCHECK_LE(num_temporal_layers, 3);
    if (num_temporal_layers == 1) {
      codec_specific.scalability_mode = ScalabilityMode::kL1T1;
    } else if (num_temporal_layers == 2) {
      codec_specific.scalability_mode = ScalabilityMode::kL1T2;
    } else if (num_temporal_layers == 3) {
      codec_specific.scalability_mode = ScalabilityMode::kL1T3;
    }
  }
  CodecSpecificInfoVP9& vp9_info = codec_specific.codecSpecific.VP9;
  vp9_info.num_spatial_layers = 1;
  vp9_info.first_active_layer = 0;
  vp9_info.first_frame_in_picture = true;
  if (vp9_info.ss_data_available) {
    vp9_info.width[0] = vp9_info.width[sid];
    vp9_info.height[0] = vp9_info.height[sid];
  }

  auto& video_controller = *layers_[sid].video_controller;
  if (codec_specific.generic_frame_info) {
    layers_[sid].awaiting_frame = false;
    uint8_t tid = encoded_image.TemporalIndex().value_or(0);
    auto& frame_config = layers_[sid].layer_config;
    RTC_DCHECK_EQ(frame_config.TemporalId(), tid == kNoTemporalIdx ? 0 : tid);
    if (frame_config.TemporalId() != (tid == kNoTemporalIdx ? 0 : tid)) {
      return false;
    }
    codec_specific.generic_frame_info =
        video_controller.OnEncodeDone(frame_config);
  }
  if (codec_specific.template_structure) {
    auto resolution = codec_specific.template_structure->resolutions[sid];
    codec_specific.template_structure = video_controller.DependencyStructure();
    codec_specific.template_structure->resolutions.resize(1);
    codec_specific.template_structure->resolutions[0] = resolution;
  }
  return true;
}

SimulcastToSvcConverter::LayerState::LayerState(
    ScalabilityMode scalability_mode,
    int num_temporal_layers)
    : video_controller(CreateScalabilityStructure(scalability_mode)),
      awaiting_frame(false) {
  VideoBitrateAllocation dummy_bitrates;
  for (int i = 0; i < num_temporal_layers; ++i) {
    dummy_bitrates.SetBitrate(0, i, 10000);
  }
  video_controller->OnRatesUpdated(dummy_bitrates);
}

// static
bool SimulcastToSvcConverter::IsConfigSupported(const VideoCodec& codec) {
  if (codec.numberOfSimulcastStreams <= 1 ||
      !SimulcastUtility::ValidSimulcastParameters(
          codec, codec.numberOfSimulcastStreams)) {
    return false;
  }
  // Ensure there's 4:2:1 scaling.
  for (int i = 1; i < codec.numberOfSimulcastStreams; ++i) {
    if (codec.simulcastStream[i].active &&
        codec.simulcastStream[i - 1].active &&
        (codec.simulcastStream[i].width !=
             codec.simulcastStream[i - 1].width * 2 ||
         codec.simulcastStream[i].height !=
             codec.simulcastStream[i - 1].height * 2)) {
      return false;
    }
  }
  int first_active_layer = -1;
  int last_active_layer = -1;
  int num_active_layers = 0;
  for (int i = 0; i < codec.numberOfSimulcastStreams; ++i) {
    if (codec.simulcastStream[i].active) {
      if (first_active_layer < 0)
        first_active_layer = i;
      last_active_layer = i;
      ++num_active_layers;
    }
  }
  // Active layers must form a continuous segment. Can't have holes, because
  // most SVC encoders can't process that.
  return num_active_layers == last_active_layer - first_active_layer + 1;
}

}  // namespace webrtc
