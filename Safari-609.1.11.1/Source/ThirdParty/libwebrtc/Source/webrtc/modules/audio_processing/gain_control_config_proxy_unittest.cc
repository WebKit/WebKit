/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/gain_control_config_proxy.h"

#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/include/mock_audio_processing.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/ref_counted_object.h"
#include "test/gtest.h"

namespace webrtc {
class GainControlConfigProxyTest : public ::testing::Test {
 protected:
  GainControlConfigProxyTest()
      : apm_(new rtc::RefCountedObject<
             ::testing::StrictMock<test::MockAudioProcessing>>()),
        agc_(),
        proxy_(&lock_, apm_, &agc_) {
    EXPECT_CALL(*apm_, GetConfig())
        .WillRepeatedly(::testing::ReturnPointee(&apm_config_));
    EXPECT_CALL(*apm_, ApplyConfig(::testing::_))
        .WillRepeatedly(::testing::SaveArg<0>(&apm_config_));
  }

  GainControl* proxy() { return &proxy_; }

  rtc::scoped_refptr<testing::StrictMock<test::MockAudioProcessing>> apm_;
  ::testing::StrictMock<test::MockGainControl> agc_;
  AudioProcessing::Config apm_config_;

 private:
  rtc::CriticalSection lock_;
  GainControlConfigProxy proxy_;
};

// GainControl API during processing.
TEST_F(GainControlConfigProxyTest, SetStreamAnalogLevel) {
  EXPECT_CALL(*apm_, set_stream_analog_level(100));
  proxy()->set_stream_analog_level(100);
}

TEST_F(GainControlConfigProxyTest, StreamAnalogLevel) {
  EXPECT_CALL(*apm_, recommended_stream_analog_level())
      .WillOnce(::testing::Return(100));
  EXPECT_EQ(100, proxy()->stream_analog_level());
}

// GainControl config setters.
TEST_F(GainControlConfigProxyTest, SetEnable) {
  proxy()->Enable(true);
  EXPECT_TRUE(apm_config_.gain_controller1.enabled);

  proxy()->Enable(false);
  EXPECT_FALSE(apm_config_.gain_controller1.enabled);
}

TEST_F(GainControlConfigProxyTest, SetMode) {
  proxy()->set_mode(GainControl::Mode::kAdaptiveAnalog);
  EXPECT_EQ(apm_config_.gain_controller1.kAdaptiveAnalog,
            apm_config_.gain_controller1.mode);

  proxy()->set_mode(GainControl::Mode::kAdaptiveDigital);
  EXPECT_EQ(apm_config_.gain_controller1.kAdaptiveDigital,
            apm_config_.gain_controller1.mode);

  proxy()->set_mode(GainControl::Mode::kFixedDigital);
  EXPECT_EQ(apm_config_.gain_controller1.kFixedDigital,
            apm_config_.gain_controller1.mode);
}

TEST_F(GainControlConfigProxyTest, SetTargetLevelDbfs) {
  proxy()->set_target_level_dbfs(17);
  EXPECT_EQ(17, apm_config_.gain_controller1.target_level_dbfs);
}

TEST_F(GainControlConfigProxyTest, SetCompressionGainDb) {
  AudioProcessing::RuntimeSetting setting;
  EXPECT_CALL(*apm_, SetRuntimeSetting(::testing::_))
      .WillOnce(::testing::SaveArg<0>(&setting));
  proxy()->set_compression_gain_db(17);
  EXPECT_EQ(AudioProcessing::RuntimeSetting::Type::kCaptureCompressionGain,
            setting.type());
  float value;
  setting.GetFloat(&value);
  EXPECT_EQ(17, static_cast<int>(value + .5f));
}

TEST_F(GainControlConfigProxyTest, SetEnableLimiter) {
  proxy()->enable_limiter(true);
  EXPECT_TRUE(apm_config_.gain_controller1.enable_limiter);
  proxy()->enable_limiter(false);
  EXPECT_FALSE(apm_config_.gain_controller1.enable_limiter);
}

TEST_F(GainControlConfigProxyTest, SetAnalogLevelLimits) {
  proxy()->set_analog_level_limits(100, 300);
  EXPECT_EQ(100, apm_config_.gain_controller1.analog_level_minimum);
  EXPECT_EQ(300, apm_config_.gain_controller1.analog_level_maximum);
}

TEST_F(GainControlConfigProxyTest, GetEnabled) {
  EXPECT_CALL(agc_, is_enabled())
      .WillOnce(::testing::Return(true))
      .WillOnce(::testing::Return(false));
  EXPECT_TRUE(proxy()->is_enabled());
  EXPECT_FALSE(proxy()->is_enabled());
}

TEST_F(GainControlConfigProxyTest, GetLimiterEnabled) {
  EXPECT_CALL(agc_, is_enabled())
      .WillOnce(::testing::Return(true))
      .WillOnce(::testing::Return(false));
  EXPECT_TRUE(proxy()->is_enabled());
  EXPECT_FALSE(proxy()->is_enabled());
}

TEST_F(GainControlConfigProxyTest, GetCompressionGainDb) {
  EXPECT_CALL(agc_, compression_gain_db()).WillOnce(::testing::Return(17));
  EXPECT_EQ(17, proxy()->compression_gain_db());
}

TEST_F(GainControlConfigProxyTest, GetTargetLevelDbfs) {
  EXPECT_CALL(agc_, target_level_dbfs()).WillOnce(::testing::Return(17));
  EXPECT_EQ(17, proxy()->target_level_dbfs());
}

TEST_F(GainControlConfigProxyTest, GetAnalogLevelMinimum) {
  EXPECT_CALL(agc_, analog_level_minimum()).WillOnce(::testing::Return(17));
  EXPECT_EQ(17, proxy()->analog_level_minimum());
}

TEST_F(GainControlConfigProxyTest, GetAnalogLevelMaximum) {
  EXPECT_CALL(agc_, analog_level_maximum()).WillOnce(::testing::Return(17));
  EXPECT_EQ(17, proxy()->analog_level_maximum());
}

TEST_F(GainControlConfigProxyTest, GetStreamIsSaturated) {
  EXPECT_CALL(agc_, stream_is_saturated())
      .WillOnce(::testing::Return(true))
      .WillOnce(::testing::Return(false));
  EXPECT_TRUE(proxy()->stream_is_saturated());
  EXPECT_FALSE(proxy()->stream_is_saturated());
}

TEST_F(GainControlConfigProxyTest, GetMode) {
  EXPECT_CALL(agc_, mode())
      .WillOnce(::testing::Return(GainControl::Mode::kAdaptiveAnalog))
      .WillOnce(::testing::Return(GainControl::Mode::kAdaptiveDigital))
      .WillOnce(::testing::Return(GainControl::Mode::kFixedDigital));
  EXPECT_EQ(GainControl::Mode::kAdaptiveAnalog, proxy()->mode());
  EXPECT_EQ(GainControl::Mode::kAdaptiveDigital, proxy()->mode());
  EXPECT_EQ(GainControl::Mode::kFixedDigital, proxy()->mode());
}

}  // namespace webrtc
