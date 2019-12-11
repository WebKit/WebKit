/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_PEER_SCENARIO_SIGNALING_ROUTE_H_
#define TEST_PEER_SCENARIO_SIGNALING_ROUTE_H_

#include <string>
#include <utility>

#include "test/network/network_emulation_manager.h"
#include "test/peer_scenario/peer_scenario_client.h"

namespace webrtc {
namespace test {

// Helper class to reduce the amount of boilerplate required for ICE signalling
// ad SDP negotiation.
class SignalingRoute {
 public:
  SignalingRoute(PeerScenarioClient* caller,
                 PeerScenarioClient* callee,
                 TrafficRoute* send_route,
                 TrafficRoute* ret_route);

  void StartIceSignaling();

  // TODO(srte): Handle lossy links.
  void NegotiateSdp(
      std::function<void(SessionDescriptionInterface* offer)> modify_offer,
      std::function<void(const SessionDescriptionInterface& answer)>
          exchange_finished);
  void NegotiateSdp(
      std::function<void(const SessionDescriptionInterface& answer)>
          exchange_finished);
  SignalingRoute reverse() {
    return SignalingRoute(callee_, caller_, ret_route_, send_route_);
  }

 private:
  PeerScenarioClient* const caller_;
  PeerScenarioClient* const callee_;
  TrafficRoute* const send_route_;
  TrafficRoute* const ret_route_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PEER_SCENARIO_SIGNALING_ROUTE_H_
