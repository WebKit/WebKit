/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/bitrate_prober.h"

#include <algorithm>

#include "test/gtest.h"

namespace webrtc {

TEST(BitrateProberTest, VerifyStatesAndTimeBetweenProbes) {
  const FieldTrialBasedConfig config;
  BitrateProber prober(config);
  EXPECT_FALSE(prober.IsProbing());

  Timestamp now = Timestamp::Millis(0);
  const Timestamp start_time = now;
  EXPECT_EQ(prober.NextProbeTime(now), Timestamp::PlusInfinity());

  const DataRate kTestBitrate1 = DataRate::KilobitsPerSec(900);
  const DataRate kTestBitrate2 = DataRate::KilobitsPerSec(1800);
  const int kClusterSize = 5;
  const int kProbeSize = 1000;
  const TimeDelta kMinProbeDuration = TimeDelta::Millis(15);

  prober.CreateProbeCluster(kTestBitrate1, now, 0);
  prober.CreateProbeCluster(kTestBitrate2, now, 1);
  EXPECT_FALSE(prober.IsProbing());

  prober.OnIncomingPacket(kProbeSize);
  EXPECT_TRUE(prober.IsProbing());
  EXPECT_EQ(0, prober.CurrentCluster().probe_cluster_id);

  // First packet should probe as soon as possible.
  EXPECT_EQ(Timestamp::MinusInfinity(), prober.NextProbeTime(now));

  for (int i = 0; i < kClusterSize; ++i) {
    now = std::max(now, prober.NextProbeTime(now));
    EXPECT_EQ(now, std::max(now, prober.NextProbeTime(now)));
    EXPECT_EQ(0, prober.CurrentCluster().probe_cluster_id);
    prober.ProbeSent(now, kProbeSize);
  }

  EXPECT_GE(now - start_time, kMinProbeDuration);
  // Verify that the actual bitrate is withing 10% of the target.
  DataRate bitrate =
      DataSize::Bytes(kProbeSize * (kClusterSize - 1)) / (now - start_time);
  EXPECT_GT(bitrate, kTestBitrate1 * 0.9);
  EXPECT_LT(bitrate, kTestBitrate1 * 1.1);

  now = std::max(now, prober.NextProbeTime(now));
  Timestamp probe2_started = now;

  for (int i = 0; i < kClusterSize; ++i) {
    now = std::max(now, prober.NextProbeTime(now));
    EXPECT_EQ(now, std::max(now, prober.NextProbeTime(now)));
    EXPECT_EQ(1, prober.CurrentCluster().probe_cluster_id);
    prober.ProbeSent(now, kProbeSize);
  }

  // Verify that the actual bitrate is withing 10% of the target.
  TimeDelta duration = now - probe2_started;
  EXPECT_GE(duration, kMinProbeDuration);
  bitrate = DataSize::Bytes(kProbeSize * (kClusterSize - 1)) / duration;
  EXPECT_GT(bitrate, kTestBitrate2 * 0.9);
  EXPECT_LT(bitrate, kTestBitrate2 * 1.1);

  EXPECT_EQ(prober.NextProbeTime(now), Timestamp::PlusInfinity());
  EXPECT_FALSE(prober.IsProbing());
}

TEST(BitrateProberTest, DoesntProbeWithoutRecentPackets) {
  const FieldTrialBasedConfig config;
  BitrateProber prober(config);

  Timestamp now = Timestamp::Zero();
  EXPECT_EQ(prober.NextProbeTime(now), Timestamp::PlusInfinity());

  prober.CreateProbeCluster(DataRate::KilobitsPerSec(900), now, 0);
  EXPECT_FALSE(prober.IsProbing());

  prober.OnIncomingPacket(1000);
  EXPECT_TRUE(prober.IsProbing());
  EXPECT_EQ(now, std::max(now, prober.NextProbeTime(now)));
  prober.ProbeSent(now, 1000);
  // Let time pass, no large enough packets put into prober.
  now += TimeDelta::Seconds(6);
  EXPECT_EQ(prober.NextProbeTime(now), Timestamp::PlusInfinity());
  // Check that legacy behaviour where prober is reset in TimeUntilNextProbe is
  // no longer there. Probes are no longer retried if they are timed out.
  prober.OnIncomingPacket(1000);
  EXPECT_EQ(prober.NextProbeTime(now), Timestamp::PlusInfinity());
}

TEST(BitrateProberTest, DoesntInitializeProbingForSmallPackets) {
  const FieldTrialBasedConfig config;
  BitrateProber prober(config);

  prober.SetEnabled(true);
  EXPECT_FALSE(prober.IsProbing());

  prober.OnIncomingPacket(100);
  EXPECT_FALSE(prober.IsProbing());
}

TEST(BitrateProberTest, VerifyProbeSizeOnHighBitrate) {
  const FieldTrialBasedConfig config;
  BitrateProber prober(config);

  const DataRate kHighBitrate = DataRate::KilobitsPerSec(10000);  // 10 Mbps

  prober.CreateProbeCluster(kHighBitrate, Timestamp::Millis(0),
                            /*cluster_id=*/0);
  // Probe size should ensure a minimum of 1 ms interval.
  EXPECT_GT(prober.RecommendedMinProbeSize(),
            (kHighBitrate * TimeDelta::Millis(1)).bytes<size_t>());
}

TEST(BitrateProberTest, MinumumNumberOfProbingPackets) {
  const FieldTrialBasedConfig config;
  BitrateProber prober(config);
  // Even when probing at a low bitrate we expect a minimum number
  // of packets to be sent.
  const DataRate kBitrate = DataRate::KilobitsPerSec(100);
  const int kPacketSizeBytes = 1000;

  Timestamp now = Timestamp::Millis(0);
  prober.CreateProbeCluster(kBitrate, now, 0);
  prober.OnIncomingPacket(kPacketSizeBytes);
  for (int i = 0; i < 5; ++i) {
    EXPECT_TRUE(prober.IsProbing());
    prober.ProbeSent(now, kPacketSizeBytes);
  }

  EXPECT_FALSE(prober.IsProbing());
}

TEST(BitrateProberTest, ScaleBytesUsedForProbing) {
  const FieldTrialBasedConfig config;
  BitrateProber prober(config);
  const DataRate kBitrate = DataRate::KilobitsPerSec(10000);  // 10 Mbps.
  const int kPacketSizeBytes = 1000;
  const int kExpectedBytesSent = (kBitrate * TimeDelta::Millis(15)).bytes();

  Timestamp now = Timestamp::Millis(0);
  prober.CreateProbeCluster(kBitrate, now, /*cluster_id=*/0);
  prober.OnIncomingPacket(kPacketSizeBytes);
  int bytes_sent = 0;
  while (bytes_sent < kExpectedBytesSent) {
    ASSERT_TRUE(prober.IsProbing());
    prober.ProbeSent(now, kPacketSizeBytes);
    bytes_sent += kPacketSizeBytes;
  }

  EXPECT_FALSE(prober.IsProbing());
}

TEST(BitrateProberTest, HighBitrateProbing) {
  const FieldTrialBasedConfig config;
  BitrateProber prober(config);
  const DataRate kBitrate = DataRate::KilobitsPerSec(1000000);  // 1 Gbps.
  const int kPacketSizeBytes = 1000;
  const int kExpectedBytesSent = (kBitrate * TimeDelta::Millis(15)).bytes();

  Timestamp now = Timestamp::Millis(0);
  prober.CreateProbeCluster(kBitrate, now, 0);
  prober.OnIncomingPacket(kPacketSizeBytes);
  int bytes_sent = 0;
  while (bytes_sent < kExpectedBytesSent) {
    ASSERT_TRUE(prober.IsProbing());
    prober.ProbeSent(now, kPacketSizeBytes);
    bytes_sent += kPacketSizeBytes;
  }

  EXPECT_FALSE(prober.IsProbing());
}

TEST(BitrateProberTest, ProbeClusterTimeout) {
  const FieldTrialBasedConfig config;
  BitrateProber prober(config);
  const DataRate kBitrate = DataRate::KilobitsPerSec(300);
  const int kSmallPacketSize = 20;
  // Expecting two probe clusters of 5 packets each.
  const int kExpectedBytesSent = 20 * 2 * 5;
  const TimeDelta kTimeout = TimeDelta::Millis(5000);

  Timestamp now = Timestamp::Millis(0);
  prober.CreateProbeCluster(kBitrate, now, /*cluster_id=*/0);
  prober.OnIncomingPacket(kSmallPacketSize);
  EXPECT_FALSE(prober.IsProbing());
  now += kTimeout;
  prober.CreateProbeCluster(kBitrate / 10, now, /*cluster_id=*/1);
  prober.OnIncomingPacket(kSmallPacketSize);
  EXPECT_FALSE(prober.IsProbing());
  now += TimeDelta::Millis(1);
  prober.CreateProbeCluster(kBitrate / 10, now, /*cluster_id=*/2);
  prober.OnIncomingPacket(kSmallPacketSize);
  EXPECT_TRUE(prober.IsProbing());
  int bytes_sent = 0;
  while (bytes_sent < kExpectedBytesSent) {
    ASSERT_TRUE(prober.IsProbing());
    prober.ProbeSent(now, kSmallPacketSize);
    bytes_sent += kSmallPacketSize;
  }

  EXPECT_FALSE(prober.IsProbing());
}
}  // namespace webrtc
