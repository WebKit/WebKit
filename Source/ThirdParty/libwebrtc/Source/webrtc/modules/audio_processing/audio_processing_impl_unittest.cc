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

#include <array>
#include <memory>

#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/optionally_built_submodule_creators.h"
#include "modules/audio_processing/test/audio_processing_builder_for_testing.h"
#include "modules/audio_processing/test/echo_canceller_test_tools.h"
#include "modules/audio_processing/test/echo_control_mock.h"
#include "modules/audio_processing/test/test_utils.h"
#include "rtc_base/checks.h"
#include "rtc_base/random.h"
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
  MOCK_METHOD(rtc::RefCountReleaseStatus, Release, (), (const, override));
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
  // TODO(peah): Investigate why this causes 2 inits.
  config = StreamConfig(32000, 2);
  EXPECT_CALL(mock, InitializeLocked).Times(2);
  EXPECT_NOERR(mock.ProcessStream(frame.data(), config, config, frame.data()));
  // ProcessStream sets num_channels_ == num_output_channels.
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
  apm_config.gain_controller1.mode =
      AudioProcessing::Config::GainController1::kAdaptiveAnalog;
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

  const int initial_analog_gain = apm->recommended_stream_analog_level();
  EXPECT_CALL(*echo_control_mock, AnalyzeCapture(testing::_)).Times(1);
  EXPECT_CALL(*echo_control_mock, ProcessCapture(NotNull(), testing::_, false))
      .Times(1);
  apm->ProcessStream(frame.data(), stream_config, stream_config, frame.data());

  // Force an analog gain change if it did not happen.
  if (initial_analog_gain == apm->recommended_stream_analog_level()) {
    apm->set_stream_analog_level(initial_analog_gain + 1);
  }

  EXPECT_CALL(*echo_control_mock, AnalyzeCapture(testing::_)).Times(1);
  EXPECT_CALL(*echo_control_mock, ProcessCapture(NotNull(), testing::_, true))
      .Times(1);
  apm->ProcessStream(frame.data(), stream_config, stream_config, frame.data());
}

TEST(AudioProcessingImplTest,
     EchoControllerObservesNoDigitalAgc2EchoPathGainChange) {
  // Tests that the echo controller doesn't observe an echo path gain change
  // when the AGC2 digital submodule changes the digital gain without analog
  // gain changes.
  auto echo_control_factory = std::make_unique<MockEchoControlFactory>();
  const auto* echo_control_factory_ptr = echo_control_factory.get();
  rtc::scoped_refptr<AudioProcessing> apm =
      AudioProcessingBuilderForTesting()
          .SetEchoControlFactory(std::move(echo_control_factory))
          .Create();
  webrtc::AudioProcessing::Config apm_config;
  // Disable AGC1 analog.
  apm_config.gain_controller1.enabled = false;
  // Enable AGC2 digital.
  apm_config.gain_controller2.enabled = true;
  apm_config.gain_controller2.adaptive_digital.enabled = true;
  apm->ApplyConfig(apm_config);

  constexpr int16_t kAudioLevel = 1000;
  constexpr size_t kSampleRateHz = 48000;
  constexpr size_t kNumChannels = 2;
  std::array<int16_t, kNumChannels * kSampleRateHz / 100> frame;
  StreamConfig stream_config(kSampleRateHz, kNumChannels);
  frame.fill(kAudioLevel);

  MockEchoControl* echo_control_mock = echo_control_factory_ptr->GetNext();

  EXPECT_CALL(*echo_control_mock, AnalyzeCapture(testing::_)).Times(1);
  EXPECT_CALL(*echo_control_mock, ProcessCapture(NotNull(), testing::_,
                                                 /*echo_path_change=*/false))
      .Times(1);
  apm->ProcessStream(frame.data(), stream_config, stream_config, frame.data());

  EXPECT_CALL(*echo_control_mock, AnalyzeCapture(testing::_)).Times(1);
  EXPECT_CALL(*echo_control_mock, ProcessCapture(NotNull(), testing::_,
                                                 /*echo_path_change=*/false))
      .Times(1);
  apm->ProcessStream(frame.data(), stream_config, stream_config, frame.data());
}

TEST(AudioProcessingImplTest, ProcessWithAgc2InjectedSpeechProbability) {
  // Tests that a stream is successfully processed for the field trial
  // `WebRTC-Audio-TransientSuppressorVadMode/Enabled-RnnVad/` using
  // injected speech probability in AGC2 digital.
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-TransientSuppressorVadMode/Enabled-RnnVad/");
  rtc::scoped_refptr<AudioProcessing> apm = AudioProcessingBuilder().Create();
  ASSERT_EQ(apm->Initialize(), AudioProcessing::kNoError);
  webrtc::AudioProcessing::Config apm_config;
  // Disable AGC1 analog.
  apm_config.gain_controller1.enabled = false;
  // Enable AGC2 digital.
  apm_config.gain_controller2.enabled = true;
  apm_config.gain_controller2.adaptive_digital.enabled = true;
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
}  // namespace webrtc
