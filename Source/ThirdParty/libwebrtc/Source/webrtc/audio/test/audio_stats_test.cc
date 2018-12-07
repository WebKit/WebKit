/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/test/audio_end_to_end_test.h"
#include "rtc_base/numerics/safe_compare.h"
#include "system_wrappers/include/sleep.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {

bool IsNear(int reference, int v) {
  // Margin is 10%.
  const int error = reference / 10 + 1;
  return std::abs(reference - v) <= error;
}

class NoLossTest : public AudioEndToEndTest {
 public:
  const int kTestDurationMs = 8000;
  const int kBytesSent = 69351;
  const int32_t kPacketsSent = 400;
  const int64_t kRttMs = 100;

  NoLossTest() = default;

  BuiltInNetworkBehaviorConfig GetNetworkPipeConfig() const override {
    BuiltInNetworkBehaviorConfig pipe_config;
    pipe_config.queue_delay_ms = kRttMs / 2;
    return pipe_config;
  }

  void PerformTest() override {
    SleepMs(kTestDurationMs);
    send_audio_device()->StopRecording();
    AudioEndToEndTest::PerformTest();
  }

  void OnStreamsStopped() override {
    AudioSendStream::Stats send_stats = send_stream()->GetStats();
    EXPECT_PRED2(IsNear, kBytesSent, send_stats.bytes_sent);
    EXPECT_PRED2(IsNear, kPacketsSent, send_stats.packets_sent);
    EXPECT_EQ(0, send_stats.packets_lost);
    EXPECT_EQ(0.0f, send_stats.fraction_lost);
    EXPECT_EQ("opus", send_stats.codec_name);
    // send_stats.jitter_ms
    EXPECT_PRED2(IsNear, kRttMs, send_stats.rtt_ms);
    // Send level is 0 because it is cleared in TransmitMixer::StopSend().
    EXPECT_EQ(0, send_stats.audio_level);
    // send_stats.total_input_energy
    // send_stats.total_input_duration
    EXPECT_FALSE(send_stats.apm_statistics.delay_median_ms);
    EXPECT_FALSE(send_stats.apm_statistics.delay_standard_deviation_ms);
    EXPECT_FALSE(send_stats.apm_statistics.echo_return_loss);
    EXPECT_FALSE(send_stats.apm_statistics.echo_return_loss_enhancement);
    EXPECT_FALSE(send_stats.apm_statistics.residual_echo_likelihood);
    EXPECT_FALSE(send_stats.apm_statistics.residual_echo_likelihood_recent_max);
    EXPECT_EQ(false, send_stats.typing_noise_detected);

    AudioReceiveStream::Stats recv_stats = receive_stream()->GetStats();
    EXPECT_PRED2(IsNear, kBytesSent, recv_stats.bytes_rcvd);
    EXPECT_PRED2(IsNear, kPacketsSent, recv_stats.packets_rcvd);
    EXPECT_EQ(0u, recv_stats.packets_lost);
    EXPECT_EQ(0.0f, recv_stats.fraction_lost);
    EXPECT_EQ("opus", send_stats.codec_name);
    // recv_stats.jitter_ms
    // recv_stats.jitter_buffer_ms
    EXPECT_EQ(20u, recv_stats.jitter_buffer_preferred_ms);
    // recv_stats.delay_estimate_ms
    // Receive level is 0 because it is cleared in Channel::StopPlayout().
    EXPECT_EQ(0, recv_stats.audio_level);
    // recv_stats.total_output_energy
    // recv_stats.total_samples_received
    // recv_stats.total_output_duration
    // recv_stats.concealed_samples
    // recv_stats.expand_rate
    // recv_stats.speech_expand_rate
    EXPECT_EQ(0.0, recv_stats.secondary_decoded_rate);
    EXPECT_EQ(0.0, recv_stats.secondary_discarded_rate);
    EXPECT_EQ(0.0, recv_stats.accelerate_rate);
    EXPECT_EQ(0.0, recv_stats.preemptive_expand_rate);
    EXPECT_EQ(0, recv_stats.decoding_calls_to_silence_generator);
    // recv_stats.decoding_calls_to_neteq
    // recv_stats.decoding_normal
    // recv_stats.decoding_plc
    EXPECT_EQ(0, recv_stats.decoding_cng);
    // recv_stats.decoding_plc_cng
    // recv_stats.decoding_muted_output
    // Capture start time is -1 because we do not have an associated send stream
    // on the receiver side.
    EXPECT_EQ(-1, recv_stats.capture_start_ntp_time_ms);

    // Match these stats between caller and receiver.
    EXPECT_EQ(send_stats.local_ssrc, recv_stats.remote_ssrc);
    EXPECT_EQ(*send_stats.codec_payload_type, *recv_stats.codec_payload_type);
    EXPECT_TRUE(rtc::SafeEq(send_stats.ext_seqnum, recv_stats.ext_seqnum));
  }
};
}  // namespace

using AudioStatsTest = CallTest;

TEST_F(AudioStatsTest, DISABLED_NoLoss) {
  NoLossTest test;
  RunBaseTest(&test);
}

}  // namespace test
}  // namespace webrtc
