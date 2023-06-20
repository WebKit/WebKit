/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/audio_processing_impl.h"
#include "modules/audio_processing/test/audio_processing_builder_for_testing.h"
#include "modules/audio_processing/test/test_utils.h"
#include "rtc_base/event.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/random.h"
#include "rtc_base/synchronization/mutex.h"
#include "system_wrappers/include/sleep.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr int kMaxFrameSize = 480;
constexpr TimeDelta kTestTimeOutLimit = TimeDelta::Minutes(10);

class AudioProcessingImplLockTest;

// Type of the render thread APM API call to use in the test.
enum class RenderApiImpl {
  ProcessReverseStreamImplInteger,
  ProcessReverseStreamImplFloat,
  AnalyzeReverseStreamImplFloat,
};

// Type of the capture thread APM API call to use in the test.
enum class CaptureApiImpl { ProcessStreamImplInteger, ProcessStreamImplFloat };

// The runtime parameter setting scheme to use in the test.
enum class RuntimeParameterSettingScheme {
  SparseStreamMetadataChangeScheme,
  ExtremeStreamMetadataChangeScheme,
  FixedMonoStreamMetadataScheme,
  FixedStereoStreamMetadataScheme
};

// Variant of echo canceller settings to use in the test.
enum class AecType {
  BasicWebRtcAecSettings,
  AecTurnedOff,
  BasicWebRtcAecSettingsWithExtentedFilter,
  BasicWebRtcAecSettingsWithDelayAgnosticAec,
  BasicWebRtcAecSettingsWithAecMobile
};

// Thread-safe random number generator wrapper.
class RandomGenerator {
 public:
  RandomGenerator() : rand_gen_(42U) {}

  int RandInt(int min, int max) {
    MutexLock lock(&mutex_);
    return rand_gen_.Rand(min, max);
  }

  int RandInt(int max) {
    MutexLock lock(&mutex_);
    return rand_gen_.Rand(max);
  }

  float RandFloat() {
    MutexLock lock(&mutex_);
    return rand_gen_.Rand<float>();
  }

 private:
  Mutex mutex_;
  Random rand_gen_ RTC_GUARDED_BY(mutex_);
};

// Variables related to the audio data and formats.
struct AudioFrameData {
  explicit AudioFrameData(int max_frame_size) {
    // Set up the two-dimensional arrays needed for the APM API calls.
    input_framechannels.resize(2 * max_frame_size);
    input_frame.resize(2);
    input_frame[0] = &input_framechannels[0];
    input_frame[1] = &input_framechannels[max_frame_size];

    output_frame_channels.resize(2 * max_frame_size);
    output_frame.resize(2);
    output_frame[0] = &output_frame_channels[0];
    output_frame[1] = &output_frame_channels[max_frame_size];

    frame.resize(2 * max_frame_size);
  }

  std::vector<int16_t> frame;

  std::vector<float*> output_frame;
  std::vector<float> output_frame_channels;
  std::vector<float*> input_frame;
  std::vector<float> input_framechannels;

  int input_sample_rate_hz = 16000;
  int input_number_of_channels = 1;
  int output_sample_rate_hz = 16000;
  int output_number_of_channels = 1;
};

// The configuration for the test.
struct TestConfig {
  // Test case generator for the test configurations to use in the brief tests.
  static std::vector<TestConfig> GenerateBriefTestConfigs() {
    std::vector<TestConfig> test_configs;
    AecType aec_types[] = {AecType::BasicWebRtcAecSettingsWithDelayAgnosticAec,
                           AecType::BasicWebRtcAecSettingsWithAecMobile};
    for (auto aec_type : aec_types) {
      TestConfig test_config;
      test_config.aec_type = aec_type;

      test_config.min_number_of_calls = 300;

      // Perform tests only with the extreme runtime parameter setting scheme.
      test_config.runtime_parameter_setting_scheme =
          RuntimeParameterSettingScheme::ExtremeStreamMetadataChangeScheme;

      // Only test 16 kHz for this test suite.
      test_config.initial_sample_rate_hz = 16000;

      // Create test config for the Int16 processing API function set.
      test_config.render_api_function =
          RenderApiImpl::ProcessReverseStreamImplInteger;
      test_config.capture_api_function =
          CaptureApiImpl::ProcessStreamImplInteger;
      test_configs.push_back(test_config);

      // Create test config for the StreamConfig processing API function set.
      test_config.render_api_function =
          RenderApiImpl::ProcessReverseStreamImplFloat;
      test_config.capture_api_function = CaptureApiImpl::ProcessStreamImplFloat;
      test_configs.push_back(test_config);
    }

    // Return the created test configurations.
    return test_configs;
  }

  // Test case generator for the test configurations to use in the extensive
  // tests.
  static std::vector<TestConfig> GenerateExtensiveTestConfigs() {
    // Lambda functions for the test config generation.
    auto add_processing_apis = [](TestConfig test_config) {
      struct AllowedApiCallCombinations {
        RenderApiImpl render_api;
        CaptureApiImpl capture_api;
      };

      const AllowedApiCallCombinations api_calls[] = {
          {RenderApiImpl::ProcessReverseStreamImplInteger,
           CaptureApiImpl::ProcessStreamImplInteger},
          {RenderApiImpl::ProcessReverseStreamImplFloat,
           CaptureApiImpl::ProcessStreamImplFloat},
          {RenderApiImpl::AnalyzeReverseStreamImplFloat,
           CaptureApiImpl::ProcessStreamImplFloat},
          {RenderApiImpl::ProcessReverseStreamImplInteger,
           CaptureApiImpl::ProcessStreamImplFloat},
          {RenderApiImpl::ProcessReverseStreamImplFloat,
           CaptureApiImpl::ProcessStreamImplInteger}};
      std::vector<TestConfig> out;
      for (auto api_call : api_calls) {
        test_config.render_api_function = api_call.render_api;
        test_config.capture_api_function = api_call.capture_api;
        out.push_back(test_config);
      }
      return out;
    };

    auto add_aec_settings = [](const std::vector<TestConfig>& in) {
      std::vector<TestConfig> out;
      AecType aec_types[] = {
          AecType::BasicWebRtcAecSettings, AecType::AecTurnedOff,
          AecType::BasicWebRtcAecSettingsWithExtentedFilter,
          AecType::BasicWebRtcAecSettingsWithDelayAgnosticAec,
          AecType::BasicWebRtcAecSettingsWithAecMobile};
      for (auto test_config : in) {
        // Due to a VisualStudio 2015 compiler issue, the internal loop
        // variable here cannot override a previously defined name.
        // In other words "type" cannot be named "aec_type" here.
        // https://connect.microsoft.com/VisualStudio/feedback/details/2291755
        for (auto type : aec_types) {
          test_config.aec_type = type;
          out.push_back(test_config);
        }
      }
      return out;
    };

    auto add_settings_scheme = [](const std::vector<TestConfig>& in) {
      std::vector<TestConfig> out;
      RuntimeParameterSettingScheme schemes[] = {
          RuntimeParameterSettingScheme::SparseStreamMetadataChangeScheme,
          RuntimeParameterSettingScheme::ExtremeStreamMetadataChangeScheme,
          RuntimeParameterSettingScheme::FixedMonoStreamMetadataScheme,
          RuntimeParameterSettingScheme::FixedStereoStreamMetadataScheme};

      for (auto test_config : in) {
        for (auto scheme : schemes) {
          test_config.runtime_parameter_setting_scheme = scheme;
          out.push_back(test_config);
        }
      }
      return out;
    };

    auto add_sample_rates = [](const std::vector<TestConfig>& in) {
      const int sample_rates[] = {8000, 16000, 32000, 48000};

      std::vector<TestConfig> out;
      for (auto test_config : in) {
        auto available_rates =
            (test_config.aec_type ==
                     AecType::BasicWebRtcAecSettingsWithAecMobile
                 ? rtc::ArrayView<const int>(sample_rates, 2)
                 : rtc::ArrayView<const int>(sample_rates));

        for (auto rate : available_rates) {
          test_config.initial_sample_rate_hz = rate;
          out.push_back(test_config);
        }
      }
      return out;
    };

    // Generate test configurations of the relevant combinations of the
    // parameters to
    // test.
    TestConfig test_config;
    test_config.min_number_of_calls = 10000;
    return add_sample_rates(add_settings_scheme(
        add_aec_settings(add_processing_apis(test_config))));
  }

  RenderApiImpl render_api_function =
      RenderApiImpl::ProcessReverseStreamImplFloat;
  CaptureApiImpl capture_api_function = CaptureApiImpl::ProcessStreamImplFloat;
  RuntimeParameterSettingScheme runtime_parameter_setting_scheme =
      RuntimeParameterSettingScheme::ExtremeStreamMetadataChangeScheme;
  int initial_sample_rate_hz = 16000;
  AecType aec_type = AecType::BasicWebRtcAecSettingsWithDelayAgnosticAec;
  int min_number_of_calls = 300;
};

// Handler for the frame counters.
class FrameCounters {
 public:
  void IncreaseRenderCounter() {
    MutexLock lock(&mutex_);
    render_count++;
  }

  void IncreaseCaptureCounter() {
    MutexLock lock(&mutex_);
    capture_count++;
  }

  int GetCaptureCounter() const {
    MutexLock lock(&mutex_);
    return capture_count;
  }

  int GetRenderCounter() const {
    MutexLock lock(&mutex_);
    return render_count;
  }

  int CaptureMinusRenderCounters() const {
    MutexLock lock(&mutex_);
    return capture_count - render_count;
  }

  int RenderMinusCaptureCounters() const {
    return -CaptureMinusRenderCounters();
  }

  bool BothCountersExceedeThreshold(int threshold) {
    MutexLock lock(&mutex_);
    return (render_count > threshold && capture_count > threshold);
  }

 private:
  mutable Mutex mutex_;
  int render_count RTC_GUARDED_BY(mutex_) = 0;
  int capture_count RTC_GUARDED_BY(mutex_) = 0;
};

// Class for handling the capture side processing.
class CaptureProcessor {
 public:
  CaptureProcessor(int max_frame_size,
                   RandomGenerator* rand_gen,
                   rtc::Event* render_call_event,
                   rtc::Event* capture_call_event,
                   FrameCounters* shared_counters_state,
                   const TestConfig* test_config,
                   AudioProcessing* apm);
  void Process();

 private:
  static constexpr int kMaxCallDifference = 10;
  static constexpr float kCaptureInputFloatLevel = 0.03125f;
  static constexpr int kCaptureInputFixLevel = 1024;

  void PrepareFrame();
  void CallApmCaptureSide();
  void ApplyRuntimeSettingScheme();

  RandomGenerator* const rand_gen_ = nullptr;
  rtc::Event* const render_call_event_ = nullptr;
  rtc::Event* const capture_call_event_ = nullptr;
  FrameCounters* const frame_counters_ = nullptr;
  const TestConfig* const test_config_ = nullptr;
  AudioProcessing* const apm_ = nullptr;
  AudioFrameData frame_data_;
};

// Class for handling the stats processing.
class StatsProcessor {
 public:
  StatsProcessor(RandomGenerator* rand_gen,
                 const TestConfig* test_config,
                 AudioProcessing* apm);
  void Process();

 private:
  RandomGenerator* rand_gen_ = nullptr;
  const TestConfig* const test_config_ = nullptr;
  AudioProcessing* apm_ = nullptr;
};

// Class for handling the render side processing.
class RenderProcessor {
 public:
  RenderProcessor(int max_frame_size,
                  RandomGenerator* rand_gen,
                  rtc::Event* render_call_event,
                  rtc::Event* capture_call_event,
                  FrameCounters* shared_counters_state,
                  const TestConfig* test_config,
                  AudioProcessing* apm);
  void Process();

 private:
  static constexpr int kMaxCallDifference = 10;
  static constexpr int kRenderInputFixLevel = 16384;
  static constexpr float kRenderInputFloatLevel = 0.5f;

  void PrepareFrame();
  void CallApmRenderSide();
  void ApplyRuntimeSettingScheme();

  RandomGenerator* const rand_gen_ = nullptr;
  rtc::Event* const render_call_event_ = nullptr;
  rtc::Event* const capture_call_event_ = nullptr;
  FrameCounters* const frame_counters_ = nullptr;
  const TestConfig* const test_config_ = nullptr;
  AudioProcessing* const apm_ = nullptr;
  AudioFrameData frame_data_;
  bool first_render_call_ = true;
};

class AudioProcessingImplLockTest
    : public ::testing::TestWithParam<TestConfig> {
 public:
  AudioProcessingImplLockTest();
  bool RunTest();
  bool MaybeEndTest();

 private:
  void SetUp() override;
  void TearDown() override;

  // Tests whether all the required render and capture side calls have been
  // done.
  bool TestDone() {
    return frame_counters_.BothCountersExceedeThreshold(
        test_config_.min_number_of_calls);
  }

  // Start the threads used in the test.
  void StartThreads() {
    const auto attributes =
        rtc::ThreadAttributes().SetPriority(rtc::ThreadPriority::kRealtime);
    render_thread_ = rtc::PlatformThread::SpawnJoinable(
        [this] {
          while (!MaybeEndTest())
            render_thread_state_.Process();
        },
        "render", attributes);
    capture_thread_ = rtc::PlatformThread::SpawnJoinable(
        [this] {
          while (!MaybeEndTest()) {
            capture_thread_state_.Process();
          }
        },
        "capture", attributes);

    stats_thread_ = rtc::PlatformThread::SpawnJoinable(
        [this] {
          while (!MaybeEndTest())
            stats_thread_state_.Process();
        },
        "stats", attributes);
  }

  // Event handlers for the test.
  rtc::Event test_complete_;
  rtc::Event render_call_event_;
  rtc::Event capture_call_event_;

  // Thread related variables.
  mutable RandomGenerator rand_gen_;

  const TestConfig test_config_;
  rtc::scoped_refptr<AudioProcessing> apm_;
  FrameCounters frame_counters_;
  RenderProcessor render_thread_state_;
  CaptureProcessor capture_thread_state_;
  StatsProcessor stats_thread_state_;
  rtc::PlatformThread render_thread_;
  rtc::PlatformThread capture_thread_;
  rtc::PlatformThread stats_thread_;
};

// Sleeps a random time between 0 and max_sleep milliseconds.
void SleepRandomMs(int max_sleep, RandomGenerator* rand_gen) {
  int sleeptime = rand_gen->RandInt(0, max_sleep);
  SleepMs(sleeptime);
}

// Populates a float audio frame with random data.
void PopulateAudioFrame(float** frame,
                        float amplitude,
                        size_t num_channels,
                        size_t samples_per_channel,
                        RandomGenerator* rand_gen) {
  for (size_t ch = 0; ch < num_channels; ch++) {
    for (size_t k = 0; k < samples_per_channel; k++) {
      // Store random 16 bit quantized float number between +-amplitude.
      frame[ch][k] = amplitude * (2 * rand_gen->RandFloat() - 1);
    }
  }
}

// Populates an integer audio frame with random data.
void PopulateAudioFrame(float amplitude,
                        size_t num_channels,
                        size_t samples_per_channel,
                        rtc::ArrayView<int16_t> frame,
                        RandomGenerator* rand_gen) {
  ASSERT_GT(amplitude, 0);
  ASSERT_LE(amplitude, 32767);
  for (size_t ch = 0; ch < num_channels; ch++) {
    for (size_t k = 0; k < samples_per_channel; k++) {
      // Store random 16 bit number between -(amplitude+1) and
      // amplitude.
      frame[k * ch] = rand_gen->RandInt(2 * amplitude + 1) - amplitude - 1;
    }
  }
}

AudioProcessing::Config GetApmTestConfig(AecType aec_type) {
  AudioProcessing::Config apm_config;
  apm_config.echo_canceller.enabled = aec_type != AecType::AecTurnedOff;
  apm_config.echo_canceller.mobile_mode =
      aec_type == AecType::BasicWebRtcAecSettingsWithAecMobile;
  apm_config.gain_controller1.enabled = true;
  apm_config.gain_controller1.mode =
      AudioProcessing::Config::GainController1::kAdaptiveDigital;
  apm_config.noise_suppression.enabled = true;
  return apm_config;
}

AudioProcessingImplLockTest::AudioProcessingImplLockTest()
    : test_config_(GetParam()),
      apm_(AudioProcessingBuilderForTesting()
               .SetConfig(GetApmTestConfig(test_config_.aec_type))
               .Create()),
      render_thread_state_(kMaxFrameSize,
                           &rand_gen_,
                           &render_call_event_,
                           &capture_call_event_,
                           &frame_counters_,
                           &test_config_,
                           apm_.get()),
      capture_thread_state_(kMaxFrameSize,
                            &rand_gen_,
                            &render_call_event_,
                            &capture_call_event_,
                            &frame_counters_,
                            &test_config_,
                            apm_.get()),
      stats_thread_state_(&rand_gen_, &test_config_, apm_.get()) {}

// Run the test with a timeout.
bool AudioProcessingImplLockTest::RunTest() {
  StartThreads();
  return test_complete_.Wait(kTestTimeOutLimit);
}

bool AudioProcessingImplLockTest::MaybeEndTest() {
  if (HasFatalFailure() || TestDone()) {
    test_complete_.Set();
    return true;
  }
  return false;
}

void AudioProcessingImplLockTest::SetUp() {}

void AudioProcessingImplLockTest::TearDown() {
  render_call_event_.Set();
  capture_call_event_.Set();
}

StatsProcessor::StatsProcessor(RandomGenerator* rand_gen,
                               const TestConfig* test_config,
                               AudioProcessing* apm)
    : rand_gen_(rand_gen), test_config_(test_config), apm_(apm) {}

// Implements the callback functionality for the statistics
// collection thread.
void StatsProcessor::Process() {
  SleepRandomMs(100, rand_gen_);

  AudioProcessing::Config apm_config = apm_->GetConfig();
  if (test_config_->aec_type != AecType::AecTurnedOff) {
    EXPECT_TRUE(apm_config.echo_canceller.enabled);
    EXPECT_EQ(apm_config.echo_canceller.mobile_mode,
              (test_config_->aec_type ==
               AecType::BasicWebRtcAecSettingsWithAecMobile));
  } else {
    EXPECT_FALSE(apm_config.echo_canceller.enabled);
  }
  EXPECT_TRUE(apm_config.gain_controller1.enabled);
  EXPECT_TRUE(apm_config.noise_suppression.enabled);

  // The below return value is not testable.
  apm_->GetStatistics();
}

CaptureProcessor::CaptureProcessor(int max_frame_size,
                                   RandomGenerator* rand_gen,
                                   rtc::Event* render_call_event,
                                   rtc::Event* capture_call_event,
                                   FrameCounters* shared_counters_state,
                                   const TestConfig* test_config,
                                   AudioProcessing* apm)
    : rand_gen_(rand_gen),
      render_call_event_(render_call_event),
      capture_call_event_(capture_call_event),
      frame_counters_(shared_counters_state),
      test_config_(test_config),
      apm_(apm),
      frame_data_(max_frame_size) {}

// Implements the callback functionality for the capture thread.
void CaptureProcessor::Process() {
  // Sleep a random time to simulate thread jitter.
  SleepRandomMs(3, rand_gen_);

  // Ensure that the number of render and capture calls do not
  // differ too much.
  if (frame_counters_->CaptureMinusRenderCounters() > kMaxCallDifference) {
    render_call_event_->Wait(rtc::Event::kForever);
  }

  // Apply any specified capture side APM non-processing runtime calls.
  ApplyRuntimeSettingScheme();

  // Apply the capture side processing call.
  CallApmCaptureSide();

  // Increase the number of capture-side calls.
  frame_counters_->IncreaseCaptureCounter();

  // Flag to the render thread that another capture API call has occurred
  // by triggering this threads call event.
  capture_call_event_->Set();
}

// Prepares a frame with relevant audio data and metadata.
void CaptureProcessor::PrepareFrame() {
  // Restrict to a common fixed sample rate if the integer
  // interface is used.
  if (test_config_->capture_api_function ==
      CaptureApiImpl::ProcessStreamImplInteger) {
    frame_data_.input_sample_rate_hz = test_config_->initial_sample_rate_hz;
    frame_data_.output_sample_rate_hz = test_config_->initial_sample_rate_hz;
  }

  // Prepare the audio data.
  StreamConfig input_stream_config(frame_data_.input_sample_rate_hz,
                                   frame_data_.input_number_of_channels);

  PopulateAudioFrame(kCaptureInputFixLevel, input_stream_config.num_channels(),
                     input_stream_config.num_frames(), frame_data_.frame,
                     rand_gen_);

  PopulateAudioFrame(&frame_data_.input_frame[0], kCaptureInputFloatLevel,
                     input_stream_config.num_channels(),
                     input_stream_config.num_frames(), rand_gen_);
}

// Applies the capture side processing API call.
void CaptureProcessor::CallApmCaptureSide() {
  // Prepare a proper capture side processing API call input.
  PrepareFrame();

  // Set the stream delay.
  apm_->set_stream_delay_ms(30);

  // Set the analog level.
  apm_->set_stream_analog_level(80);

  // Call the specified capture side API processing method.
  StreamConfig input_stream_config(frame_data_.input_sample_rate_hz,
                                   frame_data_.input_number_of_channels);
  StreamConfig output_stream_config(frame_data_.output_sample_rate_hz,
                                    frame_data_.output_number_of_channels);
  int result = AudioProcessing::kNoError;
  switch (test_config_->capture_api_function) {
    case CaptureApiImpl::ProcessStreamImplInteger:
      result =
          apm_->ProcessStream(frame_data_.frame.data(), input_stream_config,
                              output_stream_config, frame_data_.frame.data());
      break;
    case CaptureApiImpl::ProcessStreamImplFloat:
      result = apm_->ProcessStream(&frame_data_.input_frame[0],
                                   input_stream_config, output_stream_config,
                                   &frame_data_.output_frame[0]);
      break;
    default:
      FAIL();
  }

  // Retrieve the new analog level.
  apm_->recommended_stream_analog_level();

  // Check the return code for error.
  ASSERT_EQ(AudioProcessing::kNoError, result);
}

// Applies any runtime capture APM API calls and audio stream characteristics
// specified by the scheme for the test.
void CaptureProcessor::ApplyRuntimeSettingScheme() {
  const int capture_count_local = frame_counters_->GetCaptureCounter();

  // Update the number of channels and sample rates for the input and output.
  // Note that the counts frequencies for when to set parameters
  // are set using prime numbers in order to ensure that the
  // permutation scheme in the parameter setting changes.
  switch (test_config_->runtime_parameter_setting_scheme) {
    case RuntimeParameterSettingScheme::SparseStreamMetadataChangeScheme:
      if (capture_count_local == 0)
        frame_data_.input_sample_rate_hz = 16000;
      else if (capture_count_local % 11 == 0)
        frame_data_.input_sample_rate_hz = 32000;
      else if (capture_count_local % 73 == 0)
        frame_data_.input_sample_rate_hz = 48000;
      else if (capture_count_local % 89 == 0)
        frame_data_.input_sample_rate_hz = 16000;
      else if (capture_count_local % 97 == 0)
        frame_data_.input_sample_rate_hz = 8000;

      if (capture_count_local == 0)
        frame_data_.input_number_of_channels = 1;
      else if (capture_count_local % 4 == 0)
        frame_data_.input_number_of_channels =
            (frame_data_.input_number_of_channels == 1 ? 2 : 1);

      if (capture_count_local == 0)
        frame_data_.output_sample_rate_hz = 16000;
      else if (capture_count_local % 5 == 0)
        frame_data_.output_sample_rate_hz = 32000;
      else if (capture_count_local % 47 == 0)
        frame_data_.output_sample_rate_hz = 48000;
      else if (capture_count_local % 53 == 0)
        frame_data_.output_sample_rate_hz = 16000;
      else if (capture_count_local % 71 == 0)
        frame_data_.output_sample_rate_hz = 8000;

      if (capture_count_local == 0)
        frame_data_.output_number_of_channels = 1;
      else if (capture_count_local % 8 == 0)
        frame_data_.output_number_of_channels =
            (frame_data_.output_number_of_channels == 1 ? 2 : 1);
      break;
    case RuntimeParameterSettingScheme::ExtremeStreamMetadataChangeScheme:
      if (capture_count_local % 2 == 0) {
        frame_data_.input_number_of_channels = 1;
        frame_data_.input_sample_rate_hz = 16000;
        frame_data_.output_number_of_channels = 1;
        frame_data_.output_sample_rate_hz = 16000;
      } else {
        frame_data_.input_number_of_channels =
            (frame_data_.input_number_of_channels == 1 ? 2 : 1);
        if (frame_data_.input_sample_rate_hz == 8000)
          frame_data_.input_sample_rate_hz = 16000;
        else if (frame_data_.input_sample_rate_hz == 16000)
          frame_data_.input_sample_rate_hz = 32000;
        else if (frame_data_.input_sample_rate_hz == 32000)
          frame_data_.input_sample_rate_hz = 48000;
        else if (frame_data_.input_sample_rate_hz == 48000)
          frame_data_.input_sample_rate_hz = 8000;

        frame_data_.output_number_of_channels =
            (frame_data_.output_number_of_channels == 1 ? 2 : 1);
        if (frame_data_.output_sample_rate_hz == 8000)
          frame_data_.output_sample_rate_hz = 16000;
        else if (frame_data_.output_sample_rate_hz == 16000)
          frame_data_.output_sample_rate_hz = 32000;
        else if (frame_data_.output_sample_rate_hz == 32000)
          frame_data_.output_sample_rate_hz = 48000;
        else if (frame_data_.output_sample_rate_hz == 48000)
          frame_data_.output_sample_rate_hz = 8000;
      }
      break;
    case RuntimeParameterSettingScheme::FixedMonoStreamMetadataScheme:
      if (capture_count_local == 0) {
        frame_data_.input_sample_rate_hz = 16000;
        frame_data_.input_number_of_channels = 1;
        frame_data_.output_sample_rate_hz = 16000;
        frame_data_.output_number_of_channels = 1;
      }
      break;
    case RuntimeParameterSettingScheme::FixedStereoStreamMetadataScheme:
      if (capture_count_local == 0) {
        frame_data_.input_sample_rate_hz = 16000;
        frame_data_.input_number_of_channels = 2;
        frame_data_.output_sample_rate_hz = 16000;
        frame_data_.output_number_of_channels = 2;
      }
      break;
    default:
      FAIL();
  }

  // Call any specified runtime APM setter and
  // getter calls.
  switch (test_config_->runtime_parameter_setting_scheme) {
    case RuntimeParameterSettingScheme::SparseStreamMetadataChangeScheme:
    case RuntimeParameterSettingScheme::FixedMonoStreamMetadataScheme:
      break;
    case RuntimeParameterSettingScheme::ExtremeStreamMetadataChangeScheme:
    case RuntimeParameterSettingScheme::FixedStereoStreamMetadataScheme:
      if (capture_count_local % 2 == 0) {
        ASSERT_EQ(AudioProcessing::Error::kNoError,
                  apm_->set_stream_delay_ms(30));
        apm_->set_stream_key_pressed(true);
      } else {
        ASSERT_EQ(AudioProcessing::Error::kNoError,
                  apm_->set_stream_delay_ms(50));
        apm_->set_stream_key_pressed(false);
      }
      break;
    default:
      FAIL();
  }

  // Restric the number of output channels not to exceed
  // the number of input channels.
  frame_data_.output_number_of_channels =
      std::min(frame_data_.output_number_of_channels,
               frame_data_.input_number_of_channels);
}

RenderProcessor::RenderProcessor(int max_frame_size,
                                 RandomGenerator* rand_gen,
                                 rtc::Event* render_call_event,
                                 rtc::Event* capture_call_event,
                                 FrameCounters* shared_counters_state,
                                 const TestConfig* test_config,
                                 AudioProcessing* apm)
    : rand_gen_(rand_gen),
      render_call_event_(render_call_event),
      capture_call_event_(capture_call_event),
      frame_counters_(shared_counters_state),
      test_config_(test_config),
      apm_(apm),
      frame_data_(max_frame_size) {}

// Implements the callback functionality for the render thread.
void RenderProcessor::Process() {
  // Conditional wait to ensure that a capture call has been done
  // before the first render call is performed (implicitly
  // required by the APM API).
  if (first_render_call_) {
    capture_call_event_->Wait(rtc::Event::kForever);
    first_render_call_ = false;
  }

  // Sleep a random time to simulate thread jitter.
  SleepRandomMs(3, rand_gen_);

  // Ensure that the number of render and capture calls do not
  // differ too much.
  if (frame_counters_->RenderMinusCaptureCounters() > kMaxCallDifference) {
    capture_call_event_->Wait(rtc::Event::kForever);
  }

  // Apply any specified render side APM non-processing runtime calls.
  ApplyRuntimeSettingScheme();

  // Apply the render side processing call.
  CallApmRenderSide();

  // Increase the number of render-side calls.
  frame_counters_->IncreaseRenderCounter();

  // Flag to the capture thread that another render API call has occurred
  // by triggering this threads call event.
  render_call_event_->Set();
}

// Prepares the render side frame and the accompanying metadata
// with the appropriate information.
void RenderProcessor::PrepareFrame() {
  // Restrict to a common fixed sample rate if the integer interface is
  // used.
  if ((test_config_->render_api_function ==
       RenderApiImpl::ProcessReverseStreamImplInteger) ||
      (test_config_->aec_type !=
       AecType::BasicWebRtcAecSettingsWithAecMobile)) {
    frame_data_.input_sample_rate_hz = test_config_->initial_sample_rate_hz;
    frame_data_.output_sample_rate_hz = test_config_->initial_sample_rate_hz;
  }

  // Prepare the audio data.
  StreamConfig input_stream_config(frame_data_.input_sample_rate_hz,
                                   frame_data_.input_number_of_channels);

  PopulateAudioFrame(kRenderInputFixLevel, input_stream_config.num_channels(),
                     input_stream_config.num_frames(), frame_data_.frame,
                     rand_gen_);

  PopulateAudioFrame(&frame_data_.input_frame[0], kRenderInputFloatLevel,
                     input_stream_config.num_channels(),
                     input_stream_config.num_frames(), rand_gen_);
}

// Makes the render side processing API call.
void RenderProcessor::CallApmRenderSide() {
  // Prepare a proper render side processing API call input.
  PrepareFrame();

  // Call the specified render side API processing method.
  StreamConfig input_stream_config(frame_data_.input_sample_rate_hz,
                                   frame_data_.input_number_of_channels);
  StreamConfig output_stream_config(frame_data_.output_sample_rate_hz,
                                    frame_data_.output_number_of_channels);
  int result = AudioProcessing::kNoError;
  switch (test_config_->render_api_function) {
    case RenderApiImpl::ProcessReverseStreamImplInteger:
      result = apm_->ProcessReverseStream(
          frame_data_.frame.data(), input_stream_config, output_stream_config,
          frame_data_.frame.data());
      break;
    case RenderApiImpl::ProcessReverseStreamImplFloat:
      result = apm_->ProcessReverseStream(
          &frame_data_.input_frame[0], input_stream_config,
          output_stream_config, &frame_data_.output_frame[0]);
      break;
    case RenderApiImpl::AnalyzeReverseStreamImplFloat:
      result = apm_->AnalyzeReverseStream(&frame_data_.input_frame[0],
                                          input_stream_config);
      break;
    default:
      FAIL();
  }

  // Check the return code for error.
  ASSERT_EQ(AudioProcessing::kNoError, result);
}

// Applies any render capture side APM API calls and audio stream
// characteristics
// specified by the scheme for the test.
void RenderProcessor::ApplyRuntimeSettingScheme() {
  const int render_count_local = frame_counters_->GetRenderCounter();

  // Update the number of channels and sample rates for the input and output.
  // Note that the counts frequencies for when to set parameters
  // are set using prime numbers in order to ensure that the
  // permutation scheme in the parameter setting changes.
  switch (test_config_->runtime_parameter_setting_scheme) {
    case RuntimeParameterSettingScheme::SparseStreamMetadataChangeScheme:
      if (render_count_local == 0)
        frame_data_.input_sample_rate_hz = 16000;
      else if (render_count_local % 47 == 0)
        frame_data_.input_sample_rate_hz = 32000;
      else if (render_count_local % 71 == 0)
        frame_data_.input_sample_rate_hz = 48000;
      else if (render_count_local % 79 == 0)
        frame_data_.input_sample_rate_hz = 16000;
      else if (render_count_local % 83 == 0)
        frame_data_.input_sample_rate_hz = 8000;

      if (render_count_local == 0)
        frame_data_.input_number_of_channels = 1;
      else if (render_count_local % 4 == 0)
        frame_data_.input_number_of_channels =
            (frame_data_.input_number_of_channels == 1 ? 2 : 1);

      if (render_count_local == 0)
        frame_data_.output_sample_rate_hz = 16000;
      else if (render_count_local % 17 == 0)
        frame_data_.output_sample_rate_hz = 32000;
      else if (render_count_local % 19 == 0)
        frame_data_.output_sample_rate_hz = 48000;
      else if (render_count_local % 29 == 0)
        frame_data_.output_sample_rate_hz = 16000;
      else if (render_count_local % 61 == 0)
        frame_data_.output_sample_rate_hz = 8000;

      if (render_count_local == 0)
        frame_data_.output_number_of_channels = 1;
      else if (render_count_local % 8 == 0)
        frame_data_.output_number_of_channels =
            (frame_data_.output_number_of_channels == 1 ? 2 : 1);
      break;
    case RuntimeParameterSettingScheme::ExtremeStreamMetadataChangeScheme:
      if (render_count_local == 0) {
        frame_data_.input_number_of_channels = 1;
        frame_data_.input_sample_rate_hz = 16000;
        frame_data_.output_number_of_channels = 1;
        frame_data_.output_sample_rate_hz = 16000;
      } else {
        frame_data_.input_number_of_channels =
            (frame_data_.input_number_of_channels == 1 ? 2 : 1);
        if (frame_data_.input_sample_rate_hz == 8000)
          frame_data_.input_sample_rate_hz = 16000;
        else if (frame_data_.input_sample_rate_hz == 16000)
          frame_data_.input_sample_rate_hz = 32000;
        else if (frame_data_.input_sample_rate_hz == 32000)
          frame_data_.input_sample_rate_hz = 48000;
        else if (frame_data_.input_sample_rate_hz == 48000)
          frame_data_.input_sample_rate_hz = 8000;

        frame_data_.output_number_of_channels =
            (frame_data_.output_number_of_channels == 1 ? 2 : 1);
        if (frame_data_.output_sample_rate_hz == 8000)
          frame_data_.output_sample_rate_hz = 16000;
        else if (frame_data_.output_sample_rate_hz == 16000)
          frame_data_.output_sample_rate_hz = 32000;
        else if (frame_data_.output_sample_rate_hz == 32000)
          frame_data_.output_sample_rate_hz = 48000;
        else if (frame_data_.output_sample_rate_hz == 48000)
          frame_data_.output_sample_rate_hz = 8000;
      }
      break;
    case RuntimeParameterSettingScheme::FixedMonoStreamMetadataScheme:
      if (render_count_local == 0) {
        frame_data_.input_sample_rate_hz = 16000;
        frame_data_.input_number_of_channels = 1;
        frame_data_.output_sample_rate_hz = 16000;
        frame_data_.output_number_of_channels = 1;
      }
      break;
    case RuntimeParameterSettingScheme::FixedStereoStreamMetadataScheme:
      if (render_count_local == 0) {
        frame_data_.input_sample_rate_hz = 16000;
        frame_data_.input_number_of_channels = 2;
        frame_data_.output_sample_rate_hz = 16000;
        frame_data_.output_number_of_channels = 2;
      }
      break;
    default:
      FAIL();
  }

  // Restric the number of output channels not to exceed
  // the number of input channels.
  frame_data_.output_number_of_channels =
      std::min(frame_data_.output_number_of_channels,
               frame_data_.input_number_of_channels);
}

}  // namespace

TEST_P(AudioProcessingImplLockTest, LockTest) {
  // Run test and verify that it did not time out.
  ASSERT_TRUE(RunTest());
}

// Instantiate tests from the extreme test configuration set.
INSTANTIATE_TEST_SUITE_P(
    DISABLED_AudioProcessingImplLockExtensive,
    AudioProcessingImplLockTest,
    ::testing::ValuesIn(TestConfig::GenerateExtensiveTestConfigs()));

INSTANTIATE_TEST_SUITE_P(
    AudioProcessingImplLockBrief,
    AudioProcessingImplLockTest,
    ::testing::ValuesIn(TestConfig::GenerateBriefTestConfigs()));

}  // namespace webrtc
