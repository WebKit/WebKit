/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>

#include "webrtc/base/array_view.h"
#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/modules/audio_processing/agc2/gain_controller2.h"
#include "webrtc/modules/audio_processing/agc2/digital_gain_applier.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace test {

namespace {

constexpr size_t kNumFrames = 480u;
constexpr size_t kStereo = 2u;

void SetAudioBufferSamples(float value, AudioBuffer* ab) {
  for (size_t k = 0; k < ab->num_channels(); ++k) {
    auto channel = rtc::ArrayView<float>(ab->channels_f()[k], ab->num_frames());
    for (auto& sample : channel) { sample = value; }
  }
}

template<typename Functor>
bool CheckAudioBufferSamples(Functor validator, AudioBuffer* ab) {
  for (size_t k = 0; k < ab->num_channels(); ++k) {
    auto channel = rtc::ArrayView<float>(ab->channels_f()[k], ab->num_frames());
    for (auto& sample : channel) { if (!validator(sample)) { return false; } }
  }
  return true;
}

bool TestDigitalGainApplier(float sample_value, float gain, float expected) {
  AudioBuffer ab(kNumFrames, kStereo, kNumFrames, kStereo, kNumFrames);
  SetAudioBufferSamples(sample_value, &ab);

  DigitalGainApplier gain_applier;
  for (size_t k = 0; k < ab.num_channels(); ++k) {
    auto channel_view = rtc::ArrayView<float>(
        ab.channels_f()[k], ab.num_frames());
    gain_applier.Process(gain, channel_view);
  }

  auto check_expectation = [expected](float sample) {
      return sample == expected; };
  return CheckAudioBufferSamples(check_expectation, &ab);
}

}  // namespace

TEST(GainController2, Instance) {
  std::unique_ptr<GainController2> gain_controller2;
  gain_controller2.reset(new GainController2(
      AudioProcessing::kSampleRate48kHz));
}

TEST(GainController2, ToString) {
  AudioProcessing::Config config;

  config.gain_controller2.enabled = false;
  EXPECT_EQ("{enabled: false}",
            GainController2::ToString(config.gain_controller2));

  config.gain_controller2.enabled = true;
  EXPECT_EQ("{enabled: true}",
            GainController2::ToString(config.gain_controller2));
}

TEST(GainController2, DigitalGainApplierProcess) {
  EXPECT_TRUE(TestDigitalGainApplier(1000.0f, 0.5, 500.0f));
}

TEST(GainController2, DigitalGainApplierCheckClipping) {
  EXPECT_TRUE(TestDigitalGainApplier(30000.0f, 1.5, 32767.0f));
  EXPECT_TRUE(TestDigitalGainApplier(-30000.0f, 1.5, -32767.0f));
}

TEST(GainController2, Usage) {
  std::unique_ptr<GainController2> gain_controller2;
  gain_controller2.reset(new GainController2(
      AudioProcessing::kSampleRate48kHz));
  AudioBuffer ab(kNumFrames, kStereo, kNumFrames, kStereo, kNumFrames);
  SetAudioBufferSamples(1000.0f, &ab);
  gain_controller2->Process(&ab);
}

}  // namespace test
}  // namespace webrtc
