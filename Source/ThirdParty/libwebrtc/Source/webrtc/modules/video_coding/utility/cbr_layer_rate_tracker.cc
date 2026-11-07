/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/cbr_layer_rate_tracker.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>
#include <variant>

#include "api/units/data_rate.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/exp_filter.h"

namespace webrtc {
namespace {

using FrameEncodeSettings = VideoEncoderInterface::FrameEncodeSettings;

// How often the frames of a layer occur is a property of the temporal
// structure, which either stays the same forever or changes wholesale. The
// samples are constant as long as the structure is, so there is no ripple to
// suppress and the filter only has to average out the jitter of structures
// whose layers do not occur at a fixed interval. A slow filter is what lets a
// single out of phase interval, as seen after a keyframe or a dropped frame,
// pass without disturbing the estimate.
constexpr float kFrameIntervalAlpha = 0.9f;

// The number of frames between consecutive frames of temporal layer `tid` in a
// dyadic L1Tx pattern with `num_layers` temporal layers. The topmost layer
// holds every other frame, the one below it every fourth, and so on, with the
// base layer matching the layer just above it.
double DyadicFrameInterval(int tid, int num_layers) {
  if (num_layers <= 1) {
    return 1.0;
  }
  return 1 << (num_layers - std::max(tid, 1));
}

float Filtered(const ExpFilter& filter) {
  const float value = filter.filtered();
  return value == ExpFilter::kValueUndefined ? 0.0f : value;
}

}  // namespace

DataRate CumulativeCbrAllocation::SpatialLayerBitrate(int spatial_id) const {
  RTC_DCHECK_GE(spatial_id, 0);
  RTC_DCHECK_LT(spatial_id, kMaxSpatialLayers);
  return bitrate[spatial_id][kMaxTemporalLayers - 1];
}

DataRate CumulativeCbrAllocation::TotalBitrate() const {
  DataRate total = DataRate::Zero();
  for (int sid = 0; sid < kMaxSpatialLayers; ++sid) {
    total += SpatialLayerBitrate(sid);
  }
  return total;
}

CbrLayerRateTracker::CbrLayerRateTracker()
    : frame_interval_filters_(kMaxTemporalLayers,
                              ExpFilter(kFrameIntervalAlpha)) {
  // Until a frame of a higher layer shows up the stream is assumed to consist
  // of a single temporal layer holding all of the frames.
  frame_interval_filters_[0].Apply(1.0f, 1.0f);
}

void CbrLayerRateTracker::PrimeStandardPattern(int num_layers) {
  RTC_DCHECK_GT(num_layers, 1);
  RTC_DCHECK_LE(num_layers, kMaxTemporalLayers);
  num_temporal_layers_ = std::max(num_temporal_layers_, num_layers);

  for (int tid = 0; tid < kMaxTemporalLayers; ++tid) {
    frame_interval_filters_[tid].Reset(kFrameIntervalAlpha);
    if (tid < num_layers) {
      frame_interval_filters_[tid].Apply(
          1.0f, static_cast<float>(DyadicFrameInterval(tid, num_layers)));
    }
  }
}

void CbrLayerRateTracker::PrimeLayerRates(int num_layers,
                                          int spatial_id,
                                          DataRate delta_bitrate) {
  // Only the bitrates of the base layer, taken from the keyframe, and of the
  // topmost layer, taken from the frame at hand, are known. In the recommended
  // distribution, where the per frame bit budget halves for every step up the
  // temporal layer stack, every layer above the base one ends up with the same
  // share of the bitrate: the frames of a layer are twice as many but half as
  // large as those of the layer below it. Assume that is the case here, which
  // leaves the base layer as observed on the keyframe. The guesses are not
  // recorded as stated bitrates, so the first frame of a layer that turns out
  // to hold something else is not mistaken for a change of the allocation.
  for (int tid = 1; tid < num_layers; ++tid) {
    layer_rates_[spatial_id][tid].estimate = delta_bitrate;
  }
}

void CbrLayerRateTracker::UpdateLayerRates(int spatial_id,
                                           int temporal_id,
                                           DataRate layer_bitrate) {
  SpatialLayerRates& layers = layer_rates_[spatial_id];
  LayerRate& layer = layers[temporal_id];

  // A caller that changes the allocation normally scales the whole stream, so
  // a layer that moves is taken to speak for the layers that have not reported
  // since. What is carried over is only the part of the change the tracker did
  // not already assume, and a layer that restates the bitrate it already had
  // says nothing at all, which together keep a change of the distribution from
  // being handed back and forth between the layers.
  if (layer.stated.has_value() && *layer.stated != layer_bitrate &&
      layer.estimate > DataRate::Zero()) {
    const double change = layer_bitrate / layer.estimate;
    for (int tid = 0; tid < kMaxTemporalLayers; ++tid) {
      if (tid != temporal_id) {
        layers[tid].estimate = layers[tid].estimate * change;
      }
    }
  }

  layer.estimate = layer_bitrate;
  layer.stated = layer_bitrate;
}

void CbrLayerRateTracker::OnTemporalUnit(
    std::span<const FrameEncodeSettings> frames) {
  // The temporal id of the first frame of the temporal unit is what the
  // cadence is measured from.
  std::optional<int> unit_temporal_id;
  bool has_keyframe = false;
  for (const FrameEncodeSettings& frame : frames) {
    if (!std::holds_alternative<FrameEncodeSettings::Cbr>(
            frame.rate_options())) {
      continue;
    }
    RTC_DCHECK_GE(frame.spatial_id(), 0);
    RTC_DCHECK_LT(frame.spatial_id(), kMaxSpatialLayers);
    RTC_DCHECK_GE(frame.temporal_id(), 0);
    RTC_DCHECK_LT(frame.temporal_id(), kMaxTemporalLayers);
    if (!unit_temporal_id.has_value()) {
      unit_temporal_id = frame.temporal_id();
    }
    has_keyframe |=
        frame.frame_type() == VideoEncoderInterface::FrameType::kKeyframe;
  }
  if (!unit_temporal_id.has_value()) {
    return;
  }
  const int tid = *unit_temporal_id;

  // A keyframe restarts the temporal structure, and since it belongs to the
  // base layer the temporal unit that follows it reveals the layer count.
  const bool prime = last_unit_had_keyframe_ && !has_keyframe && tid > 0;
  last_unit_had_keyframe_ = has_keyframe;
  if (prime) {
    PrimeStandardPattern(tid + 1);
  }

  for (const FrameEncodeSettings& frame : frames) {
    const auto* cbr =
        std::get_if<FrameEncodeSettings::Cbr>(&frame.rate_options());
    if (cbr == nullptr) {
      continue;
    }
    if (prime) {
      PrimeLayerRates(tid + 1, frame.spatial_id(), cbr->target_bitrate);
    }
    UpdateLayerRates(frame.spatial_id(), frame.temporal_id(),
                     cbr->target_bitrate);
    num_temporal_layers_ =
        std::max(num_temporal_layers_, frame.temporal_id() + 1);
  }

  ++temporal_unit_count_;
  if (last_unit_of_layer_[tid].has_value()) {
    frame_interval_filters_[tid].Apply(
        1.0f, temporal_unit_count_ - *last_unit_of_layer_[tid]);
  }
  last_unit_of_layer_[tid] = temporal_unit_count_;

  UpdateAllocation();
}

double CbrLayerRateTracker::FrameFraction(int temporal_id) const {
  const double interval = Filtered(frame_interval_filters_[temporal_id]);
  return interval > 0.0 ? 1.0 / interval : 0.0;
}

int CbrLayerRateTracker::FramerateFactor(int temporal_id) const {
  if (temporal_id >= num_temporal_layers_ - 1) {
    return 1;
  }

  double total_fraction = 0.0;
  double cumulative_fraction = 0.0;
  for (int tid = 0; tid < num_temporal_layers_; ++tid) {
    const double fraction = FrameFraction(tid);
    total_fraction += fraction;
    if (tid <= temporal_id) {
      cumulative_fraction += fraction;
    }
  }

  if (cumulative_fraction <= 0.0) {
    return 1;
  }
  return std::max(
      1, static_cast<int>(std::round(total_fraction / cumulative_fraction)));
}

DataRate CbrLayerRateTracker::CumulativeBitrate(int spatial_id,
                                                int temporal_id) const {
  DataRate bitrate = DataRate::Zero();
  for (int tid = 0; tid <= std::min(temporal_id, num_temporal_layers_ - 1);
       ++tid) {
    bitrate += layer_rates_[spatial_id][tid].estimate;
  }
  return bitrate;
}

void CbrLayerRateTracker::UpdateAllocation() {
  allocation_.num_temporal_layers = num_temporal_layers_;
  for (int tid = 0; tid < kMaxTemporalLayers; ++tid) {
    allocation_.framerate_factor[tid] = FramerateFactor(tid);
  }
  for (int sid = 0; sid < kMaxSpatialLayers; ++sid) {
    for (int tid = 0; tid < kMaxTemporalLayers; ++tid) {
      allocation_.bitrate[sid][tid] = CumulativeBitrate(sid, tid);
    }
  }
}

}  // namespace webrtc
