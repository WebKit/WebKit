/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_tools/video_encoder/encoded_image_file_writer.h"

#include "modules/video_coding/svc/scalability_mode_util.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {

EncodedImageFileWriter::EncodedImageFileWriter(
    const VideoCodec& video_codec_setting)
    : video_codec_setting_(video_codec_setting) {
  const char* codec_string =
      CodecTypeToPayloadString(video_codec_setting.codecType);

  // Retrieve scalability mode information.
  std::optional<ScalabilityMode> scalability_mode =
      video_codec_setting.GetScalabilityMode();
  RTC_CHECK(scalability_mode);
  spatial_layers_ = ScalabilityModeToNumSpatialLayers(*scalability_mode);
  temporal_layers_ = ScalabilityModeToNumTemporalLayers(*scalability_mode);
  inter_layer_pred_mode_ =
      ScalabilityModeToInterLayerPredMode(*scalability_mode);

  RTC_CHECK_GT(spatial_layers_, 0);
  RTC_CHECK_GT(temporal_layers_, 0);
  // Create writer for every decode target.
  for (int i = 0; i < spatial_layers_; ++i) {
    for (int j = 0; j < temporal_layers_; ++j) {
      char buffer[256];
      rtc::SimpleStringBuilder name(buffer);
      name << "output-" << codec_string << "-"
           << ScalabilityModeToString(*scalability_mode) << "-L" << i << "T"
           << j << ".ivf";

      decode_target_writers_.emplace_back(std::make_pair(
          IvfFileWriter::Wrap(FileWrapper::OpenWriteOnly(name.str()), 0),
          name.str()));
    }
  }
}

EncodedImageFileWriter::~EncodedImageFileWriter() {
  for (size_t i = 0; i < decode_target_writers_.size(); ++i) {
    decode_target_writers_[i].first->Close();
    RTC_LOG(LS_INFO) << "Written: " << decode_target_writers_[i].second;
  }
}

int EncodedImageFileWriter::Write(const EncodedImage& encoded_image) {
  // L1T1 does not set `SpatialIndex` and `TemporalIndex` in `EncodedImage`.
  const int spatial_index = encoded_image.SpatialIndex().value_or(0);
  const int temporal_index = encoded_image.TemporalIndex().value_or(0);
  RTC_CHECK_LT(spatial_index, spatial_layers_);
  RTC_CHECK_LT(temporal_index, temporal_layers_);

  if (spatial_index == 0) {
    is_base_layer_key_frame =
        (encoded_image._frameType == VideoFrameType::kVideoFrameKey);
  }

  switch (inter_layer_pred_mode_) {
    case InterLayerPredMode::kOff: {
      // Write to this spatial layer.
      for (int j = temporal_index; j < temporal_layers_; ++j) {
        const int index = spatial_index * temporal_layers_ + j;
        RTC_CHECK_LT(index, decode_target_writers_.size());

        decode_target_writers_[index].first->WriteFrame(
            encoded_image, video_codec_setting_.codecType);
      }
      break;
    }

    case InterLayerPredMode::kOn: {
      // Write to this and higher spatial layers.
      for (int i = spatial_index; i < spatial_layers_; ++i) {
        for (int j = temporal_index; j < temporal_layers_; ++j) {
          const int index = i * temporal_layers_ + j;
          RTC_CHECK_LT(index, decode_target_writers_.size());

          decode_target_writers_[index].first->WriteFrame(
              encoded_image, video_codec_setting_.codecType);
        }
      }
      break;
    }

    case InterLayerPredMode::kOnKeyPic: {
      for (int i = spatial_index; i < spatial_layers_; ++i) {
        for (int j = temporal_index; j < temporal_layers_; ++j) {
          const int index = i * temporal_layers_ + j;
          RTC_CHECK_LT(index, decode_target_writers_.size());

          decode_target_writers_[index].first->WriteFrame(
              encoded_image, video_codec_setting_.codecType);
        }

        // Write to higher spatial layers only if key frame.
        if (!is_base_layer_key_frame) {
          break;
        }
      }
      break;
    }
  }

  return 0;
}

}  // namespace test
}  // namespace webrtc
