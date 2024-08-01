/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/limiter.h"

#include <algorithm>

#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/agc2/agc2_testing_common.h"
#include "modules/audio_processing/agc2/vector_float_frame.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/gunit.h"

namespace webrtc {

TEST(Limiter, LimiterShouldConstructAndRun) {
  constexpr size_t kSamplesPerChannel = 480;
  ApmDataDumper apm_data_dumper(0);

  Limiter limiter(&apm_data_dumper, kSamplesPerChannel, "");

  std::array<float, kSamplesPerChannel> buffer;
  buffer.fill(kMaxAbsFloatS16Value);
  limiter.Process(
      DeinterleavedView<float>(buffer.data(), kSamplesPerChannel, 1));
}

TEST(Limiter, OutputVolumeAboveThreshold) {
  constexpr size_t kSamplesPerChannel = 480;
  const float input_level =
      (kMaxAbsFloatS16Value + DbfsToFloatS16(test::kLimiterMaxInputLevelDbFs)) /
      2.f;
  ApmDataDumper apm_data_dumper(0);

  Limiter limiter(&apm_data_dumper, kSamplesPerChannel, "");

  std::array<float, kSamplesPerChannel> buffer;

  // Give the level estimator time to adapt.
  for (int i = 0; i < 5; ++i) {
    std::fill(buffer.begin(), buffer.end(), input_level);
    limiter.Process(
        DeinterleavedView<float>(buffer.data(), kSamplesPerChannel, 1));
  }

  std::fill(buffer.begin(), buffer.end(), input_level);
  limiter.Process(
      DeinterleavedView<float>(buffer.data(), kSamplesPerChannel, 1));
  for (const auto& sample : buffer) {
    ASSERT_LT(0.9f * kMaxAbsFloatS16Value, sample);
  }
}

}  // namespace webrtc
