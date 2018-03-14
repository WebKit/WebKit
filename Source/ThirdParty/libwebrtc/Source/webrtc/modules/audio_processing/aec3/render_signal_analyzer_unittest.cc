/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/render_signal_analyzer.h"

#include <math.h>
#include <array>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/aec3_fft.h"
#include "modules/audio_processing/aec3/fft_data.h"
#include "modules/audio_processing/aec3/render_delay_buffer.h"
#include "modules/audio_processing/test/echo_canceller_test_tools.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr float kPi = 3.141592f;

void ProduceSinusoid(int sample_rate_hz,
                     float sinusoidal_frequency_hz,
                     size_t* sample_counter,
                     rtc::ArrayView<float> x) {
  // Produce a sinusoid of the specified frequency.
  for (size_t k = *sample_counter, j = 0; k < (*sample_counter + kBlockSize);
       ++k, ++j) {
    x[j] =
        32767.f * sin(2.f * kPi * sinusoidal_frequency_hz * k / sample_rate_hz);
  }
  *sample_counter = *sample_counter + kBlockSize;
}

}  // namespace

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
// Verifies that the check for non-null output parameter works.
TEST(RenderSignalAnalyzer, NullMaskOutput) {
  RenderSignalAnalyzer analyzer;
  EXPECT_DEATH(analyzer.MaskRegionsAroundNarrowBands(nullptr), "");
}

#endif

// Verify that no narrow bands are detected in a Gaussian noise signal.
TEST(RenderSignalAnalyzer, NoFalseDetectionOfNarrowBands) {
  RenderSignalAnalyzer analyzer;
  Random random_generator(42U);
  std::vector<std::vector<float>> x(3, std::vector<float>(kBlockSize, 0.f));
  std::array<float, kBlockSize> x_old;
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(EchoCanceller3Config(), 3));
  std::array<float, kFftLengthBy2Plus1> mask;
  x_old.fill(0.f);

  for (size_t k = 0; k < 100; ++k) {
    RandomizeSampleVector(&random_generator, x[0]);

    render_delay_buffer->Insert(x);
    if (k == 0) {
      render_delay_buffer->Reset();
    }
    render_delay_buffer->PrepareCaptureProcessing();

    analyzer.Update(*render_delay_buffer->GetRenderBuffer(),
                    rtc::Optional<size_t>(0));
  }

  mask.fill(1.f);
  analyzer.MaskRegionsAroundNarrowBands(&mask);
  EXPECT_TRUE(
      std::all_of(mask.begin(), mask.end(), [](float a) { return a == 1.f; }));
  EXPECT_FALSE(analyzer.PoorSignalExcitation());
}

// Verify that a sinusiod signal is detected as narrow bands.
TEST(RenderSignalAnalyzer, NarrowBandDetection) {
  RenderSignalAnalyzer analyzer;
  Random random_generator(42U);
  std::vector<std::vector<float>> x(3, std::vector<float>(kBlockSize, 0.f));
  std::array<float, kBlockSize> x_old;
  Aec3Fft fft;
  EchoCanceller3Config config;
  config.delay.min_echo_path_delay_blocks = 0;
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(config, 3));

  std::array<float, kFftLengthBy2Plus1> mask;
  x_old.fill(0.f);
  constexpr int kSinusFrequencyBin = 32;

  auto generate_sinusoid_test = [&](bool known_delay) {
    size_t sample_counter = 0;
    for (size_t k = 0; k < 100; ++k) {
      ProduceSinusoid(16000, 16000 / 2 * kSinusFrequencyBin / kFftLengthBy2,
                      &sample_counter, x[0]);

      render_delay_buffer->Insert(x);
      if (k == 0) {
        render_delay_buffer->Reset();
      }
      render_delay_buffer->PrepareCaptureProcessing();

      analyzer.Update(*render_delay_buffer->GetRenderBuffer(),
                      known_delay ? rtc::Optional<size_t>(0) : rtc::nullopt);
    }
  };

  generate_sinusoid_test(true);
  mask.fill(1.f);
  analyzer.MaskRegionsAroundNarrowBands(&mask);
  for (int k = 0; k < static_cast<int>(mask.size()); ++k) {
    EXPECT_EQ(abs(k - kSinusFrequencyBin) <= 2 ? 0.f : 1.f, mask[k]);
  }
  EXPECT_TRUE(analyzer.PoorSignalExcitation());

  // Verify that no bands are detected as narrow when the delay is unknown.
  generate_sinusoid_test(false);
  mask.fill(1.f);
  analyzer.MaskRegionsAroundNarrowBands(&mask);
  std::for_each(mask.begin(), mask.end(), [](float a) { EXPECT_EQ(1.f, a); });
  EXPECT_FALSE(analyzer.PoorSignalExcitation());
}

}  // namespace webrtc
