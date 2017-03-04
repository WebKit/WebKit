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

class EcMetricsTest : public AfterStreamingFixture {
};

// Duplicated in apm_helpers_unittest.cc.
TEST_F(EcMetricsTest, EcMetricsAreOnByDefault) {
  // AEC must be enabled fist.
  EXPECT_EQ(0, voe_apm_->SetEcStatus(true, webrtc::kEcAec));

  bool enabled = false;
  EXPECT_EQ(0, voe_apm_->GetEcMetricsStatus(enabled));
  EXPECT_TRUE(enabled);
}

// Duplicated in apm_helpers_unittest.cc.
TEST_F(EcMetricsTest, CanEnableAndDisableEcMetrics) {
  // AEC must be enabled fist.
  EXPECT_EQ(0, voe_apm_->SetEcStatus(true, webrtc::kEcAec));

  EXPECT_EQ(0, voe_apm_->SetEcMetricsStatus(true));
  bool ec_on = false;
  EXPECT_EQ(0, voe_apm_->GetEcMetricsStatus(ec_on));
  ASSERT_TRUE(ec_on);
  EXPECT_EQ(0, voe_apm_->SetEcMetricsStatus(false));
  EXPECT_EQ(0, voe_apm_->GetEcMetricsStatus(ec_on));
  ASSERT_FALSE(ec_on);
}

// TODO(solenberg): Do we have higher or lower level tests that verify metrics?
//                  It's not the right test for this level.
TEST_F(EcMetricsTest, ManualTestEcMetrics) {
  SwitchToManualMicrophone();

  EXPECT_EQ(0, voe_apm_->SetEcMetricsStatus(true));

  // Must enable AEC to get valid echo metrics.
  EXPECT_EQ(0, voe_apm_->SetEcStatus(true, webrtc::kEcAec));

  TEST_LOG("Speak into microphone and check metrics for 5 seconds...\n");
  int erl, erle, rerl, a_nlp;
  int delay_median = 0;
  int delay_std = 0;
  float fraction_poor_delays = 0;

  for (int i = 0; i < 5; i++) {
    Sleep(1000);
    EXPECT_EQ(0, voe_apm_->GetEchoMetrics(erl, erle, rerl, a_nlp));
    EXPECT_EQ(0, voe_apm_->GetEcDelayMetrics(delay_median, delay_std,
                                             fraction_poor_delays));
    TEST_LOG("    Echo  : ERL=%5d, ERLE=%5d, RERL=%5d, A_NLP=%5d [dB], "
        " delay median=%3d, delay std=%3d [ms], "
        "fraction_poor_delays=%3.1f [%%]\n", erl, erle, rerl, a_nlp,
        delay_median, delay_std, fraction_poor_delays * 100);
  }

  EXPECT_EQ(0, voe_apm_->SetEcMetricsStatus(false));
}

// Duplicated in apm_helpers_unittest.cc.
TEST_F(EcMetricsTest, GetEcMetricsFailsIfEcNotEnabled) {
  int dummy = 0;
  EXPECT_EQ(0, voe_apm_->SetEcMetricsStatus(true));
  EXPECT_EQ(-1, voe_apm_->GetEchoMetrics(dummy, dummy, dummy, dummy));
  EXPECT_EQ(VE_APM_ERROR, voe_base_->LastError());
}

// Duplicated in apm_helpers_unittest.cc.
TEST_F(EcMetricsTest, GetEcDelayMetricsFailsIfEcNotEnabled) {
  int dummy = 0;
  float dummy_f = 0;
  EXPECT_EQ(0, voe_apm_->SetEcMetricsStatus(true));
  EXPECT_EQ(-1, voe_apm_->GetEcDelayMetrics(dummy, dummy, dummy_f));
  EXPECT_EQ(VE_APM_ERROR, voe_base_->LastError());
}

// TODO(solenberg): Do we have higher or lower level tests that verify metrics?
//                  It's not the right test for this level.
TEST_F(EcMetricsTest, ManualVerifyEcDelayMetrics) {
  SwitchToManualMicrophone();
  TEST_LOG("Verify EC Delay metrics:");
  EXPECT_EQ(0, voe_apm_->SetEcStatus(true));
  EXPECT_EQ(0, voe_apm_->SetEcMetricsStatus(true));

  for (int i = 0; i < 5; i++) {
    int delay, delay_std;
    float fraction_poor_delays;
    EXPECT_EQ(0, voe_apm_->GetEcDelayMetrics(delay, delay_std,
                                             fraction_poor_delays));
    TEST_LOG("Delay = %d, Delay Std = %d, Fraction poor delays = %3.1f\n",
             delay, delay_std, fraction_poor_delays * 100);
    Sleep(1000);
  }
}
