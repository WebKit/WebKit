/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "p2p/base/fakeportallocator.h"
#include "p2p/base/portallocator.h"
#include "rtc_base/gunit.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtualsocketserver.h"

static const char kContentName[] = "test content";
// Based on ICE_UFRAG_LENGTH
static const char kIceUfrag[] = "UF00";
// Based on ICE_PWD_LENGTH
static const char kIcePwd[] = "TESTICEPWD00000000000000";
static const char kTurnUsername[] = "test";
static const char kTurnPassword[] = "test";

class PortAllocatorTest : public testing::Test, public sigslot::has_slots<> {
 public:
  PortAllocatorTest()
      : vss_(new rtc::VirtualSocketServer()), main_(vss_.get()) {
    allocator_.reset(
        new cricket::FakePortAllocator(rtc::Thread::Current(), nullptr));
  }

 protected:
  void SetConfigurationWithPoolSize(int candidate_pool_size) {
    EXPECT_TRUE(allocator_->SetConfiguration(
        cricket::ServerAddresses(), std::vector<cricket::RelayServerConfig>(),
        candidate_pool_size, false));
  }

  void SetConfigurationWithPoolSizeExpectFailure(int candidate_pool_size) {
    EXPECT_FALSE(allocator_->SetConfiguration(
        cricket::ServerAddresses(), std::vector<cricket::RelayServerConfig>(),
        candidate_pool_size, false));
  }

  std::unique_ptr<cricket::FakePortAllocatorSession> CreateSession(
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd) {
    return std::unique_ptr<cricket::FakePortAllocatorSession>(
        static_cast<cricket::FakePortAllocatorSession*>(
            allocator_
                ->CreateSession(content_name, component, ice_ufrag, ice_pwd)
                .release()));
  }

  const cricket::FakePortAllocatorSession* GetPooledSession() const {
    return static_cast<const cricket::FakePortAllocatorSession*>(
        allocator_->GetPooledSession());
  }

  std::unique_ptr<cricket::FakePortAllocatorSession> TakePooledSession() {
    return std::unique_ptr<cricket::FakePortAllocatorSession>(
        static_cast<cricket::FakePortAllocatorSession*>(
            allocator_->TakePooledSession(kContentName, 0, kIceUfrag, kIcePwd)
                .release()));
  }

  int GetAllPooledSessionsReturnCount() {
    int count = 0;
    while (TakePooledSession() != nullptr) {
      ++count;
    }
    return count;
  }

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread main_;
  std::unique_ptr<cricket::FakePortAllocator> allocator_;
  rtc::SocketAddress stun_server_1{"11.11.11.11", 3478};
  rtc::SocketAddress stun_server_2{"22.22.22.22", 3478};
  cricket::RelayServerConfig turn_server_1{"11.11.11.11",      3478,
                                           kTurnUsername,      kTurnPassword,
                                           cricket::PROTO_UDP, false};
  cricket::RelayServerConfig turn_server_2{"22.22.22.22",      3478,
                                           kTurnUsername,      kTurnPassword,
                                           cricket::PROTO_UDP, false};
};

TEST_F(PortAllocatorTest, TestDefaults) {
  EXPECT_EQ(0UL, allocator_->stun_servers().size());
  EXPECT_EQ(0UL, allocator_->turn_servers().size());
  EXPECT_EQ(0, allocator_->candidate_pool_size());
  EXPECT_EQ(0, GetAllPooledSessionsReturnCount());
}

// Call CreateSession and verify that the parameters passed in and the
// candidate filter are applied as expected.
TEST_F(PortAllocatorTest, CreateSession) {
  allocator_->set_candidate_filter(cricket::CF_RELAY);
  auto session = CreateSession(kContentName, 1, kIceUfrag, kIcePwd);
  ASSERT_NE(nullptr, session);
  EXPECT_EQ(cricket::CF_RELAY, session->candidate_filter());
  EXPECT_EQ(kContentName, session->content_name());
  EXPECT_EQ(1, session->component());
  EXPECT_EQ(kIceUfrag, session->ice_ufrag());
  EXPECT_EQ(kIcePwd, session->ice_pwd());
}

TEST_F(PortAllocatorTest, SetConfigurationUpdatesIceServers) {
  cricket::ServerAddresses stun_servers_1 = {stun_server_1};
  std::vector<cricket::RelayServerConfig> turn_servers_1 = {turn_server_1};
  EXPECT_TRUE(
      allocator_->SetConfiguration(stun_servers_1, turn_servers_1, 0, false));
  EXPECT_EQ(stun_servers_1, allocator_->stun_servers());
  EXPECT_EQ(turn_servers_1, allocator_->turn_servers());

  // Update with a different set of servers.
  cricket::ServerAddresses stun_servers_2 = {stun_server_2};
  std::vector<cricket::RelayServerConfig> turn_servers_2 = {turn_server_2};
  EXPECT_TRUE(
      allocator_->SetConfiguration(stun_servers_2, turn_servers_2, 0, false));
  EXPECT_EQ(stun_servers_2, allocator_->stun_servers());
  EXPECT_EQ(turn_servers_2, allocator_->turn_servers());
}

TEST_F(PortAllocatorTest, SetConfigurationUpdatesCandidatePoolSize) {
  SetConfigurationWithPoolSize(2);
  EXPECT_EQ(2, allocator_->candidate_pool_size());
  SetConfigurationWithPoolSize(3);
  EXPECT_EQ(3, allocator_->candidate_pool_size());
  SetConfigurationWithPoolSize(1);
  EXPECT_EQ(1, allocator_->candidate_pool_size());
  SetConfigurationWithPoolSize(4);
  EXPECT_EQ(4, allocator_->candidate_pool_size());
}

// A negative pool size should just be treated as zero.
TEST_F(PortAllocatorTest, SetConfigurationWithNegativePoolSizeFails) {
  SetConfigurationWithPoolSizeExpectFailure(-1);
}

// Test that if the candidate pool size is nonzero, pooled sessions are
// created, and StartGettingPorts is called on them.
TEST_F(PortAllocatorTest, SetConfigurationCreatesPooledSessions) {
  SetConfigurationWithPoolSize(2);
  auto session_1 = TakePooledSession();
  auto session_2 = TakePooledSession();
  ASSERT_NE(nullptr, session_1.get());
  ASSERT_NE(nullptr, session_2.get());
  EXPECT_EQ(1, session_1->port_config_count());
  EXPECT_EQ(1, session_2->port_config_count());
  EXPECT_EQ(0, GetAllPooledSessionsReturnCount());
}

// Test that if the candidate pool size is increased, pooled sessions are
// created as necessary.
TEST_F(PortAllocatorTest, SetConfigurationCreatesMorePooledSessions) {
  SetConfigurationWithPoolSize(1);
  SetConfigurationWithPoolSize(2);
  EXPECT_EQ(2, GetAllPooledSessionsReturnCount());
}

// Test that if the candidate pool size is reduced, extra sessions are
// destroyed.
TEST_F(PortAllocatorTest, SetConfigurationDestroysPooledSessions) {
  SetConfigurationWithPoolSize(2);
  SetConfigurationWithPoolSize(1);
  EXPECT_EQ(1, GetAllPooledSessionsReturnCount());
}

// According to JSEP, existing pooled sessions should be destroyed and new
// ones created when the ICE servers change.
TEST_F(PortAllocatorTest,
       SetConfigurationRecreatesPooledSessionsWhenIceServersChange) {
  cricket::ServerAddresses stun_servers_1 = {stun_server_1};
  std::vector<cricket::RelayServerConfig> turn_servers_1 = {turn_server_1};
  allocator_->SetConfiguration(stun_servers_1, turn_servers_1, 1, false);
  EXPECT_EQ(stun_servers_1, allocator_->stun_servers());
  EXPECT_EQ(turn_servers_1, allocator_->turn_servers());

  // Update with a different set of servers (and also change pool size).
  cricket::ServerAddresses stun_servers_2 = {stun_server_2};
  std::vector<cricket::RelayServerConfig> turn_servers_2 = {turn_server_2};
  allocator_->SetConfiguration(stun_servers_2, turn_servers_2, 2, false);
  EXPECT_EQ(stun_servers_2, allocator_->stun_servers());
  EXPECT_EQ(turn_servers_2, allocator_->turn_servers());
  auto session_1 = TakePooledSession();
  auto session_2 = TakePooledSession();
  ASSERT_NE(nullptr, session_1.get());
  ASSERT_NE(nullptr, session_2.get());
  EXPECT_EQ(stun_servers_2, session_1->stun_servers());
  EXPECT_EQ(turn_servers_2, session_1->turn_servers());
  EXPECT_EQ(stun_servers_2, session_2->stun_servers());
  EXPECT_EQ(turn_servers_2, session_2->turn_servers());
  EXPECT_EQ(0, GetAllPooledSessionsReturnCount());
}

// According to JSEP, after SetLocalDescription, setting different ICE servers
// will not cause the pool to be refilled. This is implemented by the
// PeerConnection calling FreezeCandidatePool when a local description is set.
TEST_F(PortAllocatorTest,
       SetConfigurationDoesNotRecreatePooledSessionsAfterFreezeCandidatePool) {
  cricket::ServerAddresses stun_servers_1 = {stun_server_1};
  std::vector<cricket::RelayServerConfig> turn_servers_1 = {turn_server_1};
  allocator_->SetConfiguration(stun_servers_1, turn_servers_1, 1, false);
  EXPECT_EQ(stun_servers_1, allocator_->stun_servers());
  EXPECT_EQ(turn_servers_1, allocator_->turn_servers());

  // Update with a different set of servers, but first freeze the pool.
  allocator_->FreezeCandidatePool();
  cricket::ServerAddresses stun_servers_2 = {stun_server_2};
  std::vector<cricket::RelayServerConfig> turn_servers_2 = {turn_server_2};
  allocator_->SetConfiguration(stun_servers_2, turn_servers_2, 2, false);
  EXPECT_EQ(stun_servers_2, allocator_->stun_servers());
  EXPECT_EQ(turn_servers_2, allocator_->turn_servers());
  auto session = TakePooledSession();
  ASSERT_NE(nullptr, session.get());
  EXPECT_EQ(stun_servers_1, session->stun_servers());
  EXPECT_EQ(turn_servers_1, session->turn_servers());
  EXPECT_EQ(0, GetAllPooledSessionsReturnCount());
}

TEST_F(PortAllocatorTest, GetPooledSessionReturnsNextSession) {
  SetConfigurationWithPoolSize(2);
  auto peeked_session_1 = GetPooledSession();
  auto session_1 = TakePooledSession();
  EXPECT_EQ(session_1.get(), peeked_session_1);
  auto peeked_session_2 = GetPooledSession();
  auto session_2 = TakePooledSession();
  EXPECT_EQ(session_2.get(), peeked_session_2);
}

// Verify that subclasses of PortAllocatorSession are given a chance to update
// ICE parameters when TakePooledSession is called, and the base class updates
// the info itself.
TEST_F(PortAllocatorTest, TakePooledSessionUpdatesIceParameters) {
  SetConfigurationWithPoolSize(1);
  auto peeked_session = GetPooledSession();
  ASSERT_NE(nullptr, peeked_session);
  EXPECT_EQ(0, peeked_session->transport_info_update_count());
  std::unique_ptr<cricket::FakePortAllocatorSession> session(
      static_cast<cricket::FakePortAllocatorSession*>(
          allocator_->TakePooledSession(kContentName, 1, kIceUfrag, kIcePwd)
              .release()));
  EXPECT_EQ(1, session->transport_info_update_count());
  EXPECT_EQ(kContentName, session->content_name());
  EXPECT_EQ(1, session->component());
  EXPECT_EQ(kIceUfrag, session->ice_ufrag());
  EXPECT_EQ(kIcePwd, session->ice_pwd());
}

// According to JSEP, candidate filtering should be done when the pooled
// candidates are surfaced to the application. This means when a pooled
// session is taken. So a pooled session should gather candidates
// unfiltered until it's returned by TakePooledSession.
TEST_F(PortAllocatorTest, TakePooledSessionUpdatesCandidateFilter) {
  allocator_->set_candidate_filter(cricket::CF_RELAY);
  SetConfigurationWithPoolSize(1);
  auto peeked_session = GetPooledSession();
  ASSERT_NE(nullptr, peeked_session);
  EXPECT_EQ(cricket::CF_ALL, peeked_session->candidate_filter());
  auto session = TakePooledSession();
  EXPECT_EQ(cricket::CF_RELAY, session->candidate_filter());
}

// Verify that after DiscardCandidatePool, TakePooledSession doesn't return
// anything.
TEST_F(PortAllocatorTest, DiscardCandidatePool) {
  SetConfigurationWithPoolSize(1);
  allocator_->DiscardCandidatePool();
  EXPECT_EQ(0, GetAllPooledSessionsReturnCount());
}

TEST_F(PortAllocatorTest, RestrictIceCredentialsChange) {
  SetConfigurationWithPoolSize(1);
  EXPECT_EQ(1, GetAllPooledSessionsReturnCount());
  allocator_->DiscardCandidatePool();

  // Only return pooled sessions with the ice credentials that
  // match those requested in TakePooledSession().
  allocator_->set_restrict_ice_credentials_change(true);
  SetConfigurationWithPoolSize(1);
  EXPECT_EQ(0, GetAllPooledSessionsReturnCount());
  allocator_->DiscardCandidatePool();

  SetConfigurationWithPoolSize(1);
  auto credentials = allocator_->GetPooledIceCredentials();
  ASSERT_EQ(1u, credentials.size());
  EXPECT_EQ(nullptr,
            allocator_->TakePooledSession(kContentName, 0, kIceUfrag, kIcePwd));
  EXPECT_NE(nullptr,
            allocator_->TakePooledSession(kContentName, 0, credentials[0].ufrag,
                                          credentials[0].pwd));
  EXPECT_EQ(nullptr,
            allocator_->TakePooledSession(kContentName, 0, credentials[0].ufrag,
                                          credentials[0].pwd));
  allocator_->DiscardCandidatePool();
}
