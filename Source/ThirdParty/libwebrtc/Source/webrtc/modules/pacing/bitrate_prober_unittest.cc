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

#include "api/transport/field_trial_based_config.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/explicit_key_value_config.h"
#include "test/gtest.h"

namespace webrtc {

TEST(BitrateProberTest, VerifyStatesAndTimeBetweenProbes) {
  const FieldTrialBasedConfig config;
  BitrateProber prober(config);
  EXPECT_FALSE(prober.is_probing());

  Timestamp now = Timestamp::Zero();
  const Timestamp start_time = now;
  EXPECT_EQ(prober.NextProbeTime(now), Timestamp::PlusInfinity());

  const DataRate kTestBitrate1 = DataRate::KilobitsPerSec(900);
  const DataRate kTestBitrate2 = DataRate::KilobitsPerSec(1800);
  const int kClusterSize = 5;
  const DataSize kProbeSize = DataSize::Bytes(1000);
  const TimeDelta kMinProbeDuration = TimeDelta::Millis(15);

  prober.CreateProbeCluster({.at_time = now,
                             .target_data_rate = kTestBitrate1,
                             .target_duration = TimeDelta::Millis(15),
                             .min_probe_delta = TimeDelta::Millis(2),
                             .target_probe_count = 5,
                             .id = 0});
  prober.CreateProbeCluster({.at_time = now,
                             .target_data_rate = kTestBitrate2,
                             .target_duration = TimeDelta::Millis(15),
                             .min_probe_delta = TimeDelta::Millis(2),
                             .target_probe_count = 5,
                             .id = 1});
  EXPECT_FALSE(prober.is_probing());

  prober.OnIncomingPacket(kProbeSize);
  EXPECT_TRUE(prober.is_probing());
  EXPECT_EQ(0, prober.CurrentCluster(now)->probe_cluster_id);

  // First packet should probe as soon as possible.
  EXPECT_EQ(Timestamp::MinusInfinity(), prober.NextProbeTime(now));

  for (int i = 0; i < kClusterSize; ++i) {
    now = std::max(now, prober.NextProbeTime(now));
    EXPECT_EQ(now, std::max(now, prober.NextProbeTime(now)));
    EXPECT_EQ(0, prober.CurrentCluster(now)->probe_cluster_id);
    prober.ProbeSent(now, kProbeSize);
  }

  EXPECT_GE(now - start_time, kMinProbeDuration);
  // Verify that the actual bitrate is withing 10% of the target.
  DataRate bitrate = kProbeSize * (kClusterSize - 1) / (now - start_time);
  EXPECT_GT(bitrate, kTestBitrate1 * 0.9);
  EXPECT_LT(bitrate, kTestBitrate1 * 1.1);

  now = std::max(now, prober.NextProbeTime(now));
  Timestamp probe2_started = now;

  for (int i = 0; i < kClusterSize; ++i) {
    now = std::max(now, prober.NextProbeTime(now));
    EXPECT_EQ(now, std::max(now, prober.NextProbeTime(now)));
    EXPECT_EQ(1, prober.CurrentCluster(now)->probe_cluster_id);
    prober.ProbeSent(now, kProbeSize);
  }

  // Verify that the actual bitrate is withing 10% of the target.
  TimeDelta duration = now - probe2_started;
  EXPECT_GE(duration, kMinProbeDuration);
  bitrate = (kProbeSize * (kClusterSize - 1)) / duration;
  EXPECT_GT(bitrate, kTestBitrate2 * 0.9);
  EXPECT_LT(bitrate, kTestBitrate2 * 1.1);

  EXPECT_EQ(prober.NextProbeTime(now), Timestamp::PlusInfinity());
  EXPECT_FALSE(prober.is_probing());
}

TEST(BitrateProberTest, DoesntProbeWithoutRecentPackets) {
  const FieldTrialBasedConfig config;
  BitrateProber prober(config);
  const DataSize kProbeSize = DataSize::Bytes(1000);

  Timestamp now = Timestamp::Zero();
  EXPECT_EQ(prober.NextProbeTime(now), Timestamp::PlusInfinity());

  prober.CreateProbeCluster({.at_time = now,
                             .target_data_rate = DataRate::KilobitsPerSec(900),
                             .target_duration = TimeDelta::Millis(15),
                             .min_probe_delta = TimeDelta::Millis(2),
                             .target_probe_count = 5,
                             .id = 0});
  EXPECT_FALSE(prober.is_probing());

  prober.OnIncomingPacket(kProbeSize);
  EXPECT_TRUE(prober.is_probing());
  EXPECT_EQ(now, std::max(now, prober.NextProbeTime(now)));
  prober.ProbeSent(now, kProbeSize);
}

TEST(BitrateProberTest, DiscardsDelayedProbes) {
  const TimeDelta kMaxProbeDelay = TimeDelta::Millis(3);
  const test::ExplicitKeyValueConfig trials(
      "WebRTC-Bwe-ProbingBehavior/"
      "abort_delayed_probes:1,"
      "max_probe_delay:3ms/");
  BitrateProber prober(trials);
  const DataSize kProbeSize = DataSize::Bytes(1000);

  Timestamp now = Timestamp::Zero();

  // Add two probe clusters.
  prober.CreateProbeCluster({.at_time = now,
                             .target_data_rate = DataRate::KilobitsPerSec(900),
                             .target_duration = TimeDelta::Millis(15),
                             .min_probe_delta = TimeDelta::Millis(2),
                             .target_probe_count = 5,
                             .id = 0});

  prober.OnIncomingPacket(kProbeSize);
  EXPECT_TRUE(prober.is_probing());
  EXPECT_EQ(prober.CurrentCluster(now)->probe_cluster_id, 0);
  // Advance to first probe time and indicate sent probe.
  now = std::max(now, prober.NextProbeTime(now));
  prober.ProbeSent(now, kProbeSize);

  // Advance time 1ms past timeout for the next probe.
  Timestamp next_probe_time = prober.NextProbeTime(now);
  EXPECT_GT(next_probe_time, now);
  now += next_probe_time - now + kMaxProbeDelay + TimeDelta::Millis(1);

  // Still indicates the time we wanted to probe at.
  EXPECT_EQ(prober.NextProbeTime(now), next_probe_time);
  // First and only cluster removed due to timeout.
  EXPECT_FALSE(prober.CurrentCluster(now).has_value());
}

TEST(BitrateProberTest, LimitsNumberOfPendingProbeClusters) {
  const FieldTrialBasedConfig config;
  BitrateProber prober(config);
  const DataSize kProbeSize = DataSize::Bytes(1000);
  Timestamp now = Timestamp::Zero();
  prober.CreateProbeCluster({.at_time = now,
                             .target_data_rate = DataRate::KilobitsPerSec(900),
                             .target_duration = TimeDelta::Millis(15),
                             .min_probe_delta = TimeDelta::Millis(2),
                             .target_probe_count = 5,
                             .id = 0});
  prober.OnIncomingPacket(kProbeSize);
  ASSERT_TRUE(prober.is_probing());
  ASSERT_EQ(prober.CurrentCluster(now)->probe_cluster_id, 0);

  for (int i = 1; i < 11; ++i) {
    prober.CreateProbeCluster(
        {.at_time = now,
         .target_data_rate = DataRate::KilobitsPerSec(900),
         .target_duration = TimeDelta::Millis(15),
         .min_probe_delta = TimeDelta::Millis(2),
         .target_probe_count = 5,
         .id = i});
    prober.OnIncomingPacket(kProbeSize);
  }
  // Expect some clusters has been dropped.
  EXPECT_TRUE(prober.is_probing());
  EXPECT_GE(prober.CurrentCluster(now)->probe_cluster_id, 5);

  Timestamp max_expected_probe_time = now + TimeDelta::Seconds(1);
  while (prober.is_probing() && now < max_expected_probe_time) {
    now = std::max(now, prober.NextProbeTime(now));
    prober.ProbeSent(now, kProbeSize);
  }
  EXPECT_FALSE(prober.is_probing());
}

TEST(BitrateProberTest, DoesntInitializeProbingForSmallPackets) {
  const FieldTrialBasedConfig config;
  BitrateProber prober(config);
  prober.SetEnabled(true);
  ASSERT_FALSE(prober.is_probing());

  prober.CreateProbeCluster({.at_time = Timestamp::Zero(),
                             .target_data_rate = DataRate::KilobitsPerSec(1000),
                             .target_duration = TimeDelta::Millis(15),
                             .min_probe_delta = TimeDelta::Millis(2),
                             .target_probe_count = 5,
                             .id = 0});
  prober.OnIncomingPacket(DataSize::Bytes(100));

  EXPECT_FALSE(prober.is_probing());
}

TEST(BitrateProberTest, DoesInitializeProbingForSmallPacketsIfConfigured) {
  const test::ExplicitKeyValueConfig config(
      "WebRTC-Bwe-ProbingBehavior/"
      "min_packet_size:0bytes/");
  BitrateProber prober(config);
  prober.SetEnabled(true);
  ASSERT_FALSE(prober.is_probing());

  prober.CreateProbeCluster({.at_time = Timestamp::Zero(),
                             .target_data_rate = DataRate::KilobitsPerSec(1000),
                             .target_duration = TimeDelta::Millis(15),
                             .min_probe_delta = TimeDelta::Millis(2),
                             .target_probe_count = 5,
                             .id = 0});
  prober.OnIncomingPacket(DataSize::Bytes(10));

  EXPECT_TRUE(prober.is_probing());
}

TEST(BitrateProberTest, VerifyProbeSizeOnHighBitrate) {
  const FieldTrialBasedConfig config;
  BitrateProber prober(config);

  const DataRate kHighBitrate = DataRate::KilobitsPerSec(10000);  // 10 Mbps

  prober.CreateProbeCluster({.at_time = Timestamp::Zero(),
                             .target_data_rate = kHighBitrate,
                             .target_duration = TimeDelta::Millis(15),
                             .min_probe_delta = TimeDelta::Millis(2),
                             .target_probe_count = 5,
                             .id = 0});
  // Probe size should ensure a minimum of 1 ms interval.
  EXPECT_GT(prober.RecommendedMinProbeSize(),
            kHighBitrate * TimeDelta::Millis(1));
}

TEST(BitrateProberTest, ProbeSizeCanBeSetInProbeClusterConfig) {
  const FieldTrialBasedConfig config;
  BitrateProber prober(config);
  prober.SetEnabled(true);

  const DataRate kHighBitrate = DataRate::KilobitsPerSec(10000);  // 10 Mbps

  prober.CreateProbeCluster({.at_time = Timestamp::Zero(),
                             .target_data_rate = kHighBitrate,
                             .target_duration = TimeDelta::Millis(15),
                             .min_probe_delta = TimeDelta::Millis(20),
                             .target_probe_count = 5,
                             .id = 0});
  EXPECT_EQ(prober.RecommendedMinProbeSize(),
            kHighBitrate * TimeDelta::Millis(20));

  prober.OnIncomingPacket(DataSize::Bytes(1000));
  // Next time to send probe should be "min_probe_delta" if the recommended
  // number of bytes has been sent.
  prober.ProbeSent(Timestamp::Zero(), prober.RecommendedMinProbeSize());
  EXPECT_EQ(prober.NextProbeTime(Timestamp::Zero()),
            Timestamp::Zero() + TimeDelta::Millis(20));
}

TEST(BitrateProberTest, MinumumNumberOfProbingPackets) {
  const FieldTrialBasedConfig config;
  BitrateProber prober(config);
  // Even when probing at a low bitrate we expect a minimum number
  // of packets to be sent.
  const DataRate kBitrate = DataRate::KilobitsPerSec(100);
  const DataSize kPacketSize = DataSize::Bytes(1000);

  Timestamp now = Timestamp::Zero();
  prober.CreateProbeCluster({.at_time = Timestamp::Zero(),
                             .target_data_rate = kBitrate,
                             .target_duration = TimeDelta::Millis(15),
                             .min_probe_delta = TimeDelta::Millis(2),
                             .target_probe_count = 5,
                             .id = 0});

  prober.OnIncomingPacket(kPacketSize);
  for (int i = 0; i < 5; ++i) {
    EXPECT_TRUE(prober.is_probing());
    prober.ProbeSent(now, kPacketSize);
  }

  EXPECT_FALSE(prober.is_probing());
}

TEST(BitrateProberTest, ScaleBytesUsedForProbing) {
  const FieldTrialBasedConfig config;
  BitrateProber prober(config);
  const DataRate kBitrate = DataRate::KilobitsPerSec(10000);  // 10 Mbps.
  const DataSize kPacketSize = DataSize::Bytes(1000);
  const DataSize kExpectedDataSent = kBitrate * TimeDelta::Millis(15);

  Timestamp now = Timestamp::Zero();
  prober.CreateProbeCluster({.at_time = Timestamp::Zero(),
                             .target_data_rate = kBitrate,
                             .target_duration = TimeDelta::Millis(15),
                             .min_probe_delta = TimeDelta::Millis(2),
                             .target_probe_count = 5,
                             .id = 0});
  prober.OnIncomingPacket(kPacketSize);
  DataSize data_sent = DataSize::Zero();
  while (data_sent < kExpectedDataSent) {
    ASSERT_TRUE(prober.is_probing());
    prober.ProbeSent(now, kPacketSize);
    data_sent += kPacketSize;
  }

  EXPECT_FALSE(prober.is_probing());
}

TEST(BitrateProberTest, HighBitrateProbing) {
  const FieldTrialBasedConfig config;
  BitrateProber prober(config);
  const DataRate kBitrate = DataRate::KilobitsPerSec(1000000);  // 1 Gbps.
  const DataSize kPacketSize = DataSize::Bytes(1000);
  const DataSize kExpectedDataSent = kBitrate * TimeDelta::Millis(15);

  Timestamp now = Timestamp::Zero();
  prober.CreateProbeCluster({.at_time = Timestamp::Zero(),
                             .target_data_rate = kBitrate,
                             .target_duration = TimeDelta::Millis(15),
                             .min_probe_delta = TimeDelta::Millis(2),
                             .target_probe_count = 5,
                             .id = 0});
  prober.OnIncomingPacket(kPacketSize);
  DataSize data_sent = DataSize::Zero();
  while (data_sent < kExpectedDataSent) {
    ASSERT_TRUE(prober.is_probing());
    prober.ProbeSent(now, kPacketSize);
    data_sent += kPacketSize;
  }

  EXPECT_FALSE(prober.is_probing());
}

TEST(BitrateProberTest, ProbeClusterTimeout) {
  const FieldTrialBasedConfig config;
  BitrateProber prober(config);
  const DataRate kBitrate = DataRate::KilobitsPerSec(300);
  const DataSize kSmallPacketSize = DataSize::Bytes(20);
  // Expecting two probe clusters of 5 packets each.
  const DataSize kExpectedDataSent = kSmallPacketSize * 2 * 5;
  const TimeDelta kTimeout = TimeDelta::Millis(5000);

  Timestamp now = Timestamp::Zero();
  prober.CreateProbeCluster({.at_time = now,
                             .target_data_rate = kBitrate,
                             .target_duration = TimeDelta::Millis(15),
                             .min_probe_delta = TimeDelta::Millis(2),
                             .target_probe_count = 5,
                             .id = 0});
  prober.OnIncomingPacket(kSmallPacketSize);
  EXPECT_FALSE(prober.is_probing());
  now += kTimeout;
  prober.CreateProbeCluster({.at_time = now,
                             .target_data_rate = kBitrate / 10,
                             .target_duration = TimeDelta::Millis(15),
                             .min_probe_delta = TimeDelta::Millis(2),
                             .target_probe_count = 5,
                             .id = 1});
  prober.OnIncomingPacket(kSmallPacketSize);
  EXPECT_FALSE(prober.is_probing());
  now += TimeDelta::Millis(1);
  prober.CreateProbeCluster({.at_time = now,
                             .target_data_rate = kBitrate / 10,
                             .target_duration = TimeDelta::Millis(15),
                             .min_probe_delta = TimeDelta::Millis(2),
                             .target_probe_count = 5,
                             .id = 2});
  prober.OnIncomingPacket(kSmallPacketSize);
  EXPECT_TRUE(prober.is_probing());
  DataSize data_sent = DataSize::Zero();
  while (data_sent < kExpectedDataSent) {
    ASSERT_TRUE(prober.is_probing());
    prober.ProbeSent(now, kSmallPacketSize);
    data_sent += kSmallPacketSize;
  }

  EXPECT_FALSE(prober.is_probing());
}

TEST(BitrateProberTest, CanProbeImmediatelyIfConfigured) {
  const test::ExplicitKeyValueConfig trials(
      "WebRTC-Bwe-ProbingBehavior/min_packet_size:0/");

  BitrateProber prober(trials);
  prober.CreateProbeCluster({.at_time = Timestamp::Zero(),
                             .target_data_rate = DataRate::KilobitsPerSec(300),
                             .target_duration = TimeDelta::Millis(15),
                             .min_probe_delta = TimeDelta::Millis(2),
                             .target_probe_count = 5,
                             .id = 0});
  EXPECT_TRUE(prober.is_probing());
}

TEST(BitrateProberTest, CanProbeImmediatelyAgainAfterProbeIfConfigured) {
  const test::ExplicitKeyValueConfig trials(
      "WebRTC-Bwe-ProbingBehavior/min_packet_size:0/");

  BitrateProber prober(trials);
  ProbeClusterConfig cluster_config = {
      .at_time = Timestamp::Zero(),
      .target_data_rate = DataRate::KilobitsPerSec(300),
      .target_duration = TimeDelta::Millis(15),
      .min_probe_delta = TimeDelta::Millis(2),
      .target_probe_count = 1,
      .id = 0};
  prober.CreateProbeCluster(cluster_config);
  ASSERT_TRUE(prober.is_probing());
  (cluster_config.target_data_rate * cluster_config.target_duration).bytes();
  prober.ProbeSent(
      Timestamp::Zero() + TimeDelta::Millis(1),
      cluster_config.target_data_rate * cluster_config.target_duration);
  ASSERT_FALSE(prober.is_probing());

  cluster_config.id = 2;
  cluster_config.at_time = Timestamp::Zero() + TimeDelta::Millis(100);
  prober.CreateProbeCluster(cluster_config);
  EXPECT_TRUE(prober.is_probing());
}

}  // namespace webrtc
