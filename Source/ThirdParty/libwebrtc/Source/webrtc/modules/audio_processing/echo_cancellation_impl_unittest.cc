/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "modules/audio_processing/aec/aec_core.h"
#include "modules/audio_processing/echo_cancellation_impl.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/criticalsection.h"
#include "test/gtest.h"

namespace webrtc {
TEST(EchoCancellationInternalTest, ExtendedFilter) {
  EchoCancellationImpl echo_canceller;
  echo_canceller.Initialize(AudioProcessing::kSampleRate32kHz, 2, 2, 2);

  EXPECT_TRUE(echo_canceller.aec_core() == nullptr);

  echo_canceller.Enable(true);

  AecCore* aec_core = echo_canceller.aec_core();
  ASSERT_TRUE(aec_core != NULL);
  // Disabled by default.
  EXPECT_EQ(0, WebRtcAec_extended_filter_enabled(aec_core));

  Config config;
  config.Set<ExtendedFilter>(new ExtendedFilter(true));
  echo_canceller.SetExtraOptions(config);
  EXPECT_EQ(1, WebRtcAec_extended_filter_enabled(aec_core));

  // Retains setting after initialization.
  echo_canceller.Initialize(AudioProcessing::kSampleRate16kHz, 2, 2, 2);
  EXPECT_EQ(1, WebRtcAec_extended_filter_enabled(aec_core));

  config.Set<ExtendedFilter>(new ExtendedFilter(false));
  echo_canceller.SetExtraOptions(config);
  EXPECT_EQ(0, WebRtcAec_extended_filter_enabled(aec_core));

  // Retains setting after initialization.
  echo_canceller.Initialize(AudioProcessing::kSampleRate16kHz, 1, 1, 1);
  EXPECT_EQ(0, WebRtcAec_extended_filter_enabled(aec_core));
}

TEST(EchoCancellationInternalTest, DelayAgnostic) {
  EchoCancellationImpl echo_canceller;
  echo_canceller.Initialize(AudioProcessing::kSampleRate32kHz, 1, 1, 1);

  EXPECT_TRUE(echo_canceller.aec_core() == NULL);

  EXPECT_EQ(0, echo_canceller.Enable(true));
  EXPECT_TRUE(echo_canceller.is_enabled());

  AecCore* aec_core = echo_canceller.aec_core();
  ASSERT_TRUE(aec_core != NULL);
  // Enabled by default.
  EXPECT_EQ(0, WebRtcAec_delay_agnostic_enabled(aec_core));

  Config config;
  config.Set<DelayAgnostic>(new DelayAgnostic(true));
  echo_canceller.SetExtraOptions(config);
  EXPECT_EQ(1, WebRtcAec_delay_agnostic_enabled(aec_core));

  // Retains setting after initialization.
  echo_canceller.Initialize(AudioProcessing::kSampleRate32kHz, 2, 2, 2);
  EXPECT_EQ(1, WebRtcAec_delay_agnostic_enabled(aec_core));

  config.Set<DelayAgnostic>(new DelayAgnostic(false));
  echo_canceller.SetExtraOptions(config);
  EXPECT_EQ(0, WebRtcAec_delay_agnostic_enabled(aec_core));

  // Retains setting after initialization.
  echo_canceller.Initialize(AudioProcessing::kSampleRate16kHz, 2, 2, 2);
  EXPECT_EQ(0, WebRtcAec_delay_agnostic_enabled(aec_core));
}

TEST(EchoCancellationInternalTest, InterfaceConfiguration) {
  EchoCancellationImpl echo_canceller;
  echo_canceller.Initialize(AudioProcessing::kSampleRate16kHz, 1, 1, 1);

  EXPECT_EQ(0, echo_canceller.enable_drift_compensation(true));
  EXPECT_TRUE(echo_canceller.is_drift_compensation_enabled());
  EXPECT_EQ(0, echo_canceller.enable_drift_compensation(false));
  EXPECT_FALSE(echo_canceller.is_drift_compensation_enabled());

  EchoCancellationImpl::SuppressionLevel level[] = {
      EchoCancellationImpl::kLowSuppression,
      EchoCancellationImpl::kModerateSuppression,
      EchoCancellationImpl::kHighSuppression,
  };
  for (size_t i = 0; i < arraysize(level); i++) {
    EXPECT_EQ(0, echo_canceller.set_suppression_level(level[i]));
    EXPECT_EQ(level[i], echo_canceller.suppression_level());
  }

  EchoCancellationImpl::Metrics metrics;
  EXPECT_EQ(AudioProcessing::kNotEnabledError,
            echo_canceller.GetMetrics(&metrics));

  EXPECT_EQ(0, echo_canceller.Enable(true));
  EXPECT_TRUE(echo_canceller.is_enabled());

  EXPECT_EQ(0, echo_canceller.enable_metrics(true));
  EXPECT_TRUE(echo_canceller.are_metrics_enabled());
  EXPECT_EQ(0, echo_canceller.enable_metrics(false));
  EXPECT_FALSE(echo_canceller.are_metrics_enabled());

  EXPECT_EQ(0, echo_canceller.enable_delay_logging(true));
  EXPECT_TRUE(echo_canceller.is_delay_logging_enabled());
  EXPECT_EQ(0, echo_canceller.enable_delay_logging(false));
  EXPECT_FALSE(echo_canceller.is_delay_logging_enabled());

  EXPECT_EQ(0, echo_canceller.Enable(false));
  EXPECT_FALSE(echo_canceller.is_enabled());

  int median = 0;
  int std = 0;
  float poor_fraction = 0;
  EXPECT_EQ(AudioProcessing::kNotEnabledError,
            echo_canceller.GetDelayMetrics(&median, &std, &poor_fraction));

  EXPECT_EQ(0, echo_canceller.Enable(true));
  EXPECT_TRUE(echo_canceller.is_enabled());
  EXPECT_EQ(0, echo_canceller.Enable(false));
  EXPECT_FALSE(echo_canceller.is_enabled());

  EXPECT_EQ(0, echo_canceller.Enable(true));
  EXPECT_TRUE(echo_canceller.is_enabled());
  EXPECT_TRUE(echo_canceller.aec_core() != NULL);
  EXPECT_EQ(0, echo_canceller.Enable(false));
  EXPECT_FALSE(echo_canceller.is_enabled());
  EXPECT_FALSE(echo_canceller.aec_core() != NULL);
}

}  // namespace webrtc
