/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/simple_encoder_wrapper.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "api/array_view.h"
#include "api/scoped_refptr.h"
#include "api/units/data_size.h"
#include "api/units/frequency.h"
#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/scalability_mode_helper.h"
#include "api/video_codecs/video_encoder_factory_interface.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "modules/video_coding/svc/create_scalability_structure.h"
#include "modules/video_coding/svc/scalable_video_controller.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/rational.h"

namespace webrtc {
using PredictionConstraints =
    VideoEncoderFactoryInterface::Capabilities::PredictionConstraints;
using FrameEncodeSettings = VideoEncoderInterface::FrameEncodeSettings;

namespace {
enum class Inter { kS, kL, kKey };
enum class Scaling { k1_2, k2_3 };
std::string SvcToString(int spatial_layers,
                        int temporal_layers,
                        Inter inter,
                        Scaling scaling) {
  RTC_CHECK(spatial_layers > 1 || inter == Inter::kL);
  std::string res;
  res += inter == Inter::kS ? "S" : "L";
  res += std::to_string(spatial_layers);
  res += "T";
  res += std::to_string(temporal_layers);
  if (scaling == Scaling::k2_3) {
    res += "h";
  }
  if (inter == Inter::kKey) {
    res += "_KEY";
  }

  return res;
}
}  // namespace

// static
std::vector<std::string> SimpleEncoderWrapper::SupportedWebrtcSvcModes(
    const PredictionConstraints& prediction_constraints) {
  std::vector<std::string> res;

  const int max_spatial_layers =
      std::min(3, prediction_constraints.max_spatial_layers);
  const int max_temporal_layers =
      std::min(3, prediction_constraints.max_temporal_layers);
  const bool scale_by_half = absl::c_linear_search(
      prediction_constraints.scaling_factors, Rational{1, 2});
  const bool scale_by_two_thirds = absl::c_linear_search(
      prediction_constraints.scaling_factors, Rational{2, 3});
  const bool inter_layer =
      prediction_constraints.max_references > 1 &&
      prediction_constraints.buffer_space_type !=
          PredictionConstraints::BufferSpaceType::kMultiInstance;

  for (int s = 1; s <= max_spatial_layers; ++s) {
    for (int t = 1; t <= max_temporal_layers; ++t) {
      if (prediction_constraints.num_buffers > ((std::max(1, t - 1) * s) - 1)) {
        if (s == 1 || inter_layer) {
          res.push_back(SvcToString(s, t, Inter::kL, Scaling::k1_2));
          if (s == 1) {
            continue;
          }
        }
        if (scale_by_half) {
          res.push_back(SvcToString(s, t, Inter::kS, Scaling::k1_2));
          if (inter_layer) {
            res.push_back(SvcToString(s, t, Inter::kKey, Scaling::k1_2));
          }
        }
        if (scale_by_two_thirds) {
          res.push_back(SvcToString(s, t, Inter::kS, Scaling::k2_3));
          if (inter_layer) {
            res.push_back(SvcToString(s, t, Inter::kKey, Scaling::k2_3));
            res.push_back(SvcToString(s, t, Inter::kL, Scaling::k2_3));
          }
        }
      }
    }
  }

  return res;
}

// static
std::unique_ptr<SimpleEncoderWrapper> SimpleEncoderWrapper::Create(
    std::unique_ptr<VideoEncoderInterface> encoder,
    absl::string_view scalability_mode) {
  if (!encoder) {
    return nullptr;
  }

  std::optional<ScalabilityMode> sm =
      ScalabilityModeStringToEnum(scalability_mode);
  if (!sm) {
    return nullptr;
  }

  std::unique_ptr<ScalableVideoController> svc_controller =
      CreateScalabilityStructure(*sm);
  if (!svc_controller) {
    return nullptr;
  }

  return std::make_unique<SimpleEncoderWrapper>(std::move(encoder),
                                                std::move(svc_controller));
}

SimpleEncoderWrapper::SimpleEncoderWrapper(
    std::unique_ptr<VideoEncoderInterface> encoder,
    std::unique_ptr<ScalableVideoController> svc_controller)
    : encoder_(std::move(encoder)),
      svc_controller_(std::move(svc_controller)),
      layer_configs_(svc_controller_->StreamConfig()) {}

void SimpleEncoderWrapper::SetEncodeQp(int qp) {
  target_qp_ = qp;
}

void SimpleEncoderWrapper::SetEncodeFps(int fps) {
  fps_ = fps;
}

void SimpleEncoderWrapper::Encode(
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer,
    bool force_keyframe,
    EncodeResultCallback callback) {
  std::vector<ScalableVideoController::LayerFrameConfig> configs =
      svc_controller_->NextFrameConfig(force_keyframe);
  std::vector<FrameEncodeSettings> encode_settings;
  std::vector<GenericFrameInfo> frame_infos;

  for (size_t s = 0; s < configs.size(); ++s) {
    const ScalableVideoController::LayerFrameConfig& config = configs[s];
    frame_infos.push_back(svc_controller_->OnEncodeDone(config));
    FrameEncodeSettings& settings = encode_settings.emplace_back();
    settings.rate_options = VideoEncoderInterface::FrameEncodeSettings::Cqp{
        .target_qp = target_qp_};
    settings.spatial_id = config.SpatialId();
    settings.temporal_id = config.TemporalId();
    const int num = layer_configs_.scaling_factor_num[s];
    const int den = layer_configs_.scaling_factor_den[s];
    settings.resolution = {(frame_buffer->width() * num / den),
                           (frame_buffer->height() * num / den)};

    bool buffer_updated = false;
    for (const CodecBufferUsage& buffer : config.Buffers()) {
      if (buffer.referenced) {
        settings.reference_buffers.push_back(buffer.id);
      }
      if (buffer.updated) {
        RTC_CHECK(!buffer_updated);
        settings.update_buffer = buffer.id;
        buffer_updated = true;
      }
    }

    if (settings.reference_buffers.empty()) {
      settings.frame_type = FrameType::kKeyframe;
    }

    struct FrameOut : public VideoEncoderInterface::FrameOutput {
      rtc::ArrayView<uint8_t> GetBitstreamOutputBuffer(DataSize size) override {
        bitstream.resize(size.bytes());
        return bitstream;
      }

      void EncodeComplete(
          const VideoEncoderInterface::EncodeResult& result) override {
        auto* data = absl::get_if<VideoEncoderInterface::EncodedData>(&result);

        SimpleEncoderWrapper::EncodeResult res;
        if (!data) {
          res.oh_no = true;
          callback(res);
          return;
        }

        res.frame_type = data->frame_type;
        res.bitstream_data = std::move(bitstream);
        res.generic_frame_info = frame_info;
        if (res.frame_type == FrameType::kKeyframe) {
          res.dependency_structure = svc_controller->DependencyStructure();
        }
        callback(res);
      }
      std::vector<uint8_t> bitstream;
      EncodeResultCallback callback;
      GenericFrameInfo frame_info;
      ScalableVideoController* svc_controller;
    };

    auto out = std::make_unique<FrameOut>();

    out->callback = callback;
    out->frame_info = std::move(frame_infos[settings.spatial_id]);
    out->svc_controller = svc_controller_.get();

    settings.frame_output = std::move(out);
  }

  encoder_->Encode(std::move(frame_buffer),
                   {.presentation_timestamp = presentation_timestamp_},
                   std::move(encode_settings));
  presentation_timestamp_ += 1 / Frequency::Hertz(fps_);
}

}  // namespace webrtc
