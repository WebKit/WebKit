/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/regathering_controller.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/scoped_refptr.h"
#include "p2p/base/fake_port_allocator.h"
#include "p2p/base/mock_ice_transport.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/port.h"
#include "p2p/base/stun_server.h"
#include "rtc_base/gunit.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/scoped_key_value_config.h"

namespace {

const int kOnlyLocalPorts = cricket::PORTALLOCATOR_DISABLE_STUN |
                            cricket::PORTALLOCATOR_DISABLE_RELAY |
                            cricket::PORTALLOCATOR_DISABLE_TCP;
// The address of the public STUN server.
const rtc::SocketAddress kStunAddr("99.99.99.1", cricket::STUN_SERVER_PORT);
// The addresses for the public TURN server.
const rtc::SocketAddress kTurnUdpIntAddr("99.99.99.3",
                                         cricket::STUN_SERVER_PORT);
const cricket::RelayCredentials kRelayCredentials("test", "test");
const char kIceUfrag[] = "UF00";
const char kIcePwd[] = "TESTICEPWD00000000000000";

}  // namespace

namespace webrtc {

class RegatheringControllerTest : public ::testing::Test,
                                  public sigslot::has_slots<> {
 public:
  RegatheringControllerTest()
      : vss_(std::make_unique<rtc::VirtualSocketServer>()),
        thread_(vss_.get()),
        ice_transport_(std::make_unique<cricket::MockIceTransport>()),
        packet_socket_factory_(
            std::make_unique<rtc::BasicPacketSocketFactory>(vss_.get())),
        allocator_(std::make_unique<cricket::FakePortAllocator>(
            rtc::Thread::Current(),
            packet_socket_factory_.get(),
            &field_trials_)) {
    BasicRegatheringController::Config regathering_config;
    regathering_config.regather_on_failed_networks_interval = 0;
    regathering_controller_.reset(new BasicRegatheringController(
        regathering_config, ice_transport_.get(), rtc::Thread::Current()));
  }

  // Initializes the allocator and gathers candidates once by StartGettingPorts.
  void InitializeAndGatherOnce() {
    cricket::ServerAddresses stun_servers;
    stun_servers.insert(kStunAddr);
    cricket::RelayServerConfig turn_server;
    turn_server.credentials = kRelayCredentials;
    turn_server.ports.push_back(
        cricket::ProtocolAddress(kTurnUdpIntAddr, cricket::PROTO_UDP));
    std::vector<cricket::RelayServerConfig> turn_servers(1, turn_server);
    allocator_->set_flags(kOnlyLocalPorts);
    allocator_->SetConfiguration(stun_servers, turn_servers, 0 /* pool size */,
                                 webrtc::NO_PRUNE);
    allocator_session_ = allocator_->CreateSession(
        "test", cricket::ICE_CANDIDATE_COMPONENT_RTP, kIceUfrag, kIcePwd);
    // The gathering will take place on the current thread and the following
    // call of StartGettingPorts is blocking. We will not ClearGettingPorts
    // prematurely.
    allocator_session_->StartGettingPorts();
    allocator_session_->SignalIceRegathering.connect(
        this, &RegatheringControllerTest::OnIceRegathering);
    regathering_controller_->set_allocator_session(allocator_session_.get());
  }

  // The regathering controller is initialized with the allocator session
  // cleared. Only after clearing the session, we would be able to regather. See
  // the comments for BasicRegatheringController in regatheringcontroller.h.
  void InitializeAndGatherOnceWithSessionCleared() {
    InitializeAndGatherOnce();
    allocator_session_->ClearGettingPorts();
  }

  void OnIceRegathering(cricket::PortAllocatorSession* allocator_session,
                        cricket::IceRegatheringReason reason) {
    ++count_[reason];
  }

  int GetRegatheringReasonCount(cricket::IceRegatheringReason reason) {
    return count_[reason];
  }

  BasicRegatheringController* regathering_controller() {
    return regathering_controller_.get();
  }

 private:
  webrtc::test::ScopedKeyValueConfig field_trials_;
  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread thread_;
  std::unique_ptr<cricket::IceTransportInternal> ice_transport_;
  std::unique_ptr<BasicRegatheringController> regathering_controller_;
  std::unique_ptr<rtc::PacketSocketFactory> packet_socket_factory_;
  std::unique_ptr<cricket::PortAllocator> allocator_;
  std::unique_ptr<cricket::PortAllocatorSession> allocator_session_;
  std::map<cricket::IceRegatheringReason, int> count_;
};

// Tests that ICE regathering occurs only if the port allocator session is
// cleared. A port allocation session is not cleared if the initial gathering is
// still in progress or the continual gathering is not enabled.
TEST_F(RegatheringControllerTest,
       IceRegatheringDoesNotOccurIfSessionNotCleared) {
  rtc::ScopedFakeClock clock;
  InitializeAndGatherOnce();  // Session not cleared.

  BasicRegatheringController::Config config;
  config.regather_on_failed_networks_interval = 2000;
  regathering_controller()->SetConfig(config);
  regathering_controller()->Start();
  SIMULATED_WAIT(false, 10000, clock);
  // Expect no regathering in the last 10s.
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
}

TEST_F(RegatheringControllerTest, IceRegatheringRepeatsAsScheduled) {
  rtc::ScopedFakeClock clock;
  InitializeAndGatherOnceWithSessionCleared();

  BasicRegatheringController::Config config;
  config.regather_on_failed_networks_interval = 2000;
  regathering_controller()->SetConfig(config);
  regathering_controller()->Start();
  SIMULATED_WAIT(false, 2000 - 1, clock);
  // Expect no regathering.
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  SIMULATED_WAIT(false, 2, clock);
  // Expect regathering on all networks and on failed networks to happen once
  // respectively in that last 2s with 2s interval.
  EXPECT_EQ(1, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  SIMULATED_WAIT(false, 11000, clock);
  // Expect regathering to happen for another 5 times in 11s with 2s interval.
  EXPECT_EQ(6, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
}

// Tests that the schedule of ICE regathering on failed networks can be canceled
// and replaced by a new recurring schedule.
TEST_F(RegatheringControllerTest,
       ScheduleOfIceRegatheringOnFailedNetworksCanBeReplaced) {
  rtc::ScopedFakeClock clock;
  InitializeAndGatherOnceWithSessionCleared();

  BasicRegatheringController::Config config;
  config.regather_on_failed_networks_interval = 2000;
  regathering_controller()->SetConfig(config);
  regathering_controller()->Start();
  config.regather_on_failed_networks_interval = 5000;
  regathering_controller()->SetConfig(config);
  SIMULATED_WAIT(false, 3000, clock);
  // Expect no regathering from the previous schedule.
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  SIMULATED_WAIT(false, 11000 - 3000, clock);
  // Expect regathering to happen twice in the last 11s with 5s interval.
  EXPECT_EQ(2, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
}

}  // namespace webrtc
