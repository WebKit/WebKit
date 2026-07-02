/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/svc/create_scalability_structure.h"

#include <memory>
#include <optional>

#include "api/video_codecs/scalability_mode.h"
#include "modules/video_coding/svc/scalability_structure_full_svc.h"
#include "modules/video_coding/svc/scalability_structure_key_svc.h"
#include "modules/video_coding/svc/scalability_structure_l2t2_key_shift.h"
#include "modules/video_coding/svc/scalability_structure_simulcast.h"
#include "modules/video_coding/svc/scalable_video_controller.h"
#include "modules/video_coding/svc/scalable_video_controller_no_layering.h"

namespace webrtc {
namespace {

struct NamedStructureFactory {
  ScalabilityMode name;
  // Use function pointer to make NamedStructureFactory trivally destructable.
  std::unique_ptr<ScalableVideoController> (*factory)();
  ScalableVideoController::StreamLayersConfig config;
};

// Wrap std::make_unique function to have correct return type.
template <typename T>
std::unique_ptr<ScalableVideoController> Create() {
  return std::make_unique<T>();
}

template <typename T>
std::unique_ptr<ScalableVideoController> CreateH() {
  // 1.5:1 scaling, see https://w3c.github.io/webrtc-svc/#scalabilitymodes*
  typename T::ScalingFactor factor;
  factor.num = 2;
  factor.den = 3;
  return std::make_unique<T>(factor);
}

constexpr ScalableVideoController::StreamLayersConfig kConfigL1T1 = {
    .num_spatial_layers = 1,
    .num_temporal_layers = 1,
    .uses_reference_scaling = false};

constexpr ScalableVideoController::StreamLayersConfig kConfigL1T2 = {
    .num_spatial_layers = 1,
    .num_temporal_layers = 2,
    .uses_reference_scaling = false};

constexpr ScalableVideoController::StreamLayersConfig kConfigL1T3 = {
    .num_spatial_layers = 1,
    .num_temporal_layers = 3,
    .uses_reference_scaling = false};

constexpr ScalableVideoController::StreamLayersConfig kConfigL2T1 = {
    .num_spatial_layers = 2,
    .num_temporal_layers = 1,
    .uses_reference_scaling = true,
    .scaling_factor_num = {1, 1},
    .scaling_factor_den = {2, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigL2T1h = {
    .num_spatial_layers = 2,
    .num_temporal_layers = 1,
    .uses_reference_scaling = true,
    .scaling_factor_num = {2, 1},
    .scaling_factor_den = {3, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigL2T2 = {
    .num_spatial_layers = 2,
    .num_temporal_layers = 2,
    .uses_reference_scaling = true,
    .scaling_factor_num = {1, 1},
    .scaling_factor_den = {2, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigL2T2h = {
    .num_spatial_layers = 2,
    .num_temporal_layers = 2,
    .uses_reference_scaling = true,
    .scaling_factor_num = {2, 1},
    .scaling_factor_den = {3, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigL2T3 = {
    .num_spatial_layers = 2,
    .num_temporal_layers = 3,
    .uses_reference_scaling = true,
    .scaling_factor_num = {1, 1},
    .scaling_factor_den = {2, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigL2T3h = {
    .num_spatial_layers = 2,
    .num_temporal_layers = 3,
    .uses_reference_scaling = true,
    .scaling_factor_num = {2, 1},
    .scaling_factor_den = {3, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigL3T1 = {
    .num_spatial_layers = 3,
    .num_temporal_layers = 1,
    .uses_reference_scaling = true,
    .scaling_factor_num = {1, 1, 1},
    .scaling_factor_den = {4, 2, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigL3T1h = {
    .num_spatial_layers = 3,
    .num_temporal_layers = 1,
    .uses_reference_scaling = true,
    .scaling_factor_num = {4, 2, 1},
    .scaling_factor_den = {9, 3, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigL3T2 = {
    .num_spatial_layers = 3,
    .num_temporal_layers = 2,
    .uses_reference_scaling = true,
    .scaling_factor_num = {1, 1, 1},
    .scaling_factor_den = {4, 2, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigL3T2h = {
    .num_spatial_layers = 3,
    .num_temporal_layers = 2,
    .uses_reference_scaling = true,
    .scaling_factor_num = {4, 2, 1},
    .scaling_factor_den = {9, 3, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigL3T3 = {
    .num_spatial_layers = 3,
    .num_temporal_layers = 3,
    .uses_reference_scaling = true,
    .scaling_factor_num = {1, 1, 1},
    .scaling_factor_den = {4, 2, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigL3T3h = {
    .num_spatial_layers = 3,
    .num_temporal_layers = 3,
    .uses_reference_scaling = true,
    .scaling_factor_num = {4, 2, 1},
    .scaling_factor_den = {9, 3, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigS2T1 = {
    .num_spatial_layers = 2,
    .num_temporal_layers = 1,
    .uses_reference_scaling = false,
    .scaling_factor_num = {1, 1},
    .scaling_factor_den = {2, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigS2T1h = {
    .num_spatial_layers = 2,
    .num_temporal_layers = 1,
    .uses_reference_scaling = false,
    .scaling_factor_num = {2, 1},
    .scaling_factor_den = {3, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigS2T2 = {
    .num_spatial_layers = 2,
    .num_temporal_layers = 2,
    .uses_reference_scaling = false,
    .scaling_factor_num = {1, 1},
    .scaling_factor_den = {2, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigS2T2h = {
    .num_spatial_layers = 2,
    .num_temporal_layers = 2,
    .uses_reference_scaling = false,
    .scaling_factor_num = {2, 1},
    .scaling_factor_den = {3, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigS2T3 = {
    .num_spatial_layers = 2,
    .num_temporal_layers = 3,
    .uses_reference_scaling = false,
    .scaling_factor_num = {1, 1},
    .scaling_factor_den = {2, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigS2T3h = {
    .num_spatial_layers = 2,
    .num_temporal_layers = 3,
    .uses_reference_scaling = false,
    .scaling_factor_num = {2, 1},
    .scaling_factor_den = {3, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigS3T1 = {
    .num_spatial_layers = 3,
    .num_temporal_layers = 1,
    .uses_reference_scaling = false,
    .scaling_factor_num = {1, 1, 1},
    .scaling_factor_den = {4, 2, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigS3T1h = {
    .num_spatial_layers = 3,
    .num_temporal_layers = 1,
    .uses_reference_scaling = false,
    .scaling_factor_num = {4, 2, 1},
    .scaling_factor_den = {9, 3, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigS3T2 = {
    .num_spatial_layers = 3,
    .num_temporal_layers = 2,
    .uses_reference_scaling = false,
    .scaling_factor_num = {1, 1, 1},
    .scaling_factor_den = {4, 2, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigS3T2h = {
    .num_spatial_layers = 3,
    .num_temporal_layers = 2,
    .uses_reference_scaling = false,
    .scaling_factor_num = {4, 2, 1},
    .scaling_factor_den = {9, 3, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigS3T3 = {
    .num_spatial_layers = 3,
    .num_temporal_layers = 3,
    .uses_reference_scaling = false,
    .scaling_factor_num = {1, 1, 1},
    .scaling_factor_den = {4, 2, 1}};

constexpr ScalableVideoController::StreamLayersConfig kConfigS3T3h = {
    .num_spatial_layers = 3,
    .num_temporal_layers = 3,
    .uses_reference_scaling = false,
    .scaling_factor_num = {4, 2, 1},
    .scaling_factor_den = {9, 3, 1}};

constexpr NamedStructureFactory kFactories[] = {
    {.name = ScalabilityMode::kL1T1,
     .factory = Create<ScalableVideoControllerNoLayering>,
     .config = kConfigL1T1},
    {.name = ScalabilityMode::kL1T2,
     .factory = Create<ScalabilityStructureL1T2>,
     .config = kConfigL1T2},
    {.name = ScalabilityMode::kL1T3,
     .factory = Create<ScalabilityStructureL1T3>,
     .config = kConfigL1T3},
    {.name = ScalabilityMode::kL2T1,
     .factory = Create<ScalabilityStructureL2T1>,
     .config = kConfigL2T1},
    {.name = ScalabilityMode::kL2T1h,
     .factory = CreateH<ScalabilityStructureL2T1>,
     .config = kConfigL2T1h},
    {.name = ScalabilityMode::kL2T1_KEY,
     .factory = Create<ScalabilityStructureL2T1Key>,
     .config = kConfigL2T1},
    {.name = ScalabilityMode::kL2T2,
     .factory = Create<ScalabilityStructureL2T2>,
     .config = kConfigL2T2},
    {.name = ScalabilityMode::kL2T2h,
     .factory = CreateH<ScalabilityStructureL2T2>,
     .config = kConfigL2T2h},
    {.name = ScalabilityMode::kL2T2_KEY,
     .factory = Create<ScalabilityStructureL2T2Key>,
     .config = kConfigL2T2},
    {.name = ScalabilityMode::kL2T2_KEY_SHIFT,
     .factory = Create<ScalabilityStructureL2T2KeyShift>,
     .config = kConfigL2T2},
    {.name = ScalabilityMode::kL2T3,
     .factory = Create<ScalabilityStructureL2T3>,
     .config = kConfigL2T3},
    {.name = ScalabilityMode::kL2T3h,
     .factory = CreateH<ScalabilityStructureL2T3>,
     .config = kConfigL2T3h},
    {.name = ScalabilityMode::kL2T3_KEY,
     .factory = Create<ScalabilityStructureL2T3Key>,
     .config = kConfigL2T3},
    {.name = ScalabilityMode::kL3T1,
     .factory = Create<ScalabilityStructureL3T1>,
     .config = kConfigL3T1},
    {.name = ScalabilityMode::kL3T1h,
     .factory = CreateH<ScalabilityStructureL3T1>,
     .config = kConfigL3T1h},
    {.name = ScalabilityMode::kL3T1_KEY,
     .factory = Create<ScalabilityStructureL3T1Key>,
     .config = kConfigL3T1},
    {.name = ScalabilityMode::kL3T2,
     .factory = Create<ScalabilityStructureL3T2>,
     .config = kConfigL3T2},
    {.name = ScalabilityMode::kL3T2h,
     .factory = CreateH<ScalabilityStructureL3T2>,
     .config = kConfigL3T2h},
    {.name = ScalabilityMode::kL3T2_KEY,
     .factory = Create<ScalabilityStructureL3T2Key>,
     .config = kConfigL3T2},
    {.name = ScalabilityMode::kL3T3,
     .factory = Create<ScalabilityStructureL3T3>,
     .config = kConfigL3T3},
    {.name = ScalabilityMode::kL3T3h,
     .factory = CreateH<ScalabilityStructureL3T3>,
     .config = kConfigL3T3h},
    {.name = ScalabilityMode::kL3T3_KEY,
     .factory = Create<ScalabilityStructureL3T3Key>,
     .config = kConfigL3T3},
    {.name = ScalabilityMode::kS2T1,
     .factory = Create<ScalabilityStructureS2T1>,
     .config = kConfigS2T1},
    {.name = ScalabilityMode::kS2T1h,
     .factory = CreateH<ScalabilityStructureS2T1>,
     .config = kConfigS2T1h},
    {.name = ScalabilityMode::kS2T2,
     .factory = Create<ScalabilityStructureS2T2>,
     .config = kConfigS2T2},
    {.name = ScalabilityMode::kS2T2h,
     .factory = CreateH<ScalabilityStructureS2T2>,
     .config = kConfigS2T2h},
    {.name = ScalabilityMode::kS2T3,
     .factory = Create<ScalabilityStructureS2T3>,
     .config = kConfigS2T3},
    {.name = ScalabilityMode::kS2T3h,
     .factory = CreateH<ScalabilityStructureS2T3>,
     .config = kConfigS2T3h},
    {.name = ScalabilityMode::kS3T1,
     .factory = Create<ScalabilityStructureS3T1>,
     .config = kConfigS3T1},
    {.name = ScalabilityMode::kS3T1h,
     .factory = CreateH<ScalabilityStructureS3T1>,
     .config = kConfigS3T1h},
    {.name = ScalabilityMode::kS3T2,
     .factory = Create<ScalabilityStructureS3T2>,
     .config = kConfigS3T2},
    {.name = ScalabilityMode::kS3T2h,
     .factory = CreateH<ScalabilityStructureS3T2>,
     .config = kConfigS3T2h},
    {.name = ScalabilityMode::kS3T3,
     .factory = Create<ScalabilityStructureS3T3>,
     .config = kConfigS3T3},
    {.name = ScalabilityMode::kS3T3h,
     .factory = CreateH<ScalabilityStructureS3T3>,
     .config = kConfigS3T3h},
};

}  // namespace

std::unique_ptr<ScalableVideoController> CreateScalabilityStructure(
    ScalabilityMode name) {
  for (const auto& entry : kFactories) {
    if (entry.name == name) {
      return entry.factory();
    }
  }
  return nullptr;
}

std::optional<ScalableVideoController::StreamLayersConfig>
ScalabilityStructureConfig(ScalabilityMode name) {
  for (const auto& entry : kFactories) {
    if (entry.name == name) {
      return entry.config;
    }
  }
  return std::nullopt;
}

}  // namespace webrtc
