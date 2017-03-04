/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/engine/apm_helpers.h"

#include "webrtc/media/engine/webrtcvoe.h"
#include "webrtc/modules/audio_coding/codecs/mock/mock_audio_decoder_factory.h"
#include "webrtc/modules/audio_device/include/mock_audio_device.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/voice_engine/transmit_mixer.h"

namespace webrtc {
namespace {

constexpr AgcConfig kDefaultAgcConfig = { 3, 9, true };

struct TestHelper {
  TestHelper() {
    // Reply with a 10ms timer every time TimeUntilNextProcess is called to
    // avoid entering a tight loop on the process thread.
    EXPECT_CALL(mock_audio_device_, TimeUntilNextProcess())
        .WillRepeatedly(testing::Return(10));

    // This replicates the conditions from voe_auto_test.
    Config config;
    config.Set<ExperimentalAgc>(new ExperimentalAgc(false));
    EXPECT_EQ(0, voe_wrapper_.base()->Init(
        &mock_audio_device_,
        AudioProcessing::Create(config),
        MockAudioDecoderFactory::CreateEmptyFactory()));
  }

  AudioProcessing* apm() {
    return voe_wrapper_.base()->audio_processing();
  }

  const AudioProcessing* apm() const {
    return voe_wrapper_.base()->audio_processing();
  }

  test::MockAudioDeviceModule* adm() {
    return &mock_audio_device_;
  }

  voe::TransmitMixer* transmit_mixer() {
    return voe_wrapper_.base()->transmit_mixer();
  }

  bool GetEcMetricsStatus() const {
    EchoCancellation* ec = apm()->echo_cancellation();
    bool metrics_enabled = ec->are_metrics_enabled();
    EXPECT_EQ(metrics_enabled, ec->is_delay_logging_enabled());
    return metrics_enabled;
  }

  bool CanGetEcMetrics() const {
    EchoCancellation* ec = apm()->echo_cancellation();
    EchoCancellation::Metrics metrics;
    int metrics_result = ec->GetMetrics(&metrics);
    int median = 0;
    int std = 0;
    float fraction = 0;
    int delay_metrics_result = ec->GetDelayMetrics(&median, &std, &fraction);
    return metrics_result == AudioProcessing::kNoError &&
           delay_metrics_result == AudioProcessing::kNoError;
  }

 private:
  testing::NiceMock<test::MockAudioDeviceModule> mock_audio_device_;
  cricket::VoEWrapper voe_wrapper_;
};
}  // namespace

TEST(ApmHelpersTest, AgcConfig_DefaultConfiguration) {
  TestHelper helper;
  AgcConfig agc_config =
      apm_helpers::GetAgcConfig(helper.apm());

  EXPECT_EQ(kDefaultAgcConfig.targetLeveldBOv, agc_config.targetLeveldBOv);
  EXPECT_EQ(kDefaultAgcConfig.digitalCompressionGaindB,
            agc_config.digitalCompressionGaindB);
  EXPECT_EQ(kDefaultAgcConfig.limiterEnable, agc_config.limiterEnable);
}

TEST(ApmHelpersTest, AgcConfig_GetAndSet) {
  const AgcConfig agc_config = { 11, 17, false };

  TestHelper helper;
  apm_helpers::SetAgcConfig(helper.apm(), agc_config);
  AgcConfig actual_config =
      apm_helpers::GetAgcConfig(helper.apm());

  EXPECT_EQ(agc_config.digitalCompressionGaindB,
            actual_config.digitalCompressionGaindB);
  EXPECT_EQ(agc_config.limiterEnable,
            actual_config.limiterEnable);
  EXPECT_EQ(agc_config.targetLeveldBOv,
            actual_config.targetLeveldBOv);
}

TEST(ApmHelpersTest, AgcStatus_DefaultMode) {
  TestHelper helper;
  GainControl* gc = helper.apm()->gain_control();
#if defined(WEBRTC_IOS) || defined(WEBRTC_ANDROID)
  EXPECT_FALSE(gc->is_enabled());
  EXPECT_EQ(GainControl::kFixedDigital, gc->mode());
#else
  EXPECT_TRUE(gc->is_enabled());
  EXPECT_EQ(GainControl::kAdaptiveAnalog, gc->mode());
#endif
}

TEST(ApmHelpersTest, AgcStatus_EnableDisable) {
  TestHelper helper;
  GainControl* gc = helper.apm()->gain_control();
#if defined(WEBRTC_IOS) || defined(WEBRTC_ANDROID)
  apm_helpers::SetAgcStatus(helper.apm(), helper.adm(), false,
                            kAgcFixedDigital);
  EXPECT_FALSE(gc->is_enabled());
  EXPECT_EQ(GainControl::kFixedDigital, gc->mode());

  apm_helpers::SetAgcStatus(helper.apm(), helper.adm(), true,
                            kAgcFixedDigital);
  EXPECT_TRUE(gc->is_enabled());
  EXPECT_EQ(GainControl::kFixedDigital, gc->mode());
#else
  EXPECT_CALL(*helper.adm(), SetAGC(false)).WillOnce(testing::Return(0));
  apm_helpers::SetAgcStatus(helper.apm(), helper.adm(), false,
                            kAgcAdaptiveAnalog);
  EXPECT_FALSE(gc->is_enabled());
  EXPECT_EQ(GainControl::kAdaptiveAnalog, gc->mode());

  EXPECT_CALL(*helper.adm(), SetAGC(true)).WillOnce(testing::Return(0));
  apm_helpers::SetAgcStatus(helper.apm(), helper.adm(), true,
                            kAgcAdaptiveAnalog);
  EXPECT_TRUE(gc->is_enabled());
  EXPECT_EQ(GainControl::kAdaptiveAnalog, gc->mode());
#endif
}

TEST(ApmHelpersTest, EcStatus_DefaultMode) {
  TestHelper helper;
  EchoCancellation* ec = helper.apm()->echo_cancellation();
  EchoControlMobile* ecm = helper.apm()->echo_control_mobile();
  EXPECT_FALSE(ec->is_enabled());
  EXPECT_FALSE(ecm->is_enabled());
}

TEST(ApmHelpersTest, EcStatus_EnableDisable) {
  TestHelper helper;
  EchoCancellation* ec = helper.apm()->echo_cancellation();
  EchoControlMobile* ecm = helper.apm()->echo_control_mobile();

  apm_helpers::SetEcStatus(helper.apm(), true, kEcAecm);
  EXPECT_FALSE(ec->is_enabled());
  EXPECT_TRUE(ecm->is_enabled());

  apm_helpers::SetEcStatus(helper.apm(), false, kEcAecm);
  EXPECT_FALSE(ec->is_enabled());
  EXPECT_FALSE(ecm->is_enabled());

  apm_helpers::SetEcStatus(helper.apm(), true, kEcConference);
  EXPECT_TRUE(ec->is_enabled());
  EXPECT_FALSE(ecm->is_enabled());
  EXPECT_EQ(EchoCancellation::kHighSuppression, ec->suppression_level());

  apm_helpers::SetEcStatus(helper.apm(), false, kEcConference);
  EXPECT_FALSE(ec->is_enabled());
  EXPECT_FALSE(ecm->is_enabled());
  EXPECT_EQ(EchoCancellation::kHighSuppression, ec->suppression_level());

  apm_helpers::SetEcStatus(helper.apm(), true, kEcAecm);
  EXPECT_FALSE(ec->is_enabled());
  EXPECT_TRUE(ecm->is_enabled());
}

TEST(ApmHelpersTest, EcMetrics_DefaultMode) {
  TestHelper helper;
  apm_helpers::SetEcStatus(helper.apm(), true, kEcConference);
  EXPECT_TRUE(helper.GetEcMetricsStatus());
}

TEST(ApmHelpersTest, EcMetrics_CanEnableDisable) {
  TestHelper helper;
  apm_helpers::SetEcStatus(helper.apm(), true, kEcConference);

  apm_helpers::SetEcMetricsStatus(helper.apm(), true);
  EXPECT_TRUE(helper.GetEcMetricsStatus());
  apm_helpers::SetEcMetricsStatus(helper.apm(), false);
  EXPECT_FALSE(helper.GetEcMetricsStatus());
}

TEST(ApmHelpersTest, EcMetrics_NoStatsUnlessEcMetricsAndEcEnabled) {
  TestHelper helper;
  EXPECT_FALSE(helper.CanGetEcMetrics());

  apm_helpers::SetEcMetricsStatus(helper.apm(), true);
  EXPECT_FALSE(helper.CanGetEcMetrics());

  apm_helpers::SetEcStatus(helper.apm(), true, kEcConference);
  EXPECT_TRUE(helper.CanGetEcMetrics());

  apm_helpers::SetEcMetricsStatus(helper.apm(), false);
  EXPECT_FALSE(helper.CanGetEcMetrics());
}

TEST(ApmHelpersTest, AecmMode_DefaultMode) {
  TestHelper helper;
  EchoControlMobile* ecm = helper.apm()->echo_control_mobile();
  EXPECT_EQ(EchoControlMobile::kSpeakerphone, ecm->routing_mode());
  EXPECT_TRUE(ecm->is_comfort_noise_enabled());
}

TEST(ApmHelpersTest, AecmMode_EnableDisableCng) {
  TestHelper helper;
  EchoControlMobile* ecm = helper.apm()->echo_control_mobile();
  apm_helpers::SetAecmMode(helper.apm(), false);
  EXPECT_FALSE(ecm->is_comfort_noise_enabled());
  apm_helpers::SetAecmMode(helper.apm(), true);
  EXPECT_TRUE(ecm->is_comfort_noise_enabled());
}

TEST(ApmHelpersTest, NsStatus_DefaultMode) {
  TestHelper helper;
  NoiseSuppression* ns = helper.apm()->noise_suppression();
  EXPECT_EQ(NoiseSuppression::kModerate, ns->level());
  EXPECT_FALSE(ns->is_enabled());
}

TEST(ApmHelpersTest, NsStatus_EnableDisable) {
  TestHelper helper;
  NoiseSuppression* ns = helper.apm()->noise_suppression();
  apm_helpers::SetNsStatus(helper.apm(), true);
  EXPECT_EQ(NoiseSuppression::kHigh, ns->level());
  EXPECT_TRUE(ns->is_enabled());
  apm_helpers::SetNsStatus(helper.apm(), false);
  EXPECT_EQ(NoiseSuppression::kHigh, ns->level());
  EXPECT_FALSE(ns->is_enabled());
}

TEST(ApmHelpersTest, TypingDetectionStatus_DefaultMode) {
  TestHelper helper;
  VoiceDetection* vd = helper.apm()->voice_detection();
  EXPECT_FALSE(vd->is_enabled());
}

TEST(ApmHelpersTest, TypingDetectionStatus_EnableDisable) {
  TestHelper helper;
  VoiceDetection* vd = helper.apm()->voice_detection();
  apm_helpers::SetTypingDetectionStatus(helper.apm(), true);
  EXPECT_TRUE(vd->is_enabled());
  apm_helpers::SetTypingDetectionStatus(helper.apm(), false);
  EXPECT_FALSE(vd->is_enabled());
}

// TODO(solenberg): Move this test to a better place - added here for the sake
// of duplicating all relevant tests from audio_processing_test.cc.
TEST(ApmHelpersTest, HighPassFilter_DefaultMode) {
  TestHelper helper;
  EXPECT_TRUE(helper.apm()->high_pass_filter()->is_enabled());
}

// TODO(solenberg): Move this test to a better place - added here for the sake
// of duplicating all relevant tests from audio_processing_test.cc.
TEST(ApmHelpersTest, StereoSwapping_DefaultMode) {
  TestHelper helper;
  EXPECT_FALSE(helper.transmit_mixer()->IsStereoChannelSwappingEnabled());
}

// TODO(solenberg): Move this test to a better place - added here for the sake
// of duplicating all relevant tests from audio_processing_test.cc.
TEST(ApmHelpersTest, StereoSwapping_EnableDisable) {
  TestHelper helper;
  helper.transmit_mixer()->EnableStereoChannelSwapping(true);
  EXPECT_TRUE(helper.transmit_mixer()->IsStereoChannelSwappingEnabled());
  helper.transmit_mixer()->EnableStereoChannelSwapping(false);
  EXPECT_FALSE(helper.transmit_mixer()->IsStereoChannelSwappingEnabled());
}
}  // namespace webrtc
