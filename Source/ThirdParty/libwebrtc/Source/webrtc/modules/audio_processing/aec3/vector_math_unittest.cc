/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/vector_math.h"

#include <math.h>

#include "rtc_base/system/arch.h"
#include "system_wrappers/include/cpu_features_wrapper.h"
#include "test/gtest.h"

namespace webrtc {

#if defined(WEBRTC_HAS_NEON)

TEST(VectorMath, Sqrt) {
  std::array<float, kFftLengthBy2Plus1> x;
  std::array<float, kFftLengthBy2Plus1> z;
  std::array<float, kFftLengthBy2Plus1> z_neon;

  for (size_t k = 0; k < x.size(); ++k) {
    x[k] = (2.f / 3.f) * k;
  }

  std::copy(x.begin(), x.end(), z.begin());
  aec3::VectorMath(Aec3Optimization::kNone).Sqrt(z);
  std::copy(x.begin(), x.end(), z_neon.begin());
  aec3::VectorMath(Aec3Optimization::kNeon).Sqrt(z_neon);
  for (size_t k = 0; k < z.size(); ++k) {
    EXPECT_NEAR(z[k], z_neon[k], 0.0001f);
    EXPECT_NEAR(sqrtf(x[k]), z_neon[k], 0.0001f);
  }
}

TEST(VectorMath, Multiply) {
  std::array<float, kFftLengthBy2Plus1> x;
  std::array<float, kFftLengthBy2Plus1> y;
  std::array<float, kFftLengthBy2Plus1> z;
  std::array<float, kFftLengthBy2Plus1> z_neon;

  for (size_t k = 0; k < x.size(); ++k) {
    x[k] = k;
    y[k] = (2.f / 3.f) * k;
  }

  aec3::VectorMath(Aec3Optimization::kNone).Multiply(x, y, z);
  aec3::VectorMath(Aec3Optimization::kNeon).Multiply(x, y, z_neon);
  for (size_t k = 0; k < z.size(); ++k) {
    EXPECT_FLOAT_EQ(z[k], z_neon[k]);
    EXPECT_FLOAT_EQ(x[k] * y[k], z_neon[k]);
  }
}

TEST(VectorMath, Accumulate) {
  std::array<float, kFftLengthBy2Plus1> x;
  std::array<float, kFftLengthBy2Plus1> z;
  std::array<float, kFftLengthBy2Plus1> z_neon;

  for (size_t k = 0; k < x.size(); ++k) {
    x[k] = k;
    z[k] = z_neon[k] = 2.f * k;
  }

  aec3::VectorMath(Aec3Optimization::kNone).Accumulate(x, z);
  aec3::VectorMath(Aec3Optimization::kNeon).Accumulate(x, z_neon);
  for (size_t k = 0; k < z.size(); ++k) {
    EXPECT_FLOAT_EQ(z[k], z_neon[k]);
    EXPECT_FLOAT_EQ(x[k] + 2.f * x[k], z_neon[k]);
  }
}
#endif

#if defined(WEBRTC_ARCH_X86_FAMILY)

TEST(VectorMath, Sqrt) {
  if (WebRtc_GetCPUInfo(kSSE2) != 0) {
    std::array<float, kFftLengthBy2Plus1> x;
    std::array<float, kFftLengthBy2Plus1> z;
    std::array<float, kFftLengthBy2Plus1> z_sse2;

    for (size_t k = 0; k < x.size(); ++k) {
      x[k] = (2.f / 3.f) * k;
    }

    std::copy(x.begin(), x.end(), z.begin());
    aec3::VectorMath(Aec3Optimization::kNone).Sqrt(z);
    std::copy(x.begin(), x.end(), z_sse2.begin());
    aec3::VectorMath(Aec3Optimization::kSse2).Sqrt(z_sse2);
    EXPECT_EQ(z, z_sse2);
    for (size_t k = 0; k < z.size(); ++k) {
      EXPECT_FLOAT_EQ(z[k], z_sse2[k]);
      EXPECT_FLOAT_EQ(sqrtf(x[k]), z_sse2[k]);
    }
  }
}

TEST(VectorMath, Multiply) {
  if (WebRtc_GetCPUInfo(kSSE2) != 0) {
    std::array<float, kFftLengthBy2Plus1> x;
    std::array<float, kFftLengthBy2Plus1> y;
    std::array<float, kFftLengthBy2Plus1> z;
    std::array<float, kFftLengthBy2Plus1> z_sse2;

    for (size_t k = 0; k < x.size(); ++k) {
      x[k] = k;
      y[k] = (2.f / 3.f) * k;
    }

    aec3::VectorMath(Aec3Optimization::kNone).Multiply(x, y, z);
    aec3::VectorMath(Aec3Optimization::kSse2).Multiply(x, y, z_sse2);
    for (size_t k = 0; k < z.size(); ++k) {
      EXPECT_FLOAT_EQ(z[k], z_sse2[k]);
      EXPECT_FLOAT_EQ(x[k] * y[k], z_sse2[k]);
    }
  }
}

TEST(VectorMath, Accumulate) {
  if (WebRtc_GetCPUInfo(kSSE2) != 0) {
    std::array<float, kFftLengthBy2Plus1> x;
    std::array<float, kFftLengthBy2Plus1> z;
    std::array<float, kFftLengthBy2Plus1> z_sse2;

    for (size_t k = 0; k < x.size(); ++k) {
      x[k] = k;
      z[k] = z_sse2[k] = 2.f * k;
    }

    aec3::VectorMath(Aec3Optimization::kNone).Accumulate(x, z);
    aec3::VectorMath(Aec3Optimization::kSse2).Accumulate(x, z_sse2);
    for (size_t k = 0; k < z.size(); ++k) {
      EXPECT_FLOAT_EQ(z[k], z_sse2[k]);
      EXPECT_FLOAT_EQ(x[k] + 2.f * x[k], z_sse2[k]);
    }
  }
}
#endif

}  // namespace webrtc
