/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/vad_wrapper.h"

#include <limits>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "api/audio/audio_view.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"
#include "rtc_base/numerics/safe_compare.h"
#include "test/gmock.h"

namespace webrtc {
namespace {

using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::ReturnRoundRobin;
using ::testing::Truly;

constexpr int kNumFramesPerSecond = 100;

constexpr int kNoVadPeriodicReset =
    kFrameDurationMs * (std::numeric_limits<int>::max() / kFrameDurationMs);

constexpr int kSampleRate8kHz = 8000;

class MockVad : public VoiceActivityDetectorWrapper::MonoVad {
 public:
  MOCK_METHOD(int, SampleRateHz, (), (const, override));
  MOCK_METHOD(void, Reset, (), (override));
  MOCK_METHOD(float, Analyze, (rtc::ArrayView<const float> frame), (override));
};

// Checks that the ctor and `Initialize()` read the sample rate of the wrapped
// VAD.
TEST(GainController2VoiceActivityDetectorWrapper, CtorAndInitReadSampleRate) {
  auto vad = std::make_unique<MockVad>();
  EXPECT_CALL(*vad, SampleRateHz)
      .Times(1)
      .WillRepeatedly(Return(kSampleRate8kHz));
  EXPECT_CALL(*vad, Reset).Times(AnyNumber());
  auto vad_wrapper = std::make_unique<VoiceActivityDetectorWrapper>(
      kNoVadPeriodicReset, std::move(vad), kSampleRate8kHz);
}

// Creates a `VoiceActivityDetectorWrapper` injecting a mock VAD that
// repeatedly returns the next value from `speech_probabilities` and that
// restarts from the beginning when after the last element is returned.
std::unique_ptr<VoiceActivityDetectorWrapper> CreateMockVadWrapper(
    int vad_reset_period_ms,
    int sample_rate_hz,
    const std::vector<float>& speech_probabilities,
    int expected_vad_reset_calls) {
  auto vad = std::make_unique<MockVad>();
  EXPECT_CALL(*vad, SampleRateHz)
      .Times(AnyNumber())
      .WillRepeatedly(Return(sample_rate_hz));
  if (expected_vad_reset_calls >= 0) {
    EXPECT_CALL(*vad, Reset).Times(expected_vad_reset_calls);
  }
  EXPECT_CALL(*vad, Analyze)
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRoundRobin(speech_probabilities));
  return std::make_unique<VoiceActivityDetectorWrapper>(
      vad_reset_period_ms, std::move(vad), kSampleRate8kHz);
}

// 10 ms mono frame.
struct FrameWithView {
  // Ctor. Initializes the frame samples with `value`.
  explicit FrameWithView(int sample_rate_hz)
      : samples(rtc::CheckedDivExact(sample_rate_hz, kNumFramesPerSecond),
                0.0f),
        view(samples.data(), samples.size(), /*num_channels=*/1) {}
  std::vector<float> samples;
  const DeinterleavedView<const float> view;
};

// Checks that the expected speech probabilities are returned.
TEST(GainController2VoiceActivityDetectorWrapper, CheckSpeechProbabilities) {
  const std::vector<float> speech_probabilities{0.709f, 0.484f, 0.882f, 0.167f,
                                                0.44f,  0.525f, 0.858f, 0.314f,
                                                0.653f, 0.965f, 0.413f, 0.0f};
  auto vad_wrapper = CreateMockVadWrapper(kNoVadPeriodicReset, kSampleRate8kHz,
                                          speech_probabilities,
                                          /*expected_vad_reset_calls=*/1);
  FrameWithView frame(kSampleRate8kHz);
  for (int i = 0; rtc::SafeLt(i, speech_probabilities.size()); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(speech_probabilities[i], vad_wrapper->Analyze(frame.view));
  }
}

// Checks that the VAD is not periodically reset.
TEST(GainController2VoiceActivityDetectorWrapper, VadNoPeriodicReset) {
  constexpr int kNumFrames = 19;
  auto vad_wrapper = CreateMockVadWrapper(kNoVadPeriodicReset, kSampleRate8kHz,
                                          /*speech_probabilities=*/{1.0f},
                                          /*expected_vad_reset_calls=*/1);
  FrameWithView frame(kSampleRate8kHz);
  for (int i = 0; i < kNumFrames; ++i) {
    vad_wrapper->Analyze(frame.view);
  }
}

class VadPeriodResetParametrization
    : public ::testing::TestWithParam<std::tuple<int, int>> {
 protected:
  int num_frames() const { return std::get<0>(GetParam()); }
  int vad_reset_period_frames() const { return std::get<1>(GetParam()); }
};

// Checks that the VAD is periodically reset with the expected period.
TEST_P(VadPeriodResetParametrization, VadPeriodicReset) {
  auto vad_wrapper = CreateMockVadWrapper(
      /*vad_reset_period_ms=*/vad_reset_period_frames() * kFrameDurationMs,
      kSampleRate8kHz,
      /*speech_probabilities=*/{1.0f},
      /*expected_vad_reset_calls=*/1 +
          num_frames() / vad_reset_period_frames());
  FrameWithView frame(kSampleRate8kHz);
  for (int i = 0; i < num_frames(); ++i) {
    vad_wrapper->Analyze(frame.view);
  }
}

INSTANTIATE_TEST_SUITE_P(GainController2VoiceActivityDetectorWrapper,
                         VadPeriodResetParametrization,
                         ::testing::Combine(::testing::Values(1, 19, 123),
                                            ::testing::Values(2, 5, 20, 53)));

class VadResamplingParametrization
    : public ::testing::TestWithParam<std::tuple<int, int>> {
 protected:
  int input_sample_rate_hz() const { return std::get<0>(GetParam()); }
  int vad_sample_rate_hz() const { return std::get<1>(GetParam()); }
};

// Checks that regardless of the input audio sample rate, the wrapped VAD
// analyzes frames having the expected size, that is according to its internal
// sample rate.
TEST_P(VadResamplingParametrization, CheckResampledFrameSize) {
  auto vad = std::make_unique<MockVad>();
  EXPECT_CALL(*vad, SampleRateHz)
      .Times(AnyNumber())
      .WillRepeatedly(Return(vad_sample_rate_hz()));
  EXPECT_CALL(*vad, Reset).Times(1);
  EXPECT_CALL(*vad, Analyze(Truly([this](rtc::ArrayView<const float> frame) {
    return rtc::SafeEq(frame.size(), rtc::CheckedDivExact(vad_sample_rate_hz(),
                                                          kNumFramesPerSecond));
  }))).Times(1);
  auto vad_wrapper = std::make_unique<VoiceActivityDetectorWrapper>(
      kNoVadPeriodicReset, std::move(vad), input_sample_rate_hz());
  FrameWithView frame(input_sample_rate_hz());
  vad_wrapper->Analyze(frame.view);
}

INSTANTIATE_TEST_SUITE_P(
    GainController2VoiceActivityDetectorWrapper,
    VadResamplingParametrization,
    ::testing::Combine(::testing::Values(8000, 16000, 44100, 48000),
                       ::testing::Values(6000, 8000, 12000, 16000, 24000)));

}  // namespace
}  // namespace webrtc
