/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/codecs/vp8/simulcast_rate_allocator.h"

#include <algorithm>
#include <memory>
#include <vector>
#include <utility>

#include "webrtc/base/checks.h"

namespace webrtc {

SimulcastRateAllocator::SimulcastRateAllocator(
    const VideoCodec& codec,
    std::unique_ptr<TemporalLayersFactory> tl_factory)
    : codec_(codec), tl_factory_(std::move(tl_factory)) {
  if (tl_factory_.get())
    tl_factory_->SetListener(this);
}

void SimulcastRateAllocator::OnTemporalLayersCreated(int simulcast_id,
                                                     TemporalLayers* layers) {
  RTC_DCHECK(temporal_layers_.find(simulcast_id) == temporal_layers_.end());
  RTC_DCHECK(layers);
  temporal_layers_[simulcast_id] = layers;
}

BitrateAllocation SimulcastRateAllocator::GetAllocation(
    uint32_t total_bitrate_bps,
    uint32_t framerate) {
  uint32_t left_to_allocate = total_bitrate_bps;
  if (codec_.maxBitrate && codec_.maxBitrate * 1000 < left_to_allocate)
    left_to_allocate = codec_.maxBitrate * 1000;

  BitrateAllocation allocated_bitrates_bps;
  if (codec_.numberOfSimulcastStreams == 0) {
    // No simulcast, just set the target as this has been capped already.
    allocated_bitrates_bps.SetBitrate(
        0, 0, std::max(codec_.minBitrate * 1000, left_to_allocate));
  } else {
    // Always allocate enough bitrate for the minimum bitrate of the first
    // layer. Suspending below min bitrate is controlled outside the codec
    // implementation and is not overridden by this.
    left_to_allocate =
        std::max(codec_.simulcastStream[0].minBitrate * 1000, left_to_allocate);

    // Begin by allocating bitrate to simulcast streams, putting all bitrate in
    // temporal layer 0. We'll then distribute this bitrate, across potential
    // temporal layers, when stream allocation is done.

    // Allocate up to the target bitrate for each simulcast layer.
    size_t layer = 0;
    for (; layer < codec_.numberOfSimulcastStreams; ++layer) {
      const SimulcastStream& stream = codec_.simulcastStream[layer];
      if (left_to_allocate < stream.minBitrate * 1000)
        break;
      uint32_t allocation =
          std::min(left_to_allocate, stream.targetBitrate * 1000);
      allocated_bitrates_bps.SetBitrate(layer, 0, allocation);
      RTC_DCHECK_LE(allocation, left_to_allocate);
      left_to_allocate -= allocation;
    }

    // Next, try allocate remaining bitrate, up to max bitrate, in top stream.
    // TODO(sprang): Allocate up to max bitrate for all layers once we have a
    //               better idea of possible performance implications.
    if (left_to_allocate > 0) {
      size_t active_layer = layer - 1;
      const SimulcastStream& stream = codec_.simulcastStream[active_layer];
      uint32_t bitrate_bps =
          allocated_bitrates_bps.GetSpatialLayerSum(active_layer);
      uint32_t allocation =
          std::min(left_to_allocate, stream.maxBitrate * 1000 - bitrate_bps);
      bitrate_bps += allocation;
      RTC_DCHECK_LE(allocation, left_to_allocate);
      left_to_allocate -= allocation;
      allocated_bitrates_bps.SetBitrate(active_layer, 0, bitrate_bps);
    }
  }

  const int num_spatial_streams =
      std::max(1, static_cast<int>(codec_.numberOfSimulcastStreams));

  // Finally, distribute the bitrate for the simulcast streams across the
  // available temporal layers.
  for (int simulcast_id = 0; simulcast_id < num_spatial_streams;
       ++simulcast_id) {
    auto tl_it = temporal_layers_.find(simulcast_id);
    if (tl_it == temporal_layers_.end())
      continue;  // TODO(sprang): If > 1 SS, assume default TL alloc?

    uint32_t target_bitrate_kbps =
        allocated_bitrates_bps.GetBitrate(simulcast_id, 0) / 1000;
    const uint32_t expected_allocated_bitrate_kbps = target_bitrate_kbps;
    RTC_DCHECK_EQ(
        target_bitrate_kbps,
        allocated_bitrates_bps.GetSpatialLayerSum(simulcast_id) / 1000);
    const int num_temporal_streams = std::max<uint8_t>(
        1, codec_.numberOfSimulcastStreams == 0
               ? codec_.VP8().numberOfTemporalLayers
               : codec_.simulcastStream[simulcast_id].numberOfTemporalLayers);

    uint32_t max_bitrate_kbps;
    // Legacy temporal-layered only screenshare, or simulcast screenshare
    // with legacy mode for simulcast stream 0.
    if (codec_.mode == kScreensharing && codec_.targetBitrate > 0 &&
        ((num_spatial_streams == 1 && num_temporal_streams == 2) ||  // Legacy.
         (num_spatial_streams > 1 && simulcast_id == 0))) {  // Simulcast.
      // TODO(holmer): This is a "temporary" hack for screensharing, where we
      // interpret the startBitrate as the encoder target bitrate. This is
      // to allow for a different max bitrate, so if the codec can't meet
      // the target we still allow it to overshoot up to the max before dropping
      // frames. This hack should be improved.
      int tl0_bitrate = std::min(codec_.targetBitrate, target_bitrate_kbps);
      max_bitrate_kbps = std::min(codec_.maxBitrate, target_bitrate_kbps);
      target_bitrate_kbps = tl0_bitrate;
    } else if (num_spatial_streams == 1) {
      max_bitrate_kbps = codec_.maxBitrate;
    } else {
      max_bitrate_kbps = codec_.simulcastStream[simulcast_id].maxBitrate;
    }

    std::vector<uint32_t> tl_allocation = tl_it->second->OnRatesUpdated(
        target_bitrate_kbps, max_bitrate_kbps, framerate);
    RTC_DCHECK_GT(tl_allocation.size(), 0);
    RTC_DCHECK_LE(tl_allocation.size(), num_temporal_streams);

    uint64_t tl_allocation_sum_kbps = 0;
    for (size_t tl_index = 0; tl_index < tl_allocation.size(); ++tl_index) {
      uint32_t layer_rate_kbps = tl_allocation[tl_index];
      allocated_bitrates_bps.SetBitrate(simulcast_id, tl_index,
                                        layer_rate_kbps * 1000);
      tl_allocation_sum_kbps += layer_rate_kbps;
    }
    RTC_DCHECK_LE(tl_allocation_sum_kbps, expected_allocated_bitrate_kbps);
  }

  return allocated_bitrates_bps;
}

uint32_t SimulcastRateAllocator::GetPreferredBitrateBps(uint32_t framerate) {
  // Create a temporary instance without temporal layers, as they may be
  // stateful, and updating the bitrate to max here can cause side effects.
  SimulcastRateAllocator temp_allocator(codec_, nullptr);
  BitrateAllocation allocation =
      temp_allocator.GetAllocation(codec_.maxBitrate * 1000, framerate);
  return allocation.get_sum_bps();
}

const VideoCodec& webrtc::SimulcastRateAllocator::GetCodec() const {
  return codec_;
}

}  // namespace webrtc
