/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_SIMULATED_PACKET_RECEIVER_H_
#define CALL_SIMULATED_PACKET_RECEIVER_H_

#include "api/test/simulated_network.h"
#include "call/packet_receiver.h"
#include "modules/include/module.h"

namespace webrtc {

// Private API that is fixing surface between DirectTransport and underlying
// network conditions simulation implementation.
class SimulatedPacketReceiverInterface : public PacketReceiver, public Module {
 public:
  // Must not be called in parallel with DeliverPacket or Process.
  // Destination receiver will be injected with this method
  virtual void SetReceiver(PacketReceiver* receiver) = 0;

  // Reports average packet delay.
  virtual int AverageDelay() = 0;
};

}  // namespace webrtc

#endif  // CALL_SIMULATED_PACKET_RECEIVER_H_
