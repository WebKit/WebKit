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
#include <vector>

#include "absl/flags/flag.h"
#include "api/dtls_transport_interface.h"
#include "api/ice_transport_interface.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/test/rtc_error_matchers.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "p2p/base/connection_info.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/dtls/dtls_ice_integration_fixture.h"
#include "p2p/dtls/dtls_transport.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/logging.h"
#include "rtc_base/random.h"
#include "rtc_base/socket_address.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

ABSL_FLAG(int32_t,
          long_running_seed,
          7788,
          "0 means use time(0) as seed (i.e non deterministic)");
ABSL_FLAG(int32_t,
          long_running_run_time_minutes,
          7,
          "Runtime in simulated time.");
ABSL_FLAG(bool, long_running_send_data, true, "");

namespace webrtc {
namespace {

using dtls_ice_integration_fixture::TestConfig;
using ::testing::Eq;
using ::testing::IsTrue;
using ::testing::Not;

class DtlsIceIntegrationTest : public dtls_ice_integration_fixture::Base,
                               public ::testing::TestWithParam<TestConfig> {
 public:
  DtlsIceIntegrationTest() : dtls_ice_integration_fixture::Base(GetParam()) {}
  ~DtlsIceIntegrationTest() override = default;

  void SetUp() override { Base::SetUp(); }
  void TearDown() override { Base::TearDown(); }

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
};

TEST_P(DtlsIceIntegrationTest, SmokeTest) {
  ConfigureEmulatedNetwork(/* pct_loss= */ 0);
  Prepare();
  client_thread()->PostTask([&]() { client_.ice()->MaybeStartGathering(); });
  server_thread()->PostTask([&]() { server_.ice()->MaybeStartGathering(); });

  // Note: this only reaches the pending piggybacking state.
  EXPECT_THAT(
      WaitUntil(
          [&] { return client_.dtls->writable() && server_.dtls->writable(); },
          IsTrue(), wait_until_settings()),
      IsRtcOk());

  client_thread()->BlockingCall([&]() {
    EXPECT_EQ(client_.dtls->IsDtlsPiggybackSupportedByPeer(),
              client_.config.dtls_in_stun && server_.config.dtls_in_stun);
    EXPECT_EQ(client_.dtls->WasDtlsCompletedByPiggybacking(),
              client_.config.dtls_in_stun && server_.config.dtls_in_stun);
  });
  server_thread()->BlockingCall([&]() {
    EXPECT_EQ(server_.dtls->IsDtlsPiggybackSupportedByPeer(),
              client_.config.dtls_in_stun && server_.config.dtls_in_stun);
    EXPECT_EQ(server_.dtls->WasDtlsCompletedByPiggybacking(),
              client_.config.dtls_in_stun && server_.config.dtls_in_stun);
  });

  if (client_.config.dtls_in_stun && server_.config.dtls_in_stun) {
    EXPECT_GE(dtls_client().dtls->GetStunDataCount(), 0);
    EXPECT_GE(dtls_server().dtls->GetStunDataCount(), 0);
  }

  EXPECT_EQ(client_.dtls->GetRetransmissionCount(), 0);
  EXPECT_EQ(server_.dtls->GetRetransmissionCount(), 0);
}

TEST_P(DtlsIceIntegrationTest, AddCandidates) {
  Prepare();
  client_.ice()->MaybeStartGathering();
  server_.ice()->MaybeStartGathering();

  // Note: this only reaches the pending piggybacking state.
  EXPECT_THAT(
      WaitUntil(
          [&] { return client_.dtls->writable() && server_.dtls->writable(); },
          IsTrue(), wait_until_settings()),
      IsRtcOk());
  EXPECT_EQ(client_.dtls->IsDtlsPiggybackSupportedByPeer(),
            client_.config.dtls_in_stun && server_.config.dtls_in_stun);
  EXPECT_EQ(server_.dtls->IsDtlsPiggybackSupportedByPeer(),
            client_.config.dtls_in_stun && server_.config.dtls_in_stun);
  EXPECT_EQ(client_.dtls->WasDtlsCompletedByPiggybacking(),
            client_.config.dtls_in_stun && server_.config.dtls_in_stun);
  EXPECT_EQ(server_.dtls->WasDtlsCompletedByPiggybacking(),
            client_.config.dtls_in_stun && server_.config.dtls_in_stun);

  if (client_.config.dtls_in_stun && server_.config.dtls_in_stun) {
    EXPECT_GE(dtls_client().dtls->GetStunDataCount(), 0);
    EXPECT_GE(dtls_server().dtls->GetStunDataCount(), 0);
  }

  // Validate that we can add new Connections (that become writable).
  network_manager_->AddInterface(SocketAddress("192.168.2.1", 0));
  EXPECT_THAT(WaitUntil(
                  [&] {
                    return CountWritableConnections(client_.ice()) > 1 &&
                           CountWritableConnections(server_.ice()) > 1;
                  },
                  IsTrue(), wait_until_settings()),
              IsRtcOk());
}

// Check that DtlsInStun still works even if SetRemoteFingerprint is called
// "late". This is what happens if the answer sdp comes strictly after ICE has
// connected. Before this patch, this would disable stun-piggy-backing.
TEST_P(DtlsIceIntegrationTest, ClientLateCertificate) {
  client_.store_but_dont_set_remote_fingerprint = true;
  ConfigureEmulatedNetwork(/* pct_loss= */ 0);
  Prepare();
  client_thread()->PostTask([&]() { client_.ice()->MaybeStartGathering(); });
  server_thread()->PostTask([&]() { server_.ice()->MaybeStartGathering(); });

  ASSERT_THAT(WaitUntil(
                  [&] {
                    return client_thread()->BlockingCall([&]() {
                      return CountWritableConnections(client_.ice());
                    }) > 0;
                  },
                  IsTrue(), wait_until_settings()),
              IsRtcOk());

  client_thread()->BlockingCall([&]() { SetRemoteFingerprint(client_); });

  ASSERT_THAT(
      WaitUntil(
          [&] { return client_.dtls->writable() && server_.dtls->writable(); },
          IsTrue(), wait_until_settings()),
      IsRtcOk());

  client_thread()->BlockingCall([&]() {
    EXPECT_EQ(client_.dtls->IsDtlsPiggybackSupportedByPeer(),
              client_.config.dtls_in_stun && server_.config.dtls_in_stun);
    EXPECT_EQ(client_.dtls->WasDtlsCompletedByPiggybacking(),
              client_.config.dtls_in_stun && server_.config.dtls_in_stun);
  });
  server_thread()->BlockingCall([&]() {
    EXPECT_EQ(server_.dtls->WasDtlsCompletedByPiggybacking(),
              client_.config.dtls_in_stun && server_.config.dtls_in_stun);
  });

  EXPECT_EQ(client_.dtls->GetRetransmissionCount(), 0);
  EXPECT_EQ(server_.dtls->GetRetransmissionCount(), 0);
}

TEST_P(DtlsIceIntegrationTest, TestWithPacketLoss) {
  if (!IsBoringSsl()) {
    GTEST_SKIP() << "Needs boringssl.";
  }

  if (client_.config.dtls_in_stun != server_.config.dtls_in_stun) {
    // TODO jonaso, webrtc:404763475 : re-enable once
    // boringssl has been merged and test cases updated.
    GTEST_SKIP() << "TODO jonaso.";
  }

  ConfigureEmulatedNetwork();
  Prepare();

  client_thread()->PostTask([&]() { client_.ice()->MaybeStartGathering(); });
  server_thread()->PostTask([&]() { server_.ice()->MaybeStartGathering(); });

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
            client_.config.dtls_in_stun && server_.config.dtls_in_stun);
  EXPECT_EQ(server_thread()->BlockingCall([&]() {
    return server_.dtls->IsDtlsPiggybackSupportedByPeer();
  }),
            client_.config.dtls_in_stun && server_.config.dtls_in_stun);
}

TEST_P(DtlsIceIntegrationTest, LongRunningTestWithPacketLoss) {
  if (!IsBoringSsl()) {
    GTEST_SKIP() << "Needs boringssl.";
  }

  if (client_.config.dtls_in_stun != server_.config.dtls_in_stun) {
    // TODO jonaso, webrtc:404763475 : re-enable once
    // boringssl has been merged and test cases updated.
    GTEST_SKIP() << "TODO jonaso.";
  }

  int seed = absl::GetFlag(FLAGS_long_running_seed);
  if (seed == 0) {
    seed = 1 + time(0);
  }
  RTC_LOG(LS_INFO) << "seed: " << seed;
  Random rand(seed);
  ConfigureEmulatedNetwork();
  Prepare();

  client_thread()->PostTask([&]() { client_.ice()->MaybeStartGathering(); });
  server_thread()->PostTask([&]() { server_.ice()->MaybeStartGathering(); });

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

  auto now = CurrentTime();
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

    AdvanceTime(TimeDelta::Millis(delay));
    now = CurrentTime();

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
  ConfigureEmulatedNetwork(/* pct_loss= */ 0);
  Prepare();

  std::string a_long_string(500, 'a');
  client_.ice()->GetDictionaryWriter()->get().SetByteString(77)->CopyBytes(
      a_long_string);
  server_.ice()->GetDictionaryWriter()->get().SetByteString(78)->CopyBytes(
      a_long_string);

  client_thread()->PostTask([&]() { client_.ice()->MaybeStartGathering(); });
  server_thread()->PostTask([&]() { server_.ice()->MaybeStartGathering(); });

  // Note: this only reaches the pending piggybacking state.
  EXPECT_THAT(
      WaitUntil(
          [&] { return client_.dtls->writable() && server_.dtls->writable(); },
          IsTrue(), wait_until_settings()),
      IsRtcOk());

  client_thread()->BlockingCall([&]() {
    EXPECT_EQ(client_.dtls->IsDtlsPiggybackSupportedByPeer(),
              client_.config.dtls_in_stun && server_.config.dtls_in_stun);
    EXPECT_EQ(client_.dtls->WasDtlsCompletedByPiggybacking(),
              client_.config.dtls_in_stun && server_.config.dtls_in_stun);
  });

  server_thread()->BlockingCall([&]() {
    EXPECT_EQ(server_.dtls->IsDtlsPiggybackSupportedByPeer(),
              client_.config.dtls_in_stun && server_.config.dtls_in_stun);
    EXPECT_EQ(server_.dtls->WasDtlsCompletedByPiggybacking(),
              client_.config.dtls_in_stun && server_.config.dtls_in_stun);
  });

  EXPECT_EQ(client_.dtls->GetRetransmissionCount(), 0);
  EXPECT_EQ(server_.dtls->GetRetransmissionCount(), 0);
}

INSTANTIATE_TEST_SUITE_P(DtlsStunPiggybackingIntegrationTest,
                         DtlsIceIntegrationTest,
                         ::testing::ValuesIn(TestConfig::AllVariants()));

struct DtlsIceIntegrationPerformanceTest
    : public ::testing::TestWithParam<TestConfig> {
  DtlsIceIntegrationPerformanceTest() {}
  ~DtlsIceIntegrationPerformanceTest() override = default;

  void SetUp() override {}
  void TearDown() override {}

#ifdef NDEBUG
  static constexpr int kLossVariants[] = {0, 5, 10, 15};
#else
  // Only run 1 variant to not consume too much time
  static constexpr int kLossVariants[] = {10};
#endif

  static std::vector<TestConfig> Variants() {
    std::vector<TestConfig> out;
    for (auto loss : kLossVariants) {
      for (auto base : TestConfig::AllVariants()) {
        auto config = base;
        config.pct_loss = loss;
        config.client_interface_count = 1;
        config.server_interface_count = 1;
        config.fix();

        // Let's skip a few combinations...
        if (config.client_config.pqc != config.server_config.pqc) {
          continue;
        }

        // We only need to emit the 1 dtls-in-stun variant
        // since we iterate over it inside the actual test.
        if (!(config.client_config.dtls_in_stun == false &&
              config.server_config.dtls_in_stun == false)) {
          continue;
        }

        out.push_back(config);
      }
    }
    return out;
  }

  bool LessThan(SamplesStatsCounter& s1, SamplesStatsCounter& s2) {
    EXPECT_LE(s1.GetAverage(), s2.GetAverage());
    EXPECT_LE(s1.GetPercentile(0.10), s2.GetPercentile(0.10));
    EXPECT_LE(s1.GetPercentile(0.50), s2.GetPercentile(0.50));
    EXPECT_LE(s1.GetPercentile(0.95), s2.GetPercentile(0.95));
    return true;
  }
};

INSTANTIATE_TEST_SUITE_P(
    DtlsIceIntegrationPerformanceTest,
    DtlsIceIntegrationPerformanceTest,
    ::testing::ValuesIn(DtlsIceIntegrationPerformanceTest::Variants()));

TEST_P(DtlsIceIntegrationPerformanceTest, ConnectTime) {
  if (!dtls_ice_integration_fixture::Base::IsBoringSsl()) {
    GTEST_SKIP() << "Needs boringssl.";
  }

  {
    TestConfig config = GetParam();
    if (config.client_config.pqc == 1 && config.server_config.pqc &&
        config.server_config.ice_lite) {
      // TODO jonaso, webrtc:404763475 : re-enable once
      // boringssl has been merged and test cases updated.
      GTEST_SKIP() << "TODO jonaso.";
    }
  }

  int iter = 50;
  // Use a fixed seed to get consistent behavior.
  Random rand(/* seed= */ 77);
  std::vector<SamplesStatsCounter> stats_results;
  {
    TestConfig config = GetParam();
    config.client_config.dtls_in_stun = false;
    config.server_config.dtls_in_stun = false;
    dtls_ice_integration_fixture::Base base(config);
    stats_results.push_back(base.RunBenchmark(iter));
  }

  // Verify that turning ON should give better
  // result than having it OFF.
  {
    TestConfig config = GetParam();
    config.client_config.dtls_in_stun = true;
    config.server_config.dtls_in_stun = true;
    dtls_ice_integration_fixture::Base base(config);
    auto result = base.RunBenchmark(iter);
    EXPECT_TRUE(LessThan(result, stats_results[0]));
  }
}

}  // namespace
}  // namespace webrtc
