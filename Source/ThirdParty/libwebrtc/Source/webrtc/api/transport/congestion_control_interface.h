/* Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This is EXPERIMENTAL interface for media and datagram transports.

#ifndef API_TRANSPORT_CONGESTION_CONTROL_INTERFACE_H_
#define API_TRANSPORT_CONGESTION_CONTROL_INTERFACE_H_

#include <memory>
#include <string>
#include <utility>

#include "api/transport/network_control.h"
#include "api/units/data_rate.h"

namespace webrtc {

// TODO(nisse): Defined together with MediaTransportInterface. But we should use
// types that aren't tied to media, so that MediaTransportInterface can depend
// on CongestionControlInterface, but not the other way around.
// api/transport/network_control.h may be a reasonable place.
class MediaTransportRttObserver;
struct MediaTransportAllocatedBitrateLimits;
struct MediaTransportTargetRateConstraints;

// Defines congestion control feedback interface for media and datagram
// transports.
class CongestionControlInterface {
 public:
  virtual ~CongestionControlInterface() = default;

  // Updates allocation limits.
  virtual void SetAllocatedBitrateLimits(
      const MediaTransportAllocatedBitrateLimits& limits) = 0;

  // Sets starting rate.
  virtual void SetTargetBitrateLimits(
      const MediaTransportTargetRateConstraints& target_rate_constraints) = 0;

  // Intended for receive side. AddRttObserver registers an observer to be
  // called for each RTT measurement, typically once per ACK. Before media
  // transport is destructed the observer must be unregistered.
  //
  // TODO(sukhanov): Looks like AddRttObserver and RemoveRttObserver were
  // never implemented for media transport, so keeping noop implementation.
  virtual void AddRttObserver(MediaTransportRttObserver* observer) {}
  virtual void RemoveRttObserver(MediaTransportRttObserver* observer) {}

  // Adds a target bitrate observer. Before media transport is destructed
  // the observer must be unregistered (by calling
  // RemoveTargetTransferRateObserver).
  // A newly registered observer will be called back with the latest recorded
  // target rate, if available.
  virtual void AddTargetTransferRateObserver(
      TargetTransferRateObserver* observer) = 0;

  // Removes an existing |observer| from observers. If observer was never
  // registered, an error is logged and method does nothing.
  virtual void RemoveTargetTransferRateObserver(
      TargetTransferRateObserver* observer) = 0;

  // Returns the last known target transfer rate as reported to the above
  // observers.
  virtual absl::optional<TargetTransferRate> GetLatestTargetTransferRate() = 0;
};

}  // namespace webrtc

#endif  // API_TRANSPORT_CONGESTION_CONTROL_INTERFACE_H_
