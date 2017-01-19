/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>

#include <numeric>
#include <vector>

#include "webrtc/voice_engine/test/auto_test/fixtures/after_streaming_fixture.h"

#ifdef WEBRTC_IOS
  const int kMinimumReasonableDelayEstimateMs = 30;
#else
  const int kMinimumReasonableDelayEstimateMs = 45;
#endif  // !WEBRTC_IOS

class VideoSyncTest : public AfterStreamingFixture {
 protected:
  // This test will verify that delay estimates converge (e.g. the standard
  // deviation for the last five seconds' estimates is less than 20) without
  // manual observation. The test runs for 15 seconds, sampling once per second.
  // All samples are checked so they are greater than |min_estimate|.
  int CollectEstimatesDuring15Seconds(int min_estimate) {
    Sleep(1000);

    std::vector<int> all_delay_estimates;
    for (int second = 0; second < 15; second++) {
      int jitter_buffer_delay_ms = 0;
      int playout_buffer_delay_ms = 0;
      EXPECT_EQ(0, voe_vsync_->GetDelayEstimate(channel_,
                                                &jitter_buffer_delay_ms,
                                                &playout_buffer_delay_ms));

      EXPECT_GT(jitter_buffer_delay_ms, min_estimate) <<
          "The delay estimate can not conceivably get lower than " <<
          min_estimate << " ms, it's unrealistic.";

      all_delay_estimates.push_back(jitter_buffer_delay_ms);
      Sleep(1000);
    }

    return ComputeStandardDeviation(
        all_delay_estimates.begin() + 10, all_delay_estimates.end());
  }

  void CheckEstimatesConvergeReasonablyWell(int min_estimate) {
    float standard_deviation = CollectEstimatesDuring15Seconds(min_estimate);
    EXPECT_LT(standard_deviation, 30.0f);
  }

  // Computes the standard deviation by first estimating the sample variance
  // with an unbiased estimator.
  float ComputeStandardDeviation(std::vector<int>::const_iterator start,
                               std::vector<int>::const_iterator end) const {
    int num_elements = end - start;
    int mean = std::accumulate(start, end, 0) / num_elements;
    assert(num_elements > 1);

    float variance = 0;
    for (; start != end; ++start) {
      variance += (*start - mean) * (*start - mean) / (num_elements - 1);
    }
    return sqrt(variance);
  }
};

TEST_F(VideoSyncTest,
       CanNotGetPlayoutTimestampWhilePlayingWithoutSettingItFirst) {
  unsigned int ignored;
  EXPECT_EQ(-1, voe_vsync_->GetPlayoutTimestamp(channel_, ignored));
}

TEST_F(VideoSyncTest, CannotSetInitTimestampWhilePlaying) {
  EXPECT_EQ(-1, voe_vsync_->SetInitTimestamp(channel_, 12345));
}

TEST_F(VideoSyncTest, CannotSetInitSequenceNumberWhilePlaying) {
  EXPECT_EQ(-1, voe_vsync_->SetInitSequenceNumber(channel_, 123));
}

TEST_F(VideoSyncTest, CanSetInitTimestampWhileStopped) {
  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  EXPECT_EQ(0, voe_vsync_->SetInitTimestamp(channel_, 12345));
}

TEST_F(VideoSyncTest, CanSetInitSequenceNumberWhileStopped) {
  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  EXPECT_EQ(0, voe_vsync_->SetInitSequenceNumber(channel_, 123));
}

// TODO(phoglund): pending investigation in
// http://code.google.com/p/webrtc/issues/detail?id=438
TEST_F(VideoSyncTest,
       DISABLED_DelayEstimatesStabilizeDuring15sAndAreNotTooLow) {
  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  EXPECT_EQ(0, voe_vsync_->SetInitTimestamp(channel_, 12345));
  EXPECT_EQ(0, voe_vsync_->SetInitSequenceNumber(channel_, 123));
  EXPECT_EQ(0, voe_base_->StartSend(channel_));

  CheckEstimatesConvergeReasonablyWell(kMinimumReasonableDelayEstimateMs);
}

// TODO(phoglund): pending investigation in
// http://code.google.com/p/webrtc/issues/detail?id=438
TEST_F(VideoSyncTest,
       DISABLED_DelayEstimatesStabilizeAfterNetEqMinDelayChanges45s) {
  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  EXPECT_EQ(0, voe_vsync_->SetInitTimestamp(channel_, 12345));
  EXPECT_EQ(0, voe_vsync_->SetInitSequenceNumber(channel_, 123));
  EXPECT_EQ(0, voe_base_->StartSend(channel_));

  CheckEstimatesConvergeReasonablyWell(kMinimumReasonableDelayEstimateMs);
  EXPECT_EQ(0, voe_vsync_->SetMinimumPlayoutDelay(channel_, 200));
  CheckEstimatesConvergeReasonablyWell(kMinimumReasonableDelayEstimateMs);
  EXPECT_EQ(0, voe_vsync_->SetMinimumPlayoutDelay(channel_, 0));
  CheckEstimatesConvergeReasonablyWell(kMinimumReasonableDelayEstimateMs);
}

TEST_F(VideoSyncTest, CanGetPlayoutBufferSize) {
  int ignored;
  EXPECT_EQ(0, voe_vsync_->GetPlayoutBufferSize(ignored));
}
