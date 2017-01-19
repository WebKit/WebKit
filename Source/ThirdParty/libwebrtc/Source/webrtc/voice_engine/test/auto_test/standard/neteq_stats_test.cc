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

class NetEQStatsTest : public AfterStreamingFixture {
};

TEST_F(NetEQStatsTest, ManualPrintStatisticsAfterRunningAWhile) {
  Sleep(5000);

  webrtc::NetworkStatistics network_statistics;

  EXPECT_EQ(0, voe_neteq_stats_->GetNetworkStatistics(
      channel_, network_statistics));

  TEST_LOG("Inspect these statistics and ensure they make sense.\n");

  TEST_LOG("    currentAccelerateRate       = %hu \n",
      network_statistics.currentAccelerateRate);
  TEST_LOG("    currentBufferSize           = %hu \n",
      network_statistics.currentBufferSize);
  TEST_LOG("    currentSecondaryDecodedRate = %hu \n",
      network_statistics.currentSecondaryDecodedRate);
  TEST_LOG("    currentDiscardRate          = %hu \n",
      network_statistics.currentDiscardRate);
  TEST_LOG("    currentExpandRate           = %hu \n",
      network_statistics.currentExpandRate);
  TEST_LOG("    currentPacketLossRate       = %hu \n",
      network_statistics.currentPacketLossRate);
  TEST_LOG("    currentPreemptiveRate       = %hu \n",
      network_statistics.currentPreemptiveRate);
  TEST_LOG("    currentSpeechExpandRate     = %hu \n",
      network_statistics.currentSpeechExpandRate);
  TEST_LOG("    preferredBufferSize         = %hu \n",
      network_statistics.preferredBufferSize);
  TEST_LOG("    jitterPeaksFound            = %i \n",
      network_statistics.jitterPeaksFound);
  TEST_LOG("    clockDriftPPM               = %i \n",
      network_statistics.clockDriftPPM);
  TEST_LOG("    meanWaitingTimeMs           = %i \n",
      network_statistics.meanWaitingTimeMs);
  TEST_LOG("    medianWaitingTimeMs         = %i \n",
      network_statistics.medianWaitingTimeMs);
  TEST_LOG("    minWaitingTimeMs            = %i \n",
      network_statistics.minWaitingTimeMs);
  TEST_LOG("    maxWaitingTimeMs            = %i \n",
      network_statistics.maxWaitingTimeMs);

  // This is only set to a non-zero value in off-mode.
  EXPECT_EQ(0U, network_statistics.addedSamples);
}
