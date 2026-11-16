/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/test/temporal_layer_pattern_for_test.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <utility>
#include <vector>

#include "rtc_base/checks.h"

namespace webrtc {
namespace {

// The number of frames temporal layer `temporal_id` contributes to one group
// of pictures. Every layer holds every other frame of the layer above it, and
// the base layer holds as many frames as the layer just above it, giving the
// 1, 1, 2, 4, ... distribution of the dyadic pattern this class produces.
int FramesPerGroupOfPicturesInLayer(int temporal_id) {
  return temporal_id == 0 ? 1 : 1 << (temporal_id - 1);
}

// Converts per frame bit budgets into per layer bitrate fractions by weighting
// each layer with the number of frames it contributes.
std::vector<double> ToBitrateFractions(std::vector<double> frame_budgets) {
  double sum = 0.0;
  for (size_t tid = 0; tid < frame_budgets.size(); ++tid) {
    frame_budgets[tid] *=
        FramesPerGroupOfPicturesInLayer(static_cast<int>(tid));
    sum += frame_budgets[tid];
  }
  for (double& fraction : frame_budgets) {
    fraction /= sum;
  }
  return frame_budgets;
}

}  // namespace

TemporalLayerPatternForTest::TemporalLayerPatternForTest(
    int num_temporal_layers,
    int num_reference_buffers,
    std::vector<double> bitrate_fractions)
    : num_temporal_layers_(num_temporal_layers),
      // The topmost layer is never referenced and therefore does not need a
      // buffer, but at least the base layer always has to be stored.
      num_stored_layers_(std::min(num_reference_buffers,
                                  std::max(1, num_temporal_layers - 1))) {
  RTC_CHECK_GE(num_temporal_layers, 1);
  RTC_CHECK_GE(num_reference_buffers, 1);
  RTC_CHECK_EQ(bitrate_fractions.size(),
               static_cast<size_t>(num_temporal_layers));

  double sum = 0.0;
  for (double fraction : bitrate_fractions) {
    RTC_CHECK_GT(fraction, 0.0);
    sum += fraction;
  }

  // A layer holding the fraction `f` of the bitrate and `n` of the
  // `frames_per_gop` frames gives each of its frames `f / n` of the bits in a
  // group of pictures, which is `f * frames_per_gop / n` times the nominal per
  // frame budget.
  const int frames_per_gop = FramesPerGroupOfPictures(num_temporal_layers);
  rate_factors_.reserve(num_temporal_layers);
  frame_budget_factors_.reserve(num_temporal_layers);
  for (int tid = 0; tid < num_temporal_layers; ++tid) {
    rate_factors_.push_back(bitrate_fractions[tid] / sum);
    frame_budget_factors_.push_back(rate_factors_[tid] * frames_per_gop /
                                    FramesPerGroupOfPicturesInLayer(tid));
  }
}

int TemporalLayerPatternForTest::FramesPerGroupOfPictures(
    int num_temporal_layers) {
  RTC_CHECK_GE(num_temporal_layers, 1);
  return 1 << (num_temporal_layers - 1);
}

TemporalLayerPatternForTest::FrameConfig
TemporalLayerPatternForTest::NextFrameConfig() {
  const int frame_index = frame_index_++;
  const int temporal_id = TemporalIdOf(frame_index);
  FrameConfig config = {.temporal_id = temporal_id,
                        .rate_factor = rate_factors_[temporal_id]};

  if (frame_index == 0) {
    // The first frame of the pattern has nothing to reference.
    config.update_buffer = 0;
  } else if (temporal_id < num_stored_layers_) {
    // Reference the closest preceding frame of a lower temporal layer. It is
    // held in the buffer named after its temporal id and, being of a lower
    // layer than this frame, it is always stored.
    config.reference_buffer = TemporalIdOf(
        frame_index - (1 << (num_temporal_layers_ - 1 - temporal_id)));
    config.update_buffer = temporal_id;
  } else {
    // Frames of this layer are not stored, either because it is the topmost
    // layer or because there are not enough buffers available. In the latter
    // case the closest preceding frame of a lower layer may not be stored
    // either, so reference the most recently stored frame instead.
    config.reference_buffer = last_updated_buffer_;
  }

  if (config.update_buffer.has_value()) {
    last_updated_buffer_ = *config.update_buffer;
  }
  return config;
}

std::vector<double> TemporalLayerPatternForTest::GeometricDistribution(
    int num_temporal_layers,
    double ratio) {
  RTC_CHECK_GT(ratio, 0.0);
  std::vector<double> frame_budgets(num_temporal_layers);
  double budget = 1.0;
  for (int tid = 0; tid < num_temporal_layers; ++tid) {
    frame_budgets[tid] = budget;
    budget *= ratio;
  }
  return ToBitrateFractions(std::move(frame_budgets));
}

std::vector<double> TemporalLayerPatternForTest::LinearDistribution(
    int num_temporal_layers) {
  std::vector<double> frame_budgets(num_temporal_layers);
  for (int tid = 0; tid < num_temporal_layers; ++tid) {
    frame_budgets[tid] = num_temporal_layers - tid;
  }
  return ToBitrateFractions(std::move(frame_budgets));
}

int TemporalLayerPatternForTest::TemporalIdOf(int frame_index) const {
  // Frames of temporal layer `t` sit at the positions in the group of pictures
  // that have exactly `num_temporal_layers - 1 - t` trailing zero bits, with
  // the first position of the group belonging to the base layer.
  const int index_in_gop = frame_index % (1 << (num_temporal_layers_ - 1));
  if (index_in_gop == 0) {
    return 0;
  }
  return num_temporal_layers_ - 1 -
         std::countr_zero(static_cast<unsigned>(index_in_gop));
}

}  // namespace webrtc
