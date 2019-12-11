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

#include "api/scoped_refptr.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

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
}  // namespace webrtc
