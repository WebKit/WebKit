// Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "modules/video_coding/codecs/av1/libaom_speed_config_factory.h"

#include <algorithm>
#include <optional>

#include "api/field_trials_view.h"
#include "api/units/time_delta.h"
#include "api/video_codecs/encoder_speed_controller.h"
#include "api/video_codecs/video_codec.h"
#include "rtc_base/experiments/psnr_experiment.h"

namespace webrtc {

namespace {

using SpeedLevel = EncoderSpeedController::Config::SpeedLevel;
using PsnrGain = EncoderSpeedController::Config::SpeedLevel::PsnrComparison;

constexpr int kNumLevels = 15;
SpeedLevel kAllLevels[kNumLevels] = {
    {.speeds = {5, 5, 6, 6},
     .min_qp = 31,
     .min_psnr_gain = PsnrGain{.baseline_speed = 6, .psnr_threshold = 0.2}},
    {.speeds = {5, 6, 7, 7}, .min_qp = 30},
    {.speeds = {5, 6, 8, 10}, .min_qp = 30},
    {.speeds = {5, 6, 9, 11},
     .min_qp = 29,
     .min_psnr_gain = PsnrGain{.baseline_speed = 7, .psnr_threshold = 0.25}},
    {.speeds = {5, 7, 7, 7}, .min_qp = 29},
    {.speeds = {7, 7, 8, 8}, .min_qp = 28},
    {.speeds = {7, 7, 8, 9}, .min_qp = 28},
    {.speeds = {7, 7, 10, 10}, .min_qp = 28},
    {.speeds = {7, 7, 10, 11}, .min_qp = 27},
    {.speeds = {7, 7, 11, 11}, .min_qp = 26},
    {.speeds = {7, 8, 9, 9}, .min_qp = 26},
    {.speeds = {7, 9, 9, 11}, .min_qp = 25},
    {.speeds = {8, 9, 10, 11}, .min_qp = 25},
    {.speeds = {9, 10, 11, 11}, .min_qp = std::nullopt},
    {.speeds = {10, 11, 11, 11}, .min_qp = std::nullopt}};

bool HasSameSpeeds(const SpeedLevel& a,
                   const SpeedLevel& b,
                   int num_temporal_layers) {
  if (a.speeds[0] != b.speeds[0] || a.speeds[1] != b.speeds[1]) {
    // Keyframe or base layer speed differs.
    return false;
  }
  if (num_temporal_layers > 1 && a.speeds[3] != b.speeds[3]) {
    // Upper (non-reference) layer speed differs.
    return false;
  }
  // Middle temporal layer (intermedia class).
  return a.speeds[2] == b.speeds[2];
}

void AddSpeedLevels(int num_levels,
                    int num_temporal_layers,
                    EncoderSpeedController::Config& config) {
  // Add up to `num_levels` speeds - but ignore levels that have identical
  // speeds when `num_temporal_layers` is used (e.g. same base-layer speed for
  // single-layer).
  config.speed_levels.reserve(num_levels);
  for (int i = kNumLevels - 1; i >= kNumLevels - num_levels; --i) {
    if (i == kNumLevels - 1 ||
        !HasSameSpeeds(kAllLevels[i], config.speed_levels.back(),
                       num_temporal_layers)) {
      config.speed_levels.push_back(kAllLevels[i]);
    }
  }

  std::reverse(config.speed_levels.begin(), config.speed_levels.end());
}

}  // namespace

LibaomSpeedConfigFactory::LibaomSpeedConfigFactory(
    VideoCodecComplexity complexity,
    VideoCodecMode mode)
    : complexity_(complexity), mode_(mode) {}

EncoderSpeedController::Config LibaomSpeedConfigFactory::GetSpeedConfig(
    int width,
    int height,
    int num_temporal_layers,
    const FieldTrialsView& field_trials) {
  EncoderSpeedController::Config config;
  int num_levels = 0;
  switch (complexity_) {
    case VideoCodecComplexity::kComplexityLow:
      // Level 9x10x11x11 and up.
      num_levels = 2;
      break;
    case VideoCodecComplexity::kComplexityNormal:
      // Level 8x9x10x11 and up.
      num_levels = 3;
      break;
    case VideoCodecComplexity::kComplexityHigh:
      // Level 7x7x10x10 and up.
      num_levels = 8;
      break;
    case VideoCodecComplexity::kComplexityHigher:
      // Level 5x6x8x10 and up (< 720p, 5x7x7x7 otherwise)
      if (width * height < 1280 * 720) {  // Corrected condition
        num_levels = 12;
      } else {
        num_levels = 10;
      }
      break;
    case VideoCodecComplexity::kComplexityMax:
      // All levels.
      num_levels = kNumLevels;
      break;
  }

  if (mode_ == VideoCodecMode::kScreensharing) {
    num_levels = std::max(1, num_levels - 1);
  }

  AddSpeedLevels(num_levels, num_temporal_layers, config);

  // Don't cap speed based on resolution - only adjust the start value.
  const int num_pixels = width * height;
  const int available_speed_levels = config.speed_levels.size();
  if (num_pixels > 1920 * 1080) {
    config.start_speed_index = std::max(available_speed_levels - 4, 0);
  } else if (num_pixels > 1280 * 720) {
    config.start_speed_index = std::max(available_speed_levels - 3, 0);
  } else if (num_pixels > 640 * 360) {
    config.start_speed_index = std::max(available_speed_levels - 2, 0);
  } else {
    config.start_speed_index = std::max(available_speed_levels - 1, 0);
  }

  PsnrExperiment psnr_experiment(field_trials);
  if (psnr_experiment.IsEnabled()) {
    config.psnr_probing_settings = {
        .mode = EncoderSpeedController::Config::PsnrProbingSettings::Mode::
            kRegularBaseLayerSampling,
        .sampling_interval = psnr_experiment.SamplingInterval(),
        .average_base_layer_ratio = 1.0 / num_temporal_layers};
  } else {
    config.psnr_probing_settings = {
        .mode = EncoderSpeedController::Config::PsnrProbingSettings::Mode::
            kOnlyWhenProbing,
        .sampling_interval = TimeDelta::Seconds(1),
        .average_base_layer_ratio = 1.0 / num_temporal_layers};
  }

  return config;
}

}  // namespace webrtc
