/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_CBR_LAYER_RATE_TRACKER_H_
#define MODULES_VIDEO_CODING_UTILITY_CBR_LAYER_RATE_TRACKER_H_

#include <array>
#include <optional>
#include <span>

#include "absl/container/inlined_vector.h"
#include "api/units/data_rate.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "rtc_base/numerics/exp_filter.h"

namespace webrtc {

// The rates of all the layers of a stream, in the form encoder libraries tend
// to want them. Unlike `VideoBitrateAllocation` the bitrates are cumulative
// over the temporal layers: the entry for a temporal layer describes the stream
// made up of that layer and all the temporal layers below it, within the same
// spatial layer. They are not cumulative over the spatial layers.
struct CumulativeCbrAllocation {
  static constexpr int kMaxSpatialLayers = 4;
  static constexpr int kMaxTemporalLayers = 4;

  // The bitrate of all temporal layers of spatial layer `spatial_id` combined.
  DataRate SpatialLayerBitrate(int spatial_id) const;

  // The bitrate of all layers combined.
  DataRate TotalBitrate() const;

  bool operator==(const CumulativeCbrAllocation& other) const = default;

  // The number of temporal layers seen so far, or the number guessed from the
  // start of the stream. At least one.
  int num_temporal_layers = 1;

  // How many times lower the frame rate of the stream made up of the temporal
  // layers up to and including a given one is compared to the frame rate of
  // the full stream. Shared by all spatial layers. Rounded to an integer, which
  // is exact for the dyadic temporal structures used in practice. Entries from
  // the topmost temporal layer and up are one.
  std::array<int, kMaxTemporalLayers> framerate_factor = {1, 1, 1, 1};

  // `bitrate[spatial_id][temporal_id]` is the bitrate of the stream made up of
  // the temporal layers up to and including `temporal_id` within spatial layer
  // `spatial_id`. Entries above the topmost temporal layer repeat its value,
  // and spatial layers never heard from are zero.
  std::array<std::array<DataRate, kMaxTemporalLayers>, kMaxSpatialLayers>
      bitrate = {};
};

// Aggregates rate control decisions that are made per frame into a
// `CumulativeCbrAllocation`.
//
// Callers of `VideoEncoderInterface` state the bitrate of the layer a frame
// belongs to, one frame at a time. Encoder libraries on the other hand tend to
// want the bitrate and the frame rate of the stream formed by all the temporal
// layers up to and including a given one, which this class derives by tracking
// the bitrate stated for each layer together with how often frames of each
// temporal layer occur.
//
// Only the layers present in a temporal unit are heard from, so the bitrate of
// the others has to be assumed until they report. A caller that changes the
// allocation normally scales all of the temporal layers of a spatial layer by
// the same factor, so a change seen on one of them is taken to apply to those
// that have not reported since. The assumption is dropped as soon as a layer
// states a bitrate of its own, and a layer repeating the bitrate it already had
// is not taken as evidence of anything, which is what keeps a change of the
// distribution between the layers from ping ponging around rather than
// settling. Spatial layers are independent of each other in this respect. A
// layer without a frame in a temporal unit, be it a temporal or a spatial one,
// keeps the bitrate it last had.
//
// How often the frames of a temporal layer occur cannot be stated by the
// caller and is measured instead, and smoothed. All spatial layers are assumed
// to share one temporal structure, though not necessarily in phase: the cadence
// is measured from the first frame of each temporal unit, so a structure that
// shifts the spatial layers relative to each other, such as L2T2_KEY_SHIFT, is
// described correctly as well. To avoid a transient at the start of a stream,
// the state is primed on the first temporal unit after a keyframe under the
// assumption that a standard dyadic temporal pattern is used: in such a pattern
// that temporal unit belongs to the topmost temporal layer, which reveals the
// layer count and thereby how often the frames of every layer occur. Structures
// the priming does not predict are learned instead, which takes a few
// repetitions of the pattern.
//
// An instance only ever describes one configuration of one encoder. Replace it
// rather than trying to reuse it when the encoder is reconfigured.
class CbrLayerRateTracker {
 public:
  static constexpr int kMaxSpatialLayers =
      CumulativeCbrAllocation::kMaxSpatialLayers;
  static constexpr int kMaxTemporalLayers =
      CumulativeCbrAllocation::kMaxTemporalLayers;

  CbrLayerRateTracker();

  // Records the frames of one temporal unit, as passed to
  // `VideoEncoderInterface::Encode`. Temporal units must be passed in encode
  // order. Frames that do not use `Cbr` rate options are ignored.
  void OnTemporalUnit(
      std::span<const VideoEncoderInterface::FrameEncodeSettings> frames);

  // The allocation as of the most recent temporal unit.
  const CumulativeCbrAllocation& allocation() const { return allocation_; }

 private:
  // What is known about the bitrate of one temporal layer of one spatial
  // layer.
  struct LayerRate {
    // The bitrate the tracker believes the layer has: what the caller stated
    // for it scaled by the changes seen on the other layers since, or what the
    // priming guessed.
    DataRate estimate = DataRate::Zero();
    // The bitrate the caller most recently stated for the layer, if any. Only
    // a departure from this counts as a change of the allocation.
    std::optional<DataRate> stated;
  };
  using SpatialLayerRates = std::array<LayerRate, kMaxTemporalLayers>;

  // `ExpFilter` is not assignable, so the filters are held in a container that
  // can be filled with copies of a prototype on construction.
  using TemporalLayerFilters =
      absl::InlinedVector<ExpFilter, kMaxTemporalLayers>;

  // Records that `layer_bitrate` was allocated to temporal layer `temporal_id`
  // of spatial layer `spatial_id`, and carries the change over to the layers
  // that have not reported since.
  void UpdateLayerRates(int spatial_id,
                        int temporal_id,
                        DataRate layer_bitrate);

  // Primes the frame intervals as if a standard dyadic pattern with
  // `num_layers` temporal layers was in use.
  void PrimeStandardPattern(int num_layers);

  // Guesses the bitrates of the temporal layers of spatial layer `spatial_id`
  // in a standard dyadic pattern with `num_layers` temporal layers, where the
  // just observed topmost layer frame reported `delta_bitrate`.
  void PrimeLayerRates(int num_layers, int spatial_id, DataRate delta_bitrate);

  // The share of the frames of the stream that belong to `temporal_id`, or
  // zero if no two frames of that layer have been seen yet.
  double FrameFraction(int temporal_id) const;

  int FramerateFactor(int temporal_id) const;
  DataRate CumulativeBitrate(int spatial_id, int temporal_id) const;

  // Recomputes `allocation_` from the state.
  void UpdateAllocation();

  int num_temporal_layers_ = 1;
  bool last_unit_had_keyframe_ = false;
  // The number of temporal units seen so far, which doubles as the index of
  // the current one, and the index of the temporal unit each temporal layer
  // was last seen in.
  int temporal_unit_count_ = 0;
  std::array<std::optional<int>, kMaxTemporalLayers> last_unit_of_layer_;

  // What each temporal layer of each spatial layer is believed to hold.
  std::array<SpatialLayerRates, kMaxSpatialLayers> layer_rates_;

  // The number of temporal units between consecutive frames of each temporal
  // layer. Filtering the interval rather than the share of the frames each
  // layer holds directly means the samples are constant for a fixed temporal
  // structure, so the filter can be quick without the estimate rippling.
  TemporalLayerFilters frame_interval_filters_;

  CumulativeCbrAllocation allocation_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_UTILITY_CBR_LAYER_RATE_TRACKER_H_
