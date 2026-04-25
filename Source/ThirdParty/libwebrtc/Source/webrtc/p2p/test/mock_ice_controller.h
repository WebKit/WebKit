/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_TEST_MOCK_ICE_CONTROLLER_H_
#define P2P_TEST_MOCK_ICE_CONTROLLER_H_

#include <memory>
#include <vector>

#include "api/array_view.h"
#include "api/units/timestamp.h"
#include "p2p/base/connection.h"
#include "p2p/base/ice_controller_factory_interface.h"
#include "p2p/base/ice_controller_interface.h"
#include "p2p/base/ice_switch_reason.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/transport_description.h"
#include "test/gmock.h"

namespace webrtc {

class MockIceController : public IceControllerInterface {
 public:
  MockIceController() = default;
  explicit MockIceController(const IceControllerFactoryArgs& /* args */) {}
  ~MockIceController() override = default;

  MOCK_METHOD(void, SetIceConfig, (const IceConfig&), (override));
  MOCK_METHOD(void, SetSelectedConnection, (const Connection*), (override));
  MOCK_METHOD(void, AddConnection, (const Connection*), (override));
  MOCK_METHOD(void, OnConnectionDestroyed, (const Connection*), (override));
  MOCK_METHOD(ArrayView<const Connection* const>,
              GetConnections,
              (),
              (const, override));
  MOCK_METHOD(ArrayView<const Connection*>, connections, (), (const, override));
  MOCK_METHOD(bool, HasPingableConnection, (), (const, override));
  MOCK_METHOD(PingResult, GetConnectionToPing, (Timestamp), (override));
  MOCK_METHOD(bool,
              GetUseCandidateAttr,
              (const Connection*, NominationMode, IceMode),
              (const, override));
  MOCK_METHOD(const Connection*, FindNextPingableConnection, (), (override));
  MOCK_METHOD(void, MarkConnectionPinged, (const Connection*), (override));
  MOCK_METHOD(SwitchResult,
              ShouldSwitchConnection,
              (IceSwitchReason, const Connection*),
              (override));
  MOCK_METHOD(SwitchResult,
              SortAndSwitchConnection,
              (IceSwitchReason),
              (override));
  MOCK_METHOD(std::vector<const Connection*>, PruneConnections, (), (override));
};

class MockIceControllerFactory : public IceControllerFactoryInterface {
 public:
  ~MockIceControllerFactory() override = default;

  std::unique_ptr<IceControllerInterface> Create(
      const IceControllerFactoryArgs& /*args*/) override {
    RecordIceControllerCreated();
    return std::make_unique<MockIceController>();
  }

  MOCK_METHOD(void, RecordIceControllerCreated, ());
};

}  //  namespace webrtc


#endif  // P2P_TEST_MOCK_ICE_CONTROLLER_H_
