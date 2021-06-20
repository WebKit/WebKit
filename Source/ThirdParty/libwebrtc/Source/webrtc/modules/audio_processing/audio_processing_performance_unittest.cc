/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <math.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/audio_processing_impl.h"
#include "modules/audio_processing/test/audio_processing_builder_for_testing.h"
#include "modules/audio_processing/test/test_utils.h"
#include "rtc_base/atomic_ops.h"
#include "rtc_base/event.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/random.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"
#include "test/testsupport/perf_test.h"

namespace webrtc {

namespace {

static const bool kPrintAllDurations = false;

class CallSimulator;

// Type of the render thread APM API call to use in the test.
enum class ProcessorType { kRender, kCapture };

// Variant of APM processing settings to use in the test.
enum class SettingsType {
  kDefaultApmDesktop,
  kDefaultApmMobile,
  kAllSubmodulesTurnedOff,
  kDefaultApmDesktopWithoutDelayAgnostic,
  kDefaultApmDesktopWithoutExtendedFilter
};

// Variables related to the audio data and formats.
struct AudioFrameData {
  explicit AudioFrameData(size_t max_frame_size) {
    // Set up the two-dimensional arrays needed for the APM API calls.
    input_framechannels.resize(2 * max_frame_size);
    input_frame.resize(2);
    input_frame[0] = &input_framechannels[0];
    input_frame[1] = &input_framechannels[max_frame_size];

    output_frame_channels.resize(2 * max_frame_size);
    output_frame.resize(2);
    output_frame[0] = &output_frame_channels[0];
    output_frame[1] = &output_frame_channels[max_frame_size];
  }

  std::vector<float> output_frame_channels;
  std::vector<float*> output_frame;
  std::vector<float> input_framechannels;
  std::vector<float*> input_frame;
  StreamConfig input_stream_config;
  StreamConfig output_stream_config;
};

// The configuration for the test.
struct SimulationConfig {
  SimulationConfig(int sample_rate_hz, SettingsType simulation_settings)
      : sample_rate_hz(sample_rate_hz),
        simulation_settings(simulation_settings) {}

  static std::vector<SimulationConfig> GenerateSimulationConfigs() {
    std::vector<SimulationConfig> simulation_configs;
#ifndef WEBRTC_ANDROID
    const SettingsType desktop_settings[] = {
        SettingsType::kDefaultApmDesktop, SettingsType::kAllSubmodulesTurnedOff,
        SettingsType::kDefaultApmDesktopWithoutDelayAgnostic,
        SettingsType::kDefaultApmDesktopWithoutExtendedFilter};

    const int desktop_sample_rates[] = {8000, 16000, 32000, 48000};

    for (auto sample_rate : desktop_sample_rates) {
      for (auto settings : desktop_settings) {
        simulation_configs.push_back(SimulationConfig(sample_rate, settings));
      }
    }
#endif

    const SettingsType mobile_settings[] = {SettingsType::kDefaultApmMobile};

    const int mobile_sample_rates[] = {8000, 16000};

    for (auto sample_rate : mobile_sample_rates) {
      for (auto settings : mobile_settings) {
        simulation_configs.push_back(SimulationConfig(sample_rate, settings));
      }
    }

    return simulation_configs;
  }

  std::string SettingsDescription() const {
    std::string description;
    switch (simulation_settings) {
      case SettingsType::kDefaultApmMobile:
        description = "DefaultApmMobile";
        break;
      case SettingsType::kDefaultApmDesktop:
        description = "DefaultApmDesktop";
        break;
      case SettingsType::kAllSubmodulesTurnedOff:
        description = "AllSubmodulesOff";
        break;
      case SettingsType::kDefaultApmDesktopWithoutDelayAgnostic:
        description = "DefaultApmDesktopWithoutDelayAgnostic";
        break;
      case SettingsType::kDefaultApmDesktopWithoutExtendedFilter:
        description = "DefaultApmDesktopWithoutExtendedFilter";
        break;
    }
    return description;
  }

  int sample_rate_hz = 16000;
  SettingsType simulation_settings = SettingsType::kDefaultApmDesktop;
};

// Handler for the frame counters.
class FrameCounters {
 public:
  void IncreaseRenderCounter() { rtc::AtomicOps::Increment(&render_count_); }

  void IncreaseCaptureCounter() { rtc::AtomicOps::Increment(&capture_count_); }

  int CaptureMinusRenderCounters() const {
    // The return value will be approximate, but that's good enough since
    // by the time we return the value, it's not guaranteed to be correct
    // anyway.
    return rtc::AtomicOps::AcquireLoad(&capture_count_) -
           rtc::AtomicOps::AcquireLoad(&render_count_);
  }

  int RenderMinusCaptureCounters() const {
    return -CaptureMinusRenderCounters();
  }

  bool BothCountersExceedeThreshold(int threshold) const {
    // TODO(tommi): We could use an event to signal this so that we don't need
    // to be polling from the main thread and possibly steal cycles.
    const int capture_count = rtc::AtomicOps::AcquireLoad(&capture_count_);
    const int render_count = rtc::AtomicOps::AcquireLoad(&render_count_);
    return (render_count > threshold && capture_count > threshold);
  }

 private:
  int render_count_ = 0;
  int capture_count_ = 0;
};

// Class that represents a flag that can only be raised.
class LockedFlag {
 public:
  bool get_flag() const { return rtc::AtomicOps::AcquireLoad(&flag_); }

  void set_flag() {
    if (!get_flag())  // read-only operation to avoid affecting the cache-line.
      rtc::AtomicOps::CompareAndSwap(&flag_, 0, 1);
  }

 private:
  int flag_ = 0;
};

// Parent class for the thread processors.
class TimedThreadApiProcessor {
 public:
  TimedThreadApiProcessor(ProcessorType processor_type,
                          Random* rand_gen,
                          FrameCounters* shared_counters_state,
                          LockedFlag* capture_call_checker,
                          CallSimulator* test_framework,
                          const SimulationConfig* simulation_config,
                          AudioProcessing* apm,
                          int num_durations_to_store,
                          float input_level,
                          int num_channels)
      : rand_gen_(rand_gen),
        frame_counters_(shared_counters_state),
        capture_call_checker_(capture_call_checker),
        test_(test_framework),
        simulation_config_(simulation_config),
        apm_(apm),
        frame_data_(kMaxFrameSize),
        clock_(webrtc::Clock::GetRealTimeClock()),
        num_durations_to_store_(num_durations_to_store),
        input_level_(input_level),
        processor_type_(processor_type),
        num_channels_(num_channels) {
    api_call_durations_.reserve(num_durations_to_store_);
  }

  // Implements the callback functionality for the threads.
  bool Process();

  // Method for printing out the simulation statistics.
  void print_processor_statistics(const std::string& processor_name) const {
    const std::string modifier = "_api_call_duration";

    const std::string sample_rate_name =
        "_" + std::to_string(simulation_config_->sample_rate_hz) + "Hz";

    webrtc::test::PrintResultMeanAndError(
        "apm_timing", sample_rate_name, processor_name, GetDurationAverage(),
        GetDurationStandardDeviation(), "us", false);

    if (kPrintAllDurations) {
      webrtc::test::PrintResultList("apm_call_durations", sample_rate_name,
                                    processor_name, api_call_durations_, "us",
                                    false);
    }
  }

  void AddDuration(int64_t duration) {
    if (api_call_durations_.size() < num_durations_to_store_) {
      api_call_durations_.push_back(duration);
    }
  }

 private:
  static const int kMaxCallDifference = 10;
  static const int kMaxFrameSize = 480;
  static const int kNumInitializationFrames = 5;

  int64_t GetDurationStandardDeviation() const {
    double variance = 0;
    const int64_t average_duration = GetDurationAverage();
    for (size_t k = kNumInitializationFrames; k < api_call_durations_.size();
         k++) {
      int64_t tmp = api_call_durations_[k] - average_duration;
      variance += static_cast<double>(tmp * tmp);
    }
    const int denominator = rtc::checked_cast<int>(api_call_durations_.size()) -
                            kNumInitializationFrames;
    return (denominator > 0
                ? rtc::checked_cast<int64_t>(sqrt(variance / denominator))
                : -1);
  }

  int64_t GetDurationAverage() const {
    int64_t average_duration = 0;
    for (size_t k = kNumInitializationFrames; k < api_call_durations_.size();
         k++) {
      average_duration += api_call_durations_[k];
    }
    const int denominator = rtc::checked_cast<int>(api_call_durations_.size()) -
                            kNumInitializationFrames;
    return (denominator > 0 ? average_duration / denominator : -1);
  }

  int ProcessCapture() {
    // Set the stream delay.
    apm_->set_stream_delay_ms(30);

    // Call and time the specified capture side API processing method.
    const int64_t start_time = clock_->TimeInMicroseconds();
    const int result = apm_->ProcessStream(
        &frame_data_.input_frame[0], frame_data_.input_stream_config,
        frame_data_.output_stream_config, &frame_data_.output_frame[0]);
    const int64_t end_time = clock_->TimeInMicroseconds();

    frame_counters_->IncreaseCaptureCounter();

    AddDuration(end_time - start_time);

    if (first_process_call_) {
      // Flag that the capture side has been called at least once
      // (needed to ensure that a capture call has been done
      // before the first render call is performed (implicitly
      // required by the APM API).
      capture_call_checker_->set_flag();
      first_process_call_ = false;
    }
    return result;
  }

  bool ReadyToProcessCapture() {
    return (frame_counters_->CaptureMinusRenderCounters() <=
            kMaxCallDifference);
  }

  int ProcessRender() {
    // Call and time the specified render side API processing method.
    const int64_t start_time = clock_->TimeInMicroseconds();
    const int result = apm_->ProcessReverseStream(
        &frame_data_.input_frame[0], frame_data_.input_stream_config,
        frame_data_.output_stream_config, &frame_data_.output_frame[0]);
    const int64_t end_time = clock_->TimeInMicroseconds();
    frame_counters_->IncreaseRenderCounter();

    AddDuration(end_time - start_time);

    return result;
  }

  bool ReadyToProcessRender() {
    // Do not process until at least one capture call has been done.
    // (implicitly required by the APM API).
    if (first_process_call_ && !capture_call_checker_->get_flag()) {
      return false;
    }

    // Ensure that the number of render and capture calls do not differ too
    // much.
    if (frame_counters_->RenderMinusCaptureCounters() > kMaxCallDifference) {
      return false;
    }

    first_process_call_ = false;
    return true;
  }

  void PrepareFrame() {
    // Lambda function for populating a float multichannel audio frame
    // with random data.
    auto populate_audio_frame = [](float amplitude, size_t num_channels,
                                   size_t samples_per_channel, Random* rand_gen,
                                   float** frame) {
      for (size_t ch = 0; ch < num_channels; ch++) {
        for (size_t k = 0; k < samples_per_channel; k++) {
          // Store random float number with a value between +-amplitude.
          frame[ch][k] = amplitude * (2 * rand_gen->Rand<float>() - 1);
        }
      }
    };

    // Prepare the audio input data and metadata.
    frame_data_.input_stream_config.set_sample_rate_hz(
        simulation_config_->sample_rate_hz);
    frame_data_.input_stream_config.set_num_channels(num_channels_);
    frame_data_.input_stream_config.set_has_keyboard(false);
    populate_audio_frame(input_level_, num_channels_,
                         (simulation_config_->sample_rate_hz *
                          AudioProcessing::kChunkSizeMs / 1000),
                         rand_gen_, &frame_data_.input_frame[0]);

    // Prepare the float audio output data and metadata.
    frame_data_.output_stream_config.set_sample_rate_hz(
        simulation_config_->sample_rate_hz);
    frame_data_.output_stream_config.set_num_channels(1);
    frame_data_.output_stream_config.set_has_keyboard(false);
  }

  bool ReadyToProcess() {
    switch (processor_type_) {
      case ProcessorType::kRender:
        return ReadyToProcessRender();

      case ProcessorType::kCapture:
        return ReadyToProcessCapture();
    }

    // Should not be reached, but the return statement is needed for the code to
    // build successfully on Android.
    RTC_NOTREACHED();
    return false;
  }

  Random* rand_gen_ = nullptr;
  FrameCounters* frame_counters_ = nullptr;
  LockedFlag* capture_call_checker_ = nullptr;
  CallSimulator* test_ = nullptr;
  const SimulationConfig* const simulation_config_ = nullptr;
  AudioProcessing* apm_ = nullptr;
  AudioFrameData frame_data_;
  webrtc::Clock* clock_;
  const size_t num_durations_to_store_;
  std::vector<double> api_call_durations_;
  const float input_level_;
  bool first_process_call_ = true;
  const ProcessorType processor_type_;
  const int num_channels_ = 1;
};

// Class for managing the test simulation.
class CallSimulator : public ::testing::TestWithParam<SimulationConfig> {
 public:
  CallSimulator()
      : rand_gen_(42U),
        simulation_config_(static_cast<SimulationConfig>(GetParam())) {}

  // Run the call simulation with a timeout.
  bool Run() {
    StartThreads();

    bool result = test_complete_.Wait(kTestTimeout);

    StopThreads();

    render_thread_state_->print_processor_statistics(
        simulation_config_.SettingsDescription() + "_render");
    capture_thread_state_->print_processor_statistics(
        simulation_config_.SettingsDescription() + "_capture");

    return result;
  }

  // Tests whether all the required render and capture side calls have been
  // done.
  bool MaybeEndTest() {
    if (frame_counters_.BothCountersExceedeThreshold(kMinNumFramesToProcess)) {
      test_complete_.Set();
      return true;
    }
    return false;
  }

 private:
  static const float kCaptureInputFloatLevel;
  static const float kRenderInputFloatLevel;
  static const int kMinNumFramesToProcess = 150;
  static const int32_t kTestTimeout = 3 * 10 * kMinNumFramesToProcess;

  // Stop all running threads.
  void StopThreads() {
    render_thread_.Finalize();
    capture_thread_.Finalize();
  }

  // Simulator and APM setup.
  void SetUp() override {
    // Lambda function for setting the default APM runtime settings for desktop.
    auto set_default_desktop_apm_runtime_settings = [](AudioProcessing* apm) {
      AudioProcessing::Config apm_config = apm->GetConfig();
      apm_config.echo_canceller.enabled = true;
      apm_config.echo_canceller.mobile_mode = false;
      apm_config.noise_suppression.enabled = true;
      apm_config.gain_controller1.enabled = true;
      apm_config.gain_controller1.mode =
          AudioProcessing::Config::GainController1::kAdaptiveDigital;
      apm_config.level_estimation.enabled = true;
      apm_config.voice_detection.enabled = true;
      apm->ApplyConfig(apm_config);
    };

    // Lambda function for setting the default APM runtime settings for mobile.
    auto set_default_mobile_apm_runtime_settings = [](AudioProcessing* apm) {
      AudioProcessing::Config apm_config = apm->GetConfig();
      apm_config.echo_canceller.enabled = true;
      apm_config.echo_canceller.mobile_mode = true;
      apm_config.noise_suppression.enabled = true;
      apm_config.gain_controller1.mode =
          AudioProcessing::Config::GainController1::kAdaptiveDigital;
      apm_config.level_estimation.enabled = true;
      apm_config.voice_detection.enabled = true;
      apm->ApplyConfig(apm_config);
    };

    // Lambda function for turning off all of the APM runtime settings
    // submodules.
    auto turn_off_default_apm_runtime_settings = [](AudioProcessing* apm) {
      AudioProcessing::Config apm_config = apm->GetConfig();
      apm_config.echo_canceller.enabled = false;
      apm_config.gain_controller1.enabled = false;
      apm_config.level_estimation.enabled = false;
      apm_config.noise_suppression.enabled = false;
      apm_config.voice_detection.enabled = false;
      apm->ApplyConfig(apm_config);
    };

    int num_capture_channels = 1;
    switch (simulation_config_.simulation_settings) {
      case SettingsType::kDefaultApmMobile: {
        apm_.reset(AudioProcessingBuilderForTesting().Create());
        ASSERT_TRUE(!!apm_);
        set_default_mobile_apm_runtime_settings(apm_.get());
        break;
      }
      case SettingsType::kDefaultApmDesktop: {
        Config config;
        apm_.reset(AudioProcessingBuilderForTesting().Create(config));
        ASSERT_TRUE(!!apm_);
        set_default_desktop_apm_runtime_settings(apm_.get());
        break;
      }
      case SettingsType::kAllSubmodulesTurnedOff: {
        apm_.reset(AudioProcessingBuilderForTesting().Create());
        ASSERT_TRUE(!!apm_);
        turn_off_default_apm_runtime_settings(apm_.get());
        break;
      }
      case SettingsType::kDefaultApmDesktopWithoutDelayAgnostic: {
        Config config;
        apm_.reset(AudioProcessingBuilderForTesting().Create(config));
        ASSERT_TRUE(!!apm_);
        set_default_desktop_apm_runtime_settings(apm_.get());
        break;
      }
      case SettingsType::kDefaultApmDesktopWithoutExtendedFilter: {
        Config config;
        apm_.reset(AudioProcessingBuilderForTesting().Create(config));
        ASSERT_TRUE(!!apm_);
        set_default_desktop_apm_runtime_settings(apm_.get());
        break;
      }
    }

    render_thread_state_.reset(new TimedThreadApiProcessor(
        ProcessorType::kRender, &rand_gen_, &frame_counters_,
        &capture_call_checker_, this, &simulation_config_, apm_.get(),
        kMinNumFramesToProcess, kRenderInputFloatLevel, 1));
    capture_thread_state_.reset(new TimedThreadApiProcessor(
        ProcessorType::kCapture, &rand_gen_, &frame_counters_,
        &capture_call_checker_, this, &simulation_config_, apm_.get(),
        kMinNumFramesToProcess, kCaptureInputFloatLevel, num_capture_channels));
  }

  // Start the threads used in the test.
  void StartThreads() {
    const auto attributes =
        rtc::ThreadAttributes().SetPriority(rtc::ThreadPriority::kRealtime);
    render_thread_ = rtc::PlatformThread::SpawnJoinable(
        [this] {
          while (render_thread_state_->Process()) {
          }
        },
        "render", attributes);
    capture_thread_ = rtc::PlatformThread::SpawnJoinable(
        [this] {
          while (capture_thread_state_->Process()) {
          }
        },
        "capture", attributes);
  }

  // Event handler for the test.
  rtc::Event test_complete_;

  // Thread related variables.
  Random rand_gen_;

  std::unique_ptr<AudioProcessing> apm_;
  const SimulationConfig simulation_config_;
  FrameCounters frame_counters_;
  LockedFlag capture_call_checker_;
  std::unique_ptr<TimedThreadApiProcessor> render_thread_state_;
  std::unique_ptr<TimedThreadApiProcessor> capture_thread_state_;
  rtc::PlatformThread render_thread_;
  rtc::PlatformThread capture_thread_;
};

// Implements the callback functionality for the threads.
bool TimedThreadApiProcessor::Process() {
  PrepareFrame();

  // Wait in a spinlock manner until it is ok to start processing.
  // Note that SleepMs is not applicable since it only allows sleeping
  // on a millisecond basis which is too long.
  // TODO(tommi): This loop may affect the performance of the test that it's
  // meant to measure.  See if we could use events instead to signal readiness.
  while (!ReadyToProcess()) {
  }

  int result = AudioProcessing::kNoError;
  switch (processor_type_) {
    case ProcessorType::kRender:
      result = ProcessRender();
      break;
    case ProcessorType::kCapture:
      result = ProcessCapture();
      break;
  }

  EXPECT_EQ(result, AudioProcessing::kNoError);

  return !test_->MaybeEndTest();
}

const float CallSimulator::kRenderInputFloatLevel = 0.5f;
const float CallSimulator::kCaptureInputFloatLevel = 0.03125f;
}  // anonymous namespace

// TODO(peah): Reactivate once issue 7712 has been resolved.
TEST_P(CallSimulator, DISABLED_ApiCallDurationTest) {
  // Run test and verify that it did not time out.
  EXPECT_TRUE(Run());
}

INSTANTIATE_TEST_SUITE_P(
    AudioProcessingPerformanceTest,
    CallSimulator,
    ::testing::ValuesIn(SimulationConfig::GenerateSimulationConfigs()));

}  // namespace webrtc
