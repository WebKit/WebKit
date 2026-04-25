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
#include <set>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "api/numerics/samples_stats_counter.h"
#include "p2p/dtls/dtls_ice_integration_fixture.h"
#include "rtc_base/logging.h"
#include "test/gtest.h"

ABSL_FLAG(int32_t, bench_iterations, 100, "");
ABSL_FLAG(std::vector<std::string>,
          bench_pct_loss,
          std::vector<std::string>({"0", "5", "10", "25"}),
          "Packet loss in percent");
ABSL_FLAG(std::vector<std::string>,
          bench_server_candidates,
          std::vector<std::string>({"1", "2"}),
          "No of candidates on server to use");

namespace webrtc {
namespace {

std::set<int> ToIntSet(const std::vector<std::string>& args) {
  std::set<int> out;
  for (const auto& arg : args) {
    out.insert(std::stoi(arg));
  }
  return out;
}

using dtls_ice_integration_fixture::TestConfig;

struct DtlsIceIntegrationBenchmark
    : public dtls_ice_integration_fixture::Base,
      public ::testing::TestWithParam<TestConfig> {
  DtlsIceIntegrationBenchmark()
      : dtls_ice_integration_fixture::Base(GetParam()) {}
  ~DtlsIceIntegrationBenchmark() override = default;

  void SetUp() override { Base::SetUp(); }
  void TearDown() override { Base::TearDown(); }

  static std::vector<TestConfig> Variants() {
    std::vector<TestConfig> out;
    for (auto loss : {0, 5, 10, 15, 25, 50}) {
      for (auto sif : {1, 2}) {
        for (auto base : TestConfig::AllVariants()) {
          auto config = base;
          config.pct_loss = loss;
          config.client_interface_count = 1;
          config.server_interface_count = sif;
          config.fix();

          // Let's skip a few combinations...
          if (config.client_config.pqc != config.server_config.pqc) {
            continue;
          }
          if (config.client_config.dtls_in_stun !=
              config.server_config.dtls_in_stun) {
            continue;
          }
          out.push_back(config);
        }
      }
    }
    return out;
  }
};

INSTANTIATE_TEST_SUITE_P(
    DtlsIceIntegrationBenchmark,
    DtlsIceIntegrationBenchmark,
    ::testing::ValuesIn(DtlsIceIntegrationBenchmark::Variants()));

TEST_P(DtlsIceIntegrationBenchmark, Benchmark) {
  if (!IsBoringSsl()) {
    GTEST_SKIP() << "Needs boringssl.";
  }

  const int iter = absl::GetFlag(FLAGS_bench_iterations);
  if (iter == 0) {
    GTEST_SKIP() << "SKIP " << GetParam()
                 << " - filtered by bench_iterations command line argument.";
  }

  auto pct_loss_filter = ToIntSet(absl::GetFlag(FLAGS_bench_pct_loss));
  if (!pct_loss_filter.empty() &&
      !pct_loss_filter.contains(GetParam().pct_loss)) {
    GTEST_SKIP() << "SKIP " << GetParam()
                 << " - filtered by bench_pct_loss command line argument.";
  }

  auto server_candidates_filter =
      ToIntSet(absl::GetFlag(FLAGS_bench_server_candidates));
  if (!server_candidates_filter.empty() &&
      !server_candidates_filter.contains(GetParam().server_interface_count)) {
    GTEST_SKIP()
        << "SKIP " << GetParam()
        << " - filtered by bench_server_candidates command line argument.";
  }

  RTC_LOG(LS_INFO) << GetParam() << " START";

  SamplesStatsCounter stats = RunBenchmark(iter);
  RTC_LOG(LS_INFO) << GetParam() << " RESULT:"
                   << " p10: " << stats.GetPercentile(0.10)
                   << " p50: " << stats.GetPercentile(0.50)
                   << " avg: " << stats.GetAverage()
                   << " p95: " << stats.GetPercentile(0.95);
}

}  // namespace
}  // namespace webrtc
