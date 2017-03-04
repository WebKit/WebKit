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

class AgcConfigTest : public AfterStreamingFixture {
 protected:
  void SetUp() {
    // These should be defaults for the AGC config.
    default_agc_config_.digitalCompressionGaindB = 9;
    default_agc_config_.limiterEnable = true;
    default_agc_config_.targetLeveldBOv = 3;
  }

  webrtc::AgcConfig default_agc_config_;
};

// Duplicated in apm_helpers_unittest.cc.
TEST_F(AgcConfigTest, HasCorrectDefaultConfiguration) {
  webrtc::AgcConfig agc_config;

  EXPECT_EQ(0, voe_apm_->GetAgcConfig(agc_config));

  EXPECT_EQ(default_agc_config_.targetLeveldBOv, agc_config.targetLeveldBOv);
  EXPECT_EQ(default_agc_config_.digitalCompressionGaindB,
            agc_config.digitalCompressionGaindB);
  EXPECT_EQ(default_agc_config_.limiterEnable, agc_config.limiterEnable);
}

// Not needed anymore - we're not returning errors anymore, just logging.
TEST_F(AgcConfigTest, DealsWithInvalidParameters) {
  webrtc::AgcConfig agc_config = default_agc_config_;
  agc_config.digitalCompressionGaindB = 91;
  EXPECT_EQ(-1, voe_apm_->SetAgcConfig(agc_config)) << "Should not be able "
      "to set gain to more than 90 dB.";
  EXPECT_EQ(VE_APM_ERROR, voe_base_->LastError());

  agc_config = default_agc_config_;
  agc_config.targetLeveldBOv = 32;
  EXPECT_EQ(-1, voe_apm_->SetAgcConfig(agc_config)) << "Should not be able "
      "to set target level to more than 31.";
  EXPECT_EQ(VE_APM_ERROR, voe_base_->LastError());
}

// Duplicated in apm_helpers_unittest.cc.
TEST_F(AgcConfigTest, CanGetAndSetAgcStatus) {
  webrtc::AgcConfig agc_config;
  agc_config.digitalCompressionGaindB = 17;
  agc_config.targetLeveldBOv = 11;
  agc_config.limiterEnable = false;

  webrtc::AgcConfig actual_config;
  EXPECT_EQ(0, voe_apm_->SetAgcConfig(agc_config));
  EXPECT_EQ(0, voe_apm_->GetAgcConfig(actual_config));

  EXPECT_EQ(agc_config.digitalCompressionGaindB,
            actual_config.digitalCompressionGaindB);
  EXPECT_EQ(agc_config.limiterEnable,
            actual_config.limiterEnable);
  EXPECT_EQ(agc_config.targetLeveldBOv,
            actual_config.targetLeveldBOv);
}
