/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "api/array_view.h"
#include "modules/audio_processing/agc2/gain_controller2.h"
#include "modules/audio_processing/audio_buffer.h"
#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {

constexpr size_t kFrameSizeMs = 10u;
constexpr size_t kStereo = 2u;

void SetAudioBufferSamples(float value, AudioBuffer* ab) {
  // Sets all the samples in |ab| to |value|.
  for (size_t k = 0; k < ab->num_channels(); ++k) {
    std::fill(ab->channels_f()[k], ab->channels_f()[k] + ab->num_frames(),
              value);
  }
}

}  // namespace

TEST(GainController2, CreateApplyConfig) {
  // Instances GainController2 and applies different configurations.
  std::unique_ptr<GainController2> gain_controller2(new GainController2());

  // Check that the default config is valid.
  AudioProcessing::Config::GainController2 config;
  EXPECT_TRUE(GainController2::Validate(config));
  gain_controller2->ApplyConfig(config);

  // Check that attenuation is not allowed.
  config.fixed_gain_db = -5.f;
  EXPECT_FALSE(GainController2::Validate(config));

  // Check that valid configurations are applied.
  for (const float& fixed_gain_db : {0.f, 5.f, 10.f, 50.f}) {
    config.fixed_gain_db = fixed_gain_db;
    EXPECT_TRUE(GainController2::Validate(config));
    gain_controller2->ApplyConfig(config);
  }
}

TEST(GainController2, ToString) {
  // Tests GainController2::ToString().
  AudioProcessing::Config::GainController2 config;
  config.fixed_gain_db = 5.f;

  config.enabled = false;
  EXPECT_EQ("{enabled: false, fixed_gain_dB: 5}",
            GainController2::ToString(config));

  config.enabled = true;
  EXPECT_EQ("{enabled: true, fixed_gain_dB: 5}",
            GainController2::ToString(config));
}

TEST(GainController2, Usage) {
  // Tests GainController2::Process() on an AudioBuffer instance.
  std::unique_ptr<GainController2> gain_controller2(new GainController2());
  gain_controller2->Initialize(AudioProcessing::kSampleRate48kHz);
  const size_t num_frames = rtc::CheckedDivExact<size_t>(
      kFrameSizeMs * AudioProcessing::kSampleRate48kHz, 1000);
  AudioBuffer ab(num_frames, kStereo, num_frames, kStereo, num_frames);
  constexpr float sample_value = 1000.f;
  SetAudioBufferSamples(sample_value, &ab);
  AudioProcessing::Config::GainController2 config;

  // Check that samples are not modified when the fixed gain is 0 dB.
  ASSERT_EQ(config.fixed_gain_db, 0.f);
  gain_controller2->ApplyConfig(config);
  gain_controller2->Process(&ab);
  EXPECT_EQ(ab.channels_f()[0][0], sample_value);

  // Check that samples are amplified when the fixed gain is greater than 0 dB.
  config.fixed_gain_db = 5.f;
  gain_controller2->ApplyConfig(config);
  gain_controller2->Process(&ab);
  EXPECT_LT(sample_value, ab.channels_f()[0][0]);
}

}  // namespace test
}  // namespace webrtc
