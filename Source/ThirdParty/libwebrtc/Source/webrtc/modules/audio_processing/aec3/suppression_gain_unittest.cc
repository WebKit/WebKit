/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/suppression_gain.h"

#include "webrtc/typedefs.h"
#include "webrtc/system_wrappers/include/cpu_features_wrapper.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace aec3 {

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies that the check for non-null output gains works.
TEST(SuppressionGain, NullOutputGains) {
  std::array<float, kFftLengthBy2Plus1> E2;
  std::array<float, kFftLengthBy2Plus1> R2;
  std::array<float, kFftLengthBy2Plus1> N2;
  EXPECT_DEATH(
      SuppressionGain(DetectOptimization()).GetGain(E2, R2, N2, 0.1f, nullptr),
      "");
}

#endif

#if defined(WEBRTC_ARCH_X86_FAMILY)
// Verifies that the optimized methods are bitexact to their reference
// counterparts.
TEST(SuppressionGain, TestOptimizations) {
  if (WebRtc_GetCPUInfo(kSSE2) != 0) {
    std::array<float, kFftLengthBy2 - 1> G2_old;
    std::array<float, kFftLengthBy2 - 1> M2_old;
    std::array<float, kFftLengthBy2 - 1> G2_old_SSE2;
    std::array<float, kFftLengthBy2 - 1> M2_old_SSE2;
    std::array<float, kFftLengthBy2Plus1> E2;
    std::array<float, kFftLengthBy2Plus1> R2;
    std::array<float, kFftLengthBy2Plus1> N2;
    std::array<float, kFftLengthBy2Plus1> g;
    std::array<float, kFftLengthBy2Plus1> g_SSE2;

    G2_old.fill(1.f);
    M2_old.fill(.23f);
    G2_old_SSE2.fill(1.f);
    M2_old_SSE2.fill(.23f);

    E2.fill(10.f);
    R2.fill(0.1f);
    N2.fill(100.f);
    for (int k = 0; k < 10; ++k) {
      ComputeGains(E2, R2, N2, 0.1f, &G2_old, &M2_old, &g);
      ComputeGains_SSE2(E2, R2, N2, 0.1f, &G2_old_SSE2, &M2_old_SSE2, &g_SSE2);
      for (size_t j = 0; j < G2_old.size(); ++j) {
        EXPECT_NEAR(G2_old[j], G2_old_SSE2[j], 0.0000001f);
      }
      for (size_t j = 0; j < M2_old.size(); ++j) {
        EXPECT_NEAR(M2_old[j], M2_old_SSE2[j], 0.0000001f);
      }
      for (size_t j = 0; j < g.size(); ++j) {
        EXPECT_NEAR(g[j], g_SSE2[j], 0.0000001f);
      }
    }

    E2.fill(100.f);
    R2.fill(0.1f);
    N2.fill(0.f);
    for (int k = 0; k < 10; ++k) {
      ComputeGains(E2, R2, N2, 0.1f, &G2_old, &M2_old, &g);
      ComputeGains_SSE2(E2, R2, N2, 0.1f, &G2_old_SSE2, &M2_old_SSE2, &g_SSE2);
      for (size_t j = 0; j < G2_old.size(); ++j) {
        EXPECT_NEAR(G2_old[j], G2_old_SSE2[j], 0.0000001f);
      }
      for (size_t j = 0; j < M2_old.size(); ++j) {
        EXPECT_NEAR(M2_old[j], M2_old_SSE2[j], 0.0000001f);
      }
      for (size_t j = 0; j < g.size(); ++j) {
        EXPECT_NEAR(g[j], g_SSE2[j], 0.0000001f);
      }
    }

    E2.fill(0.1f);
    R2.fill(100.f);
    N2.fill(0.f);
    for (int k = 0; k < 10; ++k) {
      ComputeGains(E2, R2, N2, 0.1f, &G2_old, &M2_old, &g);
      ComputeGains_SSE2(E2, R2, N2, 0.1f, &G2_old_SSE2, &M2_old_SSE2, &g_SSE2);
      for (size_t j = 0; j < G2_old.size(); ++j) {
        EXPECT_NEAR(G2_old[j], G2_old_SSE2[j], 0.0000001f);
      }
      for (size_t j = 0; j < M2_old.size(); ++j) {
        EXPECT_NEAR(M2_old[j], M2_old_SSE2[j], 0.0000001f);
      }
      for (size_t j = 0; j < g.size(); ++j) {
        EXPECT_NEAR(g[j], g_SSE2[j], 0.0000001f);
      }
    }
  }
}
#endif

// Does a sanity check that the gains are correctly computed.
TEST(SuppressionGain, BasicGainComputation) {
  SuppressionGain suppression_gain(DetectOptimization());
  std::array<float, kFftLengthBy2Plus1> E2;
  std::array<float, kFftLengthBy2Plus1> R2;
  std::array<float, kFftLengthBy2Plus1> N2;
  std::array<float, kFftLengthBy2Plus1> g;

  // Ensure that a strong noise is detected to mask any echoes.
  E2.fill(10.f);
  R2.fill(0.1f);
  N2.fill(100.f);
  for (int k = 0; k < 10; ++k) {
    suppression_gain.GetGain(E2, R2, N2, 0.1f, &g);
  }
  std::for_each(g.begin(), g.end(),
                [](float a) { EXPECT_NEAR(1.f, a, 0.001); });

  // Ensure that a strong nearend is detected to mask any echoes.
  E2.fill(100.f);
  R2.fill(0.1f);
  N2.fill(0.f);
  for (int k = 0; k < 10; ++k) {
    suppression_gain.GetGain(E2, R2, N2, 0.1f, &g);
  }
  std::for_each(g.begin(), g.end(),
                [](float a) { EXPECT_NEAR(1.f, a, 0.001); });

  // Ensure that a strong echo is suppressed.
  E2.fill(0.1f);
  R2.fill(100.f);
  N2.fill(0.f);
  for (int k = 0; k < 10; ++k) {
    suppression_gain.GetGain(E2, R2, N2, 0.1f, &g);
  }
  std::for_each(g.begin(), g.end(),
                [](float a) { EXPECT_NEAR(0.f, a, 0.001); });
}

}  // namespace aec3
}  // namespace webrtc
