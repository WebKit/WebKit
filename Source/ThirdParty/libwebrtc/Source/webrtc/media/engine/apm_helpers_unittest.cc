/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/apm_helpers.h"

#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr AgcConfig kDefaultAgcConfig = {3, 9, true};

struct TestHelper {
  TestHelper() {
    // This replicates the conditions from voe_auto_test.
    Config config;
    config.Set<ExperimentalAgc>(new ExperimentalAgc(false));
    apm_ = rtc::scoped_refptr<AudioProcessing>(
        AudioProcessingBuilder().Create(config));
    apm_helpers::Init(apm());
  }

  AudioProcessing* apm() { return apm_.get(); }

  const AudioProcessing* apm() const { return apm_.get(); }

 private:
  rtc::scoped_refptr<AudioProcessing> apm_;
};
}  // namespace

TEST(ApmHelpersTest, AgcConfig_DefaultConfiguration) {
  TestHelper helper;
  AgcConfig agc_config = apm_helpers::GetAgcConfig(helper.apm());

  EXPECT_EQ(kDefaultAgcConfig.targetLeveldBOv, agc_config.targetLeveldBOv);
  EXPECT_EQ(kDefaultAgcConfig.digitalCompressionGaindB,
            agc_config.digitalCompressionGaindB);
  EXPECT_EQ(kDefaultAgcConfig.limiterEnable, agc_config.limiterEnable);
}

TEST(ApmHelpersTest, AgcConfig_GetAndSet) {
  const AgcConfig agc_config = {11, 17, false};

  TestHelper helper;
  apm_helpers::SetAgcConfig(helper.apm(), agc_config);
  AgcConfig actual_config = apm_helpers::GetAgcConfig(helper.apm());

  EXPECT_EQ(agc_config.digitalCompressionGaindB,
            actual_config.digitalCompressionGaindB);
  EXPECT_EQ(agc_config.limiterEnable, actual_config.limiterEnable);
  EXPECT_EQ(agc_config.targetLeveldBOv, actual_config.targetLeveldBOv);
}

TEST(ApmHelpersTest, AgcStatus_DefaultMode) {
  TestHelper helper;
  GainControl* gc = helper.apm()->gain_control();
  EXPECT_FALSE(gc->is_enabled());
#if defined(TARGET_IPHONE_SIMULATOR) && TARGET_IPHONE_SIMULATOR
  EXPECT_EQ(GainControl::kAdaptiveAnalog, gc->mode());
#elif defined(WEBRTC_IOS) || defined(WEBRTC_ANDROID)
  EXPECT_EQ(GainControl::kFixedDigital, gc->mode());
#else
  EXPECT_EQ(GainControl::kAdaptiveAnalog, gc->mode());
#endif
}

TEST(ApmHelpersTest, AgcStatus_EnableDisable) {
  TestHelper helper;
  GainControl* gc = helper.apm()->gain_control();
#if defined(WEBRTC_IOS) || defined(WEBRTC_ANDROID)
  apm_helpers::SetAgcStatus(helper.apm(), false);
  EXPECT_FALSE(gc->is_enabled());
  EXPECT_EQ(GainControl::kFixedDigital, gc->mode());

  apm_helpers::SetAgcStatus(helper.apm(), true);
  EXPECT_TRUE(gc->is_enabled());
  EXPECT_EQ(GainControl::kFixedDigital, gc->mode());
#else
  apm_helpers::SetAgcStatus(helper.apm(), false);
  EXPECT_FALSE(gc->is_enabled());
  EXPECT_EQ(GainControl::kAdaptiveAnalog, gc->mode());
  apm_helpers::SetAgcStatus(helper.apm(), true);
  EXPECT_TRUE(gc->is_enabled());
  EXPECT_EQ(GainControl::kAdaptiveAnalog, gc->mode());
#endif
}

TEST(ApmHelpersTest, EcStatus_DefaultMode) {
  TestHelper helper;
  webrtc::AudioProcessing::Config config = helper.apm()->GetConfig();
  EXPECT_FALSE(config.echo_canceller.enabled);
}

TEST(ApmHelpersTest, EcStatus_EnableDisable) {
  TestHelper helper;
  webrtc::AudioProcessing::Config config;

  apm_helpers::SetEcStatus(helper.apm(), true, kEcAecm);
  config = helper.apm()->GetConfig();
  EXPECT_TRUE(config.echo_canceller.enabled);
  EXPECT_TRUE(config.echo_canceller.mobile_mode);

  apm_helpers::SetEcStatus(helper.apm(), false, kEcAecm);
  config = helper.apm()->GetConfig();
  EXPECT_FALSE(config.echo_canceller.enabled);

  apm_helpers::SetEcStatus(helper.apm(), true, kEcConference);
  config = helper.apm()->GetConfig();
  EXPECT_TRUE(config.echo_canceller.enabled);
  EXPECT_FALSE(config.echo_canceller.mobile_mode);

  apm_helpers::SetEcStatus(helper.apm(), false, kEcConference);
  config = helper.apm()->GetConfig();
  EXPECT_FALSE(config.echo_canceller.enabled);

  apm_helpers::SetEcStatus(helper.apm(), true, kEcAecm);
  config = helper.apm()->GetConfig();
  EXPECT_TRUE(config.echo_canceller.enabled);
  EXPECT_TRUE(config.echo_canceller.mobile_mode);
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
  EXPECT_EQ(VoiceDetection::kVeryLowLikelihood, vd->likelihood());
  EXPECT_TRUE(vd->is_enabled());
  apm_helpers::SetTypingDetectionStatus(helper.apm(), false);
  EXPECT_EQ(VoiceDetection::kVeryLowLikelihood, vd->likelihood());
  EXPECT_FALSE(vd->is_enabled());
}

// TODO(solenberg): Move this test to a better place - added here for the sake
// of duplicating all relevant tests from audio_processing_test.cc.
TEST(ApmHelpersTest, HighPassFilter_DefaultMode) {
  TestHelper helper;
  EXPECT_FALSE(helper.apm()->high_pass_filter()->is_enabled());
}
}  // namespace webrtc
