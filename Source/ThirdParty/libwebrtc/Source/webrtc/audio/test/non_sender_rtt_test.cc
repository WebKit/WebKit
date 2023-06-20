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
#include "rtc_base/gunit.h"
#include "rtc_base/task_queue_for_test.h"
#include "system_wrappers/include/sleep.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

using NonSenderRttTest = CallTest;

TEST_F(NonSenderRttTest, NonSenderRttStats) {
  class NonSenderRttTest : public AudioEndToEndTest {
   public:
    const int kLongTimeoutMs = 20000;
    const int64_t kRttMs = 30;

    explicit NonSenderRttTest(TaskQueueBase* task_queue)
        : task_queue_(task_queue) {}

    BuiltInNetworkBehaviorConfig GetSendTransportConfig() const override {
      BuiltInNetworkBehaviorConfig pipe_config;
      pipe_config.queue_delay_ms = kRttMs / 2;
      return pipe_config;
    }

    void ModifyAudioConfigs(AudioSendStream::Config* send_config,
                            std::vector<AudioReceiveStreamInterface::Config>*
                                receive_configs) override {
      ASSERT_EQ(receive_configs->size(), 1U);
      (*receive_configs)[0].enable_non_sender_rtt = true;
      AudioEndToEndTest::ModifyAudioConfigs(send_config, receive_configs);
      send_config->send_codec_spec->enable_non_sender_rtt = true;
    }

    void PerformTest() override {
      // Wait until we have an RTT measurement, but no longer than
      // `kLongTimeoutMs`. This usually takes around 5 seconds, but in rare
      // cases it can take more than 10 seconds.
      EXPECT_TRUE_WAIT(HasRoundTripTimeMeasurement(), kLongTimeoutMs);
    }

    void OnStreamsStopped() override {
      AudioReceiveStreamInterface::Stats recv_stats =
          receive_stream()->GetStats(/*get_and_clear_legacy_stats=*/true);
      EXPECT_GT(recv_stats.round_trip_time_measurements, 0);
      ASSERT_TRUE(recv_stats.round_trip_time.has_value());
      EXPECT_GT(recv_stats.round_trip_time->ms(), 0);
      EXPECT_GE(recv_stats.total_round_trip_time.ms(),
                recv_stats.round_trip_time->ms());
    }

   protected:
    bool HasRoundTripTimeMeasurement() {
      bool has_rtt = false;
      // GetStats() can only be called on `task_queue_`, block while we check.
      SendTask(task_queue_, [this, &has_rtt]() {
        if (receive_stream() &&
            receive_stream()->GetStats(true).round_trip_time_measurements > 0) {
          has_rtt = true;
        }
      });
      return has_rtt;
    }

   private:
    TaskQueueBase* task_queue_;
  } test(task_queue());

  RunBaseTest(&test);
}

}  // namespace test
}  // namespace webrtc
