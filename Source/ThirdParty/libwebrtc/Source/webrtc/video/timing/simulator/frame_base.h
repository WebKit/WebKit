/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_TIMING_SIMULATOR_FRAME_BASE_H_
#define VIDEO_TIMING_SIMULATOR_FRAME_BASE_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"

namespace webrtc::video_timing_simulator {

// CRTP base struct for code reuse of departure and arrival timestamp functions.
template <typename FrameT>
struct FrameBase {
  // Data members are defined in derived struct.

  // -- CRTP accessor --
  const FrameT& self() const { return static_cast<const FrameT&>(*this); }

  // -- Value accessors --

  // Departure time (possibly offset), as determined by RTP timestamp from
  // the derived class.
  Timestamp DepartureTimestamp(Timestamp offset = Timestamp::Zero()) const {
    int64_t unwrapped_rtp_timestamp = self().unwrapped_rtp_timestamp;
    RTC_DCHECK_GE(unwrapped_rtp_timestamp, 0);
    constexpr int64_t kMicrosPerMillis = 1'000;
    constexpr int64_t kRtpVideoTicksPerMillis = 90;
    // Convert from RTP ticks to microseconds using integer division with
    // truncation. Note that this introduces an error of up to 1us. That is fine
    // for our purposes however: the arrival timestamp is logged in ms and the
    // expected frame delay variation caused by the network is also on the order
    // of ms.
    int64_t departure_timestamp_us =
        (unwrapped_rtp_timestamp * kMicrosPerMillis) / kRtpVideoTicksPerMillis;
    return Timestamp::Micros(departure_timestamp_us - offset.us());
  }

  // Arrival time (possibly offset), as determined by
  // `ArrivalTimestampInternal()` from the derived class. This allows derived
  // classes to define themselves the meaning of "arrival": typically decodable
  // or rendered, but could be assembled or decoded as well.
  Timestamp ArrivalTimestamp(Timestamp offset = Timestamp::Zero()) const {
    Timestamp arrival_timestamp = self().ArrivalTimestampInternal();
    if (!arrival_timestamp.IsFinite()) {
      return arrival_timestamp;
    }
    return Timestamp::Micros(arrival_timestamp.us() - offset.us());
  }

  // -- Per-frame metrics --
  // One way delay with required timestamp offset normalization.
  TimeDelta OneWayDelay(Timestamp arrival_offset,
                        Timestamp departure_offset) const {
    return ArrivalTimestamp(arrival_offset) -
           DepartureTimestamp(departure_offset);
  }
};

// -- Comparators and sorting --
template <typename FrameT>
bool DepartureOrder(const FrameT& a, const FrameT& b) {
  return a.DepartureTimestamp() < b.DepartureTimestamp();
}
template <typename FrameT>
void SortByDepartureOrder(std::vector<FrameT>& frames) {
  absl::c_stable_sort(frames, DepartureOrder<FrameT>);
}

template <typename FrameT>
bool ArrivalOrder(const FrameT& a, const FrameT& b) {
  return a.ArrivalTimestamp() < b.ArrivalTimestamp();
}
template <typename FrameT>
void SortByArrivalOrder(std::vector<FrameT>& frames) {
  absl::c_stable_sort(frames, ArrivalOrder<FrameT>);
}

template <typename FrameT>
inline bool AssembledOrder(const FrameT& a, const FrameT& b) {
  return a.assembled_timestamp < b.assembled_timestamp;
}
template <typename FrameT>
inline void SortByAssembledOrder(std::vector<FrameT>& frames) {
  absl::c_stable_sort(frames, AssembledOrder<FrameT>);
}

// --- Inter-frame metrics ---
// Difference in packet counts between two frames.
template <typename FrameT>
std::optional<int> InterPacketCount(const FrameT& cur, const FrameT& prev) {
  if (cur.num_packets <= 0 || prev.num_packets <= 0) {
    return std::nullopt;
  }
  return cur.num_packets - prev.num_packets;
}

// Difference in frame size (bytes) between two frames.
template <typename FrameT>
std::optional<int64_t> InterFrameSizeBytes(const FrameT& cur,
                                           const FrameT& prev) {
  if (cur.size.IsZero() || prev.size.IsZero()) {
    return std::nullopt;
  }
  return cur.size.bytes() - prev.size.bytes();
}

// Difference in departure timestamp between two frames.
template <typename FrameT>
TimeDelta InterDepartureTime(const FrameT& cur, const FrameT& prev) {
  RTC_DCHECK(cur.DepartureTimestamp().IsFinite());
  RTC_DCHECK(prev.DepartureTimestamp().IsFinite());
  return cur.DepartureTimestamp() - prev.DepartureTimestamp();
}

// Difference in arrival timestamp between two frames.
template <typename FrameT>
TimeDelta InterArrivalTime(const FrameT& cur, const FrameT& prev) {
  Timestamp cur_arrival = cur.ArrivalTimestamp();
  Timestamp prev_arrival = prev.ArrivalTimestamp();
  if (!cur_arrival.IsFinite() && !prev_arrival.IsFinite()) {
    return TimeDelta::PlusInfinity();
  }
  return cur_arrival - prev_arrival;
}

// https://datatracker.ietf.org/doc/html/rfc5481#section-1
template <typename FrameT>
TimeDelta InterFrameDelayVariation(const FrameT& cur, const FrameT& prev) {
  TimeDelta iat = InterArrivalTime(cur, prev);
  TimeDelta idt = InterDepartureTime(cur, prev);
  RTC_DCHECK(idt.IsFinite());
  return iat - idt;
}

// Difference in assembled timestamp between two frames.
template <typename FrameT>
TimeDelta InterAssembledTime(const FrameT& cur, const FrameT& prev) {
  RTC_DCHECK(cur.assembled_timestamp.IsFinite());
  RTC_DCHECK(prev.assembled_timestamp.IsFinite());
  return cur.assembled_timestamp - prev.assembled_timestamp;
}

}  // namespace webrtc::video_timing_simulator

#endif  // VIDEO_TIMING_SIMULATOR_FRAME_BASE_H_
