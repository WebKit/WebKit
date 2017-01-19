/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/test/auto_test/fixtures/after_streaming_fixture.h"

namespace {

void ExpectVolumeNear(int expected, int actual) {
  // The hardware volume may be more coarsely quantized than [0, 255], so
  // it is not always reasonable to expect to get exactly what we set. This
  // allows for some error.
  const int kMaxVolumeError = 10;
  EXPECT_NEAR(expected, actual, kMaxVolumeError);
  EXPECT_GE(actual, 0);
  EXPECT_LE(actual, 255);
}

}  // namespace

class VolumeTest : public AfterStreamingFixture {
 public:
  void SetAndVerifyMicVolume(unsigned int volume) {
    bool success = voe_volume_control_->SetMicVolume(volume) == 0;
#if !defined(WEBRTC_LINUX)
    EXPECT_TRUE(success);
#endif
    if (!success) {
      TEST_LOG("Failed to set microphone volume to %u.\n", volume);
      return;
    }

    unsigned int test_volume = 1000;
    success = voe_volume_control_->GetMicVolume(test_volume) == 0;
#if !defined(WEBRTC_LINUX)
    EXPECT_TRUE(success);
#endif
    if (success) {
      EXPECT_EQ(volume, test_volume);
    } else {
      TEST_LOG("Failed to get the microphone volume.");
      EXPECT_EQ(1000u, test_volume);
    }
  }

  void SetAndVerifyInputMute(bool enable) {
    bool success = voe_volume_control_->SetInputMute(channel_, enable) == 0;
#if !defined(WEBRTC_LINUX)
    EXPECT_TRUE(success);
#endif
    if (!success) {
      TEST_LOG("Failed to %smute input.\n", enable ? "" : "un");
      return;
    }

    bool is_muted = !enable;
    success = voe_volume_control_->GetInputMute(channel_, is_muted) == 0;
#if !defined(WEBRTC_LINUX)
    EXPECT_TRUE(success);
#endif
    if (success) {
      EXPECT_EQ(enable, is_muted);
    } else {
      TEST_LOG("Failed to mute the input.");
      EXPECT_NE(enable, is_muted);
    }
  }
};

// Some tests are flaky on Linux (Pulse Audio), which boils down to some system
// values not being acquired in time. In Pulse Audio we make one retry if
// needed, but if we fail then, a -1 is returned propagating up through VoE.
// To avoid possible bugs slipping through on other platforms we make adequate
// changes on Linux only.
TEST_F(VolumeTest, VerifyCorrectErrorReturns) {
  // All tests run on correct initialization which eliminates one possible error
  // return. In addition, we assume the audio_device returning values without
  // error, which eliminates another potential error.
  // Left to verify are sanity checks of set parameters.

  // Valid volume range: [0, 255]
  EXPECT_EQ(-1, voe_volume_control_->SetSpeakerVolume(256));
  EXPECT_EQ(-1, voe_volume_control_->SetMicVolume(256));

  // Valid panning rage: [0, 1]
  EXPECT_EQ(-1, voe_volume_control_->SetOutputVolumePan(channel_, -0.1f, 0.5f));
  EXPECT_EQ(-1, voe_volume_control_->SetOutputVolumePan(channel_, 1.1f, 0.5f));
  EXPECT_EQ(-1, voe_volume_control_->SetOutputVolumePan(channel_, 0.5f, -0.1f));
  EXPECT_EQ(-1, voe_volume_control_->SetOutputVolumePan(channel_, 0.5f, 1.1f));
}

TEST_F(VolumeTest, DefaultSpeakerVolumeIsAtMost255) {
  unsigned int volume = 1000;
  EXPECT_EQ(0, voe_volume_control_->GetSpeakerVolume(volume));
  EXPECT_LE(volume, 255u);
}

TEST_F(VolumeTest, SetVolumeBeforePlayoutWorks) {
  // This is a rather specialized test, intended to exercise some PulseAudio
  // code. However, these conditions should be satisfied on any platform.
  unsigned int original_volume = 0;
  EXPECT_EQ(0, voe_volume_control_->GetSpeakerVolume(original_volume));
  Sleep(1000);

  EXPECT_EQ(0, voe_volume_control_->SetSpeakerVolume(200));
  unsigned int volume;
  EXPECT_EQ(0, voe_volume_control_->GetSpeakerVolume(volume));
  ExpectVolumeNear(200u, volume);

  PausePlaying();
  ResumePlaying();
  EXPECT_EQ(0, voe_volume_control_->GetSpeakerVolume(volume));
  // Ensure the volume has not changed after resuming playout.
  ExpectVolumeNear(200u, volume);

  PausePlaying();
  EXPECT_EQ(0, voe_volume_control_->SetSpeakerVolume(100));
  ResumePlaying();
  // Ensure the volume set while paused is retained.
  EXPECT_EQ(0, voe_volume_control_->GetSpeakerVolume(volume));
  ExpectVolumeNear(100u, volume);

  EXPECT_EQ(0, voe_volume_control_->SetSpeakerVolume(original_volume));
}

TEST_F(VolumeTest, ManualSetVolumeWorks) {
  unsigned int original_volume = 0;
  EXPECT_EQ(0, voe_volume_control_->GetSpeakerVolume(original_volume));
  Sleep(1000);

  TEST_LOG("Setting speaker volume to 0 out of 255.\n");
  EXPECT_EQ(0, voe_volume_control_->SetSpeakerVolume(0));
  unsigned int volume;
  EXPECT_EQ(0, voe_volume_control_->GetSpeakerVolume(volume));
  ExpectVolumeNear(0u, volume);
  Sleep(1000);

  TEST_LOG("Setting speaker volume to 100 out of 255.\n");
  EXPECT_EQ(0, voe_volume_control_->SetSpeakerVolume(100));
  EXPECT_EQ(0, voe_volume_control_->GetSpeakerVolume(volume));
  ExpectVolumeNear(100u, volume);
  Sleep(1000);

  // Set the volume to 255 very briefly so we don't blast the poor user
  // listening to this. This is just to test the call succeeds.
  EXPECT_EQ(0, voe_volume_control_->SetSpeakerVolume(255));
  EXPECT_EQ(0, voe_volume_control_->GetSpeakerVolume(volume));
  ExpectVolumeNear(255u, volume);

  TEST_LOG("Setting speaker volume to the original %d out of 255.\n",
      original_volume);
  EXPECT_EQ(0, voe_volume_control_->SetSpeakerVolume(original_volume));
  Sleep(1000);
}

TEST_F(VolumeTest, DefaultMicrophoneVolumeIsAtMost255) {
  unsigned int volume = 1000;
  bool could_get_mic_volume = voe_volume_control_->GetMicVolume(volume) == 0;
#if !defined(WEBRTC_LINUX)
  EXPECT_TRUE(could_get_mic_volume);
#endif
  if (could_get_mic_volume) {
    EXPECT_LE(volume, 255u);
  } else {
    TEST_LOG("Failed to get the microphone volume.");
    EXPECT_EQ(1000u, volume);
  }
}

// The test below is disabled due to issue webrtc:6206.
TEST_F(VolumeTest, DISABLED_ManualRequiresMicrophoneCanSetMicrophoneVolumeWithAgcOff) {
  SwitchToManualMicrophone();
  EXPECT_EQ(0, voe_apm_->SetAgcStatus(false));

  unsigned int original_volume = 0;
  bool could_get_mic_volume =
      (voe_volume_control_->GetMicVolume(original_volume) == 0);
#if !defined(WEBRTC_LINUX)
  EXPECT_TRUE(could_get_mic_volume);
#endif
  if (could_get_mic_volume)
    TEST_LOG("Current microphone volume is %u.\n", original_volume);
  else
    TEST_LOG("Failed to fetch current microphone volume.\n");

  TEST_LOG("Setting microphone volume to 0.\n");
  SetAndVerifyMicVolume(0);
  Sleep(1000);
  TEST_LOG("Setting microphone volume to 255.\n");
  SetAndVerifyMicVolume(255);
  Sleep(1000);
  if (could_get_mic_volume) {
    TEST_LOG("Setting microphone volume back to %u.\n", original_volume);
    SetAndVerifyMicVolume(original_volume);
    Sleep(1000);
  }
}

TEST_F(VolumeTest, ChannelScalingIsOneByDefault) {
  float scaling = -1.0f;

  EXPECT_EQ(0, voe_volume_control_->GetChannelOutputVolumeScaling(
      channel_, scaling));
  EXPECT_FLOAT_EQ(1.0f, scaling);
}

TEST_F(VolumeTest, ManualCanSetChannelScaling) {
  EXPECT_EQ(0, voe_volume_control_->SetChannelOutputVolumeScaling(
      channel_, 0.1f));

  float scaling = 1.0f;
  EXPECT_EQ(0, voe_volume_control_->GetChannelOutputVolumeScaling(
      channel_, scaling));

  EXPECT_FLOAT_EQ(0.1f, scaling);

  TEST_LOG("Channel scaling set to 0.1: audio should be barely audible.\n");
  Sleep(2000);
}

TEST_F(VolumeTest, InputMutingIsNotEnabledByDefault) {
  bool is_muted = true;
  EXPECT_EQ(0, voe_volume_control_->GetInputMute(channel_, is_muted));
  EXPECT_FALSE(is_muted);
}

TEST_F(VolumeTest, ManualInputMutingMutesMicrophone) {
  SwitchToManualMicrophone();
  // Enable muting.
  SetAndVerifyInputMute(true);
  TEST_LOG("Muted: talk into microphone and verify you can't hear yourself.\n");
  Sleep(2000);

  // Test that we can disable muting.
  SetAndVerifyInputMute(false);
  TEST_LOG("Unmuted: talk into microphone and verify you can hear yourself.\n");
  Sleep(2000);
}

TEST_F(VolumeTest, ManualTestInputAndOutputLevels) {
  SwitchToManualMicrophone();

  TEST_LOG("Speak and verify that the following levels look right:\n");
  for (int i = 0; i < 5; i++) {
    Sleep(1000);
    unsigned int input_level = 0;
    unsigned int output_level = 0;
    unsigned int input_level_full_range = 0;
    unsigned int output_level_full_range = 0;

    EXPECT_EQ(0, voe_volume_control_->GetSpeechInputLevel(
        input_level));
    EXPECT_EQ(0, voe_volume_control_->GetSpeechOutputLevel(
        channel_, output_level));
    EXPECT_EQ(0, voe_volume_control_->GetSpeechInputLevelFullRange(
        input_level_full_range));
    EXPECT_EQ(0, voe_volume_control_->GetSpeechOutputLevelFullRange(
        channel_, output_level_full_range));

    TEST_LOG("    warped levels (0-9)    : in=%5d, out=%5d\n",
        input_level, output_level);
    TEST_LOG("    linear levels (0-32768): in=%5d, out=%5d\n",
        input_level_full_range, output_level_full_range);
  }
}

TEST_F(VolumeTest, ChannelsAreNotPannedByDefault) {
  float left = -1.0;
  float right = -1.0;

  EXPECT_EQ(0, voe_volume_control_->GetOutputVolumePan(channel_, left, right));
  EXPECT_FLOAT_EQ(1.0, left);
  EXPECT_FLOAT_EQ(1.0, right);
}

TEST_F(VolumeTest, ManualTestChannelPanning) {
  TEST_LOG("Panning left.\n");
  EXPECT_EQ(0, voe_volume_control_->SetOutputVolumePan(channel_, 0.8f, 0.1f));
  Sleep(1000);

  TEST_LOG("Back to center.\n");
  EXPECT_EQ(0, voe_volume_control_->SetOutputVolumePan(channel_, 1.0f, 1.0f));
  Sleep(1000);

  TEST_LOG("Panning right.\n");
  EXPECT_EQ(0, voe_volume_control_->SetOutputVolumePan(channel_, 0.1f, 0.8f));
  Sleep(1000);

  // To finish, verify that the getter works.
  float left = 0.0f;
  float right = 0.0f;

  EXPECT_EQ(0, voe_volume_control_->GetOutputVolumePan(channel_, left, right));
  EXPECT_FLOAT_EQ(0.1f, left);
  EXPECT_FLOAT_EQ(0.8f, right);
}
