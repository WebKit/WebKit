/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/stream_synchronization.h"

#include <algorithm>

#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/ntp_time.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
constexpr int kMaxChangeMs = 80;  // From stream_synchronization.cc
constexpr int kDefaultAudioFrequency = 8000;
constexpr int kDefaultVideoFrequency = 90000;
constexpr int kSmoothingFilter = 4 * 2;
}  // namespace

class StreamSynchronizationTest : public ::testing::Test {
 public:
  StreamSynchronizationTest()
      : sync_(0, 0), clock_sender_(98765000), clock_receiver_(43210000) {}

 protected:
  // Generates the necessary RTCP measurements and RTP timestamps and computes
  // the audio and video delays needed to get the two streams in sync.
  // |audio_delay_ms| and |video_delay_ms| are the number of milliseconds after
  // capture which the frames are received.
  // |current_audio_delay_ms| is the number of milliseconds which audio is
  // currently being delayed by the receiver.
  bool DelayedStreams(int audio_delay_ms,
                      int video_delay_ms,
                      int current_audio_delay_ms,
                      int* total_audio_delay_ms,
                      int* total_video_delay_ms) {
    int audio_frequency =
        static_cast<int>(kDefaultAudioFrequency * audio_clock_drift_ + 0.5);
    int video_frequency =
        static_cast<int>(kDefaultVideoFrequency * video_clock_drift_ + 0.5);

    // Generate NTP/RTP timestamp pair for both streams corresponding to RTCP.
    bool new_sr;
    StreamSynchronization::Measurements audio;
    StreamSynchronization::Measurements video;
    NtpTime ntp_time = clock_sender_.CurrentNtpTime();
    uint32_t rtp_timestamp =
        clock_sender_.CurrentTime().ms() * audio_frequency / 1000;
    EXPECT_TRUE(audio.rtp_to_ntp.UpdateMeasurements(
        ntp_time.seconds(), ntp_time.fractions(), rtp_timestamp, &new_sr));
    clock_sender_.AdvanceTimeMilliseconds(100);
    clock_receiver_.AdvanceTimeMilliseconds(100);
    ntp_time = clock_sender_.CurrentNtpTime();
    rtp_timestamp = clock_sender_.CurrentTime().ms() * video_frequency / 1000;
    EXPECT_TRUE(video.rtp_to_ntp.UpdateMeasurements(
        ntp_time.seconds(), ntp_time.fractions(), rtp_timestamp, &new_sr));
    clock_sender_.AdvanceTimeMilliseconds(900);
    clock_receiver_.AdvanceTimeMilliseconds(900);
    ntp_time = clock_sender_.CurrentNtpTime();
    rtp_timestamp = clock_sender_.CurrentTime().ms() * audio_frequency / 1000;
    EXPECT_TRUE(audio.rtp_to_ntp.UpdateMeasurements(
        ntp_time.seconds(), ntp_time.fractions(), rtp_timestamp, &new_sr));
    clock_sender_.AdvanceTimeMilliseconds(100);
    clock_receiver_.AdvanceTimeMilliseconds(100);
    ntp_time = clock_sender_.CurrentNtpTime();
    rtp_timestamp = clock_sender_.CurrentTime().ms() * video_frequency / 1000;
    EXPECT_TRUE(video.rtp_to_ntp.UpdateMeasurements(
        ntp_time.seconds(), ntp_time.fractions(), rtp_timestamp, &new_sr));
    clock_sender_.AdvanceTimeMilliseconds(900);
    clock_receiver_.AdvanceTimeMilliseconds(900);

    // Capture an audio and a video frame at the same time.
    audio.latest_timestamp =
        clock_sender_.CurrentTime().ms() * audio_frequency / 1000;
    video.latest_timestamp =
        clock_sender_.CurrentTime().ms() * video_frequency / 1000;

    if (audio_delay_ms > video_delay_ms) {
      // Audio later than video.
      clock_receiver_.AdvanceTimeMilliseconds(video_delay_ms);
      video.latest_receive_time_ms = clock_receiver_.CurrentTime().ms();
      clock_receiver_.AdvanceTimeMilliseconds(audio_delay_ms - video_delay_ms);
      audio.latest_receive_time_ms = clock_receiver_.CurrentTime().ms();
    } else {
      // Video later than audio.
      clock_receiver_.AdvanceTimeMilliseconds(audio_delay_ms);
      audio.latest_receive_time_ms = clock_receiver_.CurrentTime().ms();
      clock_receiver_.AdvanceTimeMilliseconds(video_delay_ms - audio_delay_ms);
      video.latest_receive_time_ms = clock_receiver_.CurrentTime().ms();
    }

    int relative_delay_ms;
    EXPECT_TRUE(StreamSynchronization::ComputeRelativeDelay(
        audio, video, &relative_delay_ms));
    EXPECT_EQ(video_delay_ms - audio_delay_ms, relative_delay_ms);

    return sync_.ComputeDelays(relative_delay_ms, current_audio_delay_ms,
                               total_audio_delay_ms, total_video_delay_ms);
  }

  // Simulate audio playback 300 ms after capture and video rendering 100 ms
  // after capture. Verify that the correct extra delays are calculated for
  // audio and video, and that they change correctly when we simulate that
  // NetEQ or the VCM adds more delay to the streams.
  void BothDelayedAudioLaterTest(int base_target_delay_ms) {
    const int kAudioDelayMs = base_target_delay_ms + 300;
    const int kVideoDelayMs = base_target_delay_ms + 100;
    int current_audio_delay_ms = base_target_delay_ms;
    int total_audio_delay_ms = 0;
    int total_video_delay_ms = base_target_delay_ms;
    int filtered_move = (kAudioDelayMs - kVideoDelayMs) / kSmoothingFilter;

    EXPECT_TRUE(DelayedStreams(kAudioDelayMs, kVideoDelayMs,
                               current_audio_delay_ms, &total_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay_ms + filtered_move, total_video_delay_ms);
    EXPECT_EQ(base_target_delay_ms, total_audio_delay_ms);

    // Set new current delay.
    current_audio_delay_ms = total_audio_delay_ms;
    clock_sender_.AdvanceTimeMilliseconds(1000);
    clock_receiver_.AdvanceTimeMilliseconds(
        1000 - std::max(kAudioDelayMs, kVideoDelayMs));
    // Simulate base_target_delay_ms minimum delay in the VCM.
    total_video_delay_ms = base_target_delay_ms;
    EXPECT_TRUE(DelayedStreams(kAudioDelayMs, kVideoDelayMs,
                               current_audio_delay_ms, &total_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay_ms + 2 * filtered_move, total_video_delay_ms);
    EXPECT_EQ(base_target_delay_ms, total_audio_delay_ms);

    // Set new current delay.
    current_audio_delay_ms = total_audio_delay_ms;
    clock_sender_.AdvanceTimeMilliseconds(1000);
    clock_receiver_.AdvanceTimeMilliseconds(
        1000 - std::max(kAudioDelayMs, kVideoDelayMs));
    // Simulate base_target_delay_ms minimum delay in the VCM.
    total_video_delay_ms = base_target_delay_ms;
    EXPECT_TRUE(DelayedStreams(kAudioDelayMs, kVideoDelayMs,
                               current_audio_delay_ms, &total_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay_ms + 3 * filtered_move, total_video_delay_ms);
    EXPECT_EQ(base_target_delay_ms, total_audio_delay_ms);

    // Simulate that NetEQ introduces some audio delay.
    const int kNeteqDelayIncrease = 50;
    current_audio_delay_ms = base_target_delay_ms + kNeteqDelayIncrease;
    clock_sender_.AdvanceTimeMilliseconds(1000);
    clock_receiver_.AdvanceTimeMilliseconds(
        1000 - std::max(kAudioDelayMs, kVideoDelayMs));
    // Simulate base_target_delay_ms minimum delay in the VCM.
    total_video_delay_ms = base_target_delay_ms;
    EXPECT_TRUE(DelayedStreams(kAudioDelayMs, kVideoDelayMs,
                               current_audio_delay_ms, &total_audio_delay_ms,
                               &total_video_delay_ms));
    filtered_move = 3 * filtered_move +
                    (kNeteqDelayIncrease + kAudioDelayMs - kVideoDelayMs) /
                        kSmoothingFilter;
    EXPECT_EQ(base_target_delay_ms + filtered_move, total_video_delay_ms);
    EXPECT_EQ(base_target_delay_ms, total_audio_delay_ms);

    // Simulate that NetEQ reduces its delay.
    const int kNeteqDelayDecrease = 10;
    current_audio_delay_ms = base_target_delay_ms + kNeteqDelayDecrease;
    clock_sender_.AdvanceTimeMilliseconds(1000);
    clock_receiver_.AdvanceTimeMilliseconds(
        1000 - std::max(kAudioDelayMs, kVideoDelayMs));
    // Simulate base_target_delay_ms minimum delay in the VCM.
    total_video_delay_ms = base_target_delay_ms;
    EXPECT_TRUE(DelayedStreams(kAudioDelayMs, kVideoDelayMs,
                               current_audio_delay_ms, &total_audio_delay_ms,
                               &total_video_delay_ms));
    filtered_move =
        filtered_move + (kNeteqDelayDecrease + kAudioDelayMs - kVideoDelayMs) /
                            kSmoothingFilter;
    EXPECT_EQ(base_target_delay_ms + filtered_move, total_video_delay_ms);
    EXPECT_EQ(base_target_delay_ms, total_audio_delay_ms);
  }

  void BothDelayedVideoLaterTest(int base_target_delay_ms) {
    const int kAudioDelayMs = base_target_delay_ms + 100;
    const int kVideoDelayMs = base_target_delay_ms + 300;
    int current_audio_delay_ms = base_target_delay_ms;
    int total_audio_delay_ms = 0;
    int total_video_delay_ms = base_target_delay_ms;

    EXPECT_TRUE(DelayedStreams(kAudioDelayMs, kVideoDelayMs,
                               current_audio_delay_ms, &total_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay_ms, total_video_delay_ms);
    // The audio delay is not allowed to change more than this.
    EXPECT_GE(base_target_delay_ms + kMaxChangeMs, total_audio_delay_ms);
    int last_total_audio_delay_ms = total_audio_delay_ms;

    // Set new current audio delay.
    current_audio_delay_ms = total_audio_delay_ms;
    clock_sender_.AdvanceTimeMilliseconds(1000);
    clock_receiver_.AdvanceTimeMilliseconds(800);
    EXPECT_TRUE(DelayedStreams(kAudioDelayMs, kVideoDelayMs,
                               current_audio_delay_ms, &total_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay_ms, total_video_delay_ms);
    EXPECT_EQ(last_total_audio_delay_ms +
                  MaxAudioDelayChangeMs(
                      current_audio_delay_ms,
                      base_target_delay_ms + kVideoDelayMs - kAudioDelayMs),
              total_audio_delay_ms);
    last_total_audio_delay_ms = total_audio_delay_ms;

    // Set new current audio delay.
    current_audio_delay_ms = total_audio_delay_ms;
    clock_sender_.AdvanceTimeMilliseconds(1000);
    clock_receiver_.AdvanceTimeMilliseconds(800);
    EXPECT_TRUE(DelayedStreams(kAudioDelayMs, kVideoDelayMs,
                               current_audio_delay_ms, &total_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay_ms, total_video_delay_ms);
    EXPECT_EQ(last_total_audio_delay_ms +
                  MaxAudioDelayChangeMs(
                      current_audio_delay_ms,
                      base_target_delay_ms + kVideoDelayMs - kAudioDelayMs),
              total_audio_delay_ms);
    last_total_audio_delay_ms = total_audio_delay_ms;

    // Simulate that NetEQ for some reason reduced the delay.
    current_audio_delay_ms = base_target_delay_ms + 10;
    clock_sender_.AdvanceTimeMilliseconds(1000);
    clock_receiver_.AdvanceTimeMilliseconds(800);
    EXPECT_TRUE(DelayedStreams(kAudioDelayMs, kVideoDelayMs,
                               current_audio_delay_ms, &total_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay_ms, total_video_delay_ms);
    EXPECT_EQ(last_total_audio_delay_ms +
                  MaxAudioDelayChangeMs(
                      current_audio_delay_ms,
                      base_target_delay_ms + kVideoDelayMs - kAudioDelayMs),
              total_audio_delay_ms);
    last_total_audio_delay_ms = total_audio_delay_ms;

    // Simulate that NetEQ for some reason significantly increased the delay.
    current_audio_delay_ms = base_target_delay_ms + 350;
    clock_sender_.AdvanceTimeMilliseconds(1000);
    clock_receiver_.AdvanceTimeMilliseconds(800);
    EXPECT_TRUE(DelayedStreams(kAudioDelayMs, kVideoDelayMs,
                               current_audio_delay_ms, &total_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay_ms, total_video_delay_ms);
    EXPECT_EQ(last_total_audio_delay_ms +
                  MaxAudioDelayChangeMs(
                      current_audio_delay_ms,
                      base_target_delay_ms + kVideoDelayMs - kAudioDelayMs),
              total_audio_delay_ms);
  }

  int MaxAudioDelayChangeMs(int current_audio_delay_ms, int delay_ms) const {
    int diff_ms = (delay_ms - current_audio_delay_ms) / kSmoothingFilter;
    diff_ms = std::min(diff_ms, kMaxChangeMs);
    diff_ms = std::max(diff_ms, -kMaxChangeMs);
    return diff_ms;
  }

  StreamSynchronization sync_;
  SimulatedClock clock_sender_;
  SimulatedClock clock_receiver_;
  double audio_clock_drift_ = 1.0;
  double video_clock_drift_ = 1.0;
};

TEST_F(StreamSynchronizationTest, NoDelay) {
  int total_audio_delay_ms = 0;
  int total_video_delay_ms = 0;

  EXPECT_FALSE(DelayedStreams(/*audio_delay_ms=*/0, /*video_delay_ms=*/0,
                              /*current_audio_delay_ms=*/0,
                              &total_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, total_audio_delay_ms);
  EXPECT_EQ(0, total_video_delay_ms);
}

TEST_F(StreamSynchronizationTest, VideoDelayed) {
  const int kAudioDelayMs = 200;
  int total_audio_delay_ms = 0;
  int total_video_delay_ms = 0;

  EXPECT_TRUE(DelayedStreams(kAudioDelayMs, /*video_delay_ms=*/0,
                             /*current_audio_delay_ms=*/0,
                             &total_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, total_audio_delay_ms);
  // The delay is not allowed to change more than this.
  EXPECT_EQ(kAudioDelayMs / kSmoothingFilter, total_video_delay_ms);

  // Simulate 0 minimum delay in the VCM.
  total_video_delay_ms = 0;
  clock_sender_.AdvanceTimeMilliseconds(1000);
  clock_receiver_.AdvanceTimeMilliseconds(800);
  EXPECT_TRUE(DelayedStreams(kAudioDelayMs, /*video_delay_ms=*/0,
                             /*current_audio_delay_ms=*/0,
                             &total_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, total_audio_delay_ms);
  EXPECT_EQ(2 * kAudioDelayMs / kSmoothingFilter, total_video_delay_ms);

  // Simulate 0 minimum delay in the VCM.
  total_video_delay_ms = 0;
  clock_sender_.AdvanceTimeMilliseconds(1000);
  clock_receiver_.AdvanceTimeMilliseconds(800);
  EXPECT_TRUE(DelayedStreams(kAudioDelayMs, /*video_delay_ms=*/0,
                             /*current_audio_delay_ms=*/0,
                             &total_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, total_audio_delay_ms);
  EXPECT_EQ(3 * kAudioDelayMs / kSmoothingFilter, total_video_delay_ms);
}

TEST_F(StreamSynchronizationTest, AudioDelayed) {
  const int kVideoDelayMs = 200;
  int current_audio_delay_ms = 0;
  int total_audio_delay_ms = 0;
  int total_video_delay_ms = 0;

  EXPECT_TRUE(DelayedStreams(/*audio_delay_ms=*/0, kVideoDelayMs,
                             current_audio_delay_ms, &total_audio_delay_ms,
                             &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // The delay is not allowed to change more than this.
  EXPECT_EQ(kVideoDelayMs / kSmoothingFilter, total_audio_delay_ms);
  int last_total_audio_delay_ms = total_audio_delay_ms;

  // Set new current audio delay.
  current_audio_delay_ms = total_audio_delay_ms;
  clock_sender_.AdvanceTimeMilliseconds(1000);
  clock_receiver_.AdvanceTimeMilliseconds(800);
  EXPECT_TRUE(DelayedStreams(/*audio_delay_ms=*/0, kVideoDelayMs,
                             current_audio_delay_ms, &total_audio_delay_ms,
                             &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  EXPECT_EQ(last_total_audio_delay_ms +
                MaxAudioDelayChangeMs(current_audio_delay_ms, kVideoDelayMs),
            total_audio_delay_ms);
  last_total_audio_delay_ms = total_audio_delay_ms;

  // Set new current audio delay.
  current_audio_delay_ms = total_audio_delay_ms;
  clock_sender_.AdvanceTimeMilliseconds(1000);
  clock_receiver_.AdvanceTimeMilliseconds(800);
  EXPECT_TRUE(DelayedStreams(/*audio_delay_ms=*/0, kVideoDelayMs,
                             current_audio_delay_ms, &total_audio_delay_ms,
                             &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  EXPECT_EQ(last_total_audio_delay_ms +
                MaxAudioDelayChangeMs(current_audio_delay_ms, kVideoDelayMs),
            total_audio_delay_ms);
  last_total_audio_delay_ms = total_audio_delay_ms;

  // Simulate that NetEQ for some reason reduced the delay.
  current_audio_delay_ms = 10;
  clock_sender_.AdvanceTimeMilliseconds(1000);
  clock_receiver_.AdvanceTimeMilliseconds(800);
  EXPECT_TRUE(DelayedStreams(/*audio_delay_ms=*/0, kVideoDelayMs,
                             current_audio_delay_ms, &total_audio_delay_ms,
                             &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  EXPECT_EQ(last_total_audio_delay_ms +
                MaxAudioDelayChangeMs(current_audio_delay_ms, kVideoDelayMs),
            total_audio_delay_ms);
  last_total_audio_delay_ms = total_audio_delay_ms;

  // Simulate that NetEQ for some reason significantly increased the delay.
  current_audio_delay_ms = 350;
  clock_sender_.AdvanceTimeMilliseconds(1000);
  clock_receiver_.AdvanceTimeMilliseconds(800);
  EXPECT_TRUE(DelayedStreams(/*audio_delay_ms=*/0, kVideoDelayMs,
                             current_audio_delay_ms, &total_audio_delay_ms,
                             &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  EXPECT_EQ(last_total_audio_delay_ms +
                MaxAudioDelayChangeMs(current_audio_delay_ms, kVideoDelayMs),
            total_audio_delay_ms);
}

TEST_F(StreamSynchronizationTest, BothDelayedVideoLater) {
  BothDelayedVideoLaterTest(0);
}

TEST_F(StreamSynchronizationTest, BothDelayedVideoLaterAudioClockDrift) {
  audio_clock_drift_ = 1.05;
  BothDelayedVideoLaterTest(0);
}

TEST_F(StreamSynchronizationTest, BothDelayedVideoLaterVideoClockDrift) {
  video_clock_drift_ = 1.05;
  BothDelayedVideoLaterTest(0);
}

TEST_F(StreamSynchronizationTest, BothDelayedAudioLater) {
  BothDelayedAudioLaterTest(0);
}

TEST_F(StreamSynchronizationTest, BothDelayedAudioClockDrift) {
  audio_clock_drift_ = 1.05;
  BothDelayedAudioLaterTest(0);
}

TEST_F(StreamSynchronizationTest, BothDelayedVideoClockDrift) {
  video_clock_drift_ = 1.05;
  BothDelayedAudioLaterTest(0);
}

TEST_F(StreamSynchronizationTest, BothEquallyDelayed) {
  const int kDelayMs = 2000;
  int current_audio_delay_ms = kDelayMs;
  int total_audio_delay_ms = 0;
  int total_video_delay_ms = kDelayMs;
  // In sync, expect no change.
  EXPECT_FALSE(DelayedStreams(kDelayMs, kDelayMs, current_audio_delay_ms,
                              &total_audio_delay_ms, &total_video_delay_ms));
  // Trigger another call with the same values, delay should not be modified.
  total_video_delay_ms = kDelayMs;
  EXPECT_FALSE(DelayedStreams(kDelayMs, kDelayMs, current_audio_delay_ms,
                              &total_audio_delay_ms, &total_video_delay_ms));
  // Change delay value, delay should not be modified.
  const int kDelayMs2 = 5000;
  current_audio_delay_ms = kDelayMs2;
  total_video_delay_ms = kDelayMs2;
  EXPECT_FALSE(DelayedStreams(kDelayMs2, kDelayMs2, current_audio_delay_ms,
                              &total_audio_delay_ms, &total_video_delay_ms));
}

TEST_F(StreamSynchronizationTest, BothDelayedAudioLaterWithBaseDelay) {
  const int kBaseTargetDelayMs = 3000;
  sync_.SetTargetBufferingDelay(kBaseTargetDelayMs);
  BothDelayedAudioLaterTest(kBaseTargetDelayMs);
}

TEST_F(StreamSynchronizationTest, BothDelayedAudioClockDriftWithBaseDelay) {
  const int kBaseTargetDelayMs = 3000;
  sync_.SetTargetBufferingDelay(kBaseTargetDelayMs);
  audio_clock_drift_ = 1.05;
  BothDelayedAudioLaterTest(kBaseTargetDelayMs);
}

TEST_F(StreamSynchronizationTest, BothDelayedVideoClockDriftWithBaseDelay) {
  const int kBaseTargetDelayMs = 3000;
  sync_.SetTargetBufferingDelay(kBaseTargetDelayMs);
  video_clock_drift_ = 1.05;
  BothDelayedAudioLaterTest(kBaseTargetDelayMs);
}

TEST_F(StreamSynchronizationTest, BothDelayedVideoLaterWithBaseDelay) {
  const int kBaseTargetDelayMs = 2000;
  sync_.SetTargetBufferingDelay(kBaseTargetDelayMs);
  BothDelayedVideoLaterTest(kBaseTargetDelayMs);
}

TEST_F(StreamSynchronizationTest,
       BothDelayedVideoLaterAudioClockDriftWithBaseDelay) {
  const int kBaseTargetDelayMs = 2000;
  audio_clock_drift_ = 1.05;
  sync_.SetTargetBufferingDelay(kBaseTargetDelayMs);
  BothDelayedVideoLaterTest(kBaseTargetDelayMs);
}

TEST_F(StreamSynchronizationTest,
       BothDelayedVideoLaterVideoClockDriftWithBaseDelay) {
  const int kBaseTargetDelayMs = 2000;
  video_clock_drift_ = 1.05;
  sync_.SetTargetBufferingDelay(kBaseTargetDelayMs);
  BothDelayedVideoLaterTest(kBaseTargetDelayMs);
}

}  // namespace webrtc
