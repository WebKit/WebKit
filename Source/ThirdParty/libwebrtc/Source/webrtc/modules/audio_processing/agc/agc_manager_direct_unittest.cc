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

#include <fstream>
#include <limits>
#include <tuple>
#include <vector>

#include "modules/audio_processing/agc/gain_control.h"
#include "modules/audio_processing/agc/mock_agc.h"
#include "modules/audio_processing/include/mock_audio_processing.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/strings/string_builder.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace webrtc {
namespace {

constexpr int kSampleRateHz = 32000;
constexpr int kNumChannels = 1;
constexpr int kInitialInputVolume = 128;
constexpr int kClippedMin = 165;  // Arbitrary, but different from the default.
constexpr float kAboveClippedThreshold = 0.2f;
constexpr int kMinMicLevel = 12;
constexpr int kClippedLevelStep = 15;
constexpr float kClippedRatioThreshold = 0.1f;
constexpr int kClippedWaitFrames = 300;
constexpr float kLowSpeechProbability = 0.1f;
constexpr float kHighSpeechProbability = 0.7f;
constexpr float kSpeechLevelDbfs = -25.0f;

constexpr float kMinSample = std::numeric_limits<int16_t>::min();
constexpr float kMaxSample = std::numeric_limits<int16_t>::max();

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

// Deprecated.
// TODO(bugs.webrtc.org/7494): Delete this helper, use
// `AgcManagerDirectTestHelper::CallAgcSequence()` instead.
// Calls `AnalyzePreProcess()` on `manager` `num_calls` times. `peak_ratio` is a
// value in [0, 1] which determines the amplitude of the samples (1 maps to full
// scale). The first half of the calls is made on frames which are half filled
// with zeros in order to simulate a signal with different crest factors.
void CallPreProcessAudioBuffer(int num_calls,
                               float peak_ratio,
                               AgcManagerDirect& manager) {
  RTC_DCHECK_LE(peak_ratio, 1.0f);
  AudioBuffer audio_buffer(kSampleRateHz, kNumChannels, kSampleRateHz,
                           kNumChannels, kSampleRateHz, kNumChannels);
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
    manager.AnalyzePreProcess(audio_buffer);
  }

  // Make the remaining half of the calls with frames whose samples are all set.
  for (int ch = 0; ch < num_channels; ++ch) {
    for (int i = 0; i < num_frames; ++i) {
      audio_buffer.channels()[ch][i] = peak_ratio * 32767.0f;
    }
  }
  for (int n = 0; n < num_calls - num_calls / 2; ++n) {
    manager.AnalyzePreProcess(audio_buffer);
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
    std::optional<int> min_mic_level) {
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
  RTC_DCHECK_GE(samples_value, kMinSample);
  RTC_DCHECK_LE(samples_value, kMaxSample);
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

// Deprecated.
// TODO(bugs.webrtc.org/7494): Delete this helper, use
// `AgcManagerDirectTestHelper::CallAgcSequence()` instead.
void CallPreProcessAndProcess(int num_calls,
                              const AudioBuffer& audio_buffer,
                              std::optional<float> speech_probability_override,
                              std::optional<float> speech_level_override,
                              AgcManagerDirect& manager) {
  for (int n = 0; n < num_calls; ++n) {
    manager.AnalyzePreProcess(audio_buffer);
    manager.Process(audio_buffer, speech_probability_override,
                    speech_level_override);
  }
}

// Reads a given number of 10 ms chunks from a PCM file and feeds them to
// `AgcManagerDirect`.
class SpeechSamplesReader {
 private:
  // Recording properties.
  static constexpr int kPcmSampleRateHz = 16000;
  static constexpr int kPcmNumChannels = 1;
  static constexpr int kPcmBytesPerSamples = sizeof(int16_t);

 public:
  SpeechSamplesReader()
      : is_(test::ResourcePath("audio_processing/agc/agc_audio", "pcm"),
            std::ios::binary | std::ios::ate),
        audio_buffer_(kPcmSampleRateHz,
                      kPcmNumChannels,
                      kPcmSampleRateHz,
                      kPcmNumChannels,
                      kPcmSampleRateHz,
                      kPcmNumChannels),
        buffer_(audio_buffer_.num_frames()),
        buffer_num_bytes_(buffer_.size() * kPcmBytesPerSamples) {
    RTC_CHECK(is_);
  }

  // Reads `num_frames` 10 ms frames from the beginning of the PCM file, applies
  // `gain_db` and feeds the frames into `agc` by calling `AnalyzePreProcess()`
  // and `Process()` for each frame. Reads the number of 10 ms frames available
  // in the PCM file if `num_frames` is too large - i.e., does not loop.
  void Feed(int num_frames, int gain_db, AgcManagerDirect& agc) {
    float gain = std::pow(10.0f, gain_db / 20.0f);  // From dB to linear gain.
    is_.seekg(0, is_.beg);  // Start from the beginning of the PCM file.

    // Read and feed frames.
    for (int i = 0; i < num_frames; ++i) {
      is_.read(reinterpret_cast<char*>(buffer_.data()), buffer_num_bytes_);
      if (is_.gcount() < buffer_num_bytes_) {
        // EOF reached. Stop.
        break;
      }
      // Apply gain and copy samples into `audio_buffer_`.
      std::transform(buffer_.begin(), buffer_.end(),
                     audio_buffer_.channels()[0], [gain](int16_t v) -> float {
                       return rtc::SafeClamp(static_cast<float>(v) * gain,
                                             kMinSample, kMaxSample);
                     });

      agc.AnalyzePreProcess(audio_buffer_);
      agc.Process(audio_buffer_);
    }
  }

  // Reads `num_frames` 10 ms frames from the beginning of the PCM file, applies
  // `gain_db` and feeds the frames into `agc` by calling `AnalyzePreProcess()`
  // and `Process()` for each frame. Reads the number of 10 ms frames available
  // in the PCM file if `num_frames` is too large - i.e., does not loop.
  // `speech_probability_override` and `speech_level_override` are passed to
  // `Process()` where they are used to override the `agc` RMS error if they
  // have a value.
  void Feed(int num_frames,
            int gain_db,
            std::optional<float> speech_probability_override,
            std::optional<float> speech_level_override,
            AgcManagerDirect& agc) {
    float gain = std::pow(10.0f, gain_db / 20.0f);  // From dB to linear gain.
    is_.seekg(0, is_.beg);  // Start from the beginning of the PCM file.

    // Read and feed frames.
    for (int i = 0; i < num_frames; ++i) {
      is_.read(reinterpret_cast<char*>(buffer_.data()), buffer_num_bytes_);
      if (is_.gcount() < buffer_num_bytes_) {
        // EOF reached. Stop.
        break;
      }
      // Apply gain and copy samples into `audio_buffer_`.
      std::transform(buffer_.begin(), buffer_.end(),
                     audio_buffer_.channels()[0], [gain](int16_t v) -> float {
                       return rtc::SafeClamp(static_cast<float>(v) * gain,
                                             kMinSample, kMaxSample);
                     });

      agc.AnalyzePreProcess(audio_buffer_);
      agc.Process(audio_buffer_, speech_probability_override,
                  speech_level_override);
    }
  }

 private:
  std::ifstream is_;
  AudioBuffer audio_buffer_;
  std::vector<int16_t> buffer_;
  const std::streamsize buffer_num_bytes_;
};

}  // namespace

// TODO(bugs.webrtc.org/12874): Use constexpr struct with designated
// initializers once fixed.
constexpr AnalogAgcConfig GetAnalogAgcTestConfig() {
  AnalogAgcConfig config;
  config.enabled = true;
  config.startup_min_volume = kInitialInputVolume;
  config.clipped_level_min = kClippedMin;
  config.enable_digital_adaptive = true;
  config.clipped_level_step = kClippedLevelStep;
  config.clipped_ratio_threshold = kClippedRatioThreshold;
  config.clipped_wait_frames = kClippedWaitFrames;
  config.clipping_predictor = kDefaultAnalogConfig.clipping_predictor;
  return config;
}

constexpr AnalogAgcConfig GetDisabledAnalogAgcConfig() {
  AnalogAgcConfig config = GetAnalogAgcTestConfig();
  config.enabled = false;
  return config;
}

// Helper class that provides an `AgcManagerDirect` instance with an injected
// `Agc` mock, an `AudioBuffer` instance and `CallAgcSequence()`, a helper
// method that runs the `AgcManagerDirect` instance on the `AudioBuffer` one by
// sticking to the API contract.
class AgcManagerDirectTestHelper {
 public:
  // Ctor. Initializes `audio_buffer` with zeros.
  AgcManagerDirectTestHelper()
      : audio_buffer(kSampleRateHz,
                     kNumChannels,
                     kSampleRateHz,
                     kNumChannels,
                     kSampleRateHz,
                     kNumChannels),
        mock_agc(new ::testing::NiceMock<MockAgc>()),
        manager(GetAnalogAgcTestConfig(), mock_agc) {
    manager.Initialize();
    manager.SetupDigitalGainControl(mock_gain_control);
    WriteAudioBufferSamples(/*samples_value=*/0.0f, /*clipped_ratio=*/0.0f,
                            audio_buffer);
  }

  // Calls the sequence of `AgcManagerDirect` methods according to the API
  // contract, namely:
  // - Sets the applied input volume;
  // - Uses `audio_buffer` to call `AnalyzePreProcess()` and `Process()`;
  // - Sets the digital compression gain, if specified, on the injected
  // `mock_agc`. Returns the recommended input volume. The RMS error from
  // AGC is replaced by an override value if `speech_probability_override`
  // and `speech_level_override` have a value.
  int CallAgcSequence(int applied_input_volume,
                      std::optional<float> speech_probability_override,
                      std::optional<float> speech_level_override) {
    manager.set_stream_analog_level(applied_input_volume);
    manager.AnalyzePreProcess(audio_buffer);
    manager.Process(audio_buffer, speech_probability_override,
                    speech_level_override);
    std::optional<int> digital_gain = manager.GetDigitalComressionGain();
    if (digital_gain) {
      mock_gain_control.set_compression_gain_db(*digital_gain);
    }
    return manager.recommended_analog_level();
  }

  // Deprecated.
  // TODO(bugs.webrtc.org/7494): Let the caller write `audio_buffer` and use
  // `CallAgcSequence()`. The RMS error from AGC is replaced by an override
  // value if `speech_probability_override` and `speech_level_override` have
  // a value.
  void CallProcess(int num_calls,
                   std::optional<float> speech_probability_override,
                   std::optional<float> speech_level_override) {
    for (int i = 0; i < num_calls; ++i) {
      EXPECT_CALL(*mock_agc, Process(_)).WillOnce(Return());
      manager.Process(audio_buffer, speech_probability_override,
                      speech_level_override);
      std::optional<int> new_digital_gain = manager.GetDigitalComressionGain();
      if (new_digital_gain) {
        mock_gain_control.set_compression_gain_db(*new_digital_gain);
      }
    }
  }

  // Deprecated.
  // TODO(bugs.webrtc.org/7494): Let the caller write `audio_buffer` and use
  // `CallAgcSequence()`.
  void CallPreProc(int num_calls, float clipped_ratio) {
    RTC_DCHECK_GE(clipped_ratio, 0.0f);
    RTC_DCHECK_LE(clipped_ratio, 1.0f);
    WriteAudioBufferSamples(/*samples_value=*/0.0f, clipped_ratio,
                            audio_buffer);
    for (int i = 0; i < num_calls; ++i) {
      manager.AnalyzePreProcess(audio_buffer);
    }
  }

  // Deprecated.
  // TODO(bugs.webrtc.org/7494): Let the caller write `audio_buffer` and use
  // `CallAgcSequence()`.
  void CallPreProcForChangingAudio(int num_calls, float peak_ratio) {
    RTC_DCHECK_GE(peak_ratio, 0.0f);
    RTC_DCHECK_LE(peak_ratio, 1.0f);
    const float samples_value = peak_ratio * 32767.0f;

    // Make half of the calls on a frame where the samples alternate
    // `sample_values` and zeros.
    WriteAudioBufferSamples(samples_value, /*clipped_ratio=*/0.0f,
                            audio_buffer);
    for (size_t ch = 0; ch < audio_buffer.num_channels(); ++ch) {
      for (size_t k = 1; k < audio_buffer.num_frames(); k += 2) {
        audio_buffer.channels()[ch][k] = 0.0f;
      }
    }
    for (int i = 0; i < num_calls / 2; ++i) {
      manager.AnalyzePreProcess(audio_buffer);
    }

    // Make half of thecalls on a frame where all the samples equal
    // `sample_values`.
    WriteAudioBufferSamples(samples_value, /*clipped_ratio=*/0.0f,
                            audio_buffer);
    for (int i = 0; i < num_calls - num_calls / 2; ++i) {
      manager.AnalyzePreProcess(audio_buffer);
    }
  }

  AudioBuffer audio_buffer;
  MockAgc* mock_agc;
  AgcManagerDirect manager;
  MockGainControl mock_gain_control;
};

class AgcManagerDirectParametrizedTest
    : public ::testing::TestWithParam<std::tuple<std::optional<int>, bool>> {
 protected:
  AgcManagerDirectParametrizedTest()
      : field_trials_(
            GetAgcMinMicLevelExperimentFieldTrial(std::get<0>(GetParam()))) {}

  bool IsMinMicLevelOverridden() const {
    return std::get<0>(GetParam()).has_value();
  }
  int GetMinMicLevel() const {
    return std::get<0>(GetParam()).value_or(kMinMicLevel);
  }

  bool IsRmsErrorOverridden() const { return std::get<1>(GetParam()); }
  std::optional<float> GetOverrideOrEmpty(float value) const {
    return IsRmsErrorOverridden() ? std::optional<float>(value) : std::nullopt;
  }

 private:
  test::ScopedFieldTrials field_trials_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    AgcManagerDirectParametrizedTest,
    ::testing::Combine(testing::Values(std::nullopt, 12, 20), testing::Bool()));

// Checks that when the analog controller is disabled, no downward adaptation
// takes place.
// TODO(webrtc:7494): Revisit the test after moving the number of override wait
// frames to AMP config. The test passes but internally the gain update timing
// differs.
TEST_P(AgcManagerDirectParametrizedTest,
       DisabledAnalogAgcDoesNotAdaptDownwards) {
  AgcManagerDirect manager_no_analog_agc(kNumChannels,
                                         GetDisabledAnalogAgcConfig());
  manager_no_analog_agc.Initialize();
  AgcManagerDirect manager_with_analog_agc(kNumChannels,
                                           GetAnalogAgcTestConfig());
  manager_with_analog_agc.Initialize();

  AudioBuffer audio_buffer(kSampleRateHz, kNumChannels, kSampleRateHz,
                           kNumChannels, kSampleRateHz, kNumChannels);

  constexpr int kAnalogLevel = 250;
  static_assert(kAnalogLevel > kInitialInputVolume, "Increase `kAnalogLevel`.");
  manager_no_analog_agc.set_stream_analog_level(kAnalogLevel);
  manager_with_analog_agc.set_stream_analog_level(kAnalogLevel);

  // Make a first call with input that doesn't clip in order to let the
  // controller read the input volume. That is needed because clipping input
  // causes the controller to stay in idle state for
  // `AnalogAgcConfig::clipped_wait_frames` frames.
  WriteAudioBufferSamples(/*samples_value=*/0.0f, /*clipping_ratio=*/0.0f,
                          audio_buffer);
  manager_no_analog_agc.AnalyzePreProcess(audio_buffer);
  manager_with_analog_agc.AnalyzePreProcess(audio_buffer);
  manager_no_analog_agc.Process(audio_buffer,
                                GetOverrideOrEmpty(kHighSpeechProbability),
                                GetOverrideOrEmpty(-18.0f));
  manager_with_analog_agc.Process(audio_buffer,
                                  GetOverrideOrEmpty(kHighSpeechProbability),
                                  GetOverrideOrEmpty(-18.0f));

  // Feed clipping input to trigger a downward adapation of the analog level.
  WriteAudioBufferSamples(/*samples_value=*/0.0f, /*clipping_ratio=*/0.2f,
                          audio_buffer);
  manager_no_analog_agc.AnalyzePreProcess(audio_buffer);
  manager_with_analog_agc.AnalyzePreProcess(audio_buffer);
  manager_no_analog_agc.Process(audio_buffer,
                                GetOverrideOrEmpty(kHighSpeechProbability),
                                GetOverrideOrEmpty(-10.0f));
  manager_with_analog_agc.Process(audio_buffer,
                                  GetOverrideOrEmpty(kHighSpeechProbability),
                                  GetOverrideOrEmpty(-10.0f));

  // Check that no adaptation occurs when the analog controller is disabled
  // and make sure that the test triggers a downward adaptation otherwise.
  EXPECT_EQ(manager_no_analog_agc.recommended_analog_level(), kAnalogLevel);
  ASSERT_LT(manager_with_analog_agc.recommended_analog_level(), kAnalogLevel);
}

// Checks that when the analog controller is disabled, no upward adaptation
// takes place.
// TODO(webrtc:7494): Revisit the test after moving the number of override wait
// frames to APM config. The test passes but internally the gain update timing
// differs.
TEST_P(AgcManagerDirectParametrizedTest, DisabledAnalogAgcDoesNotAdaptUpwards) {
  AgcManagerDirect manager_no_analog_agc(kNumChannels,
                                         GetDisabledAnalogAgcConfig());
  manager_no_analog_agc.Initialize();
  AgcManagerDirect manager_with_analog_agc(kNumChannels,
                                           GetAnalogAgcTestConfig());
  manager_with_analog_agc.Initialize();

  constexpr int kAnalogLevel = kInitialInputVolume;
  manager_no_analog_agc.set_stream_analog_level(kAnalogLevel);
  manager_with_analog_agc.set_stream_analog_level(kAnalogLevel);

  // Feed speech with low energy to trigger an upward adapation of the analog
  // level.
  constexpr int kNumFrames = 125;
  constexpr int kGainDb = -20;
  SpeechSamplesReader reader;
  reader.Feed(kNumFrames, kGainDb, GetOverrideOrEmpty(kHighSpeechProbability),
              GetOverrideOrEmpty(-42.0f), manager_no_analog_agc);
  reader.Feed(kNumFrames, kGainDb, GetOverrideOrEmpty(kHighSpeechProbability),
              GetOverrideOrEmpty(-42.0f), manager_with_analog_agc);

  // Check that no adaptation occurs when the analog controller is disabled
  // and make sure that the test triggers an upward adaptation otherwise.
  EXPECT_EQ(manager_no_analog_agc.recommended_analog_level(), kAnalogLevel);
  ASSERT_GT(manager_with_analog_agc.recommended_analog_level(), kAnalogLevel);
}

TEST_P(AgcManagerDirectParametrizedTest,
       StartupMinVolumeConfigurationIsRespected) {
  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume,
                         GetOverrideOrEmpty(kHighSpeechProbability),
                         GetOverrideOrEmpty(kSpeechLevelDbfs));
  EXPECT_EQ(kInitialInputVolume, helper.manager.recommended_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, MicVolumeResponseToRmsError) {
  const auto speech_probability_override =
      GetOverrideOrEmpty(kHighSpeechProbability);

  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, speech_probability_override,
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  // Compressor default; no residual error.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(5), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-23.0f));

  // Inside the compressor's window; no change of volume.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-28.0f));

  // Above the compressor's window; volume should be increased.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-29.0f));
  EXPECT_EQ(130, helper.manager.recommended_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(20), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-38.0f));
  EXPECT_EQ(168, helper.manager.recommended_analog_level());

  // Inside the compressor's window; no change of volume.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(5), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-23.0f));
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-18.0f));

  // Below the compressor's window; volume should be decreased.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-17.0f));
  EXPECT_EQ(167, helper.manager.recommended_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-17.0f));
  EXPECT_EQ(163, helper.manager.recommended_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-9), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-9.0f));
  EXPECT_EQ(129, helper.manager.recommended_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, MicVolumeIsLimited) {
  const auto speech_probability_override =
      GetOverrideOrEmpty(kHighSpeechProbability);

  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, speech_probability_override,
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  // Maximum upwards change is limited.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(30), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-48.0f));
  EXPECT_EQ(183, helper.manager.recommended_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(30), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-48.0f));
  EXPECT_EQ(243, helper.manager.recommended_analog_level());

  // Won't go higher than the maximum.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(30), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-48.0f));
  EXPECT_EQ(255, helper.manager.recommended_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-17.0f));
  EXPECT_EQ(254, helper.manager.recommended_analog_level());

  // Maximum downwards change is limited.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(22.0f));
  EXPECT_EQ(194, helper.manager.recommended_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(22.0f));
  EXPECT_EQ(137, helper.manager.recommended_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(22.0f));
  EXPECT_EQ(88, helper.manager.recommended_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(22.0f));
  EXPECT_EQ(54, helper.manager.recommended_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(22.0f));
  EXPECT_EQ(33, helper.manager.recommended_analog_level());

  // Won't go lower than the minimum.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(22.0f));
  EXPECT_EQ(std::max(18, GetMinMicLevel()),
            helper.manager.recommended_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(22.0f));
  EXPECT_EQ(std::max(12, GetMinMicLevel()),
            helper.manager.recommended_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, CompressorStepsTowardsTarget) {
  constexpr std::optional<float> kNoOverride = std::nullopt;
  const auto speech_probability_override =
      GetOverrideOrEmpty(kHighSpeechProbability);

  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, speech_probability_override,
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  // Compressor default; no call to set_compression_gain_db.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(5), Return(true)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-23.0f));
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  // The mock `GetRmsErrorDb()` returns false; mimic this by passing
  // std::nullopt as an override.
  helper.CallProcess(/*num_calls=*/19, kNoOverride, kNoOverride);

  // Moves slowly upwards.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(9), Return(true)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-27.0f));
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/18, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(8))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/1, kNoOverride, kNoOverride);

  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/19, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(9))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/1, kNoOverride, kNoOverride);

  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);

  // Moves slowly downward, then reverses before reaching the original target.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(5), Return(true)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-23.0f));
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/18, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(8))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/1, kNoOverride, kNoOverride);

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(9), Return(true)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-27.0f));
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/18, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(9))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/1, kNoOverride, kNoOverride);

  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
}

TEST_P(AgcManagerDirectParametrizedTest, CompressorErrorIsDeemphasized) {
  constexpr std::optional<float> kNoOverride = std::nullopt;
  const auto speech_probability_override =
      GetOverrideOrEmpty(kHighSpeechProbability);

  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, speech_probability_override,
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)))
      .WillRepeatedly(Return(false));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-28.0f));
  // The mock `GetRmsErrorDb()` returns false; mimic this by passing
  // std::nullopt as an override.
  helper.CallProcess(/*num_calls=*/18, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(8))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(9))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/1, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)))
      .WillRepeatedly(Return(false));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-18.0f));
  helper.CallProcess(/*num_calls=*/18, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(8))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(7))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(6))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/1, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(_)).Times(0);
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
}

TEST_P(AgcManagerDirectParametrizedTest, CompressorReachesMaximum) {
  constexpr std::optional<float> kNoOverride = std::nullopt;
  const auto speech_probability_override =
      GetOverrideOrEmpty(kHighSpeechProbability);

  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, speech_probability_override,
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)))
      .WillRepeatedly(Return(false));
  helper.CallProcess(/*num_calls=*/4, speech_probability_override,
                     GetOverrideOrEmpty(-28.0f));
  // The mock `GetRmsErrorDb()` returns false; mimic this by passing
  // std::nullopt as an override.
  helper.CallProcess(/*num_calls=*/15, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(8))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(9))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(10))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(11))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(12))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/1, kNoOverride, kNoOverride);
}

TEST_P(AgcManagerDirectParametrizedTest, CompressorReachesMinimum) {
  constexpr std::optional<float> kNoOverride = std::nullopt;
  const auto speech_probability_override =
      GetOverrideOrEmpty(kHighSpeechProbability);

  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, speech_probability_override,
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)))
      .WillRepeatedly(Return(false));
  helper.CallProcess(/*num_calls=*/4, speech_probability_override,
                     GetOverrideOrEmpty(-18.0f));
  // The mock `GetRmsErrorDb()` returns false; mimic this by passing
  // std::nullopt as an override.
  helper.CallProcess(/*num_calls=*/15, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(6))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(5))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(4))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(3))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(2))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/1, kNoOverride, kNoOverride);
}

TEST_P(AgcManagerDirectParametrizedTest, NoActionWhileMuted) {
  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume,
                         GetOverrideOrEmpty(kHighSpeechProbability),
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  helper.manager.HandleCaptureOutputUsedChange(false);
  helper.manager.Process(helper.audio_buffer,
                         GetOverrideOrEmpty(kHighSpeechProbability),
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  std::optional<int> new_digital_gain =
      helper.manager.GetDigitalComressionGain();
  if (new_digital_gain) {
    helper.mock_gain_control.set_compression_gain_db(*new_digital_gain);
  }
}

TEST_P(AgcManagerDirectParametrizedTest, UnmutingChecksVolumeWithoutRaising) {
  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume,
                         GetOverrideOrEmpty(kHighSpeechProbability),
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  helper.manager.HandleCaptureOutputUsedChange(false);
  helper.manager.HandleCaptureOutputUsedChange(true);

  constexpr int kInputVolume = 127;
  helper.manager.set_stream_analog_level(kInputVolume);
  EXPECT_CALL(*helper.mock_agc, Reset());

  // SetMicVolume should not be called.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_)).WillOnce(Return(false));
  helper.CallProcess(/*num_calls=*/1,
                     GetOverrideOrEmpty(kHighSpeechProbability),
                     GetOverrideOrEmpty(kSpeechLevelDbfs));
  EXPECT_EQ(127, helper.manager.recommended_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, UnmutingRaisesTooLowVolume) {
  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume,
                         GetOverrideOrEmpty(kHighSpeechProbability),
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  helper.manager.HandleCaptureOutputUsedChange(false);
  helper.manager.HandleCaptureOutputUsedChange(true);

  constexpr int kInputVolume = 11;
  helper.manager.set_stream_analog_level(kInputVolume);
  EXPECT_CALL(*helper.mock_agc, Reset());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_)).WillOnce(Return(false));
  helper.CallProcess(/*num_calls=*/1,
                     GetOverrideOrEmpty(kHighSpeechProbability),
                     GetOverrideOrEmpty(kSpeechLevelDbfs));
  EXPECT_EQ(GetMinMicLevel(), helper.manager.recommended_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest,
       ManualLevelChangeResultsInNoSetMicCall) {
  const auto speech_probability_override =
      GetOverrideOrEmpty(kHighSpeechProbability);

  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, speech_probability_override,
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  // Change outside of compressor's range, which would normally trigger a call
  // to `SetMicVolume()`.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)));

  // When the analog volume changes, the gain controller is reset.
  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));

  // GetMicVolume returns a value outside of the quantization slack, indicating
  // a manual volume change.
  ASSERT_NE(helper.manager.recommended_analog_level(), 154);
  helper.manager.set_stream_analog_level(154);
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-29.0f));
  EXPECT_EQ(154, helper.manager.recommended_analog_level());

  // Do the same thing, except downwards now.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  helper.manager.set_stream_analog_level(100);
  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-17.0f));
  EXPECT_EQ(100, helper.manager.recommended_analog_level());

  // And finally verify the AGC continues working without a manual change.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-17.0f));
  EXPECT_EQ(99, helper.manager.recommended_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest,
       RecoveryAfterManualLevelChangeFromMax) {
  const auto speech_probability_override =
      GetOverrideOrEmpty(kHighSpeechProbability);

  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, speech_probability_override,
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  // Force the mic up to max volume. Takes a few steps due to the residual
  // gain limitation.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(30), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-48.0f));
  EXPECT_EQ(183, helper.manager.recommended_analog_level());
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-48.0f));
  EXPECT_EQ(243, helper.manager.recommended_analog_level());
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-48.0f));
  EXPECT_EQ(255, helper.manager.recommended_analog_level());

  // Manual change does not result in SetMicVolume call.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  helper.manager.set_stream_analog_level(50);
  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-17.0f));
  EXPECT_EQ(50, helper.manager.recommended_analog_level());

  // Continues working as usual afterwards.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(20), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-38.0f));

  EXPECT_EQ(69, helper.manager.recommended_analog_level());
}

// Checks that, when the min mic level override is not specified, AGC ramps up
// towards the minimum mic level after the mic level is manually set below the
// minimum gain to enforce.
TEST_P(AgcManagerDirectParametrizedTest,
       RecoveryAfterManualLevelChangeBelowMinWithoutMiMicLevelnOverride) {
  if (IsMinMicLevelOverridden()) {
    GTEST_SKIP() << "Skipped. Min mic level overridden.";
  }

  const auto speech_probability_override =
      GetOverrideOrEmpty(kHighSpeechProbability);

  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, speech_probability_override,
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  // Manual change below min, but strictly positive, otherwise AGC won't take
  // any action.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  helper.manager.set_stream_analog_level(1);
  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-17.0f));
  EXPECT_EQ(1, helper.manager.recommended_analog_level());

  // Continues working as usual afterwards.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-29.0f));
  EXPECT_EQ(2, helper.manager.recommended_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(30), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-48.0f));
  EXPECT_EQ(11, helper.manager.recommended_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(20), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-38.0f));
  EXPECT_EQ(18, helper.manager.recommended_analog_level());
}

// Checks that, when the min mic level override is specified, AGC immediately
// applies the minimum mic level after the mic level is manually set below the
// minimum gain to enforce.
TEST_P(AgcManagerDirectParametrizedTest,
       RecoveryAfterManualLevelChangeBelowMin) {
  if (!IsMinMicLevelOverridden()) {
    GTEST_SKIP() << "Skipped. Min mic level not overridden.";
  }

  const auto speech_probability_override =
      GetOverrideOrEmpty(kHighSpeechProbability);

  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, speech_probability_override,
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  // Manual change below min, but strictly positive, otherwise
  // AGC won't take any action.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  helper.manager.set_stream_analog_level(1);
  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-17.0f));
  EXPECT_EQ(GetMinMicLevel(), helper.manager.recommended_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, NoClippingHasNoImpact) {
  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume,
                         GetOverrideOrEmpty(kHighSpeechProbability),
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  helper.CallPreProc(/*num_calls=*/100, /*clipped_ratio=*/0);
  EXPECT_EQ(128, helper.manager.recommended_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, ClippingUnderThresholdHasNoImpact) {
  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume,
                         GetOverrideOrEmpty(kHighSpeechProbability),
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/0.099);
  EXPECT_EQ(128, helper.manager.recommended_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, ClippingLowersVolume) {
  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/255,
                         GetOverrideOrEmpty(kHighSpeechProbability),
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/0.2);
  EXPECT_EQ(240, helper.manager.recommended_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, WaitingPeriodBetweenClippingChecks) {
  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/255,
                         GetOverrideOrEmpty(kHighSpeechProbability),
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(240, helper.manager.recommended_analog_level());

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(0);
  helper.CallPreProc(/*num_calls=*/300,
                     /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(240, helper.manager.recommended_analog_level());

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(225, helper.manager.recommended_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, ClippingLoweringIsLimited) {
  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/180,
                         GetOverrideOrEmpty(kHighSpeechProbability),
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(kClippedMin, helper.manager.recommended_analog_level());

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(0);
  helper.CallPreProc(/*num_calls=*/1000,
                     /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(kClippedMin, helper.manager.recommended_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest,
       ClippingMaxIsRespectedWhenEqualToLevel) {
  const auto speech_probability_override =
      GetOverrideOrEmpty(kHighSpeechProbability);

  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/255,
                         speech_probability_override,
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(240, helper.manager.recommended_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(30), Return(true)));
  helper.CallProcess(/*num_calls=*/10, speech_probability_override,
                     GetOverrideOrEmpty(-48.0f));
  EXPECT_EQ(240, helper.manager.recommended_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest,
       ClippingMaxIsRespectedWhenHigherThanLevel) {
  const auto speech_probability_override =
      GetOverrideOrEmpty(kHighSpeechProbability);

  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/200,
                         speech_probability_override,
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(185, helper.manager.recommended_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(40), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-58.0f));
  EXPECT_EQ(240, helper.manager.recommended_analog_level());
  helper.CallProcess(/*num_calls=*/10, speech_probability_override,
                     GetOverrideOrEmpty(-58.0f));
  EXPECT_EQ(240, helper.manager.recommended_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest,
       MaxCompressionIsIncreasedAfterClipping) {
  constexpr std::optional<float> kNoOverride = std::nullopt;
  const auto speech_probability_override =
      GetOverrideOrEmpty(kHighSpeechProbability);

  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/210,
                         speech_probability_override,
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallPreProc(/*num_calls=*/1, kAboveClippedThreshold);
  EXPECT_EQ(195, helper.manager.recommended_analog_level());

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)))
      .WillRepeatedly(Return(false));
  helper.CallProcess(/*num_calls=*/5, speech_probability_override,
                     GetOverrideOrEmpty(-29.0f));
  // The mock `GetRmsErrorDb()` returns false; mimic this by passing
  // std::nullopt as an override.
  helper.CallProcess(/*num_calls=*/14, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(8))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(9))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(10))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(11))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(12))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(13))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/1, kNoOverride, kNoOverride);

  // Continue clipping until we hit the maximum surplus compression.
  helper.CallPreProc(/*num_calls=*/300,
                     /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(180, helper.manager.recommended_analog_level());

  helper.CallPreProc(/*num_calls=*/300,
                     /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallPreProc(1, kAboveClippedThreshold);
  EXPECT_EQ(kClippedMin, helper.manager.recommended_analog_level());

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
  helper.CallProcess(/*num_calls=*/4, speech_probability_override,
                     GetOverrideOrEmpty(-34.0f));
  helper.CallProcess(/*num_calls=*/15, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(14))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(15))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(16))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(17))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/20, kNoOverride, kNoOverride);
  EXPECT_CALL(helper.mock_gain_control, set_compression_gain_db(18))
      .WillOnce(Return(0));
  helper.CallProcess(/*num_calls=*/1, kNoOverride, kNoOverride);
}

TEST_P(AgcManagerDirectParametrizedTest, UserCanRaiseVolumeAfterClipping) {
  const auto speech_probability_override =
      GetOverrideOrEmpty(kHighSpeechProbability);

  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/225,
                         speech_probability_override,
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(210, helper.manager.recommended_analog_level());

  // High enough error to trigger a volume check.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(14), Return(true)));
  // User changed the volume.
  helper.manager.set_stream_analog_level(250);
  EXPECT_CALL(*helper.mock_agc, Reset()).Times(AtLeast(1));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-32.0f));
  EXPECT_EQ(250, helper.manager.recommended_analog_level());

  // Move down...
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-10), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-8.0f));
  EXPECT_EQ(210, helper.manager.recommended_analog_level());
  // And back up to the new max established by the user.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(40), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-58.0f));
  EXPECT_EQ(250, helper.manager.recommended_analog_level());
  // Will not move above new maximum.
  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(30), Return(true)));
  helper.CallProcess(/*num_calls=*/1, speech_probability_override,
                     GetOverrideOrEmpty(-48.0f));
  EXPECT_EQ(250, helper.manager.recommended_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, ClippingDoesNotPullLowVolumeBackUp) {
  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/80,
                         GetOverrideOrEmpty(kHighSpeechProbability),
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  EXPECT_CALL(*helper.mock_agc, Reset()).Times(0);
  int initial_volume = helper.manager.recommended_analog_level();
  helper.CallPreProc(/*num_calls=*/1, /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(initial_volume, helper.manager.recommended_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, TakesNoActionOnZeroMicVolume) {
  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume,
                         GetOverrideOrEmpty(kHighSpeechProbability),
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  EXPECT_CALL(*helper.mock_agc, GetRmsErrorDb(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(30), Return(true)));
  helper.manager.set_stream_analog_level(0);
  helper.CallProcess(/*num_calls=*/10,
                     GetOverrideOrEmpty(kHighSpeechProbability),
                     GetOverrideOrEmpty(-48.0f));
  EXPECT_EQ(0, helper.manager.recommended_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, ClippingDetectionLowersVolume) {
  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/255,
                         GetOverrideOrEmpty(kHighSpeechProbability),
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  EXPECT_EQ(255, helper.manager.recommended_analog_level());
  helper.CallPreProcForChangingAudio(/*num_calls=*/100, /*peak_ratio=*/0.99f);
  EXPECT_EQ(255, helper.manager.recommended_analog_level());
  helper.CallPreProcForChangingAudio(/*num_calls=*/100, /*peak_ratio=*/1.0f);
  EXPECT_EQ(240, helper.manager.recommended_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest,
       DisabledClippingPredictorDoesNotLowerVolume) {
  AgcManagerDirectTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/255,
                         GetOverrideOrEmpty(kHighSpeechProbability),
                         GetOverrideOrEmpty(kSpeechLevelDbfs));

  EXPECT_FALSE(helper.manager.clipping_predictor_enabled());
  EXPECT_EQ(255, helper.manager.recommended_analog_level());
  helper.CallPreProcForChangingAudio(/*num_calls=*/100, /*peak_ratio=*/0.99f);
  EXPECT_EQ(255, helper.manager.recommended_analog_level());
  helper.CallPreProcForChangingAudio(/*num_calls=*/100, /*peak_ratio=*/0.99f);
  EXPECT_EQ(255, helper.manager.recommended_analog_level());
}

TEST_P(AgcManagerDirectParametrizedTest, DisableDigitalDisablesDigital) {
  if (IsRmsErrorOverridden()) {
    GTEST_SKIP() << "Skipped. RMS error override does not affect the test.";
  }

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
      CreateAgcManagerDirect(kInitialInputVolume, kClippedLevelStep,
                             kClippedRatioThreshold, kClippedWaitFrames);
  EXPECT_EQ(manager->channel_agcs_[0]->min_mic_level(), kMinMicLevel);
}

TEST(AgcManagerDirectTest, AgcMinMicLevelExperimentDisabled) {
  for (const std::string& field_trial_suffix : {"", "_20220210"}) {
    test::ScopedFieldTrials field_trial(
        GetAgcMinMicLevelExperimentFieldTrial("Disabled" + field_trial_suffix));
    std::unique_ptr<AgcManagerDirect> manager =
        CreateAgcManagerDirect(kInitialInputVolume, kClippedLevelStep,
                               kClippedRatioThreshold, kClippedWaitFrames);
    EXPECT_EQ(manager->channel_agcs_[0]->min_mic_level(), kMinMicLevel);
  }
}

// Checks that a field-trial parameter outside of the valid range [0,255] is
// ignored.
TEST(AgcManagerDirectTest, AgcMinMicLevelExperimentOutOfRangeAbove) {
  test::ScopedFieldTrials field_trial(
      GetAgcMinMicLevelExperimentFieldTrial("Enabled-256"));
  std::unique_ptr<AgcManagerDirect> manager =
      CreateAgcManagerDirect(kInitialInputVolume, kClippedLevelStep,
                             kClippedRatioThreshold, kClippedWaitFrames);
  EXPECT_EQ(manager->channel_agcs_[0]->min_mic_level(), kMinMicLevel);
}

// Checks that a field-trial parameter outside of the valid range [0,255] is
// ignored.
TEST(AgcManagerDirectTest, AgcMinMicLevelExperimentOutOfRangeBelow) {
  test::ScopedFieldTrials field_trial(
      GetAgcMinMicLevelExperimentFieldTrial("Enabled--1"));
  std::unique_ptr<AgcManagerDirect> manager =
      CreateAgcManagerDirect(kInitialInputVolume, kClippedLevelStep,
                             kClippedRatioThreshold, kClippedWaitFrames);
  EXPECT_EQ(manager->channel_agcs_[0]->min_mic_level(), kMinMicLevel);
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
        CreateAgcManagerDirect(kInitialInputVolume, kClippedLevelStep,
                               kClippedRatioThreshold, kClippedWaitFrames);
    EXPECT_EQ(manager->channel_agcs_[0]->min_mic_level(), kMinMicLevelOverride);
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
        CreateAgcManagerDirect(kInitialInputVolume, kClippedLevelStep,
                               kClippedRatioThreshold, kClippedWaitFrames);
    manager->Initialize();
    manager->set_stream_analog_level(kInitialInputVolume);
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
  CallPreProcessAndProcess(/*num_calls=*/400, audio_buffer,
                           /*speech_probability_override=*/std::nullopt,
                           /*speech_level_override=*/std::nullopt, *manager);
  CallPreProcessAndProcess(/*num_calls=*/400, audio_buffer,
                           /*speech_probability_override=*/std::nullopt,
                           /*speech_level_override=*/std::nullopt,
                           *manager_with_override);

  // Make sure that an adaptation occurred.
  ASSERT_GT(manager->recommended_analog_level(), 0);

  // Check that the test signal triggers a larger downward adaptation for
  // `manager`, which is allowed to reach a lower gain.
  EXPECT_GT(manager_with_override->recommended_analog_level(),
            manager->recommended_analog_level());
  // Check that the gain selected by `manager_with_override` equals the minimum
  // value overridden via field trial.
  EXPECT_EQ(manager_with_override->recommended_analog_level(),
            kMinMicLevelOverride);
}

// Checks that, when the "WebRTC-Audio-AgcMinMicLevelExperiment" field trial is
// specified with a valid value, the mic level never gets lowered beyond the
// override value in the presence of clipping when RMS error override is used.
// TODO(webrtc:7494): Revisit the test after moving the number of override wait
// frames to APM config. The test passes but internally the gain update timing
// differs.
TEST(AgcManagerDirectTest,
     AgcMinMicLevelExperimentCheckMinLevelWithClippingWithRmsErrorOverride) {
  constexpr int kMinMicLevelOverride = 250;

  // Create and initialize two AGCs by specifying and leaving unspecified the
  // relevant field trial.
  const auto factory = []() {
    std::unique_ptr<AgcManagerDirect> manager =
        CreateAgcManagerDirect(kInitialInputVolume, kClippedLevelStep,
                               kClippedRatioThreshold, kClippedWaitFrames);
    manager->Initialize();
    manager->set_stream_analog_level(kInitialInputVolume);
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
  CallPreProcessAndProcess(
      /*num_calls=*/400, audio_buffer,
      /*speech_probability_override=*/0.7f,
      /*speech_probability_level=*/-18.0f, *manager);
  CallPreProcessAndProcess(
      /*num_calls=*/400, audio_buffer,
      /*speech_probability_override=*/std::optional<float>(0.7f),
      /*speech_probability_level=*/std::optional<float>(-18.0f),
      *manager_with_override);

  // Make sure that an adaptation occurred.
  ASSERT_GT(manager->recommended_analog_level(), 0);

  // Check that the test signal triggers a larger downward adaptation for
  // `manager`, which is allowed to reach a lower gain.
  EXPECT_GT(manager_with_override->recommended_analog_level(),
            manager->recommended_analog_level());
  // Check that the gain selected by `manager_with_override` equals the minimum
  // value overridden via field trial.
  EXPECT_EQ(manager_with_override->recommended_analog_level(),
            kMinMicLevelOverride);
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
    config.startup_min_volume = kInitialInputVolume;
    config.enable_digital_adaptive = false;
    config.clipped_level_step = 64;
    config.clipped_ratio_threshold = kClippedRatioThreshold;
    config.clipped_wait_frames = kClippedWaitFrames;
    auto controller =
        std::make_unique<AgcManagerDirect>(/*num_capture_channels=*/1, config);
    controller->Initialize();
    controller->set_stream_analog_level(kInitialInputVolume);
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
  CallPreProcessAndProcess(/*num_calls=*/400, audio_buffer,
                           /*speech_probability_override=*/std::nullopt,
                           /*speech_level_override=*/std::nullopt, *manager);
  CallPreProcessAndProcess(/*num_calls=*/400, audio_buffer,
                           /*speech_probability_override=*/std::nullopt,
                           /*speech_level_override=*/std::nullopt,
                           *manager_with_override);

  // Make sure that an adaptation occurred.
  ASSERT_GT(manager->recommended_analog_level(), 0);

  // Check that the selected analog gain is the same for both controllers and
  // that it equals the minimum level reached when clipping is handled. That is
  // expected because the minimum microphone level override is less than the
  // minimum level used when clipping is detected.
  EXPECT_EQ(manager->recommended_analog_level(),
            manager_with_override->recommended_analog_level());
  EXPECT_EQ(manager_with_override->recommended_analog_level(),
            kDefaultAnalogConfig.clipped_level_min);
}

// Checks that, when the "WebRTC-Audio-AgcMinMicLevelExperiment" field trial is
// specified with a value lower than the `clipped_level_min`, the behavior of
// the analog gain controller is the same as that obtained when the field trial
// is not specified.
// TODO(webrtc:7494): Revisit the test after moving the number of override wait
// frames to APM config. The test passes but internally the gain update timing
// differs.
TEST(AgcManagerDirectTest,
     AgcMinMicLevelExperimentCompareMicLevelWithClippingWithRmsErrorOverride) {
  // Create and initialize two AGCs by specifying and leaving unspecified the
  // relevant field trial.
  const auto factory = []() {
    // Use a large clipped level step to more quickly decrease the analog gain
    // with clipping.
    AnalogAgcConfig config = kDefaultAnalogConfig;
    config.startup_min_volume = kInitialInputVolume;
    config.enable_digital_adaptive = false;
    config.clipped_level_step = 64;
    config.clipped_ratio_threshold = kClippedRatioThreshold;
    config.clipped_wait_frames = kClippedWaitFrames;
    auto controller =
        std::make_unique<AgcManagerDirect>(/*num_capture_channels=*/1, config);
    controller->Initialize();
    controller->set_stream_analog_level(kInitialInputVolume);
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

  CallPreProcessAndProcess(
      /*num_calls=*/400, audio_buffer,
      /*speech_probability_override=*/std::optional<float>(0.7f),
      /*speech_level_override=*/std::optional<float>(-18.0f), *manager);
  CallPreProcessAndProcess(
      /*num_calls=*/400, audio_buffer,
      /*speech_probability_override=*/std::optional<float>(0.7f),
      /*speech_level_override=*/std::optional<float>(-18.0f),
      *manager_with_override);

  // Make sure that an adaptation occurred.
  ASSERT_GT(manager->recommended_analog_level(), 0);

  // Check that the selected analog gain is the same for both controllers and
  // that it equals the minimum level reached when clipping is handled. That is
  // expected because the minimum microphone level override is less than the
  // minimum level used when clipping is detected.
  EXPECT_EQ(manager->recommended_analog_level(),
            manager_with_override->recommended_analog_level());
  EXPECT_EQ(manager_with_override->recommended_analog_level(),
            kDefaultAnalogConfig.clipped_level_min);
}

// TODO(bugs.webrtc.org/12774): Test the bahavior of `clipped_level_step`.
// TODO(bugs.webrtc.org/12774): Test the bahavior of `clipped_ratio_threshold`.
// TODO(bugs.webrtc.org/12774): Test the bahavior of `clipped_wait_frames`.
// Verifies that configurable clipping parameters are initialized as intended.
TEST_P(AgcManagerDirectParametrizedTest, ClippingParametersVerified) {
  if (IsRmsErrorOverridden()) {
    GTEST_SKIP() << "Skipped. RMS error override does not affect the test.";
  }

  std::unique_ptr<AgcManagerDirect> manager =
      CreateAgcManagerDirect(kInitialInputVolume, kClippedLevelStep,
                             kClippedRatioThreshold, kClippedWaitFrames);
  manager->Initialize();
  EXPECT_EQ(manager->clipped_level_step_, kClippedLevelStep);
  EXPECT_EQ(manager->clipped_ratio_threshold_, kClippedRatioThreshold);
  EXPECT_EQ(manager->clipped_wait_frames_, kClippedWaitFrames);
  std::unique_ptr<AgcManagerDirect> manager_custom =
      CreateAgcManagerDirect(kInitialInputVolume,
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
  if (IsRmsErrorOverridden()) {
    GTEST_SKIP() << "Skipped. RMS error override does not affect the test.";
  }

  // TODO(bugs.webrtc.org/12874): Use designated initializers once fixed.
  ClippingPredictorConfig config;
  config.enabled = false;

  std::unique_ptr<AgcManagerDirect> manager = CreateAgcManagerDirect(
      kInitialInputVolume, kClippedLevelStep, kClippedRatioThreshold,
      kClippedWaitFrames, config);
  manager->Initialize();
  EXPECT_FALSE(manager->clipping_predictor_enabled());
  EXPECT_FALSE(manager->use_clipping_predictor_step());
}

TEST_P(AgcManagerDirectParametrizedTest, ClippingPredictorDisabledByDefault) {
  if (IsRmsErrorOverridden()) {
    GTEST_SKIP() << "Skipped. RMS error override does not affect the test.";
  }

  constexpr ClippingPredictorConfig kDefaultConfig;
  EXPECT_FALSE(kDefaultConfig.enabled);
}

TEST_P(AgcManagerDirectParametrizedTest,
       EnableClippingPredictorEnablesClippingPredictor) {
  if (IsRmsErrorOverridden()) {
    GTEST_SKIP() << "Skipped. RMS error override does not affect the test.";
  }

  // TODO(bugs.webrtc.org/12874): Use designated initializers once fixed.
  ClippingPredictorConfig config;
  config.enabled = true;
  config.use_predicted_step = true;

  std::unique_ptr<AgcManagerDirect> manager = CreateAgcManagerDirect(
      kInitialInputVolume, kClippedLevelStep, kClippedRatioThreshold,
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
  EXPECT_EQ(manager.recommended_analog_level(), 255);
  manager.Process(audio_buffer, GetOverrideOrEmpty(kHighSpeechProbability),
                  GetOverrideOrEmpty(kSpeechLevelDbfs));
  CallPreProcessAudioBuffer(/*num_calls=*/10, /*peak_ratio=*/0.99f, manager);
  EXPECT_EQ(manager.recommended_analog_level(), 255);
  CallPreProcessAudioBuffer(/*num_calls=*/300, /*peak_ratio=*/0.99f, manager);
  EXPECT_EQ(manager.recommended_analog_level(), 255);
  CallPreProcessAudioBuffer(/*num_calls=*/10, /*peak_ratio=*/0.99f, manager);
  EXPECT_EQ(manager.recommended_analog_level(), 255);
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
  manager_with_prediction.Process(audio_buffer,
                                  GetOverrideOrEmpty(kHighSpeechProbability),
                                  GetOverrideOrEmpty(kSpeechLevelDbfs));
  manager_without_prediction.Process(audio_buffer,
                                     GetOverrideOrEmpty(kHighSpeechProbability),
                                     GetOverrideOrEmpty(kSpeechLevelDbfs));
  EXPECT_TRUE(manager_with_prediction.clipping_predictor_enabled());
  EXPECT_FALSE(manager_without_prediction.clipping_predictor_enabled());
  EXPECT_TRUE(manager_with_prediction.use_clipping_predictor_step());
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(), kInitialLevel);
  EXPECT_EQ(manager_without_prediction.recommended_analog_level(),
            kInitialLevel);

  // Expect a change in the analog level when the prediction step is used.
  CallPreProcessAudioBuffer(/*num_calls=*/10, kCloseToClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(/*num_calls=*/10, kCloseToClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            kInitialLevel - kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.recommended_analog_level(),
            kInitialLevel);

  // Expect no change during waiting.
  CallPreProcessAudioBuffer(kClippedWaitFrames, kCloseToClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(kClippedWaitFrames, kCloseToClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            kInitialLevel - kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.recommended_analog_level(),
            kInitialLevel);

  // Expect a change when the prediction step is used.
  CallPreProcessAudioBuffer(/*num_calls=*/10, kCloseToClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(/*num_calls=*/10, kCloseToClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            kInitialLevel - 2 * kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.recommended_analog_level(),
            kInitialLevel);

  // Expect no change when clipping is not detected or predicted.
  CallPreProcessAudioBuffer(2 * kClippedWaitFrames, kZeroPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(2 * kClippedWaitFrames, kZeroPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            kInitialLevel - 2 * kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.recommended_analog_level(),
            kInitialLevel);

  // Expect a change for clipping frames.
  CallPreProcessAudioBuffer(/*num_calls=*/1, kClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(/*num_calls=*/1, kClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            kInitialLevel - 3 * kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.recommended_analog_level(),
            kInitialLevel - kClippedLevelStep);

  // Expect no change during waiting.
  CallPreProcessAudioBuffer(kClippedWaitFrames, kClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(kClippedWaitFrames, kClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            kInitialLevel - 3 * kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.recommended_analog_level(),
            kInitialLevel - kClippedLevelStep);

  // Expect a change for clipping frames.
  CallPreProcessAudioBuffer(/*num_calls=*/1, kClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(/*num_calls=*/1, kClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            kInitialLevel - 4 * kClippedLevelStep);
  EXPECT_EQ(manager_without_prediction.recommended_analog_level(),
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
  manager_with_prediction.Process(audio_buffer,
                                  GetOverrideOrEmpty(kHighSpeechProbability),
                                  GetOverrideOrEmpty(kSpeechLevelDbfs));
  manager_without_prediction.Process(audio_buffer,
                                     GetOverrideOrEmpty(kHighSpeechProbability),
                                     GetOverrideOrEmpty(kSpeechLevelDbfs));

  EXPECT_TRUE(manager_with_prediction.clipping_predictor_enabled());
  EXPECT_FALSE(manager_without_prediction.clipping_predictor_enabled());
  EXPECT_FALSE(manager_with_prediction.use_clipping_predictor_step());
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(), kInitialLevel);
  EXPECT_EQ(manager_without_prediction.recommended_analog_level(),
            kInitialLevel);

  // Expect no change in the analog level for non-clipping frames.
  CallPreProcessAudioBuffer(/*num_calls=*/10, kCloseToClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(/*num_calls=*/10, kCloseToClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            manager_without_prediction.recommended_analog_level());

  // Expect no change for non-clipping frames.
  CallPreProcessAudioBuffer(kClippedWaitFrames, kCloseToClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(kClippedWaitFrames, kCloseToClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            manager_without_prediction.recommended_analog_level());

  // Expect no change for non-clipping frames.
  CallPreProcessAudioBuffer(/*num_calls=*/10, kCloseToClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(/*num_calls=*/10, kCloseToClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            manager_without_prediction.recommended_analog_level());

  // Expect no change when clipping is not detected or predicted.
  CallPreProcessAudioBuffer(2 * kClippedWaitFrames, kZeroPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(2 * kClippedWaitFrames, kZeroPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            manager_without_prediction.recommended_analog_level());

  // Expect a change for clipping frames.
  CallPreProcessAudioBuffer(/*num_calls=*/1, kClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(/*num_calls=*/1, kClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            manager_without_prediction.recommended_analog_level());

  // Expect no change during waiting.
  CallPreProcessAudioBuffer(kClippedWaitFrames, kClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(kClippedWaitFrames, kClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            manager_without_prediction.recommended_analog_level());

  // Expect a change for clipping frames.
  CallPreProcessAudioBuffer(/*num_calls=*/1, kClippingPeakRatio,
                            manager_with_prediction);
  CallPreProcessAudioBuffer(/*num_calls=*/1, kClippingPeakRatio,
                            manager_without_prediction);
  EXPECT_EQ(manager_with_prediction.recommended_analog_level(),
            manager_without_prediction.recommended_analog_level());
}

// Checks that passing an empty speech level and probability overrides to
// `Process()` has the same effect as passing no overrides.
TEST_P(AgcManagerDirectParametrizedTest, EmptyRmsErrorOverrideHasNoEffect) {
  AgcManagerDirect manager_1(kNumChannels, GetAnalogAgcTestConfig());
  AgcManagerDirect manager_2(kNumChannels, GetAnalogAgcTestConfig());
  manager_1.Initialize();
  manager_2.Initialize();

  constexpr int kAnalogLevel = 50;
  manager_1.set_stream_analog_level(kAnalogLevel);
  manager_2.set_stream_analog_level(kAnalogLevel);

  // Feed speech with low energy to trigger an upward adapation of the analog
  // level.
  constexpr int kNumFrames = 125;
  constexpr int kGainDb = -20;
  SpeechSamplesReader reader;

  // Check the initial input volume.
  ASSERT_EQ(manager_1.recommended_analog_level(), kAnalogLevel);
  ASSERT_EQ(manager_2.recommended_analog_level(), kAnalogLevel);

  reader.Feed(kNumFrames, kGainDb, std::nullopt, std::nullopt, manager_1);
  reader.Feed(kNumFrames, kGainDb, manager_2);

  // Check that the states are the same and adaptation occurs.
  EXPECT_EQ(manager_1.recommended_analog_level(),
            manager_2.recommended_analog_level());
  ASSERT_GT(manager_1.recommended_analog_level(), kAnalogLevel);
  EXPECT_EQ(manager_1.voice_probability(), manager_2.voice_probability());
  EXPECT_EQ(manager_1.frames_since_clipped_, manager_2.frames_since_clipped_);

  // Check that the states of the channel AGCs are the same.
  EXPECT_EQ(manager_1.num_channels(), manager_2.num_channels());
  for (int i = 0; i < manager_1.num_channels(); ++i) {
    EXPECT_EQ(manager_1.channel_agcs_[i]->recommended_analog_level(),
              manager_2.channel_agcs_[i]->recommended_analog_level());
    EXPECT_EQ(manager_1.channel_agcs_[i]->voice_probability(),
              manager_2.channel_agcs_[i]->voice_probability());
  }
}

// Checks that passing a non-empty speech level and probability overrides to
// `Process()` has an effect.
TEST_P(AgcManagerDirectParametrizedTest, NonEmptyRmsErrorOverrideHasEffect) {
  AgcManagerDirect manager_1(kNumChannels, GetAnalogAgcTestConfig());
  AgcManagerDirect manager_2(kNumChannels, GetAnalogAgcTestConfig());
  manager_1.Initialize();
  manager_2.Initialize();

  constexpr int kInputVolume = 128;
  manager_1.set_stream_analog_level(kInputVolume);
  manager_2.set_stream_analog_level(kInputVolume);

  // Feed speech with low energy to trigger an upward adapation of the input
  // volume.
  constexpr int kNumFrames = 125;
  constexpr int kGainDb = -20;
  SpeechSamplesReader reader;

  // Make sure that the feeding samples triggers an adaptation when no override
  // is specified.
  reader.Feed(kNumFrames, kGainDb, manager_1);
  ASSERT_GT(manager_1.recommended_analog_level(), kInputVolume);

  // Expect that feeding samples triggers an adaptation when the speech
  // probability and speech level overrides are specified.
  reader.Feed(kNumFrames, kGainDb,
              /*speech_probability_override=*/kHighSpeechProbability,
              /*speech_level_override=*/-45.0f, manager_2);
  EXPECT_GT(manager_2.recommended_analog_level(), kInputVolume);

  // The voice probability override does not affect the `voice_probability()`
  // getter.
  EXPECT_EQ(manager_1.voice_probability(), manager_2.voice_probability());
}

class AgcManagerDirectChannelSampleRateTest
    : public ::testing::TestWithParam<std::tuple<int, int>> {
 protected:
  int GetNumChannels() const { return std::get<0>(GetParam()); }
  int GetSampleRateHz() const { return std::get<1>(GetParam()); }
};

TEST_P(AgcManagerDirectChannelSampleRateTest, CheckIsAlive) {
  const int num_channels = GetNumChannels();
  const int sample_rate_hz = GetSampleRateHz();

  constexpr AnalogAgcConfig kConfig{.enabled = true,
                                    .clipping_predictor{.enabled = true}};
  AgcManagerDirect manager(num_channels, kConfig);
  manager.Initialize();
  AudioBuffer buffer(sample_rate_hz, num_channels, sample_rate_hz, num_channels,
                     sample_rate_hz, num_channels);

  constexpr int kStartupVolume = 100;
  int applied_initial_volume = kStartupVolume;

  // Trigger a downward adaptation with clipping.
  WriteAudioBufferSamples(/*samples_value=*/0.0f, /*clipped_ratio=*/0.5f,
                          buffer);
  const int initial_volume1 = applied_initial_volume;
  for (int i = 0; i < 400; ++i) {
    manager.set_stream_analog_level(applied_initial_volume);
    manager.AnalyzePreProcess(buffer);
    manager.Process(buffer, kLowSpeechProbability,
                    /*speech_level_dbfs=*/-20.0f);
    applied_initial_volume = manager.recommended_analog_level();
  }
  ASSERT_LT(manager.recommended_analog_level(), initial_volume1);

  // Fill in audio that does not clip.
  WriteAudioBufferSamples(/*samples_value=*/1234.5f, /*clipped_ratio=*/0.0f,
                          buffer);

  // Trigger an upward adaptation.
  const int initial_volume2 = manager.recommended_analog_level();
  for (int i = 0; i < kConfig.clipped_wait_frames; ++i) {
    manager.set_stream_analog_level(applied_initial_volume);
    manager.AnalyzePreProcess(buffer);
    manager.Process(buffer, kHighSpeechProbability,
                    /*speech_level_dbfs=*/-65.0f);
    applied_initial_volume = manager.recommended_analog_level();
  }
  EXPECT_GT(manager.recommended_analog_level(), initial_volume2);

  // Trigger a downward adaptation.
  const int initial_volume = manager.recommended_analog_level();
  for (int i = 0; i < 100; ++i) {
    manager.set_stream_analog_level(applied_initial_volume);
    manager.AnalyzePreProcess(buffer);
    manager.Process(buffer, kHighSpeechProbability,
                    /*speech_level_dbfs=*/-5.0f);
    applied_initial_volume = manager.recommended_analog_level();
  }
  EXPECT_LT(manager.recommended_analog_level(), initial_volume);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AgcManagerDirectChannelSampleRateTest,
    ::testing::Combine(::testing::Values(1, 2, 3, 6),
                       ::testing::Values(8000, 16000, 32000, 48000)));

}  // namespace webrtc
