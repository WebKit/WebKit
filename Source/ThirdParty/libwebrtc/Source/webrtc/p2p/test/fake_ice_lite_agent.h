/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_TEST_FAKE_ICE_LITE_AGENT_H_
#define P2P_TEST_FAKE_ICE_LITE_AGENT_H_

#include <map>
#include <memory>
#include <utility>

#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "p2p/base/active_ice_controller_factory_interface.h"
#include "p2p/base/active_ice_controller_interface.h"
#include "p2p/base/connection.h"
#include "p2p/base/ice_switch_reason.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/transport_description.h"
#include "rtc_base/checks.h"

namespace webrtc {

// Implement "IceLite" suitable for testing,
// by using the ActiveIceControllerInterface
class FakeIceLiteAgent : public ActiveIceControllerInterface {
 public:
  explicit FakeIceLiteAgent(const ActiveIceControllerFactoryArgs& args)
      : args_(args), network_thread_(TaskQueueBase::Current()) {}

  // Sets the current ICE configuration.
  void SetIceConfig(const IceConfig& config) override {}

  // Called when a new connection is added to the ICE transport.
  void OnConnectionAdded(const Connection* connection) override {}

  // Called when the transport switches the connection in active use.
  void OnConnectionSwitched(const Connection* connection) override {
    args_.ice_agent->UpdateState();
  }

  // Called when a connection is destroyed.
  void OnConnectionDestroyed(const Connection* connection) override {
    connections_in_use_.erase(connection);
  }

  // Called when a STUN ping has been sent on a connection. This does not
  // indicate that a STUN response has been received.
  void OnConnectionPinged(const Connection* connection) override {
    RTC_CHECK(false) << "We never send any STUN_BINDING_REQUEST !!";
  }

  // Called when one of the following changes for a connection.
  // - rtt estimate
  // - write state
  // - receiving
  // - connected
  // - nominated
  void OnConnectionUpdated(const Connection* connection) override {
    // NOTE: we don't know which field has been updated...so we need to do this
    // every time... _(
    MarkConnectionInUse(connection);
    network_thread_->PostTask(SafeTask(
        task_safety_.flag(), [this, connection = std::move(connection)]() {
          if (UnmarkConnection(connection)) {
            if (connection->receiving()) {
              if (connection->set_writable_for_fake_ice_lite()) {
                args_.ice_agent->UpdateConnectionStates();
              }
            }
          }
        }));
  }

  // Compute "STUN_ATTR_USE_CANDIDATE" for a STUN ping on the given connection.
  bool GetUseCandidateAttribute(const Connection* connection,
                                NominationMode mode,
                                IceMode remote_ice_mode) const override {
    return false;
  }

  // Called to enqueue a request to pick and switch to the best available
  // connection.
  void OnSortAndSwitchRequest(IceSwitchReason reason) override {}

  // Called to pick and switch to the best available connection immediately.
  void OnImmediateSortAndSwitchRequest(IceSwitchReason reason) override {}

  // Called to switch to the given connection immediately without checking for
  // the best available connection.
  bool OnImmediateSwitchRequest(IceSwitchReason reason,
                                const Connection* selected) override {
    switch (reason) {
      // REMOTE_CANDIDATE_GENERATION_CHANGE,
      // NETWORK_PREFERENCE_CHANGE,
      // NEW_CONNECTION_FROM_LOCAL_CANDIDATE,
      // NEW_CONNECTION_FROM_REMOTE_CANDIDATE,
      // NEW_CONNECTION_FROM_UNKNOWN_REMOTE_ADDRESS,
      // NOMINATION_ON_CONTROLLED_SIDE,
      // CONNECT_STATE_CHANGE,
      // SELECTED_CONNECTION_DESTROYED,
      // ICE_CONTROLLER_RECHECK,
      // // The webrtc application requested a connection switch.
      // APPLICATION_REQUESTED,
      case IceSwitchReason::NOMINATION_ON_CONTROLLED_SIDE:
      case IceSwitchReason::DATA_RECEIVED:
        if (selected) {
          // selected->set_writable_for_fake_ice_lite();
          args_.ice_agent->SwitchSelectedConnection(selected, reason);
          args_.ice_agent->UpdateConnectionStates();
          return true;
        }
        break;
      default:
        // All the other can happen, which is ok
        // we simply ignore.
        break;
    }
    return false;
  }

  // Only for unit tests
  const Connection* FindNextPingableConnection() override { return nullptr; }

 private:
  ActiveIceControllerFactoryArgs args_;
  TaskQueueBase* const network_thread_;
  ScopedTaskSafety task_safety_;
  std::map<const Connection*, int> connections_in_use_;

  void MarkConnectionInUse(const Connection* con) {
    connections_in_use_[con]++;
  }

  // Unmark a connection and see it it is still valid.
  bool UnmarkConnection(const Connection* con) {
    auto c = connections_in_use_.find(con);
    if (c == connections_in_use_.end()) {
      return false;
    }
    if (c->second == 1) {
      connections_in_use_.erase(c);
    }
    return true;
  }
};

class FakeIceLiteAgentIceControllerFactory
    : public ActiveIceControllerFactoryInterface {
 public:
  ~FakeIceLiteAgentIceControllerFactory() override = default;

  std::unique_ptr<ActiveIceControllerInterface> Create(
      const ActiveIceControllerFactoryArgs& args) override {
    return std::make_unique<FakeIceLiteAgent>(args);
  }
};

}  //  namespace webrtc

#endif  // P2P_TEST_FAKE_ICE_LITE_AGENT_H_
