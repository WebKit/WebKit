/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/adaptive_fir_filter_erl.h"

#include <array>
#include <vector>

#include "rtc_base/system/arch.h"
#if defined(WEBRTC_ARCH_X86_FAMILY)
#include <emmintrin.h>
#endif

#include "system_wrappers/include/cpu_features_wrapper.h"
#include "test/gtest.h"

namespace webrtc {
namespace aec3 {

#if defined(WEBRTC_HAS_NEON)
// Verifies that the optimized method for echo return loss computation is
// bitexact to the reference counterpart.
TEST(AdaptiveFirFilter, UpdateErlNeonOptimization) {
  const size_t kNumPartitions = 12;
  std::vector<std::array<float, kFftLengthBy2Plus1>> H2(kNumPartitions);
  std::array<float, kFftLengthBy2Plus1> erl;
  std::array<float, kFftLengthBy2Plus1> erl_NEON;

  for (size_t j = 0; j < H2.size(); ++j) {
    for (size_t k = 0; k < H2[j].size(); ++k) {
      H2[j][k] = k + j / 3.f;
    }
  }

  ErlComputer(H2, erl);
  ErlComputer_NEON(H2, erl_NEON);

  for (size_t j = 0; j < erl.size(); ++j) {
    EXPECT_FLOAT_EQ(erl[j], erl_NEON[j]);
  }
}

#endif

#if defined(WEBRTC_ARCH_X86_FAMILY)
// Verifies that the optimized method for echo return loss computation is
// bitexact to the reference counterpart.
TEST(AdaptiveFirFilter, UpdateErlSse2Optimization) {
  bool use_sse2 = (WebRtc_GetCPUInfo(kSSE2) != 0);
  if (use_sse2) {
    const size_t kNumPartitions = 12;
    std::vector<std::array<float, kFftLengthBy2Plus1>> H2(kNumPartitions);
    std::array<float, kFftLengthBy2Plus1> erl;
    std::array<float, kFftLengthBy2Plus1> erl_SSE2;

    for (size_t j = 0; j < H2.size(); ++j) {
      for (size_t k = 0; k < H2[j].size(); ++k) {
        H2[j][k] = k + j / 3.f;
      }
    }

    ErlComputer(H2, erl);
    ErlComputer_SSE2(H2, erl_SSE2);

    for (size_t j = 0; j < erl.size(); ++j) {
      EXPECT_FLOAT_EQ(erl[j], erl_SSE2[j]);
    }
  }
}

#endif

}  // namespace aec3
}  // namespace webrtc
