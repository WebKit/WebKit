/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_NETEQ_DELAY_MANAGER_INTERFACE_H_
#define API_NETEQ_DELAY_MANAGER_INTERFACE_H_

#include "api/neteq/neteq_controller.h"

namespace webrtc {

// Interface for the delay manager.
class DelayManagerInterface {
 public:
  virtual ~DelayManagerInterface() = default;

  // Updates the delay manager that a new packet arrived with delay
  // `arrival_delay_ms`. This updates the statistics and a new target buffer
  // level is calculated. The `reordered` flag indicates if the packet was
  // reordered. The `info` argument contains information about the packet.
  virtual void Update(int arrival_delay_ms,
                      bool reordered,
                      NetEqController::PacketArrivedInfo info) = 0;

  // Resets all state.
  virtual void Reset() = 0;

  // Gets the target buffer level in milliseconds.
  virtual int TargetDelayMs() const = 0;
};

}  // namespace webrtc

#endif  // API_NETEQ_DELAY_MANAGER_INTERFACE_H_
