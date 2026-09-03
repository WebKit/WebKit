/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_TIMING_VIDEO_JITTER_TIMING_INTERFACE_H_
#define API_VIDEO_TIMING_VIDEO_JITTER_TIMING_INTERFACE_H_

#include <cstdint>
#include <optional>
#include <span>

#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc {

// NOTE: This class is still under development and may change without notice.
class VideoJitterTimingInterface {
 public:
  struct FrameInfo {
    uint32_t rtp_timestamp = 0;
    Timestamp time = Timestamp::PlusInfinity();
    bool last_spatial_layer = false;
    bool was_retransmitted = false;
  };

  struct TemporalUnitInfo {
    uint32_t rtp_timestamp = 0;
    DataSize size = DataSize::Zero();
    Timestamp time = Timestamp::PlusInfinity();
    bool was_retransmitted = false;
  };

  struct NetworkInfo {
    TimeDelta rtt = TimeDelta::PlusInfinity();
  };

  virtual ~VideoJitterTimingInterface() = default;

  // Resets the model to its initial state.
  virtual void Reset() = 0;

  // Updates the model when a frame becomes complete.
  virtual void OnCompleteFrame(const FrameInfo& info) = 0;

  // Updates the model when temporal units become continuous.
  // Returns the estimated jitter delay (or nullopt if not available).
  virtual std::optional<TimeDelta> OnContinuousTemporalUnits(
      std::span<const uint32_t> rtp_timestamps,
      Timestamp now) = 0;

  // Updates the model when a temporal unit becomes decodable.
  // Returns the estimated jitter delay (or nullopt if not available).
  virtual std::optional<TimeDelta> OnDecodableTemporalUnit(
      const TemporalUnitInfo& info) = 0;

  // Updates the model with network information.
  virtual void OnNetworkUpdate(const NetworkInfo& info) = 0;

  // Returns the estimated local clock time for a given RTP timestamp (or
  // nullopt if not available).
  virtual std::optional<Timestamp> LocalTime(uint32_t rtp_timestamp) const = 0;
};

}  // namespace webrtc

#endif  // API_VIDEO_TIMING_VIDEO_JITTER_TIMING_INTERFACE_H_
