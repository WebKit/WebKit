/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TRANSPORT_TEST_MOCK_NETWORK_CONTROL_H_
#define API_TRANSPORT_TEST_MOCK_NETWORK_CONTROL_H_

#include "api/transport/network_control.h"
#include "test/gmock.h"

namespace webrtc {

class MockNetworkStateEstimator : public NetworkStateEstimator {
 public:
  MOCK_METHOD(absl::optional<NetworkStateEstimate>,
              GetCurrentEstimate,
              (),
              (override));
  MOCK_METHOD(void,
              OnTransportPacketsFeedback,
              (const TransportPacketsFeedback&),
              (override));
  MOCK_METHOD(void, OnReceivedPacket, (const PacketResult&), (override));
  MOCK_METHOD(void, OnRouteChange, (const NetworkRouteChange&), (override));
};

}  // namespace webrtc

#endif  // API_TRANSPORT_TEST_MOCK_NETWORK_CONTROL_H_
