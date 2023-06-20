/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/multi_channel_content_detector.h"

#include "system_wrappers/include/metrics.h"
#include "test/gtest.h"

namespace webrtc {

TEST(MultiChannelContentDetector, HandlingOfMono) {
  MultiChannelContentDetector mc(
      /*detect_stereo_content=*/true,
      /*num_render_input_channels=*/1,
      /*detection_threshold=*/0.0f,
      /*stereo_detection_timeout_threshold_seconds=*/0,
      /*stereo_detection_hysteresis_seconds=*/0.0f);
  EXPECT_FALSE(mc.IsProperMultiChannelContentDetected());
}

TEST(MultiChannelContentDetector, HandlingOfMonoAndDetectionOff) {
  MultiChannelContentDetector mc(
      /*detect_stereo_content=*/false,
      /*num_render_input_channels=*/1,
      /*detection_threshold=*/0.0f,
      /*stereo_detection_timeout_threshold_seconds=*/0,
      /*stereo_detection_hysteresis_seconds=*/0.0f);
  EXPECT_FALSE(mc.IsProperMultiChannelContentDetected());
}

TEST(MultiChannelContentDetector, HandlingOfDetectionOff) {
  MultiChannelContentDetector mc(
      /*detect_stereo_content=*/false,
      /*num_render_input_channels=*/2,
      /*detection_threshold=*/0.0f,
      /*stereo_detection_timeout_threshold_seconds=*/0,
      /*stereo_detection_hysteresis_seconds=*/0.0f);
  EXPECT_TRUE(mc.IsProperMultiChannelContentDetected());

  std::vector<std::vector<std::vector<float>>> frame(
      1, std::vector<std::vector<float>>(2, std::vector<float>(160, 0.0f)));
  std::fill(frame[0][0].begin(), frame[0][0].end(), 100.0f);
  std::fill(frame[0][1].begin(), frame[0][1].end(), 101.0f);

  EXPECT_FALSE(mc.UpdateDetection(frame));
  EXPECT_TRUE(mc.IsProperMultiChannelContentDetected());

  EXPECT_FALSE(mc.UpdateDetection(frame));
}

TEST(MultiChannelContentDetector, InitialDetectionOfStereo) {
  MultiChannelContentDetector mc(
      /*detect_stereo_content=*/true,
      /*num_render_input_channels=*/2,
      /*detection_threshold=*/0.0f,
      /*stereo_detection_timeout_threshold_seconds=*/0,
      /*stereo_detection_hysteresis_seconds=*/0.0f);
  EXPECT_FALSE(mc.IsProperMultiChannelContentDetected());
}

TEST(MultiChannelContentDetector, DetectionWhenFakeStereo) {
  MultiChannelContentDetector mc(
      /*detect_stereo_content=*/true,
      /*num_render_input_channels=*/2,
      /*detection_threshold=*/0.0f,
      /*stereo_detection_timeout_threshold_seconds=*/0,
      /*stereo_detection_hysteresis_seconds=*/0.0f);
  std::vector<std::vector<std::vector<float>>> frame(
      1, std::vector<std::vector<float>>(2, std::vector<float>(160, 0.0f)));
  std::fill(frame[0][0].begin(), frame[0][0].end(), 100.0f);
  std::fill(frame[0][1].begin(), frame[0][1].end(), 100.0f);
  EXPECT_FALSE(mc.UpdateDetection(frame));
  EXPECT_FALSE(mc.IsProperMultiChannelContentDetected());

  EXPECT_FALSE(mc.UpdateDetection(frame));
}

TEST(MultiChannelContentDetector, DetectionWhenStereo) {
  MultiChannelContentDetector mc(
      /*detect_stereo_content=*/true,
      /*num_render_input_channels=*/2,
      /*detection_threshold=*/0.0f,
      /*stereo_detection_timeout_threshold_seconds=*/0,
      /*stereo_detection_hysteresis_seconds=*/0.0f);
  std::vector<std::vector<std::vector<float>>> frame(
      1, std::vector<std::vector<float>>(2, std::vector<float>(160, 0.0f)));
  std::fill(frame[0][0].begin(), frame[0][0].end(), 100.0f);
  std::fill(frame[0][1].begin(), frame[0][1].end(), 101.0f);
  EXPECT_TRUE(mc.UpdateDetection(frame));
  EXPECT_TRUE(mc.IsProperMultiChannelContentDetected());

  EXPECT_FALSE(mc.UpdateDetection(frame));
}

TEST(MultiChannelContentDetector, DetectionWhenStereoAfterAWhile) {
  MultiChannelContentDetector mc(
      /*detect_stereo_content=*/true,
      /*num_render_input_channels=*/2,
      /*detection_threshold=*/0.0f,
      /*stereo_detection_timeout_threshold_seconds=*/0,
      /*stereo_detection_hysteresis_seconds=*/0.0f);
  std::vector<std::vector<std::vector<float>>> frame(
      1, std::vector<std::vector<float>>(2, std::vector<float>(160, 0.0f)));

  std::fill(frame[0][0].begin(), frame[0][0].end(), 100.0f);
  std::fill(frame[0][1].begin(), frame[0][1].end(), 100.0f);
  EXPECT_FALSE(mc.UpdateDetection(frame));
  EXPECT_FALSE(mc.IsProperMultiChannelContentDetected());

  EXPECT_FALSE(mc.UpdateDetection(frame));

  std::fill(frame[0][0].begin(), frame[0][0].end(), 100.0f);
  std::fill(frame[0][1].begin(), frame[0][1].end(), 101.0f);

  EXPECT_TRUE(mc.UpdateDetection(frame));
  EXPECT_TRUE(mc.IsProperMultiChannelContentDetected());

  EXPECT_FALSE(mc.UpdateDetection(frame));
}

TEST(MultiChannelContentDetector, DetectionWithStereoBelowThreshold) {
  constexpr float kThreshold = 1.0f;
  MultiChannelContentDetector mc(
      /*detect_stereo_content=*/true,
      /*num_render_input_channels=*/2,
      /*detection_threshold=*/kThreshold,
      /*stereo_detection_timeout_threshold_seconds=*/0,
      /*stereo_detection_hysteresis_seconds=*/0.0f);
  std::vector<std::vector<std::vector<float>>> frame(
      1, std::vector<std::vector<float>>(2, std::vector<float>(160, 0.0f)));
  std::fill(frame[0][0].begin(), frame[0][0].end(), 100.0f);
  std::fill(frame[0][1].begin(), frame[0][1].end(), 100.0f + kThreshold);

  EXPECT_FALSE(mc.UpdateDetection(frame));
  EXPECT_FALSE(mc.IsProperMultiChannelContentDetected());

  EXPECT_FALSE(mc.UpdateDetection(frame));
}

TEST(MultiChannelContentDetector, DetectionWithStereoAboveThreshold) {
  constexpr float kThreshold = 1.0f;
  MultiChannelContentDetector mc(
      /*detect_stereo_content=*/true,
      /*num_render_input_channels=*/2,
      /*detection_threshold=*/kThreshold,
      /*stereo_detection_timeout_threshold_seconds=*/0,
      /*stereo_detection_hysteresis_seconds=*/0.0f);
  std::vector<std::vector<std::vector<float>>> frame(
      1, std::vector<std::vector<float>>(2, std::vector<float>(160, 0.0f)));
  std::fill(frame[0][0].begin(), frame[0][0].end(), 100.0f);
  std::fill(frame[0][1].begin(), frame[0][1].end(), 100.0f + kThreshold + 0.1f);

  EXPECT_TRUE(mc.UpdateDetection(frame));
  EXPECT_TRUE(mc.IsProperMultiChannelContentDetected());

  EXPECT_FALSE(mc.UpdateDetection(frame));
}

class MultiChannelContentDetectorTimeoutBehavior
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<bool, int>> {};

INSTANTIATE_TEST_SUITE_P(MultiChannelContentDetector,
                         MultiChannelContentDetectorTimeoutBehavior,
                         ::testing::Combine(::testing::Values(false, true),
                                            ::testing::Values(0, 1, 10)));

TEST_P(MultiChannelContentDetectorTimeoutBehavior,
       TimeOutBehaviorForNonTrueStereo) {
  constexpr int kNumFramesPerSecond = 100;
  const bool detect_stereo_content = std::get<0>(GetParam());
  const int stereo_detection_timeout_threshold_seconds =
      std::get<1>(GetParam());
  const int stereo_detection_timeout_threshold_frames =
      stereo_detection_timeout_threshold_seconds * kNumFramesPerSecond;

  MultiChannelContentDetector mc(detect_stereo_content,
                                 /*num_render_input_channels=*/2,
                                 /*detection_threshold=*/0.0f,
                                 stereo_detection_timeout_threshold_seconds,
                                 /*stereo_detection_hysteresis_seconds=*/0.0f);
  std::vector<std::vector<std::vector<float>>> true_stereo_frame = {
      {std::vector<float>(160, 100.0f), std::vector<float>(160, 101.0f)}};

  std::vector<std::vector<std::vector<float>>> fake_stereo_frame = {
      {std::vector<float>(160, 100.0f), std::vector<float>(160, 100.0f)}};

  // Pass fake stereo frames and verify the content detection.
  for (int k = 0; k < 10; ++k) {
    EXPECT_FALSE(mc.UpdateDetection(fake_stereo_frame));
    if (detect_stereo_content) {
      EXPECT_FALSE(mc.IsProperMultiChannelContentDetected());
    } else {
      EXPECT_TRUE(mc.IsProperMultiChannelContentDetected());
    }
  }

  // Pass a true stereo frame and verify that it is properly detected.
  if (detect_stereo_content) {
    EXPECT_TRUE(mc.UpdateDetection(true_stereo_frame));
  } else {
    EXPECT_FALSE(mc.UpdateDetection(true_stereo_frame));
  }
  EXPECT_TRUE(mc.IsProperMultiChannelContentDetected());

  // Pass fake stereo frames until any timeouts are about to occur.
  for (int k = 0; k < stereo_detection_timeout_threshold_frames - 1; ++k) {
    EXPECT_FALSE(mc.UpdateDetection(fake_stereo_frame));
    EXPECT_TRUE(mc.IsProperMultiChannelContentDetected());
  }

  // Pass a fake stereo frame and verify that any timeouts properly occur.
  if (detect_stereo_content && stereo_detection_timeout_threshold_frames > 0) {
    EXPECT_TRUE(mc.UpdateDetection(fake_stereo_frame));
    EXPECT_FALSE(mc.IsProperMultiChannelContentDetected());
  } else {
    EXPECT_FALSE(mc.UpdateDetection(fake_stereo_frame));
    EXPECT_TRUE(mc.IsProperMultiChannelContentDetected());
  }

  // Pass fake stereo frames and verify the behavior after any timeout.
  for (int k = 0; k < 10; ++k) {
    EXPECT_FALSE(mc.UpdateDetection(fake_stereo_frame));
    if (detect_stereo_content &&
        stereo_detection_timeout_threshold_frames > 0) {
      EXPECT_FALSE(mc.IsProperMultiChannelContentDetected());
    } else {
      EXPECT_TRUE(mc.IsProperMultiChannelContentDetected());
    }
  }
}

class MultiChannelContentDetectorHysteresisBehavior
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<bool, float>> {};

INSTANTIATE_TEST_SUITE_P(
    MultiChannelContentDetector,
    MultiChannelContentDetectorHysteresisBehavior,
    ::testing::Combine(::testing::Values(false, true),
                       ::testing::Values(0.0f, 0.1f, 0.2f)));

TEST_P(MultiChannelContentDetectorHysteresisBehavior,
       PeriodBeforeStereoDetectionIsTriggered) {
  constexpr int kNumFramesPerSecond = 100;
  const bool detect_stereo_content = std::get<0>(GetParam());
  const int stereo_detection_hysteresis_seconds = std::get<1>(GetParam());
  const int stereo_detection_hysteresis_frames =
      stereo_detection_hysteresis_seconds * kNumFramesPerSecond;

  MultiChannelContentDetector mc(
      detect_stereo_content,
      /*num_render_input_channels=*/2,
      /*detection_threshold=*/0.0f,
      /*stereo_detection_timeout_threshold_seconds=*/0,
      stereo_detection_hysteresis_seconds);
  std::vector<std::vector<std::vector<float>>> true_stereo_frame = {
      {std::vector<float>(160, 100.0f), std::vector<float>(160, 101.0f)}};

  std::vector<std::vector<std::vector<float>>> fake_stereo_frame = {
      {std::vector<float>(160, 100.0f), std::vector<float>(160, 100.0f)}};

  // Pass fake stereo frames and verify the content detection.
  for (int k = 0; k < 10; ++k) {
    EXPECT_FALSE(mc.UpdateDetection(fake_stereo_frame));
    if (detect_stereo_content) {
      EXPECT_FALSE(mc.IsProperMultiChannelContentDetected());
    } else {
      EXPECT_TRUE(mc.IsProperMultiChannelContentDetected());
    }
    EXPECT_FALSE(mc.IsTemporaryMultiChannelContentDetected());
  }

  // Pass a two true stereo frames and verify that they are properly detected.
  ASSERT_TRUE(stereo_detection_hysteresis_frames > 2 ||
              stereo_detection_hysteresis_frames == 0);
  for (int k = 0; k < 2; ++k) {
    if (detect_stereo_content) {
      if (stereo_detection_hysteresis_seconds == 0.0f) {
        if (k == 0) {
          EXPECT_TRUE(mc.UpdateDetection(true_stereo_frame));
        } else {
          EXPECT_FALSE(mc.UpdateDetection(true_stereo_frame));
        }
        EXPECT_TRUE(mc.IsProperMultiChannelContentDetected());
        EXPECT_FALSE(mc.IsTemporaryMultiChannelContentDetected());
      } else {
        EXPECT_FALSE(mc.UpdateDetection(true_stereo_frame));
        EXPECT_FALSE(mc.IsProperMultiChannelContentDetected());
        EXPECT_TRUE(mc.IsTemporaryMultiChannelContentDetected());
      }
    } else {
      EXPECT_FALSE(mc.UpdateDetection(true_stereo_frame));
      EXPECT_TRUE(mc.IsProperMultiChannelContentDetected());
      EXPECT_FALSE(mc.IsTemporaryMultiChannelContentDetected());
    }
  }

  if (stereo_detection_hysteresis_seconds == 0.0f) {
    return;
  }

  // Pass true stereo frames until any timeouts are about to occur.
  for (int k = 0; k < stereo_detection_hysteresis_frames - 3; ++k) {
    if (detect_stereo_content) {
      EXPECT_FALSE(mc.UpdateDetection(true_stereo_frame));
      EXPECT_FALSE(mc.IsProperMultiChannelContentDetected());
      EXPECT_TRUE(mc.IsTemporaryMultiChannelContentDetected());
    } else {
      EXPECT_FALSE(mc.UpdateDetection(true_stereo_frame));
      EXPECT_TRUE(mc.IsProperMultiChannelContentDetected());
      EXPECT_FALSE(mc.IsTemporaryMultiChannelContentDetected());
    }
  }

  // Pass a true stereo frame and verify that it is properly detected.
  if (detect_stereo_content) {
    EXPECT_TRUE(mc.UpdateDetection(true_stereo_frame));
    EXPECT_TRUE(mc.IsProperMultiChannelContentDetected());
    EXPECT_FALSE(mc.IsTemporaryMultiChannelContentDetected());
  } else {
    EXPECT_FALSE(mc.UpdateDetection(true_stereo_frame));
    EXPECT_TRUE(mc.IsProperMultiChannelContentDetected());
    EXPECT_FALSE(mc.IsTemporaryMultiChannelContentDetected());
  }

  // Pass an additional true stereo frame and verify that it is properly
  // detected.
  if (detect_stereo_content) {
    EXPECT_FALSE(mc.UpdateDetection(true_stereo_frame));
    EXPECT_TRUE(mc.IsProperMultiChannelContentDetected());
    EXPECT_FALSE(mc.IsTemporaryMultiChannelContentDetected());
  } else {
    EXPECT_FALSE(mc.UpdateDetection(true_stereo_frame));
    EXPECT_TRUE(mc.IsProperMultiChannelContentDetected());
    EXPECT_FALSE(mc.IsTemporaryMultiChannelContentDetected());
  }

  // Pass a fake stereo frame and verify that it is properly detected.
  if (detect_stereo_content) {
    EXPECT_FALSE(mc.UpdateDetection(fake_stereo_frame));
    EXPECT_TRUE(mc.IsProperMultiChannelContentDetected());
    EXPECT_FALSE(mc.IsTemporaryMultiChannelContentDetected());
  } else {
    EXPECT_FALSE(mc.UpdateDetection(fake_stereo_frame));
    EXPECT_TRUE(mc.IsProperMultiChannelContentDetected());
    EXPECT_FALSE(mc.IsTemporaryMultiChannelContentDetected());
  }
}

class MultiChannelContentDetectorMetricsDisabled
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<bool, int>> {};

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    MultiChannelContentDetectorMetricsDisabled,
    ::testing::Values(std::tuple<bool, int>(false, 2),
                      std::tuple<bool, int>(true, 1)));

// Test that no metrics are logged when they are clearly uninteresting and would
// dilute relevant data: when the reference audio is single channel, or when
// dynamic detection is disabled.
TEST_P(MultiChannelContentDetectorMetricsDisabled, ReportsNoMetrics) {
  metrics::Reset();
  constexpr int kNumFramesPerSecond = 100;
  const bool detect_stereo_content = std::get<0>(GetParam());
  const int channel_count = std::get<1>(GetParam());
  std::vector<std::vector<std::vector<float>>> audio_frame = {
      std::vector<std::vector<float>>(channel_count,
                                      std::vector<float>(160, 100.0f))};
  {
    MultiChannelContentDetector mc(
        /*detect_stereo_content=*/detect_stereo_content,
        /*num_render_input_channels=*/channel_count,
        /*detection_threshold=*/0.0f,
        /*stereo_detection_timeout_threshold_seconds=*/1,
        /*stereo_detection_hysteresis_seconds=*/0.0f);
    for (int k = 0; k < 20 * kNumFramesPerSecond; ++k) {
      mc.UpdateDetection(audio_frame);
    }
  }
  EXPECT_METRIC_EQ(
      0, metrics::NumSamples("WebRTC.Audio.EchoCanceller."
                             "ProcessingPersistentMultichannelContent"));
  EXPECT_METRIC_EQ(
      0, metrics::NumSamples("WebRTC.Audio.EchoCanceller."
                             "PersistentMultichannelContentEverDetected"));
}

// Tests that after 3 seconds, no metrics are reported.
TEST(MultiChannelContentDetectorMetrics, ReportsNoMetricsForShortLifetime) {
  metrics::Reset();
  constexpr int kNumFramesPerSecond = 100;
  constexpr int kTooFewFramesToLogMetrics = 3 * kNumFramesPerSecond;
  std::vector<std::vector<std::vector<float>>> audio_frame = {
      std::vector<std::vector<float>>(2, std::vector<float>(160, 100.0f))};
  {
    MultiChannelContentDetector mc(
        /*detect_stereo_content=*/true,
        /*num_render_input_channels=*/2,
        /*detection_threshold=*/0.0f,
        /*stereo_detection_timeout_threshold_seconds=*/1,
        /*stereo_detection_hysteresis_seconds=*/0.0f);
    for (int k = 0; k < kTooFewFramesToLogMetrics; ++k) {
      mc.UpdateDetection(audio_frame);
    }
  }
  EXPECT_METRIC_EQ(
      0, metrics::NumSamples("WebRTC.Audio.EchoCanceller."
                             "ProcessingPersistentMultichannelContent"));
  EXPECT_METRIC_EQ(
      0, metrics::NumSamples("WebRTC.Audio.EchoCanceller."
                             "PersistentMultichannelContentEverDetected"));
}

// Tests that after 25 seconds, metrics are reported.
TEST(MultiChannelContentDetectorMetrics, ReportsMetrics) {
  metrics::Reset();
  constexpr int kNumFramesPerSecond = 100;
  std::vector<std::vector<std::vector<float>>> true_stereo_frame = {
      {std::vector<float>(160, 100.0f), std::vector<float>(160, 101.0f)}};
  std::vector<std::vector<std::vector<float>>> fake_stereo_frame = {
      {std::vector<float>(160, 100.0f), std::vector<float>(160, 100.0f)}};
  {
    MultiChannelContentDetector mc(
        /*detect_stereo_content=*/true,
        /*num_render_input_channels=*/2,
        /*detection_threshold=*/0.0f,
        /*stereo_detection_timeout_threshold_seconds=*/1,
        /*stereo_detection_hysteresis_seconds=*/0.0f);
    for (int k = 0; k < 10 * kNumFramesPerSecond; ++k) {
      mc.UpdateDetection(true_stereo_frame);
    }
    for (int k = 0; k < 15 * kNumFramesPerSecond; ++k) {
      mc.UpdateDetection(fake_stereo_frame);
    }
  }
  // After 10 seconds of true stereo and the remainder fake stereo, we expect
  // one lifetime metric sample (multichannel detected) and two periodic samples
  // (one multichannel, one mono).

  // Check lifetime metric.
  EXPECT_METRIC_EQ(
      1, metrics::NumSamples("WebRTC.Audio.EchoCanceller."
                             "PersistentMultichannelContentEverDetected"));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Audio.EchoCanceller."
                            "PersistentMultichannelContentEverDetected",
                            1));

  // Check periodic metric.
  EXPECT_METRIC_EQ(
      2, metrics::NumSamples("WebRTC.Audio.EchoCanceller."
                             "ProcessingPersistentMultichannelContent"));
  EXPECT_METRIC_EQ(1,
                   metrics::NumEvents("WebRTC.Audio.EchoCanceller."
                                      "ProcessingPersistentMultichannelContent",
                                      0));
  EXPECT_METRIC_EQ(1,
                   metrics::NumEvents("WebRTC.Audio.EchoCanceller."
                                      "ProcessingPersistentMultichannelContent",
                                      1));
}

}  // namespace webrtc
