/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/utility/simulcast_rate_allocator.h"

#include <algorithm>

namespace webrtc {

webrtc::SimulcastRateAllocator::SimulcastRateAllocator(const VideoCodec& codec)
    : codec_(codec) {}

std::vector<uint32_t> webrtc::SimulcastRateAllocator::GetAllocation(
    uint32_t bitrate_kbps) const {
  // Always allocate enough bitrate for the minimum bitrate of the first layer.
  // Suspending below min bitrate is controlled outside the codec implementation
  // and is not overridden by this.
  const uint32_t min_bitrate_bps = codec_.numberOfSimulcastStreams == 0
                                       ? codec_.minBitrate
                                       : codec_.simulcastStream[0].minBitrate;
  uint32_t left_to_allocate = std::max(min_bitrate_bps, bitrate_kbps);
  if (codec_.maxBitrate)
    left_to_allocate = std::min(left_to_allocate, codec_.maxBitrate);

  if (codec_.numberOfSimulcastStreams < 2) {
    // No simulcast, just set the target as this has been capped already.
    return std::vector<uint32_t>(1, left_to_allocate);
  }

  // Initialize bitrates with zeroes.
  std::vector<uint32_t> allocated_bitrates_bps(codec_.numberOfSimulcastStreams,
                                               0);

  // First try to allocate up to the target bitrate for each substream.
  size_t layer = 0;
  for (; layer < codec_.numberOfSimulcastStreams; ++layer) {
    const SimulcastStream& stream = codec_.simulcastStream[layer];
    if (left_to_allocate < stream.minBitrate)
      break;
    uint32_t allocation = std::min(left_to_allocate, stream.targetBitrate);
    allocated_bitrates_bps[layer] = allocation;
    left_to_allocate -= allocation;
  }

  // Next, try allocate remaining bitrate, up to max bitrate, in top layer.
  // TODO(sprang): Allocate up to max bitrate for all layers once we have a
  //               better idea of possible performance implications.
  if (left_to_allocate > 0) {
    size_t active_layer = layer - 1;
    const SimulcastStream& stream = codec_.simulcastStream[active_layer];
    uint32_t allocation =
        std::min(left_to_allocate,
                 stream.maxBitrate - allocated_bitrates_bps[active_layer]);
    left_to_allocate -= allocation;
    allocated_bitrates_bps[active_layer] += allocation;
  }

  return allocated_bitrates_bps;
}

uint32_t SimulcastRateAllocator::GetPreferedBitrate() const {
  std::vector<uint32_t> rates = GetAllocation(codec_.maxBitrate);
  uint32_t preferred_bitrate = 0;
  for (const uint32_t& rate : rates) {
    preferred_bitrate += rate;
  }
  return preferred_bitrate;
}

const VideoCodec& webrtc::SimulcastRateAllocator::GetCodec() const {
  return codec_;
}

}  // namespace webrtc
