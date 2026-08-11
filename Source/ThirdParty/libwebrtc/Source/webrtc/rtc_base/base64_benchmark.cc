/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "benchmark/benchmark.h"
#include "rtc_base/base64.h"

namespace webrtc {
namespace {

void BM_Base64Encode(benchmark::State& state) {
  std::vector<uint8_t> data(state.range(0));
  for (auto _ : state) {
    Base64Encode(data);
  }
}

void BM_Base64Decode(benchmark::State& state) {
  std::vector<uint8_t> data(state.range(0));
  std::string encoded = Base64Encode(data);
  for (auto _ : state) {
    Base64Decode(encoded);
  }
}

void BM_Base64DecodeForgiving(benchmark::State& state) {
  std::vector<uint8_t> data(state.range(0));
  std::string encoded = Base64Encode(data);
  // Add a newline every 64 chars.
  for (size_t i = 0; i < encoded.size(); i += 64) {
    encoded.insert(i, "\n");
  }
  for (auto _ : state) {
    Base64Decode(encoded, Base64DecodeOptions::kForgiving);
  }
}

BENCHMARK(BM_Base64Encode)->Range(64, 8 << 20);
BENCHMARK(BM_Base64Decode)->Range(64, 8 << 20);
BENCHMARK(BM_Base64DecodeForgiving)->Range(64, 8 << 20);

}  // namespace
}  // namespace webrtc
