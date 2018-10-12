/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_SIMULCAST_RATE_ALLOCATOR_H_
#define MODULES_VIDEO_CODING_UTILITY_SIMULCAST_RATE_ALLOCATOR_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "api/video/video_bitrate_allocator.h"
#include "api/video_codecs/video_encoder.h"
#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/constructormagic.h"

namespace webrtc {

class SimulcastRateAllocator : public VideoBitrateAllocator {
 public:
  explicit SimulcastRateAllocator(const VideoCodec& codec);
  ~SimulcastRateAllocator() override;

  VideoBitrateAllocation GetAllocation(uint32_t total_bitrate_bps,
                                       uint32_t framerate) override;
  const VideoCodec& GetCodec() const;

  static float GetTemporalRateAllocation(int num_layers, int temporal_id);

 private:
  void DistributeAllocationToSimulcastLayers(
      uint32_t total_bitrate_bps,
      VideoBitrateAllocation* allocated_bitrates_bps);
  void DistributeAllocationToTemporalLayers(
      uint32_t framerate,
      VideoBitrateAllocation* allocated_bitrates_bps) const;
  std::vector<uint32_t> DefaultTemporalLayerAllocation(int bitrate_kbps,
                                                       int max_bitrate_kbps,
                                                       int framerate,
                                                       int simulcast_id) const;
  std::vector<uint32_t> ScreenshareTemporalLayerAllocation(
      int bitrate_kbps,
      int max_bitrate_kbps,
      int framerate,
      int simulcast_id) const;
  int NumTemporalStreams(size_t simulcast_id) const;

  const VideoCodec codec_;
  const double hysteresis_factor_;
  std::vector<bool> stream_enabled_;

  RTC_DISALLOW_COPY_AND_ASSIGN(SimulcastRateAllocator);
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_UTILITY_SIMULCAST_RATE_ALLOCATOR_H_
