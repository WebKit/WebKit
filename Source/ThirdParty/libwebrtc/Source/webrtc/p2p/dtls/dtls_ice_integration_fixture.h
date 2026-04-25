#ifndef P2P_DTLS_DTLS_ICE_INTEGRATION_FIXTURE_H_
#define P2P_DTLS_DTLS_ICE_INTEGRATION_FIXTURE_H_
/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "api/candidate.h"
#include "api/crypto/crypto_options.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/field_trials.h"
#include "api/ice_transport_interface.h"
#include "api/make_ref_counted.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/scoped_refptr.h"
#include "api/test/create_network_emulation_manager.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/simulated_network.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/p2p_transport_channel.h"
#include "p2p/base/port_allocator.h"
#include "p2p/base/transport_description.h"
#include "p2p/client/basic_port_allocator.h"
#include "p2p/dtls/dtls_transport.h"
#include "p2p/test/fake_ice_lite_agent.h"
#include "p2p/test/fake_ice_transport.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/fake_network.h"
#include "rtc_base/logging.h"
#include "rtc_base/network.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/ssl_fingerprint.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/create_test_field_trials.h"
#include "test/wait_until.h"

namespace webrtc {
namespace dtls_ice_integration_fixture {

constexpr int kDefaultTimeout = 30000;

struct EndpointConfig {
  SSLProtocolVersion max_protocol_version;
  IceRole ice_role;
  SSLRole ssl_role;
  bool ice_lite = false;
  bool dtls_in_stun = false;
  bool pqc = false;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const EndpointConfig& config) {
    sink.Append("[ dtls: ");
    sink.Append(config.ssl_role == SSL_SERVER ? "server/" : "client/");
    switch (config.max_protocol_version) {
      case SSL_PROTOCOL_DTLS_10:
        sink.Append("1.0");
        break;
      case SSL_PROTOCOL_DTLS_12:
        sink.Append("1.2");
        break;
      case SSL_PROTOCOL_DTLS_13:
        sink.Append("1.3");
        break;
      default:
        sink.Append("<unknown>");
        break;
    }
    if (config.ice_role == ICEROLE_CONTROLLED) {
      if (config.ice_lite) {
        sink.Append(" ice: lite");
      } else {
        sink.Append(" ice: controlled");
      }
    }
    absl::Format(&sink, " pqc: %u dtls_in_stun: %u ", config.pqc,
                 config.dtls_in_stun);
    sink.Append(" ]");
  }
};

struct TestConfig {
  int pct_loss = -1;
  int client_interface_count = -1;
  int server_interface_count = -1;

  bool ice_lite = false;
  bool client_ssl_client = true;
  SSLProtocolVersion protocol_version;

  // Configuration for the endpoint acting as the ICE controllering.
  EndpointConfig client_config;
  // Configuration for the endpoint acting as the ICE controlled.
  EndpointConfig server_config;

  TestConfig& fix() {
    client_config.ice_role = ICEROLE_CONTROLLING;
    server_config.ice_role = ICEROLE_CONTROLLED;
    client_config.ice_lite = ice_lite;
    server_config.ice_lite = ice_lite;
    if (client_ssl_client) {
      client_config.ssl_role = SSL_CLIENT;
      server_config.ssl_role = SSL_SERVER;
    } else {
      client_config.ssl_role = SSL_SERVER;
      server_config.ssl_role = SSL_CLIENT;
    }
    client_config.max_protocol_version = protocol_version;
    server_config.max_protocol_version = protocol_version;
    return *this;
  }

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const TestConfig& config) {
    if (config.pct_loss >= 0) {
      absl::Format(&sink, "loss: %u ", config.pct_loss);
    }
    if (config.client_interface_count > 1) {
      absl::Format(&sink, "client_interface_count: %u ",
                   config.client_interface_count);
    }
    if (config.server_interface_count > 1) {
      absl::Format(&sink, "server_interface_count: %u ",
                   config.server_interface_count);
    }
    sink.Append("[ client: ");
    AbslStringify(sink, config.client_config);
    sink.Append("[ server: ");
    AbslStringify(sink, config.server_config);
    sink.Append("]");
  }

  static constexpr EndpointConfig kEndpointVariants[] = {
      {
          .dtls_in_stun = false,
          .pqc = false,
      },
      {
          .dtls_in_stun = true,
          .pqc = false,
      },
      {
          .dtls_in_stun = false,
          .pqc = true,
      },
      {
          .dtls_in_stun = true,
          .pqc = true,
      },
  };

  static std::vector<TestConfig> AllVariants() {
    std::vector<TestConfig> out;
    for (auto cc : kEndpointVariants) {
      for (auto sc : kEndpointVariants) {
        for (auto use_ice_lite : {false, true}) {
          for (auto cic : {true, false}) {
            for (auto p : {SSL_PROTOCOL_DTLS_12, SSL_PROTOCOL_DTLS_13}) {
              auto config = TestConfig{.ice_lite = use_ice_lite,
                                       .client_ssl_client = cic,
                                       .protocol_version = p,
                                       .client_config = cc,
                                       .server_config = sc}
                                .fix();
              if (p == SSL_PROTOCOL_DTLS_12 && (cc.pqc || sc.pqc)) {
                continue;
              }
              out.push_back(config);
            }
          }
        }
      }
    }
    return out;
  }
};

class Base {
 public:
  explicit Base(const TestConfig& config)
      : config_(config),
        ss_(std::make_unique<VirtualSocketServer>()),
        socket_factory_(std::make_unique<BasicPacketSocketFactory>(ss_.get())),
        client_(/* client= */ true, config.client_config),
        server_(/* client= */ false, config.server_config),
        client_ice_parameters_("c_ufrag",
                               "c_icepwd_something_something",
                               false),
        server_ice_parameters_("s_ufrag",
                               "s_icepwd_something_something",
                               false) {}
  virtual ~Base() { TearDown(); }

  struct Endpoint {
    explicit Endpoint(bool client_, const EndpointConfig& config_)
        : client(client_),
          config(config_),
          env(CreateEnvironment(CreateTestFieldTrialsPtr(
              config.dtls_in_stun ? "WebRTC-IceHandshakeDtls/Enabled/" : ""))) {
    }

    bool client;
    EmulatedNetworkManagerInterface* emulated_network_manager = nullptr;
    std::unique_ptr<NetworkManager> network_manager;
    std::unique_ptr<BasicPacketSocketFactory> packet_socket_factory;
    std::unique_ptr<PortAllocator> allocator;
    scoped_refptr<IceTransportInterface> ice_transport;
    std::unique_ptr<DtlsTransportInternalImpl> dtls;

    // Convenience getter for the internal transport.
    IceTransportInternal* ice() { return ice_transport->internal(); }

    // SetRemoteFingerprintFromCert does not actually set the fingerprint,
    // but only store it for setting later.
    bool store_but_dont_set_remote_fingerprint = false;
    std::unique_ptr<SSLFingerprint> remote_fingerprint;

    scoped_refptr<RTCCertificate> local_certificate;
    scoped_refptr<RTCCertificate> remote_certificate;

    EndpointConfig config;
    Environment env;

    void Restart(Base& test) {
      dtls.reset();
      ice_transport = nullptr;
      allocator.reset();
      packet_socket_factory.reset();

      packet_socket_factory = std::make_unique<BasicPacketSocketFactory>(
          emulated_network_manager->socket_factory());
      allocator = std::make_unique<BasicPortAllocator>(
          env, network_manager.get(), packet_socket_factory.get());
      test.SetupIceAndDtls(*this);
      allocator->Initialize();
    }
  };

  // Run benchmark for this TestConfig& `iter` iterations,
  // return statistics.
  SamplesStatsCounter RunBenchmark(int iter) {
    ConfigureEmulatedNetwork(config_.pct_loss, config_.client_interface_count,
                             config_.server_interface_count);
    Prepare();

    SamplesStatsCounter stats(iter);
    for (int i = 0; i < iter; i++) {
      int client_sent = 0;
      std::atomic<int> client_recv = 0;
      int server_sent = 0;
      std::atomic<int> server_recv = 0;
      void* id = this;

      client_thread()->BlockingCall([&]() {
        return client_.dtls->RegisterReceivedPacketCallback(
            id, [&](auto, auto) { client_recv++; });
      });
      server_thread()->BlockingCall([&]() {
        return server_.dtls->RegisterReceivedPacketCallback(
            id, [&](auto, auto) { server_recv++; });
      });

      client_thread()->PostTask(
          [&]() { client_.ice()->MaybeStartGathering(); });
      server_thread()->PostTask(
          [&]() { server_.ice()->MaybeStartGathering(); });

      auto start = CurrentTime();

      while (client_recv == 0 || server_recv == 0) {
        int delay = 50;
        AdvanceTime(TimeDelta::Millis(delay));

        // Send data
        {
          int flags = 0;
          AsyncSocketPacketOptions options;
          std::string a_string(50, 'a');

          if (client_.dtls->writable()) {
            client_thread()->BlockingCall([&]() {
              if (client_.dtls->SendPacket(a_string.c_str(), a_string.length(),
                                           options, flags) > 0) {
                client_sent++;
              }
            });
          }
          if (server_.dtls->writable()) {
            server_thread()->BlockingCall([&]() {
              if (server_.dtls->SendPacket(a_string.c_str(), a_string.length(),
                                           options, flags) > 0) {
                server_sent++;
              }
            });
          }
        }
      }
      auto end = CurrentTime();
      stats.AddSample(SamplesStatsCounter::StatsSample{
          .value = static_cast<double>((end - start).ms()),
          .time = end,
      });
      client_thread()->BlockingCall(
          [&]() { return client_.dtls->DeregisterReceivedPacketCallback(id); });
      server_thread()->BlockingCall(
          [&]() { return server_.dtls->DeregisterReceivedPacketCallback(id); });
      if (i + 1 < iter) {
        client_thread()->BlockingCall([&]() { client_.Restart(*this); });
        server_thread()->BlockingCall([&]() { server_.Restart(*this); });
      }
    }
    return stats;
  }

  static bool IsBoringSsl() { return SSLStreamAdapter::IsBoringSsl(); }

 protected:
  void SetUp() {}

  void TearDown() {
    if (client_thread() != nullptr) {
      client_thread()->BlockingCall([&]() {
        client_.dtls.reset();
        client_.ice_transport = nullptr;
        client_.allocator.reset();
      });
    }

    if (server_thread() != nullptr) {
      server_thread()->BlockingCall([&]() {
        server_.dtls.reset();
        server_.ice_transport = nullptr;
        server_.allocator.reset();
      });
    }
  }

  void ConfigureEmulatedNetwork(int pct_loss = 25,
                                int client_interface_count = 1,
                                int server_interface_count = 1) {
    network_emulation_manager_ =
        CreateNetworkEmulationManager({.time_mode = TimeMode::kSimulated});

    BuiltInNetworkBehaviorConfig networkBehavior;
    networkBehavior.link_capacity = DataRate::KilobitsPerSec(220);
    networkBehavior
        .queue_delay_ms = /* this is one way delay, i.e. divide the rtt by 2 */
        DtlsTransportInternalImpl::kDefaultHandshakeEstimateRttMs / 2;
    networkBehavior.queue_length_packets = 30;
    networkBehavior.loss_percent = pct_loss;

    auto pair = network_emulation_manager_->CreateEndpointPairWithTwoWayRoutes(
        networkBehavior, client_interface_count, server_interface_count);
    client_.emulated_network_manager = pair.first;
    server_.emulated_network_manager = pair.second;
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
      SetupEndpoint(client_, client_certificate, server_certificate);
    });

    server_thread()->BlockingCall([&]() {
      SetupEndpoint(server_, client_certificate, server_certificate);
    });

    // Setup the network.
    if (network_emulation_manager_ == nullptr) {
      network_manager_->AddInterface(SocketAddress("192.168.1.1", 0));
    }

    client_thread()->BlockingCall([&]() { client_.allocator->Initialize(); });
    server_thread()->BlockingCall([&]() { server_.allocator->Initialize(); });
  }

  Timestamp CurrentTime() {
    if (network_emulation_manager_ == nullptr) {
      return Timestamp::Micros(fake_clock_.TimeNanos() / 1000);
    } else {
      return network_emulation_manager_->time_controller()
          ->GetClock()
          ->CurrentTime();
    }
  }

  void AdvanceTime(TimeDelta delta) {
    RTC_CHECK(network_emulation_manager_ != nullptr);
    if (network_emulation_manager_ == nullptr) {
      fake_clock_.AdvanceTime(delta);
    } else {
      network_emulation_manager_->time_controller()->AdvanceTime(delta);
    }
  }

  WaitUntilSettings wait_until_settings(int timeout_ms = kDefaultTimeout) {
    if (network_emulation_manager_ == nullptr) {
      return {
          .timeout = TimeDelta::Millis(timeout_ms),
          .clock = &fake_clock_,
      };
    } else {
      return {
          .timeout = TimeDelta::Millis(timeout_ms),
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

  Endpoint& dtls_client() {
    return client_.config.ssl_role == SSL_CLIENT ? client_ : server_;
  }
  Endpoint& dtls_server() {
    return client_.config.ssl_role == SSL_SERVER ? client_ : server_;
  }

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

  TestConfig config_;
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
  // Used for simlating an ICE Lite agent.
  FakeIceLiteAgentIceControllerFactory
      fake_ice_lite_agent_ice_controller_factory_;

 private:
  void CandidateC2S(IceTransportInternal*, const Candidate& c) {
    server_thread()->PostTask(
        [this, c = c]() { server_.ice()->AddRemoteCandidate(c); });
  }
  void CandidateS2C(IceTransportInternal*, const Candidate& c) {
    client_thread()->PostTask(
        [this, c = c]() { client_.ice()->AddRemoteCandidate(c); });
  }

  void SetupEndpoint(Endpoint& ep,
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
      ep.local_certificate =
          ep.client ? client_certificate : server_certificate;
      ep.remote_certificate =
          ep.client ? server_certificate : client_certificate;
      SetupIceAndDtls(ep);
    });
  }

  void SetupIceAndDtls(Endpoint& ep) {
    // Should we be using the FakeIceLiteAgent?
    bool ice_lite_agent =
        ep.config.ice_lite && ep.config.ice_role == ICEROLE_CONTROLLED;
    ep.allocator->set_flags(ep.allocator->flags() | PORTALLOCATOR_DISABLE_TCP);
    IceTransportInit init(ep.env);
    init.set_port_allocator(ep.allocator.get());
    if (ice_lite_agent) {
      init.set_active_ice_controller_factory(
          &fake_ice_lite_agent_ice_controller_factory_);
    }
    auto channel = P2PTransportChannel::Create(
        ep.client ? "client_transport" : "server_transport",
        /* component= */ 0, std::move(init));
    ep.ice_transport = make_ref_counted<FakeIceTransport>(std::move(channel));
    // Is peer using ice-lite.
    if (ep.config.ice_lite && ep.config.ice_role == ICEROLE_CONTROLLING) {
      ep.ice()->SetRemoteIceMode(ICEMODE_LITE);
    }

    CryptoOptions crypto_options;
    if (ep.config.pqc) {
      FieldTrials field_trials("WebRTC-EnableDtlsPqc/Enabled/");
      crypto_options.ephemeral_key_exchange_cipher_groups.Update(&field_trials);
    }
    ep.dtls = std::make_unique<DtlsTransportInternalImpl>(
        ep.env, ep.ice_transport, crypto_options,
        ep.config.max_protocol_version);

    if (ice_lite_agent) {
      ep.dtls->SetFakeIceLite();
    }

    // Enable(or disable) the dtls_in_stun parameter before
    // DTLS is negotiated.
    IceConfig config;
    config.continual_gathering_policy = GATHER_CONTINUALLY;
    config.dtls_handshake_in_stun = ep.config.dtls_in_stun;
    ep.ice()->SetIceConfig(config);

    // Setup ICE.
    ep.ice()->SetIceParameters(ep.client ? client_ice_parameters_
                                         : server_ice_parameters_);
    ep.ice()->SetRemoteIceParameters(ep.client ? server_ice_parameters_
                                               : client_ice_parameters_);
    ep.ice()->SetIceRole(ep.config.ice_role);
    if (ep.client) {
      ep.ice()->SubscribeCandidateGathered(
          this,
          [this](IceTransportInternal* transport, const Candidate& candidate) {
            CandidateC2S(transport, candidate);
          });
    } else {
      ep.ice()->SubscribeCandidateGathered(
          this,
          [this](IceTransportInternal* transport, const Candidate& candidate) {
            CandidateS2C(transport, candidate);
          });
    }

    // Setup DTLS.
    ep.dtls->SetDtlsRole(ep.config.ssl_role);
    SetLocalCertificate(ep, ep.local_certificate);
    SetRemoteFingerprintFromCert(ep, ep.remote_certificate);
  }
};

}  // namespace dtls_ice_integration_fixture
}  // namespace webrtc

#endif  // P2P_DTLS_DTLS_ICE_INTEGRATION_FIXTURE_H_
