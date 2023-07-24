/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/test/audio_end_to_end_test.h"
#include "system_wrappers/include/sleep.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

using NackTest = CallTest;

TEST_F(NackTest, ShouldNackInLossyNetwork) {
  class NackTest : public AudioEndToEndTest {
   public:
    const int kTestDurationMs = 2000;
    const int64_t kRttMs = 30;
    const int64_t kLossPercent = 30;
    const int kNackHistoryMs = 1000;

    BuiltInNetworkBehaviorConfig GetSendTransportConfig() const override {
      BuiltInNetworkBehaviorConfig pipe_config;
      pipe_config.queue_delay_ms = kRttMs / 2;
      pipe_config.loss_percent = kLossPercent;
      return pipe_config;
    }

    void ModifyAudioConfigs(AudioSendStream::Config* send_config,
                            std::vector<AudioReceiveStreamInterface::Config>*
                                receive_configs) override {
      ASSERT_EQ(receive_configs->size(), 1U);
      (*receive_configs)[0].rtp.nack.rtp_history_ms = kNackHistoryMs;
      AudioEndToEndTest::ModifyAudioConfigs(send_config, receive_configs);
    }

    void PerformTest() override { SleepMs(kTestDurationMs); }

    void OnStreamsStopped() override {
      AudioReceiveStreamInterface::Stats recv_stats =
          receive_stream()->GetStats(/*get_and_clear_legacy_stats=*/true);
      EXPECT_GT(recv_stats.nacks_sent, 0U);
      AudioSendStream::Stats send_stats = send_stream()->GetStats();
      EXPECT_GT(send_stats.retransmitted_packets_sent, 0U);
      EXPECT_GT(send_stats.nacks_received, 0U);
    }
  } test;

  RunBaseTest(&test);
}

}  // namespace test
}  // namespace webrtc
