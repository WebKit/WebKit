/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc/agc_manager_direct.h"

#include <limits>

#include "modules/audio_processing/agc/gain_control.h"
#include "modules/audio_processing/agc/mock_agc.h"
#include "modules/audio_processing/include/mock_audio_processing.h"
#include "rtc_base/strings/string_builder.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace webrtc {
namespace {

constexpr int kSampleRateHz = 32000;
constexpr int kNumChannels = 1;
constexpr int kSamplesPerChannel = kSampleRateHz / 100;
constexpr int kInitialVolume = 128;
constexpr int kClippedMin = 165;  // Arbitrary, but different from the default.
constexpr float kAboveClippedThreshold = 0.2f;
constexpr int kMinMicLevel = 12;
constexpr int kClippedLevelStep = 15;
constexpr float kClippedRatioThreshold = 0.1f;
constexpr int kClippedWaitFrames = 300;

using AnalogAgcConfig =
    AudioProcessing::Config::GainController1::AnalogGainController;
using ClippingPredictorConfig = AudioProcessing::Config::GainController1::
    AnalogGainController::ClippingPredictor;
constexpr AnalogAgcConfig kDefaultAnalogConfig{};

class MockGainControl : public GainControl {
 public:
  virtual ~MockGainControl() {}
  MOCK_METHOD(int, set_stream_analog_level, (int level), (override));
  MOCK_METHOD(int, stream_analog_level, (), (const, override));
  MOCK_METHOD(int, set_mode, (Mode mode), (override));
  MOCK_METHOD(Mode, mode, (), (const, override));
  MOCK_METHOD(int, set_target_level_dbfs, (int level), (override));
  MOCK_METHOD(int, target_level_dbfs, (), (const, override));
  MOCK_METHOD(int, set_compression_gain_db, (int gain), (override));
  MOCK_METHOD(int, compression_gain_db, (), (const, override));
  MOCK_METHOD(int, enable_limiter, (bool enable), (override));
  MOCK_METHOD(bool, is_limiter_enabled, (), (const, override));
  MOCK_METHOD(int,
              set_analog_level_limits,
              (int minimum, int maximum),
              (override));
  MOCK_METHOD(int, analog_level_minimum, (), (const, override));
  MOCK_METHOD(int, analog_level_maximum, (), (const, override));
  MOCK_METHOD(bool, stream_is_saturated, (), (const, override));
};

// TODO(bugs.webrtc.org/12874): Remove and use designated initializers once
// fixed.
std::unique_ptr<AgcManagerDirect> CreateAgcManagerDirect(
    int startup_min_volume,
    int clipped_level_step,
    float clipped_ratio_threshold,
    int clipped_wait_frames,
    const ClippingPredictorConfig& clipping_predictor_config =
        kDefaultAnalogConfig.clipping_predictor) {
  AnalogAgcConfig config;
  config.startup_min_volume = startup_min_volume;
  config.clipped_level_min = kClippedMin;
  config.enable_digital_adaptive = false;
  config.clipped_level_step = clipped_level_step;
  config.clipped_ratio_threshold = clipped_ratio_threshold;
  config.clipped_wait_frames = clipped_wait_frames;
  config.clipping_predictor = clipping_predictor_config;
  return std::make_unique<AgcManagerDirect>(/*num_capture_channels=*/1, config);
}

// Calls `AnalyzePreProcess()` on `manager` `num_calls` times. `peak_ratio` is a
// value in [0, 1] which determines the amplitude of the samples (1 maps to full
// scale). The first half of the calls is made on frames which are half filled
// with zeros in order to simulate a signal with different crest factors.
void CallPreProcessAudioBuffer(int num_calls,
                               float peak_ratio,
                               AgcManagerDirect& manager) {
  RTC_DCHECK_LE(peak_ratio, 1.0f);
  AudioBuffer audio_buffer(kSampleRateHz, 1, kSampleRateHz, 1, kSampleRateHz,
                           1);
  const int num_channels = audio_buffer.num_channels();
  const int num_frames = audio_buffer.num_frames();

  // Make half of the calls with half zeroed frames.
  for (int ch = 0; ch < num_channels; ++ch) {
    // 50% of the samples in one frame are zero.
    for (int i = 0; i < num_frames; i += 2) {
      audio_buffer.channels()[ch][i] = peak_ratio * 32767.0f;
      audio_buffer.channels()[ch][i + 1] = 0.0f;
    }
  }
  for (int n = 0; n < num_calls / 2; ++n) {
    manager.AnalyzePreProcess(&audio_buffer);
  }

  // Make the remaining half of the calls with frames whose samples are all set.
  for (int ch = 0; ch < num_channels; ++ch) {
    for (int i = 0; i < num_frames; ++i) {
      audio_buffer.channels()[ch][i] = peak_ratio * 32767.0f;
    }
  }
  for (int n = 0; n < num_calls - num_calls / 2; ++n) {
    manager.AnalyzePreProcess(&audio_buffer);
  }
}

constexpr char kMinMicLevelFieldTrial[] =
    "WebRTC-Audio-2ndAgcMinMicLevelExperiment";

std::string GetAgcMinMicLevelExperimentFieldTrial(const std::string& value) {
  char field_trial_buffer[64];
  rtc::SimpleStringBuilder builder(field_trial_buffer);
  builder << kMinMicLevelFieldTrial << "/" << value << "/";
  return builder.str();
}

std::string GetAgcMinMicLevelExperimentFieldTrialEnabled(
    int enabled_value,
    const std::string& suffix = "") {
  RTC_DCHECK_GE(enabled_value, 0);
  RTC_DCHECK_LE(enabled_value, 255);
  char field_trial_buffer[64];
  rtc::SimpleStringBuilder builder(field_trial_buffer);
  builder << kMinMicLevelFieldTrial << "/Enabled-" << enabled_value << suffix
          << "/";
  return builder.str();
}

std::string GetAgcMinMicLevelExperimentFieldTrial(
    absl::optional<int> min_mic_level) {
  if (min_mic_level.has_value()) {
    return GetAgcMinMicLevelExperimentFieldTrialEnabled(*min_mic_level);
  }
  return GetAgcMinMicLevelExperimentFieldTrial("Disabled");
}

// (Over)writes `samples_value` for the samples in `audio_buffer`.
// When `clipped_ratio`, a value in [0, 1], is greater than 0, the corresponding
// fraction of the frame is set to a full scale value to simulate clipping.
void WriteAudioBufferSamples(float samples_value,
                             float clipped_ratio,
                             AudioBuffer& audio_buffer) {
  RTC_DCHECK_GE(samples_value, std::numeric_limits<int16_t>::min());
  RTC_DCHECK_LE(samples_value, std::numeric_limits<int16_t>::max());
  RTC_DCHECK_GE(clipped_ratio, 0.0f);
  RTC_DCHECK_LE(clipped_ratio, 1.0f);
  int num_channels = audio_buffer.num_channels();
  int num_samples = audio_buffer.num_frames();
  int num_clipping_samples = clipped_ratio * num_samples;
  for (int ch = 0; ch < num_channels; ++ch) {
    int i = 0;
    for (; i < num_clipping_samples; ++i) {
      audio_buffer.channels()[ch][i] = 32767.0f;
    }
    for (; i < num_samples; ++i) {
      audio_buffer.channels()[ch][i] = samples_value;
    }
  }
}

void CallPreProcessAndProcess(int num_calls,
                              const AudioBuffer& audio_buffer,
                              AgcManagerDirect& manager) {
  for (int n = 0; n < num_calls; ++n) {
    manager.AnalyzePreProcess(&audio_buffer);
    manager.Process(&audio_buffer);
  }
}

}  // namespace

// TODO(bugs.webrtc.org/12874): Use constexpr struct with designated
// initializers once fixed.
constexpr AnalogAgcConfig GetAnalogAgcTestConfig() {
  AnalogAgcConfig config;
  config.startup_min_volume = kInitialVolume;
  config.clipped_level_min = kClippedMin;
  config.enable_digital_adaptive = true;
  config.clipped_level_step = kClippedLevelStep;
  config.clipped_ratio_threshold = kClippedRatioThreshold;
  config.clipped_wait_frames = kClippedWaitFrames;
  config.clipping_predictor = kDefaultAnalogConfig.clipping_predictor;
  return config;
};

class AgcManagerDirectTestHelper {
 public:
  AgcManagerDirectTestHelper()
      : audio_buffer(kSampleRateHz,
                     kNumChannels,
                     kSampleRateHz,
                     kNumChannels,
                     kSampleRateHz,
                     kNumChannels),
        audio(kNumChannels),
        audio_data(kNumChannels * kSamplesPerChannel, 0.0f),
        mock_agc(new MockAgc()),
        manager(GetAnalogAgcTestConfig(), mock_agc) {
    ExpectInitialize();
    manager.Initialize();
    manager.SetupDigitalGainControl(mock_gain_control);
    for (size_t ch = 0; ch < kNumChannels; ++ch) {
      audio[ch] = &audio_data[ch * kSamplesPerChannel];
    }
    WriteAudioBufferSamples(/*samples_value=*/0.0f, /*clipped_ratio=*/0.0f,
                            audio_buffer);
  }

  void FirstProcess() {
    EXPECT_CALL(*mock_agc, Reset()).Times(AtLeast(1));
    EXPECT_CALL(*mock_agc, GetRmsErrorDb(_)).WillOnce(Return(false));
    CallProcess(/*num_calls=*/1);
  }

  void SetVolumeAndProcess(int volume) {
    manager.set_stream_analog_level(volume);
    FirstProcess();
  }

  void ExpectCheckVolumeAndReset(int volume) {
    manager.set_stream_analog_level(volume);
    EXPECT_CALL(*mock_agc, Reset());
  }

  void ExpectInitialize() {
    EXPECT_CALL(mock_gain_control, set_mode(GainControl::kFixedDigital));
    EXPECT_CALL(mock_gain_control, set_target_level_dbfs(2));
    EXPECT_CALL(mock_gain_control, set_compression_gain_db(7));
    EXPECT_CALL(mock_gain_control, enable_limiter(true));
  }

  void CallProcess(int num_calls) {
    for (int i = 0; i < num_calls; ++i) {
      EXPECT_CALL(*mock_agc, Process(_)).WillOnce(Return());
      manager.Process(&audio_buffer);
      absl::optional<int> new_digital_gain = manager.GetDigitalComressionGain();
      if (new_digital_gain) {
        mock_gain_control.set_compression_gain_db(*new_digital_gain);
      }
    }
  }

  void CallPreProc(int num_calls, float clipped_ratio) {
    RTC_DCHECK_GE(clipped_ratio, 0.0f);
    RTC_DCHECK_LE(clipped_ratio, 1.0f);
    const int num_clipped = kSamplesPerChannel * clipped_ratio;
    std::fill(audio_data.begin(), audio_data.end(), 0.0f);
    for (size_t ch = 0; ch < kNumChannels; ++ch) {
      for (int k = 0; k < num_clipped; ++k) {
        audio[ch][k] = 32767.0f;
      }
    }
    for (int i = 0; i < num_calls; ++i) {
      manager.AnalyzePreProcess(audio.data(), kSamplesPerChannel);
    }
  }

  void CallPreProcForChangingAudio(int num_calls, float peak_ratio) {
    RTC_DCHECK_GE(1.0f, peak_ratio);
    std::fill(audio_data.begin(), audio_data.end(), 0.0f);
    for (size_t ch = 0; ch < kNumChannels; ++ch) {
      for (size_t k = 0; k < kSamplesPerChannel; k += 2) {
        audio[ch][k] = peak_ratio * 32767.0f;
      }
    }
    for (int i = 0; i < num_calls / 2; ++i) {
      manager.AnalyzePreProcess(audio.data(), kSamplesPerChannel);
    }
    for (size_t ch = 0; ch < kNumChannels; ++ch) {
      for (size_t k = 0; k < kSamplesPerChannel; ++k) {
        audio[ch][k] = peak_ratio * 32767.0f;
      }
    }
    for (int i = 0; i < num_calls - num_calls / 2; ++i) {
      manager.AnalyzePreProcess(audio.data(), kSamplesPerChannel);
    }
  }

  AudioBuffer audio_buffer;
  std::vector<float*> audio;
  std::vector<float> audio_data;

  MockAgc* mock_agc;
  AgcManagerDirect manager;
  MockGainControl mock_gain_control;
};

class AgcManagerDirectParametrizedTest
    : public ::testing::TestWithParam<absl::optional<int>> {
 protected:
  AgcManagerDirectParametrizedTest()
      : field_trials_(GetAgcMinMicLevelExperimentFieldTrial(GetParam())) {}

  bool IsMinMicLevelOverridden() const { return GetParam().has_value(); }
  int GetMinMicLevel() const { return GetParam().value_or(kMinMicLevel); }

 private:
  test::ScopedFieldTrials field_trials_;
};

INSTANTIATE_TEST_SUITE_P(,
                         AgcManagerDirectParametrizedTest,
                         testing::Values(absl::nullopt, 12, 20));

TEST_P(AgcManagerDirectParametrizedTest,
       StartupMinVolumeConfigurationIsRespected) {
  AgcManagerDirectTestHelper helper;
  helper.FirstProcess();
  EXPECT_EQ(kInitialVolume, helper.manager.stream_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, MicVolumeResponseToRmsError) {
  AgcManagerDirectTestHelper helper;
  helper.FirstProcess();

  // Compressor default; no residual error.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(5), Return(true)));
  helper.CallProcess(/*num_calls=*/1);

  // Inside the compressor's window; no change of volume.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)));
  helper.CallProcess(/*num_calls=*/1);

  // Above the compressor's window; volume should be increased.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(130, helper.manager.stream_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(20), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(168, helper.manager.stream_analog_level());

  // Inside the compressor's window; no change of volume.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(5), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)));
  helper.CallProcess(/*num_calls=*/1);

  // Below the compressor's window; volume should be decreased.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(167, helper.manager.stream_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(163, helper.manager.stream_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-9), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(129, helper.manager.stream_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, MicVolumeIsLimited) {
  AgcManagerDirectTestHelper helper;
  helper.FirstProcess();

  // Maximum upwards change is limited.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(30), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(183, helper.manager.stream_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(30), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(243, helper.manager.stream_analog_level());

  // Won't go higher than the maximum.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(30), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(255, helper.manager.stream_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(254, helper.manager.stream_analog_level());

  // Maximum downwards change is limited.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(194, helper.manager.stream_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(137, helper.manager.stream_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(88, helper.manager.stream_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(54, helper.manager.stream_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(33, helper.manager.stream_analog_level());

  // Won't go lower than the minimum.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(std::max(18, GetMinMicLevel()),
            helper.manager.stream_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(std::max(12, GetMinMicLevel()),
            helper.manager.stream_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, CompressorStepsTowardsTarget) {
  AgcManagerDirectTestHelper helper;
  helper.FirstProcess();

  // Compressor default; no call to set_compression_gain_db.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(5), Return(true)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/20);

  // Moves slowly upwards.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(9), Return(true)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/19);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(8))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/1);

  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/19);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(9))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/1);

  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/20);

  // Moves slowly downward, then reverses before reaching the original target.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(5), Return(true)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/19);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(8))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/1);

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(9), Return(true)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/19);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(9))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/1);

  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/20);
}

TEST_P(AgcManagerDirectParametrizedTest, CompressorErrorIsDeemphasized) {
  AgcManagerDirectTestHelper helper;
  helper.FirstProcess();

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)))
      .WillRepeatedly(Return(false));
  helper.CallProcess(/*num_calls=*/19);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(8))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(9))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/20);

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)))
      .WillRepeatedly(Return(false));
  helper.CallProcess(/*num_calls=*/19);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(8))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(7))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(6))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/20);
}

TEST_P(AgcManagerDirectParametrizedTest, CompressorReachesMaximum) {
  AgcManagerDirectTestHelper helper;
  helper.FirstProcess();

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)))
      .WillRepeatedly(Return(false));
  helper.CallProcess(/*num_calls=*/19);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(8))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(9))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(10))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(11))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(12))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/1);
}

TEST_P(AgcManagerDirectParametrizedTest, CompressorReachesMinimum) {
  AgcManagerDirectTestHelper helper;
  helper.FirstProcess();

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)))
      .WillRepeatedly(Return(false));
  helper.CallProcess(/*num_calls=*/19);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(6))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(5))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(4))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(3))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(2))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/1);
}

TEST_P(AgcManagerDirectParametrizedTest, NoActionWhileMuted) {
  AgcManagerDirectTestHelper helper;
  helper.manager.HandleCaptureOutputUsedChange(false);
  helper.manager.Process(&helper.audio_buffer);
  absl::optional<int> new_digital_gain =
      helper.manager.GetDigitalComressionGain();
  if (new_digital_gain) {
    helper.mock_gain_control.set_compression_gain_db(*new_digital_gain);
  }
}

TEST_P(AgcManagerDirectParametrizedTest, UnmutingChecksVolumeWithoutRaising) {
  AgcManagerDirectTestHelper helper;
  helper.FirstProcess();

  helper.manager.HandleCaptureOutputUsedChange(false);
  helper.manager.HandleCaptureOutputUsedChange(true);
  helper.ExpectCheckVolumeAndReset(/*volume=*/127);
  // SetMicVolume should not be called.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_)).WillOnce(Return(false));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(127, helper.manager.stream_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, UnmutingRaisesTooLowVolume) {
  AgcManagerDirectTestHelper helper;
  helper.FirstProcess();

  helper.manager.HandleCaptureOutputUsedChange(false);
  helper.manager.HandleCaptureOutputUsedChange(true);
  helper.ExpectCheckVolumeAndReset(/*volume=*/11);
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_)).WillOnce(Return(false));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(GetMinMicLevel(), helper.manager.stream_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest,
       ManualLevelChangeResultsInNoSetMicCall) {
  AgcManagerDirectTestHelper helper;
  helper.FirstProcess();

  // Change outside of compressor's range, which would normally trigger a call
  // to `SetMicVolume()`.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)));

  // When the analog volume changes, the gain controller is reset.
  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));

  // GetMicVolume returns a value outside of the quantization slack, indicating
  // a manual volume change.
  ASSERT_NE(helper.manager.stream_analog_level(), 154);
  helper.manager.set_stream_analog_level(154);
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(154, helper.manager.stream_analog_level());

  // Do the same thing, except downwards now.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  helper.manager.set_stream_analog_level(100);
  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(100, helper.manager.stream_analog_level());

  // And finally verify the AGC continues working without a manual change.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(99, helper.manager.stream_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest,
       RecoveryAfterManualLevelChangeFromMax) {
  AgcManagerDirectTestHelper helper;
  helper.FirstProcess();

  // Force the mic up to max volume. Takes a few steps due to the residual
  // gain limitation.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(30), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(183, helper.manager.stream_analog_level());
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(243, helper.manager.stream_analog_level());
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(255, helper.manager.stream_analog_level());

  // Manual change does not result in SetMicVolume call.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  helper.manager.set_stream_analog_level(50);
  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(50, helper.manager.stream_analog_level());

  // Continues working as usual afterwards.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(20), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(69, helper.manager.stream_analog_level());
}

// Checks that, when the min mic level override is not specified, AGC ramps up
// towards the minimum mic level after the mic level is manually set below the
// minimum gain to enforce.
TEST(AgcManagerDirectTest, RecoveryAfterManualLevelChangeBelowMin) {
  AgcManagerDirectTestHelper helper;
  helper.FirstProcess();

  // Manual change below min, but strictly positive, otherwise
  // AGC won't take any action.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  helper.manager.set_stream_analog_level(1);
  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(1, helper.manager.stream_analog_level());

  // Continues working as usual afterwards.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(2, helper.manager.stream_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(30), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(11, helper.manager.stream_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(20), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(18, helper.manager.stream_analog_level());
}

// Checks that, when the min mic level override is specified, AGC immediately
// applies the minimum mic level after the mic level is manually set below the
// minimum gain to enforce.
TEST_P(AgcManagerDirectParametrizedTest,
       RecoveryAfterManualLevelChangeBelowMin) {
  if (!IsMinMicLevelOverridden()) {
    GTEST_SKIP() << "Skipped. Min mic level not overridden.";
  }

  AgcManagerDirectTestHelper helper;
  helper.FirstProcess();

  // Manual change below min, but strictly positive, otherwise
  // AGC won't take any action.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  helper.manager.set_stream_analog_level(1);
  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(GetMinMicLevel(), helper.manager.stream_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, NoClippingHasNoImpact) {
  AgcManagerDirectTestHelper helper;
  helper.FirstProcess();

  helper.CallPreProc(/*num_calls=*/100, /*clipped_ratio=*/0);
  EXPECT_EQ(128, helper.manager.stream_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, ClippingUnderThresholdHasNoImpact) {
  AgcManagerDirectTestHelper helper;
  helper.FirstProcess();

  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/0.099);
  EXPECT_EQ(128, helper.manager.stream_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, ClippingLowersVolume) {
  AgcManagerDirectTestHelper helper;
  helper.SetVolumeAndProcess(/*volume=*/255);

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/0.2);
  EXPECT_EQ(240, helper.manager.stream_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, WaitingPeriodBetweenClippingChecks) {
  AgcManagerDirectTestHelper helper;
  helper.SetVolumeAndProcess(255);

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(240, helper.manager.stream_analog_level());

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(0);
  helper.CallPreProc(/*num_calls=*/300,
                     /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(240, helper.manager.stream_analog_level());

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(225, helper.manager.stream_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, ClippingLoweringIsLimited) {
  AgcManagerDirectTestHelper helper;
  helper.SetVolumeAndProcess(/*volume=*/180);

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(kClippedMin, helper.manager.stream_analog_level());

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(0);
  helper.CallPreProc(/*num_calls=*/1000,
                     /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(kClippedMin, helper.manager.stream_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest,
       ClippingMaxIsRespectedWhenEqualToLevel) {
  AgcManagerDirectTestHelper helper;
  helper.SetVolumeAndProcess(/*volume=*/255);

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(240, helper.manager.stream_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(30), Return(true)));
  helper.CallProcess(/*num_calls=*/10);
  EXPECT_EQ(240, helper.manager.stream_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest,
       ClippingMaxIsRespectedWhenHigherThanLevel) {
  AgcManagerDirectTestHelper helper;
  helper.SetVolumeAndProcess(/*volume=*/200);

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(185, helper.manager.stream_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(40), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(240, helper.manager.stream_analog_level());
  helper.CallProcess(/*num_calls=*/10);
  EXPECT_EQ(240, helper.manager.stream_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest,
       MaxCompressionIsIncreasedAfterClipping) {
  AgcManagerDirectTestHelper helper;
  helper.SetVolumeAndProcess(/*volume=*/210);

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallPreProc(/*num_calls=*/1, kAboveClippedThreshold);
  EXPECT_EQ(195, helper.manager.stream_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)))
      .WillRepeatedly(Return(false));
  helper.CallProcess(/*num_calls=*/19);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(8))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(9))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(10))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(11))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(12))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(13))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/1);

  // Continue clipping until we hit the maximum surplus compression.
  helper.CallPreProc(/*num_calls=*/300,
                     /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(180, helper.manager.stream_analog_level());

  helper.CallPreProc(/*num_calls=*/300,
                     /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallPreProc(1, kAboveClippedThreshold);
  EXPECT_EQ(kClippedMin, helper.manager.stream_analog_level());

  // Current level is now at the minimum, but the maximum allowed level still
  // has more to decrease.
  helper.CallPreProc(/*num_calls=*/300,
                     /*clipped_ratio=*/kAboveClippedThreshold);
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);

  helper.CallPreProc(/*num_calls=*/300,
                     /*clipped_ratio=*/kAboveClippedThreshold);
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);

  helper.CallPreProc(/*num_calls=*/300,
                     /*clipped_ratio=*/kAboveClippedThreshold);
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(16), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(16), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(16), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(16), Return(true)))
      .WillRepeatedly(Return(false));
  helper.CallProcess(/*num_calls=*/19);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(14))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(15))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(16))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(17))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(18))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/1);
}

TEST_P(AgcManagerDirectParametrizedTest, UserCanRaiseVolumeAfterClipping) {
  AgcManagerDirectTestHelper helper;
  helper.SetVolumeAndProcess(/*volume=*/225);

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(210, helper.manager.stream_analog_level());

  // High enough error to trigger a volume check.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(14), Return(true)));
  // User changed the volume.
  helper.manager.set_stream_analog_level(250);
  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(250, helper.manager.stream_analog_level());

  // Move down...
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-10), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(210, helper.manager.stream_analog_level());
  // And back up to the new max established by the user.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(40), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(250, helper.manager.stream_analog_level());
  // Will not move above new maximum.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(30), Return(true)));
  helper.CallProcess(/*num_calls=*/1);
  EXPECT_EQ(250, helper.manager.stream_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, ClippingDoesNotPullLowVolumeBackUp) {
  AgcManagerDirectTestHelper helper;
  helper.SetVolumeAndProcess(/*volume=*/80);

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(0);
  int initial_volume = helper.manager.stream_analog_level();
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(initial_volume, helper.manager.stream_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, TakesNoActionOnZeroMicVolume) {
  AgcManagerDirectTestHelper helper;
  helper.FirstProcess();

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(30), Return(true)));
  helper.manager.set_stream_analog_level(0);
  helper.CallProcess(/*num_calls=*/10);
  EXPECT_EQ(0, helper.manager.stream_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, ClippingDetectionLowersVolume) {
  AgcManagerDirectTestHelper helper;
  helper.SetVolumeAndProcess(/*volume=*/255);
  EXPECT_EQ(255, helper.manager.stream_analog_level());
  helper.CallPreProcForChangingAudio(/*num_calls=*/100, /*peak_ratio=*/0.99f);
  EXPECT_EQ(255, helper.manager.stream_analog_level());
  helper.CallPreProcForChangingAudio(/*num_calls=*/100, /*peak_ratio=*/1.0f);
  EXPECT_EQ(240, helper.manager.stream_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest,
       DisabledClippingPredictorDoesNotLowerVolume) {
  AgcManagerDirectTestHelper helper;
  helper.SetVolumeAndProcess(/*volume=*/255);
  EXPECT_FALSE(helper.manager.clipping_predictor_enabled());
  EXPECT_EQ(255, helper.manager.stream_analog_level());
  helper.CallPreProcForChangingAudio(/*num_calls=*/100, /*peak_ratio=*/0.99f);
  EXPECT_EQ(255, helper.manager.stream_analog_level());
  helper.CallPreProcForChangingAudio(/*num_calls=*/100, /*peak_ratio=*/0.99f);
  EXPECT_EQ(255, helper.manager.stream_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, DisableDigitalDisablesDigital) {
  auto agc = std::unique_ptr<Agc>(new ::testing::NiceMock<MockAgc>());
  MockGainControl mock_gain_control;
  EXPECT_CALL(mock_gain_control, set_mode(GainControl::kFixedDigital));
  EXPECT_CALL(mock_gain_control, set_target_level_dbfs(0));
  EXPECT_CALL(mock_gain_control, set_compression_gain_db(0));
  EXPECT_CALL(mock_gain_control, enable_limiter(false));

  AnalogAgcConfig config;
  config.enable_digital_adaptive = false;
  auto manager = std::make_unique<AgcManagerDirect>(kNumChannels, config);
  manager->Initialize();
  manager->SetupDigitalGainControl(mock_gain_control);
}

TEST(AgcManagerDirectTest, AgcMinMicLevelExperimentDefault) {
  std::unique_ptr<AgcManagerDirect> manager =
      CreateAgcManagerDirect(kInitialVolume, kClippedLevelStep,
                             kClippedRatioThreshold, kClippedWaitFrames);
  EXPECT_EQ(manager->channel_agcs_[0]->min_mic_level(), kMinMicLevel);
  EXPECT_EQ(manager->channel_agcs_[0]->startup_min_level(), kInitialVolume);
}

TEST(AgcManagerDirectTest, AgcMinMicLevelExperimentDisabled) {
  for (const std::string& field_trial_suffix : {"", "_20220210"}) {
    test::ScopedFieldTrials field_trial(
        GetAgcMinMicLevelExperimentFieldTrial("Disabled" + field_trial_suffix));
    std::unique_ptr<AgcManagerDirect> manager =
        CreateAgcManagerDirect(kInitialVolume, kClippedLevelStep,
                               kClippedRatioThreshold, kClippedWaitFrames);
    EXPECT_EQ(manager->channel_agcs_[0]->min_mic_level(), kMinMicLevel);
    EXPECT_EQ(manager->channel_agcs_[0]->startup_min_level(), kInitialVolume);
  }
}

// Checks that a field-trial parameter outside of the valid range [0,255] is
// ignored.
TEST(AgcManagerDirectTest, AgcMinMicLevelExperimentOutOfRangeAbove) {
  test::ScopedFieldTrials field_trial(
      GetAgcMinMicLevelExperimentFieldTrial("Enabled-256"));
  std::unique_ptr<AgcManagerDirect> manager =
      CreateAgcManagerDirect(kInitialVolume, kClippedLevelStep,
                             kClippedRatioThreshold, kClippedWaitFrames);
  EXPECT_EQ(manager->channel_agcs_[0]->min_mic_level(), kMinMicLevel);
  EXPECT_EQ(manager->channel_agcs_[0]->startup_min_level(), kInitialVolume);
}

// Checks that a field-trial parameter outside of the valid range [0,255] is
// ignored.
TEST(AgcManagerDirectTest, AgcMinMicLevelExperimentOutOfRangeBelow) {
  test::ScopedFieldTrials field_trial(
      GetAgcMinMicLevelExperimentFieldTrial("Enabled--1"));
  std::unique_ptr<AgcManagerDirect> manager =
      CreateAgcManagerDirect(kInitialVolume, kClippedLevelStep,
                             kClippedRatioThreshold, kClippedWaitFrames);
  EXPECT_EQ(manager->channel_agcs_[0]->min_mic_level(), kMinMicLevel);
  EXPECT_EQ(manager->channel_agcs_[0]->startup_min_level(), kInitialVolume);
}

// Verifies that a valid experiment changes the minimum microphone level. The
// start volume is larger than the min level and should therefore not be
// changed.
TEST(AgcManagerDirectTest, AgcMinMicLevelExperimentEnabled50) {
  constexpr int kMinMicLevelOverride = 50;
  for (const std::string& field_trial_suffix : {"", "_20220210"}) {
    SCOPED_TRACE(field_trial_suffix);
    test::ScopedFieldTrials field_trial(
        GetAgcMinMicLevelExperimentFieldTrialEnabled(kMinMicLevelOverride,
                                                     field_trial_suffix));
    std::unique_ptr<AgcManagerDirect> manager =
        CreateAgcManagerDirect(kInitialVolume, kClippedLevelStep,
                               kClippedRatioThreshold, kClippedWaitFrames);
    EXPECT_EQ(manager->channel_agcs_[0]->min_mic_level(), kMinMicLevelOverride);
    EXPECT_EQ(manager->channel_agcs_[0]->startup_min_level(), kInitialVolume);
  }
}

// Checks that, when the "WebRTC-Audio-AgcMinMicLevelExperiment" field trial is
// specified with a valid value, the mic level never gets lowered beyond the
// override value in the presence of clipping.
TEST(AgcManagerDirectTest, AgcMinMicLevelExperimentCheckMinLevelWithClipping) {
  constexpr int kMinMicLevelOverride = 250;

  // Create and initialize two AGCs by specifying and leaving unspecified the
  // relevant field trial.
  const auto factory = []() {
    std::unique_ptr<AgcManagerDirect> manager =
        CreateAgcManagerDirect(kInitialVolume, kClippedLevelStep,
                               kClippedRatioThreshold, kClippedWaitFrames);
    manager->Initialize();
    manager->set_stream_analog_level(kInitialVolume);
    return manager;
  };
  std::unique_ptr<AgcManagerDirect> manager = factory();
  std::unique_ptr<AgcManagerDirect> manager_with_override;
  {
    test::ScopedFieldTrials field_trial(
        GetAgcMinMicLevelExperimentFieldTrialEnabled(kMinMicLevelOverride));
    manager_with_override = factory();
  }

  // Create a test input signal which containts 80% of clipped samples.
  AudioBuffer audio_buffer(kSampleRateHz, 1, kSampleRateHz, 1, kSampleRateHz,
                           1);
  WriteAudioBufferSamples(/*samples_value=*/4000.0f, /*clipped_ratio=*/0.8f,
                          audio_buffer);

  // Simulate 4 seconds of clipping; it is expected to trigger a downward
  // adjustment of the analog gain.
  CallPreProcessAndProcess(/*num_calls=*/400, audio_buffer, *manager);
  CallPreProcessAndProcess(/*num_calls=*/400, audio_buffer,
                           *manager_with_override);

  // Make sure that an adaptation occurred.
  ASSERT_GT(manager->stream_analog_level(), 0);

  // Check that the test signal triggers a larger downward adaptation for
  // `manager`, which is allowed to reach a lower gain.
  EXPECT_GT(manager_with_override->stream_analog_level(),
            manager->stream_analog_level());
  // Check that the gain selected by `manager_with_override` equals the minimum
  // value overridden via field trial.
  EXPECT_EQ(manager_with_override->stream_analog_level(), kMinMicLevelOverride);
}

// Checks that, when the "WebRTC-Audio-AgcMinMicLevelExperiment" field trial is
// specified with a value lower than the `clipped_level_min`, the behavior of
// the analog gain controller is the same as that obtained when the field trial
// is not specified.
TEST(AgcManagerDirectTest,
     AgcMinMicLevelExperimentCompareMicLevelWithClipping) {
  // Create and initialize two AGCs by specifying and leaving unspecified the
  // relevant field trial.
  const auto factory = []() {
    // Use a large clipped level step to more quickly decrease the analog gain
    // with clipping.
    AnalogAgcConfig config = kDefaultAnalogConfig;
    config.startup_min_volume = kInitialVolume;
    config.enable_digital_adaptive = false;
    config.clipped_level_step = 64;
    config.clipped_ratio_threshold = kClippedRatioThreshold;
    config.clipped_wait_frames = kClippedWaitFrames;
    auto controller =
        std::make_unique<AgcManagerDirect>(/*num_capture_channels=*/1, config);
    controller->Initialize();
    controller->set_stream_analog_level(kInitialVolume);
    return controller;
  };
  std::unique_ptr<AgcManagerDirect> manager = factory();
  std::unique_ptr<AgcManagerDirect> manager_with_override;
  {
    constexpr int kMinMicLevelOverride = 20;
    static_assert(
        kDefaultAnalogConfig.clipped_level_min >= kMinMicLevelOverride,
        "Use a lower override value.");
    test::ScopedFieldTrials field_trial(
        GetAgcMinMicLevelExperimentFieldTrialEnabled(kMinMicLevelOverride));
    manager_with_override = factory();
  }

  // Create a test input signal which containts 80% of clipped samples.
  AudioBuffer audio_buffer(kSampleRateHz, 1, kSampleRateHz, 1, kSampleRateHz,
                           1);
  WriteAudioBufferSamples(/*samples_value=*/4000.0f, /*clipped_ratio=*/0.8f,
                          audio_buffer);

  // Simulate 4 seconds of clipping; it is expected to trigger a downward
  // adjustment of the analog gain.
  CallPreProcessAndProcess(/*num_calls=*/400, audio_buffer, *manager);
  CallPreProcessAndProcess(/*num_calls=*/400, audio_buffer,
                           *manager_with_override);

  // Make sure that an adaptation occurred.
  ASSERT_GT(manager->stream_analog_level(), 0);

  // Check that the selected analog gain is the same for both controllers and
  // that it equals the minimum level reached when clipping is handled. That is
  // expected because the minimum microphone level override is less than the
  // minimum level used when clipping is detected.
  EXPECT_EQ(manager->stream_analog_level(),
            manager_with_override->stream_analog_level());
  EXPECT_EQ(manager_with_override->stream_analog_level(),
            kDefaultAnalogConfig.clipped_level_min);
}

// TODO(bugs.webrtc.org/12774): Test the bahavior of `clipped_level_step`.
// TODO(bugs.webrtc.org/12774): Test the bahavior of `clipped_ratio_threshold`.
// TODO(bugs.webrtc.org/12774): Test the bahavior of `clipped_wait_frames`.
// Verifies that configurable clipping parameters are initialized as intended.
TEST_P(AgcManagerDirectParametrizedTest, ClippingParametersVerified) {
  std::unique_ptr<AgcManagerDirect> manager =
      CreateAgcManagerDirect(kInitialVolume, kClippedLevelStep,
                             kClippedRatioThreshold, kClippedWaitFrames);
  manager->Initialize();
  EXPECT_EQ(manager->clipped_level_step_, kClippedLevelStep);
  EXPECT_EQ(manager->clipped_ratio_threshold_, kClippedRatioThreshold);
  EXPECT_EQ(manager->clipped_wait_frames_, kClippedWaitFrames);
  std::unique_ptr<AgcManagerDirect> manager_custom =
      CreateAgcManagerDirect(kInitialVolume,
                             /*clipped_level_step=*/10,
                             /*clipped_ratio_threshold=*/0.2f,
                             /*clipped_wait_frames=*/50);
  manager_custom->Initialize();
  EXPECT_EQ(manager_custom->clipped_level_step_, 10);
  EXPECT_EQ(manager_custom->clipped_ratio_threshold_, 0.2f);
  EXPECT_EQ(manager_custom->clipped_wait_frames_, 50);
}

TEST_P(AgcManagerDirectParametrizedTest,
       DisableClippingPredictorDisablesClippingPredictor) {
  // TODO(bugs.webrtc.org/12874): Use designated initializers once fixed.
  ClippingPredictorConfig config;
  config.enabled = false;

  std::unique_ptr<AgcManagerDirect> manager = CreateAgcManagerDirect(
      kInitialVolume, kClippedLevelStep, kClippedRatioThreshold,
      kClippedWaitFrames, config);
  manager->Initialize();
  EXPECT_FALSE(manager->clipping_predictor_enabled());
  EXPECT_FALSE(manager->use_clipping_predictor_step());
}

TEST_P(AgcManagerDirectParametrizedTest, ClippingPredictorDisabledByDefault) {
  constexpr ClippingPredictorConfig kDefaultConfig;
  EXPECT_FALSE(kDefaultConfig.enabled);
}

TEST_P(AgcManagerDirectParametrizedTest,
       EnableClippingPredictorEnablesClippingPredictor) {
  // TODO(bugs.webrtc.org/12874): Use designated initializers once fixed.
  ClippingPredictorConfig config;
  config.enabled = true;
  config.use_predicted_step = true;

  std::unique_ptr<AgcManagerDirect> manager = CreateAgcManagerDirect(
      kInitialVolume, kClippedLevelStep, kClippedRatioThreshold,
      kClippedWaitFrames, config);
  manager->Initialize();
  EXPECT_TRUE(manager->clipping_predictor_enabled());
  EXPECT_TRUE(manager->use_clipping_predictor_step());
}

TEST_P(AgcManagerDirectParametrizedTest,
       DisableClippingPredictorDoesNotLowerVolume) {
  AudioBuffer audio_buffer(kSampleRateHz, kNumChannels, kSampleRateHz,
                           kNumChannels, kSampleRateHz, kNumChannels);

  AnalogAgcConfig config = GetAnalogAgcTestConfig();
  config.clipping_predictor.enabled = false;
  AgcManagerDirect manager(config, new ::testing::NiceMock<MockAgc>());
  manager.Initialize();
  manager.set_stream_analog_level(/*level=*/255);
  EXPECT_FALSE(manager.clipping_predictor_enabled());
  EXPECT_FALSE(manager.use_clipping_predictor_step());
  EXPECT_EQ(manager.stream_analog_level(), 255);
  manager.Process(&audio_buffer);
  CallPreProcessAudioBuffer(/*num_calls=*/10, /*peak_ratio=*/0.99f, manager);
  EXPECT_EQ(manager.stream_analog_level(), 255);
  CallPreProcessAudioBuffer(/*num_calls=*/300, /*peak_ratio=*/0.99f, manager);
  EXPECT_EQ(manager.stream_analog_level(), 255);
  CallPreProcessAudioBuffer(/*num_calls=*/10, /*peak_ratio=*/0.99f, manager);
  EXPECT_EQ(manager.stream_analog_level(), 255);
}

TEST_P(AgcManagerDirectParametrizedTest,
       UsedClippingPredictionsProduceLowerAnalogLevels) {
  AudioBuffer audio_buffer(kSampleRateHz, kNumChannels, kSampleRateHz,
                           kNumChannels, kSampleRateHz, kNumChannels);

  AnalogAgcConfig config_with_prediction = GetAnalogAgcTestConfig();
  config_with_prediction.clipping_predictor.enabled = true;
  config_with_prediction.clipping_predictor.use_predicted_step = true;
  AnalogAgcConfig config_without_prediction = GetAnalogAgcTestConfig();
  config_without_prediction.clipping_predictor.enabled = false;
  AgcManagerDirect manager_with_prediction(config_with_prediction,
                                           new ::testing::NiceMock<MockAgc>());
  AgcManagerDirect manager_without_prediction(
      config_without_prediction, new ::testing::NiceMock<MockAgc>());

  manager_with_prediction.Initialize();
  manager_without_prediction.Initialize();

  constexpr int kInitialLevel = 255;
  constexpr float kClippingPeakRatio = 1.0f;
  constexpr float kCloseToClippingPeakRatio = 0.99f;
  constexpr float kZeroPeakRatio = 0.0f;
  manager_with_prediction.set_stream_analog_level(kInitialLevel);
  manager_without_prediction.set_stream_analog_level(kInitialLevel);
  manager_with_prediction.Process(&audio_buffer);
  manager_without_prediction.Process(&audio_buffer);
  EXPECT_TRUE(manager_with_prediction.clipping_predictor_enabled());
  EXPECT_FALSE(manager_without_prediction.clipping_predictor_enabled());
  EXPECT_TRUE(manager_with_prediction.use_clipping_predictor_step());
  EXPECT_EQ(manager_with_prediction.stream_analog_level(), kInitialLevel);
  EXPECT_EQ(manager_without_prediction.stream_analog_level(), kInitialLevel);

  // Expect a change in the analog level when the prediction step is used.
  CallPreProcessAudioBuffer(/*num_calls=*/10, kCloseToClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(/*num_calls=*/10, kCloseToClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.stream_analog_level(),
            kInitialLevel - kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.stream_analog_level(), kInitialLevel);

  // Expect no change during waiting.
  CallPreProcessAudioBuffer(kClippedWaitFrames, kCloseToClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(kClippedWaitFrames, kCloseToClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.stream_analog_level(),
            kInitialLevel - kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.stream_analog_level(), kInitialLevel);

  // Expect a change when the prediction step is used.
  CallPreProcessAudioBuffer(/*num_calls=*/10, kCloseToClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(/*num_calls=*/10, kCloseToClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.stream_analog_level(),
            kInitialLevel - 2 * kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.stream_analog_level(), kInitialLevel);

  // Expect no change when clipping is not detected or predicted.
  CallPreProcessAudioBuffer(2 * kClippedWaitFrames, kZeroPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(2 * kClippedWaitFrames, kZeroPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.stream_analog_level(),
            kInitialLevel - 2 * kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.stream_analog_level(), kInitialLevel);

  // Expect a change for clipping frames.
  CallPreProcessAudioBuffer(/*num_calls=*/1, kClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(/*num_calls=*/1, kClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.stream_analog_level(),
            kInitialLevel - 3 * kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.stream_analog_level(),
            kInitialLevel - kClippedLevelStep);

  // Expect no change during waiting.
  CallPreProcessAudioBuffer(kClippedWaitFrames, kClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(kClippedWaitFrames, kClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.stream_analog_level(),
            kInitialLevel - 3 * kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.stream_analog_level(),
            kInitialLevel - kClippedLevelStep);

  // Expect a change for clipping frames.
  CallPreProcessAudioBuffer(/*num_calls=*/1, kClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(/*num_calls=*/1, kClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.stream_analog_level(),
            kInitialLevel - 4 * kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.stream_analog_level(),
            kInitialLevel - 2 * kClippedLevelStep);
}

TEST_P(AgcManagerDirectParametrizedTest,
       UnusedClippingPredictionsProduceEqualAnalogLevels) {
  AudioBuffer audio_buffer(kSampleRateHz, kNumChannels, kSampleRateHz,
                           kNumChannels, kSampleRateHz, kNumChannels);

  AnalogAgcConfig config_with_prediction = GetAnalogAgcTestConfig();
  config_with_prediction.clipping_predictor.enabled = true;
  config_with_prediction.clipping_predictor.use_predicted_step = false;
  AnalogAgcConfig config_without_prediction = GetAnalogAgcTestConfig();
  config_without_prediction.clipping_predictor.enabled = false;
  AgcManagerDirect manager_with_prediction(config_with_prediction,
                                           new ::testing::NiceMock<MockAgc>());
  AgcManagerDirect manager_without_prediction(
      config_without_prediction, new ::testing::NiceMock<MockAgc>());

  constexpr int kInitialLevel = 255;
  constexpr float kClippingPeakRatio = 1.0f;
  constexpr float kCloseToClippingPeakRatio = 0.99f;
  constexpr float kZeroPeakRatio = 0.0f;
  manager_with_prediction.Initialize();
  manager_without_prediction.Initialize();
  manager_with_prediction.set_stream_analog_level(kInitialLevel);
  manager_without_prediction.set_stream_analog_level(kInitialLevel);
  manager_with_prediction.Process(&audio_buffer);
  manager_without_prediction.Process(&audio_buffer);
  EXPECT_TRUE(manager_with_prediction.clipping_predictor_enabled());
  EXPECT_FALSE(manager_without_prediction.clipping_predictor_enabled());
  EXPECT_FALSE(manager_with_prediction.use_clipping_predictor_step());
  EXPECT_EQ(manager_with_prediction.stream_analog_level(), kInitialLevel);
  EXPECT_EQ(manager_without_prediction.stream_analog_level(), kInitialLevel);

  // Expect no change in the analog level for non-clipping frames.
  CallPreProcessAudioBuffer(/*num_calls=*/10, kCloseToClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(/*num_calls=*/10, kCloseToClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.stream_analog_level(),
            manager_without_prediction.stream_analog_level());

  // Expect no change for non-clipping frames.
  CallPreProcessAudioBuffer(kClippedWaitFrames, kCloseToClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(kClippedWaitFrames, kCloseToClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.stream_analog_level(),
            manager_without_prediction.stream_analog_level());

  // Expect no change for non-clipping frames.
  CallPreProcessAudioBuffer(/*num_calls=*/10, kCloseToClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(/*num_calls=*/10, kCloseToClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.stream_analog_level(),
            manager_without_prediction.stream_analog_level());

  // Expect no change when clipping is not detected or predicted.
  CallPreProcessAudioBuffer(2 * kClippedWaitFrames, kZeroPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(2 * kClippedWaitFrames, kZeroPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.stream_analog_level(),
            manager_without_prediction.stream_analog_level());

  // Expect a change for clipping frames.
  CallPreProcessAudioBuffer(/*num_calls=*/1, kClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(/*num_calls=*/1, kClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.stream_analog_level(),
            manager_without_prediction.stream_analog_level());

  // Expect no change during waiting.
  CallPreProcessAudioBuffer(kClippedWaitFrames, kClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(kClippedWaitFrames, kClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.stream_analog_level(),
            manager_without_prediction.stream_analog_level());

  // Expect a change for clipping frames.
  CallPreProcessAudioBuffer(/*num_calls=*/1, kClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(/*num_calls=*/1, kClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.stream_analog_level(),
            manager_without_prediction.stream_analog_level());
}

}  // namespace webrtc
