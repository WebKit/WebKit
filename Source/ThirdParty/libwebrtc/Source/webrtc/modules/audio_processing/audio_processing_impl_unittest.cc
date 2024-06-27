/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/audio_processing_impl.h"

#include <algorithm>
#include <array>
#include <memory>
#include <tuple>

#include "absl/types/optional.h"
#include "api/audio/audio_processing.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "modules/audio_processing/optionally_built_submodule_creators.h"
#include "modules/audio_processing/test/audio_processing_builder_for_testing.h"
#include "modules/audio_processing/test/echo_canceller_test_tools.h"
#include "modules/audio_processing/test/echo_control_mock.h"
#include "modules/audio_processing/test/test_utils.h"
#include "rtc_base/checks.h"
#include "rtc_base/random.h"
#include "rtc_base/strings/string_builder.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Invoke;
using ::testing::NotNull;

class MockInitialize : public AudioProcessingImpl {
 public:
  MockInitialize() : AudioProcessingImpl() {}

  MOCK_METHOD(void, InitializeLocked, (), (override));
  void RealInitializeLocked() {
    AssertLockedForTest();
    AudioProcessingImpl::InitializeLocked();
  }

  MOCK_METHOD(void, AddRef, (), (const, override));
  MOCK_METHOD(RefCountReleaseStatus, Release, (), (const, override));
};

// Creates MockEchoControl instances and provides a raw pointer access to
// the next created one. The raw pointer is meant to be used with gmock.
// Returning a pointer of the next created MockEchoControl instance is necessary
// for the following reasons: (i) gmock expectations must be set before any call
// occurs, (ii) APM is initialized the first time that
// AudioProcessingImpl::ProcessStream() is called and the initialization leads
// to the creation of a new EchoControl object.
class MockEchoControlFactory : public EchoControlFactory {
 public:
  MockEchoControlFactory() : next_mock_(std::make_unique<MockEchoControl>()) {}
  // Returns a pointer to the next MockEchoControl that this factory creates.
  MockEchoControl* GetNext() const { return next_mock_.get(); }
  std::unique_ptr<EchoControl> Create(int sample_rate_hz,
                                      int num_render_channels,
                                      int num_capture_channels) override {
    std::unique_ptr<EchoControl> mock = std::move(next_mock_);
    next_mock_ = std::make_unique<MockEchoControl>();
    return mock;
  }

 private:
  std::unique_ptr<MockEchoControl> next_mock_;
};

// Mocks EchoDetector and records the first samples of the last analyzed render
// stream frame. Used to check what data is read by an EchoDetector
// implementation injected into an APM.
class TestEchoDetector : public EchoDetector {
 public:
  TestEchoDetector()
      : analyze_render_audio_called_(false),
        last_render_audio_first_sample_(0.f) {}
  ~TestEchoDetector() override = default;
  void AnalyzeRenderAudio(rtc::ArrayView<const float> render_audio) override {
    last_render_audio_first_sample_ = render_audio[0];
    analyze_render_audio_called_ = true;
  }
  void AnalyzeCaptureAudio(rtc::ArrayView<const float> capture_audio) override {
  }
  void Initialize(int capture_sample_rate_hz,
                  int num_capture_channels,
                  int render_sample_rate_hz,
                  int num_render_channels) override {}
  EchoDetector::Metrics GetMetrics() const override { return {}; }
  // Returns true if AnalyzeRenderAudio() has been called at least once.
  bool analyze_render_audio_called() const {
    return analyze_render_audio_called_;
  }
  // Returns the first sample of the last analyzed render frame.
  float last_render_audio_first_sample() const {
    return last_render_audio_first_sample_;
  }

 private:
  bool analyze_render_audio_called_;
  float last_render_audio_first_sample_;
};

// Mocks CustomProcessing and applies ProcessSample() to all the samples.
// Meant to be injected into an APM to modify samples in a known and detectable
// way.
class TestRenderPreProcessor : public CustomProcessing {
 public:
  TestRenderPreProcessor() = default;
  ~TestRenderPreProcessor() = default;
  void Initialize(int sample_rate_hz, int num_channels) override {}
  void Process(AudioBuffer* audio) override {
    for (size_t k = 0; k < audio->num_channels(); ++k) {
      rtc::ArrayView<float> channel_view(audio->channels()[k],
                                         audio->num_frames());
      std::transform(channel_view.begin(), channel_view.end(),
                     channel_view.begin(), ProcessSample);
    }
  }
  std::string ToString() const override { return "TestRenderPreProcessor"; }
  void SetRuntimeSetting(AudioProcessing::RuntimeSetting setting) override {}
  // Modifies a sample. This member is used in Process() to modify a frame and
  // it is publicly visible to enable tests.
  static constexpr float ProcessSample(float x) { return 2.f * x; }
};

// Runs `apm` input processing for volume adjustments for `num_frames` random
// frames starting from the volume `initial_volume`. This includes three steps:
// 1) Set the input volume 2) Process the stream 3) Set the new recommended
// input volume. Returns the new recommended input volume.
int ProcessInputVolume(AudioProcessing& apm,
                       int num_frames,
                       int initial_volume) {
  constexpr int kSampleRateHz = 48000;
  constexpr int kNumChannels = 1;
  std::array<float, kSampleRateHz / 100> buffer;
  float* channel_pointers[] = {buffer.data()};
  StreamConfig stream_config(/*sample_rate_hz=*/kSampleRateHz,
                             /*num_channels=*/kNumChannels);
  int recommended_input_volume = initial_volume;
  for (int i = 0; i < num_frames; ++i) {
    Random random_generator(2341U);
    RandomizeSampleVector(&random_generator, buffer);

    apm.set_stream_analog_level(recommended_input_volume);
    apm.ProcessStream(channel_pointers, stream_config, stream_config,
                      channel_pointers);
    recommended_input_volume = apm.recommended_stream_analog_level();
  }
  return recommended_input_volume;
}

}  // namespace

TEST(AudioProcessingImplTest, AudioParameterChangeTriggersInit) {
  MockInitialize mock;
  ON_CALL(mock, InitializeLocked)
      .WillByDefault(Invoke(&mock, &MockInitialize::RealInitializeLocked));

  EXPECT_CALL(mock, InitializeLocked).Times(1);
  mock.Initialize();

  constexpr size_t kMaxSampleRateHz = 32000;
  constexpr size_t kMaxNumChannels = 2;
  std::array<int16_t, kMaxNumChannels * kMaxSampleRateHz / 100> frame;
  frame.fill(0);
  StreamConfig config(16000, 1);
  // Call with the default parameters; there should be an init.
  EXPECT_CALL(mock, InitializeLocked).Times(0);
  EXPECT_NOERR(mock.ProcessStream(frame.data(), config, config, frame.data()));
  EXPECT_NOERR(
      mock.ProcessReverseStream(frame.data(), config, config, frame.data()));

  // New sample rate. (Only impacts ProcessStream).
  config = StreamConfig(32000, 1);
  EXPECT_CALL(mock, InitializeLocked).Times(1);
  EXPECT_NOERR(mock.ProcessStream(frame.data(), config, config, frame.data()));

  // New number of channels.
  config = StreamConfig(32000, 2);
  EXPECT_CALL(mock, InitializeLocked).Times(2);
  EXPECT_NOERR(mock.ProcessStream(frame.data(), config, config, frame.data()));
  EXPECT_NOERR(
      mock.ProcessReverseStream(frame.data(), config, config, frame.data()));

  // A new sample rate passed to ProcessReverseStream should cause an init.
  config = StreamConfig(16000, 2);
  EXPECT_CALL(mock, InitializeLocked).Times(1);
  EXPECT_NOERR(
      mock.ProcessReverseStream(frame.data(), config, config, frame.data()));
}

TEST(AudioProcessingImplTest, UpdateCapturePreGainRuntimeSetting) {
  rtc::scoped_refptr<AudioProcessing> apm =
      AudioProcessingBuilderForTesting().Create();
  webrtc::AudioProcessing::Config apm_config;
  apm_config.pre_amplifier.enabled = true;
  apm_config.pre_amplifier.fixed_gain_factor = 1.f;
  apm->ApplyConfig(apm_config);

  constexpr int kSampleRateHz = 48000;
  constexpr int16_t kAudioLevel = 10000;
  constexpr size_t kNumChannels = 2;

  std::array<int16_t, kNumChannels * kSampleRateHz / 100> frame;
  StreamConfig config(kSampleRateHz, kNumChannels);
  frame.fill(kAudioLevel);
  apm->ProcessStream(frame.data(), config, config, frame.data());
  EXPECT_EQ(frame[100], kAudioLevel)
      << "With factor 1, frame shouldn't be modified.";

  constexpr float kGainFactor = 2.f;
  apm->SetRuntimeSetting(
      AudioProcessing::RuntimeSetting::CreateCapturePreGain(kGainFactor));

  // Process for two frames to have time to ramp up gain.
  for (int i = 0; i < 2; ++i) {
    frame.fill(kAudioLevel);
    apm->ProcessStream(frame.data(), config, config, frame.data());
  }
  EXPECT_EQ(frame[100], kGainFactor * kAudioLevel)
      << "Frame should be amplified.";
}

TEST(AudioProcessingImplTest,
     LevelAdjustmentUpdateCapturePreGainRuntimeSetting) {
  rtc::scoped_refptr<AudioProcessing> apm =
      AudioProcessingBuilderForTesting().Create();
  webrtc::AudioProcessing::Config apm_config;
  apm_config.capture_level_adjustment.enabled = true;
  apm_config.capture_level_adjustment.pre_gain_factor = 1.f;
  apm->ApplyConfig(apm_config);

  constexpr int kSampleRateHz = 48000;
  constexpr int16_t kAudioLevel = 10000;
  constexpr size_t kNumChannels = 2;

  std::array<int16_t, kNumChannels * kSampleRateHz / 100> frame;
  StreamConfig config(kSampleRateHz, kNumChannels);
  frame.fill(kAudioLevel);
  apm->ProcessStream(frame.data(), config, config, frame.data());
  EXPECT_EQ(frame[100], kAudioLevel)
      << "With factor 1, frame shouldn't be modified.";

  constexpr float kGainFactor = 2.f;
  apm->SetRuntimeSetting(
      AudioProcessing::RuntimeSetting::CreateCapturePreGain(kGainFactor));

  // Process for two frames to have time to ramp up gain.
  for (int i = 0; i < 2; ++i) {
    frame.fill(kAudioLevel);
    apm->ProcessStream(frame.data(), config, config, frame.data());
  }
  EXPECT_EQ(frame[100], kGainFactor * kAudioLevel)
      << "Frame should be amplified.";
}

TEST(AudioProcessingImplTest,
     LevelAdjustmentUpdateCapturePostGainRuntimeSetting) {
  rtc::scoped_refptr<AudioProcessing> apm =
      AudioProcessingBuilderForTesting().Create();
  webrtc::AudioProcessing::Config apm_config;
  apm_config.capture_level_adjustment.enabled = true;
  apm_config.capture_level_adjustment.post_gain_factor = 1.f;
  apm->ApplyConfig(apm_config);

  constexpr int kSampleRateHz = 48000;
  constexpr int16_t kAudioLevel = 10000;
  constexpr size_t kNumChannels = 2;

  std::array<int16_t, kNumChannels * kSampleRateHz / 100> frame;
  StreamConfig config(kSampleRateHz, kNumChannels);
  frame.fill(kAudioLevel);
  apm->ProcessStream(frame.data(), config, config, frame.data());
  EXPECT_EQ(frame[100], kAudioLevel)
      << "With factor 1, frame shouldn't be modified.";

  constexpr float kGainFactor = 2.f;
  apm->SetRuntimeSetting(
      AudioProcessing::RuntimeSetting::CreateCapturePostGain(kGainFactor));

  // Process for two frames to have time to ramp up gain.
  for (int i = 0; i < 2; ++i) {
    frame.fill(kAudioLevel);
    apm->ProcessStream(frame.data(), config, config, frame.data());
  }
  EXPECT_EQ(frame[100], kGainFactor * kAudioLevel)
      << "Frame should be amplified.";
}

TEST(AudioProcessingImplTest, EchoControllerObservesSetCaptureUsageChange) {
  // Tests that the echo controller observes that the capture usage has been
  // updated.
  auto echo_control_factory = std::make_unique<MockEchoControlFactory>();
  const MockEchoControlFactory* echo_control_factory_ptr =
      echo_control_factory.get();

  rtc::scoped_refptr<AudioProcessing> apm =
      AudioProcessingBuilderForTesting()
          .SetEchoControlFactory(std::move(echo_control_factory))
          .Create();

  constexpr int16_t kAudioLevel = 10000;
  constexpr int kSampleRateHz = 48000;
  constexpr int kNumChannels = 2;
  std::array<int16_t, kNumChannels * kSampleRateHz / 100> frame;
  StreamConfig config(kSampleRateHz, kNumChannels);
  frame.fill(kAudioLevel);

  MockEchoControl* echo_control_mock = echo_control_factory_ptr->GetNext();

  // Ensure that SetCaptureOutputUsage is not called when no runtime settings
  // are passed.
  EXPECT_CALL(*echo_control_mock, SetCaptureOutputUsage(testing::_)).Times(0);
  apm->ProcessStream(frame.data(), config, config, frame.data());

  // Ensure that SetCaptureOutputUsage is called with the right information when
  // a runtime setting is passed.
  EXPECT_CALL(*echo_control_mock,
              SetCaptureOutputUsage(/*capture_output_used=*/false))
      .Times(1);
  EXPECT_TRUE(apm->PostRuntimeSetting(
      AudioProcessing::RuntimeSetting::CreateCaptureOutputUsedSetting(
          /*capture_output_used=*/false)));
  apm->ProcessStream(frame.data(), config, config, frame.data());

  EXPECT_CALL(*echo_control_mock,
              SetCaptureOutputUsage(/*capture_output_used=*/true))
      .Times(1);
  EXPECT_TRUE(apm->PostRuntimeSetting(
      AudioProcessing::RuntimeSetting::CreateCaptureOutputUsedSetting(
          /*capture_output_used=*/true)));
  apm->ProcessStream(frame.data(), config, config, frame.data());

  // The number of positions to place items in the queue is equal to the queue
  // size minus 1.
  constexpr int kNumSlotsInQueue = RuntimeSettingQueueSize();

  // Ensure that SetCaptureOutputUsage is called with the right information when
  // many runtime settings are passed.
  for (int k = 0; k < kNumSlotsInQueue - 1; ++k) {
    EXPECT_TRUE(apm->PostRuntimeSetting(
        AudioProcessing::RuntimeSetting::CreateCaptureOutputUsedSetting(
            /*capture_output_used=*/false)));
  }
  EXPECT_CALL(*echo_control_mock,
              SetCaptureOutputUsage(/*capture_output_used=*/false))
      .Times(kNumSlotsInQueue - 1);
  apm->ProcessStream(frame.data(), config, config, frame.data());

  // Ensure that SetCaptureOutputUsage is properly called with the fallback
  // value when the runtime settings queue becomes full.
  for (int k = 0; k < kNumSlotsInQueue; ++k) {
    EXPECT_TRUE(apm->PostRuntimeSetting(
        AudioProcessing::RuntimeSetting::CreateCaptureOutputUsedSetting(
            /*capture_output_used=*/false)));
  }
  EXPECT_FALSE(apm->PostRuntimeSetting(
      AudioProcessing::RuntimeSetting::CreateCaptureOutputUsedSetting(
          /*capture_output_used=*/false)));
  EXPECT_FALSE(apm->PostRuntimeSetting(
      AudioProcessing::RuntimeSetting::CreateCaptureOutputUsedSetting(
          /*capture_output_used=*/false)));
  EXPECT_CALL(*echo_control_mock,
              SetCaptureOutputUsage(/*capture_output_used=*/false))
      .Times(kNumSlotsInQueue);
  EXPECT_CALL(*echo_control_mock,
              SetCaptureOutputUsage(/*capture_output_used=*/true))
      .Times(1);
  apm->ProcessStream(frame.data(), config, config, frame.data());
}

TEST(AudioProcessingImplTest,
     EchoControllerObservesPreAmplifierEchoPathGainChange) {
  // Tests that the echo controller observes an echo path gain change when the
  // pre-amplifier submodule changes the gain.
  auto echo_control_factory = std::make_unique<MockEchoControlFactory>();
  const auto* echo_control_factory_ptr = echo_control_factory.get();

  rtc::scoped_refptr<AudioProcessing> apm =
      AudioProcessingBuilderForTesting()
          .SetEchoControlFactory(std::move(echo_control_factory))
          .Create();
  // Disable AGC.
  webrtc::AudioProcessing::Config apm_config;
  apm_config.gain_controller1.enabled = false;
  apm_config.gain_controller2.enabled = false;
  apm_config.pre_amplifier.enabled = true;
  apm_config.pre_amplifier.fixed_gain_factor = 1.f;
  apm->ApplyConfig(apm_config);

  constexpr int16_t kAudioLevel = 10000;
  constexpr size_t kSampleRateHz = 48000;
  constexpr size_t kNumChannels = 2;
  std::array<int16_t, kNumChannels * kSampleRateHz / 100> frame;
  StreamConfig config(kSampleRateHz, kNumChannels);
  frame.fill(kAudioLevel);

  MockEchoControl* echo_control_mock = echo_control_factory_ptr->GetNext();

  EXPECT_CALL(*echo_control_mock, AnalyzeCapture(testing::_)).Times(1);
  EXPECT_CALL(*echo_control_mock,
              ProcessCapture(NotNull(), testing::_, /*echo_path_change=*/false))
      .Times(1);
  apm->ProcessStream(frame.data(), config, config, frame.data());

  EXPECT_CALL(*echo_control_mock, AnalyzeCapture(testing::_)).Times(1);
  EXPECT_CALL(*echo_control_mock,
              ProcessCapture(NotNull(), testing::_, /*echo_path_change=*/true))
      .Times(1);
  apm->SetRuntimeSetting(
      AudioProcessing::RuntimeSetting::CreateCapturePreGain(2.f));
  apm->ProcessStream(frame.data(), config, config, frame.data());
}

TEST(AudioProcessingImplTest,
     EchoControllerObservesLevelAdjustmentPreGainEchoPathGainChange) {
  // Tests that the echo controller observes an echo path gain change when the
  // pre-amplifier submodule changes the gain.
  auto echo_control_factory = std::make_unique<MockEchoControlFactory>();
  const auto* echo_control_factory_ptr = echo_control_factory.get();

  rtc::scoped_refptr<AudioProcessing> apm =
      AudioProcessingBuilderForTesting()
          .SetEchoControlFactory(std::move(echo_control_factory))
          .Create();
  // Disable AGC.
  webrtc::AudioProcessing::Config apm_config;
  apm_config.gain_controller1.enabled = false;
  apm_config.gain_controller2.enabled = false;
  apm_config.capture_level_adjustment.enabled = true;
  apm_config.capture_level_adjustment.pre_gain_factor = 1.f;
  apm->ApplyConfig(apm_config);

  constexpr int16_t kAudioLevel = 10000;
  constexpr size_t kSampleRateHz = 48000;
  constexpr size_t kNumChannels = 2;
  std::array<int16_t, kNumChannels * kSampleRateHz / 100> frame;
  StreamConfig config(kSampleRateHz, kNumChannels);
  frame.fill(kAudioLevel);

  MockEchoControl* echo_control_mock = echo_control_factory_ptr->GetNext();

  EXPECT_CALL(*echo_control_mock, AnalyzeCapture(testing::_)).Times(1);
  EXPECT_CALL(*echo_control_mock,
              ProcessCapture(NotNull(), testing::_, /*echo_path_change=*/false))
      .Times(1);
  apm->ProcessStream(frame.data(), config, config, frame.data());

  EXPECT_CALL(*echo_control_mock, AnalyzeCapture(testing::_)).Times(1);
  EXPECT_CALL(*echo_control_mock,
              ProcessCapture(NotNull(), testing::_, /*echo_path_change=*/true))
      .Times(1);
  apm->SetRuntimeSetting(
      AudioProcessing::RuntimeSetting::CreateCapturePreGain(2.f));
  apm->ProcessStream(frame.data(), config, config, frame.data());
}

TEST(AudioProcessingImplTest,
     EchoControllerObservesAnalogAgc1EchoPathGainChange) {
  // Tests that the echo controller observes an echo path gain change when the
  // AGC1 analog adaptive submodule changes the analog gain.
  auto echo_control_factory = std::make_unique<MockEchoControlFactory>();
  const auto* echo_control_factory_ptr = echo_control_factory.get();

  rtc::scoped_refptr<AudioProcessing> apm =
      AudioProcessingBuilderForTesting()
          .SetEchoControlFactory(std::move(echo_control_factory))
          .Create();
  webrtc::AudioProcessing::Config apm_config;
  // Enable AGC1.
  apm_config.gain_controller1.enabled = true;
  apm_config.gain_controller1.analog_gain_controller.enabled = true;
  apm_config.gain_controller2.enabled = false;
  apm_config.pre_amplifier.enabled = false;
  apm->ApplyConfig(apm_config);

  constexpr int16_t kAudioLevel = 1000;
  constexpr size_t kSampleRateHz = 48000;
  constexpr size_t kNumChannels = 2;
  std::array<int16_t, kNumChannels * kSampleRateHz / 100> frame;
  StreamConfig stream_config(kSampleRateHz, kNumChannels);
  frame.fill(kAudioLevel);

  MockEchoControl* echo_control_mock = echo_control_factory_ptr->GetNext();

  constexpr int kInitialStreamAnalogLevel = 123;
  apm->set_stream_analog_level(kInitialStreamAnalogLevel);

  // When the first fame is processed, no echo path gain change must be
  // detected.
  EXPECT_CALL(*echo_control_mock, AnalyzeCapture(testing::_)).Times(1);
  EXPECT_CALL(*echo_control_mock,
              ProcessCapture(NotNull(), testing::_, /*echo_path_change=*/false))
      .Times(1);
  apm->ProcessStream(frame.data(), stream_config, stream_config, frame.data());

  // Simulate the application of the recommended analog level.
  int recommended_analog_level = apm->recommended_stream_analog_level();
  if (recommended_analog_level == kInitialStreamAnalogLevel) {
    // Force an analog gain change if it did not happen.
    recommended_analog_level++;
  }
  apm->set_stream_analog_level(recommended_analog_level);

  // After the first fame and with a stream analog level change, the echo path
  // gain change must be detected.
  EXPECT_CALL(*echo_control_mock, AnalyzeCapture(testing::_)).Times(1);
  EXPECT_CALL(*echo_control_mock,
              ProcessCapture(NotNull(), testing::_, /*echo_path_change=*/true))
      .Times(1);
  apm->ProcessStream(frame.data(), stream_config, stream_config, frame.data());
}

TEST(AudioProcessingImplTest,
     ProcessWithAgc2AndTransientSuppressorVadModeDefault) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-GainController2/Disabled/");
  auto apm = AudioProcessingBuilder()
                 .SetConfig({.gain_controller1{.enabled = false}})
                 .Create();
  ASSERT_EQ(apm->Initialize(), AudioProcessing::kNoError);
  webrtc::AudioProcessing::Config apm_config;
  apm_config.gain_controller1.enabled = false;
  apm_config.gain_controller2.enabled = true;
  apm_config.gain_controller2.adaptive_digital.enabled = true;
  apm_config.transient_suppression.enabled = true;
  apm->ApplyConfig(apm_config);
  constexpr int kSampleRateHz = 48000;
  constexpr int kNumChannels = 1;
  std::array<float, kSampleRateHz / 100> buffer;
  float* channel_pointers[] = {buffer.data()};
  StreamConfig stream_config(/*sample_rate_hz=*/kSampleRateHz,
                             /*num_channels=*/kNumChannels);
  Random random_generator(2341U);
  constexpr int kFramesToProcess = 10;
  for (int i = 0; i < kFramesToProcess; ++i) {
    RandomizeSampleVector(&random_generator, buffer);
    ASSERT_EQ(apm->ProcessStream(channel_pointers, stream_config, stream_config,
                                 channel_pointers),
              kNoErr);
  }
}

TEST(AudioProcessingImplTest,
     ProcessWithAgc2AndTransientSuppressorVadModeRnnVad) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-GainController2/Enabled,switch_to_agc2:true/");
  rtc::scoped_refptr<AudioProcessing> apm = AudioProcessingBuilder().Create();
  ASSERT_EQ(apm->Initialize(), AudioProcessing::kNoError);
  webrtc::AudioProcessing::Config apm_config;
  apm_config.gain_controller1.enabled = false;
  apm_config.gain_controller2.enabled = true;
  apm_config.gain_controller2.adaptive_digital.enabled = true;
  apm_config.transient_suppression.enabled = true;
  apm->ApplyConfig(apm_config);
  constexpr int kSampleRateHz = 48000;
  constexpr int kNumChannels = 1;
  std::array<float, kSampleRateHz / 100> buffer;
  float* channel_pointers[] = {buffer.data()};
  StreamConfig stream_config(/*sample_rate_hz=*/kSampleRateHz,
                             /*num_channels=*/kNumChannels);
  Random random_generator(2341U);
  constexpr int kFramesToProcess = 10;
  for (int i = 0; i < kFramesToProcess; ++i) {
    RandomizeSampleVector(&random_generator, buffer);
    ASSERT_EQ(apm->ProcessStream(channel_pointers, stream_config, stream_config,
                                 channel_pointers),
              kNoErr);
  }
}

TEST(AudioProcessingImplTest, EchoControllerObservesPlayoutVolumeChange) {
  // Tests that the echo controller observes an echo path gain change when a
  // playout volume change is reported.
  auto echo_control_factory = std::make_unique<MockEchoControlFactory>();
  const auto* echo_control_factory_ptr = echo_control_factory.get();

  rtc::scoped_refptr<AudioProcessing> apm =
      AudioProcessingBuilderForTesting()
          .SetEchoControlFactory(std::move(echo_control_factory))
          .Create();
  // Disable AGC.
  webrtc::AudioProcessing::Config apm_config;
  apm_config.gain_controller1.enabled = false;
  apm_config.gain_controller2.enabled = false;
  apm->ApplyConfig(apm_config);

  constexpr int16_t kAudioLevel = 10000;
  constexpr size_t kSampleRateHz = 48000;
  constexpr size_t kNumChannels = 2;
  std::array<int16_t, kNumChannels * kSampleRateHz / 100> frame;
  StreamConfig stream_config(kSampleRateHz, kNumChannels);
  frame.fill(kAudioLevel);

  MockEchoControl* echo_control_mock = echo_control_factory_ptr->GetNext();

  EXPECT_CALL(*echo_control_mock, AnalyzeCapture(testing::_)).Times(1);
  EXPECT_CALL(*echo_control_mock,
              ProcessCapture(NotNull(), testing::_, /*echo_path_change=*/false))
      .Times(1);
  apm->ProcessStream(frame.data(), stream_config, stream_config, frame.data());

  EXPECT_CALL(*echo_control_mock, AnalyzeCapture(testing::_)).Times(1);
  EXPECT_CALL(*echo_control_mock,
              ProcessCapture(NotNull(), testing::_, /*echo_path_change=*/false))
      .Times(1);
  apm->SetRuntimeSetting(
      AudioProcessing::RuntimeSetting::CreatePlayoutVolumeChange(50));
  apm->ProcessStream(frame.data(), stream_config, stream_config, frame.data());

  EXPECT_CALL(*echo_control_mock, AnalyzeCapture(testing::_)).Times(1);
  EXPECT_CALL(*echo_control_mock,
              ProcessCapture(NotNull(), testing::_, /*echo_path_change=*/false))
      .Times(1);
  apm->SetRuntimeSetting(
      AudioProcessing::RuntimeSetting::CreatePlayoutVolumeChange(50));
  apm->ProcessStream(frame.data(), stream_config, stream_config, frame.data());

  EXPECT_CALL(*echo_control_mock, AnalyzeCapture(testing::_)).Times(1);
  EXPECT_CALL(*echo_control_mock,
              ProcessCapture(NotNull(), testing::_, /*echo_path_change=*/true))
      .Times(1);
  apm->SetRuntimeSetting(
      AudioProcessing::RuntimeSetting::CreatePlayoutVolumeChange(100));
  apm->ProcessStream(frame.data(), stream_config, stream_config, frame.data());
}

TEST(AudioProcessingImplTest, RenderPreProcessorBeforeEchoDetector) {
  // Make sure that signal changes caused by a render pre-processing sub-module
  // take place before any echo detector analysis.
  auto test_echo_detector = rtc::make_ref_counted<TestEchoDetector>();
  std::unique_ptr<CustomProcessing> test_render_pre_processor(
      new TestRenderPreProcessor());
  // Create APM injecting the test echo detector and render pre-processor.
  rtc::scoped_refptr<AudioProcessing> apm =
      AudioProcessingBuilderForTesting()
          .SetEchoDetector(test_echo_detector)
          .SetRenderPreProcessing(std::move(test_render_pre_processor))
          .Create();
  webrtc::AudioProcessing::Config apm_config;
  apm_config.pre_amplifier.enabled = true;
  apm->ApplyConfig(apm_config);

  constexpr int16_t kAudioLevel = 1000;
  constexpr int kSampleRateHz = 16000;
  constexpr size_t kNumChannels = 1;
  // Explicitly initialize APM to ensure no render frames are discarded.
  const ProcessingConfig processing_config = {{
      {kSampleRateHz, kNumChannels},
      {kSampleRateHz, kNumChannels},
      {kSampleRateHz, kNumChannels},
      {kSampleRateHz, kNumChannels},
  }};
  apm->Initialize(processing_config);

  std::array<int16_t, kNumChannels * kSampleRateHz / 100> frame;
  StreamConfig stream_config(kSampleRateHz, kNumChannels);

  constexpr float kAudioLevelFloat = static_cast<float>(kAudioLevel);
  constexpr float kExpectedPreprocessedAudioLevel =
      TestRenderPreProcessor::ProcessSample(kAudioLevelFloat);
  ASSERT_NE(kAudioLevelFloat, kExpectedPreprocessedAudioLevel);

  // Analyze a render stream frame.
  frame.fill(kAudioLevel);
  ASSERT_EQ(AudioProcessing::Error::kNoError,
            apm->ProcessReverseStream(frame.data(), stream_config,
                                      stream_config, frame.data()));
  // Trigger a call to in EchoDetector::AnalyzeRenderAudio() via
  // ProcessStream().
  frame.fill(kAudioLevel);
  ASSERT_EQ(AudioProcessing::Error::kNoError,
            apm->ProcessStream(frame.data(), stream_config, stream_config,
                               frame.data()));
  // Regardless of how the call to in EchoDetector::AnalyzeRenderAudio() is
  // triggered, the line below checks that the call has occurred. If not, the
  // APM implementation may have changed and this test might need to be adapted.
  ASSERT_TRUE(test_echo_detector->analyze_render_audio_called());
  // Check that the data read in EchoDetector::AnalyzeRenderAudio() is that
  // produced by the render pre-processor.
  EXPECT_EQ(kExpectedPreprocessedAudioLevel,
            test_echo_detector->last_render_audio_first_sample());
}

// Disabling build-optional submodules and trying to enable them via the APM
// config should be bit-exact with running APM with said submodules disabled.
// This mainly tests that SetCreateOptionalSubmodulesForTesting has an effect.
TEST(ApmWithSubmodulesExcludedTest, BitexactWithDisabledModules) {
  auto apm = rtc::make_ref_counted<AudioProcessingImpl>();
  ASSERT_EQ(apm->Initialize(), AudioProcessing::kNoError);

  ApmSubmoduleCreationOverrides overrides;
  overrides.transient_suppression = true;
  apm->OverrideSubmoduleCreationForTesting(overrides);

  AudioProcessing::Config apm_config = apm->GetConfig();
  apm_config.transient_suppression.enabled = true;
  apm->ApplyConfig(apm_config);

  rtc::scoped_refptr<AudioProcessing> apm_reference =
      AudioProcessingBuilder().Create();
  apm_config = apm_reference->GetConfig();
  apm_config.transient_suppression.enabled = false;
  apm_reference->ApplyConfig(apm_config);

  constexpr int kSampleRateHz = 16000;
  constexpr int kNumChannels = 1;
  std::array<float, kSampleRateHz / 100> buffer;
  std::array<float, kSampleRateHz / 100> buffer_reference;
  float* channel_pointers[] = {buffer.data()};
  float* channel_pointers_reference[] = {buffer_reference.data()};
  StreamConfig stream_config(/*sample_rate_hz=*/kSampleRateHz,
                             /*num_channels=*/kNumChannels);
  Random random_generator(2341U);
  constexpr int kFramesToProcessPerConfiguration = 10;

  for (int i = 0; i < kFramesToProcessPerConfiguration; ++i) {
    RandomizeSampleVector(&random_generator, buffer);
    std::copy(buffer.begin(), buffer.end(), buffer_reference.begin());
    ASSERT_EQ(apm->ProcessStream(channel_pointers, stream_config, stream_config,
                                 channel_pointers),
              kNoErr);
    ASSERT_EQ(
        apm_reference->ProcessStream(channel_pointers_reference, stream_config,
                                     stream_config, channel_pointers_reference),
        kNoErr);
    for (int j = 0; j < kSampleRateHz / 100; ++j) {
      EXPECT_EQ(buffer[j], buffer_reference[j]);
    }
  }
}

// Disable transient suppressor creation and run APM in ways that should trigger
// calls to the transient suppressor API.
TEST(ApmWithSubmodulesExcludedTest, ReinitializeTransientSuppressor) {
  auto apm = rtc::make_ref_counted<AudioProcessingImpl>();
  ASSERT_EQ(apm->Initialize(), kNoErr);

  ApmSubmoduleCreationOverrides overrides;
  overrides.transient_suppression = true;
  apm->OverrideSubmoduleCreationForTesting(overrides);

  AudioProcessing::Config config = apm->GetConfig();
  config.transient_suppression.enabled = true;
  apm->ApplyConfig(config);
  // 960 samples per frame: 10 ms of <= 48 kHz audio with <= 2 channels.
  float buffer[960];
  float* channel_pointers[] = {&buffer[0], &buffer[480]};
  Random random_generator(2341U);
  constexpr int kFramesToProcessPerConfiguration = 3;

  StreamConfig initial_stream_config(/*sample_rate_hz=*/16000,
                                     /*num_channels=*/1);
  for (int i = 0; i < kFramesToProcessPerConfiguration; ++i) {
    RandomizeSampleVector(&random_generator, buffer);
    EXPECT_EQ(apm->ProcessStream(channel_pointers, initial_stream_config,
                                 initial_stream_config, channel_pointers),
              kNoErr);
  }

  StreamConfig stereo_stream_config(/*sample_rate_hz=*/16000,
                                    /*num_channels=*/2);
  for (int i = 0; i < kFramesToProcessPerConfiguration; ++i) {
    RandomizeSampleVector(&random_generator, buffer);
    EXPECT_EQ(apm->ProcessStream(channel_pointers, stereo_stream_config,
                                 stereo_stream_config, channel_pointers),
              kNoErr);
  }

  StreamConfig high_sample_rate_stream_config(/*sample_rate_hz=*/48000,
                                              /*num_channels=*/2);
  for (int i = 0; i < kFramesToProcessPerConfiguration; ++i) {
    RandomizeSampleVector(&random_generator, buffer);
    EXPECT_EQ(
        apm->ProcessStream(channel_pointers, high_sample_rate_stream_config,
                           high_sample_rate_stream_config, channel_pointers),
        kNoErr);
  }
}

// Disable transient suppressor creation and run APM in ways that should trigger
// calls to the transient suppressor API.
TEST(ApmWithSubmodulesExcludedTest, ToggleTransientSuppressor) {
  auto apm = rtc::make_ref_counted<AudioProcessingImpl>();
  ASSERT_EQ(apm->Initialize(), AudioProcessing::kNoError);

  ApmSubmoduleCreationOverrides overrides;
  overrides.transient_suppression = true;
  apm->OverrideSubmoduleCreationForTesting(overrides);

  //  960 samples per frame: 10 ms of <= 48 kHz audio with <= 2 channels.
  float buffer[960];
  float* channel_pointers[] = {&buffer[0], &buffer[480]};
  Random random_generator(2341U);
  constexpr int kFramesToProcessPerConfiguration = 3;
  StreamConfig stream_config(/*sample_rate_hz=*/16000,
                             /*num_channels=*/1);

  AudioProcessing::Config config = apm->GetConfig();
  config.transient_suppression.enabled = true;
  apm->ApplyConfig(config);
  for (int i = 0; i < kFramesToProcessPerConfiguration; ++i) {
    RandomizeSampleVector(&random_generator, buffer);
    EXPECT_EQ(apm->ProcessStream(channel_pointers, stream_config, stream_config,
                                 channel_pointers),
              kNoErr);
  }

  config = apm->GetConfig();
  config.transient_suppression.enabled = false;
  apm->ApplyConfig(config);
  for (int i = 0; i < kFramesToProcessPerConfiguration; ++i) {
    RandomizeSampleVector(&random_generator, buffer);
    EXPECT_EQ(apm->ProcessStream(channel_pointers, stream_config, stream_config,
                                 channel_pointers),
              kNoErr);
  }

  config = apm->GetConfig();
  config.transient_suppression.enabled = true;
  apm->ApplyConfig(config);
  for (int i = 0; i < kFramesToProcessPerConfiguration; ++i) {
    RandomizeSampleVector(&random_generator, buffer);
    EXPECT_EQ(apm->ProcessStream(channel_pointers, stream_config, stream_config,
                                 channel_pointers),
              kNoErr);
  }
}

class StartupInputVolumeParameterizedTest
    : public ::testing::TestWithParam<int> {};

// Tests that, when no input volume controller is used, the startup input volume
// is never modified.
TEST_P(StartupInputVolumeParameterizedTest,
       WithNoInputVolumeControllerStartupVolumeNotModified) {
  webrtc::AudioProcessing::Config config;
  config.gain_controller1.enabled = false;
  config.gain_controller2.enabled = false;
  auto apm = AudioProcessingBuilder().SetConfig(config).Create();

  int startup_volume = GetParam();
  int recommended_volume = ProcessInputVolume(
      *apm, /*num_frames=*/1, /*initial_volume=*/startup_volume);
  EXPECT_EQ(recommended_volume, startup_volume);
}

INSTANTIATE_TEST_SUITE_P(AudioProcessingImplTest,
                         StartupInputVolumeParameterizedTest,
                         ::testing::Values(0, 5, 15, 50, 100));

// Tests that, when no input volume controller is used, the recommended input
// volume always matches the applied one.
TEST(AudioProcessingImplTest,
     WithNoInputVolumeControllerAppliedAndRecommendedVolumesMatch) {
  webrtc::AudioProcessing::Config config;
  config.gain_controller1.enabled = false;
  config.gain_controller2.enabled = false;
  auto apm = AudioProcessingBuilder().SetConfig(config).Create();

  Random rand_gen(42);
  for (int i = 0; i < 32; ++i) {
    SCOPED_TRACE(i);
    int32_t applied_volume = rand_gen.Rand(/*low=*/0, /*high=*/255);
    int recommended_volume =
        ProcessInputVolume(*apm, /*num_frames=*/1, applied_volume);
    EXPECT_EQ(recommended_volume, applied_volume);
  }
}

class ApmInputVolumeControllerParametrizedTest
    : public ::testing::TestWithParam<
          std::tuple<int, int, AudioProcessing::Config>> {
 protected:
  ApmInputVolumeControllerParametrizedTest()
      : sample_rate_hz_(std::get<0>(GetParam())),
        num_channels_(std::get<1>(GetParam())),
        channels_(num_channels_),
        channel_pointers_(num_channels_) {
    const int frame_size = sample_rate_hz_ / 100;
    for (int c = 0; c < num_channels_; ++c) {
      channels_[c].resize(frame_size);
      channel_pointers_[c] = channels_[c].data();
      std::fill(channels_[c].begin(), channels_[c].end(), 0.0f);
    }
  }

  int sample_rate_hz() const { return sample_rate_hz_; }
  int num_channels() const { return num_channels_; }
  AudioProcessing::Config GetConfig() const { return std::get<2>(GetParam()); }

  float* const* channel_pointers() { return channel_pointers_.data(); }

 private:
  const int sample_rate_hz_;
  const int num_channels_;
  std::vector<std::vector<float>> channels_;
  std::vector<float*> channel_pointers_;
};

TEST_P(ApmInputVolumeControllerParametrizedTest,
       EnforceMinInputVolumeAtStartupWithZeroVolume) {
  const StreamConfig stream_config(sample_rate_hz(), num_channels());
  auto apm = AudioProcessingBuilder().SetConfig(GetConfig()).Create();

  apm->set_stream_analog_level(0);
  apm->ProcessStream(channel_pointers(), stream_config, stream_config,
                     channel_pointers());
  EXPECT_GT(apm->recommended_stream_analog_level(), 0);
}

TEST_P(ApmInputVolumeControllerParametrizedTest,
       EnforceMinInputVolumeAtStartupWithNonZeroVolume) {
  const StreamConfig stream_config(sample_rate_hz(), num_channels());
  auto apm = AudioProcessingBuilder().SetConfig(GetConfig()).Create();

  constexpr int kStartupVolume = 3;
  apm->set_stream_analog_level(kStartupVolume);
  apm->ProcessStream(channel_pointers(), stream_config, stream_config,
                     channel_pointers());
  EXPECT_GT(apm->recommended_stream_analog_level(), kStartupVolume);
}

TEST_P(ApmInputVolumeControllerParametrizedTest,
       EnforceMinInputVolumeAfterManualVolumeAdjustment) {
  const auto config = GetConfig();
  if (config.gain_controller1.enabled) {
    // After a downward manual adjustment, AGC1 slowly converges to the minimum
    // input volume.
    GTEST_SKIP() << "Does not apply to AGC1";
  }
  const StreamConfig stream_config(sample_rate_hz(), num_channels());
  auto apm = AudioProcessingBuilder().SetConfig(GetConfig()).Create();

  apm->set_stream_analog_level(20);
  apm->ProcessStream(channel_pointers(), stream_config, stream_config,
                     channel_pointers());
  constexpr int kManuallyAdjustedVolume = 3;
  apm->set_stream_analog_level(kManuallyAdjustedVolume);
  apm->ProcessStream(channel_pointers(), stream_config, stream_config,
                     channel_pointers());
  EXPECT_GT(apm->recommended_stream_analog_level(), kManuallyAdjustedVolume);
}

TEST_P(ApmInputVolumeControllerParametrizedTest,
       DoNotEnforceMinInputVolumeAtStartupWithHighVolume) {
  const StreamConfig stream_config(sample_rate_hz(), num_channels());
  auto apm = AudioProcessingBuilder().SetConfig(GetConfig()).Create();

  constexpr int kStartupVolume = 200;
  apm->set_stream_analog_level(kStartupVolume);
  apm->ProcessStream(channel_pointers(), stream_config, stream_config,
                     channel_pointers());
  EXPECT_EQ(apm->recommended_stream_analog_level(), kStartupVolume);
}

TEST_P(ApmInputVolumeControllerParametrizedTest,
       DoNotEnforceMinInputVolumeAfterManualVolumeAdjustmentToZero) {
  const StreamConfig stream_config(sample_rate_hz(), num_channels());
  auto apm = AudioProcessingBuilder().SetConfig(GetConfig()).Create();

  apm->set_stream_analog_level(100);
  apm->ProcessStream(channel_pointers(), stream_config, stream_config,
                     channel_pointers());
  apm->set_stream_analog_level(0);
  apm->ProcessStream(channel_pointers(), stream_config, stream_config,
                     channel_pointers());
  EXPECT_EQ(apm->recommended_stream_analog_level(), 0);
}

INSTANTIATE_TEST_SUITE_P(
    AudioProcessingImplTest,
    ApmInputVolumeControllerParametrizedTest,
    ::testing::Combine(
        ::testing::Values(8000, 16000, 32000, 48000),  // Sample rates.
        ::testing::Values(1, 2),                       // Number of channels.
        ::testing::Values(
            // Full AGC1.
            AudioProcessing::Config{
                .gain_controller1 = {.enabled = true,
                                     .analog_gain_controller =
                                         {.enabled = true,
                                          .enable_digital_adaptive = true}},
                .gain_controller2 = {.enabled = false}},
            // Hybrid AGC.
            AudioProcessing::Config{
                .gain_controller1 = {.enabled = true,
                                     .analog_gain_controller =
                                         {.enabled = true,
                                          .enable_digital_adaptive = false}},
                .gain_controller2 = {.enabled = true,
                                     .adaptive_digital = {.enabled = true}}})));

// When the input volume is not emulated and no input volume controller is
// active, the recommended volume must always be the applied volume.
TEST(AudioProcessingImplTest,
     RecommendAppliedInputVolumeWithNoAgcWithNoEmulation) {
  auto apm = AudioProcessingBuilder()
                 .SetConfig({.capture_level_adjustment = {.enabled = false},
                             .gain_controller1 = {.enabled = false}})
                 .Create();

  constexpr int kOneFrame = 1;
  EXPECT_EQ(ProcessInputVolume(*apm, kOneFrame, /*initial_volume=*/123), 123);
  EXPECT_EQ(ProcessInputVolume(*apm, kOneFrame, /*initial_volume=*/59), 59);
  EXPECT_EQ(ProcessInputVolume(*apm, kOneFrame, /*initial_volume=*/135), 135);
}

// When the input volume is emulated, the recommended volume must always be the
// applied volume and at any time it must not be that set in the input volume
// emulator.
// TODO(bugs.webrtc.org/14581): Enable when APM fixed to let this test pass.
TEST(AudioProcessingImplTest,
     DISABLED_RecommendAppliedInputVolumeWithNoAgcWithEmulation) {
  auto apm =
      AudioProcessingBuilder()
          .SetConfig({.capture_level_adjustment = {.enabled = true,
                                                   .analog_mic_gain_emulation{
                                                       .enabled = true,
                                                       .initial_level = 255}},
                      .gain_controller1 = {.enabled = false}})
          .Create();

  constexpr int kOneFrame = 1;
  EXPECT_EQ(ProcessInputVolume(*apm, kOneFrame, /*initial_volume=*/123), 123);
  EXPECT_EQ(ProcessInputVolume(*apm, kOneFrame, /*initial_volume=*/59), 59);
  EXPECT_EQ(ProcessInputVolume(*apm, kOneFrame, /*initial_volume=*/135), 135);
}

// Even if there is an enabled input volume controller, when the input volume is
// emulated, the recommended volume is always the applied volume because the
// active controller must only adjust the internally emulated volume and leave
// the externally applied volume unchanged.
// TODO(bugs.webrtc.org/14581): Enable when APM fixed to let this test pass.
TEST(AudioProcessingImplTest,
     DISABLED_RecommendAppliedInputVolumeWithAgcWithEmulation) {
  auto apm =
      AudioProcessingBuilder()
          .SetConfig({.capture_level_adjustment = {.enabled = true,
                                                   .analog_mic_gain_emulation{
                                                       .enabled = true}},
                      .gain_controller1 = {.enabled = true,
                                           .analog_gain_controller{
                                               .enabled = true,
                                           }}})
          .Create();

  constexpr int kOneFrame = 1;
  EXPECT_EQ(ProcessInputVolume(*apm, kOneFrame, /*initial_volume=*/123), 123);
  EXPECT_EQ(ProcessInputVolume(*apm, kOneFrame, /*initial_volume=*/59), 59);
  EXPECT_EQ(ProcessInputVolume(*apm, kOneFrame, /*initial_volume=*/135), 135);
}

TEST(AudioProcessingImplTest,
     Agc2FieldTrialDoNotSwitchToFullAgc2WhenNoAgcIsActive) {
  constexpr AudioProcessing::Config kOriginal{
      .gain_controller1{.enabled = false},
      .gain_controller2{.enabled = false},
  };
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-GainController2/Enabled,switch_to_agc2:true/");

  // Test config application via `AudioProcessing` ctor.
  auto adjusted =
      AudioProcessingBuilder().SetConfig(kOriginal).Create()->GetConfig();
  EXPECT_EQ(adjusted.gain_controller1, kOriginal.gain_controller1);
  EXPECT_EQ(adjusted.gain_controller2, kOriginal.gain_controller2);

  // Test config application via `AudioProcessing::ApplyConfig()`.
  auto apm = AudioProcessingBuilder().Create();
  apm->ApplyConfig(kOriginal);
  adjusted = apm->GetConfig();
  EXPECT_EQ(adjusted.gain_controller1, kOriginal.gain_controller1);
  EXPECT_EQ(adjusted.gain_controller2, kOriginal.gain_controller2);
}

TEST(AudioProcessingImplTest,
     Agc2FieldTrialDoNotSwitchToFullAgc2WithAgc1Agc2InputVolumeControllers) {
  constexpr AudioProcessing::Config kOriginal{
      .gain_controller1{.enabled = true,
                        .analog_gain_controller{.enabled = true}},
      .gain_controller2{.enabled = true,
                        .input_volume_controller{.enabled = true}},
  };
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-GainController2/Enabled,switch_to_agc2:true/");

  // Test config application via `AudioProcessing` ctor.
  auto adjusted =
      AudioProcessingBuilder().SetConfig(kOriginal).Create()->GetConfig();
  EXPECT_EQ(adjusted.gain_controller1, kOriginal.gain_controller1);
  EXPECT_EQ(adjusted.gain_controller2, kOriginal.gain_controller2);

  // Test config application via `AudioProcessing::ApplyConfig()`.
  auto apm = AudioProcessingBuilder().Create();
  apm->ApplyConfig(kOriginal);
  adjusted = apm->GetConfig();
  EXPECT_EQ(adjusted.gain_controller1, kOriginal.gain_controller1);
  EXPECT_EQ(adjusted.gain_controller2, kOriginal.gain_controller2);
}

class Agc2FieldTrialParametrizedTest
    : public ::testing::TestWithParam<AudioProcessing::Config> {};

TEST_P(Agc2FieldTrialParametrizedTest, DoNotChangeConfigIfDisabled) {
  const AudioProcessing::Config original = GetParam();
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-GainController2/Disabled/");

  // Test config application via `AudioProcessing` ctor.
  auto adjusted =
      AudioProcessingBuilder().SetConfig(original).Create()->GetConfig();
  EXPECT_EQ(adjusted.gain_controller1, original.gain_controller1);
  EXPECT_EQ(adjusted.gain_controller2, original.gain_controller2);

  // Test config application via `AudioProcessing::ApplyConfig()`.
  auto apm = AudioProcessingBuilder().Create();
  apm->ApplyConfig(original);
  adjusted = apm->GetConfig();
  EXPECT_EQ(adjusted.gain_controller1, original.gain_controller1);
  EXPECT_EQ(adjusted.gain_controller2, original.gain_controller2);
}

TEST_P(Agc2FieldTrialParametrizedTest, DoNotChangeConfigIfNoOverride) {
  const AudioProcessing::Config original = GetParam();
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-GainController2/Enabled,"
      "switch_to_agc2:false,"
      "disallow_transient_suppressor_usage:false/");

  // Test config application via `AudioProcessing` ctor.
  auto adjusted =
      AudioProcessingBuilder().SetConfig(original).Create()->GetConfig();
  EXPECT_EQ(adjusted.gain_controller1, original.gain_controller1);
  EXPECT_EQ(adjusted.gain_controller2, original.gain_controller2);

  // Test config application via `AudioProcessing::ApplyConfig()`.
  auto apm = AudioProcessingBuilder().Create();
  apm->ApplyConfig(original);
  adjusted = apm->GetConfig();
  EXPECT_EQ(adjusted.gain_controller1, original.gain_controller1);
  EXPECT_EQ(adjusted.gain_controller2, original.gain_controller2);
}

TEST_P(Agc2FieldTrialParametrizedTest, DoNotSwitchToFullAgc2) {
  const AudioProcessing::Config original = GetParam();
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-GainController2/Enabled,switch_to_agc2:false/");

  // Test config application via `AudioProcessing` ctor.
  auto adjusted =
      AudioProcessingBuilder().SetConfig(original).Create()->GetConfig();
  EXPECT_EQ(adjusted.gain_controller1, original.gain_controller1);
  EXPECT_EQ(adjusted.gain_controller2, original.gain_controller2);

  // Test config application via `AudioProcessing::ApplyConfig()`.
  auto apm = AudioProcessingBuilder().Create();
  apm->ApplyConfig(original);
  adjusted = apm->GetConfig();
  EXPECT_EQ(adjusted.gain_controller1, original.gain_controller1);
  EXPECT_EQ(adjusted.gain_controller2, original.gain_controller2);
}

TEST_P(Agc2FieldTrialParametrizedTest, SwitchToFullAgc2) {
  const AudioProcessing::Config original = GetParam();
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-GainController2/Enabled,switch_to_agc2:true/");

  // Test config application via `AudioProcessing` ctor.
  auto adjusted =
      AudioProcessingBuilder().SetConfig(original).Create()->GetConfig();
  EXPECT_FALSE(adjusted.gain_controller1.enabled);
  EXPECT_TRUE(adjusted.gain_controller2.enabled);
  EXPECT_TRUE(adjusted.gain_controller2.input_volume_controller.enabled);
  EXPECT_TRUE(adjusted.gain_controller2.adaptive_digital.enabled);

  // Test config application via `AudioProcessing::ApplyConfig()`.
  auto apm = AudioProcessingBuilder().Create();
  apm->ApplyConfig(original);
  adjusted = apm->GetConfig();
  EXPECT_FALSE(adjusted.gain_controller1.enabled);
  EXPECT_TRUE(adjusted.gain_controller2.enabled);
  EXPECT_TRUE(adjusted.gain_controller2.input_volume_controller.enabled);
  EXPECT_TRUE(adjusted.gain_controller2.adaptive_digital.enabled);
}

TEST_P(Agc2FieldTrialParametrizedTest,
       SwitchToFullAgc2AndOverrideInputVolumeControllerParameters) {
  const AudioProcessing::Config original = GetParam();
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-GainController2/Enabled,switch_to_agc2:true,"
      "min_input_volume:123,"
      "clipped_level_min:20,"
      "clipped_level_step:30,"
      "clipped_ratio_threshold:0.4,"
      "clipped_wait_frames:50,"
      "enable_clipping_predictor:true,"
      "target_range_max_dbfs:-6,"
      "target_range_min_dbfs:-70,"
      "update_input_volume_wait_frames:80,"
      "speech_probability_threshold:0.9,"
      "speech_ratio_threshold:1.0/");

  // Test config application via `AudioProcessing` ctor.
  auto adjusted =
      AudioProcessingBuilder().SetConfig(original).Create()->GetConfig();
  EXPECT_FALSE(adjusted.gain_controller1.enabled);
  EXPECT_TRUE(adjusted.gain_controller2.enabled);
  EXPECT_TRUE(adjusted.gain_controller2.input_volume_controller.enabled);
  EXPECT_TRUE(adjusted.gain_controller2.adaptive_digital.enabled);

  // Test config application via `AudioProcessing::ApplyConfig()`.
  auto apm = AudioProcessingBuilder().Create();
  apm->ApplyConfig(original);
  adjusted = apm->GetConfig();
  EXPECT_FALSE(adjusted.gain_controller1.enabled);
  EXPECT_TRUE(adjusted.gain_controller2.enabled);
  EXPECT_TRUE(adjusted.gain_controller2.input_volume_controller.enabled);
  EXPECT_TRUE(adjusted.gain_controller2.adaptive_digital.enabled);
}

TEST_P(Agc2FieldTrialParametrizedTest,
       SwitchToFullAgc2AndOverrideAdaptiveDigitalControllerParameters) {
  const AudioProcessing::Config original = GetParam();
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-GainController2/Enabled,switch_to_agc2:true,"
      "headroom_db:10,"
      "max_gain_db:20,"
      "initial_gain_db:7,"
      "max_gain_change_db_per_second:5,"
      "max_output_noise_level_dbfs:-40/");

  // Test config application via `AudioProcessing` ctor.
  auto adjusted =
      AudioProcessingBuilder().SetConfig(original).Create()->GetConfig();
  EXPECT_FALSE(adjusted.gain_controller1.enabled);
  EXPECT_TRUE(adjusted.gain_controller2.enabled);
  EXPECT_TRUE(adjusted.gain_controller2.input_volume_controller.enabled);
  EXPECT_TRUE(adjusted.gain_controller2.adaptive_digital.enabled);
  ASSERT_NE(adjusted.gain_controller2.adaptive_digital,
            original.gain_controller2.adaptive_digital);
  EXPECT_EQ(adjusted.gain_controller2.adaptive_digital.headroom_db, 10);
  EXPECT_EQ(adjusted.gain_controller2.adaptive_digital.max_gain_db, 20);
  EXPECT_EQ(adjusted.gain_controller2.adaptive_digital.initial_gain_db, 7);
  EXPECT_EQ(
      adjusted.gain_controller2.adaptive_digital.max_gain_change_db_per_second,
      5);
  EXPECT_EQ(
      adjusted.gain_controller2.adaptive_digital.max_output_noise_level_dbfs,
      -40);

  // Test config application via `AudioProcessing::ApplyConfig()`.
  auto apm = AudioProcessingBuilder().Create();
  apm->ApplyConfig(original);
  adjusted = apm->GetConfig();
  EXPECT_FALSE(adjusted.gain_controller1.enabled);
  EXPECT_TRUE(adjusted.gain_controller2.enabled);
  EXPECT_TRUE(adjusted.gain_controller2.input_volume_controller.enabled);
  EXPECT_TRUE(adjusted.gain_controller2.adaptive_digital.enabled);
  ASSERT_NE(adjusted.gain_controller2.adaptive_digital,
            original.gain_controller2.adaptive_digital);
  EXPECT_EQ(adjusted.gain_controller2.adaptive_digital.headroom_db, 10);
  EXPECT_EQ(adjusted.gain_controller2.adaptive_digital.max_gain_db, 20);
  EXPECT_EQ(adjusted.gain_controller2.adaptive_digital.initial_gain_db, 7);
  EXPECT_EQ(
      adjusted.gain_controller2.adaptive_digital.max_gain_change_db_per_second,
      5);
  EXPECT_EQ(
      adjusted.gain_controller2.adaptive_digital.max_output_noise_level_dbfs,
      -40);
}

TEST_P(Agc2FieldTrialParametrizedTest, ProcessSucceedsWithTs) {
  AudioProcessing::Config config = GetParam();
  if (!config.transient_suppression.enabled) {
    GTEST_SKIP() << "TS is disabled, skip.";
  }

  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-GainController2/Disabled/");
  auto apm = AudioProcessingBuilder().SetConfig(config).Create();

  constexpr int kSampleRateHz = 48000;
  constexpr int kNumChannels = 1;
  std::array<float, kSampleRateHz / 100> buffer;
  float* channel_pointers[] = {buffer.data()};
  StreamConfig stream_config(kSampleRateHz, kNumChannels);
  Random random_generator(2341U);
  constexpr int kFramesToProcess = 10;
  int volume = 100;
  for (int i = 0; i < kFramesToProcess; ++i) {
    SCOPED_TRACE(i);
    RandomizeSampleVector(&random_generator, buffer);
    apm->set_stream_analog_level(volume);
    ASSERT_EQ(apm->ProcessStream(channel_pointers, stream_config, stream_config,
                                 channel_pointers),
              kNoErr);
    volume = apm->recommended_stream_analog_level();
  }
}

TEST_P(Agc2FieldTrialParametrizedTest, ProcessSucceedsWithoutTs) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-GainController2/Enabled,"
      "switch_to_agc2:false,"
      "disallow_transient_suppressor_usage:true/");
  auto apm = AudioProcessingBuilder().SetConfig(GetParam()).Create();

  constexpr int kSampleRateHz = 48000;
  constexpr int kNumChannels = 1;
  std::array<float, kSampleRateHz / 100> buffer;
  float* channel_pointers[] = {buffer.data()};
  StreamConfig stream_config(kSampleRateHz, kNumChannels);
  Random random_generator(2341U);
  constexpr int kFramesToProcess = 10;
  int volume = 100;
  for (int i = 0; i < kFramesToProcess; ++i) {
    SCOPED_TRACE(i);
    RandomizeSampleVector(&random_generator, buffer);
    apm->set_stream_analog_level(volume);
    ASSERT_EQ(apm->ProcessStream(channel_pointers, stream_config, stream_config,
                                 channel_pointers),
              kNoErr);
    volume = apm->recommended_stream_analog_level();
  }
}

TEST_P(Agc2FieldTrialParametrizedTest,
       ProcessSucceedsWhenSwitchToFullAgc2WithTs) {
  AudioProcessing::Config config = GetParam();
  if (!config.transient_suppression.enabled) {
    GTEST_SKIP() << "TS is disabled, skip.";
  }

  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-GainController2/Enabled,"
      "switch_to_agc2:true,"
      "disallow_transient_suppressor_usage:false/");
  auto apm = AudioProcessingBuilder().SetConfig(config).Create();

  constexpr int kSampleRateHz = 48000;
  constexpr int kNumChannels = 1;
  std::array<float, kSampleRateHz / 100> buffer;
  float* channel_pointers[] = {buffer.data()};
  StreamConfig stream_config(kSampleRateHz, kNumChannels);
  Random random_generator(2341U);
  constexpr int kFramesToProcess = 10;
  int volume = 100;
  for (int i = 0; i < kFramesToProcess; ++i) {
    SCOPED_TRACE(i);
    RandomizeSampleVector(&random_generator, buffer);
    apm->set_stream_analog_level(volume);
    ASSERT_EQ(apm->ProcessStream(channel_pointers, stream_config, stream_config,
                                 channel_pointers),
              kNoErr);
    volume = apm->recommended_stream_analog_level();
  }
}

TEST_P(Agc2FieldTrialParametrizedTest,
       ProcessSucceedsWhenSwitchToFullAgc2WithoutTs) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-GainController2/Enabled,"
      "switch_to_agc2:true,"
      "disallow_transient_suppressor_usage:true/");
  auto apm = AudioProcessingBuilder().SetConfig(GetParam()).Create();

  constexpr int kSampleRateHz = 48000;
  constexpr int kNumChannels = 1;
  std::array<float, kSampleRateHz / 100> buffer;
  float* channel_pointers[] = {buffer.data()};
  StreamConfig stream_config(kSampleRateHz, kNumChannels);
  Random random_generator(2341U);
  constexpr int kFramesToProcess = 10;
  int volume = 100;
  for (int i = 0; i < kFramesToProcess; ++i) {
    SCOPED_TRACE(i);
    RandomizeSampleVector(&random_generator, buffer);
    apm->set_stream_analog_level(volume);
    ASSERT_EQ(apm->ProcessStream(channel_pointers, stream_config, stream_config,
                                 channel_pointers),
              kNoErr);
    volume = apm->recommended_stream_analog_level();
  }
}

INSTANTIATE_TEST_SUITE_P(
    AudioProcessingImplTest,
    Agc2FieldTrialParametrizedTest,
    ::testing::Values(
        // Full AGC1, TS disabled.
        AudioProcessing::Config{
            .transient_suppression = {.enabled = false},
            .gain_controller1 =
                {.enabled = true,
                 .analog_gain_controller = {.enabled = true,
                                            .enable_digital_adaptive = true}},
            .gain_controller2 = {.enabled = false}},
        // Full AGC1, TS enabled.
        AudioProcessing::Config{
            .transient_suppression = {.enabled = true},
            .gain_controller1 =
                {.enabled = true,
                 .analog_gain_controller = {.enabled = true,
                                            .enable_digital_adaptive = true}},
            .gain_controller2 = {.enabled = false}},
        // Hybrid AGC, TS disabled.
        AudioProcessing::Config{
            .transient_suppression = {.enabled = false},
            .gain_controller1 =
                {.enabled = true,
                 .analog_gain_controller = {.enabled = true,
                                            .enable_digital_adaptive = false}},
            .gain_controller2 = {.enabled = true,
                                 .adaptive_digital = {.enabled = true}}},
        // Hybrid AGC, TS enabled.
        AudioProcessing::Config{
            .transient_suppression = {.enabled = true},
            .gain_controller1 =
                {.enabled = true,
                 .analog_gain_controller = {.enabled = true,
                                            .enable_digital_adaptive = false}},
            .gain_controller2 = {.enabled = true,
                                 .adaptive_digital = {.enabled = true}}}));

TEST(AudioProcessingImplTest, CanDisableTransientSuppressor) {
  constexpr AudioProcessing::Config kOriginal = {
      .transient_suppression = {.enabled = false}};

  // Test config application via `AudioProcessing` ctor.
  auto adjusted =
      AudioProcessingBuilder().SetConfig(kOriginal).Create()->GetConfig();
  EXPECT_FALSE(adjusted.transient_suppression.enabled);

  // Test config application via `AudioProcessing::ApplyConfig()`.
  auto apm = AudioProcessingBuilder().Create();
  apm->ApplyConfig(kOriginal);
  adjusted = apm->GetConfig();
  EXPECT_FALSE(apm->GetConfig().transient_suppression.enabled);
}

TEST(AudioProcessingImplTest, CanEnableTs) {
  constexpr AudioProcessing::Config kOriginal = {
      .transient_suppression = {.enabled = true}};

  // Test config application via `AudioProcessing` ctor.
  auto adjusted =
      AudioProcessingBuilder().SetConfig(kOriginal).Create()->GetConfig();
  EXPECT_TRUE(adjusted.transient_suppression.enabled);

  // Test config application via `AudioProcessing::ApplyConfig()`.
  auto apm = AudioProcessingBuilder().Create();
  apm->ApplyConfig(kOriginal);
  adjusted = apm->GetConfig();
  EXPECT_TRUE(adjusted.transient_suppression.enabled);
}

TEST(AudioProcessingImplTest, CanDisableTsWithAgc2FieldTrialDisabled) {
  constexpr AudioProcessing::Config kOriginal = {
      .transient_suppression = {.enabled = false}};
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-GainController2/Disabled/");

  // Test config application via `AudioProcessing` ctor.
  auto adjusted =
      AudioProcessingBuilder().SetConfig(kOriginal).Create()->GetConfig();
  EXPECT_FALSE(adjusted.transient_suppression.enabled);

  // Test config application via `AudioProcessing::ApplyConfig()`.
  auto apm = AudioProcessingBuilder().Create();
  apm->ApplyConfig(kOriginal);
  adjusted = apm->GetConfig();
  EXPECT_FALSE(apm->GetConfig().transient_suppression.enabled);
}

TEST(AudioProcessingImplTest, CanEnableTsWithAgc2FieldTrialDisabled) {
  constexpr AudioProcessing::Config kOriginal = {
      .transient_suppression = {.enabled = true}};
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-GainController2/Disabled/");

  // Test config application via `AudioProcessing` ctor.
  auto adjusted =
      AudioProcessingBuilder().SetConfig(kOriginal).Create()->GetConfig();
  EXPECT_TRUE(adjusted.transient_suppression.enabled);

  // Test config application via `AudioProcessing::ApplyConfig()`.
  auto apm = AudioProcessingBuilder().Create();
  apm->ApplyConfig(kOriginal);
  adjusted = apm->GetConfig();
  EXPECT_TRUE(adjusted.transient_suppression.enabled);
}

TEST(AudioProcessingImplTest,
     CanDisableTsWithAgc2FieldTrialEnabledAndUsageAllowed) {
  constexpr AudioProcessing::Config kOriginal = {
      .transient_suppression = {.enabled = false}};
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-GainController2/Enabled,"
      "disallow_transient_suppressor_usage:false/");

  // Test config application via `AudioProcessing` ctor.
  auto adjusted =
      AudioProcessingBuilder().SetConfig(kOriginal).Create()->GetConfig();
  EXPECT_FALSE(adjusted.transient_suppression.enabled);

  // Test config application via `AudioProcessing::ApplyConfig()`.
  auto apm = AudioProcessingBuilder().Create();
  apm->ApplyConfig(kOriginal);
  adjusted = apm->GetConfig();
  EXPECT_FALSE(adjusted.transient_suppression.enabled);
}

TEST(AudioProcessingImplTest,
     CanEnableTsWithAgc2FieldTrialEnabledAndUsageAllowed) {
  constexpr AudioProcessing::Config kOriginal = {
      .transient_suppression = {.enabled = true}};
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-GainController2/Enabled,"
      "disallow_transient_suppressor_usage:false/");

  // Test config application via `AudioProcessing` ctor.
  auto adjusted =
      AudioProcessingBuilder().SetConfig(kOriginal).Create()->GetConfig();
  EXPECT_TRUE(adjusted.transient_suppression.enabled);

  // Test config application via `AudioProcessing::ApplyConfig()`.
  auto apm = AudioProcessingBuilder().Create();
  apm->ApplyConfig(kOriginal);
  adjusted = apm->GetConfig();
  EXPECT_TRUE(adjusted.transient_suppression.enabled);
}

TEST(AudioProcessingImplTest,
     CannotEnableTsWithAgc2FieldTrialEnabledAndUsageDisallowed) {
  constexpr AudioProcessing::Config kOriginal = {
      .transient_suppression = {.enabled = true}};
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-GainController2/Enabled,"
      "disallow_transient_suppressor_usage:true/");

  // Test config application via `AudioProcessing` ctor.
  auto adjusted =
      AudioProcessingBuilder().SetConfig(kOriginal).Create()->GetConfig();
  EXPECT_FALSE(adjusted.transient_suppression.enabled);

  // Test config application via `AudioProcessing::ApplyConfig()`.
  auto apm = AudioProcessingBuilder().Create();
  apm->ApplyConfig(kOriginal);
  adjusted = apm->GetConfig();
  EXPECT_FALSE(apm->GetConfig().transient_suppression.enabled);
}

}  // namespace webrtc
