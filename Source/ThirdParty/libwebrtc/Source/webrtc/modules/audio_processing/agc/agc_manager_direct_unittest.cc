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

#include "modules/audio_processing/agc/gain_control.h"
#include "modules/audio_processing/agc/mock_agc.h"
#include "modules/audio_processing/include/mock_audio_processing.h"
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

using ClippingPredictorConfig = AudioProcessing::Config::GainController1::
    AnalogGainController::ClippingPredictor;

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

std::unique_ptr<AgcManagerDirect> CreateAgcManagerDirect(
    int startup_min_level,
    int clipped_level_step,
    float clipped_ratio_threshold,
    int clipped_wait_frames) {
  return std::make_unique<AgcManagerDirect>(
      /*num_capture_channels=*/1, startup_min_level, kClippedMin,
      /*disable_digital_adaptive=*/true, kSampleRateHz, clipped_level_step,
      clipped_ratio_threshold, clipped_wait_frames, ClippingPredictorConfig());
}

std::unique_ptr<AgcManagerDirect> CreateAgcManagerDirect(
    int startup_min_level,
    int clipped_level_step,
    float clipped_ratio_threshold,
    int clipped_wait_frames,
    const ClippingPredictorConfig& clipping_cfg) {
  return std::make_unique<AgcManagerDirect>(
      /*num_capture_channels=*/1, startup_min_level, kClippedMin,
      /*disable_digital_adaptive=*/true, kSampleRateHz, clipped_level_step,
      clipped_ratio_threshold, clipped_wait_frames, clipping_cfg);
}

void CallPreProcessAudioBuffer(int num_calls,
                               float peak_ratio,
                               AgcManagerDirect& manager) {
  RTC_DCHECK_GE(1.f, peak_ratio);
  AudioBuffer audio_buffer(kSampleRateHz, 1, kSampleRateHz, 1, kSampleRateHz,
                           1);
  const int num_channels = audio_buffer.num_channels();
  const int num_frames = audio_buffer.num_frames();
  for (int ch = 0; ch < num_channels; ++ch) {
    for (int i = 0; i < num_frames; i += 2) {
      audio_buffer.channels()[ch][i] = peak_ratio * 32767.f;
      audio_buffer.channels()[ch][i + 1] = 0.0f;
    }
  }
  for (int n = 0; n < num_calls / 2; ++n) {
    manager.AnalyzePreProcess(&audio_buffer);
  }
  for (int ch = 0; ch < num_channels; ++ch) {
    for (int i = 0; i < num_frames; ++i) {
      audio_buffer.channels()[ch][i] = peak_ratio * 32767.f;
    }
  }
  for (int n = 0; n < num_calls - num_calls / 2; ++n) {
    manager.AnalyzePreProcess(&audio_buffer);
  }
}

}  // namespace

class AgcManagerDirectTest : public ::testing::Test {
 protected:
  AgcManagerDirectTest()
      : agc_(new MockAgc),
        manager_(agc_,
                 kInitialVolume,
                 kClippedMin,
                 kSampleRateHz,
                 kClippedLevelStep,
                 kClippedRatioThreshold,
                 kClippedWaitFrames,
                 ClippingPredictorConfig()),
        audio(kNumChannels),
        audio_data(kNumChannels * kSamplesPerChannel, 0.f) {
    ExpectInitialize();
    manager_.Initialize();
    manager_.SetupDigitalGainControl(&gctrl_);
    for (size_t ch = 0; ch < kNumChannels; ++ch) {
      audio[ch] = &audio_data[ch * kSamplesPerChannel];
    }
  }

  void FirstProcess() {
    EXPECT_CALL(*agc_, Reset()).Times(AtLeast(1));
    EXPECT_CALL(*agc_, GetRmsErrorDb(_)).WillOnce(Return(false));
    CallProcess(1);
  }

  void SetVolumeAndProcess(int volume) {
    manager_.set_stream_analog_level(volume);
    FirstProcess();
  }

  void ExpectCheckVolumeAndReset(int volume) {
    manager_.set_stream_analog_level(volume);
    EXPECT_CALL(*agc_, Reset());
  }

  void ExpectInitialize() {
    EXPECT_CALL(gctrl_, set_mode(GainControl::kFixedDigital));
    EXPECT_CALL(gctrl_, set_target_level_dbfs(2));
    EXPECT_CALL(gctrl_, set_compression_gain_db(7));
    EXPECT_CALL(gctrl_, enable_limiter(true));
  }

  void CallProcess(int num_calls) {
    for (int i = 0; i < num_calls; ++i) {
      EXPECT_CALL(*agc_, Process(_, _, _)).WillOnce(Return());
      manager_.Process(nullptr);
      absl::optional<int> new_digital_gain =
          manager_.GetDigitalComressionGain();
      if (new_digital_gain) {
        gctrl_.set_compression_gain_db(*new_digital_gain);
      }
    }
  }

  void CallPreProc(int num_calls, float clipped_ratio) {
    RTC_DCHECK_GE(1.f, clipped_ratio);
    const int num_clipped = kSamplesPerChannel * clipped_ratio;
    std::fill(audio_data.begin(), audio_data.end(), 0.f);
    for (size_t ch = 0; ch < kNumChannels; ++ch) {
      for (int k = 0; k < num_clipped; ++k) {
        audio[ch][k] = 32767.f;
      }
    }
    for (int i = 0; i < num_calls; ++i) {
      manager_.AnalyzePreProcess(audio.data(), kSamplesPerChannel);
    }
  }

  void CallPreProcForChangingAudio(int num_calls, float peak_ratio) {
    RTC_DCHECK_GE(1.f, peak_ratio);
    std::fill(audio_data.begin(), audio_data.end(), 0.f);
    for (size_t ch = 0; ch < kNumChannels; ++ch) {
      for (size_t k = 0; k < kSamplesPerChannel; k += 2) {
        audio[ch][k] = peak_ratio * 32767.f;
      }
    }
    for (int i = 0; i < num_calls / 2; ++i) {
      manager_.AnalyzePreProcess(audio.data(), kSamplesPerChannel);
    }
    for (size_t ch = 0; ch < kNumChannels; ++ch) {
      for (size_t k = 0; k < kSamplesPerChannel; ++k) {
        audio[ch][k] = peak_ratio * 32767.f;
      }
    }
    for (int i = 0; i < num_calls - num_calls / 2; ++i) {
      manager_.AnalyzePreProcess(audio.data(), kSamplesPerChannel);
    }
  }

  MockAgc* agc_;
  MockGainControl gctrl_;
  AgcManagerDirect manager_;
  std::vector<float*> audio;
  std::vector<float> audio_data;
};

TEST_F(AgcManagerDirectTest, StartupMinVolumeConfigurationIsRespected) {
  FirstProcess();
  EXPECT_EQ(kInitialVolume, manager_.stream_analog_level());
}

TEST_F(AgcManagerDirectTest, MicVolumeResponseToRmsError) {
  FirstProcess();

  // Compressor default; no residual error.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(5), Return(true)));
  CallProcess(1);

  // Inside the compressor's window; no change of volume.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)));
  CallProcess(1);

  // Above the compressor's window; volume should be increased.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)));
  CallProcess(1);
  EXPECT_EQ(130, manager_.stream_analog_level());

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(20), Return(true)));
  CallProcess(1);
  EXPECT_EQ(168, manager_.stream_analog_level());

  // Inside the compressor's window; no change of volume.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(5), Return(true)));
  CallProcess(1);
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)));
  CallProcess(1);

  // Below the compressor's window; volume should be decreased.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  CallProcess(1);
  EXPECT_EQ(167, manager_.stream_analog_level());

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  CallProcess(1);
  EXPECT_EQ(163, manager_.stream_analog_level());

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-9), Return(true)));
  CallProcess(1);
  EXPECT_EQ(129, manager_.stream_analog_level());
}

TEST_F(AgcManagerDirectTest, MicVolumeIsLimited) {
  FirstProcess();

  // Maximum upwards change is limited.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(30), Return(true)));
  CallProcess(1);
  EXPECT_EQ(183, manager_.stream_analog_level());

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(30), Return(true)));
  CallProcess(1);
  EXPECT_EQ(243, manager_.stream_analog_level());

  // Won't go higher than the maximum.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(30), Return(true)));
  CallProcess(1);
  EXPECT_EQ(255, manager_.stream_analog_level());

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  CallProcess(1);
  EXPECT_EQ(254, manager_.stream_analog_level());

  // Maximum downwards change is limited.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  CallProcess(1);
  EXPECT_EQ(194, manager_.stream_analog_level());

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  CallProcess(1);
  EXPECT_EQ(137, manager_.stream_analog_level());

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  CallProcess(1);
  EXPECT_EQ(88, manager_.stream_analog_level());

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  CallProcess(1);
  EXPECT_EQ(54, manager_.stream_analog_level());

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  CallProcess(1);
  EXPECT_EQ(33, manager_.stream_analog_level());

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  CallProcess(1);
  EXPECT_EQ(18, manager_.stream_analog_level());

  // Won't go lower than the minimum.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  CallProcess(1);
  EXPECT_EQ(12, manager_.stream_analog_level());
}

TEST_F(AgcManagerDirectTest, CompressorStepsTowardsTarget) {
  FirstProcess();

  // Compressor default; no call to set_compression_gain_db.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(5), Return(true)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(gctrl_, set_compression_gain_db(_)).Times(0);
  CallProcess(20);

  // Moves slowly upwards.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(9), Return(true)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(gctrl_, set_compression_gain_db(_)).Times(0);
  CallProcess(19);
  EXPECT_CALL(gctrl_, set_compression_gain_db(8)).WillOnce(Return(0));
  CallProcess(1);

  EXPECT_CALL(gctrl_, set_compression_gain_db(_)).Times(0);
  CallProcess(19);
  EXPECT_CALL(gctrl_, set_compression_gain_db(9)).WillOnce(Return(0));
  CallProcess(1);

  EXPECT_CALL(gctrl_, set_compression_gain_db(_)).Times(0);
  CallProcess(20);

  // Moves slowly downward, then reverses before reaching the original target.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(5), Return(true)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(gctrl_, set_compression_gain_db(_)).Times(0);
  CallProcess(19);
  EXPECT_CALL(gctrl_, set_compression_gain_db(8)).WillOnce(Return(0));
  CallProcess(1);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(9), Return(true)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(gctrl_, set_compression_gain_db(_)).Times(0);
  CallProcess(19);
  EXPECT_CALL(gctrl_, set_compression_gain_db(9)).WillOnce(Return(0));
  CallProcess(1);

  EXPECT_CALL(gctrl_, set_compression_gain_db(_)).Times(0);
  CallProcess(20);
}

TEST_F(AgcManagerDirectTest, CompressorErrorIsDeemphasized) {
  FirstProcess();

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)))
      .WillRepeatedly(Return(false));
  CallProcess(19);
  EXPECT_CALL(gctrl_, set_compression_gain_db(8)).WillOnce(Return(0));
  CallProcess(20);
  EXPECT_CALL(gctrl_, set_compression_gain_db(9)).WillOnce(Return(0));
  CallProcess(1);
  EXPECT_CALL(gctrl_, set_compression_gain_db(_)).Times(0);
  CallProcess(20);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)))
      .WillRepeatedly(Return(false));
  CallProcess(19);
  EXPECT_CALL(gctrl_, set_compression_gain_db(8)).WillOnce(Return(0));
  CallProcess(20);
  EXPECT_CALL(gctrl_, set_compression_gain_db(7)).WillOnce(Return(0));
  CallProcess(20);
  EXPECT_CALL(gctrl_, set_compression_gain_db(6)).WillOnce(Return(0));
  CallProcess(1);
  EXPECT_CALL(gctrl_, set_compression_gain_db(_)).Times(0);
  CallProcess(20);
}

TEST_F(AgcManagerDirectTest, CompressorReachesMaximum) {
  FirstProcess();

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)))
      .WillRepeatedly(Return(false));
  CallProcess(19);
  EXPECT_CALL(gctrl_, set_compression_gain_db(8)).WillOnce(Return(0));
  CallProcess(20);
  EXPECT_CALL(gctrl_, set_compression_gain_db(9)).WillOnce(Return(0));
  CallProcess(20);
  EXPECT_CALL(gctrl_, set_compression_gain_db(10)).WillOnce(Return(0));
  CallProcess(20);
  EXPECT_CALL(gctrl_, set_compression_gain_db(11)).WillOnce(Return(0));
  CallProcess(20);
  EXPECT_CALL(gctrl_, set_compression_gain_db(12)).WillOnce(Return(0));
  CallProcess(1);
}

TEST_F(AgcManagerDirectTest, CompressorReachesMinimum) {
  FirstProcess();

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)))
      .WillRepeatedly(Return(false));
  CallProcess(19);
  EXPECT_CALL(gctrl_, set_compression_gain_db(6)).WillOnce(Return(0));
  CallProcess(20);
  EXPECT_CALL(gctrl_, set_compression_gain_db(5)).WillOnce(Return(0));
  CallProcess(20);
  EXPECT_CALL(gctrl_, set_compression_gain_db(4)).WillOnce(Return(0));
  CallProcess(20);
  EXPECT_CALL(gctrl_, set_compression_gain_db(3)).WillOnce(Return(0));
  CallProcess(20);
  EXPECT_CALL(gctrl_, set_compression_gain_db(2)).WillOnce(Return(0));
  CallProcess(1);
}

TEST_F(AgcManagerDirectTest, NoActionWhileMuted) {
  manager_.HandleCaptureOutputUsedChange(false);
  manager_.Process(nullptr);
  absl::optional<int> new_digital_gain = manager_.GetDigitalComressionGain();
  if (new_digital_gain) {
    gctrl_.set_compression_gain_db(*new_digital_gain);
  }
}

TEST_F(AgcManagerDirectTest, UnmutingChecksVolumeWithoutRaising) {
  FirstProcess();

  manager_.HandleCaptureOutputUsedChange(false);
  manager_.HandleCaptureOutputUsedChange(true);
  ExpectCheckVolumeAndReset(127);
  // SetMicVolume should not be called.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_)).WillOnce(Return(false));
  CallProcess(1);
  EXPECT_EQ(127, manager_.stream_analog_level());
}

TEST_F(AgcManagerDirectTest, UnmutingRaisesTooLowVolume) {
  FirstProcess();

  manager_.HandleCaptureOutputUsedChange(false);
  manager_.HandleCaptureOutputUsedChange(true);
  ExpectCheckVolumeAndReset(11);
  EXPECT_CALL(*agc_, GetRmsErrorDb(_)).WillOnce(Return(false));
  CallProcess(1);
  EXPECT_EQ(12, manager_.stream_analog_level());
}

TEST_F(AgcManagerDirectTest, ManualLevelChangeResultsInNoSetMicCall) {
  FirstProcess();

  // Change outside of compressor's range, which would normally trigger a call
  // to SetMicVolume.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)));

  // When the analog volume changes, the gain controller is reset.
  EXPECT_CALL(*agc_, Reset()).Times(AtLeast(1));

  // GetMicVolume returns a value outside of the quantization slack, indicating
  // a manual volume change.
  ASSERT_NE(manager_.stream_analog_level(), 154);
  manager_.set_stream_analog_level(154);
  CallProcess(1);
  EXPECT_EQ(154, manager_.stream_analog_level());

  // Do the same thing, except downwards now.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  manager_.set_stream_analog_level(100);
  EXPECT_CALL(*agc_, Reset()).Times(AtLeast(1));
  CallProcess(1);
  EXPECT_EQ(100, manager_.stream_analog_level());

  // And finally verify the AGC continues working without a manual change.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  CallProcess(1);
  EXPECT_EQ(99, manager_.stream_analog_level());
}

TEST_F(AgcManagerDirectTest, RecoveryAfterManualLevelChangeFromMax) {
  FirstProcess();

  // Force the mic up to max volume. Takes a few steps due to the residual
  // gain limitation.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(30), Return(true)));
  CallProcess(1);
  EXPECT_EQ(183, manager_.stream_analog_level());
  CallProcess(1);
  EXPECT_EQ(243, manager_.stream_analog_level());
  CallProcess(1);
  EXPECT_EQ(255, manager_.stream_analog_level());

  // Manual change does not result in SetMicVolume call.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  manager_.set_stream_analog_level(50);
  EXPECT_CALL(*agc_, Reset()).Times(AtLeast(1));
  CallProcess(1);
  EXPECT_EQ(50, manager_.stream_analog_level());

  // Continues working as usual afterwards.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(20), Return(true)));
  CallProcess(1);
  EXPECT_EQ(69, manager_.stream_analog_level());
}

TEST_F(AgcManagerDirectTest, RecoveryAfterManualLevelChangeBelowMin) {
  FirstProcess();

  // Manual change below min.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  // Don't set to zero, which will cause AGC to take no action.
  manager_.set_stream_analog_level(1);
  EXPECT_CALL(*agc_, Reset()).Times(AtLeast(1));
  CallProcess(1);
  EXPECT_EQ(1, manager_.stream_analog_level());

  // Continues working as usual afterwards.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)));
  CallProcess(1);
  EXPECT_EQ(2, manager_.stream_analog_level());

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(30), Return(true)));
  CallProcess(1);
  EXPECT_EQ(11, manager_.stream_analog_level());

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(20), Return(true)));
  CallProcess(1);
  EXPECT_EQ(18, manager_.stream_analog_level());
}

TEST_F(AgcManagerDirectTest, NoClippingHasNoImpact) {
  FirstProcess();

  CallPreProc(100, 0);
  EXPECT_EQ(128, manager_.stream_analog_level());
}

TEST_F(AgcManagerDirectTest, ClippingUnderThresholdHasNoImpact) {
  FirstProcess();

  CallPreProc(1, 0.099);
  EXPECT_EQ(128, manager_.stream_analog_level());
}

TEST_F(AgcManagerDirectTest, ClippingLowersVolume) {
  SetVolumeAndProcess(255);

  EXPECT_CALL(*agc_, Reset()).Times(AtLeast(1));
  CallPreProc(1, 0.2);
  EXPECT_EQ(240, manager_.stream_analog_level());
}

TEST_F(AgcManagerDirectTest, WaitingPeriodBetweenClippingChecks) {
  SetVolumeAndProcess(255);

  EXPECT_CALL(*agc_, Reset()).Times(AtLeast(1));
  CallPreProc(1, kAboveClippedThreshold);
  EXPECT_EQ(240, manager_.stream_analog_level());

  EXPECT_CALL(*agc_, Reset()).Times(0);
  CallPreProc(300, kAboveClippedThreshold);
  EXPECT_EQ(240, manager_.stream_analog_level());

  EXPECT_CALL(*agc_, Reset()).Times(AtLeast(1));
  CallPreProc(1, kAboveClippedThreshold);
  EXPECT_EQ(225, manager_.stream_analog_level());
}

TEST_F(AgcManagerDirectTest, ClippingLoweringIsLimited) {
  SetVolumeAndProcess(180);

  EXPECT_CALL(*agc_, Reset()).Times(AtLeast(1));
  CallPreProc(1, kAboveClippedThreshold);
  EXPECT_EQ(kClippedMin, manager_.stream_analog_level());

  EXPECT_CALL(*agc_, Reset()).Times(0);
  CallPreProc(1000, kAboveClippedThreshold);
  EXPECT_EQ(kClippedMin, manager_.stream_analog_level());
}

TEST_F(AgcManagerDirectTest, ClippingMaxIsRespectedWhenEqualToLevel) {
  SetVolumeAndProcess(255);

  EXPECT_CALL(*agc_, Reset()).Times(AtLeast(1));
  CallPreProc(1, kAboveClippedThreshold);
  EXPECT_EQ(240, manager_.stream_analog_level());

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(30), Return(true)));
  CallProcess(10);
  EXPECT_EQ(240, manager_.stream_analog_level());
}

TEST_F(AgcManagerDirectTest, ClippingMaxIsRespectedWhenHigherThanLevel) {
  SetVolumeAndProcess(200);

  EXPECT_CALL(*agc_, Reset()).Times(AtLeast(1));
  CallPreProc(1, kAboveClippedThreshold);
  EXPECT_EQ(185, manager_.stream_analog_level());

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(40), Return(true)));
  CallProcess(1);
  EXPECT_EQ(240, manager_.stream_analog_level());
  CallProcess(10);
  EXPECT_EQ(240, manager_.stream_analog_level());
}

TEST_F(AgcManagerDirectTest, MaxCompressionIsIncreasedAfterClipping) {
  SetVolumeAndProcess(210);

  EXPECT_CALL(*agc_, Reset()).Times(AtLeast(1));
  CallPreProc(1, kAboveClippedThreshold);
  EXPECT_EQ(195, manager_.stream_analog_level());

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)))
      .WillRepeatedly(Return(false));
  CallProcess(19);
  EXPECT_CALL(gctrl_, set_compression_gain_db(8)).WillOnce(Return(0));
  CallProcess(20);
  EXPECT_CALL(gctrl_, set_compression_gain_db(9)).WillOnce(Return(0));
  CallProcess(20);
  EXPECT_CALL(gctrl_, set_compression_gain_db(10)).WillOnce(Return(0));
  CallProcess(20);
  EXPECT_CALL(gctrl_, set_compression_gain_db(11)).WillOnce(Return(0));
  CallProcess(20);
  EXPECT_CALL(gctrl_, set_compression_gain_db(12)).WillOnce(Return(0));
  CallProcess(20);
  EXPECT_CALL(gctrl_, set_compression_gain_db(13)).WillOnce(Return(0));
  CallProcess(1);

  // Continue clipping until we hit the maximum surplus compression.
  CallPreProc(300, kAboveClippedThreshold);
  EXPECT_CALL(*agc_, Reset()).Times(AtLeast(1));
  CallPreProc(1, kAboveClippedThreshold);
  EXPECT_EQ(180, manager_.stream_analog_level());

  CallPreProc(300, kAboveClippedThreshold);
  EXPECT_CALL(*agc_, Reset()).Times(AtLeast(1));
  CallPreProc(1, kAboveClippedThreshold);
  EXPECT_EQ(kClippedMin, manager_.stream_analog_level());

  // Current level is now at the minimum, but the maximum allowed level still
  // has more to decrease.
  CallPreProc(300, kAboveClippedThreshold);
  CallPreProc(1, kAboveClippedThreshold);

  CallPreProc(300, kAboveClippedThreshold);
  CallPreProc(1, kAboveClippedThreshold);

  CallPreProc(300, kAboveClippedThreshold);
  CallPreProc(1, kAboveClippedThreshold);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(16), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(16), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(16), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(16), Return(true)))
      .WillRepeatedly(Return(false));
  CallProcess(19);
  EXPECT_CALL(gctrl_, set_compression_gain_db(14)).WillOnce(Return(0));
  CallProcess(20);
  EXPECT_CALL(gctrl_, set_compression_gain_db(15)).WillOnce(Return(0));
  CallProcess(20);
  EXPECT_CALL(gctrl_, set_compression_gain_db(16)).WillOnce(Return(0));
  CallProcess(20);
  EXPECT_CALL(gctrl_, set_compression_gain_db(17)).WillOnce(Return(0));
  CallProcess(20);
  EXPECT_CALL(gctrl_, set_compression_gain_db(18)).WillOnce(Return(0));
  CallProcess(1);
}

TEST_F(AgcManagerDirectTest, UserCanRaiseVolumeAfterClipping) {
  SetVolumeAndProcess(225);

  EXPECT_CALL(*agc_, Reset()).Times(AtLeast(1));
  CallPreProc(1, kAboveClippedThreshold);
  EXPECT_EQ(210, manager_.stream_analog_level());

  // High enough error to trigger a volume check.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(14), Return(true)));
  // User changed the volume.
  manager_.set_stream_analog_level(250);
  EXPECT_CALL(*agc_, Reset()).Times(AtLeast(1));
  CallProcess(1);
  EXPECT_EQ(250, manager_.stream_analog_level());

  // Move down...
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-10), Return(true)));
  CallProcess(1);
  EXPECT_EQ(210, manager_.stream_analog_level());
  // And back up to the new max established by the user.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(40), Return(true)));
  CallProcess(1);
  EXPECT_EQ(250, manager_.stream_analog_level());
  // Will not move above new maximum.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(30), Return(true)));
  CallProcess(1);
  EXPECT_EQ(250, manager_.stream_analog_level());
}

TEST_F(AgcManagerDirectTest, ClippingDoesNotPullLowVolumeBackUp) {
  SetVolumeAndProcess(80);

  EXPECT_CALL(*agc_, Reset()).Times(0);
  int initial_volume = manager_.stream_analog_level();
  CallPreProc(1, kAboveClippedThreshold);
  EXPECT_EQ(initial_volume, manager_.stream_analog_level());
}

TEST_F(AgcManagerDirectTest, TakesNoActionOnZeroMicVolume) {
  FirstProcess();

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(30), Return(true)));
  manager_.set_stream_analog_level(0);
  CallProcess(10);
  EXPECT_EQ(0, manager_.stream_analog_level());
}

TEST_F(AgcManagerDirectTest, ClippingDetectionLowersVolume) {
  SetVolumeAndProcess(255);
  EXPECT_EQ(255, manager_.stream_analog_level());
  CallPreProcForChangingAudio(/*num_calls=*/100, /*peak_ratio=*/0.99f);
  EXPECT_EQ(255, manager_.stream_analog_level());
  CallPreProcForChangingAudio(/*num_calls=*/100, /*peak_ratio=*/1.0f);
  EXPECT_EQ(240, manager_.stream_analog_level());
}

TEST_F(AgcManagerDirectTest, DisabledClippingPredictorDoesNotLowerVolume) {
  SetVolumeAndProcess(255);
  EXPECT_FALSE(manager_.clipping_predictor_enabled());
  EXPECT_EQ(255, manager_.stream_analog_level());
  CallPreProcForChangingAudio(/*num_calls=*/100, /*peak_ratio=*/0.99f);
  EXPECT_EQ(255, manager_.stream_analog_level());
  CallPreProcForChangingAudio(/*num_calls=*/100, /*peak_ratio=*/0.99f);
  EXPECT_EQ(255, manager_.stream_analog_level());
}

TEST(AgcManagerDirectStandaloneTest, DisableDigitalDisablesDigital) {
  auto agc = std::unique_ptr<Agc>(new ::testing::NiceMock<MockAgc>());
  MockGainControl gctrl;
  EXPECT_CALL(gctrl, set_mode(GainControl::kFixedDigital));
  EXPECT_CALL(gctrl, set_target_level_dbfs(0));
  EXPECT_CALL(gctrl, set_compression_gain_db(0));
  EXPECT_CALL(gctrl, enable_limiter(false));

  std::unique_ptr<AgcManagerDirect> manager =
      CreateAgcManagerDirect(kInitialVolume, kClippedLevelStep,
                             kClippedRatioThreshold, kClippedWaitFrames);
  manager->Initialize();
  manager->SetupDigitalGainControl(&gctrl);
}

TEST(AgcManagerDirectStandaloneTest, AgcMinMicLevelExperiment) {
  std::unique_ptr<AgcManagerDirect> manager =
      CreateAgcManagerDirect(kInitialVolume, kClippedLevelStep,
                             kClippedRatioThreshold, kClippedWaitFrames);
  EXPECT_EQ(manager->channel_agcs_[0]->min_mic_level(), kMinMicLevel);
  EXPECT_EQ(manager->channel_agcs_[0]->startup_min_level(), kInitialVolume);
}

TEST(AgcManagerDirectStandaloneTest, AgcMinMicLevelExperimentDisabled) {
  test::ScopedFieldTrials field_trial(
      "WebRTC-Audio-AgcMinMicLevelExperiment/Disabled/");
  std::unique_ptr<AgcManagerDirect> manager =
      CreateAgcManagerDirect(kInitialVolume, kClippedLevelStep,
                             kClippedRatioThreshold, kClippedWaitFrames);
  EXPECT_EQ(manager->channel_agcs_[0]->min_mic_level(), kMinMicLevel);
  EXPECT_EQ(manager->channel_agcs_[0]->startup_min_level(), kInitialVolume);
}

// Checks that a field-trial parameter outside of the valid range [0,255] is
// ignored.
TEST(AgcManagerDirectStandaloneTest, AgcMinMicLevelExperimentOutOfRangeAbove) {
  test::ScopedFieldTrials field_trial(
      "WebRTC-Audio-AgcMinMicLevelExperiment/Enabled-256/");
  std::unique_ptr<AgcManagerDirect> manager =
      CreateAgcManagerDirect(kInitialVolume, kClippedLevelStep,
                             kClippedRatioThreshold, kClippedWaitFrames);
  EXPECT_EQ(manager->channel_agcs_[0]->min_mic_level(), kMinMicLevel);
  EXPECT_EQ(manager->channel_agcs_[0]->startup_min_level(), kInitialVolume);
}

// Checks that a field-trial parameter outside of the valid range [0,255] is
// ignored.
TEST(AgcManagerDirectStandaloneTest, AgcMinMicLevelExperimentOutOfRangeBelow) {
  test::ScopedFieldTrials field_trial(
      "WebRTC-Audio-AgcMinMicLevelExperiment/Enabled--1/");
  std::unique_ptr<AgcManagerDirect> manager =
      CreateAgcManagerDirect(kInitialVolume, kClippedLevelStep,
                             kClippedRatioThreshold, kClippedWaitFrames);
  EXPECT_EQ(manager->channel_agcs_[0]->min_mic_level(), kMinMicLevel);
  EXPECT_EQ(manager->channel_agcs_[0]->startup_min_level(), kInitialVolume);
}

// Verifies that a valid experiment changes the minimum microphone level. The
// start volume is larger than the min level and should therefore not be
// changed.
TEST(AgcManagerDirectStandaloneTest, AgcMinMicLevelExperimentEnabled50) {
  test::ScopedFieldTrials field_trial(
      "WebRTC-Audio-AgcMinMicLevelExperiment/Enabled-50/");
  std::unique_ptr<AgcManagerDirect> manager =
      CreateAgcManagerDirect(kInitialVolume, kClippedLevelStep,
                             kClippedRatioThreshold, kClippedWaitFrames);
  EXPECT_EQ(manager->channel_agcs_[0]->min_mic_level(), 50);
  EXPECT_EQ(manager->channel_agcs_[0]->startup_min_level(), kInitialVolume);
}

// Uses experiment to reduce the default minimum microphone level, start at a
// lower level and ensure that the startup level is increased to the min level
// set by the experiment.
TEST(AgcManagerDirectStandaloneTest,
     AgcMinMicLevelExperimentEnabledAboveStartupLevel) {
  test::ScopedFieldTrials field_trial(
      "WebRTC-Audio-AgcMinMicLevelExperiment/Enabled-50/");
  std::unique_ptr<AgcManagerDirect> manager =
      CreateAgcManagerDirect(/*startup_min_level=*/30, kClippedLevelStep,
                             kClippedRatioThreshold, kClippedWaitFrames);
  EXPECT_EQ(manager->channel_agcs_[0]->min_mic_level(), 50);
  EXPECT_EQ(manager->channel_agcs_[0]->startup_min_level(), 50);
}

// TODO(bugs.webrtc.org/12774): Test the bahavior of `clipped_level_step`.
// TODO(bugs.webrtc.org/12774): Test the bahavior of `clipped_ratio_threshold`.
// TODO(bugs.webrtc.org/12774): Test the bahavior of `clipped_wait_frames`.
// Verifies that configurable clipping parameters are initialized as intended.
TEST(AgcManagerDirectStandaloneTest, ClippingParametersVerified) {
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

TEST(AgcManagerDirectStandaloneTest,
     DisableClippingPredictorDisablesClippingPredictor) {
  ClippingPredictorConfig default_config;
  EXPECT_FALSE(default_config.enabled);
  std::unique_ptr<AgcManagerDirect> manager = CreateAgcManagerDirect(
      kInitialVolume, kClippedLevelStep, kClippedRatioThreshold,
      kClippedWaitFrames, default_config);
  manager->Initialize();
  EXPECT_FALSE(manager->clipping_predictor_enabled());
  EXPECT_FALSE(manager->use_clipping_predictor_step());
}

TEST(AgcManagerDirectStandaloneTest, ClippingPredictorDisabledByDefault) {
  constexpr ClippingPredictorConfig kDefaultConfig;
  EXPECT_FALSE(kDefaultConfig.enabled);
}

TEST(AgcManagerDirectStandaloneTest,
     EnableClippingPredictorEnablesClippingPredictor) {
  // TODO(bugs.webrtc.org/12874): Use designated initializers one fixed.
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

TEST(AgcManagerDirectStandaloneTest,
     DisableClippingPredictorDoesNotLowerVolume) {
  // TODO(bugs.webrtc.org/12874): Use designated initializers one fixed.
  constexpr ClippingPredictorConfig kConfig{/*enabled=*/false};
  AgcManagerDirect manager(new ::testing::NiceMock<MockAgc>(), kInitialVolume,
                           kClippedMin, kSampleRateHz, kClippedLevelStep,
                           kClippedRatioThreshold, kClippedWaitFrames, kConfig);
  manager.Initialize();
  manager.set_stream_analog_level(/*level=*/255);
  EXPECT_FALSE(manager.clipping_predictor_enabled());
  EXPECT_FALSE(manager.use_clipping_predictor_step());
  EXPECT_EQ(manager.stream_analog_level(), 255);
  manager.Process(nullptr);
  CallPreProcessAudioBuffer(/*num_calls=*/10, /*peak_ratio=*/0.99f, manager);
  EXPECT_EQ(manager.stream_analog_level(), 255);
  CallPreProcessAudioBuffer(/*num_calls=*/300, /*peak_ratio=*/0.99f, manager);
  EXPECT_EQ(manager.stream_analog_level(), 255);
  CallPreProcessAudioBuffer(/*num_calls=*/10, /*peak_ratio=*/0.99f, manager);
  EXPECT_EQ(manager.stream_analog_level(), 255);
}

TEST(AgcManagerDirectStandaloneTest,
     UsedClippingPredictionsProduceLowerAnalogLevels) {
  // TODO(bugs.webrtc.org/12874): Use designated initializers once fixed.
  ClippingPredictorConfig config_with_prediction;
  config_with_prediction.enabled = true;
  config_with_prediction.use_predicted_step = true;
  AgcManagerDirect manager_with_prediction(
      new ::testing::NiceMock<MockAgc>(), kInitialVolume, kClippedMin,
      kSampleRateHz, kClippedLevelStep, kClippedRatioThreshold,
      kClippedWaitFrames, config_with_prediction);
  ClippingPredictorConfig config_without_prediction;
  config_without_prediction.enabled = false;
  AgcManagerDirect manager_without_prediction(
      new ::testing::NiceMock<MockAgc>(), kInitialVolume, kClippedMin,
      kSampleRateHz, kClippedLevelStep, kClippedRatioThreshold,
      kClippedWaitFrames, config_without_prediction);
  manager_with_prediction.Initialize();
  manager_without_prediction.Initialize();
  constexpr int kInitialLevel = 255;
  constexpr float kClippingPeakRatio = 1.0f;
  constexpr float kCloseToClippingPeakRatio = 0.99f;
  constexpr float kZeroPeakRatio = 0.0f;
  manager_with_prediction.set_stream_analog_level(kInitialLevel);
  manager_without_prediction.set_stream_analog_level(kInitialLevel);
  manager_with_prediction.Process(nullptr);
  manager_without_prediction.Process(nullptr);
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

TEST(AgcManagerDirectStandaloneTest,
     UnusedClippingPredictionsProduceEqualAnalogLevels) {
  // TODO(bugs.webrtc.org/12874): Use designated initializers once fixed.
  ClippingPredictorConfig config_with_prediction;
  config_with_prediction.enabled = true;
  config_with_prediction.use_predicted_step = false;
  AgcManagerDirect manager_with_prediction(
      new ::testing::NiceMock<MockAgc>(), kInitialVolume, kClippedMin,
      kSampleRateHz, kClippedLevelStep, kClippedRatioThreshold,
      kClippedWaitFrames, config_with_prediction);
  ClippingPredictorConfig config_without_prediction;
  config_without_prediction.enabled = false;
  AgcManagerDirect manager_without_prediction(
      new ::testing::NiceMock<MockAgc>(), kInitialVolume, kClippedMin,
      kSampleRateHz, kClippedLevelStep, kClippedRatioThreshold,
      kClippedWaitFrames, config_without_prediction);
  constexpr int kInitialLevel = 255;
  constexpr float kClippingPeakRatio = 1.0f;
  constexpr float kCloseToClippingPeakRatio = 0.99f;
  constexpr float kZeroPeakRatio = 0.0f;
  manager_with_prediction.Initialize();
  manager_without_prediction.Initialize();
  manager_with_prediction.set_stream_analog_level(kInitialLevel);
  manager_without_prediction.set_stream_analog_level(kInitialLevel);
  manager_with_prediction.Process(nullptr);
  manager_without_prediction.Process(nullptr);
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
