/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/voice_engine/test/auto_test/fixtures/after_streaming_fixture.h"
#include "webrtc/voice_engine/test/auto_test/voe_standard_test.h"

class AudioProcessingTest : public AfterStreamingFixture {
 protected:
  // Note: Be careful with this one, it is used in the
  // Android / iPhone part too.
  void TryEnablingAgcWithMode(webrtc::AgcModes agc_mode_to_set) {
    EXPECT_EQ(0, voe_apm_->SetAgcStatus(true, agc_mode_to_set));

    bool agc_enabled = false;
    webrtc::AgcModes agc_mode = webrtc::kAgcDefault;

    EXPECT_EQ(0, voe_apm_->GetAgcStatus(agc_enabled, agc_mode));
    EXPECT_TRUE(agc_enabled);
    EXPECT_EQ(agc_mode_to_set, agc_mode);
  }

  // EC modes can map to other EC modes, so we have a separate parameter
  // for what we expect the EC mode to be set to.
  void TryEnablingEcWithMode(webrtc::EcModes ec_mode_to_set,
                             webrtc::EcModes expected_mode) {
    EXPECT_EQ(0, voe_apm_->SetEcStatus(true, ec_mode_to_set));

    bool ec_enabled = true;
    webrtc::EcModes ec_mode = webrtc::kEcDefault;

    EXPECT_EQ(0, voe_apm_->GetEcStatus(ec_enabled, ec_mode));

    EXPECT_EQ(expected_mode, ec_mode);
  }

  // Here, the CNG mode will be expected to be on or off depending on the mode.
  void TryEnablingAecmWithMode(webrtc::AecmModes aecm_mode_to_set,
                               bool cng_enabled_to_set) {
    EXPECT_EQ(0, voe_apm_->SetAecmMode(aecm_mode_to_set, cng_enabled_to_set));

    bool cng_enabled = false;
    webrtc::AecmModes aecm_mode = webrtc::kAecmEarpiece;

    voe_apm_->GetAecmMode(aecm_mode, cng_enabled);

    EXPECT_EQ(cng_enabled_to_set, cng_enabled);
    EXPECT_EQ(aecm_mode_to_set, aecm_mode);
  }

  void TryEnablingNsWithMode(webrtc::NsModes ns_mode_to_set,
                             webrtc::NsModes expected_ns_mode) {
    EXPECT_EQ(0, voe_apm_->SetNsStatus(true, ns_mode_to_set));

    bool ns_status = true;
    webrtc::NsModes ns_mode = webrtc::kNsDefault;
    EXPECT_EQ(0, voe_apm_->GetNsStatus(ns_status, ns_mode));

    EXPECT_TRUE(ns_status);
    EXPECT_EQ(expected_ns_mode, ns_mode);
  }

  void TryDetectingSilence() {
    // Here, speech is running. Shut down speech.
    EXPECT_EQ(0, voe_codec_->SetVADStatus(channel_, true));
    EXPECT_EQ(0, voe_volume_control_->SetInputMute(channel_, true));
    EXPECT_EQ(0, voe_file_->StopPlayingFileAsMicrophone(channel_));

    // We should detect the silence after a short time.
    Sleep(50);
    for (int i = 0; i < 25; i++) {
      EXPECT_EQ(0, voe_apm_->VoiceActivityIndicator(channel_));
      Sleep(10);
    }
  }

  void TryDetectingSpeechAfterSilence() {
    // Re-enable speech.
    RestartFakeMicrophone();
    EXPECT_EQ(0, voe_codec_->SetVADStatus(channel_, false));
    EXPECT_EQ(0, voe_volume_control_->SetInputMute(channel_, false));

    // We should detect the speech after a short time.
    for (int i = 0; i < 50; i++) {
      if (voe_apm_->VoiceActivityIndicator(channel_) == 1) {
        return;
      }
      Sleep(10);
    }

    ADD_FAILURE() << "Failed to detect speech within 500 ms.";
  }
};

#if !defined(WEBRTC_IOS) && !defined(WEBRTC_ANDROID)

// Duplicated in apm_helpers_unittest.cc.
TEST_F(AudioProcessingTest, AgcIsOnByDefault) {
  bool agc_enabled = false;
  webrtc::AgcModes agc_mode = webrtc::kAgcAdaptiveAnalog;

  EXPECT_EQ(0, voe_apm_->GetAgcStatus(agc_enabled, agc_mode));
  EXPECT_TRUE(agc_enabled);
  EXPECT_EQ(webrtc::kAgcAdaptiveAnalog, agc_mode);
}

// Duplicated in apm_helpers_unittest.cc.
TEST_F(AudioProcessingTest, CanEnableAgcWithAllModes) {
  TryEnablingAgcWithMode(webrtc::kAgcAdaptiveDigital);
  TryEnablingAgcWithMode(webrtc::kAgcAdaptiveAnalog);
  TryEnablingAgcWithMode(webrtc::kAgcFixedDigital);
}

// Duplicated in apm_helpers_unittest.cc.
TEST_F(AudioProcessingTest, EcIsDisabledAndAecIsDefaultEcMode) {
  bool ec_enabled = true;
  webrtc::EcModes ec_mode = webrtc::kEcDefault;

  EXPECT_EQ(0, voe_apm_->GetEcStatus(ec_enabled, ec_mode));
  EXPECT_FALSE(ec_enabled);
  EXPECT_EQ(webrtc::kEcAec, ec_mode);
}

// Not needed anymore - apm_helpers::SetEcStatus() doesn't take kEcAec.
TEST_F(AudioProcessingTest, EnablingEcAecShouldEnableEcAec) {
  TryEnablingEcWithMode(webrtc::kEcAec, webrtc::kEcAec);
}

// Duplicated in apm_helpers_unittest.cc.
TEST_F(AudioProcessingTest, EnablingEcConferenceShouldEnableEcAec) {
  TryEnablingEcWithMode(webrtc::kEcConference, webrtc::kEcAec);
}

// Not needed anymore - apm_helpers::SetEcStatus() doesn't take kEcDefault.
TEST_F(AudioProcessingTest, EcModeIsPreservedWhenEcIsTurnedOff) {
  TryEnablingEcWithMode(webrtc::kEcConference, webrtc::kEcAec);

  EXPECT_EQ(0, voe_apm_->SetEcStatus(false));

  bool ec_enabled = true;
  webrtc::EcModes ec_mode = webrtc::kEcDefault;
  EXPECT_EQ(0, voe_apm_->GetEcStatus(ec_enabled, ec_mode));

  EXPECT_FALSE(ec_enabled);
  EXPECT_EQ(webrtc::kEcAec, ec_mode);
}

// Not needed anymore - apm_helpers::SetEcStatus() doesn't take kEcDefault.
TEST_F(AudioProcessingTest, CanEnableAndDisableEcModeSeveralTimesInARow) {
  for (int i = 0; i < 10; i++) {
    EXPECT_EQ(0, voe_apm_->SetEcStatus(true));
    EXPECT_EQ(0, voe_apm_->SetEcStatus(false));
  }

  bool ec_enabled = true;
  webrtc::EcModes ec_mode = webrtc::kEcDefault;
  EXPECT_EQ(0, voe_apm_->GetEcStatus(ec_enabled, ec_mode));

  EXPECT_FALSE(ec_enabled);
  EXPECT_EQ(webrtc::kEcAec, ec_mode);
}

#endif   // !WEBRTC_IOS && !WEBRTC_ANDROID

// Duplicated in apm_helpers_unittest.cc.
TEST_F(AudioProcessingTest, EnablingEcAecmShouldEnableEcAecm) {
  // This one apparently applies to Android and iPhone as well.
  TryEnablingEcWithMode(webrtc::kEcAecm, webrtc::kEcAecm);
}

// Duplicated in apm_helpers_unittest.cc.
TEST_F(AudioProcessingTest, EcAecmModeIsEnabledAndSpeakerphoneByDefault) {
  bool cng_enabled = false;
  webrtc::AecmModes aecm_mode = webrtc::kAecmEarpiece;

  voe_apm_->GetAecmMode(aecm_mode, cng_enabled);

  EXPECT_TRUE(cng_enabled);
  EXPECT_EQ(webrtc::kAecmSpeakerphone, aecm_mode);
}

// Duplicated in apm_helpers_unittest.cc.
TEST_F(AudioProcessingTest, CanSetAecmMode) {
  EXPECT_EQ(0, voe_apm_->SetEcStatus(true, webrtc::kEcAecm));

  // Try some AECM mode - CNG enabled combinations.
  TryEnablingAecmWithMode(webrtc::kAecmEarpiece, true);
  TryEnablingAecmWithMode(webrtc::kAecmEarpiece, false);
  TryEnablingAecmWithMode(webrtc::kAecmLoudEarpiece, true);
  TryEnablingAecmWithMode(webrtc::kAecmLoudSpeakerphone, false);
  TryEnablingAecmWithMode(webrtc::kAecmQuietEarpieceOrHeadset, true);
  TryEnablingAecmWithMode(webrtc::kAecmSpeakerphone, false);
}

// Duplicated in apm_helpers_unittest.cc.
TEST_F(AudioProcessingTest, NsIsOffWithModerateSuppressionByDefault) {
  bool ns_status = true;
  webrtc::NsModes ns_mode = webrtc::kNsDefault;
  EXPECT_EQ(0, voe_apm_->GetNsStatus(ns_status, ns_mode));

  EXPECT_FALSE(ns_status);
  EXPECT_EQ(webrtc::kNsModerateSuppression, ns_mode);
}

// Duplicated in apm_helpers_unittest.cc.
TEST_F(AudioProcessingTest, CanSetNsMode) {
  // Concrete suppression values map to themselves.
  TryEnablingNsWithMode(webrtc::kNsHighSuppression,
                        webrtc::kNsHighSuppression);
  TryEnablingNsWithMode(webrtc::kNsLowSuppression,
                        webrtc::kNsLowSuppression);
  TryEnablingNsWithMode(webrtc::kNsModerateSuppression,
                        webrtc::kNsModerateSuppression);
  TryEnablingNsWithMode(webrtc::kNsVeryHighSuppression,
                        webrtc::kNsVeryHighSuppression);

  // Conference and Default map to concrete values.
  TryEnablingNsWithMode(webrtc::kNsConference,
                        webrtc::kNsHighSuppression);
  TryEnablingNsWithMode(webrtc::kNsDefault,
                        webrtc::kNsModerateSuppression);
}

// TODO(solenberg): Duplicate this test at the voe::Channel layer.
// Not needed anymore - API is unused.
TEST_F(AudioProcessingTest, VadIsDisabledByDefault) {
  bool vad_enabled;
  bool disabled_dtx;
  webrtc::VadModes vad_mode;

  EXPECT_EQ(0, voe_codec_->GetVADStatus(
      channel_, vad_enabled, vad_mode, disabled_dtx));

  EXPECT_FALSE(vad_enabled);
}

// Not needed anymore - API is unused.
TEST_F(AudioProcessingTest, VoiceActivityIndicatorReturns1WithSpeechOn) {
  // This sleep is necessary since the voice detection algorithm needs some
  // time to detect the speech from the fake microphone.
  Sleep(500);
  EXPECT_EQ(1, voe_apm_->VoiceActivityIndicator(channel_));
}

// Not needed anymore - API is unused.
TEST_F(AudioProcessingTest, CanSetDelayOffset) {
  voe_apm_->SetDelayOffsetMs(50);
  EXPECT_EQ(50, voe_apm_->DelayOffsetMs());
  voe_apm_->SetDelayOffsetMs(-50);
  EXPECT_EQ(-50, voe_apm_->DelayOffsetMs());
}

// Duplicated in apm_helpers_unittest.cc.
TEST_F(AudioProcessingTest, HighPassFilterIsOnByDefault) {
  EXPECT_TRUE(voe_apm_->IsHighPassFilterEnabled());
}

// TODO(solenberg): Check that sufficient testing is done in APM.
// Not needed anymore - API is unused.
TEST_F(AudioProcessingTest, CanSetHighPassFilter) {
  EXPECT_EQ(0, voe_apm_->EnableHighPassFilter(true));
  EXPECT_TRUE(voe_apm_->IsHighPassFilterEnabled());
  EXPECT_EQ(0, voe_apm_->EnableHighPassFilter(false));
  EXPECT_FALSE(voe_apm_->IsHighPassFilterEnabled());
}

// Duplicated in apm_helpers_unittest.cc.
TEST_F(AudioProcessingTest, StereoChannelSwappingIsOffByDefault) {
  EXPECT_FALSE(voe_apm_->IsStereoChannelSwappingEnabled());
}

// Duplicated in apm_helpers_unittest.cc.
TEST_F(AudioProcessingTest, CanSetStereoChannelSwapping) {
  voe_apm_->EnableStereoChannelSwapping(true);
  EXPECT_TRUE(voe_apm_->IsStereoChannelSwappingEnabled());
  voe_apm_->EnableStereoChannelSwapping(false);
  EXPECT_FALSE(voe_apm_->IsStereoChannelSwappingEnabled());
}

// TODO(solenberg): Check that sufficient testing is done in APM.
TEST_F(AudioProcessingTest, CanStartAndStopDebugRecording) {
  std::string output_path = webrtc::test::OutputPath();
  std::string output_file = output_path + "apm_debug.txt";

  EXPECT_EQ(0, voe_apm_->StartDebugRecording(output_file.c_str()));
  Sleep(1000);
  EXPECT_EQ(0, voe_apm_->StopDebugRecording());
}

#if defined(WEBRTC_IOS) || defined(WEBRTC_ANDROID)

// Duplicated in apm_helpers_unittest.cc.
TEST_F(AudioProcessingTest, AgcIsOffByDefaultAndDigital) {
  bool agc_enabled = true;
  webrtc::AgcModes agc_mode = webrtc::kAgcAdaptiveAnalog;

  EXPECT_EQ(0, voe_apm_->GetAgcStatus(agc_enabled, agc_mode));
  EXPECT_FALSE(agc_enabled);
  EXPECT_EQ(webrtc::kAgcAdaptiveDigital, agc_mode);
}

// Duplicated in apm_helpers_unittest.cc.
TEST_F(AudioProcessingTest, CanEnableAgcInAdaptiveDigitalMode) {
  TryEnablingAgcWithMode(webrtc::kAgcAdaptiveDigital);
}

// Duplicated in apm_helpers_unittest.cc.
TEST_F(AudioProcessingTest, AgcIsPossibleExceptInAdaptiveAnalogMode) {
  EXPECT_EQ(-1, voe_apm_->SetAgcStatus(true, webrtc::kAgcAdaptiveAnalog));
  EXPECT_EQ(0, voe_apm_->SetAgcStatus(true, webrtc::kAgcFixedDigital));
  EXPECT_EQ(0, voe_apm_->SetAgcStatus(true, webrtc::kAgcAdaptiveDigital));
}

// Duplicated in apm_helpers_unittest.cc.
TEST_F(AudioProcessingTest, EcIsDisabledAndAecmIsDefaultEcMode) {
  bool ec_enabled = true;
  webrtc::EcModes ec_mode = webrtc::kEcDefault;

  EXPECT_EQ(0, voe_apm_->GetEcStatus(ec_enabled, ec_mode));
  EXPECT_FALSE(ec_enabled);
  EXPECT_EQ(webrtc::kEcAecm, ec_mode);
}

// Not needed anymore - API is unused.
TEST_F(AudioProcessingTest, TestVoiceActivityDetection) {
  TryDetectingSilence();
  TryDetectingSpeechAfterSilence();
}

#endif  // WEBRTC_IOS || WEBRTC_ANDROID
