/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdint>
#include <ctime>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include "absl/flags/flag.h"
#include "api/candidate.h"
#include "api/crypto/crypto_options.h"
#include "api/dtls_transport_interface.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/field_trials.h"
#include "api/scoped_refptr.h"
#include "api/test/create_network_emulation_manager.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/rtc_error_matchers.h"
#include "api/test/simulated_network.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "p2p/base/connection_info.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/p2p_transport_channel.h"
#include "p2p/base/port_allocator.h"
#include "p2p/base/transport_description.h"
#include "p2p/client/basic_port_allocator.h"
#include "p2p/dtls/dtls_transport.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/fake_network.h"
#include "rtc_base/logging.h"
#include "rtc_base/network.h"
#include "rtc_base/random.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/ssl_fingerprint.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/create_test_field_trials.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

ABSL_FLAG(int32_t,
          long_running_seed,
          7788,
          "0 means use time(0) as seed (i.e non deterministic)");
ABSL_FLAG(int32_t, long_running_run_time_minutes, 7, "");
ABSL_FLAG(bool, long_running_send_data, false, "");

namespace {
constexpr int kDefaultTimeout = 30000;
}  // namespace

namespace webrtc {

using ::testing::Eq;
using ::testing::IsTrue;
using ::testing::Not;

class DtlsIceIntegrationTest : public ::testing::TestWithParam<std::tuple<
                                   /* 0 client_piggyback= */ bool,
                                   /* 1 server_piggyback= */ bool,
                                   SSLProtocolVersion,
                                   /* 3 client_dtls_is_ice_controlling= */ bool,
                                   /* 4 client_pqc= */ bool,
                                   /* 5 server_pqc= */ bool>>,
                               public sigslot::has_slots<> {
 public:
  void CandidateC2S(IceTransportInternal*, const Candidate& c) {
    server_thread()->PostTask(
        [this, c = c]() { server_.ice->AddRemoteCandidate(c); });
  }
  void CandidateS2C(IceTransportInternal*, const Candidate& c) {
    client_thread()->PostTask(
        [this, c = c]() { client_.ice->AddRemoteCandidate(c); });
  }

 private:
  struct Endpoint {
    explicit Endpoint(bool dtls_in_stun, bool pqc_)
        : env(CreateEnvironment(CreateTestFieldTrialsPtr(
              dtls_in_stun ? "WebRTC-IceHandshakeDtls/Enabled/" : ""))),
          dtls_stun_piggyback(dtls_in_stun),
          pqc(pqc_) {}

    EmulatedNetworkManagerInterface* emulated_network_manager = nullptr;
    std::unique_ptr<NetworkManager> network_manager;
    std::unique_ptr<BasicPacketSocketFactory> packet_socket_factory;
    std::unique_ptr<PortAllocator> allocator;
    std::unique_ptr<IceTransportInternal> ice;
    std::unique_ptr<DtlsTransportInternalImpl> dtls;

    // SetRemoteFingerprintFromCert does not actually set the fingerprint,
    // but only store it for setting later.
    bool store_but_dont_set_remote_fingerprint = false;
    std::unique_ptr<SSLFingerprint> remote_fingerprint;

    Environment env;
    bool dtls_stun_piggyback;
    bool pqc;
  };

 protected:
  DtlsIceIntegrationTest()
      : ss_(std::make_unique<VirtualSocketServer>()),
        socket_factory_(std::make_unique<BasicPacketSocketFactory>(ss_.get())),
        client_(std::get<0>(GetParam()),
                std::get<2>(GetParam()) == SSL_PROTOCOL_DTLS_13 &&
                    std::get<4>(GetParam())),
        server_(std::get<1>(GetParam()),
                std::get<2>(GetParam()) == SSL_PROTOCOL_DTLS_13 &&
                    std::get<5>(GetParam())),
        client_ice_parameters_("c_ufrag",
                               "c_icepwd_something_something",
                               false),
        server_ice_parameters_("s_ufrag",
                               "s_icepwd_something_something",
                               false) {}

  void ConfigureEmulatedNetwork() {
    network_emulation_manager_ =
        CreateNetworkEmulationManager({.time_mode = TimeMode::kSimulated});

    BuiltInNetworkBehaviorConfig networkBehavior;
    networkBehavior.link_capacity = DataRate::KilobitsPerSec(220);
    networkBehavior.queue_delay_ms = 100;
    networkBehavior.queue_length_packets = 30;
    networkBehavior.loss_percent = 50;

    auto pair = network_emulation_manager_->CreateEndpointPairWithTwoWayRoutes(
        networkBehavior);

    client_.emulated_network_manager = pair.first;
    server_.emulated_network_manager = pair.second;
  }

  void SetupEndpoint(Endpoint& ep,
                     bool client,
                     const scoped_refptr<RTCCertificate> client_certificate,
                     const scoped_refptr<RTCCertificate> server_certificate) {
    thread(ep)->BlockingCall([&]() {
      if (!network_manager_) {
        network_manager_ =
            std::make_unique<FakeNetworkManager>(Thread::Current());
      }
      if (network_emulation_manager_ == nullptr) {
        ep.allocator = std::make_unique<BasicPortAllocator>(
            ep.env, network_manager_.get(), socket_factory_.get());
      } else {
        ep.network_manager =
            ep.emulated_network_manager->ReleaseNetworkManager();
        ep.packet_socket_factory = std::make_unique<BasicPacketSocketFactory>(
            ep.emulated_network_manager->socket_factory());
        ep.allocator = std::make_unique<BasicPortAllocator>(
            ep.env, ep.network_manager.get(), ep.packet_socket_factory.get());
      }
      ep.allocator->set_flags(ep.allocator->flags() |
                              PORTALLOCATOR_DISABLE_TCP);
      ep.ice = std::make_unique<P2PTransportChannel>(
          ep.env, client ? "client_transport" : "server_transport", 0,
          ep.allocator.get());
      CryptoOptions crypto_options;
      if (ep.pqc) {
        FieldTrials field_trials("WebRTC-EnableDtlsPqc/Enabled/");
        crypto_options.ephemeral_key_exchange_cipher_groups.Update(
            &field_trials);
      }
      ep.dtls = std::make_unique<DtlsTransportInternalImpl>(
          ep.env, ep.ice.get(), crypto_options, std::get<2>(GetParam()));

      // Enable(or disable) the dtls_in_stun parameter before
      // DTLS is negotiated.
      IceConfig config;
      config.continual_gathering_policy = GATHER_CONTINUALLY;
      config.dtls_handshake_in_stun = ep.dtls_stun_piggyback;
      ep.ice->SetIceConfig(config);

      // Setup ICE.
      ep.ice->SetIceParameters(client ? client_ice_parameters_
                                      : server_ice_parameters_);
      ep.ice->SetRemoteIceParameters(client ? server_ice_parameters_
                                            : client_ice_parameters_);
      if (client) {
        ep.ice->SetIceRole(std::get<3>(GetParam()) ? ICEROLE_CONTROLLED
                                                   : ICEROLE_CONTROLLING);
      } else {
        ep.ice->SetIceRole(std::get<3>(GetParam()) ? ICEROLE_CONTROLLING
                                                   : ICEROLE_CONTROLLED);
      }
      if (client) {
        ep.ice->SubscribeCandidateGathered(
            [this](IceTransportInternal* transport,
                   const Candidate& candidate) {
              CandidateC2S(transport, candidate);
            });
      } else {
        ep.ice->SubscribeCandidateGathered(
            [this](IceTransportInternal* transport,
                   const Candidate& candidate) {
              CandidateS2C(transport, candidate);
            });
      }

      // Setup DTLS.
      ep.dtls->SetDtlsRole(client ? SSL_SERVER : SSL_CLIENT);
      SetLocalCertificate(ep, client ? client_certificate : server_certificate);
      SetRemoteFingerprintFromCert(
          ep, client ? server_certificate : client_certificate);
    });
  }

  void Prepare() {
    auto client_certificate =
        RTCCertificate::Create(SSLIdentity::Create("test", KT_DEFAULT));
    auto server_certificate =
        RTCCertificate::Create(SSLIdentity::Create("test", KT_DEFAULT));

    if (network_emulation_manager_ == nullptr) {
      thread_ = std::make_unique<AutoSocketServerThread>(ss_.get());
    }

    client_thread()->BlockingCall([&]() {
      SetupEndpoint(client_, /* client= */ true, client_certificate,
                    server_certificate);
    });

    server_thread()->BlockingCall([&]() {
      SetupEndpoint(server_, /* client= */ false, client_certificate,
                    server_certificate);
    });

    // Setup the network.
    if (network_emulation_manager_ == nullptr) {
      network_manager_->AddInterface(SocketAddress("192.168.1.1", 0));
    }

    client_thread()->BlockingCall([&]() { client_.allocator->Initialize(); });
    server_thread()->BlockingCall([&]() { server_.allocator->Initialize(); });
  }

  void TearDown() override {
    client_thread()->BlockingCall([&]() {
      client_.dtls.reset();
      client_.ice.reset();
      client_.allocator.reset();
    });

    server_thread()->BlockingCall([&]() {
      server_.dtls.reset();
      server_.ice.reset();
      server_.allocator.reset();
    });
  }

  ~DtlsIceIntegrationTest() override = default;

  static int CountConnectionsWithFilter(
      IceTransportInternal* ice,
      std::function<bool(const ConnectionInfo&)> filter) {
    IceTransportStats stats;
    ice->GetStats(&stats);
    int count = 0;
    for (const auto& con : stats.connection_infos) {
      if (filter(con)) {
        count++;
      }
    }
    return count;
  }

  static int CountConnections(IceTransportInternal* ice) {
    return CountConnectionsWithFilter(ice, [](auto con) { return true; });
  }

  static int CountWritableConnections(IceTransportInternal* ice) {
    return CountConnectionsWithFilter(ice,
                                      [](auto con) { return con.writable; });
  }

  WaitUntilSettings wait_until_settings() {
    if (network_emulation_manager_ == nullptr) {
      return {
          .timeout = TimeDelta::Millis(kDefaultTimeout),
          .clock = &fake_clock_,
      };
    } else {
      return {
          .timeout = TimeDelta::Millis(kDefaultTimeout),
          .clock = network_emulation_manager_->time_controller(),
      };
    }
  }

  Thread* thread(Endpoint& ep) {
    if (ep.emulated_network_manager == nullptr) {
      return thread_.get();
    } else {
      return ep.emulated_network_manager->network_thread();
    }
  }

  Thread* client_thread() { return thread(client_); }

  Thread* server_thread() { return thread(server_); }

  void SetRemoteFingerprintFromCert(Endpoint& ep,
                                    const scoped_refptr<RTCCertificate>& cert) {
    ep.remote_fingerprint = SSLFingerprint::CreateFromCertificate(*cert);
    if (ep.store_but_dont_set_remote_fingerprint) {
      return;
    }
    SetRemoteFingerprint(ep);
  }

  void SetRemoteFingerprint(Endpoint& ep) {
    RTC_CHECK(ep.remote_fingerprint);
    RTC_LOG(LS_INFO) << ((&ep == &client_) ? "client" : "server")
                     << "::SetRemoteFingerprint";
    ep.dtls->SetRemoteParameters(
        ep.remote_fingerprint->algorithm,
        reinterpret_cast<const uint8_t*>(ep.remote_fingerprint->digest.data()),
        ep.remote_fingerprint->digest.size(), std::nullopt);
  }

  void SetLocalCertificate(Endpoint& ep,
                           const scoped_refptr<RTCCertificate> certificate) {
    RTC_CHECK(certificate);
    RTC_LOG(LS_INFO) << ((&ep == &client_) ? "client" : "server")
                     << "::SetLocalCertificate: ";
    ep.dtls->SetLocalCertificate(certificate);
  }

  ScopedFakeClock fake_clock_;
  std::unique_ptr<VirtualSocketServer> ss_;
  std::unique_ptr<BasicPacketSocketFactory> socket_factory_;
  std::unique_ptr<NetworkEmulationManager> network_emulation_manager_;
  std::unique_ptr<AutoSocketServerThread> thread_;
  std::unique_ptr<FakeNetworkManager> network_manager_;

  Endpoint client_;
  Endpoint server_;

  IceParameters client_ice_parameters_;
  IceParameters server_ice_parameters_;
};

TEST_P(DtlsIceIntegrationTest, SmokeTest) {
  Prepare();
  client_.ice->MaybeStartGathering();
  server_.ice->MaybeStartGathering();

  // Note: this only reaches the pending piggybacking state.
  EXPECT_THAT(
      WaitUntil(
          [&] { return client_.dtls->writable() && server_.dtls->writable(); },
          IsTrue(), wait_until_settings()),
      IsRtcOk());
  EXPECT_EQ(client_.dtls->IsDtlsPiggybackSupportedByPeer(),
            client_.dtls_stun_piggyback && server_.dtls_stun_piggyback);
  EXPECT_EQ(server_.dtls->IsDtlsPiggybackSupportedByPeer(),
            client_.dtls_stun_piggyback && server_.dtls_stun_piggyback);
  EXPECT_EQ(client_.dtls->WasDtlsCompletedByPiggybacking(),
            client_.dtls_stun_piggyback && server_.dtls_stun_piggyback);
  EXPECT_EQ(server_.dtls->WasDtlsCompletedByPiggybacking(),
            client_.dtls_stun_piggyback && server_.dtls_stun_piggyback);

  if (!(client_.pqc || server_.pqc) && client_.dtls_stun_piggyback &&
      server_.dtls_stun_piggyback) {
    EXPECT_EQ(client_.dtls->GetStunDataCount(), 2);
    EXPECT_EQ(server_.dtls->GetStunDataCount(), 1);
  } else {
    // TODO(webrtc:404763475)
  }

  if ((client_.pqc || server_.pqc) &&
      !(client_.dtls_stun_piggyback && server_.dtls_stun_piggyback)) {
    // TODO(webrtc:404763475) : The retransmissions is due to early
    // client hello and the code only saves 1 packet.
  } else {
    EXPECT_EQ(client_.dtls->GetRetransmissionCount(), 0);
    EXPECT_EQ(server_.dtls->GetRetransmissionCount(), 0);
  }

  // Validate that we can add new Connections (that become writable).
  network_manager_->AddInterface(SocketAddress("192.168.2.1", 0));
  EXPECT_THAT(WaitUntil(
                  [&] {
                    return CountWritableConnections(client_.ice.get()) > 1 &&
                           CountWritableConnections(server_.ice.get()) > 1;
                  },
                  IsTrue(), wait_until_settings()),
              IsRtcOk());
}

// Check that DtlsInStun still works even if SetRemoteFingerprint is called
// "late". This is what happens if the answer sdp comes strictly after ICE has
// connected. Before this patch, this would disable stun-piggy-backing.
TEST_P(DtlsIceIntegrationTest, ClientLateCertificate) {
  client_.store_but_dont_set_remote_fingerprint = true;
  Prepare();
  client_.ice->MaybeStartGathering();
  server_.ice->MaybeStartGathering();

  ASSERT_THAT(
      WaitUntil([&] { return CountWritableConnections(client_.ice.get()) > 0; },
                IsTrue(), wait_until_settings()),
      IsRtcOk());
  SetRemoteFingerprint(client_);

  ASSERT_THAT(
      WaitUntil(
          [&] { return client_.dtls->writable() && server_.dtls->writable(); },
          IsTrue(), wait_until_settings()),
      IsRtcOk());

  EXPECT_EQ(client_.dtls->IsDtlsPiggybackSupportedByPeer(),
            client_.dtls_stun_piggyback && server_.dtls_stun_piggyback);

  EXPECT_EQ(client_.dtls->WasDtlsCompletedByPiggybacking(),
            client_.dtls_stun_piggyback && server_.dtls_stun_piggyback);
  EXPECT_EQ(server_.dtls->WasDtlsCompletedByPiggybacking(),
            client_.dtls_stun_piggyback && server_.dtls_stun_piggyback);

  if ((client_.pqc || server_.pqc) &&
      !(client_.dtls_stun_piggyback && server_.dtls_stun_piggyback)) {
    // TODO(webrtc:404763475) : The retransmissions is due to early
    // client hello and the code only saves 1 packet.
  } else {
    EXPECT_EQ(client_.dtls->GetRetransmissionCount(), 0);
    EXPECT_EQ(server_.dtls->GetRetransmissionCount(), 0);
  }
}

TEST_P(DtlsIceIntegrationTest, TestWithPacketLoss) {
  if (!SSLStreamAdapter::IsBoringSsl()) {
    GTEST_SKIP() << "Needs boringssl.";
  }
  ConfigureEmulatedNetwork();
  Prepare();

  client_thread()->PostTask([&]() { client_.ice->MaybeStartGathering(); });

  server_thread()->PostTask([&]() { server_.ice->MaybeStartGathering(); });

  EXPECT_THAT(WaitUntil(
                  [&] {
                    return client_thread()->BlockingCall([&]() {
                      return client_.dtls->writable();
                    }) && server_thread()->BlockingCall([&]() {
                      return server_.dtls->writable();
                    });
                  },
                  IsTrue(), wait_until_settings()),
              IsRtcOk());

  EXPECT_EQ(client_thread()->BlockingCall([&]() {
    return client_.dtls->IsDtlsPiggybackSupportedByPeer();
  }),
            client_.dtls_stun_piggyback && server_.dtls_stun_piggyback);
  EXPECT_EQ(server_thread()->BlockingCall([&]() {
    return server_.dtls->IsDtlsPiggybackSupportedByPeer();
  }),
            client_.dtls_stun_piggyback && server_.dtls_stun_piggyback);
}

TEST_P(DtlsIceIntegrationTest, LongRunningTestWithPacketLoss) {
  if (!SSLStreamAdapter::IsBoringSsl()) {
    GTEST_SKIP() << "Needs boringssl.";
  }
  int seed = absl::GetFlag(FLAGS_long_running_seed);
  if (seed == 0) {
    seed = 1 + time(0);
  }
  RTC_LOG(LS_INFO) << "seed: " << seed;
  webrtc::Random rand(seed);
  ConfigureEmulatedNetwork();
  Prepare();

  client_thread()->PostTask([&]() { client_.ice->MaybeStartGathering(); });

  server_thread()->PostTask([&]() { server_.ice->MaybeStartGathering(); });

  ASSERT_THAT(WaitUntil(
                  [&] {
                    return client_thread()->BlockingCall([&]() {
                      return client_.dtls->writable();
                    }) && server_thread()->BlockingCall([&]() {
                      return server_.dtls->writable();
                    });
                  },
                  IsTrue(), wait_until_settings()),
              IsRtcOk());

  auto now =
      network_emulation_manager_->time_controller()->GetClock()->CurrentTime();
  auto end = now + TimeDelta::Minutes(
                       absl::GetFlag(FLAGS_long_running_run_time_minutes));
  int client_sent = 0;
  int client_recv = 0;
  int server_sent = 0;
  int server_recv = 0;
  void* id = this;
  client_thread()->BlockingCall([&]() {
    return client_.dtls->RegisterReceivedPacketCallback(
        id, [&](auto, auto) { client_recv++; });
  });
  server_thread()->BlockingCall([&]() {
    return server_.dtls->RegisterReceivedPacketCallback(
        id, [&](auto, auto) { server_recv++; });
  });
  while (now < end) {
    int delay = static_cast<int>(rand.Gaussian(100, 25));
    if (delay < 25) {
      delay = 25;
    }
    network_emulation_manager_->time_controller()->AdvanceTime(
        TimeDelta::Millis(delay));
    now = network_emulation_manager_->time_controller()
              ->GetClock()
              ->CurrentTime();

    if (absl::GetFlag(FLAGS_long_running_send_data)) {
      int flags = 0;
      AsyncSocketPacketOptions options;
      std::string a_long_string(500, 'a');
      if (client_thread()->BlockingCall([&]() {
            return client_.dtls->SendPacket(
                a_long_string.c_str(), a_long_string.length(), options, flags);
          }) > 0) {
        client_sent++;
      }
      if (server_thread()->BlockingCall([&]() {
            return server_.dtls->SendPacket(
                a_long_string.c_str(), a_long_string.length(), options, flags);
          }) > 0) {
        server_sent++;
      }
    }

    EXPECT_THAT(WaitUntil(
                    [&] {
                      return client_thread()->BlockingCall([&]() {
                        return client_.dtls->writable();
                      }) && server_thread()->BlockingCall([&]() {
                        return server_.dtls->writable();
                      });
                    },
                    IsTrue(), wait_until_settings()),
                IsRtcOk());
    ASSERT_THAT(client_thread()->BlockingCall(
                    [&]() { return client_.dtls->dtls_state(); }),
                Not(Eq(DtlsTransportState::kFailed)));
    ASSERT_THAT(server_thread()->BlockingCall(
                    [&]() { return server_.dtls->dtls_state(); }),
                Not(Eq(DtlsTransportState::kFailed)));
  }

  client_thread()->BlockingCall(
      [&]() { return client_.dtls->DeregisterReceivedPacketCallback(id); });
  server_thread()->BlockingCall(
      [&]() { return server_.dtls->DeregisterReceivedPacketCallback(id); });

  RTC_LOG(LS_INFO) << "Server sent " << server_sent << " packets "
                   << " client received: " << client_recv << " ("
                   << (client_recv * 100 / (1 + server_sent)) << "%)";
  RTC_LOG(LS_INFO) << "Client sent " << client_sent << " packets "
                   << " server received: " << server_recv << " ("
                   << (server_recv * 100 / (1 + client_sent)) << "%)";
}

// Verify that DtlsStunPiggybacking works even if one (or several)
// of the STUN_BINDING_REQUESTs are so full that dtls does not fit.
TEST_P(DtlsIceIntegrationTest, AlmostFullSTUN_BINDING) {
  Prepare();

  std::string a_long_string(500, 'a');
  client_.ice->GetDictionaryWriter()->get().SetByteString(77)->CopyBytes(
      a_long_string);
  server_.ice->GetDictionaryWriter()->get().SetByteString(78)->CopyBytes(
      a_long_string);

  client_.ice->MaybeStartGathering();
  server_.ice->MaybeStartGathering();

  // Note: this only reaches the pending piggybacking state.
  EXPECT_THAT(
      WaitUntil(
          [&] { return client_.dtls->writable() && server_.dtls->writable(); },
          IsTrue(), wait_until_settings()),
      IsRtcOk());
  EXPECT_EQ(client_.dtls->IsDtlsPiggybackSupportedByPeer(),
            client_.dtls_stun_piggyback && server_.dtls_stun_piggyback);
  EXPECT_EQ(server_.dtls->IsDtlsPiggybackSupportedByPeer(),
            client_.dtls_stun_piggyback && server_.dtls_stun_piggyback);
  EXPECT_EQ(client_.dtls->WasDtlsCompletedByPiggybacking(),
            client_.dtls_stun_piggyback && server_.dtls_stun_piggyback);
  EXPECT_EQ(server_.dtls->WasDtlsCompletedByPiggybacking(),
            client_.dtls_stun_piggyback && server_.dtls_stun_piggyback);

  if (!(client_.pqc || server_.pqc) && client_.dtls_stun_piggyback &&
      server_.dtls_stun_piggyback) {
    EXPECT_EQ(client_.dtls->GetStunDataCount(), 2);
    EXPECT_EQ(server_.dtls->GetStunDataCount(), 1);
  } else {
    // TODO(webrtc:404763475)
  }

  if ((client_.pqc || server_.pqc) &&
      !(client_.dtls_stun_piggyback && server_.dtls_stun_piggyback)) {
    // TODO(webrtc:404763475) : The retransmissions is due to early
    // client hello and the code only saves 1 packet.
  } else {
    EXPECT_EQ(client_.dtls->GetRetransmissionCount(), 0);
    EXPECT_EQ(server_.dtls->GetRetransmissionCount(), 0);
  }
}

// Test cases are parametrized by
// * client-piggybacking-enabled,
// * server-piggybacking-enabled,
// * maximum DTLS version to use.
INSTANTIATE_TEST_SUITE_P(
    DtlsStunPiggybackingIntegrationTest,
    DtlsIceIntegrationTest,
    ::testing::Combine(testing::Bool(),
                       testing::Bool(),
                       testing::Values(SSL_PROTOCOL_DTLS_12,
                                       SSL_PROTOCOL_DTLS_13),
                       testing::Bool(),
                       testing::Bool(),
                       testing::Bool()));

}  // namespace webrtc
